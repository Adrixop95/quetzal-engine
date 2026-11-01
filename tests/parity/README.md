# L1 rule scenario specifications

Task 42's `tools/parity_run.py` provides the L0 harness. The `l1_*.json`
files follow the top-level contract in `docs/PARITY_TEST_PLAN.md`; nested keys
remain provisional until audited L1 setup, command and observer adapters exist.
Use `tools/parity_rules.py` for these specifications; `parity_run.py --layer L1`
does not execute them. These are P specifications,
with no measured expectations or original data. The task prompts 36, 37 and 40
referenced by task 43 are also absent; no dependency implementation is assumed.

There are 26 base cases: three siege movement cases, six target-category damage
controls, reload, range, projectile flight, mine construction, price, income,
sight, regeneration, serial production and production in two towns; plus barracks
construction, single-unit training, farm income, organic/building regeneration
and building/defence repair. The controls
in each file describe required repeats and boundary cases; those repeats are
pending work, not additional executed scenarios. Target type names are test
selection labels, not measured armor classes. Population remains uncovered.

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
