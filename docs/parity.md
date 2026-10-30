# Whole-mission parity reports

`tools/parity_report.py` orchestrates private corpus replays and compares normalized
observations. Whole-mission cross-engine parity is **blocked** in this checkout.
The audited packet translation/identity adapters from task `01-oracle-parity` are missing.
The script does not translate reference pool hashes into engine hashes or infer
object fields from a hash mismatch.

## L0 scenario harness

`tools/parity_run.py` selects JSON specifications from `tests/parity/`. The
version 1 schema is [parity_scenario.schema.json](parity_scenario.schema.json);
the dependency-free loader also checks field coverage, object references and
input tick ordering. Duplicate keys, unknown keys, booleans used as integers,
invalid horizons and unsupported tolerances are rejected.

```sh
python3 -I tools/parity_run.py --list
python3 -I tools/parity_run.py synthetic_l0
python3 -I tools/parity_run.py --all --layer L0
ctest --test-dir build -R 'parity_(scenarios|controls)' --output-on-failure
```

IDs select individual scenarios; `--all` selects every specification and `--layer`
filters L0 through L8. A layer can also select scenarios without `--all`. Listing
validates specifications without creating output or launching a process. An
unknown ID, empty selection or invalid specification is a usage failure.

| Key | Version 1 contract |
| --- | --- |
| `schema`, `id`, `layer` | Version 1; unique lowercase letters/digits/underscore/hyphen ID; L0 through L8 |
| `map` | `kind` is `synthetic` or `campaign`; `mission` is an integer from 1 through 15 |
| `setup` | Synthetic: `objects`, keyed by invented IDs, each with exactly the declared integer fields. Campaign: empty for executable L0 smoke tests; `sides`, `money`, `objects`, `tech` reserve future setup specifications |
| `input` | Synthetic: ordered `{tick, object, set}` updates; `set` contains declared integer fields. Campaign: empty until an audited input translator exists |
| `observe` | `first_tick` is 1, `last_tick` is a positive signed 32-bit integer; `fields` is a nonempty unique string list; `contract` names the normalization specification |
| `tolerance` | Exactly `{"exact":true}`; no tick offset or numeric epsilon |
| `evidence` | V/M/P specification label; running a scenario never promotes it |

`synthetic_l0` uses two mocked runners and the `synthetic-l0-v1` contract. Setup
defines invented objects and arbitrary integer units; IDs never change, and an
input at tick t applies after observing t, appearing in observation t+1. Repeated
events at the same tick preserve specification order. This checks harness
construction only; it models no original rule. Synthetic fixtures require L0
and evidence P. Scenario files contain specifications only, with no captured
state, assets, screenshots, audio or executable-derived expectations.

Every invocation creates a fresh directory under ignored `work/parity/` (or an
explicit `--output` there). Each scenario retains its specification, normalized
`reference.jsonl` and `clone.jsonl` when available, runner diagnostics and
`diff.json`. `report.json` collects scenario ID, layer and verdict. Existing run
directories and input/output overlap are rejected. The comparator reuses the
strict JSONL observation schema below: it checks the full horizon before
accepting the first difference in tick/object/field order. Categories include
`field_difference`, `object_lifecycle`, `equal_observed_fields`,
`blocked_observation_contract`, `experiment_failed` and
`skipped_missing_private_inputs`. No tick/object/field is invented for a run
that failed before producing comparable observations. Exit codes are 0 for
matching observed fields with optional skips, 77 when every scenario skipped,
1 for a difference/blocker/failed experiment, and 2 for invalid top-level input.

### Private L0 smoke test and missing prerequisites

```sh
python3 -I tools/parity_run.py private_l0 \
  --game /absolute/private/game --runtime /absolute/private/build/aztec_recomp \
  --engine "$PWD/build/aztec_engine" --output work/parity/l0-private-new \
  --timeout 60
```

Absent installation/resources/maps, runtime or engine produce a skip. With
inputs present, both runners receive separate private game copies, ordinary
Maps/Saves directories, copied profiles/configuration and immutable resource
symlinks. Both use dummy SDL drivers inside `unshare -rn` with
`timeout -s KILL`. Reference loopback/address setup fails closed; the independent
engine and normalization adapter need no interface configuration. Inherited
`RT_*` variables are removed. The reference runner sets `RT_SCRIPT`,
`RT_CMD_REPLAY`, `RT_CMD_LOG`, `RT_STATE_LOG`, `RT_UNIT_LOG` and a private dump
directory. Default menu navigation covers missions 1 through 15; `--script`
accepts private navigation text and `--commands` accepts a private oracle log.
Menu waits are watchdogs, not observation synchronization. State coverage and
packet readback are checked explicitly.

