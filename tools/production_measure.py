#!/usr/bin/env python3
"""Collect and compare passive production totals without inventing spawn identities."""
import argparse
from collections import Counter
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("production_runner", ROOT / "tools/parity_run.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)
require = runner.require
DENSE = [1, *range(100, 1301, 100), *range(1350, 1601, 50)]
SIDES = tuple(map(str, range(4)))


def scenario(mission, every_tick=False):
    s = runner.load_scenario(ROOT / f"tests/parity/private_l0_mission_{mission}.json")
    s["id"] = f"production_mission_{mission}"
    s["observe"]["ticks"] = list(range(1, 1601)) if every_tick else DENSE
    return runner.l0.validate_scenario(s)


def collect(args):
    """Reuse private copies, dummy SDL, namespaces and process/horizon checks."""
    output = runner.report.private_output(args.output)
    for path in (args.game, args.runtime, args.engine):
        require(not output.is_relative_to(path) and not path.is_relative_to(output), "output overlaps input")
    output.mkdir(parents=True, exist_ok=False)
    for mission in (2, 3):
        directory = output / f"mission_{mission}"
        directory.mkdir()
        s = scenario(mission, args.every_tick)
        runner.report.write_json(directory / "scenario.json", s)
        outcomes = {}
        for name in ("reference", "clone"):
            for repeat in (1, 2):
                key = f"{name}-{repeat}"
                try:
                    # Aggregates need neither stable cross-program identities nor
                    # normalized orders. Keep the strict L0 bundle contract intact.
                    runner.shared_runner(s, args, directory / key, name, normalize=False)
                    outcomes[key] = dict(status="completed")
                except (OSError, ValueError, RuntimeError) as exc:
                    outcomes[key] = dict(status="failed", detail=str(exc))
                runner.report.write_json(directory / "processes.json", outcomes)
    return output


def summarize(path, s):
    rows = [runner.l0.json_object(line) for line in path.read_text().splitlines()]
    require(rows and rows.pop(0) == dict(schema=2, phase="before_commands", position_unit="pixel/256"),
            "invalid native observer header")
    require(all(type(r.get("tick")) is int for r in rows)
            and [r.get("tick") for r in rows] == s["observe"]["ticks"], "incomplete or wrong schedule")
    summary = []
    for row in rows:
        require(set(row) in (runner.l0.SAMPLE, runner.l0.SAMPLE | {"diagnostics"}), "invalid sample")
        require(isinstance(row["unavailable"], list) and all(isinstance(f, str) for f in row["unavailable"])
                and "money" not in row["unavailable"]
                and "objects" not in row["unavailable"], "missing measured fields")
        require(isinstance(row["money"], dict) and set(row["money"]) == set(SIDES)
                and all(type(v) is int for v in row["money"].values()), "invalid balances")
        require(isinstance(row["objects"], list), "invalid object list")
        handles = set()
        counts = {side: Counter() for side in SIDES}
        hp = dict.fromkeys(SIDES, 0)
        neutral = 0
        for obj in row["objects"]:
            require(isinstance(obj, dict) and set(obj) == runner.l0.FIELDS | {"handle"}
                    and all(type(obj[k]) is int for k in runner.l0.FIELDS - {"orders"} | {"handle"}),
                    "invalid object fields")
            require(obj["handle"] > 0 and obj["handle"] not in handles, "duplicate or invalid handle")
            handles.add(obj["handle"])
            require(-1 <= obj["type"] < 90 and -1 <= obj["owner"] < 4, "invalid type or owner")
            if obj["owner"] == -1:
                neutral += 1
                continue
            side = str(obj["owner"])
            counts[side][str(obj["type"])] += 1
            hp[side] += obj["hp"]
        summary.append(dict(tick=row["tick"], neutral_objects=neutral, sides={
            side: dict(objects=sum(counts[side].values()), by_type=dict(counts[side]),
                       hp=hp[side], money=row["money"][side]) for side in SIDES}))
    return summary


def transitions(rows, side):
    """A sampled balance change is net cash flow, not an identified harvest."""
    return [dict(after_tick=a["tick"], through_tick=b["tick"],
                 delta=b["sides"][side]["money"] - a["sides"][side]["money"])
            for a, b in zip(rows, rows[1:])
            if a["sides"][side]["money"] != b["sides"][side]["money"]]


def compare(directory):
    s = runner.load_scenario(directory / "scenario.json")
    mission = s["map"]["mission"]
    require(mission in (2, 3) and s in (scenario(mission), scenario(mission, True)), "wrong production scenario")
    outcomes = runner.l0.json_object((directory / "processes.json").read_text())
    keys = [f"{name}-{n}" for name in ("reference", "clone") for n in (1, 2)]
    require(set(outcomes) == set(keys) and all(outcomes[k] == dict(status="completed") for k in keys),
            "missing or failed process; inspect processes.json and process.log")
    series = {key: summarize(directory / key / "observations.jsonl", s) for key in keys}
    stable = all(series[f"{name}-1"] == series[f"{name}-2"] for name in ("reference", "clone"))
    if not stable:
        return dict(mission=mission, status="unstable aggregates; needs live run", rows=[])
    a, b = series["reference-1"], series["clone-1"]
    rows = [dict(tick=x["tick"], reference=x, clone=y) for x, y in zip(a, b)]
    windows = []
    # Compare net growth over each complete 100-tick interval. Tick 1 is
    # not tick zero; the first complete interval starts at tick 100.
    by_tick = {r["tick"]: r for r in rows}
    for end in range(200, 1601, 100):
        start = end - 100
        for side in SIDES:
            growth = {name: by_tick[end][name]["sides"][side]["objects"]
                      - by_tick[start][name]["sides"][side]["objects"] for name in ("reference", "clone")}
            windows.append(dict(start=start, end=end, side=int(side), **growth,
                                within_one=abs(growth["reference"] - growth["clone"]) <= 1))
    return dict(mission=mission, status="needs live run", repeatable_aggregates=True, rows=rows,
                first_difference={side: {field: next((x["tick"] for x, y in zip(a, b)
                    if x["sides"][side][field] != y["sides"][side][field]), None)
                    for field in ("objects", "by_type", "hp", "money")} for side in SIDES},
                growth_windows=windows, money_equal=all(x["sides"][side]["money"] == y["sides"][side]["money"]
                                                       for x, y in zip(a, b) for side in SIDES),
                cash_transitions={name: {side: transitions(values, side) for side in SIDES}
                                  for name, values in (("reference", a), ("clone", b))})


def write_comparison(captures, output):
    output = runner.report.private_output(output)
    captures = captures.resolve()
    require(not output.is_relative_to(captures) and not captures.is_relative_to(output), "output overlaps captures")
    require(not output.exists(), "output already exists")
    results = []
    for mission in (2, 3):
        try:
            results.append(compare(captures / f"mission_{mission}"))
        except (OSError, ValueError, KeyError, TypeError) as exc:
            results.append(dict(mission=mission, status="invalid or missing captures; needs live run", detail=str(exc), rows=[]))
    output.mkdir(parents=True)
    runner.report.write_json(output / "report.json", dict(status="needs live run", missions=results))
    lines = ["# Production and net cash observations", "", "Status: needs live run.", "",
             "Live objects include structures and virtual objects; they are not population.",
             "Growth measures net count change, not purchases. Cash changes include spending and income.", "",
             "| Mission | Tick | Side | Reference objects | Clone objects | Reference HP | Clone HP | Reference money | Clone money |",
             "| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for result in results:
        for row in result["rows"]:
            for side in SIDES:
                a, b = (row[name]["sides"][side] for name in ("reference", "clone"))
                values = [result["mission"], row["tick"], side, a["objects"], b["objects"], a["hp"], b["hp"], a["money"], b["money"]]
                lines.append("| " + " | ".join(map(str, values)) + " |")
    (output / "series.md").write_text("\n".join(lines) + "\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    run = commands.add_parser("run")
    run.add_argument("--game", type=Path, required=True)
    run.add_argument("--runtime", type=Path, required=True)
    run.add_argument("--engine", type=Path, required=True)
    run.add_argument("--timeout", type=int, default=600)
    run.add_argument("--every-tick", action="store_true", help="locate net cash steps exactly; retains 1600 frames per process")
    run.add_argument("--output", type=Path, required=True)
    review = commands.add_parser("compare")
    review.add_argument("captures", type=Path)
    review.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "run":
            require(args.timeout > 0, "timeout must be positive")
            for key in ("game", "runtime", "engine"):
                setattr(args, key, getattr(args, key).resolve())
            args.commands = args.script = None
            collect(args)
        else:
            write_comparison(args.captures, args.output)
        print("needs live run")
        return 1
    except (OSError, ValueError) as exc:
        parser.exit(2, f"production measurement failed: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
