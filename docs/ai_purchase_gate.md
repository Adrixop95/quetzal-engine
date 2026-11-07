# AI purchase gate investigation (2026-11-07, incomplete)

The recorded early purchases do **not** establish that development should include
units or defences. The original also rejects the local force gate in state 13,
then buys through a different production-plan branch. A trial replacement policy
made the clone spend, but failed timing, prices, composition and sustained growth.
The trial was reverted. **No gameplay fix or purchase-parity claim is delivered.**

## Inputs and limits

Checkout: `2fdf155`. Read [early_payment.md](early_payment.md),
[rules_ai.md](rules_ai.md), the task-96/97 prompts and the production/harvest
run sheet before interpreting these results. Task 96 supplies integer production
work and neutral-computer activation, but explicitly leaves side-plan allocation
unfinished. Task 97's run sheet is a pending measurement protocol, not a merged
solution to that allocation. These missing semantics remain dependencies here.

Only recorded original observations were used. No reference executable or
executable-derived source was run or inspected. Private inputs, relative to
`/home/adrox/aztec-re/work/`, were:

- `parity/d3_092158_dense_l0{,_mission_2,_mission_3}/reference/observations.jsonl`
  and the earlier `dn_213657_*` streams: dense ticks through 24, then
  30, 36, 48 and 60; the newer streams expose planner fields.
- `parity/dev_212738_private_l0{,_mission_2,_mission_3}_{a,b}/reference/observations.jsonl`:
  independent archived captures at ticks 1, 6 and 12. Money and early queued
  types agree at those checkpoints. They do not repeat every dense transition.
- `parity/long23_075041_m{2,3}/{reference,clone}/observations.jsonl`:
  22 checkpoints ending at **1600**, with 100-tick gaps for much of the run.
  The corresponding `dense_scenarios/` and `long_scenarios/` files describe
  schedules, not additional observations.

All 17 supplied reference observation streams were checked for their terminal
tick. None extends beyond 1600. Exact purchase times/types throughout 2000 ticks,
and original checkpoint values at 2000, cannot be recovered from these inputs.
Sparse net money changes cannot identify every intervening payment or harvest.

New clone runs and the rejected candidate are private under this worktree's
ignored `work/ai_purchase_gate/`. They are new measurements, not copies of the
archived clone results. The current clone differs from those older results.

## What the original actually does early

Town numbers below are mission town indices, mapped using their hall type and
position, not pool iteration order. Queuing is distinguished from payment.

| Mission | Side | Town | Recorded first request/payment |
| --- | --- | --- | --- |
| 2 | 1 | 0 | Mage 28 queued by tick 5; unpaid through tick 60 |
| 2 | 2 | 3 | Footman 0 queued at tick 6; pays 67 at tick 7 |
| 2 | 2 | 2 | Hall upgrade 19 waiting by tick 60; no early payment |
| 3 | 1 | 3 | Archer 27 starts at tick 6; pays 129 |
| 3 | 1 | 4 | Archer 27 queued at tick 6; starts at tick 7; pays 129 |
| 3 | 2 | 0 | Footman 0 queued at tick 6; starts at tick 7; pays 100 |
| 3 | 2 | 1 | Footman 0 queued at tick 6; starts at tick 7; pays 67 |
| 3 | 2 | 2 | No early queued or active purchase |

Thus “every AI town buys one unit” is contradicted by the town-level records.
Mission 3 side 1 additionally starts a farm 39 in town 3 and a mine 48 in town 4
between ticks 48 and 60, paying 193 and 121. The side's combined drop of 314
is two economic purchases, not a single unit or a research payment.

The 129 archer price retains the previously documented reference-runtime
conversion discrepancy: the clone's independent pricing model gives 128.
This investigation does not justify adding a one-gold surcharge.

### State 13 is not the original's purchase entry path

In mission 3, both side-1 towns and the developed side-2 town have these recorded
post-update strategist states:

| Recorded tick | 1 | 2 | 3 | 4 | 5 | 6 |
| --- | --- | --- | --- | --- | --- | --- |
| Phase | 10 | 13 | 9 | 1 | 2 | 2 |

The two side-1 development diagnostics are 2 and 1 for mission towns 3 and 4;
the developed side-2 town reports 0. The outposts instead take
`10 -> 13 -> 9 -> 11 -> 12 -> 0`. Nevertheless town 0 receives its footman
request while on that path. Its request therefore needs another planning source.
Mission 2's purchasing side-2 town also takes the offensive path through 9/1/2;
its reported development is 0. Mission 2 side 1 takes 13/15/14 and queues its mage
without sufficient money to start it.

