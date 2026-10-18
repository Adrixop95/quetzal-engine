#!/usr/bin/env python3
"""Render a crop of an Aztec Empire .map as top-down terrain PNG.
usage: render_map.py GAME_DIR MAP_FILE OUT.png [palette=4] [shade=14] [x0 y0 w h]"""
import sys, struct, os
sys.path.insert(0, os.path.dirname(__file__))
import xbe, sprite
from PIL import Image

game, mapf, outp = sys.argv[1:4]
pid = int(sys.argv[4]) if len(sys.argv) > 4 else 4
shade = int(sys.argv[5]) if len(sys.argv) > 5 else 14
m = open(mapf, 'rb').read()
W, H = struct.unpack_from('<HH', m, 0)
cells = struct.unpack_from('<%dI' % (W * H), m, 4)
x0, y0, cw, ch = (list(map(int, sys.argv[6:10])) if len(sys.argv) > 9 else (0, 0, min(W, 24), min(H, 24)))
pd, pg = xbe.load(os.path.join(game, 'Res/Pal.xbe'))
pal565 = {i: c for a, i, c in xbe.items(pd, pg) if a == 0x450 and c}[pid]
pal = []
for i in range(256):
    v = pal565[shade * 512 + 2 * i] | pal565[shade * 512 + 2 * i + 1] << 8
    pal.append((((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31))
d, g = xbe.load(os.path.join(game, 'Res/Tiles0.xbe'))
T = {i: b for a, i, b in xbe.items(d, g) if a == 0x44c}
cache = {}
def tile(i):
    if i not in cache:
        im = None
        b = T.get(i)
        if b and len(b) > 0x20:
            w, h, ox, oy, fl, rows = sprite.decode(b)
            if (w, h) == (48, 48):
                im = Image.new('RGB', (48, 48))
                for y, r in enumerate(rows):
                    for x, v in enumerate(r):
                        im.putpixel((x, y), pal[v])
        cache[i] = im
    return cache[i]
out = Image.new('RGB', (cw * 48, ch * 48))
for y in range(ch):
    for x in range(cw):
        t = tile(cells[(y0 + y) * W + x0 + x] & 0xffff)   # low 16 bits = base tile
        if t: out.paste(t, (x * 48, y * 48))
out.save(outp)
