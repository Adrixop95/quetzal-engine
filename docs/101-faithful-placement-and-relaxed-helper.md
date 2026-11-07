# Placement policy and optional relaxed placement (task 101)

The default now rejects map overlays and mineral tiles for ordinary buildings,
checks descriptor clearance as well as body cells, and protects other buildings'
clearance from new bodies. `relaxed_placement=on` retains the previous placement
policy. These are conservative engine rules pending original-game measurements;
**exact original placement parity needs live run**. Synthetic implementation,
option persistence, UI preview and replay checks can be completed independently.

No reference process was launched for this task, as required by the sandbox task
instructions. The read-only `/home/adrox/aztec-re/work/parity` cache contains
mission L0 measurements but no placement/ztest_all/mine captures by filename.
No placement evidence was found in this worktree's initially absent `work/`.
Task 63's generator and task 66's candidate specification/comparator are present.
The audited task 64 setup/input/clock and reference observation adapters remain
missing; task 66 still declares `needs live run`. Task 104 profiles are absent,
so both helpers remain off by default. A future enhanced profile should enable
`relaxed_placement`; a faithful profile should leave it off.

## Switch and integration

Use **Game modifications → Relaxed placement** from the main-menu options or
pause menu, or pass the switch explicitly:

```sh
build/aztec_engine --mod relaxed_placement=on --game /path/to/private/game ztest_all
build/aztec_engine --mod relaxed_placement=off --game /path/to/private/game ztest_all
```

Graphical mode persists `relaxed_placement=on|off` alongside
`parallel_construction` in the existing user `modifications.ini` file.
`--modifications-file FILE` selects a different file, and `--mod` overrides loaded
settings. Old files containing only `parallel_construction` leave placement off.
Headless mode follows the existing task 48 interface: it accepts `--mod`, starts
with helpers off and does not load graphical preferences. Game-data launches must
use the isolation commands below; the examples above describe CLI syntax only.

The new option occupies modification bit 1 (value 2); parallel construction keeps
bit 0. Initial options and subsequent `SetModifications` commands participate in
hashes, saves and replay. Replay version is now 6 because placement semantics
changed; older simulation saves are refused rather than silently reinterpreted.
Turning the helper off does not remove existing construction. Waiting orders are
revalidated before starting under the active policy, as before.

Manual clicks, UI traces, per-cell red/green ghost colours, queued-placement
revalidation, automatic placement and AI offers use `can_place`/`placement_cells`.
The automatic planner retains its separate gap policy. The renderer already
consumes the shared per-cell diagnostics, so no second validity implementation
or colour rule was added. Rendering still draws a grid without a ghost sprite;
exact original colours and artwork remain task 66 work.

## Rules and evidence boundaries

Existing public descriptor/format facts are in [rules_summary.md](rules_summary.md)
and [formats.md](formats.md): mask value 2 occupies a cell; `.dat` class 0 blocks
both ground movement classes, class 1 blocks the large class, and class 2 permits
both. Tree overlays have historically coincided with class 1. These movement and
format facts do not establish the complete original construction predicate.

