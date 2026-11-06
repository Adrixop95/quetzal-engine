# L1 rule scenario specifications

## Construction flow (task 67, needs live run)

`construction.json` is a P catalogue on `ztest_all`, covering every public
Building/Defence type in the same eight classes as placement. Its eight variants
are semantic input recipes, not executable screen scripts. Task 66 is partial;
task 64 has no construction input, creation-identity or stage/worker observer.
No measured timing or reference expectation is included. Parallel construction
is explicitly off. See the [live run sheet](../../docs/EXPERIMENTS.md#construction-flow-run-sheet-needs-live-run).

`tools/construction_compare.py` accepts three complete private `construction-v1`
JSON bundles: two reference repeats and one clone capture. Each bundle contains
`schema: 1`, `contract`, `origin` (`reference` or `clone`), `alignment`, and
`samples`. Alignment has exactly `scenario`, `map_sha256`, `controls_sha256`,
`normalization_sha256`, `building_type`, `camera` ([x,y,zoom]), `resolution`
([width,height]), `overlay_region` ([x,y,width,height]), `first_tick`, `last_tick`
and `phase: "before_commands"`. Digests cover the actual private map, controlled
setup/input and audited field/identity normalization. All three alignments must
match. Capture every tick of the declared horizon, including before placement
and after completion, with `tick`, `state`, `overlay` (relative frame path) and
`unavailable` (an empty array). Unknown fields block comparison.

The `state` keys and normalized values are:

| Key | Required value |
| --- | --- |
| `site` | null before creation/after removal, or an object with integer `id`, `hp`, `hp_max`, `stage`, `progress` (0..1000), `sprite`, `overlay_sprite`, `alive` (0/1), `x`, `y`, and `footprint` (unique [tile_x,tile_y] pairs in y/x order) |
| `workers` | Canonical identity keys mapped to integer `state`, `x`, `y`, `site` (0 means unassigned); empty only for observed absence |
| `money` | Relevant side-number keys mapped to integer balances; cannot be empty |
| `queue` | Complete ordered entries with integer `id`, `type`, `remaining`, `total`, `paid`, `started`, `paused`, `site`; flags are 0/1 |
| `selection` | Ordered canonical integer object identities |
| `rally` | null for observed absence, otherwise [x,y] in world pixels |
| `sounds` | Ordered logical sound IDs emitted during the preceding tick interval |
| `completion` | Ordered canonical object identities completed during that interval |

Positions use integer world pixels. Site/queue/worker identities must follow
creation causes, never native handles or nearest-object matching. Stage/worker
codes, sprite identities (including -1 for observed absence), progress/timer
units and logical sound mapping need an audited normalization document before
an adapter emits these fields. Do not estimate progress or stage from HP alone.
The digest binds that document; validation cannot prove its correctness. A null
site/rally and empty workers/events mean observed absence, not missing support.
Events at tick t are read at the before-command boundary t+1; retain an extra
sample after completion. This avoids assigning a guessed completion tick from
presentation frames. Renderer/UI/audio observations also need that clock audit.

The comparator validates all ticks and full RGB frames before comparing, refuses
unavailable fields and alignment differences, compares the reference repeats
first, and reports the first semantic difference or exact overlay pixel count.
PNG and P6 RGB frames are supported; assets/captures remain private. Semantic
fields precede pixels within each tick. Tolerant screenshots remain separate
diagnostics. A comparison result alone does not establish an audited observer
or full construction parity. `construction_compare` CTest uses invented data;
`engine_unit` separately covers damage, pause/resume, replay continuation,
completion payment and footprint retention with no game data.

## General L1 rules

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

### L8 repeated distributions and clone decision observer

Task 75 adds **S (synthetic controls)**, with **P (protocol)** candidates in
`ai_passive.json` and `ai_provocation.json`. They are measurement recipes, not
runnable L0 fixtures: task 64's shared setup/input/reference adapters are absent.
Their map recipes still need audited terrain, enabled build trees, placement,
initial units, fog and input realization. The `reference_audit: null` field is an
intentional block. The 0.2 total-variation limit is a provisional acceptance
choice, not a measured rule. Freeze the setup, limit and groups before collecting
runs; retain their content hashes with all inputs and runtime revisions privately.

`aztec_engine --headless --ai-trace FILE` emits JSONL `engine-ai-v1` diagnostics.
The header declares schema 1, origin `engine`, 12 Hz and `last_tick`. Every tick
ends with `{"event":"tick","tick":N}` and the stream ends with an `end` marker.
Early mission termination leaves a shorter horizon and cannot pass the parser.
Resume is refused. Events contain `tick`, `event`, `side`, `town`, `x`, `y`, and
`composition` (unique `[type,count]` pairs). Town is the mission **vector index**,
not the possibly sparse town ID. Placement uses tiles; `-1,-1` means no placement.

The independently audited clone emission sites are:

- `sim/ai.cpp`: `build_order` after `apply` adds a queue item, with the reserved
  site and single type. This is a planner order, before payment.
- `GameSim::economy`: `production` after payment, marking the first active service
  of a computer-town queue item. This includes script/player requests for that
  computer town; use script-free setups to attribute it to the planner.
- `sim/ai.cpp`: `launch` after the offensive AttackMove decision is applied, or
  `defence` after the planner's direct Attack response. Each includes the full
  accepted force composition once per decision, excluding script-owned troops.
  Repeated reinforcement decisions remain separate events.

The observer has no simulation state and is excluded from hashes/saves. It does
not infer launches from ordinary movement, expansions from upgrades, or retreat
from an idle order. Expansion and retreat are unsupported planner decisions and
produce no events. The comparator understands those classes so their absence
cannot silently pass. Launch scoring and instantaneous income estimation remain
fallback policies; event observations alone do not expose an internal score.

A future reference adapter must produce `audited-ai-v1`, origin `reference`,
with an `audit` identifier equal to a **private copy** of the scenario's
`reference_audit`. The remaining header and rows follow the same schema. This
identifier is an audit attestation, not proof: retain the reviewed field/identity
mapping, tick-rate/phase conversion, raw logs, scenario/input acknowledgements
and setup fingerprints. In particular, validate paid production versus requests,
autonomous force membership, town ownership/identity, expansion (a new town's
hall at its new tile site, attributed to the issuing town), and explicit retreat.
Raw `RT_UNIT_LOG`, order/position deltas and arbitrary self-labelled JSON are
insufficient. No reference hook or audited mapping is supplied by this task.

`tools/ai_measure.py --scenario FILE --reference REF... --clone CLONE...
--output work/parity/NEW` requires distinct capture paths and at least the
scenario's repeat count **on each side**. It validates complete streams even
outside selected groups. A group selects one side, issuing town, event class and
inclusive tick interval. Its start is the observation origin or fixed provocation
tick. The report includes per-run count envelopes, first/last and ordinal event
timing windows relative to that start, and empirical total variation (TV) for
whole-force compositions and joint `(composition,x,y)` placements. TV is half the
sum of absolute frequency differences. Timings must overlap inclusively; all
clone counts must lie inside the reference count envelope; both TVs must be at
most the declared limit. Variable wave counts are supported. Zero-event runs
contribute zero counts, never invented zero latency. Entirely absent clone
classes fail; entirely absent reference classes return `needs live run`.

The CLI returns 0 for this bounded comparison passing, 1 for differences, 2 for
invalid/truncated input or reused output paths, and 77 for missing audited
reference data. A 77 result writes the explicit blocker to `diff.json`. Events
are pooled for empirical frequencies, so runs with more events carry more weight.
Ordinal matching assumes the same observation interval/trigger; it cannot identify
which internal plan caused a wave. This is a diagnostic distribution check, not
a confidence interval, proof of stochastic equivalence, launch-score recovery,
or full AI parity. Retain raw captures and distributions only under ignored work/.
See the [live run sheet](../../docs/EXPERIMENTS.md#l8-ai-run-sheet-needs-live-run).
## Placement comparison contract

`placement.json` supplies task 66 candidate screen input on ztest_all. Task 64 is
absent, so selection/card routes, clock alignment and the reference placement
observer remain blocked. `tools/placement_compare.py prepare` expands the type
and variant matrix; it does not launch games or translate semantic inputs into
unaudited clicks. Status: **needs live run**, P scenario, synthetic tooling only.
See the [live run sheet](../../docs/EXPERIMENTS.md#building-and-mine-placement-run-sheet-needs-live-run).

The comparator hook consumes JSON bundles with exactly `schema: 1`,
`contract: "placement-v1"`, `origin: "reference"` or `"clone"`, `alignment` and
`samples`. Alignment contains `scenario`, `map_sha256`, `controls_sha256`,
`building_type`, `camera: [x,y,zoom]`, `resolution: [w,h]`,
`overlay_region: [x,y,w,h]`, `first_tick`, `last_tick`, and
`phase: "before_input"`. Hash the same map bytes and audited common setup/input
specification; retain those inputs for review. Each adapter must stop at this
boundary, before the next scenario input, with matching clock/camera/UI state.
The engine raw UI trace is at a different boundary and is not accepted directly.

Every inclusive tick requires exactly one sample, with these fields:

| Field | Meaning |
| --- | --- |
| `tick` | Contiguous scenario tick; never a rendered frame counter |
| `active` | Boolean placement mode |
| `valid` | Boolean decision, or null when inactive |
| `snapped` | Integer `[x,y]` cell, or null when inactive |
| `footprint` | `[x,y,valid]` cells sorted by y then x, no duplicates; empty when inactive |
| `feedback` | Observed user feedback text, empty when none |
| `rotation` | Measured orientation integer, null when absent |
| `ghost_sprite` | Audited normalized sprite identity string, null when absent |
| `cursor` | Audited normalized visible cursor identity |
| `accepted` | Boolean confirmation accepted during the preceding input interval, independent of current placement mode |
| `overlay` | Relative path to a complete opaque RGB PNG/P6 capture at this tick |

Aggregate validity must equal the conjunction of cell validity. Both observers
must supply every actual footprint cell, not an inferred rectangle. After a
successful confirmation, observe the new mode and occupancy at the
next boundary; the preceding preview, input and `accepted` event identify the
site that was confirmed. Do not recompute that previous decision against the
new construction occupancy. Explicit cancellation has inactive/null fields, no
acceptance and an empty footprint. Unsupported rotation
or an absent ghost is null; it is not assigned a guessed orientation/sprite.

The declared rectangular overlay region is compared exactly in decoded RGB,
including cursor/ghost/grid/feedback if they fall inside it. Include all placement
feedback or separately compare its region; do not use a tiny crop to hide a
difference. Keep matching full captures for review. Both complete bundles and all
images are validated before the first differing tick/field or exact pixel count
is reported. Digest equality records a claimed alignment, not proof that an
adapter obeyed it; synthetic equality never establishes reference parity.

## V06/V10 tactics comparison (task 90)

`tests/parity/tactics.json` contains P recipes expanded by
`python3 -I tools/tactic_measure.py plan` into 84 mode/case combinations. Each
case still requires actual global-difficulty enumeration and its one-variable
sweeps. These recipes are not accepted by the shared L0 runner. The packet,
settings and original tactic/selection adapters remain pending; task 73's shared
visibility gate is present. See the [live run sheet](../../docs/EXPERIMENTS.md#v06v10-groups-tactics-and-difficulty-run-sheet-task-90-needs-live-run).

`--headless --tactic-trace FILE` writes engine-local JSONL `engine-tactics-v1`:
a header with schema 1, origin `engine`, phase `before_commands`, position scale
256, first tick 1 and the requested last tick; one snapshot before each tick's
commands; then an `event: "end"` marker with the actual final simulation tick.
Resume is rejected. Early termination or fewer rows than the horizon is
incomplete. The trace includes typed, unremoved objects (including retained
corpses), every side's command selection/groups/queue, and visibility eligibility
for every side. Object `x/y` use fixed-point pixels, destinations use whole
pixels. `visible` calls the shared task 73 gate; liveness and ownership remain
separate fields. `units` counts living typed nonstructures, `structures` counts
living structures; neither count is a claimed original population formula.
Queue rows are `[type,town,remaining,total,paid,started,paused,object]`, in storage
order. IDs, tactic IDs and order codes are native engine values. The observer
is read-only, excluded from hashes/saves, and contains no original scan-event,
bounds, attachment-graph, picking, packet-readback or global-settings observer.
This stream is deliberately not directly accepted for reference comparison.

`tools/tactic_measure.py compare` requires three complete JSON bundles with
exactly `schema: 1`, `contract: "tactics-v1"`, `origin` (`reference` for the two
repeats, `clone` for the engine), `alignment`, `samples` and `unavailable: []`.
Alignment contains exactly `scenario`, `first_tick`, `last_tick`,
`phase: "before_commands"`, `viewer`, and lowercase 64-digit `map_sha256`,
`controls_sha256`, `normalization_sha256`, `packets_sha256`, `settings_sha256`.
All three alignments must match. The normalization audit must describe the
original observer and engine adapter, common logical order codes, canonical
positive integer object/tactic identities, identity references, fixed-point
positions, whole-pixel destinations, observed selection order and production
queue semantics. Use 0 for observed absence of a target and -1 for absent tactic
or follow identity. Canonicalize references even to removed actors. Preserve
actual absence; never fill unavailable observations with zeros/empty arrays.
Hash equality is an alignment attestation, not proof of the adapter's correctness.

Each inclusive tick has exactly `tick`, `objects` (canonical object ID strings
mapped to records), and `sides` (side-indexed array, 1..8 entries). Object fields
are integer `type`, `side`, `hp`, `hp_max`, `alive`, `x`, `y`, `dest_x`, `dest_y`,
`order`, `target`, `tactic`, `follow`, `mode`, `resume`, `cooldown` and `visible`
(an array of integer 0/1 gates for every side). Side fields are integer `money`,
`units`, `structures`, `selected_tactic`, `selected` (ordered unique IDs),
`groups` (all ten ordered unique-ID lists), and `queue` (eight-integer rows as
above). Mode is 0..3; alive and queue started/paused are 0/1. Units/structures
must use the declared diagnostic categories on both sides. Native packet
acceptance, brightness, picking, population accounting and tactic scan internals
need additional audited observations; this bounded outcome contract cannot
certify them by itself.

The comparator validates all three complete bundles before reporting any early
difference. Nonidentical original repeats block the comparison. Stable repeats
produce the earliest differing tick/field; no tolerance, nearest-ID matching,
bonus, cap or scan formula is inferred. Tests use invented bundles, malformed
late rows, repeated original instability, and an actual synthetic engine run
with trace/hash repeatability checks. S success is not M game parity.
