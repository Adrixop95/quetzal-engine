# Per-class unit think state machines

Read-only static analysis. V = read in code (address cited), G = guess. Offsets are byte
offsets into the object; "tick" = one think call. Extracted bodies: `work/gaps/k2/`.

## 0. Shared machinery

Vtable slots (V, read from the vtables in Aztec.exe): `+0x5c` (slot 23) death handler,
`+0x60` (slot 24) think, `+0xcc` (51) step to tile, `+0xd0` (52) set chase target,
`+0xd4` (53) set fire target, `+0x94` (37) tile free test, `+0xa8` weapon prototype for a
target, `+0xc0` can-fire-now, `+0x14` position, `+0x90` aim point of a target.

| class | vtable | think | death | step | chase setter | fire setter |
|---|---|---|---|---|---|---|
| Marine | 0x4932cc | 0x433db0 | 0x433960 | 0x433820 | 0x433c50 | 0x433650 |
| Spider | 0x4940ec | 0x44b360 | 0x44ae70 | 0x44ab90 | 0x44b0e0 | 0x44aa80 |
| Cannon | 0x490380 | 0x417090 | 0x416b90 | 0x416710 | 0x4608c0 | 0x433c50 |
| Truck | 0x4948e8 | 0x460a10 | 0x460550 | 0x4604a0 | 0x4608c0 | 0x460420 |
| Headtruck | 0x492c38 | 0x42c880 | 0x42c4d0 | 0x42c430 | 0x433650 | 0x42c370 |
| Bird | 0x48f97c | 0x407b10 | 0x407870 | 0x407820 | 0x471950 | 0x407670 |
| Zeppelin | 0x4951b8 | 0x4719b0 | 0x471740 | 0x4716f0 | 0x471950 | 0x433650 |

(Setters shared across classes write the same byte offset, so they also fix the field
layout of the borrowing class: G for the borrowers, V for Marine `0x433c50`/`0x433650`.)

Animation player (V): `0x4034c0(anim, seq, loop)` starts a sequence (resets tick and sets
the changed flag `+0x18`); `0x403510(anim, seq)` switches sequence keeping the frame index;
`0x4033c0(anim, n)` seeks the n-th frame slot of the current sequence (a frame with
duration 0 counts as one slot) and is how a static facing pose is chosen;
`0x403460(anim)` advances one tick: frame tick `+0x14` +1, when >= frame duration go to the
next 32-byte frame record, wrap to the first frame only if loop; duration 0 = hold.
Frame record (V, from readers): `+0x08` sound id, `+0x0c` byte **fire marker**,
`+0x10`/`+0x14` dx/dy in px applied per tick (Marine attack/death, Cannon move),
`+0x1c` duration in ticks. End of a non-loop sequence = current record null or empty
(sentinel `0x4c1300`). The frame sound plays only on the first tick of a frame
(`anim +0x14 == 0`) in every think (V, e.g. tail of `0x433db0`).

Shot release (V, all ground classes): while the attack state is active, on every tick
whose current frame has the marker byte set, the unit creates a new weapon instance
`0x46d900(self, weapon id)` (factory by class table `0x494d30`) and calls its `+0x7c`
with the launch position (`+0x14` with height 0x18 and the target). The marker is tested
every tick, not once per frame: a marker frame of duration > 1 would fire more than once
(G that data uses duration 1; needs an animation dump).

Per-tick common prelude (V in every think): reload countdown -1 if non-zero; chase target
dropped when dead, not visible to the tactic side (`side +0x690`) or beyond 0x480 px
(1152 px) with tactic callback `+0x134` (re-pick) or `+0x130` (dead); fire target dropped
when dead, not visible or `+0xc0` false; when there is no fire target (and, per class,
reload is 0) the unit asks tactic `+0x120` for one (Local_assign, see
`docs/idle_autoengage.md`); HP `+0x38` regenerates by descriptor regen per tick up to a
cap (two cap fields selected by `+0x54`).

