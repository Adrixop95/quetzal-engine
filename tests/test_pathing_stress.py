"""Pathing stress acceptance through task 112's real player-command runner."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

import pathing_stress as stress

ENGINE = Path(sys.argv.pop(1)).resolve()


class StressTests(unittest.TestCase):
    def test_synthetic_courses_both_profiles_repeat_every_tick(self):
        with tempfile.TemporaryDirectory(prefix='aztec-pathing-') as directory:
            root = Path(directory)
            for name in stress.NAMES:
                for profile in ('faithful', 'enhanced'):
                    with self.subTest(course=name, profile=profile):
                        spec = stress.course(name)
                        first, rows = stress.run(root / f'{name}-{profile}', ENGINE, spec, profile, launcher=())
                        repeat, again = stress.run(root / f'{name}-{profile}-repeat', ENGINE, spec, profile, launcher=())
                        self.assertEqual(rows, again)
                        self.assertEqual(first['metrics'], repeat['metrics'])
                        self.assertEqual(first['route']['profile'], profile)
                        self.assertEqual(first['route']['outcome'], 'won', first['route']['anomalies'])
                        m = first['metrics']
                        expected = sum(u[2] in spec['goals'] for u in spec['units'])
                        self.assertEqual(m['ordered'], expected)
                        self.assertEqual(m['arrived'], expected, m)
                        self.assertEqual((m['stuck'], m['deadlocks']), (0, 0), m)
                        self.assertEqual(m['settled'], expected, m)
                        for unit in m['units']:
                            self.assertLess(unit['time_ratio'], 4, unit)
                        for row in rows:
                            for obj in row['objects']:
                                if obj['type'] not in (0, 1, 88):
                                    continue
                                x, y = (v // stress.TILE for v in obj['position'])
                                self.assertTrue(0 <= x < stress.WIDTH and 0 <= y < stress.HEIGHT, obj)
                                terrain = spec['terrain'][y][x]
                                self.assertTrue(terrain == 0 if obj['type'] == 88 else terrain >= 1, obj)
                                if name == 'construction':
                                    self.assertFalse(19 <= x <= 20 and 12 <= y <= 13, obj)
                        if name == 'construction':
                            # Prove this is an actual placed unfinished structure on the route.
                            buildings = [o for row in rows for o in row['objects'] if o['type'] == 14]
                            self.assertTrue(buildings)
                            self.assertTrue(all(o['progress'] < 1000 for o in buildings))
                        if name == 'group':
                            self.assertTrue(any(c['type'] == 0x15 for row in rows for c in row['commands']))

    def test_rejected_namespace_is_a_reported_failure(self):
        with tempfile.TemporaryDirectory(prefix='aztec-pathing-denied-') as directory:
            result, rows = stress.run(Path(directory) / 'run', ENGINE, stress.course('narrow'), 'faithful',
                                      launcher=('/bin/false',))
            self.assertFalse(rows)
            self.assertEqual(result['route']['outcome'], 'scenario-failed')
            self.assertEqual(result['metrics']['arrival_rate'], 0)


class MeasurementTests(unittest.TestCase):
    @staticmethod
    def row(tick, position, *, command=False, order=1):
        obj = dict(id=1, type=0, alive=True, carrier=0, order=order, position=position, path_failures=0)
        commands = [dict(type=0x17, ids=[1])] if command else []
        return dict(tick=tick, objects=[obj], commands=commands)

    def test_stuck_and_jitter_do_not_become_success(self):
        spec = stress.course('narrow')
        rows = [self.row(0, [168, 600])]
        rows += [self.row(t, [168, 600], command=t == 1) for t in range(1, 130)]
        m = stress.measure(rows, spec)
        self.assertEqual((m['arrived'], m['stuck'], m['deadlocks']), (0, 1, 1))
        for t, row in enumerate(rows):
            row['objects'][0]['position'] = [168 + (t % 2) * 48, 600]
        m = stress.measure(rows, spec)
        self.assertEqual(m['deadlocks'], 1)
        self.assertGreater(m['units'][0]['jitter'], 100)
        self.assertGreater(m['units'][0]['oscillation'], 100)

    def test_known_grid_optimum_and_unreachable_route(self):
        spec = stress.course('dense')
        self.assertEqual(stress.grid_distance(spec, (3, 12), (32, 12, 32, 12)), 29 * 48)
        spec['terrain'][12][32] = 0
        self.assertIsNone(stress.grid_distance(spec, (3, 12), (32, 12, 32, 12)))

    def test_early_crossing_is_not_settlement_and_teleport_is_visible(self):
        spec = stress.course('narrow')
        rows = [self.row(0, [168, 600]), self.row(1, [1560, 600], command=True), self.row(2, [168, 600])]
        m = stress.measure(rows, spec)
        self.assertEqual((m['arrived'], m['settled']), (1, 0))
        self.assertEqual(m['units'][0]['jumps'], 2)
        broken = copy.deepcopy(rows)
        broken[1]['objects'][0]['alive'] = False
        self.assertEqual(stress.measure(broken, spec)['arrived'], 0)

    def test_report_is_serializable_and_empty_observations_never_pass(self):
        self.assertEqual(stress.measure([], stress.course('narrow'))['arrival_rate'], 0)
        json.dumps(stress.measure([], stress.course('narrow')))


if __name__ == '__main__':
    unittest.main()
