#!/usr/bin/env python3
"""Generates SBO's HUD weapon icons (weapon / weapon_s) from side-profile artwork. See docs/HUD_SPRITES.md.
Also the kill feed icons (d_<weapon> in sprites/hud.txt): sdk/spritesrc/killfeed/README.md.

  scripts/gen_hud_weapon_sprites.py generate <art-dir> [--gamedir gamedir/syndicate] [--review <dir>]
                                             [--old-sprites <dir>] [--normal-level 0.45] [--work <dir>]
  scripts/gen_hud_weapon_sprites.py killfeed [--review <dir>]          # kill feed d_ icons (sdk/spritesrc/killfeed)
  scripts/gen_hud_weapon_sprites.py killfeed-sources <art-dir>
  scripts/gen_hud_weapon_sprites.py killfeed-custom-sources <amxmodx-plugins checkout>  # plugin icons from models
  scripts/gen_hud_weapon_sprites.py killfeed-custom --version N --out <dir> [--inc <killfeed_icons.inc>]
  scripts/gen_hud_weapon_sprites.py info <file.spr>...
  scripts/gen_hud_weapon_sprites.py dump <file.spr> <out.png>

Needs Python 3 (standard library only) and ImageMagick's `convert` (reads the artwork, resamples, writes the review
PNGs). The .spr files are written by this script.
"""

import argparse
import math
import os
import re
import struct
import subprocess
import sys
import tempfile

# sprites/weapon_<name>.txt -> artwork file name (without extension) and options.
#   main_only: keep the largest shape of the artwork only (the file holds a second, smaller drawing)
#   cut_red:   strongly red areas of the artwork become gaps in the shape (a red cross on a plain box); of those
#              areas only the big ones are kept (CUT_KEEP_RATIO), so small red lettering does not turn to noise
#   cut_dark:  the same for near-black areas (an opening drawn in black)
# Weapons that are not listed keep their sprites: no artwork exists for them.
# The sheets are packed in this order: add new weapons at the end, so that the other sheets stay as they are.
WEAPONS = [
    ("ben", "benelli", {}),
    ("canon", "canon-of-terror", {}),
    ("chaingun", "minigun", {}),
    ("chainsaw", "chainsaw_sword", {"main_only": True}),
    ("fa18", "fa18", {}),
    ("federal", "federal", {}),
    ("glauncher", "glauncher", {}),
    ("handgrenade", "handgrenade", {}),
    ("k444", "k444", {}),
    ("nm33", "nm33-new", {}),
    ("pm66", "pm66", {}),
    ("rc28", "rc28", {}),
    ("rpg", "rpg", {}),
    ("satchel", "satchel", {}),
    ("snark", "pitbull", {}),
    ("spa", "saan", {}),
    ("sw", "plasma-sniper", {}),
    ("sword", "sword", {}),
    ("tripmine", "mine", {}),
    ("warhammer", "warhammer", {}),
    ("wong", "fists", {}),
    ("medikit", "medikit", {"cut_red": True}),
]

ART_EXTENSIONS = (".psd", ".png", ".tga", ".bmp", ".tif", ".tiff")

# Sprite sets. cell: the icon rectangle of the set (HL25's hud.txt / weapon_*.txt: 170x45 at 640, 2x in the 1280 set,
# 3x in the 2560 set; 80x20 in the 320 set). sheet: largest sheet (HL25's own HUD sprites are 512 wide at most).
# margin: empty border inside the cell (x, y), in texels: 6x4 at 640, scaled with the set.
TIERS = {
    320: {"cell": (80, 20), "sheet": (256, 256), "margin": (3, 2)},
    640: {"cell": (170, 45), "sheet": (256, 256), "margin": (6, 4)},
    1280: {"cell": (340, 90), "sheet": (512, 512), "margin": (12, 8)},
    2560: {"cell": (510, 135), "sheet": (512, 512), "margin": (18, 12)},
}

SHEET_NAME = "{res}weapons{index}"  # + "_s" for the selected icons
ALPHA_SOLID = 0.999     # artwork whose alpha channel is fuller than this has no transparency: use its brightness
WHITE_THRESHOLD = "92%"  # ... the shape is every pixel darker than this (artwork flattened on white)
CLOSE_KERNEL = "Disk:3"  # closes the hairlines between the parts of a drawing (3 px in the artwork)
SPECK_RATIO = 0.002     # shapes smaller than this part of the largest one are stray specks
CUT_RED = "(u.r-max(u.g,u.b))>0.12"  # cut_red: red exceeds green and blue by this much (artwork flattened on white)
CUT_SMOOTH = ["-morphology", "Close", "Disk:5", "-morphology", "Open", "Disk:3"]  # joins a mottled area, drops dots
CUT_KEEP_RATIO = 0.5    # cut areas smaller than this part of the largest one are dropped (lettering, arrows)
CUT_DARK = "max(u.r,max(u.g,u.b))<0.1"  # cut_dark: near-black areas (an opening drawn as black)

PREVIEW_TINT = (255, 160, 0)  # RGB_YELLOWISH (cl_dll/hud.h)
PREVIEW_NORMAL_ALPHA = 192    # DrawWList draws the icons that are not selected at 192 / 255


