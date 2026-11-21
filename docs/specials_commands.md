# Class 13 hit effect 0x46ed90, Mage/Chief/Abdula actives, invisibility

Read-only pass, 2026-11-21. No earlier partial note existed. V = read in code or
binary data, G = guess. Numbers are hex (decimal).

## 0. Correction: 0x46ed90 belongs to class 13, not class 2

- V: weapon factory `0x46d900` case 2 installs vtable `0x495130`; case 0xd (13)
  installs vtable `0x494f10`. Both point `+0x38` at a 0x38 (56) byte record in
  table `0x494a90`, indexed by `0x494d34[id*8]`; the class is `0x494d30[id*8]`.
- V: slot `+0x7c` read from the exe: `0x494f10+0x7c` = `0x46ed90`,
  `0x495130+0x7c` = `0x46e0d0`. So class 2 uses effect `0x46e0d0` and class 13
  (Hypno weapon 0x38 (56), hypno tower weapon 0x39 (57)) uses `0x46ed90`.
  This agrees with `ground_weapons.md`; `hypno_recon.md` line 34 and the first
  vtable table in `weapon_class_map.md` (row "0x494f10 ... 2 (footman ...)") have
  it swapped and should be fixed.

### Direct-weapon record layout (classes 2 and 13), V from `0x46f310`, `0x46f2c0`, `0x46ed90` and data

| Offset | Meaning |
| --- | --- |
| +0x00 | reload (ticks) |
| +0x04 | base damage |
| +0x08 | armor-term coefficient (<<8, multiplied by target virtual `+0x10c` through `0x404810`) |
| +0x0c | helper duration in ticks |
| +0x10 | range |
| +0x14 | multiplier if target `+0xa4` (airborne), 8.8 |
| +0x18, +0x1c, +0x20, +0x24 | multipliers for target `+0xf0`, `+0xf4`, `+0xe8`, `+0xec` |
| +0x28 | muzzle sprite id (0 = none) |
| +0x2c | hit sprite id (0 = none) |
| +0x30 | sound list id (0 = none) |
| +0x34 | flag, 1 for ranged records (d13 centre/edge range per combat-parity doc) |

V (data): every class 2 and 13 record has +0x08 = 0, so no helper is ever
spawned with shipped data. Records 9 and 10 (weapons 56, 57): base 0xce4 (3300),
range 0xc8 (200), reload 0x14 (20) / 0xa (10), muzzle 0, hit 0, sound 0 / 0xc9 (201).
Ranged class 2 records (weapons 9, 15, 33, 52) use muzzle 0x6a (106), hit 0x6e (110),
sound 0xc8 (200); melee ones (4, 10, 12, 55, 59) have no sprites/sound.

### What 0x46ed90 does, in order (all V unless marked)

1. CanFire: own virtual `+0x78(owner, target)`; if false, nothing but self-delete.
2. If rec+0x2c: spawn a cosmetic object from pool `0x4c1978` via `0x449b10` at the
   target position (`+0x14`) and target height (`+0x18`).
3. If rec+0x28: spawn a muzzle object (`0x46f280`, pool `0x4c1978`) through
   `0x4189d0` at the owner position offset 0x14 (20) px along owner direction
   `+0x100`.
4. Damage: `0x46f310` (base times the five category multipliers, each `*m>>8`;
   no armor term) delivered by target virtual `+0x124(weapon, dmg)`.
5. If `0x46f2c0` (armor term) is nonzero: helper `0x46f130` / `0x46f170`, vtable
   `0x48f6c4`, holding owner, target, the amount and rec+0x0c ticks (damage over
   time; unused by shipped data).
6. If rec+0x30: random sound from that list at owner `+0x40` via `0x401480`.
7. Self-deletes (virtual `+4(1)`), then draws a beam trail from the shot origin
   to the target: if distance (`0x461fe0`) > 0x1e (30), repeatedly spawn sprite
   0x6b (107) objects (pool `0x4c1978`, `0x4189d0`, frame = angle>>3) every
   0x1e (30) px, each step jittered by `rand % (2s) - s` angle units with
   `s = max(0, 0x40 - 0x3c0/dist)` (64 - 960/dist), until less than 0x1f (31) px
   remain. G: origin is the muzzle point / owner position (decompiler variable
   tracking is broken there).

- V: no area query, no status change, no side/ownership change, no slow or stun.
  The only extras are cosmetic objects (muzzle, hit, trail 0x6b) and sound.
  Class 2 `0x46e0d0` is the same minus the trail (per `ground_weapons.md`, not
  re-read here).

## 1. Player commands: the full packet set

- V: packet type names table `0x4ad5dc` (`PCT_*`), used by broadcast `0x43d060`.
  In-game frame packets dispatched per player (stride 0x69c (1692) at
  `0x4dd2f4`) by the switch in `0x4577b0`:

