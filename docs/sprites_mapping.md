# Sprite and animation mapping

## Evidence and verification status (2026-10-25)

The mapping is **not fully screenshot-verified**. New headless runs in this worktree
were blocked by the sandbox: `unshare -rn` succeeds, but loopback configuration
fails with `Cannot open netlink socket: Operation not permitted`. An initial run
that continued past this failure reached the runtime's UDP `socket` call and
terminated with `RaiseException e06d7363`, before the menu. These failures do not
verify any animation. Logs remain local in `work/mappingdebug`, `work/startup.log`
and `work/finalprobe`; no reference executable, decompilation, masks or pixels are
included in the repository.

Evidence labels used below:

- **R**: resource structure read from the installed archives; exact ids, offsets,
  counts and durations, independent of a screenshot.
- **C**: selection/render behavior established by inspecting the local reference
  decompilation and, where its recovered call signatures are ambiguous, the
  corresponding call sites. Only metadata and independently written readers are
  stored in the repository.
- **T**: previous running-game animation traces, decoded by `zspr_anim.py` and
  `zspr_fx.py`; these establish active slots, not rendered pixels.
- **P**: comparison with a saved reference screenshot. Older tools use RGB
  tolerances of 40 or 48 per channel: their `1.00` scores are **not pixel-exact**.
  A visual sheet or a trace alone does not qualify as P.

The earlier notes' claims of pixel-exact matches should be read as tolerant P
comparisons. Their initial idle/direction checks remain useful, but do not close
attack, death, construction or effect verification. Unknown owners and remaining
pixel checks are listed explicitly below; no guessed owner is exported as known.

## Resource and coordinate contract

`docs/data/sprites_mapping.json` contains the previous resource inventory (90 UGI
types, 112 nonempty anim items, sprite header facts). `type_anims.hpp` supplies the
initialized type-to-animation ids. `SpriteDb` reads frames from the user's
`Res/Units.xbe` at runtime; new headers contain ids, offsets and selection facts
only. Frame timing and image hotspots must still come from the installed assets.

- **R/C**: Anim group `0x515`: `Anim`, version 3, 128 u32 slot starts at +8,
  32-byte frames from +0x208. Start 0 means absent; sprite id 0 terminates a
  sequence. The preceding header frame's field [2] is its looping sound.
- **R/C**: frame fields are sprite id, light id, sound id, fire marker, dx, dy,
  height, duration. Duration is in 12 Hz ticks; 0 holds the current frame. A hold
  after timed frames must not skip the timed prefix. Sounds/fire markers are
  events, not extra image layers.
- **R/C/P**: body group `0x44c`, shadow `0x454`, same sprite id. Image header:
  width/height +4/+6, flags +8, palette +0xc, cell dimensions +0x10/+0x12,
  signed hotspot +0x14/+0x16. Flag `0x40` is palette-indexed alpha RLE, not RGB565.
- **C/P**: body top-left = world position minus view origin minus hotspot minus
  `(0, height)`. Frame [6] supplies height for most ground classes; it is
  **subtracted**, correcting the previous "added Y offset" description.
  Bird/Zeppelin instead use object +0x7c; projectile height sources differ.
  Shadows use ground position, without height.
- **C/P**: palette id <4 becomes `id + 16 + 4 * side.Color`; larger ids are fixed.
  The previous Color=3 experiment supports this mapping with tolerant P scores.
- **P**: for the existing unclamped 800x600 probes, view origin is
  `(Start_X*48-298, Start_Y*48-300)`. Supply the actual origin when the view is
  clamped or moved. A single shade of 10 is a probe convenience, not a general
  lighting rule.

`AnimSet` rejects missing/truncated headers and out-of-range frame starts, and
honors both timed prefixes and zero-duration hold frames. `sprite_for` now selects
by kind instead of applying Marine slots to every type. Its compatibility input
is dir8; full-angle callers use `body_sequence`, `turret_sequence`,
`boat_sequence`, `johnka_sequence` and `sprite_for_sequence`.

## Kind-specific animation selection

Direction: dir8 = `((angle+16)>>5)&7`, dir32 = `((angle+4)>>3)&31`, east = 0,
counter-clockwise. Angles are 0..255. "Absent" below means no sequence in the
installed resource; requesting a slot does not imply it exists.

