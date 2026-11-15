# Path finding ("wave") and unit movement of the original

Decompilation read only, nothing was run. V = read in the Ghidra output (or computed directly
from it / from table bytes in Aztec.exe), G = guess or interpretation. Sources:
`work/decomp/all_full.c`, `work/gaps/dec_00453e20.c`, `dec_00455b10.c`, table bytes at
0x4bcc28, 0x4bc81c, 0x4bc838, 0x4bc84c. Complements `docs/rules_movement.md` (sections 1-3) and
`work/gaps/group_formation_leash.md`; only additions, corrections and exact rules are given.

## 1. Wave search 0x46c470

### 1.1 Data
- V 0x46bb70/0x46cdc0: wave map 0x515a8c, u16 cells, (w+2)*(h+2), one cell zero frame. Cell =
  low nibble terrain value, bits 4..15 label. Layers (bytes, w*h): 0 blocked, 9 free, 4 free
  but next to a blocked tile; fog byte < 8 forces value 0xa (10) whatever the terrain.
- V 0x46c470 class switch: 0 -> 0x515a78 (large), 1 -> 0x515a74 (small), 2 -> 0x515a80,
  3 -> 0x515a7c (margin variants), class & 4 -> no wave, straight line (1.6).
- V 0x46cc40 (only when arg 8 == 0): every tile with a non-zero occupancy entry (0x4dd2f8) is
  set to 0 unless its cell is exactly 0xa. So unexplored tiles are passable even if occupied.
- V: start and goal are clamped to the map. If the start cell is 0, the start is replaced by
  the first non-zero cell found by the ring scan 0x46ce80 (mask 0xffff, scan order in 1.4).
- V: radius != -1: only the square ring at Chebyshev distance == radius from the start is
  zeroed (radius 6: the 13 x 13 outline, usable interior 11 x 11). Done before goal marking.
- V: the goal cell (or every tile of the goal list) is then overwritten with 0xf, also if it
  was blocked, occupied or on the ring. Start == goal or start on the goal list returns 0.

### 1.2 Expansion 0x46d330 + 0x46d4d0
- V: two allocated fronts of 24000 (0x5dc0) bytes = 2000 (0x7d0) entries of (x, y, delay).
  No bounds check on the fronts (G: overflow is simply not handled).
- V: start cell gets bit 0x10 and is queued with delay 0. Round k (k = 0, 1, ...) uses label
  0x20 + 0x10 * k. For every front entry: delay >= 2 -> copied to the next front with
  delay - 1; delay < 2 -> its 8 neighbours are visited.
- V 0x46d4d0: a neighbour is taken when (cell & 0xf) != 0 and (cell & 0xfff0) == 0; it gets
  the current label and is queued with delay = cell & 7: value 9 -> 1, 0xa -> 2, 4 -> 4
  (goal 0xf -> 7, irrelevant). Neighbour order: N, NW, W, SW, S, SE, E, NE.
- V: no diagonal rule at all: a diagonal is allowed between two blocked orthogonal tiles
  (no corner test in 0x46d4d0, nor later in CanEnter 0x4347f0 or the side step 0x453e20).
- V (derived): label = round of first discovery, the delay is paid when leaving a tile.
  Cost of leaving: free 1, fog 2, wall-side 4, start 0; straight and diagonal cost the same.
  Because all waiting is simulated round by round, the label is the exact minimum of that cost
  (this corrects the "no relaxation, not a shortest path" remark in rules_movement.md 1.5).
- V: after each round the new front is scanned in queue order; the first entry whose cell
  nibble is 0xf ends the search with success (goal detected when discovered, not when left).
  Empty new front = failure. No other limit: no node count, no round limit, no time limit.
- G: labels are 12 bit (about 4093 rounds); beyond that the OR spills into the next cell.
  Not reachable on normal maps.

### 1.3 Path extraction 0x46ccb0 + 0x46d1c0
- V: goes backwards from the goal. Node count upper bound = label >> 4; nodes are written from
  the end of the buffer, the unused prefix is removed by a memmove (0x4820a0). Output node =
  (x, y, kind), kind = ((cell & 8) | 4) >> 2: 3 for values 9, 0xa, 0xf; 1 for value 4.
  Start and goal are both included, so a real path has >= 2 nodes.
- V 0x46d1c0, predecessor choice among the 8 neighbours in table order 0x4bcc28 =
  0 N(0,-1), 1 NE, 2 E, 3 SE, 4 S, 5 SW, 6 W, 7 NW: wanted label = own label - 0x10; any
  neighbour with a smaller non-zero label replaces the wanted label and resets the candidate
  list (the start, label 0x10, always wins when adjacent). Candidates = all neighbours with
  the final (minimum) label.
- V tie breaking: 1 candidate: it. 2 candidates: the one with the smaller circular distance
  (0..4) to the previous direction, on equality the second (higher index); the previous
  direction starts at 0 (N). 3 or more: only the first three (lowest indices) are looked at:
  if none is 0 -> the median of the three; if one is 0 -> 1 when 2 is among them, else 7 when
  6 is among them, else 0. G: the "1" / "7" result need not itself be a candidate.
