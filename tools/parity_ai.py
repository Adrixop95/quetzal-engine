#!/usr/bin/env python3
"""Compare private L8 event captures using repeated-reference timing windows.

The numeric capture contract and observer limitations are in tests/parity/README.md.
This tool consumes captures; it does not implement an original-game observer.
"""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def capture(value):
    """Validate the entire stream before accepting even an early comparison result."""
    if not isinstance(value, list) or len(value) != 5:
        raise ValueError('capture must be [version, scenario, horizon, required_mask, events]')
    version, scenario, horizon, mask, rows = value
    if (not integer(version, 1, 1) or not integer(scenario, 0, 2**31 - 1)
            or not integer(horizon, 1, 2**31 - 1) or not integer(mask, 1, 31)
            or not isinstance(rows, list)):
        raise ValueError('invalid capture header')
    events = {}
    previous_tick = -1
    seen = 0
    for row in rows:
        if not isinstance(row, list) or len(row) != 5:
            raise ValueError('event must be [tick, side, kind, occurrence, composition]')
        tick, side, kind, occurrence, composition = row
        if (not integer(tick, 0, horizon) or tick < previous_tick
                or not integer(side, 0, 7) or not integer(kind, 0, 4)
                or not integer(occurrence, 0, 2**31 - 1)
                or not isinstance(composition, list) or not composition):
            raise ValueError('invalid or unordered event')
        key = (side, kind, occurrence)
        if key in events or (occurrence and (side, kind, occurrence - 1) not in events):
            raise ValueError('duplicate or nonconsecutive occurrence')
        counts = {}
        for pair in composition:
            if (not isinstance(pair, list) or len(pair) != 2
                    or not integer(pair[0], 0, 89) or not integer(pair[1], 1, 2**31 - 1)
                    or pair[0] in counts):
                raise ValueError('invalid or duplicate composition member')
            counts[pair[0]] = pair[1]
        if kind in (0, 1) and (len(counts) != 1 or sum(counts.values()) != 1):
            raise ValueError('production and expansion events require one object')
        events[key] = (tick, counts)
        previous_tick = tick
        seen |= 1 << kind
    if seen & mask != mask:
        raise ValueError('capture lacks a required behaviour class')
    return tuple(value[:4]), events


def compare(references, clone):
    if len(references) < 2:
        raise ValueError('at least two reference captures are required')
    parsed = [capture(value) for value in [*references, clone]]
    header = parsed[0][0]
    if any(item[0] != header for item in parsed):
        raise ValueError('scenario, horizon or required classes differ')
    base = parsed[0][1]
    repeats = [item[1] for item in parsed[:-1]]
    actual = parsed[-1][1]
    if any(set(run) != set(base) for run in repeats):
        return {'category': 'reference_event_coverage_differs', 'pass': False}, []
    # Numeric rows: side, kind, occurrence, first tick, last tick, type, min, max.
    # Type -1 denotes total composition size, preserving a bound on force size.
    windows = []
    differences = []
    for key in sorted(set(base) | set(actual)):
        if key not in base:
            differences.append((actual[key][0], key, 'unexpected_event', -1))
            continue
        ticks = [run[key][0] for run in repeats]
        first, last = min(ticks), max(ticks)
        tick, counts = actual.get(key, (first, {}))
        if key not in actual:
            differences.append((first, key, 'missing_event', -1))
        elif not first <= tick <= last:
            differences.append((tick, key, 'timing_window', -1))
        types = sorted({ty for run in repeats for ty in run[key][1]})
        for ty in [-1, *types]:
            values = [sum(run[key][1].values()) if ty == -1 else run[key][1].get(ty, 0) for run in repeats]
            low, high = min(values), max(values)
            windows.append([*key, first, last, ty, low, high])
            count = sum(counts.values()) if ty == -1 else counts.get(ty, 0)
            if key in actual and not low <= count <= high:
                differences.append((tick, key, 'composition_window', ty))
        for ty in sorted(set(counts) - set(types)):
            differences.append((tick, key, 'unexpected_type', ty))
    if differences:
        tick, key, category, ty = min(differences)
        return {'category': category, 'pass': False, 'tick': tick,
                'event': list(key), 'type': ty}, windows
    return {'category': 'within_observed_windows', 'pass': True}, windows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, nargs='+', required=True)
    parser.add_argument('--clone', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        if not output.is_relative_to((ROOT / 'work/parity').resolve()):
            raise ValueError('output must be inside this worktree work/parity directory')
        if len(args.reference) < 2 or len({p.resolve() for p in args.reference}) != len(args.reference):
            raise ValueError('provide at least two distinct reference capture paths')
        if args.clone.resolve() in {p.resolve() for p in args.reference}:
            raise ValueError('clone capture must have its own path')
        paths = [*args.reference, args.clone]
        if any(not path.is_file() for path in paths):
            print('SKIP: missing private AI captures')
            return 77
        values = [json.loads(path.read_text()) for path in paths]
        result, windows = compare(values[:-1], values[-1])
        output.mkdir(parents=True, exist_ok=False)
        (output / 'diff.json').write_text(json.dumps(result, indent=2) + '\n')
        # Unstable event coverage provides no usable expectation set.
        if windows:
            (output / 'windows.json').write_text(json.dumps(windows) + '\n')
        print(result['category'])
        return 0 if result['pass'] else 1
    except (OSError, ValueError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
