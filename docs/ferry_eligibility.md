# Ferry eligibility: what makes a ferry accept a group (decompilation trace)

Date: 2026-11-14. Read-only analysis of the original executable. **V** = read in the
decompiled code or in the binary/vtable/mission file, **G** = inference or guess.
Addresses are function entry points. No game was launched for this report.

## Bottom line

**There is no eligibility rule for the requesting group in the code.** Every ordinary ground
group (base tactic class, vtable `0x4944f0`) runs exactly the same path as a scenario squad:
group creation by box selection (`0x448b40`) produces the same class with the same defaults,
and neither the right-click handler, the follow order, the approach step, the ferry request
nor the ferry think tests group origin, size, unit type, side, scenario id or a destination.
The complete list of conditions is in the next section. The live failure of the box-selected
groups (5, 7, 12 units) is therefore **not explained by a missing eligibility condition**; the
remaining explanations are situational (ranked list near the end), the first being that the
right click did not resolve to a follow order on the boat at all.

Confidence: high that the condition list is complete for the path click -> request -> boarding
(all functions on the path were read); medium-low on which situational cause hit the live
runs, because that cannot be decided by reading.

## The full chain and every condition on it

### 1. Right click (`0x447e00`, PCT 0x18)

- V: returns at once if the side has no selected tactic (`side+0x688`).
- V: modifier bits come from `0x42ea70`: bit 0 = key state 2, bit 1 = key state 3,
  0x40 = a UI flag. With bit 0 only the click becomes a queued tile move (`+0xf8`); with
  bit 1 only nothing is ordered; with bit 0x20 all object hit tests are skipped and the click
  is a plain tile move (`+0xd8`). The follow path needs **no modifier bits**.
- V: first hit list (`0x46fe10`) holds only alive objects whose type virtual `+8` is 0 (any
  mobile unit) or 1 (building) and whose rectangle (`+0x1c`) contains the click point. The
  type values were read from the vtables: units 0, buildings and towers 1, **boat 6**
  (`0x4083b0`), Johnka 7. A boat is never in this first list.
- V: for each object of the first list that is visible (`+0x10` with the side fog map):
  allied (`0x4472e0`) with a tactic different from the selected one -> if that tactic's kind
  (`+0x80`) is not 1 the selected group gets the follow order on it, if it is 1 a move to
  its position; not allied -> attack order. Either case ends the handler. So **a unit or
  building of another allied group under the cursor takes precedence over the boat**.
- V: second list (`0x46b230`, plain rectangle test): the first visible object with type 6
  gets `+0xe4(side)` and the selected group receives `+0xf0(boat tactic)`. **No ally test,
  no kind test, no test of the selected group.** If nothing matches, the click is a plain
  move to the tile (`+0xd8`), which looks the same on screen (units walk to the shore, stop).
- V (correction of `docs/ferry_control_flow.md`): tactic kinds are base group 0 (`0x402430`),
  town 1 (`0x45f030`), ferry 2 (`0x403870`), Johnka tactic 4 (`0x446310`). The earlier note
  "an ordinary group has kind 1" is wrong; kind 1 is the town.

### 2. Follow order (`0x451440`, tactic `+0xf0`)

- V: clears the waypoint queue (`+0xdc`), calls `+0x5c` (`0x450710`: empties the enemy
  list at `+0x5c` and clears the engage byte `+0x61`), releases a previous target
  (`target +0x70`), sets `+0x94 = 1`, `+0x9c = 0`, `+0x98 = target`, registers at the
  target (`+0x6c`, `0x44fb30`), then calls the step `+0x58` once and plays the follow voice
  set (0x68; a plain move plays set 0x64). No condition on the group.
- V: for a town tactic (vtable `0x494648`) the same function runs but its path-to virtual
  `+0x30` is a stub (`0x402970`) and its move order `+0xd8` is a no-op (`0x402880`): a
  selected town never walks. Matches the headless garrison observation.

### 3. Approach step (`0x453570`, tactic `+0x58`), the only caller of the request

