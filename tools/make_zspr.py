#!/usr/bin/env python3 -I
"""Write game/Maps/NAME.* (mission_1 terrain, NAME e.g. zspr1) with the listed types on a 4-column grid (3-tile spacing) in an open area near the top of the map;
Start_X/Y of side 0 is moved there. SIDE env (default 0) = side owning the objects (own units reveal the fog, no fighting); one [Team] per mobile unit,
buildings/defences go to [Town]; boats/johnka unsupported. usage: make_zspr.py NAME 0,1,2 [dx dy]. Prints 'name x y type' (tile coordinates) per object."""
import re, sys, shutil, os, struct
name = sys.argv[1]; types = [int(x) for x in sys.argv[2].split(',')]
SIDE = int(os.environ.get('SIDE', '0'))
ox, oy = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (0, 0)
src = 'game/Maps/'
for e in ('map', 'dat', 'rm', 'vis', 'spr', 'scn', 'mit'):
    s = src + 'mission_1.' + e
    if os.path.exists(s): shutil.copy(s, src + name + '.' + e)
t = open(src + 'mission_1.mp', encoding='latin1').read()
head = t.split('[Team0]')[0]
head = re.sub(r'MapName=.*', 'MapName=' + name, head); head = re.sub(r'Name=Mission 1', 'Name=' + name, head)
exe = open('game/Aztec.exe', 'rb').read()
def kind_of(t): return struct.unpack_from('<I', exe, 0x4933f8 + 8 * t - 0x400000)[0]
BUILD = [t for t in types if kind_of(t) in (2, 7, 9)]
MOBILE = [t for t in types if t not in BUILD and kind_of(t) not in (8, 10, 11)]
dat = open(src + 'mission_1.dat', 'rb').read(); W = 90
def open_score(x, y): return sum(dat[yy * W + xx] == 2 for yy in range(y - 6, y + 7) for xx in range(x - 6, x + 7))
if len(sys.argv) > 4: cx, cy = 34 + ox, 39 + oy
else:   # open 13x13 block far from the player's town
    cand = [(open_score(x, y), x, y) for y in range(8, 52) for x in range(8, 82) if (x - 34) ** 2 + (y - 39) ** 2 > 22 ** 2]
    top = max(c[0] for c in cand); cx, cy = min(((x, y) for sc, x, y in cand if sc >= top - 12), key=lambda q: (q[0] - 50) ** 2 + (q[1] - 30) ** 2)
head = re.sub(r'Start_X=34', 'Start_X=%d' % cx, head); head = re.sub(r'Start_Y=39', 'Start_Y=%d' % cy, head)
cells = [(cx + (i % 4) * 3 - 4, cy + (i // 4) * 3 - 4) for i in range(40)]   # 4 columns, 3-tile spacing
placed = []
nteams = len(MOBILE)
head = re.sub(r'Teams=\d+', 'Teams=%d' % nteams, head); head = re.sub(r'CurrentTeam=\d+', 'CurrentTeam=0', head)
out = [head]; k = 0
for team in range(nteams):
    out.append('[Team%d]\nScenarioID=%d\nSide=%d' % (team, team + 1, SIDE)); n = 0
    for tp in MOBILE[team:team + 1]:
        x, y = cells[k]; k += 1
        out.append('Unit%d=%3d,%3d,%3d,%5d' % (n, x, y, tp, 6000)); placed.append((x, y, tp)); n += 1
    out.append('Units=%d\n' % n)
if SIDE == 0:   # keep the victory banner away: one passive enemy far from the objects
    out.append('[Team%d]\nScenarioID=%d\nSide=1\nUnit0=%3d,%3d,0, 6000\nUnits=1\n' % (nteams, nteams + 1, 80 if cx < 45 else 10, 50)); nteams += 1
ntown = 1
for g in range(0, len(BUILD), 3):
    grp = BUILD[g:g + 3]
    x0, y0 = cells[k]; k += 1
    out.append('[Town%d]\nScenarioID=%d\nSide=%d\nX=%d\nY=%d\nType=2\nTendency=0\nBuildSpeed=1\nRace=1' % (ntown, 50 + ntown, SIDE, x0, y0))
    for j, tp in enumerate(grp):
        x, y = cells[k + 2]; k += 4
        out.append('Unit%d=%3d,%3d,%3d, 6000' % (j, x, y, tp)); placed.append((x, y, tp))
    out.append('Units=%d\n' % len(grp)); ntown += 1
text = '\n'.join(out)
text = re.sub(r'Teams=\d+', 'Teams=%d' % nteams, text)
text = re.sub(r'Towns=\d+', 'Towns=%d' % ntown, text)
open(src + name + '.mp', 'w', encoding='latin1').write(text)
print('# start', cx, cy)
for p in placed: print(name, *p)
