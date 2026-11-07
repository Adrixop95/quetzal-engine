# Original save-game stream

This describes `Saves/<profile>/single%i.sav` and `multi%i.sav` (also found
immediately under `Saves`). Both names use the same grammar. The clean reader is
`src/core/savegame.{hpp,cpp}`; it does not need an executable or game resources.
This is separate from the rewrite engine's own save format.

## Evidence and conventions

The writer is `004450e0`, the loader is `004455f0`. Serializer/constructor pairs,
virtual table dispatch and `RT_WRITE_TRACE` establish field order and widths.
Missing serializers were examined with `DecompAddrs.java` in a private copy of
the analysis project. No decompiled implementation, saves or assets belong in
version control. Existing `tools/savegame/*.py` provide useful cross-checks, but
have corrections listed below.

All scalar tokens occupy **four little-endian bytes**, including booleans,
bytes, signed shorts, counts, type IDs and references. There is no padding
between tokens or sections: Side's raw byte arrays produce unaligned words.
Raw blocks have exactly the stated byte length. Signed scalar values retain
their two's-complement wire representation. Narrow integers are truncated by
the original loader; boolean loaders generally test `value != 0`.

In the layouts below, hexadecimal numbers denote **memory member offsets**,
not file offsets. `u24` means one word from object member `+0x24`, `b20` a byte
member widened to a word, `s4c` a signed short widened to a word, and `Runit(30)`
a word encoding a reference. `D68` is a descriptor index, reconstructed as a
class-specific table base plus index times record size. Concatenation is in
wire order; `N * (...)` repeats a sequence. `blob(offset,size)` copies bytes
from that member. Unspecified member meanings remain explicitly unresolved;
see [object_fields.md](object_fields.md). Exact structure does not imply that
all original gameplay meanings have been recovered.

## Header and top-level order

| File offset | Bytes | Meaning |
|---|---:|---|
| `0x000` | 4 | Game mode word (single-player writes zero) |
| `0x004` | 4 | Campaign/mission state, loaded into `004dd164` |
| `0x008` | 256 | NUL-terminated mission `.mp` path, including unused tail bytes |
| `0x108` | 64 | Player-entered description, including unused tail bytes |
| `0x148` | 16 | Windows SYSTEMTIME: eight LE u16 values: year, month, weekday, day, hour, minute, second, milliseconds |
| `0x158` | 8 | View x, y in pixels (signed words) |
| `0x160` | 20 | Globals: local-side selector (`004cc38c`, writer can substitute zero), `004cc240`, `004cc244`, `004cc23c`, simulation tick `004c5730` |
| `0x174` | variable | Sections below |

Each of the first ten sections starts with a word count. There are no lengths
or tags identifying sections, and no checksum, compression or footer magic.

| Section | Pool/global | Each record after count |
|---|---|---|
| decorations | `004c1978` | decoration ID, Decoration |
| towns | `005156f8` | tagged object ID, Town |
| sides | `004dd2f4` | Side (implicit side index, no ID prefix) |
| tactics | `004d9b50` | object ID, tactic kind, Tactic variant |
| units | `004cc3d0` | unit ID, UGI type, world-class payload |
| weapons | `00515a98` | weapon type, weapon-class payload (no ID prefix) |
| effects | `004c1298` | effect ID, seven words `u4,u8,uc,u10,u14,u18,u1c`, LE IEEE754 double from float member `+20` |
| transports | `004c4ce0` | transport ID, Transport |
| scripts | `004cd698` | script ID, kind, Script variant |
| orders | `004d9b88` | order ID, Order queue |
| ui | `004cc7ec/004cc7e0/004cc7dc` | UI chain below, **no section count** |
| daylight | `004c1968/004c196c` | time word, stage word, **no section count** |

IDs are sparse pool slot numbers plus one, not record ordinals or addresses.
Zero references mean null. Object references use bit 31 to distinguish Town
from Tactic; decoration references use it to distinguish decoration variants.
Side references are side index plus one. Remaining reference families correspond
to the pools above. Forward references are normal: original loading first builds
ID-to-object tables and then calls fix-up virtual methods. The reader preserves
references as integers and never dereferences them. It checks stream structure,
not whether every reference resolves or every gameplay state is valid.

