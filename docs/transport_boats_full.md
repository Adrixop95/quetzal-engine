# Transport: boats, ferry tactic and submarine (full reading)

Date: 2026-11-21. Read-only decompilation and disassembly of the original executable; no game launched.
**V** = read in the code, binary or mission file structure. **G** = inference. Numbers are hex (decimal).
Builds on `docs/ferry_control_flow.md`, `ferry_timeline.md`, `ferry_eligibility.md`.

## 1. Classes involved

| Thing | Vtable / code | Notes |
| --- | --- | --- |
| Boat world object | vtable `0x48faa8`, type `+8` = 6 (`0x4083b0`) | V. Constructors `0x408300` (load), `0x408410`/`0x4084d0`. Sprite resource 0x17 (23) at `+0x64`. Pool `0x4cc3d0`. Descriptor 0x54 (84). |
| Ferry tactic | vtable `0x48fbd4`, kind `+0x80` = 2 (`0x403870`) | V. Subclass of the base group tactic; constructor `0x408d10` (`+0xf0`,`+0xf4`,`+0xf8` = 0), load `0x408a70`, save `0x408a10`, fixup `0x408cd0`. Pool `0x4d9b50`. |
| Submarine tactic | vtable `0x492fdc`, kind 3 (`0x401a00`) | V. Built in `0x42f6e0` (mp) and in the tactic loader `0x445xxx` case 3. |
| Path record | vtable `0x49312c`, base `0x446730` (`0x4940d0`) | V. Built by `0x42fc90`; pool `0x4cd698`. Not used by boats (section 5). |

### Boat object fields (V, from `0x408600`, `0x408670`, `0x408770`)
`+0x24/+0x28` pixel x,y; `+0x44/+0x48` tile x,y; `+0x5c` present byte; `+0x5d` placed byte; `+0x60` boat state;
`+0x64` animation object; `+0x68` direction bit (0 or 1). Animation index = `dir*5 + k`, k: 0 idle/travel (loop),
1 ramp opening, 2 ramp open (loop), 3 ramp closing, 4 turning (`0x408670`, `0x408770`).

### Ferry tactic fields (V)
`+0x58` leader = the boat object (first member); `+0x0c` member list (boat + hidden passengers);
`+0xe8` enable byte (`+0x110`, `0x408c80`, generic, not read on the ferry path); `+0xe9` byte (`0x408c60` sets,
`0x408c70` clears); `+0xec` scenario id (`+0x84` `0x408b10` set, `+0x88` `0x408b30` get);
`+0xf0` phase; `+0xf4` requester tactic; `+0xf8` boarded count; `+0xfc` side record used for landing.

## 2. Mission data: boat record and route (V, `0x42f560`)

`[Globals] Boats=n`, then `[Boat%i]` with keys read: `Side`, `X`, `Y` (tile coordinates), `Up`, `ScenarioId`,
and through `0x430930` `Units` + `Unit%i` (format `%i%c%i%c%i`, 5 fields) for preloaded passengers.
Keys present in the files but **never read by the exe**: `EditorX`, `EditorY`, `EndPosition` (strings absent from
the binary, V). A record whose X,Y differ from EditorX,EditorY is simply placed at X,Y (G: the editor stores the
far-end position there).

Load sequence per boat: side record = `0x4dd2f4 + Side*0x69c` (1692); `0x447580` creates a ferry tactic in that
side; `0x43d5d0(Side, X, Y, 0x54)` creates the boat; ferry `+0xd4` adds it; boat `+0xc4` (`0x408600`) places it
at tile centre (`tile*0x30+0x18`, i.e. *48+24), dir 0, anim 0, state 1; `0x408750(Up==1)` sets dir (only in
state 1); ScenarioId -> ferry `+0x84`; preloaded units; ferry `+0x90`; **`+0xfc` = the side record**;
`0x408d70` hides every preloaded mobile member (type 0) with `+0xdc(pos,0)`. Preloaded units are **not** counted
in `+0xf8` (V).

**There is no route data.** No boat code reads waypoints or the Paths pool. The route is implicit: a fixed
diagonal line from the placement tile (section 3). The far dock and the home dock are not stored; each is where
the boat next meets land.

## 3. Movement (boat think `0x408770`, slot `+0x60`)

- V: called every logic tick through the object update `0x43edc0` (slot `+0x120`, pool `0x4cc3d0`), skipped
  while `0x42e8c0(0x30)` is true (G: a pause or freeze flag).
- V: only in boat state 5. Step: d = +1 for dir 0, -1 for dir 1; x += -5*d, y += +5*d. So **dir 0 moves south
  west (x-, y+), dir 1 north east (x+, y-), 5 px per axis per tick** (about 7.07 px/tick, one diagonal tile per
  9.6 ticks). No steering, no other heading exists: every boat moves on the NE-SW diagonal.
