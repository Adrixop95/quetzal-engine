# Engine lockstep protocol

The `aztec_net` library implements an independent two-peer lockstep layer under
`src/engine/net/`. It uses nonblocking plain POSIX IPv4 UDP sockets, with no
SDL_net dependency. Simulation rules are unchanged. This is a transport library;
the engine front end does not yet expose a multiplayer lobby or launch option.
Version 1 supports exactly two participants, controlling sides 0 and 1.

## Session and driver contract

Both participants must start with the same engine build, mission, rules, seed,
script configuration and simulation state. Establish these, a fresh nonzero
64-bit session ID, peer addresses/ports and the input delay out of band. The
initial state hash is checked before either peer can execute the first tick.
It is not a substitute for a content/build identity check: the existing engine
hash does not include every static input. Keep `script_side` identical on both
peers; `local_side` is presentation state. Only local UI input may differ.

Construct `Lockstep` after `GameSim::init`, with `GameSim::state_hash(world)` as
`Config::initial_hash`. Network boundary 0 means no network ticks have executed.
At each boundary `t`:

1. Call `submit(commands)` exactly once, including when input is empty. Commands
   must belong to the configured local side. They are sealed for tick `t + delay`;
   later submissions at the same boundary fail without replacing the input.
2. Pump UDP continuously, including while stalled. Pass received datagrams to
   `receive`; call `packets()` about every 20 ms and send every returned packet.
   Unacknowledged frames/hashes are returned on each call. ACKs are one-shot;
   a duplicate message generates another ACK. A temporary send failure is
   recovered by the next retransmission of its underlying message.
3. At a 12 Hz deadline, call `step(sim, world)`. It returns false without touching
   the simulation until both input frames for `t` and matching boundary hashes
   are present. On success it submits commands in side order (0, then 1), preserves
   each side's submission order, calls `world.begin_tick()` and `sim.tick(world)`,
   and publishes the hash at boundary `t + 1`. The first executed tick is tick 1
   in the existing simulation command log.

Use a monotonic clock and rational deadlines, e.g. `tick * 1 second / 12`, rather
than rounding every tick to 83 ms. Rendering and receive/retransmission pumps are
independent of simulation deadlines. A late peer stalls simulation; there is no
prediction, rollback, synthesized missing input or automatic peer removal.
Delay is 0..8 ticks (default 2, approximately 167 ms). Both peers explicitly send
empty frames for the initial `delay` ticks. Every subsequent tick also requires
an explicit frame, even if empty. Delay cannot change within a session.

The caller must give this layer exclusive control of simulation stepping and
command submission. Do not also run local AI/UI stepping or inject commands
directly into `GameSim`. Existing deterministic AI/scripts still run inside its
tick. `Lockstep::tick()` is the number of completed network ticks; do not attach
a fresh layer to a simulation already in progress.

`error()` stops advancement on conflicting input or hashes. `desync()` preserves
the boundary and local/remote hash for a mismatch, including boundary 0. Check
it after receiving and stepping: the remote hash can arrive before the local
step completes. `synchronized()` confirms the current boundary, including the
final boundary after a test/session stops stepping. Keep pumping at shutdown to
acknowledge final retransmissions. There is no reliable shutdown handshake;
the process test services packets for another 500 ms after final synchronization.
Applications must supply their own inactivity timeout, disconnect UI and session
lifecycle. A missing peer otherwise stalls indefinitely with bounded storage.

## Datagram encoding

All integers are unsigned little endian unless specified. No native structure
layout, padding, pointers or floating-point values appear on the wire. Maximum
UDP payload is 1200 bytes; there is no application fragmentation. The sender
must reject oversized input rather than silently truncate or split a tick.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | ASCII `QLS1` |
| 4 | 1 | Protocol version, 1 |
| 5 | 1 | Kind: 1 frame, 2 hash, 3 frame ACK, 4 hash ACK |
| 6 | 1 | Sender side, 0 or 1 |
| 7 | 1 | Agreed input delay, 0..8 |
| 8 | 8 | Session ID |
| 16 | 8 | Tick for frame/ACK, completed boundary for hash/ACK |
| 24 | variable | Payload below; UDP length determines its size |

A frame payload starts with a 16-bit command count (0..32). Each command has:

| Size | Field |
| --- | --- |
| 1 | Existing engine `Pct` value |
| 4 each | `a`, `b`, `c`, signed 32-bit two's complement |
| 2 | Object ID count, 0..128 |
| 4 each | Object IDs, preserving order |

