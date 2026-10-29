#!/usr/bin/env python3
"""Scenario-based parity harness. Captures and reports stay in ignored work/parity."""
import argparse
import copy
from datetime import datetime, timezone
import importlib.util
import json
from pathlib import Path
import re
import shutil
import sys

# Resolve sibling imports even under Python's isolated (-I) mode.
ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("parity_report", ROOT / "tools/parity_report.py")
report = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(report)
FIELDS = ["hp", "side", "type", "x", "y"]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, minimum=0):
    return type(value) is int and value >= minimum


def load_scenario(path):
    """Version 1 is deliberately narrow; reject unsupported semantics explicitly."""
    s = report.json_object(path.read_text())
    require(set(s) == {"schema", "id", "layer", "map", "setup", "input", "observe", "tolerance", "evidence"},
            "unexpected or missing scenario keys")
    require(type(s["schema"]) is int and s["schema"] == 1, "unsupported scenario schema")
    require(isinstance(s["id"], str) and re.fullmatch(r"[a-z0-9][a-z0-9_-]*", s["id"]), "invalid scenario ID")
    require(s["layer"] in [f"L{i}" for i in range(9)], "invalid layer")
    require(s["evidence"] in ("V", "M", "P"), "invalid evidence label")
    m = s["map"]
    require(isinstance(m, dict) and set(m) == {"kind", "mission"}, "invalid map specification")
    require(m["kind"] in ("synthetic", "campaign") and integer(m["mission"], 1) and m["mission"] <= 15,
            "map must select a synthetic fixture or campaign mission 1..15")
    require(isinstance(s["setup"], dict) and set(s["setup"]) <= {"sides", "money", "objects", "tech"},
            "invalid setup specification")
    require(isinstance(s["input"], list), "input must be a list")
    o = s["observe"]
    require(isinstance(o, dict) and set(o) == {"first_tick", "last_tick", "fields", "contract"}, "invalid observe specification")
    require(integer(o["first_tick"], 1) and o["first_tick"] == 1 and integer(o["last_tick"], 1)
            and o["last_tick"] <= 2147483647,
            "observations must start at tick 1 and have a positive horizon")
    require(isinstance(o["fields"], list) and o["fields"] and all(isinstance(f, str) and f for f in o["fields"])
            and len(set(o["fields"])) == len(o["fields"]), "invalid observation fields")
    require(isinstance(o["contract"], str) and o["contract"], "missing normalization contract")
    require(s["tolerance"] == {"exact": True} and type(s["tolerance"].get("exact")) is bool,
            "version 1 supports exact integer comparison only")
    if m["kind"] == "synthetic":
        require(s["layer"] == "L0", "synthetic fixtures only exercise L0")
        require(s["evidence"] == "P" and o["contract"] == "synthetic-l0-v1", "synthetic evidence must be P with synthetic-l0-v1")
        require(set(s["setup"]) == {"objects"} and isinstance(s["setup"]["objects"], dict), "invalid synthetic setup")
        validate_objects(s["setup"]["objects"], o["fields"])
        previous = 0
        for event in s["input"]:
            require(isinstance(event, dict) and set(event) == {"tick", "object", "set"}, "invalid synthetic input")
            require(integer(event["tick"], 1) and previous <= event["tick"] <= o["last_tick"], "input ticks must be ordered within horizon")
            require(isinstance(event["object"], str) and event["object"] in s["setup"]["objects"], "unknown synthetic object")
            require(isinstance(event["set"], dict) and event["set"] and set(event["set"]) <= set(o["fields"])
                    and all(type(v) is int for v in event["set"].values()), "invalid synthetic field update")
            previous = event["tick"]
    return s


def validate_objects(objects, fields):
    require(all(isinstance(k, str) and k and isinstance(v, dict) and set(v) == set(fields)
                and all(type(n) is int for n in v.values()) for k, v in objects.items()), "invalid synthetic objects")


def metadata(s):
    return dict(schema=1, mission=s["map"]["mission"], phase="before_commands", **s["observe"])