The request `+0x118` is called from this function only (all other `+0x118` call sites in the
corpus belong to other classes). The step runs: once inside the follow order; every think
while the group state `+0x20` is 1 (arrived/idle); every 37th think after the last path
request while `+0x94` is set (counter `+0x88`, reset in `0x453920`); and in the walking
state when the next path waypoint is impassable or unexplored (`0x454820`). All V.

Conditions inside the step, in order (all V):

1. **Engage pre-emption.** If the enemy list `+0x5c` is non-empty and `+0x61` is set, the
   group paths towards the enemy and returns; the ferry branch is not evaluated.
2. `+0x94` must be set (otherwise the waypoint queue is served).
3. **Distance.** `0x461fe0` (approximate norm) of (group anchor `+0x10/+0x14` minus target
   anchor `target+0x10/+0x14`) must be strictly less than the reach `+0xc` (`0x4516b0`:
   4, 8, 8, 6 tiles for mode `+0x24` = 0..3; constructor default mode 3 -> 6).
4. `+0x9c` must be 0 (with `+0x9c` set the group merges into the target instead; the
   follow order clears it).
5. Target kind `+0x80` must be 2 (ferry).
6. Bit 2 of the group flag dword `+0x50` set -> silent cancel `+0xdc(0)` and no request.
   Bit 2 = every member answers true to object virtual `+0xa4`, which is true only for Bird
   and Zeppelin (`0x471240`). Bit 0 = every member is a "small" Marine (`0x432fa0`, descriptor
   byte `+0x2c`); it is only the pathfinding class and plays no role in the request.
7. Ferry ready `+0x114` (`0x408ca0`): phase `ferry+0xf0 == 0`. Nothing else (the enable
   byte `+0xe8` written by `0x408c80` is not read on this path).
8. Then `ferry.+0x118(group)`; afterwards the step re-paths to the ferry anchor anyway.

### 4. Request (`0x408dd0`) and ferry think (`0x408eb0`)

- V: request stores the requester at `+0xf4`, stores `requester.+0xa0()` at `+0xfc`, sets
  phase 1 and asks the boat object to open (`0x408670` mode 0, effective only if the boat
  state `boat+0x60` is 1).
- V (correction of `docs/transport_live_observations.md`): tactic virtual `+0xa0` is
  `0x473750` and returns the tactic's **side** (`+8`). `+0xfc` is the side in which
  `0x4474d0` creates a fresh group for the passengers on arrival. There is no "scenario
  destination" and so no rule that only scenario groups may ride.
- V: phase 1 waits for boat state 3 (ramp open), then every think scans the 3 by 3 tiles
  around the boat tile in the occupancy grid `0x4dd2f8` and takes an object when: cell holds
  an object pointer, object alive (`+0x5c`), `+0xa4` false, `object+0x30 == requester`,
  passenger count `+0xf8 < 5`. Taken objects are added to the ferry tactic (`0x408e10`,
  which hides them).
- V: the ferry closes and leaves when the count is 5, **or** when the count is non-zero and
  the requester pointer is 0. The requester is zeroed by the ferry's unregister hook
  (`0x408e50`, slot `+0x70`) when the requesting group stops following (new order, cancel,
  or the group dissolved because all its members boarded). If that happens with nobody
  aboard, the ferry goes to phase 0 (other followers present) or phase 5 (closes the ramp,
  then phase 0). So a group of at most 5 leaves when the last member has boarded; a larger
  group loads 5 and the rest stay behind as the same group.
- V: the ferry is ignored entirely while its object is missing or more than 7 tiles off the map.

### 5. Group creation by box selection (`0x448b40`, PCT 0x15)

- V: without the add modifier it creates a new base tactic (`0x4474d0` -> constructor
  `0x44f5c0`: mode 3, flags 5, `+0x94 = 0`, `+0x61 = 0`, active byte `+0xb0 = 1`), adds
  every object in the rectangle that is of the player's side and not type 1 through
  AddMember (`0x454090`), and selects it (`0x4498a0`). AddMember removes the object from its
  old tactic (also from a town), sets the anchor to the first member's tile and clears flag
  bits 0 and 2 per member. PCT 0x1d (`0x448350`) does the same for all non-building members
  of a selected town.
