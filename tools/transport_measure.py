#!/usr/bin/env python3
"""Compare private, manually normalized transport observations; never run the game."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


# Factual source locations from the campaign inventory, never script bodies.
SITES = [
    (2, 0, 5, 'BoatActivate'), (2, 0, 6, 'BoatActivate'),
    (2, 101, 35, 'SubmarineGo'), (2, 3, 64, 'BoatActivate'),
    *((7, 2, line, 'BoatActivate') for line in range(30, 35)),
    (9, 1, 18, 'SubmarineGo'),
    *((9, 6, line, 'BoatActivate') for line in range(70, 74)),
    (15, 1, 18, 'SubmarineGo'),
    *((15, trigger, line, 'JohnkaEnable') for trigger, line in
      ((3, 43), (3, 44), (4, 56), (4, 57))),
]
CLASSES = ('activation', 'boarding', 'unloading', 'route', 'arrival',
           'speed', 'turning', 'shore', 'stuck', 'completion')


def site_id(site):
    mission, trigger, line, kind = site
    return f'{mission}:{trigger}:{line}:{kind}'


def nonempty(value):
    return isinstance(value, str) and bool(value.strip())


def digest(value):
    return isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value)


def load(path):
    """Validate provenance and capture integrity, not the observer's interpretation."""
    def invalid_constant(value):
        raise ValueError(f'nonfinite JSON number: {value}')

    def unique_keys(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f'duplicate JSON key: {key}')
            result[key] = value
        return result

    data = json.loads(path.read_text(), parse_constant=invalid_constant, object_pairs_hook=unique_keys)
    # Very large JSON exponents can overflow during parsing without parse_constant.
    json.dumps(data, allow_nan=False)
    if not isinstance(data, dict) or type(data.get('schema')) is not int or data['schema'] != 1:
        raise ValueError('expected transport observation schema 1')
    if data.get('source') not in ('reference', 'engine'):
        raise ValueError('source must be reference or engine')
    for field in ('run_id', 'case', 'observer', 'alignment'):
        if not nonempty(data.get(field)):
            raise ValueError(f'missing {field}')
    if data.get('site') not in {site_id(site) for site in SITES}:
        raise ValueError('unknown campaign site')
    for field in ('binary_sha256', 'input_sha256'):
        if not digest(data.get(field)):
            raise ValueError(f'invalid {field}')
    artifacts = data.get('artifacts')
    if not isinstance(artifacts, list) or not artifacts:
        raise ValueError('capture artifacts required')
    paths = set()
    for artifact in artifacts:
        if not isinstance(artifact, dict) or not nonempty(artifact.get('path')) or not digest(artifact.get('sha256')):
            raise ValueError('invalid capture artifact')
        capture = (path.parent / artifact['path']).resolve()
        if not capture.is_file() or capture.stat().st_size == 0:
            raise ValueError('capture is absent or empty')
        if hashlib.sha256(capture.read_bytes()).hexdigest() != artifact['sha256']:
            raise ValueError('capture hash mismatch')
        paths.add(capture)
    observations = data.get('observations')
    if not isinstance(observations, dict) or not observations or observations.keys() - set(CLASSES):
        raise ValueError('unknown or absent observation classes')
    for name, rows in observations.items():
        if not isinstance(rows, list) or not rows:
            raise ValueError(f'{name}: nonempty samples required')
        previous = -1
        for row in rows:
            if not isinstance(row, dict) or set(row) != {'tick', 'value'}:
                raise ValueError(f'{name}: expected tick and value')
            tick = row['tick']
            if type(tick) is not int or tick < 0 or tick < previous:
                raise ValueError(f'{name}: ticks must be nonnegative and ordered')
            if not isinstance(row['value'], dict) or not row['value']:
                raise ValueError(f'{name}: nonempty value object required')
            previous = tick
    return data, paths


def equal_samples(left, right):
    # Python considers True == 1; observation values must retain their JSON types.
    return json.dumps(left, sort_keys=True) == json.dumps(right, sort_keys=True)


def compare(first, second, engine, classes):
    a, paths_a = load(first)
    b, paths_b = load(second)
    if a['source'] != 'reference' or b['source'] != 'reference':
        raise ValueError('two reference captures required')
    if a['run_id'] == b['run_id'] or paths_a & paths_b:
        raise ValueError('reference repetitions must have distinct run IDs and capture files')
    for field in ('case', 'site', 'binary_sha256', 'input_sha256', 'observer', 'alignment'):
        if a[field] != b[field]:
            raise ValueError(f'reference provenance mismatch: {field}')
    result = {'status': 'needs live run', 'site': a['site'], 'case': a['case'],
              'classes': {}, 'scope': 'Recorded samples only; no site closure or parity claim.'}
    c = None
    if engine:
        c, _ = load(engine)
        if c['source'] != 'engine' or any(c[f] != a[f] for f in ('case', 'site', 'input_sha256', 'alignment')):
            raise ValueError('engine case/site/input/alignment mismatch')
    for name in classes:
        left, right = a['observations'].get(name), b['observations'].get(name)
        if left is None or right is None:
            status = 'missing reference samples'
        elif not equal_samples(left, right):
            status = 'reference not repeatable'
        elif c is None:
            status = 'repeatable reference samples'
        elif name not in c['observations']:
            status = 'missing engine samples'
        elif equal_samples(left, c['observations'][name]):
            status = 'sample match'
        else:
            status = 'confirmed sample divergence'
        result['classes'][name] = status
    statuses = set(result['classes'].values())
    if statuses == {'sample match'}:
        result['status'] = 'sample match'
    elif statuses <= {'sample match', 'confirmed sample divergence'}:
        result['status'] = 'confirmed sample divergence'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('plan', help='list all 19 pending sites and measurement classes')
    check = commands.add_parser('compare')
    check.add_argument('--reference', type=Path, required=True)
    check.add_argument('--repeat', type=Path, required=True)
    check.add_argument('--engine', type=Path)
    check.add_argument('--class', dest='classes', choices=CLASSES, action='append')
    args = parser.parse_args()
    try:
        if args.command == 'plan':
            result = {'status': 'needs live run', 'classes': CLASSES,
                      'sites': [{'id': site_id(site), 'status': 'needs live run'} for site in SITES]}
        else:
            result = compare(args.reference, args.repeat, args.engine, args.classes or CLASSES)
        print(json.dumps(result, indent=2, allow_nan=False))
        return 0 if args.command == 'plan' or result['status'] == 'sample match' else 1
    except (OSError, ValueError, TypeError) as error:
        print(f'transport_measure: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
