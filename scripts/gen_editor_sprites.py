#!/usr/bin/env python3
"""SBO map-editor icons: sprites/editor/<class>.spr (and editor models) drawn from a spec in this file. <class> in
file names is the class name in lower case (Limitator -> limitator.spr): every name we ship is lower case.

    scripts/gen_editor_sprites.py status [--fgd FILE] [--own]
                                                           point classes without an editor helper (--own: on the
                                                           class itself, not inherited), what to add for each, and
                                                           whether this file has a spec (exit 1 if one has none)
    scripts/gen_editor_sprites.py generate [NAME ...]      build the sprites (all, or the named ones) into
                                                           gamedir/syndicate/sprites/editor/ (--out DIR elsewhere):
                                                           an icon with a spec is drawn from it and its source PNGs
                                                           are rewritten; a hand-edited icon (HAND_EDITED) or one with
                                                           only a source PNG is built from sdk/spritesrc/editor/
    scripts/gen_editor_sprites.py build NAME|FILE.png ...  source PNG -> sprite (a hand-edited PNG back into the .spr);
                                                           a PNG from elsewhere becomes the source
    scripts/gen_editor_sprites.py model [NAME ...]         compile the editor models (studiomdlPlusPlus, $STUDIOMDL)
                                                           to gamedir/syndicate/models/editor/ (--out DIR elsewhere);
                                                           the QC/SMD/BMP compiled and a preview.png (four views of the
                                                           compiled file, read back) go to sdk/spritesrc/editor/models/
    scripts/gen_editor_sprites.py verify                   read every .spr in sprites/editor/ back (header, palette,
                                                           frame), and check that each generated file is still what
                                                           its spec draws (exit 1 on a problem)
    scripts/gen_editor_sprites.py sheet [--out DIR]        review sheet: contact.png, index.html and one PNG per icon
                                                           (1x and 4x) next to existing icons of the same family;
                                                           default build/editor-icons/
    scripts/gen_editor_sprites.py decode FILE.spr OUT.png [--scale N]   any GoldSrc sprite (frame 0) to PNG

Python 3 standard library only. Same spec, same bytes: no timestamps, no randomness, palettes in first-use order.

House style (measured from the 57 sprites we already have, docs/dev/EDITOR_ASSETS.md):
  * 64x64, one frame, sprite version 2, type VP_PARALLEL, texture format SPR_ALPHTEST, bounding radius 45.25,
    origin (-32, 32); palette index 255 is the see-through color (stored as pure blue).
  * A filled disk over the whole 64x64 square with a 1-pixel black rim; on it one flat pictogram: flat colors,
    1-pixel black outlines, a lighter top-left and darker bottom-right edge on raised parts. No anti-aliasing
    (alpha test: a pixel is either drawn or not). Text only where a word says it better than a picture.
  * Logic entities (anything that is fired and fires) share one motif: an input arrow on the left, a light tile in
    the middle with the entity's glyph, output arrow(s) on the right.
  * The disk color is the entity's family color; where the FGD class has color(), it is that color (the editor's
    box and the icon agree).

Sources: sdk/spritesrc/editor/<class>.png is each icon's source (64x64 RGBA, transparent = see-through: alpha
below 128), <class>@4x.png the same at 4x, nearest neighbor, to look at (a 256x256 picture of 4x4 blocks is also
accepted as a source). Hand edits: edit <class>.png, run `build <class>`, add the class to HAND_EDITED (from then
on the PNG is the master and `generate` no longer redraws it from the spec). More than 255 colors are reduced
deterministically; keep to flat colors as the house style does.

Adding an icon for a new entity: add an @icon function below (name, family, label), run `generate NAME`, then
`sheet` to review it, then put iconsprite("sprites/editor/<name in lower case>.spr") on the FGD class. `status` lists what is left.
Models: entities whose direction matters (a sprite always faces the camera) get a model; see MODELS below.
"""

import argparse
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
SPRITE_DIR = os.path.join(REPO, "gamedir", "syndicate", "sprites", "editor")
MODEL_DIR = os.path.join(REPO, "gamedir", "syndicate", "models", "editor")
SHEET_DIR = os.path.join(REPO, "build", "editor-icons")
SOURCE_DIR = os.path.join(REPO, "sdk", "spritesrc", "editor")   # <class>.png (the source), <class>@4x.png (view)

# Icons whose PNG source in sdk/spritesrc/editor/ was edited by hand: the PNG is then the master, `generate` builds
# the sprite from it and no longer redraws it from the spec. Add a class here in the same change as its edited PNG.
HAND_EDITED = set()
STUDIOMDL = os.environ.get("STUDIOMDL", shutil.which("studiomdlplusplus") or "studiomdlplusplus")

SIZE = 64
TRANSPARENT = 255
SEE_THROUGH_RGB = (0, 0, 255)

# ---------------------------------------------------------------------------------------------------------------
# GoldSrc sprite (.spr version 2) and PNG files
# ---------------------------------------------------------------------------------------------------------------

SPR_TYPES = {0: "parallel_upright", 1: "facing_upright", 2: "parallel", 3: "oriented", 4: "parallel_oriented"}
SPR_FORMATS = {0: "normal", 1: "additive", 2: "indexalpha", 3: "alphatest"}


class SprError(Exception):
    pass


def read_spr(path):
    """dict with the header fields, 'palette' (list of RGB) and 'frames' (list of (x, y, w, h, bytes))."""
    with open(path, "rb") as fh:
        data = fh.read()
    if len(data) < 42 or data[:4] != b"IDSP":
        raise SprError("not a sprite (no IDSP header)")
    ident, version, typ, fmt, radius, w, h, nframes, beam, sync = struct.unpack_from("<4siiifiiifi", data, 0)
    if version != 2:
        raise SprError("version %d (GoldSrc reads 2)" % version)
    if typ not in SPR_TYPES or fmt not in SPR_FORMATS:
        raise SprError("type %d / format %d out of range" % (typ, fmt))
    if nframes < 1:
        raise SprError("no frames")
    off = 40
    (ncolors,) = struct.unpack_from("<h", data, off)
    off += 2
    if ncolors != 256:
        raise SprError("palette of %d colors (GoldSrc expects 256)" % ncolors)
    palette = [tuple(data[off + i * 3:off + i * 3 + 3]) for i in range(ncolors)]
    off += ncolors * 3
    frames = []
    for i in range(nframes):
        if off + 4 > len(data):
            raise SprError("frame %d: truncated" % i)
        (ftype,) = struct.unpack_from("<i", data, off)
        off += 4
        if ftype != 0:
            raise SprError("frame %d: group frames are not read here" % i)
        fx, fy, fw, fh = struct.unpack_from("<iiii", data, off)
        off += 16
        if fw <= 0 or fh <= 0 or fw > w or fh > h or off + fw * fh > len(data):
            raise SprError("frame %d: bad size %dx%d" % (i, fw, fh))
        frames.append((fx, fy, fw, fh, data[off:off + fw * fh]))
        off += fw * fh
    if off != len(data):
        raise SprError("%d bytes after the last frame" % (len(data) - off))
    return {"type": typ, "format": fmt, "radius": radius, "width": w, "height": h, "beamlength": beam,
            "synctype": sync, "palette": palette, "frames": frames}


def spr_bytes(indices, palette):
    """One 64x64 frame in the house format (see the module docstring)."""
    assert len(indices) == SIZE * SIZE and len(palette) == 256
    out = [struct.pack("<4siiifiiifi", b"IDSP", 2, 2, 3, 45.25, SIZE, SIZE, 1, 0.0, 1),
           struct.pack("<h", 256), b"".join(bytes(c) for c in palette),
           struct.pack("<iiiii", 0, -SIZE // 2, SIZE // 2, SIZE, SIZE), bytes(indices)]
    return b"".join(out)


def png_bytes(width, height, rgba_rows):
    """RGBA PNG; rgba_rows: list of bytes/bytearray rows of 4*width."""
    raw = b"".join(b"\0" + bytes(r) for r in rgba_rows)

    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xffffffff)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def spr_to_rgba(spr, frame=0):
    """(w, h, list of RGBA tuples); index 255 is see-through for alphatest / indexalpha sprites."""
    _, _, w, h, px = spr["frames"][frame]
    pal = spr["palette"]
    seethrough = spr["format"] in (2, 3)
    return w, h, [(0, 0, 0, 0) if (seethrough and c == TRANSPARENT) else pal[c] + (255,) for c in px]


def write_png(path, w, h, pixels, scale=1):
    rows = []
    for y in range(h):
        row = bytearray()
        for x in range(w):
            row += bytes(pixels[y * w + x]) * scale
        rows.extend([row] * scale)
    with open(path, "wb") as fh:
        fh.write(png_bytes(w * scale, h * scale, rows))


def read_png(path):
    """(w, h, list of RGBA tuples). 8-bit gray, gray+alpha, RGB, RGBA and palette (with tRNS) PNGs, no interlace."""
    with open(path, "rb") as fh:
        data = fh.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("%s: not a PNG" % path)
    off, idat, plte, trns = 8, b"", None, None
    while off < len(data):
        (length,) = struct.unpack(">I", data[off:off + 4])
        tag, body = data[off + 4:off + 8], data[off + 8:off + 8 + length]
        off += 12 + length
        if tag == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif tag == b"PLTE":
            plte = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif tag == b"tRNS":
            trns = body
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(ctype)
    if depth != 8 or channels is None or interlace:
        raise ValueError("%s: only 8-bit, non-interlaced PNGs are read (save it as 8-bit RGBA)" % path)
    raw = zlib.decompress(idat)
    stride = w * channels
    rows, prev, pos = [], bytearray(stride), 0
    for _ in range(h):
        ftype, line = raw[pos], bytearray(raw[pos + 1:pos + 1 + stride])
        pos += 1 + stride
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = prev[i]
            c = prev[i - channels] if i >= channels else 0
            if ftype == 1:
                line[i] = (line[i] + a) & 255
            elif ftype == 2:
                line[i] = (line[i] + b) & 255
            elif ftype == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif ftype == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    px = []
    for line in rows:
        for x in range(w):
            v = line[x * channels:(x + 1) * channels]
            if ctype == 6:
                px.append(tuple(v))
            elif ctype == 2:
                px.append(tuple(v) + (255,))
            elif ctype == 0:
                px.append((v[0], v[0], v[0], 255))
            elif ctype == 4:
                px.append((v[0], v[0], v[0], v[1]))
            else:
                alpha = trns[v[0]] if trns is not None and v[0] < len(trns) else 255
                px.append(plte[v[0]] + (alpha,))
    return w, h, px


def png_to_icon_pixels(path):
    """A source PNG as house-format pixels: 64x64 (a 256x256 @4x picture of 4x4 blocks is read too), alpha below
    128 = see-through, at most 255 colors (more: reduced by dropping low bits until they fit; deterministic)."""
    w, h, px = read_png(path)
    if (w, h) == (SIZE * 4, SIZE * 4):
        px = [px[(y * 4) * w + x * 4] for y in range(SIZE) for x in range(SIZE)]
    elif (w, h) != (SIZE, SIZE):
        raise ValueError("%s: %dx%d, must be 64x64 (or 256x256 at 4x)" % (path, w, h))
    out = [None if c[3] < 128 else c[:3] for c in px]
    bits = 0
    while len(set(c for c in out if c is not None)) > 255:
        bits += 1
        mask = 0xff & ~((1 << bits) - 1)
        out = [None if c is None else tuple((v & mask) | ((1 << bits) >> 1) for v in c) for c in out]
    return out


def write_source(name, pixels, folder=None):
    """The source PNGs of one icon: <name>.png (64x64, the source) and <name>@4x.png (to look at)."""
    folder = folder or SOURCE_DIR
    os.makedirs(folder, exist_ok=True)
    rgba = [(0, 0, 0, 0) if c is None else c + (255,) for c in pixels]
    written = []
    for suffix, scale in (("", 1), ("@4x", 4)):
        path = os.path.join(folder, file_of(name) + suffix + ".png")
        tmp = path + ".tmp"
        write_png(tmp, SIZE, SIZE, rgba, scale)
        if os.path.isfile(path) and open(path, "rb").read() == open(tmp, "rb").read():
            os.remove(tmp)
        else:
            os.replace(tmp, path)
            written.append(path)
    return written


# ---------------------------------------------------------------------------------------------------------------
# Drawing: shapes are predicates over pixel centers; no anti-aliasing (alpha test)
# ---------------------------------------------------------------------------------------------------------------

BLACK = (0, 0, 0)
WHITE = (255, 255, 255)


def mix(c, d, t):
    return tuple(int(round(a + (b - a) * t)) for a, b in zip(c, d))


def lighter(c, t=0.35):
    return mix(c, WHITE, t)


def darker(c, t=0.3):
    return mix(c, BLACK, t)


def circle(cx, cy, r):
    r2 = r * r
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r2


def ellipse(cx, cy, rx, ry):
    return lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


def rect(x0, y0, x1, y1):
    """Pixels x0..x1-1, y0..y1-1."""
    return lambda x, y: x0 <= x < x1 and y0 <= y < y1


def rrect(x0, y0, x1, y1, r):
    def f(x, y):
        if not (x0 <= x < x1 and y0 <= y < y1):
            return False
        qx = min(max(x, x0 + r), x1 - r)
        qy = min(max(y, y0 + r), y1 - r)
        return (x - qx) ** 2 + (y - qy) ** 2 <= r * r
    return f


def poly(*pts):
    def f(x, y):
        inside = False
        n = len(pts)
        for i in range(n):
            (ax, ay), (bx, by) = pts[i], pts[(i + 1) % n]
            if (ay > y) != (by > y) and x < ax + (y - ay) * (bx - ax) / (by - ay):
                inside = not inside
        return inside
    return f


def seg(x0, y0, x1, y1, w):
    dx, dy = x1 - x0, y1 - y0
    ll = dx * dx + dy * dy or 1e-9
    h2 = (w / 2.0) ** 2

    def f(x, y):
        t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / ll))
        return (x - x0 - t * dx) ** 2 + (y - y0 - t * dy) ** 2 <= h2
    return f


def arc(cx, cy, r0, r1, a0=0, a1=360):
    """Annulus r0..r1, angles a0..a1 in degrees, counter-clockwise, 0 = right, 90 = up."""
    def f(x, y):
        d = math.hypot(x - cx, y - cy)
        if not r0 <= d <= r1:
            return False
        a = math.degrees(math.atan2(cy - y, x - cx)) % 360
        lo, hi = a0 % 360, a1 % 360
        if a1 - a0 >= 360:
            return True
        return lo <= a <= hi if lo <= hi else (a >= lo or a <= hi)
    return f


def union(*shapes):
    return lambda x, y: any(s(x, y) for s in shapes)


def minus(a, *b):
    return lambda x, y: a(x, y) and not any(s(x, y) for s in b)


def arrow(x0, y0, x1, y1, w=6, head=12, headlen=None):
    """Shaft from (x0,y0) to the tip (x1,y1); head width `head`."""
    length = math.hypot(x1 - x0, y1 - y0)
    ux, uy = (x1 - x0) / length, (y1 - y0) / length
    hl = headlen if headlen is not None else head * 0.8
    bx, by = x1 - ux * hl, y1 - uy * hl
    px, py = -uy, ux
    shaft = poly((x0 + px * w / 2, y0 + py * w / 2), (bx + px * w / 2, by + py * w / 2),
                 (bx - px * w / 2, by - py * w / 2), (x0 - px * w / 2, y0 - py * w / 2))
    tip = poly((bx + px * head / 2, by + py * head / 2), (x1, y1), (bx - px * head / 2, by - py * head / 2))
    return union(shaft, tip)


