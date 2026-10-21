#!/usr/bin/env python3 -I
"""Summarise work/zdl/A_T.txt into docs/data/duel_damage.csv: attacker, target, first hit damage (net of +regen), median hit interval (ticks), number of hits seen."""
import glob, re, ast, csv, os, statistics, collections
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
names = {}; regen = {}
for r in csv.DictReader(open('docs/data/types.csv')): names[int(r['id'])] = r['ugi']
rows = []
for f in sorted(glob.glob('work/zdl/*_*.txt')):
    a, t = map(int, os.path.basename(f)[:-4].split('_'))
    L = open(f).read().split('\n')
    if not L[0].startswith('attacker'): rows.append([a, names[a], t, names[t], 'missing', '', '']); continue
    ev = ast.literal_eval(L[1].split(': ', 1)[1].replace('target hp events (frame, delta)', '')) if False else ast.literal_eval(L[1][L[1].index('['):])
    # regen ticks show as small positive deltas; hits are negative
    hits = [(fr, d) for fr, d in ev if d < 0]
    pos = [d for fr, d in ev if d > 0]
    rg = statistics.mode(pos) if pos else 0
    if not hits: rows.append([a, names[a], t, names[t], 0, '', 0]); continue
    dm = [-(d - rg) if rg else -d for fr, d in hits]
    iv = [b[0] - a_[0] for a_, b in zip(hits, hits[1:]) if b[0] - a_[0] > 0]
    rows.append([a, names[a], t, names[t], statistics.median(dm), statistics.median(iv) if iv else '', len(hits)])
with open('docs/data/duel_damage.csv', 'w', newline='') as f:
    c = csv.writer(f, lineterminator='\n'); c.writerow(['attacker', 'attacker_ugi', 'target', 'target_ugi', 'damage_per_hit(median, regen removed)', 'interval_ticks(median)', 'hits_seen']); c.writerows(rows)
for r in rows: print(*r)
