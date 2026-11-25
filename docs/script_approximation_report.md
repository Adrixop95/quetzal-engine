# Mission script approximation report

Audit: 2026-11-25. Counts cover parsed sites across all fifteen campaign
missions, including dormant triggers. They count source sites, not executions.
Only aggregate counts are published; scripts, mission breakdowns and reports
remain private. **V** denotes inspected reference behavior; synthetic checks
verify the independent implementation, not reference parity.

## Ranked aggregate inventory

The baseline Release sweep passed all 15 missions through 720 ticks and
reported **255 approximate sites**. After the clock and transport dispatch
corrections, **224 sites** remain approximate, a reduction of **31** (16 clock
and 15 transport sites). Unsupported execution sites remain zero; the intentional parser no-op count remains one. Partial controller fixes
retain approximate dispositions wherever native integration is still missing.

| Rank | Section | Native code | Kind | Before | After |
| --- | --- | --- | --- | --- | --- |
| 1 | action | 02 | TacticAttackTown | 29 | 29 |
| 2 | action | 18 | TacticGiftToSide | 29 | 29 |
| 3 | condition | 03 | SideTowns | 25 | 25 |
| 4 | condition | 11 | TacticValid | 18 | 18 |
| 5 | action | 0a | TacticSetCycledPath | 15 | 15 |
| 6 | condition | 05 | SideUnits | 15 | 15 |
| 7 | action | 04 | TacticJoinTactic | 13 | 13 |
| 8 | action | 0d | BoatActivate | 12 | 0 |
| 9 | action | 13 | SideCameraActivate | 11 | 11 |
| 10 | condition | 0b | TacticAttacked | 11 | 11 |
| 11 | action | 03 | TacticMoveTo | 10 | 10 |
| 12 | action | 09 | TacticSetPath | 10 | 10 |
| 13 | condition | 08 | GlobalTime | 9 | 0 |
| 14 | condition | 04 | SideEnemies | 8 | 8 |
| 15 | condition | 07 | LocalTime | 7 | 0 |
| 16 | condition | 13 | SideUnitsInRect | 7 | 7 |
| 17 | action | 14 | CenterView | 5 | 5 |
| 18 | action | 0f | JohnkaEnable | 4 | 4 |
| 19 | action | 0e | SubmarineGo | 3 | 0 |
| 20 | condition | 06 | SideUnitsOfId | 3 | 3 |
| 21 | condition | 14 | SideUnitsOfUGIInRect | 3 | 3 |
| 22 | action | 05 | TimerSet | 2 | 2 |
| 23 | action | 08 | TimerKill | 2 | 2 |
| 24 | condition | 0a | TacticIdle | 2 | 2 |
| 25 | action | 19 | PrintString | 1 | 1 |
| 26 | action | 1e | TacticKill | 1 | 1 |

The ranking retains baseline order. All other parsed codes had zero baseline
approximate sites. The condition LocalTime contributes seven closed sites and GlobalTime nine. Action codes
and condition codes are separate namespaces. Counts use the corrected native
mapping: setCycledPath is 0a and setInvertedPath is 0b. The earlier smoke name
array swapped these two labels; fixing the mapping changes no site total.

## Verified behavior implemented

- **V clocks:** initialization clamps StartTime to 0..7199, rather than taking
  it modulo 7200 (`0x418380`). LocalTime advances before trigger evaluation
  on the first and subsequent active updates and wraps at 7200 (`0x4184a0`,
  `0x4579bf`). Reinspection includes both tileset branches: the older claim
  that the alternate tileset freezes the clock was incomplete. Simulation
  daylight and script clocks now advance consistently. Single-player
  GlobalTime advances after the script pass (`0x45864c`), so the first
  condition evaluation sees zero. Multiplayer lockstep is outside this adapter.
- **V idle predicate / partial controller integration:** the shared group
  controller distinguishes inactive 0, stopped 1, waiting 2 and moving 3. Script
  idle reads states 0/1, regardless of whether individual members are moving
  or attacking (`0x417d60`). State 0 supplies no member-step callback. Successful
  paths set moving; moving controllers stop on exhaustion or failure. Failed
  paths preserve other states. Path progression uses controller idle rather
  than member orders.
  Waiting readiness still uses independent agent state; native readiness flags,
  path budgets and complete controller lifetime remain unresolved.
- **V attack-town selection / partial action integration:** locate the first
  town controller with the scenario ID, independently of hall object scenario
  IDs; ignore a town belonging to the acting side and use its hall tile
  (`0x401ae0`, `0x45c090`). Friendship does not prevent a foreign destination.
  The action dispatches through the existing shared anchor/path controller.
  Town-source launch and native nested controller ownership remain unresolved.
