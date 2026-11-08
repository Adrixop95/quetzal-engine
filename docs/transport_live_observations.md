# Live observations of the original: ferry and squad boarding in mission 2

Measured on 2026-11-08 by driving the original Aztec.exe (Polish release, discs 1 and 2
mounted, campaign profile with progress 15) through its front-end into mission 2 and
capturing the 800x600 window. Observations only; no assets or captures are stored here.
Labels: V = observed in the original. Time values are wall clock at the default game
speed from the capture timestamps, not ticks (accuracy about 1 to 2 seconds).

| Label | Observation |
| --- | --- |
| V | At mission start two ferries lie at the shore at the lower left of the map. Both are scenery-like objects: clicking either ferry with the left mouse button selects nothing. No selection ring, no command card, no portrait change. |
| V | The top row of the side panel shows one icon per controllable group: the village and a squad (three Miotacz Toporow, axe throwers, siege units). The squad icon centres the camera on the squad and selects all three. The squad command card has four stance or command buttons (sword, shield, helmet, hand) and a slot per unit with the unit name as tooltip. A status message "Oddzial gotowy" (squad ready) appears when the squad icon is clicked. |
| V | With the squad selected, a right click on a shore ferry makes the ferry lower a wooden gangplank within 5 seconds. The squad icon disappears from the panel (the units are aboard). |
| V | Within about 17 seconds of the click the ferry has left the screen on its own; the second ferry stays. No further player order was given. The ferry departs without a sail command from the player. |
| V | Gold income continues during all of this (about 60 per 3 seconds at the start of mission 2). |

## Second run: complete timeline (timer started at the right click, accuracy about 1 s)

| Label | Time | Observation |
| --- | --- | --- |
| V | 0 s | Squad selected through its panel icon (acknowledgement text shown), right click on the right-hand shore ferry. A second acknowledgement text appears at once. The three siege units start walking from the left edge of the view towards the ferry. |
| V | about 10 to 12 s | The ferry has changed pose (sails furled, boarding) and the squad icon has disappeared from the panel: the units are aboard. |
| V | about 20 to 24 s | The ferry has left the view. A blue marker moves north east on the minimap (the ferry in transit). |
| V | about 40 s | The squad icon is back in the panel and the squad can be selected again. A red marker (own units) has appeared on the minimap at the north east end of the explored coast. Jumping the camera there shows the three siege units selected on the sand at the water edge: they landed on their own, with no unload command from the player. |
| V | after landing | The ferry is not visible near the landing site shortly after the unload; the second ferry stayed at the start shore the whole time. The ferry cannot be clicked at any point. |
| V | end | Mission clock and income kept running (gold 2000 at the start, 2240 after about 100 seconds). |

The squad panel text lines heard during the run were acknowledgement lines of the squad, not ferry texts. Camera jumps on the squad icon only happen when the squad is far from the current view.

Implications for the clone (G, to be confirmed against the lifted code): the ferry is not a
player-selectable unit; boarding is done by right-clicking the ferry with the group selected;
after boarding the ferry sails by itself (likely a script-driven or default route), so the
player never uses a ferry command card or an unload button. The clone currently makes the
ferry selectable with a sail and unload command card, which differs from the original.
Open measurements: where the ferry goes, where and when the squad unloads, whether the
player can control the squad after landing, capacity (three units boarded here).

## Third and fourth runs: shuttle cycle, return trip, who may board

