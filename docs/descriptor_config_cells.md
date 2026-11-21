# Descriptor config cells: resolution

Status: V = verified by disassembly/script this session, G = guess.

## 1. DAT_004c4f40 is the string table, not a numeric config (V)
- Allocated in entry (0x004312e0): HeapAlloc(0x1000) = 1024 dword slots, each pre-filled with the
  pointer to "No string provided" (0x4ac328). Then FUN_004317e0("res\strbase.bds") parses the file
  (marker lines *ORDINAL n and *LANGUAGE blocks, ENGLISH or RUSSIAN) and stores for each ordinal a heap
  copy of the text (FUN_00477be0 alloc + lstrcpyA) at slot[ordinal*4].
- Source file: game/Res/strbase.bds (2611 lines, shipped, constant). No ini section is involved
  (aztec_settings.ini has only resolution/window_mode/smooth/video). Nothing in the session changes
  the file content; only the pointer VALUES depend on heap addresses (0x10xxxxxx), hence not reproducible.
- Offsets +0x30c.. onward are slots 0xc3.. (ordinal 195.., the unit names). They feed ONLY the last
  word of each descriptor record: marine rec k word d18 (0x4ac478 = rec0 d18, 0x4ac4c4 = rec1 d18,
  stride 0x4c, reads +0x30c,+0x310,... in FUN_00432210, 16 records). Same pattern: defence d67 from
  [+0x54..+0x74] (static init 0x41b9d0, 8 reads), headtruck d19 from [+0xf8..+0x10c] (FUN_0042b2f0,
  7 reads), building d321 from [+0x2a8..] (writer at 0x40a600, 43 reads), spider/bird/zeppelin alike.
- So the earlier note "marine d1 etc. come from DAT_004c4f40+0x30c" is wrong: the config values never
  scale HP, speed, reload or cost. They are localized name strings (pointers). The "nonimm" marine
  cells d1..d17 are plain constants. Not verified: the exact text of ordinal 195 (strbase.bds
  could not be grepped by ordinal in one try, G for which name maps to which slot).
- Practical rule for the reimplementation: store a string id (slot index) in the last word, never a
  pointer; the CSV last-word values (0x10xxxxxx) are noise.

## 2. Marine d11 = 257 (V)
FUN_00432210, record 1: BYTE stores `mov BYTE PTR ds:0x4ac4a8,bl` (bl=1), `...4a9,bl` (=1) at 0x43228d and
0x432293, then `mov ds:0x4ac4aa,al` (al=0) at 0x432299. d11 sits at rec offset 0x2c, so bytes
+0x2c=1, +0x2d=1, +0x2e=0, +0x2f=0 give 0x0101 = 257. The earlier "dword 1" came from missing
the byte-store form. Every record repeats the same 3 byte stores (0x4ac4f4/f5/f6 for rec2, etc.). The
audit's "+0x2d read once" refers to the reader of the second flag byte.

## 3. Non-immediate cells: classification (V, script m1_b.py = linear emulation of objdump stores)
Cause of the large "non-immediate" counts: the compiler loads a constant into a register once
(mov eax,0xa28 / mov ebx,1 / xor eax,eax / or ecx,0xffffffff) and then stores the register, often with the
short opcode form `mov ds:ADDR,eax` (which the earlier immediate-only scan skipped). All are
straight-line constants, none is copied from another cell or table, none depends on config.

| Kind | Writer | imm stores | register-const stores | string-table stores | CSV nonzero cells recs 1+ (excl. last word) reproduced | Mismatches |
| --- | --- | --- | --- | --- | --- | --- |
| Marine | FUN_00432210 | 99 | 201 | 16 (d18) | 243/243 | 0 |
| Defence | static init 0x0041b9d0 | 77 | 120 | 8 (d67) | 197/197 | 0 |
| Headtruck | FUN_0042b2f0 | 59 | 74 | 7 (d19) | 132/132 | 0 |
| Building | function at 0x0040a600 (no corpus symbol; ends ~0x40f36a) | 421 | 2417 | 43 (d321) | 1057/1057 | 0 |

Samples (address, record/word, register source, value = CSV):
- Defence: 0x41ba78 r1 d19 <- ecx(=1, set 0x41b9da); 0x41ba9d r1 d26 <- esi(=2, 0x41b9df);
  0x41babe d32, 0x41bac4 d33, 0x41baca d34 <- esi=2; 0x41bad0 d35, 0x41bae5 d39, 0x41baeb d40,
  0x41baf1 d41 <- ecx=1; 0x41bb5b d60 and 0x41bb61 d61 <- edx=-1 (or edx,-1); 0x41ba97 d25, 0x41baa3 d27 <- ecx=1.
- Headtruck: 0x42b394 r1 d14 <- ebx=0x80 (0x42b317); 0x42b39a d15 and 0x42b3a0 d16 <- edx=0x100;
  0x42b3a6 d17 <- esi=8; 0x42b3ac d18 <- edi=0xd; 0x42b3e1/0x42b3fb r2 d3,d6 <- ebp=0x3a98 (15000);
  0x42b42e..0x42b44c r2 d13..d18 same registers; r1 d3,d6 = 20000 and the d10..d12 = -1 cells via
  eax=0x4e20 (0x42b305) and `or eax,-1` (0x42b314) with short-form stores.
- Building (all rec 1): 0x40a678..0x40a6c6 d6..d19 <- edx=0 (xor, 0x40a609) except 0x40a6b4 d16 <- ebx=1
  (0x40a612); later records use the same few registers, e.g. rec 4 d2,d3 = -87, rec 9 d320 = 20,
  rec 12 d0/d64 = 300, rec 14 d60/d62 = 30 (all reg-const).
- Marine: 0x43224d r1 d0 <- ebp=2; 0x43227b..0x432287 r1 d8..d10 <- ecx=-1; 0x43229e d12 <- esi=0x100;
  0x4322bf d17 <- ebx=1; d1,d3,d4 = 0xa28 (2600) <- eax at 0x432227.
Cells with zero code value but nonzero CSV: 0 (after counting short-form stores). Note the linear
emulation ignores control flow; it matched with zero mismatches, so the writers are straight-line.

## 4. Reproducibility without a running process (V)
- Marine, Defence, Headtruck, Building records 1+ : every numeric word (including byte flag words) is
  reproduced exactly from the startup code alone (0 mismatches, 0 uncovered nonzero cells). Record 0
  comes from Aztec.exe .data.
- Not reproducible: the last word of each record (string pointer, runtime heap address); take the
  slot index instead. Whether the Russian build writes other slots: only language switch, no numeric change (G).
- Script: /mnt/c/Users/adria/m1_b.py (not in the repo; to be deleted by the caller).
