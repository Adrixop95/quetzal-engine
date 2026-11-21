# Object descriptor fields

Evidence date: 2026-11-21. **V** means a field read or structural layout was
verified in reference code; **G** means its interpretation is inferred.
**unused** means no runtime reader was found in the audited consumers, including
the kind's virtual accessors. An unused designation is scoped to this survey,
not a claim that an arbitrary modified descriptor could never be consulted.

The CSV sources are [descriptor_kinds.csv](data/descriptor_kinds.csv) and
[descriptor_fields.csv](data/descriptor_fields.csv). Each of the 520 words has
one CSV row, a unique name, status, meaning and evidence addresses. They generate
`descriptor_layouts` and the per-type `fields` in [rules.json](data/rules.json)
through [build_rules.py](../tools/build_rules.py). `fields` includes every column
that is nonzero in any record of that kind, including its zero-valued instances.
All-zero columns remain described in the shared layout. Aliases retain established
JSON names, including `hp_max` and Marine `armor_d14`.

All **201** columns carrying values other than zero or the constant -1 sentinel
are named: **200 V and 1 G (99.50% V)**. All **24** constant -1 columns are named
and marked unused. Thus all **225 nonzero columns (100%)** have a name and status.
These are column counts across the nine kinds, not percentages of gameplay parity,
not percentages of individual type records, and not proof that every meaning is
fully understood. The historical audit's 497 total words was an arithmetic error;
the nine record widths total 520.

## Method and evidence limits

The object factory `0x43d5d0` selects the constructors below. In particular, Bird
case 5 calls `0x407360`, Building case 2 calls `0x40fb70`, Zeppelin case 3 calls
`0x471390`, and Truck case 4 calls `0x460090`. Bird uses **object +0x64**, correcting
the earlier +0x6c example. Truck and Zeppelin have different 17-word tables.

The private survey starts with the 5,468 functions in `all_full.c`, follows
object descriptor loads in both byte-offset and dword-index notation, then reads
the containing methods and their local pointer aliases. Vtable entries identify
shared accessors outside a kind's normal address range. Supplemental cached
exports and 23 newly requested Ghidra exports cover missing methods. The recipe
is `DecompAddrs.java` as described in [weapon_class_map.md](weapon_class_map.md),
using a private copy of the Ghidra project. No original executable was run.
Original instructions at `0x411330` and `0x41e220` confirm the top rectangle
coordinate that Ghidra loses to an untracked register.

The extended per-offset scan, including Bird, Building and Zeppelin, is retained
privately at `work/descriptor/descriptor_uses_extended.json`. The public CSV
records function addresses only, including extra mid-block exports of the same
method; the tables below use those same evidence lists. Indexed array consumers
apply to every footprint cell or enabled schedule entry, not just to the base
address a text search happens to match. Ghidra fragments are corroborating
exports, not separate gameplay operations. The survey does not reproduce code,
assets, localized strings, executable bytes or generated sources.

A V label for an accessor verifies what it returns. It does not invent a gameplay
interpretation for a channel with no identified consumer. `armor_auxiliary`
(virtual +0x110) is the main example: Marine d14 is demonstrably read by
`0x433010`, but no damage consumer was identified. It is neither unread nor a
proven third resistance. For fire armor (+0x108), burn `0x429f60` and explosion
`0x44c130` consume a factor with 256 as unity. Physical armor (+0x10c) enters
weapon-specific additive terms; it is not a universal damage reduction factor.

## Corrections to previous labels

- Catapult primary HP is **d1 = 10000** (`0x460090`, `0x45fe50`), secondary HP is
  **d4 = 3000**. The previous guessed `hp_initial` at d4 was wrong.
- Headtruck and Spider maximum HP is **d3**, while d6 initializes secondary HP.
  `hp_max` now aliases d3; equal numbers in existing records hid that distinction.
- Headtruck/Spider d9 and Truck/Cannon d7 are verified weapon IDs through shared
  getters `0x42c3c0` and `0x4166a0`. Their fire and physical armor accessors are
  verified with the actual vtable slots, rather than inferred from adjacent words.
- Marine d15 is a placement sound-list ID (`0x4334d0`). d16 is the hit sound list,
  d17 the death sound list. The last word of **every** kind is a **name-string
  pointer**, returned by the name getter; none is a code pointer.
- Building d6..d54 and Defence d9..d57 are **7x7 footprint masks**. The Building
  d65..d319 array is a **gold payout schedule**. Production lists, prerequisites,
  prices and build times are separate tables, not descriptor words.

## Table conventions

Offsets are bytes from the descriptor, not from the object. Grid names use
zero-based row/column coordinates; the center is (3,3). Only mask value 2 requires
and occupies a tile in the inspected placement/occupancy/terrain consumers. Values
0 and 1 are not distinguished there; no margin behavior is inferred for 1.

Schedule names use a three-digit zero-based index. Entries 0..9 are listed
individually; the all-zero capacity tail is grouped below. Every individual name
and index, including that tail, is available in the CSV and JSON layout. A V
schedule label verifies the indexed array layout, not execution of all 255 slots.
Regeneration caps select the alternate cap when object +0x54 is zero and primary
maximum HP otherwise; the addition is conditional on being below the cap and may
cross it. Sound-list ID zero suppresses playback.

## Marine

Table `0x4ac430`, 19 words; object pointer at `+0x68`; vtable `0x4932cc`.
Constructors: `0x433190`, `0x432d50`, `0x4332e0`.
15 non-sentinel data columns; 18 nonzero columns including unused sentinels.

