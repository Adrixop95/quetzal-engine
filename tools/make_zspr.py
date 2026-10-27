#!/usr/bin/env python3 -I
"""Write game/Maps/NAME.* (mission_1 terrain, NAME e.g. zspr1) with the listed types on a 4-column grid (3-tile spacing) in an open area near the top of the map;
Start_X/Y of side 0 is moved there. SIDE env (default 0) = side owning the objects (own units reveal the fog, no fighting); one [Team] per mobile unit,
buildings/defences go to [Town]; boats/johnka unsupported. usage: make_zspr.py NAME 0,1,2 [dx dy]. Prints 'name x y type' (tile coordinates) per object."""
import re, sys, shutil, os, csv
from pathlib import Path
name = sys.argv[1]; types = [int(x) for x in sys.argv[2].split(',')]
SIDE = int(os.environ.get('SIDE', '0'))
ox, oy = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (0, 0)
game = os.environ.get('GAME_DIR', 'game')
src = os.path.join(game, 'Maps') + os.sep
for e in ('map', 'dat', 'rm', 'vis', 'spr', 'mit'):
    s = src + 'mission_1.' + e
    if os.path.exists(s): shutil.copy(s, src + name + '.' + e)
t = open(src + 'mission_1.mp', encoding='latin1').read()
head = t.split('[Team0]')[0]
# Retain the distant town, without inherited troops or mission-script actions.
head = re.sub(r'^Unit\d+=.*\n', '', head, flags=re.M)
head = re.sub(r'^Units=\d+', 'Units=0', head, flags=re.M)
Path(src + name + '.scn').write_bytes(b'')
head = re.sub(r'MapName=.*', 'MapName=' + name, head); head = re.sub(r'Name=Mission 1', 'Name=' + name, head)
metadata = (Path(__file__).resolve().parents[1] / 'src/engine/data/type_anims.hpp').read_text()
kinds = [int(kind) for kind in re.findall(r'\{"UGI_[^"]+",\s*(\d+),', metadata)]
def kind_of(t): return kinds[t] if 0 <= t < len(kinds) else -1
if any(kind_of(t) < 0 for t in types): raise ValueError('Invalid type id')
data_dir = Path(__file__).resolve().parents[1] / 'docs/data'
with (data_dir / 'types.csv').open() as source:
    type_rows = {int(row['id']): row for row in csv.DictReader(source)}
def maxhp(type_id):
    if 'HP' in os.environ: return int(os.environ['HP'])
    row = type_rows[type_id]
    field = {'Marine': 1, 'Headtruck': 3, 'Spider': 6, 'Zeppelin': 1, 'Bird': 1,
             'Truck': 4, 'Cannon': 5, 'Building': 1, 'Defence': 3}[row['kind']]
    with (data_dir / ('desc_' + row['kind'].lower() + '.csv')).open() as source:
        descriptor = next(d for d in csv.DictReader(source) if d['param'] == row['param'])
    return int(descriptor['d' + str(field)])
BUILD = [t for t in types if kind_of(t) in (2, 7)]
MOBILE = [t for t in types if t not in BUILD and kind_of(t) not in (8, 10, 11)]
dat = open(src + 'mission_1.dat', 'rb').read(); W = 90
def open_score(x, y): return sum(dat[yy * W + xx] == 2 for yy in range(y - 6, y + 7) for xx in range(x - 6, x + 7))
if len(sys.argv) > 4: cx, cy = 34 + ox, 39 + oy
else:   # open 13x13 block far from the player's town
    cand = [(open_score(x, y), x, y) for y in range(8, 52) for x in range(8, 82) if (x - 34) ** 2 + (y - 39) ** 2 > 22 ** 2]
    top = max(c[0] for c in cand); cx, cy = min(((x, y) for sc, x, y in cand if sc >= top - 12), key=lambda q: (q[0] - 50) ** 2 + (q[1] - 30) ** 2)
head = re.sub(r'(\[Side0\][^\[]*?)Color=\d+', lambda m: m.group(1) + 'Color=' + os.environ.get('COLOR', '0'), head, count=1, flags=re.S)
head = re.sub(r'Start_X=34', 'Start_X=%d' % cx, head); head = re.sub(r'Start_Y=39', 'Start_Y=%d' % cy, head)
spacing = int(os.environ.get('SPACING', '3'))
cells = [(cx + (i % 4) * spacing - 4, cy + (i // 4) * spacing - 4) for i in range(40)]   # 4 columns, 3-tile spacing
placed = []
nteams = len(MOBILE)
if 'START_TIME' in os.environ:
    head = re.sub(r'StartTime=\d+', 'StartTime=' + str(int(os.environ['START_TIME'])), head)
head = re.sub(r'Teams=\d+', 'Teams=%d' % nteams, head); head = re.sub(r'CurrentTeam=\d+', 'CurrentTeam=0', head)
out = [head]; k = 0
for team in range(nteams):
    out.append('[Team%d]\nScenarioID=%d\nSide=%d' % (team, team + 1, SIDE)); n = 0
    for tp in MOBILE[team:team + 1]:
        x, y = cells[k]; k += 1
        out.append('Unit%d=%3d,%3d,%3d,%5d' % (n, x, y, tp, maxhp(tp))); placed.append((x, y, tp)); n += 1
    out.append('Units=%d\n' % n)
if SIDE == 0:   # keep the victory banner away: one passive enemy far from the objects
    out.append('[Team%d]\nScenarioID=%d\nSide=1\nUnit0=%3d,%3d,0, 6000\nUnits=1\n' % (nteams, nteams + 1, 80 if cx < 45 else 10, 50)); nteams += 1
OPP = [int(x) for x in os.environ.get('OPP', '').split(',') if x]; OPPDX = int(os.environ.get('OPPDX', '2'))
ntown = 1
for g in range(0, len(BUILD), 6):
    grp = BUILD[g:g + 6]
    x0, y0 = cells[0][0] - 3, cells[0][1] - 3
    out.append('[Town%d]\nScenarioID=%d\nSide=%d\nX=%d\nY=%d\nType=2\nTendency=0\nBuildSpeed=1\nRace=1' % (ntown, 50 + ntown, SIDE, x0, y0))
    for j, tp in enumerate(grp):
        x, y = cells[k]; k += 1
        out.append('Unit%d=%3d,%3d,%3d,%5d' % (j, x, y, tp, maxhp(tp))); placed.append((x, y, tp))
    out.append('Units=%d\n' % len(grp)); ntown += 1
if any(kind_of(t) not in (0, 1, 3, 4, 5, 6, 9) for t in OPP): raise ValueError('Opponents must be mobile combat types')
if OPP:   # hostile side-1 unit(s) next to every placed object (fights, building destruction)
    for (x, y, tp) in list(placed):
        for oi, ot in enumerate(OPP):
            out.append('[Team%d]\nScenarioID=%d\nSide=1\nUnit0=%3d,%3d,%3d,%5d\nUnits=1\n' % (nteams, nteams + 1, x + OPPDX, y + oi, ot, maxhp(ot))); nteams += 1
text = '\n'.join(out)
text = re.sub(r'Teams=\d+', 'Teams=%d' % nteams, text)
text = re.sub(r'Towns=\d+', 'Towns=%d' % ntown, text)
open(src + name + '.mp', 'w', encoding='latin1').write(text)
print('# start', cx, cy)
for p in placed: print(name, *p)
