# Audio implementation

V = a reference rule established by static inspection, or an explicitly identified
engine construction property. G = an approximation, inference or unresolved
integration. No original executable was run for this change. Reference facts come
from [audio_events.md](audio_events.md) and its corrections in
[audio_saves_residual.md](audio_saves_residual.md). Synthetic tests establish
engine behavior, not audible or event parity.

## Mixer and resource contract

| Rule | Evidence and implementation |
|---|---|
| Sound list -> one random WAV member | V: group 1000 contains u32 WAV IDs, group 1120 contains WAVs. Zero list is silent. `SoundBank` and `AudioSystem` retain one-level lookup. |
| Eight shared slots | V: effects, speech and music share eight buffers. `Mixer` reserves one of those slots for music; `active_voices()` counts non-music voices only. |
| Request flags | V: bit 0 allows stealing; bit 1 protects a voice. The mixer first searches for free slots, then, only with bit 0, searches cyclically after the last allocated slot for the first unprotected victim. There is no numeric priority. The existing speech protection parameter also protects ordinary speech. |
| Handles | V: slot plus eight times generation. Each software slot advances its own generation; stale handles cannot stop, change or identify a replacement. Music reserves a slot without exposing a handle. |
| Random pitch | V: sentinel pitch uses `rand()/65534 - 0.25`; fixed pitch is separate. Existing simulation-resolved scene choices use this rate. G: linear PCM interpolation approximates DirectSound. |
| Random ownership | G: animation producers consume the simulation stream before presentation filtering; standalone/queued UI choices use their separate seed-one stream. Native caller/thread/interleaving equivalence remains incomplete. See [RNG implementation](rng_and_tick_order_implementation.md). |

`Mixer::play` accepts request flags after the pitch rate. Defaults retain stealing
for existing effect callers. Music and ordinary speech remain protected, while
category-5 speech is stealable. The speech queue replaces an unprotected pending
tail only when the incoming request permits replacement; active queued speech
finishes first. The former invented 32-request queue limit has been removed.
The direct `play_speech` WAV API still explicitly replaces active speech; event
producers use the queue instead.

## Event dispatch

The resource membership of every list remains user-supplied runtime data. No WAV,
caption, descriptor dump or executable-derived implementation is included.

| Event | Reference list rule | Engine integration |
|---|---|---|
| Selection | V: native class 1 for types <=26 or 82/83/85; class 2 for 27..52 or 87; class 3 otherwise. Base lists 101/401/301. Category 4. | Uses type, local ownership and acknowledgement gate, with pan. A group whose eligible speakers all select the same list needs no rank lookup. Mixed-class Auto selection is suppressed pending the representative-ranking table. |
| Attack order | V: same family, offset 2, category 4, after alliance check. | Explicit attack requests against live hostile targets queue 103/403/303. |
| Follow team | V: offset 3, category 4. Residual notes correct the older move-order interpretation. | `FollowTeam` with the supported branch queues 104/404/304. |
| Stop following | V: offset 6, category 4, when caller requests speech. | Player Stop for an already following speaker queues 107/407/307. Ordinary Stop has no invented cue. G: the engine extension is used for this native route-clearing operation. |
| Ordinary Move / AttackMove | No verified offset in the supplied evidence. | Silent by default. |
| Under attack | V: offsets 7/8, category 6; native cooldown 50. | Category gate API exists; tactic warning producers and offset branch choice are still missing. No guessed warning is emitted. |
| Main hall / other building completion | V: offsets 19/27, with exclusions 18/40/70 for the latter. | Dedicated BuildThink completion notifications are missing; animation-frame effects still work. |
| Unit ready | V: offset 26, when the produced unit joins the town. | Dedicated town notification is missing. |
| Queue idle / insufficient resources / production start | Sites use offsets 22/21/29. G: English trigger names and exact transition semantics. | No snapshot-based guess is enabled. |
| Repair, capture and upgrade announcements | Sites use offsets 20/30, 24/25 and 31; meanings include G labels in the source notes. | Require lifecycle publishers and verified branch semantics. |
| Town-controller announcement / daylight jingle | V: offset 28 and race/time-dependent lists 172/170/171, extra -500, looping with fade. | Controller transition and descriptor daylight-list integration are missing. |
| Spawn / hit / death | V: descriptor words by kind; fixed Zeppelin death list 15, vehicle explosion list 150, type-82 extra death list 150. | Descriptor callbacks are missing. Frame-driven death effects are not substitutes for these sounds. |
| Attack effects / ambient loops | V: animation frame field [2] and loop headers; some kinds also have descriptor sounds. | Existing scene dispatcher handles frame entries and loops. Native producer ordering, descriptor-only effects and engine-loop cooldowns remain G. |
| UI clicks / script audio | No general mapping established here. | No universal click WAV or script sound ID is invented. |

`speech_list_id` centralizes explicit-style and town-race conversion. Style 2
selects the Aztec family (+300 from Ross), style 3 selects Cathayan (+200), and
Auto uses native unit class or town race+1. This fixes the previous generic API's
Aztec/Cathayan inversion. Caption lookup still uses the selected WAV ID.

`--audio-event-fallbacks` explicitly enables G policies: the first eligible member
for mixed-class selection, generic offset-3 cues for otherwise unmapped basic
orders, and renderer-based fog admission/loop removal. It is off by default and
is a session option, not part of persisted front-end preferences. It does not
restore the incorrect speech-family mapping. Existing amplitude, subtitle,
sampling and audio-disable options remain independent.

