# Script conditions and flow (trigger system)

Evidence date: 2026-11-15. Read-only: Ghidra corpus (`work/decomp/all_full.c`), extra
decompiles (`work/gaps/dec_417a90.c`, `dec_45f470.c`, `dec_401a10.c`) and `objdump` of
`game/Aztec.exe`. Nothing was run. **V** = read from code/data at the cited address,
**G** = inference. Complements `docs/script_gift_command.md` and
`docs/script_codes_and_ai_purchase.md`.

## 0. Premise correction

V: the words `await`, `checkpoint`, `count`, `outcome`, `loop`, `goto` do not exist in
the executable strings, and `await` / `checkpoint` do not occur in any shipped `.scn`
file (`game/Maps`, `game/Campaign` searched). There is no step sequencer, no timeout
number and no jump. The script is a flat list of **triggers**; every trigger has a
condition list and an action list:

    trigger(<id>)
       conditions
          side(local).towns == 0
       actions
          side(local).defeat
    end

Everything below describes this real grammar. The requested "await/checkpoint" semantics
have no counterpart; sequencing is done only with `trigger(n).enable/disable` and timers.

## 1. Parser

| Function | Role (all V) |
| --- | --- |
| `0x4367f0` | opens the `.scn` (mode `rt`), loops: read word (`0x436a30`, scanf `%[a-zA-Z_0-9]s`); the word must be `trigger` (`0x4ad2d0`) else error; calls `0x436ad0` |
| `0x436ad0` | one trigger: id via `0x436d90(file, 0)` (`( %i )`), allocates from pool `0x4cd698`, constructor `0x45f0e0(id)`; then words: `conditions` -> condition mode, `actions` -> action mode, `end` (`0x4abbcc`) -> finish, anything else -> rewind and parse one line in the current mode (`0x438f30` conditions, `0x436f00` actions). Initial mode is conditions. `end` with an empty action list is an error (message id `+0x19c`) |
| `0x436d90(file, allowLocal)` | `( %i )`, or `(local)` when allowed -> local player side `0x43d200()` |
| `0x4374f0` | one `%i` |
| `0x437370` | `( %i , %i )` or `(local, %i)` |
| `0x437260` | quoted string (`%[^"]s` between double quotes) |
| `0x439180` | comparison operator, see 2.2 |
| `0x436a90` | skips space, tab, newline; returns the next char |
| `0x4369e0` | line number for error reports (`0x475c20(4, file, line, text)`) |

Trigger object (pool `0x4cd698`, vtable `0x494878`), V: `+0x04` id, `+0x08` latched
condition result (0 at construction, `0x41e970`), `+0x0c` enabled byte (1 at construction,
`0x45efe0`), `+0x10` action list, `+0x14` condition list. Both lists hold records of
0x408 (1032) bytes: dword code, 0x100 (256) payload dwords, ready byte at `+0x404`.
Enqueue: actions `0x45f3b0(code, payload, ready)`, conditions `0x45f410(code, payload,
ready)` (argument order V from the disassembly of `0x45f410` and the call at `0x439163`;
Ghidra shows it wrongly). Every parser site read pushes ready = 1.

## 2. Conditions

### 2.1 Kinds

Dispatcher `0x438f30` (first word), `0x439340` (after `side(n).`), `0x439eb0` (after
`side(n).tactic(id).`). Evaluator: `0x418360(code, payload)` = call through the table at
`0x4a8e8c` (0x16 = 22 entries). Payload dwords: 0 side, 1 timer id, 2 UGI type index,
3 tactic/defence id, 4 value, 5 operator, 6 second side, 7..10 rect x1, y1, x2, y2.
All V unless marked.

