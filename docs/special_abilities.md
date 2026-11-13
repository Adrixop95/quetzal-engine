# Special abilities and unit mechanics

This is a static inventory and a partial implementation, not campaign ability
parity. **V** means a fact checked in the private original instruction comments
or established rules metadata. **G** means an assumption or an unestablished
candidate. A V row does not certify the clone. Clone status describes this
checkout, including the changes below. No reference executable was launched.

The inventory covers all 90 public type IDs, including ordinary types to make
negative coverage explicit. The rules inputs are `types.csv`, `desc_*.csv`,
`weapons_ids.csv`, `weapon_class_*.csv`, `buildtree.csv`, `townhall_ids.csv` and
`rules_mechanics.json` under `docs/data`. Those existing inputs were read without
changing or adding extracted tables. No private source, asset dump or campaign
fixture is included here.

V: a read-only scan of all fifteen installed mission definitions finds Demolisher,
Hypno, Chief, Ninja and Snowman among mission 7's placed units; Mage appears in
several early missions and Catapult in mission 1. G: placement is a lower bound
on reachability, not an exhaustive mission unlock proof. Build trees, DisabledID,
scripted grants, town capture and runtime prerequisites can change reachability.
Unplaced units are retained in the inventory; none is declared unreachable merely
because this scan did not find it. The shared campaign observer and winning routes
remain incomplete in this checkout.

