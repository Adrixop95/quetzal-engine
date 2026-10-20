#!/usr/bin/env python3 -I
"""Analyse a tools/zpath_run.sh output dir. usage: zpath_analyze.py OUTDIR [--json]
Per unit (footman, side-0 units of the course): arrival tick, path length walked, stuck ticks, oscillation; per run: success rate, makespan, efficiency."""
import sys, json, math, collections
d = sys.argv[1]; C = json.load(open(d + '/course.json'))
GX = {k: v for k, v in C['goals'].items()}
rows = collections.defaultdict(list)
for l in open(d + '/pos.txt'):
    f = l.split()
    if len(f) < 10 or int(f[3]) != 0 or int(f[9]) == 0: continue
    rows[f[1]].append((int(f[0]), int(f[4]), int(f[5]), int(f[6]), int(f[7]), int(f[8])))
units = {}
for k, v in rows.items():
    v.sort(); units[k] = v
t0 = C['order_frame']
# assign goal per unit by nearest start tile: units from course list in creation order are not identifiable; use first-position heuristic
res = []
for k, v in units.items():
    if v[0][3] < 5: continue
    f0 = [r for r in v if r[0] >= t0]
    if not f0: continue
    sx = f0[0][3]
    grp = '1' if (len(GX) > 1 and sx > 44) else '0'
    gx, gy = GX[grp]
    gpx, gpy = gx * 48 + 24, gy * 48 + 24
    path = 0.0; stuck = 0; maxstuck = 0; cur = 0; arr = None; seen = collections.Counter(); osc = 0; prev = None; last_move = f0[0][0]
    tiles = []
    for fr, x, y, tx, ty, st in f0:
        if prev:
            dd = math.hypot(x - prev[0], y - prev[1]); path += dd
            if dd < 0.5:
                if arr is None: cur += 1
            else:
                if cur > maxstuck: maxstuck = cur
                cur = 0; last_move = fr
        prev = (x, y)
        if arr is None and math.hypot(x - gpx, y - gpy) <= 5 * 48: arr = fr - t0
        if not tiles or tiles[-1] != (tx, ty): tiles.append((tx, ty))
    maxstuck = max(maxstuck, cur)
    revis = len(tiles) - len(set(tiles))
    fx, fy = f0[-1][1], f0[-1][2]
    res.append(dict(id=k, grp=grp, arrived=arr, path_px=round(path), final_dist=round(math.hypot(fx - gpx, fy - gpy)), max_idle=maxstuck, idle_tail=cur, revisits=revis, last_move=last_move - t0))
n = len(res); ok = [r for r in res if r['arrived'] is not None]
opt = {k: v for k, v in C['optimal_px'].items()}
summary = dict(course=C['name'], n=n, fog=C['fog'], arrived=len(ok), success=round(len(ok) / max(n, 1), 3),
    makespan=max([r['arrived'] for r in ok], default=None), median_arrival=sorted(r['arrived'] for r in ok)[len(ok) // 2] if ok else None,
    mean_path_ratio=round(sum(r['path_px'] / max(1, opt[r['grp']]) for r in ok) / max(len(ok), 1), 3) if ok else None,
    lost=sum(1 for r in res if r['arrived'] is None), stuck_final=sum(1 for r in res if r['arrived'] is None and r['idle_tail'] > 60),
    max_idle_ticks=max([r['max_idle'] for r in res], default=0), mean_revisits=round(sum(r['revisits'] for r in res) / max(n, 1), 2),
    ideal_ticks=round(opt["0"] / 9.0) if opt.get('0') else None)
if '--json' in sys.argv: print(json.dumps(dict(summary=summary, units=res)))
else:
    print(json.dumps(summary))
