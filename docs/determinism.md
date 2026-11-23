# Simulation determinism audit

Evidence labels (audit 2026-11-23): V = inspected source fact, with reference
or clone scope stated; M = measurement rechecked from retained captures or an
offline clone probe; G = inference, proposed procedure or uncorroborated
historical report. A clone V does not establish native parity. A marker covers
one paragraph/list item or the entire immediately following table. Unresolved
meanings remain G even inside a table whose codec structure is V.
See [label_audit.md](label_audit.md) for counts and evidence limits.

**V** (clone construction; native scheduling/consumption remains G). V03 progress, 2026-10-28. These are independent-engine construction properties
and synthetic checks, not reference parity or campaign completion.

## Ordering and tick phase

**V** (clone construction; native scheduling/consumption remains G). Player commands are stably sorted by side, retaining submission order within a
side. All are applied and logged before pause handling. Thus a later-side
PauseOff can override an earlier-side PauseOn in the same tick. A paused tick
still advances world/simulation tick counters and the driver's animation clock;
it skips scripts, economy, AI, effects and movement. After player commands, an
active tick advances the day clock and animation ages, updates non-projectile effects, executes scripts, economy, AI, town maintenance/service/capture, tactics/follow/orders, facility service and movement, then projectile/transport/presentation state. Global time advances at completion.
AI and script commands are recomputed during replay, rather than logged twice.
These are engine policies pending V02 reference-order evidence.

**V** (clone construction; native scheduling/consumption remains G). The replay observer runs before submitting the current tick's commands and before
`begin_tick`. Its tick 1 state is the initialized world; its tick t state follows
t-1 updates. Unordered, zero-tick and beyond-horizon logs are rejected before
initializing the destination world; an unreadable requested script is rejected.
Reinitialization clears pending commands, the command log and tick counters.

## Random streams and arithmetic

**V** (clone construction; native scheduling/consumption remains G). The simulation owns a uint32_t random state initialized to zero. Its CRT-compatible recurrence is `state = state * 214013 + 2531011` with unsigned wrapping; output is `(state >> 16) & 0x7fff`. Seed zero's first output is 38 by arithmetic, not a new live measurement. Combat, decorative, planner and supported scene-sound draws share this state. State and simulation sound choices are hashed. Native seed/consumption parity remains unresolved.

`WanderSimulation` remains a separate demo. Standalone UI/audio calls have a separate presentation stream; simulation-selected scene sounds are supplied to the dispatcher independently of mute/visibility. Rendering and device playback do not advance simulation RNG. See [rng_and_tick_order_implementation.md](rng_and_tick_order_implementation.md).

**V** (clone construction; native scheduling/consumption remains G). Production multipliers now use exact integer representations of the existing
binary32 coefficients over 2^24. A split product computes rounding without host
`long double`, FMA, or nonportable 128-bit types; values saturate at INT_MAX.
Nonpositive base costs/times return zero. Warehouse income uses the exact ratio
11184811/8388608, with a bounded result. These preserve the specified coefficients,
including their binary32 approximation, rather than replacing them with ideal
2/3, 4/3 or 4/5 ratios. Synthetic rounding and overflow controls pass. This change
does not investigate the historical lifted 161-versus-160 price artifact.
Script timer/comparison storage uses int64_t instead of platform-width `long`.

## State hash contract 2

Hashes mix integer values and ordered collection lengths, never addresses,
container capacity, C++ struct bytes or host padding. They are diagnostic digests,
not collision-proof proofs of equality. Equal immutable rules and loaded sight/
animation resources are a precondition; this is not a rules/content digest.
Engine saves enforce a separate content fingerprint; the independent lockstep transport still requires the caller to establish matching static inputs.

Coverage now includes:

- World/simulation ticks, pause flag, shared simulation RNG, spawn counter and pending
  command payloads/ID ordering.
- Object source, identity, ownership, HP, orders, animation clocks/variant,
  construction/harvest/repair state, movement links and previous/current positions.
