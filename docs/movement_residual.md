# Movement and unit behaviour: residual unknowns

Static analysis only (corpus, objdump of Aztec.exe, dec2). V = read in code. G = inference.
Numbers are given as hex (decimal).

## 1. Member step 0x455b10: lost call arguments

The vtable 0x48fbd4 lists 0x453ac0 (+0x20), 0x453b60 (+0x24), 0x453be0 (+0x28),
0x453e20 (+0x2c) and 0x455b10 (+0x44). 0x4516b0 is at +0xc. The town vtable 0x494648 has
the same entries, except that +0xc is 0x459790.

- V reach / search radius (slot +0xc): 0x4516b0 returns {4, 8, 8, 6}[tactic +0x24 mode].
  The town override 0x459790 returns 0xc (12). The same value is the leash limit and the
  radius argument of the local path.
- V local path call (slot +0x28 = 0x453be0). It has 5 stack arguments, in the order
  (record, goalX, goalY, goalHolder, radius):
  - Unconnected member (+0x5f == 0) without target, 0x4569f8..0x456a33: record +0x14 is set
    to 0. The goal is the tactic anchor tile (+0x10, +0x14). goalHolder = &tactic+0xc8 when
    0x456c60(tactic+0xc8) resolves a list, else NULL. The radius comes from slot +0xc. If the
    result is 0, record +0x15 is set to 1 (own path flag).
  - Target branch, 0x456795..0x456804: the tile list built by the scan of tiles around the
    target is passed through 0x456b90 / 0x416b70. The goal is the point that 0x416b70
    returns, and the radius again comes from slot +0xc. The base 0x456a90 is called with
    (0x30, 0x30) = 48 px.
  - 0x453be0 passes the goal, goalHolder and radius to the wave 0x46c470 as its last
    arguments (flag 0, which means that occupancy is respected).
  - So the "radius 6" in the logs is the reach of mode 3. Mode 0 searches 4, modes 1/2
    search 8 and towns search 12.
- V connected branch look-ahead, 0x456868..0x4569f0:
  1. pos = the member tile (unit slot +0x88).
  2. d1 = 0x461c20(anchor - pos).
  3. r = sidestep(member, &pos, &d1, &anchor, flag 0). The anchor tile is passed as the
     excluded tile (see section 2).
  4. If r == 2: stop, no step.
  5. Otherwise (ground units only, unit slot +0xa4 == 0) the member's occupancy entry is
     cleared. Then next = pos + d1 and d2 = 0x461c20(anchor - next), and
     sidestep(member, &next, &d2, &anchor, 0) runs (its result is ignored). The entry is
     then restored.
  6. The step is ordered only when d1 + d2 != (0, 0) (0x456a60 against a zero vector).
     The order is unit slot +0xcc(pos + d1).
  - Effect: d2 never changes the step. It only vetoes it when the look-ahead direction is
    the exact reverse of d1 (anti-oscillation guard).
  - Result 1 (rotated d1) and result 3 (connected occupant) both proceed. With result 3,
    d1 is not changed (G: the step is ordered into the occupied tile, and the unit-side
    CanEnter decides what happens next).

## 2. Side step 0x453e20: the "garbled" branch

- V arguments (ret 0x14 = 5 dwords): member, tile*, delta* (in/out), excludeTile* (may be
  NULL), flag byte.
- V the blocked-occupant test (flag != 0) applies only when the grid entry has bits in
  0xffff0000, i.e. it is a pointer. It also requires occupant +0x30 == this tactic. The
  formation scan is slot +0x60 with argument 0.
- V the garbled branch, 0x453ee4..0x453f04: when excludeTile != NULL and tile + delta ==
  *excludeTile (blocked on the first test, not a connected occupant), the code jumps to
  the shared exit at 0x453f96. That exit applies 0x462310 (45 degrees clockwise) to the
  current delta and returns 2.
  - On the normal path the current delta at that point is ccw(d), so the exit restores d.
  - On this early path the delta is still d, so the function returns 2 with
    delta = cw(d). This is a quirk; callers stop on 2 (0x456 branch above). G: harmless.
