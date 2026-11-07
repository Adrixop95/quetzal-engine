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

    def test_outcome_origin_tracks_first_local_result_and_replay(self):
        self.script.write_text(
            'trigger(4) conditions localTime == 2 actions side(1).victory end\n'
            'trigger(9) conditions localTime == 10 actions\n'
            'side(local).defeat side(local).victory end\n'
            'trigger(9) conditions localTime == 10 actions side(local).victory end\n')
        save = Path(self.temp.name) / 'outcome.azs'
        first = self.run_engine('--save-at', '5', '--save', str(save))
        self.assertEqual(first['status'], 'defeat')
        expected = dict(trigger=9, action_line=3, tick=10)
        self.assertEqual(first['outcome_origin'], expected)
        resumed = self.run_engine('--resume', str(save))
        self.assertEqual(resumed['outcome_origin'], expected)
        self.script.write_text('trigger(2) conditions localTime > 100 actions side(local).victory end\n')
        self.assertIsNone(self.run_engine()['outcome_origin'])

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
        self.script.write_text('trigger(0) conditions always actions side(local).moveCommand(1,2) end\n')
        report = self.run_engine()
        self.assertTrue(report['script_errors'])
        self.assertEqual(len(report['unsupported_actions']), 1)
        self.assertEqual(report['unsupported_executions'], 20)
        self.script.unlink()
        self.assertTrue('missing mission script' in self.run_engine()['script_errors'])

    def test_action_sites_distinguish_execution_from_terminal_conditions(self):
        self.script.write_text(
            'trigger(3) conditions localTime == 2 actions boat(0).activate trigger(3).disable end\n'
            'trigger(3) conditions always actions johnka(0).enable end\n'
            'trigger(4) conditions localTime > 100 actions submarine(0).go end\n')
        save = Path(self.temp.name) / 'actions.azs'
        report = self.run_engine('--no-auto-orders', '--save-at', '5', '--save', str(save))
        rows = report['action_observations']
        self.assertEqual(len(rows), 4)
        self.assertEqual([(r['trigger_index'], r['action_index']) for r in rows],
                         [(0, 0), (0, 1), (1, 0), (2, 0)])
        self.assertEqual([(r['executions'], r['first_tick'], r['last_tick']) for r in rows],
                         [(1, 2, 2), (1, 2, 2), (1, 1, 1), (0, 0, 0)])
        self.assertEqual(rows[0]['false_condition_lines'], [1])
        self.assertFalse(rows[0]['enabled'])
        self.assertFalse(rows[2]['enabled'])
        self.assertEqual(rows[2]['false_condition_lines'], [])
        self.assertTrue(rows[3]['enabled'])
        self.assertEqual(report['unsupported_executions'], 0)
        self.assertTrue(all(row['supported'] for row in rows))
        resumed = self.run_engine('--no-auto-orders', '--resume', str(save))
        self.assertEqual(rows, resumed['action_observations'])
        self.assertEqual(report['state_hash'], resumed['state_hash'])
        output = Path(self.temp.name)
        harness.write_action_findings(output, [dict(id='synthetic', runs=[dict(engine=report)])])
        findings = (output / 'actions.md').read_text()
        self.assertTrue('SubmarineGo | implemented | 0 | 0 / 0 | True | [3]' in findings)
        self.assertTrue('does not establish that the original never fires' in findings)
        harness.validate_action_observations(report)
        for malformed in (None, [dict(rows[0], executions=0)], [rows[0], rows[0]],
                          [dict(rows[0], first_tick=report['ticks'] + 1)]):
            with self.subTest(malformed=malformed), self.assertRaises(ValueError):
                harness.validate_action_observations(dict(report, action_observations=malformed))

    def test_disabled_victory_is_reported_even_with_true_conditions(self):
        self.script.write_text(
            'trigger(0) conditions always actions trigger(1).disable trigger(0).disable end\n'
            'trigger(1) conditions always actions side(local).victory end\n')
        report = self.run_engine('--no-auto-orders')
        self.assertEqual(report['status'], 'timeout')
        self.assertEqual(report['unmet_objectives'], ['victory trigger 1: disabled at termination'])
        victory = report['action_observations'][-1]
        self.assertEqual(victory['kind'], 'SideVictory')
        self.assertEqual(victory['executions'], 0)
        self.assertEqual(victory['false_condition_lines'], [])

    def test_order_observations_do_not_change_simulation(self):
        orders = Path(self.temp.name) / 'movement.orders'
        observations = Path(self.temp.name) / 'objects.jsonl'
        orders.write_text('1 move 0 3 2 1\n')
        args = ('--no-auto-orders', '--orders', str(orders))
        control = self.run_engine(*args)
        measured = self.run_engine(*args, '--observations', str(observations), '--order-observations')
        self.assertEqual(control['state_hash'], measured['state_hash'])
        rows = [json.loads(line) for line in observations.read_text().splitlines()]
        self.assertEqual(rows[0]['phase'], 'before_commands')
        before = rows[1]['objects']['engine:1']
        after = rows[2]['objects']['engine:1']
        self.assertEqual(set(after), set(rows[0]['fields']))
        self.assertEqual(before['order'], 0)
        self.assertEqual((after['dest_x'], after['dest_y']), (3 * 48 + 24, 2 * 48 + 24))
        self.assertEqual((after['target'], after['tactic'], after['scenario']), (0, -1, -1))

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

    def test_flying_orders_and_save_continuation(self):
        mission = self.maps / 'Mission_1.mp'
        mission.write_text(mission.read_text().replace('Unit0=1,1,3,100', 'Unit0=1,1,7,100'))
        orders = Path(self.temp.name) / 'flight.orders'
        first = Path(self.temp.name) / 'first.txt'
        second = Path(self.temp.name) / 'second.txt'
        save = Path(self.temp.name) / 'flight.azs'
        for selection in ('', ' 1'):
            with self.subTest(selection=selection):
                orders.write_text(f'1 move 0 5 2{selection}\n7 attack-move 0 6 3{selection}\n')
                route = ('--no-auto-orders', '--orders', str(orders))
                baseline = self.run_engine(*route, '--tick-hashes', str(first),
                                           '--save-at', '5', '--save', str(save))
                self.assertEqual(baseline['commands'], 2)
                resumed = self.run_engine(*route, '--tick-hashes', str(second), '--resume', str(save))
                runs = [dict(returncode=0, engine=r, tick_hashes=str(p))
                        for r, p in ((baseline, first), (resumed, second))]
                self.assertTrue(harness.paired_hashes(*runs, start=5)['equal'])
                self.assertNotEqual(baseline['state_hash'], self.run_engine('--no-auto-orders')['state_hash'])

    def test_explicit_game_save_ignores_unselected_environment(self):
        alternate = Path(self.temp.name) / 'unselected'
        (alternate / 'Res').mkdir(parents=True)
        (alternate / 'Res' / 'Light.xbe').write_bytes(b'unselected input')
        save = Path(self.temp.name) / 'selected.azs'
        baseline = self.run_engine('--no-auto-orders', '--save-at', '5', '--save', str(save))
        with patch.dict(os.environ, AZTEC_GAME=str(alternate)):
            resumed = self.run_engine('--no-auto-orders', '--resume', str(save))
            self.assertEqual(resumed['state_hash'], baseline['state_hash'])
            self.assertEqual(resumed['status'], 'victory')
        # The selected installation is still fingerprinted, even without usable sight data.
        (self.game / 'Res' / 'light.xbe').write_bytes(struct.pack('<I', 0) + b'changed')
        refused = self.run_engine('--no-auto-orders', '--resume', str(save), code=1)
        self.assertTrue('content is incompatible' in refused['error'])

    def test_headless_save_loads_in_mission_ui(self):
        profile = Path(self.temp.name) / 'profile'
        slot = profile / 'Saves' / 'engine_slot1.azs'
        slot.parent.mkdir(parents=True)
        self.run_engine('--no-auto-orders', '--save-at', '5', '--save', str(slot))
        unused = Path(self.temp.name) / 'unused'
        (unused / 'Res').mkdir(parents=True)
        (unused / 'Res' / 'Light.xbe').write_bytes(b'unselected input')
        playtest = Path(self.temp.name) / 'resume.txt'
        playtest.write_text('key ctrl+L\nexpect tick == 6\nwait 8\n'
                            'expect outcome == 1\nexpect tick == 10\nquit\n')
        result = subprocess.run([str(ENGINE), '--game', str(self.game),
                                 '--rules-dir', str(ROOT / 'docs/data'), '--software',
                                 '--no-audio', '--script', str(playtest), '1'],
                                env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
                                         AZTEC_USER_DIR=str(profile), AZTEC_GAME=str(Path(self.temp.name) / 'unused')),
                                capture_output=True, text=True, timeout=8)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue('loaded game (tick 5)' in result.stdout)
        self.assertTrue(slot.is_file())

    def test_automatic_orders_skip_ineligible_targets(self):
        mission = self.maps / 'Mission_1.mp'
        mission.write_text(mission.read_text()
                           .replace('Teams=2', 'Teams=3')
                           .replace('Unit0=1,1,3,100', 'Unit0=1,1,0,100')
                           .replace('Unit0=6,6,3,100', 'Unit0=2,1,7,100') +
                           '[Team2]\nSide=1\nUnits=1\nUnit0=6,6,3,100\n')
        orders = Path(self.temp.name) / 'eligible.orders'
        orders.write_text('1 attack-move 0 6 6 1\n')
        expected = self.run_engine('--no-auto-orders', '--orders', str(orders))
        # The nearer flying scout cannot be damaged by this infantry weapon.
        self.assertEqual(self.run_engine(), expected)

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

    def test_failed_launch_is_unverified_in_matrix(self):
        (ROOT / 'work').mkdir(exist_ok=True)
        temp = tempfile.TemporaryDirectory(dir=ROOT / 'work')
        self.addCleanup(temp.cleanup)
        output = Path(temp.name) / 'failed'
        with patch.object(harness.subprocess, 'Popen', side_effect=OSError('unavailable')), contextlib.redirect_stdout(io.StringIO()):
            code = harness.main(['--game', str(self.game), '--engine', str(ENGINE),
                                 '--out', str(output), '--missions', '1', '--max-ticks', '20'])
        self.assertEqual(code, 1)
        row = json.loads((output / 'summary.json').read_text())['missions'][0]
        self.assertIsNone(row['deterministic'])
        self.assertFalse(row['passed'])

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

        self.script.write_text('trigger(0) conditions localTime == 15 actions side(local).victory end\n')
        for expected in (0, 1):
            output = Path(temp.name) / str(expected)
            if expected:
                self.script.write_text('trigger(0) conditions always actions side(local).defeat end\n')
            with patch.object(harness.subprocess, 'Popen', side_effect=synthetic_launch), contextlib.redirect_stdout(io.StringIO()):
                code = harness.main(['--game', str(self.game), '--engine', str(ENGINE),
                                     '--out', str(output), '--missions', '1', '--max-ticks', '20'])
            self.assertEqual(code, expected)
            summary = json.loads((output / 'summary.json').read_text())
            self.assertEqual(len(summary['missions']), 1)
            self.assertTrue(summary['missions'][0]['deterministic'])
            self.assertTrue((output / 'aztec_engine').is_file())
            self.assertEqual(len(summary['matrix']), 15)
            if not expected:
                self.assertTrue(summary['missions'][0]['save_continuation']['equal'])
                self.assertEqual(summary['missions'][0]['paired_ticks']['first_tick'], 0)
            self.assertTrue((output / 'matrix.md').is_file())

    def test_fresh_process_save_and_exact_continuation(self):
        save = Path(self.temp.name) / 'mid.azs'
        first = Path(self.temp.name) / 'first.txt'
        second = Path(self.temp.name) / 'second.txt'
        orders = Path(self.temp.name) / 'continuation.orders'
        orders.write_text('1 move 0 2 2\n7 move 0 3 3\n')
        route = ('--no-auto-orders', '--orders', str(orders))
        baseline = self.run_engine(*route, '--tick-hashes', str(first), '--save-at', '5', '--save', str(save))
        resumed = self.run_engine(*route, '--tick-hashes', str(second), '--resume', str(save))
        self.assertEqual(resumed['commands'], 2)
        pair = [dict(returncode=0, engine=r, tick_hashes=str(p))
                for r, p in ((baseline, first), (resumed, second))]
        self.assertTrue(harness.paired_hashes(*pair, start=5)['equal'])
        self.assertEqual(harness.read_hashes(second)[0][0], 5)
        text = second.read_text().splitlines()
        text[1] = '6 0'
        second.write_text('\n'.join(text) + '\n')
        self.assertEqual(harness.paired_hashes(*pair, start=5)['first_mismatch_tick'], 6)
        second.write_text('\n'.join(text[:-1]) + '\n')
        self.assertIsNone(harness.paired_hashes(*pair, start=5)['equal'])
        second.write_text('5 0\n7 0\n')
        self.assertIsNone(harness.paired_hashes(*pair, start=5)['equal'])
        # A changed script must refuse the save before continuing.
        self.script.write_text('trigger(0) conditions always actions side(local).defeat end\n')
        self.assertEqual(self.run_engine('--resume', str(save), code=1)['status'], 'error')

    def test_full_fifteen_mission_matrix(self):
        for mission in range(2, 16):
            for ext, source in (('mp', self.maps / 'Mission_1.mp'), ('map', self.maps / 'mission_1.map')):
                (self.maps / f'mission_{mission}.{ext}').write_bytes(source.read_bytes())
        for mission in range(1, 16):
            (self.maps / f'mission_{mission}.scn').write_text(
                'trigger(0) conditions localTime == 3 actions trigger(0).disable end\n'
                'trigger(1) conditions localTime == 15 actions side(local).victory end\n')
        temp = tempfile.TemporaryDirectory(dir=ROOT / 'work')
        self.addCleanup(temp.cleanup)
        output = Path(temp.name) / 'all'
        plan = Path(temp.name) / 'routes.json'
        routes = [dict(id=f'mission-{m}', mission=m, route='Move, regroup, then wait for invented objectives',
                       expected_outcome='victory', save_tick=5,
                       steps=[dict(tick=1, action='move', side=0, x=2, y=2, ids=[1]),
                              dict(tick=7, action='attack-move', side=0, x=3, y=3, ids=[1])],
                       checkpoints=[dict(id='started', trigger_index=0, action_index=0, kind='TriggerDisable'),
                                    dict(id='won', trigger_index=1, action_index=0, kind='SideVictory')])
                  for m in range(1, 16)]
        plan.write_text(json.dumps(routes))
        common = ['--game', str(self.game), '--engine', str(ENGINE), '--max-ticks', '20',
                  '--routes', str(plan), '--no-auto-orders', '--require-reference']
        real_popen = subprocess.Popen
        with patch.object(harness.subprocess, 'Popen',
                          side_effect=lambda cmd, **kw: real_popen(cmd[2:], **kw)), contextlib.redirect_stdout(io.StringIO()):
            code = harness.main([*common, '--out', str(output)])
        self.assertEqual(code, 1)
        summary = json.loads((output / 'summary.json').read_text())
        self.assertEqual([m['mission'] for m in summary['matrix']], list(range(1, 16)))
        self.assertTrue(all(not r['verified'] and r['reference']['status'] == 'needs live run'
                            for r in summary['missions']))
        self.assertTrue(all(m['progression'] == 'pending' for m in summary['matrix']))
        self.assertFalse(list(output.rglob('diff.json')))
        references = Path(temp.name) / 'reference'
        references.mkdir()
        for row in summary['missions']:
            # Invented oracle fixture; never export this as real reference evidence.
            timeline = dict(schema=1, contract='campaign-checkpoints-v1', mission=row['mission'],
                            route_sha256=row['route_sha256'], status='victory', ticks=15,
                            events=[dict(id='started', tick=3), dict(id='won', tick=15)])
            for repetition in (1, 2):
                (references / f"{row['id']}-{repetition}.json").write_text(json.dumps(timeline))
        paired = Path(temp.name) / 'paired'
        with patch.object(harness.subprocess, 'Popen',
                          side_effect=lambda cmd, **kw: real_popen(cmd[2:], **kw)), contextlib.redirect_stdout(io.StringIO()):
            code = harness.main([*common, '--out', str(paired), '--reference-dir', str(references)])
        self.assertEqual(code, 0)
        rows = json.loads((paired / 'summary.json').read_text())['missions']
        self.assertTrue(all(r['verified'] and r['passed'] and r['save_continuation']['equal'] for r in rows))
        self.assertTrue(all(r['resumed_run']['engine']['commands'] == 2 for r in rows))
        self.assertEqual(len(list(paired.rglob('diff.json'))), 15)
        self.assertTrue('first_divergence' in (paired / 'matrix.md').read_text())
        for repetition in (1, 2):
            path = references / f'mission-1-{repetition}.json'
            timeline = json.loads(path.read_text())
            timeline['events'][-1]['tick'] = 14
            path.write_text(json.dumps(timeline))
        different = Path(temp.name) / 'different'
        with patch.object(harness.subprocess, 'Popen',
                          side_effect=lambda cmd, **kw: real_popen(cmd[2:], **kw)), contextlib.redirect_stdout(io.StringIO()):
            code = harness.main([*common, '--missions', '1', '--out', str(different),
                                 '--reference-dir', str(references)])
        self.assertEqual(code, 1)
        diff = json.loads((different / 'mission-1-reference/diff.json').read_text())
        self.assertEqual((diff['layer'], diff['category'], diff['first_divergence']['tick']), ('L7', 'timing', 14))

    def test_route_validation_and_checkpoint_order(self):
        valid = dict(steps=[dict(tick=1, action='move', side=0, x=2, y=2, ids=[])],
                     checkpoints=[dict(id='start', trigger_index=0, action_index=0, kind='TriggerDisable'),
                                  dict(id='finish', trigger_index=1, action_index=0, kind='SideVictory')])
        harness.validate_route(valid, 20)
        for change in (dict(orders='ambiguous'), dict(steps='bad'), dict(checkpoints=[{}]),
                       dict(checkpoints=valid['checkpoints'] * 2)):
            with self.subTest(change=change), self.assertRaises(ValueError):
                harness.validate_route(dict(valid, **change), 20)
        for change in (dict(tick=True), dict(tick=21), dict(action='board'), dict(ids=[1, 1]), dict(side=8)):
            with self.subTest(change=change), self.assertRaises(ValueError):
                harness.validate_route(dict(steps=[dict(valid['steps'][0], **change)]), 20)
        self.script.write_text('trigger(0) conditions localTime == 3 actions trigger(0).disable end\n'
                               'trigger(1) conditions localTime == 15 actions side(local).victory end\n')
        run = dict(returncode=0, engine=self.run_engine('--no-auto-orders'))
        self.assertTrue(harness.checkpoint_timeline(valid, run)['passed'])
        self.assertFalse(harness.checkpoint_timeline(dict(checkpoints=valid['checkpoints'][::-1]), run)['passed'])
        self.assertFalse(harness.checkpoint_timeline(dict(checkpoints=[dict(valid['checkpoints'][0], kind='Wrong')]), run)['passed'])
        run['engine']['action_observations'][-1]['first_tick'] = 3
        self.assertFalse(harness.checkpoint_timeline(valid, run)['passed'])

    def test_reference_first_difference_and_invalid_evidence(self):
        checkpoints = [dict(id='finish', trigger_index=0, action_index=0, kind='SideVictory')]
        run = dict(returncode=0, engine=self.run_engine())
        row = dict(id='route', mission=1, route_sha256='a' * 64, max_ticks=20, runs=[run, run],
                   checkpoints=checkpoints, checkpoint_checks=[dict(passed=True, events=[dict(id='finish', tick=10)])] * 2)
        directory = Path(self.temp.name)
        value = dict(schema=1, contract='campaign-checkpoints-v1', mission=1, route_sha256='a' * 64,
                     status='victory', ticks=10, events=[dict(id='finish', tick=10)])

        def write_pair(first, second=None):
            for i, data in enumerate((first, first if second is None else second), 1):
                (directory / f'route-{i}.json').write_text(json.dumps(data))

        self.assertIsNone(harness.reference_comparison(row, directory)['equal'])
        write_pair(value)
        self.assertTrue(harness.reference_comparison(row, directory)['equal'])
        write_pair(dict(value, events=[dict(id='finish', tick=9)]))
        difference = harness.reference_comparison(row, directory)['first_divergence']
        self.assertEqual((difference['tick'], difference['field']), (9, 'checkpoint'))
        write_pair(dict(value, status='defeat', ticks=8, events=[]))
        self.assertEqual(harness.reference_comparison(row, directory)['first_divergence']['tick'], 8)
        write_pair(value, dict(value, status='defeat'))
        self.assertEqual(harness.reference_comparison(row, directory)['status'], 'needs live run')
        for change in (dict(mission=2), dict(route_sha256='b' * 64), dict(ticks=True), dict(events='bad'),
                       dict(events=[dict(id='finish', tick=11)]), dict(events=[dict(id='unknown', tick=3)])):
            write_pair(dict(value, **change))
            self.assertIsNone(harness.reference_comparison(row, directory)['equal'])
        (directory / 'route-1.json').write_text('{')
        self.assertIsNone(harness.reference_comparison(row, directory)['equal'])
        args = type('Args', (), dict(max_ticks=20, no_auto_orders=True, order_interval=120))()
        identity = harness.route_identity(dict(steps=[]), args)
        self.assertNotEqual(identity, harness.route_identity(dict(steps=[], checkpoints=checkpoints), args))

    def test_expected_defeat_scenario_and_fresh_retry(self):
        temp = tempfile.TemporaryDirectory(dir=ROOT / 'work')
        self.addCleanup(temp.cleanup)
        plan = Path(temp.name) / 'plan.json'
        self.script.write_text('trigger(0) conditions localTime == 15 actions side(local).defeat end\n'
                               'trigger(1) conditions localTime == 100 actions side(local).victory end\n')
        real_popen = subprocess.Popen
        for attempt, expected in enumerate(('defeat', 'victory')):
            if attempt:
                self.script.write_text('trigger(0) conditions localTime == 15 actions side(local).victory end\n')
            plan.write_text(json.dumps([dict(id=expected, mission=1, route='Wait for the synthetic outcome',
                                            expected_outcome=expected, save_tick=5)]))
            output = Path(temp.name) / f'attempt-{attempt}'
            with patch.object(harness.subprocess, 'Popen',
                              side_effect=lambda cmd, **kw: real_popen(cmd[2:], **kw)), contextlib.redirect_stdout(io.StringIO()):
                code = harness.main(['--game', str(self.game), '--engine', str(ENGINE), '--missions', '1',
                                     '--scenarios', str(plan), '--out', str(output), '--max-ticks', '20'])
            self.assertEqual(code, 0)
            row = json.loads((output / 'summary.json').read_text())['missions'][0]
            self.assertTrue(row['passed'])
            self.assertEqual(row['runs'][0]['status'], expected)

    def test_scenarios_and_missing_missions_remain_failures(self):
        plan = Path(self.temp.name) / 'scenarios.json'
        plan.write_text(json.dumps([dict(id='defeat', mission=1, route='Wait for scripted defeat',
                                        expected_outcome='defeat', save_tick=5)]))
        self.assertEqual(harness.scenarios(plan, [1], 20)[0]['expected_outcome'], 'defeat')
        plan.write_text(json.dumps([dict(id='../bad', mission=1, route='Wait',
                                        expected_outcome='victory', save_tick=5)]))
        with self.assertRaises(ValueError):
            harness.scenarios(plan, [1], 20)
        matrix = harness.mission_matrix({1: self.script}, [1, 2], [])
        self.assertEqual(matrix[1]['availability'], 'missing')
        self.assertEqual(len(matrix), 15)
        temp = tempfile.TemporaryDirectory(dir=ROOT / 'work')
        self.addCleanup(temp.cleanup)
        output = Path(temp.name) / 'missing'
        with contextlib.redirect_stdout(io.StringIO()):
            code = harness.main(['--game', str(self.game), '--engine', str(ENGINE),
                                 '--missions', '2', '--out', str(output), '--max-ticks', '20'])
        self.assertEqual(code, 1)
        summary = json.loads((output / 'summary.json').read_text())
        self.assertEqual(summary['missions'][0]['error'], 'missing mission data')
        self.assertEqual(summary['matrix'][1]['availability'], 'missing')

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
