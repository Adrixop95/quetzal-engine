# Computer player: target score, plan builder, threat trigger, side plan, throttling

Evidence date: 2026-11-15. Read-only: Ghidra corpus `work/decomp/all_full.c`, one extra
decompile (`work/gaps/dec_45e8e0.c`) and `objdump` of `game/Aztec.exe` for hidden stack
arguments. Nothing was run. **V** = read at the cited address, **G** = inference.
Fixed point is 24.8 ("8.8"): raw 0x100 (256) = 1.0.

## 0. Fixed-point helpers (all V)

`0x402fa0` copy; `0x402fb0` int -> fixed (`<< 8`); `0x402fc0` add; `0x403000` multiply
(`a*b >> 8`); `0x403020` divide (`(a << 8) / b`); `0x44df50` divide in place; `0x404810`
multiply in place; `0x4047e0` assign; `0x4047f0` add in place; `0x406d80` subtract in place;
`0x44df30` double -> fixed (times 256.0 at `0x4944b8`); compares `0x44df90` (a < b),
`0x44dfa0` (a == b), `0x44bcf0` (b <= a), `0x403050` (b < a), `0x406da0` (a <= b).
`0x461fe0` = octagonal distance approximation of (dx, dy).

## 1. Static tables (built once by `0x44dc10`, called from `0x44dbe0`; V)

90 (0x5a) object types; one prototype per type is created for the measurement.

- **Weight `W[t]`** at `0x4cec14` (90 dwords): virtual `+0x3c` of the prototype divided by
  the maximum over all types, so 0..1.0. Slot `+0x3c` is the maximum hit points getter
  (existing note `work/gaps/desc_other_kinds.md`), so W = relative toughness.
- **Matrix `M[a][b]`** at `0x4ced7c` (index `a*90 + b`, 8100 dwords). Let
  `k = 0x44df10(a, b)` = kill time `0x46fac0(a, b) + 1`, or 0x400 (1024) when a cannot
  attack b. If a is a building (kind 1): raw 1. Else if `k <= 0x100`: raw `256 - k`
  (= `1 - k/256`). Else: raw 1 when b is a building, otherwise 0. So M is "how fast a
  kills b", 0 = useless.
- **Type lists**: `0x4cd6d0` (count `0x4cd6cc`) = all non-building types, filled from type
  89 down to 0; `0x4cd838`/`0x4cd6c0` = defence buildings; `0x4cd9a0`/`0x4cd6c4` and
  `0x4cdb08`/`0x4cd6c8` = other buildings.
- **Per development level list** at `0x4cdc70`: 11 rows of 0x5b (91) dwords, row L = count
  followed by the non-building types whose required level `0x45c920(t)` (table `0x515580`)
  is <= L. Types with requirement >= 11 (0xb) are in no row.

## 2. Target score `0x44cd30` (V, from disassembly)

Hidden signature: `0x44cd30(out, record, town)`; called by `0x44cfe0` for every town of the
side (list side `+0x660`) and every intelligence record (list side `+0x68c`) whose owner is
not allied (`0x4472e0` false).

```
D = dist(town pos, record object pos) / dist(map width, map height)     (0x461fe0 both)
F = sum over unit types (W[t] * count[t]) / sum(count[t])    (0 when the record is empty)
G = min(sum(count[t]) / 256, 1.0)
if record owner is a computer side and not "all enemies" (0x43d2c0, 0x447320(side,-1)):
    D = D * 3.0                                              (0x300, at 0x44cefa)
score = D + 0.5 * F + 0.5 * G                                (0.5 = double at 0x44cf4c/7b)
```

- `count[t]` is record dword `t + 1` (90 counts at record `+4 .. +0x168`); positions from
  virtual `+0x94`; map size from `DAT_00515428` virtuals `+4`, `+8`.
- Correction to the older note: the 0x300 is a **multiplier of the distance term**, not an
  multiplied by 3.0 (corrected; an earlier note said added).
