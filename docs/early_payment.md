# Early computer spending: task 96, partial (2026-11-07)

The early drain is **discrete payment at production start**, not a fractional
charge each tick. Do not add a per-side cash drain or mission-specific purchases.
The original task remains incomplete: the clone does not yet reproduce the
side plans that request the first purchases in missions 2 and 3.

## Evidence and money series

Read-only archived reference observations under the user's private
`work/parity/dn_213657_dense_l0{,_mission_2,_mission_3}/reference/observations.jsonl`
cover the requested ticks. These dense runs have only one capture per mission;
they are supporting observations, not newly repeated reference certification.
The two `dev_212738_*` captures per mission agree at ticks 1, 6 and 12.
A fresh isolated reference setup failed at `ip link set lo up` with
`Cannot open netlink socket: Operation not permitted`. No new reference run
or repeatability claim is made. Raw records remain outside the repository.

All observations below are before commands. Each money tuple lists sides 0, 1
and 2; side 3 remains zero. The clone before columns use the archived dense
captures. Clone after columns use two fresh isolated processes per mission,
with dummy SDL drivers and a private installation copy. Both after runs agree
on all money and object records at every listed tick.

Mission 1: reference, clone before and clone after are `(1500, 0, 0)` at every
sample. There is no computer town or purchase in this window.

| Tick | Mission 2 reference | Mission 2 clone before / after | Mission 3 reference | Mission 3 clone before / after |
| --- | --- | --- | --- | --- |
| 1 | (2000, 0, 500) | (2000, 0, 500) / (2000, 0, 500) | (3500, 700, 1000) | (3500, 700, 1000) / (3500, 700, 1000) |
| 2 | (2000, 0, 500) | (2000, 0, 500) / (2000, 0, 500) | (3500, 701, 1000) | (3500, 700, 1000) / (3500, 700, 1000) |
| 3 | (2000, 0, 500) | (2000, 0, 500) / (2000, 0, 500) | (3500, 701, 1000) | (3500, 701, 1000) / (3500, 701, 1000) |
| 4 | (2000, 0, 500) | (2000, 0, 500) / (2000, 0, 500) | (3500, 701, 1000) | (3500, 701, 1000) / (3500, 701, 1000) |
| 5 | (2000, 0, 500) | (2000, 0, 500) / (2000, 0, 500) | (3500, 701, 1000) | (3500, 701, 1000) / (3500, 701, 1000) |
| 6 | (2000, 0, 500) | (2000, 0, 500) / (2000, 0, 500) | (3500, 572, 1000) | (3500, 701, 1000) / (3500, 701, 1000) |
| 9 | (2000, 0, 433) | (2000, 0, 500) / (2000, 0, 500) | (3500, 443, 833) | (3500, 701, 1000) / (3500, 701, 1000) |
| 12 | (2000, 0, 433) | (2000, 0, 500) / (2000, 0, 500) | (3500, 443, 833) | (3500, 701, 1000) / (3500, 701, 1000) |
| 18 | (2000, 0, 433) | (2000, 0, 500) / (2000, 0, 500) | (3500, 443, 833) | (3500, 701, 1000) / (3500, 701, 1000) |
| 24 | (2000, 0, 433) | (2000, 0, 500) / (2000, 0, 500) | (3500, 443, 833) | (3500, 701, 1000) / (3500, 701, 1000) |
| 36 | (2000, 0, 433) | (2000, 0, 500) / (2000, 0, 500) | (3500, 443, 833) | (3500, 701, 1000) / (3500, 701, 1000) |
| 60 | (2000, 0, 433) | (2000, 0, 500) / (2000, 0, 500) | (3500, 129, 833) | (3500, 701, 1000) / (3500, 701, 1000) |

Mission 2 side 2 queues a footman (type 0) by tick 6 and starts it at tick 7,
paying 67 once. Side 1 queues a mage (28) by tick 5 but cannot pay; it remains
queued at tick 60. No side 0 purchase occurs.

Mission 3 side 1 starts an archer (27) at tick 6 for 129 and another at tick 7
for 129. Its extra gold at tick 2 is income, not a refund. Side 2 starts footmen
at two towns at tick 7 for 100 and 67. The third side-2 town has no active work.
At tick 60 side 1 has started a mine (48) and farm (39), paying 121 and 193;
these explain the later 443-to-129 step. No research payment is visible in the
first twelve ticks. Mission 2 has a hall-upgrade request (19) waiting at tick 60,
which is not evidence that it has been paid.

