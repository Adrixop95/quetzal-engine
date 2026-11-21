# Command packets, lockstep and network layer

Static analysis only (corpus `all_full.c`, `full.s`, PE strings). V = read in code/data, G = inference.
Numbers are hex with decimal in parentheses where useful. Builds on tick_order_and_rand.md and
specials_commands.md; corrections to those are marked "correction".

## 1. The packet (0xd4 = 212 bytes, 0x35 dwords), V

One structure serves every purpose: the outgoing queue, the lockstep ring, lobby messages and the wire
format. It is sent raw with `sendto(len 0xd4)`; there is no extra header, no length field and no CRC.

| Off | Meaning | Writer |
|---|---|---|
| +0x00 | session id (`0x4cc38c`, host `timeGetTime()` at create, `0x43c730`) | `0x43a410`/`0x43a580`/`0x43d060` |
| +0x04 | sender player index (`0x4cc240`) | same; SP sets 0 |
| +0x08 | packet type (PCT_* table `0x4ad5dc`, 0x2b names) | producer |
| +0x0c | frame stamp = globalTime `0x4c5730` | `0x43d060`, or send with stamp flag=1, SP main loop |
| +0x10 | MP: `rand()` of the game thread at send time (desync token) | `0x43d060` only |
| +0x14..+0x50 | payload (see tables) | producer |
| +0x54..+0xd3 | MP: 0x80 (128) bytes copied from `0x4dd268..0x4dd2e7` | `0x43d060` |

- V the +0x54 block: `0x4dd268..0x4dd288` receive the per-phase value of `0x515570`, `0x4dd290..0x4dd2a8`
  the per-side value after each side's command. All 25 references to `0x515570` in `full.s` are inside
  `0x4577b0` and are zero stores or reads; nothing increments it. G: profiling/debug counters with the
  increments compiled out, so the block is effectively constant. It is only printed (12 dwords) on a
  desync. A compatible implementation can send zeros there (G, the rest of the range up to `0x4dd2e7`
  is other globals, unidentified).
- V SP leaves +0x00 and +0x10 stale (whatever the queue entry held); only +0x04=0 and +0x0c are set.

### Producers: outgoing queue `0x4d9c50`, V
- Object `0x4d9c28`: ring at `this+0x28` = `0x4d9c50`, 64 (0x40) entries x 0xd4, write index
  `this+0x3528` = `0x4dd150`, read index `0x515564`, mutex `0x515568`.
- V the writers (`0x436704`, panel buttons) do `idx = (idx+1) & 0x3f` with no full check: more than 64
  queued commands silently overwrite unread ones (G: unreachable at one pop per tick in practice).
- V one entry is popped per tick (step 20); empty queue gives type 0x25 FRAME_NOTHING. So a side
  issues exactly one command per tick; extra commands are delayed one tick each.

### Frame packet types consumed by the switch in `0x4577b0` (V handler, payload offsets V from the handler, producer G unless noted)

`side` below is the side array entry `0x4dd2f4 + side*0x69c`. Coordinates are world pixels (cell = /0x30 (48), V in `0x447e00`).

