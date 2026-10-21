#!/usr/bin/env python3 -I
"""Sprite/animation mapping extractor for Aztec Empire (ids and offsets only, no pixels).
usage:
  sprmap.py json GAME_DIR HEAPDUMP OUT.json   type -> anim ids, anim slot tables (from Res/Units.xbe 0x515)
  sprmap.py sheet GAME_DIR OUTDIR [type...]   PNG sheets (rows = anim slots, cols = frames) for eyeballing
HEAPDUMP = file written by script command 'heapdump' (data section after game init; descriptor tables live there).
Anim item: 'Anim', u32 ver=3, u32 slot[128] (start frame index, 0 = absent) at +8, frames of 32 bytes from +0x208.
Frame i = 0x208 + 32*i: i32 sprite, light, sound, fire, dx, dy, yoff, duration. Sequence ends at sprite==0.
The frame at index start-1 is a 'header' frame (its [2] = looping sound while state active)."""
import sys, struct, os, json
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'tools'))
import xbe, sprite

KIND = {0:(0x4ac430,0x4c,'Marine'),1:(0x4ae378,0x4c,'Spider'),2:(0x49b538,0x508,'Building'),3:(0x4bcd00,0x44,'Zeppelin'),
        4:(0x4bc738,0x44,'Truck'),5:(0x49b370,0x58,'Bird'),6:(0x4ab2b0,0x50,'Headtruck'),7:(0x4aa3b0,0x110,'Defence'),
        8:(None,0,'Boat'),9:(0x4a8e18,0x40,'Cannon'),10:(None,0,'Johnka'),11:(None,0,'Camera')}
FIXED_ANIM = {8: [23], 10: [29]}
TWO_ANIMS = {1, 6, 7}   # second anim id = record[2]

def load_units(game):
    d, g = xbe.load(os.path.join(game, 'Res/Units.xbe'))
    its = {(a, i): b for a, i, b in xbe.items(d, g)}
    return its

def parse_anim(b):
    assert b[:4] == b'Anim' and struct.unpack_from('<I', b, 4)[0] == 3
    slots = struct.unpack_from('<128I', b, 8)
    out = {}
    for s, v in enumerate(slots):
        if not v: continue
        hdr = struct.unpack_from('<8i', b, 0x208 + (v - 1) * 32)
        frames = []; i = v
        while 0x208 + i * 32 + 32 <= len(b):
            f = struct.unpack_from('<8i', b, 0x208 + i * 32)
            if f[0] == 0: break
            frames.append(list(f)); i += 1
        out[s] = {'start': v, 'header': list(hdr), 'frames': frames}
    return out

