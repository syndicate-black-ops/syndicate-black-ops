#!/usr/bin/env python3
"""Animated spray maker: a GIF to a tempdecal.wad (docs/ANIMATED_SPRAYS.md).

    scripts/gif_to_spray.py in.gif [-o tempdecal.wad] [--size 64x64] [--max-frames 64] [--colors 255]
    scripts/gif_to_spray.py --check tempdecal.wad

The GIF is scaled to fit the size (aspect kept, the rest transparent), all frames share one palette of 255 colors
(index 255 is the transparent color), and the frames after the first are stored as changes from the frame before,
in the block after the palette (format: src/cl_dll/spray_anim_core.cpp). The file stays a normal one-lump spray:
the engine accepts it and draws the first frame; SBO's client plays the others. The engine draws sprays of 1024 to
20480 bytes only: when the frames do not fit, every second frame is left out (its time goes to the one before)
until they do. --check prints what the engine and the client would make of a file.

Python 3 standard library only.
"""

import argparse
import struct
import sys

MAX_FILE = 20480
MIN_FILE = 1024
MAX_FRAMES = 64
TRANSPARENT = 255


# ---------------------------------------------------------------- GIF reader

def _lzw_decode(data, min_code_size, count):
    clear = 1 << min_code_size
    end = clear + 1
    size = min_code_size + 1
    table = [bytes([i]) for i in range(clear)] + [b'', b'']
    out = bytearray()
    prev = None
    bits = 0
    nbits = 0
    pos = 0
    while len(out) < count:
        while nbits < size:
            if pos >= len(data):
                return bytes(out)
            bits |= data[pos] << nbits
            pos += 1
            nbits += 8
        code = bits & ((1 << size) - 1)
        bits >>= size
        nbits -= size
        if code == clear:
            table = table[:clear + 2]
            size = min_code_size + 1
            prev = None
            continue
        if code == end:
            break
        if prev is None:
            entry = table[code] if code < len(table) else b''
            out += entry
            prev = entry
            continue
        if code < len(table):
            entry = table[code]
            table.append(prev + entry[:1])
        elif code == len(table):
            entry = prev + prev[:1]
            table.append(entry)
        else:
            raise ValueError('bad LZW code')
        out += entry
        prev = entry
        if len(table) == (1 << size) and size < 12:
            size += 1
    return bytes(out)


def read_gif(path):
    """Returns (width, height, [(rgba bytearray, delay ms)]) with the frames composed as a viewer shows them."""
    data = open(path, 'rb').read()
    if data[:6] not in (b'GIF87a', b'GIF89a'):
        raise ValueError('%s: not a GIF' % path)
    width, height, flags, bg_index, _ = struct.unpack_from('<HHBBB', data, 6)
    pos = 13
    global_table = None
    if flags & 0x80:
        n = 2 << (flags & 7)
        global_table = data[pos:pos + 3 * n]
        pos += 3 * n

    canvas = bytearray(width * height * 4)
    frames = []
    delay = 100
    transparent = None
    disposal = 0
    while pos < len(data):
        block = data[pos]
        pos += 1
        if block == 0x3B:
            break
        if block == 0x21:
            label = data[pos]
            pos += 1
            sub = bytearray()
            while True:
                n = data[pos]
                pos += 1
                if n == 0:
                    break
                sub += data[pos:pos + n]
                pos += n
            if label == 0xF9 and len(sub) >= 4:
                packed, d, t = struct.unpack_from('<BHB', sub, 0)
                disposal = (packed >> 2) & 7
                delay = d * 10
                transparent = t if packed & 1 else None
            continue
        if block != 0x2C:
            raise ValueError('%s: bad block 0x%02x' % (path, block))

        x0, y0, w, h, iflags = struct.unpack_from('<HHHHB', data, pos)
        pos += 9
        table = global_table
        if iflags & 0x80:
            n = 2 << (iflags & 7)
            table = data[pos:pos + 3 * n]
            pos += 3 * n
        if table is None:
            raise ValueError('%s: no color table' % path)
        min_code = data[pos]
        pos += 1
        lzw = bytearray()
        while True:
            n = data[pos]
            pos += 1
            if n == 0:
                break
            lzw += data[pos:pos + n]
            pos += n
        indices = _lzw_decode(bytes(lzw), min_code, w * h)
        indices = indices + bytes(w * h - len(indices))

        rows = list(range(h))
        if iflags & 0x40:
            rows = list(range(0, h, 8)) + list(range(4, h, 8)) + list(range(2, h, 4)) + list(range(1, h, 2))

        previous = bytearray(canvas) if disposal == 3 else None
        for src_row, y in enumerate(rows):
            cy = y0 + y
            if cy >= height:
                continue
            for x in range(w):
                cx = x0 + x
                if cx >= width:
                    continue
                i = indices[src_row * w + x]
                if i == transparent or 3 * i + 2 >= len(table):
                    continue
                o = (cy * width + cx) * 4
                canvas[o:o + 4] = bytes((table[3 * i], table[3 * i + 1], table[3 * i + 2], 255))
        frames.append((bytearray(canvas), delay))

        if disposal == 2:
            for y in range(y0, min(y0 + h, height)):
                for x in range(x0, min(x0 + w, width)):
                    o = (y * width + x) * 4
                    canvas[o:o + 4] = b'\0\0\0\0'
        elif disposal == 3 and previous is not None:
            canvas = previous
        delay = 100
        transparent = None
        disposal = 0

    if not frames:
        raise ValueError('%s: no frames' % path)
    return width, height, frames


