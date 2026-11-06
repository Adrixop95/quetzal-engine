#!/usr/bin/env python3
"""Asset-free economy/service contracts; invented values are not parity evidence."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('town_compare', ROOT / 'tools/town_compare.py')
town = importlib.util.module_from_spec(spec)
spec.loader.exec_module(town)


class TownTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        state = dict(clock=7199, money={'0': 900},
                     towns={'1': dict(side=0, hall=1, level=1, mode=1, marked=[2], tree=[12, 14])},
                     objects={'1': dict(type=11, side=0, town=1, x=100, y=100, hp=100,
                                        hp_max=100, alive=1, protected=0, active=1)},
                     queue=[dict(id=1, town=1, type=0, remaining=1, total=10, paid=100,
                                 started=1, paused=0, site=0)], events=[])
        alignment = dict(scenario='town-service', map_sha256='a' * 64, controls_sha256='b' * 64,
                         normalization_sha256='c' * 64, first_tick=1, last_tick=3, phase='before_commands')
        self.reference = dict(schema=1, contract='town-v1', origin='reference', alignment=alignment,
                              samples=[dict(tick=t, state=copy.deepcopy(state), unavailable=[]) for t in range(1, 4)])
        self.repeat = copy.deepcopy(self.reference)
        self.clone = copy.deepcopy(self.reference)
        self.clone['origin'] = 'clone'

    def compare(self):
        paths = []
        for name in ('reference', 'repeat', 'clone'):
            path = self.root / (name + '.json')
            path.write_text(json.dumps(getattr(self, name)))
            paths.append(path)
        return town.compare(*paths)

    def test_each_behaviour_class_reports_tick_and_field(self):
        self.assertEqual(self.compare()['status'], 'equal')
        controls = [
            ('clock', lambda s: s.update(clock=0)),
            ('money.0', lambda s: s['money'].update({'0': 899})),
            ('objects.1.active', lambda s: s['objects']['1'].update(active=0)),
            ('objects.1.hp', lambda s: s['objects']['1'].update(hp=99)),
            ('objects.1.protected', lambda s: s['objects']['1'].update(protected=1)),
            ('towns.1.level', lambda s: s['towns']['1'].update(level=2)),
            ('towns.1.tree.length', lambda s: s['towns']['1']['tree'].append(19)),
            ('queue[0].remaining', lambda s: s['queue'][0].update(remaining=0)),
            ('queue[0].town', lambda s: (s['towns'].update({'2': copy.deepcopy(s['towns']['1'])}),
                                       s['queue'][0].update(town=2))),
        ]
        for field, mutation in controls:
            with self.subTest(field=field):
                original = copy.deepcopy(self.clone)
                mutation(self.clone['samples'][1]['state'])
                result = self.compare()
                self.assertEqual(result['status'], 'different')
                self.assertEqual(result['first']['tick'], 2)
                self.assertEqual(result['first']['field'], field)
                self.clone = original

    def test_events_are_ordered_and_missing_events_differ(self):
        for kind in sorted(town.EVENTS):
            with self.subTest(kind=kind):
                event = dict(kind=kind, town=1, object=1, value=7)
                self.clone['samples'][1]['state']['events'] = [event]
                self.assertEqual(self.compare()['first']['field'], 'events.length')
        events = [dict(kind='debit', town=1, object=1, value=7),
                  dict(kind='refund', town=1, object=1, value=3)]
        self.reference['samples'][1]['state']['events'] = events
        self.repeat = copy.deepcopy(self.reference)
        self.clone['samples'][1]['state']['events'] = list(reversed(events))
        self.assertEqual(self.compare()['first']['field'], 'events[0].kind')

    def test_repeat_instability_takes_priority(self):
        self.repeat['samples'][2]['state']['money']['0'] -= 1
        self.clone['samples'][0]['state']['money']['0'] -= 2
        result = self.compare()
        self.assertEqual(result['status'], 'reference nondeterminism')
        self.assertEqual(result['first']['tick'], 3)

    def test_all_ticks_validated_before_reporting_earlier_difference(self):
        mutations = [lambda b: b['samples'].pop(), lambda b: b['samples'][-1].update(tick=2),
                     lambda b: b['samples'][-1].update(unavailable=['service']),
                     lambda b: b['samples'][-1]['state'].pop('events'),
                     lambda b: b['samples'][-1]['state'].update(clock=True),
                     lambda b: b['samples'][-1]['state']['queue'][0].update(remaining=11),
                     lambda b: b['samples'][-1]['state']['queue'][0].update(town=99),
                     lambda b: b['samples'][-1]['state']['objects']['1'].update(active=True),
                     lambda b: b['samples'][-1]['state']['towns']['1'].update(tree=[12, 12]),
                     lambda b: b['samples'][-1]['state'].update(events=[dict(kind='guessed')]),
                     lambda b: b['alignment'].update(normalization_sha256='d' * 64),
                     lambda b: b['alignment'].update(scenario='unknown'),
                     lambda b: b['alignment'].update(phase='after_commands'),
                     lambda b: b.update(origin='reference')]
        for mutation in mutations:
            original = copy.deepcopy(self.clone)
            self.clone['samples'][0]['state']['money']['0'] -= 1
            mutation(self.clone)
            with self.assertRaises(ValueError):
                self.compare()
            self.clone = original

    def test_catalogue_is_pending_and_covers_behaviour_classes(self):
        catalogue = json.loads(town.SPEC.read_text())
        self.assertEqual(catalogue['status'], 'needs live run')
        self.assertEqual(catalogue['evidence'], 'P')
        self.assertEqual(catalogue['map'], 'ztest_all')
        self.assertEqual(catalogue['modifications'], {'parallel_construction': False})
        self.assertEqual({c['id'] for c in catalogue['cases']},
                         {'resource-clock', 'training-towns', 'payment-cancel', 'town-service', 'hall-tree-save'})
        self.assertTrue(all(c['variants'] and c['input'] and c['measure'] for c in catalogue['cases']))


if __name__ == '__main__':
    unittest.main()