def type_table(heap_path, exe_path):
    h = open(heap_path, 'rb').read(); exe = open(exe_path, 'rb').read()
    def rd(a, n=1):
        if 0x49b000 <= a < 0x531000: return struct.unpack_from('<%dI' % n, h, 4 + a - 0x49b000)
        return struct.unpack_from('<%dI' % n, exe, a - 0x400000)
    def cstr(a):
        o = a - 0x400000; return exe[o:exe.index(b'\0', o)].decode()
    nm = rd(0x4acb4c, 90); res = []
    for t in range(90):
        k, p = rd(0x4933f8 + 8 * t, 2)
        e = {'type': t, 'name': cstr(nm[t]), 'kind': k, 'kind_name': KIND[k][2], 'param': p, 'anims': []}
        if k in FIXED_ANIM: e['anims'] = FIXED_ANIM[k]
        elif KIND[k][0] is not None:
            base, st, _ = KIND[k]; rec = rd(base + p * st, st // 4); e['record'] = [int(x) for x in rec]
            e['anims'] = [rec[0]] + ([rec[2]] if k in TWO_ANIMS else [])
        res.append(e)
    return res

def cmd_json(game, heap, out):
    its = load_units(game); types = type_table(heap, os.path.join(game, 'Aztec.exe'))
    anims = {}
    for (g, i), b in its.items():
        if g == 0x515 and b[:4] == b'Anim': anims[i] = parse_anim(b)
    # sprite header facts for every sprite id used (offsets, cell size, palette id) - no pixels
    used = {f[0] for a in anims.values() for s in a.values() for f in s['frames']}
    spr = {}
    for sid in sorted(used):
        e = {}
        for g, nm in ((0x44c, 'body'), (0x454, 'shadow')):
            b = its.get((g, sid))
            if b and len(b) >= 0x18:
                m, w, h, fl = struct.unpack_from('<IHHB', b, 0)
                pal, cw, ch, ox, oy = struct.unpack_from('<HHHhh', b, 0xc)
                e[nm] = {'w': w, 'h': h, 'flags': fl, 'pal': pal, 'cell': [cw, ch], 'ox': ox, 'oy': oy}
        spr[sid] = e
    json.dump({'types': types, 'anims': {str(k): {str(s): v for s, v in a.items()} for k, a in anims.items()}, 'sprites': {str(k): v for k, v in spr.items()}},
              open(out, 'w'), separators=(',', ':'))
    print('types', len(types), 'anims', len(anims), 'sprites', len(spr))

def decode40(b):
    """Flag 0x40: palette-indexed RLE with optional alpha (c<0x80 skip, 0x80-0xBF solid 1 byte/px, >=0xC0 blended 2 bytes/px: index, alpha).
    Rows hold (index, alpha) tuples or None."""
    m, w, h, fl = struct.unpack_from('<IHHB', b, 0); ox, oy = struct.unpack_from('<hh', b, 0x14); rows = []
    for r in range(h):
        ao, bo = struct.unpack_from('<II', b, 0x18 + 8 * r); row = []; p = bo; i = ao
        while len(row) < w:
            c = b[i]; i += 1; n = c & 0x3f
            if c < 0x80: row.extend([None] * c)
            elif c < 0xc0:
                for _ in range(n): row.append((b[p], 255)); p += 1
            else:
                for _ in range(n): row.append((b[p], b[p + 1])); p += 2
        rows.append(row[:w])
    return w, h, ox, oy, fl, rows

def pal_for(game, pid, shade=14):
    d, g = xbe.load(os.path.join(game, 'Res/Pal.xbe'))
    p = {i: c for a, i, c in xbe.items(d, g) if a == 0x450 and c}[pid]
    n = len(p) // 512; shade = min(shade, n - 1); out = []
    for i in range(256):
        v = p[shade * 512 + 2 * i] | p[shade * 512 + 2 * i + 1] << 8
        out.append((((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31))
    return out

def cmd_sheet(game, outdir, heap, types):
    from PIL import Image
    its = load_units(game); T = type_table(heap, os.path.join(game, 'Aztec.exe'))
    os.makedirs(outdir, exist_ok=True); pals = {}
    def img(sid):
        b = its.get((0x44c, sid))
        if not b or len(b) < 0x18: return None
        if b[8] & 0x40: w, h, ox, oy, fl, rows = decode40(b)
        else: w, h, ox, oy, fl, rows = sprite.decode(b)
        pid = struct.unpack_from('<H', b, 0xc)[0]
        if pid < 4: pid += 0x10 + 1 * 4   # team 1 palette block (assumed)
        if pid not in pals:
            try: pals[pid] = pal_for(game, pid)
            except Exception: pals[pid] = [(i, i, i) for i in range(256)]
        pal = pals[pid]; im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
        for y, r in enumerate(rows):
            for x, v in enumerate(r):
                if v is None: continue
                im.putpixel((x, y), (pal[v] + (255,)) if not (fl & 0x40) else (pal[v[0]] + (v[1],)))
        return im, ox, oy
    for e in T:
        if types and e['type'] not in types: continue
        for aid in e['anims']:
            a = parse_anim(its[(0x515, aid)]); CW = 96; rows = []
            for s, sq in a.items():
                rows.append((s, [f[0] for f in sq['frames']]))
            if not rows: continue
            cols = max(len(r[1]) for r in rows); sheet = Image.new('RGBA', (cols * CW, len(rows) * CW), (60, 90, 60, 255))
            for ry, (s, ids) in enumerate(rows):
                for cx, sid in enumerate(ids):
                    r = img(sid)
                    if r:
                        im, ox, oy = r
                        sheet.alpha_composite(im, (cx * CW + CW // 2 - ox, ry * CW + CW * 2 // 3 - oy)) if True else None
            sheet.save(os.path.join(outdir, 't%02d_%s_a%d.png' % (e['type'], e['name'][4:], aid)))

def cmd_hpp(game, heap, out):
    T = type_table(heap, os.path.join(game, 'Aztec.exe')); L = ['// Generated by tools/sprmap.py hpp (ids only). Do not edit.', '#pragma once', '#include <cstdint>', 'namespace aztec::data {',
        'struct TypeAnimInfo { const char* name; uint8_t kind; uint8_t param; uint16_t anim; uint16_t anim2; };  // anim2 = second anim (weapon/turret) or 0',
        'inline constexpr TypeAnimInfo kTypeAnims[90] = {']
    for e in T:
        a = e['anims'] + [0, 0]
        L.append('  {"%s", %d, %d, %d, %d},' % (e['name'], e['kind'], e['param'], a[0], a[1]))
    L += ['};', '}  // namespace aztec::data', '']
    open(out, 'w').write('\n'.join(L))

def cmd_animsheet(game, outdir, ids):
    """Sheets for raw anim ids (effects, decorations): tools/sprmap.py animsheet GAME OUTDIR id..."""
    global type_table
    import types as _t
    real = type_table
    type_table = lambda h, e: [{'type': i, 'name': 'UGI_A%d' % i, 'anims': [i]} for i in ids]
    try: cmd_sheet(game, outdir, None, set(ids))
    finally: type_table = real

if __name__ == '__main__':
    c = sys.argv[1]
    if c == 'json': cmd_json(*sys.argv[2:5])
    elif c == 'hpp': cmd_hpp(*sys.argv[2:5])
    elif c == 'animsheet': cmd_animsheet(sys.argv[2], sys.argv[3], [int(x) for x in sys.argv[4:]])
    elif c == 'sheet': cmd_sheet(sys.argv[2], sys.argv[3], os.environ.get('HEAP', 'work/zt_all/heap.bin'), {int(x) for x in sys.argv[4:]})