def convert(args, data=None):
    proc = subprocess.run(["convert"] + args, input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if proc.returncode != 0:
        sys.exit("convert %s failed:\n%s" % (" ".join(args), proc.stderr.decode(errors="replace")))
    return proc.stdout


# ---------------------------------------------------------------------------------------------------------------
# GoldSrc sprite files (dsprite_t, engine spritegn.h): header, 8-bit palette, frames.
# ---------------------------------------------------------------------------------------------------------------

SPR_HEADER = struct.Struct("<4siiifiiifi")  # ident version type texFormat boundingradius width height numframes beamlength synctype
SPR_FRAME = struct.Struct("<5i")            # frame type (0 = single), origin x, origin y, width, height

SPR_VP_PARALLEL = 2
SPR_ADDITIVE = 1
ST_RAND = 1


class Sprite:
    def __init__(self, width, height, pixels, palette=None, sprite_type=SPR_VP_PARALLEL, tex_format=SPR_ADDITIVE,
                 synctype=ST_RAND):
        assert len(pixels) == width * height
        self.width, self.height, self.pixels = width, height, bytes(pixels)
        # grey ramp: index = brightness (the HUD draws these additively: black adds nothing)
        self.palette = palette if palette is not None else bytes(v for i in range(256) for v in (i, i, i))
        self.type, self.tex_format, self.synctype = sprite_type, tex_format, synctype
        self.version, self.numframes, self.beamlength = 2, 1, 0.0
        self.radius = math.sqrt((width / 2.0) ** 2 + (height / 2.0) ** 2)
        self.origin = (-(width // 2), height // 2)
        self.trailing = 0

    def to_bytes(self):
        out = SPR_HEADER.pack(b"IDSP", self.version, self.type, self.tex_format, self.radius, self.width,
                              self.height, 1, self.beamlength, self.synctype)
        out += struct.pack("<h", len(self.palette) // 3) + self.palette
        out += SPR_FRAME.pack(0, self.origin[0], self.origin[1], self.width, self.height) + self.pixels
        return out

    @staticmethod
    def from_bytes(data):
        ident, version, stype, texfmt, radius, width, height, numframes, beam, sync = SPR_HEADER.unpack_from(data, 0)
        if ident != b"IDSP" or version != 2:
            raise ValueError("not a GoldSrc sprite (ident %r, version %d)" % (ident, version))
        offset = SPR_HEADER.size
        (colors,) = struct.unpack_from("<h", data, offset)
        offset += 2
        palette = data[offset:offset + colors * 3]
        offset += colors * 3
        group, ox, oy, fw, fh = SPR_FRAME.unpack_from(data, offset)
        if group != 0:
            raise ValueError("frame groups are not supported")
        offset += SPR_FRAME.size
        spr = Sprite(fw, fh, data[offset:offset + fw * fh], palette, stype, texfmt, sync)
        spr.version, spr.numframes, spr.beamlength, spr.radius, spr.origin = version, numframes, beam, radius, (ox, oy)
        spr.max_size = (width, height)
        # only the first frame is read (HUD sheets have one)
        spr.trailing = len(data) - (offset + fw * fh) if numframes == 1 else -1
        return spr

    @staticmethod
    def load(path):
        with open(path, "rb") as f:
            return Sprite.from_bytes(f.read())

    def describe(self):
        return ("type %d texFormat %d radius %.3f size %dx%d frames %d beamlength %g synctype %d palette %d "
                "origin %d,%d" % (self.type, self.tex_format, self.radius, self.width, self.height, self.numframes,
                                  self.beamlength, self.synctype, len(self.palette) // 3, self.origin[0],
                                  self.origin[1]))

    def rgb(self, rect=None):
        """RGB bytes of rect (x, y, w, h), or of the whole frame."""
        x0, y0, w, h = rect if rect else (0, 0, self.width, self.height)
        out = bytearray()
        for y in range(y0, y0 + h):
            for c in self.pixels[y * self.width + x0:y * self.width + x0 + w]:
                out += self.palette[c * 3:c * 3 + 3]
        return bytes(out)

    def grey(self, rect):
        """Brightness (red channel of the palette) of rect (x, y, w, h), one bytes object per row."""
        x0, y0, w, h = rect
        red = self.palette[0::3]
        return [bytes(red[c] for c in self.pixels[y * self.width + x0:y * self.width + x0 + w])
                for y in range(y0, y0 + h)]


def write_sprite(path, width, height, pixels):
    """Writes the sheet, reads it back and checks it."""
    spr = Sprite(width, height, pixels)
    data = spr.to_bytes()
    with open(path, "wb") as f:
        f.write(data)
    back = Sprite.load(path)
    expected = 42 + 768 + 20 + width * height
    if (len(data) != expected or back.trailing != 0 or back.pixels != spr.pixels or back.palette != spr.palette
            or (back.width, back.height) != (width, height) or back.max_size != (width, height)
            or (back.type, back.tex_format, back.numframes) != (SPR_VP_PARALLEL, SPR_ADDITIVE, 1)):
        sys.exit("%s: read-back check failed" % path)
    return len(data)


# ---------------------------------------------------------------------------------------------------------------
# Artwork -> silhouette
# ---------------------------------------------------------------------------------------------------------------

def find_art(art_dir, stem):
    for ext in ART_EXTENSIONS:
        path = os.path.join(art_dir, stem + ext)
        if os.path.isfile(path):
            return path
    return None


def components(path):
    """Connected shapes of a black/white image: (id, area, is_white), largest first."""
    out = convert([path, "-define", "connected-components:verbose=true", "-connected-components", "4", "null:"])
    found = []
    for line in out.decode().splitlines():
        m = re.match(r"\s*(\d+): \S+ \S+ (\d+) (?:s?rgba?|gray)\(([\d.]+)", line)
        if m:
            found.append((int(m.group(1)), int(m.group(2)), float(m.group(3)) > 127))
    return found


def make_mask(src, opts, work, name):
    """The weapon's shape, white on black, at the artwork's size, trimmed to the shape. Returns the PNG's path."""
    first = src + "[0]"  # the flattened image of a PSD
    stage1 = os.path.join(work, name + "-1.png")
    stage2 = os.path.join(work, name + "-2.png")
    cut = os.path.join(work, name + "-cut.png")
    mask = os.path.join(work, name + ".png")

    channels = convert([first, "-format", "%[channels]", "info:"]).decode()
    alpha = float(convert([first, "-alpha", "extract", "-format", "%[fx:mean]", "info:"])) if "a" in channels else 1.0
    if alpha < ALPHA_SOLID:
        shape = [first, "-alpha", "extract", "-threshold", "50%"]
    else:
        shape = [first, "-background", "white", "-alpha", "remove", "-alpha", "off", "-colorspace", "gray",
                 "-threshold", WHITE_THRESHOLD, "-negate"]
    # no interior lines: close the hairlines. Real holes (trigger guards) are wider and stay.
    convert(shape + ["-bordercolor", "black", "-border", "8", "-morphology", "Close", CLOSE_KERNEL,
                     "-colorspace", "gray", "-depth", "8", stage1])

    white = [c for c in components(stage1) if c[2]]
    if not white:
        sys.exit("%s: no shape found" % src)
    largest = max(c[1] for c in white)
    drop = [c for c in white if c[1] != largest and (opts.get("main_only") or c[1] < largest * SPECK_RATIO)]
    args = [stage1]
    if drop:
        args += ["-define", "connected-components:remove=" + ",".join(str(c[0]) for c in drop),
                 "-define", "connected-components:mean-color=true", "-connected-components", "4",
                 "-background", "black", "-alpha", "remove", "-alpha", "off"]
    convert(args + ["-colorspace", "gray", "-depth", "8", stage2])

    # gaps for the red areas of the artwork (same size as stage 2: with its border)
    flat = [first, "-background", "white", "-alpha", "remove", "-alpha", "off"]
    cuts = []
    if opts.get("cut_red"):
        cuts.append(flat + ["-fx", CUT_RED])
    if opts.get("cut_dark"):
        cuts.append(flat + ["-fx", CUT_DARK])
    args, cut_areas = [stage2], []
    for kind in cuts:
        convert(kind + ["-bordercolor", "black", "-border", "8"] + CUT_SMOOTH + ["-colorspace", "gray", "-depth", "8",
                                                                             cut])
        areas = [c for c in components(cut) if c[2]]
        if not areas:
            continue
        keep = max(c[1] for c in areas) * CUT_KEEP_RATIO
        small = [c for c in areas if c[1] < keep]
        cut_args = [cut]
        if small:
            cut_args += ["-define", "connected-components:remove=" + ",".join(str(c[0]) for c in small),
                         "-define", "connected-components:mean-color=true", "-connected-components", "4",
                         "-background", "black", "-alpha", "remove", "-alpha", "off"]
        args += ["("] + cut_args + ["-negate", ")", "-compose", "Multiply", "-composite",
                 "-compose", "Over"]  # -border composes too
        cut_areas += [c[1] for c in areas if c[1] >= keep]
    convert(args + ["-bordercolor", "black", "-border", "1", "-trim", "+repage", "-colorspace", "gray", "-depth", "8",
                    mask])
    width, height = (int(v) for v in convert([mask, "-format", "%w %h", "info:"]).split())
    return mask, (width, height), {"alpha": alpha < ALPHA_SOLID, "removed": [c[1] for c in drop], "cut": cut_areas}


def render_icon(mask, tier):
    """The shape fitted into the set's cell: brightness bytes, cell width x cell height."""
    (cw, ch), (mx, my) = tier["cell"], tier["margin"]
    data = convert([mask, "-filter", "Lanczos", "-resize", "%dx%d" % (cw - 2 * mx, ch - 2 * my),
                    "-gravity", "center", "-background", "black", "-extent", "%dx%d" % (cw, ch),
                    "-colorspace", "gray", "-depth", "8", "gray:-"])
    if len(data) != cw * ch:
        sys.exit("%s: unexpected icon size" % mask)
    return data


# ---------------------------------------------------------------------------------------------------------------
# Sheets and sprites/weapon_*.txt
# ---------------------------------------------------------------------------------------------------------------

def pow2(n):
    p = 1
    while p < n:
        p *= 2
    return p


def pack(res, icons):
    """icons: [(name, bytes)] -> [(sheet index (1-based), width, height, pixels, {name: (x, y, w, h)})]"""
    tier = TIERS[res]
    (cw, ch), (sw, sh) = tier["cell"], tier["sheet"]
    cols, rows = sw // cw, sh // ch
    sheets = []
    for start in range(0, len(icons), cols * rows):
        chunk = icons[start:start + cols * rows]
        height = max(pow2(((len(chunk) + cols - 1) // cols) * ch), 16)
        pixels = bytearray(sw * height)
        rects = {}
        for i, (name, data) in enumerate(chunk):
            # down the first column, then the next one (the old sheets: one column of five)
            x, y = (i // rows) * cw, (i % rows) * ch
            for row in range(ch):
                pixels[(y + row) * sw + x:(y + row) * sw + x + cw] = data[row * cw:(row + 1) * cw]
            rects[name] = (x, y, cw, ch)
        sheets.append((len(sheets) + 1, sw, height, bytes(pixels), rects))
    return sheets


ENTRY = re.compile(r"^\s*(\S+)\s+(\d+)\s+(\S+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*(//.*)?$")
COUNT = re.compile(r"^(\s*)(\d+)(\s*(//.*)?)$")


def parse_weapon_txt(path):
    """{(entry name, set): (sprite, x, y, w, h)} of a sprites/weapon_*.txt (first entry wins, as in the client)."""
    entries = {}
    with open(path, "rb") as f:
        for line in f.read().decode("latin-1").splitlines():
            m = ENTRY.match(line)
            if m:
                entries.setdefault((m.group(1), int(m.group(2))),
                                   (m.group(3),) + tuple(int(m.group(i)) for i in range(4, 8)))
    return entries


def update_weapon_txt(path, placement):
    """Rewrites the weapon / weapon_s lines of a sprites/weapon_*.txt; every other line is kept as it is.

    placement: {set: (sheet name, (x, y, w, h))}. Returns the lines written."""
    with open(path, "rb") as f:
        raw = f.read().decode("latin-1")
    eol = "\r\n" if "\r\n" in raw else "\n"
    lines = raw.split(eol)

    def entry(name, res):
        sheet, (x, y, w, h) = placement[res]
        return "%s%s%d %s\t%d\t%d\t%d\t%d" % (name, "\t\t\t" if name == "weapon" else "\t\t", res,
                                              sheet + ("_s" if name == "weapon_s" else ""), x, y, w, h)

    out, written, count_at, last_entry, seen, entries = [], [], None, None, set(), 0
    for line in lines:
        m = ENTRY.match(line)
        if m and m.group(1) in ("weapon", "weapon_s"):
            key = (m.group(1), int(m.group(2)))
            if key[1] not in placement or key in seen:
                continue  # a set that is no longer generated, or a duplicate
            seen.add(key)
            line = entry(*key)
            written.append(line)
        if m:
            entries += 1
            last_entry = len(out)
        elif count_at is None and COUNT.match(line):
            count_at = len(out)
        out.append(line)
    if count_at is None or last_entry is None:
        sys.exit("%s: no sprite count / entries found" % path)

    # sets the file didn't have: after the last entry, biggest set last
    new = [entry(name, res) for res in sorted(placement) for name in ("weapon", "weapon_s") if (name, res) not in seen]
    out[last_entry + 1:last_entry + 1] = new
    written += new
    entries += len(new)
    m = COUNT.match(out[count_at])
    out[count_at] = "%s%d%s" % (m.group(1), entries, m.group(3))

    with open(path, "wb") as f:
        f.write(eol.join(out).encode("latin-1"))
    return written


# ---------------------------------------------------------------------------------------------------------------
# Review sheets
# ---------------------------------------------------------------------------------------------------------------

class Canvas:
    def __init__(self, width, height):
        self.width, self.height = width, height
        self.rgb = bytearray(width * height * 3)
        self.labels = []

    def blit_grey(self, x, y, rows, tint, scale=255):
        """Additive, tinted like the HUD: colour * brightness * scale."""
        for dy, row in enumerate(rows):
            base = ((y + dy) * self.width + x) * 3
            for dx, v in enumerate(row):
                if v:
                    for c in range(3):
                        i = base + dx * 3 + c
                        self.rgb[i] = min(255, self.rgb[i] + tint[c] * v * scale // (255 * 255))

    def frame(self, x, y, w, h, grey=40):
        for dx in range(-1, w + 1):
            for yy in (y - 1, y + h):
                i = (yy * self.width + x + dx) * 3
                self.rgb[i:i + 3] = bytes((grey, grey, grey))
        for dy in range(-1, h + 1):
            for xx in (x - 1, x + w):
                i = ((y + dy) * self.width + xx) * 3
                self.rgb[i:i + 3] = bytes((grey, grey, grey))

    def label(self, x, y, text):
        self.labels.append((x, y, text))

    def save(self, path):
        args = ["-size", "%dx%d" % (self.width, self.height), "-depth", "8", "rgb:-", "-fill", "#c0c0c0",
                "-pointsize", "13", "-gravity", "NorthWest"]
        for x, y, text in self.labels:
            args += ["-annotate", "+%d+%d" % (x, y), text]
        convert(args + [path], bytes(self.rgb))


def load_icon(sprites_dir, cache, entry):
    sheet, x, y, w, h = entry
    if sheet not in cache:
        cache[sheet] = Sprite.load(os.path.join(sprites_dir, sheet + ".spr"))
    return cache[sheet].grey((x, y, w, h))


def contact_sheet(path, title, rows, groups, columns):
    """rows: [(label, [icon rows per group])]; groups: column titles; every icon has the same size."""
    w, h = len(rows[0][1][0][0]), len(rows[0][1][0])
    pad, label_h, head = 12, 18, 40
    block_w = len(groups) * (w + pad) + pad
    per_col = (len(rows) + columns - 1) // columns
    canvas = Canvas(columns * block_w + pad, head + per_col * (h + label_h + pad) + pad)
    canvas.label(pad, 6, title)
    for i, (label, icons) in enumerate(rows):
        bx = pad + (i // per_col) * block_w
        by = head + (i % per_col) * (h + label_h + pad)
        canvas.label(bx, by, label)
        for g, icon in enumerate(icons):
            x, y = bx + g * (w + pad), by + label_h
            canvas.frame(x, y, w, h)
            # icons that are not selected: at 192 / 255, as the weapon list draws them
            canvas.blit_grey(x, y, icon, PREVIEW_TINT, 255 if groups[g].endswith("selected") else PREVIEW_NORMAL_ALPHA)
            if i % per_col == 0:
                canvas.label(x, by - 16 if by > head else 22, groups[g])
    canvas.save(path)


def write_review(review, sprites_dir, old_dir, names):
    os.makedirs(review, exist_ok=True)
    cache = {}
    made = []
    for res in sorted(TIERS):
        rows = []
        for name in names:
            entries = parse_weapon_txt(os.path.join(sprites_dir, "weapon_%s.txt" % name))
            rows.append(("weapon_%s" % name, [load_icon(sprites_dir, cache, entries[("weapon", res)]),
                                                 load_icon(sprites_dir, cache, entries[("weapon_s", res)])]))
        w, h = TIERS[res]["cell"]
        path = os.path.join(review, "contact_%d.png" % res)
        contact_sheet(path, "%d set, %dx%d: normal (as drawn in the weapon list, 192/255) and selected, HUD orange"
                      % (res, w, h), rows, ["normal", "selected"], 1 if res >= 1280 else 2 if res == 640 else 3)
        made.append(path)

    if old_dir:
        old_cache, rows = {}, []
        for name in names:
            old = parse_weapon_txt(os.path.join(old_dir, "weapon_%s.txt" % name))
            new = parse_weapon_txt(os.path.join(sprites_dir, "weapon_%s.txt" % name))
            rows.append(("weapon_%s" % name, [load_icon(old_dir, old_cache, old[("weapon", 640)]),
                                                 load_icon(old_dir, old_cache, old[("weapon_s", 640)]),
                                                 load_icon(sprites_dir, cache, new[("weapon", 640)]),
                                                 load_icon(sprites_dir, cache, new[("weapon_s", 640)])]))
        path = os.path.join(review, "old_vs_new_640.png")
        contact_sheet(path, "640 set: old sprites and new ones", rows,
                      ["old normal", "old selected", "new normal", "new selected"], 1)
        made.append(path)
    return made


# ---------------------------------------------------------------------------------------------------------------
# Kill feed icons (d_<name> in sprites/hud.txt; docs/dev/KILLFEED_PORT.md "SBO weapons", sdk/spritesrc/killfeed/README.md)
# ---------------------------------------------------------------------------------------------------------------

# d_<name> -> source. <name> is what the server sends in DeathMsg / SboDeathMsg: the killer's weapon classname
# without "weapon_" (bullets and melee: the inflictor is the player), or the inflictor's classname without "monster_"
# / "func_" (projectiles, mines, func_tank). Source: artwork stem (killfeed-sources makes the PNG from it),
# "tripod:<name>" (that icon's PNG on a tripod), "cut" (a rectangle of an existing HUD sheet, kept pixel for pixel:
# CS 1.6's d_headshot), "drawn" (the PNG is the source itself, drawn by hand; killfeed-sources keeps it) or "=<name>"
# (another entry's picture, a hud.txt line only).
#   flop: mirror (the profile artwork faces left; kill feed icons face the victim, to the right, as Half-Life's)
#   crop: (x, y, w, h) of the artwork to use (in artwork pixels)
#   spr, rect: "cut": the sheet (relative to killfeed-sources --cstrike) and the rectangle (x, y, w, h)
# Only weapons with the owner's artwork get an icon; the others keep their old d_ entry (crossbow / bolt: HL's own
# icons in 320hud1 / 640hud1). The sheet is packed in this order: add new icons at the end.
KILLFEED = [
    ("ben", "benelli", {"flop": True}),
    ("canon", "canon-of-terror", {"flop": True}),
    ("chaingun", "minigun", {"flop": True}),
    ("chainsaw", "chainsaw_sword", {"main_only": True, "flop": True}),
    ("fa18", "fa18", {"flop": True}),
    ("federal", "federal", {"flop": True}),
    ("glauncher", "glauncher", {"flop": True}),
    ("handgrenade", "handgrenade", {}),
    ("k444", "k444", {"flop": True}),
    ("nm33", "nm33-new", {"flop": True}),
    ("pm66", "pm66", {"flop": True}),
    ("rc28", "rc28", {"flop": True}),
    ("rpg", "rpg", {"flop": True}),
    ("spa", "saan", {"flop": True}),
    ("sw", "plasma-sniper", {"flop": True}),
    ("sword", "sword", {"flop": True}),
    ("warhammer", "warhammer", {"flop": True}),
    ("wong", "fists", {"crop": (0, 60, 330, 360), "main_only": True, "flop": True}),
    ("medikit", "medikit", {"cut_red": True}),
    ("tank", "tripod:chaingun", {}),
    # projectiles and placed weapons: the weapon's icon under the inflictor's name
    ("rpg_rocket", "=rpg", {}),
    ("grenade", "=handgrenade", {}),
    ("satchel", "satchel", {"cut_dark": True}),
    ("tripmine", "mine", {}),
    ("snark", "pitbull", {"flop": True}),
    ("tanklaser", "=tank", {}),
    ("headshot", "cut", {"spr": "sprites/640hud1.spr", "rect": (0, 240, 36, 16)}),  # CS 1.6 (cstrike/sprites/hud.txt)
    ("gm", "drawn", {}),  # a Game Master's monster: the artifact pyramid with its eye
]

KF_CELL_H = 16          # height of every d_ icon (Half-Life's d_ icons: 16 in the 320 and 640 sets)
KF_MAX_W = 64           # widest icon
KF_MARGIN = (1, 1)      # empty border (x, y)
KF_SHEET_W = 256
KF_SHEET = "killfeed"   # sprites/killfeed.spr, used by the 320 and 640 sets (the d_ icons are the same size in both)
KF_SETS = (320, 640)
KF_MAX_SHEET = 512      # larger sets (--scale): sheets at most 512 wide and high (HL25's own HUD sprites)
KF_SOURCE_H = 192       # height of the source PNGs (sdk/spritesrc/killfeed/<name>.png)
KF_GAMMA = 0.8          # brightens the anti-aliased edges a little, so thin parts stay visible at 16 pixels


def kf_source_dir(root):
    return os.path.join(root, "sdk", "spritesrc", "killfeed")


def kf_png_from_mask(mask, out, flop):
    """White silhouette on transparency, KF_SOURCE_H high (or narrower if very wide)."""
    convert([mask] + (["-flop"] if flop else []) + [
        "-filter", "Lanczos", "-resize", "%dx%d" % (KF_SOURCE_H * 8, KF_SOURCE_H),
        "(", "+clone", "-fill", "white", "-colorize", "100", ")", "+swap", "-alpha", "off",
        "-compose", "CopyOpacity", "-composite", "-strip", "-define", "png:color-type=6", out])


def kf_tripod(src_png, out):
    """The icon on a tripod (a mounted gun): the gun on top, three legs under its middle."""
    w, h = (int(v) for v in convert([src_png, "-format", "%w %h", "info:"]).split())
    gun_h = KF_SOURCE_H * 55 // 100
    gw = w * gun_h // h
    legs = KF_SOURCE_H - gun_h + 8
    cx, top, bottom, t = gw * 45 // 100, gun_h - 8, KF_SOURCE_H - 1, max(6, KF_SOURCE_H // 24)
    draw = "polygon %d,%d %d,%d %d,%d %d,%d " % (cx - t, top, cx + t, top, cx - legs + t, bottom, cx - legs - t, bottom)
    draw += "polygon %d,%d %d,%d %d,%d %d,%d " % (cx - t, top, cx + t, top, cx + legs + t, bottom, cx + legs - t, bottom)
    draw += "rectangle %d,%d %d,%d" % (cx - t, top, cx + t, bottom)
    convert(["-size", "%dx%d" % (max(gw, cx + legs + t + 1), KF_SOURCE_H), "xc:none",
             "(", src_png, "-resize", "x%d" % gun_h, ")", "-geometry", "+0+0", "-composite",
             "-fill", "white", "-draw", draw, "-trim", "+repage", "-strip", "-define", "png:color-type=6", out])


def cmd_killfeed_sources(args):
    """sdk/spritesrc/killfeed/<name>.png from the artwork (and the drawn .svg sources)."""
    out_dir = kf_source_dir(args.root)
    os.makedirs(out_dir, exist_ok=True)
    work_tmp = tempfile.TemporaryDirectory(prefix="killfeed-")
    work = work_tmp.name
    for name, source, opts in KILLFEED:
        out = os.path.join(out_dir, name + ".png")
        if source.startswith("="):
            continue  # shares another icon's picture (hud.txt entry only)
        if source == "drawn":
            print("%-12s drawn: kept" % name)
            continue
        if source == "cut":
            # pixel for pixel: brightness -> alpha of a white picture (the sheets are drawn additively)
            if not args.cstrike:
                print("%-12s cut: kept (no --cstrike)" % name)
                continue
            spr = Sprite.load(os.path.join(args.cstrike, opts["spr"]))
            x, y, w, h = opts["rect"]
            grey = b"".join(spr.grey((x, y, w, h)))
            convert(["-size", "%dx%d" % (w, h), "-depth", "8", "gray:-", "(", "+clone", "-fill", "white",
                     "-colorize", "100", ")", "+swap", "-alpha", "off", "-compose", "CopyOpacity", "-composite",
                     "-strip", "-define", "png:color-type=6", out], grey)
        elif source.startswith("tripod:"):
            kf_tripod(os.path.join(out_dir, source[7:] + ".png"), out)
        else:
            src = find_art(args.art_dir, source)
            if not src:
                sys.exit("%s: artwork %s missing in %s" % (name, source, args.art_dir))
            if opts.get("crop"):
                x, y, w, h = opts["crop"]
                cropped = os.path.join(work, name + "-crop.png")
                convert([src + "[0]", "-crop", "%dx%d+%d+%d" % (w, h, x, y), "+repage", cropped])
                src = cropped
            mask, size, info = make_mask(src, opts, work, name)
            kf_png_from_mask(mask, out, opts.get("flop"))
        w, h = (int(v) for v in convert([out, "-format", "%w %h", "info:"]).split())
        print("%-12s %-22s %4dx%-4d %s" % (name, source, w, h, os.path.relpath(out)))
    work_tmp.cleanup()


def kf_render(png, scale=1, verbatim=False):
    """The icon at kill feed size x scale: brightness bytes, width, KF_CELL_H * scale high (width fits the shape).
    verbatim: a picture already at kill feed size ("cut"), copied (scale 1) or enlarged."""
    cell_h = KF_CELL_H * scale
    if verbatim:
        size = ["-filter", "Lanczos", "-resize", "%d%%" % (100 * scale)] if scale != 1 else []
        args = [png, "-alpha", "extract"] + size
        w = int(convert(args + ["-format", "%w", "info:"]))
        args += ["-gravity", "center", "-background", "black", "-extent", "%dx%d" % (w, cell_h)]
    else:
        mx, my = KF_MARGIN[0] * scale, KF_MARGIN[1] * scale
        args = [png, "-alpha", "extract", "-filter", "Lanczos",
                "-resize", "%dx%d" % (KF_MAX_W * scale - 2 * mx, cell_h - 2 * my), "-gamma", "%g" % (1 / KF_GAMMA)]
        w = int(convert(args + ["-format", "%w", "info:"])) + 2 * mx
        args += ["-gravity", "center", "-background", "black", "-extent", "%dx%d" % (w, cell_h)]
    data = convert(args + ["-colorspace", "gray", "-depth", "8", "gray:-"])
    if len(data) != w * cell_h:
        sys.exit("%s: unexpected icon size" % png)
    return data, w


def kf_pack(icons, scale=1, sheet_w=None):
    """Rows of KF_CELL_H (x scale), left to right, in table order.
    icons: [(name, bytes, w)] -> (sheet width, height, pixels, {name: rect})"""
    cell_h = KF_CELL_H * scale
    sheet_w, max_h = (sheet_w or KF_SHEET_W, 256) if scale == 1 else (min(KF_SHEET_W * scale, KF_MAX_SHEET), KF_MAX_SHEET)
    rects, x, y = {}, 0, 0
    for name, data, w in icons:
        if x + w > sheet_w:
            x, y = 0, y + cell_h
        rects[name] = (x, y, w, cell_h)
        x += w
    height = max(pow2(y + cell_h), 16)
    if height > max_h:
        sys.exit("kill feed icons do not fit a %dx%d sheet" % (sheet_w, max_h))
    pixels = bytearray(sheet_w * height)
    for name, data, w in icons:
        x, y = rects[name][:2]
        for row in range(cell_h):
            pixels[(y + row) * sheet_w + x:(y + row) * sheet_w + x + w] = data[row * w:(row + 1) * w]
    return sheet_w, height, bytes(pixels), rects


def update_hud_txt(path, placement):
    """d_<name> entries of the KF_SETS in sprites/hud.txt: existing lines rewritten in place, new ones after the
    set's last d_ entry; the count on the first line follows. Every other line is kept byte for byte."""
    with open(path, "rb") as f:
        raw = f.read().decode("latin-1")
    eol = "\r\n" if "\r\n" in raw else "\n"
    lines = raw.split(eol)

    def entry(name, res):
        x, y, w, h = placement[name]
        return "d_%s\t\t%d\t%s\t%d\t%d\t%d\t%d" % (name, res, KF_SHEET, x, y, w, h)

    seen, last_d, out = set(), {}, []
    for line in lines:
        m = ENTRY.match(line)
        if m and m.group(1).startswith("d_") and int(m.group(2)) in KF_SETS:
            key = (m.group(1)[2:], int(m.group(2)))
            if key[0] in placement:
                if key in seen:
                    continue
                seen.add(key)
                line = entry(*key)
            last_d[key[1]] = len(out)
        out.append(line)
    for res in sorted(KF_SETS, reverse=True):  # the later set first, so the earlier set's index stays right
        if res not in last_d:
            sys.exit("%s: no d_ entries of the %d set" % (path, res))
        new = [entry(name, res) for name in placement if (name, res) not in seen]
        out[last_d[res] + 1:last_d[res] + 1] = new
    count = sum(1 for line in out if ENTRY.match(line))
    m = COUNT.match(out[0])
    if not m:
        sys.exit("%s: no sprite count on the first line" % path)
    out[0] = "%s%d%s" % (m.group(1), count, m.group(3))
    with open(path, "wb") as f:
        f.write(eol.join(out).encode("latin-1"))
    return count


def kf_review(path, sprites_dir, names):
    """Every d_ icon of the 640 set as the feed draws it (white and HUD orange, 1x and 4x), old entries included."""
    entries = {}
    with open(os.path.join(sprites_dir, "hud.txt"), "rb") as f:
        for line in f.read().decode("latin-1").splitlines():
            m = ENTRY.match(line)
            if m and m.group(1).startswith("d_") and int(m.group(2)) == 640:
                entries.setdefault(m.group(1), (m.group(3),) + tuple(int(m.group(i)) for i in range(4, 8)))
    cache, rows = {}, []
    for name in sorted(entries):
        rows.append((name, load_icon(sprites_dir, cache, entries[name])))
    scale, pad, label_w = 4, 8, 120
    row_h = KF_CELL_H * scale + pad
    width = label_w + KF_MAX_W + pad + KF_MAX_W * scale + pad + KF_MAX_W * scale + pad
    canvas = Canvas(width, 30 + len(rows) * row_h)
    canvas.label(pad, 6, "d_ icons, 640 set: 1x white | 4x white | 4x HUD orange (new sheet: %s.spr)" % KF_SHEET)
    for i, (name, icon) in enumerate(rows):
        y = 30 + i * row_h
        canvas.label(pad, y + 20, name + (" *" if name[2:] in names else ""))
        big = [bytes(v for v in row for _ in range(scale)) for row in icon for _ in range(scale)]
        canvas.blit_grey(label_w, y, icon, (255, 255, 255))
        canvas.blit_grey(label_w + KF_MAX_W + pad, y, big, (255, 255, 255))
        canvas.blit_grey(label_w + KF_MAX_W + pad + KF_MAX_W * scale + pad, y, big, PREVIEW_TINT)
    canvas.save(path)


def cmd_killfeed(args):
    """sprites/killfeed.spr and the d_ entries of sprites/hud.txt from sdk/spritesrc/killfeed/<name>.png.
    --scale N (a larger sprite set, not shipped yet): only writes <out>/killfeed_<set>.spr and prints its hud.txt
    lines."""
    sprites_dir = os.path.join(args.gamedir, "sprites")
    src_dir = kf_source_dir(args.root)
    scale = args.scale
    if scale < 1:
        sys.exit("--scale: 1 or more")
    if scale > 1 and not (args.out and args.set):
        sys.exit("--scale %d needs --out <dir> and --set <sprite set, e.g. 1280>" % scale)
    icons, alias = [], {}
    for name, source, opts in KILLFEED:
        if source.startswith("="):
            alias[name] = source[1:]
            continue
        png = os.path.join(src_dir, name + ".png")
        if not os.path.isfile(png):
            sys.exit("%s: missing (killfeed-sources makes it)" % png)
        data, w = kf_render(png, scale, verbatim=(source == "cut"))
        icons.append((name, data, w))
    sheet_w, height, pixels, rects = kf_pack(icons, scale)
    for name, target in alias.items():
        rects[name] = rects[target]
    sheet = KF_SHEET if scale == 1 else "%s_%s" % (KF_SHEET, args.set)
    if scale == 1:
        path = os.path.join(sprites_dir, sheet + ".spr")
    else:
        os.makedirs(args.out, exist_ok=True)
        path = os.path.join(args.out, sheet + ".spr")
    size = write_sprite(path, sheet_w, height, pixels)
    print("%s: %dx%d, %d bytes, %d icons" % (os.path.relpath(path), sheet_w, height, size, len(icons)))
    placement = {name: rects[name] for name, _, _ in KILLFEED}
    for name, _, _ in KILLFEED:
        x, y, w, h = placement[name]
        if scale == 1:
            print("  d_%-14s %3d %3d %3d %3d" % (name, x, y, w, h))
        else:
            print("d_%s\t\t%s\t%s\t%d\t%d\t%d\t%d" % (name, args.set, sheet, x, y, w, h))
    if scale > 1:
        return
    count = update_hud_txt(os.path.join(sprites_dir, "hud.txt"), placement)
    print("sprites/hud.txt: %d entries" % count)
    if args.review:
        os.makedirs(args.review, exist_ok=True)
        out = os.path.join(args.review, "killfeed_640.png")
        kf_review(out, sprites_dir, set(placement))
        print("review: %s" % out)


# ---------------------------------------------------------------------------------------------------------------
# Plugin kill feed icons (docs/KILLFEED_ICONS.md): one sheet, sprites/sbo_kf_custom_v<N>.spr, shipped with the
# server plugins (amxmodx-plugins server/syndicate/killfeed_icons/), its rectangles in include/killfeed_icons.inc.
# The sources are sdk/spritesrc/killfeed/<name>.png like the game's icons, made by killfeed-custom-sources from the
# plugins' model meshes (SMD, amxmodx-plugins checkout): a white side silhouette, muzzle to the right.
# ---------------------------------------------------------------------------------------------------------------

# name (the DeathMsg code the plugin gives the kill) -> parts [(SMD path in the plugins checkout, z offset)], view
#   view: (horizontal axis, vertical axis) of the model, "flip": mirror, "level": turn the main direction horizontal
#   (a weapon model lying on its side), "draw": ImageMagick -draw primitives on the source's shape (black: a gap,
#   white: more shape), for the inner detail the artwork-made icons have
KF_CUSTOM = [
    ("quadbarrel", [("server/syndicate/weaponmod/cso_quadbarrel/modelsrc/mesh/w_ref.smd", 0)],
     {"view": (0, 1), "level": True, "draw": [
         # inner detail as d_ben has, in the 192-high source's pixels (about 13 per kill feed pixel)
         "fill black stroke black stroke-width 13 line 225,42 800,74",     # between the stacked barrels
         "fill black stroke black stroke-width 13 line 166,10 150,118",    # stock / receiver break
         "fill black stroke black stroke-width 10 line 770,30 770,116",    # muzzle ends
         "fill black stroke none rectangle 136,121 240,160",               # trigger guard: old outline out ...
         "fill none stroke white stroke-width 13 ellipse 186,118 36,30 0,180",  # ... a solid guard
     ]}),
    ("hhgrenade", [("server/syndicate/custom_weapons/holy_hand_grenade/modelsrc/mesh/w_holygrenade.smd", 0)],
     {"view": (0, 2)}),
    # the level 1 sentry on its base (the plugin puts the turret 16 units up)
    ("sentry", [("server/syndicate/sentryguns/modelsrc/mesh/TFSentry-Base_Template_Biped1.smd", 0),
                ("server/syndicate/sentryguns/modelsrc/mesh/TFSentry-1_Template_Biped1.smd", 16)],
     {"view": (1, 2), "flip": True}),
]
KF_CUSTOM_SHEET = "sbo_kf_custom_v%d"
KF_CUSTOM_RENDER_H = 768    # silhouette height before it is scaled to KF_SOURCE_H
INC_BEGIN = "// BEGIN generated by scripts/gen_hud_weapon_sprites.py killfeed-custom"
INC_END = "// END generated"


def read_smd_triangles(path):
    """The triangles' vertex positions of an SMD reference mesh ((x, y, z) * 3 per triangle)."""
    tris, section, cur = [], None, []
    with open(path, encoding="latin-1") as f:
        for line in f:
            t = line.split()
            if not t:
                continue
            if t[0] in ("nodes", "skeleton", "triangles"):
                section = t[0]
                continue
            if t[0] == "end":
                section = None
                continue
            if section != "triangles":
                continue
            if len(t) >= 9 and re.match(r"^-?\d", t[1]):
                cur.append(tuple(float(v) for v in t[1:4]))
                if len(cur) == 3:
                    tris.append(cur)
                    cur = []
            else:
                cur = []    # material line
    return tris


def silhouette(tris, view, flip, height):
    """Filled orthographic silhouette of the triangles: (bytes 0 / 255, width, height)."""
    a, b = view
    sx = -1.0 if flip else 1.0
    pts = [(sx * p[a], p[b]) for t in tris for p in t]
    x0, x1 = min(p[0] for p in pts), max(p[0] for p in pts)
    y0, y1 = min(p[1] for p in pts), max(p[1] for p in pts)
    scale = (height - 4) / max(y1 - y0, 1e-6)
    width = int((x1 - x0) * scale) + 4
    img = bytearray(width * height)
    for t in tris:
        (ax, ay), (bx, by), (cx, cy) = [((sx * p[a] - x0) * scale + 2, (y1 - p[b]) * scale + 2) for p in t]
        area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
        if abs(area) < 1e-9:
            continue
        for y in range(max(0, int(min(ay, by, cy))), min(height - 1, int(max(ay, by, cy)) + 1) + 1):
            yy = y + 0.5
            for x in range(max(0, int(min(ax, bx, cx))), min(width - 1, int(max(ax, bx, cx)) + 1) + 1):
                xx = x + 0.5
                w0 = ((bx - xx) * (cy - yy) - (by - yy) * (cx - xx)) / area
                w1 = ((cx - xx) * (ay - yy) - (cy - yy) * (ax - xx)) / area
                if w0 >= -0.02 and w1 >= -0.02 and 1 - w0 - w1 >= -0.02:
                    img[y * width + x] = 255
    return bytes(img), width, height


def level_triangles(tris, view):
    """Turn the triangles about the view's normal so their main direction (least squares) is horizontal."""
    a, b = view
    pts = [p for t in tris for p in t]
    ma, mb = sum(p[a] for p in pts) / len(pts), sum(p[b] for p in pts) / len(pts)
    saa = sum((p[a] - ma) ** 2 for p in pts)
    sbb = sum((p[b] - mb) ** 2 for p in pts)
    sab = sum((p[a] - ma) * (p[b] - mb) for p in pts)
    ang = -0.5 * math.atan2(2 * sab, saa - sbb)
    c, s = math.cos(ang), math.sin(ang)

    def turn(p):
        q = list(p)
        q[a], q[b] = c * p[a] - s * p[b], s * p[a] + c * p[b]
        return tuple(q)
    return [[turn(p) for p in t] for t in tris]


def cmd_killfeed_custom_sources(args):
    """sdk/spritesrc/killfeed/<name>.png of the plugin icons from the plugins' model meshes."""
    out_dir = kf_source_dir(args.root)
    for name, parts, opts in KF_CUSTOM:
        tris = []
        for rel, dz in parts:
            path = os.path.join(args.plugins, rel)
            if not os.path.isfile(path):
                sys.exit("%s: %s missing" % (name, path))
            tris += [[(p[0], p[1], p[2] + dz) for p in t] for t in read_smd_triangles(path)]
        if opts.get("level"):
            tris = level_triangles(tris, opts["view"])
        data, w, h = silhouette(tris, opts["view"], opts.get("flip"), KF_CUSTOM_RENDER_H)
        out = os.path.join(out_dir, name + ".png")
        draw = []
        for prim in opts.get("draw", []):
            draw += ["-draw", prim]
        convert(["-size", "%dx%d" % (w, h), "-depth", "8", "gray:-", "-filter", "Lanczos", "-resize",
                 "x%d" % KF_SOURCE_H, "-trim", "+repage"] + draw + ["(", "+clone", "-fill", "white", "-colorize", "100", ")", "+swap",
                 "-alpha", "off", "-compose", "CopyOpacity", "-composite", "-trim", "+repage", "-strip",
                 "-define", "png:color-type=6", out], data)
        w, h = (int(v) for v in convert([out, "-format", "%w %h", "info:"]).split())
        print("%-12s %4dx%-4d %s" % (name, w, h, os.path.relpath(out)))


def cmd_killfeed_custom(args):
    """<out>/sbo_kf_custom_v<N>.spr from the KF_CUSTOM sources, and its rectangles (printed, and written into the
    generated block of --inc)."""
    src_dir = kf_source_dir(args.root)
    icons = []
    for name, _, _ in KF_CUSTOM:
        png = os.path.join(src_dir, name + ".png")
        if not os.path.isfile(png):
            sys.exit("%s: missing (killfeed-custom-sources makes it)" % png)
        data, w = kf_render(png)
        icons.append((name, data, w))
    sheet_w, height, pixels, rects = kf_pack(icons, sheet_w=min(pow2(sum(w for _, _, w in icons)), KF_SHEET_W))
    sheet = KF_CUSTOM_SHEET % args.version
    os.makedirs(args.out, exist_ok=True)
    path = os.path.join(args.out, sheet + ".spr")
    size = write_sprite(path, sheet_w, height, pixels)
    print("%s: %dx%d, %d bytes, %d icons" % (path, sheet_w, height, size, len(icons)))
    block = [INC_BEGIN, "#define KILLFEED_ICONS_SHEET \"sprites/%s.spr\"" % sheet]
    for name, _, _ in KF_CUSTOM:
        x, y, w, h = rects[name]
        block.append("#define KF_ICON_%s %d, %d, %d, %d" % (name.upper(), x, y, w, h))
    block.append(INC_END)
    print("\n".join(block))
    if args.inc:
        with open(args.inc, "rb") as f:
            raw = f.read().decode("latin-1")
        eol = "\r\n" if "\r\n" in raw else "\n"
        m = re.search(re.escape(INC_BEGIN) + ".*?" + re.escape(INC_END), raw, re.S)
        if not m:
            sys.exit("%s: no generated block (%s ... %s)" % (args.inc, INC_BEGIN, INC_END))
        raw = raw[:m.start()] + eol.join(block) + raw[m.end():]
        with open(args.inc, "wb") as f:
            f.write(raw.encode("latin-1"))
        print("%s: rectangles written" % args.inc)
    if args.review:
        os.makedirs(args.review, exist_ok=True)
        rows = [(name, [data[r * w:(r + 1) * w] for r in range(KF_CELL_H)]) for name, data, w in icons]
        scale, pad, label_w = 4, 8, 120
        row_h = KF_CELL_H * scale + pad
        canvas = Canvas(label_w + KF_MAX_W + pad + 2 * (KF_MAX_W * scale + pad), 30 + len(rows) * row_h)
        canvas.label(pad, 6, "%s.spr: 1x white | 4x white | 4x HUD orange" % sheet)
        for i, (name, icon) in enumerate(rows):
            y = 30 + i * row_h
            canvas.label(pad, y + 20, name)
            big = [bytes(v for v in row for _ in range(scale)) for row in icon for _ in range(scale)]
            canvas.blit_grey(label_w, y, icon, (255, 255, 255))
            canvas.blit_grey(label_w + KF_MAX_W + pad, y, big, (255, 255, 255))
            canvas.blit_grey(label_w + KF_MAX_W + pad + KF_MAX_W * scale + pad, y, big, PREVIEW_TINT)
        out = os.path.join(args.review, sheet + ".png")
        canvas.save(out)
        print("review: %s" % out)


# ---------------------------------------------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------------------------------------------

def cmd_generate(args):
    sprites_dir = os.path.join(args.gamedir, "sprites")
    if not os.path.isdir(sprites_dir):
        sys.exit("%s: no sprites folder" % args.gamedir)
    if not 0.0 < args.normal_level <= 1.0:
        sys.exit("--normal-level: 0 < level <= 1")

    todo, missing = [], []
    for name, stem, opts in WEAPONS:
        src = find_art(args.art_dir, stem)
        txt = os.path.join(sprites_dir, "weapon_%s.txt" % name)
        if not os.path.isfile(txt):
            sys.exit("%s: missing" % txt)
        if src:
            todo.append((name, src, opts))
        else:
            missing.append((name, stem))
    if missing and not args.partial:
        # the sheets are packed in the order of WEAPONS: a missing file would drop that weapon's icons
        sys.exit("artwork missing in %s: %s\n(--partial generates without them: their weapon_*.txt keep pointing at "
                 "the previous sheets, which this run may overwrite)"
                 % (args.art_dir, ", ".join("%s (weapon_%s)" % (stem, name) for name, stem in missing)))

    work_tmp = None
    work = args.work
    if work:
        os.makedirs(work, exist_ok=True)
    else:
        work_tmp = tempfile.TemporaryDirectory(prefix="hud-sprites-")
        work = work_tmp.name

    masks = []
    for name, src, opts in todo:
        mask, size, info = make_mask(src, opts, work, name)
        masks.append((name, mask))
        print("weapon_%-12s %-22s shape %4dx%-4d %s%s" % (
            name, os.path.basename(src), size[0], size[1], "alpha" if info["alpha"] else "brightness",
            ", removed shapes of %s px" % "/".join(str(a) for a in info["removed"]) if info["removed"] else "")
            + (", gaps of %s px" % "/".join(str(a) for a in info["cut"]) if info["cut"] else ""))

    lut_normal = bytes(min(255, int(v * args.normal_level + 0.5)) for v in range(256))
    placement = {name: {} for name, _ in masks}
    total, files = 0, []
    for res in sorted(TIERS):
        icons = [(name, render_icon(mask, TIERS[res])) for name, mask in masks]
        for index, width, height, pixels, rects in pack(res, icons):
            sheet = SHEET_NAME.format(res=res, index=index)
            # selected: the shape at full brightness; normal: the same icon, dimmer (as the old sheets)
            for suffix, data in (("", pixels.translate(lut_normal)), ("_s", pixels)):
                path = os.path.join(sprites_dir, sheet + suffix + ".spr")
                size = write_sprite(path, width, height, data)
                total += size
                files.append((path, width, height, size))
            for name, rect in rects.items():
                placement[name][res] = (sheet, rect)

    print()
    for path, width, height, size in files:
        print("%-28s %4dx%-4d %7d bytes" % (os.path.basename(path), width, height, size))
    print("%d sprite files, %d bytes" % (len(files), total))

    for name, _ in masks:
        txt = os.path.join(sprites_dir, "weapon_%s.txt" % name)
        print("\n%s" % os.path.relpath(txt))
        for line in update_weapon_txt(txt, placement[name]):
            print("  " + line.expandtabs(8))

    if args.review:
        print()
        for path in write_review(args.review, sprites_dir, args.old_sprites, [name for name, _ in masks]):
            print("review: %s" % path)
    if work_tmp:
        work_tmp.cleanup()


def cmd_info(args):
    for path in args.files:
        spr = Sprite.load(path)
        print("%s: %d bytes, %s%s" % (path, os.path.getsize(path), spr.describe(),
                                      "" if spr.trailing == 0 else ", more frames or data follow"))


def cmd_dump(args):
    spr = Sprite.load(args.file)
    convert(["-size", "%dx%d" % (spr.width, spr.height), "-depth", "8", "rgb:-", args.out], spr.rgb())


def main():
    root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    parser = argparse.ArgumentParser(description="HUD weapon icons from profile artwork (docs/HUD_SPRITES.md)")
    sub = parser.add_subparsers(dest="command", required=True)

    gen = sub.add_parser("generate", help="write the sprite sheets and update sprites/weapon_*.txt")
    gen.add_argument("art_dir", help="folder with the artwork (<name>.psd, .png, ...)")
    gen.add_argument("--gamedir", default=os.path.join(root, "gamedir", "syndicate"))
    gen.add_argument("--review", help="folder for the contact sheets (PNG)")
    gen.add_argument("--old-sprites", help="folder with the previous weapon_*.txt and sheets, for old_vs_new_640.png")
    gen.add_argument("--normal-level", type=float, default=0.45,
                     help="brightness of the normal icon, 1 = the selected one (default %(default)s)")
    gen.add_argument("--work", help="keep the intermediate masks in this folder")
    gen.add_argument("--partial", action="store_true", help="generate although artwork is missing")
    gen.set_defaults(func=cmd_generate)

    kfs = sub.add_parser("killfeed-sources", help="make sdk/spritesrc/killfeed/<name>.png from the artwork")
    kfs.add_argument("art_dir", help="folder with the artwork (<name>.psd, .png, ...)")
    kfs.add_argument("--cstrike", help="Counter-Strike's cstrike folder, for the \"cut\" icons (d_headshot)")
    kfs.set_defaults(func=cmd_killfeed_sources, root=root)

    kf = sub.add_parser("killfeed", help="write sprites/killfeed.spr and the d_ entries of sprites/hud.txt")
    kf.add_argument("--gamedir", default=os.path.join(root, "gamedir", "syndicate"))
    kf.add_argument("--review", help="folder for the contact sheet (PNG)")
    kf.add_argument("--scale", type=int, default=1,
                    help="larger sprite set: 2 = HL25's 1280 set, 3 = its 2560 set (not shipped; needs --out, --set)")
    kf.add_argument("--set", help="the sprite set's number, for the sheet name and the printed hud.txt lines")
    kf.add_argument("--out", help="folder for the larger set's sheet")
    kf.set_defaults(func=cmd_killfeed, root=root)

    kcs = sub.add_parser("killfeed-custom-sources",
                         help="make sdk/spritesrc/killfeed/<name>.png of the plugin icons from their model meshes")
    kcs.add_argument("plugins", help="amxmodx-plugins checkout (the SMD sources)")
    kcs.set_defaults(func=cmd_killfeed_custom_sources, root=root)

    kc = sub.add_parser("killfeed-custom", help="write the plugin icons' sheet sbo_kf_custom_v<N>.spr")
    kc.add_argument("--version", type=int, required=True, help="sheet version: a new number for every change")
    kc.add_argument("--out", required=True, help="folder for the sheet (the plugins' killfeed_icons/sprites)")
    kc.add_argument("--inc", help="include/killfeed_icons.inc: its generated block gets the rectangles")
    kc.add_argument("--review", help="folder for the contact sheet (PNG)")
    kc.set_defaults(func=cmd_killfeed_custom, root=root)

    info = sub.add_parser("info", help="print the header of sprite files")
    info.add_argument("files", nargs="+")
    info.set_defaults(func=cmd_info)

    dump = sub.add_parser("dump", help="first frame of a sprite as an image")
    dump.add_argument("file")
    dump.add_argument("out")
    dump.set_defaults(func=cmd_dump)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
