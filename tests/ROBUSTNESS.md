The `fuzz` CTest target uses synthetic fixtures for every core file loader. It
checks valid inputs, every truncation boundary, every single-bit mutation, and
unaligned zero, signed-extreme and overflowing field values. File-loading
wrappers are exercised as well as in-memory parsers. Temporary files are removed
at exit. No game assets, original executable bytes or executable code are used.

| Input family | Coverage |
| --- | --- |
| XBE, palettes and sound banks | Archive tables, offsets, failed reopen, palette payloads, sound sets and subtitles |
| Ordinary and alpha sprites | Raw 8/16-bit pixels, RLE control/payload streams, preview file loading and decoded pixel budgets |
| Terrain, resource, passability and visibility maps | Truncated dimensions/payloads, extreme dimensions, file and memory entry points |
| Mission INI, scripts, strings, briefings, keys and settings | Text mutations, count budgets, numeric overflow, non-terminated script views and file wrappers |
| Player files, original save headers and containers | Truncations, byte/field mutations, section failures and file wrappers |
| WAV and music archives | Chunk sizes, music table mutations, actual track reads and invalid 64-bit ranges |
| Engine saves and command logs | Length/count mutations, trailing data, invalid command tags, non-finite camera fields and failure output handling |
| Engine resource adapters | Mission path indices, animation slots/frames/durations, light masks and invented PE headers for daylight loading |
| Optional rules and playtest input | Extreme/malformed CSV descriptor fields, overflowing speed conversion, text mutations and oversized text rejection |

`map_viewer`, `playthrough`, `save_import`, `portability`, renderer and engine
unit tests additionally exercise composed loading paths with synthetic inputs.
This deterministic corpus is regression coverage, not exhaustive fuzzing or
proof that all malformed files are safe.

Run with sanitizers in a build directory inside the worktree:

```sh
cmake -S . -B build_sanitize -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DAZTEC_SANITIZERS=ON -DAZTEC_GEN_DIR= -DGAME_DIR=
cmake --build build_sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build_sanitize -L unit --output-on-failure
```

`AZTEC_SANITIZERS` instruments all independent libraries, tools and tests with
ASan/UBSan and disables recovery, so sanitizer findings fail CTest. GCC and
Clang are supported; other compilers fail configuration when the option is on.
The default build remains unchanged. The dedicated Ubuntu CI job runs all
synthetic `unit` tests, including the malformed-input corpus, without private
inputs. Socket-process checks, game-data tests and the longer AI soak are
outside that job's selection; they remain separate release requirements.

The `fuzz` test has a 55-second CTest timeout. Internal loader budgets are
centralized in `src/core/loader_limits.hpp`: 256 MiB per binary file or music
track, 1 MiB per text input, 4 million decoded sprite pixels, 16 million grid
cells, and 16,384 aggregate mission records or music table entries. Archive
offset tables share a 256 MiB budget across groups. Optional CSV and playtest
text also use the text budget. Mission path indices must fit the record budget.
Engine saves reject oversized mission names, embedded NULs, non-finite camera
values and trailing bytes; logs reject impossible counts and unknown tags.
Failed save decoding preserves the caller's previous save; failed log decoding
leaves an empty log. WAV payloads retain clamping for partial track reads.

Local validation on 2026-10-28 used GCC Release and Clang ASan/UBSan builds.
The corpus ran 117,068 deterministic mutations plus explicit regressions. UBSan
found descriptor speed multiplication overflow, fixed by validating the range
before fixed-point conversion; footprint rounding now uses wider arithmetic.
Additional validation fixes bound sparse mission path ids, validate daylight
header/section ranges, clear daylight keys on reopen, and reject malformed
engine save/log framing without exposing partial decoded state.

LeakSanitizer's thread inspection fails in this restricted sandbox. Local
ASan/UBSan runs therefore used `ASAN_OPTIONS=detect_leaks=0`; this is not leak
verification. CI retains `detect_leaks=1` and still needs a hosted run. Private
game-data and socket-process tests skip here; those skips do not establish
release acceptance, campaign parity, or completion of roadmap V20.
