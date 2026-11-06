# Credits

Syndicate Black Ops was originally made by [Streumon Studio](https://streumon-studio.com/).
SBO 2.0.0 is by MrCalvin, built from his decompilation of SBO 1.38 and on the work of the projects below. Thank you all.

Each line names the project's license and links the texts the licenses require. The game code is under the
[Half-Life 1 SDK license](LICENSE); the parts taken from BugfixedHL-Rebased (GPL-3.0), cs16-client and ReSemiclipHL
(GPL-2.0) and the other projects below stay under their own licenses.

## Code we include or adapted

- [Half-Life SDK](https://github.com/ValveSoftware/halflife) (Valve): the base of all game code, and community fixes from its pull requests; Half-Life 1 SDK license
- [hl1_source_sdk](https://github.com/tmp64/hl1_source_sdk) (tmp64): VGUI2, tier1 and Steam API for GoldSrc (submodule `external/hl1_source_sdk`); Source 1 SDK license, third-party notices in its `thirdpartylegalnotices.txt`
- [hl1_source_sdk fork](https://github.com/sabianroberts/hl1_source_sdk) (sabianroberts): two scale helpers for the server browser; Source 1 SDK license
- [Half-Life Updated](https://github.com/twhl-community/halflife-updated) (TWHL community): SDK bug fixes; Half-Life 1 SDK license
- [Half-Life Featureful](https://github.com/FreeSlave/halflife-featureful) (FreeSlave): map entities, object hints, guide entities, monster maker and button options; Half-Life 1 SDK license
- [Spirit of Half-Life](https://github.com/LogicAndTrick/halflife-updated/tree/spirit/spirit-1.8-bug-fixes) (Laurie Cheers; 1.8 on Half-Life Updated by LogicAndTrick): Aurora particle system, locus entities, `env_dlight`, teleport landmarks; Half-Life 1 SDK license
- [SevenKewp](https://github.com/wootguy/SevenKewp) (wootguy): `trigger_changevalue`, `trigger_copyvalue`, `trigger_condition`, map music; Half-Life 1 SDK license
- [ReGameDLL_CS](https://github.com/rehlds/ReGameDLL_CS) (ReGameDLL_CS contributors): spectator, tutor, weather and round handling; MIT ([notice](licenses/ReGameDLL_CS-MIT.txt))
- [BugfixedHL-Rebased](https://github.com/tmp64/BugfixedHL-Rebased) (tmp64): VGUI2 chat, scoreboard, kill feed, voice status, spectator panel, MOTD, Advanced options, console colors, chat flood check; GPL-3.0 ([text](licenses/GPL-3.0.txt))
- [cs16-client](https://github.com/FWGS/cs16-client) (Flying With Gauss): client rain and snow (`environment.cpp`) and its particle manager; GPL-2.0-or-later with the Half-Life linking exception ([text and exception](licenses/GPL-2.0.txt); particle manager: Half-Life SDK license)
- [ReSemiclipHL](https://github.com/rtxa/ReSemiclipHL) (s1lent, Adidasman, rtxa): semiclip; GPL-2.0 ([text](licenses/GPL-2.0.txt))
- [czeror-sdk](https://github.com/twhl-community/czeror-sdk) (TWHL community): `trigger_sequence` and the sequence file handling; Half-Life 1 SDK license
- [Aura-SE](https://github.com/phoenixprojectsoftware/Aura-SE) and [Aura-CL](https://github.com/phoenixprojectsoftware/Aura-CL) (The Phoenix Project Software): `game_playvideo`, the video player, the server browser's design; Half-Life 1 SDK license
- [Zombie Panic! HL](https://github.com/Monochrome-Inc/ZombiePanic-HL) (Monochrome-Inc): console colors method; Half-Life 1 SDK license
- [OpenAG](https://github.com/YaLTeR/OpenAG) (YaLTeR): weapon hand switch (`cl_righthand`); Half-Life 1 SDK license
- [X-Half-Life Deathmatch](https://www.moddb.com/mods/x-half-life-deathmatch) (Xawari): multi-layer overviews, `env_static`, the view model message; Half-Life 1 SDK license
- SBO Source (the Source engine SBO): `game_timer`, the HUD menu look, `vgui_screen` placement; Source 1 SDK license
- [Mapbase](https://github.com/mapbase-source/source-sdk-2013) (Blixibon): the generic timer behind SBO Source's `game_timer`; Source 1 SDK license

## Libraries

- [libogg](https://github.com/xiph/ogg) and [libtheora](https://github.com/xiph/theora) (Xiph.Org Foundation): video decoding; BSD-3-Clause ([ogg](external/libogg/COPYING), [theora](external/libtheora/COPYING))
- [stb_vorbis and stb_image](https://github.com/nothings/stb) (Sean Barrett): video sound and server browser pictures; public domain or MIT (text at the end of each file)
- [Monocypher](https://github.com/LoupVaillant/Monocypher) (Loup Vaillant and others): updater signatures; BSD-2-Clause or CC0 ([text](external/monocypher/LICENCE.md))

## Assets

- [Counter-Strike](https://store.steampowered.com/app/10/) and [Condition Zero](https://store.steampowered.com/app/80/) (Valve): weather sprites, `ambience/rain.wav`, reload sound `fa18_clipin.wav`, tutor sounds and art; © Valve
- [Half-Life Featureful](https://github.com/FreeSlave/halflife-featureful) (FreeSlave): object hint sprite `sprites/hud_brackets.spr`; Half-Life 1 SDK license
- [Spirit of Half-Life 1.9](https://github.com/sohl-modders/Spirit-of-Half-Life-1.9) (sohl-modders): 27 Aurora scripts and `sprites/blacksmoke.spr`; no license stated
- [Boxyt](https://steamcommunity.com/id/hassareth/): AFK sign sprite `sprites/afkstatus_boxyt.spr`
- [Barlow](https://github.com/jpt/barlow) (The Barlow Project Authors): Barlow Condensed SemiBold font; SIL OFL 1.1 ([text](gamedir/syndicate/resource/fonts/ofl-barlow.txt))
- [Liberation Fonts](https://github.com/liberationfonts) (Google, Red Hat): Linux client fonts; SIL OFL 1.1 ([text](gamedir/syndicate/resource/fonts/license-liberation.txt))
- [DejaVu Fonts](https://dejavu-fonts.github.io/) (Bitstream, DejaVu authors): Linux client fonts; Bitstream Vera license, DejaVu changes public domain ([text](gamedir/syndicate/resource/fonts/license-dejavu.txt))
- [GoldSrc Map Editor Entity Icons](https://github.com/Descen/GoldSrc-Map-Editor-Entity-Icons) (Descen; designs after Valve's, Banan2288 and the Sven Co-op team): editor models and sprites; no license stated
- [GoldSrc FGD Collection](https://github.com/twhl-community/GoldSrcFGDCollection) (Erty, TWHL community): the base of `syndicate.fgd`; Half-Life 1 SDK license

## References and ideas

- [Sven Co-op](https://store.steampowered.com/app/225840/) (Sven Co-op team): entity names and behavior of the logic entities
- Go-Mod Reborn: model scale, the Game Master spawn menu layout
- [NextClient](https://github.com/Next21Team) (Next21 Team): server browser filter, view model effects
- [XashXT](https://github.com/FWGS/xashxt) and [PrimeXT](https://github.com/SNMetamorph/PrimeXT): Aurora `lightmodel` and `killcondition` keys (behavior only, no code)
- [Spirinity](https://github.com/edgarbarney/halflife-spirinity) (LogicAndTrick, FranticDreamer): teleport landmark reference
- [discord-rpc](https://github.com/discord/discord-rpc) (Discord): the Rich Presence protocol (no code)
- [TWHL](https://twhl.info/wiki/page/Scripting_with_sequence_files): the sequence file tutorial

## Players

- [VODKA](https://steamcommunity.com/id/VODKA_EYE/): found the K444 crosshair position and the Assassin loadout bug

## Tools

- [xwin](https://github.com/Jake-Shadle/xwin) (Jake Shadle): Windows SDK and CRT for the Linux build
- [ReSDHLT](https://github.com/metita/ReSDHLT) (metita), based on [SDHLT](https://github.com/seedee/SDHLT) (seedee): map compiler for the test maps
