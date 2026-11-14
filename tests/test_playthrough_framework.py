#!/usr/bin/env python3
"""Generated real-command scenarios and adversarial detector/report checks."""
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ENGINE = Path(sys.argv.pop(1)).resolve()
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import playthrough_route as route  # noqa: E402


class FrameworkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='aztec-routes-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.game = route.demo_game(self.root / 'game')
        self.args = SimpleNamespace(engine=ENGINE, rules_dir=ROOT / 'docs/data', capture_every=None,
                                    max_ticks=2000, mode='headless', timeout=20, goal_ticks=600,
                                    stuck_ticks=240, idle_ticks=1200, economy_ticks=1200, path_failures=3)
        self.serial = 0

    def run_route(self, script):
        self.serial += 1
        output = self.root / f'run-{self.serial}'
        report = route.execute(self.args, output, self.game, 'demo', script, launcher=())
        self.assertEqual(report, json.loads((output / 'report.json').read_text()))
        self.assertIn(report['outcome'], (output / 'report.md').read_text())
        return report, output

    def test_whole_loop_repeat_and_observations_do_not_change_game(self):
        script = (ROOT / 'tests/playtest/playthrough_demo.txt').read_text()
        a, output = self.run_route(script)
        self.assertEqual((a['outcome'], a['anomalies']), ('won', []))
        self.assertLess(a['ticks'], 2000)
        b, other = self.run_route(script)
        left, right = [list(route.read_observations(p / 'observations.jsonl')) for p in (output, other)]
        self.assertEqual(left, right)
        commands = [c['type'] for r in left for c in r['commands']]
        for kind in (0x1c, 0x52, 0x15, 0x16, 0x17, 0x54, 0x55, 0x51):
            self.assertIn(kind, commands)
        self.assertGreater(left[-1]['income'], 0)
        embarked = [o for r in left for o in r['objects'] if o['carrier']]
        self.assertTrue(embarked)
        self.assertTrue(all(o['alive'] for o in embarked))
        # Run the same input with telemetry disabled and compare the ordinary final hash.
        direct = self.root / 'direct.txt'
        direct.write_text(script.replace('quit', 'print\nquit'))
        command = a['process']['command']
        command = command[:]
        command[command.index('--script') + 1] = str(direct)
        command[command.index('--ui-trace') + 1] = str(self.root / 'direct-ui.jsonl')
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', AZTEC_USER_DIR=str(self.root / 'direct-profile'))
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(f'hash {int(left[-1]["hash"]):016x}', result.stdout)
        self.assertEqual(b['outcome'], 'won')

    def test_unreachable_order_fails_and_captures_exact_anomaly_ticks(self):
        self.args.goal_ticks = 20
        self.args.stuck_ticks = 10
        self.args.capture_every = 25
        report, output = self.run_route('select id 1\norder-move 26 12\nawait 80 tile 1 26 12 == 1\nquit\n')
        self.assertEqual(report['outcome'], 'scenario-failed')
        self.assertTrue(any(a['kind'] == 'goal-timeout' and a['object'] == 1 for a in report['anomalies']))
        self.assertTrue(report['capture_replay']['verified'])
        for anomaly in report['anomalies']:
            if anomaly['tick']:
                self.assertTrue((output / anomaly['capture']).is_file(), anomaly)
        self.assertEqual(report['process']['returncode'], 3)

    def test_repeated_unreachable_requests_have_measured_path_failures(self):
        self.args.goal_ticks = 10
        report, _ = self.run_route('select id 1\norder-move 26 12\norder-move 26 12\norder-move 26 12\nwait 40\nquit\n')
        self.assertIn('repeated-path-failure', [a['kind'] for a in report['anomalies']])

    def test_invalid_inputs_placement_and_worker_gather_fail_loudly(self):
        for script in ('select id 999\n', 'select type 999\n', 'select group -1\n',
                       'build 14 5 6 3\n', 'train 0 51 3\n', 'train 0 1x 3\n',
                       'gather 0 5 18 3\n', 'select type -1\n', 'checkpoint money >= 999999\n',
                       'await 2 unknown == 1\n', 'unknown-command\n'):
            with self.subTest(script=script):
                report, _ = self.run_route(script)
                self.assertEqual(report['outcome'], 'scenario-failed')
                self.assertEqual(report['process']['returncode'], 3)

    def test_tick_budget_including_low_level_run_and_timeout(self):
        self.args.max_ticks = 12
        self.args.economy_ticks = 100
        for script in ('wait 1000\n', 'run 1000\n'):
            report, _ = self.run_route(script)
            self.assertEqual((report['outcome'], report['ticks']), ('timeout', 12))

    def test_terminal_checkpoints_are_executed_with_captures_enabled(self):
        self.args.capture_every = 400
        script = (ROOT / 'tests/playtest/playthrough_demo.txt').read_text().replace('checkpoint outcome == 1', 'checkpoint outcome == 2')
        report, _ = self.run_route(script)
        self.assertEqual(report['outcome'], 'scenario-failed')
        self.assertEqual(report['process']['returncode'], 3)

    def test_ignored_production_is_not_reported_as_success(self):
        path = self.game / 'Maps/demo.mp'
        path.write_text(path.read_text().replace('StartMoney=10000', 'StartMoney=0'))
        report, _ = self.run_route('train 0 50 3\nwait 10\nquit\n')
        # Production may queue unpaid work; completion still has to be explicitly witnessed.
        self.assertEqual(report['outcome'], 'scenario-failed')
        report, _ = self.run_route('select id 2\ncontext 2\nwait 10\nquit\n')
        self.assertIn('order-ignored', [a['kind'] for a in report['anomalies']])

    def test_enemy_ai_commands_are_not_player_order_failures(self):
        path = self.game / 'Maps/demo.mp'
        text = path.read_text().replace('Sides=1', 'Sides=2').replace('Towns=1', 'Towns=2')
        path.write_text(text + '[Side1]\nOnlyComp=1\nStartMoney=10000\nStart_X=27\nStart_Y=6\n'
                        '[Town1]\nSide=1\nX=27\nY=6\nType=1\nRace=0\n')
        report, output = self.run_route('select id 1\norder-move 6 12\nwait 150\nquit\n')
        rows = list(route.read_observations(output / 'observations.jsonl'))
        commands = [c for row in rows for c in row['commands']]
        self.assertTrue(commands)
        self.assertTrue(all(c['side'] == 0 for c in commands))
        self.assertFalse(any(a['kind'] == 'order-ignored' for a in report['anomalies']))

    def test_unsupported_dormant_script_sites_are_reported(self):
        path = self.game / 'Maps/demo.scn'
        path.write_text('trigger(1) conditions localTime > 100 actions side(local).moveCommand(1,2) end\n')
        report, _ = self.run_route('wait 3\nquit\n')
        self.assertIn('unsupported-action', [a['kind'] for a in report['anomalies']])

    def test_objective_flag_and_existing_low_level_inputs(self):
        report, _ = self.run_route('click 5 5\nkey H\nawait 10 objective 0 == 1\ncheckpoint tick >= 1\ncheckpoint units 0 == 2\nquit\n')
        self.assertEqual(report['process']['returncode'], 0)
        self.assertEqual(report['outcome'], 'scenario-failed')  # Still running is never a win.

    def test_defeat_remains_lost_and_skipped_terminal_checkpoints_fail(self):
        path = self.game / 'Maps/demo.scn'
        path.write_text('trigger(1) conditions localTime == 3 actions side(local).defeat end\n')
        report, _ = self.run_route('await 20 outcome == 2\ncheckpoint outcome == 2\nquit\n')
        self.assertEqual(report['outcome'], 'lost')
        self.assertTrue(report['script_complete'])
        path.write_text('trigger(1) conditions localTime == 3 actions side(local).victory end\n')
        self.args.max_ticks = 3
        report, _ = self.run_route('await 20 outcome == 1\ncheckpoint money >= 9999999\nquit\n')
        self.assertEqual(report['engine_outcome'], 'won')
        self.assertEqual(report['outcome'], 'scenario-failed')
        self.assertFalse(report['script_complete'])

    def test_context_attack_uses_the_normal_attack_packet(self):
        path = self.game / 'Maps/demo.mp'
        text = path.read_text().replace('Sides=1', 'Sides=2').replace('Teams=1', 'Teams=2')
        path.write_text(text + '[Side1]\nStart_X=7\nStart_Y=12\n'
                        '[Team1]\nSide=1\nUnits=1\nUnit0=6,12,0,100\n')
        report, output = self.run_route('select id 1\ncontext 2\nwait 2\nquit\n')
        rows = list(route.read_observations(output / 'observations.jsonl'))
        self.assertTrue(any(c['type'] == 0x18 and c['a'] == 2 for r in rows for c in r['commands']))
        self.assertEqual(report['process']['returncode'], 0)

    @unittest.skipUnless(hasattr(os, 'killpg'), 'Unix process groups required')
    def test_wall_timeout_kills_the_process_group(self):
        directory = self.root / 'wall-timeout'
        directory.mkdir()
        report = route.launch([sys.executable, '-c', 'import time; time.sleep(30)'], directory, dict(os.environ), 0.05)
        self.assertTrue(report['wall_timeout'])
        self.assertLess(report['returncode'], 0)

    def test_production_launcher_always_uses_namespaces(self):
        with patch.object(route, 'launch', return_value=dict(returncode=1, wall_timeout=False)) as launch:
            def fake(command, directory, env, timeout):
                (directory / 'stdout.log').write_text('')
                (directory / 'stderr.log').write_text('namespace denied\n')
                return dict(returncode=1, wall_timeout=False, command=command)
            launch.side_effect = fake
            report = route.execute(self.args, self.root / 'isolated', self.game, 'demo', 'quit\n')
        self.assertEqual(report['process']['command'][:2], ['unshare', '-rn'])
        self.assertEqual(report['outcome'], 'scenario-failed')

    @unittest.skipUnless(shutil.which('xvfb-run') and shutil.which('Xvfb'), 'Xvfb unavailable')
    def test_xvfb_mode(self):
        self.args.mode = 'xvfb'
        report, _ = self.run_route((ROOT / 'tests/playtest/playthrough_demo.txt').read_text())
        if report['outcome'] != 'won':
            output = self.root / f'run-{self.serial}'
            if 'SDL_Init: x11 not available' in (output / 'stderr.log').read_text():
                self.skipTest('Xvfb cannot create a usable X11 server in this environment')
        self.assertEqual(report['outcome'], 'won')


