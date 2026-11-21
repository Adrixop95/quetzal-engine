# Interface geometry: side panel, minimap, tooltips, messages, menus (original Aztec.exe)

Read-only pass, 2026-11-21, about 50 tool calls. Sources: `work/decomp/all_full.c`, `dec2.sh`
output (`work/gaps/dec_<addr>.c`), `dec_41eef0.c`, objdump of `game/Aztec.exe`
(vtables, `.data`), `Res/strbase.bds` (English strings by ordinal).
V = read in code or data, G = inference. Numbers hex (decimal). Builds on `interface_ingame.md`,
`worldview_mouse_cursors.md`, `campaign_menus_saves.md`.
Coordinates below are for the 800x600 mode unless stated. W, H = view size (`0x412a90`).

## 0. Corrections to earlier notes

- V: vtable slot `+0x5c` of panel buttons is **not** a grey-out/highlight hook. It is the
  **tooltip getter** `(x, y, &anchor) -> char*`: if the point is inside the button rect it writes
  the button's top-left to `anchor` and returns a string; otherwise it forwards to the next
  sibling (`+4`). Sec. 3.
- V: `0x5154e9 != 0` means the interface is shown (`0x464190` and `0x469e50` draw the panel only
  when it is non-zero). Confirms worldview_mouse_cursors.md sec. 0.
- V: the 0x1f ATTACKMODE values, from the tooltips: 2 = Aggressive (226), 1 = Defensive (227),
  3 = **Balanced** (228), 0 = **Direct Control** (229). interface_ingame.md had 0/3 swapped.
- V: the two 0x1e buttons are **Manual Repair** (230, value 1, vt `0x493970`) and **Manual
  Destroy** (231, value 2, vt `0x4939e4`), not AutoRepair/Repair. So packet 0x1a "DESTROYHERE"
  is used for both repair and destroy target clicks. The mode is town `+0x128` vt `+0x40`, and
  the mode value picks which list is used (G).
- V: the `0x195/0x196/0x197` values in the panel builder `0x43faf0` are **y coordinates**
  (405/406/407), not sprite groups.
- V: the panel is L-shaped. It has a full-height strip 0xcc (204) wide at x = W-0xcc and a bottom
  block 0xfe x 0xd0 (254x208) at (W-0xfe, H-0xd0). Mouse and tooltip use panel-local
  x = x - (W - 0xfe) (`0x4353a0`, `0x464190`).
- V: `0x515844` (getter `0x462f80`, setter `0x462f90`) is the display-mode index into the (w,h)
  table `0x515510` (index -1 gives 640x480 = 0x280 x 0x1e0). Panel art switches on
  `0x515844 != 0` (G: 0 = 800x600, other = 1024x768).

## 1. Side panel layout (V `0x43faf0`, draw `0x469e50`)

Draw order in `0x469e50` (full panel redraw; it runs when the panel is dirty or
`0x515a04 == 0`, otherwise only the top strip is re-blitted):
| Item | Position (screen) | Source |
|---|---|---|
| Top strip sprite 0x125 (293), or 0x124 (292) when the mode is not 0 | (W-0xcc, 0) | view bank `+0x340d8` |
| Panel regions (list `view+0x24`, vt `+0x54` draw) | per region | |
| Animated emblem `view+0x345d4`, frame = game time `0x4c1968` / 0x78 (120) | (W-0x76 (118), 0x120 (288)); y 0x14e (334) when the mode is not 0 | G: day/sun clock |
| Lower block sprite: 0x127/0x126 (295/294) with no selection or a squad, 0x129/0x128 (297/296) with a town | (W-0xfe, 0x188 (392)); y 0x1e0 (480) when the mode is not 0 | then squad panel `0x4cc7e0` or town panel `0x4cc7dc` vt `+0xc` |
| Minimap `0x464ba0` | sec. 2 | |
| Dirty rect: the corner between (W-0xfe, H-0xd0) and (W-0xcc, H) | | vt `+0x20` |