Town records written by this version have bit 31 set. The loader only constructs
a Town when that bit is present. The reader rejects untagged Town records and
unknown dispatch kinds rather than guessing their lengths.

## Shared payloads

**Visible** (`0046ae80` / `0046aef0`, 8 words):
`u10,u14,u18,u1c,b20,u24,u28,b2c`.

**World** (`0043e5b0` / `0043e680`, 23 words including Visible):
`Visible, Robject(30), u34,u38,u3c,u40,u44,u48,s4c,u50,u54,u58,b5c,b5d,b5e,b5f`.

**Animation at pointer member P** (`004031e0` load):
`resource_id, has_frames, [sequence_header_frame,current_frame if has_frames != 0],
b0c,u14,b18`. Five or seven words. Resource is an entry in `Units.xbe`, group
`0x515`. Sequence header/current are indices relative to the animation's `0x208`-byte header,
with a 32-byte frame stride. They replace pointers at animation `+8/+10`. The trailing fields are loop flag, tick within the current frame, and frame-changed flag (confirmed in `00403460/004034c0`).
This is **animation playback state**, not WaveSearch/pathfinding state.

**Harvest** (`0042b240` / `0042b280`): `Rscript(4),u8,uc`.

**Decoration** (`00418760` / `00418800`): `Visible,u34,Animation(30)`.
Both decoration tag variants have this same payload.

## World-class payloads

All start with World. Type selects the class using the 90-type membership in
[object_types.md](object_types.md), also exposed by `save_unit_kind`.
Minimum word counts include World and inactive animations, but exclude ID/type.

| Kind / class | Save / load | Tail in wire order | Minimum words |
|---|---|---|---:|
| 0 Marine | `00432ba0 / 00432d50` | `b60,u64,u6c,u70,u74,u78,u80,u84,u88,Runit(8c),Runit(90),Runit(94),u98,u9c,Reffect(a0),ua4,D68,Animation(7c)` | 45 |
| 1 Spider | `00449c30 / 00449e70` | `b60,u64,u68,u78,u7c,u80,u84,u88,u8c,b90,b91,b92,u94,Runit(98),Runit(9c),Runit(a0),D6c,Animation(70),Animation(74)` | 50 |
| 2 Building | `0040f3a0 / 0040f530` | `b60,b8c,u68,u78,u7c,Rdecoration(80),Reffect(84),Rtransport(88),D64,Animation(6c),has70,[Harvest if has70],has74,[Harvest if has74]` | 39 |
| 3 Zeppelin | `00470e80 / 00470fd0` | `u60,u68,u6c,u70,u74,u78,u7c,u80,u84,b88,Runit(90),D64,Animation(8c)` | 40 |
| 4 Truck | `0045fa80 / 0045fc00` | `b60,u64,u6c,u70,u74,u78,u7c,u80,u84,Runit(88),Runit(8c),b90,b91,u98,D68,Animation(94)` | 43 |
| 5 Bird | `00406f40 / 004070a0` | `u60,u68,u6c,u70,u74,u78,u7c,u80,u84,b88,u8c,u90,Runit(94),D64,Animation(98)` | 42 |
| 6 Headtruck | `0042b790 / 0042b9c0` | `b60,u64,u68,u70,u74,u78,u7c,u80,u84,u88,u8c,Runit(90),Runit(94),b98,ua4,ua8,uac,D6c,Animation(9c),Animation(a0)` | 51 |
| 7 Defence | `0041c6c0 / 0041c8f0` | `b60,b9c,u64,u68,Runit(78),Runit(7c),u80,u84,u88,u8c,Rdecoration(90),Reffect(94),Rtransport(98),D6c,Animation(70),has74,[Animation(74) if has74]` | 43 |
| 8 Boat | `00408250 / 00408300` | `u60,u68,Animation(64)` | 30 |
| 9 Cannon | `00415a80 / 00415c00` | `b60,u64,u70,u74,u78,u7c,b80,b81,u84,Runit(88),Runit(8c),Runit(90),D68,Animation(6c)` | 41 |
| 10 Johnka | `0042d7a0 / 0042d870` | `u60,u68,Runit(6c),u70,Animation(64)` | 32 |
| 11 Camera | base plus Camera constructor | `u60,u64` | 25 |

