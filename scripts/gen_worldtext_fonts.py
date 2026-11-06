#!/usr/bin/env python3
"""point_worldtext fonts: sprites/worldtext/<font>.spr and <font>.txt (glyph atlas and metrics) from a TrueType font.

    scripts/gen_worldtext_fonts.py generate [NAME ...]   rasterize the fonts (FONTS below) from their TTF files: writes
                                                         the sources sdk/spritesrc/worldtext/<font>.png,
                                                         <font>_outline.png, <font>.txt, then builds the sprites
    scripts/gen_worldtext_fonts.py build [NAME ...]      sources -> gamedir/syndicate/sprites/worldtext/<font>.spr and
                                                         .txt (after editing a source PNG by hand)
    scripts/gen_worldtext_fonts.py verify                read the shipped sprites and metrics back, check them against
                                                         the sources and the limits below (exit 1 on a problem)
    scripts/gen_worldtext_fonts.py sheet OUT.png [NAME]  a preview: every glyph of the font on a dark background

Python 3 standard library only; PNG files through scripts/gen_editor_sprites.py (a small TrueType reader and rasterizer is below: no PIL, no FreeType). Same TTF, same
bytes: no timestamps, no randomness. The client is cl_dll/worldtext.cpp; mapper guide and licenses: docs/POINT_WORLDTEXT.md.

The atlas (limits of Valve's engine, also on HL25):
  * Frames of 256x256 (FRAME). The engine refuses a texture of more than 524288 pixels and, on a card without
    non-power-of-two textures, scales every texture down to gl_max_size (256 by default on older drivers): 256x256
    loads everywhere unchanged. A font has as many frames as its glyphs need.
  * SPR_INDEXALPHA: palette index = alpha (0 see-through .. 255 solid), the color is palette entry 255 (white); the
    client tints it with the text's color (the triangle API's color) and draws with kRenderTransAlpha.
  * Two masks per glyph at the same place in two frame sets: the glyph's coverage (frames 0 .. N-1) and the same glyph
    widened by OUTLINE_PX (frames N .. 2N-1): drawn in black under the glyph it is the outline; in the text's color,
    faint, it is the glow. Each glyph has PAD pixels around it that are empty in both masks (no bleeding when the
    engine filters).
  * Glyph metrics in atlas pixels at an em of EM pixels: x y w h (the glyph's box in its frame), left (pen to the box's
    left edge), top (baseline to the box's top edge), advance. A line is EM pixels high (as in Source: "textsize" is
    the line height in world units).

The metrics file (text, one item per line, // comments):
    worldtext 1                       format version
    name sans
    em 64                             pixels per em (= line height)
    ascent 59  descent 14             pixels above / below the baseline (the font's)
    spacingunit 64                    textspacingX/Y are in 1/spacingunit of textsize (Source's font sizes)
    frame 256 256                     frame size
    frames 9                          frames per mask (the sprite has twice as many)
    glyph <codepoint> <frame> <x> <y> <w> <h> <left> <top> <advance>
"""

import argparse
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
# PNG reading / writing and write_if_changed: the editor icon generator's (one implementation in scripts/)
import gen_editor_sprites as ges  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
SOURCE_DIR = os.path.join(REPO, "sdk", "spritesrc", "worldtext")
OUT_DIR = os.path.join(REPO, "gamedir", "syndicate", "sprites", "worldtext")

FRAME = 256         # frame size (see above)
EM = 64             # pixels per em
OUTLINE_PX = 4.0    # the outline mask: the glyph widened by this many pixels
PAD = 7             # empty pixels around each glyph (outline + 2 for filtering)
SUBSAMPLES = 8      # sub-scanlines per pixel row (anti-aliasing)
MAX_FRAMES = 32     # per mask; the client checks the same

FONT_DIR = "/usr/share/fonts/truetype"

# The characters of every font: printable ASCII, Latin-1, and a few signage extras when the font has them
CHARSET = list(range(0x20, 0x7F)) + list(range(0xA0, 0x100)) + [
    0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x20AC, 0x2122,    # dashes, quotes, bullet, ...
    0x2190, 0x2191, 0x2192, 0x2193, 0x2194, 0x2195,                                    # arrows
    0x25B2, 0x25B6, 0x25BC, 0x25C0, 0x25A0, 0x25CF, 0x2713, 0x2717,                    # triangles, square, dot, check
]

