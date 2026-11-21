# World view: mouse commands, selection, cursors, placement preview (original Aztec.exe)

Read-only pass, 2026-11-21, about 45 tool calls. Sources: `work/decomp/all_full.c`,
`work/gaps/dec_00448b40.c`, objdump (`work/gaps/dis_435bc0.s`, data at `0x4bc900`).
V = read in code or data, G = inference. Numbers hex (decimal). Builds on `interface_ingame.md`.

## 0. Key finding: the world view is not a region of list 0x515530

- V `0x435bc0` (game-thread input pump): drains the 64-entry mouse event ring `0x4c4f60`
  (0x14 (20) bytes per event, read index `0x4c5484`, write `0x4c5480`), forwards each event to
  `0x4353a0` (panel regions), and then runs its **own world-view logic** in the same function.
  Commands are written as 0xd4 (212) byte records into the local command ring
  `this+0x28 + idx*0xd4`, idx `this+0x3528 & 0x3f` (mutex `0x515568`). These are the
  FRAME_* packets of the `0x4577b0` switch (docs/specials_commands.md).
- V: event bytes are (Lprev, Rprev, Lnow, Rnow, dbl): in `0x4355d0` the call passes
  `0x4c5488/0x4c5489` (previous, copied after the call) and `0x4c4f4a/0x4c4f49` (current).
  So in `0x4353a0`, slot `+0x30` is **left button released** and `+0x2c` left pressed; `+0x38`
  right released, `+0x34` right pressed. interface_ingame.md sec. 6 has these swapped (G for
  the slot names, V for the argument order).
- G (strong): `0x5154e9 != 0` means the interface **is shown**, the opposite of
  interface_ingame.md. Reasons: `0x435bc0` and `0x464380` use the whole view rect as world when
  `0x5154e9 == 0`, and test the minimap only when it is non-zero; `0x4353a0` dispatches to the
  panel regions only when it is non-zero (or a modal is up).
- World hit area (V `0x435bc0`, `0x464380`): interface hidden: rect (0,0)-view size
  (`0x412a90(&0x4dd300)`). Interface shown: rect (0,0)-(screen_w - 0xcc (204), screen_h), minus
  the corner of 0xfe x 0xd0 (254x208) at the bottom right of the view; outside that, the event
  goes to the minimap path (sec. 1.6). World point = screen - `0x4dd338/0x4dd33c` + camera
  `0x4dd330/0x4dd334` (V).

## 1. Left and right buttons in the world

### 1.1 Press / drag / release (V `0x435bc0`, 0x43657b..0x436754)
- Press of either button: stores screen anchor `this+8/+0xc` and world anchor `this+0x10/+0x14`.
- Drag: while a button is held and the anchor is set, if |dx| > 0x10 or |dy| > 0x10 (16 px,
  strictly greater, screen pixels) then `0x4c548c = (left held)` and the rubber band flags
  `0x4dd384 = 0x4dd385 = 1` are set when the left button is the one held (V 0x4365cb).
  So a right drag never becomes a box (`0x4c548c` stays 0, V 0x4365db: setne on the left byte).
- Release (any change that is not a new press): one record is queued, then the anchor is
  reset to -1 and `0x4c548c = 0`:
  | Case | Packet | Payload |
  |---|---|---|
  | left release, no drag, `0x4cc7d4 == 0` | 0x16 SELECTGROUP | world point, flags |
  | left release, no drag, `0x4cc7d4 != 0` | 0x1a "DESTROYHERE" | world point, flags |
  | right release, no drag | 0x18 ATTACK | world point, flags |
  | left release after drag | 0x15 CREATEGROUP | rect (min x, min y, max x, max y) of world anchor and release point, flags |
  | `0x4c548c` not 0/1 | 0x19 (no handler) | unreachable here (V: only 0/1 written) |
- Flags = `0x42ea70()` (bit 1 Shift/Add, bit 2 Ctrl/Inv, 0x40 town list), V.
- Right press while `0x4cc7d4` is set clears it (`0x440930`) before anything else (V):
  right click cancels the repair-target mode.
- Double click: the flag is forwarded only to region slot `+0x44`; the world branch never reads
  it (V). No "select all of a type" exists in `0x435bc0`, `0x448520` or `0x448b40` (V for
  these three; G that it does not exist elsewhere).

### 1.2 Click select, packet 0x16 -> `0x448520(side, point, flags)` (V)
- Candidates: `0x46fe10(point)` -> `0x46b230`: objects of kind 0 (unit) or 1 (building)
  with `+0x5c` set whose rect (vt `+0x1c`) contains the point (half-open), scanned over the
  cells around the point (bucket grid `0x515a40`, tile 0x30 (48)). First match in scan order wins.
  Ownership: object vt `+0x44` == side id `side+4`.
