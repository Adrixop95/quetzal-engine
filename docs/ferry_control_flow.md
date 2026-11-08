# Ferry control flow in the original (decompilation)

Evidence date: 2026-11-08. **V** = read from the Ghidra decompilation of the original
(function addresses only). **G** = inference. This replaces the earlier inference in
`transport_live_observations.md` that boarding needs "a scenario destination for the group";
that inference was too strong (see "What the live runs do and do not show").

## The path of a right click on a ferry

1. **V** The command `PCT_FRAME_ATTACK` (case `0x18` of the logic tick `0x4577b0`) is handled by
   `0x447e00` with the side object, the world position and modifier bits. It finds the object
   under the cursor and, if it belongs to an allied side and has a tactic other than the
   selected one, asks that target tactic for its kind through virtual `+0x80`. For a kind
   other than 1 (an ordinary unit group has kind 1) the selected group receives the order
   `+0xf0(target tactic)`; for kind 1 it receives a plain move. Enemy objects get an attack
   order. **No eligibility test of the selected group appears in this function.**
2. **V** The ferry is a tactic: the vtable `0x48fbd4` is the tactic class (its method list
   contains the tactic assignment and think methods `0x4516b0`, `0x451c30`, `0x452c70`,
   `0x454820`, `0x455a50` and others) and the ferry overrides a few slots: `+0x108` clears a
   flag at `+0xe9` (`0x408c70`), `+0x110` stores a byte at `+0xe8` (`0x408c80`, the
   enable flag set by `boat.activate`/`disable`), `+0x114` is "ready" (`0x408ca0`: phase at
   `+0xf0` equals 0), `+0x118` is the boarding request (`0x408dd0`) and `+0x138` is its
   think (`0x408eb0`). Its kind (`+0x80`) is 2.
3. **V** The approach step of the selected group is the tactic method `0x453570`. With a
   target tactic stored at `+0x98` and the mode byte at `+0x94` set, the group walks to
   the target's anchor (`target + 0x10`). When the distance from the group's anchor tile
   to the target, in the approximate integer norm `0x461fe0`, becomes smaller than the
   group's reach (virtual `+0xc`), and the target's `+0x80` returns 2, then: if bit 2 of the
   group's flag dword `+0x50` is set the group first calls its virtual `+0xdc(0)` and does
   **not** send the request; otherwise, when the target answers ready (`+0x114`), it calls
   `target.+0x118(group)`.
4. **V** The group flag dword `+0x50` is recomputed by `0x4554f0` and `0x4559e0` whenever the
   membership changes: it starts as 5; for every member, if the member's virtual `+0xa0`
   returns false bit 0 is cleared, and if its virtual `+0xa4` (the airborne predicate) returns
   false bit 2 is cleared. So bit 2 means "every member is airborne": an all-air group never
   requests a ferry. Bit 0 means "every member answers true to `+0xa0`" (meaning not
   established).
5. **V** The ferry think `0x408eb0` is documented in `transport_rules.md` (3 by 3 tile scan
   around the ferry, up to five passengers that belong to the requesting tactic, closing,
   travel, unload in rings around the arrival point, turn, return). The ferry is skipped if its
   object is missing or farther than 7 tiles outside the map.

## What the live runs do and do not show

- **V (live)** The scenario squad of three siege units boarded every time (four runs).
- **V (live)** Ad hoc groups made by box-selecting the village units (5, 7, 12 units) walked to
  the shore near the ferries and no unit boarded; the ferry never opened.
- By the code none of these groups is excluded by an eligibility rule: they are ground units,
  so bit 2 of their flag is clear, and the handler at step 1 gives every group the same order.
- **G** The likely reason is the distance condition of step 3 (and the 3 by 3 scan of the ferry
  think): a larger group spreads its units over several tiles around its anchor, stopped by
  trees and by the shore, so the anchor never reaches the required distance or no member stands
  inside the 3 by 3 block of the ferry. The three-unit siege squad fits in the block.
- **Not yet separated:** group size versus unit type versus anchor distance. Decisive
  experiment (headless, no desktop): run mission 2 in the recompiled original and send
  groups of 1, 2 and 3 foot units (and a mixed 3-unit group) to the ferry, and watch the
  ferry phase field (`+0xf0` of the ferry tactic) and the group anchor tile per tick.

## What the clone must do

Implement the request exactly as in steps 3 and 4: the order is a normal right click on an
allied object whose tactic kind is not 1; the group approaches the ferry's anchor; the request
is sent when the anchor is closer than the group's reach, the ferry is in phase 0, and the group
is not all-airborne; the ferry then takes up to five members of the requesting group that stand
inside its 3 by 3 tile block. Do not add a "scenario squad" rule.
