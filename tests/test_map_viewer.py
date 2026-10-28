#!/usr/bin/env python3
"""Exercise the standalone inspector on invented missions, without game assets."""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

VIEWER = str(Path(sys.argv.pop(1)).resolve())


class ViewerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.maps = Path(self.temp.name) / 'Maps'
        self.maps.mkdir()
        (self.maps / 'Mission_1.MP').write_text(
            '[Globals]\nMapName=shared\nMapSize=3,2\nTileSet=1\n'
            'Sides=2\nTowns=1\nTeams=1\nBoats=1\nJohnkas=1\nSubmarines=1\n'
            '[Side0]\nStart_X=0\nStart_Y=1\nDefenceCount=1\nDefence0=9,2,1,17\n'
            '[Side1]\nStart_X=-1\nStart_Y=-1\n'
            '[Town0]\nSide=1\nX=2\nY=0\nType=3\nUnits=1\nUnit0=2,0,14,500\n'
            '[Team0]\nSide=0\nScenarioId=42\nUnits=1\nUnit0=1,1,3,75\n'
            '[Boat0]\nSide=0\nX=0\nY=0\n'
            '[Johnka0]\nSide=1\nX=1\nY=0\n'
            '[Submarine0]\nSide=1\nX=2\nY=1\n', encoding='ascii')
        (self.maps / 'SHARED.Map').write_bytes(
            struct.pack('<HH6I', 3, 2, 7, (12 << 16) | 8, 9, 10, 11, 12))
        # A second layer deliberately has dimensions different from the terrain.
        (self.maps / 'mission_1.VIS').write_bytes(
            struct.pack('<HHH6BHH2B', 2, 3, 2, 0, 1, 2, 80, 152, 255, 1, 2, 10, 152))

    def contains(self, fragment, text):
        self.assertTrue(fragment in text, f"Missing {fragment!r} in {text!r}")

    def run_viewer(self, *args, expected=0, video='invalid'):
        env = dict(os.environ, SDL_VIDEODRIVER=video, SDL_AUDIODRIVER='dummy')
        result = subprocess.run([VIEWER, '--maps', str(self.maps), *args],
                                capture_output=True, text=True, env=env, timeout=8)
        self.assertEqual(result.returncode, expected, result.stderr)
        return result

    def test_dump_uses_loaders_without_sdl_or_archives(self):
        output = self.run_viewer('--dump', '1').stdout
        self.contains('mission "Mission_1"\nterrain 3 2 tileset 1\n', output)
        self.contains('terrain-row 0 7:0 8:12 9:0\n', output)
        self.contains('start side 0 tile 0 1\nstart side 1 tile -1 -1\n', output)
        self.contains('town 0 side 1 tile 2 0 type 3 scenario -1\n', output)
        self.contains('object town side 0 tile 2 1 type 17 hp 0 group 0 scenario 9 defence 1\n', output)
        self.contains('object town side 1 tile 2 0 type 14 hp 500', output)
        self.contains('object team side 0 tile 1 1 type 3 hp 75 group 0 scenario 42', output)
        for source in ('boat', 'johnka', 'submarine'):
            self.contains('object ' + source, output)
        self.contains('fog-layers 2\nfog 0 3 2\n', output)
        self.contains('fog-row 0 1 80 152 255\nfog 1 1 2\n', output)
        self.assertEqual(output, self.run_viewer('--dump', 'mission_1.MP').stdout)

    def test_listing_and_all_missions_are_sorted(self):
        (self.maps / 'a_custom.mp').write_text('[Globals]\nMapName=shared\n', encoding='ascii')
        self.assertEqual(self.run_viewer('--list').stdout, 'a_custom.mp\nMission_1.MP\n')
        output = self.run_viewer('--dump').stdout
        self.assertEqual(output.count('end-mission\n'), 2)
        self.assertLess(output.index('mission "a_custom"'), output.index('mission "Mission_1"'))
        self.contains('fog-layers 0\n', output)

    def test_explicit_path_without_game_or_maps_directory(self):
        result = subprocess.run(
            [VIEWER, '--dump', str(self.maps / 'Mission_1.MP')],
            cwd=self.temp.name, capture_output=True, text=True, timeout=8,
            env=dict(os.environ, SDL_VIDEODRIVER='invalid'))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.contains('mission "Mission_1"', result.stdout)

    def test_mission_terrain_takes_precedence_over_shared_map(self):
        (self.maps / 'mission_1.map').write_bytes(struct.pack('<HHI', 1, 1, 99))
        output = self.run_viewer('--dump').stdout
        self.contains('terrain 1 1 tileset 1\nterrain-row 0 99:0\n', output)
        self.contains('fog 0 3 2\n', output)

    def test_present_malformed_inputs_fail(self):
        fog = self.maps / 'mission_1.VIS'
        fog.write_bytes(b'\x01\x00')
        self.contains('Cannot load fog', self.run_viewer('--dump', expected=1).stderr)
        fog.unlink()
        (self.maps / 'SHARED.Map').write_bytes(b'\x03\x00\x02\x00')
        self.contains('Cannot load terrain', self.run_viewer('--dump', expected=1).stderr)

    def test_dump_continues_past_broken_mission(self):
        (self.maps / 'a_broken.mp').write_text('[Globals]\nMapName=missing\n', encoding='ascii')
        result = self.run_viewer('--dump', expected=1)
        self.contains('a_broken', result.stderr)
        self.contains('mission "Mission_1"', result.stdout)

    def test_missing_mission_and_bad_options_fail(self):
        for args in (('--dump', 'missing'), ('--unknown',), ('--frames', '0'),
                     ('--frames', '2x'), ('--maps',), ('--dump', '--list')):
            self.run_viewer(*args, expected=1)

    def test_sdl_view_with_synthetic_data(self):
        # Exercise initialization, rendering, fallback colors and orderly teardown.
        self.run_viewer('--game', self.temp.name, '--frames', '2', video='dummy')


if __name__ == '__main__':
    unittest.main()
