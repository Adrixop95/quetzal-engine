# RNG and tick order implementation

This is a partial alignment with the reference contracts in
[tick_order_and_rand.md](tick_order_and_rand.md) and
[audio_events.md](audio_events.md). No original executable or reference runtime
was run for this task. Matching the generator does **not** establish matching
random consumption or game parity.

Labels: **V reference** means an inspected fact already supported by those
research notes. **V engine** means an implementation property checked with
synthetic inputs. **G** marks a provisional mapping, missing behavior or an
inference requiring further evidence. These labels do not mean live measurement.

## Generator and ownership

**V reference:** the game-thread CRT state advances modulo 2^32 by multiplying
by 214013 and adding 2531011. Its result is bits 16 through 30, an unsigned
15-bit value. Seed zero produces 38, 7719, 21238. Zero is a valid state; it is
never a request to replace the seed. Other threads start with CRT seed one.
The loader calls srand once before its loading work. The SP/campaign zero-seed
conclusion in the reference notes is G; multiplayer can leave a different seed
in the process.

**V engine:** `sim/random.hpp` expresses that recurrence using unsigned
arithmetic. Each `GameSim` owns an explicit state, reset to zero in `init`,
before initializing objects. It is shared by aircraft initialization, combat,
decoration choices and computer-town planning. Interleaving two simulations
cannot perturb either stream. This models the game-thread isolation without
using process-global rand or host-thread-local state shared between simulations.

**V engine:** the pure planner takes the current mission state as its RNG input
and returns the advanced state. TownState::random records that planner call's
output, rather than selecting an independently seeded stream on its next call.
The mission RNG, logical object pool and active sound-loop choices participate
in the simulation hash. Replay reconstructs them from the same mission inputs
and command log.

**G:** the clone does not yet reproduce loading draws for winds and all native
constructors. Consequently, the three known values test the generator and the
synthetic aircraft loader, not the first three draws of a real loaded mission.
Original-save RNG restoration and arbitrary multiplayer seeds remain unresolved;
importing an original save does not provide original RNG continuation.

## Random-use inventory

This inventories all random generators and draw sites in `src/engine`, including
existing approximations. The reference notes enumerate the 123 native call sites
in 51 functions; their unidentified purposes remain unidentified here.

| Clone site | Before | Current implementation and evidence boundary |
| --- | --- | --- |
| `GameSim::init`, `spawn`: Bird minimum speed | Separate combat LCG, seed 12345 substituted when zero | Shared MSVC stream, one draw for a Bird with a positive cruise height. V engine; complete native creation order and loading draw count remain G |
| `spawn`: generic animation variant | One visual-stream draw for every birth | No simulation draw. Uses the same stable ID-based presentation fallback as mission placements. V engine; this is G visual selection, not a claim that every native constructor is nonrandom |
| `kill`: vehicle explosion/debris and fallback rubble slots | Separate visual LCG | Shared MSVC draws at the existing event sites. V engine; native death variants, rubble fallback and exact event counts remain G |
| `fire_weapon`: class-7 impact decoration | Visual LCG | Shared draw for the existing three-slot choice; G producer/variant parity |
| `fire_weapon`: class-8 volley buckets and scatter | Combat LCG | Shared draws for delay, radius when positive, and angle, for each of sixteen payloads. V engine; exact native scatter arithmetic and consumption remain G |
| `advance_effects`: contact and projectile impact decorations | Visual LCG | Shared draws, newest projectile first. V engine; complete native effect-owner mapping remains G |
| `ai::make_plan` candidate scoring | Per-town MSVC stream starting at one | Same MSVC primitive, supplied from the mission stream by the simulation adapter. V engine; planner scoring/offer/placement parity remains G |
| Supported animation frame/header sound requests | Audio-owned LCG, initially seven; no random pitch | Simulation chooses a sound-list member and a raw pitch draw. New loops retain both choices until their logical emitter disappears or changes set. V engine; native per-object emission phase, fixed/random pitch flags and sound lifetime mapping remain G |
| Selection/acknowledgement and standalone audio APIs | Audio-owned LCG | Separate MSVC stream with default seed one. V engine; mapping game-thread input-pump speech into replayed simulation events is still G |
| Music | Ascending cyclic selection | No random draws, unchanged; V reference selection policy |
| `WanderSimulation` | Independent LCG | Unchanged demo/test driver, outside GameSim/replay semantics |
| Movement avoidance/recovery | ID/tick arithmetic, deterministic tie breaks | Still no mission RNG draws here. V engine; these policies do not reproduce native random waypoint/recovery behavior |
| Initial object and imported animation variants | ID-based arithmetic | Still no RNG draw; G visual fallback |

