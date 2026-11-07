#!/usr/bin/env python3
"""Unattended driver for Codex tasks (uses no Claude tokens).

Loop: resume tasks stopped by the Codex usage limit (retrying every 20 minutes), start queued prompts
(tasks/queue/*.md, legacy work/codex/queue fallback, at most CODEX_MAX_PARALLEL running (default 4)), and land finished tasks: apply the worktree changes to a feature branch,
build + ctest, commit, push, open a PR into develop, wait for green CI and squash-merge it (never --admin).
Run:  nohup tools/codex_supervisor.py > work/codex/supervisor.log 2>&1 &
"""
import json
import os
import re
import shlex
import subprocess
import sys
import threading
import time

import codex_orch
from codex_orch import queue_files
from codex_state import SupervisorState

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ORCH = os.path.join(ROOT, "tools/codex_orch.py")
GIT = ["git", "-c", "user.name=adrox", "-c", "user.email=adrixop95@gmail.com"]


LAND = os.environ.get("LAND_ROOT", os.path.expanduser("~/aztec-re-land"))


def run(cmd, cwd=ROOT, check=False):
    r = subprocess.run(cmd, shell=True, cwd=cwd, text=True, capture_output=True)
    if check and r.returncode:
        raise RuntimeError(cmd + "\n" + r.stdout[-800:] + r.stderr[-800:])
    return r


def status():
    out = {}
    for line in run(f"python3 {ORCH} status").stdout.splitlines():
        p = line.split()
        if len(p) >= 2:
            out[p[0]] = p[1]
    return out


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)


def ensure_land():
    if not os.path.isdir(LAND):
        run(f"git worktree add --detach {shlex.quote(LAND)} origin/develop", check=True)
    merge = lrun("git rev-parse -q --verify MERGE_HEAD").returncode == 0
    dirty = lrun("git status --porcelain").stdout.strip()
    if merge or dirty:
        log("recovering interrupted LAND work", "merge" if merge else "dirty checkout")
        if merge:
            lrun("git merge --abort")
        for cmd in ("git reset -q --hard", "git clean -qfd -e build_sup",
                    "git checkout -q --detach origin/develop"):
            result = lrun(cmd)
            if result.returncode:
                raise RuntimeError("LAND recovery failed: " + cmd + result.stderr)
        log("LAND recovery complete; discarded interrupted working changes")


def lrun(cmd):
    return run(cmd, cwd=LAND)


def union_markdown(conflicted):
    """Docs conflicts are additive: keep both sides without Codex."""
    for f in [f for f in conflicted if f.endswith(".md")]:
        if lrun(f"git show :1:{f} > /tmp/.u_base && git show :2:{f} > /tmp/.u_ours && git show :3:{f} > /tmp/.u_theirs "
                f"&& git merge-file --union -p /tmp/.u_ours /tmp/.u_base /tmp/.u_theirs > {f}.u && mv {f}.u {f}").returncode == 0:
            lrun(f"git add {f}")


