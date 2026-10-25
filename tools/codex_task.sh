#!/bin/sh
# Run a Codex CLI task in its own git worktree (background). usage: tools/codex_task.sh NAME PROMPT_FILE
# Output: work/codex/NAME.log (event log), work/codex/NAME.last (final message). Merge the resulting branch afterwards.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; NAME=$1; PF=$2; mkdir -p "$ROOT/work/codex"
PRE="Repository rules: everything you write (code comments, docs, UI text, commit messages) must be in ENGLISH. Never add game assets, Aztec.exe or anything derived from the executable's code to the repo; never edit files outside the scope in the task; use a build directory inside your worktree (not build/ or build_*/ of the main checkout); commit your work on your branch with clear messages when done and verify the build and tests first. When finished print a short summary of what changed and how it was verified."
(cd "$ROOT" && nohup codex exec --worktree -s workspace-write -o "$ROOT/work/codex/$NAME.last" "$PRE

$(cat "$PF")" > "$ROOT/work/codex/$NAME.log" 2>&1 &)
echo "started $NAME"
