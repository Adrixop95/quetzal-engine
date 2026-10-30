#!/usr/bin/env python3
"""Group private first-divergence reports into drafts for human review."""

import argparse
from collections import defaultdict
import json
import os
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "work/parity"
LAYERS = tuple(f"L{i}" for i in range(9))
PASSES = {"equal_observed_fields", "equal_observed_visual_ui"}
SKIPS = {"skipped_missing_private_inputs", "skipped"}
# Public output uses a closed vocabulary: arbitrary private strings never escape.
PUBLIC_CATEGORIES = PASSES | SKIPS | {
    "field_difference", "object_lifecycle", "blocked_observation_contract",
    "experiment_failed", "reference_nondeterminism", "reference_semantic_nondeterminism",
    "blocked_setup_contract", "blocked_alignment_contract", "invalid_observation",
    "visual_structure", "visual_pixels", "ui_sequence",
}


def private_path(path):
    base = PRIVATE.resolve()
    resolved = path.resolve()
    if not base.is_relative_to(ROOT) or not resolved.is_relative_to(base):
        raise ValueError("paths must stay inside this worktree's work/parity")
    return resolved


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON key")
            result[key] = value
        return result
    private_path(path)
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique)


def category_name(value):
    if not isinstance(value, str) or not re.fullmatch(r"[a-z][a-z0-9_]{0,79}", value):
        raise ValueError("invalid category identifier")
    return value


def result_category(diff):
    if not isinstance(diff, dict):
        raise ValueError("diff must be a JSON object")
    category = category_name(diff.get("category"))
    first = diff.get("first")
    if first is not None:
        if category != "different" or not isinstance(first, dict):
            raise ValueError("inconsistent first-divergence report")
        first_category = category_name(first.get("category"))
        if first_category in PASSES | SKIPS | {"different"}:
            raise ValueError("first divergence requires a failure category")
        return first_category
    if category == "different":
        raise ValueError("different report requires a first divergence")
    return category


def result_layer(path, diff):
    """Use declared metadata; a directory name alone never establishes a layer."""
    layers = [diff["layer"]] if "layer" in diff else []
    scenario = path.parent / "scenario.json"
    if scenario.exists():
        metadata = read_json(scenario)
        if not isinstance(metadata, dict) or "layer" not in metadata:
            raise ValueError("scenario metadata requires a layer")
        layers.append(metadata["layer"])
    summary = path.parent.parent / "report.json"
    if summary.exists() and summary.parent.is_relative_to(PRIVATE.resolve()):
        rows = read_json(summary)
        if isinstance(rows, list):
            matches = [row for row in rows if isinstance(row, dict) and row.get("id") == path.parent.name]
            if len(matches) > 1:
                raise ValueError("duplicate scenario in run summary")
            if matches and "layer" in matches[0]:
                layers.append(matches[0]["layer"])
    if any(layer not in LAYERS for layer in layers) or len(set(layers)) > 1:
        raise ValueError("invalid or conflicting layer metadata")
    return layers[0] if layers else "Unknown"


def collect(input_dir):
    """Read direct and nested harness diffs once; ignore drafts and directory symlinks."""
    input_dir = private_path(input_dir)
    if not input_dir.exists():
        return []
    if not input_dir.is_dir():
        raise ValueError("input must be a directory")
    records = []
    for directory, dirs, files in os.walk(input_dir, followlinks=False):
        dirs[:] = sorted(d for d in dirs if not (Path(directory) / d).is_symlink()
                         and Path(directory) / d != PRIVATE.resolve() / "tasks")
        if "diff.json" not in files:
            continue
        path = Path(directory) / "diff.json"
        if path.is_symlink():
            raise ValueError("diff files must not be symlinks")
        diff = read_json(path)
        category = result_category(diff)
        status = "pass" if category in PASSES else "skip" if category in SKIPS else "fail"
        records.append(dict(path=path, layer=result_layer(path, diff), category=category, status=status))
    return sorted(records, key=lambda row: str(row["path"]))


def write_drafts(records, output):
    output = private_path(output)
    if not output.is_relative_to(private_path(PRIVATE / "tasks")):
        raise ValueError("drafts must stay inside work/parity/tasks")
    groups = defaultdict(list)
    for row in records:
        if row["status"] == "fail":
            groups[row["category"]].append(row)
    # Validate all destinations before creating anything. Existing drafts preserve review edits.
    targets = {category: output / f"{category}.md" for category in groups}
    for target in targets.values():
        if target.is_symlink() or (target.exists() and not target.is_file()):
            raise ValueError("draft destination must be an ordinary file")
    if groups:
        output.mkdir(parents=True, exist_ok=True)
    created = 0
    for category, rows in sorted(groups.items()):
        target = targets[category]
        if target.exists():
            continue
        sources = "\n".join(f"- {json.dumps(str(row['path'].relative_to(ROOT)))}" for row in rows)
        text = f"""# Draft: investigate {category}

Review required: a human or Claude must review this private draft before queuing.
This draft is a diagnostic lead, not an established gameplay defect.

Read CLAUDE.md, docs/parity.md, docs/PARITY_TEST_PLAN.md and docs/EXPERIMENTS.md.
Inspect the following private first-divergence reports and their companion inputs:

{sources}

Reproduce the earliest reported difference with the same scenario and observation
contract. Check complete coverage, identity, units and tick alignment first.
For experiment failures or blockers, repair the prerequisite before inferring a
semantic defect. Never invent a tick, object, field or rule from missing evidence.
Use separate private game copies, dummy SDL drivers, a hard timeout and
unshare -rn for live runs. Skip absent private inputs; keep isolation failures
as failures. Keep assets, captures, audio, observations and executable-derived
material in ignored work/parity/. Write public changes in English only.

Limit implementation to the reproduced cause. Add meaningful synthetic regression
controls, rerun the paired scenario when prerequisites permit, then verify the
Release build, CTest and pre-commit. Update docs/parity.md and
docs/engine_rules_status.md only for actual evidence. Review and sanitize any
prompt before moving it to tasks/queue/; this tool never queues work.
"""
        with target.open("x", encoding="utf-8") as stream:
            stream.write(text)
        created += 1
    return created


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=PRIVATE)
    parser.add_argument("--output", type=Path, default=PRIVATE / "tasks")
    args = parser.parse_args(argv)
    try:
        records = collect(args.input)
        created = write_drafts(records, args.output)
        print(f"Reports: {len(records)}; new review drafts: {created}")
        return 0 if records else 77
    except (OSError, ValueError, TypeError) as exc:
        parser.exit(2, f"parity_triage: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
