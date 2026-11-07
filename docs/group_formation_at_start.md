# Group formation at mission start (task 98)

Status: **needs live run**. Slot enumeration, member assignment and engine
implementation remain pending. No startup movement policy or mover speed was
changed. The user requires a repeated original measurement before changing engine
behavior; endpoint examples alone do not determine an assignment algorithm.

## Evidence and limits

Read-only inspection used the two cached `dn_213657_dense_l0*` and
`d3_092158_dense_l0*` native reference capture sets under the supplied private
`/home/adrox/aztec-re/work/parity` directory. These contain ticks 1 through 24,
then 30, 36, 48 and 60. After matching initial owner/type/fixed-point position,
all initial Marine position, execution state, anchor, step destination, velocity,
tactic state and mode samples agree across the sets: 1,680 comparisons in mission
1, 1,736 in mission 2 and 3,836 in mission 3. This is a narrow cached-field repeat
check, not a fresh launch or a full-state repeatability verdict. The older set
lacks the newer member, descriptor and animation probes. No reference was run
inside the sandbox and no private source or capture is included here.

The newer capture gives these additional observations for the initial Marines:

| Mission | Initial cohort | Observed moving | Changed position at 60 | Centered at 60 | Members whose anchor changes |
| --- | --- | --- | --- | --- | --- |
| 1 | 60 | 33 | 33 | 60 | 0 |
| 2 | 62 | 35 | 35 | 34 | 29 |
| 3 | 137 | 7 | 7 | 137 | 0 |

These are Marine-cohort counts, not all mobile kinds. The supplied nine moving
mission-3 units therefore need an identity/kind audit; do not silently substitute
seven for the all-mobile claim. At tick 1, member-record tile differs from the
reserved unit tile for 31/11/6 Marines respectively, matching the initial walking
counts. But the initial member tile equals the tick-60 unit tile for only
27/27/130 members. Treating that record as an assigned final slot is unsupported.
Anchors remain fixed in missions 1 and 3 across the sampled interval; a universal
fixed-anchor rule would already contradict mission 2.

The supplied initial walkers are not the entire later moving set: mission 1 has
two later starters, and the cached initial Marine cohort in mission 3 has one.
A tick-1 execution state cannot by itself decide every startup order. Reserved
tile, continuous position, current step destination, member tile and tactic
anchor must remain separate observations.

The signed velocities 2091/1478 and 1307 are movement observations, not descriptor
speeds in pixels. As documented in [idle_tactic_walk.md](idle_tactic_walk.md), the
reference Marine's approximate norm explains the axis velocity 2091 for descriptor
speed 8 over a centered 48-pixel step. The same axis divisor gives 1307 for
speed 5, truncating toward zero. This arithmetic does not establish the complete
normalization rule for arbitrary directions or other mobile classes. The engine
loads Marine descriptor d5 already; replacing its speed with 2091/256 would mix
descriptor speed and step normalization. Diagonal normalization, update/snap timing
and tile reservation still need shared-mover parity work after repeated traces.

The current ordinary mover uses a continuous hexagonal destination lattice and
flow fields. Calling a group move at initialization would inherit that policy;
it cannot be claimed to reproduce the observed tile slots. No arbitrary ring,
nearest-free search, random walk or mission-specific endpoint table was added.

## Measurement support

`tools/formation_measure.py run` uses the shared L0 launcher without normalized
spawn identities. Each mission gets two independent reference and clone processes,
private installations with immutable asset symlinks and copied mutable state,
ordinary Maps/Saves directories, dummy SDL, `timeout -s KILL` and `unshare -rn`.
Reference network setup still fails closed. Process errors are retained and do not
prevent collecting the other repetitions. The default schedule observes every
tick 1..120; `--last-tick` accepts 60..600. Captures remain under ignored
`work/parity`. Dense frames require more disk space than the old sparse schedule.

`compare` requires complete dense reference timelines and retains member records,
formation flags, animation timers, descriptor speeds and canonical group identities
based on initial Marine membership. Native tactic pointers are never compared
across processes. The observer additionally reports `member.pool_ordinal` and
`pool_count`: these mean active-pool traversal position and count, **not** a proven
formation assignment order. Rebuild the reference host to obtain these fields.
Existing probe failures remain explicit. A new tactic pointer or disappearing
cohort member requires a lifecycle/membership audit and fails this report.

