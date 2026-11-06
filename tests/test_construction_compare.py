#!/usr/bin/env python3
"""Invented construction captures exercise coverage, repeatability and divergence."""
import copy
import csv
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('construction', ROOT / 'tools/construction_compare.py')
construction = importlib.util.module_from_spec(spec)
spec.loader.exec_module(construction)


class ConstructionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'frame.ppm').write_bytes(b'P6\n8 6\n255\n' + bytes(8 * 6 * 3))
        alignment = dict(scenario='invented', map_sha256='a' * 64, controls_sha256='b' * 64,
                         normalization_sha256='c' * 64, building_type=14, camera=[0, 0, 1],
                         resolution=[8, 6], overlay_region=[0, 0, 8, 6], first_tick=1,
                         last_tick=3, phase='before_commands')
        state = dict(site=dict(id=1, hp=10, hp_max=100, stage=0, progress=100, sprite=4,
                              overlay_sprite=-1, alive=1, x=100, y=100, footprint=[[2, 2]]),
                     workers={'2': dict(state=1, x=50, y=50, site=1)}, money={'0': 900},
                     queue=[dict(id=1, type=14, remaining=9, total=10, paid=100, started=1, paused=0, site=1)],
                     selection=[1], rally=None, sounds=[], completion=[])
        self.reference = dict(schema=1, contract='construction-v1', origin='reference', alignment=alignment,
                              samples=[dict(tick=t, state=copy.deepcopy(state), unavailable=[], overlay='frame.ppm')
                                       for t in range(1, 4)])
        self.repeat = copy.deepcopy(self.reference)
        self.clone = copy.deepcopy(self.reference)
        self.clone['origin'] = 'clone'

    def compare(self):
        paths = []
        for name in ('reference', 'repeat', 'clone'):
            path = self.root / f'{name}.json'
            path.write_text(json.dumps(getattr(self, name)))
            paths.append(path)
        return construction.compare(*paths)

    def test_catalogue_covers_every_structure_and_keeps_modification_off(self):
        catalogue = json.loads(construction.SPEC.read_text())
        types = [t for values in catalogue['classes'].values() for t in values]
        with (ROOT / 'docs/data/types.csv').open() as stream:
            structures = {int(r['id']) for r in csv.DictReader(stream) if r['kind'] in ('Building', 'Defence')}
        self.assertEqual(set(types), structures)
        self.assertEqual(len(types), len(structures))
        self.assertEqual(catalogue['modifications'], {'parallel_construction': False})
        self.assertEqual(catalogue['status'], 'needs live run')

    def test_equal_and_each_lifecycle_field(self):
        self.assertEqual(self.compare()['status'], 'equal')
        mutations = [lambda s: s['site'].update(stage=1), lambda s: s['site'].update(hp=11),
                     lambda s: s['site'].update(sprite=5), lambda s: s['site'].update(footprint=[[3, 2]]),
                     lambda s: s['workers']['2'].update(state=2), lambda s: s['workers']['2'].update(x=51),
                     lambda s: s['money'].update({'0': 899}), lambda s: s['queue'][0].update(paid=99),
                     lambda s: s.update(selection=[], rally=[10, 11]),
                     lambda s: s.update(sounds=[17]), lambda s: s.update(completion=[1]),
                     lambda s: s.update(site=None)]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                saved = copy.deepcopy(self.clone)
                mutation(self.clone['samples'][1]['state'])
                result = self.compare()
                self.assertEqual(result['status'], 'different')
                self.assertEqual(result['first']['tick'], 2)
                self.clone = saved

    def test_unstable_reference_prevents_clone_verdict(self):
        self.repeat['samples'][2]['state']['money']['0'] -= 1
        self.clone['samples'][0]['state']['money']['0'] -= 1
        result = self.compare()
        self.assertEqual(result['status'], 'reference nondeterminism')
        self.assertEqual(result['first']['tick'], 3)

    def test_exact_pixels_and_late_missing_frame(self):
        pixels = bytearray(8 * 6 * 3)
        pixels[0] = 255
        (self.root / 'different.ppm').write_bytes(b'P6\n8 6\n255\n' + pixels)
        self.clone['samples'][0]['overlay'] = 'different.ppm'
        self.assertEqual(self.compare()['first']['different_pixels'], 1)
        self.clone['samples'][-1]['overlay'] = 'missing.ppm'
        with self.assertRaises(OSError):
            self.compare()

    def test_incomplete_unavailable_and_unaligned_captures_fail_closed(self):
        mutations = [lambda b: b['samples'].pop(), lambda b: b['samples'][1].update(tick=1),
                     lambda b: b['samples'][-1].update(unavailable=['workers']),
                     lambda b: b['samples'][-1]['state'].pop('completion'),
                     lambda b: b['samples'][-1]['state']['site'].update(hp=True),
                     lambda b: b['samples'][-1]['state']['site'].update(footprint=[[2, 2], [2, 2]]),
                     lambda b: b['samples'][-1]['state']['queue'][0].update(remaining=11),
                     lambda b: b['samples'][-1].update(overlay='../outside.ppm'),
                     lambda b: b['alignment'].update(normalization_sha256='d' * 64),
                     lambda b: b['alignment'].update(phase='after_commands'),
                     lambda b: b.update(origin='reference')]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                saved = copy.deepcopy(self.clone)
                self.clone['samples'][0]['state']['money']['0'] -= 1
                mutation(self.clone)
                with self.assertRaises(ValueError):
                    self.compare()
                self.clone = saved


if __name__ == '__main__':
    unittest.main()
