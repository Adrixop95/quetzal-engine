# Building and Defence descriptor words

Evidence date: 2026-11-14. **V** = read in the Ghidra decompilation (function addresses given),
**G** = guess. Sources: `work/decomp/all_full.c` plus `work/gaps/dec_410010.c` (31 functions
decompiled for this task, including the virtual methods missing from the corpus). Virtual slot
numbers come from the vtables read out of `game/Aztec.exe`: Building `0x48fd20`, Defence
`0x4905bc`.

Not in either record: cost, build time, prerequisites and production lists. No code reads such a
word from these descriptors; they live in the BuildTree table `0x4af130` (`0x411890`,
`0x411ac0`, see `docs/rules_types.md`). The Building record is: header, 7x7 mask, sight, three
armor words, flags, the income block and a 255 entry schedule array.

## Building (table `0x49b538`, 0x508 bytes, object field `+0x64`)

| Word | Offset | Name | V/G | Evidence | Notes |
| --- | --- | --- | --- | --- | --- |
| 0 | `+0x00` | sprite resource id | V | `0x40fb70`, `0x40fdf0` | passed to the sprite loader `0x4030c0`; equals the type id 200..415 |
| 1 | `+0x04` | maximum hit points | V | `0x410010` (set on completion), `0x40fa50` (heal clamp, virtual `+0xd8`) | constructor starts HP at 0 |
| 2 | `+0x08` | hit rectangle x1 (pixels, relative to tile centre) | V | `0x411330` (virtual `+0x1c`) | |
| 3 | `+0x0c` | hit rectangle y1 | V (partial) | `0x411330` | the read is lost to a register in the decompilation; implied by the min/max pairing with word 5 |
| 4 | `+0x10` | hit rectangle x2 | V | `0x411330` | |
| 5 | `+0x14` | hit rectangle y2 | V | `0x411330` | |
| 6 to 54 | `+0x18` to `+0xd8` | 7x7 footprint mask, row major, centre word 30 (`+0x78`) | V | `0x411230` (occupy), `0x4104b0` (release), `0x409940` (placement test), `0x42acd0` and `0x410010` (terrain income), `0x45bd50`, `0x409ce0`, `0x409e60` (pass `desc+0x18`) | every reader tests `== 2` only. Data words 15 to 18, 21 to 40, 42 to 46 are this mask |
| (mask value 1) | | margin cell | G | no reader distinguishes 1 from 0 | `0x409940` treats only 2 as required cells; value 1 has no effect in the code found |
| 55 | `+0xdc` | sight (light mask) id | V | `0x40fa30` (virtual `+0x74`) | already named |
| 56 | `+0xe0` | fire/area damage factor, 8.8 fixed (256 = 1.0) | V | `0x40fa90` (virtual `+0x108`); consumers `0x44c130`, `0x429f60` | 0 = immune (all town halls, some others), 128 to 512. New |
| 57 | `+0xe4` | physical armor value | V | `0x40fab0` (virtual `+0x10c`) | 0 in every record |
| 58 | `+0xe8` | third armor value | V | `0x40fad0` (virtual `+0x110`) | 0 in every record |
| 59 byte 0 | `+0xec` | indestructible flag | V | `0x410560` (virtual `+0x124`, damage is dropped when set), `0x40f9b0` (virtual `+0x38`, reported HP is 0 unless object state `+0x68` is 1) | set for records 0, 1, 2, 10, 12 to 17, 38 (the hall records that also have word 60 and period 300 or 150). New |
| 59 byte 1 | `+0xed` | income depends on terrain | V | `0x410010` | set for records 7, 8, 20, 22, 35, 36 (mines and farms) |
| 59 bytes 2, 3 | `+0xee` | unused | V (never read) | | always 0 |
| 60 | `+0xf0` | gold rate: constant, or channel A coefficient | V | `0x410010`, `0x410580`, `0x45bd50`, `0x409ce0` | already named |
| 61 | `+0xf4` | channel B coefficient | V | `0x410010`, `0x410580`, `0x45bd50`, `0x409ce0` | already named |
| 62 | `+0xf8` | repair credit rate | V | `0x40fb70` | 0 means no repair accumulator is created |
| 63 byte 0 | `+0xfc` | gold uses the explicit day schedule | V | `0x410010` | records 7, 20, 35 |
| 64 | `+0x100` | period in ticks, or number of schedule entries when word 63 is set; -1 = no income | V | `0x410010`, `0x410580`, `0x45ea00` | 6, 10, 8 for the scheduled records |
| 65 to 319 | `+0x104` to `+0x4fc` | day schedule: clock values (0..7199) of payout events, word 64 entries | V | `0x410010` passes `desc+0x104` and the count to `0x45f6e0`, which copies that many dwords | capacity 255; used: 65 to 70 (record 7), 65 to 74 (record 20), 65 to 72 (record 35, first entry 0). All other words 0, unused. New |
| 320 | `+0x500` | repair credit period (ticks) | V | `0x40fb70` | already named |
| 321 | `+0x504` | name string pointer | V | `0x410620` (virtual `+0x84`, index 0; index 1 gives "Building") | pointer, not data |

