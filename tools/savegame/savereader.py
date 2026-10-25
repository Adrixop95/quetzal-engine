#!/usr/bin/env python3 -I
"""Reference reader for Aztec Empire save games (Saves/singleN.sav, multiN.sav).

Every function mirrors one serializer of the original executable (address in the comment); the layout was derived from
the disassembly of the writer (FUN_004450e0 and the per-class vtable slots) and verified word by word against a
WriteFile trace of the recompiled game (runtime RT_WRITE_TRACE).  Each stream element is a little-endian u32; "memory
offset" names (+0x24 ...) are offsets inside the in-memory object the value was read from.

usage: savereader.py FILE.sav [-v]     parse and report
"""
import struct, sys, os

_exe = None
def _exe_bytes():
    global _exe
    if _exe is None:
        root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
        _exe = open(os.path.join(root, 'game/Aztec.exe'), 'rb').read()
    return _exe
def _va(a): return a - 0x400000
def table(va, n, fmt='<2i'):
    d = _exe_bytes(); sz = struct.calcsize(fmt)
    return [struct.unpack_from(fmt, d, _va(va) + sz * i) for i in range(n)]
def type_kind(t): return table(0x4933f8, 90)[t][0]      # unit type -> world object kind
def weapon_kind(t): return table(0x494d30, 40)[t][0]    # weapon type -> weapon class kind

class Cur:
    """Word cursor that records every field read: (stream offset, label, memory offset, kind, value)."""
    def __init__(s, data, pos=0): s.d = data; s.p = pos; s.log = []; s.last = {}
    def u(s, label=None, mem=None, kind='u32'):
        v = struct.unpack_from('<I', s.d, s.p)[0]
        s.log.append((s.p, label, mem, kind, v)); s.p += 4
        if mem is not None: s.last[mem] = v
        return v
    def buf(s, n, label=None, mem=None):
        b = s.d[s.p:s.p + n]
        if len(b) != n: raise EOFError('buffer past end of file at %d' % s.p)
        s.log.append((s.p, label, mem, 'buf%d' % n, None)); s.p += n; return b

def U(r, *offs):
    for o in offs: r.u('+0x%x' % o, o, 'u32')
def B(r, *offs):                       # byte member written as a u32
    for o in offs: r.u('+0x%x' % o, o, 'u8')
def Wd(r, *offs):                      # signed 16-bit member written as a u32
    for o in offs: r.u('+0x%x' % o, o, 'i16')
def REF(r, k, o): return r.u('+0x%x' % o, o, 'ref_' + k)   # pool index + 1 (0 = null)
def DESC(r, o=None): r.u('desc', o, 'desc')                  # index of the static type-descriptor record
def PT(r, o): r.u('+0x%x' % o, o, 'u32'); r.u('+0x%x' % (o + 4), o + 4, 'u32')

def PATH(r, name):
    """Path-finder state object (WaveSearch): 5 or 7 words."""
    r.u(name + '.id', None, 'u32')
    if r.u(name + '.has_steps', None, 'bool'):
        r.u(name + '.cur', None, 'step_index'); r.u(name + '.end', None, 'step_index')
    r.u(name + '.b_0c', None, 'u8'); r.u(name + '.i_14', None, 'u32'); r.u(name + '.b_18', None, 'u8')

# ---------------------------------------------------------------- world objects (pool 0x4cc3d0)
def s_46ae80(r): U(r, 0x10, 0x14, 0x18, 0x1c); B(r, 0x20); U(r, 0x24, 0x28); B(r, 0x2c)   # VisibleThing base
def s_43e5b0(r):                                                                           # world object base
    s_46ae80(r); REF(r, 'obj', 0x30); U(r, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x48); Wd(r, 0x4c); U(r, 0x50, 0x54, 0x58); B(r, 0x5c, 0x5d, 0x5e, 0x5f)
def s_marine(r):                                                                           # 432ba0 kind 0
    s_43e5b0(r); B(r, 0x60); U(r, 0x64, 0x6c, 0x70, 0x74, 0x78, 0x80, 0x84, 0x88)
    REF(r, 'unit', 0x8c); REF(r, 'unit', 0x90); REF(r, 'unit', 0x94); U(r, 0x98, 0x9c); REF(r, 'cb0', 0xa0); U(r, 0xa4)
    DESC(r, 0x68); PATH(r, 'path@+0x7c')
