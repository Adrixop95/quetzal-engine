# Hypno family, special objects and ownership

Evidence: static research, completed 2026-11-18. **V** identifies facts read
from the original's private decompilation, instruction listings or the existing
public descriptor tables. **G** identifies engine policies or unresolved
interpretations. No original executable or reference runtime was run for this
task. Synthetic tests establish engine behavior, not reference parity.

The research inputs were read only. Additional functions were decompiled in a
private temporary copy of the Ghidra project, with read-only processing and no
analysis. No decompiled or lifted implementation, executable bytes, assets or
mission data are included here. This document contains factual descriptions and
addresses only. Read it alongside [weapon_class_map.md](weapon_class_map.md),
[hypno_recon.md](hypno_recon.md) and [ownership_mechanism.md](ownership_mechanism.md).
The corrections below supersede the older ownership document's interpretations
of the capture predicates and its unresolved script-gift executor.

## Building and unit results

| Label | Object | Descriptor, handlers and result |
| --- | --- | --- |
| V | Chinese hypno tower, type 69 | Defence parameter 8, 25000 maximum HP, weapon 57. It uses the common Defence vtable `0x4905bc`, getter `0x41d320`, think `0x41d790` and initialization `0x41d1a0`. Target assignment, aiming, attack animation and weapon dispatch use the ordinary tower path. No conversion, secondary-HP attack, prisoner creation or separate spell handler occurs there. |
| V | Weapon 57 | Class 13, parameter 10; direct damage through `0x46dfe0` and effect `0x46e0d0`. Reload 10, damage 3300, range 200. The record has zero duration and periodic-damage fields. Air multiplier 0, nonorganic 256/256, the two Marine category multipliers 512/256 and 384/256, structure multiplier 128/256. Compared with Hypno weapon 56, the tower reloads faster and has a different sound. The hit ends in target virtual `+0x124`, not set-side. |
| V | Hypno school, type 72 | Building parameter 42, 25000 maximum HP. Common Building vtable `0x48fd20`, constructor `0x40fb70`, completion `0x410010` and think `0x410cc0`. Its descriptor has no harvest rate, repair yield or special periodic producer. It is a technology prerequisite: completed school ownership anywhere on the side unlocks Hypno 54 and tower 69. The town-local prerequisite is barracks 67. |
| V | Prison, type 73 | Building parameter 30, 25000 maximum HP, the same generic Building handlers. It has no prisoner collection, conversion process, release command or periodic unit spawn. Completed prison ownership anywhere on the side unlocks Chief 55, with barracks 67 required in the producing town. |
| V | School/prison production | The build-tree recipes carry the side-wide prerequisite suffix. Town production `0x459bb0` consumes queue entries and initializes completed objects; `0x45c930` creates objects and attaches their tactic. These facilities unlock ordinary town production; neither runs an independent training queue or changes a victim's owner. |
| V | Chief, type 55 | Marine parameter 15, primary/secondary HP 5000/5000, natural threshold 5000, regeneration 25, movement descriptor 10, weapon 55. Weapon 55 is class 2, parameter 8: reload 6, damage 2000, range 10; air 0, nonorganic 768/256, both Marine categories 128/256, structures 512/256. No rally, healing, capture bonus or ownership virtual override was found. |
| V | Abdula, type 82 | Marine parameter 5, primary/secondary HP 12000/3000, natural threshold 12000, regeneration 20, movement descriptor 10, weapon 59. Weapon 59 is class 2, parameter 11: reload 11, damage 3500, range 10; air 0, nonorganic 192/256, both Marine categories 256/256, structures 128/256. No hero-specific conversion or healing override was found. |
| V | Chief/Abdula effects | `0x46ed90` applies the class-2 direct hit through target `+0x124`. Its optional periodic helper depends on the record's extra damage/duration fields; those fields are zero for weapons 55 and 59. Other operations create sound and visual effects. There is no owner change or attacker heal in these two weapon records. |
| V | Elefant, type 57 | Marine parameter 11, primary/secondary HP 28000/28000, natural threshold 14000, regeneration 24, movement descriptor 4. Weapon 47 is class 12, two copies of carrier 46; carrier 46 is class 0 and delivers class-9 weapon 45. The two-component effect `0x41e4a0` launches the children at separated muzzle positions. There is no separate stomp, transport, hypnosis or healing virtual. Projectile and impact fidelity depends on the shared weapon implementation. |
| V | Shared Marine virtuals | All three use vtable `0x4932cc`, constructor `0x433190`, think `0x433db0`, damage `0x4336b0` and death `0x433960`. The descriptor bytes used by `0x432fa0`, `0x432ff0` and `0x433000` are category predicates, not commands. Target assignment methods `0x433650` and `0x433c50` retain target references; `0x433c80` activates/deactivates map occupancy. Healing `+0x104` is the generic capped HP addition `0x4029d0`; these units' weapons do not invoke it as a special ability. |
| V | Shipyard, type 86 | Defence parameter 6, 60000 maximum HP, weapon field -1. It uses the same Defence methods as the tower, and the weapon getter returns null. There is no vessel production handler or shipyard-specific command in that class. It is absent from the ordinary build tree; scripted vessels are loaded separately by scenario handlers. Its Defence predicate still counts it as a capture defender despite its lack of a weapon. |
| V | Johnka, type 88 | Dedicated vtable `0x492d64`; initialization `0x42dba0`, think `0x42dce0`, weapon getter `0x42d950`, target retention `0x42dc10`. The getter returns weapon prototype 54 for non-airborne targets. Weapon 54 is a class-8 volley: 30 rockets, scatter radius 96, range 300, ground explosion payload 53. The visual object has a zero maximum-HP getter, not a Marine descriptor. There are no passenger-list or ownership-changing methods in the Johnka object/controller examined here. |

