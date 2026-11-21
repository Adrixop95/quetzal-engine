# Economy residual unknowns (static pass)

Date 2026-11-21. Read-only pass over `work/decomp/all_full.c`, `objdump` of `game/Aztec.exe`
and vtable/constant bytes read from the executable. Nothing was run. V = read in code/data
here; G = inference. Numbers hex (decimal). Complements `docs/economy_rules.md`.

## A. The 11 unknowns of economy_rules.md section 4

1. **Side +0x690 = per-side light map** (V). Created by `0x449440` as a byte grid with
   0x30 (48) px cells (one byte per tile), cleared to 0; saved in the savegame `0x449540`.
   At mission start `0x430ae0` fills it with 0x20 (32) (`0x47d460`) and then applies
   `dst = max(dst, A - B)` (`0x47d430` -> `0x480d20`, no underflow) where A/B are the grids
   `0x4d9c0c`/`0x4d9c10`, both loaded from `res\light.xbe` in `0x46a1a0`. Objects test
   their visibility against it (object virtual +0x10, result in byte +0x129). The AI tactic
   path `0x454820` uses the threshold `> 7`. `0x449980` fills the whole map with 0x18 (24)
   for a computer side under option `0x42e8c0(0x2f)` (G: meaning). Placement needs `> 0x20`,
   so a body cell must be brighter than the ambient base (V for the test; G that the extra
   light comes from own units/buildings; the writer that raises cells was not located).
2. **Server-side placement re-validation**: none found. The packet 0x1c path
   (`0x4577b0` -> `0x45cec0` -> `0x410960`) does not call `0x409940`/`0x45bd50` (V for
   those functions). The AI path tests itself (`0x457000`/`0x41e610` call the site selector
   `0x45bf10` and delete the object on failure, V). Whether the net model is lockstep
   (every peer runs the same handler) is G; needs a multiplayer live run.
3. **Ring capacity 50** V: town constructor `0x4594f0` calls `0x4113c0(town+0xf8, 0x32)`.
   On push failure `0x45cec0` deletes the new object (V(doc)); no money moves.
4. **Town +0x168** V: a transient guard. The AI admission paths (`0x41e610`, same code in
   `0x457000`) set it to the just-pushed ring tail (`0x45cfb0(count-1)`) while the site
   selector `0x45bf10` runs, and reset it to 0 right after (on failure that tail is removed
   with `0x45cfd0`). Constructor sets 0 (`0x4594f0`); the savegame stores it as an object
   id (`0x444ba0`). In normal play it is 0 when `0x459bb0` runs, so the skip never triggers
   (G: only matters if production ran inside site selection).
   `+0x158 == 2` in the `0x459bb0` message branches: not re-read (still G: notification).
5. **`0x45d160` sums** V (objdump): `fmul factor; fiadd [town+0x174]; ftol -> [town+0x174]`,
   i.e. `est = trunc(est + yield * factor)` per building, truncating after each addition;
   reset to 0 at entry. Per building `yield = 0x410580`, which always uses the terrain
   formula (`0x42acd0` with d60/d61 coefficients) even for constant-rate buildings (V).
   Called from completion `0x459bb0` and member removal `0x45ed20`.
6. **Automatic cancels**:
   - `0x45b1e0` (CheckCapture, every town update) V: radius r = 8 tiles if town level
     `+0x15c != 0`, else 4. For each non-allied object (`0x4472e0` false) within r: if the
     town's side is computer (`0x43d2c0`), the object's tactic side is not computer and
     town phase `+0x180 < 0x10`: `0x461a30(side)` (alarm side tactics, G), `0x45dd50`
     (release town +0x188), `0x45a880` (cancel the active item with refund), `0x45cf80`
     (empty the waiting ring, no money), phase := 0x10 (16). It fires on presence alone,
     even when the capture itself is blocked. Human towns reset `+0x180 = 0` when no
     capturer is found.
   - `0x45a920` (CaptureTown) V: calls `0x45a880` only if the old owner is computer and the
     capturer's side is not computer. The refund goes to the old owner (town +8 is still the
     old side at that point).
7. **Script money path found** V: script action names `addMoney` -> action 0x1c (28) and
   `takeMoney` -> 0x1d (29) in the parser `0x4379e0`. Executors in an adjacent table slot
   pair `0x49b1cc`/`0x49b1d0` (G for the table base `0x49b15c`): `0x402e10` money += arg,
   `0x402e70` money -= arg, both clamp at 0; arg = action +0x34, side index = action +0.
   They write `+0x698` directly and do **not** update the `+0x63c`/`+0x640` statistics.
   Also `0x434af0` (key handler) adds 0x3e8 (1000) to the local side for code 0x2a when its
   flag argument is set (G: cheat). `StartMoney` goes through `0x4498f0` (loader).
8. `0x447320(side, -1)`: not re-read (budget).
9. Layer 0 of `0x46c330`: not re-read (budget).
10. Race 1 level 3: unchanged (G: unused).
11. Division by zero in `0x40a3c0`: unchanged.

## B. Repairs (V)

- `RepairerThink 0x45d270` only emits notifications (0x79 (121) for mode 1, 0x83 (131) for
  mode 2) for a non-computer side. The work is repairer vtable `0x4947a8` slot +0x44 =
  `0x444730`, called from it each town update (town update `0x45e8e0` order: `0x454820`,
  `0x45d270`, `0x459bb0`, `0x45b1e0`, `0x45d4d0`).
