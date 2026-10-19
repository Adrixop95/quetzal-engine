#!/usr/bin/env python3 -I
"""Allocation size per class: for every call to a constructor, find the preceding `push imm; call alloc`.
usage: ctor_sizes.py text.asm classes.json out.json   (text.asm = objdump -d -M intel of .text)"""
import re, sys, json, collections
asm, cj, out = sys.argv[1:4]
cl = json.load(open(cj))
c2v = {}
for vt, c in cl.items():
    for f in c['ctors']: c2v.setdefault(int(f, 16), []).append(vt)
ins = []
pat = re.compile(r'\s*([0-9a-f]+):\t(?:[0-9a-f]{2} )+\s*\t?(.*)')
for l in open(asm):
    m = pat.match(l)
    if m: ins.append((int(m.group(1), 16), m.group(2).strip()))
res = collections.defaultdict(collections.Counter)
allocs = collections.Counter()
for i, (a, t) in enumerate(ins):
    if not t.startswith('call'): continue
    m = re.match(r'call\s+0x([0-9a-f]+)', t)
    if not m: continue
    tgt = int(m.group(1), 16)
    if tgt not in c2v: continue
    for j in range(i - 1, max(i - 16, 0), -1):
        mc = re.match(r'call\s+0x([0-9a-f]+)', ins[j][1])
        if mc:
            mp = re.match(r'push\s+0x([0-9a-f]+)$', ins[j - 1][1])
            if mp:
                res[tgt][int(mp.group(1), 16)] += 1
                allocs[int(mc.group(1), 16)] += 1
            break
json.dump({'%08x' % k: dict(v) for k, v in res.items()}, open(out, 'w'))
print('ctors with size:', len(res), 'top allocators:', [(hex(a), n) for a, n in allocs.most_common(4)])
