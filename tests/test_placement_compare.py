#!/usr/bin/env python3
"""Invented placement observations: strict coverage, alignment and divergence controls."""
import copy
import csv
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('placement', ROOT / 'tools/placement_compare.py')
placement = importlib.util.module_from_spec(spec)
spec.loader.exec_module(placement)


class PlacementTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'frame.ppm').write_bytes(b'P6\n8 6\n255\n' + bytes(8 * 6 * 3))
        alignment = dict(scenario='invented', map_sha256='a' * 64, controls_sha256='b' * 64,
                         building_type=13, camera=[0, 0, 1], resolution=[8, 6],
                         overlay_region=[0, 0, 8, 6], first_tick=1, last_tick=3, phase='before_input')
        self.reference = dict(schema=1, contract='placement-v1', origin='reference', alignment=alignment,
                              samples=[dict(tick=tick, active=True, valid=True, snapped=[12, 12],
                              footprint=[[11, 12, True], [12, 12, True]], feedback='', rotation=None,
                              ghost_sprite=None, cursor='arrow', accepted=False, overlay='frame.ppm')
                              for tick in range(1, 4)])
        self.clone = copy.deepcopy(self.reference)
        self.clone['origin'] = 'clone'

    def compare(self):
        for name, data in [('reference', self.reference), ('clone', self.clone)]:
            (self.root / f'{name}.json').write_text(json.dumps(data))
        return placement.compare(self.root / 'reference.json', self.root / 'clone.json')

    def test_every_structure_class_is_catalogued_once(self):
        catalogue = json.loads(placement.SPEC.read_text())
        self.assertEqual(set(catalogue['classes']), {'outpost', 'village', 'town', 'fortress',
                                                  'economy', 'military', 'defence', 'mine'})
        types = [tid for values in catalogue['classes'].values() for tid in values]
        with (ROOT / 'docs/data/types.csv').open() as stream:
            structures = {int(row['id']) for row in csv.DictReader(stream) if row['kind'] in ('Building', 'Defence')}
        self.assertEqual(set(types), structures)
        self.assertEqual(len(types), len(structures))
        self.assertEqual(len(placement.plan()), len(types) * len(catalogue['variants']))

    def test_equal_and_first_decision_difference(self):
        self.assertIsNone(self.compare()['first'])
        row = self.clone['samples'][1]
        row['valid'] = False
        row['footprint'][0][2] = False
        result = self.compare()
        self.assertEqual(result['first']['tick'], 2)
        self.assertEqual(result['first']['field'], 'valid')
        self.assertEqual(result['status'], 'different')

    def test_snapping_ghost_rotation_cursor_feedback_and_acceptance(self):
        changes = dict(snapped=[13, 12], ghost_sprite='units:12:0', rotation=1,
                       cursor='build', feedback='Blocked', accepted=True)
        for field, value in changes.items():
            with self.subTest(field=field):
                saved = copy.deepcopy(self.clone)
                self.clone['samples'][2][field] = value
                self.assertEqual(self.compare()['first']['field'], field)
                self.assertEqual(self.compare()['first']['tick'], 3)
                self.clone = saved

    def test_pixel_difference_and_late_missing_capture(self):
        pixels = bytearray(8 * 6 * 3)
        pixels[3] = 255
        (self.root / 'different.ppm').write_bytes(b'P6\n8 6\n255\n' + pixels)
        self.clone['samples'][0]['overlay'] = 'different.ppm'
        first = self.compare()['first']
        self.assertEqual((first['tick'], first['field'], first['different_pixels']), (1, 'overlay', 1))
        self.clone['samples'][-1]['overlay'] = 'missing.ppm'
        with self.assertRaises(OSError):
            self.compare()  # do not report an earlier mismatch from incomplete data

    def test_invalid_observations_fail_closed(self):
        mutations = [lambda b: b['samples'].pop(),
                     lambda b: b['samples'][1].update(tick=1),
                     lambda b: b['samples'][0].update(valid=False),
                     lambda b: b['samples'][0].update(valid=1),
                     lambda b: b['samples'][0].update(snapped=None),
                     lambda b: b['samples'][0].update(footprint=[[12, 12, True], [12, 12, True]]),
                     lambda b: b['samples'][0].update(active=False),
                     lambda b: b['samples'][0].update(overlay='../outside.ppm'),
                     lambda b: b['alignment'].update(camera=[0, 0, 2]),
                     lambda b: b['alignment'].update(phase='after_input'),
                     lambda b: b.update(origin='reference')]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                saved = copy.deepcopy(self.clone)
                mutation(self.clone)
                with self.assertRaises(ValueError):
                    self.compare()
                self.clone = saved

    def test_inactive_cancel_is_observed(self):
        self.clone['samples'][2].update(active=False, valid=None, snapped=None, footprint=[])
        self.assertEqual(self.compare()['first']['field'], 'active')

    def test_confirmation_can_close_mode_before_next_boundary(self):
        for bundle in (self.reference, self.clone):
            bundle['samples'][2].update(active=False, valid=None, snapped=None,
                                        footprint=[], accepted=True)
        self.assertIsNone(self.compare()['first'])


if __name__ == '__main__':
    unittest.main()
