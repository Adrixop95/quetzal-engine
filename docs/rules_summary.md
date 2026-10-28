# Gameplay rules for the rewrite

Generate `docs/data/rules.json` with `python3 -I tools/build_rules.py`. Numeric inputs are `docs/data/*.csv`; behavioral inputs are `docs/data/rules_mechanics.json`. **V** = code read and/or observed, **G** = inference, **O** = unresolved. A verified reference rule does not mean it is implemented in the rewrite; see [engine status](engine_rules_status.md).

| Rule | Status | Finding / evidence |
|---|---|---|
| Logic tick / tile / fixed-point products | V | 12 Hz / 48 px / 8.8 products shifted right 8 |
| Type, price, time and build-tree tables | V | 90 types; building descriptors now cover all 43 variants |
| Price and time branches | V | Outposts use separate factors; developed towns use BuildSpeed arrays. Computer discount 0.8 unless hostile to everyone. Original conversion truncates after adding 0.5; `0x45c5b0`, `0x45c690` |
| AI footman 161 gold / 56 ticks puzzle | V | Fast developed town uses price x2 and time x2/3. Original price is 160. Lifted runtime conversion can produce 161; no second training charge in BuildThink. Exact original spawn interval remains O |
| Marine / vehicle movement speeds | V | Marine d5; Headtruck/Spider d7; Zeppelin/Truck d5; historical `speed_measured.csv` includes rounding/steering effects |
| Marine HP fields | V | Primary initial/max HP d1; d4 is the secondary value used by Hypno kill-time estimates. Engine now loads primary d1 and independent secondary d4; see engine status for remaining behavior gaps |
| Marine regeneration | V | d2 each active tick below the state-dependent d1/d3 threshold |
| Weapon damage and category multiplication | V | Sequential 8.8 multipliers; Building/Defence are both nonorganic and structure; Bird/Zeppelin are airborne but organic |
| Weapon class 7 / complete burn behavior | O | Distance-dependent weapon and FireThrow still lack a complete behavioral spec |
| Weapon range and reload getters | V | See generated weapons; Marine cooldown is reload+1, projectile/animation timing is additional |
| Effective projectile hit interval | G | Historical duel intervals are observations, not universal reload values |
| Sight data and visibility test | V | Per-type light-mask IDs, interpolated brightness >32, daylight dependent. Marine d6 is a mask ID, not pixels; `0x432f80`, `0x47d070`, `0x402490` |
| Auto-engage | V | Tactic visibility/hostility scan, kill-time assignment, bounded arrays of 100 per side; scan every 15..23 ticks. Marine pursuit reference is retained only within 1152 units of the reference approximate pixel norm; `0x454820`, `0x451c30`, `0x433db0` |
| Attack modes | V numeric contract / O live behavior | Direct 0, defensive 1, aggressive 2, balanced 3 (default); group distances 4/8/8/6 tiles, distinct from sight/weapon range. See [AI rules](rules_ai.md#tactic-attack-modes) for gates and engine limits |
| One scalar sight/auto-engage radius per type | G | Historical 3..8-tile estimates are insufficient: daylight, masks, group bounds and weapon eligibility all matter |
| MapExplorer | V | Computer town intelligence; nearby-town box abs(dx),abs(dy)<8 tiles. This is not unit sight; `0x432050`, `0x449980` |
| Buildings and air cannot be damaged | G | Rejected as a general rule: footman base damage is 150 against structures, zero against air; archer is 300 against structures and 800 against air. Ordinary buildings are omitted by normal global combat assignment |
| Corrected structure/air custom-map duels | O | Old generator overlapped hall and target, inherited footmen, and used wrong tower/air HP. Fixed generator and isolated runner are ready; new runtime runs abort on sandbox-denied UDP sockets |
| Building footprint | V | 7x7 occupancy grid at descriptor +0x18..+0xd8; cells equal to 2 occupied. +0xdc is sight mask |
| Income accumulation and event cycle | V | Fixed-point accumulator; periodic and scheduled day-clock triggers; 7200 ticks/day; gold at `0x4111af` |
| Farm schedule | V | Rus: 1000 then every 1050 through 6250; Aztec: 700 then every 650 through 6550; Chinese: 0 then every 900 through 6300. Clock starts at mission StartTime, not zero |
| Mine/farm yield | V | Resource-map averages over occupied cells and d60/d61 factors; no fixed gold amount per race. Historical mine +3/+6 and farm +146 are location/phase observations |
| Warehouse | V | Any completed warehouse in the same town enables x1.33333337 for farms only, once; no mine bonus; `0x45c450` |
| Repair income versus gold | V | Building d62 feeds repair credits; d60/d61 feed gold. d64 is schedule count/period, not a weapon ID |
| Repair and destroy | V | Town repairer modes 0/1/2, marked building lists, per-tick credits and damage-priority groups; destroy command toggles a listed building, not immediate deletion |
| Garrison command | V | Extract live mobile members from the selected town into a new selected tactic; `0x448350` |
| Building boarding/capacity | O | No verified boarding mechanism or capacity field; do not invent slots from the command name |
| Population cap | O | No verified universal cap; queue capacity 50 and assignment arrays 100 are separate limits. Unsupported rewrite AI cap 80 has been removed; synthetic production above 110 is not reference evidence |
| Town upgrades / prerequisites | V | Hall changes install race/level build tree; ordinary prerequisites are town-local, advanced-race prerequisites side-wide; `0x459bb0`, `0x411ac0` |
| Generic armor/damage upgrades | O | No verified stat bonus beyond descriptor/weapon data and build-tree unlocks |
| Tendency / BuildSpeed / Difficulty mission keys | V | No Tendency/BuildSpeed reads in town loader; map Difficulty is not used by inspected price/time paths. Computer strategist actively chooses normal/fast speed |
| Other difficulty modifiers | O | Do not infer a global difficulty bonus from the old spend experiment |
| Computer AI, autonomous attacks, Defence#, Camera# | V | See [AI specification](rules_ai.md): town state machine, force intelligence and script-controlled tactics; Defence# creates towers with dedicated tactics, Camera# registers camera data |
| Exact attack composition, launch scoring and camera activation | O | State transitions established; remaining planner/scoring and presentation details are explicitly listed in the AI spec |
| Side counters | V | +0x698 cash, +0x63c earned, +0x640 spent, +0x144 completed-type counts, +0x2b0 disabled-type flags |

The local evidence was read on 2026-10-25 from `work/decomp/all.c` and missing functions decompiled into private `work/` files. Original x86 instructions were checked for floating-point conversions and ambiguous Ghidra signatures. No executable-derived source or game assets belong in the repository. New live observations were blocked by socket restrictions; historical measurements are labeled accordingly.
