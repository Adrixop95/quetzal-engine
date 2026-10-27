#!/usr/bin/env python3
"""Asset-free controls for screenshot comparison bookkeeping."""
import importlib.util
from pathlib import Path
import tempfile
import os
import subprocess
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
spec = importlib.util.spec_from_file_location('zspr_compare', TOOLS / 'zspr_compare.py')
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)


class ComparatorControls(unittest.TestCase):
    def test_exact_and_tolerant_scores_are_separate(self):
        self.assertEqual(compare.pixel_match(0xf800, 0xf800, 0), (True, True))
        self.assertEqual(compare.pixel_match(0xf800, 0xf000, 0), (False, False))
        self.assertEqual(compare.pixel_match(0xf800, 0xf000, 8), (False, True))
        self.assertEqual(compare.pixel_match(0xf800, 0xf000, 7), (False, False))

    def test_alpha_background_is_required_for_prediction(self):
        self.assertEqual(compare.composite565(0xffff, 0, (0, 0, 0)), 0)
        self.assertEqual(compare.composite565(0xffff, 255, (0, 0, 0)), 0xffff)
        self.assertEqual(compare.composite565(0xffff, 128, (0, 0, 0)), 0x8410)
        self.assertNotEqual(compare.composite565(0xf800, 128, (0, 0, 0)),
                            compare.composite565(0xf800, 128, (0, 0, 255)))

    def test_stability_includes_animation_and_object_state(self):
        with tempfile.TemporaryDirectory() as root:
            a, b = Path(root) / 'before', Path(root) / 'after'
            for path in (a, b):
                path.write_text('invented object position')
                Path(str(path) + '.deref').write_text('invented active frame')
            self.assertTrue(compare.stable_capture(a, b))
            Path(str(b) + '.deref').write_text('different active frame')
            self.assertFalse(compare.stable_capture(a, b))
            Path(str(b) + '.deref').write_text('invented active frame')
            b.write_text('different position')
            self.assertFalse(compare.stable_capture(a, b))
            a.write_bytes(b''); b.write_bytes(b'')
            self.assertFalse(compare.stable_capture(a, b))
            with self.assertRaises(ValueError):
                compare.stable_capture(a, a)


class GeneratorControls(unittest.TestCase):
    def test_mobile_cannon_and_building_use_distinct_sections(self):
        with tempfile.TemporaryDirectory() as root:
            maps = Path(root) / 'Maps'; maps.mkdir()
            (maps / 'mission_1.mp').write_text(
                '[Globals]\nMapName=mission_1\nName=Mission 1\nStartTime=100\nTowns=1\nTeams=1\n'
                '[Side0]\nColor=0\nStart_X=34\nStart_Y=39\n'
                '[Town0]\nUnit0=1,1,0,6000\nUnits=1\n[Team0]\nUnits=0\n')
            (maps / 'mission_1.dat').write_bytes(bytes([2]) * (90 * 60))
            (maps / 'mission_1.scn').write_text('inherited scenario must not run')
            env = dict(os.environ, GAME_DIR=root, START_TIME='5400', SPACING='0', OPP='')
            result = subprocess.run([sys.executable, str(TOOLS / 'make_zspr.py'), 'zsynthetic', '87,14'],
                                    env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            text = (maps / 'zsynthetic.mp').read_text()
            self.assertTrue('StartTime=5400' in text)
            self.assertTrue(' 87,28000' in text)
            self.assertNotIn('Unit0=1,1,0,6000', text)
            self.assertEqual((maps / 'zsynthetic.scn').read_bytes(), b'')
            team = text.split('[Team0]')[1].split('[Team1]')[0]
            self.assertTrue(' 87,' in team)
            self.assertNotIn(' 14,', team)


if __name__ == '__main__':
    unittest.main()