- V: nothing in this path marks the group as different from a scenario team. A scenario team
  (`[Team5]`, three type 3 trucks, side 0) is the same class; its only code-relevant difference
  from a foot group is flag bit 0 (trucks are "large", pathfinding class 0).

### 6. Mission 2 data (V, `Maps/mission_2.mp` and `.scn`)

- Shore ferries `Boat0` (28,112) and `Boat1` (36,112) are **Side 0** (the player), no
  preloaded units; the script only activates boats 17, 18 and 9, never these two.
- Player town at (7,112) with 12 units (types 0, 1, 6); the squad starts at (14..15,110..111).

## Why the live box-selected groups did not board (ranked, all G)

1. **The click did not become a follow on the boat.** The boat is found only by the
   rectangle test of the second list; a miss, or a type 0/1 allied object of another group
   under the cursor, or modifier bit 0 / bit 0x20, yields a plain move with the identical
   picture "units walk to the shore near the ferry and stop". The squad runs centred the
   camera through the squad icon; the box-selection runs had a different camera position and
   synthetic mouse input. Check: after the click the group must have `+0x94 = 1` and
   `+0x98 =` ferry tactic.
2. **Engage pre-emption.** In modes 2 and 3 an arrived group (state 1) with a non-allied
   object in its scan rectangle (about 6 to 8 tiles around the members, `0x454820`,
   `0x455570`) sets `+0x61` and from then on the step never reaches the ferry branch. No such
   object is expected at the mission 2 start shore, so this is unlikely there.
3. **Anchor never inside the reach.** Possible only if the group anchor stays 6 or more
   tiles (norm `0x461fe0`) from the ferry tactic anchor, for example mode 0 (reach 4) plus a
   path that ends early. Unlikely for units "gathered beside" the ferry.
4. Ferry not in phase 0 at that time (still returning). Unlikely over 32 s.

Group size, unit type, town origin and scenario membership are ruled out by the code (V).

## Decisive check (headless, memory read only)

Create a group through PCT 0x15 and order it through PCT 0x18 on the boat's pixel position
(or write the follow fields as in `ferry_timeline.md`), then log per tick: `side+0x688`;
group `+0x20`, `+0x24`, `+0x50`, `+0x61`, `+0x94`, `+0x98`, `+0x9c`, anchor, count of list
`+0x5c`; ferry `+0xf0`, `+0xf4`, `+0xf8`; boat `+0x60`. Reading: `+0x94 == 0` after the
click -> cause 1; `+0x61 == 1` -> cause 2; anchor distance >= reach -> cause 3. By the
code, a group written with the follow fields must be accepted regardless of how it was built.

## Rules for the clone

Right click inside a boat rectangle (after unit/building hits) = follow order on the boat
tactic for any selected group. Request when: following, not engaged, anchor distance < reach
(4/8/8/6 by mode), group not all-airborne, ferry phase 0. Ferry loads members of the
requester standing in its 3 by 3 block, up to 5; leaves at 5 or when the requester stops
following/has dissolved with at least one aboard; cancels (closes) if the requester leaves
with nobody aboard. Passengers land as a new group of the requester's side. No scenario rule.

## Not read / unknown

- Boat visibility test `+0x10` (`0x402490`) and rectangle `+0x1c` (`0x46ba50`): exact extents.
- How often the tactic think runs per logic tick (affects only timing of the request).
- Meaning of key states 2 and 3 in `0x42ea70` (which physical keys) and who sets bit 0x20.
- Alliance mask `side+0x648` between side 0 and the neutral side 2 in mission 2.
- Formation routine `0x450910` and member assignment `0x451c30` were not read in full; they
  do not call the request, but they decide where members stand relative to the 3 by 3 block.
- Whether tiles around (28,112) and (36,112) are passable for both movement classes.
