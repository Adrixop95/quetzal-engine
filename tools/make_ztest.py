#!/usr/bin/env python3 -I
"""Write game/Maps/ztest.* : mission_1 terrain with one unit of each listed type on the enemy side. usage: make_ztest.py 0,1,2 ..."""
import re, sys, shutil, os
src = 'game/Maps/'
types = [int(x) for x in sys.argv[1].split(',')]
for e in ('map', 'dat', 'rm', 'vis', 'spr', 'scn', 'mit'):
    s = src + 'mission_1.' + e
    if os.path.exists(s): shutil.copy(s, src + 'ztest.' + e)
t = open(src + 'mission_1.mp', encoding='latin1').read()
head = t.split('[Team0]')[0]
head = re.sub(r'MapName=.*', 'MapName=ztest', head); head = re.sub(r'Name=Mission 1', 'Name=ZTest', head)
import struct
exe = open('game/Aztec.exe', 'rb').read()
def kind_of(t): o = 0x4933f8 + 8 * t - 0x48f000 + 0x8f000; return struct.unpack_from('<I', exe, o)[0]
BUILD = [t for t in types if kind_of(t) in (2, 7, 9) ]
BOATS = [t for t in types if kind_of(t) == 8]
JOHNK = [t for t in types if kind_of(t) == 10]
MOBILE = [t for t in types if t not in BUILD and t not in BOATS and t not in JOHNK and t != 89]
types = MOBILE
nteams = (len(types) + 8) // 9
head = re.sub(r'Teams=\d+', 'Teams=%d' % nteams, head); head = re.sub(r'CurrentTeam=\d+', 'CurrentTeam=0', head)
dat = open(src + 'mission_1.dat', 'rb').read(); W = 90
pos = [(x, y) for y in range(2, 58, 2) for x in range(2, 88, 2) if dat[y * W + x] == 2 and not (28 <= x <= 42 and 33 <= y <= 47)]
out = [head]; k = 0
for team in range(nteams):
    out.append('[Team%d]\nScenarioID=%d\nSide=1' % (team, team + 1)); n = 0
    for tp in types[team * 9:team * 9 + 9]:
        x, y = pos[k * 3 % len(pos)] if False else pos[k]; k += 1
        out.append('Unit%d=%3d,%3d,%3d,%5d' % (n, x, y, tp, 6000)); n += 1
    out.append('Units=%d\n' % n)
ntown = 1
bp = [p for p in pos[len(types):] ]
bi = 0
for g in range(0, len(BUILD), 6):
    grp = BUILD[g:g + 6]
    x0, y0 = bp[bi * 4] if bi * 4 < len(bp) else (10, 10); bi += 1
    out.append('[Town%d]\nScenarioID=%d\nSide=1\nX=%d\nY=%d\nType=2\nTendency=0\nBuildSpeed=1\nRace=1' % (ntown, 50 + ntown, x0, y0))
    for j, tp in enumerate(grp):
        x, y = bp[bi * 4 + j * 2] if bi * 4 + j * 2 < len(bp) else (12 + j, 12)
        out.append('Unit%d=%3d,%3d,%3d, 6000' % (j, x, y, tp))
    out.append('Units=%d\n' % len(grp)); ntown += 1
text = '\n'.join(out)
text = re.sub(r'Towns=\d+', 'Towns=%d' % ntown, text)
open(src + 'ztest.mp', 'w', encoding='latin1').write(text)
