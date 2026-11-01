#!/usr/bin/env python3
"""Fail-closed front-end capture controls with invented pixels and runners."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import ui_capture as capture


class CaptureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.scenario = ROOT / 'tests/parity/ui/frontend.json'

    def test_bad_schedules_and_capture_paths(self):
        original = json.loads(self.scenario.read_text())
        for change in ('negative', 'backwards', 'escape', 'duplicate', 'newline', 'count'):
            spec = copy.deepcopy(original)
            if change == 'negative': spec['clone'][0]['frame'] = -1
            if change == 'backwards': spec['clone'][-1]['frame'] = 0
            if change == 'escape': spec['clone'][0]['args'] = ['../public']
            if change == 'duplicate': spec['clone'].insert(0, spec['clone'][0])
            if change == 'newline': spec['reference']['input'][0] = 'wait 1\nquit'
            if change == 'count': spec['reference']['screens'].pop()
            path = self.root / 'invalid.json'
            path.write_text(json.dumps(spec))
            with self.subTest(change=change), self.assertRaises(ValueError):
                capture.load_spec(path)

    def test_numeric_difference_and_resolution(self):
        left, right = self.root / 'a.ppm', self.root / 'b.ppm'
        left.write_bytes(b'P6\n2 1\n255\n' + bytes([0] * 6))
        right.write_bytes(b'P6\n2 1\n255\n' + bytes([0, 0, 0, 30, 0, 0]))
        result = capture.pixels(left, right)
        self.assertEqual((result['changed_pixels'], result['mae']), (1, 5))
        right.write_bytes(b'P6\n1 1\n255\n' + bytes([0] * 3))
        with self.assertRaises(ValueError):
            capture.pixels(left, right)

    def test_unaligned_captures_cannot_pass_parity(self):
        output = self.root / 'run'
        raster = b'P6\n1 1\n255\n' + bytes([0] * 3)
        source = self.root / 'clone.ppm'
        source.write_bytes(raster)
        def reference(argv, directory, timeout, extra):
            for i in range(1, 4):
                (directory / f'shot_{i:02d}.ppm').write_bytes(raster)
        args = SimpleNamespace(scenario=self.scenario, output=output, game=self.root,
                               runtime=source, timeout=1)
        with patch.object(capture, 'private_output', return_value=output), \
                patch.object(capture, 'private_game'), patch.object(capture, 'execute', side_effect=reference), \
                patch.object(capture, 'capture_clone', return_value=({'campaign': source}, [])):
            self.assertEqual(capture.run(args), 1)
        result = json.loads((output / 'report.json').read_text())
        self.assertEqual(result['category'], 'unmeasured_reference_parity')
        self.assertEqual(result['paired_pixels'], {})
        self.assertEqual(result['audio'], 'unobserved')
        self.assertEqual(result['unaligned_exploratory_pixels']['campaign']['changed_pixels'], 0)

    def test_truncated_route_is_rejected(self):
        spec = capture.load_spec(self.scenario)
        def engine(argv, directory, timeout, setup_network=False):
            (directory / 'trace.jsonl').write_text(json.dumps(dict(contract='engine-menu-raw-v1', frame=0)) + '\n')
        args = SimpleNamespace(game=self.root, clone=self.root / 'engine', timeout=1)
        with patch.object(capture, 'private_game'), patch.object(capture, 'execute', side_effect=engine):
            with self.assertRaisesRegex(ValueError, 'horizon'):
                capture.capture_clone(args, spec, self.root / 'short')


if __name__ == '__main__':
    unittest.main()
