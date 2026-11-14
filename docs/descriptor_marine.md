# Marine descriptor words (partial result for task 144)

Evidence date: 2026-11-14. **V** = read from the Ghidra decompilation (function addresses
only), **G** = guess. The Marine descriptor is the 19 word record at `0x4ac430` (16 records
plus the shared records of the Marine kind), reached through object field `+0x68`
(`param_1[0x1a]` in the decompiled code).

| Word | Offset | Name | Source | Notes |
| --- | --- | --- | --- | --- |
| 0 | `+0x00` | animation set id | V (`0x433190`, existing table) | already named |
| 1 | `+0x04` | initial and maximum hit points | V (`0x433190`, `0x45fe50`) | already named |
| 2 | `+0x08` | regeneration per tick | V | already named |
| 3 | `+0x0c` | regeneration cap (alternate) | V | already named |
| 4 | `+0x10` | secondary hit points, initial | V (`0x433190`, kill-time `0x46fac0`) | already named |
| 5 | `+0x14` | speed in px per tick | V (`0x415e70`, `0x415f50`, `0x433820`, `0x433886`) | already named |
| 6 | `+0x18` | sight mask id | V (`0x4045bb`, `0x4045c3`) | already named |
| 7 | `+0x1c` | weapon id | V (`0x4166a0`) | already named |
| 8 to 10 | `+0x20` to `+0x28` | unused (always -1) | V (never read by matched code) | |
| 11 | `+0x2c` | class flag bytes: byte 0 (`+0x2c`), byte 1 (`+0x2d`, soldier flag, selects the "Soldier" or "Animal" name string in `0x4337e0`) and byte 2 (`+0x2e`, Hypno predicate for virtual `+0xf8`) | V (`0x432fa0`, `0x4337e0`, `0x433000`) | |
| 12 | `+0x30` | armor value returned by virtual `+0x108` (fire) | V (`0x45ff10`) | already named |
| 13 | `+0x34` | armor value returned by virtual `+0x10c` (physical) | V (`0x45ff20`) | already named |
| 14 | `+0x38` | armor value (third), named `armor_d14` | G | not read at a fixed offset by Marine code; kept as guess |
| 15 | `+0x3c` | unknown small integer (0, 2 to 7) | G | read only by `0x4166e0` which belongs to another class (name accessor of the vehicle family), so not a Marine field in the code found; likely unused |
| 16 | `+0x40` | **hit sound list id** (always 0 for Marines: no hit sound) | V (`0x4336b0` reads it through `param_1[0x1a]`, picks a random entry of the list) | |
| 17 | `+0x44` | **death sound list id** (0 or 1) | V (`0x433960`, the Marine death handler, loads the list and plays a random entry) | |
| 18 | `+0x48` | **unit name string pointer** (guest address) | V (`0x4337e0`, name accessor for index 0) | pointer, not data |

Result: of the 15 Marine words that are non-zero in some type, 14 now have a name or a
verified use; word 15 stays a guess. The kind is therefore at about 93 percent, up from 13
of 15.
