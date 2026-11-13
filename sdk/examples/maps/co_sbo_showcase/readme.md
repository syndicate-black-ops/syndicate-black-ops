# Example map: co_sbo_showcase

`co_sbo_showcase.map` is the source of `co_sbo_showcase`, a small playable co_ map that shows what is new in SBO
2.0.0. One route leads through the rooms below, a 3-5 minute walk; every room names its feature and its entities in
`point_worldtext` labels. The mission ends at the button on the courtyard tower. Open the map, look at how a room
does it, and copy what you need into your own map.

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_sbo_showcase.map` | your editor (Valve 220 format) | the map source |
| `co_sbo_showcase.seq` | `syndicate/sequences/` | the courtyard's lightning strike (`trigger_sequence` `lt_seq` runs `%lightning`; SBO docs: Sequence files): seen and heard only in or near the courtyard |
| `co_sbo_showcase.txt` | `syndicate/maps/` | the map's briefing |

## The route

| Room | What it demonstrates | Entities |
|---|---|---|
| Spawn hall | Spawns, labels with glow and rainbow, an about box, a tutor message on every spawn (centered: `align` 4), HP 999 for the presser | `info_player_deathmatch` (20), `info_player_start`, `max_round`, `point_worldtext`, `game_messagebox`, `env_tutor`, `trigger_changevalue` on `!activator` |
| Spawn hall west wall | Object hints: a sprite marks each object for the players near it (SBO docs: Object hints): a suit charger (green with charge, red empty), a lock button and a lamp button behind the lock (yellow when +use would push it, orange while its master is off); faint brackets within 512 units (the default) | the `objecthint` key on `func_recharge`, `func_button` (2); `multisource`, `light` (named) |
| Materials | Nine floors (grass, gravel, dirt, snow, wood, carpet, tile, metal, grate): footsteps, impact debris, dust, sparks and decals differ by material | `point_worldtext` laid flat on each floor |
| Aurora hall | Eight particle effects: mist, fireflies, energy swirl, color fountain, steam vent, ember barrel, spark shower, signal fire | `env_particle` playing scripts from `aurora/` |
| Apache booth | A helicopter in a yard fires real rockets in pairs at the player (Fire Rockets, Start Inactive, no path); an open window with a `CLIP` brush keeps players out | `monster_apache`, `func_button` |
| Fades | A crate fades out, in, and to an additive ghost over 3 seconds | `env_render`, `func_wall` |
| Light styles | Named room lights get another appearance for a while: flicker, candle, pulse, power failure with a red alarm light | `light` (named), `trigger_lightstyle`, `multi_manager` |
| Entity lab | North of the light styles room. Player freeze: the presser or every player for 3 seconds (Freeze / Unfreeze, Toggle, Copy input through relays, All players). Kill monster: a glass cage with two monster makers, kill by name, by class (gibs), by squad, or an instant kill by trigger_hurt_remote. Hurt remote: hurt a crate, poison gas on the presser (never below 25 health), heal, full health, +50 armor. Player numbers: a health check and a button that works only above 90% health. Look: look at the screen for 2 seconds. Locus beam: pylon to pylon, a beam to the presser, a beam weapon that breaks a crate, scorch marks, a beam straight up | `trigger_playerfreeze` (6), `trigger_relay` (Forward activator), `multi_manager`, `trigger_killmonster` (3), `monstermaker` (2, with `squadname`), `trigger_hurt_remote` (6), `func_breakable` (2), `player_calc_ratio`, `trigger_compare` (as a button master and fired), `game_text`, `trigger_look`, `env_dlight`, `locus_beam` (5), `calc_position`, `info_target` |
| Dynamic lights | A dark room: a red alarm light (toggle), a blue light switched by relays (On / Off), a green light for 3 seconds, a yellow light that follows the presser for 5 seconds, an orange light riding a moving train (started at the map start: a named train waits for a trigger) | `env_dlight` (5), `trigger_relay`, `func_train` (with an ORIGIN brush), `path_corner`, `trigger_auto` |
| Model scale | A 5x imp, 1x and 0.2x robots, a 0.3x zombie; a Fed with scale buttons and smooth growing / shrinking; a button that spawns a random monster and grows it to 3x | `env_model`, `cycler`, `trigger_changevalue`, `env_render` (Scale with a fade time), `trigger_random`, `monstermaker` (5), `func_monsterclip` |
| Washroom | A wall mirror and a shiny floor that reflect models (players, scientists); the rooms behind the mirror and under the floor are mirrored copies of the world. A flame burns on the sink; Aurora particles are not reflected, so the mirrored room has its own flame at the mirrored spot | `env_mirror` (2), `monster_scientist`, `path_corner`, `env_particle` (2, `aurora/signal.aur`), `env_explosion`, `env_spark` |
| Lounge | Music in the room only (Play Mode Loop, linear falloff: full volume within 256 units, silent at 768); a wall screen and a kiosk screen with buttons | `ambient_generic`, `vgui_screen` (2, layout `example_screen`), `vgui_control_proxy`, `trigger_auto`, `{screen` faces on `func_wall` |
| Seamless teleport | Halfway along a bent corridor the player moves to a copy of it elsewhere in the map, turned 90 degrees, without a jump in view or weapon | `trigger_teleport` with `info_landmark` (2 pairs) |
| Courtyard | 3D sky (the skyline is a room high above the map), rain and snow, grass, sunlight, jump pads, safe falls, a water pool where rain makes ripples | `env_sky` and the `env_sky` tool texture, `env_rain`, `env_snow`, worldspawn `grassDensity` / `grassFadeDist`, `light_environment`, `trigger_jump`, `trigger_safe_fall`, `!waterblue` |
| Courtyard west wall | Day / night, map fog on / off, a fog zone for whoever walks in, the lightning strike (every 30-60 seconds or by button; the flash and the thunder reach only the players in or near the courtyard) | `global_light_control`, `env_fog`, `env_fogzone`, `trigger_sequence`, `trigger_lightstyle`, `env_laser`, `env_explosion`, `env_fade` (Activator Only) fired per player by `game_zone_player`, `trigger_random` + `ambient_generic` (Play Mode 5: play once, linear falloff), `info_target` |
| Tower | END button: a 10 second countdown, then the mission is complete | `game_timer`, `game_end`, `game_text`, `multi_manager` |

Worldspawn also sets `skyname` `citynight` and `MaxRange` `100000`.

## Textures

The worldspawn `wad` key is `halflife.wad;anzymut.wad;roux.wad;co_escape.wad;sbo_tools.wad`. Add these to the
editor's textures:

- `halflife.wad`: in `valve/` (stock Half-Life): `sky`, `!waterblue`, `AAATRIGGER`, `CLIP`.
- `anzymut.wad`, `roux.wad`, `co_escape.wad`: in `syndicate/`.
- `sbo_tools.wad`: in `syndicate/`: `{screen` (the lounge screens; SBO docs: World screens).
- `env_sky` (the courtyard's sky brushes): the tool texture is not in any shipped WAD. Any 16 x 16 texture named
  `env_sky` in a WAD the editor loads does (SBO docs: env_sky); its picture is never drawn.

## Compiling

The map compiles with ReSDHLT (https://github.com/metita/ReSDHLT; the same tool names and options as SDHLT 1.2.0):

```
sdHLCSG -chart -wadinclude sbo_tools co_sbo_showcase
sdHLBSP -chart co_sbo_showcase
sdHLVIS -fast -chart co_sbo_showcase
sdHLRAD -fast -bounce 2 -chart co_sbo_showcase
```

- `-wadinclude sbo_tools` puts `{screen` into the `.bsp`.
- ReSDHLT v0.21.0 leaves the areas under the `env_sky` texture (the courtyard) without sunlight; SDHLT 1.2.0
  lights them. With ReSDHLT v0.21.0, check the courtyard's lighting after compiling (SBO docs: Map compiler bugs).
- The original Valve tools do not know `env_sky` and draw it as an ordinary texture.

Put the `.bsp` and `co_sbo_showcase.txt` in `syndicate/maps/` and `co_sbo_showcase.seq` in `syndicate/sequences/`.
The models, sprites, sounds, Aurora scripts, the sky and the screen layout `ui/scripts/screens/example_screen.res`
the map uses ship with the game.