def s_spider(r):                                                                           # 449c30 kind 1
    s_43e5b0(r); B(r, 0x60); U(r, 0x64, 0x68); PT(r, 0x78); PT(r, 0x80); U(r, 0x88, 0x8c); B(r, 0x90, 0x91, 0x92); U(r, 0x94)
    REF(r, 'unit', 0x98); REF(r, 'unit', 0x9c); REF(r, 'unit', 0xa0); DESC(r); PATH(r, 'path@+0x70'); PATH(r, 'path@+0x74')
def s_sub42b240(r): REF(r, 'ce0', 4); U(r, 8, 0xc)
def s_building(r):                                                                         # 40f3a0 kind 2
    s_43e5b0(r); B(r, 0x60, 0x8c); U(r, 0x68, 0x78, 0x7c); REF(r, 'c60', 0x80); REF(r, 'cb0', 0x84); REF(r, 'd10', 0x88); DESC(r)
    PATH(r, 'path@+0x6c')
    if r.u('has@+0x70', None, 'bool'): s_sub42b240(r)
    if r.u('has@+0x74', None, 'bool'): s_sub42b240(r)
def s_zeppelin(r):                                                                         # 470e80 kind 3
    s_43e5b0(r); U(r, 0x60, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x84); B(r, 0x88); REF(r, 'unit', 0x90); DESC(r); PATH(r, 'path@+0x8c')
def s_truck(r):                                                                            # 45fa80 kind 4
    s_43e5b0(r); B(r, 0x60); U(r, 0x64, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x84); REF(r, 'unit', 0x88); REF(r, 'unit', 0x8c)
    B(r, 0x90, 0x91); U(r, 0x98); DESC(r); PATH(r, 'path@+0x94')
def s_bird(r):                                                                             # 406f40 kind 5
    s_43e5b0(r); U(r, 0x60, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x84); B(r, 0x88); U(r, 0x8c, 0x90); REF(r, 'unit', 0x94); DESC(r); PATH(r, 'path@+0x98')
def s_headtruck(r):                                                                        # 42b790 kind 6
    s_43e5b0(r); B(r, 0x60); U(r, 0x64, 0x68, 0x70, 0x74, 0x78, 0x7c, 0x80, 0x84, 0x88, 0x8c); REF(r, 'unit', 0x90); REF(r, 'unit', 0x94)
    B(r, 0x98); U(r, 0xa4, 0xa8, 0xac); DESC(r); PATH(r, 'path@+0x9c'); PATH(r, 'path@+0xa0')
def s_defence(r):                                                                          # 41c6c0 kind 7
    s_43e5b0(r); B(r, 0x60, 0x9c); U(r, 0x64, 0x68); REF(r, 'unit', 0x78); REF(r, 'unit', 0x7c); U(r, 0x80, 0x84, 0x88, 0x8c)
    REF(r, 'c60', 0x90); REF(r, 'cb0', 0x94); REF(r, 'd10', 0x98); DESC(r); PATH(r, 'path@+0x70')
    if r.u('has@+0x74', None, 'bool'): PATH(r, 'path@+0x74')
def s_boat(r): s_43e5b0(r); U(r, 0x60, 0x68); PATH(r, 'path@+0x64')                       # 408250 kind 8
def s_cannon(r):                                                                           # 415a80 kind 9
    s_43e5b0(r); B(r, 0x60); U(r, 0x64); PT(r, 0x70); PT(r, 0x78); B(r, 0x80, 0x81); U(r, 0x84)
    REF(r, 'unit', 0x88); REF(r, 'unit', 0x8c); REF(r, 'unit', 0x90); DESC(r); PATH(r, 'path@+0x6c')
def s_johnka(r): s_43e5b0(r); U(r, 0x60, 0x68); REF(r, 'unit', 0x6c); U(r, 0x70); PATH(r, 'path@+0x64')   # 42d7a0 kind 10
def s_camera(r): s_43e5b0(r)                                                               # kind 11 (UGI_CAMERA)
KIND = {0: s_marine, 1: s_spider, 2: s_building, 3: s_zeppelin, 4: s_truck, 5: s_bird, 6: s_headtruck, 7: s_defence,
        8: s_boat, 9: s_cannon, 10: s_johnka, 11: s_camera}
KIND_NAME = {0: 'Marine', 1: 'Spider', 2: 'Building', 3: 'Zeppelin', 4: 'Truck', 5: 'Bird', 6: 'Headtruck', 7: 'Defence',
             8: 'Boat', 9: 'Cannon', 10: 'Johnka', 11: 'Camera'}