| Type | Name | Payload | Handler |
|---|---|---|---|
| 0x11 (17) | FRAME_PLAYERREMOVE | none | network thread only, never reaches the switch (section 2.5) |
| 0x12 (18) | FRAME_SAVE | +0x14 = 0 (V `0x434af0`) | `0x515559 = 1` -> save menu state 5 (MP via `0x429600`) |
| 0x13 (19) | FRAME_PAUSEON | none | `0x463480(0x4dd300)` pause toggle. Producer not found |
| 0x14 (20) | FRAME_PAUSEOFF | +0x14 = 0 | not in the switch: sent out of band by `0x43d300`, handled in `0x43a850` (2.6) |
| 0x15 (21) | FRAME_CREATEGROUP | +0x14 x0, +0x18 y0, +0x1c x1, +0x20 y1 (min/max of the drag box), +0x24 modifier flags | `0x448b40(side, &+0x14, (byte)+0x24)`. V producer `0x4366c7`: box drag, mode 1 |
| 0x16 (22) | FRAME_SELECTGROUP | +0x14 x, +0x18 y, +0x1c flags | `0x448520(side, &+0x14, +0x1c)`. V producer `0x43665c`: click with button flag `bl` set |
| 0x17 (23) | FRAME_MOVE | - | no producer, no handler (V) |
| 0x18 (24) | FRAME_ATTACK | +0x14 x, +0x18 y, +0x1c flags | `0x447e00(side, &+0x14, +0x1c)` generic order. V producer `0x436665` |
| 0x19 (25) | FRAME_ADDTOGROUP | as 0x15 | V produced at `0x4366c4` (box drag, mode != 1) but **no case in the switch**: ignored. G: dead code path or never reached |
| 0x1a (26) | FRAME_DESTROYHERE | +0x14 x, +0x18 y | `0x4489b0(side, &+0x14)`. V producer: the 0x16 path when `0x440940()` is true (G: destroy cursor mode) |
| 0x1b (27) | FRAME_BIRTH | - | no handler |
| 0x1c (28) | FRAME_BUILD | +0x14, +0x18 position pair (copied to a local and passed on), +0x1c item id (-1 -> treated as 0x25) | town = side `+0x688` with vtable `+0x80` == 1; `0x45cec0(town,item)`, `0x45cd50`, then `0x410960` or `0x41d6a0` by vtable `+0xfc`. Producer `0x441b1d` |
| 0x1d (29) | FRAME_GARRISON | +0x14 flags (`0x42ea70`), +0x18 = 0 (V `0x434af0`) | `0x448350(side)` |
| 0x1e (30) | FRAME_REPEARMODE | +0x14 mode | town tactic `+0x128` vtable `+0x40/+0x3c`; toggles `0x4cc7d4`. Producers `0x443a1f`, the auto one in step 6 (+0x14=0,+0x18=0) |
| 0x1f (31) | FRAME_ATTACKMODE | +0x14 object id, +0x18 mode | `0x447ba0(id)` virtual `+0x100(mode)`. V producer `0x4434de` (+0x18 = 2) |
| 0x20 (32) | FRAME_BUILDMODE | +0x14 object id, +0x18 value | object `+0x158 = value` |
| 0x21 (33) | FRAME_PAUSEBUILD | +0x14 object id, +0x18 1 or 2 | 1: `+0x110` vtable `+0xc`; 2: `0x45a880`. V producer `0x44307d` |
| 0x22 (34) | FRAME_QUEUEKILL | +0x14 object id, +0x18 queue entry (G) | `0x45cfd0(obj, +0x18)`. Producer `0x442482` |
| 0x23 (35) | FRAME_SELECTBYNO | +0x14 = +0x18 = 0, +0x1c object id, +0x20 flags (V `0x4416e0`) | if +0x04 == local player: camera centre on the object; then `0x448f60(side, +0x1c, +0x20)` |
| 0x24 (36) | FRAME_FOLLOWTEAM | +0x1c id, +0x20 flags (G by analogy; stack offsets in `0x441630` ambiguous) | `0x448210(side, +0x1c, +0x20)`; on success and local sender, `0x4cc7f0` vtable `+0x24` |
| 0x25 (37) | FRAME_NOTHING | none | no-op |

Modifier flags (`0x42ea70`, V in interface_ingame.md): bit 1 Add (Shift), bit 2 Inv (Ctrl).
Local-only side effects (camera, panel refresh) are gated on `+0x04 == 0x43d200()`, so they are safe in
lockstep (V).

## 2. Single player versus multiplayer

### 2.1 Common ring `0x4c5810`, V
16 frames x 8 slots x 0xd4 = 0x6a00 (27136) bytes; slot = `(stamp & 0xf) * 8 + player`. `0x43d120`
copies slot `((globalTime-1) & 0xf)*8 + side` and returns false when globalTime == 0 or `0x4cc394`
(in-game flag) == 0. Both SP (`0x43d220(1)` sets `0x4cc394 = 1`) and MP use it.

### 2.2 Single player, V
- Each tick: pop queue, `+0x04 = 0`, `+0x0c = globalTime`, `0x43d170` stores to slot
  `(g & 0xf)*8 + 0x4cc240` (= 0), `0x43d1c0` g += 1. The next tick reads slot g. So one packet per
  side per tick, only side 0 human; other sides are skipped by `0x43d2c0` (empty name).
- Correction: "8 slots, 7 read": the tick loop reads sides 0..6 (counter `0x4dd290..0x4dd2ac`), while
  the network layer has 8 player records (`0x4cc248`, 8 x 0x30), 8 arrival flags (`0x4cc214[8]`) and
  counts 8 names (`0x43a820`). G: an 8th player would take part in lockstep but its commands would
  never run; the host's max-player value `0x4cc23c` probably never exceeds 7 (UI not traced).

