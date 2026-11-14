# Script command queue and the gift to side command (decompilation survey)

Evidence date: 2026-11-14. **V** = read from the Ghidra decompilation (function addresses
only), **G** = guess.

## Queue and consumer

- V: script handlers do not act directly; they enqueue a record with `0x45f3b0` (`0x45f410`
  is the twin for a second queue). A record is 0x408 bytes: command code (dword), 256
  dwords of payload, and a "ready" byte at record dword 0x101.
- V: `0x45f4e0` walks the queue (object field `+0x10`) and, for every record whose ready
  byte is non-zero, calls the dispatcher `0x402f80(code, payload)`.
- V: `0x402f80` is a plain table call: `table[code](payload)` with the table of 0x22
  entries at `0x49b15c` (codes 0x00 to 0x21, one handler address each). Entries read from
  the executable: 0x00 `0x401a60`, 0x01 `0x401aa0`, 0x02 `0x401ae0`, 0x03 `0x401b60`,
  0x04 `0x401bb0`, 0x05 `0x401cb0`, 0x06 `0x401c90`, 0x07 `0x401c70`, 0x08 `0x401ce0`,
  0x09 `0x401d00`, 0x0a `0x401e80`, 0x0b `0x401dc0`, 0x0c `0x401f40`, 0x0d `0x402000`,
  0x0e `0x402060`, 0x0f `0x4020c0`, 0x10 `0x402120`, 0x11 `0x402180`, 0x12 `0x4022e0`,
  0x13 `0x402330`, 0x14 `0x402b00`, 0x15 `0x402c40`, 0x16 `0x402c70`, 0x17 `0x402ca0`,
  0x18 `0x402d50`, 0x19 `0x402df0`, 0x1a `0x402cd0`, 0x1b `0x402d10`, 0x1c `0x402e10`,
  0x1d `0x402e70`, 0x1e `0x402ed0`, 0x1f `0x401c30`, 0x20 `0x401c50`, 0x21 `0x402f10`.

## Command 0x18: gift to side (handler `0x402d50`)

Payload: dword 0 is the giving side index, dword 0x0c the receiving side index (side
structs at `0x4dd2f4`, stride 0x69c).

1. V: `0x447100(giver side)` looks in the giver's object list (side field `+0x654`) for an
   object whose kind (virtual `+0xcc`) is 2, whose byte `+0xb0` is non-zero and whose id
   (virtual `+0x88`) equals the id passed by the script; if none is found it falls back to
   `0x447180(giver side)`. If nothing is found the command does nothing.
2. V: if the object's virtual `+0x80` returns 1 (a town), the handler calls
   `0x45da00(town, receiving side)`. That routine is the full side change of a town: it
   detaches the town's members (virtual `+0x7c`), clears the town's tactic and counters
   (`0x45cf80`, `0x45dd50`), creates a fresh tactic on the old side (`0x4474d0`), removes
   the town from the old side lists (`0x447220`), stores the new side at `+0x08`, adds the
   town to the new side lists (`0x446fe0`) and runs the per-member side reassignment
   (members virtual `+0xf0` or `+0xf4` depending on flags `+0x94`, `+0x9c`, `+0x9d`).
3. V: otherwise (any other object kind) it creates a new tactic in the receiving side
   (`0x4474d0(receiving side)`), adds the object to it (tactic virtual `+0xb8`) and sets the
   tactic's follow value from the object (tactic virtual `+0x84` with the object's virtual
   `+0x88` result). In other words the object joins a new group owned by the receiver.
4. G: step 2 matches the ownership mechanism already documented for town capture
   (`docs/ownership_mechanism.md`, `0x45b1e0` and `0x45a920`); the two paths probably end
   in the same `0x45da00` side change. Not confirmed by a live run.

## What this means for the clone

- The clone should model a script gift as an ownership change that creates a new group on
  the receiving side, with the town case running the full town side change (members move
  with it).
## Other handlers read (second pass)

Payload dword 0 is the acting side index in all of them below. **V** unless marked.

