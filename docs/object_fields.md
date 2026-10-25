# Serialized object members

This is a field map for the original save format, not a proposed C++ object ABI.
Offsets are hexadecimal and relative to the object named in the first column.
All scalar members occupy four bytes **on disk**, even byte and short members.
Wire order, conditional branches, counted lists and exact raw block lengths are
in [savegame.md](savegame.md). Together the two documents enumerate the stream;
unlisted memory members (vtables, allocator links, reconstructed pointers and
caches) are not serialized.

“Unresolved scalar/flag” means its storage and wire representation are known,
but its gameplay meaning has not been established. A reference family or a
static descriptor is a verified structural meaning; a target's gameplay role
is not inferred merely from a plausible offset. This distinction matters when
rebuilding simulation state. Raw arrays may include unused bytes and cannot
safely be treated as packed host structs.

## Common visible and world members

World classes inherit both tables; Decoration and weapon classes inherit only
Visible. Save/load methods establish these offsets independently of C++ layout.

| Class | Field | Memory offset | Meaning |
|---|---|---|---|
| Visible | scalar members | `+10,+14,+18,+1c` | Unresolved visible-object state words |
| Visible | flags | `+20,+2c` | Boolean members, stored as words |
| Visible | position x/y | `+24,+28` | World pixel coordinates |
| World | owning group | `+30` | Tagged Tactic/Town reference; **not an animation pointer** |
| World | scalar | `+34` | Unresolved scalar |
| World | health | `+38` | Current HP; 6,000 for a full-health footman |
| World | maximum health | `+3c` | Maximum HP |
| World | scalar | `+40` | Unresolved scalar |
| World | tile x/y | `+44,+48` | Tile coordinates; nominal tile width 48 pixels |
| World | short member | `+4c` | Signed 16-bit state member, widened to a word |
| World | UGI | `+50` | Object type ID, 0..89; also present in the outer unit prefix |
| World | scalars | `+54,+58` | Unresolved scalar members |
| World | flags | `+5c,+5d,+5e,+5f` | Four distinct boolean members |

`+4,+8,+c` in Visible are reconstructed spatial list links. They are not part
of the Visible stream. Type-specific animations, descriptors and fixed-point
movement state occur in different places in different classes.

## World-class extensions

The offsets in each row are individual fields, listed in wire order within
that category. No arithmetic progression or implicit struct padding is implied.
The full interleaving of categories is specified in savegame.md.

