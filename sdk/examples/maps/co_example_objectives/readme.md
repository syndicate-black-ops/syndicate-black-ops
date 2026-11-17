# Example map: co_example_objectives

Objectives. A three-step mission: turn on the power, take the keycard, open the security door. The objectives show in the panel at the top left, and the mission ends 5 seconds after the last one. Every setup has a label on the wall next to it and an INFO button that opens a box with the entities and keys it uses. Open the `.map` in your editor to see how it is built.

SBO docs: trigger_objective.

## Entities

`trigger_objective` (3), `item_pickup`, `player_hasinventory` as a button **Master**, `func_door`, `multi_manager`, `game_text`, `game_end`

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_example_objectives.map` | your editor (Valve 220 format) | the map source |

## Textures

The worldspawn `wad` key is `halflife.wad;anzymut.wad;roux.wad;co_escape.wad;sbo_tools.wad`. The map's textures come from `anzymut.wad`, `roux.wad` and `co_escape.wad` in `syndicate/`; `halflife.wad` (in `valve/`) only gives the tool textures `ORIGIN`, `AAATRIGGER` and `sky`.

## Compiling

The map compiles with ReSDHLT (https://github.com/metita/ReSDHLT) or SDHLT, as the docs page Compiling shows:

```
sdHLCSG -chart co_example_objectives
sdHLBSP -chart co_example_objectives
sdHLVIS -chart co_example_objectives
sdHLRAD -bounce 2 -chart co_example_objectives
```

Worldspawn sets `MaxRange` `100000`.
