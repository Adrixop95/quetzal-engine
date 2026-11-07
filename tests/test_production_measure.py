#!/usr/bin/env python3
"""Invented production births, cash clocks and failed/repeated capture controls."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("production_measure", ROOT / "tools/production_measure.py")
measure = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(measure)


class ProductionControls(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def fixture(self, directory, every_tick=False):
        directory.mkdir()
        s = measure.scenario(2, every_tick)
        (directory / "scenario.json").write_text(json.dumps(s))
        outcomes = {}
        for name in ("reference", "clone"):
            for repeat in (1, 2):
                key = f"{name}-{repeat}"
                run = directory / key
                run.mkdir()
                rows = [dict(schema=2, phase="before_commands", position_unit="pixel/256")]
                for tick in s["observe"]["ticks"]:
                    # Invented units arrive repeatedly; native handles differ
                    # between processes and do not imply cross-program identity.
                    objects = [dict(handle=i + repeat * 10000, type=4, owner=1,
                                    hp=500, x=i, y=0, orders=None)
                               for i in range(1 + tick // 200)]
                    objects.append(dict(handle=repeat * 10000 + 999, type=-1, owner=0,
                                        hp=0, x=0, y=0, orders=None))
                    rows.append(dict(tick=tick, objects=objects, money={
                        "0": 400 + tick // 240 * 17, "1": 90, "2": 0, "3": 0},
                        queues=None, view=None, frame=None,
                        unavailable=["orders", "queues", "view", "frame"]))
                self.write_rows(run / "observations.jsonl", rows)
                outcomes[key] = dict(status="completed")
        (directory / "processes.json").write_text(json.dumps(outcomes))
        return s

    def write_rows(self, path, rows):
        path.write_text("".join(json.dumps(row) + "\n" for row in rows))

    def test_schedule(self):
        self.assertEqual(measure.DENSE, [1, *range(100, 1301, 100), 1350, 1400, 1450, 1500, 1550, 1600])
        for mission in (2, 3):
            self.assertEqual(measure.scenario(mission)["map"]["mission"], mission)
            self.assertEqual(measure.scenario(mission, True)["observe"]["ticks"], list(range(1, 1601)))

    def test_births_and_virtual_objects_without_identity_matching(self):
        directory = self.root / "capture"
        self.fixture(directory)
        result = measure.compare(directory)
        self.assertTrue(result["repeatable_aggregates"])
        self.assertTrue(result["money_equal"])
        self.assertTrue(all(w["within_one"] for w in result["growth_windows"]))
        final = result["rows"][-1]["reference"]["sides"]
        self.assertEqual(final["1"]["objects"], 9)
        self.assertEqual(final["0"]["by_type"], {"-1": 1})
        first = result["cash_transitions"]["reference"]["0"][0]
        self.assertEqual(first, dict(after_tick=200, through_tick=300, delta=17))

    def test_exact_steps_and_cash_are_not_harvest_events(self):
        directory = self.root / "capture"
        self.fixture(directory, True)
        result = measure.compare(directory)
        changes = result["cash_transitions"]["reference"]["0"]
        self.assertEqual([c["through_tick"] for c in changes], list(range(240, 1601, 240)))
        self.assertTrue(all(c["through_tick"] == c["after_tick"] + 1 for c in changes))

    def test_failed_process_and_truncation_are_refused(self):
        directory = self.root / "capture"
        s = self.fixture(directory)
        path = directory / "clone-1/observations.jsonl"
        rows = [json.loads(line) for line in path.read_text().splitlines()]
        self.write_rows(path, rows[:-1])
        with self.assertRaisesRegex(ValueError, "schedule"):
            measure.compare(directory)
        self.write_rows(path, rows)
        duplicate = copy.deepcopy(rows)
        duplicate[-1]["objects"].append(duplicate[-1]["objects"][0])
        self.write_rows(path, duplicate)
        with self.assertRaisesRegex(ValueError, "duplicate"):
            measure.summarize(path, s)
        (directory / "processes.json").write_text("{}")
        with self.assertRaisesRegex(ValueError, "process"):
            measure.compare(directory)

    def test_repeat_mismatch_suppresses_counts(self):
        directory = self.root / "capture"
        self.fixture(directory)
        path = directory / "reference-2/observations.jsonl"
        rows = [json.loads(line) for line in path.read_text().splitlines()]
        rows[-1]["money"]["0"] += 1
        self.write_rows(path, rows)
        result = measure.compare(directory)
        self.assertIn("unstable", result["status"])
        self.assertEqual(result["rows"], [])

    def test_growth_tolerance_does_not_hide_money_or_count_gap(self):
        directory = self.root / "capture"
        self.fixture(directory)
        for repeat in (1, 2):
            path = directory / f"clone-{repeat}/observations.jsonl"
            rows = [json.loads(line) for line in path.read_text().splitlines()]
            rows[-1]["objects"] = [o for o in rows[-1]["objects"] if o["type"] == -1]
            rows[-1]["money"]["0"] += 1
            self.write_rows(path, rows)
        result = measure.compare(directory)
        self.assertFalse(result["money_equal"])
        self.assertEqual(result["first_difference"]["0"]["money"], 1600)
        self.assertEqual(result["first_difference"]["1"]["objects"], 1600)
        self.assertTrue(any(not w["within_one"] for w in result["growth_windows"]))
        self.assertEqual(result["rows"][-1]["clone"]["sides"]["1"]["objects"], 0)

    def test_collection_keeps_failures_and_continues(self):
        from types import SimpleNamespace
        output = self.root / "capture"
        args = SimpleNamespace(output=output, game=Path("/private/game"), runtime=Path("/private/runtime"),
                               engine=Path("/private/engine"), every_tick=False)
        with patch.object(measure.runner.report, "private_output", return_value=output), patch.object(
                measure.runner, "shared_runner", side_effect=RuntimeError("namespace denied")) as launch:
            measure.collect(args)
        self.assertEqual(launch.call_count, 8)
        for mission in (2, 3):
            outcomes = json.loads((output / f"mission_{mission}/processes.json").read_text())
            self.assertEqual(len(outcomes), 4)
            self.assertTrue(all(o["status"] == "failed" for o in outcomes.values()))
            with self.assertRaisesRegex(ValueError, "process"):
                measure.compare(output / f"mission_{mission}")


if __name__ == "__main__":
    unittest.main()
