"""Campaign collector controls with invented files and a simulated process launcher."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('save_roundtrip', SOURCE / 'tools/save_roundtrip.py')
collector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collector)


class CollectorTests(unittest.TestCase):
    def test_campaign_and_negative_controls(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game, rules, orders = (root / name for name in ('game', 'rules', 'orders'))
            for path in (game / 'Maps', game / 'Res', rules, orders):
                path.mkdir(parents=True)
            engine = root / 'engine'
            engine.write_bytes(b'invented executable placeholder')
            for mission in range(1, 16):
                (game / f'Maps/mission_{mission}.mp').write_text('invented mission', encoding='ascii')
                (orders / f'mission_{mission}.orders').write_text('1 move 0 1 1\n', encoding='ascii')
            calls = []
            control = 'match'

            def run(args, mission, private, directory, *, save_tick=None, resume=None):
                self.assertEqual(os.environ['AZTEC_USER_DIR'], str(directory / 'profile'))
                self.assertNotEqual(private, game)
                self.assertFalse((private / 'Maps').is_symlink())
                self.assertFalse((private / 'Saves').is_symlink())
                self.assertTrue((private / 'Res').is_symlink())
                directory.mkdir()
                calls.append((mission, save_tick, resume))
                end = args.max_ticks - (control == 'early')
                start = int(resume.parent.parent.name.split('-')[-1]) if resume else 0
                hashes = [(tick, f'{tick + 1:x}') for tick in range(start, end + 1)]
                if resume:
                    self.assertEqual(resume.read_bytes(), b'invented save')
                    if control == 'mismatch':
                        hashes[0] = (start, 'f')
                    if control == 'truncated':
                        hashes = hashes[:-1]
                else:
                    (directory / 'midmission.azs').write_bytes(b'invented save')
                path = directory / 'ticks.txt'
                path.write_text(''.join(f'{t} {h}\n' for t, h in hashes), encoding='ascii')
                return dict(returncode=0, tick_hashes=str(path), engine=dict(status='timeout', ticks=end,
                                                                          state_hash=f'{end + 1:x}'))

            for control in ('match', 'mismatch', 'truncated', 'early'):
                output = root / 'work' / control
                argv = ['collector', '--game', str(game), '--engine', str(engine), '--rules-dir', str(rules),
                        '--orders-dir', str(orders), '--max-ticks', '5', '--save-ticks', '1', '3', '--out', str(output)]
                with patch.object(collector, 'ROOT', root), patch.object(collector, 'run_once', run), \
                        patch.object(sys, 'argv', argv), contextlib.redirect_stdout(io.StringIO()):
                    before = os.environ.get('AZTEC_USER_DIR')
                    self.assertEqual(collector.main(), 0 if control == 'match' else 1)
                    self.assertEqual(os.environ.get('AZTEC_USER_DIR'), before)
                results = json.loads((output / 'results.json').read_text())
                self.assertEqual(len(results), 30)
                self.assertEqual({row['mission'] for row in results}, set(range(1, 16)))
                self.assertTrue(all(row['passed'] == (control == 'match') for row in results))
            self.assertEqual(len(calls), 240)
            self.assertFalse((game / 'Saves').exists())


if __name__ == '__main__':
    unittest.main()