The clone runs `aztec_engine --headless --no-auto-orders --observations JSONL`.
`engine-index-v1` observes live, nonremoved objects before command submission
and `begin_tick`: tick 1 is initialized state, t is after t-1 updates. IDs are
`engine:<Object.id>` with append-only engine identity; missing IDs indicate
death/removal and newly live objects appear at the next boundary. Integer `x`
and `y` are engine world pixels times 256, `hp` is the current engine HP value,
`side` is the engine side index and `type` is the engine UGI ID (including -1
markers). Campaign CLI numbers 1..15 enter the header; custom map names use
mission 0. The horizon remains the requested limit: an earlier terminal outcome
leaves a truncated stream and cannot count as full coverage. Output failures,
script errors, unsupported executions and incomplete horizons fail the runner.

This engine-local contract does **not** establish original identity, units,
ownership or tick equivalence. `--reference-adapter` uses the reference adapter
interface below and must audit the mapping and produce the exact scenario
header/coverage. No such adapter is supplied by this task. Without it, a
successful raw reference run reports `blocked_observation_contract`; pool
hashes are never relabeled as semantic fields. Nonempty campaign setup/input,
nonempty replay packets, other layers and other private normalization contracts
also block explicitly. Task 01's packet translator and audited observer remain
dependencies; the completed L0 selection, runners, observer and synthetic
comparison do not depend on their presence. For standalone comparison, use the
existing `parity_report.py compare` command below.

On 2026-10-29, the supplied read-only installation and runtime were used with
private copies in `work/parity/l0-private-20261029/`. The clone completed 12 ticks
with valid observations. The reference attempt failed at loopback setup with a
denied netlink socket before runtime launch. The combined result is
`experiment_failed`, with no semantic divergence or new gameplay M/V evidence.
CTest's synthetic controls cover schema rejection, selection/skip behavior,
mocked runners, exact first differences, lifecycle, truncation, contract
mismatch, isolation failure propagation and real headless before-command
observations using an invented installation.

## Run the corpus

First build the independent engine and run the checks in [SETUP.md](SETUP.md).
Use your own recorded corpus and explicit private paths:

```sh
python3 tools/parity_report.py self-test
python3 tools/parity_report.py run \
  --corpus /absolute/private/work/corpus \
  --game /absolute/private/game \
  --runtime /absolute/private/build/aztec_recomp \
  --output work/parity/run-001 --timeout 600
```

Each `m<N>/cmd.txt` must contain the `RT_CMD_LOG` format: decimal frame and side,
then exactly 53 hexadecimal dwords. Packets must be in nondecreasing frame order,
with at most one packet per frame/side, matching the reference replayer's lookup.
Empty logs are valid for no-input recordings. The default horizon is the last
tick of that mission's recorded `state.txt`; `--ticks N` overrides it and must
include every command. An explicit horizon describes a recorded interval, not
proof of mission completion. No campaign missions without recordings are tested.

For each recording, the runner:

1. Copies commands into a fresh output directory and records input digests.
2. Creates separate writable game copies for two reference runs. Only `Aztec.exe`,
   `Res`, `Video`, `Music` and `Campaign` are symlinked. `Maps`, `Saves`, INI files
   and player profiles are copied, following [EXPERIMENTS.md](EXPERIMENTS.md).
3. Navigates the campaign menu at 800x600 and replays with `RT_CMD_REPLAY`, while
   collecting `RT_CMD_LOG`, `RT_STATE_LOG` and `RT_UNIT_LOG` every tick. Campaign
   navigation supports missions 1 through 15; menu/profile selection still needs
   live verification on each installation.
4. Requires both state streams to cover ticks 1 through the horizon and verifies
   exact injected-packet readback. It compares four pool count/hash pairs,
   retaining every difference privately. Without a reference adapter, a raw
   difference reports `reference_nondeterminism` with tick/pool only; matching
   pools report `blocked_observation_contract`. Recorded state hashes determine
   the horizon only, since host state can vary between processes.