def land(name):
    """Land a finished task in the dedicated checkout LAND (the main checkout is never touched)."""
    ensure_land()
    branch = "feature/codex-" + name
    lrun("git reset -q --hard && git clean -qfd -e build_sup && git fetch -q origin")
    env = f"LAND_ROOT={LAND}"
    r = lrun(f"{env} python3 {ORCH} harvest {name} {branch} '{name}'")
    if "prepared" not in r.stdout:
        conflicted = lrun("git diff --name-only --diff-filter=U").stdout.split()
        if "did not apply" not in r.stdout or not conflicted:
            log("harvest produced nothing for", name, r.stdout[-200:], r.stderr[-200:])
            return
        union_markdown(conflicted)
        conflicted = lrun("git diff --name-only --diff-filter=U").stdout.split()
        log("patch of", name, "conflicts with develop in", " ".join(conflicted) or "Markdown only (merged by union)")
        if conflicted:
          codex_in_land("This worktree holds a task patch applied with git apply --3way onto the latest develop; it has conflict markers in: "
                        + " ".join(conflicted) + ". Resolve every conflict preserving the intent of BOTH sides (the task adds a feature, develop gained "
                        "other features in the same files). Remove all markers, make the project build and tests pass (cmake -S . -B build_sup "
                        "-DAZTEC_GEN_DIR= && cmake --build build_sup && ctest --test-dir build_sup), keep everything English. Do NOT run git "
                        "write commands; leave the resolved files in the working tree.", fallback=False)
        bad = verify_land(conflicted) if conflicted else ""
        if bad:
            log("could not resolve patch conflicts for", name, bad)
            lrun("git reset -q --hard; git checkout -q --detach origin/develop")
            return
    lrun("cmake -S . -B build_sup -DAZTEC_GEN_DIR= -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build_sup --parallel 8 2>&1 | tail -3")
    if lrun("ctest --test-dir build_sup >/dev/null 2>&1").returncode != 0:
        out = lrun("ctest --test-dir build_sup --output-on-failure 2>&1 | tail -30").stdout
        log("tests FAILED for", name, out)
        lrun("git reset -q --hard")
        return_to_codex(name, "Landing failed: the tests fail on a clean checkout of develop with your changes applied. Fix the cause and finish again. Test output tail:\n" + out[-1500:])
        return
    committed = False
    for _ in range(3):   # pre-commit hooks may rewrite files (whitespace, EOF): re-add and retry
        lrun("git add -A -- . ':!build_sup'")
        c = lrun(" ".join(GIT) + f" commit -qm 'Codex task {name}: automated landing (CI-gated)'")
        if c.returncode == 0:
            committed = True
            break
        last = (c.stdout + c.stderr)[-1500:]
    if not committed and "nothing to commit" in last:
        log("no changes to land for", name, "(already in develop or empty result)")
        lrun("git reset -q --hard")
        mark_harvested(name)
        return
    if not committed:
        log("commit failed for", name, "\n", last)
        lrun("git reset -q --hard")
        return_to_codex(name, "Landing failed: pre-commit/commit rejected your changes. Fix the reported problems. Output tail:\n" + last[-1500:])
        return
    lrun(f"git push -q -u origin {branch}")
    pr = lrun(f"gh pr create --base develop --title 'Codex task {name}' --body 'Automated landing of the Codex task {name}; gated by ctest, pre-commit and CI.'").stdout.strip()
    log("PR", pr)
    lrun("git checkout -q --detach origin/develop")
    if not pr.startswith("http"):
        log("could not open PR for", name)
        return
    mark_harvested(name)


MAX_ATTEMPTS = 3
INFRA_RETRIES = int(os.environ.get("CODEX_INFRA_RETRIES", "3"))
RETRY_DELAY = float(os.environ.get("CODEX_INFRA_RETRY_DELAY", "120"))
STUCK_SECONDS = float(os.environ.get("CODEX_STUCK_SECONDS", "21600"))
STATE = SupervisorState(os.path.join(ROOT, "work/codex/state/supervisor"), STUCK_SECONDS)


def return_to_codex(name, text):
    """Send a failed landing back to the Codex session that made it (at most 3 times); otherwise leave it for a human."""
    key = "task:" + name
    if not STATE.begin(key, "landing", MAX_ATTEMPTS):
        log("STUCK: task", name, STATE.record(key).get("stuck"))
        return
    try:
        result = run(f"python3 {ORCH} feedback {name} {shlex.quote(text)}")
        outcome = (result.stdout + result.stderr).strip()
        log("feedback to", name, outcome)
    except Exception as error:
        outcome = str(error)
        raise
    finally:
        STATE.finish(key, "landing", outcome)


def mark_harvested(name):
    import json
    f = os.path.join(ROOT, "work/codex/state", name + ".json")
    t = json.load(open(f))
    t["harvested"] = True
    json.dump(t, open(f, "w"))


def pending_merge():
    """Names of codex tasks whose PR is still open (their code is not in develop yet)."""
    out = run("gh pr list --json headRefName --jq '.[].headRefName'").stdout.split()
    return {h[len("feature/codex-"):] for h in out if h.startswith("feature/codex-")}


LAND_LOCK = threading.Lock()   # LAND is one checkout: landing tasks and the merge queue take turns


CLAUDE_TOOLS = ["Read", "Edit", "Write", "Bash(cmake:*)", "Bash(ctest:*)", "Bash(ninja:*)", "Bash(grep:*)", "Bash(ls:*)",
                "Bash(git status:*)", "Bash(git diff:*)", "Bash(git show:*)", "Bash(git log:*)"]