Regions built by `0x43faf0`. All x values are panel-local (screen x = W-0xfe + x); y is screen y
in 800x600. Constructor arguments: (x, y, rows/cols, cell w, cell h, 0x21, 0x3b, ...).
| Global | vtable | Rect / grid | Click (+0x10) | Tooltip (+0x5c) |
|---|---|---|---|---|
| `0x4cc7ec` root | `0x493d94` | (0x32, 0) 0x3c x 0x16 (50,0, 60x22) | `0x440650` | `0x440660` returns 0 |
| `0x4cc7e8` town list | `0x493e08` | (0x48, 0xda) = (72, 218), 1 column x (`0x5154d4`+4) rows of 0x3b x 0x2c (59x44) | `0x402a10` | `0x440d30` (G: 211/212 Scrolls towns up/down) |
| `0x4cc7e4` team list | `0x493e80` | (0xae, 0xda) = (174, 218), same grid | `0x402a10` | `0x440fa0`: arrows -0xf -> 213 "Scrolls teams up", -0xe -> 214 "down" (anchor at the bottom), entry -> the entry's own `+0x28` |
| `0x4cc7e0` squad panel | `0x493acc` | grid (0x1a, 0x1cf) = (26, 463), 4x3 cells 0x2f x 0x2a (47x42) | `0x442ad0` | `0x442ae0`: arrows 223/224 "Scrolls command list up/down"; cell n -> name (vt `+0x84`) of the n-th member of the selected group |
| `0x4cc7dc` town build grid | `0x4937f4` | grid (0x18, 0x1cf) = (24, 463), 3x3 cells 0x2a x 0x2a (42x42) | `0x441a50` | `0x441ba0`, sec. 3 |
| `0x4cc7d8` build queue | `0x493878` | grid (0xc0, 0x1cf) = (192, 463), 1x3 cells 42x42 | `0x442420` | `0x4424f0` -> 222 "Building Queue" |

Buttons (rect = top-left + size, panel-local x, screen y):
| Global | vt | Rect | Click | Tooltip hook -> ordinal |
|---|---|---|---|---|
| `0x4cc794` | `0x493b50` | (0x0e, 0x196) 0x3b x 0x2c = (14, 406, 59x44) | `0x442da0` centre camera | `0x442f10` -> 225 "Head Button" |
| `0x4cc798` | `0x493bc4` | (0x4e, 0x195) 0x30 x 0x2c = (78, 405, 48x44) | `0x443490` mode 2 | `0x443580` -> 226 "Aggressive Tactic" |
| `0x4cc79c` | `0x493c38` | (0x7e, 0x195) 0x26 x 0x2c = (126, 405, 38x44) | `0x4435e0` mode 1 | `0x4436d0` -> 227 "Defensive Tactic" |
| `0x4cc7a0` | `0x493cac` | (0xa4, 0x195) = (164, 405, 38x44) | `0x443730` mode 3 | `0x443820` -> 228 "Balanced Tactic" |
| `0x4cc7a4` | `0x493d20` | (0xca, 0x195) = (202, 405, 38x44) | `0x443880` mode 0 | `0x443970` -> 229 "Direct Control" |
| `0x4cc7a8` | `0x493b50` | (0x0c, 0x196) 0x3b x 0x2c = (12, 406, 59x44) | `0x442da0` | `0x442f10` -> 225 |
| `0x4cc7b0` | `0x493970` | (0x50, 0x196) 0x28 x 0x2c = (80, 406, 40x44) | `0x4439d0` 0x1e val 1 | `0x443b00` -> 230 "Manual Repair" |
| `0x4cc7b4` | `0x4939e4` | (0x78, 0x196) 0x24 x 0x2c = (120, 406, 36x44) | `0x443b60` 0x1e val 2 | `0x443c90` -> 231 "Manual Destroy" |
| `0x4cc7b8` | `0x493a58` | (0x9c, 0x196) 0x22 x 0x2c = (156, 406, 34x44) | `0x443cf0` 0x20 | `0x443e20` -> 233 "Fast Build Mode" if town `+0x158 == 2`, else 234 "Economy Build Mode"; "" (`0x4ae194`) with no selection |
| `0x4cc7ac` | `0x4938fc` | (0xc8, 0x197) 0x2a x 0x2a = (200, 407, 42x42) | `0x442ff0` (0x21 PAUSEBUILD) | `0x443340` -> 125 "Current Building Button", or 123 "No Money" / 124 "Can't build" for the current item |

