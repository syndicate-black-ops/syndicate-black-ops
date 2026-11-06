#!/usr/bin/env python3
"""Left-handed view model textures: the "_lh" copies of the textures that carry lettering.

cl_righthand 1 mirrors the view model left to right (docs/WEAPON_HAND.md), and lettering on it reads backwards. For
every texture listed below this script writes a copy, `<name>_lh.bmp` next to the source in sdk/modelsrc, in which
each lettering region is mirrored back in place. The model's QC lists the copies as a second skin family
($texturegroup "skinfamilies": family 0 the textures as they are, family 1 the "_lh" copies); the client draws a
mirrored view model with family 1 (cl_dll/weapon_hand.cpp WeaponHand_MirroredSkin). No pixel is painted: the copies
are the source pixels, moved.

  scripts/gen_lefthand_textures.py generate   write the _lh textures
  scripts/gen_lefthand_textures.py check      exit 1 if a _lh texture is missing or differs from what generate writes

Python 3 standard library only; deterministic. Then build the models: scripts/modelsrc.py install --accept
v_medikit v_satchel v_satchel_radio v_nm33 v_fa18 v_federal v_glauncher v_pm66 v_rc28 v_rifle v_rpg.

The axis of each region. The mirrored model is the original image flipped left to right on the screen, so a region
is flipped along the texture axis that runs left to right on the screen where the lettering is seen (the first frame
of the idle sequence, view from the eye): the screen flip pulled back into texture space through the mesh's UV
mapping. Measured from the sources' reference mesh and idle animation:
  * "u" (columns reversed): most regions; the pulled-back flip is close to diag(-1, 1).
  * "v" (rows reversed): the fa18's lettering (it runs along the barrel), the NM33's slanted top-left label, and
    expl.bmp, the EXPLOSIVE sticker on the satchel, which is held turned: the pulled-back flip is a reflection 16
    degrees off the v axis, so the sticker's lettering reads the right way round at a slant about 30 degrees
    steeper than the right-handed one.
Every v_ texture was looked at (2026-11-01). No lettering, so no copy: v_canon (ornaments), v_warhammer (rune-like
glyphs, not letters), v_mini (its displays are bars and noise), v_k444, v_ben, v_shotgun, v_katana, v_sword (a
skull), v_grenade, v_tripmine, v_squeak, v_wong; the RPG's other panels are glyph-like patterns.
"""
import argparse
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "sdk", "modelsrc", "weapons")