# 5x7 pixel font (uppercase, digits, a few signs)
FONT = {
    "A": ".###.#...##...#######...##...##...#", "B": "####.#...##...#####.#...##...#####.",
    "C": ".###.#...##....#....#....#...#.###.", "D": "####.#...##...##...##...##...#####.",
    "E": "######....#....####.#....#....#####", "F": "######....#....####.#....#....#....",
    "G": ".###.#...##....#.####...##...#.####", "H": "#...##...##...#######...##...##...#",
    "I": ".###...#....#....#....#....#...###.", "J": "..###...#....#....#....#.#..#..##..",
    "K": "#...##..#.#.#..##...#.#..#..#.#...#", "L": "#....#....#....#....#....#....#####",
    "M": "#...###.###.#.##.#.##...##...##...#", "N": "#...##...###..##.#.##..###...##...#",
    "O": ".###.#...##...##...##...##...#.###.", "P": "####.#...##...#####.#....#....#....",
    "Q": ".###.#...##...##...##.#.##..#..##.#", "R": "####.#...##...#####.#.#..#..#.#...#",
    "S": ".#####....#.....###.....#....#####.", "T": "#####..#....#....#....#....#....#..",
    "U": "#...##...##...##...##...##...#.###.", "V": "#...##...##...##...##...#.#.#...#..",
    "W": "#...##...##...##.#.##.#.##.#.#.#.#.", "X": "#...##...#.#.#...#...#.#.#...##...#",
    "Y": "#...##...#.#.#...#....#....#....#..", "Z": "#####....#...#...#...#...#....#####",
    "0": ".###.#...##..###.#.###..##...#.###.", "1": "..#...##....#....#....#....#...###.",
    "2": ".###.#...#....#...#...#...#...#####", "3": "####.....#....#.###.....#....#####.",
    "4": "...#...##..#.#.#..#.#####...#....#.", "5": "######....####.....#....##...#.###.",
    "6": "..##..#...#....####.#...##...#.###.", "7": "#####....#...#...#...#....#....#...",
    "8": ".###.#...##...#.###.#...##...#.###.", "9": ".###.#...##...#.####....#...#..##..",
    "_": "..............................#####", "?": ".###.#...#....#...#...#.........#..",
    "#": ".#.#..#.#.#####.#.#.#####.#.#..#.#.", "=": "..........#####.....#####..........",
    "<": "...#...#...#...#.....#.....#.....#.", ">": ".#.....#.....#.....#...#...#...#...",
    "+": ".......#....#..#####..#....#.......", "-": "...............#####...............",
    "(": "...#...#...#....#....#.....#.....#.", ")": ".#.....#.....#....#....#...#...#...",
    ".": ".........................##...##...", ",": "....................##....#...#....",
    ":": ".....##...##.........##...##.......", "!": "..#....#....#....#....#.........#..",
    "/": "....#....#...#...#...#...#....#....", "{": "..##..#....#...##....#....#.....##.",
    "}": ".##.....#....#....##...#....#..##..", "%": "##...##..#...#...#...#...#..##...##",
    "&": ".##..#..#.#.#...#...#.#.##..#..##.#",
    " ": "." * 35,
}


def text_mask(x, y, s, scale=1, spacing=1):
    """Shape: the string's pixels with its top-left at (x, y)."""
    cells = set()
    x, y = int(round(x)), int(round(y))
    cx = x
    for ch in s:
        g = FONT[ch]
        for gy in range(7):
            for gx in range(5):
                if g[gy * 5 + gx] == "#":
                    for sy in range(scale):
                        for sx in range(scale):
                            cells.add((cx + gx * scale + sx, y + gy * scale + sy))
        cx += (5 + spacing) * scale
    return lambda px, py: (int(math.floor(px)), int(math.floor(py))) in cells


def text_width(s, scale=1, spacing=1):
    return (len(s) * (5 + spacing) - spacing) * scale


class Canvas:
    """A disk icon. Everything drawn after the disk is clipped to the inside of its black rim."""

    def __init__(self, bg, size=SIZE, disk=True, rim=BLACK, radius=32.3):
        self.size = size
        self.bg, self.disk, self.radius = bg, disk, radius   # for the padding check (verify)
        self.shift = (0, 0)   # added to every shape drawn from now on (logic(): the glyph moves with its tile)
        self.px = [None] * (size * size)
        c = size / 2.0
        self.inside = [True] * (size * size)
        if disk:
            d = circle(c, c, radius)
            mask = [d(i % size + 0.5, i // size + 0.5) for i in range(size * size)]
            for i, m in enumerate(mask):
                if m:
                    self.px[i] = bg
            for i, m in enumerate(mask):
                if m and self._edge(mask, i, eight=False):
                    self.px[i] = rim
            self.inside = [m and self.px[i] != rim for i, m in enumerate(mask)]
        elif bg is not None:
            self.px = [bg] * (size * size)

    def _edge(self, mask, i, eight=True):
        x, y = i % self.size, i // self.size
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)) + (((1, 1), (1, -1), (-1, 1), (-1, -1)) if eight else ()):
            nx, ny = x + dx, y + dy
            if not (0 <= nx < self.size and 0 <= ny < self.size) or not mask[ny * self.size + nx]:
                return True
        return False

    def mask(self, shape):
        n, (sx, sy) = self.size, self.shift
        return [self.inside[i] and shape(i % n + 0.5 - sx, i // n + 0.5 - sy) for i in range(n * n)]

    def draw(self, shape, fill, outline=BLACK, shade=False, eight=True, hi=None, lo=None):
        """Fill `shape`; a 1-pixel outline around it (outside); shade: lighter top-left, darker bottom-right edge."""
        n = self.size
        m = self.mask(shape)
        if outline is not None:
            for i in range(n * n):
                if m[i] or not self.inside[i]:
                    continue
                x, y = i % n, i // n
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)) + (((1, 1), (1, -1), (-1, 1), (-1, -1))
                                                                    if eight else ()):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < n and 0 <= ny < n and m[ny * n + nx]:
                        self.px[i] = outline
                        break
        hi = hi or lighter(fill)
        lo = lo or darker(fill, 0.25)
        for i in range(n * n):
            if not m[i]:
                continue
            c = fill
            if shade:
                x, y = i % n, i // n

                def out(dx, dy):
                    nx, ny = x + dx, y + dy
                    return not (0 <= nx < n and 0 <= ny < n) or not m[ny * n + nx]
                if out(1, 0) or out(0, 1):
                    c = lo
                elif out(-1, 0) or out(0, -1):
                    c = hi
            self.px[i] = c
        return self

    def text(self, x, y, s, fill, outline=None, scale=1):
        return self.draw(text_mask(x, y, s, scale), fill, outline, eight=True)

    def text_c(self, cx, y, s, fill, outline=None, scale=1):
        return self.text(int(round(cx - text_width(s, scale) / 2.0)), y, s, fill, outline, scale)

    def pixels(self):
        return self.px


def quantize(pixels):
    """Palette (256 RGB, index 255 = see-through) and indices; colors in first-use order."""
    pal, index, out = [], {}, []
    for c in pixels:
        if c is None:
            out.append(TRANSPARENT)
            continue
        if c not in index:
            if len(pal) >= 255:
                raise ValueError("more than 255 colors")
            index[c] = len(pal)
            pal.append(c)
        out.append(index[c])
    pal = pal + [(0, 0, 0)] * (255 - len(pal)) + [SEE_THROUGH_RGB]
    return out, pal


# ---------------------------------------------------------------------------------------------------------------
# Shared motifs
# ---------------------------------------------------------------------------------------------------------------

TILE = (240, 240, 244)
ARROW = (250, 250, 250)


def logic(bg, outputs=("right",), tile=TILE, tile_box=(18, 18, 46, 46)):
    """The logic motif: input arrow, tile, output arrow(s). Returns the canvas; draw the glyph inside tile_box.

    With a fan of outputs (anything but one "right") the tile and the glyph sit 2 px further left and the input
    arrow is the narrow one, so that the three short arrows have room inside the padding (art 3 px from the rim)."""
    c = Canvas(bg)
    fan = tuple(outputs) != ("right",)
    if fan:
        c.draw(arrow(5, 32, 15, 32, w=4, head=9, headlen=5), ARROW, shade=True)
    else:
        c.draw(arrow(6, 32, 17, 32, w=6, head=12, headlen=7), ARROW, shade=True)
    x0, y0, x1, y1 = tile_box
    dx = -2 if fan else 0
    c.draw(rrect(x0 + dx, y0, x1 + dx, y1, 4), tile, shade=True)
    for o in outputs:
        if o == "right":
            c.draw(arrow(47 + dx, 32, 58, 32, w=6, head=12, headlen=7), ARROW, shade=True)
        elif o == "rightn":
            c.draw(arrow(45, 32, 57, 32, w=4, head=9, headlen=5), ARROW, shade=True)
        elif o in ("up", "upw"):
            c.draw(arrow(45, 25, 53, 17, w=4 if o == "upw" else 5, head=9 if o == "upw" else 11, headlen=5),
                   ARROW if o == "upw" else o_color(o), shade=True)
        elif o in ("down", "downw"):
            c.draw(arrow(45, 39, 53, 47, w=4 if o == "downw" else 5, head=9 if o == "downw" else 11, headlen=5),
                   ARROW if o == "downw" else o_color(o), shade=True)
    c.shift = (dx, 0)
    return c


def loop_head(cx, cy, r, angle, width, length):
    """Arrowhead at `angle` of a circle, pointing counter-clockwise along it."""
    a = math.radians(angle)
    px, py = cx + r * math.cos(a), cy - r * math.sin(a)
    tx, ty = -math.sin(a), -math.cos(a)          # counter-clockwise tangent, screen coordinates
    nx, ny = math.cos(a), -math.sin(a)
    return poly((px + nx * width / 2, py + ny * width / 2), (px + tx * length, py + ty * length),
                (px - nx * width / 2, py - ny * width / 2))


def o_color(o):
    return {"up": (90, 210, 90), "down": (235, 70, 60)}[o]


def cloud(c, cx, cy, s, fill=(245, 247, 250)):
    shape = union(circle(cx - 8 * s, cy + 1 * s, 6 * s), circle(cx, cy - 3 * s, 8.5 * s),
                  circle(cx + 9 * s, cy + 1 * s, 6 * s), rrect(cx - 13 * s, cy, cx + 14 * s, cy + 7 * s, 3 * s))
    c.draw(shape, fill, shade=True, lo=(200, 206, 216))


def bulb(c, cx, cy, s, fill=(255, 240, 120), base=(170, 170, 180)):
    c.draw(union(circle(cx, cy, 9 * s), poly((cx - 6 * s, cy + 4 * s), (cx + 6 * s, cy + 4 * s),
                                              (cx + 4 * s, cy + 12 * s), (cx - 4 * s, cy + 12 * s))),
           fill, shade=True)
    c.draw(rrect(cx - 4.5 * s, cy + 12 * s, cx + 4.5 * s, cy + 18 * s, 1.5 * s), base, shade=True)


def entity_cube(c, x0, y0, s, fill=(250, 170, 60)):
    """A small isometric-ish box: the stand-in for "an entity"."""
    c.draw(poly((x0, y0 + 4 * s), (x0 + 6 * s, y0), (x0 + 16 * s, y0), (x0 + 10 * s, y0 + 4 * s)), lighter(fill))
    c.draw(poly((x0 + 10 * s, y0 + 4 * s), (x0 + 16 * s, y0), (x0 + 16 * s, y0 + 10 * s),
                (x0 + 10 * s, y0 + 14 * s)), darker(fill, 0.2))
    c.draw(rect(x0, y0 + 4 * s, x0 + 10 * s, y0 + 14 * s), fill)


def toggle(c, x0, y0, on=True, w=22, h=12):
    c.draw(rrect(x0, y0, x0 + w, y0 + h, h / 2.0), (90, 200, 100) if on else (150, 150, 160), shade=True)
    kx = x0 + w - h / 2.0 if on else x0 + h / 2.0
    c.draw(circle(kx, y0 + h / 2.0, h / 2.0 - 1.5), (250, 250, 250), shade=True)


def lcd(c, x0, y0, x1, y1, s, fg=(255, 200, 40), scale=1):
    c.draw(rrect(x0, y0, x1, y1, 2), (30, 34, 40))
    c.text_c((x0 + x1) / 2.0, (y0 + y1) / 2.0 - 3.5 * scale, s, fg, scale=scale)


# ---------------------------------------------------------------------------------------------------------------
# Icon specs. Family colors: logic = FGD color(180 120 255), numbers = color(170 221 85), vectors =
# color(170 179 43), positions = color(128 200 64), state = color(128 128 255), light styles = color(255 255 128),
# world screens = color(80 255 120) toned down.
# ---------------------------------------------------------------------------------------------------------------

LOGIC = (180, 120, 255)
NUMBERS = (170, 221, 85)
VECTORS = (170, 179, 43)
POSITIONS = (128, 200, 64)
STATE = (128, 128, 255)
LIGHTS = (255, 255, 128)
SCREENS = (80, 214, 120)

ICONS = {}       # name -> dict(fn, family, label, where)

FAMILY_REFS = {  # existing icons shown next to a family on the review sheet
    "logic": ["trigger_relay", "multi_manager", "trigger_changetarget", "trigger_counter"],
    "numbers": ["trigger_changevalue", "trigger_copyvalue", "game_counter", "game_counter_set"],
    "vectors": ["trigger_changevalue", "trigger_copyvalue", "info_null", "info_landmark"],
    "state": ["multisource", "env_global", "trigger_auto", "game_team_master"],
    "weather": ["env_fog", "env_fade", "info_sunlight", "env_global"],
    "effects": ["env_explosion", "env_spark", "env_shooter", "env_blood"],
    "rules": ["game_end", "game_score", "env_message", "game_text"],
    "lights": ["light_bounce", "light_surface", "info_texlights", "light_shadow"],
    "screens": ["game_text", "env_message", "info_compile_parameters", "trigger_relay"],
    "sound": ["ambient_generic", "target_cdaudio", "speaker", "env_sound"],
    "compile": ["info_minlights", "info_texlights", "info_compile_parameters", "info_sunlight"],
}

FAMILY_TITLES = {
    "weather": "Weather and sky (environment)", "effects": "Effects: smoke, fire, particles, explosions",
    "rules": "Game rules and messages", "logic": "Logic: triggered, fires targets",
    "numbers": "Numbers (locus / calc)", "vectors": "Vectors and positions (locus / calc)",
    "state": "On/off state", "lights": "Light styles", "screens": "World screens (VGUI2)",
    "sound": "Sound and music", "compile": "Compiler hints (SDHLT)",
}


def icon(name, family, label, where="develop"):
    def deco(fn):
        ICONS[name] = {"fn": fn, "family": family, "label": label, "where": where}
        return fn
    return deco


# --- develop: weather and sky ---------------------------------------------------------------------------------

@icon("env_rain", "weather", "Rain (whole map)")
def _env_rain():
    c = Canvas((100, 150, 255))
    cloud(c, 32, 22, 1.25)
    drop = (40, 80, 220)
    for x, y in ((18, 38), (30, 40), (42, 38), (24, 50), (36, 51)):
        c.draw(seg(x + 3, y - 4, x - 1, y + 4, 3), (200, 225, 255), outline=drop, eight=False)
    return c


@icon("env_snow", "weather", "Snow (whole map)")
def _env_snow():
    c = Canvas((150, 185, 230))
    cloud(c, 32, 22, 1.25)
    for x, y in ((19, 41), (33, 43), (46, 40), (26, 53), (40, 53)):
        flake = union(seg(x - 4, y, x + 4, y, 1.5), seg(x - 2.5, y - 3.5, x + 2.5, y + 3.5, 1.5),
                      seg(x - 2.5, y + 3.5, x + 2.5, y - 3.5, 1.5))
        c.draw(flake, WHITE, outline=(60, 90, 150), eight=False)
    return c


@icon("env_sky", "weather", "3D sky camera")
def _env_sky():
    c = Canvas((128, 192, 255))
    c.draw(circle(42, 18, 7), (255, 214, 60), shade=True)
    cloud(c, 25, 21, 0.95)
    body = (70, 74, 84)
    c.draw(rrect(14, 33, 50, 52, 3), body, shade=True)
    c.draw(rrect(20, 29, 31, 34, 1), body)
    c.draw(circle(32, 42.5, 8), (40, 40, 48))
    c.draw(circle(32, 42.5, 5.5), (60, 140, 230), outline=None, shade=True, hi=(150, 210, 255))
    c.draw(circle(30, 40.5, 1.5), WHITE, outline=None)
    c.draw(rect(43, 36, 47, 39), (240, 80, 70), outline=None)
    return c


# --- feat/cs-ambient-sound: map music ----------------------------------------------------------------------------

@icon("ambient_music", "sound", "Map music (MP3, everywhere)", where="feat/cs-ambient-sound")
def _ambient_music():
    c = Canvas((220, 30, 220))                                  # the FGD's color()
    note = WHITE
    # kept well inside the rim (padding rule): notes in the upper half, the label in the middle of the lower half
    c.draw(seg(26, 14, 44, 10, 4), note, shade=True)            # the beam
    c.draw(rect(24, 14, 27, 34), note, shade=True)              # the stems
    c.draw(rect(42, 10, 45, 30), note, shade=True)
    c.draw(ellipse(21, 35, 5.5, 4.5), note, shade=True)         # the heads
    c.draw(ellipse(39, 31, 5.5, 4.5), note, shade=True)
    c.draw(rrect(20, 41, 44, 51, 2), (70, 10, 70))
    c.text_c(32, 43, "MP3", WHITE)
    return c


# --- develop: effects -----------------------------------------------------------------------------------------

def smoke(c, base_y=54, gray=(120, 118, 118), dots=False):
    c.draw(rrect(24, base_y - 5, 40, base_y + 2, 2), (80, 82, 88), shade=True)
    for cx, cy, r in ((32, base_y - 11, 7), (27, base_y - 21, 8), (36, base_y - 30, 9), (29, base_y - 40, 7.5)):
        c.draw(circle(cx, cy, r), gray, shade=True, hi=lighter(gray, 0.4))
    if dots:
        for x, y in ((47, 24), (51, 33), (16, 30), (13, 40), (46, 44)):
            c.draw(rect(x, y, x + 3, y + 3), (225, 225, 225))


@icon("env_smoker", "effects", "Smoke emitter")
def _env_smoker():
    c = Canvas((205, 200, 196))
    smoke(c)
    return c


