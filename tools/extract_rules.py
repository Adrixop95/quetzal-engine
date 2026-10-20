#!/usr/bin/env python3 -I
"""Extract gameplay constant tables from game/Aztec.exe (static .data/.rdata) and a runtime dump (work/heap1.bin,
made by the script command `heapdump`; descriptor arrays are filled at game start). Writes docs/data/*.csv.
usage: tools/extract_rules.py [heapdump]"""
import struct, sys, csv, os
exe = open('game/Aztec.exe', 'rb').read()
heap = open(sys.argv[1] if len(sys.argv) > 1 else 'work/heap1.bin', 'rb').read()
def rx(a, n, f='I'): return struct.unpack_from('<%d%s' % (n, f), exe, a - 0x400000)
def rh(a, n, f='I'): return struct.unpack_from('<%d%s' % (n, f), heap, 4 + a - 0x49b000)
def cstr(a):
    o = a - 0x400000; return exe[o:exe.index(b'\0', o)].decode()
names = [cstr(rx(0x4acb4c + 4 * i, 1)[0])[4:] for i in range(90)]
kinds = ['Marine', 'Spider', 'Building', 'Zeppelin', 'Truck', 'Bird', 'Headtruck', 'Defence', 'Boat', 'Cannon', 'Johnka', 'Camera']
price = rx(0x4bc3e8, 90); btime = rx(0x4bc550, 90); dev = rh(0x515580, 90)
out = 'docs/data/'
def w(fn, hdr, rows):
    with open(out + fn, 'w', newline='') as f:
        c = csv.writer(f, lineterminator='\n'); c.writerow(hdr); c.writerows(rows)
rows = []
for i in range(90):
    k, p = rx(0x4933f8 + 8 * i, 2)
    rows.append([i, names[i], kinds[k], p, price[i] if price[i] < 2**31 - 1 else -1, btime[i] if btime[i] < 2**31 - 1 else -1, dev[i]])
w('types.csv', ['id', 'ugi', 'kind', 'param', 'unit_price', 'build_time', 'dev_level'], rows)
# build tree: [race 3][town hall type 4][33 entries][34 dwords]; dword0 item, then prereqs (0xffffffff or >=0x5a ends)
rows = []
for r in range(3):
    for g in range(4):
        for e in range(33):
            v = rx(0x4af130 + r * 0x4620 + g * 0x1188 + e * 0x88, 34)
            if v[0] == 0xffffffff: break
            pre = []
            for x in v[1:]:
                if x == 0xffffffff or (x >= 0x5a and not x & 0xfc000000): break
                pre.append(x)
            rows.append([r, g, e, v[0] & 0x3ffffff, int(bool(v[0] & 0x4000000)), ' '.join('%d%s' % (x & 0x3ffffff, 'A' if x & 0x8000000 else '') for x in pre)])
w('buildtree.csv', ['race', 'townhall_type', 'slot', 'item_ugi', 'flagB_construct', 'prereq_ugi(A=flag 0x8000000)'], rows)
w('townhall_ids.csv', ['townhall_type', 'rus', 'aztec', 'chinese'], [[t] + [(lambda x: -1 if x == 0xffffffff else x)(rx(0x4bc390 + 4 * (3 * t + r), 1)[0]) for r in range(3)] for t in range(4)])
fl = rx(0x4bc3c0, 8, 'f') + rh(0x4bc3e0, 2, 'f')
w('town_factors.csv', ['name', 'value'], [['Ktime[0..2]', ' '.join('%g' % x for x in fl[0:3])], ['Kprice[0..2]', ' '.join('%g' % x for x in fl[3:6])], ['KWarehouse[0..1]', ' '.join('%g' % x for x in fl[6:8])], ['KAdvPostTime', '%g' % fl[8]], ['KAdvPostPrice', '%g' % fl[9]]])
# descriptors (runtime dump)
D = [('marine', 0x4ac430, 0x4c, 16), ('building', 0x49b538, 0x508, 32), ('defence', 0x4aa3b0, 0x110, 9), ('headtruck', 0x4ab2b0, 0x50, 8),
     ('truck', 0x4bc738, 0x44, 1), ('spider', 0x4ae378, 0x4c, 2), ('zeppelin', 0x4bcd00, 0x44, 2), ('bird', 0x49b370, 0x58, 3), ('cannon', 0x4a8e18, 0x40, 1)]
for nm, base, st, n in D:
    rows = []
    for k in range(n):
        v = rh(base + k * st, st // 4, 'i')
        rows.append([k] + list(v))
    w('desc_%s.csv' % nm, ['param'] + ['d%d' % i for i in range(st // 4)], rows)
# weapons: id -> (class, param), class data tables (static .rdata)
rows = [[i] + list(rx(0x494d30 + 8 * i, 2, 'i')) for i in range(60)]
w('weapons_ids.csv', ['weapon_id', 'class', 'param'], rows)
WT = {'ball0': (0x48f868, 20, 5, 13), 'arrow1': (0x48f408, 64, 16, 8), 'axe3': (0x48f750, 24, 6, 4), 'w7': (0x494218, 36, 9, 8), 'w9': (0x493f60, 32, 8, 4),
      'w12': (0x4906f0, 12, 3, 8), 'w2_13': (0x494a90, 56, 14, 12), 'w8': (0x4ae448, 16, 4, 4)}
for nm, (b, st, nw, n) in WT.items():
    w('weapon_class_%s.csv' % nm, ['param'] + ['d%d' % i for i in range(nw)], [[k] + list(rx(b + k * st, nw, 'i')) for k in range(n)])
