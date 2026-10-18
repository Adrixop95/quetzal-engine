#!/usr/bin/env python3
"""Reader for Aztec Empire Res/*.xbe resource archives (not Xbox executables)."""
import struct, sys

def load(path):
    d = open(path, 'rb').read()
    n, = struct.unpack_from('<I', d, 0)
    recs = [struct.unpack_from('<IIII', d, 4 + 16 * i) for i in range(n)]
    groups = []
    for i, (gid, tab, lo, hi) in enumerate(recs):
        cnt = hi - lo
        offs = list(struct.unpack_from('<%dI' % cnt, d, tab)) if cnt > 0 else []
        groups.append(dict(id=gid, table=tab, first=lo, last=hi, offs=offs))
    return d, groups

def items(d, groups):
    """Yield (group_id, index, bytes). Item end = next offset, else next group's first offset, else EOF."""
    starts = [g['offs'][0] for g in groups if g['offs']]
    for gi, g in enumerate(groups):
        nxt = starts[gi + 1] if gi + 1 < len(starts) else len(d)
        for k, o in enumerate(g['offs']):
            e = g['offs'][k + 1] if k + 1 < len(g['offs']) else nxt
            yield g['id'], g['first'] + k, d[o:e]

if __name__ == '__main__':
    d, g = load(sys.argv[1])
    for x in g:
        o = x['offs']
        ok = all(o[i] <= o[i + 1] for i in range(len(o) - 1)) and (not o or o[-1] <= len(d))
        print(f"id={x['id']:#x} range=[{x['first']},{x['last']}) count={len(o)} table={x['table']:#x} ok={ok}")
