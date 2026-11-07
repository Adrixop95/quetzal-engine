"""Synthetic save generator, tick-zero observation and fail-closed runner controls."""
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("compare_resume", ROOT / "tools/savegame/compare_resume.py")
resume = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(resume)
GENERATOR = Path(sys.argv.pop(1)).resolve()
PROBE = Path(sys.argv.pop(1)).resolve()


class ResumeTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "work").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / "work")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.save = self.root / "synthetic.sav"
        self.clone = self.root / "clone.json"
        subprocess.run([GENERATOR, "--generate", self.save, self.clone], check=True)
        self.state = json.loads(self.clone.read_text())
        self.state["save_sha256"] = hashlib.sha256(self.save.read_bytes()).hexdigest()
        self.write(self.clone, self.state)
        self.reference = self.root / "reference.json"
        self.write(self.reference, self.state)

    def write(self, path, value):
        path.write_text(json.dumps(value) + "\n")

    def test_generated_state_and_missing_coverage(self):
        state = resume.load_snapshot(self.clone)
        self.assertEqual(state["saved_tick"], 123)
        self.assertEqual(state["state"]["units"]["4"]["x256"], 25 * 256 + 64)
        self.assertEqual(state["state"]["units"]["90"]["town"], 0x80000007)
        result = resume.compare(self.reference, self.clone)
        self.assertEqual(result["status"], "incomplete")
        self.assertFalse(result["complete"])
        self.assertTrue("timers" in result["unsupported"])

    def test_probe_loads_generated_save_with_invented_installation(self):
        game = self.root / "invented game"
        maps = game / "mApS"
        maps.mkdir(parents=True)
        resources = game / "rEs"
        resources.mkdir()
        for name in ("Pal", "Units", "Tiles0", "Tiles1", "common", "fnts", "light"):
            (resources / (name + ".xbe")).write_bytes(struct.pack("<I", 0))
        (maps / "SYNTHETIC.MP").write_text("[Globals]\nMapSize=2,2\nSides=2\n[Side0]\n[Side1]\n")
        (maps / "SYNTHETIC.MAP").write_bytes(struct.pack("<HH4I", 2, 2, 0, 0, 0, 0))
        output = self.root / "probe.json"
        result = subprocess.run([PROBE, "--game", game, "--save", self.save, "--rules", ROOT / "docs/data",
                                 "--output", output], capture_output=True, text=True, timeout=8,
                                env=dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy"))
        self.assertEqual(result.returncode, 0, result.stderr)
        obj = json.loads(output.read_text())
        obj["save_sha256"] = self.state["save_sha256"]
        self.assertEqual(obj, self.state)
        for ticks in (0, 1, 12):
            continued = subprocess.run([PROBE, "--game", game, "--save", self.save,
                                       "--rules", ROOT / "docs/data", "--output", output,
                                       "--ticks", str(ticks)], capture_output=True, text=True, timeout=8)
            self.assertEqual(continued.returncode, 0, continued.stderr)
            obj = json.loads(output.read_text())
            obj["save_sha256"] = self.state["save_sha256"]
            self.write(output, obj)
            self.assertEqual(resume.load_snapshot(output)["elapsed_ticks"], ticks)
            self.assertEqual(obj["saved_tick"], 123)
        for ticks in ("-1", "3601", "1tail", ""):
            bad = subprocess.run([PROBE, "--ticks", ticks], capture_output=True, timeout=8)
            self.assertEqual(bad.returncode, 2)

    def test_mismatch_and_identity(self):
        state = copy.deepcopy(self.state)
        state["state"]["units"]["4"]["hp"] -= 1
        self.write(self.reference, state)
        self.assertEqual(resume.compare(self.reference, self.clone)["domain"], "units")
        state["save_sha256"] = "f" * 64
        self.write(self.reference, state)
        with self.assertRaises(ValueError):
            resume.compare(self.reference, self.clone)

    def test_continuation_boundaries_and_repeated_reference(self):
        obj = copy.deepcopy(self.state)
        obj.update(schema=2, elapsed_ticks=12, phase="after_tick_before_commands")
        obj["unsupported"] += sorted(resume.PRESENTATION)
        for path in (self.reference, self.clone):
            self.write(path, obj)
        repeat = self.root / "repeat.json"
        self.write(repeat, obj)
        result = resume.compare(self.reference, self.clone, repeat, repeat)
        self.assertTrue(result["repeated"])
        self.assertEqual(result["status"], "incomplete")
        obj["state"]["money"][0] += 1
        self.write(repeat, obj)
        self.assertEqual(resume.compare(self.reference, self.clone, repeat, self.clone)["status"],
                         "reference_nondeterminism")
        self.assertEqual(resume.compare(self.reference, self.clone, self.reference, repeat)["status"],
                         "clone_nondeterminism")
        obj["elapsed_ticks"] = 11
        self.write(self.clone, obj)
        with self.assertRaises(ValueError):
            resume.compare(self.reference, self.clone)
        for elapsed in (-1, 3601, True):
            obj["elapsed_ticks"] = elapsed
            self.write(self.clone, obj)
            with self.assertRaises(ValueError):
                resume.load_snapshot(self.clone)

    def test_missing_malformed_and_advanced_snapshots(self):
        mutations = [
            lambda x: x.update(elapsed_ticks=1),
            lambda x: x.update(elapsed_ticks=False),
            lambda x: x.update(saved_tick=True),
            lambda x: x.update(phase="after_tick"),
            lambda x: x.update(unsupported=[]),
            lambda x: x["unsupported"].append("orders"),
            lambda x: x["state"].pop("units"),
            lambda x: x["state"].update(scripts=None),
            lambda x: x["state"]["money"].append(1.5),
            lambda x: x["state"]["fog"][0]["values"].pop(),
            lambda x: x["state"]["units"]["4"].update(side=8),
            lambda x: (x["state"].update({k: 0 for k in x["unsupported"]}), x.update(unsupported=[])),
        ]
        for mutate in mutations:
            with self.subTest(mutate=mutate):
                state = copy.deepcopy(self.state)
                mutate(state)
                self.write(self.reference, state)
                with self.assertRaises(ValueError):
                    resume.load_snapshot(self.reference)
        self.reference.write_text('{"schema":1,"schema":1}')
        with self.assertRaises(ValueError):
            resume.load_snapshot(self.reference)

    def args(self):
        game = self.root / "source-game"
        game.mkdir()
        (game / "Maps").mkdir()
        (game / "Saves").mkdir()
        return SimpleNamespace(output=self.root / "run", game=game, save=self.save,
                               reference_adapter=self.root / "observer", clone=GENERATOR,
                               runtime=self.root / "runtime", rules=ROOT / "docs/data", timeout=20)

    def test_missing_reference_never_launches(self):
        args = self.args()
        args.reference_adapter = None
        with patch.object(resume.report, "execute") as execute:
            self.assertEqual(resume.run(args)["status"], "needs live run")
            execute.assert_not_called()

    def test_missing_save_records_pending_status(self):
        args = self.args()
        args.save = self.root / "missing.sav"
        with patch.object(resume.report, "execute") as execute:
            self.assertEqual(resume.run(args)["status"], "needs live run")
            execute.assert_not_called()
        self.assertEqual(json.loads((args.output / "result.json").read_text())["complete"], False)

    def test_runner_isolation_and_partial_result(self):
        args = self.args()
        calls = []

        def execute(argv, directory, timeout, extra, *, setup_network):
            calls.append((argv, directory))
            self.assertEqual(setup_network, directory.name.startswith("reference"))
            self.assertEqual(extra["RT_NO_VIDEO"], "1")
            obj = copy.deepcopy(self.state)
            del obj["save_sha256"]
            self.write(Path(argv[argv.index("--output") + 1]), obj)

        with patch.object(resume.report, "execute", side_effect=execute):
            self.assertEqual(resume.run(args)["status"], "incomplete")
        self.assertEqual(len(calls), 4)
        self.assertEqual(len({directory for _, directory in calls}), 4)
        self.assertEqual(len({Path(argv[argv.index("--save") + 1]) for argv, _ in calls}), 4)
        self.assertEqual(hashlib.sha256(self.save.read_bytes()).hexdigest(), self.state["save_sha256"])
        for argv, directory in calls:
            self.assertEqual(Path(argv[argv.index("--game") + 1]), directory / "game")
            self.assertFalse((directory / "game/Saves").is_symlink())
        with self.assertRaises(ValueError):
            resume.run(args)  # Never reuse reports from an older run.

    def test_runner_continuation_repeat_mismatch(self):
        args = self.args()
        args.ticks = 12

        def execute(argv, directory, timeout, extra, *, setup_network):
            obj = copy.deepcopy(self.state)
            del obj["save_sha256"]
            obj.update(schema=2, elapsed_ticks=12, phase="after_tick_before_commands")
            obj["unsupported"] += sorted(resume.PRESENTATION)
            if directory.name == "reference-repeat":
                obj["state"]["units"]["4"]["hp"] -= 1
            self.write(Path(argv[argv.index("--output") + 1]), obj)

        with patch.object(resume.report, "execute", side_effect=execute):
            result = resume.run(args)
        self.assertEqual(result["status"], "reference_nondeterminism")
        self.assertFalse(result["complete"])
        self.assertEqual(json.loads((args.output / "result.json").read_text()), result)

    def test_runner_refuses_ignored_continuation_boundary(self):
        args = self.args()
        args.ticks = 12

        def execute(argv, directory, timeout, extra, *, setup_network):
            self.assertEqual(argv[-2:], ["--ticks", "12"])
            obj = copy.deepcopy(self.state)
            del obj["save_sha256"]
            self.write(Path(argv[argv.index("--output") + 1]), obj)

        with patch.object(resume.report, "execute", side_effect=execute):
            result = resume.run(args)
        self.assertEqual(result["status"], "needs live run")
        self.assertTrue("boundary" in result["detail"])

    def test_namespace_failure_and_mutation_are_not_matches(self):
        args = self.args()
        with patch.object(resume.report, "execute", side_effect=RuntimeError("namespace denied")):
            self.assertEqual(resume.run(args)["status"], "needs live run")
        args.output = self.root / "mutated"

        def mutate(argv, directory, timeout, extra, *, setup_network):
            Path(argv[argv.index("--save") + 1]).write_bytes(b"changed")

        with patch.object(resume.report, "execute", side_effect=mutate):
            result = resume.run(args)
        self.assertEqual(result["status"], "needs live run")
        self.assertTrue("modified" in result["detail"])


if __name__ == "__main__":
    unittest.main()
