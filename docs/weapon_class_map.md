# Weapon class map of the original (decompilation survey)

Latest scoped audit: [Fire, air and siege](fire_air_siege.md) (2026-11-21).
Its corrections supersede the historical passes below. **G** = clone approximation.

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
| `0x4950a8` | `0x46e580` | `0x46e8b0` | 4 and 5 (Knight, Ninja) | formula V (3000, 1500, reload 19, range 72); effect V (sweep, see below) |
| `0x494430` | `0x44c8d0` | `0x44ca60` | 7 (Demolisher and the distance based weapons ids 3, 17, 18, 25, 29, 37, 38, 41, 42, 49, 53) | O: "unread (distance based)" |
| `0x495130` | `0x46dfe0` | `0x46e0d0` | 2 (footman, rifleman, bear, bizon, chief, watch tower, Abdula) | V: direct base/category damage, optional armor helper; see ground_weapons.md |
| `0x494f10` | `0x46dfe0` | `0x46ed90` | 13 (Hypno, hypno tower) | V: direct damage, optional armor helper and cosmetic trail; current armor coefficients are zero; no side change |
| `0x494f98` | `0x46ec90` | `0x46ed00` | 10 (Jaguar) | V: 1800 base, 1125 against non-organic or structure, 1.25 for two category flags, zero against air; the effect deals the damage and then calls the attacker's virtual `+0x104` with damage divided by 3 (V: primary HP heal capped at maximum) |
| `0x495020` | `0x46ead0` | `0x46eb10` | 6 (Snowman) | V: 18000, 9000 against non-organic, zero against air; the effect deals the damage and then applies a status through virtual `+0x9c` with value `0x12` to every non-allied, non-airborne object of the two Marine categories whose hit rectangles overlap a 96 px square around the attacker (V: add 18 attack cooldown ticks) |
| `0x4950a8` | `0x46e580` | `0x46e8b0` | 4 and 5 (Knight, Ninja) | V: 3000/1500, reload 19, range 72; central full-damage sector and two half-damage flanks, sweep radius 96 |
| `0x4943a4` | `0x44c100` / `0x44c130` | `0x44c3c0` | 7 (Demolisher and explosion ids 3, 17, 18, 25, 29, 37, 38, 41, 42, 49, 53) | V: full inner damage, 8.8 falloff and armor outside; immediate area effect |
| `0x494430` | `0x44c8d0` | `0x44ca60` | 8 (volley) | V: separate scatter scheduler; task 141 |
| `0x4950a8` | `0x46e580` | `0x46e8b0` | 4 and 5 (Knight, Ninja) | formula V (3000, 1500, reload 19, range 72); effect O |
| `0x494430` | `0x44c8d0` | `0x44ca60` | 8 (Katusha, air tower, Johnka) | V: sixteen delayed explosions; corrected class link in the scoped audit |
| `0x48f63c` | base | base | arrow family (class 1) and projectile objects | formula V; secondary weapon rec[12] O |

Other factory links are recorded below; the final ground-class table and
[ground_weapons.md](ground_weapons.md) supersede the early survey labels.

## Historical unresolved list (before the ground-class implementation)

This list records the original investigation scope. The ground-class rows are now
resolved; the burn family remains separate task 141 work:

| Type | Weapon id | Class | Missions placing it |
| --- | --- | --- | --- |
| Mage (28) | 24 | 3 (guess, damage 500, reload 35, range 250) | all 15 |
| Aztec crossbow tower (49) | 28 | 3 (guess) | 4, 8 to 11, 13 to 15 |
| Demolisher (2) | 38 | 7 (unread) | 4, 11, 12, 14 |
| Spider flamethrower (35), wheel firethrower (59) | 34 | 11 (V, active flame updates; scoped audit below) | 12, 14, 15 |

