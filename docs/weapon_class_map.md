# Weapon class map of the original (decompilation survey)

Evidence date: 2026-11-08. **V** = read from the Ghidra decompilation of the original
(function addresses only; no code is reproduced here). **O** = not read yet.
Method: weapon ids are mapped through the table at `0x494d30` to a class and a parameter;
`0x470780` fills the prototype table at `0x515ac0` by calling the weapon factory
`0x46d900`; each weapon class is a vtable whose slots are `+0x5c` damage against a target,
`+0x60` reload, `+0x74` range, `+0x78` can-fire and `+0x7c` hit effect. Every weapon hit
ends in the target's virtual `+0x124` (Marine: `0x4336b0`), which calls the generic damage
routine `0x43ea30`.

## Vtables found with the weapon factory as a constructor

| Vtable | Damage `+0x5c` | Effect `+0x7c` | Weapon classes | State |
| --- | --- | --- | --- | --- |
| `0x494f10` | `0x46dfe0` | `0x46ed90` | 2 (footman, rifleman, bear, bizon, chief, watch tower, Abdula) | V formula; the effect `0x46ed90` is O |
| `0x495130` | `0x46dfe0` | `0x46e0d0` | 13 (Hypno, hypno tower) | V: direct damage through `+0x124`, optional damage-over-time helper only when the record carries a duration (not for Hypno); no side change |
| `0x494f98` | `0x46ec90` | `0x46ed00` | 10 (Jaguar) | V: 1800 base, 1125 against non-organic or structure, 1.25 for two category flags, zero against air; the effect deals the damage and then calls the attacker's virtual `+0x104` with damage divided by 3 (likely a heal or recoil, O) |
| `0x495020` | `0x46ead0` | `0x46eb10` | 6 (Snowman) | V: 18000, 9000 against non-organic, zero against air; the effect deals the damage and then applies a status through virtual `+0x9c` with value `0x12` to every non-allied, non-airborne object of the two Marine categories within about 96 px of the target (an area freeze or slow, status meaning O) |
| `0x4950a8` | `0x46e580` | `0x46e8b0` | 4 and 5 (Knight, Ninja) | formula V (3000, 1500, reload 19, range 72); effect O |
| `0x494430` | `0x44c8d0` | `0x44ca60` | 7 (Demolisher and the distance based weapons ids 3, 17, 18, 25, 29, 37, 38, 41, 42, 49, 53) | O: "unread (distance based)" |
| `0x48f63c` | base | base | arrow family (class 1) and projectile objects | formula V; secondary weapon rec[12] O |

The remaining classes (0, 3, 8, 9, 11, 12, 14) are created through other constructors
(for example `0x44c010`, `0x444080`, `0x41e350`, `0x42e180`, `0x429c90`); their vtables are
not yet tied to the class ids.

## Unresolved weapons by unit type (from docs/data/rules.json)

Weapon-bearing types whose weapon class the rules data still marks as guess or unread:

| Type | Weapon id | Class | Missions placing it |
| --- | --- | --- | --- |
| Mage (28) | 24 | 3 (guess, damage 500, reload 35, range 250) | all 15 |
| Aztec crossbow tower (49) | 28 | 3 (guess) | 4, 8 to 11, 13 to 15 |
| Demolisher (2) | 38 | 7 (unread) | 4, 11, 12, 14 |
| Spider flamethrower (35), wheel firethrower (59) | 34 | 11 (guess, burn per tick) | 12, 14, 15 |

Types with no weapon id in the descriptor table (the weapon is selected another way):
Zeppelin (7), Zeppelin2 (85), Eagle (33), Deltaplan (34), Paraplan (60), Catapult (83),
Hopcannon (87), Boat (84), Johnka (88), the Chinese firethrow tower (68) and the air
facility and air tower family. Classes 0 (ids 2, 30, 32, 40, 43, 46, 50), 9 (ids 21, 23,
27, 45) and 14 (id 0) are guesses with no unit mapped yet.

## How to resolve a class

1. Take the weapon id of the unit (descriptor `weapon_id`) and read its class and
   parameter from `docs/data/rules.json` (`weapons`).
2. Find the vtable with `work/classes.json` (constructors list the factory `0x46d900`) and
   read slots `+0x5c`, `+0x60`, `+0x74`, `+0x78`, `+0x7c`.
3. Decompile those functions (`tools/ghidra_scripts/DecompAddrs.java` through
   `analyzeHeadless` on the project in `work/path/gproj`; `work/decomp/all.c` holds the
   functions that were already decompiled).
4. Record, for each hit effect: the damage and its multipliers, any area, any status
   applied to the target (virtual slots such as `+0x9c`), any effect on the attacker
   (`+0x104`), any spawned helper object, and whether the owner or side changes.
5. Check the behaviour in the headless reference runs when a number needs confirming
   (see `hypno_recon.md`).