| Word | Offset | Name | Status | Meaning | Reading functions / evidence |
| --- | --- | --- | --- | --- | --- |
| 0 | +0x00 | `anim_id` | V | Body animation resource ID passed to the animation loader. | `0x433190`, `0x4332e0` |
| 1 | +0x04 | `hp_initial` (aliases: `hp_max`) | V | Initial primary HP at object +0x38; also the virtual +0x3c maximum. | `0x433190`, `0x433db0`, `0x45fe50` |
| 2 | +0x08 | `regen_per_tick` | V | Primary HP added each active tick while below the selected regeneration cap; the addition can cross the cap. | `0x433db0` |
| 3 | +0x0c | `regen_cap_alt` | V | Regeneration cap when object +0x54 is zero; otherwise regeneration uses primary maximum HP. | `0x433db0` |
| 4 | +0x10 | `secondary_hp_initial` | V | Initial secondary HP at object +0x3c; distinct from the virtual maximum-HP getter. | `0x433190` |
| 5 | +0x14 | `speed_px_per_tick` | V | Maximum movement speed in pixels per logic tick. | `0x433730`, `0x433820`, `0x433886`, `0x433db0` |
| 6 | +0x18 | `sight_mask_id` | V | Light-mask resource ID; not a radius in pixels. | `0x432f80` |
| 7 | +0x1c | `weapon_id` | V | Index into the weapon prototype table 0x515ac0; -1 means unarmed. Getter also rejects nonpositive damage for the target. | `0x4166a0` |
| 8 | +0x20 | `unused_008` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 9 | +0x24 | `unused_009` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 10 | +0x28 | `unused_010` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 11 | +0x2c | `flags_bytes` | V | Packed bytes: +0x2c virtual +0xa0 predicate; +0x2d category +0xe8 (and complementary +0xf4), Soldier/Animal name; +0x2e Hypno +0xf8 predicate. Byte 3 has no identified reader. | `0x432fa0`, `0x432fe0`, `0x432ff0`, `0x433000`, `0x4337e0` |
| 12 | +0x30 | `armor_fire` | V | Fire and area-explosion damage factor returned by virtual +0x108; 256 is unity. Consumers: 0x429f60 and 0x44c130. | `0x45ff10` |
| 13 | +0x34 | `armor_physical` | V | Physical armor value returned by virtual +0x10c; used by armor-dependent damage terms, not a universal damage multiplier. | `0x45ff20` |
| 14 | +0x38 | `armor_auxiliary` (aliases: `armor_d14`) | V | Value returned by virtual +0x110. No damage consumer found in the audited corpus; gameplay interpretation remains open. | `0x433010` |
| 15 | +0x3c | `spawn_sound_list_id` | V | Sound-list ID: a random entry is played on placement; zero disables the list. | `0x4334d0` |
| 16 | +0x40 | `hit_sound_list_id` | V | Sound-list ID played after damage; zero in every Marine record. | `0x4336b0`, `0x4336c6` |
| 17 | +0x44 | `death_sound_list_id` | V | Sound-list ID played by the death handler; zero disables the list. | `0x433960`, `0x4339a1`, `0x4339b9`, `0x4339ee`, `0x433a03`, `0x433a2a` |
| 18 | +0x48 | `name_string_pointer` | V | Guest pointer to the localized name returned by virtual +0x84 for argument zero; not a code pointer or stable resource ID. | `0x4337e0` |

## Headtruck

Table `0x4ab2b0`, 20 words; object pointer at `+0x6c`; vtable `0x492c38`.
Constructors: `0x42bda0`, `0x42b9c0`, `0x42bf70`.
17 non-sentinel data columns; 20 nonzero columns including unused sentinels.

| Word | Offset | Name | Status | Meaning | Reading functions / evidence |
| --- | --- | --- | --- | --- | --- |
| 0 | +0x00 | `anim_id` | V | Body animation resource ID passed to the animation loader. | `0x42bda0`, `0x42bf70` |
| 1 | +0x04 | `turret_variant_index` | V | Selects a group of four entries in body animation metadata (section 0x27 for Headtruck, section 9 for Spider); exact entry semantics remain open. | `0x42c3a0` |
| 2 | +0x08 | `turret_anim_id` | V | Animation resource ID for the independently drawn head/turret. | `0x42bda0`, `0x42bf70` |
| 3 | +0x0c | `hp_initial` (aliases: `hp_max`) | V | Initial primary HP at object +0x38; also the virtual +0x3c maximum. | `0x41cbe0`, `0x42bda0`, `0x42c880`, `0x44a0f0` |
| 4 | +0x10 | `regen_per_tick` | V | Primary HP added each active tick while below the selected regeneration cap; the addition can cross the cap. | `0x42c880` |
| 5 | +0x14 | `regen_cap_alt` | V | Regeneration cap when object +0x54 is zero; otherwise regeneration uses primary maximum HP. | `0x42c880` |
| 6 | +0x18 | `secondary_hp_initial` | V | Initial secondary HP at object +0x3c; distinct from the virtual maximum-HP getter. | `0x42bda0` |
| 7 | +0x1c | `speed_px_per_tick` | V | Maximum movement speed in pixels per logic tick. | `0x42c880` |
| 8 | +0x20 | `sight_mask_id` | V | Light-mask resource ID; not a radius in pixels. | `0x42bc50` |
| 9 | +0x24 | `weapon_id` | V | Index into the weapon prototype table 0x515ac0; -1 means unarmed. Getter also rejects nonpositive damage for the target. | `0x42c3c0` |
| 10 | +0x28 | `unused_010` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 11 | +0x2c | `unused_011` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 12 | +0x30 | `unused_012` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 13 | +0x34 | `turret_turn_step` | V | Turret heading step per tick in 1/256-circle units; also used in the reload-time retarget threshold. | `0x42c880` |
| 14 | +0x38 | `armor_fire` | V | Fire and area-explosion damage factor returned by virtual +0x108; 256 is unity. Consumers: 0x429f60 and 0x44c130. | `0x44a170` |
| 15 | +0x3c | `armor_physical` | V | Physical armor value returned by virtual +0x10c; used by armor-dependent damage terms, not a universal damage multiplier. | `0x44a180` |
| 16 | +0x40 | `armor_auxiliary` | V | Value returned by virtual +0x110. No damage consumer found in the audited corpus; gameplay interpretation remains open. | `0x44a190` |
| 17 | +0x44 | `spawn_sound_list_id` | V | Sound-list ID: a random entry is played on placement; zero disables the list. | `0x42c1d0`, `0x42c2fc` |
| 18 | +0x48 | `engine_sound_list_id` | V | Acceleration sound-list ID; repeated playback has a ten-tick cooldown. | `0x42c880` |
| 19 | +0x4c | `name_string_pointer` | V | Guest pointer to the localized name returned by virtual +0x84 for argument zero; not a code pointer or stable resource ID. | `0x42c400` |

## Spider

