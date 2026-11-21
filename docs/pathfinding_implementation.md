# Original wave search: implementation and integration limits

2026-11-21, baseline `91afe1c`, independent Release build. **Partial implementation;
not a completed replacement of game movement.** Ground point queries now use the
wave. The reservation-based mover is an explicit candidate
(`MoverConfig::original_ground`, or `path_bench --wave`), disabled by default.
Promoting it in the game failed existing integration gates. The default group
mover, water movement, enhanced unstick mode and startup formations retain their
existing policies. No original executable or reference runtime was run.

V below means a rule established in [the supplied specification](pathfinding_wave.md),
or a directly checked engine construction/measurement, as explicitly qualified.
G means an interpretation, engine policy or unresolved integration. Synthetic
success and repeatability establish neither original timing nor game parity.
A targeted read-only cross-check of `0x46bdb0`, `0x46c180` and `0x46d1c0` in
private Ghidra output confirmed eight-neighbor wall proximity and the documented
non-candidate extraction branch. No executable-derived implementation or lookup
data is included.

## Implemented rules

| Rule | Evidence and implementation |
|---|---|
| Framed u16 map; nibble 0/9/4/10/15 | V specification/static cross-check; `wave_search` builds fresh layers with a zero border. `wall_sides=false` selects base classes 0/1 (0/9), true selects margin classes 2/3 (0/4/9); candidate member requests use base layers. Fog brightness below 8 overrides terrain and occupancy; otherwise nonzero occupancy blocks the tile |
| Round fronts and discovery labels | V specification; start label 1, first discovery label 2, delayed entries carried between rounds. Leaving free/fog/wall-side costs 1/2/4; the start has zero departure delay. Diagonal and straight costs are equal |
| Expansion and corners | V specification; N, NW, W, SW, S, SE, E, NE. No orthogonal-neighbor corner test |
| Endpoints, radius and goal lists | V specification; clamped endpoints, blocked-start relocation by the first passable ring entry, square outline only for a radius, goal markers written afterward even over blockers/outline. Radius 6 leaves an 11x11 interior inside a 13x13 outline |
| Extraction | V specification; steepest nonzero label descent, clockwise compass enumeration from N, previous direction initially N, circular two-way tie and first-three-candidate rule. Node kinds 1/3 and both endpoints retained. One-node results provide no step |
| Unreachable endpoint | V specification; first Chebyshev ring containing labels, minimum label within that ring, first scanned tie. Scan top row, bottom row, left column, right column |
| Passability | V specification; Marine small/large terrain predicates retained. `can_enter` additionally checks occupancy; its margin option excludes wall-side cells. Eight-neighbor connectivity has a separate cache from the legacy field's four-neighbor connectivity |
| Candidate reservations and sidesteps | V rule, synthetic integration only; one destination tile reserved immediately, old reservation released before displacement, no disc pushes or swaps. Try straight, clockwise 45 degrees, counter-clockwise 45 degrees only; a sidestep discards the cached path |
| Candidate path admission | V rule, synthetic integration only; two requester identities, repeat grants for the same identity, next-tick grants from the two-entry retry-priority list, strict greater-priority insertion with stable equal ties. Denial increments retry count; grant resets it |
| Candidate Marine arithmetic | V formula, synthetic integration only; truncating fixed-to-pixel conversion, octagonal norm, constant step velocity, exact final snap, early next-step request within three speeds unless about to attack. Axis norm 47 and diagonal norm 66 for a 48-pixel step |
| Candidate Truck/Headtruck arithmetic | V formula, synthetic integration only; explicit `Agent::vehicle` selects signed wrapped turn difference, turn of eight, whole-pixel acceleration/braking `max*max/48+1`, pre-turn difference threshold 56, next request below 48 pixels, braking below 24, no final snap |
| Determinism | V engine construction; candidate reservation, cached path, retry, admission, team/fog, vehicle and pending-step state participate in hashes. Repeated synthetic runs compare every tick through a transcript digest. All 48 default benchmark hashes and non-timing metrics remain unchanged |

`astar` retains its historical API name but uses the wave for ground queries.
`legacy_astar` remains available for legacy-field tests and water behavior. Ground
`cost` is the discovery label minus two for a multi-node route: the first step is
free, and subsequent values measure departure rounds. This is not the cost of
walking the extracted route, since steepest descent need not follow cost edges.
The benchmark explicitly keeps its legacy geometric denominator for comparable
before/after ratios. Disc smoothing is a separate legacy helper; the candidate
never smooths its tile path.