### 2.3 Multiplayer frame flow, V
1. Mission start (`0x4577b0` prologue): if MP, `0x43d060` broadcasts a 0x25 stamped with g = 0.
2. Each loop pass in MP first waits `0x43cec0(800)` = `WaitForMultipleObjects({0x4cc21c frame-complete
   event, 0x4c4d6c quit event}, 800 ms)`. Result 1 = quit; 0x102 (timeout) -> `0x43cee0` (2.4) and wait
   again; 0 -> run the tick.
3. The tick executes slot g-1 for each human side (step 5), then pops one command and `0x43d060`:
   +0x54 block, `+0x10 = rand()`, `+0x00` session, `+0x0c = g`, `+0x04` local index, store in own ring
   slot, broadcast (`0x43a580(pkt, 0)`, no restamp).
4. Network thread `0x43acd0`, frame types 0x11..0x25 -> `0x43a850` under mutex `0x4cc210`:
   - drop unless session matches and in game;
   - store the packet at `(stamp & 0xf)*8 + sender` regardless of its stamp;
   - stamp == g -> arrival flag `0x4cc214[sender] = 1`;
   - flag not set and stamp > g -> send 0x26 RESEND (+0x14 = g) to that sender (it skipped us);
   - when the number of set flags == `0x4cc244` (human count): clear flags; desync check (2.7);
     `g += 1`; pre-set the flag of every human whose slot `(g & 0xf)` already has stamp == g;
     `SetEvent(0x4cc21c)`.
- Latency (G from the order): input read in pass k is queued, sent in pass k+1 with stamp g, and runs in
  the pass after frame g completes. Same two-pass delay as SP. There is no adaptive latency, no input
  delay setting and no frame batching: every tick needs all peers' packets for the previous frame, so the
  tick rate is bounded by the slowest peer plus one-way network delay, on top of the `Sleep` pacing of
  step 22.
- V the sender expects to receive its own broadcast (loopback of UDP broadcast); if it does not, the
  timeout path parses its own slot ("Parsing self", `0x43cfc2`), which costs 800 ms per frame. G: on a
  modern stack without broadcast loopback this would make MP run at about 1 tick per 0.8 s.
- G hazard: stores are by `stamp & 0xf` with no window check, so a duplicate older by 16 frames would
  overwrite the current slot. A reimplementation should reject stamps outside [g, g+15].

### 2.4 Timeout and resend, V `0x43cee0`
- g == 0: re-send PCT_START (0xf, +0x14 player count, +0x18 seed) unicast (`0x43a4c0`, player record
  address) to every human whose flag is not set.
- g > 0: for each missing human: if it is self and own slot has stamp g, feed it to `0x43a850`;
  otherwise send 0x26 RESEND with +0x14 = g.
- Receiving 0x26: if +0x14 == `0x4c5744`, resend the buffer `0x4c5738`; else if own slot `(f & 0xf)` has
  stamp f, resend it unicast unchanged; else log "PCT_RESEND required unacceptable frame". V
  `0x4c5738..0x4c5744` has no writer in the corpus (bss, zero): a RESEND for frame 0 matches and sends
  a zeroed packet that every receiver drops (session 0). G: a lost frame-0 packet may stall the game
  forever. Needs a live check.

### 2.5 Player leaving, V
- `0x43cde0` (called at mission end and from three other sites): in game, it builds 0x11
  FRAME_PLAYERREMOVE in the current queue read slot and sends it via `0x43d060` at once; in the lobby it
  broadcasts 8 REMOVEME and, as host, `0x43ca30` (broadcast 0xd PLAYERREMOVED for self and 4
  SERVERREMOVED with the server name).
- Receivers (`0x43a850`, 0x11): if the sender is human, clear its name byte (`+0x10` of its record),
  recount `0x4cc244` (`0x43a820`); if the sender is the local player, leave the session. The packet is not
  put in the ring and does not set a flag; the frame completes with the reduced count. The side then
  fails `0x43d2c0` and its commands are skipped. G: its units stay, with no AI takeover found.
- Silent drop (crash, cable): no timeout, no kick, no vote. The frame never completes; every 800 ms
  `0x43cee0` resends RESEND requests. G: the game hangs until the user quits (quit event `0x4c4d6c`).
- PING 0x28 is answered with 0x29 (if the session matches); 0x29 clears bit `sender` in `0x4c5720`. 0x28
  is only sent by the thread to itself at start-up (`0x43a780`, `0x43a7d0`, and the broadcast in 3);
  no peer pinging and no reader of `0x4c5720` found: no latency measurement (V no reader, G unused).

