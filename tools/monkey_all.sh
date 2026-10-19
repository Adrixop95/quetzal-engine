#!/bin/sh
# usage: tools/monkey_all.sh SECONDS SEED  -> runs missions 1..9 in parallel with random input (trace build), results in work/monkey/<m>
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; S=${1:-120}; SEED=${2:-1}
for m in 1 2 3 4 5 6 7 8 9; do
  D="$ROOT/work/monkey/$m"; mkdir -p "$D"
  python3 -I "$ROOT/tools/monkey.py" $m $SEED $S > "$D/s.txt"
  ( RT_CALLS_FILE="$D/calls.txt" RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$D/s.txt" RT_DUMP_DIR="$D" RT_DEBUG=1 RT_DEBUG_FILE="$D/debug.log" \
    timeout -s KILL $((S+120)) unshare -rn sh -c "ip link set lo up; ip addr add 192.168.1.125/24 dev lo; exec $ROOT/build_trace/aztec_recomp $ROOT/game/Aztec.exe" >"$D/out.txt" 2>&1; echo "m$m exit=$? $(grep -m1 -E 'FATAL|fatal|SIGSEGV|unimplemented' "$D/out.txt")" ) &
done
wait
