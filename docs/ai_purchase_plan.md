# AI purchase plans: source investigation and replacement

The cheapest-unit/force-deficit purchase policy has been replaced with composition
plans and allocation across a side's towns. **Exact purchase and growth parity
remains incomplete.** Mission 3 side 2 matches the requested army checkpoints,
but that does not establish full AI parity. Read this alongside
[the first investigation](ai_purchase_gate.md).

V means a fact inspected in original instruction comments or an archived original
observation. G means an assumption or incomplete engine adapter. New C++ is an
independent implementation of behavioral rules; no original code, assets, private
captures or extracted tables are included. The reference executable was never run.

## Recovered branch

All entries in this call-path table are V. Addresses identify research locations,
not code incorporated into the engine.

| Entry | Role |
| --- | --- |
| `449980` | Side update; advances the active side plan through `457000` when it is incomplete. |
| `45e8e0`, `459bb0`, `45e7c0` | Town update, production work/payment, then the computer strategist. |
| `45dfa0`, `45dff0`, `45e530` | Create offensive, local and threatened-town plans through `44d130`. |
| `45e2a0`, `45e340`, `45e770` | Progress town-owned plans through `44dbc0` and `457000`. |
| `456f20`, `456f50` | Append a requested type or reference an existing unit. |
| `457000` → `45cec0` | Allocate outstanding requests to eligible towns and insert production objects into the ring at town `+0xf8`. |
| `45c5b0`, `45c690` | Effective price and nominal production time. |

V: `460a10` is unit thinking, including tactic callbacks. It is not the missing
purchase-policy dispatcher. Direct queue insertion callers also include tactic
requests at `461180`/`461950`, science at `45b5f0`, economic requests at
`41e610`/`41e850`, and other construction paths. Queue activity alone does not
identify which planning system requested it.

V: a progressing plan scans the side's towns. A recipient must have no active
production and no waiting queue, and be in phase 0, 2, 14 or 22. It scans the
plan's outstanding requests backwards and reserves one producible request per
eligible town. Affordability is tested at production start, not plan reservation.
A town-owned strategist does not progress its plan while its own production is
busy. An economic request can occupy the lane before military allocation.

V: phase 9 requires a developed source town and a selected non-own town record;
an outpost does not create its own offensive plan. It can nevertheless receive
another town's plan request while in phase 0. Thus increasing the local phase-13
force target is not the explanation for its early purchase.

## Composition and growth limits

V: composition uses 90-type intelligence records and a matchup matrix based on
kill time. Matrix strength is the 8.8 complement of `(kill_time + 1) / 256` for
kill times through 255. Non-attacking structures contribute a minimal raw weight
of 1; long or impossible mobile matchups contribute zero, with a minimal weight
against structures. This is a combat-composition calculation, not a price ranking.

V: candidate types are gated by maximum side development and build-tree
availability. Ordinary new counters test the requesting town; the additional
target-class pass uses side availability. G: the engine maps that virtual target
predicate to its flier classification. New-counter scores combine normalized
maximum HP, matchup strength and a random perturbation from 0 through 63.
Reusable counters compete with their unweighted matchup strength. Type 33 is
excluded from the ordinary pass. The additional target-class pass uses a
perturbation from 0 through 31. Batch size depends on development and a random
choice, rather than a universal minimum-one purchase rule.

V: eligible nearby troops can satisfy requests without purchases. Offensive plans
can gather town troops as well as eligible non-town tactics; the all-enemies
sentinel suppresses this reuse/tactic branch. Demand, already allocated strength,
existing members, producer availability, cash, production work and subsequent
losses constrain growth. Plan storage holds at most 256 entries, and the town
waiting ring holds 50. Neither is a demonstrated global population cap.

V: MapExplorer retains previously recorded intelligence when a town is hidden.
Once one member reveals a town, its live membership is counted; counting only
individually visible members loses composition information. Periodic rebuilds
clear the cached records. Reserved plan membership differs from production town.

The implementation keeps remembered counts and explicit request ownership.
Queue entries link to a plan generation and slot. Completion fills that request;
cancellation or loss makes it available again. Old work cannot fill a replacement
plan which happens to reuse the same slot. These links and planner RNG/knowledge
participate in hashes and replay. Replay semantics version is now **8**.

## Prices

