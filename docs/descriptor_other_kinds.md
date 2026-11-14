# Descriptor words of Bird, Zeppelin, Truck, Cannon, Headtruck and Spider

Evidence date: 2026-11-14. Read-only analysis of the decompilation corpus, of 14 functions
decompiled on demand (`work/gaps/dec_460550.c`) and of the vtables and small accessors read
directly from the executable image (functions marked `*` are not in the corpus and were read
as disassembly).

**V** = the read is seen in code at the cited address. **G** = guess. "named" in the notes
means the word already has a name in `tools/build_rules.py`.

## Shared conventions (apply to every table below)

- Virtual slot numbers are byte offsets in the object vtable. Vtables: Bird `0x48f97c`,
  Zeppelin `0x4951b8`, Truck `0x4948e8`, Cannon `0x490380`, Headtruck `0x492c38`, Spider
  `0x4940ec`.
- Slot `+0x3c` = maximum hit points getter. Slot `+0x74` = sight mask id (returns 0 when the
  object alive flag `+0x5c` is clear). Slot `+0xa8` = weapon lookup: descriptor word is an
  index into the weapon pointer table `0x515ac0`, `-1` means no weapon. Slot `+0x84` = name
  accessor (argument 0 = unit name, 1 = class name string). Slot `+0xc4` = place on the map
  (sets `+0x5d`, registers the tile, sets state 1) and plays a random entry of the spawn sound
  list. Slot `+0x5c` = death handler. Slot `+0x60` = Think.
- Armor: three consecutive virtuals `+0x108`, `+0x10c`, `+0x110`, each copies one descriptor
  word. They are 8.8 damage multipliers (256 = normal, 512 = double damage, 128 = half).
  `+0x108` is consumed by the burn weapon `0x429f60` and the area explosion `0x44c130`
  (fire and explosion factor). `+0x10c` is consumed by `0x444160`, `0x403da0`, `0x46f2c0`
  (physical factor, as in `docs/engine_rules_status.md`). No caller of `+0x110` on these
  objects was found (the `+0x110` calls at `0x401e80` to `0x401fe4` take an argument and
  belong to another class), so the third word stays a guess.
- Regeneration, same code in every Think: if object `+0x54` is zero the cap is the
  "regeneration cap" word, otherwise the cap is the maximum hit point word; while hit points
  (`+0x38`) are below the cap, the "regeneration per tick" word is added each tick.
- Constructors copy two words into the object: `+0x38` (hit points) and `+0x3c` (the project's
  "secondary hit points").
- Sound list ids are arguments of `0x401250` (load list), one random entry is played with
  `0x401480`.
- Speed words: acceleration and braking per tick are `speed * speed / 48 + 1`, capped at the
  speed word (Truck `0x460a10`, Headtruck `0x42c880`, Zeppelin `0x4719b0`).

## Bird, table `0x49b370`, 22 words, pointer at object `+0x64`

| Word | Offset | Name | V/G | Evidence | Notes |
| --- | --- | --- | --- | --- | --- |
| 0 | `+0x00` | animation set id | V | `0x407360`, `0x407450` | passed to the animation loader `0x4030c0` |
| 1 | `+0x04` | initial and maximum hit points | V | `0x407360`, `0x4711a0*` (slot `+0x3c`), `0x407b10` | named |
| 2 | `+0x08` | regeneration per tick | V | `0x407b10` | 12, 12, 15 |
| 3 | `+0x0c` | regeneration cap | V | `0x407b10` | 1700, 3000, 13000 |
| 4 | `+0x10` | secondary hit points, initial | V | `0x407360` (to object `+0x3c`) | 7000, 12000, 8000 |
| 5 | `+0x14` | maximum flight speed, px per tick | V | `0x407b10` | 12, 18, 15. Speed climbs by 2 per tick up to it; brakes near the target |
| 6 | `+0x18` | turn rate, heading units (of 256) per tick | V | `0x407b10` | 8, 7, 6. Also sets the circling radius `speed * 256 / 6 / turn rate`; divided by 3 when close to an attacked target |
| 7 | `+0x1c` | cruise altitude | V | `0x407b10` | 85, 110, 110. Object `+0x7c` climbs 1 per tick up to it (starts at 30, `0x407360`) |
| 8 | `+0x20` | sight mask id | V | `0x407240*` (slot `+0x74`) | named |
| 9 | `+0x24` | weapon id | V | `0x407780` (slot `+0xa8`) | 52, 35, -1 (third bird unarmed) |
| 10 to 12 | `+0x28` to `+0x30` | unused | V (no reader) | | always -1 |
| 13 | `+0x34` | living creature flag (byte) | V | `0x4077e0`, `0x407870*`, `0x4078c6` | 0 = machine: class name "Vehicle", death adds explosion sound list 0x96 and debris effect 0x98. 1 = class name "Bird", no explosion |
| 14 | `+0x38` | armor factor, fire and explosion | V | `0x4712a0` (slot `+0x108`) | 256 |
| 15 | `+0x3c` | armor factor, physical | V | `0x407270*` (slot `+0x10c`) | 256 |
| 16 | `+0x40` | armor factor, third | G | `0x407280*` (slot `+0x110`), accessor only | 256, no consumer found |
| 17 | `+0x44` | spawn sound list id | V | `0x407520*`, fragments `0x4075d7`, `0x4075e0` | 9, 9, 10 |
| 18 | `+0x48` | hit sound list id | V | `0x4076a0` (slot `+0x124`, after base damage `0x43ea30`) | 0 in all records, carries no data |
| 19 | `+0x4c` | death sound list id | V | `0x407870*` (slot `+0x5c`) | 0, 0, 12 |
| 20 | `+0x50` | attack sound list id | V | `0x407b10` (played right after the weapon fires) | 0, 202, 0 |
| 21 | `+0x54` | unit name string pointer | V | `0x4077e0` | pointer, not data |

