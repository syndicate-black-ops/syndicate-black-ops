# Example map: co_example_aliases

Aliases. One button fires `*which`, an alias that points to the red lamp or the blue lamp, and a second button switches the alias. Every setup has a label on the wall next to it and an INFO button that opens a box with the entities and keys it uses. Open the `.map` in your editor to see how it is built.

SBO docs: Entity triggering.

## Entities

`info_alias`, `light` and `env_sprite` with the same name, `point_worldtext` (Start Off), `multi_manager`

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_example_aliases.map` | your editor (Valve 220 format) | the map source |

## Textures

The worldspawn `wad` key is `halflife.wad;anzymut.wad;roux.wad;co_escape.wad;sbo_tools.wad`. The map's textures come from `anzymut.wad`, `roux.wad` and `co_escape.wad` in `syndicate/`; `halflife.wad` (in `valve/`) only gives the tool textures `ORIGIN`, `AAATRIGGER` and `sky`.

## Compiling

The map compiles with ReSDHLT (https://github.com/metita/ReSDHLT) or SDHLT, as the docs page Compiling shows:

```
sdHLCSG -chart co_example_aliases
sdHLBSP -chart co_example_aliases
sdHLVIS -chart co_example_aliases
sdHLRAD -bounce 2 -chart co_example_aliases
```

Worldspawn sets `MaxRange` `100000`.
