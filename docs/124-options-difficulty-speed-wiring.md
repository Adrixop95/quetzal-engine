# Options, speed and difficulty (task 124)

Status: settings wiring implemented; full task acceptance remains partial pending
verified global difficulty/AI mappings, original providers and live measurements.
V labels a fact inspected in private original sources/data. G labels an independent
engine policy or an assumption. Inspection is not a live parity measurement.
No original executable was launched. No private code, assets or generated data
belong in this document or the public changes.

## Evidence and behavior

| Setting | Evidence | Engine behavior / remaining gap |
| --- | --- | --- |
| Resolution | V: option update at 0x41ec80 uses 800x600 and 1024x768; availability is device-dependent. | G: portable window resizing, retaining existing fullscreen/borderless modes and arbitrary CLI sizes. |
| Gamma | V: 0x41a848 through 0x41a876 updates the display's gamma and rebuilds its conversion state at 0x478680. | G: full-frame RGB power curve, range 0.5..2.5; exact original curve and slider scale need measurement. |
| Music, effects, speech | V: separate mixer groups, applied by 0x43f070 after profile selection. Original slider constructors use attenuation units, not linear percentages. | G: independent linear volume controls retained; original attenuation curve not claimed. |
| Sampling | V: 0x41ec80 distinguishes 11025, 22050 and 44100 Hz. | G: SDL output device rate changes with box-filter conversion from the internal 44100 Hz stream, preserving playback duration. Original device filtering remains unmeasured. |
| Confirmation | V: 0x41a0c0 toggles Acknowledgement; 0x401480 / 0x4016b0 gate category-4 speech. | Gates spoken acknowledgements. It is not evidence for disabling destructive-action dialogs. Existing save/load confirmation stays explicit. |
| Speech style | V: 0x41a18e and handlers select Auto or one of three race voice families; 0x453d50 and other speech callers override the speaker's race only for an explicit family. Private string database identifies Auto, Ross, Aztec and Cathayan. | Family chooses a sound set at runtime; selection/order acknowledgements are dispatched independently of simulation RNG. G: the current Auto caller uses the selected member's side race; mixed-race speaker descriptors remain unresolved. The old profile reader's `unit_speed_mode` name was incorrect. Other original notification callers remain incomplete. |
| Game speed | V: handler 0x41a9e0 stores an update delay; slider range comes from 0x4285a0. At 0x4587eb the simulation loop subtracts elapsed work from that delay and sleeps for the remainder (minimum sleep when overdue). | Configurable real-time update interval, 60..200 ms, default 83 ms. Logical movement, combat, economy, AI, scripts and animation advance per tick, without changing their tick arithmetic. At adequate performance, ticks per second approach 1000 / interval. Rendering remains independent. Automated tick scripts/headless replay keep logical tick scheduling. |
| Scroll speed | V: 0x41a9b0 updates scroll state; 0x428600 bounds the control; 0x465c4d and 0x4698f0 use scroll displacement and acceleration. | G: scales the existing smooth camera's world-pixel velocity, preserving default camera speed. Original frame-cadence/acceleration parity needs measurement. |
| Network protocol | V: 0x41a130 toggles TCP/IP (0) and IPX (1); 0x43a780 applies the preference during provider setup. | Preference retained; original DirectPlay providers unavailable. Existing independent UDP test transport is not original protocol support. |
| Campaign difficulty | G: the task's premise of a global campaign difficulty control remains unverified in this checkout. V: `.mp` Side Difficulty exists as editor metadata; existing documented loader inspection excludes it from runtime side initialization. | No invented AI, income, price, HP or damage multiplier. A real global control, its profile location and AI consumers must be identified before a faithful implementation. |

V: profile load/save at 0x43f070 / 0x43f210 copies the options block. The
speech-style field is at block-relative +0x0c. G: clone preferences use the
per-user `frontend-options.txt` profile, version 2; version 1 remains readable.
Original installation profiles are never modified. Engine campaign progress is
still a separate completion record. Multiple named clone profiles are a missing
front-end dependency, not established by writing preferences to the current user.