**V:** Hypno's special predicate is Marine virtual `+0xf8`, method `0x433000`,
which reads descriptor byte `+0x2e`. The kill-time estimator `0x46fac0` uses the
target's secondary HP when that predicate is true; it does not subtract those HP.
Chief, Abdula and Elefant have that byte clear. The ordinary damage and death paths
remain those described in [hypno_recon.md](hypno_recon.md).

**V:** The four proposed additional predicate sites are a different virtual call:
`0x447f90` and `0x4482a2` dispatch on the selected **tactic** from side commands;
`0x4551f4` and `0x455323` dispatch on the tactic being updated by `0x454820`.
Tactic slot `+0xf8` resolves to `0x44fc10`, which appends a path point and updates
the queued-point count. These calls take coordinates. They do not invoke the
Marine predicate and do not change another object's owner, HP or hypnosis state.

## Camera object and script presentation

**V:** Type 89 uses the base object vtable `0x48f2d8`, not the Marine class.
`0x402330`, the script camera-activation executor, looks up a side camera record
through `0x4470a0`, creates a type-89 marker at its tile position and gives it the
sight mask `clamp(record third value + 59, 64, 71)`. The second script argument
sets the marker lifetime. Generic think `0x402a50` decrements a positive lifetime
and destroys the marker on reaching zero; zero and negative values do not count
down. This is temporary vision, not viewport movement or damage.

**V:** Camera deactivation is command `0x21`, executor `0x402f10`. It finds the
most recently stored marker for that camera record and deletes it if it still
exists. Activation does not first delete an older marker. An older activation
can therefore outlive the latest one. The marker is not a member of a combat
tactic or an ordinary unit in the side's live-unit list.

**G:** The engine represents these invisible markers in World script-camera state
rather than as selectable objects. Each marker retains its owner, location, mask,
lifetime and whether it is the record's current marker. It contributes to the
owner's visibility only, independently of the viewing side. Expiration removes
current vision while retaining the normal explored floor. Marker state is hashed
and reconstructed by script replay. One lifetime decrement per following engine
tick is the scheduling policy pending the live control below.

## Ownership mechanism and event audit

**V:** Object tactic assignment `0x43ecf0` calls object virtual `+0x7c` if the
new tactic's side differs. Base set-side `0x43ed50` changes object field `+0x40`;
for a registered object (`+0x5d`) with a tactic it removes the old side-list entry
and adds the new one. Building override `0x410ca0` and Defence override
`0x41d770` delegate to that setter and refresh their attached presentation object
through `0x42a940`. These are the two direct setter callers in the lifted listing.
They are not special conversion abilities.

