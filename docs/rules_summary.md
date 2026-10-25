# Rules summary (consumed by the sim engine)

Machine-readable: `docs/data/rules.json` (built by `python3 -I tools/build_rules.py` from `docs/data/*.csv`, which `tools/extract_rules.py` writes from `Aztec.exe` plus a `heapdump`).
Details and code addresses: `docs/rules_types.md`. Status: **V** verified (code read and/or observed in the running game), **G** guess, **O** open.

| Rule | Status | Source |
|---|---|---|
| Logic tick 12 Hz, tile 48 px, fixed point 8.8 (product >> 8) | V | code |
| Type table: 90 UGI types, kind, param | V | `types.csv` |
| Price table `UnitPrices[90]`, `BuildTime[90]`, DevLevel, build tree per race/hall (`buildtree.csv`) | V (tables) | exe |
| Price = ftol(UnitPrices*K*f+0.5); f=0.8 for computer side not hostile to all; K(adv)=1.0 | V code / G applies to humans | 0x45c5b0 |
| Time = ftol(BuildTime*1.5*f'+0.5) (KAdvPostTime), Kprice/Ktime arrays unused unless Town+0x15c != 0 | G | 0x45c690 |
| AI actual spend: buildings/trucks 0.8*P+1 (workshop 961, mine 121, truck 361) | V observed | zeco1 |
| AI footman costs 161 every 56 ticks | O (formula says 81) | zeco1 |
| Marine speed = d5 px/tick; Headtruck/Spider speed = d7; Zeppelin/Truck = d5 | V measured | `speed_measured.csv` |
| Marine HP regen d2 per tick (8 for footman), initial HP d1, max d4 | V | log |
| Armor model: damage multipliers by target category (air 0xa4, non-organic 0xf0, structure 0xec, Marine flags 0xe8/0xf4); armor d13 scales armor-based part | V code, V footman vs truck 300 | duels |
| Weapon id -> (class, param) table, per-class record formulas (classes 1,2/13,0,3,4/5,6,8,9,10,12) | V code | `rules.json weapons` |
| Weapon class 7 (distance-based) and class 11 damage | O / G | |
| Reload ticks / range px per weapon (class 1: d2/d3, class 2/13: d0/d4, 4/5: 19/72, 6: 25/10, 10: 11/10, 11: 40/96) | V code; V rifleman range 72 (hit at 68 px) | |
| Hit interval observed = reload+1 for footman (10); archer ~19 (reload 12 + flight/anim) | V / G | duels |
| Projectile speed (Arrow d1 px/tick), flight time | G | |
| Sight radius per type | O (no field identified). Idle units auto-engage within 3-6 tiles (type dependent) | G weak |
| Buildings: HP d1, 7x7 occupancy grid (+0x18..0xdc), attack weapon id d64? | V HP, V grid, G weapon | |
| Passive income: hall/village f8/100 gold per tick, paid every 300 ticks (RUS_TOWN +60, RUS_VILLAGE +60 more) | V | zecn |
| Mines: RUS +3 per 100 ticks, AZT +6 per 100 ticks | V | zecn |
| Farms: lump payments (RUS +146 at ~852; AZT 29 @152, 125 @800), cycle unknown | G | zecn |
| Warehouse multiplier 1.333 (KWarehouse, types 0x12,0x28,0x46) | G code, not seen | |
| Side: money +0x698, spent +0x640, earned +0x63c; allied/enemy/neutral masks +0x648/0x64c/0x650; per-type arrays +0x144/+0x30c | V (money), G (arrays) | |
| Difficulty modifiers | O (AI spend unaffected by Side Difficulty 0..200) | zecd |
| Population limits, repair, destroy, garrison, Tactic/Defence/Triggers parameters | O | |
