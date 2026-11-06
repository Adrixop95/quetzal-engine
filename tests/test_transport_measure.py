"""Invented transport samples test evidence gates, not original vessel semantics."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


TOOL = Path(__file__).resolve().parents[1] / 'tools' / 'transport_measure.py'
SPEC = importlib.util.spec_from_file_location('transport_measure', TOOL)
measure = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(measure)


class TransportMeasure(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.paths = []
        self.data = []
        for source, run in (('reference', 'first'), ('reference', 'repeat'), ('engine', 'engine')):
            capture = self.root / f'{run}.log'
            capture.write_text('Invented capture\n')
            data = dict(schema=1, source=source, run_id=run, case='synthetic-control',
                        site=measure.site_id(measure.SITES[0]), observer='synthetic-v1',
                        alignment='simulation tick zero at fixture start; pixels',
                        binary_sha256='a' * 64, input_sha256='b' * 64,
                        artifacts=[dict(path=capture.name, sha256=hashlib.sha256(capture.read_bytes()).hexdigest())],
                        observations={name: [dict(tick=1, value={'synthetic': 1})] for name in measure.CLASSES})
            self.paths.append(self.root / f'{run}.json')
            self.data.append(data)
        self.save()

    def save(self):
        for path, data in zip(self.paths, self.data):
            path.write_text(json.dumps(data))

    def compare(self, engine=True):
        self.save()
        return measure.compare(self.paths[0], self.paths[1], self.paths[2] if engine else None, measure.CLASSES)

    def test_site_inventory(self):
        result = subprocess.run([sys.executable, '-I', str(TOOL), 'plan'], capture_output=True, text=True, check=True)
        plan = json.loads(result.stdout)
        self.assertEqual(len(plan['sites']), 19)
        self.assertEqual(len({s['id'] for s in plan['sites']}), 19)
        self.assertEqual({m: sum(s[0] == m for s in measure.SITES) for m in (2, 7, 9, 15)},
                         {2: 4, 7: 5, 9: 5, 15: 5})
        self.assertTrue(all(s['status'] == 'needs live run' for s in plan['sites']))

    def test_each_behaviour_class(self):
        self.assertEqual(self.compare()['status'], 'sample match')
        original = copy.deepcopy(self.data)
        for name in measure.CLASSES:
            with self.subTest(behaviour=name):
                self.data = copy.deepcopy(original)
                self.data[2]['observations'][name][0]['value']['synthetic'] = 2
                result = self.compare()
                self.assertEqual(result['status'], 'confirmed sample divergence')
                self.assertEqual(result['classes'][name], 'confirmed sample divergence')
                self.data[1]['observations'][name][0]['value']['synthetic'] = 2
                self.assertEqual(self.compare()['status'], 'needs live run')
                del self.data[1]['observations'][name]
                self.assertEqual(self.compare()['classes'][name], 'missing reference samples')

    def test_timing_order_and_completion_are_compared(self):
        self.data[2]['observations']['boarding'][0]['tick'] = 2
        self.data[0]['observations']['unloading'][0]['value'] = {'cargo': [1, 2]}
        self.data[1]['observations']['unloading'][0]['value'] = {'cargo': [1, 2]}
        self.data[2]['observations']['unloading'][0]['value'] = {'cargo': [2, 1]}
        result = self.compare()
        for name in ('boarding', 'unloading'):
            self.assertEqual(result['classes'][name], 'confirmed sample divergence')
        self.assertEqual(self.compare(False)['status'], 'needs live run')

    def test_missing_engine_samples(self):
        del self.data[2]['observations']['completion']
        self.assertEqual(self.compare()['status'], 'needs live run')

    def test_repetition_and_provenance(self):
        original = copy.deepcopy(self.data)
        for field in ('run_id', 'case', 'site', 'binary_sha256', 'input_sha256', 'observer', 'alignment'):
            with self.subTest(field=field):
                self.data = copy.deepcopy(original)
                self.data[1][field] = self.data[0][field] if field == 'run_id' else 'different'
                with self.assertRaises(ValueError):
                    self.compare()
        self.data = copy.deepcopy(original)
        self.data[1]['artifacts'] = self.data[0]['artifacts']
        with self.assertRaisesRegex(ValueError, 'distinct'):
            self.compare()

    def test_failed_captures_and_bad_samples(self):
        original = copy.deepcopy(self.data)
        for bad in ([], [{'tick': -1, 'value': {'x': 1}}], [{'tick': True, 'value': {'x': 1}}],
                    [{'tick': 1, 'value': {}}], [{'tick': 2, 'value': {'x': 1}}, {'tick': 1, 'value': {'x': 1}}]):
            self.data = copy.deepcopy(original)
            self.data[0]['observations']['route'] = bad
            with self.assertRaises(ValueError):
                self.compare()
        self.data = copy.deepcopy(original)
        (self.root / 'first.log').write_text('changed')
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            self.compare()
        (self.root / 'first.log').write_text('')
        with self.assertRaisesRegex(ValueError, 'absent or empty'):
            self.compare()

    def test_cli_failure_is_nonzero(self):
        result = subprocess.run([sys.executable, '-I', str(TOOL), 'compare',
                                 '--reference', str(self.paths[0]), '--repeat', str(self.paths[1])],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 1)
        self.paths[0].write_text('{')
        result = subprocess.run([sys.executable, '-I', str(TOOL), 'compare',
                                 '--reference', str(self.paths[0]), '--repeat', str(self.paths[1])],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 2)

    def test_types_and_invalid_json_values(self):
        self.data[2]['observations']['activation'][0]['value']['synthetic'] = True
        self.assertEqual(self.compare()['classes']['activation'], 'confirmed sample divergence')
        for text in ('{"schema":1,"schema":1}', '{"value":NaN}', '{"value":1e999}'):
            self.paths[0].write_text(text)
            with self.assertRaises(ValueError):
                measure.load(self.paths[0])

    def test_engine_provenance_and_partial_class_scope(self):
        self.data[2]['input_sha256'] = 'c' * 64
        with self.assertRaisesRegex(ValueError, 'engine case/site/input/alignment mismatch'):
            self.compare()
        self.data[2]['input_sha256'] = 'b' * 64
        self.data[2]['observations'] = {'boarding': self.data[2]['observations']['boarding']}
        self.save()
        self.assertEqual(measure.compare(*self.paths, ['boarding'])['status'], 'sample match')
        self.assertEqual(self.compare()['status'], 'needs live run')


if __name__ == '__main__':
    unittest.main()
