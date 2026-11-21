# Audio and save residuals (original Aztec.exe)

Read-only static analysis of `work/decomp/all_full.c`, objdump of `game/Aztec.exe`, and new
Ghidra output `work/gaps/dec_0045ea00.c` (covers 0x45ea00, 0x45ed20, 0x451440).
V = read in code or data. G = inference. Numbers are hex (decimal). The game was not run.
This builds on `audio_events.md`, `campaign_menus_saves.md`, `docs/savegame.md`,
`docs/descriptor_*.md`; facts already there are not repeated.

## 1. Sound resource layout (summary, with corrections)

- V `Res/Snd.xbe` group 0x3e8 (1000) = sound lists (u32 arrays of WAV ids, count = size/4,
  `0x401250`). Group 0x460 (1120) = WAVs. List id -> WAV id is one level only: the caller picks
  `list[rand() % count]`. List id 0 = silent. Texts for captions: group 6.
- V music (`0x43a160`): table `0x4c5594[0..31]` = 1<<i; availability mask `0x4c5614` is built by
  probing music.xbe group 0x460 elements 0x20 (32) down to 1, so bit n-1 = track n.
  Active mask `0x4c5618`, cursor `0x4c561c` (reset to 0 only in `0x43a160`).
- V `0x42ec90` (mission load, both new and save-load paths): `Music=N` != 0 -> `0x43a210(0)` then
  `0x43a230(N)` (N clamped to 0x20, mask = (1<<(N-1)) & avail). Otherwise `MusicMask & avail`.
  A second path in the same function sets mask = full availability (`0x43a1f0` -> `0x43a210`).
- V `0x43a260` = current track getter (cursor+1, or 0 when mask empty).
- V voice list ids: style s = option `0x51543c`, else unit group `+0x90`, else town `+0x164`.
  Units: s=1 or `0x4c4f3c`==0 -> 0x65+k (101+k), s=2 -> 0x191+k (401+k), s=3 -> 0x12d+k (301+k).
  Towns use a different encoding of `+0x164`: 0 -> Russian 0x65+k, 1 -> 0x191+k, other -> 0x12d+k
  (V `0x45e988`..`0x45e9c7`, same pattern in `0x459bb0`, `0x45a920`, `0x45d270`).

## 2. Voice offsets k (all call sites found by literal scan, V for site, G for meaning)

| k | Lists (Ru/Az/Ch) | Site | Category | Meaning |
|---|---|---|---|---|
| 0 | 101/401/301 | `0x453d50` | 4 | selection (known) |
| 1 | 102/402/302 | `0x455800` = Tactic vtable `0x48fbd4` slot +0x12c; Town override `0x45ed20` (vtable `0x494648`) calls it with a unit | 1 | G: member removed/lost from group. Skipped if vfunc +0xb0 finds something, if hidden (+0x9d) or if vfunc +0x80 == 2. Also sets the "last event" location `0x5159a8/ac`, `0x5159a0 = 4` (`0x462510`, local player only) |
| 2 | 103/403/303 | `0x44fd02` family | 4 | attack order (known) |
| 3 | 104/404/304 | `0x451440` = Tactic slot +0xf0 | 4 | V: "follow team" order: sets follow flag +0x94 = 1, leader +0x98, increments leader +0xac (debug strings "Lost/Hidden team ordered to follow") |
| 6 | 107/407/307 | `0x454401` family | 4 | V: stop following: clears route queue, clears +0x94, decrements leader +0xac; plays only when a flag argument is set (G: player-issued) |
| 7, 8 | 108,109/408,409/308,309 | `0x455570` | 6 | under attack (known) |
| 4, 5, 0x17 | - | no literal site found | - | G unused (could be computed; not seen) |

Town and production announcements (category 1 unless noted; Russian id = 0x65+k):

| k | Ru id | Function | Trigger (G unless marked) |
|---|---|---|---|
| 0x13 (19) | 0x78 (120) | `0x459bb0` BuildThink | the town's main building (+0xf0) finished; town flag +0x170 = 1, build tree reset to race record `0x4af130 + (idx + race*4)*0x462` (V) |
| 0x1b (27) | 0x80 (128) | `0x459bb0` | another building finished, type not 0x12/0x28/0x46 (18/40/70) (V condition) |
| 0x16 (22) | 0x7b (123) | `0x459bb0` | state +0x158 == 2 and production queue empty: queue finished/idle |
| 0x15 (21) | 0x7a (122) | `0x459bb0` | side money check `0x447d30(cost 0x45c5b0)` fails: not enough resources |
| 0x1d (29) | 0x82 (130) | `0x459bb0` | next item can start (build tree slot +0x34 true): production started |
| 0x1a (26) | 0x7f (127) | `0x459bb0` | produced unit spawned and added to the town group (slot +0x134): unit ready |
| 0x1f (31) | 0x84 (132) | `0x459bb0` | category 5 (positional speech): G upgrade/research complete |
| 0x14 (20), 0x1e (30) | 0x79, 0x83 | `0x45d270` RepairerThink | G repair started / repair finished |
| 0x18 (24), 0x19 (25) | 0x7d, 0x7e | `0x45a920` CaptureTown | G town captured / town lost |
| 0x1c (28) | 0x81 (129) | `0x45e8e0` (Town slot +0x138, the per-tick town update called from `0x4577b0`) | V: town controller +0x16c becomes the town itself and side check `0x43d2c0` false. Then `0x45ea00` runs |