- Credit: slot +0x38 `0x4445a0`: `credit(+0x28) += n * 100`; n comes from the d62/d320
  building accumulator (`0x410cc0`). **Repairs never touch side money.**
- `0x444730`: if credit != 0, buckets list entries by maxHP/HP (object +0x3c / +0x38,
  integer division): defence; >= 4; == 3; < 3. Mode 2 order: >=4, 3, <3, defence; other
  modes: defence first. Credit is cleared after the pass (no carry).
- `0x444920` repair: share = credit / count; amount = min(share, credit, maxHP - HP); heal
  via object +0xd8 with factor 1.0 (`0x4bc6b8`); zero amount removes the entry (+0x34).
- Mode 2 (destroy): credit x3, share = credit / count, amount = min(share, HP), damage via
  object +0x124 with effect `0x46d900(obj, 10)` and factor 3.0 (`0x4bc6bc`), credit / 3 at
  the end. No refund.
- Mode change `0x444b80` clears the list and marks (`0x4445c0`). Capture resets the mode
  (repairer +0x3c from `0x45a920`).

## C. Warehouse factor (V)

`0x45c450`: default 1.0 from `0x48f2ac` (not `0x4bc3d8`; both hold 1.0). For farm types
0xc/0x27/0x41 (12/39/65) it asks tree slot +0x38 = `0x411e40` for 0x12/0x28/0x46
(18/40/70), which returns `town_completed_count[type] (tree +0x16c) != 0`. So the factor is
1.3333334 (`0x4bc3dc`) iff **this town** has at least one completed warehouse (any of the
three ids); not side-wide. The count is incremented by tree +0x10 `0x411d50` at completion
and decremented by tree +0x18 `0x411f00` when the building leaves the town (`0x45ed20`), so
losing the warehouse drops the factor at the next payout. The factor multiplies each payout
in `0x410cc0` after the `(acc + 0x80) >> 8` rounding.

## D. Selling and destruction (V)

- No sell action exists: no "sell" string; the only money writers are those of
  economy_rules 2.1 plus the script executors and cheat above.
- Destroy mode (repairer mode 2, packet 0x1a toggles targets) damages buildings; no refund.
- Member removal `0x45ed20` (building dies or leaves the town): repairer entry removed;
  if it is the hall, `0x45dc50` reverts the hall; if it is the item under construction,
  tree +8 releases the reservation and the timer is cleared **without refund** (price
  lost); otherwise tree +0x18 decrements the completed count. For a non-defence building
  with a period, `0x45d160` recomputes the estimate; `0x45d070` follows (not read).
- Income of a dead building stops because only active buildings run `0x410cc0` (G).

## E. Production start gate with several lanes

- One timer (`town +0x110`) and one ring per town = one lane per town (V `0x4594f0`).
  Within a town the head blocks strictly (prerequisite wait, then the `0x447d00` money
  test; economy_rules 3.1). All towns of a side pay from the same `+0x698`; whichever town
  is updated first in the tick takes the money (G: order = global object update order, not
  read). No reservation between towns, no partial payment.
- Computer towns: strategist runs only when timer and ring are empty (V(doc) ai_phases 0).
- Town constructor sets `+0x17c = 0x32` (50), `+0x158 = 1`, `+0x168 = 0` (V `0x4594f0`).

## F. Capture of a town (`0x45a920`, V unless marked)

- Trigger in `0x45b1e0`: capturer = non-allied object with object +0xa4 false (G: meaning)
  within dist^2 < 5 tiles of the hall (global-list branch; the spatial-query branch's
  distance expression is garbled by Ghidra, objdump needed). Branch choice: spatial query
  when `4*r^2 < [0x4cc3e8]` (object count), else the global list `0x4cc3d0`.
- Blocked if any allied live mobile unit, or allied defence building, is within r
  (dist^2 <= r^2), or if the item under construction is a defence building.
- Effects: town buildings change owner (virtual +0x7c); non-building members go to the old
  side (`0x4474d0(oldSide)` virtual +0xd4; G: they stay with the old owner). Waiting ring
  emptied (`0x45cf80`; nothing was paid). Refund of the active item only in the
  computer -> human case (A.6). Otherwise an active **building** keeps its progress and
  continues for the new owner (old owner's price is lost); an active **unit** is cleared
  and destroyed without refund. Phase `+0x180`, `+0x184` and rally reset; town moves from
  the old side (`0x447220`) to the new side (`0x446fe0`), `town+8 = 0x4dd2f4 + idx*0x69c`;
  tree slots +0x1c/+0x20 rebind (G); BuildSpeed `+0x158 := 1`; selection via `0x4498a0`.
- **No side money is transferred** by capture (no money writer is called besides the
  conditional refund).

## G. Still unknown / needs a live run

- Writer that raises the light map above 0x20 (unit/building light radius): static.
- Net authority for placement: multiplayer live run.
- Update order of towns within a tick (which lane pays first): static, then live.
- Exact "repair finished" condition in `0x444730` (uses a stack value Ghidra lost).
- Spatial-branch distance test in `0x45b1e0` (objdump) and the meaning of object +0xa4.
- `0x461a30` effect on tactics, `0x45d070`, tree +0x1c/+0x20, `+0x158 == 2` messages.
- Items 8 and 9 (not re-read).
- Live checks: +33% payout only while the town's own warehouse stands; capture keeps a
  half-built building for the new owner; script addMoney leaves statistics unchanged.