# name -> font files (the first that has a character draws it), license, Source font size for the spacing unit
FONTS = {
    "sans": {
        "files": ["liberation/LiberationSans-Bold.ttf", "dejavu/DejaVuSans-Bold.ttf"],
        "title": "Liberation Sans Bold 2.1.5 (fallback DejaVu Sans Bold 2.37 for arrows and symbols)",
        "license": "SIL Open Font License 1.1 (Liberation); Bitstream Vera / DejaVu license (DejaVu)",
        "spacingunit": 64,      # Source's Liberation Sans (font 4) is a 64 pixel font
    },
    "mono": {
        "files": ["dejavu/DejaVuSansMono-Bold.ttf"],
        "title": "DejaVu Sans Mono Bold 2.37",
        "license": "Bitstream Vera / DejaVu license (free to use, modify and redistribute)",
        "spacingunit": 128,     # Source's Roboto Mono (font 8) is a 128 pixel font
    },
}


# ---------------------------------------------------------------------------------------------------------------
# TrueType reader (the tables a rasterizer needs: head, hhea, hmtx, maxp, cmap, loca, glyf)
# ---------------------------------------------------------------------------------------------------------------

class TTF:
    def __init__(self, path):
        with open(path, "rb") as fh:
            self.data = data = fh.read()
        self.path = path
        num = struct.unpack_from(">H", data, 4)[0]
        self.tables = {}
        for i in range(num):
            tag, _, off, length = struct.unpack_from(">4sIII", data, 12 + 16 * i)
            self.tables[tag.decode("latin-1")] = (off, length)
        head = self.tables["head"][0]
        self.units = struct.unpack_from(">H", data, head + 18)[0]
        self.loca_long = struct.unpack_from(">h", data, head + 50)[0] == 1
        hhea = self.tables["hhea"][0]
        self.ascender, self.descender = struct.unpack_from(">hh", data, hhea + 4)
        self.num_hmetrics = struct.unpack_from(">H", data, hhea + 34)[0]
        self.num_glyphs = struct.unpack_from(">H", data, self.tables["maxp"][0] + 4)[0]
        self.cmap = self._read_cmap()

    def _read_cmap(self):
        d = self.data
        base = self.tables["cmap"][0]
        n = struct.unpack_from(">H", d, base + 2)[0]
        best = None
        for i in range(n):
            pid, eid, off = struct.unpack_from(">HHI", d, base + 4 + 8 * i)
            fmt = struct.unpack_from(">H", d, base + off)[0]
            rank = {(3, 10, 12): 0, (0, 4, 12): 0, (3, 1, 4): 1, (0, 3, 4): 1}.get((pid, eid, fmt))
            if rank is not None and (best is None or rank < best[0]):
                best = (rank, base + off, fmt)
        if best is None:
            raise SystemExit("%s: no Unicode cmap" % self.path)
        _, off, fmt = best
        out = {}
        if fmt == 4:
            segx2 = struct.unpack_from(">H", d, off + 6)[0]
            ends = off + 14
            starts = ends + segx2 + 2
            deltas = starts + segx2
            ranges = deltas + segx2
            for s in range(segx2 // 2):
                end = struct.unpack_from(">H", d, ends + 2 * s)[0]
                start = struct.unpack_from(">H", d, starts + 2 * s)[0]
                delta = struct.unpack_from(">h", d, deltas + 2 * s)[0]
                ro = struct.unpack_from(">H", d, ranges + 2 * s)[0]
                for c in range(start, end + 1):
                    if c == 0xFFFF:
                        continue
                    if ro == 0:
                        g = (c + delta) & 0xFFFF
                    else:
                        g = struct.unpack_from(">H", d, ranges + 2 * s + ro + 2 * (c - start))[0]
                        if g:
                            g = (g + delta) & 0xFFFF
                    if g:
                        out[c] = g
        else:
            ngroups = struct.unpack_from(">I", d, off + 12)[0]
            for i in range(ngroups):
                a, b, g = struct.unpack_from(">III", d, off + 16 + 12 * i)
                for c in range(a, b + 1):
                    out[c] = g + (c - a)
        return out

    def advance(self, g):
        hmtx = self.tables["hmtx"][0]
        i = min(g, self.num_hmetrics - 1)
        return struct.unpack_from(">H", self.data, hmtx + 4 * i)[0]

    def _glyph_offset(self, g):
        loca = self.tables["loca"][0]
        if self.loca_long:
            a, b = struct.unpack_from(">II", self.data, loca + 4 * g)
        else:
            a, b = (2 * v for v in struct.unpack_from(">HH", self.data, loca + 2 * g))
        return (self.tables["glyf"][0] + a, b - a)

    def contours(self, g, depth=0):
        """The glyph's outline: a list of contours, each a list of (x, y, on_curve) in font units."""
        off, length = self._glyph_offset(g)
        if length == 0:
            return []
        d = self.data
        ncont = struct.unpack_from(">h", d, off)[0]
        p = off + 10
        if ncont >= 0:
            ends = struct.unpack_from(">%dH" % ncont, d, p)
            p += 2 * ncont
            ilen = struct.unpack_from(">H", d, p)[0]
            p += 2 + ilen
            npts = ends[-1] + 1 if ncont else 0
            flags = []
            while len(flags) < npts:
                f = d[p]
                p += 1
                flags.append(f)
                if f & 8:
                    r = d[p]
                    p += 1
                    flags.extend([f] * r)
            flags = flags[:npts]
            xs, ys = [], []
            v = 0
            for f in flags:
                if f & 2:
                    dv = d[p]
                    p += 1
                    v += dv if f & 16 else -dv
                elif not f & 16:
                    v += struct.unpack_from(">h", d, p)[0]
                    p += 2
                xs.append(v)
            v = 0
            for f in flags:
                if f & 4:
                    dv = d[p]
                    p += 1
                    v += dv if f & 32 else -dv
                elif not f & 32:
                    v += struct.unpack_from(">h", d, p)[0]
                    p += 2
                ys.append(v)
            out, start = [], 0
            for e in ends:
                out.append([(xs[i], ys[i], bool(flags[i] & 1)) for i in range(start, e + 1)])
                start = e + 1
            return out
        # composite
        if depth > 8:
            return []
        out = []
        while True:
            fl, gi = struct.unpack_from(">HH", d, p)
            p += 4
            if fl & 1:
                a1, a2 = struct.unpack_from(">hh", d, p)
                p += 4
            else:
                a1, a2 = struct.unpack_from(">bb", d, p)
                p += 2
            m = (1.0, 0.0, 0.0, 1.0)
            if fl & 8:
                s = struct.unpack_from(">h", d, p)[0] / 16384.0
                p += 2
                m = (s, 0.0, 0.0, s)
            elif fl & 0x40:
                sx, sy = struct.unpack_from(">hh", d, p)
                p += 4
                m = (sx / 16384.0, 0.0, 0.0, sy / 16384.0)
            elif fl & 0x80:
                m = tuple(v / 16384.0 for v in struct.unpack_from(">hhhh", d, p))
                p += 8
            dx, dy = (a1, a2) if fl & 2 else (0, 0)     # point matching (not XY values): not used by these fonts
            for c in self.contours(gi, depth + 1):
                out.append([(m[0] * x + m[2] * y + dx, m[1] * x + m[3] * y + dy, on) for x, y, on in c])
            if not fl & 0x20:
                break
        return out


def flatten(contour, scale, steps=10):
    """A TrueType contour (quadratic, implied on-curve points) as a closed polyline in pixels (y up)."""
    pts = [(x * scale, y * scale, on) for x, y, on in contour]
    n = len(pts)
    if n < 2:
        return []
    # start on an on-curve point (or the midpoint of the first two off-curve points)
    k = next((i for i in range(n) if pts[i][2]), None)
    if k is None:
        a, b = pts[0], pts[1]
        pts = [((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, True)] + pts[1:] + pts[:1]
        k = 0
    pts = pts[k:] + pts[:k]
    out = [(pts[0][0], pts[0][1])]
    cur = pts[0]
    ctrl = None
    for q in pts[1:] + pts[:1]:
        if q[2]:
            if ctrl is None:
                out.append((q[0], q[1]))
            else:
                quad(out, cur, ctrl, q, steps)
                ctrl = None
            cur = q
        else:
            if ctrl is not None:
                mid = ((ctrl[0] + q[0]) / 2, (ctrl[1] + q[1]) / 2, True)
                quad(out, cur, ctrl, mid, steps)
                cur = mid
            ctrl = q
    return out


def quad(out, a, c, b, steps):
    for i in range(1, steps + 1):
        t = i / steps
        u = 1 - t
        out.append((u * u * a[0] + 2 * u * t * c[0] + t * t * b[0], u * u * a[1] + 2 * u * t * c[1] + t * t * b[1]))


def rasterize(polys, x0, ytop, w, h):
    """Coverage 0..1 of the polygons (pixels, y up) in the box w x h whose top-left corner is (x0, ytop): rows from the
    top. Nonzero winding; SUBSAMPLES sub-scanlines per row, exact horizontal coverage of each span."""
    edges = []
    for poly in polys:
        for i in range(len(poly)):
            ax, ay = poly[i]
            bx, by = poly[(i + 1) % len(poly)]
            if ay == by:
                continue
            # in box coordinates, y down
            edges.append((ax - x0, ytop - ay, bx - x0, ytop - by))
    cov = [[0.0] * w for _ in range(h)]
    inv = 1.0 / SUBSAMPLES
    for row in range(h):
        acc = cov[row]
        for s in range(SUBSAMPLES):
            y = row + (s + 0.5) * inv
            xs = []
            for ax, ay, bx, by in edges:
                if (ay <= y < by) or (by <= y < ay):
                    t = (y - ay) / (by - ay)
                    xs.append((ax + t * (bx - ax), 1 if by > ay else -1))
            if not xs:
                continue
            xs.sort()
            wind = 0
            for i in range(len(xs) - 1):
                wind += xs[i][1]
                if wind == 0:
                    continue
                a, b = max(0.0, xs[i][0]), min(float(w), xs[i + 1][0])
                if b <= a:
                    continue
                ia, ib = int(a), int(b)
                if ia == ib:
                    acc[ia] += (b - a) * inv
                    continue
                acc[ia] += (ia + 1 - a) * inv
                for px in range(ia + 1, min(ib, w)):
                    acc[px] += inv
                if ib < w:
                    acc[ib] += (b - ib) * inv
    return [[min(1.0, v) for v in r] for r in cov]


def widen(cov, radius):
    """The coverage widened by radius pixels (a disk, soft edge): max over the disk of coverage times the falloff."""
    h, w = len(cov), len(cov[0])
    r = int(math.ceil(radius + 0.5))
    kernel = []
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            f = min(1.0, max(0.0, radius + 0.5 - math.hypot(dx, dy)))
            if f > 0:
                kernel.append((dy, dx, f))
    src = [(y, x, cov[y][x]) for y in range(h) for x in range(w) if cov[y][x] > 0.004]
    out = [[0.0] * w for _ in range(h)]
    for y, x, c in src:
        for dy, dx, f in kernel:
            yy, xx = y + dy, x + dx
            if 0 <= yy < h and 0 <= xx < w:
                v = c * f
                if v > out[yy][xx]:
                    out[yy][xx] = v
    return out


# ---------------------------------------------------------------------------------------------------------------
# Font -> glyph bitmaps -> atlas
# ---------------------------------------------------------------------------------------------------------------

def find_font(rel):
    path = os.path.join(FONT_DIR, rel)
    if not os.path.isfile(path):
        raise SystemExit("font file %s not found (Debian/Ubuntu: fonts-liberation, fonts-dejavu)" % path)
    return path


def render_font(name):
    spec = FONTS[name]
    fonts = [TTF(find_font(f)) for f in spec["files"]]
    main = fonts[0]
    scale = EM / main.units
    glyphs = []
    for cp in CHARSET:
        ttf = next((f for f in fonts if cp in f.cmap), None)
        if ttf is None:
            continue
        g = ttf.cmap[cp]
        s = EM / ttf.units
        polys = [p for p in (flatten(c, s) for c in ttf.contours(g)) if len(p) >= 3]
        adv = ttf.advance(g) * s
        if polys:
            minx = min(x for p in polys for x, _ in p)
            maxx = max(x for p in polys for x, _ in p)
            miny = min(y for p in polys for _, y in p)
            maxy = max(y for p in polys for _, y in p)
            left = int(math.floor(minx)) - PAD
            top = int(math.ceil(maxy)) + PAD
            w = int(math.ceil(maxx)) + PAD - left
            h = top - (int(math.floor(miny)) - PAD)
            cov = rasterize(polys, left, top, w, h)
            out = widen(cov, OUTLINE_PX)
        else:
            left, top, w, h, cov, out = 0, 0, 0, 0, [], []
        if w > FRAME or h > FRAME:
            raise SystemExit("%s U+%04X: glyph of %dx%d does not fit a frame" % (name, cp, w, h))
        glyphs.append({"cp": cp, "left": left, "top": top, "w": w, "h": h, "adv": adv, "cov": cov, "out": out})
    return {"name": name, "glyphs": glyphs, "ascent": main.ascender * scale, "descent": -main.descender * scale}


def pack(glyphs):
    """Shelf packing, tallest first: sets each glyph's frame, x, y. Returns the number of frames."""
    order = sorted((g for g in glyphs if g["w"]), key=lambda g: (-g["h"], -g["w"], g["cp"]))
    frame, x, y, shelf = 0, 0, 0, 0
    for g in order:
        if x + g["w"] > FRAME:
            x, y, shelf = 0, y + shelf, 0
        if y + g["h"] > FRAME:
            frame, x, y, shelf = frame + 1, 0, 0, 0
        g["frame"], g["x"], g["y"] = frame, x, y
        x += g["w"]
        shelf = max(shelf, g["h"])
    for g in glyphs:
        if not g["w"]:
            g["frame"], g["x"], g["y"] = 0, 0, 0
    frames = frame + 1
    if frames > MAX_FRAMES:
        raise SystemExit("%d frames per mask: more than MAX_FRAMES %d" % (frames, MAX_FRAMES))
    return frames


def atlas(glyphs, frames, key):
    """frames x FRAME x FRAME bytes (alpha 0..255) of one mask."""
    pix = [bytearray(FRAME * FRAME) for _ in range(frames)]
    for g in glyphs:
        m = g[key]
        buf = pix[g["frame"]]
        for r in range(g["h"]):
            base = (g["y"] + r) * FRAME + g["x"]
            for c in range(g["w"]):
                buf[base + c] = int(round(max(0.0, min(1.0, m[r][c])) * 255))
    return pix


def metrics_text(name, font, frames):
    spec = FONTS[name]
    lines = [
        "// SBO point_worldtext font \"%s\" (SBO docs: point_worldtext)" % name,
        "// %s" % spec["title"],
        "// License: %s" % spec["license"],
        "worldtext 1",
        "name %s" % name,
        "em %d" % EM,
        "ascent %d" % int(round(font["ascent"])),
        "descent %d" % int(round(font["descent"])),
        "spacingunit %d" % spec["spacingunit"],
        "frame %d %d" % (FRAME, FRAME),
        "frames %d" % frames,
        "// glyph <codepoint> <frame> <x> <y> <w> <h> <left> <top> <advance>",
    ]
    for g in sorted(font["glyphs"], key=lambda g: g["cp"]):
        lines.append("glyph %d %d %d %d %d %d %d %d %.2f" % (g["cp"], g["frame"], g["x"], g["y"], g["w"], g["h"],
                                                             g["left"], g["top"], g["adv"]))
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------------------------------------------
# PNG and SPR files
# ---------------------------------------------------------------------------------------------------------------

def write_png_gray(path, w, h, rows):
    """A mask as an RGBA PNG: white, alpha = the mask (gen_editor_sprites' PNG writer)."""
    rgba = [b"".join(b"\xff\xff\xff" + bytes((v,)) for v in r) for r in rows]
    write_if_changed(path, ges.png_bytes(w, h, rgba))


def read_png_gray(path):
    """A mask from a PNG (gen_editor_sprites' PNG reader): min(alpha, gray), so a white PNG with alpha and an opaque
    gray one both work. -> (w, h, rows of bytes)"""
    w, h, px = ges.read_png(path)
    rows = []
    for y in range(h):
        line = px[y * w:(y + 1) * w]
        rows.append(bytes(min(c[3], (c[0] * 299 + c[1] * 587 + c[2] * 114) // 1000) for c in line))
    return w, h, rows


def spr_bytes(frames_pixels):
    """Sprite version 2, VP_PARALLEL, SPR_INDEXALPHA, one single frame per atlas frame. (gen_editor_sprites.spr_bytes
    writes the editor icons' one 64x64 alphatest frame; the atlas needs many 256x256 indexalpha frames.)"""
    n = len(frames_pixels)
    radius = math.sqrt(2 * (FRAME / 2) ** 2)
    out = struct.pack("<4siiifiiifi", b"IDSP", 2, 2, 2, radius, FRAME, FRAME, n, 0.0, 0)
    # indexalpha: index = alpha; entry 255 is the color (white); the ramp is for sprite viewers
    out += struct.pack("<h", 256) + b"".join(bytes((i, i, i)) for i in range(256))
    for px in frames_pixels:
        out += struct.pack("<iiiii", 0, -FRAME // 2, FRAME // 2, FRAME, FRAME) + bytes(px)
    return out


def write_if_changed(path, data):
    """gen_editor_sprites.write_if_changed, with the folder made and text written as UTF-8."""
    if isinstance(data, str):
        data = data.encode("utf-8")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    return ges.write_if_changed(path, data)


def sheet_rows(frames_pixels):
    """frames stacked top to bottom: FRAME wide, FRAME * n high"""
    return [bytes(px[r * FRAME:(r + 1) * FRAME]) for px in frames_pixels for r in range(FRAME)]


def rows_to_frames(rows, n):
    return [bytearray(b"".join(rows[f * FRAME:(f + 1) * FRAME])) for f in range(n)]


def parse_metrics(text):
    """The metrics file as the client reads it (cl_dll/worldtext_font.cpp does the same checks)."""
    info, glyphs = {}, []
    for ln in text.splitlines():
        ln = ln.split("//")[0].split()
        if not ln:
            continue
        if ln[0] == "glyph":
            cp, fr, x, y, w, h, left, top = (int(v) for v in ln[1:9])
            glyphs.append((cp, fr, x, y, w, h, left, top, float(ln[9])))
        else:
            info[ln[0]] = ln[1:]
    return info, glyphs


# ---------------------------------------------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------------------------------------------

def cmd_generate(args):
    for name in args.names or sorted(FONTS):
        if name not in FONTS:
            raise SystemExit("unknown font %s (FONTS: %s)" % (name, ", ".join(sorted(FONTS))))
        font = render_font(name)
        frames = pack(font["glyphs"])
        fill, out = atlas(font["glyphs"], frames, "cov"), atlas(font["glyphs"], frames, "out")
        write_png_gray(os.path.join(SOURCE_DIR, name + ".png"), FRAME, FRAME * frames, sheet_rows(fill))
        write_png_gray(os.path.join(SOURCE_DIR, name + "_outline.png"), FRAME, FRAME * frames, sheet_rows(out))
        write_if_changed(os.path.join(SOURCE_DIR, name + ".txt"), metrics_text(name, font, frames))
        print("%s: %d glyphs, %d frames per mask" % (name, len(font["glyphs"]), frames))
        build(name)
    return 0


def build(name):
    with open(os.path.join(SOURCE_DIR, name + ".txt"), encoding="utf-8") as fh:
        text = fh.read()
    info, _ = parse_metrics(text)
    n = int(info["frames"][0])
    masks = []
    for suffix in ("", "_outline"):
        w, h, rows = read_png_gray(os.path.join(SOURCE_DIR, name + suffix + ".png"))
        if w != FRAME or h != FRAME * n:
            raise SystemExit("%s%s.png: %dx%d, the metrics say %d frames of %d" % (name, suffix, w, h, n, FRAME))
        masks += rows_to_frames(rows, n)
    for path, data in ((os.path.join(OUT_DIR, name + ".spr"), spr_bytes(masks)),
                       (os.path.join(OUT_DIR, name + ".txt"), text)):
        print("%s %s" % ("wrote" if write_if_changed(path, data) else "unchanged", os.path.relpath(path, REPO)))


def cmd_build(args):
    for name in args.names or sorted(FONTS):
        build(name)
    return 0


def cmd_verify(args):
    bad = 0
    for name in sorted(FONTS):
        spr = os.path.join(OUT_DIR, name + ".spr")
        txt = os.path.join(OUT_DIR, name + ".txt")
        try:
            with open(spr, "rb") as fh:
                d = fh.read()
            with open(txt, encoding="utf-8") as fh:
                text = fh.read()
        except OSError as e:
            print("FAIL %s: %s" % (name, e))
            bad += 1
            continue
        ident, ver, typ, fmt, _, mw, mh, nframes, _, _ = struct.unpack_from("<4siiifiiifi", d, 0)
        info, glyphs = parse_metrics(text)
        n = int(info["frames"][0])
        problems = []
        if ident != b"IDSP" or ver != 2 or fmt != 2:
            problems.append("not an indexalpha sprite version 2")
        if (mw, mh) != (FRAME, FRAME) or nframes != 2 * n:
            problems.append("%d frames of %dx%d, expected %d of %d" % (nframes, mw, mh, 2 * n, FRAME))
        if mw * mh > 524288:
            problems.append("frame larger than the engine's texture limit")
        if len(d) != 40 + 2 + 768 + nframes * (20 + FRAME * FRAME):
            problems.append("file size %d" % len(d))
        for cp, fr, x, y, w, h, left, top, adv in glyphs:
            if not (0 <= fr < n and 0 <= x and 0 <= y and x + w <= FRAME and y + h <= FRAME and w >= 0 and h >= 0):
                problems.append("glyph %d outside its frame" % cp)
        if not any(g[0] == ord("?") for g in glyphs):
            problems.append("no '?' (the client's stand-in for a missing character)")
        with open(os.path.join(SOURCE_DIR, name + ".txt"), encoding="utf-8") as fh:
            if fh.read() != text:
                problems.append("metrics differ from sdk/spritesrc/worldtext/%s.txt (run build)" % name)
        print("%s %s: %d glyphs, %d frames per mask%s" % ("FAIL" if problems else "ok", name, len(glyphs), n,
                                                          "".join("\n  " + p for p in problems)))
        bad += bool(problems)
    return 1 if bad else 0


def cmd_sheet(args):
    name = args.name or "sans"
    with open(os.path.join(SOURCE_DIR, name + ".txt"), encoding="utf-8") as fh:
        info, glyphs = parse_metrics(fh.read())
    n = int(info["frames"][0])
    _, _, fill = read_png_gray(os.path.join(SOURCE_DIR, name + ".png"))
    _, _, outl = read_png_gray(os.path.join(SOURCE_DIR, name + "_outline.png"))
    cols, cell = 16, EM + 2 * PAD + 8
    rows_n = (len(glyphs) + cols - 1) // cols
    W, H = cols * cell, rows_n * cell
    img = [bytearray([40]) * W for _ in range(H)]
    for i, (cp, fr, x, y, w, h, left, top, adv) in enumerate(glyphs):
        ox, oy = (i % cols) * cell + 4, (i // cols) * cell + 4
        for r in range(h):
            for c in range(w):
                a = fill[fr * FRAME + y + r][x + c] / 255.0
                o = outl[fr * FRAME + y + r][x + c] / 255.0
                bg = 40 * (1 - o)
                img[oy + r][ox + c] = int(bg * (1 - a) + 255 * a)
    write_png_gray(args.out, W, H, img)
    print("wrote %s (%d glyphs)" % (args.out, len(glyphs)))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("generate")
    p.add_argument("names", nargs="*")
    p.set_defaults(fn=cmd_generate)
    p = sub.add_parser("build")
    p.add_argument("names", nargs="*")
    p.set_defaults(fn=cmd_build)
    p = sub.add_parser("verify")
    p.set_defaults(fn=cmd_verify)
    p = sub.add_parser("sheet")
    p.add_argument("out")
    p.add_argument("name", nargs="?")
    p.set_defaults(fn=cmd_sheet)
    a = ap.parse_args()
    sys.exit(a.fn(a))


if __name__ == "__main__":
    main()
