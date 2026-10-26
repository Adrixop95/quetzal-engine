# Setup on a new machine

Start with root `CLAUDE.md` or `AGENTS.md`, then [HANDOFF.md](HANDOFF.md).
Use a feature branch from develop. This PUBLIC GPL-3.0 repository must never
receive game assets, Aztec.exe, generated/lifted C or decompiled source.
A clean clone builds and tests without any of those private inputs.

## Dependencies and independent build

Linux is the verified development/CI path. Install CMake >=3.16, GCC/g++ with
C++20 support, a build tool (Make or Ninja), SDL2 development files >=2.0.18
(`SDL_RenderGeometry`), Python 3, git, binutils (`objdump`), pkg-config and
pre-commit. ffmpeg enables intro video playback; it is not needed for compilation.
For Debian/Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake libsdl2-dev python3 python3-venv \
  pre-commit ffmpeg binutils pkg-config git
sh tools/bootstrap.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
python3 -m unittest discover -s tests -p test_handoff_tools.py
```

Alternatively install pre-commit in a Python virtual environment outside the
tracked tree and add it to PATH. Bootstrap checks dependencies, creates ignored
local directories, installs hooks and prints next steps; repeated runs are safe.
With read-only git metadata use `sh tools/bootstrap.sh --no-hooks` and run checks
manually. It does not install packages or alter branches. Always build inside
the current worktree. If a system ccache wrapper attempts to write outside it,
set `CCACHE_DIR="$PWD/build/ccache"` for configure/build (create build/ first).

CTest reports four asset-independent checks and two game-data checks; the latter
skip without an installation. Skipping is not game-data verification. See
[CONTRIBUTING.md](../CONTRIBUTING.md) for GAME_DIR precedence and test labels.
Run pre-commit on all tracked files before a PR; use `--files PATH...` as well for
new untracked files while working with read-only git metadata.

## Your own game files

Copy your legally obtained installation into ignored `game/` with `Res/`,
`Maps/`, configuration/profile files and, for reference analysis, `Aztec.exe`.
Do not force-add anything in game/. No download or game extraction is provided.
The independent engine loads assets at runtime; daylight lookup also uses the
local executable when available. An external installation can be used instead:

```sh
cmake -S . -B build -DGAME_DIR=/absolute/path/to/game
ctest --test-dir build --output-on-failure
build/aztec_engine --game /absolute/path/to/game 1
```

`GAME_DIR` configures tests, while `--game` / `AZTEC_GAME` configures the engine.
Experiments may write maps, saves, profiles and settings. Use private writable
copies as described in [EXPERIMENTS.md](EXPERIMENTS.md), with read-only asset
symlinks and separate ordinary Maps/Saves directories. Never share writable
installation state across reference processes.

## Regenerate local analysis

`work/`, `work/gen/`, `work/decomp/` and `ghidra/` are ignored and intentionally
absent from a clone. `sh tools/regen_analysis.sh` creates directories and runs
available stages; SKIP messages identify missing private prerequisites. A real
command failure exits nonzero. It never downloads private inputs or writes its
reports/headers into docs/ or src/. Existing files must come from the same
executable revision; use a fresh clone/worktree for another game version.
Historical scripts contain fixed addresses and are not universal PE analyzers.

```sh
# No Ghidra: disassemble if an executable is present, then reuse existing inputs.
sh tools/regen_analysis.sh --game /absolute/path/to/game
# Full lift, runtime build and decompilation (optional, slow):
sh tools/regen_analysis.sh --game /absolute/path/to/game --ghidra /opt/ghidra_12
```

For a full regeneration install Ghidra 12.x with its supported JDK (see the
Ghidra distribution's installation instructions); `support/analyzeHeadless` must
be executable. Provide C compiler/SDL2/Make dependencies too. The wrapper calls
`tools/build_recomp.sh GHIDRA_DIR GAME_DIR`, which imports into local
`ghidra/AztecProj`, runs `LiftAll.java` with `tools/extra_entries.txt`, writes
`work/gen/gen_*.c` / `gen_funcs.h` and builds `build/aztec_recomp`. The wrapper
then runs `ExportAll.java` into `work/decomp/all.c` and `functions.txt`.
This path needs Ghidra and the user's executable; it is excluded from CI.
To avoid an optional runtime target in subsequent independent builds, configure
again with `-DAZTEC_GEN_DIR=` or use another in-worktree build directory.

| Stage | Inputs | Private outputs / prerequisite |
| --- | --- | --- |
| Disassembly | Aztec.exe; `objdump -d -M intel -j .text` | work/text.asm; no Ghidra required |
| analyze_classes.py | executable, gen/gen_funcs.h | classes.json; function boundaries need a prior lift |
| class_tree.py | text.asm, classes.json, gen_funcs.h | class_edges.json |
| ctor_sizes.py | text.asm, classes.json | ctor_sizes.json |
| source_map.py | executable, text.asm, gen_funcs.h, classes.json | source_map.json |
| func_names.py | executable, text.asm, gen_funcs.h | func_names.json |
| class_fields.py | classes.json, decomp/all.c | classes.md; prior Ghidra export required |
| object_kinds.py | executable, decomp/all.c, classes.json | object_kinds.txt |
| class_model.py | survey/m*/objs_*.txt, classes/edges/ctor sizes | class_model.json; live reference dumps required |
| gen_class_docs.py, gen_model_headers.py | model, source map, edges, classes | classes_observed.md, objects.hpp, both in work/ |

All output paths in the table are under work/. The wrapper also writes
`work/vtables.txt` for object scans. It does not collect live dumps automatically.
`tools/survey.sh` is a historical survey driver with fixed build_trace/game paths
and parallel writable-state hazards. Read it first and adapt experiments to the
private-game, namespace and timeout procedure in EXPERIMENTS.md; collect
`work/survey/m<N>/objs_<N>.txt` before rerunning the wrapper. Do not mistake an
empty model for observed coverage. Ghidra `DecompAddrs.java` can supplement missing
function exports; see its header and EXPERIMENTS.md.

The handoff tooling tests cover queue preference/fallback, bootstrap idempotence,
missing inputs, failure propagation and the cached analysis chain using invented
metadata. They do not verify a Ghidra lift or a real game revision.

Without an executable the wrapper reports skipped analysis and exits successfully.
With only an executable it generates text.asm and reports the missing lift.
Without a fresh Ghidra installation, cached gen_funcs.h/decompilation and live
dumps allow later stages to run; a new machine without those inputs cannot
reproduce them from the public repo alone. The independent engine remains usable.

## Resume development

Read [tasks/README.md](../tasks/README.md), choose the first eligible prompt in
`tasks/queue/`, and ask the assistant to read `CLAUDE.md` and execute it.
`python3 tools/codex_supervisor.py` resumes unattended work on a fully configured,
authenticated, writable machine; it creates/pushes feature branches and PRs and
can squash merge, so read its workflow before launching. This setup task does
not launch the supervisor. Private sessions/logs are not required for manual work.
