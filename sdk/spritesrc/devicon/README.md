# Developer scoreboard icon (`ui/gfx/dev_icon<size>.tga`)

The icon after the developer's name on the scoreboard: the robot's head
from the owner's Steam avatar.

- `avatar.png`: the owner's avatar (1024x1024), the source artwork.
- `face.png`: the head cut out of it, leveled, with a dark rim, on a 256x256 transparent canvas (generated).

The game files are `gamedir/syndicate/ui/gfx/dev_icon16.tga`, `dev_icon24.tga`, `dev_icon32.tga` and
`dev_icon64.tga` (32-bit, uncompressed, alpha). 32 and 64 are downscaled from `face.png`; 16 and 24 are the same head
traced pixel by pixel for that size (`PIXEL_ART` in the script), because a downscale turns to grey at those sizes.
The scoreboard draws the largest one that fits the row at its own size.

## Regenerate

Needs Python 3 and ImageMagick 6 (`convert`).

```
scripts/gen_dev_icon.py                  # face.png + the four TGAs
scripts/gen_dev_icon.py --preview <dir>  # also <dir>/dev_icon_sizes.png (16, 24, 32, 48 px on the board's colors)
```
