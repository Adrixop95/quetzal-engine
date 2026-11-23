# Special units, abilities and command completion

Audit: 2026-11-23, current checkout. This matrix supersedes stale clone-status
entries in [special_abilities.md](special_abilities.md). **V** means an inspected
reference rule or established descriptor fact; **G** means an unresolved detail
or engine policy. **done** means that the stated rule has an implementation and
synthetic coverage, **partial** means that the identified limits remain, and
**missing** means intentionally absent or awaiting the named dependency. None of
these states establishes live reference parity. No original executable was run.

Evidence, including corrections to older interpretations:
[specials_commands.md](specials_commands.md),
[special_mechanics_residual.md](special_mechanics_residual.md),
[special_abilities.md](special_abilities.md),
[towers_air_specials.md](towers_air_specials.md),
[hypno_and_ownership.md](hypno_and_ownership.md), and
[projectiles_effects.md](projectiles_effects.md).

## Special units and automatic abilities

| Unit / types | Ability or rule | Evidence | Clone state and limits |
| --- | --- | --- | --- |
| Mage 28 | Weapon 24: straight projectile, contact and explosion payload 25 | V | partial: snapshot aim, contact, payload and friendly splash implemented; native first movement/deletion phase remains G |
| Mage 28 | Separate spell, mana, cost or cast cooldown | V absence in command/effect audit | done: no fabricated spell command; ordinary Attack uses the weapon |
| Chief 55 | Weapon 55: direct category damage, recovery | V | done: descriptor-driven damage and regeneration; `ground_weapons`, `hypno_ownership` |
| Chief 55 | Leadership aura, healing, summon or capture bonus | G candidate; no such command in V audit | missing: intentionally no invented ability |
| Abdula 82 | Weapon 59: direct damage and recovery | V | done: ordinary Marine combat, no life drain or conversion |
| Abdula 82 | Death-only sprite 151, random variant, y+1; sound set 150 | V | done: one cosmetic emission, sound offset -400, no explosion damage or revival; ordinary Marine death variant also consumes RNG |
| Abdula 82 | Hero cast, resurrection or summon | G candidate; no such command in V audit | missing: intentionally absent |
| Hypno 54 | Weapon 56: class-13 primary-HP damage | V | done: no secondary-HP subtraction, slow, stun or ownership change |
| Hypno 54 | Secondary HP in kill-time estimate | V | done: descriptor predicate and estimator retained; exact native target scoring remains G |
| Hypno 54 / tower 69 | Trail sprite 107, 30-pixel steps, distance-dependent angle jitter | V; origin G | partial: class-13-only trail implemented; owner-centre origin remains an explicit fallback, native origin and integer-vector equivalence unresolved |
| Knight 6 / Ninja 53 | Full central sweep and half side hits | V | done: three 96-pixel sector scans, angular widths 20/30 and side offsets 25; crowded query ordering/edge equivalence remains G |
| Ninja 53 / all selectable units | Invisibility or cloak command | V absence in scoped visibility/command audit | done: visibility follows light/fog; no name-based cloak mechanic |
| Bear 5 | Category-adjusted melee and animal recovery threshold | V | done: direct weapon and stable service; no inferred extra attack |
| Snowman 56 | Direct melee plus 18 ticks added to nearby hostile ground Marine attack countdowns | V | done: bounded square query and category filters; no movement stun, knockback or facing change; exact spatial edge/order equivalence remains G |
| Jaguar 31 | Damage/3 self-heal, including overkill, capped at maximum primary HP | V | done: class-10 hit; secondary HP unaffected |
| Demolisher 2 | Standalone area attack, friendly damage and per-victim falloff | V | done: class-7 area dispatch; exact animation/pool phase remains G |
| Johnka 88 | Script enable/disable, turn-gated shutdown, diagonal patrol | V; visual-think cadence G | partial: lifecycle and five-pixel-per-axis motion implemented; exact native phase/order and campaign route observations pending |
| Johnka 88 | Weapon 54: sixteen scattered payload-53 explosions over ten updates | V | done: current checkout contains class-8 scheduler/getters; older task-141 dependency notes are superseded; reload 30 is not a rocket count |
| Johnka 88 | Passenger transport | G unsupported candidate | missing: scripted Johnka rejects boarding; generic non-scripted type-88 transport compatibility is an engine policy, not a verified ability |
| Elefant 57 | Paired carrier 46 shots with payload 45 | V | partial: paired launch and point payload implemented; exact paired muzzle geometry and native phase remain G |
| Truck axethrower 3 / Wheel axethrower 61 | Paired projectile weapons | V | partial: both children launch; exact native geometry/phase pending |
| Spider crossbow 30 / Wheel crossbow 62 | Straight projectile and payload | V | partial: shared projectile implementation; exact native phase pending |
| Truck flamethrower 8 | Descriptor arrow and chained payload | V | partial: descriptor dispatch implemented; native projectile phase remains G; the name does not select class 11 |
| Spider flamethrower 35 / Wheel firethrower 59 | Class-11 stream: source-following muzzle, per-update segment damage, ally quarter factor | V | partial: lifecycle and damage implemented, including after source death while retained; animation/visible-flag deletion handshake remains G |
| Truck Katusha 9 / Wheel Katusha 58 | Class-8 sixteen-position, ten-update scattered area salvo | V | done: release/radius/angle draw order, getters and payload retained; native pool ordering remains G |
| Tuzem 29 | Descriptor projectile | V | partial: projectile implemented; no established healer ability |
| Tuzem 29 | End-of-mission dying-frame latch | V readers; arming writer unresolved | missing: no guessed countdown producer or corpse freeze |
| Bizon 32 / Archer 27 / Paraplan 60 | Descriptor category/air eligibility | V | done: weapon formulas gate eligibility; projectile timing limits apply to Archer |
| Spider axethrower 36 | Shared Marine descriptor | V | done: follows its descriptor; no launcher inferred from its name |
| Catapult 83 / Cannon 4 / Hopcannon 87 | Siege projectile, falling carrier and area payload | V | partial: shared projectile dispatch implemented; ground muzzle-height producer and native phase remain G |
| Boat 84 | Script ferry, ordered cargo, boarding and automatic unloading | V/G | partial: ferry state machine and commands implemented; native timing and isolated live observations remain pending |
| Camera 89 | Scripted temporary vision, current marker deactivation, lifetime | V/G | partial: world camera markers implemented and hashed; tick alignment remains G; vision and viewport centering are separate |