The first five buttons are children of the squad panel and the last five of the town queue panel.
`+0x31 = 1` on all buttons; `+0x30 = 1` on the centre and current-item buttons (G: toggle or
sticky flag).
Unknowns: the extra list row and the 1024x768 y offsets (the builder uses fixed y values; G that
`0x462f90` moves the regions), and grey-out. No disabled state is drawn from `+0x5c`. G: per-button
state comes from the draw slot `+0x54`, which was not read.

## 2. Minimap (V `0x46a1a0` map init, `0x462f90` mode change, `0x464ba0` draw)

- Box 0xb4 x 0xb4 (180x180), 8-bit work surface (`0x47ea60(.., 0xb4, 0xb4, 8)`).
- Scale `view+0xc4` (= `0x4dd3c4`) = 0xb4 / max(map_w, map_h), in integer pixels per tile.
- Origin `view+0x48/0x4c` (= `0x4dd348/0x4dd34c`) = (W-0xfe+0x3e, 0x19) = (W-192, 25), plus the
  centring offset ((0xb4 - map_w*scale)/2, (0xb4 - map_h*scale)/2). In 800x600 the box spans
  x 608..788, y 25..205, which is above the lists at y 218.
- Layers (profiling labels `"draw minimap"`, `"collect lights"`, `"put units"`): terrain/light byte
  >> 3 per tile, then unit dots (colour index side*4 + 0x11), then the camera frame
  (`view+0x50`, scaled through `0x464a30`).
- Click handling: worldview_mouse_cursors.md 1.6 (left = move camera, right = order 0x18 with flag 0x20,
  Ctrl+left = select the nearest own group). No other minimap orders.
- Update rate: `0x464ba0` is called from the panel redraw `0x469e50` only (V for this caller).
  G: it redraws with the panel, i.e. when the panel is dirty; the exact cadence needs a live run.

## 3. Tooltips (the "hint text"), V

- `0x465a50` (view update) calls `0x464190(cursor)`. When the interface is shown, this calls the
  `+0x5c` chain of the root region (`view+0x24`), then the visible panels (`0x4cc7dc`, then
  `0x4cc7d8` if a town is listed (`0x464270(0x4cc7e8)`), else `0x4cc7e0`). It converts the anchor
  back to screen. The text pointer is stored at `view+0x38104` and the anchor at
  `+0x38108/+0x3810c`. A change of text forces a full redraw.
- Draw (end of the view draw): font slot 2. The text is split at the first '\n' into two lines.
  The box width is the longer line, the height is 1 or 2 line heights (`0x462340(2)`) plus 8. It
  is clamped to the screen and drawn above the anchor, as a filled box with a frame (vt `+0x3c`,
  `+0x34`).
- Build grid texts (`0x441ba0`, entry state at cell `+0x54`):
  - -1 (can build): 217 "%s\nPrice: %i Time: %i" (`0x45c5b0` price, `0x45c690` time);
  - -2: 218 "%s... \nAlready exists";
  - -3: 219 "%s... \nBuilding is in progress";
  - 0x5a: 122 "%s... \nUpgrade is in process";
  - otherwise 220 "%s\nBuild: " followed by the names of the missing prerequisites, separated
    by ", " (`0x4ae190`). 221 "Some error occurred :(" is shown when an id is invalid.
  - Arrows: 215/216 "Scrolls building tree (up/down)".
- There is no other status line (consistent with worldview_mouse_cursors.md sec. 3).

## 4. Resource display, messages, outcome text (V, view draw around corpus line 59290)

- Money: `wsprintf("%d", side+0x698)` (fmt `0x4a8e14`, getter `0x469850`), font 1, right-aligned
  at (W - text_w - 0x18, 5). The shown value `0x515a24` moves toward the real value by
  max(|diff|/8, 10) per frame. A coin animation (`view+0x34500`, frame `0x515a28` & 7, which steps
  +1 or -1 while the value changes) is drawn at (W-0x16, 1). Only when the interface is shown.
- Script messages (`printString` ring, 16 entries `0x515898`, expiry 4000 ms): drawn when
  `view+0x38124 == 0` and `0x51575c & 2 == 0`. Font 4, at x = 10, y = 0x14 (20) + 0x12 (18) per
  live line, oldest first. Expired entries are freed during the walk.