### 2.6 Pause in MP, V
Key 0x1e when paused: SP `0x463480`, MP `0x43d300` broadcasts 0x14 PAUSEOFF out of band (no stamp
use). `0x43a850` on 0x14: `0x463480(0x4dd300)` and `0x4c5724 = 1`; the main loop gate
`0x515400 == 0 || 0x43d2f0()` (reads and clears `0x4c5724`) then lets the tick run. Pause-on is the
lockstep packet 0x13; its producer was not found (G: the "Pause" dialog `0x429200`).

### 2.7 Desync detection, V
On frame completion, if g != 0 and the human count > 1: compare `+0x10` (rand token) of the first two
human players (indices 0..6 only) in slot `(g-1) & 0xf`. On mismatch: once per process (`0x4ad690`
latch) set `0x4dd158 = 1`, and format "RND Miss" plus three 12-dword dumps of the local +0x54 block for
frames g-1, g-2, g-3 into `0x4c5660`. V `0x4dd158` has exactly two references, both writes (the other is
the debug key 0x1f); `0x4c5660` has 8 references, all pushes as a wsprintf target. So a desync is
detected but has no effect: no abort, no resync, no message. Only players 0 and 1 (first two humans)
are compared.
- G: valid as a checksum only because all simulation rand calls run on the game thread (per-thread
  `_holdrand`) and both peers seed with the same `srand(0x4dd15c)` in `0x42ec90`.

### 2.8 Host start and seed, V
- `0x43c9b0` (menu `0x41a279`), host only (`0x4cc39b` net up, `0x4cc395` hosting): `0x446820()`,
  g = 0, broadcast 0xf START with +0x14 = player count `0x4cc244`, +0x18 = seed = `timeGetTime()` stored
  in `0x4dd15c`.
- Receiver (thread, case 0xf): if not in game and session matches: count = +0x14, seed = +0x18,
  `0x4cc394 = 1`, g = 0, `0x446820()`, `ResetEvent(0x4cc21c)`, signal `0x4cc224`, then
  `0x428f30(0x429070())` (start the mission whose path the lobby holds). G: the host enters the game the
  same way, through its own looped-back broadcast (it does not set `0x4cc394` in `0x43c9b0`).
- Mission load: `0x458aa0` -> `0x42ec90(path, local index, 0)` -> `srand(0x4dd15c)`.

### 2.9 Save and load in MP, V
`0x43d340` (save, called at corpus 37169) writes five dwords, `0x43d3a0` (load) reads session
`0x4cc38c`, local index `0x4cc240`, player count `0x4cc244`, max players `0x4cc23c`, globalTime
`0x4c5730` (G that the save side writes the same five in order: the decompile lost the arguments). The
ring and the rand state are not saved (G: an MP load relies on `srand` of the reload).

## 3. Transport, V

- Raw Winsock 2 loaded at run time (`LoadLibrary("WS2_32.dll")`, `0x43acd0`), not DirectPlay. Two
  datagram sockets, chosen by `0x4ad5d8` (copied from the menu option `0x5154ea`): 0 = TCP/IP, 1 = IPX.
  - UDP: `socket(AF_INET 2, SOCK_DGRAM 2, getprotobyname("UDP"))`, `SO_BROADCAST` (0x20) on,
    bound to the first address of `gethostbyname(gethostname())` (not INADDR_ANY), port bytes
    `7d 08` = **32008**.
  - IPX: `socket(AF_IPX 6, SOCK_DGRAM, NSPROTO_IPX 0x3e8 (1000))`, socket number bytes `72 06`
    (0x7206 = 29190); broadcast address = own network number, node ff:ff:ff:ff:ff:ff.
- Unicast `0x43a410(addr, pkt, stamp, ipx)`, broadcast `0x43a580(pkt, stamp)` to 255.255.255.255:32008
  (or IPX broadcast). Both fill +0x00 session, +0x04 local index, and +0x0c = g when `stamp` is 1;
  `sendto(s, pkt, 0xd4, MSG_DONTROUTE 4, addr, 0x10)`. Receive `0x43a6b0`: blocking `recvfrom` of 0xd4
  bytes; the from-address goes to `0x4c5650` and is used as the reply address. On error the thread
  sleeps 1000 ms.
- Every frame packet is broadcast to the whole LAN segment; there is no peer list on the send path
  (G: LAN only, no Internet play without a tunnel). There is no ack, sequence number, compression or
  encryption; reliability comes only from RESEND (2.4).
- Threads and sync (V): net init `0x43c500` allocates the server list (0x2700 = 32 x 0x138) and the
  player table (0x180 = 8 x 0x30), creates mutex `0x4cc210`, events `0x4cc234` (ready), `0x4cc220`,
  `0x4cc21c` (frame complete) and the receive thread `0x43acd0`. At start-up the thread sends a 0x28
  PING to itself and loops until it reads back its own `timeGetTime` stamp (+0x14 at the `0x43b0xx`
  loop), then stores its own address (G: discovering the local address and checking loopback).
