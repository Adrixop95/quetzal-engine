# Movement, pathfinding and collisions in the original (Aztec Empire) and the plan for a new system

This document describes the original movement stack (from the decompilation `work/decomp/all.c`, additional decompilation of selected addresses in Ghidra and from logs
of the recompiled game), results of quantitative experiments, and the design of the replacement (`src/engine/sim/path/`).
Labels: **[ver]** = verified by experiment or directly in the code, **[hyp]** = hypothesis / unconfirmed.
Machine-readable tables: `docs/data/movement_*.csv`. Raw results: `docs/data/movement_experiments.csv`, `docs/data/movement_bench.csv`.

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
| L80 | `DAT_00515a80` | L78 with adjacency penalty (`9` -> `4` when a neighbour == 0) | 2 | like class 0 + wall margin (not used by Marine; `param_7 & ~2` in `0x46c330`) |
| L7C | `DAT_00515a7c` | L74 with adjacency penalty | 3 | like class 1 + wall margin |
| L84 | `DAT_00515a84` | constant 9 (whole map) | 4, 5 | air: not used by the wave (classes with bit 4 go along a straight line, 1.7) |
| L88 | `DAT_00515a88` | L84 with edges = 4 | 6, 7 | as above |

`FUN_0046c180(ptr, 0)` checks the 8 neighbours of the layer for the value 0. Passability test of a single tile `FUN_0046c330(&(x,y), class)`
(`0x46c330`): for classes without bit 2, `layer[tile] != 0`; for classes with bit 2, `(layer[tile] & 8) >> 3` (only the value 9, i.e. without adjacent tiles).
The unit tile functions (`CanEnter`, vtable `+0x94`) call `FUN_0046c330(.., (byte +0x2c) != 0)`, i.e. class 0 or 1. The layers are NOT
modified during the game (the only writes: `0x46bdb0`) [ver: the only reference to `.dat` in the whole `all.c`]. Destroyed/placed buildings enter only through the occupancy map.

### 1.3 Occupancy map (`Map.cpp`, `DAT_004dd2f8`)
Object of class `Map` (vtable `0x493288`, constructors `0x4319e0` (zeroed), `0x431b10` (from file), `0x431c10` (resize)): `+4` width (byte), `+8` height,
`+0xc` data `u32[w*h]`, `+0x10` array of pointers to rows (`rows[y][x]`). The instance `DAT_004dd2f8` is the **occupancy map**: `0` = free tile,
otherwise a **pointer to an object** (unit/building; the unit constructor writes `this`, `0x433820`/`0x44ab90` move the entry on a step). Values 1-2
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
| `radius` | `-1` = unlimited; otherwise an empty square of radius `radius` tiles with value 0 is drawn around the start, i.e. **search limited** to the square `(2r+1)^2` |

Order of steps: copy the layer to the wave map (`FUN_0046cdc0`: `u16`, stride `w+2`, zero frame; `fog<8 -> 10`, otherwise the layer value), optionally apply occupancy,
clip coordinates, fix the start, apply the limit square, set the goal mark(s) `0xf`, run the wave `FUN_0046d330`; if the goal is unreachable choose the
nearest visited tile (`FUN_0046ce80(.., 0xfff0)`: Chebyshev rings around the goal, selects the smallest wave number in the first ring with any visited tile),
extract the path (`FUN_0046ccb0`). Counter `DAT_00515a70` counts queries in the tick.

### 1.5 Wave (`FUN_0046d330` + `FUN_0046d4d0`)
- Wave cell `u16`: low nibble = layer value (0 block, 4, 9, 10, `0xf` goal), higher bits = wave number (`0x20 + 0x10*k`), bit `0x10` = start.
- BFS **from the start** in 8 directions (no corner-cutting ban), in "rounds"; queue element `(x, y, delay)`; a newly visited cell gets the delay
  `value & 7` (9 -> 1, 10 -> 2, 4 -> 4): a cell adjacent to a wall "costs" 4 rounds, unknown (fog) 2, normal 1. **Metric = Chebyshev**
  (a diagonal costs the same as a straight step), not Euclidean; hence paths of length up to ~1.2x optimum and "stair-step" tracks.
- Queues 2 x 24000 B (2000 front elements); ends when the front touches a cell with nibble `0xf` or the front is empty (goal unreachable).
- Extraction (`FUN_0046ccb0`): from the goal backwards through neighbours with wave number 1 lower; ties are resolved by `FUN_0046d1c0`, preferring continuation of the previous direction
  (direction table `DAT_004bcc28`). Result reversed (`FUN_004820a0`).
- `FUN_0046d650` (`0x46d650`) is a plain `a != b`; `Heapsort.cpp` (`0x473030`) does not take part in the wave (sorting is in other modules).

### 1.6 Query limit per tick (`FUN_0046c410`, `0x46c410`)
Two slots `DAT_00515a68/6c` ("granted in this tick") and two positions of the waiting queue (`DAT_00515a58/5c` attempt counters, `DAT_00515a60/64` units). Start of the tick
(`0x4577b0`) copies the waiting ones to granted and zeroes the counter `DAT_00515a70`. Effect: **max 2 path searches per tick** (24/s at 12 Hz); a unit that did not
get a slot receives code 6 (retry next tick) and increments its attempt counter; the two most "starved" have priority in the next tick. [ver: the `RT_PATH_LOG` log
shows exactly 2 queries in consecutive ticks during a jam]

### 1.7 Straight track (`param_7 & 4`)
For flying units (vtable method `+0xa4` != 0, class `| 4`): no wave; points about every 1 px along a straight line in fixed-point 24.8 (Bresenham-like with `FUN_004047e0/f0`
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
| Truck (kind 4, `0x460090`), Headtruck (kind 6, `0x42bda0`) | tile step like Marine (`0x4604a0`, `0x42c430`) | tile occupancy, class 0 (`0x44bc80`) | vehicles: slower turning (animation 8/16 dir.) [hyp] |
| Bird (kind 5), Zeppelin (kind 3) | `0x407820`/`0x4716f0`: target = centre of the tile in 24.8, flight in a straight line | **no occupancy** (`CanEnter` = map bounds only) | straight track (1.7) |
| Boat (kind 8), Johnka (kind 10) | `+0xcc` = stub; scripted movement (waypoints from `[Boat#]`/`[Johnka#]`, `Boat.cpp` `0x408770`, `Johnka` `0x42dce0`) | outside the wave | not controlled by the player; **not checked in detail** [hyp] |
| Defence/Cannon (kind 7/9) | immobile | occupy tiles like buildings | |

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
- **There is no pushing** of units nor mutual avoidance in the continuous sense; conflict = waiting / 45-degree turn / re-planning limited to the 13x13 square.


## 4. Layer "how it works in brief" (model to reproduce the behaviour)
1. Group move order -> **one** group path (Chebyshev wave over the terrain, without units, with fog forcing "passable" on unexplored tiles).
2. Members walk in tile steps (8 directions, one tile occupied at a time) behind the formation anchor; conflict = 45-degree turn / waiting / local
   re-planning limited to 13x13 tiles (max 2 such queries per tick in the whole game).
3. End of move = radius of 3 tiles from the target + "arrived" units block the following ones (hence the irregular cluster of units around the target).
4. Missing: continuous collision avoidance, pushing, movement in fractions of a tile, path hierarchy, memory beyond 8 steps.

