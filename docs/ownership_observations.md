# Ownership changes of buildings in the original (outpost capture, mission 7)

Evidence date: 2026-11-08. **V** = observed in the recompiled original running headless
(sampling every 2 ticks), **G** = inference. Numbers only; no captures.

| Label | Observation |
| --- | --- |
| V | In mission 7 the Aztec outpost (type 37, 30000 hit points, secondary hit points 0) starts on side 1 with two archers (type 27, side 1) standing 48 to 96 px from it and one Jaguar and two Tuzem 48 px away. |
| V | The outpost changes owner from side 1 to the player (side 0) at tick 426 of the headless run, two ticks after the last side 1 unit within 320 px stops being listed (the last guard archer is still there at tick 424, 96 px away; at tick 426 no side 1 object is within 320 px) and when the nearest player unit (an axe-thrower truck) is 84 to 99 px from it with a dozen player units inside 200 px. |
| V | The owner stays the player's for the rest of the sampled window (to tick 500) with no player unit closer than 65 px and none standing on it, and its hit points do not change (30000). |
| V | Earlier coarse runs show the outpost going back to side 1 at about tick 4480 after the player units have left; no unit changes side in these runs, only this building. |
| G | Capture is decided by presence: the building flips to the side that has units near it when the previous owner has none (or no armed unit) nearby, and flips back the same way. The exact radius, the unit types that count and any delay are not established. |

Next step (task 142): find the ownership change in the Building class think routine (outposts
are Building types 10, 37 and 63; the code path ends in the side-list update `0x43ed50`
through virtual `+0x7c`, with the list functions `0x449ab0`/`0x449ac0`) and read the radius and
the condition. Until then the clone's outpost capture can use: flip to the side with
armed units within about 6 tiles when the owner has none within that radius.