| Type (ID / UGI) | Ability or special behavior | V/G | Clone status |
| --- | --- | --- | --- |
| 0 / FOOTMAN | Descriptor recovery; direct melee weapon | V | Existing; attack timing remains provisional |
| 1 / RIFLEMAN | Descriptor recovery; direct ranged weapon | V | Existing |
| 2 / DEMOLISHER | Standalone area attack; friendly fire and distance falloff | V | Implemented here; release timing needs live run |
| 3 / TRUCK_AXETHROWER | Paired axe weapons and chained payloads | V | Existing components; flight/contact timing partial |
| 4 / CANNON | Paired ball weapons and spatial blasts | V | Existing; ballistic/contact timing partial |
| 5 / BEAR | Descriptor recovery; category-dependent melee damage | V | Existing |
| 6 / KNIGHT | Frontal melee sweep with reduced side hits | V | Only single-target damage exists; sector query pending |
| 7 / ZEPPELIN | Unarmed flight and descriptor recovery | V | Existing flight; steering parity pending |
| 8 / TRUCK_FLAMETHROWER | Arrow weapon with chained area payload | V | Existing; complete chained impact semantics partial |
| 9 / TRUCK_KATUSHA | Timed scattered area salvo | V | Missing salvo scheduling; deliberately remains disabled |
| 10 / RUS_FORPOST | Town upgrades, race/level build tree, troop extraction, repair modes | V | Existing production and command-card controls; capture policy provisional |
| 11 / RUS_VILLAGE | Town upgrades, race/level build tree, troop extraction, repair modes | V | Existing production and command-card controls; capture policy provisional |
| 12 / RUS_FARM | Scheduled/resource-dependent income and repair credits | V | Existing harvest clock; live phase/balance pending |
| 13 / RUS_MINE | Resource-map placement/yield and periodic income | V | Existing placement/harvest; task 103 live audit pending |
| 14 / RUS_BARRACKS | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 15 / RUS_STABLE | Town prerequisite plus category-specific unit service facility | V | Prerequisites exist; automatic facility coverage/refresh missing |
| 16 / RUS_BEARLAIR | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 17 / RUS_WATCH_TOWER | Defence weapon and town repair/destruction service | V | Existing; weapon-specific flight/contact limits apply |
| 18 / RUS_WAREHOUSE | Town income/storage factor and repair credits | V | Existing economy; live timing pending |
| 19 / RUS_TOWN | Town upgrades, race/level build tree, troop extraction, repair modes | V | Existing production and command-card controls; capture policy provisional |
| 20 / RUS_WORKSHOP | Town prerequisite plus nonorganic unit service facility | V | Prerequisites exist; automatic facility coverage/refresh missing |
| 21 / RUS_ARMORY | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 22 / RUS_LABS | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 23 / RUS_FORTRESS_TOWER | Defence weapon and town repair/destruction service | V | Existing; weapon-specific flight/contact limits apply |
| 24 / RUS_FORTRESS | Town upgrades, race/level build tree, troop extraction, repair modes | V | Existing production and command-card controls; capture policy provisional |
| 25 / RUS_AIR_FACILITY | Town prerequisite plus airborne service branch | V | Prerequisites exist; automatic facility coverage/refresh missing |
| 26 / RUS_CANNON_TOWER | Defence weapon and town repair/destruction service | V | Existing; weapon-specific flight/contact limits apply |
| 27 / ARCHER | Descriptor recovery; ranged projectile | V | Existing |
| 28 / MAGE | Axe-family projectile and chained payload; separate spell unestablished | V/G | Projectile exists; no invented spell button |
| 29 / TUZEM | Arrow-family projectile; no healer contract established | V/G | Existing projectile; healer claim remains unverified |
| 30 / SPIDER_CROSSBOW | Crossbow projectile and descriptor recovery | V | Existing |
| 31 / JAGUAR | Life drain on melee hit; capped self-healing | V | Implemented here; animation release timing needs live run |
| 32 / BIZON | Descriptor recovery; direct melee weapon | V | Existing |
| 33 / EAGLE | Unarmed flight and descriptor recovery | V | Existing flight; steering parity pending |
| 34 / DELTAPLAN | Paired ball weapons from a flying attacker | V | Components exist; flight/aim timing partial |
| 35 / SPIDER_FLAMETHROWER | Fire stream with spatial victims and friendly damage | V | Missing stream lifecycle and spatial damage dispatch |
| 36 / SPIDER_AXETHROWER | Marine descriptor shared with Footman, despite its name | V | Existing descriptor weapon; no inferred spider launcher |
| 37 / AZT_FORPOST | Town upgrades, race/level build tree, troop extraction, repair modes | V | Existing production and command-card controls; capture policy provisional |
| 38 / AZT_VILLAGE | Town upgrades, race/level build tree, troop extraction, repair modes | V | Existing production and command-card controls; capture policy provisional |
| 39 / AZT_FARM | Scheduled/resource-dependent income and repair credits | V | Existing harvest clock; live phase/balance pending |
| 40 / AZT_WAREHOUSE | Town income/storage factor and repair credits | V | Existing economy; live timing pending |
| 41 / AZT_BARRACKS | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 42 / AZT_ARCHER_TOWER | Defence weapon and town repair/destruction service | V | Existing; weapon-specific flight/contact limits apply |
| 43 / AZT_HUT | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 44 / AZT_STABLE | Town prerequisite plus category-specific unit service facility | V | Prerequisites exist; automatic facility coverage/refresh missing |
| 45 / AZT_JAGUARLAIR | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 46 / AZT_NEST | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 47 / AZT_TOWN | Town upgrades, race/level build tree, troop extraction, repair modes | V | Existing production and command-card controls; capture policy provisional |
| 48 / AZT_MINE | Resource-map placement/yield and periodic income | V | Existing placement/harvest; task 103 live audit pending |
| 49 / AZT_CROSSBOW_TOWER | Defence weapon and town repair/destruction service | V | Existing; weapon-specific flight/contact limits apply |
| 50 / AZT_TEMPLE | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 51 / AZT_WORKSHOP | Town prerequisite plus nonorganic unit service facility | V | Prerequisites exist; automatic facility coverage/refresh missing |
| 52 / AZT_AIR_FACILITY | Town prerequisite plus airborne service branch | V | Prerequisites exist; automatic facility coverage/refresh missing |
| 53 / NINJA | Frontal melee sweep, same weapon family as Knight | V | Only single-target damage exists; no verified invisibility |
| 54 / HYPNO | Beam weapon; secondary-HP kill-time estimate | V | Beam exists; secondary transitions/conversion unverified |
| 55 / CHIEF | Direct weapon and descriptor recovery; separate leadership cast unestablished | V/G | Direct attack exists; aura/summon claim unverified |
| 56 / SNOWMAN | Melee hit plus nearby ground attack delay | V | Implemented here; bounded spatial ordering and release timing remain provisional |
| 57 / ELEFANT | Paired axe-family weapons | V | Existing components; contact semantics partial |
| 58 / WHEEL_KATUSHA | Timed scattered area salvo | V | Missing salvo scheduling; deliberately remains disabled |
| 59 / WHEEL_FIRETHROWER | Fire stream with spatial victims and friendly damage | V | Missing stream lifecycle and spatial damage dispatch |
| 60 / PARAPLAN | Flying direct weapon and descriptor recovery | V | Existing; flight steering partial |
| 61 / WHEEL_AXETHROWER | Paired axe weapons | V | Existing components; flight/contact timing partial |
| 62 / WHEEL_CROSSBOW | Crossbow projectile and descriptor recovery | V | Existing |
| 63 / CHI_FORPOST | Town upgrades, race/level build tree, troop extraction, repair modes | V | Existing production and command-card controls; capture policy provisional |
| 64 / CHI_VILLAGE | Town upgrades, race/level build tree, troop extraction, repair modes | V | Existing production and command-card controls; capture policy provisional |
| 65 / CHI_FARM | Scheduled/resource-dependent income and repair credits | V | Existing harvest clock; live phase/balance pending |
| 66 / CHI_MINE | Resource-map placement/yield and periodic income | V | Existing placement/harvest; task 103 live audit pending |
| 67 / CHI_BARRACKS | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 68 / CHI_FIRETHROW_TOWER | Building descriptor aliases Chinese town descriptor; name is not proof of a flame weapon | V/G | Loaded as Building; no inferred turret ability |
| 69 / CHI_HYPNO_TOWER | Defence beam weapon | V | Beam damage exists; hypnosis/conversion semantics unverified |
| 70 / CHI_WAREHOUSE | Town income/storage factor and repair credits | V | Existing economy; live timing pending |
| 71 / CHI_TOWN | Town upgrades, race/level build tree, troop extraction, repair modes | V | Existing production and command-card controls; capture policy provisional |
| 72 / CHI_HYPNO_SCHOOL | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 73 / CHI_PRISON | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 74 / CHI_WORKSHOP | Town prerequisite plus nonorganic unit service facility | V | Prerequisites exist; automatic facility coverage/refresh missing |
| 75 / CHI_ARMORY | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 76 / CHI_FORTRESS | Town upgrades, race/level build tree, troop extraction, repair modes | V | Existing production and command-card controls; capture policy provisional |
| 77 / CHI_STABLE | Town prerequisite plus category-specific unit service facility | V | Prerequisites exist; automatic facility coverage/refresh missing |
| 78 / CHI_CAVE | Town development/prerequisite and repair/destruction service; separate research unestablished | V/G | Existing build-tree gates and town service; no invented research bonus |
| 79 / CHI_AIR_FACILITY | Town prerequisite plus airborne service branch | V | Prerequisites exist; automatic facility coverage/refresh missing |
| 80 / CHI_WATCHTOWER | Unarmed defence descriptor | V | Existing; no invented attack |
| 81 / CHI_AIRTOWER | Anti-air scattered salvo | V | Missing salvo scheduling; deliberately remains disabled |
| 82 / ABDULA | Named Marine; direct weapon; special death decoration | V | Weapon exists; special death decoration missing; hero cast unestablished |
| 83 / CATAPULT | Catapult ball weapon and area payload | V | Existing siege movement, range and blast; flight timing partial |
| 84 / BOAT | Scripted ferry, boarding, ordered cargo and unloading | V/G | Existing transport commands; timing and capacity parity partial |
| 85 / ZEPPELIN2 | Armed Zeppelin with paired ball weapons | V | Existing components; flight/aim timing partial |
| 86 / SHIPYARD | Unarmed Defence descriptor used as shipyard | V | Existing object; no player ship-production contract established |
| 87 / HOPCANNON | Cannon ball weapon and area payload | V | Existing siege movement, range and blast; flight timing partial |
| 88 / JOHNKA | Scripted Johnka route/activation and transport | V/G | Script activation/cargo exists; autonomous patrol pending |
| 89 / CAMERA | Scripted camera controller | V/G | Existing controller adapter; exact visibility remains open |

