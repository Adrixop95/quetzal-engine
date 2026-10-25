#!/bin/sh
# Make a save game of campaign mission M (1..15) after WAIT_MS of play, via the scripted in-game menu, with a WriteFile trace.
# usage: runsave.sh MISSION SLOT(1..9) WAIT_MS NAME     -> work/savegame/runs/NAME/{wt.txt,s.txt,shot_01.ppm}; the save lands in game/Saves/single<SLOT>.sav (move it away!)
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; M=$1; SLOT=$2; W=$3; N=$4
D="$ROOT/work/savegame/runs/$N"; mkdir -p "$D"
{ echo "wait 6000"; echo "click 140 239"; echo "wait 800"; echo "click 160 543"; echo "wait 5000"
  if [ "$M" -ge 10 ]; then echo "click 645 421"; echo "wait 1200"; row=$((257+20*(M-10))); else row=$((257+20*(M-1))); fi
  echo "click 150 $row"; echo "wait 1200"; echo "click 140 544"; echo "wait 6000"; echo "click 700 430"; echo "wait $W"
  echo "click 626 10"; echo "wait 1000"; echo "click 250 190"; echo "wait 1500"; echo "click 200 $((138+20*SLOT))"; echo "wait 600"
  echo "click 184 359"; echo "wait 1200"; echo "key 30"; echo "key 28"; echo "wait 2500"; echo "shot"; echo "quit"; } > "$D/s.txt"
RT_WRITE_TRACE="$D/wt.txt" RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$D/s.txt" RT_DUMP_DIR="$D" \
  timeout -s KILL 200 unshare -rn sh -c "ip link set lo up; ip addr add 192.168.1.125/24 dev lo; exec $ROOT/build_fmt/aztec_recomp $ROOT/game/Aztec.exe" > "$D/out.txt" 2>&1
