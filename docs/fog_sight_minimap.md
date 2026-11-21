# Fog of war, sight and minimap of the original (decompilation only)

Source: `work/decomp/all_full.c`, extra decompiles in `work/gaps/dec_0042a510.c`, vtable slots read
from `Aztec.exe`. Nothing was run. V = read directly in the decompiled code, G = inference.
Complements `docs/sight_fog.md` (which describes the reimplementation) and `work/gaps/idle_autoengage.md`.

## 1. Data model

| Item | Fact | Mark | Where |
|---|---|---|---|
| Per side fog map | `side +0x690`, one byte per vertex; object fields `+4` width, `+8` height, `+0xc/+0x10` cell size, `+0x14` data | V | `0x402a20`, `0x47c980`, `0x432050` |
| Grid size | (map width + 1) x (map height + 2), cell 0x30 (48) px | V | `0x46a1a0` (allocates the two scratch maps with the same size) |
| Scratch "ambient/light" map | global `DAT_004d9c10`, saturation cap byte `+0x18` = 0x98 (152), glow archive at `+0x1c` | V | `0x46a1a0`, `0x47cb30` |
| Scratch "darkness" map | global `DAT_004d9c0c`, mask archive at `+0x18` | V | `0x46a1a0`, `0x47d070` |
| Mask archive | `res\light.xbe`, resource type 0x455 (1109), looked up by the mask id | V | `0x46a1a0`, `0x47d070`, `0x47cb30` |
| Mask format | u16 width, u16 height, then bytes row major; the fractional path reads the next row at +0x14 (20), so masks are 20 wide | V header, G "all masks are 20 wide" | `0x47d070`, `0x47cb30` |
| Side array | `DAT_004dd2f4`, stride 0x69c (1692); side count `DAT_004cc23c` (`0x43d1e0`); local side `DAT_004cc240` (`0x43d200`); `DAT_004cc244` (`0x43d1f0`) | V | |

The byte is a brightness, not a state enum. The three states come from thresholds:

| State | Byte value | Mark | Where |
|---|---|---|---|
| Never seen | 0 (map is zero filled when created) | V | `0x449440` (`0x47c980(map,0)`) |
| Explored, not lit now | 1..0x20 (32): each tick the map is clamped to at most 0x20 before the new light is merged in | V | `0x4577b0` -> `0x47d460(map,0x20)` -> `0x480f07` |
| Visible now | above 0x20 (33..0x98) | V | `0x402490`, `0x410340`, `0x401480` |

`0x449980` fills the map with 0x18 (24) in one path (G: a "map already explored" start; the
condition was not read).

## 2. Per tick update (`0x4577b0`, same block also in the `.vis` loader `0x430ae0`)

Order, all V unless marked. No period counter guards this block: it runs on every simulation
tick of `0x4577b0` (G: nothing was found that skips it).

1. Ambient map := `DAT_005153f8 << 3` (daylight level times 8). `DAT_005153f8` is written by
   `0x418380`, `0x4184a0`, `0x418650` (daylight keys).
2. Glow: for every live object of list `DAT_004cc3d0` (units and buildings) whose virtual `+0x70`
   is non zero, of list `DAT_00515a98` (virtual `+0x80`) and of list `DAT_004c1978` (virtual `+0x5c`),
   stamp that mask additively into the ambient map with saturation at 0x98 (`0x47cb30`).
   G: the last two lists are projectiles and effects.
3. Every side map := min(old, 0x20) (`0x47d460`). This is what turns "visible" into "explored".
4. For each side s: darkness map := 0x98 everywhere. Exception: 0 everywhere when option/cheat
   0x2e (46) is on (`0x42e8c0`), `DAT_004cc244 == 1` and s is the local side (G: reveal map in
   single player). Then for every object of `DAT_004cc3d0` with virtual `+0x44` (side) == s, stamp
   the sight mask returned by virtual `+0x74` with a per cell minimum (`0x47d070`).
5. Side map := max(side map, ambient - darkness) where ambient >= darkness (`0x47d430` -> `0x480d20`).
6. Each display proxy of list `DAT_004c4ce0` gets `+0x129` := its virtual `+0x10` against the
   local side map; then `0x42a940` synchronises proxies (section 4).

