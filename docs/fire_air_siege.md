# Fire, air and siege investigation

Evidence date: 2026-11-21. **V** means inspected original behavior or an existing
public descriptor value, **G** means an engine approximation. No live reference
run was possible in this environment. Synthetic checks establish internal
behavior and replay, not reference parity. Private decompilation remains under
ignored `work/fire_air_siege/`; no executable-derived source is reproduced here.
This document supersedes the older guesses in `weapon_class_map.md` where noted.

## Fire and burning

**V** Class 11 is vtable `0x4927a8`. `0x429c20` returns reload **40** ticks;
`0x429c30` returns range **96** pixels. `0x429c00` returns ten times the
single-update damage from `0x429f60`. The ten is a potential-damage estimate,
not an extra multiplier on every update.

**V** For a victim, one application of `0x429f60` is zero for the source itself
or an airborne object. Otherwise start at **2000**, multiply by **64/256** for
an allied side, then by the victim's virtual `+0x108` multiplier. Integer HP
conversion follows the fixed-point operations. With ordinary resistance this
is **2000 enemy / 500 allied HP per active update**, at most **20000 / 5000**
for ten uninterrupted contacts. Neutral and allied side relations are distinct.

**V** `0x42a010` starts the weapon's own animation 157 once, in its owner's
32-direction facing. It does not create a victim-attached burn object.
`0x42a0b0` follows the owner's current position/facing with a 24-pixel muzzle
offset, advances the flame animation and queries a segment of range 96 through
`0x470600`. That query intersects live object boxes (`0x4707d0`) and admits both
unit and building object categories. Every victim gets its own damage
calculation on every active update. The owner reference is retained; this path
does not test the owner's alive flag. Leaving the segment stops damage. There
is no independent burn duration refreshed on the victim. Animation 157 has ten
one-tick frames in the existing resource inventory. The end-of-animation and
pool iteration phase still need live confirmation.

**V** Types **35** and **59** select weapon **34**, class 11. Type **8** selects
weapon **58**, class 1: speed **50**, reload **65**, range **300**, base damage
**19000**, secondary weapon **0**, armor coefficient **0**, duration **0**. Its
category factors are those in the existing arrow record. The apparent flame
name does not give it class 11 behavior or a damage-over-time tail.

**V** The separate helper at `0x404730`, vtable `0x48f6c4`, is the arrow/direct
weapon residual-damage accumulator. `0x4041e0` only creates it for the armor
component when that component is nonzero; its duration comes from arrow d6.
The update applies successive cumulative-damage differences and decrements its
duration, stopping on a dead victim. It is not created by class 11 or by type 8's
zero armor coefficient. General residual scheduling outside these scoped fire
weapons belongs to the ground-weapon task; no universal burn status is inferred.

**V** The Chinese firethrow tower **68** is an ordinary Building, not Defence.
Its Building vtable `0x48fd20` has no weapon getter and its update `0x410cc0`
contains building/damage-decoration/harvest behavior, not flame dispatch.
Its public descriptor has d60=70, period d64=100 and repair supply d62=30,
d320=10. Its construction, service protection and economy remain handled by
the existing Building systems. Its name is not evidence of a hidden weapon. Its +0x124 callback (`0x410560`)
also ignores damage when the descriptor protection byte is set. The clone
already applies this flag to town service; complete generic hit-callback
routing remains a ground-weapon dependency. Its zero +0x108 factor already
prevents class 11 damage.

**G** The clone uses its provisional object boxes for the flame intersection
and the established ten-frame inventory for the active duration. Reference box
hotspots, exact endpoint inclusion and sprite-resource changes need measurements.
It follows source movement and facing, includes allied/neutral bystanders and
retains source identity through death. Flame age and all scheduled combat state
participate in deterministic hashes and command-log replay.

## Air units and targeting

