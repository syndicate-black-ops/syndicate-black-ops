# Example map: co_example_calc

Calc entities. The five setups of the Calc entities guide: an explosion 200 units in front of your eyes, a room that pushes you away from its center, a speed trap, a window that gets more solid as you get hurt, and an explosion where you look. Every setup has a label on the wall next to it and an INFO button that opens a box with the entities and keys it uses. Open the `.map` in your editor to see how it is built.

SBO docs: Calc entities guide.

## Entities

`calc_subvelocity`, `calc_position`, `env_explosion`, `calc_velocity_path`, `info_target`, `trigger_push`, `calc_numfromvec`, `trigger_compare`, `game_text`, `calc_ratio`, `env_render`, `func_wall`, `trigger_multiple`, `game_player_hurt`

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_example_calc.map` | your editor (Valve 220 format) | the map source |

## Textures

The worldspawn `wad` key is `halflife.wad;anzymut.wad;roux.wad;co_escape.wad;sbo_tools.wad`. The map's textures come from `anzymut.wad`, `roux.wad` and `co_escape.wad` in `syndicate/`; `halflife.wad` (in `valve/`) only gives the tool textures `ORIGIN`, `AAATRIGGER` and `sky`.

## Compiling

The map compiles with ReSDHLT (https://github.com/metita/ReSDHLT) or SDHLT, as the docs page Compiling shows:

```
sdHLCSG -chart co_example_calc
sdHLBSP -chart co_example_calc
sdHLVIS -chart co_example_calc
sdHLRAD -bounce 2 -chart co_example_calc
```

Worldspawn sets `MaxRange` `100000`.