5. With a reference adapter, normalizes both repeats and requires equal declared
   fields over the entire horizon. With an engine adapter too, invokes engine
   replay in another private game copy and compares fields. Raw pool differences
   remain unaudited even when the declared semantic subset repeats.

Every process runs with both SDL dummy drivers inside `unshare -rn`, behind
`timeout -s KILL`. A shell with `-e` configures loopback and the private address;
setup failure stops before the runtime. Runtime nonzero exits and incomplete
state logs fail the mission. There is no fallback outside isolation. Waiting in
an input script is a watchdog, not tick synchronization. The runner removes
inherited `RT_*` variables to prevent accidental replay/record overrides.

Use a new output directory for every invocation. All output is restricted to
this worktree's ignored `work/parity/`; existing runs are never overwritten.
`report.json` contains one row per recording: mission, category, tick, object ID,
field and, when available, reference/engine values. Raw commands, state, unit
logs, process output, invocation arguments and game copies remain private.
Exit codes: 0 means all observed fields matched, 1 means a difference, blocked
mission or failed experiment, and 2 means a top-level input/usage failure.

## Missing adapter interfaces

These are extension contracts, **not supplied implementations**. Do not pass the
current `aztec_engine` as `--engine-adapter`: its CLI cannot read oracle packets
or write these observations. The engine's binary `serialize_log` format is also
different from the oracle text packets. Packet IDs alone do not establish packet
payload or object-ID compatibility.

An executable `--engine-adapter PATH` receives:

```text
--game PRIVATE_GAME --mission N --ticks N --commands ORACLE_LOG --output JSONL
```

It must replay every packet at its original tick/side and preserve command order,
explicitly rejecting unsupported packets. It must implement an audited mapping
from oracle object references to engine IDs and emit state before commands for
ticks 1 through N. Skipping unsupported commands invalidates the comparison.

An executable `--reference-adapter PATH` receives:

```text
--state-log STATE --unit-log UNITS --mission N --ticks N --commands ORACLE_LOG --output JSONL
```

It must normalize independently verified observations, failing if those logs lack
required fields or identity evidence. Additional reference instrumentation may be
necessary. `RT_UNIT_LOG` raw addresses are process-local; its historical side
column can be -1 and its raw max-HP slot is not a virtual maximum-HP getter.
Treating those fields as canonical IDs/ownership/maxima would produce false
findings. Matching objects merely by sorted type/position is ambiguous for
coincident units, spawns and deaths. These problems belong to prerequisite task
01; this task does not guess a mapping or add executable-derived code.

Both adapters must emit identical headers followed by one JSON object per tick.
This invented example demonstrates the format only:

```json
{"schema":1,"mission":1,"phase":"before_commands","contract":"synthetic-v1","fields":["hp","x","y"],"first_tick":1,"last_tick":1}
{"tick":1,"objects":{"spawn:0":{"hp":100,"x":256,"y":512}}}
```

The contract identifier must name a documented normalization specification,
including cross-engine identity mapping, fixed-point coordinate units, HP units,
spawn/death conventions and tick origin. `spawn:0` is an illustrative canonical
ID, not an original object ID. Observations contain integer fields only; pointers,
render state, padding and other unaudited fields are excluded. The fields list
states coverage explicitly; matching that subset never proves full game parity.
The reference hook observes state at the first command fetch of the frame,
**before** its logic update. Engine post-step hashes need explicit alignment;
no heuristic tick offset or coordinate tolerance is applied here.

The comparator checks headers, consecutive ticks, exact field coverage and the
entire declared horizon. Truncated or malformed logs fail even when an earlier
value differs. First divergence is ordered by tick, then canonical object ID
lexically, then field name lexically. Object appearance/disappearance reports
`presence`. This is a stable diagnostic order, not a causal rule attribution.

For independently captured normalized streams:

```sh
python3 tools/parity_report.py compare \
  work/parity/run-001/reference.jsonl work/parity/run-001/engine.jsonl \
  --output work/parity/compare-001.json
```

The built-in synthetic controls exercise equality, differing HP, lifecycle,
truncation, missing ticks, incompatible contracts, packet/state validation and
isolation failure propagation. They verify tooling behavior, not oracle parity.

