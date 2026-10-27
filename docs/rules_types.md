# Gameplay rule data and evidence

**V** = verified by code or observation; **G** = inference; **O** = open. [Summary](rules_summary.md), [AI specification](rules_ai.md), and `data/rules_mechanics.json` carry the same distinction. `tools/build_rules.py` combines the behavioral file and numeric CSVs into `data/rules.json`. The runtime engine currently loads CSVs; generating JSON does not implement these behaviors in the engine.

Research uses a local decompilation and, where signatures or floating-point expressions are unreliable, original x86 instructions. Only factual rules and numeric tables are distributed. All decompilation, lifted source, game resources and experiment outputs remain local.

## Types, descriptors and build trees

**V:** 90 UGI types are described by `types.csv`; IDs and category names are listed in [object types](object_types.md). The descriptor pointer is Marine/Truck/Cannon +0x68, Building/Bird/Zeppelin +0x64, Defence/Headtruck/Spider +0x6c. Descriptor arrays are populated at startup, so the CSVs come from a local heap dump. They are not all static executable constants.

| Kind | Table | Record bytes | Records |
|---|---|---|---|
| Marine | 0x4ac430 | 0x4c | 16 |
| Building | 0x49b538 | 0x508 | 43 |
| Defence | 0x4aa3b0 | 0x110 | 9 |
| Headtruck | 0x4ab2b0 | 0x50 | 8 |
| Truck | 0x4bc738 | 0x44 | 1 |
| Spider | 0x4ae378 | 0x4c | 2 |
| Zeppelin | 0x4bcd00 | 0x44 | 2 |
| Bird | 0x49b370 | 0x58 | 3 |
| Cannon | 0x4a8e18 | 0x40 | 1 |

The earlier Building export stopped at 32 and omitted Chinese variants through parameter 42. `extract_rules.py` now exports 43. The additional rows use the same startup dump as the existing rows; cross-mission descriptor stability remains **O**.

**V:** BuildTree (`0x411890`, `0x411ac0`) uses 3 races x 4 hall levels x 33 entries of 34 dwords (`0x4af130`). Entry bit 0x4000000 distinguishes construction on the map; prerequisite bit 0x8000000 selects side-wide completed-type counts instead of town-local counts. DisabledID and queued/completed counts also affect availability. A repeated building cannot be built merely because its prerequisites still exist. `DevLevel` (`0x45c780`) is calculated from these trees; unavailable types have level 10. Town development is a separate sum, capped at 9 (`0x45d070`).

## Prices, time and the footman puzzle

**V:** `UnitPrices[90]` is at 0x4bc3e8; `BuildTime[90]` at 0x4bc550. Town price/time getters are `0x45c5b0` / `0x45c690`. Both compute `trunc(table[type] * K * f + 0.5)` using the original x87 conversion.

| Branch | Price K | Time K |
|---|---|---|
| Outpost, Town+0x15c ==0 | KAdvPostPrice =1 | KAdvPostTime =1.5 |
| Developed town, side hostile to everyone | Kprice[0] =float32 2/3 | Ktime[0] =2 |
| Other developed town | Kprice[BuildSpeed] | Ktime[BuildSpeed] |

`f=0.8f` for computer sides not hostile to everyone, otherwise 1. Speed indices 0/1/2 have price factors 2/3,1,2 and time factors 2,1,2/3. The map loader does not read BuildSpeed, but the computer strategist actively selects normal or fast (`0x45e7c0`). Human towns default to normal and can receive the speed command. Hall level, not a generic AI flag, chooses the K branch.

This resolves the doubled footman cost: a developed computer town in fast mode computes 160 gold and 53 time units for the base 100/100 entry. BuildThink (`0x459bb0`, payment at `0x45a426`) has one TakeMoney at production start. There is no evidence for an additional payment at spawn.

The **historical oracle observation** was 161 every 56 ticks. The lifted FISTP path rounds with host `nearbyintl` before truncation, whereas original `__ftol` (`0x4810e4`) selects rounding toward zero. Float32 0.8 makes many half-integer expressions slightly larger than one half; the oracle can therefore return one more than the original. Do not promote the oracle's `0.8*P+1` to an original rule. Exact original spawn tick parity remains **O**.