# ---------------------------------------------------------------- image work

def fit(rgba, sw, sh, dw, dh):
    """Area-average scale into dw x dh, aspect kept, centered, the rest transparent."""
    scale = min(dw / sw, dh / sh)
    tw = max(1, round(sw * scale))
    th = max(1, round(sh * scale))
    ox = (dw - tw) // 2
    oy = (dh - th) // 2
    out = bytearray(dw * dh * 4)
    for ty in range(th):
        y0 = ty * sh // th
        y1 = max(y0 + 1, (ty + 1) * sh // th)
        for tx in range(tw):
            x0 = tx * sw // tw
            x1 = max(x0 + 1, (tx + 1) * sw // tw)
            r = g = b = a = n = 0
            for y in range(y0, y1):
                for x in range(x0, x1):
                    o = (y * sw + x) * 4
                    pa = rgba[o + 3]
                    r += rgba[o] * pa
                    g += rgba[o + 1] * pa
                    b += rgba[o + 2] * pa
                    a += pa
                    n += 1
            o = ((oy + ty) * dw + ox + tx) * 4
            if a:
                out[o:o + 4] = bytes((r // a, g // a, b // a, a // n))
    return out


def median_cut(colors, count):
    """colors: {rgb: weight}. Returns up to count palette colors."""
    boxes = [list(colors.items())]
    while len(boxes) < count:
        best = None
        for i, box in enumerate(boxes):
            if len(box) < 2:
                continue
            spread = max(max(c[0][k] for c in box) - min(c[0][k] for c in box) for k in range(3))
            if best is None or spread > best[0]:
                best = (spread, i)
        if best is None or best[0] == 0:
            break
        box = boxes.pop(best[1])
        k = max(range(3), key=lambda k: max(c[0][k] for c in box) - min(c[0][k] for c in box))
        box.sort(key=lambda c: c[0][k])
        total = sum(c[1] for c in box)
        acc = 0
        cut = 1
        for j, c in enumerate(box):
            acc += c[1]
            if acc * 2 >= total:
                cut = max(1, min(len(box) - 1, j + 1))
                break
        boxes += [box[:cut], box[cut:]]
    palette = []
    for box in boxes:
        total = sum(c[1] for c in box)
        palette.append(tuple(sum(c[0][k] * c[1] for c in box) // total for k in range(3)))
    return palette


def quantize(frames_rgba, count=255):
    colors = {}
    for rgba in frames_rgba:
        for o in range(0, len(rgba), 4):
            if rgba[o + 3] >= 128:
                c = (rgba[o], rgba[o + 1], rgba[o + 2])
                colors[c] = colors.get(c, 0) + 1
    if len(colors) <= count:
        palette = list(colors)
    else:
        palette = median_cut(colors, count)
    lookup = {}

    def nearest(c):
        i = lookup.get(c)
        if i is None:
            i = min(range(len(palette)), key=lambda j: sum((palette[j][k] - c[k]) ** 2 for k in range(3)))
            lookup[c] = i
        return i

    frames = []
    for rgba in frames_rgba:
        idx = bytearray(len(rgba) // 4)
        for p in range(len(idx)):
            o = p * 4
            idx[p] = nearest((rgba[o], rgba[o + 1], rgba[o + 2])) if rgba[o + 3] >= 128 else TRANSPARENT
        frames.append(idx)
    palette = palette + [(0, 0, 0)] * (255 - len(palette)) + [(0, 0, 255)]
    return palette, frames


# ---------------------------------------------------------------- spray file

def encode_frame(prev, cur):
    """The codes of src/cl_dll/spray_anim_core.cpp: literal 0x00-0x7F, skip 0x80-0xBF, fill 0xC0-0xFF."""
    out = bytearray()
    n = len(cur)
    i = 0
    while i < n:
        if cur[i] == prev[i]:
            j = i
            while j < n and j - i < 64 and cur[j] == prev[j]:
                j += 1
            out.append(0x80 | (j - i - 1))
            i = j
            continue
        j = i
        while j < n and j - i < 64 and cur[j] == cur[i]:
            j += 1
        if j - i >= 3:
            out += bytes((0xC0 | (j - i - 1), cur[i]))
            i = j
            continue
        j = i
        while j < n and j - i < 128:
            if j + 1 < n and cur[j] == prev[j] and cur[j + 1] == prev[j + 1]:
                break
            if j + 2 < n and cur[j] == cur[j + 1] == cur[j + 2]:
                break
            j += 1
        if j == i:
            j = i + 1
        out.append(j - i - 1)
        out += cur[i:j]
        i = j
    return bytes(out)


def mip(indices, w, h, step):
    return bytes(indices[y * w + x] for y in range(0, h, step) for x in range(0, w, step))


def build_wad(w, h, palette, frames, delays):
    pix = w * h
    lump = bytearray(b'{LOGO'.ljust(16, b'\0'))
    offsets = [40, 40 + pix, 40 + pix + pix // 4, 40 + pix + pix // 4 + pix // 16]
    lump += struct.pack('<II4I', w, h, *offsets)
    for step in (1, 2, 4, 8):
        lump += mip(frames[0], w, h, step)
    lump += struct.pack('<H', 256)
    for c in palette:
        lump += bytes(c)
    while len(lump) % 4:
        lump += b'\0'
    if len(frames) > 1:
        lump += b'SANM' + struct.pack('<BBH', 1, len(frames), 0)
        for d in delays:
            lump += struct.pack('<H', max(0, min(65535, d)))
        for k in range(1, len(frames)):
            code = encode_frame(frames[k - 1], frames[k])
            lump += struct.pack('<H', len(code)) + code
    while len(lump) % 4:
        lump += b'\0'
    # the engine draws files of MIN_FILE bytes or more
    short = MIN_FILE - (12 + len(lump) + 32)
    if short > 0:
        lump += bytes((short + 3) & ~3)
    header = b'WAD3' + struct.pack('<ii', 1, 12 + len(lump))
    entry = struct.pack('<iiibbh16s', 12, len(lump), len(lump), 0x43, 0, 0, b'{LOGO')
    return header + bytes(lump) + entry


def pick(frames, delays, n):
    """n frames spread evenly over the loop; each keeps the time up to the next one picked."""
    starts = [k * len(frames) // n for k in range(n)] + [len(frames)]
    return ([frames[starts[k]] for k in range(n)],
            [sum(delays[starts[k]:starts[k + 1]]) for k in range(n)])


def make_spray(gif, size, max_frames, colors=255):
    sw, sh, gif_frames = read_gif(gif)
    dw, dh = size
    scaled = [fit(rgba, sw, sh, dw, dh) for rgba, _ in gif_frames]
    delays = [d for _, d in gif_frames]
    palette, frames = quantize(scaled, colors)

    # drop identical neighbors (their time goes to the one before)
    i = 1
    while i < len(frames):
        if frames[i] == frames[i - 1]:
            delays[i - 1] += delays[i]
            del frames[i], delays[i]
        else:
            i += 1

    # as many frames as fit (at most max_frames), spread evenly
    best = None
    lo, hi = 1, min(len(frames), max_frames)
    while lo <= hi:
        n = (lo + hi) // 2
        wad = build_wad(dw, dh, palette, *pick(frames, delays, n))
        if len(wad) <= MAX_FILE:
            best = (wad, n)
            lo = n + 1
        else:
            hi = n - 1
    if best is None:
        raise ValueError('%dx%d does not fit in %d bytes even as one frame: use a smaller --size' % (dw, dh, MAX_FILE))
    wad, n = best
    return wad, n, len(frames) - n, len(gif_frames)


def check(path):
    """The engine's checks (Draw_CustomCacheWadInit, Draw_ValidateCustomLogo) and the animation block."""
    d = open(path, 'rb').read()
    problems = []
    if not (MIN_FILE <= len(d) <= MAX_FILE):
        problems.append('size %d is outside %d..%d: the engine shows no spray' % (len(d), MIN_FILE, MAX_FILE))
    if d[:4] != b'WAD3':
        return ['no WAD3 id']
    numlumps, infotable = struct.unpack_from('<ii', d, 4)
    if numlumps != 1:
        return ['%d lumps (must be 1)' % numlumps]
    if infotable + 32 != len(d):
        return ['bogus infotableofs']
    filepos, disksize, size = struct.unpack_from('<iii', d, infotable)
    if size != disksize or size < 1 or filepos < 12 or filepos + size > infotable:
        return ['bad lump entry']
    w, h = struct.unpack_from('<II', d, filepos + 16)
    offs = struct.unpack_from('<4I', d, filepos + 24)
    if not (0 < w <= 256 and 0 < h <= 256):
        return ['size %dx%d' % (w, h)]
    pix = w * h
    for k in range(3):
        if offs[k] + (pix >> (2 * k)) != offs[k + 1]:
            return ['bad mip offsets']
    msize = pix + pix // 4 + pix // 16 + pix // 64
    pal = filepos + 40 + msize
    colors = struct.unpack_from('<H', d, pal)[0]
    if colors > 256 or colors + 2 * (colors + 1) + msize + offs[0] > disksize:
        return ['bad palette']
    last = d[pal + 2 + 765:pal + 2 + 768]
    print('%s: %d bytes, %dx%d, %s' % (path, len(d), w, h,
          'transparent color index 255' if last == b'\0\0\xff' else 'no transparent color'))
    block = filepos + ((40 + msize + 2 + 3 * colors + 3) & ~3)
    if d[block:block + 4] == b'SANM':
        n = d[block + 5]
        delays = struct.unpack_from('<%dH' % n, d, block + 8)
        print('  animated: %d frames, %d ms per loop' % (n, sum(delays)))
    else:
        print('  still')
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('input')
    ap.add_argument('-o', '--output', default='tempdecal.wad')
    ap.add_argument('--size', default='64x64', help='WxH, multiples of 16 (default 64x64)')
    ap.add_argument('--max-frames', type=int, default=MAX_FRAMES)
    ap.add_argument('--colors', type=int, default=255,
                    help='palette size, 2..255 (fewer colors: smaller frames, so more of them fit)')
    ap.add_argument('--check', action='store_true', help='check a tempdecal.wad instead')
    args = ap.parse_args()

    if args.check:
        problems = check(args.input)
        for p in problems:
            print('  PROBLEM: ' + p)
        return 1 if problems else 0

    try:
        w, h = (int(v) for v in args.size.lower().split('x'))
    except ValueError:
        ap.error('--size must be WxH')
    if w % 16 or h % 16 or not (16 <= w <= 256 and 16 <= h <= 256):
        ap.error('--size: width and height must be multiples of 16 from 16 to 256')
    max_frames = max(1, min(MAX_FRAMES, args.max_frames))

    try:
        wad, frames, dropped, total = make_spray(args.input, (w, h), max_frames, max(2, min(255, args.colors)))
    except (ValueError, OSError, struct.error) as e:
        print('error: %s' % e, file=sys.stderr)
        return 1
    open(args.output, 'wb').write(wad)
    note = ' (%d frames left out, the others spread over the loop)' % dropped if dropped else ''
    print('%s: %dx%d, %d of %d frames, %d bytes%s' % (args.output, w, h, frames, total, len(wad), note))
    return 0


if __name__ == '__main__':
    sys.exit(main())