The command side comes from the sender field; a payload cannot select another
side. Recognized types are `0x13..0x25`, `0x50` and `0x51`; interpretation and
ownership checks remain in the simulation. Counts and the 1200-byte total limit
both apply. A hash payload is exactly one 64-bit existing engine state hash.
ACKs have no payload and echo the acknowledged tick/boundary, using the ACK
sender's own side. Version, magic, session, side, delay, lengths, command types,
counts and receive window are validated before modifying protocol state.

## Reliability and bounds

Frames are immutable once sealed. A byte-identical duplicate is acknowledged
and ignored; conflicting retained input stops the session. Hash duplicates must
also agree. Frames and hashes are retransmitted until explicitly acknowledged.
A received boundary hash `h` also implicitly acknowledges all locally sent
frames and hashes with tick/boundary less than `h`: the peer could only execute
those ticks after receiving their input and comparing their boundary hashes.
ACKs themselves are never acknowledged. Reordering and duplicate ACKs are safe.

At local boundary `t`, receive frames through `t + delay + 1` and hashes through
`t + 1` only. The extra tick accommodates the remote peer advancing first. Old
valid messages are acknowledged without replaying commands or retaining history.
Retained local/remote frames, hashes and retransmissions stay bounded by the
delay and one boundary of progress; the pending ACK queue holds at most 32
entries. Decoding checks counts/lengths before allocations. The UDP adapter
discards oversized datagrams and datagrams from any endpoint except its configured
peer. Receive pumps should cap work per iteration to preserve scheduling.

This protocol assumes trusted LAN peers. Session IDs and endpoint filtering
prevent accidental cross-session traffic; they provide no authentication,
encryption or cheating resistance. State hashes detect covered differences;
matching hashes do not prove full game parity or complete hash coverage.
No discovery, NAT traversal, joining in progress, recovery snapshot or original
save synchronization is implemented. The socket adapter currently targets POSIX
systems; a native Windows socket adapter remains future work.

## Tests and verification

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build -R lockstep --output-on-failure
```

`tests/test_lockstep.cpp` runs two simulated peers with deterministic packet
loss, duplicates, delayed delivery and reordering. It checks input delays 0, 2
and 8, empty frames, command ordering, actual unit movement, stalled input,
malformed/truncated/oversized/foreign packets, conflicting input and state-hash
desyncs. Its final state and command log must equal a local ordered control run.

`tests/test_lockstep_processes.py` launches two `lockstep_peer` processes. Each
binds its own ephemeral port on `127.0.0.1`; the launcher exchanges ports over
stdin, avoiding the race of closing reservation sockets before binding. Each
process advances the synthetic engine for 24 ticks paced at 12 Hz, compares its
state/log against a local control, exchanges final hashes, and reports the same
final hash and command count. Socket failures and timeouts fail the test.
Processes are bounded by internal and CTest timeouts and killed on launcher
failure. Dummy SDL video/audio drivers are set; no display or game installation
is used, and no original game files are read or written.

In the implementation worktree on 2026-10-27, the simulated peer checks passed.
The two-process test failed at socket creation with `Operation not permitted`
under the sandbox. This is an unverified real-UDP check, not a passing network
result; rerun it on a host permitting IPv4 UDP sockets.

## Reference runtime observations and compatibility boundary

These are facts from the public, independently written
[`runtime/winsock.c`](../runtime/winsock.c), not a recovered original wire
specification:

- The host exposes a Winsock subset through `WS2_32.dll`, including startup,
  protocol/host lookup, socket/options, bind/address lookup, send/receive and
  `WSAIoctl` nonblocking mode. Startup advertises Winsock 1.0. Normal IPv4
  datagrams go through POSIX sockets, and errno values map to Winsock errors.
- The runtime needs networking even in single player. The reference experiment
  guide records a UDP bind on port 32008; parallel reference instances need
  network namespace isolation and separate writable installations.
- Guest AF_IPX requests use a host placeholder with self-loopback discovery and
  an IPX-over-UDP LAN path. The host maps the bound IPX socket number to UDP
  `34000 + (socket_number & 0x3ff)`. Its node address embeds the host IPv4 address.
  `RT_HOST_IP` and `RT_IPX_PEERS` are experiment controls for host addressing and
  explicit peer destinations. These mappings belong to the shim.
- `RT_ORACLE_UDP` substitutes a bounded in-memory single-player UDP queue. Its
  success cannot demonstrate communication between separate processes.

None of these shims establishes the original lobby, reliable command exchange,
packet contents or state-hash protocol. Original IPX/UDP wire compatibility is
out of scope: `QLS1` is a new engine protocol. No executable-derived source or
assets are used in this implementation. No reference game run is needed to
validate its synthetic transport behavior. Any future reference experiment
must still follow [EXPERIMENTS.md](EXPERIMENTS.md): private writable copies with
read-only asset symlinks, dummy SDL drivers, `unshare -rn`, and a hard timeout.
