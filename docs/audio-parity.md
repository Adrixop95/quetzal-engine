# Audio events, voices and controls (task 122)

Status: **needs live run; partial implementation**. The original was not run.
V means read in private original instructions/data, not measured playback.
G means inference or independent engine policy. Synthetic checks establish
engine behavior only. No original audio, captions, scripts, captures, numeric
dumps or executable-derived source are included here.

The baseline Release build, runnable CTest entries and pre-commit gates passed
before editing. Git metadata is read-only; changes remain uncommitted. Historical
audio notes are in [audio.md](audio.md); the changes below supersede its claims
that selection has no application producer and the mixer uses 32 effect voices.

The updated Release build and full CTest suite passed: 63 passed, six skipped
(private-data checks and restricted UDP process creation). All tracked-file and
new-document pre-commit gates passed. No private-data playback or original-game
comparison was performed.

## Evidence and remaining coverage

Private research used instruction comments in `work/gen/gen_*.c`, with the local
decompilation used only to locate functions. These are research inputs, never
implementation templates. Resource members remain in the user's archives and
are resolved at runtime. Addresses below identify research sites, not copied code.

| Area | Evidence | Clone and remaining work |
| --- | --- | --- |
| Selection | V: `f_00453d50` requests an acknowledgement from a sound set chosen by speech style; `f_00454240` chooses native style from the group's representative type. Ordinary Russian, Aztec and Chinese types have different families. Abdula, Catapult and Zeppelin2 use Russian; Hopcannon uses Aztec. Ownership does not determine native speech. | Implemented for changed mouse selections of owned living mobile units. G: the first selected mobile object represents a mixed selection. Original group representative ranking, repeat-click behavior, town themes and keyboard selection still need alignment. |
| Orders | V: other group state transitions have distinct voice call sites, including private functions around `00451500`, `00452b00`, `00453400` and `00453700`. Selection speech alone cannot identify accepted, rejected or autonomous orders. | Still incomplete. Do not play the selection set for every submitted command. The exact transition/command acceptance contract must be audited before routing these events. |
| Attack/death | V: frame entry and header sounds are read from animation records; separate descriptor sound requests also exist, including weapon impact at `0044bdxx` and vehicle motion branches. | Existing body/turret/effect/decoration dispatcher remains. Descriptor-only impact/motion sounds are not covered by an animation-only claim; descriptor loading and matching simulation events remain open. |
| Construction/completion | V: `f_00459bb0` has separate announcement paths for construction, production and completion; construction animation seeking is distinct from timed frame entry. | Existing construction/operation/destruction animation requests remain. Announcement state transitions and exceptional products still need mapping; no guessed completion sound is emitted for every spawned object. |
| UI | V: the return-to-game handler `f_004577b0` requests a direct WAV before returning to gameplay. | Resume button now emits that request with UI provenance. Generic menu/panel clicks remain unverified; this cue is not used as a universal click. |
| Under attack | V: `f_00455570` has friendly/attacker eligibility checks, group state gates, a countdown and a category that suppresses warnings when their position is inside the view. | The frame-sound dispatcher does not implement this warning policy. Exact attacker/town eligibility, countdown updates and on-screen boundaries remain open. |
| Other alerts | V: repair/destruction announcements in `f_0045d270` and town/event calls pass through the speech dispatcher rather than body animation. | Mission event, repair, destruction and town announcement producers remain incomplete. |
| Music | V: mission loading reads explicit Music or MusicMask; `f_0043a160`, `f_0043a210`, `f_0043a230`, `f_0043a280` intersect availability and advance through allowed tracks. | Existing runtime mission playlist, ascending order, explicit override and unreadable-track checks remain. Campaign configuration is described in audio.md; exact native transition delays and disc behavior need measurement. |
| Script sound actions | V: the inspected campaign scripts contain no explicit sound/music/speech action tokens. This is an inventory observation, not evidence that every script-triggered game event is silent. | The current parser has no explicit sound action. PrintString is text; it is not treated as a guessed speech ID. State changes caused by scripts still need the corresponding event producers. Custom-script sound syntax remains unverified. |

