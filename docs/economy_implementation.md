# Economy implementation

Task 151, 2026-11-21. **V** denotes a reference contract in
[economy_rules.md](economy_rules.md),
[descriptor_building_defence.md](descriptor_building_defence.md) or the existing
instruction audit in [task 118](118-economy-construction-harvest-audit.md).
**G** denotes an engine policy or an unmeasured ordering detail. Passing synthetic
checks establishes implementation behavior, not original-game parity. No original
executable or reference runtime was run for this task. No private code, resources
or extracted tables were added.

## Implemented contracts

| Rule | Evidence and implementation |
| --- | --- |
| **V** Integer construction work | Freeze `maxHP / adjustedTime` with integer truncation at payment. Apply the first increment immediately. Buildings, including automatically placed structures and hall upgrades, use their live HP; units use the quantized work counter. Damage delays buildings and pause stops work. Zero work never finishes. No builder, worker-distance gate or per-tick cash drain is used. |
| **V** Cancellation | Started work refunds `((256 - fraction) * pricePaid) >> 8`, where `fraction = work * 256 / maxHP`; buildings use current HP. Waiting entries refund zero. The recorded payment survives speed changes. Cancelling an upgrade restores its prior hall type; destroyed construction refunds nothing. |
| **V** Gold increment | Constant rate is `(((word60 * 65536) / 100) + 128) >> 8`. Terrain coefficients and the average over mask cells equal to 2 follow economy_rules section 2.3. Empty resource cells remain in the denominator. Sample once at completion, including the interval before the first active update; do not consume resources or resample later. |
| **V** Gold events | Accumulate fixed 8.8 increments on active ticks. On an event pay `(acc + 128) >> 8`, clearing only a nonzero rounded result. Periodic gold clocks start at completion, modulo the 7200-tick day. A period of -1 creates no effective accumulator. |
| **V** Explicit schedules | Read the count and dates from the existing runtime-loaded descriptor CSV, rather than synthesizing evenly spaced events. Descriptor records 7/20/35 contain 6/10/8 dates; the existing type mapping identifies these as farm types 12/39/65. The task's phrase “scheduled mine records” refers to these descriptor records; periodic mine types 13/48/66 are separate. No new executable-derived schedule table is embedded. |
| **V** Warehouse payout | Multiply the rounded payout by 1 or the binary32 value 1.3333334 and truncate. Only farm types qualify, and only a completed warehouse in the same town enables it. Multiple warehouses do not stack; mines and halls retain factor 1. |
| **V** Repair lifetime | Initialize the separate repair increment and periodic clock at creation/admission, including unpaid waiting time. Advance its accumulator only while the building is active. Completion initializes gold without resetting repair. Repair events supply town credit times 100, independently of money. |
| **V** Town lane | Default production has one lane per town. A head missing money or a prerequisite remains unpaid and blocks successors. Disabled types, reservations and queue capacity still apply. Completion frees the lane for the successor in the same update. Optional parallel modifications remain explicit clone settings. |
| **V** No population cap | No population or supply gate is added. The existing 50-waiting-entry capacity is a queue bound, independent of living unit count. |

Gold/repair rates, clocks and residues already participate in deterministic state
hashing. Queue admission clocks and prior hall types now participate too. Engine
command-log saves reconstruct these values by replay.

## Existing data and oracle notes

The checks use `docs/data/types.csv`, `town_factors.csv`, `desc_building.csv`,
`buildtree.csv` and `rules_mechanics.json` without modifying those inputs.
The historical lifted-runtime price of 161 is not a reference price of 160 plus
a fee: the original conversion truncates after adding one half. Existing tests
retain the 160 computer/fast-price example and the corresponding 53-tick nominal
time. Synthetic HP 101 and nominal time 7 produce work 14 and eight increments.

Existing economy regressions retain the reported hall cadence of 60 per 300
active ticks (word60 20 gives increment 51), the resource-dependent 16 per 100
mine example, and mixed-channel farm payout 367 versus 363 for the other fixture.
These are arithmetic checks using existing metadata and invented resource maps;
they are not new live measurements. The historical +3/+6 mine observations in
`rules_mechanics.json` remain **G**, location-dependent leads. Scheduled startup,
residue retention, day wrap, warehouse ownership/completion/non-stacking, repair
credit without cash, cancellation, independent towns, blocked spawn exits and
production above 110 living units retain focused coverage.

New regressions cover a prerequisite-blocked head with an affordable successor,
release without resubmission, automatic construction damage/refunds, in-place
upgrade damage/cancellation, creation-aged repair after unpaid waiting, gold
sampling before the first active tick, and disabled accumulators. Existing
construction/replay tests now assert HP starting from zero when paid and gold
initialization at completion rather than a tick later.

## G policies and numbers still needing live measurement

- **G:** Exact command, town timer and active-building dispatch order. The clone
  skips accrual on the completion tick; measure the first increment, completion,
  first gold accrual, and first repair credit at individual ticks. Start's
  immediate advance does not process completion until the next timer update.
- **G:** Automatic site selection and placement predicates retain existing engine
  policies; this task moves site creation to production start. A site failure
  leaves an unpaid head. Waiting explicit placements retain the existing visible
  one-HP placeholder, then begin paid work at zero HP. Measure admission HP,
  targetability and map occupancy before payment.
- **G:** Hall cancellation restores full HP for the previous type and initializes
  its accumulators again. Measure old HP, repair-clock age, footprint and gold
  phase across upgrade start, pause, damage, cancellation and completion.
- **G:** Shared-cash town arbitration follows stable queue insertion order.
  Measure which town pays first when two heads compete for the last funds.
- Measure the 101/7 quantization example, damaged-building refund fractions and
  first/final-tick refund amounts using an observable original setup; synthetic
  correctness does not validate the native observer's clock or identity mapping.
- Measure all three explicit schedules across startup after a missed date and
  day wrap, especially the schedule containing clock zero. Confirm exact event
  dates from live cash changes rather than interpolating sparse net cash samples.
- Measure terrain yields for empty/partial/full masks and both channels; verify
  the stored yield after resource changes. Measure warehouse removal, same-town
  versus other-town ownership and rounding around fractional payouts.
- Measure repair first-payout phase after waiting/construction lasting less than,
  equal to and greater than its period, including a day wrap; verify zero-rate
  descriptors produce no credit.

The task-64 audited identity/semantic observation adapters and the live task-97
production/income comparison are not available as a completed parity baseline
here. Static implementation and synthetic regressions do not depend on them and
are completed independently. Native record/replay and aligned cash/HP event
measurement remain external work under the prohibition on running the original.

## Validation

Baseline and final worktree-local Release builds passed with `AZTEC_GEN_DIR=`.
The final CTest run passed 73 tests and skipped six: five optional private-data
entries and restricted UDP process creation. Both `test_formats` and the clone's
`test_engine` game-data checks also passed separately with dummy SDL drivers under
`unshare -rn`, using the ignored private installation copy and a private user
profile. These runs did not launch the original or reference runtime. Pre-commit
passed for all tracked files and explicitly for this new document. Changes remain
uncommitted because git metadata is read-only.