**V:** Marine primary initial and virtual maximum HP are descriptor d1 (`0x433190`, `0x45fe50`). Descriptor d4 initializes the separate object +0x3c value, exposed through virtual +0x78 for Hypno kill-time estimates (`0x4028c0`, `0x46fac0`). It was previously mislabeled primary max HP. Bear is 40000 primary /36000 secondary; Bizon 13000/23000. The JSON labels, duel generator and engine CSV reader now distinguish them. The engine hashes secondary HP separately; Hypno assignment remains unimplemented.

## Sight and target acquisition

**V:** Object virtual +0x74 returns a light-mask ID. The field is Marine/Truck/Cannon d6, Building d55, Defence d58, Bird/Headtruck/Spider d8, Zeppelin d7 (`0x432f80`, `0x415dd0`, `0x40fa30`, `0x41cb80`, `0x407240`, `0x42bc50`, `0x471180`). Footman uses 64, archer 66, rifleman 68. These are resource identifiers in `Res/light.xbe` group 0x455, not pixel radii.

Each logic frame (`0x4577b0`) stamps masks onto a 48-pixel grid using minimum darkness and position-dependent interpolation (`0x47d070`). Global illumination and local darkness combine by nonnegative unsigned subtraction, retaining the larger resulting brightness (`0x480d20`). Previously explored cells have a brightness floor of 32. A mobile object's visibility interpolates its four surrounding cells and requires a result strictly above 32 (`0x402490`). Structures instead test the four bounding-box corner cells (`0x410340`). Thus daylight, fractional position and other units affect effective sight. A single universal circle would lose verified behavior.

**V:** Marine Think (`0x433db0`) delegates movement/assignment to its tactic. Tactic Think (`0x454820`) scans around the group's bounds at intervals `15+floor(rand()*8/32767)` ticks; its coarse rectangle extends six tiles before the lower bound and eight beyond the upper bound. This is a broad candidate search, not a guaranteed attack radius. Global/local assignment (`0x451c30`, `0x452c70`) then uses visibility, relations, weapon eligibility, estimated kill times and allied damage. Each assignment array admits at most 100 objects. A Marine pursuit reference is dropped if invisible or beyond a reference distance value of 1152; firing still needs CanFire and the weapon's range.

Distance checks use the approximate integer norm (`0x461fe0`), not Euclidean distance: set M=max(abs(dx),abs(dy)), s=min(abs(dx),abs(dy)), a=s+(s>>1), then return M+(a>>2)+(a>>6)-(M>>7)-(M>>5). Even axial values differ from raw pixel displacement. Apply the 1152 limit to this metric.

MapExplorer (`0x432050`, `0x449980`) records visible town forces and nearby-town counts within an eight-tile box for computer strategy. It does not calculate unit sight. Historical `auto_engage_tiles.csv` estimates are **G**, not a scalar-radius source.

## Damage and categories

**V:** Target virtual predicates are +0xa4 airborne, +0xf0 nonorganic and +0xec structure. Building and Defence satisfy both nonorganic and structure; Bird and Zeppelin are airborne but do not satisfy nonorganic. Marine +0xe8/+0xf4 are complementary categories derived from descriptor d11 byte 1; +0xf8 is byte 2 (Hypno). Multiple predicates apply successively.

Marine weapon getter (`0x4166a0`) returns no weapon if its damage calculation for that target is <=0. Generic CanAttack (`0x43e9f0`) also requires a live target and weapon CanFire. This rules out a blanket “all air/structures are untargetable” interpretation.

| Attacker | Ordinary building / Defence base damage | Air base damage |
|---|---|---|
| Footman | 600 x0.5 nonorganic x0.5 structure =150 | 0 |
| Archer | 800 x0.75 nonorganic x0.5 structure =300 | 800 |
| Rifleman | 1600 | 2000 |

