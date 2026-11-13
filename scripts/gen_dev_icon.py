#!/usr/bin/env python3
"""Developer scoreboard icon: the robot's face from the developer's Steam avatar.

The scoreboard shows this icon on the developer's row (src/cl_dll/vgui2/score_panel.cpp, docs/dev/SCOREBOARD_PORT.md).
They are client-local TGAs loaded by the VGUI2 scoreboard (scorepanel.res "dev_icon16".."dev_icon64"), not
precached resources.

Steps (all deterministic: same source, same bytes):
  1. cut the head out of sdk/spritesrc/devicon/avatar.png: in a box around the head, every light pixel reachable
     from the box's edge without crossing the black outline is background; the head is the largest remaining
     region (outline included), with its holes filled;
  2. face.png: the head leveled (TILT), on transparency, centered on a square canvas (FACE_SIZE) (kept in
     sdk/spritesrc/devicon/ as the master; RIM > 0 adds a dark rim, off since it showed as a black outline);
  3. the game files: ui/gfx/dev_icon16/24/32/64.tga (32-bit, uncompressed, alpha, made ready for the game's texture
     drawing by alpha_test_ready: the scoreboard draws textures filtered and without blending, so a see-through
     pixel's color would be filtered into the edge and a faint pixel drawn solid): 32 and 64 downscaled from
     face.png, 16 and 24 drawn for that size (PIXEL_ART: the same head, traced pixel by pixel);
     the scoreboard draws the largest one that fits the row at its own size;
  4. --preview <dir>: the icon at 16, 24, 32 and 48 pixels on the scoreboard's background color (48: the 64 one
     scaled down, to judge the larger art).

Needs Python 3 and ImageMagick 6 (`convert`), as scripts/gen_hud_weapon_sprites.py.

Usage:
  scripts/gen_dev_icon.py                 # face.png + the TGAs
  scripts/gen_dev_icon.py --preview DIR   # also the preview sheet DIR/dev_icon_sizes.png
"""

import argparse
import os
import subprocess
import sys
from collections import deque

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(ROOT, "sdk", "spritesrc", "devicon")
AVATAR = os.path.join(SRC_DIR, "avatar.png")
FACE = os.path.join(SRC_DIR, "face.png")
OUT_DIR = os.path.join(ROOT, "gamedir", "syndicate", "ui", "gfx")
TGA_SIZES = (16, 24, 32, 64)

# 16 and 24 pixels: the same turned head, drawn pixel by pixel for that size (a downscale of face.png turns to grey
# mush there): traced from face.png with the fine details left out (ear rings, rim lines, the mouth's zigzag), a
# solid black outline, the red glasses solid with dark lenses and one glint each, the head's side panel shaded.
# K outline, B lens, W head, s side panel, R frame, G glint, . transparent. (rows, top offset)
PIXEL_COLORS = {"K": (10, 10, 10), "B": (10, 10, 10), "W": (237, 237, 237), "s": (178, 178, 182),
                "R": (255, 75, 37), "G": (255, 255, 255)}
PIXEL_ART = {
    16: ([
        "..KKKKKKKKKKKK..",
        "..KsKWWWWWWWWK..",
        "..KsKWRRRWWRRRR.",
        "..KsRRBGBRRBBGBR",
        "..KRKRGBBRRBGBBR",
        "KKKsKRBBBRRBBBBR",
        "KsKsKWRRRWWRRRR.",
        "KKKsKWWWWWWWWK..",
        "..KsKWKKKKKKWK..",
        "..KsKWWWWWWWWK..",
        "..KKKKKKKKKKKK..",
    ], 3),
    24: ([
        "...KKKKKKKKKKKKKKKKKK...",
        "...KssKWWWWWWWWWWWWWK...",
        "...KssKWRRRRRRWWRRRRRRR.",
        "...KssKRBBBBBBRRBBBBBBBR",
        "...KssRRBBBGBBRRBBBBGBBR",
        "...KRRKRBBGBBBRRBBBGBBBR",
        ".KKKssKRBBGBBBRRBBBGBBBR",
        "KKssssKRBBBBBBRRBBBBBBBR",
        "KKKsssKRBBBBBBRRBBBBBBBR",
        "KKssssKWRRRRRRWWRRRRRRR.",
        ".KKKssKWWWWWWWWWWWWWK...",
        "...KssKWWWKKKKKKKKWWK...",
        "...KssKWWWKKKKKKKKWWK...",
        "...KssKWWWWWWWWWWWWWK...",
        "...KKKKKKKKKKKKKKKKKK...",
        
    ], 4),
}