## G policies and outstanding integration

- **G extraction safety:** the documented three-way rule can select a direction
  absent from the candidates. An invented 8x8 regression reaches the goal but
  selects a blocked predecessor. The engine reports `extraction_failed`, returns
  no usable route, and the point-query wrapper returns `found=false`. Which
  original behavior resolves this case is unconfirmed; no replacement tie rule
  is invented. Valid non-candidate choices that still descend remain permitted.
- **G memory safety extensions:** fronts grow beyond the original unchecked
  2000-entry buffers. Search stops before a 12-bit label wraps and reports
  `label_limit`; it can still return a labelled partial route. Oversized-front
  and label-limit synthetic checks cover these extensions.
- **G group allocation:** candidate destinations are distinct reachable tiles
  around the command anchor, in stable ID/ring order. This is an engine policy,
  not the original tactic's anchor/member allocation. Static disconnected goal
  allocation still precedes the member wave; the core's exact failed-wave ring
  policy must not be confused with this provisional group allocation.
- **G route ownership:** the candidate uses a global member wave and caches at
  most eight steps. The core supports local radius-6 searches, but original
  anchor routes, local target lists, repath timers, connected-member look-ahead,
  same-tactic result 3 and formation scans are not integrated. Retrying an
  exhausted partial route is provisional. Full tactic coordination is a missing
  prerequisite to promoting this candidate; this checkout has no completed
  original tactic adapter to wire in.
- **G layer integration:** callers supply fog and cleared own occupancy through
  the wave API. GameSim does not yet provide original per-side wave layers to
  the candidate. Current NavGrid wall proximity includes dynamic footprints;
  correspondence to original layer updates around buildings is unconfirmed.
  Base/margin construction and margin CanEnter are implemented; automatic
  tactic search-class selection and group margin requests remain unresolved.
- **G movement integration:** the existing independent integer sine/atan helpers
  supply directions; no original trigonometric tables were copied. Thinks per
  logic tick, attack-state scheduling and vehicle model/initial-heading wiring
  into GameSim remain open. Spider still needs its separate queued-step/arrival
  rules. Aircraft, Bird, boats and Johnka retain their separate existing systems.

An earlier default-promotion experiment, before separating base and margin
member layers, failed seven CTest entries: `path_bench_smoke`,
`zpath_tools`, `engine_unit`, `ground_weapons`, `ai_skirmish`,
`playthrough_framework`, and `pathing_stress`. The stress suite had six faithful
course failures, including stationary allies, opposing traffic and forests. The
AI soak stopped sustained firing in later intervals. These observations reject
promotion; they do not identify which unresolved tactic rule caused each failure,
or demonstrate that the original itself stalls. Existing assertions were kept.

## Benchmark before and candidate after

V engine measurement: `path_bench --json`, all 16 courses, counts 1/10/30,
1490 ticks per order, default modifications, explored terrain. Before: **656/656
arrivals, zero stuck**. Candidate after: **585/656 arrivals, 71 lost, 53 stuck**.
The retained default after the changes still has **656/656 arrivals, zero stuck**;
all non-timing output, including every hash and arrival list, matches the fresh
before run. Runtime milliseconds are host-dependent and are excluded from that
comparison. The candidate remains deterministic despite its failures.

V archived evidence: [original CSV](data/path_benchmark_original.csv) contains
123 rows: 14 `ok`, 105 `missing_units`, four `namespace_denied`. Only the seven
explored `ok` rows support the reference column below. The seven fog rows are
listed separately. Missing/denied rows are failures of measurement, never zero
arrivals or evidence of a movement defect. The [engine CSV](data/path_benchmark_new.csv)
contains 48 successful baseline rows; the fresh before run reproduces their
movement metrics. All 48 archived hashes already differ from the fresh baseline
before this task, so hash preservation is assessed against the fresh run only. These are archived observations, not new reference
verification. G: no numeric tolerance or full parity is inferred from them.

In the table, before and candidate makespans refer only to arrivals observed
inside the horizon. An incomplete candidate makespan is **not** a completion
of all units. Path/time ratios average arrivals only, so losing difficult units
can improve those averages. `A` = arrivals, `S` = stuck units. Reference `-`
means no accepted explored measurement for that course/count. Arrival means
within six tiles of the command goal, not final settlement. The old disc-overlap
metric may count visual proximity in the tile-reserving candidate; ownership
uniqueness is checked separately in synthetic tests.

