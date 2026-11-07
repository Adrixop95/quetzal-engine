# Mine audit (task 103)

Synthetic audit: complete. No contradiction with the documented harvest rules was
found that justifies changing simulation behavior. Original placement thresholds,
worker/depletion semantics and the reported mission-2 cash phase: **needs live run**.
No reference or private-data game process was launched. Task 101 is present,
including its default placement policy and optional relaxed helper. Task 64's
aligned interactive observation/setup adapters remain unavailable; custom-map
reference comparisons below depend on those adapters or audited manual routes.

## Deterministic cases and findings

`test_mine_audit` in `tests/test_engine.cpp` runs from `test_engine --pure` and
CTest `engine_unit`. Fixtures are invented; they use the public rules tables,
not original maps, assets or executable-derived code. Existing
`test_mine_construction_flow` covers all three races' UI placement, construction,
completion and first active tick. `test_resource_clock_save_boundary` covers
mid-harvest save/replay, global pause and day wrap for mine, farm and hall.

Reference contracts: [rules_types.md](rules_types.md#economy-and-harvest-cycle),
[rules_summary.md](rules_summary.md), the V08/V09 section of
[engine_rules_status.md](engine_rules_status.md), and `harvest_tick` in
`src/engine/sim/rules.cpp`. Placement remains a provisional engine policy as
recorded by [task 101](101-faithful-placement-and-relaxed-helper.md).

| Synthetic case | Result and interpretation |
| --- | --- |
| All mine types 13/48/66; single resource tile at every 7x7 mask offset | Placement accepts exactly offsets with body value 2, in default and relaxed modes. Resource in clearance or ignored cells alone is insufficient. Empty and channel-A-only sites fail placement. |
| Partly covered deposit; one mineral-bearing body cell, other cells empty | Over 600 ticks, each race earns 48. Averaging includes empty body cells. Resource outside the body does not contribute (also covered by `test_production`). |
| Position offsets 0, 1, 23, 47 pixels within the anchor tile | Partial-deposit income stays 48 over 600 ticks. Placement commands themselves use integer tile anchors. |
| Full deposit, channel B=65 | Rus/Aztec earn 16 per 100 active ticks, 96 over 600; Chinese earns 24 per 100, 144 over 600. Assert cash on every tick, including the first payout boundary. |
| Zero, one or four nearby friendly marines | Same full-deposit income on every tick. Units stay alive and idle. These are unit-count controls, not an assignment of workers. |
| Zero, one or two completed same-town warehouses, including mixed races | Same mine income for every unit count. No mine bonus or stacking. Existing farm controls separately check the farm-only multiplier, other-town exclusion and incomplete warehouses. |
| Invalid preplaced mine on no resource | Zero over 600 ticks. This bypasses placement deliberately; a normal placement is rejected. |
| Mineral byte 1 over every body cell | Placement accepts it; every mine earns zero over 600 ticks because its integer rate is zero. Reproduces a possible “mine produces nothing” symptom. Positive deposit placement threshold is provisional; the rate follows the documented formula. Measure original acceptance before changing it. |
| Natural resource use over 600 ticks | Resource map is unchanged, including with nearby units. No finite quantity or natural depletion mechanism exists in the documented model or engine path. |
| Inject resource removal after 50 active ticks | At tick 100 accumulated work still pays 8 Rus/Aztec or 12 Chinese; no further cash through 600. Nearby unit survives and can move afterward. This is a synthetic rate-change test, not evidence of natural depletion or worker return. |
| Two injected overlapping Rus mines sharing a body/resource cell | Each independently earns 96 over 600; mine total 192 plus 120 from the hall, unchanged resource bytes. Killing one leaves the other earning another 96 plus the next 120 from the hall. Ordinary placement rejects overlap, so this is an invalid-world robustness control, not an achievable player construction claim. |
| Destroy mine with 47 ticks pending after tick 600 | Cash stays at its last paid value for another 600 ticks; unpaid accumulation is discarded. Footprint becomes placeable again, nearby unit survives and can move. Uses the normal death cleanup via `script_kill`, not a measurement of player demolition timing. Existing town-service tests cover the demolition command path. |
| Serialize/load at tick 47, before first payout | Replay hash matches at load and every subsequent tick through 600, including all cash boundaries, for all races. This tests engine command-log saves, not original-save harvesting imports. |

No crash, negative cash, unexplained frozen cash or lost mobile unit occurred in
these fixtures. Zero cash on empty/trace deposits and cash stopping after resource
removal/death are explicit controls. Workers cannot be assigned, counted inside a
mine, depleted or returned using an engine mining API: that API does not exist.
The rules do not specify one. Adding worker-dependent yield, consumption or
return behavior would invent mechanics. Those requests remain open for original
measurement; synthetic nearby-unit checks cannot establish them.

One likely placement lead is already recorded in task 101: default placement
rejects mineral overlays. Compare mineral rock overlays with clear terrain before
adding exceptions. No arbitrary threshold, worker system or clock offset was added.

## First harvest and the supplied mission-2 observation

The supplied report states +60 every 300 ticks and an approximately 100-tick
clone delay, with values 2000,2060,2120,2180,2240,2300 and tick labels
300,600,900,1200,1500. There are six values and five labels; the exact initial
sample/payout alignment cannot be inferred. Preserve that ambiguity in the live
comparison instead of assigning a missing label silently.

Rus hall/village d60=20, period=300 yields accumulator increment 51 and rounded
payout 60 after 300 active ticks. The synthetic completed hall starts at 2000 and
pays 2060 at tick 300, 2120 at 600, 2180 at 900, 2240 at 1200 and 2300 at 1500.
Every tick is checked at StartTime 0, 100 and 7150 (including wrap). Thus StartTime
100 alone does not cause a 100-tick delay: the periodic clock is relative to
activation, not global multiples of 300. Mines instead use period 100 and
terrain-dependent increments. A +60/300 cash stream alone does not identify a mine.

There is no worker travel/spawn gate in this income path. Construction skips
income on completion, then initializes clocks for its first active tick; existing
construction tests verify that convention. An incomplete producer, different
initial activation, scripts/spending or observer phase could shift the stream.
No exact cause was reproduced here. Do not subtract 100 ticks, grant an early
payment or alter mine periods based on sparse aggregate cash samples.

## Modification compatibility

This audit adds no alternate simulation behavior and therefore no new switch or
preference bit. Placement controls use task 101's existing
`--mod relaxed_placement=on|off`, default off. Its graphical setting persists in
`modifications.ini` alongside `parallel_construction`; headless runs require
explicit CLI settings. New behavioral helpers, if later justified, must follow
task 48's named switch, default-off persistence and save/hash/replay contract.

## Reproduce the synthetic audit

```sh
sh tools/bootstrap.sh --no-hooks
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
pre-commit run --files tests/test_engine.cpp docs/103-mine-audit.md
git diff --check
```

## Exact live commands: mission-2 first cash step

Run only on a namespace-capable host. The existing production collector creates
separate private installation copies with immutable asset symlinks, copied mutable
state and ordinary Maps/Saves directories for each process. Both programs run with
`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`, a timeout and `unshare -rn`.
It collects two independent reference and two clone runs per mission, including
mission 3 as a control. It retains every tick through 1600 with `--every-tick`;
allow roughly 2.3 GB per process for frames. It deliberately returns 1 for pending
parity, even after successful collection. Keep failures and inspect manifests.

```sh
mines_clock="$PWD/work/parity/mines-clock-$(date -u +%Y%m%dT%H%M%S)"
python3 -I tools/production_measure.py run --every-tick \
  --game /home/adrox/aztec-re/game \
  --runtime /home/adrox/aztec-re/build/aztec_recomp \
  --engine "$PWD/build/aztec_engine" --timeout 1200 --output "$mines_clock"
python3 -I tools/production_measure.py compare "$mines_clock" \
  --output "$mines_clock-tables"
```

Inspect owner-0 cash at every tick, especially 299/300/301, 399/400/401 and each
subsequent step. Record initial day clock, producer type and completion/active
state. Audit script grants, spending/refunds and before/after-update observation
phase. Confirm the same first divergent tick in both independent repeats.
Aggregate cash is not a harvest-event observer; add an audited producer/activation
observer if it cannot identify the cause. The original runtime supplied here may
lack the collector's native observer: record missing observer/output as a blocker,
or build the hand-written observer on the capable host as described in the
existing production run sheet. Never replace missing reference observations with
clone expectations. Task 96's separate post-fix AI acceptance is outside this audit.

## Exact live commands: controlled mine cases

Use task 101's custom-map setup with fresh private copies. Before launching, author
and audit normal-input routes for each case in the table: mask offsets/partial
coverage, non-resource and trace-byte deposits, overlay/no-overlay, no/one/several
observed workers if such a mechanic exists, warehouse/no-warehouse, long harvest
until actual exhaustion, overlapping/shared deposits, demolition and save/load
before a payout. Record which controls cannot be achieved through normal input.
The current catalogue is a plan, not an executable worker/depletion scenario.
An audited route and observer are prerequisites; these commands refuse missing
scripts rather than inventing coordinates or worker assignments.

Prepare a fresh root and independent reference/default/relaxed copies for one
case and repetition. Repeat preparation for every case and at least twice per
variant; set `MINES_CASE` to a descriptive identifier before each preparation.
All original maps and generated maps stay private.

```sh
export MINES_CASE=placement-offsets-repeat-1
export MINES_PROBE="$PWD/work/mines-$MINES_CASE-$(date -u +%Y%m%dT%H%M%S)"
python3 - <<'PY'
import os
from pathlib import Path
import shutil
source = Path('/home/adrox/aztec-re/game')
root = Path(os.environ['MINES_PROBE'])
root.mkdir(parents=True, exist_ok=False)
for variant in ('reference', 'default', 'relaxed'):
    game = root / variant / 'game'
    game.mkdir(parents=True)
    (game / 'Maps').mkdir()
    (game / 'Saves').mkdir()
    for name in ('Aztec.exe', 'Res', 'Video', 'Music', 'Campaign'):
        if (source / name).exists():
            (game / name).symlink_to((source / name).resolve())
    for item in source.iterdir():
        if item.is_file() and item.suffix.lower() in ('.ini', '.plr', '.cfg'):
            shutil.copy2(item, game / item.name)
    for item in (source / 'Maps').iterdir():
        if item.name.lower().startswith('mission_14.'):
            shutil.copy2(item, game / 'Maps' / item.name)
PY
for variant in reference default relaxed; do
  python3 tools/make_testmaps.py --game "$MINES_PROBE/$variant/game"
done
```

Review generated layout manifests and retain only the selected generated custom
map in each Maps directory before original custom-map navigation. Apply identical
reviewed resource patches to the three private map copies for each control; record
byte values and masks privately. The generator alone does not configure every
case above. Author `reference/input.txt`, `default/input.txt`, `relaxed/input.txt`
under the probe root using RT_SCRIPT and PlaytestScript respectively. Reference
routes include menu navigation; clone routes begin directly in `ztest_all`.

```sh
test -s "$MINES_PROBE/reference/input.txt" && (
  cd "$MINES_PROBE/reference/game" || exit 1
  timeout -s KILL 300 unshare -rn sh -eu -c '
    ip link set lo up
    ip addr add 192.168.1.125/24 dev lo
    exec env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
      RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_RESOLUTION=800x600 \
      RT_SCRIPT="$MINES_PROBE/reference/input.txt" \
      RT_DUMP_DIR="$MINES_PROBE/reference" \
      RT_CMD_LOG="$MINES_PROBE/reference/commands.txt" \
      RT_STATE_LOG="$MINES_PROBE/reference/state.txt" \
      RT_UNIT_LOG="$MINES_PROBE/reference/units.log" RT_UNIT_EVERY=1 \
      /home/adrox/aztec-re/build/aztec_recomp "$PWD/Aztec.exe"
  '
) > "$MINES_PROBE/reference/stdout.log" 2> "$MINES_PROBE/reference/stderr.log"
printf '%s\n' "$?" > "$MINES_PROBE/reference/exit-status.txt"
for variant in default relaxed; do
  mines_relaxed=off
  if test "$variant" = relaxed; then mines_relaxed=on; fi
  test -s "$MINES_PROBE/$variant/input.txt" || exit 1
  timeout -s KILL 300 unshare -rn env \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    AZTEC_USER_DIR="$MINES_PROBE/$variant/user" \
    "$PWD/build/aztec_engine" --game "$MINES_PROBE/$variant/game" \
    --rules-dir "$PWD/docs/data" --no-audio \
    --mod parallel_construction=off --mod "relaxed_placement=$mines_relaxed" \
    --ui-trace "$MINES_PROBE/$variant/ui.jsonl" \
    --script "$MINES_PROBE/$variant/input.txt" ztest_all \
    > "$MINES_PROBE/$variant/stdout.log" 2> "$MINES_PROBE/$variant/stderr.log"
  printf '%s\n' "$?" > "$MINES_PROBE/$variant/exit-status.txt"
done
```

Do not bypass namespace failures. Retain process exit codes, runtime/engine/input
hashes, cash per active tick, initial and final resource bytes, producer identities,
construction state and observed worker identities/orders privately. Extend the
observer if resource quantity or worker assignment is unavailable; unit logs alone
cannot prove either. Use two successful independent original runs before changing
placement thresholds, natural depletion or worker rules. Test original save/load
through normal UI separately from engine command-log replay. Publish only reviewed
findings; no captures, game files, modified maps or derived code belong in git.

## Validation

On 2026-11-07, bootstrap (`--no-hooks`), Release configure/build, full CTest and
all-file pre-commit passed before changes. After changes the Release build and
full CTest passed again: 65 entries, 59 passed and six skipped (formats, engine,
lockstep processes, private save import, campaign script inventory and private
parity rules). All-file and explicit changed-file pre-commit gates pass, as does
`git diff --check`. All four shell blocks pass `bash -n`; live commands were not
executed. Changes remain uncommitted because git metadata is read-only.
