#!/usr/bin/env python3
"""Map Ghidra functions to original source files via assert/log calls FUN_00475c20(level, file_string_va, line, 0)."""
import re, struct, sys, collections
exe = open(sys.argv[1], 'rb').read()
dec = open(sys.argv[2]).read()
# PE sections
pe = struct.unpack_from('<I', exe, 0x3c)[0]
nsec = struct.unpack_from('<H', exe, pe + 6)[0]
opt = struct.unpack_from('<H', exe, pe + 20)[0]
base = struct.unpack_from('<I', exe, pe + 52)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + opt + 40 * i
    vs, va, rs, ro = struct.unpack_from('<IIII', exe, o + 8)
    secs.append((base + va, vs, ro, rs))
def cstr(va):
    for a, vs, ro, rs in secs:
        if a <= va < a + vs:
            o = ro + va - a
            e = exe.index(b'\0', o)
            return exe[o:e].decode('latin1')
fn_re = re.compile(r'^// ==== ([0-9a-f]{8}) (\S+)$', re.M)
parts = fn_re.split(dec)
out = collections.defaultdict(list)
for i in range(1, len(parts), 3):
    addr, name, body = parts[i], parts[i + 1], parts[i + 2]
    files = collections.Counter(m.group(1) for m in re.finditer(r'FUN_00475(?:c20|dd0)\(\d+,(0x[0-9a-f]+),', body))
    if files:
        f = cstr(int(files.most_common(1)[0][0], 16))
        out[f.split('\\')[-1]].append(addr)
    else:
        out['?'].append(addr)
for f, a in sorted(out.items(), key=lambda x: -len(x[1])):
    print(f"{f}\t{len(a)}\t{a[0]}..{a[-1]}")
