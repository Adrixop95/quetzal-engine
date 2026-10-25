#!/usr/bin/env python3 -I
"""Analyse a tools/zpath_run.sh output dir. usage: zpath_analyze.py OUTDIR [--json]
Units are matched to course.json units by their first logged tile. Per unit: arrival tick (within 5 tiles of the group goal, counted from the
group's order frame), walked path, longest idle streak before arrival, tile revisits, start latency (ticks from order to first movement).
Per run: success rate, makespan, median arrival, path ratio vs optimal, lost/stuck counts."""
import sys, json, math, collections
d = sys.argv[1]; C = json.load(open(d + '/course.json'))
GOALS = {int(k): v for k, v in C['goals'].items()}
OF = {int(k): v for k, v in C.get('order_frames', {'0': C['order_frame']}).items()}
cu = {(u[0], u[1]): u for u in C['units']}
rows = collections.defaultdict(list); first = {}
for l in open(d + '/pos.txt'):
    f = l.split()
    if len(f) < 10 or int(f[9]) == 0: continue
    rows[f[1]].append((int(f[0]), int(f[4]), int(f[5]), int(f[6]), int(f[7]), int(f[8]), int(f[3])))
res = []
# match logged objects to course units: greedy nearest start tile (units may walk a few tiles before the first logged tick), same type
pairs = []
for k, v in rows.items():
    v.sort(); t0 = v[0]
    for u in C['units']:
        if len(u) > 3 and u[3] != t0[6]: continue
        if len(u) <= 3 and t0[6] != 0: continue
        dd = (t0[3] - u[0]) ** 2 + (t0[4] - u[1]) ** 2
        if dd <= 9 * 9: pairs.append((dd, k, (u[0], u[1])))
pairs.sort(); usedk = set(); usedu = set(); match = {}
for dd, k, uk in pairs:
    if k in usedk or uk in usedu: continue
    usedk.add(k); usedu.add(uk); match[k] = cu[uk]
for k, v in rows.items():
    t0 = v[0]
    u = match.get(k)
    if u is None: continue
    grp = u[2]; gx, gy = GOALS[grp]; gpx, gpy = gx * 48 + 24, gy * 48 + 24; of = OF[grp]
    f0 = [r for r in v if r[0] >= of]
    if not f0: continue
    path = 0.0; cur = 0; maxidle = 0; arr = None; prev = None; tiles = []; started = None
    for fr, x, y, tx, ty, st, ty_ in f0:
        if prev:
            dd = math.hypot(x - prev[0], y - prev[1]); path += dd
            if dd >= 0.5:
                if started is None: started = fr - of
                maxidle = max(maxidle, cur); cur = 0
            elif arr is None and started is not None: cur += 1
        prev = (x, y)
        if arr is None and math.hypot(x - gpx, y - gpy) <= 6 * 48: arr = fr - of
        if not tiles or tiles[-1] != (tx, ty): tiles.append((tx, ty))
    maxidle = max(maxidle, cur)
    fx, fy = f0[-1][1], f0[-1][2]
    res.append(dict(id=k, grp=grp, type=t0[6], arrived=arr, path_px=round(path), final_dist=round(math.hypot(fx - gpx, fy - gpy)), max_idle=maxidle, idle_tail=cur,
                    revisits=len(tiles) - len(set(tiles)), latency=started))
n = len(res); ok = [r for r in res if r['arrived'] is not None]
opt = {int(k): v for k, v in C['optimal_px'].items()}
lat = [r['latency'] for r in res if r['latency'] is not None]
summary = dict(course=C['name'], n=n, fog=C['fog'], arrived=len(ok), success=round(len(ok) / max(n, 1), 3),
    makespan=max([r['arrived'] for r in ok], default=None), median_arrival=sorted(r['arrived'] for r in ok)[len(ok) // 2] if ok else None,
    mean_path_ratio=round(sum(r['path_px'] / max(1, opt[r['grp']]) for r in ok) / max(len(ok), 1), 3) if ok else None,
    lost=sum(1 for r in res if r['arrived'] is None), never_started=sum(1 for r in res if r['latency'] is None),
    max_idle_ticks=max([r['max_idle'] for r in res], default=0), mean_revisits=round(sum(r['revisits'] for r in res) / max(n, 1), 2),
    mean_latency=round(sum(lat) / len(lat), 1) if lat else None, max_latency=max(lat) if lat else None,
    ideal_ticks=round(opt[0] / 9.0) if opt.get(0) else None)
if '--json' in sys.argv: print(json.dumps(dict(summary=summary, units=res)))
else: print(json.dumps(summary))