Orders (V for the negative): none of the thinks reads an order field. Move, attack,
follow, ferry boarding and garrison are tactic-level (`docs/group_formation_leash.md`,
`docs/ferry_control_flow.md`); the unit only receives `+0xcc` step-to-tile (one tile at a
time, issued from tactic member step `+0x11c`), `+0xd0` chase target and `+0xd4` fire
target. A unit asks for the next step itself by calling tactic `+0x11c` when standing or
when near the end of the current step.

## 1. Marine (think 0x433db0)

Fields (V): `+0x64` state, `+0x68` descriptor, `+0x6c/+0x70` position x256,
`+0x74/+0x78` step destination (px), `+0x7c` anim, `+0x80` facing 0..7, `+0x84` reload,
`+0x88` fidget timer, `+0x8c` chase, `+0x90` fire target, `+0x94` running attack target,
`+0x98/+0x9c` step velocity x256, `+0xa0/+0xa4` looping sound object / id.

| state | meaning | anim (seq) | exit |
|---|---|---|---|
| 0 | gone | - | think returns at once |
| 1 | stand | seq 0, frame = facing (not advanced) | fidget -> 5; step -> 2; fire -> 3 |
| 2 | step between tiles | seq facing+1 (1..8), loop | arrive -> 1 (then fire test) |
| 3 | attack | seq facing+9 (9..16), no loop | sequence end -> 1 |
| 4 | dying | seq 0x11..0x13 (17..19) | sequence end -> 0, refcount `+0x34` -1 |
| 5 | fidget | seq 0x14..0x16 (20..22) | sequence end -> 1, random facing rand&7 |

- V `0x433820` step: refused in states 3 and 4; destination = tile*0x30+0x18; facing =
  (angle+0x10)>>5; velocity = speed (descriptor `+0x14`) x256 along the offset; seq
  facing+1 started (or switched with `0x403510` if already stepping); occupancy grid
  `0x4dd2f8` moved; tactic `+0x124` notified; state 2.
- V state 2: when distance > speed, advance; once within 3*speed of the destination and no
  shot is possible, call tactic `+0x11c` early (pre-request next tile). Else snap to the
  centre, seq 0 / facing, state 1, fire test, then tactic `+0x11c`.
- V fire (prelude, also on arrival): alive `+0x5c`, reload 0, fire target alive, state
  not 2/3, weapon `+0x78` can-fire -> state 3, facing = (angle+0x10)>>5 &7, seq facing+9,
  reload = weapon `+0x60` + 1, target moved to `+0x94`, `+0x90` cleared.
- V state 3/4/5: position moves by frame dx/dy; state 3 fires on marker frames (above).
  Reload is independent of animation length: shot period = max(reload+1, attack anim
  length + 1 for re-entry) ticks (G for the "max" wording; V that a new attack needs state
  != 3 and reload 0).
- V idle: state 1 calls tactic `+0x11c` every tick; fidget timer `+0x88` = rand%200+0x3c
  (60..259) ticks, at 0 picks seq 0x14+rand%3 and enters 5 only if that sequence exists;
  any non-stand state resets the timer to -1. State 1 also sets global `0x4cc690` = 1 when
  `0x4ab670` == 0 (G: "some unit idle" flag).
- V death `0x433960`: clears occupancy cell, byte `+0x60` = 0, state 4, seq 0x11+rand%3,
  releases `+0x94`, kills the looping sound, plays a random sound of descriptor `+0x44`.
  Kind 0x52 (82, UGI_ABDULA) also spawns sound list 0x96 and effect object 0x97. Kind
  0x1d (29, UGI_TUZEM) while dying stops advancing on a frame of duration >= 11 once global
  `0x4c12c0` is set (V; G: the latch from `0x43a280`, meaning unknown).

## 2. Spider (think 0x44b360): hull plus turret

