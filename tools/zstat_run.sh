#!/bin/sh
# usage: tools/zstat_run.sh MAPNAME OUTDIR WAITMS [extra script lines file]
# Runs in a private mount namespace where game/Maps holds only MAPNAME.* (so it is the only list entry: page 1, row 1).
# Starts custom mission at list row MAPROW y-coordinate (last page), logs units per tick to OUTDIR/units.log (RT_UNIT_LOG).
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"; mkdir -p "$2"; M="$(mktemp -d)"; cp "$ROOT"/game/Maps/[Mm]ission_* "$ROOT"/game/Maps/"$1".* "$M"/; O="$(cd "$2" && pwd)"
{ echo "wait 6000"; echo "click 140 239"; echo "wait 800"; echo "click 160 543"; echo "wait 5000"; echo "click 650 58"; echo "wait 2500"
  echo "click 645 421"; echo "wait 800"; echo "click 150 378"; echo "wait 800"; echo "shot"; echo "click 160 543"; echo "wait 7000"; echo shot
  [ -n "$4" ] && cat "$4"
  echo "wait $3"; echo shot; echo quit; } > "$O/s.txt"
RT_UNIT_LOG="$O/units.log" RT_UNIT_EVERY="${RT_UNIT_EVERY:-4}" RT_DUMP_DIR="$O" RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$O/s.txt" \
  timeout -s KILL 200 unshare -rnm sh -c "ip link set lo up; ip addr add 192.168.1.125/24 dev lo; mount --bind $M $ROOT/game/Maps; exec $ROOT/build_stats/aztec_recomp $ROOT/game/Aztec.exe" >"$O/out.txt" 2>&1
echo "exit=$?"
