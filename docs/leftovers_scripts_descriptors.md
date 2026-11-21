# Leftovers: script actions, condition timing, descriptor words, ferry rate

Evidence date: 2026-11-21. Static analysis only (corpus `all_full.c` plus objdump of
`Aztec.exe`). **V** = seen in code at the cited address, **G** = guess.
Payload dword n = byte offset 4n of the 256-dword script command payload.

## 1. Script actions

### moveCommand (code 0x12, handler `0x4022e0`)

- V `0x4022e0`: side = payload dword 0 (side struct `0x4dd2f4 + side * 0x69c`). Reads the
  side plan `side + 0x694`; if non-null and `0x457380(plan)` is true (plan done flag
  `+0x810` set and plan town `+0x80c` null or its virtual `+0xb4` false), it calls
  `0x44cc80(side, x = dword 2, y = dword 3)`. Ghidra dropped the second argument; objdump
  shows both pushes.
- V `0x44cc80`: walks the side town list (`+0x660`), takes each town position (town
  `+0xf0`, virtual `+0x88`), computes the octagonal distance `0x461fe0` to (x, y) and returns
  the nearest town (`ret 8`). It writes nothing.
- V: `0x4022e0` drops the return value. **moveCommand has no effect.** Also, the side plan
  `+0x694` is null in a fresh game (`ai_scoring_threat.md` section 5), so it normally exits
  at the first test. A reimplementation can treat it as a no-op.

### cameraActivate (code 0x13, handler `0x402330`, about 0x402330..0x402421)

- V: side = dword 0; camera id = **dword 0x0c (byte 0x30)**; duration = **dword 0x0d (byte 0x34)**.
- V `0x4470a0(side, id)`: finds the record whose dword 0 == id in the side list
  `+0x684`. The record is {id, tile x `+4`, tile y `+8`, radius `+0xc`, created object
  `+0x10`}. If none is found, nothing happens. G: these are the map's camera points.
- V: it takes an object from pool `0x4cc3d0`, zeroes it and builds it with the world-object
  constructor `0x43e890(owner side, tile x, tile y, 0x59)`, so the pixel position is
  tile * 0x30 + 0x18 and `+0x40` = owner side. The vtable is `0x48f2d8` (methods
  `0x402430..0x402af0`). It then sets:
  - `+0x60` = clamp(radius + 0x3b, 0x40, 0x47), that is light/sight mask ids 64..71. Read
    back by virtual `+0x74` (`0x402a70`). For Building and Defence objects the same slot is
    the sight (light mask) id. So a radius of 5..12 maps to masks 0x40..0x47.
  - `+0x64` = duration (dword 0x0d).
  - record `+0x10` = the new object (0 if the allocation failed).
- V Think, slot `+0x60` = `0x402a50`: if `+0x64` > 0, decrement it; at 0, delete the object
  (virtual `+4(1)`). A duration <= 0 (for example -1) means it **never expires**. Unit: simulation
  ticks.
- V: virtual `+0x08` returns 9 (`0x402a40`, object kind 9). Save `0x402aa0` writes `+0x60`
  and `+0x64`.
- Effect (G from the parts above): an invisible revealer owned by the side, placed at the
  camera point. It lights a radius-dependent sight mask for `duration` ticks. No camera
  scroll happens here (camera jump is code 0x14, `0x402b00`).

## 2. Conditions and submarine

### Tick units of globalTime (0x08) and localTime (0x07)

- V: the session frame loop is `0x4577b0`, from 0x457915 to the back-edge 0x45882f. Each
  iteration is one simulation tick. In it:
  - `0x4184a0` is called once at 0x4579bf (localTime clock `0x4c1968` +1, wraps above 0x1c1f).
  - every tactic in pool `0x4d9b50` gets virtual `+0x138` (0x4580ad..0x4580c7).
  - every side gets `0x449980`.
- V globalTime `0x4c5730`:
  - single player (`0x43d210` false): `0x43d1c0` adds 1 at 0x45864c, once per iteration.
  - multiplayer: `0x43a850` (lockstep frame receive) adds 1 at 0x43ac72 when all players'
    frame packets have arrived, then signals `0x4cc21c`. The loop waits on that signal with
    `0x43cec0(0x320 ms)` and on a timeout skips the tick (0x457954 jumps to the loop tail).
  - so in both modes globalTime = completed simulation ticks since the session started
    (reset to 0 at 0x43c14e and 0x43c9d2).
- V: localTime and globalTime **advance at the same rate**, 1 per tick. localTime starts at
  ini StartTime and wraps at 7200 (0x1c20).
- V tick length: at the loop tail (0x4587e5..0x458809) the loop runs
  `Sleep(period - elapsed)` with a minimum of 1 ms, where period = `DAT_005154dc` ms. The
  period is set at 0x41a9ee as `0x104 - slider` (260 - setting) and loaded at 0x4310e0.
  G: the slider is the game speed option; its value range was not read.

### Rectangle edges for unitsInRect (0x13, `0x418030`) and unitsOfUGIInRect (0x14, `0x4181d0`)

- V `0x418030`: the corners are tile * 0x30 (no +0x30 on the max corner) and are sorted to
  min/max. Query `0x46b460`.
- V `0x46b460`: for each object in the covered grid cells it takes the box from virtual
  `+0x1c` = {left, top, right, bottom} (min x, min y, max x, max y, as built by
  `0x411330`). Both top and bottom are shifted by virtual `+0x18`. The object counts when
  `top < R.y2 && R.y1 < bottom && left < R.x2 && R.x1 < right`.
