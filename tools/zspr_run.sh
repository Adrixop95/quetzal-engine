#!/bin/sh
# usage: tools/zspr_run.sh NAME TYPES OUTDIR [ROWY=318] [wait_ms=7000]
# GAME_DIR supplies a read-only installation. Only a private directory under
# work/ is writable by the game. BIN and VTABLES may point outside this worktree.
# SAMPLES, SAMPLE_MS, SAMPLE_SHOTS and POKE_FILE enable animation probes.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
NAME=$1
case "$NAME" in z*) ;; *) echo 'Map names must start with z.' >&2; exit 2;; esac
case "$NAME" in *[!a-zA-Z0-9_-]*) echo 'Invalid map name.' >&2; exit 2;; esac
SOURCE_GAME="$(cd "${GAME_DIR:-$ROOT/game}" && pwd)"
BIN=${BIN:-$ROOT/build_spr/aztec_recomp}
VTABLES=${VTABLES:-$ROOT/work/vtables.txt}
mkdir -p "$3" "$ROOT/work"
O="$(cd "$3" && pwd)"
if [ -e "$O/out.txt" ]; then echo 'Use a fresh output directory to preserve probe evidence.' >&2; exit 2; fi
G=$(mktemp -d "$ROOT/work/zspr_game_${NAME}.XXXXXX")
cleanup() {
  if [ "${KEEP_GAME:-0}" = 1 ]; then echo "Private game directory: $G"; else rm -rf "$G"; fi
}
trap cleanup EXIT
trap 'exit 130' INT TERM
mkdir -p "$G/Maps" "$G/Saves" "$G/adrox"
for f in Res Video Music Campaign Aztec.exe Keys.ini; do ln -s "$SOURCE_GAME/$f" "$G/$f"; done
if [ -d "$SOURCE_GAME/Saves" ]; then cp -r "$SOURCE_GAME/Saves/." "$G/Saves/"; fi
for f in "$SOURCE_GAME/"*.plr "$SOURCE_GAME/aztec_settings.ini"; do
  if [ -f "$f" ]; then cp "$f" "$G/"; fi
done
for f in "$SOURCE_GAME/Maps/"*; do
  b=$(basename "$f")
  case "$b" in z*) ;; *) ln -s "$f" "$G/Maps/$b";; esac
done
GAME_DIR="$G" python3 -B -I tools/make_zspr.py "$NAME" "$2" > "$O/placed.txt"
cp "$VTABLES" "$O/vtables.txt"
{
  echo 'wait 6000'; echo 'click 140 239'; echo 'wait 800'; echo 'click 160 543'
  echo 'wait 5000'; echo 'click 650 58'; echo 'wait 2500'
  for i in 1 2 3 4 5 6 7 8; do echo 'click 645 421'; echo 'wait 800'; done
  echo shot
  echo "click 150 ${4:-318}"; echo 'wait 800'; echo 'click 160 543'
  echo "wait ${5:-7000}"; echo shot; echo 'wait 1000'; echo shot
  echo "objscan $O/vtables.txt $O/objs.txt"; echo "heapdump $O/heap.bin"
  if [ -n "${PROBE_SCRIPT:-}" ]; then cat "$PROBE_SCRIPT"; fi
  i=0
  while [ "$i" -lt "${SAMPLES:-0}" ]; do
    if [ -n "${POKE_FILE:-}" ]; then awk -v n="$i" '$1==n{print "poke " $2 " " $3}' "$POKE_FILE"; fi
    echo "objscan $O/vtables.txt $O/s_$i.txt"
    if [ "${SAMPLE_SHOTS:-0}" = 1 ]; then
      echo shot
      echo "objscan $O/vtables.txt $O/s_$i.after.txt"
    fi
    echo "wait ${SAMPLE_MS:-100}"
    i=$((i+1))
  done
  echo 'wait 500'; echo quit
} > "$O/s.txt"
# Sample i uses shot_(i+4).ppm. The first three shots are renamed below.
: > "$O/samples.tsv"
i=0
while [ "$i" -lt "${SAMPLES:-0}" ]; do
  if [ "${SAMPLE_SHOTS:-0}" = 1 ]; then
    printf '%s\tshot_%02d.ppm\t%s\n' "s_$i.txt" "$((i+4))" "s_$i.after.txt" >> "$O/samples.tsv"
  fi
  i=$((i+1))
done
cd "$G"
status=0
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RT_OBJSCAN_DEREF=1 RT_DUMP_DIR="$O" \
  RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$O/s.txt" \
  timeout -s KILL 400 unshare -rn sh -c \
  'ip link set lo up && ip addr add 192.168.1.125/24 dev lo && exec "$@"' sh "$BIN" "$G/Aztec.exe" \
  > "$O/out.txt" 2>&1 || status=$?
if [ "$status" -eq 0 ] && [ ! -s "$O/objs.txt" ]; then status=1; fi
echo "exit=$status"
cd "$O"
if [ -f shot_01.ppm ]; then mv shot_01.ppm list.ppm; fi
if [ -f shot_02.ppm ]; then mv shot_02.ppm shot_01.ppm; fi
if [ -f shot_03.ppm ]; then mv shot_03.ppm shot_02.ppm; fi
exit "$status"
