# Player-command playthrough framework

The route mode of `tools/playthrough.py` drives the independent engine's existing
`--script` playtest loop at virtual 12 Hz. It submits the same `Command` types as
`CommandsUi`; the ordinary simulation handles payment, prerequisites, placement,
pathfinding, combat and transports. It never grants money, creates units directly,
teleports, changes mission flags or forces an outcome. Selection changes only the
local UI selection, as mouse selection does. Group assignment and recall also use
normal queued group commands. Existing campaign sweep options and low-level
click/key scripts continue to work.

This is a framework, not fifteen completed mission routes or original-game parity
evidence. Worker-based resource gathering has no command in this checkout.
`harvest`/`gather` therefore construct income-producing buildings through the
normal build path; attempts to use a mobile worker fail as unsupported. The
synthetic loop exercises the implemented building economy. Worker orders require
a future engine/UI command implementation; this framework deliberately does not
invent one. Existing economy, AI and transport behavior can still have gaps.

## Run and repeat

Build and test inside your worktree, without the private reference runtime:

```sh
sh tools/bootstrap.sh --no-hooks
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
```

Run the invented town/passage/ferry scenario without any game installation:

```sh
python3 tools/playthrough.py --demo --max-ticks 2000 --capture-every 120
ctest --test-dir build -R playthrough_framework --output-on-failure
```

The generator in `tools/playthrough_route.py:demo_game` writes empty resource
archives, a synthetic map and a normal mission victory trigger into private
output. The public script is `tests/playtest/playthrough_demo.txt`. It builds a
factory and farm, witnesses income, trains two units, assigns and recalls a group,
moves through a narrow wall opening, boards three passengers, crosses a river,
unloads, then attack-moves the group into the objective area. Victory requires
all three units in that area. No victory or construction state is injected by
the player script. The test repeats the full route and compares every tick hash,
then compares with a run without telemetry. Negative integration cases exercise
unreachable goals, repeated failed paths, ignored orders, invalid placement,
malformed input, failed conditions, dormant unsupported actions and hard budgets.
Synthetic detector unit checks cover casualties, idle units, stuck orders and
income stalls. These checks do not establish mission balance or game parity.

A private mission route uses the same script language:

```sh
python3 tools/playthrough.py \
  --game /home/adrox/aztec-re/game --mission 1 \
  --scenario /home/adrox/aztec-re/work/playthrough/mission-1.txt \
  --capture-every 120 --max-ticks 43200 --timeout 180 \
  --out work/playthrough/reports/mission-1-attempt-1
```

Repeat the command with a new output directory for each attempt. Replay the
retained `input.txt` with `--scenario` and the same mission and engine/rules to
repeat a run. Compare the contiguous `observations.jsonl` hash fields to establish
internal repeatability. The pre-existing campaign sweep/save/replay driver is
described in [playthrough.md](playthrough.md); it does not accept this richer
script format through its older `--orders` interface.

The default mode is `--mode headless`: dummy SDL video/audio, software rendering,
no fog and no lighting. It renders offscreen so captures and UI clicks remain
available; it uses virtual time rather than the separate engine `--headless`
policy driver. `--mode xvfb` uses `xvfb-run`, X11, software rendering and dummy
audio for the same script. It requires working Xvfb/xauth/X11 sockets. Both modes
launch the engine inside `unshare -rn`; isolation or X11 failures remain failed
runs. There is no CLI switch to bypass namespaces. The tests replace the launcher
only for their generated fixtures. The process group is killed on wall timeout.

Every production run uses a private installation directory: ordinary Maps/Saves,
read-only asset/map symlinks and copied mutable configuration, plus a separate
`AZTEC_USER_DIR`. It never runs against the shared writable installation and never
launches the original executable or lifted runtime. Keep symlink targets stable
during an attempt. Namespace support is required on the Linux host. Tests skip
the optional live Xvfb check when this environment cannot create its X11 server;
that skip is not an Xvfb verification.

## Script commands

Commands are case-insensitive. `#` starts a comment. Arguments are whitespace
separated, with no quoting/expansion. Tile coordinates are zero-based; object IDs
are the engine's stable, one-based IDs. Each player command submits at the next
simulation tick and consumes one virtual frame. Successful conditions/checkpoints
can continue to the next statement in that frame. Empty or foreign selections,
unavailable production and invalid high-level arguments fail the script.