## First-divergence categories and current evidence

2026-10-27: the supplied private corpus contained nine recorded missions. All
nine isolated attempts failed during loopback setup with a denied netlink socket.
The reference runtime was never launched; no engine replay could be measured.
Private details are in `work/parity/mission-audit/report.json`. The public table
contains coverage and failure categories only, with no game-derived state.

| Category | Missions in this attempt | Tick / object / field | Meaning |
| --- | --- | --- | --- |
| `experiment_failed` | 9 of 9 | unavailable | Namespace network setup denied; no simulation observation |
| `blocked_observation_contract` | prerequisite absent; not reached | unavailable | Engine packet replay and normalized reference observer missing |
| `reference_nondeterminism` | not measured | unavailable | Reference pool comparison could not run |
| `object_lifecycle` | not measured | unavailable | Canonical object presence differs |
| `field_difference` | not measured | unavailable | Exact normalized field differs |
| `equal_observed_fields` | not measured | unavailable | Declared fields agree through the requested horizon |

No movement, combat, economy or script divergence is established by this attempt.
Priorities in [engine_rules_status.md](engine_rules_status.md) distinguish the
observed tooling blockers from existing rule gaps awaiting mission evidence.

## V02 replay audit (2026-10-28)

The reference wrapper now rejects missing/unreadable replay files, signed or
out-of-range integers, short/extra packets, decreasing frames and duplicate
frame/side keys. It fails before serving any packet, instead of silently using
live input or zero-filled malformed records. The independent C parser is tested
by `oracle_packets` without lifted code. This is a wrapper construction property;
it does not verify the guest tick's ordering or packet payload semantics.
`RT_CMD_LOG` also records injected non-NOTHING packets. Rebuild a private runtime
before using packet readback validation: the old runtime returned before logging
injected commands. The runner compares every recorded frame/side/dword against
the input through the requested horizon. Empty input remains a valid no-input
replay. Frame/side lookup permits different text ordering of sides in one frame;
it does not establish the reference's actual side-fetch order.

Raw pool differences are retained in `pool-differences.json`. When a reference
adapter is supplied, it must normalize **both** runs, with the same mission,
phase, contract and complete horizon. A first semantic difference is reported as
`reference_semantic_nondeterminism`. Repeating declared fields alongside differing
raw pools leaves excluded bytes unaudited; it does not diagnose those bytes as
harmless or establish full reference repeatability.

`GameSim::replay` accepts a `ReplayObserver` callback before command submission
and `World::begin_tick`: observation tick 1 is the initialized world, and tick t
is the state after t-1 updates. `determinism_state` verifies this boundary. The
engine's append-only object indices are stable within a replay; they are **not**
an audited cross-engine identity mapping. The observer hook therefore does not
supply the missing packet translator or normalized reference adapter.

A new nine-mission corpus attempt used the explicit read-only installation and
runtime paths with private game copies, both SDL dummy drivers, `unshare -rn`
and 45-second hard timeouts. All nine attempts failed at loopback setup with
`Operation not permitted`; the reference executable never launched. The private
report is `work/parity/v02-isolated/report.json`. No semantic mismatch, transient
pool cause or lifted price-rounding diagnosis was measured. The earlier
`v02-baseline` attempt rejected its too-short explicit horizon before launch and
is not reference-run evidence. V02 remains open, including original-instruction
versus lifted-arithmetic validation and audited ownership/max-HP observers.

## L4/L5 visual and interaction controls (2026-10-29, partial task 44)

`tools/parity_visual.py` implements the independent part of L4/L5 while task 42's
shared scenario runner is absent from this checkout. It compares explicitly
normalized capture bundles and provides an isolated private capture **probe**.
The probe does not produce an aligned bundle. Matching reference ticks/camera,
reference UI state instrumentation, structural-mask extraction and audited
command/object normalization remain prerequisites. A wall-clock `RT_SCRIPT wait`
or `shot` must never be assigned a guessed simulation tick.

Run the asset-independent controls with:

```sh
ctest --test-dir build -R parity_visual_ui --output-on-failure
```

