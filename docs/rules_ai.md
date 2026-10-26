# Computer-player rules for a clean implementation

This specifies reference behavior, not the current `sim/ai.cpp`. **V** means code read, **G** means a plausible policy with incomplete evidence, **O** means unresolved. Evidence addresses are navigation aids for local research; this document contains no recovered implementation. Machine-readable companion: `data/rules_mechanics.json`.

The reference has several cooperating systems: side intelligence, town economy and construction, production plans, tactics controlling groups, and mission triggers. A replacement should keep those responsibilities separate. “Train a random unit every 56 ticks and attack only when scripted” is not a specification of the original AI.

## Inputs and update order

**V:** The town loader (`0x430770`) reads coordinates, side, race, hall type, scenario ID and initial objects. It does not read the `.mp` keys `Tendency` or `BuildSpeed`. Constructor defaults (`0x4594f0`) are tendency 0, speed 1, autonomous state 0, science timer 50 and no requested unit (sentinel 90). Save loading restores these fields. Do not assign behavior to unused mission keys simply because they appear in the maps.

**V:** Side update (`0x449980`) advances its elapsed tick counter and, for computer sides when AI is enabled, refreshes type-availability information, progresses side plans, refreshes town intelligence and runs side strategy. The generic object/trigger update precedes town harvest consumption in the frame loop. A town update (`0x45e8e0`) runs tactic processing, repairer processing, construction/training, capture checks, nearby unit service and then the computer strategist. Script conditions and orders are a separate input to those systems.

**O:** The complete relative order among every side plan, trigger action, spawn callback and tactic assignment still needs a live trace. Preserve deterministic order in a clean implementation and do not advertise exact reference tick parity yet.

## Town intelligence

**V:** MapExplorer (`0x432050`, `0x431e00`) maintains knowledge of town forces. It is not the fog-of-war implementation. A town must be live and have at least one visible member before it is recorded. For the observer's own town, count eligible live ordinary buildings; for other towns, count visible/live membership according to the observer branch. Also include forces from other live towns within `abs(dx)<8 && abs(dy)<8` tiles of the inspected town. Intelligence records contain a town reference and 90 type counts (`0x45ef90`).

Rebuild the record set and clear the side's known-type marks on first use and every `(side_index+1)*1000` elapsed ticks. Between rebuilds, update the existing information. The computer side's initial visibility layer is set to 24. This intelligence can therefore differ from an omniscient count of all enemy objects.

**V:** Town development level (`0x45d070`) is the sum of the type DevLevel of eligible live ordinary buildings plus queued ordinary buildings, clamped to 9. It is not just the hall level. Build-tree availability distinguishes completed town-local prerequisites, side-wide advanced-race prerequisites, queued objects and DisabledID (`0x411ac0`).

**O:** Full weighting of intelligence records when choosing an enemy town or attack composition is not established. A clean AI can use these counts as its inputs, but its chosen scoring must remain labeled G until compared with the reference.

## Production speed and spending

**V:** Every computer strategist update (`0x45e7c0`) compares estimated income with twice estimated spending. Choose normal BuildSpeed 1 if income is smaller, otherwise fast BuildSpeed 2. The comparison uses `Kprice[2]=2`; it can change while a queue is busy. The selected speed affects purchases when the hall level is nonzero. Outposts have separate price/time factors; towns hostile to everyone use index 0 instead of the selected speed.

| Speed | Index | Price factor | Time factor |
|---|---|---|---|
| Slow | 0 | float32 2/3 | 2 |
| Normal | 1 | 1 | 1 |
| Fast | 2 | 2 | float32 2/3 |

Price and time also receive x0.8 for a computer side that is not hostile to everyone. Convert using original `trunc(value+0.5)`. A fast computer town therefore charges 160 for a base-price-100 footman and computes 53 time units from a base-time-100 entry. The historical oracle reported 161 and a 56-tick interval: its floating-point conversion uses host rounding before truncation. Do not implement an extra footman charge or a universal 56-tick scheduler (`0x45c5b0`, `0x45c690`, `0x459bb0`, `0x4810e4`; local `runtime/core.c` and generated conversion inspection).