@icon("Fumee_particules", "effects", "Smoke particle emitter (1.38)")
def _fumee():
    c = Canvas((150, 145, 145))
    smoke(c, gray=(214, 212, 210), dots=True)
    return c


@icon("Flamme_particules", "effects", "Fire particle emitter (1.38)")
def _flamme():
    c = Canvas((150, 40, 30))
    c.draw(poly((32, 8), (44, 28), (47, 40), (42, 51), (32, 55), (22, 51), (17, 40), (22, 30), (27, 34)),
           (255, 120, 20), shade=True, hi=(255, 170, 60))
    c.draw(poly((33, 26), (40, 40), (38, 49), (32, 52), (26, 49), (25, 41), (29, 38)), (255, 220, 60),
           outline=None)
    for x, y in ((49, 18), (15, 22), (52, 30), (12, 34)):
        c.draw(rect(x, y, x + 3, y + 3), (255, 200, 60))
    return c


@icon("env_particle", "effects", "Aurora particle system")
def _env_particle():
    c = Canvas((255, 160, 40))
    c.draw(poly((25, 54), (39, 54), (35, 45), (29, 45)), (70, 74, 84), shade=True)
    pts = ((32, 38, 3.5), (25, 31, 3), (39, 30, 3), (19, 23, 2.5), (31, 23, 3), (45, 21, 2.5), (15, 16, 2),
           (25, 14, 2.5), (37, 13, 2.5), (48, 14, 2), (46, 33, 2), (18, 36, 2))
    cols = ((255, 255, 255), (255, 240, 120), (120, 220, 255), (255, 255, 255))
    for i, (x, y, r) in enumerate(pts):
        c.draw(circle(x, y, r), cols[i % len(cols)])
    return c


@icon("nuclear_explosion", "effects", "Nuclear explosion")
def _nuclear():
    c = Canvas((255, 205, 0))
    k = (20, 20, 20)
    for a in (90, 210, 330):
        c.draw(arc(32, 33, 7, 24, a - 30, a + 30), k, outline=None)
    c.draw(circle(32, 33, 4.5), k, outline=None)
    return c


# --- develop: game rules and messages -------------------------------------------------------------------------

@icon("max_round", "rules", "Maximum rounds")
def _max_round():
    c = Canvas((255, 70, 150))
    cx, cy, r0, r1, end = 32, 26, 10, 15, 35
    c.draw(arc(cx, cy, r0, r1, end, 360 + end - 70), WHITE, shade=True)
    c.draw(loop_head(cx, cy, (r0 + r1) / 2.0, end, 13, 8), WHITE)       # the arrowhead at the open end
    c.text_c(cx, cy - 7, "5", (40, 20, 40), scale=2)
    c.draw(rrect(18, 44, 46, 54, 2), (60, 20, 50))
    c.text_c(32, 46, "MAX", WHITE)
    return c


@icon("env_tutor", "rules", "Tutor message (CZ tutor)")
def _env_tutor():
    c = Canvas((70, 170, 170))
    c.draw(rrect(9, 15, 55, 47, 4), (30, 40, 56), shade=True, hi=(60, 74, 96))
    c.draw(rect(12, 18, 52, 23), (90, 130, 190), outline=None)
    c.draw(circle(21, 34, 6.5), (255, 210, 60), shade=True)
    c.text_c(21, 31, "?", (40, 30, 0))
    for y in (29, 34, 39):
        c.draw(rect(31, y, 50 if y != 39 else 44, y + 2), (210, 220, 230), outline=None)
    return c


# --- develop: redrawn, replacing the interim copies from Descen's pack ------------------------------------------

REPLACED = "develop (replaces the interim pack copy)"


@icon("game_timer", "rules", "Game timer (HUD countdown)", where=REPLACED)
def _game_timer():
    c = Canvas((220, 30, 220))
    c.draw(rect(29, 12, 35, 17), (90, 96, 110), shade=True)
    c.draw(rrect(10, 17, 54, 46, 4), (90, 96, 110), shade=True)
    lcd(c, 13, 20, 51, 43, "30", fg=(255, 70, 60), scale=3)
    c.draw(rrect(20, 48, 44, 53, 2), (60, 20, 60), outline=BLACK)
    c.draw(rect(22, 49, 35, 52), (255, 200, 40), outline=None)
    return c


@icon("trigger_sequence", "logic", "Trigger sequence (sequences/<map>.seq)", where=REPLACED)
def _trigger_sequence():
    c = logic(LOGIC)
    for i, col in enumerate(((90, 170, 240), (250, 160, 50), (90, 200, 110))):
        y = 21 + i * 8
        c.text(21, y, str(i + 1), (40, 40, 40))
        c.draw(rect(28 + i * 5, y + 1, 34 + i * 5, y + 6), col, outline=None)
    return c


@icon("Limitator", "rules", "Ammo limiter", where=REPLACED)
def _limitator():
    c = Canvas((255, 70, 150))
    for i, h in enumerate((19, 25, 31)):
        x = 15 + i * 12
        c.draw(union(rect(x, 53 - h, x + 9, 53), ellipse(x + 4.5, 53 - h, 4.5, 6)),
               (200, 120, 60), shade=True)
        c.draw(rect(x, 53 - h + 4, x + 9, 53), (235, 190, 70), shade=True)
    c.draw(rect(10, 16, 54, 20), (40, 20, 40))
    c.draw(rrect(20, 7, 44, 15, 2), (40, 20, 40))
    c.text_c(32, 8, "MAX", WHITE)
    return c


@icon("multispawn", "rules", "Multi-spawn point manager", where=REPLACED)
def _multispawn():
    c = Canvas((90, 140, 220))
    for cx, n, col in ((19, "1", (150, 150, 160)), (45, "2", (90, 200, 110))):
        c.draw(ellipse(cx, 44, 9, 4.5), col, shade=True)
        c.draw(union(circle(cx, 22, 4.5), rrect(cx - 5, 27, cx + 5, 40, 2)), (250, 250, 250) if n == "2"
               else (200, 200, 210), shade=True)
        c.text_c(cx, 48, n, WHITE, outline=BLACK)
    c.draw(arrow(26, 32, 38, 32, w=4, head=10, headlen=6), (255, 220, 60), shade=True)
    return c


@icon("game_messagebox", "rules", "Message box", where=REPLACED)
def _game_messagebox():
    c = Canvas((110, 160, 230))
    c.draw(rrect(11, 16, 53, 49, 3), (240, 240, 244), shade=True)
    c.draw(rect(12, 17, 52, 23), (60, 90, 160), outline=None)
    c.draw(rect(46, 18, 51, 22), (230, 70, 60), outline=None)
    for y, x1 in ((26, 48), (30, 44), (34, 48)):
        c.draw(rect(15, y, x1, y + 2), (110, 110, 120), outline=None)
    c.draw(rrect(34, 38, 50, 46, 2), (90, 200, 110), outline=(30, 60, 40))
    c.text_c(42, 39, "OK", WHITE)
    return c


# --- branch feat/video-player -----------------------------------------------------------------------------------

@icon("game_playvideo", "rules", "Video (full screen)", where="feat/video-player")
def _game_playvideo():
    c = Canvas((110, 160, 230))
    # a strip of film: dark frame with sprocket holes top and bottom, a play button in the picture
    film = (40, 44, 54)
    c.draw(rrect(11, 17, 53, 47, 2), film, shade=True, hi=(70, 76, 90))
    for x in range(14, 51, 6):
        c.draw(rect(x, 19, x + 3, 21), (230, 230, 236), outline=None)
        c.draw(rect(x, 43, x + 3, 45), (230, 230, 236), outline=None)
    c.draw(rect(14, 24, 50, 40), (70, 120, 200), outline=None)
    c.draw(poly((27, 26), (27, 38), (38, 32)), WHITE, shade=True)
    return c


# --- branch feat/point-worldtext ---------------------------------------------------------------------------------

@icon("point_worldtext", "rules", "World text (text drawn in the world)", where="feat/point-worldtext")
def _point_worldtext():
    c = Canvas((255, 190, 60))
    # a text panel standing in the world, seen at an angle
    c.draw(poly((11, 19), (53, 15), (53, 47), (11, 44)), (40, 52, 70), shade=True, hi=(70, 86, 110))
    c.text_c(32, 21, "ABC", WHITE, outline=BLACK, scale=2)
    for y, x1 in ((37, 46), (41, 40)):
        c.draw(rect(18, y, x1, y + 2), (120, 220, 255), outline=None)
    return c


# --- branch feat/movewith ---------------------------------------------------------------------------------------

@icon("trigger_changeparent", "logic", "Trigger change parent", where="feat/movewith")
def _changeparent():
    c = logic(LOGIC)
    c.draw(seg(27, 27, 37, 38, 3), (40, 40, 40), outline=None)
    c.draw(rect(21, 21, 32, 32), (70, 120, 220), shade=True)
    c.draw(rect(34, 35, 42, 43), (250, 160, 50), shade=True)
    return c


# --- branch feat/mapper-entities: logic -------------------------------------------------------------------------

@icon("trigger_condition", "logic", "Trigger condition", where="feat/mapper-entities")
def _condition():
    c = logic(LOGIC, outputs=("up", "down"))
    c.draw(poly((32, 20), (44, 32), (32, 44), (20, 32)), (255, 215, 60), shade=True)
    c.text_c(32, 25, "?", (40, 30, 0), scale=2)
    return c


@icon("trigger_random", "logic", "Trigger random", where="feat/mapper-entities")
def _random():
    c = logic(LOGIC, outputs=("upw", "rightn", "downw"))
    for x, y in ((24, 24), (37, 24), (30.5, 30.5), (24, 37), (37, 37)):
        c.draw(circle(x + 1, y + 1, 2.8), (30, 30, 30), outline=None)
    return c


@icon("trigger_timer", "logic", "Trigger timer", where="feat/mapper-entities")
def _timer():
    c = logic(LOGIC)
    c.draw(rect(30, 18, 34, 22), (90, 90, 100))
    c.draw(circle(32, 33, 10), WHITE)
    c.draw(seg(32, 33, 32, 25.5, 2), (30, 30, 30), outline=None)
    c.draw(seg(32, 33, 37, 33, 2), (220, 50, 50), outline=None)
    return c


@icon("trigger_entity_iterator", "logic", "Trigger entity iterator", where="feat/mapper-entities")
def _iterator():
    c = logic(LOGIC)
    for i, y in enumerate((21, 29, 37)):
        c.draw(rect(29, y, 42, y + 6), (250, 160, 50) if i == 1 else (200, 200, 210))
    c.draw(poly((21, 29), (27, 32), (21, 35)), (60, 60, 60), outline=None)
    return c


@icon("trigger_sensor", "logic", "Trigger sensor", where="feat/trigger-sensor")
def _sensor():
    c = logic(LOGIC)
    for r0 in (6.5, 11, 15.5):
        c.draw(arc(26, 32, r0, r0 + 2.5, -45, 45), (60, 180, 70), outline=None)
    c.draw(circle(26, 32, 3.5), (250, 160, 50))
    return c


@icon("trigger_playerfreeze", "logic", "Trigger player freeze", where="feat/featureful-entities")
def _playerfreeze():
    c = logic(LOGIC)
    ice = (150, 215, 255)
    c.draw(rrect(21, 21, 43, 43, 3), ice, shade=True, hi=(220, 245, 255), lo=(90, 160, 220))
    flake = (30, 90, 170)
    c.draw(union(seg(32, 24, 32, 40, 2), seg(25, 28, 39, 36, 2), seg(25, 36, 39, 28, 2)), flake, outline=None)
    return c


@icon("trigger_killmonster", "logic", "Trigger kill monster", where="feat/featureful-entities")
def _killmonster():
    c = logic(LOGIC)
    bone = (245, 240, 225)
    c.draw(union(circle(32, 29, 9), rrect(26, 32, 38, 42, 2)), bone, shade=True)
    k = (40, 30, 30)
    c.draw(circle(28.5, 29, 2.5), k, outline=None)
    c.draw(circle(35.5, 29, 2.5), k, outline=None)
    for x in (28, 31, 34):
        c.draw(rect(x, 38, x + 2, 42), k, outline=None)
    return c


@icon("trigger_hurt_remote", "logic", "Trigger hurt (no touch)", where="feat/featureful-entities")
def _hurt_remote():
    c = logic(LOGIC)
    c.draw(poly((35, 20), (24, 34), (31, 34), (28, 44), (40, 29), (33, 29), (37, 20)), (255, 70, 50), shade=True)
    return c


@icon("env_dlight", "lights", "Dynamic light", where="feat/featureful-entities")
def _env_dlight():
    c = Canvas(LIGHTS)
    ray = (250, 150, 30)
    for a in range(0, 360, 45):
        r = math.radians(a)
        c.draw(seg(32 + 15 * math.cos(r), 27 - 15 * math.sin(r), 32 + 21 * math.cos(r), 27 - 21 * math.sin(r), 3),
               ray, outline=None)
    bulb(c, 32, 25, 1.0)
    return c


@icon("locus_beam", "vectors", "Locus beam", where="feat/featureful-entities")
def _locus_beam():
    c = Canvas(VECTORS)
    c.draw(seg(19, 42, 42, 25, 5), (140, 220, 255), outline=(30, 90, 170))
    c.draw(seg(19, 42, 42, 25, 1.5), WHITE, outline=None)
    c.draw(circle(18, 43, 5), (250, 160, 50), shade=True)
    entity_cube(c, 35, 17, 1.0)
    return c


@icon("player_calc_ratio", "numbers", "Player number (health, armor, ammo)", where="feat/featureful-entities")
def _player_calc_ratio():
    c = Canvas(NUMBERS)
    body = (70, 110, 200)
    c.draw(circle(22, 22, 6), (250, 210, 170), shade=True)
    c.draw(rrect(13, 29, 31, 50, 4), body, shade=True)
    c.draw(arrow(33, 36, 41, 36, w=3, head=8, headlen=5), WHITE)
    c.draw(rrect(42, 27, 56, 45, 2), TILE, shade=True)
    c.text_c(49, 32, "%", (30, 60, 20))
    return c


@icon("trigger_compare", "numbers", "Trigger compare", where="feat/mapper-entities")
def _compare():
    c = logic(NUMBERS, outputs=("upw", "rightn", "downw"))
    k = (40, 40, 40)
    c.draw(union(seg(32, 24, 32, 40, 2), seg(21, 25, 43, 25, 2), rect(26, 40, 39, 43)), k, outline=None)
    for px in (24, 40):
        c.draw(seg(px, 25, px, 31, 1), k, outline=None)
        c.draw(minus(ellipse(px, 31, 5.5, 5), rect(0, 0, 64, 31)), (250, 160, 50))
    return c


# --- branch feat/mapper-entities: numbers, vectors, positions --------------------------------------------------

@icon("game_number", "numbers", "Game number", where="feat/mapper-entities")
def _game_number():
    c = Canvas(NUMBERS)
    c.draw(rrect(9, 18, 55, 46, 4), (90, 96, 110), shade=True)
    lcd(c, 13, 22, 51, 42, "42", scale=2)
    return c


@icon("locus_variable", "numbers", "Locus variable", where="feat/mapper-entities")
def _locus_variable():
    c = Canvas(NUMBERS)
    c.draw(rrect(10, 16, 54, 48, 4), TILE, shade=True)
    c.text_c(32, 25, "{X}", (30, 60, 20), scale=2)
    return c


@icon("calc_ratio", "numbers", "Calculate number", where="feat/mapper-entities")
def _calc_ratio():
    c = Canvas(NUMBERS)
    c.draw(rrect(10, 16, 54, 48, 4), TILE, shade=True)
    c.text_c(32, 18, "F(X)", (30, 60, 20), scale=1)
    c.draw(seg(14, 44, 50, 44, 1), (120, 130, 120), outline=None)
    c.draw(seg(14, 44, 14, 28, 1), (120, 130, 120), outline=None)
    for i in range(16):
        x = 15 + i * 2.2
        c.draw(rect(x, 43 - int(12 * math.sin(i / 15.0 * math.pi * 0.5)), x + 2,
                    44 - int(12 * math.sin(i / 15.0 * math.pi * 0.5))), (230, 70, 60), outline=None)
    return c


@icon("calc_eval_number", "numbers", "Calculate number from numbers", where="feat/mapper-entities")
def _calc_eval():
    c = Canvas(NUMBERS)
    c.draw(rrect(17, 9, 47, 56, 4), (70, 76, 90), shade=True)
    lcd(c, 20, 13, 44, 25, "A+B")
    keys = ((240, 240, 244), (240, 240, 244), (255, 170, 60))
    for r in range(3):
        for k in range(3):
            c.draw(rect(21 + k * 8, 30 + r * 8, 26 + k * 8, 35 + r * 8), keys[k], shade=True)
    return c


@icon("calc_numfromvec", "vectors", "Calculate number from vector", where="feat/mapper-entities")
def _numfromvec():
    c = Canvas(VECTORS)
    c.draw(arrow(10, 44, 27, 21, w=4, head=10, headlen=8), (60, 140, 230), shade=True)
    c.draw(arrow(30, 33, 40, 33, w=3, head=8, headlen=5), WHITE)
    c.draw(rrect(42, 25, 57, 41, 2), TILE, shade=True)
    c.text_c(49.5, 30, "#", (40, 50, 10))
    return c


