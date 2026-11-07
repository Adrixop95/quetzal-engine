#!/usr/bin/env python3
"""Prepare dense passive probes and summarize the initial Marine cohort privately."""
import argparse
from collections import Counter
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from parity_l0 import json_object, require, validate_scenario  # noqa: E402

TICKS = list(range(1, 25))


def prepare(directory):
    directory.mkdir(parents=True, exist_ok=False)
    for mission in (1, 2, 3):
        suffix = "" if mission == 1 else f"_mission_{mission}"
        scenario = json_object((ROOT / f"tests/parity/private_l0{suffix}.json").read_text())
        scenario["id"] = f"idle_walk_mission_{mission}"
        scenario["observe"]["ticks"] = TICKS
        validate_scenario(scenario)
        (directory / f"{scenario['id']}.json").write_text(json.dumps(scenario, indent=2) + "\n")


def summarize(path, expected_ticks=None):
    expected = TICKS if expected_ticks is None else expected_ticks
    rows = [json_object(line) for line in path.read_text().splitlines()]
    require(rows and rows[0] == dict(schema=2, phase="before_commands", position_unit="pixel/256"),
            "invalid native observer header")
    rows = rows[1:]
    ticks = [row.get("tick") for row in rows]
    require(all(type(t) is int for t in ticks) and ticks == sorted(set(ticks))
            and (ticks[:24] == TICKS if expected_ticks is None else ticks == expected),
            "incomplete or wrong measurement schedule")
    cohort = {}
    identities = set()
    walking_counts = []
    new_counts = []
    seeds = []
    for row in rows[:len(expected)]:
        objects = {}
        for obj in row["objects"]:
            handle = obj["handle"]
            require(type(handle) is int and handle > 0 and handle not in objects, "duplicate or invalid object handle")
            objects[handle] = obj
        orders = {}
        for order in row["diagnostics"]["orders"]:
            handle = order["handle"]
            require(handle not in orders, "duplicate order handle")
            orders[handle] = order
        require(set(objects) <= set(orders), "missing live order diagnostic")
        if row["tick"] == 1:
            for handle, obj in objects.items():
                order = orders[handle]
                require("unavailable" not in order, "unavailable initial order probe")
                if order["kind"] != 0:
                    continue
                identity = tuple(obj[key] for key in ("owner", "type", "x", "y"))
                require(all(type(v) is int for v in identity) and identity not in identities,
                        "ambiguous initial cohort identity")
                identities.add(identity)
                cohort[handle] = dict(identity=list(identity), samples=[])
            require(cohort, "empty initial Marine cohort")
        require(set(cohort) <= set(objects), "initial cohort member disappeared; lifecycle audit required")
        walking = 0
        for handle, unit in cohort.items():
            order, obj = orders[handle], objects[handle]
            require("unavailable" not in order and order["kind"] == 0, "unavailable Marine order probe")
            require([obj["owner"], obj["type"]] == unit["identity"][:2], "cohort identity changed")
            sample = {key: order[key] for key in ("unit_state", "tactic_state", "tactic_mode", "anchor_tile",
                                                 "tile", "step_destination", "step_velocity")}
            for key in ("anchor_tile", "tile", "step_destination", "step_velocity"):
                require(isinstance(sample[key], list) and len(sample[key]) == 2
                        and all(type(n) is int for n in sample[key]), "invalid movement vector")
            for key in ("unit_state", "tactic_state", "tactic_mode"):
                require(type(sample[key]) is int, "invalid movement state")
            for key in ("x", "y"):
                require(type(obj[key]) is int, "invalid cohort position")
            sample.update(tick=row["tick"], position=[obj["x"], obj["y"]])
            # Optional new probes are retained, including explicit probe failures.
            for key in ("member", "formation_marked", "idle_animation_timer", "descriptor_speed_px"):
                if key in order:
                    sample[key] = order[key]
            if "member" in sample and "unavailable" in sample["member"]:
                sample["member"] = {"unavailable": sample["member"]["unavailable"]}
            unit["samples"].append(sample)
            walking += order["unit_state"] == 2
        walking_counts.append(walking)
        new_counts.append(sum(h not in cohort and orders[h].get("kind") == 0 for h in objects))
        seeds.append(row["diagnostics"].get("mission_seed"))
    first = Counter()
    for unit in cohort.values():
        unit["first_observed_walk_tick"] = next((s["tick"] for s in unit["samples"] if s["unit_state"] == 2), None)
        if unit["first_observed_walk_tick"] is not None:
            first[unit["first_observed_walk_tick"]] += 1
    return dict(schema=1, scope="initial_marine_cohort", ticks=expected, mission_seed=seeds,
                walking_counts=walking_counts, additional_marine_counts=new_counts,
                first_observed_walk_counts={str(t): first[t] for t in sorted(first)},
                cohort=sorted(cohort.values(), key=lambda unit: unit["identity"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    plan = commands.add_parser("prepare")
    plan.add_argument("--output", type=Path, required=True)
    summary = commands.add_parser("summarize")
    summary.add_argument("reference", type=Path)
    summary.add_argument("repeat", type=Path)
    summary.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "prepare":
            prepare(args.output)
            return 0
        first, repeat = summarize(args.reference), summarize(args.repeat)
        result = dict(repeatable=first == repeat, reference=first, repeat=repeat)
        with args.output.open("x") as output:
            output.write(json.dumps(result, indent=2, sort_keys=True) + "\n")
        return 0 if result["repeatable"] else 1
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, f"idle walk measurement failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
