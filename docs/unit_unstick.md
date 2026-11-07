# Unit unstick helper (task 102)

Implementation and synthetic validation: complete. Original-game observation:
**needs live run**. No reference process was launched for this task.

Task 48's modification mechanism is present. Task 104's faithful/enhanced
profiles are absent, so `unit_unstick` defaults **off**. When profiles are
introduced, faithful must keep it off and enhanced should enable it. Task 64's
shared custom-map input/observer adapter is also absent; the work here does not
claim paired movement parity or implement that adapter.

## Switch and persistence

Use `--mod unit_unstick=on` or `--mod unit_unstick=off` with the graphical engine,
headless engine or `path_bench`. The graphical Game modifications menu also has
a Unit unstick toggle, available before and during a mission. It persists in the
existing per-user `modifications.ini`, overridable with `--modifications-file`:

```ini
parallel_construction=off
unit_unstick=on
```

Graphical CLI settings override the persisted file. As with parallel construction,
headless runs use explicit CLI settings and default off; they do not load the
menu's persisted file. Modification mask `0x40` records this option in initial save
settings and subsequent `SetModifications` commands; `0x2` remains assigned to
relaxed placement; bits 2–5 encode parallel training. Replays restore it and
mid-session toggles deterministically. No save format version change is needed;
older binaries correctly reject saves containing the newly assigned bit.

## Recovery policy

The enabled mover tracks best distance to the assigned destination slot. Motion
must improve that distance by more than one pixel to reset its watchdog; jitter
and repeated loops cannot continually postpone recovery. Every 12 ticks without
such progress it records a stall event and attempts friendly cooperation and a
local repath. Existing ghosting, squeezing and early crowd settlement heuristics
remain exclusive to the disabled behavior.

The local occupancy-aware search examines at most 81 cells in a 9 by 9 square.
It uses full disc clearance, dynamic footprints, occupied positions and swept
segments, and rejoins the shared field at a lower integration cost. Steering
also tries this search when its preferred segment is occupied. A free direct
passage retains direct steering; an occupied passage can select a local detour.
If no local detour exists, ordinary collision avoidance continues while the
watchdog waits for cooperation or emergency recovery. This is a local search,
not a new global occupancy field.

An idle or arrived friendly blocker first looks for a nearby side bay outside
the moving unit's forward lane. Without a bay, both units can exchange positions
atomically if both discs fit and neither destination intersects another unit.
Explicit hold units, enemy units and corpses are never cooperative blockers.
Current simulation side IDs determine friendship, including ownership changes.
The blocked mover retains its order; the yielding unit retains its idle/arrived
state at its new location.

After 120 ticks without progress, recovery relocates the mover to the nearest
free tile centre offering lower shared-field cost, or the nearest reachable
free tile if no such progress tile exists. Its present tile is excluded. Distance
ties use row/column order. Terrain, movement class, full disc radius, building
footprints and all living unit discs are checked. The same order then resumes.
This explicitly permits crossing an occupied enclosure during emergency recovery.
If the map has no legal free destination, relocation is physically impossible;
the order remains active and retries instead of overlapping another object or
entering a wall. Fully occupied destinations cannot acquire a guaranteed arrival
until space becomes available. Slow routes and unavoidable detours can also
activate the watchdog; this helper makes no optimal-time claim.

All choices use integer simulation state, stable agent order and fixed
unit/tick-seeded neighbour ordering. No clock, external RNG or rendering state
participates. Enabled state hashing includes the option, team, watchdog and local
waypoints. Disabled hashing retains the previous byte-for-byte contract.

## Synthetic results

Reproduce without assets or namespaces:

```sh
sh tools/bootstrap.sh --no-hooks
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
pre-commit run --files docs/unit_unstick.md
build/path_bench --json --n 10 --ticks 1200 --unstick-scenarios --mod unit_unstick=off
build/path_bench --json --n 10 --ticks 1200 --unstick-scenarios --mod unit_unstick=on
```

The four explicitly requested `unstick_*` courses are separate from the legacy
16-course default catalogue. Maps remain 90 by 60 tiles. Columns start in single
file on both sides of the same three-tile-long, one-tile-wide gap; five units
move each way simultaneously. The boxed scenario has one mover and eight idle
neighbours. The forest corridor is small-unit-only terrain byte 1 through a
21-tile-long corridor, with two idle blockers. The stationary gap has one mover
and one idle blocker. All agents are radius 18, speed 9, Small and friendly.
`--n 10` controls the column count; the other scenarios have fixed populations.