Table `0x4ae378`, 19 words; object pointer at `+0x6c`; vtable `0x4940ec`.
Constructors: `0x44a350`, `0x449e70`, `0x44a210`.
16 non-sentinel data columns; 19 nonzero columns including unused sentinels.

| Word | Offset | Name | Status | Meaning | Reading functions / evidence |
| --- | --- | --- | --- | --- | --- |
| 0 | +0x00 | `anim_id` | V | Body animation resource ID passed to the animation loader. | `0x44a210`, `0x44a350` |
| 1 | +0x04 | `turret_variant_index` | V | Selects a group of four entries in body animation metadata (section 0x27 for Headtruck, section 9 for Spider); exact entry semantics remain open. | `0x44ab40` |
| 2 | +0x08 | `turret_anim_id` | V | Animation resource ID for the independently drawn head/turret. | `0x44a210`, `0x44a350` |
| 3 | +0x0c | `hp_initial` (aliases: `hp_max`) | V | Initial primary HP at object +0x38; also the virtual +0x3c maximum. | `0x44a0f0`, `0x44a210`, `0x44a350`, `0x44b360` |
| 4 | +0x10 | `regen_per_tick` | V | Primary HP added each active tick while below the selected regeneration cap; the addition can cross the cap. | `0x44b360` |
| 5 | +0x14 | `regen_cap_alt` | V | Regeneration cap when object +0x54 is zero; otherwise regeneration uses primary maximum HP. | `0x44b360` |
| 6 | +0x18 | `secondary_hp_initial` | V | Initial secondary HP at object +0x3c; distinct from the virtual maximum-HP getter. | `0x44a210`, `0x44a350` |
| 7 | +0x1c | `speed_px_per_tick` | V | Maximum movement speed in pixels per logic tick. | `0x44aab0`, `0x44b360` |
| 8 | +0x20 | `sight_mask_id` | V | Light-mask resource ID; not a radius in pixels. | `0x42bc50` |
| 9 | +0x24 | `weapon_id` | V | Index into the weapon prototype table 0x515ac0; -1 means unarmed. Getter also rejects nonpositive damage for the target. | `0x42c3c0` |
| 10 | +0x28 | `unused_010` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 11 | +0x2c | `unused_011` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 12 | +0x30 | `unused_012` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 13 | +0x34 | `turret_turn_step` | V | Turret heading step per tick in 1/256-circle units; also used in the reload-time retarget threshold. | `0x44b360` |
| 14 | +0x38 | `armor_fire` | V | Fire and area-explosion damage factor returned by virtual +0x108; 256 is unity. Consumers: 0x429f60 and 0x44c130. | `0x44a170` |
| 15 | +0x3c | `armor_physical` | V | Physical armor value returned by virtual +0x10c; used by armor-dependent damage terms, not a universal damage multiplier. | `0x44a180` |
| 16 | +0x40 | `armor_auxiliary` | V | Value returned by virtual +0x110. No damage consumer found in the audited corpus; gameplay interpretation remains open. | `0x44a190` |
| 17 | +0x44 | `spawn_sound_list_id` | V | Sound-list ID: a random entry is played on placement; zero disables the list. | `0x44a780`, `0x44a8af`, `0x44a8bc`, `0x44a8c6`, `0x44a8dc`, `0x44a8e9`, `0x44a8f3`, `0x44a8fe`, `0x44a907`, `0x44a911`, `0x44a91e`, `0x44a92b`, `0x44a935`, `0x44a94b`, `0x44a958`, `0x44a962`, `0x44a96d`, `0x44a976`, `0x44a980`, `0x44a998`, `0x44a9ae`, `0x44a9cc`, `0x44a9d1`, `0x44a9ea`, `0x44a9f5`, `0x44aa15` |
| 18 | +0x48 | `name_string_pointer` | V | Guest pointer to the localized name returned by virtual +0x84 for argument zero; not a code pointer or stable resource ID. | `0x44ab60` |

## Truck

Table `0x4bc738`, 17 words; object pointer at `+0x68`; vtable `0x4948e8`.
Constructors: `0x460090`, `0x45fc00`, `0x4601e0`.
14 non-sentinel data columns; 17 nonzero columns including unused sentinels.

| Word | Offset | Name | Status | Meaning | Reading functions / evidence |
| --- | --- | --- | --- | --- | --- |
| 0 | +0x00 | `anim_id` | V | Body animation resource ID passed to the animation loader. | `0x460090`, `0x4601e0` |
| 1 | +0x04 | `hp_initial` (aliases: `hp_max`) | V | Initial primary HP at object +0x38; also the virtual +0x3c maximum. | `0x45fe50`, `0x460090`, `0x460a10` |
| 2 | +0x08 | `regen_per_tick` | V | Primary HP added each active tick while below the selected regeneration cap; the addition can cross the cap. | `0x460a10` |
| 3 | +0x0c | `regen_cap_alt` | V | Regeneration cap when object +0x54 is zero; otherwise regeneration uses primary maximum HP. | `0x460a10` |
| 4 | +0x10 | `secondary_hp_initial` | V | Initial secondary HP at object +0x3c; distinct from the virtual maximum-HP getter. | `0x460090` |
| 5 | +0x14 | `speed_px_per_tick` | V | Maximum movement speed in pixels per logic tick. | `0x460a10` |
| 6 | +0x18 | `sight_mask_id` | V | Light-mask resource ID; not a radius in pixels. | `0x415dd0` |
| 7 | +0x1c | `weapon_id` | V | Index into the weapon prototype table 0x515ac0; -1 means unarmed. Getter also rejects nonpositive damage for the target. | `0x4166a0` |
| 8 | +0x20 | `unused_008` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 9 | +0x24 | `unused_009` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 10 | +0x28 | `unused_010` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 11 | +0x2c | `armor_fire` | V | Fire and area-explosion damage factor returned by virtual +0x108; 256 is unity. Consumers: 0x429f60 and 0x44c130. | `0x45ff00` |
| 12 | +0x30 | `armor_physical` | V | Physical armor value returned by virtual +0x10c; used by armor-dependent damage terms, not a universal damage multiplier. | `0x45ff10` |
| 13 | +0x34 | `armor_auxiliary` | V | Value returned by virtual +0x110. No damage consumer found in the audited corpus; gameplay interpretation remains open. | `0x45ff20` |
| 14 | +0x38 | `spawn_sound_list_id` | V | Sound-list ID: a random entry is played on placement; zero disables the list. | `0x4602b0`, `0x460382`, `0x460393`, `0x4603b0` |
| 15 | +0x3c | `engine_sound_list_id` | V | Acceleration sound-list ID; repeated playback has a ten-tick cooldown. | `0x460a10` |
| 16 | +0x40 | `name_string_pointer` | V | Guest pointer to the localized name returned by virtual +0x84 for argument zero; not a code pointer or stable resource ID. | `0x460470` |

