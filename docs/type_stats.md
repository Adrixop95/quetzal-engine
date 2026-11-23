# Type statistics (from observing live objects of a test map)

Evidence labels (audit 2026-11-23): V = inspected source fact, with reference
or clone scope stated; M = measurement rechecked from retained captures or an
offline clone probe; G = inference, proposed procedure or uncorroborated
historical report. A clone V does not establish native parity. A marker covers
one paragraph/list item or the entire immediately following table. Unresolved
meanings remain G even inside a table whose codec structure is V.
See [label_audit.md](label_audit.md) for counts and evidence limits.

**V** (reference fields and clone loader). Source: test map with one unit of each type (`tools/ztest_run.sh`). `secondary_hp` = object field `+0x3c`, not maximum primary HP. For class Marine (kind 0) the type description record lies at
`0x4ac430 + parameter*0x4c` (parameter = second column of the type table); the constructor `0x433190` sets: `object+0x7c = Anim(animation id = record[0])`, `object+0x38 = record[1]` (starting health),
`object+0x3c = record[4]` (secondary health). Maximum primary HP is returned from record[1] by `0x45fe50`. Record[5] is speed, [7] is weapon ID, [12..14] are armour accessors, [15..17] are sound-list IDs and [18] is a name pointer. See [descriptor_fields.md](descriptor_fields.md) for the full labelled layout.

**G** (historical secondary-HP snapshot).

