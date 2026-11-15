# Group movement, formation and leash (base tactic, vtable 0x4944f0)

Decompilation read only, nothing was run. V = read in the Ghidra output, G = guess or
interpretation. Sources: `work/decomp/all_full.c`, `work/gaps/dec_004516b0.c`,
`dec_00453570.c`, `dec_00453920.c`, `dec_451c30.c`, `dec_00455b10.c`, `dec_00453e20.c`.
Ghidra lost several call arguments (thiscall/stack), these places are marked G.

## Summary

There are no formation offsets and there is no speed matching. A group is a tile anchor that
walks along a group path one path node at a time. Each member, whenever it is idle, asks the
group for one single tile step towards the anchor. The group waits before advancing the anchor
(up to 20 thinks per node), which is the only thing that keeps fast and slow members together.
The reach (4/8/8/6 tiles by mode) is used as (a) combat leash around the anchor, (b) radius
outside which a disconnected member is split off into a "lost team" that follows and merges
back, (c) merge/boarding distance of the follow order.

## Fields (tactic)

| Off | Meaning | |
| --- | --- | --- |
| +0x0c | member list, record 0x28 (40) bytes | V 0x454090 |
| +0x10, +0x14 | anchor tile x, y; +0x18 byte "anchor valid" | V 0x454220 |
| +0x20 | state: 1 stopped, 2 waiting for members, 3 moving | V 0x454820, 0x4545d0, 0x453920 |
| +0x24 | mode 0..3 (reach index) | V 0x4516b0 |
| +0x2c, +0x30 | dx, dy of the current leg | V 0x454820 |
| +0x38, +0x3c, +0x40 | group path (12 byte nodes x, y, kind), node count, node index | V 0x453920, 0x454820 |
| +0x50 | flags: bit 0 all members "large" (+0xa0), bit 2 all members airborne (+0xa4) | V 0x454090, 0x4559e0 |
| +0x54 | wait counter per node (thresholds 0x13 = 19, 0x14 = 20) | V 0x454820 |
| +0x58 | leader object | V 0x454240 |
| +0x5c list, +0x60, +0x61 | enemy list, urgent flag, "engaged" flag | V 0x454820, 0x450710 |
| +0x64 | re-plan countdown, reseeded to 15..22 (0xf..0x16) thinks | V 0x454820 |
| +0x88 | thinks since last path, threshold 0x24 (36) | V 0x454820, 0x453920 |
| +0x90 | voice class 1/2/3 from leader type (not a speed) | V 0x454240, 0x451440 |
| +0x94, +0x98 | following flag, followed tactic | V 0x451440 |
| +0x9c, +0x9d | merge-on-arrival flag, hidden flag | V 0x452990 |
| +0xa0 list, +0xac | tactics following me, count of references | V 0x453fd0, 0x451440 |
| +0xb8, +0xbc/+0xc0, +0xc4, +0xc6 | pending path flag, pending destination, wait counters | V 0x453920 |
| +0xc8 list | tiles of the connected cluster (built by formation scan) | V 0x450910 |
| +0xd8, +0xd9 | formation scan done this think; "somebody stands at the anchor" | V 0x450910 |
| +0xdc list, +0xe4 | queued waypoints, count | V 0x453360 |
| +0xea | "lost team" flag (setter 0x408af0, slot +0x64) | V |

Member record: +0 object, +4/+8 tile (updated by the unit through slot +0x124 = 0x455a50 at
every step start, V 0x433820), +0x14 byte "in place", +0x15 byte "has own path", +0x16 packed
own path (4 bits per step, at most 8 steps from 9 nodes, V 0x44ff60, 0x4518c0), +0x1c path
length, +0x20 path index, +0x24 path retry counter. Object: +0x30 tactic, +0x5f byte
"connected to the cluster" mark.

## Reach (0x4516b0, slot +0xc)

- V: table {4, 8, 8, 6} indexed by mode +0x24 (modes 0, 1, 2, 3). Unit: tiles.
- V: distances use the approximate norm 0x461fe0 on tile deltas: with a = max(|dx|,|dy|),
  b = 1.5 * min, result = a + (b>>2) + (b>>6) - (a>>7) - (a>>5). Examples: (4,0) = 4,
  (3,3) = 4, (6,0) = 6, (4,4) = 5, (8,0) = 8, (6,6) = 8.
- V: comparisons are `reach < norm` (outside) in 0x454820 and 0x455b10, and `norm < reach`
  (strictly inside) in 0x453570 and 0x452990.
- V: mode semantics from the code: mode 0 never lets members act on targets (0x455b10 takes
  the no-target branch); mode 1 the same while the group moves (state 3); the engaged flag
  +0x61 is set in the think tail when the enemy list is non-empty and (state == 1 or mode == 2)
  and mode is not 0 or 1, so mode 2 engages also while moving, mode 3 only when stopped.
- G: 0 = hold/direct (4), 1 = defensive (8), 2 = aggressive (8), 3 = balanced (6).

## Move order (0x453360, slot +0xd8)

