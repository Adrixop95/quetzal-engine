# Interface: menu states, briefing flow, in-game controls, screen layout (original Aztec.exe)

Read-only pass, 2026-11-21, bounded at about 50 tool calls. Sources: `work/decomp/all_full.c`,
`work/gaps/dec_41eef0.c`, objdump of `game/Aztec.exe`, installed `Keys.ini`.
V = read in code or data. G = inference. Numbers hex (decimal).
Builds on `campaign_menus_saves.md` (sec. 4) and `docs/121-input-hotkeys-commands-audit.md`.

## 1. Menu states: complete table

Widget roots `0x4c4994[s]` are assigned in `0x41eef0` (V, assignment lines in `dec_41eef0.c`).
Panel constructor `0x427240(bg, ...)` takes a full-screen `common.xbe` sprite; `0x427690` is a
root with no background, i.e. an overlay drawn over the running game (V for the call, G for "overlay").

| State | Root | Background | Buttons (constructor -> handler) | Meaning |
|---|---|---|---|---|
| 0 | `0x4c4994` | (see csaves.md) | Create/Remove/Credits/Accept/Quit | Choose Player, SP (known) |
| 1 | `0x4c4998` | - | Back 141 `0x427af0`->`0x418c00`, Options 48 `0x427870`, Play Mission 139 `0x427a00`->`0x419290`, Custom 37 `0x427a50`, Load Game 138 `0x4279b0`, Prev/Next `0x4282c0`/`0x428310` | campaign list (V) |
| 2 | `0x4c499c` | - | Single Player 31 `0x427910` ... | Choose Player, MP (known) |
| 4 | `0x4c49a4` | 0x11c (284) | Back, Options, Load This 156 `0x427fa0` | Load Game list (V) |
| 5 | `0x4c49a8` | none (overlay) | Cancel 148 `0x427d20`, Save 157 `0x427ff0`->`0x41a340` | in-game save slot list (V) |
| 7 | `0x4c49b0` | 0x11c (284) | Back+save profile 141 `0x427aa0`->`0x418be0`, option cyclers 142..146, sliders `0x428400`..`0x428600` | front-end Options (V) |
| 8 | `0x4c49b4` | none (overlay) | same controls as 7 | **in-game Options** (V: shown by mission loop when `0x51555c`, set by in-game Options button `0x41a810`) |
| 13 | `0x4c49c8` | - | - | Credits (known) |
| 14 | `0x4c49cc` | none (overlay) | Continue 159 `0x428090`->`0x419df0`, Save Game 160 `0x4280e0`->`0x419da0`, End Mission 161 `0x428130`->`0x419e00`, Objectives 164 `0x428360`->`0x41a7f0`, Options 48 `0x4283b0`->`0x41a810` | **in-game pause menu** (V). Enter hook `0x41ea30` calls vtable `+0x14(!MP)` on widget `0x4c4c6c` (G: enables Save Game only in single player) |
| 15 | `0x4c49d0` | none (overlay) | Cancel `0x427d20`, Save 157 `0x428040`->`0x419df0`, text field `0x427460`, label ordinal 0xb8/4 = 46 | **Enter save name** dialog (V: shown when `0x51555b`, set by Save `0x41a340`; on OK the loop calls `0x41a3d0`) |
| 16 | `0x4c49d4` | - | - | **unused**: root never assigned in `0x41eef0`, no push (V) |
| 17 | `0x4c49d8` | 0x11d (285) desktop | Start Game `0x4287a0`->`0x41ad40`, Objectives `0x428750`->`0x41acc0`, Diary `0x428700`->`0x41ab30`, Order `0x4286b0`->`0x41aa10`, Back `0x428640`->`0x419df0` | **briefing desktop** (V). Hit rects 0x424a9f..0x424bb7 per frontend_screens.md lie inside `0x41eef0` |
| 18 | `0x4c49dc` | 0x122 (290) framed order | page `0x4287f0`->`0x41aa90` + text lines `0x4272d0` | **Order** reader (V) |
| 19 | `0x4c49e0` | 0x108 (264) scroll | page buttons (`0x41abb0` prev / `0x41ac30` next, G for assignment to 19) | **Objectives** reader (V push from `0x41acc0`) |
| 20 | `0x4c49e4` | 0x10e (270) book | page buttons | **Diary** reader (V push from `0x41ab30`) |
| 21 | `0x4c49e8` | none (overlay) | - | **in-game Objectives** (V: shown when `0x51555d`, set by Objectives `0x41a7f0`) |
| -1 | `0x4c4990` | none | - | message box / confirmation used by `0x429200` (`0x4c4cc1`, V) |