Fields (V): `+0x64` hull state (0 gone, 1 stand, 2 move), `+0x68` turret state (0 aim,
1 firing), `+0x6c` descriptor, `+0x70` hull anim, `+0x74` turret anim, `+0x78/+0x7c`
position x256, `+0x80/+0x84` destination, `+0x88/+0x8c` queued tile, `+0x90` queued flag,
`+0x91` hull heading (0..255, snapped to 8 directions), `+0x92` turret heading, `+0x94`
reload, `+0x98` chase, `+0x9c` fire target, `+0xa0` running attack target.

- V step `0x44ab90`: while moving (state 2) it only queues the tile (`+0x88`, flag
  `+0x90`); otherwise heading = (bearing+0x10)&0xe0, hull seq (heading>>5)+1 loop, state 2,
  occupancy and tactic `+0x124` updated.
- V state 2: velocity = unit vector of `+0x91` * speed (descriptor `+0x1c`); arrival is an
  overshoot test per quadrant (`0x44bcf0`, `0x406da0`); then snap, tactic `+0x11c`,
  state 1; queued tile and `+0x94` free -> immediate `+0xcc`, else hull seq 0 loop.
  State 1: tactic `+0x11c` every tick (no fidget).
- V turret: turns toward the fire target bearing (or the hull heading when none) by
  descriptor `+0x34` per tick; pose = seek (heading+4)>>3 &31 (32 directions).
  Turret state 0 -> 1 when target alive, reload 0, can-fire, and turret heading equals
  **exactly** the bearing to the weapon aim point (`weapon +0x68`); the tactic is asked
  once more (`+0x120`) and the shot proceeds only if the target did not change.
  Seq dir32+2 no loop, reload = weapon `+0x60` (no +1), target moved to `+0xa0`.
  State 1 fires on marker frames; at sequence end turret seq 0 (no loop), state 0.
- V reload: at reload == 12 the turret switches to seq 1 keeping the frame (G: "loaded"
  pose); the tactic is asked for a target early when reload < (256/turn rate)/2, so the
  turret can pre-turn.
- V death `0x44ae70`: instant, state 0, refcount -1, both anims seq 0x7f, sound list
  0x96, effect objects 0x98 (rand%3) and 0x71 (rand%20) (G: explosion, debris). No dying
  state.

## 3. Cannon (think 0x417090): rotate-then-step, tile move by animation

Fields (V): `+0x64` state, `+0x68` descriptor, `+0x6c` anim, `+0x70/+0x74` position
x256, `+0x78/+0x7c` destination, `+0x80` pending move flag, `+0x81` heading byte,
`+0x84` reload, `+0x88` chase, `+0x8c` fire target, `+0x90` running attack target.

| state | meaning | anim | exit |
|---|---|---|---|
| 1 | stand | seq 0 at heading | fire target and reload 0 -> 2; else tactic `+0x11c` |
| 2 | rotate | seek (h+4)>>3 &31 | heading reached -> fire (4) or step (`+0xcc`) |
| 3 | move one tile | seq dir8+1, no loop, dx/dy from frames | end -> 2 if shot ready else 1 |
| 4 | fire | seq dir8+9, no loop | end -> 2 or 1 |

- V turn rate descriptor `+0x4` per tick. Fire bearing rounded to 8 directions
  ((a+0x10)&0xe0); move bearing exact. At the fire heading the tactic is re-asked
  (`+0x120`); same target -> reload = weapon `+0x60`, target to `+0x90`, `+0x8c` cleared,
  state 4. Marker frames fire.
- V step `0x416710`: accepted in states 1/2 only; if heading differs from the step
  bearing -> state 2 with pending flag; else seq dir8+1, first frame offset applied,
  state 3, occupancy and tactic `+0x124`.
- V death `0x416b90`: instant, same as Spider (seq 0x7f, 0x96, 0x98, 0x71).

## 4. Truck (think 0x460a10): steering with momentum

Fields (V): `+0x64` state, `+0x68` descriptor, `+0x6c/+0x70` position x256, `+0x74`
heading 0..255, `+0x78/+0x7c` destination x256, `+0x80` current speed, `+0x84` reload,
`+0x88` chase, `+0x8c` fire target, `+0x90/+0x91` anim started flags, `+0x94` anim,
`+0x98` engine sound cooldown.

