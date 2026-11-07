#!/usr/bin/env python3
"""Invented L0 contract, native reference-memory and headless clone controls."""
import copy
import importlib.util
import json
import os
import shutil
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
walk_spec = importlib.util.spec_from_file_location("idle_walk_measure", ROOT / "tools/idle_walk_measure.py")
idle_walk = importlib.util.module_from_spec(walk_spec)
walk_spec.loader.exec_module(idle_walk)
formation_spec = importlib.util.spec_from_file_location("formation_measure", ROOT / "tools/formation_measure.py")
formation = importlib.util.module_from_spec(formation_spec)
formation_spec.loader.exec_module(formation)
spec = importlib.util.spec_from_file_location("parity_initial", ROOT / "tools/parity_initial.py")
initial = importlib.util.module_from_spec(spec)
spec.loader.exec_module(initial)


class SharedControls(unittest.TestCase):
    def setUp(self):
        (ROOT / "work/parity").mkdir(parents=True, exist_ok=True)
        tmp = tempfile.TemporaryDirectory(dir=ROOT / "work/parity")
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.s = legacy.harness.load_scenario(ROOT / "tests/parity/private_l0.json")
        self.s["observe"]["ticks"] = [1, 3]

    def test_campaign_followup_scenarios(self):
        for mission in range(1, 16):
            scenario = legacy.harness.load_scenario(ROOT / f"tests/parity/{initial.scenario_id(mission)}.json")
            self.assertEqual(scenario["map"], dict(kind="campaign", mission=mission))
            self.assertEqual(scenario["observe"]["ticks"], [1, 6, 12])
            self.assertEqual(scenario["input"], [])
            l0.validate_scenario(scenario)

    def test_dense_idle_walk_schedules(self):
        directory = self.root / "dense"
        idle_walk.prepare(directory)
        for mission in (1, 2, 3):
            scenario = legacy.harness.load_scenario(directory / f"idle_walk_mission_{mission}.json")
            self.assertEqual(scenario["map"]["mission"], mission)
            self.assertEqual(scenario["observe"]["ticks"], list(range(1, 25)))
        with self.assertRaises(FileExistsError):
            idle_walk.prepare(directory)

    def walk_fixture(self, path, handle_base):
        # Walking counts from the cached single mission-1 capture (2026-11-06).
        # All identities, positions and individual assignments below are invented.
        # This tests the evidence summarizer, not simulation or reference parity.
        counts = [31, 33, 33, 31, 31, 31, 22, 22, 22, 22, 18, 17,
                  16, 16, 13, 13, 12, 12, 12, 12, 9, 8, 7, 4]
        velocities = ([0, -2091], [-2091, 0], [-1652, -1117])
        rows = [dict(schema=2, phase="before_commands", position_unit="pixel/256")]
        for tick, count in enumerate(counts, 1):
            objects, orders = [], []
            for i in range(60):
                handle = handle_base + i
                objects.append(dict(handle=handle, owner=0, type=27, x=i * 12288, y=12288))
                orders.append(dict(handle=handle, kind=0, unit_state=2 if i < count else 1,
                                   tactic_state=1, tactic_mode=3, anchor_tile=[i, 1], tile=[i, 1],
                                   step_destination=[i * 48, 24], step_velocity=velocities[i % 3]))
            rows.append(dict(tick=tick, objects=objects, diagnostics=dict(orders=orders)))
        path.write_text("".join(json.dumps(row) + "\n" for row in rows))
        return rows, counts

    def test_idle_walk_recorded_counts_and_signed_velocities(self):
        a, b = self.root / "walk-a.jsonl", self.root / "walk-b.jsonl"
        rows, counts = self.walk_fixture(a, 100)
        self.walk_fixture(b, 1000)
        summary = idle_walk.summarize(a)
        self.assertEqual(summary, idle_walk.summarize(b))
        self.assertEqual(summary["walking_counts"], counts)
        self.assertEqual(summary["first_observed_walk_counts"], {"1": 31, "2": 2})
        self.assertIsNone(summary["cohort"][-1]["first_observed_walk_tick"])
        self.assertEqual([unit["samples"][0]["step_velocity"] for unit in summary["cohort"][:3]],
                         [[0, -2091], [-2091, 0], [-1652, -1117]])
        self.assertEqual(summary["mission_seed"], [None] * 24)
        rows[5]["diagnostics"]["orders"][0]["step_velocity"] = [0, -2090]
        a.write_text("".join(json.dumps(row) + "\n" for row in rows))
        self.assertNotEqual(idle_walk.summarize(a), idle_walk.summarize(b))

    def test_idle_walk_rejects_incomplete_or_ambiguous_evidence(self):
        path = self.root / "walk-invalid.jsonl"
        rows, _ = self.walk_fixture(path, 100)
        for mutation in (lambda r: r.pop(), lambda r: r.pop(7),
                         lambda r: r[1]["objects"].append(r[1]["objects"][0]),
                         lambda r: r[2]["diagnostics"]["orders"].pop(),
                         lambda r: r[2]["objects"].pop(),
                         lambda r: r[1]["diagnostics"]["orders"][0].update(step_velocity=[True, 0])):
            with self.subTest(mutation=mutation):
                invalid = copy.deepcopy(rows)
                mutation(invalid)
                path.write_text("".join(json.dumps(row) + "\n" for row in invalid))
                with self.assertRaises(ValueError):
                    idle_walk.summarize(path)

    def formation_fixture(self, path, base):
        # Generated trajectories test reporting only; they prescribe no game rule.
        rows = [dict(schema=2, phase="before_commands", position_unit="pixel/256")]
        for tick in range(1, 61):
            objects, orders = [], []
            for i in range(3):
                x = (i * 48 + 24) * 256
                if i == 0:
                    x += min(max(tick - 1, 0), 6) * 2048
                if i == 2:
                    x += (tick - 1) * 100
                walking = (i == 0 and tick < 7) or i == 2
                objects.append(dict(handle=base + i, owner=0, type=27, x=x, y=24 * 256))
                orders.append(dict(handle=base + i, kind=0, tactic=base + 100 if i < 2 else base + 200,
                                   unit_state=2 if walking else 1, tactic_state=1, tactic_mode=3,
                                   anchor_tile=[1, 1], tile=[1, 0], step_destination=[72, 24],
                                   step_velocity=[2048 if i == 0 else 100, 0],
                                   member=dict(pool_ordinal=i, pool_count=3)))
            rows.append(dict(tick=tick, objects=objects, diagnostics=dict(orders=orders)))
        path.write_text("".join(json.dumps(row) + "\n" for row in rows))
        return rows

    def test_formation_dense_scenarios_and_endpoints(self):
        for mission in (1, 2, 3):
            s = formation.scenario(mission, 120)
            self.assertEqual(s["observe"]["ticks"], list(range(1, 121)))
            self.assertEqual(s["input"], [])
        a, b = self.root / "formation-a", self.root / "formation-b"
        self.formation_fixture(a, 100)
        self.formation_fixture(b, 1000)
        report = formation.compare(a, b, 60)
        self.assertTrue(report["repeatable"])
        first, idle, censored = report["endpoints"]
        self.assertEqual(first["first_walk_tick"], 1)
        self.assertEqual(first["first_displacement_tick"], 2)
        self.assertEqual(first["observed_idle_tail_tick"], 7)
        self.assertEqual(first["endpoint_tile"], [1, 0])
        self.assertEqual(first["endpoint_offset"], [0, -1])
        self.assertIsNone(idle["first_walk_tick"])
        self.assertEqual(idle["observed_idle_tail_tick"], 1)
        self.assertIsNone(censored["observed_idle_tail_tick"])
        self.assertIsNone(censored["endpoint_tile"])
        self.assertEqual(first["group"], idle["group"])
        self.assertNotEqual(first["group"], censored["group"])

    def test_formation_full_tail_and_member_repeatability(self):
        a, b = self.root / "formation-a", self.root / "formation-b"
        self.formation_fixture(a, 100)
        rows = self.formation_fixture(b, 1000)
        for change in (lambda r: r[-1]["objects"][0].update(x=123),
                       lambda r: r[35]["diagnostics"]["orders"][0]["member"].update(pool_ordinal=1),
                       lambda r: r[45]["diagnostics"]["orders"][0].update(anchor_tile=[2, 1]),
                       lambda r: r[55]["diagnostics"]["orders"][0].update(tactic=1200)):
            altered = copy.deepcopy(rows)
            change(altered)
            b.write_text("".join(json.dumps(row) + "\n" for row in altered))
            result = formation.compare(a, b, 60)
            self.assertFalse(result["repeatable"])
            self.assertEqual(result["endpoints"], [])

    def test_formation_rejects_sparse_missing_and_same_file(self):
        a, b = self.root / "formation-a", self.root / "formation-b"
        self.formation_fixture(a, 100)
        rows = self.formation_fixture(b, 1000)
        with self.assertRaises(ValueError):
            formation.compare(a, a, 60)
        for change in (lambda r: r.pop(40), lambda r: r.pop(),
                       lambda r: r[55]["objects"].pop(),
                       lambda r: r[1]["diagnostics"]["orders"][0].pop("tactic")):
            altered = copy.deepcopy(rows)
            change(altered)
            b.write_text("".join(json.dumps(row) + "\n" for row in altered))
            with self.assertRaises(ValueError):
                formation.compare(a, b, 60)

    def test_formation_clone_comparison_and_repeat_gate(self):
        a, b, c, d = [self.root / name for name in ("ref-a", "ref-b", "clone-a", "clone-b")]
        for path, base in ((a, 100), (b, 1000), (c, 2000), (d, 3000)):
            self.formation_fixture(path, base)
        result = formation.compare(a, b, 60, c, d)
        self.assertTrue(result["clone_repeatable_positions"])
        self.assertTrue(all(r["tick_60_equal"] for r in result["clone_comparison"]))
        for path in (c, d):
            rows = [json.loads(line) for line in path.read_text().splitlines()]
            for row in rows[2:]:
                row["objects"][0]["x"] = rows[1]["objects"][0]["x"]
            path.write_text("".join(json.dumps(row) + "\n" for row in rows))
        result = formation.compare(a, b, 60, c, d)
        first = result["clone_comparison"][0]
        self.assertEqual(first["first_position_difference_tick"], 2)
        self.assertTrue(first["moved_reference"])
        self.assertFalse(first["moved_clone"])
        self.assertFalse(first["tick_60_equal"])
        rows[-1]["objects"][0]["x"] += 1
        d.write_text("".join(json.dumps(row) + "\n" for row in rows))
        result = formation.compare(a, b, 60, c, d)
        self.assertFalse(result["clone_repeatable_positions"])
        self.assertNotIn("clone_comparison", result)

    def test_formation_collection_preserves_failures_and_independent_runs(self):
        args = formation.argparse.Namespace(output=self.root / "collection", game=self.root / "source-game",
                                            runtime=self.root / "runtime", engine=self.root / "engine", last_tick=60)
        with patch.object(formation.runner, "shared_runner", side_effect=RuntimeError("namespace denied")) as run:
            formation.collect(args)
        self.assertEqual(run.call_count, 12)
        self.assertEqual(len({call.args[2] for call in run.call_args_list}), 12)
        self.assertTrue(all(call.kwargs == dict(normalize=False) for call in run.call_args_list))
        for mission in (1, 2, 3):
            outcomes = json.loads((args.output / f"mission_{mission}/processes.json").read_text())
            self.assertEqual(len(outcomes), 4)
            self.assertTrue(all(row["status"] == "failed" for row in outcomes.values()))

    def test_formation_cli_pending_status_and_exclusive_report(self):
        a, b = self.root / "ref-a", self.root / "ref-b"
        self.formation_fixture(a, 100)
        self.formation_fixture(b, 1000)
        output = self.root / "formation-review.json"
        command = [sys.executable, "-I", str(ROOT / "tools/formation_measure.py"), "compare",
                   str(a), str(b), "--last-tick", "60", "--output", str(output)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(json.loads(output.read_text())["status"], "needs live run")
        before = output.read_bytes()
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(output.read_bytes(), before)

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
                      view=copy.deepcopy(self.s["view"]), ticks=self.s["observe"]["ticks"], samples=samples, runner=name)
        path = directory / "bundle.json"
        path.write_text(json.dumps(bundle))
        return path, bundle

    def test_campaign_tables_require_repeats_and_keep_unknown_domains(self):
        self.s["observe"]["ticks"] = [1, 6, 12]
        a, reference = self.complete_bundle("reference")
        b, clone = self.complete_bundle("clone")
        identity = next(iter(clone["samples"][0]["objects"]))
        clone["samples"][1]["objects"][identity].update(hp=99, x=18433)
        clone["samples"][1]["money"]["0"] = 99
        b.write_text(json.dumps(clone))
        roots = [self.root / f"run{i}" for i in range(4)]
        for root in roots:
            for name, path in (("reference", a), ("clone", b)):
                dest = root / "private_l0"
                dest.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, dest / f"{name}.json")
                for tick in (1, 6, 12):
                    shutil.copyfile(path.parent / f"{tick}.ppm", dest / f"{tick}.ppm")
        result = initial.mission_report(1, roots[:2], roots[2:])
        self.assertTrue(result["before_after_aligned"])
        row = result["before"]["rows"][1]
        self.assertEqual(row["observed_divergences"], 3)
        self.assertEqual(row["counts"]["positions"], dict(compared=2, divergences=1))
        self.assertIsNone(row["divergences"])
        for domain in ("tech", "population"):
            self.assertEqual(row["counts"][domain], dict(compared=None, divergences=None))
        left, right = copy.deepcopy(reference["samples"][0]), copy.deepcopy(clone["samples"][0])
        left["unavailable"] = ["orders", "position_fraction", "money"]
        left["objects"][identity]["orders"] = None
        counts = initial.count_sample(left, right)["counts"]
        for domain in ("orders", "positions", "money"):
            self.assertIsNone(counts[domain]["compared"])
        left, right = copy.deepcopy(reference["samples"][0]), copy.deepcopy(clone["samples"][0])
        right["objects"] = {}
        counts = initial.count_sample(left, right)["counts"]
        self.assertEqual(counts["objects"], dict(compared=1, divergences=1))
        self.assertEqual(counts["hp"], dict(compared=0, divergences=0))
        right["objects"] = copy.deepcopy(left["objects"])
        right["objects"][identity].update(owner=1, type=4, orders=dict(kind="move", target=None, x=1, y=2))
        counts = initial.count_sample(left, right)["counts"]
        for domain in ("owner", "type", "orders"):
            self.assertEqual(counts[domain], dict(compared=1, divergences=1))
        # A changed but internally repeatable baseline cannot justify an after-fix claim.
        for root in roots[2:]:
            changed = copy.deepcopy(reference)
            changed["samples"][0]["money"]["0"] += 1
            (root / "private_l0/reference.json").write_text(json.dumps(changed))
        self.assertFalse(initial.mission_report(1, roots[:2], roots[2:])["before_after_aligned"])
        (roots[1] / "private_l0/clone.json").write_text(json.dumps(reference | {"runner": "clone"}))
        unstable = initial.mission_report(1, roots[:2], None)
        self.assertEqual(unstable["before"]["category"], "run_nondeterminism")
        self.assertEqual(unstable["before"]["rows"], [])
        self.assertEqual(initial.mission_report(2, roots[:2], None)["before"]["category"], "missing_captures")
        (roots[0] / "private_l0/reference.json").write_text('{"schema":2}')
        self.assertEqual(initial.mission_report(1, roots[:2], None)["before"]["category"], "invalid_captures")

    def test_campaign_report_creates_fifteen_pending_tables_without_captures(self):
        output = self.root / "tables"
        self.assertEqual(initial.main(["--output", str(output)]), 1)
        report = json.loads((output / "report.json").read_text())
        self.assertEqual(len(report["missions"]), 15)
        self.assertTrue(all(r["status"] == "needs live run" for r in report["missions"]))
        self.assertEqual(len(list(output.glob("mission_*.md"))), 15)
        self.assertTrue("| 12 | — | population | — | — |" in (output / "mission_15.md").read_text())
        with self.assertRaises(SystemExit):
            initial.main(["--output", str(output)])

    def test_all_campaign_specs_use_shared_runners_with_invented_maps(self):
        fixture = legacy.ParityTests()
        fixture.directory = self.root
        game = fixture.synthetic_game()
        (game / "Aztec.exe").write_bytes(b"invented marker, never executed")
        maps = game / "Maps"
        for mission in range(1, 16):
            for extension in ("mp", "map"):
                if mission > 1:
                    shutil.copyfile(maps / f"mission_1.{extension}", maps / f"mission_{mission}.{extension}")
            (maps / f"mission_{mission}.scn").write_text("")
        original = subprocess.run
        current = 0

        def invented_process(argv, **kwargs):
            self.assertEqual(argv[4:6], ["unshare", "-rn"])
            env = kwargs["env"]
            self.assertEqual(env["SDL_VIDEODRIVER"], "dummy")
            self.assertEqual(env["SDL_AUDIODRIVER"], "dummy")
            if argv[10] == str(OBSERVER):
                script = Path(env["RT_SCRIPT"]).read_text()
                row = 257 + 20 * (current - 1 if current <= 9 else current - 10)
                self.assertTrue(f"click 150 {row}" in script)
                self.assertEqual("click 645 421" in script, current > 9)
                return original([str(OBSERVER), env["RT_L0_SCHEDULE"], env["RT_L0_OUTPUT"], env["RT_DUMP_DIR"]],
                                **kwargs, timeout=10)
            return original(argv[10:], **kwargs, timeout=10)

        args = legacy.argparse_args(game=game, engine=ENGINE, runtime=OBSERVER, commands=None, script=None, timeout=10)
        with patch.object(legacy.harness.report.subprocess, "run", side_effect=invented_process):
            for current in range(1, 16):
                scenario = legacy.harness.load_scenario(ROOT / f"tests/parity/{initial.scenario_id(current)}.json")
                base = self.root / f"mission_{current}"
                base.mkdir()
                for name in ("reference", "clone"):
                    legacy.harness.shared_runner(scenario, args, base / name, name)
                    bundle = json.loads((base / f"{name}.json").read_text())
                    self.assertEqual(bundle["mission"], current)
                    self.assertEqual(bundle["ticks"], [1, 6, 12])
                    self.assertFalse((base / name / "game/Maps").is_symlink())

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

    def test_repeat_review_keeps_candidates_provisional(self):
        a, _ = self.complete_bundle("reference")
        b, clone = self.complete_bundle("clone")
        identity = next(iter(clone["samples"][0]["objects"]))
        clone["samples"][0]["objects"][identity]["hp"] = 99
        clone["samples"][1]["objects"][identity]["x"] += 1
        b.write_text(json.dumps(clone))
        repeated = self.root / "repeated"
        for name in ("reference", "clone"):
            shutil.copytree(self.root / name, repeated / name)
        paths = a, b, repeated / "reference/bundle.json", repeated / "clone/bundle.json"
        review = l0.review_repeats(*paths)
        self.assertEqual(review["category"], "repeated_observations")
        self.assertEqual(review["initial_counts"], dict(reference=1, clone=1, common=1))
        self.assertFalse(review["hp_candidates"][0]["confirmed"])
        self.assertEqual(review["scalar_counts"][0], dict(tick=1, objects_compared=1,
                         reference_only=0, clone_only=0, fields_compared=9,
                         divergences=1, hp_divergences=1, position_divergences=0))
        self.assertEqual(review["scalar_counts"][1]["position_divergences"], 1)
        self.assertEqual(review["movement"][1]["clone"], [identity])
        (repeated / "clone/3.ppm").write_bytes(b"P6\n800 600\n255\n" + bytes([1]) * (800 * 600 * 3))
        self.assertEqual(l0.review_repeats(*paths)["category"], "run_nondeterminism")
        clone["samples"][1]["objects"][identity]["hp"] = 7
        paths[3].write_text(json.dumps(clone))
        self.assertEqual(l0.review_repeats(*paths)["category"], "run_nondeterminism")

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
        self.assertEqual(len(b["samples"][0]["objects"]), 63)
        self.assertEqual({obj["owner"] for obj in b["samples"][0]["objects"].values()}, {0, 1})
        self.assertEqual(set(b["samples"][0]["objects"]), set(b["samples"][1]["objects"]))
        self.assertEqual(b["samples"][1]["objects"][identity]["x"], 18432 + 128)
        self.assertNotIn("position_fraction", b["samples"][0]["unavailable"])
        self.assertEqual(b["samples"][0]["diagnostics"]["pool"], dict(
            capacity=133, allocated=63, free_slot_filtered=70, inactive_object_filtered=0, live_objects=63))
        self.assertTrue("frame_phase" in b["samples"][0]["unavailable"])
        counts = l0.diagnostic_counts(b)
        self.assertEqual(counts[0], dict(tick=1, owner=0, objects=32, decoded_unit_states=31,
                                        towns=1, queue_entries=0))
        self.assertEqual(counts[1]["queue_entries"], 1)
        # Repeated native streams include the same raw order/queue diagnostics.
        repeat = run / "repeat.jsonl"
        subprocess.run([str(OBSERVER), str(schedule), str(repeat), str(run)], check=True, timeout=10)
        self.assertEqual(native.read_bytes(), repeat.read_bytes())
        state_dir = self.root / "state-only"
        state_dir.mkdir()
        state_log = state_dir / "native.jsonl"
        subprocess.run([str(OBSERVER), str(schedule), str(state_log), str(state_dir)], check=True, timeout=10,
                       env=dict(os.environ, RT_L0_STATE_ONLY="1"))
        state_rows = [json.loads(line) for line in state_log.read_text().splitlines()][1:]
        self.assertEqual([r["tick"] for r in state_rows], [1, 3])
        self.assertTrue(all(r["frame"] is None and "frame" in r["unavailable"] for r in state_rows))
        self.assertFalse(list(state_dir.glob("*.ppm")))
        self.assertEqual([r["objects"] for r in state_rows],
                         [json.loads(line)["objects"] for line in native.read_text().splitlines()[1:]])
        clone_path = self.root / "clone.json"
        clone = copy.deepcopy(b)
        clone["runner"] = "clone"
        for sample in clone["samples"]:
            del sample["diagnostics"]
        clone_path.write_text(json.dumps(clone))
        verdict = l0.compare(self.root / "reference.json", clone_path)
        self.assertIsNone(verdict["first_known_difference"])
        self.assertEqual(verdict["notices"], [dict(category="free_slot_filtered", tick=t, count=70) for t in (1, 3)])
        clone["samples"][0]["objects"][identity]["hp"] = 97
        clone_path.write_text(json.dumps(clone))
        self.assertEqual(l0.compare(self.root / "reference.json", clone_path)["first_known_difference"]["field"], "hp")
        invalid = copy.deepcopy(b)
        invalid["samples"][0]["diagnostics"]["pool"]["live_objects"] = 64
        with self.assertRaises(ValueError):
            l0.validate_bundle(invalid, self.root)
        rows = native.read_text().splitlines()
        duplicate = json.loads(rows[1])
        duplicate["objects"].append(copy.deepcopy(duplicate["objects"][0]))
        rows[1] = json.dumps(duplicate)
        native.write_text("\n".join(rows) + "\n")
        with self.assertRaises(ValueError):
            l0.bundle(self.s, native, "reference", self.root / "bad.json")

    def test_pool_liveness_and_wrapped_queue_probes(self):
        for case in ("inactive", "sentinel_live", "classes", "queue", "production_span", "production_type", "cycle", "active_count"):
            run = self.root / case / "reference"
            run.mkdir(parents=True)
            schedule, native = run / "schedule", run / "native.jsonl"
            l0.write_schedule(self.s, schedule)
            result = subprocess.run([str(OBSERVER), str(schedule), str(native), str(run)],
                                    env=dict(os.environ, L0_TEST_CASE=case), capture_output=True, timeout=10)
            if case in ("cycle", "active_count"):
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(b"pool" in result.stderr)
                continue
            self.assertEqual(result.returncode, 0, result.stderr)
            b = l0.bundle(self.s, native, "reference", run.parent / "reference.json")
            d = b["samples"][0]["diagnostics"]
            if case == "inactive":
                self.assertEqual(d["pool"]["live_objects"], 62)
                self.assertEqual(d["pool"]["inactive_object_filtered"], 1)
            elif case == "queue":
                self.assertEqual(d["queues"][0]["queued_handles"], [0x600100, 0x600200])
                self.assertEqual(d["queues"][0]["raw"], dict(queue=0x620100, vtable=0x48ff84,
                                 data=0x651000, head=49, tail=1, capacity=50, count=2,
                                 queued_objects=[0x600108, 0x600208]))
                self.assertEqual(d["queues"][1]["owner"], 1)
                self.assertEqual(d["queues"][1]["queued_handles"], [0x600300])
                self.assertEqual(d["queues"][0]["queued_types"], [28, 27])
                self.assertEqual(d["queues"][1]["queued_types"], [28])
                self.assertIsNone(d["queues"][0]["production"])
                self.assertEqual(d["queues"][1]["production"], dict(
                    type=28, work_per_tick=17, price=93, accumulated_work=51, paused=True))
                self.assertEqual(d["queues"][1]["planner"], dict(
                    development=4, science_timer=-1, phase=14, speed=2))
                self.assertNotIn("diagnostics.queues", b["samples"][0]["unavailable"])
                self.assertIsNone(b["samples"][0]["queues"])
                self.assertTrue("queues" in b["samples"][0]["unavailable"])
            elif case in ("production_span", "production_type"):
                self.assertEqual(d["pool"]["live_objects"], 63)
                self.assertEqual(d["queues"][1]["production"], dict(
                    unavailable="Invalid L0 production object"))
                self.assertEqual(d["queues"][1]["queued_handles"], [0x600300])
            elif case == "sentinel_live":
                self.assertEqual(d["pool"]["live_objects"], 63)
                obj = b["samples"][0]["objects"]["initial:-1:3:-256000:-256000"]
                self.assertEqual(obj["hp"], 0)
            else:
                self.assertFalse("position_fraction" in b["samples"][0]["unavailable"])
                for i, kind in enumerate((0, 30, 10, 7, 83, 33, 3, 17)):
                    obj = next(o for o in b["samples"][0]["objects"].values() if o["type"] == kind)
                    self.assertEqual(obj["x"], (72 + i * 48) * 256 + (0 if i in (2, 7) else 64))

    def test_native_unit_execution_decode(self):
        run = self.root / "unit_steps" / "reference"
        run.mkdir(parents=True)
        schedule, native = run / "schedule", run / "native.jsonl"
        l0.write_schedule(self.s, schedule)
        subprocess.run([str(OBSERVER), str(schedule), str(native), str(run)],
                       env=dict(os.environ, L0_TEST_CASE="unit_steps"), check=True, timeout=10)
        b = l0.bundle(self.s, native, "reference", run.parent / "reference.json")
        for row in b["samples"]:
            self.assertEqual(row["diagnostics"]["mission_seed"], 0xfedcba98)
            for state, kind in enumerate(("inactive", "idle", "walk", "attack", "death", "idle_animation")):
                record = row["diagnostics"]["orders"][state + 1]
                self.assertEqual(record["unit_state"], state)
                self.assertEqual(record["execution_kind"], kind)
                self.assertEqual(record["step_destination"], [456, 789])
                self.assertEqual(record["step_velocity"], [2371, -512])
                self.assertEqual(record["attack_target_handle"], 0x600000)
                self.assertEqual(record["firing_target_handle"], 0x600200)
                self.assertEqual(record["pursuit_target_handle"], 0x600300)
                self.assertEqual(record["formation_marked"], bool(state % 2))
                self.assertEqual(record["idle_animation_timer"], -1)
                self.assertEqual(record["descriptor_speed_px"], 8)
                self.assertEqual(record["member"], dict(pool_ordinal=state + 1, pool_count=7,
                                                      tile=[11, 13], active=True, route_pending=True,
                                                      route_count=4, route_cursor=2, search_counter=7))
            missing = row["diagnostics"]["orders"][7]
            self.assertEqual(missing["member"]["unavailable"], "Missing L0 tactic member")
            self.assertIsNone(missing["descriptor_speed_px"])

    def test_invalid_member_probe_preserves_scalar_observations(self):
        errors = dict(member_duplicate="Duplicate L0 tactic member", member_span="Invalid L0 member pool span",
                      member_chain="Invalid L0 active pool chain", member_short="Short L0 tactic member record")
        for case, message in errors.items():
            with self.subTest(case=case):
                run = self.root / case / "reference"
                run.mkdir(parents=True)
                schedule, native = run / "schedule", run / "native.jsonl"
                l0.write_schedule(self.s, schedule)
                subprocess.run([str(OBSERVER), str(schedule), str(native), str(run)],
                               env=dict(os.environ, L0_TEST_CASE=case), check=True, timeout=10)
                bundle = l0.bundle(self.s, native, "reference", run.parent / "reference.json")
                for row in bundle["samples"]:
                    self.assertEqual(len(row["objects"]), 63)
                    member = row["diagnostics"]["orders"][1]["member"]
                    self.assertEqual(member["unavailable"], message)
                    self.assertNotIn("tile", member)
                    self.assertTrue("raw" in member)

    def test_page_committed_pool_padding(self):
        for case in ("pool_padding", "pool_tail"):
            with self.subTest(case=case):
                run = self.root / case / "reference"
                run.mkdir(parents=True)
                schedule, native = run / "schedule", run / "native.jsonl"
                l0.write_schedule(self.s, schedule)
                result = subprocess.run([str(OBSERVER), str(schedule), str(native), str(run)],
                                        env=dict(os.environ, L0_TEST_CASE=case), capture_output=True, timeout=10)
                if case == "pool_tail":
                    self.assertNotEqual(result.returncode, 0)
                    self.assertTrue(b"Invalid L0 free pool chain" in result.stderr)
                    continue
                self.assertEqual(result.returncode, 0, result.stderr)
                b = l0.bundle(self.s, native, "reference", run.parent / "reference.json")
                for row in b["samples"]:
                    self.assertEqual(len(row["objects"]), 63)
                    self.assertEqual(row["diagnostics"]["pool"]["capacity"], 155)
                    self.assertEqual(row["diagnostics"]["pool"]["free_slot_filtered"], 92)

    def test_optional_probe_failures_keep_both_samples_and_other_records(self):
        cases = {
            "invalid_queue": ("queues", "bounds", "count", 51),
            "queue_vtable": ("queues", "vtable", "vtable", 0),
            "queue_data": ("queues", "data span", "data", 0xfffffff0),
            "queue_head": ("queues", "bounds", "head", 50),
            "queue_tail": ("queues", "bounds", "tail", 50),
            "queue_capacity": ("queues", "bounds", "capacity", 0),
            "queue_null": ("queues", "queued object", "queued_objects", [0]),
            "queue_object_span": ("queues", "queued object", "queued_objects", [0xfffffff0]),
            "town_pool": ("queues", "pool metadata", "count", 3),
            "town_chain": ("queues", "active pool chain", "base", 0x620000),
            "short_town": ("queues", "Short", "stride", 0x100),
            "invalid_order": ("orders", "type or kind", "type", 90),
            "order_tactic": ("orders", "tactic span", "tactic", 0xfffffff0),
        }
        for case, (probe, reason, field, value) in cases.items():
            with self.subTest(case=case):
                run = self.root / case / "reference"
                run.mkdir(parents=True)
                schedule, native = run / "schedule", run / "native.jsonl"
                l0.write_schedule(self.s, schedule)
                result = subprocess.run([str(OBSERVER), str(schedule), str(native), str(run)],
                                        env=dict(os.environ, L0_TEST_CASE=case), capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stderr)
                b = l0.bundle(self.s, native, "reference", run.parent / "reference.json")
                self.assertEqual([r["tick"] for r in b["samples"]], [1, 3])
                for row in b["samples"]:
                    self.assertEqual(row["money"]["0"], 123)
                    self.assertEqual(len(row["objects"]), 62 if probe == "orders" else 63)
                    self.assertTrue(f"diagnostics.{probe}" in row["unavailable"])
                    if probe == "queues":
                        self.assertIsNone(l0.diagnostic_counts(b)[0]["queue_entries"])
                    records = row["diagnostics"][probe]
                    bad = records[1 if probe == "orders" else 0]
                    self.assertTrue(reason in bad["unavailable"])
                    self.assertEqual(bad["raw"][field], value)
                    self.assertNotIn("queued_handles", bad)
                    invalid = copy.deepcopy(b)
                    invalid["samples"][0]["unavailable"].remove(f"diagnostics.{probe}")
                    with self.assertRaises(ValueError):
                        l0.validate_bundle(invalid, run.parent)
                    invalid = copy.deepcopy(b)
                    del invalid["samples"][0]["diagnostics"][probe][1 if probe == "orders" else 0]["raw"]
                    with self.assertRaises(ValueError):
                        l0.validate_bundle(invalid, run.parent)
                    if probe == "queues" and case not in ("town_pool", "town_chain", "short_town"):
                        self.assertEqual(records[1]["owner"], 1)
                        self.assertEqual(records[1]["queued_handles"], [0x600300])
                        self.assertNotIn("unavailable", records[1])
                    if probe == "orders":
                        self.assertNotIn("unavailable", records[2])
                    # Normalized fields stay unavailable even when raw probes succeed.
                    self.assertTrue("orders" in row["unavailable"])
                    self.assertTrue("queues" in row["unavailable"])
                    self.assertNotIn("diagnostics.orders" if probe == "queues" else "diagnostics.queues",
                                     row["unavailable"])

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
        # Mission metadata HP=100 does not override the truck descriptor.
        self.assertEqual(outputs[0]["samples"][0]["objects"]["initial:0:3:18432:18432"]["hp"], 20000)
        # Empty invented archives cannot establish a rendered original UI panel.
        self.assertEqual(outputs[0]["samples"][0]["unavailable"], ["panel_assets"])


if __name__ == "__main__":
    unittest.main()
