#!/bin/sh
# usage: tools/explore.sh scenario.txt outdir   -> runs trace build (800x600), saves shots + calls.txt in outdir
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; mkdir -p "$2"; O="$(cd "$2" && pwd)"
RT_CALLS_FILE="$O/calls.txt" RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$1" RT_DUMP_DIR="$O" \
  timeout -s KILL "${3:-120}" unshare -rn sh -c "ip link set lo up; ip addr add 192.168.1.125/24 dev lo; exec $ROOT/build_trace/aztec_recomp $ROOT/game/Aztec.exe" >"$O/out.txt" 2>&1
python3 -I - "$O" <<'P'
import sys,glob
from PIL import Image
o=sys.argv[1]; fs=sorted(glob.glob(o+'/shot_*.ppm'))
if fs:
    ims=[Image.open(f).resize((400,300)) for f in fs]; cols=3; rows=(len(ims)+2)//3
    c=Image.new('RGB',(1200,300*rows))
    for i,im in enumerate(ims): c.paste(im,(400*(i%3),300*(i//3)))
    c.save(o+'/collage.png')
P