def mock_runner(s, output):
    """An invented state machine, not a game model or a reference measurement."""
    objects = copy.deepcopy(s["setup"]["objects"])
    with output.open("w") as stream:
        stream.write(json.dumps(metadata(s)) + "\n")
        for tick in range(1, s["observe"]["last_tick"] + 1):
            stream.write(json.dumps(dict(tick=tick, objects=objects)) + "\n")
            for event in s["input"]:
                if event["tick"] == tick:
                    objects[event["object"]].update(event["set"])


def validate_observations(s, path):
    with path.open() as stream:
        require(report.header(stream) == metadata(s), "observations do not match scenario mission, contract, fields or horizon")
    # Self comparison validates the complete stream, including truncation and field coverage.
    report.compare(path, path)


def reference_runner(s, args, directory):
    directory.mkdir()
    report.private_game(args.game, directory / "game")
    commands = directory / "commands.txt"
    commands.write_text("")
    if args.commands:
        shutil.copyfile(args.commands, commands)
    require(report.command_horizon(commands) <= s["observe"]["last_tick"], "commands exceed scenario horizon")
    script = directory / "script.txt"
    mission = s["map"]["mission"]
    navigation = (ROOT / "tests/scenarios/mission1_start.txt").read_text()
    row = 257 + 20 * (mission - 1 if mission <= 9 else mission - 10)
    navigation = navigation.replace("click 150 257", ("click 645 421\nwait 1200\n" if mission > 9 else "") + f"click 150 {row}")
    if args.script:
        navigation = args.script.read_text()
    script.write_text(navigation + f"\nwait {((s['observe']['last_tick'] + 11) // 12 + 20) * 1000}\nquit\n")
    report.execute([args.runtime, directory / "game/Aztec.exe"], directory, args.timeout, dict(
        RT_SCRIPT=str(script), RT_CMD_REPLAY=str(commands), RT_CMD_LOG=str(directory / "readback.txt"),
        RT_STATE_LOG=str(directory / "state.txt"), RT_UNIT_LOG=str(directory / "units.txt"),
        RT_UNIT_EVERY="1", RT_NO_GRAB="1", RT_NO_VIDEO="1", RT_RESOLUTION="800x600",
        RT_DUMP_DIR=str(directory)))
    report.state_rows(directory / "state.txt", s["observe"]["last_tick"])
    report.verify_packet_readback(commands, directory / "readback.txt", s["observe"]["last_tick"])
    if not args.reference_adapter:
        return report.result("blocked_observation_contract", detail="audited reference normalization/identity adapter is missing")
    adapter = directory / "adapter"
    adapter.mkdir()
    output = directory.parent / "reference.jsonl"
    report.execute([args.reference_adapter, "--state-log", directory / "state.txt", "--unit-log", directory / "units.txt",
                    "--mission", mission, "--ticks", s["observe"]["last_tick"], "--commands", commands,
                    "--output", output], adapter, args.timeout, setup_network=False)
    validate_observations(s, output)
    return None


def clone_runner(s, args, directory):
    directory.mkdir()
    report.private_game(args.game, directory / "game")
    output = directory.parent / "clone.jsonl"
    report.execute([args.engine, "--headless", "--game", directory / "game", "--rules-dir", ROOT / "docs/data",
                    "--no-auto-orders", "--max-ticks", s["observe"]["last_tick"], "--report", directory / "report.json",
                    "--observations", output, s["map"]["mission"]], directory, args.timeout, setup_network=False)
    engine = json.loads((directory / "report.json").read_text())
    require(engine["ticks"] == s["observe"]["last_tick"] and not engine["script_errors"]
            and not engine["unsupported_executions"], "clone did not complete the supported scenario horizon")
    validate_observations(s, output)
    return None