- So the test is **strict overlap of the object box with the open pixel rectangle**
  [x1*48, x2*48) x [y1*48, y2*48). A box that only touches an edge is not counted. Tile
  x2/y2 itself is outside unless a box reaches into the rectangle. Equal corners
  (x1 == x2) count nothing. Membership is by box overlap, not by tile position.
- G: what virtual `+0x18` returns per unit (altitude offset).

### submarine <id>.go (code 0x0e, `0x402060` -> virtual `+0x14c` = `0x42fbb0`)

- V class: the submarine is **not a unit**. It is a tactic (group) subclass, vtable
  `0x492fdc`, built on the base tactic constructor `0x44f5c0`. It is created by the mission
  setup loop at 0x42f9df (ini keys "Submarines" / "Submarine%i", G for the key use) and by
  map load case 3 (`0x445a27`). Tactic-kind virtual `+0x80` = 3. Lookup is in the tactic
  pool `0x4d9b50` by id = payload dword 7.
- V `+0xf0`: an animated effect object (pool `0x4c1978`, constructor `0x4189d0`, vtable
  `0x4904ac`), placed at the tactic tile * 0x30 + 0x18. Placement slot `+0x90` =
  `0x42fad0` creates it with animation set 0x1b (27), sequence 0, loop flag 1: the idle,
  visible submarine. Save `0x42fa80` / load `0x42fab0` store it as an object reference.
- V go `0x42fbb0`: delete `+0xf0` (virtual `+4(1)`); create a new effect: animation set 27,
  sequence 1, loop flag 0 (one-shot, G for the flag meaning); on failure log via `0x475c20`
  (Submarine.hpp line 0x3d, G); remove the tactic from its side (`0x447d70`). There are no
  hit points, members or movement.

## 3. Descriptor words

- **Marine word 15 (`+0x3c`) = spawn sound list id.** V: Marine vtable `0x4932cc` (written by
  the constructors at 0x432df0 and 0x433228), slot `+0xc4` = `0x4334d0` (place on map:
  `+0x5c` = 1, state `+0x64` = 1). At 0x4335ed it reads `desc + 0x3c`; if non-zero, it loads
  the list (`0x401250`), picks a random entry (`0x481070` % count) and plays it at the
  object position (`0x401480`). 0 = silent. Same pattern as the spawn sound of Cannon
  (word 14) and Zeppelin (word 15). The values 0 and 2 to 7 are sound list ids. This
  supersedes the "likely unused" guess.
- **Defence words 60 to 62 (`+0xf0`, `+0xf4`, `+0xf8`)**: V no reader. A scan of all
  `mov r,[obj+0x6c]` followed by `[r+0xf0|0xf4|0xf8]` found none, and the corpus has no
  such access. They stay unused (always -1). Meaning G.
- **Defence word 63 (`+0xfc`)**: read in `0x41d790` (already V). No new reader found;
  "launch height" stays G.
- **Third armor word (virtual `+0x110`)**: V the whole executable has only two indirect
  `call [reg+0x110]` sites, at 0x401f30 (inside handler 0x0a, `0x401e80`) and 0x401ff0
  (inside handler 0x0c, `0x401f40`). Both call script tactic objects (boat byte `+0xe8`
  setter `0x408c80`, `ferry_control_flow.md`), not units. So **no consumer of the unit
  third armor value exists**: it is dead data in every kind. G caveat: a call through a
  register loaded from the vtable (`mov r,[vt+0x110]; call r`) would not be found by this
  search.
- **Building word 3 (`+0x0c`) = hit rectangle y1, now fully V.** `0x411330`: loaded at
  0x411364, spilled to the stack at 0x411369, reloaded at 0x411375, added to pos.y at
  0x41137b. The result is min/max-paired with pos.y + word 5, giving box top/bottom.
- **Defence word 6 (`+0x18`) = hit rectangle y1, now fully V.** `0x41e220`: the same
  sequence at 0x41e254 / 0x41e259 / 0x41e265 / 0x41e26b, paired with word 8.

## 4. Ferry think rate

- V: the boat tactic vtable is `0x48fbd4`. The ferry think `0x408eb0` is at slot `+0x138`
  (pointer at `0x48fd0c`).
- V: `0x4577b0` calls `+0x138` on every object of tactic pool `0x4d9b50` once per
  frame-loop iteration (0x4580ad..0x4580c7), with no gating. (The other `+0x138` site,
  0x45802f, is the same loop over pool `0x5156f8`.) The only skips are the loop paths that
  drop the whole tick (multiplayer not ready or timeout, 0x457924 / 0x457954).
- V (partial read): the head of `0x408eb0` has no frame-counter modulo and no period test.
  So the ferry think **runs every simulation tick**.

## Remaining unknowns

- Who fills the side camera-point list `+0x684` (map loader not traced), and the exact
  pixel radius of light masks 0x40..0x47.
- The game-speed slider range, which gives the tick period `0x104 - slider` ms.
- What virtual `+0x18` returns, used to shift boxes in the rectangle test.
- The meaning of the `0x4189d0` last argument (loop or once), and whether the
  0x475c20 call in `0x42fbb0` is only an error path.
- Defence word 63 meaning, and Defence words 60 to 62 (unread).
- Whether any pause path inside the frame loop skips the tactic pass; only the network
  skips were checked.
