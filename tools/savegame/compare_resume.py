#!/usr/bin/env python3
"""Private original-save import comparison; never treats missing coverage as parity."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("parity_report", ROOT / "tools/parity_report.py")
report = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(report)
DOMAINS = {"units", "money", "fog", "clock", "orders", "queues", "research", "upgrades",
           "scripts", "timers", "diplomacy"}
BASE = {"units", "money", "fog", "clock"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value):
    return type(value) is int and -(1 << 63) <= value < (1 << 64)


def numeric_tree(value):
    if isinstance(value, dict):
        return all(isinstance(k, str) and numeric_tree(v) for k, v in value.items())
    if isinstance(value, list):
        return all(numeric_tree(v) for v in value)
    return integer(value)


def load_snapshot(path):
    obj = report.json_object(Path(path).read_text())
    require(set(obj) == {"schema", "phase", "elapsed_ticks", "saved_tick", "unsupported", "state", "save_sha256"},
            "unexpected or missing snapshot keys")
    require(type(obj["schema"]) is int and obj["schema"] == 1, "unsupported schema")
    require(obj["phase"] == "after_load_before_tick" and type(obj["elapsed_ticks"]) is int
            and obj["elapsed_ticks"] == 0, "observer must stop before the first continued tick")
    require(integer(obj["saved_tick"]) and 0 <= obj["saved_tick"] < 2**32, "invalid saved tick")
    digest = obj["save_sha256"]
    require(isinstance(digest, str) and len(digest) == 64
            and all(c in "0123456789abcdef" for c in digest), "invalid save digest")
    missing = obj["unsupported"]
    require(isinstance(missing, list) and all(isinstance(k, str) and k in DOMAINS for k in missing)
            and len(set(missing)) == len(missing), "invalid unsupported domains")
    state = obj["state"]
    require(isinstance(state, dict) and BASE <= state.keys() and set(state).isdisjoint(missing)
            and set(state) | set(missing) == DOMAINS and numeric_tree(state), "incomplete state coverage")
    # Version 1 defines normalized layouts only for BASE. Accepting arbitrary
    # numeric placeholders for the other domains would allow a false full match.
    require(set(state) == BASE and set(missing) == DOMAINS - BASE,
            "schema 1 has no audited layout for extended resume domains")
    require(integer(state["clock"]) and 0 <= state["clock"] < 7200, "invalid clock")
    money = state["money"]
    require(isinstance(money, list) and 1 <= len(money) <= 8 and all(integer(v) for v in money), "invalid money")
    units = state["units"]
    require(isinstance(units, dict) and len(units) <= 16384, "invalid units")
    for identity, unit in units.items():
        require(identity.isascii() and identity.isdecimal() and str(int(identity)) == identity
                and 0 < int(identity) < 2**32, "invalid original unit ID")
        require(isinstance(unit, dict) and set(unit) == {"type", "side", "town", "hp", "secondary_hp", "x256", "y256"}
                and all(integer(v) for v in unit.values()), "invalid unit fields")
        require(0 <= unit["type"] < 90 and -1 <= unit["side"] < len(money)
                and (unit["town"] == 0 or 0x80000000 < unit["town"] < 2**32), "invalid unit ownership or type")
    fog = state["fog"]
    require(isinstance(fog, list) and len(fog) == len(money), "invalid fog sides")
    for layer in fog:
        require(isinstance(layer, dict) and set(layer) == {"width", "height", "values"}, "invalid fog layer")
        w, h, values = layer["width"], layer["height"], layer["values"]
        require(integer(w) and integer(h) and 0 < w * h <= 16777216 and w > 0 and h > 0
                and isinstance(values, list) and len(values) == w * h
                and all(integer(v) and 0 <= v <= 255 for v in values), "invalid fog grid")
    return obj


def compare(left, right):
    a, b = load_snapshot(left), load_snapshot(right)
    require(a["save_sha256"] == b["save_sha256"] and a["saved_tick"] == b["saved_tick"],
            "different save or saved tick")
    common = sorted(a["state"].keys() & b["state"].keys())
    for domain in common:
        if a["state"][domain] != b["state"][domain]:
            return dict(status="mismatch", domain=domain, complete=False)
    missing = sorted(set(a["unsupported"]) | set(b["unsupported"]))
    return dict(status="incomplete", complete=False,
                compared=common, unsupported=missing)


def run(args):
    output = args.output.resolve()
    require(output.is_relative_to((ROOT / "work").resolve()), "private output must be under this worktree's work directory")
    require(not output.exists(), "use a fresh output directory")
    output.mkdir(parents=True)
    result_path = output / "result.json"
    if not args.reference_adapter:
        result = dict(status="needs live run", detail="audited reference save-load observer is missing", complete=False)
        report.write_json(result_path, result)
        return result
    try:
        source = args.save.resolve()
        data = source.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        private_save = output / "input.sav"
        private_save.write_bytes(data)
        for label, binary in (("reference", args.reference_adapter), ("clone", args.clone)):
            directory = output / label
            directory.mkdir()
            report.private_game(args.game.resolve(), directory / "game")
            target = directory / "snapshot.json"
            argv = [binary.resolve(), "--game", directory / "game", "--save", private_save, "--output", target]
            argv += ["--runtime", args.runtime.resolve()] if label == "reference" else ["--rules", args.rules.resolve()]
            report.execute(argv, directory, args.timeout,
                           dict(RT_NO_VIDEO="1", RT_NO_GRAB="1", RT_RESOLUTION="800x600"),
                           setup_network=label == "reference")
            require(hashlib.sha256(private_save.read_bytes()).hexdigest() == digest, "observer modified the input save")
            obj = report.json_object(target.read_text())
            require("save_sha256" not in obj, "save digest is supplied by the runner")
            obj["save_sha256"] = digest
            report.write_json(target, obj)
            load_snapshot(target)
        result = compare(output / "reference/snapshot.json", output / "clone/snapshot.json")
    except (OSError, ValueError, RuntimeError) as exc:
        result = dict(status="needs live run", detail=str(exc), complete=False)
    report.write_json(result_path, result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    diff = sub.add_parser("compare")
    diff.add_argument("reference", type=Path)
    diff.add_argument("clone", type=Path)
    live = sub.add_parser("run")
    for name in ("game", "save", "runtime", "clone", "rules", "output"):
        live.add_argument("--" + name, type=Path, required=True)
    live.add_argument("--reference-adapter", type=Path)
    live.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args()
    try:
        if args.action == "run":
            require(args.timeout > 0, "timeout must be positive")
            result = run(args)
        else:
            result = compare(args.reference, args.clone)
        print(json.dumps(result, sort_keys=True))
        return 1  # Schema 1 cannot establish complete resume coverage.
    except (OSError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
