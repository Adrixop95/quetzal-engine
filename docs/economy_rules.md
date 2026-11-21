# Economy, construction and production rules of the original

Evidence date: 2026-11-15. Static, read-only pass over `work/decomp/all_full.c`,
`work/gaps/dec_410010.c`, `objdump` of `game/Aztec.exe` and table bytes read from the
executable. Nothing was run. **V** = read in code/data at the cited address in this pass.
**V(doc)** = stated as verified in `docs/` and not re-read here. **G** = inference.
Numbers are given as hex (decimal).

## 0. Objects and fields used below

| Thing | Field | Meaning | Evidence |
| --- | --- | --- | --- |
| Side | `+0x698` | money | V `0x447d00`, `0x4477d0` |
| Side | `+0x63c` | total money received (statistic) | V `0x4477d0` |
| Side | `+0x640` | total money spent (statistic) | V `0x447d00` |
| Side | `+0x688` | currently selected town (target of packet 0x1c/0x1e) | V `0x4577b0` |
| Side | `+0x690` | per-side byte map used by placement (see 1.1) | V `0x45bd50` |
| Town | `+0xf0` | hall object | V `0x459bb0` |
| Town | `+0xf4` | build tree object | V `0x459bb0`, `0x411ac0` |
| Town | `+0xf8` | waiting queue ring | V `0x45cec0`, `0x45cfd0` |
| Town | `+0x110` | active production timer (vtable `0x4947fc`) | V |
| Town | `+0x158` | BuildSpeed 0/1/2 | V `0x45c5b0` (index into `0x4bc3cc`), set by packet 0x20 in `0x4577b0` |
| Town | `+0x15c` | hall level 0..3 (0 = outpost) | V `0x459bb0`, `0x45c5b0` |
| Town | `+0x164` | race 0..2 | V `0x459bb0`, `0x45dc50` |
| Town | `+0x168` | object that must not be started (queue head equal to it is skipped) | V read `0x459bb0`, meaning G |
| Town | `+0x16c` | rally target (tactic/town); equals the town itself when none | V `0x459bb0` |
| Town | `+0x174` | income estimate | V `0x45d160` |
| Town | `+0x17c` | AI freeze flag: -1 while a non-defence building is queued, 0 after | V `0x45cec0`, `0x45cfd0`, `0x45a880`, `0x459bb0` |
| Town | `+0x1b4` | sum of prices paid by this town | V `0x459bb0`, read by `0x45c300` |
| Timer | `+4` object, `+8` work per tick, `+0xc` price paid, `+0x10` accumulated work, `+0x14` paused | V `0x40a3c0`, `0x40a400`, `0x40a480`, `0x40a490` |

Timer vtable `0x4947fc` (V, read from the executable): `+0` start `0x40a3c0`, `+4` get
object `0x4467a0`, `+8` clear `0x40a470`, `+0xc` toggle pause `0x40a570`, `+0x10` is paused
`0x40a4f0`, `+0x14` is empty `0x40a4e0`, `+0x18` price `0x40a480`, `+0x1c` done fraction
`0x40a490`, `+0x20` advance `0x40a400`.

## 1. Construction

### 1.1 Placement test `0x409940` (V)

Signature (recovered from the pushes in `0x45bd50`):
`(centreX, centreY, x, y, mapW, mapH, radius, mask7x7, outMask, sideMap, resMap, needA, needB)`.

For each of the 0x31 (49) mask cells (anchor = mask centre, offsets -3..+3) a cell is
"good" only if all of the following hold:

1. inside the map;
2. `dx*dx + dy*dy < radius*radius` measured in tiles from (centreX, centreY) (Euclidean,
   strict);
3. terrain byte in layer 0 (`0x46c330(pos, 0)`, grid `0x515a78`) is non-zero (buildable);
4. the object grid cell (`*0x4dd2f8 +0x10`) is empty;
5. none of the 4 orthogonal neighbour cells holds an object of kind 1 (building, virtual
   `+8` returns 1). Diagonal neighbours are not tested. This gives a one-tile gap between
   buildings; units in neighbour cells do not block;
6. if `sideMap` is given: its byte at the cell (`map+0x14`, width `map+4`) is `> 0x20` (32).

The result is true only if every mask cell with value 2 is good. Cells with value 0 or 1
never influence the result (value 1 has no reader, V).

Resource requirement (only evaluated when `outMask != NULL`): if `needA` or `needB` is set
and no value-2 cell has a non-zero byte in a required channel (A = byte 0, B = byte 1 of the
2-byte resource map `0x4d9c18`), the result is false and the out mask is zeroed. One
resource-bearing body cell is enough; the threshold is "non-zero" (answers the open
"mineral minimum" question for the UI path: V).

