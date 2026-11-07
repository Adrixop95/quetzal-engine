#!/usr/bin/env python3
"""Shared L0 bundle contract. No private data or executable knowledge is embedded here."""
import json
import re

FIELDS = {"type", "owner", "x", "y", "hp", "orders"}
VIEW = {"camera", "resolution", "side", "selection", "panel", "cursor"}
SAMPLE = {"tick", "objects", "money", "queues", "view", "frame", "unavailable"}


def validate_diagnostics(row):
    """Reference-only probes are evidence for an adapter audit, never comparable state."""
    if "diagnostics" not in row:
        return
    d = row["diagnostics"]
    keys = {"pool", "orders", "queues", "camera", "viewport", "local_side_selector", "frame_phase"}
    require(isinstance(d, dict) and set(d) in (keys, keys | {"mission_seed"}), "invalid diagnostics")
    if "mission_seed" in d:
        require(integer(d["mission_seed"]) and d["mission_seed"] <= 0xffffffff, "invalid mission seed")
    p = d["pool"]
    require(isinstance(p, dict) and set(p) == {"capacity", "allocated", "free_slot_filtered",
                                             "inactive_object_filtered", "live_objects"}
            and all(integer(v) for v in p.values())
            and p["capacity"] == p["allocated"] + p["free_slot_filtered"]
            and p["allocated"] == p["inactive_object_filtered"] + p["live_objects"]
            and p["live_objects"] == len(row["objects"]), "invalid pool accounting")
    require(isinstance(d["orders"], list) and len(d["orders"]) == p["allocated"]
            and isinstance(d["queues"], list), "invalid raw order/queue probes")
    for probe in ("orders", "queues"):
        failed = False
        for record in d[probe]:
            require(isinstance(record, dict), "invalid diagnostic record")
            if "unavailable" in record:
                failed = True
                require(isinstance(record["unavailable"], str) and record["unavailable"]
                        and isinstance(record.get("raw"), dict) and record["raw"],
                        "unavailable probe needs a reason and raw values")
                if probe == "queues":
                    require("queued_handles" not in record, "failed queue probe has usable handles")
        require((f"diagnostics.{probe}" in row["unavailable"]) == failed,
                "inaccurate diagnostic availability")


def require(ok, message):
    if not ok:
        raise ValueError(message)


def integer(value, minimum=0):
    return type(value) is int and value >= minimum


def json_object(text):
    def unique(pairs):
        obj = {}
        for key, value in pairs:
            require(key not in obj, "duplicate JSON key")
            obj[key] = value
        return obj
    value = json.loads(text, object_pairs_hook=unique,
                       parse_constant=lambda _: require(False, "non-finite JSON number"))
    require(isinstance(value, dict), "expected JSON object")
    return value


def validate_view(v):
    require(isinstance(v, dict) and set(v) == VIEW, "invalid view contract")
    require(isinstance(v["camera"], list) and len(v["camera"]) == 3
            and all(integer(n) and n <= 2147483647 for n in v["camera"][:2]) and type(v["camera"][2]) is int
            and v["camera"][2] == 1, "camera requires integer top-left pixels and zoom 1")
    require(v["resolution"] == [800, 600] and all(type(n) is int for n in v["resolution"])
            and integer(v["side"]) and v["side"] < 4 and v["selection"] == []
            and v["panel"] is True and v["cursor"] == [400, 300]
            and all(type(n) is int for n in v["cursor"]), "unsupported fixed UI state")


