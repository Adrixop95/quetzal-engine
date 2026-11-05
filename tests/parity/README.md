# L1 rule scenario specifications

Task 42's `tools/parity_run.py` provides the L0 harness. The `l1_*.json`
files follow the top-level contract in `docs/PARITY_TEST_PLAN.md`; nested keys
remain provisional until audited L1 setup, command and observer adapters exist.
Use `tools/parity_rules.py` for these specifications; `parity_run.py --layer L1`
does not execute them. These are P specifications,
with no measured expectations or original data. Tasks 36, 37 and 40 now have
archived prompts under `tasks/done/` and integrated production, repeatable-mine
and siege implementations with synthetic coverage. They do not supply the
missing reference setup, command or observer adapters described below.

There are 26 base cases: three siege movement cases, six target-category damage
controls, reload, range, projectile flight, mine construction, price, income,
sight, regeneration, serial production and production in two towns; plus barracks
construction, single-unit training, farm income, organic/building regeneration
and building/defence repair. The controls
in each file describe required repeats and boundary cases; those repeats are
pending work, not additional executed scenarios. Target type names are test
selection labels, not measured armor classes. Population remains uncovered.

Task 65's asset-free measurement handoff is available with:

```sh
python3 -I tools/parity_rules.py plan --output work/parity/l1-task65-plan
```