## Zeppelin, table `0x4bcd00`, 17 words, pointer at object `+0x64`

| Word | Offset | Name | V/G | Evidence | Notes |
| --- | --- | --- | --- | --- | --- |
| 0 | `+0x00` | animation set id | V | `0x471390`, `0x471480` | |
| 1 | `+0x04` | initial and maximum hit points | V | `0x471390`, `0x4711a0*`, `0x4719b0` | named |
| 2 | `+0x08` | regeneration per tick | V | `0x4719b0` | 12 |
| 3 | `+0x0c` | regeneration cap | V | `0x4719b0` | 2200, 6000 |
| 4 | `+0x10` | secondary hit points, initial | V | `0x471390` | 3000 |
| 5 | `+0x14` | speed, px per tick | V | `0x4719b0` | named |
| 6 | `+0x18` | cruise altitude | V | `0x4719b0` | 95, 110. Object `+0x7c` climbs 1 per tick up to it |
| 7 | `+0x1c` | sight mask id | V | `0x471180*` | named |
| 8 | `+0x20` | weapon id | V | `0x471680` (slot `+0xa8`) | -1 (transport), 44 |
| 9 to 11 | `+0x24` to `+0x2c` | unused | V (no reader) | | always -1 |
| 12 | `+0x30` | armor factor, fire and explosion | V | `0x471280` (slot `+0x108`) | 512: double damage from fire and explosions |
| 13 | `+0x34` | armor factor, physical | V | `0x471290` (slot `+0x10c`) | 256 |
| 14 | `+0x38` | armor factor, third | G | `0x4712a0` (slot `+0x110`), accessor only | 256 |
| 15 | `+0x3c` | spawn sound list id | V | `0x471550*` (slot `+0xc4`) | 9 |
| 16 | `+0x40` | unit name string pointer | V | `0x4716c0` | class name is always "Vehicle" |

The death handler `0x471740` uses fixed sound list 0xf and debris effect 0x98, no descriptor word.

## Truck (Catapult), table `0x4bc738`, 17 words, pointer at object `+0x68`

| Word | Offset | Name | V/G | Evidence | Notes |
| --- | --- | --- | --- | --- | --- |
| 0 | `+0x00` | animation set id | V | `0x460090`, `0x4601e0` | |
| 1 | `+0x04` | initial and maximum hit points | V | `0x460090` (to object `+0x38`), `0x45fe50*` (slot `+0x3c`), `0x460a10` | 10000. Correction: `build_rules.py` has `hp_initial` at word 4 (guess); it is word 1 |
| 2 | `+0x08` | regeneration per tick | V | `0x460a10` | 12 |
| 3 | `+0x0c` | regeneration cap | V | `0x460a10` | 2500 |
| 4 | `+0x10` | secondary hit points, initial | V | `0x460090` (to object `+0x3c`) | 3000 |
| 5 | `+0x14` | speed, px per tick | V | `0x460a10` | named |
| 6 | `+0x18` | sight mask id | V | `0x415dd0*` | named |
| 7 | `+0x1c` | weapon id | V | `0x4166a0` (slot `+0xa8` of both Truck and Cannon) | 32 |
| 8 to 10 | `+0x20` to `+0x28` | unused | V (no reader) | | always -1 |
| 11 | `+0x2c` | armor factor, fire and explosion | V | `0x45ff00*` (slot `+0x108`) | 256 |
| 12 | `+0x30` | armor factor, physical | V | `0x45ff10*` (slot `+0x10c`) | 256 |
| 13 | `+0x34` | armor factor, third | G | `0x45ff20*` (slot `+0x110`), accessor only | 256 |
| 14 | `+0x38` | spawn sound list id | V | `0x4602b0*`, fragments `0x460382` to `0x4603b0` | 8 |
| 15 | `+0x3c` | engine (acceleration) sound list id | V | `0x460a10` | 13. Played while accelerating, then a 10 tick cooldown in object `+0x98` |
| 16 | `+0x40` | unit name string pointer | V | `0x460470` | |