def codex_in_land(prompt, timeout=3000, fallback=True):
    """Run Codex in LAND; when its usage limit is hit, fall back to Claude Haiku (open PR flows only, no git writes)."""
    r = subprocess.run(["codex", "exec", "-C", LAND, "-s", "workspace-write", prompt],
                       text=True, capture_output=True, timeout=timeout)
    out = r.stdout + r.stderr
    if fallback and "usage limit" in out:
        log("Codex limit hit, falling back to Claude Haiku")
        c = subprocess.run(["claude", "-p", "--model", "haiku", "--permission-mode", "acceptEdits",
                            "--add-dir", LAND, "--allowedTools", *CLAUDE_TOOLS],
                           input=prompt, cwd=LAND, text=True, capture_output=True, timeout=timeout)
        out = c.stdout + c.stderr
    return out[-400:]


def verify_land(files):
    """Gate for a resolved merge: no markers in the touched files, build and tests pass."""
    if files and lrun("grep -lE '^(<<<<<<<|>>>>>>>) ' -- " + " ".join(files)).stdout.strip():
        return "conflict markers remain in " + lrun("grep -lE '^(<<<<<<<|>>>>>>>) ' -- " + " ".join(files)).stdout
    if lrun("cmake -S . -B build_sup -DAZTEC_GEN_DIR= -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1").returncode:
        return "cmake configure failed"
    b = lrun("cmake --build build_sup --parallel 12 2>&1")
    if b.returncode:
        return "build failed: " + b.stdout[-600:]
    t = lrun("ctest --test-dir build_sup --output-on-failure 2>&1 | tail -30")
    if "100% tests passed" not in t.stdout:
        return "tests failed: " + t.stdout[-600:]
    return ""


def push_land(head, message):
    lrun("git add -A -- . ':!build_sup'")
    for _ in range(3):   # pre-commit hooks may rewrite files: re-add and retry
        c = lrun(" ".join(GIT) + " commit -qm " + json.dumps(message))
        if c.returncode == 0:
            break
        lrun("git add -A -- . ':!build_sup'")
    else:
        return "commit failed: " + (c.stdout + c.stderr)[-300:]
    p = lrun(f"git push -q origin HEAD:{head}")
    return "push failed: " + p.stderr[-300:] if p.returncode else ""


def sync_with_develop(num, head, hint=""):
    """Merge origin/develop into the PR branch; Codex resolves conflicts. Returns "" on success, else a reason."""
    with LAND_LOCK:
        ensure_land()
        lrun("git merge --abort >/dev/null 2>&1; git reset -q --hard && git clean -qfd -e build_sup && git fetch -q origin")
        lrun(f"git checkout -q -f --detach origin/{head}")
        lrun(" ".join(GIT) + " merge --no-commit --no-ff origin/develop")
        conflicted = lrun("git diff --name-only --diff-filter=U").stdout.split()
        union_markdown(conflicted)
        conflicted = lrun("git diff --name-only --diff-filter=U").stdout.split()
        try:
            if conflicted:
                log("PR", num, "conflicts:", " ".join(conflicted))
                codex_in_land(
                    "This worktree is in the middle of a git merge of origin/develop into a feature branch and has conflict markers in: "
                    + " ".join(conflicted) + ". Resolve every conflict preserving the intent of BOTH sides (the branch adds a feature, "
                    "develop gained other features in the same files; keep both behaviours, merge tests of both). Remove all markers, make "
                    "the project build and tests pass (cmake -S . -B build_sup -DAZTEC_GEN_DIR= && cmake --build build_sup && ctest "
                    "--test-dir build_sup), keep everything English. Do NOT run git commit or other git write commands; leave resolved files "
                    "in the working tree. " + hint)
            bad = verify_land(conflicted) if conflicted else ""
            if not bad:
                bad = push_land(head, f"Merge develop into {head}" + (" (conflicts resolved by Codex)" if conflicted else ""))
            return bad
        finally:
            lrun("git merge --abort >/dev/null 2>&1; git reset -q --hard; git checkout -q --detach origin/develop")


