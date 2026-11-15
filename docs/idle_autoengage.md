# Idle unit auto-engage in the original (decompilation read)

Evidence date: 2026-11-15. **V** = read in the Ghidra decompilation, **G** = guess.
Function addresses only. Distances "tiles" are tactic tile coordinates, "px" are world
pixels (48 px per tile). The norm everywhere is `0x461fe0` (V: octagonal approximation,
about max + 3/8 min of the absolute components), not Euclidean.

## Summary

- V: a Marine has **no scan of its own**. Every unit belongs to a tactic (`+0x30`), also a
  single unit; all enemy discovery, target choice, chase steps and the leash are done by the
  tactic through callbacks. A "single idle unit" is a one-member tactic in mode 3 (constructor
  default, `0x44f5c0`).
- V: the leash is measured from the **tactic position** (`tactic +0x10/+0x14`, tiles), not from
  the unit's own post: 4 / 8 / 8 / 6 tiles for modes 0 / 1 / 2 / 3 (`0x4516b0`).

## Marine object (vtable `0x4932cc`, think `0x433db0` = slot `+0x60`)

Fields (V): `+0x30` tactic, `+0x5c` alive byte, `+0x64` state (0 gone, 1 stand, 2 step
between tiles, 3 attack animation, 4 dying, 5 idle fidget), `+0x84` reload countdown,
`+0x88` fidget timer, `+0x8c` **chase target** (set by slot `+0xd0` `0x433c50`, read by
`+0xbc` `0x432fd0`), `+0x90` **fire target** (set by slot `+0xd4` `0x433650`), `+0x94`
target of the running attack animation, `+0x68` descriptor (weapon id at descriptor `+0x1c`).

Per tick, unless dying (all V, `0x433db0`):

1. Reload `+0x84` counts down by 1.
2. Chase target is dropped when it is dead, or not visible in the fog map of the tactic side
   (target virtual `+0x10` = `0x402490`, bilinear read of the side map at side `+0x690`),
   or farther than **0x480 px (1152 px, 24 tiles)** from the unit (test `< 0x481`). On a drop
   the tactic callback `+0x134` (`0x454660`) immediately re-picks the best remaining enemy
   from the existing assignment table (dead target: callback `+0x130`, an empty function).
3. Fire target is dropped when dead, not visible, or `0x43e9f0` (slot `+0xc0`) is false:
   weapon prototype from `0x4166a0` (null when weapon damage against this target is <= 0)
   and weapon can-fire `0x46d660`: norm of the offset minus both object radii (virtual
   `+0x20`) <= weapon range (`0x46df90`, weapon record `+0x10`); weapons with record flag
   `+0x34` use the plain centre distance.
4. **If there is no fire target and reload is 0, the unit asks its tactic for one every
   tick**: tactic slot `+0x120` (`0x455ad0`) -> Local_assign `0x452c70(unit)`. This is the
   only "scan" at unit level; it only looks at enemies the tactic has already collected and
   only accepts one the unit can fire at right now (step 3 test inside Local_assign).
   Gate in `0x455ad0`: nothing happens when the tactic has no assignment block (`+0x74`
   null), or when the tactic is on a path (state `+0x20` == 3) and its mode is 0 or 1
   (Marine slot `+0xe0` is the constant-false stub `0x402950`).
5. Fire: alive, reload 0, fire target alive, state not 2 or 3, weapon can-fire -> state 3,
   facing set, reload = weapon reload (`+0x60`) + 1, the fire target moves to `+0x94` and
   `+0x90` is cleared, so a new target is requested for every shot. A unit in a step
   (state 2) only tests this when it arrives on the tile centre.
6. Standing (state 1): tactic slot `+0x11c` (`0x455a90`) -> member step `0x455b10` every
   tick; fidget animation after rand % 200 + 0x3c ticks (60 to 259), animation 0x14 to 0x16.

## (1) Scan: when and how enemies are found (tactic think `0x454820`)

- V: period = the re-plan timer `tactic +0x64`, reseeded to `rand*8/0x7fff + 0xf` =
  **15 to 22 ticks** (0xf to 0x16).
- V: area = bounding box of the tactic position and all member tiles, widened: low edge
  `(min*3 - 0x12)*0x10` px = min tile minus **6 tiles**, high edge
  `(max*3 + 0x15)*0x10 + 0x30` px = max tile origin plus **8 tiles of px** (the far edge of
  the tile 7 beyond). Queried from the object grid `DAT_004c4d18` with `0x470330`. It is one
  constant box for every unit type; there is no per-type radius here.
- V: an object in the box counts when it is alive (`+0x5c`), visible (virtual `+0x10`; G:
  against the fog map of the own side, the argument is dropped in the decompiled text) and
  its side is not friendly. Friendly test `0x4472e0(side, other)`: true for the same side
  index or when bit `other` is set in the alliance mask `side +0x648` (argument -1: true only
  when the mask is -1).