Other roots: `0x4c498c` (alternate message box, `0x4c4cc2` selects it), `0x4c4988` text-entry box
(`0x4293b0`, `0x4c4cc3`) (V `0x428e10`, `0x428ec0`).

Display modes for a state (V):
- stack push `0x428cd0` / replace `0x428d00` (front-end thread);
- in-mission: the mission loop `0x4577b0` shows states 14, 21, 5, 15, 8 through `0x429570(s)`
  (single player: enter hook, then `0x4295a0` blocks on event `0x4c4d6c`) or `0x429600(s)`
  (multiplayer, non-blocking overlay, `0x4c4cd4 = 1`). The choice is `0x43d210()` (MP test).
  Triggers: `0x51555a` -> 14, `0x51555d` -> 21, `0x515559` -> 5, `0x51555b` -> 15, `0x51555c` -> 8.

### Begin Campaign (ordinal 140)
V: none of the 41 constructors `0x427730`..`0x4283b0` loads offset 0x230 (ordinal 140) and the
state-1 root holds only the seven buttons above. So **Begin Campaign does not exist** in this build;
"Play Mission" on the selected list row is the only campaign start. frontend_screens.md's
"Begin Campaign at the bottom middle" is wrong (G that the string is a leftover).

## 2. Path main menu -> campaign -> briefing -> mission -> back

1. State 0 Accept Player `0x418c40` -> push 1 (V).
2. State 1, row select, Play Mission `0x419290` (V, 0x419adc..0x419d4b):
   disc/file checks; mission AVI `0x419110`; stores `0x4dd164` (0x419b03); copies mission path
   to `0x4c4b58` (0x419afe); opens `<mission>.mit` (ext string `0x4aa234` "mit"); `0x4191e0`
   finds `~`-prefixed (0x7e) sections: "Rozkaz" `0x4aa22c` -> `0x4c1bc0` (Order),
   "Cele" `0x4aa224` -> `0x4c1bc4` (Objectives), "Pamietnik" `0x4aa218` -> `0x4c1bc8` (Diary);
   then **push 0x11 (17), the briefing**. The mission itself is not started here.
3. Briefing (state 17, sprite 285) is a normal stack state, drawn by the generic widget root. Its
   Order/Objectives/Diary buttons wrap text with `0x429a50(text, width, lines, prefix, out, 10)`
   and push the reader: Order width 0x140 (320) x 0xb (11) lines -> 18; Objectives 0xe1 (225) x
   0x10 (16) lines, prefix `0x4aa3a0` (4 spaces) -> 19; Diary 0xf0 (240) x 0x22 (34) lines -> 20
   (V). Page index `0x4c1b60`, page count `0x4c1b64`; turning plays sound 0x1f5 (501) and redraws
   the full screen; Next on the last Order page pops back (`0x41aa90`, V). `0x4c1b90 = 1` while a
   reader is open; a right click (mouse `0x4355d0`) or any key (`0x434af0`) closes it (V).
4. Start Game `0x41ad40` -> `0x428f30(0x4c4b58)` (V): copies the path, increments stack top
   `0x4c4a14` unless the top is state 1 (`0x429040`), sets `0x4c4cc0 = 1` (game running),
   signals `0x4c4d6c` and suspends the front-end thread `0x4dd154`. Custom (`0x41a2c0`) and
   Load This (`0x41a2e0`) use the same entry, so they skip the briefing (V).
5. Mission loop `0x4577b0`: resumes the front-end thread (ResumeThread `0x4dd154`, V line in
   `0x4577b0`) so overlay menus can run; Escape (action 0x12) sets `0x51555a` -> state 14.
6. End Mission `0x419e00`: pop (closes overlay), `0x428fa0` (clears `0x4c4cc0`, SetEvent
   `0x4c4d6c`, or `0x43cde0` in MP), pop again (V). Victory/defeat end goes through the tail of
   `0x4577b0` (movie, progress, see csaves.md) and the WM case 0x10 path (`0x428fa0` +
   events, V at corpus line ~200306).
7. Return state (G): the second pop undoes the increment of step 4, so the player lands on the
   state that was on top at launch: briefing 17 for a campaign start, 6 for Custom, 4 for Load.
   Whether the briefing then re-enters or is popped to state 1 is not traced.

## 3. In-game input

