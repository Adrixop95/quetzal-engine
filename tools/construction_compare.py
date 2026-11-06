#!/usr/bin/env python3
"""Compare audited private construction captures; no live adapters are supplied."""
import argparse
import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location('construction_visual', ROOT / 'tools/parity_visual.py')
visual = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(visual)
from parity_l0 import first_difference

SPEC = ROOT / 'tests/parity/construction.json'
FIELDS = ('site', 'workers', 'money', 'queue', 'selection', 'rally', 'sounds', 'completion')
ALIGNMENT = {'scenario', 'map_sha256', 'controls_sha256', 'normalization_sha256',
             'building_type', 'camera', 'resolution', 'overlay_region', 'first_tick', 'last_tick', 'phase'}
require = visual.require


def integers(value, size=None):
    return isinstance(value, list) and (size is None or len(value) == size) and all(type(v) is int for v in value)


def record(value, fields):
    return isinstance(value, dict) and set(value) == set(fields.split()) and all(type(v) is int for v in value.values())


def validate_state(state):
    require(isinstance(state, dict) and set(state) == set(FIELDS), 'incomplete construction state')
    site = state['site']
    if site is not None:
        require(isinstance(site, dict) and 'footprint' in site, 'invalid site')
        require(record({k: v for k, v in site.items() if k != 'footprint'},
                       'id hp hp_max stage progress sprite overlay_sprite alive x y'), 'invalid site fields')
        require(site['id'] > 0 and site['hp_max'] > 0 and 0 <= site['progress'] <= 1000
                and site['alive'] in (0, 1), 'invalid site values')
        cells = site['footprint']
        require(isinstance(cells, list) and all(integers(c, 2) for c in cells), 'invalid footprint')
        require(cells == [list(c) for c in sorted({tuple(c) for c in cells}, key=lambda c: (c[1], c[0]))],
                'unordered or duplicate footprint')
    workers = state['workers']
    require(isinstance(workers, dict) and all(k and record(v, 'state x y site')
            for k, v in workers.items()), 'invalid workers')
    money = state['money']
    require(isinstance(money, dict) and money and all(k in tuple(map(str, range(8)))
            and type(v) is int for k, v in money.items()), 'invalid money')
    queue = state['queue']
    require(isinstance(queue, list) and all(record(q, 'id type remaining total paid started paused site')
            for q in queue), 'invalid queue')
    require(len({q['id'] for q in queue}) == len(queue) and all(q['id'] > 0 and 0 <= q['type'] < 90
            and 0 <= q['remaining'] <= q['total'] and q['paid'] >= 0 and q['started'] in (0, 1)
            and q['paused'] in (0, 1) for q in queue), 'invalid queue values')
    require(all(integers(state[k]) for k in ('selection', 'sounds', 'completion'))
            and (state['rally'] is None or integers(state['rally'], 2)), 'invalid events or selection/rally')


def capture(path):
    b = visual.load(path)
    require(set(b) == {'schema', 'contract', 'origin', 'alignment', 'samples'}, 'invalid bundle fields')
    require(type(b['schema']) is int and b['schema'] == 1 and b['contract'] == 'construction-v1', 'invalid contract')
    require(b['origin'] in ('reference', 'clone'), 'invalid origin')
    a = b['alignment']
    require(isinstance(a, dict) and set(a) == ALIGNMENT, 'missing alignment controls')
    for key in ('map_sha256', 'controls_sha256', 'normalization_sha256'):
        value = a[key]
        require(isinstance(value, str) and len(value) == 64 and all(c in '0123456789abcdef' for c in value), 'invalid digest')
    require(isinstance(a['scenario'], str) and a['scenario'], 'missing scenario')
    require(type(a['building_type']) is int and a['building_type'] in
            [t for types in visual.load(SPEC)['classes'].values() for t in types], 'invalid structure type')
    require(a['phase'] == 'before_commands', 'invalid observation phase')
    require(isinstance(a['camera'], list) and len(a['camera']) == 3
            and all(type(v) in (int, float) for v in a['camera']) and a['camera'][2] > 0, 'invalid camera')
    resolution = a['resolution']
    require(isinstance(resolution, list) and len(resolution) == 2
            and all(type(v) is int and 0 < v <= 4096 for v in resolution), 'invalid resolution')
    region = a['overlay_region']
    require(isinstance(region, list) and len(region) == 4 and all(type(v) is int for v in region), 'invalid region')
    x, y, w, h = region
    require(min(x, y) >= 0 and min(w, h) > 0 and x + w <= resolution[0] and y + h <= resolution[1], 'region outside frame')
    first, last = a['first_tick'], a['last_tick']
    require(type(first) is int and type(last) is int and 1 <= first <= last <= first + 100000, 'invalid horizon')
    require(isinstance(b['samples'], list) and len(b['samples']) == last - first + 1, 'incomplete ticks')
    pixels = []
    for tick, row in enumerate(b['samples'], first):
        require(isinstance(row, dict) and set(row) == {'tick', 'state', 'overlay', 'unavailable'}, 'invalid sample fields')
        require(type(row['tick']) is int and row['tick'] == tick, 'missing or unordered tick')
        require(row['unavailable'] == [], 'unavailable observations block comparison')
        validate_state(row['state'])
        name = row['overlay']
        require(isinstance(name, str) and name and not Path(name).is_absolute(), 'invalid frame path')
        iw, ih, channels, data = visual.asset(path, name)
        require([iw, ih] == resolution and channels == 3, 'invalid frame resolution')
        pixels.append(b''.join(data[(yy * iw + x) * 3:(yy * iw + x + w) * 3] for yy in range(y, y + h)))
    return b, pixels


def difference(a, b):
    left, lp = a
    right, rp = b
    for lrow, rrow, lframe, rframe in zip(left['samples'], right['samples'], lp, rp):
        delta = first_difference(lrow['state'], rrow['state'])
        if delta:
            field, expected, actual = delta
            return dict(tick=lrow['tick'], field=field, reference=expected, actual=actual)
        bad = sum(lframe[i:i + 3] != rframe[i:i + 3] for i in range(0, len(lframe), 3))
        if bad:
            return dict(tick=lrow['tick'], field='overlay', different_pixels=bad, pixels=len(lframe) // 3)
    return None


def compare(reference, repeat, clone):
    a, r, b = (capture(path) for path in (reference, repeat, clone))
    require([v[0]['origin'] for v in (a, r, b)] == ['reference', 'reference', 'clone'], 'invalid capture origins')
    require(all(visual.identical(a[0]['alignment'], v[0]['alignment']) for v in (r, b)), 'unaligned captures')
    unstable = difference(a, r)
    delta = unstable or difference(a, b)
    return dict(status='reference nondeterminism' if unstable else 'different' if delta else 'equal',
                first=delta, ticks=len(a[1]), evidence='Comparison only; setup and observer audit required.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--repeat', type=Path, required=True)
    parser.add_argument('--clone', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        output = visual.private_output(args.output)
        require(not output.exists(), 'output already exists')
        result = compare(args.reference, args.repeat, args.clone)
        output.parent.mkdir(parents=True, exist_ok=True)
        visual.write_json(output, result)
        return int(result['status'] != 'equal')
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.exit(2, f'construction_compare: {exc}\n')


if __name__ == '__main__':
    sys.exit(main())