- V (derived): extraction is "steepest label descent", not "label minus cost", so the returned
  path can run through wall-side tiles although the labels were computed with their penalty.

### 1.4 Unreachable goal 0x46ce80(goal, 0xfff0)
- V: square rings r = 1, 2, ... around the goal (the goal cell itself is not tested). Scan
  order inside a ring: top row left to right, bottom row left to right, left column top to
  bottom, right column top to bottom. The first ring that contains any labelled cell stops the
  scan; inside it the smallest label wins, first found on ties. That tile becomes the end of
  the path (partial path). The start is labelled 0x10, so the loop always ends; if nothing
  else is nearer, the result is the start itself and the path has 1 node.
- V: callers treat < 2 nodes as "no path": group 0x453920 returns 0 (and stops, slot +0x34,
  when already moving); member 0x453be0 returns 5 and clears its path length.
- V (derived): with radius 6 a goal outside the ring is always "unreachable", so the member
  walks to the labelled interior tile nearest (Chebyshev) to the goal, lowest label first.

### 1.5 Budget 0x46c410 and tick reset 0x4577b0
- V: two granted slots 0x515a68/6c. Granted if the requester is already in a slot or a slot is
  free. Otherwise it is put in the wait list (0x515a60/64 requester, 0x515a58/5c counter) at
  the first position whose stored counter is strictly smaller than its own retry counter, and
  gets "no". Tick start: wait list is moved into the granted slots, counters and 0x515a70
  zeroed.
- V 0x453be0: refusal returns 6 and increments the member retry counter (record +0x24); a
  grant resets it to 0.

### 1.6 Air (class & 4)
- V 0x46c470: no map is consulted. Nodes every tile along the major axis from start to goal
  with an 8 bit fraction on the minor axis, kind 3, goal appended; count = max(|dx|,|dy|) + 1.

## 2. Passability, occupancy, side step

- V CanEnter (slot +0x94): Marine 0x4347f0: inside map, occupancy entry == 0, and terrain
  0x46c330 with class = (descriptor byte +0x2c != 0) (1 small, 0 large). Spider, Truck,
  Headtruck 0x44bc80: the same with class 0 always. Returns 0 ok, -1 blocked. Air: map bounds
  only (rules_movement.md 2.2, not re-read). Water units are scripted, outside this system.
- V 0x46c330: class without bit 1: layer byte != 0; class 2/3: only value 9 (bit 3).
- V: a unit owns exactly one tile. Step start (Marine 0x433820, Spider 0x44ab90, Truck
  0x4604a0): old tile entry = 0, new tile entry = unit pointer, immediately, then group slot
  +0x124 and unit +0x44/+0x48 = new tile. Mobile units have no footprint larger than one tile.
- V: unit vs unit is only this tile test; no pushing, no swap.
- V side step 0x453e20 (tactic slot +0x2c; args member object, tile, delta in/out, flag):
  1. delta (0,0) -> 0.
  2. CanEnter(tile + delta) ok -> 0.
  3. if flag and the occupant is an object of the same tactic: run the formation scan (slot
     +0x60) if +0xd8 is clear; occupant marked connected (+0x5f) -> 3, nothing else tried.
  4. delta rotated 45 degrees clockwise (0x462310, table 0x4bc84c: N->NE->E->SE->S->SW->W->NW)
     and tested -> 1 with the new delta.
  5. else 45 degrees counter-clockwise of the original (0x4622e0 twice, table 0x4bc838) -> 1.
  6. else delta restored to the original -> 2.
  Only +-45 degrees, clockwise first, no 90 degree try. G: one early "return 2" comparison in
  the decompilation is garbled (lost register) and was ignored.
- V 0x453ac0: result 0 or 1 with non-zero delta orders the step at once (unit slot +0xcc).
- V 0x461c20 (direction to the anchor/target): pure axis step when major >= 5 * minor / 2,
  else diagonal.
- V member own path 0x453be0: own occupancy entry cleared during the search and restored
  (ground only), class = small flag | (air ? 4 : 0), occupancy respected (arg 8 = 0), fog of
  the own side (+0x690). Packed by 0x44ff60: at most 9 nodes = 8 steps, 4 bits per step
  (direction 0..7 as in 1.3, | 8 when the node kind is 3).
- V 0x453b60: one packed step per call through the side-step test; path exhausted -> 4;
  a non-zero result clears the path length and is returned.

### Repath conditions and timers (thinks of 0x454820 unless noted)
- V 0x455b10: member with own path: finished or blocked -> own path flag +0x15 and retry
  counter cleared, then normal handling in the same call. Connected member without target
  drops its own path. Unconnected member without target requests a local path (radius and
  goal arguments lost; G radius 6 as in logs). Member with target and result 2: scans tiles
  around the target inside weapon range and inside reach, then local path to that list.
- V 0x455b10 connected branch: after the first test (result != 2) the member occupancy entry
  is cleared, the test is repeated from tile + step with the direction from there to the
  anchor, the entry is restored, and the step is ordered if a vector test 0x456a60 passes.
  G: a one tile look-ahead; its exact effect on the chosen step was not resolved.
