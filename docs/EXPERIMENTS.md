# Running experiments with the reference runtime

The reference runtime (`runtime/` + lifted code generated locally into `work/gen`, never committed) runs the original game natively
and is used as a test oracle and a data-extraction tool. It needs the user's own game files in `game/` (untracked).

## Build
```
cmake -S . -B build -DAZTEC_GEN_DIR=$PWD/work/gen          # normal build
cmake -S . -B build_trace -DAZTEC_GEN_DIR=$PWD/work/gen -DAZTEC_TRACE_CALLS=ON   # function coverage
```
Use a private build directory per task (`build_<task>`); they are git-ignored.

## Running headless
- `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_RESOLUTION=800x600 RT_SCRIPT=file.txt RT_DUMP_DIR=dir build/aztec_recomp game/Aztec.exe`
- Several instances in parallel must each run in their own network namespace (the game binds UDP 32008 and fails otherwise):
  `unshare -rn sh -c "ip link set lo up; ip addr add 192.168.1.125/24 dev lo; exec build/aztec_recomp game/Aztec.exe"`
- Never run two instances that write the same files in `game/` (Saves, Maps, *.plr). For custom maps build a private game directory
  (symlink `Res`, `Video`, `Music`, `Aztec.exe`; private `Maps`, `Saves`) and point the runtime at its `Aztec.exe`.
- Always wrap runs in `timeout -s KILL N`.

## Script commands (`RT_SCRIPT`, one per line, coordinates in game pixels)
`wait MS`, `move X Y`, `click X Y`, `rclick X Y`, `down`, `up`, `dragto DX DY` (relative), `key DIK`, `keydown/keyup DIK`, `shot`,
`quit`, `f9`, `cheat unlock|win|gold`, `objscan VTABLES OUT` (live objects), `heapdump PATH`, `scan/snap/diff` (memory search),
`poke ADDR VAL`, `pokeside OFFSET VAL`. `RT_TIMESCALE=N` speeds up game time (>=2 skips videos).
Environment: `RT_STATE_LOG=file` (per-tick state hashes), `RT_CMD_LOG=file` / `RT_CMD_REPLAY=file` (record/replay lockstep commands),
`RT_UNIT_LOG`, `RT_DEBUG=1 RT_DEBUG_FILE=file` (categorised debug log), `RT_CALLS_FILE` (coverage with the trace build).

## Menu navigation (800x600)
Player select `(140,239)` then accept `(160,543)`; campaign row `y = 257 + 20*(mission-1)` (missions 10-15: press next `(645,421)` first);
play `(140,544)`; briefing start `(700,430)`; custom missions button `(650,58)`, list rows from `y = 258`, accept `(160,543)`;
in-game menu button `(626,10)`, save `(250,190)`; slot `y = 138 + 20*slot`; save confirm `(184,359)` then `key <DIK>` per character and Enter (28).

## Test maps
`tools/make_ztest.py`, `tools/make_zspr.py`, `tools/make_zstat.py`, `tools/make_zpath.py` generate custom maps (INI `.mp`, see `docs/formats.md`).
Buildings must be placed through `[Town]` sections, mobile units through `[Team]` (a building in `[Team]` crashes). `.scn` files must use CRLF.
Remove generated maps from `game/Maps` when done.

## Analysis tools
`tools/analyze_classes.py`, `class_tree.py`, `class_model.py`, `source_map.py` (class and source-file recovery), `tools/coverage.py`,
`tools/survey.sh`, `tools/corpus.sh` (record/replay corpus), `tools/ztest_run.sh`, `tools/zstat_*`, `tools/zspr_*`, `tools/zpath_*`.
The Ghidra decompilation (`work/decomp/all.c`) is local-only; `tools/ghidra_scripts/DecompAddrs.java` decompiles functions missing from it.
