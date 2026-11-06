#!/usr/bin/env python3
"""Plan all-type V07 measurements and compare audited per-tick combat bundles."""
import argparse
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location('projectile_measure', ROOT / 'tools/projectile_measure.py')
projectile = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(projectile)
report = projectile.report
# Existing engine descriptor bindings, not new reference measurements.
BINDINGS = {'Marine': (7, 1, 13), 'Spider': (9, 3, 15), 'Building': (None, 1, 57),
            'Zeppelin': (8, 1, 13), 'Truck': (7, 1, 12), 'Bird': (9, 1, 15),
            'Headtruck': (9, 3, 15), 'Defence': (59, 3, 65), 'Cannon': (7, 2, 12)}
TABLES = {0: 'ball0', 1: 'arrow1', 2: 'w2_13', 3: 'axe3', 7: 'w7',
          8: 'w8', 9: 'w9', 12: 'w12', 13: 'w2_13'}
METRICS = ('damage_by_victim', 'reload_ticks', 'range_boundary_px', 'flight_ticks',
           'impact_tick', 'splash_by_victim', 'burn_ticks', 'death_tick', 'corpse_ticks')


def read_csv(name):
    with (ROOT / 'docs/data' / f'{name}.csv').open() as stream:
        return list(csv.DictReader(stream))


def inventory():
    types = []
    for row in read_csv('types'):
        kind = row['kind']
        value = dict(type=int(row['id']), name=row['ugi'], kind=kind, weapon=None,
                     hp=None, armor=None, mapping='unresolved', evidence='P',
                     categories=dict(structure=kind in ('Building', 'Defence'),
                                     flier=kind in ('Zeppelin', 'Bird'),
                                     nonorganic=kind in ('Building', 'Defence', 'Spider', 'Truck', 'Headtruck', 'Cannon'),
                                     category_e8=False, category_f4=False))
        if kind in BINDINGS:
            field, hp, armor = BINDINGS[kind]
            desc = next(d for d in read_csv('desc_' + kind.lower()) if d['param'] == row['param'])
            value.update(hp=int(desc[f'd{hp}']), armor=int(desc[f'd{armor}']))
            if kind == 'Marine':
                e8 = bool((int(desc['d11']) >> 8) & 255)
                value['categories'].update(category_e8=e8, category_f4=not e8)
            if field is None:
                value['mapping'] = 'no descriptor weapon'
            else:
                value.update(weapon=int(desc[f'd{field}']), mapping='descriptor candidate')
        types.append(value)
    weapons = []
    for row in read_csv('weapons_ids'):
        wid, klass, param = (int(row[k]) for k in ('weapon_id', 'class', 'param'))
        desc = next((d for d in read_csv('weapon_class_' + TABLES[klass])
                     if int(d['param']) == param), None) if klass in TABLES else None
        children = []
        if desc:
            fields = {0: ('d4',), 1: ('d12',), 3: ('d5',), 8: ('d0',), 12: ('d0', 'd1')}.get(klass, ())
            children = [int(desc[f]) for f in fields if int(desc[f]) != 0]
        weapons.append(dict(weapon=wid, klass=klass, parameter=param, children=children,
                            owners=[t['type'] for t in types if t['weapon'] == wid],
                            table='present' if desc else 'sentinel' if wid == 0 else 'unresolved',
                            measured=False, matching=None, fixed=False, evidence='P'))
    known = {w['weapon'] for w in weapons}
    if any(t['weapon'] is not None and t['weapon'] >= 0 and t['weapon'] not in known for t in types):
        raise ValueError('descriptor references an absent weapon ID')
    if any(c not in known for w in weapons for c in w['children']):
        raise ValueError('payload references an absent weapon ID')
    by_id = {w['weapon']: w for w in weapons}
    for w in weapons:
        reachable, seen, pending = set(), set(), [w['weapon']]
        while pending:
            wid = pending.pop()
            if wid in seen:
                continue
            seen.add(wid)
            reachable.update(by_id[wid]['owners'])
            pending.extend(parent['weapon'] for parent in weapons if wid in parent['children'])
        w['reachable_owners'] = sorted(reachable)
        w['coverage'] = 'owner scenarios' if reachable else 'requires dispatch audit'
    return dict(status='needs live run', types=types, weapons=weapons)


