#!/bin/sh
# Lift Aztec.exe with Ghidra and build the native runtime.
# usage: tools/build_recomp.sh /path/to/ghidra /path/to/game_dir   (game_dir contains Aztec.exe)
set -e
GHIDRA="$1"; GAME="$2"; ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ -x "$GHIDRA/support/analyzeHeadless" ] && [ -f "$GAME/Aztec.exe" ] || { echo "usage: $0 GHIDRA_DIR GAME_DIR"; exit 2; }
mkdir -p "$ROOT/ghidra" "$ROOT/work/gen"
if [ ! -d "$ROOT/ghidra/AztecProj.rep" ]; then
  "$GHIDRA/support/analyzeHeadless" "$ROOT/ghidra" AztecProj -import "$GAME/Aztec.exe" -overwrite
fi
"$GHIDRA/support/analyzeHeadless" "$ROOT/ghidra" AztecProj -process Aztec.exe -readOnly -noanalysis \
  -scriptPath "$ROOT/tools/ghidra_scripts" -postScript LiftAll.java "$ROOT/work/gen" 40
cmake -S "$ROOT" -B "$ROOT/build" -DAZTEC_GEN_DIR="$ROOT/work/gen"
cmake --build "$ROOT/build" --target aztec_recomp -j"$(nproc)"
echo "run: $ROOT/build/aztec_recomp $GAME/Aztec.exe"
