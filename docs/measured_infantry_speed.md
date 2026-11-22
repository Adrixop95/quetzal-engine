# Measured infantry walking speed (headless oracle)

Status: M (measured). Method: the recompiled original binary was run headless under
`tools/parity_run.py` with a synthetic scenario that gives each unit type one move order along
a row; the per tick position of every unit was logged and the median pixels per logic tick over
the ticks in which the unit actually moved was computed. The original game was not run on a
desktop. The scenarios and logs are private research material and are not part of the repository.

| Unit | Median px per tick | Value in the rules documents | Match |
|---|---|---|---|
| Rifleman | 6.0 (two independent scenarios, 212 and 237 moving ticks; mean 6.07) | 6 | yes |
| Footman | 9.0 | 9 | yes |
| Archer | 8.0 | 8 | yes |
| Bear | 5.0 | 5 | yes |

What this does not show:

- Only straight movement along one row on open ground was measured; no turning, no obstacles,
  no formation walking.
- Other unit classes (vehicles, cavalry, air units, boats beyond the ferry measurement in
  `docs/ferry_timeline.md`) were not measured here.
- In scenarios that start units on map rows 12, 20 or 28 nothing moved at all, including the
  Rifleman. The start tiles of those rows read as 0 in the scenario passability layer (with the
  convention of the private scenario generator that 0 means blocked; not checked against the
  engine), while the row 4 start tile is open. So measurements use row 4.
- The observation is of the headless build; it is not a measurement of the retail executable on
  a desktop.
