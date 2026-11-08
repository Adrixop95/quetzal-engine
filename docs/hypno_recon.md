# Hypno reconnaissance (headless reference runs, mission 7)

Evidence date: 2026-11-08. **V** = observed in the recompiled original running headless
under the L0 observer (no window, no network, passive player). Positions in pixels.
No assets or captures are stored here; the observations are numbers only.

Method: mission 7 was run for 6000 ticks twice (coarse sampling every 50 ticks) and once
with sampling every 2 ticks around two Hypno engagements (ticks 2800 to 2900 and 4400 to
4500). The observer was extended locally (not committed) with the secondary hit points
(`+0x3c`), the object state (`+0x64`) and the target handle (`+0x90`).

| Label | Observation |
| --- | --- |
| V | Pool handles are reused: a freed slot receives a new object of another type and side. A naive per-handle owner comparison reports false ownership changes. Only same-type, same-place changes count. Under that rule the only owner change in 6000 ticks is the outpost (type 37, side 1 to player at tick about 450 to 500 and back to side 1 at about 4480 to 4500). |
| V | No unit changed side during the three runs, including during two Hypno engagements with player knights and riflemen. |
| V | The Hypno (type 54) has 6000 primary and 6000 secondary hit points. For most types the secondary value equals the primary one; Bizon is 13000 / 23000 and Snowman 45000 / 22500, matching the descriptor field d4 noted in `rules_types.md`. |
| V | Secondary hit points of the targets of Hypno attacks never changed (a knight stayed at 10000, a rifleman at 5000) while their primary hit points fell (knight 10018 to 8242) and regenerated about 22 per tick afterwards. |
| V | Hypno behaviour states seen: 1 idle, 2 approach, 3 attack. They attack from contact range (about 48 px for melee targets, 100 px against a rifleman). A Hypno lost 4988 hit points in one 2-tick window (6000 to 1012) and others lost 1588 and 3000 in the second engagement; they regenerate about 12 per tick. |
| V | A Hypno killed or lost a target inside the sampling window with no ownership transfer; the next target at the same handle was a new object at full health. |
| G | These runs show the Hypno acting as an ordinary attacker against the player's units. The conversion (ownership transfer) that the rule documents suspect is not reproduced by passive-player runs. It may need a target whose primary hit points are brought to zero by the Hypno, a Hypno school or tower in range, or an explicit order; none of that was exercised. |

Open for the next reconnaissance step (still headless, using the runtime input script
commands `poke` and `click`): force a Hypno next to a unit of another side, lower the
target's primary hit points through memory, and watch side and secondary hit points per
tick; repeat with the Chinese hypno tower of missions 9, 11 and 15.

## Decompilation result: Hypno is a plain ranged damage dealer (no ownership transfer)

Read from the Ghidra decompilation of the original (function addresses only, no code).
**V** = established from the code.

| Label | Finding |
| --- | --- |
| V | The Hypno unit (type 54) has weapon id 56 in its descriptor. Weapon 56 is class 13, the same class as weapon 57. Classes 2 and 13 share the damage formula `0x46dfe0` and differ only in the virtual method at slot `+0x7c` (the hit effect): class 2 uses `0x46ed90`, class 13 uses `0x46e0d0`. Weapon objects are created by the weapon factory `0x46d900` and filled into the table at `0x515ac0` by `0x470780`. |
| V | The class 13 hit effect (`0x46e0d0`) checks CanFire, plays the effect animation and sound from the weapon record, and then calls the target's virtual method at `+0x124` with the attacker and the computed damage. For a Marine target that method (`0x4336b0`) calls the generic damage routine `0x43ea30` and plays the hit sound. A damage-over-time helper object (vtable `0x48f6c4`, think `0x404730`) is only created when the weapon record carries a non-zero duration field; weapon 56 has zero there, so Hypno applies one direct hit per shot. |
| V | The generic damage routine `0x43ea30` subtracts the damage from the primary hit points (`+0x38`), notifies the victim's tactic through its virtual `+0x128` with the attacker's tactic, and at zero hit points calls the death handler (`0x433960`), removes the object from its tactic and clears the alive flag. It never reads or writes the secondary hit points (`+0x3c`) and never changes the owner. |
| V | The Marine death handler (`0x433960`) clears the tile occupancy, sets the death animation state, drops the target reference and plays a death sound. It has no branch on the attacker and no side change. |
| V | The secondary hit points (`+0x3c`, descriptor field d4) are read only by the kill-time estimate `wep_CalcKillTime` (`0x46fac0`): when the attacker answers the Hypno predicate (`+0xf8`, descriptor class byte 2), the estimate uses the target's secondary value instead of its primary hit points. That estimate drives computer assignment decisions, not damage. |
| V | Hypno weapon record (data, not code): reload 20 ticks, base damage 3300, range 200 px, multipliers 0 against air, 1.0 against non-organic, 2.0 and 1.5 for the two Marine category flags, 0.5 against structures. |

Conclusion: the original has **no hypnosis conversion in the Hypno weapon, the damage
routine or the Marine death path**. This agrees with the headless runs (no ownership
transfer, secondary hit points never changed) and replaces the earlier open question. The
Hypno is an anti-Marine ranged attacker with doubled damage against one category and a
special kill-time estimate for the computer players. The clone should keep ordinary damage
handling for weapon 56 and implement only the secondary-HP kill-time estimate when the AI
work needs it (task 108). Buildings of the hypno family (hypno tower, hypno school) still
need their own check, because they may spawn or train units differently.