- Player record (0x30): +0x00 sockaddr (16), +0x10 name (NUL-terminated, up to 0x20); empty name =
  not human (`0x43d2c0`). Server list entry (0x138): +0x00 host sockaddr, +0x10 session id, +0x14 name
  (0x20), +0x34 path (from SERVERINFO, G), +0x134 "open" flag.

### Lobby messages (types 1..0x10), handled in `0x43acd0` (V types and flow, payload offsets V from stack offsets unless G)

| Type | Name | Payload / action |
|---|---|---|
| 1 | ENUMSERVERS | broadcast by browse `0x43c640` (then waits 1000 ms). Host replies 2 with +0x14 = hosting flag, +0x18 server name (0x20) |
| 2 | SERVERNAME | client, while browsing (`0x4cc398`), list < 32: add entry (dedupe by name); if +0x14 != 0 send 10 |
| 3 | SERVERCREATED | broadcast by host create `0x43c730` (+0x14 hosting, +0x18 name); same add as 2 when not in game |
| 4 | SERVERREMOVED | +0x18 name: remove entry; if it was our session, leave and show a message (string `+0x1d4`) |
| 5 | CONNECTME | to host: +0x14 slot or -1, +0x18 player name. Full/occupied -> 7; else fill record, reply 6 (+0x14 slot), then 0xe for each player |
| 6 | WELCOME | client: session = +0x00, local index = +0x14 |
| 7 | SERVERFULL | client stops waiting |
| 8 | REMOVEME | host: if the sender address matches its record, broadcast 0xd (+0x14 index) |
| 9 | GETPLAYERS | host replies 0xe for every record |
| 0xa | QUERRYINFO | host replies 0xb with +0x14.. = mission file name (after the last `\`) |
| 0xb | SERVERINFO | client stores it in the entry (with `GetCurrentDirectory`, G: map path) |
| 0xc / 0xe | PLAYERADDED / PLAYERINFO | +0x14 index, +0x18 sockaddr (16), name at +0x34 (G offset); fill record, recount, update lobby UI (`0x41aef0`/`0x41afd0`) |
| 0xd | PLAYERREMOVED | +0x14 index: clear record; if local, leave session |
| 0xf | START | 2.8 |
| 0x10 | STOP | no case (V) |
| 0x26 | RESEND | 2.4 |
| 0x27 | OUT | no case (V) |
| 0x28 / 0x29 | PING / PINGANSWER | 2.5 |
| 0x2a | TEST | no case (V) |
| 0x2c | (no name) | sent as reply to an unexpected WELCOME/SERVERFULL (V); beyond the name table |

## 4. Replay / record

- None found. V: no string with replay/record/demo/playback; the only lockstep storage is the 16-frame
  ring; the save path stores five network dwords, not packets; the debug log buffer `0x4c5660` is never
  output (no file write, no OutputDebugString reference). G: there is no replay feature. A replay could be
  built in a reimplementation by logging (seed, per-tick packets of sides 0..6), since the simulation is
  driven only by these packets plus the seed.

## 5. Minimum for a compatible lockstep (G, derived from the above)
1. Same seed path: `srand(seed)` at mission load, all sim rand on one thread, one extra `rand()` per tick
   after the tick (the +0x10 token) in MP only. SP never draws it, so SP and MP rand streams differ.
2. Frame g executes only after packets stamped g-1 from every human (one per player per frame).
3. Exactly one command per player per tick, 0x25 when idle; execute sides 0..6 in ascending order.
4. Wire: 212-byte datagrams, UDP 32008 broadcast, little-endian dwords, fields above.

## Unknowns / needs a live run
- Whether broadcast loopback happens on the original setup (host entering the game through its own
  START, own frame packets setting the own flag).
- Frame-0 packet loss: does the game really stall (2.4)?
- Producer of 0x13 PAUSEON; exact producers/offsets for 0x20, 0x22, 0x24 (stack offsets ambiguous
  around pushes).
- Whether 0x19 ADDTOGROUP is ever produced in normal play (it would be ignored).
- What `0x4dd2ac..0x4dd2e7` (tail of the +0x54 block) contains; whether `0x515570` is written through a
  pointer.
- Max players allowed by the host UI (7 or 8) and the meaning of an 8th slot.
- Behaviour of a side after PLAYERREMOVE (idle units or AI).
- Tick pacing in MP (`0x5154dc` sleep target versus network wait).
