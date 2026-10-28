# Setup on a new machine

Start with root `CLAUDE.md` or `AGENTS.md`, then [HANDOFF.md](HANDOFF.md).
Use a feature branch from develop. This PUBLIC GPL-3.0 repository must never
receive game assets, Aztec.exe, generated/lifted C or decompiled source.
A clean clone builds and tests without any of those private inputs.

For prebuilt Linux/Windows releases, see [PACKAGES.md](PACKAGES.md): extraction,
external game-data setup, bundled rules/dependencies, optional ffmpeg and licenses.
The release workflow stages packages with `tools/package_release.py` and checks
the extracted archives in separate fresh runner jobs using `tests/test_package.py`.
To exercise local staging without downloading distro dependency sources:

```sh
python3 tools/package_release.py --binary build/aztec_engine --dest build/package-local --runtime local
unshare -rn env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy python3 tests/test_package.py build/package-local
```

Use a fresh staging destination each time. Local mode uses host libraries and is
not a redistributable release. Linux release bundling requires Ubuntu/Debian,
enabled deb-src indexes and network access for exact dependency sources; Windows
release staging requires the static SDL2 source/license paths from vcpkg. Neither
release mode reads private game data. Package rules resolve beside the executable;
source builds fall back to `docs/data`. Both modes accept `--rules-dir` overrides.

## Dependencies and independent build

Linux is the primary local development path. CI also builds the independent
engine with MSVC, MinGW (UCRT64), and Apple Clang and runs synthetic tests. Install CMake >=3.16, GCC/g++ with
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

CTest includes asset-independent checks and two game-data checks; the latter
skip without an installation. Skipping is not game-data verification. See
[CONTRIBUTING.md](../CONTRIBUTING.md) for GAME_DIR precedence and test labels.
Run pre-commit on all tracked files before a PR; use `--files PATH...` as well for
new untracked files while working with read-only git metadata.

## Windows and macOS independent builds

These builds cover `src/core/`, the independent engine, tools and synthetic tests.
Keep `AZTEC_GEN_DIR` empty: the private reference runtime has a separate Linux
workflow and is not ported by this setup. Use a recent compiler with C++20
support (Visual Studio 2022, current MinGW-w64 GCC, or current Xcode Clang).
CMake prefers SDL2's config package (>=2.0.18) and falls back to pkg-config.
SDL2 include paths and libraries come from its imported target; threads and
compiler warning flags are selected for the host. ffmpeg on PATH is optional
for intro playback. Windows video playback declines paths containing quotes,
percent signs or exclamation marks because the pipe uses cmd.exe.

For MSVC, install Visual Studio 2022 with Desktop development with C++, CMake,
Python 3, and vcpkg. From PowerShell at the repository root, with
`VCPKG_ROOT` set to your vcpkg installation:

```powershell
& "$env:VCPKG_ROOT/vcpkg.exe" install sdl2:x64-windows
cmake -S . -B build_msvc -A x64 "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build_msvc --config Release --parallel 2
ctest --test-dir build_msvc -C Release -L unit --output-on-failure
```

The vcpkg CMake integration copies SDL2.dll beside the executables. When using a
standalone SDL2 SDK instead, set `SDL2_DIR` to its CMake package directory and
put the matching SDL2.dll beside the executables or on PATH.

For MinGW, use the MSYS2 **UCRT64** shell and keep all dependencies in that
prefix; do not mix MSVC libraries or MSYS libraries with MinGW libraries:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-python
cmake -S . -B build_mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build_mingw --parallel 2
ctest --test-dir build_mingw -L unit --output-on-failure
```

For macOS, install Xcode command line tools (`xcode-select --install`), Homebrew,
CMake, SDL2, Ninja and Python 3:

```sh
brew install cmake sdl2 ninja python
cmake -S . -B build_macos -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix sdl2)" -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build_macos --parallel 2
ctest --test-dir build_macos -L unit --output-on-failure
```

CTest's `unit` label selects all synthetic checks, including malformed inputs,
case-insensitive resource lookup, unaligned samples and explicit little-endian
serialization. Python 3 must be discoverable to register the Python checks.
For headless checks set `SDL_VIDEODRIVER=dummy` and `SDL_AUDIODRIVER=dummy`.
Use `-C Release` with multi-configuration generators such as Visual Studio.
The CI matrix in `.github/workflows/build.yml` exercises all three setups;
local Linux checks alone do not verify native Windows or macOS behavior.
Run pre-commit separately as described above before submitting a PR.

Game paths may contain spaces. The engine resolves resource directory and file
components ignoring case, including `Res`, `Maps`, `Music` and `Video`, so a
case-sensitive macOS volume can read installations with different capitalization.
Prefer exact names if multiple entries differ only by case. Binary resources and
engine saves use explicit little-endian scalar access; WAV music is decoded into
aligned native samples before mixing. Existing little-endian engine saves retain
their format. These checks do not establish gameplay parity or exercise native
audio/video devices. Private game-data tests still need your own installation.

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

## Standalone map inspector

`aztec_map_viewer` is a separate, read-only tool using the engine mission loader
and core terrain/fog loaders. It does not start a simulation. Build it with the
normal build above, or `cmake --build build --target aztec_map_viewer --parallel 2`.

```sh
build/aztec_map_viewer --game /absolute/path/to/game
build/aztec_map_viewer --game /absolute/path/to/game 3
build/aztec_map_viewer --maps /absolute/path/to/Maps --list
build/aztec_map_viewer --maps /absolute/path/to/Maps --dump mission_3
build/aztec_map_viewer --maps /absolute/path/to/Maps --dump > build/maps.txt
```

The browser discovers every `.mp` file, including custom missions, in sorted,
case-insensitive filename order. Select a mission by number (`3` resolves to
`mission_3`), basename, or `.mp` path. PageUp/PageDown cycles missions. Arrow keys
scroll, middle-button dragging pans, the wheel zooms around the pointer, Home
resets the camera, and Escape exits. O toggles object/town markers, S toggles
start crosses, T toggles terrain-overlay markers, and F cycles through fog
layers and off. Click a tile to inspect its base/overlay IDs, placed object
type, side, HP and scenario ID, and values from every fog layer.

Terrain textures use the mission tileset when resource archives are available;
without archives the view uses deterministic tile ID colors. Overlays appear as
small pale squares, towns as boxes labelled T plus town index, placed objects
as boxes labelled with their type ID, and starts as crosses labelled S plus side
index. Marker colors identify side indices modulo eight. These are inspection
symbols, not an animated or pixel-parity game renderer. Object/start markers
remain visible above fog for inspection. Fog values are shown as darkening
(0 lit, 152 fully dark); grids with different dimensions are scaled over the
terrain extent. Missing `.vis` means no fog layers; malformed present fog fails
loading. Terrain lookup prefers the mission basename, then `MapName` in its `.mp`.

`--dump` needs neither SDL initialization nor resource archives. With no mission
argument it dumps all discovered missions; otherwise it dumps the selected one.
The deterministic text includes terrain rows as `base:overlay`, side start
positions (including disabled `-1,-1` starts), town records, every placed object
and its source/group/scenario fields, and each fog layer's original dimensions
and byte rows. It returns nonzero on bad input and continues past failed entries
when dumping all missions. `--list` only lists filenames. The synthetic CTest
`map_viewer` verifies dumps, errors and a dummy-SDL rendering smoke check;
`--frames N` allows a bounded SDL view for smoke checks. No private data is needed.

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
