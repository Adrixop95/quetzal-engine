#!/usr/bin/env python3 -I
"""Write game/Maps/<name>.* (mission_1 terrain, empty scenario + generated triggers) for rule experiments.
usage: make_zstat.py NAME SPEC...   SPEC = side:type:count:x:y[:move=tx,ty@tick]   (units on a 1-tile grid, row-major, 5 per row)
Side 0 = human (gets a Town far away so the mission does not end), side 1 = AI. Each SPEC becomes one [Team] (ScenarioID = index+1)."""
import re, sys, shutil, os, csv
src = 'game/Maps/'; name = sys.argv[1]; specs = sys.argv[2:]
for e in ('map', 'dat', 'rm', 'vis', 'spr', 'mit'):
    s = src + 'mission_1.' + e
    if os.path.exists(s): shutil.copy(s, src + name + '.' + e)
hp = {}; uname = {}
for r in csv.DictReader(open('docs/data/types.csv')): hp[int(r['id'])] = (r['kind'], int(r['param'])); uname[int(r['id'])] = r['ugi']
def maxhp(t):
    k, p = hp[t]
    f = {'Marine': ('marine', 4), 'Headtruck': ('headtruck', 3), 'Spider': ('spider', 6), 'Zeppelin': ('zeppelin', 4), 'Bird': ('bird', 4), 'Truck': ('truck', 4), 'Cannon': ('cannon', 5), 'Building': ('building', 1), 'Defence': ('defence', 4)}[k]
    row = [r for r in csv.DictReader(open('docs/data/desc_%s.csv' % f[0])) if int(r['param']) == p][0]
    return int(row['d%d' % f[1]])
t = open(src + 'mission_1.mp', encoding='latin1').read()
head = t.split('[Team0]')[0]
head = re.sub(r'MapName=.*', 'MapName=' + name, head); head = re.sub(r'\nName=Mission 1', '\nName=' + name, head)
head = re.sub(r'Teams=\d+', 'Teams=%d' % len(specs), head); head = re.sub(r'CurrentTeam=\d+', 'CurrentTeam=0', head)
town = t[t.index('[Town0]'):t.index('[Team0]')]
out = [head]; trig = []; teams = []; towns = []
for i, s in enumerate(specs):
    p = s.split(':'); side, ty, n, x, y = map(int, p[:5])
    if hp[ty][0] in ('Building', 'Defence'):
        towns.append((side, ty, n, x, y)); continue
    teams.append(1)
    out.append('[Team%d]\nScenarioID=%d\nSide=%d' % (i, i + 1, side))
    for k in range(n):
        out.append('Unit%d=%3d,%3d,%3d,%5d' % (k, x + k % 5, y + k // 5, ty, maxhp(ty)))
    out.append('Units=%d\n' % n)
    for q in p[5:]:
        m = re.match(r'move=(\d+),(\d+)@(\d+)', q)
        if m: trig.append((int(m.group(3)), 'side(%d).tactic(%d).moveTo(%s,%s)' % (side, i + 1, m.group(1), m.group(2))))
for j, (side, ty, n, x, y) in enumerate(towns):
    race = 0 if uname[ty].startswith('RUS') or ty in (87,) and False else (1 if uname[ty].startswith('AZT') else (2 if uname[ty].startswith('CHI') else 0))
    out.append('[Town%d]\nScenarioID=%d\nSide=%d\nX=%d\nY=%d\nType=2\nTendency=0\nBuildSpeed=1\nRace=%d' % (j + 1, 50 + j, side, x, y, race))
    for k in range(n): out.append('Unit%d=%3d,%3d,%3d,%5d' % (k, x + 2 * (k % 4), y + 2 * (k // 4), ty, maxhp(ty)))
    out.append('Units=%d\n' % n)
head_towns = 1 + len(towns)
rest = t[t.index('[Paths]'):] if '[Paths]' in t else ''
txt = '\n'.join(out) + '\n' + rest
txt = re.sub(r'Towns=\d+', 'Towns=%d' % head_towns, txt); txt = re.sub(r'Teams=\d+', 'Teams=%d' % len(teams), txt)
open(src + name + '.mp', 'w', encoding='latin1').write(txt)
open(src + name + '.scn', 'w', newline='\r\n').write(''.join('trigger(%d)\n  conditions\n    localTime == %d\n  actions\n    %s\nend\n\n' % (j, tm, a) for j, (tm, a) in enumerate(trig)))