**V:** Side-list wrappers `0x449ab0` and `0x449ac0` delegate to `0x447010` and
`0x447250`; those update the side's object collection and per-type counts. The
complete wrapper-call audit in the private instruction listing is:

| Label | Wrapper | Call instruction addresses and purpose |
| --- | --- | --- |
| V | Add, `0x449ab0` | `0x410046`, `0x41d1b9`, `0x4334ec`, `0x44a7a0`, `0x40753c`, `0x4163b0`, `0x42c1ec`, `0x4602cc`, `0x47156c`: ordinary Building, Defence, Marine, Spider, Bird, Cannon, Headtruck, Truck and Zeppelin initialization/completion. These register existing ownership rather than converting a target. |
| V | Add, `0x449ab0` | `0x43ed97`: registered object enters its new owner's list. |
| V | Remove, `0x449ac0` | `0x410a44`, `0x43ea95`, `0x43eca3`: building destruction, generic damage death and explicit object removal. |
| V | Remove, `0x449ac0` | `0x43ed7a`: registered object leaves its old owner's list. |

**V:** Actual cross-side changes found in the object-setter call paths are:

- **Capture:** `0x45b1e0` invokes `0x45a920`, changing the town tactic's side,
  setting its hall/building members' sides directly and reassigning mobile
  members to the capturer's receiving tactic. Pending construction and attached
  town state are also handled. Thus the older statement that *every* side change
  requires AddMember is too strong: capture sets some members directly.
- **Scripted gift:** parser `0x4382c0` enqueues command `0x18` through
  `0x45f3b0`; dispatch `0x402f80` uses the table at `0x49b15c`. Entry `0x18`
  resolves to `0x402d50`. A town uses `0x45da00`, a separate explicit town
  transfer path. Other tactics are handed to a receiving-side tactic through
  its member-transfer method. Assignment then reaches `0x43ecf0` and set-side.
- **Tactic reassignment:** `0x454090` (AddMember) handles loaders, newly produced
  units, regrouping, group merging and transport boarding/unloading. These use
  the destination tactic's side. Normal same-side regrouping is not conversion.
  Initial object construction/save restoration also establishes a side without
  implying that an existing enemy object has changed allegiance.

**V:** Other `+0x7c` calls in unit and weapon effects dispatch on **weapons**;
`0x41e4a0` invokes child effects. Calls in `0x447050` and `0x430cb0` dispatch on
tactics and destroy them; the sound-system sites near `0x4792dd`/`0x47a038` use
unrelated interfaces. Slot offsets alone do not identify ownership operations.
No additional victim-side conversion event was found for the scoped objects.
This is a bounded call-path audit, not a claim about unexamined editor behavior.

## Capture corrections and engine behavior

**V:** The capture candidate is non-allied and non-airborne, with squared **tile**
distance below five. Neutral sides are also non-allied; an explicit enemy
relation is unnecessary. Defence scanning uses radius four for outposts and
eight for developed halls. The flag at town `+0x15c` is the hall level/category,
set from `0x45c4f0` during production completion `0x459bb0`, rather than an unknown
tower-presence flag.

**V:** The defender test excludes ordinary Building objects: their object-kind
virtual `+8` returns one and `+0xfc` returns false. Defence objects also return
one for kind, but true for `+0xfc`; even an unarmed shipyard defends. Marine kind
returns zero, so a living Marine counts independently of its weapon. Defenders
are not restricted to the ground. The town's active-construction helper is an
additional guard when its current object answers true to `+0xfc`.

**V:** There are two reference scan branches. The spatial-query branch checks
the owning side's registered list for defenders; the global-pool branch tests
allied sides as well. The selected branch depends on object count versus the
search area. Its scan ordering and decompiler coordinate recovery cannot be
replaced by a nearest-enemy assumption without evidence.

**G:** The engine now implements the established tile thresholds, hall radius,
Building/Defence distinction, airborne defenders and non-allied candidates.
It consistently allows allied defenders and uses the last eligible object in
stable World order. Exact spatial-list ordering and branch equivalence remain
in the run sheet. An active Defence construction queue object also protects its town outside the
normal radius; that queue entry represents the native construction helper.
Town transfer continues to cancel/refund pending production as an existing engine
policy; the exact original refund/queue behavior is not established here.

