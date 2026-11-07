# Task 100: parallel training

The independent engine implements `parallel_training=1..8` and
`parallel_training=per building` (`per-building` on the command line).
Default `1` is off. See [modifications.md](modifications.md) for settings,
queue policy, production-building matching, save metadata and live toggles.
Use **Options > Game modifications** or the in-game pause menu. F9 retains its
existing cursor-confinement function.

## Implementation and evidence

This task uses the existing modification bitset, persisted `modifications.ini`,
`SetModifications` command and command-log saves. Each queue item already owns its
payment, remaining work and pause state; the scheduler now assigns several unit
slots per town. Building concurrency remains independently controlled. Extra
production buildings can be purchased in per-building mode. Both sides use the
same setting; the AI fills an affordable batch from its existing target only
when training concurrency is enabled.

Synthetic checks cover:

- Default, explicit `1` and `off`: equal complete state hashes every tick,
  one running unit, and baseline serial payment/progress/completion checks.
- Values 1–8: payment per started unit, waiting order, N first-cycle completions,
  save metadata round-trip and identical replay/continuation hashes every tick.
- Cash shortage, unpaid cancellation, independent paid refund and pause timers.
- Two and three barracks, capacity shared by different matching unit recipes,
  independent stable capacity, and side-wide technology excluded from capacity.
- Incomplete, removed, enemy and other-town buildings excluded from capacity.
- Live enable/disable, retained paid work, per-slot Shift-click pause and replay.
- Computer training on both sides, default serial purchases, enabled batches,
  planner-target accounting and deterministic recomputation.
- Interaction with serial/parallel construction, persisted menu settings, CLI
  overrides, invalid values and lockstep mismatch rejection.

The default-mode test compares complete hashes between the implicit default and
explicit `1`/`off` at initialization and every tick through 120. It also checks
serial queue size, lump payment, unpaid waiting entries, progress and same-tick
handover against baseline production facts at ticks 1, 50 and 120. This avoids
pinning unrelated movement or world state while still testing serial behavior.
It establishes internal consistency, not universal original-game equivalence.
The baseline Release build and all 63 CTest entries completed without failures;
six entries skipped for unavailable private inputs or restricted networking.
Final verification also passed: Release build, 57 passed CTest entries with the
same six skips, pre-commit on all tracked files and the new task document, and
`git diff --check`. No files were committed; git metadata remains read-only.

No reference runtime, original assets, generated code or decompilation was used.
The modification does not depend on unmerged task 96 (AI early spending); its
existing planner/spending behavior remains the foundation. This task makes no
claim to fix that planner's baseline parity gaps or to measure original training
prices, refunds, timing, spawn positions or production-building restrictions.

## Reproduce synthetic verification

Run from the worktree root. Build outputs stay inside this worktree.

```sh
sh tools/bootstrap.sh --no-hooks
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
pre-commit run --files docs/parallel_training.md
```

## Original-game baseline: needs live run

Only original-game observations and real mission acceptance need a live run.
The modification itself is intentionally absent from the original. Codex did
not run the reference: network namespaces are unavailable in this environment.
Use the following commands on a namespace-capable host, from this worktree.
Do not remove isolation when setup fails.

First author and audit a private `work/parallel-training-input/reference.rt`
script through normal reference UI input. It must select mission 1, queue several
units, pause/resume production and cancel waiting and active entries, then quit.
Use the documented 800x600 menu navigation in EXPERIMENTS.md as a starting point;
audit town selection and production-card coordinates in the actual mission.
Also provide `work/parallel-training-input/replay.rt` with the same startup and
mission navigation, but no gameplay commands: recorded packets supply those.
Keep the same duration before quitting so the replay covers the recording.
Record the initial profile, town, speed, cash, queue acceptance and observation
phase. Millisecond waits alone do not locate exact simulation boundaries.
The private input is required, not supplied or fabricated by this task.

Create three independent private installations with immutable asset symlinks and
ordinary copied Maps/Saves/profile files. The existing helper handles that split:

```sh
export training_probe="$PWD/work/parallel-training-live-$(date -u +%Y%m%dT%H%M%S)"
python3 -I - <<'PY'
import importlib.util
import os
from pathlib import Path
spec = importlib.util.spec_from_file_location('report', 'tools/parity_report.py')
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)
scripts = [Path('work/parallel-training-input') / name
           for name in ('reference.rt', 'replay.rt')]
for script in scripts:
    if not script.is_file():
        raise SystemExit('Missing audited reference input: ' + str(script))
root = Path(os.environ['training_probe'])
root.mkdir(parents=True)
for script in scripts:
    (root / script.name).write_bytes(script.read_bytes())
for name in ('record', 'repeat', 'replay', 'clone-1', 'clone-3', 'clone-per-building'):
    directory = root / name
    directory.mkdir()
    report.private_game(Path('/home/adrox/aztec-re/game'), directory / 'game')
PY
```