| Course | N | Reference makespan | Before makespan | Candidate makespan | Candidate A / S | Path ratio before / candidate | Time ratio before / candidate |
|---|---:|---:|---:|---:|---:|---:|---:|
| open | 1 | 363 | 365 | 361 | 1 / 0 | 0.907 / 0.920 | 0.904 / 0.895 |
| open | 10 | 377 | 366 | 415 | 10 / 0 | 0.881 / 0.941 | 0.879 / 0.936 |
| open | 30 | 398 | 378 | 461 | 30 / 0 | 0.880 / 1.001 | 0.885 / 1.013 |
| gap1 | 1 | 363 | 365 | 361 | 1 / 0 | 0.907 / 0.920 | 0.904 / 0.895 |
| gap1 | 10 | 425 | 392 | 469 | 10 / 0 | 0.907 / 0.987 | 0.921 / 0.983 |
| gap1 | 30 | 548 | 468 | 675 | 30 / 0 | 0.928 / 1.134 | 1.009 / 1.248 |
| gap2 | 1 | 363 | 364 | 361 | 1 / 0 | 0.904 / 0.920 | 0.902 / 0.895 |
| gap2 | 10 | - | 375 | 424 | 10 / 0 | 0.888 / 0.956 | 0.890 / 0.955 |
| gap2 | 30 | - | 408 | 522 | 30 / 0 | 0.901 / 1.055 | 0.930 / 1.078 |
| gap4 | 1 | - | 364 | 361 | 1 / 0 | 0.904 / 0.920 | 0.902 / 0.895 |
| gap4 | 10 | - | 371 | 412 | 10 / 0 | 0.882 / 0.941 | 0.885 / 0.935 |
| gap4 | 30 | - | 384 | 481 | 30 / 0 | 0.884 / 1.030 | 0.895 / 1.046 |
| corr1 | 1 | - | 365 | 361 | 1 / 0 | 0.907 / 0.920 | 0.904 / 0.895 |
| corr1 | 10 | - | 444 | 448 | 10 / 0 | 0.934 / 0.977 | 0.976 / 0.974 |
| corr1 | 30 | - | 468 | 778 | 30 / 0 | 0.937 / 1.125 | 1.017 / 1.446 |
| corr2 | 1 | - | 364 | 361 | 1 / 0 | 0.904 / 0.920 | 0.902 / 0.895 |
| corr2 | 10 | - | 374 | 427 | 10 / 0 | 0.887 / 0.958 | 0.890 / 0.960 |
| corr2 | 30 | - | 415 | 562 | 30 / 0 | 0.905 / 1.074 | 0.940 / 1.149 |
| maze | 1 | - | 1313 | 1292 | 1 / 0 | 0.934 / 0.943 | 0.934 / 0.919 |
| maze | 10 | - | 1366 | 1480 | 10 / 0 | 0.947 / 1.009 | 0.951 / 1.000 |
| maze | 30 | - | 1439 | 1488 | 12 / 0 | 0.959 / 1.003 | 0.974 / 1.000 |
| utrap | 1 | - | 339 | 347 | 1 / 0 | 0.871 / 0.916 | 0.869 / 0.889 |
| utrap | 10 | - | 353 | 384 | 10 / 0 | 0.865 / 0.933 | 0.867 / 0.920 |
| utrap | 30 | - | 372 | 437 | 30 / 0 | 0.886 / 0.985 | 0.895 / 0.989 |
| wall | 1 | - | 445 | 465 | 1 / 0 | 0.897 / 0.968 | 0.896 / 0.936 |
| wall | 10 | - | 458 | 476 | 10 / 0 | 0.892 / 0.940 | 0.894 / 0.917 |
| wall | 30 | - | 472 | 559 | 30 / 0 | 0.902 / 0.974 | 0.905 / 0.966 |
| rooms | 1 | - | 398 | 410 | 1 / 0 | 0.881 / 0.934 | 0.880 / 0.906 |
| rooms | 10 | - | 420 | 536 | 10 / 0 | 0.883 / 0.999 | 0.892 / 0.986 |
| rooms | 30 | - | 440 | 626 | 30 / 0 | 0.900 / 1.076 | 0.915 / 1.076 |
| swap | 1 | - | 322 | 320 | 1 / 0 | 0.895 / 0.912 | 0.892 / 0.887 |
| swap | 10 | - | 365 | 394 | 10 / 0 | 0.904 / 0.960 | 0.903 / 0.951 |
| swap | 30 | - | 405 | 453 | 9 / 21 | 0.947 / 1.029 | 0.970 / 1.133 |
| swap1 | 1 | - | 323 | 320 | 1 / 0 | 0.897 / 0.912 | 0.895 / 0.887 |
| swap1 | 10 | - | 434 | 370 | 3 / 7 | 0.912 / 0.971 | 0.942 / 0.962 |
| swap1 | 30 | - | 713 | 425 | 5 / 25 | 1.107 / 0.958 | 1.377 / 1.076 |
| diag | 1 | - | 458 | 452 | 1 / 0 | 0.906 / 0.923 | 0.905 / 0.893 |
| diag | 10 | - | 472 | 500 | 10 / 0 | 0.896 / 0.952 | 0.897 / 0.938 |
| diag | 30 | - | 503 | 557 | 30 / 0 | 0.909 / 1.004 | 0.926 / 1.005 |
| park | 1 | - | 149 | 150 | 1 / 0 | 0.807 / 0.828 | 0.802 / 0.807 |
| park | 10 | - | 374 | 414 | 10 / 0 | 0.857 / 0.918 | 0.856 / 0.913 |
| park | 30 | - | 406 | 482 | 30 / 0 | 0.859 / 0.965 | 0.877 / 0.974 |
| mixed | 1 | - | 365 | 361 | 1 / 0 | 0.907 / 0.920 | 0.904 / 0.895 |
| mixed | 10 | - | 801 | 833 | 10 / 0 | 0.883 / 0.915 | 0.890 / 0.908 |
| mixed | 30 | - | 809 | 1029 | 30 / 0 | 0.889 / 0.977 | 0.905 / 0.995 |
| clusters | 1 | - | 374 | 393 | 1 / 0 | 0.899 / 0.962 | 0.897 / 0.943 |
| clusters | 10 | - | 382 | 456 | 10 / 0 | 0.883 / 1.003 | 0.885 / 1.000 |
| clusters | 30 | - | 399 | 504 | 30 / 0 | 0.885 / 1.047 | 0.894 / 1.062 |

