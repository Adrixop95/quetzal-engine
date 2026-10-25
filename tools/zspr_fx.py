#!/usr/bin/env python3 -I
"""Catalogue of every anim-carrying object class seen in a SAMPLES run: per (vtable, anim id) the slots/frames observed, samples seen, object pointer offset of the Anim.
usage: zspr_fx.py GAME RUNDIR"""
import sys, os, glob, re, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sprmap
game, run = sys.argv[1], sys.argv[2]
its = sprmap.load_units(game); cache = {}
files = sorted(glob.glob(os.path.join(run, 's_*.txt')), key=lambda f: int(re.findall(r'(\d+)\.txt', f)[0]))
stat = collections.defaultdict(lambda: {'n': 0, 'slots': set(), 'frames': set(), 'off': set(), 'life': collections.Counter()})
for fi, f in enumerate(files):
    vt = {}
    for l in open(f):
        t = l.split()
        if t[0] != 'P': vt[t[0]] = t[1]
    for l in open(f + '.deref'):
        t = l.split()
        a = t[0]
        if a not in vt: continue
        off = int(t[1], 16); d = [int(x, 16) for x in t[3:]]
        rid, data, hdr, cur = d[0], d[1], d[2], d[4]
        if not (0 < rid < 420 and data > 0x1000 and (0x515, rid) in its and cur and abs(cur - data) < 0x10000): continue
        an = cache.get(rid) or cache.setdefault(rid, sprmap.parse_anim(its[(0x515, rid)]))
        base = data + 0x208; idx = (cur - base) // 32; st = (hdr - base) // 32 + 1 if hdr else None
        slot = next((s for s, v in an.items() if v['start'] == st), None)
        s = stat[(vt[a], rid)]; s['n'] += 1; s['off'].add(off); s['life'][a] += 1
        if slot is not None: s['slots'].add(slot); s['frames'].add(idx - st)
for (v, rid), s in sorted(stat.items()):
    print('vt %s anim %3d  objs %2d  max-life %3d samples  off %s  slots %s  frames<=%d' % (v, rid, len(s['life']), max(s['life'].values()), sorted(hex(o) for o in s['off']), sorted(s['slots'])[:12], max(s['frames']) if s['frames'] else -1))