- Effect identity, payload, endpoints, interpolation positions and clocks.
- Full town identity/owner/position/level, side AI/disabled-type/start inputs,
  script paths, resource cells/dimensions, queues/groups/repairers and AI state.
- Relations, mission and per-side outcomes, script-side identity, import mode,
  script program plus enabled triggers/timers/path state, and fog bytes/dimensions.
- **V** (clone construction; native scheduling/consumption remains G). Navigation terrain and dynamic blocker **counts**, grid version, movement
  configuration, agents, steering caches, order references and flow-field costs.
  Grid/field digests are cached at mutation/build boundaries to avoid rehashing
  immutable cost arrays every tick.

Local selection, viewing side, camera requests/serials, fog invalidation counters,
messages, playback-device state, replay history, diagnostics and spatial scratch arrays are
excluded. Simulation sound choices and random state are included. Messages/camera requests affect presentation only; replay history is
not read by simulation. Spatial arrays are rebuilt before use. Map artwork,
profiles and unresolved imported-save records are not simulation inputs once
initialization has populated the world. New state producers must extend this audit.

The changed digest is incompatible with earlier recorded engine save hashes.
The UI now refuses a replay mismatch or invalid replay before replacing the active
world. Save requests wait for a completed tick with no pending commands, so the
command-log snapshot cannot silently lose future inputs. A locally paused game
with pending commands must resume before that deferred save can complete. Current bounded, fingerprinted and atomic save handling is documented in [engine_saves.md](engine_saves.md); full native campaign continuation remains open.

## Reproducible checks and remaining evidence

**G** (historical coverage or pending native acceptance). `determinism_state` checks omitted-field sensitivity, presentation exclusions,
cross-side and same-side ordering, exact before-command alignment, replay-log
validation, script-program differences and navigation/configuration differences. It also checks four known spawn-variant draws from
the initial seed.
`determinism_processes` runs a synthetic 14,400-tick world (20 simulation minutes):
training/spawns, conflicting pause commands, movement, combat/death, a save at
tick 7,201, fresh-process restoration and continued inputs. It compares every tick
against another process using a different viewing side, camera update count,
interpolation fraction and audio volume settings, and against full command-log
replay. It checks both differing and truncated trace controls. No assets or
reference binary are needed. Presentation checks exercise camera/interpolation
and silent audio updates; they do not verify a renderer or live audio device.

Run the portable tests and optionally export a synthetic transcript:

```sh
AZTEC_DETERMINISM_TRACE="$PWD/build/determinism.trace" \
  ctest --test-dir build -R 'determinism|oracle_packets|parity_controls' --output-on-failure
python3 tests/test_determinism_processes.py --compare \
  build/gcc.trace build_clang/clang.trace
```

**G** (historical coverage or pending native acceptance). The build workflow exports transcripts on GCC/Linux, MSVC/Windows,
MinGW/Windows and AppleClang/macOS and requires exact comparison in a dependent
job. Adding that job does not establish that remote CI has passed. Campaign
coverage, native Windows execution, renderer/audio-device variations and the V02
reference-order/RNG observations remain required before V03 can be closed.

## Label audit corrections (2026-11-23)

Evidence checked: Reference: CRT 0x481070 and dispatcher 0x4577b0; their existence does not prove clone draw/phase parity. Clone: `sim/random.hpp`, `GameSim::init`, `tick`, `state_hash`, `commands.cpp`, save handling and determinism tests. Construction properties are V clone; native consumption and historical multi-process totals require their own observations.

1. Replaced the obsolete seed-12345/1664525 generator and presentation-only draw inventory with the seed-zero CRT-compatible stream; native consumption remains unresolved.
2. Corrected the active tick ordering to GameSim::tick, including effects before scripts and projectiles after movement.
3. Replaced the obsolete missing save-hardening claim with its implementation and acceptance boundary.
4. Corrected the missing-save-content-check claim; saves already enforce fingerprints.
5. Separated excluded device playback from included simulation sound choices in the hash inventory.

No original executable or reference runtime was run for this audit. Historical
validation totals retain their original dates; they are not this
audit's test results. Private captures, derived source and probe output remain
outside the tracked documentation.