def scenarios():
    inv = inventory()
    victims = [t for t in inv['types'] if t['hp'] is not None and t['hp'] > 0]
    result = []
    for source in victims:
        if source['weapon'] is None or source['weapon'] <= 0:
            continue
        for target in victims:
            result.append(dict(id=f"type{source['type']}_damage_type{target['type']}",
                               family=f"weapon{source['weapon']}", weapon=source['weapon'],
                               source_type=source['type'], source_hp=source['hp'],
                               target_type=target['type'], target_hp=target['hp'],
                               variant='clear_middle', purpose='damage_by_armor_and_category',
                               map='ztest_battle', evidence='P'))
        for variant in (*projectile.catalogue()['variants'], 'reload', 'range_boundary',
                        'composite_schedule', 'target_death', 'corpse_removal'):
            result.append(dict(id=f"type{source['type']}_{variant}", family=f"weapon{source['weapon']}",
                               weapon=source['weapon'], source_type=source['type'], source_hp=source['hp'],
                               target_type=0, target_hp=next(t['hp'] for t in victims if t['type'] == 0),
                               variant=variant, purpose=variant, map='ztest_battle', evidence='P'))
    return result


def plan(output):
    output = report.private_output(output)
    output.mkdir(parents=True, exist_ok=False)
    inv = inventory()
    report.write_json(output / 'inventory.json', inv)
    report.write_json(output / 'scenarios.json', scenarios())
    report.write_json(output / 'measurement-form.json', dict(
        status='needs live run', evidence='P',
        metrics={m: dict(reference=None, clone_before=None, clone_after=None) for m in METRICS},
        blockers=['Task 64 audited setup/command and reference combat observers are absent.',
                  'Category flags, armor, single-shot/reload/range controls need live audit.',
                  'No observation may be filled from engine expectations.']))
    lines = ['# V07 weapon coverage', '', 'P: existing metadata candidates; needs live run.', '',
             '| Class | IDs | Direct owners | Measured | Matching | Fixed |',
             '| --- | --- | --- | --- | --- | --- |']
    for klass in sorted({w['klass'] for w in inv['weapons']}):
        rows = [w for w in inv['weapons'] if w['klass'] == klass]
        lines.append(f"| {klass} | {', '.join(str(w['weapon']) for w in rows)} | "
                     f"{sum(len(w['owners']) for w in rows)} | No | Unmeasured | No |")
    (output / 'coverage.md').write_text('\n'.join(lines) + '\n')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def ints(row, fields):
    return isinstance(row, dict) and set(row) == set(fields.split()) and all(type(v) is int for v in row.values())


