#!/usr/bin/env python3
"""SBO info_waypoint HUD sprites (docs/INFO_WAYPOINT.md): sprites/waypoint.spr (the default marker icon) and
sprites/waypoint_arrow.spr (the arrow beside a marker on the screen's edge, pointing right).

    scripts/gen_waypoint_sprites.py generate [--gamedir gamedir/syndicate] [--review DIR]

Drawn from the shapes below with the shape functions of scripts/gen_editor_sprites.py (no art files), supersampled
4x for smooth edges. Both are white, texture format SPR_INDEXALPHA (the palette index is the opacity, the color is
palette entry 255): the client tints them with the marker's color and draws a dark copy under them as a shadow.
64x64, one frame, type VP_PARALLEL. Python 3 standard library only; same shapes, same bytes.
"""

import argparse
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_editor_sprites as ges  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIZE = 64
SS = 4  # supersampling


def icon_shape(n):
    """The marker: a diamond ring around a filled diamond (n x n supersampled box)."""
    c = n / 2.0
    u = n / 64.0

    def diamond(r):
        return ges.poly((c, c - r), (c + r, c), (c, c + r), (c - r, c))
    ring = ges.minus(diamond(29 * u), diamond(20 * u))
    core = diamond(11 * u)
    return ges.union(ring, core)


def arrow_shape(n):
    """The arrow: a chevron pointing right, its tip at the right edge."""
    u = n / 64.0
    outer = ges.poly((8 * u, 6 * u), (58 * u, 32 * u), (8 * u, 58 * u), (20 * u, 32 * u))
    return outer


def render(shape):
    """SIZE x SIZE opacities 0..255."""
    n = SIZE * SS
    mask = shape(n)
    out = []
    for y in range(SIZE):
        for x in range(SIZE):
            hits = 0
            for sy in range(SS):
                for sx in range(SS):
                    if mask(x * SS + sx + 0.5, y * SS + sy + 0.5):
                        hits += 1
            out.append(int(round(255.0 * hits / (SS * SS))))
    return out


def spr_bytes(alpha):
    """Sprite version 2, VP_PARALLEL, SPR_INDEXALPHA, one frame; palette: gray ramp, entry 255 white."""
    palette = [(i, i, i) for i in range(256)]
    radius = math.hypot(SIZE / 2.0, SIZE / 2.0)
    out = [struct.pack("<4siiifiiifi", b"IDSP", 2, 2, 2, radius, SIZE, SIZE, 1, 0.0, 1),
           struct.pack("<h", 256), b"".join(bytes(c) for c in palette),
           struct.pack("<iiiii", 0, -SIZE // 2, SIZE // 2, SIZE, SIZE), bytes(alpha)]
    return b"".join(out)


SPRITES = [("waypoint", icon_shape), ("waypoint_arrow", arrow_shape)]


def cmd_generate(args):
    out_dir = os.path.join(args.gamedir, "sprites")
    for name, shape in SPRITES:
        alpha = render(shape)
        path = os.path.join(out_dir, name + ".spr")
        with open(path, "wb") as fh:
            fh.write(spr_bytes(alpha))
        print("wrote", path)
        if args.review:
            os.makedirs(args.review, exist_ok=True)
            png = os.path.join(args.review, name + ".png")
            ges.write_png(png, SIZE, SIZE, [(255, 255, 255, a) for a in alpha], scale=4)
            print("wrote", png)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    g = sub.add_parser("generate")
    g.add_argument("--gamedir", default=os.path.join(ROOT, "gamedir", "syndicate"))
    g.add_argument("--review")
    args = ap.parse_args()
    cmd_generate(args)


if __name__ == "__main__":
    main()