# ---------------------------------------------------------------- tactics / towns (pool 0x4d9b50, 0x5156f8)
def s_tactic(r):                                                                           # 44e0f0
    r.u('global@4d9b78', None, 'u32')
    U(r, 4); REF(r, 'c30', 8); U(r, 0x10, 0x14, 0x1c, 0x20, 0x24); Wd(r, 0x28); U(r, 0x2c, 0x30, 0x34, 0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50, 0x54)
    REF(r, 'unit', 0x58); B(r, 0x60, 0x61); U(r, 0x64, 0x68); B(r, 0x6c, 0x6d); U(r, 0x70, 0x78, 0x7c); REF(r, 'unit', 0x80); U(r, 0x84, 0x88)
    B(r, 0x8c, 0x94); REF(r, 'obj', 0x98); B(r, 0x9c, 0x9d); U(r, 0xac); B(r, 0xb0); U(r, 0xb4); B(r, 0xb8); U(r, 0xbc, 0xc0)
    Wd(r, 0xc4, 0xc6); B(r, 0xd8, 0xd9, 0xe8, 0xe9, 0xea); U(r, 0xec, 0x90); B(r, 0x18)
    n = r.u('list@+0xc.count', None, 'count')                       # members: 11 words each
    for i in range(n):
        REF(r, 'unit', 0); U(r, 4, 8, 0xc, 0x10); B(r, 0x14, 0x15); U(r, 0x16, 0x1c, 0x20, 0x24)
    for i in range(r.last[0x3c]):                                   # array at +0x38, 12 bytes each
        for k in range(3): r.u('arr@+0x38[%d].%d' % (i, k))
    n = r.u('list@+0x5c.count', None, 'count')
    for i in range(n):
        if i == 0 and r.last[0x60]: r.u('list@+0x5c[0]', None, 'ref_unit')
        else: r.u('list@+0x5c[%d]' % i, None, 'ref_obj')
    if r.u('has@+0x74', None, 'bool'):                              # route table: blocks of 0x19c bytes
        c7c, c78 = r.last[0x7c], r.last[0x78]
        for i in range(c7c): r.u('rt[%d].unit' % i, None, 'ref_unit'); r.u('rt[%d].1' % i); r.u('rt[%d].2' % i)
        for k in range(c78): r.u('col[%d].unit' % k, None, 'ref_unit'); r.u('col[%d].1' % k)
        for i in range(c7c):
            for j in range(c78): r.u('rt[%d][%d]' % (i, j))
    n = r.u('list@+0xc8.count', None, 'count')
    for i in range(n): r.u('lc8[%d].0' % i); r.u('lc8[%d].1' % i)
    n = r.u('list@+0xdc.count', None, 'count')
    for i in range(n): r.u('ldc[%d].0' % i); r.u('ldc[%d].1' % i)
    n = r.u('list@+0xa0.count', None, 'count')
    for i in range(n): r.u('la0[%d]' % i, None, 'ref_obj')
def s_queue(r):                                                                            # 411820 ring buffer of unit refs
    cnt = r.u('queue.count'); r.u('queue.cap'); r.u('queue.c'); r.u('queue.head')
    for i in range(cnt): r.u('queue[%d]' % i, None, 'ref_unit')
def s_40a580(r): r.u('+8'); r.u('+0xc'); r.u('+0x10'); r.u('b+0x14', None, 'u8'); r.u('unit(+4)', None, 'ref_unit')
def s_444b20(r):
    r.u('+0x28'); r.u('+0x2c'); n = r.u('+0x18', None, 'count')
    for i in range(n): r.u('list[%d]' % i, None, 'ref_unit')
def s_411f50(r):                                                                           # BuildTree block
    r.buf(0x16c, 'buf@+0x16c'); r.buf(0x168, 'buf@+0x4'); r.buf(0x1188, 'buf@+0x2e8'); r.u('desc(+0x1470)', None, 'desc')
    r.u('b+0x2e0', None, 'u8'); r.u('b+0x2e1', None, 'u8'); r.u('b+0x2e2', None, 'u8'); r.u('+0x2e4')
def s_town(r):                                                                             # 45d820, derives from Tactic
    s_tactic(r)
    REF(r, 'unit', 0xf0); U(r, 0x158, 0x15c, 0x160, 0x164); r.u('flag_a', None, 'bool'); B(r, 0x170, 0x171)
    U(r, 0x174, 0x17c, 0x180, 0x18c, 0x190, 0x194, 0x198, 0x1b0, 0x1b4); r.u('global@515721', None, 'u8'); U(r, 0x178)
    REF(r, 'obj', 0x16c); r.buf(0x14, 'buf@+0x19c')
    s_queue(r); s_40a580(r); s_444b20(r); r.u('flag_b', None, 'bool'); r.u('flag_c', None, 'bool'); s_411f50(r)