These numbers are **V code calculations**, not newly observed hits. Normal global combat assignment excludes ordinary buildings/town halls from its candidate-member path, while Defence passes +0xfc. Explicit structure orders and the final building-distance/CanFire path still need live checks (**O**). The old custom-map generator also placed its automatic hall on top of the target structure, inherited four footmen and supplied incorrect Defence/Bird/Zeppelin HP. Those setup defects are fixed; no-hit logs do not prove immunity.

Weapon IDs map through `0x494d30` to class and parameter; prototypes live in `0x515ac0`. Virtual +0x5c computes damage, +0x60 reload, +0x74 range, +0x78 CanFire. Estimated kill time (`0x46fac0`) uses target HP, damage and reload, with a special target-HP accessor for +0xf8 attackers.

- **V:** Classes 2/13 (`0x46dfe0`): start rec[1], multiply and shift by 8 for airborne rec[5], nonorganic rec[6], +0xf4 rec[7], +0xe8 rec[8], structure rec[9], then add rec[2]*physical_armor/256. Reload rec[0], range rec[4].
- **V:** Arrow class 1 (`0x403d00`): start rec[4]; corresponding multipliers rec[7..11]; add rec[5]*armor/256 and secondary weapon rec[12]. An airborne multiplier of zero also suppresses the armor component. Speed rec[1], reload rec[2], range rec[3].
- **V code:** Classes 4/5: 3000, 1500 versus nonorganic or structure, zero versus allied/air; reload 19, range 72. Class 6: 18000, 9000 versus nonorganic, zero versus air; reload 25, range 10. Class 10: 1800, 1125 versus nonorganic or structure, x1.25 for Marine category flags, zero versus air; reload 11, range 10. Classes 8 and 12 delegate/sum other weapons.
- **G/O:** Full semantics of classes 0/3/7/9/11 and projectile lifetime/splash/burn sequencing are not fully annotated. Generated weapons preserve their individual uncertainty.

**V observations:** Footman versus ordinary Marine: 600 per 10 ticks; versus a truck: 300. Archer direct damage: 800. Observed hit intervals include projectile flight and animation. Marine cooldown is reload+1; it is not always the interval between target HP drops. `duel_damage.csv` contains historical noisy/no-target cases.

## Economy and harvest cycle

**V:** Clock `0x4c1968` advances once per logic tick and wraps at 7200 (`0x4184a0`). Mission StartTime sets its initial phase. Completed Building (`0x410010`) creates two independent accumulators: gold at object +0x70 and repair credit supply at +0x74. They only progress while the building is active (`0x410cc0`).

| Descriptor field | Meaning |
|---|---|
| +0x18..+0xd8 | 7x7 occupancy mask; cells equal to 2 are occupied |
| +0xdc / d55 | Sight mask ID |
| byte +0xed | Whether gold rate is terrain dependent |
| +0xf0 / d60 | Gold constant/channel A coefficient |
| +0xf4 / d61 | Channel B coefficient |
| +0xf8 / d62 | Repair-credit rate; not passive gold |
| +0xfc / d63 | Gold uses an explicit day-clock schedule |
| +0x100 / d64 | Gold schedule count or periodic interval; -1 disables it |
| +0x104 onward | Day-clock schedule entries |
| +0x500 / d320 | Periodic interval for repair-credit supply |

Constant gold rate in 8.8 units is `(floor((d60<<16)/100)+128)>>8`. Terrain rate (`0x42acd0`, `0x42b040`) averages the map's channel A/B bytes over occupancy cells equal to 2. Channel A/B resource names remain unresolved (see `formats.md`). For each coefficient form `C=((floor((coefficient<<16)/100)*2)+256)>>8`; sum `2*A*Ca + 2*B*Cb`, divide by occupied count, then shift right 8. This yields the per-tick accumulator increment. Mines are therefore location dependent.

Harvest (`0x42b180`, `0x42b1e0`, `0x42b200`) starts accumulator A at zero and adds its increment every active building tick. On an event, return `(A+128)>>8`; reset A only if that value is nonzero. Gold payout (`0x4111af`) is original truncation of this integer times the warehouse factor, with no further +0.5.

