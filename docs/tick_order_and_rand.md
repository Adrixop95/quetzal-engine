# Tick order, pool order and rand() in Aztec.exe

Sources: corpus `work/decomp/all_full.c`, objdump `work/gaps/full.s`, plus a direct-call graph built from that objdump (virtual calls are not included). V = read in code, G = guess. Numbers are given in hex (decimal).

## 0. Correction to earlier docs

- V `0x4577b0` is not the tick itself. It is the per-mission run loop. Its callers
  `0x458a40/0x458a70/0x458aa0/0x458ad0` (all four run on the game thread `0x458af0`) call the loader
  `0x42ec90`, then `0x4577b0` once, then the teardown `0x430cb0`. One logic tick is one pass of the
  loop in `0x4577b0`, from `0x457977` (timeGetTime) to the `Sleep` near `0x4587f0`. Everything that
  movement_residual.md section 5 says about "one call per tick" applies to one loop pass.
- V pool `0x5156f8` (element 0x1b8 (440) bytes, capacity 0x100 (256)) holds the towns, not the
  units. Its slot `+0x138` is the town update. Units and world objects are in `0x4cc3d0`
  (element 0xb0 (176) bytes, capacity 10000).

## 1. Order of operations in one tick (one loop pass of 0x4577b0)

The steps run in this order. All are V unless marked otherwise.

0. Wait on mutex `0x51556c` and check the quit/abort events `0x4c4d6c/0x4c4d68`. In MP only,
   `0x43cec0(800)` waits for the peers' frame. A return of 0x102 (timeout) calls `0x43cee0`
   (resend).
1. `t0 = timeGetTime()`, then wait on the world mutex `0x4c4d58`. The mutex is held until the end
   of the pass.
2. Clear `0x515a70`, `0x515a58` and `0x515a5c`, and copy `0x515a60[0..1]` into `0x515a68[0..1]`
   (two dwords, then zeroed). G: per-tick event latches.
3. localTime `0x4184a0`: both tileset branches increment `0x4c1968` and wrap to zero
   above 0x1c1f (7199). The tileset index `0x4dd2e8` selects phase table `0x4a8f68`
   or `0x4a9338` (stride 9 dwords), not whether time advances. V: alternate branch
   rechecked on 2026-11-24; this corrects the earlier tileset-zero-only reading.
4. Countdown `0x4bc7f4`: when it reaches 0, set `0x4c1964 = 1`.
5. Command packets: sides 0..6 in ascending order (counter `0x4dd290..0x4dd2ac`, 7 entries,
   stride 0x69c in the side array `0x4dd2f4`).
   - A side is skipped when `0x43d2c0(side)`, i.e. its byte `+0x10` in `0x4cc248[side*0x30]`, is 0.
   - `0x43d120` copies 0xd4 (212) bytes from the ring `0x4c5810`, slot
     `((globalTime-1) & 0xf) * 8 + side`. It copies nothing when globalTime == 0 or `0x4cc394 == 0`.
   - Each side gets at most one packet per tick. The type switch covers 0x12, 0x13, 0x15, 0x16,
     0x18, 0x1a and 0x1c..0x24; 0x25 is the no-op.
6. Local side only: under a condition, an automatic packet of type 0x1e is pushed to the outgoing
   queue `0x4d9c50` (64 entries, write index `0x4dd150`).
7. Effects pool `0x4c1978`: virtual `+0x60`.
8. Script/trigger pool `0x4cd698`: virtual `+8`. For the trigger class (vtable `0x4948b0`) this is
   `0x45f0a0`.
9. Town pool `0x5156f8`: virtual `+0x138` (town update; the tactic think runs from inside it at
   `0x45e8e9`).
