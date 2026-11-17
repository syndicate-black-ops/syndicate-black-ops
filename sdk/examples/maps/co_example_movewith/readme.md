# Example map: co_example_movewith

A train with a door at a station platform. A `func_train` drives between two stops. Its door, a roof lamp, two headlights and a sign ride with it because each has **Move with** set to the train, and the door opens at each stop. Every setup has a label on the wall next to it and an INFO button that opens a box with the entities and keys it uses. Open the `.map` in your editor to see how it is built.

SBO docs: Moving objects.

The full Move With map, `co_example_movewith_full`, is in `_full/`: a small city with a train, a lift, a crane, a drawbridge, a boat, a carousel and more, and a test wing whose rooms each show one tricky case. Its `readme.md` lists every place.

## Entities

`func_train`, `path_corner` (Fire On Pass, Wait here), `func_door`, `env_sprite` and `point_worldtext` with **Move with** `train_main`, `multi_manager`, `trigger_auto`

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_example_movewith.map` | your editor (Valve 220 format) | the map source |

## Textures

The worldspawn `wad` key is `halflife.wad;anzymut.wad;roux.wad;co_escape.wad;sbo_tools.wad`. The map's textures come from `anzymut.wad`, `roux.wad` and `co_escape.wad` in `syndicate/`; `halflife.wad` (in `valve/`) only gives the tool textures `ORIGIN`, `AAATRIGGER` and `sky`.

## Compiling

The map compiles with ReSDHLT (https://github.com/metita/ReSDHLT) or SDHLT, as the docs page Compiling shows:

```
sdHLCSG -chart co_example_movewith
sdHLBSP -chart co_example_movewith
sdHLVIS -chart co_example_movewith
sdHLRAD -bounce 2 -chart co_example_movewith
```

Worldspawn sets `MaxRange` `100000`.