## Tactic and Town

**Tactic** (`0044e0f0` / `0044eb40`) has this fixed prefix (53 words):

```
global_004d9b78,u4,Rside(8),u10,u14,u1c,u20,u24,s28,u2c,u30,u34,
u3c,u40,u44,u48,u4c,u50,u54,Runit(58),b60,b61,u64,u68,b6c,b6d,u70,
u78,u7c,Runit(80),u84,u88,b8c,b94,Robject(98),b9c,b9d,uac,bb0,ub4,bb8,
ubc,uc0,sc4,sc6,bd8,bd9,be8,be9,bea,uec,u90,b18
```

Then:

1. Member count, members of 11 words each:
   `Runit(0),u4,u8,uc,u10,b14,b15,u16,u1c,u20,u24`, relative to each member.
2. `u3c` triples of words from array `+38`.
3. Target count, target references. If `b60` is nonzero, the first is a unit
   reference; all remaining references (or all when zero) are object references.
4. Route-present word. If nonzero: `u7c` rows of `(unit_ref,u,u)`, `u78` columns
   of `(unit_ref,u)`, then `u7c*u78` matrix words, row-major. Memory row stride
   is `0x19c`; row metadata is at `+190/+194/+198`. Column arrays start at
   `+a0f0/+a28c`. Counts describe serialized elements, not byte lengths.
5. Count and pairs of words for list `+c8`; count and pairs for list `+dc`;
   count and object references for list `+a0`.

Tactic kinds 0 and 5 have no tail. Kind 2 adds
`uf0,Robject(f4),uf8,Rside(fc)`; kind 3 adds `uf0` (a scalar, not a decoration
reference); kind 4 adds `bf0,bf1`. Kind 1 is not a valid dispatch here (Town
has its own earlier section).

**Town** (`0045d820` / `004597d0`) starts with Tactic, then:

```
Runit(f0),u158,u15c,u160,u164,Runit(168),b170,b171,u174,u17c,u180,
u18c,u190,u194,u198,u1b0,u1b4,global_byte_00515721,u178,Robject(16c),
blob(19c,14), Queue(f8), HarvestState(110), UnitList(128),
Robject(184),Rorder(188), BuildTree(f4)
```

Sizes in `blob` notation are hexadecimal. `Queue` is count from `+14`, capacity
`+10`, cursor `+c`, head `+8`, then count unit references in logical reverse
iteration order (`00411820`). `HarvestState` (`0040a580`) is
`u8,uc,u10,b14,Runit(4)`. `UnitList` (`00444b20`) is `u28,u2c,count`, then unit
references. `BuildTree` (`00411f50` / `004118f0`) is
`blob(16c,16c),blob(4,168),blob(2e8,1188),D1470,b2e0,b2e1,b2e2,u2e4`.
Its descriptor is a **byte displacement** from `004af130`, rather than a record
index. Empty Town payload is 5,624 bytes; empty Tactic is 236 bytes.

## Side, map and fog

**Side** (`00449540`, in-memory size `0x69c`):

```
u4,u8,uc,u10,u14,b138,u13c,u140,u698,u63c,u640,u644,u648,u64c,u650,
Rorder(694),Robject(688),
blob(18,20),blob(38,100),blob(144,16c),blob(2b0,5b),
blob(30c,16c),blob(478,168),blob(5e0,5a)
```

Then four count-prefixed reference lists, in order: `+654` objects, `+660`
objects, `+678` units, `+66c` orders. Explorer-present word; if nonzero, explorer
count then entries `(object_ref, 0x168 raw bytes)` (`0045ef30`). Then width,
height and width*height raw fog bytes from the map at `+690`; camera count and
count raw 20-byte camera records from list `+684`. Empty Side is 1,657 bytes.
No terrain `.map`, resource `.rm` or passability `.dat` is embedded here: the
loader reloads the mission path. Fog is saved independently for each side.

## Weapons and projectiles

All weapon payloads start with **WeaponBase** (`0046d840` / `0046d880`):
`Visible,Runit(30),u34` (10 words). The first reference is the owning unit.
Weapon **type** selects a class kind, not necessarily the same number:

