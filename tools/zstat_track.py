#!/usr/bin/env python3 -I
"""usage: zstat_track.py units.log [type...]  -> per object (addr,type): first/last frame, hp, and per-sample position (x,y px)."""
import sys, collections
tr = collections.OrderedDict(); money = collections.defaultdict(list)
for l in open(sys.argv[1]):
    p = l.split()
    if p[0] == 'U':
        f, a, t, s, x, y, hp, mh = int(p[1]), p[2], int(p[3]), int(p[4]), int(p[5]), int(p[6]), int(p[7]), int(p[8])
        tr.setdefault((a, t), []).append((f, x, y, hp, mh))
    elif p[0] == 'S' and p[2] in '01': money[int(p[2])].append((int(p[1]), int(p[3]), int(p[4]), int(p[5])))
if __name__ == '__main__':
    want = {int(x) for x in sys.argv[2:]}
    for (a, t), v in tr.items():
        if want and t not in want: continue
        print(a, t, 'frames %d-%d' % (v[0][0], v[-1][0]), 'n', len(v))
        prev = None
        for r in v[::max(1, len(v) // 12)]: print('   ', r)
