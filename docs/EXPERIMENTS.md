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
`blocked_observation_contract`: logical reference orders/queues,
actual UI state and complete-render alignment remain unaudited. Captures are
probes held at a simulation boundary, not certified paired L4/L5 frames. The
five-second per-sample rendering wait may fail if the guest renderer needs a
mutex held by the stopped logic thread; retain this result so the complete-frame
hook can be placed at a verified boundary. Pool liveness and class-specific
fractional coordinates now have synthetic checks; their live coverage still
needs checks against mission placements. Initial anchor
ambiguity or a spawned object is an explicit experiment failure, not something
to resolve by matching the nearest unit. See the schema and outstanding work in
[PARITY_TEST_PLAN.md](PARITY_TEST_PLAN.md#shared-l0-schema-2-implementation).

Do not update public parity passes from these probes. Update
`docs/parity_matrix.md` with counts only after selecting and reviewing the
returned reports. Never commit the captures, private JSON or game-derived data.
### L0 queue observer correction (2026-11-06): needs live run

The supplied 2026-11-05 mission-1 failure treated `town+0xf8` as a pointer
and followed `0x48ff84` into the image. Read-only inspection of the existing
private `work/decomp/all.c` and `work/classes.md` establishes an **embedded**
ring at `town+0xf8`. Town constructors `0x4594f0` and `0x4597d0` construct
that ring in place with capacity `0x32`; ring constructor `0x4113c0` sets its
vtable to `0x48ff84`. Relative to the embedded ring, data is `+4`, head is
`+8`, tail is `+0xc`, capacity is `+0x10`, and count is `+0x14`.
The indexed reader `0x411440` and index helper `0x411470` establish logical
head-first access through four-byte object pointers with capacity wraparound.
Admission `0x45cec0`, cancellation `0x45cfd0`, and the save reader in
`tools/savegame/savereader.py` cross-check this interpretation. These are layout
facts, not copied executable code. The earlier production behavior findings
in `engine_rules_status.md` establish queue ownership/capacity but did not
establish the observer's extra pointer indirection.

The corrected probe reads the ring in place. Each failed optional record has
an `unavailable` reason and a `raw` object containing the values used in its
validation. A failed queue has no `queued_handles`; it is not an empty queue.
Raw queue records include the embedded address, vtable, data pointer, head,
tail, capacity, count and any object pointers read. Invalid town-pool metadata
or chains produce one pool-level unavailable record, since town identities
cannot safely be enumerated. Short town records fail individually. Invalid
optional order types/kinds or tactic spans also fail individually. Guest spans
are checked for null, alignment and overflow against the runtime's reserved
4 GiB guest mapping. These checks do not prove arbitrary guest pointers refer
to live allocations. Base object-pool failures still stop observation.

Sample `unavailable` additionally includes `diagnostics.queues` or
`diagnostics.orders` exactly when that raw probe contains a failed record;
the bundle validator checks this agreement. Existing normalized `orders`,
`queues`, `view` and `frame_phase` limitations remain declared. Successfully
reading raw queue handles does not finish normalized production semantics.
Synthetic checks cover a wrapped ring beside a second owner's nonempty ring,
invalid queue bounds/vtable/data/null entries, malformed town pools/records,
and invalid optional order records, preserving both ticks 1 and 3.

**Live acceptance is pending.** No real mission-1 first-tick completion or
owner-0/1 town/queue counts were measured in this sandbox. Task 81's observer
implementation is present, while its live completion remains outstanding.
Rebuild and run this exact first-tick control on the unrestricted host from
this worktree (use fresh output paths if these already exist):

```sh
cmake -S . -B build_l0_reference -DCMAKE_BUILD_TYPE=Release \
  -DAZTEC_GEN_DIR=/home/adrox/aztec-re/work/gen
cmake --build build_l0_reference --target aztec_recomp --parallel 2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
python3 - <<'PYSCENARIO'
import json
from pathlib import Path
scenario = json.loads(Path("tests/parity/private_l0.json").read_text())
scenario["observe"]["ticks"] = [1]
directory = Path("work/parity/l0-queue-scenarios")
directory.mkdir(parents=True, exist_ok=True)
(directory / "private_l0.json").write_text(json.dumps(scenario) + "\n")
PYSCENARIO
for repetition in 1 2; do
  python3 -I tools/parity_run.py private_l0 --flat-output \
    --scenarios-dir "$PWD/work/parity/l0-queue-scenarios" \
    --game /home/adrox/aztec-re/game \
    --runtime "$PWD/build_l0_reference/aztec_recomp" \
    --engine "$PWD/build/aztec_engine" --timeout 120 \
    --output "$PWD/work/parity/l0-queue-fix-$repetition"
  # A blocked contract is expected; retain runners.json and all failure logs.
done
python3 - <<'PYCOUNTS'
import json
from pathlib import Path
for repetition in (1, 2):
    directory = Path(f"work/parity/l0-queue-fix-{repetition}")
    rows = [json.loads(line) for line in
            (directory / "reference/observations.jsonl").read_text().splitlines()]
    samples = [row for row in rows if "tick" in row]
    assert len(samples) == 1 and samples[0]["tick"] == 1, "Incomplete first-tick sample"
    sample = samples[0]
    records = sample["diagnostics"]["queues"]
    print(directory, "tick", sample["tick"], "objects", len(sample["objects"]))
    for owner in (0, 1):
        towns = [r for r in records if r.get("owner") == owner]
        available = [r for r in towns if "unavailable" not in r]
        print("owner", owner, "towns", len(towns), "available_towns", len(available),
              "unavailable_towns", len(towns) - len(available),
              "queue_entries", sum(len(r["queued_handles"]) for r in available))
    failures = [r for r in records if "unavailable" in r]
    print("queue_failures", json.dumps(failures))
    print("unavailable", sample["unavailable"])
    assert not failures, "Queue probe still unavailable; return raw diagnostics"
PYCOUNTS
```

`parity_run.py` creates separate private game directories using read-only asset
symlinks and copied mutable files, launches with dummy SDL video/audio inside
`unshare -rn`, and records argv/environment and process status. Keep isolation
intact. Return `runners.json` to establish normal reference process completion,
not just a JSON line written before a later crash. Return both native streams,
count output, bundles, diagnostics and logs privately. Then rerun the original
1/6/12 schedule twice using the preceding full run sheet. The first-tick control
and synthetic checks do not establish live parity; current status: **needs live run**.

### First live findings follow-up (2026-11-05): needs live run

The orchestrator's first mission-1 run sampled ticks 1, 6 and 12 and reported
63 matching live identities plus 70 suspected inactive entries. This is one
externally supplied run, not a repeated rule measurement. The observer now
checks both allocator chains/counts, enumerates allocated slots, and requires
the world liveness byte at object `+0x5c`. It reports free allocator slots
separately from allocated but inactive world objects. The previous pool bound
was actually the allocator's free-list head. No sentinel HP/owner/position
filter is used. A synthetic 133-slot pool retains exactly 63 live objects of
sides 0/1, including across fractional movement; another control retains a live
object with all three sentinel values. This does not prove the live count yet.

Reference `diagnostics.orders` records allocated handles, type, owner, class,
liveness, tactic, tile and (for Marines) unit state, attack-target handle and
step destination. Tactic state/mode, anchor and route count/cursor are included.
`diagnostics.queues` records each town/hall/owner, production/staged handles,
the three production words at town `+118/+11c/+120`, and the logical
head-to-tail ring contents, including wrapped rings. These are
read-only probes. A tile step is not a logical command destination; a ring
entry alone does not establish remaining/total time, payment, started or paused
semantics. **Normalized orders and production queues remain unfinished.**
The camera and viewport globals are recorded, but selection/panel/cursor/local
view translation and a complete-render barrier also remain unfinished. The
phase probe explicitly says `raw_primary_present`; it does not certify an L4
frame. These limitations continue to block the shared observation contract.

Rebuild with the commands above. On the unrestricted host, repeat the original
schedule into fresh directories, preserving the earlier captures:

```sh
for repetition in 1 2; do
  python3 -I tools/parity_run.py private_l0 --flat-output \
    --game /home/adrox/aztec-re/game \
    --runtime "$PWD/build_l0_reference/aztec_recomp" \
    --engine "$PWD/build/aztec_engine" --timeout 120 \
    --output "$PWD/work/parity/l0-followup-$repetition"
  # Preserve blocked verdicts; process failures remain failures in runners.json.
done
cmp work/parity/l0-followup-1/reference/observations.jsonl \
    work/parity/l0-followup-2/reference/observations.jsonl
cmp work/parity/l0-followup-1/clone/observations.jsonl \
    work/parity/l0-followup-2/clone/observations.jsonl
python3 -I tools/parity_l0.py \
  work/parity/l0-followup-1/reference.json work/parity/l0-followup-1/clone.json \
  --repeat-reference work/parity/l0-followup-2/reference.json \
  --repeat-clone work/parity/l0-followup-2/clone.json \
  --output work/parity/l0-followup-review.json
```

The review intentionally returns nonzero while acceptance is pending. It rejects
unequal repeated bundles or frame bytes as `run_nondeterminism`, and otherwise
exports live/common identity counts, initial HP candidates grouped by type/value,
and moved identities at each sample relative to tick 1. Pool exclusions are
notices, never divergences. HP candidates remain `confirmed: false`: repeatable
incomplete observations are not enough to change an engine rule. Inspect
`diagnostics.pool` to distinguish genuinely free slots from inactive allocated
objects; require the 63 expected live identities, owners 0/1 and their exact
initial anchors before interpreting the HP or movement leads.

Sample more densely to locate the first movement and order changes. Prepare a
private scenario, then run the same paired/repeated experiment:

```sh
mkdir -p work/parity/l0-followup-scenarios
python3 - <<'PY'
import json
from pathlib import Path
s = json.loads(Path('tests/parity/private_l0.json').read_text())
s['observe']['ticks'] = [1, 2, 3, 4, 5, 6, 8, 12, 18, 24, 36, 60]
Path('work/parity/l0-followup-scenarios/private_l0.json').write_text(json.dumps(s, indent=2) + '\n')
PY
for repetition in 1 2; do
  python3 -I tools/parity_run.py private_l0 --flat-output \
    --scenarios-dir "$PWD/work/parity/l0-followup-scenarios" \
    --game /home/adrox/aztec-re/game \
    --runtime "$PWD/build_l0_reference/aztec_recomp" \
    --engine "$PWD/build/aztec_engine" --timeout 180 \
    --output "$PWD/work/parity/l0-dense-$repetition"
done
cmp work/parity/l0-dense-1/reference/observations.jsonl \
    work/parity/l0-dense-2/reference/observations.jsonl
cmp work/parity/l0-dense-1/clone/observations.jsonl \
    work/parity/l0-dense-2/clone/observations.jsonl
python3 -I tools/parity_l0.py \
  work/parity/l0-dense-1/reference.json work/parity/l0-dense-1/clone.json \
  --repeat-reference work/parity/l0-dense-2/reference.json \
  --repeat-clone work/parity/l0-dense-2/clone.json \
  --output work/parity/l0-dense-review.json
```

Read Marine unit states with positions and tactic/route changes: rest 1/5,
march 2, attack 3, dying 4. Compare the snapshots *before* work for tick n;
tick 6 reflects five completed updates. An initial script or AI tactic may
move without any human packet. Do not label such movement a missing engine
movement rule, or equate a physical rest state with a logical idle command.
Use the shared schedule unchanged between both processes and repetitions.

Before confirming type 32 (13000 versus 14000, six objects) or type 6
(12000 versus 10000, three objects), retain the actual difficulty/profile and
mission settings from both private game copies, initial mission HP overrides,
rules identity and scenario identity beside the review. Repeat each separately
controlled difficulty/map-setting variant twice in fresh private copies and
fresh output directories. The current shared schema has no audited difficulty
input adapter; equal requested mission IDs do not prove equal difficulty.
Do not edit the shared installation, invent a difficulty switch, or change the
engine HP tables from these leads. No engine HP or movement rule was changed
in this follow-up. All live conclusions, full semantic adapters and measured
L1/L4/L5/L8 acceptance remain **needs live run**.

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
## V05/V12 vessel run sheet (task 86): needs live run

Task 86 adds **S** comparison controls, **P** live recipes and **no M** transport
measurements. The sandbox reference prohibition applies here too. Task 71's
diagnostics are present; V02 aligned identity/dispatch/cargo observations and
V05/V10 water movement/command contracts remain incomplete. Engine behaviour is
unchanged until an original measurement repeated in two runs confirms a
divergence. All 19 sites remain open, including those dormant in engine runs.

List the complete per-site checklist without reading private data:

```sh
python3 -I tools/transport_measure.py plan
python3 -I tests/test_transport_measure.py
```

The previous record/replay pair checks replay, not independent repeatability.
Use the following **two independent recordings per mission** on the orchestrator
host. Supply normal-input routes at `work/vessel-routes/mission_N.rt` for missions
2/7/9/15 first; each starts after briefing entry. Missing routes deliberately
stop preparation. Do not substitute passive timeouts, cheats or memory writes.
Navigation uses the historical 800x600 coordinates; retain screenshots proving
entry, since locked missions and slow menu transitions can invalidate a run.
These commands collect evidence; they cannot by themselves identify cargo or
script dispatch. Keep runtime version, input routes, captures and all modified
test maps private.

```sh
vessel_probe=$(mktemp -d "$PWD/work/vessel-reference.XXXXXX")
export vessel_probe
python3 - <<'PY'
import importlib.util
import os
from pathlib import Path

spec = importlib.util.spec_from_file_location('playthrough', 'tools/playthrough.py')
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)
source = Path('/home/adrox/aztec-re/game')
maps, available = harness.discover(source)
output = Path(os.environ['vessel_probe'])
routes = {m: Path(f'work/vessel-routes/mission_{m}.rt').read_text()
          for m in (2, 7, 9, 15)}
for mission, route in routes.items():
    assert mission in available and route.strip(), f'missing route: {mission}'
    commands = ['wait 6000', 'click 140 239', 'wait 800', 'click 160 543', 'wait 5000']
    if mission >= 10:
        commands += ['click 645 421', 'wait 800']
    row = mission - 1 if mission < 10 else mission - 10
    commands += [f'click 150 {257 + 20 * row}', 'wait 800', 'shot',
                 'click 140 544', 'wait 3000', 'click 700 430', 'wait 7000', 'shot']
    commands += route.splitlines() + ['shot', 'quit']
    for repetition in (1, 2):
        run = output / f'mission_{mission}' / f'run_{repetition}'
        run.mkdir(parents=True)
        harness.private_game(source, maps, run / 'game')
        (run / 'input.rt').write_text('\n'.join(commands) + '\n')
PY
sha256sum /home/adrox/aztec-re/build/aztec_recomp > "$vessel_probe/runtime.sha256"
python3 -I tools/transport_measure.py plan > "$vessel_probe/plan.json"
for mission in 2 7 9 15; do
  for repetition in 1 2; do
    vessel_run="$vessel_probe/mission_$mission/run_$repetition"
    test -s "$vessel_run/input.rt" || break
    sha256sum "$vessel_run/input.rt" > "$vessel_run/input.sha256"
    (
      cd "$vessel_run/game" || exit 1
      timeout -s KILL 900 unshare -rn env -u RT_CMD_REPLAY \
        SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_RESOLUTION=800x600 \
        RT_SCRIPT="$vessel_run/input.rt" RT_DUMP_DIR="$vessel_run" \
        RT_UNIT_LOG="$vessel_run/units.log" RT_UNIT_EVERY=1 \
        RT_STATE_LOG="$vessel_run/state.txt" RT_CMD_LOG="$vessel_run/commands.txt" \
        sh -ec '
          ip link set lo up
          ip addr add 192.168.1.125/24 dev lo
          exec /home/adrox/aztec-re/build/aztec_recomp "$1/Aztec.exe"
        ' sh "$vessel_run/game"
    ) > "$vessel_run/stdout.log" 2> "$vessel_run/stderr.log"
    printf '%s\n' "$?" > "$vessel_run/exit-status.txt"
  done
done
```

Reject nonzero exits, empty logs, wrong-menu captures and unequal mission entry
state before normalizing anything. Record/replay remains a separate control as
in the task-71 sheet. Do not use replay as the second independent recording.
The supplied unit log reads a generic object pool and raw offsets: it does not
provide audited vessel/cargo membership or transport-script start/finish events.
Do not interpret missing rows as unloading or death. A reference observer adapter
must establish those identities and align simulation ticks before these classes
can receive measurements. No transport observer adapter or automatic raw-log
normalizer is supplied by task 86; this is an explicit dependency, not a working
live measurement pipeline.

Run matched isolated controls on private copies of the maps, twice per control.
Copy any map or script symlink to an ordinary private file before editing it.
Retain hashes and the exact data changes, and keep acceptance scripts intact.
Use this matrix for Boat, Submarine and Johnka separately where applicable:

| Class | Required observations and controls |
| --- | --- |
| activation | Vessel source/section/scenario/side identity, initial Up/Enabled, dispatch/enable tick, first phase/motion; inactive, delayed and repeated activation controls |
| boarding | Candidate eligibility, stable identities, positions, arrival times and membership at each tick; permute source order and distance, test below/at/above observed capacity, simultaneous candidates, foreign side and empty load |
| unloading | Ordered membership transitions and pixel landing coordinates; empty/occupied landing, more passengers than free landing tiles and moving blockers |
| route | Every position/waypoint/phase from start to finish; symmetric alternative water channels, blocked channel and off-map approach |
| arrival | First arrival, opening, boarding, departure and enabled/idle transitions on the same simulation clock; vary travel distance separately from animation length |
| speed | Pixel displacement per simulation tick in straight unobstructed water; both directions and chosen game speeds; never use render frames as ticks |
| turning | Body orientation and position through both turn directions, reversal and repeated activation while turning |
| shore | Pixel position and phase at each shoreline boundary; concave shore, narrow channel, occupied landing and off-map start |
| stuck | Position/order/phase through a fixed blocked interval, then after removing the blocker; unreachable destination and persistent blocker controls |
| completion | Script start and finish, vessel enabled/idle state, final passenger identities/ownership, repeat dispatch and objective condition transitions |

Normalize observations into separate private JSON files, one case/site/run per
file. Schema 1 requires `source` (`reference` or `engine`), distinct `run_id`,
`case`, a `site` ID from `plan`, `binary_sha256`, `input_sha256`, `observer`,
`alignment`, `artifacts` and `observations`. Hash strings use lowercase SHA-256.
`input_sha256` hashes a shared canonical case specification (map/script/route
hashes and game speed); it must describe equivalent engine/reference inputs.
`observer` identifies the audited observer/normalization revision; both reference
runs must use the same revision. `alignment` documents the common tick origin,
position units and stable identity mapping. Never align each event independently
or discard a delay. Reference runtime hashes must agree across repetitions.

Each artifact is `{"path":"units.log","sha256":"<actual hash>"}`, relative to
the JSON file or absolute; the tool checks actual nonempty bytes. Include every
capture used to derive samples, not merely a token log. Repetitions must use
distinct capture files and run IDs. Provenance validation cannot prove that an
operator ran independent processes or interpreted raw fields correctly.
Each observation class contains a nonempty ordered list of
`{"tick":12,"value":{"cargo":["passenger-A","passenger-B"]}}` samples.
This is an invented schema example, not a capacity observation. Use explicit
fields/units from the matrix above; retain passenger/position list order. Omit
unmeasured classes. Do not fill unknown values with guessed defaults. A dormant
site is unmeasured; an empty list never proves original nonexecution.

After the observer and normalizer exist, compare a site/case using exact commands
such as these (the private JSON filenames below are the normalizer's outputs):

```sh
python3 -I tools/transport_measure.py compare \
  --reference "$vessel_probe/mission_2/run_1/boat-case.json" \
  --repeat "$vessel_probe/mission_2/run_2/boat-case.json" \
  --engine "$vessel_probe/mission_2/engine/boat-case.json" \
  > "$vessel_probe/mission_2/boat-comparison.json"
# For one isolated behaviour control, add e.g. --class boarding.
```

Exit 0 means only that every requested class has identical recorded samples;
exit 1 retains missing/nonrepeatable evidence or confirmed sample divergence;
exit 2 rejects malformed provenance/captures. Reference-only comparison stays
`needs live run` even if samples repeat, because no engine comparison exists.
Default comparison requires all ten classes; a selected-class pass does not close
the other classes. The comparator checks supplied JSON values, not semantic
completeness, observer correctness, successful campaign routes or nonexecution.
Only an audited two-run divergence permits an engine behaviour correction.
Then add synthetic engine regressions using independent fixtures for the measured
class, repeat build/CTest/pre-commit, and run all 19 original sites with the
objective/replay/save controls. Do not promote V05/V12 on these tool tests alone.

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

## V14 campaign UI run sheet (task 77, needs live run)

Run these commands from this worktree root on a namespace-capable host, never
inside the restricted Codex sandbox. All captures, profiles and traces remain
private. Do not bypass `unshare -rn` after a namespace failure.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build -R 'campaign_ui|ui_capture|frontend_capture_controls' --output-on-failure
python3 -I tools/ui_capture.py frontend \
  --scenario tests/parity/ui/campaign-ui.json \
  --game /home/adrox/aztec-re/game \
  --runtime /home/adrox/aztec-re/build/aztec_recomp \
  --clone "$PWD/build/aztec_engine" \
  --output "$PWD/work/parity/ui/campaign77-live-01" --timeout 60
```

The runner creates separate private game copies for the reference and both
clone repetitions, symlinks immutable assets, copies mutable installation files,
sets dummy SDL video/audio and executes through `timeout -s KILL` and
`unshare -rn`. Clone captures now have separate `AZTEC_USER_DIR` profiles and
clear inherited front-end automation variables. Reference loopback/address setup
fails closed. Keep the nonzero exit status: this front-end runner deliberately
reports exploratory, unaligned evidence, even if every capture succeeds.
The scenario retains the previously documented reference startup/campaign/
briefing probe. Its labels are requested capture points, not observed screen IDs.
The extended clone route has 44 frames; it does not pretend to be an equivalent
44-frame reference route.

Before sequence comparison, finish/audit the task-64 reference UI adapter and
measure the original menu graph through normal inputs. It must emit one actual
screen observation per presentation, with normalized IDs from the documented
`reference-ui-raw-v1` contract in ui_screens.md. No environment switch in this
checkout currently supplies that stream. Do not rename a clone trace or fill it
from the scenario's expected labels. Extend the reference input route to walk the
same states and record its declared horizon before launch. In particular, the
current reference startup probe is insufficient for the extended clone route.

After that adapter produces `reference/screens.jsonl`, compare a complete route
as follows (44 is the candidate horizon only when the audited reference route
has explicitly been scheduled for 44 presentations):

```sh
python3 -I tools/ui_sequence.py \
  --reference work/parity/ui/campaign77-live-01/reference/screens.jsonl \
  --clone work/parity/ui/campaign77-live-01/clone-1/trace.jsonl \
  --reference-frames 44 --clone-frames 44 \
  --output work/parity/ui/campaign77-live-01/sequence
```

With the current missing observer this produces status `needs live run` and exits
1. Screen-order equality alone is not L4/L5 acceptance. Repeat with fresh output
folders and independently captured reference traces; require both reference
repeats to agree. Capture normal winning and losing command routes for each of
15 missions, then cover objective review, defeat/retry, return, next briefing,
final ending, profile reopen, locked/unlocked selection, empty/occupied/corrupt
slots, overwrite/load confirmation/cancel and settings after process restart.
Use a fresh private profile for each initial-state control. Original new-game,
difficulty, player selection, campaign-map, options categories, credits, quit
confirmation and multiplayer graph routes still need observation and adapters.
Do not force a simulation outcome as evidence of a normally reachable mission
completion. No live result, unlock policy, graph completeness or pixel score is
claimed by the synthetic regressions.
## L6 live audio run sheet (task 76, needs live run)

**Evidence: synthetic only; status: needs live run.** Do not execute this sheet
inside the restricted Codex sandbox. Use a host that permits `unshare -rn` and
namespace interface/socket setup. No live attempt was made for this follow-up.
Task 64's shared fixed-tick observer/input translator is missing here; the smoke
steps below are available independently, while paired capture acceptance remains
blocked on it. All outputs belong under ignored `work/parity/`. Record event
metadata only: never enable `RT_AUDIO_DUMP`, screenshots or sample capture.

First verify the independent build and synthetic regressions:

```sh
sh tools/bootstrap.sh --no-hooks
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
```

For a startup reference hook/isolation smoke, use a fresh destination:

```sh
python3 -I tools/audio_parity.py probe \
  --game /home/adrox/aztec-re/game \
  --runtime /home/adrox/aztec-re/build/aztec_recomp \
  --output "$PWD/work/parity/l6-startup-01" --timeout 60
cat work/parity/l6-startup-01/report.json
```

The probe copies writable Maps/Saves/profiles/configuration, symlinks immutable
resources, clears inherited `RT_*` variables, and runs with both dummy SDL
drivers through `timeout -s KILL 60 unshare -rn sh -ec`, configuring loopback and
192.168.1.125 inside the namespace. Failure stops before launch. It requests
`RT_AUDIO_LOG`, `RT_CMD_LOG` and `RT_STATE_LOG`; the default script waits 3000 ms
and quits. Exit 1 is expected even after raw capture (`captured_unmapped`); exit
77 means missing private inputs. Missing/empty audio logs are blocked, never a
pass. Check `invocation.json` and `process.log`. An old runtime without the audio
hook requires a private rebuild from hand-written runtime sources plus the
user's local lift; never modify the supplied binary or publish derived outputs.
A startup run is not a mission, event-matrix or record/replay acceptance run.

A clone-only dispatcher smoke uses its own private copy and a tick-driven script.
Run from the worktree root on the same namespace-capable host:

```sh
python3 - <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, str(Path('tools').resolve()))
from parity_report import execute, private_game
root = Path('work/parity/l6-clone-smoke-01').resolve()
root.mkdir(parents=True, exist_ok=False)
private_game(Path('/home/adrox/aztec-re/game'), root / 'game')
script = root / 'input.txt'
script.write_text('run 120\nquit\n')
execute([Path('build/aztec_engine').resolve(), '--game', root / 'game', '1',
         '--software', '--no-mouse-grab', '--no-edge-scroll',
         '--script', script, '--audio-log', root / 'clone.jsonl'],
        root, 60, setup_network=False)
PY
```

This uses dummy SDL inside its own namespace; `execute` also clears inherited
`RT_*` variables, including sample-dump settings. It runs the graphical
application with a dummy video driver: `--headless` currently omits the audio
dispatcher. Do not pass `--no-audio`, which disables captions and event requests.
This smoke is not aligned to the reference startup run.

After task 64 is available, have its driver/auditor prepare private scripts at
`work/parity/l6-input/reference-record.txt`, `reference-replay.txt` and
`clone.txt` for one common scenario/horizon/camera/UI state. The replay script
must navigate into the same mission without reissuing the recorded in-mission
orders. Reference scripts use milliseconds/DIK keys; clone scripts use
frames/ticks/SDL keys. Reusing one script verbatim is not a shared clock contract.
The scenario must exercise selection, orders, combat, construction (`building`),
production announcements (`production`), UI, ambient loops and music. Then run:

```sh
python3 -I tools/audio_parity.py probe \
  --game /home/adrox/aztec-re/game \
  --runtime /home/adrox/aztec-re/build/aztec_recomp \
  --script "$PWD/work/parity/l6-input/reference-record.txt" \
  --output "$PWD/work/parity/l6-record-01" --timeout 120
python3 -I tools/audio_parity.py probe \
  --game /home/adrox/aztec-re/game \
  --runtime /home/adrox/aztec-re/build/aztec_recomp \
  --script "$PWD/work/parity/l6-input/reference-replay.txt" \
  --replay "$PWD/work/parity/l6-record-01/commands.txt" \
  --output "$PWD/work/parity/l6-replay-01" --timeout 120
```

Inspect both reports even though both commands return 1 for unmapped captures.
Require complete nonempty mission state logs over the declared horizon and
audit injected packet readback against the recording, excluding NOTHING packets.
Use the existing `parity_report.verify_packet_readback`, `state_rows` and
`pool_differences` helpers after task 64 establishes the exact horizon. Preserve
transient reference state differences as failures; matching clone hashes cannot
replace this audit. Each probe creates a separate private installation copy.

For the paired clone capture, rerun the clone Python block above with a fresh
`root` of `work/parity/l6-clone-01` and replace the two script lines with:

```python
script = Path('work/parity/l6-input/clone.txt').resolve()
```

Keep mission number, viewport/camera, profile/options, action sequence and tick
horizon consistent with the shared scenario. The startup/smoke scripts are not
substitutes for these audited inputs. For each raw capture, prepare a private
SHA-256-bound manifest as specified in [parity.md](parity.md#private-capture-and-normalization).
Use the same scenario/clock/identity header on both sides and an explicitly
audited tick offset. Reference annotations must resolve every buffer request to
an evidenced sound ID, kind and trigger. Never take IDs or ordering from clone
expectations. Streamed music transitions, speech/caption timing and events absent
from the DirectSound shim need an audited reference observer; do not fabricate
extra events or assert completeness while those observations are missing.

After those audits, normalize and compare:

```sh
python3 -I tools/audio_parity.py normalize reference \
  work/parity/l6-record-01/reference.jsonl \
  work/parity/l6-record-01/reference-manifest.json \
  --output work/parity/l6-record-01/reference-normalized.json
python3 -I tools/audio_parity.py normalize clone \
  work/parity/l6-clone-01/clone.jsonl \
  work/parity/l6-clone-01/clone-manifest.json \
  --output work/parity/l6-clone-01/clone-normalized.json
python3 -I tools/audio_parity.py compare \
  work/parity/l6-record-01/reference-normalized.json \
  work/parity/l6-clone-01/clone-normalized.json \
  --require-all-classes --output work/parity/l6-record-01/diff.json
```

Normalize replay with its own digest/annotations and compare it to the recording
before accepting clone differences. Compare preserves the global sequence and
reports the first differing event; timing tolerance is one tick. The strict
coverage gate requires events in all eight classes on both sides. It cannot
establish per-mission or settings coverage by itself. Repeat in fresh directories
for each mission and independently varied effects/music/speech gains, mute,
acknowledgements and captions; test positional loops entering/leaving visibility,
range and object lifetime, plus music transitions through the allowed playlist.
Retain event logs and failure reports privately. Publish only evidence labels
and factual counts, with unresolved mappings still marked **needs live run**.
## L8 AI run sheet (needs live run)

Task 75 / V11, 2026-11-05: **S** clone semantic observer and synthetic distribution
controls; **P** passive/provocation recipes. **Needs live run.** No reference run
was attempted: this sandbox denies the required network namespaces. Dependency
`64-shared-runner-observer` is still in the queue and its audited reference
production/town/tactic/input mapping is absent. No M evidence or planner policy
correction follows from these controls. The existing twenty-minute skirmish and
replay assertions remain required.

Validation in this worktree: Release build passed; 44 CTest entries passed and
six skipped (five private-data entries and restricted UDP process creation).
Pre-commit passed for tracked files and all new files. The eight `ai_measure`
controls include actual headless synthetic traces, repeated byte identity and
traced/untraced tick-hash equality. These results contain no reference evidence.

First verify the independent controls (no original installation needed):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build -R '^(ai_unit|ai_skirmish|parity_ai|ai_measure)$' --output-on-failure
```

The following command works now and writes a blocked report, returning 77. Use a
fresh output name for each invocation; missing data must never become a pass:

```sh
python3 -I tools/ai_measure.py --scenario tests/parity/ai_passive.json \
  --reference work/parity/l8/reference-1.jsonl work/parity/l8/reference-2.jsonl \
  --clone work/parity/l8/clone-1.jsonl work/parity/l8/clone-2.jsonl \
  --output work/parity/l8-missing-reference-01
```

On an unrestricted host, finish task 64 before using the remaining commands.
Review the proposed private adapter interface below when implementing it; the
executable `work/parity/l8/reference-adapter` **does not exist in this checkout**.
It must launch the selected runtime, install audited normalized AI observers,
realize the scenario input at exact simulation ticks, and write complete
`audited-ai-v1` JSONL. Do not replace it with raw unit-log conversion. See
[the event contracts](../tests/parity/README.md#l8-repeated-distributions-and-clone-decision-observer).

Prepare each recipe independently. Place its fully audited fixtures under
`work/parity/l8/setup/<case>/Maps/` (all map/resource sidecars and script), and
`clone-orders.txt` beside that Maps directory. The order file may be empty for a
passive run. Script/input setup must produce the declared attacks, force loss,
harvest suspension and reinforcements on both sides; the present clone CLI order
file alone cannot express all those actions. Retain action acknowledgements and
before/after state controls. Confirm zero inherited campaign tactics, matching
seed, cash, technology, enabled IDs, daylight/fog and town identity mapping. Freeze
a private `scenario.json` copy with `reference_audit` set to the reviewed audit ID;
keep evidence P until actual observations justify M. Candidate values and setups
are deliberately provisional; do not silently change them between repeats.

After those prerequisites are satisfied, these are the exact paired commands.
Run from the worktree root. `case_name` selects either `ai_passive` or
`ai_provocation`. Each process gets ordinary private Maps/Saves and copied
settings; only read-only assets are symlinked. Never pass the shared installation
as the runtime's writable game directory. Use a new case root for another batch.

```sh
case_name=ai_passive
source_game=/home/adrox/aztec-re/game
reference_runtime=/home/adrox/aztec-re/build/aztec_recomp
adapter="$PWD/work/parity/l8/reference-adapter"
setup="$PWD/work/parity/l8/setup/$case_name"
batch="$PWD/work/parity/l8/$case_name"
engine="$PWD/build/aztec_engine"
rules="$PWD/docs/data"
test -x "$adapter" && test -f "$setup/scenario.json" || exit 1
mkdir "$batch" || exit 1
for repeat in 1 2 3 4 5; do
  for origin in reference clone; do
    run="$batch/$origin-$repeat"
    mkdir -p "$run/game/Maps" "$run/game/Saves" "$run/user" || exit 1
    for asset in Aztec.exe Res Video Music Campaign; do
      test ! -e "$source_game/$asset" || ln -s "$source_game/$asset" "$run/game/$asset" || exit 1
    done
    for config in "$source_game"/*.ini "$source_game"/*.plr "$source_game"/*.cfg; do
      test ! -f "$config" || cp "$config" "$run/game/" || exit 1
    done
    cp -R "$setup/Maps/." "$run/game/Maps/" || exit 1
    if test "$origin" = reference; then
      timeout -s KILL 300 unshare -rn sh -eu -c '
        ip link set lo up
        ip addr add 192.168.1.125/24 dev lo
        cd "$1/game"
        exec env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
          RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_RESOLUTION=800x600 \
          "$2" --game "$1/game" --runtime "$3" --scenario "$4/scenario.json" \
          --output "$1/events.jsonl"
      ' sh "$run" "$adapter" "$reference_runtime" "$setup" >"$run/stdout.log" 2>"$run/stderr.log" || exit 1
    else
      map_name=zai_passive
      test "$case_name" != ai_provocation || map_name=zai_provocation
      timeout -s KILL 300 unshare -rn env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        AZTEC_USER_DIR="$run/user" "$engine" --headless --game "$run/game" \
        --rules-dir "$rules" --no-auto-orders --orders "$setup/clone-orders.txt" \
        --max-ticks 14400 --ai-trace "$run/events.jsonl" \
        --tick-hashes "$run/hashes.txt" --report "$run/report.json" "$map_name" \
        >"$run/stdout.log" 2>"$run/stderr.log" || exit 1
    fi
  done
done
python3 -I tools/ai_measure.py --scenario "$setup/scenario.json" \
  --reference "$batch"/reference-*/events.jsonl --clone "$batch"/clone-*/events.jsonl \
  --output "$batch/comparison"
```

Repeat the entire block with `case_name=ai_provocation` after auditing that setup.
Never bypass failed loopback/socket setup. Preserve nonzero exits, early horizons,
missing reference classes and empty clone classes as failures/blockers. Run
reference record/replay controls before interpreting these measurements. Five
repeats per side are an initial protocol, not statistical proof; retain per-run
counts, tick windows, whole-force/type/site frequencies and raw provenance. The
full recipe includes expansion/retreat, which the clone currently cannot issue;
once observed on the reference they must fail rather than be removed to get green.
Use overlapping windows to bracket policy changes, then add synthetic regressions
for each measured boundary and rerun all existing AI soak/replay checks. Only
measured results can justify replacing cheapest-unit, eight-tile placement,
force-count launch and average-income fallback policies or changing V11 to M.
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
## Construction flow run sheet (needs live run)

Task 67 status: **needs live run** (2026-11-06). Task 66's partial placement
implementation is present, but its reference acceptance is open. Task 64 has no
audited construction command, creation-identity, worker or stage adapter in this
checkout. Live reference execution is denied in this sandbox; no new reference
process was launched and no construction measurements were obtained. The
supplied runtime and local derived source are not evidence of observed timing.

**S:** `engine_unit` adds economy/military/defence controls for paused work,
retained damage through resume, replay continuation at every remaining tick,
completion with no second payment, retained footprint and no stale refund.
Existing mine tests cover all races and serial-placeholder tests cover waiting
sites, payment and cancellation. `construction_compare` tests strict complete
capture validation, semantic/pixel differences and reference instability.
**P:** `tests/parity/construction.json` lists every Building/Defence type and
eight construction-flow variants. These are proposed input recipes, not measured
routes. Placeholder sprite, worker assignment/travel, phase timing, cost/refund
schedule and completion sound/selection/rally remain unmeasured. The engine uses
its existing town timer; no worker rule or new duration was guessed, and optional
parallel construction remains off. No behavior fix can be justified by these
synthetic checks alone.

On a host allowing the required isolation, prepare a fresh private game for each
type/variant/repetition. This example prepares one case; choose a new output
directory for every case. Run from the worktree root:

```sh
python3 - <<'PY'
from pathlib import Path
import shutil

source = Path('/home/adrox/aztec-re/game')
root = Path('work/parity/construction-live')
root.mkdir(parents=True, exist_ok=False)
for label in ('reference', 'repeat', 'clone'):
    game = root / label / 'game'
    (game / 'Maps').mkdir(parents=True)
    (game / 'Saves').mkdir()
    for name in ('Aztec.exe', 'Res', 'Video', 'Music', 'Campaign'):
        if (source / name).exists():
            (game / name).symlink_to(source / name)
    for item in source.iterdir():
        if item.is_file() and item.suffix.lower() in ('.ini', '.plr'):
            shutil.copy2(item, game / item.name)
    for item in (source / 'Maps').iterdir():
        if item.is_file() and item.stem.lower() == 'mission_14':
            shutil.copy2(item, game / 'Maps' / item.name)
PY
for label in reference repeat clone; do
  python3 tools/make_testmaps.py --game "work/parity/construction-live/$label/game"
done
```

The generator preserves private terrain and writes a layout manifest. Resolve
the case's owning town, legal site and card route from that manifest. Showcase
structures start completed, so remove the matching preplaced type from the
controlled town in all copies (or establish another matched town); otherwise
uniqueness checks can reject the construction. Record those private map changes
and their digests. Keep town level, prerequisites, speed, cash, visibility,
resource values, AI, camera and initial selection identical. Establish actual
outpost/upgrade UI reachability before attempting a placement route. A worker
interruption case must first establish whether the worker is an independent
object, an overlay or another mechanism; do not invent an engine worker type.

Author separately audited routes as absolute paths
`work/parity/construction-live/{reference,repeat,clone}/input.txt`.
Reference `RT_SCRIPT` uses milliseconds, while clone `--script` waits count
frames. These scripts can collect launch/UI diagnostics only. Task 64 must
supply equivalent simulation-tick commands and observers before the output can
be converted to `construction-v1`; do not relabel raw frames or hashes as aligned
state. The supplied prebuilt runtime may lack newer observer hooks. Rebuild it
privately if needed; keep generated source out of the repository.

After routes exist, the reference diagnostic launch is:

```sh
construction_probe="$PWD/work/parity/construction-live"
for label in reference repeat; do
  test -s "$construction_probe/$label/input.txt" || exit 1
  (
    cd "$construction_probe/$label/game" || exit 1
    timeout -s KILL 180 unshare -rn env \
      SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
      RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_RESOLUTION=800x600 \
      RT_SCRIPT="$construction_probe/$label/input.txt" \
      RT_DUMP_DIR="$construction_probe/$label" \
      RT_CMD_LOG="$construction_probe/$label/commands.txt" \
      RT_STATE_LOG="$construction_probe/$label/state.txt" \
      RT_UNIT_LOG="$construction_probe/$label/units.txt" RT_UNIT_EVERY=1 \
      sh -ec '
        ip link set lo up
        ip addr add 192.168.1.125/24 dev lo
        exec /home/adrox/aztec-re/build/aztec_recomp "$1/Aztec.exe"
      ' sh "$construction_probe/$label/game"
  ) > "$construction_probe/$label/stdout.log" 2> "$construction_probe/$label/stderr.log" || exit 1
done
test -s "$construction_probe/clone/input.txt" || exit 1
timeout -s KILL 180 unshare -rn env \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  AZTEC_USER_DIR="$construction_probe/clone/profile" \
  build/aztec_engine --game "$construction_probe/clone/game" \
  --mod parallel_construction=off --size 800x600 --software --no-audio \
  --script "$construction_probe/clone/input.txt" \
  --ui-trace "$construction_probe/clone/ui.jsonl" \
  --capture-dir "$construction_probe/clone/frames" --capture-every 1 ztest_all
```

Namespace or interface failure is a failed experiment; never rerun without
isolation. `RT_UNIT_LOG` does not supply all construction/worker/queue fields,
and raw clone UI traces do not contain the normalized construction contract.
The clone diagnostic command disables audio output; use separately audited
audio request observers for completion sound IDs. Neither sound silence nor a
missing log establishes an absent completion event.

Collect record/replay controls and two independent reference repeats before
interpreting a clone difference. For each catalogue type/variant, measure:

1. Accepted placement tick, placeholder/body/overlay sprite identity and exact
   occupied mask before work, including the initial interval without a body.
2. Assigned worker identity/state/position every tick through walking, start,
   interruption, reassignment and resume. Observe actual stage boundaries, HP
   and damage separately from nominal elapsed progress.
3. Balance and ordered queue before/after acceptance, start, every stage,
   cancellation, interruption and completion. Check cancellation while waiting,
   while active, while paused and after destruction; record refund rounding and
   repeated cancellation without assuming the clone's existing 8.8 formula.
4. Completion event tick and sound ID, selected identities, rally state, queue
   removal/next-item start, then first active/harvest tick. Retain a sample after
   the final event. Repeat all reachable speed branches and use measured
   durations to extend observation horizons; timeout is not completion.

After audited adapters produce three full `construction-v1` bundles, following
[the contract](../tests/parity/README.md#construction-flow-task-67-needs-live-run),
compare them in a fresh private output:

```sh
python3 -I tools/construction_compare.py \
  --reference work/parity/construction-live/reference/construction.json \
  --repeat work/parity/construction-live/repeat/construction.json \
  --clone work/parity/construction-live/clone/construction.json \
  --output work/parity/construction-live/comparison.json
```

Exit 0 means equality within the declared observations; 1 is a clone difference
or unstable reference; 2 is an input/alignment/coverage error. Missing fields or
frames cannot pass. First resolve repeat instability, then fix engine behavior
from measured tick/state/event differences and add asset-free boundary tests.
Keep overlay exact differences separate from tolerant visual metrics. Promote
V08 P to M only with reviewed reference measurements, and retain **needs live
run** for unobserved classes/variants. All maps, logs, normalization definitions,
numeric captures and images stay in ignored `work/`; none belong in git.

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

## Economy and town service run sheet (needs live run)

V08/V09 follow-up, 2026-11-06: **S** for asset-free engine continuation and
comparison checks; **P** for the five `tests/parity/town.json` families and their
variants. No live reference was launched in this sandbox and no new repeated
measurements justify an engine behavior change. Task 64's semantic input/reference
observer adapters and task 66 placement acceptance remain incomplete. This work
completes the independent comparison/test portion only. Parallel construction
remains a default-off modification. Status: **needs live run**.

Use the task 66 placement catalogue and task 67 construction catalogue/run sheet
above for all building classes, placeholder, worker travel, stages, payment,
cancel/refund and completion sound/selection/rally. The town catalogue adds clock,
training/spawn, service/protection and upgrade/build-tree/save observations.
Run every selected variant separately with the same race, level, speed, resource
mask, cash, prerequisites, hall identity, initial HP and controlled AI/fog state.
Showcase objects are initially complete: establish a matched legal setup rather
than assuming the generated map directly executes a construction scenario.

The following exact preparation commands create independent diagnostic copies
for one case. Repeat with a fresh output path for each variant, race, speed and
save boundary. No command below executes a high-level catalogue recipe by itself.
Run from the worktree root on the orchestrator's namespace-capable host:

```sh
export town_probe="$PWD/work/parity/town-resource-clock-live"
python3 - <<'PY'
import os
from pathlib import Path
import shutil

source = Path('/home/adrox/aztec-re/game')
root = Path(os.environ['town_probe'])
root.mkdir(parents=True, exist_ok=False)
for label in ('reference', 'repeat', 'replay', 'clone'):
    game = root / label / 'game'
    (game / 'Maps').mkdir(parents=True)
    (game / 'Saves').mkdir()
    for name in ('Aztec.exe', 'Res', 'Video', 'Music', 'Campaign'):
        if (source / name).exists():
            (game / name).symlink_to(source / name)
    for item in source.iterdir():
        if item.is_file() and item.suffix.lower() in ('.ini', '.plr'):
            shutil.copy2(item, game / item.name)
    for item in (source / 'Maps').iterdir():
        if item.is_file() and item.stem.lower() == 'mission_14':
            shutil.copy2(item, game / 'Maps' / item.name)
PY
for label in reference repeat replay clone; do
  python3 -I tools/make_testmaps.py --game "$town_probe/$label/game" || exit 1
done
```

Resolve case inputs from each generated layout manifest and audit identical
private map/setup changes. Supply the audited reference navigation/input route
as `$town_probe/reference/input.txt` and the clone route as
`$town_probe/clone/input.txt`. Millisecond `RT_SCRIPT` and frame-based clone
scripts are diagnostic controls; they cannot establish tick alignment. Task 64
must provide measured command translation, tick phase and semantic observers.
Do not manufacture `town-v1` from raw HP deltas or engine output. The supplied
runtime may require a private rebuild for observer hooks.

These diagnostic commands preserve isolation and record/repeat/replay separately:

```sh
test -s "$town_probe/reference/input.txt" || exit 1
for label in reference repeat replay; do
  town_replay_env=()
  if [ "$label" = replay ]; then
    test -s "$town_probe/reference/commands.txt" || exit 1
    town_replay_env=("RT_CMD_REPLAY=$town_probe/reference/commands.txt")
  fi
  (
    cd "$town_probe/$label/game" || exit 1
    timeout -s KILL 1500 unshare -rn env -u RT_CMD_REPLAY \
      SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
      RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_RESOLUTION=800x600 \
      RT_SCRIPT="$town_probe/reference/input.txt" RT_DUMP_DIR="$town_probe/$label" \
      RT_CMD_LOG="$town_probe/$label/commands.txt" \
      RT_STATE_LOG="$town_probe/$label/state.txt" \
      RT_UNIT_LOG="$town_probe/$label/units.txt" RT_UNIT_EVERY=1 \
      "${town_replay_env[@]}" sh -ec '
        ip link set lo up
        ip addr add 192.168.1.125/24 dev lo
        exec /home/adrox/aztec-re/build/aztec_recomp "$1/Aztec.exe"
      ' sh "$town_probe/$label/game"
  ) > "$town_probe/$label/stdout.log" 2> "$town_probe/$label/stderr.log" || exit 1
done
test -s "$town_probe/clone/input.txt" || exit 1
timeout -s KILL 1500 unshare -rn env \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  AZTEC_USER_DIR="$town_probe/clone/profile" \
  build/aztec_engine --game "$town_probe/clone/game" --rules-dir "$PWD/docs/data" \
  --mod parallel_construction=off --size 800x600 --software --no-audio \
  --script "$town_probe/clone/input.txt" --ui-trace "$town_probe/clone/ui.jsonl" \
  --capture-dir "$town_probe/clone/frames" --capture-every 1 ztest_all \
  > "$town_probe/clone/stdout.log" 2> "$town_probe/clone/stderr.log"
```

A namespace/interface failure fails the experiment. Never bypass isolation.
Set route duration and timeout to cover at least two relevant harvest/service
periods and one sample after completion; 1500 seconds is a process limit, not a
measured duration. Check command readback and reference repeatability first.
Capture actual audio request IDs with an audited hook for the construction
contract; dummy audio and `--no-audio` do not prove absence of a sound request.

For service, observe generated work separately from the target HP transitions,
including protected targets that consume work, capped/zero shares, town-local
supply and unused-credit disposal. For hall/tree cases, use independently loaded
branches at paused, precompletion and postcompletion boundaries. Observe hall
identity, type, HP and the enabled build tree on every tick; require the same
result on uninterrupted and loaded branches in both implementations. Engine
command-log saves and reference simulation saves use separate formats; each
implementation loads its own saves. Audited reference save/load routes and
resumed observations remain dependencies; missing support blocks that variant.

After audited adapters produce complete bundles using
[the town contract](../tests/parity/README.md#economy-and-town-service-v08v09-needs-live-run),
run these exact comparisons in fresh private outputs:

```sh
python3 -I tools/town_compare.py \
  --reference "$town_probe/reference/town.json" \
  --repeat "$town_probe/replay/town.json" \
  --clone "$town_probe/clone/town.json" --output "$town_probe/replay-comparison.json"
python3 -I tools/town_compare.py \
  --reference "$town_probe/reference/town.json" \
  --repeat "$town_probe/repeat/town.json" \
  --clone "$town_probe/clone/town.json" --output "$town_probe/comparison-before.json"
```

Exit 0 means equal declared observations; 1 means a clone divergence or unstable
reference; 2 means invalid, missing or unaligned input. The first comparison
also checks replay stability, but never substitutes for the independent repeat.
For construction observations use `tools/construction_compare.py` as above;
that contract retains worker, sound, selection, rally and exact overlay checks.
Return the private normalization audit, command readback, full reference repeat
and clone-before bundles plus comparison outputs to the orchestrator. Fix only
divergences confirmed by both original runs, then add an asset-free regression
and repeat the matched clone as `comparison-after.json`. No reference/clone
values are currently measured. Keep all logs, saves, numeric observations,
frames and game-derived material in ignored `work/`; publish only reviewed
factual expectations and evidence labels. V08/V09 remain P / **needs live run**.
## V07 all-weapon run sheet, task 87 (needs live run)

**P:** `tools/weapon_measure.py` inventories the existing public metadata for all
90 types and all 60 weapon IDs. It records direct owners, payload/composite
children, reachable owners, missing tables and IDs requiring a dispatch audit.
These are candidate bindings from the current engine, not newly verified
reference mappings. Ordinary Building descriptors have no weapon binding;
Boat/Johnka/Camera bindings remain unresolved. Do not infer immunity or an absent
attack from these entries. No new numeric data was extracted from private files.

**S:** synthetic tests check all bindings, every descriptor attacker/victim pair,
structure placement, primary-HP overrides and the repeated-reference comparison
gate. There are 4,290 proposed cases: 39 descriptor attackers against 87 positive-HP
descriptor victim types, plus the 18 task-74 controls and reload, range boundary,
composite schedule, target death and corpse removal for each attacker. Testing
all victim types preserves distinct physical armor and overlapping categories;
it does not certify the engine's category mappings. The seven task-74 family
names alone do not cover every dispatch class.

**M:** none from this task. No reference was launched in the sandbox. Task 64's
audited setup/command/reference observer is absent; task 74's placement generator
and engine trace diagnostics are present. No combat behavior or unresolved engine
mapping was changed. Keep unknown families disabled until two independent
reference runs confirm a divergence. The table reports this task's evidence,
not a rejection of historical inspected descriptor arithmetic.

| Existing dispatch class | Measured | Matching | Fixed |
| --- | --- | --- | --- |
| 0 ball | No | Unmeasured | No |
| 1 arrow | No | Unmeasured | No |
| 2 direct | No | Unmeasured | No |
| 3 axe | No | Unmeasured | No |
| 4 | No | Unmeasured | No |
| 5 | No | Unmeasured | No |
| 6 | No | Unmeasured | No |
| 7 flame/blast | No | Unmeasured | No |
| 8 wrapper | No | Unmeasured | No |
| 9 payload | No | Unmeasured | No |
| 10 | No | Unmeasured | No |
| 11 unresolved | No | Unmeasured | No |
| 12 composite | No | Unmeasured | No |
| 13 direct | No | Unmeasured | No |
| 14 sentinel (ID 0) | N/A | N/A | No |

Run from the worktree root. Planning needs no installation or executable:

```sh
python3 -I tools/weapon_measure.py plan --output work/parity/v07-all-plan
```

Keep its `inventory.json`, `scenarios.json`, `coverage.md` and blank
`measurement-form.json` private. Null values mean unmeasured. Unowned weapon IDs
remain listed; measure them through a verified parent launch or document their
unreachable/unused status after a live dispatch audit. Never invent an owner or
launch arbitrary weapon IDs with memory pokes as a normal gameplay comparison.

The following exact commands prepare and launch one candidate on a namespace-
capable host. Substitute another ID from `scenarios.json`, with a fresh output
for each attempt. `prepare` uses the task-74 ztest_battle generator, places both
attacking and defending structures through Town sections, and supplies primary
HP from the current engine bindings. Requested positions may relocate; audit
actual `.layout.json` and `pending.json` before collecting a rule measurement.

```sh
python3 -I tools/weapon_measure.py prepare --id type27_clear_middle \
  --game /home/adrox/aztec-re/game --output work/parity/v07-all/type27-clear-middle
weapon_case="$PWD/work/parity/v07-all/type27-clear-middle"
build/aztec_map_viewer --maps "$weapon_case/game/Maps" --dump ztest_battle \
  > "$weapon_case/map-dump.txt"
export ZSTAT_GAME_DIR="$weapon_case/game"
export ZSTAT_RUNTIME=/home/adrox/aztec-re/build/aztec_recomp
export RT_UNIT_EVERY=1 TMO=120
mkdir "$weapon_case/record"
RT_CMD_LOG="$weapon_case/record/commands.txt" \
RT_STATE_LOG="$weapon_case/record/state.txt" \
  sh tools/zstat_run.sh ztest_battle "$weapon_case/record" 65000
for repetition in 1 2; do
  replay_dir="$weapon_case/replay-$repetition"
  mkdir "$replay_dir"
  RT_CMD_REPLAY="$weapon_case/record/commands.txt" \
  RT_CMD_LOG="$replay_dir/commands.txt" RT_STATE_LOG="$replay_dir/state.txt" \
    sh tools/zstat_run.sh ztest_battle "$replay_dir" 65000
done
timeout -s KILL 100 unshare -rn env \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  AZTEC_USER_DIR="$weapon_case/clone-profile" \
  "$PWD/build/aztec_engine" --headless --game "$weapon_case/game" \
  --rules-dir "$PWD/docs/data" --no-auto-orders --max-ticks 720 \
  --combat-trace "$weapon_case/clone-combat.jsonl" \
  --observations "$weapon_case/clone-objects.jsonl" \
  --tick-hashes "$weapon_case/clone-hashes.txt" ztest_battle
python3 -I tools/projectile_measure.py summarize \
  --trace "$weapon_case/clone-combat.jsonl" --output "$weapon_case/clone-samples"
```

These are launch diagnostics, not controlled attack commands or a normalized
combat observer. `zstat_run.sh` makes independent writable copies, sets dummy
SDL, and uses `timeout -s KILL ... unshare -rn` with loopback/address setup.
Namespace, setup, runtime or horizon failures remain failures. Never bypass
isolation. The supplied reference unit log cannot observe projectile, burn,
interception, secondary HP or corpse lifecycle fully. There is no reference
combat environment switch or audited adapter invocation to add to these commands
yet; do not relabel an engine trace or populate observations from expectations.

Before measurement, resolve task 64 and each candidate's pending controls. Use
a normal accepted attack with readback at the same simulation tick. For damage,
isolate one shot per victim/category; for reload, allow at least three launches
without moving or retargeting. For range, bracket actual accepted launches in
pixels with matched footprint/visibility controls. Sweep splash offsets from the
task-74 protocol, then radius minus one/radius/radius plus one separately for
every victim, relation and combined category. Measure both composite child
launches and impacts independently, through source/target death and interruption.
Track HP, secondary HP, armor, ground/render/air height, interception events,
burn start/end and damage ticks, death, corpse creation and removal. Extend
horizons until flight/burn/corpse completion and a damage-free tail are observed;
720 ticks or no damage alone establishes none of those durations. Retain native
ordered events to disambiguate multiple events within one tick.

After audited adapters produce the three complete `weapon-v1` bundles described
in [the contract](../tests/parity/README.md#v07-all-weapon-comparison-task-87), run:

```sh
python3 -I tools/weapon_measure.py compare \
  --reference work/parity/v07-all/type27-clear-middle/reference.json \
  --repeat work/parity/v07-all/type27-clear-middle/reference-repeat.json \
  --clone work/parity/v07-all/type27-clear-middle/clone-before.json \
  --output work/parity/v07-all/type27-clear-middle/comparison-before.json
```

Exit 0 is equality within the declared observations, 1 is a difference or unstable
reference, and 2 is an invalid/incomplete input. `repeatable_difference` permits
review of a candidate correction only after the observer/setup audit. It does not
authorize a rule inferred from clone behavior. Retain clone-before, make only a
confirmed correction, add synthetic boundary/replay tests, and rerun with
`clone-after.json` and a fresh `comparison-after.json`. Fill each metric's private
form from the actual samples, citing both repeated reference captures and the
adapter audit. Publish only reviewed factual expectations and aggregate counts;
keep maps, captures, raw numeric records and executable-derived inputs private.
Update V07 P to M only for the classes and controls actually measured.

Status: **needs live run**.
## L0 pool and mission startup follow-up (2026-11-06): needs live run

Task 82's embedded-ring correction and optional-probe failure handling are
present in this checkout. Its live acceptance is still pending. The supplied
orchestrator run removed the pool divisibility check temporarily and completed
mission 1 at ticks 1/6/12: 63 common live objects, equal money, nine initial HP
differences, and movement counts 33 versus 15 at tick 6. This is supplied evidence,
not a new repeated measurement. No reference or private-data process was launched
in this sandbox. Do not mark the mission as matching the original yet.

Read-only inspection of the private allocator establishes the layout:

| Pool offset | Meaning |
| --- | --- |
| `+4` | Reserved storage base |
| `+8` | Free-chain head, not an end pointer |
| `+0xc` | Requested allocation size including list links |
| `+0x10` | Allocation stride rounded to eight bytes |
| `+0x14` | Requested reservation slot count |
| `+0x18` | Allocated slot count |
| `+0x1c` | Committed bytes, increased by the OS page size |
| `+0x20` | Number of active lists |
| `+0x24` | Active-list sentinel storage |

Constructor `0x473c40` establishes the storage fields; list constructor
`0x473e00` adds eight link bytes and sentinel storage. Growth at `0x473bb0`
commits another page and exposes only complete strides. Allocation/free at
`0x473d80`/`0x473dc0` maintain the free head and allocated count. Consequently,
capacity is committed bytes divided by stride, rounded down. The remainder
cannot contain a valid slot. The observer retains the full doubly linked active
chain and disjoint free-chain/count audits, plus alignment and overflow checks.
Synthetic controls cover the reported byte-size/stride combination, 63 live
objects, all complete free slots, and rejection of a pointer into the partial
tail. These are format facts and independently written tests; no derived code
has been copied into the repository.

The initial HP cause is a loader mismatch, not evidence for a side handicap.
Mission unit loader `0x430930` scans three integers separated by two characters
(five conversions), then constructs the specified type. It never consumes the
fourth integer as health. Marine constructor `0x433190` initializes primary HP
from descriptor d1 and secondary HP from d4. The existing descriptor data gives
12000 for type 6 and 13000 for type 32, explaining both reported differences.
The clone now leaves mission-placement health to descriptor initialization;
the parser still retains the fourth integer for map inspection. Explicit spawn
health and saved health keep their existing semantics. Asset-free regressions
cover both owners, town/team placement, omitted/negative/below/above-descriptor
metadata, explicit instance health and saved secondary health. The previous
all-type initialization test now checks descriptor health instead of preserving
its invented fourth-field value. The live nine-to-zero HP result is **pending**.

Movement remains unresolved. Inspection of mission 1's script finds scheduled
attacks and attacked-group reactions, without a direct immediate startup move
action. This does not rule out an early reaction, tactic formation movement or
an observation-phase difference. Do not add an invented idle walk or a
mission-specific position correction. Marine Think (`0x433db0`) establishes
execution states 0 inactive, 1 idle, 2 walk, 3 attack, 4 death, 5 idle animation;
current-step destination at `+0x74/+0x78` is in pixels and walking velocity at
`+0x98/+0x9c` is in pixel/256 per tick. The observer now decodes these states and
records the actual step destination, replacing the misleading tile-centre
value. Candidate attack, firing and pursuit targets are separate handles at
`+0x90/+0x94/+0x8c`. The existing tactic state/mode, anchor and route diagnostics
remain available. Synthetic tests deliberately give the tile and step different
values and retain distinct targets and signed velocity.

The embedded town ring additionally reports `queued_types` in the same logical
order as `queued_handles`, after validating each object's type-field span and
type range. Failed records retain raw pointers and have no usable handles/types.
These improvements decode native execution and waiting-ring entries; they do
**not** complete normalized logical orders or production remaining/total/payment
semantics. Those domains remain declared unavailable in the shared contract.
Other mobile classes still need an order-layout audit. No movement engine fix,
full order/queue parity or measured owner queue count is claimed.

Build the independent engine and the updated reference observer from this
worktree. The supplied external runtime predates these changes unless rebuilt:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build --parallel 2
cmake -S . -B build_l0_reference -DCMAKE_BUILD_TYPE=Release \
  -DAZTEC_GEN_DIR=/home/adrox/aztec-re/work/gen
cmake --build build_l0_reference --target aztec_recomp --parallel 2
```

On the namespace-capable orchestrator host, run both repeats of all three
scenarios. Use a fresh `startup_probe` for each attempt, preserve nonzero
contract verdicts and inspect `runners.json` for process failures. The runner
creates separate private game directories (immutable asset symlinks, copied
mutable files), uses dummy SDL video/audio, and launches inside `unshare -rn`
with the required reference loopback setup. Never bypass a failed namespace.

```sh
startup_probe="$PWD/work/parity/l0-startup-$(date -u +%Y%m%dT%H%M%S)"
mkdir -p "$startup_probe"
for scenario in private_l0 private_l0_mission_2 private_l0_mission_3; do
  for repetition in 1 2; do
    python3 -I tools/parity_run.py "$scenario" --flat-output \
      --game /home/adrox/aztec-re/game \
      --runtime "$PWD/build_l0_reference/aztec_recomp" \
      --engine "$PWD/build/aztec_engine" --timeout 120 \
      --output "$startup_probe/$scenario-$repetition"
  done
  python3 -I tools/parity_l0.py \
    "$startup_probe/$scenario-1/reference.json" \
    "$startup_probe/$scenario-1/clone.json" \
    --repeat-reference "$startup_probe/$scenario-2/reference.json" \
    --repeat-clone "$startup_probe/$scenario-2/clone.json" \
    --output "$startup_probe/$scenario-review.json"
done
```

Each review reports per-tick scalar counts (common objects, compared fields,
HP/position/all scalar divergences and unmatched identities), plus reference
owner-0/1 live object, decoded-unit-state, town and waiting-queue-entry counts.
Scalar scope is exactly type/owner/x/y/HP plus four side balances, excluding
unavailable orders, queues and UI. Failed town enumeration or a failed owner
queue yields null counts, never zero. Reviews first require repeated bundles
and frame bytes to match within each program; `run_nondeterminism` blocks the
counts. Raw handles and all numeric streams/reports remain private.

For the first changed movement step, generate dense private versions of each
scenario with ticks 1 through 12 (do not edit the tracked passive scenarios):

```sh
python3 - <<'PYDENSE'
import json
from pathlib import Path
root = Path('work/parity/l0-startup-dense-scenarios')
root.mkdir(parents=True, exist_ok=False)
for name in ('private_l0', 'private_l0_mission_2', 'private_l0_mission_3'):
    scenario = json.loads(Path(f'tests/parity/{name}.json').read_text())
    scenario['observe']['ticks'] = list(range(1, 13))
    (root / f'{name}.json').write_text(json.dumps(scenario, indent=2) + '\n')
PYDENSE
```

Repeat the loop above with a fresh `startup_probe` and append
`--scenarios-dir "$PWD/work/parity/l0-startup-dense-scenarios"` to each runner
invocation. Return both repeats, native order/queue diagnostics, process logs and
reviews privately. Confirm the first changing execution state/step against tactic
state, mission actions and spawn ordering before implementing movement changes.
Preserve the clone-before captures separately, then rerun the identical schedules
after each fix. Populate the matrix only from reviewed comparisons; use null for
unmeasured after-values and missions 2/3 until those captures exist.

Status: **needs live run**. HP loader correction and pool/diagnostic regressions
are implemented; repeated reference confirmation, movement reproduction,
normalized orders/production semantics and mission-2/3 divergence results remain
open. This is not the first verified matching game output yet.