**V reference / G missing engine consumers:** movement/goal/recovery functions,
building yield and progress, defence thinking, tactic initialization/path/target
assignment/scan intervals, speech and production notifications, town AI branches,
wind creation and updates, and multiplayer packet generation all contain draws.
The clone implements only subsets of these behaviors and does not insert dummy
or mission-specific draws to imitate their missing effects. Unidentified unit,
projectile and removal functions in the reference table are additional gaps.
The MP once-per-tick packet draw is not part of the independent lockstep format.

**G extra/misplaced consumers:** the existing three-slot impact, debris/rubble,
planner and scatter policies may differ from the native branches even though
they now use the right generator. Animation-based sound dispatch is an existing
supported producer, not a substitute for descriptor spawn/hit/death/weapon,
production and warning sound producers. No universal random draw on unit birth,
render frame, PCM callback or wall-clock interval was added.

## Sound and determinism

**V engine:** both graphical and headless loaders read optional `Snd.xbe` and
`Units.xbe` metadata through `RulesData::load_audio`, independent of audio-device
creation, `--no-audio`, volume, camera and viewing side. Supported scene requests
consume list-choice then pitch draws before presentation filtering. Resolved
choices are exposed as per-tick output. The mixer/backend consumes those choices
without drawing again when a hidden sound becomes audible or a voice is stolen.
Pausing retains logical loops without drawing or repeating one-shot events.
Missing/empty sound lists consume no draws. These policies are deterministic
engine construction, not verified native sound lifecycle equivalence.

**V reference:** randomized pitch is `rand()/65534 - 0.25`, giving playback rate
`0.75 + rand()/65534`. Ambient construction always draws pitch; the generic
helper draws only for the random-pitch sentinel and when its enable flag permits
it. **V engine:** resolved scene pitch drives a 16.16 playback step, spanning
0.75 through 1.25. **G:** linear sample interpolation approximates DirectSound;
fixed-pitch versus randomized native caller flags and enable-flag transitions
are not yet represented by the animation-only scene producer.

**G:** scene choices currently run together after unit/projectile work. Native
helpers called inside an object update can consume draws before the next object.
That interleaving, descriptor-only producers, loading sounds and input-pump
side effects remain necessary before claiming an identical mission RNG sequence.
UI-only calls retain their separate stream to avoid introducing view-dependent
simulation behavior while the input event/replay mapping is incomplete.

## Tick phases

**V reference:** the native order is day clock/latches, commands for sides 0..6,
effects, scripts, towns, sides, tactics, units, projectiles, winds, sounds,
outcome, fog/minimap and global time. Sound helpers may also run inside earlier
phases. The clone now follows these supported phase boundaries:

| Phase | Engine behavior | Evidence / remaining difference |
| --- | --- | --- |
| Command boundary | Stable ascending sides 0..6, preserving same-side submission order; side 7 packets are logged but ignored | V engine/reference side range; G active-side gate and original one-packet-per-side ring/latency. Multiple commands remain an engine protocol convention |
| Day clock / visual clocks | Day clock advances on both tilesets and wraps at 7200; visual clocks advance on active ticks | V reference branches rechecked on 2026-11-24. G: day-clock advance follows pause commands instead of preceding packet processing; visual clocks remain grouped |
| Effects | Decorations advance before scripts, newest first | V engine. New decorations created during the walk wait until the next effects phase |
| Scripts | Trigger evaluation before town/unit work | V engine. G: source-order triggers and numeric/path collections have not been mapped to native loader creation order |
| Towns / sides | Production/economy, then AI and town service/capture; AI/service/capture walk towns in reverse creation-vector order | V engine order. G: grouped passes and side-indexed production queues do not yet reproduce interleaved native town callbacks and side updates; harvest remains grouped here |
| Tactics | Follow updates precede unit orders | V engine. G: no independent native tactic pool/lifetime model; target assignment and scan cadence retain documented policies |
| Units / movement | Logical object pool walks newest first; independent mover follows, then position synchronization | V engine object order. G: batch navigation/collision resolution is not native sequential object movement |
| Projectiles | Advance after units/movement, newest first | V engine. Projectiles created by units participate in this tick; projectiles created during this walk wait for its next invocation |
| Winds | No independent subsystem | G missing; no guessed RNG consumption |
| Sounds / outcome | Resolve supported scene sound choices, then copy the script side outcome to the world result | V engine/reference phase boundary; G sound interleaving and native local-side mapping |
| Fog / minimap | Fog/light/vision update after combat and sounds; graphical minimap consumes the result | V engine boundary; G exact native fog/transport callback integration |
| Global time | Completed World::tick advances last; the command-log serial identifies the current invocation | V engine; paused command boundaries also increment the serial/completed counter. G original startup globalTime and packet-ring semantics |