Consequences (V arithmetic): brightness = ambient + glow - mask darkness; outside every own mask the
darkness is 0x98 = the ambient cap, so glow alone never reveals anything. At a low daylight level
the lit radius shrinks, because `level*8 - darkness` must exceed 0x20. There is no height or
terrain term anywhere in the stamp or the test (V for `0x47d070`, `0x402490`): no line of sight.

Stamp placement (`0x47d070`, `0x47cb30`, V): origin = pos + cell - (cell * mask size)/2 per axis,
floor division by 48; cell aligned positions copy whole bytes, other positions use the weight
tables `DAT_0051691c`/`DAT_00516d1c` (sub cell offset, 0x200 entries per offset) over two columns
and two rows. Clipped to the map.

Sight mask id accessors (virtual `+0x74`), all V:

| Class | Accessor | Returns | Zero when |
|---|---|---|---|
| Marine | `0x432f80` | `desc +0x18` | object `+0x64 == 0` (G: not on the map, e.g. carried) |
| Building | `0x40fa30` | `desc(+0x64) +0xdc` | byte `+0x5c == 0` |
| Defence | `0x41cb80` | `desc +0xe8` (per `docs/descriptor_building_defence.md`) | not re-read here |
| other unit classes | `0x407240` (`desc +0x20`), `0x42bc50` (`+0x6c` record `+0x20`), `0x415dd0` (`+0x68` record `+0x18`), `0x471180` (`desc +0x1c`) | | byte `+0x5c == 0` |

Byte `+0x5c` is also the flag the minimap requires for unit dots (`0x465940`) and that
`0x41d790` treats as "target still valid". G: "object is active/placed".
Glow mask accessors (virtual `+0x70`): Building `0x40fa10` (current animation record, default
`DAT_004c1300`), Defence `0x44a100`, Marine `0x432f60`.

## 3. Visibility tests

- `0x402490(this, fogmap)` (virtual `+0x10` of 30 vtables: units, projectiles, effects), V:
  false when x (`+0x24`) or y (`+0x28`) is outside 0..width*48 / 0..height*48; otherwise bilinear
  interpolation in 8.8 fixed point of the fog bytes around the position, first sample at
  (x/48 + 1, y/48 + 1), and returns `value > 0x2000`, i.e. brightness strictly above 32 (`0x403050`).
  G: the other three samples are the +1 neighbours (not read line by line).
- `0x410340(this, fogmap)` (virtual `+0x10` of Building vtable `0x48fd20` and Defence `0x4905bc`), V:
  takes the bounding rectangle from virtual `+0x1c`, clips it to the map and returns true when
  any of its four corners has a fog byte above 0x20. No interpolation.
- `0x42a510` (proxy virtual `+0x10`), V: forwards to the source object's test while the source
  pointer `+0x30` is set, else uses `0x402490` on the proxy's own last position.
- `0x46ba50` is not a visibility test. V: it fills a rectangle {x-0x1c, y-0x30-h, x+0x1c, y+0x10-h}
  (28 px half width, 48 up, 16 down, h = virtual `+0x18` height). G: it is the default virtual
  `+0x1c` bounds used by `0x410340` style tests and by the renderer.

## 4. Sharing between sides

- V: step 4 above only accepts objects whose side index equals the map's side. The alliance mask
  `side +0x648` (test `0x4472e0`, set/clear `0x4476b0`, `0x447710`, `0x447770`) is never read in
  `0x4577b0`, `0x430ae0` or the renderer `0x465a50`. Allies do not share vision.
- V: `0x4472e0` callers are targeting and AI code only (`0x404b90`, `0x447c30`, `0x447e00`,
  `0x451c30`, `0x452c70`, `0x454820`, `0x455570`, `0x45b1e0`, `0x45d4d0`, `0x46eb4a`...).
- V: AI code `0x454820` reads its own side map with threshold "byte > 7"; `0x45bd50` passes the
  side map and the constant 8 to the placement test (G: building sites must be explored, 8 or more).
- V: `0x432050` builds the side "explorer" knowledge list (`+0x68c`, records of 0x16c bytes) from
  the fog map, or from a temporary map filled with 0x28 (40, everything visible) when its flag
  argument is set.

## 5. What the fog hides

- Units, projectiles, effects (renderer `0x465a50`, V): every live drawable of kind != 1 returned by
  the screen query is drawn only when its virtual `+0x10` against the local side map is true. Own
  units pass because they light their own cell (G).