V archived fog observations (all listed units arrived, zero stuck):

| Course | N | Makespan | Mean path ratio | Mean time ratio |
|---|---:|---:|---:|---:|
| gap1 | 10 | 480 | 1.100205 | 1.144824 |
| gap1 | 1 | 437 | 1.108332 | 1.082874 |
| gap1 | 30 | 661 | 1.155623 | 1.577643 |
| gap2 | 1 | 441 | 1.118679 | 1.092786 |
| open | 10 | 422 | 1.021805 | 1.006801 |
| open | 1 | 404 | 1.025367 | 1.001101 |
| open | 30 | 448 | 1.036598 | 1.028029 |

G: the candidate benchmark has no evolving side-fog model. Its explored results
cannot be compared to these fog observations as equivalent inputs. Fog departure
cost, occupancy override and brightness boundary are tested synthetically only.

## Reproduction and validation

Use the independent engine only; these commands never launch the original:

```sh
sh tools/bootstrap.sh --no-hooks
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
mkdir -p work/pathfinding-wave
build/path_bench --json > work/pathfinding-wave/default.jsonl
build/path_bench --wave --json > work/pathfinding-wave/candidate.jsonl
pre-commit run --all-files
pre-commit run --files src/engine/sim/path/wave.cpp src/engine/sim/path/wave.hpp src/engine/sim/path/wave_mover.cpp tests/test_wave.cpp docs/pathfinding_implementation.md
```

`--wave` is explicit and incompatible with `--mod unit_unstick=on`; combining them
returns an error rather than silently benchmarking enhanced disc movement.
Repeat each benchmark, remove only `ms_per_tick`, and compare all remaining
fields. Preserve an old-checkout run for an independent before measurement.
Neither benchmark requires private game data, SDL game execution or a namespace.

V construction checks: the baseline Release build and all 79 baseline CTest
entries passed or skipped as expected (73 passed, six skipped); all pre-commit
hooks passed. The added `wave` test covers the rules and safety gaps above,
including replacement-order reservation ownership and a repeated 600-tick
10-unit bottleneck. The legacy field comparison now names `legacy_astar`
explicitly; the public point-query corner test now requires the original direct
diagonal. The course, golden-hash, AI and playthrough assertions remain intact.
Final verification: Release build and all 80 CTest entries pass or skip as
expected (74 passed, six private-data/network skips). The focused wave test
passes 24,142 checks. All pre-commit hooks pass for tracked and new files.
The final candidate repeats all 48 non-timing benchmark rows exactly; default
metrics and hashes match the fresh pre-change baseline in all 48 cases.