| Class | Field | Memory offset | Meaning |
|---|---|---|---|
| Marine | occupancy | `+60` | Boolean used when restoring map occupancy |
| Marine | scalar | `+64` | Unresolved scalar |
| Marine | precise x/y | `+6c,+70` | Fixed-point pixel position, scale 256 |
| Marine | movement scalars | `+74,+78` | Unresolved movement state |
| Marine | playback scalars | `+80,+84,+88` | Direction/state and animation counters; individual enum meanings unresolved |
| Marine | unit links | `+8c,+90,+94` | Three unit references; gameplay roles unresolved |
| Marine | scalars | `+98,+9c,+a4` | Unresolved scalar members |
| Marine | effect link | `+a0` | Reference to pool `004c1298` |
| Marine | descriptor | `+68` | Class type descriptor index, stride `0x4c`, load base `004ac430` |
| Marine | animation | `+7c` | Pointer reconstructed from nested Animation state |
| Spider | occupancy | `+60` | Map occupancy boolean |
| Spider | scalars | `+64,+68` | Unresolved state words |
| Spider | coordinate pairs | `+78/+7c,+80/+84` | Two serialized coordinate pairs; units/roles unresolved |
| Spider | scalars | `+88,+8c,+94` | Unresolved scalar members |
| Spider | flags | `+90,+91,+92` | Three byte/boolean state members |
| Spider | unit links | `+98,+9c,+a0` | Unit references |
| Spider | descriptor | `+6c` | Class descriptor index, stride `0x4c`, base `004ae378` |
| Spider | animations | `+70,+74` | Two nested animation states |
| Building | occupancy / flag | `+60,+8c` | Occupancy boolean and unresolved boolean |
| Building | scalars | `+68,+78,+7c` | Unresolved building state |
| Building | linked effects | `+80,+84,+88` | Decoration, effect and transport references respectively |
| Building | descriptor | `+64` | Class descriptor index, stride `0x508`, base `0049b538` |
| Building | animation | `+6c` | Nested animation state |
| Building | optional harvest | `+70,+74` | Presence words followed by Harvest subobjects |
| Zeppelin | scalars | `+60,+68,+6c,+70,+74,+78,+7c,+80,+84` | Unresolved flight/movement state words |
| Zeppelin | flag | `+88` | Boolean state member |
| Zeppelin | unit link | `+90` | Unit reference |
| Zeppelin | descriptor | `+64` | Class descriptor index, stride `0x44`, base `004bcd00` |
| Zeppelin | animation | `+8c` | Nested animation state |
| Truck | occupancy | `+60` | Map occupancy boolean |
| Truck | scalars | `+64,+6c,+70,+74,+78,+7c,+80,+84,+98` | Unresolved movement/combat state |
| Truck | unit links | `+88,+8c` | Unit references |
| Truck | flags | `+90,+91` | Byte/boolean state members |
| Truck | descriptor | `+68` | Class descriptor index, stride `0x44`, base `004bc738` |
| Truck | animation | `+94` | Nested animation state |
| Bird | scalars | `+60,+68,+6c,+70,+74,+78,+7c,+80,+84,+8c,+90` | Unresolved flight/movement state |
| Bird | flag | `+88` | Boolean state member |
| Bird | unit link | `+94` | Unit reference |
| Bird | descriptor | `+64` | Class descriptor index, stride `0x58`, base `0049b370` |
| Bird | animation | `+98` | Nested animation state |
| Headtruck | occupancy | `+60` | Map occupancy boolean |
| Headtruck | scalars | `+64,+68,+70,+74,+78,+7c,+80,+84,+88,+8c,+a4,+a8,+ac` | Unresolved movement/combat state |
| Headtruck | unit links | `+90,+94` | Unit references |
| Headtruck | flag | `+98` | Byte/boolean state member |
| Headtruck | descriptor | `+6c` | Class descriptor index, stride `0x50`, base `004ab2b0` |
| Headtruck | animations | `+9c,+a0` | Two nested animation states |
| Defence | occupancy / flag | `+60,+9c` | Map occupancy and unresolved boolean |
| Defence | scalars | `+64,+68,+80,+84,+88,+8c` | Unresolved defence state |
| Defence | unit links | `+78,+7c` | Unit references |
| Defence | linked effects | `+90,+94,+98` | Decoration, effect and transport references respectively |
| Defence | descriptor | `+6c` | Class descriptor index, stride `0x110`, base `004aa3b0` |
| Defence | animations | `+70,+74` | Required first animation, presence-prefixed second animation |
| Boat | scalars | `+60,+68` | Unresolved boat state |
| Boat | animation | `+64` | Nested animation state |
| Cannon | occupancy | `+60` | Map occupancy boolean |
| Cannon | scalars | `+64,+84` | Unresolved combat state |
| Cannon | coordinate pairs | `+70/+74,+78/+7c` | Two coordinate pairs, units/roles unresolved |
| Cannon | flags | `+80,+81` | Byte/boolean members |
| Cannon | unit links | `+88,+8c,+90` | Unit references |
| Cannon | descriptor | `+68` | Class descriptor index, stride `0x40`, base `004a8e18` |
| Cannon | animation | `+6c` | Nested animation state |
| Johnka | scalars | `+60,+68,+70` | Unresolved state words |
| Johnka | unit link | `+6c` | Unit reference |
| Johnka | animation | `+64` | Nested animation state |
| Camera | scalars | `+60,+64` | Two Camera-specific state words |

## Animation and small subobjects

| Class | Field | Memory offset | Meaning |
|---|---|---|---|
| Animation | resource ID | `+0` | `Units.xbe` group `0x515` element number |
| Animation | frame pointers | `+8,+10` | Sequence header/current frame indices on disk; pointers reconstructed as resource base + `0x208 + index*0x20` |
| Animation | resource pointer | `+4` | Reconstructed, not serialized |
| Animation | flags | `+c,+18` | Loop flag and frame-changed flag respectively |
| Animation | counter | `+14` | Tick within the current frame (incremented until frame duration) |
| Harvest | script link | `+4` | Reference to pool `004cd698` |
| Harvest | scalars | `+8,+c` | Two unresolved words |
| Decoration | scalar | `+34` | Unresolved state word |
| Decoration | animation | `+30` | Nested Animation pointer |
| Effect (`004c1298`) | scalars | `+4,+8,+c,+10,+14,+18,+1c` | Seven state words |
| Effect (`004c1298`) | floating state | `+20` | In-memory float saved as an eight-byte double |

The Animation presence token describes whether `+8` is null; it is not a
standalone in-memory boolean. A null animation pointer and an animation with
no active frames are different states.

## Tactic and Town fields