`rules_protocol.json` supplies ordered metric definitions and proposed field
normalization. It contains no measured expectations. The command writes copied
specifications and null reference/clone-before/clone-after forms; it does not
prepare maps, launch processes or normalize captures. `--id` selects one case.
The [live run sheet](../../docs/EXPERIMENTS.md#l1-task-65-run-sheet-needs-live-run)
lists all 26 pending cases, clock-horizon requirements and the additional
population control variant. Task 64's audited runner/observers are still missing
in this checkout. Their integration and live measurements remain required.

`setup.preplaced_objects` names canonical scenario identities and type IDs;
positions and command destinations are tiles. `input` contains abstract commands
at simulation ticks. Both commands at the same tick retain list order.
`observe` requires every tick from `first_tick` through `last_tick`, at the
before-command boundary. The 720-tick horizon and tick-12 command times are
experiment choices, not original timing measurements. `tolerance: 0` requires
exact integer observations. All observed fields need an audited definition of
units, phase and identity before either adapter emits them. A scenario header's
contract is `l1:<scenario id>:v1`; this name reserves an interface and does not
prove that its adapter has been audited. The legacy normalized header's integer
`mission` is a private map/run discriminator and must agree across all streams.

List and attempt candidate launch probes:

```sh
python3 -I tools/parity_rules.py list
python3 -I tools/parity_rules.py probe \
  --game /absolute/private/game --runtime /absolute/private/aztec_recomp \
  --engine "$PWD/build/aztec_engine" \
  --output work/parity/l1-new-run --timeout 45
```

`--id l1_speed_cannon` selects one case; omitting it selects all. Outputs require
new directories inside this worktree's ignored `work/parity/`. Missing private
inputs return 77 before generation. Generation or isolated launch failure returns
1 and records unavailable tick/object/field as null. Successful launch also
returns 1 with `blocked_setup_contract`: the probes do not implement scenario
commands. Input/usage errors return 2. Existing results are never overwritten.

The generator copies mission 1 privately, removes inherited mobile units using
`make_zstat.py`, puts mobile objects in Teams and structures in Towns, replaces
movement passability with a clear interior/border and clears the resource map.
Only immutable resources/executable are symlinked; Maps/Saves and settings are
private. Visual terrain and fog still come from the template. Generated towns
include automatic halls, and their membership is not the declared scenario's
membership. These files are placement candidates pending control of AI, tech,
money, damaged HP, fog, day clock, legal placement and town membership. In
particular, the empty-resource mine case is only a proposed zero-yield control.
`pending.json` retains unimplemented commands and controls; it must be resolved
before interpreting a map as an isolated rule experiment. No candidate launch
can produce a parity pass or numeric expectation.

With `--engine`, the clone receives a separate snapshot of each generated game
before the reference launch can mutate it. Its bounded headless run also uses
dummy SDL and `unshare -rn`, with automatic human orders and parallel construction
disabled. It checks the complete engine-local observation horizon independently
even when the reference fails. Success remains `blocked_setup_contract`: abstract
commands, damaged HP and other controls have not been applied or audited. The
engine-local observation header is never relabeled as an L1 contract.
`runners.json` retains both diagnostics, and each scenario's `diff.json` plus the
run summary declare layer L1 for triage/matrix collection. A launch or coverage
failure takes precedence over a setup blocker; no semantic tick is invented.

When audited adapters provide normalized observations in the JSONL format of
`docs/parity.md`, compare two reference repeats before the clone:

```sh
python3 -I tools/parity_rules.py compare --id l1_speed_cannon \
  --reference work/parity/run/reference.jsonl \
  --repeat work/parity/run/reference-repeat.jsonl \
  --clone work/parity/run/clone.jsonl --output work/parity/l1-comparison
```

This checks identical contracts, exact field coverage, the full scenario horizon
and initial identities. A reference repeat mismatch reports
`reference_semantic_nondeterminism` and creates no expectations. For repeated
reference observations, `expectations.json` contains only integer rows
`[tick, object_index, field_index, value]`. The separate private
`expectation_keys.json` maps indices to canonical identities and fields and
records the horizon; absent object rows denote absence at that tick.
`diff.json` records the first clone difference ordered by tick, object, then
field, using the existing strict comparator. Malformed/truncated observations
are errors even when an earlier value differs. Exporting observations does not
automatically promote P to M: setup/command coverage and observer audit still
need review. Keep all three files private, including their numeric values.

`parity_rules` in CTest covers synthetic comparisons, numeric export, unstable
references, incomplete coverage, private copy isolation, missing-input skip and
namespace failure propagation. `parity_rules_private` skips unless both game/
and a reference runtime in the build directory are present. Use the explicit
private command above for the external read-only installation. Remove/archive
its ignored `work/parity/ctest-l1` output before rerunning a successful-input
CTest probe; this preserves the no-overwrite rule.

## L8 AI event windows (observer pending)

`tools/parity_ai.py` compares **private, numeric-only event captures**. It is a
comparison utility, not a reference/clone observer or a scenario launcher. No L8
measurements or timing expectations are supplied by this checkout. Audited town,
tactic and production identities, passive-player setup and synchronized event
observers remain prerequisites. Do not infer attack waves from movement alone,
retreat from target loss, or expansion from a hall upgrade.

A capture is a JSON array `[1, scenario, horizon, required_mask, events]`. All
leaves are integers. `scenario` is a private nonnegative setup identifier;
`horizon` is the last fully observed simulation tick, starting from tick zero.
Every producer must observe the entire horizon, including intervals without
events, at the same documented simulation phase. A declared horizon does not
prove that observation occurred: audit the producer and retain raw logs privately.
Scenario identity must cover map, seed, sides, starting cash/technology, visibility
and scripted inputs; matching an arbitrary identifier alone cannot audit them.

Each event is `[tick, side, kind, occurrence, [[type, count], ...]]`. Events are
ordered by tick. Occurrences start at zero independently for each side/kind and
must be consecutive. Type IDs are 0..89, sides 0..7, and counts are positive.
Duplicate types/events are rejected. An event groups the complete composition
at one observed decision, rather than emitting one event per member.

| Kind | Event | Composition |
| ---: | --- | --- |
| 0 | Production starts after payment | Type being constructed/trained, count 1 |
| 1 | Expansion creates another town | New town's hall type, count 1 |
| 2 | Offensive force is released | All members released by that decision |
| 3 | Defence response is issued | All members redirected by that decision |
| 4 | Retreat is issued | All members withdrawn by that decision |

`required_mask` contains bit `1 << kind` for every required class (31 requires
all five). Each capture must contain every required class. A passive scenario
can focus on production/expansion/waves; defence and retreat need controlled
provocations at identical scenario ticks. Describe those inputs and observer
definitions in private setup metadata. Response latency is comparable only when
the provoking event is aligned. Absence of expansion/retreat observations does
not establish parity for those classes; investigate setup or report them open.

After collecting at least two independent reference repeats and a clone run:

```sh
python3 -I tools/parity_ai.py \
  --reference work/parity/l8-run/reference.json work/parity/l8-run/repeat.json \
  --clone work/parity/l8-run/clone.json --output work/parity/l8-comparison
```

The command returns 77 for missing captures, 2 for invalid input or an existing
output, 1 for a difference, and 0 for events within the observed windows. It
requires a new output directory inside this worktree's `work/parity/`. The entire
capture is validated before comparison. Different headers, incomplete required
classes, malformed compositions and unordered events are errors. Reference
repeats with different event identities/counts return
`reference_event_coverage_differs` and produce no windows; resolve the variation
before using this occurrence-based comparison. It is not a distribution test for
variable numbers of waves.

For stable event coverage, `windows.json` contains only integer rows
`[side, kind, occurrence, first_tick, last_tick, type, minimum, maximum]`.
Timing and per-type counts use the inclusive envelope of reference repeats;
type -1 additionally bounds total force size. No extra timing margin is added.
Missing/extra clone events and unseen types fail. `diff.json` identifies the
earliest event difference by tick, then side/kind/occurrence. Per-type envelopes
do not capture correlations or composition probabilities, and a finite set of
repeats does not establish every possible reference outcome. A comparison pass
only covers this event contract; it is not full AI/game parity. Keep captures,
windows, setup metadata and raw outputs private even when they contain numbers
only. The CTest `parity_ai` exercises this comparator with invented values.

`testmaps.json` is a separate provisional L4 specification for private generated
`ztest_all` and `ztest_battle` maps of kind `campaign-derived`. It records semantic
state recipes and explicit missing input/observer adapters. It is not accepted by
the strict L0 harness or executed by `parity_rules.py`. Generate maps with
`tools/make_testmaps.py`; see EXPERIMENTS.md for isolated direct launch probes.
No successful clone probe or structural test establishes original-game loading.

## V07 projectile candidates

`projectiles.json` is a separate P protocol catalogue for seven candidate families
and 18 controls each. It is consumed by `tools/projectile_measure.py`, not the
L1 runner. Private ztest_battle placement preparation and engine-local JSONL
trajectory/impact/per-victim sampling are implemented; single-shot reference
setup, exact pixel offsets, terrain-height fixtures and an audited reference
projectile/burn observer are pending. See the [run sheet](../../docs/EXPERIMENTS.md#v07-projectile-run-sheet-needs-live-run).
Status: **needs live run**. Synthetic capture equality does not promote P to M.
