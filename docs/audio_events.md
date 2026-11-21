# Audio events of the original (gap analysis)

Read-only static analysis of the corpus `work/decomp/all_full.c` and of constants in the game's own `Aztec.exe`.
V = the code was read directly. G = an inference. Attenuation values are DirectSound
hundredths of a dB (0 = full volume, -4000 = treated as silent).
The original game was not run.

## 1. Sound lists

- V `0x401250`: a sound list id is an element of Snd.xbe group 0x3e8 (1000). The list object is
  {ptr, count = size/4}. Each entry is a u32 WAV id in group 0x460 (1120) (`0x474c80`).
- V: the **caller** makes the random choice: `wav = list[rand() % count]`, using the CRT `_rand`.
  Examples: Marine death `0x433960`, which reads descriptor +0x44, and selection `0x453d50`. `0x401480` itself
  receives a WAV id, not a list.
- V: a list id of zero means no playback (`0x433960` checks the descriptor word before loading the list).
- Descriptor mapping (V, from the descriptor docs, re-checked for Marine at `0x433960`):
  Marine +0x3c spawn, +0x40 hit, +0x44 death. Vehicle and Bird: +0x44 spawn,
  +0x48 hit, +0x4c death, +0x50 attack. Vehicle death also plays fixed list 0x96 (150)
  when the low byte of +0x34 is 0 (`0x407870`). Other kinds: spawn +0x38/+0x3c, engine
  +0x3c/+0x48 with a 10-tick cooldown (`0x460a10`, `0x42c880`). The kind at `0x471740` uses fixed death list 0xf (15).
  Marine death at `0x433960` also plays fixed list 0x96 when type == 0x52 (82).
- Animation frames also carry a list id in field [2] (docs/formats.md, docs/sprites_mapping.md).
- V `0x401480`: pitch randomisation. When the last argument is 2.0 (constant at 0x48f2b0),
  it is replaced by `rand()/65534 - 0.25` (constants 0x48f290 and 0x48f288). The voice frequency is then set to
  `base * (1 + p)`, so the pitch varies by up to about +-25%. Any other value is a fixed pitch offset (0 = unchanged).
  The ambient loop constructor `0x401060` always draws the pitch with rand.
- G, determinism: list choice and pitch both consume the shared CRT `rand()` that the simulation also uses.
  The pitch `rand()` is skipped when the flag at 0x4c1291 is 0 (that flag is set to 1 at `0x465a50` and to 0 at `0x430cb0` and `0x4012e0`).

## 2. Positional sound (`0x401480` effects, `0x4016b0` speech, `0x401330` loop update)

- Pan (V): `x - (camX + viewW/2)`, clamped to +-4000 (`0x401480`, `0x401330`, `0x4016b0`).
- Category, the 2nd argument of `0x401480` (V):
  - 0 = global UI: no attenuation; plays only if the owner argument == the local player (`0x43d200`).
  - 4 = acknowledgement: as category 0, plus the Acknowledgement option at 0x515438 must be on
    (read from the registry near line 20474 of the corpus).
  - Any other value = world sound, with no owner check:
    - Distance `d = |(dx,dy)|` from the view centre (`0x461fe0`, G: Euclidean).
    - Attenuation: 0 below 500 (0x1f4); `(500-d)*4000/300` from 500 to 800 (0x320); -4000 beyond.
      Rejected when attenuation plus the extra volume argument is <= -4000.
    - **Fog gate:** rejected when the local player's fog byte at tile `(x/0x30, y/0x30)` is below 0x21 (33).
      The fog map is the player record at `0x4dd2f4 + 0x690 + p*0x69c`. The check runs only at start, not in the loop update `0x401330`.
- Effects go through `0x474c40` and `0x474c80`. The effects master attenuation (0x515cd8) is added
  to the per-sound value. The sound is then played through `0x476940`.
- Voices (V `0x476940`, `0x4772c0`): **8 hardware slots in total**, shared by effects, speech and music.
  - The voice handle is `slot + 8*generation`.
  - When all 8 slots are busy and the request flag bit0 (steal) is set, the search goes cyclically from the last used slot
    and evicts the first slot whose flag bit1 (protected) is clear. Otherwise the request fails.
  - There is no other priority value; the request is simply rejected.
  - Flags: Marine death passes 1 (steal, not protected), music passes 3 (`0x4752f0`), and ordinary speech passes 3 (`0x4016b0`).
- Ambient loops (V `0x401060`, `0x4011f0`, `0x4011c0`):
  - Pool of 0x28-byte objects (`0x401010`).
  - Started with loop=1.
  - Each update recomputes pan and volume (`0x401330`). When the voice ended or became inaudible, the handle is cleared
    and on the next update the loop restarts through `0x401480` (fog checked again).
  - On destruction the loop fades to -4000 over 300 (`0x474950`, 300/100 = 3 fade steps).
