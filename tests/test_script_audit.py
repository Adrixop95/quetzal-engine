"""Synthetic campaign inventory, including strict unsupported-action failures."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


BINARY = sys.argv.pop(1)


class ScriptAudit(unittest.TestCase):
    def test_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            maps = Path(directory)
            for number in range(1, 16):
                (maps / f'MiSsIoN_{number}.ScN').write_text(
                    'trigger(0) conditions always actions side(local).victory end\n',
                    encoding='ascii')

            def run(*args):
                return subprocess.run([BINARY, '--maps', str(maps), *args],
                                      capture_output=True, text=True, check=False)

            result = run('--require-supported')
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.count('\taction\tSideVictory\t1\timplemented'), 15)
            self.assertTrue('unsupported_action_sites=0' in result.stderr)
            (maps / 'MiSsIoN_3.ScN').write_text(
                'trigger(0) conditions always actions boat(42).activate end\n', encoding='ascii')
            self.assertEqual(run().returncode, 0)
            result = run('--require-supported')
            self.assertEqual(result.returncode, 1)
            self.assertTrue('mission_3: trigger 0 line 1: unsupported BoatActivate' in result.stderr)
            (maps / 'MiSsIoN_3.ScN').write_text('invalid', encoding='ascii')
            self.assertTrue('mission_3: parse failure' in run().stderr)
            self.assertEqual(run().returncode, 1)
            (maps / 'MiSsIoN_3.ScN').unlink()
            result = run('--optional')
            self.assertEqual(result.returncode, 1)
            self.assertTrue('mission_3: missing script' in result.stderr)

    def test_absent_installation(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([BINARY, '--game', directory, '--optional'],
                                    env={**os.environ, 'GAME_DIR': ''}, capture_output=True, check=False)
            self.assertEqual(result.returncode, 77)


if __name__ == '__main__':
    unittest.main()
