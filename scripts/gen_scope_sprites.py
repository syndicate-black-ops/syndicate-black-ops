#!/usr/bin/env python3
"""Sniper scope overlay: sprites/scope_quad_*.spr, one quadrant of the picture.

The scope of SBO 1.38 is a 768x768 picture (sprites/1024*.spr around sprites/centre.spr): a black mask with a round
hole, a reticle of 1-pixel lines and a red center cross. This script draws the same picture again from its measured
geometry (the constants below, in pixels of the 768 picture) at a higher resolution, and writes only its top left
quadrant: the picture is symmetric, and the client (cl_dll/sniper.cpp, CHudSniper::DrawScaled) draws the quadrant
four times, mirrored, as high as the screen. The 1.38 files are not touched (the software renderer draws them).

  scripts/gen_scope_sprites.py generate [--gamedir DIR]   write the sprites
  scripts/gen_scope_sprites.py measure  [--gamedir DIR]   measure the 1.38 art again (where the constants come from)

Python 3 standard library only. The output is deterministic.

Two sets (the client loads one, by the height of the picture on screen):
  * scope_quad_11/12/21/22.spr: the quadrant as 2x2 tiles, a picture of 2041 pixels, lines 3 pixels. Screens higher
    than 1020 pixels (1080: 0.53x, 1440: 0.71x, 2160: 1.06x).
  * scope_quad_small.spr: the quadrant in one tile, a picture of 1020 pixels, lines 2 pixels. Smaller screens
    (480: 0.47x .. 1020: 1x). The engine's filter reads 2x2 pixels of the sprite per screen pixel: far below half
    size it skips pixels and thin lines break up, which is why small screens have their own set.

Form (sniper.cpp has the same numbers: SCOPE_TILE, SCOPE_BORDER):
  * Tiles are 512x512, one frame each. Valve's engine refuses a texture of more than 524288 pixels
    ("GL_LoadTexture: too big") and, on a card without non-power-of-two textures, resamples every texture to a power
    of two of at most gl_max_size (256): 512 loads everywhere and is at worst halved.
  * A tile holds its part of the quadrant inside a border of 1 pixel: black on the outside of the picture, the
    neighboring tile's pixels between the two tiles. The client draws the part inside the border, so filtering at a
    tile's edge reads the right pixels: no seam.
  * The mirror axes: lines on an axis (the reticle's arms, the red cross) are split by it, half in each quadrant.
    A line of 3 pixels can only be halved through the middle of its center pixel, so in the 2041 set the axis runs
    through the middle of the quadrant's last column and row (the client draws up to the middle of that pixel:
    1020.5 pixels per quadrant); the 1020 set has lines of 2 pixels and the axis on the edge of the last pixel, whose
    border is that pixel again (its mirror image). Either way a mirrored quadrant continues the picture exactly.
  * SPR_ALPHTEST like the 1.38 tiles (palette index 255 is the hole). The hole's palette color is black, not 1.38's
    white: filtering mixes it into the pixels along every edge, where it must not show.
"""
import argparse
import math
import os
import struct
import sys

# ---------------------------------------------------------------------------------------------------------------
# The 1.38 picture, measured (the "measure" command prints these from the files). Pixels of the 768x768 picture,
# as distances from the middle of the reticle (the middle of pixel 128 of centre.spr).
# ---------------------------------------------------------------------------------------------------------------

ART_SIZE = 768

# the hole: least-squares circle through its edge (the edge is within 1.5 pixels of it everywhere). In 1.38 its
# center is 3.5 pixels left of the reticle and 0.5 below; here it is centered on the reticle (the picture must be
# symmetric to be drawn from one quadrant).
CIRCLE_RADIUS = 369.46

# the reticle: lines 1 pixel wide
ARM_START = 22.5                    # the four arms start here ...
ARM_END_H = 119.5                   # ... and end here (left/right)
ARM_END_V = 124.5                   # (up/down)
BAR_LENGTH = 14.0                   # the outer end of each arm is a bar 3 pixels wide, this long
BAR_END_V = 123.5                   # (up/down: the bar stops 1 pixel short, at the end line)
END_LINE_H = (119.0, 8.5)           # line across the end of the left/right arms: (distance, half length)
END_LINE_V = (124.0, 7.5)
TICKS_H = ((40.0, 4.5), (58.0, 5.5), (76.0, 6.5), (94.0, 7.5))   # ticks across the left/right arms
TICKS_V = ((32.0, 3.5), (54.0, 4.5), (76.0, 5.5), (98.0, 6.5))   # ticks across the up/down arms
CENTER_PLUS = 1.5                   # black plus in the middle: half length
RED_DASHES = ((1.5, 7.5), (9.5, 14.5))   # red cross: two dashes on each of the four sides (from, to)
# (1.38's left arm has its end line and outer tick 1 pixel longer at the bottom; symmetric here)