def fix_ci(num, head):
    """Ask Codex to fix a failing CI check on the PR branch."""
    rows = [l.split("\t") for l in run(f"gh pr checks {num}").stdout.splitlines() if "\tfail" in l]
    logs = ""
    for r in rows[:2]:
        rid = r[-1].split("/runs/")[-1].split("/")[0] if "/runs/" in r[-1] else ""
        if rid:
            logs += run(f"gh run view {rid} --log-failed 2>&1 | tail -60").stdout
    with LAND_LOCK:
        ensure_land()
        lrun("git merge --abort >/dev/null 2>&1; git reset -q --hard && git clean -qfd -e build_sup && git fetch -q origin")
        lrun(f"git checkout -q -f --detach origin/{head}")
        try:
            codex_in_land("CI fails on this branch. Fix the cause with minimal changes, keep everything English, verify build, ctest and "
                          "pre-commit locally, do NOT run git write commands. Failing log tail:\n" + logs[-3500:])
            bad = verify_land([])
            return bad or push_land(head, "Fix CI (by Codex)")
        finally:
            lrun("git reset -q --hard; git checkout -q --detach origin/develop")


def gh_json(cmd):
    result = run(cmd)
    if result.returncode and not (cmd.startswith("gh pr checks ") and result.returncode in (1, 8)):
        raise RuntimeError(cmd + ": " + result.stderr[-500:])
    return json.loads(result.stdout)


def open_prs():
    return sorted((row["number"], row["headRefName"]) for row in
                  gh_json("gh pr list --limit 1000 --json number,headRefName")
                  if row["headRefName"].startswith("feature/codex-"))


# Branch protection requires only these checks. Slower platform jobs (MSVC, MinGW, AppleClang, sanitizers) still run and are
# visible on the pull request, but they must not hold the serial merge queue or trigger code-fix attempts; their failures are
# handled from the post-merge run on develop. Set CODEX_REQUIRED_CHECKS to a comma list (empty value: gate on every check).
REQUIRED_CHECKS = {name.strip() for name in os.environ.get("CODEX_REQUIRED_CHECKS", "build,pre-commit").split(",") if name.strip()}


def gating_checks(checks):
    """Checks that decide pending, failed and passed. Falls back to all checks when no required check is listed yet."""
    if not REQUIRED_CHECKS:
        return checks
    required = [check for check in checks if check.get("name") in REQUIRED_CHECKS]
    return required if required else checks


def rollup_checks(num):
    """Checks in the bucket format of `gh pr checks --json`, built from statusCheckRollup (older gh releases lack that flag)."""
    rows = gh_json(f"gh pr view {num} --json statusCheckRollup").get("statusCheckRollup") or []
    checks = []
    for row in rows:
        name = row.get("name") or row.get("context") or ""
        status = (row.get("status") or "").upper()
        conclusion = (row.get("conclusion") or row.get("state") or "").upper()
        if status in ("QUEUED", "IN_PROGRESS", "PENDING", "WAITING", "REQUESTED") or (status != "COMPLETED" and not conclusion):
            bucket = "pending"
        elif conclusion == "SUCCESS":
            bucket = "pass"
        elif conclusion in ("SKIPPED", "NEUTRAL"):
            bucket = "skipping"
        elif conclusion == "CANCELLED":
            bucket = "cancel"
        elif conclusion in ("FAILURE", "ERROR", "TIMED_OUT", "ACTION_REQUIRED", "STARTUP_FAILURE"):
            bucket = "fail"
        else:
            bucket = "pending"
        checks.append(dict(bucket=bucket, name=name, state=conclusion or status,
                           link=row.get("detailsUrl") or row.get("targetUrl") or ""))
    return checks


def pr_checks(num):
    """`gh pr checks --json` where supported, otherwise the statusCheckRollup equivalent."""
    result = run(f"gh pr checks {num} --json bucket,link,name,state")
    if "unknown flag" in (result.stderr or ""):
        return rollup_checks(num)
    return gh_json(f"gh pr checks {num} --json bucket,link,name,state")


