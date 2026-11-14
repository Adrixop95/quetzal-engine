# Save/load full-state audit (task 116)

This task verifies continuation of the independent engine, not original-game
save compatibility or gameplay parity. No original executable was run, and no
executable code or lifted source was inspected. The independent engine read its
normal private simulation/sight inputs at runtime. There are no new (V)
original-game findings. (G) Replay
restoration, compatibility rejection and presentation policies below are engine
contracts; they remain assumptions about the desired original-game experience.

## Restoration contract and fixes

A save represents a completed tick, including tick zero. The engine reconstructs
the mission with identical fingerprinted inputs, initial modifications and the
applied command log, then checks tick and simulation hash before publishing the
candidate world. Tick advancement also records paused simulation ticks. The UI
waits for pending commands to be processed before saving; pending commands are
hashed and cannot silently produce a successful incomplete replay. Direct editor
mutations, imported original saves and different simulation inputs are outside
this replay contract. Restoration cost grows with elapsed mission time.

The audit found two presentation losses. Local UI selection was discarded on
load, although deterministic command selection and control groups were replayed.
Format 5 now stores selection order and viewing side separately from gameplay
hashes. A replayed historical camera request could also override the saved
camera on the first frame. Both startup resume and in-session load now mark
those requests consumed. Subsequent script camera requests still work normally.

Readers return distinct English diagnostics for unsupported AZS2/layout/replay
versions, damaged/truncated input, unreadable files and oversized files. The UI,
slot labels and headless runner surface them. Failed reads/replays publish no
partial state. Format 4 and earlier layouts are deliberately rejected rather
than replayed under incomplete presentation semantics. Replay version stays 7:
this change does not alter simulation or its hash contract.

## Field inventory

The following are observations of the independent source and synthetic checks,
not (V) claims about the original. All rows require unchanged mission/rule/sight
inputs. Immutable input identities are checked separately from mutable state.

| State | Restoration and verification |
| --- | --- |
| Transport | Cargo order, carrier/boarding IDs, enabled/route/arrival flags, landing coordinates, removed passenger state and disabled movement agents replay; these fields enter the simulation/movement digests. |
| Production | Ordered town queues, work clocks, paid amounts, pause flags, reserved sites, placeholder IDs, construction HP/progress and runtime town speeds replay and enter the digest. |
| AI | Planner phase, timers, target, remaining work, defence, known towns and income/spending accounting replay and enter the digest. AI decisions are regenerated, not injected a second time from the external command log. |
| Scripts | Script program, local time, timer values/running/existence, enabled triggers and tactic locks/path cursors/cycle/inversion replay and enter the script digest. Trigger execution histories and outcome provenance are reconstructed diagnostics. |
| Counters/objectives | Money, unit/town/tactic state, clock conditions, per-side outcomes and mission outcome replay. There is no independent general script-counter or structured objective-progress store in this checkout; preserving nonexistent semantics cannot establish campaign completeness. |
| Fog/exploration | Every side's persistent visibility grid, dimensions and brightness mode enter the digest; exploration memory is retained in the grid rather than a separate explored bitmap. `fog_version` is a local rendering invalidation counter. |
| Projectiles/effects | Ordered effects, trajectories, age/lifetime, aim, damage/payload, source/target and animation state replay and enter the digest. Diagnostic trace IDs are reconstructed but excluded. |
| Objects/movement | Health, harvest residues/cursors, tactics/orders, animation clocks, construction, spawn counter, navigation blockers, movement agents/orders/flow fields and movement recovery state enter the existing digest. |
| Selection/groups | Command selection and all ten groups are hashed/replayed; local UI selection is format-5 presentation metadata and does not change gameplay hashes. |
| Camera/messages | Camera position/zoom is explicit save metadata. Script center serial/coordinates and timed messages reconstruct; the full-state fixture also compares these outside the gameplay hash. |
| Diplomacy | The entire directed relation matrix and changes to town ownership replay and enter the digest. |
| Random state | The simulation's visual RNG stream and spawn counter replay and enter the digest; births and projectile impact effects exercise subsequent draws. Audio has its own presentation RNG and is outside simulation continuation. |
| Session/input | Initial and commanded modification bits and simulation pause replay. Local targeting mode, modal windows, audio playback and wall-clock interpolation are not saved. (G) Loading closes the modal and resumes the saved simulation rather than restoring an open save dialog. |

The existing `determinism_state` test checks sensitivity to future-affecting
fields, pending command ordering and view-independent hashes. This task did not
find a simulation hash divergence requiring a new hash contract. This inventory
is not a proof over every possible command sequence or future engine field.

## Synthetic acceptance

`save_state_processes` creates 15 invented river/town scenarios, without game
files or repository rule tables. Generated prices, health, weapon, sight masks,
terrain and scripts are test inputs only. Each runs 400 ticks and saves at
0, 1, 2, 5, 20, 45, 60, 120, 240, 355 and 399. Every case exits and restores in a
fresh process. The comparator requires the loaded boundary and every subsequent
tick through 400, with explicit lengths and first-divergence reporting.

Coverage assertions require boarding in progress, occupied cargo, active
construction, queued production, projectiles, births, evolving fog, AI elapsed
state and mission victory. The tick-1 checkpoint must contain boarding, combat
and construction; tick 120 must contain cargo. Scripts exercise stopped/restarted
timers, trigger disable flags, repeated money changes, diplomacy and camera/text
requests. Paused ticks and later unload/production/impact behavior test future
state rather than only equality at the boundary. All 165 continuations match.

