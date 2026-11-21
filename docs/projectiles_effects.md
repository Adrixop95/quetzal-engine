# Projectiles and weapon hit effects (gap closure)

Evidence date: 2026-11-21. V = read in the decompilation or disassembly of the original
(addresses only). G = inference not directly read. Numbers as hex (decimal).
Record word n of a weapon is `rec[n]` (rules.json `record_raw`); the in-memory record
pointer is weapon `+0x38`, so record offset `+4*n` is `rec[n]`.
Builds on weapon_class_map.md, ground_weapons.md and fire_air_siege.md; only new or
sharpened facts are listed in detail.

## 0. Weapon objects are per-shot pool objects

- V: the factory `0x46d900` allocates every weapon object (prototypes and per-shot
  instances) from the pool at `0x515a98`, created by `FUN_00473e00(pool, 0x108, 2000, 0, 1)`
  (static initialiser near the factory, corpus line 62906): element 0x108 (264) bytes plus an 8 byte header,
  capacity 0x7d0 (2000). Pool `0x473c40` reserves capacity times element size with
  VirtualAlloc and commits pages on demand (`0x473bb0`).
- V: the base constructor `0x46e4c0` stores the owner at `+0x30` and increments the owner's
  reference count `+0x34`; position starts at (-1000, -1000) (`0xfffffc18`). The owner is
  therefore kept alive (as an object) while any of its shots exists.
- V: the direct and melee effects destroy their own instance at the end (virtual `+4`):
  class 2 `0x46e0d0`, class 13 `0x46ed90`, class 4/5 `0x46e8b0`, class 6 `0x46eb10`,
  class 10 `0x46ed00`. Classes 0, 3, 7 and 11 survive as world objects and remove themselves
  from their update method (vtable slot `+0x84`).

## 1. Class 2 hit effect `0x46e0d0` (vtable `0x495130`)

All V, only when CanFire (`+0x78`, `0x46d660`) accepts the target:

1. Impact animation: if `rec[11]` (`+0x2c`) is non-zero, allocate an effect object from the
   effect pool `0x4c1978` and init it with `0x449b10` at the victim position (`+0x14`) and
   victim height (`+0x18`), animation id `rec[11]`.
2. Muzzle animation: if `rec[10]` (`+0x28`) is non-zero, spawn animation `rec[10]` through
   `0x4189d0` at attacker position plus 0x14 (20) px along the attacker facing
   (`+0x100`, `0x461f70` angle to vector, `0x461d50` scale).
3. Damage: victim `+0x124` with the getter result (category multipliers, see ground_weapons.md).
4. Residual helper: `rec[2]` (`+8`) times victim armor `+0x10c` (8.8) non-zero creates helper
   vtable `0x48f6c4` (from pool `0x515a98`) holding the victim (victim refcount `+0x34` +1)
   with duration `rec[3]` (`+0xc`). All current records have `rec[2]` = 0, so none spawn.
5. Sound: if `rec[12]` (`+0x30`) is non-zero, load sound list `rec[12]` (`0x401250`) and play
   entry `rand() % count` with `0x401480` at the attacker position, volume offset 0, flag 1,
   factor 2.0. This consumes one `rand()` per shot that has a sound.
6. Destroy the instance. No area, no status, no side change.

Record values (V, rules.json): ids 9, 15, 33, 52 (rifleman/watch tower family, bizon,
Paraplan) carry muzzle 0x6a (106), impact 0x6e (110), sound list 0xc8 (200). Ids 4, 10, 12,
55, 59 (footman, bear, chief, Abdula family) carry 0/0/0: no animation and no sound from the
weapon (any melee sound must come from the unit animation, G).
Class 13 `0x46ed90` is the same sequence plus the trail (animation 0x6b (107), see
ground_weapons.md); id 56 has no sound, id 57 sound list 0xc9 (201).

## 2. Other hit effects

- Class 4/5 `0x46e8b0` (V): three sector queries `0x470440` and `+0x124` only; no animation,
  no sound, no `rand()`; instance destroyed. Sweep geometry as in weapon_class_map.md.
- Class 6 `0x46eb10` (V): direct `+0x124`, then the 96 px square scan `0x470130` (capacity
  0x80 (128)) adding 0x12 (18) cooldown via `+0x9c`; no animation or sound; destroyed.
- Class 10 `0x46ed00` (V): `+0x124`, then attacker `+0x104` heal with damage / 3; nothing
  else; destroyed.
- Class 3 terminal phase (V, `0x4050f0`): on arrival it spawns `rec[5]` and switches its own
  sprite to sequence 0x20 (32), the impact/fade frames of projectile animation `rec[0]`
  (0x65/0x66 (101/102) axes, 0x6d (109) Mage/crossbow bolt).
- Class 0 impact (V, `0x406c00`): spawns `rec[4]` and removes itself on the next update; no
  own impact sprite.
- Class 7 explosion `0x44c3c0` (attached to a victim) and `0x44c590` (at a point, used as
  payload through slot `+0x88`) (V): set state 1, explosion animation `rec[0]` (0x96/0x97 =
  150/151) with variant `rand() % 3`, apply damage immediately (section 3), then play sound
  list `rec[7]` (0x96 = 150) entry `rand() % count` at the explosion with volume offset
  `rec[8]` (-500..-100); `0x401480` skips the sound when base plus offset is below -3999
  (DirectSound hundredths of dB, G). The object lives until the animation ends (`0x44c730`).
  RNG order per explosion: variant, then sound.

## 3. Projectile flight, collision, target loss and splash

