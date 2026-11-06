#!/usr/bin/env python3 -I
"""Compare complete, aligned per-unit movement observations; never launch a game."""
import argparse
import importlib.util
import json
import math
from pathlib import Path

SPEC = importlib.util.spec_from_file_location('parity_report', Path(__file__).with_name('parity_report.py'))
REPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPORT)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate(plan):
    require(set(plan) == {'schema', 'contract', 'mission', 'position_scale', 'units', 'tolerance'},
            'unexpected or missing movement plan keys')
    require(type(plan['schema']) is int and plan['schema'] == 1, 'unsupported movement plan')
    require(isinstance(plan['contract'], str) and plan['contract'], 'missing audited contract')
    require(type(plan['mission']) is int and 0 <= plan['mission'] <= 15, 'invalid mission')
    require(plan['position_scale'] == 256 and type(plan['position_scale']) is int,
            'positions must use pixels times 256')
    tolerance = plan['tolerance']
    require(isinstance(tolerance, dict) and set(tolerance) == {
        'arrival_ticks', 'arrival_radius_px', 'path_ratio_max', 'shape_px', 'idle_px', 'stall_ticks'},
        'unexpected or missing tolerances')
    for key, value in tolerance.items():
        require(type(value) in (int, float) and math.isfinite(value) and value >= 0,
                f'invalid tolerance: {key}')
    require(type(tolerance['arrival_ticks']) is int and type(tolerance['stall_ticks']) is int
            and tolerance['stall_ticks'] > 0 and tolerance['idle_px'] > 0 and tolerance['path_ratio_max'] >= 1,
            'invalid tick or ratio tolerance')
    require(isinstance(plan['units'], dict) and plan['units'], 'missing unit identity mapping')
    for name, unit in plan['units'].items():
        require(name and isinstance(unit, dict) and set(unit) == {'reference', 'clone', 'goal', 'order_tick'},
                'invalid unit mapping')
        require(all(isinstance(unit[key], str) and unit[key] for key in ('reference', 'clone')),
                'invalid observer identity')
        require(isinstance(unit['goal'], list) and len(unit['goal']) == 2
                and all(type(v) is int for v in unit['goal']), 'goal must be fixed-point x/y')
        require(type(unit['order_tick']) is int and unit['order_tick'] >= 1, 'invalid order tick')
    for origin in ('reference', 'clone'):
        identities = [unit[origin] for unit in plan['units'].values()]
        require(len(identities) == len(set(identities)), 'unit mapping must be one-to-one')


def load(path):
    with path.open() as stream:
        metadata = REPORT.header(stream)
        require({'x', 'y', 'hp', 'side', 'type'} <= set(metadata['fields']), 'missing unit fields')
        rows = list(REPORT.snapshots(stream, metadata))
    return metadata, rows


def distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1]) / 256


def measure(rows, identity, unit, tolerance):
    start = unit['order_tick']
    positions = []
    signature = None
    for row in rows:
        obj = row['objects'].get(identity)
        require(obj is not None, f"missing unit {identity} at tick {row['tick']}")
        current = obj['type'], obj['side']
        signature = signature or current
        require(current == signature and obj['hp'] > 0,
                f"unit lifecycle changed: {identity} at tick {row['tick']}")
        positions.append((obj['x'], obj['y']))
    first = rows[0]['tick']
    require(first <= start < rows[-1]['tick'], 'order lacks a complete observation interval')
    arrival = None
    path = 0.0
    idle = 0
    max_idle = 0
    idle_at = {}
    # before_commands: the order at start can first affect the sample at start+1.
    for tick in range(start, rows[-1]['tick'] + 1):
        index = tick - first
        if arrival is not None:
            break
        if tick > start:
            step = distance(positions[index - 1], positions[index])
            path += step
            idle = idle + 1 if step < tolerance['idle_px'] else 0
            max_idle = max(max_idle, idle)
        idle_at[tick] = idle
        if distance(positions[index], unit['goal']) <= tolerance['arrival_radius_px']:
            arrival = tick
    return dict(arrival_tick=arrival, path_px=path, max_idle_ticks=max_idle), positions, idle_at, signature


def compare(plan, reference, clone):
    validate(plan)
    left, a = load(reference)
    right, b = load(clone)
    require(left == right, 'observation mission, horizon, phase or contract differs')
    require(left['contract'] == plan['contract'] and left['mission'] == plan['mission'],
            'movement plan does not match observation contract')
    tolerance = plan['tolerance']
    events, units = [], {}
    for name, unit in sorted(plan['units'].items()):
        r, rp, ri, rs = measure(a, unit['reference'], unit, tolerance)
        c, cp, ci, cs = measure(b, unit['clone'], unit, tolerance)
        require(rs == cs, f'unit type/side mapping differs: {name}')
        separations = {tick: distance(rp[tick - left['first_tick']], cp[tick - left['first_tick']])
                       for tick in range(unit['order_tick'], left['last_tick'] + 1)}
        def event(tick, cause):
            events.append(dict(tick=tick, unit=name, cause_class=cause))
        for tick in range(unit['order_tick'], left['last_tick'] + 1):
            # Keep shape coverage after arrival: displaced parked units matter too.
            if separations[tick] > tolerance['shape_px']:
                event(tick, 'route_shape')
                break
        for tick, idle in ci.items():
            if idle >= tolerance['stall_ticks'] and ri.get(tick, 0) < tolerance['stall_ticks']:
                event(tick, 'extra_stall')
                break
        arrivals = [v for v in (r['arrival_tick'], c['arrival_tick']) if v is not None]
        delta = None
        ratio = None
        if len(arrivals) == 2:
            delta = c['arrival_tick'] - r['arrival_tick']
            if abs(delta) > tolerance['arrival_ticks']:
                event(min(arrivals) + tolerance['arrival_ticks'] + 1, 'arrival_timing')
            if r['path_px'] > 0:
                ratio = c['path_px'] / r['path_px']
                if ratio > tolerance['path_ratio_max']:
                    event(max(arrivals), 'path_length')
            elif c['path_px'] > 0:
                event(max(arrivals), 'path_length')
        elif arrivals:
            # A horizon before the deadline cannot establish a timing failure.
            deadline = arrivals[0] + tolerance['arrival_ticks'] + 1
            if deadline <= left['last_tick']:
                event(deadline, 'arrival_timing')
        units[name] = dict(reference=r, clone=c, arrival_delta_ticks=delta,
                           clone_reference_path_ratio=ratio,
                           max_aligned_separation_px=max(separations.values()))
    events.sort(key=lambda e: (e['tick'], e['unit'], e['cause_class']))
    # Non-arrivals are censored, including unreachable-goal controls. Never turn
    # identical stationary traces into a claimed arrival-quality pass.
    complete = all(u[origin]['arrival_tick'] is not None
                   for u in units.values() for origin in ('reference', 'clone'))
    return dict(status='different' if events else 'within_tolerance' if complete else 'incomplete',
                first_divergence=events[0] if events else None, units=units,
                tolerance=tolerance, contract=left['contract'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('plan', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('clone', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        result = compare(REPORT.json_object(args.plan.read_text()), args.reference, args.clone)
    except (ValueError, OSError, StopIteration, KeyError, TypeError) as exc:
        result = dict(status='invalid', error=str(exc), first_divergence=None)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    return 0 if result['status'] == 'within_tolerance' else 1


if __name__ == '__main__':
    raise SystemExit(main())
