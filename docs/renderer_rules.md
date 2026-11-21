# World renderer rules (0x465a50 and callees)

Read-only analysis of `FUN_00465a50` (view object `0x4dd300`, 2075 decompiled lines) and the
helpers it calls. V = read in code (decompile or objdump), G = inference. Numbers are hex (decimal).
Builds on `fog_sight_minimap.md`, `sprites_mapping.md`, `rendering_animation_second_pass.md`,
`ai_tables_and_light.md`, `tick_order_and_rand.md`; this file only adds or corrects.

## 0. Vector helpers (V)
`0x402850` make point, `0x412f70` add, `0x4041c0` subtract (this - arg), `0x464a30` scale by int,
`0x412a40`/`0x404dc0` make rect, `0x469750` rect intersect (returns bool), `0x463980(sprite,out,col,row)`
cell rect of a sprite sheet (cell w/h = header +0x10/+0x12), `0x4639f0` hotspot = header +0x14/+0x16
(signed), `0x464a70` cell count = sheet size (+4/+6) / cell size, `0x47a8e0` palette id = header +0xc.

## 1. Camera, grid and frame structure

| Item | Rule | V/G |
|---|---|---|
| Camera | view +0x30/+0x34 = world pixel of the viewport's top left; +0x38/+0x3c = viewport origin on screen; +0x60/+0x64 = viewport size in tiles | V (use), G (names) |
| Tile origin | +0x50/+0x54 = camera / 0x30 (48) per axis; +0x58/+0x5c = camera - tile*48 (sub-tile scroll) | V |
| Tile to screen | screen = 48*i - sub_x + vp_x, 48*j - sub_y + vp_y, where i, j are tile offsets from the tile origin | V |
| Terrain range | tiles tx..tx+w+2 by ty..ty+h+4 (+2 columns, +4 rows), clipped to the map rect | V |
| Overlay range | columns tx-1..tx+w+3, rows ty..ty+h+4; screen centre 48*(i,j) + (0x18,0x18) - sub + vp, with an extra (2,2) term whose sign the decompile does not settle | V range, G (2,2) |
| Object query | world rect of the viewport grown by 0x30 (48) on every side (`0x46b360`) | V |
| Object cull | per object: rect = ground pos - max(body size, underlay size) - (0,height), must intersect the view rect (`0x469750`); `0x46ba50` (x +/- 0x1c, y-0x30-h .. y+0x10-h) is the separate pick or bounds rect from `fog_sight_minimap.md` | V |
| Display list | entries of 0x4c bytes in a growable array (`0x46abd0` capacity 0x800 (2048), push `0x46ac80`) | V |

Entry layout (`0x469a30` for objects, `0x469ae0` for overlays, V): +0/+4 screen ground position,
+8/+0xc height vector (0,h) (h = -1 marks an underlay for the comparator), +0x10 body sprite id
(object virtual +0x28), +0x14 second layer id (virtual +0x24, G order), +0x18 underlay id (virtual
+0x2c), +0x1c HP ratio (hp << 8)/maxhp (virtuals +0x38/+0x3c), +0x20..+0x2c bar rect, +0x30 side
colour index (`0x473750(side)`), +0x34 selected by local side, +0x38 display size (virtual +0x20),
+0x3c flash flag (virtual +0x48), +0x40 0 object / 1 overlay, +0x44 marker kind, +0x48 unknown.

## 2. Draw order inside one world draw (V)

1. Lock view mutex `+0x380ec`; scroll (keys 0x26..0x29, velocity `+0x380dc>>2` steps), follow mode `+0xc8`.
2. Build the per pixel light surface `+0x380f4` from the local side fog map (`0x47d5e0`, see 4).
3. Terrain overlays (upper 16 bits of map cell) are pushed into the display list (not drawn yet).
4. Terrain tiles, row by row (y outer, x inner): tile id = low half of the cell (`0x463e70`), sheet
   slot `view+0xd4+id*4`. Corners equal -> flat shade (`0x47a990`/`0x47b0b0`); else per pixel shade
   from the light surface (`0x47b8c0`/`0x47c070`). Animated tiles: cell row = counter `0x515a04` (0..7).
5. Objects from the query: skip if virtual +0xc false or kind (+8) == 1 (buildings are drawn through
   kind 8 proxies). Fog test virtual +0x10 against the local side map unless kind 8 and `0x465980` true.
   **Underlay** (virtual +0x2c, archive `+0x100d4`) is drawn immediately here, with team palette and
   full 2D light sampling, so it is below every sorted item. Then the entry is pushed.
