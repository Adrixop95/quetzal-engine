#!/usr/bin/env python3
"""Asset-independent triage and public matrix regression controls."""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / f"tools/{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


triage = load("parity_triage")
matrix = load("parity_matrix")


class TriageControls(unittest.TestCase):
    def setUp(self):
        triage.PRIVATE.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=triage.PRIVATE)
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.input = self.root / "input"
        self.input.mkdir()
        # Drafts always live in the private review area, including in synthetic tests.
        drafts = triage.PRIVATE / "tasks"
        drafts.mkdir(exist_ok=True)
        temporary_drafts = tempfile.TemporaryDirectory(dir=drafts)
        self.addCleanup(temporary_drafts.cleanup)
        self.drafts = Path(temporary_drafts.name)

    def write(self, path, value):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def diff(self, name, category, layer=None, **fields):
        path = self.write(self.input / name / "diff.json", dict(category=category, **fields))
        if layer:
            self.write(path.parent / "scenario.json", dict(id=path.parent.name, layer=layer))
        return path

    def cli(self, name, *args, code=0):
        result = subprocess.run([sys.executable, "-I", str(ROOT / f"tools/{name}.py"),
                                 *map(str, args)], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        return result

    def test_grouping_nested_harness_and_private_review(self):
        first = self.diff("one", "field_difference", "L0", tick=2, field="hp", reference=7, engine=8)
        second = self.diff("run/two", "field_difference", tick=3)
        self.write(second.parent.parent / "report.json", [dict(id="two", layer="L1")])
        self.diff("lifecycle", "object_lifecycle", "L2")
        self.diff("blocked", "blocked_observation_contract", "L0", tick=None)
        self.diff("pass", "equal_observed_fields", "L0")
        self.diff("skip", "skipped_missing_private_inputs", "L8")
        self.cli("parity_triage", "--input", self.input, "--output", self.drafts)
        self.assertEqual({p.stem for p in self.drafts.glob("*.md")}, {
            "field_difference", "object_lifecycle", "blocked_observation_contract"})
        draft = self.drafts / "field_difference.md"
        text = draft.read_text()
        self.assertTrue(json.dumps(str(first.relative_to(ROOT))) in text)
        self.assertTrue(json.dumps(str(second.relative_to(ROOT))) in text)
        self.assertTrue("Review required" in text)
        self.assertTrue("not an established gameplay defect" in text)
        draft.write_text("Reviewer edits\n")
        self.cli("parity_triage", "--input", self.input, "--output", self.drafts)
        self.assertEqual(draft.read_text(), "Reviewer edits\n")
        records = triage.collect(self.input)
        self.assertEqual(len(records), 6)
        text = matrix.render(records)
        self.assertTrue("| L0 | 3 | 1 | 2 | 0 |" in text)
        self.assertTrue("| L1 | 1 | 0 | 1 | 0 |" in text)
        self.assertTrue("| L8 | 1 | 0 | 0 | 1 |" in text)
        self.assertTrue("| Total | 6 | 1 | 4 | 1 |" in text)
        self.assertTrue("| field_difference | 2 |" in text)

    def test_public_counts_never_export_private_values(self):
        self.diff("private_scenario_canary", "field_difference", "L1", tick=987654321,
                  object_id="private_object_canary", field="private_field_canary",
                  reference=123456789, engine=234567890, detail="private_detail_canary")
        self.diff("future", "private_category_canary")
        self.diff("visual", "different", "L4", first=dict(category="visual_pixels", tick=1))
        output = self.root / "matrix.md"
        self.cli("parity_matrix", "--input", self.input, "--output", output)
        text = output.read_text()
        for value in ("canary", "987654321", "123456789", "234567890", "work/parity", str(self.root)):
            self.assertNotIn(value, text)
        self.assertTrue("| unclassified | 1 |" in text)
        self.assertTrue("| visual_pixels | 1 |" in text)
        self.assertTrue("| Unknown | 1 | 0 | 1 | 0 |" in text)
        self.cli("parity_triage", "--input", self.input, "--output", self.drafts)
        self.assertTrue((self.drafts / "visual_pixels.md").exists())
        before = text
        self.cli("parity_matrix", "--input", self.input, "--output", output)
        self.assertEqual(output.read_text(), before)

    def test_empty_missing_and_all_pass_skip(self):
        for input_dir in (self.input, self.root / "absent"):
            self.cli("parity_triage", "--input", input_dir, "--output", self.drafts, code=77)
            output = self.root / "empty.md"
            self.cli("parity_matrix", "--input", input_dir, "--output", output, code=77)
            self.assertTrue("| Total | 0 | 0 | 0 | 0 |" in output.read_text())
        self.diff("equal", "equal_observed_fields")
        self.diff("skip", "skipped_missing_private_inputs")
        self.cli("parity_triage", "--input", self.input, "--output", self.drafts)
        self.assertEqual(list(self.drafts.iterdir()), [])

    def test_malformed_inputs_do_not_replace_matrix_or_create_drafts(self):
        self.diff("a-valid", "field_difference", "L0")
        bad = self.input / "z-bad/diff.json"
        bad.parent.mkdir()
        output = self.root / "matrix.md"
        output.write_text("Reviewed matrix\n")
        for text in ('{', '[]', '{}', '{"category":"../escape"}',
                     '{"category":"equal_observed_fields","category":"experiment_failed"}',
                     '{"category":"different","first":null}',
                     '{"category":"different","first":{"category":"equal_observed_fields"}}',
                     '{"category":"equal_observed_fields","first":{"category":"field_difference"}}'):
            bad.write_text(text)
            self.cli("parity_matrix", "--input", self.input, "--output", output, code=2)
            self.cli("parity_triage", "--input", self.input, "--output", self.drafts, code=2)
            self.assertEqual(output.read_text(), "Reviewed matrix\n")
            self.assertEqual(list(self.drafts.iterdir()), [])

    def test_layer_conflicts_are_errors(self):
        path = self.diff("run/one", "field_difference", "L0")
        self.write(path, dict(category="field_difference", layer="L1"))
        with self.assertRaises(ValueError):
            triage.collect(self.input)
        self.write(path, dict(category="field_difference"))
        self.write(path.parent.parent / "report.json", [dict(id="one", layer="L8")])
        with self.assertRaises(ValueError):
            triage.collect(self.input)
        self.write(path.parent / "scenario.json", dict(layer="private_layer_canary"))
        with self.assertRaises(ValueError):
            triage.collect(self.input)

    def test_private_paths_and_symlinks(self):
        path = self.diff("one", "field_difference", "L0")
        self.cli("parity_triage", "--input", ROOT / "tests", "--output", self.drafts, code=2)
        self.cli("parity_triage", "--input", self.input, "--output", ROOT / "tasks/queue", code=2)
        self.cli("parity_matrix", "--input", self.input, "--output", ROOT / "docs/parity.md", code=2)
        self.cli("parity_matrix", "--input", self.input, "--output", path, code=2)
        try:
            (self.input / "linked-directory").symlink_to(path.parent, target_is_directory=True)
        except (OSError, NotImplementedError):
            return  # Other path controls still run on hosts without symlink support.
        self.assertEqual(len(triage.collect(self.input)), 1)
        (self.drafts / "field_difference.md").symlink_to(self.root / "outside.md")
        self.cli("parity_triage", "--input", self.input, "--output", self.drafts, code=2)
        linked = self.input / "linked/diff.json"
        linked.parent.mkdir()
        linked.symlink_to(path)
        self.cli("parity_matrix", "--input", self.input, "--output", self.root / "matrix.md", code=2)


if __name__ == "__main__":
    unittest.main()