- **V plain-group join / partial action integration:** keep the source identity
  while following the target; merge only when the anchor distance is strictly
  less than the source reach (`0x452990`, `0x453570`). Use the documented tile
  norm and mode reach, follow a moving target, and redirect merging followers
  when their target is absorbed. An independent identity guard rejects cyclic
  merge chains. Town-source recruitment and full native hierarchy remain open.
- **V trigger timing, retained:** execute the previous true latch, then evaluate
  the short-circuit condition list (`0x45f0a0`). Actions fire one update late
  and repeat each enabled update while latched. Self-disable finishes the
  action list and evaluation; disabling preserves the latch. Existing synthetic
  tests already covered this contract, so no fired-once flag was added.

The [gap list](script_runtime_gap_list.md) records each corrected subrule and
keeps the remaining gaps explicit. General side plans, town-source recruitment
and launch, nested native ownership, empty controller lifetime and exact member
readiness are dependencies absent from this checkout. The independent base-group
and clock changes do not require them. This report does not claim that all 255
sites now match native behavior.

## Validation

Baseline: Release build, all 85 CTest entries (79 passed, six skipped),
and all pre-commit hooks passed before edits. Private smoke used a fresh
installation/profile per mission, dummy SDL drivers, timeout and `unshare -rn`.
No original executable or reference runtime was launched.

New invented-script checks cover conflicting member/controller idle states,
missing tactics, first/duplicate/own/friendly town selection, strict merge reach,
movement before absorption, follower redirection, cycle rejection, hashes and
replay continuation. Clock controls cover both tilesets, first-evaluation phase,
one-update action delay, day wrap and extreme signed StartTime values. Existing
trigger repeat/frozen-latch tests remain active. Replay semantics advance to 16;
older incompatible replay saves are rejected by the existing version gate.

Final Release build and all 85 CTest entries succeeded: 79 passed and six
private-data/network entries skipped. Separate private format and engine-data
checks passed with fresh installation copies, dummy SDL and `unshare -rn`.
The final 720-tick sweep passed all 15 missions and confirmed 239 approximate
sites, zero unsupported sites and one intentional parser no-op. The retained
manifest matches the final binary and gap-list hashes. Pre-commit passed for
tracked files and this new report. Pinned hashes from the invented engine
fixture were updated for the explicit group state and joining flag in replay
semantics 16; these are independent-engine expectations, not native captures.
Native Windows/macOS CI and live reference parity were not verified here.

## Transport dispatch follow-up (task 180, 2026-11-25)

The twelve BoatActivate and three SubmarineGo sites now have closed action
contracts in the [gap list](script_runtime_gap_list.md). Task 159 already supplies
the independent boat states, phases, loading scan, landing rings and cargo-loss
rules. The action adapter selects the last matching scenario, forces immediate
boat travel without changing direction, or replaces the stationary submarine
loop with its finite departure effect and retires its controller. A missing or
retired target silently does nothing, as the native handlers specify.

New invented-script controls exercise complete passenger trips and boat loss,
including count/rectangle/tactic gates and delayed victory/defeat, activation in
every phase, duplicate lookup, submarine lifetime and replay. The campaign-site
check now uses ScriptRunner dispatch for every action. These close fifteen
action sites, not all nineteen historical transport sites: four Johnka
sites remain approximate. General script conditions retain their dispositions;
there is no separate boat-arrived or submarine-arrived condition in the grammar.

Shared formation, shore routing, collision/placement and exact tick scheduling
boundaries remain explicit in the gap list and task-159 specification. No live
reference parity or native Windows/macOS result is inferred from these tests.
Replay semantics remain 16: the runtime correction removes presentation warnings
only; transport state transitions and gameplay hashes are unchanged.

Validation: baseline and final Linux Release builds passed. Both full CTest runs
completed all 85 entries with 79 passes and six expected private-data/network
skips. Pre-commit passed. Separate private format/engine-data checks and all 19
campaign trigger-dispatch controls passed. The final clone-only sweep completed
all 15 missions at 720 ticks, confirming 224 approximate sites, zero unsupported
sites and one intentional parser no-op. The retained manifest matches the final
binary and gap-list hashes. Private checks used installation copies with immutable
asset symlinks, private profiles, dummy SDL, timeouts and `unshare -rn`; outputs
remain ignored under `work/transport180/`. No original executable or reference
runtime was launched. Native Windows/macOS CI was not run in this worktree.
