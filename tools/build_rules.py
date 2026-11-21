#!/usr/bin/env python3 -I
"""Consolidate numeric CSVs and rules_mechanics.json into docs/data/rules.json.
Descriptor labels and evidence come from descriptor_fields.csv. Raw numeric CSVs
are preserved; regeneration requires no game installation or executable."""
import argparse, csv, json, os
from pathlib import Path
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
def rd(fn):
    with open('docs/data/' + fn, newline='', encoding='utf-8') as source:
        return list(csv.DictReader(source))
def num(x):
    try: return int(x)
    except ValueError:
        try: return float(x)
        except ValueError: return x
R = {'_note': 'status: V/verified = inspected read or observed fact; G/guess = inferred meaning; unused = no runtime reader found in the audited consumers; O = open. Descriptor coverage is naming coverage, not game parity. See descriptor_layouts and docs/descriptor_fields.md.', 'constants': {}}
types = {int(r['id']): {k: num(v) for k, v in r.items()} for r in rd('types.csv')}
desc = {k: {int(r['param']): [num(r['d%d' % i]) for i in range(len(r) - 1)] for r in rd('desc_%s.csv' % k)} for k in
        ('marine', 'building', 'defence', 'headtruck', 'truck', 'spider', 'zeppelin', 'bird', 'cannon')}
speed = {int(r['type']): float(r['mean_px_per_tick_midrun']) for r in rd('speed_measured.csv')}
# One CSV row per word, including zero-valued indexed cells and unused sentinels.
layouts = {}
kmap = {}
for row in rd('descriptor_kinds.csv'):
    kind = row['kind']
    kmap[row['object_kind']] = kind
    layouts[kind] = {
        'table_address': row['table_address'],
        'pointer_field': row['pointer_field'],
        'vtable_address': row['vtable_address'],
        'constructors': row['constructors'].split(';'),
        'word_count': int(row['words']),
        'words': [],
    }
for row in rd('descriptor_fields.csv'):
    kind = row['kind']
    index = int(row['index'])
    if not 0 <= index < layouts[kind]['word_count']:
        raise ValueError(f'{kind}: descriptor index out of bounds: {index}')
    if not row['name'] or row['status'] not in ('verified', 'guess', 'unused'):
        raise ValueError(f'{kind} d{index}: missing name or valid status')
    if not row['meaning'] or (row['status'] != 'unused' and not row['evidence']):
        raise ValueError(f'{kind} d{index}: missing meaning or evidence')
    layouts[kind]['words'].append({
        'index': index, 'offset': index * 4, 'name': row['name'],
        'status': row['status'], 'meaning': row['meaning'],
        'evidence': row['evidence'].split(';') if row['evidence'] else [],
        'aliases': row['aliases'].split(';') if row['aliases'] else [],
    })
active = {}
for kind, layout in layouts.items():
    words = layout['words']
    words.sort(key=lambda word: word['index'])
    if [word['index'] for word in words] != list(range(layout['word_count'])):
        raise ValueError(f'{kind}: duplicate or missing descriptor words')
    names = [name for word in words for name in [word['name'], *word['aliases']]]
    if len(set(names)) != len(names):
        raise ValueError(f'{kind}: duplicate descriptor names or aliases')
    if any(len(raw) != layout['word_count'] for raw in desc[kind].values()):
        raise ValueError(f'{kind}: raw descriptor width disagrees with layout')
    # Keep per-type fields compact; the shared layout names every zero word too.
    active[kind] = {i for i in range(layout['word_count'])
                    if any(raw[i] != 0 for raw in desc[kind].values())}
