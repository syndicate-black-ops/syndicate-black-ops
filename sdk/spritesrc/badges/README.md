# Animated scoreboard badges (`ui/gfx/badge_<name><size>.tga`)

Badges after some players' names on the scoreboard. Each source
here is an animated picture; the game files are one frame sheet per size (16, 24, 32, 64): every frame in order, 4 per
row, no gap (32-bit, uncompressed, alpha). The scoreboard draws the largest sheet that fits the row and shows the frame
for the current moment.

- `boxyt.webp`: Boxyt's badge, a breakdancer in a round frame (128x128, 15 frames, 80 ms each).

## Regenerate

Needs Python 3 and ImageMagick 6 (`convert`, `identify`) with WebP support.

```
scripts/gen_scoreboard_badges.py                  # the sheets
scripts/gen_scoreboard_badges.py --preview <dir>  # also <dir>/<name>_sizes.png (first frames at each size)
```

A new badge: put its source here, add it to `BADGES` in the script, run it, then add one line to `BADGES` in
`src/cl_dll/vgui2/score_panel.cpp` with the frame count and frame time the script prints.
