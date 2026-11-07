#!/usr/bin/env python3
"""Invented V06/V10 controls; these do not establish original-game parity."""
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
spec = importlib.util.spec_from_file_location('tactic_measure', ROOT / 'tools/tactic_measure.py')
measure = importlib.util.module_from_spec(spec)
spec.loader.exec_module(measure)
ENGINE = Path(sys.argv.pop()).resolve() if len(sys.argv) > 1 else None


class TacticControls(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def bundle(self, origin):
        obj = dict(type=0, side=0, hp=100, hp_max=100, alive=1, x=256, y=512,
                   dest_x=1, dest_y=2, order=0, target=0, tactic=7, follow=-1,
                   mode=3, resume=0, cooldown=0, visible=[1, 0])
        side = dict(money=1000, units=1, structures=0, selected_tactic=7,
                    selected=[1], groups=[[1]] + [[] for _ in range(9)], queue=[])
        row = dict(tick=1, objects={'1': obj}, sides=[side, dict(copy.deepcopy(side), units=0, selected=[])])
        alignment = dict(scenario='invented', first_tick=1, last_tick=3, phase='before_commands', viewer=0)
        for key in measure.ALIGNMENT:
            if key.endswith('_sha256'):
                alignment[key] = 'a' * 64
        return dict(schema=1, contract='tactics-v1', origin=origin, alignment=alignment,
                    samples=[dict(copy.deepcopy(row), tick=t) for t in range(1, 4)], unavailable=[])

    def compare(self, change=None):
        captures = [self.bundle(o) for o in ('reference', 'reference', 'clone')]
        if change:
            change(captures)
        paths = [self.root / f'{i}.json' for i in range(3)]
        for path, value in zip(paths, captures):
            path.write_text(json.dumps(value))
        return measure.compare(paths)

    def test_complete_contract_and_earliest_tick_field(self):
        value, code = self.compare()
        self.assertEqual(code, 0)
        self.assertEqual(value['ticks'], 3)
        for field, replacement in [('target', 2), ('follow', 10), ('mode', 2), ('visible', [1, 1]), ('hp', 99)]:
            def change(captures):
                captures[2]['samples'][1]['objects']['1'][field] = replacement
            with self.subTest(field=field):
                value, code = self.compare(change)
                self.assertEqual(code, 1)
                self.assertEqual(value['tick'], 2)
                self.assertEqual(value['difference']['field'], f'.objects.1.{field}' + ('[1]' if field == 'visible' else ''))
        for field, replacement in [('selected', []), ('groups', [[] for _ in range(10)]),
                                   ('money', 999), ('units', 2), ('queue', [[0, 0, 10, 10, 0, 0, 0, 0]])]:
            value, code = self.compare(lambda c: c[2]['samples'][2]['sides'][0].update({field: replacement}))
            self.assertEqual((code, value['tick']), (1, 3))

    def test_reference_instability_is_blocked_even_when_clone_matches_first_run(self):
        result, code = self.compare(lambda c: c[1]['samples'][2]['objects']['1'].update(target=9))
        self.assertEqual((code, result['status']), (77, 'needs live run'))
        self.assertEqual(result['reason'], 'reference repeats differ')

    def test_full_validation_precedes_early_difference(self):
        mutations = [lambda c: c[2]['samples'].pop(),
                     lambda c: c[1]['samples'][2].update(tick=2),
                     lambda c: c[2]['samples'][2]['objects']['1'].update(hp=True),
                     lambda c: c[0]['samples'][2]['objects']['1'].update(visible=[1]),
                     lambda c: c[0]['samples'][2]['sides'][0].update(selected=[1, 1]),
                     lambda c: c[0]['samples'][2]['sides'][0].update(groups=[]),
                     lambda c: c[0]['samples'][2]['sides'][0].update(queue=[[0]]),
                     lambda c: c[0].update(unavailable=['selection']),
                     lambda c: c[0].update(contract='engine-tactics-v1'),
                     lambda c: c[0]['alignment'].update(settings_sha256=''),
                     lambda c: c[0]['alignment'].update(first_tick=True),
                     lambda c: c[0]['alignment'].update(viewer=7),
                     lambda c: c[0]['samples'][2].update(unknown=1)]
        for mutation in mutations:
            def change(c):
                c[2]['samples'][0]['objects']['1']['hp'] = 99
                mutation(c)
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.compare(change)
        with self.assertRaises(ValueError):
            self.compare(lambda c: c[2]['alignment'].update(packets_sha256='b' * 64))

    def test_missing_reused_and_duplicate_keys(self):
        paths = [self.root / f'{i}.json' for i in range(3)]
        result, code = measure.compare(paths)
        self.assertEqual((code, result['status']), (77, 'needs live run'))
        with self.assertRaises(ValueError):
            measure.compare([paths[0]] * 3)
        self.compare()
        paths[0].write_text(paths[0].read_text().replace('"hp": 100', '"hp": 100, "hp": 101'))
        with self.assertRaises(ValueError):
            measure.compare(paths)

    def test_plan_covers_all_modes_and_required_controls(self):
        cases = measure.plan()
        self.assertEqual(len({c['id'] for c in cases}), len(cases))
        self.assertEqual({c['mode'] for c in cases}, {0, 1, 2, 3})
        for name in ('idle-in-range', 'moving', 'under-attack', 'group-bounds', 'scan-phase',
                     'assignment', 'follow-chain-cycle', 'fog-selection', 'population', 'global-difficulty'):
            self.assertEqual(sum(c['id'].startswith(name + '-mode-') for c in cases), 4)

    @unittest.skipUnless(ENGINE, 'engine binary not supplied')
    def test_headless_diagnostics_are_repeatable_and_hash_neutral(self):
        game = self.root / 'game'
        maps = game / 'Maps'
        maps.mkdir(parents=True)
        resources = game / 'Res'
        resources.mkdir()
        for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'common', 'fnts', 'light'):
            (resources / f'{name}.xbe').write_bytes(struct.pack('<I', 0))
        (maps / 'ztest_battle.mp').write_text(
            '[Globals]\nMapSize=20,20\nSides=2\nTeams=2\n'
            '[Side0]\nStart_X=5\nStart_Y=5\n[Side1]\nStart_X=7\nStart_Y=5\n'
            '[Team0]\nSide=0\nUnits=1\nUnit0=5,5,27,30000\n'
            '[Team1]\nSide=1\nUnits=1\nUnit0=7,5,0,30000\n')
        (maps / 'ztest_battle.map').write_bytes(struct.pack('<HH', 20, 20) + bytes(1600))
        (maps / 'ztest_battle.scn').write_text('')
        orders = self.root / 'orders.txt'
        orders.write_text('1 move 0 10 5 1\n')
        def run(name, traced):
            hashes = self.root / f'{name}.hashes'
            argv = [str(ENGINE), '--headless', '--game', str(game), '--rules-dir', str(ROOT / 'docs/data'),
                    '--no-auto-orders', '--orders', str(orders), '--max-ticks', '12', '--tick-hashes', str(hashes)]
            if traced:
                argv += ['--tactic-trace', str(self.root / f'{name}.jsonl')]
            result = subprocess.run([*argv, 'ztest_battle'], capture_output=True, text=True, timeout=8,
                                    env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'))
            self.assertEqual(result.returncode, 0, result.stderr)
            return hashes.read_bytes()
        self.assertEqual(run('plain', False), run('trace', True))
        self.assertEqual(run('trace', True), run('repeat', True))
        trace = self.root / 'trace.jsonl'
        self.assertEqual(trace.read_bytes(), (self.root / 'repeat.jsonl').read_bytes())
        rows = [json.loads(line) for line in trace.read_text().splitlines()]
        self.assertEqual(rows[0]['contract'], 'engine-tactics-v1')
        self.assertEqual(rows[0]['phase'], 'before_commands')
        self.assertEqual(rows[-1], dict(event='end', tick=12))
        self.assertEqual(len(rows), 14)
        for tick, row in enumerate(rows[1:-1], 1):
            measure.sample(row, tick)
        self.assertEqual(rows[1]['objects']['1']['dest_x'], 0)
        self.assertEqual(rows[2]['objects']['1']['dest_x'], 10 * 48 + 24)
        self.assertTrue(any(r['objects']['1']['target'] for r in rows[1:-1]))


if __name__ == '__main__':
    unittest.main()
