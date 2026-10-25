#!/usr/bin/env python3
"""Pre-commit hook: refuse files that must never enter the repository (game assets, executables, generated code)."""
import re
import sys

FORBIDDEN = re.compile(
    r"(^|/)(game|work|ghidra|debug)/"          # local game install, derived data, decompiler projects, debug dumps
    r"|\.(exe|dll|xbe|avi|wav|iso|sav|plr|bds)$"  # original binaries / assets / user data
    r"|(^|/)gen_[0-9]+\.c$|(^|/)gen_(funcs\.h|table\.c)$",  # lifted code generated from the executable
    re.IGNORECASE,
)

bad = [f for f in sys.argv[1:] if FORBIDDEN.search(f)]
for f in bad:
    print(f"forbidden file (game asset or executable-derived output): {f}")
sys.exit(1 if bad else 0)