@icon("calc_vecfromnums", "vectors", "Calculate vector from numbers", where="feat/mapper-entities")
def _vecfromnums():
    c = Canvas(VECTORS)
    for i, ch in enumerate("XYZ"):
        y = 17 + i * 11
        c.draw(rrect(11, y, 24, y + 9, 2), TILE, shade=True)
        c.text_c(17.5, y + 1, ch, (40, 50, 10))
    c.draw(arrow(26, 32, 33, 32, w=3, head=8, headlen=5), WHITE)
    c.draw(arrow(37, 45, 52, 21, w=4, head=10, headlen=8), (60, 140, 230), shade=True)
    return c


@icon("calc_subvelocity", "vectors", "Calculate vector from entity", where="feat/mapper-entities")
def _subvelocity():
    c = Canvas(VECTORS)
    entity_cube(c, 12, 29, 1.4)
    c.draw(arrow(32, 35, 53, 19, w=4, head=10, headlen=8), (60, 140, 230), shade=True)
    return c


@icon("calc_velocity_path", "vectors", "Calculate vector between positions", where="feat/mapper-entities")
def _velocity_path():
    c = Canvas(VECTORS)
    c.draw(circle(19, 39, 5), (250, 160, 50), shade=True)
    c.text_c(19, 46, "A", WHITE, outline=BLACK)
    c.draw(circle(45, 25, 5), (250, 160, 50), shade=True)
    c.text_c(45, 12, "B", WHITE, outline=BLACK)
    c.draw(arrow(23, 36, 40, 27, w=4, head=10, headlen=8), (60, 140, 230), shade=True)
    return c


@icon("calc_velocity_polar", "vectors", "Calculate rotated vector", where="feat/mapper-entities")
def _velocity_polar():
    c = Canvas(VECTORS)
    c.draw(arrow(15, 46, 53, 46, w=3, head=8, headlen=6), (200, 200, 205), shade=True)
    c.draw(arc(15, 46, 20, 23, 6, 40), (255, 220, 60))
    c.draw(loop_head(15, 46, 21.5, 40, 9, 6), (255, 220, 60))
    c.draw(arrow(15, 46, 36, 13, w=4, head=10, headlen=8), (60, 140, 230), shade=True)
    c.draw(circle(15, 46, 3), (40, 40, 40), outline=None)
    return c


@icon("calc_position", "vectors", "Calculate position", where="feat/mapper-entities")
def _calc_position():
    c = Canvas(POSITIONS)
    entity_cube(c, 12, 29, 1.4)
    c.draw(arc(40, 24, 8, 11), (230, 50, 50))
    c.draw(union(seg(40, 9, 40, 18, 2), seg(40, 30, 40, 39, 2), seg(25, 24, 34, 24, 2), seg(46, 24, 55, 24, 2)),
           (230, 50, 50))
    c.draw(circle(40, 24, 2), (230, 50, 50), outline=None)
    return c


# --- branch feat/mapper-entities phase C: on/off state ----------------------------------------------

@icon("env_state", "state", "Local state (a master)", where="feat/mapper-entities (phase C)")
def _env_state():
    c = Canvas(STATE)
    c.draw(rrect(10, 18, 54, 46, 4), TILE, shade=True)
    toggle(c, 21, 26, on=True, w=22, h=12)
    c.text_c(32, 49, "ON", WHITE, outline=BLACK)
    return c


@icon("calc_state", "state", "Calculate state (a master)", where="feat/mapper-entities (phase C)")
def _calc_state():
    c = Canvas(STATE)
    wire = (40, 40, 40)
    c.draw(union(seg(22, 24.5, 28, 24.5, 2), seg(22, 39.5, 28, 39.5, 2)), wire, outline=None)
    toggle(c, 7, 20, on=True, w=16, h=9)
    toggle(c, 7, 35, on=False, w=16, h=9)
    c.draw(union(rect(28, 18, 37, 47), minus(circle(37, 32.5, 14.5), rect(0, 0, 37, 64))), TILE, shade=True)
    c.text_c(37, 26, "&", wire, scale=2)
    c.draw(arrow(53, 32, 60, 32, w=4, head=9, headlen=5), ARROW, shade=True)
    return c


# --- branch feat/mapper-entities phase D: light styles ----------------------------------------------

@icon("trigger_lightstyle", "lights", "Trigger change lightstyle (with fades)",
      where="feat/mapper-entities (phase D)")
def _trigger_lightstyle():
    c = logic(LIGHTS)
    bulb(c, 32, 26, 0.75)
    c.draw(union(*[seg(20 + i * 4, 43 if i % 2 else 40, 24 + i * 4, 40 if i % 2 else 43, 1.5) for i in range(6)]),
           (40, 40, 40), outline=None)
    return c


@icon("global_light_control", "lights", "Global light control",
      where="feat/mapper-entities (phase D)")
def _global_light_control():
    c = Canvas(LIGHTS)
    c.draw(minus(circle(32, 32, 17), rect(32, 0, 64, 64)), (255, 210, 60), shade=True)
    c.draw(minus(circle(32, 32, 17), rect(0, 0, 32, 64)), (40, 50, 90), shade=True)
    c.draw(minus(circle(37, 28, 8), circle(41, 25, 7)), (230, 230, 240), outline=None)
    for a in range(90, 271, 45):
        r = math.radians(a)
        c.draw(seg(32 + 20 * math.cos(r), 32 - 20 * math.sin(r), 32 + 24.5 * math.cos(r), 32 - 24.5 * math.sin(r),
                   3), (255, 210, 60))
    return c


# --- branch feat/world-screens ----------------------------------------------------------------------------------

def screen_picture(c, x0, y0, x1, y1):
    c.draw(rect(x0, y0, x1, y1), (24, 44, 80), outline=None)
    c.draw(rect(x0, y0, x1, y0 + 4), (70, 120, 200), outline=None)
    c.draw(rrect(x0 + 4, y1 - 9, x0 + 17, y1 - 3, 1), (90, 200, 110), outline=(200, 255, 210))
    c.draw(rect(x0 + 4, y0 + 8, x1 - 4, y0 + 10), (180, 200, 230), outline=None)
    c.draw(rect(x0 + 4, y0 + 13, x1 - 10, y0 + 15), (180, 200, 230), outline=None)


def cursor(c, x, y):
    c.draw(poly((x, y), (x, y + 12), (x + 3, y + 9), (x + 6, y + 14), (x + 8, y + 13), (x + 5, y + 8),
                (x + 9, y + 8)), WHITE)


@icon("vgui_screen", "screens", "World screen (VGUI2 panel)", where="feat/world-screens")
def _vgui_screen():
    c = Canvas(SCREENS)
    c.draw(rrect(11, 15, 53, 45, 3), (60, 64, 72), shade=True)
    screen_picture(c, 14, 18, 50, 42)
    c.draw(poly((26, 45), (38, 45), (41, 51), (23, 51)), (60, 64, 72), shade=True)
    cursor(c, 35, 29)
    return c


@icon("vgui_control_proxy", "screens", "World screen control", where="feat/world-screens")
def _vgui_control_proxy():
    c = logic(SCREENS)
    c.draw(rect(21, 24, 43, 30), (180, 200, 230))
    c.draw(rrect(21, 34, 35, 41, 1), (90, 200, 110), outline=(30, 60, 40))
    cursor(c, 31, 36)
    return c


# --- develop: redrawn in house style, replacing the off-style pack icons -----------------------------------------

REDRAWN = "develop (redrawn in house style)"


@icon("info_target", "vectors", "Beam target / point (FGD color 200 100 50)", where=REDRAWN)
def _info_target():
    # was a half-size disk (32x32 on the 64x64 frame); now the full disk like every other icon
    c = Canvas((200, 100, 50))
    mark = (255, 236, 200)
    c.draw(union(rrect(29, 20, 35, 44, 2), rrect(20, 29, 44, 35, 2)), mark, shade=True)
    c.draw(circle(32, 32, 2), (90, 40, 20), outline=None)
    for a in (45, 135, 225, 315):           # four arrowheads pointing at the point
        r = math.radians(a)
        ux, uy = math.cos(r), -math.sin(r)
        px, py = -uy, ux
        tip, base, half = 13, 21, 5.5
        c.draw(poly((32 + ux * tip, 32 + uy * tip), (32 + ux * base + px * half, 32 + uy * base + py * half),
                    (32 + ux * base - px * half, 32 + uy * base - py * half)), mark, shade=True)
    return c


@icon("mission_failed", "rules", "Mission failed (ends the co-op mission as failed)", where=REDRAWN)
def _mission_failed():
    # the mission's orders on a clipboard, struck through: fired, it ends the co-op mission as failed
    c = Canvas((215, 50, 50))
    c.draw(rrect(17, 13, 47, 53, 3), (150, 100, 60), shade=True)            # the board
    c.draw(rect(21, 18, 43, 49), (246, 244, 236), outline=None)             # the orders
    for y, x1 in ((22, 39), (26, 41), (30, 37), (34, 41), (38, 39), (42, 36)):
        c.draw(rect(23, y, x1, y + 2), (160, 160, 170), outline=None)
    c.draw(rrect(26, 10, 38, 16, 2), (170, 172, 180), shade=True)           # the clip
    c.draw(union(seg(22, 21, 42, 46, 5), seg(42, 21, 22, 46, 5)), (230, 30, 30), shade=True)
    return c


@icon("info_null", "vectors", "Null point (a spotlight's target; removed at map load)", where=REDRAWN)
def _info_null():
    # was a half-size disk like info_target's; a point that is nothing: the null sign, so it is not taken for
    # info_target's crosshair
    c = Canvas((128, 255, 146))
    c.draw(union(arc(32, 32, 9, 15), seg(19, 45, 45, 19, 5)), (250, 250, 250), shade=True)
    return c


@icon("cycler_sprite", "effects", "Sprite cycler placeholder (the FGD Model key default)", where=REDRAWN)
def _cycler_sprite():
    # cycler's red (its editor box): a film strip of three frames, a spark growing frame by frame (an animated
    # sprite), and SPR. The FGD gives it as the Model key's default, so the editor shows it until a sprite is set
    c = Canvas((232, 88, 88))
    c.draw(rect(10, 20, 54, 42), (40, 40, 48))
    for x in range(12, 54, 5):
        c.draw(rect(x, 21, x + 2, 23), (200, 200, 210), outline=None)
        c.draw(rect(x, 39, x + 2, 41), (200, 200, 210), outline=None)
    for i, r in enumerate((2.0, 3.0, 4.2)):
        x0 = 13 + i * 14
        c.draw(rect(x0, 25, x0 + 11, 37), (236, 238, 246), outline=None)
        c.draw(circle(x0 + 5.5, 31, r), (255, 200, 40), shade=True)
    c.draw(rrect(21, 46, 43, 55, 2), (90, 20, 20))
    c.text_c(32, 47, "SPR", WHITE)
    return c


# --- SDHLT vis hints (classes in the compiler's FGD, tools/sdhlt.fgd; the icons are in SBO's sprites/editor/) ---
# info_portal makes hlvis treat the leaf the info_leaf it targets is in as visible from the leaf it is in itself.
# Both icons draw a map split into leaves (convex cells); the yellow cell is the leaf that is made visible.

LEAF_CELL = (236, 238, 246)
LEAF_LIT = (255, 214, 60)


@icon("info_leaf", "compile", "Vis hint: the leaf an info_portal makes visible (SDHLT, FGD color 200 200 255)",
      where=REDRAWN)
def _info_leaf():
    c = Canvas((200, 200, 255))
    # a floor plan cut into four convex leaves; the one this entity stands in is lit, a pin marks the spot
    for pts, col in ((((14, 18), (34, 15), (30, 33), (12, 36)), LEAF_CELL),
                     (((34, 15), (50, 20), (52, 34), (30, 33)), LEAF_LIT),
                     (((12, 36), (30, 33), (28, 50), (16, 47)), LEAF_CELL),
                     (((30, 33), (52, 34), (46, 47), (28, 50)), LEAF_CELL)):
        c.draw(poly(*pts), col)
    c.draw(union(circle(41, 22, 4.5), poly((37, 24), (45, 24), (41, 31))), (230, 50, 50), shade=True)
    c.draw(circle(41, 22, 1.5), WHITE, outline=None)
    return c


@icon("info_portal", "compile", "Vis hint: makes an info_leaf's leaf visible from here (SDHLT, FGD color 255 200 "
      "200)", where=REDRAWN)
def _info_portal():
    c = Canvas((255, 200, 200))
    # two leaves side by side: from this one (the eye) the target leaf (lit as on the info_leaf icon) is seen
    c.draw(rrect(10, 19, 31, 45, 3), LEAF_CELL, shade=True)
    c.draw(rrect(34, 19, 54, 45, 3), LEAF_LIT, shade=True)
    c.draw(lambda x, y: circle(20, 35.9, 8.9)(x, y) and circle(20, 28.1, 8.9)(x, y), WHITE)
    c.draw(circle(20, 32, 3.5), (60, 120, 210), outline=None)
    c.draw(circle(20, 32, 1.5), (20, 20, 30), outline=None)
    c.draw(arrow(29, 32, 49, 32, w=3, head=9, headlen=6), (40, 40, 48), outline=None)
    return c


@icon("info_minlights", "compile", "Light hint: minimum light per texture (SDHLT hlrad, FGD color 210 100 0)",
      where=REDRAWN)
def _info_minlights():
    c = Canvas((210, 100, 0))
    # a texture (bricks) and a light gauge whose lower part is cut off at the red minimum line: hlrad lights that
    # texture no darker than the value given for it
    c.draw(rect(13, 18, 35, 40), (190, 80, 60), outline=BLACK)
    mortar = (230, 220, 200)
    for y in (23, 29, 35):
        c.draw(rect(13, y, 35, y + 1), mortar, outline=None)
    for y0, xs in ((18, (24,)), (24, (18, 30)), (30, (24,)), (36, (18, 30))):
        for x in xs:
            c.draw(rect(x, y0, x + 1, min(y0 + 5, 40)), mortar, outline=None)
    for k, col in enumerate(((250, 250, 245), (200, 200, 205), (140, 140, 150), (70, 70, 80))):
        c.draw(rect(41, 16 + k * 7, 51, 23 + k * 7), col, outline=BLACK)
    c.draw(rect(38, 30, 54, 33), (230, 30, 30), outline=BLACK)
    c.draw(rrect(15, 43, 35, 52, 2), (60, 30, 0))
    c.text_c(25, 44, "MIN", WHITE)
    return c


# --- feat/featureful-guide-entities: Featureful entity guide (docs/FEATUREFUL_GUIDE_ENTITIES.md) ----------------

GUIDE = "feat/featureful-guide-entities"


@icon("env_warpball", "effects", "Xen warpball (teleport flash, beams)", where=GUIDE)
def _env_warpball():
    c = Canvas((40, 90, 60))
    for x0, y0, x1, y1 in ((32, 32, 16, 17), (32, 32, 48, 18), (32, 32, 17, 46), (32, 32, 46, 46)):
        mx, my = (x0 + x1) / 2.0 + (4 if y1 < 32 else -4), (y0 + y1) / 2.0
        c.draw(union(seg(x0, y0, mx, my, 2.2), seg(mx, my, x1, y1, 2.2)), (150, 255, 120), outline=(20, 60, 20))
    c.draw(circle(32, 32, 11), (120, 230, 160), shade=True, hi=(220, 255, 230))
    c.draw(circle(32, 32, 5), WHITE, outline=None)
    return c


@icon("env_shockwave", "effects", "Shockwave ring", where=GUIDE)
def _env_shockwave():
    c = Canvas((60, 90, 140))
    c.draw(ellipse(32, 38, 24, 9), (188, 220, 255), outline=BLACK)
    c.draw(ellipse(32, 38, 19, 6), (60, 90, 140), outline=BLACK)
    c.draw(ellipse(32, 38, 13, 4.5), (230, 245, 255), outline=BLACK)
    c.draw(ellipse(32, 38, 8, 2.5), (60, 90, 140), outline=BLACK)
    c.draw(circle(32, 20, 5), (255, 230, 120), shade=True)
    return c


@icon("env_beamtrail", "effects", "Beam trail behind an entity", where=GUIDE)
def _env_beamtrail():
    c = Canvas((90, 70, 130))
    c.draw(poly((14, 42), (38, 24), (42, 31), (18, 48)), (230, 230, 240), outline=BLACK)
    c.draw(poly((17, 45), (39, 28), (40, 30), (18, 47)), (170, 170, 190), outline=None)
    entity_cube(c, 34, 16, 1.1)
    return c


