# Contributing

Use English for all new code comments, documentation, UI text, commit messages,
and other contributed text. Keep changes focused on the task and follow the
existing code style: two spaces, attached braces, and no tabs in C++.
The root `.editorconfig` and `.clang-format` describe these conventions. Format
only code you change; avoid unrelated formatting of existing files.

Never commit game assets, `Aztec.exe`, or code derived from the executable,
including generated C, decompiled source, and lifted code. Keep private game
installations and research outputs in ignored local directories. Review
`git diff --cached` before committing; ignore rules do not protect files that
are already tracked or explicitly added with force.

## Build

Requirements: CMake 3.16 or newer, a C++20 compiler, and SDL2 development files.
On Ubuntu, install `build-essential cmake libsdl2-dev`; Ninja is optional.
No game installation is needed to build the core, engine, viewer, path benchmark,
and test executables.

Run these commands from your checkout or worktree:

```sh
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build_release --parallel 2
ctest --test-dir build_release --output-on-failure
```

Use a build directory inside your current worktree. Do not reuse the main
checkout's `build/` or `build_*/` directories. An empty `AZTEC_GEN_DIR` keeps the
recompiled runtime out of the build, as in CI.

## Tests

A single CTest run covers all test executables:

| CTest name | Executable | Inputs |
| --- | --- | --- |
| `path` | `test_path` | Synthetic pathfinding and movement courses |
| `engine_unit` | `test_engine --pure` | In-memory engine, audio, and mixer checks |
| `formats` | `test_formats` | Your game installation |
| `engine` | `test_engine` | Your game installation and repository rules data |

`formats` and `engine` return skip code 77 when the game directory is absent.
They remain visible as skipped tests in CTest. Existing but incomplete or corrupt
installations can fail checks. Some optional files within an installation are
not required by every check.

Point the `GAME_DIR` CMake cache variable at a private installation containing
`Res/`, `Maps/`, and the other original data directories:

```sh
cmake -S . -B build_release -DGAME_DIR=/absolute/path/to/game
ctest --test-dir build_release --output-on-failure
```

The cache defaults to the `GAME_DIR` environment variable at configuration time.
A nonempty cache value takes precedence and is passed to tests through their
environment. With an empty cache value, tests read the environment at run time,
then fall back to the checkout's `game/` directory:

```sh
cmake -S . -B build_release -DGAME_DIR=
GAME_DIR=/absolute/path/to/game ctest --test-dir build_release --output-on-failure
ctest --test-dir build_release -L unit --output-on-failure
```

For direct execution, use `build_release/test_engine --pure` or pass a game
directory to `test_engine` or `test_formats`. A nonempty `GAME_DIR` environment
variable takes precedence over that directory argument. The engine's own
`--game` option and `AZTEC_GAME` environment variable are separate from this
test configuration.

## Run the tools

The following tools use the build directory above:

```sh
build_release/aztec_engine --help
build_release/aztec_engine --game /absolute/path/to/game 1
build_release/aztec_view /absolute/path/to/game /absolute/path/to/game/Maps/mission_1.map
build_release/path_bench --n 1,10,30 --ticks 1500 open gap1
```

`path_bench` uses synthetic courses and needs no game data. Use `--json` for
machine-readable benchmark output. Keep reports, screenshots, and logs under
`build_release/`, `work/`, or `test-output/` so they remain ignored.

Python tools under `tools/` and `src/tools/` document their arguments in their
opening comments or usage messages. Run them with `python3 -I path/to/tool.py`
and the documented arguments. Research tools may require your own installation,
Python packages, Ghidra, or private generated files. Read their entry points
before running them: some write into the game directory, some import optional
modules, and some assume a local `game/` or `build/` directory. They are not part
of the release build or CI tests. Keep all generated assets and executable-derived
outputs private and out of commits.

See the [documentation index](docs/README.md) and
[engine guide](src/engine/README.md) for the available research notes and engine
commands. Before submitting, build from a fresh directory, run CTest, review the
staged diff for prohibited files and unrelated edits, and commit with a clear
English message. Include the commands used for verification in your contribution.

## Branching model (gitflow)

- `main` holds released code only; `develop` is the integration branch and the default branch.
- Work on `feature/<topic>` branches created from `develop`; open a pull request into `develop`.
- Both `main` and `develop` are protected: changes arrive through pull requests with green `build` and `pre-commit` checks. Never push to them directly.
- Releases: open a pull request `develop` -> `main`, then tag `vX.Y.Z` on `main` (the release workflow builds the binaries).
- Hotfixes: `hotfix/<topic>` from `main`, merged into both `main` and `develop`.
- Install the hooks once: `pip install pre-commit && pre-commit install`.