**V** The actual Bird weapon getter is **`0x407780`**, reading descriptor **d9**.
The earlier document's `0x42c3c0` is the **Headtruck** getter, not Bird. Zeppelin
uses **`0x471680`**, descriptor **d8**. Both reject id -1 or non-positive damage
against the proposed victim. Cannon uses descriptor **d7**. These fields were
already loaded in the clone; the former status text was stale.

| Type | Weapon and dispatch (V) | Maximum speed, px/tick (V) | Cruise height, px (V) |
| --- | --- | --- | --- |
| Eagle 33 | d9=-1, unarmed | 15 | 110 |
| Deltaplan 34 | 35 = two arrows 36, each dispatching explosion 37 | 18 | 110 |
| Paraplan 60 | 52, direct category damage | 12 | 85 |
| Zeppelin 7 | d8=-1, unarmed | 8 | 95 |
| Zeppelin2 85 | 44 = two zero-horizontal-speed carriers 43, impact explosion 42 | 9 | 110 |

**V** Bird speed d5 is a maximum, not a constant getter for the current speed.
Bird initialization (`0x407520`) chooses a minimum speed in 6..12 using the
reference random stream. In flight (`0x407b10`) it turns toward the goal using
d6 angle units per update (Paraplan 8, Deltaplan 7, Eagle 6), accelerates by 2
when the heading error is below 56 angle units, and caps at d5. Without an
attack target near the goal, braking is
`2 + floor(floor(d5*d5/2)/(3*d5+24))`, bounded by that individual minimum.
When an attack target is behind by more than 64 angle units and closer than
twice weapon range, the turn rate is divided by three. A Bird fires during a
flight pass when aligned, ready and able to hit, then releases that target for
its tactic to assign another. It does not select a separate firing animation.
The clone retains its explicit attack target between passes (**G** tactic policy).

**V** Zeppelin turns by 8 angle units per update. Its acceleration/braking step
is `floor(d5*d5/48)+1`, bounded by zero and d5. It can fire while idle or moving,
independently of heading alignment (`0x4719b0`). Aircraft start at height 30;
Bird and Zeppelin rise by one pixel per update to d7 and d6 respectively.
These heights are world/render heights, not a second air-target category.

**V** Deltaplan weapon 35's d2=25 is the paired launch offset, **not reload**.
Each child 36 uses speed 15, reload 30, range 140, animation 158 and secondary
37. Its direct and armor terms are zero. Explosion 37 is ground-only, inner
radius 20, outer radius 30, centre damage 1500. Thus an overlapping pair can
deal 3000 before spatial falloff. It cannot attack air.

**V** Paraplan weapon 52 has range 200, reload 20, base 2600, air multiplier
256/256 and zero multipliers against non-organic targets, either Marine category
and structures. It can attack ordinary Bird/Zeppelin targets because their
category virtuals do not classify them as non-organic. The beam effects do not
supply a travelling projectile delay.

**V** Positive-damage anti-air choices in the current public records are Rifleman,
Rus watch tower, Archer, Aztec archer tower, Bizon, Paraplan and Chinese air tower.
The direct/arrow air factors are respectively 320/256, 320/256, 256/256,
256/256, 160/256 and 256/256; the air tower uses an air-only explosion. Ground-only
explosions, Deltaplan and class 11 reject airborne victims. Eagle and Zeppelin 7
are unarmed; a missing weapon does not fall back to a generic shot.

**G** The clone uses integer trigonometry and an isolated deterministic random
stream. Native RNG, steering/shot phase order, Bird target release, idle circling,
resource-driven alignment/cooldown animation and tactic reacquisition are not
claimed as full reference parity. Flight state, altitude, cooldowns and scatter
are hashed. Ground navigation does not block airborne pursuit.

## Facilities

**V** Types **25, 52, 79** are unarmed Building producers. The existing build
recipes connect Rus air facility 25 to Zeppelin2 85 (developed hall level 3),
Aztec air facility 52 to Deltaplan 34, and Chinese air facility 79 to Paraplan 60
(levels 2/3 where the recipes expose them). Eagle 33 uses nest **46**, not air
facility 52. The original unarmed Zeppelin 7 is not a replacement for type 85 in
these production recipes. Existing town queues/prerequisites implement these
connections; no autonomous free aircraft or weapon is added to the facilities.

