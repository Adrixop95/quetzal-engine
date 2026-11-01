#!/usr/bin/env python3
"""Synthetic capture contracts, isolation failures, fail-closed front-end scenarios and frame input."""
import copy
import csv
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
ENGINE = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None
module = importlib.util.spec_from_file_location('capture', ROOT / 'tools/ui_capture.py')
capture = importlib.util.module_from_spec(module)
module.loader.exec_module(capture)


class CaptureTests(unittest.TestCase):
    def test_catalogue_and_type_coverage(self):
        rows = capture.validate(capture.load(capture.CATALOGUE))
        ids = {row['id'] for row in rows}
        with (ROOT / 'docs/data/types.csv').open() as stream:
            for row in csv.DictReader(stream):
                self.assertTrue('command-card-' + row['ugi'].lower().replace('_', '-') in ids)
        for n in range(1, 6):
            self.assertTrue(f'hud-mission-{n}' in ids)
        for button in ('campaign', 'custom-map', 'options', 'quit', 'new-game', 'load', 'multiplayer', 'credits'):
            for state in ('normal', 'hover', 'pressed'):
                self.assertTrue(f'main-{button}-{state}' in ids)
        spec = capture.load(capture.CATALOGUE)
        spec['screens'][0]['id'] = '../escape'
        with self.assertRaises(ValueError): capture.validate(spec)
        spec = capture.load(capture.CATALOGUE)
        spec['screens'][0]['runners']['clone'] = {'mission': None, 'input': [['wait_ms', 100]]}
        with self.assertRaises(ValueError): capture.validate(spec)

    def test_explicit_frame_schedule(self):
        route = {'mission': None, 'input': [['wait', 3], ['move', 140, 239], ['down'], ['up'], ['key', 'Escape']]}
        ref, clone = capture.scripts(route, Path('/tmp/frame.png'))
        self.assertEqual(ref, '3 move 140 239\n4 down\n5 up\n6 keydown 1\n7 keyup 1\n8 shot\n9 quit\n')
        self.assertEqual(clone, 'wait 3\nmove 140 239\ndown\nup\nkey Escape\nwait 1\nshot /tmp/frame.png\nquit\n')

    def test_lossless_metrics(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'frame.png'
            px = bytes([0, 10, 255, 30, 40, 50])
            path.write_bytes(capture.png_bytes(2, 1, px))
            decoded = capture.image_file(path)
            self.assertEqual(decoded, (2, 1, 3, px))
            changed = (2, 1, 3, bytes([0, 10, 252, 30, 40, 50]))
            result = capture.metrics(decoded, changed)
            self.assertEqual(result['different_pixels'], 1)
            self.assertEqual(result['mae'], 0.5)
            self.assertEqual(capture.metrics(decoded, decoded)['different_pixels'], 0)

    def test_failed_repeats_are_not_determinism(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            game = root / 'source'
            game.mkdir()
            binary = root / 'binary'
            binary.touch()
            args = type('Args', (), dict(game=game, runtime=binary, repeats=2, timeout=3))()
            row = dict(runners={'reference': dict(mission=None, input=[])})
            with patch.object(capture, 'execute', side_effect=RuntimeError('namespace denied')) as execute:
                result = capture.capture(args, row, 'reference', root / 'output')
            self.assertEqual(result['status'], 'capture_failed')
            self.assertEqual(len(result['errors']), 2)
            self.assertTrue(execute.call_args.kwargs['setup_network'])
            self.assertFalse((root / 'output/reference.png').exists())

    def test_unmeasured_phases_remain_null(self):
        private = ROOT / 'work/parity'
        private.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=private) as temp:
            root = Path(temp)
            spec = root / 'spec.json'
            row = dict(id='missing-screen', reach='Unavailable route.', evidence='P',
                       runners={name: {'blocked': 'Missing observer.'} for name in ('reference', 'clone')})
            capture.write_json(spec, dict(schema=1, screens=[row]))
            args = type('Args', (), dict(spec=spec, screen=None, all=True, output=root / 'out', phase='after'))()
            self.assertEqual(capture.run(args), 1)
            self.assertEqual(capture.load(args.output / 'missing-screen/after.json')['difference'], None)
            import json
            comparison = json.loads((args.output / 'before-after.json').read_text())
            self.assertEqual(comparison, [dict(id='missing-screen', before=None, after=None)])

    @unittest.skipUnless(ENGINE, 'engine path required')
    def test_frontend_navigation_and_repeatability(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            resources = root / 'Res'
            resources.mkdir()
            (root / 'Maps').mkdir()
            for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'common', 'fnts', 'light'):
                (resources / f'{name}.xbe').write_bytes(struct.pack('<I', 0))
            script = root / 'input.txt'
            before, after = root / 'main.png', root / 'options.png'
            script.write_text(f'wait 2\nshot {before}\nmove 400 370\ndown\nup\nwait 2\nshot {after}\nquit\n')
            env = {k: v for k, v in os.environ.items() if k not in ('AZTEC_MENU_SHOT', 'AZTEC_UI_SCRIPT')}
            env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', AZTEC_UI_SCRIPT=str(script))
            argv = [str(ENGINE), '--game', str(root), '--rules-dir', str(ROOT / 'docs/data'),
                    '--size', '800x600', '--software', '--no-audio', '--no-mouse-grab']
            frames = []
            for _ in range(2):
                run = subprocess.run(argv, env=env, capture_output=True, timeout=10)
                self.assertEqual(run.returncode, 0, run.stderr)
                self.assertEqual(capture.image_file(before)[:3], (800, 600, 3))
                frames.append((before.read_bytes(), after.read_bytes()))
            self.assertEqual(frames[0], frames[1])
            self.assertNotEqual(frames[0][0], frames[0][1])


class FrontendCaptureTests(unittest.TestCase):
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
            self.assertEqual(capture.run_frontend(args), 1)
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
