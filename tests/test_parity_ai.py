#!/usr/bin/env python3
"""Synthetic L8 windows; all numbers here are invented test inputs."""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('parity_ai', ROOT / 'tools/parity_ai.py')
ai = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ai)


def sample(offset=0):
    return [1, 73, 1000, 31, [
        [10 + kind * 100 + offset, 1, kind, 0, [[2, 1]] if kind < 2 else [[2, 3], [4, 1]]]
        for kind in range(5)]]


class Windows(unittest.TestCase):
    def test_boundaries_and_numeric_export(self):
        for clone in (sample(), sample(5), sample(10)):
            result, rows = ai.compare([sample(), sample(10)], clone)
            self.assertTrue(result['pass'])
            self.assertEqual(rows[0], [1, 0, 0, 10, 20, -1, 1, 1])
            self.assertTrue(all(type(n) is int for row in rows for n in row))

    def test_first_divergence(self):
        clone = sample(5)
        clone[4][2][0] += 10
        clone[4][3][4][0][1] += 1
        result, _ = ai.compare([sample(), sample(10)], clone)
        self.assertEqual((result['category'], result['tick'], result['event']),
                         ('timing_window', 225, [1, 2, 0]))

    def test_composition_and_force_size(self):
        repeat = sample(10)
        repeat[4][2][4] = [[2, 1], [4, 3]]
        clone = sample(5)
        clone[4][2][4] = [[2, 3], [4, 3]]
        result, _ = ai.compare([sample(), repeat], clone)
        self.assertEqual((result['category'], result['type']), ('composition_window', -1))
        clone[4][2][4] = [[2, 3], [5, 1]]
        result, _ = ai.compare([sample(), sample(10)], clone)
        self.assertFalse(result['pass'])

    def test_missing_and_extra_events(self):
        for change, category in ((-1, 'missing_event'), (1, 'unexpected_event')):
            clone = sample(5)
            if change < 0:
                # Preserve required class coverage with a second reference wave.
                clone = sample()
                reference = sample()
                reference[4].append([600, 1, 2, 1, [[2, 1]]])
                repeat = copy.deepcopy(reference)
            else:
                reference, repeat = sample(), sample(10)
                clone[4].append([600, 1, 2, 1, [[2, 1]]])
            result, windows = ai.compare([reference, repeat], clone)
            self.assertEqual(result['category'], category)
            _, expected = ai.compare([reference, repeat], reference)
            self.assertEqual(windows, expected)

    def test_unstable_references_produce_no_windows(self):
        repeat = sample(10)
        repeat[4].append([600, 1, 2, 1, [[2, 1]]])
        result, rows = ai.compare([sample(), repeat], sample(5))
        self.assertEqual(result['category'], 'reference_event_coverage_differs')
        self.assertEqual(rows, [])

    def test_malformed_captures(self):
        invalid = []
        for position, value in ((0, True), (1, -1), (2, 0), (3, 0), (3, 32)):
            data = sample(); data[position] = value; invalid.append(data)
        for position, value in ((0, 1001), (1, 8), (2, 5), (3, 1), (4, [[2, 0]]),
                                (4, [[2, True]]), (4, [[2, 1], [2, 3]])):
            data = sample(); data[4][-1][position] = value; invalid.append(data)
        data = sample(); data[4].pop(); invalid.append(data)
        data = sample(); data[4].append(data[4][-1]); invalid.append(data)
        data = sample(); data[4].reverse(); invalid.append(data)
        data = sample(); data[4][0][4] = [[2, 2]]; invalid.append(data)
        for data in invalid:
            with self.subTest(data=data), self.assertRaises(ValueError):
                ai.compare([sample(), sample(10)], data)
        with self.assertRaises(ValueError):
            ai.compare([sample()], sample())
        data = sample(); data[2] += 1
        with self.assertRaises(ValueError):
            ai.compare([sample(), data], sample())

    def test_cli_skip_and_private_output(self):
        private = ROOT / 'work/parity'
        private.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=private) as directory:
            folder = Path(directory)
            paths = [folder / name for name in ('reference.json', 'repeat.json', 'clone.json')]
            command = [sys.executable, '-I', str(ROOT / 'tools/parity_ai.py'),
                       '--reference', str(paths[0]), str(paths[1]), '--clone', str(paths[2]),
                       '--output', str(folder / 'result')]
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 77)
            for path, value in zip(paths, (sample(), sample(10), sample(5))):
                path.write_text(json.dumps(value))
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertTrue((folder / 'result/windows.json').is_file())
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 2)
            command[-1] = str(ROOT / 'build/forbidden-ai-output')
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 2)
            command[-1] = str(folder / 'duplicate')
            command[5] = command[4]
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 2)


if __name__ == '__main__':
    unittest.main()