Record twice independently, then replay the first recording. All paths are
private. Each launch has its own namespace and copied writable installation.
An input that quits successfully can still fail to exercise the intended queue;
inspect actual acceptance and command readback before interpreting results.

```sh
for training_mode in record repeat replay; do
  training_run="$training_probe/$training_mode"
  (
    cd "$training_run/game" || exit 1
    if [ "$training_mode" = replay ]; then
      export RT_CMD_REPLAY="$training_probe/record/commands.txt"
      training_script="$training_probe/replay.rt"
    else
      unset RT_CMD_REPLAY
      training_script="$training_probe/reference.rt"
    fi
    timeout -s KILL 240 unshare -rn env \
      SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
      RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_RESOLUTION=800x600 \
      RT_SCRIPT="$training_script" RT_DUMP_DIR="$training_run" \
      RT_CMD_LOG="$training_run/commands.txt" RT_STATE_LOG="$training_run/state.txt" \
      RT_UNIT_LOG="$training_run/units.txt" \
      sh -ec '
        ip link set lo up
        ip addr add 192.168.1.125/24 dev lo
        exec /home/adrox/aztec-re/build/aztec_recomp "$1/Aztec.exe"
      ' sh "$training_run/game"
  ) > "$training_run/stdout.log" 2> "$training_run/stderr.log"
  printf '%s\n' "$?" > "$training_run/exit-status.txt"
done
```

Before launching, clear unrelated inherited `RT_*` experiment switches. Record
runtime/build hashes and all controls privately. Review both reference repeats
for lump start payments, cancel refund amounts, queue transitions, pause and
spawn ticks. Raw runtime and engine state hashes have different representations;
do not compare them directly. Exact queue normalization/payment observers are
still prerequisites for a normalized parity verdict; these launch commands do
not provide those missing semantic mappings.

## Real mission clone smoke and continuation: needs live run

The private installations created above can exercise real-data startup, AI,
saves and determinism without writing into the source installation. These exact
headless commands deliberately use no player orders; they are smoke checks, not
proof of queue UI acceptance or mission completion. Both halves use dummy SDL
and `unshare -rn`. Keep errors, skips and timeouts as failures.

```sh
for training_value in 1 3 per-building; do
  training_run="$training_probe/clone-$training_value"
  timeout -s KILL 180 unshare -rn env \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    AZTEC_USER_DIR="$training_run/profile" \
    "$PWD/build/aztec_engine" --headless --game "$training_run/game" \
    --rules-dir "$PWD/docs/data" --mod parallel_construction=off \
    --mod "parallel_training=$training_value" --no-auto-orders --max-ticks 1200 \
    --save-at 600 --save "$training_run/midpoint.azs" \
    --tick-hashes "$training_run/whole.hashes" --report "$training_run/whole.json" 1
  printf '%s\n' "$?" > "$training_run/whole-exit-status.txt"
  timeout -s KILL 180 unshare -rn env \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    AZTEC_USER_DIR="$training_run/resumed-profile" \
    "$PWD/build/aztec_engine" --headless --game "$training_run/game" \
    --rules-dir "$PWD/docs/data" --resume "$training_run/midpoint.azs" \
    --no-auto-orders --max-ticks 1200 \
    --tick-hashes "$training_run/resumed.hashes" --report "$training_run/resumed.json" 1
  printf '%s\n' "$?" > "$training_run/resumed-exit-status.txt"
done
```

Check terminal state hashes and every overlapping tick after the save boundary.
The resumed setting comes from the save even if CLI/preferences differ. Separately
play mission 1 through the UI with an isolated profile: choose 3, queue more than
three units, inspect every active bar, pause a middle unit, cancel another, save
and reload. Repeat with two and three barracks in per-building mode, and switch
back to 1 while several units are running. Confirm mode persistence after a fresh
process. The new synthetic fixture covers these mechanisms; real display and
original-data integration remain unobserved. Keep all captures, scripts, logs,
original saves and derived data under ignored `work/`.

## Landing regression correction

The first landing onto develop failed only in `engine_unit`: six assertions used
three whole-world hash constants recorded before task 99 landed. Develop revision
`ab389c3` adds formation obstacles during `GameSim::init` and includes them in the
movement state hash, including for this fixture's town hall. The old constants
therefore described an obsolete world-state representation; updating them would
repeat the same integration problem on a future unrelated hash change.

Reproduced the six failures in a fresh `git archive develop` snapshot under
ignored `build_landing/source`, with the task patch applied and a separate Release
build in `build_landing/build`. The correction replaces the cross-revision golden
hashes with per-tick default/explicit-off equality and serial production facts.
No training scheduler or formation behavior changed in this correction.

After the correction, the clean develop snapshot with the task applied builds
successfully and passes its complete 65-entry suite: 59 passed, six skipped
(private-data checks and restricted UDP process creation). The original worktree
also passes its complete 63-entry suite: 57 passed, the same six skips. Pre-commit
on all tracked files and this new document, plus `git diff --check`, pass.
