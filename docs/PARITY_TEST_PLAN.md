# Parity test plan: original game versus the clone

Status: plan (written 2026-10-28). Implementation is delegated to the tasks
`tasks/queue/42-*` to `46-*`. This plan defines **what is compared, how, and what counts
as a pass**. It never stores game data: all captured material lives in the ignored
`work/parity/` directory, and only code, scenario specifications and aggregate
result tables are committed.

## Principles

1. **Two runners, one scenario.** A scenario is a small text/JSON specification
   (map, setup, scripted input, observation points). The *reference runner* drives
   the original game through the recompiled runtime (`RT_SCRIPT`, `RT_CMD_REPLAY`,
   `RT_STATE_LOG`; see `docs/EXPERIMENTS.md`). The *clone runner* drives
   `aztec_engine` headless with the same specification. Both emit the same
   observation schema.
2. **Measure, do not guess.** A rule only moves from P (placeholder) to M/V when a
   reference measurement exists. Each scenario records the evidence label.
3. **Exact where the original is deterministic, tolerant where it is not.**
   Simulation state is compared exactly (integers, per tick). Rendering is compared
   with documented tolerances and structural checks. AI behaviour is compared by
   windows and distributions, not by tick.
4. **First divergence, not pass/fail.** Every comparison reports the first tick,
   object and field that differ, grouped into the categories of `docs/parity.md`.
5. **Triage closes the loop.** A failing scenario becomes a task prompt in
   `tasks/queue/` (generated, reviewed, then run by Codex), so findings turn into
   fixes without manual transcription.
6. **Public repo safety.** No screenshots, audio, extracted values tied to the
   executable, or generated code are committed. Result tables hold only counts,
   categories and scenario names.

## Layers

| Layer | What is compared | Method | Pass criterion |
| --- | --- | --- | --- |
| L0 Harness | runner parity | one scenario schema, two runners, one observation schema | the same scenario runs on both and produces comparable output |
| L1 Rule micro-experiments | speed, damage, armor, reload, range, projectile flight, build time, price, income, sight, regeneration, repair, population | isolated test maps, one mechanic per scenario, measured on the original, stored as scenario expectations | engine value equals the measurement (exact for integers, stated epsilon otherwise) |
| L2 Command replay | whole-mission simulation | replay recorded command logs on both, compare per-tick state hashes then field observers (positions, hp, money, queues) | no divergence for N ticks; otherwise the first-divergence report |
| L3 Movement and pathing | routes, arrival tick, stalls, group behaviour | `zpath` style suites: obstacles, chokepoints, crowds, unreachable goals | arrival tick within tolerance, no stalls the original does not have, path length ratio bounded |
| L4 Visual | scene frames at fixed tick and camera | capture both at identical state; compare per region (map, units, HUD, minimap, command card, overlays) with structural masks plus tolerant pixel metrics | per-region metrics under thresholds; masks equal for UI layout |
| L5 UI interaction | hover, press, release, hotkeys, tooltips, cursor shapes, menus | scripted input events at pixel coordinates; record the UI state sequence (button frames, highlights) and the command emitted | same state sequence and same emitted command |
| L6 Audio | which sound fires when | event log (sound id, tick, trigger) from both | same events in the same order within one tick |
| L7 Playthrough | mission completion | human-like routes per mission with expected outcome (V19 matrix) | each mission reaches its outcome in the clone; divergences logged |
| L8 AI behaviour | build orders, attack timing, defence reactions | scripted passive-player scenarios; compare timing windows and composition over repeated runs | measured windows overlap; no missing behaviour class |

## Scenario specification (L0)

A scenario file contains: `id`, `layer`, `map`, `setup` (sides, money, preplaced
objects, tech), `input` (list of timed events: commands or raw mouse/keyboard events),
`observe` (ticks and fields, or screen regions), `tolerance`, and `evidence`
(V/M/P). Scenarios live in `tests/parity/` (specification only). Runners write
`work/parity/<id>/{reference,clone}.json`; the comparator writes
`work/parity/<id>/diff.json`.

## Execution

* Synthetic scenarios (no game data) run in CI as part of CTest.
* Private scenarios run locally through `tools/parity_run.py --all` (reference runner
  in an isolated network namespace as in `docs/EXPERIMENTS.md`), nightly via the
  supervisor when enabled, and always before declaring a roadmap row done.
* The aggregate matrix is committed as `docs/parity_matrix.md`
  (scenario count, pass/fail/skip per layer, first-divergence categories).

## Order of work

1. L0 harness and scenario schema, plus comparator and report (task 42).
2. L1 rule micro-experiments, the highest-value layer because every rule fixed there
   removes divergences from L2, L3 and L8 (task 43).
3. L4 and L5 visual and interaction capture (task 44) and L6 audio events (task 45).
4. Triage tooling and the matrix (task 46), then L2, L3, L7 and L8 expand on top of it
   using the existing tasks 11, 19, 22, 23 and 35.

## Out of scope

Bit-exact reproduction of the original rendering, exact AI trajectories, and original
network wire compatibility.
