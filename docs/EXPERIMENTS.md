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

## Shared L0 observer run sheet (needs live run)

Run this outside the restricted Codex sandbox, from this worktree. No live
reference result was obtained here. Do not remove `unshare -rn` or run against
shared writable Maps/Saves to bypass a launch failure. The supplied prebuilt
`/home/adrox/aztec-re/build/aztec_recomp` predates this observer: rebuild the
hand-written runtime in this worktree, reading the existing private generated
sources without editing them:

```sh
cmake -S . -B build_l0_reference -DCMAKE_BUILD_TYPE=Release \
  -DAZTEC_GEN_DIR=/home/adrox/aztec-re/work/gen
cmake --build build_l0_reference --target aztec_recomp --parallel 2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build -R 'shared_l0|parity_scenarios' --output-on-failure
```

If the cached generated sources do not match this installation, stop and
regenerate them privately following the normal reference build instructions.
Neither generated sources nor binaries belong in a PR. Launch the one shared
specification twice into fresh directories:

```sh
python3 -I tools/parity_run.py private_l0 --flat-output \
  --game /home/adrox/aztec-re/game \
  --runtime "$PWD/build_l0_reference/aztec_recomp" \
  --engine "$PWD/build/aztec_engine" --timeout 120 \
  --output "$PWD/work/parity/private_l0"
python3 -I tools/parity_run.py private_l0 --flat-output \
  --game /home/adrox/aztec-re/game \
  --runtime "$PWD/build_l0_reference/aztec_recomp" \
  --engine "$PWD/build/aztec_engine" --timeout 120 \
  --output "$PWD/work/parity/private_l0_repeat"
```

These commands intentionally return nonzero for blocked/failed observations;
they do not require a pass to retain diagnostics. Keep existing runs and choose
fresh output paths for further attempts. Each runner creates its own game copy
with immutable asset/executable symlinks, copied Maps/configuration/profile
files and ordinary private Saves. Both subprocesses use dummy SDL video/audio,
a hard timeout and `unshare -rn`; the reference also sets up namespace loopback.
`reference/invocation.json` and `clone/invocation.json` retain exact argv and
RT/SDL controls. The clone user profile is isolated under its run directory.

The shared schedule uses new controls `RT_L0_SCHEDULE=<schedule.txt>` and
`RT_L0_OUTPUT=<observations.jsonl>`, plus `RT_DUMP_DIR=<reference-directory>`.
`RT_CMD_REPLAY` is an empty private file, so navigation cannot inject live
lockstep commands. Existing RT_SCRIPT menu navigation starts the mission; it
does not schedule observations. An optional `--script /absolute/private/start.txt`
replaces only this navigation when a local profile requires different clicks.
The observer itself captures at the declared simulation ticks, not script waits.

Return both private run directories to the orchestrator, especially:

- `scenario.json`, `diff.json`, `runners.json`, `report.json`;
- both canonical JSON bundles and native JSONL streams;
- each runner's schedule, invocation/process logs and tick captures;
- the rebuilt runtime identity and any build/namespace errors.