def validate_scenario(s):
    require(isinstance(s, dict) and set(s) == {"schema", "id", "layer", "map", "setup", "input", "observe", "view", "tolerance", "evidence"},
            "unexpected shared L0 scenario keys")
    require(type(s["schema"]) is int and s["schema"] == 2 and s["layer"] == "L0", "unsupported shared scenario")
    require(isinstance(s["id"], str) and re.fullmatch(r"[a-z0-9][a-z0-9_-]*", s["id"]), "invalid scenario ID")
    require(isinstance(s["map"], dict) and s["map"].keys() == {"kind", "mission"} and s["map"]["kind"] == "campaign"
            and integer(s["map"]["mission"], 1) and s["map"]["mission"] <= 15, "invalid campaign")
    require(s["setup"] == {} and s["input"] == [], "shared L0 currently supports mission initialization and no commands")
    o = s["observe"]
    require(isinstance(o, dict) and set(o) == {"ticks", "contract"} and o["contract"] == "shared-l0-v2", "invalid observation contract")
    ticks = o["ticks"]
    require(isinstance(ticks, list) and ticks and all(integer(t, 1) and t <= 2147483647 for t in ticks)
            and ticks == sorted(set(ticks)) and ticks[0] == 1 and len(ticks) <= 100000, "observation ticks must be positive, unique and ordered")
    validate_view(s["view"])
    require(s["tolerance"] == {"exact": True} and s["tolerance"]["exact"] is True and s["evidence"] == "P",
            "shared L0 needs exact comparison and provisional evidence")
    return s


def write_schedule(s, path):
    """Small transport consumed by both native observers; JSON remains the source spec."""
    validate_scenario(s)
    v = s["view"]
    path.write_text("L0 2 %d %d %d\n" % (v["camera"][0], v["camera"][1], v["side"])
                    + "".join(f"{tick}\n" for tick in s["observe"]["ticks"]))


def bundle(s, native, runner, destination):
    """Assign identities once by initial owner/type/position, then follow native handles.

    Pool addresses and engine vector indices are never cross-program identities.
    Ambiguous initial anchors are refused; no nearest-neighbour matching is used.
    """
    validate_scenario(s)
    rows = [json_object(line) for line in native.read_text().splitlines()]
    require(rows and rows[0] == {"schema": 2, "phase": "before_commands", "position_unit": "pixel/256"},
            "invalid native observer header")
    samples = rows[1:]
    require([r.get("tick") for r in samples] == s["observe"]["ticks"], "native observer schedule differs")
    require(samples and all(set(r) in (SAMPLE, SAMPLE | {"diagnostics"})
                            and isinstance(r["objects"], list) for r in samples), "invalid native sample schema")
    require(samples[0]["objects"], "empty initial campaign observation")
    for row in samples:
        for obj in row["objects"]:
            require(isinstance(obj, dict) and set(obj) == FIELDS | {"handle"}
                    and integer(obj["handle"], 1)
                    and all(type(obj[f]) is int for f in FIELDS - {"orders"}), "invalid native object")
    identities = {}
    anchors = set()
    for obj in samples[0]["objects"]:
        anchor = tuple(obj[f] for f in ("owner", "type", "x", "y"))
        require(anchor not in anchors, "ambiguous initial identity anchor")
        anchors.add(anchor)
        require(obj["handle"] not in identities, "duplicate native object handle")
        identities[obj["handle"]] = "initial:" + ":".join(map(str, anchor))
    retired = set()
    previous = set(identities)
    for row in samples:
        objects = {}
        live = {obj["handle"] for obj in row["objects"]}
        require(not retired & live, "reused native handle needs a creation generation")
        retired.update(previous - live)
        previous = live
        for obj in row["objects"]:
            require(set(obj) == FIELDS | {"handle"}, "invalid native object fields")
            handle = obj["handle"]
            require(handle in identities, "spawn identity needs an audited creation observer")
            identity = identities[handle]
            require(identity not in objects, "duplicate native object handle")
            objects[identity] = {key: obj[key] for key in sorted(FIELDS)}
        for obj in objects.values():
            orders = obj["orders"]
            if isinstance(orders, dict) and "target" in orders:
                target = orders["target"]
                require(type(target) is int and (target == 0 or target in identities), "unknown order target")
                orders["target"] = identities[target] if target else None
        row["objects"] = objects
        # A requested view is not proof that the guest rendered it.
        require(isinstance(row["unavailable"], list), "invalid native unavailable fields")
        require(first_difference(row["view"], s["view"]) is None or row["view"] is None and "view" in row["unavailable"],
                "native camera/UI observation differs from requested view")
    out = dict(schema=2, contract=s["observe"]["contract"], mission=s["map"]["mission"],
               phase="before_commands", position_unit="pixel/256", view=s["view"],
               ticks=s["observe"]["ticks"], samples=samples, runner=runner)
    validate_bundle(out, destination.parent)
    destination.write_text(json.dumps(out, indent=2, sort_keys=True) + "\n")
    return out


