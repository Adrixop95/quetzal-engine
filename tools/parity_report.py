#!/usr/bin/env python3
"""Private mission replay orchestration and strict normalized-state comparison."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def result(category, tick=None, object_id=None, field=None, **details):
    return dict(category=category, tick=tick, object_id=object_id, field=field, **details)


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def private_output(path):
    path = path.resolve()
    if not path.is_relative_to((ROOT / "work/parity").resolve()):
        raise ValueError("output must be inside this worktree's work/parity")
    return path


def json_object(line):
    def unique(pairs):
        value = {}
        for key, item in pairs:
            if key in value:
                raise ValueError(f"duplicate observation key: {key}")
            value[key] = item
        return value

    value = json.loads(line, object_pairs_hook=unique)
    if not isinstance(value, dict):
        raise ValueError("observation must be a JSON object")
    return value


def header(stream):
    value = json_object(next(stream))
    if (value.get("schema") != 1 or value.get("phase") != "before_commands"
            or type(value.get("mission")) is not int
            or not isinstance(value.get("contract"), str) or not value["contract"]
            or not isinstance(value.get("fields"), list) or not value["fields"]
            or any(not isinstance(f, str) or not f for f in value["fields"])
            or len(set(value["fields"])) != len(value["fields"])
            or type(value.get("first_tick")) is not int or value["first_tick"] < 1
            or type(value.get("last_tick")) is not int
            or value["last_tick"] < value["first_tick"]):
        raise ValueError("invalid normalized observation header")
    return value


def snapshots(stream, metadata):
    for tick in range(metadata["first_tick"], metadata["last_tick"] + 1):
        try:
            row = json_object(next(stream))
        except StopIteration as exc:
            raise ValueError(f"truncated observation log at tick {tick}") from exc
        objects = row.get("objects")
        if type(row.get("tick")) is not int or row["tick"] != tick or not isinstance(objects, dict):
            raise ValueError(f"invalid observation at tick {tick}")
        for identity, fields in objects.items():
            if (not identity or not isinstance(fields, dict)
                    or set(fields) != set(metadata["fields"])
                    or any(type(v) is not int for v in fields.values())):
                raise ValueError(f"invalid fields at tick {tick}, object {identity}")
        yield row
    if next(stream, None) is not None:
        raise ValueError("extra observations beyond declared horizon")


def compare(reference, engine):
    """Validate both entire streams even when a difference occurs early."""
    with reference.open() as left, engine.open() as right:
        a, b = header(left), header(right)
        if a != b:
            raise ValueError("observation headers differ (mission, horizon or contract)")
        first = None
        for x, y in zip(snapshots(left, a), snapshots(right, b), strict=True):
            for identity in sorted(set(x["objects"]) | set(y["objects"])):
                if identity not in x["objects"] or identity not in y["objects"]:
                    difference = result("object_lifecycle", x["tick"], identity, "presence",
                                        reference=identity in x["objects"], engine=identity in y["objects"])
                    first = first or difference
                    continue
                for field in sorted(a["fields"]):
                    u, v = x["objects"][identity][field], y["objects"][identity][field]
                    if u != v:
                        first = first or result("field_difference", x["tick"], identity, field,
                                                reference=u, engine=v)
        return first or result("equal_observed_fields", through_tick=a["last_tick"])


def command_horizon(path):
    last = 0
    seen = set()
    for number, line in enumerate(path.read_text().splitlines(), 1):
        words = line.split()
        if len(words) != 55:
            raise ValueError(f"{path}:{number}: expected frame, side and 53 hex dwords")
        frame, side = map(int, words[:2])
        data = [int(word, 16) for word in words[2:]]
        if (not max(last, 1) <= frame <= 0xffffffff or not 0 <= side < 16 or (frame, side) in seen
                or any(not 0 <= word <= 0xffffffff for word in data)):
            raise ValueError(f"{path}:{number}: invalid or duplicate oracle packet")
        seen.add((frame, side))
        last = frame
    return last


def state_rows(path, ticks):
    rows = []
    for line in path.read_text().splitlines():
        words = line.split()
        if len(words) != 5 or any(not re.fullmatch(r"\d+:[0-9a-fA-F]{16}", w) for w in words[1:]):
            raise ValueError(f"invalid reference state row in {path}")
        tick = int(words[0])
        if tick != len(rows) + 1:
            raise ValueError(f"non-contiguous reference state in {path}")
        rows.append(words[1:])
        if tick == ticks:
            return rows
    raise ValueError(f"reference state truncated before tick {ticks}: {path}")


def private_game(source, destination):
    """Only known immutable assets are symlinked; writable state is copied."""
    destination.mkdir()
    for name in ("Aztec.exe", "Res", "Video", "Music", "Campaign"):
        if (source / name).exists():
            (destination / name).symlink_to((source / name).resolve())
    for name in ("Maps", "Saves"):
        if (source / name).exists():
            shutil.copytree(source / name, destination / name, symlinks=False)
        else:
            (destination / name).mkdir()
    for item in source.iterdir():
        if item.is_file() and item.suffix.lower() in (".ini", ".plr"):
            shutil.copy2(item, destination / item.name)


def execute(argv, directory, timeout, extra=None):
    env = {k: v for k, v in os.environ.items() if not k.startswith("RT_")}
    env.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    env.update(extra or {})
    # Fail closed: shell -e stops before the runtime on either network setup error.
    isolated = ["timeout", "-s", "KILL", str(timeout), "unshare", "-rn", "sh", "-ec",
                'ip link set lo up\nip addr add 192.168.1.125/24 dev lo\nexec "$@"',
                "parity", *map(str, argv)]
    write_json(directory / "invocation.json", dict(argv=isolated, environment={
        k: v for k, v in env.items() if k.startswith(("RT_", "SDL_"))}))
    with (directory / "process.log").open("w") as output:
        completed = subprocess.run(isolated, env=env, cwd=directory, stdout=output,
                                   stderr=subprocess.STDOUT, check=False)
    if completed.returncode:
        raise RuntimeError(f"isolated process exited {completed.returncode}; see {directory / 'process.log'}")


def replay_mission(args, mission, recording, directory):
    directory.mkdir()
    commands = directory / "commands.txt"
    shutil.copy2(recording / "cmd.txt", commands)
    last_command = command_horizon(commands)
    ticks = args.ticks
    if ticks is None:
        recorded = (recording / "state.txt").read_text().splitlines()
        if not recorded:
            raise ValueError("empty recorded state log; supply --ticks for an explicit horizon")
        ticks = int(recorded[-1].split()[0])
        state_rows(recording / "state.txt", ticks)
    if ticks < max(1, last_command):
        raise ValueError("replay horizon must include every recorded command")
    write_json(directory / "inputs.json", dict(mission=mission, ticks=ticks,
               commands_sha256=hashlib.sha256(commands.read_bytes()).hexdigest(),
               runtime_sha256=hashlib.sha256(args.runtime.read_bytes()).hexdigest()))
    reference_runs = []
    for repetition in (1, 2):
        run = directory / f"reference-{repetition}"
        run.mkdir()
        game = run / "game"
        private_game(args.game, game)
        script = (ROOT / "tests/scenarios/mission1_start.txt").read_text()
        row = 257 + 20 * (mission - 1 if mission <= 9 else mission - 10)
        navigation = ("click 645 421\nwait 1200\n" if mission > 9 else "") + f"click 150 {row}"
        script = script.replace("click 150 257", navigation)
        # Wall-clock waiting is only a watchdog; state coverage is checked separately.
        script += f"\nwait {((ticks + 11) // 12 + 20) * 1000}\nquit\n"
        (run / "start.txt").write_text(script)
        execute([args.runtime, game / "Aztec.exe"], run, args.timeout, dict(
            RT_CMD_REPLAY=str(commands), RT_CMD_LOG=str(run / "commands-read.txt"),
            RT_STATE_LOG=str(run / "state.txt"), RT_UNIT_LOG=str(run / "units.txt"),
            RT_UNIT_EVERY="1", RT_SCRIPT=str(run / "start.txt"), RT_NO_GRAB="1",
            RT_NO_VIDEO="1", RT_RESOLUTION="800x600"))
        reference_runs.append(state_rows(run / "state.txt", ticks))
    for index, (a, b) in enumerate(zip(*reference_runs, strict=True), 1):
        for pool, (u, v) in enumerate(zip(a, b, strict=True)):
            if u != v:
                return result("reference_nondeterminism", index, field=f"pool[{pool}]",
                              detail="pool hash/count only; object and semantic field unavailable")
    if not args.engine_adapter or not args.reference_adapter:
        return result("blocked_observation_contract", detail=(
            "reference replay repeated; engine packet replay and reference normalized observer adapters required"))
    observed = directory / "reference.jsonl"
    observer = directory / "observer"
    observer.mkdir()
    execute([args.reference_adapter, "--state-log", directory / "reference-1/state.txt",
             "--unit-log", directory / "reference-1/units.txt", "--mission", mission,
             "--ticks", ticks, "--commands", commands, "--output", observed], observer, args.timeout)
    engine = directory / "engine"
    engine.mkdir()
    private_game(args.game, engine / "game")
    execute([args.engine_adapter, "--game", engine / "game", "--mission", mission,
             "--ticks", ticks, "--commands", commands, "--output", engine / "state.jsonl"], engine, args.timeout)
    verdict = compare(observed, engine / "state.jsonl")
    with observed.open() as stream:
        metadata = header(stream)
    if metadata["mission"] != mission or metadata["first_tick"] != 1 or metadata["last_tick"] != ticks:
        raise ValueError("adapter observations do not cover the requested mission/horizon")
    return verdict


def run_corpus(args):
    output = private_output(args.output)
    output.mkdir(parents=True, exist_ok=False)
    recordings = sorted(args.corpus.glob("m*/cmd.txt"), key=lambda p: p.parent.name)
    if not recordings:
        raise ValueError("no m<N>/cmd.txt recordings found")
    report = []
    for command in recordings:
        match = re.fullmatch(r"m([1-9]\d*)", command.parent.name)
        if not match:
            continue
        mission = int(match[1])
        try:
            if mission > 15:
                raise ValueError("campaign navigation supports missions 1 through 15")
            verdict = replay_mission(args, mission, command.parent, output / command.parent.name)
        except (OSError, ValueError, RuntimeError, StopIteration) as exc:
            verdict = result("experiment_failed", detail=str(exc))
        report.append(dict(mission=mission, **verdict))
        write_json(output / "report.json", report)
        print(f"mission {mission}: {verdict['category']}")
    if not report:
        raise ValueError("no valid mission recording directories found")
    return 0 if all(r["category"] == "equal_observed_fields" for r in report) else 1


class Controls(unittest.TestCase):
    def test_comparison(self):
        with tempfile.TemporaryDirectory() as temporary:
            a, b = Path(temporary) / "a", Path(temporary) / "b"
            meta = dict(schema=1, mission=1, phase="before_commands", contract="synthetic-v1",
                        fields=["x", "hp"], first_tick=1, last_tick=2)
            rows = [dict(tick=i, objects={"spawn:0": dict(x=i, hp=100)}) for i in (1, 2)]

            def save(path, values):
                path.write_text("\n".join(json.dumps(v) for v in [meta, *values]) + "\n")

            save(a, rows)
            save(b, rows)
            self.assertEqual(compare(a, b)["category"], "equal_observed_fields")
            rows[1]["objects"]["spawn:0"]["hp"] = 99
            save(b, rows)
            self.assertEqual(compare(a, b), result("field_difference", 2, "spawn:0", "hp", reference=100, engine=99))
            rows[1]["objects"] = {}
            save(b, rows)
            self.assertEqual(compare(a, b)["category"], "object_lifecycle")
            save(b, rows[:1])
            with self.assertRaises(ValueError):
                compare(a, b)
            b.write_text(json.dumps(meta) + '\n{"tick":1,"objects":{},"objects":{}}\n')
            with self.assertRaises(ValueError):
                compare(a, b)
            b.write_text("[]\n")
            with self.assertRaises(ValueError):
                compare(a, b)
            rows[1]["tick"] = 3
            save(b, rows)
            with self.assertRaises(ValueError):
                compare(a, b)
            save(b, rows[:1])
            meta["contract"] = "different"
            save(b, rows[:1])
            with self.assertRaises(ValueError):
                compare(a, b)

    def test_packets_and_state(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "log"
            path.write_text("1 0 " + " ".join(["0"] * 53) + "\n")
            self.assertEqual(command_horizon(path), 1)
            path.write_text(path.read_text() * 2)
            with self.assertRaises(ValueError):
                command_horizon(path)
            path.write_text("1" + " 0:0000000000000000" * 4 + "\n")
            self.assertEqual(len(state_rows(path, 1)), 1)
            with self.assertRaises(ValueError):
                state_rows(path, 2)

    def test_isolation_failure(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as temporary:
            with patch("subprocess.run", return_value=subprocess.CompletedProcess([], 1)) as mock:
                with self.assertRaises(RuntimeError):
                    execute(["unused"], Path(temporary), 1)
                self.assertTrue("unshare" in mock.call_args.args[0])
                self.assertTrue("-ec" in mock.call_args.args[0])
                self.assertEqual(mock.call_count, 1)

    def test_private_game(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            (source / "Res").mkdir()
            (source / "Maps").mkdir()
            (source / "Maps/mission.mp").write_text("invented map")
            (source / "profile.plr").write_text("invented profile")
            private_game(source, root / "copy")
            self.assertTrue((root / "copy/Res").is_symlink())
            self.assertFalse((root / "copy/Maps").is_symlink())
            (root / "copy/Maps/mission.mp").write_text("changed")
            (root / "copy/profile.plr").write_text("changed")
            self.assertEqual((source / "Maps/mission.mp").read_text(), "invented map")
            self.assertEqual((source / "profile.plr").read_text(), "invented profile")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    run = sub.add_parser("run", help="replay every m<N>/cmd.txt twice before engine comparison")
    for flag in ("corpus", "game", "runtime"):
        run.add_argument("--" + flag, type=lambda p: Path(p).resolve(), required=True)
    run.add_argument("--output", type=Path, default=ROOT / "work/parity/latest")
    run.add_argument("--ticks", type=int, help="explicit horizon, otherwise use recorded state.txt")
    run.add_argument("--timeout", type=int, default=600, help="hard timeout in seconds for each process")
    run.add_argument("--engine-adapter", type=lambda p: Path(p).resolve())
    run.add_argument("--reference-adapter", type=lambda p: Path(p).resolve())
    diff = sub.add_parser("compare", help="compare validated normalized JSONL streams")
    diff.add_argument("reference", type=Path)
    diff.add_argument("engine", type=Path)
    diff.add_argument("--output", type=Path, required=True)
    sub.add_parser("self-test", help="run invented equal/different/truncated controls")
    args = parser.parse_args()
    try:
        if args.mode == "self-test":
            return 0 if unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls)).wasSuccessful() else 1
        if args.mode == "run":
            if args.timeout <= 0 or (args.ticks is not None and args.ticks <= 0):
                raise ValueError("timeout and ticks must be positive")
            return run_corpus(args)
        output = private_output(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        if output.exists():
            raise ValueError("output already exists")
        verdict = compare(args.reference, args.engine)
        write_json(output, verdict)
        return 0 if verdict["category"] == "equal_observed_fields" else 1
    except (OSError, ValueError, RuntimeError, StopIteration) as exc:
        parser.exit(2, f"parity_report: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
