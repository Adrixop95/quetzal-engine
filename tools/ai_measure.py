#!/usr/bin/env python3
"""Compare repeated L8 semantic traces. Reference normalization needs task 64."""
import argparse
from collections import Counter
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
KINDS = {'build_order', 'production', 'expansion', 'launch', 'defence', 'retreat'}


def integer(value, low=0, high=2**31 - 1):
    return type(value) is int and low <= value <= high


def scenario(value):
    if not isinstance(value, dict) or not integer(value.get('schema'), 1, 1):
        raise ValueError('invalid scenario schema')
    if (not isinstance(value.get('id'), str) or not value['id']
            or not integer(value.get('horizon'), 1) or not integer(value.get('repeats'), 2)
            or type(value.get('tv_limit')) not in (int, float) or not 0 <= value['tv_limit'] <= 1
            or not isinstance(value.get('groups'), list) or not value['groups']):
        raise ValueError('invalid scenario controls')
    names = set()
    for group in value['groups']:
        if (not isinstance(group, dict) or not isinstance(group.get('id'), str) or not group['id']
                or group['id'] in names or not isinstance(group.get('event'), str) or group['event'] not in KINDS
                or not integer(group.get('side'), 0, 7) or not integer(group.get('town'))
                or not integer(group.get('start'), 1, value['horizon'])
                or not integer(group.get('end'), group['start'], value['horizon'])):
            raise ValueError('invalid or duplicate observation group')
        names.add(group['id'])
    return value


def trace(rows, spec, origin):
    """Require every completed tick, exact semantics, and an explicit end marker."""
    if not isinstance(rows, list) or len(rows) < 2 or not isinstance(rows[0], dict):
        raise ValueError('missing trace header')
    header = rows[0]
    contract = 'engine-ai-v1' if origin == 'engine' else 'audited-ai-v1'
    if (not integer(header.get('schema'), 1, 1) or header.get('contract') != contract
            or header.get('origin') != origin or not integer(header.get('hz'), 12, 12)
            or not integer(header.get('last_tick'), spec['horizon'], spec['horizon'])):
        raise ValueError('incompatible AI trace header')
    if origin == 'reference' and (not spec.get('reference_audit')
                                  or header.get('audit') != spec['reference_audit']):
        raise ValueError('missing or different reference observer audit')
    tick = 1
    events = []
    for index, row in enumerate(rows[1:], 1):
        if not isinstance(row, dict) or not integer(row.get('tick'), 1, spec['horizon']):
            raise ValueError('invalid trace tick')
        kind = row.get('event')
        if kind == 'end':
            if index != len(rows) - 1 or row['tick'] != spec['horizon'] or tick != spec['horizon'] + 1:
                raise ValueError('incomplete trace horizon')
            return events
        if row['tick'] != tick or tick > spec['horizon']:
            raise ValueError('missing, repeated or unordered completed tick')
        if kind == 'tick':
            tick += 1
            continue
        if (not isinstance(kind, str) or kind not in KINDS or not integer(row.get('side'), 0, 7) or not integer(row.get('town'))
                or not integer(row.get('x'), -1) or not integer(row.get('y'), -1)
                or (row['x'] == -1) != (row['y'] == -1)
                or not isinstance(row.get('composition'), list) or not row['composition']):
            raise ValueError('invalid AI event')
        counts = {}
        for pair in row['composition']:
            if (not isinstance(pair, list) or len(pair) != 2 or not integer(pair[0], 0, 89)
                    or not integer(pair[1], 1) or pair[0] in counts):
                raise ValueError('invalid composition')
            counts[pair[0]] = pair[1]
        if kind in {'build_order', 'production', 'expansion'} and sum(counts.values()) != 1:
            raise ValueError('building/production events require one object')
        if kind in {'launch', 'defence', 'retreat'} and row['x'] != -1:
            raise ValueError('force events have no construction placement')
        events.append(dict(row, composition=tuple(sorted(counts.items()))))
    raise ValueError('missing trace end marker')


def tv(a, b):
    """Total variation of empirical whole-event distributions, preserving correlation."""
    ac, bc = Counter(a), Counter(b)
    return sum(abs(ac[k] / len(a) - bc[k] / len(b)) for k in ac.keys() | bc.keys()) / 2