- V (leash notes): group repath when the next node fails 0x46c330 or is unexplored (< 8);
  follower/engaged groups every 37th think (+0x88 > 0x24 = 36); stragglers split every
  15..22 thinks (+0x64); anchor waits at most 20 (0x14) thinks per node; pending group path
  (+0xb8) retried each think until the budget grants it.
- V: a search refused by the budget is simply repeated at the next request (code 6).

## 3. Per tick movement arithmetic

Common (V): positions are 24.8 fixed point (px * 256). Pixel = (v + (v < 0 ? 0xff : 0)) >> 8
(toward zero). Length = octagonal norm 0x461fe0: a = max, m = min, b = m + (m >> 1),
n = a + (b >> 2) + (b >> 6) - (a >> 7) - (a >> 5). Scaling 0x461d50: out = v * len / n(v)
per component (truncating division); n == 0 gives (len, len).

### 3.1 Marine (step 0x433820, think 0x433db0 state 2)
- V step: target px = tile * 48 + 24 (0x30, 0x18). d = target - pixel position. Facing =
  ((atan 0x461ac0 + 0x10) >> 5) mod 8. Velocity = d * (speed << 8) / n(d), speed = descriptor
  +0x14 in px per tick. Refused in states 3 and 4. State = 2. Velocity is fixed for the step.
- V (computed): from a tile centre, axis step d = (48, 0): n = 47, so 48/47 = 1.021 of the
  nominal speed; diagonal d = (48, 48): n = 66, each component 48/66 of speed, real length
  1.028 of nominal over 67.9 px, so a diagonal tile takes about 1.40 times an axis tile.
- V think: dist = n(target - pixel pos). If dist > speed: pos += velocity; then if the new
  dist <= 3 * speed and the unit is not about to attack, the group is asked for the next step
  (slot +0x11c) while still moving; a new step re-aims from the current position, so tile
  centres are cut by up to 3 * speed px. If dist <= speed: pos = target exactly, state 1,
  possible switch to attack (state 3), then the group is asked.
- V: no acceleration, no turn rate. State 1 asks the group every tick.

### 3.2 Truck 0x460a10 and Headtruck 0x42c880
Truck: speed +0x80, heading +0x74, max = descriptor +0x14. Headtruck: speed +0x88, headings
+0x78 and +0x7c, max = descriptor +0x1c. Same code shape.
- V per tick in the moving states: want = atan(target - pos); diff = (signed char)(want -
  heading). |diff| < 8 -> heading = want, else heading += 8 * sign(diff) (mod 256).
  pos += unit vector(heading, 0x461f70) scaled to speed << 8. dist = n(target - pos) in 24.8.
- V: dist < 0x3000 (12288 = 48 px): ask the group for the next step (every such tick).
- V: dist < 0x1800 (6144 = 24 px): speed -= max * max / 48 + 1; speed < 1 -> 0 and state 1
  (no snap to the centre). Else if |diff| < 0x38 (56, the value before turning): speed +=
  max * max / 48 + 1, capped at max. Else speed -= max * max / 48 + 1, floor 0.
  Example max 8: 2 per tick; max 12: 4 per tick. Speed is whole px per tick.
- V: Truck in the attack state turns towards the target quantised to 32 ((angle + 0x10) & 0xe0).

### 3.3 Zeppelin 0x4719b0 (speed +0x80, max descriptor +0x14)
- V: same turn (8 per tick) and same integration. dist < 0x1800: brake by max^2/48 + 1, stop
  at < 1 (state 1), report tile, ask next step. dist >= (speed + 24) * 256: accelerate when
  |diff| < 0x38 else brake, no step request. Between: report tile and ask next step.

### 3.4 Spider 0x44ab90 / 0x44b360 and Bird 0x407b10 (partly read)
- V Spider: a step ordered while moving is stored (+0x88/+0x8c, flag +0x90) and taken on
  arrival if CanEnter still passes. Heading = (atan + 0x10) & 0xe0, velocity = unit vector of
  that heading scaled to descriptor +0x1c << 8, constant. G: arrival = position passed the
  target on the tested axis (comparisons 0x44bcf0 / 0x406da0), then snap, ask group, state 1.
- V Bird: speed += 2 per tick up to max (+0x14); without target and dist < max + 24:
  speed -= 2 + (max * max / 2) / (3 * max + 24); next node asked when dist < (max + 8) * 3.
  G: units of these distances (pixels) and the turning part were not re-read.

## Unknowns
- Lost arguments in 0x455b10: goal list / radius of the member search, and the look-ahead in
  the connected branch (which of the two tests decides, behaviour on result 3).
- Garbled early "return 2" in 0x453e20.
- Magnitude of the unit vector 0x461f70 (irrelevant if 0x461d50 renormalises, G).
- Spider arrival test and Bird turning were only skimmed; Boat/Johnka not covered.
- Occupancy sentinel values 1 and 2 and building footprints (writers not traced here); the
  wave treats any non-zero entry as blocked.
- Behaviour when a front exceeds 2000 entries or labels exceed 12 bits (not handled in code).
- How many thinks of 0x454820 run per logic tick.