| Code | Handler | What it does |
| --- | --- | --- |
| `0x14` | `0x402b00` | Camera jump: tile x = payload dword 2, tile y = dword 3; the camera origin becomes `tile * 0x30 - viewport / 2`, clamped to zero and to the map size (`map * 0x30 - viewport - 4` horizontally, `- 1` vertically), under the render mutex |
| `0x15` | `0x402c40` | `0x4476b0(side, payload dword 0x0c)`: side relation call with a second side (G: set or change a relation or mark; not read) |
| `0x16` | `0x402c70` | `0x447710(side, payload dword 0x0c)`: the matching reverse call (G) |
| `0x17` | `0x402ca0` | `0x447770(side, payload dword 0x0c)`: third call of the same family (G) |
| `0x18` | `0x402d50` | gift to side, see above |
| `0x19` | `0x402df0` | `0x462760(payload + 0x38)`: takes a string at payload byte 0x38 (print or message path, G) |
| `0x1a` | `0x402cd0` | finds the side's designated object (`0x447100`) and calls its virtual `+0x104` |
| `0x1b` | `0x402d10` | same lookup, virtual `+0x108` |
| `0x1c` | `0x402e10` | adds payload dword 0x0d to the side's counter at side struct `+0x698`, floored at 0 |
| `0x1d` | `0x402e70` | subtracts payload dword 0x0d from the same counter, floored at 0 |
| `0x1e` | `0x402ed0` | same lookup, virtual `+0x13c` |
| `0x1f` | `0x401c30` | object from `0x4467b0(payload dword 0x0c, 1)`, virtual `+0x30` |
| `0x20` | `0x401c50` | same lookup, virtual `+0x2c` |
| `0x21` | `0x402f10` | takes the side's object from `0x4470a0`, finds its entry in the list at `0x4cc3d0` by pointer (`field +0x10`), destroys it (virtual `+0x04`, flag 1) and clears the field |

## Script command names per code (second look at the parser)

Evidence date: 2026-11-14. **V** for the pairing, taken from the parser functions
(`0x4375e0`, `0x437720`, `0x4378a0`, `0x4379e0`, `0x437fb0`, `0x4382c0`, `0x4388e0`,
`0x438c50`, `0x436f00`) that match a script word and then enqueue a code with `0x45f3b0`.
The pairing is read as "the last word matched before the enqueue"; entries marked ? are
weaker.

| Code | Script word | Code | Script word |
| --- | --- | --- | --- |
| `0x00` | victory | `0x11` | UGI_CAMERA |
| `0x01` | defeat | `0x12` | moveCommand |
| `0x02` | attackTown | `0x13` | cameraActivate |
| `0x03` | moveTo | `0x14` | centerView |
| `0x04` | joinTactic | `0x15` | setFriendlyTo |
| `0x05` to `0x08` | start (four variants) | `0x16` | setNeutralTo |
| `0x09` | setPath | `0x17` | setEnemyTo |
| `0x0a` | setCycledPath | `0x18` | giftToSide |
| `0x0b` | setInvertedPath | `0x19` | printString |
| `0x0c` | setCycledInvertedPath | `0x1a` | ? (parser also near setCycledPath) |
| `0x0d` | activate | `0x1b` | unlock |
| `0x0e` | ? (no word matched in `0x4378a0`) | `0x1c` | addMoney |
| `0x0f` | enable | `0x1d` | takeMoney |
| `0x10` | disable | `0x1e` | ? (second giftToSide variant) |
| `0x1f`, `0x20` | disable, enable (second form, `0x438c50`) | `0x21` | cameraDeactivate |

So V/G for the handlers above: 0x15 to 0x17 are the three relation commands
(friendly, neutral, enemy towards the second side, the handlers differ by the relation they
set); 0x19 is printString; 0x1c and 0x1d are addMoney and takeMoney, so the counter at
side struct `+0x698` is money (V by name, floored at zero); 0x21 destroys the camera
object (cameraDeactivate).
- Open: codes 0x0e, 0x1a, 0x1e (and which giftToSide variant maps to which of 0x18, 0x1e).
