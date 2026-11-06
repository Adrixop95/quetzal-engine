#!/usr/bin/env python3
"""Invented L8 traces: no reference measurements or executable-derived fixtures."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
ENGINE = sys.argv.pop(1) if len(sys.argv) > 1 else None
loader = importlib.util.spec_from_file_location('ai_measure', ROOT / 'tools/ai_measure.py')
ai = importlib.util.module_from_spec(loader)
loader.loader.exec_module(ai)


def spec(kind='launch'):
    return dict(schema=1, id='invented', horizon=20, repeats=2, tv_limit=0.1,
                reference_audit='synthetic-only', groups=[dict(id='wave', event=kind, side=1,
                                                             town=0, start=5, end=20)])


def event(tick=10, kind='launch', composition=None, x=-1, y=-1):
    return dict(tick=tick, event=kind, side=1, town=0, x=x, y=y,
                composition=composition or [[0, 3], [1, 1]])


def capture(events, origin='reference'):
    rows = [dict(schema=1, contract='audited-ai-v1' if origin == 'reference' else 'engine-ai-v1',
                 origin=origin, hz=12, last_tick=20, audit='synthetic-only')]
    for tick in range(1, 21):
        rows.extend(e for e in events if e['tick'] == tick)
        rows.append(dict(event='tick', tick=tick))
    return [*rows, dict(event='end', tick=20)]


def compare(a, b, scenario=None):
    return ai.compare(scenario or spec(), [capture(run) for run in a],
                      [capture(run, 'engine') for run in b])


class Measurements(unittest.TestCase):
    def test_timing_and_stimulus_latency(self):
        result = compare([[event(9)], [event(11)]], [[event(10)], [event(12)]])
        self.assertTrue(result['passed'])
        self.assertEqual(result['groups'][0]['timing_windows']['first'], [[4, 6], [5, 7]])
        result = compare([[event(9, 'defence')], [event(11, 'defence')]],
                         [[event(12, 'defence')], [event(13, 'defence')]], spec('defence'))
        self.assertTrue('first_timing_window' in result['groups'][0]['differences'])

    def test_joint_distribution_catches_correlations_and_probabilities(self):
        a, b = event(composition=[[0, 4]]), event(composition=[[1, 4]])
        mixed = event(composition=[[0, 2], [1, 2]])
        self.assertTrue('composition_distribution' in compare([[a], [b]], [[mixed], [mixed]])['groups'][0]['differences'])
        # Same types and force sizes; a probability shift still fails.
        result = compare([[a] * 9 + [b], [a] * 9 + [b]], [[a, b] * 5, [a, b] * 5])
        self.assertAlmostEqual(result['groups'][0]['composition_tv'], 0.4)
        self.assertFalse(result['passed'])
        self.assertTrue(compare([[a], [b]], [[b], [a]])['passed'])

    def test_placement_and_variable_wave_counts(self):
        a = event(kind='build_order', composition=[[12, 1]], x=8, y=9)
        b = dict(a, x=9)
        self.assertTrue('placement_distribution' in compare([[a], [a]], [[b], [b]], spec('build_order'))['groups'][0]['differences'])
        # Different numbers of events are supported, with every zero-event run counted.
        a = event()
        self.assertTrue(compare([[a], [a, a]], [[a, a], [a]])['passed'])
        self.assertTrue('event_count_window' in compare([[a], [a]], [[], [a]])['groups'][0]['differences'])
        # Matching first/last waves cannot hide a delayed middle wave.
        result = compare([[event(8), event(10), event(15)]] * 2,
                         [[event(8), event(12), event(15)]] * 2)
        self.assertTrue('1_timing_window' in result['groups'][0]['differences'])

    def test_no_missing_class_can_pass(self):
        for kind in ai.KINDS:
            composition = [[11, 1]] if kind in {'production', 'build_order', 'expansion'} else [[0, 3]]
            a = event(kind=kind, composition=composition)
            result = compare([[a], [a]], [[], []], spec(kind))
            self.assertTrue('missing_behaviour_class' in result['groups'][0]['differences'])
            result = compare([[], []], [[], []], spec(kind))
            self.assertEqual(result['status'], 'needs live run')
            self.assertFalse(result['passed'])

    def test_full_stream_validation(self):
        good = capture([event()], 'engine')
        variants = [good[:-1], good[:-2] + good[-1:], good + [good[-1]]]
        for field, value in [('tick', True), ('side', 8), ('town', -1), ('x', -2),
                             ('composition', [[0, True]]), ('composition', [[0, 1], [0, 1]]),
                             ('event', 'movement')]:
            rows = copy.deepcopy(good)
            next(row for row in rows if row.get('event') == 'launch')[field] = value
            variants.append(rows)
        for rows in variants:
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                ai.trace(rows, spec(), 'engine')
        with self.assertRaises(ValueError):
            ai.trace(good, spec(), 'reference')
        with self.assertRaises(ValueError):
            ai.compare(spec(), [capture([event()])], [good, good])

    def test_cli_reports_blocked_and_rejects_duplicate_runs(self):
        private = ROOT / 'work/parity'
        private.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=private) as directory:
            folder = Path(directory)
            scenario = folder / 'scenario.json'
            scenario.write_text(json.dumps(dict(spec(), reference_audit=None)))
            paths = [folder / f'{i}.jsonl' for i in range(4)]
            command = [sys.executable, '-I', str(ROOT / 'tools/ai_measure.py'), '--scenario', str(scenario),
                       '--reference', *map(str, paths[:2]), '--clone', *map(str, paths[2:]), '--output']
            result = subprocess.run([*command, str(folder / 'blocked')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 77, result.stderr)
            self.assertEqual(json.loads((folder / 'blocked/diff.json').read_text())['status'], 'needs live run')
            scenario.write_text(json.dumps(spec()))
            for i, path in enumerate(paths):
                path.write_text(''.join(json.dumps(row) + '\n' for row in capture([event()], 'reference' if i < 2 else 'engine')))
            result = subprocess.run([*command, str(folder / 'pass')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            paths[3].write_text(paths[3].read_text().rsplit('\n', 2)[0] + '\n')
            result = subprocess.run([*command, str(folder / 'truncated')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2, result.stderr)
            duplicate = list(command)
            duplicate[7] = duplicate[6]
            result = subprocess.run([*duplicate, str(folder / 'duplicate')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2, result.stderr)

    def test_candidate_protocols_remain_unmeasured(self):
        for name in ('ai_passive', 'ai_provocation'):
            data = ai.scenario(json.loads((ROOT / f'tests/parity/{name}.json').read_text()))
            self.assertIsNone(data['reference_audit'])
            self.assertEqual(data['evidence'], 'P')

    @unittest.skipUnless(ENGINE, 'engine binary not supplied')
    def test_headless_trace_and_hash_neutrality(self):
        build = ROOT / 'build'
        build.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=build) as directory:
            folder = Path(directory)
            game = folder / 'game'
            maps, resources = game / 'Maps', game / 'Res'
            maps.mkdir(parents=True); resources.mkdir()
            for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'common', 'fnts', 'light'):
                (resources / f'{name}.xbe').write_bytes(struct.pack('<I', 0))
            (maps / 'ai.mp').write_text(
                '[Globals]\nMapSize=40,40\nSides=2\nTowns=2\nTeams=0\n'
                '[Side0]\nStart_X=8\nStart_Y=8\n'
                '[Side1]\nStart_X=28\nStart_Y=28\nOnlyComp=1\nMoney=12000\n'
                '[Town0]\nSide=0\nType=1\nRace=0\nX=8\nY=8\nUnits=1\nUnit0=8,8,11,30000\n'
                '[Town1]\nSide=1\nType=1\nRace=0\nX=28\nY=28\nUnits=1\nUnit0=28,28,11,30000\n')
            (maps / 'ai.map').write_bytes(struct.pack('<HH', 40, 40) + bytes(6400))
            (maps / 'ai.scn').write_text('')
            for name in ('plain', 'traced', 'repeat'):
                command = [ENGINE, '--headless', '--game', str(game), '--rules-dir', str(ROOT / 'docs/data'),
                           '--no-auto-orders', '--max-ticks', '20', '--tick-hashes', str(folder / f'{name}.hash')]
                if name != 'plain':
                    command += ['--ai-trace', str(folder / f'{name}.jsonl')]
                result = subprocess.run([*command, 'ai'], capture_output=True, text=True,
                                        env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'), timeout=10)
                self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((folder / 'plain.hash').read_bytes(), (folder / 'traced.hash').read_bytes())
            self.assertEqual((folder / 'repeat.jsonl').read_bytes(), (folder / 'traced.jsonl').read_bytes())
            events = ai.trace([json.loads(row) for row in (folder / 'traced.jsonl').read_text().splitlines()], spec(), 'engine')
            self.assertTrue(any(e['event'] == 'build_order' for e in events))
            self.assertTrue(any(e['event'] == 'production' for e in events))


if __name__ == '__main__':
    unittest.main()
