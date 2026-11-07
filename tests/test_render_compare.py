#!/usr/bin/env python3
"""Invented shared-runner streams and rasters; never launches the reference."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import render_compare as render


class RenderTests(unittest.TestCase):
    def setUp(self):
        private = ROOT / "work/parity"
        private.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=private)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.plan = self.root / "plan.json"
        self.view = dict(camera=[0, 0, 1], resolution=[800, 600], side=0,
                         selection=[], panel=True, cursor=[400, 300])
        self.frame = b"P6\n800 600\n255\n" + bytes([20, 40, 60]) * (800 * 600)
        self.masks = {}
        for i, region in enumerate(render.visual.REGIONS):
            name = region + ".pgm"
            data = bytearray(800 * 600)
            data[i] = 255
            (self.root / name).write_bytes(b"P5\n800 600\n255\n" + data)
            self.masks[region] = name
        self.spec = dict(schema=1, scene="aircraft-altitude", ticks=[3],
                         regions={r: dict(channel=3, max_bad_fraction=0, max_mae=2)
                                  for r in render.visual.REGIONS},
                         reference=[self.make_run("ref1", "reference"), self.make_run("ref2", "reference")],
                         before=[self.make_run("before1", "clone"), self.make_run("before2", "clone")], after=None)

    def make_run(self, name, runner):
        directory = self.root / name
        directory.mkdir()
        (directory / "frame.ppm").write_bytes(self.frame)
        obj = dict(type=7, owner=0, x=256, y=256, hp=100,
                   orders=dict(kind="idle", target=None, x=1, y=1))
        bundle = dict(schema=2, contract="shared-l0-v2", mission=1, phase="before_commands",
                      position_unit="pixel/256", view=self.view, ticks=[1, 3], runner=runner,
                      samples=[dict(tick=t, objects={"initial:0:7:256:256": obj},
                                    money={str(i): 0 for i in range(4)},
                                    queues={str(i): [] for i in range(4)}, view=self.view,
                                    frame="frame.ppm", unavailable=[]) for t in (1, 3)])
        (directory / "bundle.json").write_text(json.dumps(bundle))
        return dict(bundle=name + "/bundle.json", masks={str(t): dict(self.masks) for t in (1, 3)})

    def compare(self):
        self.plan.write_text(json.dumps(self.spec))
        return render.compare(self.plan)

    def change(self, label, index, operation):
        entry = self.spec[label][index]
        path = self.root / entry["bundle"]
        bundle = json.loads(path.read_text())
        operation(bundle)
        path.write_text(json.dumps(bundle))

    def pixels(self, label, delta):
        for entry in self.spec[label]:
            path = self.root / entry["bundle"]
            frame = bytearray(self.frame)
            frame[len(b"P6\n800 600\n255\n")] += delta
            (path.parent / "frame.ppm").write_bytes(frame)

    def test_before_after_and_separate_exact_tolerant_metrics(self):
        self.pixels("before", 90)
        self.spec["after"] = [self.make_run("after1", "clone"), self.make_run("after2", "clone")]
        # Tolerated after pixels still expose an exact mismatch.
        self.pixels("after", 3)
        result = self.compare()
        self.assertTrue(result["eligible_for_engine_fix"])
        self.assertEqual(result["before"]["category"], "different")
        self.assertEqual(result["after"]["category"], "equal_observed_regions")
        before = result["before"]["samples"][0]["regions"]["map"]
        after = result["after"]["samples"][0]["regions"]["map"]
        self.assertEqual(before["mae"], 30)
        self.assertEqual(after["mae"], 1)
        self.assertEqual(after["exact_mismatch_fraction"], 1)
        self.assertEqual(after["bad_fraction"], 0)
        self.assertEqual(result["before"]["samples"][0]["tick"], 3)

    def test_missing_after_is_not_zero_error(self):
        result = self.compare()
        self.assertIsNone(result["after"])
        self.assertFalse(result["eligible_for_engine_fix"])

    def test_repeat_frames_and_state_must_match_full_schedule(self):
        self.change("reference", 1, lambda b: b["samples"][0]["money"].update({"0": 1}))
        result = self.compare()
        self.assertEqual(result["category"], "run_nondeterminism")
        self.assertEqual(result["first"], dict(tick=1, field="money"))
        self.assertIsNone(result["before"])
        self.change("reference", 1, lambda b: b["samples"][0]["money"].update({"0": 0}))
        path = self.root / "ref2/frame.ppm"
        path.write_bytes(self.frame[:-1] + b"\xff")
        self.assertEqual(self.compare()["first"]["field"], "frame")

    def test_unavailable_phase_blocks_even_equal_repeated_pixels(self):
        for index in range(2):
            self.change("reference", index, lambda b: b["samples"][1]["unavailable"].append("frame_phase"))
        result = self.compare()
        self.assertEqual(result["category"], "blocked_observation_contract")
        self.assertTrue(result["before"]["diagnostic_only"])
        self.assertFalse(result["eligible_for_engine_fix"])

    def test_different_state_is_not_a_render_fix(self):
        for index in range(2):
            self.change("before", index, lambda b: b["samples"][1]["money"].update({"0": 1}))
        self.pixels("before", 90)
        result = self.compare()
        self.assertFalse(result["eligible_for_engine_fix"])
        self.assertEqual(result["before"]["blockers"][0]["state_differences"], ["money"])

    def test_absent_frame_is_unavailable_not_zero_error(self):
        def missing(bundle):
            bundle["samples"][1]["frame"] = None
            bundle["samples"][1]["unavailable"] = ["frame"]
        for index in range(2):
            self.change("reference", index, missing)
        result = self.compare()
        self.assertEqual(result["category"], "blocked_observation_contract")
        self.assertIsNone(result["before"]["samples"][0]["regions"])
        self.assertFalse(result["eligible_for_engine_fix"])

    def test_clone_and_mask_repeatability(self):
        self.change("before", 1, lambda b: b["samples"][1]["money"].update({"0": 1}))
        result = self.compare()
        self.assertEqual(result["runner"], "before")
        self.assertEqual(result["first"], dict(tick=3, field="money"))
        self.change("before", 1, lambda b: b["samples"][1]["money"].update({"0": 0}))
        self.spec["before"][1]["masks"] = copy.deepcopy(self.spec["before"][1]["masks"])
        self.spec["before"][1]["masks"]["3"]["map"] = "units.pgm"
        self.assertEqual(self.compare()["first"], dict(tick=3, field="masks"))

    def test_structural_shift_cannot_hide_pixels(self):
        mask = bytearray(800 * 600)
        mask[1] = 255
        (self.root / "shift.pgm").write_bytes(b"P5\n800 600\n255\n" + mask)
        for entry in self.spec["before"]:
            entry["masks"] = copy.deepcopy(entry["masks"])
            entry["masks"]["3"]["map"] = "shift.pgm"
        region = self.compare()["before"]["samples"][0]["regions"]["map"]
        self.assertEqual(region["mask_difference"], 2)
        self.assertFalse(region["passed"])

    def test_complete_validation_before_repeat_failure(self):
        self.change("reference", 1, lambda b: b["samples"][0]["money"].update({"0": 1}))
        (self.root / "before2/frame.ppm").write_bytes(b"truncated")
        with self.assertRaises(ValueError):
            self.compare()

    def test_invalid_plans_masks_and_alignment(self):
        original = copy.deepcopy(self.spec)
        for case in ("same_run", "tick", "path", "empty_mask", "missing_mask", "threshold", "view"):
            self.spec = copy.deepcopy(original)
            with self.subTest(case=case):
                if case == "same_run": self.spec["reference"][1] = self.spec["reference"][0]
                if case == "tick": self.spec["ticks"] = [2]
                if case == "path": self.spec["before"][0]["bundle"] = "../outside.json"
                if case == "empty_mask":
                    (self.root / "empty.pgm").write_bytes(b"P5\n800 600\n255\n" + bytes(800 * 600))
                    self.spec["before"][0]["masks"]["3"]["map"] = "empty.pgm"
                if case == "missing_mask": del self.spec["before"][0]["masks"]["1"]
                if case == "threshold": self.spec["regions"]["map"]["max_mae"] = -1
                if case == "view": self.change("before", 0, lambda b: b.update(mission=2))
                with self.assertRaises(ValueError): self.compare()

    def test_cli_invalid_input_retains_failure(self):
        self.spec["before"] = []
        self.plan.write_text(json.dumps(self.spec))
        output = self.root / "report.json"
        result = subprocess.run([sys.executable, "-I", str(ROOT / "tools/render_compare.py"),
                                 str(self.plan), "--output", str(output)], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(json.loads(output.read_text())["category"], "invalid_observation")


if __name__ == "__main__":
    unittest.main()
