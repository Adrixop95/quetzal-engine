#!/usr/bin/env python3 -I
"""Recover C++ classes of Aztec.exe from vtables: usage  analyze_classes.py game/Aztec.exe work/gen/gen_funcs.h out.json
Output (derived data, do not commit): vtables, constructors (functions that store a vtable address into [reg]),
vtable method lists, direct base candidates (ctor that calls another ctor before storing its own vtable)."""
import re, struct, sys, json, subprocess, collections
exe, funcs_h, out = sys.argv[1:4]
d = open(exe, 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]
opt = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 52)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + opt + 40 * i
    name = d[o:o+8].rstrip(b'\0').decode()
    vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
    secs.append((name, base + va, vs, ro, rs))
def rd(a):
    for n, va, vs, ro, rs in secs:
        if va <= a < va + vs and a - va < rs: return struct.unpack_from('<I', d, ro + a - va)[0]
    return None
funcs = sorted(int(m, 16) for m in re.findall(r'f_([0-9a-f]{8})\(CPU', open(funcs_h).read()))
fset = set(funcs)
def owner(a):
    import bisect
    i = bisect.bisect_right(funcs, a) - 1
    return funcs[i] if i >= 0 else None
# constructors / vtable starts: mov dword ptr [reg], imm32 (C7 /0) where imm32 points at a dword that looks like a function address
text = None
for n, va, vs, ro, rs in secs:
    if n == '.text': text = (va, ro, rs)
va, ro, rs = text
code = d[ro:ro + rs]
stores = collections.defaultdict(list)
for m in re.finditer(rb'\xc7[\x00-\x07\x40-\x47\x80-\x87]', code):
    i = m.start()
    modrm = code[i + 1]
    mod = modrm >> 6; rm = modrm & 7
    if mod == 0 and rm == 5: continue
    ln = 2 + (1 if rm == 4 else 0) + (1 if mod == 1 else 4 if mod == 2 else 0)
    j = i + ln
    if j + 4 > len(code): continue
    imm = struct.unpack_from('<I', code, j)[0]
    first = rd(imm)
    if first is not None and imm >= 0x48f000 and first in fset:
        stores[imm].append(owner(va + i))
starts = sorted(stores)
sset = set(starts)
vtables = {}
for a in starts:
    run = []
    b = a
    while True:
        v = rd(b)
        if v is None or not (0x401000 <= v < 0x48e000) or (b != a and b in sset): break
        run.append(v); b += 4
    vtables[a] = run
ctors = stores
res = {}
for vt, ms in vtables.items():
    res['%08x' % vt] = {'methods': ['%08x' % x for x in ms], 'ctors': sorted({'%08x' % c for c in ctors.get(vt, []) if c})}
json.dump(res, open(out, 'w'), indent=0)
n_with = sum(1 for v in res.values() if v['ctors'])
print(f'{len(res)} vtable candidates, {n_with} with constructors')