def capture(path):
    b = report.json_object(path.read_text())
    require(set(b) == {'schema', 'contract', 'origin', 'alignment', 'samples'}, 'invalid bundle fields')
    require(type(b['schema']) is int and b['schema'] == 1 and b['contract'] == 'weapon-v1', 'invalid contract')
    require(b['origin'] in ('reference', 'clone'), 'invalid origin')
    a = b['alignment']
    require(isinstance(a, dict) and set(a) == set(
        'scenario specification_sha256 map_sha256 controls_sha256 normalization_sha256 first_tick last_tick phase'.split()),
        'incomplete alignment')
    for key in ('specification_sha256', 'map_sha256', 'controls_sha256', 'normalization_sha256'):
        require(isinstance(a[key], str) and len(a[key]) == 64 and
                all(c in '0123456789abcdef' for c in a[key]), 'invalid alignment digest')
    case = next((c for c in scenarios() if c['id'] == a['scenario']), None)
    require(case is not None, 'unknown scenario')
    digest = hashlib.sha256(json.dumps(case, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    require(a['specification_sha256'] == digest, 'changed scenario specification')
    first, last = a['first_tick'], a['last_tick']
    require(type(first) is int and type(last) is int and first == 1 and 1 < last <= 100000,
            'invalid observation horizon')
    require(a['phase'] == 'after_tick', 'invalid observation phase')
    require(isinstance(b['samples'], list) and len(b['samples']) == last, 'incomplete ticks')
    accepted = False
    source_ids, target_ids = set(), set()
    for tick, row in enumerate(b['samples'], 1):
        require(isinstance(row, dict) and set(row) == {'tick', 'objects', 'flights', 'events', 'unavailable'},
                'incomplete sample')
        require(type(row['tick']) is int and row['tick'] == tick, 'missing or unordered tick')
        require(row['unavailable'] == [], 'unsupported observations block comparison')
        for channel, fields in (
                ('objects', 'id type side x y hp secondary_hp armor categories alive corpse burn'),
                ('flights', 'id source target weapon x y height altitude'),
                ('events', 'kind id source target weapon victim x y height before after')):
            values = row[channel]
            require(isinstance(values, list) and all(ints(v, fields) for v in values), 'invalid ' + channel)
            if channel != 'events':
                identities = [v['id'] for v in values]
                require(identities == sorted(set(identities)) and all(i > 0 for i in identities),
                        'unordered or duplicate identities')
        require(all(0 <= o['type'] < 90 and 0 <= o['side'] < 8 and o['alive'] in (0, 1)
                    and o['corpse'] in (0, 1) and o['burn'] in (0, 1) and o['categories'] >= 0
                    for o in row['objects']), 'invalid object state')
        if tick == 1:
            source_ids = {o['id'] for o in row['objects'] if o['type'] == case['source_type'] and o['side'] == 0}
            target_ids = {o['id'] for o in row['objects'] if o['type'] == case['target_type'] and o['side'] == 1}
            require(source_ids and target_ids, 'missing initial source/target')
        require(all(1 <= e['kind'] <= 9 for e in row['events']), 'unknown event kind')
        accepted |= any(e['kind'] == 1 and e['weapon'] == case['weapon']
                        and e['source'] in source_ids and e['target'] in target_ids for e in row['events'])
    require(accepted, 'no accepted attack; cannot compare an idle no-hit control')
    return b


def difference(left, right):
    for a, b in zip(left['samples'], right['samples']):
        for channel in ('objects', 'flights', 'events'):
            if a[channel] != b[channel]:
                return dict(tick=a['tick'], channel=channel, reference=a[channel], actual=b[channel])
    return None


def compare(reference, repeat, clone):
    a, r, b = (capture(p) for p in (reference, repeat, clone))
    require([v['origin'] for v in (a, r, b)] == ['reference', 'reference', 'clone'], 'invalid capture origins')
    require(a['alignment'] == r['alignment'] == b['alignment'], 'unaligned captures')
    unstable = difference(a, r)
    delta = unstable or difference(a, b)
    return dict(status='reference nondeterminism' if unstable else 'different' if delta else 'equal',
                first=delta, repeatable_difference=not unstable and delta is not None,
                evidence='Comparison only; reviewed observer and setup audit required.',
                scenario=a['alignment']['scenario'], ticks=len(a['samples']))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    for mode in ('plan', 'prepare', 'compare'):
        command = sub.add_parser(mode)
        command.add_argument('--output', type=Path, required=True)
        if mode == 'prepare':
            command.add_argument('--id', required=True)
            command.add_argument('--game', type=lambda p: Path(p).resolve(), required=True)
        elif mode == 'compare':
            for role in ('reference', 'repeat', 'clone'):
                command.add_argument('--' + role, type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.mode == 'plan':
            plan(args.output)
        elif args.mode == 'prepare':
            case = next((c for c in scenarios() if c['id'] == args.id), None)
            require(case is not None, 'unknown scenario')
            projectile.prepare(case, args.game, args.output)
        else:
            output = report.private_output(args.output)
            require(not output.exists(), 'output already exists')
            result = compare(args.reference, args.repeat, args.clone)
            output.parent.mkdir(parents=True, exist_ok=True)
            report.write_json(output, result)
            return int(result['status'] != 'equal')
        return 0
    except (OSError, ValueError, KeyError, TypeError, StopIteration, struct.error) as exc:
        parser.exit(2, f'weapon_measure: {exc}\n')


if __name__ == '__main__':
    sys.exit(main())