| Case | Default policy | Relaxed policy | Original evidence still required |
| --- | --- | --- | --- |
| Descriptor value 2 | Require free body cell | Same | Placement boundary/snap alignment |
| Descriptor value 1 | Require clearance; body may not enter another structure's clearance; clearance rings can overlap | Ignore | Whether these cells are placement clearance, and for which classes |
| Descriptor value 0 | Ignore | Same | Verify no additional hidden clearance |
| Missing mask | Existing width/height rectangle | Same | Fallback is engine policy |
| Terrain classes 0/1 | Reject required cells | Reject body cells | Per-building terrain exceptions |
| Nonzero map overlay on otherwise open terrain | Reject required cells | Ignore overlay | Which overlays block; decorative exceptions are unresolved |
| Mineral resource (high byte of `.rm`) | Reject ordinary buildings on required cells | Ordinary buildings may overlap it | Deposit channel/threshold and restrictions by building class |
| Farm resource (low byte) | Allowed without a minimum yield | Same | Actual farm yield/placement threshold |
| Mine (13/48/66) | At least one body cell must have a nonzero mineral byte; only mineral body cells may use class 0; class 1 and overlays still block | Existing mineral exception also permits class 1; overlays ignored | Deposit shape, terrain/overlay exceptions and threshold |
| Living building, any owner | Body and clearance checks | Body overlap blocks | Relation-specific exceptions |
| Ground unit, any owner | Collision disc intersecting required cell blocks | Collision disc intersecting body blocks | Unit/radius/worker exceptions |
| Flying unit | Does not block | Same | Confirm original behaviour |
| Dead/removed object | Ignore | Same | Rubble and persistent map objects |
| Pending unstarted placement | Reserve body and clearance, excluding the current reservation | Reserve body | Original reservation timing |
| Town hall | Its body and clearance are protected; in-place upgrades skip their own old hall | Body protected, no clearance | Own/allied/enemy town-centre distance metric and special rules |
| Town radius | Existing maximum-axis distance of 16 tiles from owning completed hall | Same | Actual metric, radius and origin |
| Map edge | Every required cell must be in bounds | Every body cell must be in bounds | Original border margin |

Overlay rejection and value-1 clearance are deliberately conservative policies,
not measured original rules. In particular, a mineral rock represented by an
overlay is currently rejected even by a mine in default mode. Measure such cases
before claiming faithful mining or adding overlay-ID exceptions. There is no
new guessed town-centre spacing constant. Task 103's mine/harvest audit is still
queued; resource thresholds and exact harvesting rules are independent open work.

## Synthetic verification

`test_placement_modes` generates a 40×40 map and independently sweeps all 49
barracks mask positions with invented overlays, class-0 rocks, class-1 trees and
mineral bytes in both modes. It also covers farms, all three mine types, building
and ground-unit collisions, clearance occupants, reservations, town overlap,
map edges, radius rejection, active UI preview changes, rejected/accepted
commands and replay after live option changes. The active-helper test checks that the same overlaid cell changes from red to
green; the existing renderer test also covers mixed per-cell output. Save tests cover every combination of the
two helper bits; menu tests cover both toggles, persistence, CLI overrides,
legacy settings and unknown bits/values. Economy fixtures now use mineral bytes
for mines and agricultural bytes for farms/defences instead of placing every
structure on a map filled with mineral deposits.

Validation on 2026-11-07: bootstrap with `--no-hooks`, Release configure/build,
and the full CTest suite passed before and after changes. The final suite ran
57 tests successfully; six checks skipped (formats, engine, lockstep processes,
private save import, campaign script inventory and private parity rules).
All pre-commit gates passed, including an explicit check of this new document,
and `git diff --check` passed. No private game or reference process was launched.
Changes remain uncommitted because git metadata is read-only.

Run without game data:

```sh
sh tools/bootstrap.sh --no-hooks
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
```

## Live run sheet (needs live run)

Run on a namespace-capable host from this worktree root. Never bypass failed
isolation. All generated maps, scripts, profiles, logs and captures stay under
ignored `work/`. Use fresh directories per repetition and variant; never modify
the read-only installation. First expand task 66's existing candidate catalogue:

```sh
python3 tools/placement_compare.py list
python3 tools/placement_compare.py prepare --output work/parity/placement101-plan
python3 - <<'PY'
from pathlib import Path
import shutil
source = Path('/home/adrox/aztec-re/game')
for label in ('reference', 'faithful', 'relaxed'):
    target = Path('work/parity/placement101-live') / label / 'game'
    target.mkdir(parents=True, exist_ok=False)
    (target / 'Maps').mkdir()
    (target / 'Saves').mkdir()
    for name in ('Aztec.exe', 'Res', 'Video', 'Music', 'Campaign'):
        if (source / name).exists():
            (target / name).symlink_to((source / name).resolve())
    for item in source.iterdir():
        if item.is_file() and item.suffix.lower() in ('.ini', '.plr', '.cfg'):
            shutil.copy2(item, target / item.name)
    for item in (source / 'Maps').iterdir():
        if item.name.lower().startswith('mission_14.'):
            shutil.copy2(item, target / 'Maps' / item.name)
PY
for label in reference faithful relaxed; do
  python3 tools/make_testmaps.py --game "work/parity/placement101-live/$label/game"
done
```

