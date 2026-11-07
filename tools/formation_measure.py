#!/usr/bin/env python3
"""Collect dense startup observations and report candidate formation endpoints privately."""
import argparse
from collections import Counter
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import idle_walk_measure as walk  # noqa: E402

SPEC = importlib.util.spec_from_file_location("formation_runner", ROOT / "tools/parity_run.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)
require = runner.require


def scenario(mission, last_tick):
    require(type(last_tick) is int and 60 <= last_tick <= 600, "horizon must be 60..600")
    suffix = "" if mission == 1 else f"_mission_{mission}"
    s = runner.load_scenario(ROOT / f"tests/parity/private_l0{suffix}.json")
    s["id"] = f"formation_mission_{mission}"
    s["observe"]["ticks"] = list(range(1, last_tick + 1))
    return runner.l0.validate_scenario(s)


def collect(args):
    output = runner.report.private_output(args.output)
    for path in (args.game, args.runtime, args.engine):
        require(not output.is_relative_to(path) and not path.is_relative_to(output), "output overlaps input")
    output.mkdir(parents=True, exist_ok=False)
    for mission in (1, 2, 3):
        directory = output / f"mission_{mission}"
        directory.mkdir()
        s = scenario(mission, args.last_tick)
        runner.report.write_json(directory / "scenario.json", s)
        outcomes = {}
        for name in ("reference", "clone"):
            for repeat in (1, 2):
                key = f"{name}-{repeat}"
                try:
                    runner.shared_runner(s, args, directory / key, name, normalize=False)
                    outcomes[key] = dict(status="completed")
                except (OSError, ValueError, RuntimeError) as error:
                    outcomes[key] = dict(status="failed", detail=str(error))
                runner.report.write_json(directory / "processes.json", outcomes)


def timeline(path, ticks):
    summary = walk.summarize(path, ticks)
    rows = [runner.l0.json_object(line) for line in path.read_text().splitlines()][1:]
    # Tactic pointers are process-local. Use the complete initial member set as
    # the group identity, never the anchor (different groups can share an anchor).
    identities = {obj["handle"]: [obj[k] for k in ("owner", "type", "x", "y")]
                  for obj in rows[0]["objects"]}
    groups = {}
    for order in rows[0]["diagnostics"]["orders"]:
        if order.get("kind") == 0:
            require(type(order.get("tactic")) is int and order["tactic"] > 0, "missing tactic identity")
            groups.setdefault(order["tactic"], []).append(identities[order["handle"]])
    groups = {handle: sorted(members) for handle, members in groups.items()}
    units = {tuple(unit["identity"]): unit for unit in summary["cohort"]}
    for row in rows:
        for order in row["diagnostics"]["orders"]:
            identity = identities.get(order["handle"])
            if identity is None or tuple(identity) not in units:
                continue
            require(order.get("tactic") in groups, "new tactic needs membership audit")
            units[tuple(identity)]["samples"][row["tick"] - 1]["group"] = groups[order["tactic"]]
    return summary


def endpoints(summary):
    results = []
    for unit in summary["cohort"]:
        samples = unit["samples"]
        last = samples[-1]
        final = last["position"]
        centered = all(n % (48 * 256) == 24 * 256 for n in final)
        # The final constant idle tail is an observation, not proof of a final
        # order destination. A walking/censored tail never receives an arrival.
        tail = len(samples)
        if centered:
            while tail and samples[tail - 1]["position"] == final and samples[tail - 1]["unit_state"] in (1, 5):
                tail -= 1
        arrival = samples[tail]["tick"] if tail < len(samples) else None
        tile = [n // (48 * 256) for n in final] if centered else None
        velocities = Counter(tuple(s["step_velocity"]) for s in samples if s["unit_state"] == 2)
        results.append(dict(identity=unit["identity"], group=samples[0]["group"],
                            first_walk_tick=unit["first_observed_walk_tick"],
                            first_displacement_tick=next((s["tick"] for s in samples
                                                         if s["position"] != samples[0]["position"]), None),
                            final_position=final, endpoint_tile=tile, observed_idle_tail_tick=arrival,
                            idle_tail_samples=len(samples) - tail,
                            endpoint_offset=[tile[i] - last["anchor_tile"][i] for i in (0, 1)] if tile else None,
                            anchor_changes=[dict(tick=b["tick"], tile=b["anchor_tile"])
                                            for a, b in zip(samples, samples[1:])
                                            if a["anchor_tile"] != b["anchor_tile"]],
                            walking_velocities=[dict(vector=list(v), samples=n) for v, n in sorted(velocities.items())]))
    return results


def clone_positions(path, reference):
    rows = [runner.l0.json_object(line) for line in path.read_text().splitlines()]
    require(rows.pop(0) == dict(schema=2, phase="before_commands", position_unit="pixel/256"),
            "invalid clone observer header")
    require([row.get("tick") for row in rows] == reference["ticks"], "incomplete clone schedule")
    initial = {}
    for obj in rows[0]["objects"]:
        identity = tuple(obj[k] for k in ("owner", "type", "x", "y"))
        require(identity not in initial, "ambiguous clone identity")
        initial[identity] = obj["handle"]
    positions = {}
    for unit in reference["cohort"]:
        identity = tuple(unit["identity"])
        require(identity in initial, "initial reference unit absent from clone")
        handle = initial[identity]
        samples = []
        for row in rows:
            matches = [o for o in row["objects"] if o["handle"] == handle]
            require(len(matches) == 1, "clone cohort disappeared or duplicate handle")
            obj = matches[0]
            require([obj["owner"], obj["type"]] == list(identity[:2]), "clone cohort identity changed")
            require(all(type(obj[k]) is int for k in ("x", "y")), "invalid clone position")
            samples.append([obj["x"], obj["y"]])
        positions[identity] = samples
    return positions


def compare(reference, repeat, last_tick, clone=None, clone_repeat=None):
    require(reference.resolve() != repeat.resolve(), "two separate reference files are required")
    ticks = list(range(1, last_tick + 1))
    a, b = timeline(reference, ticks), timeline(repeat, ticks)
    stable = a == b
    result = dict(status="needs live run", repeatable=stable,
                  scope="initial_marine_cohort; not whole-game parity",
                  reference=a, repeat=b, endpoints=endpoints(a) if stable else [])
    require((clone is None) == (clone_repeat is None), "provide both clone repetitions")
    if stable and clone is not None:
        require(clone.resolve() != clone_repeat.resolve(), "two separate clone files are required")
        c, d = clone_positions(clone, a), clone_positions(clone_repeat, a)
        result["clone_repeatable_positions"] = c == d
        if c == d:
            differences = []
            for unit in a["cohort"]:
                identity = tuple(unit["identity"])
                original = [s["position"] for s in unit["samples"]]
                candidate = c[identity]
                moved = lambda values: any(p != values[0] for p in values[1:])
                differences.append(dict(identity=list(identity),
                                        first_position_difference_tick=next((t for t, x, y in zip(ticks, original, candidate)
                                                                             if x != y), None),
                                        moved_reference=moved(original), moved_clone=moved(candidate),
                                        tick_60_reference=original[59], tick_60_clone=candidate[59],
                                        tick_60_equal=original[59] == candidate[59]))
            result["clone_comparison"] = differences
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    run = commands.add_parser("run")
    for name in ("game", "runtime", "engine"):
        run.add_argument(f"--{name}", type=lambda p: Path(p).resolve(), required=True)
    run.add_argument("--timeout", type=int, default=180)
    run.set_defaults(commands=None, script=None)
    review = commands.add_parser("compare")
    review.add_argument("reference", type=Path)
    review.add_argument("repeat", type=Path)
    review.add_argument("--clone", type=Path)
    review.add_argument("--clone-repeat", type=Path)
    for command in (run, review):
        command.add_argument("--last-tick", type=int, default=120)
        command.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        require(60 <= args.last_tick <= 600, "horizon must be 60..600")
        require(getattr(args, "timeout", 1) > 0, "timeout must be positive")
        if args.command == "run":
            collect(args)
        else:
            result = compare(args.reference, args.repeat, args.last_tick, args.clone, args.clone_repeat)
            output = runner.report.private_output(args.output)
            with output.open("x") as stream:
                stream.write(json.dumps(result, indent=2, sort_keys=True) + "\n")
        # Even repeated endpoints cannot establish the rule that assigns slots.
        return 1
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, f"formation measurement failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
