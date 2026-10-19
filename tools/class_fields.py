#!/usr/bin/env python3 -I
"""Field catalog per class from Ghidra decompiler output: class_fields.py work/classes.json work/decomp/all.c out.md
Uses constructors and vtable methods whose first parameter is `this`. Writes addresses/offsets only (no decompiled code)."""
import re, json, sys, collections
cj, allc, out = sys.argv[1:4]
classes = json.load(open(cj))
txt = open(allc).read()
parts = re.split(r'\n// ==== ([0-9a-f]{8}) (\S+)\n', txt)
funcs = {}
for i in range(1, len(parts), 3): funcs[parts[i]] = parts[i + 2]
ctor_of = {}
for vt, c in classes.items():
    for f in c['ctors']: ctor_of.setdefault(f, []).append(vt)
def this_name(body):
    m = re.search(r'\(\s*(?:void|int|undefined\d?|uint)\s*\*+\s*(this|param_1)\b', body[:400])
    return m.group(1) if m else None
fre = re.compile(r'\*\((\w[\w ]*?) ?\*\)\(\(int\)(?:this|param_1) \+ (0x[0-9a-f]+|\d+)\)')
idx = re.compile(r'\b(?:this|param_1)\[(0x[0-9a-f]+|\d+)\]')
lines = []
for vt in sorted(classes):
    c = classes[vt]
    fields = collections.defaultdict(lambda: [collections.Counter(), 0])
    used = []
    for f in set(c['methods']) | set(c['ctors']):
        body = funcs.get(f)
        if not body: continue
        used.append(f)
        for m in fre.finditer(body):
            off = int(m.group(2), 0)
            fields[off][0][m.group(1)] += 1; fields[off][1] += 1
        for m in idx.finditer(body):
            off = int(m.group(1), 0) * 4
            fields[off][0]['dword[]'] += 1; fields[off][1] += 1
    base = []
    for f in c['ctors']:
        body = funcs.get(f, '')
        for g in re.findall(r'(FUN_[0-9a-f]{8})\((?:this|param_1)', body):
            ga = g[4:]
            if ga in ctor_of: base += ctor_of[ga]
    lines.append(f"## class vtable {vt}  ctors {','.join(c['ctors'])}  methods {len(c['methods'])}"
                 + (f"  base-candidates {','.join(sorted(set(base) - {vt}))}" if set(base) - {vt} else ''))
    lines.append('methods: ' + ' '.join(c['methods']))
    lines.append('decompiled members: %d of %d' % (len(used), len(set(c['methods']) | set(c['ctors']))))
    for off in sorted(fields):
        ty, n = fields[off]
        lines.append(f"  +{off:#x}: {', '.join(t for t, _ in ty.most_common(3))} (refs {n})")
    lines.append('')
open(out, 'w').write('\n'.join(lines))
print('classes', len(classes))
