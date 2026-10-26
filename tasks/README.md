# Portable task queue

Read root `CLAUDE.md` (identical to `AGENTS.md`) and `docs/HANDOFF.md` first.
`tasks/queue/NAME.md` is a complete English prompt: purpose, scope, evidence,
implementation requirements and acceptance checks. Names should contain only
letters, numbers, underscores and hyphens. Numeric prefixes express priority.
Read prerequisites inside each task; alphabetical launch order does not enforce
completion order. Only run dependent tasks after their prerequisites merge.
Move a prompt to `tasks/done/` after its reviewed implementation is merged,
adding the PR reference and verification results. Keep private evidence in work/.

On a new machine follow `docs/SETUP.md`, then ask Claude Code or Codex to read
`CLAUDE.md` and complete the first eligible prompt. This manual route needs no
previous chat or Codex session. For Codex CLI automation:

```sh
python3 tools/codex_orch.py start 01-oracle-parity
python3 tools/codex_orch.py status
python3 tools/codex_orch.py resume all
# Or start the unattended supervisor in a normal writable checkout:
python3 tools/codex_supervisor.py
```

The supervisor requires installed/authenticated `codex` and `gh`, git credentials,
a configured origin, writable git metadata, hooks and build dependencies. It can
start up to two tasks, resume sessions, create feature branches, push, open PRs
into develop and squash merge after CI. Running it opts into that unattended
workflow; do not run it in an assistant worktree with read-only git metadata.
Use manual task execution when dependencies or review need to control ordering.

Both tools prefer `tasks/queue/`; only when that directory is absent do they use
`work/codex/queue/`. Both `.md` and legacy `.txt` prompts are accepted. Prompt
files stay in place on launch; local state prevents repeated starts. State,
logs, session ids and patches stay in ignored `work/codex/`. They are not portable:
a fresh clone starts remaining prompts afresh. Finish and archive merged tasks
before moving machines. To continue unmerged work, transfer the feature branch
and English progress notes; do not assume an old session id can be resumed.
