#!/usr/bin/env python3 -I
"""usage: zstat_speed.py units.log -> for units starting at x<400: type, per-tick displacement stats while moving (px/tick)."""
import sys, statistics
sys.argv = [sys.argv[0], sys.argv[1]]
exec(open(__file__.replace('zstat_speed', 'zstat_track')).read().split("if __name__")[0])
for (a, t), v in tr.items():
    if v[0][1] >= 400 or v[0][0] > 10: continue
    d = []; 
    for p, q in zip(v, v[1:]):
        if q[0] - p[0] == 1:
            dd = ((q[1] - p[1]) ** 2 + (q[2] - p[2]) ** 2) ** .5
            if dd > 0: d.append(round(dd, 2))
    if not d: print(t, 'no move'); continue
    mid = d[len(d) // 4: 3 * len(d) // 4]
    print('type %2d maxhp %6d  moving ticks %3d  median %.2f  min %.2f max %.2f  mean(mid) %.3f  total %d px' % (t, v[0][4], len(d), statistics.median(d), min(mid), max(mid), statistics.mean(mid), sum(d)))