10. Sides 0..`0x4cc23c`-1 in ascending order: `0x449980(side)`.
11. Tactic pool `0x4d9b50`: virtual `+0x138`.
12. Unit/world-object pool `0x4cc3d0`: virtual `+0x120`.
13. Weapon/projectile pool `0x515a98`: virtual `+0x84`.
14. Wind array `0x4d9bbc[0..0x515574)`, index ascending: `0x470c10` (non-null entries only).
15. Sound-emitter pool `0x4c1298` (element 0x28): virtual `+8` (`0x4011f0`, positional update).
16. `0x4018c0` decrements `0x4ae980` and handles a music track change. `0x43a280` decrements
    `0x49b368` and plays queued speech through `0x475010`.
17. Outcome: the local side's `+0x140` is copied to `0x515420` when it is != 2.
18. Fog/minimap build:
    - `0x47c980/0x47cb30` on `0x4d9c10` from pools `0x4cc3d0`, `0x515a98` and `0x4c1978`.
    - Per side ascending: `0x47d460(side+0x690, 0x20)`.
    - Per side: `0x47d070` from the objects of `0x4cc3d0` owned by that side, then `0x47d430`.
    - Pool `0x4c4ce0`: byte `+0x129` = visibility (virtual `+0x10`). Then `0x42a940` runs on each
      element. This is the only per-tick update that pool gets. G: transports.
19. Byte `+0x2c` is cleared in every element of `0x4cc3d0`, `0x515a98`, `0x4c1978` and `0x4c4ce0`.
20. Outgoing command: one packet is popped from `0x4d9c50` (read index `0x515564`). If the queue
    is empty, the packet is type 0x25.
    - SP: stamp `+0xc = globalTime`, write the packet to ring slot `(globalTime & 0xf)`
      (`0x43d170`), then `globalTime += 1` (`0x43d1c0`, `0x4c5730`).
    - MP: `0x43d060` sends the packet. In MP, globalTime is advanced by `0x43a850`, called from
      `0x43c2a4` and `0x43cfc2`.
21. `0x435bc0`: the game-thread input pump. It turns mouse/keyboard input into packets in
    `0x4d9c50`.
22. Menu flags `0x515559..0x51555d` (pause, menus) and `0x4296f0`, then `0x429060`, `0x428ec0` and
    `0x465a50`. After that: release `0x4c4d58`, then `Sleep(max(1, [0x5154dc] - (now - t0)))`.

Notes on the order:
- G latency: input read in pass k (step 21) is queued in pass k+1 (step 20) and runs in pass k+2
  (step 5).
- V `0x515570` is reset before every phase and stored afterwards into `0x4dd268..0x4dd28c`. Those
  values go into every MP packet at `+0x54` (0x80 bytes). No direct write to it outside
  `0x4577b0` was found. Unknown: whether something writes it through a pointer, i.e. whether it is a
  checksum or a cost counter.
- The fog work is in step 18. No separate per-tick fog function was found elsewhere in this loop.

## 2. Pools: iteration order and slot reuse (all V)

- Allocator: `0x473c40` / `0x473e00`. List object vtable `0x4953f8`:
  - `+0x10 = 0x473f50` push_front
  - `+0x14 = 0x473fb0` push_back
  - `+8 = 0x474170` unlink + free
- Every game-object pool listed here is allocated through vtable `+0x10` with list 0.
  New objects are therefore inserted at the **front** of the list.
- Iteration: `0x4740d0` returns `head->next` and `0x474110` steps to the next element (the walk
  ends when the pointer leaves the slab). The order is **newest first, oldest last**.
  - Pools `0x4c1978`, `0x5156f8`, `0x4d9b50`, `0x4cc3d0`, `0x515a98` and `0x4c4ce0`: the tick reads
    `next` before the virtual call. An object deleted during its own update is safe. An object
    created during the walk is not visited in that tick (it goes in front of the cursor).
  - Pools `0x4cd698` and `0x4c1298`: `next` is read after the call.