Out mask: body cells get 1 (good) or 0 (bad), all other cells get 2 (used for the
red/green ghost).

### 1.2 Callers and parameters

| Caller | Use | Centre | Radius | Extra |
| --- | --- | --- | --- | --- |
| `0x45bd50` (called from UI `0x462650`) | player ghost test | hall tile (town `+0xf0` virtual `+0x88`) | 8 | sideMap = side `+0x690`; non-defence buildings: needA = (d60 != 0), needB = (d61 != 0); defence: no resource test. V |
| `0x409ce0` | AI site for income buildings (d60 or d61 non-zero) | hall | 8 | scans the 16x16 square hall-8..hall+7, keeps the site with the highest `0x42acd0` yield (ties: last), fails if the best yield is 0. V |
| `0x409e60` | AI site for other buildings | spiral from hall, stops when a whole ring is outside distance 8 | 100 (no limit) around the candidate itself | keeps the site with the lowest resource sum (`0x42acd0` with coefficients 0x100). V |
| `0x40a080` | AI site for defence | spiral from an existing defence building of the town (else the hall), candidates with dist^2 < 0x40 (64) from the hall | 100 | lowest resource sum. V |

Selector: `0x40a270` / `0x45bf10` (V).

So the town build radius is 8 tiles, Euclidean, strict (`dx^2+dy^2 < 64`), measured from the
hall tile to every body cell (V). The task-101 value of 16 is not supported.

G: the meaning of side `+0x690` (threshold > 32). Most likely the side's explored/light
map, i.e. body cells must be lit. Not decoded.

G: because needA is derived from d60 != 0 for every non-defence building, a hall-type
record (constant d60, terrain flag off) would also need a channel-A cell if it were placed
through this path. Whether any such record is placeable by the player is not checked.

### 1.3 Ordering a building (V)

- Packet 0x1c (`0x4577b0`): type (default 0x25 (37) when -1) is admitted into the selected
  town by `0x45cec0`; if the new object is a building that is not a hall upgrade
  (`0x45cd50`), its position is set from the packet (`0x410960`, defence `0x41d6a0`).
- The packet handler does not call `0x409940` again. The placement test is done on the
  client before the packet is sent. G: no server-side re-validation exists (only this
  handler was read).
- Admission `0x45cec0`: build-tree availability (tree virtual `+4`), create the object
  (`0x43d5d0`), push to the ring (`+0xf8` virtual `+8`; on failure the object is deleted),
  tree virtual `+0` reserves the type. A hall upgrade additionally calls tree `+0x30`.
  Nothing is paid at admission.

### 1.4 Build time and progress (V)

- Nominal time: `0x45c690` = `trunc(BuildTime[type] * K * f + 0.5)`, table `0x4bc550`.
- Start `0x40a3c0`: `work = maxHP / time` (signed integer division, truncated), stores the
  price, clears accumulated work and the pause flag, then calls advance once immediately.
- Advance `0x40a400` (once per town update while not paused):
  - building (kind 1): `HP = HP + work` through the object's set-HP virtual `+0xc8`;
    complete when `HP >= maxHP`. Damage therefore delays completion.
  - other objects (units): `accumulated += work`; complete when `accumulated >= maxHP`.
- Duration in ticks is `ceil(maxHP / work)`, which can exceed the nominal time. Example:
  maxHP 0x65 (101), time 7 gives work 0xe (14) and 8 updates.
- No builder unit, worker count or distance appears in this path. There is no per-tick
  gold drain.

### 1.5 Completion (V, `0x459bb0`)

- Building: placed/activated at its own position (virtual `+0xc4`), tree virtual `+0x10`
  moves the type from reserved to completed. Non-defence building: town `+0x17c = 0`, and if
  its period word (`desc+0x100`) is not -1 the town income estimate is recomputed
  (`0x45d160`).
- Hall upgrade (object == town hall): town `+0x170 = 1`, level = `0x45c4f0(type)`, the tree
  is switched to `0x4af130 + (level + race*4) * 0x1188` bytes.
- `0x410010` (building becomes complete): HP is set to maximum, state `+0x68 = 2`, the gold
  accumulator `+0x70` is created.
- The timer is cleared and, in the same call, the next queue head may be paid and started.

### 1.6 Cancel and refund (V)

- Active item: packet 0x21 with argument 2 calls `0x45a880`; argument 1 toggles pause
  (`0x40a570`).