- Plain click: first own object whose group (`+0x30`, or its carrier `0x440d00(+0x98)` when
  `+0x94,+0x9c,+0x9d` are set, G: unit inside a transport/building) differs from the current
  selection `side+0x688`: calls group vt `+0x10c(side)` and selects that group (`0x4498a0`).
  A click selects the **whole group (tactic)** of the unit; buildings select their town (G:
  building `+0x30` is the town). Clicking an object of the already selected group does nothing.
- Shift (bit 1) only, selection exists: the first own non-building (kind != 1) object not in the
  current group is added with `selection vt+0xd4(obj)`. Shift adds one unit per click.
- Ctrl (bit 2) only: first own non-building object -> new group `0x4474d0(side)`, add the
  object, select it (detach one unit into its own group).
- Shift+Ctrl: nothing.
- With flag 0x20 (minimap) and Ctrl: nearest object within 0xf0 (240) px (`0x470010`) whose
  tactic (vt `+0xa0`) is this side's -> select its group. Minimap without Ctrl: nothing.

### 1.3 Box select, packet 0x15 -> `0x448b40(side, rect, flags)` (V)
- Candidates `0x470220(rect)` -> `0x46b360`: kind 0 or 1, `+0x5c` set, object rect strictly
  overlapping the box (any overlap, not centre inside).
- Without Shift or with no current selection: new group; every own object with kind != 1
  is added (vt `+0xd4`). **Buildings are never box-selected**; no unit/building priority is
  needed. If at least one was added the new group is selected (`0x4498a0`), else the empty
  group is freed (`0x447050`).
- Shift with a selection: own units not already in the current group are collected,
  `0x447800(list, flags)` (G: removes them from their old groups), placed in a new group which
  is then merged into the current one via vt `+0xf4(current, 1)` (G: merge); empty -> freed.
- Only own units (side id test), V. No unit-count cap seen.

### 1.4 Repair-target click, packet 0x1a -> `0x4489b0` (V)
- `0x4cc7d4` is toggled by packet 0x1e (case in `0x4577b0`, corpus line ~48326) together with
  the town repair object `town+0x128` vt `+0x3c(mode)`. With a town selected whose `+0x128`
  vt `+0x40` != 0, the first object under the point that is own, kind 1 (building), belongs to
  this town (`+0x30`) and is not the town centre (`+0x110` vt `+4`) is added to the repair list
  (vt `+0x34`) unless already there (vt `+0x30`). G: the packet name DESTROYHERE is a leftover;
  the effect is "repair this building".

### 1.5 Right click, packet 0x18 -> `0x447e00(side, point, flags)`
- V: queued on right release without a box, world point + `0x42ea70` flags; on the minimap the
  point is converted to world (sec. 1.6) and flag 0x20 is set. The target object is chosen
  inside `0x447e00` from the point (see docs/specials_commands.md); `0x435bc0` passes no object.
- A right drag is treated as a right click at the release point (V, sec. 1.1).

### 1.6 Minimap path (V 0x43618b..0x436754)
- Rect: origin `0x4dd348/0x4dd34c`, size map_w/h (map vt `+4/+8`) times scale `0x4dd3c4`;
  only when the interface is shown and no box drag is active.
- World = (p - origin) * (0x30 / scale). Left held: centre the camera there (clamped as in
  key Center), clears follow `0x4dd3c8`. Right release: 0x18 with flags | 0x20. Left release:
  0x25 FRAME_NOTHING.

### 1.7 Building placement mode `0x4dd3c9` (V `0x435bc0` else-branch)
- Each event calls `0x462650` (validity). Left **press** inside the world area with
  `0x4c5478 != 0` queues 0x1c BUILD (tile x = world x / 0x30, tile y = world y / 0x30, type
  `0x4c5470`) and leaves placement mode; right press leaves it. Invalid left presses are ignored.

## 2. Cursor selection `0x464290(x, y)` (V), drawn by `0x464550`

