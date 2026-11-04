#!/usr/bin/env python3
"""Invented rasters exercise exact, tolerant and unavailable UI comparisons."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[1]
module = importlib.util.spec_from_file_location('ui_diff', ROOT / 'tools/ui_diff.py')
diff = importlib.util.module_from_spec(module)
module.loader.exec_module(diff)


class DiffTests(unittest.TestCase):
    def setUp(self):
        self.spec = diff.load(ROOT / 'tests/parity/ui/diff-synthetic.json')
        self.data = bytearray(8 * 4 * 3)
        self.data[3:6] = bytes((1, 2, 3))

    def image(self, data=None):
        return 8, 4, 3, bytes(self.data if data is None else data)

    def test_exact_and_lossless_diff(self):
        result, heat = diff.compare(self.image(), self.image(), self.spec)
        self.assertEqual(result['status'], 'pixel_pass')
        self.assertEqual(result['alignment_shift'], [0, 0])
        self.assertEqual(sum(r['different_pixels'] for r in result['regions']), 0)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'diff.png'
            path.write_bytes(heat)
            self.assertEqual(diff.image_file(path)[3], bytes(96))

    def test_one_channel_static_difference_and_text_candidates(self):
        data = self.data.copy()
        data[3 * 11] = 1
        data[3 * 12] = 1
        result, _ = diff.compare(self.image(), self.image(data), self.spec)
        self.assertEqual(result['status'], 'pixel_fail')
        art, text = result['regions'][:2]
        self.assertEqual(art['different_pixels'], 1)
        self.assertEqual(art['rectangles'][0]['rect'], [3, 1, 1, 1])
        self.assertTrue('wrong text' in text['rectangles'][0]['cause_candidates'])
        self.assertEqual(result['cause_evidence'], 'candidates_only')

    def test_dynamic_tolerance_boundary(self):
        data = self.data.copy()
        data[3 * 16] = 10
        result, _ = diff.compare(self.image(), self.image(data), self.spec)
        self.assertEqual(result['status'], 'pixel_pass')
        self.assertEqual(result['regions'][2]['bad_pixels'], 1)
        data[3 * 17] = 10
        result, _ = diff.compare(self.image(), self.image(data), self.spec)
        self.assertEqual(result['status'], 'pixel_fail')

    def test_translation_cannot_hide_layout_or_clipping(self):
        data = bytearray(96)
        for y in range(4):
            for x in range(7):
                data[3 * (y * 8 + x + 1):3 * (y * 8 + x + 2)] = self.data[3 * (y * 8 + x):3 * (y * 8 + x + 1)]
        result, _ = diff.compare(self.image(), self.image(data), self.spec)
        self.assertEqual(result['alignment_shift'], [1, 0])
        self.assertEqual(result['status'], 'pixel_fail')
        self.assertEqual(sum(r['clipped_pixels'] for r in result['regions']), 4)
        self.assertTrue('wrong offset' in result['regions'][1]['rectangles'][0]['cause_candidates'])

    def test_ambiguous_or_missing_anchor_fails(self):
        image = self.image(bytes(96))
        result, _ = diff.compare(image, image, self.spec)
        self.assertEqual(result['status'], 'pixel_fail')
        self.assertIsNone(result['alignment_shift'])
        result, _ = diff.compare(self.image(), image, self.spec)
        self.assertEqual(result['alignment_status'], 'missing_or_ambiguous_anchor')

    def test_partition_and_tolerances_fail_closed(self):
        for change in ('hole', 'overlap', 'static_tolerance', 'dynamic_tolerance', 'dynamic_anchor'):
            spec = copy.deepcopy(self.spec)
            if change == 'hole': spec['regions'].pop()
            if change == 'overlap': spec['regions'][1]['rect'][0] = 3
            if change == 'static_tolerance': spec['regions'][0]['tolerance'] = {}
            if change == 'dynamic_tolerance': spec['regions'][2]['tolerance']['max_mae'] = float('nan')
            if change == 'dynamic_anchor': spec['alignment']['anchors'] = [[0, 2, 1, 1]]
            with self.subTest(change=change), self.assertRaises(ValueError):
                diff.compare(self.image(), self.image(), spec)

    def test_components_do_not_wrap_rows_and_report_is_bounded(self):
        boxes = diff.rectangles([7, 8, 9, 17], 8)
        self.assertEqual(len(boxes), 2)
        self.assertEqual(sum(b['different_pixels'] for b in boxes), 4)
        boxes = diff.rectangles(range(0, 4000, 2), 4000)
        self.assertEqual(len(boxes), 256)
        self.assertTrue(boxes[-1]['merged'])
        self.assertEqual(sum(b['different_pixels'] for b in boxes), 2000)

    def test_all_cause_names_remain_candidates(self):
        classes = diff.candidates('text', [(b'abc', b'def')], False)
        self.assertTrue('wrong palette' in classes)
        self.assertTrue(set(classes) <= set(diff.CAUSES))
        self.assertNotIn('wrong palette', diff.candidates('static_art', [(b'abc', b'def'), (b'abc', b'ghi')], False))

    def test_capture_failure_is_unmeasured_even_with_stale_images(self):
        private = ROOT / 'work/parity/ui'
        private.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=private) as temp:
            root = Path(temp)
            phase = root / 'captures/example/before'
            phase.mkdir(parents=True)
            diff.write_json(phase / 'report.json', dict(id='example', phase='before', runners={
                'reference': {'status': 'capture_failed'}, 'clone': {'status': 'repeatable_capture'}}))
            for runner in ('reference', 'clone'):
                (phase / (runner + '.png')).write_bytes(diff.png_bytes(8, 4, self.data))
            row = dict(id='example', reach='Synthetic screen.', evidence='P', comparison=self.spec,
                       runners={r: {'blocked': 'Synthetic fixture.'} for r in ('reference', 'clone')})
            diff.write_json(root / 'spec.json', dict(schema=1, screens=[row]))
            args = SimpleNamespace(spec=root / 'spec.json', screen=None, phase='before',
                                   captures=root / 'captures', output=root / 'diff')
            self.assertEqual(diff.run(args), 1)
            report = json.loads((args.output / 'report.json').read_text())['screens'][0]
            self.assertEqual(report['status'], 'unmeasured')
            self.assertIsNone(report['regions'])
            self.assertFalse(list(args.output.glob('*.png')))
            diff.write_json(phase / 'report.json', dict(id='example', phase='before', runners={
                r: {'status': 'repeatable_capture'} for r in ('reference', 'clone')}))
            args.output = root / 'measured'
            self.assertEqual(diff.run(args), 0)
            measured = json.loads((args.output / 'report.json').read_text())['screens'][0]
            self.assertEqual(measured['status'], 'pixel_pass')
            self.assertEqual(diff.image_file(args.output / 'example-diff.png')[3], bytes(96))


if __name__ == '__main__':
    unittest.main()