- Oddity, V: the loop walks position i of the unit-type list but tests record dword i
  (not dword `type+1`) for non-zero before adding type `list[i-1]`. G: an original
  off-by-index; some unit types are skipped or counted depending on another type's count.
- Null record: score 0x7fff00.
- Selection `0x44cfe0`: per town, start 0x7fffff00, keep the record with the **lowest**
  score (near, weak, small, preferably human or all-enemies owner); if one was found call
  town virtual `+0x150` (G: stores the target, town `+0x184`). Runs for every town on every
  side update (see part 5).

## 3. Plan builder `0x44d130` (V unless marked; inner loops from disassembly)

Hidden signature: `this` = side, stack args `(record, town, mode byte)`. Returns the plan.

Setup:
- Plan object from pool `0x4d9b88`, constructor `0x456da0(plan, record object, tactic)`;
  the tactic is new (`0x4474d0`) unless the side is all-enemies. `L` = highest town
  development level of the side (`0x447290`, town `+0x178`, 0..9).
- Reusable units: for each tactic of the side (list `+0x654`) that is not farther than 25
  tiles (`0x44db80`: `2*dist > 0x32` rejects; G on which two points), not locked (`+0xcc`
  != 2), has `+0xb0`, state `+0x20` 0 or 1, and not all of `+0x94/+0x9c/+0x9d`: every
  active non-building member is stored by type, **max 0x20 (32) per type**. When the mode
  byte is non-zero the same is done for members of the side's towns (list `+0x660`).
- `total` = sum of the record's 90 counts.

Main pass, for enemy type `e` = 89 down to 0 with `count[e] != 0`, while `n < total`
(`n` = entries added so far):

1. Pick the best own type `t` from row L of `0x4cdc70` (walked last to first), skipping
   `M[t][e] == 0` and type 0x21 (33):
   - a reusable unit of type t exists: accepted when no best yet or
     `M[best][e] <= M[t][e]`; best score := `M[t][e]`, reuse flag on (no random term);
   - otherwise `s = M[t][e] * W[t] + (rand() & 0x3f)` (0..63 raw, up to 0.246); needs
     `s >= best score`; then the town's build filter (object at town `+0xf4`, virtual
     `+4(t, &out)`) decides: buildable and (no best yet or `s > best score`) -> best := t,
     reuse flag off; not buildable and out index < 90 -> remember t as the wanted type.
     V: the "best wanted score" variable (`[esp+0x58]`) is assigned to itself at `0x44d701`
     and stays 0, so the **last** unbuildable candidate with s > 0 wins.
2. No best -> next enemy type. Otherwise
   `need = 2 * count[e] * M[e][best] - sum over k (M[k][e] * picks[k])`
   (`picks[k]` = entries of type k added so far in this call, all enemy types together).
   `need <= 0` -> next enemy type.
3. **Batch**: `b = rand() % (L + 1) + 1` (1..10). Add up to b entries of type best while
   (`need > 0` or `n < total`): each adds one entry (`0x456f20` new request, or `0x456f50`
   existing unit when reusing), `picks[best]++`, `n++`, `need -= M[best][e]`. If the
   reusable stock runs out mid-batch the pick is redone (jump to step 1; `picks` was already
   incremented, V).
4. After the batch: if `need > 0` and `n < total` go to step 1 again for the same e, else
   next e.

After the loop: if a wanted type was remembered, town virtual `+0x15c(type)` (G: writes the
recommendation town `+0x18c`, 90 = none, read by phase 10).

Second pass (not limited by `total`): for each type k with `count[k] != 0` whose prototype
virtual `+0xa4` is true (G: airborne), choose own type t from row L with side byte
`+0x5e0 + t` set (type available to the side, refreshed in `0x449980`) maximising
`M[t][k]`; after each accepted candidate the bar becomes `M[t][k] + (rand() & 0x1f)`. Then
add `count[k]` entries of t (reuse first, else new).