| Label | Observation |
| --- | --- |
| V | The left ferry behaves exactly like the right one. A right click with the scenario squad selected makes the three siege units walk to the ferry; the ferry has left the view about 12 s after the click. |
| V | The squad icon is back in the panel about 48 s after the click (landed). |
| V | The ferry is back at its original dock, ramp closed, between 56 s and 64 s after the click, and is ready for the next request. So the ferry is a shuttle: dock, load, sail to the far shore, unload, sail back, dock. The full cycle is about 60 s. The second ferry never moved during the first ferry's trip. |
| V | A group of 12 foot and mounted units (formed by box-selecting the village units, which the game names a squad) was sent to the ferry with a right click. All 12 walked to the ferry and gathered beside it, but none boarded and the ferry did not open or leave. Selecting the village building itself selects the same 12 units and gives the same result when right-clicking the ferry. |
| V | So boarding is not open to any selected unit. The scenario squad of three siege units boards; an ad hoc group of 12 does not. Which condition rejects the 12 (group size above the capacity of five, unit type, or the requester not being the scenario group tied to this ferry) is not decided by these runs. |
| V | Camera notes: clicking the squad icon centres the camera only when the squad is out of view; edge scrolling happens when the cursor touches the screen border. |

## Original control flow read from the lifted code (V, function addresses)

- Ferry controller object fields: `+0x58` the ferry world object, `+0xf0` controller phase, `+0xf4` requesting group controller, `+0xf8` passenger count (limit 5), `+0xfc` destination controller of the passengers.
- `f_00408dd0` (boarding request, a virtual method): stores the requester at `+0xf4`, asks the requester for its destination controller through the requester's virtual method at `+0xa0` and stores it at `+0xfc`, sets phase 1 (opening) and starts the opening animation.
- `f_00408eb0` (per think): ends or ignores the ferry when its object is missing or off the map by more than 7 tiles. In phase 1 to 5 it requires the ferry object's state `0x60` to equal 3 (docked and idle) before taking passengers, scans a 3 by 3 tile block centred on the ferry, and accepts candidates that belong to the requester until five are aboard. When boarding is finished it sets phase 2 (closing). When closing ends it sets phase 3 (travel) and starts the travel animation. On arrival (object state back to 1) it calls the unload routine `f_004474d0` of the destination controller, then sets phase 5 (turn), then phase 0 (idle) after the return.
- `f_00408d50` (boat.activate script action): sets phase 3 and starts the travel animation directly, without a boarding request.
- `f_00408770` (visual think): the ferry travels by changing both pixel coordinates by five with opposite signs per think, until the next tile fails the passability test; at that point the phase flips between travel and turn, and the direction bit at `+0x68` toggles (`1 - dir`), which makes the ferry shuttle back along the same line. Direction (north east on the maps seen so far) is therefore fixed by the scenario placement and direction bit, not chosen by the player.
- Open: the eligibility predicate that rejects the 12-unit group, and the exact unit think time between pixel steps. Both need a follow-up read of the requester's virtual method at `+0xa0` and of the passenger predicate called in the 3 by 3 scan.

## Fifth run: eligibility tests (box-selected groups versus the scenario squad)

| Label | Observation |
| --- | --- |
| V | Box-selecting units around the village makes the game create an ad hoc squad with its own panel icon (acknowledgement text appears). Groups of 12, 7 (three foot soldiers and four ranged) and 5 (three cavalry and two foot soldiers) were each sent to a ferry with a right click. In every case the units walked to the shore area near the ferries and stopped. The ferry never opened its ramp, no unit boarded and the ferry never left, for at least 32 s after the click. |
| V | The scenario squad of three siege units boarded every time (four runs, both ferries). |
| V | So boarding is not decided by group size alone or by unit type alone among the tested groups: three different ad hoc groups with sizes 5, 7 and 12 and mixed types were all refused, even the group of 5 which fits the capacity of five. |
| G | Inference from the code (`f_00408dd0` stores the requester and asks it for its destination controller through the virtual method at `+0xa0`; unloading transfers the passengers to that controller): a ferry only accepts a group that has a scenario destination for it. The siege squad has one (its landing town on the far shore), ad hoc squads do not. Not yet proven: the task that implements this must compare the scenario records of the siege squad and the village soldiers (tactics and towns of mission 2 in the private scenario file) and find the field that differs. |
| V | Side effect worth copying: a refused group still walks towards the ferry area; there is no refusal message. |
