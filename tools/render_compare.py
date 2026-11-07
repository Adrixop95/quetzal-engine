#!/usr/bin/env python3
"""V13 repeated shared-runner frames, structural masks and before/after L4 metrics."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import parity_l0 as l0
import parity_visual as visual
from parity_report import private_output, write_json

SCENES = ("aircraft-altitude", "transport-initiation", "turn", "steering", "hop",
          "destruction", "attack-death", "overlap", "fog", "minimap")
HEADER = ("schema", "contract", "mission", "phase", "position_unit", "view", "ticks")
require = visual.require


def local_path(plan, name):
    require(isinstance(name, str) and name, "missing private input path")
    path = (plan.parent / name).resolve()
    require(path.is_relative_to(plan.parent.resolve()), "input path escapes plan directory")
    return path


def read_run(plan, entry, runner):
    require(isinstance(entry, dict) and set(entry) == {"bundle", "masks"}, "invalid run entry")
    path = local_path(plan, entry["bundle"])
    bundle = l0.validate_bundle(visual.load(path), path.parent)
    require(bundle["runner"] == runner, "wrong runner provenance")
    masks = entry["masks"]
    require(isinstance(masks, dict) and set(masks) == {str(t) for t in bundle["ticks"]},
            "masks must cover the entire shared schedule")
    frames, decoded_masks = {}, {}
    for row in bundle["samples"]:
        tick = row["tick"]
        frames[tick] = visual.asset(path, row["frame"]) if row["frame"] else None
        require(isinstance(masks[str(tick)], dict) and set(masks[str(tick)]) == set(visual.REGIONS),
                "all six structural masks required")
        decoded_masks[tick] = {}
        for region, name in masks[str(tick)].items():
            mask = visual.image_file(local_path(plan, name))
            require(mask[:3] == (800, 600, 1) and set(mask[3]) <= {0, 255} and 255 in mask[3],
                    "invalid/empty structural mask")
            decoded_masks[tick][region] = mask[3]
    return dict(path=path, bundle=bundle, frames=frames, masks=decoded_masks)


def stable(first, second):
    # Raw address diagnostics and file names are not normalized observations.
    for a, b in zip(first["bundle"]["samples"], second["bundle"]["samples"], strict=True):
        tick = a["tick"]
        for field in ("objects", "money", "queues", "view", "unavailable"):
            if not visual.identical(a[field], b[field]):
                return dict(tick=tick, field=field)
        if first["frames"][tick] != second["frames"][tick]:
            return dict(tick=tick, field="frame")
        if first["masks"][tick] != second["masks"][tick]:
            return dict(tick=tick, field="masks")
    return None


def measure(reference, clone, ticks, thresholds):
    rows, blocked = [], []
    a = {r["tick"]: r for r in reference["bundle"]["samples"]}
    b = {r["tick"]: r for r in clone["bundle"]["samples"]}
    for tick in ticks:
        missing = sorted(set(a[tick]["unavailable"] + b[tick]["unavailable"]))
        differences = [field for field in ("objects", "money", "queues", "view")
                       if not visual.identical(a[tick][field], b[tick][field])]
        if missing or differences:
            blocked.append(dict(tick=tick, unavailable=missing, state_differences=differences))
        ap, bp = reference["frames"][tick], clone["frames"][tick]
        metrics = None if ap is None or bp is None else {
            region: visual.region_metrics(ap[3], bp[3], reference["masks"][tick][region],
                                          clone["masks"][tick][region], thresholds[region])
            for region in visual.REGIONS}
        rows.append(dict(tick=tick, regions=metrics))
    return dict(category="blocked_observation_contract" if blocked else
                "different" if any(not m["passed"] for row in rows for m in row["regions"].values())
                else "equal_observed_regions", blockers=blocked, samples=rows,
                diagnostic_only=bool(blocked))


def compare(plan):
    spec = visual.load(plan)
    require(set(spec) == {"schema", "scene", "ticks", "regions", "reference", "before", "after"}
            and type(spec["schema"]) is int and spec["schema"] == 1 and spec["scene"] in SCENES,
            "invalid V13 plan")
    ticks = spec["ticks"]
    require(isinstance(ticks, list) and ticks and all(type(t) is int and t > 0 for t in ticks)
            and ticks == sorted(set(ticks)), "invalid scene ticks")
    visual.validate_regions(spec["regions"])
    runs = {}
    for label in ("reference", "before", "after"):
        entries = spec[label]
        if label == "after" and entries is None:
            continue
        require(isinstance(entries, list) and len(entries) == 2, "two independent captures required per version")
        runs[label] = [read_run(plan, entry, "reference" if label == "reference" else "clone")
                       for entry in entries]
    paths = [run["path"] for pair in runs.values() for run in pair]
    require(len(paths) == len(set(paths)), "capture paths must be distinct")
    reference = runs["reference"][0]
    header = {k: reference["bundle"][k] for k in HEADER}
    for pair in runs.values():
        for run in pair:
            require(visual.identical(header, {k: run["bundle"][k] for k in HEADER}), "unaligned shared schedules/views")
    require(set(ticks) <= set(header["ticks"]), "scene tick absent from shared schedule")
    # All bundles, frames and masks have been validated before any verdict.
    result = dict(scene=spec["scene"], evidence="SYN/P", status="needs live run",
                  before=None, after=None, eligible_for_engine_fix=False)
    for label, pair in runs.items():
        difference = stable(*pair)
        if difference:
            return dict(result, category="run_nondeterminism", runner=label, first=difference)
    for label in ("before", "after"):
        if label in runs:
            result[label] = measure(reference, runs[label][0], ticks, spec["regions"])
    result["eligible_for_engine_fix"] = result["before"]["category"] == "different"
    result["category"] = ("blocked_observation_contract" if any(
        result[label] and result[label]["diagnostic_only"] for label in ("before", "after"))
        else "repeated_scene_comparison")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("plan", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        require(not output.exists(), "use a fresh output file")
        output.parent.mkdir(parents=True, exist_ok=True)
        try:
            result = compare(args.plan.resolve())
        except (ValueError, OSError, KeyError, TypeError, visual.struct.error, visual.zlib.error) as exc:
            write_json(output, dict(category="invalid_observation", reason=str(exc), status="needs live run"))
            return 2
        write_json(output, result)
        return 0 if (result["category"] == "repeated_scene_comparison" and result["after"]
                     and result["after"]["category"] == "equal_observed_regions") else 1
    except (ValueError, OSError) as exc:
        parser.exit(2, f"render_compare: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