```
type  0.. 9: 14,1,0,7,2,3,3,12,4,2
type 10..19:  2,10,2,5,1,2,6,7,7,8
type 20..29:  8,9,3,9,3,7,3,9,3,7
type 30..39:  0,1,0,2,11,12,1,7,7,12
```

| Kind | Tail after WeaponBase | Minimum total words |
|---|---|---:|
| 0 BallExpl (`00406440`) | `u38,u44,u48,u4c,u50,u54,u58,u5c,u60,u64,D3c,Animation(40)` | 26 |
| 1 Arrow (`00403560`) | `u38,u40,u44,u48,u4c,u50,u54,u58,u5c,u60,Runit(64),D3c,Animation(68)` | 27 |
| 2, 9, 12, 13 descriptor variants | `D38` | 11 |
| 3 Axe (`00404840`) | `u38,u40,u44,u48,u4c,u50,u54,u58,D3c,Animation(5c)` | 24 |
| 4, 5, 6, 10 base-only effects | empty | 10 |
| 7 SplashDamage (`0044bd00`) | `u3c,u44,D38,Animation(40)` | 18 |
| 8 SplashDamage (`0044c790`) | `u38,u40,u104,D3c`, then 16 triples of words at `+44` | 62 |
| 11 FireThrow (`00429aa0`) | `u3c,Animation(38)` | 16 |
| 14 (`004046b0`) | `Runit(38),u3c,u40,u44` | 14 |

Kind 13 has no corresponding type among the 40 known weapon types. Descriptor
families differ even where payload lengths match; the reader preserves the
index without constructing the static description object.

## Triggers, timers, transport, orders and UI

Script kinds start with scenario ID `u4`:

| Kind | Remaining payload |
|---|---|
| 0 | `u8` |
| 1 Trigger (`0045f1c0` load) | `u8,b0c,condition_count,condition_count*0x408 bytes,action_count,action_count*0x408 bytes` |
| 2 Timer (`0045f770` load) | `u8,b0c,b14`; if `b14!=0`: sample count from `+20`, `u1c`, sample_count words; otherwise `u18,u1c` |
| 3 enabled scenario object | `u8,b0c` |
| 4 scripted path | point count, count `(x,y)` word pairs |

Trigger condition/action packets are **raw 0x408-byte buffers**, including
unused payload bytes. The reader preserves them rather than executing commands
or interpreting unknown condition opcodes. Packet argument semantics remain a
separate unresolved layer of the mission scripting format.

Transport (`0042a2a0` / `0042a3b0`, 296 bytes):
`Visible,Runit(30),u34,u38,u3c,u40,u44,u48,u4c,b114,u118,u11c,u120,u124,
b128,b129,b12a,b12b,blob(50,c4)`.

Order queue (`00456e00` load): `Robject(80c),Robject(808),count,b810`, then
count `(tag,value)` pairs. Tag zero makes value a unit reference, other tags
leave it a scalar. The member array occupies up to `0x800` bytes in memory;
the reader bounds its serialized count independently.

UI: two selection levels, each `count,u28`, then count
`(Robject(c),b14,u10)` records; then panel `Robject(4c),u50`; then build panel
`Robject(4c),u11d8,blob(50,1188)`. Empty UI occupies 4,520 bytes.
Daylight contributes the final 8 bytes.

## Reader contract and limitations

`parse_savegame` publishes a result only after consuming the whole input.
`SaveGame.bytes` owns the input; each section, record and field has a checked
byte range. Scalar fields retain wire bits and memory offsets with a nested
scope for animations and embedded objects. Opaque blocks, including unused
original memory, are retained without guessing meanings or copying host
structures over the file. `field(offset,scope)` finds scalar members; repeated
member-list entries can be inspected in `fields` order.

Failures clear output and report a byte offset. The reader rejects short data,
unknown kinds/types, impossible counts, overflow-sized matrices/fog, files over
256 MiB, more than 16,384 entries per counted list and more than 1,048,576 scalar
or block fields. Arrays have a 16,777,216-element budget and are checked against
remaining bytes before multiplication. Some valid very large original states
may exceed these deliberate resource limits. It does not validate referenced
animation resources, descriptor indices, sparse reference existence, campaign
paths, enum ranges or simulation invariants. It does not restore a live engine
world or provide a save writer.