| Code | Script form | Evaluator | Value compared |
| --- | --- | --- | --- |
| 0x00 | `always` | `0x417b40` | returns 1, no operands |
| 0x01 | `side(n).money OP v` | `0x417b50` | side `+0x698` |
| 0x02 | `side(n).tactics OP v` | `0x417b80` | side `+0x65c` (tactic count) |
| 0x03 | `side(n).towns OP v` | `0x417bb0` | side `+0x668` (town count) |
| 0x04 | `side(n).enemies OP v` | `0x417c50` | number of sides i with `0x447360(side, i)` (enemy) and (side i `+0x65c` != 0 or `+0x668` != 0) |
| 0x05 | `side(n).units OP v` | `0x417be0` | side `+0x680` |
| 0x06 | `side(n).unitsOfId(UGI_x) OP v` | `0x417c10` | side dword array `+0x144 + type*4`; type = index in the name table `0x4acb4c` |
| 0x07 | `localTime OP v` | `0x417ce0` | `DAT_004c1968`: day clock, from ini `StartTime`, +1 per call of `0x4184a0`, wraps to 0 above 0x1c1f (7199) |
| 0x08 | `globalTime OP v` | `0x417d00` | `0x43d1b0()` = `DAT_004c5730`, frame counter incremented in `0x43a850` |
| 0x09 | `timer(id) OP v` | `0x417d20` | timer value (`+0x08`); `0x401930` creates the timer (value 0, running) if it does not exist |
| 0x0a | `side(n).tactic(id).idle OP v` | `0x417d60` | 1 when tactic state `+0x20` is 0 or 1, else 0 |
| 0x0b | `side(n).tactic(id).attacked OP v` | `0x417dc0` | tactic virtual `+0xd0` |
| 0x0c | no script word found | `0x417e10` | side plan `+0x694` complete (`0x457380`); false if no plan |
| 0x0d | no script word found | `0x417e60` | side plan `+0x694` == 0 |
| 0x0e | `side(n).isFriendly(m) OP v` | `0x417ea0` | `0x4472e0(side n, m)` |
| 0x0f | `side(n).isNeutral(m) OP v` | `0x417ee0` | `0x447320(side n, m)` |
| 0x10 | `side(n).isEnemy(m) OP v` | `0x417f20` | `0x447360(side n, m)` |
| 0x11 | `side(n).tactic(id).valid OP v` | `0x417f60` | byte `+0xb0` of the object; 0 when not found |
| 0x12 | `side(n).tactic(id).locked OP v` | `0x417fd0` | virtual `+0xcc` == 2 |
| 0x13 | `side(n).unitsInRect(x1,y1,x2,y2) OP v` | `0x418030` | count, see below |
| 0x14 | `side(n).unitsOfUGIInRect(UGI_x,x1,y1,x2,y2) OP v` | `0x4181d0` | same, plus virtual `+0x6c` == type |
| 0x15 | `side(n).defence(id).valid OP v` | `0x417f60` | same evaluator as 0x11 |

- `n` and `m` accept `local`; `id` of a tactic also accepts `local` syntactically
  (`0x436d90(.., 1)`), defence id, timer value and rect take integers only.
- Tactic lookup is `0x447100` (locked tactics by ScenarioId, then towns by id, see
  `docs/script_codes_and_ai_purchase.md`). When nothing is found, 0x0a, 0x0b, 0x12 return
  false regardless of the operator; 0x11/0x15 compare 0 (so `valid == 0` is true).
- Rect count (`0x418030`): tiles times 0x30 (48) px, corners sorted to min/max, spatial
  query `0x46b460` on `0x4c4d18`; counts objects whose kind (virtual `+0x08`) is 0, 1, 6
  or 7, with alive byte `+0x5c` set and side (virtual `+0x44`) == n. Edge inclusivity is
  inside `0x46b460` (not read).
- Codes 0x0c, 0x0d: no `0x45f410` call site with these codes exists in the corpus (G:
  dead, or only present in saved state).

### 2.2 Operators and compare

V `0x439180` -> code; V `0x417a90(actual, value, op)`:

| Text | Code | Test |
| --- | --- | --- |
| `==` | 0 | actual == value |
| `>` | 1 | actual > value |
| `>=` | 2 | actual >= value |
| `!=` | 3 | actual != value |
| `<` | -1 | actual < value |
| `<=` | -2 | actual <= value |

Signed 32-bit compare. Any other code is a fatal report (`0x475c20`).

