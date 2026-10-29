#!/usr/bin/env python3
"""Synthetic L1 controls: coverage, private map isolation and fail-closed measurements."""
import copy
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('parity_rules', ROOT / 'tools/parity_rules.py')
rules = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rules)


class RuleControls(unittest.TestCase):
    def setUp(self):
        private = ROOT / 'work/parity'
        private.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=private)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.case = copy.deepcopy(next(c for c in rules.scenarios() if c['id'] == 'l1_speed_cannon'))
        self.case['observe']['last_tick'] = 2

    def observations(self, name, hp=2, truncate=False):
        path = self.root / name
        meta = dict(schema=1, mission=1, phase='before_commands', contract='l1:l1_speed_cannon:v1',
                    fields=['x', 'y'], first_tick=1, last_tick=2)
        rows = [dict(tick=1, objects={'source': dict(x=1, y=0)}),
                dict(tick=2, objects={'source': dict(x=hp, y=0)})]
        if truncate:
            rows.pop()
        path.write_text('\n'.join(json.dumps(row) for row in [meta, *rows]) + '\n')
        return path

    def test_coverage_and_no_public_expectations(self):
        cases = rules.scenarios()
        self.assertEqual({c['mechanic'] for c in cases}, {
            'speed', 'damage_armor', 'reload', 'range', 'projectile_flight',
            'build_time', 'price', 'income', 'sight', 'regeneration'})
        self.assertEqual(len({c['id'] for c in cases}), len(cases))
        self.assertTrue(all(c['evidence'] == 'P' and 'expectations' not in c for c in cases))

    def test_expectations_and_first_divergence(self):
        a, b, c = self.observations('a'), self.observations('b'), self.observations('c', 3)
        output = self.root / 'comparison'
        self.assertEqual(rules.evaluate(self.case, a, b, c, output), 1)
        diff = json.loads((output / 'diff.json').read_text())
        self.assertEqual((diff['tick'], diff['object_id'], diff['field']), (2, 'source', 'x'))
        self.assertEqual((diff['reference'], diff['engine']), (2, 3))
        values = json.loads((output / 'expectations.json').read_text())
        self.assertTrue(values)
        self.assertTrue(all(type(value) is int for row in values for value in row))
        with self.assertRaises(FileExistsError):
            rules.evaluate(self.case, a, b, c, output)
        self.assertEqual(rules.evaluate(self.case, a, b, a, self.root / 'equal'), 0)

    def test_unstable_or_incomplete_reference_cannot_create_expectations(self):
        a, b = self.observations('a'), self.observations('b', 3)
        output = self.root / 'unstable'
        self.assertEqual(rules.evaluate(self.case, a, b, a, output), 1)
        self.assertFalse((output / 'expectations.json').exists())
        self.assertEqual(json.loads((output / 'diff.json').read_text())['category'],
                         'reference_semantic_nondeterminism')
        c = self.observations('truncated', truncate=True)
        with self.assertRaises(ValueError):
            rules.evaluate(self.case, a, a, c, self.root / 'incomplete')
        self.assertFalse((self.root / 'incomplete/expectations.json').exists())
        self.case['observe']['fields'].append('hp')
        with self.assertRaises(ValueError):
            rules.evaluate(self.case, a, a, a, self.root / 'missing-field')

    def test_initial_identity_is_required(self):
        path = self.observations('wrong-identity')
        path.write_text(path.read_text().replace('"source"', '"unrelated"'))
        with self.assertRaises(ValueError):
            rules.evaluate(self.case, path, path, path, self.root / 'wrong-identity-output')
        self.assertFalse((self.root / 'wrong-identity-output/expectations.json').exists())

    def test_missing_private_inputs_skip_without_launch(self):
        with patch.object(rules.report, 'execute') as execute:
            self.assertEqual(rules.probe([self.case], self.root / 'absent', self.root / 'missing',
                                        self.root / 'skip', 1), 77)
            execute.assert_not_called()

    def test_candidate_copy_and_namespace_failure(self):
        source = self.root / 'source'
        (source / 'Maps').mkdir(parents=True)
        (source / 'Res').mkdir()
        (source / 'Aztec.exe').write_text('synthetic prerequisite')
        (source / 'profile.plr').write_text('unchanged')
        (source / 'Maps/mission_1.mp').write_text(
            '[Globals]\nMapName=mission_1\nTowns=1\nTeams=1\nCurrentTeam=0\n'
            '[Town0]\nUnits=1\nUnit0=1,1,0,1\n[Team0]\nUnits=0\n')
        for ext in ('dat', 'rm', 'vis', 'spr', 'mit'):
            (source / f'Maps/mission_1.{ext}').write_bytes(b'synthetic')
        terrain = struct.pack('<HH', 90, 60) + bytes(4 * 90 * 60)
        (source / 'Maps/mission_1.map').write_bytes(terrain)
        runtime = self.root / 'runtime'
        runtime.touch()
        failure = rules.subprocess.CompletedProcess([], 1)
        # Real map generation, mocked namespace process only; no fallback launch is allowed.
        original = rules.subprocess.run

        def isolated(argv, **kwargs):
            if 'unshare' in argv:
                self.assertTrue('-ec' in argv)
                self.assertEqual(kwargs['env']['SDL_VIDEODRIVER'], 'dummy')
                self.assertEqual(kwargs['env']['SDL_AUDIODRIVER'], 'dummy')
                return failure
            return original(argv, **kwargs)

        with patch.object(rules.subprocess, 'run', side_effect=isolated) as run:
            output = self.root / 'probe'
            self.assertEqual(rules.probe([self.case], source, runtime, output, 1), 1)
            self.assertEqual(run.call_count, 2)
        candidate = output / self.case['id'] / 'game'
        self.assertTrue((candidate / 'Res').is_symlink())
        self.assertFalse((candidate / 'Maps').is_symlink())
        self.assertFalse((candidate / 'Maps/mission_1.mp').exists())
        self.assertEqual((source / 'Maps/mission_1.map').read_bytes(), terrain)
        self.assertEqual((source / 'profile.plr').read_text(), 'unchanged')
        dat = (candidate / f"Maps/{self.case['id']}.dat").read_bytes()
        self.assertEqual((dat[0], dat[16 * 90 + 16]), (0, 2))
        row = json.loads((output / 'report.json').read_text())[0]
        self.assertEqual(row['category'], 'experiment_failed')
        self.assertIsNone(row['tick'])
        with self.assertRaises(ValueError):
            rules.report.private_output(ROOT / 'docs/invalid')


if __name__ == '__main__':
    unittest.main()
