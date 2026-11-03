# Windows initialization prompt for Claude Code

Use this when continuing the project on a Windows laptop. Agent tooling (bash scripts,
`unshare` network isolation, the reference runtime, the Codex supervisor) is Linux
based, so the work runs in WSL2 (Ubuntu) driven from VS Code. The engine itself also
builds natively on Windows (MSVC and MinGW jobs in CI), see `docs/SETUP.md`.

## Steps only the human can do

1. Install VS Code, the "WSL" extension, the Claude Code extension, and Git for Windows.
2. In an administrator PowerShell: `wsl --install -d Ubuntu`, reboot, create the Linux user.
3. Be ready to complete interactive logins when asked: GitHub (`gh auth login`), OpenAI
   Codex (`codex login`), Claude Code, and to add a new SSH public key on GitHub.
4. Provide the original game files (they are not in the repository): copy the game
   folder into the Linux home directory, for example from a USB stick or `scp`, so the
   path is `~/aztec-re/game` (inside the WSL file system, not under `/mnt/c`).
5. Optional: copy private analysis output `work/` from the old machine (reference
   runtime inputs). Without it, the engine, synthetic tests and Codex work still run.

## Prompt to paste into Claude Code (opened in a WSL window of VS Code)

```text
You are continuing the Quetzal Engine project (a clean C++20 reimplementation of the
1999 RTS Aztec Empire, repo git@github.com:Adrixop95/quetzal-engine.git). Set up this
Windows laptop (WSL2 Ubuntu is the work environment) so work can continue without any
earlier chat. Do the following, verify each step with a real command, and report
what you could not verify. Do not bypass permission prompts or blocked actions; if
something needs my interactive input (logins, passwords, SSH key upload, copying game
files), stop and tell me exactly what to do, then continue when I confirm.

1. Check the environment: `uname -a`, `lsb_release -a`, that this is WSL2, and free disk
   space (need about 15 GB). Install packages with apt: git, build-essential, cmake,
   ninja-build, g++, libsdl2-dev, python3, python3-pip, pre-commit, ccache, unzip, jq,
   xvfb. Install Node.js LTS (via nvm or the distro package), the GitHub CLI `gh`, the
   Codex CLI (`npm i -g @openai/codex`) and make sure Claude Code works here.
2. Git identity: ask me for the name and e-mail and set `git config --global`. Create a
   new SSH key (`ssh-keygen -t ed25519`), print the PUBLIC key and ask me to add it to
   GitHub; then verify `ssh -T git@github.com`. Run `gh auth login` with me.
3. Clone to `~/aztec-re` (`git clone git@github.com:Adrixop95/quetzal-engine.git ~/aztec-re`),
   check out `develop`, then read `CLAUDE.md`, `AGENTS.md`, `docs/HANDOFF.md`,
   `docs/SETUP.md`, `docs/ROADMAP.md` and `tasks/README.md` and follow their rules (public
   repository, everything in English, feature branches from `develop`, PRs into `develop`
   with squash merge after green CI, never push to `main` or `develop`, never use
   `--admin`, never commit game assets, screenshots, audio or executable-derived data).
4. Run `sh tools/bootstrap.sh` (pre-commit hooks etc.), then build and test the engine:
   `cmake -S . -B build_play -G Ninja -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= &&
   cmake --build build_play --parallel && ctest --test-dir build_play`.
5. Game data: check that `~/aztec-re/game/Aztec.exe` exists; if not, tell me how to copy
   the folder. Then run the engine on it: `build_play/aztec_engine --game ~/aztec-re/game 1`
   (a window needs WSLg; if no display is available, use `SDL_VIDEODRIVER=dummy` headless
   runs and tell me which GUI route works). If `work/` was copied, build the reference
   runtime per `docs/EXPERIMENTS.md` and run `build/aztec_recomp game/Aztec.exe` once.
6. Codex orchestration: read `tools/codex_orch.py` and `tools/codex_supervisor.py`
   docstrings. Check `codex login` works. Do NOT start the supervisor if the desktop PC
   still runs it (two supervisors duplicate PRs). Ask me first; if I say yes, start it
   with `CODEX_MAX_PARALLEL=3 setsid nohup python3 tools/codex_supervisor.py >
   work/codex/supervisor.log 2>&1 < /dev/null & disown`. The queue is `tasks/queue/*.md`;
   local task state lives in `work/codex/state/` (not in git), so tasks already merged
   may re-run; compare `gh pr list --state merged` with the queue and ask me before
   starting anything.
7. Finish with a short report: what works (commands and results), what is missing, and
   the exact commands I use daily (build, run a mission, run tests, check Codex status
   `python3 tools/codex_orch.py status`, check PRs `gh pr list`).
After the setup, whenever I write just "continue", read `docs/NEXT_STEPS.md` and follow it.
Respond to me in Polish; code, docs, commits and PR text stay in English.
```

## Notes

- The Codex usage limit belongs to the account and works on any machine.
- Keep one supervisor only. Stop the old one with `pkill -f '^python3 tools/codex_supervisor.py'`.
- Secrets (`~/keys`, tokens) are never in the repository; set them up on the new machine
  yourself.
