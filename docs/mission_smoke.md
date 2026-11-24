# Campaign mission smoke runner

`tools/mission_smoke.py` exercises all fifteen campaign missions using the
independent engine and the user's installation. It sends no player input:
`--no-auto-orders` disables the headless exercising policy, while ordinary
computer AI and mission scripts continue to run. The default budget is 720
simulation ticks per mission. Victory or defeat ends a run early; reaching the
tick budget is a successful smoke result, not a wall timeout or a campaign win.

Build the clone normally, then run from the repository root:

```sh
python3 -I tools/mission_smoke.py --ticks 720
python3 -I tools/mission_smoke.py --game /absolute/path/to/game \
  --engine build/aztec_engine --ticks 720 --timeout 120 \
  --output work/mission-smoke-local
```

Game-directory selection uses the engine's own configuration resolver, without
loading game data: `--game`, nonempty `AZTEC_GAME`, `--settings FILE` or the normal
per-user settings file, then `./game`. Relative paths resolve from the caller's
working directory. An absent installation or missing Maps directory prints a
clear `SKIP` and returns zero. Once Maps exists, missing campaign files, broken
scripts and incomplete installations are failures; remaining missions still run.
Malformed settings are errors, including when another path override is supplied,
matching normal engine behavior. Build the engine before invoking the tool.

Live collection requires Linux `unshare` and GNU `timeout`. Every mission uses a
fresh private installation with resource/map symlinks, copied mutable
configuration, ordinary Maps/Saves directories and an isolated engine user
profile. Both SDL drivers are `dummy`. Every simulation runs inside
`timeout -s KILL N unshare -rn`; namespace failure is retained as a failed launch.
There is no isolation bypass. The engine's configuration query loads no mission
and runs no simulation. Neither the original executable nor the reference
runtime is launched.

Output must be a fresh directory inside this worktree's ignored `work/` or
`build/`. Resolved paths cannot redirect reports into tracked directories or
overlap installation, rules or engine inputs. Existing output is never replaced.
The console prints only aggregate success/failure counts and the report path.
Keep every per-mission artifact private; do not copy generated tables into docs,
tests or source control.

Each private mission directory contains stdout/stderr, the engine report and a
process result. The top-level `report.json` is updated after each mission and
includes the binary and gap-list hashes, requested budget and process commands.
It distinguishes load/runtime errors, crashes, wall timeouts, launch/isolation
failures, malformed reports and sanitizer diagnostics. Exit status is zero for
all missions passing or absent-data skip, one for any mission failure, and two
for setup/configuration errors. Known script approximations and unresolved
descriptors remain diagnostics rather than automatically failing a bounded run.

`--smoke-diagnostics` adds these opt-in fields to the existing headless engine
report, leaving normal deterministic reports unchanged:

- Script inventory includes every parsed action and condition, including dormant
  sites. The wrapper maps stable engine names to native hexadecimal codes and
  reads dispositions and explanations from
  [the runtime gap list](script_runtime_gap_list.md). Approximate sites,
  unsupported execution and intentional parser no-ops are distinguished.
  Existing action observations give actual execution counts. Inventory does not
  establish that a site executes or that a condition takes either branch. The
  private manifest also ranks `script_gap_totals` by site count, grouped by
  section, native code, kind and disposition; failed launches add no inventory.
- Unresolved descriptors identify object IDs, types, first observed ticks and
  reasons. Unknown types and missing primary-HP descriptor provenance are checked
  before initialization and after every completed tick, including cargo and new
  objects. Untyped submarine decoration controllers are intentional and excluded.
  This detects descriptor fallback, not every unknown descriptor field or missing
  visual/weapon behavior.
- Tick timing measures `World::begin_tick` plus `GameSim::tick` with a monotonic
  clock. Reports retain total and maximum completed-tick seconds; the wrapper
  adds their mean. Loading, hashing, diagnostic scans and report I/O are excluded.
  Process wall time includes startup and loading. Timing is instrumentation-
  and machine-dependent and never a deterministic or reference-parity metric.

Use an instrumented clone for sanitizer coverage:

```sh
cmake -S . -B build_mission_sanitized -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DAZTEC_GEN_DIR= -DAZTEC_SANITIZERS=ON
cmake --build build_mission_sanitized --target aztec_engine --parallel 2
python3 -I tools/mission_smoke.py --game /absolute/path/to/game \
  --engine build_mission_sanitized/aztec_engine --sanitizer-build \
  --ticks 720 --timeout 300 --output work/mission-smoke-sanitized-local
```

`--sanitizer-build` records the caller's instrumentation declaration; it does not
instrument a Release binary. Sanitizer log signatures, including UBSan's
`runtime error:`, fail a mission even if its process exits zero. A plain Release
run with no such output does not establish sanitizer coverage. Keep the original
logs for stack traces. The manifest records sanitizer environment options.

## Local validation, 2026-11-23

The Release sweep loaded and ran all 15 missions to the requested 720-tick
budget: 15 passed, zero load failures, crashes or unresolved descriptor objects.
The inventory reported 255 approximate script sites across actions and
conditions, one intentional parser no-op and zero unsupported execution sites.
Two missing-trigger target diagnostics were retained; these did not prevent
loading or bounded execution. These aggregate diagnostics do not prove trigger
reachability, campaign completion or fidelity to the reference.

The separate ASan/UBSan sweep also loaded and ran all 15 missions to the same
720-tick budget: 15 passed, zero load failures, crashes, sanitizer log findings
or unresolved descriptor objects. LeakSanitizer failed during the initial
configuration query under the sandbox's tracing environment. The completed
collection used `ASAN_OPTIONS=detect_leaks=0`; leak detection was not verified.
The same five synthetic smoke controls passed against the instrumented binary.
Private reports are under `work/mission-smoke-release/` and
`work/mission-smoke-sanitized/`; the latter also retains the collection
environment note. The final Release suite completed with 79 passes and six
skips across 85 CTest entries. Separate private-data format and engine checks
passed under dummy SDL and `unshare -rn`; pre-commit passed for tracked and new
files.

Synthetic controls exercise dormant unsupported inventory, native-code mapping,
configuration precedence, opt-in report stability, descriptor fallback, missing
installation skip, private output restrictions, crash/sanitizer/isolation failure
reporting and continuing the full sweep after a script load error. The live
sweep control uses invented missions and skips when Linux namespaces are
unavailable; the other controls are registered for native Windows and macOS builds.
Local Linux results do not verify native Windows/macOS CI.

The general side-plan executor, nested tactic/controller fidelity and other
runtime gaps listed in the linked audit remain absent or approximate. The smoke
runner does not depend on those missing behaviors and does not replace them.
No mission-specific engine fixes were necessary in the Release sweep. Per-mission
data, timings and logs remain only in ignored output.
