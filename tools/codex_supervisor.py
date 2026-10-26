#!/usr/bin/env python3
"""Unattended driver for Codex tasks (uses no Claude tokens).

Loop: resume tasks stopped by the Codex usage limit (retrying every 20 minutes), start queued prompts
(work/codex/queue/*.txt, at most 2 running), and land finished tasks: apply the worktree changes to a feature branch,
build + ctest, commit, push, open a PR into develop, wait for green CI and squash-merge it (never --admin).
Run:  nohup tools/codex_supervisor.py > work/codex/supervisor.log 2>&1 &
"""
import glob
import os
import subprocess
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ORCH = os.path.join(ROOT, "tools/codex_orch.py")
GIT = ["git", "-c", "user.name=adrox", "-c", "user.email=adrixop95@gmail.com"]


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


def land(name):
    branch = "feature/codex-" + name
    run("git fetch -q origin && git checkout -q develop && git merge -q --ff-only origin/develop")
    r = run(f"python3 {ORCH} harvest {name} {branch} '{name}'")
    if "prepared" not in r.stdout:
        log("harvest produced nothing for", name)
        return
    b = run("cmake -S . -B build_sup -DAZTEC_GEN_DIR= -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build_sup --parallel 6 2>&1 | tail -3")
    t = run("ctest --test-dir build_sup --output-on-failure 2>&1 | tail -40")
    if run("ctest --test-dir build_sup >/dev/null 2>&1").returncode != 0:
        log("tests FAILED for", name, "- leaving", branch, "unmerged\n", t.stdout)
        run("git add -A && git stash -q -u -m codex-" + name + "-failed; git checkout -q develop")
        return
    run("git add -A -- . ':!build_sup' && " + " ".join(GIT) + f" commit -qm 'Codex task {name}: automated landing (CI-gated)' ")
    run(f"git push -q -u origin {branch}")
    pr = run(f"gh pr create --base develop --title 'Codex task {name}' --body 'Automated landing of the Codex task {name}; gated by ctest, pre-commit and CI.'").stdout.strip()
    log("PR", pr)
    num = pr.rstrip("/").split("/")[-1]
    for _ in range(60):
        time.sleep(30)
        c = run(f"gh pr checks {num}").stdout
        if "pending" in c or not c.strip():
            continue
        if "fail" in c:
            log("CI failed for PR", num)
            break
        m = run(f"gh pr merge {num} --squash --delete-branch")
        log("merge", num, m.stdout.strip()[-80:], m.stderr.strip()[-120:])
        break
    run("git checkout -q develop && git fetch -q origin && git merge -q --ff-only origin/develop")


def main():
    last_resume = 0
    while True:
        try:
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
            if len(running) < 2:
                for q in sorted(glob.glob(os.path.join(ROOT, "work/codex/queue/*.txt"))):
                    name = os.path.basename(q)[:-4]
                    run(f"python3 {ORCH} start {name} {q} && mv {q} {q}.started")
                    log("started", name)
                    break
        except Exception as e:  # keep the loop alive
            log("error", e)
        time.sleep(300)


if __name__ == "__main__":
    main()