def validate_bundle(b, directory):
    require(isinstance(b, dict) and set(b) == {"schema", "contract", "mission", "phase", "position_unit", "view", "ticks", "samples", "runner"}
            and type(b["schema"]) is int and b["schema"] == 2 and b["contract"] == "shared-l0-v2"
            and b["phase"] == "before_commands" and b["position_unit"] == "pixel/256"
            and b["runner"] in ("reference", "clone"), "invalid L0 bundle header")
    require(integer(b["mission"], 1) and b["mission"] <= 15, "invalid bundle mission")
    validate_view(b["view"])
    require(isinstance(b["ticks"], list) and b["ticks"] and all(integer(t, 1) and t <= 2147483647 for t in b["ticks"])
            and b["ticks"] == sorted(set(b["ticks"])) and b["ticks"][0] == 1, "invalid bundle schedule")
    require(isinstance(b["samples"], list) and all(isinstance(r, dict) and integer(r.get("tick"), 1) for r in b["samples"])
            and [r["tick"] for r in b["samples"]] == b["ticks"], "unaligned bundle schedule")
    for r in b["samples"]:
        require(set(r) in (SAMPLE, SAMPLE | {"diagnostics"}), "invalid sample keys")
        validate_diagnostics(r)
        require(isinstance(r["unavailable"], list) and all(isinstance(f, str) for f in r["unavailable"]), "invalid unavailable fields")
        require(first_difference(r["view"], b["view"]) is None or r["view"] is None and "view" in r["unavailable"],
                "sample camera/UI differs")
        require(isinstance(r["objects"], dict), "invalid objects")
        for identity, obj in r["objects"].items():
            require(isinstance(identity, str) and identity and isinstance(obj, dict) and set(obj) == FIELDS,
                    "invalid object schema")
            require(all(type(obj[f]) is int for f in FIELDS - {"orders"}), "object scalars must be integers")
            orders = obj["orders"]
            if orders is None:
                require("orders" in r["unavailable"], "missing orders are not declared")
            else:
                require(isinstance(orders, dict) and set(orders) == {"kind", "target", "x", "y"}
                        and orders["kind"] in ("idle", "move", "attack", "attack_move", "follow")
                        and (orders["target"] is None or isinstance(orders["target"], str))
                        and type(orders["x"]) is int and type(orders["y"]) is int, "invalid orders")
        require(isinstance(r["money"], dict) and set(r["money"]) == {"0", "1", "2", "3"}
                and all(type(v) is int for v in r["money"].values()), "invalid money")
        if r["queues"] is None:
            require("queues" in r["unavailable"], "missing queues are not declared")
        else:
            require(isinstance(r["queues"], dict) and set(r["queues"]) == set(r["money"]), "invalid queue sides")
            for queue in r["queues"].values():
                require(isinstance(queue, list), "invalid queue")
                for item in queue:
                    require(isinstance(item, dict) and set(item) == {"type", "town", "remaining", "total", "paid", "started", "paused"}
                            and all(type(item[k]) is int for k in ("type", "town", "remaining", "total", "paid"))
                            and type(item["started"]) is bool and type(item["paused"]) is bool, "invalid queue item")
        frame = r["frame"]
        if frame is None:
            require("frame" in r["unavailable"], "missing frame is not declared")
        else:
            require(isinstance(frame, str) and frame, "invalid frame path")
            path = (directory / frame).resolve()
            require(path.is_relative_to(directory.resolve()) and path.is_file(), "frame missing or outside bundle")
            # The existing visual reader validates dimensions and opaque RGB pixels.
            require(path.read_bytes().startswith(b"P6\n800 600\n255\n"), "invalid frame format/resolution")
            require(path.stat().st_size == len(b"P6\n800 600\n255\n") + 800 * 600 * 3, "truncated frame")
    return b