Plan storage and ranking:
- Entries are 8 bytes at plan `+4`: `{kind, value}`; kind 1 = build request (value = type,
  `0x456f20`), kind 0 = existing unit (value = pointer, reference count `+0x34`++, unit
  moved to the plan tactic and the tactic locked, `0x456f50`). Count at `+0x804`,
  **max 0x100 (256)**; town at `+0x80c`; done flag `+0x810`.
- There is **no sorting**: entries stay in insertion order (enemy type 89 -> 0, then the
  second pass). Ranking exists only in the candidate choice above.
- Allocation `0x457000` scans entries from the last to the first; a dead reused unit is
  turned back into a build request of its type; each eligible town (empty queue, no
  production, phase 0, 2, 0xe or 0x16) takes requests it can build. No money test.
- Plan ends with `0x446fc0` (registers in side list `+0x66c`) and tactic state `[5] = 4`.

## 4. Threat trigger

### `0x45b1e0` "CheckCapture" (V), called from the town update `0x45e8e0`

- Radius `r` = 8 tiles when town `+0x15c` != 0, else 4; compared as squared tile distance
  (64 or 16). Scan method: if `4*r*r < DAT_004cc3e8` (object count; 256 or 64) use the
  spatial query `0x46ff20` (centre and radius times 0x30, max 256 results), otherwise walk
  the global object list `0x4cc3d0`.
- **Threat**: one active object inside r is enough, with: not allied to the town's side
  (`0x4472e0` false), town's side is a computer (`0x43d2c0`), the object's owner side is
  **not** a computer, and town phase `+0x180` < 0x10 (16). Then: `0x461a30(side, town)`
  (every tactic of the side that is not locked, has `+0xb0` and is not fully flagged gets
  virtual `+0xf4(town, 0)`; G: recall to the town), `0x45dd50` (drop the town's own plan
  `+0x188`), `0x45a880` (cancel current production), `0x45cf80` (empty the queue),
  phase := 0x10. No count threshold, no timer; a computer-owned intruder never triggers it.
- **Capture candidate**: a non-allied object with virtual `+0xa4` false (G: not airborne)
  at squared distance < 5 (dx*dx + dy*dy <= 4). **Defended** when any active allied object
  inside r is a non-building or a defence building (virtual `+0xfc`); in the spatial branch
  this is searched in the side's object list `+0x678`. Candidate and not defended and the
  current production (`+0x110`) is not a defence building -> `0x45a920(town, intruder)`
  (capture).
- End: when no capture candidate was found and the town's side is **not** a computer,
  phase := 0. For a computer side the phase is not reset here.
- Leaving the threat phases (phase 16 handler `0x45e3e0`, existing table): `0x44cc00`
  returns true when any active object within 8 tiles (squared <= 64) has side field
  `+0x40` different from the town's side (allies included, V); false -> plan dropped,
  phase 0.
- `0x461a30` is also called from `0x45ece0` (computer side, G: town attacked callback).

### `0x461180` (V) is not a threat function

Hidden signature `side.0x461180(town)`, only caller `0x45b5f0` (DevelopScience) when town
`+0x15c` == 0. It picks the specialisation building for an undeveloped town:
- fails if any other town of the side with `+0x15c` == 0 has `(+0x190) + (+0x194)` larger
  than this town's sum, or is in phase 4 (so one town at a time, the largest first; the
  two fields are unnamed);
- counts the side's towns by `+0x15c` value 1, 2, 3 per race (`+0x164`, 3x3 table);
- walks the town's preference list at `+0x19c` (terminated by -1) and takes the first
  value whose count for this race is 0; default 2;
- queues and places the building: value 1 -> type 0x0b / 0x26 / 0x40 by race, fallback
  0x13 / 0x2f / (race 2: none seen); value 2 and 3 -> other types (branch not fully read;
  0x2f, 0x26, 0x4c appear in the tail via `0x461950`).

## 5. Side plan at side `+0x694`