## Implemented mechanics and evidence

V: standalone area weapon class 7 exposes descriptor d3 as range and d6 as
reload (`0x44bf00`, `0x44beb0`). Its damage estimate evaluates the target at the
attack center (`0x44c100`, `0x44c130`). The area dispatch (`0x44c3c0`) places the
hit at the target position with a two-pixel vertical offset, evaluates each
nearby victim separately, and has no friendly-only exclusion. Air/ground
eligibility, target extent, inner/outer radii and category falloff come from the
existing spatial blast rule. This corrects the former placeholder travelling
projectile metadata and missing getters for Demolisher. The scatter wrapper
(class 8) remains unavailable: its own getters and timed pattern cannot be
replaced by the child area's getters.

V: life-drain class 10 (`0x46ed00`) applies its category-adjusted damage, then
heals its source by integer damage divided by three. This uses calculated damage,
including overkill, rather than the victim's remaining HP. The common healing
operation (`0x4029d0`) caps at the source's maximum. The clone now applies that
passive effect to Jaguar without modifying secondary HP. The existing class-10
damage calculation supplies the amount; no new balance constants are embedded.

V: Snowman's class-6 secondary reaction (`0x46eb10`) calls the recipient's
attack-countdown increment (`0x471220`) with 18. It selects living nonfriendly
ground recipients with either category flag in a 96-pixel square around the
attacker, bounded to 128 spatial candidates. The clone now applies that attack
delay through the existing hashed cooldown. It is not a movement stun. G: engine
object order substitutes for the native spatial collection order, and recipient
countdown decrement still follows the clone's object-update order. Exact query
edges and crowded-scene ordering need a live comparison.