## Cannon

Table `0x4a8e18`, 16 words; object pointer at `+0x68`; vtable `0x490380`.
Constructors: `0x415f50`, `0x415c00`, `0x415e70`.
13 non-sentinel data columns; 16 nonzero columns including unused sentinels.

| Word | Offset | Name | Status | Meaning | Reading functions / evidence |
| --- | --- | --- | --- | --- | --- |
| 0 | +0x00 | `anim_id` | V | Body animation resource ID passed to the animation loader. | `0x415e70`, `0x415f50` |
| 1 | +0x04 | `turn_step` | V | Facing step per tick in 1/256-circle units; snaps to the desired heading within one step. | `0x417090` |
| 2 | +0x08 | `hp_initial` (aliases: `hp_max`) | V | Initial primary HP at object +0x38; also the virtual +0x3c maximum. | `0x415dc0`, `0x415e70`, `0x415f50`, `0x417090` |
| 3 | +0x0c | `regen_per_tick` | V | Primary HP added each active tick while below the selected regeneration cap; the addition can cross the cap. | `0x417090` |
| 4 | +0x10 | `regen_cap_alt` | V | Regeneration cap when object +0x54 is zero; otherwise regeneration uses primary maximum HP. | `0x417090` |
| 5 | +0x14 | `secondary_hp_initial` | V | Initial secondary HP at object +0x3c; distinct from the virtual maximum-HP getter. | `0x415e70`, `0x415f50` |
| 6 | +0x18 | `sight_mask_id` | V | Light-mask resource ID; not a radius in pixels. | `0x415dd0` |
| 7 | +0x1c | `weapon_id` | V | Index into the weapon prototype table 0x515ac0; -1 means unarmed. Getter also rejects nonpositive damage for the target. | `0x4166a0` |
| 8 | +0x20 | `unused_008` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 9 | +0x24 | `unused_009` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 10 | +0x28 | `unused_010` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 11 | +0x2c | `armor_fire` | V | Fire and area-explosion damage factor returned by virtual +0x108; 256 is unity. Consumers: 0x429f60 and 0x44c130. | `0x45ff00` |
| 12 | +0x30 | `armor_physical` | V | Physical armor value returned by virtual +0x10c; used by armor-dependent damage terms, not a universal damage multiplier. | `0x45ff10` |
| 13 | +0x34 | `armor_auxiliary` | V | Value returned by virtual +0x110. No damage consumer found in the audited corpus; gameplay interpretation remains open. | `0x45ff20` |
| 14 | +0x38 | `spawn_sound_list_id` | V | Sound-list ID: a random entry is played on placement; zero disables the list. | `0x416390`, `0x416499`, `0x4164b2`, `0x4164bf`, `0x4164cc`, `0x4164d6`, `0x4164ec`, `0x4164f9`, `0x416503`, `0x41650e`, `0x416517`, `0x416521`, `0x41652e`, `0x41653b`, `0x416545`, `0x41655b`, `0x416563`, `0x416568`, `0x41656c`, `0x416572`, `0x41657d`, `0x416586`, `0x416590`, `0x4165a8`, `0x4165be`, `0x4165dc`, `0x4165e1`, `0x4165f2`, `0x416612` |
| 15 | +0x3c | `name_string_pointer` | V | Guest pointer to the localized name returned by virtual +0x84 for argument zero; not a code pointer or stable resource ID. | `0x4166e0` |

## Bird

Table `0x49b370`, 22 words; object pointer at `+0x64`; vtable `0x48f97c`.
Constructors: `0x407360`, `0x4070a0`, `0x407450`.
18 non-sentinel data columns; 21 nonzero columns including unused sentinels.

| Word | Offset | Name | Status | Meaning | Reading functions / evidence |
| --- | --- | --- | --- | --- | --- |
| 0 | +0x00 | `anim_id` | V | Body animation resource ID passed to the animation loader. | `0x407360`, `0x407450` |
| 1 | +0x04 | `hp_initial` (aliases: `hp_max`) | V | Initial primary HP at object +0x38; also the virtual +0x3c maximum. | `0x407360`, `0x407b10`, `0x4711a0` |
| 2 | +0x08 | `regen_per_tick` | V | Primary HP added each active tick while below the selected regeneration cap; the addition can cross the cap. | `0x407b10` |
| 3 | +0x0c | `regen_cap_alt` | V | Regeneration cap when object +0x54 is zero; otherwise regeneration uses primary maximum HP. | `0x407b10` |
| 4 | +0x10 | `secondary_hp_initial` | V | Initial secondary HP at object +0x3c; distinct from the virtual maximum-HP getter. | `0x407360` |
| 5 | +0x14 | `speed_px_per_tick` | V | Maximum movement speed in pixels per logic tick. | `0x407b10` |
| 6 | +0x18 | `turn_step` | V | Flight heading step per tick in 1/256-circle units; also controls the circling radius. | `0x407b10` |
| 7 | +0x1c | `cruise_altitude_px` | V | Cruise height; object +0x7c rises one pixel per tick toward it. | `0x407b10` |
| 8 | +0x20 | `sight_mask_id` | V | Light-mask resource ID; not a radius in pixels. | `0x407240` |
| 9 | +0x24 | `weapon_id` | V | Index into the weapon prototype table 0x515ac0; -1 means unarmed. Getter also rejects nonpositive damage for the target. | `0x407780` |
| 10 | +0x28 | `unused_010` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 11 | +0x2c | `unused_011` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 12 | +0x30 | `unused_012` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 13 | +0x34 | `organic_flag` | V | Low byte selects Bird versus Vehicle name and suppresses machine explosion sound/debris when nonzero. | `0x4077e0`, `0x407870`, `0x4078c6`, `0x4078cb`, `0x4078ea`, `0x4078f9`, `0x4078ff` |
| 14 | +0x38 | `armor_fire` | V | Fire and area-explosion damage factor returned by virtual +0x108; 256 is unity. Consumers: 0x429f60 and 0x44c130. | `0x4712a0` |
| 15 | +0x3c | `armor_physical` | V | Physical armor value returned by virtual +0x10c; used by armor-dependent damage terms, not a universal damage multiplier. | `0x407270` |
| 16 | +0x40 | `armor_auxiliary` | V | Value returned by virtual +0x110. No damage consumer found in the audited corpus; gameplay interpretation remains open. | `0x407280` |
| 17 | +0x44 | `spawn_sound_list_id` | V | Sound-list ID: a random entry is played on placement; zero disables the list. | `0x407520`, `0x4075d7`, `0x4075e0`, `0x407611` |
| 18 | +0x48 | `hit_sound_list_id` | V | Sound-list ID played after damage; zero in all exported Bird records. | `0x4076a0`, `0x4076b6` |
| 19 | +0x4c | `death_sound_list_id` | V | Sound-list ID played by the death handler; zero disables the list. | `0x407870` |
| 20 | +0x50 | `attack_sound_list_id` | V | Sound-list ID played when the weapon fires; zero disables the list. | `0x407b10` |
| 21 | +0x54 | `name_string_pointer` | V | Guest pointer to the localized name returned by virtual +0x84 for argument zero; not a code pointer or stable resource ID. | `0x4077e0` |

