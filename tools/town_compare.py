#!/usr/bin/env python3
"""Compare complete, audited economy/service captures after reference repetition."""
import argparse
import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('town_construction', ROOT / 'tools/construction_compare.py')
construction = importlib.util.module_from_spec(spec)
spec.loader.exec_module(construction)
visual = construction.visual
require = visual.require
record = construction.record
integers = construction.integers
SPEC = ROOT / 'tests/parity/town.json'
ALIGNMENT = {'scenario', 'map_sha256', 'controls_sha256', 'normalization_sha256',
             'first_tick', 'last_tick', 'phase'}
EVENTS = {'accepted', 'rejected', 'debit', 'refund', 'activation', 'harvest', 'service',
          'spawn', 'completion', 'upgrade', 'save', 'load'}


def identities(values):
    return integers(values) and all(v > 0 for v in values) and len(values) == len(set(values))


def validate_state(state):
    require(isinstance(state, dict) and set(state) ==
            {'clock', 'money', 'towns', 'objects', 'queue', 'events'}, 'incomplete town state')
    require(type(state['clock']) is int and 0 <= state['clock'] < 7200, 'invalid day clock')
    require(isinstance(state['money'], dict) and state['money'] and all(
        k in tuple(map(str, range(8))) and type(v) is int for k, v in state['money'].items()), 'invalid money')
    for field in ('towns', 'objects'):
        require(isinstance(state[field], dict) and all(
            k.isascii() and k.isdecimal() and int(k) > 0 and str(int(k)) == k
            for k in state[field]), 'invalid canonical identities')
    require(state['towns'], 'missing controlled towns')
    for town in state['towns'].values():
        require(isinstance(town, dict) and 'marked' in town and 'tree' in town, 'incomplete town')
        require(record({k: v for k, v in town.items() if k not in ('marked', 'tree')},
                       'side hall level mode'), 'invalid town fields')
        require(0 <= town['side'] < 8 and town['hall'] >= 0 and town['level'] >= 0
                and town['mode'] in (0, 1, 2) and identities(town['marked']), 'invalid town values')
        require(integers(town['tree']) and len(set(town['tree'])) == len(town['tree'])
                and all(0 <= t < 90 for t in town['tree']), 'invalid build tree')
    for obj in state['objects'].values():
        require(record(obj, 'type side town x y hp hp_max alive protected active'), 'invalid object fields')
        require(0 <= obj['type'] < 90 and 0 <= obj['side'] < 8 and obj['town'] >= 0
                and obj['hp_max'] > 0 and all(obj[k] in (0, 1) for k in ('alive', 'protected', 'active')),
                'invalid object values')
    queue = state['queue']
    require(isinstance(queue, list) and all(record(q, 'id town type remaining total paid started paused site')
            for q in queue), 'invalid town queue')
    require(identities([q['id'] for q in queue]) and all(str(q['town']) in state['towns']
            and 0 <= q['type'] < 90 and 0 <= q['remaining'] <= q['total'] and q['paid'] >= 0
            and q['started'] in (0, 1) and q['paused'] in (0, 1) and q['site'] >= 0 for q in queue),
            'invalid queue values')
    require(isinstance(state['events'], list), 'invalid events')
    for event in state['events']:
        require(isinstance(event, dict) and event.get('kind') in EVENTS
                and record({k: v for k, v in event.items() if k != 'kind'}, 'town object value')
                and str(event['town']) in state['towns'] and event['object'] >= 0, 'invalid event')


def capture(path):
    bundle = visual.load(path)
    require(set(bundle) == {'schema', 'contract', 'origin', 'alignment', 'samples'}, 'invalid bundle fields')
    require(type(bundle['schema']) is int and bundle['schema'] == 1
            and bundle['contract'] == 'town-v1', 'invalid contract')
    require(bundle['origin'] in ('reference', 'clone'), 'invalid origin')
    alignment = bundle['alignment']
    require(isinstance(alignment, dict) and set(alignment) == ALIGNMENT, 'missing alignment controls')
    for key in ('map_sha256', 'controls_sha256', 'normalization_sha256'):
        digest = alignment[key]
        require(isinstance(digest, str) and len(digest) == 64 and all(c in '0123456789abcdef' for c in digest),
                'invalid digest')
    require(alignment['scenario'] in [c['id'] for c in visual.load(SPEC)['cases']], 'unknown town case')
    require(alignment['phase'] == 'before_commands', 'invalid observation phase')
    first, last = alignment['first_tick'], alignment['last_tick']
    require(type(first) is int and type(last) is int and 1 <= first < last <= first + 100000, 'invalid horizon')
    require(isinstance(bundle['samples'], list) and len(bundle['samples']) == last - first + 1, 'incomplete ticks')
    for tick, row in enumerate(bundle['samples'], first):
        require(isinstance(row, dict) and set(row) == {'tick', 'state', 'unavailable'}, 'invalid sample')
        require(type(row['tick']) is int and row['tick'] == tick, 'missing or unordered tick')
        require(row['unavailable'] == [], 'unavailable observations block comparison')
        validate_state(row['state'])
    return bundle


def difference(left, right):
    for a, b in zip(left['samples'], right['samples']):
        delta = construction.first_difference(a['state'], b['state'])
        if delta:
            field, expected, actual = delta
            return dict(tick=a['tick'], field=field, reference=expected, actual=actual)
    return None


def compare(reference, repeat, clone):
    a, r, b = (capture(path) for path in (reference, repeat, clone))
    require([v['origin'] for v in (a, r, b)] == ['reference', 'reference', 'clone'], 'invalid capture origins')
    require(all(visual.identical(a['alignment'], v['alignment']) for v in (r, b)), 'unaligned captures')
    unstable = difference(a, r)
    delta = unstable or difference(a, b)
    return dict(status='reference nondeterminism' if unstable else 'different' if delta else 'equal',
                first=delta, ticks=len(a['samples']), evidence='Comparison only; setup and observer audit required.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('reference', 'repeat', 'clone', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    try:
        output = visual.private_output(args.output)
        require(not output.exists(), 'output already exists')
        result = compare(args.reference, args.repeat, args.clone)
        output.parent.mkdir(parents=True, exist_ok=True)
        visual.write_json(output, result)
        return int(result['status'] != 'equal')
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.exit(2, f'town_compare: {exc}\n')


if __name__ == '__main__':
    sys.exit(main())
