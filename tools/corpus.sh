#!/bin/sh
# Record a replay corpus: random input (tools/monkey.py) per campaign mission, commands (RT_CMD_LOG) and per-tick state hashes (RT_STATE_LOG).
# usage: tools/corpus.sh record SECONDS SEED | tools/corpus.sh replay   -> work/corpus/m<N>/{cmd.txt,state.txt,state_replay.txt}
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; MODE=$1; S=${2:-90}; SEED=${3:-1}; OUT="$ROOT/work/corpus"; mkdir -p "$OUT"
run() { # dir env...
  D=$1; shift
  ( env "$@" RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$D/s.txt" timeout -s KILL $((S+150)) unshare -rn sh -c "ip link set lo up; ip addr add 192.168.1.125/24 dev lo; exec $ROOT/build/aztec_recomp $ROOT/game/Aztec.exe" >"$D/out.txt" 2>&1; echo "$(basename $D) done" ) &
}
for m in 1 2 3 4 5 6 7 8 9; do
  D="$OUT/m$m"; mkdir -p "$D"
  if [ "$MODE" = record ]; then
    python3 -I "$ROOT/tools/monkey.py" $m $SEED $S > "$D/s.txt"
    run "$D" RT_CMD_LOG="$D/cmd.txt" RT_STATE_LOG="$D/state.txt"
  else
    y=$((257+20*(m-1))); sed "s/click 150 257/click 150 $y/" "$ROOT/tests/scenarios/mission1_start.txt" > "$D/r.txt"; printf 'wait %d\nquit\n' $(((S+40)*1000)) >> "$D/r.txt"; cp "$D/r.txt" "$D/s.txt"
    run "$D" RT_CMD_REPLAY="$D/cmd.txt" RT_STATE_LOG="$D/state_replay.txt"
  fi
done
wait