BLACK = (0, 0, 0)
RED = (202, 17, 17)                 # palette entry 26 of centre.spr

# ---------------------------------------------------------------------------------------------------------------
# The new sets
# ---------------------------------------------------------------------------------------------------------------

TILE = 512              # sprite frame size
BORDER = 1              # pixels around a tile's part of the picture
INNER = TILE - 2 * BORDER   # 510


class ScopeSet:
    """tiles: tiles per side of the quadrant. line / bar: width of the 1 pixel lines and of the 3 pixel bars, in
    pixels of the set; both odd (the axis runs through the middle of a pixel) or both even (between two pixels)."""

    def __init__(self, files, tiles, line, bar):
        assert line % 2 == bar % 2
        self.files, self.tiles, self.line, self.bar = files, tiles, line, bar
        self.odd = line % 2 == 1
        # columns of the quadrant, counted from the axis: column 0 is the pixel on the axis (odd) or next to it
        self.columns = tiles * INNER + (1 if self.odd else 0)
        self.picture = 2 * self.columns - (1 if self.odd else 0)    # pixels of the whole picture
        self.scale = self.picture / float(ART_SIZE)
        self.shift = 0.5 if self.odd else 0.0   # column i covers the distances [i - shift, i + 1 - shift]


# 1 pixel of 768 is 2.66 pixels of the 2041 picture and 1.33 of the 1020 picture
SETS = (
    ScopeSet((("scope_quad_11", "scope_quad_12"), ("scope_quad_21", "scope_quad_22")), 2, 3, 9),
    ScopeSet((("scope_quad_small",),), 1, 2, 4),
)

INDEX_BLACK, INDEX_RED, INDEX_HOLE = 0, 1, 255

# GoldSrc sprite file (dsprite_t, engine spritegn.h): header, palette, frames
SPR_HEADER = struct.Struct("<4siiifiiifi")  # ident version type texFormat boundingradius width height numframes beamlength synctype
SPR_FRAME = struct.Struct("<5i")            # frame type (0 = single), origin x, origin y, width, height
SPR_VP_PARALLEL = 2
SPR_ALPHTEST = 3
ST_RAND = 1


