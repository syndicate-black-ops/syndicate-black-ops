# Prefab: security station (world screens showcase)

`security_station.map` is a complete security station built only from SBO's world screen entities and stock
logic entities: a console with a bent touch screen, a status monitor over a blast door, a lockable door, an alarm
and switchable room lights. Everything the screens show comes from the map, so they always match what the map does,
whoever changed it. How it works: the SBO docs, World screens, "Showcase: security station".

## Using it

1. Add `anzymut.wad` and `sbo_tools.wad` (both in `syndicate/`) to the editor's textures.
2. Open `security_station.map`, select everything, copy it, and paste it into your map. The station stands south of a
   wall whose face is at y 0 (the prefab includes that wall piece: delete it when you place the station against a
   wall of your own), the floor is at z 0, and it spans x 0 .. 768. The partition with the blast door is at
   x 752 .. 768 and reaches 448 units into the room: shorten or move it to fit your room.
3. Compile with `-wadinclude sbo_tools` on CSG (`{screen` goes into the .bsp).
4. The layouts `ui/scripts/screens/security_main.res` and `security_mirror.res` and their animation scripts
   `security_main_anims.txt` / `security_mirror_anims.txt` ship with the game: nothing to add to your map's `.res`
   file. To change them, copy a layout and its script under a new name into your map's folder and set the
   `vgui_screen`s' `panelname` to that name (SBO docs: World screens, "A map's own layout").

## What is in it

Every entity name starts with `sec_`. For a second station in the same map, rename all of them (find and replace
`sec_` with, for example, `sec2_` in the .map file before pasting it).

| Entity | What it does |
|---|---|
| `func_wall` `sec_console` | The console: desk, control strip, display, frame. Its `{screen` faces (the upright display and the strip slanted toward the player) are one 160 x 100 unit panel; the texture is fitted so that the layout's bend (pixel row 420 of 600) is exactly at the edge between them. |
| `func_wall` `sec_monitor` | The 64 x 32 status monitor over the door, `{screen` on its west face. |
| `vgui_screen` `sec_screen` (2) | The console's screen (`panelname` `security_main`, `surface` `sec_console`) and the monitor's (`security_mirror`, `surface` `sec_monitor`, spawnflag 2 "Input Disabled": display only). Both have the same name, so every `vgui_control_proxy` aimed at `sec_screen` changes both. The console's keys `~door`, `~alarm`, `~lights` connect its buttons to the map; its tabs are local animations (`anim:page_status` / `anim:page_log`), each player's own. |
| `trigger_relay` `sec_door_toggle`, `sec_alarm_toggle`, `sec_lights_toggle` | One per system: firing it switches that system. Its target alternates between the system's two state managers (a `trigger_changetarget` in each manager points it at the other one). The console's buttons and the wall switches fire these relays. |
| `multi_manager` `sec_door_unlock` / `sec_door_lock`, `sec_alarm_on` / `_off`, `sec_lights_off` / `_on` | One per state: switch the real entities, set the screens' texts, play the state's animation, point the relay at the other state, log the event. |
| `multisource` `sec_door_ms`, `trigger_relay` `sec_ms_flip` | The door lock. `sec_ms_flip` is the multisource's only input, and each fire flips it, so the lock starts closed. The open button and the door have `master` `sec_door_ms`. |
| `func_door` `sec_door` | The blast door (moves up, closes after 4 seconds). Only the open button opens it: it has a name (touch does nothing) and must not have the "Use Only" flag (that would let players use the door directly). |
| `func_button` (3) | On the wall: the door lock switch (`sec_door_toggle`), the alarm switch (`sec_alarm_toggle`) and the open button (`sec_door_open`, locked while the door is locked: it buzzes). The switches show that the screens follow changes made without them. |
| `light` `sec_room_light` (3), `light` (fill) | The room lights the LIGHTS button switches, plus a dim light that stays on. |
| `light` `sec_alarm_light`, `env_sprite` `sec_alarm_sprite`, `ambient_generic` `sec_alarm_siren` | The alarm: a flashing red light (`pattern`), a pulsing red glow and the looping siren (`ambience/siren.wav`). |
| `vgui_control_proxy` `sec_p_...`, `sec_a_...` | `sec_p_<control>_<state>`: a text (the status values, the button texts), `sec_p_log1-3`: the log lines, `sec_p_alarmbar`: the alarm bar's value; `sec_a_<system>_<state>` (mode 5): the state's animation (color fades, the alarm's header pulse), kept per state group `door` / `alarm` / `lights` for players who join later. |
| `info_target` `sec_alarm_vars`, `trigger_copyvalue` `sec_alarm_tick` / `sec_alarm_push`, `trigger_changevalue` `sec_alarm_reset`, `trigger_relay` `sec_alarm_tick_on` / `_off` | The alarm bar: while the alarm is on, `sec_alarm_tick` (constant, every second) adds `$f_step` (3.334) to `$f_level` and fires `sec_alarm_push`, which copies `$f_level` into `sec_p_alarmbar`'s `text` and fires it (the bar takes it as percent: full after 30 seconds). |
| `info_target` `sec_log`, `trigger_copyvalue` `sec_log_cp*` / `sec_log_pub*`, `trigger_changevalue` `sec_log_set_*`, `multi_manager` `sec_log_shift` / `sec_log_publish` / `sec_ev_*` | The event log: `sec_log` holds the three lines in the custom keyvalues `$s_l1` .. `$s_l3`. An event moves line 2 to 3 and line 1 to 2, writes its text into line 1, copies the three lines into the log proxies' `text` and fires them. |

## Changing texts

- **Fixed texts on the screens** (title, row names, the button texts at map start): in the layout files. Colors
  and the header pulse: in the `_anims.txt` scripts.
- **Texts the map sets** (status values, button texts after a change): the `text` key of the `vgui_control_proxy`
  named `sec_p_<control>_<state>`, e.g. `sec_p_DoorState_unlock` sets "UNLOCKED". Keep the map start state's text
  and color the same as the layout's (the layout is what players see before anything is changed).
- **Log lines**: the `m_iszNewValue` of the `trigger_changevalue` `sec_log_set_<event>` (at most 127 characters,
  shown on one line). The first line at map start is `sec_log`'s `$s_l1` and `LogLine1` in the layout.

## Limits to know

- The tabs are each player's own; the buttons (DOOR, ALARM, LIGHTS) change the systems for everybody.
- While the alarm is on, the console and the monitor repaint every frame (the pulse).
- 16 screens at most per map on a client: this station uses two.
