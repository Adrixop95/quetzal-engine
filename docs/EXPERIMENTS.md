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

## Campaign transport and objective run sheet: needs live run

The task-71 sandbox explicitly denies live reference runs. Do not execute this
sheet there or remove `unshare -rn` to work around a denial. All commands below
are for a host allowing the documented namespace/network setup. Run from this
worktree root in Bash. Keep every generated file under ignored `work/`.
No successful reference observation is supplied by this change.

First retain the complete static inventory and the paired engine findings:

```sh
mkdir -p work
campaign_inventory=$(mktemp -d "$PWD/work/campaign-inventory.XXXXXX")
build/aztec_script_audit --maps /home/adrox/aztec-re/game/Maps \
  > "$campaign_inventory/counts.tsv" 2> "$campaign_inventory/sites.txt"
# This strict gate currently returns 1 for 19 unsupported sites; retain the failure.
build/aztec_script_audit --maps /home/adrox/aztec-re/game/Maps --require-supported \
  > "$campaign_inventory/strict.tsv" 2> "$campaign_inventory/strict.txt"
python3 tools/playthrough.py --game /home/adrox/aztec-re/game \
  --no-auto-orders --max-ticks 14400 --timeout 180
# The printed report directory contains actions.md, matrix.md and summary.json.
# Repeat with --scenarios work/campaign-routes/scenarios.json once normal player
# routes and mid-mission save ticks are available. No route is supplied here.
```

For reference controls, prepare **separate** record and replay installations for
all 15 missions. This uses only read-only resource/map symlinks, ordinary private
Maps/Saves directories and copied writable profiles. It generates normal campaign
menu navigation, not a custom-map substitution. The route is initially a passive
one-minute probe; optional `work/campaign-routes/mission_N.rt` files can append
ordinary input commands after entering the mission. Do not put cheats, memory
writes or forced outcomes into acceptance routes. Frame/time sleeps are navigation
assumptions: verify the entry captures before interpreting any logs.

```sh
campaign_reference=$(mktemp -d "$PWD/work/campaign-reference.XXXXXX")
export campaign_reference
python3 - <<'PY'
import importlib.util
import os
from pathlib import Path

spec = importlib.util.spec_from_file_location('playthrough', 'tools/playthrough.py')
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)
source = Path('/home/adrox/aztec-re/game')
maps, available = harness.discover(source)
assert all(m in available for m in range(1, 16))
output = Path(os.environ['campaign_reference'])
for mission in range(1, 16):
    directory = output / f'mission_{mission}'
    directory.mkdir()
    for phase in ('record', 'replay'):
        run = directory / phase
        run.mkdir()
        harness.private_game(source, maps, run / 'game')
    commands = ['wait 6000', 'click 140 239', 'wait 800', 'click 160 543', 'wait 5000']
    if mission >= 10:
        commands += ['click 645 421', 'wait 800']
    row = mission - 1 if mission < 10 else mission - 10
    commands += [f'click 150 {257 + 20 * row}', 'wait 800', 'shot',
                 'click 140 544', 'wait 3000', 'click 700 430', 'wait 7000', 'shot']
    route = Path('work/campaign-routes') / f'mission_{mission}.rt'
    if route.is_file():
        commands += route.read_text().splitlines()
    commands += ['wait 60000', 'shot', 'quit']
    (directory / 'input.rt').write_text('\n'.join(commands) + '\n')
PY
sha256sum /home/adrox/aztec-re/build/aztec_recomp > "$campaign_reference/runtime.sha256"
for mission in $(seq 1 15); do
  mission_probe="$campaign_reference/mission_$mission"
  for phase in record replay; do
    run_probe="$mission_probe/$phase"
    replay_env=()
    if [ "$phase" = replay ]; then
      # A missing recording is a failure; never replace it with an empty input.
      test -s "$mission_probe/record/commands.txt" || break
      replay_env=("RT_CMD_REPLAY=$mission_probe/record/commands.txt")
    fi
    (
      cd "$run_probe/game" || exit 1
      timeout -s KILL 200 unshare -rn env -u RT_CMD_REPLAY \
        SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_RESOLUTION=800x600 \
        RT_SCRIPT="$mission_probe/input.rt" RT_DUMP_DIR="$run_probe" \
        RT_UNIT_LOG="$run_probe/units.log" RT_UNIT_EVERY=1 \
        RT_STATE_LOG="$run_probe/state.txt" RT_CMD_LOG="$run_probe/commands.txt" \
        "${replay_env[@]}" sh -ec '
          ip link set lo up
          ip addr add 192.168.1.125/24 dev lo
          exec /home/adrox/aztec-re/build/aztec_recomp "$1/Aztec.exe"
        ' sh "$run_probe/game"
    ) > "$run_probe/stdout.log" 2> "$run_probe/stderr.log"
    printf '%s\n' "$?" > "$run_probe/exit-status.txt"
  done
done
```