Types with no weapon id in the descriptor table (the weapon is selected another way):
Zeppelin (7), Zeppelin2 (85), Eagle (33), Deltaplan (34), Paraplan (60), Catapult (83),
Hopcannon (87), Boat (84), Johnka (88), the Chinese firethrow tower (68) and the air
facility and air tower family. The historical open list also included classes 0 (ids 2, 30, 32, 40, 43, 46, 50), 9 (ids 21, 23,
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
| 0 | `0x48f8f4` | `0x406700` | `0x4068e0` | `0x4069c0` | Ballistic carrier projectile. Its damage is delegated to the weapon whose id is in `rec[4]` (a second weapon prototype). Launch computes the flight from the start and target points: speed from `rec[1]`, an arc over the flight time, frame from the direction (`0x461ac0` angle), and a spawn height offset of two pixels. Flight-time method `0x406960` divides the distance by `rec[1]`; range is `rec[3]`. Weapons 2, 30, 32, 40, 43, 46, 50 |
| 1 | `0x48f558` | `0x403b60` | `0x403d00` | `0x403f40` | Arrow family; formula already in the rules notes (start `rec[4]`, multipliers `rec[7..11]`, armor `rec[5]`, secondary `rec[12]`) |
| 3 | `0x48f7e0` | `0x404c80` | `0x404b90` | `0x404f70` | Straight projectile. Damage: zero against airborne targets and against allied objects (same-side test `0x4472e0` on the side list); otherwise `rec[4]` (0.6875 of it against non-organic targets, `0xb0` out of 256) multiplied by `48 / rec[1]` (the projectile speed), plus the damage of the secondary weapon whose id is in `rec[5]` when present. Flight-time method `0x404c30` is distance divided by `rec[1]`; range is `rec[3]`. Launch `0x404f70` clamps the aim point to the weapon range, sets the angle and direction frame, and the projectile moves by `rec[1]` per tick. Multi-target scan `+0x64` (`0x404e10`) lists objects in a box and applies the same damage rule to each. Weapons 5, 6, 22, 24 (Mage), 26, 28 (Aztec crossbow tower) |
| 7 | `0x4943a4` | `0x44c010` | `0x44c100` | `0x44c3c0` | Distance based area explosion. `0x44c130`: applies only to targets whose airborne flag equals the flag in `rec[5]`; with distance d from the explosion, add the target extent (28) to both radii; full damage `rec[4]` inside the inner radius bypasses armor; 8.8 linear falloff between the radii is scaled by 204/256 for `+0xe8`, then by armor virtual `+0x108`; zero beyond the outer radius. Weapons 3, 17, 18, 25, 29, 37, 38 (Demolisher), 41, 42, 49, 53 |
| 9 | `0x493fe0` | `0x444080` | `0x444160` | none (inherits) | Direct damage with multipliers: base `rec[0]`, then `rec[3]` against airborne, `rec[4]` non-organic, `rec[5]` for `+0xf4`, `rec[6]` for `+0xe8`, `rec[7]` structure (each applied as value times current divided by 256), plus `rec[1]` times the target's armor value (`+0x10c`) divided by 256. Weapons 21, 23, 27, 45 |
| 11 | `0x4927a8` | `0x429c90` | `0x429c00` | `0x42a010` | Flame. Potential damage estimate is 10 times `0x429f60`; each active update applies that function once: zero for the object itself or an airborne target, otherwise 2000 (`0x7d000` / 256 scale) reduced to one quarter (`0x40` of 256) when the target belongs to an allied side, then scaled by the target's fire armor factor (virtual `+0x108`). Weapon 34 (spider flamethrower, wheel firethrower). Effect `0x42a010` starts this weapon itself; `0x42a0b0` follows the caster and scans the flame segment for ten active frames (see scoped audit) |
| 12 | `0x490738` | `0x41e350` | `0x41e3c0` | `0x41e4a0` | Sum of two weapons: the damage is the sum of the damages of the weapons with ids `rec[0]` and `rec[1]`. Weapons 7, 35, 39, 44, 47, 51 (axe thrower, cannon, elefant and the towers that use them) |
| 2 | `0x495130` | (factory) | `0x46dfe0` | `0x46e0d0` | Direct base/category hit plus optional armor helper; see ground_weapons.md |
| 4, 5 | `0x4950a8` | (factory) | `0x46e580` | `0x46e8b0` | Knight and Ninja melee: 3000, 1500 against non-organic or structure, reload 19, range 72; full central sweep and half-damage flanks |
| 6 | `0x495020` | (factory) | `0x46ead0` | `0x46eb10` | Snowman: 18000, 9000 against non-organic, zero against air; the effect adds 18 attack cooldown ticks through virtual `+0x9c` to non-allied ground Marine categories overlapping a 96 px square around the attacker |
| 10 | `0x494f98` | (factory) | `0x46ec90` | `0x46ed00` | Jaguar: 1800, 1125 against non-organic or structure, 1.25 for two category flags, zero against air; the effect calls the attacker's virtual `+0x104` with damage divided by 3 |
| 13 | `0x494f10` | (factory) | `0x46dfe0` | `0x46ed90` | Hypno and hypno tower: direct damage, no side change (see `hypno_recon.md`) |

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
- Ground hit effects are resolved in the final table and ground_weapons.md; class 11
  burn helpers and class 8 scheduling remain task 141 work.

## Weapon graph of the unresolved units (third pass)

Evidence date: 2026-11-08. **V** = read from the decompilation or from the descriptor
and weapon record tables in `docs/data/rules.json` (already in the repository). Weapon
record layouts by class (V from the methods above):

- class 12 (sum): `rec[0]` and `rec[1]` are the ids of the two component weapons,
  `rec[2]` the lateral launch offset; reload and range delegate to the first child
  (`0x41e480`, `0x41e5f0`); damage sums both components (the same id twice is a double shot).
- class 0 (ballistic carrier): `rec[1]` speed per tick, `rec[2]` reload,
  `rec[2]` the perpendicular launch offset; range and reload delegate to the first
  component. The damage is the sum of both components (the same id twice is a double shot).
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
| Axe thrower trucks and wheels (3, 61), Rus fortress tower (23) | weapon id 7 | 7 (class 12, reload 35) = 5 + 6 (class 3, speed 25, reload 35, range 250, factor 500) then impact 21 (class 9, damage 2000 with armor component) |
| Cannon truck (4), Rus cannon tower (26) | weapon ids 39 and 51 | 39 (class 12, reload 45) = 40 + 40 (class 0, speed 30, range 290) then impact 41 (class 7, radii 35 and 70, damage 4000, air 0) |
| Elefant (57) | weapon id 47 | 47 (class 12, reload 40) = 46 + 46 (class 0, range 230) then impact 45 (class 9, damage 3800) |
| Catapult (83) | Truck descriptor field d7 = 32 | 32 (class 0, speed 30, range 200) then impact 3 (class 7, radii 24 and 72, damage 3000) |
| Hopcannon (87) | Cannon descriptor field d7 = 2 | 2 (class 0, range 250) then impact 3 (as the catapult) |
| Deltaplan (34) | Bird descriptor field d9 = 35 | 35 (class 12, reload 30) = 36 + 36 (class 1 arrow, speed 15, reload 30, range 140) |
| Paraplan (60) | Bird descriptor field d9 = 52 | 52 (class 2 direct, damage 2600, range 200, no effect on structures or non-organic) |
| Zeppelin2 (85) | Zeppelin descriptor field d8 = 44 | 44 (class 12, reload 48) = 43 + 43 (class 0, speed 0, reload 48, range 30) then impact 42 (class 7, radii 30 and 80, damage 3000): a bomb |
| Eagle (33), Zeppelin (7) | descriptor weapon field is -1 | no weapon in the descriptor; their behaviour (scout, carrier or something else) is still unknown |
| Axe thrower trucks and wheels (3, 61), Rus fortress tower (23) | weapon id 7 | 7 (class 12, launch offset 25, child reload 35) = 5 + 6 (class 3, speed 25, reload 35, range 250, factor 500) then impact 21 (class 9, damage 2000 with armor component) |
| Cannon truck (4), Rus cannon tower (26) | weapon ids 39 and 51 | 39 (class 12, launch offset 25, child reload 45) = 40 + 40 (class 0, speed 30, range 290) then impact 41 (class 7, radii 35 and 70, damage 4000, air 0) |
| Elefant (57) | weapon id 47 | 47 (class 12, launch offset 15, child reload 40) = 46 + 46 (class 0, range 230) then impact 45 (class 9, damage 3800) |
| Catapult (83) | Truck descriptor field d7 = 32 | 32 (class 0, speed 30, range 200) then impact 3 (class 7, radii 24 and 72, damage 3000) |
| Hopcannon (87) | Cannon descriptor field d7 = 2 | 2 (class 0, range 250) then impact 3 (as the catapult) |
| Deltaplan (34) | Bird descriptor field d9 = 35 | 35 (class 12, launch offset 25, child reload 30) = 36 + 36 (class 1 arrow, speed 15, reload 30, range 140) |
| Paraplan (60) | Bird descriptor field d9 = 52 | 52 (class 2 direct, damage 2600, range 200, no effect on structures or non-organic) |
| Zeppelin2 (85) | Zeppelin descriptor field d8 = 44 | 44 (class 12, launch offset 15, child reload 48) = 43 + 43 (class 0, speed 0, range 30) then impact 42 (class 7, radii 30 and 80, damage 3000): a bomb |
| Eagle (33), Zeppelin (7) | descriptor weapon field is -1 | no weapon in the descriptor; the scoped getter audit establishes that both are unarmed |
| Demolisher (2) | Marine weapon id 38 | class 7 explosion used directly (damage falls off with distance) |
| Spider and wheel flamethrowers (35, 59) | weapon id 34 | class 11 burn (see above); has no weapon record |

Corrections to the open list from `gap_recon.md`:

- Mage and the Aztec crossbow tower are not a plain 500 damage shot: they combine the
  class 3 hit (factor times 48 divided by speed, integer division) with a class 7 impact
  that depends on distance. Their damage in the clone must come from this chain.
- Catapult, Hopcannon, cannons, bombs and the elefant all work through class 0 carriers that
  hand the damage to a class 7 or class 9 impact weapon, so one implementation of the weapon
  graph covers all of them.
- Later getter checks establish unarmed Eagle/Zeppelin, Johnka weapon 54 and the
  unarmed Building firethrow tower 68; see the scoped audit below.

## Weapon effects resolved (second reading)

**V**:

- Jaguar (weapon class 10): the effect calls the attacker's virtual `+0x104` with one third of
  the damage. For a Marine `+0x104` (`0x4029d0`) is **Heal(amount)**: it adds to the
  current hit points and caps at the maximum (virtual `+0x3c`). The Jaguar heals itself for a
  third of the damage it deals (life steal), up to its maximum hit points.
- Snowman (weapon class 6) area effect: virtual `+0x9c` (`0x471220`) adds 18 to
  the Marine attack cooldown at `+0x84`, which `0x433db0` decrements each tick and
  initializes to reload plus one. Facing is `+0x80`. The older turning claim was
  based on another object's layout and is superseded. The scan is a 96 px square
  centred on the attacker; qualifying ground, non-allied Marine categories receive
  the cooldown addition when their hit rectangles overlap it. No movement slow,
  rotation, ownership change or secondary HP damage occurs.
- Marine virtual `+0xa0` (`0x432fa0`) returns descriptor byte `+0x2c` (class flag byte 0); bit 0 of
  a group's flag dword requires that every member answers true. Its meaning is not yet
  established; it is not used by the ferry.

## Units without a weapon, Johnka, class 8 and the anti-air table (fourth pass)

**V** from the decompilation and the rules tables.

- The weapon getter of the Bird kind (`0x407780`; the earlier `0x42c3c0` is Headtruck) reads the weapon id from descriptor dword 9
  (`+0x24`), of the Zeppelin kind (`0x471680`) from dword 8 (`+0x20`) and returns no weapon
  when the id is -1 or when the damage against the target is not positive. **The Eagle (33)
  and the first Zeppelin (7) have id -1: they are unarmed.** (Scouts or decoys in the
  missions that place them; the Zeppelin death routine `0x471740` plays a crash effect.)
- **Johnka (88)** has no descriptor weapon: its getter (`0x42d950`) returns the prototype at
  `0x515b98`, which is **weapon 54**, only when the target is not airborne.
- **Class 8 is a rocket volley:** `rec[0]` is the id of the explosion weapon (class 7), `rec[1]`
  the scatter radius (96 px), `rec[2]` the range and `rec[3]` the reload in ticks (`0x4039b0`; corrected, not a rocket count). Weapon
  19 (Katusha trucks and wheels) fires explosion 17 (80 rockets, range 200), weapon 20 (the
  Chinese air tower) fires explosion 18 (50 rockets, range 200), weapon 54 (Johnka) fires
  explosion 53 (30 rockets, range 300). Explosions 17 and 53 are ground-only; **explosion 18
  has the airborne flag set and is the only air-only explosion in the game.**
- **Class 8 is a scheduled volley:** `rec[0]` is the explosion id, `rec[1]`
  the scatter radius (96 px), `rec[2]` range, `rec[3]` reload. It schedules **16**
  explosion positions in ten update buckets, not 80/50/30 rockets. Weapon 19
  (Katusha trucks/wheels) uses explosion 17, range 200, reload 80; weapon 20
  (Chinese air tower) uses explosion 18, range 200, reload 50; weapon 54
  (Johnka's special getter) uses explosion 53, range 300, reload 30. Explosions
  17/53 are ground-only; **18 is air-only**. All three have damage 3000 and
  radii 15/25. See the later audit for the dispatch functions and timing limits.

### Which weapons can hit an airborne target (from the damage formulas)

| Result | Units and towers |
| --- | --- |
| Yes | Rifleman (direct damage times 1.25), Rus watch tower (1.25), Archer (arrows 1.0), Aztec archer tower (1.0), Bizon (0.62), Paraplan (1.0, air to air), Chinese air tower (volley, air only) |
| No | Footman, Bear, Knight, Ninja, Snowman, Jaguar, Chief, Abdula, Tuzem, Hypno and the hypno tower, spider and wheel crossbows, axe-thrower trucks and wheels, the flamethrower family, Mage and the Aztec crossbow tower (class 3 gives zero against airborne targets and its explosion is ground-only), cannons, cannon towers, Elefant, Catapult, Hopcannon, Zeppelin2 bombs, Katusha, Johnka, Deltaplan |

The unit rule behind it: the weapon getter drops any weapon whose damage against the target
is not positive, so a zero airborne multiplier means the unit does not attack air targets at
all. The clone must reproduce this list (it determines which armies can defend against the
Eagle, Deltaplan, Paraplan and Zeppelin waves of the missions that use them).

## Knight and Ninja sweep effect (`0x46e8b0`)

Evidence date: 2026-11-14. **V** = read from the decompilation (function address only).

- The effect asks the owner's object pool (virtual `+0x100` on the owner's world) for the
  facing angle (0x00 to 0xff scale) and runs three sector queries through `0x470440` with
  the sector argument `0x60` (a width byte).
- Query 1 is centred on the facing angle with radius `0x14` (20 px). Every victim found
  receives the full weapon damage, taken from the weapon virtual `+0x5c` (3000, or 1500
  against non-organic and structures) through the victim virtual `+0x124`.
- Queries 2 and 3 are centred on the facing angle minus `0x19` and plus `0x19` (about
  35 degrees to each side) with radius `0x1e` (30 px). Victims there receive half of the
  weapon damage (integer division by 2).
- G: a victim that sits in both the frontal sector and a side sector may be hit more than
  once, because the three result lists are walked independently; a live test would settle
  it.
- The fire helper `0x42a010` is not in the decompilation corpus (no function at that
  address was exported); it is now read, see the next section.

## Fire stream helpers (`0x42a010`, `0x429e20`, `0x429f60`)

Evidence date: 2026-11-14. **V** = read from the Ghidra decompilation (function addresses
only), **G** = guess.

- V: `0x42a010` is the fire object set-up, not a damage effect. It reads the owner's facing
  angle (owner virtual `+0x100`), combines it with the owner position, and picks the
  sprite direction frame as `((angle + 4) / 8) & 0x1f` (32 directions) through `0x4034c0`.
- V: `0x429e20` collects victims: a circle query around the point `weapon virtual +0x68`
  displaced by `weapon virtual +0x74` (reach), up to a caller supplied maximum, keeping only
  victims whose per victim damage from `0x429f60` is non-zero, and returns the pair
  list (victim, damage).
- V: `0x429f60` per victim damage: zero if the victim is the weapon owner or is airborne
  (victim virtual `+0xa4`). Otherwise the base value is `0x7d000` (512000); if the side
  test `0x4472e0` says the victim is friendly, the base is scaled by a fixed point factor
  `0x40` through `0x404810`; the result is multiplied by the victim fire armor (virtual
  `+0x108`) and shifted right by 8.
- V: `0x404810` is a fixed point multiply, `a = (a * b) >> 8`, so the friendly factor
  `0x40` is 64/256: a quarter of the damage (512000 becomes 128000).
- Consequence: burn damage per hit is about 2000 times the fire armor value of the victim
  (the armor value is a 256 based multiplier), a quarter of that for allies, and zero
  against air units.

## Corrections from the towers, air units and specials pass (2026-11-14)

- V: the Snowman status value 0x12 added through virtual `+0x9c` (`0x471220`) goes into the
  Marine attack reload countdown (`+0x84`, `0x433db0`): it is an 18 tick attack delay, not a
  stun or a knockback, and it does not turn the facing.
- V: volley class 8 always releases 16 explosions, each at `rand % 10` ticks within the
  scatter radius (96 px); `rec[3]` (80, 50, 30) is the reload.
## Ground classes completed (2026-11-18)

This pass supersedes earlier O/G effect labels and the swapped class 2/13 factory
links above. **V** = read from the private original or from the existing public
records. Formulas, helper lifetimes, status meanings, clone limits and live probes
are in [ground_weapons.md](ground_weapons.md). In particular, Snowman adds attack
cooldown; class 3's damage getter is a kill-time estimate rather than one impact.
Class 7's inner radius bypasses armor; only its falloff band applies e8 and fire
armor. A target's hit extent is 28 px, not the clone's movement collision radius.

All rows below are V. Speed is per tick; range and radii are pixels. Reload means
the weapon getter, before the Marine's additional tick and animation scheduling.
Payload-only getters are listed for completeness and do not create an extra shot.

| Class | Weapon ids | Active descriptor or chain | Speed; reload; range | Payload / area / other effects |
| --- | --- | --- | --- | --- |
| 0 | 2 | Hopcannon 87 | 30; 37; 250 | Explosion 3 at actual landing |
| 0 | 30 | No active descriptor in the current type table | 0; 40; 15 | Vertical drop, explosion 3 |
| 0 | 32 | Catapult 83 | 30; 30; 200 | Explosion 3 |
| 0 | 40 | Cannon 4 via pair 39 | 30; 45; 290 | Explosion 41, two carriers |
| 0 | 43 | Zeppelin2 85 via pair 44 | 0; 48; 30 | Vertical bombs, explosion 42 |
| 0 | 46 | Elefant 57 via pair 47 | 20; 40; 230 | Point payload 45, two carriers |
| 0 | 50 | Rus cannon tower 26 via pair 51 | 30; 45; 360 | Explosion 49, two carriers |
| 3 | 5,6 | Axe trucks/wheels 3,61 and fortress tower 23 via pair 7 | 25; 35; 250 | Contact 500/tick, payload 21 |
| 3 | 22 | No active descriptor in the current type table | 20; 20; 200 | Contact 600/tick, payload 23 |
| 3 | 24 | Mage 28 | 20; 20; 200 | Contact 600/tick, explosion 25 |
| 3 | 26 | No active descriptor in the current type table | 20; 20; 200 | Contact 600/tick, payload 27 |
| 3 | 28 | Aztec crossbow tower 49 | 20; 20; 225 | Contact 800/tick, explosion 29 |
| 7 | 3 | Payload of 2,30,32 | Direct; 40; 192 | Radii 24/72, damage 3000, ground |
| 7 | 17,18 | Class 8 Katusha / air tower payloads | Direct; 30; 200 | Radii 15/25, damage 3000; 17 ground, 18 air only; class 8 scheduler belongs to task 141 |
| 7 | 25 | Mage payload | Direct; 30; 200 | Radii 20/35, damage 2000, ground |
| 7 | 29 | Crossbow tower payload | Direct; 30; 200 | Radii 20/50, damage 2700, ground |
| 7 | 37 | No active descriptor/chain in the current tables | Direct; 40; 180 | Radii 20/30, damage 1500, ground |
| 7 | 38 | Demolisher 2, directly | Direct; 30; 180 | Radii 25/55, damage 3500, ground; attacker survives unless independently damaged |
| 7 | 41 | Cannon payload | Direct; 45; 330 | Radii 35/70, damage 4000, ground |
| 7 | 42 | Zeppelin2 bomb payload | Direct; 50; 120 | Radii 30/80, damage 3000, ground |
| 7 | 49 | Cannon tower payload | Direct; 45; 360 | Radii 35/70, damage 4000, ground |
| 7 | 53 | Johnka via class 8 weapon 54 | Direct; 30; 200 | Radii 15/25, damage 3000, ground; vessel/volley dependency is separate |
| 9 | 21 | Axe-pair point payload | Payload only; 0; 0 | Base 2000; multipliers air 0, non-organic 160/256, f4 1, e8 1, structure 160/256 |
| 9 | 23,27 | Payloads of inactive 22,26 | Payload only; 0; 0 | Base 1800; air 0, non-organic 1, f4 128/256, e8 336/256, structure 2 |
| 9 | 45 | Elefant point payload | Payload only; 0; 0 | Base 3800; same category multipliers as 23/27 |
| 14 | 0 | No active campaign weapon | No shot | Absent-chain sentinel; optional save helper class is not a weapon |

For every class 3 row, contact damage is zero against allies/air and is multiplied
by 176/256 for non-organic targets. Its secondary effect has its own alliance and
air rules. For every class 7 row, damage includes allies, and radii gain the 28 px
target extent. For every class 9 row, the armor coefficient is zero in the current
records; point effects include allies. These classes change primary HP only.
Class 0/3 create projectiles, class 7 creates an explosion animation helper, and
class 9 is a point application without persistent status. None changes sides,
secondary HP or movement speed, and none spawns a controllable unit.

The new synthetic `ground_weapons` test covers formulas, Mage/tower duels and
groups, friendly/air controls, moving-target misses, Demolisher groups, Knight and
Ninja sectors, Jaguar overkill healing/cap, Snowman additive cooldown and point
payloads. Replay hashes cover transient class 3 identity and direct explosions.
## Fire, air and siege implementation audit (2026-11-21)

**V** means inspected original behavior; **G** means an engine approximation.
See [fire_air_siege.md](fire_air_siege.md) for formulas, address evidence,
implementation boundaries and the every-tick live run sheet. No live observation
was obtained for this audit.

| Family | Established behavior (V) | Clone implementation / remaining limits |
| --- | --- | --- |
| Class 11, 35/59 | Weapon 34: reload 40, range 96; potential damage is ten times a 2000-HP application, quarter allied damage, no self/air damage, victim +0x108 resistance | Ten active animation updates scan the source-facing segment; movement, source death, allied/neutral victims and replay covered. Object boxes and first pool phase remain G |
| Truck flamethrower 8 | Weapon 58 is an arrow: speed 50, reload 65, range 300, base 19000, zero duration and armor component | Ordinary arrow impact, no added burn tail |
| Chinese firethrow tower 68 | Unarmed Building with harvest/service fields; no hidden class 11 dispatch in its update | Existing Building economy; no guessed weapon |
| Bird | Actual getter 0x407780, d9; 33 unarmed, 34 weapon 35, 60 weapon 52 | Descriptor maximum speed, cruise altitude, turn/acceleration rules, flight pursuit and heading-gated Bird firing. Native RNG/tactic/animation phase parity remains open |
| Zeppelin | Getter 0x471680, d8; 7 unarmed, 85 weapon 44 | Twin falling bombs start at current altitude; no invented bomb speed 48 |
| Cannon | Descriptor d7; Hopcannon 87 selects carrier 2 | Reload 37, speed 30, range 250 and payload 3; first impact phase/ground launch height remain G |
| Deltaplan | Twin arrows 36 dispatch ground explosion 37: 1500 damage each, radii 20/30, speed 15, reload 30, range 140 | Endpoint spatial impacts include bystanders; no air damage |
| Air facilities 25/52/79 | Existing recipes train 85/34/60 respectively; Eagle uses nest 46 | Existing town production gates, no free autonomous aircraft |
| Air tower 81 | Weapon 20 schedules air-only explosion 18 | Sixteen delayed spatial explosions, range 200, reload 50 |
| Catapult 83 | Carrier 32: speed 30, reload 30, range 200; explosion 3, damage 3000, radii 24/72 | Existing spatial ballistic impact; synthetic timing and replay checks |
| Katusha 9/58 | Weapon 19 schedules ground explosion 17, 16 positions, ten buckets, range 200, reload 80 | Deterministic snapshot scatter/delays; no travelling rocket invented |

The scoped implementation does not complete the separate ground-weapon task
140. Its queued work includes general arrow residual helpers, continuous Axe
collision, target-box provenance and reference firing-animation integration.
Johnka's special getter is bound to weapon 54, with existing vessel enable
controls. Vessel tactic acquisition and native firing phase still need live checks.