**V:** Town income estimate (`0x45c270`) combines its estimated harvesting rate with `floor((cash / number_of_towns)*256)/(elapsed_ticks+1)`. Spending estimate (`0x45c300`) uses cumulative town spending scaled by 256 and divided by elapsed ticks plus one. Starting cash influences the early decision. This is not a Difficulty modifier.

**V:** BuildThink has one payment at production start, tests affordability, advances one current construction/training object, installs completed building prerequisites, and routes finished mobile objects to the production tactic. The queue is initialized with capacity 50. Queue cancellation removes reservations; charging and refunds cannot be modeled as “pay on enqueue, always refund 100%.”

## Autonomous town state machine

**V:** Run a strategy transition only when there is no active production object and the town queue is empty. Advance the science timer each strategist tick unless it is -1. The state IDs and the following branches come from the dispatcher at `0x4ae9e8`, handlers `0x45dda0..0x45e770`. Labels below use English descriptions of their behavior.

| State | Purpose and established transitions |
|---|---|
| 0 | Wait for the side's active plan to clear, then enter 10. |
| 1 | Prepare an offensive production plan using the selected town intelligence record; success ->2, failure ->11. |
| 2 | Progress the offensive plan. If harvesting is enabled and income is below twice spending, request an economic building. On plan completion, activate the plan, release it and enter 11. |
| 3 | Attempt science/build-tree development while science timer >0; accepted request ->4; failure resets the timer and ->13. |
| 4 | After the upgrade request, ->13. |
| 5 | Choose randomly from currently available defence types, enqueue and find a placement. Accepted request ->6; placement failure cancels it, disables further defence attempts and ->0. |
| 6 | After defence construction, ->11. |
| 7 | Request a farm/warehouse, falling back to a mine; ->8 on request path. |
| 8 | After economic construction, ->11. |
| 9 | If the selected non-outpost enemy town still belongs to another side, ->1; otherwise ->11. |
| 10 | With no requested type and science timer <=100*(development+1), ->13; otherwise ->3. |
| 11 | If harvesting enabled and income <2*spending, ->7; otherwise ->12. |
| 12 | If defence attempts disabled, ->11. Otherwise count live Defence members: below development level ->5, at/above it ->0. |
| 13 | Count live non-Defence members: below 3*(development+1) ->15, otherwise ->9. This is a town force target, not a population cap. |
| 14 | Progress a local production plan, optionally request economy when income <2*spending. Plan completed ->9, attach/release plan. |
| 15 | Create a local production plan from the town's own intelligence record; success ->14, failure ->9. |
| 16 | Threatened-town branch: check for nearby enemies; while present ->21, otherwise release the plan and ->0. |
| 17 | After a threatened-town upgrade, ->21. |
| 18 | Choose/place a defence as in 5; success ->19; failure disables defence attempts and ->16. |
| 19 | After threatened-town defence construction, ->16. |
| 20 | If defence attempts enabled and Defence count <development, ->18; otherwise ->16. |
| 21 | If live non-Defence members <7*(development+1), ->23; otherwise ->20. |
| 22 | Progress threatened-town local production plan; completed ->20, attach/release plan. |
| 23 | Create threatened-town production plan; success ->22, failure ->20. |

**V:** Economy choice (`0x41e610`) prefers a warehouse when there are more than two farms, no warehouse was encountered in the town scan, and that warehouse is available; otherwise request the race's farm. The caller falls back to mine selection (`0x41e850`). Placement failure rolls back the queued reservation. Development (`0x45b5f0`) traverses missing prerequisites, scores available development paths, considers the number of recipes unlocked and randomizes ties. Hall upgrades change the existing town's hall/build tree rather than adding another town.

**O:** Exact recursive development scores, mine-versus-farm terrain ranking, production-plan composition and the entry conditions for the threatened branch are incomplete. The state table establishes control flow, not every planner policy. Do not replace these unknowns with an undocumented fixed build order.