def write_sprite(path, width, height, pixels, palette):
    """One-frame sprite as 1.38's scope tiles are (type 2, texFormat 3, synctype 1)."""
    assert len(pixels) == width * height and len(palette) == 768
    radius = math.sqrt((width / 2.0) ** 2 + (height / 2.0) ** 2)
    out = SPR_HEADER.pack(b"IDSP", 2, SPR_VP_PARALLEL, SPR_ALPHTEST, radius, width, height, 1, 0.0, ST_RAND)
    out += struct.pack("<h", 256) + palette
    out += SPR_FRAME.pack(0, -(width // 2), height // 2, width, height) + bytes(pixels)
    with open(path, "wb") as f:
        f.write(out)


def read_sprite(path):
    """(width, height, palette, pixels of the first frame, texFormat)"""
    with open(path, "rb") as f:
        data = f.read()
    ident, version, _, texfmt = SPR_HEADER.unpack_from(data, 0)[:4]
    if ident != b"IDSP" or version != 2:
        raise ValueError("%s: not a GoldSrc sprite" % path)
    offset = SPR_HEADER.size
    (colors,) = struct.unpack_from("<h", data, offset)
    offset += 2
    palette = data[offset:offset + colors * 3]
    offset += colors * 3
    _, _, _, width, height = SPR_FRAME.unpack_from(data, offset)
    offset += SPR_FRAME.size
    return width, height, palette, data[offset:offset + width * height], texfmt


def palette():
    pal = bytearray(768)            # all black, the hole (255) too
    pal[INDEX_RED * 3:INDEX_RED * 3 + 3] = bytes(RED)
    pal[INDEX_BLACK * 3:INDEX_BLACK * 3 + 3] = bytes(BLACK)
    return bytes(pal)


# ---------------------------------------------------------------------------------------------------------------
# Drawing: the quadrant as rows[j][i], i and j counted from the axes outward
# ---------------------------------------------------------------------------------------------------------------

def columns(st, a, b):
    """The columns whose middle is within the distances a..b (pixels of the 768 picture) from an axis."""
    return (max(0, int(math.ceil(a * st.scale + st.shift - 0.5))),
            min(st.columns, int(math.ceil(b * st.scale + st.shift - 0.5))))


def across(st, center, width):
    """The columns of a line 'width' pixels of the set wide, its middle 'center' (768 picture) from an axis."""
    start = int(math.floor(center * st.scale + st.shift - width / 2.0 + 0.5))
    return max(0, start), min(st.columns, start + width)


def fill(rows, i0, i1, j0, j1, index):
    for j in range(j0, j1):
        rows[j][i0:i1] = bytes([index]) * (i1 - i0)


def draw_quadrant(st):
    # mask: black where the pixel's middle is outside the circle
    r = CIRCLE_RADIUS * st.scale
    rows = []
    for j in range(st.columns):
        row = bytearray([INDEX_BLACK]) * st.columns
        d = r * r - (j + 0.5 - st.shift) ** 2
        if d > 0:
            inside = min(st.columns, int(math.floor(math.sqrt(d) + st.shift - 0.5)) + 1)
            row[0:inside] = bytes([INDEX_HOLE]) * inside
        rows.append(row)

    def along(horizontal, a, b, width, index):   # on the axis: half of the width is in this quadrant
        i0, i1 = columns(st, a, b)
        j0, j1 = across(st, 0.0, width)
        if horizontal:
            fill(rows, i0, i1, j0, j1, index)
        else:
            fill(rows, j0, j1, i0, i1, index)

    def cross(horizontal, distance, half):       # across the arm
        i0, i1 = across(st, distance, st.line)
        j0, j1 = columns(st, 0.0, half)
        if horizontal:
            fill(rows, i0, i1, j0, j1, INDEX_BLACK)
        else:
            fill(rows, j0, j1, i0, i1, INDEX_BLACK)

    for horizontal, arm_end, bar_end, end_line, ticks in ((True, ARM_END_H, ARM_END_H, END_LINE_H, TICKS_H),
                                                          (False, ARM_END_V, BAR_END_V, END_LINE_V, TICKS_V)):
        along(horizontal, ARM_START, arm_end, st.line, INDEX_BLACK)
        along(horizontal, bar_end - BAR_LENGTH, bar_end, st.bar, INDEX_BLACK)
        for distance, half in ticks + (end_line,):
            cross(horizontal, distance, half)
        for a, b in RED_DASHES:
            along(horizontal, a, b, st.line, INDEX_RED)
        along(horizontal, 0.0, CENTER_PLUS, st.line, INDEX_BLACK)

    # the middle of the reticle is black; the corner of the picture is black (the client stretches the texels
    # 2..6 of the first tile over the screen beside the picture)
    assert rows[0][0] == INDEX_BLACK
    assert all(rows[st.columns - 1 - j][st.columns - 1 - i] == INDEX_BLACK for j in range(8) for i in range(8))
    return rows


def quadrant_pixel(st, rows, x, y):
    """Pixel x, y of the top left quadrant (0, 0 is the picture's corner); beyond the axes: the mirror image."""
    if x < 0 or y < 0:
        return INDEX_BLACK
    last = st.columns - 1
    i = last - x if x <= last else (x - last if st.odd else x - last - 1)
    j = last - y if y <= last else (y - last if st.odd else y - last - 1)
    return rows[j][i]


def tile_pixels(st, rows, row, col):
    """Tile (row, col) of the top left quadrant: 512 pixels from BORDER before its part of the picture."""
    out = bytearray()
    for y in range(row * INNER - BORDER, row * INNER - BORDER + TILE):
        for x in range(col * INNER - BORDER, col * INNER - BORDER + TILE):
            out.append(quadrant_pixel(st, rows, x, y))
    return out


def cmd_generate(args):
    sprites = os.path.join(args.gamedir, "sprites")
    pal = palette()
    for st in SETS:
        rows = draw_quadrant(st)
        print("picture %d (%.4f x the 768 art), quadrant %dx%d tiles, lines %d / %d pixels, axis %s" %
              (st.picture, st.scale, st.tiles, st.tiles, st.line, st.bar,
               "through the last pixel" if st.odd else "after the last pixel"))
        for row in range(st.tiles):
            for col in range(st.tiles):
                path = os.path.join(sprites, st.files[row][col] + ".spr")
                write_sprite(path, TILE, TILE, tile_pixels(st, rows, row, col), pal)
                print("  wrote %s (%d bytes)" % (path, os.path.getsize(path)))


# ---------------------------------------------------------------------------------------------------------------
# Measuring the 1.38 art
# ---------------------------------------------------------------------------------------------------------------

OLD_GRID = (("1024top_left", "1024top", "1024top_right"),
            ("1024left", "centre", "1024right"),
            ("1024bottom_left", "1024bottom", "1024bottom_right"))


def runs(values, wanted):
    """Runs of wanted in a sequence: (first, last) index pairs."""
    out, start = [], None
    for i, v in enumerate(list(values) + [None]):
        if v == wanted and start is None:
            start = i
        elif v != wanted and start is not None:
            out.append((start, i - 1))
            start = None
    return out


def cmd_measure(args):
    sprites = os.path.join(args.gamedir, "sprites")
    picture = [bytearray(ART_SIZE) for _ in range(ART_SIZE)]   # 1 = opaque
    for r in range(3):
        for c in range(3):
            w, h, pal, pixels, texfmt = read_sprite(os.path.join(sprites, OLD_GRID[r][c] + ".spr"))
            used = sorted(set(pixels))
            print("%-16s %dx%d texFormat %d, colors %s" % (OLD_GRID[r][c], w, h, texfmt, ", ".join(
                "%d=(%d,%d,%d)" % ((i,) + tuple(pal[i * 3:i * 3 + 3])) for i in used)))
            for y in range(h):
                for x in range(w):
                    if pixels[y * w + x] != 255:
                        picture[r * 256 + y][c * 256 + x] = 1

    # edge of the hole: outermost transparent pixel of every row and column
    points = []
    for y in range(ART_SIZE):
        xs = [x for x in range(ART_SIZE) if not picture[y][x]]
        if xs:
            points += [(xs[0], y + 0.5), (xs[-1] + 1, y + 0.5)]
    for x in range(ART_SIZE):
        ys = [y for y in range(ART_SIZE) if not picture[y][x]]
        if ys:
            points += [(x + 0.5, ys[0]), (x + 0.5, ys[-1] + 1)]

    # least-squares circle: x^2 + y^2 = a x + b y + c
    def det(m):
        return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
                + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]))
    n = len(points)
    sx = sum(p[0] for p in points)
    sy = sum(p[1] for p in points)
    sxx = sum(p[0] * p[0] for p in points)
    syy = sum(p[1] * p[1] for p in points)
    sxy = sum(p[0] * p[1] for p in points)
    z = [p[0] * p[0] + p[1] * p[1] for p in points]
    m = [[sxx, sxy, sx], [sxy, syy, sy], [sx, sy, n]]
    rhs = [sum(p[0] * v for p, v in zip(points, z)), sum(p[1] * v for p, v in zip(points, z)), sum(z)]
    sol = []
    for i in range(3):
        mi = [row[:] for row in m]
        for k in range(3):
            mi[k][i] = rhs[k]
        sol.append(det(mi) / det(m))
    cx, cy = sol[0] / 2.0, sol[1] / 2.0
    radius = math.sqrt(sol[2] + cx * cx + cy * cy)
    off = [math.hypot(p[0] - cx, p[1] - cy) - radius for p in points]
    print("\nhole: center (%.2f, %.2f) radius %.2f; edge within %.2f .. %+.2f of the circle (rms %.2f, %d points)" %
          (cx, cy, radius, min(off), max(off), math.sqrt(sum(v * v for v in off) / n), n))

    w, h, pal, pixels, _ = read_sprite(os.path.join(sprites, "centre.spr"))
    print("\nreticle (centre.spr, pixel 128,128 is the center: 384,384 of the picture); runs as offsets from it:")
    for index in sorted(set(pixels) - {255}):
        print(" color %d = (%d,%d,%d)" % ((index,) + tuple(pal[index * 3:index * 3 + 3])))
        for name, lines in (("row", [pixels[y * w:(y + 1) * w] for y in range(h)]),
                            ("column", [pixels[x::w] for x in range(w)])):
            merged = []   # consecutive lines with the same runs
            for i, values in enumerate(lines):
                r = [(a - 128, b - 128) for a, b in runs(values, index) if b > a or name == "row"]
                if name == "row":   # single pixels belong to the columns' runs
                    r = [(a, b) for a, b in r if b > a]
                if not r:
                    continue
                if merged and merged[-1][2] == r and merged[-1][1] == i - 129:
                    merged[-1][1] = i - 128
                else:
                    merged.append([i - 128, i - 128, r])
            for first, last, r in merged:
                where = "%+d" % first if first == last else "%+d..%+d" % (first, last)
                print("  %s %s: %s" % (name, where, " ".join("%+d..%+d" % ab for ab in r)))


def main():
    root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    for name, func in (("generate", cmd_generate), ("measure", cmd_measure)):
        p = sub.add_parser(name)
        p.add_argument("--gamedir", default=os.path.join(root, "gamedir", "syndicate"))
        p.set_defaults(func=func)
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
