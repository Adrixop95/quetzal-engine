#!/usr/bin/env python3
"""Invented L0 contract, native reference-memory and headless clone controls."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ENGINE, OBSERVER = map(lambda p: Path(p).resolve(), sys.argv[1:3])
del sys.argv[1:3]
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("legacy_controls", ROOT / "tests/test_parity_run.py")
# The existing invented installation fixture needs an engine argument during import.
sys.argv.insert(1, str(ENGINE))
legacy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(legacy)
l0 = legacy.harness.l0


class SharedControls(unittest.TestCase):
    def setUp(self):
        (ROOT / "work/parity").mkdir(parents=True, exist_ok=True)
        tmp = tempfile.TemporaryDirectory(dir=ROOT / "work/parity")
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.s = legacy.harness.load_scenario(ROOT / "tests/parity/private_l0.json")
        self.s["observe"]["ticks"] = [1, 3]

    def complete_bundle(self, name):
        directory = self.root / name
        directory.mkdir()
        samples = []
        for tick in self.s["observe"]["ticks"]:
            frame = directory / f"{tick}.ppm"
            frame.write_bytes(b"P6\n800 600\n255\n" + bytes(800 * 600 * 3))
            samples.append(dict(tick=tick, objects={"initial:0:3:18432:18432": dict(
                type=3, owner=0, x=18432, y=18432, hp=100, orders=dict(kind="idle", target=None, x=0, y=0))},
                money={str(i): 100 for i in range(4)}, queues={str(i): [] for i in range(4)}, view=copy.deepcopy(self.s["view"]),
                frame=f"{tick}.ppm", unavailable=[]))
        bundle = dict(schema=2, contract="shared-l0-v2", mission=1, phase="before_commands", position_unit="pixel/256",
                      view=copy.deepcopy(self.s["view"]), ticks=[1, 3], samples=samples, runner=name)
        path = directory / "bundle.json"
        path.write_text(json.dumps(bundle))
        return path, bundle

    def test_first_divergence_and_alignment(self):
        a, _ = self.complete_bundle("reference")
        b, rows = self.complete_bundle("clone")
        self.assertEqual(l0.compare(a, b)["category"], "equal_observed_fields")
        rows["samples"][1]["objects"]["initial:0:3:18432:18432"]["hp"] = 99
        b.write_text(json.dumps(rows))
        self.assertEqual(l0.compare(a, b), dict(category="field_difference", tick=3,
                         object_id="initial:0:3:18432:18432", field="hp", reference=100, engine=99))
        for mutation in (lambda r: r["ticks"].reverse(), lambda r: r["samples"].pop(),
                         lambda r: r["view"]["camera"].__setitem__(0, 1),
                         lambda r: r["samples"][1].__setitem__("frame", "../escape.ppm")):
            bad = copy.deepcopy(rows)
            mutation(bad)
            b.write_text(json.dumps(bad))
            with self.assertRaises(ValueError):
                l0.compare(a, b)
        rows["samples"][1]["unavailable"] = ["queues"]
        rows["samples"][1]["queues"] = None
        b.write_text(json.dumps(rows))
        blocked = l0.compare(a, b)
        self.assertEqual(blocked["category"], "blocked_observation_contract")
        self.assertEqual(blocked["first_known_difference"]["tick"], 3)
        self.assertEqual(blocked["first_known_difference"]["field"], "hp")

    def test_shared_orchestrator_keeps_private_outputs_and_reports_blocks(self):
        fixture = legacy.ParityTests()
        fixture.directory = self.root
        game = fixture.synthetic_game()
        (game / "Aztec.exe").write_bytes(b"invented marker, never executed")
        scenarios = self.root / "scenarios"
        scenarios.mkdir()
        (scenarios / "private_l0.json").write_text(json.dumps(self.s))
        output = self.root / "shared"
        original = subprocess.run

        def invented_process(argv, **kwargs):
            self.assertEqual(argv[4:6], ["unshare", "-rn"])
            self.assertEqual(kwargs["env"]["SDL_VIDEODRIVER"], "dummy")
            self.assertEqual(kwargs["env"]["SDL_AUDIODRIVER"], "dummy")
            # Only synthetic controls replace namespace launch; no installation is read.
            if argv[10] == str(OBSERVER):
                env = kwargs["env"]
                return original([str(OBSERVER), env["RT_L0_SCHEDULE"], env["RT_L0_OUTPUT"], env["RT_DUMP_DIR"]],
                                **kwargs, timeout=10)
            return original(argv[10:], **kwargs, timeout=10)

        with patch.object(legacy.harness.report.subprocess, "run", side_effect=invented_process):
            status = legacy.harness.main(["private_l0", "--game", str(game), "--runtime", str(OBSERVER),
                                          "--engine", str(ENGINE), "--scenarios-dir", str(scenarios),
                                          "--output", str(output), "--flat-output"])
        self.assertEqual(status, 1)
        self.assertTrue((output / "reference.json").is_file())
        self.assertTrue((output / "clone.json").is_file())
        self.assertEqual(json.loads((output / "diff.json").read_text())["category"], "blocked_observation_contract")
        self.assertTrue((output / "reference/game/Res").is_symlink())
        self.assertFalse((output / "reference/game/Maps").is_symlink())
        self.assertTrue((output / "clone/tick_3.ppm").is_file())

    def test_pixels_are_compared_at_the_declared_tick(self):
        a, _ = self.complete_bundle("reference")
        b, _ = self.complete_bundle("clone")
        frame = b.parent / "3.ppm"
        data = bytearray(frame.read_bytes())
        data[-1] = 1
        frame.write_bytes(data)
        self.assertEqual(l0.compare(a, b), dict(category="frame_difference", tick=3, object_id=None, field="frame"))
        frame.write_bytes(data[:-1])
        with self.assertRaises(ValueError):
            l0.compare(a, b)

    def test_schema_and_native_parser_reject_bad_schedule(self):
        for ticks in ([], [3, 1], [1, 1], [True], [0, 1], [2, 3], [1, 2147483648]):
            bad = copy.deepcopy(self.s)
            bad["observe"]["ticks"] = ticks
            with self.assertRaises(ValueError):
                l0.validate_scenario(bad)
        schedule = self.root / "bad.txt"
        schedule.write_text("L0 2 0 0 0\n3\n1\n")
        done = subprocess.run([str(OBSERVER), str(schedule), str(self.root / "native"), str(self.root)],
                              capture_output=True, timeout=5)
        self.assertNotEqual(done.returncode, 0)

    def test_reference_native_tick_gate_and_truthful_unknowns(self):
        run = self.root / "reference"
        run.mkdir()
        schedule = run / "schedule"
        native = run / "native.jsonl"
        l0.write_schedule(self.s, schedule)
        subprocess.run([str(OBSERVER), str(schedule), str(native), str(run)], check=True, timeout=10)
        b = l0.bundle(self.s, native, "reference", self.root / "reference.json")
        self.assertEqual([row["tick"] for row in b["samples"]], [1, 3])
        identity = "initial:0:3:18432:18432"
        self.assertEqual(b["samples"][0]["objects"][identity]["owner"], 0)
        self.assertEqual(b["samples"][1]["objects"][identity]["hp"], 98)
        self.assertEqual(b["samples"][0]["money"]["0"], 123)
        self.assertTrue("frame_phase" in b["samples"][0]["unavailable"])
        rows = native.read_text().splitlines()
        duplicate = json.loads(rows[1])
        duplicate["objects"].append(copy.deepcopy(duplicate["objects"][0]))
        rows[1] = json.dumps(duplicate)
        native.write_text("\n".join(rows) + "\n")
        with self.assertRaises(ValueError):
            l0.bundle(self.s, native, "reference", self.root / "bad.json")

    def test_headless_schema_schedule_and_repeatable_frames(self):
        fixture = legacy.ParityTests()
        fixture.directory = self.root
        game = fixture.synthetic_game()
        outputs = []
        for repetition in (1, 2):
            base = self.root / str(repetition)
            run = base / "clone"
            run.mkdir(parents=True)
            schedule, native = run / "schedule", run / "native.jsonl"
            l0.write_schedule(self.s, schedule)
            env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
            trace_args = ["--combat-trace", str(run / "combat.jsonl"),
                          "--observations", str(run / "objects.jsonl"), "--order-observations"] if repetition == 2 else []
            result = subprocess.run([str(ENGINE), "--headless", "--game", str(game), "--rules-dir", str(ROOT / "docs/data"),
                                     "--no-auto-orders", "--max-ticks", "3", "--l0-schedule", str(schedule),
                                     "--l0-output", str(native), "--l0-frames", str(run),
                                     "--tick-hashes", str(run / "hashes.txt"), *trace_args, "1"],
                                    env=env, capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            outputs.append(l0.bundle(self.s, native, "clone", base / "clone.json"))
        self.assertEqual(outputs[0], outputs[1])
        self.assertEqual((self.root / "1/clone/hashes.txt").read_bytes(),
                         (self.root / "2/clone/hashes.txt").read_bytes())
        observations = [json.loads(row) for row in (self.root / "2/clone/objects.jsonl").read_text().splitlines()]
        self.assertEqual(observations[0]["phase"], "before_commands")
        self.assertTrue({"order", "dest_x", "dest_y", "target", "tactic", "scenario"} <=
                        set(observations[0]["fields"]))
        self.assertEqual([row["tick"] for row in observations[1:]], [1, 2, 3])
        for row in observations[1:]:
            self.assertTrue(row["objects"])
            for obj in row["objects"].values():
                self.assertEqual(set(obj), set(observations[0]["fields"]))
        combat = [json.loads(row) for row in (self.root / "2/clone/combat.jsonl").read_text().splitlines()]
        self.assertEqual(combat[0]["contract"], "engine-combat-v1")
        self.assertEqual([row["tick"] for row in combat[1:] if row["event"] == "tick"], [1, 2, 3])
        self.assertEqual(combat[-1], {"event": "end", "tick": 3})
        for tick in (1, 3):
            self.assertEqual((self.root / f"1/clone/tick_{tick}.ppm").read_bytes(),
                             (self.root / f"2/clone/tick_{tick}.ppm").read_bytes())
        self.assertEqual(outputs[0]["samples"][0]["objects"]["initial:0:3:18432:18432"]["hp"], 100)
        # Empty invented archives cannot establish a rendered original UI panel.
        self.assertEqual(outputs[0]["samples"][0]["unavailable"], ["panel_assets"])


if __name__ == "__main__":
    unittest.main()
