# Mission script implementation

Evidence: [condition and flow survey](script_conditions_flow.md),
[command survey](script_gift_command.md), and
[tactic lookup survey](script_codes_and_ai_purchase.md). **V** means a rule
established by those static inspections or an explicitly identified engine
construction test. **G** means an approximation or unconfirmed integration
choice. No reference executable was run for this task. Synthetic tests and
clone replay agreement do not establish original-game parity.

The independent implementation is `src/core/script.*` and
`src/engine/sim/script_runner.*`. Original scripts and private experiment output
stay outside tracked files.

## Trigger and timer rules

- **V, static rule implemented:** scripts are flat trigger lists. Each trigger
  starts enabled with a false latch. On an enabled update it executes all actions
  if its previous latch is true, then replaces the latch with the short-circuit
  AND of its conditions. An empty condition list is false. Actions repeat every
  enabled tick while latched; a condition becoming false does not cancel the
  already latched action pass. Self-disable completes the action list and the
  final evaluation. A disabled trigger neither executes nor updates its latch.
- **V:** enable/disable addresses the first trigger with the requested ID.
  Duplicate IDs have independent enablement, latches and execution traces;
  missing IDs do nothing. A forward enable can affect this pass, whereas a
  backward enable takes effect at the next update of that trigger.
- **G:** file order substitutes for native pool traversal order. Existing timers
  increment before triggers; timers created during a trigger pass first increment
  on the next tick. The relative native timer/trigger pool phase remains open.
- **V, static rule implemented:** a timer starts running at zero on its first
  evaluated condition or set/stop/start action. There is no special timer zero
  and no eager scan of conditions. Short-circuiting does not create skipped
  timers. Set preserves the stop flag, stop pauses, start resumes, and kill
  removes without creating. A subsequent condition recreates a killed timer.
  The counter uses explicit signed 32-bit wraparound without C++ signed overflow.
- **V:** comparison operator values are `== 0`, `> 1`, `>= 2`, `!= 3`, `< -1`,
  `<= -2`. Counter comparisons are signed. **V:** localTime is a day clock
  initialized from mission StartTime and wrapping at 7200. **G:** the clone
  advances it once per active script tick and maps globalTime to active elapsed
  ticks; the native frame/day/timer clock ratios and phase are unmeasured.
- **V, construction:** gameplay hashes include each trigger's latch and
  enablement, timer existence/value/pause, both clocks and tactic state. Diagnostic
  condition snapshots are read-only; previewing a missing timer as zero does not
  create it. Only actual evaluation in an enabled trigger creates it. Replay
  reconstructs latches and timers; older engine saves can fail their hash check.

## Condition coverage

The public grammar exposes 20 of the native dispatcher's 22 condition codes.
The remaining two have no identified script producers.

| Native codes | Engine behavior and evidence boundary |
| --- | --- |
| 00 | **V:** always true |
| 01 | **V:** compare side money |
| 02, 03 | **G:** count distinct living group identities and complete living halls as proxies for native tactic/town lists; empty tactic lifetimes and duplicate scenario IDs are not represented faithfully |
| 04 | **V:** count hostile active sides, rather than hostile objects. **G:** living groups, mobile units or halls stand in for native list counters |
| 05, 06 | **G:** living non-structure and per-type counts; embarked living units retain side identity |
| 07, 08, 09 | Day/global clock and lazy timers as described above |
| 0a, 0b | **V:** missing lookup is false regardless of operator. **G:** idle means all members idle with no pending path; attacked uses the existing persistent side notification adapter |
| 0c, 0d | **V:** native side-plan complete / no-plan checks. **G / missing dependency:** no independent side-plan model or identified script grammar; these are not exposed or claimed implemented |
| 0e, 0f, 10 | **V:** compare directional friendship, neutrality or hostility |
| 11, 12, 15 | **V:** missing valid compares zero; missing locked returns false. Plain team lookup requires reservation; town lookup is the fallback. **G:** defence provenance and numerical group identities approximate native objects |
| 13, 14 | **V:** sort both rectangle axes and count living runtime categories 0, 1, 6, 7, optionally restricted by UGI. These include ordinary units, structures, boats and Johnkas, and exclude cameras; they are not construction-class numbers. **G:** inclusive tile bounds substitute for the uninspected native spatial-query edges |

Additional **V** static check: category accessors at `0x402430`, `0x45f030`,
`0x4083b0`, `0x42d930` and `0x402a40`, checked through the documented unit,
structure and vessel-object vtables, distinguish ordinary objects from cameras.
The initial construction-class filter was rejected because it made an existing
boat objective fire immediately. Synthetic rectangle regressions cover all
construction families without copying any mission fixture.

## Action coverage

The table describes independent behavior, not a copied dispatch table. Native
codes identify the rules already documented in the linked surveys.