@icon("env_quakefx", "effects", "Quake 1 particle effects", where=GUIDE)
def _env_quakefx():
    c = Canvas((120, 60, 40))
    cols = ((255, 200, 60), (255, 120, 30), (255, 240, 160))
    k = 0
    for r, n in ((7, 6), (13, 10), (19, 14)):
        for i in range(n):
            a = math.radians(i * 360.0 / n + r * 7)
            x, y = 32 + r * math.cos(a), 32 + r * math.sin(a)
            c.draw(rect(x - 1.5, y - 1.5, x + 1.5, y + 1.5), cols[k % 3], outline=None)
            k += 1
    c.draw(circle(32, 32, 3), WHITE, outline=None)
    return c


@icon("env_decal", "effects", "Decal sprayer", where=GUIDE)
def _env_decal():
    c = Canvas((150, 150, 160))
    c.draw(rect(38, 14, 50, 50), (200, 196, 186), outline=BLACK)
    splat = union(circle(44, 32, 5), circle(41, 26, 2.5), circle(47, 38, 2.5), circle(41, 38, 2), circle(47, 26, 2))
    c.draw(splat, (40, 40, 44), outline=None)
    c.draw(rrect(12, 26, 26, 50, 3), (220, 60, 50), shade=True)
    c.draw(rect(16, 21, 22, 26), (70, 74, 84))
    for x, y in ((29, 22), (33, 19), (31, 26), (36, 23)):
        c.draw(rect(x, y, x + 2, y + 2), (40, 40, 44), outline=None)
    return c


PLAYER_BG = (70, 130, 200)


def person(c, cx, top, s=1.0, fill=(250, 210, 150), body=(60, 90, 160)):
    """A small standing figure: head, body, legs."""
    c.draw(circle(cx, top + 4 * s, 4 * s), fill, shade=True)
    c.draw(rrect(cx - 6 * s, top + 9 * s, cx + 6 * s, top + 22 * s, 2 * s), body, shade=True)
    c.draw(rect(cx - 5 * s, top + 22 * s, cx - 1 * s, top + 32 * s), (50, 50, 60))
    c.draw(rect(cx + 1 * s, top + 22 * s, cx + 5 * s, top + 32 * s), (50, 50, 60))


@icon("player_capabilities", "rules", "Player capabilities (attack, jump, duck, use, steps, movement off / on)",
      where=GUIDE)
def _player_capabilities():
    c = Canvas(PLAYER_BG)
    person(c, 24, 14, 1.0)
    for i, on in enumerate((True, False, True)):
        toggle(c, 36, 16 + i * 12, on=on, w=16, h=9)
    return c


@icon("player_speed", "rules", "Player speed", where=GUIDE)
def _player_speed():
    c = Canvas(PLAYER_BG)
    person(c, 34, 14, 1.0)
    for y in (24, 32, 40):
        c.draw(rect(12, y, 24, y + 2), WHITE, outline=None)
    return c


@icon("player_hasweapon", "logic", "Player has weapon (pass / fail)", where=GUIDE)
def _player_hasweapon():
    c = logic(LOGIC, outputs=("up", "down"))
    c.draw(poly((21, 28), (40, 28), (40, 33), (30, 33), (28, 40), (23, 40), (24, 33), (21, 33)), (60, 64, 72))
    c.text_c(38, 35, "?", (40, 30, 0))
    return c


@icon("player_flicker", "rules", "Player flashlight flicker", where=GUIDE)
def _player_flicker():
    c = Canvas((40, 40, 60))
    c.draw(rrect(12, 28, 30, 38, 2), (180, 180, 190), shade=True)
    c.draw(poly((30, 26), (36, 22), (36, 44), (30, 40)), (200, 200, 210), shade=True)
    c.draw(poly((37, 24), (52, 16), (52, 48), (37, 42)), (255, 240, 120), outline=None)
    for y in (22, 30, 38):
        c.draw(rect(44, y, 48, y + 3), (40, 40, 60), outline=None)
    return c


@icon("game_player_settings", "rules", "Player settings (weapons, ammo, health, armor)", where=GUIDE)
def _game_player_settings():
    c = Canvas(PLAYER_BG)
    person(c, 22, 14, 1.0)
    c.draw(rrect(34, 16, 50, 30, 2), WHITE, shade=True)
    c.draw(rect(40, 18, 44, 28), (220, 40, 40), outline=None)
    c.draw(rect(37, 21, 47, 25), (220, 40, 40), outline=None)
    c.draw(rrect(34, 34, 50, 46, 3), (90, 170, 255), shade=True)
    return c


@icon("trigger_vehiclecontrols", "logic", "Lock / unlock vehicle controls", where=GUIDE)
def _trigger_vehiclecontrols():
    c = logic(LOGIC)
    c.draw(circle(32, 32, 9), (60, 64, 72))
    c.draw(circle(32, 32, 5.5), TILE, outline=None)
    c.draw(rect(31, 23, 33, 41), (60, 64, 72), outline=None)
    c.draw(rect(23, 31, 41, 33), (60, 64, 72), outline=None)
    return c


def speaker(c, x, y, fill=(70, 74, 84)):
    c.draw(rect(x, y + 4, x + 6, y + 14), fill, shade=True)
    c.draw(poly((x + 6, y + 4), (x + 14, y - 2), (x + 14, y + 20), (x + 6, y + 14)), fill, shade=True)


@icon("ambient_random", "sound", "Random sound (one of up to 5)", where=GUIDE)
def _ambient_random():
    c = Canvas((90, 180, 160))
    speaker(c, 12, 22)
    c.draw(rrect(32, 18, 52, 38, 3), WHITE, shade=True)
    for x, y in ((36, 22), (46, 22), (41, 27), (36, 32), (46, 32)):
        c.draw(rect(x, y, x + 3, y + 3), (30, 30, 30), outline=None)
    return c


@icon("ambient_extraspeaker", "sound", "Sound on another entity (follows it)", where=GUIDE)
def _ambient_extraspeaker():
    c = Canvas((90, 180, 160))
    entity_cube(c, 30, 26, 1.2)
    speaker(c, 12, 14)
    for r0 in (6, 10):
        c.draw(arc(26, 24, r0, r0 + 2, -40, 40), WHITE, outline=None)
    return c


@icon("env_soundmark", "sound", "AI sound mark (monsters only)", where=GUIDE)
def _env_soundmark():
    c = Canvas((200, 120, 60))
    c.draw(circle(32, 34, 4), (255, 230, 80))
    for r0 in (8, 13, 18):
        c.draw(arc(32, 34, r0, r0 + 2.5, 20, 160), (255, 230, 80), outline=None)
    c.text_c(32, 42, "AI", WHITE)
    return c


SCRIPT_BG = (255, 0, 255)       # the FGD color(255 0 255) of the scripted entities, toned down below
SCRIPT_DISK = (200, 70, 200)


@icon("monster_target", "effects", "Target for monsters (a fake enemy)", where=GUIDE)
def _monster_target():
    c = Canvas((60, 170, 170))                                   # the FGD color(0 200 200), darker
    for r0, col in ((18, (230, 60, 50)), (13, WHITE), (8, (230, 60, 50)), (3.5, WHITE)):
        c.draw(circle(32, 32, r0), col)
    return c


@icon("scripted_tanksequence", "logic", "Scripted tank sequence", where=GUIDE)
def _scripted_tanksequence():
    c = Canvas(SCRIPT_DISK)
    c.draw(rrect(14, 30, 40, 44, 3), (90, 100, 80), shade=True)
    c.draw(rect(38, 34, 52, 38), (70, 80, 60), shade=True)
    c.draw(rrect(18, 44, 36, 50, 2), (60, 60, 60))
    for x, y in ((48, 20), (52, 26), (44, 24)):
        c.draw(rect(x, y, x + 3, y + 3), (255, 220, 80), outline=None)
    c.text_c(26, 16, "SEQ", WHITE)
    return c


@icon("trigger_startpatrol", "logic", "Trigger start patrol", where=GUIDE)
def _trigger_startpatrol():
    c = logic(LOGIC)
    for (x0, y0, x1, y1) in ((23, 38, 32, 26), (32, 26, 41, 38)):
        c.draw(seg(x0, y0, x1, y1, 1.6), (60, 60, 60), outline=None)
    for x, y in ((23, 38), (32, 26), (41, 38)):
        c.draw(circle(x, y, 3), (247, 181, 82))
    return c


@icon("scripted_action", "logic", "Scripted action (attack / reload / jump on cue)", where=GUIDE)
def _scripted_action():
    c = Canvas(SCRIPT_DISK)
    person(c, 22, 16, 1.0)
    c.draw(rect(28, 26, 40, 29), (60, 60, 60))
    for x in (44, 49, 54):
        c.draw(rect(x, 26, x + 2, 29), (255, 220, 80), outline=None)
    return c


@icon("scripted_schedule", "logic", "Scripted schedule (AI goes to / from a spot)", where=GUIDE)
def _scripted_schedule():
    c = Canvas(SCRIPT_DISK)
    person(c, 20, 16, 1.0)
    c.draw(arrow(28, 32, 46, 32, w=5, head=11, headlen=6), ARROW, shade=True)
    c.draw(circle(50, 42, 3.5), (255, 220, 80))
    return c


@icon("scripted_following", "logic", "Scripted following (start / stop following)", where=GUIDE)
def _scripted_following():
    c = Canvas(SCRIPT_DISK)
    person(c, 38, 12, 1.0, body=(60, 120, 60))
    person(c, 22, 14, 0.9)
    c.draw(arrow(18, 50, 34, 50, w=4, head=9, headlen=5), ARROW, shade=True)
    return c


@icon("motion_manager", "vectors", "Motion manager (moves / turns an entity every frame)", where=GUIDE)
def _motion_manager():
    c = Canvas(VECTORS)
    entity_cube(c, 14, 22, 1.2)
    c.draw(arc(32, 32, 16, 19, 200, 340), WHITE, outline=BLACK)
    c.draw(loop_head(32, 32, 17.5, 340, 9, 7), WHITE)
    c.draw(arrow(36, 44, 52, 44, w=4, head=9, headlen=5), ARROW, shade=True)
    return c


@icon("trigger_motion", "vectors", "Trigger motion (sets position / movement once)", where=GUIDE)
def _trigger_motion():
    c = logic(VECTORS)
    entity_cube(c, 22, 24, 0.9)
    c.draw(arrow(36, 40, 44, 32, w=3, head=7, headlen=4), (60, 60, 60), outline=None)
    return c


# ---------------------------------------------------------------------------------------------------------------
# Editor models: for classes whose direction matters (a sprite always faces the camera)
# ---------------------------------------------------------------------------------------------------------------

def quad(tris, texname, p0, p1, p2, p3, n, uv=((0, 0), (1, 0), (1, 1), (0, 1))):
    """Two triangles; p0..p3 counter-clockwise seen from outside (bottom-left, bottom-right, top-right, top-left of
    the texture: SMD v = 1 is the top row of the picture)."""
    tris.append((texname, (p0, n, uv[0]), (p1, n, uv[1]), (p2, n, uv[2])))
    tris.append((texname, (p0, n, uv[0]), (p2, n, uv[2]), (p3, n, uv[3])))


# The six faces of a box: (outward normal, the texture's right, the texture's up), in the entity's frame (+X forward,
# +Y left, +Z up). Seen from outside, right x up = normal, so the corners run counter-clockwise. Side faces are
# upright; the top and bottom have the texture's up towards +X (forward).
BOX_FACES = {"front": ((1, 0, 0), (0, 1, 0), (0, 0, 1)), "back": ((-1, 0, 0), (0, -1, 0), (0, 0, 1)),
             "left": ((0, 1, 0), (-1, 0, 0), (0, 0, 1)), "right": ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
             "top": ((0, 0, 1), (0, -1, 0), (1, 0, 0)), "bottom": ((0, 0, -1), (0, 1, 0), (1, 0, 0))}


def box(tris, lo, hi, tex):
    """An axis-aligned box from corner lo to corner hi; tex: face name -> texture (missing faces are not drawn)."""
    c = [(lo[i] + hi[i]) / 2.0 for i in range(3)]
    h = [(hi[i] - lo[i]) / 2.0 for i in range(3)]

    def ext(v):
        return sum(abs(v[i]) * h[i] for i in range(3))
    for face, (n, r, u) in BOX_FACES.items():
        if face not in tex:
            continue
        dn, dr, du = ext(n), ext(r), ext(u)

        def pt(sr, su):
            return tuple(c[i] + n[i] * dn + r[i] * dr * sr + u[i] * du * su for i in range(3))
        quad(tris, tex[face], pt(-1, -1), pt(1, -1), pt(1, 1), pt(-1, 1), n)


def _model_vgui_screen():
    """A 32x32 screen (the default width / height) in the entity's frame: origin = the lower-left corner seen
    from the front, facing +X (the entity's angles), width along +Y (the viewer's right), height along +Z; a
    1-unit back plate and an arrow out of the front show which way it faces (docs/WORLD_SCREENS.md)."""
    tex = Canvas(None, disk=False)
    tex.px = [(60, 64, 72)] * (SIZE * SIZE)
    tex.inside = [True] * (SIZE * SIZE)
    screen_picture(tex, 3, 3, 61, 61)
    cursor(tex, 40, 30)
    front = tex.pixels()
    side = [(60, 64, 72)] * (SIZE * SIZE)
    arrowtex = [(255, 200, 40)] * (SIZE * SIZE)
    W = H = 32.0
    tris = []
    x0, x1 = -1.0, 0.0
    # front (+X): seen from +X, the viewer's right is +Y, so the width runs along +Y
    quad(tris, "front.bmp", (x1, 0, 0), (x1, W, 0), (x1, W, H), (x1, 0, H), (1, 0, 0))
    quad(tris, "side.bmp", (x0, W, 0), (x0, 0, 0), (x0, 0, H), (x0, W, H), (-1, 0, 0))
    quad(tris, "side.bmp", (x0, 0, 0), (x1, 0, 0), (x1, 0, H), (x0, 0, H), (0, -1, 0))
    quad(tris, "side.bmp", (x1, W, 0), (x0, W, 0), (x0, W, H), (x1, W, H), (0, 1, 0))
    quad(tris, "side.bmp", (x0, 0, H), (x1, 0, H), (x1, W, H), (x0, W, H), (0, 0, 1))
    quad(tris, "side.bmp", (x0, W, 0), (x1, W, 0), (x1, 0, 0), (x0, 0, 0), (0, 0, -1))
    # facing arrow: a flat 4-sided pyramid out of the middle of the front
    cy, cz, L, r = W / 2, H / 2, 10.0, 3.0
    tip = (L, cy, cz)
    base = [(0.0, cy - r, cz - r), (0.0, cy + r, cz - r), (0.0, cy + r, cz + r), (0.0, cy - r, cz + r)]
    for i in range(4):
        a, b = base[i], base[(i + 1) % 4]
        ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
        vx, vy, vz = tip[0] - a[0], tip[1] - a[1], tip[2] - a[2]
        n = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
        ln = math.sqrt(sum(k * k for k in n)) or 1.0
        n = tuple(k / ln for k in n)
        tris.append(("arrow.bmp", (a, n, (0, 1)), (b, n, (1, 1)), (tip, n, (0.5, 0))))
    return {"textures": {"front.bmp": front, "side.bmp": side, "arrow.bmp": arrowtex}, "tris": tris,
            "where": "feat/world-screens", "label": "World screen (VGUI2 panel): 32x32, faces +X"}


# --- label boxes: the pack's scripted_sequence.mdl layout, drawn by SBO -----------------------------------------
# A plain box exactly the size of the FGD class's box, the class name on its four sides, top and bottom plain, and a
# flat arrow on the floor in front of it (+X, the entity's yaw). Measured from scripted_sequence.mdl (read_mdl):
# box -16..16 x -16..16 x 0..72 = its FGD size(); side texture 64x144 and top/bottom 64x64 (2 texels per unit);
# 1-texel black rim, 1-texel light line (216 226 230) inside it, flat fill; the label in light capitals 6 texels
# tall with a 1-texel black outline (8 neighbors), lines 10 texels apart, centered; the arrow a 4-sided pyramid with
# its base 2 units in front of the box (x 18, 12 wide, 6 high) and its tip on the floor at x 32.

LABEL_LIGHT = (216, 226, 230)
LABEL_FONT = {  # 6 rows each; SBO's own blocky capitals in the pack's 4-wide, 6-tall look
    "A": ["####", "#..#", "####", "#..#", "#..#", "#..#"], "B": ["###.", "#..#", "###.", "#..#", "#..#", "###."],
    "C": ["####", "#...", "#...", "#...", "#...", "####"], "D": ["###.", "#..#", "#..#", "#..#", "#..#", "###."],
    "E": ["####", "#...", "###.", "#...", "#...", "####"], "G": ["####", "#...", "#...", "#.##", "#..#", "####"],
    "I": ["#", "#", "#", "#", "#", "#"], "L": ["#...", "#...", "#...", "#...", "#...", "####"],
    "M": ["#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#"], "N": ["#..#", "##.#", "#.##", "#..#", "#..#", "#..#"],
    "O": ["####", "#..#", "#..#", "#..#", "#..#", "####"], "R": ["####", "#..#", "###.", "#..#", "#..#", "#..#"],
    "S": ["####", "#...", "####", "...#", "...#", "####"], "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#.."],
    "Y": ["#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    "F": ["####", "#...", "###.", "#...", "#...", "#..."], "U": ["#..#", "#..#", "#..#", "#..#", "#..#", "####"],
    "V": ["#...#", "#...#", "#...#", ".#.#.", ".#.#.", "..#.."], "P": ["####", "#..#", "####", "#...", "#...", "#..."],
    "W": ["#...#", "#...#", "#...#", "#.#.#", "##.##", "#...#"],
}


