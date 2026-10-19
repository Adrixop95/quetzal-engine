#!/usr/bin/env python3 -I
"""Build the object model from live-object dumps (objscan output of tools/survey.sh).
usage: class_model.py work/survey work/classes.json work/class_edges.json work/ctor_sizes.json out.json
Per class: instance count, missions, allocation size, per-dword field statistics and a type guess."""
import sys, json, glob, collections, re
survey, cj, ej, sj, out = sys.argv[1:6]
cl = json.load(open(cj)); edges = json.load(open(ej)); csz = json.load(open(sj))
ctor_vt = collections.defaultdict(list)
for vt, c in cl.items():
    for f in c['ctors']: ctor_vt[f].append(vt)
objs = collections.defaultdict(list)    # vt -> [(mission, rec)]
pools = collections.defaultdict(list)    # mission -> [(base, hwm, stride, pool_addr)]
for path in sorted(glob.glob(survey + '/m*/objs_*.txt')):
    mission = path.split('/')[-2]
    seen = set()
    for l in open(path):
        t = l.split()
        if t[0] == 'P':
            pools[mission].append((int(t[2], 16), int(t[3], 16), int(t[4], 16), int(t[1], 16))); continue
        key = t[0]
        if key in seen: continue
        seen.add(key)
        objs[t[1]].append((mission, int(t[0], 16), int(t[2], 16), int(t[3], 16), [int(x, 16) for x in t[4:]]))
def pool_stride(m, a):
    for base, hwm, stride, pa in pools.get(m, []):
        if base and base <= a < max(hwm, base) + stride: return stride - 8
    return None
model = {}
def kind(vals):
    vs = set(vals)
    if len(vs) == 1: return 'const'
    if all(v == 0 for v in vals): return 'zero'
    if all(0x10000000 <= v < 0x70000000 or v == 0 for v in vals): return 'ptr'
    if all(0x400000 <= v < 0x800000 or v == 0 for v in vals): return 'code/data ptr'
    if all(v < 0x10000 or v >= 0xffff0000 for v in vals): return 'int'
    if all((v & 0xff) == 0 for v in vals): return 'fixed8'
    return 'dword'
for vt, lst in objs.items():
    sizes = collections.Counter()
    for m, a, h0, h1, d in lst:
        if (h0 & 0xffff0000) == 0xa1100000: sizes[h1] += 1
        elif 0x50000000 <= a < 0x6e000000:
            ps = pool_stride(m, a)
            sizes[ps if ps else 'pool'] += 1
    nfields = 63
    fields = []
    for i in range(1, nfields + 1):
        vals = [(r[4][i - 1] if i - 1 < len(r[4]) else 0) for r in lst]
        sv = set(vals)
        fields.append({'off': 4 * i, 'kind': kind(vals), 'distinct': len(sv), 'min': min(vals), 'max': max(vals), 'zero_frac': round(sum(1 for v in vals if v == 0) / len(vals), 2)})
    model[vt] = {'count': len(lst), 'missions': sorted({m for m, *_ in lst}), 'sizes': {str(k): v for k, v in sizes.items()},
                 'methods': len(cl[vt]['methods']) if vt in cl else 0, 'ctors': cl[vt]['ctors'] if vt in cl else [],
                 'base': max(edges[vt], key=edges[vt].get) if vt in edges else None, 'fields': fields}
json.dump(model, open(out, 'w'), indent=0)
print('classes seen alive:', len(model), 'of', len(cl))
tot = sum(m['count'] for m in model.values())
print('objects:', tot)
for vt, m in sorted(model.items(), key=lambda kv: -kv[1]['count'])[:25]:
    print(vt, m['count'], 'missions', len(m['missions']), 'sizes', m['sizes'], 'methods', m['methods'], 'base', m['base'])
