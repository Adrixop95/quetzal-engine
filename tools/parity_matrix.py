#!/usr/bin/env python3
"""Write aggregate parity counts without exporting private observations."""

import argparse
from collections import Counter
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("parity_triage", ROOT / "tools/parity_triage.py")
triage = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(triage)
PUBLIC_OUTPUT = ROOT / "docs/parity_matrix.md"


def render(records):
    lines = ["# Parity matrix", "",
             "Counts of scenario results in the selected diff reports. Repeated runs count",
             "separately. Pass means equality of the observed contract only; synthetic",
             "passes do not establish original-game parity. Unreported scenarios are excluded.", "",
             "| Layer | Scenarios | Pass | Fail | Skip |",
             "| --- | ---: | ---: | ---: | ---: |"]
    totals = Counter(row["status"] for row in records)
    for layer in (*triage.LAYERS, "Unknown"):
        counts = Counter(row["status"] for row in records if row["layer"] == layer)
        lines.append(f"| {layer} | {counts.total()} | {counts['pass']} | {counts['fail']} | {counts['skip']} |")
    lines.extend([f"| Total | {totals.total()} | {totals['pass']} | {totals['fail']} | {totals['skip']} |", "",
                  "| Category | Results |", "| --- | ---: |"])
    categories = Counter(row["category"] if row["category"] in triage.PUBLIC_CATEGORIES else "unclassified"
                         for row in records)
    if categories:
        lines.extend(f"| {category} | {count} |" for category, count in sorted(categories.items()))
    else:
        lines.append("| None | 0 |")
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=triage.PRIVATE)
    parser.add_argument("--output", type=Path, default=PUBLIC_OUTPUT)
    args = parser.parse_args(argv)
    try:
        output = args.output.absolute()
        if output == PUBLIC_OUTPUT:
            if output.is_symlink() or output.resolve() != PUBLIC_OUTPUT:
                raise ValueError("public matrix must be an ordinary worktree file")
        else:
            if output.suffix != ".md" or output.is_symlink():
                raise ValueError("private matrix output must be an ordinary Markdown file")
            output = triage.private_path(output)
        records = triage.collect(args.input)
        text = render(records)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding="utf-8")
        print(f"Scenario results: {len(records)}")
        return 0 if records else 77
    except (OSError, ValueError, TypeError) as exc:
        parser.exit(2, f"parity_matrix: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