Body turn rate is a constant 8 heading units per tick in `0x460a10`, not a descriptor word.
The death handler `0x460550` uses fixed lists 0x96, 0x98, 0x71.

## Cannon (Hopcannon), table `0x4a8e18`, 16 words, pointer at object `+0x68`

| Word | Offset | Name | V/G | Evidence | Notes |
| --- | --- | --- | --- | --- | --- |
| 0 | `+0x00` | animation set id | V | `0x415e70`, `0x415f50` | 28 |
| 1 | `+0x04` | turn rate, heading units per tick | V | `0x417090` | 8. Turns the facing byte at object `+0x81` toward the target; snaps when the difference is smaller |
| 2 | `+0x08` | initial and maximum hit points | V | `0x415e70`, `0x415f50`, `0x415dc0*` (slot `+0x3c`), `0x417090` | 28000 |
| 3 | `+0x0c` | regeneration per tick | V | `0x417090` | 12 |
| 4 | `+0x10` | regeneration cap | V | `0x417090` | 7000 |
| 5 | `+0x14` | secondary hit points, initial | V | `0x415e70`, `0x415f50` (to object `+0x3c`) | 28000 |
| 6 | `+0x18` | sight mask id | V | `0x415dd0*` | named |
| 7 | `+0x1c` | weapon id | V | `0x4166a0` | 2 |
| 8 to 10 | `+0x20` to `+0x28` | unused | V (no reader) | | always -1 |
| 11 | `+0x2c` | armor factor, fire and explosion | V | `0x45ff00*` (slot `+0x108`, shared with Truck) | 256 |
| 12 | `+0x30` | armor factor, physical | V | `0x45ff10*` | 256 |
| 13 | `+0x34` | armor factor, third | G | `0x45ff20*`, accessor only | 256 |
| 14 | `+0x38` | spawn sound list id | V | `0x416390*` (slot `+0xc4`), fragments `0x416499` to `0x416612` | 14 |
| 15 | `+0x3c` | unit name string pointer | V | `0x4166e0` | this is the `0x4166e0` read that `descriptor_marine.md` could not place: it is the Cannon name accessor, not a Marine field |

The Cannon has no speed word. Death handler `0x416b90` uses fixed lists 0x96, 0x98, 0x71.

## Headtruck, table `0x4ab2b0`, 20 words, pointer at object `+0x6c`

| Word | Offset | Name | V/G | Evidence | Notes |
| --- | --- | --- | --- | --- | --- |
| 0 | `+0x00` | body animation set id | V | `0x42bda0`, `0x42bf70` | 14 or 17 (two chassis) |
| 1 | `+0x04` | head variant index | V | `0x42c3a0` (slot `+0x80`) | 0 to 4. Selects a group of 4 entries in body animation record 0x27 (`index + variant * 4`); meaning of the record (head mounting or muzzle point) is a guess |
| 2 | `+0x08` | head (turret) animation set id | V | `0x42bda0`, `0x42bf70` | 50 to 54, always 50 + word 1 |
| 3 | `+0x0c` | initial and maximum hit points | V | `0x42bda0`, `0x44a0f0*` (slot `+0x3c`), `0x42c880` | named |
| 4 | `+0x10` | regeneration per tick | V | `0x42c880` | 12 |
| 5 | `+0x14` | regeneration cap | V | `0x42c880` | 3500 to 5000 |
| 6 | `+0x18` | secondary hit points, initial | V | `0x42bda0` (to object `+0x3c`) | named `hp_max` in `build_rules.py`; the maximum getter reads word 3, so the name should be secondary hit points |
| 7 | `+0x1c` | speed, px per tick | V | `0x42c880` | named |
| 8 | `+0x20` | sight mask id | V | `0x42bc50*` | named |
| 9 | `+0x24` | weapon id | V | `0x42c3c0` (slot `+0xa8`) | was a guess, now verified |
| 10 to 12 | `+0x28` to `+0x30` | unused | V (no reader) | | always -1 |
| 13 | `+0x34` | head turn rate, heading units per tick | V | `0x42c880` | 8 or 16. Head heading (object `+0x7c`) turns toward the target or back to the body heading. Also: while reloading, a new target is requested once the reload counter is below `(256 / rate) / 2` |
| 14 | `+0x38` | armor factor, fire and explosion | V | `0x44a170*` (slot `+0x108`) | 256 for type 0, 128 for the rest (half fire damage). Was a guess, now verified |
| 15 | `+0x3c` | armor factor, physical | V | `0x44a180*` (slot `+0x10c`) | 256. Was a guess, now verified |
| 16 | `+0x40` | armor factor, third | G | `0x44a190*`, accessor only | 256 |
| 17 | `+0x44` | spawn sound list id | V | `0x42c1d0*` (slot `+0xc4`), fragment `0x42c2fc` | 8 |
| 18 | `+0x48` | engine (acceleration) sound list id | V | `0x42c880` | 13, cooldown counter in object `+0xac` |
| 19 | `+0x4c` | unit name string pointer | V | `0x42c400` | |