## Zeppelin

Table `0x4bcd00`, 17 words; object pointer at `+0x64`; vtable `0x4951b8`.
Constructors: `0x471390`, `0x470fd0`, `0x471480`.
14 non-sentinel data columns; 17 nonzero columns including unused sentinels.

| Word | Offset | Name | Status | Meaning | Reading functions / evidence |
| --- | --- | --- | --- | --- | --- |
| 0 | +0x00 | `anim_id` | V | Body animation resource ID passed to the animation loader. | `0x471390`, `0x471480` |
| 1 | +0x04 | `hp_initial` (aliases: `hp_max`) | V | Initial primary HP at object +0x38; also the virtual +0x3c maximum. | `0x4711a0`, `0x471390`, `0x4719b0` |
| 2 | +0x08 | `regen_per_tick` | V | Primary HP added each active tick while below the selected regeneration cap; the addition can cross the cap. | `0x4719b0` |
| 3 | +0x0c | `regen_cap_alt` | V | Regeneration cap when object +0x54 is zero; otherwise regeneration uses primary maximum HP. | `0x4719b0` |
| 4 | +0x10 | `secondary_hp_initial` | V | Initial secondary HP at object +0x3c; distinct from the virtual maximum-HP getter. | `0x471390` |
| 5 | +0x14 | `speed_px_per_tick` | V | Maximum movement speed in pixels per logic tick. | `0x4719b0` |
| 6 | +0x18 | `cruise_altitude_px` | V | Cruise height; object +0x7c rises one pixel per tick toward it. | `0x4719b0` |
| 7 | +0x1c | `sight_mask_id` | V | Light-mask resource ID; not a radius in pixels. | `0x471180` |
| 8 | +0x20 | `weapon_id` | V | Index into the weapon prototype table 0x515ac0; -1 means unarmed. Getter also rejects nonpositive damage for the target. | `0x471680` |
| 9 | +0x24 | `unused_009` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 10 | +0x28 | `unused_010` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 11 | +0x2c | `unused_011` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 12 | +0x30 | `armor_fire` | V | Fire and area-explosion damage factor returned by virtual +0x108; 256 is unity. Consumers: 0x429f60 and 0x44c130. | `0x471280` |
| 13 | +0x34 | `armor_physical` | V | Physical armor value returned by virtual +0x10c; used by armor-dependent damage terms, not a universal damage multiplier. | `0x471290` |
| 14 | +0x38 | `armor_auxiliary` | V | Value returned by virtual +0x110. No damage consumer found in the audited corpus; gameplay interpretation remains open. | `0x4712a0` |
| 15 | +0x3c | `spawn_sound_list_id` | V | Sound-list ID: a random entry is played on placement; zero disables the list. | `0x471550` |
| 16 | +0x40 | `name_string_pointer` | V | Guest pointer to the localized name returned by virtual +0x84 for argument zero; not a code pointer or stable resource ID. | `0x4716c0` |

## Defence

Table `0x4aa3b0`, 68 words; object pointer at `+0x6c`; vtable `0x4905bc`.
Constructors: `0x41cce0`, `0x41c8f0`, `0x41cf00`.
39 non-sentinel data columns; 42 nonzero columns including unused sentinels.

