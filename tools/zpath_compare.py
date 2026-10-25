#!/usr/bin/env python3 -I
"""Merge original-game experiment summaries (work/zp/<dir>/*/summary.json) with path_bench --json output into CSV + a markdown table.
usage: zpath_compare.py ORIG_DIR BENCH_JSONL OUT_PREFIX   (writes OUT_PREFIX_experiments.csv, OUT_PREFIX_bench.csv, OUT_PREFIX_compare.md)"""
import sys, json, glob, csv
orig_dir, bench, out = sys.argv[1:4]
orig = []
for f in sorted(glob.glob(orig_dir + '/*/summary.json')):
    try: s = json.load(open(f))
    except Exception: continue
    if s.get('n', 0) > 0: orig.append(s)
new = [json.loads(l) for l in open(bench) if l.strip()]
keys = ['course', 'fog', 'n', 'arrived', 'success', 'makespan', 'median_arrival', 'mean_path_ratio', 'lost', 'never_started', 'max_idle_ticks', 'mean_revisits', 'mean_latency', 'max_latency', 'ideal_ticks']
with open(out + '_experiments.csv', 'w', newline='') as f:
    w = csv.writer(f); w.writerow(keys)
    for s in sorted(orig, key=lambda s: (s['course'], s['fog'], s['n'])): w.writerow([s.get(k) for k in keys])
bk = ['course', 'n', 'arrived', 'success', 'makespan', 'median_arrival', 'mean_path_ratio', 'lost', 'max_idle_ticks', 'mean_revisits', 'ideal_ticks', 'overlap_samples', 'ms_per_tick', 'hash']
with open(out + '_bench.csv', 'w', newline='') as f:
    w = csv.writer(f); w.writerow(bk)
    for s in new: w.writerow([s.get(k) for k in bk])
nb = {(s['course'], s['n']): s for s in new}
lines = ['| kurs | n | oryginał (mgła): dotarło / makespan / mediana / stosunek drogi / max postój | oryginał (zbadane) | nowy: dotarło / makespan / mediana / stosunek / max postój | ideał |', '|---|---|---|---|---|---|']
def fmt(s): return '-' if not s else '%s/%s | %s | %s | %s | %s' % (s.get('arrived'), s.get('n'), s.get('makespan'), s.get('median_arrival'), s.get('mean_path_ratio'), s.get('max_idle_ticks'))
idx = {(s['course'], s['n'], s['fog']): s for s in orig}
for c in sorted({s['course'] for s in orig}):
    for n in sorted({s['n'] for s in orig if s['course'] == c}):
        a, b, x = idx.get((c, n, 'fog')), idx.get((c, n, 'explored')), nb.get((c, n))
        lines.append('| %s | %d | %s | %s | %s | %s |' % (c, n, fmt(a).replace(' | ', ' / '), fmt(b).replace(' | ', ' / '), (fmt(x).replace(' | ', ' / ') if x else '-'), (a or b or {}).get('ideal_ticks')))
open(out + '_compare.md', 'w').write('\n'.join(lines) + '\n')
print('\n'.join(lines))