| Command | Behavior |
| --- | --- |
| `select type TYPE` | Select all live owned objects of this type. |
| `select id ID` | Select one live owned object. |
| `select group GROUP` | Recall control group 0 through 9, using `SelectGroup`. |
| `group assign GROUP` | Submit `CreateGroup` for the current selection. |
| `train TYPE N BUILDING_ID` | Queue 1 through 50 mobile units using `Build`; the building selects its town, as in the UI. |
| `build TYPE X Y BUILDING_ID` | Construct at a tile with the menu's `Build` or `PlaceBuilding`, normal footprint/terrain/occupancy checks and town rules. |
| `harvest TYPE X Y BUILDING_ID` | Construct an income producer through the build path; `gather` is an alias. Mobile worker gathering is unsupported. |
| `order-move X Y` | Submit `Move` to the tile center for the selection. |
| `attack-move X Y` | Submit `AttackMove` to the tile center. |
| `context ID` | Right-click semantics for an object: board an owned ferry, attack another side's object, or move to an owned ordinary object. |
| `unload X Y` | Submit the normal `Unload` command with nearby-land resolution. |
| `await TIMEOUT CONDITION` | Wait at most this many simulation ticks; failure or a terminal outcome before the condition is true exits with status 3. |
| `checkpoint CONDITION` | Assert now; failure immediately ends the script with status 3. |

`train` purchases through the engine's town production mechanism; it does not
force an arbitrary structure to produce a type. Prerequisites choose the actual
producer. Queues can contain unpaid work, and engine command rejection can still
occur after validation. Always witness completion with a count/checkpoint. The
ignored-order detector catches requests not queued/created in full.

Conditions use `NAME OP INTEGER`, where OP is `==`, `!=`, `<`, `<=`, `>` or `>=`:

| Condition | Observed value |
| --- | --- |
| `money >= N` | Local side cash. |
| `income >= N` | Actual cumulative local harvest income since initialization, independent of spending/refunds. |
| `count TYPE >= N` | Live, fully completed owned objects of a type. |
| `units SIDE >= N` | Live non-structure objects on this side (includes ferries). SIDE can be omitted for the local side. |
| `towns SIDE >= N` | Live town buildings on this side. |
| `tile ID X Y == 1` | The live, unembarked object's position is inside this exact tile. |
| `cargo ID == N` | Current passenger count of the object, or -1 if absent. |
| `complete ID == 1` | A live object has completed construction. |
| `objective TRIGGER_ID == FLAG` | Mission trigger's current enabled flag (0/1); an absent trigger reads 0. This does not create or complete an objective. |
| `tick >= N` | Absolute simulation tick. |
| `outcome == N` | 0 running, 1 victory, 2 defeat. |
| `selection == N` / `enemies == N` | Selected-object count / live hostile non-structure count. |

The old commands remain available: `wait`, `until`, `run`, `move` (mouse pixels),
`click`, `rclick`, `mclick`, `down`, `up`, `rdown`, `rup`, `drag`, `key`, `keydown`,
`keyup`, `shot`, `print`, `expect`, `quit`. Use `order-move` for tile movement;
`move` still moves the mouse. `expect` retains its accumulated failure behavior;
new `checkpoint` aborts at the first failed assertion. Prefer `wait`/`await` for
route captures: `run N` advances ticks without presenting intermediate frames.
An exact anomaly capture inside such a jump cannot be claimed; the report then
records capture failure. High-level commands, waits and malformed/unknown actions
cannot silently pass.

The runner prepends internal `budget N` and `record FILE` directives. The budget
is a hard simulation limit even for `run`; `--frames` also bounds stalled/paused
virtual frames. Internal `capture TICK FILE` directives schedule exact replay
frames. Route input must not contain any of these three runner-owned directives.
Output paths cannot contain whitespace or `#` because they enter this script
language. Direct `--script` users may use these directives themselves.

## Reports and detectors

The default new output directory is
`work/playthrough/reports/<UTC timestamp>/`. `--out` must name a new directory
inside this worktree's ignored `work/`. The adjacent `<name>-inputs/` holds the
private generated/copied installation. `report.json` (schema 1) and `report.md`
contain the verdict and anomalies. Raw process logs, input, UI trace, private
profile and post-tick object observations remain alongside them.

The JSON contains:

- `outcome`: `won`, `lost`, `timeout`, or `scenario-failed`.
- `engine_outcome`: the observed mission result, separately retained if a
  detector or checkpoint invalidates a victory.
- `ticks`: the last contiguous observed simulation tick.
- `script_complete`: whether all route steps/checkpoints executed; a victory
  reached at the budget boundary cannot hide skipped assertions.