`campaign_save_processes` uses 15 generated mission files and empty invented
archives. It checks headless continuation, Ctrl+S/Ctrl+L in separate processes,
selection and camera after a historical script camera event plus manual panning,
the actual pause-menu save/load confirmation flow, frontend slot selection,
corrupt files and old version refusals. It requires the camera to have moved
before saving, so merely restoring the old script camera cannot pass.

`engine_save` retains truncation/bit-flip/checksum-repaired malformed input,
resource limits, unchanged-output, atomic-write and replay-refusal checks. New
cases cover legacy/future layout diagnostics and invalid selection count, IDs,
duplicates and viewing side, including checksummed malformed metadata.
`save_roundtrip_controls` exercises all 15 collector mission slots with synthetic
process results, including first-hash mismatch, truncation and early termination;
none may be reported as a full-horizon pass.

## Private campaign collector

On a namespace-capable Linux host, create private `mission_N.orders` files for
all requested missions using the headless move/attack-move command format. Keep
routes under ignored `work/`. Choose save ticks around mission-specific events;
a tick number alone does not demonstrate boarding, combat or construction.

```sh
python3 -I tools/save_roundtrip.py \
  --engine build/aztec_engine --game /home/adrox/aztec-re/game \
  --orders-dir work/save-routes --max-ticks 2400 \
  --save-ticks 1 120 600 1200 --out work/save-roundtrip-campaign
```

The default mission list is all 15. Each save tick has its own uninterrupted
recording and a separate restoring process. Each process gets its own private
game directory, ordinary Maps/Saves directories, immutable asset symlinks,
copied configuration and private user profile. Engine/rules/routes are pinned
inside the output directory. Launches use dummy SDL video/audio inside
`unshare -rn`, with process-group wall timeouts. The collector never launches the
original executable. All saves, command routes, logs, hashes and results stay
under this worktree's ignored `work/`, and the source installation is read-only.

`results.json` records execution failures and first divergence, and is updated
following each case. Missing data, rejected commands, a save never reached, a
failed restore or truncated transcript cannot pass. Equal continuation through
an early victory/defeat is retained as an equality result but is not counted as
reaching N ticks. Review the mission report as well: unsupported script behavior
can be equally wrong in both runs.

## Original-game and long-route run sheet

These are pending maintainer measurements, not results. Record Windows version,
original game revision, engine revision/diff, mission, speed, profile, route and
save tick privately. Repeat each capture twice. Never publish saves, scripts,
raw state dumps or assets.

| Measurement | Required capture / acceptance |
| --- | --- |
| Original save/load policy (G) | For all 15 missions, save during combat, boarding, occupied transport and construction; record which selections/groups/camera, timers, objectives and modal/pause state survive. Mark a finding (V) only after the captures establish it. |
| Full campaign engine routes | Supply routes that reach all requested save ticks, with event evidence at each tick. Run the collector through the long horizon, then repeat native Windows fresh-process UI continuation. Require boundary and every continued hash, including later cargo unload, births, projectile impacts and script objectives. |
| Quick and menu slots | On Windows, verify quick save/load, all three menu slots, overwrite confirmation, frontend resume, camera after prior script events and visible selection/groups. Record cancellation and failed-load state preservation. |
| Compatibility/corruption | Present AZS2, earlier AZS3 layout/replay versions, truncations, damaged checksum, mismatched content and checksummed bad state hashes. Require a clear refusal and unchanged active session/slot; never overwrite the only copy. |
| Unsupported script/objective semantics | Resolve the missing counter/objective/presentation producers before claiming original mission-state completeness. Extend this inventory and synthetic continuation tests whenever those fields are introduced. |

Tasks 126 (script presentation/objectives), 120 (fog parity), 105 (transport)
and the all-mission campaign sweep remain queued in this checkout. Their engine
implementations are partial; this task completes the independent persistence
checks available here without inventing missing game semantics. Original-save
interoperability remains the separate tasks 72/94. All-15 winning routes and
original-game parity are not established by save determinism.

## Local validation

Baseline: Release build, 63 passing CTests, six skips, and all-file pre-commit
passed before editing. Final Linux GCC Release build and CTest passed: 65
passed, six skipped (five private-data entries and UDP process isolation). Both
tracked-file pre-commit and explicit new-file checks passed. Native Windows and
macOS were not run.

The private independent-engine campaign collector used dummy SDL, `unshare -rn`,
private symlink installations and a single scripted move at tick 1 in each
mission. The 400-tick sweep at save ticks 1/20/120/240 produced 52 full-horizon
passes and seven equal continuations through earlier mission outcomes; one
scheduled save was never reached. There were no hash divergences. This collector
correctly returned failure for the eight incomplete-horizon cases. These short
runs do not supply event-specific campaign boarding/construction coverage or a
winning route. Private evidence remains under `work/save-fidelity-campaign/`.
The final-binary sweep through 200 ticks at save ticks 1/20/60/120 passed all
60 cases across all 15 missions, including every loaded boundary and continued
tick. Private evidence remains under `work/save-fidelity-campaign-200/`. This
short smoke horizon verifies continuation, not full-mission completion.