def label_texture(w, h, fill, lines=()):
    """Flat face texture: black rim, light line, fill; `lines` centered as the pack's labels are."""
    px = [fill] * (w * h)
    for y in range(h):
        for x in range(w):
            if x in (0, w - 1) or y in (0, h - 1):
                px[y * w + x] = BLACK
            elif x in (1, w - 2) or y in (1, h - 2):
                px[y * w + x] = LABEL_LIGHT
    ink = set()
    top = (h - (len(lines) * 10 - 4)) // 2
    for i, line in enumerate(lines):
        glyphs = [LABEL_FONT[ch] for ch in line]
        width = sum(len(g[0]) for g in glyphs) + len(glyphs) - 1
        x = (w - width) // 2
        y0 = top + 10 * i
        for g in glyphs:
            for gy, row in enumerate(g):
                for gx, c in enumerate(row):
                    if c == "#":
                        ink.add((x + gx, y0 + gy))
            x += len(g[0]) + 1
    for (x, y) in ink:
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if (x + dx, y + dy) not in ink:
                    px[(y + dy) * w + x + dx] = BLACK
    for (x, y) in ink:
        px[y * w + x] = LABEL_LIGHT
    return (w, h, px)


def label_box(lines, fill, label, compare="scripted_sequence", lo=(-16, -16, 0), hi=(16, 16, 72)):
    """A plain box from lo to hi (the FGD size), `lines` on the four sides, plain top and bottom, and a flat arrow on
    the floor in front pointing +X (the yaw). 2 texels per unit; the side faces must be square in plan (x = y)."""
    sw, sh = int(2 * (hi[0] - lo[0])), int(2 * (hi[2] - lo[2]))
    tx = {"label.bmp": label_texture(sw, sh, fill, lines), "plain.bmp": label_texture(64, 64, fill)}
    tris = []
    box(tris, lo, hi, {"front": "label.bmp", "back": "label.bmp", "left": "label.bmp",
                       "right": "label.bmp", "top": "plain.bmp", "bottom": "plain.bmp"})
    # floor arrow: base square-ish triangle 2 units in front of the box (y -6..6, height 6), tip 14 units further
    x0, z0 = hi[0] + 2.0, float(lo[2])
    tip, l, r, up = (x0 + 14.0, 0.0, z0), (x0, 6.0, z0), (x0, -6.0, z0), (x0, 0.0, z0 + 6.0)
    mid = tuple((tip[k] + l[k] + r[k] + up[k]) / 4.0 for k in range(3))
    for t in ((l, tip, up), (up, tip, r), (r, tip, l), (l, up, r)):
        ax, ay, az = (t[1][k] - t[0][k] for k in range(3))
        bx, by, bz = (t[2][k] - t[0][k] for k in range(3))
        n = (ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx)
        if sum(n[k] * (t[0][k] - mid[k]) for k in range(3)) < 0:     # counter-clockwise from outside
            t, n = (t[0], t[2], t[1]), tuple(-k for k in n)
        ln = math.sqrt(sum(k * k for k in n)) or 1.0
        n = tuple(k / ln for k in n)
        tris.append(("plain.bmp", (t[0], n, (0.3, 0.3)), (t[1], n, (0.7, 0.3)), (t[2], n, (0.5, 0.7))))
    return {"textures": tx, "tris": tris, "where": "develop", "label": label, "compare": compare}


# Rule: every labeled-box model has its own fill color, clearly distinct from every other labeled box and from the
# pack's editor models (their fills, read with read_mdl), so that classes are told apart at a glance in the editor.
# check_label_colors() refuses a duplicate or a color closer than LABEL_COLOR_MIN_DISTANCE (RGB distance) to another.
PACK_MODEL_FILLS = {
    "scripted_sequence": (164, 128, 255), "aiscripted_sequence": (164, 0, 255), "genericmdl": (0, 0, 200),
    "info_bigmomma": (20, 190, 60), "info_node": (255, 227, 0), "info_node_air": (255, 227, 0),
    "info_teleport_destination": (255, 128, 240), "monstermaker": (150, 245, 95), "path_corner": (247, 181, 82),
    "path_track": (255, 148, 0), "trigger_camera": (221, 226, 230), "vgui_screen": (60, 64, 72),
}
LABEL_BOX_COLORS = {
    "monster_generic": (77, 216, 216),  # teal: the Monster / ModelBase FGD color(0 200 200), lightened
    "cycler": (232, 88, 88),            # red: no pack model uses red
    "env_model": (70, 110, 230),        # blue
    "env_static": (150, 140, 120),      # stone grey
    "cycler_weapon": (230, 60, 170),    # magenta
    "monster_furniture": (140, 90, 50),  # wood brown
}
LABEL_COLOR_MIN_DISTANCE = 80


def check_label_colors():
    """Problems with LABEL_BOX_COLORS and the spawn-zone colors (empty: fine)."""
    probs = []
    own = sorted(list(LABEL_BOX_COLORS.items()) + [(c, z[1]) for c, z in SPAWN_ZONES.items()])
    for i, (a, ca) in enumerate(own):
        others = [(b, cb) for b, cb in own[i + 1:]] + [(b + " (pack)", cb) for b, cb in sorted(PACK_MODEL_FILLS.items())
                                                       if b != a]
        for b, cb in others:
            d = math.sqrt(sum((x - y) ** 2 for x, y in zip(ca, cb)))
            if d < LABEL_COLOR_MIN_DISTANCE:
                probs.append("label box %s: color %s is %s %s's %s (distance %.0f, at least %d)"
                             % (a, ca, "the same as" if d == 0 else "too close to", b, cb, d,
                                LABEL_COLOR_MIN_DISTANCE))
    return probs


def _model_monster_generic():
    """monster_generic's placeholder (the FGD key default) until the mapper sets a model."""
    return label_box(("MONSTER", "GENERIC"), LABEL_BOX_COLORS["monster_generic"],
                     "monster_generic placeholder: 32x32x72, arrow = +X")


def _model_cycler():
    """cycler's placeholder (the FGD key default) until the mapper sets a model."""
    return label_box(("CYCLER",), LABEL_BOX_COLORS["cycler"], "cycler placeholder: 32x32x72, arrow = +X")


def _model_env_model():
    """env_model's placeholder (the FGD key default; the game treats it as no model)."""
    return label_box(("ENV", "MODEL"), LABEL_BOX_COLORS["env_model"], "env_model placeholder: 32x32x72, arrow = +X")


def _model_env_static():
    """env_static's placeholder (the FGD key default; the game treats it as no model)."""
    return label_box(("ENV", "STATIC"), LABEL_BOX_COLORS["env_static"],
                     "env_static placeholder: 32x32x72, arrow = +X")


def _model_cycler_weapon():
    """cycler_weapon's placeholder (the FGD key default; the game treats it as no model)."""
    return label_box(("CYCLER", "WEAPON"), LABEL_BOX_COLORS["cycler_weapon"],
                     "cycler_weapon placeholder: 32x32x72, arrow = +X")


def _model_monster_furniture():
    """monster_furniture's placeholder (the FGD key default; the game treats it as no model)."""
    return label_box(("MONSTER", "FURNITURE"), LABEL_BOX_COLORS["monster_furniture"],
                     "monster_furniture placeholder: 32x32x72, arrow = +X")


# --- spawn zones: the agent player model, tinted to the zone, on a numbered floor disc ----------------------------
# info_player_deathmatch is zone 1, info_player_go2..go5 zones 2-5 (the FGD shows these instead of agent.mdl).
# The agent's reference mesh, textures and the first frame of its first sequence (look_idle, what the editor showed
# for agent.mdl) come from sdk/modelsrc/players/agent/; a root bone "zone_disc" carries the disc, so the agent's
# bones and studiomdl's turn of the root bones are exactly agent.mdl's (same front, same scale, feet at z -36).

AGENT_SRC = os.path.join(REPO, "sdk", "modelsrc", "players", "agent")
SPAWN_ZONES = {  # class: (zone number, color); 80+ RGB from each other and from every editor box (check_label_colors)
    "info_player_deathmatch": (1, (240, 0, 0)),
    "info_player_go2": (2, (0, 120, 160)),
    "info_player_go3": (3, (160, 240, 0)),
    "info_player_go4": (4, (120, 0, 120)),
    "info_player_go5": (5, (0, 240, 160)),
}
SPAWN_TINT = 0.3            # how far the agent's palette moves toward the zone color (light: still the agent)
SPAWN_DISC = (28.0, -36.0)  # disc radius (56 units across, wider than the 32-unit hull) and height
SPAWN_BLOCK = (16.0, 10.0)  # the number block over the head: its size, and its gap above the head (top at z 37)
SPAWN_HEAD_TOP = 37.0


def smd_blocks(path):
    """(nodes lines, first skeleton frame lines, triangle lines) of an SMD."""
    lines = open(path, encoding="latin-1").read().splitlines()
    i = lines.index("nodes")
    j = lines.index("end", i)
    nodes = lines[i + 1:j]
    k = lines.index("skeleton", j)
    frame, m = [], k + 2                        # after "time 0"
    while m < len(lines) and not lines[m].startswith("time") and lines[m] != "end":
        frame.append(lines[m])
        m += 1
    tris = []
    if "triangles" in lines:
        t = lines.index("triangles")
        tris = [ln for ln in lines[t + 1:] if ln != "end"]
    return nodes, frame, tris


def tint_bmp(path, color, t):
    """An 8-bit BMP with its palette moved toward `color` by t (indices unchanged)."""
    b = bytearray(open(path, "rb").read())
    hsize = struct.unpack_from("<I", b, 14)[0]
    ncol = struct.unpack_from("<I", b, 46)[0] or 256
    o = 14 + hsize
    for i in range(ncol):
        bb, gg, rr = b[o + 4 * i:o + 4 * i + 3]
        r2, g2, b2 = mix((rr, gg, bb), color, t)
        b[o + 4 * i:o + 4 * i + 3] = bytes((b2, g2, r2))
    return bytes(b)


def disc_texture(color, size=128):
    """The disc's texture: the zone color in a circle with a 1-pixel dark rim (the number is on the block above the
    head only: on the disc as well, a top view read it twice)."""
    r = SPAWN_DISC[0]
    rim, out = (20, 20, 24), []
    for ty in range(size):
        for tx in range(size):
            d = math.hypot(((tx + 0.5) / size * 2 - 1) * r, ((ty + 0.5) / size * 2 - 1) * r)
            out.append(color if d <= r - 0.6 else rim if d <= r + 0.6 else (30, 30, 34))
    return out


def block_texture(num, color):
    """A face of the number block: the zone color, a dark rim, the number big (white, black outline)."""
    c = Canvas(color, disk=False)
    for r0 in (rect(0, 0, 64, 3), rect(0, 61, 64, 64), rect(0, 0, 3, 64), rect(61, 0, 64, 64)):
        c.draw(r0, (20, 20, 24), outline=None)
    c.text_c(32.5, 8, str(num), WHITE, outline=BLACK, scale=7)
    return c.pixels()