- `process`: command, return code, wall-timeout flag and launch error if any.
- `scenario_sha256`, `engine_sha256`, `mode`, and configured detector `bounds`.
- `pending_goals`: orders still short of their target when the simulation ended.
- `anomalies`: `kind`, `tick`, `object`, `position` in world pixels, and English
  `reason`, plus a relative `capture` path when verified. Global events use null
  object/position. Unstamped log warnings use the last observed tick; that is
  their collection tick, not proof of the original event time.
- `capture_replay`: whether the second process reproduced the full tick/hash
  sequence and exit status, and its process diagnostics.

Observations finish with an `event: "end"` record containing the final tick,
script completion flag and failure count. A missing or inconsistent terminal
record cannot pass. Snapshot consumers should distinguish this record from
ordinary tick/hash/object rows.

The route CLI exits zero only for a victory with no anomalies. Loss, timeout,
checkpoint failures and anomalies return 1; configuration errors return 2.
Direct engine scripts use the existing status 3 for failed expectations. A route
ending while the mission is still running is a scenario failure unless a tick or
wall budget was exhausted. Wall timeouts remain timeouts with retained partial
anomalies. A clean defeat is `lost`, not a successful campaign completion.

| Detection | Default and interpretation |
| --- | --- |
| `goal-timeout` | A requested movement target was not approached within 600 ticks. Arrival allows 48 pixels on each axis for formation slots. Replacing a move replaces its monitored target; boarding/stop cancels it. |
| `goal-incomplete` | The scenario ended while still running, before a requested target was reached. |
| `stuck` | Position/order/target unchanged for 240 ticks while holding an order or boarding request. |
| `repeated-path-failure` | At least 3 actual unreachable requests or failed local repath searches for an object; not inferred from merely slow motion. |
| `passenger-lost` | An embarked passenger died or disappeared from a carrier without a living, detached object. |
| `transport-destroyed` | A live ferry was destroyed. |
| `economy-stall` | No actual recorded harvest income for 1200 ticks, even when spending hides positive income in cash samples. |
| `order-ignored` | Move, board, unload, attack or production observations disagree with the submitted request. |
| `idle` | An owned mobile non-ferry remained unembarked without an order for 1200 ticks. |
| `unsupported-action` / `engine-warning` | Dormant unsupported mission action sites, executions, engine warning/error/FAIL output or placeholder-rule fallback. |
| `process-failure` / `invalid-observations` / `capture-failure` | Launch/exit failure, broken evidence, or unverified/missing exact capture. |

Flags `--goal-ticks`, `--stuck-ticks`, `--idle-ticks`, `--economy-ticks` and
`--path-failures` configure the bounds. Tune them for intended waiting/production
cycles. Idle, attack and economy findings are investigation signals; the runner
fails conservatively. It cannot prove that every slow attack is ignored or every
idle unit is a bug. Detectors monitor owned objects; enemy AI failures are exposed
indirectly through outcomes and observed interactions, not a semantic AI oracle.
Natural victory/defeat stops the simulation: unexpired movement goals are retained
in `pending_goals` and do not invalidate that result. Use route checkpoints when
an exact arrival is itself an acceptance condition.

With `--capture-every N`, periodic captures go under `captures/`. If anomalies
occur, a fresh process replays the same script with scheduled anomaly-tick shots.
The runner attaches exact frames only after comparing every tick hash and exit
status. Missing captures or changed replay become explicit failures. Pre-launch
and tick-zero findings may have no rendered frame. Captures show the current
camera, so a reported object's position can be outside the image. Automatic
capture does not move the camera or change input. Captures and reports remain
private and git-ignored.

To add a detector, extend `Detectors.feed()` in `tools/playthrough_route.py` using
post-tick observations and `add(kind, tick, object, reason, identity)`. The identity
controls duplicate suppression across a persistent episode. Add an adversarial
synthetic test, including a nearby non-failing case, then test the full generated
route. If the observable is missing, add a read-only diagnostic to
`PlaytestScript::observe`; keep diagnostic counters out of save/hash state and
compare telemetry-enabled/disabled runs. Never infer a failed path solely from
an unchanged position or silently fill missing observations with expectations.

## Private mission routes

Keep all real mission scripts, map coordinates, object IDs, unit positions,
captures and reports under `/home/adrox/aztec-re/work/playthrough/` on the
maintainer's machine, or the equivalent ignored `work/playthrough/` in an isolated
worktree. The source game installation is `/home/adrox/aztec-re/game` and remains
read-only. Never add original assets, executable code, derived sources, captures,
real mission scenarios or generated outputs to the public repository. Only the
framework, this documentation and independently invented fixtures belong in git.
Do not convert a private winning route into a public regression fixture. Record
unavailable mission/engine prerequisites as blockers rather than forcing states
or claiming all-mission completion from this synthetic demo.