States (V): 1 stand, 2 drive, 3 turn to target while driving, 4 firing while driving,
5 dying anim then state 0 (refcount -1). Fire target lost -> state 2 unless standing.
- V entry to 3: alive, reload 0, target alive, state 1 or 2, can-fire.
- V heading: 8 units per tick toward the destination (state 2) or toward the target bearing
  rounded to 8 directions (state 3); within 8 it snaps, the tactic is re-asked
  (`+0x120`) and with the same target state 4, reload = weapon `+0x60`.
- V speed (s = descriptor `+0x14`): if distance < 0x1800 (24 px): v -= 1+s*s/0x30, at <= 0
  v = 0 and state 2 -> 1; else if |heading error| < 0x38: v += s*s/0x30+1 capped at s,
  engine sound (descriptor `+0x3c` list) when not capped and cooldown 0, cooldown = 10;
  else decelerate the same amount, floor 0. Within 0x3000 (48 px) tactic `+0x11c`
  pre-requests the next tile.
- V anim `0x4607b0`: state 4: seq dir8+9 once (no loop); otherwise heading not on an
  8-direction -> static seq 0 frame (h+4)>>3 &31; aligned -> seq dir8+1 loop.
- V state 4 fires on marker frames; at sequence end state 2 and the fire target is
  released.
- V death `0x460550`: instant (state 0, seq 0x7f, 0x96, 0x98, 0x71). State 5 is therefore
  never entered by this handler (G: reached from another path, unknown).

## 5. Headtruck (think 0x42c880): Truck hull plus Spider turret

Fields (V): `+0x64` hull state (1 stand, 2 drive, 3 dying anim), `+0x68` turret state,
`+0x6c` descriptor, `+0x70/+0x74` position, `+0x78` hull heading, `+0x7c` turret heading
(rotated with the hull), `+0x88` speed, `+0x8c` reload, `+0x90` chase, `+0x94` fire
target, `+0x9c` hull anim, `+0xa0` turret anim, `+0xa4/+0xa8` smoke countdown/period,
`+0xac` engine sound cooldown.

- V hull: as Truck (turn 8/tick, speed field descriptor `+0x1c`, 24 px stop, 48 px
  pre-request, engine sound list descriptor `+0x48`, cooldown 10); anim `0x42c740`.
- V turret: as Spider (turn rate descriptor `+0x34`, exact-bearing fire, reload == 12 ->
  turret seq 1, early target request, seq dir32+2 attack, marker fires). The target stays
  in `+0x94` during the shot and is released at sequence end.
- V smoke: period `+0xa8` = 0 when slot `+0x3c` / slot `+0x38` < 4, else 10; 3 while
  accelerating; on countdown expiry effect object 0x69 is spawned near the unit (G: damage
  or exhaust smoke; slot `+0x3c`/`+0x38` are G max HP / HP).
- V death `0x42c4d0`: instant like Spider (both anims seq 0x7f). State 3 is not set here (G).

## 6. Bird (think 0x407b10), air base 0x4936c8

Fields (V): `+0x60` state, `+0x64` descriptor, `+0x68/+0x6c` position x256, `+0x70`
heading, `+0x74/+0x78` destination x256, `+0x7c` altitude, `+0x80` speed, `+0x84` reload,
`+0x88` anim started flag, `+0x8c` straight-flight hold, `+0x90` minimum speed, `+0x94`
single target (chase and fire), `+0x98` anim.

States (V): 0 gone, 1 stand/hover (anim advanced, tactic `+0x11c`), 2 fly, 3 special anim
(marker frame sets reload to 0; end -> seq 0 facing, state 1; G: who enters it unknown),
4 dying (end -> 0, refcount -1). Altitude +1 per tick up to descriptor `+0x1c`.
- V step `0x407820`: refused in 3/4; destination tile centre; state 2. No occupancy update
  until arrival.
