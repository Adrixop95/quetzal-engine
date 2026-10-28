#!/usr/bin/env python3
"""Unattended driver for Codex tasks (uses no Claude tokens).

Loop: resume tasks stopped by the Codex usage limit (retrying every 20 minutes), start queued prompts
(tasks/queue/*.md, legacy work/codex/queue fallback, at most CODEX_MAX_PARALLEL running (default 4)), and land finished tasks: apply the worktree changes to a feature branch,
build + ctest, commit, push, open a PR into develop, wait for green CI and squash-merge it (never --admin).
Run:  nohup tools/codex_supervisor.py > work/codex/supervisor.log 2>&1 &
"""
import json
import os
import subprocess
import sys
import threading
import time

from codex_orch import queue_files

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
        run(f"git worktree add --detach {LAND} origin/develop")


def lrun(cmd):
    return run(cmd, cwd=LAND)


def land(name):
    """Land a finished task in the dedicated checkout LAND (the main checkout is never touched)."""
    ensure_land()
    branch = "feature/codex-" + name
    lrun("git reset -q --hard && git clean -qfd -e build_sup && git fetch -q origin")
    env = f"LAND_ROOT={LAND}"
    r = lrun(f"{env} python3 {ORCH} harvest {name} {branch} '{name}'")
    if "prepared" not in r.stdout:
        log("harvest produced nothing for", name, r.stdout[-200:], r.stderr[-200:])
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


LAND_TRIES = {}


def return_to_codex(name, text):
    """Send a failed landing back to the Codex session that made it (at most 3 times); otherwise leave it for a human."""
    LAND_TRIES[name] = LAND_TRIES.get(name, 0) + 1
    if LAND_TRIES[name] > 3:
        log("STUCK: task", name, "failed landing 3 times")
        return
    log("feedback to", name, run(f"python3 {ORCH} feedback {name} {json.dumps(text)}").stdout.strip())


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
ATTEMPTS = {}                  # (pr number, kind) -> tries; reset when the PR makes progress
MAX_ATTEMPTS = 3
STUCK = set()


def codex_in_land(prompt, timeout=3000):
    r = subprocess.run(["codex", "exec", "-C", LAND, "-s", "workspace-write", prompt],
                       text=True, capture_output=True, timeout=timeout)
    return (r.stdout + r.stderr)[-400:]


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
        for f in [f for f in conflicted if f.endswith(".md")]:   # docs conflicts are additive: keep both sides without Codex
            if lrun(f"git show :1:{f} > /tmp/.u_base && git show :2:{f} > /tmp/.u_ours && git show :3:{f} > /tmp/.u_theirs "
                    f"&& git merge-file --union -p /tmp/.u_ours /tmp/.u_base /tmp/.u_theirs > {f}.u && mv {f}.u {f}").returncode == 0:
                lrun(f"git add {f}")
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


def attempt(num, kind):
    ATTEMPTS[(num, kind)] = ATTEMPTS.get((num, kind), 0) + 1
    return ATTEMPTS[(num, kind)] <= MAX_ATTEMPTS


def queue_step():
    """One step of the serial merge queue: the oldest open codex PR is synced, fixed or merged; the rest wait their turn."""
    out = run("gh pr list --json number,headRefName --jq '.[]|\"\\(.number) \\(.headRefName)\"'").stdout.splitlines()
    prs = sorted((int(l.split()[0]), l.split()[1]) for l in out if len(l.split()) == 2 and l.split()[1].startswith("feature/codex-"))
    for num, head in prs:
        if num in STUCK:
            continue
        v = json.loads(run(f"gh pr view {num} --json mergeStateStatus,mergeable").stdout or "{}")
        state, mergeable = v.get("mergeStateStatus"), v.get("mergeable")
        checks = run(f"gh pr checks {num}").stdout
        failed, pending = "\tfail" in checks, "\tpending" in checks
        if mergeable == "UNKNOWN":
            return
        if mergeable == "CONFLICTING" or state in ("BEHIND", "DIRTY"):
            if not attempt(num, "sync"):
                STUCK.add(num)
                log("STUCK: PR", num, "cannot be synced with develop after", MAX_ATTEMPTS, "tries")
                continue
            bad = sync_with_develop(num, head, "Previous attempt failed: " + str(ATTEMPTS.get((num, "last"), "")))
            ATTEMPTS[(num, "last")] = bad
            log("PR", num, "synced with develop" if not bad else "sync FAILED: " + bad)
            return
        if failed:
            if not attempt(num, "ci"):
                STUCK.add(num)
                log("STUCK: PR", num, "CI keeps failing after", MAX_ATTEMPTS, "fixes")
                continue
            bad = fix_ci(num, head)
            log("PR", num, "CI fix pushed" if not bad else "CI fix FAILED: " + bad)
            return
        if pending or not checks.strip():
            return   # wait for CI; the queue is serial so nothing else merges meanwhile
        m = run(f"gh pr merge {num} --squash --delete-branch")
        log("merged PR", num, (m.stdout + m.stderr).strip()[-150:])
        return


def queue_loop():
    while True:
        try:
            queue_step()
        except Exception as e:
            log("queue error", e)
        time.sleep(45)


def main():
    last_resume = 0
    threading.Thread(target=queue_loop, daemon=True).start()
    while True:
        try:
            if run("git branch --show-current").stdout.strip() == "develop":
                run("git fetch -q origin && git merge -q --ff-only origin/develop")   # pick up new queue files
            st = status()
            running = [n for n, s in st.items() if s == "running"]
            for n, s in st.items():
                if s == "done":
                    log("landing", n)
                    with LAND_LOCK:
                        land(n)
            if time.time() - last_resume > 1200 and any(s in ("limited", "stopped") for s in st.values()):
                last_resume = time.time()
                log("resume attempt", run(f"python3 {ORCH} resume all").stdout.strip())
            running = [n for n, s in status().items() if s == "running"]
            free = int(os.environ.get("CODEX_MAX_PARALLEL", "4")) - len(running)
            if free > 0:
                for q in queue_files():
                    name = os.path.splitext(os.path.basename(q))[0]
                    if name in st:
                        continue  # Local state prevents rerunning a tracked prompt.
                    deps = [d.strip() for d in (open(q).read().split("\n", 3)[0].replace("Depends:", "").split(",")) if d.strip()] \
                        if open(q).read().startswith("Depends:") else []
                    if any(st.get(d) != "harvested" or d in pending_merge() for d in deps):
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
    main()
