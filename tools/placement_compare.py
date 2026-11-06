#!/usr/bin/env python3
"""Expand placement input candidates and compare complete, aligned private captures."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / 'tests/parity/placement.json'
module = importlib.util.spec_from_file_location('placement_visual', ROOT / 'tools/parity_visual.py')
visual = importlib.util.module_from_spec(module)
module.loader.exec_module(visual)
require = visual.require


def integer(value):
    return type(value) is int


def pair(value):
    return isinstance(value, list) and len(value) == 2 and all(integer(v) for v in value)


def plan():
    spec = visual.load(SPEC)
    return [dict(id=f"placement-{tid}-{variant}", building_class=kind, building_type=tid,
                 variant=variant, map=spec['map'], evidence='P', input=spec['input'],
                 controls=spec['controls'], blockers=spec['blockers'])
            for kind, types in spec['classes'].items() for tid in types
            for variant in spec['variants']]


ALIGNMENT = {'scenario', 'map_sha256', 'controls_sha256', 'building_type', 'camera',
             'resolution', 'overlay_region', 'first_tick', 'last_tick', 'phase'}
FIELDS = ('active', 'valid', 'snapped', 'footprint', 'feedback', 'rotation',
          'ghost_sprite', 'cursor', 'accepted')


def capture(path):
    """Validate every tick and raster before reporting even an early difference."""
    bundle = visual.load(path)
    require(set(bundle) == {'schema', 'contract', 'origin', 'alignment', 'samples'}, 'invalid bundle fields')
    require(type(bundle['schema']) is int and bundle['schema'] == 1
            and bundle['contract'] == 'placement-v1', 'unsupported placement contract')
    require(bundle['origin'] in ('reference', 'clone'), 'invalid capture origin')
    meta = bundle['alignment']
    require(isinstance(meta, dict) and set(meta) == ALIGNMENT, 'missing alignment controls')
    for field in ('map_sha256', 'controls_sha256'):
        value = meta[field]
        require(isinstance(value, str) and len(value) == 64 and all(c in '0123456789abcdef' for c in value),
                'missing map or controls digest')
    require(isinstance(meta['scenario'], str) and meta['scenario'], 'missing scenario')
    require(integer(meta['building_type']) and 0 <= meta['building_type'] < 90, 'invalid building type')
    require(meta['phase'] == 'before_input', 'placement samples must precede input at the declared tick')
    require(isinstance(meta['camera'], list) and len(meta['camera']) == 3
            and all(type(v) in (int, float) for v in meta['camera']) and meta['camera'][2] > 0, 'invalid camera')
    require(pair(meta['resolution']) and all(0 < v <= 4096 for v in meta['resolution']), 'invalid resolution')
    region = meta['overlay_region']
    require(isinstance(region, list) and len(region) == 4 and all(integer(v) for v in region), 'invalid overlay region')
    x, y, w, h = region
    require(min(x, y) >= 0 and min(w, h) > 0 and x + w <= meta['resolution'][0]
            and y + h <= meta['resolution'][1], 'overlay region outside frame')
    first, last = meta['first_tick'], meta['last_tick']
    require(integer(first) and integer(last) and 1 <= first <= last <= first + 100000, 'invalid horizon')
    samples = bundle['samples']
    require(isinstance(samples, list) and len(samples) == last - first + 1, 'incomplete tick coverage')
    pixels = []
    for tick, row in enumerate(samples, first):
        require(isinstance(row, dict) and set(row) == {'tick', 'overlay', *FIELDS}, 'invalid sample fields')
        require(integer(row['tick']) and row['tick'] == tick, 'unordered, duplicate or missing tick')
        require(type(row['active']) is bool and type(row['accepted']) is bool, 'invalid placement flags')
        require(isinstance(row['feedback'], str) and isinstance(row['cursor'], str) and row['cursor'], 'missing UI feedback')
        require(row['rotation'] is None or integer(row['rotation']), 'invalid rotation')
        require(row['ghost_sprite'] is None or (isinstance(row['ghost_sprite'], str) and row['ghost_sprite']), 'invalid ghost identity')
        footprint = row['footprint']
        require(isinstance(footprint, list) and len(footprint) <= 64, 'invalid footprint')
        coords = []
        for cell in footprint:
            require(isinstance(cell, list) and len(cell) == 3 and pair(cell[:2]) and type(cell[2]) is bool,
                    'footprint requires x, y, validity')
            coords.append(tuple(cell[:2]))
        require(coords == sorted(set(coords), key=lambda p: (p[1], p[0])), 'duplicate or unordered footprint cells')
        if row['active']:
            require(pair(row['snapped']) and type(row['valid']) is bool and footprint, 'missing active decision')
            require(row['valid'] == all(c[2] for c in footprint), 'decision disagrees with footprint')
        else:
            require(row['valid'] is None and row['snapped'] is None and footprint == []
                    and row['ghost_sprite'] is None and row['rotation'] is None,
                    'inactive placement retains a decision')
        name = row['overlay']
        require(isinstance(name, str) and name and not Path(name).is_absolute(), 'overlay must be a relative capture path')
        image = (path.parent / name).resolve()
        require(image.is_relative_to(path.parent.resolve()), 'overlay escapes capture directory')
        iw, ih, channels, data = visual.image_file(image)
        require([iw, ih] == meta['resolution'] and channels == 3, 'frame resolution or format differs')
        pixels.append(b''.join(data[((yy * iw + x) * 3):((yy * iw + x + w) * 3)] for yy in range(y, y + h)))
    return bundle, pixels


def compare(reference, clone):
    a, ap = capture(reference)
    b, bp = capture(clone)
    require(a['origin'] == 'reference' and b['origin'] == 'clone', 'capture origins reversed or duplicated')
    require(visual.identical(a['alignment'], b['alignment']), 'unaligned captures')
    differences = []
    for left, right, lp, rp in zip(a['samples'], b['samples'], ap, bp):
        for field in FIELDS:
            if not visual.identical(left[field], right[field]):
                differences.append(dict(tick=left['tick'], field=field, reference=left[field], clone=right[field]))
        bad = sum(lp[i:i + 3] != rp[i:i + 3] for i in range(0, len(lp), 3))
        if bad:
            differences.append(dict(tick=left['tick'], field='overlay', different_pixels=bad,
                                    pixels=len(lp) // 3))
    return dict(status='different' if differences else 'equal', first=next(iter(differences), None),
                differences=differences, ticks=len(ap),
                evidence='Comparison only; live adapter and alignment audit remain required.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    sub.add_parser('list')
    prepare = sub.add_parser('prepare')
    prepare.add_argument('--output', type=Path, required=True)
    diff = sub.add_parser('compare')
    diff.add_argument('--reference', type=Path, required=True)
    diff.add_argument('--clone', type=Path, required=True)
    diff.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.mode == 'list':
            for case in plan():
                print(case['id'], case['building_class'], 'P needs live run')
            return 0
        output = visual.private_output(args.output)
        if args.mode == 'prepare':
            output.mkdir(parents=True, exist_ok=False)
            visual.write_json(output / 'candidates.json', plan())
            visual.write_json(output / 'status.json', dict(status='needs live run', dependency='64-shared-runner-observer',
                              detail='Candidate screen inputs only; audited setup and reference observer are absent.'))
            return 0
        result = compare(args.reference, args.clone)
        output.parent.mkdir(parents=True, exist_ok=True)
        visual.write_json(output, result)
        return int(result['status'] != 'equal')
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.exit(2, f'placement_compare: {exc}\n')


if __name__ == '__main__':
    sys.exit(main())
