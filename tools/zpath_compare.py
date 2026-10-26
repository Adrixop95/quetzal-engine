#!/usr/bin/env python3 -I
"""Merge analyzed reference runs and path_bench JSONL; retain invalid run status.
usage: zpath_compare.py ORIG_DIR BENCH_JSONL OUT_PREFIX
Writes OUT_PREFIX_original.csv, OUT_PREFIX_new.csv and OUT_PREFIX_compare.md.
"""
import csv
import json
from pathlib import Path
import sys

orig_dir, bench, out = sys.argv[1:4]
original = [json.loads(p.read_text()) for p in sorted(Path(orig_dir).glob('*/summary.json'))]
new = [json.loads(line) for line in Path(bench).read_text().splitlines() if line.strip()]
metrics = ['arrived', 'success', 'lost', 'stuck', 'makespan', 'median_arrival',
           'mean_path_ratio', 'mean_time_ratio', 'max_idle_ticks', 'mean_revisits', 'ideal_ticks']

def write_csv(path, rows, keys):
    with open(path, 'w', newline='') as stream:
        writer = csv.DictWriter(stream, keys, extrasaction='ignore')
        writer.writeheader()
        for row in rows:
            writer.writerow({key: round(value, 6) if isinstance(value, float) else value
                             for key, value in row.items() if key in keys})

write_csv(out + '_original.csv', original,
          ['course', 'fog', 'n', 'ticks', 'status', 'matched', 'missing', *metrics,
           'never_started', 'mean_latency', 'max_latency', 'source', 'note'])
write_csv(out + '_new.csv', new,
          ['course', 'n', 'ticks', *metrics, 'settled', 'overlap_samples', 'ms_per_tick', 'hash'])
index = {}
for row in original:
    key = row['course'], row['n'], row['fog']
    # An invalid attempt must never overwrite an accepted observation.
    if key not in index or row['status'] == 'ok':
        index[key] = row

def fmt(row):
    if not row:
        return 'unmeasured'
    if row.get('status', 'ok') != 'ok':
        return row['status']
    return f"{row['arrived']}/{row['n']} / {row['makespan']} / {row['mean_path_ratio']:.3f}"

lines = ['| Course | Units | Original fog: arrived / ticks / distance ratio | Original explored | New |',
         '|---|---:|---|---|---|']
for row in new:
    name, count = row['course'], row['n']
    a, b = index.get((name, count, 'fog')), index.get((name, count, 'explored'))
    if any(s and s.get('ticks') != row['ticks'] for s in (a, b)):
        raise ValueError(f'observation budgets differ for {name}/{count}')
    lines.append(f'| {name} | {count} | {fmt(a)} | {fmt(b)} | {fmt(row)} |')
Path(out + '_compare.md').write_text('\n'.join(lines) + '\n')
print('\n'.join(lines))