## Air units and bombers

| Unit / types | Ability or rule | Evidence | Clone state and limits |
| --- | --- | --- | --- |
| Zeppelin 7 | Unarmed flight, initial altitude zero, climb to descriptor cruise altitude | V | done: no weapon fallback; no verified passenger ability |
| Eagle 33 | Unarmed Bird flight, initial altitude 30 and climb | V | done: no generic attack fallback |
| Paraplan 60 | Weapon 52 direct attack, including airborne targets | V | done: nose alignment gates release, shot releases target and re-picks |
| Deltaplan 34 | Paired arrows 36 with chained explosions | V | partial: paired shots and payload implemented; muzzle geometry and exact flight phase remain G |
| Zeppelin2 85 | Weapon 44: paired falling bombs with explosion 42 | V | partial: short-range bombing, altitude and retained target implemented; exact paired geometry/flight phase remains G |
| Bird 33/34/60 | Turning, speed change, chase, fly-out branch, circling | V/G | partial: descriptor steering and nose gate implemented; exact flight paths, cycle alignment, native arrival/tactic phase and state-3 transitions remain unresolved |
| Zeppelin 7/85 | Independent move destination, brake/hover, no nose gate for bombs | V/G | partial: implemented movement and range-only bombing; native steering/arrival phase remains unresolved |
| All aircraft | No landing state, recovery window and airborne predicate | V | done: airborne damage eligibility is independent of current altitude; facility service uses the inspected branch rules |

## Towers and facilities

Every tower type is listed, including names whose descriptors do not supply an
attack. Defence and ordinary Building kinds remain distinct.

