#!/usr/bin/env python3 -I
"""Function names from trace-guard calls (FUN_00444470(&guard, level, "name")) found in the executable.
usage: func_names.py game/Aztec.exe text.asm work/gen/gen_funcs.h out.json"""
import re, sys, struct, json, bisect, collections
exe, asm, fh, out = sys.argv[1:5]
d = open(exe, 'rb').read()
def cstr(a):
    o = a - 0x48f000 + 0x8f000
    if not (0 <= o < len(d)): return None
    s = d[o:o + 80].split(b'\0')[0]
    return s.decode('latin1') if s and all(32 <= c < 127 for c in s) else None
funcs = sorted(int(m, 16) for m in re.findall(r'f_([0-9a-f]{8})\(CPU', open(fh).read()))
pat = re.compile(r'\s*([0-9a-f]+):\t(?:[0-9a-f]{2} )+\s*\t?(.*)')
ins = []
for l in open(asm):
    m = pat.match(l)
    if m: ins.append((int(m.group(1), 16), m.group(2).strip()))
names = collections.defaultdict(set)
for i, (a, t) in enumerate(ins):
    m = re.match(r'call\s+0x444470$', t)
    if not m: continue
    # string pushed in the preceding few instructions
    for j in range(i - 1, max(i - 8, 0), -1):
        mp = re.match(r'push\s+0x([0-9a-f]+)$', ins[j][1])
        if mp:
            s = cstr(int(mp.group(1), 16))
            if s and re.match(r'^[a-z]{2,5}_\w+$', s):
                f = funcs[bisect.bisect_right(funcs, a) - 1]
                names[f].add(s)
                break
res = {'%08x' % k: sorted(v) for k, v in names.items()}
json.dump(res, open(out, 'w'), indent=0)
print(len(res), 'functions named')
pref = collections.Counter(n.split('_')[0] for v in res.values() for n in v)
print(pref.most_common(30))