- Speech (V `0x4016b0`): plays only if owner == local player.
  - Category 4 = acknowledgement: gated by the option; flags 3.
  - Category 5 = positional speech:
    - Plays only if the caller's flag is set.
    - Attenuation is 0 below 500, `(500-d)*700/300` up to 800, then -700.
    - Flags 1, so it is not protected.
  - Category 6 = warning: suppressed when the position lies inside the view rectangle.
  - Fallback: if there is no device (`0x4771b0`) or the speech master is <= -4000 (`0x474b80(2)`),
    the caption is shown instead (group 6 text, `0x462760`).
- Speech queue (V `0x474da0`, worker `0x4756dd`): a new request with bit0 set can overwrite the
  last pending entry if that entry is not protected (bit1); otherwise it is appended. Queued speech starts
  only after the active speech ends.

## 3. Voice lines, selection and orders (voice class +0x90)

- Class (V `0x454240`): from the type of the group's representative unit (vfunc +0x6c):
  - 1 for type <= 0x1a (26) or 0x52, 0x53 or 0x55 (82, 83, 85).
  - 2 for 0x1b..0x34 (27..52) or 0x57 (87).
  - 3 otherwise.
- Representative unit (V `0x44fbd0`, used by `0x453fd0` and `0x454090`): the higher `table[type]` at 0x4bc3e8 wins; on a tie, the higher type id wins.
  G: the table looks like unit value/cost (for example type 0x52 = 10000, three entries = 0x7fffffff).
- List id (V, same pattern at every call site): style = option at 0x51543c, or +0x90 when the option is 0.
  - Style 1 or flag 0x4c4f3c == 0: Russian list `0x65 + k` (101+k).
  - Style 2: `0x191 + k` (401+k).
  - Style 3: `0x12d + k` (301+k).
  - G: 0x4c4f3c is set to 1 in `entry` (0x4312e0) and appears constant.
- Event offsets k (V call sites, G meaning):
  - k=0 selection (`0x453d50`, category 4; returns early when vfunc +0x80 == 1).
  - k=1 `0x455800` (G: an order to a target or building).
  - k=2 attack order (`0x44fd02` family, after the alliance check `0x4472e0`).
  - k=3 (`0x451472` to `0x451584` fragments, G: move order).
  - k=6 (`0x454401` family, G: clearing the waypoint queue / releasing a carrier).
  - k=7 and k=8 under-attack warning (`0x455570`, category 6, cooldown field +0x70 = 0x32 (50)).
  - k=0x13..0x1e (19..30) building, production, repair and destruction announcements (`0x459bb0`, `0x45a920`, `0x45d270`, `0x45ea8e`).
- Speech position: group tile `(+0x10, +0x14) * 0x30` (V `0x453d50`).

## 4. Music

- V `0x43a160`: scans music.xbe group 0x460 for tracks 1..32 (0x20) and builds the availability mask
  (bit n-1 = track n).
- V `0x42ec90`: `[Globals] Music=N` (N != 0) gives a mask with bit N only (`0x43a210(0)`, then `0x43a230`).
  Otherwise the mask is `MusicMask & available` (`0x43a210`). Another load path (around line 481 of that function) uses the full availability mask.
- V `0x43a280` (called from `0x4577b0`): when no music voice is active (`0x475240`), it plays the next set
  bit from a cursor 0..31 (0x1f), cyclically, then advances the cursor. The order is ascending with no randomness.
  A single track is not looped by itself; it is started again by this poll.
- V `0x475050` and `0x475180`: music is streamed into a protected looping buffer (`0x4752f0`, flags 3).
  If a track is already playing, the new one waits as pending until the current one ends. There is no crossfade.
- V `0x474950` mixer: channel bits 1 = music (0x515c04), 2 = speech (0x515bc0), 4 = effects (0x515cd8).
  A per-voice handle sets that voice relative to its channel master.
  The fade time argument divided by 100 gives the number of steps; 0 means instant.
  Masters are loaded from the settings at 0x5154c8, 0x5154cc and 0x5154c4 (`0x418e90`, `0x43f070`).
  `0x430cb0` silences all three (mask 7) and then restores them.
  `0x428d20` fades the music voice to -4000 over 500 when `0x429060` returns false.

## Unknowns

- Exact meaning of k=1, 3 and 6 and of the building offsets 0x13..0x1e; the triggers of the `0x459bb0` branches.
- The fade step period (G: about 100 ms per step), and the worker that applies fades.
- The function of countdown 0x49b368 in `0x43a280` (it sets 0x4c12c0).
- Why the category-4 effect calls in `0x45ea00`..`0x45ec45` are used (G: UI or production clicks).
- Whether the 0x4c1291 flag ever differs between machines (affects rand consumption).
- The full meaning of the `0x4bc3e8` table, and whether repeated clicks re-trigger the acknowledgement.