| Kind | Body/secondary animation rules (C/R unless otherwise stated) | Existing running evidence |
| --- | --- | --- |
| Marine 0 | Idle 0/frame dir8; move 1+dir8 loop; attack 9+dir8 once; die 17..19; fidget 20..22; icon 23. | T: Footman/Bear attack/move. P: initial idle on nine types, directions and Color. |
| Spider 1 | Idle body 0/frame 0; move body 1+dir8 loop. Attack changes the turret, not legs. Turret initially 1/frame dir32, later 0/frame dir32, firing 2+dir32 once. Icon body 9, variant offset `race*4`. Death hides both anims and spawns 152 plus debris 113; no body death/fidget sequence. | T: moving legs and firing turret. P: initial turret; body comparison was only 0.63..0.77. |
| Building 2 | Built slot 1 loop. Construction seeks slot 0; slot 6/frame 0 is an overlay while object state +0x68 is 1. Icon 7. Destruction replaces the anim resource, described below. | T/P: built body, T: damage fire. |
| Zeppelin 3 | Idle/turning slot 0/frame dir32. At dir8 centres move uses 1+dir8 loop; changing direction preserves phase. Die requests 17..19, absent in anims 5/26; separate decoration 152 is spawned. Icon 23. | P: Zeppelin idle at altitude; Zeppelin2 remained a poor match. |
| Truck 4 | Idle/turning 0/frame dir32; aligned move 1+dir8 loop; attack 9+dir8 once. Death hides the body and spawns 152/113. Icon 23. | T: Catapult attack. P: idle dir32. |
| Bird 5 | Idle/turning 0/frame dir32. Flight at dir8 centres and steering cooldown +0x8c == 0 uses 1+dir8 loop; otherwise 0/frame dir32. Anim 13 has 29 one-tick flight frames; anims 6/9 have four. Firing occurs in flight through the weapon object, with no separate attack sequence selection. Die requests absent slots 17..19 and can spawn 152. Icon 23. | Flight and death still need P; previous moving captures are not synchronized with dumps. |
| Headtruck 6 | Body slot 1+dir32 of body angle +0x78; hold frame 0 when stationary, advance wheel cycle when moving. Turret angle +0x7c; turret 1 initially, 0 after attack, seek dir32; attack 2+dir32 once. Death hides both and spawns 152/113. Icon body 39, variant offset `race*4`. | T: wheel cycle and firing; P: initial body/turret on nine types. |
| Defence 7 | Construction slot 0 seek; built slot 1 loop. With separate turret: same turret convention as Headtruck, body remains slot 1. Without turret: attack body slot 8 once, then 1. Overlay slot 6/frame 0 while state +0x64 == 1; icon 7 plus `race*4`. Death replaces resource with rubble. | T/P: built towers, P: initial separate turrets; attack/death still need P. |
| Boat 8 | Anim 23; orientation +0x68 chooses base 0 or 5. Closed base+0 loop; opening +1 once, then open +2 loop; closing +3 once, then closed; turning +4 once, then orientation toggles. Scripted travel also uses base+0. No Marine combat/fidget slots. | C/R only; requires a `[Boat]` probe, not `[Team]`. |
| Cannon 9 | Idle/turning 0/frame dir32 (angle byte +0x81); hop move 1+dir8 once; fire 9+dir8 once; icon 17. Death hides body and spawns 152/113; slot 17 is not a death animation. | T: hop/fire; P: initial idle. |
| Johnka 10 | Anim 29; orientation +0x68 selects slot 0 or 1, each 32 one-tick frames, looping. No direction-indexed idle or Marine combat slots. | C/R only; requires a `[Johnka]` probe. |
| Camera 11 | No body animation. | R/C. |

For Truck/Bird/Zeppelin, the caller's cycle-ready flag includes alignment to an
8-direction centre; Bird also requires its steering cooldown to expire. No
non-Marine fidget sequence has been established. Turret slot 1 is the initial or
cooldown idle sequence; slot 0 is restored after firing. Frame [3] triggers the
weapon during firing, not at the beginning of every attack.

C references: Bird `0x407870/0x407a60/0x407b10`; Truck
`0x460550/0x4607b0`; Headtruck `0x42c4d0/0x42c740/0x42c880`;
Spider `0x44ae70/0x44ab90/0x44b360`; Zeppelin `0x471740/0x4718b0`;
Cannon `0x416710/0x416b90/0x417090`; Boat `0x408670/0x408770`;
Johnka `0x42dc60/0x42dce0`; Defence `0x41d3b0/0x41d790`.

### Construction, damage and destruction

**C/R** (`0x410c30`, Defence `0x41d700`): with N construction frames, seek
`floor((N+1)*hp/maxhp)-1`. If the quotient is zero, hide the body. The construction
clock is HP progress, not elapsed animation ticks. At full HP the caller switches to the built state/slot; it must not keep
seeking construction. Once built, the body remains slot 1 at every HP level.

