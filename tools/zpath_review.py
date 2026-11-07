#!/usr/bin/env python3 -I
"""Gate a movement before/after report on two exact normalized reference runs."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path

SPEC = importlib.util.spec_from_file_location('zpath_trace', Path(__file__).with_name('zpath_trace.py'))
TRACE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TRACE)


def review(plan, reference, repeat, before, after):
    TRACE.validate(plan)
    TRACE.require(not reference.samefile(repeat), 'reference runs must be separate files')
    # Check the complete streams, including the parked tail and other observed
    # objects. Tolerances for clone comparisons must not hide an unstable oracle.
    repetition = TRACE.REPORT.compare(reference, repeat)
    reference_plan = copy.deepcopy(plan)
    for unit in reference_plan['units'].values():
        unit['clone'] = unit['reference']
    TRACE.compare(reference_plan, reference, repeat)
    if repetition['category'] != 'equal_observed_fields':
        return dict(status='reference_not_repeatable', repetition=repetition,
                    before=None, after=None)
    baseline = TRACE.compare(plan, reference, before)
    candidate = TRACE.compare(plan, reference, after)
    return dict(status=candidate['status'], repetition=repetition,
                before=baseline, after=candidate)


def markdown(scenario, report):
    # JSON quoting keeps names on a single line; escape Markdown table syntax.
    def label(value):
        return json.dumps(value, ensure_ascii=True).replace('|', '&#124;')

    lines = [f'Scenario: {label(scenario)}', '', f"Status: {report['status']}", '']
    if report.get('before') is None:
        lines += ['No accepted before/after comparison.', '',
                  'Diagnostic: ' + json.dumps(report, ensure_ascii=True), '']
        return '\n'.join(lines)
    lines += ['Arrival ticks are absolute before-command samples; null arrivals are censored.', '',
              '| Unit | Original arrival (both runs) | Clone before | Clone after | Path px original / before / after | Max idle ticks original / before / after |',
              '|---|---:|---:|---:|---|---|']
    for name, baseline in report['before']['units'].items():
        candidate = report['after']['units'][name]
        measures = [baseline['reference'], baseline['clone'], candidate['clone']]
        arrivals = ['censored' if m['arrival_tick'] is None else str(m['arrival_tick']) for m in measures]
        paths = ' / '.join(f"{m['path_px']:.3f}" for m in measures)
        idle = ' / '.join(str(m['max_idle_ticks']) for m in measures)
        lines.append(f'| {label(name)} | {arrivals[0]} | {arrivals[1]} | {arrivals[2]} | {paths} | {idle} |')
    for phase in ('before', 'after'):
        event = report[phase]['first_divergence']
        description = (f"tick {event['tick']}, unit {label(event['unit'])}, {event['cause_class']}"
                       if event else 'none observed')
        lines += ['', f'First divergence {phase}: {description}.']
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('plan', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('repeat', type=Path)
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    parser.add_argument('--scenario', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--markdown', type=Path, required=True)
    args = parser.parse_args()
    try:
        report = review(TRACE.REPORT.json_object(args.plan.read_text()), args.reference,
                        args.repeat, args.before, args.after)
    except (ValueError, OSError, StopIteration, KeyError, TypeError) as exc:
        report = dict(status='invalid', error=str(exc), before=None, after=None)
    report['scenario'] = args.scenario
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
    args.markdown.write_text(markdown(args.scenario, report))
    return 0 if report['status'] == 'within_tolerance' else 1


if __name__ == '__main__':
    raise SystemExit(main())
