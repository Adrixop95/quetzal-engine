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


## Packet replay and determinism audit

The current parity runner requires a rebuilt private reference runtime that logs
injected non-NOTHING packets to `RT_CMD_LOG`; older runtimes returned before that
recording point. Exact frame/side/dword readback is checked before repeatability.
See [parity.md](parity.md#v02-replay-audit-2026-10-28) for the fail-closed parser,
normalized repeatability requirements and the before-command observer boundary.
Raw pool differences remain diagnostics, with excluded fields unaudited.

The 2026-10-28 isolated probe used:

```sh
python3 tools/parity_report.py run \
  --corpus /home/adrox/aztec-re/work/corpus \
  --game /home/adrox/aztec-re/game \
  --runtime /home/adrox/aztec-re/build/aztec_recomp \
  --output work/parity/v02-isolated --timeout 45
```

All nine recorded missions failed during loopback setup; no runtime simulation
was observed. Do not reuse that output directory for a retry or count these
failures as reference repeatability. Synthetic long replay, command ordering,
RNG-stream and hash checks are documented separately in [determinism.md](determinism.md).
## UI capture controls

`ctest --test-dir build -R ui_screenshots --output-on-failure` needs no game
installation. It writes only synthetic captures into `build/ui-screenshots/`;
its region comparisons and tolerances are described in engine_rules_status.md.

For local visual review, prepare a private game directory as above, then run:

```sh
timeout -s KILL 30 unshare -rn env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/aztec_engine --game "$PWD/work/ui-game" --rules-dir "$PWD/docs/data" \
  --size 800x600 --software --no-audio --no-hud --no-light --no-sim \
  --screenshot "$PWD/work/ui-probe/engine.png" 4
```

The independent engine does not need the reference runtime's namespace network
interface setup. A successful engine capture does not resolve a failed reference
capture. For the reference use the setup and navigation documented above, with
`RT_RESOLUTION=800x600`, a private `RT_SCRIPT` and `RT_DUMP_DIR`. The 2026-10-28
UI attempt failed at namespace loopback configuration with a netlink permission
error before the game launched. No reference UI observations were obtained.
Keep every capture containing original assets in ignored `work/`; never commit
it. For a future differential comparison, align mission, camera, selection,
gold, UI scale, cursor, tooltip delay and message state before measuring each UI
region separately. Report exact agreement and tolerant mismatch rates separately.

### Frame-scripted UI catalogue

`tools/ui_capture.py` reads `tests/parity/ui/screens.json`. Each entry has separate
reference and clone routes; an explicit blocker replaces an unavailable route.
See [ui_screens.md](ui_screens.md) for the inventory and measurement limits.

```sh
python3 tools/ui_capture.py --list
python3 tools/ui_capture.py --screen hud-mission-1 --phase before \
  --game /absolute/path/to/read-only/game \
  --runtime /absolute/path/to/rebuilt/aztec_recomp
# After a change, repeat with --phase after and the same --output directory.
```

The default output is this worktree's ignored `work/parity/ui/`. A phase must be
fresh. Both runners use separate private game copies for each repetition, dummy
SDL video/audio, a kill timeout and `unshare -rn`. Reference network setup remains
mandatory and fails closed. At least two independent processes must produce
identical source capture bytes. Canonical lossless RGB PNGs are also compared.
The tool records exact differing-pixel counts, mismatch fractions, mean absolute
error, root mean square error and maximum channel error when both images exist.
`before-after.json` retains null for unavailable measurements. Failed or blocked
captures return a nonzero exit status; missing images never count as zero error.

The reference needs a runtime rebuilt with the opt-in `RT_UI_SCRIPT` hook in
`runtime/backend.c`; the supplied prebuilt runtime may lack it. Build privately
inside the current worktree, using the user's local generated source directory:

```sh
cmake -S . -B build_ui_reference -DCMAKE_BUILD_TYPE=Release \
  -DAZTEC_GEN_DIR=/absolute/path/to/private/work/gen
cmake --build build_ui_reference --target aztec_recomp --parallel 2
```

`RT_UI_SCRIPT` is an absolute guest-presentation schedule, with lines such as
`4 move 140 239`, `5 down`, `6 up`, `10 shot`, `11 quit`.
The hook applies input before the next guest presentation and captures the
composed renderer synchronously. It replaces the threaded `RT_SCRIPT` path.
It does **not** virtualize the reference clock. A presentation can be a partial
redraw; menus may stop presenting while idle. Thus the hook is experimental,
may time out, and cannot establish tick alignment or deterministic reference
rendering until measured. Do not substitute millisecond sleeps for these gaps.

The clone front end accepts `AZTEC_UI_SCRIPT` using the existing playtest grammar:
`wait N` counts rendered frames, `move`, `down`, `up`, `key`, `shot PATH`, `quit`.
Cursor animation uses frame-derived time. This script is limited to the front
end; mission captures start the requested mission directly using `--script`.
`--menu-session` enables pause/objectives when launching a mission directly.
Gameplay scripts advance one simulation tick per frame; front-end scripts do not
advance a world. Reference and clone frame counts are not equivalent simulation
observations. L4 structural masks/alignment and L5 shared semantic UI/command
observers remain prerequisites for parity claims; existing `parity_visual.py`
can compare aligned bundles once those observations exist.

## Generated showcase and battle maps

`tools/make_testmaps.py` writes `ztest_all` and `ztest_battle` into an explicitly
supplied **private** game copy. It reads public type/HP metadata and the selected
campaign mission at run time; it never reads an executable. It copies `.map`,
`.dat`, `.rm`, `.vis` and `.spr` unchanged, generates English `.mp`/CRLF `.scn`
files, and writes a private `.layout.json` coordinate/health manifest. No generated
file belongs in the public repository. Existing outputs require `--force`;
symlink outputs are rejected even with that option.

Prepare ordinary Maps/Saves directories and symlink only immutable resources:

```sh
mkdir -p work/testmaps-game/Maps work/testmaps-game/Saves
reference_game=/absolute/path/to/read-only/game
for item in Aztec.exe Res Video Music Campaign; do
  ln -s "$reference_game/$item" "work/testmaps-game/$item"
done
# Copy configuration/profile files when present; never symlink writable profiles.
cp "$reference_game"/*.plr "$reference_game"/*.ini work/testmaps-game/
# Include all case variants of the selected campaign basename.
find "$reference_game/Maps" -maxdepth 1 -iname 'mission_14.*' \
  -exec cp '{}' work/testmaps-game/Maps/ \;
python3 tools/make_testmaps.py --game work/testmaps-game
python3 tools/make_testmaps.py --game work/testmaps-game --force --variant cvc
python3 tools/make_testmaps.py --game work/testmaps-game --force --variant stress --races 2,0
```

The default template is mission 14 (180x180); `--source-mission mission_11`
selects another existing campaign. Filenames are resolved ignoring case. A
minimum 180x180 template is required. The generator preserves terrain rather
than inventing a fully open map. Mission 14 is mostly clear, but obstacles move
placements to the nearest clear, reserved footprint with stable distance/y/x
ordering. The layout therefore describes a **target grid**, not an exact labelled
floor texture. English mission notes label race rows and category columns;
`.layout.json` supplies actual coordinates. Units reserve 3x3 tiles, buildings
and automatic halls reserve 5x5 tiles, with an additional one-tile gap. Enough terrain must
exist or generation fails before outputs are written. Terrain spacing is a test
layout policy, not a measured sprite/weapon footprint guarantee.

`ztest_all` has three race rows, native buildings and units plus shared special
types, and explicit HP at 100/66/33/10 percent of the public descriptor HP. All
four town levels are present for every race; Aztec fortress falls back to its
town, as the format specifies. Each Town also creates an implicit healthy hall;
separate explicitly placed hall types exercise damaged hall sprites. Buildings,
including shipyard type 86, occur only in Town sections; mobile types occur in
Team sections. Neutral mines use side 3 and the copied resource map. Separate
copies of the mobile set exercise idle, move, path/inverted path, cyclic/inverted
cyclic path and locked tactics through deterministic `.scn` actions. The ordinary
`.mp` Team record has no order field. Friendly diplomacy keeps race showcases
from fighting; scenario order movement can eventually leave the initial cells.

Types 84/88 use Boat/Johnka sections at the off-map entry positions supported by
campaign files. They remain inactive: copied land terrain is not evidence of a
navigable water route. Type 89 uses Side Camera records, rather than an unsafe
Team insertion. These special records do **not** establish visible vessel/camera
object coverage or nautical combat parity. Shipyard placement on land also tests
loading only. No independent construction/burning/death-state field is known;
zero HP is deliberately not treated as a verified destroyed-state encoding.

`ztest_battle` uses two distinct races (`--races 0,1` by default), corresponding
armies facing across the map, native factories/defences and a fortress per side,
a neutral third group, empty DisabledID and one million starting money. Each army
contains its native mobile combat types plus Abdula, catapult, second zeppelin
and hopping cannon. The layout mirrors target positions, with deterministic
terrain relocation; it does not promise exact mirrored final coordinates.
Reserve groups are preplaced and locked, then released with attack-town orders
at localTime 300 and 600. These are **reserve waves**, not newly spawned units.
Loss of all enemy towns wins; loss of all player towns loses. Timings are scenario
clock values, not a measured wall-clock duration. `pvc` keeps side 0 human; `cvc`
marks both sides OnlyComp; `stress` multiplies each army group sixfold and keeps
side 0 human. OnlyComp is effective in the clone; original computer control and
full technology/prerequisite behavior still require live reference validation.
Empty DisabledID enables IDs but does not bypass factory/town prerequisites.

Inspect and launch using a private profile and bounded, isolated processes:

```sh
build/aztec_map_viewer --maps work/testmaps-game/Maps --dump ztest_all
build/aztec_map_viewer --maps work/testmaps-game/Maps --dump ztest_battle
for map in ztest_all ztest_battle; do
  timeout -s KILL 30 unshare -rn env \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    AZTEC_USER_DIR="$PWD/work/testmaps-profile" \
    build/aztec_engine --game "$PWD/work/testmaps-game" \
    --size 800x600 --software --no-audio --no-sim \
    --screenshot "$PWD/work/$map.png" "$map"
done
# Each reference run gets another private copy containing only the selected map.
ZSTAT_GAME_DIR="$PWD/work/testmaps-game" \
ZSTAT_RUNTIME=/absolute/path/to/reference/aztec_recomp \
  sh tools/zstat_run.sh ztest_all work/testmaps-reference-all 1200
# Use a fresh output directory and ztest_battle for the combat probe.
```

`tests/parity/testmaps.json` records map kind `campaign-derived`, semantic input
recipes and explicit missing adapters for selection, construction, queues and
burning/destruction. It is a provisional L4 specification, not an executable
`parity_run.py` L0 scenario. The current clone rejects `.scn buildCommand` and
vessel activation, and its playtest grammar has no cheat command. Reference
`cheat unlock`/`cheat gold` can prepare its UI route; the generated clone already
has money and enabled IDs. Existing shared UI/command observers and production
coordinate adapters are incomplete. Thus deterministic cross-program scripted
state reachability is still outstanding; these recipes must not be counted as
executed parity tests. See also the new UI inventory entries in `ui_screens.md`.

Synthetic CTest `testmaps_tools` runs the CLI on invented terrain, checks section
counts, IDs, coordinates, all ordinary object types, all HP fractions, automatic
hall separation, building/mobile separation, town levels, CRLF scripts, variants,
repeatability, unchanged source/copy bytes and overwrite/symlink rejection. It
also dumps both maps through the actual map viewer. It needs no original data.

Cleanup is explicit: remove only generator-owned files from the **private** Maps
directory after experiments (the layout manifests are private outputs too):

```sh
python3 - <<'PY'
from pathlib import Path
maps = Path('work/testmaps-game/Maps')
for name in ('ztest_all', 'ztest_battle'):
    for ext in ('map', 'dat', 'rm', 'vis', 'spr', 'mp', 'scn', 'layout.json'):
        (maps / f'{name}.{ext}').unlink(missing_ok=True)
PY
```

Local verification on 2026-11-04: Release build and all 38 runnable CTest checks
passed; six checks skipped for private inputs/restricted networking. Both private
maps dumped successfully and produced 800x600 clone captures. The final showcase
(685 explicit objects plus automatic halls and special records) ran 24 ticks;
player/computer combat ran 720 ticks, computer/computer ran 720 ticks and stress
(1047 explicit objects) ran 24 ticks. These bounded probes reported no script
errors or unsupported actions; reaching the requested horizon is not a victory
or a soak/parity result. Both reference probes failed at `ip link set lo up`
with `Cannot open netlink socket: Operation not permitted`, before the binary
launched. Original loading, vessel entry routes, original computer control and
cross-program UI state reachability remain unverified. Full pre-commit could not
initialize its remote hook environment because github.com DNS resolution failed;
local asset/English hooks, JSON parsing and whitespace checks passed separately.
