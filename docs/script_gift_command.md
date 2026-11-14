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
- Unread: the neighbouring handlers 0x14 to 0x1e (for example 0x17 `0x402ca0` calls
  `0x447770(side, dword 0x0c)`, 0x1b `0x402d10` forwards to virtual `+0x108` of the found
  object); the mapping of script command names to codes beyond 0x14, 0x18, 0x19 is open.
