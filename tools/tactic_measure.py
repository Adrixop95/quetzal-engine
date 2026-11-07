#!/usr/bin/env python3
"""Plan V06/V10 probes and compare complete, audited per-tick captures."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('parity_report', ROOT / 'tools/parity_report.py')
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)
OBJECT = {'type', 'side', 'hp', 'hp_max', 'alive', 'x', 'y', 'dest_x', 'dest_y',
          'order', 'target', 'tactic', 'follow', 'mode', 'resume', 'cooldown', 'visible'}
SIDE = {'money', 'units', 'structures', 'selected_tactic', 'selected', 'groups', 'queue'}
ALIGNMENT = {'scenario', 'map_sha256', 'controls_sha256', 'normalization_sha256',
             'packets_sha256', 'settings_sha256', 'first_tick', 'last_tick', 'phase', 'viewer'}


def integer(value, minimum=None):
    if type(value) is not int or (minimum is not None and value < minimum):
        raise ValueError('invalid integer')
    return value


def exact(value, keys):
    if not isinstance(value, dict) or set(value) != keys:
        raise ValueError('missing or unknown fields')


def identities(value):
    if not isinstance(value, list) or any(type(i) is not int or i < 1 for i in value) or len(set(value)) != len(value):
        raise ValueError('identities must be unique positive integers in observed order')


def sample(row, tick):
    exact(row, {'tick', 'objects', 'sides'})
    if integer(row['tick']) != tick or not isinstance(row['objects'], dict):
        raise ValueError('missing, duplicate or unordered tick')
    sides = row['sides']
    if not isinstance(sides, list) or not 1 <= len(sides) <= 8:
        raise ValueError('missing side observations')
    for identity, obj in row['objects'].items():
        if not re.fullmatch(r'[1-9][0-9]*', identity):
            raise ValueError('invalid canonical object identity')
        exact(obj, OBJECT)
        for key in OBJECT - {'visible'}:
            integer(obj[key])
        if (obj['alive'] not in (0, 1) or obj['mode'] not in range(4)
                or obj['target'] < 0 or obj['type'] < 0 or obj['hp_max'] < 0):
            raise ValueError('invalid object state')
        if (not isinstance(obj['visible'], list) or len(obj['visible']) != len(sides)
                or any(type(v) is not int or v not in (0, 1) for v in obj['visible'])):
            raise ValueError('visibility must be observed for every side')
    for side in sides:
        exact(side, SIDE)
        for key in ('money', 'units', 'structures', 'selected_tactic'):
            integer(side[key])
        if min(side['units'], side['structures']) < 0:
            raise ValueError('negative population count')
        identities(side['selected'])
        if not isinstance(side['groups'], list) or len(side['groups']) != 10:
            raise ValueError('observe all ten control groups')
        for group in side['groups']:
            identities(group)
        if not isinstance(side['queue'], list):
            raise ValueError('missing production queue')
        for item in side['queue']:
            if not isinstance(item, list) or len(item) != 8:
                raise ValueError('invalid queue row')
            for value in item:
                integer(value)
            if item[5] not in (0, 1) or item[6] not in (0, 1):
                raise ValueError('invalid queue flags')


def capture(path, origin):
    data = report.json_object(path.read_text())
    exact(data, {'schema', 'contract', 'origin', 'alignment', 'samples', 'unavailable'})
    if (type(data['schema']) is not int or data['schema'] != 1 or data['contract'] != 'tactics-v1'
            or data['origin'] != origin or data['unavailable'] != []):
        raise ValueError('requires a complete audited tactics-v1 capture')
    alignment = data['alignment']
    exact(alignment, ALIGNMENT)
    if (not isinstance(alignment['scenario'], str) or not alignment['scenario']
            or alignment['phase'] != 'before_commands'
            or integer(alignment['viewer'], 0) > 7):
        raise ValueError('invalid alignment')
    for key in ALIGNMENT:
        if key.endswith('_sha256') and (not isinstance(alignment[key], str)
                                       or not re.fullmatch('[0-9a-f]{64}', alignment[key])):
            raise ValueError('missing setup, packet, settings or normalization digest')
    start = integer(alignment['first_tick'], 1)
    stop = integer(alignment['last_tick'], start)
    if not isinstance(data['samples'], list) or len(data['samples']) != stop - start + 1:
        raise ValueError('incomplete per-tick coverage')
    side_count = None
    for tick, row in enumerate(data['samples'], start):
        sample(row, tick)
        if side_count is None:
            side_count = len(row['sides'])
        if len(row['sides']) != side_count or alignment['viewer'] >= side_count:
            raise ValueError('side coverage changes or viewer is absent')
    return data


def first_difference(a, b, path=''):
    if type(a) is not type(b):
        return dict(field=path, reference=a, clone=b)
    if isinstance(a, dict):
        if set(a) != set(b):
            return dict(field=path + '.keys', reference=sorted(a), clone=sorted(b))
        for key in sorted(a):
            diff = first_difference(a[key], b[key], f'{path}.{key}')
            if diff:
                return diff
    elif isinstance(a, list):
        if len(a) != len(b):
            return dict(field=path + '.length', reference=len(a), clone=len(b))
        for index, (x, y) in enumerate(zip(a, b)):
            diff = first_difference(x, y, f'{path}[{index}]')
            if diff:
                return diff
    elif a != b:
        return dict(field=path, reference=a, clone=b)
    return None


def compare(paths):
    resolved = [p.resolve() for p in paths]
    if len(set(resolved)) != 3:
        raise ValueError('two independent reference captures and a clone capture are required')
    if any(not p.is_file() for p in resolved):
        return dict(status='needs live run', reason='missing audited captures'), 77
    a, b, c = [capture(p, origin) for p, origin in zip(resolved, ('reference', 'reference', 'clone'))]
    if a['alignment'] != b['alignment'] or a['alignment'] != c['alignment']:
        raise ValueError('capture alignment differs')
    # Validate every sample before reporting even the earliest divergence.
    for row, repeated in zip(a['samples'], b['samples']):
        diff = first_difference(row, repeated)
        if diff:
            return dict(status='needs live run', reason='reference repeats differ', tick=row['tick'], difference=diff), 77
    for row, cloned in zip(a['samples'], c['samples']):
        diff = first_difference(row, cloned)
        if diff:
            return dict(status='difference', tick=row['tick'], difference=diff,
                        evidence='repeated observation; investigate cause before a rule correction'), 1
    return dict(status='matches observed contract', ticks=len(a['samples']),
                evidence='bounded per-tick comparison; no bonus, cap or scan law inferred'), 0


def plan():
    catalogue = report.json_object((ROOT / 'tests/parity/tactics.json').read_text())
    return [dict(case, id=f"{case['id']}-mode-{mode}", mode=mode)
            for case in catalogue['cases'] for mode in catalogue['modes']]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('plan')
    check = sub.add_parser('compare')
    check.add_argument('--reference', type=Path, nargs=2, required=True)
    check.add_argument('--clone', type=Path, required=True)
    check.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'plan':
            print(json.dumps(dict(status='needs live run', evidence='P', cases=plan()), indent=2))
            return 0
        output = report.private_output(args.output)
        if output.exists():
            raise ValueError('output already exists; retain earlier evidence')
        result, code = compare([*args.reference, args.clone])
        output.mkdir(parents=True, exist_ok=False)
        report.write_json(output / 'diff.json', result)
        print(json.dumps(result))
        return code
    except (ValueError, OSError, KeyError, TypeError) as exc:
        print(f'tactic measurement: {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
