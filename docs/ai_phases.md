# Strategist phase handlers (table 0x4ae9e8)

Scope: the 24 town strategist phases, the town virtuals +0x150 / +0x15c, the town field
+0x15c (specialisation), the plan builder distance test, the build filter, `0x461180`, and
how an attack is launched. V = read in decompilation or disassembly; G = interpretation.
Field names are town offsets unless stated.

## 0. Frame (V)

- Strategist `0x45e7c0` (town vtable `0x494648` +0x138 is the town update `0x45e8e0`):
  computer side only; speed +0x158 := 1 if harvest < 2.0 * spend, else 2; then **one**
  handler `table[+0x180]` runs, only if production +0x110 is empty (virtual +4 == 0) and
  the queue +0xf8 is empty (virtual +0x1c == 0); then counter +0x17c++ unless it is -1.
- Table checked from the .data bytes at `0x4ae9e8` (24 dwords, V):
  0 `45dda0`, 1 `45dfa0`, 2 `45e2a0`, 3 `45e090`, 4 `45e280`, 5 `45e0d0`, 6 `45e290`,
  7 `45e230`, 8 `45e290`, 9 `45df50`, 10 `45ddd0`, 11 `45df10`, 12 `45de90`, 13 `45de20`,
  14 `45e340`, 15 `45dff0`, 16 `45e3e0`, 17 `45e750`, 18 `45e5e0`, 19 `45e760`,
  20 `45e4c0`, 21 `45e440`, 22 `45e770`, 23 `45e530`.
- "dev" below is `0x45c0e0` = town +0x178 (development level 0..9, V).
- "count" = alive members (member byte +0x5c != 0) of the town member list +0xc whose
  virtual +0xfc (defence building) is false; "defence count" = same with +0xfc true.

## 1. Per phase (all V unless marked)

| Ph (hex) | Handler | Action | Next |
| --- | --- | --- | --- |
| 0 (0x0) | `0x45dda0` | idle; side plan +0x694 test | side +0x694 == 0 -> 10, else stays 0 |
| 10 (0xa) | `0x45ddd0` | development timer | +0x17c <= (dev*5+5)*0x14 = 100*(dev+1) and recommendation +0x18c == 0x5a (90) -> 13; else -> 3 |
| 3 (0x3) | `0x45e090` | science/building: only if +0x17c > 0, DevelopScience `0x45b5f0` | ok -> 4; fail -> +0x17c := 0, -> 13; counter <= 0 (-1 = building queued, 0 = just finished) -> stays 3 |
| 4 (0x4) | `0x45e280` | none | -> 13 |
| 13 (0xd) | `0x45de20` | garrison test | count >= 3*dev+3 -> 9, else -> 15 |
| 15 (0xf) | `0x45dff0` | find the side intelligence record (list side +0x68c) whose dword 0 is this town; `0x44d130(record, town, mode 0)`; plan stored at +0x188 (no record: assert line 0x990, G then null plan) | plan -> 14, else -> 9 |
| 14 (0xe) | `0x45e340` | economy side request (see 2.3), then `0x44dbc0` = `0x457000(plan, side towns +0x660)` (allocate plan entries to towns); `0x457380` plan done (+0x810 set and tactic +0x80c not busy via virtual +0xb4) | done -> 9, tactic joins the town `0x457410` (tactic virtual +0xf4(town, 0)), plan dropped `0x45dd50` |
| 9 (0x9) | `0x45df50` | attack test | specialisation +0x15c != 0 and target +0x184 != 0 and side of the record's object (record dword 0, virtual +0xa0) != own side -> 1, else -> 11 |
| 1 (0x1) | `0x45dfa0` | offensive plan `0x44d130(+0x184 record, town, mode 1)` -> +0x188 | plan -> 2, else -> 11 |
| 2 (0x2) | `0x45e2a0` | as 14 (economy request, `0x44dbc0`, done test) | done -> launch `0x4573b0`, -> 11, plan dropped `0x45dd50` |
| 11 (0xb) | `0x45df10` | economy gate | byte +0x170 and harvest `0x45c270` < 2 * spend `0x45c300` (unsigned ints) -> 7, else -> 12 |
| 7 (0x7) | `0x45e230` | `0x41e610` (farm/house by race), else `0x41e850`; both null -> clears +0x170 | always -> 8 (it writes 0xc then 8, V; the 0xc is dead) |
| 8, 6 | `0x45e290` | none | -> 11 |
| 12 (0xc) | `0x45de90` | defence test | +0x171 == 0 -> 11; defence count >= dev -> 0; else -> 5 |
| 5 (0x5) | `0x45e0d0` | defence building: candidates = list `0x4cd838` (count `0x4cd6c0`) walked last->first, filter virtual +4(type, 0) true; pick `rand() % n`; queue `0x45cec0`; if `0x45cd50` (G: is building) place `0x45bf10` (index from `0x45cfb0` into +0x168) | no candidate -> 0; queue null or placement fail (cancel `0x45cfd0`, object virtual +0x128) -> 0 and +0x171 := 0; ok -> 6 |
| 16 (0x10) | `0x45e3e0` | threat check `0x44cc00(town pos, radius 8, side)` | object inside -> 21; none -> plan dropped `0x45dd50`, -> 0 |
| 21 (0x15) | `0x45e440` | force test | count >= 7*(dev+1) -> 20, else -> 23 |
| 23 (0x17) | `0x45e530` | own record (as 15) **written into +0x184**, `0x44d130(own record, town, mode 1)` (assert line 0xa67 if none) | plan -> 22, else -> 20 |
| 22 (0x16) | `0x45e770` | `0x44dbc0` + done test (no economy request) | done -> 20, tactic joins town, plan dropped |
| 20 (0x14) | `0x45e4c0` | defence test | +0x171 and defence count < dev -> 18, else -> 16 |
| 18 (0x12) | `0x45e5e0` | identical to phase 5 | ok -> 19; no candidate or fail -> 16 (fail also +0x171 := 0) |
| 19 (0x13) | `0x45e760` | none | -> 16 |
| 17 (0x11) | `0x45e750` | none; G: unreachable (no handler writes 17) | -> 21 |