- V: stop test: tile = pixel/48; when |tile centre x - x| < 5 and the next diagonal tile (tx-d, ty+d) is set in
  layer 1 of `0x46c330` (grid `0x515a74`, the small-unit walkable grid), the boat snaps to the tile centre and
  goes to state 1. G: "next tile is walkable land" = dock. Water passability of the tiles crossed is never
  tested; a line that meets no land runs off the map (then section 6, boat lost).
- V: animation end transitions: state 2 -> 3 (anim k=2), 4 -> 1 (k=0), 6 -> **dir toggled**, state 5 (k=0 of new
  dir). Also plays the animation frame sound through `0x401480`.
- Boat state table (V, `0x408670` modes and `0x408770`):

| State | Meaning | Entered by |
| --- | --- | --- |
| 0 | dead / inert (think does nothing) | `+0x5c(0)` = `0x4083a0` |
| 1 | idle, ramp closed | placement, close end, stop on land |
| 2 | ramp opening | mode 0 from state 1 |
| 3 | ramp open | anim end of 2 |
| 4 | ramp closing | mode 1 from state 3 |
| 5 | travelling | anim end of 6 (dir toggled), or mode 3 from any state (no toggle) |
| 6 | turning | mode 2 from state 1 |

Boat `+0x74` (`0x4089b0`) returns 0x47 (71) in states 2..6, or in state 1 when on the map, else 0 (meaning G,
probably a cursor/selection code; boats stay unselectable as observed).

## 4. Ferry tactic think `0x408eb0` (slot `+0x138`) and phase table

V: called every logic tick for every tactic in pool `0x4d9b50` from `0x4577b0`; no divisor. Guard first: the
boat pointer `+0x58` is non-null and the boat tile (`+0x88`) lies in (-8, width+7) x (-8, height+7); otherwise
the "lost" branch of section 6.

| Phase | Condition each tick (V) | Action |
| --- | --- | --- |
| 0 | none | idle; a follower sends `+0x118` (`0x408dd0`) when `+0x114` sees phase 0: requester, `+0xfc` = requester side, phase 1, boat mode 0 (opens only from state 1). |
| 1 | boat state must be 3, else wait | if count == 5, or count != 0 and requester == 0: requester = 0, phase 2, boat mode 1 (close). Else scan the 3x3 tiles around the boat tile in occupancy grid `0x4dd2f8`: object present (`+0x5c`), not airborne (`+0xa4`), `+0x30 == requester`, count < 5 -> `+0xd4` (`0x408e10`: AddMember `0x454090`, then hide with `+0xdc(own tile,0)` unless type 6) and count+1. |
| 2 | boat state 1 (closed) | phase 3, boat mode 2 (turn; at its end dir toggles and travel starts). |
| 3 | boat state 1 -> mode 0 (open). boat state 3 -> unload | unload (section 6), then `+0xfc` = 0, count = 0, phase 4, boat mode 1 (close). |
| 4 | boat state 1 | phase 5, boat mode 2 (turn, toggle, travel back). |
| 5 | boat state 1 -> phase 0; boat state 3 -> mode 1 (close) | return trip ends when the line meets land again. |

Extra conditions not in the earlier docs (V):
- **Unregister hook `0x408e50` (`+0x70`)**: removes the follower; if it is the requester, requester = 0, and if
  nobody is aboard `+0xfc` = 0 and phase = 0 when other followers remain, else 5 (close the ramp, then 0).
  With passengers aboard the next phase-1 tick departs.
- **`boat.activate` (script op 0xd, `0x402000` -> `0x408d50`)** picks the *last* kind-2 tactic whose scenario id
  matches and forces phase 3 and boat mode 3 (state 5, travel, **no turn and no dir toggle**) in any phase,
  even with the ramp open or with followers waiting. Then the normal phase 3 -> 4 -> 5 -> 0 chain follows.
- A request made while the boat is not in state 1 (e.g. still travelling) leaves the ferry in phase 1 until the
  boat reaches state 3, which never happens from state 5/6 alone; only possible if phase 0 was reached early by
  the unregister hook (G, edge case).
- After an unload `+0xfc` = 0; a second `boat.activate` without a new request would call `0x4474d0(0)` (G:
  invalid side, possible crash in the original; the clone should skip landing or keep the boat side).

Timing (V measured in `ferry_timeline.md`, matched to the states): ramp opening <= 8 ticks, loading one scan per
tick (3 units in 16 ticks including the walk-in), closing about 8 ticks, turn about 0x1c (28) ticks, travel
48/5 = 9.6 ticks per diagonal tile, opening + unload about 10 ticks, closing 8, turn 28, return. Exact animation
frame counts live in sprite resource 0x17 (not read).

## 5. Paths (`0x42fc90`)