V: Marine attacks install reload plus one in `0x433db0`; the damage call occurs
on an animation release marker. G: the clone still releases attacks when its
simulation cooldown permits firing, before exact animation-marker integration.
The added effects are therefore mechanically implemented with a known timing
limit, not verified frame/tick parity. Range geometry, collision extents and
Euclidean distance also retain the existing engine approximations.

V: none of these inspected attack paths contains a separate cash debit or
ability cooldown. All three effects use the existing Attack command, button and
attack target cursor; their production cost remains descriptor/build-tree driven. G: the clone
uses its existing attack-move behavior for terrain clicks. No separate cast,
mana cost, artificial cooldown or spell hotkey is introduced. Command-card
hover text describes area friendly fire/reload, life drain or nearby attack delay.
These attacks can also be issued through existing right-click and replay commands.

The new synthetic checks use invented descriptors and worlds. They cover area
range rejection, per-victim falloff, friendly fire, airborne/dead exclusions,
source attribution, unchanged money, cooldown repetition, pause/stop and UI
attack targeting. Life-drain checks cover damaged/full attackers, category
adjustments, lethal overkill and unchanged secondary HP. Attack-delay checks cover
the square boundary, friendly/neutral relations,
air/structure/dead exclusions, additive countdown changes and pause. All three
compare state hashes with fresh command-log replay; area tests additionally
perturb cooldown and victim HP to check hash sensitivity. Existing projectile tests retain
travelling weapons and no longer classify a standalone area hit as a projectile.
The complete state hash already includes HP, cooldown, ownership, orders and
effects; these mechanics add no mutable state or save-format fields. Hash
agreement establishes engine determinism under identical inputs, not original
parity or compatibility with old replay outcomes after a behavior change.

## Other findings and missing dependencies

V: Knight and Ninja share the sweep implementation (`0x46e8b0`), with frontal
victims and reduced side hits. The clone's class-4/5 damage constants alone do
not implement those sectors. Snowman's additional callback is now resolved as
an attack delay, as described above; movement stun and knockback remain
unestablished candidates (G).

V: scatter salvos prepare sixteen entries with release offsets across ten ticks
(`0x44ca60`, `0x44cb10`). The clone currently lacks that pattern and deliberately
does not enable the launcher through the newly established child getters.
V: fire-stream damage has a spatial victim scan, excludes its source and permits
reduced friendly damage (`0x429e20`, `0x429f60`); its original animation and
lifecycle gates remain outside the current simulation implementation. These are
campaign weapon gaps, not evidence of selectable spell commands.

V: automatic town facility service (`0x45d4d0`) checks workshop, stable and air
facility groups and dispatches service through a virtual method. Its small-pool
and large-pool branches differ in geometry and category handling; the large-pool
query is bounded. V: the ordinary service setter refreshes recovery to ten ticks
(`0x402990`); some classes instead perform immediate capped healing
(`0x41cbe0`). The existing recovery countdown works, but facility producers and
those dispatch distinctions remain missing. G: a universal healing radius or a
priest unit inferred from a unit name would be unsupported.

V: Hypno uses a beam descriptor and its kill-time estimate reads secondary HP
(`0x46fac0`). G: this does not establish a complete hypnosis damage, conversion,
resistance-reset or ownership-transfer contract. Those semantics remain open;
the existing generic beam damage must not be reported as verified conversion.
V: Abdula has a type-specific death decoration branch in `0x433960`. G: this
inspection did not establish a selectable hero power, summon or resurrection.
Likewise, no audited active invisibility, garrison-inside-building or independent
research-stat-bonus contract was established. These are open candidates, not
proof that the original lacks them. Town troop extraction is already implemented
and must not be confused with boarding or hiding inside a building.