Request types: phases 15/1/23 make plans (unit build requests, kind 1, and reused units,
kind 0; composition in `ai_scoring_threat.md` part 3); phase 3 queues one non-defence
building (`0x45b5f0` / `0x461180`); 7 queues an economic building; 5/18 a defence building.
No handler tests money; only `0x45b5f0` does (side +0x698 >= price `0x45c5b0`).

## 2. Loops

### 2.1 Normal cycle (V)
`0 -> 10 -> {13 | 3 -> 4 -> 13} -> {9 | 15 -> 14 -> 9} -> {1 -> 2 -> 11 | 11}`
`-> {7 -> 8 -> 11}* -> 12 -> {0 | 5 -> 6 -> 11 -> ... -> 12}`.

- Military branch (phase 10 -> 13) while counter <= 100*(dev+1) ticks and no
  recommendation; science branch (10 -> 3) otherwise. Science queues one building per
  cycle; since a queued non-defence building freezes the counter at -1 and its completion
  resets it to 0 (`0x459e4e`), phase 3 waits for completion and the next cycle restarts
  the 100*(dev+1) window (G: alternating build-up / tech-up).
- Garrison: below 3*dev+3 (3 at dev 0, 30 at dev 9) non-defence members, phase 15 builds a
  plan against the town's own record (mode 0: only reusable units of nearby tactics).
- Attack: only a specialised town (+0x15c != 0) with a foreign target attacks; phases 1/2
  create and fill one plan, launch it, then continue with economy/defence. One wave per
  cycle; G: the next wave needs a full new cycle through 0/10/13.
- Economy loop 11 -> 7 -> 8 -> 11 repeats one economic building per pass while +0x170 and
  harvest < 2*spend; ends when both requests fail (+0x170 := 0) or income catches up.
- Defence loop 12 -> 5 -> 6 -> 11 -> 12 until defence count >= dev, then back to 0.

