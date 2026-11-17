# Example map: co_example_locus

Locus keys. The six setups of the Locus guide, with the same names: an explosion where you stand, one above your head, one where you last stepped in, a button that only heals you when you are hurt, a door that opens after five kills and a beam that follows you. The explosions of setups 1 to 3 have **No Damage** on. Every setup has a label on the wall next to it and an INFO button that opens a box with the entities and keys it uses. Open the `.map` in your editor to see how it is built.

SBO docs: Locus guide.

## Entities

`env_explosion` with **Position** `!activator`, `calc_position`, `locus_variable`, `trigger_multiple`, `trigger_compare`, `game_player_hurt`, `game_number`, `calc_eval_number`, `monster_zombie` (TriggerTarget), `func_door`, `locus_beam`, `info_target`

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_example_locus.map` | your editor (Valve 220 format) | the map source |

## Textures

The worldspawn `wad` key is `halflife.wad;anzymut.wad;roux.wad;co_escape.wad;sbo_tools.wad`. The map's textures come from `anzymut.wad`, `roux.wad` and `co_escape.wad` in `syndicate/`; `halflife.wad` (in `valve/`) only gives the tool textures `ORIGIN`, `AAATRIGGER` and `sky`.

## Compiling

The map compiles with ReSDHLT (https://github.com/metita/ReSDHLT) or SDHLT, as the docs page Compiling shows:

```
sdHLCSG -chart co_example_locus
sdHLBSP -chart co_example_locus
sdHLVIS -chart co_example_locus
sdHLRAD -bounce 2 -chart co_example_locus
```

Worldspawn sets `MaxRange` `100000`.
