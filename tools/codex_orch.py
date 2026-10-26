#!/usr/bin/env python3
"""Orchestrate Codex CLI tasks that run in their own git worktrees.

Commands (state lives in work/codex/state/<name>.json, local only):
  start NAME [PROMPT_FILE] start a task in the background (new worktree)
  status                   show every task: running / done / limited (usage limit hit) / stopped
  resume NAME|all          resume tasks that stopped (usage limit or crash) in the same session and worktree
  harvest NAME BRANCH "Title"   copy the worktree changes to a new branch off origin/develop, commit, push, open a PR
  log NAME                 print the tail of the task log

Prompts default to tasks/queue/NAME.md (legacy work/codex/queue if absent).
Typical flow when the Codex usage limit resets: `tools/codex_orch.py resume all`.
"""
import glob
import json
import os
import re
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
STATE = os.path.join(ROOT, "work/codex/state")
PREAMBLE = (
    "Repository rules: everything you write (code comments, docs, UI text, commit messages) must be in ENGLISH. "
    "Never add game assets, Aztec.exe or anything derived from the executable's code to the repo; never edit files outside "
    "the scope of the task; use a build directory inside your worktree; the git directory is read-only for you, so leave "
    "your changes uncommitted in the worktree and list the changed files in your final message. Verify build and tests first."
)
RESUME_PROMPT = (
    "You were interrupted (usage limit or crash). Continue the task from where you stopped: inspect the current state of the "
    "worktree (git status, your notes, partial files), finish the remaining items of the original task, verify, and end with a "
    "summary listing the changed files."
)


def queue_files():
    """Prefer the portable queue; use the legacy directory when it is absent."""
    directory = os.path.join(ROOT, "tasks/queue")
    if not os.path.isdir(directory):
        directory = os.path.join(ROOT, "work/codex/queue")
    return sorted(glob.glob(os.path.join(directory, "*.md")) +
                  glob.glob(os.path.join(directory, "*.txt")))


def queued_prompt(name):
    for path in queue_files():
        if os.path.splitext(os.path.basename(path))[0] == name:
            return path
    raise FileNotFoundError("No queued prompt for " + name)


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, text=True, capture_output=True, **kw)


def load(name):
    with open(os.path.join(STATE, name + ".json")) as f:
        return json.load(f)


def save(t):
    os.makedirs(STATE, exist_ok=True)
    with open(os.path.join(STATE, t["name"] + ".json"), "w") as f:
        json.dump(t, f, indent=1)


def names():
    return sorted(f[:-5] for f in os.listdir(STATE) if f.endswith(".json")) if os.path.isdir(STATE) else []


def refresh(t):
    """Fill session/workdir from the log header and compute the status."""
    log = os.path.join(ROOT, "work/codex", t["name"] + ".log")
    text = open(log, errors="replace").read() if os.path.exists(log) else ""
    m = re.search(r"session id: (\S+)", text)
    if m:
        t["session"] = m.group(1)
    m = re.search(r"workdir: (\S+)", text)
    if m:
        t["workdir"] = m.group(1)
    ps = sh("ps -eo args").stdout.splitlines()
    alive = any(l.startswith("codex exec") and t["name"] + ".last" in l for l in ps)
    last = os.path.join(ROOT, "work/codex", t["name"] + ".last")
    has_last = os.path.exists(last) and os.path.getsize(last) > 0
    if t.get("harvested"):
        t["status"] = "harvested"
    elif alive:
        t["status"] = "running"
    elif "hit your usage limit" in text[-3000:]:
        t["status"] = "limited"
    elif has_last:
        t["status"] = "done"
    else:
        t["status"] = "stopped"
    save(t)
    return t


def start(name, prompt_file):
    prompt = open(prompt_file).read()
    os.makedirs(os.path.join(ROOT, "work/codex"), exist_ok=True)
    log = os.path.join(ROOT, "work/codex", name + ".log")
    last = os.path.join(ROOT, "work/codex", name + ".last")
    if os.path.exists(last):
        os.remove(last)
    subprocess.Popen(
        ["codex", "exec", "--worktree", "-s", "workspace-write", "-o", last, PREAMBLE + "\n\n" + prompt],
        cwd=ROOT, stdout=open(log, "w"), stderr=subprocess.STDOUT, start_new_session=True)
    save({"name": name, "prompt_file": os.path.abspath(prompt_file), "started": time.time()})
    print("started", name)


def resume(name):
    t = refresh(load(name))
    if t["status"] in ("running", "done", "harvested"):
        print(name, "is", t["status"], "- not resuming")
        return
    if "session" not in t or "workdir" not in t:
        print(name, "has no recorded session; restart it with: start", name, t.get("prompt_file"))
        return
    log = os.path.join(ROOT, "work/codex", name + ".log")
    last = os.path.join(ROOT, "work/codex", name + ".last")
    if os.path.exists(last):
        os.remove(last)
    subprocess.Popen(
        ["codex", "exec", "resume", t["session"], "-c", 'sandbox_mode="workspace-write"', "-o", last,
         RESUME_PROMPT],
        cwd=t["workdir"], stdout=open(log, "a"), stderr=subprocess.STDOUT, start_new_session=True)
    print("resumed", name, t["session"])


def harvest(name, branch, title):
    t = refresh(load(name))
    wd = t["workdir"]
    os.chdir(wd)
    sh("git add -A -- . ':!.build*' ':!build*' ':!work'")
    patch = sh("git diff --cached --binary").stdout
    sh("git reset -q")
    if not patch.strip():
        print("no changes in", wd)
        return
    pf = os.path.join(ROOT, "work/codex", name + ".patch")
    open(pf, "w").write(patch)
    os.chdir(ROOT)
    r = sh("git fetch -q origin && git checkout -q -b %s origin/develop && git apply --3way %s" % (branch, pf))
    print(r.stdout, r.stderr)
    print("Branch", branch, "prepared with the patch applied; review, build, test, then commit and open the PR "
          "(git add the files; git commit; git push -u origin %s; gh pr create --base develop --title '%s')" % (branch, title))
    t["harvested"] = True
    save(t)


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
    elif a[0] == "start":
        start(a[1], a[2] if len(a) > 2 else queued_prompt(a[1]))
    elif a[0] == "status":
        for n in names():
            t = refresh(load(n))
            print("%-12s %-10s session=%s workdir=%s" % (n, t["status"], t.get("session", "-")[:8], t.get("workdir", "-")))
    elif a[0] == "resume":
        for n in (names() if a[1] == "all" else [a[1]]):
            resume(n)
    elif a[0] == "harvest":
        harvest(a[1], a[2], a[3] if len(a) > 3 else a[2])
    elif a[0] == "log":
        print(open(os.path.join(ROOT, "work/codex", a[1] + ".log"), errors="replace").read()[-3000:])
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
