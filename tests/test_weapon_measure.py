#!/usr/bin/env python3
"""Invented V07 captures test inventory coverage and the repeated-reference gate."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('weapon_measure', ROOT / 'tools/weapon_measure.py')
weapon = importlib.util.module_from_spec(spec)
spec.loader.exec_module(weapon)


class WeaponTests(unittest.TestCase):
    def setUp(self):
        base = ROOT / 'work/parity/weapon-tests'
        base.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=base)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.case = weapon.scenarios()[0]
        digest = hashlib.sha256(json.dumps(self.case, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
        a = dict(scenario=self.case['id'], specification_sha256=digest, map_sha256='a' * 64,
                 controls_sha256='b' * 64, normalization_sha256='c' * 64,
                 first_tick=1, last_tick=3, phase='after_tick')
        objects = [dict(id=i + 1, type=t, side=i, x=i * 256, y=0, hp=100, secondary_hp=100,
                        armor=10, categories=1, alive=1, corpse=0, burn=0)
                   for i, t in enumerate((self.case['source_type'], self.case['target_type']))]
        event = dict(kind=1, id=0, source=1, target=2, weapon=self.case['weapon'], victim=0,
                     x=0, y=0, height=0, before=0, after=0)
        self.reference = dict(schema=1, contract='weapon-v1', origin='reference', alignment=a,
                              samples=[dict(tick=t, objects=copy.deepcopy(objects), flights=[],
                                            events=[event] if t == 2 else [], unavailable=[])
                                       for t in range(1, 4)])
        self.repeat = copy.deepcopy(self.reference)
        self.clone = copy.deepcopy(self.reference)
        self.clone['origin'] = 'clone'

    def compare(self):
        paths = []
        for name in ('reference', 'repeat', 'clone'):
            p = self.root / f'{name}.json'
            p.write_text(json.dumps(getattr(self, name)))
            paths.append(p)
        return weapon.compare(*paths)

    def test_every_existing_weapon_and_descriptor_type_is_accounted_for(self):
        inv = weapon.inventory()
        self.assertEqual(len(inv['types']), 90)
        self.assertEqual({w['weapon'] for w in inv['weapons']}, set(range(60)))
        self.assertTrue(any(not w['owners'] and w['children'] for w in inv['weapons']))
        self.assertTrue(any(w['table'] == 'unresolved' for w in inv['weapons']))
        self.assertTrue(all(not w['measured'] and w['matching'] is None and not w['fixed'] for w in inv['weapons']))
        cases = weapon.scenarios()
        self.assertEqual(len({c['id'] for c in cases}), len(cases))
        attackers = {t['type'] for t in inv['types'] if t['weapon'] is not None and t['weapon'] > 0}
        victims = {t['type'] for t in inv['types'] if t['hp'] is not None and t['hp'] > 0}
        self.assertEqual({(c['source_type'], c['target_type']) for c in cases
                          if c['purpose'] == 'damage_by_armor_and_category'},
                         {(s, t) for s in attackers for t in victims})
        for source in attackers:
            self.assertTrue(set(weapon.projectile.catalogue()['variants']) <=
                            {c['variant'] for c in cases if c['source_type'] == source})
        weapon.plan(self.root / 'plan')
        self.assertTrue('Unmeasured' in (self.root / 'plan/coverage.md').read_text())
        with self.assertRaises(FileExistsError):
            weapon.plan(self.root / 'plan')

    def test_comparison_detects_each_combat_channel_and_tick(self):
        self.assertEqual(self.compare()['status'], 'equal')
        for field in ('hp', 'secondary_hp', 'armor', 'categories', 'burn', 'alive', 'corpse', 'x'):
            saved = copy.deepcopy(self.clone)
            self.clone['samples'][2]['objects'][1][field] ^= 1
            result = self.compare()
            self.assertEqual(result['status'], 'different')
            self.assertEqual(result['first']['tick'], 3)
            self.clone = saved
        self.clone['samples'][1]['events'][0]['height'] = 10
        self.assertEqual(self.compare()['first']['channel'], 'events')
        self.clone = copy.deepcopy(self.reference)
        self.clone['origin'] = 'clone'
        self.clone['samples'][2]['flights'] = [dict(id=1, source=1, target=2, weapon=self.case['weapon'],
                                                   x=10, y=20, height=3, altitude=768)]
        self.assertEqual(self.compare()['first']['channel'], 'flights')

    def test_unstable_reference_prevents_clone_verdict(self):
        self.repeat['samples'][2]['objects'][1]['hp'] = 99
        self.clone['samples'][1]['objects'][1]['hp'] = 98
        result = self.compare()
        self.assertEqual(result['status'], 'reference nondeterminism')
        self.assertEqual(result['first']['tick'], 3)
        self.assertFalse(result['repeatable_difference'])

    def test_missing_misaligned_idle_and_unsupported_captures_fail(self):
        mutations = [lambda b: b['samples'].pop(),
                     lambda b: b['samples'][1].update(tick=3),
                     lambda b: b['samples'][1].update(unavailable=['burn']),
                     lambda b: b['samples'][1].update(events=[]),
                     lambda b: b['samples'][0].update(objects=[]),
                     lambda b: b['samples'][0]['objects'][0].update(hp=True),
                     lambda b: b['samples'][0]['objects'][0].pop('corpse'),
                     lambda b: b['alignment'].update(map_sha256='d' * 64),
                     lambda b: b['alignment'].update(specification_sha256='d' * 64),
                     lambda b: b['alignment'].update(last_tick=4),
                     lambda b: b.update(origin='reference')]
        for mutation in mutations:
            saved = copy.deepcopy(self.clone)
            mutation(self.clone)
            with self.assertRaises(ValueError):
                self.compare()
            self.clone = saved


if __name__ == '__main__':
    unittest.main()
