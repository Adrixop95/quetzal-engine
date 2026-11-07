#!/usr/bin/env python3
"""Prepare private V07 candidates and summarize complete engine combat traces."""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / f'{name}.py')
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


maps = module('make_testmaps')
report = module('parity_report')


def catalogue():
    return json.loads((ROOT / 'tests/parity/projectiles.json').read_text())


def scenarios():
    spec = catalogue()
    return [dict(id=f'{family}_{variant}', family=family, source_type=tid, variant=variant,
                 map=spec['map'], evidence='P')
            for family, tid in spec['families'].items() for variant in spec['variants']]


def prepare(case, source, output):
    """Generate placement candidates only; never translate unaudited commands."""
    output = report.private_output(output)
    output.mkdir(parents=True, exist_ok=False)
    private = output / 'game'
    private.mkdir()
    (private / 'Maps').mkdir()
    (private / 'Saves').mkdir()
    for name in ('Aztec.exe', 'Res', 'Video', 'Music', 'Campaign'):
        if (source / name).exists():
            (private / name).symlink_to((source / name).resolve())
    for item in source.iterdir():
        if item.is_file() and item.suffix.lower() in ('.ini', '.plr', '.cfg'):
            shutil.copy2(item, private / item.name)
    spec = catalogue()
    sources = {ext: maps.find(source / 'Maps', f"{spec['template']}.{ext}")
               for ext in (*maps.EXTENSIONS, 'mp')}
    terrain = sources['map'].read_bytes()
    w, h = struct.unpack('<HH', terrain[:4])
    dat = sources['dat'].read_bytes()
    if min(w, h) < 180 or len(dat) != w * h or len(terrain) != 4 + 4 * w * h:
        raise ValueError('requires valid campaign terrain at least 180 by 180')
    mission = maps.Mission(spec['map'], w, h, dat,
                           int(maps.read_ini(sources['mp'])['Globals']['TileSet']))
    for side in range(3):
        mission.side(side, neutral=side == 2)
        mission.town(side, side, 3, 15 + side * 20, h - 20)
    rows = maps.metadata()
    # Passive-player and exact single-shot commands require the audited V02/L1 adapter.
    variant = case['variant']
    x, y = w // 2, h // 2
    separation = {'clear_near': 2, 'clear_middle': 4, 'clear_far': 8}.get(variant, 4)
    for role, side, xx in (('source', 0, x - separation), ('target', 1, x)):
        row = dict(rows[case.get(f'{role}_type', 0)])
        if f'{role}_hp' in case:
            row['hp'] = case[f'{role}_hp']
        group = mission.groups['Town'][side] if row['kind'] in maps.STRUCTURES else mission.team(side)
        mission.unit(group, row, xx, y)
    if variant.startswith('splash_') or variant in ('friendly', 'neutral'):
        victim = spec['victim_types'].get(variant.removeprefix('splash_'), 0)
        side = 0 if variant == 'friendly' else 2 if variant == 'neutral' else 1
        group = mission.groups['Town'][side] if rows[victim]['kind'] in maps.STRUCTURES else mission.team(side)
        mission.unit(group, rows[victim], x, y + 4)
    if variant in ('structure', 'unit'):
        tid = 14 if variant == 'structure' else 0
        group = mission.groups['Town'][2] if tid == 14 else mission.team(2)
        mission.unit(group, rows[tid], x - separation // 2, y)
    mission.labels = [f"V07 {case['id']}: placement candidate; needs live run.",
                      'See pending.json before interpreting any observations.']
    for ext in maps.EXTENSIONS:
        shutil.copyfile(sources[ext], private / 'Maps' / f"{spec['map']}.{ext}")
    mission.write(private / 'Maps')
    report.write_json(output / 'scenario.json', dict(case, protocol=spec))
    report.write_json(output / 'pending.json', dict(
        status='needs live run', evidence='P', controls=spec['controls'],
        missing=['Audited reference projectile/impact/burn observer (unit log is insufficient).',
                 'Tick-12 single-shot command and tick-13 movement/source-death adapters.',
                 'Exact pixel splash offset sweep and terrain/height fixtures.',
                 'Passive AI, visibility and regeneration controls.',
                 'Variant-specific reload/range/composite/death/corpse controls when requested.'],
        placements=mission.placements))
    # A launch control cannot silently become a measured no-hit result.
    return private


def integer(value):
    if type(value) is not int:
        raise ValueError('trace values must be integers')
    return value


def summarize(path):
    """Validate a full engine trace before deriving any diagnostic samples."""
    rows = [report.json_object(line) for line in path.read_text().splitlines()]
    if len(rows) < 3:
        raise ValueError('incomplete combat trace')
    header, *body = rows
    if (header.get('schema') != 1 or header.get('contract') != 'engine-combat-v1'
            or header.get('origin') != 'engine' or header.get('position_scale') != 256
            or header.get('height_scale') != 1 or header.get('first_tick') != 1):
        raise ValueError('unsupported combat observer contract')
    horizon = integer(header['last_tick'])
    if horizon < 1 or body[-1] != dict(event='end', tick=horizon):
        raise ValueError('truncated horizon or early outcome; no measurements exported')
    tick = 1
    flights = {}
    victims = {}
    fields = {'tick', 'event', 'id', 'source', 'target', 'side', 'kind', 'x', 'y', 'height',
              'altitude', 'victim', 'victim_x', 'victim_y', 'victim_type', 'victim_side',
              'relation', 'before', 'after'}
    for row in body[:-1]:
        if integer(row['tick']) != tick or tick > horizon:
            raise ValueError('missing, unordered or duplicate tick')
        event = row['event']
        if event == 'tick':
            if set(row) != {'tick', 'event', 'objects'} or not isinstance(row['objects'], list):
                raise ValueError('invalid end-of-tick snapshot')
            ids = set()
            for obj in row['objects']:
                if not isinstance(obj, list) or len(obj) != 7:
                    raise ValueError('invalid object snapshot')
                for value in obj:
                    integer(value)
                if obj[0] < 1 or obj[0] in ids or obj[6] not in (0, 1):
                    raise ValueError('invalid object identity/alive flag')
                ids.add(obj[0])
            tick += 1
            continue
        if set(row) != fields or event not in ('launch', 'position', 'impact', 'damage', 'miss'):
            raise ValueError('unknown combat event or missing fields')
        for key in fields - {'event'}:
            integer(row[key])
        identity = row['id']
        if identity < 1:
            raise ValueError('missing projectile identity')
        if event == 'launch':
            if identity in flights:
                raise ValueError('reused projectile identity')
            flights[identity] = dict(launch_tick=tick, impact_tick=None, positions=[], damage=[], misses=[],
                                     identity=[row[k] for k in ('source', 'target', 'side', 'kind')])
        if identity not in flights:
            raise ValueError('event without launch')
        flight = flights[identity]
        if flight['identity'] != [row[k] for k in ('source', 'target', 'side', 'kind')]:
            raise ValueError('projectile identity changes during flight')
        if event in ('launch', 'position', 'impact'):
            if flight['impact_tick'] is not None:
                raise ValueError('flight continues after impact')
            flight['positions'].append([tick, row['x'], row['y'], row['height'], row['altitude']])
        if event == 'impact':
            flight['impact_tick'] = tick
            flight['impact_position'] = [row['x'], row['y']]
        if event in ('damage', 'miss'):
            if flight['impact_tick'] != tick or row['victim'] < 1 or row['after'] > row['before']:
                raise ValueError('invalid impact damage')
            if [row['x'], row['y']] != flight['impact_position']:
                raise ValueError('damage location differs from impact')
            if event == 'miss' and row['before'] != row['after']:
                raise ValueError('miss changes HP')
            sample = dict(event=event, tick=tick, victim=row['victim'], type=row['victim_type'],
                          side=row['victim_side'], relation=row['relation'],
                          distance_squared_fixed=(row['victim_x'] - row['x']) ** 2
                                                 + (row['victim_y'] - row['y']) ** 2,
                          damage=row['before'] - row['after'])
            flight['damage' if event == 'damage' else 'misses'].append(sample)
            if event == 'damage':
                victims.setdefault(row['victim'], []).append([tick, identity, sample['damage']])
    if tick != horizon + 1:
        raise ValueError('missing end-of-tick coverage')
    for flight in flights.values():
        end = flight['impact_tick'] if flight['impact_tick'] is not None else horizon
        observed = {position[0] for position in flight['positions']}
        if observed != set(range(flight['launch_tick'], end + 1)):
            raise ValueError('missing per-tick projectile positions')
    return dict(status='needs live run', evidence='synthetic/engine only', flights=flights,
                victim_damage=victims, burn_duration=None, splash_radius=None, falloff=None,
                friendly_fire=None,
                detail='Samples describe engine behavior. No reference rule or burn duration is inferred.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    sub.add_parser('list')
    prep = sub.add_parser('prepare')
    prep.add_argument('--id', required=True)
    prep.add_argument('--game', type=lambda s: Path(s).resolve(), required=True)
    prep.add_argument('--output', type=Path, required=True)
    analyze = sub.add_parser('summarize')
    analyze.add_argument('--trace', type=Path, required=True)
    analyze.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.mode == 'list':
            for case in scenarios():
                print(case['id'], 'P', 'needs live run')
        elif args.mode == 'prepare':
            selected = [c for c in scenarios() if c['id'] == args.id]
            if not selected:
                raise ValueError('unknown scenario')
            prepare(selected[0], args.game, args.output)
        else:
            result = summarize(args.trace)
            output = report.private_output(args.output)
            output.mkdir(parents=True, exist_ok=False)
            report.write_json(output / 'samples.json', result)
        return 0
    except (OSError, ValueError, KeyError, StopIteration, struct.error) as exc:
        parser.exit(2, f'projectile_measure: {exc}\n')


if __name__ == '__main__':
    sys.exit(main())
