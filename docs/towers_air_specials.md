# Towers, air units and open special abilities (decompilation read)

Evidence date: 2026-11-15. Read-only; no game launched. **V** = read in the Ghidra decompilation
at the cited address (or in the rules CSV under `docs/data`), **G** = inference. Numbers hex and
decimal. Object fields are byte offsets. Vtables: Defence `0x4905bc`, Bird `0x48f97c`, Zeppelin
`0x4951b8`, Marine `0x4932cc`, volley weapon (class 8) `0x494430`, explosion (class 7) `0x4943a4`.

## 1. Defence tower (think `0x41d790`, slot `+0x60`)

Fields (V, `0x41d790`, `0x41d1a0`, `0x41d220`): `+0x64` state (1 under construction, 2 working,
3 dying, 0 gone), `+0x68` fire phase (0 aim, 1 shot animation), `+0x6c` descriptor, `+0x70` base
sprite, `+0x74` turret sprite (0 = none), `+0x78` aim target, `+0x7c` target of the running shot,
`+0x80` reload countdown, `+0x84` turret heading 0..255, `+0x88` damage level, `+0x30` owning tactic.

| Step | Behaviour | V/G | Evidence |
| --- | --- | --- | --- |
| Activation | Slot `+0xc4` sets alive, state 2, HP = descriptor word 3, turret heading = `rand & 0xff` (rand is consumed here) | V | `0x41d1a0` |
| Construction | Slot `+0xc8` sets state 1 and picks the build frame from `hp * (frames+1) / maxhp`; a state 1 tower does nothing in Think | V | `0x41d700`, `0x41d790` |
| Target upkeep | Aim target dropped when dead (tactic callback `+0x130`), when not visible to the owner side (target virtual `+0x10` with side `+0x690`), or when slot `+0xc0` fails | V | `0x41d790` |
| In-range test | Slot `+0xc0` = `0x43e9f0`: target alive, weapon getter non-null, weapon can-fire `+0x78` true | V | `0x43e9f0` |
| Weapon getter | Descriptor `+0xec` id, prototype table `0x515ac0`; returns null when id is -1 (`0xffffffff`) or damage `+0x5c` against that target is <= 0, so a tower never targets what it cannot hurt | V | `0x41d320` |
| Acquire | Tactic slot `+0x120` (`0x455ad0` -> Local_assign `0x452c70`) is called every tick while target is null and reload is 0; also once reload hits 0, and early while reloading when there is no target and reload < `(0x100 / step) / 2` (step = descriptor word 4) | V | `0x41d790`, `0x455ad0` |
| Tactic gate | The path-state gate of `0x455ad0` does not block towers: Defence slot `+0xe0` is the constant-true stub `0x471240` (Marine has constant false) | V | vtable, `0x455ad0` |
| Turn | Aim point = weapon `+0x68`(tower, target); wanted heading = `0x461ac0`; signed 8-bit difference. If abs(diff) >= step: heading += sign * step (mod 256), turret frame `((h+4)>>3)&0x1f`, no shot this tick | V | `0x41d790` |
| Fire gate | abs(diff) < step: heading snaps to exact, the tactic is asked again (`+0x120`); if the target is unchanged and weapon can-fire and reload == 0: reload = weapon `+0x60`, phase 1, shot target latched in `+0x7c`, aim target cleared (a new target is requested for every shot). Animation: turret animation `frame + 2`, or base animation 8 for turretless towers | V | `0x41d790` |
| Step values | 8 (0x08) = 32 facings (records 0, 1), 32 (0x20) (record 7 air tower), 128 (0x80) (others). With 128 the turn takes one tick at most. The phrase "may fire below half a step" in `descriptor_building_defence.md` is wrong: the fire gate is abs(diff) < step; half of `256/step` is only the early re-acquire window | V | `0x41d790`, `desc_defence.csv` |
| Release | While phase 1, on every tick whose current animation frame has its marker byte (`frame record +0xc`) set, a weapon object is created (`0x46d900`, weapon id from prototype `+0x34`) and its effect `+0x7c` is called with the tower position, launch height = descriptor `+0xfc` (36..68 px, 0x24..0x44) and the latched target | V | `0x41d790` |
| Launch height meaning | Third argument of the position getter `+0x14`, the z of the muzzle; confirmed as height because Bird and Zeppelin pass their altitude field in the same place | V | `0x41d790`, `0x407b10`, `0x4719b0` |
| End of shot | When the shot animation ends: latched target released, phase 0, sprite reset | V | `0x41d790` |
| Reload cue | At reload == 12 (0x0c) a turret animation call `0x403510(turret, 1)` (reload visual) | V call, G meaning | `0x41d790` |
| Attack delay | Slot `+0x9c` = `0x41cbb0` adds to `+0x80` (reload); slot `+0x98` reads it | V | `0x41cbb0`, `0x41cba0` |
| Damage look | level = `3 - (hp*4 - 1) / maxhp`; on change spawns effect `0x9a`/`0x9b`/`0x9c` with sound list `0x97`/`0x98`/`0x99`; at level 3 (HP <= 25 %) spark effect `0x69` every 3 ticks at x offset `rand % 29 - 14` | V | `0x41d790` |
| Service heal | Slot `+0xd8` = `0x41cbe0`: immediate `hp += n` capped at descriptor word 3 (towers heal instantly; units get a regeneration window instead, see section 3) | V | `0x41cbe0` |
| Garrison, ammo | None. No occupant list, counter or ammunition field is read in Think, the weapon getter, activation or the target setter; the only counters are reload and reference counts | V (absence in the functions read) | `0x41d790`, `0x41d320`, `0x41d1a0`, `0x41d220` |
| Airborne | Slot `+0xa4` is constant false (`0x402950`) | V | vtable |

