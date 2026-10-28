#!/usr/bin/env python3
"""Synthetic simulation and campaign-runner failure checks; no original data."""
import importlib.util
import contextlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ENGINE = Path(sys.argv.pop(1)).resolve()
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('playthrough', ROOT / 'tools/playthrough.py')
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


class PlaythroughTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.game = Path(self.temp.name) / 'game'
        self.maps = self.game / 'Maps'
        self.maps.mkdir(parents=True)
        resources = self.game / 'Res'
        resources.mkdir()
        for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'common', 'fnts', 'light'):
            (resources / f'{name}.xbe').write_bytes(struct.pack('<I', 0))
        (self.maps / 'Mission_1.mp').write_text(
            '[Globals]\nMapSize=8,8\nSides=2\nTeams=2\n'
            '[Side0]\nStart_X=1\nStart_Y=1\n'
            '[Side1]\nOnlyComp=1\nStart_X=6\nStart_Y=6\n'
            '[Team0]\nSide=0\nUnits=1\nUnit0=1,1,3,100\n'
            '[Team1]\nSide=1\nUnits=1\nUnit0=6,6,3,100\n', encoding='ascii')
        (self.maps / 'mission_1.map').write_bytes(struct.pack('<HH64I', 8, 8, *([0] * 64)))
        self.script = self.maps / 'mission_1.scn'
        self.script.write_text('trigger(0) conditions localTime == 10 actions side(local).victory end\n')

    def run_engine(self, *extra, code=0):
        result = subprocess.run([str(ENGINE), '--headless', '--game', str(self.game),
                                 '--rules-dir', str(ROOT / 'docs/data'), '--max-ticks', '20', *extra, '1'],
                                env=dict(os.environ, SDL_VIDEODRIVER='invalid', SDL_AUDIODRIVER='invalid'),
                                capture_output=True, text=True, timeout=8)
        self.assertEqual(result.returncode, code, result.stderr)
        return json.loads(result.stdout)

    def test_outcomes_and_determinism_without_sdl(self):
        a = self.run_engine()
        self.assertEqual(a['status'], 'victory')
        self.assertEqual(a['ticks'], 10)
        self.assertGreater(a['commands'], 0)
        self.assertEqual(a, self.run_engine())
        self.script.write_text('trigger(0) conditions localTime == 10 actions side(local).defeat end\n')
        self.assertEqual(self.run_engine()['status'], 'defeat')

    def test_impossible_and_merely_unmet_objectives(self):
        self.script.write_text('trigger(0) conditions side(local).unitsInRect(10,10,11,11) > 0 actions side(local).victory end\n')
        report = self.run_engine()
        self.assertEqual(report['status'], 'timeout')
        self.assertEqual(len(report['unreachable_objectives']), 1)
        self.assertEqual(len(report['unmet_objectives']), 1)
        self.script.write_text('trigger(0) conditions localTime > 100 actions side(local).victory end\n')
        report = self.run_engine()
        self.assertFalse(report['unreachable_objectives'])
        self.assertTrue(report['unmet_objectives'])

    def test_script_parse_missing_and_unsupported_errors(self):
        self.script.write_text('broken script')
        self.assertTrue('script parse error' in self.run_engine(code=1)['error'])
        self.script.write_text('trigger(0) conditions always actions boat(0).activate end\n')
        report = self.run_engine()
        self.assertTrue(report['script_errors'])
        self.assertEqual(report['unsupported_executions'], 20)
        self.script.unlink()
        self.assertTrue('missing mission script' in self.run_engine()['script_errors'])

    def test_orders_validation_and_execution(self):
        orders = Path(self.temp.name) / 'orders'
        orders.write_text('1 move 0 2 2 1\n')
        report = self.run_engine('--no-auto-orders', '--orders', str(orders))
        self.assertEqual(report['commands'], 1)
        orders.write_text('1 move 0 2 2 2\n')
        self.assertTrue('invalid order object' in self.run_engine('--orders', str(orders), code=1)['error'])
        orders.write_text('21 move 0 2 2\n')
        self.assertTrue('exceeds run limit' in self.run_engine('--orders', str(orders), code=1)['error'])
        self.assertEqual(self.run_engine('--max-ticks', '0', code=1)['status'], 'error')

    def test_discovery_and_private_copy(self):
        (self.maps / 'custom.mp').write_text('not a campaign')
        (self.maps / 'mission_10.MP').write_text('invented mission')
        (self.game / 'profile.plr').write_bytes(b'private profile')
        (self.game / 'Saves').mkdir()
        (self.game / 'Saves' / 'original.sav').write_bytes(b'private save')
        maps, missions = harness.discover(self.game)
        self.assertEqual(sorted(missions), [1, 10])
        target = Path(self.temp.name) / 'copy'
        harness.private_game(self.game, maps, target)
        self.assertFalse((target / 'Maps').is_symlink())
        self.assertFalse((target / 'Saves').is_symlink())
        self.assertFalse(list((target / 'Saves').iterdir()))
        self.assertFalse((target / 'profile.plr').is_symlink())
        (target / 'profile.plr').write_bytes(b'changed')
        self.assertEqual((self.game / 'profile.plr').read_bytes(), b'private profile')
        self.assertTrue((target / 'Maps' / 'Mission_1.mp').is_symlink())

    def test_comparison_requires_two_successful_matching_runs(self):
        report = self.run_engine()
        run = {'returncode': 0, 'engine': report}
        self.assertTrue(harness.compare([run, run]))
        different = dict(report, trace_hash='different')
        self.assertFalse(harness.compare([run, {'returncode': 0, 'engine': different}]))
        self.assertIsNone(harness.compare([run, {'returncode': 1}]))
        self.assertIsNone(harness.compare([]))

    def test_runner_launch_failure_is_preserved(self):
        args = type('Args', (), dict(
            engine=ENGINE, rules_dir=ROOT / 'docs/data', max_ticks=10,
            order_interval=120, no_auto_orders=False, orders_dir=None, timeout=1))()
        with patch.object(harness.subprocess, 'Popen', side_effect=OSError('namespace unavailable')):
            report = harness.run_once(args, 1, self.game, Path(self.temp.name) / 'run')
        self.assertEqual(report['status'], 'launch_error')
        self.assertTrue('namespace unavailable' in report['error'])

    def test_campaign_summary_and_exit_status(self):
        (ROOT / 'work').mkdir(exist_ok=True)
        temp = tempfile.TemporaryDirectory(dir=ROOT / 'work')
        self.addCleanup(temp.cleanup)
        real_popen = subprocess.Popen

        def synthetic_launch(command, **kwargs):
            self.assertEqual(command[:2], ['unshare', '-rn'])
            self.assertEqual(kwargs['env']['SDL_VIDEODRIVER'], 'dummy')
            self.assertEqual(kwargs['env']['SDL_AUDIODRIVER'], 'dummy')
            # Synthetic data needs no namespace privileges in CI.
            return real_popen(command[2:], **kwargs)

        for expected in (0, 1):
            output = Path(temp.name) / str(expected)
            if expected:
                self.script.write_text('trigger(0) conditions always actions side(local).defeat end\n')
            with patch.object(harness.subprocess, 'Popen', side_effect=synthetic_launch), contextlib.redirect_stdout(io.StringIO()):
                code = harness.main(['--game', str(self.game), '--engine', str(ENGINE),
                                     '--out', str(output), '--max-ticks', '20'])
            self.assertEqual(code, expected)
            summary = json.loads((output / 'summary.json').read_text())
            self.assertEqual(len(summary['missions']), 1)
            self.assertTrue(summary['missions'][0]['deterministic'])
            self.assertTrue((output / 'aztec_engine').is_file())

    @unittest.skipUnless(sys.platform.startswith("linux"), "the runner needs unshare and POSIX signals")
    def test_runner_crash_and_wall_timeout(self):
        args = type('Args', (), dict(engine=ENGINE, rules_dir=ROOT / 'docs/data', max_ticks=10,
                    order_interval=120, no_auto_orders=False, orders_dir=None, timeout=1))()
        real_popen = subprocess.Popen
        for script, expected in [('import os, signal; os.kill(os.getpid(), signal.SIGKILL)', 'crash'),
                                 ('import time; time.sleep(30)', 'wall_timeout'),
                                 ('raise SystemExit(1)', 'isolation_or_launch_error')]:
            directory = Path(self.temp.name) / expected
            with patch.object(harness.subprocess, 'Popen',
                              side_effect=lambda command, **kwargs: real_popen([sys.executable, '-c', script], **kwargs)):
                report = harness.run_once(args, 1, self.game, directory)
            self.assertEqual(report['status'], expected)

    def test_output_must_be_private_and_separate_from_inputs(self):
        for extra in (['--out', self.temp.name],
                      ['--out', str(ROOT / 'work' / 'bad-playthrough'), '--rules-dir', str(ROOT / 'work')]):
            with self.assertRaises(SystemExit) as raised, contextlib.redirect_stderr(io.StringIO()):
                harness.main(['--game', str(self.game), '--engine', str(ENGINE), *extra])
            self.assertEqual(raised.exception.code, 2)


if __name__ == '__main__':
    unittest.main()
