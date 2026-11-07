#!/usr/bin/env python3
"""Compare repeated L8 semantic traces. Reference normalization needs task 64."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
KINDS = {'build_order', 'production', 'expansion', 'launch', 'defence', 'retreat'}
# Accepted command events cannot recover private strategist variables.
BEHAVIOURS = {
    'composition_choice': (('production', 'launch'), None),
    'placement': (('build_order',), None),
    'launch_scoring': ((), 'Launch events do not expose candidate scores or rejected launches.'),
    'instantaneous_income': ((), 'Requires an audited estimate with units and sampling phase; build reactions are a proxy.'),
    'side_plan_arbitration': ((), 'Requires competing town requests, shared cash and accepted/rejected plan identity.'),
    'hall_upgrade_selection': ((), 'Requires explicit upgrade candidates, selection and accepted upgrade events.'),
    'expansion': (('expansion',), None),
    'retreat': (('retreat',), None),
    'sustained_opposition': (('launch', 'defence'), 'Command activity alone does not establish firing, losses or surviving opposition.'),
    'defence_latency': (('defence',), None),
}


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


def envelope(values):
    return [min(values), max(values)] if values else None


def observation(runs, start):
    """Retain zeros for counts and null for unavailable latency, without imputation."""
    events = [e for run in runs for e in run]
    counts = [len(run) for run in runs]
    first = [run[0]['tick'] - start for run in runs if run]
    last = [run[-1]['tick'] - start for run in runs if run]
    def frequencies(selected):
        result = {}
        for field in ('composition', 'placement'):
            values = [e['composition'] if field == 'composition'
                      else (e['composition'], e['x'], e['y']) for e in selected]
            result[field] = [dict(value=value, count=count)
                             for value, count in sorted(Counter(values).items())]
        return result
    return dict(counts=counts, count_window=envelope(counts), observed_runs=len(first),
                first_window=envelope(first), last_window=envelope(last), frequencies=frequencies(events),
                runs=[dict(ticks=[e['tick'] - start for e in run], frequencies=frequencies(run)) for run in runs])


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
        result = dict(id=group['id'], event=group['event'], start=group['start'], end=group['end'],
                      original=observation(a, group['start']), clone=observation(b, group['start']),
                      confirmed_timing_differences=[], differences=[])
        if not flat_a:
            result['differences'].append('unobserved_reference_class')
        elif sum(bool(run) for run in a) < 2:
            result['differences'].append('insufficient_reference_observations')
        if flat_a and not flat_b:
            result['differences'].append('missing_behaviour_class')
        if flat_a and flat_b:
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
                    if len(times_a) >= 2:
                        result['confirmed_timing_differences'].append(edge)
            for field in ('composition', 'placement'):
                def values(events):
                    return [e['composition'] if field == 'composition'
                            else (e['composition'], e['x'], e['y']) for e in events]
                distance = tv(values(flat_a), values(flat_b))
                result[f'{field}_tv'] = distance
                if distance > spec['tv_limit'] + 1e-12:
                    result['differences'].append(f'{field}_distribution')
        results.append(result)
    missing = any(set(row['differences']) & {'unobserved_reference_class', 'insufficient_reference_observations'}
                  for row in results)
    passed = all(not row['differences'] for row in results)
    return dict(status='needs live run' if missing else 'within_observed_windows' if passed else 'different',
                passed=passed, groups=results, tv_limit=spec['tv_limit'],
                reference_runs=len(refs), clone_runs=len(engines))


def behaviour_report(spec, before=None, after=None):
    """Keep all ten requested classes visible, including unsupported diagnostics."""
    rows = []
    for name, (events, blocker) in BEHAVIOURS.items():
        groups = []
        for group in spec['groups']:
            if group['event'] not in events:
                continue
            def find(report):
                return next((r for r in report['groups'] if r['id'] == group['id']), None) if report else None
            b, a = find(before), find(after)
            groups.append(dict(id=group['id'], start=group['start'], end=group['end'],
                               original=b['original'] if b else None,
                               clone_before=b['clone'] if b else None, clone_after=a['clone'] if a else None,
                               differences_before=b['differences'] if b else None,
                               differences_after=a['differences'] if a else None,
                               confirmed_timing_differences=b['confirmed_timing_differences'] if b else []))
        if not groups and not blocker:
            blocker = 'No observation group for this behaviour in this scenario.'
        selected = [find_report for find_report in (after or before or {}).get('groups', [])
                    if find_report['event'] in events]
        unavailable = any(set(row['differences']) & {'unobserved_reference_class', 'insufficient_reference_observations'}
                          for row in selected)
        status = ('needs live run' if blocker or before is None or unavailable else
                  'different' if any(row['differences'] for row in selected) else 'within_observed_event_windows')
        rows.append(dict(behaviour=name, blocker=blocker, groups=groups,
                         status=status))
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenario', type=Path, required=True)
    parser.add_argument('--reference', type=Path, nargs='+')
    parser.add_argument('--clone', type=Path, nargs='+')
    parser.add_argument('--after', type=Path, nargs='+', help='fresh clone captures after a measured fix')
    parser.add_argument('--plan', action='store_true', help='write pending per-behaviour forms without running games')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        if not output.is_relative_to((ROOT / 'work/parity').resolve()) or output.exists():
            raise ValueError('output must be a new directory inside work/parity')
        spec = scenario(json.loads(args.scenario.read_text()))
        if args.plan:
            if args.reference or args.clone or args.after:
                raise ValueError('--plan cannot consume captures')
        elif not args.reference or not args.clone:
            raise ValueError('comparison requires --reference and --clone')
        paths = [*(args.reference or []), *(args.clone or []), *(args.after or [])]
        if len({p.resolve() for p in paths}) != len(paths):
            raise ValueError('every run must have a distinct capture path')
        if args.plan or not spec.get('reference_audit') or any(not p.is_file() for p in paths):
            result = dict(status='needs live run', passed=False,
                          reason='missing audited reference observer or private captures',
                          behaviours=behaviour_report(spec))
            code = 77
        else:
            rows = [[json.loads(line) for line in p.read_text().splitlines()] for p in paths]
            split = len(args.reference)
            end = split + len(args.clone)
            result = compare(spec, rows[:split], rows[split:end])
            after = compare(spec, rows[:split], rows[end:]) if args.after else None
            result['behaviours'] = behaviour_report(spec, result, after)
            if after:
                result = dict(status=after['status'], passed=after['passed'], before=result, after=after,
                              behaviours=result['behaviours'])
            code = 0 if result['passed'] else 77 if result['status'] == 'needs live run' else 1
        # Fingerprints bind reports to private inputs, not to an asserted observer audit.
        result['inputs'] = [dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest()
                                if p.is_file() else None) for p in [args.scenario, *paths]]
        result['scope'] = 'Event projection only; review setup, fixed seed and task 85 replay evidence before any planner fix.'
        output.mkdir(parents=True, exist_ok=False)
        (output / 'scenario.json').write_text(json.dumps(spec, indent=2) + '\n')
        (output / 'diff.json').write_text(json.dumps(result, indent=2) + '\n')
        print(result['status'])
        return code
    except (OSError, ValueError, TypeError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