All rasters, temporary synthetic installations, traces and diagnostic bundles
created by these controls stay under ignored `work/parity/`. The ten Python test
methods cover exact equality, tolerant RGB differences, local pixel/MAE failure,
shifted structural masks, UI/command sequence differences, incomplete horizons,
alignment/contract rejection, invalid rasters/thresholds, absent private inputs,
isolation failure propagation and actual engine UI inputs. The last test runs
the same invented installation twice and checks identical traces. This verifies
tooling and independent engine behavior only.

### Private comparison contract

The public `tests/parity/visual-ui-synthetic.json` is an invented schema example
with five observation points. Its input labels describe comparator controls;
`tests/parity/ui-input.txt` is the separate executable engine playtest control.
Private scenarios and manifests containing original state belong in
`work/parity/`, including numeric alignment values. Compare them with:

```sh
python3 tools/parity_visual.py compare work/parity/CASE/scenario.json \
  work/parity/CASE/reference.json work/parity/CASE/clone.json \
  --output work/parity/CASE/diff.json
```

Use a fresh output file. Exit 0 means the declared samples agree within their
thresholds, 1 means a difference, and 2 means invalid/incomplete observations.
The output path is restricted to this worktree's `work/parity/`.

A scenario declares `schema: 1`, `id`, `evidence` (P/M/V), `alignment`, `regions`
and an ordered `observe` array of `{step, tick}`. The evidence label is supplied
by the investigator, never promoted by the comparator. Alignment must include
all these fields: `map`, `setup`, `phase`, `camera`, `selection`, `money`, `side`,
`ui_scale`, `resolution`, `light`, `animation_ms`, `bindings`, `tooltip_ms`, and
`messages`. Camera is the top-left world pixel position and zoom; resolution is
`[width, height]`. Setup identifies the same starting simulation state. Static
alignment describes the common viewing conditions for the whole scenario;
use separate scenarios when camera/selection/light conditions change. Observation
ticks must increase strictly; repeated UI observations in a paused tick need
separate scenarios with distinct step labels. The contract must define tick
origin, interpolation and input/render phase, not merely matching tick numbers.

Each bundle declares `schema: 1`, `scenario`, a nonempty normalization `contract`,
identical `alignment`, and `samples` in the exact scenario order. Each sample
contains `step`, `tick`, `image`, `masks`, `state` and `commands`. Image/mask paths
are relative to the bundle directory and cannot escape it, including by symlink.
Both complete bundles are validated before any first-divergence verdict. Missing
samples/fields, wrong ticks/resolution, incompatible contracts and truncated
rasters fail. Header equality checks the supplied alignment assertions; it does
not independently establish their truth.

Images support the reference's binary P6 PPM and the clone's opaque RGB/RGBA8,
non-interlaced PNG. Structural masks are full-resolution binary P5 PGM files
(maximum 255, pixels 0 or 255), independently obtained for each capture. All six
named regions are required: `map`, `units`, `hud`, `minimap`, `command_card`,
`overlays`. A mask must contain at least one included pixel; choose a scene with
coverage in every region. Region overlap is allowed. Mask provenance and the
meaning of included structures belong to the normalization contract: annotate
layout/occupancy independently on each side, rather than copying one mask to
both captures. Do not automatically erase differences, fit/shift images, or infer
alignment from pixels. No game-derived masks are public fixtures.

Each region specifies `channel` (integer RGB channel tolerance),
`max_bad_fraction` and `max_mae`. A pixel is bad when its largest absolute RGB
channel difference exceeds `channel`. MAE is the sum of absolute RGB differences
divided by three times the included pixel count, on a 0..255 scale. Pixels are
measured over the union of the two masks. Structural mask differences always
fail, including when RGB values agree. Reports retain mask difference counts,
exact mismatch fraction, tolerant bad-pixel fraction and MAE separately.
Thresholds in the synthetic example are invented controls, not measured game
acceptance limits. No resampling, alpha blending or perceptual color transform
is applied.

Normalized UI `state` has exactly `buttons`, `cursor`, `tooltip`, `menu` and
`target_mode`. Buttons map canonical control names to `{frame, highlight,
enabled}`; frame is a nonnegative integer and the other fields are booleans.
Cursor/menu/target mode are nonempty canonical strings; tooltip is a string.
`commands` is an ordered list of `{name, arguments}`, with semantic command names
and an arguments object. The normalization contract must specify every argument,
canonical object identity and coordinate units, reject unsupported commands and
include commands emitted since the previous observation. Observe every input
transition so intermediate states/commands cannot disappear between samples.
The comparator checks the entire state and ordered command list exactly; it
reports changed fields for every sample. First divergence is ordered by scenario
tick, then the six region names in the order above, then UI field name (commands
last). This diagnostic order is not a causal attribution.

