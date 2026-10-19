#!/usr/bin/env python3 -I
"""Coverage of lifted functions: tools/coverage.py work/gen/gen_funcs.h calls1.txt [calls2.txt ...]
calls files come from a build with -DAZTEC_TRACE_CALLS=ON (RT_CALLS_FILE). Prints per-4K bins and totals."""
import re, sys, collections
fs = sorted(int(m, 16) for m in re.findall(r'f_([0-9a-f]{8})\(CPU', open(sys.argv[1]).read()))
called = set()
for p in sys.argv[2:]:
    called |= {int(l.split()[0], 16) for l in open(p)}
hit = [f for f in fs if f in called]
eh = [f for f in fs if f >= 0x48b000]
print(f"functions {len(fs)}, executed {len(hit)} ({100*len(hit)//len(fs)}%)")
print(f"in 0x48b000+ (EH cleanup funclets, run only on exceptions): {len(eh)}, executed {sum(f in called for f in eh)}")
core = [f for f in fs if f < 0x48b000]
print(f"outside funclets: {len(core)}, executed {sum(f in called for f in core)}")
b = collections.defaultdict(lambda: [0, 0])
for f in core:
    k = f >> 12 << 12; b[k][0] += 1; b[k][1] += f in called
print("least covered 4K blocks (never or <10%):")
for k, (n, h) in sorted(b.items()):
    if n >= 20 and h * 10 < n: print(f"  {k:#x} {h}/{n}")
