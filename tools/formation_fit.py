#!/usr/bin/env python3
"""Compare a private formation dataset with the engine's movement-only candidate.

The probe executes the production MoveSystem, not a Python approximation. Input
measurements are never used as orders, speeds, destinations or expected routes.
Only initial tiles, anchors, types and obstacles enter the candidate simulation.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess


def compare(dataset, maps, probe, rules, details=False):
    reports = {}
    ticks = dataset['ticks']
    if not ticks or ticks[0] != 1 or ticks != sorted(set(ticks)):
        raise ValueError('ticks must be increasing and begin at one')
    for name, mission in dataset['missions'].items():
        text = (maps / (name + '.mp')).read_text(encoding='latin1')
        size = re.search(r'MapSize\s*=\s*(\d+)\s*,\s*(\d+)', text, re.I)
        if not size:
            raise ValueError(f'{name}: missing MapSize')
        w, h = map(int, size.groups())
        terrain = (maps / (name + '.dat')).read_bytes()
        if len(terrain) != w * h:
            raise ValueError(f'{name}: invalid terrain size')
        # Placement order is read from the mission, never from guest handles.
        placement = {}
        for match in re.finditer(r'(?im)^Unit\d+\s*=\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(\d+)', text):
            x, y, kind = map(int, match.groups())
            placement.setdefault((kind, x, y), len(placement))
        members = []
        records = []
        for group_id, group in enumerate(mission['groups']):
            ax, ay = group['anchor_tile']
            ordered = sorted(group['members'], key=lambda m: placement.get(
                (m['type'], *m['tile_per_tick'][0]), len(placement)))
            for member in ordered:
                if len(member['tile_per_tick']) != len(ticks):
                    raise ValueError(f'{name}: incomplete member timeline')
                x, y = member['tile_per_tick'][0]
                members.append(member)
                records.append((member['type'], x, y, group_id, ax, ay))
        for obstacle in mission['other_objects_tile_at_tick1']:
            records.append((obstacle['type'], *obstacle['tile'], -1, 0, 0))
        lines = [f'{w} {h} {len(records)} {max(ticks)}', ' '.join(map(str, terrain))]
        lines.extend(' '.join(map(str, record)) for record in records)
        result = subprocess.run([str(probe), str(rules)], input='\n'.join(lines) + '\n',
                                text=True, capture_output=True, check=True)
        actual = {}
        for line in result.stdout.splitlines():
            tick, index, x, y = map(int, line.split())
            actual[tick, index] = [x, y]
        walkers = [(i, m) for i, m in enumerate(members) if m['state_at_tick1'] == 2]
        before = after = before_ticks = after_ticks = 0
        per_tick = {str(t): 0 for t in ticks}
        residuals = []
        timing_residuals = []
        for i, member in walkers:
            expected = member['tile_per_tick']
            before += expected[0] == expected[-1]
            final = actual[max(ticks), i]
            after += final == expected[-1]
            for tick, tile in zip(ticks, expected):
                before_ticks += expected[0] == tile
                match = actual[tick, i] == tile
                after_ticks += match
                per_tick[str(tick)] += match
            differing = [t for t, tile in zip(ticks, expected) if actual[t, i] != tile]
            if differing and details:
                timing_residuals.append({'type': member['type'], 'start': expected[0],
                                         'differing_ticks': differing})
            if final != expected[-1] and details:
                residuals.append({'type': member['type'], 'start': expected[0],
                                  'candidate': final, 'observed': expected[-1]})
        idle = [(i, m) for i, m in enumerate(members) if m['state_at_tick1'] != 2]
        idle_matches = sum(actual[max(ticks), i] == m['tile_per_tick'][-1] for i, m in idle)
        reports[name] = {'walkers': len(walkers), 'idle_members': len(idle),
                         'idle_final_matches': idle_matches, 'before_final': before,
                         'after_final': after, 'before_tick_tiles': before_ticks,
                         'after_tick_tiles': after_ticks, 'tick_tiles': len(walkers) * len(ticks),
                         'matches_by_tick': per_tick, 'residuals': residuals,
                         'timing_residuals': timing_residuals}
    return reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dataset', type=Path)
    parser.add_argument('--maps', required=True, type=Path)
    parser.add_argument('--probe', type=Path, default=Path('build/formation_probe'))
    parser.add_argument('--rules', type=Path, default=Path('docs/data'))
    parser.add_argument('--details', action='store_true', help='print residuals for private review')
    args = parser.parse_args()
    try:
        reports = compare(json.loads(args.dataset.read_text()), args.maps,
                          args.probe.resolve(), args.rules.resolve(), args.details)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(2, f'formation comparison failed: {error}\n')
    print(json.dumps(reports, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
