# Example map: co_example_colorcorrection

Color correction. A button turns a black and white look on for the whole map, and the two side rooms each have their own look, sepia and night, that fades in as you walk in. The same color wall in every room shows the change. Every setup has a label on the wall next to it and an INFO button that opens a box with the entities and keys it uses. Open the `.map` in your editor to see how it is built.

SBO docs: Color correction.

## Entities

`color_correction`, `color_correction_volume`

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_example_colorcorrection.map` | your editor (Valve 220 format) | the map source |

## Textures

The worldspawn `wad` key is `halflife.wad;anzymut.wad;roux.wad;co_escape.wad;sbo_tools.wad`. The map's textures come from `anzymut.wad`, `roux.wad` and `co_escape.wad` in `syndicate/`; `halflife.wad` (in `valve/`) only gives the tool textures `ORIGIN`, `AAATRIGGER` and `sky`.

## Compiling

The map compiles with ReSDHLT (https://github.com/metita/ReSDHLT) or SDHLT, as the docs page Compiling shows:

```
sdHLCSG -chart co_example_colorcorrection
sdHLBSP -chart co_example_colorcorrection
sdHLVIS -chart co_example_colorcorrection
sdHLRAD -bounce 2 -chart co_example_colorcorrection
```

Worldspawn sets `MaxRange` `100000`.
