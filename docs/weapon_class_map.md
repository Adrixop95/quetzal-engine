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

## Classes read in detail (second pass)

Evidence date: 2026-11-08. **V** = read from the decompilation. Class-to-vtable links come
from the factory `0x46d900` (case number to constructor) and are labelled **G** where the
case number could not be tied to the constructor with certainty. `rec[n]` is word n of the
weapon record in `docs/data/rules.json` (`record_raw`).

| Class (G for the link) | Vtable | Constructor | Damage `+0x5c` | Hit effect `+0x7c` | What it is (V) |
| --- | --- | --- | --- | --- | --- |
| 0 | `0x48f8f4` | `0x406700` | `0x4068e0` | `0x4069c0` | Ballistic carrier projectile. Its damage is delegated to the weapon whose id is in `rec[4]` (a second weapon prototype). Launch computes the flight from the start and target points: speed from `rec[1]`, an arc over the flight time, frame from the direction (`0x461ac0` angle), and a spawn height offset of two pixels. Range method `0x406960` divides the distance by `rec[1]`. Weapons 2, 30, 32, 40, 43, 46, 50 |
| 1 | `0x48f558` | `0x403b60` | `0x403d00` | `0x403f40` | Arrow family; formula already in the rules notes (start `rec[4]`, multipliers `rec[7..11]`, armor `rec[5]`, secondary `rec[12]`) |
| 3 | `0x48f7e0` | `0x404c80` | `0x404b90` | `0x404f70` | Straight projectile. Damage: zero against airborne targets and against allied objects (same-side test `0x4472e0` on the side list); otherwise `rec[4]` (0.6875 of it against non-organic targets, `0xb0` out of 256) multiplied by `48 / rec[1]` (the projectile speed), plus the damage of the secondary weapon whose id is in `rec[5]` when present. Range method `0x404c30` is the distance divided by `rec[1]` (flight time). Launch `0x404f70` clamps the aim point to the weapon range, sets the angle and direction frame, and the projectile moves by `rec[1]` per tick. Multi-target scan `+0x64` (`0x404e10`) lists objects in a box and applies the same damage rule to each. Weapons 5, 6, 22, 24 (Mage), 26, 28 (Aztec crossbow tower) |
| 7 | `0x4943a4` | `0x44c010` | `0x44c100` | `0x44c3c0` | Distance based area explosion. `0x44c130`: applies only to targets whose airborne flag equals the flag in `rec[5]`; with distance d between the objects, full damage `rec[4]` inside `rec[1]`, linear falloff between `rec[1]` and `rec[2]`, zero beyond it; 0.8 (`0xcc` of 256) of the result for targets with the `+0xe8` category, then scaled by the target's armor factor from virtual `+0x108`. Weapons 3, 17, 18, 25, 29, 37, 38 (Demolisher), 41, 42, 49, 53 |
| 9 | `0x493fe0` | `0x444080` | `0x444160` | none (inherits) | Direct damage with multipliers: base `rec[0]`, then `rec[3]` against airborne, `rec[4]` non-organic, `rec[5]` for `+0xf4`, `rec[6]` for `+0xe8`, `rec[7]` structure (each applied as value times current divided by 256), plus `rec[1]` times the target's armor value (`+0x10c`) divided by 256. Weapons 21, 23, 27, 45 |
| 11 | `0x4927a8` | `0x429c90` | `0x429c00` | `0x42a010` | Burn. Damage per application is 10 times `0x429f60`: zero for the object itself or an airborne target, otherwise 2000 (`0x7d000` / 256 scale) reduced to one quarter (`0x40` of 256) when the target belongs to an allied side, then scaled by the target's fire armor factor (virtual `+0x108`). Weapon 34 (spider flamethrower, wheel firethrower). Effect `0x42a010` and the helper object it creates are still to be read |
| 12 | `0x490738` | `0x41e350` | `0x41e3c0` | `0x41e4a0` | Sum of two weapons: the damage is the sum of the damages of the weapons with ids `rec[0]` and `rec[1]`. Weapons 7, 35, 39, 44, 47, 51 (axe thrower, cannon, elefant and the towers that use them) |
| 2 | `0x494f10` | (factory) | `0x46dfe0` | `0x46ed90` | Direct damage; effect still to be read |
| 4, 5 | `0x4950a8` | (factory) | `0x46e580` | `0x46e8b0` | Knight and Ninja melee: 3000, 1500 against non-organic or structure, reload 19, range 72; effect to be read |
| 6 | `0x495020` | (factory) | `0x46ead0` | `0x46eb10` | Snowman: 18000, 9000 against non-organic, zero against air; the effect also applies status `0x12` through virtual `+0x9c` to non-allied, non-airborne objects of the two Marine categories within about 96 px |
| 10 | `0x494f98` | (factory) | `0x46ec90` | `0x46ed00` | Jaguar: 1800, 1125 against non-organic or structure, 1.25 for two category flags, zero against air; the effect calls the attacker's virtual `+0x104` with damage divided by 3 |
| 13 | `0x495130` | (factory) | `0x46dfe0` | `0x46e0d0` | Hypno and hypno tower: direct damage, no side change (see `hypno_recon.md`) |

