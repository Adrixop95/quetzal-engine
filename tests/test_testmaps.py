#!/usr/bin/env python3
"""Invented terrain exercises the private test-map generator without game assets."""
import configparser
import csv
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VIEWER = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None
GENERATOR = ROOT / 'tools/make_testmaps.py'


def ini(path):
    result = configparser.ConfigParser(interpolation=None)
    result.read(path)
    return result


class TestMaps(unittest.TestCase):
    def run_generator(self, game, *args, success=True):
        result = subprocess.run([sys.executable, '-I', str(GENERATOR), '--game', str(game), *args],
                                capture_output=True, text=True, timeout=120)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def fixture(self, root):
        maps = root / 'Maps'
        maps.mkdir()
        w = h = 180
        (maps / 'MiSsIoN_14.MaP').write_bytes(struct.pack('<HH', w, h) + struct.pack('<I', 1000) * (w * h))
        (maps / 'MiSsIoN_14.DaT').write_bytes(bytes([2]) * w * h)
        (maps / 'MiSsIoN_14.Rm').write_bytes(struct.pack('<HH', w, h) + bytes(w * h * 2))
        (maps / 'MiSsIoN_14.ViS').write_bytes(struct.pack('<H', 4) + (struct.pack('<HH', w, h) + bytes(w * h)) * 4)
        # A one-pixel invented raw preview, independent of any original art.
        (maps / 'MiSsIoN_14.SpR').write_bytes(struct.pack('<IHHIIII', 0xE0AC0000, 1, 1, 1, 0, 0, 0) + b'\0')
        (maps / 'MiSsIoN_14.Mp').write_text('[Globals]\nTileSet=0\n')
        return maps

    def validate(self, maps, name):
        doc = ini(maps / f'{name}.mp')
        g = doc['Globals']
        self.assertEqual(g['mapname'], name)
        self.assertEqual(g['mapsize'], '180,180')
        ids = set()
        occupied = set()
        footprints = []
        all_types = set()
        with (ROOT / 'docs/data/types.csv').open() as stream:
            kinds = {int(r['id']): r['kind'] for r in csv.DictReader(stream)}
        for kind, key in [('Side', 'sides'), ('Town', 'towns'), ('Team', 'teams'), ('Boat', 'boats'), ('Johnka', 'johnkas')]:
            sections = [s for s in doc.sections() if s.startswith(kind) and s[len(kind):].isdigit()]
            self.assertEqual(len(sections), int(g[key]))
            self.assertEqual(set(sections), {f'{kind}{i}' for i in range(int(g[key]))})
            for section in sections:
                group = doc[section]
                if kind == 'Side':
                    self.assertTrue(int(group['race']) in range(3))
                    self.assertEqual(group['disabledid'], '')
                    self.assertGreaterEqual(int(group['startmoney']), 100000)
                    continue
                self.assertTrue(int(group['side']) in range(int(g['sides'])))
                scenario = int(group['scenarioid'])
                self.assertNotIn(scenario, ids)
                ids.add(scenario)
                if kind in ('Boat', 'Johnka'):
                    all_types.add(84 if kind == 'Boat' else 88)
                    self.assertTrue(int(group['y']) in (-2, 182))
                    continue
                if kind == 'Town':
                    self.assertTrue(int(group['type']) in range(4))
                    hall = int(group['x']), int(group['y'])
                    self.assertNotIn(hall, occupied)
                    occupied.add(hall)
                    footprints.append((*hall, 2))
                unit_keys = {k for k in group if k.startswith('unit') and k[4:].isdigit()}
                self.assertEqual(unit_keys, {f'unit{i}' for i in range(int(group['units']))})
                for key in unit_keys:
                    x, y, tid, hp = map(int, group[key].split(','))
                    self.assertTrue(0 <= x < 180 and 0 <= y < 180)
                    self.assertTrue(tid in range(90))
                    self.assertGreater(hp, 0)
                    self.assertLessEqual(hp, 10000000)
                    self.assertNotIn((x, y), occupied)
                    occupied.add((x, y))
                    radius = 2 if kinds[tid] in ('Building', 'Defence') else 1
                    footprints.append((x, y, radius))
                    all_types.add(tid)
                    self.assertEqual(kinds[tid] in ('Building', 'Defence'), kind == 'Town')
        for i, (x, y, radius) in enumerate(footprints):
            self.assertTrue(all(max(abs(x - xx), abs(y - yy)) >= radius + rr + 2
                                for xx, yy, rr in footprints[:i]), 'reserved footprints overlap')
        scn = (maps / f'{name}.scn').read_bytes()
        self.assertTrue(b'\r\n' in scn)
        self.assertNotIn(b'\n', scn.replace(b'\r\n', b''))
        if name == 'ztest_all':
            self.assertEqual(all_types, set(range(89)))
            self.assertTrue(all(int(doc[f'Side{i}']['camerascount']) >= 1 for i in range(3)))
            for race in range(3):
                self.assertEqual({int(doc[s]['type']) for s in doc if s.startswith('Town')
                                  and int(doc[s]['side']) == race}, {0, 1, 2, 3})
            manifest = json.loads((maps / f'{name}.layout.json').read_text())
            self.assertEqual({p['hp'] for p in manifest['placements'] if p['type'] == 0}, {6000, 3960, 1980, 600})
            for p in manifest['placements']:
                if p['side'] < 3:
                    self.assertTrue(3 + p['side'] * 58 <= p['y'] < 3 + (p['side'] + 1) * 58)
            for side, native in enumerate((range(27), range(27, 53), range(53, 82))):
                self.assertTrue(set(native) <= {p['type'] for p in manifest['placements'] if p['side'] == side})
            for tid in sorted(set(range(90)) - {84, 88, 89}):
                self.assertEqual({p['fraction'] for p in manifest['placements'] if p['type'] == tid}, {100, 66, 33, 10})
        else:
            self.assertTrue(b'side(0).victory' in scn)
            self.assertTrue(b'side(0).defeat' in scn)
            self.assertTrue(b'localTime == 300' in scn)
            self.assertTrue(b'localTime == 600' in scn)
        if VIEWER:
            result = subprocess.run([str(VIEWER), '--maps', str(maps), '--dump', name], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(name in result.stdout)
        return doc, len(occupied)

    def test_variants_and_determinism(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            maps = self.fixture(root)
            sources = {p.name: p.read_bytes() for p in maps.iterdir()}
            self.run_generator(root)
            self.validate(maps, 'ztest_all')
            pvc, normal = self.validate(maps, 'ztest_battle')
            self.assertEqual(pvc['Side0']['onlycomp'], '0')
            initial = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in maps.iterdir()}
            self.run_generator(root, success=False)
            self.run_generator(root, '--force')
            self.assertEqual(initial, {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in maps.iterdir()})
            for ext in ('map', 'dat', 'rm', 'vis', 'spr'):
                source = next(data for name, data in sources.items() if name.lower().endswith('.' + ext))
                for name in ('ztest_all', 'ztest_battle'):
                    self.assertEqual(source, (maps / f'{name}.{ext}').read_bytes())
            self.run_generator(root, '--force', '--variant', 'cvc', '--races', '2,0')
            cvc, _ = self.validate(maps, 'ztest_battle')
            self.assertEqual(cvc['Side0']['onlycomp'], '1')
            self.assertEqual(cvc['Side1']['onlycomp'], '1')
            self.assertEqual(cvc['Side0']['race'], '2')
            self.run_generator(root, '--force', '--variant', 'stress')
            _, stress = self.validate(maps, 'ztest_battle')
            self.assertGreater(stress, normal * 3)
            for name, data in sources.items():
                self.assertEqual((maps / name).read_bytes(), data)
            victim = maps / 'ztest_all.scn'
            victim.unlink()
            victim.symlink_to(maps / 'MiSsIoN_14.Mp')
            self.run_generator(root, '--force', success=False)
            self.assertEqual((maps / 'MiSsIoN_14.Mp').read_bytes(), sources['MiSsIoN_14.Mp'])

    def test_reject_bad_template_and_arguments(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            maps = self.fixture(root)
            for args in [('--races', '0,0'), ('--races', '0,9'), ('--source-mission', '../outside')]:
                self.run_generator(root, *args, success=False)
            (maps / 'MiSsIoN_14.DaT').write_bytes(b'\0')
            self.run_generator(root, success=False)
            self.assertFalse((maps / 'ztest_all.mp').exists())


if __name__ == '__main__':
    unittest.main()
