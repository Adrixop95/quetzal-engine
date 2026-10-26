#!/bin/sh
# usage: tools/zpath_run.sh COURSE NUNITS FOG OUTDIR [SECONDS] [PAGES] [ROW_Y]
# ZPATH_GAME_DIR is a read-only installation; all writable files stay under OUTDIR.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
C=$1; N=$2; FOG=$3; O=$4; SECS=${5:-130}; PAGES=${6:-0}; ROW=${7:-258}
BIN=${ZPATH_RUNTIME:-${BIN:-$ROOT/build_path/aztec_recomp}}
SOURCE=${ZPATH_GAME_DIR:-$ROOT/game}
SOURCE="$(cd "$SOURCE" && pwd)"
mkdir -p "$O"
O="$(cd "$O" && pwd)"
# Refuse reuse so a mistaken OUTDIR cannot remove somebody else's game files.
G="$O/game"
mkdir "$G"
mkdir "$G/Maps" "$G/Saves"
ZPATH_GAME_DIR="$SOURCE" python3 -I "$ROOT/tools/make_zpath.py" "$C" "$N" "$O" "$FOG"
for item in Aztec.exe Res Video Music Campaign; do ln -s "$SOURCE/$item" "$G/$item"; done
for file in "$SOURCE"/*.plr "$SOURCE"/*.ini; do
  [ ! -f "$file" ] || cp "$file" "$G/"
done
mv "$O"/Maps/zpath1.* "$G/Maps/"
rmdir "$O/Maps"
{
  echo "wait 9000"; echo "click 140 239"; echo "wait 1500"; echo "click 160 543"; echo "wait 7000"
  echo "click 650 58"; echo "wait 3500"
  for i in $(seq 1 "$PAGES"); do echo "click 645 421"; echo "wait 1200"; done
  echo "click 150 $ROW"; echo "wait 1500"; echo "click 160 543"; echo "wait 9000"
  echo "wait $((SECS*1000))"; echo shot; echo quit
} > "$O/s.txt"
status=0
(cd "$G" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  RT_CMD_REPLAY="$O/cmd.txt" RT_POS_LOG="$O/pos.txt" RT_PATH_LOG="$O/path.txt" RT_DUMP_DIR="$O" \
  RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$O/s.txt" \
  timeout -s KILL "$((SECS+120))" unshare -rn sh -ec \
  'ip link set lo up; ip addr add 192.168.1.125/24 dev lo; exec "$1" "$2"' sh "$BIN" "$G/Aztec.exe" \
  > "$O/out.txt" 2>&1) || status=$?
printf '%s %s exit=%s\n' "$C" "$N" "$status"
printf '%s\n' "$status" > "$O/exit_status.txt"
exit "$status"