### 2.2 Threat cycle (V)
Entered from `0x45b1e0` (phase := 0x10 when phase < 0x10, non-computer intruder within r).
`16 -> 21 -> {20 | 23 -> 22 -> 20} -> {18 -> 19 -> 16 | 16}`; leaves to 0 only in 16
when `0x44cc00` finds nothing within 8 tiles. Force target 7*(dev+1) (7..70).

### 2.3 Flags (V)
- +0x170 (economy wanted): 1 in constructor `0x459596`, loaded `0x4598b8`, set 1 when the
  town hall (+0xf0) completes `0x459ca9`; cleared in 7 (`0x45e26d`).
- +0x171 (defence allowed): 1 in constructor `0x45959d`, loaded `0x4598c8`, set 1 when a
  defence building completes (`0x459e45`, production `0x459bb0`); cleared in 5/18.
- Phases 2 and 14 also call `0x41e610` / `0x41e850` under the same economy condition each
  tick, without changing phase.

## 3. Town virtuals (V, vtable `0x494648`)

- +0x150 = `0x459b90(target)`: target +0x184 := arg **only if phase +0x180 < 0x10**. Called by
  target selection `0x44cfe0` every side tick, so the target is frozen during threat
  phases (where phase 23 overwrites +0x184 with the own record).
- +0x15c = `0x4597a0(type)`: recommendation +0x18c := arg. Only caller `0x44d9ab` in
  `0x44d130`, when the remembered wanted type != 0x5a. Other writers of +0x18c:
  constructor 0x5a (`0x4595cc`), loader (`0x4598fe`). V: nothing resets it to 0x5a, so
  after the first recommendation phase 10 always takes the science branch (G effect).
- `0x45b5f0` with +0x15c != 0: if +0x18c != 0x5a, `0x45b940(town, rec)` walks the filter
  prerequisite chain recursively and returns the first buildable leaf (cost = sum of
  depths); if that fails, or +0x18c == 0x5a, it scans list `0x4cdb08` (count `0x4cd6c8`)
  minimising `0x45b940 + (10 - 0x45b8b0)` (`0x45b8b0` = how many filter entries list the
  type as prerequisite, G: prefer types that unlock more), ties 50 % by `rand() & 0x7f < 0x40`.
  Then speed 1, money test, queue, place.

## 4. Town field +0x15c (specialisation) (V)

- Values: `0x45c4f0(object type)`: 0x0b/0x26/0x40 -> 1; 0x13/0x2f/0x47 -> 2; 0x18/0x4c -> 3;
  other -> -1. 0 = plain hall (undeveloped).
- Writers: constructor 0 (`0x459584`); loader (`0x45987f`); production completion
  `0x459cbb`: when the finished object is the town's main building +0xf0, +0x15c :=
  `0x45c4f0(it)`, +0x170 := 1 and the build filter gets tree `0x4af130 + (spec + race*4) *
  0x462` (virtual +0xc, then +0x2c); `0x45dc50(value)`: +0x15c := value, creates the hall
  type `0x4bc390[value*3 + race]` (fallback `0x4bc394[value*3]`), same tree switch.
  `0x45dc50` callers: scenario load `0x43082c` (key "Type", via `0x45dc30` which first sets
  race +0x164), and with 0 at `0x45a8d0` and `0x45ed59` when the object removed is +0xf0
  (G: hall lost/captured -> back to undeveloped).
- Hall table `0x4bc390` [spec][race]: 0: 0x0a 0x25 0x3f; 1: 0x0b 0x26 0x40; 2: 0x13 0x2f
  0x47; 3: 0x18 -1 0x4c (race 1 has no spec 3; fallback slot is also -1, G: invalid).
- The 8 tile radius in `0x45b1e0` and the attack permission in phase 9 both test this
  field != 0; price/time factors in `0x45c5b0` / `0x45c690` too.

## 5. `0x461180` specialisation choice (V from decompilation; race variable G = +0x164)

