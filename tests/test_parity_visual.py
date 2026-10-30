#!/usr/bin/env python3
"""Invented rasters and real independent UI handlers; no game installation."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
ENGINE = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None
spec = importlib.util.spec_from_file_location('visual', ROOT / 'tools/parity_visual.py')
visual = importlib.util.module_from_spec(spec)
spec.loader.exec_module(visual)


class VisualTests(unittest.TestCase):
    def setUp(self):
        private = ROOT / 'work/parity'
        private.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=private)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.scenario = ROOT / 'tests/parity/visual-ui-synthetic.json'
        self.spec = visual.load(self.scenario)
        self.reference = self.root / 'reference.json'
        self.clone = self.root / 'clone.json'
        self.image = bytes([20, 40, 60] * 96)
        (self.root / 'frame.ppm').write_bytes(b'P6\n12 8\n255\n' + self.image)
        masks = {}
        for index, region in enumerate(visual.REGIONS):
            masks[region] = region + '.pgm'
            mask = bytes(255 if i % 6 == index else 0 for i in range(96))
            (self.root / masks[region]).write_bytes(b'P5\n12 8\n255\n' + mask)
        state = dict(buttons={'move': dict(frame=0, enabled=True, highlight=False)},
                     cursor='arrow', tooltip='', menu='closed', target_mode='none')
        self.bundle = dict(schema=1, scenario=self.spec['id'], contract='invented-v1',
                           alignment=self.spec['alignment'], samples=[dict(**s, state=copy.deepcopy(state),
                           commands=[], image='frame.ppm', masks=masks) for s in self.spec['observe']])
        self.write(self.reference, self.bundle)
        self.write(self.clone, self.bundle)

    def write(self, path, data):
        path.write_text(json.dumps(data))

    def compare(self):
        return visual.compare(self.scenario, self.reference, self.clone)

    def test_equal_and_region_metrics(self):
        result = self.compare()
        self.assertIsNone(result['first'])
        self.assertEqual(set(result['samples'][0]['regions']), set(visual.REGIONS))
        self.assertEqual(result['samples'][0]['regions']['map']['pixels'], 16)

    def alter_pixels(self, delta, count=1):
        pixels = bytearray(self.image)
        for i in range(count):
            pixels[i * 18] += delta
        (self.root / 'other.ppm').write_bytes(b'P6\n12 8\n255\n' + pixels)
        self.bundle['samples'][0]['image'] = 'other.ppm'
        self.write(self.clone, self.bundle)

    def test_tolerance_reports_exact_separately(self):
        self.alter_pixels(3)
        result = self.compare()
        self.assertIsNone(result['first'])
        self.assertGreater(result['samples'][0]['regions']['map']['exact_mismatch_fraction'], 0)
        self.assertEqual(result['samples'][0]['regions']['map']['bad_fraction'], 0)

    def test_local_pixel_failure_and_mae(self):
        self.alter_pixels(80, 2)
        first = self.compare()['first']
        self.assertEqual((first['tick'], first['field'], first['category']), (0, 'map', 'visual_pixels'))
        self.alter_pixels(120)
        self.assertIsNotNone(self.compare()['first'])  # MAE fails even below bad-pixel fraction.

    def test_structural_shift_cannot_hide_pixels(self):
        mask = bytearray((self.root / 'command_card.pgm').read_bytes())
        mask[-2], mask[-1] = mask[-1], mask[-2]
        (self.root / 'shift.pgm').write_bytes(mask)
        self.bundle['samples'][0]['masks'] = dict(self.bundle['samples'][0]['masks'], command_card='shift.pgm')
        self.write(self.clone, self.bundle)
        self.assertEqual(self.compare()['first']['category'], 'visual_structure')

    def test_ui_state_and_command_sequence(self):
        for field, value in [('cursor', 'attack'), ('tooltip', 'Move'), ('menu', 'open'), ('target_mode', 'move'),
                             ('buttons', {'move': dict(frame=1, enabled=True, highlight=True)})]:
            changed = copy.deepcopy(self.bundle)
            changed['samples'][1]['state'][field] = value
            self.write(self.clone, changed)
            self.assertEqual(self.compare()['first']['field'], field)
        self.bundle['samples'][2]['commands'] = [dict(name='move', arguments=dict(unit='unit:a', x=100, y=100))]
        self.write(self.clone, self.bundle)
        self.assertEqual(self.compare()['first']['field'], 'commands')

    def test_full_horizon_and_alignment_validation(self):
        for change in ('truncate', 'extra', 'tick', 'contract', 'alignment', 'missing_state', 'missing_mask', 'path', 'null_row', 'nonfinite'):
            changed = copy.deepcopy(self.bundle)
            if change == 'truncate': changed['samples'].pop()
            if change == 'extra': changed['samples'].append(changed['samples'][-1])
            if change == 'tick': changed['samples'][-1]['tick'] = 80
            if change == 'contract': changed['contract'] = 'other'
            if change == 'alignment': changed['alignment']['camera'][0] += 1
            if change == 'missing_state': del changed['samples'][-1]['state']['cursor']
            if change == 'missing_mask': changed['samples'][-1]['masks'] = {}
            if change == 'path': changed['samples'][-1]['image'] = '../outside.ppm'
            if change == 'null_row': changed['samples'][-1] = None
            if change == 'nonfinite': changed['alignment']['money'] = float('nan')
            changed['samples'][0]['state']['cursor'] = 'attack'
            self.write(self.clone, changed)
            with self.subTest(change=change), self.assertRaises(ValueError): self.compare()

    def test_invalid_rasters_and_thresholds(self):
        path = self.root / 'bad.ppm'
        for payload in (b'P6\n12 8\n255\n', b'P5\n1 1\n255\n\x00extra', b'unknown'):
            path.write_bytes(payload)
            with self.assertRaises(ValueError): visual.image_file(path)
        # Independently encoded 2x2 RGB rasters exercise every PNG predictor.
        pixels = bytes([10, 20, 30, 30, 40, 50, 50, 60, 70, 70, 80, 90])
        filtered = [([10, 20, 30, 30, 40, 50], [50, 60, 70, 70, 80, 90]),
                    ([10, 20, 30, 20, 20, 20], [50, 60, 70, 20, 20, 20]),
                    ([10, 20, 30, 30, 40, 50], [40, 40, 40, 40, 40, 40]),
                    ([10, 20, 30, 25, 30, 35], [45, 50, 55, 30, 30, 30]),
                    ([10, 20, 30, 20, 20, 20], [40, 40, 40, 20, 20, 20])]

        def chunk(kind, data):
            return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))

        for filter_id, (first, second) in enumerate(filtered):
            raw = bytes([filter_id, *first, filter_id, *second])
            encoded = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 2, 0, 0, 0))
                       + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
            path.write_bytes(encoded)
            self.assertEqual(visual.image_file(path), (2, 2, 3, pixels))
        path.write_bytes(encoded[:-1] + bytes([encoded[-1] ^ 1]))
        with self.assertRaises(ValueError): visual.image_file(path)
        self.spec['regions']['map']['max_mae'] = float('nan')
        with self.assertRaises(ValueError): visual.validate_spec(self.spec)
        with self.assertRaises(ValueError): visual.private_output(ROOT / 'docs/private.json')

    def test_missing_private_resources_skip(self):
        args = type('Args', (), dict(output=self.root / 'skip', game=self.root / 'absent',
                    runtime=self.root / 'missing', clone=self.root / 'missing'))()
        self.assertEqual(visual.probe(args), 77)
        self.assertEqual(visual.load(args.output / 'report.json')['category'], 'skipped')

    def test_reference_isolation_failure_propagates(self):
        # No process is launched; assert exact isolation policy and fail-closed result.
        with patch.object(visual.execute.__globals__['subprocess'], 'run') as run:
            run.return_value.returncode = 1
            with self.assertRaises(RuntimeError):
                visual.execute(['runtime', 'game/Aztec.exe'], self.root, 3)
            argv = run.call_args.args[0]
            self.assertTrue('unshare' in argv)
            self.assertTrue('-rn' in argv)
            self.assertTrue('ip link set lo up' in argv[8])
            self.assertEqual(run.call_args.kwargs['env']['SDL_VIDEODRIVER'], 'dummy')
            self.assertEqual(run.call_args.kwargs['env']['SDL_AUDIODRIVER'], 'dummy')

    @unittest.skipUnless(ENGINE, 'engine path required')
    def test_engine_input_trace_and_png(self):
        game = self.root / 'game'
        resources = game / 'Res'
        resources.mkdir(parents=True)
        maps = game / 'Maps'
        maps.mkdir()
        for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'fnts', 'light'):
            (resources / f'{name}.xbe').write_bytes(struct.pack('<I', 0))

        def sprite(w, h, color):
            return struct.pack('<6I', 0xe0ac0000, w | (h << 16), 0x11, 14, w | (h << 16), 0) + struct.pack('<H', color) * (w * h)

        items = {292: sprite(204, 493, 0xf800), 297: sprite(254, 208, 0x07e0)}
        for index in (275, 257, 268, 301, 288, 269): items[index] = sprite(40, 40, 0xffff)
        count = max(items) + 1
        archive = bytearray(struct.pack('<5I', 1, 0x44c, 20, 0, count) + bytes(count * 4))
        for index in range(count):
            struct.pack_into('<I', archive, 20 + index * 4, len(archive))
            archive.extend(items.get(index, b''))
        (resources / 'common.xbe').write_bytes(archive)
        (maps / 'mission_1.mp').write_text('[Globals]\nMapSize=32,32\nSides=1\nTeams=1\n'
            '[Side0]\nStart_X=8\nStart_Y=8\n[Team0]\nSide=0\nUnits=1\nUnit0=8,8,0,100\n')
        (maps / 'mission_1.map').write_bytes(struct.pack('<HH', 32, 32) + bytes(32 * 32 * 4))
        script = self.root / 'input.txt'
        capture = self.root / 'engine.png'
        script.write_text((ROOT / 'tests/parity/ui-input.txt').read_text().replace('quit', f'shot {capture}\nquit'))
        trace = self.root / 'ui.jsonl'
        argv = [str(ENGINE), '--game', str(game), '1', '--rules-dir', str(ROOT / 'docs/data'),
                '--size', '800x600', '--software', '--no-audio', '--no-light', '--no-fog', '--no-hud',
                '--cam', '8,8', '--zoom', '1', '--select', '0,0,800,600', '--no-edge-scroll',
                '--script', str(script), '--ui-trace', str(trace)]
        result = subprocess.run(argv, env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'),
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = [json.loads(line) for line in trace.read_text().splitlines()]
        self.assertTrue(rows[0]['buttons'][0]['hover'])
        self.assertEqual(rows[1]['target_mode'], 0)  # Press captures Move; release activates it.
        self.assertTrue(rows[1]['left_down'])
        self.assertFalse(rows[2]['left_down'])
        self.assertEqual(rows[2]['target_mode'], 1)
        self.assertEqual(rows[4]['target_mode'], 0)
        self.assertEqual(rows[4]['commands'][0]['type'], 0x17)
        self.assertEqual(rows[4]['commands'][0]['ids'], rows[0]['selection'])
        self.assertEqual(rows[6]['cursor']['shape'], 'attack')
        self.assertEqual(rows[8]['commands'][0]['type'], 0x51)
        self.assertEqual(rows[11]['target_mode'], 0)
        self.assertEqual(visual.image_file(capture)[:3], (800, 600, 3))
        original = trace.read_text()
        result = subprocess.run(argv, env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'),
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(original, trace.read_text())


if __name__ == '__main__':
    unittest.main()
