#!/usr/bin/env python3 -I
"""Analyze reference positions with a fixed per-order observation budget.

usage: zpath_analyze.py OUTDIR [--ticks 1490] [--json]
Only type/start matches within two tiles before orders are accepted. Missing units or
truncated observations produce an invalid status and no aggregate success rate.
Arrival means entering the six-tile goal disk, shared with path_bench.
"""
import argparse
import collections
import json
import importlib.util
import math
from pathlib import Path


def analyze(directory, ticks=1490):
    directory = Path(directory)
    course = json.loads((directory / 'course.json').read_text())
    goals = {int(k): v for k, v in course['goals'].items()}
    orders = {int(k): v for k, v in course.get('order_frames', {'0': course['order_frame']}).items()}
    rows = collections.defaultdict(list)
    last_frame = 0
    with (directory / 'pos.txt').open() as stream:
        for line in stream:
            fields = line.split()
            if len(fields) != 10:
                continue
            frame = int(fields[0])
            # Loading the custom mission resets the frame counter; discard the
            # earlier campaign/menu epoch, including objects whose addresses recur.
            if frame < last_frame:
                rows.clear()
            last_frame = frame
            if int(fields[9]):
                rows[fields[1]].append(tuple(map(int, [fields[0], *fields[3:9]])))
    initial = {}
    for obj, samples in rows.items():
        samples.sort()
        fr, typ, x, y, tx, ty, state = samples[0]
        if fr < min(orders.values()):
            initial.setdefault((tx, ty, typ), []).append(obj)
    # Formation initialization can shift spawns by one tile. Bound matching
    # tightly, require every intended unit, and never match distant campaign units.
    candidates = []
    for unit in course['units']:
        ux, uy, group = unit[:3]
        typ = unit[3] if len(unit) > 3 else 0
        options = []
        for (tx, ty, kind), objects in initial.items():
            distance = (ux - tx) ** 2 + (uy - ty) ** 2
            if kind == typ and distance <= 4:
                options.extend((distance, obj) for obj in objects)
        candidates.append(sorted(options))
    assigned = {}
    def assign(index, seen):
        for distance, obj in candidates[index]:
            if obj in seen:
                continue
            seen.add(obj)
            if obj not in assigned or assign(assigned[obj], seen):
                assigned[obj] = index
                return True
        return False
    for index in range(len(candidates)):
        assign(index, set())
    matched = {index: obj for obj, index in assigned.items()}
    result = []
    for index, unit in enumerate(course['units']):
        ux, uy, group = unit[:3]
        typ = unit[3] if len(unit) > 3 else 0
        if index not in matched:
            continue
        obj = matched[index]
        gx, gy = goals[group]
        goal = (gx * 48 + 24, gy * 48 + 24)
        order = orders[group]
        path = 0.0
        idle = max_idle = 0
        arrival = latency = None
        tiles = []
        prev = (ux * 48 + 24, uy * 48 + 24)
        for fr, ty, x, y, tx, ty_, state in rows[obj]:
            if fr < order:
                prev = (x, y)
                continue
            elapsed = fr - order
            if elapsed >= ticks or arrival is not None:
                break
            distance = math.hypot(x - prev[0], y - prev[1])
            path += distance
            if distance < 0.5:
                idle += 1
                max_idle = max(max_idle, idle)
            else:
                idle = 0
                if latency is None:
                    latency = elapsed
            prev = (x, y)
            tile = (tx, ty_)
            if not tiles or tiles[-1] != tile:
                tiles.append(tile)
            if math.hypot(x - goal[0], y - goal[1]) <= 6 * 48:
                arrival = elapsed
        result.append(dict(id=obj, grp=group, type=typ, arrived=arrival,
                           path_px=path, max_idle=max_idle, idle_tail=idle,
                           revisits=len(tiles) - len(set(tiles)), latency=latency,
                           observed_ticks=max(0, last_frame - order + 1)))
    expected = course['n']
    missing = expected - len(result)
    truncated = any(r['arrived'] is None and r['observed_ticks'] < ticks for r in result)
    status = 'missing_units' if missing else 'truncated' if truncated else 'ok'
    arrived = [r for r in result if r['arrived'] is not None]
    summary = dict(course=course['name'], n=expected, fog=course['fog'], ticks=ticks,
                   status=status, matched=len(result), missing=missing,
                   source=str(directory.resolve()))
    if status == 'ok':
        # Baseline is the same full goal-centre octile route used by the generator.
        # Arrival stops earlier, so a ratio below one is expected (see movement rules).
        # Older manifests used a different start for this baseline. Recompute
        # from the declared first unit of each group, as path_bench does.
        spec = importlib.util.spec_from_file_location('make_zpath', Path(__file__).with_name('make_zpath.py'))
        generator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(generator)
        grid = [[int(v) for v in row] for row in course['grid']]
        optimal = {}
        for unit in course['units']:
            group = unit[2]
            if group not in optimal:
                optimal[group] = generator.bfs_len(grid, tuple(unit[:2]), tuple(goals[group]))
        latency = [r['latency'] for r in result if r['latency'] is not None]
        summary.update(arrived=len(arrived), success=len(arrived) / expected,
                       lost=expected - len(arrived),
                       stuck=sum(r['arrived'] is None and r['idle_tail'] >= 120 for r in result),
                       never_started=sum(r['latency'] is None for r in result),
                       makespan=max((r['arrived'] for r in arrived), default=None),
                       median_arrival=sorted(r['arrived'] for r in arrived)[len(arrived) // 2] if arrived else None,
                       mean_path_ratio=sum(r['path_px'] / optimal[r['grp']] for r in arrived) / len(arrived) if arrived else None,
                       mean_time_ratio=sum(r['arrived'] / (optimal[r['grp']] / SPEED[r['type']]) for r in arrived) / len(arrived) if arrived else None,
                       max_idle_ticks=max((r['max_idle'] for r in result), default=0),
                       mean_revisits=sum(r['revisits'] for r in result) / expected,
                       mean_latency=sum(latency) / len(latency) if latency else None,
                       max_latency=max(latency, default=None), ideal_ticks=int(optimal[0] / 9))
    return dict(summary=summary, units=result)


SPEED = {0: 9, 1: 6, 2: 7, 5: 5, 6: 15, 27: 8, 28: 5, 29: 10, 31: 16,
         32: 12, 53: 11, 54: 10, 55: 10, 56: 4, 57: 4, 82: 10}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory')
    parser.add_argument('--ticks', type=int, default=1490)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    if args.ticks <= 0:
        parser.error('--ticks must be positive')
    result = analyze(args.directory, args.ticks)
    print(json.dumps(result if args.json else result['summary']))
    return 0 if result['summary']['status'] == 'ok' else 1


if __name__ == '__main__':
    raise SystemExit(main())