**G:** A usable clean fallback can choose an affordable unit from the available build tree, prioritize missing economic prerequisites, and assemble a force until the verified development-based target is reached. Keep this fallback separate from V data and deterministic under the simulation seed. Exact reference attack launch sizes and timings must remain O.

## Tactics, acquisition and attack waves

**V:** Tactics own groups, paths, following/attachment state and combat assignment. Their periodic scan (`0x454820`) runs after a random interval of 15..23 ticks, around the group's expanded bounds, and checks visibility, liveness and hostility. Global assignment (`0x451c30`) takes at most 100 eligible friendly and 100 enemy combat objects. It excludes ordinary buildings from that normal candidate path, while Defence passes the discriminator. Local assignment (`0x452c70`) uses CanAttack, estimated kill times, potential allied damage and assignment weighting. It is not simply the nearest visible target.

Marine Think (`0x433db0`) asks its owning tactic for movement/target updates, drops a dead or invisible pursuit target, and drops one beyond 1152 in the reference approximate pixel norm (`0x461fe0`; formula in `rules_types.md`). A usable weapon requires positive damage against the actual target (`0x4166a0`); CanAttack also invokes the weapon's CanFire range/eligibility check (`0x43e9f0`). Air immunity is therefore weapon dependent, not a blanket air-category rule.

**V:** Autonomous town states 1/2 prepare and activate offensive plans. Mission actions such as `attackTown`, `moveTo`, following and path orders provide additional attack-wave control. A rewrite limited to script-driven attacks omits autonomous planning. Side response (`0x461a30`) can redirect eligible live tactics after town attack notification (`0x45ece0`); it does not establish the rewrite's invented “idle units within 10 tiles” rule.

**O:** Exact composition scoring and force-release thresholds need further planner reading and live verification. The 100-entry assignment arrays do not limit the number of live units on a side.

## Defence# and Camera# mission entries

**V:** `[SideN] DefenceCount` and `Defence#` are read by `0x430410`. Entries accept three integers and an optional fourth (default -1). The loader creates an object at the supplied coordinates, requires building kind with the Defence discriminator, creates a dedicated tactic, places/completes the object, attaches it to that tactic and assigns the optional scenario ID. Invalid object categories are discarded. These entries describe preplaced defensive objects; they are not build-order waypoints. They participate in tactic control and can be addressed through scenario/script state.

**V:** `CamerasCount` and `Camera#` are read separately (`0x430350`). The loader registers an indexed record through `0x446f70`; it does not create a combat unit or an attack route. The scan format admits four integer slots, but accepts the record only when three conversions succeed. Treat the fourth slot as unverified rather than assuming a four-field mission contract.

**O:** The precise meaning of the third camera value and how camera activation affects presentation/visibility need callback tracing. Camera entries must not be treated as AI attack targets based only on their coordinates.

## Conditions and triggers

**V:** A trigger has a condition list and an action list. `0x45f470` requires every enabled condition to pass (disabled conditions are skipped); an empty condition list returns false. `0x45f4e0` runs enabled actions in list order. A trigger's enabled flag gates evaluation in the generic trigger Think (`0x45f0a0`). Conditions dispatch through `0x418360`; actions through `0x402f80`. A successful evaluation does not itself make a trigger one-shot: scripts must disable it or change its conditions.

**O:** The complete condition/action dispatch catalog, initial enable state for every scenario form, and repeated same-tick enable/disable ordering are not fully specified. The clean runner's assumptions about these details remain placeholders in `engine_rules_status.md`.

## Verification and remaining work

The specification was checked against local decompilation, missing-method decompilation and x86 instructions on 2026-10-25. The old economic logs corroborate fast production and phase-dependent income, but use the lifted runtime's conversion behavior. Corrected structure/air maps were prepared; runtime execution inside `unshare -rn` aborts at UDP socket creation under this sandbox. No new duel success is claimed.

Remaining O items: a demonstrated global population limit (or proof of absence), building boarding capacity (if such a mechanism exists), full planner scores and attack release rules, camera activation, all difficulty effects outside the inspected paths, troop stat upgrades, and live timing/category regressions. A clean AI may implement G policies for these, but the data must retain their status.
