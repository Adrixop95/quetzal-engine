#!/bin/sh
# usage: tools/zpath_run.sh COURSE NUNITS FOG OUTDIR [SECONDS] [PAGES] [ROW_Y]
# Builds a private game dir (OUTDIR/game, symlinks to assets, own Maps), the course (tools/make_zpath.py), replays box-select+move via
# RT_CMD_REPLAY and logs positions (RT_POS_LOG) and WaveSearch calls (RT_PATH_LOG). Runs in a private netns (parallel-safe).
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
C=$1; N=$2; FOG=$3; O=$4; SECS=${5:-90}; PAGES=${6:-8}; ROW=${7:-278}; BIN=${BIN:-$ROOT/build_path/aztec_recomp}
mkdir -p "$O"; O="$(cd "$O" && pwd)"; G="$O/game"; rm -rf "$G"; mkdir -p "$G/Maps"
python3 -I "$ROOT/tools/make_zpath.py" "$C" "$N" "$O" "$FOG" || exit 1
cp "$ROOT/game/Aztec.exe" "$ROOT/game/aztec_settings.ini" "$ROOT/game/Keys.ini" "$ROOT"/game/*.plr "$G/" 2>/dev/null
for d in Res Video Music Campaign; do ln -s "$ROOT/game/$d" "$G/$d"; done
for f in "$ROOT"/game/Maps/*; do case "$f" in */ztest.*|*/zpath*) ;; *) ln -s "$f" "$G/Maps/";; esac; done
mv "$O"/Maps/zpath1.* "$G/Maps/"; rmdir "$O/Maps"
{ echo "wait 9000"; echo "click 140 239"; echo "wait 1500"; echo "click 160 543"; echo "wait 7000"; echo "click 650 58"; echo "wait 3500"
  for i in $(seq 1 $PAGES); do echo "click 645 421"; echo "wait 1200"; done
  [ -n "$SHOTLIST" ] && echo shot
  echo "click 150 $ROW"; echo "wait 1500"; echo "click 160 543"; echo "wait 9000"
  echo "wait $((SECS*1000))"; echo shot; echo quit; } > "$O/s.txt"
( cd "$G" && RT_CMD_REPLAY="$O/cmd.txt" RT_POS_LOG="$O/pos.txt" RT_PATH_LOG="$O/path.txt" RT_DUMP_DIR="$O" RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$O/s.txt" \
  timeout -s KILL $((SECS+120)) unshare -rn sh -c "ip link set lo up; ip addr add 192.168.1.125/24 dev lo; exec $BIN $G/Aztec.exe" >"$O/out.txt" 2>&1 )
echo "$C $N exit=$? $(grep -m1 FATAL "$O/out.txt")"