Before custom-map navigation, remove the copied mission_14 and generated maps
other than ztest_all from each private Maps directory. Retain its layout manifest
for actual shifted coordinates. Audit separate input scripts at
`reference/input.txt`, `faithful/input.txt` and `relaxed/input.txt` under the live
root. The reference uses RT_SCRIPT menu/custom-map navigation; the clone uses
PlaytestScript starting directly in ztest_all. These routes cannot be supplied
accurately until measured: task 66's catalogue is a candidate specification,
not an executable synchronized scenario. Confirm producer, race, prerequisites,
cash, fog, diplomacy, inactive AI, camera, resolution and stable pre-input phase.
Use identical measured world sites and sweep inputs in all three runs.

After those input scripts exist, launch diagnostic captures exactly as follows:

```sh
test -s work/parity/placement101-live/reference/input.txt
timeout -s KILL 90 unshare -rn sh -eu -c '
  ip link set lo up
  ip addr add 192.168.1.125/24 dev lo
  exec env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_RESOLUTION=800x600 \
    RT_SCRIPT="$PWD/work/parity/placement101-live/reference/input.txt" \
    RT_DUMP_DIR="$PWD/work/parity/placement101-live/reference" \
    RT_CMD_LOG="$PWD/work/parity/placement101-live/reference/commands.txt" \
    RT_STATE_LOG="$PWD/work/parity/placement101-live/reference/state.txt" \
    /home/adrox/aztec-re/build/aztec_recomp \
    "$PWD/work/parity/placement101-live/reference/game/Aztec.exe"
'
for label in faithful relaxed; do
  mode=off
  if [ "$label" = relaxed ]; then mode=on; fi
  test -s "work/parity/placement101-live/$label/input.txt"
  timeout -s KILL 90 unshare -rn env \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    AZTEC_USER_DIR="$PWD/work/parity/placement101-live/$label/profile" \
    build/aztec_engine --game "$PWD/work/parity/placement101-live/$label/game" \
    --modifications-file "$PWD/work/parity/placement101-live/$label/modifications.ini" \
    --mod parallel_construction=off --mod "relaxed_placement=$mode" \
    --size 800x600 --software --no-audio \
    --script "$PWD/work/parity/placement101-live/$label/input.txt" \
    --ui-trace "$PWD/work/parity/placement101-live/$label/ui.jsonl" \
    --capture-dir "$PWD/work/parity/placement101-live/$label/frames" \
    --capture-every 1 ztest_all
done
```

These are diagnostic launches. Task 64 must supply audited normalization before
raw traces become aligned `placement-v1` observations. Then compare each clone
bundle, keeping the expected relaxed differences separate from default parity:

```sh
for label in faithful relaxed; do
  python3 tools/placement_compare.py compare \
    --reference work/parity/placement101-live/reference/placement.json \
    --clone "work/parity/placement101-live/$label/placement.json" \
    --output "work/parity/placement101-live/$label/comparison.json"
done
```

For each building class/race, repeat clear and blocked controls twice. Move one
obstacle through every mask position (0/1/2) and just outside the mask. Separately
vary `.dat` 0/1/2, actual tree/rock/decorative overlays on class 2, resources
low-only/high-only/both/zero and high-byte values 1/65, mineral on body versus
clearance, partial/full deposits and map edges. Place completed/incomplete
buildings, ground/air units and reservations of every relation at identical sites;
record invalid clicks, accepted commands and resulting construction separately.
Sweep axes/diagonals around own/allied/enemy centres and borders one tile at a
time. Include farm and mine controls on agricultural land, bare mineral rock,
overlaid mineral rock, mixed terrain and occupied deposits. Record which cells
are tested, the first rejecting reason, snap position, per-cell colour and
acceptance. Do not infer validity from an attractive ghost frame alone.

Only these original-game measurements and the missing normalization dependency
have status **needs live run**. Deterministic synthetic success establishes the
implemented policies and switch plumbing, not original-game balance or parity.