V: the map's `Neutral=1` branch selects the original all-enemies mask sentinel.
It does not initialize symmetric neutral diplomacy. The sentinel must be retained
independently of whether the currently listed sides happen to be enemies.
Script diplomacy changes individual mask bits; restoring the last cleared enemy
bit restores the sentinel.

V: developed all-enemies towns use price factor float32 2/3 and time factor 2.
Other computer towns use the selected speed factors and the 0.8 multiplier.
Outposts use their separate base-price and 1.5-time branches. This explains the
100/67 footman prices and work increments 40/30 without a mission-specific rule.

V: speed compares estimated income against twice cumulative spending per elapsed
time. Income combines current side cash per town with an instantaneous producer
estimate, rather than accumulated historical harvest. This replaces the old
harvest-history approximation. The independent conversion remains unchanged:
archers cost 128 where the archived lifted runtime reports 129. No one-gold
surcharge has been added.

## Comparison with archived observations

V: original streams are the `d3_092158_dense_l0*_mission_{2,3}` and earlier
`dn_213657_*` observations, the two `dev_212738_*_{a,b}` startup captures, and
`long23_075041_m{2,3}/reference/observations.jsonl`, as listed in the first report.
The long streams end at 1600. Sparse checkpoints do not identify every intervening
purchase. The startup repetitions corroborate selected early checkpoints, not
an independently repeated dense event series.

The following table uses **before-command observation ticks**. Clone production
events occur one update before they become visible in its observation stream.
V labels all original entries; replacement entries are fresh engine measurements.

| Mission / town | Original first request/payment | Replacement |
| --- | --- | --- |
| 2 / side 1 town 0 | Mage 28 queued by 5; unpaid through 60 | Archer 27 queued by 6; unpaid through 60: wrong type. |
| 2 / side 2 town 3 | Footman queued 6, paid 7, price 67 | Queued visible 7, paid visible 8, price 67; engine events at 6/7. Price and work increment match; observation timing differs. |
| 2 / side 2 town 2 | Hall upgrade 19 waiting by 60 | Extra footman at the same early boundary, price 100: wrong request. |
| 3 / side 1 towns 3, 4 | Archer payments visible 6/7, price 129 each | Both visible 8, price 128 each: types match, timing and runtime prices differ. |
| 3 / side 2 towns 0, 1 | Footman payments visible 7, prices 100/67 | Both visible 8, prices 100/67; engine payments at 7. Prices and work increments match. |
| 3 / side 2 town 2 | No early request | No early request through 60. |

V: original mission 3 side 1 additionally pays 193 for a farm and 121 for a mine
between 48 and 60. The replacement starts a farm for 192 and another archer for
64 at engine tick 56. It does not reproduce the mine request.

The checkpoint cells below are **money / live physical objects / mobile units**.
Mobile classification is the same as the first report: Marine, Spider, Headtruck,
Truck, Cannon, Bird, Zeppelin, Boat and Johnka. This distinguishes army growth
from farms, other buildings and virtual objects. Original columns are V.

| Mission | Side | Tick | Original | Replacement |
| --- | --- | --- | --- | --- |
| 2 | 1 | 100 | 16 / 66 / 38 | 0 / 66 / 38 |
| 2 | 1 | 500 | 170 / 66 / 38 | 26 / 67 / 39 |
| 2 | 1 | 1000 | 430 / 66 / 38 | 158 / 68 / 40 |
| 2 | 1 | 1600 | 63 / 69 / 39 | 2 / 69 / 41 |
| 2 | 2 | 100 | 433 / 18 / 16 | 333 / 18 / 16 |
| 2 | 2 | 500 | 295 / 21 / 18 | 196 / 21 / 19 |
| 2 | 2 | 1000 | 315 / 22 / 18 | 376 / 21 / 19 |
| 2 | 2 | 1600 | 307 / 24 / 18 | 556 / 21 / 19 |
| 3 | 1 | 100 | 147 / 58 / 44 | 189 / 56 / 44 |
| 3 | 1 | 500 | 420 / 62 / 44 | 8 / 65 / 51 |
| 3 | 1 | 1000 | 52 / 71 / 44 | 39 / 75 / 59 |
| 3 | 1 | 1600 | 675 / 82 / 51 | 188 / 85 / 65 |
| 3 | 2 | 100 | 833 / 87 / 81 | 833 / 87 / 81 |
| 3 | 2 | 500 | 1029 / 90 / 84 | 1029 / 90 / 84 |
| 3 | 2 | 1000 | 1285 / 92 / 86 | 1285 / 92 / 86 |
| 3 | 2 | 1600 | 64 / 95 / 89 | 1564 / 95 / 89 |

