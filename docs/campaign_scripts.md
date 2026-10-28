# Campaign script audit (V12)

The 2026-10-28 read-only audit parsed every condition and action in all 15
campaign `.scn` files from a private installation. Both `Campaign/Maps` and
`Maps` produced the same inventory: 92 trigger definitions, 129 conditions and
363 action sites. Script text, objective strings and original files stay private.
These are data counts, not reference execution observations.

Run the inventory without starting SDL or a simulation:

```sh
build/aztec_script_audit --maps /absolute/private/game/Campaign/Maps
build/aztec_script_audit --maps /absolute/private/game/Maps --require-supported
```

The tool resolves mixed-case filenames, requires missions 1 through 15, parses
through the engine's own reader, and emits counts by condition/action kind.
Diagnostics identify unsupported sites by mission, trigger and source line;
they also identify repeated trigger IDs and missing trigger targets. It emits
no strings or original script bodies. Default success establishes inventory and
parser coverage. `--require-supported` additionally fails on any unsupported
site, including sites that a bounded run did not reach. Neither mode proves
objective reachability or reference semantics. `script_campaign_inventory` is
an optional read-only CTest that skips without game data; `script_audit` exercises
the CLI with invented scripts, including malformed/missing files and strict
coverage failure.

## Per-mission inventory

| Mission | Triggers | Conditions | Actions | Unsupported action sites |
| --- | ---: | ---: | ---: | --- |
| 1 | 18 | 18 | 30 | 0 |
| 2 | 9 | 10 | 50 | 3 BoatActivate, 1 SubmarineGo |
| 3 | 2 | 3 | 2 | 0 |
| 4 | 4 | 8 | 28 | 0 |
| 5 | 4 | 5 | 18 | 0 |
| 6 | 4 | 6 | 19 | 0 |
| 7 | 6 | 10 | 36 | 5 BoatActivate |
| 8 | 7 | 9 | 22 | 0 |
| 9 | 7 | 13 | 30 | 4 BoatActivate, 1 SubmarineGo |
| 10 | 3 | 4 | 9 | 0 |
| 11 | 5 | 9 | 37 | 0 |
| 12 | 4 | 5 | 18 | 0 |
| 13 | 5 | 7 | 18 | 0 |
| 14 | 9 | 10 | 23 | 0 |
| 15 | 5 | 12 | 23 | 4 JohnkaEnable, 1 SubmarineGo |

There are **19 unsupported required action sites**, across missions 2, 7, 9 and
15. Mission 6 also disables nonexistent trigger 1 at two sites. This is reported
as a data warning; the engine's existing absent-trigger disable policy is retained.
No repeated trigger definitions were found in this installation.

## Condition and action coverage

Every condition kind present in these scripts has an evaluator:

| Condition kind | Sites |
| --- | ---: |
| Always | 12 |
| LocalTime / GlobalTime / Timer | 7 / 9 / 7 |
| SideEnemies / SideTowns / SideUnits | 8 / 25 / 15 |
| SideUnitsInRect / SideUnitsOfUGIInRect / SideUnitsOfId | 7 / 3 / 3 |
| TacticAttacked / TacticValid / TacticIdle | 11 / 18 / 2 |
| IsFriendly / IsEnemy | 1 / 1 |

Every action kind is classified below. "Implemented" means there is an engine
branch, including provisional semantics; it does not mean verified reference
behavior. The runner, inventory and headless playthrough report share the same
support classification.

| Action kind | Sites | Engine status |
| --- | ---: | --- |
| TriggerDisable / TriggerEnable | 135 / 7 | Implemented |
| SideVictory / SideDefeat | 16 / 21 | Implemented |
| TacticAttackTown / TacticMoveTo / TacticJoinTactic | 29 / 10 / 13 | Implemented |
| TacticSetCycledPath / TacticSetPath | 15 / 10 | Implemented |
| TacticGiftToSide / TacticKill | 29 / 1 | Implemented |
| SideSetFriendlyTo / SideSetEnemyTo | 32 / 4 | Implemented |
| TimerSet / TimerKill | 2 / 2 | Implemented |
| CenterView / SideCameraActivate | 5 / 11 | Provisional jump; activation's second argument is ignored |
| PrintString / PrintStringR | 1 / 1 | English text displayed; alternate-language variant omitted |
| BoatActivate / SubmarineGo / JohnkaEnable | 12 / 3 / 4 | Unsupported |