**V** Type **81** is Defence and selects weapon **20**, class 8, payload **18**.
It shoots only air targets, range **200**, reload **50**. See the dispatch table
below. Acquisition, footprint extents, resource firing-frame timing and computer
assignment still share the independent engine's provisional tactic policy.

## Catapult, Hopcannon, bombs and rockets

**V** Class 0 ballistic carriers use d1 for horizontal speed, d2 for reload,
d3 for range and d4 for impact-weapon id (`0x406960`, `0x4069c0`, `0x406c00`).
The launch snapshots an aim point with a two-pixel Y offset. With speed v>0,
flight parameter `T=floor(distance/v)`; horizontal velocity points toward that
aim with magnitude v. Vertical speed starts at zero. With launch height H,
vertical acceleration is `2*H/(T*T)`; for T=0 it is 2 pixels/update. Each update
moves horizontally, subtracts the current vertical speed from height, then
increments that speed. The impact weapon dispatches when height reaches or
passes zero. The projectile is not clamped to the aim point on the final tick.
Exact subpixel arithmetic and the first pool update still need live comparison.

| Shooter | Carrier(s) | Speed | Range | Reload | Impact (V) |
| --- | --- | --- | --- | --- | --- |
| Catapult 83 | 32 | 30 | 200 | 30 | 3: damage 3000, radii 24/72, ground |
| Hopcannon 87 | 2 | 30 | 250 | 37 | 3: same splash |
| Zeppelin2 85 | 43 + 43 through 44 | 0 | 30 | 48 | 42: damage 3000, radii **30/80**, ground |

The previous bomb radii 80/120 were a shifted-field reading: **120 is the
explosion's range field**, not its outer radius. Paired weapon 44's value 15 is
its muzzle offset, not reload. A bomb falls at its launch ground position;
it does not acquire horizontal speed 48. The clone now uses the aircraft's
current height for its falling carriers. Ground launch height still falls back
to 24 pixels (**G**) without the relevant animation inputs.

**V** Class 7 blast calculation (`0x44c130`) first requires the victim's air flag
to equal d5. Let r be the victim's reference bounding half-width, D the centre
distance, I=d1 and O=d2. Damage is zero outside O+r, full d4 inside I+r; the
outer band is proportional to `(O+r-D)/(O-I)`. In that band, virtual +0xe8
applies 204/256, then virtual +0x108 supplies the resistance multiplier. The
inner branch returns full damage directly. This path includes allied victims.
The clone's pre-existing structure/category approximation and collision discs
remain **G**; this task loads the correct +0x108 descriptor factors for the scoped
families but does not certify reference bounding geometry or rounding.

| Class 8 weapon | Shooter | Payload | Scatter radius | Range | Reload (V) |
| --- | --- | --- | --- | --- | --- |
| 19 | Katusha truck 9, wheel 58 | 17: 3000, radii 15/25, ground | 96 | 200 | 80 |
| 20 | Chinese air tower 81 | 18: 3000, radii 15/25, air only | 96 | 200 | 50 |
| 54 | Johnka's special getter | 53: 3000, radii 15/25, ground | 96 | 300 | 30 |

**V** Factory `0x46d900` creates class 8 with vtable `0x494430`.
`0x4039a0`/`0x4039b0` read **d2 range / d3 reload**. `0x44ca60` stores **16**
impact positions, each with a random dispatch bucket 0..9, radial offset
`rand()%d1` and angle `rand()&255`. `0x44cb10` dispatches matching positions
on each of ten updates. Each dispatch creates the payload explosion directly;
there is no travelling rocket object or separate rocket speed in this path.
The old interpretation of **80/50/30 rockets** was incorrect. `0x44c8d0`
delegates the damage estimate to one payload; it does not multiply by sixteen.