Mission 3 side 2 matches all four object/mobile counts and the first three cash
checkpoints. At 1600 its cash is 1500 too high despite matching those counts.
Mission 3 side 1 overproduces mobile units. Mission 2's extra outpost footman
explains its first additional 100 debit, but not its complete later divergence.
Neither aggregate matches nor the recovered branch establish full parity.

## Explicit remaining assumptions and dependencies

- G: the engine still chooses the first eligible known target. The original
  `44cd30`/`44cfe0` target valuation also weights distance and force information;
  its complete fixed-point calculation has not been implemented here.
- G: a per-town deterministic CRT sequence starts at 1. The original uses shared
  randomness consumed by other systems. This is not a recovered mission seed or
  RNG-call schedule, and no seed was fitted to the recordings.
- G: town-script eligibility, the native integer proximity metric, all side-plan
  arbitration and tactic attachment/release are not completely represented.
  Explicit script groups are preserved; nearby town troops use a Euclidean test.
  The separate active side-plan pointer and its phase-0 wait remain missing
  dependencies of the full side planner, as does task 64's complete observer.
- G: native composition can recommend an unavailable counter's prerequisite to
  the science branch. That recommendation path is still missing. Science and
  defence choice, economic placement/ranking and construction arbitration retain
  their documented approximations. Replacing combat purchases does not resolve
  those independent policies or the outstanding task-96/97 allocation work.
- G: the engine still runs production for all towns before town strategy, whereas
  V: the original town path interleaves production and strategy. This is a
  remaining timing dependency, not a reason to shift observation labels or add
  recorded purchase ticks. Income activation and other simulation dependencies
  can also affect the later cash series.

These are material limits: the replacement implements recovered composition and
queue-allocation rules, but it is **not yet a fully faithful side planner**.

## Reproduction and checks

Baseline: bootstrap with `--no-hooks`, independent Release configure/build in this
worktree's `build/`, CTest and all-file pre-commit passed: 63 passed, six skipped.
Git metadata remained read-only and no commit was made.

Final clone captures are private under
`work/ai_purchase_plan/final_{1,2}_m{2,3}/`. Each invocation
uses `timeout -s KILL 180 unshare -rn`, both dummy SDL drivers, a separate profile,
and `parity_report.private_game` (immutable resource symlinks; copied mutable
configuration, Maps and Saves). The external installation is read-only. No
reference process or namespace-network workaround was used. Schedules request
all ticks 1 through 60 and checkpoints 100, 500, 1000 and 1600. The engine runs
with `--profile faithful --headless --no-auto-orders --max-ticks 1600`, explicit
rules/game paths, L0 output, AI trace and a report.

Synthetic checks cover remembered intelligence, hidden town membership, outpost
eligibility, unpaid reservations, cross-town slot ownership, completion, loss,
cancellation, stale plan generations, mask restoration, hashing and replay.
The 14,400-tick pressure/replay fixture now uses durable script-owned guards:
hall HP alone does not prevent capture. It counts accepted launch events rather
than assuming that a previous remaining-count of zero identifies a launch.
Surviving troops can satisfy one side's plans without a purchase in every
five-minute window. The fixture still requires waves, firing, losses and births
in every window, combat from both sides, and per-side production over the run.
This synthetic fixture does not measure campaign balance.

The trace-neutrality fixture now uses the actual `StartMoney` mission key instead
of `Money`. The latter left it with insufficient cash for its asserted payment
once the all-enemies pricing shortcut was removed.

Final verification: Release build passed; CTest reported **63 passed, six skipped,
zero failed**. All-file pre-commit and an explicit pre-commit check of this new
untracked document passed. Skips remain skips, including the default private-data
and restricted network checks. Four separate clone processes completed 1600
updates with zero script errors and unsupported executions. Within each mission,
both repetitions agree on all 64 requested object/money/queue/view records, AI
trace events and the final simulation hash. This checks engine repeatability,
not original/engine state-hash equivalence or rendering parity.
