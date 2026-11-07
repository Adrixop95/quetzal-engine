#!/usr/bin/env python3
"""Invented combat captures: completeness, identity, geometry and evidence limits."""
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
spec = importlib.util.spec_from_file_location('projectile_measure', ROOT / 'tools/projectile_measure.py')
measure = importlib.util.module_from_spec(spec)
spec.loader.exec_module(measure)
ENGINE = Path(sys.argv.pop()).resolve() if len(sys.argv) > 1 else None


class ProjectileControls(unittest.TestCase):
    def setUp(self):
        private = ROOT / 'work/parity/projectile-tests'
        private.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=private)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def capture(self):
        header = dict(schema=1, contract='engine-combat-v1', origin='engine',
                      position_scale=256, height_scale=1, first_tick=1, last_tick=4)
        launch = dict(tick=1, event='launch', id=1, source=9, target=2, side=0, kind=0,
                      x=0, y=0, height=0, altitude=0, victim=0, victim_x=0, victim_y=0,
                      victim_type=-1, victim_side=-1, relation=-1, before=0, after=0)
        position = dict(launch, event='position', tick=2, x=256)
        impact = dict(launch, event='impact', tick=3, x=512)
        damage = dict(impact, event='damage', victim=2, victim_x=768, victim_type=0,
                      victim_side=0, relation=1, before=100, after=90)
        return [header, launch, dict(tick=1, event='tick', objects=[]),
                position, dict(tick=2, event='tick', objects=[]), impact, damage,
                dict(damage, victim=3, victim_x=1024, after=95),
                dict(tick=3, event='tick', objects=[]),
                dict(tick=4, event='tick', objects=[]), dict(event='end', tick=4)]

    def summarize(self, rows):
        path = self.root / 'trace.jsonl'
        path.write_text(''.join(json.dumps(row) + '\n' for row in rows))
        return measure.summarize(path)

    def test_numeric_samples_do_not_invent_parity_or_burn(self):
        result = self.summarize(self.capture())
        flight = result['flights'][1]
        self.assertEqual((flight['launch_tick'], flight['impact_tick']), (1, 3))
        self.assertEqual([s['damage'] for s in flight['damage']], [10, 5])
        self.assertEqual(flight['damage'][0]['distance_squared_fixed'], 256 ** 2)
        self.assertEqual(flight['damage'][0]['relation'], 1)
        self.assertEqual(result['status'], 'needs live run')
        rows = self.capture()
        rows.insert(8, dict(rows[6], event='miss', victim=4, after=100))
        misses = self.summarize(rows)['flights'][1]['misses']
        self.assertEqual(len(misses), 1)
        self.assertEqual(misses[0]['damage'], 0)
        for key in ('burn_duration', 'splash_radius', 'falloff', 'friendly_fire'):
            self.assertIsNone(result[key])

    def test_reject_truncation_missing_positions_and_identity_reuse(self):
        mutations = [lambda r: r.pop(), lambda r: r.pop(3),
                     lambda r: r.insert(2, copy.deepcopy(r[1])),
                     lambda r: r[3].update(id=99),
                     lambda r: r[0].update(origin='reference'),
                     lambda r: r[6].update(before=True),
                     lambda r: r[6].update(tick=4),
                     lambda r: r[0].update(last_tick=5),
                     lambda r: r[6].update(source=12),
                     lambda r: r[6].update(x=1234)]
        for mutation in mutations:
            rows = self.capture()
            mutation(rows)
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.summarize(rows)

    def test_candidate_is_private_and_actual_positions_are_retained(self):
        source = self.root / 'source'
        maps = source / 'Maps'
        maps.mkdir(parents=True)
        (source / 'Res').mkdir()
        (source / 'Aztec.exe').write_text('invented executable marker')
        (source / 'test.plr').write_text('unchanged')
        (maps / 'mission_14.mp').write_text('[Globals]\nTileSet=0\n')
        (maps / 'mission_14.map').write_bytes(struct.pack('<HH', 180, 180) + bytes(4 * 180 * 180))
        (maps / 'mission_14.dat').write_bytes(bytes([2]) * 180 * 180)
        for ext in ('rm', 'vis', 'spr'):
            (maps / f'mission_14.{ext}').write_bytes(b'invented')
        output = self.root / 'candidate'
        private = measure.prepare(measure.scenarios()[0], source, output)
        self.assertTrue((private / 'Res').is_symlink())
        self.assertFalse((private / 'Maps').is_symlink())
        self.assertEqual(list((private / 'Maps').glob('*.mp')), [private / 'Maps/ztest_battle.mp'])
        self.assertTrue(b'\r\n' in (private / 'Maps/ztest_battle.scn').read_bytes())
        pending = json.loads((output / 'pending.json').read_text())
        self.assertEqual(pending['status'], 'needs live run')
        self.assertEqual(len(pending['placements']), 2)
        (private / 'test.plr').write_text('mutated')
        self.assertEqual((source / 'test.plr').read_text(), 'unchanged')
        with self.assertRaises(FileExistsError):
            measure.prepare(measure.scenarios()[0], source, output)
        with self.assertRaises(ValueError):
            measure.prepare(measure.scenarios()[0], source, ROOT / 'docs/invalid')
        building_case = dict(measure.scenarios()[0], source_type=17, target_type=14,
                             source_hp=12345, target_hp=23456)
        buildings = self.root / 'buildings'
        measure.prepare(building_case, source, buildings)
        pending = json.loads((buildings / 'pending.json').read_text())
        self.assertEqual([(p['type'], p['hp']) for p in pending['placements']],
                         [(17, 12345), (14, 23456)])
        ini = measure.maps.read_ini(buildings / 'game/Maps/ztest_battle.mp')
        self.assertEqual(ini['Town0']['Units'], '1')
        self.assertEqual(ini['Town1']['Units'], '1')
        self.assertFalse(any(s.startswith('Team') for s in ini.sections()))

    def test_protocol_covers_families_and_controls(self):
        cases = measure.scenarios()
        self.assertEqual(len(cases), 126)
        self.assertEqual({c['family'] for c in cases},
                         {'arrow', 'bolt', 'catapult', 'cannon', 'fire', 'magic', 'air'})
        self.assertTrue(all(c['map'] == 'ztest_battle' and c['evidence'] == 'P' for c in cases))
        self.assertEqual(len({c['id'] for c in cases}), len(cases))

    @unittest.skipUnless(ENGINE, 'engine binary not supplied')
    def test_engine_capture_round_trip_and_hashes(self):
        game = self.root / 'synthetic'
        maps = game / 'Maps'
        maps.mkdir(parents=True)
        resources = game / 'Res'
        resources.mkdir()
        for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'common', 'fnts', 'light'):
            (resources / f'{name}.xbe').write_bytes(struct.pack('<I', 0))
        (maps / 'ztest_battle.mp').write_text(
            '[Globals]\nMapSize=20,20\nSides=2\nTeams=2\n'
            '[Side0]\nStart_X=5\nStart_Y=5\n'
            '[Side1]\nStart_X=7\nStart_Y=5\n'
            '[Team0]\nSide=0\nUnits=1\nUnit0=5,5,27,30000\n'
            '[Team1]\nSide=1\nUnits=1\nUnit0=7,5,0,30000\n')
        (maps / 'ztest_battle.map').write_bytes(struct.pack('<HH', 20, 20) + bytes(1600))
        (maps / 'ztest_battle.scn').write_text('')
        orders = self.root / 'orders.txt'
        orders.write_text('1 attack-move 0 7 5 1\n')
        def run(name, trace, observe_orders=False):
            hashes = self.root / f'{name}.hashes'
            argv = [str(ENGINE), '--headless', '--profile', 'faithful', '--game', str(game), '--rules-dir', str(ROOT / 'docs/data'),
                    '--no-auto-orders', '--orders', str(orders), '--max-ticks', '10',
                    '--tick-hashes', str(hashes)]
            if trace:
                argv += ['--combat-trace', str(self.root / f'{name}.jsonl')]
            if observe_orders:
                argv += ['--observations', str(self.root / f'{name}.objects.jsonl'), '--order-observations']
            completed = subprocess.run([*argv, 'ztest_battle'], capture_output=True, text=True,
                                       env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'), timeout=8)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            return hashes.read_text()
        self.assertEqual(run('plain', False), run('traced', True))
        self.assertEqual(run('traced', True), run('repeat', True))
        self.assertEqual((self.root / 'traced.jsonl').read_bytes(), (self.root / 'repeat.jsonl').read_bytes())
        result = measure.summarize(self.root / 'traced.jsonl')
        self.assertTrue(result['flights'])
        self.assertTrue(any(f['damage'] for f in result['flights'].values()))
        self.assertEqual(run('traced', True), run('combined', True, observe_orders=True))
        self.assertEqual((self.root / 'traced.jsonl').read_bytes(), (self.root / 'combined.jsonl').read_bytes())
        rows = [json.loads(line) for line in (self.root / 'combined.objects.jsonl').read_text().splitlines()]
        self.assertEqual(rows[0]['phase'], 'before_commands')
        self.assertTrue({'order', 'dest_x', 'dest_y', 'target', 'tactic', 'scenario'} <= set(rows[0]['fields']))
        self.assertEqual(len(rows), 11)
        for row in rows[1:]:
            self.assertTrue(row['objects'])
            for obj in row['objects'].values():
                self.assertEqual(set(obj), set(rows[0]['fields']))

    @unittest.skipUnless(ENGINE, 'engine binary not supplied')
    def test_headless_help_exposes_trace(self):
        result = subprocess.run([ENGINE, '--headless', '--help'], capture_output=True, text=True, check=True)
        self.assertTrue('--combat-trace FILE' in result.stdout)
        self.assertTrue('--observations FILE [--order-observations]' in result.stdout)


if __name__ == '__main__':
    unittest.main()
