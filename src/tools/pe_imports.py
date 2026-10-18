#!/usr/bin/env python3
"""List PE imports with IAT slot addresses: pe_imports.py Aztec.exe"""
import struct, sys
d = open(sys.argv[1], 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]
opt = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 52)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + opt + 40 * i
    vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
    secs.append((va, max(vs, rs), ro))
def off(rva):
    for va, sz, ro in secs:
        if va <= rva < va + sz: return ro + rva - va
imp_rva = struct.unpack_from('<I', d, pe + 24 + 96 + 8)[0]
o = off(imp_rva)
while True:
    ol, _, _, name, ft = struct.unpack_from('<IIIII', d, o)
    if not name: break
    dll = d[off(name):d.index(b'\0', off(name))].decode()
    t = off(ol or ft); slot = ft
    while True:
        v, = struct.unpack_from('<I', d, t)
        if not v: break
        if v & 0x80000000: fn = f'#{v & 0xffff}'
        else:
            q = off(v) + 2; fn = d[q:d.index(b'\0', q)].decode()
        print(f'{base + slot:#010x} {dll} {fn}')
        t += 4; slot += 4
    o += 20