**C/T** (`0x410cc0`, `0x41d790`): damage stage for positive HP is
`3-floor((4*hp-1)/maxhp)`. At >75% HP there is no fire; at <=75%, <=50%, <=25%
use separate Decoration anims 154, 155, 156, slot 0 looping. Spawn at building
position plus `(0,1)`, height 0. Damage sounds are 151/152/153; these sound-set
numbers must not be confused with animation ids of the same values.

**C/R** (`0x4106a0`, `0x41d3b0`): destruction is **not building slot 5**. The
existing animation is replaced by anim 116 for radius <97, 115 for 97..144, or 114
for >=145; select a random slot 0..19, once. Each is one 250-tick frame with height
-1. Construction cancellation instead requests an absent sequence. Body deletion
waits for the destruction sequence to finish. Construction stages, overlay,
rubble selection and lifetime still require synchronized P checks.

## Projectile and effect data

`effects.hpp` contains weapon parameter-to-animation ids and a complete metadata
inventory of the 32 populated anim items in 31..199 (including turret anims
50..55). All other ids in this interval are empty in the installed Units.xbe.
Counts/durations below are R; ownership/selection is C unless marked unknown.
Exact per-frame light, sound, duration, displacement and hotspot data is read at
runtime through `AnimSet` and sprite headers.

| Class / role | Anim ids | Slots and timing | Height / draw handling | Running evidence |
| --- | --- | --- | --- | --- |
| Arrow, vtable 0x48f558 | 111,112,158; parameter mapping in header | dir32 slot, looping; 111/112: one tick/frame, one frame; 158: six one-tick frames | World fixed256 +0x40/+0x44, height fixed256 +0x58; body/shadow in ordinary list. | Previously seen in T, new P pending. |
| Axe, 0x48f7e0 | 101,102,109 | dir32 slot loop; three, three, four one-tick frames respectively. Collision requests slot 32 once, absent in these resources. | Same position fields as Arrow; height = frame [6]. | T: 101/102 flight. |
| BallExpl, 0x48f8f4 | 100,117,118 | dir32 slot loop; three/eight/three one-tick frames. Anim 100 also contains slot 32 (11 ticks); this alone does not prove BallExpl uses that slot. | Position fixed256 +0x44/+0x48, height fixed256 +0x54. Explosion comes from a separate weapon object. | T: anim 100. |
| FireThrow, 0x4927a8 | 157, fixed in constructor | dir32 slot once, ten one-tick frames; direction changes preserve phase | Position +0x24/+0x28, height pixels +0x3c; ordinary list. | T: frames observed up to 8; R establishes ten total, not eight. |
| SplashDamage, 0x4943a4 | 150/151 from its own descriptor d0 | random slot 0..2 once; slot 0 = 11 ticks, slots 1/2 = ten ticks | Position +0x24/+0x28, height pixels +0x44. Light accessor returns current frame [1]. | T: both ids. |
| Shockwave / DecorationOnce, 0x49406c | 105 | slot 0 once, 30 one-tick frames | Position +0x24/+0x28, constructor height +0x34 (call-site dependent). Alpha pixels need background compositing for P. | T: ring; P pending. |
| Vehicle/flying death explosion / Decoration, 0x4904ac | 152 | random slot 0..2 once, 11/10/10 ticks | Caller supplies height; airborne explosion keeps aircraft altitude. | T; P pending. |
| Vehicle debris / Decoration | 113 | random slot 0..19 once, one 250-tick frame | Caller uses height -1: underlay. | T: one-frame variants. |
| Building/Defence rubble | 114..116 | random slot 0..19 once, one 250-tick frame | Frame [6] = -1: underlay. | C/R; P pending. |
| Damage fire / Decoration | 154..156 | slot 0 loop, ten one-tick frames | Constructor height 0; frame light follows flame animation. | T; P pending. |
| Turret/muzzle lights | 50..55 | slots 0/1 are dir32 seeks; firing slots 2..33. Firing is 1 tick for 50/51/53/55, 3 ticks for 52, 11 ticks (durations 1,10) for 54. | Second body/shadow pair, same object height; do not invent an additional muzzle sprite. | T/P for selected turrets; firing P pending. |
| Immediate-weapon muzzle / Decoration | 106 | slot 0 once; resource also contains slots 1..3, all one tick | Spawn point is 20 pixels along the firing direction; caller supplies height. | C/R; P pending. |
| Immediate-weapon beam segments / Decoration | 107 | direction slot 0..31 once, one tick | Segments placed along the beam in approximately 30-pixel steps; height 0. | C/R; P pending. |
| Immediate-weapon impact / DecorationOnce | 110 | slot 0 once, four one-tick frames | Target position and target height. | C/R; P pending. |
| Owner not established | 104,153 | 104: five slots x10 ticks; 153: three slots, 11/10/10 ticks | Resource decoded; class, spawn conditions and offsets remain unknown. | R only. |