### 2.3 Evaluation and timing

- V: the simulation tick `0x4577b0` calls virtual `+0x08` of every object in pool
  `0x4cd698` once per tick (G: pool order = creation order = file order for triggers).
- V trigger update `0x45f0a0`: if enabled (`+0x0c`): **first**, if the latched result
  `+0x08` is non-zero, run all actions (`0x45f4e0` -> `0x402f80(code, payload)`, table
  `0x49b15c`); **then** latch = `0x45f470()`.
- V `0x45f470`: AND over the condition list, short-circuit on the first false; an empty
  list gives false (hence the explicit `always`).
- Consequences (V from the code order): actions run one tick after the conditions became
  true; a trigger **repeats every tick** while it stays enabled and its latch is set
  (there is no fired-once flag), so one-shot behaviour needs `trigger(self).disable`; a
  disabled trigger neither runs nor re-evaluates and its latch keeps its last value, so
  re-enabling a trigger whose latch was 1 fires it on its next update before any new test.
- There is no timeout or tick operand on a condition.

## 3. Flow control

There are no steps, loops or jumps. Flow is built from the following (all V; parsers
`0x438c50`, `0x4388e0`; codes from the `push` before `call 0x45f3b0`):

| Code | Script form | Handler | Effect |
| --- | --- | --- | --- |
| 0x1f | `trigger(id).disable` (push `0x438d63`) | `0x401c30` | `0x4467b0(id, kind 1)`, virtual `+0x30` = `0x45f010`: enabled = 0 |
| 0x20 | `trigger(id).enable` (push `0x438da4`) | `0x401c50` | virtual `+0x2c` = `0x45f000`: enabled = 1 |
| 0x05 | `timer(id).set(v)` (word `set` `0x4ad4cc`, push `0x438a87`) | `0x401cb0` | timer value = payload dword 0x0d (virtual `+0x14` = `0x41e9e0`) |
| 0x06 | `timer(id).stop` (push `0x438a34`) | `0x401c90` | timer byte `+0x0c` = 1 (paused) |
| 0x07 | `timer(id).start` (push `0x4389f3`) | `0x401c70` | timer byte `+0x0c` = 0 (running) |
| 0x08 | `timer(id).kill` (word `0x4ad448`, push `0x438ac4`) | `0x401ce0` | destroys the timer object if it exists (no creation) |

So "start (four variants)" in `docs/script_gift_command.md` is wrong: codes 5 to 8 are
set, stop, start, kill of a timer. The timer id is payload dword 0x0c.

Timer object, V: kind 3 in pool `0x4cd698` (vtable `0x48f2b4`, kind `0x401a00`), created
on first reference by `0x401930(id)` with value 0 and running. Update `0x401a10`: value
+= 1 per tick unless byte `+0x0c` is set. A `timer(id) OP v` condition after `kill`
therefore silently recreates the timer at 0, running.

Lookup `0x4467b0(id, kind)`: first pool object with that id and kind; with duplicate ids
only the first trigger is addressed (V).

## 4. Victory, defeat, mission end

- V: side `+0x140` is the outcome: 2 = playing (set in `0x446870`), 1 = won, 0 = lost.
- V: code 0x00 `side(n).victory` (`0x401a60`) sets 1 and code 0x01 `side(n).defeat`
  (`0x401aa0`) sets 0, each only while the field is still 2. The first one wins; later
  victory/defeat on the same side is ignored.
- V `0x4577b0`: after the tick it reads the **local** side's outcome; if != 2 it stores
  it in `DAT_00515420` (G: and leaves the frame loop). Then:
  - campaign (`DAT_004dd164` = mission number != 0) and outcome 1: once (flag
    `DAT_004dd160`), next = max(mission + 1, `0x43f550()`); if next < 0x10 (16) store it
    with `0x43f560(next)` and no video; otherwise `0x43f560(0x10)` and play `final.avi`
    (`0x4ae93c`).
  - otherwise, if `0x429020()` is true and outcome != 2: play `win.avi` (`0x4ae934`) for
    1, `lose.avi` (`0x4ae928`) for 0.
  - in the video cases `DAT_00515560` = 1, event `0x4c4d60` is set and the thread waits
    on `0x4c4d5c`.