- V: empties the waypoint queue +0xdc, pushes the destination tile (count +0xe4 = 1), clears
  +0x61, drops a follow (target slot +0x70, target +0xac minus 1, +0x94 = 0), calls slot +0x5c
  (0x450710: releases the enemy list, every live member gets +0xd0(0) and +0xd4(0), i.e. target
  dropped, +0x61 = 0), then path-to (slot +0x30) and plays voice set 0x64 (100) or by class
  400/300 (0x190/0x12c).
- V path-to 0x453920: stores destination in +0xbc/+0xc0 with +0xb8 = 1 and returns if the path
  budget 0x46c410 refuses (two path slots, 0x515a68..0x515a6c, priority by wait counter); the
  think retries (common end of 0x454820). Then 0x46c470 searches ONE path for the whole group
  from the anchor to the destination with group flags +0x50 into +0x38. Fewer than 2 nodes:
  returns 0 (and calls stop +0x34 when already moving). Else state = 3, index = 0, +0x88 = 0.
- V: members get no destination from the order. No per-member offset table exists in any of
  the functions read.

## Follow order (0x451440, slot +0xf0) and step (0x453570, slot +0x58)

- V 0x451440: clears waypoint queue, slot +0x5c, releases an old follow, sets +0x94 = 1,
  +0x9c = 0, +0x98 = target, target +0xac plus 1, target slot +0x6c(this), then the step.
  Voice set 0x68 (104) or 404/304.
- V step 0x453570, in order: (1) enemy list non-empty and +0x61: path to the urgent enemy, or
  to the focus object +0x80, or to the first enemy's anchor; (2) not following: drop queued
  waypoints equal to the anchor, return 1 if none left, else path to the next; (3) following:
  if norm(target anchor - own anchor) < reach then with +0x9c the target absorbs all members
  (slot +0xb8 = 0x44faf0, AddMember each), selection is moved (0x4498a0), followers of this
  group are redirected to the target, voice 0x6a, follow ends; without +0x9c the ferry request
  for kind 2 (see ferry_eligibility.md). Otherwise or afterwards path to the target anchor.
- V callers in the think 0x454820: every think while state == 1; and when +0x88 > 0x24 (36)
  and (engaged or following), so a moving follower re-paths every 37th think.

## Anchor (+0x10, +0x14)

- V: only writer is 0x454220 (slot +0x90). Callers read: AddMember 0x454090 (first member and
  +0x18 == 0: anchor = that member's tile), box selection 0x448b40 (first member's tile), the
  think's node advance, and the empty-group path of the think.
- V node advance (0x454820, state 3), evaluated only when +0x54 > 0:
  1. if any member record has +0x14 == 0 or its object is not alive (+0x5c == 0): wait while
     +0x54 < 0x14 (20);
  2. unless +0xea, wait (same limit) while a hidden merging sub-team (+0x94, +0x9c, +0x9d all
     set) is in list +0xa0;
  3. run the formation scan if not done;
  4. advance only if +0xd9 is set or +0x54 > 0x13 (19).
  Advance: +0x54 = 0; next node must pass 0x46c330 and have map byte > 7, else re-path via the
  step; anchor = node, +0x2c/+0x30 = leg delta, index +1. Path exhausted: next queued waypoint
  via the step, or stop (slot +0x34 = 0x4545d0: state = 1).
- V: so the anchor moves one path node per at most 20 thinks, and immediately (next think with
  +0x54 > 0) when all members are in place and one of them stands on or near the anchor.
- V: the anchor is not recomputed when a member is removed.
- V 0x4545d0: on stop, if side test 0x43d2c0 passes and the anchor tile holds a type 1 object
  of the same side whose tactic (not kind 4) has it as hall (+0xf0), that tactic absorbs the
  group (slot +0xb8). G: units walked onto their own town hall join the town.

## Formation scan (0x450910, slot +0x60) - connectivity, not slots

- V: clears +0x5f on all members, +0xd9 = 0, then marks +0x5f = 1:
  1. airborne members (+0xa4) inside the pixel box anchor*48 - 0xa8 (168) .. + 0xd8 (216),
     i.e. tile centre +/- 192 px = 4 tiles; +0xd9 = 1 if norm(pixel pos - anchor centre) <
     0x90 (144 px = 3 tiles);
  2. members inside anchor*48 - 0x60 (96) .. + 0x90 (144), i.e. centre +/- 120 px, the 5 by 5
     tile block around the anchor; +0xd9 = 1 if one stands on the anchor tile;
  3. flood fill: for every newly marked member, unmarked members of the same tactic within
     +/- 0x78 (120 px, 2.5 tiles) of it are marked too;
  4. +0xd9 = 1 also when the anchor tile is occupied by any object.
- V: sets +0xd8 = 1; +0xd8 is cleared at the start of each think.
- So "formation" = a blob chained by gaps of at most 2 tiles from the 5 by 5 block at the anchor.

## Member movement (0x455b10, slot +0x44)

- V: called by the unit through slot +0x11c (0x455a90, only when group state != 0) from its
  standing state (unit state 1, corpus line 2943) and after a step ends (G, line 3093).
