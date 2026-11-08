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