| id | type | class | parameter | secondary_hp |
|---|---|---|---|---|
| 0 | FOOTMAN | Marine | 3 | 6000 |
| 1 | RIFLEMAN | Marine | 9 | 5000 |
| 2 | DEMOLISHER | Marine | 14 | 8500 |
| 3 | TRUCK_AXETHROWER | Headtruck | 1 | 20000 |
| 4 | CANNON | Headtruck | 2 | 15000 |
| 5 | BEAR | Marine | 7 | 36000 |
| 6 | KNIGHT | Marine | 6 | 10000 |
| 7 | ZEPPELIN | Zeppelin | 0 | 3000 |
| 8 | TRUCK_FLAMETHROWER | Headtruck | 0 | 17500 |
| 9 | TRUCK_KATUSHA | Headtruck | 6 | 16000 |
| 10 | RUS_FORPOST | Building | 0 | 0 |
| 11 | RUS_VILLAGE | Building | 10 | 0 |
| 12 | RUS_FARM | Building | 7 | 0 |
| 13 | RUS_MINE | Building | 8 | 0 |
| 14 | RUS_BARRACKS | Building | 5 | 0 |
| 15 | RUS_STABLE | Building | 9 | 0 |
| 16 | RUS_BEARLAIR | Building | 6 | 0 |
| 17 | RUS_WATCH_TOWER | Defence | 3 | 0 |
| 18 | RUS_WAREHOUSE | Building | 11 | 0 |
| 19 | RUS_TOWN | Building | 1 | 0 |
| 20 | RUS_WORKSHOP | Building | 4 | 0 |
| 21 | RUS_ARMORY | Building | 28 | 0 |
| 22 | RUS_LABS | Building | 29 | 0 |
| 23 | RUS_FORTRESS_TOWER | Defence | 0 | 0 |
| 24 | RUS_FORTRESS | Building | 2 | 0 |
| 25 | RUS_AIR_FACILITY | Building | 3 | 0 |
| 26 | RUS_CANNON_TOWER | Defence | 1 | 0 |
| 27 | ARCHER | Marine | 10 | 3500 |
| 28 | MAGE | Marine | 8 | 6000 |
| 29 | TUZEM | Marine | 1 | 2600 |
| 30 | SPIDER_CROSSBOW | Spider | 0 | 26000 |
| 31 | JAGUAR | Marine | 12 | 23000 |
| 32 | BIZON | Marine | 13 | 23000 |
| 33 | EAGLE | Bird | 2 | 8000 |
| 34 | DELTAPLAN | Bird | 1 | 12000 |
| 35 | SPIDER_FLAMETHROWER | Spider | 1 | 28000 |
| 36 | SPIDER_AXETHROWER | Marine | 3 | 6000 |
| 37 | AZT_FORPOST | Building | 12 | 0 |
| 38 | AZT_VILLAGE | Building | 13 | 0 |
| 39 | AZT_FARM | Building | 20 | 0 |
| 40 | AZT_WAREHOUSE | Building | 26 | 0 |
| 41 | AZT_BARRACKS | Building | 19 | 0 |
| 42 | AZT_ARCHER_TOWER | Defence | 4 | 0 |
| 43 | AZT_HUT | Building | 31 | 0 |
| 44 | AZT_STABLE | Building | 24 | 0 |
| 45 | AZT_JAGUARLAIR | Building | 21 | 0 |
| 46 | AZT_NEST | Building | 23 | 0 |
| 47 | AZT_TOWN | Building | 14 | 0 |
| 48 | AZT_MINE | Building | 22 | 0 |
| 49 | AZT_CROSSBOW_TOWER | Defence | 5 | 0 |
| 50 | AZT_TEMPLE | Building | 25 | 0 |
| 51 | AZT_WORKSHOP | Building | 27 | 0 |
| 52 | AZT_AIR_FACILITY | Building | 18 | 0 |
| 53 | NINJA | Marine | 2 | 7500 |
| 54 | HYPNO | Marine | 0 | 6000 |
| 55 | CHIEF | Marine | 15 | 5000 |
| 56 | SNOWMAN | Marine | 4 | 22500 |
| 57 | ELEFANT | Marine | 11 | 28000 |
| 58 | WHEEL_KATUSHA | Headtruck | 5 | 14000 |
| 59 | WHEEL_FIRETHROWER | Headtruck | 7 | 16000 |
| 60 | PARAPLAN | Bird | 0 | 7000 |
| 61 | WHEEL_AXETHROWER | Headtruck | 3 | 16000 |
| 62 | WHEEL_CROSSBOW | Headtruck | 4 | 15000 |
| 63 | CHI_FORPOST | Building | 15 | 0 |
| 64 | CHI_VILLAGE | Building | 38 | 0 |
| 65 | CHI_FARM | Building | 35 | 0 |
| 66 | CHI_MINE | Building | 36 | 0 |
| 67 | CHI_BARRACKS | Building | 33 | 0 |
| 68 | CHI_FIRETHROW_TOWER | Building | 16 | 0 |
| 69 | CHI_HYPNO_TOWER | Defence | 8 | 0 |
| 70 | CHI_WAREHOUSE | Building | 39 | 0 |
| 71 | CHI_TOWN | Building | 16 | 0 |
| 72 | CHI_HYPNO_SCHOOL | Building | 42 | 0 |
| 73 | CHI_PRISON | Building | 30 | 0 |
| 74 | CHI_WORKSHOP | Building | 40 | 0 |
| 75 | CHI_ARMORY | Building | 41 | 0 |
| 76 | CHI_FORTRESS | Building | 17 | 0 |
| 77 | CHI_STABLE | Building | 37 | 0 |
| 78 | CHI_CAVE | Building | 34 | 0 |
| 79 | CHI_AIR_FACILITY | Building | 32 | 0 |
| 80 | CHI_WATCHTOWER | Defence | 2 | 0 |
| 81 | CHI_AIRTOWER | Defence | 7 | 0 |
| 82 | ABDULA | Marine | 5 | 3000 |
| 83 | CATAPULT | Truck | 0 | 3000 |
| 85 | ZEPPELIN2 | Zeppelin | 1 | 3000 |
| 86 | SHIPYARD | Defence | 6 | 0 |
| 87 | HOPCANNON | Cannon | 0 | 28000 |

## Label audit corrections (2026-11-23)

Evidence checked: Reference: 0x433190 writes primary d1 and secondary d4 separately; 0x45fe50 returns primary maximum HP. Clone: `Rules::load` and `GameSim` initialization preserve this split. The old object snapshot table remains G: its secondary values, zeros and omitted types are not a complete maximum-HP inventory.

1. Corrected the snapshot column to secondary HP, preserving its recorded numbers without treating zero-valued construction objects as zero maximum HP.
2. Replaced the obsolete unknown/size interpretation of descriptor fields with the verified layout.
3. Removed the raw descriptor-word column; the independently described layout is linked instead.

No original executable or reference runtime was run for this audit. Historical
validation totals retain their original dates; they are not this
audit's test results. Private captures, derived source and probe output remain
outside the tracked documentation.
