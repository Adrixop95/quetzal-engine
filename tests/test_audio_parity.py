#!/usr/bin/env python3
"""Invented playback events test L6 ordering, clock tolerance and rejected evidence."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import audio_parity as audio


def capture(events=None):
    if events is None:
        events = [dict(seq=i, tick=i + 2, sound=i + 100, kind='music' if c == 'music' else 'sound',
                       trigger=c) for i, c in enumerate(audio.CLASSES)]
    return dict(header=dict(schema=1, scenario='invented-audio', contract='synthetic-clock-v1',
                            first_tick=0, last_tick=30, complete=True),
                events=events, event_count=len(events))


class AudioParityTests(unittest.TestCase):
    def test_clone_speech_and_subtitle_events_preserve_order(self):
        events = [dict(seq=i, tick=2, sound=100, kind=kind, trigger='orders')
                  for i, kind in enumerate(('subtitle', 'speech'))]
        with tempfile.TemporaryDirectory() as folder:
            raw = Path(folder) / 'raw.jsonl'
            raw.write_text(''.join(json.dumps(event) + '\n' for event in events))
            manifest = dict(header=capture()['header'], tick_offset=0,
                            raw_sha256=hashlib.sha256(raw.read_bytes()).hexdigest())
            normalized = audio.normalize(raw, manifest, 'clone')
            self.assertEqual(normalized['events'], events)
            self.assertEqual(audio.compare(capture(events), normalized)['category'], 'equal_observed_events')

    def test_all_trigger_classes_and_one_tick_tolerance(self):
        a = capture()
        b = copy.deepcopy(a)
        for event in b['events']:
            event['tick'] += 1
        result = audio.compare(a, b)
        self.assertEqual(result['category'], 'equal_observed_events')
        self.assertTrue(all(row['status'] == 'compared' for row in result['classes'].values()))
        b['events'][3]['tick'] += 1
        # Keep event clocks monotonic after changing one event.
        b['events'][4]['tick'] += 1
        result = audio.compare(a, b)
        self.assertEqual(result['first']['category'], 'timing')
        self.assertEqual(result['first']['triggers'], ['building'])

    def test_order_across_classes_at_same_tick(self):
        a = capture()
        for event in a['events']:
            event['tick'] = 10
        b = copy.deepcopy(a)
        b['events'][0], b['events'][1] = b['events'][1], b['events'][0]
        for i, event in enumerate(b['events']):
            event['seq'] = i
        result = audio.compare(a, b)
        self.assertEqual(result['first']['category'], 'identity_or_order')
        self.assertEqual(result['classes']['selection']['differences'], 2)
        self.assertEqual(result['classes']['orders']['differences'], 2)

    def test_missing_extra_duplicate_and_wrong_sound_or_kind(self):
        a = capture()
        short = capture(copy.deepcopy(a['events'][:-1]))
        self.assertEqual(audio.compare(a, short)['first']['category'], 'missing_event')
        self.assertEqual(audio.compare(short, a)['first']['category'], 'extra_event')
        b = copy.deepcopy(a)
        b['events'].append(dict(b['events'][-1], seq=7))
        b['event_count'] += 1
        self.assertEqual(audio.compare(a, b)['first']['category'], 'extra_event')
        for field, value in [('sound', 999), ('kind', 'loop_start')]:
            b = copy.deepcopy(a)
            b['events'][0][field] = value
            self.assertEqual(audio.compare(a, b)['first']['category'], 'identity_or_order')

    def test_empty_is_unobserved_and_not_a_pass(self):
        result = audio.compare(capture([]), capture([]))
        self.assertEqual(result['category'], 'no_events')
        self.assertTrue(all(row['status'] == 'unobserved' for row in result['classes'].values()))

    def test_reject_incomplete_malformed_and_unresolved(self):
        a = capture()
        mutations = [lambda b: b.update(event_count=99),
                     lambda b: b.update(header=None),
                     lambda b: b['header'].update(complete=False),
                     lambda b: b['header'].update(schema=True),
                     lambda b: b['events'][-1].update(trigger='unknown'),
                     lambda b: b['events'][-1].update(sound=None),
                     lambda b: b['events'][-1].update(seq=99),
                     lambda b: b['events'][-1].update(tick=-1),
                     lambda b: b['events'][-1].update(tick=31),
                     lambda b: b['events'][-1].update(tick=0),
                     lambda b: b['events'][-1].update(tick=True)]
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                b = copy.deepcopy(a)
                mutate(b)
                with self.assertRaises(ValueError):
                    audio.compare(a, b)
        b = copy.deepcopy(a)
        b['events'][0]['sound'] += 1  # Earlier difference must not hide malformed tail.
        b['events'][-1]['trigger'] = 'unknown'
        with self.assertRaises(ValueError):
            audio.compare(a, b)
        for field, value in [('contract', 'another-clock'), ('scenario', 'another-scene'), ('last_tick', 29)]:
            b = copy.deepcopy(a)
            b['header'][field] = value
            with self.assertRaises(ValueError):
                audio.compare(a, b)

    def test_reference_annotations_bind_exact_raw_capture(self):
        with tempfile.TemporaryDirectory() as folder:
            raw = Path(folder) / 'raw.jsonl'
            raw.write_text(json.dumps(dict(seq=0, tick=4, kind='play', buffer=42, flags=0,
                                           sound=None, trigger='unknown')) + '\n')
            manifest = dict(header=capture()['header'], tick_offset=1,
                            raw_sha256=hashlib.sha256(raw.read_bytes()).hexdigest(), annotations={})
            with self.assertRaises(ValueError):
                audio.normalize(raw, manifest, 'reference')
            manifest['annotations']['0'] = dict(sound=123, kind='sound', trigger='selection')
            result = audio.normalize(raw, manifest, 'reference')
            self.assertEqual(result['events'][0]['sound'], 123)  # Never the buffer handle.
            self.assertEqual(result['events'][0]['tick'], 5)
            manifest['annotations']['1'] = manifest['annotations']['0']
            with self.assertRaises(ValueError):
                audio.normalize(raw, manifest, 'reference')
            del manifest['annotations']['1']
            raw.write_text(raw.read_text() + '\n')
            with self.assertRaises(ValueError):
                audio.normalize(raw, manifest, 'reference')

    def test_clone_annotations_cannot_rewrite_known_identity(self):
        with tempfile.TemporaryDirectory() as folder:
            raw = Path(folder) / 'raw.jsonl'
            raw.write_text(json.dumps(dict(seq=0, tick=2, kind='sound', sound=100, trigger='unknown')) + '\n')
            manifest = dict(header=capture()['header'], tick_offset=0,
                            raw_sha256=hashlib.sha256(raw.read_bytes()).hexdigest(),
                            annotations={'0': dict(trigger='combat')})
            self.assertEqual(audio.normalize(raw, manifest, 'clone')['events'][0]['trigger'], 'combat')
            manifest['annotations']['0']['sound'] = 42
            with self.assertRaises(ValueError):
                audio.normalize(raw, manifest, 'clone')

    def test_private_probe_skips_missing_inputs_and_propagates_isolation_failure(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            args = SimpleNamespace(output=root / 'missing', game=root / 'game',
                                   runtime=root / 'runtime', script=None, timeout=1)
            with patch.object(audio, 'private_output', side_effect=lambda p: p):
                self.assertEqual(audio.probe(args), 77)
                args.game.mkdir()
                (args.game / 'Aztec.exe').touch()  # Invented empty input, never executed.
                args.runtime.touch()
                args.output = root / 'failed'
                with patch.object(audio, 'execute', side_effect=RuntimeError('isolation denied')):
                    self.assertEqual(audio.probe(args), 1)
                report = json.loads((args.output / 'report.json').read_text())
                self.assertEqual(report['category'], 'experiment_failed')
                self.assertEqual(set(report['classes'].values()), {'unmeasured'})
                self.assertFalse((args.output / 'game/Maps').is_symlink())

    def test_cli_rejects_public_output(self):
        result = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/audio_parity.py'),
                                 'compare', 'missing', 'missing', '--output', str(ROOT / 'docs/forbidden.json')],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 2)
        self.assertTrue('work/parity' in result.stderr)
        self.assertFalse((ROOT / 'docs/forbidden.json').exists())

    def test_cli_verdicts_and_exclusive_output(self):
        private = ROOT / 'work/parity'
        private.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=private, prefix='synthetic-audio-') as folder:
            root = Path(folder)
            a, b, output = root / 'a.json', root / 'b.json', root / 'diff.json'
            a.write_text(json.dumps(capture()))
            b.write_text(a.read_text())
            command = [sys.executable, '-I', str(ROOT / 'tools/audio_parity.py'),
                       'compare', str(a), str(b), '--output', str(output)]
            def run():
                return subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(run().returncode, 0)
            original = output.read_bytes()
            self.assertEqual(run().returncode, 2)
            self.assertEqual(output.read_bytes(), original)
            output.unlink()
            changed = capture()
            changed['events'][0]['sound'] += 1
            b.write_text(json.dumps(changed))
            self.assertEqual(run().returncode, 1)
            self.assertEqual(json.loads(output.read_text())['first']['index'], 0)
            output.unlink()
            b.write_text('{')
            self.assertEqual(run().returncode, 2)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