- V: sets record +0x14 = 1. Branches:
  - Own path active (+0x15): +0x14 = 0, follow the packed path one step (0x453b60); stop
    using it when finished or blocked. A connected member without a target drops its own path.
  - No target, connected (+0x5f): direction = 8-way quantised sign of (anchor - member tile)
    (0x461c20: axis step when major >= 2.5 * minor, else diagonal); 0x453e20 tests the tile
    (member slot +0x94 == -1 means blocked), tries the direction rotated by 45 degrees
    (0x462310) then the other side (0x4622e0 twice); result 3 = blocked by a connected fellow
    member. If a step results, member slot +0xcc(tile + step) orders ONE tile step. A member on
    the anchor tile gets a zero vector and stays.
  - No target, not connected: +0x14 = 0; individual path search 0x453be0 (slot +0x28, budget
    0x46c410, own passability flags) and +0x15 = 1 on success. G: goal is the first tile in the
    cluster list +0xc8 (or the anchor) with the reach as tolerance; arguments were lost.
  - Has target (modes 2, 3, and mode 1 when not moving): if reach < norm(member - anchor) it
    steps towards the anchor instead of the target (this is the combat leash). Otherwise it
    takes the direction to the target (member slot +0xb4) and tries up to 8 alternately rotated
    variants, accepting the first whose next tile is within reach of the anchor. If blocked
    (result 2) it scans tiles around the target within weapon range (weapon slot +0x74, pixel
    norm) that are also within reach of the anchor and paths to them (+0x15 = 1).
- V: think re-plan, airborne member with a target farther than reach from the anchor: target
  dropped (member slot +0xd0).

## Speed

- V: the unit step setup 0x433820 scales the step by its own descriptor speed
  (`[unit+0x68]+0x14 << 8`, 0x461d50) and reads nothing from the tactic; it only reports the
  new tile (slot +0x124).
- V: tactic +0x90 (1/2/3 from the leader's type id in 0x454240: up to 0x1a, 0x52, 0x53, 0x55
  -> 1; 0x1b..0x34 and 0x57 -> 2; rest -> 3) only selects the voice set (0x451440, 0x453360).
- Therefore no speed matching (V for the functions read). Fast members reach the anchor and
  wait; the anchor waits for slow ones up to 20 thinks per node, then leaves them.

## Falling behind: lost team (0x454820 re-plan block, every 15..22 thinks)

- V: a fresh tactic is created (0x4474d0). Every member that is not marked connected (+0x5f),
  not airborne (+0xa4), not +0xec, and has reach < norm(member tile - anchor) is moved into it
  (AddMember, slot +0xd4). If none qualified the tactic is freed (0x447d70).
- V: otherwise the new tactic gets slot +0x64 (0x408af0, sets +0xea) and slot +0xf4
  (0x452990, follow with merge: +0x94 = 1, +0x9c = 1, +0x9d = hidden argument). G: arguments
  are (1) and (original group, hidden = 1); debug strings "Lost team ..." / "Hidden team ...".
- V 0x452990: same side only; merges at once if already inside reach, else follows; the merge
  happens in 0x453570 when norm < reach. The lost team's anchor is its first member's tile.
- V: a hidden merging sub-team's leader still counts in the leader election (0x453fd0).
- So stragglers are not teleported or sped up: they are detached, walk to the main group as
  their own group and are re-absorbed inside the reach.

## Member dies or is taken

- V: AddMember of the new tactic (0x454090) first calls the old tactic's slot +0x18
  (0x4554f0): finds the record (0x451410), removes it, recomputes flags +0x50 (start 5, clear
  bit 0 / bit 2 per remaining member), then slot +0x1c (0x4559e0), which re-elects the leader
  (slot +0x74 = 0x453fd0) if the removed object was the leader.
- V: slot +0x10 (0x4506d0) is the record-based removal: object refcount +0x34 minus 1, tactic
  pointer cleared (0x43ecf0), record removed, slot +0x1c.
- V leader order 0x44fbd0: higher value in rank table 0x4bc3e8 by type id, ties by higher id.
- V: no anchor, path or state change on removal. A dead (+0x5c == 0) member still listed
  blocks the node advance only until the 20 think timeout.
- V empty group (0x454820): absorbs hidden merging sub-teams (slot +0xb8, anchor from them),
  clears enemies, other followers get cancel (+0xdc) or are redirected to this group's follow
  target (+0xf0), selection is cleared, the tactic is freed when +0xac < 1.

## Unknowns

- How often 0x454820 runs per logic tick; all "thinks" above are in that unit.
- Lost arguments: goal and tolerance of the individual path in 0x455b10; arguments of +0x64
  and +0xf4 for the lost team (hidden or visible).
- Where state 2 is entered and where record +0x14 is cleared besides 0x455b10 (0x453ac0 and
  the unit side were not traced); exact meaning of record +0x14 between calls.
- Meaning of member slot +0xec (excluded from the straggler test) and of side test 0x43d2c0.
- +0x48 (node index / member count bookkeeping in 0x453920 and the advance) not understood.
- 0x451c30 and 0x452c70 (combat assignment) not re-read here; see ai_assign_structure.md.
- The names of modes 0..3 (G above) need one live read of +0x24 per UI setting.