| Type | Ability or rule | Evidence | Clone state and limits |
| --- | --- | --- | --- |
| Rus watch tower 17 | Weapon 15 direct ranged hit | V | done: descriptor muzzle/impact/sound, no class-13 trail |
| Rus fortress tower 23 | Weapon 7 paired arrows | V | partial: shared paired weapons; native muzzle geometry/phase pending |
| Rus cannon tower 26 | Weapon 14 paired carriers | V | partial: shared falling carriers and explosions; native geometry/phase pending |
| Aztec archer tower 42 | Weapon 28 arrow | V | partial: shared projectile; native phase pending |
| Aztec crossbow tower 49 | Weapon 51 straight projectile/payload | V | partial: shared contact and payload implementation; native phase pending |
| Chinese firethrow tower 68 | Ordinary Building descriptor alias | V | done: no inferred flame turret |
| Chinese hypno tower 69 | Weapon 57, reload 10, direct category damage | V | done: primary HP only, sound set 201, no conversion; trail origin limit above applies |
| Chinese watchtower 80 | Unarmed Defence descriptor | V | done: no invented attack |
| Chinese air tower 81 | Weapon 20 class-8 salvo with airborne-only explosion 18 | V | done: sixteen bursts over ten updates; workshop/air-facility prerequisites |
| Shipyard 86 | Unarmed Defence, counts as capture guard | V | done: no independent ship production command |
| Armed Defence types | Whole-step turn gate, target eligibility, ready/early acquisition, latched animation-marker release, launch height | V | done: `towers_specials`; nearest eligible assignment remains a G substitute for native scoring |
| Defence types | Construction gate, additive reload delay, immediate capped service healing | V | done: no garrison/ammunition requirement |
| Defence types | Activation random heading, reload visual at countdown 12, damage-stage sound and three-tick low-HP sparks | V | done: activation consumes a heading draw, including construction completion; finite slot-1 reload cue, stage loops 151/152/153 and sprite-105 sparks implemented; native phase equivalence remains G |
| Schools 72 / prison 73 | Side-wide prerequisite for Hypno/tower or Chief, with town-local barracks | V | done: ordinary production gates; no prisoners, independent queue or conversion |
| Workshops 20/51/74 | Machine service, including buildings, towers, boats and Johnka | V | done: ten-tick regeneration refresh or class-specific immediate heal |
| Stables 15/44/77 | Animal service | V | done: six Marine animal descriptors; soldiers are not serviced |
| Air facilities 25/52/79 | Air service in large-pool branch, only with workshop or stable | V | done: air facility alone does not open the service path; branch geometry/candidate limits retained |
| Other prerequisite buildings | Town build-tree unlocks and ordinary repair/destruction | V | done: no inferred research-stat bonuses, priest, summon or free aircraft |

## Player command buttons and channels

The command audit finds no spell packet. The original right-click packet is
contextual; the clone also has explicit Move, Stop and AttackMove commands.
Command payload conventions and save/log serialization are engine contracts,
not native packet compatibility. Both modern and original panel layouts remain
presentations of these commands.

| Button / command | Evidence | Clone state and limits |
| --- | --- | --- |
| Move / contextual ground click | V generic order / G adapter | partial: tile/formation movement exists; native path/formation parity remains open |
| Attack / contextual enemy click | V | done: descriptor weapon dispatch; special passives use this existing channel |
| Attack on terrain | G engine extension | done: AttackMove with existing stance policy |
| Stop | V generic order / G packet representation | done: clears orders; retained animation shots keep their victim |
| Aggressive attack mode 2 | V newer tactic audit | done: UI/key binding and hashed group mode; no special-unit cast |
| Defensive attack mode 1 | V newer tactic audit | done: UI/key binding and tactic behavior |
| Balanced attack mode 3 | V newer tactic audit | done: UI/key binding and tactic behavior |
| Direct attack mode 0 | V newer tactic audit | done: UI/key binding and tactic behavior |
| Follow own team / guard click | V/G | partial: FollowTeam and tactic following implemented; modifier/attachment branches remain unresolved and rejected |
| Town Garrison / troop extraction | V | done: releases live town troops into a fresh tactic; does not board a building |
| Follow allied ferry / boarding | V/G | partial: ferry group/cargo lifecycle implemented; native timing still pending |
| Manual unload | G unsupported command | missing: intentionally retired; ferry unloading is automatic |
| Automatic repair mode 0 | V | done: repairer list maintenance and earned repair credits |
| Marked repair mode 1 / Repair | V | done: mode selection, marking, service and pruning |
| Marked destroy mode 2 / Destroy | V | done: toggles eligible members in the town service list; no immediate deletion |
| Build / production item / hall upgrade | V/G | partial: town recipes, placement, prerequisites and progress implemented; native queue/placement/UI phase remains open |
| Slow / normal / fast production (BuildMode) | V | done: recorded town-local speed command and price/time branches |
| Pause/resume current production | V | done: per-item PauseBuild, separate from global pause |
| Cancel current production | V | done: PauseBuild action 2 and refund policy |
| Queue cancellation | V | done: QueueKill removes waiting/active item with tested payment/refund behavior |
| Create group / select group | V/G | partial: deterministic selection and groups implemented; native identity/payload compatibility remains open |
| Add to group | G engine representation | done: explicit engine extension, not evidence for a separate native packet handler |
| Select by tactic number / member cell | V/G | partial: command selection implemented; native camera/selection presentation remains open |
| Save / load / global pause | V/G | partial: engine replay saves and pause implemented; original-save simulation continuation remains a separate incomplete dependency |
| Page previous/next / queue scroll | G presentation | done: local panel navigation; no simulation ability |
| Hidden/missing packet cases 0x14/0x17/0x19/0x1b | V no dispatch case in scoped reference switch | missing: no additional native behavior inferred from numeric gaps; similarly numbered engine commands do not prove handlers |
| Separate cast, mana, cloak, research bonus or building-occupant buttons | G candidates; V no cast packet in audit | missing: intentionally absent until evidence establishes them |