Corrections to earlier notes/Python labels: the second unit prefix word is a
UGI type, not a size; animations are not paths; Camera has `u60,u64`; Side
`+694/+688` and Town `+168/+184/+188` are references, not presence flags;
Tactic kind 3's tail is scalar. Descriptor pointer offsets are class-specific,
so `+68` is not universally the descriptor pointer.

## Reproducing reference validation

Use a private game directory as in [EXPERIMENTS.md](EXPERIMENTS.md). Symlink
`Res`, `Video`, `Music`, `Aztec.exe`; copy mission files from both `Maps` and
`Campaign/Maps` into private `Maps`, copy a profile, and create private
`Saves/<profile-name>`. All oracle output stays under ignored `work/savegame`.
Configure and build in the task worktree:

```
cmake -S . -B build_save -DAZTEC_GEN_DIR=/home/adrox/aztec-re/work/gen
cmake --build build_save -j6
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir build_save --output-on-failure
```

Run with `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RT_NO_VIDEO=1
RT_NO_GRAB=1 RT_RESOLUTION=800x600 RT_SCRIPT=<script> RT_WRITE_TRACE=<trace>
RT_DUMP_DIR=<existing-directory>`, wrapped in `timeout -s KILL 100 unshare -rn`.
When socket creation is prohibited, the additive `RT_ORACLE_UDP=1` debug hook
provides an in-memory single-player UDP loopback; no interface configuration is
needed. It is unsuitable for LAN validation. Normal networking uses real UDP
when this environment variable is absent.

Mission 4 save script after startup:
`wait 6000; click 140 239; wait 800; click 160 543; wait 5000;
click 150 317; wait 1200; click 140 544; wait 6000; click 700 430;
wait 5000; click 626 10; wait 1000; click 250 190; wait 1500;
click 200 158; wait 600; click 184 359; wait 1200; key 30; key 28;
wait 2500; shot; quit` (one command per line).

The local run produced a 150,752-byte save at tick 67: 1 Town, 4 Sides,
50 Tactics, 104 units, 0 weapons, 2 effects, 31 transports and 17 script
objects. The C++ and Python readers agreed on every section boundary and EOF.
`build_save/test_savegame <save-path>...` prints the C++ reader's inventory.
Traced writes fit within parsed field ranges (section count prefixes sit
between records); individual array writes can subdivide a retained raw block.
Write contents were checked against the trace's first 16 bytes. Full semantic recovery
of every opaque block and scalar is not claimed.


A fresh process loaded slot 1 through the top-left Load button at `(150,60)`,
selected row `(180,258)`, accepted `(150,544)`, waited 6,500 ms, then opened the
in-game save menu and saved slot 2 (`y=178`, description `b`). This produced
151,132 bytes at tick 157. Both saves consumed exactly to EOF in the C++ reader;
11,633 and 11,727 traced writes respectively matched input bytes and field
ranges. All 104 units retained the same multiset of `(UGI,max HP)`. Current HP,
positions, object counts and sparse pool IDs can change as the game advances
and reallocates objects after loading, so this is a gameplay load/save round
trip, not a byte-identical serialization round trip. No multiplayer session
was exercised; multi-slot structural support follows the common writer/loader.

Validation used all six CTest targets with dummy SDL drivers, including the
existing game-data checks. The save tests cover every UGI and weapon type,
all Tactic/script dispatch variants, optional harvest/animation state, route
matrices, malformed kinds/counts, truncation and reuse after failure. The fuzz
target passed 94,262 deterministic mutations. The save and fuzz targets also
passed Clang AddressSanitizer and UndefinedBehaviorSanitizer, including both
local game-generated saves. These checks validate parsing and the stated
resource bounds; unresolved gameplay semantics remain listed above.

## Independent engine import

`aztec_engine --game /path/to/private-game --load-original /path/to/single0.sav`
loads an original save directly, bypassing the front-end. Do not also specify a
mission or `--wander`. Use `--no-sim` to inspect the imported snapshot without
advancing it. Normal simulation resumes with the engine's current rules and the
limitations below; this is not full original-game continuation parity.