def first_difference(a, b, path=""):
    if type(a) is not type(b):
        return path, a, b
    if isinstance(a, dict):
        for key in sorted(set(a) | set(b)):
            p = f"{path}.{key}" if path else key
            if key not in a or key not in b:
                return p, a.get(key), b.get(key)
            d = first_difference(a[key], b[key], p)
            if d:
                return d
    elif isinstance(a, list):
        if len(a) != len(b):
            return path + ".length", len(a), len(b)
        for i, (x, y) in enumerate(zip(a, b)):
            d = first_difference(x, y, f"{path}[{i}]")
            if d:
                return d
    elif a != b:
        return path, a, b
    return None


def compare(reference, clone):
    a = validate_bundle(json_object(reference.read_text()), reference.parent)
    b = validate_bundle(json_object(clone.read_text()), clone.parent)
    require(a["runner"] == "reference" and b["runner"] == "clone", "wrong runner provenance")
    require(all(a[k] == b[k] for k in ("schema", "contract", "mission", "phase", "position_unit", "view", "ticks")),
            "unaligned bundle contract")
    first = None
    blocked = None
    notices = []
    for x, y in zip(a["samples"], b["samples"]):
        pool = x.get("diagnostics", {}).get("pool", {})
        for category in ("free_slot_filtered", "inactive_object_filtered"):
            if pool.get(category):
                notices.append(dict(category=category, tick=x["tick"], count=pool[category]))
        missing = set(x["unavailable"] + y["unavailable"])
        if missing and blocked is None:
            blocked = dict(category="blocked_observation_contract", tick=x["tick"], object_id=None,
                           field=",".join(sorted(missing)))
        for identity in sorted(set(x["objects"]) | set(y["objects"])):
            left, right = x["objects"].get(identity), y["objects"].get(identity)
            excluded = {"orders"} if "orders" in missing else set()
            if "position_fraction" in missing:
                excluded.update(("x", "y"))
            if left is not None:
                left = {k: v for k, v in left.items() if k not in excluded}
            if right is not None:
                right = {k: v for k, v in right.items() if k not in excluded}
            d = first_difference(left, right)
            if d and first is None:
                first = dict(category="field_difference", tick=x["tick"], object_id=identity,
                             field=d[0] or "presence", reference=d[1], engine=d[2])
        for field in ("money", "queues"):
            if field in missing:
                continue
            d = first_difference(x[field], y[field], field)
            if d and first is None:
                first = dict(category="field_difference", tick=x["tick"], object_id=None,
                             field=d[0], reference=d[1], engine=d[2])
        if not missing and first is None and (reference.parent / x["frame"]).read_bytes() != (clone.parent / y["frame"]).read_bytes():
            first = dict(category="frame_difference", tick=x["tick"], object_id=None, field="frame")
    result = dict(blocked, first_known_difference=first) if blocked else first or dict(
        category="equal_observed_fields", through_tick=a["ticks"][-1])
    if notices:
        result["notices"] = notices
    return result


def scalar_counts(reference, clone):
    """Count comparable scalar fields only; missing domains never become matches."""
    fields = ("type", "owner", "x", "y", "hp")
    rows = []
    for left, right in zip(reference["samples"], clone["samples"]):
        common = set(left["objects"]) & set(right["objects"])
        differences = {field: sum(left["objects"][i][field] != right["objects"][i][field]
                                  for i in common) for field in fields}
        money = sum(left["money"][side] != right["money"][side] for side in left["money"])
        rows.append(dict(tick=left["tick"], objects_compared=len(common),
                         reference_only=len(set(left["objects"]) - common),
                         clone_only=len(set(right["objects"]) - common),
                         fields_compared=len(common) * len(fields) + len(left["money"]),
                         divergences=sum(differences.values()) + money,
                         hp_divergences=differences["hp"],
                         position_divergences=differences["x"] + differences["y"]))
    return rows


