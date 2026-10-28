# Engine save format and safety

Engine saves are independent command-log replays, not original `.sav` files.
The pause menu and Ctrl+S/Ctrl+L use `Saves/engine_slot1.azs` for the current
mission. Loading a slot requires starting that mission; a fresh process can then
load it. Imported original-save worlds cannot use this replay format.

## Compatibility and encoding

New writes use AZS3. AZS2 and unknown format or replay versions are refused;
there is no automatic migration. `kSaveFormatVersion` describes the byte layout.
`kSaveReplayVersion` must change whenever engine simulation semantics or hashing
change incompatibly, even if the byte layout remains the same. This is an
explicit compatibility policy, not a guarantee that the pending V03 audit has
found every future-affecting state field.

All multibyte values are little-endian, without native padding. Camera values are
IEEE-754 binary32. The layout, in order, is:

| Field | Encoding |
| --- | --- |
| Magic | Four bytes `AZS3` |
| Format version, replay version | Two u32 values, currently 3 and 1 |
| Mission length, mission | u32 byte count, then ASCII basename bytes without a terminator |
| Tick, state hash, script hash, content hash | Four u64 values |
| Camera x, y, zoom | Three binary32 values |
| Command-log length, log | u32 byte count, then an AZC1 log |
| Integrity checksum | u64 over every preceding byte |

AZC1 contains its four-byte magic, a u32 command count, then records containing
u64 tick, u32 packed type/side, three signed 32-bit command arguments, u32 ID
count and that many u32 IDs. The packed type occupies bits 0-7, side bits 8-15;
upper bits must be zero. Signed arguments retain their two's-complement bits.

The integrity checksum and content fingerprints use the project's existing
64-bit FNV-1a recurrence (initial value 1469598103934665603, multiplier
1099511628211). They detect accidental changes; they are not authentication for
hostile files. Tests also repair checksums on malformed inputs to exercise the
parser independently of this integrity gate.

The UI fingerprints the resolved mission definition, selected terrain map,
optional passability/resource/visibility/script files, all CSV files in the
selected rules directory, and the optional sight inputs selected through
`AZTEC_GAME` (`Light.xbe`, `Units.xbe`, `Aztec.exe`). File tags, absence markers
and file digests participate; absolute installation paths do not. CSV entries
are sorted by filename. This deliberately conservative policy can refuse a save
when an unused CSV changes. These inputs must remain stable while a session is
loading/initializing. Before saving or loading, the UI rechecks them against the
session fingerprint and refuses changes; it does not hot-reload rules.

## Bounded, transactional loading

The reader checks file size before allocation and requires exact consumption,
including the checksum. Unknown commands, invalid sides, reserved packed bits,
zero/decreasing command ticks, commands after the save tick, trailing data and
truncated or oversized counts fail. Equal command ticks retain submission order.
Limits apply on writing as well as reading:

- Save file: at most 32 MiB + 512 bytes; command log: at most 32 MiB.
- At most 200,000 commands and 4,096 object IDs per command.
- At most 1,036,800 ticks (24 hours at 12 Hz), bounding replay work. Replay time
  still depends on mission complexity; saves restore by replaying from tick zero.
- Mission basename: 1-128 ASCII letters, digits, underscores or hyphens.
- Camera positions: finite, absolute value at most 10,000,000; zoom: finite,
  between 0.125 and 8 inclusive.

Failed deserialization leaves the destination unchanged. `replay_save` checks
mission, content and script identity, rejects script parse failure, and replays
in a private world. It returns success only if both tick and state hash match.
The UI then replaces the world/simulation and restores the camera. On refusal,
the active simulation, camera, audio and command UI remain intact. Replay
restores the currently implemented script outcomes, including victory, rather
than persisting a separate campaign profile. A matching partial state hash is
not evidence of complete campaign continuation or reference parity.

## Atomic writes

The writer validates and encodes before touching the slot. It exclusively creates
a unique sibling temporary file, writes all bytes, checks flush, synchronizes
file data (`fsync` / `_commit`), checks close, then atomically replaces the slot
with POSIX rename or Windows `MoveFileExW` with replacement/write-through flags.
The temporary and destination are on the same filesystem. Failures before
replacement preserve the previous slot and remove the writer's temporary file;
pre-existing temporary names are never opened for writing or removed.
A process crash can leave an orphan temporary file, which the reader ignores.
The POSIX parent directory is not synchronized, so survival of the new directory
entry across sudden power loss is not guaranteed. This is not a backup system.

## Evidence and remaining acceptance

`engine_save` checks every truncation, every single-bit corruption, structural
mutations with repaired checksums, fixed-seed fuzzing, version/limit rejection,
unchanged outputs, content identity, write/rename failures, script progression
and refusal of replay mismatch. `engine_save_processes` writes at 14,400 ticks,
exits, loads in a second process and compares the next 300 ticks against an
uninterrupted synthetic run. Camera and mission metadata roundtrip. On POSIX,
a file-size resource limit injects a real short write: the old slot must remain
byte-identical and the temporary file must be removed. These tests use no assets.

V16 remains open: V03's full state/ordering audit, every new state producer,
long campaign saves, frontend/progression acceptance and native Linux/Windows
campaign continuation still require evidence. The tests are registered with the
`unit` label for the existing platform CI matrix; registration is not a native
Windows test result. No original-game behavior is claimed by these checks.

Local validation (2026-10-28): Linux GCC Release build and CTest passed
(19 passed; three private-data checks and the UDP process check skipped).
The two engine-save tests also passed with Clang ASan/UBSan, with leak detection
disabled. GCC sanitizer configuration was unavailable because its runtime
libraries were missing; the Clang sanitizer build succeeded. All tracked-file
pre-commit checks and explicit checks of the new files passed using cached hook
environments. No private game or reference runtime was run for this work.