The shared original observer still lacks complete normalized orders/queues and
technology/population semantics. Tasks 65/87 (measured weapons), 88 (service),
103 (mine audit), 105 (transport) and 106 (campaign sweep) remain queued; their
full acceptance is not present here, although parts of their mechanics already
exist in the checkout. This change implements the independent area, drain and attack-delay
parts without depending on those measurements. Full campaign reachability,
remaining effects and live comparisons stay unfinished. The reference executable
cannot be run in this environment, as required by this task.

## Live run sheet: needs live run

Use a namespace-capable operator machine and the private-copy procedure in
[EXPERIMENTS.md](EXPERIMENTS.md). Every process needs a separate private game
copy, immutable asset symlinks, ordinary private Maps/Saves, copied mutable
configuration, dummy SDL video/audio, a hard timeout and `unshare -rn`. Never
run against the shared installation. Do not bypass namespace setup failures.
Keep captures, inputs, raw state, executable hashes and original-derived fixtures
in ignored private output; publish only reviewed behavioral conclusions here.

For each case retain two independent reference runs and two clone runs with the
same mission/map, seed, difficulty, profile, modifications and input schedule.
Align the before-command observation phase and record the first repeated
mismatch. Save/replay the clone around the effect, including pause, death and
ownership changes. An unavailable field stays unavailable, not zero.

| Priority | Case and controls | Required observation / unresolved decision |
| --- | --- | --- |
| 1 | Demolisher versus stationary ground target; ally at inner/falloff/outer boundary; air control; repeat mission 7 | Exact animation release tick, reload interval, target extent/approximate distance, two-pixel origin and friendly/self damage; ensure the target is not damaged twice |
| 1 | Jaguar versus organic, nonorganic and structure targets; attacker damaged/full; victim almost dead | Damage and capped healing in the same native phase, integer rounding, overkill, secondary HP, release marker and repeated-hit interval |
| 1 | Workshop/stable/air facilities built, incomplete, destroyed and captured; friendly/enemy recipients | Facility count eligibility, exact friendly relation, scan phase/order, ten-tick refresh versus immediate healing; cover both sides of the native pool threshold and query capacity |
| 1 | Hypno and hypnosis tower against each victim category, then kill/capture the source | Primary/secondary HP transitions, conversion threshold, reset/retained orders, town/tactic membership, selection, color, visibility, and duration; distinguish ordinary beam damage from conversion |
| 2 | Knight and Ninja with front/side/rear victims, friendly and air controls | Sector origin/angles/boundaries, victim-list handling, side-hit rounding, movement during release and attack animation markers |
| 2 | Snowman hit with nearby organic, nonorganic and building controls | Confirm the 18-tick attack-countdown increment, category and square boundaries, bounded-query order and countdown-decrement phase; check that movement is unaffected |
| 2 | Both Katushas and anti-air tower; fixed seed, one shot then Stop/source death | Sixteen-entry scatter pattern, random consumption, ten release ticks, height/aim snapshots, independent wrapper getters, friendly damage and cancellation semantics |
| 2 | Spider/Wheel firethrowers; front/side targets and allied bystanders | Stream release/expiry, spatial scan shape, per-tick damage, friendly coefficient, source exclusion, height and movement during the stream |
| 3 | Mage, Chief and Abdula in reachable mission/script contexts; inspect both command-card pages and shortcuts | Identify any actual active command/aura/summon; record packet, cursor, valid targets, debit, cooldown, duration and cancellation. Include Abdula death decoration; a name alone is insufficient |
| 3 | Mines, farms, warehouses and hall upgrades across all fifteen missions | DisabledID/build-tree/script grants, placement/resource channels, activation/harvest phase, repair credits and upgrade retention; retain task-103 placement controls |
| 3 | Boats/Johnka and extracted town troops; candidate hiding/garrison UI | Audit existing transport capacity/routes and extraction separately from any discovered concealment/occupancy command; never infer population from queue capacity |
| 3 | Every mission's placed objects, production menus and scripted births/grants | Complete reachable-type/ability matrix through winning routes, including research/unlocks and timed effects if observed; missing route/observer dependencies remain explicit |

Validation: baseline and implementation Release builds passed. The full CTest
suite passed 63 entries, with six private-data/network entries skipped and no
failures. After the final diagnostic effect-class correction, the Release rebuild
and all five focused engine/combat/determinism checks passed. Pre-commit passed
for all tracked files and separately for this new document; `git diff --check`
passed. Inventory coverage was checked for all IDs 0 through 89 exactly once.
No original runtime, live reference measurements or private-data campaign
playthrough was executed. Changes remain uncommitted because git metadata is
read-only. Full campaign ability parity remains incomplete as detailed above.