### Action ids (V, name/id table at `0x4ab7a0` region, read by `0x42e210`)
add 2, inv 3, filtertype 4, filtercondition 5, fastscroll 6, viewall 7, temporary 8, center 9,
putontop 0xc, garrison 0xd, send 0xe, friend 0xf, autorepair 0x14, repair 0x15, destroy 0x16,
aggressive 0x17, defencive 0x18, normal 0x19, directcontrol 0x1a, lastcaller 0x1b,
toggleinterface 0x1c, scrollleft..down 0x26..0x29, printscreen 0x2d, cheatwin 0x34.
Digits are ids 0x36..0x3f (V `0x434af0`: `0x35 < id < 0x40`). Followed by the key name table
(DIK scan codes, e.g. q 0x10, lshift 0x2a). Held state test `0x42e880(id)`, raw key `0x42e8c0`.

### Key dispatch `0x434af0` (V unless noted)
| Id | Effect |
|---|---|
| 7 ViewAll | toggles `0x4c547c` via `0x46a9a0` (status bars) |
| 9 Center | centres camera on own tactic `+0x688` of the local side, clamped (see sec. 5); with Inv held toggles `0x4dd3c8` (monitor/follow) |
| 0xc PutOnTop | `0x440910` |
| 0xd Garrison | queues packet 0x1d (unless `0x4dd3c9`) |
| digit n (0x36+n) | Inv held: `0x440840(n)` assign; else `0x4408b0(n)` recall. If raw lshift (0x2a) held: `0x45c930` on the tactic with the camera position (G: route/rally to group) |
| 0x12 (Escape, G name) | single player sets `0x51555a` (menu 14); otherwise closes overlay `0x428d20` |
| 0x1b LastCaller | `0x462540` |
| 0x1c ToggleInterface | `0x462f10(!0x5154e9)` |
| 0x1d (no Keys.ini name) | toggles `0x5154d4` 800x600 <-> 1024x768 and signals video thread |
| 0x1e (unnamed) | Pause: message "Pause" `0x4acb44` via `0x429200`; SP `0x463480`, MP `0x43d300` |
| 0x1f (unnamed) | toggles `0x4c5479` -> `0x46ab50` (G: grid/overlay) |
| 0x23 (unnamed) | MP: packet 0x12 FRAME_SAVE; SP: `0x515559` (save list) |
| 0x2b, 0x2c | toggle `0x4c4d08` (closes menus), toggle `0x515414` |
| 0x2d PrintScreen | `0x515415 = 1` |
| 0x33 | `0x515558 = 1` |
| 0x34 CheatWin | local side `+0x140 = 1` (won) |
| 0x14..0x1a | no case seen in the filtered listing; G: routed to the panel buttons below |

### Command panel buttons -> packets (V: functions that write the local ring `0x4dd150`)
| Handler | Packet | G meaning |
|---|---|---|
| `0x443490`, `0x4435e0`, `0x443730`, `0x443880` | 0x1f ATTACKMODE (2,1,3,0) | Aggressive/Defensive/Normal/Direct |
| `0x4439d0`, `0x443b60` | 0x1e REPEARMODE | AutoRepair / Repair |
| `0x443cf0` | 0x20 BUILDMODE | fast/economy production |
| `0x443068` | 0x21 PAUSEBUILD | pause/resume current item |
| `0x442466` | 0x22 QUEUEKILL | remove queued item |
| `0x441afb` | 0x1c BUILD | production entry |
| `0x4416e0` | 0x23 SELECTBYNO | squad/town list icon |
| `0x441630` | 0x24 FOLLOWTEAM | join/route to list entry |
| `0x434af0` | 0x1d GARRISON, 0x12 SAVE | keys |

Not found as literal ring writers in this pass: 0x15 CREATEGROUP, 0x16 SELECTGROUP,
0x18 ATTACK, 0x1a DESTROYHERE (G: built in the mouse handler `0x4353a0` or group functions
`0x440840`/`0x4408b0` with a computed type). Which buttons appear for which unit/building
class (the visibility predicate) was **not** traced; the panel chain is `0x443f00` (unread).

### Mouse `0x4355d0` (DirectInput, V)
- Cursor accumulates relative deltas onto `0x4c5468/0x4c546c`, clamped to 0..`0x4dd368`-1 /
  0..`0x4dd36c`-1 (screen size).
- Double click: left press within 500 ms (0x1f4) of the previous one and less than 5 px away on
  both axes (stored in this+0x18..0x20); passed as a flag to `0x4353a0`.
- FastScroll (action 6, Caps held): camera `0x4dd330/0x4dd334` += mouse delta, clamped to
  0..map_w*0x30 - view_w - 4 and 0..map_h*0x30 - view_h - 1 (tile 0x30 = 48 px); clears
  monitoring `0x4dd3c8`.
