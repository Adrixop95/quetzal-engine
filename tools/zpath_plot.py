#!/usr/bin/env python3 -I
"""Trail plot of a zpath run: usage zpath_plot.py OUTDIR out.ppm  (4 px per tile; walls grey, trails coloured by tick)"""
import sys, json, collections
d = sys.argv[1]; C = json.load(open(d + '/course.json')); S = 6
W, H = C['W'], C['H']; img = [[(20, 20, 20)] * (W * S) for _ in range(H * S)]
for y, row in enumerate(C['grid']):
    for x, ch in enumerate(row):
        if ch == '0':
            for j in range(S):
                for i in range(S): img[y * S + j][x * S + i] = (90, 90, 90)
        elif ch == '2':
            for j in range(S):
                for i in range(S): img[y * S + j][x * S + i] = (40, 50, 40)
fr = [int(l.split()[0]) for l in open(d + '/pos.txt')]; mx = max(fr)
for l in open(d + '/pos.txt'):
    f = l.split()
    if int(f[3]) != 0 or int(f[9]) == 0: continue
    t = int(f[0]); x = int(f[4]) * S // 48; y = int(f[5]) * S // 48
    if 0 <= x < W * S and 0 <= y < H * S:
        c = int(255 * t / mx); img[y][x] = (255 - c, 80 + c // 2, c)
with open(sys.argv[2], 'wb') as o:
    o.write(b'P6\n%d %d\n255\n' % (W * S, H * S))
    for r in img: o.write(bytes(v for p in r for v in p))
