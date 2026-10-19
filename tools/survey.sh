#!/bin/sh
# Class survey: start each campaign mission (1-15) and custom maps in parallel, dump live objects at several times.
# usage: tools/survey.sh   -> work/survey/<name>/objs_{1,2,3}.txt
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; OUT="$ROOT/work/survey"; mkdir -p "$OUT"
python3 -I - "$ROOT/work/classes.json" "$ROOT/work/vtables.txt" <<'P'
import json,sys
open(sys.argv[2],'w').write('\n'.join(json.load(open(sys.argv[1]))))
P
gen() {  # name pageclicks row
  D="$OUT/$1"; mkdir -p "$D"
  { echo "wait 6000"; echo "click 140 239"; echo "wait 800"; echo "click 160 543"; echo "wait 5000"
    for i in $(seq 1 $2); do echo "click 645 421"; echo "wait 1200"; done
    echo "click 150 $3"; echo "wait 1200"; echo "click 140 544"; echo "wait 6000"
    echo "click 700 430"; echo "wait 7000"
    for t in 1 2 3; do echo "objscan $ROOT/work/vtables.txt $D/objs_$t.txt"; echo "wait 15000"; done
    echo "shot"; echo "quit"; } > "$D/s.txt"
}
for m in 1 2 3 4 5 6 7 8 9; do gen m$m 0 $((257+20*(m-1))); done
for m in 10 11 12 13 14 15; do gen m$m 1 $((257+20*(m-10))); done
for m in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
  D="$OUT/m$m"
  ( RT_CALLS_FILE="$D/calls.txt" RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$D/s.txt" RT_DUMP_DIR="$D" \
    timeout -s KILL 200 unshare -rn sh -c "ip link set lo up; ip addr add 192.168.1.125/24 dev lo; exec $ROOT/build_trace/aztec_recomp $ROOT/game/Aztec.exe" >"$D/out.txt" 2>&1; echo "m$m done" ) &
done
wait