- Done fraction `0x40a490`, in 1/256: building `(HP << 8) / maxHP`; unit
  `(accumulated << 8) / maxHP`.
- Refund = `((0x100 - fraction) * pricePaid) >> 8`, added with `0x4477d0`. The price is the
  amount actually paid at start. Damage to a building under construction increases the
  refund.
- If the cancelled item is a hall upgrade, `0x45dc50` reverts the hall to the type for its
  current level (table `0x4bc390`) and restores the tree. Otherwise the object is destroyed
  (virtual `+0x128`); for a non-defence building town `+0x17c = 0`.
- Waiting item: packet 0x22 with the ring index calls `0x45cfd0`: removes it, releases the
  tree reservation, destroys the object. No money moves (nothing was paid).
- `0x45a880` is also called from `0x45a920` (town changes hands, computer town and human
  counterpart) and from `0x45b1e0` (computer town under human threat, guarded by town
  `+0x180 < 0x10`). V for the calls, G for the exact conditions.
- Destroy mode (packet 0x1a, `0x4489b0`) gives no refund. V(doc).

## 2. Income

### 2.1 Money writers (V, complete list from the corpus)

| Function | Effect |
| --- | --- |
| `0x446870` | side constructor, money = 0 |
| `0x4498f0` | set money; called by the mission loader `0x42ffb0` (and the constructor) |
| `0x4477d0` | add: `+0x698 += n`, `+0x63c += n`. Callers: only `0x410cc0` (income) and `0x45a880` (refund) |
| `0x447d00` | take: fails without change if money < n, else `+0x698 -= n`, `+0x640 += n`. Caller: `0x459bb0` |
| `0x447d30` | read-only "can afford" (`n <= money`) |

No other function in the corpus writes `+0x698`. G: script cash grants, if any, must go
through code outside the corpus or through `0x4498f0`; not found.

### 2.2 Who pays and when (V)

Every completed building runs `0x410cc0` each tick while active. If its gold accumulator
(`+0x70`) exists, `0x42b1e0` adds the stored increment and asks the clock trigger; on a
trigger `0x42b200` returns `(acc + 0x80) >> 8` and clears `acc` only if that value is
non-zero. The payout is `trunc(value * warehouseFactor)` (`0x45c450`), added to the side of
the building's town (`0x4477d0`). The repair accumulator (`+0x74`) is handled right after
and feeds the town repairer (town `+0x128` virtual `+0x38`), not the cash.

### 2.3 Rate channels (V, `0x410010`)

Chosen by descriptor bytes `+0xed` (terrain) and `+0xfc` (schedule); `desc+0x100 == -1`
means no income at all.

| `+0xed` | `+0xfc` | Increment per tick (8.8 fixed) | Clock |
| --- | --- | --- | --- |
| 0 | 0 | `(((d60 << 16) / 100) + 0x80) >> 8` (`0x42afe0`) | periodic, period = `desc+0x100` (`0x45f6b0`) |
| 0 | 1 | same | schedule: `desc+0x100` entries from `desc+0x104` (`0x45f6e0`) |
| 1 | 0 | terrain formula (`0x42b040`) | periodic |
| 1 | 1 | terrain formula | schedule |

Terrain formula (`0x42b040` + `0x42acd0`):
`cA = ((((d60 << 16) / 100) * 2) + 0x100) >> 8`, same for `cB` from d61;
`inc = ((2 * sumA * cA + 2 * sumB * cB) / n) >> 8`, where the sums run over the n mask cells
with value 2 (empty cells count in n). The value is computed once at completion and stored;
the resource map is not read again and is never decremented (no depletion).

Examples: d60 = 0x14 (20), period 0x12c (300): inc = 0x33 (51), payout
`(51*300 + 128) >> 8` = 0x3c (60). d60 = 0x19 (25) gives cA = 0x81 (129).

Clock: day clock `0x4c1968`, wraps at 0x1c20 (7200) (V(doc) `0x4184a0`). The periodic
trigger stores the clock value at creation, so the first payout is one period after
completion, not on a global boundary (V `0x45f6b0`; fire test `0x45f8c0` is trigger virtual
`+0x24` of vtable `0x4948b0`, V for the slot, V(doc) for its body).

Warehouse factor (`0x45c450`, constants read from the executable): 1.0 (`0x4bc3d8`), or
float32 1.3333334 (`0x4bc3dc`) for farm types 0xc/0x27/0x41 (12/39/65) when the town tree
reports a completed 0x12/0x28/0x46 (18/40/70). Mines and halls always 1.0.