`src/engine/save_import.{hpp,cpp}` adapts the decoded stream. The file loader
strips Windows or Unix directory components from the saved mission path and
resolves its basename case-insensitively inside the supplied installation's
`Maps` directory. It never follows an absolute path from a save. Supply that
mission's original `.mp`, `.map`, `.dat` and `.rm` files: terrain, passability and
resource deposits are not embedded in the save. Missing missions or malformed
imports fail with a diagnostic and leave the destination world unchanged.
The lower-level adapter requires a world with the matching mission already loaded.

The importer replaces initial mission placements with saved units, maps sparse
original pool IDs to dense engine IDs, and resolves owning Town/Tactic references
and their Side references independently of record order. `World::original_unit_ids`
retains the inverse unit mapping for imported objects. Town `+f0` unit links
are treated as town hall references to restore town positions (an inference
supported by all nine linked hall types in the locally checked save);
recognised hall types restore race and level where unambiguous (type 47 shares
two levels and retains matching mission metadata). Objects restore current and
secondary HP separately, pixel positions and Marine fractional positions.
Simulation initialization obtains maximum HP from the type descriptors, not
the saved secondary field. Zero/nonpositive HP
remains dead through simulation initialization. Side treasury, team colour and
nonzero disabled-type bytes for the 90 UGI types populate engine economy and
build-menu restrictions. Saved fog replaces each side's mission `.vis` layer. A grid matching the map
dimensions is copied directly; a `(width+1)*(height+1)` grid is sampled at each
tile's top-left corner for the engine, with its full border retained in the
decoded snapshot. Other dimensions are rejected. This conversion does not
establish original fog interpolation parity. Simulation tick, daylight clock,
local side and initial camera offset are restored. An explicit `--cam` overrides
the saved view. The mission's resource grid remains loaded from `.rm`.

Unknown fields are retained in memory in `World::original_save`, including
Side per-type arrays and Town BuildTree research/production blocks. **Research
and tech progression are not interpreted or executed**: only the decoded type
restrictions affect build menus. Current research levels, production queues,
construction progress, harvest state, spending statistics, diplomacy, outcomes,
orders/targets, pathfinding state, facing/playback, RNG, transports,
projectiles, effects, decorations, selection and active triggers/timers are not
restored to their engine systems. Ungrouped objects have side -1 because their
ownership is unresolved. Tactic scenario identity is not inferred from an
unresolved scalar. Mission scripts are not started afresh on import, avoiding
repeated startup grants/spawns, but saved script execution is not resumed.
Engine fog updates and other subsequent simulation use independent engine rules.
Mission metadata supplies fields without a verified saved equivalent; imported
town speed defaults to 1. The retained original snapshot does not track later
engine changes and is not a save writer. Ctrl+S reports that saving an imported
world as an engine command replay is unsupported.

Synthetic `save_import` CTest coverage builds original wire-format saves in
memory, decodes them with the unchanged core reader, and checks ownership,
positions, HP, economy, restrictions, fog, resource preservation, clock continuity,
simulation initialization, invalid references and atomic failure. Optional
`save_import_private` scans `GAME_DIR/Saves` for `.sav` files and checks loading
and initialization against the supplied mission assets. It returns skip code 77
when the installation or original saves are absent. To run it explicitly:

```sh
GAME_DIR=/path/to/private-game ctest --test-dir build -R save_import --output-on-failure
```

These checks establish the supported field mapping, not equivalent gameplay
continuation. Running either game still requires the private-copy, dummy SDL
and `unshare -rn` procedure in [EXPERIMENTS.md](EXPERIMENTS.md).

Local validation on 2026-10-27 imported the available private save containing
259 units and 9 towns at tick 116. Per-unit type, HP, position and owning side,
Side treasury and adapted fog bytes matched the core decoder. The 90-by-90 map
had 91-by-91 saved fog layers. Headless CLI runs with both dummy SDL drivers,
inside `unshare -rn` against a private game copy, advanced one tick and wrote
screenshots with markers and with original sprites, exiting successfully.
All synthetic CTest checks and pre-commit gates passed; the optional private-save
check also passed when supplied that installation. This did not compare resumed
simulation against a reference-runtime replay, and makes no parity claim.

## Resume follow-up: evidence and policy

