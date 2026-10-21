#!/usr/bin/env python3 -I
"""Compare a zspr_run.sh screenshot with predicted idle sprites. usage: zspr_verify.py GAME_DIR RUNDIR [shot=shot_02.ppm]
Reads objs.txt (objscan), finds the view origin by matching predicted Marine-kind sprites against the screenshot, then for every idle (state 1) unit tests all
8 directions of anim slot 0 and reports which direction matches best vs the object's direction field. Writes RUNDIR/verify.png (screenshot crop | predicted)."""
import sys, os, struct, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'tools'))
import sprmap, sprite, xbe
from PIL import Image, ImageChops, ImageStat
game, run = sys.argv[1], sys.argv[2]; shot = sys.argv[3] if len(sys.argv) > 3 else 'shot_02.ppm'
its = sprmap.load_units(game); T = sprmap.type_table(os.path.join(run, 'heap.bin'), os.path.join(game, 'Aztec.exe'))
scr = Image.open(os.path.join(run, shot)).convert('RGB').crop((0, 0, 600, 600))
objs = []
for l in open(os.path.join(run, 'objs.txt')):
    t = l.split()
    if t[0] == 'P' or t[1] != '004932cc': continue
    f = lambda o: int(t[3 + o // 4], 16)
    objs.append(dict(type=f(0x50), state=f(0x64), dir=f(0x80), x=f(0x24), y=f(0x28), side=f(0x44) if False else None))
pals = {}
TEAM = int(os.environ.get('TEAM', '0'))
SHADE = int(os.environ.get('SHADE', '10'))
def render(sid, team):
    b = its.get((0x44c, sid))
    if not b or len(b) < 0x18: return None
    if b[8] & 0x40: w, h, ox, oy, fl, rows = sprmap.decode40(b)
    else: w, h, ox, oy, fl, rows = sprite.decode(b)
    pid = struct.unpack_from('<H', b, 0xc)[0]
    if pid < 4: pid += 0x10 + team * 4
    if pid not in pals: pals[pid] = sprmap.pal_for(game, pid, SHADE)
    pal = pals[pid]; im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    for y, r in enumerate(rows):
        for x, v in enumerate(r):
            if v is None: continue
            im.putpixel((x, y), pal[v] + (255,) if not (fl & 0x40) else pal[v[0]] + (v[1],))
    return im, ox, oy
anims = {}
def slot0(t, d):
    a = T[t]['anims'][0]
    if a not in anims: anims[a] = sprmap.parse_anim(its[(0x515, a)])
    return anims[a][0]['frames'][d][0]
def score(pred, ox, oy, scr, thr=48):
    """fraction of opaque predicted pixels whose screenshot pixel is close"""
    w, h = pred.size; ok = n = 0
    px = pred.load(); sp = scr.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 255: continue
            X, Y = ox + x, oy + y
            if 0 <= X < 600 and 0 <= Y < 600:
                n += 1; s = sp[X, Y]
                if abs(s[0] - r) < thr and abs(s[1] - g) < thr and abs(s[2] - b) < thr: ok += 1
    return ok / n if n else 0, n
idle = [o for o in objs if o['state'] == 1 and o['type'] != 0 or (o['state'] == 1 and o['x'] < 3000 and o['y'] < 600)]
cx, cy = [int(x) for l in open(os.path.join(run, 'placed.txt')) if l.startswith('# start') for x in l.split()[2:4]]
ORX, ORY = cx * 48 - 298, cy * 48 - 300   # view origin (unclamped), verified
objs = [o for o in objs if 0 <= o['x'] - ORX < 600 and 0 <= o['y'] - ORY < 600]

out = Image.new('RGB', (len([o for o in objs if True]) * 70, 200))
k = 0
for o in objs:
    if o['state'] != 1: continue
    res = []
    for d in range(8):
        sid = slot0(o['type'], d); im, sx, sy = render(sid, TEAM)
        s, n = score(im, o['x'] - sx - ORX, o['y'] - sy - ORY, scr); res.append(s)
    bd = max(range(8), key=lambda d: res[d])
    print('type %2d %-24s field-dir %d best-dir %d scores %s' % (o['type'], T[o['type']]['name'], o['dir'], bd, ' '.join('%.2f' % x for x in res)))
    cx0, cy0 = o['x'] - ORX, o['y'] - ORY
    out.paste(scr.crop((cx0 - 35, cy0 - 60, cx0 + 35, cy0 + 40)), (k * 70, 0))
    im, sx, sy = render(slot0(o['type'], o['dir']), TEAM)
    tmp = Image.new('RGB', (70, 100), (0, 0, 0)); tmp.paste(im, (35 - sx, 60 - sy), im); out.paste(tmp, (k * 70, 100)); k += 1
out.resize((out.width * 2, out.height * 2)).save(os.path.join(run, 'verify.png'))