Mines and farms are the terrain records (7, 8, 20, 22, 35, 36; V(doc)); scheduled records
are 7, 20, 35 with 6, 10, 8 entries (V(doc)).

Town income estimate `0x45d160` (town `+0x174`): for completed non-defence buildings with a
period, `trunc(0x410580(building) * warehouseFactor)`. V. Note the decompilation assigns
rather than sums; whether it accumulates is not resolved (G).

### 2.4 Repair credit (V unless marked)

- The repair accumulator is created in the Building constructor `0x40fb70`, not at
  completion: only if d62 (`desc+0xf8`) is non-zero, increment
  `(((d62 << 16) / 100) + 0x80) >> 8` (`0x42afe0`), periodic clock with period d320
  (`desc+0x500`). Its clock therefore starts at object creation.
- It is advanced only in the active branch of `0x410cc0`; each trigger passes
  `(acc + 0x80) >> 8` to the town repairer.
- Repairer: credit x 100, spent and cleared every tick, modes 0/1/2 via packet 0x1e,
  defence first. V(doc) `0x4445c0`, `0x444730`, `0x444920`, `0x45d4d0`; not re-read.

## 3. Production queue and service

### 3.1 Town production step `0x459bb0` (V)

Per town update:

1. If the timer holds an item: paused -> return; `advance` not finished -> return;
   otherwise complete it (1.5 for buildings). For a unit: spawn position = `0x45c330`
   (position of a town building whose type is in the unit's prerequisite list, else the
   hall), then `0x40a2d0` spirals to a free buildable tile; if none is found the function
   returns and completion is retried next update (the lane stays blocked). The unit joins
   the town tactic (`0x454090`) and, if town `+0x16c` is not the town itself, is sent to the
   rally target.
2. Queue head = ring virtual `+0x14`(0). Empty (or equal to town `+0x168`): un-pause an idle
   paused timer and return.
3. Prerequisite wait: if tree checking is on (tree virtual `+0x34`), the type is not
   available and the first missing prerequisite id is `< 0x5a` (90), return without paying.
   The item waits at the head; it is not rejected. (This decodes the "`< 0x5a`" test listed
   as unknown in `script_codes_and_ai_purchase.md`: 0x5a is the "none" marker written by
   `0x411ac0`.) V for the markers, G for the exact stack slot Ghidra shows.
4. Price = `0x45c5b0(type)`; `0x447d00(side, price)`. If the side cannot pay, return; the
   head keeps blocking the lane and is retried every update. There is no partial payment.
5. Pop the head. Hall upgrade: the hall morphs (hall virtual `+0x134`), the placeholder
   object is deleted and the hall becomes the timed object. Defence or ordinary building:
   joins the town tactic now. Town `+0x1b4 += price`.
6. `timer.start(object, 0x45c690(type))` with the price recorded; the first work increment
   is applied in the same update.

One serial lane per town; completion and the next start can happen in the same update.

### 3.2 Price and time (V, constants read from the executable)

`price = trunc(UnitPrices[type] * K * f + 0.5)` (`0x45c5b0`, table `0x4bc3e8`, conversion
`0x4810e4`); `time` likewise from `0x4bc550` (`0x45c690`).

| Branch | Price K | Time K |
| --- | --- | --- |
| level 0 (town `+0x15c == 0`) | 1.0 (`0x4bc3e4`) | 1.5 (`0x4bc3e0`) |
| developed, side passes `0x447320(side, -1)` | 0.6667 (`0x4bc3cc[0]`) | 2.0 (`0x4bc3c0[0]`) |
| developed otherwise | `0x4bc3cc[speed]` = 0.6667 / 1 / 2 | `0x4bc3c0[speed]` = 2 / 1 / 0.6667 |

`f` = 0.8 for a computer side (`0x43d2c0`) that fails `0x447320(side, -1)`, else 1
(V(doc) for 0.8). Table heads: prices 100, 160, 250, 450, 750, 500, 280, 800 ...; times
100, 120, 140, 220, 250, 350, 160, 250 ... Types 84, 86, 88 have price and time
0x7fffffff; type 82 is 10000/10000.

Hall type per level and race (`0x4bc390`, V): level 0: 10, 37, 63; level 1: 11, 38, 64;
level 2: 19, 47, 71; level 3: 24, none, 76. `0x45c4f0` maps 11/38/64 -> 1, 19/47/71 -> 2,
24/76 -> 3.

### 3.3 Build tree (V, `0x411ac0`, table `0x4af130`)

