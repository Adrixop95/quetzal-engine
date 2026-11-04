#!/usr/bin/env python3
"""Region-based UI diagnostics for ui_capture.py phases; outputs remain private.

Pixels identify differences, not their semantic cause. Cause classes are candidates.
A successful image comparison alone never establishes L5 interaction parity.
"""
import argparse
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ui_capture import CATALOGUE, png_bytes, validate
from parity_visual import image_file, load, require
from parity_report import ROOT, private_output, write_json

KINDS = {'static_art', 'text', 'dynamic_world', 'animated'}
CAUSES = ['wrong sprite or frame', 'wrong palette', 'wrong offset', 'wrong font or glyph',
          'wrong text', 'wrong layout or scale', 'missing element', 'extra element']


def rectangle(rect, width, height):
    require(isinstance(rect, list) and len(rect) == 4 and all(type(n) is int for n in rect),
            'rectangle must be [x, y, width, height]')
    x, y, w, h = rect
    require(x >= 0 and y >= 0 and w > 0 and h > 0 and x + w <= width and y + h <= height,
            'rectangle outside frame')
    return [(y + dy) * width + x + dx for dy in range(h) for dx in range(w)]


def specification(spec, width, height):
    require(isinstance(spec, dict) and spec.get('resolution') == [width, height],
            'region specification resolution differs')
    align = spec.get('alignment', {})
    radius = align.get('max_shift')
    require(type(radius) is int and 0 <= radius <= 16, 'max_shift must be 0..16')
    anchors = align.get('anchors')
    require(isinstance(anchors, list) and anchors, 'explicit alignment anchors required')
    anchor_pixels = sorted({p for rect in anchors for p in rectangle(rect, width, height)})
    rows = spec.get('regions')
    require(isinstance(rows, list) and rows, 'regions required')
    covered, names, regions = set(), set(), []
    for row in rows:
        require(isinstance(row.get('name'), str) and row['name'] and row['name'] not in names,
                'unique region names required')
        names.add(row['name'])
        require(row.get('kind') in KINDS, 'invalid region kind')
        points = rectangle(row.get('rect'), width, height)
        require(not covered.intersection(points), 'regions overlap')
        covered.update(points)
        if row['kind'] in ('dynamic_world', 'animated'):
            tolerance = row.get('tolerance', {})
            require(set(tolerance) == {'channel', 'max_bad_fraction', 'max_mae'},
                    'dynamic regions require all tolerance fields')
            for key, limit in [('channel', 255), ('max_bad_fraction', 1), ('max_mae', 255)]:
                value = tolerance[key]
                require(type(value) in (float, int) and math.isfinite(value) and 0 <= value <= limit,
                        'invalid tolerance')
            require(type(tolerance['channel']) is int, 'channel tolerance must be integer')
        else:
            require('tolerance' not in row, 'static and text regions must be pixel exact')
        regions.append((row, points))
    require(len(covered) == width * height, 'regions must partition the whole frame')
    static = {p for row, points in regions if row['kind'] in ('static_art', 'text') for p in points}
    require(set(anchor_pixels) <= static, 'alignment anchors must lie in static regions')
    return radius, anchor_pixels, regions


def sample(data, width, height, point, dx=0, dy=0):
    x, y = point % width + dx, point // width + dy
    return data[3 * (y * width + x):3 * (y * width + x) + 3] if 0 <= x < width and 0 <= y < height else None


def align_frames(a, b, width, height, radius, anchors):
    """Find a unique exact integer translation on declared anchors, never rescale."""
    matches = []
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            if all(sample(a, width, height, p) == sample(b, width, height, p, dx, dy) for p in anchors):
                matches.append((dx, dy))
    if len(matches) != 1:
        return None
    return matches[0]