G: changing selection queues one voice, independently of simulation RNG, hashes,
commands and saves. Selection pan uses the existing camera-centre calculation,
with no distance cut-off for acknowledgement speech. Exact group centres and
original member RNG are not established. Speech style 0 chooses native type;
1/2/3 force Russian/Aztec/Chinese. The resource set members are never embedded.

## Limits, priorities and mixing

V: `f_00476940` and `f_004772c0` use eight shared playback buffers. A new
stealing-enabled request searches cyclically for an unprotected victim when no
free buffer exists. A protected buffer cannot be stolen this way. Music uses a
protected buffer (`f_004752f0`); acknowledgement speech requests protection too
(`f_004016b0`). This differs from the old clone policy of 32 effect voices and
stealing the one nearest completion.

The independent mixer now shares an eight-slot budget among effects, speech and
music, protects flagged speech/music, and steals eligible voices cyclically.
Loops and unprotected casual speech may be stolen. G: the exact low-level buffer reclamation timing is
represented by sample completion in the software mixer. Streaming music occupies
one logical slot; native DirectSound streaming/caching is not emulated. Failed
allocations produce neither accepted playback events nor retained scene loops.

V: `f_00474da0` preserves protected pending speech and can replace an unprotected
pending item. The protection flag also determines whether active speech may be
stolen. The worker around `004756dd` takes queued speech only after the
active utterance finishes. `queue_speech_set` provides this behavior for scene/UI
producers. G: the clone bounds its pending queue at 32, drains at most one request
per presentation update, and drops a queued acknowledgement if disabled before
it starts. These are explicit engine bounds/scheduling, not original limits.
The older direct `play_speech` API remains an immediate replacement API for tools;
application selection uses the queue. Log tick is playback-start tick; the queued
request retains its trigger and set identity. It is not a sample-delivery log.

V: `f_00474950` independently adjusts music, speech and other buffers. Original
settings are attenuation values and include timed fades, not raw PCM amplitude
percentages. The clone already mixes all three channels independently and updates
active voices live. G: its existing 0..1 CLI/UI gain remains linear amplitude;
original slider taper, default levels, fades and mute boundaries are **not**
claimed equivalent. Queued speech also uses the speech channel. Caption lifetime,
always-show subtitles, missing-WAV fallback and muted-utterance handling retain
the existing engine policies documented in audio.md.

V: the Frequency option selects 11025, 22050 or 44100 Hz (`00419fxx`,
`f_004767a0`). The clone now exposes all three rates in the live Audio screen and
`--sampling-rate`, persists them alongside `--speech-style`, and reads old
`QUETZAL_OPTIONS_1` files with native style/44100 defaults. New files use version 2.
No original profile/registry is written. G: internal mixing stays at 44100 Hz;
SDL converts the final stereo output to the selected device frequency. A live
rate change reopens the device and drops converted pending samples while retaining
voice positions. Converter filtering, buffering/latency and device failure
recovery need native checks; matching option values is not waveform parity.

## Synthetic verification

`audio_events` uses invented XBE/WAV/animation records, an event recording sink
and `init(..., false)`. It opens no real audio device. Added cases cover:

- Eight shared slots, protected music/speech, cyclic stealing instead of shortest
  remaining duration, all-protected rejection and freeing the music slot.
- Rejected playback producing no events or stale loop entries.
- Protected speech ordering, replaceable pending speech, no active interruption,
  retained pan/provenance, acknowledgement suppression and queue cleanup.
- Native type families, special units, forced speech styles and ownership checks.
- Recording the output converter at all three sample rates, changing rate without
  new playback requests, rejecting unsupported rates and retaining active loops.
- Existing exact synthetic effect/music/speech mixing assertions, plus 1,200-tick
  audio-on/off simulation hash equality with queued selection and rate changes,
  followed by command-log replay equality.

