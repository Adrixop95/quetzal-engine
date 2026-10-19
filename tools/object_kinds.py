#!/usr/bin/env python3 -I
"""Object factory table of Aztec.exe (NewUnit switch): object_kinds.py game/Aztec.exe work/decomp/all.c work/classes.json
Prints: kind -> constructor -> vtable, and the type table at 0x4933f8 (kind, parameter) per object type."""
import re, struct, json, sys, collections
exe, allc, cj = sys.argv[1:4]
d = open(exe, 'rb').read()
def rd(a): o = a - 0x48f000 + 0x8f000; return struct.unpack_from('<I', d, o)[0]
txt = open(allc).read()
i = txt.index('// ==== 0043d5d0'); j = txt.index('// ====', i + 10)
cases = re.findall(r'case (\d+):.*?(FUN_[0-9a-f]{8})\(piVar2', txt[i:j], re.S)
cl = json.load(open(cj))
c2v = {}
for vt, c in cl.items():
    for f in c['ctors']: c2v.setdefault(f, []).append(vt)
types = []
for t in range(0, 400):
    k, p = rd(0x4933f8 + t * 8), rd(0x4933f8 + t * 8 + 4)
    if k > 20: break
    types.append((t, k, p))
cnt = collections.Counter(k for _, k, _ in types)
print('object types:', len(types))
for k, f in sorted((int(k), f[4:]) for k, f in cases):
    vts = [v for v in c2v.get(f, []) if len(cl[v]['methods']) > 0]
    print(f'kind {k:2d}: ctor {f}  vtables {",".join(vts)}  types {cnt[k]}: ' + ' '.join(str(t) for t, kk, _ in types if kk == k))
