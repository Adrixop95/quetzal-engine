#!/usr/bin/env python3 -I
"""Synthetic reference-log checks; no game data or executable required."""
import importlib.util
import copy
import json
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]

def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / (name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

ANALYZER = module('zpath_analyze')
GENERATOR = module('make_zpath')
TRACE = module('zpath_trace')

class TraceTests(unittest.TestCase):
    def setUp(self):
        self.plan = dict(schema=1, contract='synthetic-movement-v1', mission=1,
                         position_scale=256,
                         units={'scout': dict(reference='pool7', clone='1', goal=[1024, 0], order_tick=1)},
                         tolerance=dict(arrival_ticks=0, arrival_radius_px=0, path_ratio_max=1.25,
                                        shape_px=10, idle_px=0.5, stall_ticks=2))
        self.points = [(0, 0), (256, 0), (512, 0), (768, 0), (1024, 0)]

    def write(self, path, points, origin):
        header = dict(schema=1, mission=1, phase='before_commands', contract=self.plan['contract'],
                      fields=['hp', 'side', 'type', 'x', 'y'], first_tick=1, last_tick=len(points))
        identity = self.plan['units']['scout'][origin]
        rows = [header] + [dict(tick=tick, objects={identity: dict(hp=6000, side=0, type=0, x=x, y=y)})
                           for tick, (x, y) in enumerate(points, 1)]
        path.write_text('\n'.join(json.dumps(row) for row in rows) + '\n')

    def compare(self, clone=None, reference=None, mutate=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write(root / 'reference', reference or self.points, 'reference')
            self.write(root / 'clone', clone or self.points, 'clone')
            if mutate:
                mutate(root / 'clone')
            return TRACE.compare(self.plan, root / 'reference', root / 'clone')

    def test_equal_trace_and_per_unit_metrics(self):
        result = self.compare()
        self.assertEqual(result['status'], 'within_tolerance')
        self.assertIsNone(result['first_divergence'])
        unit = result['units']['scout']
        self.assertEqual(unit['reference']['arrival_tick'], 5)
        self.assertEqual(unit['reference']['path_px'], 4)
        self.assertEqual(unit['clone_reference_path_ratio'], 1)

    def test_first_route_divergence_has_unit_and_tick(self):
        self.plan['tolerance']['shape_px'] = 0.5
        result = self.compare(clone=[(0, 0), (256, 256), *self.points[2:]])
        self.assertEqual(result['first_divergence'], dict(tick=2, unit='scout', cause_class='route_shape'))

    def test_arrival_deadline_and_inclusive_tolerance(self):
        clone = [*self.points[:4], self.points[3], self.points[4]]
        reference = [*self.points, self.points[-1]]
        result = self.compare(clone=clone, reference=reference)
        self.assertEqual(result['first_divergence'], dict(tick=6, unit='scout', cause_class='arrival_timing'))
        self.plan['tolerance']['arrival_ticks'] = 1
        self.assertEqual(self.compare(clone=clone, reference=reference)['status'], 'within_tolerance')

    def test_extra_stall_includes_first_affected_sample(self):
        result = self.compare(clone=[(0, 0)] * 5)
        self.assertEqual(result['first_divergence'], dict(tick=3, unit='scout', cause_class='extra_stall'))
        self.assertIsNone(result['units']['scout']['clone_reference_path_ratio'])

    def test_matching_unreachable_goals_are_censored(self):
        result = self.compare(reference=[(0, 0)] * 5, clone=[(0, 0)] * 5)
        self.assertEqual(result['status'], 'incomplete')
        self.assertIsNone(result['first_divergence'])
        self.assertEqual(result['units']['scout']['clone']['max_idle_ticks'], 4)

    def test_distance_stops_at_first_arrival(self):
        result = self.compare(reference=[*self.points, (0, 0)], clone=[*self.points, (0, 0)])
        self.assertEqual(result['units']['scout']['clone']['path_px'], 4)

    def test_path_ratio_bound(self):
        result = self.compare(clone=[(0, 0), (0, 768), (512, 768), (768, 0), (1024, 0)])
        self.assertEqual(result['first_divergence'], dict(tick=5, unit='scout', cause_class='path_length'))
        self.assertGreater(result['units']['scout']['clone_reference_path_ratio'], 1.25)

    def test_delayed_order_ignores_prior_motion(self):
        self.plan['units']['scout']['order_tick'] = 3
        result = self.compare(reference=[(10000, 0), *self.points[1:]],
                              clone=[(10000, 0), *self.points[1:]])
        self.assertEqual(result['units']['scout']['clone']['path_px'], 2)

    def test_invalid_tail_rejected_even_after_divergence(self):
        def truncate(path):
            path.write_text('\n'.join(path.read_text().splitlines()[:-1]) + '\n')
        with self.assertRaisesRegex(ValueError, 'truncated'):
            self.compare(clone=[(0, 0)] * 5, mutate=truncate)

    def test_missing_and_reused_unit_identity_rejected(self):
        def replace(path):
            rows = [json.loads(line) for line in path.read_text().splitlines()]
            rows[2]['objects'] = {}
            path.write_text('\n'.join(json.dumps(row) for row in rows) + '\n')
        with self.assertRaisesRegex(ValueError, 'missing unit'):
            self.compare(mutate=replace)
        self.plan['units']['other'] = copy.deepcopy(self.plan['units']['scout'])
        with self.assertRaisesRegex(ValueError, 'one-to-one'):
            self.compare()

    def test_wrong_phase_and_nonfinite_tolerance_rejected(self):
        def phase(path):
            path.write_text(path.read_text().replace('before_commands', 'after_commands'))
        with self.assertRaisesRegex(ValueError, 'header'):
            self.compare(mutate=phase)
        self.plan['tolerance']['shape_px'] = float('nan')
        with self.assertRaisesRegex(ValueError, 'tolerance'):
            self.compare()

    def test_custom_map_mission_zero(self):
        self.plan['mission'] = 0
        def mission(path):
            path.write_text(path.read_text().replace('"mission": 1', '"mission": 0'))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for origin in ('reference', 'clone'):
                self.write(root / origin, self.points, origin)
                mission(root / origin)
            self.assertEqual(TRACE.compare(self.plan, root / 'reference', root / 'clone')['status'],
                             'within_tolerance')

    def test_earliest_unit_wins_over_name_order(self):
        self.plan['tolerance']['shape_px'] = 0.5
        self.plan['units']['alpha'] = dict(reference='pool8', clone='2', goal=[1024, 0], order_tick=1)
        def second_unit(path):
            rows = [json.loads(line) for line in path.read_text().splitlines()]
            for row in rows[1:]:
                obj = copy.deepcopy(row['objects']['1'])
                obj['y'] = 256 if row['tick'] >= 4 else 0
                row['objects']['2'] = obj
            path.write_text('\n'.join(json.dumps(row) for row in rows) + '\n')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for origin in ('reference', 'clone'):
                self.write(root / origin, self.points, origin)
            second_unit(root / 'clone')
            rows = [json.loads(line) for line in (root / 'reference').read_text().splitlines()]
            for row in rows[1:]:
                row['objects']['pool8'] = copy.deepcopy(row['objects']['pool7'])
            (root / 'reference').write_text('\n'.join(json.dumps(row) for row in rows) + '\n')
            text = (root / 'clone').read_text()
            rows = [json.loads(line) for line in text.splitlines()]
            rows[2]['objects']['1']['y'] = 256
            (root / 'clone').write_text('\n'.join(json.dumps(row) for row in rows) + '\n')
            result = TRACE.compare(self.plan, root / 'reference', root / 'clone')
            self.assertEqual(result['first_divergence'], dict(tick=2, unit='scout', cause_class='route_shape'))

    def test_cli_invalid_capture_writes_nonpassing_report(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'plan.json').write_text(json.dumps(self.plan))
            (root / 'reference').write_text('{}\n')
            (root / 'clone').write_text('{}\n')
            result = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/zpath_trace.py'),
                                     str(root / 'plan.json'), str(root / 'reference'), str(root / 'clone'),
                                     '--output', str(root / 'report.json')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(json.loads((root / 'report.json').read_text())['status'], 'invalid')

class LogTests(unittest.TestCase):
    def analyze(self, samples, count=1, ticks=4):
        grid, start, goal, _ = GENERATOR.course('open')
        course = dict(name='open', n=count, fog='fog',
                      grid=[''.join(map(str, row)) for row in grid],
                      units=[[6 + i, 26, 0, 0] for i in range(count)],
                      goals={'0': goal}, order_frames={'0': 2}, order_frame=2)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'course.json').write_text(json.dumps(course))
            (root / 'pos.txt').write_text('\n'.join(samples) + '\n')
            return ANALYZER.analyze(root, ticks)

    @staticmethod
    def row(frame, x=6, y=26):
        return f'{frame} abc 4797132 0 {x * 48 + 24} {y * 48 + 24} {x} {y} 1 1'

    def test_arrival_freezes_distance_and_revisits(self):
        first = self.analyze([self.row(1), self.row(2, 80, 30)])
        extended = self.analyze([self.row(1), self.row(2, 80, 30), self.row(3)])
        self.assertEqual(first['summary']['status'], 'ok')
        for key in ('path_px', 'revisits', 'arrived', 'max_idle'):
            self.assertEqual(first['units'][0][key], extended['units'][0][key])
        self.assertEqual(first['summary']['arrived'], 1)

    def test_missing_unit_does_not_shrink_denominator(self):
        summary = self.analyze([self.row(1), self.row(2, 80, 30)], count=2)['summary']
        self.assertEqual(summary['n'], 2)
        self.assertEqual(summary['status'], 'missing_units')
        self.assertNotIn('success', summary)

    def test_truncated_log_is_not_a_loss(self):
        summary = self.analyze([self.row(1), self.row(2)])['summary']
        self.assertEqual(summary['status'], 'truncated')
        self.assertNotIn('lost', summary)

    def test_idle_unit_is_counted_as_stuck(self):
        summary = self.analyze([self.row(i) for i in range(1, 132)], ticks=130)['summary']
        self.assertEqual(summary['status'], 'ok')
        self.assertEqual((summary['lost'], summary['stuck'], summary['never_started']), (1, 1, 1))
        self.assertEqual(summary['max_idle_ticks'], 130)

    def test_distant_campaign_unit_is_rejected(self):
        summary = self.analyze([self.row(1, 36, 42), self.row(5, 36, 42)])['summary']
        self.assertEqual(summary['status'], 'missing_units')

    def test_frame_reset_discards_previous_mission(self):
        summary = self.analyze([self.row(1, 36, 42), self.row(100, 36, 42),
                                self.row(1), self.row(2, 80, 30)])['summary']
        self.assertEqual(summary['status'], 'ok')

class LayoutTests(unittest.TestCase):
    def test_delayed_group_receives_full_observation_budget(self):
        if not BENCHMARK:
            self.skipTest('pass path_bench to check delayed group budgets')
        # The second park order starts at tick 200. Its 450-tick budget must
        # continue past global tick 450, as the reference analyzer does.
        output = subprocess.check_output([BENCHMARK, '--json', '--n', '30', '--ticks', '450', 'park'], text=True)
        result = json.loads(output)
        self.assertEqual(result['arrived'], 30)
        self.assertEqual(result['lost'], 0)
        self.assertEqual(result['ticks'], 450)
        self.assertLess(result['makespan'], 450)

    def test_cpp_and_reference_generator_geometry(self):
        if not BENCHMARK:
            self.skipTest('pass path_bench to check cross-language course geometry')
        output = subprocess.check_output([BENCHMARK, '--describe', '--n', '1'], text=True)
        for line in output.splitlines():
            course = json.loads(line)
            grid, start, goal, _ = GENERATOR.course(course['name'])
            with self.subTest(course=course['name']):
                self.assertEqual(course['grid'], [''.join(map(str, row)) for row in grid])
                self.assertEqual(course['start'], list(start))
                self.assertEqual(course['goal'], list(goal))

class ComparisonTests(unittest.TestCase):
    def test_stalls_and_no_arrivals_are_reported(self):
        # An unreachable order has no arrival tick or distance ratio. It still
        # has useful stuck/idle metrics, and must not crash the Markdown report.
        reference = dict(course='sealed', n=1, fog='explored', ticks=130, status='ok',
                         arrived=0, makespan=None, mean_path_ratio=None, stuck=1, max_idle_ticks=130)
        clone = dict(reference, makespan=-1, mean_path_ratio=0, max_idle_ticks=125)
        invalid = dict(course='sealed', n=1, fog='fog', ticks=130, status='missing_units')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, row in [('valid', reference), ('invalid', invalid)]:
                (root / name).mkdir()
                (root / name / 'summary.json').write_text(json.dumps(row))
            (root / 'clone.jsonl').write_text(json.dumps(clone) + '\n')
            subprocess.run([sys.executable, '-I', str(ROOT / 'tools/zpath_compare.py'),
                            str(root), str(root / 'clone.jsonl'), str(root / 'report')],
                           check=True, capture_output=True, text=True)
            report = (root / 'report_compare.md').read_text()
            self.assertTrue('distance ratio / stuck / max idle' in report)
            self.assertTrue('0/1 / unmeasured / unmeasured / 1 / 130' in report)
            self.assertTrue('0/1 / unmeasured / unmeasured / 1 / 125' in report)
            self.assertTrue('missing_units' in report)

if __name__ == '__main__':
    BENCHMARK = sys.argv.pop(1) if len(sys.argv) > 1 else None
    unittest.main()
