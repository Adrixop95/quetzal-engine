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

## Isolated rule experiments

`make_zstat.py`, `zstat_run.sh` and `zstat_duels.py` require `ZSTAT_GAME_DIR` pointing at a private game copy and `ZSTAT_RUNTIME` pointing at the reference executable. Never point the writable copy at the reference installation. Create ordinary private `Maps` and `Saves` directories, symlink only read-only resources/executable, and copy player/configuration files. For example, from the worktree root:

```sh
mkdir -p work/rules-game/Maps work/rules-game/Saves
reference_game=/absolute/path/to/read-only/game
for item in Aztec.exe Res Video Music Campaign; do
  ln -s "$reference_game/$item" "work/rules-game/$item"
done
cp "$reference_game"/*.plr "$reference_game"/*.ini work/rules-game/
cp "$reference_game"/Maps/mission_1.* work/rules-game/Maps/
export ZSTAT_GAME_DIR="$PWD/work/rules-game"
export ZSTAT_RUNTIME=/absolute/path/to/reference/aztec_recomp
python3 -I tools/make_zstat.py zrules 0:0:1:3:4 1:14:1:8:4
sh tools/zstat_run.sh zrules work/zrules 40000
```

The runner creates a further private game directory inside OUTDIR for every invocation, copies only the requested map into its map list, runs headless with both dummy SDL drivers inside `unshare -rn`, and propagates runtime failures. It uses the first custom-map row (258), overridable with `MAPROW`. The runner configures loopback and the private local address inside the namespace before launching the game. A denied setup command is a failed experiment; never run outside the requested namespace to avoid that failure.

The generator removes mission 1's inherited footmen and places each generated town hall eight tiles away from the tested structure. Teams have consecutive section numbers even when building specs are interspersed. Defence HP comes from d3; Marine/Bird/Zeppelin primary HP from d1. The old tool used armor or secondary state fields as HP and placed the hall on the target, invalidating structure/air conclusions. The historical unit log's raw object +0x3c is not virtual max HP, and its side column may be -1 because +0x30 is a tactic pointer. Match objects by type and initial position; inspect their orders and visibility before interpreting no hits.

Recommended category matrix: footman/archer/rifleman against barracks (14), watch tower (17), eagle (33) and zeppelin (7), plus a footman ground-target control. Use separate maps or distant slots, no inherited units, initial separation both inside and outside weapon range, and repeat with explicit target/town orders as well as idle acquisition. Ordinary buildings can be skipped by normal global tactic assignment even when the weapon can damage them. A zero-hit result alone does not verify immunity.

On 2026-10-25 the corrected custom-map experiment was attempted in the restricted worktree environment. `unshare -rn` started, but interface configuration was denied and the supplied runtime aborted at UDP socket creation (`RaiseException e06d7363`). New category, population and timing observations remain open; see `rules_summary.md`. Decompilation and x86 inspection were still possible. Keep all private maps, dumps, screenshots, Ghidra projects and generated source under ignored `work/` (or temporary directories), never in a commit.

## Projectile measurement protocol

The 2026-10-28 projectile probe used a private game copy with read-only resource
symlinks, ordinary Maps/Saves directories and copied player/settings files.
The supplied `/home/adrox/aztec-re/build/aztec_recomp` was selected through
`ZSTAT_RUNTIME`. After generating `zprojectile` with archer type 27 at (8,8)
and enemy footman type 0 at (10,8), the following run failed:

```sh
ZSTAT_GAME_DIR="$PWD/work/projectile-game" \
ZSTAT_RUNTIME=/home/adrox/aztec-re/build/aztec_recomp RT_UNIT_EVERY=1 \
  sh tools/zstat_run.sh zprojectile work/projectile-probe 12000
```

The runner returned status 1 at `ip link set lo up` inside `unshare -rn`:
`Cannot open netlink socket: Operation not permitted`. The game did not launch,
and no unit log was produced. Private inputs, script and failure output remain
under `work/projectile-game/` and `work/projectile-probe/`. No projectile,
collision, burn or damage rule can be inferred from this run. Rerun on a host
where the documented namespace setup succeeds; do not bypass isolation.

Use these controls when measurements become possible:

1. Establish a successful record/replay control before interpreting weapon
   results. Capture commands, per-tick unit HP/positions and synchronized
   projectile observations. The existing unit log covers units, not projectile
   altitude or collision events; it cannot establish a ballistic arc alone.
2. For Arrow, Axe and BallExpl, repeat stationary shots at several measured
   separations on clear ground. Record launch/impact ticks, ground position and
   render height separately. Repeat with a moving target and a source killed
   during flight to distinguish snapshot aim, tracking and flight cancellation.
3. Compare each clear-ground control with the same endpoints and an intervening
   terrain feature, structure or unit. Vary obstacle placement and height
   independently. Do not equate movement passability with projectile collision
   without observations. Record the actual impact location and all HP changes.
4. For flame, observe a single launch through the end of all damage events,
   separating travel, impact animation and subsequent burn ticks. Repeat after
   moving the victim and after source death. Structure damage-fire animations
   alone do not establish damage-over-time behavior.
5. Measure splash on each victim independently: physical armor, structure,
   airborne, organic/nonorganic and overlapping category flags. Use a direct
   target and neighbours at several distances, including the observed radius
   boundary, with allied/neutral controls. Keep reloads and overlapping shots
   out of the measurement window and account for regeneration. A payload
   computed for the direct target is not evidence of identical neighbour damage.

Only after these observations should integer trajectory parameters, collision
ordering, burn schedules and per-victim damage calculations replace the engine
placeholders. Add synthetic boundary and command-replay tests for each measured
rule, including state-hash coverage for any new flight or burn state. Keep
unmeasured weapon families explicitly unresolved.
