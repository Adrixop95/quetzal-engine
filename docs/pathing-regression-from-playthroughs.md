# Pathing regression from playthroughs (task 128)

Synthetic implementation and regression coverage are complete. Original-path
acceptance remains **needs live run**. No original executable or reference runtime
was launched. Task 112's player-command framework and task 102's recovery helper
are available in this checkout. Faithful/enhanced profiles are also present;
the older profile-availability note in `unit_unstick.md` predates them.

Evidence labels in this document: **V** means a reference-code observation,
including the cited prior audit; **G** means an assumption, interpretation or
proposed acceptance rule. Synthetic results describe this engine only and are
not original-game evidence.

## Courses and execution

`tests/pathing_stress.py` generates twelve invented 40 by 28 maps, empty resource
archives and deliberately invented rules. It reads no installation, executable,
private captures or research data. The moving ground types have speed 8 and the
small movement class. Vessels use the engine's default speed 8. The engine's
provisional collision discs remain in use. These fixtures cannot establish
original radii, speeds, balance, footprint masks or vessel controllability.

| Course | Stress and acceptance |
| --- | --- |
| narrow | One unit through a three-tile-long, one-tile-wide passage. |
| forest | Small-only corridor with two idle friendly blockers. |
| dense | Sixteen units converge on one destination in open terrain. |
| allies | An idle ally occupies the narrow passage. |
| blocked | An impassable destination tile; accept the declared adjacent approach region. |
| unreachable | A wall separates start and target; accept the declared approach on the start side. |
| edges | Travel from the upper left tile to the lower right tile. |
| group | Sixteen units, group assign/recall, then a two-tile passage and group arrival. |
| construction | Build a real unfinished two-by-two structure on the route through the ordinary placement/payment command. |
| water | Vessel movement within a water channel. |
| shore | Vessel approaches the last water tile at the shoreline. |
| opposing | Two friendly four-unit columns cross in opposite directions through the same narrow passage. |

G: acceptance regions are tile rectangles with radius one for single units and
radius three for groups. `unreachable` explicitly targets the last reachable
column for measurement, while retaining the player's original destination in the
command. This is closest-approach success, not arrival at an inaccessible tile.
The shoreline command targets water: in the current engine a vessel move onto
land invokes unloading, a separate behavior already covered by task 112's demo.

