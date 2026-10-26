#!/bin/sh
# Regenerate private analysis only; never write generated source into tracked paths.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
GAME="$ROOT/game"
GHIDRA=
while [ "$#" -gt 0 ]; do
  case "$1" in
    --game|--ghidra)
      [ "$#" -ge 2 ] || { echo "Missing value for $1" >&2; exit 2; }
      case "$1" in --game) GAME=$2 ;; --ghidra) GHIDRA=$2 ;; esac
      shift 2 ;;
    --help)
      echo "usage: $0 [--game GAME_DIR] [--ghidra GHIDRA_12_DIR]"
      echo "Without Ghidra, reuse work/gen and work/decomp; report unavailable steps as SKIP."
      exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
done
cd "$ROOT"
GAME=$(realpath "$GAME")
command -v python3 >/dev/null
mkdir -p work/gen work/decomp
if [ -n "$GHIDRA" ]; then
  GHIDRA=$(realpath "$GHIDRA")
  [ -x "$GHIDRA/support/analyzeHeadless" ] || { echo "Invalid Ghidra directory" >&2; exit 2; }
  [ -f "$GAME/Aztec.exe" ] || { echo "Missing $GAME/Aztec.exe" >&2; exit 2; }
  # This existing helper imports, lifts into work/gen, and builds in this worktree's build/.
  sh tools/build_recomp.sh "$GHIDRA" "$GAME"
  "$GHIDRA/support/analyzeHeadless" "$ROOT/ghidra" AztecProj \
    -process Aztec.exe -readOnly -noanalysis -scriptPath "$ROOT/tools/ghidra_scripts" \
    -postScript ExportAll.java "$ROOT/work/decomp"
else
  echo "SKIP lift/build and decompilation: pass --ghidra DIR to generate fresh inputs."
fi
if [ ! -f "$GAME/Aztec.exe" ]; then
  echo "SKIP executable analysis: place your own Aztec.exe in game/ or pass --game DIR."
  exit 0
fi
command -v objdump >/dev/null
objdump -d -M intel -j .text "$GAME/Aztec.exe" > work/text.asm
if [ ! -s work/gen/gen_funcs.h ]; then
  echo "SKIP class analysis: work/gen/gen_funcs.h requires the Ghidra lifter. Disassembly is ready."
  exit 0
fi
python3 -I tools/analyze_classes.py "$GAME/Aztec.exe" work/gen/gen_funcs.h work/classes.json
python3 -I tools/class_tree.py work/text.asm work/classes.json work/gen/gen_funcs.h work/class_edges.json
python3 -I tools/ctor_sizes.py work/text.asm work/classes.json work/ctor_sizes.json
python3 -I tools/source_map.py "$GAME/Aztec.exe" work/text.asm work/gen/gen_funcs.h work/classes.json work/source_map.json
python3 -I tools/func_names.py "$GAME/Aztec.exe" work/text.asm work/gen/gen_funcs.h work/func_names.json
python3 -I - <<'PY'
import json
from pathlib import Path
Path('work/vtables.txt').write_text('\n'.join(json.loads(Path('work/classes.json').read_text())) + '\n')
PY
if [ -s work/decomp/all.c ]; then
  python3 -I tools/class_fields.py work/classes.json work/decomp/all.c work/classes.md
  python3 -I tools/object_kinds.py "$GAME/Aztec.exe" work/decomp/all.c work/classes.json > work/object_kinds.txt
else
  echo "SKIP fields/factory: work/decomp/all.c requires Ghidra ExportAll.java."
fi
if find work/survey -path '*/m*/objs_*.txt' -type f -print 2>/dev/null | grep -q .; then
  python3 -I tools/class_model.py work/survey work/classes.json work/class_edges.json work/ctor_sizes.json work/class_model.json
  python3 -I tools/gen_class_docs.py work/class_model.json work/source_map.json work/class_edges.json work/classes.json work/classes_observed.md
  python3 -I tools/gen_model_headers.py work/class_model.json work/source_map.json work/class_edges.json work/classes.json work/objects.hpp
else
  echo "SKIP live model: collect work/survey/m*/objs_*.txt with isolated reference experiments first."
fi
echo "Available analysis regenerated under ignored work/. Missing inputs were reported above."
