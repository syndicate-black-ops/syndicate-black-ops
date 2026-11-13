#!/usr/bin/env python3
"""Color correction lookup tables for color_correction / color_correction_volume (docs/COLOR_CORRECTION.md).

    scripts/colorlut.py convert IN OUT                      .cube <-> .raw (a .cube of another size is resampled to
                                                            the 32x32x32 of a .raw; a .raw becomes a 32-point .cube)
    scripts/colorlut.py bake OUT [--size N] [--from IN] [--preset NAME]
                             [--saturation S] [--contrast C] [--brightness B] [--gamma G]
                             [--tint "R G B"] [--tintamount A]
                                                            a table from the simple keys (the same math as the game),
                                                            over a preset or another table; OUT .raw or .cube
    scripts/colorlut.py strip OUT.png [--size N] [--from IN]
                                                            a strip image (N squares of N x N side by side, red across,
                                                            green down, blue square by square): neutral, or IN's
                                                            colors; grade it in an image editor, then:
    scripts/colorlut.py fromstrip IN.png OUT                the graded strip image back to a table
    scripts/colorlut.py presets                             the preset names

Formats: Source .raw (32x32x32 RGB bytes, 98,304 bytes, red fastest), .cube (LUT_3D_SIZE 2 - 64, values 0 - 1, red
fastest). Python 3 standard library only.
"""

import argparse
import math
import os
import struct
import sys
import zlib

RAW_SIZE = 32


# --- tables: size and a flat list of (r, g, b) floats 0 - 1, red fastest -------------------------------------------

def identity(size):
    n = size - 1
    return size, [(r / n, g / n, b / n) for b in range(size) for g in range(size) for r in range(size)]


def clamp01(v):
    if not v > 0.0:
        return 0.0
    return 1.0 if v > 1.0 else v


def read_raw(path):
    data = open(path, "rb").read()
    if len(data) != RAW_SIZE ** 3 * 3:
        sys.exit(f"{path}: a .raw table is {RAW_SIZE ** 3 * 3} bytes, this one is {len(data)}")
    return RAW_SIZE, [(data[i] / 255.0, data[i + 1] / 255.0, data[i + 2] / 255.0) for i in range(0, len(data), 3)]


def read_cube(path):
    size, values = 0, []
    for no, line in enumerate(open(path, encoding="latin-1"), 1):
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        word = s.split()[0]
        if word == "TITLE":
            continue
        if word == "LUT_3D_SIZE":
            size = int(s.split()[1])
            if not 2 <= size <= 64:
                sys.exit(f"{path}:{no}: LUT_3D_SIZE must be 2 to 64")
            continue
        if word in ("DOMAIN_MIN", "DOMAIN_MAX"):
            want = 0.0 if word == "DOMAIN_MIN" else 1.0
            if [float(x) for x in s.split()[1:4]] != [want] * 3:
                sys.exit(f"{path}:{no}: only the domain 0 0 0 - 1 1 1 is supported")
            continue
        if word[0].isalpha():
            sys.exit(f"{path}:{no}: unsupported keyword {word}")
        parts = s.split()
        if len(parts) != 3:
            sys.exit(f"{path}:{no}: a value line needs three numbers")
        values.append(tuple(clamp01(float(x)) for x in parts))
    if not size or len(values) != size ** 3:
        sys.exit(f"{path}: {len(values)} values, LUT_3D_SIZE {size} needs {size ** 3}")
    return size, values


def read_table(path):
    ext = os.path.splitext(path)[1].lower()
    if ext == ".raw":
        return read_raw(path)
    if ext == ".cube":
        return read_cube(path)
    sys.exit(f"{path}: not a .raw or .cube file")


def write_table(path, table, title="SBO color correction"):
    size, values = table
    ext = os.path.splitext(path)[1].lower()
    if ext == ".raw":
        if size != RAW_SIZE:
            table = resample(table, RAW_SIZE)
            size, values = table
        out = bytearray()
        for r, g, b in values:
            out += bytes((round(clamp01(r) * 255), round(clamp01(g) * 255), round(clamp01(b) * 255)))
        open(path, "wb").write(bytes(out))
    elif ext == ".cube":
        lines = [f'TITLE "{title}"', f"LUT_3D_SIZE {size}", "DOMAIN_MIN 0.0 0.0 0.0", "DOMAIN_MAX 1.0 1.0 1.0"]
        lines += [f"{clamp01(r):.6f} {clamp01(g):.6f} {clamp01(b):.6f}" for r, g, b in values]
        open(path, "w", newline="\n").write("\n".join(lines) + "\n")
    else:
        sys.exit(f"{path}: write a .raw or .cube file")


def sample(table, r, g, b):
    """The table at a color, trilinear (as the game's 3D texture filters it)."""
    size, values = table
    n = size - 1

    def axis(v):
        f = clamp01(v) * n
        i = min(int(f), n - 1)
        return i, f - i

    (ri, rf), (gi, gf), (bi, bf) = axis(r), axis(g), axis(b)
    out = [0.0, 0.0, 0.0]
    for db, wb in ((0, 1 - bf), (1, bf)):
        for dg, wg in ((0, 1 - gf), (1, gf)):
            for dr, wr in ((0, 1 - rf), (1, rf)):
                w = wr * wg * wb
                if w:
                    v = values[(ri + dr) + (gi + dg) * size + (bi + db) * size * size]
                    for c in range(3):
                        out[c] += v[c] * w
    return tuple(out)


def resample(table, size):
    n = size - 1
    return size, [sample(table, r / n, g / n, b / n) for b in range(size) for g in range(size) for r in range(size)]