BuildCommand, MoveCommand, CameraDeactivate and JohnkaDisable have no sites in
this installation's campaign. They remain unsupported grammar branches. Other
parsed methods absent from the campaign still have synthetic condition/action
controls; campaign absence must not be confused with custom-map support.

## Synthetic contract and fixes

`script_unit` uses independently invented worlds and scripts. It checks:

- Source-order evaluation, immediate forward enablement, next-tick backward
  enablement, disabling a later trigger, completing the current action list
  after self-disable, repeated conditions, repeated IDs and initialization reset.
  The initial disabled-if-enabled-by-another-trigger heuristic remains P.
- Timer 0 initialized before the first tick (value 1 on tick 1), action-assigned
  timers visible to later evaluations, stop/start without losing the value,
  kill suppressing every comparison, restart and negative values. Timer storage
  and comparisons use fixed-width 64-bit values across supported platforms.
- Every parsed condition kind, all six comparisons, inclusive rectangle edges,
  side/tactic identity, dead/removed objects and incomplete halls.
- Forward/reverse and finite/cyclic paths, waiting for idle, town attack,
  joins/locks/kills, one-way alliances, gifts, money, camera jump requests,
  messages and first-outcome-wins behavior for the mission and individual sides.
- Gift cancels the movement agent as well as the object order, and rejects an
  invalid recipient. The prior engine behavior could leave an idle gifted unit
  moving along its old route. Exact reference gift-order semantics remain open.
- Explicit defence provenance survives world loading and participates in the
  state hash. A defence is not accidentally assigned to town 0; a team tower
  with a matching scenario ID cannot satisfy `defence(N).valid`.
- Per-tick replay equality across viewing sides and replay-based save
  continuation through trigger/timer/path/gift transitions. Per-side outcomes
  are now hashed in addition to the human mission outcome.
- All seven unsupported grammar branches increment the counter and agree with
  the audit classification. No unsupported branch is replaced by a silent no-op.

These checks establish engine consistency. They do not prove trigger timing,
pathfinding, gift identity, transport behavior or mission outcomes against the
reference. Camera activation duration/locking and foreign-side presentation
remain unresolved; the current jump request alone does not fulfill V12.

## Remaining dependencies and evidence

V02's aligned reference observer, V05's water/transport movement, V10's command
semantics and V11's validated AI behavior are not complete in this checkout.
The independent inventory, engine consistency fixes and synthetic fixtures can
precede those adapters. Boat/Johnka/submarine routes, boarding/unloading and
activation phases must be measured before implementing those actions. Build and
move command argument/selection rules also need evidence before an adapter to
player commands is justified. Required actions remain unsupported explicitly.

A control reference run used a private writable game copy, read-only resource
symlinks, both SDL dummy drivers, a hard timeout and `unshare -rn` through
`tools/zstat_run.sh`. It failed at `ip link set lo up` with `Cannot open netlink
socket: Operation not permitted`; the runtime never launched. Private inputs,
script and failure output are under ignored `work/v12-reference-game/` and
`work/v12-reference-probe/`. No reference record/replay or objective validation
was obtained. The two inventory reports are in ignored `work/v12-*-inventory.*`.

V12 stays unchecked until required action coverage reaches zero, reference order
and timing observations are available, and mission-specific objective scenarios
exercise paths, alliances, gifts, defence identity and victory/defeat. A parser
pass, a deterministic replay, or a bounded timeout is insufficient evidence.

## Validation in this worktree

Release configure/build passed with `AZTEC_GEN_DIR=` and a ccache directory
inside the worktree. The full CTest run passed 19 entries and skipped five:
formats, engine, original-save private import, campaign inventory (no configured
installation), and real UDP lockstep processes (socket access denied).
The read-only campaign inventory was then run with the private installation and
passed separately. Both campaign directories produced identical TSV counts;
the strict action gate returned status 1 for the 19 unsupported sites.
The final script unit and audit CLI checks also passed after the last fixture
change. All pre-commit hooks passed for tracked and new files using copied local
hook caches; network fetching was unavailable. `git diff --check` passed.
The reference probe failure above remains a failure, not a skipped parity check.