Tower weapons (V from `desc_defence.csv`, `weapons_ids.csv`): record 0 weapon 7 (0x07), height 36;
record 1 weapon 51 (0x33), 36; record 3 weapon 15 (0x0f), 68; record 4 weapon 14 (0x0e), 40;
record 5 weapon 28 (0x1c), 46; record 7 (air tower) weapon 20 (0x14), 64, step 32; record 8 (hypno
tower) weapon 57 (0x39), 60; records 2 and 6 unarmed (-1).

Paired weapons (class 12, effect `0x41e4a0`, V): both component weapons are created and launched
in the same call (same tick), from two points offset to either side of the muzzle; the offset
length comes from the class 12 record word at `+8` (G for exact geometry, decompilation garbled).

## 2. Air units

### Common (V)

- Airborne predicate = object slot `+0xa4`: constant true `0x471240` for Bird and Zeppelin,
  constant false `0x402950` for Marine and Defence. Every weapon formula that zeroes air damage
  tests this slot; the unit and tower weapon getters (`0x41d320`, `0x407780`, `0x471680`,
  `0x4166a0`) return no weapon when damage <= 0, so "who may attack air" is exactly the list in
  `docs/weapon_class_map.md` (rifleman, Rus watch tower, archer, Aztec archer tower, Bizon,
  Paraplan, Chinese air tower).
- Altitude `+0x7c`: Bird starts at 30 (0x1e) (`0x407360`), Zeppelin at 0 (`0x471390`); both
  climb 1 per tick to the descriptor cruise word (Bird 85/110/110, Zeppelin 95/110)
  (`0x407b10`, `0x4719b0`). Nothing in either Think lowers it: there is no landing. Death is
  state 4 then removal (`0x407870`, `0x471740`).
- No take-off state: placement (`+0xc4`) puts the unit in state 1 and the climb runs in every
  live state. State 3 is only an animation wait that returns to state 1.
- Reload countdown is `+0x84` for both, minus 1 per tick; slot `+0x9c` (`0x471220`) adds to it.
- Regeneration as documented (cap switches to max HP while `+0x54` is non-zero).
- Move order slot `+0xcc` (`0x407820`, `0x4716f0`): destination = tile centre
  `(tile*48 + 24) << 8`, state 2; ignored in states 3 and 4.

### Bird kind (Paraplan, Deltaplan, Eagle), think `0x407b10`