def ci_state(num):
    """Separate runner failures from executed CI failures; API errors fail closed."""
    checks = pr_checks(num)
    checks = gating_checks(checks)
    # gh pr checks exits 1 for failed checks and 8 for pending checks, even with JSON.
    infrastructure = set()
    cancelled_without_run = False
    for check in checks:
        if check["bucket"] not in ("fail", "cancel"):
            continue
        match = re.search(r"/actions/runs/(\d+)", check.get("link", ""))
        if not match:
            cancelled_without_run |= check["bucket"] == "cancel"
            continue
        rid = match.group(1)
        detail = gh_json(f"gh run view {rid} --json jobs,status,conclusion")
        if detail.get("status") != "completed":
            return "pending", []
        jobs = detail.get("jobs", [])
        bad_jobs = [job for job in jobs if job.get("conclusion") in ("failure", "cancelled", "timed_out")]
        no_steps = not jobs or (bool(bad_jobs) and all(
            not any(step.get("status") == "completed" and step.get("conclusion") != "skipped"
                    for step in job.get("steps", [])) for job in bad_jobs))
        not_acquired = "not acquired" in json.dumps(detail).lower()
        if check["bucket"] == "cancel" or detail.get("conclusion") == "cancelled" or no_steps or not_acquired:
            infrastructure.add(rid)
    if infrastructure or cancelled_without_run:
        return "infrastructure", sorted(infrastructure)
    if any(check["bucket"] in ("fail", "cancel") for check in checks):
        return "failed", []
    if not checks or any(check["bucket"] == "pending" for check in checks):
        return "pending", []
    return "passed", []


def rerun_infrastructure(num, runs):
    key = str(num)
    for rid in runs:
        with STATE.lock:
            record = STATE.record(key)
            retry = record["runs"].setdefault(rid, {"next": 0})
            if time.time() < retry["next"]:
                continue
            kind = "infra:" + rid
            if not STATE.begin(key, kind, INFRA_RETRIES):
                return
            # Save the delay before gh, including when the service dies inside gh.
            retry["next"] = time.time() + RETRY_DELAY
            STATE.save()
        try:
            result = run(f"gh run rerun {rid} --failed")
            outcome = "rerun requested" if result.returncode == 0 else "rerun failed: " + result.stderr[-500:]
        except Exception as error:
            outcome = str(error)
            raise
        finally:
            STATE.finish(key, kind, outcome)


PREWARM_DEPTH = int(os.environ.get("CODEX_PREWARM_DEPTH", "3"))
_REPO = []


def repo_slug():
    if not _REPO:
        _REPO.append(run("gh repo view --json nameWithOwner -q .nameWithOwner").stdout.strip())
    return _REPO[0]


def prewarm(prs):
    """Bring the first pull requests of the queue up to date server-side (no checkout, no Codex) so that their CI
    already runs while older pull requests merge. Conflicting branches stay with the normal sync path."""
    done = set()
    for num, head in prs[:PREWARM_DEPTH]:
        try:
            view = gh_json(f"gh pr view {num} --json mergeStateStatus,mergeable")
            if view.get("mergeStateStatus") != "BEHIND" or view.get("mergeable") != "MERGEABLE":
                continue
            result = run(f"gh api -X PUT repos/{repo_slug()}/pulls/{num}/update-branch")
            log("PR", num, "update-branch", "requested" if result.returncode == 0
                else (result.stderr + result.stdout).strip()[-120:])
            if result.returncode == 0:
                done.add(num)
        except Exception as error:
            log("PR", num, "update-branch deferred:", error)
    return done


