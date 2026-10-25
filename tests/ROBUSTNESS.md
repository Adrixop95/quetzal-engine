The `fuzz` CTest target uses synthetic fixtures for all core file loaders. It
checks valid inputs, every truncation boundary, every single-bit mutation, and
unaligned zero, signed-extreme and overflowing field values. Explicit regression
checks cover parser progress, nested count budgets, failed archive reopen,
non-terminated script views and oversized files. Temporary files are removed at
exit. No game assets or executable data are required.

Run with sanitizers in a build directory inside the worktree:

```sh
cmake -S . -B .build-sanitize -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build .build-sanitize --target test_fuzz
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir .build-sanitize -R '^fuzz$' --output-on-failure
```

The test has a 55-second CTest timeout. Internal loader budgets are centralized
in `src/core/loader_limits.hpp`: 256 MiB per binary file or music track, 1 MiB
per text input, 4 million decoded sprite pixels, 16 million grid cells, and
16,384 aggregate mission records or music table entries. Archive offset tables
share a 256 MiB budget across groups. Inputs exceeding these limits fail through
the existing APIs. WAV data payloads retain the documented clamping behavior
for partial track reads.
