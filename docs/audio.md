# Engine audio and parity evidence

The engine reads the user's `Res/Snd.xbe`, `Res/Units.xbe` and
`Music/music.xbe` / `Music/Music_cd2.xbe` at runtime. This document contains
format facts and engine behavior, without audio data. See [formats.md](formats.md#sound-srccoreaudio)
and [sprites_mapping.md](sprites_mapping.md) for the underlying resource facts.

## Event mapping

A sound-set number is an index into Snd.xbe group `0x3e8`. Its members are
WAV numbers in group `0x460`; these are different namespaces. The engine
chooses one member with a presentation-only PRNG, independently of simulation
RNG. Matching the original random member sequence is not established.

| Event | Resource mapping | Engine behavior and evidence |
| --- | --- | --- |
| Unit movement / attack / death | Selected body animation, frame field `[2]` | Dispatch on frame entry using durations, including consecutive frames sharing the same sprite. Synthetic coverage; original timing still depends on simulation parity. |
| Turret attack | Secondary animation selected by `turret_sequence`, frame `[2]` | Uses the independent attack clock; synthetic Headtruck coverage. |
| Building construction | HP-selected frame in body slot 0 | Seeking construction frames does not trigger timed frame cues. |
| Building completion / operation | Built body slot 1, frame `[2]` and preceding header `[2]` | Frame cue on entry; header sound persists as a positional loop. Synthetic coverage. |
| Building destruction | Destruction animation 114/115/116, variant slot | Frame cues follow the chosen destruction sequence. Synthetic radius/variant coverage; missing simulation inputs remain a limitation. |
| Projectile, impact, damage fire | Effect's animation/slot, frame `[2]` and header `[2]` | Uses effect age and loop flag. Building fire animations 154/155/156 are distinct from damage sound sets 151/152/153. |
| Ambient operation / movement | Header record preceding the selected body or turret sequence, field `[2]` | One loop per object/channel. Updates positional gain; stops when no longer requested, hidden, out of range, removed, replay-loaded or leaving the mission. Synthetic lifecycle coverage. |
| Unit selection / order acknowledgement | Separate speech calls, not implied by animation-frame sounds | Speech WAV/caption playback and acknowledgement suppression now have synthetic checks. **Event-to-WAV/type/race routing is still missing** in the UI and command paths; no guessed IDs are dispatched. |
| Building announcements / warnings | Separate speech/event calls | **Incomplete.** Animation sounds above work; completion, damage, repair and destruction announcements are not fully mapped or dispatched. |
| Mission music | `.mp` `[Globals] Music=N`, otherwise `MusicMask & available` | An explicit track overrides the mask. Allowed tracks start at the lowest number and cycle in ascending order, as documented in formats.md. Synthetic multi-track coverage. |

Sound sets may contain speech WAVs, but animation sounds do not identify
selection/order voices. `play_speech(sound_id, is_acknowledgement)` accepts an
explicit WAV ID and reads its group `0x6` caption, converting CP1250 to UTF-8.
The speech channel uses its own live gain; muting effects does not mute speech.
Acknowledgement suppression skips both the voice request and its caption.
Captions work with a missing WAV, muted speech or no device. The subtitles option
also displays captions alongside audible requests. Replacement by the next
utterance and six seconds of presentation time per caption are engine policies
(P), not measured original priority, cooldown or caption duration. Captions clear
on expiry, disabling subtitles, load and mission exit; they never enter World,
commands, saves, hashes or simulation RNG. There is currently **no application
producer with an evidenced speech ID**, so this API does not establish campaign
voice coverage or briefing narration. Event/type/race mappings remain open.
`[Sounds]` in mission files is documented as editor-only; it is not used to invent
mission ambient events.

`--volume V` sets effects, music and speech together. `--sfx-volume V`,
`--music-volume V` and `--speech-volume V` override individual gains in argument
order; values must be finite and within 0..1. `--no-acknowledgement` and
`--no-subtitles` suppress confirmations or captions. The front-end Options/Audio
screen and paused mission Audio screen adjust the same settings during a
session. Settings do not alter simulation tick processing or data; they are not
written into the original player's files. `--no-audio` still disables the whole
playback dispatcher, including captions.

## Campaign music configuration

Read-only inspection of the local mission INI files on 2026-10-28 found these
values; this is configuration evidence, not observed playback. Both local music
archives together advertise tracks 1 through 12 (some tracks appear on both
discs). The resulting playlist after intersecting availability is:

| Mission | Music | MusicMask (decimal) | Track order |
| --- | --- | --- | --- |
| 1 | 0 | 3 | 1, 2 |
| 2 | 0 | 11 | 1, 2, 4 |
| 3 | 0 | 72 | 4, 7 |
| 5 | 0 | 66 | 2, 7 |
| 4, 6 through 15 | 0 | 62463 | 1 through 10 |

Custom missions use their own fields and available archives; the engine does
not hard-code this campaign table. Bits for tracks 13 through 16 in mask 62463
are ignored when those tracks are absent.

## Dispatch and mixing

`emit_scene_audio` runs after each simulation tick, rather than each rendered
frame. Animation lookup supplies frame-entry metadata based on elapsed duration;
held frames and held Marine corpses do not repeatedly play their entry sound.
Body and attack clocks match the separate visual clocks. Construction seeks
are silent. An initial dispatch before the first simulation advance captures tick-zero
frame cues and ambient loops. `--pre-ticks` deliberately skips historical audio.

The listener is the camera centre. Pan and distance gain use the existing rules:
no distance attenuation below 500 pixels, attenuation through 800 pixels, then
silence. The application supplies the view's fog visibility predicate. This
closes unconditional playback from hidden objects, but inherits the renderer's
fog approximations and lighting update schedule; exact reference fog parity is
not established. Synthetic tests control visibility explicitly.

Loops retain their chosen WAV while audible, and restart after returning from
silence or after voice stealing. Effect header loops share a channel for a
source/animation/slot combination. The mixer has 32 effect voices, converts PCM
to stereo 44100 Hz and mixes music separately. These channel/voice policies are
engine policies, not verified original limits. Pausing simulation leaves active
loops and music playing; pause-menu reference behavior remains unverified.

Music can play without an effects bank. Starting another mission stops the old
stream even when its allowed mask is empty. Stopping music clears any pending
end notification so `update()` cannot accidentally restart it. Unreadable tracks
are skipped within a bounded pass over allowed tracks. No random mission-name
salt changes the starting track.

## Headless event logs and checks

`--audio-log FILE` writes JSONL accepted playback requests:

```json
{"seq":0,"tick":7,"kind":"sound","source":0,"set":1,"sound":1,"trigger":"combat"}
```

Kinds are `sound`, `loop_start`, `loop_stop`, `music`, `music_stop`,
`speech` and `subtitle`. Speech/subtitle `sound` is the WAV/caption ID;
neither record contains caption text. For music, `sound` is the track number.
Direct WAV calls have set 0. Object loop sources encode object ID and channel;
effect sources have bit 63 set and encode source ID, animation and slot.
One-shot sources are currently 0. Music starts and transitions use the caller's
current world tick; scene events use the dispatch tick. Each stream uses
consecutive sequence indices. Triggers describe engine dispatch contexts, and
unknown provenance remains `unknown`; loop stops retain the start context.
`building` covers construction/structure animation requests; `production` is
reserved for explicit production announcements. The new label does not supply
an announcement dispatcher or an evidenced WAV mapping.
These logs record requests, not samples delivered by the device. Keep private-bank
logs under ignored `work/parity/`. `--no-audio` disables playback and produces
no playback events. The JSON example above uses invented values.

The reference shim's `RT_AUDIO_LOG` and the L6 normalizer/comparator are described
in [parity.md](parity.md#l6-audio-event-comparison-2026-10-29). Reference buffers
still need audited resource/trigger mappings; raw traces cannot establish parity.

The `audio_events` CTest creates invented XBE/WAV/Anim records in a temporary
directory, uses the application's dispatcher and an AudioSystem without an SDL
device, asserts event kinds/sets/IDs and tick boundaries, and prints the log.
It checks frame holds, reused images, body/turret/death/building/effect paths,
visibility/distance loop cleanup, mixer looping/gain/stopping, mission music
order/override/stopping and music without Snd.xbe. It also checks speech gain
independent of effects, muted/missing-WAV/no-device captions, acknowledgement
suppression, utterance replacement, expiry and scene cleanup. A 1,200-tick
movement/command scenario compares the complete simulation hash at every tick
with audio dispatch disabled versus changing gains, acknowledgements, captions,
visibility, listener position and mixer cadence. Command-log replay must finish
with the same hash. This verifies independence for that synthetic scenario;
V03's campaign and cross-platform requirements remain open. No game installation or
executable is needed:

```sh
cmake --build build --parallel 2
ctest --test-dir build -R audio_events -V
```

On 2026-10-28 the Release baseline and updated builds succeeded. The complete
CTest run retained the baseline `lockstep_processes` failure because UDP sockets
are denied in the sandbox; the new audio check passed. The default private-data
checks skipped. An explicit read-only format check against the local original
banks resolved all 275 sound-set entries and reported 367 readable WAVs plus one
bad WAV; the existing check still returned success. This is bank-reader coverage,
not proof that every trigger plays.

A private game copy was prepared under `work/audio-game` with read-only resource
symlinks and ordinary Maps/Saves directories. The dummy-SDL reference attempt
inside `timeout ... unshare -rn` failed at `ip link set lo up` with
`Cannot open netlink socket: Operation not permitted`. No run outside isolation
was substituted. Consequently reference record/replay, per-mission playback,
voice/announcement triggering and audible original/engine comparison remain
**unverified**. Rerun those probes following [EXPERIMENTS.md](EXPERIMENTS.md) on
a host that permits namespace networking before claiming full audio parity.

## V15 validation update (2026-10-28, partial)

The current Release build, synthetic audio and campaign UI checks, and all
runnable CTest entries pass. Explicit private format and engine checks passed
inside `unshare -rn` with both dummy SDL drivers and a private installation copy.
The 15-mission script inventory still reports 19 unsupported transport action
sites, so required event producers are not complete. A mission 1 dummy-SDL
capture with music/event logging succeeded; this is a smoke check, not an
acceptance run or audible comparison. The original-save check skipped because
this private copy contains no original saves.

The bounded reference attempt again failed at namespace interface setup before
launch (`Cannot open netlink socket: Operation not permitted`). Its script,
configuration, command and state log destinations are under ignored
`work/v14-v15-probe/`; no record/replay or original audio observation was
obtained. Selection/order speech routing, building announcements and warning
policies, exact fog attenuation, original voice interruption and the required
per-mission event matrix remain unresolved. V15 is incomplete.

## L6 iteration (2026-11-01, synthetic evidence)

The provider now shares decoration-frame selection between rendering and audio.
This covers damage-fire and vehicle-death fallback decorations; simulation-owned
fire already uses the effect dispatcher and suppresses the fallback, following
the renderer's existing ownership rule. Body and turret requests remain available.
Decoration channels keep their positions after an earlier decoration finishes,
so cleanup stops the finished loop rather than the surviving one. This change
reuses existing engine animation selection; it adds no reference-derived mapping.

Three regression cases first failed and then passed: omitted fallback requests,
incorrect loop-source cleanup, and duplicate fire when a simulation effect owns
it. The invented event-log scenario compares 37 requests through task 45's
normalizer/comparator and also requires exact ticks and loop source lifetimes.
It covers construction silence, damaged/completed buildings, healing, visibility,
distance, removal, held frames and two death decorations with different lifetimes.
The simulation-independence scenario now includes damaged and dying objects,
retaining per-tick hash equality for 1,200 ticks and command-log replay equality.

The isolated reference probe under `work/parity/l6-iterate-reference/` failed
at namespace loopback setup before launch. The clone completed a bounded
mission smoke with a nonempty request log, and a separate private-copy run with
audio disabled reached the same final simulation hash. These are integration
checks, not reference sound observations or audible comparison. Selection/order
speech, UI cues, production/completion announcements, generic effect provenance,
original music transitions and all seven live trigger-class comparisons remain
unverified. The task 45 comparator is present; audited reference identity/trigger
and music observers plus synchronized paired captures remain dependencies.

## Task 76 follow-up (2026-11-05, synthetic evidence)

**Status: needs live run.** Task 64's shared fixed-tick observer/input contract
is absent from this checkout. Independent work adds a distinct `production`
trigger, all-class coverage enforcement, stricter reference row validation,
and command/state recording plus replay controls to the private capture probe.
The synthetic mixer regression changes gains on already playing effect, speech
and music voices, mutes/unmutes them independently, and checks exact invented
sample sums without restarting voices or emitting additional playback requests.
No synthetic samples or private event captures are saved in the repository.

The existing speech/caption API, acknowledgement settings, live controls, music
order and positional loop policies remain engine behavior with synthetic
coverage. Original settings semantics, application speech/announcement routing,
reference music observation and missing/misordered original events cannot be
resolved without the dependency and audited live evidence. Use the exact commands
and evidence gates in [EXPERIMENTS.md](EXPERIMENTS.md#l6-live-audio-run-sheet-task-76-needs-live-run).