| Code | Action | Implementation / remaining boundary |
| --- | --- | --- |
| 00, 01 | victory, defeat | **V:** change a side only while playing; first result wins. Only the mission human side ends the local mission, independent of viewing side |
| 02, 03 | attackTown, moveTo | **G:** whole-group attack-move via existing orders; native tactic hierarchy, target selection and coordinate details remain incomplete |
| 04 | joinTactic | **V:** requires both source and target lookup. **G:** reassigns flat member group IDs; town-specific virtual behavior and child-group nesting are incomplete |
| 05, 06, 07, 08 | timer set, stop, start, kill | **V:** implemented lifecycle above |
| 09, 0a, 0b, 0c | path, cycled path, inverted path, cycled inverted path | **G:** existing idle-gated waypoint follower with direction/cycling state in hashes; native route-controller timing unmeasured |
| 0d, 0e, 0f, 10 | boat activate, submarine go, Johnka enable, Johnka disable | Existing vessel adapter, **G** for native transport/controller/effect parity; see [transport follow-up](campaign_scripts.md#transport-follow-up-2026-11-04-needs-live-run) |
| 11, 12 | side buildCommand, moveCommand | **G / missing dependency:** still explicitly unsupported and counted as such. The town AI planner does not provide the native general side plan. Native build creates/replaces a plan from available type requests; move checks plan completion. The existing parser's one-type/integer-arguments approximation is not a complete native build grammar. No campaign site in the supplied inventory exercises these actions |
| 13, 21 | camera activate, deactivate | Existing stationary vision source and latest-source deactivation. **G:** lifetime/update phase and exact native camera integration; see [presentation contract](mission_script_presentation.md) |
| 14 | centerView | Tile-origin view request; **G:** independent viewport clamping/scaling details |
| 15, 16, 17 | friendly, neutral, enemy | **V:** directional relation changes |
| 18 | giftToSide | **V:** plain-group gift allocates a separate receiver group, even with a colliding donor scenario ID. Town gift uses the shared town ownership transfer, including members, owner lists and production ownership. **G:** fresh numeric identity substitutes for native group/follow hierarchy; pending town production cancellation/refund and child-group resets remain approximations |
| 19 | printString | **V:** 16 ring slots leave capacity for 15 visible prints; insertion evicts the oldest when full. Deadline is 4000 ms of presentation time, independent of simulation ticks. Existing draw retirement removes at most one expired entry per second. UTF-8 presentation and exact layout remain **G** |
| 1a, 1b | lock, unlock | **V:** reserved teams are script-addressable; unlocking makes a plain team unreachable through that ID, including subsequent lock, kill or gift. Town fallback remains addressable. **G:** placed groups initially reserve their scenario IDs; map Locked fields and AI reuse of explicitly unlocked groups need fuller tactic integration |
| 1c, 1d | addMoney, takeMoney | **V:** signed 32-bit addition/subtraction floored at zero; explicit wraparound avoids undefined signed overflow |
| 1e | kill | **V:** kills all located members with no damage/attacker notification. **G:** native recursive child-tactic flags have no independent hierarchy equivalent |
| 1f, 20 | trigger disable, enable | **V:** first matching trigger only, preserving latch |

**V:** printStringR is consumed and discarded in the inspected language revision;
it does not add a second message action. The parser remains permissive about
empty action lists and requires an explicit conditions/actions section; complete
native parser error and optional-section compatibility is not claimed.

## Verification

`script_unit` uses invented worlds and scripts to check initial false latches,
one-tick delay, repeating actions, empty/AND conditions, self-disable,
forward/backward enables, frozen latches, duplicate IDs, lazy timer lifecycle,
stopped sets, kill/recreation, signed overflow, day wrap, directional relations,
rectangle normalization/class filtering, separate gift groups, town ownership,
locked lookup, missing join targets, scripted kills, outcome precedence, message
capacity/deadlines, paths and command-log replay. Presentation-time expiry remains
independent of gameplay hashes. Integration checks cover result UI, forced-world
probes, transport activation and headless save continuation with the extra tick.

Private verification uses the user's existing mission files with immutable asset
symlinks, ordinary private Maps/Saves directories, dummy SDL, timeout and
`unshare -rn`. Campaign inventory success means parser/action coverage, not
objective reachability or original-game parity. The private campaign contains
15 scripts, all parsing with zero unsupported action sites; mission 6 has two
missing-trigger-target warnings. Live reference validation is intentionally not
performed under this task's prohibition. General side plans and native tactic
hierarchies remain dependencies rather than invented successful no-ops.

Validation on 2026-11-22: Release configure/build and all 79 registered CTest
entries succeeded, with six default skips (private data/namespace-dependent
checks). Pre-commit passed for tracked files and this new document. Separate
isolated private format, engine integration and strict campaign inventory checks
passed. Inventory retained the two mission-6 missing-target warnings.

The final bounded campaign run covered all 15 missions twice through 600 ticks,
with no player orders, then resumed each mission from a tick-300 save. Every
paired tick hash and all 15 save continuations matched; there were no process
failures or unsupported action executions. All missions reached the 600-tick
budget still running. The campaign harness therefore returned its unmet-victory
status (1); this is a repeatability/smoke result, not campaign completion or
reference parity. Captures and reports remain under ignored
`work/script-semantics/`.