Every town announcement also calls `0x462510` (last-event location). The town tick `0x45e8e0`
calls `0x45d270` and `0x459bb0` each tick.

## 3. Category-4 effect in `0x45ea00` (V)

- After the k=0x1c announcement the town race index `+0x164` selects a type from {0xc, 0x27, 0x41}
  (12, 39, 65) and a list from {0xac, 0xaa, 0xab} (172, 170, 171).
- If the town has that type (`0x45c9b0`), the code finds the first member of that type that is alive (byte +0x5c).
  It then scans that member's descriptor list at `+0x64 -> +0x100` (terminated by -1) for the value `0x4c1968`, which is the daylight time word.
- On a match it plays the list through `0x401480` with category 4, at the town position, extra volume -500 (0xfffffe0c), loop=1, and random pitch (2.0).
  It then immediately calls `0x474950(handle, -4000, 15000)`: a fade to silence over 15000 = 150 fade steps.
- G: a race-specific jingle that plays only at certain times of day. Category 4 is used only to restrict it to the local
  owner, so it is also muted when the Acknowledgement option is off.
- After that: if side +0x138 is set it calls the town-object slot +0x28, and if `0x42e8c0(0x2f)` returns false it calls `0x45e7c0`.

## 4. Countdown `0x49b368` (V) and fade period

- `.data` initial value 0xffffffff. The only writes are: reset to -1 in mission teardown (`0x430f6d`) and the
  decrement in `0x43a280` (only when > 0). When the decrement reaches 0, `0x4c12c0 = 1` is set.
- `0x4c12c0` is read in vehicle death (`0x4079cd`, fragment `0x4078ea`), which then zeroes `table[rand()]` of the unit
  pool index `0x4cc3d4`. It is also read at `0x43396e` (Marine death) and at `0x433df2`.
- G: an anti-piracy trap that corrupts state over time. No code arms it in this build, so it is dormant.
  The clone should ignore it.
- Fade step period: the worker that applies fades was not traced. G: 100 time units per step (argument/100).
  The wall-clock period needs a live run or a read of the mixer thread.

## 5. Per-type sound lists (select, attack, under attack, death, spawn)

- V: select, attack order and under attack are **not per type**. They come from the voice class
  (`0x454240`: type <= 0x1a or 0x52/0x53/0x55 -> 1; 0x1b..0x34 or 0x57 -> 2; else 3) and the k table above.
  The weapon/attack sound itself comes from animation frame field [2] and weapon descriptors.
  The only per-type descriptor attack word is Bird +0x50.
- V: spawn, hit and death are descriptor words: Marine +0x3c/+0x40/+0x44, Bird/Vehicle +0x44/+0x48/+0x4c/+0x50,
  Truck +0x38 (engine +0x3c), Headtruck +0x44 (engine +0x48), Cannon +0x38, Spider +0x44, Zeppelin +0x3c.
  Fixed lists: Zeppelin death 0xf, vehicle explosion 0x96, Marine type 0x52 death 0x96.
- V, important: in the file image only record 0 of most descriptor tables is filled. For example, Bird `0x49b370`
  has record 0 with spawn = 9 and records 1 and 2 all zero. Code writes into the tables at start-up (for example `0x406e49` writes
  `0x49b3fc`). A per-type table read from static data is therefore wrong. The per-record values in
  `docs/descriptor_other_kinds.md` (Bird spawn 9/9/10, death 0/0/12, attack 0/202/0) must come
  from the start-up initialisers or a dump.
- Type -> record: table `0x4933f8` (8 bytes per type: kind, record). Examples: 0x21 Eagle = Bird record 2,
  0x22 Deltaplan = Bird record 1, 0x3c Paraplan = Bird record 0, 0x36 Hypno = Marine record 0. A full table needs
  the initialiser values (open item).

## 6. In-mission save format: residuals

