# Movement, pathfinding and collisions in the original (Aztec Empire) and the plan for a new system

This document describes the original movement stack (from the decompilation `work/decomp/all.c`, additional decompilation of selected addresses in Ghidra and from logs
of the recompiled game), results of quantitative experiments, and the design of the replacement (`src/engine/sim/path/`).
Labels: **[ver]** = directly supported by inspected code or identified logs; **[ver-code]** and **[ver-log]** distinguish those sources where needed; **[hyp]** = unconfirmed interpretation. Code inspection is not an experiment.
The 2026-10-26 audit used the read-only local `work/decomp/all.c` and supplemental `work/path/dec1.c` (Marine/Spider steps) from `/home/adrox/aztec-re/`. Only behavioral descriptions and statistics belong in this repository.
Machine-readable results: [original](data/path_benchmark_original.csv) and [replacement](data/path_benchmark_new.csv). Private inputs and raw outputs stay under ignored `work/path-benchmark/`.

## 1. Passability map and layers of `WaveSearch.cpp`

### 1.1 Input data
- `DAT_004c4d0c` = bytes of the mission `.dat` file (`w*h`, 1 byte per 48 px tile). Values: `0` water/wall, `1` obstacle passable only for small
  units (scattered single tiles on land), `2` open land. [ver: this is how the layer construction works, see below]
- `DAT_00515a54` = width, `DAT_00515a50` = height of the map (from the methods `+4/+8` of the map object `DAT_00515428`).
- `FUN_0046bb70` (init, called when a map is loaded, `0x46bb70`): allocates the wave map `DAT_00515a8c` (`(w+2)*(h+2)` fields `u16`, zeroed)
  and 6 byte layers `w*h`: `DAT_00515a74/78/7c/80/84/88`, then calls `FUN_0046bdb0`. Zeroes the query limit counters `DAT_00515a58..6c`.

### 1.2 Layers (`FUN_0046bdb0`, `0x46bdb0`)
Value of a layer cell: `0` = **impassable**, `9` = passable, `4` = passable but adjacent (8 neighbours) to an impassable cell or on the map edge
(slower in the wave, see 1.5). Types:

| layer | address | from `.dat` | class `param_7` | meaning |
|---|---|---|---|---|
| L78 | `DAT_00515a78` | `dat>=2` -> 9, otherwise 0 | 0 | **large** units (knight, bizon, snowman, elephant; bit `+0x2c` of the descriptor = 0) |
| L74 | `DAT_00515a74` | `dat>=1` -> 9, `0` -> 0 | 1 | **small** units (footman, archer, bear, jaguar...; descriptor byte `+0x2c` != 0) |
| L80 | `DAT_00515a80` | L78 with adjacency penalty (`9` -> `4` when a neighbour == 0) | 2 | like class 0 + wall margin (not used by Marine; `class & ~2` in `0x46c330`) |
| L7C | `DAT_00515a7c` | L74 with adjacency penalty | 3 | like class 1 + wall margin |
| L84 | `DAT_00515a84` | constant 9 (whole map) | 4, 5 | air: not used by the wave (classes with bit 4 go along a straight line, 1.7) |
| L88 | `DAT_00515a88` | L84 with edges = 4 | 6, 7 | as above |

`FUN_0046c180(ptr, 0)` checks the 8 neighbours of the layer for the value 0. Passability test of a single tile `FUN_0046c330(&(x,y), class)`
(`0x46c330`): for classes without bit 2, `layer[tile] != 0`; for classes with bit 2, `(layer[tile] & 8) >> 3` (only the value 9, i.e. without adjacent tiles).
The unit tile functions (`CanEnter`, vtable `+0x94`) call `FUN_0046c330(.., (byte +0x2c) != 0)`, i.e. class 0 or 1. The inspected layer construction writes these arrays in `0x46bdb0`; the query copies them rather than mutating them. Building occupancy is applied separately [ver-code]. A claim about every possible terrain mutation would require a complete write-reference audit [hyp].