| Class | Field | Memory offset | Meaning |
|---|---|---|---|
| Tactic | scalar | `+4` | Unresolved identity/state word |
| Tactic | owner | `+8` | Side reference |
| Tactic | members | `+c` | Count-prefixed member list, 11 words per member |
| Tactic | scalar members | `+10,+14,+1c,+20,+24,+2c,+30,+34,+40,+44,+48,+4c,+50,+54,+64,+68,+70,+84,+88,+ac,+b4,+bc,+c0,+ec,+90` | Unresolved AI state words, in fixed-prefix wire order |
| Tactic | short members | `+28,+c4,+c6` | Signed 16-bit state members |
| Tactic | flags | `+60,+61,+6c,+6d,+8c,+94,+9c,+9d,+b0,+b8,+d8,+d9,+e8,+e9,+ea,+18` | Boolean AI state; `+60` controls the first target's reference family |
| Tactic | point array/count | `+38,+3c` | Count triples of words, exact gameplay meaning unresolved |
| Tactic | unit links | `+58,+80` | Unit references |
| Tactic | target list | `+5c` | Unit/object target reference list |
| Tactic | route matrix | `+74` | Optional route/assignment state, with serialized dimensions |
| Tactic | columns/rows | `+78,+7c` | Route matrix counts |
| Tactic | object link | `+98` | Tagged Tactic/Town reference |
| Tactic | pair lists | `+c8,+dc` | Count-prefixed pairs of state words |
| Tactic | object list | `+a0` | Count-prefixed tagged object references |
| Tactic member | unit | `+0` | Unit reference |
| Tactic member | scalars | `+4,+8,+c,+10,+16,+1c,+20,+24` | Per-unit AI state words; `+16` is deliberately unaligned |
| Tactic member | flags | `+14,+15` | Per-member booleans |
| Tactic kind 2 | scalars | `+f0,+f8` | Variant state words |
| Tactic kind 2 | links | `+f4,+fc` | Object and Side references |
| Tactic kind 3 | scalar | `+f0` | Variant state word |
| Tactic kind 4 | flags | `+f0,+f1` | Variant booleans |
| Town | unit links | `+f0,+168` | Unit references |
| Town | scalar members | `+158,+15c,+160,+164,+174,+17c,+180,+18c,+190,+194,+198,+1b0,+1b4,+178` | Unresolved settlement/economy state words |
| Town | flags | `+170,+171` | Boolean settlement state |
| Town | object link | `+16c` | Tagged object reference |
| Town | raw state | `+19c` | 20-byte state block |
| Town | queue | embedded at `+f8` | Ring of unit references |
| Town | harvest state | embedded at `+110` | Three scalars, boolean, unit reference |
| Town | unit list | embedded at `+128` | Two scalars, count, unit references |
| Town | links | `+184,+188` | Object reference and order-queue reference |
| Town | build tree | pointer at `+f4` | Nested BuildTree state |
| Town queue | count/capacity/cursor/head | `+14,+10,+c,+8` | Ring state words in wire order, followed by unit references |
| Town harvest state | scalars / flag / link | `+8,+c,+10 / +14 / +4` | Three unresolved words, boolean, unit reference |
| Town unit list | scalars/count | `+28,+2c,+18` | Two state words, list count; unit references follow |
| BuildTree | raw state arrays | `+16c,+4,+2e8` | Blocks of `0x16c,0x168,0x1188` bytes respectively; per-entry meanings unresolved |
| BuildTree | descriptor | `+1470` | Byte displacement relative to `004af130` |
| BuildTree | flags/scalar | `+2e0,+2e1,+2e2 / +2e4` | Three booleans and a state word |

Two additional tokens belong to globals rather than any object's storage:
Tactic's leading `004d9b78` state word and Town's `00515721` boolean.

## Side and fog

| Class | Field | Memory offset | Meaning |
|---|---|---|---|
| Side | scalar members | `+4,+c,+10,+14,+13c,+63c,+644,+648,+64c,+650` | Unresolved identity, relations or statistics words; individual meanings not established |
| Side | team colour | `+8` | Team palette colour selector, also used by the renderer |
| Side | flag | `+138` | Boolean side state |
| Side | outcome | `+140` | Victory/outcome state |
| Side | treasury | `+698` | Current gold |
| Side | spending | `+640` | Cumulative spending |
| Side | current orders | `+694` | Order-queue reference |
| Side | base object | `+688` | Tagged Tactic/Town reference |
| Side | name | `+18` | Fixed 32-byte name buffer |
| Side | raw state | `+38,+144,+30c,+478` | Blocks of `0x100,0x16c,0x16c,0x168` bytes respectively; contents unresolved |
| Side | disabled types | `+2b0` | `0x5b` bytes of type restrictions/state |
| Side | per-type state | `+5e0` | `0x5a` raw bytes; individual flags unresolved |
| Side | object lists | `+654,+660` | Two lists of tagged Tactic/Town references |
| Side | unit list | `+678` | Unit reference list |
| Side | order list | `+66c` | Order reference list |
| Side | explorer | `+68c` | Optional map-exploration list: object link and `0x168` bytes per entry |
| Side | fog map | `+690` | Pointer to map; serialized as width, height, width*height bytes |
| Side | cameras | `+684` | Count and raw 20-byte camera records; component meanings unresolved |
| Fog map | width/height/data | `+4,+8,+14` | Grid dimensions and byte data pointer; pointer itself is not serialized |