## Dependencies and acceptance limits

The checkout already contains a basic options/audio flow, but lacks the complete
original front-end graph described by task 109. This task extends the existing
flow. Exact original layout, provider/lobby navigation, named-player selection,
all speech event mappings, and live campaign difficulty effects remain open.
Do not treat synthetic tests or source inspection as completion of task 124's
full original-game acceptance.

## Maintainer Windows run sheet (needs live run)

Use two independent runs with an unchanged installation, selected player, map,
seed and input route. Keep captures, original profile files and numeric logs
private. Record executable/build identity, setting before/after, navigation and
failure status. Change one control at a time, then restart and read it back.

1. Enumerate every options choice and slider endpoint. Verify both resolutions,
   unavailable-mode behavior, gamma endpoints and neutral gamma. Capture the
   same paused image for RGB comparison, including UI text and cursor.
2. For each game-speed endpoint and the default, time at least 100 completed
   simulation updates under light load. Repeat under load, pause/resume and
   while options are open. Record completed update boundaries, wall time,
   unit distance, weapon reloads, construction, economy events and script timer.
   Do not infer updates from rendered frames or a requested delay.
3. Measure scroll displacement for one second of held arrow and edge input,
   including start/stop acceleration, at every endpoint and both resolutions.
   Repeat across game speeds to separate rendering from simulation cadence.
4. For each audio channel, test mute and midpoint independently; record relative
   gain with an unchanged sound. Measure actual output rate, playback duration
   and pitch for each sampling choice. Check active music through a rate change.
5. Toggle Confirmation and issue selection/move orders; compare acknowledgement
   speech separately from mission notifications. Then exercise quit, overwrite,
   load, restart and player deletion. Record which prompts appear in both states.
6. For each speech style and each speaker race, issue the same selection/order
   and notification. Record the selected family/set privately. Check Auto, mute,
   caption fallback and restart persistence without changing simulation state.
7. Enumerate protocol labels and available providers without a network game;
   reopen options after a restart. Provider support needs its own transport task.
8. Locate the alleged campaign difficulty selector through normal navigation.
   Record every actual label, profile bytes changed, whether it applies before
   campaign start or mid-mission, and whether the value survives restart. If no
   such selector is present, report that result and game edition explicitly.
   Repeat a fixed AI encounter at each actual setting with matching seed/map;
   measure spending/build speed, army composition, attack launch timing, damage,
   HP, income and prices. Separate the global choice from map editor Difficulty.
   Return consumer addresses/observed effects, never decompiled code or dumps.
9. Repeat missions 1..15 through ordinary campaign progression with the verified
   settings and save/resume. Synthetic routes do not establish campaign balance.

## Validation

Baseline and final Release builds pass. Final CTest: 63 passed, six skipped,
zero failures (69 registered). Skips: formats, engine, lockstep_processes,
save_import_private, script_campaign_inventory and parity_rules_private, for
unavailable private data or restricted network process creation. Pre-commit
passes for all tracked files and separately for this new task document.
Changes remain uncommitted because git metadata is read-only.
Focused synthetic checks pass for version-1 migration and version-2 preference
round trips, malformed settings and CLI ranges, original-profile speech-style
round trips, both settings navigation routes, paused input isolation, camera
control persistence, update pacing/phase changes, gamma pixel output and resize,
voice-family overrides/Auto/acknowledgement gating, and audio-rate conversion.
All fixtures are invented. Device-rate checks force SDL's dummy audio driver;
no original installation or namespace escape is used.

Runtime timing remains bounded by the engine's existing catch-up policy. The
speed setting changes wall-clock scheduling only and is deliberately absent
from deterministic simulation hashes and command-log saves. Replaying a save
with another speed resumes the same logical state. Current player preferences
apply to playback speed and presentation; no named-profile difficulty or AI
modifier is claimed.