R['descriptor_layouts'] = layouts
out_types = []
for i in range(90):
    t = dict(types[i]); k = kmap.get(t['kind'])
    if k and t['param'] in desc[k]:
        d = desc[k][t['param']]; t['descriptor_raw'] = d
        f = {}
        for word in layouts[k]['words']:
            ix = word['index']
            if ix in active[k]:
                for name in [word['name'], *word['aliases']]:
                    f[name] = {'value': d[ix], 'status': word['status'], 'index': ix}
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
        w.update(reload_ticks=rec[0], damage=rec[1], armor_scaled_damage=rec[2], range_px=rec[4], mult_air=rec[5] / 256, mult_nonorganic=rec[6] / 256, mult_f4=rec[7] / 256, mult_e8=rec[8] / 256, mult_structure=rec[9] / 256, armor_helper_duration_ticks=rec[3], status='verified (V: direct category damage; optional armor helper, inactive in current records; ground_weapons.md)')
    elif c == 1 and rec:
        w.update(projectile_speed=rec[1], reload_ticks=rec[2], range_px=rec[3], damage=rec[4], armor_scaled_damage=rec[5], mult_air=rec[7] / 256, mult_nonorganic=rec[8] / 256, mult_f4=rec[9] / 256, mult_e8=rec[10] / 256, mult_structure=rec[11] / 256, chained_weapon=rec[12], status='verified (formula from code; archer 800 observed)')
    elif c == 0 and rec: w.update(projectile_speed=rec[1], reload_ticks=rec[2], range_px=rec[3], delegates_to_weapon=rec[4], status='verified (record and delegated damage; class link inferred)', class_link_status='guess', evidence=['0x46d900', '0x406700', '0x4068e0', '0x406960', '0x4069c0', '0x4039a0', '0x4039b0'])
    elif c == 3 and rec: w.update(projectile_speed=rec[1], reload_ticks=rec[2], range_px=rec[3], damage=rec[4], contact_damage_per_tick=rec[4], chained_weapon=rec[5], mult_nonorganic=176 / 256, damage_speed_numerator=48, damage_formula='base factor is zero against allied or airborne, otherwise reduced by 176/256 for nonorganic; multiply base by integer floor(48/speed), then always add secondary weapon damage when present', status='verified (V: contact ticks plus spatial impact; damage getter is an estimate; ground_weapons.md; class link inferred)', class_link_status='guess', evidence=['0x46d900', '0x404c80', '0x404b90', '0x404c30', '0x404e10', '0x404f70'])
    elif c == 8 and rec: w.update(delegates_to_weapon=rec[0], status='verified (code)')
    elif c == 12 and rec: w.update(sum_of_weapons=[rec[0], rec[1]], status='verified (code)')
    elif c == 9 and rec: w.update(damage=rec[0], armor_scaled_damage=rec[1], mult_air=rec[3] / 256, mult_nonorganic=rec[4] / 256, mult_f4=rec[5] / 256, mult_e8=rec[6] / 256, mult_structure=rec[7] / 256, status='verified (V: point impact applies to every overlapping target; ground_weapons.md; class link inferred)', class_link_status='guess', evidence=['0x46d900', '0x444080', '0x444160'])
    elif c in (4, 5): w.update(reload_ticks=19, range_px=72, damage=3000, damage_vs_nonorganic_or_structure=1500, status='verified (code; rifleman first hit at 68 px observed); 0 vs allied or air; central sector and two half-damage flanks (V; ground_weapons.md)')
    elif c == 6: w.update(reload_ticks=25, range_px=10, damage=18000, damage_vs_nonorganic=9000, target_attack_cooldown_add=18, status='verified (V: 0 vs air; cooldown area is a 96 px square around attacker, not a turn/freeze; ground_weapons.md)')
    elif c == 10: w.update(reload_ticks=11, range_px=10, damage=1800, damage_vs_nonorganic_or_structure=1125, mult_organic_f4_or_e8=1.25, attacker_heal_divisor=3, status='verified (V: 0 vs air; heal uses nominal damage, capped at maximum HP; ground_weapons.md)')
    elif c == 11: w.update(reload_ticks=40, range_px=96, damage_base=2000, damage_estimate_multiplier=10, mult_allied=64 / 256, damage_formula='damage getter returns ten times the helper damage estimate; helper base is 2000 scaled by allied and fire armor factors, zero for self or airborne targets', status='verified (damage formula; class link inferred; helper sequencing open)', class_link_status='guess', hit_effect_status='open (helper sequencing)', evidence=['0x46d900', '0x429c90', '0x429c00', '0x429f60', '0x42a010', '0x429c20', '0x429c30'])
    elif c == 7 and rec: w.update(reload_ticks=rec[6], range_px=rec[3], target_airborne=bool(rec[5]), target_extent_px=28, inner_damage_bypasses_armor=True, inner_radius_px=rec[1], outer_radius_px=rec[2], damage=rec[4], required_airborne_flag=rec[5], mult_e8=204 / 256, armor_virtual='0x108', damage_formula='add target extent to both radii; full damage inside inner radius bypasses armor; otherwise use 8.8 linear falloff, then category and fire armor factors; zero beyond outer radius or for mismatching airborne flag', status='verified (V: 8.8 falloff outside inner radius plus target extent; inner damage bypasses armor; ground_weapons.md; class link inferred)', class_link_status='guess', evidence=['0x46d900', '0x44c010', '0x44c100', '0x44c130', '0x44c3c0'])
    elif c == 14: w.update(status='verified (V: id 0 is the absent-chain sentinel; no factory constructor or campaign weapon; ground_weapons.md)')
    W.append(w)
R['weapons'] = W
R['build_tree'] = [{k: num(v) for k, v in r.items()} for r in rd('buildtree.csv')]
R['townhall_ids'] = [{k: num(v) for k, v in r.items()} for r in rd('townhall_ids.csv')]
with open('docs/data/rules_mechanics.json', encoding='utf-8') as source:
    R['mechanics'] = json.load(source)
R['economy'] = R['mechanics']['economy']
R['auto_engage'] = R['mechanics']['target_acquisition']
R['constants'].update({'tick_hz': {'value': 12, 'status': 'verified'}, 'tile_px': {'value': 48, 'status': 'verified'},
    'fixed_point': {'value': '8.8, product>>8', 'status': 'verified'},
    'price_formula': R['mechanics']['production']['price'],
    'Kprice': R['mechanics']['production']['build_speed']['Kprice'], 'Ktime': R['mechanics']['production']['build_speed']['Ktime'], 'KAdvPostTime': 1.5, 'KAdvPostPrice': 1.0, 'KWarehouse': [1.0, R['mechanics']['economy']['warehouse']['factor_with']]})
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=Path('docs/data/rules.json'))
args = parser.parse_args()
args.output.write_text(json.dumps(R, indent=1) + '\n', encoding='utf-8')
print(args.output, args.output.stat().st_size)