Unused Building words: 57, 58 (read by getters but always 0), bytes `+0xee`/`+0xef`, `+0xfd`
to `+0xff`, and the schedule tail (75 to 319, and 65 to 74 outside the three scheduled records).

## Defence (table `0x4aa3b0`, 0x110 bytes, object field `+0x6c`)

Records are filled by the static initialiser `0x41b9d0`.

| Word | Offset | Name | V/G | Evidence | Notes |
| --- | --- | --- | --- | --- | --- |
| 0 | `+0x00` | base sprite resource id | V | `0x41cce0`, `0x41cf00` | 214, 416 are shared by two records each |
| 1 | `+0x04` | sprite variant index | V | `0x41d300` (virtual `+0x80`: section 7 entry `index + word1*4`) | 1 for records 0 and 7, which share a sprite file with records 1 and 2/8. New |
| 2 | `+0x08` | turret (second layer) sprite resource id, 0 = none | V | `0x41cce0`, `0x41cf00` | 51, 52, 55. New |
| 3 | `+0x0c` | maximum hit points | V | `0x41d1a0`, `0x41cf00`, `0x41cbe0` (heal clamp) | |
| 4 | `+0x10` | turret turn step per tick, in 1/256 of a circle | V | `0x41d790` | also `256/word4` = number of facings (32, 8, 2); the tower fires once the heading error is below one full step (half of `256/word4` is only the early re-acquire window during reload, see `docs/towers_air_specials.md`). New |
| 5 | `+0x14` | hit rectangle x1 | V | `0x41e220` (virtual `+0x1c`) | |
| 6 | `+0x18` | hit rectangle y1 | V (partial) | `0x41e220` | read lost to a register, as in Building word 3 |
| 7 | `+0x1c` | hit rectangle x2 | V | `0x41e220` | |
| 8 | `+0x20` | hit rectangle y2 | V | `0x41e220` | |
| 9 to 57 | `+0x24` to `+0xe4` | 7x7 footprint mask, centre word 33 (`+0x84`) | V | `0x41e120` (occupy), `0x41d240` (release), `0x40a080` and `0x45bd50` (pass `desc+0x24` to `0x409940`) | only value 2 is tested; value 1 as for Building (G) |
| 58 | `+0xe8` | sight (light mask) id | V | `0x41cb80` (virtual `+0x74`) | already named |
| 59 | `+0xec` | weapon id, -1 = unarmed | V | `0x41d320` (virtual `+0xa8`, indexes weapon table `0x515ac0`) | already named |
| 60 to 62 | `+0xf0` to `+0xf8` | unused, always -1 | V (never read) / G for meaning | | probably spare weapon slots |
| 63 | `+0xfc` | projectile launch height (pixels) | V read, G meaning | `0x41d790` (passed when the projectile is positioned) | 0 for the two unarmed records, 36 to 68 otherwise. New |
| 64 | `+0x100` | fire/area damage factor, 8.8 fixed | V | `0x41cc00` (virtual `+0x108`) | 128, 256, 512. New |
| 65 | `+0x104` | physical armor value | V | `0x41cc20` (virtual `+0x10c`) | 0 in every record |
| 66 | `+0x108` | third armor value | V | `0x41cc40` (virtual `+0x110`) | 0 in every record |
| 67 | `+0x10c` | name string pointer | V | `0x41d360` (virtual `+0x84`; index 1 gives "Defence building"), `0x41b9d0` | pointer, not data |

Unused Defence words: 60 to 62 (constant -1), 65, 66 (always 0).

## Totals

- Building: all 55 data words have a verified reader. 54 V plus word 3 (V by pairing). The
  only guess is the meaning of mask value 1.
- Defence: of 39 data words, 35 V; words 60 to 62 are unread (meaning G) and word 63 is read
  but its meaning is G.