- Free list: LIFO (`0x473dc0` push, `0x473d80` pop). The most recently freed slot is reused first.
- Growth: `0x473bb0` commits one page and pushes its slots in ascending address order. The
  highest address of a new page is therefore handed out first, and allocation then goes downward
  within the page. Slot size = (size + 8) rounded up to 8.
- Pool parameters (element bytes / capacity):

  | Pool | Element | Capacity |
  |---|---|---|
  | `0x4c1298` | 0x28 | 10000 |
  | `0x4c1978` | 0x38 | 10000 |
  | `0x4c4ce0` | 300 | 1000 |
  | `0x4cc3d0` | 0xb0 | 10000 |
  | `0x4cd698` | 0x24 | 0x1000 |
  | `0x4d9b50` | 0x100 | 2000 |
  | `0x4d9b88` | 0x814 | 100 |
  | `0x5156f8` | 0x1b8 | 0x100 |
  | `0x515a98` | 0x108 | 2000 |

- G: an object "handle" is its slot address, or an id kept separately. That was not traced.

## 3. rand / srand

- V `_rand` is at `0x481070`. It is the MT CRT version: per-thread `_holdrand` at `_getptd()+0x14`,
  with `x = x*0x343fd + 0x269ec3`, returning `(x >> 16) & 0x7fff`.
- V `_initptd` (`0x482e0b`) sets each new thread's seed to 1.
- V **srand IS called**, at `0x481063`, by exactly one call site: `0x42ecbf`, at the start of the
  mission loader `0x42ec90`. The argument is `[0x4dd15c]`. This is the game thread, the same thread
  that runs `0x4577b0`. The earlier "no srand" claim is wrong.
- V `0x4dd15c` is in .bss (past the end of .data at 0x4c2000), so it is 0 at process start. It has
  only three writers:
  - `0x43c9b0`, MP host start, called from the menu `0x41a279`: `seed = timeGetTime()`, broadcast
    in message 0xf.
  - `0x43acd0`/`0x43acf8`, the network receive thread: stores the received seed.
- Consequences:
  - G SP and campaign: every mission load reseeds with 0. The first rand() is 0x26 (38).
  - G a save load also goes through `0x42ec90`, so it reseeds with 0 as well. Whether saves
    store `_holdrand`: unknown, probably not.
  - G after an MP game in the same process, the old MP seed persists into later SP missions.