- Edge flags `0x5153d8`: x < 4 -> 4, x > w-4 -> 5, y < 4 -> 8, y > h-4 -> 0xa (margin 4 px).
  Consumer and pixel speed not found (G: view update scaled by `0x5153dc`).
- Clicks go to `0x4353a0(x, y, buttons, ..., dbl)` only when no modal is up; right click closes
  an open reader (`0x4c1b90`).

## 4. Screen layout

- V `0x41ec80`: resolutions offered are 800x600 and 1024x768 at 16 bpp (mode test 0x400 x 0x300 x
  0x10 via `0x479a70`); flag `0x5154d4` selects 1024x768, also toggled in game by action 0x1d.
  There is no 640x480 mode.
- V: viewport size `0x4dd340/0x4dd344`, camera origin `0x4dd330/0x4dd334` in pixels, camera
  clamp as above (`0x434af0` case 9, `0x4355d0`).
- V: scroll speed option `0x5154d8` (options block +0xa8) is copied to `0x5153dc` by
  `0x431000`/`0x43f070`; game speed `0x5154dc` (slider shows `0x104 - value`, 260 - value).
- V: front-end fixed rectangles (e.g. full redraw 0x8c,0x71,0x294,0x1dc in the front-end loop)
  imply an 800x600 design space for menus (G: centred at 1024x768).

## 5. Unknowns
- Button visibility per selected class, cursor choice per hover target, status/hint line text
  source, build placement preview colours: not reached (panel `0x443f00`, mouse `0x4353a0`).
- Box-select threshold and Shift-add implementation (`0x4353a0`), control group storage
  (`0x440840`/`0x4408b0`).
- Edge-scroll consumer of `0x5153d8` and pixels per tick vs `0x5153dc`.
- Exact state after End Mission / victory (sec. 2 step 7).
- Names of actions 0x12, 0x1d, 0x1e, 0x1f, 0x23, 0x2b, 0x2c, 0x33 (installed by the default
  table, not Keys.ini).
- After sec. 6: panel visibility depends on selection kind (squad or town), not on unit class
  (V). Still open: cursor per hover target, status/hint line, placement preview colours, box
  select and Shift-add (world-view widget handler not found), and whether some buttons are
  greyed per class (update hooks at vtable `+0x5c`, not read).

## 6. Second pass: command panel, mouse routing, scrolling

### Panel layout (V, built in `0x43faf0`; vtable slot 4 at vt+0x10 = click handler)
- `0x443f00` (UI refresh) calls slot `+0x68` on `0x4cc7ec`, `0x4cc7e0`, `0x4cc7dc`, then
  `0x441320(local side +0x688)`, which switches the panel by **selection kind**, not unit class:
  - selection found in list `0x4cc7e8` (vt `0x493e08`, at 0x48,0xda) -> hide squad panel
    `0x4cc7e0`, show `0x4cc7dc` and `0x4cc7d8` and give them the selected object (`+0x4c`),
    reset their page/scroll (`+0x24/+0x40/+0x44`);
  - selection found in list `0x4cc7e4` (vt `0x493e80`, at 0xae,0xda) -> hide `0x4cc7dc`,
    show `0x4cc7e0`.
  G: `0x4cc7e8` is the town list and `0x4cc7e4` the squad list. Reasons: town-only packets
  sit in the `0x4cc7dc/0x4cc7d8` panels, and `0x42ea70` reads `0x4cc7e8+0x1d`.
  Panels for the other side's objects are not refreshed (V: owner test `+0xa0` against `0x43d200`).
- Squad panel `0x4cc7e0` (vt `0x493acc`, grid `0x4149a0` at (0x1a,0x1cf), 4x3 cells of
  0x2f x 0x2a (47x42), sprite group 0x195/0x196 (405/406)):
  | Child | vtable | Click | Effect |
  |---|---|---|---|
  | `0x4cc7a4` | `0x493d20` | `0x443880` | 0x1f mode 0 |
  | `0x4cc7a0` | `0x493cac` | `0x443730` | 0x1f mode 3 |
  | `0x4cc79c` | `0x493c38` | `0x4435e0` | 0x1f mode 1 |
  | `0x4cc798` | `0x493bc4` | `0x443490` | 0x1f mode 2 |
  | `0x4cc794` | `0x493b50` | `0x442da0` | centre camera on the selection (no packet) |
- Town panel `0x4cc7dc` (vt `0x4937f4`, 0x11dc (4572) bytes, grid at (0x18,0x1cf), 3x3 cells of
  0x2a x 0x2a (42x42)): click `0x441a50`, which contains the 0x1c BUILD writer `0x441afb`
  (production cell).