def s_45ef30(r):
    n = r.u('count', None, 'count')
    for i in range(n): r.u('ref', None, 'ref_obj'); r.buf(0x168, 'blk')
def s_side(r):                                                                             # 449540 (Side, 0x69c bytes)
    U(r, 4, 8, 0xc, 0x10, 0x14); B(r, 0x138); U(r, 0x13c, 0x140, 0x698, 0x63c, 0x640, 0x644, 0x648, 0x64c, 0x650)
    r.u('flag_a', None, 'bool'); r.u('flag_b', None, 'bool')
    r.buf(0x20, 'name@+0x18'); r.buf(0x100, 'buf@+0x38'); r.buf(0x16c, 'buf@+0x144'); r.buf(0x5b, 'disabled@+0x2b0')
    r.buf(0x16c, 'buf@+0x30c'); r.buf(0x168, 'buf@+0x478'); r.buf(0x5a, 'buf@+0x5e0')
    for nm, k in (('list@+0x654', 'obj'), ('list@+0x660', 'obj'), ('list@+0x678', 'unit'), ('list@+0x66c', 'd40')):
        n = r.u(nm + '.count', None, 'count')
        for i in range(n): r.u('%s[%d]' % (nm, i), None, 'ref_' + k)
    if r.u('has_explorer@+0x68c', None, 'bool'): s_45ef30(r)
    w = r.u('fog.w'); h = r.u('fog.h'); r.buf(w * h, 'fog')
    n = r.u('cameras.count', None, 'count')
    for i in range(n): r.buf(0x14, 'camera[%d]' % i)

# ---------------------------------------------------------------- misc pools
def s_deco(r): s_46ae80(r); U(r, 0x34); PATH(r, 'path@+0x30')                             # 418760 (pool 0x4c1978)
def s_wbase(r): s_46ae80(r); REF(r, 'unit', 0x30); U(r, 0x34)                              # 46d840 weapon base
def s_weapon(r, kind):                                                                     # pool 0x515a98
    s_wbase(r)
    if kind == 0: U(r, 0x38, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x58, 0x5c, 0x60, 0x64); DESC(r); PATH(r, 'path@+0x40')   # 406440
    elif kind == 1: U(r, 0x38, 0x40, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x58, 0x5c, 0x60); REF(r, 'unit', 0x64); DESC(r); PATH(r, 'path@+0x68')  # 403560 Arrow
    elif kind in (2, 9, 12, 13): DESC(r)
    elif kind == 3: U(r, 0x38, 0x40, 0x44, 0x48, 0x4c, 0x50, 0x54, 0x58); DESC(r); PATH(r, 'path@+0x5c')   # 404840 Axe
    elif kind in (4, 5, 6, 10): pass
    elif kind == 7: U(r, 0x3c, 0x44); DESC(r); PATH(r, 'path@+0x40')                    # 44bd00 SplashDamage
    elif kind == 8:                                                                         # 44c790 SplashDamage
        U(r, 0x38, 0x40, 0x104); DESC(r)
        for k in range(16): r.u('e%d.0' % k); r.u('e%d.1' % k); r.u('e%d.2' % k)
    elif kind == 11: U(r, 0x3c); PATH(r, 'path@+0x38')                                  # 429aa0 FireThrow
    elif kind == 14: REF(r, 'unit', 0x38); U(r, 0x3c, 0x40, 0x44)                       # 4046b0
    else: raise ValueError('weapon kind %d' % kind)
def s_401140(r):                                                                           # pool 0x4c1298 (7 words + double)
    for o in (4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c): U(r, o)
    r.buf(8, 'double')
def s_42a2a0(r):                                                                           # pool 0x4c4ce0
    s_46ae80(r); REF(r, 'unit', 0x30); U(r, 0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x4c); B(r, 0x114); U(r, 0x118, 0x11c, 0x120, 0x124)
    B(r, 0x128, 0x129, 0x12a, 0x12b); r.buf(0xc4, 'buf')
def s_trig(r, kind):                                                                       # pool 0x4cd698 (triggers, timers, paths)
    r.u('id(+4)')
    if kind == 0: r.u('+8')
    elif kind == 3: r.u('+8'); r.u('b+0xc', None, 'u8')
    elif kind == 2:                                                                        # Timer
        r.u('+8'); r.u('b+0xc', None, 'u8')
        if r.u('b+0x14', None, 'u8'):
            n = r.u('+0x20', None, 'count'); r.u('+0x1c'); r.buf(n * 4, 'samples')
        else: r.u('+0x18'); r.u('+0x1c')
    elif kind == 1:                                                                        # Trigger: condition and action packets
        r.u('+8'); r.u('b+0xc', None, 'u8')
        n = r.u('conds.count', None, 'count')
        for i in range(n): r.buf(0x408, 'cond[%d]' % i)
        n = r.u('acts.count', None, 'count')
        for i in range(n): r.buf(0x408, 'act[%d]' % i)
    elif kind == 4:                                                                        # scripted path
        n = r.u('points.count', None, 'count')
        for i in range(n): r.u('x%d' % i); r.u('y%d' % i)
    else: raise ValueError('trigger kind %d' % kind)