Mission 3 membership diagnostics provide a separate allocation observation:
between ticks 4 and 5, types 27, 28 and 29 leave both side-1 town tactics.
For town 3 their counts change from 8/2/4 to 0/0/0; for town 4 from 10/4/4
to 0/0/0. Types 30/31/32 and ordinary buildings remain. This movement happens
after the recorded force-gate decision; it cannot explain that earlier branch
by simply reducing its force count. A unit's production town and its current
tactic membership are different concepts.

The clone currently conflates these responsibilities: its offensive plan uses
`3 * (development + 1) - available_units`, and its target predicate requires
hostility. The original neutral side also enters phase 1, so neutral diplomacy
does not prevent that planning transition. These observations establish missing
plan behavior; they do not reveal its complete scoring, composition or arbitration.

Counting all unit/defence development towards the clamp would change the original
phase path to 13/15/14 and produce a local target of 30. That hypothesis is
inconsistent with the recorded development diagnostics and phase path. Counting
only buildings as forces would likewise select the wrong branch in these towns.

## Rejected independent candidate

A temporary three-part policy was tested: allow non-allied neutral target towns,
restrict the offensive branch to developed source towns, and request at least
one unit even when the existing available army exceeds the offensive target.
The minimum-one rule was an explicit fit, not a recovered original rule.
The development calculation and original force threshold were unchanged.

Both missions completed 2000 clone updates with the trial. The trial was rejected:

- Mission 2 town 3 paid for a footman on simulation tick 7, first visible in
  the before-commands observation at tick 8, for **160**, versus the original's
  tick **7** and price **67**. The clone work rate produced a 54-update job;
  the original's active record at tick 100 still has accumulated work 2820
  at increment 30. Fixing only the planner cannot reconcile that discrepancy.
- Mission 3 both side-1 archer debits first appear at tick **8**, at **128**
  each, versus ticks **6/7**, at **129** each. The outpost footman remains absent;
  the developed town pays **160**, versus **67** in its original record.
- The trial starts the side-1 farm/mine on simulation tick 58, but this timing
  proximity does not establish the subsequent economic policy. At tick 1000,
  mission 3 side 1 has **67** objects and **39** gold versus **71/52** in the
  original; side 2 has **97/120** versus **92/1285**. At tick 1600 the trial
  gives **72/1124** and **98/600**, versus **82/675** and **95/64**.

No candidate change remains in `planner.cpp`. Making the first purchase happen
is insufficient evidence to retain a policy that fails the requested series.

## Checkpoint comparison of the unchanged clone

Each cell is **money / all live objects / mobile units**. Mobile units here are
types classified as Marine, Spider, Headtruck, Truck, Cannon, Bird, Zeppelin,
Boat or Johnka in `docs/data/rules.json`; buildings, defences, cameras and unknown
virtual types are excluded. These are observed physical objects, not a population
limit or a count of completed jobs. Active training objects may be outside the
observed physical pool. Side 3 has zero money and objects throughout.

Original values use the archived long streams. Clone values use the fresh
unchanged runs described below. A dash means unavailable, with no extrapolation.

| Mission | Side | Tick | Original | Clone |
| --- | --- | --- | --- | --- |
| 2 | 0 | 100 | 2000 / 20 / 19 | 2000 / 21 / 19 |
| 2 | 0 | 500 | 2060 / 20 / 19 | 2060 / 21 / 19 |
| 2 | 0 | 1000 | 2180 / 20 / 19 | 2180 / 21 / 19 |
| 2 | 0 | 2000 | — | 2360 / 21 / 19 |
| 2 | 1 | 100 | 16 / 66 / 38 | 0 / 66 / 38 |
| 2 | 1 | 500 | 170 / 66 / 38 | 26 / 67 / 39 |
| 2 | 1 | 1000 | 430 / 66 / 38 | 30 / 69 / 41 |
| 2 | 1 | 2000 | — | 16 / 73 / 41 |
| 2 | 2 | 100 | 433 / 18 / 16 | 500 / 18 / 16 |
| 2 | 2 | 500 | 295 / 21 / 18 | 590 / 18 / 16 |
| 2 | 2 | 1000 | 315 / 22 / 18 | 770 / 18 / 16 |
| 2 | 2 | 2000 | — | 1040 / 18 / 16 |
| 3 | 0 | 100 | 3500 / 30 / 30 | 3500 / 30 / 30 |
| 3 | 0 | 500 | 3500 / 30 / 30 | 3500 / 30 / 30 |
| 3 | 0 | 1000 | 3500 / 30 / 30 | 3500 / 30 / 30 |
| 3 | 0 | 2000 | — | 3500 / 30 / 30 |
| 3 | 1 | 100 | 147 / 58 / 44 | 701 / 54 / 42 |
| 3 | 1 | 500 | 420 / 62 / 44 | 1043 / 54 / 42 |
| 3 | 1 | 1000 | 52 / 71 / 44 | 220 / 58 / 42 |
| 3 | 1 | 2000 | — | 761 / 59 / 42 |
| 3 | 2 | 100 | 833 / 87 / 81 | 1000 / 87 / 81 |
| 3 | 2 | 500 | 1029 / 90 / 84 | 610 / 88 / 81 |
| 3 | 2 | 1000 | 1285 / 92 / 86 | 1000 / 88 / 81 |
| 3 | 2 | 2000 | — | 610 / 88 / 81 |