class DetectorTests(unittest.TestCase):
    @staticmethod
    def row(tick, **updates):
        obj = dict(id=1, type=0, alive=True, position=[24, 24], order=1, destination=[600, 24],
                   target=0, carrier=0, boarding=0, progress=1000, mobile=True, path_failures=0, cargo=[])
        row = dict(tick=tick, objects=[obj], income=0, commands=[], queue=[], unsupported=0)
        row.update(updates)
        return row

    def test_stuck_idle_economy_and_repeated_path_events_have_context(self):
        detector = route.Detectors(stuck=2, idle=2, economy=2, path=2)
        for tick in range(4):
            row = self.row(tick)
            row['objects'][0]['path_failures'] = tick
            detector.feed(row)
        kinds = {a['kind'] for a in detector.anomalies}
        self.assertTrue({'stuck', 'economy-stall', 'repeated-path-failure'} <= kinds)
        detector = route.Detectors(idle=2)
        for tick in range(4):
            row = self.row(tick)
            row['objects'][0]['order'] = 0
            detector.feed(row)
        self.assertEqual(detector.anomalies[0]['kind'], 'idle')
        self.assertEqual(detector.anomalies[0]['position'], [24, 24])

    def test_actual_income_even_during_spending_resets_stall_clock(self):
        detector = route.Detectors(economy=2)
        for tick in range(5):
            detector.feed(self.row(tick, income=tick))
        self.assertFalse(detector.anomalies)

    def test_passenger_loss_and_destroyed_transport(self):
        start = self.row(0)
        start['objects'][0].update(type=84, cargo=[2])
        passenger = dict(start['objects'][0], id=2, type=0, cargo=[], carrier=1)
        start['objects'].append(passenger)
        end = copy.deepcopy(start)
        end['tick'] = 1
        for obj in end['objects']:
            obj.update(alive=False, cargo=[])
        detector = route.Detectors()
        detector.feed(start)
        detector.feed(end)
        self.assertTrue({'passenger-lost', 'transport-destroyed'} <= {a['kind'] for a in detector.anomalies})

    def test_missing_or_gapped_snapshots_cannot_pass(self):
        detector = route.Detectors()
        with self.assertRaises(ValueError):
            detector.feed(self.row(1))
        detector.feed(self.row(0))
        with self.assertRaises(ValueError):
            detector.feed(self.row(2))


if __name__ == '__main__':
    unittest.main()
