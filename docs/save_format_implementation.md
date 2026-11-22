# Original profile and save format implementation

V = a layout fact recorded in the cited reference documentation, or an explicitly
identified property verified by synthetic implementation tests. G = an inference
that has not been checked by running the original. No original executable,
reference runtime, private save, profile or asset was run or used as a fixture
for this implementation. These labels do not establish gameplay parity.

The independent codec lives in `src/core/userfiles.*` and `src/core/savegame.*`.
It uses the layouts in [campaign menus and saves](campaign_menus_saves.md),
[save residuals](audio_saves_residual.md#6-in-mission-save-format-residuals),
[formats](formats.md#user-files) and the detailed [save grammar](savegame.md).
No executable-derived implementation is included.

## Profiles

| Field | Wire layout | Evidence |
| --- | --- | --- |
| Profile kind | One byte at `0x00`; zero single-player, nonzero multiplayer | V documented |
| Name | 67-byte field at `0x01`, including terminating NUL; maximum 66 name bytes | V fixed layout; V synthetic boundary checks |
| Campaign progress | Little-endian u32 at `0x44`, starts at 1; preserved without clamping | V documented; `PlayerFile::const_one` retains its legacy API name |
| Unknown creation field | Little-endian u32 at `0x48`, starts at zero | V documented; exposed as `reserved`, meaning unresolved |
| Options | 224 bytes at `0x4c`, through exact EOF at 300 bytes | V documented |

The options codec writes acknowledgement at relative `0x08`, speech style at
`0x0c`, SFX/music/speech volume at `0x94/0x98/0x9c`, frequency at `0xa0`, display
mode at `0xa4`, scroll/game speed at `0xa8/0xac`, IEEE754 binary64 gamma at
`0xb0`, and the three bytes at `0xb8/0xb9/0xba`. All integer words and gamma bits
are explicitly little-endian. Unknown options, name padding and the unknown
creation field survive a parse/serialize cycle. New profiles start with zeroed
unknown bytes rather than copying uninitialized memory. V: synthetic byte checks.

`parse_player_file` and `load_player_file` accept an optional English error
string and clear output on failure. Exact size and name termination are checked;
undocumented option values and arbitrary campaign progress are retained.
`serialize_player_file` returns empty on invalid names or retained-buffer size,
with an optional error. `write_player_file` validates before file replacement.
Names are byte strings; encoding conversion and path selection belong to callers.
Embedded NUL and names over 66 bytes are rejected rather than silently shortened.

## Original saves

V documented: `singleN.sav` and `multiN.sav` share a stream layout. The 344-byte
header contains two u32 words (mode and campaign mission), a 256-byte mission
path, a 64-byte description and eight little-endian u16 SYSTEMTIME members.
The legacy `SaveHeader::zero0/zero1` names remain source compatible. Header
serialization retains unused string tails. Maximum string lengths are 255 and
63 bytes; both fields require a terminating NUL.

V documented: signed view x/y and five global words precede the following
sections. The first ten have u32 record counts; UI and daylight have none.

1. Decorations
2. Towns
3. Sides
4. Tactics
5. Units
6. Weapons
7. Effects
8. Transports
9. Scripts
10. Orders
11. UI selection/panels
12. Daylight time/stage

V documented: body scalars always occupy four little-endian bytes, including
narrow flags, IDs, counts and references. Raw blocks can leave the next word
unaligned. There is no checksum, footer, compression or magic to invent.
Opaque BuildTree, research, script packets, fog, camera records and double
payloads stay byte sequences. IDs stay integers; the codec never loads pointers.

`serialize_savegame(game, bytes, error)` emits the decoded field stream, rebuilding
outer pool counts and offsets. `header`, `view_x`, `view_y`, `globals` and each
scalar `SaveField::value` supply the written values. Raw fields copy their
`offset/size` ranges from `game.bytes`. This supports byte-preserving round trips,
scalar edits and record insertion/removal when the resulting grammar and links
are valid. `SaveRecord::id/type/kind`, record offsets/sizes and section offsets/sizes
are inspection metadata, not a second editable copy of the stream. Edit ID/type
fields in `record.fields`. Parse serialized output again to obtain fresh ranges.

For example, after parsing a snapshot, update `game.header.description` or a
unit field's `value` at memory offset `0x38` to change the saved description or
current HP. Call `serialize_savegame` or `write_savegame`; leave opaque blocks
intact. Adding a conditional payload requires its complete field sequence and
matching count/presence words. The writer does not infer missing payloads from
an edited dispatch type. Raw field bytes can be edited in the retained buffer;
editing retained scalar bytes alone does not override decoded scalar values.

V construction: before returning bytes, the writer runs the structural parser
and the separate typed-reference validator over its output. Null/duplicate pool
IDs and dangling references are refused. Forward references and sparse IDs are
checked after all pools exist. The reader remains useful for structural
inspection of unresolved links; import and writing require resolved links.
This stricter policy can reject stale links the original might clear. Raw packet
words are not guessed to be references. Descriptor/animation resources and
unresolved gameplay invariants are not validated.

## Errors, limits and storage

V construction: profile and header parsing use temporary results, so aliased
retained-buffer input is supported. Failed reads clear output and set an error;
save errors include a byte offset. Short input, unterminated strings, unknown
dispatches, impossible counts, oversized matrices/fog and trailing bytes produce
failure without publishing a partial decoded save. Existing reader budgets remain
256 MiB per save, 16,384 counted entries, 16,777,216 array elements and 1,048,576
fields. Writers enforce the same limits and check raw ranges before copying.
These are resource policies, not claimed original game limits.

V construction: `write_player_file` and `write_savegame` use an exclusively
created temporary sibling, check writes/flush/close and synchronize its contents,
then replace the destination. Errors remove temporary output and preserve the
previous destination. No profile directories or slot filenames are selected
automatically. Callers supply a writable user-owned path; assets and the game
installation need not be writable. Native Windows replacement uses the existing
project's platform approach but was not exercised here. Directory metadata is
not synchronized for a power-loss durability guarantee.

## Validation and remaining dependencies

V synthetic: `test_savegame` constructs every fixture in memory. It checks
profile field positions, binary64 endianness, unknown-byte preservation, exact
300-byte size, 66-byte names, maximum/truncated strings, every short profile and
minimal-save prefix, dispatch/count failures, all 90 unit and 40 weapon types,
tactic/script variants, timer samples, active animations and maximum order queues.
Round trips compare every emitted byte, including nonzero opaque blocks and
header padding. Edits verify decoded HP, campaign/header fields, clocks and pool
counts. Disk checks cover replacement, fresh reads, invalid-model preservation
and missing-parent errors. Writer checks cover malformed field sizes/ranges,
section order, missing records, duplicate IDs and unresolved links.

Run the independent checks without private inputs:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
pre-commit run --all-files
pre-commit run --files src/core/save_io.hpp docs/save_format_implementation.md
```

G: acceptance of newly serialized files by the original is expected from the
documented grammar and synthetic byte round trips, but remains unmeasured.
No original or reference process was launched, as required by this task.

The existing engine import restores only its documented subset. The retained
snapshot does not follow later engine updates. Export of an advanced engine
world still depends on unresolved orders, production/research, script/timer,
diplomacy and other state mappings described in
[resume policy](savegame.md#resume-follow-up-evidence-and-policy). Those dependent
parts remain incomplete in this checkout; the format codec is independently
implemented here. No engine save conversion or new UI save mode is claimed.
