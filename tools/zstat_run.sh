#!/bin/sh
# usage: ZSTAT_GAME_DIR=private/copy ZSTAT_RUNTIME=/path/to/aztec_recomp tools/zstat_run.sh MAP OUT WAITMS [script]
# Each run gets private writable Maps, Saves and player/configuration files.
set -eu
ROOT="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$ROOT"
: "${ZSTAT_GAME_DIR:?Set ZSTAT_GAME_DIR to a private game copy; see docs/EXPERIMENTS.md.}"
: "${ZSTAT_RUNTIME:?Set ZSTAT_RUNTIME to the reference runtime executable.}"
mkdir -p "$2"
O="$(CDPATH='' cd -- "$2" && pwd)"
SOURCE="$(CDPATH='' cd -- "$ZSTAT_GAME_DIR" && pwd)"
RUNTIME="$(realpath "$ZSTAT_RUNTIME")"
M="$(mktemp -d "$O/game.XXXXXX")"
mkdir "$M/Maps" "$M/Saves"
for entry in Aztec.exe Res Video Music Campaign; do
  [ ! -e "$SOURCE/$entry" ] || ln -s "$SOURCE/$entry" "$M/$entry"
done
for entry in "$SOURCE"/*.plr "$SOURCE"/*.ini; do
  [ ! -f "$entry" ] || cp "$entry" "$M/"
done
# Only the requested mission is listed; campaign assets are already copied into it.
cp "$SOURCE/Maps/$1".* "$M/Maps/"
{
  echo 'wait 6000'; echo 'click 140 239'; echo 'wait 800'; echo 'click 160 543'
  echo 'wait 5000'; echo 'click 650 58'; echo 'wait 2500'
  echo "click 150 ${MAPROW:-258}"; echo 'wait 800'; echo shot
  echo 'click 160 543'; echo 'wait 7000'; echo shot
  [ -z "${4:-}" ] || cat "$4"
  echo "wait $3"; echo shot; echo quit
} > "$O/s.txt"
cd "$M"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
RT_UNIT_LOG="$O/units.log" RT_UNIT_EVERY="${RT_UNIT_EVERY:-4}" RT_DUMP_DIR="$O" \
RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$O/s.txt" \
  timeout -s KILL "${TMO:-200}" unshare -rn sh -ec '
    ip link set lo up
    ip addr add 192.168.1.125/24 dev lo
    exec "$1" "$2"
  ' sh "$RUNTIME" "$M/Aztec.exe" >"$O/out.txt" 2>&1
# Failures propagate to callers; a failed run must never count as a no-hit duel.
test -s "$O/units.log"