## Projectiles and other pools

Every projectile inherits Visible. All weapon classes also serialize `+30`
(owner unit reference) and `+34` (unresolved scalar). The following table lists
all additional serialized members, grouped by storage meaning. Descriptor
indices and animations replace pointers; no process addresses are restored.

| Class / kind | Field | Memory offset | Meaning |
|---|---|---|---|
| BallExpl / 0 | scalars | `+38,+44,+48,+4c,+50,+54,+58,+5c,+60,+64` | Unresolved projectile state words |
| BallExpl / 0 | descriptor/animation | `+3c / +40` | Descriptor index and Animation |
| Arrow / 1 | scalars | `+38,+40,+44,+48,+4c,+50,+54,+58,+5c,+60` | Unresolved projectile state |
| Arrow / 1 | target link | `+64` | Unit reference |
| Arrow / 1 | descriptor/animation | `+3c / +68` | Descriptor index and Animation |
| Axe / 3 | scalars | `+38,+40,+44,+48,+4c,+50,+54,+58` | Unresolved projectile state |
| Axe / 3 | descriptor/animation | `+3c / +5c` | Descriptor index and Animation |
| Weapon kinds 2,9,12,13 | descriptor | `+38` | Kind-specific descriptor index |
| Weapon kinds 4,5,6,10 | extension | none | No additional serialized members |
| SplashDamage / 7 | scalars | `+3c,+44` | Unresolved area-effect state |
| SplashDamage / 7 | descriptor/animation | `+38 / +40` | Descriptor index and Animation |
| SplashDamage / 8 | scalars | `+38,+40,+104` | Unresolved area-effect state |
| SplashDamage / 8 | descriptor | `+3c` | Descriptor index |
| SplashDamage / 8 | entries | `+44` | Sixteen triples of words (192 bytes), per-entry meanings unresolved |
| FireThrow / 11 | scalar/animation | `+3c / +38` | Unresolved state word and Animation |
| Weapon kind 14 | link/scalars | `+38 / +3c,+40,+44` | Unit reference and three state words |
| Transport | link | `+30` | Unit reference |
| Transport | scalars | `+34,+38,+3c,+40,+44,+48,+4c,+118,+11c,+120,+124` | Unresolved transport state |
| Transport | flags | `+114,+128,+129,+12a,+12b` | Byte/boolean members |
| Transport | raw state | `+50` | `0xc4` bytes |
| Order queue | links | `+80c,+808` | Two tagged Tactic/Town references |
| Order queue | count/flag | `+804 / +810` | Number of tag/value entries and byte/boolean flag |
| Order entry | tag/value | `+0,+4` relative to entry | Tag zero: value is a unit reference; otherwise a scalar |

## Scenario objects and UI

| Class | Field | Memory offset | Meaning |
|---|---|---|---|
| All script kinds | scenario ID | `+4` | Identifier used by scenario scripts |
| Script kind 0 | scalar | `+8` | Unresolved state word |
| Trigger, Timer, kind 3 | scalar/flag | `+8 / +c` | Scenario state word and boolean |
| Trigger | packets | dynamic lists | Condition count and `0x408`-byte packets, then action count and packets; opcodes/arguments are preserved raw |
| Timer | mode | `+14` | Selects sample-array form when nonzero |
| Timer | state | `+18,+1c` | Two scalar words in the non-array form; precise timer units unresolved |
| Timer | sample count/state | `+20,+1c` | Count and state word in array form, then count samples |
| Scripted path / kind 4 | points | list at `+8` | Count-prefixed x/y pairs of words |
| UI selection level | count/state | count / `+28` | Count and state word, followed by entries |
| UI selection entry | object/flag/scalar | `+c / +14 / +10` | Tagged object reference, boolean, unresolved word |
| UI panel | link/state | `+4c,+50` | Tagged object reference and word |
| UI build panel | link/state | `+4c,+11d8` | Tagged object reference and word |
| UI build panel | raw build state | `+50` | `0x1188` bytes |

The final daylight tokens are globals `004c1968` (day time) and `004c196c`
(stage), not object fields. The camera view and five game globals precede the
object sections. Per-Side camera records and UGI_CAMERA world objects are two
different serialized structures.