- Town sub-panel `0x4cc7d8` (vt `0x493878`, grid at (0xc0,0x1cf), 1x3 cells): click `0x442420`,
  which contains the 0x22 QUEUEKILL writer (queue). Its buttons:
  | Child | vtable | Click | Effect |
  |---|---|---|---|
  | `0x4cc7b8` | `0x493a58` | `0x443cf0` | 0x20 BUILDMODE |
  | `0x4cc7b4` | `0x4939e4` | `0x443b60` | 0x1e, value 2 when pressed, 0 when released |
  | `0x4cc7b0` | `0x493970` | `0x4439d0` | 0x1e, value 1 when pressed, 0 when released |
  | `0x4cc7ac` | `0x4938fc` | `0x442ff0` | contains the 0x21 PAUSEBUILD writer `0x443068` |
  | `0x4cc7a8` | `0x493b50` | `0x442da0` | centre camera |
  Both 0x1e buttons send only when the selected object's virtual `+0x80` returns 1 (V).
  G: 1 = town, and the buttons are AutoRepair (value 1) and Repair (value 2).
  Inference from the ordinals: the 0x1f modes match keys A/S/D/F as 2 = Aggressive,
  1 = Defensive, 0 = Normal, 3 = Direct (G).
- Slot `+0x5c` of each button (`0x443580`, `0x4436d0`, `0x443820`, `0x443970`, `0x443b00`,
  `0x443c90`, `0x443e20`, `0x442f10`) is a per-button update hook (G: highlight or enable state).
  Not read.
- Packet modifier flags `0x42ea70` (V): bit 1 = Add (Shift held), bit 2 = Inv (Ctrl held),
  bit 0x40 = town list `+0x1d`. These are the flags that `0x447e00` tests (csaves/mage doc).

### Mouse routing `0x4353a0` (V)
- When the interface is shown (`0x5154e9 == 0`, no modal), x is made relative to
  `screen_w - 0xfe`, i.e. **the side panel is 0xfe (254) px wide on the right edge**.
- The event goes to every visible region in list `0x515530` (`+0x25` visible) whose rect
  (`+0x04..+0x10`, or the hover rect `+0x14..+0x20`) contains the point. Vtable slots used:
  `+0x24` enter / `+0x28` leave (hover flag `+0x24`), `+0x30` left down, `+0x2c` left up,
  `+0x38` right down, `+0x34` right up, `+0x44` double click, `+0x48` wheel. After a left
  press, `+0x4c` is sent to every region (G: drops the other regions' pressed state).
- The world-view region's handlers (box select, Shift add, hover cursor) were not identified in
  this pass.

### Scrolling `0x465a50` (view update of `0x4dd300`, V)
- Keys ScrollLeft..Down (0x26..0x29) move at full speed at once: speed `+0x380e0` is set to
  max `+0x380dc`.
- Edge scrolling uses direction flags `+0x380d8` (same bit code as `0x5153d8`: 4 = horizontal
  with 1 = right, 8 = vertical with 2 = down). Speed ramps up by max/4 per frame to max and
  back down by max/4 when the cursor leaves the edge. So it accelerates over 4 frames (V).
- Max = scroll-speed option `0x5154d8`, in pixels per view update. It is set in the
  constructor `0x462860` (`this[0xe037]`). Steps `0x469980` / `0x4699a0` / `0x4698f0` / `0x469910`
  move camera `+0x30/+0x34` (= `0x4dd330/0x4dd334`) and clamp at 0 (V for left).
- G: frame rate and wall-clock speed not measured.

## Corrections (2026-11-21, from docs/worldview_mouse_cursors.md)

- V: `0x4353a0` receives (left previous, right previous, left now, right now), so slot `+0x30`
  is the left release, not the left press.
- G (strong): `0x5154e9 != 0` most likely means the interface is shown, the opposite of
  what the panel notes above say.
- V: the world view is not a region of list `0x515530`; its input runs in the game thread
  mouse pump `0x435bc0`, which writes frame packets into the local ring.

## Corrections (2026-11-21, from docs/interface_geometry.md)

V: attack mode 0 is Direct Control and mode 3 is Balanced; the stance buttons are not
Aggressive or Defensive guesses. The two `0x1e` buttons are Manual Repair and Manual Destroy.
Vtable slot `+0x5c` of a button returns its tooltip text and anchor; it is not a grey-out or
progress hook. The values `0x195` to `0x197` are y coordinates, not sprite groups.