| Item | Behaviour | V/G |
| --- | --- | --- |
| States | 1 hover/idle (calls tactic `+0x11c` every tick, no movement), 2 flying, 3 animation wait, 4 dying | V |
| Target | `+0x94`, set by slot `+0xd4` (`0x407670`); dropped only when dead (tactic `+0x130`). No visibility or leash test here (unlike Zeppelin and Marine) | V |
| Chase | With a target, the destination is overwritten every tick with the target position | V |
| Turning | Rate = descriptor word 6 per tick. If there is a target with a usable weapon, abs(heading error) > 0x40 (64) and distance < weapon range * 2, the rate is divided by 3 (wide fly-out before the next pass) | V |
| Attack run | Fire when heading error < turn rate (nose on target), reload == 0, slot `+0xc0` true (alive, weapon, can-fire range). Then: weapon object from `0x46d900`, reload = weapon `+0x60`, effect `+0x7c` from own position at own altitude, target released and cleared, tactic `+0x134` (re-pick), attack sound list descriptor `+0x50` | V |
| Cruise | When heading error <= `0x38 - rate` and no target: after an 18 tick (0x12) timer `+0x8c` the heading drifts 1 unit per tick to the nearest multiple of 0x20 (8 directions) | V |
| Speed | +2 per tick up to descriptor word 5 while heading error < 0x38 (56); near the goal without a target it drops by `2 + (s*s/2)/(3s+24)` (s = max speed) down to a floor `+0x90` | V |
| Never stops | Movement is applied every tick in state 2 and nothing in Think returns state 2 to 1, so a bird that was ordered once keeps circling its goal. Circle radius `speed*256/6/rate` px: Paraplan 64, Deltaplan 109, Eagle 106 | V formula, G "forever" (no writer of state 1 found in `0x407200..0x408500` except placement and the state 3 exit) |
| Arrival | Within `(speed + 8) * 3` px of the nearer circle centre: tile `+0x44/+0x48` updated through tactic `+0x124`, then tactic `+0x11c` | V |
| Weapons | Paraplan 52 (0x34) class 2 direct, range 200, hits air; Deltaplan 35 (0x23) = 36 + 36 arrows; Eagle none (-1) | V (CSV, `0x407780`) |

### Zeppelin kind (Zeppelin 7, Zeppelin2 85), think `0x4719b0`

| Item | Behaviour | V/G |
| --- | --- | --- |
| States | 1 hovering (speed 0), 2 moving, 3 animation wait, 4 dying | V |
| Target | `+0x90`, set by slot `+0xd4` (`0x433650`); dropped when dead or not visible to the own side (tactic `+0x130`) | V |
| Bombing | Each tick: alive, reload == 0, target set, state 1 or 2, weapon getter non-null, weapon can-fire -> weapon object, reload = weapon `+0x60`, effect `+0x7c` from own position and altitude. The target is kept after the shot (no re-pick) and the Zeppelin does not steer at the target itself: it only flies to ordered tiles and drops when a target is in range | V |
| Weapon | Zeppelin2: 44 (0x2c) class 12 = 43 + 43 (class 0 carrier, range 30 px) -> explosion 42 (radii 80/120, damage 3000, ground only). It must be almost above the target. Zeppelin (type 7): -1, unarmed | V (CSV, `0x471680`) |
| Steering | Turn 8 units per tick, snap below 8 | V |
| Speed | accel/brake `s*s/48 + 1` (s = descriptor speed). Accelerates only when heading error < 0x38 and distance >= `(speed + 24)` px; brakes while turning; inside 24 px (0x1800 in 24.8) it brakes to 0 and enters state 1 | V |
| Transport | No cargo field or boarding call is read in Think or the constructor; "transport" for type 7 is unsupported by this code | V (absence), G conclusion |

## 3. Specials still open in docs/special_abilities.md

Listed open there: Knight/Ninja sweep (already read, `0x46e8b0`), Snowman meaning, scatter salvo,
fire stream lifecycle, facility service, Hypno tower/school, Mage/Chief/Abdula actives, invisibility,
garrison, research bonuses, Zeppelin/Eagle steering. Read in this pass:

| Topic | Finding | V/G | Evidence |
| --- | --- | --- | --- |
| Snowman | Slot `+0x9c` of Marine is `0x471220`: adds 18 (0x12) to `+0x84`. Marine `+0x84` is the attack reload countdown (decremented each tick, gates firing, set to reload + 1 on a shot). So it is an 18 tick attack delay: no stun, no knockback, no movement change. `weapon_class_map.md` ("turns the facing by 18 units") is wrong; `special_abilities.md` is right | V | `0x46eb10`, `0x471220`, `0x433db0`, vtable `0x4932cc` |
| Snowman victims | Not friendly (`0x4472e0`), not airborne (`+0xa4`), and category `+0xe8` or `+0xf4`; towers answer false to both, so in practice Marines; candidate buffer 132 dwords (query cap 128 G); box about 96 px (0x60) from the attacker position | V (box edges G) | `0x46eb10` |
| Volley (class 8) launch | Effect `0x44ca60`: tick counter 0, active, height = target virtual `+0x18`; 16 (0x10) entries, each `release = rand % 10`, `radius = rand % rec[1]` (96), `angle = rand & 0xff`, point = target position + polar offset. Rand order per entry: release, radius, angle (48 rand calls) | V | `0x44ca60` |
| Volley think | `0x44cb10` (slot `+0x84`): each tick, every entry whose release equals the counter creates explosion weapon `rec[0]` and calls its slot `+0x88`(point, height); counter + 1; at counter 10 the volley deletes itself. Always 16 explosions over ticks 0..9; it stalls if the owner pointer is null | V | `0x44cb10` |
| Volley getters | Range `+0x74` = `rec[2]` (200/200/300, `0x4039a0`); reload `+0x60` = `rec[3]` (`0x4039b0`): 80 (Katusha, weapon 19), 50 (air tower, weapon 20), 30 (Johnka, weapon 54). So `rec[3]` is the reload in ticks, not a rocket count as `weapon_class_map.md` says. `+0x70` = `rec[1]` scatter radius; can-fire `0x4038a0` = centre distance to aim point <= range; damage estimate = child explosion damage (`0x44c8d0`) | V | cited |
| Explosion at a point | Class 7 slot `+0x88` (`0x44c590`): stores height (clamped at 0), random animation `rand % 3`, queries objects within outer radius `rec[2]`, each gets `0x44c130` damage through `+0x124`; sound list `rec +0x1c`. Air tower explosion 18 has the airborne flag, so each of its 16 bursts hits only airborne objects | V | `0x44c590`, `weapon_class_w7.csv` |
| Hypno tower | Defence record 8, weapon 57 (class 13, record 10): reload 10, damage 3300, range 200, air 0, non-organic x1.0, x2.0 / x1.5 for the two Marine categories, structures x0.5, sound list 201. Effect `0x46e0d0`: hit sprite `rec +0x2c` and muzzle sprite `rec +0x28` are 0; damage-over-time needs `rec +0x8` non-zero and it is 0. Plain direct damage at twice the fire rate of the Hypno unit; no conversion | V | `0x46e0d0`, `weapon_class_w2_13.csv` |
| Hypno school | Type 72 is a Building (record 42) and appears only as a build-tree prerequisite (flag A) of Hypno (54) and the hypno tower (69); no has-type query for 0x48 exists in the corpus | V (CSV), G (absence of other code) | `buildtree.csv` rows 83, 94, 96, 111, 123 |
| Air tower prerequisite | Type 81 requires 74 (workshop) and 79 (air facility) | V | `buildtree.csv` row 124 |
| Facility service | Town `RepairUnits` `0x45d4d0`: three flags from town has-type (`+0x38`): workshop 20/51/74, stable 15/44/77, air facility 25/52/79. Runs only if the workshop or stable flag is set (air facility alone does nothing). Small pool (object count `0x4cc3e8` <= 256): every alive friendly object within tile distance squared < 65 (0x41) of the town with (`+0xf0` and one flag) or (`+0xf4` and the other) gets slot `+0xd8`(10). Large pool: box query 384 px (0x180), and additionally airborne objects when the air facility flag is set | V (which flag pairs with `+0xf0` vs `+0xf4` is G: registers lost; `+0xf0` is true for towers, so workshop is the likely partner) | `0x45d4d0` |
| Healing semantics | Slot `+0xd8`: Marine, Bird, Zeppelin `0x402990` sets `+0x54` = 10 (regeneration then runs to max HP instead of the cap word); Defence `0x41cbe0` adds 10 HP immediately. Slot `+0x104` (`0x4029d0`) is the capped heal used by Jaguar | V | cited |
| Building repair queue | Same function: when the town repair queue is empty, every alive damaged member of kind 1 (not the hall) is queued | V | `0x45d4d0` |

## Unknowns

- Bird minimum speed `+0x90`: written in the placement fragments `0x4075d7`/`0x4075e0`, value not read.
- How a Bird returns to state 1 (hover) after an order, if ever; who sets state 3 for Bird/Zeppelin.
- Call period of `RepairUnits` `0x45d4d0`, and the pairing of the workshop/stable flags with `+0xf0`/`+0xf4`.
- Exact Snowman box edges and class 12 muzzle offset geometry (decompilation garbled).
- Whether the shot animation has exactly one marker frame (a second marker would fire twice); needs the sprite data.
- Local_assign `0x452c70` scoring for towers and air targets was not re-read here (see `work/gaps/idle_autoengage.md`).
- Read later (see docs/specials_commands.md and docs/projectiles_effects.md): the class 13 effect `0x46ed90`, the class 2 effect `0x46e0d0`, the fire stream lifecycle; Mage, Chief and Abdula have no active abilities and invisibility does not exist as a mechanic.
- Zeppelin (type 7) purpose: no cargo code found in the Zeppelin class; a script-level use was not searched.