- Outcome banner: when `view+0x38120 != 2` (2 = playing), a box of 0xc8 x 0x1e (200x30) with
  text in font 3, centred at (view_w/2, view_h/2 - 5): string 130 "You WIN!!!" if the value is 1,
  else 131 "You LOSE!!!".
- Pause: message box with 235/236 "Pause mode on/off" (G: the "Pause" text in
  interface_ingame.md is one of these).

## 5. Selection display (partial)

- V: ViewAll (action 7) sets `view+0x380f1` through `0x46a9a0` (status bars for all objects).
- V (rendering_animation_second_pass.md): ellipse and health-arc art are chosen by display size,
  using the view clock.
- V: the rubber band rect `0x4dd374..0x4dd380` is used by `0x447c30`. Where it is drawn was not
  found in this pass.
- Not found: on-screen group numbers. G: none are drawn, because control groups are only
  recalled with the digit keys.
- Needs a live run: health bar colours and thresholds, and the band colour.

## 6. Shift box-select helpers

- `0x447800(side, list, flags)` (V): does nothing unless `flags & 0xc` and a selection
  `side+0x688` exist. For every candidate in `list` it walks the members of the current
  selection (vt `+0xb0`). A candidate is **removed** from the list unless some member matches:
  - bit 4: same type (vt `+0x6c`);
  - bit 8: same health band, where band = hp (`+0x38`) * 256 / max hp (`+0x3c`) and
    0 = up to 0x66 (40%), 1 = 0x67..0xb2, 2 = 0xb3 (70%) and above.
  So this is the FilterType / FilterCondition key filter (actions 4/5), not "remove from old
  groups". G: `0x42ea70` sets only bits 1, 2 and 0x40, so bits 4 and 8 must be added by another
  caller. That caller was not found, and no `0x42e880(4/5)` call is in the corpus.
- Group vt `0x4944f0`. Slot `+0xf4` = `0x452990(target, quiet)` (V): only for a target on the same
  side. It clears this group's order queue (`+0xdc`) and calls `+0x5c` (G: stop). Then:
  - If this group was the selection, it selects the target (`0x4498a0`).
  - It sets `+0x94 = +0x9c = 1` and `+0x9d = quiet`, `+0x98 = target`, and adds 1 to the
    target's `+0xac`.
  - If `quiet == 0`, it plays a random acknowledgement (sample group 0x69, or 0x131/0x195 by
    race, `0x4016b0`).

  So Shift+box makes a new group and **attaches it to the current group as a joined sub-team**,
  silently (quiet = 1). This is not a flat merge. These are the fields that
  worldview_mouse_cursors.md 1.2 reads as "carrier".
- Slot `+0x10c` = `0x44fa40(side)` (V): if `+0xb0` is set, it calls `+0xe4(side)` on members whose
  `+0xb8` is true, and recurses into joined sub-teams (`+0x94`, `+0x9c`, `+0x9d`). G: this
  propagates the selection or highlight to attached teams.

## 7. Front-end screens: button rects from `0x41eef0` (V, (left,top)-(right,bottom))

