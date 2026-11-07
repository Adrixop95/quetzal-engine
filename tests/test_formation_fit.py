"""Generated offline comparison fixture; no original game measurements."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

PROBE = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None
ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('formation_fit', ROOT / 'tools/formation_fit.py')
FIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIT)


class FormationFitTest(unittest.TestCase):
    def test_exact_timeline_and_residual(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'invented.mp').write_text('[Globals]\nMapSize=12,12\n[Team0]\n'
                                             'Unit0=8,6,0\nUnit1=2,6,0\n')
            (root / 'invented.dat').write_bytes(bytes([2]) * 144)
            (root / 'types.csv').write_text('id,name,kind,param\n0,synthetic,0,0\n')
            (root / 'desc_marine.csv').write_text(
                'param,d0,d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11\n'
                '0,0,100,0,100,100,8,0,-1,0,0,0,1\n')
            ticks = [1, 4, 10, 16, 22, 28, 40]
            walker = {'type': 0, 'state_at_tick1': 2,
                      'tile_per_tick': [[x, 6] for x in [2, 3, 4, 5, 6, 7, 7]]}
            idle = {'type': 0, 'state_at_tick1': 1, 'tile_per_tick': [[8, 6]] * len(ticks)}
            data = {'ticks': ticks, 'missions': {'invented': {
                'groups': [{'anchor_tile': [8, 6], 'members': [idle, walker]}],
                'other_objects_tile_at_tick1': []}}}
            first = FIT.compare(data, root, PROBE, root, True)
            self.assertEqual(first, FIT.compare(data, root, PROBE, root, True))
            report = first['invented']
            self.assertEqual(report['before_final'], 0)
            self.assertEqual(report['after_final'], 1)
            self.assertEqual(report['after_tick_tiles'], len(ticks))
            self.assertEqual(report['residuals'], [])
            walker['tile_per_tick'][-1] = [7, 5]
            report = FIT.compare(data, root, PROBE, root, True)['invented']
            self.assertEqual(report['after_final'], 0)
            self.assertEqual(len(report['residuals']), 1)
            # Expected final data has no influence on candidate routing.
            self.assertEqual(report['residuals'][0]['candidate'], [7, 6])
            json.dumps(report)


if __name__ == '__main__':
    unittest.main()
