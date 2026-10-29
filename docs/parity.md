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
