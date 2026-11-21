# Descriptor CSV provenance (desc_*.csv)

## 1. How the CSVs were obtained
tools/extract_rules.py reads tables from a RUNTIME HEAP DUMP (work/heap1.bin, made by the script
command heapdump on the running original; base 0x49b000), not from the file. Per its header:
"descriptor arrays are filled at game start". docs/data_tables_audit.md confirms the layout
(table base, stride, record count) and that rules.json is generated from the CSVs.
File check (Aztec.exe .data): record 0 of every kind equals the CSV except the LAST word; records 1+
are all zero in the file. So records 1+ come only from the dump (startup writers below).
The last word of every record is a runtime heap pointer (0x10xxxxxx, localised name string);
it is not reproducible and not in any static writer. Treat as non-data (e.g. marine rec0 d18,
defence d67, headtruck d19, spider d18, bird d21, building d321, truck d16, zeppelin d16, cannon d15).

## 2. Writers and spot checks (immediates compared against the CSV, objdump of Aztec.exe + all_full.c)
Method: every "mov DWORD PTR ds:ADDR,imm" inside the table range, mapped to (record, word).

| Kind | Table, stride, records | Startup writer | imm stores | Match | Status |
| --- | --- | --- | --- | --- | --- |
| Marine | 0x4ac430, 0x4c, 16 | FUN_00432210 (0x432253..0x432b78) | 99 (corpus: 270) | 99/99 (corpus 261/270, see 3) | V |
| Defence | 0x4aa3b0, 0x110, 9 | static init 0x41b9d0 (0x41b9e7..0x41c691) | 77 (corpus 536) | 77/77, 536/536 | V |
| Headtruck | 0x4ab2b0, 0x50, 8 | FUN_0042b2f0 (0x42b32b..0x42b73d) | 59 (corpus 133) | all | V |
| Spider | 0x4ae378, 0x4c, 2 | FUN_00449b70 | 8 (corpus 18) | all | V |
| Bird | 0x49b370, 0x58, 3 | FUN_00406dc0 | 17 (corpus 42) | all | V |
| Building | 0x49b538, 0x508, 43 | code at 0x40a63c..0x40f36a (function not in corpus) | 421 | 421/421 | V |
| Zeppelin | 0x4bcd00, 0x44, 2 | code at 0x470de0..0x470e56 (not in corpus) | 9 | 9/9 | V (partial coverage) |
| Truck | 0x4bc738, 0x44, 1 | none needed: only record 0 (file) | 0 | rec0 = file | V |
| Cannon | 0x4a8e18, 0x40, 1 | none needed: only record 0 (file) | 0 | rec0 = file | V |

Spot checks (record, word, immediate = CSV): marine 1/d2 0xc=12, 8/d0 0x10=16, 15/d0 0x1e=30;
defence 1/d3 0x9c40=40000, 4/d3 0x7530=30000, 8/d3 0x61a8=25000; headtruck 1/d0 14, 4/d0 17, 7/d2 54;
spider 1/d0 22, 1/d2 54, 1/d4 12; bird 1/d3 3000, 2/d1 13000, 2/d3 13000;
building 1/d1 40000, 21/d0 305, 42/d1 25000; zeppelin 1/d0 26, 1/d1 14000, 1/d2 12.
Record counts agree: writers touch records 1..n-1 (marine 15, defence 8, headtruck 7, spider 1, bird 2,
building 42, zeppelin 1), record 0 comes from the file.

## 3. Differences and gaps
- Marine word d11 (record offset 0x2c): code stores dword 1 (records 1,2,3,5,8,9,10,14 ...), CSV has
  257 (0x101): another store sets the byte at +0x2d (audit lists +0x2d read once). Likely a later byte
  store; not located. G for that byte (9 cells in corpus compare).
- Many non-zero CSV cells (e.g. marine 159, defence 128, headtruck 81, spider 11, bird 22,
  building 678, zeppelin 8) are not direct immediate stores to the table address: the compiler writes
  them from a register (repeated constants like 0xa28) or from computed values (marine d1 etc. come from
  *(DAT_004c4f40+0x30c..0x32c), i.e. config/game-settings values, so those cells depend on loaded data
  and are NOT file constants). Not individually verified; the 15 + 16 corpus "nonimm" writes are of
  this type. Status G for those cells; the compared cells show zero mismatches.
- Zeppelin and building writers are not in all_full.c under the table symbols (found by objdump only);
  function entry addresses were not resolved (instruction ranges given above).
- The heap-pointer last word of each record is a runtime value; do not compare or embed.
- Dump-specific dynamic words: config-derived cells (marine records at stride 0x4c after +0x30c reads)
  reflect the dump session's settings.

## Correction (2026-11-21, from docs/descriptor_config_cells.md)

V: `DAT_004c4f40` is the string table, not a numeric configuration. Offsets `+0x30c` onward
are unit name strings that feed only the last word of each record (a heap pointer, so it
should be stored as a string id). Hit points, speed, reload and costs are not scaled by any
configuration. Marine word 11 equals 257 because `0x432210` stores the three flag bytes
(`+0x2c` = 1, `+0x2d` = 1, `+0x2e` = 0) with byte stores that the first scan missed. The
non-immediate cells are register stores of constants loaded once; no cell is copied from
another cell or computed from a table. Marine, Defence, Headtruck and Building records 1 and
up therefore reproduce from the start-up code alone.
