#!/usr/bin/env python3
"""Pre-commit hook: the repository is English only. Flags Polish diacritics in text files.

Quoted original game data (cp1250 strings, Polish in-game texts) may be kept by adding the file to ALLOW below
or by putting the marker `lint: allow-polish` on the same line.
"""
import re
import sys

POLISH = re.compile("[ąćęłńóśźżĄĆĘŁŃÓŚŹŻ]")
ALLOW = {
    "tools/hooks/english_only.py",  # contains the character class itself
    "docs/formats.md",       # quotes original Polish game strings as data
    "docs/PLAN.md",          # quotes menu texts used by scripted tests
}

failed = False
for path in sys.argv[1:]:
    if path in ALLOW:
        continue
    try:
        text = open(path, encoding="utf-8").read()
    except (UnicodeDecodeError, OSError):
        continue
    for n, line in enumerate(text.splitlines(), 1):
        if POLISH.search(line) and "lint: allow-polish" not in line:
            print(f"{path}:{n}: non-English text: {line.strip()[:100]}")
            failed = True
sys.exit(1 if failed else 0)
