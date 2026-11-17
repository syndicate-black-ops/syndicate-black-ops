# Example map: co_example_worldtext

World text. A room with a text on each wall, the floor and the ceiling, each with the angles that face its reader, every font, a centered and an uncentered text with their origins marked, and a text a button changes. Every setup has a label on the wall next to it and an INFO button that opens a box with the entities and keys it uses. Open the `.map` in your editor to see how it is built.

SBO docs: World text.

## Entities

`point_worldtext`, `trigger_changevalue`

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_example_worldtext.map` | your editor (Valve 220 format) | the map source |

## Textures

The worldspawn `wad` key is `halflife.wad;anzymut.wad;roux.wad;co_escape.wad;sbo_tools.wad`. The map's textures come from `anzymut.wad`, `roux.wad` and `co_escape.wad` in `syndicate/`; `halflife.wad` (in `valve/`) only gives the tool textures `ORIGIN`, `AAATRIGGER` and `sky`.

## Compiling

The map compiles with ReSDHLT (https://github.com/metita/ReSDHLT) or SDHLT, as the docs page Compiling shows:

```
sdHLCSG -chart co_example_worldtext
sdHLBSP -chart co_example_worldtext
sdHLVIS -chart co_example_worldtext
sdHLRAD -bounce 2 -chart co_example_worldtext
```

Worldspawn sets `MaxRange` `100000`.