- V fly: turn rate t = descriptor `+0x18`; turning when the error > 0x38-t or a target
  exists; with a target, error > 0x40 and distance < weapon range * 2 the turn rate is
  t/3 (wide pass). When |error| < t: heading snaps and, with target, reload 0, `+0xc0` and a
  weapon, the shot is released **immediately (no attack animation)** at the current
  altitude; reload = weapon instance `+0x60`; target released; tactic `+0x134` re-picks;
  attack sound descriptor `+0x50`. After any turn the hold `+0x8c` = 0x12 (18) ticks, then
  the heading drifts 1 per tick back to the nearest 8-direction.
- V speed: no target and distance < s+0x18 (s = descriptor `+0x14`): v -= 2 +
  (s*s/2)/(3s+0x18), floor `+0x90`; else |error| < 0x38: v += 2 capped at s. Arrival when
  the distance < (s+8)*3: tile update with tactic `+0x124`, then tactic `+0x11c`.
- V anim `0x407a60`: aligned to an 8-direction and hold 0 -> seq dir8+1 loop; else seq 0
  frame (h+4)>>3 &31.
- V death `0x407870`: state 4, seq 0x11+rand%3, sound list descriptor `+0x4c`; when the
  descriptor byte `+0x34` is 0 (G: mechanical, e.g. Deltaplan) also sound 0x96 and effect
  0x98.

## 7. Zeppelin (think 0x4719b0)

Fields (V): `+0x60` state, `+0x64` descriptor, `+0x68/+0x6c` position, `+0x70` heading,
`+0x7c` altitude (+1 per tick to descriptor `+0x18`), `+0x80` speed, `+0x84` reload,
`+0x8c` anim, `+0x90` target.

- V fire: in state 1 or 2 with reload 0, target alive and visible, can-fire: a weapon
  instance is fired **directly, no animation**, from the current altitude; reload = weapon
  `+0x60`; the target is kept (repeat bombing every reload ticks).
- V state 2: heading 8/tick; within 0x1800 (24 px) v -= 1+s*s/0x30, at <= 0 state 1;
  while farther than (v+0x18) px it accelerates by s*s/0x30+1 (cap s, error < 0x38) or
  decelerates; when closer the tile is updated (tactic `+0x124`) and tactic `+0x11c` asked.
  Anim `0x4718b0` (G: same scheme as Bird). State 1 does not advance the anim.
- V state 3: wait for the sequence end -> state 1 (G: entry unknown). State 4: dying,
  end -> 0, refcount -1. Death `0x471740`: state 4, seq 0x11+rand%3, sound list 0xf,
  effect 0x98 at the altitude (`+0x18`).

## 8. Reload semantics summary (V)

| class | reload set to | set when | new shot needs |
|---|---|---|---|
| Marine | weapon `+0x60` + 1 | attack anim start | reload 0 and state not 2/3 |
| Spider, Headtruck | weapon `+0x60` | turret anim start | reload 0, turret state 0, exact bearing |
| Cannon, Truck | weapon `+0x60` | anim start after turning | reload 0, heading on target (8-dir) |
| Bird, Zeppelin | weapon instance `+0x60` | at the shot | reload 0 (Bird also heading within t) |

Reload counts down every tick in all states, including during the attack animation and
(Marine excepted: not in state 4) while dying.

## 9. Unknowns

1. Frame durations of marker frames (multi-shot risk) and the frame indices of the markers
   in each attack sequence: needs an animation-file dump (static) or a live frame trace.
2. Who sets Truck state 5, Headtruck state 3, Bird state 3 and Zeppelin state 3 (none in
   the death handlers): search the other vtable slots (e.g. Bird 0x407720, 0x471250).
3. Meaning of `0x4c12c0` and the Tuzem death freeze; meaning of objects 0x71, 0x98, 0x69.
4. Spider/Headtruck turret seq 1 at reload 12: visual meaning (live run to confirm).
5. Exact semantics of slots `+0x38`/`+0x3c` used for Headtruck smoke.
6. Live-run checks: Marine shot period versus attack anim length; Bird pass behaviour
   (t/3 wide turn); Zeppelin repeat bombing of a kept target.