def rectangles(points, width):
    """Four-connected mismatch components, bounded to 256 report rectangles."""
    pending, result = set(points), []
    while pending:
        if len(result) == 255:
            xs, ys = [p % width for p in pending], [p // width for p in pending]
            result.append(dict(rect=[min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1],
                               different_pixels=len(pending), merged=True))
            break
        seed = pending.pop()
        stack, component = [seed], [seed]
        while stack:
            p = stack.pop()
            adjacent = [p - width, p + width]
            if p % width: adjacent.append(p - 1)
            if p % width < width - 1: adjacent.append(p + 1)
            for q in adjacent:
                if q in pending:
                    pending.remove(q)
                    stack.append(q)
                    component.append(q)
        xs, ys = [p % width for p in component], [p // width for p in component]
        result.append(dict(rect=[min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1],
                           different_pixels=len(component), merged=False))
    return sorted(result, key=lambda r: (r['rect'][1], r['rect'][0]))


def candidates(kind, pairs, shifted):
    if shifted:
        return ['wrong offset', 'wrong layout or scale']
    forward, reverse = {}, {}
    palette = True
    for a, b in pairs:
        if b is None or a in forward and forward[a] != b or b in reverse and reverse[b] != a:
            palette = False
            break
        forward[a], reverse[b] = b, a
    result = ['wrong font or glyph', 'wrong text'] if kind == 'text' else ['wrong sprite or frame']
    if palette: result.append('wrong palette')
    return result + ['wrong offset', 'wrong layout or scale', 'missing element', 'extra element']


def compare(reference, clone, spec):
    require(reference[:3] == clone[:3] and reference[2] == 3, 'expected equal RGB dimensions')
    width, height = reference[:2]
    radius, anchors, regions = specification(spec, width, height)
    a, b = reference[3], clone[3]
    shift = align_frames(a, b, width, height, radius, anchors)
    # Retain unaligned diagnostics when anchors fail; never accept them as a pass.
    dx, dy = shift or (0, 0)
    heat = bytearray(width * height * 3)
    results = []
    for row, points in regions:
        pairs = [(sample(a, width, height, p), sample(b, width, height, p, dx, dy)) for p in points]
        errors = [tuple(abs(x - y) for x, y in zip(u, v)) if v is not None else (255, 255, 255)
                  for u, v in pairs]
        changed = [p for p, (u, v) in zip(points, pairs) if u != v]
        tolerance = row.get('tolerance', dict(channel=0, max_bad_fraction=0, max_mae=0))
        bad = sum(max(e) > tolerance['channel'] for e in errors)
        mae = sum(sum(e) for e in errors) / (3 * len(points))
        clipped = sum(v is None for _, v in pairs)
        accepted = (not clipped and bad / len(points) <= tolerance['max_bad_fraction']
                    and mae <= tolerance['max_mae'])
        classes = candidates(row['kind'], pairs, shift not in (None, (0, 0)))
        boxes = rectangles(changed, width)
        for box in boxes:
            box['cause_candidates'] = classes
        for p in changed:
            heat[p * 3:p * 3 + 3] = bytes((255, 0, 255) if row['kind'] in ('static_art', 'text') else (255, 160, 0))
        results.append(dict(name=row['name'], kind=row['kind'], pixels=len(points), different_pixels=len(changed),
                            bad_pixels=bad, clipped_pixels=clipped, mae=mae, accepted=accepted, rectangles=boxes))
    return dict(status='pixel_pass' if shift == (0, 0) and all(r['accepted'] for r in results) else 'pixel_fail',
                alignment_shift=list(shift) if shift is not None else None,
                alignment_status='unique_exact' if shift is not None else 'missing_or_ambiguous_anchor',
                cause_evidence='candidates_only', l5='unmeasured', regions=results), png_bytes(width, height, heat)


def run(args):
    rows = validate(load(args.spec))
    if args.screen:
        require(set(args.screen) <= {row['id'] for row in rows}, 'unknown screen ID')
        rows = [row for row in rows if row['id'] in args.screen]
    root, output = private_output(args.captures), private_output(args.output)
    require(output.is_relative_to((ROOT / 'work/parity/ui').resolve()),
            'UI diff output must be inside work/parity/ui')
    require(not output.exists(), 'use a fresh diff output directory')
    output.mkdir(parents=True)
    reports = []
    for row in rows:
        result = dict(id=row['id'], phase=args.phase, status='unmeasured', regions=None)
        try:
            phase = root / row['id'] / args.phase
            report = load(phase / 'report.json')
            require(report.get('id') == row['id'] and report.get('phase') == args.phase, 'capture identity differs')
            require(all(report['runners'][r]['status'] == 'repeatable_capture' for r in ('reference', 'clone')),
                    'both runners must have repeatable captures')
            spec = row.get('comparison', {})
            require('regions' in spec, spec.get('blocked', 'screen region specification unavailable'))
            measured, heat = compare(image_file(phase / 'reference.png'), image_file(phase / 'clone.png'), spec)
            result.update(measured)
            (output / (row['id'] + '-diff.png')).write_bytes(heat)
        except (ValueError, OSError, KeyError, TypeError) as exc:
            result['reason'] = str(exc)
        reports.append(result)
    write_json(output / 'report.json', dict(schema=1, screens=reports))
    print(f'{len(reports)} screens: ' + ', '.join(f'{s}={sum(r["status"] == s for r in reports)}'
                                               for s in ('pixel_pass', 'pixel_fail', 'unmeasured')))
    return int(any(r['status'] != 'pixel_pass' for r in reports))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--spec', type=Path, default=CATALOGUE)
    parser.add_argument('--screen', action='append')
    parser.add_argument('--phase', choices=('before', 'after'), required=True)
    parser.add_argument('--captures', type=Path, default=ROOT / 'work/parity/ui')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        return run(args)
    except (ValueError, OSError, TypeError, KeyError) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