Periodic triggers (`0x45f8c0`) measure day-clock difference modulo 7200 from their creation/last event, fire when it reaches the period and record current clock. Scheduled triggers start at entry 0 and consume at most one due entry per tick. After the last entry, reset the cursor only when clock <=first entry; missed dates do not create a backlog of full harvests.

| Farm | Clock entries |
|---|---|
| Rus | 1000,2050,3100,4150,5200,6250 |
| Aztec | 700,1350,2000,2650,3300,3950,4600,5250,5900,6550 |
| Chinese | 0,900,1800,2700,3600,4500,5400,6300 |

The Chinese reset comparison includes equality because its first entry is zero. At StartTime 1200, Rus consumes the already-due 1000 entry immediately and next pays near elapsed tick 850; Aztec consumes 700 and next pays near 150, then 800. This explains the old partial-cycle logs. The gold amounts vary with terrain and fixed-point rounding; do not encode +146 or +125 as race constants.

**V:** `Town::harvest multiplier` (`0x45c450`) applies only to farm IDs 12/39/65. Any completed warehouse ID 18/40/70 in the same town enables the single float32 factor 1.3333333730697632 instead of 1. No stacking, distance or matching-race check. Mines and halls always receive factor 1. RUS_TOWN/VILLAGE d60=20 with period 300 produce roughly 60 per event; RUS_FORPOST d60=10/300, RUS_FORTRESS d60=60/150.

## Repair, destroy, garrison and limits

**V:** Garrison packet 0x1d (`0x448350`) extracts live non-building members of the selected town into a new tactic, then selects that tactic if nonempty. No building boarding or slot-capacity mechanism has been identified (**O**). The label “garrison” must not justify implementing a transport feature.

**V:** Repair packet 0x1e (`0x4577b0`) changes selected town repairer mode 0 off /1 repair /2 destroy. A mode change clears its pending list and object marks (`0x4445c0`, `0x444b80`). Completed buildings' separate d62/d320 Harvest accumulators supply integer credits; the repairer receives harvested_value*100, processes work and clears the budget each tick. This is independent of Side cash.

Eligible marked objects are building kind (including Defence), attached to a tactic, excluding the town hall and current construction. Repair requires missing HP; destroy may accept full-health targets. Off-mode town service also places damaged eligible ordinary buildings into the list (`0x45d4d0`). Repair processes Defence first and ordinary buildings in groups of decreasing damage, then distributes integer shares within each group (`0x444730`, `0x444920`). Repair factor is 1.0. Destroy triples the group budget, applies weapon 10 with damage factor 3.0, and divides remaining budget by 3; normal damage handling still applies.

Destroy packet 0x1a (`0x4489b0`) toggles an owned building under the cursor in the selected town's nonzero-mode list; it rejects a different town's object and current construction. It is not immediate deletion of arbitrary selected objects. No cash refund is present in this command path.

**G:** Workshops/stables/air facilities also enable nearby allied mobile-unit service for 10 HP. The small-pool and larger-pool branches use different spatial/category paths, so exact boundaries and airborne parity remain **O**.

**O:** No universal population cap is verified. The inspected build-tree/production path has prerequisite/type counts and a queue of 50, while assignment arrays truncate at 100. Neither proves a side cap. The rewrite's 80-unit limit is a placeholder. Side +0x144 is completed-type counts, +0x2b0 disabled-type flags, +0x698 cash, +0x63c earned, +0x640 spent. Historical unit-log side fields are unreliable because they interpret the tactic pointer at +0x30 as a Side pointer; raw +0x3c is not necessarily virtual max HP.

**V:** Town upgrades install the race/level build tree through hall completion callbacks (`0x459bb0`, `0x45c4f0`). Village IDs 11/38/64, town IDs 19/47/71 and fortress IDs 24/76 are recognized hall upgrades. **O:** Universal troop-stat upgrades and all difficulty modifiers outside the inspected price/time and loader paths remain unverified. See [AI rules](rules_ai.md) for autonomous development and attack logic.