`campaign_ui` covers option persistence/migration, malformed CLI values, live
style/rate controls and fitting all eight rows on the supported test surfaces.
Its modal input scenario closes Audio with Escape so it does not depend on a
particular Back-button coordinate.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
pre-commit run --files docs/audio-parity.md
```

## Native maintainer run sheet

No original process was launched by Codex. The shared L0 runner exists in this
checkout, but its presence does not supply an audited original audio identity,
trigger, speech queue or native music observer. Task 64's audio-alignment needs
and the L6 observer/mapping work remain dependencies. Complete independent mixer,
control and synthetic work now; keep paired original event comparisons pending.

On the Windows installation, use a separate private copy of the installation and
a fresh private clone profile. Start with the original settings/profile copied
into that private installation; record language, executable revision, mission,
viewing side, chosen units, camera, speed, speech style and three volume values.
Never run an experiment against the shared writable installation. Retain native
captures and worksheet details privately. No original audio goes into git.

After the normal native build from SETUP.md, the clone command is:

```powershell
$privateGame = 'D:\PrivateAudioProbe\game'
$env:AZTEC_USER_DIR = 'D:\PrivateAudioProbe\clone-profile'
& '.\build_msvc\Release\aztec_engine.exe' --game $privateGame 1 `
  --speech-style 0 --sampling-rate 44100 `
  --audio-log 'D:\PrivateAudioProbe\clone-mission-1.jsonl'
```

Only the maintainer launches the original installed game from the private copy.
Do not invent an original `--audio-log` switch or populate an expected stream
from the clone. Until a native observer is available, record observed utterances,
event order and timing separately; an unidentified sound remains unidentified.
Manual listening cannot establish the archive ID, completed update tick or voice
allocation flags. Those fields need the audited observer. Two independent
original runs and two clone runs are required per case, with matching controls.

| Case | Actions and measurements to return |
| --- | --- |
| Selection by type | Select every mobile type alone, repeatedly, through drag selection and control groups; include all special units and mixed groups in both orders. Record representative type, set/member when observed, pan, suppression and repeats. Repeat with captured/gifted units. |
| Orders | Move, attack, attack-move, stop, follow, board/unload; include rejected/out-of-range orders and autonomous acquisition. Record acceptance versus voice start, duplication, grouping and interruption. |
| Combat | Isolate melee, projectile, impact, turret and each death kind. Count simultaneous requests while offscreen, hidden and at the attenuation boundaries. Separate descriptor requests from animation requests. |
| Buildings | Start/pause/cancel/finish each product class, train a unit, upgrade a hall, damage/repair/destroy a building and complete a queue with exceptions. Record construction loops, completion announcements and cancellations. |
| UI | Resume by button/Escape, select menu and panel controls, disabled clicks and modal transitions. Identify distinct sounds and exact triggers. |
| Alerts | Attack own units/towns with allied/enemy attackers, both inside and outside the view. Reattack before/after warning expiry; record cooldown, grouping, priority and minimap alert relation. |
| Mission events | For every mission 1 through 15, collect startup, objectives, reinforcements, transport actions, victory and defeat. Record script trigger/action provenance only when actually observed. Leave unreachable events pending. |
| Music | For every mission 1 through 15, record starting track, natural transitions, repeat order, disc/archive availability, empty mask, pause, mission exit/restart and load. Run long enough for actual ends; do not infer playback from MusicMask. |
| Speech/style | All four styles, acknowledgements off/on, effects muted with speech audible and vice versa, missing/inaudible speech, captions, many simultaneous announcements and selection during speech. Measure protected queue retention and low-priority replacement. |
| Volume/sampling | Sweep every slider including mute/full, changing while each channel plays. Return setting-to-attenuation observations and fade times. Repeat each sampling rate with music, speech and effects together, switch live, and test missing device/reconnect. |

For namespace-capable Linux reference collection, follow EXPERIMENTS.md exactly:
separate private game directories with immutable asset symlinks and ordinary
mutable Maps/Saves, both dummy SDL drivers, hard timeout and `unshare -rn`.
Never bypass isolation after namespace failure. A native Windows listening test
is distinct from a lifted-runtime headless comparison. Missing observer,
identity mapping, tick alignment or failed namespace setup means **needs live
run**, even when every synthetic check passes.
