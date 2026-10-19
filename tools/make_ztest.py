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
open(src + 'ztest.mp', 'w', encoding='latin1').write('\n'.join(out))