- V return codes: 0 = original delta passes (0x453fbd); 1 = rotated passes; 2 = blocked;
  3 = connected friendly occupant.

## 3. Occupancy grid and building footprint

- V grid = DAT_004dd2f8 + 0x10 (row pointers indexed by y & 0xff, dword per x). Every
  writer found stores an object pointer or 0. The store scan covered every store reached
  from a ds:0x4dd2f8 load: Marine 0x433xxx, Spider 0x44axxx/0x44bxxx, Truck 0x460xxx,
  0x40f5c3, 0x41c983, 0x42ba53, 0x449efe, 0x45fc93, and the member search 0x453be0 /
  0x455b10. No store of the constants 1 or 2 into the grid exists.
- V the values 1 and 2 are building descriptor mask values, not grid values. The footprint
  writer is 0x411230:
  - It covers a 7 x 7 window centred on the building tile (+0x44, +0x48). The mask is
    dword[49] at descriptor (+0x64) + 0x18, row-major, centre at +0x78.
  - Cells == 2 receive the building pointer, and the centre tile always does.
  - Cells == 1 (clearance) are not written to the occupancy grid.
  - Map objects with one tile (constructor 0x40fb70, descriptor table 0x49b538 stride 0x508)
    write their pointer on their own tile.
- V unit-vs-building and unit-vs-unit use the same test: CanEnter (Marine 0x4347f0,
  Spider/Truck 0x44bc80) needs entry == 0 and returns -1 otherwise. A body cell of a
  building therefore blocks exactly like a unit. Clearance cells do not block movement.
- V the pointer test (entry & 0xffff0000) is also used by placement 0x409xxx. That code
  rejects neighbours whose object slot +8 == 1, which is the object kind, not a grid
  sentinel.
- G the guard against small non-pointer values is defensive. No writer of such values was
  found.

## 4. Spider arrival and Bird turning

- V Spider 0x44b360, state 2, 0x44b725..0x44b7f9:
  - Heading byte +0x91 is quantised to 0x20 steps. Position +0x78/+0x7c and target
    +0x80/+0x84 are in 24.8.
  - Arrival depends on the heading:
    - 1..0x7f: posY <= tgtY.
    - > 0x80: tgtY <= posY.
    - 0: tgtX <= posX.
    - 0x80: posX <= tgtX.
  - Helpers: 0x44bcf0(a, b) = b <= a; 0x406da0(a, b) = a <= b; the arguments are
    (pos, target).
  - Diagonals test only the y axis. Heading 0 = +x and 0x40 = -y (screen up).
  - On arrival, 0x44b7ff: pos = target exactly, the group is asked (+0x11c) and the state
    becomes 1.
  - Then, if the stored-step flag +0x90 is set and CanEnter(+0x94) != -1, the stored step
    is ordered (+0xcc). Otherwise +0x90 = 0 and the animation is stopped (0x4034c0(.,0,1)).
- V Bird 0x407b10, state 2. Heading is +0x70. The turn rate r = descriptor +0x18.
  diff = (signed char)(atan(target - pos) - heading).
  - If |diff| > 0x38 (56) - r, or the bird has a target (+0x94):
    - The rate is cut to r/3 when there is a target object (slot +0xa8), |diff| > 0x40 (64)
      and n(dist) < target slot +0x74 * 0x200 (512).
    - If |diff| < rate: heading = want. With a target and the cooldown +0x84 == 0, this is
      also where the attack is released.
    - Otherwise heading += sign(diff) * rate (mod 256).
    - The hold timer +0x8c is set to 0x12 (18).
  - Otherwise (small deviation, no target):
    - If the hold timer is 0, the heading drifts 1 per tick toward the nearest multiple of
      0x20 ((h + 0x10) & 0xfe0).
    - Else the timer counts down.
  - Velocity = unit(heading, 0x461f70) scaled to speed +0x80 << 8. The speed is updated in
    0x407a60, which was not re-read here.