### 1.3 Occupancy map (`Map.cpp`, `DAT_004dd2f8`)
Object of class `Map` (vtable `0x493288`, constructors `0x4319e0` (zeroed), `0x431b10` (from file), `0x431c10` (resize)): `+4` width (byte), `+8` height,
`+0xc` data `u32[w*h]`, `+0x10` array of pointers to rows (`rows[y][x]`). The instance `DAT_004dd2f8` is the **occupancy map**: `0` = free tile,
ordinary occupied cells store a **pointer to an object** (unit/building; the unit constructor writes `this`, `0x433820`/`0x44ab90` move the entry on a step). Values 1-2
and >2 (`FUN_0043eac0` on contact with a cell `>2` calls the neighbour's vtable method `+0x68`) [hyp: 1/2 are constant obstacles; see 6.1].
Occupancy is **tile-based**: a unit occupies exactly one tile (the target tile of the step, see 2.1), the source tile is released at the start of the step.

### 1.4 Parameters of `wsr_WaveSearch` (`FUN_0046c470`, `0x46c470`, cdecl, 10 arguments)
`int WaveSearch(sx, sy, gx, gy, out, fog, cls, flag, goals, radius)`; returns the number of path points (0 = start == goal or start on the goal list).

| arg | meaning |
|---|---|
| `sx,sy` | start tile (clipped to the map; if the start cell = 0 the nearest passable tile is searched `FUN_0046ce80(.., 0xffff)`) |
| `gx,gy` | goal tile (clipped to the map), "single goal" mode when `goals == 0` |
| `out` | result buffer: array of triples `(x, y, z)` (3 x int32); `z = 3` normal tile (layer value 9/10), `z = 1` tile adjacent to a wall (value 4) |
| `fog` | pointer to the side's visibility map object (`Side+0x690`, bytes from `+0x14`, stride `w+1`); cells with value `< 8` ("unexplored") **enter the wave as 10 regardless of terrain** (see 1.5, 6.2) |
| `cls` | movement class (1.2). `cls & 4` -> straight line without a wave (1.7) |
| `flag` (byte `param_8`) | `0` = take the occupancy map into account (`FUN_0046cc40`: every non-zero occupancy cell zeroes the wave cell, unless it has value 10); `!= 0` = ignore units/buildings, terrain only |
| `goals` | optional goal list (node list of `x,y` pairs): all its cells get the goal mark `0xf`; if the start lies on the list, 0 is returned |
| `radius` | `-1` = no artificial boundary; otherwise **only the perimeter** at `sx +/- radius`, `sy +/- radius` is zeroed, clipped to map bounds. Goals are marked afterwards and can reopen perimeter cells; this is not a strict bounding-box check |

Order of steps: copy the layer to the wave map (`FUN_0046cdc0`: `u16`, stride `w+2`, zero frame; `fog<8 -> 10`, otherwise the layer value), optionally apply occupancy,
clip coordinates, fix the start, apply the limit square, set the goal mark(s) `0xf`, run the wave `FUN_0046d330`; if the goal is unreachable choose the
nearest visited tile (`FUN_0046ce80(.., 0xfff0)`: Chebyshev rings around the goal, selects the smallest wave number in the first ring with any visited tile),
extract the path (`FUN_0046ccb0`). Counter `DAT_00515a70` counts queries in the tick.

### 1.5 Wave (`FUN_0046d330` + `FUN_0046d4d0`)
- Wave cell `u16`: low nibble = layer value (0 block, 4, 9, 10, `0xf` goal), higher bits = wave number (`0x20 + 0x10*k`), bit `0x10` = start.
- BFS **from the start** in 8 directions (no corner-cutting ban), in "rounds"; queue element `(x, y, delay)`; a newly visited cell gets the delay
  `value & 7` (9 -> 1, 10 -> 2, 4 -> 4): a cell adjacent to a wall "costs" 4 rounds, unknown (fog) 2, normal 1. **Metric = Chebyshev**
  (a diagonal costs the same as a straight step), not Euclidean. Cells are marked on first discovery, with no later relaxation, so this is not a general weighted shortest-path solver. There is no established 1.2x bound [ver-code].
- Queues 2 x 24000 B (2000 front elements); ends when the front touches a cell with nibble `0xf` or the front is empty (goal unreachable).
- Extraction (`FUN_0046ccb0`): from the goal backwards, requesting the preceding wave number. `FUN_0046d1c0` can select an even smaller nonzero label; its direction tie rules include direction continuity and special multi-way cases (`DAT_004bcc28`). Points are written backwards into their final order; `FUN_004820a0` compacts an unused prefix, rather than reversing the path [ver-code].
- `FUN_0046d650` (`0x46d650`) is a plain `a != b`; `Heapsort.cpp` (`0x473030`) does not take part in the wave (sorting is in other modules).

### 1.6 Query limit per tick (`FUN_0046c410`, `0x46c410`)
Two slots `DAT_00515a68/6c` ("granted in this tick") and two positions of the waiting queue (`DAT_00515a58/5c` attempt counters, `DAT_00515a60/64` units). Start of the tick
(`0x4577b0`) copies the waiting ones to granted and zeroes the counter `DAT_00515a70`. Effect: **two requester identities admitted per tick**; a repeated request from an already admitted identity is allowed. This is not a hard two-call budget inside `WaveSearch` itself [ver-code]. A requester that did not
get a slot receives code 6 (retry next tick) and increments its attempt counter; the two most "starved" have priority in the next tick. [ver-log: archived `work/zp/m/gap1_30_explored/path.txt`, frames 348 onward, includes pairs of local queries; that observation alone does not establish a universal call limit]

### 1.7 Straight track (`param_7 & 4`)
For flying units (vtable method `+0xa4` != 0, class `| 4`): no wave; points about every tile along a straight line in fixed-point tile coordinates (8 fractional bits) (Bresenham-like with `FUN_004047e0/f0`
operations on constant numbers), `z = 3` at each point, last point = goal. Number of points = `max(|dx|,|dy|)` tiles + 1 (step = 1 tile on the main axis).

## 2. Unit movement

### 2.1 Marine (`Marine.cpp`, `0x433190`, vtable `0x4932cc`) [ver]
- State `+0x64`: 0 dead/inactive, 1 rest, 2 march, 3 attack, 4 dying, 5 rest animation.
- Position: `+0x6c/+0x70` = x,y in 24.8 (px*256), `+0x24/+0x28` px, `+0x44/+0x48` tile. Step vector `+0x98/+0x9c` (24.8), speed = type record dword 5 (px/tick).
- **Tile step** `0x433820` (vtable `+0xcc`, `StepTo(tile)`): target = centre of the tile `(x*48+24, y*48+24)`; direction 0..255 from `FUN_00461ac0` (atan2), animation
  `(angle+16)>>5` = 8 directions; vector = `normalize(target - position) * speed`; state := 2; **occupancy entry of the source tile = 0, of the target tile = this** (immediately),
  `+0x44/48 := target`; notification of the group (group vtable `+0x124`).
- `Think` (vtable `+0x60`, `0x433db0`) in state 2: every tick `position += vector`; when the distance to the target is <= speed -> snap to the tile centre, state 1 and **ask the group for the
  next step** (group vtable `+0x11c`); when near the target (<= 3 * speed) and there is an attack target - switch to attack. The unit **does not turn gradually** (direction change is immediate,
  speed constant, no acceleration).
- `CanEnter(tile)` (vtable `+0x94`, `0x4347f0`): -1 when outside the map, `occupancy != 0` or `!FUN_0046c330(tile, class)`, otherwise 0.
- Flags returned by the unit: `+0xa0` (vtable) = byte of record `+0x2c` (small/large, wave class 1/0), `+0xa4` = flight (Marine: 0).
- Speeds (px/tick, type record dword 5): footman 9, rifleman 6, demolisher 7, bear 5, knight 15, archer 8, mage 5, tuzem 10, jaguar 16, bizon 12, ninja 11, hypno 10, chief 10,
  snowman 4, elefant 4, abdula 10. [ver: footman 9 px/tick measured in the log; the rest from type records, `docs/type_stats.md`]

### 2.2 Other classes
| class | step / movement | collisions | notes |
|---|---|---|---|
| Spider (kind 1, `0x44a350`) | `0x44ab90`: direction quantized to 8 (`byte +0x91 & 0xe0`), speed = record `+0x1c` (24.8), state 2 | same tile occupancy, `CanEnter` always class 0 | waits for the step when `+0x64==2` (remembers the target `+0x88/+0x8c`) |
| Truck (kind 4, `0x460090`), Headtruck (kind 6, `0x42bda0`) | tile step like Marine (`0x4604a0`, `0x42c430`) | tile occupancy, class 0 (`0x44bc80`) | heading approaches the desired angle by 8/256 turn per tick (snaps when error <8); Headtruck also adjusts its other heading (`0x42c880`, `0x460a10`) [ver-code]; these are not Marine-style instantaneous turns |
| Bird (kind 5), Zeppelin (kind 3) | `0x407820`/`0x4716f0`: target = centre of the tile in 24.8; wave waypoints form a straight track | **no occupancy** (`CanEnter` = map bounds only) | straight track (1.7) |
| Boat (kind 8), Johnka (kind 10) | `+0xcc` = stub; scripted movement (waypoints from `[Boat#]`/`[Johnka#]`, `Boat.cpp` `0x408770`, `Johnka` `0x42dce0`) | outside the wave | not controlled by the player; **not checked in detail** [hyp] |
| Building (kind 2) | fixed footprint | occupancy blocks the ground search when `flag=0`; terrain layers remain separate | [ver-code]; footprint dimensions depend on type |
| Defence/Cannon (kind 7/9) | treated as stationary in the existing analysis | ground blockers | full kind-9 movement/footprint audit remains open [hyp]; UGI cannon id 4 is actually kind 6, not kind 9 |
| Other (kind 11) | no general player movement contract established | unknown | excluded from mobile benchmarks [hyp] |

## 3. Groups: `Tactic.cpp` (vtable `0x48fbd4`, 83 methods) [partly ver]
Each unit has an owner (`+0x30` = group object, "Tactic"); the player also commands groups: `Side+0x688` = currently selected group. Commands
(`PCT_FRAME_*`, `0x4577b0`): `CREATEGROUP` `0x15` (`0x448b40`: selection by rectangle `d5..d8` = x1,y1,x2,y2 in pixels), `SELECTGROUP` `0x16` (click at point `d5,d6`),
`ATTACK` `0x18` (`0x447e00`: **right click** = move/attack order: `d5,d6` pixels, `d7` flags; no target under the cursor -> `group->vtable[0xd8](tile = px/48)`),
`FOLLOWTEAM` `0x24` (`0x448210`), `SELECTBYNO` `0x23` (`0x448f60`). There is no separate `PCT_FRAME_MOVE` (0x17) in the tick switch [ver: no `case 0x17`].

### 3.1 Course of a move order [ver in logs]
1. `group->0xd8` (`0x453360`) clears the list of path points, appends the target tile, calls `0x453920` (`SetGoal`).
2. `0x453920`: query limit (1.6; no slot -> waiting flag `+0xb8`, attempt in the next tick); then **one** `WaveSearch(group position, goal, out=group buffer, fog=Side+0x690,
   cls = group field +0x50, flag=1 (ignore units), goals=0, radius=-1)`. Result: the **group** path (formation anchor).
3. Members do not search for paths themselves. Each moves towards `anchor + formation_offset` (`0x455b10`, vtable `+0x44`) in small tile steps along the direction (8 dir., `0x453ac0` + `0x453e20`).
   The anchor moves along the group path only when the units are close enough (coherence table `{4,8,8,6}` tiles by group state, `0x4516b0`).
4. The move is considered finished when the unit is within 144 px (3 tiles) of the target (`0x450910` marks units "arrived" with flag `+0x5f` and the group with flag `+0xd9`); further units stop
   behind the arrived ones (code 3 in `TryStep`).

### 3.2 Unit step and jam (`0x453ac0`, `0x453b60`, `0x453be0`, `0x453e20`)
- The member path is **max 8 steps** stored as nibbles `direction | (z==3)<<3` (`FUN_0044ff60`, read `FUN_004518c0`): after 8 steps it must plan the next segment.
- `0x453b60` (`FollowPath`): takes the next direction (table `DAT_004bc81c`: N, NE, E, SE, S, SW, W, NW), `TryMove`; when the segment is exhausted returns 4 (new planning needed).
- `0x453be0` (`PlanPath`, member plan): query limit (code 6 = wait), releases its own occupancy cell, calls `WaveSearch(position, goal/goal list, out=DAT_004d6c68, fog,
  cls = (flag+0xa0 != 0) | (flight ? 4 : 0), flag=0 (**take occupancy into account**), goals, radius=6)`, restores the cell, packs <= 8 steps; result < 2 -> code 5 (no path).
  In the logs: queries `rad=6` with `flag=0` during jams, `flag=1,rad=-1` for group paths.
- `0x453e20` (`TryStep(delta)`): target tile occupied/blocked -> returns 1 or tries to turn the direction by +-45 degrees (`FUN_00462310`/`FUN_004622e0`: rotate the direction by 1 position) at most 2 times;
  when the occupant is a member of the same group already in position (flag `+0x5f`) -> code 3 (stand still); with codes 1/2 the group recalculates the formation (`0x450910`).
- **There is no pushing** of units nor mutual avoidance in the continuous sense; conflict = waiting / 45-degree turn / re-planning with the radius-6 boundary described in 1.4.


## 4. Layer "how it works in brief" (model to reproduce the behaviour)
1. Group move order -> **one** group path (Chebyshev wave over the terrain, without units, with fog forcing "passable" on unexplored tiles).
2. Members walk in tile steps (8 directions, one tile occupied at a time) behind the formation anchor; conflict = 45-degree turn / waiting / local
   re-planning with a radius-6 perimeter and two admitted requester identities per tick.
3. End of move = radius of 3 tiles from the target + "arrived" units block the following ones (hence the irregular cluster of units around the target).
4. Missing from this ground-unit movement model: continuous collision avoidance and pushing. Pixel positions do move fractionally each tick; exclusive tile ownership is reserved at step start. Member path segments store at most 8 steps.

## 5. Benchmark protocol and results (2026-10-26)

The course definitions are shared in intent between `make_zpath.py` and `courses.cpp`; `zpath_tools` checks their terrain grids, starts and goals against each other without assets. Courses use 90x60 tiles, with 1, 10 or 30 units; `mixed` varies Marine speed and terrain class. `clusters` adds four 6x6 building-sized terrain blocks. These are **footprint proxies**, not placed building objects, and do not establish live-building collision behavior. `rooms` has a large pillar and two doors; `corr1` is a 21-tile-long, one-tile-wide corridor; `gap1` is a three-tile-thick bottleneck.

Both analyzers count first entry into a disk of radius six tiles (288 px) around each group goal, with a budget of **1490 ticks per group order**. This broad arrival criterion permits comparison with the original formation; it does not mean every replacement agent has settled in its assigned slot. `test_path` separately checks settled states on the courses. A lost unit has not entered that disk by the budget; a stuck unit is lost and has an idle tail of at least 120 observed ticks. Tile revisits measure oscillation only approximately: a legitimate detour can revisit a tile too.

Distance and idle/revisit statistics stop at first arrival. Distance ratio divides traveled pixels by the first declared unit's obstacle-aware goal-centre octile route for its group (48 px axial, approximately 68 px diagonal, no corner cutting). Time ratio divides elapsed ticks by that route length / the unit type's speed. These are **group centre-route baselines**, not per-agent continuous geometric optima; the starting formation differs by several tiles, and arrival stops 288 px short. Ratios below one are expected and do not prove superoptimal movement. The replacement uses 10/14 search costs; the Python baseline uses 48/68. Neither accounts for crowd delays in the ideal. Absolute ticks, completion counts and revisits are the primary comparison metrics.

### 5.1 Original observations and unavailable coverage

New reference attempts used `/home/adrox/aztec-re/build/aztec_recomp`, private game directories with resource/executable symlinks, dummy SDL video/audio drivers, and `unshare -rn`. All four attempts (`gap1`, `corr1`, `rooms`, `clusters`, 30 units, explored) failed at namespace interface configuration with `Cannot open netlink socket: Operation not permitted`. No original process was launched outside the requested namespace. The CSV records these as `namespace_denied` with blank movement metrics, not as lost units.

An audit of existing read-only logs under `/home/adrox/aztec-re/work/zp/{m,m2}` recovered **14 usable archived runs** from 2026-10-20: open and gap1 at 1/10/30 units in both fog modes, plus gap2 at one unit in both modes. Their generated terrain files match the current generator, all intended units can be matched by type within two tiles of their declared initial positions before orders, and path logs contain group queries to the declared goals. Formation initialization can shift initial positions; the matching bound accommodates that without accepting distant campaign units. The CSV includes raw position-log SHA-256 hashes and source directories for reproduction.

The other **105 archived runs are rejected as `missing_units`**: their logged initial units do not match the course manifest. In particular, the m2 runs contain campaign units around tiles such as (36,42) rather than the declared start around (6,26). The previous analyzer's nine-tile matching tolerance and denominator based only on matched units could produce misleading rates. Missing units now invalidate aggregate metrics. No corridor, maze, opposing-traffic or building-cluster original loss rate is established by these archives. The open/gap1 accepted runs all complete: observed lost/stuck rate **0%** (one run per configuration, not a statistical population estimate).

### 5.2 Short comparison (30 units)

Each entry is arrived / makespan in ticks / maximum idle ticks before arrival. Entries without original measurements remain open work; the replacement cannot establish original loss rates for those courses. Original entries are reanalyzed archived observations; replacement entries were run in this worktree. A dash means unmeasured, never zero failures.

| Course | Original fog | Original explored | Replacement | Replacement mean time ratio | Replacement mean revisits |
|---|---|---|---|---:|---:|
| Open | 30/30 / 448 / 12 | 30/30 / 398 / 12 | 30/30 / 378 / 2 | 0.885 | 0.03 |
| One-tile bottleneck | 30/30 / 661 / 92 | 30/30 / 548 / 99 | 30/30 / 468 / 6 | 1.009 | 2.87 |
| One-tile corridor | — | — | 30/30 / 468 / 3 | 1.017 | 4.03 |
| Rooms and pillar | — | — | 30/30 / 440 / 3 | 0.915 | 0.47 |
| Building-sized clusters | — | — | 30/30 / 399 / 9 | 0.894 | 0.40 |
| Opposing one-tile traffic | — | — | 30/30 / 713 / 10 | 1.377 | 12.83 |
| Serpentine maze | — | — | 30/30 / 1439 / 2 | 0.974 | 3.10 |

The replacement completes all **48 configurations (16 courses x 3 counts)** within the budget. At 12-tick sampling intervals, the 30-unit swap1 run recorded 27 pairs with squared distance below 60% of the squared sum of nominal radii; other runs recorded zero. Temporary squeezing deliberately reduces effective radii, so nominal-radius overlaps can occur. Final settlement tests separately reject deeply overlapping final pairs. The one-tile opposing-traffic course remains the clearest oscillation/crowding weakness. The shared-field correction reduced 30-unit gap1 makespan from 489 to 468 ticks and mean revisits from 5.33 to 2.87; open-field revisits fell from 2.60 to 0.03. It is not a uniform speedup: the maze changed from 1424 to 1439 ticks and swap1 from 689 to 713. Those trade-offs are retained because following the field now respects complete edge costs and all course settlement tests pass. The new random-grid check verifies that following the field has the same weighted cost as A*; the old test checked only decreasing labels.

### 5.3 Reproduction

Use a fresh output directory on every reference invocation. `zpath_run.sh`, `zpath_one.sh` and `zpath_matrix.sh` propagate failures; retries must use new directories. Game assets, logs, maps and any executable-derived files remain ignored local data.

```sh
CCACHE_DISABLE=1 cmake -S . -B build_movement -DCMAKE_BUILD_TYPE=Release
CCACHE_DISABLE=1 cmake --build build_movement -j 4
ctest --test-dir build_movement --output-on-failure
mkdir -p work/path-benchmark
build_movement/path_bench --json --n 1,10,30 --ticks 1490 > work/path-benchmark/new.jsonl
ZPATH_GAME_DIR=/home/adrox/aztec-re/game \
ZPATH_RUNTIME=/home/adrox/aztec-re/build/aztec_recomp \
  sh tools/zpath_run.sh corr1 30 explored work/path-benchmark/fresh-corr1 150
python3 -I tools/zpath_analyze.py work/path-benchmark/fresh-corr1 --ticks 1490
```

To reanalyze an archive, call `zpath_analyze.py ARCHIVE --ticks 1490` and save its summary under an ignored directory, one `summary.json` per run. Exit 1 indicates an invalid observation. `zpath_compare.py SUMMARY_ROOT NEW_JSONL OUT_PREFIX` writes `_original.csv`, `_new.csv` and `_compare.md`; invalid reference rows retain their status and have no invented performance metrics. This comparison requires equal tick budgets. Final verification in this worktree: build passed; six data-independent ctest targets passed, including the new benchmark and analyzer checks. With the read-only game directory, `formats` also passed; the broader data-dependent `engine` test was stopped after more than six minutes without completing. Its result is unresolved. Do not combine a historical summary produced by the old analyzer with these measurements.

## 6. Remaining uncertainties

### 6.1 Occupancy and mobile kinds

The meanings of sentinel occupancy values 1 and 2, complete per-type building footprints, moving/constructed/destroyed building updates, Boat/Johnka collisions, and kind-9/11 behavior remain unverified. The benchmark covers Marine types; Spider, vehicle and air semantics above are code observations, not course measurements. The module currently supplies ground movement, not kind-specific vehicle turning or scripted water movement.

### 6.2 Visibility and search boundaries

`fog < 8` forcing wave value 10 and occupancy preserving that value are verified branches. The exact game interpretation of visibility-byte ranges, and whether a custom explored `.vis` stays fully explored during a run, need direct fog-map dumps. A file labeled `explored` describes the generated input, not proof of all runtime visibility bytes. Zeroing the radius perimeter before marking goals can leave exits through goal-list cells; a strict 13x13 search restriction is therefore an approximation. First-discovery wave labels and extraction tie cases have not been exhaustively differential-tested against all inputs.

## 7. Design of the new system (`src/engine/sim/path/`)
Goal: keep the "feel" (speed px/tick per type, 12 Hz step, 8 animation directions, groups move together, formation reaches a cluster around the target), remove the
local planning and occupancy constraints described above. Everything in integers (positions 24.8 px, angles 0..255 as in the original), no `float`, no dependencies.

| element | files | description |
|---|---|---|
| maths | `fixed.hpp` | `isqrt` (exact), `sin/cos` table of 65 values, `angle_of` (binary search over cross products), `rotate`, `facing8` (same as the original: 0 = N, clockwise) |
| grid | `navgrid.hpp/.cpp` | `.dat` bytes (0/1/2) -> classes `Small` (dat>=1) and `Large` (dat>=2) like layers L74/L78; dynamic building blocks with reference counter; disc vs tile test (`disc_hits`), pushing a disc out of walls (`push_out`), free segment for a disc (`segment_clear`) |
| flow field | `flowfield.hpp/.cpp` | integer Dijkstra (Dial buckets, costs 10/14, penalty 4 next to walls), 8 directions **without corner-cutting**, one field per order/class instead of N queries; any number of goals; steering minimizes edge cost plus remaining field cost |
| A* | `astar.hpp/.cpp` | point-to-point, octile metric, "string pulling" smoothing with segments free for a disc; for AI and single units |
| group movement | `mover.hpp/.cpp` | `MoveSystem`: agents (disc of radius 18 px, speed, class), `order_move` (field + slots), `tick` |
| scenarios | `courses.hpp/.cpp` | the same obstacle courses as `tools/make_zpath.py` (for tests and benchmark) |

### 7.1 Move order (`order_move`)
1. One flow field from the goal tile (goal blocked -> nearest free; goal unreachable for the first agent -> flood field from the agent and selection of the reachable tile nearest to the goal,
   i.e. what the original did only after the failed wave `FUN_0046ce80`).
2. **Target slots**: hexagonal grid of points around the goal (spacing `2r + 4 px`), only reachable and without collision with a wall, sorted from the goal; agents and slots are sorted along the direction
   of approach and paired (the front of the group takes the front slots), so nobody waits on an occupied goal point and paths do not cross.
3. No original-style requester quota: a field is shared per order/class and rebuilt when the grid version changes. Benchmark timings include steering and collision handling, not just field construction.

### 7.2 Tick (`MoveSystem::tick`), in id order (deterministic)
1. Spatial hashing (cell 96 px, counting sort).
2. **Preferred velocity**: visible slot (segment free for the disc, <=16 tiles) -> go straight, otherwise follow the field gradient with a "jump": up to 10 consecutive field tiles, the
   furthest visible one is selected; the steering target is buffered for 3-6 ticks. Landing exactly on the slot (no overshoot).
3. **Local avoidance** (velocity sampling): 13 angles (+-0..64 units out of 256) x 3 speeds (100%, 50%, stop); penalty for deviation, direction hysteresis, wall, and the predicted time of collision with each
   neighbour within a horizon of 8 ticks (quadratic equation in integers, `isqrt`); standing neighbours count less (they can be pushed), "hold" and moving ones more heavily. A tie is resolved by the first
   candidate; the order favours a right turn over a left one, so head-on meetings pass each other like right-hand traffic.
4. Position integration, then **overlap resolution** (3 iterations): a pair of discs is pushed apart along the normal; a moving one pushes a standing one (correction share 48/208 of 256), two moving ones equally,
   a standing one with flag `hold` does not give way; after each iteration pushing out of walls. At the end an agent never ends inside a wall (revert to the previous position).
5. **Jam**: window of 24 ticks; movement < 25% of expected -> jam level +1: level>=1 disables avoidance for the window (contact physics stays), >=3 lateral push of direction, >=4 withdrawal of lower priority
   (parity of id + window; breaks head-on collision in a narrow passage), >=5 reduction of radius to 70% for 48 ticks ("squeezing"); a unit close to a slot whose neighbours already stand there occupies
   the place where it is. The level drops with progress.
6. Arrival: distance to the slot <= 2 px -> state `Arrived` (stands, but can be pushed by passers-by).

### 7.3 Design decisions and trade-offs
- *Why a flow field and not A\* per unit*: 30 units = one field; no original requester starvation (original: two admitted identities/tick) and stable, explicit id ordering. For large maps (255x255) HPA* (16x16 clusters) could be added
  as the first level of the field; **not done**, because the current course maps do not establish a need for that extra level.
- *Why velocity sampling and not ORCA*: ORCA requires linear programming and square roots in the loop; sampling with `isqrt` is simple, integer and fully deterministic
  (no dependence on the iteration order beyond id). The checked course matrix covers 1, 10 and 30 units. No ORCA comparison or thousand-unit scaling result has been established.
- *Preserving the "feel"*: speeds from type records (px/tick), no acceleration, instant turning (animation direction quantized to 8 as in the original). Two differences are **deliberate**: continuous positions with disc contact instead of
  exclusive tile ownership (contact resolution and temporary squeezing can let opposing units pass in a one-tile corridor; radius-18 discs do not both fit side by side at full size) and smooth stopping in the slot instead of the "3 tile radius".
- *Unit class*: `Small`/`Large` corresponds to bit `+0x2c` of the type record (knight, bizon, snowman, elephant are `Large`). Classes 2/3 (with wall margin) and flying (straight track) of the original are not needed:
  the margin is given by the wall penalty. The current `MoveSystem` exposes only ground Small/Large classes. Flying movement has not been implemented in this module.
- *Lockstep determinism*: no `float`, no `std::unordered_*`, no dependence on pointers; sorts have full keys (ties by id/coordinates). `MoveSystem::state_hash` for network comparisons;
  test `test_path` is the reference for the hash value.
- *Limits*: no tracking of moving targets (follow/attack-move), formations of a given shape (only the order relative to the direction of approach is kept), vehicles with a turning radius, water units.
