#!/usr/bin/env python3
"""Unattended driver for Codex tasks (uses no Claude tokens).

Loop: resume tasks stopped by the Codex usage limit (retrying every 20 minutes), start queued prompts
(tasks/queue/*.md, legacy work/codex/queue fallback, at most CODEX_MAX_PARALLEL running (default 4)), and land finished tasks: apply the worktree changes to a feature branch,
build + ctest, commit, push, open a PR into develop, wait for green CI and squash-merge it (never --admin).
Run:  nohup tools/codex_supervisor.py > work/codex/supervisor.log 2>&1 &
"""
import os
import subprocess
import sys
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
        log("tests FAILED for", name, lrun("ctest --test-dir build_sup --output-on-failure 2>&1 | tail -30").stdout)
        lrun("git reset -q --hard")
        return
    committed = False
    for _ in range(3):   # pre-commit hooks may rewrite files (whitespace, EOF): re-add and retry
        lrun("git add -A -- . ':!build_sup'")
        c = lrun(" ".join(GIT) + f" commit -qm 'Codex task {name}: automated landing (CI-gated)'")
        if c.returncode == 0:
            committed = True
            break
        last = (c.stdout + c.stderr)[-1500:]
    if not committed:
        log("commit failed for", name, "\n", last)
        lrun("git reset -q --hard")
        return
    lrun(f"git push -q -u origin {branch}")
    pr = lrun(f"gh pr create --base develop --title 'Codex task {name}' --body 'Automated landing of the Codex task {name}; gated by ctest, pre-commit and CI.'").stdout.strip()
    log("PR", pr)
    lrun("git checkout -q --detach origin/develop")
    if not pr.startswith("http"):
        log("could not open PR for", name)
        return
    mark_harvested(name)


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


RESOLVED = set()


def resolve_conflicts(num, head):
    """Merge origin/develop into a PR branch inside the landing checkout and let Codex resolve the conflict markers."""
    if num in RESOLVED:
        return
    RESOLVED.add(num)
    ensure_land()
    lrun("git reset -q --hard && git clean -qfd -e build_sup && git fetch -q origin")
    lrun(f"git checkout -q -f -B {head} origin/{head}")
    lrun("git merge --no-commit origin/develop")
    prompt = ("This worktree is in the middle of a git merge of origin/develop into a feature branch and has conflict markers. "
              "Resolve every conflict by preserving the intent of BOTH sides (the branch adds a feature, develop gained other features "
              "in the same files). Remove all conflict markers, make the project build and the tests pass (cmake -S . -B build_sup "
              "-DAZTEC_GEN_DIR= && cmake --build build_sup && ctest --test-dir build_sup), keep everything English. Do NOT run git "
              "commit or any other git write command; leave the resolved files in the working tree and summarize.")
    log("resolving conflicts of PR", num, run(f"codex exec -C {LAND} -s workspace-write {json.dumps(prompt)} 2>&1 | tail -5").stdout[-300:])
    if lrun("grep -rlE '^(<<<<<<<|>>>>>>>) ' --include='*' . --exclude-dir=.git --exclude-dir=build_sup").stdout.strip():
        log("conflict markers remain for PR", num)
        lrun("git merge --abort; git reset -q --hard")
        return
    if lrun("cmake --build build_sup --parallel 8 >/dev/null 2>&1 && ctest --test-dir build_sup >/dev/null 2>&1").returncode != 0:
        log("build/tests fail after resolving PR", num)
        lrun("git merge --abort; git reset -q --hard")
        return
    lrun("git add -A -- . ':!build_sup'")
    lrun(" ".join(GIT) + " commit -qm 'Merge develop into " + head + " (conflicts resolved by Codex)'")
    lrun(f"git push -q origin {head}")
    lrun("git checkout -q --detach origin/develop")
    log("pushed conflict resolution for PR", num)


def merge_green_prs():
    out = run("gh pr list --json number,headRefName --jq '.[]|\"\\(.number) \\(.headRefName)\"'").stdout.splitlines()
    for line in out:
        num, head = (line.split() + ["", ""])[:2]
        if head.startswith("feature/codex-"):
            c = run(f"gh pr checks {num}").stdout
            if c.strip() and "pending" not in c and "fail" not in c:
                m = run(f"gh pr merge {num} --squash --delete-branch")
                log("merged green PR", num, m.stderr.strip()[-100:])
                if "conflict" in (m.stderr + m.stdout).lower():
                    resolve_conflicts(num, head)


def main():
    last_resume = 0
    while True:
        try:
            merge_green_prs()
            st = status()
            running = [n for n, s in st.items() if s == "running"]
            for n, s in st.items():
                if s == "done":
                    log("landing", n)
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
