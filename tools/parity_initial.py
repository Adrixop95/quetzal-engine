#!/usr/bin/env python3
"""Campaign startup tables from repeated shared L0 bundles; no live launcher."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("parity_run", ROOT / "tools/parity_run.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)
l0 = runner.l0
DOMAINS = ("objects", "type", "owner", "hp", "positions", "orders", "money", "tech", "population")


def scenario_id(mission):
    return "private_l0" if mission == 1 else f"private_l0_mission_{mission}"


def count_sample(a, b):
    """Count field instances, not inferred causes. Unknown domains stay null."""
    common = set(a["objects"]) & set(b["objects"])
    missing = set(a["unavailable"] + b["unavailable"])
    counts = {name: dict(compared=None, divergences=None) for name in DOMAINS}
    counts["objects"] = dict(compared=len(set(a["objects"]) | set(b["objects"])),
                             divergences=len(set(a["objects"]) ^ set(b["objects"])))
    for name, fields in (("type", ("type",)), ("owner", ("owner",)), ("hp", ("hp",)),
                         ("positions", ("x", "y")), ("orders", ("orders",))):
        if name in missing or name == "positions" and "position_fraction" in missing:
            continue
        counts[name] = dict(compared=len(common) * len(fields), divergences=sum(
            a["objects"][i][f] != b["objects"][i][f] for i in common for f in fields))
    if "money" not in missing:
        counts["money"] = dict(compared=len(a["money"]), divergences=sum(
            a["money"][s] != b["money"][s] for s in a["money"]))
    # shared-l0-v2 has no audited technology/population observer. Object counts
    # cannot substitute for population, nor map settings for runtime technology.
    return dict(tick=a["tick"], objects_compared=len(common), counts=counts,
                observed_divergences=sum(c["divergences"] or 0 for c in counts.values()),
                divergences=None, unavailable=sorted(missing | {"tech", "population"}))


def phase(roots, mission):
    if roots is None:
        return dict(category="not_collected", status="needs live run", rows=[]), None
    paths = [root / scenario_id(mission) / f"{name}.json"
             for root in roots for name in ("reference", "clone")]
    if any(not p.is_file() for p in paths):
        return dict(category="missing_captures", status="needs live run", rows=[]), None
    # Validate every complete stream before retaining counts or divergence leads.
    bundles = [l0.validate_bundle(l0.json_object(p.read_text()), p.parent) for p in paths]
    scenario = runner.load_scenario(ROOT / "tests/parity" / f"{scenario_id(mission)}.json")
    l0.require(all(b["mission"] == mission and b["ticks"] == [1, 6, 12]
                   and b["view"] == scenario["view"] for b in bundles), "wrong campaign alignment")
    review = l0.review_repeats(paths[0], paths[1], paths[2], paths[3])
    if review["category"] != "repeated_observations":
        return dict(review, rows=[]), None
    a, b = bundles[:2]
    return dict(category="repeated_observations", status="needs live run",
                comparison=review["comparison"],
                rows=[count_sample(x, y) for x, y in zip(a["samples"], b["samples"])]), (paths[0], a)


def mission_report(mission, before, after):
    result = dict(mission=mission, scenario=scenario_id(mission), evidence="P/S",
                  status="needs live run", before_after_aligned=None)
    references = []
    for name, roots in (("before", before), ("after", after)):
        try:
            result[name], reference = phase(roots, mission)
        except (OSError, ValueError, KeyError, TypeError) as exc:
            result[name], reference = dict(category="invalid_captures", detail=str(exc), rows=[]), None
        references.append(reference)
    if all(references):
        (pa, a), (pb, b) = references
        result["before_after_aligned"] = l0.first_difference(a, b) is None and all(
            x["frame"] is None or (pa.parent / x["frame"]).read_bytes() == (pb.parent / y["frame"]).read_bytes()
            for x, y in zip(a["samples"], b["samples"]))
        if not result["before_after_aligned"]:
            result["status"] = "reference changed; needs live run"
    return result


def display(value):
    return "—" if value is None else str(value)


def table(result):
    lines = [f"# Mission {result['mission']}: {result['status']}", "",
             "Counts are field instances; positions count x and y separately. Orders count complete records.",
             "A dash is unavailable. These tables cannot establish full initial-state parity.", "",
             f"Before/after reference alignment: {display(result['before_after_aligned'])}.", ""]
    for stage in ("before", "after"):
        data = result[stage]
        lines += [f"## {stage.title()}: {data['category']}", "",
                  "| Tick | Objects compared | Domain | Fields compared | Divergences |",
                  "| ---: | ---: | --- | ---: | ---: |"]
        rows = {r["tick"]: r for r in data["rows"]}
        for tick in (1, 6, 12):
            row = rows.get(tick, {})
            for domain in DOMAINS:
                count = row.get("counts", {}).get(domain, {})
                lines.append(f"| {tick} | {display(row.get('objects_compared'))} | {domain} | "
                             f"{display(count.get('compared'))} | {display(count.get('divergences'))} |")
        lines.append("")
    return "\n".join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, nargs=2, metavar=("RUN", "REPEAT"))
    parser.add_argument("--after", type=Path, nargs=2, metavar=("RUN", "REPEAT"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        output = runner.report.private_output(args.output)
        l0.require(not output.exists(), "output already exists")
        for root in (args.before or []) + (args.after or []):
            root = root.resolve()
            l0.require(not output.is_relative_to(root) and not root.is_relative_to(output), "output overlaps input")
        reports = [mission_report(m, args.before, args.after) for m in range(1, 16)]
        output.mkdir(parents=True)
        for report in reports:
            runner.report.write_json(output / f"mission_{report['mission']}.json", report)
            (output / f"mission_{report['mission']}.md").write_text(table(report))
        # Repeated domains are investigation leads, never evidence of a shared cause.
        leads = {}
        for stage in ("before", "after"):
            leads[stage] = {d: [r["mission"] for r in reports if any(
                row["counts"][d]["divergences"] for row in r[stage]["rows"])] for d in DOMAINS}
        runner.report.write_json(output / "report.json", dict(status="needs live run", missions=reports,
                                                              divergence_domains=leads))
        print(f"needs live run: 15 mission tables in {output}")
        return 1
    except (OSError, ValueError) as exc:
        parser.exit(2, f"parity_initial: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