Order dispatch is tied to newly submitted local UI commands. G: it is not yet a
post-acceptance simulation notification and does not cover script/AI orders,
all native tactic branches, repeated selection clicks or representative group
coordinates. The mixed-class ranking table and vfunc selection suppression are
not exposed by current engine metadata. These are dependencies for complete V
selection/order integration; neither price nor type ordering is substituted for
the missing table.

## Position, volume and speech categories

V: world effects use distance attenuation from the view centre: full below 500,
linear in hundredths of a decibel to -4000 at 800, then rejected. Pan is horizontal
displacement clamped to +/-4000. `audio_fog_visible` reads the local player's tile
brightness and requires >=33, independently of ownership, building memory and
the UI fog switch. Loaded darkness is converted to engine brightness. Missing or
out-of-range fog data is rejected in this strict path. G: that representation
conversion does not certify native light-map equality.

V: fog is checked at loop start/restart, not during an active loop's gain update.
The scene dispatcher now continues an existing loop through fog changes, while
blocking an invisible restart. Distance and removal still stop it. G: destruction
currently stops immediately; the native three-step fade is not implemented, and
the worker's wall-clock step period remains unresolved. No fade duration was
invented from the static argument.

`queue_speech_at` implements the V local-owner gate, acknowledgement option for
category 4, caller-enable gate and distance attenuation capped at -700 for category
5, and half-open on-screen rectangle suppression for category 6. Other categories
retain local-only, protected speech. This API is ready for future tactic/town
publishers; it does not itself infer a warning or production event.

V: music, speech and effects have independent masters. `set_attenuations` accepts
original hundredths-of-decibel values, clamps positive values to full volume and
treats <=-4000 as silence. `set_volumes` preserves the existing explicit normalized
amplitude UI/CLI controls (`--volume`, `--sfx-volume`, `--music-volume`,
`--speech-volume`); zero mutes that channel and live changes affect active voices. Amplitudes <=0.01
reach the native -4000 silence floor. New muted effects/loops/music do not consume
a slot; unmuting permits the next music poll to retry. Positional effects also
reject when distance attenuation plus the master reaches that floor.
G: the UI's percentage curve/default amplitudes are engine choices, and original
profile attenuation words are not automatically imported into these controls.
The attenuation API does not implement native fades or per-handle master updates.

Acknowledgement suppression affects category 4, not ordinary announcements.
Captions support muted/no-device/missing-WAV speech, CP1250 conversion, the
subtitle option and a six-second engine presentation lifetime (G). The supplied
fallback rule is V; caption polling/lifetime equivalence is not established.
`--no-audio` suppresses the backend; 11025/22050/44100 sampling choices preserve
the internal presentation clock. G: SDL resampling is not native device parity.

## Mission and menu music

V: runtime availability is restricted to track IDs 1..32. Nonzero mission
`Music=N` overrides `MusicMask`, clamps above 32 and intersects availability;
otherwise `MusicMask & available` applies. The next allowed bit is selected in
ascending cyclic order without RNG or mission-name salt. An active track finishes
before a playlist change takes effect. A one-track playlist restarts through the
presentation poll rather than looping the whole track in software. A request
rejected by eight protected slots is retried by that poll after a slot frees.

V: the cursor resets when the music bank is initialized, not merely when its mask
changes. The engine now retains its 0..31 position through mask changes and scene
stops. G: whether every native mission-entry path reinitializes the bank remains
open in the residual notes. Engine mission teardown explicitly stops music and
speech; a new mission applies its own globals. No original save restores voices
or cursor. Duplicate CD-bank IDs retain the existing first-readable-file policy;
CD switching fidelity remains unresolved.

The engine menu remains silent after mission teardown. G: the native teardown
clears the mask, but a complete menu/briefing music lifecycle was not established
by these documents. No menu track is invented. There are no mission-specific
hardcoded track numbers or game-derived playlists in the tests.

## Validation and remaining dependencies

`tests/test_audio.cpp` builds invented XBE lists, WAV samples and animation records
in a temporary directory. It checks shared saturation, non-stealing rejection,
protected effects/speech/music, cyclic victims, stale handles, family overrides,
command cues, category gates/pan, fog threshold, loop continuation/restart,
attenuation/mute and cursor retention/deferred music. Existing muted/rendered
1,200-tick hash and replay tests remain in place. `test_campaign_ui.cpp` checks
option parsing. The full independent Release build, CTest suite and pre-commit
are the required gates; skips are not game-data verification.

This is a partial integration of the known rules. The public checkout does not
provide complete normalized sound descriptor initializers, the representative
ranking table or dedicated town/tactic audio event publishers. Those dependencies
must be integrated before descriptor callbacks, warning cooldown/offset branches,
production announcements and daylight jingles can be implemented faithfully.
Task 122 remains queued; no completion of its live-event requirements is assumed.
Neither private static table extraction nor renderer snapshots can establish the
missing initialized records and transition contracts. No private data is promoted
to public fixtures.

A maintainer's later native comparison should record list/WAV/category/flags for
selection, hostile attack, follow/stop-following, offscreen attack warnings,
production and building completion, followed by mission entry/exit/menu and
music-mask changes. Repeat with each voice style, acknowledgement disabled,
individual masters muted and all eight slots occupied. Measure the fade step
period and validate final descriptor records after initialization. Keep captures
and resources private, follow [EXPERIMENTS.md](EXPERIMENTS.md), and report each
unobserved trigger as unobserved. Codex did not run the original executable or
claim reference parity in this work.

Validation in this worktree: baseline and final Release builds passed; both full
CTest runs reported no failures across 82 registered entries (76 passed, six
skipped for unavailable private data or restricted UDP process setup). Pre-commit
passed for all tracked files and separately for this new document. All new audio
fixtures were synthetic. No live game-data or native audio parity run was made.