- Abort if another undeveloped town has a larger +0x190 + +0x194 or is in phase 4.
- Counts specialised towns per race and value (3x3); walks preference list +0x19c (-1
  end) and takes the first value with count 0. **Correction**: if no value is free the
  function returns false (no default 2).
- Value 1: build 0x0b / 0x26 / 0x40 (race 0/1/2) if the filter allows; on filter, queue or
  placement failure it falls through to value 2.
- Value 2: 0x13 / 0x2f / 0x47; on failure falls through to value 3 (race 2 failure calls
  `0x461920` first).
- Value 3: race 0 0x18 (queue+place inline, failure `0x461920`); race 1 0x2f if buildable
  else 0x26 (no spec-3 hall for race 1); race 2 0x4c; races 1/2 via `0x461950` (queue +
  place helper). Returns true when a building was queued.
- The queued object is the hall upgrade; its completion sets +0x15c (section 4, G link).

## 6. Plan builder details (V)

- `0x44db80(ax, ay, bx, by)`: true (reject) when `2 * len(a - b) > 0x32`, i.e. distance
  > 25 (0x19); `0x461fe0` length. In `0x44d130` (`0x44d2dc`, `0x44d415`) a = candidate
  tactic position (virtual +0x94), b = the **town** argument's position ([esp+0x30ec]).
  First loop: side tactics +0x654; second (mode != 0): side towns +0x660.
- Build filter at town +0xf4 (class vtable `0x48ffb4`, ctor `0x411890`), virtual +4 =
  `0x4119b0(type, out)`: entries of 0x88 bytes (34 dwords) at filter +0x2e8, -1 ended,
  entry = {type, prerequisite list...}. Returns true when entry[1] == -1 (nothing
  missing). With `out` it copies the entry; type absent -> out = {type, 0x5a, -1}, false.
  So the "out index" (`[esp+0x354]` in `0x44d130`, test `0x44d6dd`) is the first missing
  prerequisite; < 0x5a means "in this town's tree but blocked", and then the candidate type
  itself (esi, not the prerequisite) is remembered as wanted. In `0x45b940`, -2 and -3
  in the list are also stop codes (meaning unknown).

## 7. Attack wave (V unless marked)

1. Target pick: `0x44cfe0` per town minimum `0x44cd30` score over non-allied records,
   stored via +0x150 into +0x184 (only while phase < 0x10).
2. Phase 9 checks +0x15c != 0 and that the record's object (dword 0, G: enemy town) is of
   another side; phase 1 builds plan `0x44d130(record, town, 1)`: plan +0x808 = record
   object (refcount +0xac++), +0x80c = new tactic (`0x4474d0`, refcount +0xac), mode 1 also
   reuses units of the side's towns within 25 tiles.
3. Phase 2 each tick: `0x457000` hands requests to idle towns (phase 0/2/0xe/0x16) until
   `0x457380` reports done.
4. Launch `0x4573b0`: if tactic and target exist and the plan is done, tactic virtual +0xd8
   (base tactic vtable `0x4944f0` -> `0x453360`) with the target object's position (virtual
   +0x94). `0x453360` clears the tactic's waypoint list, adds that one waypoint, detaches
   from a holder (+0x94/+0x98), calls virtual +0x5c and +0x30(pos), plays a voice line
   (G: "move team to the enemy town"; combat comes from the tactic logic on arrival).
5. Then `0x45dd50` releases the plan (`0x447200`, `0x456e90`, pool `0x4d9b88`); the tactic
   keeps the order.

## 8. Unknowns

- Town fields +0x190 / +0x194 and the race argument in `0x461180` (decompiler lost it).
- `0x461920`, `0x45cd50`, `0x45bf10` placement internals; `0x44cc00` exact argument order.
- Whether phase 17 is reachable through any computed write (no direct writer seen).
- What +0x80c tactic +0xd8 does for derived tactic classes (only the base entry read).
- Meaning of filter list codes -2 / -3; how the filter list is refreshed (+0x10, +0x28).
- Effect of the never-reset recommendation +0x18c in practice (once built, `0x45b940`
  presumably fails and the general scan is used; not verified).
