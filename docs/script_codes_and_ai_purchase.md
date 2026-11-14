# Script queue codes 0x0e, 0x1a, 0x1e and the AI purchase gate

Evidence date: 2026-11-14. Read-only survey of the Ghidra corpus (`work/decomp/all_full.c`),
extra decompiles (`work/gaps/dec_0042fbb0.c`, `work/gaps/dec_0045e0d0.c`) and `objdump`
disassembly of the executable (`work/gaps/da_parsers.txt`). Nothing was run.
**V** = read from code or data at the cited address. **G** = inference.

## Part 1. Script codes 0x0e, 0x1a, 0x1e

### How the code number was recovered

V: `0x45f3b0` takes the command code as a stack argument that Ghidra shows as
`unaff_retaddr`, so the decompilation hides it. The disassembly shows the `push <code>`
right before each `call 0x45f3b0`, preceded by the string compare (`0x48a450`) of the
script word. That gives an exact word per code, replacing the "last word matched" guess in
`docs/script_gift_command.md`.

### Result

| Code | Script word (string address) | Parser, enqueue site | Script form | Payload |
| --- | --- | --- | --- | --- |
| `0x0e` | `go` (`0x4ad3a8`) | `0x4378a0`, push at `0x437908` | `submarine <id>.go` | dword 7 (byte `0x1c`) = id from `0x436d90` |
| `0x1a` | `lock` (`0x4ad464`) | `0x4382c0`, push at `0x438657` | `side <n>.tactic <id>.lock` | dword 0 = side index, dword 1 = tactic id |
| `0x1e` | `kill` (`0x4ad448`) | `0x4382c0`, push at `0x43874a` | `side <n>.tactic <id>.kill` | dword 0 = side index, dword 1 = tactic id |

Related corrections from the same pass, all V:

- `0x1b` = `unlock` (`0x4ad45c`, push at `0x4386a3`); `0x18` = `giftToSide`
  (`0x4ad450`, push at `0x438706`, single variant; the receiving side is parsed with
  `0x436d90(.., 1)` into payload dword 0x0c). There is no second giftToSide variant: the
  record previously guessed as that is `kill`.
- `0x0d` = `boat <id>.activate` (`0x4375e0`, word `0x4ad38c`).
- The top-level action dispatcher `0x436f00` selects the sub-parser by object word:
  `side` (`0x4ad36c`) -> `0x4379e0`; `trigger` (`0x4ad2d0`) -> `0x438c50`; `boat`
  (`0x4ad364`) -> `0x4375e0`; `submarine` (`0x4ad358`) -> `0x4378a0`; `johnka`
  (`0x4ad350`) -> `0x437720`; `timer` (`0x4ad348`) -> `0x4388e0`; `centerView`
  (`0x4ad33c`) inline. Inside `0x4379e0` the word `tactic` (`0x4ad430`) calls `0x4382c0`
  and `buildCommand` (`0x4ad420`) calls `0x437fb0`.

### Lookup used by 0x1a, 0x1b, 0x1e (`0x447100`)

- V: the tactic id is passed on the stack (again hidden as `unaff_retaddr`). `0x447100`
  walks the side's tactic list (side `+0x654`, filled by `0x446fa0`) and returns the first
  tactic whose virtual `+0xcc` returns 2, whose byte `+0xb0` is non-zero and whose virtual
  `+0x88` (`0x408b30`, returns field `+0xec`) equals the id. Otherwise `0x447180` walks the
  town list (side `+0x660`, filled by `0x446fe0`) and matches on id only.
- V: virtual `+0xcc` is `0x408b90`: returns 2 when byte `+0xe9` is set, else 0 when state
  `+0x20` is 0 or 1, else 1. So only tactics with byte `+0xe9` set are reachable by
  script id; towns are always reachable.
- V: field names from the tactic debug dump `0x44e990` (format strings at `0x4ae4c0`):
  "Locked %i, Lost %i, ScenarioId %i". G (by position in the dump): `+0xe9` = Locked,
  `+0xec` = ScenarioId. The map loader `0x44eb40` reads both from the file.

### Handlers

All three slot functions are identical in the tactic vtable `0x4944f0`, the town vtable
`0x494648` and the boat vtable `0x48fbd4` (V, vtable dumps).

