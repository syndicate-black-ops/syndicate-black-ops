#!/usr/bin/env python3
"""Writes gamedir/syndicate/sbo_tools.wad, SBO's tool textures for map editors and compilers.

    python3 tools/tools-wad/make_sbo_tools_wad.py [out.wad]

{screen (64 x 64): marks the faces of a brush entity that show a world screen (vgui_screen "surface",
docs/WORLD_SCREENS.md). Every pixel is palette index 255, the transparent color of a '{' texture: the face is
invisible in the game (the vgui_screen sets its carrier's render mode to Solid), and the client draws the panel there.
Index 255 is pure blue, which is what editors that draw '{' textures opaque show.

env_sky (64 x 64): the faces through which the 3D sky shows (docs/ENV_SKY.md). SDHLT and ReSDHLT make a brush whose
texture name starts with "env_sky" sky and leave its faces out of the BSP, so the game never draws the picture.

env_sky is a flat light-blue field with its name in a 5 x 7 pixel font, so the editor shows which faces have it.
The compile tools' own tool textures (CLIP, NULL, SOLIDHINT, ...) come with the compile tools (sdhlt.wad), not here.
Standard library only; the same input gives the same bytes.
"""
import os
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

FONT = {
    'E': ('11111', '10000', '10000', '11110', '10000', '10000', '11111'),
    'K': ('10001', '10010', '10100', '11000', '10100', '10010', '10001'),
    'N': ('10001', '10001', '11001', '10101', '10011', '10001', '10001'),
    'S': ('01111', '10000', '10000', '01110', '00001', '00001', '11110'),
    'V': ('10001', '10001', '10001', '10001', '10001', '01010', '00100'),
    'Y': ('10001', '10001', '01010', '00100', '00100', '00100', '00100'),
    '_': ('00000', '00000', '00000', '00000', '00000', '00000', '11111'),
}

SKY_BLUE = (120, 180, 255)

# (texture name, label lines, fill color)
TEXTURES = [
    ('env_sky', ['ENV_SKY'], SKY_BLUE),
]

FILL, TEXT = 1, 2   # palette indices of a labeled texture; every other index is the gray ramp


def miptex(name, width, height, index):
    """A WAD3 miptex lump of one palette index, with its four mip levels and the palette (gray ramp, 255 = blue)."""
    pixels = b''.join(bytes([index]) * ((width >> m) * (height >> m)) for m in range(4))
    offsets = [40]
    for m in range(3):
        offsets.append(offsets[-1] + (width >> m) * (height >> m))
    palette = b''.join(bytes([i, i, i]) for i in range(255)) + bytes([0, 0, 255])
    return (name.encode('latin-1').ljust(16, b'\0') + struct.pack('<II4I', width, height, *offsets) + pixels +
            struct.pack('<H', 256) + palette + b'\0\0')


def labeled(name, lines, fill, size=64):
    """A WAD3 miptex lump: a flat field of `fill` with `lines` centered in the pixel font (twice as large where every
    line fits), black or white text by the field's brightness; the smaller mips keep a pixel as text where at least
    half of its block is text."""
    px = bytearray([FILL]) * (size * size)
    if lines:
        scale = 2 if all(12 * len(s) - 2 <= size - 4 for s in lines) else 1
        lh = 8 * scale
        y0 = (size - (len(lines) * lh - scale)) // 2
        for n, s in enumerate(lines):
            assert 6 * len(s) - 1 <= size - 4, s
            x0 = (size - (6 * len(s) - 1) * scale) // 2
            for k, ch in enumerate(s):
                g = FONT[ch]
                for row in range(7):
                    for col in range(5):
                        if g[row][col] == '1':
                            for dy in range(scale):
                                for dx in range(scale):
                                    x = x0 + (6 * k + col) * scale + dx
                                    y = y0 + n * lh + row * scale + dy
                                    px[y * size + x] = TEXT
    mips = [bytes(px)]
    for m in range(1, 4):
        w, f = size >> m, 1 << m
        mip = bytearray(w * w)
        for y in range(w):
            for x in range(w):
                n = sum(px[(y * f + j) * size + x * f + i] == TEXT for j in range(f) for i in range(f))
                mip[y * w + x] = TEXT if 2 * n >= f * f else FILL
        mips.append(bytes(mip))
    offsets = [40]
    for m in range(3):
        offsets.append(offsets[-1] + len(mips[m]))
    lum = 0.299 * fill[0] + 0.587 * fill[1] + 0.114 * fill[2]
    text = (0, 0, 0) if lum >= 110 else (255, 255, 255)
    palette = bytearray(b''.join(bytes([i, i, i]) for i in range(256)))
    palette[3 * FILL:3 * FILL + 3] = bytes(fill)
    palette[3 * TEXT:3 * TEXT + 3] = bytes(text)
    return (name.encode('latin-1').ljust(16, b'\0') + struct.pack('<II4I', size, size, *offsets) + b''.join(mips) +
            struct.pack('<H', 256) + bytes(palette) + b'\0\0')


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'gamedir', 'syndicate', 'sbo_tools.wad')
    lumps = [('{screen', miptex('{screen', 64, 64, 255))]
    lumps += [(name, labeled(name, lines, fill)) for name, lines, fill in TEXTURES]
    data, directory, pos = b'', b'', 12
    for name, lump in lumps:
        assert len(name) <= 15, name
        directory += struct.pack('<iiibbh', pos, len(lump), len(lump), 0x43, 0, 0) + \
            name.encode('latin-1').ljust(16, b'\0')
        data += lump
        pos += len(lump)
    with open(out, 'wb') as f:
        f.write(b'WAD3' + struct.pack('<ii', len(lumps), 12 + len(data)) + data + directory)
    print('%s: %d bytes, %d texture(s)' % (out, 12 + len(data) + len(directory), len(lumps)))


if __name__ == '__main__':
    main()