## 5. Thinks per logic tick, tick length

- V one logic tick = one call of 0x4577b0. The frame loop has call sites 0x458a55,
  0x458a85, 0x458ab4 and 0x458ae1.
  - Pacing: 0x4587e5 reads a time import ([0x48f27c]). The code then computes
    delay = [0x5154dc] - elapsed + carry and sleeps through [0x48f104], with a minimum of
    1 ms when the result is negative or the flag 0x515424 is set.
  - The delay default is 83 ms (options doc, handler 0x41a9e0, range 60..200), i.e.
    about 12 Hz. So the logic tick IS the 12 Hz simulation step, not a multiple of it.
  - G: the four call sites are per-mode variants with one tick per loop pass.
- V tactic think 0x454820 has 3 callers:
  - 0x45e8e9: town update, slot +0x138.
  - 0x42e142: 0x42e0a0, slot +0x138 of vtable 0x492e90 (runs only while byte +0xf0 is set).
  - 0x4306c1: inside 0x430690.
  Each runs once per pool pass, and the pool 0x4d9b50 is updated once per tick in 0x4577b0
  (ai_scoring_threat.md 6). So there is one think per tactic per tick, with no divider. All
  "thinks" counters (+0x64 15..22, +0x88 37, 20 per node) are 12 Hz ticks. Member steps
  0x455b10 are driven from that think.
- V units: object pool 0x5156f8 +0x138 once per tick, so each unit think runs once per
  tick as well.
- G the generic vtable 0x48fbd4 has 0x408eb0 at +0x138 (via 0x48fcd4 + 0x38), which does
  not call 0x454820. Which tactic classes use it was not traced.

## 6. auto_engage_tiles.csv rows above 7 tiles

- V slot +0x128 (0x455570, town wrapper 0x45ece0) is only the sighting report. It has no
  distance argument and no per-type field.
- V weapon ranges do not explain the rows. Weapon record +0x10 (d4) is 0xa (10) px for
  both the footman (weapon 4, class 2 param 0) and the bear (weapon 10, class 2 param 2).
  Yet the footman reaches 3 tiles and the bear 14.
- V sight mask ids (d6) are 64..68 for all of these marines and do not order the results
  either: bear 65, mage 67, footman 64, rifleman 68.
- G the only per-tactic distance equal to 12 is the town reach 0x459790 = 0xc (12). It
  matches the rows mage, tuzem, ninja and spider_axethrower = 12.
  - Hypothesis: in those tests the defenders belonged to a town tactic. The leash and
    local-path radius was then 12, so a scan-detected enemy (box, section idle doc) could
    be chased up to 12 tiles.
  - Bear 14 remains unexplained. One option is the forced retaliation path (damage
    0x43ea30, 25-tile collection), but the bear is melee. Another is the "?" at 12 being a
    missing sample.

## Still unknown

- Unit-side behaviour after result 3 of the side step: the step order into an occupied
  tile (unit slot +0xcc and CanEnter at step start).
- What 0x416b70 returns in the target branch (assumed target tile). The exact tile scan
  around the target (0x4562b6..0x456770) was not re-read.
- Which tactic classes use vtable 0x48fbd4 +0x138 (0x408eb0), and whether any tactic thinks
  less than once per tick.
- The import names at [0x48f27c] and [0x48f104] (assumed timeGetTime / Sleep) and the
  exact carry term [esp+0x24] in the pacing.
- Bird speed update 0x407a60 and the distance units. The Boat/Johnka collision path.
- The engage rows bear 14 and demolisher/cannon 10. They need a live control with the
  tactic type and mode of the defender recorded.
