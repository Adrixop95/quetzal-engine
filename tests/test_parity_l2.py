#!/usr/bin/env python3
"""Invented L2 streams exercise the real shared launcher without a game runtime."""
from argparse import Namespace
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("parity_l2", ROOT / "tools/parity_l2.py")
l2 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(l2)


class ReplayControls(unittest.TestCase):
    def setUp(self):
        private = ROOT / "work/parity"
        private.mkdir(parents=True, exist_ok=True)
        tmp = tempfile.TemporaryDirectory(dir=private)
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.harness = l2.shared()
        self.game = self.root / "source"
        self.game.mkdir()
        (self.game / "Res").mkdir()
        (self.game / "Maps").mkdir()
        (self.game / "Aztec.exe").write_text("invented marker, never executed")
        self.args = Namespace(game=self.game, runtime=self.game / "Aztec.exe", engine=self.game / "Aztec.exe",
                              output=self.root / "output", ticks=3, timeout=10, engine_adapter=None)
        self.packet = "2 0 " + " ".join(["0"] * 53) + "\n"
        self.calls = []
        self.mutate = lambda row, directory: None

    def invented_execute(self, argv, directory, timeout, extra=None, **kwargs):
        self.calls.append((argv, directory, extra))
        self.assertTrue((directory / "game/Res").is_symlink())
        self.assertFalse((directory / "game/Maps").is_symlink())
        if extra and "RT_L0_OUTPUT" in extra:
            self.assertEqual(extra["RT_L0_STATE_ONLY"], "1")
            recording = "RT_CMD_REPLAY" not in extra
            self.assertEqual(recording, directory.parent.name == "record")
            if not recording:
                self.assertEqual(Path(extra["RT_CMD_REPLAY"]).read_text(), self.packet)
            Path(extra["RT_CMD_LOG"]).write_text(self.packet)
            Path(extra["RT_STATE_LOG"]).write_text("".join(
                f"{tick}" + " 1:0000000000000000" * 4 + "\n" for tick in range(1, 4)))
            rows = [dict(schema=2, phase="before_commands", position_unit="pixel/256")]
            for tick in range(1, 4):
                row = dict(tick=tick, objects=[dict(handle=7, owner=0, type=3, x=tick, y=0, hp=100, orders=None)],
                           money={str(i): 100 for i in range(4)}, queues=None, view=None, frame=None,
                           unavailable=["orders", "queues", "view", "frame", "frame_phase"])
                self.mutate(row, directory)
                rows.append(row)
            Path(extra["RT_L0_OUTPUT"]).write_text("".join(json.dumps(row) + "\n" for row in rows))
        else:
            self.assertTrue("--commands" in argv)
            self.assertFalse(kwargs["setup_network"])
            reference = json.loads((directory.parent / "replay-1/reference.json").read_text())
            reference["runner"] = "clone"
            reference["samples"][1]["objects"]["initial:0:3:1:0"]["hp"] = 99
            Path(argv[-1]).write_text(json.dumps(reference))

    def run_mission(self):
        with patch.object(self.harness.report, "execute", side_effect=self.invented_execute):
            return l2.mission(self.args, 2, self.root / "mission", self.harness)

    def test_record_and_two_exact_replays_gate_translation(self):
        verdict = self.run_mission()
        self.assertEqual(verdict["category"], "blocked_command_translation")
        self.assertTrue(verdict["raw_reference_repeatable"])
        self.assertEqual(len(self.calls), 3)
        self.assertTrue("click 150 277" in (self.root / "mission/record-input.txt").read_text())
        hashes = json.loads((self.root / "mission/reference-hashes.json").read_text())
        self.assertEqual(len(hashes), 3)
        self.assertTrue(all(r["reference"] == r["engine"] for r in hashes))
        self.assertFalse(verdict["reference_observation"]["complete_observed_contract"])

    def test_first_reference_difference_stops_clone(self):
        def mutate(row, directory):
            if directory.parent.name == "replay-2" and row["tick"] == 2:
                row["objects"][0]["hp"] = 90
        self.mutate = mutate
        self.args.engine_adapter = Path("/invented/adapter")
        verdict = self.run_mission()
        self.assertEqual(verdict["category"], "reference_nondeterminism")
        self.assertEqual(verdict["detail"]["first_known_difference"]["tick"], 2)
        self.assertEqual(len(self.calls), 3)

    def test_adapter_difference_and_equal_prefix(self):
        self.args.engine_adapter = Path("/invented/adapter")
        verdict = self.run_mission()
        first = verdict["first_known_difference"]
        self.assertEqual((first["tick"], first["object_id"], first["field"]), (2, "initial:0:3:1:0", "hp"))
        self.assertEqual(verdict["observed_equal_through_tick"], 1)
        self.assertEqual(verdict["category"], "blocked_observation_contract")
        self.assertEqual(len(self.calls), 4)

    def test_failed_readback_is_not_repeatability(self):
        original = self.invented_execute

        def bad(*args, **kwargs):
            original(*args, **kwargs)
            if args[1].parent.name == "replay-1":
                (args[1] / "readback.txt").write_text("")

        with patch.object(self.harness.report, "execute", side_effect=bad), self.assertRaises(ValueError):
            l2.mission(self.args, 1, self.root / "mission", self.harness)

    def test_raw_pool_difference_gates_clone_even_with_equal_fields(self):
        original = self.invented_execute

        def bad(*args, **kwargs):
            original(*args, **kwargs)
            if args[1].parent.name == "replay-2":
                path = args[1] / "state.txt"
                path.write_text(path.read_text().replace("0000000000000000", "0000000000000001", 1))

        self.args.engine_adapter = Path("/invented/adapter")
        with patch.object(self.harness.report, "execute", side_effect=bad):
            verdict = l2.mission(self.args, 1, self.root / "mission", self.harness)
        self.assertEqual((verdict["category"], verdict["tick"], verdict["field"]),
                         ("reference_nondeterminism", 1, "pool[0]"))
        self.assertEqual(len(self.calls), 3)

    def test_empty_record_is_not_command_coverage(self):
        self.packet = ""
        verdict = self.run_mission()
        self.assertEqual(verdict["category"], "blocked_input_coverage")

    def test_late_malformed_observation_cannot_hide_early_difference(self):
        def mutate(row, directory):
            if directory.parent.name == "replay-2":
                if row["tick"] == 2:
                    row["objects"][0]["hp"] = 90
                if row["tick"] == 3:
                    row["money"]["0"] = "invalid"
        self.mutate = mutate
        with self.assertRaisesRegex(ValueError, "invalid money"):
            self.run_mission()

    def test_unmapped_spawn_is_explicitly_blocked_after_two_replays(self):
        def spawn(row, directory):
            if row["tick"] == 3:
                row["objects"].append(dict(handle=8, owner=0, type=3, x=4, y=4, hp=100, orders=None))
        self.mutate = spawn
        verdict = self.run_mission()
        self.assertEqual(verdict["category"], "blocked_observation_contract")
        self.assertTrue("spawn identity" in verdict["detail"])
        self.assertEqual(len(self.calls), 3)
        self.assertTrue(verdict["raw_reference_repeatable"])

    def test_complete_projection_hash_and_contiguous_horizon(self):
        self.run_mission()
        path = self.root / "mission/replay-1/reference.json"
        a = json.loads(path.read_text())
        for row in a["samples"]:
            row["queues"] = {str(i): [] for i in range(4)}
            row["unavailable"] = ["view", "frame", "frame_phase"]
            for obj in row["objects"].values():
                obj["orders"] = dict(kind="idle", target=None, x=0, y=0)
        self.harness.l0.validate_bundle(a, path.parent)
        b = copy.deepcopy(a)
        b["samples"][0]["objects"] = dict(reversed(list(b["samples"][0]["objects"].items())))
        verdict, hashes = l2.compare(a, b, self.harness.l0)
        self.assertTrue(verdict["complete_observed_contract"])
        self.assertEqual(verdict["through_tick"], 3)
        b["samples"][2]["money"]["0"] -= 1
        verdict, hashes = l2.compare(a, b, self.harness.l0)
        self.assertEqual((verdict["tick"], verdict["field"]), (3, "money.0"))
        self.assertEqual(verdict["observed_equal_through_tick"], 2)
        self.assertEqual(hashes[0]["reference"], hashes[0]["engine"])
        self.assertNotEqual(hashes[2]["reference"], hashes[2]["engine"])
        for bundle in (a, b):
            bundle["ticks"] = [1, 3]
            bundle["samples"].pop(1)
        with self.assertRaises(ValueError):
            l2.compare(a, b, self.harness.l0)

    def test_all_missions_retain_launch_failures(self):
        with patch.object(l2, "shared", return_value=self.harness), \
                patch.object(self.harness.report, "execute", side_effect=RuntimeError("namespace denied")):
            self.assertEqual(l2.run(self.args), 1)
        rows = json.loads((self.args.output / "report.json").read_text())
        self.assertEqual([r["mission"] for r in rows], [1, 2, 3])
        self.assertTrue(all(r["layer"] == "L2" and r["category"] == "experiment_failed" for r in rows))


if __name__ == "__main__":
    unittest.main()