## Spider, table `0x4ae378`, 19 words, pointer at object `+0x6c`

| Word | Offset | Name | V/G | Evidence | Notes |
| --- | --- | --- | --- | --- | --- |
| 0 | `+0x00` | body animation set id | V | `0x44a210`, `0x44a350` | 22 |
| 1 | `+0x04` | head variant index | V | `0x44ab40` (slot `+0x80`) | 0, 4; same use as Headtruck word 1 |
| 2 | `+0x08` | head (turret) animation set id | V | `0x44a210`, `0x44a350` | 50, 54 (shares the Headtruck head sets) |
| 3 | `+0x0c` | initial and maximum hit points | V | `0x44a210`, `0x44a0f0*`, `0x44b360` | named |
| 4 | `+0x10` | regeneration per tick | V | `0x44b360` | 15, 12 |
| 5 | `+0x14` | regeneration cap | V | `0x44b360` | 10000, 7000 |
| 6 | `+0x18` | secondary hit points, initial | V | `0x44a210`, `0x44a350` (to object `+0x3c`) | named `hp_max`, same remark as Headtruck |
| 7 | `+0x1c` | speed, px per tick | V | `0x44b360`, `0x44aab0` | named. Applied directly, no acceleration ramp |
| 8 | `+0x20` | sight mask id | V | `0x42bc50*` (shared with Headtruck) | named |
| 9 | `+0x24` | weapon id | V | `0x42c3c0` (Spider vtable slot `+0xa8` points to the Headtruck function) | was a guess, now verified. 1, 34 |
| 10 to 12 | `+0x28` to `+0x30` | unused | V (no reader) | | always -1 |
| 13 | `+0x34` | head turn rate, heading units per tick | V | `0x44b360` | 4, 8. Head heading byte at object `+0x92`; same retarget rule as Headtruck |
| 14 | `+0x38` | armor factor, fire and explosion | V | `0x44a170*` | 256 |
| 15 | `+0x3c` | armor factor, physical | V | `0x44a180*` | 256 |
| 16 | `+0x40` | armor factor, third | G | `0x44a190*`, accessor only | 256 |
| 17 | `+0x44` | spawn sound list id | V | `0x44a780*`, fragments `0x44a8af` to `0x44aa15` | 14 |
| 18 | `+0x48` | unit name string pointer | V | `0x44ab60` | |

## Summary

| Kind | Newly named V | Newly named G | Guess upgraded to V | Unused words |
| --- | --- | --- | --- | --- |
| Bird | 15 (0, 2 to 7, 9, 13 to 15, 17, 19 to 21) | 1 (16) | | 10 to 12; 18 is read but zero in all records |
| Zeppelin | 10 (0, 2, 3, 4, 6, 8, 12, 13, 15, 16) | 1 (14) | | 9 to 11 |
| Truck | 11 (0 to 4, 7, 11, 12, 14 to 16) | 1 (13) | word 1 replaces the guessed `hp_initial` at word 4 | 8 to 10 |
| Cannon | 11 (0 to 5, 7, 11, 12, 14, 15) | 1 (13) | | 8 to 10 |
| Headtruck | 9 (0, 1, 2, 4, 5, 13, 17, 18, 19) | 1 (16) | 9, 14, 15 | 10 to 12 |
| Spider | 10 (0, 1, 2, 4, 5, 13, 14, 15, 17, 18) | 1 (16) | 9 | 10 to 12 |
| Total | 66 | 6 | 4 plus the Truck hit point fix | |

Open points:

- The third armor word of every kind (slot `+0x110`): no consumer found in the corpus.
- The exact content of animation record 0x27 selected by the head variant index.
- `descriptor_marine.md` assigns `0x45ff10` and `0x45ff20` to slots `+0x108` and `+0x10c`. In
  the Truck and Cannon vtables those functions sit at `+0x10c` and `+0x110` (`0x45ff00` is at
  `+0x108`). The Marine vtable was not re-read here; worth one check, since it decides which
  Marine word is the physical factor.
- No capacity or cost word exists in any of these six records (price and build time live in
  the type table, transport capacity of the Zeppelin is not a descriptor word).