V: `[Paths] Count=n`, keys `0..n-1`, value `id, x,y, x,y, ...` (first `%i`, then `%i , %i` pairs separated by
commas), stored as 8-byte points in a list at record `+8`; record id at `+4`. V: no reference from the boat,
ferry or submarine code. The consumer was not identified in this pass (unknown; G: script or AI movement).

## 6. Passenger fate

- **Unload destination (V, `0x408eb0` phase 3):** `0x4474d0(+0xfc)` creates a fresh base group (constructor
  `0x44f5c0`: mode 3, flags 5, not following) registered in that side (`0x446fa0`). Members are placed ring by
  ring around the boat tile, ring r = 1, 2, ...: row y-r (x-r..x+r), row y+r, column x-r, column x+r (corners
  visited twice). For each cell the first non-boat member whose `+0x94(tile)` is not -1 gets `+0xdc(tile,1)`
  (shown) and is added to the new group. Repeats while the ferry has more than one member (the boat). V matches
  the timeline: the three units landed on tiles (x-1..x+1, y-1) of the stop tile. The landing group is a new,
  unselected group; it is not the original requester. If no cell ever accepts a member the loop does not end
  (G, hang risk; clone should cap the radius).
- **+0xfc side:** the requester's side (`0x473750` returns `+8`) for a request, the boat's own side for preloaded
  boats (loader). Not a destination point.
- **Requester dies or stops following (V):** handled by `0x408e50` above. Passengers already aboard are members of
  the ferry tactic, not of the requester, so they survive and land as a new group of `+0xfc`. If all members boarded
  the requester group empties and is released, which is the normal "leave with fewer than 5" path (G that group
  destruction goes through `+0x70`; the cancel order `0x4543e0` does).
- **Boat lost (V, `0x408eb0` fallback):** if `+0x58` is null or the boat is more than 7 tiles off the map, every
  member gets `+0x128` (`0x43ec60`, object death: present 0, dead flag `+0x5e`, removed from the tactic; the
  side notification `0x449ac0` is skipped only for descriptor 0x54), `+0x58` = 0, every follower gets `+0xdc(1)`
  (cancel), active byte `+0xb0` = 0, and the tactic is deleted (`0x447d70`) when its reference count `+0xac` < 1.
  So **passengers die with the boat**. When the boat object itself dies (`0x43ec60` on it: state 0, removed via
  tactic `+0x12c`), the leader re-pick `0x453fd0` skips hidden passengers (present byte 0), so `+0x58` becomes
  null and the same branch runs next tick (V for the chain; G that nothing else re-assigns `+0x58`). Whether
  anything can damage a boat at all is unknown: type 6 is excluded from the attack hit list (`0x447e00`).

## 7. Submarine (kind 3, vtable `0x492fdc`)

- V load (`0x42f6e0`): `[Globals] Submarines=n`, `[Submarine%i]` keys read `Side` (default 0), `X`, `Y`,
  `ScenarioId`. Creates a tactic in the side, `+0xf0` = 0, then `+0x90` (`0x42fad0`): base placement
  (`0x454220`) and an effect object (pool `0x4c1978`, vtable `0x4904ac`, `0x4189d0`) with sprite resource 0x1b
  (27), animation 0, looping, stored at `+0xf0`.
- V think `+0x138` = `0x401110`, empty. No members, no route, no movement, no combat.
- V go: script `submarine.<id>.go` (op 0xe, `0x4378a0` parse, `0x402060` exec, last kind-3 tactic with matching
  scenario id) -> `+0x14c` = `0x42fbb0`: delete the looping effect, spawn animation 1 **non-looping** at the anchor
  tile centre (`+0x10/+0x14`*48+24), delete the tactic. The effect think `0x418ab0` deletes itself when the
  one-shot animation ends. So the submarine is a scripted decoration: a looping idle sprite, then one
  appearance/dive animation, then nothing.
- Save/fixup: `+0x140` `0x42fa80` saves the effect reference, `+0x144` `0x42fab0` restores it.

## 8. Unknowns and what needs a live run

1. Frame counts of boat animations (resource 0x17, k = 0..4 per direction) and submarine animations (0x1b):
   read the sprite file or measure. Live/asset.
2. What anim 0 and anim 1 of the submarine show (idle in water vs surfacing/diving). Live visual.
3. Meaning of `0x42e8c0(0x30)` that freezes object updates. Code read.
4. Meaning of boat `+0x74` value 0x47. Code read.
5. Real unit overrides of `+0x94` (placement test) and `+0xdc`; base stubs return -1 / store the present byte. Code read.
6. Whether a boat can be damaged or killed in normal play (and so whether passenger death is reachable). Live.
7. Consumer of the Paths records. Code read.
8. Behaviour of a second `boat.activate` with `+0xfc` = 0. Live (or code read of `0x44f5c0` with side 0).