def number_block_tris(bone):
    """SMD triangle lines of the number block over the head: a cube, the number on its four sides (each upright from
    outside) and on top (upright in a top view). Built in the model's frame, written in the SMD's (see above)."""
    size, gap = SPAWN_BLOCK
    h = size / 2.0
    z0 = SPAWN_HEAD_TOP + gap
    c = (0.0, 0.0, z0 + h)
    faces = (((1, 0, 0), (0, 1, 0), (0, 0, 1)), ((-1, 0, 0), (0, -1, 0), (0, 0, 1)),
             ((0, 1, 0), (-1, 0, 0), (0, 0, 1)), ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
             ((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (1, 0, 0), (0, -1, 0)))
    tris = []
    for n, rt, up in faces:
        def pt(sr, su):
            return tuple(c[i] + n[i] * h + rt[i] * h * sr + up[i] * h * su for i in range(3))
        quad(tris, "block.bmp", pt(-1, -1), pt(1, -1), pt(1, 1), pt(-1, 1), tuple(float(k) for k in n))
    out = []

    def smd(v):
        return (v[1], -v[0], v[2])      # model -> SMD (the inverse of studiomdl's turn of the root bone)
    for t in tris:
        out.append(t[0])
        for p, n, uv in t[1:]:
            out.append("%d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f" % ((bone,) + smd(p) + smd(n) + tuple(uv)))
    return out


def spawn_zone_writer(cls):
    num, color = SPAWN_ZONES[cls]

    def write(name, folder):
        nodes, ref_frame, tris = smd_blocks(os.path.join(AGENT_SRC, "mesh", "agent.smd"))
        _, seq_frame, _ = smd_blocks(os.path.join(AGENT_SRC, "anims", "look_idle.smd"))
        disc = len(nodes)
        nodes = nodes + ['%3d "zone_disc" -1' % disc]
        bone = "%3d 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000" % disc
        r, z = SPAWN_DISC
        out = []
        for k in range(32):                     # a fan of 32 triangles, counter-clockwise seen from above
            a0, a1 = 2 * math.pi * k / 32, 2 * math.pi * (k + 1) / 32
            pts = [(0.0, 0.0), (r * math.cos(a0), r * math.sin(a0)), (r * math.cos(a1), r * math.sin(a1))]
            out.append("disc.bmp")
            for x, y in pts:
                out.append("%d %.6f %.6f %.6f 0.000000 0.000000 1.000000 %.6f %.6f"
                           % (disc, x, y, z, (x / r + 1) / 2, (y / r + 1) / 2))
        out += number_block_tris(disc)
        head = ["version 1", "nodes"] + nodes + ["end", "skeleton", "time 0"]
        with open(os.path.join(folder, name + "_ref.smd"), "w", newline="\n") as fh:
            fh.write("\n".join(head + ref_frame + [bone, "end", "triangles"] + tris + out + ["end"]) + "\n")
        with open(os.path.join(folder, name + "_seq.smd"), "w", newline="\n") as fh:
            fh.write("\n".join(head + seq_frame + [bone, "end"]) + "\n")
        for f in sorted(os.listdir(os.path.join(AGENT_SRC, "textures"))):
            if f.lower().endswith(".bmp"):
                with open(os.path.join(folder, f), "wb") as fh:
                    fh.write(tint_bmp(os.path.join(AGENT_SRC, "textures", f), color, SPAWN_TINT))
        with open(os.path.join(folder, "disc.bmp"), "wb") as fh:
            fh.write(bmp_bytes(disc_texture(color), 128, 128))
        with open(os.path.join(folder, "block.bmp"), "wb") as fh:
            fh.write(bmp_bytes(block_texture(num, color)))
        qc = ['$modelname "%s.mdl"' % name, '$cd "."', '$cdtexture "."', "$scale 1.0",
              '$bodygroup "body"', "{", 'studio "%s_ref"' % name, "}",
              '$sequence idle "%s_seq" fps 1 loop ACT_IDLE 1' % name]
        with open(os.path.join(folder, name + ".qc"), "w", newline="\n") as fh:
            fh.write("\n".join(qc) + "\n")
    return {"write": write, "where": "develop", "compare": None,
            "label": "%s: zone %d, agent tinted %s on a numbered disc" % (cls, num, color)}


MODELS = {"vgui_screen": _model_vgui_screen, "monster_generic": _model_monster_generic, "cycler": _model_cycler,
          "env_model": _model_env_model, "env_static": _model_env_static, "cycler_weapon": _model_cycler_weapon,
          "monster_furniture": _model_monster_furniture}
for _cls in SPAWN_ZONES:
    MODELS[_cls] = (lambda c: lambda: spawn_zone_writer(c))(_cls)
MODEL_SRC_DIR = os.path.join(SOURCE_DIR, "models")   # <name>/: the QC, SMD and BMP files `model` compiled, preview.png


def bmp_bytes(pixels, w=SIZE, h=SIZE):
    idx, pal = quantize(pixels)
    pal = pal[:255] + [(0, 0, 0)]
    rows = []
    for y in range(h - 1, -1, -1):
        rows.append(bytes(idx[y * w:(y + 1) * w]))
    data = b"".join(rows)
    palb = b"".join(bytes((b, g, r, 0)) for r, g, b in pal)
    off = 14 + 40 + len(palb)
    return (b"BM" + struct.pack("<IHHI", off + len(data), 0, 0, off)
            + struct.pack("<IiiHHIIiiII", 40, w, h, 1, 8, 0, len(data), 2835, 2835, 256, 256) + palb + data)


def model_sources(name, spec, folder):
    if "write" in spec:         # a model with its own sources (e.g. the spawn zones: an existing mesh plus parts)
        spec["write"](name, folder)
        return
    for tex, px in spec["textures"].items():
        w, h = SIZE, SIZE
        if isinstance(px, tuple):       # (width, height, pixels); a plain list is 64x64
            w, h, px = px
        with open(os.path.join(folder, tex), "wb") as fh:
            fh.write(bmp_bytes(px, w, h))
    lines = ["version 1", "nodes", '0 "origin" -1', "end", "skeleton", "time 0", "0 0 0 0 0 0 0", "end",
             "triangles"]
    for t in spec["tris"]:
        lines.append(t[0])
        for (p, n, uv) in t[1:]:
            lines.append("0 %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f" % (p + n + uv))
    lines.append("end")
    with open(os.path.join(folder, name + "_ref.smd"), "w", newline="\n") as fh:
        fh.write("\n".join(lines) + "\n")
    with open(os.path.join(folder, name + "_seq.smd"), "w", newline="\n") as fh:
        fh.write('version 1\nnodes\n0 "origin" -1\nend\nskeleton\ntime 0\n0 0 0 0 0 0 0\nend\n')
    # $origin's fourth value cancels the compiler's 90-degree turn of the root bone (studiomdl adds 90 to it, as
    # for SMDs authored facing -Y), so the SMD coordinates are the entity's frame: +X is where its yaw points.
    qc = ['$modelname "%s.mdl"' % name, '$cd "."', '$cdtexture "."', "$scale 1.0", "$origin 0 0 0 -90",
          '$bodygroup "body"', "{", 'studio "%s_ref"' % name, "}"]
    qc += ["$texrendermode %s fullbright" % t for t in sorted(spec["textures"])]
    qc += ['$sequence idle "%s_seq" fps 1 loop ACT_IDLE 1' % name]
    with open(os.path.join(folder, name + ".qc"), "w", newline="\n") as fh:
        fh.write("\n".join(qc) + "\n")


# ---------------------------------------------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------------------------------------------

def render(name):
    return ICONS[name]["fn"]().pixels()


def build_spr(name):
    idx, pal = quantize(render(name))
    return spr_bytes(idx, pal)


def file_of(name):
    """The file name of a class's sprite and source PNGs: the class name in lower case (every shipped name is lower
    case, scripts/check_lowercase.sh): Limitator -> limitator.spr, limitator.png"""
    return name.lower()


def class_of(name):
    """The class (ICONS key) a file stem or command-line name stands for, case aside; else the name itself"""
    for n in ICONS:
        if n.lower() == name.lower():
            return n
    return name


def source_names():
    if not os.path.isdir(SOURCE_DIR):
        return []
    return sorted(class_of(f[:-4]) for f in os.listdir(SOURCE_DIR) if f.endswith(".png") and not f.endswith("@4x.png"))


def write_if_changed(path, data):
    old = open(path, "rb").read() if os.path.isfile(path) else None
    if old != data:
        with open(path, "wb") as fh:
            fh.write(data)
    return old != data


def cmd_generate(args):
    """Every icon: from its spec (sprite + source PNGs), or from its source PNG (hand edited, or no spec)."""
    names = [class_of(n) for n in args.names] or sorted(set(ICONS) | set(source_names()))
    out = args.out or SPRITE_DIR
    os.makedirs(out, exist_ok=True)
    for n in names:
        src = os.path.join(SOURCE_DIR, file_of(n) + ".png")
        if n in ICONS and n not in HAND_EDITED:
            pixels = render(n)
            if not args.out:
                write_source(n, pixels)
            how = "spec"
        elif os.path.isfile(src):
            pixels = png_to_icon_pixels(src)
            how = "source PNG"
        else:
            sys.exit("%s: no spec and no sdk/spritesrc/editor/%s.png" % (n, file_of(n)))
        idx, pal = quantize(pixels)
        path = os.path.join(out, file_of(n) + ".spr")
        changed = write_if_changed(path, spr_bytes(idx, pal))
        print("%s %s (from its %s)" % ("wrote" if changed else "unchanged", os.path.relpath(path, REPO), how))
    return 0


def cmd_build(args):
    """Source PNG(s) -> sprite(s), whatever the specs say: a hand-edited PNG back into a .spr."""
    out = args.out or SPRITE_DIR
    for arg in args.pngs:
        path = arg if arg.endswith(".png") else os.path.join(SOURCE_DIR, file_of(arg) + ".png")
        name = class_of(os.path.basename(path)[:-4].replace("@4x", ""))
        pixels = png_to_icon_pixels(path)
        idx, pal = quantize(pixels)
        dst = os.path.join(out, file_of(name) + ".spr")
        changed = write_if_changed(dst, spr_bytes(idx, pal))
        src = os.path.join(SOURCE_DIR, file_of(name) + ".png")
        if os.path.abspath(path) == os.path.abspath(src):
            # the edited source stays as the author saved it; only its @4x view is redrawn
            rgba = [(0, 0, 0, 0) if c is None else c + (255,) for c in pixels]
            write_png(os.path.join(SOURCE_DIR, file_of(name) + "@4x.png"), SIZE, SIZE, rgba, 4)
        else:
            write_source(name, pixels)      # a PNG from elsewhere (or an @4x one) becomes the source
        print("%s %s from %s" % ("wrote" if changed else "unchanged", os.path.relpath(dst, REPO), path))
        if name in ICONS and name not in HAND_EDITED:
            print("  note: %s has a spec; add it to HAND_EDITED in scripts/gen_editor_sprites.py, or the next "
                  "`generate` redraws it from the spec" % name)
    return 0


def read_mdl(path):
    """Read a compiled one-bone editor model back: textures (name, w, h, RGB rows), and every triangle in the
    entity's frame as it is drawn (bone pose of the first frame applied), with texel coordinates."""
    d = open(path, "rb").read()
    if d[:4] != b"IDST" or struct.unpack_from("<i", d, 4)[0] != 10:
        raise ValueError("%s: not a version 10 studio model" % path)
    numbones, boneindex = struct.unpack_from("<ii", d, 140)
    seqindex = struct.unpack_from("<i", d, 168)[0]
    numtex, texindex = struct.unpack_from("<ii", d, 180)
    numskinref, numfam, skinindex, numbp, bpindex = struct.unpack_from("<5i", d, 192)
    if numbones != 1:
        raise ValueError("%s: %d bones (the editor models have one)" % (path, numbones))
    # bone 0 in the first frame of the first sequence: value + animation value * scale
    vals = list(struct.unpack_from("<6f", d, boneindex + 64))
    scales = struct.unpack_from("<6f", d, boneindex + 88)
    animindex = struct.unpack_from("<i", d, seqindex + 124)[0]
    offs = struct.unpack_from("<6H", d, animindex)
    for j in range(6):
        if offs[j]:
            vals[j] += struct.unpack_from("<h", d, animindex + offs[j] + 2)[0] * scales[j]
    rx, ry, rz = vals[3:]
    cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    m = ((cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz),
         (cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz),
         (-sy, sx * cy, cx * cy))

    def xf(v):
        return tuple(sum(m[i][k] * v[k] for k in range(3)) + vals[i] for i in range(3))
    textures = []
    for i in range(numtex):
        o = texindex + 80 * i
        name = d[o:o + 64].split(b"\0")[0].decode("latin-1")
        flags, w, h, idx = struct.unpack_from("<4i", d, o + 64)
        pal = d[idx + w * h: idx + w * h + 768]
        rgb = [tuple(pal[3 * b:3 * b + 3]) for b in d[idx: idx + w * h]]
        textures.append({"name": name, "flags": flags, "w": w, "h": h, "rgb": rgb})
    skins = struct.unpack_from("<%dh" % numskinref, d, skinindex)
    tris = []
    for bp in range(numbp):
        nummodels, base, modelindex = struct.unpack_from("<3i", d, bpindex + 76 * bp + 64)
        for mi in range(nummodels):
            mo = modelindex + 112 * mi
            nummesh, meshindex, numverts, vinfo, vindex = struct.unpack_from("<5i", d, mo + 72)
            verts = [xf(struct.unpack_from("<3f", d, vindex + 12 * k)) for k in range(numverts)]
            for me in range(nummesh):
                numtris, triindex, skinref = struct.unpack_from("<3i", d, meshindex + 20 * me)
                tex = skins[skinref]
                o = triindex
                while True:
                    n = struct.unpack_from("<h", d, o)[0]
                    o += 2
                    if n == 0:
                        break
                    fan, n = n < 0, abs(n)
                    pts = []
                    for _ in range(n):
                        vi, ni, s_, t_ = struct.unpack_from("<4h", d, o)
                        o += 8
                        pts.append((verts[vi], s_, t_))
                    for k in range(n - 2):
                        if fan:
                            tri = (pts[0], pts[k + 1], pts[k + 2])
                        elif k % 2 == 0:
                            tri = (pts[k], pts[k + 1], pts[k + 2])
                        else:
                            tri = (pts[k + 1], pts[k], pts[k + 2])
                        tris.append((tex, tri))
    return {"textures": textures, "tris": tris, "bone": vals}


def render_mdl(mdl, view, size=200, extent=(48.0, 48.0, 36.0)):
    """Orthographic picture of read_mdl()'s triangles, z-buffered, back faces culled (as the editor draws them),
    textures nearest, fullbright. view: (forward, right, up) unit vectors of the camera; extent: half size of the
    window around (0, 0, center_z) in units, center_z."""
    fwd, right, up = view
    half, _, cz = extent
    scale = size / (2.0 * half)
    bg = (44, 44, 52)
    px = [bg] * (size * size)
    zb = [float("inf")] * (size * size)

    def proj(p):
        q = (p[0], p[1], p[2] - cz)
        return (size / 2.0 + sum(q[i] * right[i] for i in range(3)) * scale,
                size / 2.0 - sum(q[i] * up[i] for i in range(3)) * scale,
                sum(q[i] * fwd[i] for i in range(3)))
    for ti, tri in mdl["tris"]:
        tex = mdl["textures"][ti]
        (a, sa, ta), (b, sb, tb), (c, sc, tc) = tri
        A, B, C = proj(a), proj(b), proj(c)
        area = (B[0] - A[0]) * (C[1] - A[1]) - (B[1] - A[1]) * (C[0] - A[0])
        if area <= 0:       # studiomdl stores the SMD's counter-clockwise faces clockwise; the engine culls the rest
            continue
        x0, x1 = max(0, int(min(A[0], B[0], C[0]))), min(size - 1, int(max(A[0], B[0], C[0])) + 1)
        y0, y1 = max(0, int(min(A[1], B[1], C[1]))), min(size - 1, int(max(A[1], B[1], C[1])) + 1)
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                X, Y = x + 0.5, y + 0.5
                w0 = ((B[0] - X) * (C[1] - Y) - (B[1] - Y) * (C[0] - X)) / area
                w1 = ((C[0] - X) * (A[1] - Y) - (C[1] - Y) * (A[0] - X)) / area
                w2 = 1.0 - w0 - w1
                if w0 < -1e-6 or w1 < -1e-6 or w2 < -1e-6:
                    continue
                z = w0 * A[2] + w1 * B[2] + w2 * C[2]
                i = y * size + x
                if z >= zb[i]:
                    continue
                zb[i] = z
                s_ = min(tex["w"] - 1, max(0, int(w0 * sa + w1 * sb + w2 * sc)))
                t_ = min(tex["h"] - 1, max(0, int(w0 * ta + w1 * tb + w2 * tc)))
                px[i] = tex["rgb"][t_ * tex["w"] + s_]
    return px


def preview_png(rows, png_path):
    """rows: [(mdl_path, label), ...]; one row of four views per model, all at one scale: 3/4 from the front left,
    front (+X), left side, top. A reference model (spec "compare") goes in the first row to compare the style."""
    mdls = [(read_mdl(path), label) for path, label in rows]
    pts = [p for mdl, _ in mdls for _, tri in mdl["tris"] for p, _, _ in tri]
    zs = [p[2] for p in pts]
    half = max(max(abs(p[i]) for p in pts for i in (0, 1)), (max(zs) - min(zs)) / 2.0) + 6.0
    cz = (max(zs) + min(zs)) / 2.0

    def norm(v):
        ln = math.sqrt(sum(k * k for k in v))
        return tuple(k / ln for k in v)
    iso_f = norm((-1.0, -1.0, -0.8))
    iso_r = norm((iso_f[1], -iso_f[0], 0.0))
    iso_u = (iso_r[1] * iso_f[2] - iso_r[2] * iso_f[1], iso_r[2] * iso_f[0] - iso_r[0] * iso_f[2],
             iso_r[0] * iso_f[1] - iso_r[1] * iso_f[0])
    views = [("3/4 FRONT LEFT", (iso_f, iso_r, iso_u)),
             ("FRONT (+X)", ((-1, 0, 0), (0, 1, 0), (0, 0, 1))),
             ("LEFT (+Y)", ((0, -1, 0), (-1, 0, 0), (0, 0, 1))),
             ("TOP (+X UP)", ((0, 0, -1), (0, -1, 0), (1, 0, 0)))]
    size, pad, cap = 200, 8, 14
    rowh = size + pad + cap
    W, H = len(views) * (size + pad) + pad, pad + cap + len(mdls) * rowh
    img = [(30, 30, 36)] * (W * H)

    def text(x0, y0, t, color):
        m = text_mask(x0, y0, t.upper()[:W // 6 - 4])
        for y in range(y0, y0 + 7):
            for x in range(x0, W - pad):
                if m(x, y):
                    img[y * W + x] = color
    for k, (name, _) in enumerate(views):
        text(pad + k * (size + pad), pad, name, (235, 235, 240))
    for r, (mdl, label) in enumerate(mdls):
        oy = pad + cap + r * rowh
        for k, (_, v) in enumerate(views):
            pic = render_mdl(mdl, v, size, (half, half, cz))
            ox = pad + k * (size + pad)
            for y in range(size):
                img[(oy + y) * W + ox: (oy + y) * W + ox + size] = pic[y * size:(y + 1) * size]
        text(pad, oy + size + 4, label, (200, 200, 210))
    write_png(png_path, W, H, [c + (255,) for c in img], 2)


def cmd_model(args):
    probs = check_label_colors()
    if probs:
        sys.exit("\n".join(probs))
    names = args.names or sorted(MODELS)
    if not os.access(STUDIOMDL, os.X_OK):
        sys.exit("studiomdl not found: %s (set STUDIOMDL)" % STUDIOMDL)
    out = args.out or MODEL_DIR
    os.makedirs(out, exist_ok=True)
    for n in names:
        spec = MODELS[n]()
        work = tempfile.mkdtemp(prefix="editor-mdl-")
        try:
            model_sources(n, spec, work)
            res = subprocess.run([STUDIOMDL, n + ".qc"], cwd=work, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, timeout=120)
            built = os.path.join(work, "models", n + ".mdl")
            if not os.path.isfile(built):
                built = os.path.join(work, n + ".mdl")
            if res.returncode != 0 or not os.path.isfile(built):
                sys.stdout.write(res.stdout.decode("utf-8", "replace"))
                sys.exit("%s: studiomdl failed (exit %d)" % (n, res.returncode))
            src = args.keep_sources and os.path.join(args.keep_sources, n)
            if not args.out and not src:
                src = os.path.join(MODEL_SRC_DIR, n)
            if src:
                os.makedirs(src, exist_ok=True)
                made = set()
                for f in sorted(os.listdir(work)):
                    if os.path.isfile(os.path.join(work, f)) and not f.endswith(".mdl"):
                        write_if_changed(os.path.join(src, f), open(os.path.join(work, f), "rb").read())
                        made.add(f)
                for f in os.listdir(src):       # files an earlier version of the spec compiled
                    if f not in made and f != "preview.png" and os.path.isfile(os.path.join(src, f)):
                        os.remove(os.path.join(src, f))
                rows = [(built, spec.get("label", n))]
                ref = spec.get("compare") and os.path.join(MODEL_DIR, spec["compare"] + ".mdl")
                if ref and os.path.isfile(ref):
                    rows.insert(0, (ref, "reference: models/editor/%s.mdl (the pack)" % spec["compare"]))
                if "write" in spec:     # read_mdl reads one-bone models only: no preview for these
                    pass
                else:
                    preview_png(rows, os.path.join(src, "preview.png"))
            dst = os.path.join(out, n + ".mdl")
            changed = write_if_changed(dst, open(built, "rb").read())
            print("%s %s%s" % ("wrote" if changed else "unchanged", os.path.relpath(dst, REPO),
                               " (sources and preview.png in %s)" % os.path.relpath(src, REPO) if src else ""))
        finally:
            shutil.rmtree(work, ignore_errors=True)
    return 0


def check_spr(path):
    """List of problems with one editor sprite (empty: fine)."""
    try:
        s = read_spr(path)
    except (OSError, SprError) as err:
        return [str(err)]
    probs = []
    if (s["width"], s["height"]) != (SIZE, SIZE):
        probs.append("size %dx%d (house: 64x64)" % (s["width"], s["height"]))
    if len(s["frames"]) != 1:
        probs.append("%d frames (house: 1)" % len(s["frames"]))
    if s["format"] != 3:
        probs.append("format %s (house: alphatest)" % SPR_FORMATS[s["format"]])
    if s["type"] != 2:
        probs.append("type %s (house: parallel)" % SPR_TYPES[s["type"]])
    fx, fy, fw, fh, _ = s["frames"][0]
    if (fx, fy) != (-fw // 2, fh // 2):
        probs.append("frame origin %d,%d (centered: %d,%d)" % (fx, fy, -fw // 2, fh // 2))
    return probs


PADDING = 3   # px between an icon's art and the inside of its black rim (kept where possible;
              # a class may break it on purpose: PADDING_EXEMPT)
PADDING_EXEMPT = set()   # classes allowed to break it on purpose (add with a reason in a comment)

def padding_problems():
    """Disk icons whose art (anything not the disk's fill) comes closer than PADDING to the rim."""
    out = []
    for n in sorted(ICONS):
        c = ICONS[n]["fn"]()
        if not getattr(c, "disk", False) or n in PADDING_EXEMPT:
            continue
        mid = c.size / 2.0
        worst = None
        for i, inside in enumerate(c.inside):
            if not inside or c.px[i] is None or c.px[i] == c.bg:
                continue
            d = (c.radius - 1) - math.hypot(i % c.size + 0.5 - mid, i // c.size + 0.5 - mid)
            worst = d if worst is None else min(worst, d)
        if worst is not None and worst < PADDING:
            out.append("%s: art %.1f px from the rim (padding rule: %d px; fix it, or add to PADDING_EXEMPT with a reason)"
                       % (n, max(worst, 0.0), PADDING))
    return out


def cmd_verify(args):
    bad = 0
    files = sorted(f for f in os.listdir(SPRITE_DIR) if f.lower().endswith(".spr"))
    for f in files:
        for p in check_spr(os.path.join(SPRITE_DIR, f)):
            print("%s: %s" % (f, p))
            bad += 1
    sources = source_names()
    for n in sorted(set(ICONS) | set(sources)):
        path = os.path.join(SPRITE_DIR, file_of(n) + ".spr")
        src = os.path.join(SOURCE_DIR, file_of(n) + ".png")
        if not os.path.isfile(path):
            print("%s.spr: missing (run generate %s)" % (n, n))
            bad += 1
            continue
        data = open(path, "rb").read()
        if n in ICONS and n not in HAND_EDITED and data != build_spr(n):
            print("%s.spr: differs from what its spec draws (run generate %s, or the file was edited)" % (n, n))
            bad += 1
        if n in ICONS and not os.path.isfile(src):
            print("%s: no source PNG in sdk/spritesrc/editor/ (run generate %s)" % (n, n))
            bad += 1
        if os.path.isfile(src):
            idx, pal = quantize(png_to_icon_pixels(src))
            if spr_bytes(idx, pal) != data:
                print("%s.spr: differs from sdk/spritesrc/editor/%s.png (run build %s, or generate)" % (n, n, n))
                bad += 1
            if not os.path.isfile(os.path.join(SOURCE_DIR, file_of(n) + "@4x.png")):
                print("%s: no %s@4x.png next to the source (run build %s)" % (n, n, n))
                bad += 1
    for p in check_label_colors():
        print(p)
        bad += 1
    pad = padding_problems()
    for p in pad:
        print("warning: " + p)
    for n in sorted(HAND_EDITED):
        if not os.path.isfile(os.path.join(SOURCE_DIR, file_of(n) + ".png")):
            print("%s: in HAND_EDITED but has no source PNG" % n)
            bad += 1
    print("verify: %d sprites read, %d specs, %d source PNGs, %d hand-edited, %s"
          % (len(files), len(ICONS), len(sources), len(HAND_EDITED), "OK" if not bad else "%d problem(s)" % bad))
    if pad:
        print("verify: %d padding warning(s) (rule, not a failure)" % len(pad))
    return 1 if bad else 0


def cmd_status(args):
    sys.path.insert(0, HERE)
    import fgd
    path = args.fgd or fgd.FGD_PATH
    classes, _ = fgd.parse_fgd(path)
    missing = []
    for name, cls in classes.items():
        if cls["kind"] != "pointclass":
            continue
        helpers = [h for h, _ in (cls["helpers"] if args.own else fgd.effective_helpers(classes, name))]
        if any(h in fgd.ASSET_HELPERS + ("decal",) for h in helpers):
            continue
        missing.append(name)
    bad = 0
    for n in missing:
        if n in MODELS:
            gen = os.path.isfile(os.path.join(MODEL_DIR, n + ".mdl"))
            print('%-28s model spec: yes  model: %-3s  -> studio("models/editor/%s.mdl")'
                  % (n, "yes" if gen else "no", n))
        elif "model" in fgd.effective_props(classes, n):
            print("%-28s has a model key  -> studio() (the editor shows the mapper's model)" % n)
        elif n not in ICONS and os.path.isfile(os.path.join(SPRITE_DIR, file_of(n) + ".spr")):
            print('%-28s sprite exists (no spec)  -> iconsprite("sprites/editor/%s.spr")' % (n, file_of(n)))
        elif n in ICONS:
            gen = os.path.isfile(os.path.join(SPRITE_DIR, file_of(n) + ".spr"))
            print('%-28s spec: yes  sprite: %-3s  -> iconsprite("sprites/editor/%s.spr")'
                  % (n, "yes" if gen else "no", file_of(n)))
        else:
            print("%-28s spec: NO   (add an @icon spec, or a studio()/sprite() helper if its model is a key)" % n)
            bad += 1
    print("status: %d point class(es) without an editor helper in %s, %d without a spec"
          % (len(missing), os.path.relpath(path, REPO) if path.startswith(REPO) else path, bad))
    return 1 if bad else 0


def cmd_decode(args):
    s = read_spr(args.spr)
    w, h, px = spr_to_rgba(s)
    write_png(args.png, w, h, px, args.scale)
    return 0


# --- review sheet ---------------------------------------------------------------------------------------------

def rgba_of(name):
    return [(0, 0, 0, 0) if c is None else c + (255,) for c in render(name)]


def previous_rgba(name):
    """The sprite as committed at HEAD, when it differs from the current file (the pack copy a redraw replaces)."""
    rel = os.path.relpath(os.path.join(SPRITE_DIR, file_of(name) + ".spr"), REPO).replace(os.sep, "/")
    try:
        res = subprocess.run(["git", "-c", "submodule.recurse=false", "cat-file", "--filters", "HEAD:" + rel],
                             cwd=REPO,
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        return None
    cur = os.path.join(SPRITE_DIR, file_of(name) + ".spr")
    if res.returncode != 0 or (os.path.isfile(cur) and open(cur, "rb").read() == res.stdout):
        return None
    tmp = tempfile.NamedTemporaryFile(suffix=".spr", delete=False)
    try:
        tmp.write(res.stdout)
        tmp.close()
        return spr_to_rgba(read_spr(tmp.name))[2]
    finally:
        os.remove(tmp.name)


def existing_rgba(name):
    s = read_spr(os.path.join(SPRITE_DIR, file_of(name) + ".spr"))
    return spr_to_rgba(s)[2]


class Sheet:
    def __init__(self, w, h, bg=(58, 58, 64)):
        self.w, self.h = w, h
        self.px = [bg + (255,)] * (w * h)

    def blit(self, x, y, pixels, scale=1, size=SIZE):
        for py in range(size * scale):
            for px in range(size * scale):
                c = pixels[(py // scale) * size + px // scale]
                if c[3]:
                    self.px[(y + py) * self.w + x + px] = c

    def text(self, x, y, s, color=(235, 235, 235)):
        m = text_mask(0, 0, s.upper(), 1)
        for py in range(7):
            for px in range(text_width(s)):
                if m(px + 0.5, py + 0.5) and 0 <= x + px < self.w:
                    self.px[(y + py) * self.w + x + px] = color + (255,)

    def save(self, path):
        rows = [b"".join(bytes(c) for c in self.px[y * self.w:(y + 1) * self.w]) for y in range(self.h)]
        with open(path, "wb") as fh:
            fh.write(png_bytes(self.w, self.h, rows))


def cmd_sheet(args):
    out = args.out or SHEET_DIR
    pngdir = os.path.join(out, "png")
    os.makedirs(pngdir, exist_ok=True)
    order = list(FAMILY_TITLES)
    fams = {f: sorted((n for n in ICONS if ICONS[n]["family"] == f),
                      key=lambda n: (ICONS[n]["where"] != "develop", n.lower())) for f in order}
    previous = {}
    for n in ICONS:
        write_png(os.path.join(pngdir, n + ".png"), SIZE, SIZE, rgba_of(n))
        write_png(os.path.join(pngdir, n + "@4x.png"), SIZE, SIZE, rgba_of(n), 4)
        if ICONS[n]["where"] in (REPLACED, REDRAWN):
            prev = previous_rgba(n)
            if prev:
                previous[n] = prev
                write_png(os.path.join(pngdir, "previous_" + n + ".png"), SIZE, SIZE, prev)
    refs = sorted(set(r for f in order for r in FAMILY_REFS[f]))
    for r in refs:
        write_png(os.path.join(pngdir, "existing_" + r + ".png"), SIZE, SIZE, existing_rgba(r))
        write_png(os.path.join(pngdir, "existing_" + r + "@4x.png"), SIZE, SIZE, existing_rgba(r), 4)

    # contact.png: per family one band; new icons at 4x with their 1x below, existing icons of the family at 2x
    cell, per_row = 280, 6
    ref_w = 4 * 140 + 20
    width = 20 + per_row * cell + ref_w
    bands = []
    y = 20
    for f in order:
        names = fams[f]
        if not names:
            continue
        rows = (len(names) + per_row - 1) // per_row
        h = 30 + rows * 340
        bands.append((f, names, y, h))
        y += h + 20
    sh = Sheet(width, y)
    for f, names, y0, h in bands:
        sh.text(20, y0, FAMILY_TITLES[f] + "   (right: existing icons of the family, 2x)", (255, 220, 120))
        for i, n in enumerate(names):
            x = 20 + (i % per_row) * cell
            yy = y0 + 22 + (i // per_row) * 340
            sh.blit(x, yy, rgba_of(n), 4)
            sh.blit(x, yy + 262, rgba_of(n), 1)
            sh.text(x + 72, yy + 270, n, (255, 255, 255))
            if n in previous:
                sh.blit(x + 210, yy + 262, previous[n], 1)
                sh.text(x + 180, yy + 290, "WAS:", (255, 150, 150))
            where = ICONS[n]["where"]
            sh.text(x + 72, yy + 282, "develop, redrawn" if where in (REPLACED, REDRAWN) else where.split(" ")[0],
                    (160, 200, 255) if where.startswith("develop") else (200, 170, 255))
        rx = 20 + per_row * cell
        for j, r in enumerate(FAMILY_REFS[f]):
            sh.blit(rx + j * 140, y0 + 22, existing_rgba(r), 2)
            sh.text(rx + j * 140, y0 + 156, r[:21], (190, 190, 190))
    sh.save(os.path.join(out, "contact.png"))

    # index.html
    html = ["<!doctype html><html lang=en><head><meta charset=utf-8>",
            "<meta name=viewport content='width=device-width,initial-scale=1'>",
            "<title>SBO editor icons</title><style>",
            ":root{--bg:#f4f4f6;--fg:#1d1d22;--card:#fff;--mut:#666;--line:#ddd}",
            "@media (prefers-color-scheme:dark){:root{--bg:#1c1c20;--fg:#eee;--card:#26262c;--mut:#aaa;"
            "--line:#3a3a42}}",
            "body{background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,sans-serif;margin:0;padding:16px}",
            "h1{font-size:22px}h2{font-size:18px;margin-top:32px;border-bottom:1px solid var(--line)}",
            "img{image-rendering:pixelated}.row{display:flex;flex-wrap:wrap;gap:12px}",
            ".card{background:var(--card);border:1px solid var(--line);border-radius:8px;padding:10px;width:290px}",
            ".card code{font-weight:600}.mut{color:var(--mut);font-size:13px}",
            ".refs{display:flex;gap:10px;flex-wrap:wrap;margin:8px 0}.refs figure{margin:0;text-align:center;"
            "font-size:12px;color:var(--mut)}",
            ".bg{background:repeating-conic-gradient(#999 0 25%,#bbb 0 50%) 0 0/16px 16px;display:inline-block}",
            "</style></head><body>",
            "<h1>SBO editor icons: review</h1>",
            "<p>Generated by <code>scripts/gen_editor_sprites.py</code>. Each new icon at 4x and 1x (the editor "
            "draws it at about 1x), the existing icons of the same family beside each group. The full sheet: "
            "<a href='contact.png'>contact.png</a>.</p>",
            "<h2>Style decisions (approve or adjust)</h2><ul>",
            "<li>Format as the 57 existing sprites: 64x64, one frame, alpha test, parallel, palette index 255 "
            "see-through.</li>",
            "<li>A filled disk with a 1-pixel black rim; one flat pictogram on it, 1-pixel black outlines, light "
            "top-left / dark bottom-right edges on raised parts. No anti-aliasing.</li>",
            "<li>Disk color = family color; where the FGD class has <code>color()</code> it is that color (logic "
            "purple 180 120 255, numbers green 170 221 85, vectors olive 170 179 43, positions 128 200 64, world "
            "screens green). Develop's effect and weather classes use their FGD box color where the pictogram "
            "still reads on it (rain, sky, particles, 1.38 smoke).</li>",
            "<li>Logic entities share one motif: input arrow, a light tile with the glyph, output arrow(s); the "
            "existing trigger_relay / multi_manager icons read the same way. Two colored outputs (green / red) = "
            "condition true / false; three = random or compare.</li>",
            "<li>Text only where a word beats a picture: MAX, ON, A+B, F(X), {X}, XYZ, &lt;=&gt;.</li>",
            "<li>All drawn from scratch by the script (no pixels from Descen's pack or Sven Co-op). The five interim "
            "pack copies (game_timer, trigger_sequence, Limitator, multispawn, game_messagebox) are redrawn; their "
            "cards show the previous file as 'was'.</li>",
            "<li>Sources: sdk/spritesrc/editor/&lt;class&gt;.png (64x64, the source) and &lt;class&gt;@4x.png (to "
            "look at); a hand-edited PNG goes back into the sprite with <code>gen_editor_sprites.py build</code>.</li>",
            "</ul>"]
    for f in order:
        names = fams[f]
        if not names:
            continue
        html.append("<h2>%s</h2><div class=refs>" % FAMILY_TITLES[f])
        for r in FAMILY_REFS[f]:
            html.append("<figure><span class=bg><img src='png/existing_%s.png' width=64 height=64 alt=''></span>"
                        "<figcaption>%s (existing)</figcaption></figure>" % (r, r))
        html.append("</div><div class=row>")
        for n in names:
            info = ICONS[n]
            html.append("<div class=card><span class=bg><img src='png/%s@4x.png' width=256 height=256 alt='%s'>"
                        "</span> <span class=bg><img src='png/%s.png' width=64 height=64 alt=''></span>%s"
                        "<div><code>%s</code></div><div class=mut>%s<br>%s</div></div>"
                        % (n, n, n, (" <span class=mut>was:</span> <span class=bg><img src='png/previous_%s.png' "
                                     "width=64 height=64 alt=''></span>" % n) if n in previous else "",
                           n, info["label"], info["where"]))
        html.append("</div>")
    html.append("<h2>Editor models</h2><p>")
    for n, fn in sorted(MODELS.items()):
        html.append("<code>%s</code>: %s (%s). " % (n, fn()["label"], fn()["where"]))
    html.append("</p></body></html>")
    with open(os.path.join(out, "index.html"), "w", newline="\n") as fh:
        fh.write("\n".join(html) + "\n")
    print("wrote %s, %s and %s/" % (os.path.join(out, "contact.png"), os.path.join(out, "index.html"), pngdir))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("generate")
    p.add_argument("names", nargs="*")
    p.add_argument("--out")
    p = sub.add_parser("build")
    p.add_argument("pngs", nargs="+", help="class names (sdk/spritesrc/editor/<name>.png) or PNG paths")
    p.add_argument("--out")
    p = sub.add_parser("model")
    p.add_argument("names", nargs="*")
    p.add_argument("--out")
    p.add_argument("--keep-sources", help="put the generated QC/SMD/BMP and preview.png into this folder "
                   "(default: sdk/spritesrc/editor/models/<name>/; none with --out)")
    sub.add_parser("verify")
    p = sub.add_parser("status")
    p.add_argument("--fgd", help="FGD to check (default sdk/syndicate.fgd), e.g. a branch's via git show")
    p.add_argument("--own", action="store_true", help="count only helpers on the class itself, not inherited")
    p = sub.add_parser("sheet")
    p.add_argument("--out")
    p = sub.add_parser("decode")
    p.add_argument("spr")
    p.add_argument("png")
    p.add_argument("--scale", type=int, default=1)
    args = ap.parse_args()
    return {"generate": cmd_generate, "build": cmd_build, "model": cmd_model, "verify": cmd_verify,
            "status": cmd_status,
            "sheet": cmd_sheet, "decode": cmd_decode}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