| Word | Offset | Name | Status | Meaning | Reading functions / evidence |
| --- | --- | --- | --- | --- | --- |
| 0 | +0x00 | `anim_id` | V | Base sprite animation resource ID. | `0x41cce0`, `0x41cf00` |
| 1 | +0x04 | `turret_variant_index` | V | Selects a group of four entries in base animation metadata section 7. | `0x41d300` |
| 2 | +0x08 | `turret_anim_id` | V | Turret sprite resource ID; zero means no second layer. | `0x41cce0`, `0x41cf00` |
| 3 | +0x0c | `hp_initial` (aliases: `hp_max`) | V | Primary initial and maximum HP. | `0x41cbe0`, `0x41cf00`, `0x41d1a0`, `0x44a0f0` |
| 4 | +0x10 | `turret_turn_step` | V | Turret heading step per tick in 1/256-circle units; also controls reload-time retargeting. | `0x41d790` |
| 5 | +0x14 | `hit_rect_left_px` | V | Hit rectangle left coordinate in pixels relative to the object position. Original instructions confirm both coordinates where decompilation loses a register. | `0x41e220` |
| 6 | +0x18 | `hit_rect_top_px` | V | Hit rectangle top coordinate in pixels relative to the object position. Original instructions confirm both coordinates where decompilation loses a register. | `0x41e220` |
| 7 | +0x1c | `hit_rect_right_px` | V | Hit rectangle right coordinate in pixels relative to the object position. Original instructions confirm both coordinates where decompilation loses a register. | `0x41e220` |
| 8 | +0x20 | `hit_rect_bottom_px` | V | Hit rectangle bottom coordinate in pixels relative to the object position. Original instructions confirm both coordinates where decompilation loses a register. | `0x41e220` |
| 9 | +0x24 | `footprint_0_0` | V | Footprint row 0, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 10 | +0x28 | `footprint_0_1` | V | Footprint row 0, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 11 | +0x2c | `footprint_0_2` | V | Footprint row 0, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 12 | +0x30 | `footprint_0_3` | V | Footprint row 0, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 13 | +0x34 | `footprint_0_4` | V | Footprint row 0, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 14 | +0x38 | `footprint_0_5` | V | Footprint row 0, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 15 | +0x3c | `footprint_0_6` | V | Footprint row 0, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 16 | +0x40 | `footprint_1_0` | V | Footprint row 1, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 17 | +0x44 | `footprint_1_1` | V | Footprint row 1, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 18 | +0x48 | `footprint_1_2` | V | Footprint row 1, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 19 | +0x4c | `footprint_1_3` | V | Footprint row 1, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 20 | +0x50 | `footprint_1_4` | V | Footprint row 1, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 21 | +0x54 | `footprint_1_5` | V | Footprint row 1, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 22 | +0x58 | `footprint_1_6` | V | Footprint row 1, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 23 | +0x5c | `footprint_2_0` | V | Footprint row 2, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 24 | +0x60 | `footprint_2_1` | V | Footprint row 2, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 25 | +0x64 | `footprint_2_2` | V | Footprint row 2, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 26 | +0x68 | `footprint_2_3` | V | Footprint row 2, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 27 | +0x6c | `footprint_2_4` | V | Footprint row 2, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 28 | +0x70 | `footprint_2_5` | V | Footprint row 2, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 29 | +0x74 | `footprint_2_6` | V | Footprint row 2, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 30 | +0x78 | `footprint_3_0` | V | Footprint row 3, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 31 | +0x7c | `footprint_3_1` | V | Footprint row 3, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 32 | +0x80 | `footprint_3_2` | V | Footprint row 3, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 33 | +0x84 | `footprint_3_3` | V | Footprint row 3, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 34 | +0x88 | `footprint_3_4` | V | Footprint row 3, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 35 | +0x8c | `footprint_3_5` | V | Footprint row 3, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 36 | +0x90 | `footprint_3_6` | V | Footprint row 3, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 37 | +0x94 | `footprint_4_0` | V | Footprint row 4, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 38 | +0x98 | `footprint_4_1` | V | Footprint row 4, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 39 | +0x9c | `footprint_4_2` | V | Footprint row 4, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 40 | +0xa0 | `footprint_4_3` | V | Footprint row 4, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 41 | +0xa4 | `footprint_4_4` | V | Footprint row 4, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 42 | +0xa8 | `footprint_4_5` | V | Footprint row 4, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 43 | +0xac | `footprint_4_6` | V | Footprint row 4, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 44 | +0xb0 | `footprint_5_0` | V | Footprint row 5, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 45 | +0xb4 | `footprint_5_1` | V | Footprint row 5, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 46 | +0xb8 | `footprint_5_2` | V | Footprint row 5, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 47 | +0xbc | `footprint_5_3` | V | Footprint row 5, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 48 | +0xc0 | `footprint_5_4` | V | Footprint row 5, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 49 | +0xc4 | `footprint_5_5` | V | Footprint row 5, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 50 | +0xc8 | `footprint_5_6` | V | Footprint row 5, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 51 | +0xcc | `footprint_6_0` | V | Footprint row 6, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 52 | +0xd0 | `footprint_6_1` | V | Footprint row 6, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 53 | +0xd4 | `footprint_6_2` | V | Footprint row 6, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 54 | +0xd8 | `footprint_6_3` | V | Footprint row 6, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 55 | +0xdc | `footprint_6_4` | V | Footprint row 6, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 56 | +0xe0 | `footprint_6_5` | V | Footprint row 6, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 57 | +0xe4 | `footprint_6_6` | V | Footprint row 6, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x40a080`, `0x41d240`, `0x41e120`, `0x42acd0`, `0x45bd50` |
| 58 | +0xe8 | `sight_mask_id` | V | Light-mask resource ID; not a radius in pixels. | `0x41cb80` |
| 59 | +0xec | `weapon_id` | V | Weapon prototype index into 0x515ac0; -1 means unarmed; getter rejects nonpositive damage for the target. | `0x41d320` |
| 60 | +0xf0 | `unused_060` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 61 | +0xf4 | `unused_061` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 62 | +0xf8 | `unused_062` | unused | No runtime read found in the audited descriptor consumers or virtual accessors. Constant -1 in all exported records; spare-weapon interpretation is unproven. | No reader found in this survey |
| 63 | +0xfc | `projectile_launch_height_px` | G | Argument forwarded with the firing position and target to the weapon hit/launch method; height interpretation inferred from the calling convention. | `0x41d790` |
| 64 | +0x100 | `armor_fire` | V | Fire and area-explosion damage factor, virtual +0x108; 256 is unity. | `0x41cc00` |
| 65 | +0x104 | `armor_physical` | V | Physical armor value, virtual +0x10c; zero in all exported records. | `0x41cc20` |
| 66 | +0x108 | `armor_auxiliary` | V | Value returned by virtual +0x110; zero in all exported records. No damage consumer found; gameplay meaning remains open. | `0x41cc40` |
| 67 | +0x10c | `name_string_pointer` | V | Guest pointer to the localized name, returned by virtual +0x84 for argument zero; not a code pointer. | `0x41d360` |

## Building

Table `0x49b538`, 322 words; object pointer at `+0x64`; vtable `0x48fd20`.
Constructors: `0x40fb70`, `0x40f530`, `0x40fdf0`.
55 non-sentinel data columns; 55 nonzero columns including unused sentinels.

| Word | Offset | Name | Status | Meaning | Reading functions / evidence |
| --- | --- | --- | --- | --- | --- |
| 0 | +0x00 | `anim_id` | V | Base sprite animation resource ID. | `0x40fb70`, `0x40fdf0` |
| 1 | +0x04 | `hp_initial` (aliases: `hp_max`) | V | Primary maximum HP installed on completion; construction begins at zero HP. | `0x40fa50`, `0x410010`, `0x4711a0` |
| 2 | +0x08 | `hit_rect_left_px` | V | Hit rectangle left coordinate in pixels relative to the object position. Original instructions confirm both coordinates where decompilation loses a register. | `0x411330` |
| 3 | +0x0c | `hit_rect_top_px` | V | Hit rectangle top coordinate in pixels relative to the object position. Original instructions confirm both coordinates where decompilation loses a register. | `0x411330` |
| 4 | +0x10 | `hit_rect_right_px` | V | Hit rectangle right coordinate in pixels relative to the object position. Original instructions confirm both coordinates where decompilation loses a register. | `0x411330` |
| 5 | +0x14 | `hit_rect_bottom_px` | V | Hit rectangle bottom coordinate in pixels relative to the object position. Original instructions confirm both coordinates where decompilation loses a register. | `0x411330` |
| 6 | +0x18 | `footprint_0_0` | V | Footprint row 0, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x41015d`, `0x410180`, `0x41018c`, `0x4101c3`, `0x4101c7`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 7 | +0x1c | `footprint_0_1` | V | Footprint row 0, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 8 | +0x20 | `footprint_0_2` | V | Footprint row 0, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 9 | +0x24 | `footprint_0_3` | V | Footprint row 0, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 10 | +0x28 | `footprint_0_4` | V | Footprint row 0, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 11 | +0x2c | `footprint_0_5` | V | Footprint row 0, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 12 | +0x30 | `footprint_0_6` | V | Footprint row 0, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 13 | +0x34 | `footprint_1_0` | V | Footprint row 1, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 14 | +0x38 | `footprint_1_1` | V | Footprint row 1, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 15 | +0x3c | `footprint_1_2` | V | Footprint row 1, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 16 | +0x40 | `footprint_1_3` | V | Footprint row 1, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 17 | +0x44 | `footprint_1_4` | V | Footprint row 1, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 18 | +0x48 | `footprint_1_5` | V | Footprint row 1, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 19 | +0x4c | `footprint_1_6` | V | Footprint row 1, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 20 | +0x50 | `footprint_2_0` | V | Footprint row 2, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 21 | +0x54 | `footprint_2_1` | V | Footprint row 2, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 22 | +0x58 | `footprint_2_2` | V | Footprint row 2, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 23 | +0x5c | `footprint_2_3` | V | Footprint row 2, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 24 | +0x60 | `footprint_2_4` | V | Footprint row 2, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 25 | +0x64 | `footprint_2_5` | V | Footprint row 2, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 26 | +0x68 | `footprint_2_6` | V | Footprint row 2, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 27 | +0x6c | `footprint_3_0` | V | Footprint row 3, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 28 | +0x70 | `footprint_3_1` | V | Footprint row 3, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 29 | +0x74 | `footprint_3_2` | V | Footprint row 3, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 30 | +0x78 | `footprint_3_3` | V | Footprint row 3, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 31 | +0x7c | `footprint_3_4` | V | Footprint row 3, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 32 | +0x80 | `footprint_3_5` | V | Footprint row 3, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 33 | +0x84 | `footprint_3_6` | V | Footprint row 3, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 34 | +0x88 | `footprint_4_0` | V | Footprint row 4, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 35 | +0x8c | `footprint_4_1` | V | Footprint row 4, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 36 | +0x90 | `footprint_4_2` | V | Footprint row 4, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 37 | +0x94 | `footprint_4_3` | V | Footprint row 4, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 38 | +0x98 | `footprint_4_4` | V | Footprint row 4, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 39 | +0x9c | `footprint_4_5` | V | Footprint row 4, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 40 | +0xa0 | `footprint_4_6` | V | Footprint row 4, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 41 | +0xa4 | `footprint_5_0` | V | Footprint row 5, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 42 | +0xa8 | `footprint_5_1` | V | Footprint row 5, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 43 | +0xac | `footprint_5_2` | V | Footprint row 5, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 44 | +0xb0 | `footprint_5_3` | V | Footprint row 5, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 45 | +0xb4 | `footprint_5_4` | V | Footprint row 5, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 46 | +0xb8 | `footprint_5_5` | V | Footprint row 5, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 47 | +0xbc | `footprint_5_6` | V | Footprint row 5, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 48 | +0xc0 | `footprint_6_0` | V | Footprint row 6, column 0, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 49 | +0xc4 | `footprint_6_1` | V | Footprint row 6, column 1, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 50 | +0xc8 | `footprint_6_2` | V | Footprint row 6, column 2, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 51 | +0xcc | `footprint_6_3` | V | Footprint row 6, column 3, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 52 | +0xd0 | `footprint_6_4` | V | Footprint row 6, column 4, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 53 | +0xd4 | `footprint_6_5` | V | Footprint row 6, column 5, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 54 | +0xd8 | `footprint_6_6` | V | Footprint row 6, column 6, relative to the 7x7 grid centered on the object tile. Only value 2 requires/occupies a tile; meanings of 0 versus 1 are not distinguished by inspected consumers. | `0x409940`, `0x409ce0`, `0x409e60`, `0x410010`, `0x4104b0`, `0x411230`, `0x42acd0`, `0x45bd50` |
| 55 | +0xdc | `sight_mask_id` | V | Light-mask resource ID; not a radius in pixels. | `0x40fa30` |
| 56 | +0xe0 | `armor_fire` | V | Fire and area-explosion damage factor, virtual +0x108; 256 is unity. | `0x40fa90` |
| 57 | +0xe4 | `armor_physical` | V | Physical armor value, virtual +0x10c; zero in all exported records. | `0x40fab0` |
| 58 | +0xe8 | `armor_auxiliary` | V | Value returned by virtual +0x110; zero in all exported records. No damage consumer found; gameplay meaning remains open. | `0x40fad0` |
| 59 | +0xec | `flags_bytes` | V | Low byte +0xec rejects damage and masks reported HP outside construction; byte +0xed selects terrain-dependent gold. Upper bytes have no identified reader. | `0x40f9b0`, `0x410010`, `0x410560` |
| 60 | +0xf0 | `harvest_grass_factor` | V | Constant gold rate, or terrain channel A coefficient when +0xed is set; channel A resource identity remains open. | `0x409ce0`, `0x410010`, `0x41015d`, `0x410180`, `0x41018c`, `0x4101c3`, `0x4101c7`, `0x41020e`, `0x410235`, `0x410241`, `0x41027e`, `0x410293`, `0x4102b7`, `0x4102c3`, `0x4102fa`, `0x4102fe`, `0x410580`, `0x45bd50` |
| 61 | +0xf4 | `harvest_stone_factor` | V | Terrain channel B gold coefficient; channel B resource identity remains open. | `0x409ce0`, `0x410010`, `0x41015d`, `0x410180`, `0x41018c`, `0x4101c3`, `0x4101c7`, `0x410580`, `0x45bd50` |
| 62 | +0xf8 | `repair_income_per_100_clock_ticks` | V | Repair-credit rate passed to a separate accumulator, not passive gold; historical field name retained. | `0x40fb70` |
| 63 | +0xfc | `gold_schedule_flag` | V | Low byte: nonzero uses an explicit day-clock gold schedule; zero uses a periodic trigger. | `0x410010` |
| 64 | +0x100 | `harvest_schedule_count_or_period` | V | Number of explicit schedule entries when d63 is set; otherwise period in ticks. -1 disables the gold accumulator. | `0x410010`, `0x41015d`, `0x410180`, `0x41018c`, `0x41020e`, `0x410235`, `0x410241`, `0x410293`, `0x4102b7`, `0x4102c3`, `0x410580`, `0x45ea00` |
| 65 | +0x104 | `gold_schedule_clock_000` | V | Day-clock value for gold payout entry 0; only the first d64 entries are copied when d63 is set. Capacity 255, currently at most ten entries; unused tail is zero. | `0x410010`, `0x41020e`, `0x410235`, `0x410241`, `0x45f6e0` |
| 66 | +0x108 | `gold_schedule_clock_001` | V | Day-clock value for gold payout entry 1; only the first d64 entries are copied when d63 is set. Capacity 255, currently at most ten entries; unused tail is zero. | `0x410010`, `0x45f6e0` |
| 67 | +0x10c | `gold_schedule_clock_002` | V | Day-clock value for gold payout entry 2; only the first d64 entries are copied when d63 is set. Capacity 255, currently at most ten entries; unused tail is zero. | `0x410010`, `0x45f6e0` |
| 68 | +0x110 | `gold_schedule_clock_003` | V | Day-clock value for gold payout entry 3; only the first d64 entries are copied when d63 is set. Capacity 255, currently at most ten entries; unused tail is zero. | `0x410010`, `0x45f6e0` |
| 69 | +0x114 | `gold_schedule_clock_004` | V | Day-clock value for gold payout entry 4; only the first d64 entries are copied when d63 is set. Capacity 255, currently at most ten entries; unused tail is zero. | `0x410010`, `0x45f6e0` |
| 70 | +0x118 | `gold_schedule_clock_005` | V | Day-clock value for gold payout entry 5; only the first d64 entries are copied when d63 is set. Capacity 255, currently at most ten entries; unused tail is zero. | `0x410010`, `0x45f6e0` |
| 71 | +0x11c | `gold_schedule_clock_006` | V | Day-clock value for gold payout entry 6; only the first d64 entries are copied when d63 is set. Capacity 255, currently at most ten entries; unused tail is zero. | `0x410010`, `0x45f6e0` |
| 72 | +0x120 | `gold_schedule_clock_007` | V | Day-clock value for gold payout entry 7; only the first d64 entries are copied when d63 is set. Capacity 255, currently at most ten entries; unused tail is zero. | `0x410010`, `0x45f6e0` |
| 73 | +0x124 | `gold_schedule_clock_008` | V | Day-clock value for gold payout entry 8; only the first d64 entries are copied when d63 is set. Capacity 255, currently at most ten entries; unused tail is zero. | `0x410010`, `0x45f6e0` |
| 74 | +0x128 | `gold_schedule_clock_009` | V | Day-clock value for gold payout entry 9; only the first d64 entries are copied when d63 is set. Capacity 255, currently at most ten entries; unused tail is zero. | `0x410010`, `0x45f6e0` |
| 75..319 | +0x12c..+0x4fc | `gold_schedule_clock_010` .. `gold_schedule_clock_254` | V (array layout) | All-zero schedule capacity tail; no enabled record currently uses these entries. | `0x410010`, `0x45f6e0` |
| 320 | +0x500 | `repair_credit_period_ticks` | V | Periodic interval for the independent repair-credit accumulator; -1 where no accumulator is enabled. | `0x40fb70` |
| 321 | +0x504 | `name_string_pointer` | V | Guest pointer to the localized name, returned by virtual +0x84 for argument zero; not a code pointer. | `0x410620` |