def diagnostic_counts(bundle):
    """Owner counts for live reruns; a failed queue is unknown, never empty."""
    result = []
    for sample in bundle["samples"]:
        diagnostics = sample.get("diagnostics")
        if diagnostics is None:
            continue
        pool_failed = any("owner" not in r for r in diagnostics["queues"])
        for owner in (0, 1):
            orders = [r for r in diagnostics["orders"] if r.get("owner") == owner and r.get("alive")]
            queues = [r for r in diagnostics["queues"] if r.get("owner") == owner]
            failed = pool_failed or any("unavailable" in r for r in queues)
            result.append(dict(tick=sample["tick"], owner=owner,
                               objects=sum(o["owner"] == owner for o in sample["objects"].values()),
                               decoded_unit_states=sum("execution_kind" in r and r["execution_kind"] != "unknown"
                                                       for r in orders),
                               towns=None if pool_failed else len(queues),
                               queue_entries=None if failed else sum(len(r["queued_handles"]) for r in queues)))
    return result


def review_repeats(reference, clone, repeat_reference, repeat_clone):
    """Retain leads only after each program reproduces its entire observation stream."""
    bundles = []
    for name, first, second in (("reference", reference, repeat_reference), ("clone", clone, repeat_clone)):
        a = validate_bundle(json_object(first.read_text()), first.parent)
        b = validate_bundle(json_object(second.read_text()), second.parent)
        require(a["runner"] == b["runner"] == name, "wrong repeat provenance")
        d = first_difference(a, b)
        if d:
            return dict(category="run_nondeterminism", runner=name, field=d[0], status="needs live run")
        for sample in a["samples"]:
            if sample["frame"] and (first.parent / sample["frame"]).read_bytes() != (second.parent / sample["frame"]).read_bytes():
                return dict(category="run_nondeterminism", runner=name, tick=sample["tick"], field="frame", status="needs live run")
        bundles.append(a)
    verdict = compare(reference, clone)
    a, b = bundles
    initial_a, initial_b = a["samples"][0]["objects"], b["samples"][0]["objects"]
    common = set(initial_a) & set(initial_b)
    hp = {}
    for identity in sorted(common):
        left, right = initial_a[identity], initial_b[identity]
        if left["hp"] != right["hp"]:
            key = left["type"], left["hp"], right["hp"]
            hp.setdefault(key, []).append(identity)
    movement = []
    for x, y in zip(a["samples"], b["samples"]):
        row = dict(tick=x["tick"])
        for name, sample, initial in (("reference", x, initial_a), ("clone", y, initial_b)):
            row[name] = [identity for identity in sorted(common & set(sample["objects"]))
                         if any(sample["objects"][identity][axis] != initial[identity][axis] for axis in ("x", "y"))]
        movement.append(row)
    return dict(category="repeated_observations", status="needs live run", comparison=verdict,
                initial_counts=dict(reference=len(initial_a), clone=len(initial_b), common=len(common)),
                scalar_scope=["type", "owner", "x", "y", "hp", "money"],
                scalar_counts=scalar_counts(a, b), reference_diagnostic_counts=diagnostic_counts(a),
                hp_candidates=[dict(type=k[0], reference=k[1], engine=k[2], object_ids=v, confirmed=False)
                               for k, v in sorted(hp.items())], movement=movement,
                limitations=["HP candidates require matched difficulty/map settings and completed observation contracts.",
                             "Movement counts do not distinguish script/AI orders from phase or movement-rule differences."])


def main(argv=None):
    import argparse
    from pathlib import Path
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("clone", type=Path)
    parser.add_argument("--repeat-reference", type=Path)
    parser.add_argument("--repeat-clone", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        private = (Path(__file__).resolve().parents[1] / "work/parity").resolve()
        require(output.is_relative_to(private) and output != private, "comparison output must stay in work/parity")
        require(not output.exists(), "comparison output already exists")
        require(bool(args.repeat_reference) == bool(args.repeat_clone), "both repeated bundles are required")
        verdict = (review_repeats(args.reference, args.clone, args.repeat_reference, args.repeat_clone)
                   if args.repeat_reference else compare(args.reference, args.clone))
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(verdict, indent=2, sort_keys=True) + "\n")
        print(json.dumps(verdict, sort_keys=True))
        return int(verdict["category"] != "equal_observed_fields")
    except (OSError, ValueError) as exc:
        parser.exit(2, f"parity_l0: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
