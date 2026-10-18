"""Decoder for Aztec Empire sprite items (type 0x44c / 0x454).
Header: u32 magic 0xE0AC0000, u16 w, u16 h, u8 flags(+8), u8 +9, u8 +10, u8 +11, u32 +12, i16 ox, i16 oy (+0x14), row table (+0x18, 8 B/row: u32 ctrl_off, u32 pix_off).
flags: 0x01 raw bitmap (0x10: 16-bit px), 0x02 RLE 8-bit palette index, 0x40 RLE 16-bit color; 0x04/0x08 shading/translucency variants.
Returns (w, h, ox, oy, flags, rows); a pixel is None (transparent), an int palette index (8-bit) or an int 16-bit colour."""
import struct

class Bad(Exception): pass

def decode(b):
    magic, w, h, flags = struct.unpack_from('<IHHB', b, 0)
    ox, oy = struct.unpack_from('<hh', b, 0x14)
    rows = []
    if flags & 1:
        bpp = 2 if flags & 0x10 else 1
        base = 0x18
        for r in range(h):
            row = b[base + r * w * bpp: base + (r + 1) * w * bpp]
            if bpp == 2:
                row = [row[i] | row[i + 1] << 8 for i in range(0, len(row), 2)]
            rows.append(list(row))
        return w, h, ox, oy, flags, rows
    for r in range(h):
        ao, bo = struct.unpack_from('<II', b, 0x18 + 8 * r)
        row = []
        p, i = bo, ao
        if flags & 0x40:
            while len(row) < w:
                c = b[i]; i += 1
                n = c & 0x3f
                if c >= 0xc0:
                    for _ in range(n):
                        row.append(b[p] | b[p + 1] << 8); p += 2
                else:
                    row.extend([None] * (n if c >= 0x80 else c))
        else:
            draw = False
            while len(row) < w:
                n = b[i]; i += 1
                if draw:
                    row.extend(b[p:p + n]); p += n
                else:
                    row.extend([None] * n)
                draw = not draw
        rows.append(row[:w])
    return w, h, ox, oy, flags, rows
