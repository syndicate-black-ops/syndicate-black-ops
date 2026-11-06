#!/usr/bin/env python3
"""Writes gamedir/syndicate/sbo_tools.wad, SBO's tool textures for map editors and compilers (docs/WORLD_SCREENS.md).

    python3 tools/tools-wad/make_sbo_tools_wad.py [out.wad]

{screen (64 x 64): marks the faces of a brush entity that show a world screen (vgui_screen "surface"). Every pixel
is palette index 255, the transparent color of a '{' texture: the face is invisible in the game (the vgui_screen sets
its carrier's render mode to Solid), and the client draws the panel there. Index 255 is pure blue, which is what
editors that draw '{' textures opaque show. Standard library only.
"""
import os
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def miptex(name, width, height, index):
    """A WAD3 miptex lump of one palette index, with its four mip levels and the palette."""
    pixels = b''.join(bytes([index]) * ((width >> m) * (height >> m)) for m in range(4))
    offsets = [40]
    for m in range(3):
        offsets.append(offsets[-1] + (width >> m) * (height >> m))
    palette = b''.join(bytes([i, i, i]) for i in range(255)) + bytes([0, 0, 255])   # gray ramp, 255 = blue
    return (name.encode('latin-1').ljust(16, b'\0') + struct.pack('<II4I', width, height, *offsets) + pixels +
            struct.pack('<H', 256) + palette + b'\0\0')


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'gamedir', 'syndicate', 'sbo_tools.wad')
    lumps = [('{screen', miptex('{screen', 64, 64, 255))]
    data, directory, pos = b'', b'', 12
    for name, lump in lumps:
        directory += struct.pack('<iiibbh', pos, len(lump), len(lump), 0x43, 0, 0) + \
            name.encode('latin-1').ljust(16, b'\0')
        data += lump
        pos += len(lump)
    with open(out, 'wb') as f:
        f.write(b'WAD3' + struct.pack('<ii', len(lumps), 12 + len(data)) + data + directory)
    print('%s: %d bytes, %d texture(s)' % (out, 12 + len(data) + len(directory), len(lumps)))


if __name__ == '__main__':
    main()
