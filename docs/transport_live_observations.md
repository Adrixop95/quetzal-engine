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