| Code | Handler | Virtual | Function | Effect |
| --- | --- | --- | --- | --- |
| `0x1a` lock | `0x402cd0` | `+0x104` | `0x408c60` | V: sets byte `+0xe9` = 1 |
| `0x1b` unlock | `0x402d10` | `+0x108` | `0x408c70` | V: sets byte `+0xe9` = 0 |
| `0x1e` kill | `0x402ed0` | `+0x13c` | `0x452be0` | V: see below |
| `0x0e` go | `0x402060` | `+0x14c` of kind 3 | `0x42fbb0` | V: see below |

- Lock meaning, V: the AI plan builder `0x44d130` skips tactics whose `+0xcc` is 2 when it
  gathers reusable troops, and the plan code itself calls `+0x104` on every tactic it
  creates for reserved units (`0x456f50`, `0x457000`). So a locked tactic is reserved:
  not taken by AI plans, and addressable by script. Consequence, V: after `unlock` a plain
  tactic no longer passes the `0x447100` filter, so a later `lock`/`kill`/`giftToSide` on
  the same id falls through to the town list and does nothing unless a town has that id.
- Kill (`0x452be0`), V: while the member list (`+0xc`) is non-empty, call the first
  member's virtual `+0x128`; then for every child tactic in list `+0xa0` whose bytes
  `+0x94`, `+0x9c`, `+0x9d` are all non-zero, call its `+0x13c` recursively. For unit
  classes `+0x128` is `0x43ec60` (same address in 14 unit vtables, e.g. `0x4948e8`): hit
  points `+0x38` = 0, virtual `+0x5c(0)` unless already flagged, removal from the side
  (`0x449ac0`) and from the tactic (tactic virtual `+0x12c`), alive byte `+0x5c` = 0, dead
  byte `+0x5e` = 1. No damage routine and no attacker are involved. For a town the same
  function kills every member of the town.
- Go (`0x402060` -> `0x42fbb0`), V: the handler scans the global tactic pool `0x4d9b50`
  for the last object whose virtual `+0x80` returns 3 (class with vtable `0x492fdc`,
  map-load case 3 in `0x4455f0`) and whose id equals payload dword 7, then calls `+0x14c`.
  `0x42fbb0` destroys the object stored at `+0xf0` (virtual `+4`, flag 1), creates one
  object from pool `0x4c1978` with `0x4189d0(pos, 0, 0x1b, 1, 0)` at the tactic tile
  (`+0x10`, `+0x14`) times 0x30 plus 0x18, and removes the tactic from its side
  (`0x447d70`). G: this is the submarine surfacing or departure effect; id `0x1b` in the
  `0x4c1978` namespace is not identified.
- Kind values of virtual `+0x80`, V: 0 plain tactic (`0x402430`), 1 town (`0x45f030`),
  2 boat (`0x403870`), 3 submarine (`0x401a00`), 4 class `0x492e90` (`0x446310`),
  5 class `0x49313c` (`0x430650`). `0x0d` (`0x402000`) picks kind 2 and calls `0x408d50`
  (state `+0xf0` = 3).

### Unknowns, part 1

- What the pool `0x4c1978` object id `0x1b` is, and what `+0xf0` of the submarine holds.
- Whether any shipped mission uses `submarine`, `lock` or `kill` (campaign scripts not
  searched here).
- Byte `+0xb0` of a tactic (required by `0x447100`) is unnamed.

## Part 2. AI purchase and production decision

### Call path (all V)

- Side update `0x449980`: increments the side tick counter `+0x13c`; for a computer side
  (`0x43d2c0`) and when flag `0x2f` is off (`0x42e8c0`): progresses the active side plan
  (`+0x694`) through `0x457000`, rebuilds intelligence when
  `tick % ((side index + 1) * 1000) == 0` (`0x432050`), then target selection `0x44dbe0`
  -> `0x44cfe0`.
- Town update `0x45e8e0` (town vtable `+0x138`): production `0x459bb0`, capture/threat
  `0x45b1e0`, then the strategist `0x45e7c0` (skipped when flag `0x2f` is on).
- Strategist `0x45e7c0`: computer sides only. It sets the speed, then runs one phase
  handler from the table at `0x4ae9e8` indexed by town `+0x180`, **only if the town has no
  active production (`+0x110`) and an empty queue ring (`+0xf8`)**. Then, if the counter
  `+0x17c` is not -1, it increments it.

### Phase table (`0x4ae9e8`, 24 entries, V)