- Buildings (V): kind 1 objects are never drawn directly. Each Building/Defence creates a display
  proxy (`0x42a650`, vtable `0x492830`, kind 8 from `0x42a4f0`; callers `0x40fb70`, `0x41cce0`,
  `0x410ab3`..`0x410bb6`). Proxies with flag `+0x128` are drawn without a fog test.
- Remembered buildings (V, `0x42a940`): while `+0x129` (visible to the local side) is set the proxy
  copies position, height, bounds, alive flag `+0x114`, three state words `+0x118..+0x120`, side
  `+0x124` and byte `+0x5c` from the source and marks itself dirty. While not visible nothing is
  copied, so the last seen picture stays. A proxy whose source died is deleted only once its
  place is visible again; a proxy seen for the first time (`+0x12a`) marks overlapping orphan
  proxies for deletion (`+0x12b`). The four flags and the 0xc4 byte footprint copy are saved
  (`0x42a3b0`).
- Terrain shade (V): the renderer uses `(fog byte + 4) >> 3` as shade index (`0x465a50`).
- Sounds (V, `0x401480`): positional sounds are dropped when the local fog byte at the source cell
  is below 0x21 (not currently visible) or the source is too far from the view centre (cut at
  800 px, attenuation from 500 px).
- Targeting (V, from `idle_autoengage.md`): chase targets are dropped when virtual `+0x10` fails
  against the fog map of the attacker's side; `0x41d790` shows the same test for a weapon target.

## 6. Minimap (`0x464ba0`, called from `0x469e50`)

- V: 8 bit buffer of 0xb4 x 0xb4 (180 x 180) px (`0x47ea60`). Scale `this +0xc4` px per tile
  (special cased 1 and 2, general loop otherwise).
- V: the background is the local side fog map only: pixel = fog byte >> 3 (0..0x13), blitted with
  the table at `this +0xcc` and parameter 0xf (`0x47b820`). No terrain colour is sampled; never
  seen cells are index 0. (Agrees with `docs/formats.md`: minimap matches the `.vis` layer.)
- V: buildings: for every live proxy of `DAT_004c4ce0` with `+0x128`, the footprint cells
  (`0x42a720`) are drawn as squares of max(2, scale) px (`0x47b0b0`). Because proxies are frozen
  outside sight, remembered buildings stay on the minimap.
- V: units: every live object of `DAT_004cc3d0` with byte `+0x5c` set, kind != 1 and visible in the
  local fog map (virtual `+0x10`) is drawn as one square of the same size.
- V: colour = palette index `side(+8) * 4 + 0x11` (team colour selector times 4 plus 17), with
  shade argument 9, or 0x11 (17) when virtual `+0x40` is true and the object belongs to the local
  side (G: selected, drawn brighter).
- V: when option 2 is on (`0x42e880(2)`), white (0xffffff) line segments are drawn from the point
  list of the local side's base object (`side +0x688`, virtual `+0xc0`), and a white rectangle of
  half size `DAT_005159a0 * 4` around `DAT_005159a8` (G: camera frame).
- Update rate: G. `0x464ba0` is called from the panel draw `0x469e50`, itself called from the frame
  renderer `0x465a50` and `0x469b50`; no tick divider was seen, so it is rebuilt every drawn frame.

## 7. Unknowns

- Exact meaning of marine `+0x64` and of byte `+0x5c` (construction finished or placed).
- Contents of the masks 64..71 in `light.xbe` (radius per id) and whether all are 20 x 20.
- The three remaining sample points and weights of `0x402490`.
- Which classes populate lists `DAT_00515a98` and `DAT_004c1978`.
- Condition for the 0x18 fill in `0x449980`; meaning of option ids 0x2e and 2.
- Whether the minimap redraw is throttled by its caller, and what the proxy flag `+0x128` is set from.
- Whether aircraft height changes the stamp position (the stamp receives the plain position from
  virtual `+0x14`; height `+0x18` is not passed, V for the call site only).

## Correction (2026-11-21, from docs/renderer_rules.md)

V: the terrain shade is `(fog byte + 4) >> 3` only for tiles whose four corners are equal.
Other tiles use the per pixel light surface (bilinear blocks plus a 4x4 ordered dither,
built by `0x47d5e0` and `0x480dec`). Unit bodies sample only one row of that surface, at
their ground y.