def queue_step():
    """Skip blocked/waiting PRs; perform at most one code change or squash merge."""
    prs = open_prs()
    warmed = prewarm(prs) if PREWARM_DEPTH > 0 else set()
    for num, head in prs:
        if num in warmed:
            continue   # its CI restarts on the new head; look at it on the next pass
        key = str(num)
        try:
            view = gh_json(f"gh pr view {num} --json mergeStateStatus,mergeable,headRefOid")
            STATE.observe_head(key, view["headRefOid"])
            if STATE.blocked(key):
                continue
            ci, runs = ci_state(num)
            if ci == "infrastructure":
                rerun_infrastructure(num, runs)
                continue
            if view.get("mergeable") == "UNKNOWN":
                continue
            if view.get("mergeable") == "CONFLICTING" or view.get("mergeStateStatus") in ("BEHIND", "DIRTY"):
                kind = "sync"
            elif ci == "pending":
                continue
            elif ci == "failed":
                kind = "ci"
            else:
                result = run(f"gh pr merge {num} --squash --delete-branch")
                log("PR", num, "merged" if not result.returncode else "merge deferred",
                    (result.stdout + result.stderr).strip()[-150:])
                if result.returncode:
                    continue
                return
            if not STATE.begin(key, kind, MAX_ATTEMPTS):
                continue
            try:
                bad = (sync_with_develop(num, head, "Previous attempt: " + STATE.record(key).get("last", ""))
                       if kind == "sync" else fix_ci(num, head))
            except Exception as error:
                bad = str(error)
            STATE.finish(key, kind, bad)
            log("PR", num, kind, bad or "success")
            return
        except Exception as error:
            log("PR", num, "deferred:", error)


def report_status():
    for name in codex_orch.names():
        task = codex_orch.refresh(codex_orch.load(name))
        record = STATE.record("task:" + name)
        print(f"task {name} {task['status']} harvested={bool(task.get('harvested'))} "
              f"attempts={json.dumps(record['attempts'])} STUCK={record.get('stuck', {}).get('reason', '-')}")
    for num, head in open_prs():
        record = STATE.record(num)
        try:
            ci, _ = ci_state(num)
        except Exception as error:
            ci = "unknown: " + str(error)
        stuck = record.get("stuck", {})
        reason = stuck.get("reason", "-")
        if stuck and time.time() >= stuck["until"]:
            reason += " (expired; retry on next pass)"
        print(f"PR {num} task={head.removeprefix('feature/codex-')} CI={ci} action={record.get('active', 'waiting')} "
              f"attempts={json.dumps(record['attempts'])} STUCK={reason}")


def queue_loop():
    while True:
        try:
            queue_step()
        except Exception as e:
            log("queue error", e)
        time.sleep(int(os.environ.get("CODEX_QUEUE_INTERVAL", "20")))


def main():
    STATE.recover()
    last_resume = 0
    threading.Thread(target=queue_loop, daemon=True).start()
    while True:
        try:
            if run("git branch --show-current").stdout.strip() == "develop":
                run("git fetch -q origin && git merge -q --ff-only origin/develop")   # pick up new queue files
            st = status()
            running = [n for n, s in st.items() if s == "running"]
            for n, s in st.items():
                if s == "done" and not STATE.blocked("task:" + n):
                    log("landing", n)
                    with LAND_LOCK:
                        land(n)
            if time.time() - last_resume > 1200 and any(s in ("limited", "stopped") for s in st.values()):
                last_resume = time.time()
                for name, task_status in st.items():
                    if task_status in ("limited", "stopped"):
                        log("resume attempt", run(f"python3 {ORCH} resume {shlex.quote(name)}").stdout.strip())
            running = [n for n, s in status().items() if s == "running"]
            free = int(os.environ.get("CODEX_MAX_PARALLEL", "4")) - len(running)
            if free > 0:
                for q in queue_files():
                    name = os.path.splitext(os.path.basename(q))[0]
                    if name in st:
                        continue  # Local state prevents rerunning a tracked prompt.
                    deps = [d.strip() for d in (open(q).read().split("\n", 3)[0].replace("Depends:", "").split(",")) if d.strip()] \
                        if open(q).read().startswith("Depends:") else []
                    if any((st.get(d) != "harvested" and not os.path.exists(os.path.join(ROOT, "tasks", "done", d + ".md"))) or d in pending_merge() for d in deps):
                        continue
                    result = subprocess.run([sys.executable, ORCH, "start", name, q],
                                            cwd=ROOT, text=True, capture_output=True)
                    if result.returncode:
                        raise RuntimeError(result.stdout + result.stderr)
                    log("started", name)
                    free -= 1
                    if free <= 0:
                        break
        except Exception as e:  # keep the loop alive
            log("error", e)
        time.sleep(300)


if __name__ == "__main__":
    if sys.argv[1:] == ["status"]:
        report_status()
    elif not sys.argv[1:]:
        main()
    else:
        raise SystemExit("Usage: codex_supervisor.py [status]")