**G:** Script gifts and captures now share an ownership helper that updates both
World ownership and the movement agent's team immediately. HP and secondary HP
are preserved. Town state follows the recipient. Ordinary damage never calls
this helper. Existing script identity, order stopping and cargo policies remain
engine representations rather than native tactic-layout compatibility.

**V/G:** Scripted Johnka patrol now uses its own terrain-independent diagonal
motion, five pixels on each axis per visual think (`0x42dce0`). The enable/disable
controller `0x42e0a0` waits for a turn phase before deactivation, and reactivation
restarts at the stored tile center. The engine retains orientation and hashes
patrol/turn state; one visual think per engine tick is G. Scripted Johnka objects reject boarding; no Johnka passenger
capacity was verified. The older generic player-controlled transport fallback
for non-scripted type-88 objects is not evidence that the original carries units.

## Dependencies and pending live run sheet

Task 141's class-8 volley scheduler is absent from this checkout. The work that
does not depend on it is implemented: Johnka's weapon-54 mapping, preservation
of the volley/payload distinction, patrol and enable/disable lifecycle. Its
volley still lacks verified scheduling/getter integration and consequently does
not fire in the engine; it must not become a fabricated single direct shot.
Task 140/shared projectile work also controls complete Elefant impact fidelity.
These dependencies do not block the ownership and facility findings.

The original cannot be run in this environment. On a capable host, use the
private symlink installation and `unshare -rn` isolation from
[EXPERIMENTS.md](EXPERIMENTS.md), with dummy SDL video/audio. Keep raw logs and
all game/research artifacts private. Record object identity beyond reused pool
handles, type, side, tactic, primary/secondary HP and tick at each event.

| Label | Control | Measurements still needed |
| --- | --- | --- |
| G | Tower hit and kill against ground targets | Attack-animation dispatch tick, effect/sound tick, damage/reload cadence; verify no side/secondary-HP change. |
| G | School and prison in two towns | Unlock across towns, loss of prerequisite during queued training, production/refund behavior through capture and gift. |
| G | Capture boundary matrix | Squared distances four/five, radii four/eight, ordinary building versus Defence/air guard, own/allied guard and small/large object pools; multiple candidates and an unfinished Defence outside the radius. |
| G | Gift group and town | Confirm receiving tactic identity, donor/recipient ID collisions, pending paths, active construction, selection and queue cleanup. |
| G | Camera durations 0, 1, 2 and -1 | First/last contributing vision tick, low/high mask clamp, two activations of one record, expiration followed by deactivation; repeat from another viewing side. |
| G | Johnka patrol | Up orientation, both off-map turns (including negative coordinates), five-pixel visual cadence versus simulation ticks, disable during travel, enable after a turn, target/volley timing after task 141. |
| G | Chief, Abdula and Elefant | Hit/kill and natural/service regeneration timing, paired Elefant muzzle/impact timing; no new special ability is assumed while these timings remain unmeasured. |

## Verification

The pre-change Release build, all available CTest checks and pre-commit gates
passed; six checks skipped for unavailable private inputs/process capabilities.
`hypno_ownership` adds invented descriptor/map/script tests for facility
prerequisites and production, ordinary hits and deaths without conversion,
capture thresholds/defenders, ownership/mover consistency, temporary vision,
Johnka lifecycle and deterministic replay. Existing script expectations now
separate camera revelation from viewport centering. Engine save replay semantics
advance from develop version 12 to 13 because the simulation semantics changed.
The integration retains develop's world-based camera state, tile-origin viewport
centering, presentation-timed messages, shuttle controller and expanded save-size
limit. Synthetic camera checks use that shared world state.
The water-path stress fixture uses non-scripted type-88 team units so its
player-command checks remain independent of the scripted Johnka patrol.
Final integration verification uses `build_sup` with `AZTEC_GEN_DIR=`; 71 CTest
checks passed and six skipped (private game-data checks and restricted
network-process checks). Both
`pre-commit run --all-files` and explicit checks of every changed/new file
passed. `git diff --check` passed. No reference process was run.
