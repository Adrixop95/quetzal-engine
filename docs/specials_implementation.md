# Towers, aircraft and special effects implementation

**V** = inspected reference rule or existing public descriptor fact; **G** =
independent engine approximation or unconfirmed detail. Synthetic coverage of
a V rule does not establish live parity. No original executable was run.
The checkout already contained the ground-weapon and fire/air/siege work;
this change finishes the scoped tower cycle, aircraft target handling and
protected damage callbacks.

## Towers

**V:** Defence uses descriptor d4 as its turn step and d63 as its launch height.
The heading difference is signed modulo 256. An error at least one whole step
turns without starting a shot; an error smaller than one step snaps to the aim.
Steps 8, 32 and 128 remain distinct. Reload uses the weapon getter without the
Marine extra tick. See `0x41d790` and [towers_air_specials.md](towers_air_specials.md).

**V:** A missing target is requested every ready tick, including ticks skipped
by the former four-tick scan. During reload, acquisition is permitted inside
the early `(256 / step) / 2` window. Dead, invisible, out-of-range and
non-positive-damage aim targets are discarded. A shot latches its victim and
clears the aim target. There is no occupant or ammunition requirement.

**V:** Resource-backed attacks use turret slots `2 + dir32`, or base slot 8.
Each marker tick releases the weapon with the descriptor launch height.
Defence first tests frame zero on the update following initiation, then
advances the animation. Completion releases the latched target. New commands
do not replace that victim; source death cancels the shot. Frames are read
from the user's archive; no frame data is embedded. Marine timing is separate.

**G:** Target requests use the clone's nearest eligible enemy policy. Native
tactic scoring is not reproduced; the alignment-time recheck uses the same
nearest-enemy substitute and requires its assignment to remain unchanged.
Activation retains the placement heading instead of consuming a native random
heading. Reload cues, damage sparks and exact native pool ordering remain
unmeasured. Without animation resources the existing immediate-release fallback
remains explicit. Arrow height uses linear interpolation from muzzle to
snapshotted aim height; native subpixel trajectories still need comparison.

## Birds and Zeppelins

**V:** Bird and Zeppelin are airborne even at low altitude. Height rises one
pixel per live update to the descriptor cruise height, without landing. Bird
starts at 30; Zeppelin starts at zero (`0x471390`, placement inspection).
This supersedes the statement in [fire_air_siege.md](fire_air_siege.md) that
both start at 30.

**V:** Bird chases its target, turns at its descriptor rate, applies the close
target wide-turn branch and fires only nose-on, ready and in range. Alignment
also gates Direct-mode attacks. A shot releases the target; the clone requests
another on the following update and keeps flying toward its last goal. Held
Bird targets do not use the Marine visibility/leash drop. Zeppelin fires in
range without a heading gate, retains its target and drops invisible targets.
It flies to ordered destinations rather than chasing its bombing target.
Arrival ends movement without losing that target. Aircraft retain their
flight/hover visuals when firing.

**G:** Integer steering, movement phase, Bird circling and tactic reassignment
remain clone policies. Reacquisition can choose the same victim. Newly idle
Zeppelins still share the generic acquisition cadence. Complete flight-state
and animation alignment require measurements; exact flight paths are not
claimed as reference parity.

## Existing specials retained and checked

| Label | Implemented rule |
| --- | --- |
| V | Snowman adds 18 to victims' attack countdown, without a movement stun, facing change or secondary-HP change. The bounded rectangle query and category filters have existing regression coverage. |
| V | Class 8 snapshots sixteen explosion positions, consuming release/radius/angle randomness in that order. Release is modulo ten, radial scatter is modulo the descriptor radius (96 for campaign launchers), and the emissions run over ten subsequent updates. `rec[3]` is reload, not a count. |
| V | Knight/Ninja apply full central damage and half side damage. **20 and 30 are angular sector widths, not pixel radii**: `0x46e8b0` queries within 96 pixels, with side sectors centred 25 angle units either side. The existing implementation follows the corrected [ground audit](ground_weapons.md). |
| V | Class 11 evaluates each intersecting victim every active update: base 2000, quarter damage for allies, then the victim's 8.8 fire factor. Source and airborne victims take zero. No victim-attached burn tail is added. |
| V | Fire factors remain descriptor-driven: zero means immune, Zeppelin uses 512 and Headtruck records 1 through 7 use 128; record zero uses 256. Explosion inner damage bypasses this factor; its falloff band uses it. |
| V | Building protection byte d59 blocks damage callbacks (`0x410560`). Direct, projectile, fire and blast delivery now share the guard. Scripted removal and town capture keep separate semantics. |
| V | Hypno tower weapon 57 is ordinary category damage with reload ten, no secondary-HP change or conversion. The class-13 correction in [specials_commands.md](specials_commands.md) applies. |

**G:** Native random-stream correspondence, spatial query order, sweep edge
membership and projected explosion query geometry remain unmeasured. The
clone uses its deterministic stream and stable object order. Launched volley
bursts survive source death; native pool phase and cancellation need comparison.

## Regression and validation scope

`towers_specials` uses invented animation banks and worlds to cover base/turret
markers, deferred frame-zero testing, launch heights, reload, ready-tick
acquisition, whole-step aiming boundaries, unfinished towers, invalid air/ground
targets, pause/Stop during retained shots, protected halls, fire factors, ascent,
Bird re-picking and Zeppelin bombing. Every tower animation tick and a repeated
Bird pass are compared with fresh command-log replay. `ground_weapons`,
`engine_unit`, `combat_unit` and `hypno_ownership` retain Snowman, sweep,
sixteen-burst volley, fire-stream and plain-damage checks.

Reference record/replay and exact normalized comparisons are unavailable under
the explicit prohibition on executing the original. They do not block these
independent changes. Facility-service timing, hero abilities, native tactic
scoring and exact RNG remain separate dependencies; they are not claimed done.

Verification: the Release build passed. CTest passed 74 entries with six
private-data/network entries skipped and no failures. Pre-commit passed for
tracked files and separately for the new test/document; `git diff --check`
passed. The corrected AI fixture attacks an ordinary town building and checks
that its protected hall remains intact. A clone-only mission 1 smoke reached
the configured 120-tick bound using dummy SDL inside `unshare -rn`, with a
private installation copy and no script errors or unsupported executions.
Its bounded `timeout` outcome is not campaign completion. No original run or
reference comparison was performed, and all changes remain uncommitted.