### Engine trace and capture probe

`aztec_engine --script INPUT --ui-trace work/parity/CASE/ui.jsonl` writes a raw
`engine-ui-raw-v1` row per rendered frame, after UI input and before presentation.
Rows include frame/tick, actual camera/zoom, interpolation alpha, selection,
mouse/held left button, targeting/placement, panel/menu state, tooltip text,
cursor shape/visibility/animation time, six command button states and commands
newly submitted by `CommandsUi::update`. Raw command payloads and object indices
use engine conventions; they are not oracle packets or canonical cross-engine
identities. The engine currently draws command button sprite frame zero even
while pressed; the trace records that behavior. Production/queue controls are
outside this raw button-state coverage. Commands from setup, scripts or other
sources are outside the UI submission window. A normalized adapter must refuse
comparisons needing those uncovered observations.

Scripted cursor animation uses virtual frame time at 12 Hz. Trace rows include
the requested scripted screenshot path; scripts should write those screenshots
under the same private case directory. Rendering errors and missing captures must
be treated as failed experiments. The trace is a diagnostic source, not directly
a normalized comparison bundle. Its presence does not supply the missing
reference observer or shared task 42 runner.

To reproduce the private availability/capture probe:

```sh
python3 tools/parity_visual.py probe \
  --game /home/adrox/aztec-re/game \
  --runtime /home/adrox/aztec-re/build/aztec_recomp \
  --clone build/aztec_engine --output work/parity/l4-l5-probe-next --timeout 30
```

The output directory must be new. Missing private installation/runtime or engine
returns 77 with a skip report. Both runners use private game copies, dummy SDL,
`unshare -rn` and a hard timeout. Reference loopback setup fails closed. Engine
capture needs no namespace interface setup. Even successful probes return 1 with
`blocked_alignment_contract`: their frames are unpaired and cannot pass parity.

The final local attempt in `work/parity/l4-l5-final-probe/` produced a clone PNG and four raw
UI rows. Reference setup failed with a denied netlink socket before launching
the game. No reference frame, UI observation or emitted command was measured.
No visual/UI rule is promoted to M/V. The aggregate evidence table is
[parity_visual_ui_results.md](parity_visual_ui_results.md).
## L1 rule micro-experiments (2026-10-29, partial task 43)

Task 42's scenario harness is absent (`tools/parity_run.py` is not present).
The 19 P specifications in `tests/parity/l1_*.json` use the top-level schema in
[PARITY_TEST_PLAN.md](PARITY_TEST_PLAN.md), with provisional nested keys described
in [the scenario guide](../tests/parity/README.md). Referenced task prompts
36/37/40 are absent too. Independent work supplies candidate maps, isolation
probes and private numeric expectation export; command/observer adapters and
validated isolated setup remain dependencies. No clone simulation rule changed.

The selection prioritizes Cannon, Catapult and Hopcannon movement, six damage
category controls, siege reload/range/flight, mine construction/price/income,
sight/regeneration and serial versus two-town production. Each specification
lists required controls and repeats. Armor classes must be observed, not inferred
from target names. Boundary sweeps, nonzero resource patches, production speed
branches, projectile obstacles and faction variants remain unexecuted protocols.

The explicit external installation/runtime were used read-only:

```sh
python3 -I tools/parity_rules.py probe \
  --game /home/adrox/aztec-re/game \
  --runtime /home/adrox/aztec-re/build/aztec_recomp \
  --output work/parity/l1-isolated-20261029 --timeout 45
```

All 19 candidates generated in separate private game copies. Each bounded
launch used both dummy SDL drivers and `unshare -rn`; every attempt failed at
loopback setup with `Cannot open netlink socket: Operation not permitted`.
The runtime never launched. Private scenario copies, maps, invocation records,
logs and aggregate report remain in the ignored output directory. No numeric
expectations were measured or exported, and no semantic first divergence exists.
The report uses `experiment_failed` with null tick/object/field, not a fabricated
rule mismatch. Generated maps remain candidates: commands, AI, damaged HP, tech,
money, fog and town membership have not been controlled or validated.