def s_4d9b88(r):                                                                           # pool 0x4d9b88 (order queues)
    r.u('ref@+0x80c', None, 'ref_obj'); r.u('ref@+0x808', None, 'ref_obj'); n = r.u('count@+0x804', None, 'count'); r.u('b+0x810', None, 'u8')
    for i in range(n):
        t = r.u('e%d.t' % i); r.u('e%d.v' % i, None, 'ref_unit' if t == 0 else 'u32')
def s_uilevel(r):
    n = r.u('count', None, 'count'); r.u('+0x28')
    for i in range(n): REF(r, 'obj', 0xc); B(r, 0x14); U(r, 0x10)
def s_ui(r):
    s_uilevel(r); s_uilevel(r)                                                             # interface object chain (0x4cc7ec)
    REF(r, 'obj', 0x4c); U(r, 0x50)                                                        # 0x4cc7e0 (440490)
    REF(r, 'obj', 0x4c); U(r, 0x11d8); r.buf(0x1188, 'buf')                                # 0x4cc7dc (4403a0)

def parse(d):
    r = Cur(d, 0); res = {'sections': {}}
    def sec(name): res['sections'][name] = r.p
    r.u('flag0'); r.u('mission(4dd164)'); r.buf(256, 'map_path'); r.buf(64, 'description'); r.buf(16, 'systime')
    r.u('view.x'); r.u('view.y')
    for i in range(5): r.u('global%d' % i)
    sec('decorations'); n = r.u('count', None, 'count')
    for i in range(n): r.u('idx', None, 'ref_c60'); s_deco(r)
    sec('towns'); n = r.u('count', None, 'count')
    for i in range(n): r.u('idx', None, 'ref_obj'); s_town(r)
    sec('sides'); n = r.u('count', None, 'count')
    for i in range(n): s_side(r)
    sec('tactics'); n = r.u('count', None, 'count')
    for i in range(n):
        r.u('idx', None, 'ref_obj'); k = r.u('kind')
        if k in (0, 5): s_tactic(r)
        elif k == 2: s_tactic(r); U(r, 0xf0); REF(r, 'obj', 0xf4); U(r, 0xf8); REF(r, 'c30', 0xfc)
        elif k == 3: s_tactic(r); REF(r, 'c60', 0xf0)
        elif k == 4: s_tactic(r); B(r, 0xf0, 0xf1)
        else: raise ValueError('tactic kind %d at %d' % (k, r.p))
    sec('units'); n = r.u('count', None, 'count')
    for i in range(n):
        r.u('idx', None, 'ref_unit'); t = r.u('type'); KIND[type_kind(t)](r)
    sec('weapons'); n = r.u('count', None, 'count')
    for i in range(n): t = r.u('type'); s_weapon(r, weapon_kind(t))
    sec('pool4c1298'); n = r.u('count', None, 'count')
    for i in range(n): r.u('idx', None, 'ref_cb0'); s_401140(r)
    sec('pool4c4ce0'); n = r.u('count', None, 'count')
    for i in range(n): r.u('idx', None, 'ref_d10'); s_42a2a0(r)
    sec('triggers'); n = r.u('count', None, 'count')
    for i in range(n): r.u('idx', None, 'ref_ce0'); k = r.u('kind'); s_trig(r, k)
    sec('orders'); n = r.u('count', None, 'count')
    for i in range(n): r.u('idx', None, 'ref_d40'); s_4d9b88(r)
    sec('ui'); s_ui(r)
    sec('daylight'); r.u('day.time'); r.u('day.stage')
    res['end'] = r.p; res['log'] = r.log
    return res

if __name__ == '__main__':
    d = open(sys.argv[1], 'rb').read()
    try:
        res = parse(d)
        print('%s: %d bytes, parsed %d -> %s' % (sys.argv[1], len(d), res['end'], 'OK' if res['end'] == len(d) else 'MISMATCH'))
        print(res['sections'])
    except Exception as e:
        print('%s: %d bytes: ERROR %s' % (sys.argv[1], len(d), e)); sys.exit(1)
