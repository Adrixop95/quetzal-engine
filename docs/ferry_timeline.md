# Ferry timeline measured in the original (headless, mission 2)

Evidence date: 2026-11-08. **V** = observed in the recompiled original under the L0 observer,
sampling every 4 ticks, mission 2, scenario squad of three axe-thrower trucks ordered to
follow the beach ferry by writing the follow order into the group tactic fields (target-mode
byte `+0x94` = 1 and target tactic `+0x98` = the ferry tactic, as the follow routine `0x451440`
does). Ticks count from the mission start; the order was written at tick 16. Pixels are world
pixels (48 per tile). The ferry tactic fields are `+0xf0` phase and `+0xf8` passenger count.

| Tick | Observation |
| --- | --- |
| 1 | Ferry at (1368, 5400), phase 0, no passengers; squad at about (744, 5352), idle. |
| 16 to 20 | The group has the follow order and starts walking (unit state 2) towards the ferry. |
| 84 | The nearest squad unit is about 114 px from the ferry (about 2 tiles) and the ferry goes to **phase 1 (opening)**: the request was sent between ticks 80 and 84, at a group anchor distance well inside the 6 tile reach. |
| 92 | 2 passengers aboard (the units that reached the 3 by 3 tile block first). |
| 100 | 3 passengers aboard, the squad is no longer in the object list (cargo), the ferry is in **phase 2 (closing)**. |
| 108 | **Phase 3 (travel)** starts. The ferry does not move for about 28 ticks (the start animation). |
| 136 to 428 | The ferry moves **+5 px in x and -5 px in y per tick** (north east) without steering: (1428, 5340) at tick 148, (2828, 3940) at tick 428, a run of 31 tiles on each axis. |
| 444 | **Phase 4 (arrival)**: passenger count back to 0 and the three units reappear on land next to the ferry at (2916, 3853), (2856, 3864) and (2808, 3864), that is in a ring 48 to 60 px from the arrival point (the ferry stopped at (2856, 3912)). |
| 452 | **Phase 5 (turn)**. The ferry waits about 28 ticks. |
| about 480 to 596 | The ferry moves back south west at 5 px per tick per axis (2796, 3972) at tick 492, (2396, 4372) at tick 572, along the same line. Phase stays 5 until it docks (not reached within the window). |

Whole trip to unloading, counted from the request: 360 ticks (84 to 444); the live game ran at
about 9 ticks per second, which matches the 40 s measured on the Windows install.

Other observations from the same runs:

- A group that is a **town garrison group** (the 13 units around the village, which includes the
  village building itself, type 11) accepted the same order fields (target mode 1, target =
  the other beach ferry) but **none of its members moved** and the ferry stayed in phase 0 for
  the whole window. The scenario field team walked at once. So the follow order is only acted
  on by ordinary field groups; groups tied to a town (garrison or defence groups) ignore it.
- The in-game mouse of the headless runtime is relative, so scripted clicks in the game
  (unlike menu clicks) do not select anything. Orders must be injected through memory writes
  or command packets.
- Ad hoc groups created by box selection in the interactive Windows session walked to the
  shore but did not trigger the request (docs/transport_live_observations.md). This run does
  not reproduce those groups; creating one headless needs the command packet route
  (`PCT_FRAME_CREATEGROUP` followed by the attack command on the ferry object).

For the clone (task 129): ferry speed 5 px per axis per tick (about 7.07 px per tick along
the diagonal), start and turn pauses of about 28 ticks, request at group anchor distance of
about 2 tiles for a team that walks in, one-way trip 297 ticks for this ferry's line,
unload ring 48 to 60 px around the arrival point, return along the same line.
