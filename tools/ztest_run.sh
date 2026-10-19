#!/bin/sh
# usage: tools/ztest_run.sh TYPES(comma list) OUTDIR [probe]  -> starts the ZTest custom mission (last page, row 2), dumps objects
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"; python3 -I tools/make_ztest.py "$1"; mkdir -p "$2"; O="$(cd "$2" && pwd)"
{ echo "wait 6000"; echo "click 140 239"; echo "wait 800"; echo "click 160 543"; echo "wait 5000"; echo "click 650 58"; echo "wait 2500"
  for i in 1 2 3 4 5 6 7 8; do echo "click 645 421"; echo "wait 800"; done
  echo "click 150 278"; echo "wait 800"; echo "click 160 543"; echo "wait 7000"; echo shot
  echo "objscan $ROOT/work/vtables.txt $O/objs.txt"; echo "heapdump $O/heap.bin"; echo "wait 1500"; echo quit; } > "$O/s.txt"
RT_DUMP_DIR="$O" RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$O/s.txt" timeout -s KILL 100 "$ROOT/build/aztec_recomp" "$ROOT/game/Aztec.exe" >"$O/out.txt" 2>&1
echo "exit=$? $(grep -m1 FATAL "$O/out.txt")"