# texture (relative to sdk/modelsrc/weapons) -> regions (x0, y0, x1, y1 in pixels from the top left, x1/y1
# exclusive; axis "u" or "v", what the region holds)
TEXTURES = {
    "medikit/v_/textures/medikitbody.bmp": [
        (68, 124, 186, 155, "u", "FIRST AID"),
        (10, 48, 33, 161, "u", "open this end, left side, with its arrow"),
        (222, 48, 246, 161, "u", "open this end, right side, with its arrow"),
    ],
    "satchel/v_/textures/expl.bmp": [
        (11, 8, 103, 39, "v", "EXPLOSIVE, the bomb and the small print"),
    ],
    "satchel/v_/textures/satchel_body.bmp": [
        (50, 14, 108, 37, "u", "C4 PLASTIC EXPLOSIVE plate"),
    ],
    "satchel/v_/textures/radioside.bmp": [
        (9, 9, 39, 26, "u", "the radio's display"),
    ],
    "satchel_radio/v_/textures/det_front.bmp": [
        (2, 5, 17, 11, "u", "detonator label, left"),
        (48, 4, 60, 18, "u", "detonator label, right"),
    ],
    "nm33/v_/textures/v_nm33-null.bmp": [
        (99, 411, 142, 425, "u", "the red digits 223 30"),
        (63, 222, 170, 231, "u", "CROON NM33 Sniper rifle"),
        (5, 119, 77, 136, "v", "slanted label, top left"),
        (54, 92, 75, 99, "u", "small label under the triangle logo"),
    ],
    "fa18/v_/textures/fa18_01.bmp": [
        (365, 213, 400, 276, "v", "red lettering"),
        (403, 318, 425, 420, "v", "white lettering"),
    ],
    "federal/v_/textures/skin.bmp": [
        (330, 31, 435, 44, "u", "Federal Assault Rifle"),
        (26, 74, 61, 114, "u", "red fire-mode labels, one side"),
        (539, 74, 574, 114, "u", "red fire-mode labels, other side"),
        (34, 205, 71, 215, "u", "small plate, one side"),
        (546, 205, 584, 215, "u", "small plate, other side"),
    ],
    "glauncher/v_/textures/skin1.bmp": [
        (51, 43, 122, 60, "u", "VINDICO AR-7"),
    ],
    "glauncher/v_/textures/skin2.bmp": [
        (50, 62, 78, 78, "u", "red label"),
    ],
    "pm66/v_/textures/test.bmp": [
        (341, 81, 396, 94, "u", "name plate"),
        (271, 49, 319, 96, "u", "sticker"),
    ],
    "rc28/v_/textures/rccote.bmp": [
        (47, 32, 79, 43, "u", "RC 28"),
        (82, 29, 111, 39, "u", "CROON"),
    ],
    "rifle/v_/textures/skinplasma2.bmp": [
        (305, 225, 432, 242, "u", "Plasma Sniper Rifle SW"),
        (358, 49, 390, 66, "u", "red label"),
        (73, 356, 113, 369, "u", "small label"),
        (230, 322, 252, 333, "u", "red 077"),
    ],
    "rpg/v_/textures/v_rpg-null.bmp": [
        (131, 116, 186, 128, "u", "SAAN"),
    ],
}


def lh_name(path):
    stem, ext = os.path.splitext(path)
    return stem + "_lh" + ext


def read_bmp(path):
    """An 8-bit BMP: (file bytes, pixel data offset, width, height, row stride, rows bottom-up)."""
    data = bytearray(open(path, "rb").read())
    if data[:2] != b"BM":
        raise SystemExit("%s: not a BMP" % path)
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp, compression = struct.unpack_from("<HI", data, 28)
    if bpp != 8 or compression != 0:
        raise SystemExit("%s: %d bits, compression %d: only uncompressed 8-bit BMPs" % (path, bpp, compression))
    bottom_up = height > 0
    height = abs(height)
    stride = (width + 3) & ~3
    if offset + stride * height > len(data):
        raise SystemExit("%s: truncated" % path)
    return data, offset, width, height, stride, bottom_up


def mirrored(path, regions):
    data, offset, width, height, stride, bottom_up = read_bmp(path)

    def at(x, y):
        row = (height - 1 - y) if bottom_up else y
        return offset + row * stride + x

    out = bytearray(data)
    for x0, y0, x1, y1, axis, what in regions:
        if not (0 <= x0 < x1 <= width and 0 <= y0 < y1 <= height):
            raise SystemExit("%s: region %r outside the %dx%d texture" % (path, (x0, y0, x1, y1), width, height))
        for y in range(y0, y1):
            for x in range(x0, x1):
                sx, sy = (x0 + x1 - 1 - x, y) if axis == "u" else (x, y0 + y1 - 1 - y)
                out[at(x, y)] = data[at(sx, sy)]
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("command", choices=("generate", "check"))
    args = ap.parse_args()

    bad = 0
    for rel, regions in sorted(TEXTURES.items()):
        src = os.path.join(SRC, rel)
        dst = lh_name(src)
        want = mirrored(src, regions)
        have = open(dst, "rb").read() if os.path.exists(dst) else None
        name = os.path.relpath(dst, ROOT)
        if args.command == "generate":
            if have != want:
                with open(dst, "wb") as f:
                    f.write(want)
                print("wrote     %s" % name)
            else:
                print("unchanged %s" % name)
        elif have != want:
            print("%s %s" % ("missing " if have is None else "differs ", name))
            bad = 1
        else:
            print("ok        %s" % name)
    return bad


if __name__ == "__main__":
    sys.exit(main())