At the last shared checkpoint, tick 1600:

| Mission | Side | Original | Clone |
| --- | --- | --- | --- |
| 2 | 0 | 2300 / 20 / 19 | 2300 / 21 / 19 |
| 2 | 1 | 63 / 69 / 39 | 72 / 71 / 41 |
| 2 | 2 | 307 / 24 / 18 | 950 / 18 / 16 |
| 3 | 0 | 3500 / 30 / 30 | 3500 / 30 / 30 |
| 3 | 1 | 675 / 82 / 51 | 1151 / 58 / 42 |
| 3 | 2 | 64 / 95 / 89 | 280 / 88 / 81 |

The original mission-2 side-2 growth from 18 to 24 objects is two footmen and
four farms, not six additional army units. Mission 3 side 1 grows from 54 to 82
objects, but only from 42 to 51 mobile units. A force-target adjustment fitted to
aggregate object growth would confuse economic construction with army production.
The unchanged clone also currently contains the mission-2 type-minus-one object;
the older task-96 count correction is not the current placement behavior.

## Reproduction and verification

Baseline and final checks use `sh tools/bootstrap.sh --no-hooks`, Release CMake
with `AZTEC_GEN_DIR=` in this worktree's `build/`, build with two jobs, CTest and
all-file pre-commit. Baseline and final: **62 passed, six skipped, zero failed**;
pre-commit passed, including an explicit check of this untracked document.
The skipped entries include private-data checks and restricted network
process creation; they are not game-data verification.

Each clone process used an ordinary private copy made with
`tools/parity_report.py:private_game`: immutable assets symlinked, Maps/Saves
and mutable configuration copied, and a separate `AZTEC_USER_DIR`. No external
installation files were changed. The observation schedule contains ticks 1–12,
60, 100, 500, 1000, 1600 and 2000. With an absolute private run directory in
`purchase_run`, the command is:

```sh
timeout -s KILL 180 unshare -rn env \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  AZTEC_USER_DIR="$purchase_run/profile" \
  "$PWD/build/aztec_engine" --headless --profile faithful \
  --game "$purchase_run/game" --rules-dir "$PWD/docs/data" \
  --no-auto-orders --max-ticks 2000 \
  --l0-schedule "$purchase_run/schedule.txt" \
  --l0-output "$purchase_run/observations.jsonl" \
  --l0-frames "$purchase_run" --ai-trace "$purchase_run/ai.jsonl" \
  --report "$purchase_run/report.json" 2
```

Use mission argument 3 for the other mission. The schedule header is
`L0 2 0 0 0`, followed by one tick per line. All four processes exited zero and
reported 2000 updates, no script errors and no unsupported executions. Report
status `timeout` means the headless simulation reached its tick horizon; no
process hit the external timeout. Baseline and trial are distinct policies, not
independent repeatability runs. Their captures are retained privately.

## Remaining dependencies

Needed before a defensible planner fix:

1. Side-plan request/composition and membership allocation, including the
   outpost request that arrives without entering the offensive branch. Task 96
   explicitly leaves this unfinished; the full task-64 side-plan observer is
   also missing. This investigation completed the recorded-data review and clone
   measurements that do not depend on that observer.
2. An audited explanation of the differing effective production factors and
   observation/update phase. The trial demonstrates a large 67-versus-160
   discrepancy in addition to the known 128-versus-129 conversion artifact.
3. Original observations through 2000, with adequate temporal resolution and
   repeated town request/active-production diagnostics. The supplied long series
   cannot identify exact intermediate starts even before 1600.

The observed phase paths, early money changes and the rejected candidate's
failures are measurements. The replacement minimum and target policy were a fit.
No full rule, sustained-growth tolerance, exact purchase parity or original
tick-2000 values are proven. Status: **needs additional recorded evidence and
side-plan implementation**.
