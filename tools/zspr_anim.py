#!/usr/bin/env python3 -I
"""Decode per-object animation state from zspr_run.sh SAMPLES runs (needs RT_OBJSCAN_DEREF files s_N.txt.deref).
usage: zspr_anim.py GAME RUNDIR [typefilter] -> per object a timeline of (anim id, slot, frame idx, tick, state field) over the samples.
Anim object layout (0x20 B): +0 resource id, +4 data ptr, +8 header-frame ptr of current sequence, +0xc loop flag, +0x10 current frame ptr, +0x14 tick in frame, +0x18 changed flag."""
import sys, os, glob, struct, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sprmap
game, run = sys.argv[1], sys.argv[2]; flt = {int(x) for x in sys.argv[3].split(',')} if len(sys.argv) > 3 else None
its = sprmap.load_units(game); T = sprmap.type_table(os.path.join(run, 'heap.bin'), os.path.join(game, 'Aztec.exe'))
files = sorted(glob.glob(os.path.join(run, 's_*.txt')), key=lambda f: int(re.findall(r'(\d+)\.txt', f)[0]))
WORLD = {'004932cc', '004940ec', '0048fd20', '004951b8', '004948e8', '0048f97c', '00492c38', '004905bc', '0048faa8', '00490380', '00492d64'}
tl = {}
for fi, f in enumerate(files):
    objs = {}
    for l in open(f):
        t = l.split()
        if t[0] == 'P': continue
        objs[t[0]] = t
    der = {}
    if os.path.exists(f + '.deref'):
        for l in open(f + '.deref'):
            t = l.split(); der.setdefault(t[0], []).append((int(t[1], 16), [int(x, 16) for x in t[3:]]))
    for a, t in objs.items():
        w = lambda o: int(t[3 + o // 4], 16)
        ty = w(0x50)
        if ty >= 90 or (flt and ty not in flt) or t[1] not in WORLD: continue
        rec = {'state': {o: w(o) for o in (0x60, 0x64, 0x68, 0x74, 0x78, 0x7c, 0x80, 0x84) if True}, 'hp': w(0x38), 'pos': (w(0x24), w(0x28)), 'anims': []}
        for off, d in der.get(a, []):
            rid, data, hdr, loop, cur, tick = d[0], d[1], d[2], d[3] & 0xff, d[4], d[5]
            if 0 < rid < 420 and data and (0x515, rid) in its and data > 0x1000 and d[1] != 0 and (cur == 0 or abs(cur - data) < 0x10000):
                base = data + 0x208
                idx = (cur - base) // 32 if cur else None
                st = ((hdr - base) // 32 + 1) if hdr else None   # first frame index of the sequence
                an = sprmap.parse_anim(its[(0x515, rid)]) if rid not in tl.setdefault('_c', {}) else tl['_c'][rid]
                tl['_c'][rid] = an
                slot = next((s for s, v in an.items() if v['start'] == st), None)
                rec['anims'].append((off, rid, slot, (idx - st) if idx is not None and st else None, tick, loop))
        tl.setdefault((a, ty), []).append((fi, rec))
for k, v in tl.items():
    if k == '_c': continue
    a, ty = k; print('== %s type %d %s' % (a, ty, T[ty]['name']))
    last = None
    for fi, r in v:
        cur = (tuple(r['anims']), tuple(sorted(r['state'].items())))
        if cur != last:
            print('  s%03d hp%d pos%s state%s anims%s' % (fi, r['hp'], r['pos'], {hex(o): x for o, x in r['state'].items() if o in (0x64, 0x68, 0x60)}, [(hex(o), rid, sl, fr, tk) for o, rid, sl, fr, tk, lp in r['anims']]))
        last = cur