- 3 races x 4 levels, each 33 entries of 0x22 (34) dwords (0x88 bytes): dword 0 = type with
  flags, then a prerequisite list ended by -1; the entry list ends with type -1.
- Entry flag 0x4000000: one per town. Completed in this town -> marker -2 (unavailable);
  already queued -> marker -3; otherwise prerequisites are checked.
- Prerequisite flag 0x8000000: use the side-wide completed count (tree `+0x2d8`) instead of
  the town count (tree `+0x16c`).
- A type whose DisabledID byte (tree `+0x2dc`) is set is skipped entirely.
- Tree flag `+0x2e1 == 0` disables prerequisite checking (all listed types available).
- Table contents (type, U = one per town, <prerequisites>, s = side-wide):
  - race 0 level 0: 0 11 19 24
  - race 0 level 1: 0 1<14> 2<14,22s> 6<15> 5<15,16s> 12 13 18U<12> 14U 15U<14> 16U<15> 17<14>
  - race 0 level 2: 0 1<14> 2<14,22s> 3<20> 4<20,21s> 8<20,51s> 9<20,75s> 13 14U 20U 21U<20> 22U<14> 17<14> 23<20>
  - race 0 level 3: 0 1<14> 2<14,22s> 6<15> 5<15,16s> 3<20> 4<20,21s> 85<25> 14U 20U 15U<14> 25U<14,20> 17<14> 23<20> 26<20,21s>
  - race 1 level 0: 27 38 47
  - race 1 level 1: 27 28<41,50s> 29<41,43s> 32<44> 31<44,45s> 33<46> 39 40U<39> 41U 46U 44U<41> 43U<41> 45U<44> 42<41>
  - race 1 level 2: 27 28<41,50s> 29<41,43s> 30<51> 35<51,75s> 87<51,21s> 34<52> 48 41U 50U<41> 51U<41> 52U<51> 42<41> 49<50>
  - race 1 level 3: empty
  - race 2 level 0: 53 64 71 76
  - race 2 level 1: 53 54<67,72s> 55<67,73s> 57<77> 56<77,78s> 65 70U<65> 67U 72U<67> 77U<67> 78U<77> 80 69<67,72s>
  - race 2 level 2: 53 54<67,72s> 55<67,73s> 58<74,75s> 59<74> 61<74,21s> 62<74,51s> 60<79> 66 67U 79U<67,80> 74U<67> 73U<67> 75U<74> 80
  - race 2 level 3: 53<67> 54<67,72s> 55<67,73s> 57<77> 56<77,78s> 58<74,75s> 59<74> 60<79> 67U 79U<67,80> 74U<67> 77U<67> 80 69<67,72s> 81<74,79>
- Consequence: level 0 (outpost) lists only the basic unit and the hall upgrades; farms and
  mines are ordinary repeatable entries; warehouses are one per town and need a farm.

### 3.4 Limits

- No population or supply test exists in admission (`0x45cec0`), start (`0x459bb0`) or the
  tree (`0x411ac0`). V for these three functions. The only caps are: tree availability,
  the one-per-town flag, the ring capacity (50 waiting entries, V(doc)) and money.
- Packet 0x20 sets town `+0x158` (BuildSpeed) directly, with no range check in the handler
  (V `0x4577b0`).

## 4. Unknowns

1. Meaning of side `+0x690` and its `> 0x20` threshold in placement (1.2).
2. Whether the server side re-validates placement (only the packet 0x1c handler was read).
3. Ring capacity 50 and its rejection behaviour: taken from docs, not re-read.
4. Town `+0x168` (queue head that is never started) and the role of `+0x158 == 2` in the
   message branches of `0x459bb0` (looks like voice/notification selection only).
5. `0x45d160`: sum or last value.
6. Exact conditions of the two automatic cancel-with-refund calls (`0x45a920`, `0x45b1e0`).
7. Script-driven money changes: no writer besides the loader `0x4498f0` was found in the
   corpus.
8. `0x447320(side, -1)` ("hostile to everyone" in docs) not re-read.
9. Layer 0 of `0x46c330` (grid `0x515a78`): assumed to be the buildable-terrain layer.
10. Race 1 level 3 tree is empty and has no hall type: assumed unused.
11. Division by zero in `0x40a3c0` if the computed time is 0 (types with BuildTime 0: 10,
    37, 63, 89) is not guarded; those types are presumably never queued.

## Correction (2026-11-21, from docs/ai_tables_and_light.md)

V: build tree byte `+0x2e1 == 0` makes nothing buildable; it does not make everything
available. Admission `0x45cec0` clears it for a hall upgrade and `0x45cfd0` sets it back.