Normal selection, `Move`, group assign/recall and `PlaceBuilding` commands run
through the task-112 framework and ordinary simulation. The fixtures' mission
victory requires all commanded units in their declared goal regions and at least
850 elapsed ticks. The hard budget is 900. No player script forces victory or
changes object positions, movement state, construction progress or money.
The test additionally requires every commanded unit to have finished its order
and remain in its accepted region at the final snapshot. An early crossing alone
cannot pass. The construction test witnesses an actual unfinished structure and
checks mobile positions against its footprint. Every course also checks map
bounds and terrain medium at every observed tick.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build -R '^(path|pathing_stress|playthrough_framework)$' --output-on-failure
python3 tests/pathing_stress.py --out work/pathing-stress-attempt-1
python3 tests/pathing_stress.py --out work/pathing-stress-attempt-2
```

The CLI always launches under `unshare -rn`, with dummy SDL audio/video and a
private generated installation and user directory per process. It never runs
against the user's installation. The injected launcher in unit tests is restricted
to invented fixtures, matching task 112's test convention. Namespace/launch
failure produces a failed framework report and zero arrival coverage; it cannot
become a successful empty run. No original process is used by any test or CLI.
Use a new output directory each time. Reports, maps, input scripts and observations
remain under ignored `work/`. The Python CTest repeats all 24 course/profile
combinations, comparing every observation and metric. It also exercises stationary
failures, looping motion, false early arrival, displacement jumps, an unreachable
grid route and process failure.

The route runner now accepts `--profile faithful|enhanced`, defaults to faithful,
records the requested profile in JSON, and uses the same profile for capture
replay. Existing task-112 invocations retain their default.

## Measurements and limits

The JSON preserves per-object command tick, first arrival, first settlement,
traveled pixels, grid baseline, time ratio, jitter, oscillation, maximum stationary
interval, stuck episodes, failed-path counter, displacement jumps, final settlement
and deadlock verdict. Markdown reports arrival rate, settled count, last arrival,
maximum time ratio and the motion anomaly totals.

- First arrival is the first living, unembarked snapshot inside the declared
  region. Elapsed time includes the command's first completed update.
- Settlement additionally requires no active order. Final settlement is checked
  independently, so later displacement or disappearance cannot hide behind an
  earlier arrival or settlement.
- The baseline is an exact Dijkstra shortest route on the fixture's eight-neighbour
  tile-centre graph, with 48-pixel axial and Euclidean diagonal costs, no corner
  cutting, terrain-class checks and the construction footprint. It ends at a
  passable tile centre in the accepted region and divides by synthetic speed 8.
  G: this is an obstacle-aware grid optimum without crowds, wall penalties or
  turning costs, not the continuous geometric optimum or the original wave metric.
  Actual motion crosses the rectangle boundary before its centre and can cut
  across open tile interiors, so ratios below one are expected.
- Jitter counts direction reversals with a negative consecutive nonzero displacement
  dot product. Oscillation counts A-B-A tile transitions, ignoring repeated samples
  in one tile. Both can include useful avoidance or detours; neither proves a bug.
- A stuck episode is 120 consecutive unchanged integer-pixel positions with an
  active order. A deadlock is an unsettled final unit with no improvement of at
  least one pixel toward its accepted region for 120 ticks. These are G diagnostic
  thresholds. Final-settlement acceptance also catches persistent movement inside
  the region that would evade the stationary detector.
- Motion statistics continue after first arrival through the last snapshot.
  Displacements over 12 pixels are reported as jumps against nominal speed 8;
  these expose recovery/cooperation and overlap correction rather than certifying
  a specific relocation cause. They are never hidden as ordinary path speed.
- The framework's ordinary anomaly report remains attached. Idle/economy and
  raw requested-goal timers exceed the scenario budget because these courses
  deliberately contain idle blockers, no income, formation slots and explicit
  closest-approach goals. Actual stationary-order detection remains at 120 ticks.
  Failed local searches are measured, with a high failure threshold to avoid
  interpreting normal occupancy-aware search attempts as a permanent stall.

## Synthetic results

Both profiles achieve 49/49 arrivals and 49/49 final settlements across the twelve
courses, with zero stuck episodes and zero final deadlocks. Two complete CLI runs
under namespaces produce identical observation streams and metrics for all 24
course/profile combinations, in addition to the CTest repeats. These are engine
measurements on invented data only.

| Course | Last arrival faithful / enhanced | Maximum grid time ratio faithful / enhanced |
| --- | --- | --- |
| narrow | 165 / 165 | 0.982 / 0.982 |
| forest | 174 / 195 | 1.036 / 1.161 |
| dense | 160 / 232 | 1.026 / 1.487 |
| allies | 167 / 191 | 0.994 / 1.137 |
| blocked | 166 / 166 | 0.988 / 0.988 |
| unreachable | 75 / 75 | 0.962 / 0.962 |
| edges | 279 / 279 | 0.953 / 0.953 |
| group | 167 / 275 | 1.120 / 1.803 |
| construction | 166 / 166 | 0.974 / 0.974 |
| water | 81 / 81 | 0.964 / 0.964 |
| shore | 93 / 93 | 0.969 / 0.969 |
| opposing | 205 / 279 | 1.250 / 1.620 |

Across complete traces, faithful/enhanced totals are 470/891 jitter reversals,
59/204 tile oscillations and 1/14 displacement jumps. The additional enhanced
crowd motion and slower arrivals remain visible; the correction establishes
bounded settlement for these cases, not zero jitter or a performance advantage.
The JSON retains every unit and the complete first-arrival/settlement distinction.
The last first-settlement times across the courses are 292 faithful and 607
enhanced ticks after the corresponding command.

## Enhanced arrival correction

Previously the helper could move already arrived members of the same order to
successive side bays, dispersing a dense group. Other members continued chasing
occupied slots or repeatedly returned toward the shared-field anchor. A new
standalone mover regression requires all sixteen agents settled within three
tiles of the destination after 900 ticks; it exposed this failure independently
of the broad playthrough arrival rectangle.

With `unit_unstick` enabled, cooperation now leaves arrived members of the same
order in place. After a stalled interval, a unit whose nearby slot is occupied by
an arrived teammate can settle at its current collision-free position within one
tile of that slot. At the existing 120-tick emergency timeout, a unit within three
tiles of the goal first searches a bounded seven-by-seven area around that goal
for a free reachable tile centre, within three tiles of the goal and two tiles of
its assigned slot. The nearest candidate wins, with row/column ties. It relocates
and finishes there. If no candidate exists, existing recovery continues.

This is an explicit G enhanced policy, including emergency relocation. It cannot
guarantee arrival when no legal space exists. It does not claim universally faster
movement: enhanced crowds still incur additional avoidance and recovery. Neither
branch changes the helper-off/faithful movement path. All 48 legacy course/count
cases retain every hash and non-timing metric, including the existing pinned
movement digest. No new original-path expectation or measured-radius claim was
introduced.

## Reference-code comparison

The private lifted sources were inspected read-only under
`/home/adrox/aztec-re/work/gen`. Only factual descriptions are recorded here;
no lifted/decompiled passages, generated C, assets or numeric research dumps were
copied. See [rules_movement.md](rules_movement.md), especially its wave, Marine,
occupancy and group sections, and [unit_unstick.md](unit_unstick.md).

| Topic | Reference evidence | Relation to this change |
| --- | --- | --- |
| Wave search | V: the wave expansion uses low-bit cell delays; the prior audit identifies normal, unknown and wall-adjacent delays and start-rooted eight-direction discovery (`f_0046d330`, `f_0046d4d0`, private `gen_117.c`). | The engine retains goal-rooted flow fields with different costs. G: matching a synthetic shortest route cannot prove wave tie or route parity. |
| Occupancy | V: occupancy overlay suppresses occupied wave cells except unknown cells (`f_0046cc40`, private `gen_117.c`); the prior audit separates terrain layers from object occupancy. | The engine uses dynamic building footprints and disc collision avoidance. Synthetic construction clearance does not prove original footprint masks. |
| Footprint/reservation | V: the prior Marine step audit records tile-centre targets and reservation of the next tile at step start (`f_00433820`, private `gen_066.c`); target/vector fields were rechecked. | The engine's continuous provisional discs remain an acknowledged difference. G: no radius change is justified by these stress results. |
| Steering/blocking | V: the member-plan path calls the wave with occupancy enabled (`f_00453be0`, private `gen_098.c`); the prior group audit documents bounded local plans, waiting and direction alternatives in `f_00453e20`. | Enhanced side steps, continuous avoidance and relocation are helper policies. Faithful original-path acceptance still needs repeated native traces. |

Task 89's unresolved WaveSearch/footprint measurements and task 102's original
stall observations are not supplied by this checkout. Preserving faithful engine
hashes proves internal compatibility only. There is no new measured original path
against which to assert faithful agreement.

## Maintainer run sheet: needs live run

Capture on the Windows installation; Codex must not launch the original. Keep the
entire game installation, generated maps, scripts, logs, screenshots, trajectories,
object identities and numeric series private. The synthetic fixed-speed fixture
rules are deliberately not original descriptors. Do not directly compare their
timing table with native original timing.

1. Generate the courses with the CLI above on a namespace-capable Linux host.
   Transfer its generated synthetic `game/Maps/demo.*` files privately to the
   Windows capture machine. Preserve each course/profile directory separately.
   The map files are generated inputs; do not add their output to git.
2. For each original attempt, create a separate private installation with ordinary
   Maps/Saves and copied mutable settings/profile files. Link immutable Res,
   Music, Video and Aztec.exe read-only to the original installation. Fail setup
   if the private directories or symlinks cannot be created; do not fall back to
   running in the shared installation. Copy the selected course's `demo.*` into
   the private Maps directory. Load that custom map through the actual game UI;
   audit whether the original accepts its terrain, side, unit and vessel records.
   A rejected map or startup adjustment is an observation failure, not a path result.
3. Record executable/build identity, map hash, difficulty, handicap, modifications,
   fog/exploration, initial object identity/type/position, actual speeds and command
   membership. Reproduce each group selection and destination through native UI
   input. Apply the construction command at its measured order boundary. Retain
   native completed-tick positions and order/reservation state if the observer
   supports them. Wall-clock video alone does not establish an exact tick path.
4. Run each case twice from a fresh process with identical controls. Verify initial
   state and complete trajectory repeatability before classifying a difference.
   Inspect blocked/unreachable closest approach, stationary blockers, group arrival,
   footprint changes during construction and vessel/shore semantics separately.
5. Run the faithful clone twice on the same generated map with the normal audited
   rules matching the captured original descriptors, not the fixture's synthetic
   rules. For example, on Windows PowerShell, after staging the private installation:

   ```powershell
   $env:SDL_VIDEODRIVER = 'dummy'
   $env:SDL_AUDIODRIVER = 'dummy'
   $env:AZTEC_USER_DIR = "$PWD\work\pathing-native\clone-1-profile"
   & '.\build\Release\aztec_engine.exe' --game "$PWD\work\pathing-native\clone-1-game" demo --rules-dir '.\docs\data' --profile faithful --script "$PWD\work\pathing-native\clone-1-input.txt" --software --no-audio --no-light --no-fog --frames 901
   ```

   Use a separate generated player script beginning with `budget 900` and
   `record ABSOLUTE_PATH_WITHOUT_SPACES` followed by that course's retained
   `route/input.txt`. It must use the same native command tick and selection
   membership. Use a fresh game/profile/output for the repeat. The example's
   hidden fog/explored setting must be matched and audited on the original;
   rendering `--no-fog` alone does not establish wave exploration equivalence.
   A Linux clone collection can instead use `tools/playthrough.py --scenario`,
   `--game`, `--mission demo`, `--rules-dir docs/data`, `--profile faithful` and
   `--max-ticks 900`; that runner stages a private copy and requires `unshare -rn`.
6. Compare aligned completed ticks per object. Record the first differing tick,
   native versus clone tile/pixel path, arrival region, completed order, path
   length, grid baseline, stationary intervals, reversals, tile oscillation and
   deadlock tail. Classify wave tie, footprint, reservation, steering, goal fallback,
   selection/setup or observer mismatch. G: require the same original divergence
   in two successful runs before changing faithful movement. Keep before/after
   runs with unchanged original controls and a documented per-case tolerance.

Fill one row for every course above; **all original and paired-path cells are
currently pending**, including task-89/102 legacy courses. The native observation
adapter and tick/identity audit are prerequisites for automated path comparison;
the playthrough framework is a clone observer, not a native-original adapter.

| Case/profile | Original repetitions aligned | Clone repetitions aligned | Original / clone arrival ticks | Time / path ratios | Stuck / jitter / oscillation / deadlocks | First divergence and class | After-fix repeats and tolerance |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Each of the twelve courses, faithful | pending | pending with original rules | pending | pending | pending | pending | pending |
| Task-89 legacy movement configurations | pending | engine hashes preserved only | pending | pending | pending | pending | pending |
| Task-102 helper-off scenarios | pending | engine checks only | pending | pending | pending | pending | pending |

The synthetic suite and enhanced correction do not complete original parity,
Windows interactive verification, real mission playthroughs or fifteen-mission
campaign acceptance. The unmeasured parts remain needs live run.

## Verification

The baseline and final Release builds pass. Final CTest passes all 67 runnable
entries; five private-data entries and the restricted UDP process test skip.
All pre-commit hooks pass, including explicit checks of the new untracked files,
and `git diff --check` is clean. The two isolated full stress CLI runs agree at
every tick, and all 48 helper-off legacy benchmark rows retain identical hashes
and non-timing metrics. Changes remain uncommitted because git metadata is
read-only. No game assets, original executable, derived code or merge-hotspot
documents were added or changed.