Arrow params 0..5 select 111,112,112,158,111,111; Axe params 0..5 select
101,102,109,109,109,109; BallExpl params 0..6 select
100,100,100,100,117,118,100. SplashDamage descriptor params 0..10 select
150,150,150,150,151,150,150,151,151,151,150. These are separate descriptor tables,
not interchangeable "weapon ids" (`docs/data/weapon_class_*.csv`). Immediate
weapon descriptors (`weapon_class_w2_13.csv`, params 1/5/6/7) supply 106 at +0x28
and 110 at +0x2c; `0x46ed90` constructs those decorations and the directional 107
beam segments. These are code-established owners, still without a P check.

Light dispatch differs between units and effects. The tick queries unit virtual
+0x70, projectile/effect virtual +0x80, and Decoration virtual +0x5c. Arrow and
Axe emit frame [1] only while active (object +0x38 != 0); FireThrow and
SplashDamage return current frame [1]. BallExpl applies a height-dependent
reduction to frame [1], clamped to zero. Its +0x70 method is a different weapon
query, not the light accessor. Unit +0x70 usually reads the body animation;
separate turret frame [1] is not directly queried by that accessor.
`render_rules.hpp` records the light dispatch offsets.

## Trees, decorations, lighting and draw order

### Terrain overlays

**R/C**: map cell upper 16 bits are an overlay sprite id in Tiles{TileSet}.xbe.
Both Tiles0 and Tiles1 contain nonempty shadow ids 4908..4937 and 5036..5132;
bodies use the same id in group 0x44c. Do not classify this whole range as trees.
Mission 4's known tree ids are 4909..4913. Their bodies are four stacked cells
(192x768 total); use the header's cell dimensions and hotspot, not full-sheet
height. Centre is `(tile_x*48+24,tile_y*48+24)`.

**C** (`0x465a50`): overlays enter the same sorted display list as units. Still
wind uses frame 0; slow wind uses `(counter&7)>>1`; fast wind uses `counter&3`.
The counter advances on unpaused view updates, and wind strength comes from view
wind regions (`0x470b90`). It is not a random phase derived from tile coordinates
and is not an Anim sequence. New per-frame P checks remain pending.

### Light masks and per-tile maps

**R**: light.xbe group 0x455 masks have u16 width/height (20x20), followed by 400
bytes. Nonempty ids: 1..16 and 64..71. Ids 17..63 are empty. In 1..16 the corner
is zero and centre increases from 10 to 152; in 64..71 the corner is 152 and centre
zero. Thus the previous blanket statement "all masks contain darkness" was wrong:
1..16 add light; 64..71 describe vision darkness (152 = dark).

**C**: these functions in tick `0x4577b0` construct lighting/visibility maps,
**not the display list**:

1. `0x47cb30`: add glow masks to the global brightness grid, saturated at 152.
   It starts each tick at daylight shade *8. Sources include units (virtual
   +0x70), projectiles with a nonzero light accessor (+0x80), and decorations
   (+0x5c). Light positions use ground world position, not sprite hotspots or
   projected altitude.
2. `0x47d070`: take the minimum of vision-darkness masks on the current side's
   temporary visibility grid (initially 152). Living side-owned units supply
   vision through virtual +0x74 and their descriptor; Boat/Johnka use 71.
3. `0x47d430` / helper `0x480d20`: update the persistent side map with the maximum
   of its previous value and nonnegative `(brightness - vision_darkness)`.
   The side map is first limited to 32 by `0x47d460`, retaining explored terrain
   at low brightness while current visibility can raise it again.

**C/R**: grid spacing is 48 pixels, dimensions map width+1 and height+2; data
pointer +0x14, spacing fields +0xc/+0x10. A mask starts at
`position + cell_size - mask_size*cell_size/2` (position minus nine tiles for a
20-wide mask). The 20x20 samples define a 19x19-cell footprint. Fractional tile
positions use four-neighbor interpolation; aligned updates select the interior
samples. Uniform terrain shade is `(combined_brightness+4)>>3`, 0..19, directly
indexing the palette; it is **not** `19-(darkness>>3)`. Per-pixel gradients require
the interpolated grid. `LightMask` reads these samples at runtime; no mask bytes
are embedded. Buildings obtain vision id from descriptor +0xdc, Defence +0xe8;
these are visibility sources, distinct from frame-driven glow. Screenshot
validation of mask positioning/interpolation and night lighting is still open.