- V 123 (0x7b) call sites in 51 functions. Purpose is V where a doc or the code says so, otherwise G:

  | Function | Sites | Use |
  |---|---|---|
  | `0x401060` | 1 | ambient sound pitch (float) |
  | `0x401480` | 1 | sound pick/variation |
  | `0x46d900` | 1 | weapon factory, `rand % n` sound from list (`0x401250`) |
  | `0x407450` | 4 | unit movement / waypoint jitter (G) |
  | `0x407820` | 5 | unit movement / waypoint jitter (G) |
  | `0x407b10` | 2 | recovery think |
  | `0x410580` | 3 | building yield/harvest |
  | `0x410cc0` | 4 | active building progress |
  | `0x416280` | 2 | unknown |
  | `0x416b70` | 3 | goal-point pick |
  | `0x41d1a0` | 1 | defence tower |
  | `0x41d240` | 3 | defence tower |
  | `0x41d790` | 4 | defence tower think `+0x60` |
  | `0x42c0a0` | 2 | marine/vehicle step and heading (G) |
  | `0x42c430` | 3 | marine/vehicle step and heading (G) |
  | `0x42c880` | 3 | marine/vehicle step and heading (G) |
  | `0x4333b0` | 7 | unknown, unit class |
  | `0x433db0` | 3 | projectile-like think state 2 |
  | `0x43d060` | 1 | **MP packet `+0x10` = rand()** (G: desync check). MP only, once per tick |
  | `0x44a650` | 5 | unknown |
  | `0x44c130` | 2 | class-7 blast |
  | `0x44c590` | 2 | class-7 `+0x88`, random animation `% 3` |
  | `0x44c930` | 3 | unknown |
  | `0x44d130` | 3 | AI plan builder |
  | `0x44f5c0` | 2 | tactic mode init |
  | `0x44fc10` | 1 | tactic path point |
  | `0x451410` | 1 | tactic |
  | `0x451c30` | 1 | target assignment |
  | `0x452c70` | 2 | target assignment |
  | `0x453d50` | 1 | speech voice pick |
  | `0x4543c0` | 1 | tactic |
  | `0x454820` | 1 | tactic think scan interval 15+ |
  | `0x455570` | 1 | tactic |
  | `0x455800` | 1 | tactic |
  | `0x459bb0` | 7 | production/purchase |
  | `0x45a920` | 2 | CaptureTown |
  | `0x45b5f0` | 1 | AI development |
  | `0x45c9d0` | 1 | town |
  | `0x45cde0` | 1 | town |
  | `0x45d270` | 2 | RepairerThink notifications |
  | `0x45e090` | 1 | AI phase step |
  | `0x45e530` | 1 | AI phase step |
  | `0x45e7c0` | 3 | AI phase step |
  | `0x4601e0` | 2 | movement |
  | `0x4604a0` | 3 | movement |
  | `0x460a10` | 1 | movement |
  | `0x46ed60` | 2 | unknown |
  | `0x470a90` | 4 | wind creation (loader) |
  | `0x470c10` | 7 | wind per-tick update |
  | `0x471480` | 2 | unknown |
  | `0x471740` | 3 | removal state 4 |

  The definitive site list is `grep 'call   0x481070' work/gaps/full.s`.
- V no other LCG constants (0x343fd, 0x41c64e6d) appear in .text, so no private RNG was found.

## 4. Does non-simulation code perturb the sequence?

- V threads:
  - `0x458af0`: game thread. Runs srand, the sim and the input pump `0x435bc0`.
  - Main thread (WinMain): priority 0xf after it creates the game thread.
  - `0x463b00`: timer every 0x52 (82) ms that calls `0x463a40` on the world view `0x4dd300`.
  - `0x475550`: audio streaming.
  - `0x43acd0`: network receive.
- V because `_holdrand` is per-thread, rand() calls on the main, view, audio or network threads use
  their own state, which starts at 1. They **cannot** change the game thread's sequence. The
  call graph shows `0x463a40 -> 0x463980 -> 0x404dc0 -> 0x46d900`, which can reach rand on its own
  thread only.
- Risk, G: calls on the game thread outside the object updates do consume the sim sequence:
  - The input pump `0x435bc0` statically reaches rand:
    - `0x4041c0 -> 0x46d900`, and from `0x46d900` on to `0x401480` and `0x44c010 -> 0x44c130`
    - `0x462650 -> 0x45bd50 -> 0x43e0b0`, which leads to `0x407450`, `0x4601e0` and `0x471480`
    - `0x412a90 -> ... -> 0x416b70`
  - The sound helpers `0x401060/0x401480` are called from sim objects and draw from the sim
    sequence.
  - The loader `0x42ec90` reaches `0x470a90` (wind) and `0x44f5c0` after srand, so the loading
    draws are part of the deterministic sequence.
  - MP adds one draw per tick in `0x43d060`.

## Unknowns

- Whether the paths from `0x435bc0` to rand actually run during normal input (static only;
  `0x4041c0` is built on a stack object, G: a preview or temporary object).
- What writes `0x515570`.
- Why only 7 command slots are read when the ring has 8 per frame.
- The id/handle scheme over slot addresses.
- Whether saves store the rand state.
- What `0x4c4ce0` holds and what `0x42a940` does.
- The exact SP globalTime start value at mission start (`0x4c5730` is zeroed in `0x43c9b0` for MP;
  the SP reset was not found).
- The purpose of the 9 functions marked "unknown" in the table.
