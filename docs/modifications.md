# Game modifications

Modifications are clone-only simulation options. Every option defaults to off;
without an enabled option the engine uses its default original-style rules.

| Option | Default | Behavior when enabled |
| --- | --- | --- |
| `parallel_construction` | off | Each ordinary building progresses independently. Units still train one at a time per town. Hall upgrades retain the serial lane. |

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
The file uses English `name=on|off` lines, with optional blank or `#` comment lines:

```ini
parallel_construction=off
```

A missing or invalid preference file leaves defaults unchanged. Unknown names
or values in command-line options are errors. Command-line values override the
file regardless of argument order and do not themselves rewrite preferences:

```sh
build/aztec_engine --mod parallel_construction=on 1
build/aztec_engine --modifications-file work/preferences.ini --mod parallel_construction=off 1
build/aztec_engine --headless --mod parallel_construction=on --game work/private-game 1
```

Headless runs use explicit CLI modifications and ignore interactive preferences,
so personal settings cannot silently change automated scenarios. Resumed saves
use their recorded settings.

## Menus and live changes

Open **Options > Game modifications** in the front-end, or **Game modifications**
in the in-game pause menu. Each option has a description and on/off toggle.
Menu changes persist to the preference file; a failed write reports an error on
stderr. During a mission they submit a deterministic `SetModifications` command
that applies when simulation resumes. Changing mode never pays twice, resets
progress or refunds work. Turning parallel mode off makes current production obey
the shared serial lane again.

Each placed construction is visible in the scrolling production panel. Click its
slot to cancel; the tooltip shows its refund. Shift-click a started construction
to pause/resume only that site. Unit queue ordering and cancellation stay intact.

## Saves, replay and multiplayer

The AZS3 container with replay version 4 stores the initial modification bitset
following its command log. Later changes are commands in that log. Unknown bits
and older replay versions are refused. Save loading restores the recorded initial
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
