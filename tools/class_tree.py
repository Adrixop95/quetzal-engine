#!/usr/bin/env python3 -I
"""Inheritance edges of the recovered classes.
usage: class_tree.py text.asm work/classes.json work/gen/gen_funcs.h out.json
A constructor that stores several vtables in sequence (inlined base constructors) gives the chain base -> derived;
a constructor that calls another constructor before its own store gives base = callee's final vtable."""
import re, sys, json, bisect, collections
asm, cj, fh, out = sys.argv[1:5]
cl = json.load(open(cj))
vts = {int(k, 16) for k in cl}
funcs = sorted(int(m, 16) for m in re.findall(r'f_([0-9a-f]{8})\(CPU', open(fh).read()))
pat = re.compile(r'\s*([0-9a-f]+):\t(?:[0-9a-f]{2} )+\s*\t?(.*)')
ins = []
for l in open(asm):
    m = pat.match(l)
    if m: ins.append((int(m.group(1), 16), m.group(2).strip()))
def owner(a):
    i = bisect.bisect_right(funcs, a) - 1
    return funcs[i] if i >= 0 else None
body = collections.defaultdict(list)
for a, t in ins:
    o = owner(a)
    if o: body[o].append((a, t))
ctors = {}   # ctor function -> ordered vtable stores
for f, lst in body.items():
    st = []
    for a, t in lst:
        m = re.match(r'mov\s+DWORD PTR \[[^\]]+\],0x([0-9a-f]+)$', t)
        if m and int(m.group(1), 16) in vts: st.append(int(m.group(1), 16))
    if st: ctors[f] = st
final = {f: st[-1] for f, st in ctors.items()}
edges = collections.defaultdict(collections.Counter)
for f, st in ctors.items():
    for a, b in zip(st, st[1:]):
        if a != b: edges[b][a] += 1
    first_store = None
    for a, t in body[f]:
        m = re.match(r'mov\s+DWORD PTR \[[^\]]+\],0x([0-9a-f]+)$', t)
        if m and int(m.group(1), 16) in vts: first_store = a; break
    for a, t in body[f]:
        if first_store is not None and a >= first_store: break
        m = re.match(r'call\s+0x([0-9a-f]+)', t)
        if m and int(m.group(1), 16) in final and final[int(m.group(1), 16)] != st[0]:
            edges[st[0]][final[int(m.group(1), 16)]] += 1
res = {'%08x' % k: {'%08x' % b: n for b, n in v.items()} for k, v in edges.items()}
json.dump(res, open(out, 'w'), indent=0)
print('classes with base edges:', len(res), 'of', len(vts))