Expected current verdict after successful launch is
`blocked_observation_contract`: reference orders/queues, fractional coordinates,
actual UI state and complete-render alignment remain unaudited. Captures are
probes held at a simulation boundary, not certified paired L4/L5 frames. The
five-second per-sample rendering wait may fail if the guest renderer needs a
mutex held by the stopped logic thread; retain this result so the complete-frame
hook can be placed at a verified boundary. Reference owner decoding and pool
coverage also need live checks against mission placements. Initial anchor
ambiguity or a spawned object is an explicit experiment failure, not something
to resolve by matching the nearest unit. See the schema and outstanding work in
[PARITY_TEST_PLAN.md](PARITY_TEST_PLAN.md#shared-l0-schema-2-implementation).

Do not update public parity passes from these probes. Update
`docs/parity_matrix.md` with counts only after selecting and reviewing the
returned reports. Never commit the captures, private JSON or game-derived data.
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

## L3 path tracing run sheet (task 68, needs live run)

Task 64's shared scenario runner/observer is absent in this checkout. This task
implements the offline comparator only. Do not run reference processes inside
Codex, bypass `unshare -rn`, or treat historical zpath logs as aligned streams.
The preparation and comparison commands below are executable now; the shared
capture invocation must come from task 64 once its audited adapter is available.
There is no working shared L3 capture command in the current tree.

First preserve the independent 48-configuration baseline and run synthetic checks:

```sh
mkdir -p work/parity/l3-live
build/path_bench --json > work/parity/l3-live/path-before.jsonl
ctest --test-dir build -R 'path|zpath_tools' --output-on-failure
```

Prepare one private map candidate per course/count/fog combination. This example
uses one-tile opposing traffic; change only the selected course/count/fog for
another case, and use a fresh destination. It writes no shared installation data:

```sh
case_dir="$PWD/work/parity/l3-live/swap1-30-explored"
mkdir -p "$case_dir/game/Maps" "$case_dir/game/Saves"
reference_game=/home/adrox/aztec-re/game
for item in Aztec.exe Res Video Music Campaign; do
  if [ -e "$reference_game/$item" ]; then
    ln -s "$reference_game/$item" "$case_dir/game/$item"
  fi
done
for item in "$reference_game"/*.plr "$reference_game"/*.ini; do
  if [ -f "$item" ]; then cp "$item" "$case_dir/game/"; fi
done
ZPATH_GAME_DIR="$reference_game" python3 -I tools/make_zpath.py \
  swap1 30 "$case_dir/game" explored
build/aztec_map_viewer --maps "$case_dir/game/Maps" --dump zpath1 \
  > "$case_dir/map-dump.txt"
```

`game/course.json` describes geometry, initial units, group destinations and raw
packet order frames; `game/cmd.txt` contains reference packets. These are setup
candidates, not a shared semantic scenario. Task 64 must convert setup and orders
for BOTH programs and verify actual initial positions, unit types, ownership,
identity mappings and command readback. Never pass reference packets directly
to the clone or assume pool IDs equal engine IDs. The current harness supports
numbered campaign missions only, so `zpath1` also requires its custom-map adapter.
Use the supplied reference binary `/home/adrox/aztec-re/build/aztec_recomp` only
if it supports the audited observer; otherwise build the hand-written observer
with private generated inputs inside this worktree.

On an unrestricted namespace-capable host, capture one reference recording,
two independent reference replays and two clone runs from the same shared
scenario. Each process needs its own private Maps/Saves/profiles and immutable
resource symlinks, both dummy SDL drivers, a kill timeout and `unshare -rn`.
Reference namespace setup must fail closed at `ip link set lo up` or
`ip addr add 192.168.1.125/24 dev lo`. Retain command readback, before-command
normalized positions **every tick**, order tick, goals, complete horizon and
producer/identity/scale provenance. Keep 1490 ticks after each unit's order,
including the delayed park group's full budget; extend if arrivals are censored.
Validate reference replay repeatability before clone comparison. Namespace,
command, setup, mapping and observation failures are failed experiments.

Required coverage (each ground course at 1/10/30 units, both fog modes):

| Control | Existing preparation | Additional audited setup needed |
| --- | --- | --- |
| Open and obstacle detours | open, wall, clusters, maze, utrap, rooms, diag | Actual terrain/footprint checks |
| Chokepoints and crowds | gap1/2/4, corr1/2, swap/swap1, park, mixed | Opposing orders and delayed budgets |
| Unreachable goal | Synthetic path regression only | Sealed destination with normal orders |
| Disconnected group | Synthetic path regression only | Members on both sides of sealed wall, both selection orders |
| Boats and Johnka | None in zpath generator | Types 84/88, measured water channels/landing destinations |
| Flight | None in zpath generator | Bird/Zeppelin controls, matched distance and obstacle crossing |
| Radius/footprint boundaries | Ground layout proxies only | Small/large units, actual building occupancy and narrow clearance |

Do not label a `ZP_TYPES` substitution as a boat/flight suite: the generator's
legacy type table and ground passability do not establish water or air setup.
For unreachable/disconnected controls retain closest approach, terminal position,
orders and stalls; null arrival is censored even if both programs stop.

After task 64 supplies complete normalized streams, write a private plan with
its **actual audited contract name**, mission identifier (0 for a custom map)
and identity mapping.
This example uses placeholder identities/contract and a tile-(80,30) pixel-centre
goal (each coordinate times 256); replace them with observed values:

```sh
cat > "$case_dir/trace-plan.json" <<'JSON'
{
  "schema": 1,
  "contract": "REPLACE_WITH_AUDITED_CONTRACT",
  "mission": 0,
  "position_scale": 256,
  "units": {
    "group0-unit0": {
      "reference": "REPLACE_REFERENCE_ID",
      "clone": "REPLACE_CLONE_ID",
      "goal": [989184, 374784],
      "order_tick": 62
    }
  },
  "tolerance": {
    "arrival_ticks": 12,
    "arrival_radius_px": 288,
    "path_ratio_max": 1.25,
    "shape_px": 48,
    "idle_px": 0.5,
    "stall_ticks": 120
  }
}
JSON
# Add EVERY intended unit and its own group goal/order tick before comparison.
python3 -I tools/zpath_trace.py "$case_dir/trace-plan.json" \
  "$case_dir/reference.jsonl" "$case_dir/clone.jsonl" \
  --output "$case_dir/trace-comparison.json"
```

The report includes each unit's arrival tick, traveled distance, maximum idle
run, clone/reference path ratio, maximum aligned separation, and first divergent
unit/tick/cause class. Shape is aligned positional deviation, so inspect both
trajectories to separate speed from route choice. Provisional gates and censoring
are documented in [rules_movement.md](rules_movement.md#11-l3-trace-comparison-preparation-2026-11-05-needs-live-run).
A missing/truncated observation, lifecycle change or mismatched contract produces
`invalid` with a nonzero exit code, never a pass. Keep every map, log, plot and
report containing private observations under ignored `work/`.

Only after reference repeatability and a diagnosed paired divergence should a
path behavior change be made. Save before/after `path_bench --json` outputs and
compare all 48 hashes and movement metrics excluding wall-clock timing. Preserve
them unless a documented reference measurement justifies the specific change.
Update L3 paired counts from actual selected diff reports after live capture;
currently **0 scenarios, 0 pass, 0 fail, 0 skip**. No first live divergence or
arrival/path tolerance pass is available. Status: **needs live run**.
## Building and mine placement run sheet (needs live run)

Task 66 depends on **64-shared-runner-observer**, which is still queued and is
absent from this checkout. No live reference placement observations were supplied
or collected for this follow-up. Do not promote the task 47 provisional rules
from deterministic engine tests or code inspection. Live reference launches are
denied in this sandbox; run the following probes on a host with the required
namespace support. Preserve any isolation/launch failure as a failed experiment.

`tests/parity/placement.json` is a candidate screen-input catalogue for ztest_all,
covering every public Building/Defence type once in eight classes and ten variant
families. It is **not** an executable `parity_run.py` scenario. The declared
screen coordinates must be used identically in both programs after measuring the
camera and setup route. They are candidates, not assertions of a clear site or
of the original's snap boundary. Hall/outpost placement routes and rotation may
be unavailable; record that fact rather than inventing input or a rotate key.
Expand the catalogue privately:

```sh
python3 tools/placement_compare.py list
python3 tools/placement_compare.py prepare --output work/parity/placement-plan
```

Prepare two fresh, separate private installations; this command copies only the
campaign template needed by the generator and writable configuration files. It
never changes the read-only installation. Run from this worktree root, once:

```sh
python3 - <<'PY'
from pathlib import Path
import shutil
source = Path('/home/adrox/aztec-re/game')
for label in ('reference', 'clone'):
    target = Path('work/parity/placement-live') / label / 'game'
    target.mkdir(parents=True, exist_ok=False)
    (target / 'Maps').mkdir()
    (target / 'Saves').mkdir()
    for name in ('Aztec.exe', 'Res', 'Video', 'Music', 'Campaign'):
        if (source / name).exists():
            (target / name).symlink_to((source / name).resolve())
    for item in source.iterdir():
        if item.is_file() and item.suffix.lower() in ('.ini', '.plr', '.cfg'):
            shutil.copy2(item, target / item.name)
    for item in (source / 'Maps').iterdir():
        if item.name.lower().startswith('mission_14.'):
            shutil.copy2(item, target / 'Maps' / item.name)
PY
python3 tools/make_testmaps.py --game work/parity/placement-live/reference/game
python3 tools/make_testmaps.py --game work/parity/placement-live/clone/game
```

Keep only the generated ztest_all files in each private Maps directory before
reference custom-map navigation (first row at 258). Retain `.layout.json` for
actual shifted positions. On the live host, prepare and audit separate input
routes at `work/parity/placement-live/reference/input.txt` (RT_SCRIPT grammar,
including player/custom-map navigation) and `clone/input.txt` (PlaytestScript
screen input, starting directly in ztest_all). Select the producer and building
card through the UI; confirm its type and prerequisites, money, owning town,
fog, diplomacy, passive AI and camera before the common screen sweep. Copy fresh
private profiles/maps per repetition and variant. Never reuse modified placement
state for a new distance/deposit boundary control.

These are **unaligned diagnostic probes**, executable once those input files
exist; they cannot emit normalized per-tick reference decisions by themselves:

```sh
test -s work/parity/placement-live/reference/input.txt
timeout -s KILL 90 unshare -rn sh -eu -c '
  ip link set lo up
  ip addr add 192.168.1.125/24 dev lo
  exec env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_RESOLUTION=800x600 \
    RT_SCRIPT="$PWD/work/parity/placement-live/reference/input.txt" \
    RT_DUMP_DIR="$PWD/work/parity/placement-live/reference" \
    RT_CMD_LOG="$PWD/work/parity/placement-live/reference/commands.txt" \
    RT_STATE_LOG="$PWD/work/parity/placement-live/reference/state.txt" \
    /home/adrox/aztec-re/build/aztec_recomp \
    "$PWD/work/parity/placement-live/reference/game/Aztec.exe"
'
test -s work/parity/placement-live/clone/input.txt
timeout -s KILL 90 unshare -rn env \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  AZTEC_USER_DIR="$PWD/work/parity/placement-live/clone/profile" \
  build/aztec_engine --game "$PWD/work/parity/placement-live/clone/game" \
  --size 800x600 --software --no-audio \
  --script "$PWD/work/parity/placement-live/clone/input.txt" \
  --ui-trace "$PWD/work/parity/placement-live/clone/ui.jsonl" \
  --capture-dir "$PWD/work/parity/placement-live/clone/frames" \
  --capture-every 1 ztest_all
```

`--ui-trace` now includes `placement`: snapped `cell`, aggregate `valid`,
`cells` with individual blocking reasons, actual `overlay_drawn`, and
`ghost_sprite: null` because the engine currently draws only the coloured grid.
The existing cursor, tooltip, command and screenshot fields complete the raw
engine diagnostic. Click validation, overlay and trace share the preview query.
This trace is after input/before presentation; a successful click can close the
mode, so inspect the previous preview and submitted command together. Raw frames
and raw tick numbers must **not** be relabelled as aligned reference observations.

After task 64 supplies audited setup/input/clock and observation adapters, emit
`placement-v1` bundles as described in [tests/parity/README.md](../tests/parity/README.md#placement-comparison-contract).
Compare complete per-tick captures using:

```sh
python3 tools/placement_compare.py compare \
  --reference work/parity/placement-live/reference/placement.json \
  --clone work/parity/placement-live/clone/placement.json \
  --output work/parity/placement-live/comparison.json
```

Exit 0 means equality of the declared observations, 1 a measured difference, and
2 invalid/missing/unaligned observations. Missing captures are never zero error.
Measure radius/metric, clearance mask values, deposit channel/threshold and
terrain exceptions before changing engine rules. Sweep own/allied/enemy town
centre distances along axes and diagonals; test blocked cells, reservations,
ground/air occupants and mineral values independently. Capture cancel, invalid
click, repeated placement and any measured rotation input, including feedback,
cursor and ghost. Keep exact overlay pixel differences separate from tolerant
visual diagnostics and from semantic validity. All maps, logs, bundles and
captures remain private under ignored `work/`; commit none of them.
## L1 task 65 run sheet (needs live run)

Task 64 (`64-shared-runner-observer`) is still in the queue in this checkout.
The shared setup/command adapters and audited semantic reference observer are
absent. This task prepares the independent portion only. No reference process
was launched in Codex: namespace creation is denied. No engine rule change is
justified by new measurements, and all 26 rules retain P. Baseline Release build
and CTest passed (43 passes, six skips); all pre-commit hooks passed.

Generate an asset-free handoff, without invoking either executable:

```sh
python3 -I tools/parity_rules.py plan --output work/parity/l1-task65-plan
```

This creates ordered `measurements.json`, a normalization proposal, and each
case's scenario and blank measurement form. It requires no private installation
and applies no inputs. Null reference/clone-before/clone-after values mean
**unmeasured**, never zero or equality. `tests/parity/rules_protocol.json` defines
one primary metric per case and proposed integer units for every observed field.
The existing matrix has only a layer row, no individual case ordering; this run
sheet makes the existing filename/list order explicit. Collect cases in that
order. Triage measured fixes by impact: damage/armor, reload, build time, income,
then the other mechanics. Keep the original clone measurement before each fix.

After task 64 supplies audited adapters, on a namespace-capable host:

1. Make independent private reference-record, reference-repeat and clone copies
   per case/control. Symlink immutable resources/executable from
   `/home/adrox/aztec-re/game`; copy Maps/Saves/settings. Use the runtime
   `/home/adrox/aztec-re/build/aztec_recomp`. Keep all outputs under ignored
   `work/parity/`. Do not reuse mutated profiles between runs.
2. Resolve the scenario's controls and candidate `pending.json`: verify actual
   placements, generated halls, town membership, money, tech/speed, initial HP,
   resource footprints, day phase, visibility, AI and legal command acceptance.
   Task 64 must supply the invocation for its runner; the existing `probe`
   command is only a launch control and cannot execute the abstract inputs.
3. Run dummy SDL video/audio inside `timeout -s KILL N unshare -rn sh -ec`.
   Reference setup must bring loopback up and add `192.168.1.125/24` before exec.
   A denied namespace/network setup is a failed experiment. Never bypass it.
   Record exact executable/configuration identities and command readback privately.
4. Record and replay twice; validate packet readback, complete horizons and
   semantic reference repeatability before clone comparison. Preserve actual
   event ticks, not wall-clock script waits. Use canonical scenario identities,
   stable spawn/projectile identities, and the same before-command boundary.
   Events during tick T first appear in the tick T+1 snapshot; retain native
   launch/impact/service events to distinguish multiple changes within a tick.
   Generic HP deltas cannot establish flight time, launch count or immunity.
5. Resolve every proposed field definition against both audited observers.
   Inapplicable fields need a documented representation on each row kind;
   unsupported fields must block the case, never silently become zero. Positions
   are pixel*256, inputs are tiles. Scope balances/supply to the correct side/town.
   Preserve combined category flags and native armor precision. The proposal
   does not certify the current engine-index or historical raw unit logs.
6. Compare complete normalized streams with the existing command below. It
   exports private numeric observations, not a scalar rule measurement. Derive
   the primary metric using the protocol definition and retain all secondary
   events/controls. Missing events require a longer horizon or a blocker, never
   an assumed timing/rate. For income, regeneration and repair, extend the common
   scenario/stream horizon through at least two relevant clock/service events.
   The base 720 ticks and empty resource map alone cannot measure positive yield.
7. Fill each private measurement form with reference, clone-before, clone-after,
   unit, control variant, uncertainty and evidence label linking the private
   repeatable capture/audit. Add a synthetic regression for every justified
   engine fix. Rerun the same controlled clone after the fix. Publish only the
   reviewed small integer expectation with its specification/evidence label;
   keep raw numeric streams, extracted metadata and captures private. Change P
   to M/V only after the reference measurement and adapter audit exist.

```sh
# Substitute the selected ID and the audited adapter output paths for each case.
python3 -I tools/parity_rules.py compare --id l1_build_barracks \
  --reference work/parity/l1-live/l1_build_barracks/reference.jsonl \
  --repeat work/parity/l1-live/l1_build_barracks/reference-repeat.jsonl \
  --clone work/parity/l1-live/l1_build_barracks/clone-before.jsonl \
  --output work/parity/l1-live/l1_build_barracks/comparison-before
```

Population is not covered by the existing 26 base inputs. Before closing L1,
add a separate population control variant to `l1_queue_two_towns`: record living
side population and per-town accepted queues/spawns before and after crossing
80, 100 and 110 units, with matched single-town, human/AI, sufficient-cash,
prerequisite and free-spawn-space controls. These thresholds are experimental
choices from existing synthetic cap regressions, not reference limits. Distinguish
queue/tactic capacity from a side population restriction. Task 64 needs a
population/command-acceptance observer and controlled repeated queue inputs for
this variant; the base two commands cannot establish any cap or its absence.

The per-case table below is also the handoff status report. N/A means no audited
live value for either implementation. No before/after rule comparison is claimed.

| Order | Scenario | Metric | Reference | Clone before | Clone after | Evidence |
| ---: | --- | --- | --- | --- | --- | --- |
| 1 | `l1_build_barracks` | completion_delay | N/A | N/A | N/A | P |
| 2 | `l1_build_mine` | completion_delay | N/A | N/A | N/A | P |
| 3 | `l1_damage_armored` | hit_damage | N/A | N/A | N/A | P |
| 4 | `l1_damage_bird` | hit_damage | N/A | N/A | N/A | P |
| 5 | `l1_damage_building` | hit_damage | N/A | N/A | N/A | P |
| 6 | `l1_damage_defence` | hit_damage | N/A | N/A | N/A | P |
| 7 | `l1_damage_ground` | hit_damage | N/A | N/A | N/A | P |
| 8 | `l1_damage_zeppelin` | hit_damage | N/A | N/A | N/A | P |
| 9 | `l1_flight_catapult` | flight_duration | N/A | N/A | N/A | P |
| 10 | `l1_income_farm` | harvest_credit | N/A | N/A | N/A | P |
| 11 | `l1_income_mine` | harvest_credit | N/A | N/A | N/A | P |
| 12 | `l1_price_mine` | accepted_debit | N/A | N/A | N/A | P |
| 13 | `l1_queue_serial` | completion_delay | N/A | N/A | N/A | P |
| 14 | `l1_queue_two_towns` | completion_delay | N/A | N/A | N/A | P |
| 15 | `l1_range_hopcannon` | range_boundary | N/A | N/A | N/A | P |
| 16 | `l1_regeneration` | regeneration_delta | N/A | N/A | N/A | P |
| 17 | `l1_regeneration_building` | regeneration_delta | N/A | N/A | N/A | P |
| 18 | `l1_regeneration_organic` | regeneration_delta | N/A | N/A | N/A | P |
| 19 | `l1_reload_cannon` | launch_interval | N/A | N/A | N/A | P |
| 20 | `l1_repair_building` | repair_delta | N/A | N/A | N/A | P |
| 21 | `l1_repair_defence` | repair_delta | N/A | N/A | N/A | P |
| 22 | `l1_sight_cannon` | visibility_boundary | N/A | N/A | N/A | P |
| 23 | `l1_speed_cannon` | position_delta | N/A | N/A | N/A | P |
| 24 | `l1_speed_catapult` | position_delta | N/A | N/A | N/A | P |
| 25 | `l1_speed_hopcannon` | position_delta | N/A | N/A | N/A | P |
| 26 | `l1_train_cannon` | spawn_delay | N/A | N/A | N/A | P |

Status: **needs live run**.
## V19 scripted route run sheet: needs live run

Task 80 supplies route tooling and synthetic regressions while task 71 remains
incomplete. This sandbox denies live reference runs; execute this sheet only on
a host permitting network namespaces. Keep `unshare -rn` and dummy SDL enabled.
The current result is **needs live run**, with 0/15 verified campaign routes.
No live runs were attempted for this iteration.

Run from the worktree root. Create a private route workspace and a baseline
specification for every mission. These empty player-input plans are **pending
specifications**, not solutions or verified routes:

```sh
mkdir -p work/campaign-routes
python3 - <<'PY'
import json
from pathlib import Path

path = Path('work/campaign-routes/scenarios.json')
if path.exists():
    raise SystemExit('Refusing to overwrite existing player route specifications')
rows = [dict(id=f'mission-{m}-win', mission=m,
             route=f'Pending measured player completion route for mission {m}',
             expected_outcome='victory', save_tick=600, steps=[], checkpoints=[])
        for m in range(1, 16)]
path.write_text(json.dumps(rows, indent=2) + '\n')
PY
```

Author normal player steps and named objective checkpoints in that private file
using [the route contract](playthrough.md#scripted-route-contract-2026-11-05-needs-live-run).
Choose save ticks strictly before the measured outcome and after mission start.
Inspect action source indices/kinds using the script audit and `actions.md`.
Empty steps are useful passive controls; they must not be described as winning
player routes without actual completion. Commands beyond move/attack-move need
engine driver work first; transport semantics need task 71. Add separate defeat
specifications where useful; they do not replace the fifteen winning routes.

On the unrestricted host, run paired clone simulations and fresh-process saves:

```sh
campaign_route_stamp=$(date -u +%Y%m%dT%H%M%S)
campaign_clone="$PWD/work/parity/campaign-routes-$campaign_route_stamp"
python3 tools/playthrough.py --game /home/adrox/aztec-re/game \
  --engine "$PWD/build/aztec_engine" --rules-dir "$PWD/docs/data" \
  --routes "$PWD/work/campaign-routes/scenarios.json" --no-auto-orders \
  --max-ticks 43200 --timeout 900 --require-reference --out "$campaign_clone"
campaign_clone_status=$?
printf '%s\n' "$campaign_clone_status" > "$campaign_clone/gate-exit-status.txt"
```

This first strict run intentionally fails without reference timelines, even if
clone completion succeeds. Inspect `summary.json`, `matrix.md`, `actions.md`,
per-run process logs, tick transcripts and the save-continuation result. Isolation
failures are failures; do not rerun without namespaces. A real missing game file
is not a synthetic-test result. The output includes the exact order files,
route fingerprints, executable digest and copied rules for review.

Use the complete reference preparation and record/replay shell commands in
[the campaign transport run sheet](#campaign-transport-and-objective-run-sheet-needs-live-run),
with one **audited normal-input** `work/campaign-routes/mission_N.rt` per mission
matching the authored route. That sheet creates separate private installations
for record and replay and invokes the supplied runtime with dummy SDL inside
`timeout -s KILL 200 unshare -rn`, setting loopback only inside the namespace.
Set its final `wait` and process timeout to cover the route's actual duration;
its default one-minute probe cannot measure a long route. A concrete one-hour
variant after that sheet has prepared `$campaign_reference` is below. It uses
separate `route-record` and `route-replay` installations and preserves each exit
status; it does not reuse the initial probe's writable state.

```sh
export campaign_reference
python3 - <<'PY'
import importlib.util
import os
from pathlib import Path

spec = importlib.util.spec_from_file_location('playthrough', 'tools/playthrough.py')
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)
source = Path('/home/adrox/aztec-re/game')
maps, _ = harness.discover(source)
for mission in range(1, 16):
    directory = Path(os.environ['campaign_reference']) / f'mission_{mission}'
    route = Path('work/campaign-routes') / f'mission_{mission}.rt'
    if not route.is_file():
        raise SystemExit(f'Missing audited normal-input route: {route}')
    for phase in ('route-record', 'route-replay'):
        run = directory / phase
        run.mkdir()
        harness.private_game(source, maps, run / 'game')
    # The earlier preparation has already appended the matching mission_N.rt.
    navigation = (directory / 'input.rt').read_text()
    if not navigation.endswith('wait 60000\nshot\nquit\n'):
        raise SystemExit('Unexpected navigation suffix; review before launching')
    navigation = navigation.removesuffix('wait 60000\nshot\nquit\n')
    (directory / 'route-input.rt').write_text(navigation + 'wait 3600000\nshot\nquit\n')
PY
for mission in $(seq 1 15); do
  mission_probe="$campaign_reference/mission_$mission"
  for phase in route-record route-replay; do
    run_probe="$mission_probe/$phase"
    replay_env=()
    if [ "$phase" = route-replay ]; then
      test -s "$mission_probe/route-record/commands.txt" || break
      replay_env=("RT_CMD_REPLAY=$mission_probe/route-record/commands.txt")
    fi
    (
      cd "$run_probe/game" || exit 1
      timeout -s KILL 3900 unshare -rn env -u RT_CMD_REPLAY \
        SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_RESOLUTION=800x600 \
        RT_SCRIPT="$mission_probe/route-input.rt" RT_DUMP_DIR="$run_probe" \
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

Review replay command readback and reference repeatability before deriving
semantic observations. Screen-coordinate clicks and millisecond waits do not
establish equivalent simulation ticks or object identity. The replay runtime
suppresses live lockstep commands when replay is enabled; retain navigation and
check that replay packets actually cover the route. A general objective/outcome
observer and route translator remain unavailable. These commands collect private
inputs for that audit; they cannot themselves generate certified timelines.

Once an audited observer establishes route equivalence and tick alignment,
write its two actual reference timelines per route to
`work/campaign-routes/reference/ID-1.json` and `ID-2.json`, following the documented
`campaign-checkpoints-v1` format. Use the `route_sha256` from the first clone
summary. Preserve the underlying observer version, runtime digest, readback and
raw records privately. Never populate reference events from clone output or
expected mission script timings. Missing observations keep this step pending.
Then compare in a fresh private output directory:

```sh
python3 tools/playthrough.py --game /home/adrox/aztec-re/game \
  --engine "$PWD/build/aztec_engine" --rules-dir "$PWD/docs/data" \
  --routes "$PWD/work/campaign-routes/scenarios.json" --no-auto-orders \
  --max-ticks 43200 --timeout 900 --require-reference \
  --reference-dir "$PWD/work/campaign-routes/reference" \
  --out "$PWD/work/parity/campaign-paired-$(date -u +%Y%m%dT%H%M%S)"
```

A changed route, save tick, tick budget or policy changes the fingerprint; collect
new reference timelines for that specification. Publish only factual aggregate
counts and English route specifications. Keep saves, command logs, observations,
assets, screenshots and other captured data under ignored `work/`. Update L7
paired counts only after reviewing actual `ID-reference/diff.json` results;
pending routes and synthetic oracle fixtures add no live parity results.