- V: `0x434af0` (key handler) case 0x34 also sets the local outcome to 1 when its flag
  argument is set (G: cheat/debug key).
- A non-local side's outcome only changes that side's field; the only other reader of
  `+0x140` found is the score screen `0x41b4b0` (prints the value when non-zero).

## 5. Presentation commands

- `printString("text")`, code 0x19, parsed inline in `0x436f00` (push `0x4370fd`); the
  text is copied to payload byte 0x38. Handler `0x402df0` -> `0x462760`: 16-entry ring
  (strings `DAT_00515898[]`, write index `DAT_00515850`, read index `DAT_00515854`), old
  entry freed, string duplicated, expiry = `timeGetTime()` + 4000 ms (0xfa0) stored in
  `DAT_00515858[]`. All V. Wall-clock time, not ticks.
- `printStringR("text")`: V the string is parsed (`0x437260`, word `0x4ad320`) but no
  `0x45f3b0` call follows in `0x436f00` (the function has only two enqueue sites: 0x14
  and 0x19), so it produces no action. G: line for the other language build.
- `centerView(x, y)` (pair parsed by `0x437370`), code 0x14 (push `0x437078`), handler
  `0x402b00`: camera jump, see `docs/script_gift_command.md`.
- `UGI_CAMERA` is **not a command**. It is entry 0x59 (89) of the UGI name table
  (`0x4acb4c`; its pointer is at `0x4accb0`). Code 0x11 belongs to
  `side(n).buildCommand(...)` (`0x437fb0`): payload dword 0x0b = count, dwords 0x0d.. =
  type indexes; handler `0x402180` builds a plan object (pool `0x4d9b88`, `0x456da0`),
  adds every listed type available to the side (byte at side `+0x5e0 + type`,
  `0x456f20`) and, if at least one was added, replaces the side plan `+0x694`. V. The
  name loop in `0x437fb0` is seeded with index 0x59 at `0x4accb0`; its direction and
  range were not checked.
- `side(n).cameraActivate(a, b)`, code 0x13 (push `0x437c44`), handler `0x402330`: takes
  the side's camera slot (`0x4470a0`, list side `+0x684`, id hidden on the stack),
  creates an object of type 0x59 (UGI_CAMERA) from pool `0x4cc3d0` at the slot position
  (`0x43e890(side, x, y, 0x59)`, vtable `0x48f2d8`), field `+0x60` = clamp(slot dword 3
  + 0x3b, 0x40, 0x47), field `+0x64` = payload dword 0x0d, and stores it in slot `+0x10`.
  V for the code; G: a sight-revealing camera unit.
- `side(n).cameraDeactivate(id)`, code 0x21 (push `0x437c8f`), handler `0x402f10`
  destroys that object. `side(n).moveCommand(..)` is code 0x12 (push `0x437bef`, handler
  `0x4022e0`, not read).

## 6. Unknowns

- How the frame loop in `0x4577b0` exits after the outcome leaves 2, and what
  `0x429020()`, `0x43f550()`, `0x43f560()` are exactly (campaign progress get/set is G).
- Whether pool `0x4cd698` iteration order equals creation order (decides same-tick
  ordering between triggers and timers).
- Unit of `globalTime` versus the timer tick (`0x43a850` versus `0x4577b0` call ratio
  not traced) and the call rate of `0x4184a0` for `localTime`.
- Producers of condition codes 0x0c / 0x0d; rect edge inclusivity (`0x46b460`).
- Meaning of the two `cameraActivate` operands and of camera fields `+0x60`, `+0x64`.
- `buildCommand` name-table range; `moveCommand` handler `0x4022e0`.
- The third class in pool `0x4cd698` (vtable `0x4948b0`, kind 2, dword array at `+0x18`;
  G: the path object used by setPath) was not read.
- Not verified by a live run; the one-tick latch delay and the every-tick repeat are
  read from code order only.
