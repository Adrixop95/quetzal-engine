# Economy, construction and harvest audit (task 118)

Static audit and synthetic regression coverage are complete for the changes below.
Live parity, including the reported mission-2 income lag, **needs measurement**.
No original executable, reference runtime or private-data game process was run.
Changes remain uncommitted because git metadata is read-only.

**V** means a behavioral fact checked in private original instructions or an
existing reference specification identified below. **G** means an assumption or
unverified engine policy. Synthetic expectations are independently chosen inputs
and arithmetic consequences, not original-game measurements. Private lifted and
decompiled sources were read only; no passages, dumps or game resources are
included here.

## Evidence and comparison

The current checkout already contains tasks 36/47/48 construction and queue work,
task 96's integer production clock, task 101's placement policy, task 103's mine
tests and task 104's modification profiles. Task 97's live growth/income acceptance
and task 64's audited input, identity and semantic observation adapters are not
complete. This audit completes the independent static work; the Windows run sheet
below records work that depends on observations or those adapters.

| Area | Original contract and evidence | Clone result / remaining limit |
| --- | --- | --- |
| Resources and income | **V:** `.rm` has two resource bytes per tile. Building coefficients select a constant rate or a mask average of the two channels. They produce Side cash; d62 supplies a separate town repair budget. `0x410010`, `0x42acd0`, `0x42b040`, `0x411171..0x4111da`; [existing formulas](rules_types.md#economy-and-harvest-cycle). **G:** calling these spendable wood/food/mineral inventories would add an unsupported resource system; channel names remain unresolved. | Two terrain channels, one spendable cash pool and separate repair credits are implemented. Initial cash, script grants/removals, production payment and cancellation also change cash; net cash alone cannot identify harvest events. |
| Worker harvest cycle | **V:** the inspected building harvest path adds fixed-point work each active tick and pays on a clock trigger. Its inputs contain no worker identity, walk distance, carrying capacity, gather order or deposit timer. **G:** a separate worker economy elsewhere is unverified. | No mobile harvest workers are implemented. Walking, carrying and deposit numbers are therefore unestablished, rather than zero-valued reference constants. Nearby-unit controls in task 103 cannot establish worker participation. No speculative worker system was added. |
| Terrain rate lifecycle | **V:** `0x42b040` computes the mask average once, passes it to accumulator initialization `0x42b180` and stores the increment. `0x42b1e0` subsequently adds that stored increment. The constructor does not retain the resource map or mask for periodic resampling. | **Fixed:** the clone previously resampled terrain each tick. It now stores gold and repair increments at accumulator activation and includes them in the state hash. Existing completion/upgrade resets clear them. Exact creation versus first-active-tick ordering remains an observation item. |
| Mine depletion and states | **V:** the inspected mine income path reads resource bytes without decrementing them; the stored-rate harvest path has no finite reserve, worker occupancy or exhausted-mine branch. The building Think dispatch distinguishes active, construction and death behavior; this is not a mine-specific exhaustion state machine. **G:** absence of depletion in all uninspected paths is unproved. | No natural resource consumption or exhausted state is invented. Corrected task 103's synthetic resource-removal expectation: an already active mine retains its rate. Destruction stops further payments and discards pending accumulation. |
| Payout rounding and phase | **V:** accumulate in 8.8 units; an event returns `(accumulated+128)>>8`; retain residue if that result is zero. Periodic clocks use elapsed day time modulo 7200 from creation/last trigger (`0x45f6b0`, `0x45f8c0`). Scheduled clocks start at entry zero, consume at most one entry per tick and reset after wrap (`0x45f6e0`). | Existing clock/rounding tests pass. No global 100-tick offset or mission-specific early payout is justified by these rules. The mission-2 cause remains unresolved. |
| Warehouses | **V:** `0x45c450` supplies the single binary32 4/3 factor only to farms 12/39/65 if the same town has a completed warehouse 18/40/70. No stacking or matching-race requirement; payout truncates after multiplication. | Implemented. Existing tests cover no, one, two, mixed-race, other-town and incomplete warehouses. Mine/hall yields remain unchanged. |
| Placement | **V:** descriptor body cells equal to 2 determine occupancy and harvest averaging; build-tree prerequisites distinguish town-local and side-wide completed counts. **G:** the complete placement predicate, overlay exceptions, value-1 clearance, mineral minimum and town radius are unverified. | Task 101's shared placement predicate covers bounds, body/clearance, terrain, overlays, resource restrictions, units, structures and reservations. Its 16-tile town radius, positive mineral threshold and conservative overlay policy remain **G**, including the known mineral-overlay rejection lead. Automatic placement's gap/ring search is also **G**. No threshold was changed without evidence. |
| Costs and build time | **V:** town getters `0x45c5b0`/`0x45c690` select outpost/developed, speed and computer/hostility factors, then perform original truncation after adding one half. `0x459bb0` charges once at production start. Original `0x4810e4` truncates; the historical lifted-runtime rounding artifact is not an extra price. | Existing original-arithmetic price tests retained. Payment, chosen speed and integer work increment freeze at start; waiting entries are unpaid. No second charge on spawn or per-tick gold construction drain. |
| Construction progress and builders | **V:** `0x40a3c0` freezes `floor(maxHP/adjusted_time)` work per update. `0x40a400` advances actual building HP, or a separate work counter for non-building training. Damage can delay construction; pause stops work. No builder identity or worker-count multiplier appears in this timer. **G:** visible builder animation/participation beyond this path remains unverified. | Task 96's quantized work clock is present. Synthetic maximum 101 / time 7 gives increment 14 and eight updates. Visible placeholder construction retains HP damage. Exact acceptance/start/completion ordering and automatic structures/hall-upgrade HP transitions still need observation. Helpers for parallel construction/training remain explicit profile settings. |
| Cancellation and refunds | **V:** `0x40a490..0x40a4dc` measures completed building HP or accumulated unit work as a fraction of maximum HP in 256 parts. `0x45a880` refunds the unbuilt fraction of the recorded price, with integer truncation. Waiting removal `0x45cfd0` releases a reservation without payment refund. | **Fixed:** the clone used elapsed ticks divided by the rounded-up duration. Refund queries now use actual live construction HP or accumulated integer unit work. UI tooltip and cancellation share the same query, including dead/missing construction rejection. Damage immediately affects the refund. |
| Repair | **V:** modes 0/1/2 select off/repair/destroy; changes clear marks. Completed buildings supply harvested repair credits, multiplied by 100; the budget is spent and cleared each tick, independently of cash. Defence is serviced first for repair; ordinary structures use damage groups and fixed integer shares. `0x4445c0`, `0x444730`, `0x444920`, `0x45d4d0`; [service instruction review](rules_types.md#repair-destroy-garrison-and-limits). | Existing numeric tests cover group priority, capped-target residue, no carry between ticks, town-local budgets, automatic off-mode repair, protection and replay. Mobile-unit service spatial/category parity remains **G**. |
| Selling and destroying | **V:** packet 0x1a (`0x4489b0`) toggles eligible owned town buildings in the current service list; it is not instant deletion. Hall/current construction are excluded. Service destruction triples allocated work and applies explicit damage; no cash refund appears in this path. | Implemented through town service. Construction cancellation is separate and refundable; killed construction gives no refund. A general sell-for-gold action is unverified and was not added. |
| Population and supply | **V:** the inspected queue has 50 waiting entries per town plus separate active work; prerequisites and completed/reserved type counts constrain availability. **G:** no universal population/supply cap is established. Assignment limits of 100 are not evidence of one. | No arbitrary population cap added. Existing synthetic production above 110 units and independent-town queue-capacity tests retained. Housing/food upkeep would be speculative. |
| Production queue | **V:** admission `0x45cec0`, waiting removal `0x45cfd0` and production `0x459bb0` distinguish reserved/waiting and paid/active work. A town has a serial lane by default; completion can start the successor in the same update. | Existing FIFO, cash-stall, pause, missing-hall, same-tick handover, capacity and multi-town tests retained. Spawn exit search and shared-cash town arbitration are **G**. Optional parallel modes do not establish original behavior. |
| Upgrades | **V:** recognized hall upgrades replace the town's race/level build tree through production callbacks (`0x459bb0`, `0x45c4f0`). **G:** a universal troop-stat research model is not established by that callback or the AI science timer. | Hall identity/tree transitions and replay have existing all-race coverage. Exact HP/footprint/income-reset transitions remain observation items. No speculative research costs, bonuses or refund rules were added. |

## Changes and synthetic expected numbers

`test_harvest_rate_lifecycle` writes invented CSV descriptors into a temporary
directory. Coefficients 25/50 and channel bytes 40/80 across two occupied cells
yield increment 200; covering only one cell yields 100 because empty occupied
cells still contribute to the denominator. With period 7, payouts are respectively
5 and 3, at ticks 7/14/21. Removing resource bytes at tick 2 leaves these payouts
unchanged. A producer initialized dry stays at zero after later resource addition.
Scheduled controls at ticks 3 and 11 pay 2 then 6 (full coverage) or 1 then 3
(partial coverage). Repair increment 256 with period 3 resets every three ticks.
Save/replay reconstructs both increments; perturbing either changes the hash.

`test_payment_clock` uses invented price 37, maximum HP/work 101 and nominal
time 7. Work increment 14 means one update completes 35/256, refunding 31;
seven updates complete 248/256, refunding 1. The previous elapsed-time method
would refund 32 and 4. A waiting order refunds zero. Actual cancellation credits
the expected amount and starts/pays the successor in the freed lane.

Its synthetic construction costs 43 and has maximum HP 30000. After three
increments of 4285 from initial HP 1, HP is 12856 and refund is 24. Removing
5000 HP immediately increases the refund to 31, including before another
production update. Cancellation leaves cash 88 from initial 100 and cannot
credit it twice. Existing damage/pause/resume checks still complete at 30000 HP.

Task 103's injected resource-removal test now expects six unchanged mine events
over 600 ticks instead of a half-event followed by zero. Its historical document
is superseded on that point by this inspected stored-rate rule. It did not
measure natural depletion. Other task 103 placement/yield controls remain intact.

After integration with develop, replay semantics version is **10**, because cached
increments enter the state hash and cancellation changes future cash in addition
to develop's version 9 behavior. Develop's byte-layout version 5 and selection-aware
maximum save size are retained; older replay versions are refused. Synthetic
pinned hashes are updated for the added state fields. No original save-continuation support is implied.

## Mission-2 income lag

**V, reported observation:** tasks 97 and 103 describe a roughly 100-tick delay
in the clone's first player cash step, with a 300-tick period and increments of
60. Their sample list has six balances and five tick labels, so the exact first
sample alignment is ambiguous. This audit did not obtain new measurements.

**V, static:** a periodic trigger stores the current clock at construction
(`0x45f6b0`), not a global period boundary. The loader initializes the day clock
through `0x418380` before the subsequent loader stages (`0x42f35a`). Income is
delivered from the active building path. None of these facts identifies a
100-tick travel, spawn or deposit delay. Existing invented-hall tests still pay
60 at elapsed ticks 300/600/900/1200/1500 for StartTime 0, 100 and 7150.

**G, remaining candidates:** initial producer activation, a hall transition,
observer/menu-to-mission alignment, script cash changes or spending can shift
the observed net-cash series. The terrain-rate fix does not establish a fix for
this report and should not be described as one. Task 97's computer plan/growth
acceptance also remains outside the completed static work.

## Windows original-game run sheet (pending)

The maintainer must collect these measurements on the Windows installation;
Codex cannot launch the original here. Use a new private writable installation
copy and profile per independent run. Keep executable/assets read only and
Maps/Saves/settings private. Keep all captures, saves, raw tables, modified maps,
observer output and scripts outside git. Do not reuse a previous run's outputs.

An audited observer of completed simulation updates, producer identity, active
state and cash events is a prerequisite for exact tick claims. Wall-clock waits,
video frame counts, clone state and requested checkpoint labels do not substitute
for that observer. If unavailable, record the case as blocked, retain manual UI
observations separately and finish the task 64 adapter before certifying ticks.
Original Windows execution does not use the Linux lifted-runtime launcher.

For every case use two independent original runs and two faithful-clone runs.
Record executable/build and input hashes, mission/map identity, profile,
difficulty, speed, StartTime, modifications and before/after-update observation
phase. Select the faithful profile explicitly; record all helper settings.
Identify objects by audited stable identity, not nearest position. Use normal
player input for construction, cancellation, damage, save/load and destruction.

| Case | Setup and measurement | Required result fields / unresolved question |
| --- | --- | --- |
| M2 income | Mission 2, passive player side 0, no purchases; capture tick zero and every completed tick through 1600. Repeat mission 3 as a control. | Per producer: type/town/owner, initial active/completion state, creation and activation clocks, stored increment, pending amount, event tick and payout. Per side: cash plus separately attributed grants, purchases and refunds. Inspect 299/300/301 and 399/400/401, then every subsequent step; locate the first actual divergent event. |
| Channels and mine lifecycle | Same mine type on empty, trace-byte, partial and full deposits; no/one/several nearby units; long run; save/load before payout. Repeat all races. | Placement acceptance, producer state, every payout, resource bytes before/after, any observed worker assignment and exhaustion transition. If workers exist, separately record walk/gather amount/capacity/return/deposit ticks; otherwise mark those fields unobserved. Do not manufacture worker orders. |
| Placement | Sweep every mask offset, map borders, resource/nonresource/overlay terrain, occupied cells, units, waiting sites and distance from own/other halls. | Accepted/rejected anchor and exact terrain/mask control; determine mineral threshold, clearance, overlays and town-distance metric before changing policies. Unachievable overlap cases remain synthetic-only. |
| Construction and builders | One order with zero/one/several nearby friendly units; pause/resume; damage mid-build; build a second item in the same and another town. | Acceptance/payment/first work/completion/first income ticks, HP each update, work increment, actual builder identities if any, stages and queue handover. |
| Refund boundary | Cancel unpaid waiting work and paid work before first update, after one update, near completion, while paused and after damage. | Recorded paid price, maximum HP/work, actual completed HP/work, 256-part fraction, cash immediately before/after cancellation and next-item payment. Distinguish pending completion and blocked spawn. |
| Repair/destruction/selling | Several damaged ordinary buildings plus Defence, protected target, hall and current construction. Change modes; destroy eligible targets. | Mark acceptance, source credits, priority/shares, exact HP changes, cleared residual credits, cash, any actual selling action and its prerequisites. |
| Queues and limits | Fill 50 waiting entries, add active work; repeat in two towns and at high side object counts. Block the spawn exit and remove the block. | Admission count, actual rejection reason, payment, pause/cancel behavior, birth tick and any observed supply/population constraint. Record UI errors rather than inferring a cap from counts. |
| Upgrades | Each race/level, damaged hall, active harvest near event boundary; cancel/resume where accepted; save/load around completion. | Hall identity/type/HP, footprint and build tree before/after; costs/refunds, timer/income reset, available recipes and any measured stat change. |

Return both raw repeats and first-divergence analysis privately. Publish only
reviewed English behavioral facts and synthetic regression expectations. A
failed launch, missing observer, unsupported field or unmatched object identity
remains a blocker, never a successful observation. The task remains open for
live acceptance until these measurements resolve the income phase and other
explicit **G** policies.

## Validation

Baseline bootstrap with `--no-hooks`, Release configure/build with
`AZTEC_GEN_DIR=` and `GAME_DIR=`, all 69 CTest entries and all-file pre-commit
completed before edits: 63 tests passed and six private-data/network checks
skipped. Synthetic checks do not establish original-game parity.

After changes, the Release build and full CTest passed again: 63 passed, zero
failed, six skipped (formats, engine, lockstep processes, private save import,
campaign script inventory and private parity rules). All-file pre-commit and
explicit new-document hooks passed, along with `git diff --check`. The new
numeric harvest/refund cases run in `engine_unit`; no original runtime or
private game data was used for this verification.

Integration validation on latest develop: configure/build in `build_sup` passed;
all 72 CTest entries completed with 66 passed, zero failed and six skipped for
private data or restricted networking. All-file pre-commit and whitespace checks
passed. The harvest lifecycle regression also verifies cumulative income in live
and replayed simulations. Git metadata was left unchanged.

Reproduce without private data:

```sh
sh tools/bootstrap.sh --no-hooks
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
pre-commit run --files docs/118-economy-construction-harvest-audit.md
git diff --check
```