Status (2026-11-05): **partial; needs live run**. This checkout does not contain
verified translations from original command tags, BuildTree/per-type entries,
diplomacy members or timer units into the engine's state. The V02 reference
identity/load-boundary observer is also incomplete. The follow-up implements
independent validation and observation tooling; it does **not** complete
original-save simulation resume. Do not infer engine orders from unrelated
animation fields, research from disabled-type bytes, or timer values from the
saved simulation tick.

| Domain | Synthetic evidence in this follow-up | Remaining semantic dependency |
|---|---|---|
| Orders and queues | Sparse forward order links; tag-zero unit references; scalar tags kept intact; 256-entry capacity and dangling/duplicate IDs rejected | Command tags, active/pending order translation, production time/payment/placement state |
| Research and upgrades | Nonzero Side per-type and Town BuildTree bytes retained through import and simulation initialization; blocks now have stable names | Per-entry meanings, current research, completed upgrades and effects on engine rules |
| Scripts and timers | Trigger condition/action buffers, enabled-state wire bytes, both timer forms and path points retained; scenario/state fields named | Packet translation, enable semantics, timer units/sample cursor and ScriptRunner restoration |
| Diplomacy | Typed side links checked; unresolved Side state retained | Member-to-relation mapping; mission defaults are still used by simulation |
| Fog | Per-side saved tile samples restored and observed after initialization; border adaptation retains the full original grid | Original interpolation and subsequent fog-update parity |
| Town identity | Saved hall links restore `Object::town_hall` before initialization | Ambiguous hall levels/build-tree completion still need evidence |

`validate_savegame_references` is a separate graph check: the structural parser
still accepts unresolved IDs for inspection. Import requires every nonnull typed
reference to resolve in the correct pool, including orders, script harvest
links, route members and effects. It builds all pools before checking forward
links, rejects duplicate/null identities, and reports the offending byte. It
does not interpret untyped words inside raw packets. Import failure leaves the
active world unchanged. This strict policy can reject a save whose stale links
the original loader might clear; no silent repair is attempted.

Interoperability is import only. The original snapshot is immutable provenance,
not a second simulation: it does not change when the engine advances. Export to
original `.sav`, conversion to AZS3 command replay and resumed multiplayer are
out of scope. Unknown state remains available to private investigation without
pretending that retention restores engine execution. Synthetic fixture output
is a test generator, not an original-save writer for gameplay.

`aztec_save_probe --game DIR --save FILE --rules DIR --output FILE` loads a save
through the real importer and initializes GameSim, then emits JSON before any
continued tick. Its numeric `state` contains original-ID-keyed units (type,
zero-based side, original tagged town ID or zero, current/secondary HP and
positions in pixel*256), side-indexed money, tile-sampled fog and daylight clock.
It reports the saved absolute tick separately from elapsed tick zero. Raw
packets, padding, paths, descriptions and resource contents are excluded.
Unsupported orders, queues, research, upgrades, scripts, timers and diplomacy
are listed explicitly. Those fields are never replaced with mission defaults
in the comparison stream. The observer refuses an already advanced world.

`tools/savegame/compare_resume.py` adds the input SHA-256, validates both complete
JSON documents, checks phase/ticks/save identity and compares common supported
domains exactly. Missing domains produce `incomplete`, never a match; malformed
observations fail and namespace/observer failures produce `needs live run`.
Schema 1 defines layouts only for units, money, fog and clock, and rejects claims
of extended coverage using arbitrary numeric placeholders. It therefore cannot
produce a complete tick-zero match or exit zero. Extending the schema requires
audited layouts/mappings for the remaining domains; a future fully covered
tick-zero match would still not prove continued simulation parity.
Reference adapters must observe the loaded runtime, not
reparse the input save as a substitute for reference state. See the run sheet
for the adapter boundary and missing prerequisite.

`save_resume_controls` generates invented `.sav` bytes via
`test_save_import --generate SAVE SNAPSHOT`, loads them through the standalone
probe with invented mission/archive files, and tests schema, identity,
phase, mismatch, missing coverage, private copies, input mutation and namespace
failure controls. `save_import` also checks retained nondefault continuation
fields and atomic graph-validation failures. No real saves or resources are
fixtures. These checks provide synthetic structural evidence only; no new live
reference result was obtained for this follow-up.

