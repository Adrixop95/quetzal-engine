#!/bin/sh
# usage: tools/zspr_run.sh NAME TYPES OUTDIR [ROWY=318] [wait_ms=7000]
# Builds map NAME (make_zspr.py) and runs the game from a private game dir (work/zspr_game: symlinks, own Maps without other agents' z* maps),
# starts it from the custom missions list, saves shot_01.ppm/shot_02.ppm (+ list.ppm) in OUTDIR. Private network namespace.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"; NAME=$1; mkdir -p "$3"; O="$(cd "$3" && pwd)"; G=$ROOT/work/zspr_game_$NAME
rm -rf "$G"; mkdir -p "$G/Maps"
for f in Res Video Music Campaign Aztec.exe Keys.ini; do ln -s "$ROOT/game/$f" "$G/$f"; done
cp -r game/Saves game/*.plr game/aztec_settings.ini "$G/" 2>/dev/null; mkdir -p "$G/adrox"
for f in game/Maps/*; do b=$(basename "$f"); case "$b" in z*) ;; *) ln -s "$ROOT/$f" "$G/Maps/$b";; esac; done
cd "$ROOT"; python3 -I tools/make_zspr.py "$NAME" "$2" > "$O/placed.txt" || exit 1
mv game/Maps/$NAME.* "$G/Maps/"
{ echo "wait 6000"; echo "click 140 239"; echo "wait 800"; echo "click 160 543"; echo "wait 5000"; echo "click 650 58"; echo "wait 2500"
  for i in 1 2 3 4 5 6 7 8; do echo "click 645 421"; echo "wait 800"; done
  echo shot
  echo "click 150 ${4:-318}"; echo "wait 800"; echo "click 160 543"; echo "wait ${5:-7000}"; echo shot; echo "wait 1000"; echo shot; echo "objscan $ROOT/work/vtables.txt $O/objs.txt"; echo "heapdump $O/heap.bin"; echo "wait 500"; echo quit; } > "$O/s.txt"
cd "$G"
RT_DUMP_DIR="$O" RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$O/s.txt" timeout -s KILL 120 unshare -rn sh -c "ip link set lo up; ip addr add 192.168.1.125/24 dev lo; exec $ROOT/build/aztec_recomp $G/Aztec.exe" >"$O/out.txt" 2>&1
echo "exit=$? $(grep -m1 FATAL "$O/out.txt")"
cd "$O"; mv shot_01.ppm list.ppm 2>/dev/null; mv shot_02.ppm shot_01.ppm 2>/dev/null; mv shot_03.ppm shot_02.ppm 2>/dev/null
rm -rf "$G"