### Display list and layer order

**C**: view `0x465a50` builds the display list (`0x469a30` for objects,
`0x469ae0` for terrain overlays), sorts through `0x472fa0` using comparator
`0x464b10`, and draws entries in list order. The sorting helper uses the inverse
of the comparator's sign, so reading that sign alone gives the wrong direction.

- Height -1 entries precede ordinary entries; their mutual order is unspecified.
- Ordinary sort key is ascending ground Y, then descending X. Height does not
  move the sort key into the air: it is the entry's separate `(0,height)` offset.
  Bird and Zeppelin have no separate "draw all air last" pass.
- Each item draws base shadow, base body, secondary shadow, secondary body, then
  the optional construction overlay. Shadows use shade 12 at ground position;
  body positions subtract the height offset. Shadows are **not** all drawn in a
  single global first pass.
- Terrain overlays participate in the same ordering with height zero. Render
  groups and object field offsets for each unit/projectile/effect class are in
  `render_rules.hpp`. The renderer now consumes this ordering; see the current
  [integration inventory](render_integration.md) for synthetic checks and pending
  reference validation.

## Reproducing probes and remaining work

`tools/zspr_run.sh` now accepts `GAME_DIR`, `BIN`, `VTABLES`; it creates its maps
inside a unique private game directory under this worktree's `work/`. Original
assets/maps are read through symlinks; Saves/profiles/settings are private copies.
It never creates temporary maps in the source installation. It forces dummy SDL,
uses `unshare -rn`, wraps the run in `timeout`, and reports setup/runtime failures
as nonzero exits. `SAMPLES=N SAMPLE_SHOTS=1` records dereferenced object dumps and
an accompanying `samples.tsv`; `KEEP_GAME=1` retains the private installation.

```
GAME_DIR=/path/to/game BIN=/path/to/aztec_recomp VTABLES=/path/to/vtables.txt \
  SAMPLES=80 SAMPLE_MS=83 SAMPLE_SHOTS=1 OPP=0 \
  sh tools/zspr_run.sh zprobe 3,83,30,7,33,87 work/zprobe 318 1500
python3 -B tools/zspr_compare.py /path/to/game work/zprobe \
  --objects s_0.txt --shot shot_04.ppm --report work/zprobe/pixels.json
```

The new comparator uses the active Anim frame from the dereferenced dump, the
class's position/height fields, and the requested team/shade/origin. It reports
exact RGB565 pixel counts, skips partial alpha without an explicit background,
and does not search for a better
frame/position/shade. `--tolerance 40` adds a separately named tolerant score.
Occlusion, team colour, lighting, gamma and a tick between scan/shot can reduce
scores; a low score must not be called verification. Alpha effects need a separate
background-composited comparison (`--background` supplies a registered image;
the assumed model is named in the report). `--after-objects` and
`--require-stable` validate pre/post-shot scan brackets; these are not atomic
render/tick snapshots. See [render_integration.md](render_integration.md).
`zspr_scan.py` remains an exploratory candidate
search, not a proof of current animation state.

This worktree reran comparisons against the saved `work/zspr/fB` reference capture
in the main checkout, reading its files only. At shade 10/team 0: anim 200 body
slot 1 frame 0 matched 10355/10356 pixels within tolerance, but 0 pixels exactly;
anim 205 matched 11829/11837 within tolerance, 113 exactly. Tower anims 215 and 314
scored approximately 0.90/0.87 within tolerance. These support the built-body
silhouettes but do not add new exact or construction/attack verification. Reports
are local in `work/archived_fB_tolerance.json`.

Remaining acceptance work: synchronized P comparisons of non-Marine attack/death,
Bird flight, Boat/Johnka script phases, construction/overlay/rubble, all projectile
and effect frames including alpha compositing, tree wind phases, glow and visibility
interpolation, and overlapping objects/shadows/air. Unknown effect owners
104/153 also remain open. No guessed mapping has been promoted to P.

Validation in this worktree: the full CMake build in `build-sprite-mapping/`
passes, and `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir
build-sprite-mapping --output-on-failure` passes all five tests. Added checks cover
kind-specific selection, construction/damage boundaries, hold-frame timing,
malformed animation indices, every effect inventory entry and all 24 light masks.
The comparator's synthetic positive control matched 10356/10356 pixels; moving
its origin by one pixel reduced the exact score to 0.1374. This validates the
comparison tool, not the reference game's rendering. The control and report
remain ignored local data under `work/comparator_control/`.