## Implementation, tests and remaining dependencies

`sim/rules.cpp` retains direct-family muzzle, hit and sound IDs independently.
`sim/game_sim.cpp` emits class-2 decorations without a beam, class-13 trails
without fabricated muzzle/impact art, and Abdula's death decoration without a
weapon payload. Direct-family sound is selected after damage; Abdula's extra
sound precedes its random death-decoration variant. Sound choices occur before
view, distance and device filtering. One-shot events last one simulation tick and are
cleared on pause; stage loops retain their selected member through pause. Audio
presentation cannot consume simulation RNG.

The scoped private reads of Marine death `0x433960` and Defence think
`0x41d790` confirm that stage sounds persist until the damage stage changes,
the reload cue is skipped during a running shot, and low-HP sparks start
immediately then repeat every three updates at height 80, y+1 and x jitter
from -14 through 14. Tower clocks, damage stage and selected sound are hashed;
replay reconstructs them without raw-state serialization. Construction
activation consumes its heading draw only once on completion.

The Marine death read confirms the ordinary death
variant draw, Abdula's -400 volume offset, decoration height one and y+1. Only
behavioral facts are recorded here. No private implementation or fixture is
included. Missing sound archives produce unresolved silent cues without invented
set members. Fixed-rate playback retains the current audio adapter; exact native
pitch/base-volume and spatial attenuation remain G. The trail retains the
existing owner-centre origin policy; native origin is not claimed resolved.

`ground_weapons` uses invented worlds to cover Mage contact/payload, Knight/Ninja
sectors, Bear/Chief/Abdula direct hits, Hypno-only trails, Snowman delay, Jaguar
overkill healing, one-time cosmetic deaths, effect expiry, unchanged ownership
and secondary HP, pause and replay. `audio_events` uses invented sound/animation
banks to check direct/death sets, position, visibility, mute, pause, event routing
and replay without presentation changing the hash/RNG. `towers_specials`,
`hypno_ownership`, `engine_unit`, `unit_tactics`, `script_unit` and `transport`
retain tower markers, aircraft, volleys, streams, facilities, script activation,
town commands and ferry coverage. Replay semantics advance from 14 to 15.

Tasks 140/141 no longer block the shared projectile and volley mechanics in this
checkout. The remaining dependencies are native animation/construction phase
alignment, original pool/visibility/deletion phase, paired muzzle and beam-origin geometry, full native tactic scoring and an
audited reference observer with live observations. These do not justify new
hero casts, ownership effects or guessed latch writers. Exact reference timing,
campaign reachability and native Windows/macOS execution are not verified by
Linux synthetic checks.

## Verification

The baseline and final Release builds passed with `AZTEC_GEN_DIR=`. The complete
CTest run passed all 77 runnable checks; six of 83 entries skipped for private
inputs or restricted network-process capabilities. Focused checks were repeated
after the final synthetic fixture additions. Pre-commit passed on tracked files
and explicitly on this new matrix; `git diff --check` passed.

A clone-only mission-1 smoke run reached its 120-tick limit with no script errors
or unsupported actions. It used dummy SDL video/audio inside `unshare -rn`, a
separate private symlink installation, copied mutable configuration and a private
user directory. Its bounded result is not mission completion or reference parity.
No original executable was launched; private outputs remain ignored and all
public changes remain uncommitted.