**G** The clone schedules these sixteen snapshot explosions on the next ten
simulation updates, preserves them after the source dies, and includes every
spatial victim of the right air category. RNG sequence, draw order, pool timing,
projected-height query bounds and shared-stream interactions need comparison.
Johnka's inspected special getter (`0x42d950`) now selects weapon 54; its
existing vessel enable/transport controls remain in force. Transport tactic
selection and native firing phase still need their own observations.

## Remaining live run sheet

Run on a namespace-capable host only, with the reference and clone built inside
that checkout. Follow `EXPERIMENTS.md`: a distinct private game copy per process,
immutable asset symlinks, private writable Maps/Saves/profile state,
`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`, `unshare -rn`, and a hard timeout.
Do not bypass namespace setup. Keep records and captures private. Each row needs
two independent native runs and a matching clone scenario, with the revision,
diff hash, command log, stable creation identities and every-tick observations.

| Probe | Setup | Record / acceptance question |
| --- | --- | --- |
| Flame cadence | 35 and 59, one stationary target at several headings/distances | Spawn, first/last active frame, HP each update, 40-tick readiness and animation delay; distinguish estimated 20000 from actual contact damage |
| Flame geometry | Enemy, ally, neutral, air and building boxes crossing the segment | Exact 24-pixel muzzle point, segment endpoint, sprite hotspots, boundary inclusion and virtual +0x108 rounding |
| Flame interruption | Move/turn/stop/kill caster or victim during ten active frames | Source retention, whether only residual flame updates continue, and no victim-attached tail |
| Truck and named tower | Type 8 on organic/structure targets; completed 68 beside enemy | 58's single impact and zero residual duration; confirm 68 has no attack launch while tracking harvest separately |
| Air motion | 33/34/60/7/85 moving straight, reversing and approaching a goal | Initial height 30, ascent, cruise maximum, per-instance minimum, acceleration, turn rate and overshoot/circling |
| Bird attack pass | 34 or 60 crossing range with target ahead/behind/moving | Alignment threshold, first shot, cooldown, target release/reassignment and shot-versus-movement phase |
| Anti-air | Every positive/zero category in the table, stationary and moving air | Acquisition versus explicit order, visibility, direct beam impact, resistance and airborne bystander immunity |
| Facilities | One of 25/52/79 with and without its recipe prerequisites | Available aircraft type, paid queue time, producer exit and hall-level gates; distinguish nest 46 |
| Carrier flight | 83/87 at short/full range; 85 at several heights | First movement update, T calculation, height/vertical speed, overshoot, exact impact update and paired offsets |
| Splash | Payloads 3/37/42 at inner/outer boundaries and fractional distances | Bounding half-width, inner-branch bypass, +0xe8 multiplier, +0x108 arithmetic and allied inclusion |
| Rocket schedule | 9/58/81, fixed target, move/kill it after launch | Exactly 16 positions, ten buckets, radial scatter, reload 80/50, first pool phase, target-height query and damage per victim |
| Determinism | Repeat all above and command-log replay | Native repeatability versus clone repeatability; these are separate claims |

## Checkout verification

Baseline and final Release builds passed with `AZTEC_GEN_DIR=` and `GAME_DIR=`.
Final CTest: **63 passed, 6 skipped, 0 failed**. Skips were the five private-data
entries and restricted network process test. Pre-commit passed on all tracked
files and separately on this new document. The pure engine suite also passed
with AddressSanitizer and UndefinedBehaviorSanitizer (`detect_leaks=0`).

Synthetic cases cover flame duration, ally/neutral/air restrictions, source
movement/death, truck zero residual damage, unarmed facilities, air production
recipes, category dispatch, carrier/bomb impact timing, Deltaplan splash,
rocket snapshot/delay/source-death behavior, flight acceleration/turn/ascent,
pause and command-log replay. No original runtime was launched. Task 140
general residual damage, generic hit callbacks and native animation-marker
scheduling remain separate dependencies; live geometry/RNG/phase checks remain
on the run sheet above.