Local verification on 2026-11-05: the independent Release build succeeded; all
41 runnable checks out of 47 CTest entries passed, with six skips for private
inputs/restricted networking. The final save-focused regressions and pre-commit
gates passed, including explicit checks of the new untracked source files.
No live reference process was launched. Status remains **needs live run**;
the semantic mappings and audited reference observer remain open.

## V16 continuation controls (2026-11-06)

**S / needs live run:** task 94 builds on the present task-72 structural importer.
Task 83 is still pending repeated reference confirmation; no new engine behavior
or semantic importer mapping is justified by this work. No reference process or
private game-data process was run. Retained wire state is not restored execution.

`aztec_save_probe ... --ticks N` selects schema 2 for N in 0..3600. It initializes
once, advances exactly N simulation updates without injected player commands,
and observes before the next update/command processing. At zero the phase is
`after_load_before_tick`; otherwise it is `after_tick_before_commands`.
`saved_tick` remains the saved absolute tick; `elapsed_ticks` is the number of
continued updates. The supported numeric layouts remain units, money, fog and
clock. Camera, frontend and progression join the explicitly unsupported domains.
The graphical loader already uses saved view coordinates, but this world-only
observer cannot validate its final camera, zoom or front-end transitions.

Schema 2 is a diagnostic of the current partial importer: restored orders,
production, research and scripts are not executed. Differences after N updates
can reveal consequences of those omissions; they cannot establish their cause
without repeated original observations. New objects or reordered engine IDs
cause observation refusal because there is no audited identity mapping for
births. Imported dead objects remain in the unit stream as in schema 1. Extending
the stream for live/dead membership, births and new field classes needs a new
audited layout, not omission of unmatched objects or numeric placeholders.

The comparison runner now launches two independent reference processes and two
independent clone processes, each with a private installation and private save
copy. Both repeats must agree before comparing programs. Different repeated
state produces `reference_nondeterminism` or `clone_nondeterminism`; differing
boundaries or ignored `--ticks` are failures. Offline `compare` accepts
`--repeat-reference` and `--repeat-clone`; without both it reports `repeated=false`
on an incomplete result. Neither schema can produce a complete match or exit
zero. Existing schema-1 adapters still work without `--ticks`; schema-2 adapters
must implement the exact continued-update barrier, not approximate it with waits.

`campaign_save_processes` generates three distinct invented mission files named
mission_1 through mission_3. Each has a completed trigger before the save and a
victory trigger afterward, plus commands on both sides of the save boundary.
Fresh-process headless continuation compares every tick from the saved boundary
through victory, terminal hashes, command counts and script observations.
Separate graphical processes save at tick 5 with nondefault camera/zoom, load
through the mission shortcut, and reach victory; front-end slot selection and
confirmation resolve the correct saved mission in another process. The test is
registered on all platforms, including Windows, but native Windows execution
remains pending. Invented mission numbers are not evidence for actual campaign
missions 1-3. The test also confirms automation does not write campaign progress;
normal-session victory/unlock/profile persistence still requires live acceptance.

| Field class / acceptance | Evidence from this task | Still required |
| --- | --- | --- |
| Units, money, fog, clock at 0/N | S: generated saves, bounded observation and repeated comparison controls | M: real original saves, two reference runs per boundary |
| Orders and queues | Existing structural retention only | Command, payment, remaining-time and placement mappings |
| Research/upgrades | Existing nondefault byte retention only | Active/completed semantics and rule effects |
| Script/timer/diplomacy | Existing graph validation/retention only | Restored execution and repeated original measurements |
| Camera and front-end | S: AZS3 graphical save/load and slot confirmation | M: original view normalization and real campaign UI |
| Progression | S: automation does not award progress | M: normal fresh-session finish, unlock, retry and profile persistence |
| Missions 1-3 / platforms | S: three generated mission continuations on Linux | Real mission completion; native Windows checks; task 83 evidence |

The exact external commands and return-data requirements are in
[EXPERIMENTS.md](EXPERIMENTS.md#v16-original-save-continuation-and-missions-1-3-needs-live-run).
Status: **needs live run**.

Validation for this iteration: Release build passed; 48 of 54 CTest entries
passed with six expected private-input/network skips. Pre-commit checks also
passed, including explicit checks for the new untracked test. No native Windows
or actual original-save/campaign result is claimed.
