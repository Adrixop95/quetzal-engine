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

## Shared L0 schema 2 implementation

Status: **needs live run; partial reference normalization**. Schema 1 synthetic
controls and their JSONL readers remain supported. `private_l0.json` now uses
schema 2. One JSON specification generates the same native schedule for the
hand-written runtime and `aztec_engine --headless`. With `--flat-output`, one
selected scenario writes directly into `work/parity/<id>/`:

```sh
python3 -I tools/parity_run.py private_l0 --flat-output \
  --output work/parity/private_l0 --game /absolute/private/installation \
  --runtime /absolute/rebuilt/aztec_recomp --engine "$PWD/build/aztec_engine"
```

Outputs are `scenario.json`, `{reference,clone}.json`, `diff.json`, `runners.json`
and `report.json`. Each runner has a private game copy, invocation/process log,
`schedule.txt`, native `observations.jsonl` and `tick_<n>.ppm` captures. Existing
output directories are refused. `--output` without `--flat-output` retains the
older multi-scenario run-directory layout. Nothing from these directories is
public evidence until reviewed; captures always remain ignored.

Schema 2 currently accepts campaign missions 1..15, empty `setup` and `input`,
provisional evidence P and exact tolerance. Unsupported setup/commands are
rejected rather than silently ignored. `observe` contains
`{"contract":"shared-l0-v2","ticks":[1,6,12]}`. Ticks must start at 1 and be
strictly increasing positive 32-bit integers. The fixed `view` contains:

```json
{
  "camera": [0, 0, 1],
  "resolution": [800, 600],
  "side": 0,
  "selection": [],
  "panel": true,
  "cursor": [400, 300]
}
```

Camera coordinates are top-left world pixels; zoom and UI scale are 1. Other
supported viewing sides are 1..3. The native transport starts with
`L0 2 <camera-x> <camera-y> <side>` followed by one tick per line; both C and C++
read the same checked parser in `src/parity/schedule.h`.

Both observers use the `before_commands` boundary: tick n describes the state
before commands and simulation work for tick n. Clone rendering uses alpha 1
and the current simulation day clock, with an explicitly positioned camera,
empty selection, panel and cursor. Its headless loop supplies state and pixels
synchronously, without a second simulation process or wall-clock screenshot
schedule. Automatic human orders and resume are disabled for this contract.

Each canonical bundle records schema, contract, mission, phase, position unit,
requested view, ticks, runner provenance and samples. Each sample has `tick`,
`objects`, `money`, `queues`, observed `view`, relative `frame` and `unavailable`.
An optional reference-only `diagnostics` object retains pool accounting, raw
order/tactic and town-queue probes, camera/viewport globals and capture phase.
These probes are excluded from cross-program field comparison. Nonzero pool
exclusions produce `free_slot_filtered` or `inactive_object_filtered` notices;
they cannot become object-presence divergences. Invalid pool accounting fails
validation. Notices do not override a blocked contract or a real difference.
Objects contain `type`, `owner`, `x`, `y`, `hp`, `orders`; positions use
`pixel/256`. Orders contain `kind`, canonical `target` or null, and destination
`x`,`y` in pixels. Queues are ordered lists per side, with type, town, remaining,
total, paid, started and paused fields. Money is an integer map by side.

Native addresses/engine IDs are private tracking handles. Initial identities
use the exact `(owner,type,x,y)` anchor at tick 1, then retain that identity across
movement and owner changes. Duplicate anchors, unknown spawns, reused handles
and unresolved targets are refused. This is deliberately insufficient for
production/spawn scenarios: those require an audited creation-generation and
mission-placement identity observer. No nearest-position matching is performed.

The reference observer runs on the logic thread at the existing oracle command
fetch. It blocks that thread, freezes `GetTickCount`/`timeGetTime`, requests the
camera position, and waits up to five seconds for a renderer callback. Paused
wall time is subtracted when the clock resumes. It writes normalized integer
HP, type, class-specific fixed-point position, owner through the owning tactic,
and four reference cash slots. The existing legacy unit log is unchanged.
The reference currently declares **logical orders, normalized production queues,
observed UI state and complete-frame phase unavailable**. Fractional positions
are read for Marine, Spider, Zeppelin, Truck, Bird and Headtruck; Building and
Defence positions are integral. Other classes keep `position_fraction`
unavailable. The allocator's active/free chains and counts are checked before
world objects with a cleared `+0x5c` liveness flag are excluded. Retained vtables,
owner -1, zero HP and off-map positions do not establish allocation or liveness.
A renderer callback can be a
partial DirectDraw blit; its capture is diagnostic. A timeout declares the frame
unavailable too. A requested camera is not certified as a rendered camera.
This code therefore does not yet unblock L1/L4/L5 acceptance. The missing audited
normalization and complete-render boundary still need implementation and live
verification; removing `unavailable` labels without that work is invalid.

The comparator validates entire bundles and frames before finding the first
known object/field, money/queue or exact-pixel difference. Missing capability
labels produce `blocked_observation_contract`, never equality, including when
both programs lack a field. `first_known_difference` retains a diagnostic
first difference in supported fields even while the overall result is blocked.
Exact pixel comparison is a harness control; it is
not the L4 tolerant/structural acceptance metric. Misaligned contracts, truncated
samples/images, duplicate JSON keys and escaped frame paths are errors. Run a
comparison independently with:

```sh
python3 -I tools/parity_l0.py work/parity/private_l0/reference.json \
  work/parity/private_l0/clone.json --output work/parity/private_l0/recheck.json
```

`shared_l0` CTest uses invented guest memory, an invented framebuffer and an
invented installation. It checks sparse native ticks, ownership indirection,
state/frame repeatability, full runner output, explicit missing capabilities,
validation failures and an HP negative control at tick 3. Its synthetic
subprocess shim replaces namespace launch only for these invented inputs.
No reference game process is executed by this test.