## Pool storage and creation

**V reference:** active game-object lists insert at the front, iterate newest
first and reuse the most recently freed slot. Most native walks save next before
the callback; new insertions at the head are not visited during that same walk.

**V engine:** `PoolOrder` keeps logical slots, an active doubly linked order and
a LIFO free stack. Unit creation inserts at the head. Permanent removal returns
the logical slot; boarding and dormant patrols do not destroy their object.
Command IDs remain stable, append-only identities; a stale command cannot become
a command for the new occupant of a reused logical slot. Storage handles are
separate from command IDs, including in synthetic fixtures with unusual IDs.
Snapshots protect walks against deletion and head insertion.

**V engine:** effect/projectile vectors retain chronological storage while their
phase walks run backward. Lifetime ownership is separate from visual class,
so rubble remains in the effects phase and projectile impact tails remain in
the projectile phase. Survivors retain relative order; callback-created
effects are appended afterward, so the walk does not alternate directions on
successive ticks. Effects/projectiles do not expose reusable storage addresses.
Town vectors are immutable after initialization and are walked backward where
mapped. **G:** original allocation pages, descending initial slab addresses,
save-pool identities, script/tactic/sound-emitter slot reuse and spatial-query
iteration order are not emulated. The native handle/address scheme itself is
unresolved in the source notes; no pointer IDs were invented.

## Replay compatibility and verification

**V engine:** AZS3 retains container layout 5 and now requires replay semantics
14 (previously 13). Standalone command logs use AZC2 and reject AZC1 rather than
silently replaying old commands with new random/order semantics. New saves also
fingerprint `Snd.xbe`, since sound metadata now affects simulation draws. Parsing
failures preserve caller output; replay/save continuation reconstructs the shared
RNG, sound-loop choices and pool/free-list state.

Synthetic checks cover the seed-zero sequence, seed-one independence, unsigned
wraparound, mission reseeding and three Bird loading draws; newest-first walks,
LIFO reuse and mutation snapshots; projectile order stable over consecutive ticks,
units before projectiles, once-only effect aging, end-of-tick global time and
tileset clock wrap; lethal duel order and launch-tick projectile behavior;
version rejection, new pinned engine hashes and replay equality. The audio suite
checks pitch endpoints/interpolation and 1200 ticks with scene RNG enabled,
changing visibility, volume, selection, device cadence and output rate, followed
by command-log replay. Existing long process/replay suites remain required.

Native packet/identity observers, missing object/tactic/wind/sound producers and
original-save RNG continuation are dependencies for the remaining G items. This
task does the independent generator/order work without claiming those tasks have
landed. Reference record/replay was deliberately not attempted under the explicit
prohibition on running the original.

Validation in this worktree: baseline and final Release build/full CTest gates;
73 passes and six expected private-data/network skips in the default suite.
Five focused ASan/UBSan checks pass (`engine_unit`, `ai_unit`, `script_unit`,
`determinism_state`, `audio_events`), including a final audio rerun after the
silent-choice backend guard. Pre-commit checks include the new untracked files.
Separate `formats` and `engine` checks pass against private game copies with
dummy SDL inside `unshare -rn`. Two independent 120-tick mission-1 clone runs
match at every tick; saving at tick 60 and resuming in another private copy
matches all 61 recorded continuation checkpoints through tick 120. These are
clone repeatability/data checks, not an original-game comparison or campaign
completion. Probe inputs, logs, saves and reports remain in ignored
`work/rng_tick_validation/`.