6. Sort with `0x472fa0` and comparator `0x464b10`: underlays (h = -1) first, then ground y ascending,
   then x descending (helper inverts the sign; see sprites_mapping.md). No separate air pass.
7. For each entry in order:
   a. selection ellipse if object and +0x34 (before the layers);
   b. object: for layer +0x10 then +0x14: shadow (archive `+0x2a0d4`, same id) then body (archive
      `+0x100d4`); overlay: shadow (archive `+0x1a0d4`) then body (tile archive `+0xd4`, wind frame);
   c. health bar if +0x1c != 0 and object;
   d. marker if +0x44 != 0: common sprite 5 (kind 1) or 2 (other), frame = `0x43d1b0()` % frame count,
      at ground position minus hotspot.
8. Drag selection box (`view+0x74..+0x80`): rect `0x47ebe0` plus 4 surface line calls (vt +0x30).
9. Building placement preview if `+0xc9` (section 3).
10. Path display if option `0x42e880(2)`: dots interpolated between waypoints with phase
    `0x515a14` (0..0xb, +1 per draw), sprite from table `0x4bc968` by direction (`0x47b020`).
11. Menu dim box (0x16,0x50)-(0x244,0x208) = (22,80)-(580,520) via `0x47eb70` when `+0x86`.
12. Panel and minimap `0x469e50`, tooltip text, flip.

Positions (V): body at entry - (0,h) - hotspot; shadow at entry - shadow hotspot (ground, no height);
ellipse centred on x = (rect.left + rect.right - w)/2 at y = entry.y - h - ellipse_h/2 (it follows
the airborne position); health bar at the same x centring, y = rect.top (+0x24).

## 3. Shading, palettes and remap modes

Draw primitive signature (V): `blit(sprite, [cell], surface, x, y, shade, palmgr=view+0xcc, palette_id, clip, ...)`.
The palette id selects a shade table; the shade index selects one row of it.

| Item | Rule | V/G |
|---|---|---|
| Team colour | if sprite palette id (+0xc) < 4: id += 0x10 + side_colour*4; bodies, underlays and placement preview | V |
| Fixed palettes | ellipse, health bar: palette 3, shade 0; shadows: palette `view+0x380fc`, shade 0xc (12) | V |
| Lit bodies | per pixel light, sampling one row of the light surface at the entry's ground y (flag 1): light varies across the width only | V |
| Underlay and terrain | full 2D sampling of the light surface (flag 0) | V |
| Flash | if +0x3c and counter `0x515a04` odd: flat shade 0x13 (19, brightest) instead of the light surface: blinks every other draw | V (meaning of virtual +0x48 G) |
| Placement modes | grid of 7 x 7 cells of 0x30 (48) from `0x5158d8` (49 dwords, ends 0x51599c); cell status 0 -> shade row 0x14 (20), 1 -> 0x15 (21), other -> 0x16 (22); the building sprite (archive `+0x100d4`, id = virtual +0x80 of the placement object `0x4c5470`) is drawn cell by cell with a 48 px clip, team palette of the local side, position snapped to 48 (`0x462710`) | V; colours of rows 0x14..0x16 G (likely ok / blocked tints, come from the palette resource) |
| Shade rows | 0..0x13 are light levels; 0x14..0x16 extra rows in the same resource | V for use, G for count |
| Tables | resource type 0x44e (8 bpp, 0x100 bytes per row), 0x44f (15 bpp) / 0x450 (16 bpp) (0x200 bytes per row, u16), 0x479 (0x400 bytes per row; used for 16 bit source and alpha sprites); bank offset (mgr+0x1c - 1) * mgr+0x14 | V |
| Daylight tint | `0x418650`/daylight keyframes call `0x4786f0(mgr, r, g, b, 0xb, level)`: stores RGB scale at mgr +0x28/+0x30/+0x38, row range +0x40 = 0xb, +0x44 = daylight level; 15/16 bpp tables are rebuilt (`0x4788b0`, `0x478b20`) multiplying channels by scale*gamma (+0x20) and ramping linearly back to plain gamma over (0xb - level + 1) rows; palettes with their flag byte set use gamma only | V mechanics, G ramp direction and visual effect |
| Gamma | `0x478680` sets +0x20 and rebuilds 15/16 bpp tables | V |

Sprite formats in the blitters (`0x47a990`, `0x47b0b0`, `0x47b8c0`, `0x47c070`, V): header +8 flags:
1 = raw rows (with 0x10 = 16 bit source pixels, 4 = colour key at header +0xa); 2 = span or RLE
(8 = alternate table variant `0x478530`); 0x40 = alpha RLE, only to a 2 byte surface. Surface bytes
per pixel = (bpp + 1) >> 3; bpp 0xf selects RGB555 blitters, 0x10 RGB565. 8 bpp surfaces are supported.

