#!/usr/bin/env python3 -I
"""Consolidate numeric CSVs and rules_mechanics.json into docs/data/rules.json.
Run tools/extract_rules.py first. Field labels are only given where verified from code or experiment."""
import csv, json, os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
def rd(fn): return list(csv.DictReader(open('docs/data/' + fn)))
def num(x):
    try: return int(x)
    except ValueError:
        try: return float(x)
        except ValueError: return x
R = {'_note': 'status: V/verified = code read and/or observed in game; G/guess = inferred; O = open; raw = unlabeled dwords', 'constants': {}}
types = {int(r['id']): {k: num(v) for k, v in r.items()} for r in rd('types.csv')}
desc = {k: {int(r['param']): [num(r['d%d' % i]) for i in range(len(r) - 1)] for r in rd('desc_%s.csv' % k)} for k in
        ('marine', 'building', 'defence', 'headtruck', 'truck', 'spider', 'zeppelin', 'bird', 'cannon')}
speed = {int(r['type']): float(r['mean_px_per_tick_midrun']) for r in rd('speed_measured.csv')}
# per-kind labeled field indices: name -> (index, status)
L = {'marine': {'anim_id': (0, 'verified'), 'hp_initial': (1, 'verified'), 'regen_per_tick': (2, 'verified'), 'regen_cap_alt': (3, 'verified'), 'hp_max': (1, 'verified'), 'secondary_hp_initial': (4, 'verified'),
                'speed_px_per_tick': (5, 'verified'), 'sight_mask_id': (6, 'verified'), 'weapon_id': (7, 'verified'), 'flags_bytes': (11, 'verified'), 'armor_fire': (12, 'verified'), 'armor_physical': (13, 'verified'), 'armor_d14': (14, 'guess')},
     'headtruck': {'hp_initial': (3, 'verified'), 'hp_max': (6, 'verified'), 'speed_px_per_tick': (7, 'verified'), 'sight_mask_id': (8, 'verified'), 'weapon_id': (9, 'guess'), 'armor_fire': (14, 'guess'), 'armor_physical': (15, 'guess')},
     'spider': {'hp_initial': (3, 'verified'), 'hp_max': (6, 'verified'), 'speed_px_per_tick': (7, 'verified'), 'sight_mask_id': (8, 'verified'), 'weapon_id': (9, 'guess')},
     'zeppelin': {'hp_initial': (1, 'verified'), 'sight_mask_id': (7, 'verified'), 'speed_px_per_tick': (5, 'verified')},
     'truck': {'hp_initial': (4, 'guess'), 'sight_mask_id': (6, 'verified'), 'speed_px_per_tick': (5, 'verified')},
     'bird': {'hp_initial': (1, 'verified'), 'sight_mask_id': (8, 'verified')},
     'cannon': {'sight_mask_id': (6, 'verified')},
     'defence': {'hp_initial': (3, 'verified'), 'sight_mask_id': (58, 'verified'), 'weapon_id': (59, 'verified')},
     'building': {'hp_initial': (1, 'verified'), 'sight_mask_id': (55, 'verified'),
                  'harvest_grass_factor': (60, 'verified'), 'harvest_stone_factor': (61, 'verified'),
                  'repair_income_per_100_clock_ticks': (62, 'verified'),
                  'harvest_schedule_count_or_period': (64, 'verified')}}
out_types = []
kmap = {'Marine': 'marine', 'Headtruck': 'headtruck', 'Spider': 'spider', 'Zeppelin': 'zeppelin', 'Truck': 'truck', 'Bird': 'bird', 'Building': 'building', 'Defence': 'defence', 'Cannon': 'cannon'}
for i in range(90):
    t = dict(types[i]); k = kmap.get(t['kind'])
    if k and t['param'] in desc[k]:
        d = desc[k][t['param']]; t['descriptor_raw'] = d
        f = {}
        for name, (ix, st) in L.get(k, {}).items():
            if ix < len(d): f[name] = {'value': d[ix], 'status': st}
        if f: t['fields'] = f
    if i in speed: t['speed_measured_px_per_tick'] = speed[i]
    out_types.append(t)
R['types'] = out_types
# weapons
ids = rd('weapons_ids.csv'); tabs = {}
for fn, key in (('weapon_class_arrow1.csv', 1), ('weapon_class_ball0.csv', 0), ('weapon_class_axe3.csv', 3), ('weapon_class_w7.csv', 7), ('weapon_class_w8.csv', 8), ('weapon_class_w9.csv', 9), ('weapon_class_w12.csv', 12)):
    tabs[key] = {int(r['param']): [num(r['d%d' % i]) for i in range(len(r) - 1)] for r in rd(fn)}