V, confirmed against `docs/savegame.md`:
- Writer `0x4450e0`:
  - Header: mode word, `0x4dd164`, path 0x100, description 0x40, SYSTEMTIME 0x10.
  - View `0x462560`, then globals `0x43d340`: 5 words `0x4cc38c, 0x4cc240, 0x4cc244, 0x4cc23c`, tick `0x4c5730`.
  - Pools in this order: decorations, towns, sides, tactics, units, weapons, effects, transports, scripts, orders.
  - Then UI `0x443ed0` and daylight `0x418620` (2 words `0x4c1968/0x4c196c`).
- Loader `0x4455f0`: same order. The fix-up order is decorations +0x54, sides +0x08, towns +0x144, tactics +0x144,
  units +0x54, weapons +0x54, effects +0x10, transports +0x54, orders +0x04, then UI `0x443f00` and daylight
  `0x418650` (which also re-applies the daylight palette under mutex `0x5153ec`). There is **no** loop for the scripts pool.
- New V: around the stream the writer calls `0x46fd00`, which destroys the weapon prototype objects `0x515ac0..0x515bb0`
  (60 slots, vfunc +4 with delete flag). Afterwards `0x470780` recreates slots 1..0x3b (59) via `0x46d900(0, i)`.
  The load path also calls `0x470780` at the end of `0x42ec90`. So **saving resets** any state held in those prototypes
  (G: per-weapon-type shared state).
- New V: the load path is `0x42ec90` with param_3 != 0. It still reloads the `.mp` globals, map, passability `.dat`,
  Tileset, `Music/MusicMask` and **Winds**, and sets `0x4d9b78 = 0`.
  It skips mission object and script creation (`0x42ffb0, 0x430770, 0x430880, 0x42f560, 0x42fc90, 0x42f6e0, 0x42f8e0, 0x430ae0`)
  and calls `0x445570` instead.

Not saved (V: absent from the writer):
- CRT `rand()` state. No `srand` call exists in the corpus.
- Wind objects `0x4d9bbc[0..0x515574)`. On load they are rebuilt by `0x470a90` with 4 `rand()` calls each (position, direction,
  speed 10..39), from whatever the rand state is at that moment.
- Music cursor `0x4c561c` (and the current track). On load only the mask is recomputed. Speech queue, active voices, ambient loops, sound cooldowns.
- Globals cleared in mission teardown and not in the stream: `0x4dd160` (end-of-mission latch), `0x4c12c0`, `0x49b368`,
  `0x4c1964`, `0x4c4d44`, `0x515720`, `0x4cc690`, `0x4bc7f4`, `0x4ae980`, `0x4ae19c`, `0x4ab670`.
- Terrain, `.rm` resources and passability (reloaded from the mission path). Fog **is** saved, per side.
  Timers, triggers and selection are saved.

Determinism implication (G):
- A reloaded game cannot continue the original run bit for bit: rand restarts from the process's
  current state, and wind rebuilding consumes 4 extra `rand()` calls per wind.
- Two loads of the same save in fresh processes should match each other only if the rand state at load is
  the same. That requires the same front-end history, because sound and UI also use `rand()` (G).
- The script pool needs no fix-up because script variants store only scenario ids, counts and raw
  packets, with no object pointers (V from the formats). Script latches are restored from the stream.

## Unknowns (live run marked L)

- The exact meaning of k=1 (the contract of slot +0x12c and the +0xb0 test). Speech category 1 in `0x4016b0` is not described in `audio_events.md`.
- Final names of the town announcement triggers (k 0x14, 0x15, 0x16, 0x18, 0x19, 0x1d, 0x1e, 0x1f) (L to confirm the captions, group 6).
- Fade step wall-clock period (L, or read the mixer worker).
- Start-up initialiser values for descriptor records 1..n, needed for the per-type spawn/hit/death table
  (read the initialisers near `0x406e00` or dump memory: L).
- What the weapon prototypes reset by `0x46fd00` hold (`0x46d900`).
- Whether `0x43a160` (which resets the music cursor) runs on every mission start (callers at corpus lines 9522, 17431, 33526 and others not read).
- Whether the order in which two fresh-process loads consume `rand()` matches (L).

## Correction (2026-11-21, from docs/tick_order_and_rand.md)

V: `srand` is called once, at `0x42ecbf` at the start of the mission loader `0x42ec90`, with the
seed in `0x4dd15c` (0 at process start; only a multiplayer start writes it). The earlier
statement that no `srand` exists was wrong. The rand state is per thread (the CRT in use is the
multithreaded one at `0x481070`), so rand calls on the renderer, audio, view timer and network
threads cannot shift the simulation sequence. Also: `0x4577b0` is the run loop of a whole
mission, not a single tick; one tick is one pass of its loop.