Findings that change what the clone must do:

- Classes 3, 0 and 7 are not independent: projectile classes delegate or add the damage of a
  second weapon prototype (`rec[4]` or `rec[5]`), and explosions (class 7) are those second
  weapons. A faithful implementation therefore needs a weapon graph, not a flat table.
- Damage against airborne targets is zero for classes 3, 6, 10 and 4/5 and for the burn, and
  class 9 and 2/13 carry an explicit airborne multiplier (zero in the rules data for the
  checked weapons). Anti-air in the original comes from weapons that set the airborne flag in
  class 7 (`rec[5]`) and from the Bird, Zeppelin and tower weapons listed in the next task.
- Allied fire is suppressed by a side-list test (`0x4472e0`) in classes 3 and 11 (burn is
  reduced to one quarter instead of zero).
- Hit effects of classes 2, 4/5, 6, 10 and 11, and the helper objects they spawn, are still
  to be read; they are named in `tasks/queue/140` and `141`.

## Weapon graph of the unresolved units (third pass)

Evidence date: 2026-11-08. **V** = read from the decompilation or from the descriptor
and weapon record tables in `docs/data/rules.json` (already in the repository). Weapon
record layouts by class (V from the methods above):

- class 12 (sum): `rec[0]` and `rec[1]` are the ids of the two component weapons,
  `rec[2]` the reload; the damage is the sum of both components (the same id twice is a
  double shot).
- class 0 (ballistic carrier): `rec[1]` flight parameter, `rec[2]` speed per tick,
  `rec[3]` range, `rec[4]` id of the impact weapon that supplies the damage.
- class 3 (straight projectile): `rec[1]` speed per tick, `rec[2]` reload, `rec[3]` range,
  `rec[4]` damage factor, `rec[5]` id of the impact weapon.
- class 7 (explosion): `rec[1]` inner radius, `rec[2]` outer radius, `rec[4]` damage,
  `rec[5]` required airborne flag of the target (0 means ground targets only).
- class 9 and class 2 and 13 keep the direct-damage layouts described above.

| Unit (type) | How the weapon is found (V) | Weapon chain |
| --- | --- | --- |
| Mage (28) | Marine descriptor weapon id 24 | 24 (class 3, speed 20, reload 20, range 200, factor 600) then impact 25 (class 7, inner radius 20, outer radius 35, damage 2000, ground only) |
| Aztec crossbow tower (49) | Defence descriptor weapon id 28 | 28 (class 3, speed 20, reload 20, range 225, factor 800) then impact 29 (class 7, radii 20 and 50, damage 2700, ground only) |
| Axe thrower trucks and wheels (3, 61), Rus fortress tower (23) | weapon id 7 | 7 (class 12, reload 25) = 5 + 6 (class 3, speed 25, reload 35, range 250, factor 500) then impact 21 (class 9, damage 2000 with armor component) |
| Cannon truck (4), Rus cannon tower (26) | weapon ids 39 and 51 | 39 (class 12, reload 25) = 40 + 40 (class 0, speed 30, range 290) then impact 41 (class 7, radii 35 and 70, damage 4000, air 0) |
| Elefant (57) | weapon id 47 | 47 (class 12, reload 15) = 46 + 46 (class 0, range 230) then impact 45 (class 9, damage 3800) |
| Catapult (83) | Truck descriptor field d7 = 32 | 32 (class 0, speed 30, range 200) then impact 3 (class 7, radii 24 and 72, damage 3000) |
| Hopcannon (87) | Cannon descriptor field d7 = 2 | 2 (class 0, range 250) then impact 3 (as the catapult) |
| Deltaplan (34) | Bird descriptor field d9 = 35 | 35 (class 12, reload 25) = 36 + 36 (class 1 arrow, speed 15, reload 30, range 140) |
| Paraplan (60) | Bird descriptor field d9 = 52 | 52 (class 2 direct, damage 2600, range 200, no effect on structures or non-organic) |
| Zeppelin2 (85) | Zeppelin descriptor field d8 = 44 | 44 (class 12, reload 15) = 43 + 43 (class 0, speed 48, range 30) then impact 42 (class 7, radii 80 and 120, damage 3000): a bomb |
| Eagle (33), Zeppelin (7) | descriptor weapon field is -1 | no weapon in the descriptor; their behaviour (scout, carrier or something else) is still unknown |
| Demolisher (2) | Marine weapon id 38 | class 7 explosion used directly (damage falls off with distance) |
| Spider and wheel flamethrowers (35, 59) | weapon id 34 | class 11 burn (see above); has no weapon record |

Corrections to the open list from `gap_recon.md`:

- Mage and the Aztec crossbow tower are not a plain 500 damage shot: they combine the
  class 3 hit (factor times 48 divided by speed, integer division) with a class 7 impact
  that depends on distance. Their damage in the clone must come from this chain.
- Catapult, Hopcannon, cannons, bombs and the elefant all work through class 0 carriers that
  hand the damage to a class 7 or class 9 impact weapon, so one implementation of the weapon
  graph covers all of them.
- Only the Eagle, the first Zeppelin, Johnka, the boat and the firethrow tower family
  have no resolved weapon.