Class 3 straight projectile (V):
- Launch `0x404f70`: start and aim stored in 8.8 at `+0x40/+0x44` and `+0x4c/+0x50`; the aim
  is the target point (weapon `+0x68`) read once, clamped to range `+0x74`; velocity
  `+0x54/+0x58` = `rec[1]` px per tick along the aim; frame `((angle+4)/8) & 0x1f`.
- Update `0x4050f0`, state 1, per tick: advance the animation, move by the velocity, then
  (a) segment query `0x470600` (capacity 0x80 (128)) from the new position to the aim,
  calling each object's `+0x118(segment, 0x30)` (Marine: empty `0x402a00`; a warning hook);
  (b) point query `0x46fd20` (capacity 0x80) at the new position: every unit or building whose
  hit rectangle contains the point gets `+0x124` with the contact damage (0 for airborne or
  allied, `rec[4]` times 0xb0/0x100 (176/256) for non-organic). The call is made even with
  zero damage. (c) If the remaining distance is below `rec[1]` px, create `rec[5]` through
  `0x46d900` and fire it at the aim point (slot `+0x88`), set sequence 0x20, enter state 2.
- State 2: keeps moving with the same velocity, no damage; deleted when the terminal
  animation has finished and the visible flag (`+0x2c`) is clear.
- No terrain collision and no homing: a dead or moved target changes nothing; the payload
  always lands at the clamped aim point.

Class 0 ballistic carrier (V, `0x4069c0` launch, `0x406c00` update): aim = target point
+ (0, 2), snapshot; T = floor(dist / `rec[1]`) (T = 0 when `rec[1]` = 0); horizontal velocity
`rec[1]` px/tick; height starts at the launch height argument (8.8) with vertical speed 0
and acceleration 2H/(T*T) (0x200 = 2 px when T = 0). Each tick: move, height -= vspeed,
vspeed += accel; when height <= 0 (`0x406da0`) spawn `rec[4]` at the current position (not
the aim) and delete next tick. The apex is the launch height: there is no upward arc
(the flight is a fall from the launch height over T ticks, G for the visual impression).
No contact damage, no terrain test, no homing.

Class 7 splash `0x44c130` (V): candidates from the circle query `0x470010` with radius
`rec[2]` (outer radius), units and buildings that are alive (`+0x5c`); for each candidate in
query order, damage = 0 if airborne flag differs from `rec[5]`; d = integer distance
(`0x461fe0`) to the victim position; r = victim `+0x20` (28 for Marines); 0 if d >= O + r
(O = `rec[2]`, `0x403050`); full `rec[4]` if d <= I + r (I = `rec[1]`, `0x406da0`), no armor;
otherwise linear: `rec[4] * (O + r - d) / (O - I)` in 8.8, times 0xcc/0x100 (204/256) if
`+0xe8`, times `+0x108`, shifted right 8. Linear, not stepped. Applied immediately and
once, victim by victim (`+0x124` right after each calculation), allies included.
G: whether the circle query with radius O (without r) already excludes targets in the
O..O+r band depends on `0x46b0b0` (box versus circle); not read.

## 4. Fire stream (class 11) lifecycle (V, `0x42a010`, `0x42a0b0`)

- Start: `0x42a010` (slot `+0x7c`) starts animation 0x9d (157) on the stream object in the
  owner's 32-direction facing.
- Each update `0x42a0b0` while the animation has frames left: position = owner position +
  0x18 (24) px along the owner facing; re-face the sprite; segment query `0x470600` (capacity
  0x80 (128), pushed at `0x42a19f`) from that muzzle to muzzle + range (`+0x74`, 0x60 (96)) px
  along facing; every unit or building box intersecting the segment takes `0x429f60` damage
  (2000 enemy, 500 allied before `+0x108`, 0 for the owner or airborne).
- End: when the animation reports no frames left and the visible flag is clear, the object
  deletes itself; there is no other timer. With the ten one-tick frames of animation 157 that
  is ten damage updates (frame count from the resource inventory, V there; first and last
  frame inclusion G).
- The only gate inside the update is the owner pointer being non-null; the owner's alive
  state is not tested, and the owner is refcounted so it cannot be freed meanwhile.

## 5. Limits and identities

- V: weapon/projectile/explosion/stream pool `0x515a98`: 2000 objects; the 60-odd prototypes
  in `0x515ac0` occupy part of it. Effect/animation pool `0x4c1978`: `0x473e00(pool, 0x38,
  10000, 0, 1)` at `0x418710`, 10000 objects of 0x38 (56) bytes.
- V: spatial queries are capped at 0x80 (128) per call (class 3 contact and segment, fire,
  Snowman).
- V: no numeric projectile id is drawn. Allocation pops the pool's free-list head
  (`0x473d80`), so the most recently freed slot is reused first; updates and saves walk the
  pool list (`0x4740d0`, `0x474110`). Saves serialise each pool object with its own `+0x50`
  (class 0: `0x406440`; base `0x46d840` writes the owner as an object reference `0x444ba0`).
- G: when a pool is full the commit fails, the factory returns null and the class 0/3 payload
  path calls the fatal error handler `0x475c20(1, ...)`; for the effect pool the null result
  just skips the animation. Not reachable in normal play.

## Still unknown

- Exact update order of the weapon pool relative to units (phase of first movement tick).
- `0x46b0b0` circle query shape (band O..O+r inclusion) and building `+0x20` extents.
- Who supplies the class 0 launch height for ground shooters (attack animation data).
- Meaning of the visible flag `+0x2c` handshake with the renderer (byte `+0x18` of the
  animation), which can delay deletion by a frame.
- Sound list contents 200/201/150 and the base volume used by `0x401480`.
- Who calls `+0x118` meaningfully (non-Marine overrides of the projectile warning hook).