| L1 result in this attempt | Count |
| --- | --- |
| P base specifications / generated candidate maps | 19 |
| Isolated launch failures before simulation | 19 |
| Measured rules / clone parity passes | 0 |
| First semantic divergences | unavailable |

`tools/parity_rules.py compare` consumes complete normalized streams from two
reference repeats and the clone through the existing strict comparator. It
checks the scenario contract, horizon, fields and initial identities before
exporting integer-only expectations privately. Reference repeat disagreement
creates no expectations; malformed or truncated observations fail. The comparator
retains the first tick/object/field difference when one is actually observed.
Its synthetic controls establish tooling behavior only. Matching streams and
exporting numbers do not audit commands, setup, units or identity mapping and
never automatically promote a P scenario to M. See the scenario guide for the
private format, exit codes and remaining integration contract.
## L6 audio event comparison (2026-10-29)

`tools/audio_parity.py` supplies the audio-specific comparison independently of
task 42's shared scenario harness, which is absent from this checkout. It compares
the **global request sequence**, including interleaved trigger classes. Sound ID,
kind and trigger must match at each sequence index; ticks may differ by at most
one. It reports the first divergent index and every difference, with counts and
coverage for `selection`, `orders`, `combat`, `building`, `ui`, `music` and
`ambient`. Unobserved classes remain `unobserved`; two empty streams return
`no_events`, never a pass. Index comparison is deliberately conservative: one
insertion can produce subsequent `identity_or_order` differences. Those counts
are divergent sequence positions, not independent root causes.

The clone's `--audio-log FILE` now emits JSONL with `seq`, `tick`, `kind`,
`source`, `set`, `sound` and `trigger`. `sound` is the chosen WAV ID, or the track
number for music. It logs accepted requests without requiring an audio device;
`--no-audio` still disables requests. Music changes retain the caller's current
world tick instead of resetting to zero. Structure animation requests use
`building`; mobile attack/death and independently firing turret requests use `combat`; other mobile
animation requests use `ambient`. These are engine dispatch labels, not measured
original causes. Generic effects remain `unknown`. Loops retain their start
trigger at stop, including unknown provenance. Selection/order/UI contexts can
be supplied through `AudioSystem::event_trigger`, but the missing application
voice/UI dispatchers are not invented by this instrumentation.

The reference's `RT_AUDIO_LOG=FILE` hooks secondary DirectSound buffer Play,
Stop and release while playing. It emits JSONL with `seq`, `tick`, `buffer`,
`kind` (`play`, `stop`, `release`), `flags`, `sound:null` and `trigger:"unknown"`.
The tick reads the existing command oracle clock at the request; this does not
establish its phase relative to the clone. Writes are serialized, flushed and
fail on an I/O error. Primary buffers and automatic callback exhaustion are
excluded. No samples are recorded. Buffer handles are process-local identities,
not sound IDs: the shim cannot infer a sound resource, causal trigger or music
track from a handle. In particular streamed music may reuse one playing buffer
across track changes. A rebuilt private runtime is required to enable the hook;
the supplied prebuilt binary is never modified.

### Private capture and normalization

Use a fresh output directory for each attempt:

```sh
python3 -I tools/audio_parity.py probe \
  --game /absolute/private/game --runtime /absolute/private/build/aztec_recomp \
  --script /absolute/private/audio-input.txt \
  --output work/parity/audio-attempt --timeout 60
```

Without `--script`, the probe only waits at startup and quits; it is a hook/setup
smoke probe, not a six-trigger scenario. It reuses the mission runner's private
installation copy and fail-closed `timeout ... unshare -rn` setup, with both SDL
dummy drivers. Missing game/runtime inputs return 77 and a skip report. Failed
isolation or runtime exit returns 1; there is no fallback outside isolation.
Successful raw capture also returns 1 (`captured_unmapped`), because it alone is
not a parity verdict. An absent trace reports
`blocked_missing_audio_hook_or_no_requests`. Every probe report marks each class
unmeasured. The script, game copy, invocation, process log and event trace remain
under this worktree's ignored `work/parity/`.