Arrival is first distance at most **2 pixels from the assigned slot**, measured
in completed simulation ticks after the order. The JSON `arrival_ticks` array
preserves mover creation order and uses -1 for a censored arrival. Idle blockers
receive no arrival metric. The budget is 1200 ticks, ending early after all first
arrivals. Legacy courses retain their six-tile goal disk and old tick convention.

A comparable stall count (`stall_windows`) counts each complete 12-tick interval
of displacement below 0.5 pixels per tick before first arrival. `stall_events`
separately counts algorithm detections: disabled windows are 24 ticks and enabled
windows are 12 ticks, with different progress criteria, so those event totals
must not be interpreted as the same measurement.

| Scenario | Arrival ticks off | Arrival ticks on | Stall windows off / on | Recovery events off / on |
| --- | --- | --- | --- | --- |
| Opposing column | 321, 321, 309, 349, 350, 187, 200, 242, 339, 318 | 273, 378, 371, 311, 338, 146, 237, 185, 160, 258 | 12 / 1 | 50 / 64 |
| Boxed by idle friends | 181 | 176 | 2 / 2 | 1 / 2 |
| Forest corridor | 338 | 182 | 8 / 0 | 6 / 2 |
| Stationary gap blocker | 174 | 181 | 0 / 0 | 1 / 4 |

Every ordered unit arrives in both modes. Enabled cooperation performs four
side steps and two emergency relocations in the column, two side steps in the
box, two swaps in the forest and one swap in the stationary gap. The column's
last arrival is 28 ticks later, but its maximum idle interval drops from 46 to
12 ticks and sampled deep overlaps drop from three to zero. The stationary gap
also regresses by seven ticks. These tradeoffs are retained in the report;
this option promises recovery, not a universally faster route.

The 48 existing course/count cases (1/10/30 units, 1490 ticks per order) were
captured before and after implementation: every existing hash and non-timing
metric is unchanged with the helper off, including golden hash
`74aad6e9f83f2c2e`. Synthetic tests repeat enabled and disabled scenarios, verify
free versus occupied route selection, friendly swaps/side bays, emergency escape
from a held enemy enclosure, footprint clearance, preservation of enemy positions,
persisted/CLI switches, initial-save replay and live modification commands.

## Original observation: needs live run

Run only on a host where network namespaces and namespace loopback setup work.
These exact commands use the supplied read-only installation/runtime. The
existing launcher creates a fresh private game directory per process, symlinks
immutable assets, copies mutable settings and uses dummy SDL, `timeout` and
`unshare -rn`. Never retry against the shared installation or without isolation.
Outputs stay under ignored `work/`.

```sh
live_root="$PWD/work/unit-unstick-live-$(date -u +%Y%m%dT%H%M%S)"
mkdir -p "$live_root"
for course in unstick_column unstick_box unstick_forest unstick_stationary; do
  for repeat in 1 2; do
    output="$live_root/$course-$repeat"
    ZPATH_GAME_DIR=/home/adrox/aztec-re/game \
    ZPATH_RUNTIME=/home/adrox/aztec-re/build/aztec_recomp \
      sh tools/zpath_run.sh "$course" 10 explored "$output" 180
    python3 -I tools/zpath_analyze.py "$output" --ticks 1200 --json \
      > "$output/analysis.json"
  done
 done
```

The generator now places the same adversarial terrain and units as the synthetic
courses. Stationary units have group -1, are excluded from selection rectangles
and orders, and must still be identified in the reference log. The analyzer
rejects missing stationary blockers. Selection rectangles for these courses are
tight around the moving column/unit to leave adjacent blockers idle. Inspect
initial reference positions and actual selection membership before accepting a
run; the original may adjust initial formations.

The legacy reference analyzer still reports its six-tile **goal-centre** disk.
Its results cannot be compared numerically with this task's two-pixel assigned
**slot** arrivals. Retain native logs and independently audit identities, order
boundaries, stationary blockers, arrival regions and repeated state before any
paired comparison. Task 64's missing adapter prevents an audited automated
reference/clone comparison here. Original arrivals, original stall behavior and
any parity verdict remain unmeasured; the implementation, synthetic recovery
checks and disabled hash preservation do not require that live dependency.

## Verification in this worktree

Baseline and final Release builds pass. The final CTest run passes all 57
runnable entries; five private-data entries and the restricted UDP process
entry skip. All pre-commit hooks pass for tracked files and this new document,
and `git diff --check` is clean. The 34 zpath tooling tests include repeatable
helper metrics, cross-language course geometry and missing-blocker rejection.
All changes remain uncommitted because git metadata is read-only. No game assets,
reference captures, executable-derived code or merge-hotspot documents changed.
