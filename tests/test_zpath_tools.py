#!/usr/bin/env python3 -I
"""Synthetic reference-log checks; no game data or executable required."""
import importlib.util
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

if __name__ == '__main__':
    BENCHMARK = sys.argv.pop(1) if len(sys.argv) > 1 else None
    unittest.main()