tabs[2] = tabs[13] = {int(r['param']): [num(r['d%d' % i]) for i in range(len(r) - 1)] for r in rd('weapon_class_w2_13.csv')}
W = []
for r in ids:
    wid, c, p = int(r['weapon_id']), int(r['class']), int(r['param'])
    w = {'id': wid, 'class': c, 'param': p}
    rec = tabs.get(c, {}).get(p)
    if rec: w['record_raw'] = rec
    if c in (2, 13) and rec:
        w.update(reload_ticks=rec[0], damage=rec[1], armor_scaled_damage=rec[2], range_px=rec[4], mult_air=rec[5] / 256, mult_nonorganic=rec[6] / 256, mult_f4=rec[7] / 256, mult_e8=rec[8] / 256, mult_structure=rec[9] / 256, status='verified (formula from code; footman 600/10 observed)')
    elif c == 1 and rec:
        w.update(projectile_speed=rec[1], reload_ticks=rec[2], range_px=rec[3], damage=rec[4], armor_scaled_damage=rec[5], mult_air=rec[7] / 256, mult_nonorganic=rec[8] / 256, mult_f4=rec[9] / 256, mult_e8=rec[10] / 256, mult_structure=rec[11] / 256, chained_weapon=rec[12], status='verified (formula from code; archer 800 observed)')
    elif c == 0 and rec: w.update(reload_ticks=rec[2], range_px=rec[3], delegates_to_weapon=rec[4], status='guess (reload/range via vtable +0x60/+0x74, damage delegated)')
    elif c == 3 and rec: w.update(reload_ticks=rec[2], range_px=rec[3], damage=rec[4], chained_weapon=rec[5], status='guess')
    elif c == 8 and rec: w.update(delegates_to_weapon=rec[0], status='verified (code)')
    elif c == 12 and rec: w.update(sum_of_weapons=[rec[0], rec[1]], status='verified (code)')
    elif c == 9 and rec: w.update(damage=rec[0], armor_scaled_damage=rec[1], mult_air=rec[3] / 256, mult_nonorganic=rec[4] / 256, mult_f4=rec[5] / 256, mult_e8=rec[6] / 256, mult_structure=rec[7] / 256, status='guess')
    elif c in (4, 5): w.update(reload_ticks=19, range_px=72, damage=3000, damage_vs_nonorganic_or_structure=1500, status='verified (code; rifleman first hit at 68 px observed); 0 vs allied or air')
    elif c == 6: w.update(reload_ticks=25, range_px=10, damage=18000, damage_vs_nonorganic=9000, status='verified (code); 0 vs air')
    elif c == 10: w.update(reload_ticks=11, range_px=10, damage=1800, damage_vs_nonorganic_or_structure=1125, mult_organic_f4_e8=1.25, status='verified (code); 0 vs air')
    elif c == 11: w.update(reload_ticks=40, range_px=96, status='guess (damage 10*f(0x429f60) per tick of burn)')
    elif c == 7: w.update(status='unread (distance based, 0x44c130)')
    W.append(w)
R['weapons'] = W
R['build_tree'] = [{k: num(v) for k, v in r.items()} for r in rd('buildtree.csv')]
R['townhall_ids'] = [{k: num(v) for k, v in r.items()} for r in rd('townhall_ids.csv')]
R['mechanics'] = json.load(open('docs/data/rules_mechanics.json'))
R['economy'] = R['mechanics']['economy']
R['auto_engage'] = R['mechanics']['target_acquisition']
R['constants'].update({'tick_hz': {'value': 12, 'status': 'verified'}, 'tile_px': {'value': 48, 'status': 'verified'},
    'fixed_point': {'value': '8.8, product>>8', 'status': 'verified'},
    'price_formula': R['mechanics']['production']['price'],
    'Kprice': R['mechanics']['production']['build_speed']['Kprice'], 'Ktime': R['mechanics']['production']['build_speed']['Ktime'], 'KAdvPostTime': 1.5, 'KAdvPostPrice': 1.0, 'KWarehouse': [1.0, R['mechanics']['economy']['warehouse']['factor_with']]})
json.dump(R, open('docs/data/rules.json', 'w'), indent=1)
print('rules.json', os.path.getsize('docs/data/rules.json'))