| Phase | Handler | Rule |
| --- | --- | --- |
| 0 | `0x45dda0` | if side `+0x694` (active side plan) is 0 -> 10, else wait |
| 10 | `0x45ddd0` | if counter `+0x17c` <= `(dev*5+5)*20` and `+0x18c` == 90 -> 13, else -> 3 |
| 3 | `0x45e090` | if counter > 0: DevelopScience `0x45b5f0` ok -> 4; fail -> counter = 0, -> 13 |
| 4 | `0x45e280` | -> 13 |
| 13 | `0x45de20` | count alive members with virtual `+0xfc` == 0; count >= `dev*3+3` -> 9, else -> 15 |
| 15 | `0x45dff0` | create local plan `0x44d130(own record, 0)`; ok -> 14, else -> 9 |
| 14 | `0x45e340` | economy check, progress plan; when complete -> 9 and the plan tactic joins the town (`0x457410`) |
| 9 | `0x45df50` | if `+0x15c` != 0 and target `+0x184` exists and belongs to another side -> 1, else -> 11 |
| 1 | `0x45dfa0` | create offensive plan `0x44d130(target, 1)`; ok -> 2, else -> 11 |
| 2 | `0x45e2a0` | economy check, progress plan; when complete launch (`0x4573b0`, tactic virtual `+0xd8` to the target) -> 11 |
| 11 | `0x45df10` | if economy flag `+0x170` and harvest < 2 * spend -> 7, else -> 12 |
| 7 | `0x45e230` | economic request `0x41e610`, else `0x41e850`; both fail clears `+0x170`; always ends in 8 |
| 8, 6 | `0x45e290` | -> 11 |
| 12 | `0x45de90` | if `+0x171` == 0 -> 11; else count alive members with `+0xfc` != 0; count >= dev -> 0, else -> 5 |
| 5 | `0x45e0d0` | queue a random available type from list `0x4cd838` (count `0x4cd6c0`) and place it; ok -> 6; fail -> 0 and `+0x171` = 0 |
| 16 | `0x45e3e0` | foreign object within 8 tiles (`0x44cc00`)? yes -> 21; no -> drop plan, -> 0 |
| 21 | `0x45e440` | count (as phase 13) >= `(dev+1)*7` -> 20, else -> 23 |
| 23 | `0x45e530` | create plan against own record, mode 1; ok -> 22, else -> 20 |
| 22 | `0x45e770` | progress plan; complete -> 20, join town |
| 20 | `0x45e4c0` | if `+0x171` and defence count < dev -> 18, else -> 16 |
| 18 | `0x45e5e0` | as phase 5; ok -> 19; fail -> 16 |
| 19, 17 | `0x45e760`, `0x45e750` | -> 16, -> 21 |

V: phases 16 and up are entered from `0x45b1e0`, which sets phase 16 when the current
phase is below 16 (threat branch) and resets to 0 at its end condition.

### Numeric gates

Money (V unless marked):

- There is **no reserve threshold for unit purchases**. A plan request is queued without a
  money test (`0x457000` -> `0x45cec0`); the only test is at production start in
  `0x459bb0`: `0x447d00(side, price)` fails when side money `+0x698` < price, otherwise
  subtracts it (and adds to side `+0x640` and town spent `+0x1b4`). An unaffordable head
  of queue blocks the town, and a blocked town runs no strategist phase.
- Science is the one branch with a pre-check: `0x45b5f0` forces speed 1 and queues only if
  side money >= price.
- Price `0x45c5b0` = `table[type] * factor * mult + 0.5`, table at `0x4bc3e8` (first
  entries 100, 160, 250, 450, 750, 500, 280, 800, 1000, 1200, 0, 1500, 240, 150, 450).
  `mult` = 0.8 for a computer side that is not all-enemies (side `+0x64c` != -1,
  `0x447320`), else 1.0. `factor`: town `+0x15c` == 0 -> 1.0; developed and all-enemies
  -> 2/3; otherwise by speed `+0x158`: speed 1 -> 1.0, speed 2 -> 2.0 (`0x4bc3cc`).
- Time `0x45c690` = `table[type] * factor * mult + 0.5`, table at `0x4bc550` (100, 120,
  140, 220, 250, 350, 160, 250, 300, 350, 0, 160, 180, 110, 250). `factor`: `+0x15c` == 0
  -> 1.5; developed and all-enemies -> 2.0; otherwise speed 1 -> 1.0, speed 2 -> 2/3
  (`0x4bc3c0`).
