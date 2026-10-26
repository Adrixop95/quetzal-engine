# Assistant instructions

Quetzal Engine is a GPL-3.0 C++20 reimplementation of Aztec Empire (1999).
It loads the user's original data; a local static recompilation is the behavior oracle.
This repository is PUBLIC. Never add game assets, Aztec.exe, executable-derived
code, generated C, lifted code or decompiled source. Keep those in ignored
`game/`, `work/` and `ghidra/`. Write everything in English, including comments,
docs, UI text and commit messages. Change only files within the task's scope.

Use gitflow: `feature/<topic>` from `develop`, PR into `develop`, green CI build
and pre-commit gates, then squash merge. Never push directly to `main` or
`develop`. Releases use a PR from `develop` into `main`. Respect read-only git
metadata: leave changes uncommitted when required and list changed files.

Map: `src/core/` format readers, `src/engine/` independent engine, `runtime/`
reference host, `tests/` checks/scenarios, `tools/` analysis and automation,
`docs/` specifications, `docs/data/` rules metadata, `tasks/` portable prompts.
`game/`, `work/`, `ghidra/`, `build/` and `build_*/` are private ignored outputs.

Read [docs/HANDOFF.md](docs/HANDOFF.md), [docs/SETUP.md](docs/SETUP.md) and
[tasks/README.md](tasks/README.md) before starting. Verify the baseline first:

```sh
sh tools/bootstrap.sh                   # --no-hooks with read-only git metadata
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
```

Keep build directories inside this worktree. Game-data tests skip without a
local installation; synthetic tests must pass. Repeat checks after changes.
Run reference record/replay, headless probes and isolated experiments as described
in [docs/EXPERIMENTS.md](docs/EXPERIMENTS.md). Do not treat engine determinism,
reference code inspection or tolerant screenshot matches as full game parity.