Each root's children are built before `DAT_root = 0x427240(bg)` (or `0x427690` overlay).
Fonts: titles and dialog text use `0x4272d0(.., font 3, align 2)`, list and text lines use font 4,
option labels use font 4. Buttons use font slot 4 (frontend_screens.md).
| State | Controls |
|---|---|
| 0 (bg 284) | Create (72,28)-(242,88), Remove (314,28)-(486,88), Credits (556,28)-(728,88), Accept (72,512)-(242,572), Quit (556,512)-(728,572), `0x427960` (496,192)-(696,232) |
| 13 Credits | scroller `0x4273b0` (140,113)-(660,476) |
| 2 MP chooser | `0x427910` Single Player (104,192)-(304,232) |
| 1 campaign | Load 138 (72,28)-(242,88), Custom 37 (556,28)-(728,88), Play 139 (72,512)-(242,572), Options 48 (314,512)-(486,572), Back 141 (556,512)-(728,572), `0x4282c0` (620,400)-(680,440), `0x428310` (620,230)-(680,270) |
| 6 custom | Accept `0x427d70` (72,512)-(242,572), Options, Back, page `0x428220` (620,400)-(680,440), `0x428270` (620,220)-(680,260) |
| 4 load | Load This `0x427fa0` (72,512)-(242,572), Options, Back, list `0x4289f0`, title ordinal 0xb4/4 = 45 (font 3); backgrounds 0x3ea (1002), 0x11b (283), 0x11c (284) appear here (G which is which) |
| 5 save list (overlay) | Save `0x427ff0` (150,328)-(222,388), Cancel (350,328)-(422,388), slots `0x428b60`, frame (100,92)-(472,396) |
| 15 save name (overlay) | Save `0x428040` (400,300)-(480,340), Cancel (500,300)-(580,340), field `0x427460` (220,260)-(580,282), title 46 "Enter Save Name" font 3, frame (200,200)-(600,350) |
| 14 pause (overlay) | buttons 72x30 at x 220..292: Continue (220,145), Save Game (220,175), Objectives (220,205), Options (220,235), End Mission (220,265); title 47 "Make Your Choice" font 3; frame (140,82)-(372,315) |
| 7 / 8 options | Back `0x427aa0` (state 8: (160,460)-(550,..)); right column x 360..500: `0x428400` y 236, Music 50 `0x4284d0` y 256, Effects 51 `0x428550` y 276, Speech 52 `0x428510` y 296, sliders `0x4285a0` y 376 and `0x428600` y 396 (16 px tall); cyclers `0x427b90` (180,314)-(345,334), `0x427be0` (180,334)-(345,354), `0x427c30` (220,354)-(345,374); state 7 bg 284, state 8 overlay |
| 17 briefing (285) | Start (643,338)-(778,514), Objectives (462,308)-(610,469), Diary (339,18)-(595,157), Order (158,197)-(437,518), Back (0,0)-(151,226) |
| 18 Order (290) | page `0x4287f0` (361,447)-(441,527); 11 text lines `0x4c1be0`.., 0x100 (256) bytes each, font 4 |
| 19 Objectives (264) | next `0x428890` (264,151)-(503,490), prev `0x428840` (259,105)-(506,143); 16 lines |
| 20 Diary (270) | right page `0x428890` (408,113)-(660,489), left page `0x428840` (139,111)-(396,491); 34 lines up to `0x4c3ce0` |
| 21 in-game Objectives (overlay) | Back `0x427af0` (160,510)-(550,530); 16 lines font 4 |
| msg boxes `0x4c4990` / `0x4c498c` | OK `0x427cd0` (400,300)-(500,340) and Cancel (520,300)-(620,340) (`0x4c498c`: OK only at (520,300)); 2 text lines font 3; frame (180,200)-(620,350) |
| text box `0x4c4984`/`0x4c4988` | same buttons, field (200,270)-(600,292) |

MP screens 3, 9..12 use the same outer rects plus list `0x427530(0x140, 0x104, 7 rows, 100, 0x14)`.
Discrepancy: frontend_screens.md lists Prev at y 230 and Next at y 400. Here `0x4282c0` is at
y 400 and `0x428310` at y 230. Their ordinals (162 Next / 163 Prev) were not read from the
constructors.
Tooltips: the front end has none (no `+0x5c` provider outside the game panel was found, G).

## 8. Pause and in-game menu actions (summary, V from interface_ingame.md plus rects above)

- 14: Continue `0x419df0` (pop), Save Game `0x419da0` (-> 5, enabled only in single player through
  enter hook `0x41ea30`), Objectives `0x41a7f0` (-> 21), Options `0x41a810` (-> 8), End Mission
  `0x419e00`.
- 5: slot list, Save `0x41a340` -> 15. 15: name field, Save -> `0x41a3d0` writes. 8: same option
  controls as 7, as an overlay. 21: objective lines plus Back.

## 9. Unknowns / need a live run

- 1024x768 panel: row count `0x5154d4`+4 lists, y offsets of buttons and grids, sprites 292/294/296.
- Button grey-out or pressed art (draw slot `+0x54`), and the meaning of `+0x30/+0x31`.
- Health bars, selection ellipses, rubber band colour, on-screen group numbers.
- Minimap refresh cadence and dot colours (remap 0x11 + side*4).
- Who sets filter bits 4/8 for `0x447800`.
- Labels of `0x427960` (state 0) and the Prev/Next ordinals on states 1 and 6.
- The tooltip delay: none is seen in code (it appears as soon as the hover text changes, G).