- Speed choice in `0x45e7c0`: harvest = `((side money / town count) << 8) / (side ticks
  + 1) + town +0x174` (`0x45c270`, `0x45c110`); spend = `(town +0x1b4 << 8) / (side ticks
  + 1)` (`0x45c300`). If harvest < spend * 2.0 (`0x4bc3d4`) -> speed 1, else speed 2.
- Economy gate (phases 2, 14, 11): same two quantities, integer test harvest < 2 * spend,
  and byte `+0x170` set. Request `0x41e610`: per race (`+0x164` = 0/1/2) types
  0x0c/0x27/0x41 and 0x12/0x28/0x46; if the town has no second-type building and already
  has **3 or more** of the first type, queue the second type; otherwise queue the first.
  Fallback `0x41e850`: type 0x0d/0x30/0x42.

Timers:

- Counter `+0x17c`: starts at 50 (`0x4594f0`); +1 per strategist call; set to -1 when a
  non-defence building is queued (`0x45cec0`), which freezes it; set to 0 when such a
  building completes (`0x459bb0`), is cancelled (`0x45cfd0`, `0x45a880`) or when science
  fails (`0x45e090`).
- Development timer (phase 10): military path while counter <= `100 * (dev + 1)` and no
  pending recommendation (`+0x18c` == 90); otherwise the science/building branch.
- Intelligence rebuild every `(side index + 1) * 1000` side ticks (`0x449980`).

Caps:

- Development level `+0x178` (`0x45d070`): sum of weights (`0x45c780`, cached at
  `0x515580`) over alive and queued non-defence buildings, clamped to **9**. Weights: 0
  for types 0x0a, 0x0c, 0x0d, 0x12, 0x25, 0x27, 0x28, 0x30, 0x3f, 0x41, 0x42, 0x46; 1 for
  0x0b, 0x13, 0x18, 0x26, 0x2f, 0x40, 0x47, 0x4c; otherwise (hall level column != 0 ? 1
  : 0) plus the weights of the prerequisites in the build tree at `0x4af130`; 10 when not
  in the tree.
- Force thresholds: local plan when count < `3 * dev + 3` (phase 13); threatened town
  plan when count < `7 * (dev + 1)` (phase 21); defence when defence count < dev.
- Plan composition `0x44d130`: at most **256** plan entries (`0x456f20`, `0x456f50`); at
  most **32** reusable units gathered per type; total requests stop at the **sum of the
  target record's 90 type counts**; each pick adds `rand() % (max side dev + 1) + 1`
  entries (`0x447290` gives the maximum town dev of the side); type 0x21 is excluded;
  scores get `rand() & 0x3f`, the second (virtual `+0xa4`) pass `rand() & 0x1f` and one
  request per enemy of that class.
- Allocation `0x457000`: per pass, each town with empty production, empty queue and phase
  in {0, 2, 14, 22} takes one request it can build. No money test.
- Target selection `0x44cfe0` / `0x44cd30`: per town, minimum score over known records of
  non-allied sides, start value 0x7fffff00; records of a computer side that is not
  all-enemies get +0x300 (3.0 in 8.8). The full score (distance over map diagonal and
  force terms) was not reduced to a formula.

### Unknowns, part 2

- How often `0x45e8e0` runs per town (every simulation tick is G; the caller was not
  traced), so the counter unit is "strategist calls".
- Whether the phase 13/21 count (`+0xfc` == 0 over the member list) includes non-defence
  buildings as well as units (the test itself is V; unit `+0xfc` is `0x402950`).
- Exact fixed-point score in `0x44cd30` and the demand arithmetic inside `0x44d130`
  (helpers `0x402fa0`, `0x403000`, `0x4047e0`, `0x406d80` are 8.8 wrappers, not expanded).
- `0x45b1e0` threat trigger: only the phase writes were read (condition
  `x * 4 < DAT_004cc3e8` and distance below 5 not interpreted).
- `0x461180` (science for undeveloped towns), `0x45b940` / `0x45b8b0` scoring details,
  `0x45bf10` placement, who creates the side plan stored at side `+0x694`.
- The build-tree wait in `0x459bb0` (`value < 0x5a` test before payment) is not decoded.
- Town `+0x174` (`0x45d160`) is an income estimate from buildings with a producer field;
  its formula (`0x45c450`, `0x410580`) was not read.
