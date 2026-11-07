# Play profiles and game modifications

Profiles select the complete helper preset. Interactive play defaults to
`enhanced`; headless runs default to `faithful` and ignore interactive preferences.
Parity runners, measurement launchers and ordinary synthetic simulations select
`faithful` explicitly. A new helper must default to off in `Modifications::faithful()`.
A faithful preset disables every helper; it preserves the pre-profile engine
rules and hashes, but does not establish parity for existing engine approximations.

| Switch | faithful | enhanced | Current implementation |
| --- | --- | --- | --- |
| `parallel_construction` | off | on | Each ordinary building progresses independently; units and hall upgrades retain the serial town lane. |
| `parallel_training` | 1 (off) | 3 | Train 1–8 units per town, or one per completed matching producer with `per building`. `on` selects 3. |
| `relaxed_placement` | off | on | Allow map objects and ignore building clearance during placement. |
| `unit_unstick` | off | on | Repath, yield and relocate stalled units; see [unit_unstick.md](unit_unstick.md). |

Helpers apply to the whole simulation, including computer players.

## Command line and precedence

```sh
build/aztec_engine --profile faithful 1
build/aztec_engine --profile enhanced --mod parallel_construction=off --mod parallel_training=5 1
build/aztec_engine --headless --profile faithful --game work/private-game --max-ticks 120 1
build/aztec_engine --headless --profile enhanced --mod unit_unstick=off --game work/private-game 1
```

All four switches accept `--mod NAME=on|off`. `parallel_training` additionally
accepts `1` through `8` and `per building` (CLI alias `per-building`); `off`
selects serial training (`1`), while the internal value zero selects per-building capacity. Unknown names, profiles and
invalid values are errors. The profile selects the existing training behavior.

Interactive precedence is: enhanced default, persisted profile and overrides,
explicit CLI profile, explicit CLI overrides. A CLI profile resets the stored
overrides; CLI `--mod` values win regardless of their position relative to
`--profile`. Repeated values use the last value. Headless starts from faithful,
applies its CLI profile and overrides, and never reads `modifications.ini`.
Resumed saves restore their recorded effective settings instead of CLI preferences.
Command-line selection alone does not rewrite preferences.

## Preferences and helper menu

`modifications.ini` lives in SDL's per-user `Quetzal/Engine` directory.
`AZTEC_USER_DIR` selects an absolute alternative user directory;
`--modifications-file FILE` selects the interactive helper preference file.
Original game settings are never edited. The file stores a profile and only its
explicit overrides (including overrides equal to the preset):

```ini
profile=enhanced
parallel_construction=off
parallel_training=5
unit_unstick=off
```

Blank and `#` comment lines are accepted. Profile and override lines may appear
in either order. Missing or malformed files leave the default intact. Legacy
files without `profile=` keep the faithful base and their saved switch values.

F9 opens the helper panel during play and in the front-end. It is also available
through Options > Game modifications and the pause menu. It shows the profile,
all effective values and explicit overrides. Clicking the profile
switches the entire preset and clears overrides. Clicking a helper toggles its
override; training cycles through 1–8 and per-building capacity. Training overrides can be supplied
through the CLI or preference file. Cursor confinement now uses Ctrl+F9.
Menu edits persist immediately and report write failures to stderr. In a mission,
effective changes submit the existing deterministic `SetModifications` command
and apply when simulation resumes.

## Construction, saves and hashing

Parallel construction retains task 48's semantics: accepted placements create
visible unpaid placeholders immediately in both presets. Payment happens when
work starts; shared cash uses insertion order. Unpaid cancellations refund
nothing; paid cancellations refund the established unbuilt fraction. Missing
halls suspend work. Placement, prerequisites and conservative AI planning stay
in force. Switching helpers never resets progress, pays twice or refunds work.
Switching construction off returns current work to the serial town lane.

The AZS3 replay-version-4 effective bitset keeps construction in bit 0, placement
in bit 1, and training in bits 2–5 (0–7 for capacities 1–8, or 8 for per-building
capacity), preserving develop saves. Unstick uses bit 6. Other bits and training
encodings above 8 are rejected. Old all-off and construction-only saves retain their
encoding. Current and initial effective settings participate in simulation hashes,
replay and lockstep compatibility. Profile labels and override bookkeeping are
preference metadata and do not change hashes. Saves restore effective values;
they do not reconstruct a user's original preset/override choices. Save UI can
therefore show faithful with overrides after loading an enhanced save.

Synthetic checks pin pre-task movement hashes at initialization and ticks 1
through 5 in a fixture without buildings or mission groups. Construction hashes
are compared against the legacy all-off bitset at initialization and ticks 1 and
5, so unrelated formation changes on develop do not invalidate the baseline.
Checks also test fresh-process preference reloads and actual F9 edits across engine
restarts, and verify enhanced save continuation. These establish internal
compatibility. Original behavior comparisons remain **needs live run**; commands
are in [104-profiles-faithful-and-enhanced.md](104-profiles-faithful-and-enhanced.md).