Returns a sprite index into the view sprite bank `0x4dd300+0x340d8 + idx*4`; animated
(frame `0x5159fc` advances every 0x50 (80) ms through the sprite's frame count).
| Condition (checked in order) | Index |
|---|---|
| not in game (`0x429060`) or no world `0x4dd2f4` or `0x4dd32e == 0` | 1 |
| modal up (`0x4296b0`) or placement mode `0x4dd3c9` or cursor outside world area | 4 |
| edge scroll active (`0x5159e0` = view `+0x380d8`, copied only when no scroll key is held) | table `0x4bc904[flags]` |
| in world: table `0x4bc944[0x447c30(side, world point)]` | see below |

Edge table `0x4bc904` (flags 4 = horizontal, +1 right; 8 = vertical, +2 down), V data:
up 8 -> 0xe (14), up-left 0xc -> 0xf (15), left 4 -> 0x10 (16), down-left 0xe -> 0x11 (17),
down 0xa -> 0x12 (18), down-right 0xf -> 0x13 (19), right 5 -> 0x14 (20), up-right 0xd -> 0x15 (21);
other entries 4.

Hover class `0x447c30` -> table `0x4bc944` = {0, 2, 3, 4, 5} (V):
- 3 -> cursor 4 (default) when: a drag box is open (`0x4dd374/78 != 0x4dd37c/80`, G: box
  rect), no selection, no visible object, or the object is own/allied.
- Repair mode `0x4cc7d4` with a town selected: town `+0x128` vt `+0x40` = 2 -> class 1 ->
  cursor 2; any other non-zero -> class 4 -> cursor 5; 0 -> cursor 4. Independent of the target.
- Otherwise the first object under the point (`0x46b580`: kind 0/1, `+0x5c` set, `+0x5e`
  clear, visible for light mask `side+0x690` via vt `+0x10`): `0x4472e0(side, owner +0x40)`
  true (own, or bit in alliance mask `side+0x648`; owner -1 only when the mask is -1) -> class 3;
  false -> class 0 -> **cursor 0 (attack)**.
- So: there are no separate move / enter / forbidden / garrison cursors in the world view; the
  default cursor 4 doubles as move (V for the table, G for the visual meaning of 0, 2, 4, 5).
  Index 2 is never produced (class 1 only from repair mode 2), so 2 and 5 are the two repair
  mode cursors (G: manual repair / auto repair).

## 3. Status / hint line
- Not found. Panel button enter/leave (`0x412600`/`0x412640`, vt `+0x24/+0x28` of e.g. vt
  `0x493d20`) only set highlight `+0x26`, invalidate the rect (`0x411720`) and redraw
  (`0x429710(1)`); no text is set (V). `0x464290`/`0x447c30` set no text (V). G: the game has
  no hover hint line; status text is only the message box `0x429200` (e.g. Pause).

## 4. Building placement preview (V, view draw function around corpus line 58913)
- Drawn when view `+0xc9` (= `0x4dd3c9`) is set: calls `0x462650` again, takes the building
  sprite `view+0x100d4 + type_vt+0x80 * 4` and draws it cut into a 7x7 grid of 0x30 (48) px
  cells (x loop to 0x150 (336)), placed at the mouse tile with offset 0xa8 (168) = 3.5 cells
  minus the sprite hotspot.
- Each cell uses blit `0x47b0b0` with colour remap table `mode * 0x400` from `0x478460`
  (side colours `0x473750`): cell value 1 -> mode 0x15 (21), 0 -> 0x14 (20), 2 -> 0x16 (22).
  Grid `0x5158d8` (49 dwords, cleared by `0x462650`).
- Grid values come from `0x409940` via `0x45bd50(town, type, tx, ty, grid)`: footprint cell
  (type `+0x64` footprint value 2) -> 1 if buildable, 0 if not; non-footprint cell -> 2.
  If the type needs an adjacency flag (type `+0x64` `+0xf0/+0xf4`, G: water/coast) and none is
  found, all 49 cells become 0 and the result is invalid.
- Per-cell buildable (V, partial): inside radius `param_7` of the town (squared distance),
  no object on the map cell, terrain test `0x46c330`, no building (kind 1) on the 4
  neighbouring cells, light/explored test with mask `side+0x690`.
- Result `0x4c5478` = all footprint cells OK (V); placement needs the selection to be a town
  (vt `+0x80 == 1`), else placement mode is cancelled (`0x462650`, V).
- G: 0x15 = normal/valid tint, 0x14 = red/blocked, 0x16 = faint outside cells. The actual
  colours of remap tables 20..22 were not read.

## 5. Extra seen
- Add key (action 2) held: the selected group's route is drawn with arrow sprites, direction
  index into table `0x4bc968` (sprites 0x80..0x87) from the step delta (V, same draw function,
  G: waypoint path display).

## 6. Unknowns
- Visual content of cursor sprites 0, 2, 4, 5, 14..21 and of remap modes 0x14..0x16.
- Exact view size returned by `0x412a90` and the panel/minimap geometry when the interface is
  shown (0xcc vs 0xfe widths).
- `0x447800` and vt `+0xf4` semantics in Shift box select; group vt `+0x10c` on click.
- Where the rubber band `0x4dd374..0x4dd380` is written and drawn.
- Full condition list of `0x409940` (height byte <= 0x20 test, `param_12/13` flags).
- Inside `0x447e00`: how the target object under the point is picked (documented elsewhere).
