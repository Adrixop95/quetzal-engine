#!/usr/bin/env python3
"""L2 preparation and hash-first comparison using the shared native observer."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def shared():
    spec = importlib.util.spec_from_file_location("l2_shared_runner", ROOT / "tools/parity_run.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def compare(a, b, l0):
    """Hashes cover declared comparable simulation fields, never raw pools or frames.

    Both bundles must already have passed validate_bundle (including all frames).
    Reference-to-reference comparison uses the same projection as cross-program
    comparison; unavailable fields always prevent a complete parity verdict.
    """
    l0.require(all(a[k] == b[k] for k in ("schema", "contract", "mission", "phase", "position_unit", "view", "ticks")),
               "unaligned L2 observation contract")
    l0.require(a["ticks"] == list(range(1, a["ticks"][-1] + 1)), "L2 requires every tick")
    hashes, first, blocked = [], None, set()
    for x, y in zip(a["samples"], b["samples"], strict=True):
        missing = set(x["unavailable"] + y["unavailable"]) - {
            "view", "frame", "frame_phase", "diagnostics.orders", "diagnostics.queues"}
        l0.require(missing <= {"orders", "queues", "position_fraction"}, "unsupported L2 simulation availability")
        blocked.update(missing)

        def project(row):
            fields = l0.FIELDS - ({"orders"} if "orders" in missing else set())
            if "position_fraction" in missing:
                fields -= {"x", "y"}
            return dict(objects={i: {f: o[f] for f in sorted(fields)} for i, o in row["objects"].items()},
                        money=row["money"], **({} if "queues" in missing else dict(queues=row["queues"])))

        left, right = project(x), project(y)
        u, v = digest(left), digest(right)
        hashes.append(dict(tick=x["tick"], reference=u, engine=v, excluded=sorted(missing)))
        if u != v and first is None:
            for identity in sorted(set(left["objects"]) | set(right["objects"])):
                d = l0.first_difference(left["objects"].get(identity), right["objects"].get(identity))
                if d:
                    first = dict(category="field_difference", tick=x["tick"], object_id=identity,
                                 field=d[0] or "presence", reference=d[1], engine=d[2])
                    break
            if first is None:
                d = l0.first_difference(left, right)
                first = dict(category="field_difference", tick=x["tick"], object_id=None,
                             field=d[0], reference=d[1], engine=d[2])
    reached = first["tick"] - 1 if first else a["ticks"][-1]
    verdict = first or dict(category="equal_observed_fields", through_tick=reached)
    if blocked:
        verdict = dict(category="blocked_observation_contract", first_known_difference=first,
                       unavailable=sorted(blocked))
    return dict(verdict, observed_equal_through_tick=reached,
                complete_observed_contract=not blocked and first is None), hashes


def mission(args, number, directory, harness):
    report, l0 = harness.report, harness.l0
    directory.mkdir()
    s = harness.load_scenario(ROOT / "tests/parity/private_l0.json")
    s = copy.deepcopy(s)
    s["id"] = f"l2_mission_{number}"
    s["map"]["mission"] = number
    s["observe"]["ticks"] = list(range(1, args.ticks + 1))
    report.write_json(directory / "scenario.json", s)
    # Scripted input records packets once. Subsequent runs replay those exact
    # packets; wall-clock click timing is never treated as a tick schedule.
    navigation = (ROOT / "tests/scenarios/mission1_start.txt").read_text()
    navigation = navigation.replace("click 150 257", f"click 150 {257 + 20 * (number - 1)}")
    actions = (ROOT / "tests/scenarios/l2_commands.txt").read_text()
    script = directory / "record-input.txt"
    script.write_text(navigation + "\n" + actions)
    run_args = copy.copy(args)
    run_args.commands = None
    run_args.script = script
    record = directory / "record"
    record.mkdir()
    harness.shared_runner(s, run_args, record / "reference", "reference", record=True, normalize=False, state_only=True)
    source = record / "reference/commands.txt"
    rows = report.command_rows(source)
    # Record may continue past the declared observation horizon. Keep the source
    # intact and replay only the explicit prefix; no command is retimed.
    commands = directory / "commands.txt"
    commands.write_text("".join(line + "\n" for line, row in zip(source.read_text().splitlines(), rows, strict=True)
                                if row[0] <= args.ticks))
    report.write_json(directory / "inputs.json", dict(mission=number, ticks=args.ticks,
                      commands_sha256=hashlib.sha256(commands.read_bytes()).hexdigest(),
                      runtime_sha256=hashlib.sha256(args.runtime.read_bytes()).hexdigest(),
                      engine_sha256=hashlib.sha256(args.engine.read_bytes()).hexdigest(),
                      script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()))
    run_args.script = None
    runs = []
    for repetition in (1, 2):
        run = directory / f"replay-{repetition}"
        run.mkdir()
        harness.shared_runner(s, run_args, run / "reference", "reference", packets=commands, normalize=False, state_only=True)
        runs.append(run)
    pools = report.pool_differences(*(report.state_rows(r / "reference/state.txt", args.ticks) for r in runs))
    report.write_json(directory / "pool-differences.json", pools)
    # Preserve raw repeatability evidence even when spawn/handle mapping refuses
    # normalization. Identity failures must never be converted into equal state.
    bundles = []
    try:
        for run in runs:
            bundles.append(l0.bundle(s, run / "reference/observations.jsonl", "reference", run / "reference.json"))
    except ValueError as exc:
        if not any(reason in str(exc) for reason in (
                "spawn identity", "reused native handle", "ambiguous initial identity anchor")):
            raise
        if pools:
            return report.result("reference_nondeterminism", tick=pools[0]["tick"],
                                 field=f"pool[{pools[0]['pool']}]", detail=str(exc),
                                 raw_pool_differences=len(pools))
        return report.result("blocked_observation_contract", detail=str(exc),
                             raw_reference_repeatable=not pools, raw_pool_differences=len(pools))
    repeat, hashes = compare(*bundles, l0)
    report.write_json(directory / "reference-hashes.json", hashes)
    report.write_json(directory / "reference-repeat.json", repeat)
    if pools or repeat.get("first_known_difference") or repeat["category"] == "field_difference":
        first = repeat.get("first_known_difference") or (repeat if repeat["category"] == "field_difference" else None)
        return report.result("reference_nondeterminism", tick=first["tick"] if first else pools[0]["tick"],
                             object_id=first["object_id"] if first else None,
                             field=first["field"] if first else f"pool[{pools[0]['pool']}]",
                             detail=repeat, raw_pool_differences=len(pools))
    # A partial semantic repeat is evidence only for its declared projection.
    if not any(row[4] != 0x25 for row in report.command_rows(commands)):
        return report.result("blocked_input_coverage", detail="script recorded no command packets within the horizon")
    if not args.engine_adapter:
        return report.result("blocked_command_translation", detail="audited oracle-packet-to-clone adapter is missing",
                             raw_reference_repeatable=True, reference_observation=repeat)
    clone = directory / "clone"
    clone.mkdir()
    report.private_game(args.game, clone / "game")
    output = directory / "clone.json"
    report.execute([args.engine_adapter, "--engine", args.engine, "--game", clone / "game",
                    "--scenario", directory / "scenario.json", "--commands", commands, "--output", output],
                   clone, args.timeout, {"AZTEC_USER_DIR": str(clone / "profile")}, setup_network=False)
    observed = l0.validate_bundle(l0.json_object(output.read_text()), directory)
    l0.require(observed["runner"] == "clone", "wrong clone provenance")
    verdict, hashes = compare(bundles[0], observed, l0)
    report.write_json(directory / "state-hashes.json", hashes)
    return dict(verdict, raw_reference_repeatable=True)


def run(args):
    harness = shared()
    report = harness.report
    args.output = report.private_output(args.output)
    for path in (args.game, args.runtime, args.engine, args.engine_adapter):
        if path is not None:
            harness.require(not args.output.is_relative_to(path) and not path.is_relative_to(args.output),
                            "output overlaps an input path")
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    for number in (1, 2, 3):
        directory = args.output / f"m{number}"
        try:
            verdict = mission(args, number, directory, harness)
        except (OSError, ValueError, RuntimeError, StopIteration) as exc:
            verdict = report.result("experiment_failed", detail=str(exc))
        row = dict(id=f"m{number}", layer="L2", mission=number, fix_iterations=[], **verdict)
        directory.mkdir(exist_ok=True)
        report.write_json(directory / "diff.json", row)
        rows.append(row)
        report.write_json(args.output / "report.json", rows)
        print(f"mission {number}: {verdict['category']}")
    return int(any(r["category"] != "equal_observed_fields" for r in rows))
