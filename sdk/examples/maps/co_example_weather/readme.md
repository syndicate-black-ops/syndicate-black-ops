# Example map: co_example_weather

Weather. An open yard: slanted rain in the west half, snow in the east half and a fog zone at the north wall. STORM and CALM change the rain while the map runs. Every setup has a label on the wall next to it and an INFO button that opens a box with the entities and keys it uses. Open the `.map` in your editor to see how it is built.

SBO docs: Weather.

## Entities

`func_rainvolume` (Wind X / Y), `func_snowvolume`, `env_fogzone`, `trigger_changevalue`, `light_environment`

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_example_weather.map` | your editor (Valve 220 format) | the map source |

## Textures

The worldspawn `wad` key is `halflife.wad;anzymut.wad;roux.wad;co_escape.wad;sbo_tools.wad`. The map's textures come from `anzymut.wad`, `roux.wad` and `co_escape.wad` in `syndicate/`; `halflife.wad` (in `valve/`) only gives the tool textures `ORIGIN`, `AAATRIGGER` and `sky`.

## Compiling

The map compiles with ReSDHLT (https://github.com/metita/ReSDHLT) or SDHLT, as the docs page Compiling shows:

```
sdHLCSG -chart co_example_weather
sdHLBSP -chart co_example_weather
sdHLVIS -chart co_example_weather
sdHLRAD -bounce 2 -chart co_example_weather
```

Worldspawn sets `MaxRange` `100000`.
