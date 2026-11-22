# Native CI investigation

Investigation checkout: `5bac978` (2026-11-22), with uncommitted fixes.
**V** means verified from repository history, source or a local synthetic run.
**G** means an inference or an unresolved native-platform explanation. Linux
results do not establish that the Windows or macOS jobs are green.

## Confirmed menu bounds defect

**V:** `ModificationOptions::draw` and `update` iterated seven rows. There are
four settings, one profile selector and one Back button: six rows. Row 6 took
the ordinary-setting branch and computed index 5 into the four-element
`Modifications::names` array. Drawing constructed a string from the invalid
pointer; clicking that row performed another invalid read. This is undefined
behavior, even when a particular optimized Linux binary happens to survive.

**V:** `git blame` and `git show 4516937 -- src/engine/ui/frontend.cpp` identify
the introducing change as task 124, `4516937f1fb245e37dae8fdc33ca24c19db73183`:
both modifications-menu loops changed from six iterations to seven alongside
audio/settings work. This predates tasks 152–162. Those later commits do not
introduce these two invalid loop bounds. A native job's first observed failure
could still be later, because undefined behavior depends on the binary layout.

**V:** Before the fix, Debug with ASan, UBSan and `_GLIBCXX_ASSERTIONS` aborted
both `campaign_ui` and `ui_capture` at `std::array<const char*, 4>::operator[]`
with `Assertion '__n < this->size()' failed`. The latter failure was specifically
`test_campaign_route_with_synthetic_resources`. The ordinary Release baseline
passed both tests. The standalone expression `Modifications{}.setting(5)` also
aborted with the library assertion.

**G:** This defect explains the reported Windows menu access violations. A
native rerun or stack trace is still required to connect the reported last menu
frame 29 to the invalid read; the local assertion alone does not identify the
Windows instruction that faulted.

**V:** The fix derives the Back row and total row count from the setting-name
array and shares them between drawing and input. Existing row positions stay
the same. Regression coverage draws the modifications page at the existing
synthetic surface sizes, clicks the former extra row without changing settings
or closing the page, and verifies that Back still closes it. Sanitizer builds
now define `_GLIBCXX_ASSERTIONS`, so the existing CI sanitizer configuration also
checks libstdc++ container bounds.

## Timing comparison for tasks 152 and 153

**V:** Historical snapshots were extracted with `git archive` into ignored
`build_native_history/source`, without changing git metadata. Each engine was
built in Release without private inputs. The same current Python tests were run
against each executable; all runs below passed. Times are single Linux samples,
including Python and child processes, rounded to hundredths of a second. CPU
time is the sum of child user/system time measured with `getrusage`.

| Engine revision | playthrough wall / CPU seconds | pathing wall / CPU seconds |
| --- | --- | --- |
| `5bb9c6a` (parent of task 152) | 1.56 / 1.28 | 9.45 / 7.75 |
| `210a439` (task 152 wave search) | 1.59 / 1.29 | 9.44 / 7.74 |
| `333ed72` (task 153 unit tactics) | 1.59 / 1.31 | 10.37 / 8.51 |
| `5bac978` plus menu fix | 1.54 / 1.28 | 10.31 / 8.42 |

**V:** Task 152 produces no material change in these samples. Task 153 adds
about 10% to the pathing fixture's wall and CPU times. The current playthrough
fixture remains approximately unchanged. These measurements do not show a
Linux regression approaching the reported 60/120-second native timeouts.

**G:** A slower runner, native SDL behavior, process/filesystem overhead, or a
platform-specific slowdown could account for the AppleClang failures. These
Linux samples cannot distinguish them. No timeout was raised and no movement
or acquisition behavior was changed on the basis of this hypothesis.

## Missing native evidence

**V:** `gh run list --branch develop` and the workflow-filtered variant failed
with `error connecting to api.github.com`. A `gh api` connectivity attempt also
failed; the web fallback could not retrieve the public Actions API. Consequently
no actual job IDs or job logs were obtained, and no comparison with earlier
green AppleClang CI runs was possible. The failure descriptions in the task are
reported symptoms, not independently retrieved CI evidence.

**V:** `custom_mission_skirmish` passes locally in both Release and the strict
Debug configuration. History shows that task 156 (`2c047d7`) updates its expected
script termination ticks, but this does not prove that it fixes the reported
MSVC failure. No separate MSVC root cause or first failing native run is verified.

**G:** The remaining native investigation needs the latest failing code-build
run and the preceding green runs, including the individual custom-mission
assertion output and AppleClang per-test durations. On a network-enabled host:

```sh
gh run list --branch develop --workflow 'Build and test' --limit 100
gh api repos/Adrixop95/quetzal-engine/actions/runs/RUN_ID/jobs
gh api repos/Adrixop95/quetzal-engine/actions/jobs/JOB_ID/logs
```

Inspect these code landings in order when locating the first failing native run:
`210a439` (152), `333ed72` (153), `3af2ec4` (154), `77dbd14` (155),
`2c047d7` (156), `b2c939d` (157), `d9e696d` (160), `d123a01` (158),
`041dfeb` (161), `41e0f56` (162). Documentation-only runs can skip the native
jobs, so their green status does not establish a passing native baseline.

## Local validation

**V:** Before changes, bootstrap with `--no-hooks`, Release configuration/build,
all 82 CTest entries and pre-commit passed (six CTest entries skipped for absent
private data or restricted networking). No original executable or reference
runtime was launched, and no installed game data was needed.

**V:** After changes, the Release rebuild and all 82 CTest entries passed again
(the same six skipped), as did pre-commit, including an explicit check of this
new report. `campaign_ui` and `custom_mission_skirmish` also passed with a 1 MiB
Linux stack limit; this is not a substitute for an MSVC run.

**V:** The strict Debug unit run passed 74 of 75 checks, including `campaign_ui`
(7.11 s), `ui_capture` (18.48 s), `custom_mission_skirmish` (0.52 s),
`playthrough_framework` (9.54 s) and `pathing_stress` (60.77 s). The only failure
was the existing 90-second `save_state_processes` timeout in this unoptimized
instrumented build; Release passed it in about six seconds. No address,
undefined-behavior or container-bounds diagnostic was reported after the fix.
Repository timeout limits remain unchanged.

**V:** Running `tests/test_save_state_processes.py` directly against the same
Debug binary with a 300-second local deadline then completed successfully:
all fifteen synthetic scenarios, save boundaries and continued ticks matched
in fresh processes. This resolves the functional check while retaining the
failed 90-second CTest result as a timing limitation of this configuration.

The stricter build uses:

```sh
cmake -S . -B build_native_debug -DCMAKE_BUILD_TYPE=Debug \
  -DAZTEC_GEN_DIR= -DGAME_DIR= -DAZTEC_SANITIZERS=ON \
  '-DCMAKE_CXX_FLAGS=-D_GLIBCXX_ASSERTIONS -fstack-protector-strong -fno-sanitize-recover=all -fsanitize-address-use-after-scope'
cmake --build build_native_debug --parallel 2
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build_native_debug -L unit --output-on-failure
```

**V:** Valgrind is unavailable locally. LeakSanitizer with `detect_leaks=1`
fails in this sandbox with its fatal ptrace/process-inspection diagnostic;
`detect_leaks=0` leaves address/undefined-behavior instrumentation active. That
environmental failure is distinct from the reproducible menu bounds assertion.
