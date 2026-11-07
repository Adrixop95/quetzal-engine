# Task 104: faithful and enhanced play profiles

Implementation and synthetic validation cover profile presets, CLI precedence,
preference migration and overrides, F9 menu edits, unchanged faithful hashes,
and save/replay of effective helper settings. The regression hashes come only
from an invented movement fixture without buildings or mission groups, not the
game executable. Construction also compares named faithful settings against the
legacy all-off bitset. Construction semantics are unchanged. Tasks 100-102 are not merged;
training, placement and unstick remain explicitly labelled configuration stubs.

No reference or private-data game was run in this restricted environment.
Original-game behavior and real installation/display acceptance are **needs live
run**. This is a limitation of live validation, not an unfinished profile
implementation. Disabling helpers does not resolve unrelated engine parity gaps.

## Independent verification

```sh
sh tools/bootstrap.sh --no-hooks
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
pre-commit run --files docs/game_modifications.md docs/104-profiles-faithful-and-enhanced.md tests/test_profiles.cpp tests/test_profiles.py
```

The baseline passed all 63 CTest entries with six expected skips (private data
and restricted UDP process creation), and all pre-commit gates. The added profile
check uses two independent preference-client processes. The playthrough check
uses invented resources and launches two actual graphical engine processes with
dummy SDL, edits helpers through F9, then inspects persisted overrides.

The completed Release build and 64-entry CTest suite passed: 58 checks passed and
six skipped for the same private-data/network prerequisites. After the final
Ctrl+F9 shortcut adjustment, all six affected UI/profile/process checks passed
again. Both all-file and new-file pre-commit gates passed. No git metadata was
changed; all implementation files remain uncommitted.

## Landing regression repair

Landing on develop `ab389c3` exposed three stale construction hash constants.
Task 99 registers halls as formation obstacles and includes those obstacles in
the movement digest. The old construction fixture therefore changed before
profiles were applied. The same three assertions failed in an isolated archive
of develop with the original profile patch, reproducing the landing failure.

The fixed regression uses a mobile unit without buildings or mission groups for
its six golden hashes (initial state and ticks 1-5). An independent, unmodified
develop build produced exactly the same six hashes. Construction separately
compares faithful against the legacy all-off bitset at initialization and ticks
1 and 5, while retaining its production, save and replay assertions. No engine
behavior or formation implementation was changed by this repair.

Verification after the fix:

- Original worktree: Release build; 64 CTest entries, 58 passed and six skipped.
- Isolated develop plus the complete profile patch: Release build; 66 CTest
  entries, 60 passed and six skipped. Private-data and namespace/UDP prerequisites
  account for skips in this environment.
- Unmodified develop: Release `test_engine` build and `engine_unit` passed;
  the independently linked synthetic movement probe matched all golden hashes.
- All-file and new-file pre-commit gates and `git diff --check` passed.

All archived sources, builds and probe outputs stay in ignored `build_landing/`
inside this worktree. Git metadata and the three documentation hotspots remain
unchanged. The repair updates `tests/test_engine.cpp`, `docs/game_modifications.md`
and this task note; the earlier profile implementation remains uncommitted.

## Exact live run commands

Run only on a namespace-capable host. Keep outputs under ignored `work/` and use
separate private installations for each process. The existing parity harness
creates private resource symlinks, copies mutable data and launches dummy SDL
under bounded `unshare -rn`; its clone launcher now passes `--profile faithful`.
It does not pass clone options to the original executable.

```sh
profile_probe="$PWD/work/profiles-live-$(date -u +%Y%m%dT%H%M%S)"
mkdir -p "$profile_probe"
for profile_repeat in 1 2; do
  python3 -I tools/parity_run.py private_l0 \
    --game /home/adrox/aztec-re/game \
    --runtime /home/adrox/aztec-re/build/aztec_recomp \
    --engine "$PWD/build/aztec_engine" --timeout 120 \
    --output "$profile_probe/parity-$profile_repeat"
  profile_status=$?
  printf '%s\n' "$profile_status" > "$profile_probe/parity-$profile_repeat.exit-status.txt"
done
```

Retain failures and inspect both `runners.json` files and process logs. These
initial-state comparisons are limited by the existing observers and do not prove
construction parity. Use an independently audited original construction route to
verify payment, progress and cancellation before making that claim.

For paired faithful CLI checks on real data, first stage two separate copies
with the established private-game helper. Immutable resources are symlinked;
Maps, profiles and Saves remain private ordinary files/directories.

```sh
export profile_probe
python3 - <<'PY'
import importlib.util
import os
from pathlib import Path

spec = importlib.util.spec_from_file_location('playthrough', 'tools/playthrough.py')
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)
source = Path('/home/adrox/aztec-re/game')
maps, _ = harness.discover(source)
root = Path(os.environ['profile_probe'])
for name in ('faithful', 'overrides', 'interactive'):
    run = root / name
    run.mkdir()
    harness.private_game(source, maps, run / 'game')
PY
for profile_case in faithful overrides; do
  profile_run="$profile_probe/$profile_case"
  profile_extra=()
  if [ "$profile_case" = overrides ]; then
    profile_extra=(--profile enhanced --mod parallel_construction=off \
      --mod parallel_training=off --mod relaxed_placement=off --mod unit_unstick=off)
  fi
  timeout -s KILL 120 unshare -rn env \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy AZTEC_USER_DIR="$profile_run/user" \
    "$PWD/build/aztec_engine" --headless --profile faithful "${profile_extra[@]}" \
    --game "$profile_run/game" --rules-dir "$PWD/docs/data" \
    --no-auto-orders --max-ticks 120 --tick-hashes "$profile_run/hashes.txt" \
    --report "$profile_run/report.json" 1 \
    > "$profile_run/stdout.log" 2> "$profile_run/stderr.log"
  printf '%s\n' "$?" > "$profile_run/exit-status.txt"
done
cmp "$profile_probe/faithful/hashes.txt" "$profile_probe/overrides/hashes.txt"
cmp "$profile_probe/faithful/report.json" "$profile_probe/overrides/report.json"
```

The last block requires Bash arrays. Require successful process statuses and
complete reports before interpreting `cmp`. It compares two clone configurations,
not the clone against the original. No captures or private numeric observations
belong in the public repository.

For actual interactive acceptance on a display-capable machine, the staging
block also creates an `interactive` private installation. Run:

```sh
AZTEC_USER_DIR="$profile_probe/interactive/user" \
  build/aztec_engine --game "$profile_probe/interactive/game" --profile enhanced 1
AZTEC_USER_DIR="$profile_probe/interactive/user" \
  build/aztec_engine --game "$profile_probe/interactive/game" 1
```

On the first run use F9 to change the profile and individual switches, resume,
and quit. On the second inspect the same values without a CLI profile reset.
Check Ctrl+F9 confinement, focus release and visible construction progress separately.
This manual display check intentionally needs real SDL drivers. Repeat original
construction controls with audited normal input in the isolated reference;
observer/route gaps remain blockers, never reasons to bypass isolation.
