#!/usr/bin/env python3 -I
"""Duel experiments. usage: zstat_duels.py run A:T [A:T ...]   (attacker type A on side 0 vs target type T on side 1, distance DIST tiles)
Packs 3 duels per map (zdl<k>), runs all maps in parallel, writes work/zdl/<A>_<T>.txt summaries of target HP drops. Env DIST (default 5), WAIT (ms)."""
import sys, os, subprocess, re, collections, csv
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
SLOTS = [(3, 4), (28, 4), (22, 30), (22, 42)]
DIST = int(os.environ.get('DIST', 5)); WAIT = os.environ.get('WAIT', '40000')
pairs = [tuple(map(int, a.split(':'))) for a in sys.argv[2:]]
kind = {int(r['id']): r['kind'] for r in csv.DictReader(open('docs/data/types.csv'))}
maps = [pairs[i:i + len(SLOTS)] for i in range(0, len(pairs), len(SLOTS))]
procs = []
os.makedirs('work/zdl', exist_ok=True)
for k, grp in enumerate(maps):
    specs = []
    for (a, t), (x, y) in zip(grp, SLOTS):
        if os.environ.get('NOMOVE'):
            specs += ['0:%d:1:%d:%d' % (a, x, y), '1:%d:1:%d:%d' % (t, x + DIST, y)]
        elif os.environ.get('MOVE_TARGET'):
            specs += ['0:%d:1:%d:%d' % (a, x, y), '1:%d:1:%d:%d:move=%d,%d@1300' % (t, x + DIST, y, x, y)]
        elif os.environ.get('STAY'):
            specs += ['0:%d:1:%d:%d:move=%d,%d@1300' % (a, x, y, x + DIST, y), '1:%d:1:%d:%d' % (t, x + DIST, y)]
        else:
            specs += ['0:%d:1:%d:%d:move=%d,%d@1300' % (a, x, y, x + DIST - 1, y), '1:%d:1:%d:%d' % (t, x + DIST, y)]
    name = os.environ.get('PFX', 'zdl') + '%d' % k
    subprocess.check_call(['python3', '-I', 'tools/make_zstat.py', name] + specs)
def run(k):
    return subprocess.Popen(['tools/zstat_run.sh', os.environ.get('PFX', 'zdl') + '%d' % k, 'work/zdl/' + os.environ.get('PFX', 'zdl') + 'm%d' % k, WAIT], env=dict(os.environ, RT_UNIT_EVERY='1'), stdout=subprocess.DEVNULL)
active = []
for k in range(len(maps)):
    while len(active) >= 12:
        active[0].wait(); active.pop(0)
    active.append(run(k))
for p in active: p.wait()
for k, grp in enumerate(maps):
    tr = collections.defaultdict(list)
    for l in open('work/zdl/' + os.environ.get('PFX', 'zdl') + 'm%d/units.log' % k):
        q = l.split()
        if q[0] == 'U': tr[(q[2], int(q[3]))].append((int(q[1]), int(q[5]), int(q[6]), int(q[7]), int(q[8])))
    for (a, t), (x, y) in zip(grp, SLOTS):
        cx = lambda tx: tx * 48 + 24
        def find(ty, tx):
            for (ad, tt), v in tr.items():
                if tt == ty and v[0][1] > 0 and abs(v[0][1] - cx(tx)) < 60 and abs(v[0][2] - cx(y)) < 60 or (tt == ty and v[0][1] > 0 and abs(v[0][1] - cx(tx)) < 130 and abs(v[0][2] - cx(y)) < 130):
                    return v
        va, vt = find(a, x), find(t, x + DIST)
        with open('work/zdl/%d_%d%s.txt' % (a, t, os.environ.get('SUFFIX', '')), 'w') as f:
            if not vt or not va: f.write('missing %s %s\n' % (bool(va), bool(vt))); continue
            f.write('attacker %d hp0 %d, target %d hp0 %d maxhp %d\n' % (a, va[0][3], t, vt[0][3], vt[0][4]))
            ev = []
            for p, q in zip(vt, vt[1:]):
                if q[3] != p[3]: ev.append((q[0], q[3] - p[3]))
            f.write('target hp events (frame, delta): %s\n' % ev[:60])
            ev = []
            for p, q in zip(va, va[1:]):
                if q[3] != p[3]: ev.append((q[0], q[3] - p[3]))
            f.write('attacker hp events: %s\n' % ev[:30])
            fh = next((q[0] for p_, q in zip(vt, vt[1:]) if q[3] - p_[3] < -20), None)
            if fh:
                pa = next(r for r in va if r[0] >= fh); pt = next(r for r in vt if r[0] >= fh)
                f.write('FIRSTHIT frame %d dist_px %.1f (attacker %s target %s)\n' % (fh, ((pa[1]-pt[1])**2 + (pa[2]-pt[2])**2) ** .5, pa[1:3], pt[1:3]))
            f.write('target last frame %d, attacker pos %s -> %s\n' % (vt[-1][0], va[0][1:3], va[-1][1:3]))