# Box around the head in avatar.png (1024x1024): x0, y0, x1, y1 (exclusive). The top edge cuts the antenna off
# above its base, the bottom edge runs through the neck, under the head's outline.
HEAD_BOX = (190, 178, 672, 505)
# The head's bottom edge (avatar.png coordinates): everything below this line is background (the neck)
CUT_LINE = ((240, 499), (630, 450))
TILT = 7.2         # degrees clockwise that level the head (the slope of CUT_LINE)
DARK = 96          # a pixel with max(R, G, B) below this is outline
FACE_SIZE = 256    # master canvas
FACE_MARGIN = 4    # transparent pixels around the head on the master (before the rim)
RIM = 0            # dark rim around the head on the master, pixels (0: none; it drew as a black outline in the game)

# Scoreboard background (scorepanel.res "bgcolor_override" 0 0 0 64) over a dark game scene, and the row of the
# local player (clientscheme ThisPlayerBgColor), for the preview
PREVIEW_BACKGROUNDS = (("scoreboard", (31, 29, 27)), ("own row", (66, 60, 52)))
PREVIEW_SIZES = (16, 24, 32, 48)


def run(args, data=None):
    return subprocess.run(args, input=data, stdout=subprocess.PIPE, check=True).stdout


def read_rgba(path, box):
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    raw = run(["convert", path, "-crop", f"{w}x{h}+{x0}+{y0}", "+repage", "-depth", "8", "rgba:-"])
    if len(raw) != w * h * 4:
        sys.exit(f"unexpected size reading {path}")
    return w, h, raw


def head_mask(w, h, raw):
    dark = bytearray(w * h)
    for i in range(w * h):
        r, g, b = raw[4 * i], raw[4 * i + 1], raw[4 * i + 2]
        dark[i] = max(r, g, b) < DARK

    # background: light pixels reachable from the box's edge
    bg = bytearray(w * h)
    (ax, ay), (bx, by) = CUT_LINE
    for i in range(w * h):
        x, y = HEAD_BOX[0] + i % w, HEAD_BOX[1] + i // w
        if y > ay + (by - ay) * (x - ax) / (bx - ax):
            bg[i] = 1
    q = deque()
    for x in range(w):
        for y in (0, h - 1):
            q.append(y * w + x)
    for y in range(h):
        for x in (0, w - 1):
            q.append(y * w + x)
    while q:
        i = q.popleft()
        if bg[i] == 2 or dark[i]:
            continue
        bg[i] = 2
        x, y = i % w, i // w
        if x > 0:
            q.append(i - 1)
        if x < w - 1:
            q.append(i + 1)
        if y > 0:
            q.append(i - w)
        if y < h - 1:
            q.append(i + w)

    # largest region of the rest (the head; drops the body's and the antenna's bits at the box's edges)
    label = [0] * (w * h)
    best, best_n, n_label = 0, 0, 0
    for s in range(w * h):
        if bg[s] or label[s]:
            continue
        n_label += 1
        label[s] = n_label
        q.append(s)
        n = 0
        while q:
            i = q.popleft()
            n += 1
            x, y = i % w, i // w
            for j, ok in ((i - 1, x > 0), (i + 1, x < w - 1), (i - w, y > 0), (i + w, y < h - 1)):
                if ok and not bg[j] and not label[j]:
                    label[j] = n_label
                    q.append(j)
        if n > best_n:
            best, best_n = n_label, n
    mask = bytearray(1 if label[i] == best else 0 for i in range(w * h))

    # fill holes: anything not reachable from the edge through non-head pixels
    outside = bytearray(w * h)
    for s in [x for x in range(w)] + [(h - 1) * w + x for x in range(w)] + \
             [y * w for y in range(h)] + [y * w + w - 1 for y in range(h)]:
        if not mask[s] and not outside[s]:
            outside[s] = 1
            q.append(s)
    while q:
        i = q.popleft()
        x, y = i % w, i // w
        for j, ok in ((i - 1, x > 0), (i + 1, x < w - 1), (i - w, y > 0), (i + w, y < h - 1)):
            if ok and not mask[j] and not outside[j]:
                outside[j] = 1
                q.append(j)
    return bytearray(0 if outside[i] else 1 for i in range(w * h))