## Weapon status reconciliation

Classes 0, 3, 7, 9 and 11 now report verified record/formula evidence from
[weapon_class_map.md](weapon_class_map.md), with function addresses. Their
class-to-constructor links remain separately labeled G, matching that document's
evidence boundary. Class 11 helper sequencing remains open. This avoids upgrading
a verified damage getter into a claim that the entire weapon effect is understood.

The class 0 shared getters `0x4039a0` and `0x4039b0` confirm reload d2 and range
d3; movement `0x4069c0` uses d1 as speed. Class 3 `0x404b90` multiplies the base
by integer `floor(48/speed)` and then adds the secondary weapon result even when
the base is suppressed. Class 7 records now expose inner/outer radius, damage,
required airborne flag and the 204/256 category multiplier. Both radii include
the target extent (28 pixels); full inner damage bypasses armor, while outer
falloff uses 8.8 precision before category and armor factors. Class 9 carries its
verified category and armor terms. Class 11's tenfold damage getter is an estimate,
not evidence of exactly ten executed burn applications.

## Validation

`descriptor_fields` in CTest runs [test_descriptor_fields.py](../tests/test_descriptor_fields.py)
without private data. It rejects missing names, statuses, evidence or mappings for
nonzero columns, including constant -1 slots; a zero in one type cannot hide a
column used by another. It also checks the HP/weapon/armor corrections, weapon
evidence boundaries and byte-for-byte regeneration into a temporary output.
All-zero columns may be ignored by coverage validation. The builder currently
requires a complete, unique 520-word layout, so later data additions in existing
zero columns inherit an explicit name instead of silently disappearing.