def run_scenario(s, args, directory):
    directory.mkdir()
    report.write_json(directory / "scenario.json", s)
    if s["map"]["kind"] == "synthetic":
        for name in ("reference", "clone"):
            mock_runner(s, directory / f"{name}.jsonl")
        verdict = report.compare(directory / "reference.jsonl", directory / "clone.jsonl")
        return dict(verdict, runners="mocked", evidence="P")
    missing = [str(p) for p in (args.game / "Aztec.exe", args.game / "Res", args.game / "Maps", args.runtime, args.engine) if not p.exists()]
    if missing:
        return report.result("skipped_missing_private_inputs", detail=", ".join(missing))
    if (s["layer"] != "L0" or s["setup"] or s["input"] or args.commands and report.command_horizon(args.commands)
            or s["observe"]["contract"] != "engine-index-v1" or s["observe"]["fields"] != FIELDS):
        return report.result("blocked_observation_contract", detail="setup/input translator or requested layer/normalization is not implemented")
    outcomes = []
    for name, runner in (("reference", reference_runner), ("clone", clone_runner)):
        try:
            outcomes.append(runner(s, args, directory / name))
        except (OSError, ValueError, RuntimeError, StopIteration) as exc:
            outcomes.append(report.result("experiment_failed", runner=name, detail=str(exc)))
    report.write_json(directory / "runners.json", outcomes)
    for category in ("experiment_failed", "blocked_observation_contract"):
        for outcome in outcomes:
            if outcome and outcome["category"] == category:
                return outcome
    return report.compare(directory / "reference.jsonl", directory / "clone.jsonl")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ids", nargs="*")
    parser.add_argument("--all", action="store_true", help="select every scenario")
    parser.add_argument("--list", action="store_true", help="list matching specifications without running")
    parser.add_argument("--layer", choices=[f"L{i}" for i in range(9)])
    parser.add_argument("--scenarios-dir", type=Path, default=ROOT / "tests/parity")
    parser.add_argument("--output", type=Path, default=ROOT / "work/parity" / datetime.now(timezone.utc).strftime("l0-%Y%m%dT%H%M%S%f"))
    parser.add_argument("--game", type=Path, default=ROOT / "game")
    parser.add_argument("--runtime", type=Path, default=ROOT / "build/aztec_recomp")
    parser.add_argument("--engine", type=Path, default=ROOT / "build/aztec_engine")
    parser.add_argument("--reference-adapter", type=Path)
    parser.add_argument("--script", type=Path, help="private RT_SCRIPT navigation file")
    parser.add_argument("--commands", type=Path, help="private oracle packet log; nonempty logs require a future clone translator")
    parser.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args(argv)
    try:
        require(args.timeout > 0, "timeout must be positive")
        require(not (args.all and args.ids), "use IDs or --all")
        for key in ("game", "runtime", "engine", "reference_adapter", "script", "commands"):
            if getattr(args, key) is not None:
                setattr(args, key, getattr(args, key).resolve())
        scenarios = [load_scenario(p) for p in sorted(args.scenarios_dir.glob("*.json"))]
        ids = [s["id"] for s in scenarios]
        require(len(set(ids)) == len(ids), "duplicate scenario IDs")
        require(not set(args.ids) - set(ids), "unknown scenario ID")
        selected = [s for s in scenarios if (not args.ids or s["id"] in args.ids) and (not args.layer or s["layer"] == args.layer)]
        require(selected, "no matching scenarios")
        if args.list:
            for s in selected:
                print(f"{s['id']}\t{s['layer']}\t{s['map']['kind']}\t{s['evidence']}")
            return 0
        require(args.ids or args.all or args.layer, "select scenario IDs, --all or --layer")
        output = report.private_output(args.output)
        # Never place a run inside any selected input directory (or overwrite an input).
        for p in (args.game, args.runtime, args.engine, args.reference_adapter, args.script, args.commands, args.scenarios_dir.resolve()):
            if p is not None:
                require(not output.is_relative_to(p) and not p.is_relative_to(output), "output overlaps an input path")
        output.mkdir(parents=True, exist_ok=False)
        rows = []
        for s in selected:
            try:
                verdict = run_scenario(s, args, output / s["id"])
            except (OSError, ValueError, RuntimeError, StopIteration) as exc:
                verdict = report.result("experiment_failed", detail=str(exc))
            report.write_json(output / s["id"] / "diff.json", verdict)
            rows.append(dict(id=s["id"], layer=s["layer"], **verdict))
            print(f"{s['id']}: {verdict['category']}")
        report.write_json(output / "report.json", rows)
        if all(r["category"] == "skipped_missing_private_inputs" for r in rows):
            return 77
        return int(any(r["category"] not in ("equal_observed_fields", "skipped_missing_private_inputs") for r in rows))
    except (OSError, ValueError) as exc:
        parser.exit(2, f"parity_run: {exc}\n")


if __name__ == "__main__":
    sys.exit(main())
