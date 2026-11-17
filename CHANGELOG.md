# Changelog

All notable changes to Syndicate Black Ops for players, mappers and server operators, compared with SBO 1.38. The
newest release is first. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
versions follow [Semantic Versioning](https://semver.org/).

## [2.2.0]

### New

- The 2.0.0 docs are now available, see: https://sbo.sbotnas.io (if you see the old page hard-refresh or clear your browser cache)
- Added an offline copy of the docs to the game in `syndicate/manual/`, with search
- Bullets that hit water make a splash with a ripple and a splash sound, including shots from monsters, turrets and mounted guns
- Guns give off a puff of smoke from the barrel in first person when you fire
- The classic Advanced window has a Check for updates at start option on the General tab, as the New GameUI menus already had

### Changed

- The Assassin's cloak now hides you from monsters, turrets and mounted guns, which only spot you within a range that shrinks the less you move, and shrinks further in the dark
- Trip mines slow you down by 6 again, as in 1.38, and their description now says so
- In Co-op, when a player kills another player, the kill feed shows the weapon icon in green
- No default key quits the game anymore. In 1.38, F10 opened the quit window and F12 quit the game
- Enter is no longer bound by default. In 1.38, a new install used it for Use, and Use Defaults made it Primary attack
- Team chat (`say_team`) has been removed, and U is no longer bound to it. In 1.38, the server ignored team messages, so they were never sent
- The Bullet impact debris and smoke option is now called Bullet impact and gun effects, and it also turns the water splashes and gun smoke on and off
- Fog volumes look the same from every angle, and their sides fade softly where they meet open air

### Fixed

- Fixed your name color disappearing after you change your name
- Fixed weapons and items briefly flashing in your view when a map, a command or a plugin gives them to you
- Fixed new installs starting with a darker screen than SBO is made for, because the gamma and brightness values the SBO team shipped were missing. If you already have SBO installed, set Gamma and Brightness in Options > Video (this requires a game restart), or type `gamma 3` and `brightness 1` in the console, the values SBO 1.38 shipped with
- Fixed the Pitbull pickup showing Half-Life's snark nest world model instead of the Pitbull world model
- Fixed the hand grenade's loadout description saying 250 damage, while a thrown grenade does 100
- Fixed the Targeting system being spelled "Targetting" in the loadout menu
- Fixed a Co-op player banned for teamkills (`mp_ban 1`) looking like a normal agent instead of wearing the hunted model
- Fixed on-screen map text sometimes not showing when it replaced another text that was still on the screen
- Fixed the radio key opening an empty radio menu in Bounty Killer. The radio only works in Co-op
- Fixed a Co-op player who leaves the server or becomes the Game Master counting as a death, which could lose the round early with `mp_hardcore`
- Fixed a Co-op mission showing both FAILED MISSION and SUCCESSFULL MISSION when the map triggers both. The game showed every result and could count the second one. Now only the first result shows and counts. This was a bug in SBO 1.38
- Fixed the sky smearing when a map turns its 3D sky off. The map's normal skybox shows instead
- Fixed the name color picker not showing the current color or updating as you move through the color wheel

### For mappers

- Added an SDK download with example maps, prefabs and the FGD to each release
- Removed `co_sbo_showcase` from the game download. It now comes with the SDK
- `info_waypoint` marks a spot with an icon, a label and its distance in meters. It shows through walls and moves to the edge of the screen with an arrow when the spot is off the screen
- Fixed J.A.C.K. not reading any entity after `env_particle` in the editor file (FGD). A helper line for an upcoming series of J.A.C.K. plugins stopped it, and it has been removed
- `env_entity_glow` draws an outline or a soft halo around models and brush entities, also through walls and floors, to mark an objective or anything else players should find
- `env_static` has a new In the 3D sky flag, which shows the model in the sky room of an `env_sky` without any network cost
- `env_sky` has new Sky fog color, Sky fog density and Skybox fog keys, which give the sky room of a 3D sky its own fog, while the map's fog stays out of it
- A Kill target on an `env_static` now hides its model. Before, the model stayed on screen
- Aurora particle scripts read the top-level `attachment` and `lifetime` keys of XashXT and PrimeXT scripts
- Players always get their fists when they spawn, also on maps with a `game_player_equip`. Before, any `game_player_equip` took the fists away
- Color correction lookup files must be `.raw` files, the only kind the game can send to players
- The "hidden" Object Hint Template has been removed. It did the same as None
- The editor file (FGD) no longer offers the Not in multiplayer flag (2048). The engine removes every entity with this flag on any SBO server
- Fixed the Unkillable key doing nothing on the Apache, turrets and sentry guns
- Fixed locus ranges of two positions, such as `0 0 0 .. 10 20 30`, reading the second position wrong. Blanks around `..` are now allowed
- More help text in the editor file (FGD) was corrected to match what the game does
- The SDK has nine small example maps, one topic each, such as locus, calc_ entities, color correction and world text
- Added `env_sky` tool texture to `sbo_tools.wad` for 3D skyboxes

### For server operators

- `mp_XP` has been removed. XP and levels are always on, and Create server no longer shows the option
- `mp_falldamage` has been removed. It did nothing, fall damage always depends on the speed of the fall
- Fixed a server crash when a plugin removes an entity while it takes damage, as the sentry gun plugin did

## [2.1.0] - 2026-11-13

### New

- 2.1.0 is a required update with breaking changes: players on 2.0.0 cannot join 2.1.0 servers
- Mapping and entity guides are coming soon
- The scoreboard shows uncategorized players in a Ghosts section
- The Game Master can select and remove monsters placed by the map, not only the ones the Game Master spawned
- At the end of the map vote, a gold border draws around the winning map
- Advanced options has three new settings: field of view, weapon tilt, and view roll

### Changed

- Dead players can enter Game Master mode again, as in 1.38
- Kill lines in the console now show the full name of the weapon instead of its class name, for example "Fists" instead of "wong"
- Monster names in the kill feed were updated to match the original SBO names
- The SourceMod-style radio menus have a dark background, so you can read them on bright maps

### Fixed

- Fixed the spectator map overview zooming in when the player you watch zooms with a scope. Only that player's view zooms now, in full screen or in the picture-in-picture window
- Fixed the K444, Federal, RC 28, SW, NM33 and PM66 freezing on the first frame of their idle animation
- Fixed the Vindico grenade launcher and the RC 28 firing an extra shot when you hold both fire buttons
- The fire rate of weapons no longer changes with your frame rate
- Fixed an empty minigun playing its firing animation for as long as you hold the fire button
- Fixed the next hand grenade popping into your hand without its draw animation after a throw
- Fixed the flashlight cutting off the chainsaw's sound when you switch it on or off
- Fixed Werewolves being unable to move about 50% of the time. Their model has a second walk animation with no ground movement, and the game picked it at random
- Destroyed turrets, sentry guns and ichthyosaur bodies fade away when the map sets the `Fade Corpse` flag
- Fixed the last wave of Feds on `co_necropolis` never coming. Its monster makers asked for a monster type that does not exist in SBO
- Fixed some mortar strikes on `co_dfs` and `co_rain_of_fire` never coming. Their `multi_manager` has 22 targets, but SBO 1.38 fired only the first 16
- Fixed chapter titles showing only to the first player on the server. Every player sees them now, including players who join later
- Fixed fans and other spinning objects always playing their sound at full volume, ignoring whatever volume the mapper set. The game read the volume key under a differently written name, so it never found it
- Fixed spinning objects pushing spectators around and turning their view. The game still treated dead and waiting players as walking, so a spinning object they touched carried them along
- Fixed radiation (Geiger counter) clicks carrying over into the next map
- Fixed the Apache and Osprey rotor sound changing pitch for everyone whenever the first player on the server moved
- Fixed on-screen map text (`game_text`) disappearing too early when the map shows it again before the first one has faded out. The repeated text now restarts and stays for its full time
- Fixed maps without a sky set showing the sky of the previous map. They now show the desert sky by default
- Fixed Assassins stuttering after they land
- Fixed all Assassins sharing one footstep timer
- Fixed players losing their weapons and HUD when a mounted gun they use is removed from the map
- Fixed the sound of turnable valve wheels and levers continuing to play after you let go of them

### For mappers

- Movewith (experimental):
  - Any entity can be attached to another entity and moves and turns with it, as in Spirit of Half-Life. Put the parent's name in the new Move with key (`movewith`)
  - `trigger_changeparent` gives an entity a new parent while the map runs
  - Movewith is experimental, test thoroughly when using it

- New entities:
  - `trigger_objective`: shows mission objectives in a panel on the player's screen, which players can switch between short, full and hidden with H. On maps that use it, the scoreboard also gets an objectives section that players can expand and collapse
  - `color_correction` and `color_correction_volume`: change the colors of the player's view (color grading), for the whole map or inside an area, with simple keys or `.raw` lookup files
  - `env_overlay`: show a static image, animated sprite, or a color tint over the player's screen
  - `env_elight`: a dynamic light that lights models only
  - `info_light_origin`: a model takes its lighting from this marker instead of beneath its origin
  - `info_alias`, `multi_alias`, `trigger_changealias` and `info_group`: aliases, which are stand-in names that point to other entities and can be repointed while the map runs. Write `*name` in a Target, Kill target or Master to use whatever the alias `name` points to at that moment. For example, one button can open a different door after a `trigger_changealias` repoints its alias
    - `info_alias` points to one entity, and switches between two when it is turned on and off
    - `multi_alias` points to several entities at once, or picks one or some of them at random
    - `info_group` is a list of named members, used as `group.member`. When triggered, it points an alias at itself, so `*alias.member` switches to this group's members all at once
  - `multi_watcher` and `watcher_count`: watch other entities, for example whether a door is open, a monster is alive or a light is on, and fire their targets when that changes. They can also be used as a Master
  - `train_setspeed`: changes the speed of a moving `func_tracktrain`, at once or gradually, so a train can speed up, slow down or stop by itself
  - `item_pickup`, `player_inventory` and `player_hasinventory`: items players pick up and carry, such as keys and keycards, and checks for whether a player carries them
- In-game editors that you open from the console and use while you look at your map: `colorcorrectionui` for color correction looks (`.raw` files for `color_correction`), `particleeditorui` for particle effects (`.aur` files for `env_particle`) and `worldscreeneditorui` for world screen layouts (`.res` files for `vgui_screen`). They work in single player, in a game you host, and on a server with `sv_cheats 2`
- New console commands for testing maps with `sv_cheats 1`, for example `cheat_noclip`, `ent_create` and `ent_fire`. The cheat commands guide lists all of them

- A target ending in `*`, for example `room1_*`, triggers every entity whose name starts with `room1_`
- Target and Kill target accept `!activator`, the entity that started the chain (usually the player). Kill target `!caller` makes an entity remove itself
- 3D skies (`env_sky`) now draw the entities in the sky room too, such as moving clouds, sprites and models, not only its brushes
- `env_rain` and `env_snow` get the keys of the rain and snow volumes (amount, drop size, speed and more), and all four get wind keys to blow the rain or snow in a set direction
- Spawn points can be grouped into spawn sets by giving them a name, and the new Spawn set key of `multispawn` switches to any set, in any order
- The game now triggers entities named `game_round_<N>` when round N starts, `game_round_failed` when a round is lost, and `game_mission_success` or `game_mission_failed` when the mission ends
- `game_player_settings` can set a player's `level` and `xp`
- `trigger_gravity` can be switched on and off by name, has a Master, and has Start Off and Reset on leave options
- `trigger_camera` has new All Players and Hide HUD and crosshair flags
- `monster_apache` has a new No Wreckage flag (1024). Flag 8, which the editor called No Wreckage, is renamed Explode sooner, because that is what it always did
- `trigger_relay` has a Master
- Fixed the Master key doing nothing on `button_target`, `trigger_hurt`, `trigger_push`, `trigger_monsterjump`, `momentary_rot_button` and a triggered `game_player_equip`. These entities never checked their Master, so they worked while locked
- Spawn On Break of `func_breakable` and `func_pushable` accepts any item, weapon or ammo class name, not only the fixed list
- Monsters have an Unkillable key that keeps them from dying
- A key written as `#name` on a monster maker is passed on as `name` to every monster it makes
- `multi_manager` can now fire up to 64 targets instead of 16 as in the original
- Buttons, doors and `momentary_door` can use custom sounds or sentences now
- `aiscripted_sequence` and `scripted_action` have an Idle Animation that the monster plays until the action is triggered, and `aiscripted_sequence` now has No Interruptions
- `trigger_changevalue` and `trigger_copyvalue`:
  - A changed key now takes effect at once on fog, `ambient_generic`, `func_rotating`, `func_conveyor`, `trigger_push`, lights, `env_explosion`, `env_dlight`, `env_sky`, `multi_manager`, rain and snow. Before, many of these kept the old value
  - A value of three numbers, such as `64 0 0`, now sets each part of a vector key. It was read as `64 64 64`
  - Add, Multiply and the other operations work on more keys, and leave a key alone when they can't read it
  - A monster maker that ran out starts again when it gets a new Number of Monsters, and raising a monster's health above its maximum raises the maximum too
  - Changes that would crash the server or break the entity, such as a model the map has not loaded, are refused

- Fixed the New Train rot. Speed key of `path_corner` being ignored. The game never read the key
- Fixed a moving `func_guntarget` not stopping when triggered. It restarted from its next stop instead
- Fixed rotating buttons never playing their locked and unlocked sounds. The game never loaded those sounds for rotating buttons
- Fixed a named monster maker with Start ON not starting with the map (its first trigger turned it off)
- Fixed a bug in monster maker set to unlimited monsters (-1) stopping after about 4.29 billion monsters had been spawned. If anyone wants to confirm, run the any map for 138 years and let me know if it's fixed
- Fixed `trigger_changetarget` ignoring its Delay and its Kill target. It changed the target at once and never read either key
- Fixed a `trigger_hurt` with only a Kill target not removing that target. It fired its targets only when it also had a Target
- Fixed `aiscripted_sequence` ignoring its interruption setting
- Fixed a `func_trackautochange` crashing the server when a trigger without an activator used it. The game used the activator without checking that there was one
- Fixed a monster maker without a monster type stopping the server when the map loaded. It tried to load a monster with an empty name
- Fixed a `game_player_equip` that gives an item the map does not place crashing the server. The game never loaded the item
- Fixed `max_round` 1 and 2 being changed to 5. The game turned any value of 2 or less into 5
- Fixed the Limitator giving 1 reserve ammo for a value of 0 or above 910. A value of 0 now gives none, and anything above 900 gives 900, the most a player can carry
- Fixed a `func_rotating` with volume 0 playing at full volume, and an `env_shooter` without a Material Sound making glass sounds
- Fixed turrets, Sentry Guns, the Apache and the Osprey never firing their Trigger Condition. In SBO 1.38, their own AI code never checked it
- Fixed Custom Field Of View, Prisoner to, Ignored by, Speak As and Fire after moving doing nothing on turrets, sentry guns, Apache, Osprey, Pitbull, Cyborg, Cyber Minotaur, sitting scientist and named `aiscripted_sequence` entities
- Fixed Feds dropped by an Osprey ignoring the Osprey's Classification and Is player ally, so an allied Osprey drops allied Feds
- Fixed a `trigger_camera` started by a monster or a timer taking over the view of the player in the first server slot, even a spectator. When no player started the camera, the game gave the view to that slot
- Fixed a `trigger_teleport` without a Target sending players to a random spot. An empty Target matched any entity without a name
- Fixed entities with a Global Entity Name, such as some breakable objects, staying destroyed when the server changes back to their map. The game never cleared their saved state on a map change
- Fixed entity behavior that did not match the editor help:
  - `calc_velocity_polar` turned vectors that pointed up into vectors that pointed down
  - `env_state` did the opposite of On and Off while it was turning on
  - `multisource` with a Delay before trigger flipped the wrong input
  - `env_global` ignored its Initial Value when it created a new state
  - `trigger_condition` without a Check interval now checks every 0.1 seconds
  - `speaker` with announcement preset 7 stayed silent one time in nine
  - `scripted_action` with `!activator` as Entity to attack aimed at the wrong entity
  - `scripted_following` made Cops follow without being player allies with a Followage policy
  - `trigger_copyvalue` Round down and Round to nearest rounded negative values the wrong way
- The editor file (FGD) uses the SBO monster names, has correct help text and defaults, and no longer lists keys and flags that do nothing

## [2.0.0] - 2026-11-06

### New

- SBO 2.0.0 needs the Half-Life 25th Anniversary update and runs natively on Linux.
- A new chat box shows names in green while alive, red while dead and yellow without a loadout.
- Chat has been completely reimplemented using VGUI2, and as a result now supports name and chat colors, copy + paste, multi-line messages, history, unicode characters, and more
- You can pick your chat and name colors with a color wheel, and set the chat text size, colors, new message sound and message time.
- A new scoreboard shows Steam avatars, level, XP and the round count, with a mute button and a right-click menu.
- The kill feed has an icon for every SBO weapon and a turret icon. SBO 1.38 had no kill feed icons for them, so the game used the default skull icon as a fallback each time
- The kill feed shows a headshot icon for kills by a headshot
- The kill feed names the monster that killed a player and shows the monsters that players kill, while SBO 1.38 showed no killer for a monster kill and didn't show monster deaths
- Players talking on voice show their Steam avatars.
- The console keeps a copy of every chat line with its time, such as `[12:34:56]`, and also shows kill lines and server text in their respective colors
- `hud_scale 1` makes the HUD larger on big screens.
- The server browser, Options, Create server and the joining screen are new, and Half-Life's default menu style is still available in Advanced options
- The loadout, map briefing and message of the day windows are new
- B opens the map briefing (`showbriefing`). In 1.38, the only way to see the map briefing was by hitting Enter in spectator, which was removed in 2.0.0
- Servers can show a web page as their message of the day.
- The game updates itself automatically.
- Spectators have six camera modes, including first person and a map overview with a picture-in-picture view.
- `spectate` or `/spec` lets you watch, and spectating stays on through map changes and reconnects.
- An info box shows the watched player's health, armor, weapon, legs and level.
- Game Master mode is built into the game: on Co-op maps the Game Master can fly around, spawn monsters by tier, place weapon and ammo drops and teleporters, select monsters and give them orders, and see health bars over players and monsters.
- Players pass through each other for the first 15 seconds of a round.
- Maps can have rain, snow and fog.
- Footsteps and bullet impacts have their own sounds and effects for each surface, such as wood, grass, snow, carpet and gravel.
- Maps can use particles, 3D skies, mirrors, music, videos, text and screens.
- 16 weapons have new pickup models.
- An AFK sign shows over idle players.
- You can hold your weapons left-handed.
- Sprays can be animated.
- `thirdperson` and `firstperson` work in multiplayer.
- Discord shows your game status.
- The new map co_sbo_showcase shows what maps can do in SBO 2.0.
- co_siege and co_forgottenworld show a countdown timer.
- Every stock map has a new overview, with a picture per floor where needed.

### Changed

- SBO no longer ships a `config.cfg`, so updates keep your settings
- The default keys changed: Space jumps, M opens the model menu and F12 no longer quits. SBO 1.38 had no jump key on Space, left the model menu without a key and quit the game on F12.
- All menus are redone, and the radio and text menus are larger.
- After a failed mission the server goes to the next map.
- The next map starts by itself after a mission. In SBO 1.38, as in Half-Life, the scoreboard waited up to 2 minutes for a key press, which a click on the scoreboard doesn't give.
- The speed you gain from leveling up applies right away. In SBO 1.38 the new speed took effect only after your next weapon switch.
- The speed you lose for team kills and suicides applies right away.
- Levels can go below -40, but speed stops dropping at -40.
- In Co-op, your speed after the speed penalties of your loadout is never below 80. In 1.38, a player at level −40 with a heavy loadout couldn't move
- Suicides no longer cost points in Battle Royale and Bounty Killer. In SBO 1.38 a suicide cost 5 points in Battle Royale and 10 in Bounty Killer, which only made matches last longer
- Items now do what their descriptions say: Croon Armor Skin absorbs 60%, Croon Cyber Torso Armor covers the stomach, and Croon Cyber Torso and Skull Armor absorbs 70% and covers the stomach. The item descriptions said this, but the code gave less, so the code now matches the descriptions.
- Trip mines no longer slow you down, satchel charges deal 200 to 300 damage, and the Adrenaline Surge gives 75 extra health in every mode. The item descriptions said trip mines don't slow you down and satchels deal 200 to 300 damage, but the code took 6 off your speed and always dealt 200. Co-op already gave 75 extra health, so every mode now does the same.
- The descriptions of the Canon, Benelli, rocket launcher, minigun and RC 28 show their real damage and fire rates. The descriptions gave other numbers than the code used, so they now match the code.
- Bullet hits on monsters sound like flesh.
- Weapons move with screen shake and recoil.
- `m_rawinput 1` turns on raw mouse input.
- Steam's "Mute players" setting works and matches the scoreboard.
- Servers can no longer change your mouse sensitivity.

### Fixed

- Fixed not being able to pick up satchels, from the map or a dead player's backpack, while you have satchels deployed
- Buttons with the Sparks flag now spark at the button, as their mappers set them. Half-Life placed the sparks in the wrong spot and stopped them after the first one, so in 1.38 they never showed; co_aemos and some custom maps have such buttons.
- The sniper scope works at every resolution and field of view. This was a bug in SBO 1.38.
- Your field of view is kept after leg injuries, falls and respawns. This was a bug in SBO 1.38.
- The K444 crosshair is centered. This was a bug in SBO 1.38.
- Fixed the Vindico grenade launcher not reloading when one grenade is left in the magazine
- Other players now see RC 28 shots, the FA18 reload sound plays, and RPG explosions show their full smoke. This was a bug in SBO 1.38.
- A primed grenade is no longer thrown by itself when you leave a mounted gun. This was a bug in SBO 1.38.
- You can throw Pitbulls while looking straight up. This was a bug in SBO 1.38.
- Fixed a satchel charge or a trip mine under a door jamming the door. The door now sets it off. This was a bug in SBO 1.38
- Grenade launcher and plasma sniper kills show their own kill icons. This was a bug in SBO 1.38.
- A VERY BAD SHOT can now actually happen. In SBO 1.38 the hit roll dropped its fractions, so the roll for VERY BAD SHOT almost never came up.
- Cops give XP for every hit, not only the killing one. In SBO 1.38 a Cop gave XP only for hits during its death animation.
- The medikit gives XP only when it heals a player. In SBO 1.38 it also gave XP for hitting a wall, a monster or a body.
- Assassin's speed no longer depends on the armor you picked before choosing Assassin. In SBO 1.38 the server kept the armor you had picked before Assassin and used its weight.
- Picking another loadout after Assassin while dead no longer leaves you with only the blade. In SBO 1.38 the loadout menu spawned you before your new choice had arrived.
- The spawn health penalty never takes you below 10 health. In SBO 1.38 a spawn health of 27 to 35 dropped to 2 to 10.
- No new round starts during the mission end or the map vote. This was a bug in SBO 1.38.
- A joining player can no longer cancel a mission success. This was a bug in SBO 1.38.
- A player who leaves frees their place for the grenade launcher or the nuke launcher. This was a bug in SBO 1.38.
- Fixed a single player in Battle Royale losing their weapons and respawning with "ROUND START" every 10 seconds. The game now waits for a second player
- In Bounty Killer, the first player spawns once, as the Hunted. This was a bug in SBO 1.38.
- Fixed players in the menus or waiting for the next round seeing no chat. The game only sent chat to players it counted as in the game, and SBO 1.38 counted waiting players as observers instead, so they were skipped
- Spectators move at the right speed. This was a bug in SBO 1.38.
- The spectator clock runs. This was a bug in SBO 1.38.
- Players who join late see the map's fire and smoke before they first spawn. This was a bug in SBO 1.38.
- Mounted guns target every player. This was a bug in SBO 1.38.
- Cyborgs walk instead of standing still. This was a bug in SBO 1.38.
- Demon Slaves and werewolves keep the settings the map gives them. This was a bug in SBO 1.38.
- The Cyber Minotaur fires from its gun and picks targets it can see. This was a bug in SBO 1.38.
- Plasma sniper and Heavy Weapons Fed impacts no longer show random models. This was a bug in SBO 1.38.
- Rappelling Feds no longer shoot at walls. This was a bug in SBO 1.38.
- A falling Apache explodes when it lands on a player or a monster. This was a bug in SBO 1.38.
- Demon Slaves on HardCore monster health have 900 health, not 9000. This was a bug in SBO 1.38.
- The Gargantua's eye glow stays on its head. This was a bug in SBO 1.38.
- The G-Man moves at the end of a mission. This was a bug in SBO 1.38.
- Map cameras give your view back when you die or start spectating. This was a bug in SBO 1.38.
- Ice floors stop being slippery when you step off them. This was a bug in SBO 1.38.
- Fixed maps without sky light using the sky light of the previous map. The game never reset the sky light between maps. This was a bug in SBO 1.38
- Train and elevator sounds stop when they stop. This was a bug in SBO 1.38.
- Gibs that get stuck disappear. This was a bug in SBO 1.38.
- The nuclear explosion's afterglow shows. This was a bug in SBO 1.38.
- Monsters can no longer break breakables that only a trigger should break. This was a bug in SBO 1.38.
- Dead players no longer leave empty weapon boxes. This was a bug in SBO 1.38.
- On co_aemos, the artifact can no longer fail the mission after it is picked up. This was a bug in SBO 1.38.
- co_bunker13 and co_ufo show their own skies. This was a bug in SBO 1.38.
- co_gore plays its announcements, co_bunker13 plays a missing sound, and co_fondery shows its pipe textures. This was a bug in SBO 1.38.
- The Cyber eyes light no longer flickers. This was a bug in SBO 1.38.
- Radio calls play once instead of once per player. This was a bug in SBO 1.38.
- The game no longer crashes at the co_appart bathtub or with long-lived Pitbulls. This was a bug in SBO 1.38.

### Removed

- The "Reload map?" vote after a failed mission is gone. A failed mission now moves on to the next map.

### For mappers

- Custom maps keep the behavior they had in 1.38.
- Weather and fog come from `env_rain`, `env_snow`, `env_fog` and `env_fogzone`.
- New effects are `env_particle`, `env_sky`, `env_mirror` and `env_dlight`.
- Text and screens come from `point_worldtext`, `vgui_screen`, `game_playvideo` and HTML map briefings.
- HUD entities are `game_timer`, `game_messagebox`, `env_tutor` and `trigger_sequence`.
- `ambient_music` plays music, and `ambient_generic` plays MP3 files.
- Movement entities are `trigger_teleport` with landmarks, `trigger_jump`, `trigger_safe_fall` and `func_vehicle`.
- Logic entities cover values, conditions, random choices, timers, Locus and calculations, with hooks at round start and round end.
- Models can be scaled, can have fullbright textures, and can be placed with `env_model` and `env_static`.
- Light styles can change during the map, and maps can set their own surface sounds.
- Monster makers pass weapons, squads and spawnflags on to their monsters.
- A new FGD comes with editor icons and models.
- `maxplayers 1` lets you test a map alone, and noclip, god mode and all weapons work with `sv_cheats 1` set in the console.
- The sources of every shipped model and of `co_sbo_showcase` are included

### For server operators

- AFK handling, semiclip and the spectator mode are built into the game.
- `mp_afk_time` sets the delay before the AFK sign shows (60 seconds), and `mp_afk_spectate_time` moves idle players to spectator (off by default).
- `mp_loadouttime` limits the time a player can spend in the loadout menu.
- `mp_missionendtime` sets the time after a mission for the map vote.
- `mp_killfeed_npc` sets which monster kills show in the kill feed.
- `game.cfg` no longer overrides `server.cfg`. In SBO 1.38 the shipped `game.cfg` forced its own values on every map, over the server's settings.
- New `server.cfg` and `listenserver.cfg` files explain every setting, and `mp_hardcore` defaults to 100.
- Older clients are refused with a message asking the player to update.
- `mp_allowmonsters` is removed. In SBO 1.38 the setting worked the wrong way round: 1 removed every monster.
- The level and health penalty needs `mp_XP 1`. The SBO 1.38 Windows server already worked this way, but its Linux server took levels and health even with `mp_XP 0`.
- Plugins need the 2.0 gamedata, which comes with the server files.
- The Linux server library is `dlls/hl.so`.
- All file names are lower case, so `weapon_K444` is now `weapon_k444`. Linux servers treat upper and lower case as different names.
- Plugins can use the new menus and add kill feed icons.
- Rounds, missions and monster kills are written to the server log.
- Servers can have 2048 entities and a message of the day of up to 3072 bytes.
- `game_version` shows the server build.
