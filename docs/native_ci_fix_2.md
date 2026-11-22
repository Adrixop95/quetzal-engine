# Remaining native CI failures after task 163

Investigation revision: `91b8f6e` (PR 296), with uncommitted fixes.
**V** means verified from job logs, repository/upstream source, or the specified
Linux synthetic experiment. **G** means an inference requiring a native rerun.
These labels do not claim original-game parity. No original executable, private
runtime, installation, assets, or executable-derived source was used.

## Retrieved jobs and history

**V:** Local `gh pr checks 296`, `gh run list --branch develop --workflow
'Build and test' --limit 100`, and `gh api` failed to connect to api.github.com.
The connected GitHub tool successfully retrieved the equivalent PR metadata,
workflow/job lists and decoded job logs. PR head `2119da3` ran as
`00a208e`, the temporary merge into `5bac978`, in
[Build and test 45042279743](https://github.com/Adrixop95/quetzal-engine/actions/runs/45042279743).

| Job | Observed failure |
| --- | --- |
| [MSVC 137269232135](https://github.com/Adrixop95/quetzal-engine/actions/runs/45042279743/job/137269232135) | `custom_mission_skirmish`, 0.22 s; only assertion 214, absence of `Saves/engine_campaign.txt` |
| [AppleClang 137269232159](https://github.com/Adrixop95/quetzal-engine/actions/runs/45042279743/job/137269232159) | `mouse_grab`, 0.05 s; assertion 31 three times; framework/stress reach 60.10/120.12 s limits |
| [MinGW 137269232104](https://github.com/Adrixop95/quetzal-engine/actions/runs/45042279743/job/137269232104) | MSYS2 database signature downloads time out; configure, build and tests never execute |

**V:** The MinGW failure occurs at package setup: `mirror.msys2.org` transfers
less than one byte per second for ten seconds while downloading database
signatures. There is no failing engine test or introducing source commit to fix
in this run. A fresh infrastructure run is needed; no dependency verification
or signature checks were disabled.

**V:** The same macOS failures occur in the inspected task-152, task-153 and
task-161 jobs: `136885985391`, `137187011359`, and `137225889227`. They are
not new failures introduced by PR 296. Documentation-only green workflow runs
contain no native jobs and cannot establish a passing test baseline.

## Windows profile isolation

**V:** The first landing containing `custom_mission_skirmish` is task 125,
`634838a` (PR 239). Its
[MSVC job 136453115822](https://github.com/Adrixop95/quetzal-engine/actions/runs/44783759405/job/136453115822)
already fails the same assertion in 0.21 s. The test does not demonstrate a
skirmish script, command-line, save replay, or mission-result failure: all its
printed tick/outcome checks pass.

**V:** The test sets `AZTEC_USER_DIR` with `SDL_setenv`, while `user_file` reads
it with `std::getenv`. SDL2's
[Windows setter](https://github.com/libsdl-org/SDL/blob/release-2.32.10/src/stdlib/SDL_getenv.c)
uses `SetEnvironmentVariableA`; this need not update the application's C runtime
environment table. The repository already addresses this distinction in
`test_portability.cpp`, introduced with task 51 (`c9e69f0`). Campaign UI and
campaign-flow fixtures also used the mismatched setter. In particular,
`campaign_ui` writes an unlock file before the skirmish test runs in CTest.

**G:** On the failing Windows job the ignored override makes those fixtures
share SDL's default preference directory. The skirmish assertion then sees
the campaign fixture's unlock file. The job log does not print the resolved
profile path, so that particular Windows path remains inferred.

**V:** A Linux fault-injection experiment reproduces precisely assertion 214.
A temporary preload intercepts only `SDL_setenv("AZTEC_USER_DIR", ...)`, returns
success, and leaves the C runtime value unchanged. The inherited profile points
to a private directory inside `build_native_probe/`, seeded with an invented
campaign progress file. The original test fails assertion 214. The updated test
passes under the same preload and inherited profile: its C runtime setter
selects its own temporary directory. This does not reproduce the Windows CRT
implementation, but directly verifies the cross-test contamination mechanism.

**V:** The fix extracts the existing `_putenv_s`/`setenv` test helper into
`tests/test_env.hpp` and uses it in all three affected profile fixtures and the
portability check. Each affected fixture now checks the exact resolved profile
path and exits before engine calls if isolation fails. The skirmish check still
requires that no campaign-progress file was created. Engine environment-reading
semantics remain the same.

## macOS resize assertion

**V:** Task 124, `4516937` (PR 241), added the failing resize/gamma assertions.
Its [first PR job 136784015978](https://github.com/Adrixop95/quetzal-engine/actions/runs/44883592078/job/136784015978)
already reports assertion 31 three times. The preceding task-125
[AppleClang job 136453115644](https://github.com/Adrixop95/quetzal-engine/actions/runs/44783759405/job/136453115644)
passes `mouse_grab` in 0.02 s. None of tasks 152–163 changed that assertion.

**V:** The assertion compares renderer output pixels immediately after resize
with the requested logical window dimensions. `Window` explicitly permits
HiDPI output and documents the distinction. The macOS job installs
`sdl2-compat` 2.32.74 over SDL3 3.4.20, rather than the Linux SDL2 2.32.10
implementation. The compatibility layer forwards output-size queries to SDL3.
The failure output contains no actual width/height values and reports no
mouse-confinement or gamma-pixel failures.

**G:** Deferred renderer resize notification explains the mismatch; HiDPI is
another reason physical pixels cannot be equated to window coordinates. A
native rerun is needed to distinguish the actual dimensions on that runner.

**V:** The test now pumps resize notifications, checks the requested logical
size through `SDL_GetWindowSize`, redraws, checks renderer pixels against the
software window surface, and verifies mouse-coordinate scaling. The following
gamma readback still verifies recreation after resize. All mouse policy,
fullscreen and Alt/focus checks remain enabled. No arbitrary delay or platform
skip was added.

## macOS virtual-time pacing

**V:** There is no earlier green timing for either of these tests in the
inspected history. The
[first task-112 PR run](https://github.com/Adrixop95/quetzal-engine/actions/runs/43581324760/job/132406520730)
already times out `playthrough_framework` at 60.06 s. Task 112 landed as
`2ff6dc4`. The
[task-128 landing run](https://github.com/Adrixop95/quetzal-engine/actions/runs/43691283931/job/132742381960)
already times out its newly added `pathing_stress` at 120.06 s; it also times out
the framework at 60.07 s. Task 128 landed as `e747a93`. The task-129 run repeats
both timeouts. The task-125 macOS run reaches 60.05/120.07 s. These precede
the wave-search and unit-scan changes in tasks 152 and 153.

| macOS job / landing | Framework seconds | Stress seconds |
| --- | --- | --- |
| `132406520730` / task 112 PR | 60.06 timeout | not yet present |
| `132742381960` / task 128 | 60.07 timeout | 120.06 timeout |
| `132751758118` / task 129 | 60.05 timeout | 120.08 timeout |
| `136453115644` / task 125 | 60.05 timeout | 120.07 timeout |
| `136885985391` / task 152 | 60.06 timeout | 120.04 timeout |
| `137187011359` / task 153 | 60.07 timeout | 120.03 timeout |
| `137269232159` / PR 296 | 60.10 timeout | 120.12 timeout |

**V:** `git blame` traces the unconditional frame limiter to the initial engine
loop (`18c6db1`), before scripted playback (`63910fa`). The newly added large
test suites exposed that older pacing policy. Task 161 changes the
`original_look` branch, but these fixtures do not request that option.
`MoverConfig::original_ground` defaults to false and is not enabled by the
route runner; the task-152 wave search is not on these fixtures' execution path.
The earlier Linux before/after-task-152/153 measurements are recorded separately
in [native_ci_fix.md](native_ci_fix.md); they show approximately 1.1 times the
pathing CPU cost after task 153, not the order-of-magnitude wall-time difference.

**V:** `Window::create` infers active vsync from `SDL_GetRendererInfo.flags`.
Native SDL2's
[software renderer](https://github.com/libsdl-org/SDL/blob/release-2.32.10/src/render/software/SDL_render_sw.c)
advertises `SDL_RENDERER_PRESENTVSYNC` in its renderer flags. In
[sdl2-compat 2.32.74](https://github.com/libsdl-org/sdl2-compat/blob/release-2.32.74/src/sdl2_compat.c),
`SDL_GetRendererInfo` includes that flag only when the SDL3 renderer's vsync
property is enabled. The fixtures request software rendering without enabling
vsync. Consequently the default 240-FPS fallback limiter runs on that backend.
Scripted playback already advances one virtual simulation tick per frame;
sleeping between those frames adds wall time without advancing simulation.

**V:** A private Linux preload changed only the software renderer's reported
vsync flag to the compatibility layer's value. It also counted requested
`SDL_Delay` calls. No navigation, unit scanning, simulation, rendering or input
code was replaced. Single-course measurements use the same invented `narrow`
fixture, faithful profile, 852 observations and Release engine:

| Engine / reported flags | Wall seconds | Child CPU seconds | Requested sleeps |
| --- | --- | --- | --- |
| Before / native SDL2 | 0.175 | 0.127 | none counted |
| Before / compatibility flags | 3.435 | 0.206 | 852 calls, 3120 ms |
| After / native SDL2 | 0.175 | 0.125 | none counted |
| After / compatibility flags | 0.176 | 0.126 | 0 calls, 0 ms |

**V:** This is a 19.6-times wall-time ratio with a 1.6-times child CPU ratio,
dominated by explicit sleeps rather than pathfinding. These are single samples,
not a statistical performance benchmark or a native macOS profile.

**V:** Full suites against the same injected renderer flags also pass outside
CTest before the fix, but pathing exceeds CTest's existing 120-second limit:

| Suite / engine | Wall seconds | User + system seconds |
| --- | --- | --- |
| Framework before (20 tests) | 21.70 | 1.31 + 0.18 |
| Framework after (21 tests, including the new regression) | 1.56 | 1.15 + 0.14 |
| Stress before (6 tests, 48 course/profile/repeat processes) | 157.78 | 18.01 + 0.63 |
| Stress after (same 6 tests and 48 processes) | 10.33 | 8.27 + 0.23 |

Both framework runs skip the unavailable Xvfb case. Stress wall time decreases
by 15.3 times, while CPU time decreases by 2.2 times. The synthetic suites still
check every observation, arrival and repeated-run comparison. The injected
Linux framework run does not itself exceed 60 seconds; native backend/scheduler
costs remain necessary to explain that particular macOS boundary crossing.

**V:** Scripted playback now bypasses the frame-delay block and creates its
window without presentation vsync. Every frame, observation, UI trace,
screenshot request and simulation tick still executes. Interactive pacing and
CTest timeout limits are unchanged. Regression coverage repeats a twelve-tick
route with `--fps 1`, requires completion within five seconds, and compares all
observations with the ordinary run. The original engine fails that regression
with `subprocess.TimeoutExpired`; the updated engine passes.

**G:** Backend pacing accounts for the native timeout symptoms. The local
injection establishes the mechanism without reproducing Apple's scheduler,
SDL3 implementation or machine load. Only a native CI rerun can confirm the
complete fix on macOS.

## Validation

**V:** Before edits: bootstrap `--no-hooks`, Release configure/build, all 82
CTest entries and pre-commit passed. Six entries skipped for absent private
inputs or restricted networking. The four reported tests also passed in Debug
with ASan, UBSan, `_GLIBCXX_ASSERTIONS` and stack protection.

**V:** After code/test edits: the Release rebuild and all 82 CTest entries pass
again with the same six skips; pre-commit passes, including explicit checks of
the new report and helper. All seven targeted Debug sanitizer checks pass:
portability, campaign UI, campaign flow, custom missions, mouse grab, framework
and pathing stress. Framework/stress take 9.84/61.43 seconds in that unoptimized
instrumented build. Address/undefined-behavior and container-bounds checks report
no diagnostics. Leak detection is disabled because of the sandbox's known
LeakSanitizer process-inspection limitation; address/UB checking stays enabled.

No Windows/macOS binaries or native CI reruns were executed in this worktree.
The outstanding MinGW package-download rerun and native confirmation remain
explicit validation gaps, not synthetic passes. All changes remain uncommitted
because git metadata is read-only.