def bbox(w, h, mask):
    xs = [i % w for i in range(w * h) if mask[i]]
    ys = [i // w for i in range(w * h) if mask[i]]
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


def make_face():
    w, h, raw = read_rgba(AVATAR, HEAD_BOX)
    mask = head_mask(w, h, raw)
    bx0, by0, bx1, by1 = bbox(w, h, mask)
    cw, ch = bx1 - bx0, by1 - by0
    out = bytearray(cw * ch * 4)
    for y in range(ch):
        for x in range(cw):
            si = (by0 + y) * w + bx0 + x
            di = (y * cw + x) * 4
            if mask[si]:
                out[di:di + 3] = raw[4 * si:4 * si + 3]
                out[di + 3] = 255
    inner = FACE_SIZE - 2 * (FACE_MARGIN + RIM)
    # head leveled (TILT), scaled to fit, then a dark rim (a dilated silhouette behind it), centered on the square
    # canvas
    run(["convert", "-size", f"{cw}x{ch}", "-depth", "8", "rgba:-",
         "-background", "none", "-virtual-pixel", "transparent", "-filter", "Lanczos", "+distort", "SRT", str(TILT),
         "-trim", "+repage", "-resize", f"{inner}x{inner}",
         "-background", "none", "-gravity", "center", "-extent", f"{FACE_SIZE}x{FACE_SIZE}",
         *(["(", "+clone", "-alpha", "extract", "-morphology", "Dilate", f"Disk:{RIM}",
            "-background", "rgb(8,8,8)", "-alpha", "shape", ")",
            "+swap", "-compose", "over", "-composite"] if RIM > 0 else []),
         "-define", "png:exclude-chunks=date,time", "-strip", FACE], data=bytes(out))


def alpha_test_ready(rgba, w, h, cell=0):
    """The picture as the game draws it right: the client's VGUI2 surface draws a TGA filtered and without blending
    (a pixel is drawn or not, docs/dev/SCOREBOARD_PORT.md "Badge edges"). So every pixel is made fully opaque (alpha
    128 and up) or fully see-through, and the see-through ones get the color of the opaque pixels next to them (grown
    outwards a few pixels): filtering at the edge then mixes in the art's own colors, not the black of the empty
    pixels, which drew a dark fringe. cell > 0: a sheet of cell x cell frames, colors never cross a frame's edge."""
    px = bytearray(rgba)
    for i in range(0, len(px), 4):
        px[i + 3] = 255 if px[i + 3] >= 128 else 0
    filled = [px[i + 3] == 255 for i in range(0, len(px), 4)]
    for _ in range(4):
        grow = []
        for y in range(h):
            for x in range(w):
                k = y * w + x
                if filled[k]:
                    continue
                acc = [0, 0, 0]
                n = 0
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        nx, ny = x + dx, y + dy
                        if not (0 <= nx < w and 0 <= ny < h) or not filled[ny * w + nx]:
                            continue
                        if cell and (nx // cell != x // cell or ny // cell != y // cell):
                            continue
                        j = (ny * w + nx) * 4
                        acc = [acc[c] + px[j + c] for c in range(3)]
                        n += 1
                if n:
                    grow.append((k, [v // n for v in acc]))
        for k, rgb in grow:
            px[k * 4:k * 4 + 3] = bytes(rgb)
            filled[k] = True
    return bytes(px)


def write_tga(path, w, h, rgba):
    """32-bit uncompressed TGA, rows bottom-up (descriptor 0x08: 8 alpha bits, origin bottom-left), as the other
    ui/gfx TGAs; written here because ImageMagick 6's TGA rows and origin flag disagree."""
    header = bytes([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, w & 255, w >> 8, h & 255, h >> 8, 32, 0x08])
    body = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            i = (y * w + x) * 4
            r, g, b, a = rgba[i:i + 4]
            body += bytes((b, g, r, a))
    with open(path, "wb") as f:
        f.write(header + bytes(body))


def pixel_art(size):
    rows, top = PIXEL_ART[size]
    rgba = bytearray(size * size * 4)
    for y, row in enumerate(rows):
        if len(row) != size:
            sys.exit(f"PIXEL_ART[{size}] row {y} is {len(row)} wide")
        for x, c in enumerate(row):
            if c != ".":
                i = ((top + y) * size + x) * 4
                rgba[i:i + 4] = bytes(PIXEL_COLORS[c] + (255,))
    return rgba


def make_tgas():
    for size in TGA_SIZES:
        path = os.path.join(OUT_DIR, f"dev_icon{size}.tga")
        if size in PIXEL_ART:
            write_tga(path, size, size, alpha_test_ready(pixel_art(size), size, size))
            print("wrote", os.path.relpath(path, ROOT))
            continue
        # Lanczos, then a light unsharp mask: the glasses' frame and lenses stay apart at 16 and 24 pixels
        raw = run(["convert", FACE, "-filter", "Lanczos", "-resize", f"{size}x{size}",
                   "-unsharp", "0x0.75+1.2+0", "-depth", "8", "rgba:-"])
        write_tga(path, size, size, alpha_test_ready(raw, size, size))
        print("wrote", os.path.relpath(path, ROOT))


def make_preview(out_dir):
    """One tile per size and background: the icon at its real size (left) and magnified with whole pixels to 96
    (right), drawn as the game draws it: the smallest TGA at least that large, scaled to the size."""
    os.makedirs(out_dir, exist_ok=True)
    args = ["convert"]
    for name, (r, g, b) in PREVIEW_BACKGROUNDS:
        args += ["("]
        for size in PREVIEW_SIZES:
            tga = min(t for t in TGA_SIZES + (max(TGA_SIZES),) if t >= size or t == max(TGA_SIZES))
            src = os.path.join(OUT_DIR, f"dev_icon{tga}.tga")
            zoom = 96 // size
            args += ["(", "-size", "200x140", f"xc:rgb({r},{g},{b})",
                     "(", src, "-filter", "Point" if tga == size else "Triangle", "-resize", f"{size}x{size}", "-write", "mpr:icon", "+delete", ")",
                     "mpr:icon", "-gravity", "west", "-geometry", f"+{36 - size // 2}+10", "-composite",
                     "(", "mpr:icon", "-filter", "Point", "-resize", f"{size * zoom}x{size * zoom}", ")",
                     "-gravity", "east", "-geometry", f"+{(96 - size * zoom) // 2 + 12}+10", "-composite",
                     "-gravity", "northwest", "-fill", "rgb(200,200,200)", "-pointsize", "13",
                     "-annotate", "+8+6", f"{size} px ({name})", ")"]
        args += ["+append", ")"]
    path = os.path.join(out_dir, "dev_icon_sizes.png")
    args += ["-append", "-strip", path]
    run(args)
    print("wrote", path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--preview", metavar="DIR", help="also write the preview sheet into DIR")
    a = ap.parse_args()
    make_face()
    print("wrote", os.path.relpath(FACE, ROOT))
    make_tgas()
    if a.preview:
        make_preview(a.preview)


if __name__ == "__main__":
    main()