- V (all code references to `[reg+0x694]` listed by disassembly): writers are only the
  side constructor `0x446870` (0 at `0x44690c`), the side **loader** at `0x446c41` (dword
  read with `0x445030`) and the load fix-up `0x4473a0` (`0x444f00` turns the saved id into
  a pointer). Readers: save `0x44960e`, side update `0x449a51/64`, phase 0 `0x45ddaf`.
- So **no runtime code creates a side plan**: it exists only when a map or save file
  carries one. In a fresh game it is 0, phase 0 always goes to phase 10, and the
  `0x457000` call in `0x449980` never runs. (G caveat: a write through a computed pointer
  would not be found by this search.)
- The `(side index + 1) * 1000` period is **not** a plan rebuild. V `0x449980`: flag =
  `side ticks (+0x13c) % ((side +4 + 1) * 1000) == 0`, passed to `0x432050`. With the flag
  (or when list `+0x68c` is missing) `0x432050` allocates a fresh tile map (`0x47d3d0`,
  filled 0x28), clears the 90 counters at side `+0x478`, creates the record list if needed
  (pool item 0x16c bytes, 0x400 per block) and then feeds every object of list `0x5156f8`
  through `0x431e00`; without the flag it only does the feeding pass with the existing map
  `+0x690`. Periods: side 0 = 1000 (0x3e8), side 1 = 2000, ... side 7 = 8000 ticks.
- Town plans (`+0x188`) are created only by the phase handlers 15, 1 and 23 via
  `0x44d130`.

## 6. Throttling (V)

- Simulation step in `0x4577b0`, one pass per logic tick, in this order: effects list
  `0x4c1978` (`+0x60`), list `0x4cd698` (`+8`), objects `0x5156f8` (`+0x138`), **all sides**
  `0x449980` (stride 0x69c), **tactic pool `0x4d9b50` (`+0x138`)**, then `0x4cc3d0`.
- Towns live in the tactic pool; town vtable `0x494648 + 0x138` = `0x45e8e0`. So the town
  update runs **once per town per logic tick**, with no divider: `0x454820` (tactic think,
  own timer), `0x45d270`, production `0x459bb0`, CheckCapture `0x45b1e0`, `0x45d4d0`,
  owner bookkeeping, then the strategist `0x45e7c0` unless flag 0x2f is set.
- Strategist: every tick it recomputes the speed and increments counter `+0x17c` (unless
  -1); at most **one phase handler per tick**, and only when the town has no production
  and an empty queue. So the counter unit is logic ticks (50 start, phase 10 limit
  `100 * (dev + 1)` ticks).
- Side update, every tick for computer sides without flag 0x2f: on tick 1 fill the map
  `+0x690` with 0x18; refresh the type-available bytes `+0x5e0` when side byte `+0x138`
  was set; progress the side plan if any; intelligence pass `0x432050` (every tick,
  full rebuild on the period above); target selection `0x44dbe0` -> `0x44cfe0` (every
  tick, all towns x all records).
- Sides are updated before towns in the same tick, so a target chosen in a tick is seen
  by the strategist in that tick.

## 7. Unknowns

- Real-time length of a logic tick (the pacing code at the top of `0x4577b0` was not read).
- Town virtuals `+0x150` and `+0x15c` (assumed target `+0x184` and recommendation `+0x18c`).
- The build filter object at town `+0xf4` (virtual `+4(type, &out)`): what the out index is.
- Exact operands of the 25-tile test `0x44db80` in `0x44d130` (decompiler shows a stack
  address as one coordinate).
- In the spatial branch of `0x45b1e0` the capture distance uses the candidate variable as
  the x base in the decompilation; not checked in disassembly.
- Town fields `+0x190`, `+0x194`, and the value 2/3 branches of `0x461180`.
- Whether any shipped map stores a side plan (`+0x694`); file data not inspected.
- The index oddity in `0x44cd30` (record dword i vs type list) is read from disassembly
  but its practical effect depends on the type order, not evaluated.
- `0x431e00` (what one intelligence feeding step records) not read.
