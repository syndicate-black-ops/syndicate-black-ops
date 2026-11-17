# Example map: co_example_movewith_full

The full Move With map. A small city under a blue sky, where you spawn on the plaza: an elevated train runs on a loop around the middle blocks, and the places around it show parts that ride on moving objects. A blue pad by the plaza takes you down to the test wing, a hallway whose rooms each show one tricky case. Every place has a board with what to do and what to look for, and a lectern with an Info button that opens a box with the entities and keys it uses. Open the `.map` in your editor to see how it is built.

The small example in the folder above, `co_example_movewith`, has only the train. SBO docs: Moving objects.

## Files

| File | Where it goes | What it is |
|---|---|---|
| `co_example_movewith_full.map` | your editor (Valve 220 format) | the map source |
| `van_exhaust.aur` | `syndicate/aurora/` | the thin exhaust of the van's tailpipe and the boat gun's muzzle (`env_particle`) |
| `train_where.res` | `syndicate/ui/scripts/screens/` | the layout of the screen on the train (`vgui_screen` panel `train_where`) |

## The city

| Place | What it shows | Entities |
|---|---|---|
| Train with a screen, a sign, a door and a camera | The door, the screen, the sign, the camera, the lights, the engine sound and the spawn spots ride with the train, and the door opens at each stop. Inside, hold Use on the wheel to lower or raise the windows; each pane of glass rides on the window frames and breaks at any height | `func_tracktrain`, `path_track`, `multi_manager`, `func_door`, `vgui_screen`, `vgui_control_proxy`, `trigger_copyvalue`, `point_worldtext`, `trigger_camera`, `ambient_generic`, `env_sprite`, `momentary_rot_button`, `momentary_door`, `func_breakable` |
| Spawn spots inside the train | The Train button makes every player spawn inside the train, wherever it is, and the Plaza button puts the spawn spots back on the plaza; the red square kills you so you respawn | `info_player_go2` with **Move with** the train, `multispawn`, `trigger_hurt` |
| Lift with a gate | The gate, the railings and the button ride with the cab, and the gate opens when the cab arrives | `func_door`, `func_wall`, `func_button`, `multi_manager`, `trigger_relay`, `multi_watcher` |
| Sign with 200 parts | 120 tiles and 80 lamps turn as one sign on a roof | `func_rotating`, `func_wall`, `env_sprite` |
| Building-site crane | The arm turns, the hoist goes down and up, and the crate on the hook moves with both | `func_door_rotating`, `func_door`, `func_wall`, `multi_manager` |
| Drawbridge | The lamp, the mast, the railings and the sign tilt with the deck, and the lantern keeps hanging straight down (Axis Locks 112) | `func_door_rotating`, `env_sprite`, `func_wall` |
| Boat with a gun | A two-barrel gun on a boat aims where you look while the boat sails and turns; the wake and the smoke ride along | `func_tracktrain`, `func_tank`, `func_tankcontrols`, `func_breakable`, `env_particle` |
| Carousel | The seats, the pole, the roof and the lights turn together with the floor; a pushable crate and a scientist ride along without a Move With key | `func_rotating`, `func_wall`, `func_breakable`, `env_sprite`, `func_pushable`, `monster_scientist` |
| Turning platform | Crates, a robot model and a lamp turn with the platform and stay in their places | `func_rotating`, `func_wall`, `env_sprite`, `func_pushable` |
| Van with a loudspeaker | The music plays from the van and moves with it, and the tailpipe smoke follows it | `func_tracktrain`, `ambient_generic`, `env_particle` |
| Helicopter with a rope | A scientist model hangs on a rope under a helicopter that flies a loop above the roofs | `monster_apache`, `path_corner`, `env_sprite`, `env_beam` |

## The test wing

| Room | What it shows | Entities |
|---|---|---|
| Train blocked by a player | When the train's plow hits you, the whole train waits | `func_train`, `func_wall`, `env_sprite`, `trigger_relay` |
| Removing the cart | Killing the cart removes the siren, the lamp and the crate that ride on it | `func_train`, `ambient_generic`, `env_sprite`, `func_wall`, `trigger_relay` (Kill Target) |
| Hiding and showing the cart | Hiding a parent doesn't hide its children | `func_wall_toggle`, `func_train`, `env_sprite`, `func_wall` |
| Lamps fixed to a helicopter model | **Move with** `heli.1` and `heli.2` put the lamps on the attachment points of the parent's model | `env_sprite`, `func_train` |
| Making the crate follow something else | The crate gets a new parent, or none | `trigger_changeparent`, `trigger_changevalue`, `func_rotating` |
| Following a train through an alias | **Move with** `*train_alias` follows whichever train the alias names | `info_alias`, `trigger_changeparent`, `func_train` |
| Locking a riding part's height or angle | Axis Locks 0, 8, 32 and 112 side by side | `func_wall` with `parentflags` |
| Props next to a moving parent | `env_static` props never move, next to a train with riding parts | `env_static`, `func_train` |
| Things that stay put on purpose | A mirror, rain and fog ignore **Move with**, and the console says so with `developer 1` set in the console | `env_mirror`, `func_rainvolume`, `env_fogzone` |
| A normal lift and a Move With lift | A plain lift next to a `func_wall` that moves with a small train | `func_train`, `func_wall` |
| A cart that drives out of sight | The cart jumps to a closed room far below and back, and its lamp and sign stay on it | `func_train`, `path_corner` (Teleport) |
| New round | A new round keeps the Move With links the map started with | `func_train`, `func_wall`, `env_sprite`, `trigger_hurt` |
| Moving a crate while it rides on a cart | `trigger_motion` moves the crate relative to the cart | `trigger_motion`, `func_wall` |
| Blocking a train's door | When you block the door, the whole train waits with it | `func_train`, `func_door` |

## Textures

The worldspawn `wad` key is `halflife.wad;anzymut.wad;roux.wad;co_escape.wad;sbo_tools.wad`. The map's textures come from `anzymut.wad`, `roux.wad`, `co_escape.wad` and `sbo_tools.wad` in `syndicate/`, and from `halflife.wad` in `valve/`.

## Compiling

Copy the two files above to their folders, then compile the map with ReSDHLT (https://github.com/metita/ReSDHLT) or SDHLT, as the docs page Compiling shows:

```
sdHLCSG -chart co_example_movewith_full
sdHLBSP -chart co_example_movewith_full
sdHLVIS -chart co_example_movewith_full
sdHLRAD -bounce 2 -chart co_example_movewith_full
```

The city is large, so a full VIS takes more than half an hour.

Put the `.bsp` in `syndicate/maps/` and type `map co_example_movewith_full` in the console.

Worldspawn sets `MaxRange` `100000`.