An audited private manifest must bind each raw file by `raw_sha256`, explicitly
state `tick_offset`, and provide a header with `schema:1`, `scenario`, `contract`,
`first_tick`, `last_tick` and `complete:true`. The contract must document resource
identity namespaces, tick origin/phase and matching scenario/input conditions.
Completeness is a driver/auditor assertion, not something inferred from silence
in an event log. A truncated raw file fails against the original manifest digest;
a newly asserted digest does not prove that the scenario finished. The probe
alone cannot produce this manifest.

For reference traces, `annotations` must map **every** decimal sequence index to
an independently evidenced `{sound,kind,trigger}` object. Allowed normalized kinds
are `sound`, `loop_start`, `loop_stop`, `music`, `music_stop`. Resolve IDs against
the user's resources and triggers against isolated input/observer evidence,
never by copying clone expectations or matching handles across processes. These
mappings and any executable-derived observations stay private. Clone annotations
may resolve only an `unknown` trigger; they cannot rewrite IDs, kinds or known
triggers. No event can be dropped. Unknown IDs/triggers, sequence gaps,
nonmonotonic/out-of-window ticks and incompatible capture contracts are rejected.
A streamed music track transition missing from the shim requires additional
reference instrumentation, rather than a fabricated annotation/event.

```sh
python3 -I tools/audio_parity.py normalize reference \
  work/parity/audio-attempt/reference.jsonl work/parity/audio-attempt/reference-manifest.json \
  --output work/parity/audio-attempt/reference-normalized.json
python3 -I tools/audio_parity.py normalize clone \
  work/parity/audio-attempt/clone.jsonl work/parity/audio-attempt/clone-manifest.json \
  --output work/parity/audio-attempt/clone-normalized.json
python3 -I tools/audio_parity.py compare \
  work/parity/audio-attempt/reference-normalized.json work/parity/audio-attempt/clone-normalized.json \
  --output work/parity/audio-attempt/diff.json
```

Normalized documents contain `header`, `event_count` and ordered `events`;
each event has `seq`, `tick`, `sound`, `kind`, `trigger`. Full validation precedes
comparison, including tail records after an early difference. CLI output files
are created exclusively under `work/parity/`. Compare returns 0 for equal
nonempty observed events, 1 for differences/no events and 2 for invalid input.
Equality covers only the declared capture and observed classes.

### Evidence and remaining dependencies

The isolated local probe in `work/parity/l6-audio-probe/` used the supplied
read-only installation and prebuilt runtime. It failed at loopback setup with
`Cannot open netlink socket: Operation not permitted`, before launching the
runtime. No reference sound ID, trigger, timing or music transition was measured.

A separate clone-only scripted smoke run completed inside `unshare -rn` with
both dummy SDL drivers and that private installation copy. Its nonempty JSONL
trace parsed with consecutive sequence indices and nondecreasing ticks; outputs
are in `work/parity/l6-clone-scripted/`. This checks the application's event-file
path only. An earlier clone-only run in `work/parity/l6-clone-probe/` timed out
and its partial trace is not accepted as a complete capture. No paired reference
comparison or trigger-coverage claim follows from either run.

| Trigger class | Current clone coverage by inspection | Differential evidence |
| --- | --- | --- |
| Selection | Voice dispatcher missing | unmeasured |
| Orders | Acknowledgement dispatcher missing | unmeasured |
| Combat | Animation requests; generic effects need causal annotations | unmeasured |
| Building | Animation frame/header requests; speech announcements incomplete | unmeasured |
| UI | Direct-WAV API can label requests; application dispatch missing | unmeasured |
| Music | Track start/stop requests and caller tick | unmeasured |
| Ambient | Mobile animation/loop requests | unmeasured |

The independent pieces are implemented and synthetically checked. Full L6
acceptance remains open: task 42's shared scenario/input runners, audited
reference resource/trigger and clock mapping, streamed music transition
instrumentation, and successful isolated paired captures are still needed.
No V/M gameplay label is promoted. `audio_events`, `audio_trace` and
`audio_parity` test invented banks, serialization and comparator rejection/
ordering/tolerance behavior without game data. Reference shim sources also pass
a standalone C syntax check; this does not verify a linked private runtime.
The final Release build and CTest run passed all 28 runnable entries; five
private-input/network entries skipped. All-file pre-commit gates and explicit
gates for new files passed. Changes remain uncommitted in this worktree.