## Clock interpretation and implementation

Local read-only inspection of the production timer, checked against the archived
increments, establishes these facts. The three `production_words` at town
`+0x118/+0x11c/+0x120` are integer work per tick, price and accumulated work;
the first word is not a duration. Training accumulates work independently of the
unit's health. Buildings use their actual health, so damage delays completion.
The increment is maximum HP divided by the adjusted nominal time, truncated.
An undamaged job can therefore take more ticks than that nominal time.
For example, maximum 101 and nominal time 7 give increment 14 and eight ticks.
A zero increment does not advance work. Start initializes and advances the timer;
completion can start the next queued item in the same town update. Pausing stops
work. Payment occurs once on accepting the active item, independently of these
increments, and remains fixed through subsequent speed changes and damage.

The engine now freezes that increment at payment, uses the resulting undamaged
duration for queue/UI estimates, and uses actual building health for construction.
The increment participates in state hashes and command-log replay. Replay semantics
version 5 rejects older engine saves instead of replaying them under new timing. Generated
fixtures verify integer truncation, successive payment boundaries, no repeated
charge while training, damaged construction, pause/resume and replay. Existing
upgrade tests cover the common production path. This does not verify original
research completion or establish a separate research charging schedule. The
strategist's science timer is a planning timer, not a cash-debit clock.

The oracle's documented conversion artifact remains relevant: the archers in
these captures cost 129, whereas the independent original-x87 pricing model
computes 128. Do not introduce a hidden one-gold surcharge. Resolve the runtime
conversion with the original instructions and rerun before certifying prices.
See [pricing evidence](rules_types.md#prices-time-and-the-footman-puzzle).

## World object correction

Mission 2's extra owner-0, type-minus-one entry is `[Submarine0]`. The mission
loader creates its reference controller in a separate pool, not the ordinary
physical-object pool observed by L0. World placement now skips that controller,
while retaining the parsed source, coordinates and scenario ID for inspection.
Boat and Johnka physical objects remain present. IDs remain contiguous. Synthetic
placement covers both vessels, the submarine controller and an automatic hall.
Two fresh mission-2 clone runs report **104** live objects at tick 1, matching the
archived reference count; the previous clone reported **105**. Submarine scenario
actions remain unimplemented; this correction does not implement those actions.

## Remaining work and next live run

Neutral computer sides now run the planner and receive computer intelligence
initialization. Neutral diplomacy is not an AI-disable flag. A synthetic neutral
computer queues on simulation tick 5 and pays on tick 6, leaving the human balance
unchanged. This removes one independent cause of missing AI work, but does not
supply the missing side-plan requests in these campaigns.

In mission 3 the native order records show the side planner moving archers,
crossbowmen and mages out of both towns' memberships by tick 5. The clone retains
them. Town-only cheapest-unit planning cannot reproduce that allocation and its
subsequent requests. Side-plan arbitration/composition is still absent, as already
recorded in [the AI inventory](engine_rules_status.md). Task 83's waiting-ring and
execution diagnostics exist here; full normalized order/queue semantics and an
audited side-plan observer from task 64 do not. Those missing parts, the income
phase difference at tick 2, and oracle conversion must be resolved before claiming
V08/V11 money parity. No mission-specific purchase sequence has been installed.

The optional reference diagnostics now additionally expose active production
`type`, `work_per_tick`, `price`, `accumulated_work`, `paused`, and town planner
`development`, `science_timer`, `phase`, `speed`. Invalid production pointers
produce an unavailable diagnostic without calling guest methods. Older raw words
remain available. These are native diagnostics, not a normalized queue contract.
The three tracked private L0 scenarios now request the twelve ticks above.
Rebuild the private reference runtime in this worktree as described in
[EXPERIMENTS.md](EXPERIMENTS.md) and run both repeats per mission using the shared
L0 runner. Retain
the existing clone-before captures. Do not bypass failed namespace setup. Capture
the additional planner fields before changing composition or side-plan ownership.

## Verification

Release build and pre-commit pass. CTest reports 49 passes and six expected skips
out of 55 entries. Separate private-installation format and engine checks pass
with dummy SDL inside `unshare -rn`. The six clone captures complete successfully;
all twelve observation records and frame files agree between repeats within each
mission. These checks do not establish reference money parity. Changes remain
uncommitted because git metadata is read-only.
