#!/usr/bin/env python3
"""L0 controls use invented state and assets only; no reference installation."""
import contextlib
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ENGINE = Path(sys.argv.pop(1)).resolve()
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("parity_run", ROOT / "tools/parity_run.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


class ParityTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "work/parity").mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=ROOT / "work/parity")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.scenario = harness.load_scenario(ROOT / "tests/parity/synthetic_l0.json")

    def main(self, *args):
        with contextlib.redirect_stdout(io.StringIO()):
            return harness.main(list(map(str, args)))

    def test_selection_and_mocked_end_to_end(self):
        output = self.directory / "run"
        self.assertEqual(self.main("--all", "--layer", "L0", "--game", self.directory / "absent",
                                   "--runtime", self.directory / "missing", "--output", output), 0)
        rows = json.loads((output / "report.json").read_text())
        self.assertEqual({r["category"] for r in rows}, {"equal_observed_fields", "skipped_missing_private_inputs"})
        row = next(r for r in rows if r["id"] == "synthetic_l0")
        self.assertEqual(row["runners"], "mocked")
        observations = (output / "synthetic_l0/clone.jsonl").read_text().splitlines()
        self.assertEqual(json.loads(observations[2])["objects"]["fixture:0"]["x"], 0)
        self.assertEqual(json.loads(observations[3])["objects"]["fixture:0"]["x"], 10)
        self.assertEqual(self.main("private_l0", "--game", self.directory / "absent", "--output", self.directory / "skip"), 77)
        before = set(self.directory.iterdir())
        self.assertEqual(self.main("--list", "--layer", "L0"), 0)
        self.assertEqual(set(self.directory.iterdir()), before)
        for args in (("--layer", "L8"), ("unknown",), ("synthetic_l0", "--output", output),
                     ("synthetic_l0", "--output", ROOT / "tests/parity/unsafe")):
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                self.main(*args)

    def test_schema_rejects_invalid_or_ambiguous_inputs(self):
        path = self.directory / "scenario.json"
        variants = []
        for key, value in (("schema", True), ("id", "../escape"), ("layer", "L9"),
                           ("evidence", "M"), ("tolerance", {"exact": 1}), ("extra", 0)):
            variants.append(dict(self.scenario, **{key: value}))
        for change in ({"first_tick": 2}, {"last_tick": 0}, {"fields": ["hp", "hp"]}, {"contract": "unknown"}):
            s = copy.deepcopy(self.scenario)
            s["observe"].update(change)
            variants.append(s)
        for change in ({"tick": True}, {"tick": 20}, {"object": "unknown"}, {"set": {"hp": 1.5}}):
            s = copy.deepcopy(self.scenario)
            s["input"][0].update(change)
            variants.append(s)
        for s in variants:
            with self.subTest(s=s):
                path.write_text(json.dumps(s))
                with self.assertRaises(ValueError):
                    harness.load_scenario(path)
        path.write_text('{"id":"a","id":"b"}')
        with self.assertRaises(ValueError):
            harness.load_scenario(path)

    def test_first_difference_and_full_stream_validation(self):
        a, b = self.directory / "a.jsonl", self.directory / "b.jsonl"
        harness.mock_runner(self.scenario, a)
        rows = [json.loads(line) for line in a.read_text().splitlines()]
        rows[2]["objects"]["fixture:0"].update(hp=99, x=3)
        rows[3]["objects"] = {}
        b.write_text("\n".join(map(json.dumps, rows)) + "\n")
        verdict = harness.report.compare(a, b)
        self.assertEqual((verdict["category"], verdict["tick"], verdict["object_id"], verdict["field"]),
                         ("field_difference", 2, "fixture:0", "hp"))
        # Malformed later data must override an earlier semantic difference.
        b.write_text("\n".join(map(json.dumps, rows[:-1])) + "\n")
        with self.assertRaises(ValueError):
            harness.report.compare(a, b)
        rows[2] = json.loads(a.read_text().splitlines()[2])
        b.write_text("\n".join(map(json.dumps, rows)) + "\n")
        self.assertEqual(harness.report.compare(a, b)["category"], "object_lifecycle")
        rows[0]["contract"] = "different"
        b.write_text("\n".join(map(json.dumps, rows)) + "\n")
        with self.assertRaises(ValueError):
            harness.report.compare(a, b)

    def synthetic_game(self):
        game = self.directory / "game"
        maps, resources = game / "Maps", game / "Res"
        maps.mkdir(parents=True)
        resources.mkdir()
        for name in ("Pal", "Units", "Tiles0", "Tiles1", "common", "fnts", "light"):
            (resources / f"{name}.xbe").write_bytes(struct.pack("<I", 0))
        (maps / "mission_1.mp").write_text(
            "[Globals]\nMapSize=8,8\nSides=2\nTeams=2\n"
            "[Side0]\nStart_X=1\nStart_Y=1\n[Side1]\nOnlyComp=1\nStart_X=6\nStart_Y=6\n"
            "[Team0]\nSide=0\nUnits=1\nUnit0=1,1,3,100\n"
            "[Team1]\nSide=1\nUnits=1\nUnit0=6,6,3,100\n")
        (maps / "mission_1.map").write_bytes(struct.pack("<HH64I", 8, 8, *([0] * 64)))
        (maps / "mission_1.scn").write_text("trigger(0) conditions localTime == 10 actions side(local).victory end\n")
        return game

    def test_real_headless_observer_and_failure_paths(self):
        game = self.synthetic_game()
        a, b = self.directory / "a.jsonl", self.directory / "b.jsonl"
        orders = self.directory / "orders.txt"
        orders.write_text("1 move 0 3 3 1\n")
        command = [str(ENGINE), "--headless", "--game", str(game), "--rules-dir", str(ROOT / "docs/data"),
                   "--max-ticks", "5", "--no-auto-orders", "--orders", str(orders)]
        for output in (a, b):
            completed = subprocess.run([*command, "--observations", str(output), "1"],
                                       capture_output=True, text=True, timeout=10)
            self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(harness.report.compare(a, b)["category"], "equal_observed_fields")
        rows = [json.loads(line) for line in a.read_text().splitlines()]
        first = rows[1]["objects"]["engine:1"]
        # Tile centre (1,1), 48 pixels per tile, fixed point in 1/256 pixels.
        self.assertEqual((first["x"], first["y"]), (72 * 256, 72 * 256))
        self.assertNotEqual(rows[-1]["objects"]["engine:1"]["x"], first["x"])
        completed = subprocess.run([*command, "--observations", str(self.directory / "missing/output"), "1"],
                                   capture_output=True, text=True, timeout=10)
        self.assertNotEqual(completed.returncode, 0)
        self.assertTrue("cannot open observations" in completed.stdout)
        # Early mission completion cannot be accepted as coverage through tick 20.
        subprocess.run([*command, "--max-ticks", "20", "--observations", str(b), "1"],
                       check=True, capture_output=True, timeout=10)
        with self.assertRaises(ValueError):
            harness.report.compare(b, b)

    @unittest.skipUnless(sys.platform.startswith("linux"), "private runners require Linux namespaces")
    def test_isolated_reference_environment_and_private_copy(self):
        game = self.synthetic_game()
        (game / "Aztec.exe").write_bytes(b"invented placeholder, never executed")
        (game / "profile.plr").write_text("invented profile")
        run = self.directory / "reference"
        run.mkdir()
        s = harness.load_scenario(ROOT / "tests/parity/private_l0.json")
        args = argparse_args(game=game, runtime=Path("/missing/runtime"), commands=None, script=None,
                             timeout=3, reference_adapter=None)

        def fake_run(argv, **kwargs):
            self.assertEqual(argv[:8], ["timeout", "-s", "KILL", "3", "unshare", "-rn", "sh", "-ec"])
            self.assertTrue('ip link set lo up\nip addr add' in argv[8])
            env = kwargs["env"]
            self.assertNotIn("RT_POISON", env)
            self.assertEqual(env["SDL_VIDEODRIVER"], "dummy")
            self.assertEqual(env["SDL_AUDIODRIVER"], "dummy")
            self.assertTrue(Path(env["RT_SCRIPT"]).is_file())
            self.assertEqual(Path(env["RT_CMD_REPLAY"]).read_text(), "")
            Path(env["RT_CMD_LOG"]).write_text("")
            Path(env["RT_STATE_LOG"]).write_text("\n".join(f"{i}" + " 0:0000000000000000" * 4 for i in range(1, 13)) + "\n")
            return subprocess.CompletedProcess(argv, 0)

        with patch.dict(os.environ, {"RT_POISON": "bad"}), patch.object(harness.report.subprocess, "run", side_effect=fake_run):
            verdict = harness.reference_runner(s, args, run / "process")
        self.assertEqual(verdict["category"], "blocked_observation_contract")
        private = run / "process/game"
        self.assertTrue((private / "Res").is_symlink())
        self.assertFalse((private / "Maps").is_symlink())
        self.assertFalse((private / "Saves").is_symlink())
        (private / "profile.plr").write_text("changed")
        self.assertEqual((game / "profile.plr").read_text(), "invented profile")
        with patch.object(harness.report.subprocess, "run", return_value=subprocess.CompletedProcess([], 1)):
            with self.assertRaises(RuntimeError):
                harness.reference_runner(s, args, run / "failed")

    @unittest.skipUnless(sys.platform.startswith("linux"), "private runners require Linux namespaces")
    def test_clone_runner_preserves_namespace_and_checks_requested_contract(self):
        game = self.synthetic_game()
        scenario = harness.load_scenario(ROOT / "tests/parity/private_l0.json")
        scenario["observe"]["last_tick"] = 5
        args = argparse_args(game=game, engine=ENGINE, timeout=5)
        original_run = subprocess.run

        def synthetic_process(argv, **kwargs):
            self.assertEqual(argv[:8], ["timeout", "-s", "KILL", "5", "unshare", "-rn", "sh", "-ec"])
            self.assertEqual(argv[8], 'exec "$@"')
            # Only this synthetic test strips isolation to work on restricted CI hosts.
            return original_run(argv[10:], **kwargs, timeout=5)

        with patch.object(harness.report.subprocess, "run", side_effect=synthetic_process):
            self.assertIsNone(harness.clone_runner(scenario, args, self.directory / "clone"))
        observations = self.directory / "clone.jsonl"
        harness.validate_observations(scenario, observations)
        scenario["observe"]["last_tick"] = 6
        with self.assertRaises(ValueError):
            harness.validate_observations(scenario, observations)

    def test_failed_reference_does_not_prevent_clone_and_is_never_a_pass(self):
        game = self.synthetic_game()
        (game / "Aztec.exe").write_bytes(b"invented placeholder, never executed")
        scenario = harness.load_scenario(ROOT / "tests/parity/private_l0.json")
        args = argparse_args(game=game, engine=ENGINE, runtime=ENGINE, commands=None)
        failure = harness.report.result("experiment_failed", runner="reference")
        with patch.object(harness, "reference_runner", return_value=failure), \
                patch.object(harness, "clone_runner", return_value=None) as clone:
            verdict = harness.run_scenario(scenario, args, self.directory / "failed")
        self.assertEqual(verdict["category"], "experiment_failed")
        clone.assert_called_once()
        scenario["setup"] = {"money": {"0": 123}}
        with patch.object(harness, "reference_runner") as reference:
            verdict = harness.run_scenario(scenario, args, self.directory / "blocked")
        self.assertEqual(verdict["category"], "blocked_observation_contract")
        reference.assert_not_called()


def argparse_args(**kwargs):
    from argparse import Namespace
    return Namespace(**kwargs)


if __name__ == "__main__":
    unittest.main()
