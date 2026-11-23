#!/usr/bin/env python3
"""Invented-data controls for smoke inventory, isolation and failure reporting."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ENGINE = Path(sys.argv.pop(1)).resolve()
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('mission_smoke', ROOT / 'tools/mission_smoke.py')
smoke = importlib.util.module_from_spec(spec)
spec.loader.exec_module(smoke)


class SmokeTests(unittest.TestCase):
    def setUp(self):
        (ROOT / 'build').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / 'build')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.game = self.root / 'game'
        self.maps = self.game / 'Maps'
        self.maps.mkdir(parents=True)
        resources = self.game / 'Res'
        resources.mkdir()
        for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'common', 'fnts', 'light'):
            (resources / f'{name}.xbe').write_bytes(struct.pack('<I', 0))
        for mission in range(1, 16):
            (self.maps / f'mission_{mission}.mp').write_text(
                '[Globals]\nMapSize=8,8\nSides=1\nTeams=1\n'
                '[Side0]\nStart_X=1\nStart_Y=1\n'
                '[Team0]\nSide=0\nUnits=1\nUnit0=1,1,3,100\n', encoding='ascii')
            (self.maps / f'mission_{mission}.map').write_bytes(struct.pack('<HH64I', 8, 8, *([0] * 64)))
            (self.maps / f'mission_{mission}.scn').write_text(
                'trigger(0) conditions localTime > 100 actions side(local).buildCommand(7) {UGI_FOOTMAN} end\n',
                encoding='ascii')

    def engine(self, *args):
        result = subprocess.run([str(ENGINE), '--headless', '--game', str(self.game),
                                 '--settings', str(self.root / 'settings.ini'), *args], capture_output=True,
                                text=True, timeout=10, env=dict(os.environ, AZTEC_USER_DIR=str(self.root / 'profile')))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return json.loads(result.stdout)

    def test_configuration_and_opt_in_diagnostics(self):
        (self.root / 'settings.ini').write_text('game=absent\n')
        self.assertEqual(self.engine('--print-game-dir'), {'game': str(self.game)})
        args = SimpleNamespace(engine=ENGINE, settings=self.root / 'settings.ini', game=None)
        with patch.dict(os.environ, AZTEC_GAME='', AZTEC_USER_DIR=str(self.root / 'profile')):
            self.assertEqual(smoke.configured_game(args), (Path.cwd() / 'absent').resolve())
        with patch.dict(os.environ, AZTEC_GAME=str(self.game)):
            self.assertEqual(smoke.configured_game(args), self.game)
        unicode_path = self.root / 'caf\u00e9'
        encoded_path = os.fsencode(unicode_path).decode('latin1')
        response = SimpleNamespace(returncode=0, stdout=json.dumps({'game': encoded_path}), stderr='')
        with patch.object(smoke.subprocess, 'run', return_value=response):
            self.assertEqual(smoke.configured_game(args), unicode_path)
        report = self.engine('--no-auto-orders', '--smoke-diagnostics', '--max-ticks', '3',
                             '--rules-dir', str(ROOT / 'docs/data'), '1')
        self.assertTrue(report['smoke']['loaded'])
        self.assertEqual(report['ticks'], 3)
        self.assertGreater(report['smoke']['tick_seconds'], 0)
        self.assertLessEqual(report['smoke']['max_tick_seconds'], report['smoke']['tick_seconds'])
        gaps = smoke.script_gaps(report['smoke']['script_inventory'], smoke.gap_catalog(ROOT / 'docs/script_runtime_gap_list.md'))
        self.assertEqual([(row['code'], row['disposition']) for row in gaps],
                         [('07', 'approximate'), ('11', 'unsupported')])
        self.assertEqual(report['unsupported_executions'], 0)  # Dormant build site is still inventoried.
        normal = self.engine('--no-auto-orders', '--max-ticks', '3', '--rules-dir', str(ROOT / 'docs/data'), '1')
        self.assertNotIn('smoke', normal)
        self.assertEqual(normal['state_hash'], report['state_hash'])
        # A stripped rules directory still loads types, but must expose descriptor fallback.
        rules = self.root / 'rules'
        rules.mkdir()
        (rules / 'types.csv').write_bytes((ROOT / 'docs/data/types.csv').read_bytes())
        fallback = self.engine('--no-auto-orders', '--smoke-diagnostics', '--max-ticks', '1',
                               '--rules-dir', str(rules), '1')
        self.assertEqual(fallback['smoke']['unresolved_descriptors'][0]['first_tick'], 0)

    def test_private_output_and_skip(self):
        with self.assertRaises(ValueError):
            smoke.private_output(ROOT / 'docs' / 'private-report')
        link = self.root / 'redirect'
        if os.name != 'nt':
            link.symlink_to(ROOT / 'docs', target_is_directory=True)
            with self.assertRaises(ValueError):
                smoke.private_output(link / 'report')
        with patch.object(smoke, 'configured_game', return_value=self.root / 'absent'), contextlib.redirect_stdout(io.StringIO()) as out:
            self.assertEqual(smoke.main(['--engine', str(ENGINE)]), 0)
        self.assertIn('SKIP', out.getvalue())

    def test_catalog_covers_native_codes_and_closed_noop(self):
        catalog = smoke.gap_catalog(ROOT / 'docs/script_runtime_gap_list.md')
        self.assertEqual(len(catalog), len(smoke.ACTION_NAMES) + len(smoke.CONDITION_NAMES) + 1)
        self.assertEqual(catalog['action', 'SideMoveCommand']['disposition'], 'implemented')
        self.assertEqual(catalog['action', 'SideCameraDeactivate']['code'], '21')
        with self.assertRaises(ValueError):
            smoke.script_gaps([dict(section='action', kind='unknown')], catalog)

    def test_crash_isolation_and_sanitizer_failures(self):
        args = SimpleNamespace(engine=ENGINE, rules_dir=ROOT / 'docs/data', timeout=2,
                               ticks=3, sanitizer_build=True)
        catalog = smoke.gap_catalog(ROOT / 'docs/script_runtime_gap_list.md')
        for index, (code, log, status) in enumerate((
                (-11, '', 'crash'), (1, 'unshare: denied', 'isolation_or_launch_error'),
                (0, 'runtime error: synthetic diagnostic', 'sanitizer_failure'),
                (137, '', 'wall_timeout'))):
            def launch(*unused, **kwargs):
                kwargs['stderr'].write(log.encode('ascii'))
                process = unittest.mock.MagicMock()
                process.__enter__.return_value.wait.return_value = code
                return process
            with patch.object(smoke, 'private_game', side_effect=lambda source, maps, dest: dest.mkdir()), \
                    patch.object(smoke.subprocess, 'Popen', side_effect=launch):
                row = smoke.run_mission(args, 1, self.game, self.maps, self.root / f'failure-{index}', catalog)
            self.assertEqual(row['status'], status)
            self.assertEqual(bool(row['sanitizer_findings']), status == 'sanitizer_failure')

    @unittest.skipUnless(sys.platform == 'linux', 'live isolation uses Linux namespaces')
    def test_full_sweep_continues_after_load_failure(self):
        if not shutil.which('unshare') or not shutil.which('timeout'):
            self.skipTest('isolation tools unavailable')
        if subprocess.run(['unshare', '-rn', 'true'], capture_output=True).returncode:
            self.skipTest('network namespace creation unavailable')
        (self.maps / 'mission_2.scn').write_text('broken script')
        output = self.root / 'smoke'
        with contextlib.redirect_stdout(io.StringIO()):
            code = smoke.main(['--engine', str(ENGINE), '--game', str(self.game), '--ticks', '3',
                               '--output', str(output)])
        self.assertEqual(code, 1)
        report = json.loads((output / 'report.json').read_text())
        self.assertEqual(len(report['missions']), 15)
        self.assertEqual(report['missions'][1]['status'], 'load_error')
        self.assertEqual(sum(row['status'] == 'passed' for row in report['missions']), 14)
        row = report['missions'][-1]
        self.assertIn('--no-auto-orders', row['command'])
        self.assertEqual(row['command'][:6], ['timeout', '-s', 'KILL', '120', 'unshare', '-rn'])
        self.assertTrue((output / 'mission_15/game/Maps/mission_15.mp').is_symlink())
        self.assertFalse((self.game / 'Saves').exists())


if __name__ == '__main__':
    unittest.main()
