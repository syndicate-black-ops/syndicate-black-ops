#!/usr/bin/env python3
"""SBO inventory HUD icons (docs/INVENTORY.md): sprites/320inventory.spr and sprites/640inventory.spr, and their
lines for sprites/hud.txt.

    scripts/gen_inventory_sprites.py generate [--gamedir gamedir/syndicate] [--review DIR]
    scripts/gen_inventory_sprites.py hudtxt          print the hud.txt lines

Drawn from the specs below with the shape functions of scripts/gen_editor_sprites.py (no art files): flat
pictograms, supersampled 4x for smooth edges. HUD sprites are drawn additive (black is see-through), so the icons
are light on black; the HUD draws them in their own colors (white tint), so the keycards keep red / blue / yellow and
the generic key uses the HUD color. Two sets as the rest of hud.txt: 320 (18x18 icons) and 640 (36x36 icons), each
in one sheet. Python 3 standard library only; same specs, same bytes.
"""

import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_editor_sprites as ges  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SS = 4                                       # supersampling
HUD_COLOR = (66, 246, 201)                   # cl_dll/hud.h RGB_YELLOWISH (SBO's HUD color)

# hud.txt name, base color, shape
ICONS = [
    ("inv_card_red", (235, 55, 45), "card"),
    ("inv_card_blue", (60, 120, 245), "card"),
    ("inv_card_yellow", (245, 215, 50), "card"),
    ("inv_key", HUD_COLOR, "key"),
]

# set: (icon size, sheet size, cell step)
SETS = {320: (18, 64, 0), 640: (36, 128, 0)}
GRID = 2  # icons per sheet row


def shapes(kind, s):
    """List of (mask, shade) for an icon drawn in an s x s box (supersampled coordinates). shade: 1.0 the base
    color, < 1 darker, > 1 toward white."""
    u = s / 36.0
    if kind == "card":
        body = ges.rrect(int(3 * u), int(8 * u), int(33 * u), int(28 * u), 3 * u)
        stripe = ges.rect(int(3 * u), int(11 * u), int(33 * u), int(14.5 * u))
        chip = ges.rrect(int(7 * u), int(17 * u), int(14 * u), int(23 * u), 1.2 * u)
        line1 = ges.rect(int(17 * u), int(18 * u), int(29 * u), int(19.5 * u))
        line2 = ges.rect(int(17 * u), int(21.5 * u), int(25 * u), int(23 * u))
        rim = ges.minus(body, ges.rrect(int(4.5 * u), int(9.5 * u), int(31.5 * u), int(26.5 * u), 2 * u))
        return [(body, 0.8), (rim, 1.25), (stripe, 0.35), (chip, 1.6), (line1, 0.45), (line2, 0.45)]
    if kind == "key":
        ring = ges.arc(11 * u, 18 * u, 4 * u, 8.5 * u)
        shaft = ges.rect(int(18 * u), int(16 * u), int(33 * u), int(20.5 * u))
        tooth1 = ges.rect(int(26 * u), int(20 * u), int(29 * u), int(26 * u))
        tooth2 = ges.rect(int(30 * u), int(20 * u), int(33 * u), int(24 * u))
        shine = ges.arc(11 * u, 18 * u, 6.5 * u, 8.5 * u, 100, 200)
        return [(ges.union(ring, shaft, tooth1, tooth2), 1.0), (shine, 1.35)]
    raise ValueError(kind)


def shade(c, k):
    if k <= 1.0:
        return tuple(int(round(v * k)) for v in c)
    return ges.mix(c, (255, 255, 255), min(1.0, k - 1.0))


def render(kind, color, size):
    """size x size RGB pixels (black background)."""
    n = size * SS
    layers = shapes(kind, n)
    acc = [[0.0, 0.0, 0.0] for _ in range(size * size)]
    for y in range(n):
        for x in range(n):
            px = None
            for mask, k in layers:
                if mask(x + 0.5, y + 0.5):
                    px = shade(color, k)
            if px is None:
                continue
            a = acc[(y // SS) * size + (x // SS)]
            a[0] += px[0]
            a[1] += px[1]
            a[2] += px[2]
    return [tuple(int(round(v / (SS * SS))) for v in a) for a in acc]


def quantize(pixels):
    """Palette (256 RGB) and indices; channels rounded until there are at most 256 colors."""
    for step in (1, 2, 4, 6, 8, 12, 16):
        q = [tuple(min(255, (v + step // 2) // step * step) for v in p) for p in pixels]
        colors = sorted(set(q))
        if (0, 0, 0) in colors:
            colors.remove((0, 0, 0))
        colors = [(0, 0, 0)] + colors
        if len(colors) <= 256:
            index = {c: i for i, c in enumerate(colors)}
            return colors + [(0, 0, 0)] * (256 - len(colors)), [index[c] for c in q]
    raise ValueError("too many colors")


def sheet(res):
    size, sheet_size, _ = SETS[res]
    step = sheet_size // GRID
    pixels = [(0, 0, 0)] * (sheet_size * sheet_size)
    places = []
    for i, (name, color, kind) in enumerate(ICONS):
        x0, y0 = (i % GRID) * step, (i // GRID) * step
        icon = render(kind, color, size)
        for y in range(size):
            for x in range(size):
                pixels[(y0 + y) * sheet_size + x0 + x] = icon[y * size + x]
        places.append((name, x0, y0, size, size))
    return sheet_size, pixels, places


def spr_bytes(w, h, indices, palette):
    """Sprite version 2, VP_PARALLEL, SPR_ADDITIVE, one frame (as the 640hud*.spr sheets)."""
    radius = ((w / 2.0) ** 2 + (h / 2.0) ** 2) ** 0.5
    out = [struct.pack("<4siiifiiifi", b"IDSP", 2, 2, 1, radius, w, h, 1, 0.0, 1),
           struct.pack("<h", 256), b"".join(bytes(c) for c in palette),
           struct.pack("<iiiii", 0, -(w // 2), h // 2, w, h), bytes(indices)]
    return b"".join(out)


def hudtxt_lines():
    lines = []
    for res in (320, 640):
        _, _, places = sheet(res)
        for name, x, y, w, h in places:
            lines.append("%s\t%d %dinventory\t%d\t%d\t%d\t%d" % (name, res, res, x, y, w, h))
    return lines


def cmd_generate(args):
    out_dir = os.path.join(args.gamedir, "sprites")
    for res in (320, 640):
        size, pixels, _ = sheet(res)
        palette, indices = quantize(pixels)
        path = os.path.join(out_dir, "%dinventory.spr" % res)
        with open(path, "wb") as fh:
            fh.write(spr_bytes(size, size, indices, palette))
        print("wrote", path)
        if args.review:
            os.makedirs(args.review, exist_ok=True)
            png = os.path.join(args.review, "%dinventory.png" % res)
            ges.write_png(png, size, size, [palette[i] + (255,) for i in indices], scale=8 if res == 320 else 4)
            print("wrote", png)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("generate")
    g.add_argument("--gamedir", default=os.path.join(ROOT, "gamedir", "syndicate"))
    g.add_argument("--review")
    sub.add_parser("hudtxt")
    args = ap.parse_args()
    if args.cmd == "generate":
        cmd_generate(args)
    else:
        print("\n".join(hudtxt_lines()))


if __name__ == "__main__":
    main()
