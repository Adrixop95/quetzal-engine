#!/usr/bin/env python3 -I
"""Map functions and classes to original source files using the assert/log strings embedded in the executable.
usage: source_map.py game/Aztec.exe text.asm work/gen/gen_funcs.h work/classes.json out.json"""
import re, sys, struct, json, bisect, collections
exe, asm, fh, cj, out = sys.argv[1:6]
d = open(exe, 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]; opt = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 52)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + opt + 40 * i
    vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
    secs.append((base + va, vs, ro, rs))
def file2va(fo):
    for va, vs, ro, rs in secs:
        if ro <= fo < ro + rs: return va + fo - ro
srcs = {}
for m in re.finditer(rb'[A-Za-z]:\\[\x20-\x7e]{3,120}\.(?:cpp|c|h|hpp)\x00', d):
    srcs[file2va(m.start())] = m.group(0)[:-1].decode().split('\\')[-1]
funcs = sorted(int(m, 16) for m in re.findall(r'f_([0-9a-f]{8})\(CPU', open(fh).read()))
pat = re.compile(r'\s*([0-9a-f]+):\t(?:[0-9a-f]{2} )+\s*\t?(.*)')
fn_file = collections.defaultdict(collections.Counter)
for l in open(asm):
    m = pat.match(l)
    if not m: continue
    t = m.group(2)
    mm = re.match(r'push\s+0x([0-9a-f]+)$', t.strip())
    if mm and int(mm.group(1), 16) in srcs:
        a = int(m.group(1), 16)
        i = bisect.bisect_right(funcs, a) - 1
        if i >= 0: fn_file[funcs[i]][srcs[int(mm.group(1), 16)]] += 1
cl = json.load(open(cj))
cls = {}
for vt, c in cl.items():
    votes = collections.Counter()
    for f in set(c['methods']) | set(c['ctors']):
        for fl, n in fn_file.get(int(f, 16), {}).items(): votes[fl] += n
    cls[vt] = votes.most_common(3)
res = {'sources': sorted(set(srcs.values())), 'func_file': {'%08x' % k: v.most_common(1)[0][0] for k, v in fn_file.items()}, 'class_files': cls}
json.dump(res, open(out, 'w'), indent=0)
print(len(srcs), 'source files;', len(fn_file), 'functions mapped;', sum(1 for v in cls.values() if v), 'of', len(cl), 'classes with a source file')
print(' '.join(res['sources']))
