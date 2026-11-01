#!/usr/bin/env python3
"""Synthetic capture contracts, isolation failures and front-end frame input."""
import csv
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
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


if __name__ == '__main__':
    unittest.main()
