#!/bin/sh
# Check a fresh checkout and install local hooks; never install system packages.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HOOKS=1
case "${1:-}" in
  --no-hooks) HOOKS=0 ;;
  --help) echo "usage: $0 [--no-hooks]"; exit 0 ;;
  '') ;;
  *) echo "usage: $0 [--no-hooks]" >&2; exit 2 ;;
esac
cd "$ROOT"
missing=0
for tool in git cmake g++ python3 pre-commit objdump pkg-config; do
  if command -v "$tool" >/dev/null 2>&1; then
    echo "Found $tool"
  else
    echo "Missing required tool: $tool" >&2
    missing=1
  fi
done
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists 'sdl2 >= 2.0.18'; then
  echo "Found SDL2 >= 2.0.18"
else
  echo "Missing SDL2 development files >= 2.0.18 (pkg-config sdl2)." >&2
  missing=1
fi
command -v ffmpeg >/dev/null 2>&1 || echo "Optional: install ffmpeg for intro video playback."
[ "$missing" -eq 0 ] || { echo "See docs/SETUP.md for installation commands." >&2; exit 1; }
mkdir -p build build_analysis work/gen work/decomp
if [ "$HOOKS" -eq 1 ]; then
  pre-commit install || {
    echo "Hook installation failed. With read-only git metadata use --no-hooks and run pre-commit manually." >&2
    exit 1
  }
else
  echo "Hook installation skipped; run pre-commit manually in this restricted worktree."
fi
cat <<'NEXT'
Next steps (from the repository root):
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAZTEC_GEN_DIR=
  cmake --build build --parallel 2
  ctest --test-dir build --output-on-failure
  pre-commit run --all-files
Read CLAUDE.md, docs/HANDOFF.md, docs/SETUP.md and tasks/README.md.
Place your own game installation in ignored game/ before data-dependent checks.
Optional analysis: sh tools/regen_analysis.sh (see --help for Ghidra).
NEXT
