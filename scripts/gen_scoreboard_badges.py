#!/usr/bin/env python3
"""Animated scoreboard badges: frame sheets from an animated picture.

The scoreboard draws a badge after some players' names (BADGES in src/cl_dll/vgui2/score_panel.cpp,
docs/dev/SCOREBOARD_PORT.md "Scoreboard badges"). A still badge is one TGA per size (the developer icon,
scripts/gen_dev_icon.py); an animated one is one sheet per size made here: every frame of the source, in order, left to
right and top to bottom, COLUMNS frames per row, each frame SIZE x SIZE pixels with no gap (the art keeps its own
transparent margin). The scoreboard picks the frame from the clock and draws that part of the sheet.

Steps (deterministic: same source, same bytes):
  1. the source's frames, coalesced (ImageMagick), and its frame time; every frame must have the same time;
  2. each frame downscaled (Lanczos) to 16, 24, 32 and 64 pixels;
  3. ui/gfx/<name><size>.tga: the sheet (32-bit, uncompressed, alpha, as the other ui/gfx TGAs);
  4. --preview <dir>: <dir>/<name>_sizes.png, the first frames at each size on the scoreboard's background color.

Prints each badge's frame count and frame time: the BADGES row in score_panel.cpp must use the same numbers.

Needs Python 3 and ImageMagick 6 (`convert`, `identify`) with WebP support.

Usage:
  scripts/gen_scoreboard_badges.py                 # the sheets
  scripts/gen_scoreboard_badges.py --preview DIR   # also the preview sheets
"""

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gen_dev_icon import write_tga  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(ROOT, "sdk", "spritesrc", "badges")
OUT_DIR = os.path.join(ROOT, "gamedir", "syndicate", "ui", "gfx")
SIZES = (16, 24, 32, 64)
COLUMNS = 4  # frames per sheet row (score_panel.cpp: BADGE_SHEET_COLUMNS)

# name: the source in sdk/spritesrc/badges/ (the game files are ui/gfx/badge_<name><size>.tga)
BADGES = {
    "boxyt": "boxyt.webp",
}


def run(args):
    return subprocess.run(args, stdout=subprocess.PIPE, check=True).stdout


def frame_time_ms(src):
    delays = set(run(["identify", "-format", "%T\n", src]).decode().split())
    if len(delays) != 1:
        sys.exit(f"{src}: frames have different times ({sorted(delays)}); the scoreboard plays one frame time")
    return int(delays.pop()) * 10  # ImageMagick: 1/100 s


def frames_at(src, size):
    raw = run(["convert", src, "-coalesce", "-filter", "Lanczos", "-resize", f"{size}x{size}!",
               "-depth", "8", "rgba:-"])
    frame = size * size * 4
    if not raw or len(raw) % frame:
        sys.exit(f"unexpected size reading {src}")
    return [raw[i:i + frame] for i in range(0, len(raw), frame)]


def make_sheet(frames, size):
    rows = (len(frames) + COLUMNS - 1) // COLUMNS
    w, h = COLUMNS * size, rows * size
    sheet = bytearray(w * h * 4)
    for n, frame in enumerate(frames):
        fx, fy = (n % COLUMNS) * size, (n // COLUMNS) * size
        for y in range(size):
            row = frame[y * size * 4:(y + 1) * size * 4]
            i = ((fy + y) * w + fx) * 4
            sheet[i:i + size * 4] = row
    return w, h, bytes(sheet)


def make_preview(name, out_dir):
    """The first four frames of each sheet, at their own size, on the scoreboard's dark background."""
    args = ["convert", "-size", "760x80", "xc:#1e1e1e"]
    x = 8
    for size in SIZES:
        sheet = os.path.join(OUT_DIR, f"badge_{name}{size}.tga")
        for n in range(4):
            args += ["(", sheet, "-crop", f"{size}x{size}+{n * size}+0", "+repage", ")",
                     "-geometry", f"+{x}+8", "-composite"]
            x += size + 4
        x += 12
    out = os.path.join(out_dir, f"{name}_sizes.png")
    run(args + [out])
    print(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--preview", metavar="DIR", help="also write <DIR>/<name>_sizes.png")
    a = ap.parse_args()
    for name, source in BADGES.items():
        src = os.path.join(SRC_DIR, source)
        ms = frame_time_ms(src)
        count = None
        for size in SIZES:
            frames = frames_at(src, size)
            count = len(frames)
            w, h, sheet = make_sheet(frames, size)
            path = os.path.join(OUT_DIR, f"badge_{name}{size}.tga")
            write_tga(path, w, h, sheet)
            print(f"{os.path.relpath(path, ROOT)}  {w}x{h}")
        print(f"{name}: {count} frames, {ms} ms per frame")
        if a.preview:
            os.makedirs(a.preview, exist_ok=True)
            make_preview(name, a.preview)


if __name__ == "__main__":
    main()
