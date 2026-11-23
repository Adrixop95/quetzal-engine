# Measured Rifleman shot period (headless oracle)

Status: M (measured, one run, three gaps). Superseded in its conclusion: see the resolution at the end of this document.

Setup: the recompiled original binary was run headless under `tools/parity_run.py`. A Rifleman
and a stationary Bear on the opposing side stood within range on open tiles, and the hit
events (hit point drops of the Bear) were logged per logic tick. The original game was not run
on a desktop. Scenario and logs are private research material.

Result: the Bear lost 1600 hit points at ticks 151, 178, 204 and 231, so the gaps between
consecutive Rifleman shots were 27, 26 and 27 ticks.

Relation to the rules documents: `docs/animation_timing.md` and `docs/unit_state_machines.md`
give a shot period of 21 ticks (reload 20 plus 1). That value was derived as the reload term
only. `docs/unit_state_machines.md` also states, with a G label, that the period is the larger
of the reload term and the attack animation length plus one tick for re-entry. The measured
26 to 27 ticks is above the reload term, so the animation branch of that rule is the more
likely one for the Rifleman, which would mean the attack animation (seq 9 to 16, 18 frames)
lasts about 25 to 26 ticks. This is an inference and has not been checked in code.

An earlier, uncontrolled run against moving targets also showed a constant gap of 27 against a
Zeppelin and gaps of 22 to 30 against moving Bears and a Demolisher, which is consistent
with a period near 27 with some start and stop effects.

What this does not show:

- Only the Rifleman against one stationary target type was measured; three gaps in one run.
- The shooter is identified by elimination (the only armed enemy unit on that side); the Bear
  also hit the Rifleman (gap 34) and killed it at tick 239.
- Other Marine types (Footman 10, Demolisher 31, Archer) were not measured.
- Consequence for the clone: a Rifleman shot period of 21 ticks is probably too fast. Before
  changing the clone, confirm the animation length per tick from the animation tables.

## Follow-up (task 162)

The animation hypothesis above is not supported by the clone data. `docs/shot_period_implementation.md`
derives an attack animation length of 10 logic ticks for the Rifleman, so the explicit maximum
rule still gives 21 ticks and does not explain the measured 26 to 27. The difference of about 5
to 6 ticks stays unexplained. Candidates that nothing has checked yet: a target acquisition or
re-aim delay between shots, a re-entry step through the idle state, or a different reload
value in the original than in the clone tables. The clone keeps 21 ticks until this is resolved.

## Resolution (task 173)

The 26 to 27 tick gaps are explained in `docs/shot_period_implementation.md` (section on the
Rifleman discrepancy). The Bear in the measured run was not stationary: it changed position
during the interval, so the Rifleman returned to stand after each attack and waited 5 to 6
ticks for a tile-completion step before the next attack could start. The stationary period of
21 ticks and the 10 tick attack animation stand. The statement above that the target was
stationary is wrong, and a constant of 26 to 27 ticks must not be used for a stationary
Rifleman. The explanation is inferred from the code and the logged frames; a measurement
against a target that really does not move would confirm it.