Retain nonzero exit statuses, absent logs and wrong-menu captures as failed
experiments. Existing reference logs are not a script-action observer: dispatch
ticks, cargo membership and ownership must be established with aligned unit/object
observations and normal-input controls. Raw engine and reference hashes use
different representations and cannot be compared directly. Audit packet readback
and reference repeatability as described in [parity.md](parity.md) before using a
recording as an oracle. A passive probe cannot explain dormant late objectives.

For each of the 19 sites in [the disposition table](campaign_scripts.md#transport-follow-up-2026-11-04-needs-live-run),
record the initial vessel identity/side/position, script dispatch tick, first
motion tick, approach route and waypoints, boarding candidates and boarding order,
capacity/eligibility, loaded membership, unload coordinates/order, final route
and completion tick. Vary candidate placement/order, off-map entry, occupied
landing points, repeated activation and Johnka initial enabled state separately.
Use matched inactive and delayed-activation controls in **private copies**; retain
which data changed. Preserve original campaign scripts for the acceptance run.
Camera activation requires separate duration/locking and second-argument controls.

For every victory/defeat chain, record prerequisite trigger enablement, condition
transitions, alliances, gifts and protected tactic membership before and after
transport. Pay particular attention to mission 11's town-derived protected group;
the current engine's immediate defeat is not evidence of original behavior.
For a stalled engine ID, rerun its normal route with the existing observation
option, keeping outputs private:

```sh
# Select an already prepared private game directory; do not use the shared installation.
timeout -s KILL 180 unshare -rn env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/aztec_engine --headless --game "$campaign_reference/mission_2/record/game" \
  --rules-dir "$PWD/docs/data" --no-auto-orders --max-ticks 14400 \
  --report "$campaign_reference/mission_2/engine.json" \
  --observations "$campaign_reference/mission_2/engine-objects.jsonl" --order-observations 2
```

Inspect order/destination/target changes with position and passability; 600 ticks
without positional progress is only a lead. Require mission-specific normal
routes, paired engine replay, mid-mission save continuation and aligned reference
outcomes before promoting V12/V19 evidence. When a site is not observed, keep its
status **needs live run** until a reachable route or an evidenced original
nonexecution explanation exists. Do not infer that the original never fires it.
## V07 projectile run sheet (needs live run)

The task-74 tooling adds **P** placement specifications and **S** synthetic
observer checks, not **M** reference measurements. Use
`python3 -I tools/projectile_measure.py list` to enumerate 126 cases: arrow
(type 27), bolt (30), catapult (83), cannon (4), fire (35), magic (28) and air
(34), each with 18 controls. Family names are candidate selections; air type 34
and standalone flame behavior are unresolved in the engine. The JSON protocol
is [projectiles.json](../tests/parity/projectiles.json). It includes three
separations, moving target, source death, terrain/structure/unit interception,
five victim categories, allied/neutral bystanders and three burn controls.
Every case creates a separate `ztest_battle` placement candidate on mission 14
terrain; it does not alter the existing full battle-map generator.

Prepare one case from the read-only installation, on a machine that can run the
required namespaces. Run from this worktree root; use new output directories
for every attempt:

```sh
python3 -I tools/projectile_measure.py prepare --id arrow_clear_middle \
  --game /home/adrox/aztec-re/game \
  --output work/parity/projectiles/arrow-clear-middle
build/aztec_map_viewer \
  --maps work/parity/projectiles/arrow-clear-middle/game/Maps --dump ztest_battle \
  > work/parity/projectiles/arrow-clear-middle/map-dump.txt
```

Only immutable resources/executable are symlinked. Maps/Saves and copied settings
are private. `scenario.json`, `pending.json` and the map's `.layout.json` preserve
requested and actual placements. The generator relocates occupied/impassable
slots. It does **not** silently apply the tick-12 attack, tick-13 move/source
kill, suppress reloads, construct a terrain-height obstacle, set pixel splash
offsets or control original AI/visibility/regeneration. Resolve `pending.json`
with an audited setup/command adapter before treating this as a measurement.
Friendly/neutral variants retain an enemy direct target and add a bystander;
no-hit friendly acquisition would not measure friendly splash.

The independent engine can already capture its implemented combat behavior:

```sh
case_dir="$PWD/work/parity/projectiles/arrow-clear-middle"
timeout -s KILL 100 unshare -rn env \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  AZTEC_USER_DIR="$case_dir/engine-profile" \
  "$PWD/build/aztec_engine" --headless --game "$case_dir/game" \
  --rules-dir "$PWD/docs/data" --no-auto-orders --max-ticks 720 \
  --combat-trace "$case_dir/engine-combat.jsonl" \
  --observations "$case_dir/engine-units.jsonl" \
  --tick-hashes "$case_dir/engine-hashes.txt" \
  --report "$case_dir/engine-report.json" ztest_battle
python3 -I tools/projectile_measure.py summarize \
  --trace "$case_dir/engine-combat.jsonl" \
  --output "$case_dir/engine-samples"
```

This remains an engine candidate with native acquisition, not the isolated
single-shot experiment. Engine-local traces cannot be relabeled as reference
captures. Resume traces are rejected: the diagnostic flight IDs would otherwise
lack their launch records. An early mission outcome or missing tick/position
makes the summarizer fail without exporting samples. An in-flight projectile at
the complete horizon has `impact_tick: null`; extend the horizon before claiming
its travel time. An empty complete trace is zero engine launches, never a verified
reference immunity or unsupported-weapon implementation.

Reference launch, record and two replay controls (exact commands; **not run in
Codex**, where namespace networking is denied):

```sh
case_dir="$PWD/work/parity/projectiles/arrow-clear-middle"
export ZSTAT_GAME_DIR="$case_dir/game"
export ZSTAT_RUNTIME=/home/adrox/aztec-re/build/aztec_recomp
export RT_UNIT_EVERY=1 TMO=120
mkdir "$case_dir/record"
RT_CMD_LOG="$case_dir/record/commands.txt" \
RT_STATE_LOG="$case_dir/record/state.txt" \
  sh tools/zstat_run.sh ztest_battle "$case_dir/record" 65000
for repetition in 1 2; do
  replay_dir="$case_dir/replay-$repetition"
  mkdir "$replay_dir"
  RT_CMD_REPLAY="$case_dir/record/commands.txt" \
  RT_CMD_LOG="$replay_dir/commands.txt" RT_STATE_LOG="$replay_dir/state.txt" \
    sh tools/zstat_run.sh ztest_battle "$replay_dir" 65000
done
```

`zstat_run.sh` makes another pristine private game copy for each launch, sets
both dummy SDL drivers, wraps the process in `timeout -s KILL`, and runs
`unshare -rn sh -ec` with loopback/address setup. Any namespace failure is an
experiment failure; never retry without isolation. Its UI waits are milliseconds,
not simulation tick guarantees: verify the actual tick horizon in the logs.
Keep captures, command/unit logs and generated maps in ignored `work/`.

The supplied runtime's unit log has no audited projectile identity, ground
coordinates/altitude, impact, victim-distance or burn lifecycle observer. There
is currently **no reference projectile-log environment switch** supplied by this
task. The commands above exercise launch/record/replay prerequisites only; they
cannot fill that gap. V02/L1 setup/command and reference semantic observer work
remains a dependency. Verify packet readback and normalized reference repeats as
specified in [parity.md](parity.md) before interpreting combat differences.
Do not substitute raw state-hash equality or unit HP differences for trajectory
or burn observations. Repeat the prepare/run sequence with another listed ID
and fresh directories for every family/control after the adapter is available.

The `engine-combat-v1` JSONL contract has an engine-origin header, 1-based first
and requested last tick, position scale 256 and height scale 1. Events are in
simulation execution order inside each tick:

- `launch`, `position`, `impact`: stable diagnostic flight ID, source/target IDs,
  side, effect kind, ground x/y (pixels times 256), integer render height and raw
  ball altitude (pixels times 256). Direct weapons emit launch/impact in one tick;
  their effect kind is Decoration, not a fabricated projectile class.
- `damage` and `miss`: victim identity/type/side, diplomacy (0 enemy, 1 friend,
  2 neutral), victim ground position **at evaluation**, and HP before/after.
  Misses describe living victims evaluated without damage, including immunity,
  range or diplomacy filtering; they do not identify which condition rejected
  the victim. Negative HP is retained before death handling.
- `tick`: end-of-tick object rows `[id,type,side,x,y,hp,alive]`, including dead
  objects. `end` confirms the last completed tick. Existing `--observations`
  remains a separate before-command unit stream.

The summarizer validates full coverage and projectile lifetimes, then exports
per-flight trajectories, launch/impact ticks and per-victim damage/miss samples
with squared fixed-point distance and diplomacy. Splash radius/falloff rules,
burn duration and friendly-fire rules remain null. These samples can expose
engine behavior for later comparison; they cannot establish a rule from one
victim/shot. Splash measurement needs matched actual distances, category flags,
zero-damage controls and refined inner/outer boundaries. Burn measurement needs
explicit start/removal status and damage events through a damage-free tail,
with movement/source death and regeneration accounted for. Damage-fire decoration
lifetimes are not a burn-damage clock. No burn mechanic or terrain interception
was enabled by this tooling.

Synthetic verification: `ctest --test-dir build -R 'engine_unit|projectile_measure' --output-on-failure`. Checks cover paired
launch identity, trace/hash independence, pause, unresolved weapons, deterministic
repeated captures, actual hit coordinates, per-victim samples, private placement
copies and rejection of incomplete/invalid captures. Status: **needs live run**.
## V06 sight/fog consumer run sheet (needs live run)

The synthetic `render_contracts`, `animation_headless` and `engine_unit` checks
exercise invented masks and raw brightness, including the strict 32/33 boundary.
`tests/parity/fog.json` specifies the later aligned visual experiment. It is an
L4 preparation recipe, not an executable parity scenario. V02/V10 tick/identity
observers and validated camera/input routes are still dependencies. No reference
run is authorized inside the Codex sandbox; use these commands on an unrestricted
host. Failed namespace setup remains a failed run, never retry without isolation.

Run from the worktree root, using fresh private destinations:

```sh
mkdir -p work/v06-game/Maps work/v06-game/Saves work/v06-clone
reference_game=/home/adrox/aztec-re/game
for item in Aztec.exe Res Video Music Campaign; do
  if [ -e "$reference_game/$item" ]; then
    ln -s "$reference_game/$item" "work/v06-game/$item"
  fi
done
for item in "$reference_game"/*.plr "$reference_game"/*.ini; do
  if [ -f "$item" ]; then cp "$item" work/v06-game/; fi
done
find "$reference_game/Maps" -maxdepth 1 -iname 'mission_1.*' \
  -exec cp '{}' work/v06-game/Maps/ \;
ZSTAT_GAME_DIR="$PWD/work/v06-game" python3 -I tools/make_zstat.py zfog \
  0:0:1:20:20:move=28,20@120 1:0:1:24:20 1:14:1:28:24
build/aztec_map_viewer --maps work/v06-game/Maps --dump zfog \
  > work/v06-clone/map.txt
# Engine preparation capture only: this does not align a reference tick.
timeout -s KILL 30 unshare -rn env \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  AZTEC_GAME="$PWD/work/v06-game" AZTEC_USER_DIR="$PWD/work/v06-profile" \
  build/aztec_engine --game "$PWD/work/v06-game" --rules-dir "$PWD/docs/data" \
  --size 800x600 --software --no-audio --no-sim \
  --screenshot "$PWD/work/v06-clone/preparation.png" zfog
# The runner creates a further private game copy and configures namespace loopback.
ZSTAT_GAME_DIR="$PWD/work/v06-game" \
ZSTAT_RUNTIME=/home/adrox/aztec-re/build/aztec_recomp RT_UNIT_EVERY=1 \
  sh tools/zstat_run.sh zfog work/v06-reference-preparation 16000
```

Inspect the private map dump before launch: the generator inherits terrain and a
remote hall, and adds an enemy hall for the structure. Their sight, glow and AI
can contaminate the intended control. Lock/move or separate them through validated
inputs before measurement; do not infer a mask radius from the initial spacing.
The scout move at scenario time 120 is a preparation route, not proof of a
reference/engine aligned observation. Record actual positions and clocks.

Repeat each measured day/night, mask-edge, scout-departure/death and viewing-side
case in a fresh private process/profile. Keep simulation frozen at the agreed
observation boundary; align camera, zoom, viewport, selection and cursor. Save the
four raw neighbouring samples, mask ID, object IDs and positions beside captures.
Compare terrain, unit/building and minimap regions separately. Report exact pixel
counts separately from tolerant metrics; absent reference images are unavailable
measurements. Keep every generated map, log and capture under ignored `work/`.
## Original-save tick-zero run sheet

Task 07 follow-up / V16 status: **needs live run**. Live execution is prohibited
in the Codex sandbox; do not remove `unshare -rn` or use the shared installation
as writable state. The commands below are for a namespace-capable external host.
All outputs remain in this worktree's ignored directories.

First build and exercise the synthetic generator and actual clone observer:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build -R 'savegame|save_import|save_resume_controls' --output-on-failure
build/test_save_import --generate build/resume-synthetic.sav build/resume-synthetic.json
```

The generated file is invented test data; do not install or load it in the
original game. `save_resume_controls` supplies invented mission files and empty
resource archives to exercise the standalone clone probe without private data.

The reference prerequisite is still missing: an audited save-load observer at
**load completion before the first resumed simulation update**. Existing
`RT_UNIT_LOG`/`RT_STATE_LOG` command-fetch logs do not establish that boundary,
and their raw pool hashes cannot substitute for normalized fields. The hook
accepts a private executable adapter with this interface:

```text
reference-observer --game PRIVATE_DIR --save ABSOLUTE_SAV --output SNAPSHOT_JSON --runtime ABSOLUTE_RUNTIME
```

The adapter must stage the supplied save in its private profile, load it in the
supplied runtime using `PRIVATE_DIR/Aztec.exe`, synchronize at the audited load
barrier, and emit the schema documented in [savegame.md](savegame.md#resume-follow-up-evidence-and-policy).
Return nonzero if loading or observation fails. It must report actual loaded
state and unsupported domains, preserve original pool IDs (or use an audited
identity map), convert positions to pixel*256, and sample bordered fog using the
same tile convention. It must not advance a tick, use a sleep as the observation
barrier, copy values from the `.sav` decoder instead of observing the runtime,
or claim mappings for unresolved research/script/diplomacy fields. The runner
supplies `save_sha256`; the adapter must omit it. No such audited adapter is
claimed to exist in this checkout. Completing this V02 prerequisite and the
semantic mappings remains work; merely supplying a script path is insufficient.

Select one real save locally. This exact command records the missing observer
status without launching either process (exit 1 is expected):

```sh
python3 tools/savegame/compare_resume.py run \
  --game /home/adrox/aztec-re/game \
  --save /home/adrox/aztec-re/game/Saves/single1.sav \
  --runtime /home/adrox/aztec-re/build/aztec_recomp \
  --clone "$PWD/build/aztec_save_probe" --rules "$PWD/docs/data" \
  --output "$PWD/work/save-resume/missing-observer-01"
```

Adjust only the save filename if it lives under `Saves/<profile>/`. After the
reference observer is implemented and audited, install it privately at
`work/save-resume/reference-observer` and run:

```sh
python3 tools/savegame/compare_resume.py run \
  --game /home/adrox/aztec-re/game \
  --save /home/adrox/aztec-re/game/Saves/single1.sav \
  --runtime /home/adrox/aztec-re/build/aztec_recomp \
  --reference-adapter "$PWD/work/save-resume/reference-observer" \
  --clone "$PWD/build/aztec_save_probe" --rules "$PWD/docs/data" \
  --output "$PWD/work/save-resume/live-01" --timeout 60
python3 tools/savegame/compare_resume.py compare \
  work/save-resume/live-01/reference/snapshot.json \
  work/save-resume/live-01/clone/snapshot.json
```

The runner makes independent reference/clone game copies, symlinks immutable
resources, copies Maps/Saves/profiles/configuration, and copies the input save.
Each process runs with dummy SDL video/audio inside `timeout -s KILL 60 unshare
-rn`; reference loopback/address setup uses shell `-e` and stops on failure.
Inherited `RT_*` settings are removed. Reference video/grab are disabled. Clone
execution needs no interface setup but still uses its own namespace. Input
mutation, stale output directories, wrong ticks/phases/digests, missing
observations and malformed JSON never count as successful comparison. Failure
logs and exact invocations remain beside `result.json`. The current clone will
produce `incomplete` even when supported fields match, because seven requested
domains remain unsupported.

Repeat with a fresh output directory and a save exercising each of movement,
attack targets, queued production, research/upgrade progress, active/disabled
triggers, scalar/sample timers, asymmetric diplomacy and explored fog. Obtain
two reference observations before comparing the clone. Keep private reports and
inputs local; publish only evidence labels and a factual result summary. A
matching supported-field snapshot or retained opaque block is not complete
resume, and tick-zero equality alone is not continuation parity.