## 4. Light surface, fog and animation

| Item | Rule | V/G |
|---|---|---|
| Source | local side fog map (side +0x690, one byte per vertex, = daylight*8 + glow - vision, see fog doc) | V |
| Light surface | `0x47d5e0` fills one 48 x 48 block per tile with `0x480dec`; only tiles whose 4 corner bytes changed since the cached copy (+0x18) are refilled | V |
| Uniform tile | block filled with byte >> 3 (no +4) | V |
| Gradient tile | bilinear over 48 px (8.24 fixed point, /48 and /2304), then mapped through a dither table at `0x4bf284` selected by (row & 3) << 0xa with 0x100 byte sub tables per column phase: ordered 4 x 4 dither to the shade index | V structure, G table contents |
| Flat terrain shade | uniform tiles drawn directly use (byte + 4) >> 3 (rounded), unlike the light surface (truncated) | V |
| Water / animated tiles | cell row = `0x515a04` (0..7) when the sheet has >= 2 cells | V; sheets with 4 cells and rows 4..7 unexplained (G) |
| Wind (overlays) | strength = max over wind regions `0x4d9bbc` (count `0x515574`) that intersect the view, sampled at two points (`0x470b90`); 0 -> frame 0, 1 -> `0x515a08` >> 1, 2 -> `0x515a08` & 3 | V |
| Ellipse frame | `0x515a04` & 3 | V |
| Counters | `0x515a04`, `0x515a08` += 1 & 7, `0x515a0c` = (+1) % 0x12 (18), once per view draw unless paused (`0x4296b0`) | V |

## 5. Selection and health art (V)

Table `0x4bc88c`, 12 byte records {threshold, ellipse slot, health slot} in the common archive
(`view+0x340d8 + slot*4`): (0x2c=44, 0x134, 0x107), (0x37=55, 0x135, 0x106), (0x43=67, 0x136, 0x105),
(0x53=83, 0x137, 0x104), (0x66=102, 0x138, 0x103), (0x7d=125, 0x139, 0x102), (0x9a=154, 0x13a, 0x102),
(0xbd=189, 0x13b, 0x102), then threshold 0xe7=231. Index = first record whose threshold >= 2 * display
size (entry +0x38, doubled in place). Ellipse uses that record; the health bar uses the previous
record (index - 1, floor 0).
Health: frame = 2 - level, level = 2 if ratio > 0xb2 (178), 1 if > 0x66 (102), else 0 (`0x469a10`);
width clipped to w * ratio >> 8. Shown when cheat or option `0x42e8c0(7)`, or when the object is
selected (virtual +0x40) and of the local side, or virtual +0x34 is true, and maxhp != 0.

## 6. Frame pacing (V)

- The world is drawn once per logic pass: `0x4577b0` calls `0x465a50` at `0x4587d1`, after
  `0x4296f0`, `0x429060`, `0x428ec0`, while still holding the world mutex `0x4c4d58`; then it sleeps
  for the rest of `[0x5154dc]` ms. Other callers: initial draw `0x457840`, loaders and menus
  (`0x4295c2`, `0x430f44`, `0x435b54`, `0x43f1f6` with force 1, `0x413552`, `0x41b701`, `0x458b3c`, `0x458dd0`).
- No interpolation: positions are read straight from the objects (virtual +0x14/+0x18). All view
  phases advance once per draw, i.e. once per tick in play.
- `0x463b00` (82 ms timer) only animates the loading spinner (`0x463a40`, 31 frames at (0x17a,0x3f)); it is not a view timer.
- `+0x87` set: the world is not redrawn. `+0x86` set (G: in-game menu open): the world is redrawn
  only when `0x515a04 == 0` (every 8th draw) or when forced, then dimmed (step 11).

## 7. Unknowns

- Colours of shade rows 0x14..0x16 and of shadow palette `view+0x380fc`; palette 3 row 0 meaning.
- Meaning of virtual +0x34 (health visibility), +0x48 (flash), marker +0x44 (from the proxy's
  building virtual +0x40 via `0x469880..0x4698d0`) and entry +0x48.
- Exact (2,2) term in the overlay position; which layer is virtual +0x24 vs +0x28 (push order inferred).
- Dither table `0x4bf284` contents; whether the 0x479 alpha path blends with the background.
- Ramp direction of the daylight RGB scale across rows; `0x5153fc` use.
- Animated terrain sheets with fewer than 8 cells.
