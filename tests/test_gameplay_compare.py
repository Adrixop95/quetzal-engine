#!/usr/bin/env python3
"""Synthetic timeline, cadence and isolation controls; no original assets."""
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
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
ENGINE = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None
module = importlib.util.spec_from_file_location('gameplay', ROOT / 'tools/gameplay_compare.py')
gameplay = importlib.util.module_from_spec(module)
module.loader.exec_module(gameplay)


class TimelineTests(unittest.TestCase):
    def test_route_validation_and_non_consuming_captures(self):
        spec = gameplay.scenario(gameplay.SPEC)
        script = gameplay.clone_script(spec)
        self.assertTrue(script.endswith('until 7200\nquit\n'))
        self.assertNotIn('shot', script)
        frame = 0
        applied = []
        for line in script.splitlines():
            command, *args = line.split()
            if command == 'wait':
                frame += int(args[0])
            elif command not in ('until', 'quit'):
                applied.append(frame)
                frame += 1
        self.assertEqual(applied, [e['frame'] for e in spec['input']])

    def test_first_divergence_grouping_and_late_corruption(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            spec = gameplay.load(ROOT / 'tests/parity/visual-ui-synthetic.json')
            scenario = root / 'scenario.json'
            gameplay.write_json(scenario, spec)
            (root / 'frame.ppm').write_bytes(b'P6\n12 8\n255\n' + bytes(288))
            masks = {}
            for index, region in enumerate(gameplay.compare.__globals__['REGIONS']):
                masks[region] = region + '.pgm'
                (root / masks[region]).write_bytes(b'P5\n12 8\n255\n' + bytes(255 if i % 6 == index else 0 for i in range(96)))
            state = dict(buttons={'move': dict(frame=0, enabled=True, highlight=False)},
                         cursor='arrow', tooltip='', menu='closed', target_mode='none')
            bundle = dict(schema=1, scenario=spec['id'], contract='invented-v1',
                          alignment=spec['alignment'], samples=[dict(**s, state=copy.deepcopy(state),
                          commands=[], image='frame.ppm', masks=masks) for s in spec['observe']])
            reference, clone = root / 'reference.json', root / 'clone.json'
            gameplay.write_json(reference, bundle)
            gameplay.write_json(clone, bundle)
            self.assertIsNone(gameplay.compare_timeline(scenario, reference, clone)['first'])
            changed = bytearray(288)
            changed[0] = 255
            (root / 'changed.ppm').write_bytes(b'P6\n12 8\n255\n' + changed)
            bundle['samples'][0]['image'] = 'changed.ppm'
            bundle['samples'][0]['state']['menu'] = 'pause'
            bundle['samples'][0]['commands'] = [dict(name='move', arguments={})]
            gameplay.write_json(clone, bundle)
            report = gameplay.compare_timeline(scenario, reference, clone)
            self.assertEqual(report['first']['sample_index'], 0)
            self.assertEqual(set(report['triage']), {'visual_pixels', 'ui_sequence', 'commands'})
            self.assertTrue(all(row['attribution'] == 'unverified' for row in report['triage'].values()))
            bundle['samples'][-1]['image'] = 'missing.ppm'
            gameplay.write_json(clone, bundle)
            with self.assertRaises(OSError):
                gameplay.compare_timeline(scenario, reference, clone)

    def test_failure_is_not_a_difference_and_drafts_preserve_edits(self):
        private = ROOT / 'work/parity/ui'
        private.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=private) as temp:
            output = Path(temp) / 'out'
            args = type('Args', (), dict(spec=gameplay.SPEC, output=output,
                        game=Path(temp), clone=Path(temp), runtime=Path(temp), timeout=1, every=120))()
            with patch.object(gameplay, 'private_game'), \
                    patch.object(gameplay, 'execute', side_effect=RuntimeError('namespace denied')), \
                    patch.object(gameplay, 'clone_record', side_effect=RuntimeError('failed')), \
                    patch.object(gameplay, 'draft'):
                self.assertEqual(gameplay.record(args), 1)
            rows = json.loads((output / 'report.json').read_text())
            self.assertEqual(len(rows), 3)
            self.assertTrue(all(row['difference'] is None for row in rows))
            self.assertTrue(all(row['runners']['reference']['status'] == 'experiment_failed' for row in rows))
            drafts = Path(temp) / 'drafts'
            gameplay.draft(drafts, 'blocked_reference', dict(reason='first'))
            path = drafts / 'gameplay-blocked_reference.md'
            path.write_text('Human edits.\n')
            gameplay.draft(drafts, 'blocked_reference', dict(reason='second'))
            self.assertEqual(path.read_text(), 'Human edits.\n')

    def test_timeout_prefix_is_never_complete(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            args = type('Args', (), dict(clone=root / 'binary', every=2, timeout=1))()
            def failed_process(*args, **kwargs):
                rows = [dict(contract='engine-ui-raw-v1', frame=i, tick=i + 1,
                             screenshot='' if i == 0 else str(root / 'captures/tick-2.png'), outcome=0)
                        for i in range(2)]
                (root / 'ui.jsonl').write_text(''.join(json.dumps(row) + '\n' for row in rows))
                raise RuntimeError('timeout')
            with patch.object(gameplay, 'execute', side_effect=failed_process), \
                    patch.object(gameplay, 'image_file', return_value=(800, 600, 3, b'')):
                report = gameplay.clone_record(args, gameplay.scenario(gameplay.SPEC), root, 1)
            self.assertEqual(report['status'], 'incomplete_clone_recording')
            self.assertFalse(report['complete'])
            self.assertEqual(report['last_tick'], 2)
            self.assertEqual(report['process_failure'], 'timeout')

    @unittest.skipUnless(ENGINE, 'engine path required')
    def test_periodic_capture_across_pause_and_until(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            game = root / 'game'
            maps, resources = game / 'Maps', game / 'Res'
            maps.mkdir(parents=True)
            resources.mkdir()
            captures = root / 'captures'
            captures.mkdir()
            for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'common', 'fnts', 'light'):
                (resources / f'{name}.xbe').write_bytes(struct.pack('<I', 0))
            (maps / 'mission_1.mp').write_text('[Globals]\nMapSize=32,32\nSides=1\nTeams=1\n'
                '[Side0]\nStart_X=8\nStart_Y=8\n[Team0]\nSide=0\nUnits=1\nUnit0=8,8,0,100\n')
            (maps / 'mission_1.map').write_bytes(struct.pack('<HH', 32, 32) + bytes(32 * 32 * 4))
            script = root / 'input.txt'
            script.write_text('wait 2\nkey Escape\nwait 4\nkey Escape\nuntil 12\nquit\n')
            trace = root / 'ui.jsonl'
            argv = [str(ENGINE), '--game', str(game), '1', '--rules-dir', str(ROOT / 'docs/data'),
                    '--size', '800x600', '--software', '--no-audio', '--no-light', '--no-fog',
                    '--menu-session', '--script', str(script), '--ui-trace', str(trace),
                    '--capture-dir', str(captures), '--capture-every', '2']
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', AZTEC_USER_DIR=str(root / 'profile'))
            runs = []
            for _ in range(2):
                result = subprocess.run(argv, env=env, capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stderr)
                rows = [json.loads(line) for line in trace.read_text().splitlines()]
                observed = [r for r in rows if r['screenshot']]
                self.assertEqual([r['tick'] for r in observed], [2, 4, 6, 8, 10, 12])
                self.assertGreater(sum(r['tick'] == 2 for r in rows), 1)
                self.assertGreaterEqual(rows[-1]['tick'], 12)
                self.assertTrue(all(gameplay.image_file(Path(r['screenshot']))[:3] == (800, 600, 3) for r in observed))
                runs.append((trace.read_bytes(), [Path(r['screenshot']).read_bytes() for r in observed]))
            self.assertEqual(runs[0], runs[1])
            for bad in ('0', '-1', '1oops', '9999999999999999999999999'):
                result = subprocess.run([*argv[:-1], bad], env=env, capture_output=True, timeout=5)
                self.assertNotEqual(result.returncode, 0)
            # Stop at the first terminal result instead of applying later route
            # clicks to the result menu, which could restart and truncate the trace.
            (maps / 'mission_1.scn').write_text('trigger(0) conditions localTime == 10 actions side(local).defeat end\n')
            script.write_text('until 7200\nquit\n')
            result = subprocess.run(argv, env=env, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stderr)
            rows = [json.loads(line) for line in trace.read_text().splitlines()]
            self.assertEqual(rows[-1]['tick'], 10)
            self.assertEqual(rows[-1]['outcome'], 2)
            (maps / 'mission_1.scn').unlink()
            script.write_text('until 0\nquit\n')
            result = subprocess.run(argv, env=env, capture_output=True, timeout=5)
            self.assertEqual(result.returncode, 3)


if __name__ == '__main__':
    unittest.main()