Only equal reference summaries produce endpoint candidates. Each candidate has
first walking and displacement ticks, endpoint tile/offset, anchor changes and
walking velocity counts. `observed_idle_tail_tick` is the start of the final
constant centered idle tail, not proof of permanent arrival or the intended order
endpoint. A walking or uncentered final sample is censored. Extend the horizon
rather than inventing an arrival tick. No sparse interval is labelled exact.

Optional paired clone files are matched through initial owner/type/position.
Their complete cohort position timelines must repeat before comparison. Reports
retain each unit's first positional divergence, whether it moved, and both exact
tick-60 positions. This does not compare hidden clone walking state or normalize
logical orders. Reference repeatability here is deliberately stricter than the
cached movement-only check because all available optional probes are retained.

## Live run sheet

Run on the namespace-capable orchestrator host, from this worktree. Do not run
the shared installation directly. Both commands below return **1** for
`needs live run`, including successful acquisition/reporting; CLI and invalid
input failures return **2**. The shell explicitly accepts 1 and stops on 2.
Always inspect `processes.json`, each `invocation.json`, `process.log`, clone
reports and the complete observation horizon before interpreting a comparison.
A successful standalone comparison cannot override a failed runner manifest.

```sh
set -eu
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
pre-commit run --files tools/formation_measure.py docs/group_formation_at_start.md
cmake -S . -B build_formation_reference -DCMAKE_BUILD_TYPE=Release \
  -DAZTEC_GEN_DIR=/home/adrox/aztec-re/work/gen -DGAME_DIR=
cmake --build build_formation_reference --target aztec_recomp --parallel 2

formation_probe="$PWD/work/parity/formation-$(date -u +%Y%m%dT%H%M%S)"
python3 -I tools/formation_measure.py run \
  --game /home/adrox/aztec-re/game \
  --runtime "$PWD/build_formation_reference/aztec_recomp" \
  --engine "$PWD/build/aztec_engine" --last-tick 120 --timeout 180 \
  --output "$formation_probe" || test "$?" -eq 1
for mission in 1 2 3; do
  formation_mission="$formation_probe/mission_$mission"
  python3 -I tools/formation_measure.py compare \
    "$formation_mission/reference-1/observations.jsonl" \
    "$formation_mission/reference-2/observations.jsonl" \
    --clone "$formation_mission/clone-1/observations.jsonl" \
    --clone-repeat "$formation_mission/clone-2/observations.jsonl" \
    --last-tick 120 --output "$formation_mission/review.json" || test "$?" -eq 1
done
```

Retain private revision/diff, runtime and engine hashes and launch manifests.
Return both native reference timelines, reviews and clone-before captures to the
orchestrator. If seed/animation differences prevent strict repeats, audit those
fields and the seed controls explicitly; do not remove differing fields merely
to pass. The stored mission seed is not current CRT state.

Use stable group membership, pool ordinal and every centered endpoint to test
slot order and assignment hypotheses. Resolve ties with repeated controlled maps:
permute member/source order while preserving positions, permute positions while
preserving order, vary group size, rotate/translate the same group, and add an
occupied or impassable candidate slot. A candidate rule must explain idle members,
later starters, duplicate-slot exclusion, anchor changes and all endpoints without
mission-specific exceptions. Startup initialization occurs before tick-1 execution
is observed; if these observations cannot disambiguate assignment, add a read-only
startup observer rather than inferring the missing event from a final snapshot.

Only then implement initialization through a shared ordinary-mover contract and
add generated simulation fixtures for the demonstrated rule. Repeat the same
paired runs after that change and require the moving set and tick-60 positions to
match for all audited mobile kinds, not just the initial Marine cohort. Tasks 83
and 95 remain open; their existing probes are available, but they do not supply
the missing formation rule. No dependency was assumed to have merged implicitly.

## Synthetic validation

The shared L0 tests generate all trajectories and guest-memory fixtures. They
cover full-horizon repeat checks, signed velocity retention, group identity across
process pointers, active-pool ordinals, endpoint censoring, late differences,
missing/sparse samples, paired clone positions and independent failure manifests.
These validate measurement tooling, not original formation behavior. The engine
idle-leash regression now checks both Balanced and Defensive units remain still
without acquiring a distant target for every tick through 60. Determinism tests
continue comparing runs with each other; no golden hashes were introduced.