| Type | Name | Handler (V) |
| --- | --- | --- |
| 0x12 (18) | FRAME_SAVE | sets `0x515559` |
| 0x13 (19) | FRAME_PAUSEON | `0x463480` |
| 0x15 (21) | FRAME_CREATEGROUP | `0x448b40` |
| 0x16 (22) | FRAME_SELECTGROUP | `0x448520` |
| 0x18 (24) | FRAME_ATTACK | `0x447e00` (the generic right-click order) |
| 0x1a (26) | FRAME_DESTROYHERE | `0x4489b0` |
| 0x1c (28) | FRAME_BUILD | building `0x45cec0`/`0x45cd50`, then `0x410960` or `0x41d6a0`; -1 becomes 0x25 |
| 0x1d (29) | FRAME_GARRISON | `0x448350` |
| 0x1e (30) | FRAME_REPEARMODE | toggles `0x4cc7d4`, tactic `+0x128` vtable `+0x3c` |
| 0x1f (31) | FRAME_ATTACKMODE | object by id (`0x447ba0`) virtual `+0x100(mode)` |
| 0x20 (32) | FRAME_BUILDMODE | object `+0x158 = value` |
| 0x21 (33) | FRAME_PAUSEBUILD | 1: `+0x110` vtable `+0xc`; 2: `0x45a880` |
| 0x22 (34) | FRAME_QUEUEKILL | `0x45cfd0` |
| 0x23 (35) | FRAME_SELECTBYNO | camera centre, then `0x448f60` |
| 0x24 (36) | FRAME_FOLLOWTEAM | `0x448210` |

- V: 0x14, 0x17, 0x19, 0x1b have no case in this switch; 0x25 is FRAME_NOTHING.
- V: there is no cast/spell/ability packet type. The attack-mode buttons
  (`0x443490`, `0x4435e0`, `0x443730`, `0x443880`, vtables at `0x493bd4`,
  `0x493c48`, `0x493cbc`, `0x493d30`) only queue 0x1f with mode 2, 1, 3, 0
  into the local ring (index `0x4dd150`, mask 0x3f) with no multiplayer test, so
  single player uses the same channel. G: the four modes are stances.
- V: `0x447e00` is called only from `0x4577b0` (and itself). It branches only on
  flags 1, 2, 0x20 and on what lies under the cursor (visible own/allied/enemy
  object via virtual `+0x10(light mask +0x690)`, side test `0x4472e0`, building
  kind 6): move to cell (`/0x30` (48), selection `+0xd8`), `+0xf8` cell order,
  attack/enter object (`+0xf0`), `+0xe0`, follow/guard own unit (`+0xf4`), stop
  (`+0x80`/`+0xfc`). No unit type test, no cost or cooldown.

## 2. Mage (0x1c, 28), Chief (0x37, 55), Abdula (0x52, 82)

- V: the only unit-type comparisons with these ids in the corpus are
  `type +0x50 == 0x52` inside `0x433960` (Marine death). No `== 0x1c` or
  `== 0x37` test on `+0x50` exists.
- V: Abdula death extra (`0x433960`): random sound from list 0x96 (150) and a
  decoration object sprite 0x97 (151), frame `rand % 3`, at the death position
  with y+1. Everything else is the ordinary Marine death (animation
  `rand % 3 + 0x11`, map-cell clear, tactic unlink).
- V (packets above): no player-triggered ability exists for any unit, hence no
  cost, cooldown, range or duration to report. Their combat is the descriptor
  weapon only: Mage weapon 0x18 (24, class 3 projectile, then explosion 25);
  Chief and Abdula use class 2 direct weapons (`weapon_class_map.md`).
  G: Chief = weapon 0x37 (55, record 8: reload 6, base 0x7d0 (2000), range 0xa (10),
  multipliers 0/768/128/128/512) — the id/type coincidence was not checked
  against the descriptor.
- G: names like "Mage" or "Chief" imply nothing more in the code; Abdula is a
  named Marine with a death decoration.

## 3. Invisibility

- V: target visibility is `0x402490` (Marine vtable `0x4932cc` slot `+0x10`):
  bilinear sample of the per-player light map (`0x402a20`, 0x30 (48) px cells)
  at the object position `+0x24/+0x28`, compared against 0x2000 (8192, 32.0 in
  24.8) by `0x403050`. It reads no type, flag or ability state.
- V: `0x433db0` (Marine Think) and `0x447e00` (orders) use only this test. So
  "invisible" in existing docs means unlit / fog of war.
- V: the strings "Hidden team ..." (`0x4ae6cc`, `0x4ae8a4`) are AI team logs,
  and no string contains invis/stealth/cloak. Conclusion: no unit
  invisibility mechanic, except invisible scenario controllers (Submarine,
  per `engine_rules_status.md`).

## Unknowns

- Exact origin of the class 13 trail (muzzle vs owner centre).
- Meaning of the four attack modes 0..3 and of Marine `+0x100` (`0x433020`,
  not in the corpus as its own function).
- Packets 0x14, 0x17, 0x19, 0x1b have no case in `0x4577b0`: handled elsewhere or
  unused.
- Chief/Abdula weapon id to descriptor mapping (G above).
- Whether a scripted (mission) action gives these units extra behaviour; mission
  scripts were not read.