def compare(spec, references, clones):
    spec = scenario(spec)
    if len(references) < spec['repeats'] or len(clones) < spec['repeats']:
        raise ValueError('insufficient repeats on one or both implementations')
    # Validate every stream, including events outside selected observation groups.
    refs = [trace(rows, spec, 'reference') for rows in references]
    engines = [trace(rows, spec, 'engine') for rows in clones]
    results = []
    for group in spec['groups']:
        def select(runs):
            return [[e for e in run if e['event'] == group['event'] and e['side'] == group['side']
                     and e['town'] == group['town'] and group['start'] <= e['tick'] <= group['end']]
                    for run in runs]
        a, b = select(refs), select(engines)
        flat_a, flat_b = [e for run in a for e in run], [e for run in b for e in run]
        result = dict(id=group['id'], event=group['event'], differences=[])
        if not flat_a:
            result['differences'].append('unobserved_reference_class')
        elif not flat_b:
            result['differences'].append('missing_behaviour_class')
        else:
            counts_a, counts_b = [len(run) for run in a], [len(run) for run in b]
            result['count_windows'] = [[min(counts_a), max(counts_a)], [min(counts_b), max(counts_b)]]
            # A clone count outside the reference envelope is a coverage difference.
            if min(counts_b) < min(counts_a) or max(counts_b) > max(counts_a):
                result['differences'].append('event_count_window')
            # First/last times are relative to the declared stimulus/observation start.
            # Runs without this class contribute zero to count windows, never fake latency zero.
            result['timing_windows'] = {}
            positions = [('first', 0), ('last', -1)]
            positions += [(str(n), n) for n in range(min(max(counts_a), max(counts_b)))]
            for edge, position in positions:
                times_a = [run[position]['tick'] - group['start'] for run in a if run and len(run) > position]
                times_b = [run[position]['tick'] - group['start'] for run in b if run and len(run) > position]
                aw, bw = [min(times_a), max(times_a)], [min(times_b), max(times_b)]
                result['timing_windows'][edge] = [aw, bw]
                if max(aw[0], bw[0]) > min(aw[1], bw[1]):
                    result['differences'].append(f'{edge}_timing_window')
            for field in ('composition', 'placement'):
                def values(events):
                    return [e['composition'] if field == 'composition'
                            else (e['composition'], e['x'], e['y']) for e in events]
                distance = tv(values(flat_a), values(flat_b))
                result[f'{field}_tv'] = distance
                if distance > spec['tv_limit'] + 1e-12:
                    result['differences'].append(f'{field}_distribution')
        results.append(result)
    missing = any('unobserved_reference_class' in row['differences'] for row in results)
    passed = all(not row['differences'] for row in results)
    return dict(status='needs live run' if missing else 'within_observed_windows' if passed else 'different',
                passed=passed, groups=results, tv_limit=spec['tv_limit'],
                reference_runs=len(refs), clone_runs=len(engines))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenario', type=Path, required=True)
    parser.add_argument('--reference', type=Path, nargs='+', required=True)
    parser.add_argument('--clone', type=Path, nargs='+', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        if not output.is_relative_to((ROOT / 'work/parity').resolve()) or output.exists():
            raise ValueError('output must be a new directory inside work/parity')
        spec = scenario(json.loads(args.scenario.read_text()))
        paths = [*args.reference, *args.clone]
        if len({p.resolve() for p in paths}) != len(paths):
            raise ValueError('every run must have a distinct capture path')
        if not spec.get('reference_audit') or any(not p.is_file() for p in paths):
            result = dict(status='needs live run', passed=False,
                          reason='missing audited reference observer or private captures')
            code = 77
        else:
            rows = [[json.loads(line) for line in p.read_text().splitlines()] for p in paths]
            result = compare(spec, rows[:len(args.reference)], rows[len(args.reference):])
            code = 0 if result['passed'] else 77 if result['status'] == 'needs live run' else 1
        output.mkdir(parents=True, exist_ok=False)
        (output / 'diff.json').write_text(json.dumps(result, indent=2) + '\n')
        print(result['status'])
        return code
    except (OSError, ValueError, TypeError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
