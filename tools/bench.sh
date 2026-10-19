#!/bin/sh
# Benchmark: start mission 1 at a given resolution and report logic ticks/s and presented frames/s.
# usage: tools/bench.sh [WxH]   (default 3440x1440)
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RES="${1:-3440x1440}"
OUT=$(mktemp -d)
cp "$ROOT/tests/scenarios/mission1_start.txt" "$OUT/s.txt"; printf 'wait 25000\nquit\n' >> "$OUT/s.txt"
RT_DEBUG=1 RT_DEBUG_FILE="$OUT/d.log" RT_RESOLUTION="$RES" RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$OUT/s.txt" \
  timeout -s KILL 120 "$ROOT/build/aztec_recomp" "$ROOT/game/Aztec.exe" >/dev/null 2>&1
python3 - "$OUT/d.log" "$RES" <<'P'
import re,sys
frames=[];pres=[]
for l in open(sys.argv[1]):
    m=re.match(r'\[ *(\d+)\].*\[netstate\].*frame=(\d+)',l)
    if m: frames.append((int(m.group(1)),int(m.group(2))))
    m=re.match(r'\[ *(\d+)\].*presented (\d+) frames',l)
    if m: pres.append((int(m.group(1)),int(m.group(2))))
act=[f for f in frames if f[1]>0]
if len(act)>6:
    a,b=act[2],act[-1]
    print("resolution %s: logic ticks/s = %.1f (12.0 is the game's own speed)"%(sys.argv[2],(b[1]-a[1])*1000.0/(b[0]-a[0])))
t0=act[0][0] if act else 0
late=[p[1] for p in pres if p[0]>t0+3000]
if late: print("presented frames/s (mission) avg %.1f"%(sum(late)/len(late)))
P
