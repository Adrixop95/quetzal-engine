# AI combat assignment functions (structure survey, not a full formula read)

Evidence date: 2026-11-14. **V** = read from the Ghidra decompilation (function addresses
only), **G** = guess. This is the result of a first pass over `0x451c30`, `0x452c70` and
`0x454820`. These are not the group reach rule (that one is the table at `0x4516b0`, see
`docs/ownership_mechanism.md`); they are the combat assignment code of the tactic object.

## Global assignment (`0x451c30`, profiler label "Global_assign")

- V: refuses to run when `0x42e8c0(0x32)` returns true (a global gate; meaning not read).
- V: phase `collect_team`: walks the tactic's member list (tactic field `+0x0c`) and copies
  up to 100 members into a work array (work block `+0xa0f0`); only members with the active
  byte `+0x5c` set, and for member kind 1 only those whose virtual `+0xfc` is true. Each
  copied member gets its reference counter (`+0x34`) incremented.
- V: phase `collect_enemies`: walks the enemy candidate list (tactic field `+0x5c`, list
  object `+0x17` words in) and keeps targets that are active, hostile (`0x4472e0` is false
  for their side) and visible; enemies are stored in records of 0x19c bytes (work block
  `+0x190`). A target is dropped if it is a kind 1 object that already has a living owner
  tactic pointing back at it. Enemies are only accepted while the distance (`0x461fe0` of
  the offset from tactic position `+0x10`, `+0x14`) is below 0x19.
- V: if no team member or no enemy was collected, the members are released through their
  virtual `+0xd0` and the function returns false (so the tactic falls back to its default).
- V: phase `enemy_danger`: for every enemy, sums over all team members the kill time
  function `0x46fac0(enemy, member)` (positive values only), scaled by 0x400, into a danger
  value (record `+0x194`), then normalises by the number of members (`<< 8`).
- V: phase `team_efficiency`: for every pair (enemy, member) takes `0x46fac0(member, enemy)`,
  scales by `0xf` and `0x23` (15 and 35, with a comparison against zero choosing between two
  forms), and combines it with the enemy danger into a per pair value stored in the
  member's matrix row. G: the result is a priority or desirability score of "this member
  attacks that enemy".
- V: when the tactic's urgent flag (`+0x60`) is set, the first enemy becomes the target
  (`+0x80`) and each member is either left unassigned or released depending on whether it
  can attack (virtual `+0xa8`) and whether its airborne check (virtual `+0xa4`) and the
  reach of virtual `+0x0c` allow it.
- V: otherwise the function runs the `enemy_loop` and `assignment_loop` phases (not read in
  this pass): that is where members are bound to enemies.

## Local assignment (`0x452c70`, "Local_assign")

- V: loops over enemies and, per enemy, over members, with profiler labels `CanAttack`,
  `CalcKillTimes`, `hit_loop_body`, `calculations` and `attack`. G: it is the fine grained
  pass that decides who attacks whom this tick using the same kill time function.

## Tactic think (`0x454820`, "tTactic::Think")

- V: top level tick for a tactic; its body was not read in this pass.

## What is still open

- The numeric rules behind `enemy_loop` and `assignment_loop` (how scores turn into
  assignments, ties, re-planning interval).
- The gate `0x42e8c0(0x32)`.
- How these functions relate to the AI purchase rules already covered by task 108.
