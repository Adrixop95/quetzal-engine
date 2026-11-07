"""Invented missions 1-3: fresh-process continuation and graphical save cameras.

No original saves or assets are read. This test also runs on native Windows;
it deliberately does not launch the Linux-only reference harness.
"""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ENGINE = Path(sys.argv.pop(1)).resolve()
ROOT = Path(__file__).resolve().parents[1]


class CampaignSaveTests(unittest.TestCase):
    def test_three_generated_missions(self):
        with tempfile.TemporaryDirectory(prefix='quetzal-campaign-save-') as temporary:
            root = Path(temporary)
            game = root / 'invented game'
            maps = game / 'Maps'
            maps.mkdir(parents=True)
            resources = game / 'Res'
            resources.mkdir()
            for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'common', 'fnts', 'light'):
                (resources / f'{name}.xbe').write_bytes(struct.pack('<I', 0))
            for mission in (1, 2, 3):
                with self.subTest(mission=mission):
                    stem = f'mission_{mission}'
                    (maps / f'{stem}.mp').write_text(
                        '[Globals]\nMapSize=32,32\nSides=2\nTeams=2\n'
                        '[Side0]\nStart_X=1\nStart_Y=1\n'
                        '[Side1]\nStart_X=30\nStart_Y=30\n'
                        '[Team0]\nSide=0\nUnits=1\nUnit0=1,1,7,100\n'
                        '[Team1]\nSide=1\nUnits=1\nUnit0=30,30,7,100\n', encoding='ascii')
                    (maps / f'{stem}.map').write_bytes(struct.pack('<HH1024I', 32, 32, *([0] * 1024)))
                    finish = 30 + mission
                    (maps / f'{stem}.scn').write_text(
                        'trigger(0) conditions localTime == 3 actions trigger(0).disable end\n'
                        f'trigger(1) conditions localTime == {finish} actions side(local).victory end\n',
                        encoding='ascii')
                    profile = root / f'profile-{mission}'
                    profile.mkdir()
                    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
                               AZTEC_USER_DIR=str(profile))
                    common = [str(ENGINE), '--profile', 'faithful', '--game', str(game), '--rules-dir', str(ROOT / 'docs/data')]

                    def run(args):
                        command = [*common, *args, str(mission)]
                        if "--headless" in args:
                            command.remove("--headless")
                            command.insert(1, "--headless")
                        result = subprocess.run(command, env=env,
                                                capture_output=True, text=True, timeout=10)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        return result

                    orders = root / 'orders.txt'
                    orders.write_text('1 move 0 8 8 1\n7 move 0 12 12 1\n', encoding='ascii')
                    slot = root / f'{stem}.azs'
                    first, second = root / 'first.txt', root / 'second.txt'
                    headless = ['--headless', '--no-auto-orders', '--orders', str(orders), '--max-ticks', '50']
                    baseline = json.loads(run([*headless, '--tick-hashes', str(first),
                                               '--save-at', '5', '--save', str(slot)]).stdout)
                    saved = slot.read_bytes()
                    resumed = json.loads(run([*headless, '--tick-hashes', str(second), '--resume', str(slot)]).stdout)
                    self.assertEqual(slot.read_bytes(), saved)
                    for field in ('state_hash', 'status', 'ticks', 'commands', 'action_observations', 'outcome_origin'):
                        self.assertEqual(baseline[field], resumed[field], field)
                    self.assertEqual(resumed['status'], 'victory')
                    self.assertEqual(resumed['ticks'], finish)
                    self.assertEqual(resumed['commands'], 2)
                    # Compare every saved-boundary and continued tick, not only the terminal hash.
                    tail = first.read_text().splitlines()[5:]
                    self.assertEqual(tail, second.read_text().splitlines())

                    script = root / 'controls.txt'
                    trace = root / 'ui.jsonl'
                    graphical = ['--software', '--no-audio', '--no-light', '--no-vsync',
                                 '--size', '800x600', '--script', str(script), '--ui-trace', str(trace)]
                    script.write_text('until 5\nkey ctrl+S\nexpect tick == 6\nquit\n', encoding='ascii')
                    self.assertTrue('saved game (tick 5)' in run([*graphical, '--cam', '14,12', '--zoom', '1.5']).stdout)
                    rows = [json.loads(line) for line in trace.read_text().splitlines()]
                    camera = next(row['camera'] for row in rows if row['tick'] == 5)
                    self.assertEqual(camera[2], 1.5)
                    ui_slot = profile / 'Saves/engine_slot1.azs'
                    saved = ui_slot.read_bytes()
                    script.write_text(f'key ctrl+L\nexpect tick == 6\nuntil {finish}\n'
                                      f'expect outcome == 1\nexpect tick == {finish}\nquit\n', encoding='ascii')
                    self.assertTrue('loaded game (tick 5)' in run(graphical).stdout)
                    rows = [json.loads(line) for line in trace.read_text().splitlines()]
                    self.assertEqual(rows[0]['camera'], camera)
                    self.assertTrue(any(row['outcome'] == 1 and row['tick'] == finish for row in rows))
                    self.assertEqual(ui_slot.read_bytes(), saved)
                    menu = root / 'menu.txt'
                    menu_trace = root / 'menu.jsonl'
                    menu.write_text('0 move 310 470\n0 down\n1 up\n2 move 310 210\n2 down\n3 up\n'
                                    '4 keydown Return\n5 keyup Return\n6 keydown Return\n', encoding='ascii')
                    frontend = subprocess.run([*common, '--software', '--no-audio', '--size', '800x600',
                                               '--menu-script', str(menu), '--menu-trace', str(menu_trace)],
                                              env=env, capture_output=True, text=True, timeout=10)
                    self.assertEqual(frontend.returncode, 0, frontend.stdout + frontend.stderr)
                    menu_rows = [json.loads(line) for line in menu_trace.read_text().splitlines()]
                    self.assertTrue(any(row['rendered_screen'] == 'load-confirmation' for row in menu_rows))
                    self.assertEqual(menu_rows[-1]['mission'], stem)
                    # Automation intentionally never awards campaign progress.
                    self.assertFalse((profile / 'Saves/engine_campaign.txt').exists())


if __name__ == '__main__':
    unittest.main()
