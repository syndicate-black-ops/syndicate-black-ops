# Example map: co_example_worldscreens

World screens. A flat screen on a wall with the game's example layout: HELLO shows a text and writes a line on the screen, LIGHT switches a lamp. Every setup has a label on the wall next to it and an INFO button that opens a box with the entities and keys it uses. Open the `.map` in your editor to see how it is built.

SBO docs: World screens.

## Entities

`vgui_screen` (layout `example_screen`, keys `~hello` and `~light`), `vgui_control_proxy`, `game_text`, `light`, `env_sprite`

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_example_worldscreens.map` | your editor (Valve 220 format) | the map source |

## Textures

The worldspawn `wad` key is `halflife.wad;anzymut.wad;roux.wad;co_escape.wad;sbo_tools.wad`. The map's textures come from `anzymut.wad`, `roux.wad` and `co_escape.wad` in `syndicate/`; `halflife.wad` (in `valve/`) only gives the tool textures `ORIGIN`, `AAATRIGGER` and `sky`.

## Compiling

The map compiles with ReSDHLT (https://github.com/metita/ReSDHLT) or SDHLT, as the docs page Compiling shows:

```
sdHLCSG -chart co_example_worldscreens
sdHLBSP -chart co_example_worldscreens
sdHLVIS -chart co_example_worldscreens
sdHLRAD -bounce 2 -chart co_example_worldscreens
```

Worldspawn sets `MaxRange` `100000`.
