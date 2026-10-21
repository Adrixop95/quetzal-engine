#!/usr/bin/env python3 -I
"""For each object of the given UGI types in a zspr run, find which (anim, slot, frame) sprite best matches the screenshot at the object position.
usage: zspr_scan.py GAME RUNDIR TYPE[,TYPE..] [shot] ; prints best 3 candidates per object plus raw words +0x58..+0xb0 for field correlation.
Draw top-left = pos - (ox, oy) (+ optional y offset frame[6]); origin = Start*48 - (298,300) (Start from placed.txt '# start')."""
import sys, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'tools'))
import sprmap, sprite
from PIL import Image
game, run, tl = sys.argv[1], sys.argv[2], {int(x) for x in sys.argv[3].split(',')}
shot = sys.argv[4] if len(sys.argv) > 4 else 'shot_02.ppm'
its = sprmap.load_units(game); T = sprmap.type_table(os.path.join(run, 'heap.bin'), os.path.join(game, 'Aztec.exe'))
scr = Image.open(os.path.join(run, shot)).convert('RGB').crop((0, 0, 600, 600)); sp = scr.load()
cx, cy = [int(x) for l in open(os.path.join(run, 'placed.txt')) if l.startswith('# start') for x in l.split()[2:4]]
OX, OY = cx * 48 - 298, cy * 48 - 300
SHADE = int(os.environ.get('SHADE', '10')); pals = {}
cache = {}
def spr(sid):
    if sid in cache: return cache[sid]
    b = its.get((0x44c, sid)); r = None
    if b and len(b) >= 0x18:
        if b[8] & 0x40: w, h, ox, oy, fl, rows = sprmap.decode40(b)
        else: w, h, ox, oy, fl, rows = sprite.decode(b)
        pid = struct.unpack_from('<H', b, 0xc)[0]
        if pid < 4: pid += 0x10
        if pid not in pals: pals[pid] = sprmap.pal_for(game, pid, SHADE)
        pal = pals[pid]; px = []
        for y, row in enumerate(rows):
            for x, v in enumerate(row):
                if v is None: continue
                if fl & 0x40:
                    if v[1] < 255: continue
                    v = v[0]
                px.append((x, y, pal[v]))
        r = (px, ox, oy)
    cache[sid] = r; return r
def score(sid, X, Y, thr=40):
    r = spr(sid)
    if not r or not r[0]: return 0
    px, ox, oy = r; ok = n = 0
    for x, y, c in px:
        a, b = X - ox + x, Y - oy + y
        if 0 <= a < 600 and 0 <= b < 600:
            n += 1; s = sp[a, b]
            if abs(s[0] - c[0]) < thr and abs(s[1] - c[1]) < thr and abs(s[2] - c[2]) < thr: ok += 1
    return ok / n if n > 20 else 0
anims = {}
for l in open(os.path.join(run, 'objs.txt')):
    t = l.split()
    if t[0] == 'P': continue
    f = lambda o: int(t[3 + o // 4], 16)
    ty = f(0x50)
    if ty not in tl or ty >= 90 or t[1] == '0048f5e0': continue
    X, Y = f(0x24) - OX, f(0x28) - OY
    if not (0 <= X < 600 and 0 <= Y < 600): continue
    print('== type %d %s vt %s pos %d,%d screen %d,%d' % (ty, T[ty]['name'], t[1], f(0x24), f(0x28), X, Y))
    print('   words', ' '.join('%x:%x' % (o, f(o)) for o in range(0x58, 0xb0, 4) if f(o)))
    for ai, aid in enumerate(T[ty]['anims']):
        if aid not in anims: anims[aid] = sprmap.parse_anim(its[(0x515, aid)])
        res = []
        for s, sq in anims[aid].items():
            for i, fr in enumerate(sq['frames']):
                res.append((score(fr[0], X, Y + 0 * fr[6]), s, i, fr[0], fr[6]))
        res.sort(reverse=True)
        print('   anim', aid, 'best', ['%.2f slot%d frame%d spr%d y%d' % r for r in res[:3]])
