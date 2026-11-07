"""Forced-state runner controls using invented missions only."""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

BINARY = Path(sys.argv.pop(1)).resolve()
RULES = Path(sys.argv.pop(1)).resolve()


class CampaignProbe(unittest.TestCase):
    def test_forced_conditions_and_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            maps = root / 'Maps'
            maps.mkdir()
            resources = root / 'Res'
            resources.mkdir()
            for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'common', 'fnts', 'light'):
                (resources / f'{name}.xbe').write_bytes(struct.pack('<I', 0))
            (maps / 'mission_1.mp').write_text(
                '[Globals]\nSides=2\nTeams=2\n'
                '[Side0]\n[Side1]\nOnlyComp=1\n'
                '[Team0]\nSide=0\nUnits=1\nUnit0=1,1,0,100\n'
                '[Team1]\nSide=1\nUnits=1\nUnit0=6,6,0,100\n')
            (maps / 'mission_1.map').write_bytes(struct.pack('<HH64I', 8, 8, *([0] * 64)))
            script = maps / 'mission_1.scn'
            script.write_text('trigger(0) conditions side(1).units == 0 actions side(local).victory end\n')
            events = root / 'events.txt'
            events.write_text('2 kill-side 1\n')

            def run(code=0, *extra):
                result = subprocess.run(
                    [str(BINARY), '--game', str(root), '--rules-dir', str(RULES),
                     '--mission', '1', '--events', str(events), '--max-ticks', '4', *extra],
                    env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'),
                    capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, code, result.stdout + result.stderr)
                return result.stdout

            first = run()
            self.assertEqual(first, run())
            self.assertIn('status\tvictory\nticks\t2', first)
            self.assertIn('outcome_trigger\t0', first)
            isolated = run(0, '--mode', 'scripts')
            self.assertIn('mode\tscripts', isolated)
            self.assertIn('status\tvictory\nticks\t2', isolated)
            events.write_text('')
            self.assertIn('status\ttimeout', run(1))
            run(0, '--expect', 'timeout')
            events.write_text('2 kill-side 1\n3 kill-side 0\n')
            self.assertIn('unapplied_events\t1', run(1))
            for invalid in ('1 unsupported 1\n', '0 kill-side 1\n', '1 kill-side 8\n',
                            '1 kill-type 1\n', '2 kill-side 1\n1 kill-side 0\n', '1 kill-side 1 junk\n'):
                events.write_text(invalid)
                run(2)
            events.write_text('2 kill-side 1\n')
            script.write_text(
                'trigger(0) conditions side(1).units == 0 actions side(local).victory end\n'
                'trigger(1) conditions localTime == 100 actions side(local).moveCommand(1,1) end\n')
            self.assertIn('unsupported_sites\t1', run(1))


if __name__ == '__main__':
    unittest.main()
