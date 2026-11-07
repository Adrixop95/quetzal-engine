# Game modifications

Modifications are clone-only simulation options. The faithful profile defaults to off (parallel training uses value `1`);
without an enabled option the engine uses its default original-style rules.

| Option | Default | Behavior when enabled |
| --- | --- | --- |
| `unit_unstick` | off | Repath, yield and relocate stalled units. |
| `relaxed_placement` | off | Allow map objects and ignore building clearance during placement. |
| `parallel_training` | 1 (off) | Train 1–8 units per town, or one per completed matching production building. Applies equally to human and computer sides. |
| `parallel_construction` | off | Each ordinary building progresses independently. Unit concurrency is controlled separately by `parallel_training`. Hall upgrades retain the serial lane. |

Accepted map placements always create a visible, unpaid construction placeholder
immediately, in both modes. The default town queue advances one item at a time,
including buildings. Parallel mode separates buildings from that serial lane.
Payment occurs when work starts. Shared cash is arbitrated in insertion order;
unpaid cancellations refund nothing and paid cancellations refund the established
unbuilt fraction. Missing halls suspend work. Placement, prerequisites and the
AI's conservative busy-town planning policy remain in force.

## Preferences and command line

The interactive engine stores `modifications.ini` in SDL's per-user preference
directory for organization `Quetzal`, application `Engine` (normally
`~/.local/share/Quetzal/Engine/` on Linux). It does not write original game settings.
The file uses English `name=value` lines, with optional blank or `#` comment lines:

```ini
parallel_construction=off
parallel_training=1
relaxed_placement=off
unit_unstick=off
```

A missing or invalid preference file leaves defaults unchanged. Unknown names
or values in command-line options are errors. Command-line values override the
file regardless of argument order and do not themselves rewrite preferences:

```sh
build/aztec_engine --mod parallel_construction=on 1
build/aztec_engine --mod parallel_training=4 1
build/aztec_engine --mod "parallel_training=per building" 1
build/aztec_engine --modifications-file work/preferences.ini --mod parallel_construction=off 1
build/aztec_engine --headless --mod parallel_construction=on --game work/private-game 1
```

Headless runs use explicit CLI modifications and ignore interactive preferences,
so personal settings cannot silently change automated scenarios. Resumed saves
use their recorded settings.

## Menus and live changes

Open **Options > Game modifications** in the front-end, or **Game modifications**
in the in-game pause menu. Each option has a description. Construction, relaxed placement and unit unstick toggle on/off; training cycles through 1–8 and "per building".
in the in-game pause menu. The panel shows the profile and effective values. Construction and relaxed placement toggle on/off; training cycles through 1–8 and "per building".
Menu changes persist to the preference file; a failed write reports an error on
stderr. During a mission they submit a deterministic `SetModifications` command
that applies when simulation resumes. Changing mode never pays twice, resets
progress or refunds work. Turning parallel mode off makes current production obey
the shared serial lane again.

Each placed construction is visible in the scrolling production panel. Click its
slot to cancel; the tooltip shows its refund. Shift-click a started construction
to pause/resume only that site. Every running unit also displays a status marker and its own progress/refund tooltip; Shift-click pauses or resumes that unit. The pause button controls the first item. Scroll to reach additional slots. Unit queue ordering and cancellation stay intact.

## Saves, replay and multiplayer

The AZS3 container with replay version 4 stores the initial modification bitset
following its command log. Later changes are commands in that log. Unknown encodings
and older replay versions are refused. Bit zero retains construction and bit one retains relaxed placement; bits 2–5 encode training as 0–7 for values 1–8, or 8 for "per building". Bit 6 enables unit unstick independently of training. Default metadata and hashes remain unchanged. Pre-merge unit-unstick builds used bit 2 for recovery; saves and replays with that option enabled are incompatible with this merged layout. Save loading restores the recorded initial
and older replay versions are refused. Bit zero retains construction and bit one retains relaxed placement; bits 2–5 encode training as 0–7 for values 1–8, or 8 for "per building". Bit 6 stores the unit-unstick configuration stub. Default metadata and hashes remain unchanged. Binaries predating training reject enabled training encodings. Save loading restores the recorded initial
set before replay, independent of local preferences. Low-level `GameSim::replay`
callers must pass the initial set explicitly; the default is all options off.

Initial and current modification sets participate in simulation state hashing.
Lockstep's existing initial hash handshake rejects peers with different sets
before either advances a tick. Live changes travel through the same ordered
command protocol as other simulation commands. The engine does not yet expose
a multiplayer lobby; this contract covers the transport-independent lockstep API.

See [the rule inventory](engine_rules_status.md#task-48-placeholders-and-optional-parallel-construction-2026-10-30)
for V/M/P evidence and the failed isolated reference launch. Engine replay and
synthetic tests establish internal consistency, not original-game parity.

## Parallel training policy

Numeric values share a town-wide capacity across all unit types. The special
value `per building` (CLI alias `per-building`) shares a separate capacity for
each production building type. The first town-local structure prerequisite in
the existing build tree identifies that producer; side-wide prerequisites unlock
technology and never add slots. Units with no local producer prerequisite use
the completed town hall. Only living, present, completed buildings belonging to
the same town and side count. In this mode production-building recipes can be
placed repeatedly, allowing additional barracks, stables and other producers.
The special mode has no eight-building cap. `off` is an alias for `1`;
`parallel_training=on` selects the enhanced profile capacity of 3.

The queue processes eligible items in insertion order and pays the full price
for each unit on its own start tick. Paused units occupy capacity. Running units
retain separate work clocks and refunds. A capacity reduction suspends excess
items in queue order without losing their progress or charging them again;
restoring capacity resumes them. A completed unit frees its slot for another
item on the same tick, as in serial mode. Ordinary buildings and hall upgrades
remain barriers between training batches unless the existing construction
option gives ordinary buildings independent work. Missing halls suspend work.

Computer players fill batches only with the global training modification enabled,
using the planner's existing unit choice, remaining target and affordable count.
The planner's other busy-town gates stay in force. No side has a private override.
Engine saves reconstruct all timers, payments, pauses and mode changes from their
recorded initial setting and command log, using the existing save mechanism.

See [task 100 validation and live commands](parallel_training.md). This intentional
modification has synthetic coverage; it is not an original-game parity claim.
The profile table, helper switches, persistence and replay contract are maintained
in [game_modifications.md](game_modifications.md).

See [the rule inventory](engine_rules_status.md#task-48-placeholders-and-optional-parallel-construction-2026-10-30)
for task 48's construction evidence and reference limitations.