- V: each such object is reported to slot `+0x128` (`0x455570`): ignored when the object is
  kind 7 or has no active tactic (`+0xb0`); raises the "enemy sighted" voice and minimap ping
  for a human side with a cooldown `+0x70` = 0x32 (50) ticks; then, **if the report is
  forced or the tactic mode is 0, 2 or 3**, the tactic of that enemy is appended to the enemy
  tactic list `+0x5c` and the byte `+0x6c` is set. G: the scan passes forced = 0.
- V: at the end of the same think, `+0x6c` triggers Global_assign (`0x451c30`, slot `+0x48`),
  which collects the visible, hostile members of listed enemy tactics whose anchor is within
  **0x19 (25) tiles** of the tactic position and gives members a chase target (slot `+0xd0`).
- G: the per-type engage distance of `docs/data/auto_engage_tiles.csv` therefore comes from
  visibility (the fog of the side, i.e. the per-type sight radius) capped by the box, not
  from a per-type scan radius. The csv rows above 7 tiles (bear 14, mage 12 ...) are not
  explained by the box and need the forced path or another caller.

## (2) Chase, attack and leash (member step `0x455b10`)

- V: no chasing at all in mode 0, and none in mode 1 while the tactic is on a path
  (state 3); the member then keeps or returns to its formation slot.
- V: otherwise, when the member has a chase target: if the member tile is farther from the
  tactic position than the reach (slot `+0xc` = `0x4516b0`: **4, 8, 8, 6 tiles** for modes
  0, 1, 2, 3) it steps **back toward the tactic position**; else it takes the approach
  point from the unit (slot `+0xb4` = `0x433b70`: weapon range - 0x18 < 0x31 gives a contact
  point at 0x30 px via `0x462030`, longer ranges a stand-off point at range - 0x48 px via
  `0x4620e0`) and tries up to 8 neighbouring alternatives until the destination itself lies
  inside the reach.
- V: attack starts as soon as the unit-level test in step 3 passes (weapon range), also while
  merely standing in mode 0.
- V: return: when a re-plan scan finds no hostile object in the box and the enemy list is
  non-empty, slot `+0x5c` (`0x450710`) empties the list and clears chase and fire target of
  every member; members without a chase target fall into the formation-slot branch of
  `0x455b10` and walk back. So disengage latency is at most one re-plan (22 ticks).
- V: the think-time release "target beyond reach of the tactic position" and the reach test
  in `0x454660` only run for members whose virtual `+0xa4` is true; for the Marine that slot
  is the constant-false stub, so a Marine keeps its chase target and is held only by the
  step leash above and by the 1152 px / visibility drop.

## (3) Unit under a group tactic

- V: same code path; no separate "single unit" logic. Differences are only data: the leash
  centre is the group position, the scan box covers all members, targets are distributed by
  the assignment scores, and modes come from the UI or packet setter `0x454350` (it also
  clears assignments through slot `+0x58`).
- V: a tactic with `+0xea` set and the three flags `+0x94/+0x9c/+0x9d` set forwards every
  sighting to its parent tactic `+0x98` (`0x455570` head). G: computer-side sub-tactics.

## (4) Retaliation

- V: every damage application (`0x43ea30`, reached from Marine slot `+0x124` `0x4336b0`) calls
  the victim tactic slot `+0x128` with the victim as the first argument (non-zero = forced)
  and the dword at `+0x30` of the damage source argument as the enemy. Forced reports add the
  attacker tactic to the enemy list **in every mode, including mode 1**, and set `+0x6c`,
  so Global_assign runs at the end of the next think of that tactic (think runs every tick;
  G: within one tick). It also sets the victim word `+0x4c` = 0x14 (20; G: hit timer).
- V: the reaction is then the normal one: fire if in weapon range; chase only as allowed by
  the mode gates and reach in section 2 (mode 0 never moves). No extra retaliation radius
  exists in the handler; the limits are the 25 tile collection radius and visibility of the
  attacker in Global_assign.

## Unknown / not read

- What the `+0x30` of the damage source is (attacker object or a record holding it); the
  handler treats the value as an object with a tactic at `+0x30`.
- The visibility threshold applied to the interpolated fog value in `0x402490`, and which
  side map the scan passes (argument dropped by the decompiler).
- The gate `0x42e8c0(0x31/0x32)`, the scoring loops of Global_assign, and how the formation
  slot branch chooses the return tile (slots `+0x28`, `+0x2c`, `+0x20`, `0x456c60`).
- Why some csv rows engage beyond the 6/7 tile box (needs a live control or the other
  callers of slot `+0x128`: `0x452be0`, `0x457000`, `0x45e0d0` family).
- Slot `+0x60` (`0x450910`, member regroup boxes of -0xa8/+0xd8 and -0x60/+0x90 px around the
  tactic position) and slot `+0x54` were only skimmed.
