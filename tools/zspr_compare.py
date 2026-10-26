#!/usr/bin/env python3
"""Compare active, dereferenced Anim frames with a reference screenshot.

Usage: zspr_compare.py GAME RUN [--objects objs.txt] [--shot shot_02.ppm]
       [--team 0] [--shade 10] [--origin X Y] [--report OUT.json]

Reports exact RGB565 matches on opaque body pixels, without searching for a
better frame, position, shade or direction. Occlusion, light gradients and
movement between objscan and shot can reduce scores. Alpha pixels are excluded.
The input files are read-only; the optional report contains ids/counts only.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import sprmap
import sprite
import xbe
from PIL import Image


def signed(value):
    return value - 0x100000000 if value & 0x80000000 else value


def compare(args):
    run = Path(args.run)
    objects = run / args.objects
    items = sprmap.load_units(args.game)
    screenshot = Image.open(run / args.shot).convert('RGB')
    if args.origin is None:
        start = next(line.split()[2:4] for line in (run / 'placed.txt').read_text().splitlines()
                     if line.startswith('# start'))
        origin = (int(start[0]) * 48 - 298, int(start[1]) * 48 - 300)
    else:
        origin = args.origin
    # Keep probe field offsets in sync with the engine's metadata header.
    header = Path(__file__).resolve().parents[1] / 'src/engine/data/render_rules.hpp'
    layouts = {}
    for match in re.finditer(r'\{"([^"]+)",\s*(0x[0-9a-f]+),\s*(0x[0-9a-f]+),\s*'
                             r'(0x[0-9a-f]+|0),\s*(0x[0-9a-f]+),\s*(0x[0-9a-f]+),\s*'
                             r'PositionSource::(\w+),\s*HeightSource::(\w+),\s*'
                             r'(0x[0-9a-f]+|0),', header.read_text()):
        name, vt, anim, second, x, y, position, height, height_offset = match.groups()
        layouts[int(vt, 16)] = (name, int(anim, 16), int(second, 16), int(x, 16), int(y, 16),
                               position, height, int(height_offset, 16))
    deref = {}
    for line in Path(str(objects) + '.deref').read_text().splitlines():
        words = line.split()
        deref[(int(words[0], 16), int(words[1], 16))] = [int(w, 16) for w in words[3:]]
    palette_data, groups = xbe.load(str(Path(args.game) / 'Res/Pal.xbe'))
    palettes = {i: data for group, i, data in xbe.items(palette_data, groups) if group == 0x450}
    result = []
    for line in objects.read_text().splitlines():
        words = line.split()
        if not words or words[0] == 'P':
            continue
        address, vt = int(words[0], 16), int(words[1], 16)
        if vt not in layouts:
            continue
        name, primary, second, xo, yo, position, height_source, height_offset = layouts[vt]
        raw = [int(w, 16) for w in words[3:]]
        def field(offset):
            return signed(raw[offset // 4])
        x, y = field(xo), field(yo)
        if position == 'Fixed256':
            x, y = x // 256, y // 256
        elif position == 'Fixed256Truncate':
            x = x // 256 if x >= 0 else -((-x) // 256)
            y = y // 256 if y >= 0 else -((-y) // 256)
        elif position == 'TileCentre':
            x, y = x * 48 + 24, y * 48 + 24
        for offset in (primary, second):
            if not offset:
                continue
            anim = deref.get((address, offset))
            if not anim or len(anim) < 6:
                continue
            aid, data, sequence_header, loop, current, tick = anim[:6]
            resource = items.get((0x515, aid))
            if not resource or resource[:4] != b'Anim' or current < data + 0x208:
                continue
            frame_offset = current - data
            if frame_offset + 32 > len(resource) or (frame_offset - 0x208) % 32:
                continue
            frame = struct.unpack_from('<8i', resource, frame_offset)
            image = items.get((0x44c, frame[0]))
            if not image:
                continue
            height_frame = frame
            if offset == second and height_source == 'Frame':
                base_anim = deref.get((address, primary))
                if not base_anim:
                    continue
                base_resource = items.get((0x515, base_anim[0]), b'')
                base_frame_offset = base_anim[4] - base_anim[1]
                if not (0x208 <= base_frame_offset <= len(base_resource) - 32):
                    continue
                height_frame = struct.unpack_from('<8i', base_resource, base_frame_offset)
            h = height_frame[6] if height_source == 'Frame' else 0
            if height_source in ('ObjectPixels', 'ObjectFixed256'):
                h = field(height_offset)
                if height_source == 'ObjectFixed256':
                    h //= 256
            if image[8] & 0x40:
                w, ih, ox, oy, flags, rows = sprmap.decode40(image)
            else:
                w, ih, ox, oy, flags, rows = sprite.decode(image)
            palette_id = struct.unpack_from('<H', image, 12)[0]
            if palette_id < 4:
                palette_id += 16 + 4 * args.team
            palette = palettes[palette_id]
            shade = min(args.shade, len(palette) // 512 - 1)
            palette = struct.unpack_from('<256H', palette, shade * 512)
            matched = count = close = 0
            for py, row in enumerate(rows):
                for px, value in enumerate(row):
                    if value is None:
                        continue
                    if flags & 0x40:
                        value, alpha = value
                        if alpha != 255:
                            continue
                    sx, sy = x - ox - origin[0] + px, y - h - oy - origin[1] + py
                    if not (0 <= sx < min(598, screenshot.width) and 0 <= sy < screenshot.height):
                        continue
                    r, g, b = screenshot.getpixel((sx, sy))
                    observed = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                    count += 1
                    matched += observed == palette[value]
                    expected = palette[value]
                    close += (abs((observed >> 11) - (expected >> 11)) * 8 <= args.tolerance and
                              abs(((observed >> 5) & 63) - ((expected >> 5) & 63)) * 4 <= args.tolerance and
                              abs((observed & 31) - (expected & 31)) * 8 <= args.tolerance)
            slots = sprmap.parse_anim(resource)
            start = (sequence_header - data - 0x208) // 32 + 1
            slot = next((s for s, seq in slots.items() if seq['start'] == start), None)
            result.append(dict(object=f'{address:08x}', kind=name, component=hex(offset), anim=aid,
                               slot=slot, frame=(frame_offset - 0x208) // 32 - start,
                               tick=tick, sprite=frame[0], height=h, pixels=count, matched=matched,
                               score=matched / count if count else None,
                               tolerance=args.tolerance, within_tolerance=close,
                               tolerance_score=close / count if count else None))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game')
    parser.add_argument('run')
    parser.add_argument('--objects', default='objs.txt')
    parser.add_argument('--shot', default='shot_02.ppm')
    parser.add_argument('--team', type=int, choices=range(8), default=0)
    parser.add_argument('--shade', type=int, choices=range(20), default=10)
    parser.add_argument('--origin', type=int, nargs=2)
    parser.add_argument('--report')
    parser.add_argument('--tolerance', type=int, choices=range(256), default=0)
    args = parser.parse_args()
    result = compare(args)
    output = json.dumps(result, indent=2) + '\n'
    if args.report:
        Path(args.report).write_text(output)
    print(output, end='')