# --- the simple keys: the same math as the client (src/cl_dll/colorcorrection_core.cpp ApplySimpleKeys) -----------

def simple_keys(rgb, saturation=1.0, contrast=1.0, brightness=0.0, gamma=1.0, tint=(255, 255, 255), tintamount=0.0):
    r, g, b = rgb
    lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
    inv_gamma = 1.0 / gamma if gamma > 0.001 else 1.0
    amount = clamp01(tintamount)
    out = []
    for c, v in enumerate((r, g, b)):
        v = lum + (v - lum) * saturation
        v = (v - 0.5) * contrast + 0.5
        v += brightness
        v = clamp01(v)
        if inv_gamma != 1.0 and v > 0.0:
            v = v ** inv_gamma
        t = tint[c] / 255.0
        v = v * (1.0 - amount) + v * t * amount
        out.append(clamp01(v))
    return tuple(out)


# --- presets ------------------------------------------------------------------------------------------------------

def smoothstep(e0, e1, x):
    t = clamp01((x - e0) / (e1 - e0))
    return t * t * (3 - 2 * t)


def tealorange(rgb):
    """Teal shadows, orange highlights and skin, a little more color (the film look)."""
    r, g, b = rgb
    lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
    t = smoothstep(0.15, 0.75, lum)
    shadow, light = (0.55, 1.0, 1.1), (1.2, 0.92, 0.62)
    tone = [shadow[c] + (light[c] - shadow[c]) * t for c in range(3)]
    out = [v * tone[c] for c, v in enumerate((r, g, b))]
    lum2 = 0.2126 * out[0] + 0.7152 * out[1] + 0.0722 * out[2]
    return tuple(clamp01(lum2 + (v - lum2) * 1.25) for v in out)


PRESETS = {
    "identity": lambda rgb: rgb,
    "tealorange": tealorange,
}


# --- strip images (PNG, standard library) -------------------------------------------------------------------------

def write_png(path, width, height, rows):
    raw = b"".join(b"\0" + bytes(row) for row in rows)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


def read_png(path):
    """8-bit RGB or RGBA, not interlaced: (width, height, rows of RGB bytes)."""
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{path}: not a PNG")
    pos, idat, width = 8, b"", 0
    while pos + 8 <= len(data):
        length, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if tag == b"IHDR":
            width, height, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or ctype not in (2, 6) or interlace:
                sys.exit(f"{path}: needs an 8-bit RGB or RGBA PNG, not interlaced")
            bpp = 3 if ctype == 2 else 4
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
    if not width:
        sys.exit(f"{path}: no IHDR")
    raw = zlib.decompress(idat)
    stride = width * bpp
    rows, prev = [], bytearray(stride)
    for y in range(height):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        prev = line
        rows.append(bytes(v for i, v in enumerate(line) if bpp == 3 or i % 4 != 3))
    return width, height, rows


def strip_image(path, table):
    size, values = table
    rows = []
    for g in range(size):
        row = []
        for b in range(size):
            for r in range(size):
                row += [round(clamp01(v) * 255) for v in values[r + g * size + b * size * size]]
        rows.append(row)
    write_png(path, size * size, size, rows)


def from_strip(path):
    width, height, rows = read_png(path)
    size = height
    if width != size * size or not 2 <= size <= 64:
        sys.exit(f"{path}: a strip image is N*N x N pixels (e.g. 1024 x 32), this one is {width} x {height}")
    values = []
    for b in range(size):
        for g in range(size):
            for r in range(size):
                x = (b * size + r) * 3
                values.append(tuple(rows[g][x + c] / 255.0 for c in range(3)))
    return size, values


# --- commands -----------------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("convert")
    p.add_argument("src")
    p.add_argument("dst")
    p = sub.add_parser("bake")
    p.add_argument("dst")
    p.add_argument("--size", type=int, default=RAW_SIZE)
    p.add_argument("--from", dest="src")
    p.add_argument("--preset", default="identity", choices=sorted(PRESETS))
    p.add_argument("--saturation", type=float, default=1.0)
    p.add_argument("--contrast", type=float, default=1.0)
    p.add_argument("--brightness", type=float, default=0.0)
    p.add_argument("--gamma", type=float, default=1.0)
    p.add_argument("--tint", default="255 255 255")
    p.add_argument("--tintamount", type=float, default=0.0)
    p = sub.add_parser("strip")
    p.add_argument("dst")
    p.add_argument("--size", type=int, default=RAW_SIZE)
    p.add_argument("--from", dest="src")
    p = sub.add_parser("fromstrip")
    p.add_argument("src")
    p.add_argument("dst")
    sub.add_parser("presets")
    a = ap.parse_args()

    if a.cmd == "convert":
        write_table(a.dst, read_table(a.src))
    elif a.cmd == "bake":
        if not 2 <= a.size <= 64:
            sys.exit("--size must be 2 to 64")
        base = read_table(a.src) if a.src else identity(a.size)
        if base[0] != a.size and not a.src:
            base = resample(base, a.size)
        tint = tuple(int(x) for x in a.tint.split())
        if len(tint) != 3:
            sys.exit('--tint needs "R G B"')
        preset = PRESETS[a.preset]
        size, values = base
        values = [simple_keys(preset(v), a.saturation, a.contrast, a.brightness, a.gamma, tint, a.tintamount)
                  for v in values]
        write_table(a.dst, (size, values), title=f"SBO {a.preset}")
    elif a.cmd == "strip":
        table = read_table(a.src) if a.src else identity(a.size)
        strip_image(a.dst, table)
    elif a.cmd == "fromstrip":
        write_table(a.dst, from_strip(a.src))
    elif a.cmd == "presets":
        print("\n".join(sorted(PRESETS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
