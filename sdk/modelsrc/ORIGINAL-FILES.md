> **Record.** This is `README.md` as it was generated just before the rebuilt models
> were installed into `gamedir/`: the inventory of the original SBO 1.38 model files
> and the comparison of every rebuilt model against them. Not regenerated.

# SBO model sources

Source files (QC script, reference meshes, animations, textures) for the models that ship in
`gamedir/syndicate/models/`, so that the models can be rebuilt with the studiomdlPlusPlus
compiler (studiomdlPlusPlus) and no longer need external
texture files (`nameT.mdl`) or external sequence-group files (`name01.mdl`, `name02.mdl`, ...).

**This file and everything under the category folders are generated** by `scripts/modelsrc.py`
from the shipped models. Do not edit them by hand: change `scripts/modelsrc_rules.json` (or the
script) and run the pipeline again.

Status: the sources were decompiled from the shipped binaries and each one was recompiled and
compared with the shipped model (results below). **Nothing was tested in the game**, and no
shipped `.mdl` has been replaced. The comparison needs a studiomdlPlusPlus build with the
fixes listed under "Compiler fixes this tree depends on".

## Layout

```
sdk/modelsrc/
  monsters/<model>/           anims/  mesh/  textures/  <model>.qc
  players/<model>/            (models/player/<model>/<model>.mdl)
  weapons/<weapon>/v_/        view model     v_<weapon>.qc
  weapons/<weapon>/w_/        world model    w_<weapon>.qc
  weapons/<weapon>/p_/        player-held model  p_<weapon>.qc
  props/<model>/  misc/<model>/
```

Every model folder is self-contained: `mesh/` holds the reference meshes (`$cd ".\mesh"`),
`textures/` the 8-bit BMP textures (`$cdtexture ".\textures"`), `anims/` one SMD per sequence
(`name_blend01.smd`, `name_blend02.smd` for blended sequences). All paths are lower case.
The QC files follow the style of the hand-cleaned SBO model sources (no decompiler comments, one
blank line between blocks, the same order of commands in every file).

To compile one model (the output goes to the folder given with `-o`, never into `gamedir/`):

```bash
studiomdlplusplus sdk/modelsrc/monsters/zgrunt/zgrunt.qc -o /tmp/rebuilt
```

## Pipeline

```bash
scripts/modelsrc.py build   [model ...]   # shipped MDL -> sdk/modelsrc/
scripts/modelsrc.py verify  [model ...]   # sdk/modelsrc/ -> rebuilt MDL in scratch -> comparison
scripts/modelsrc.py readme                # this file
scripts/modelsrc.py all                   # the three steps for every model
```

| File | Role |
| --- | --- |
| `scripts/modelsrc.py` | the pipeline (Python 3 standard library only) |
| `scripts/modelsrc_mdl.py` | MDL reader used for the inventory, the QC files and the comparison |
| `scripts/modelsrc_readme.py` | writes this README |
| `scripts/modelsrc_rules.json` | rules table: folders, texture renames, per-model special cases |

External tools, used as they are (override with the environment variables):

| Variable | Default | Version used for this README |
| --- | --- | --- |
| `MDLDEC` | `mdldec` on PATH | Half-Life Studio Model Decompiler 1.2.1 (FWGS mdldec) |
| `STUDIOMDL` | `studiomdlplusplus` on PATH | `studiomdlplusplus (Oct 12 2026 10:55:22 7818ddb8)` |

Scratch files (decompiler output and logs, rebuilt models, `report.json`) go to
`build/modelsrc/` (git-ignored; `MODELSRC_SCRATCH` overrides). The pipeline never writes under
`gamedir/`. A run with no model names rebuilds every category folder from nothing, so the tree
cannot hold stale files; the same input gives the same output byte for byte.

### What `build` does for each model

1. **Read** `name.mdl`, its texture file `nameT.mdl` and its sequence-group files `nameNN.mdl`
   (found by file name, which is also how the decompiler finds them).
2. **Stage** a scratch copy in which the texture names, sub-model names and sequence names are
   replaced by neutral ones (`tex000.bmp`, `ref000`, `seq000`). mdldec names its output files
   after those names, silently renames the ones it does not like (`&`, `+`, accents) and lets
   repeated names overwrite each other; the neutral names avoid all three problems.
3. **Decompile** the staged copy with `mdldec -a -t -u -m` (`-u`: texel = u x width, the
   convention of studiomdlPlusPlus).
4. **Normalize**: move the files to `mesh/`, `anims/`, `textures/` under their final lower-case
   names and put the final texture names into the mesh files. Each BMP is read back and must
   hold exactly the pixels and palette stored in the model.
5. **Write the QC** from the model itself, in the reference style. mdldec's own QC is not used
   (it stays in scratch): it writes the stored eye position without the axis swap the compiler
   applies to `$eyeposition`, keeps `$externaltextures` and `$sequencegroupsize`, and writes
   texture modes that the compiler derives from the texture name anyway.

### Fixes the pipeline applies

- **Lower-case paths**: every folder, file, texture name and mesh name. Sequence names, body
  group names and bone names are data the game looks up, so they are kept as shipped.
- **One file per model**: no `$externaltextures`, no `$sequencegroup`/`$sequencegroupsize`. The
  textures and all sequences are compiled into `name.mdl`.
- **Zombie Grunt crash fix** (see below): a consequence of the previous point.
- **Texture names**: accents are removed (`pmulticote.bmp`), and the placeholder name `null.bmp`,
  which different models use for different images, becomes `<model>-null.bmp`. The BMP file, the
  mesh files and the QC use the new name consistently. The list of placeholder names is
  `generic_texture_names` in the rules table.
- **Repeated names**: sub-models or sequences that share a name get `_2`, `_3` ... file names
  (`heavyhead_2.smd`, `idle1_2.smd`). The sequence name inside the QC stays as shipped.

Nothing else is changed. Everything unusual that was not fixed before is listed under
"Flagged, not fixed".

### The Zombie Grunt crash fix

The analysis: the shipped `zgrunt.mdl` was
compiled with `$externaltextures` and `$sequencegroupsize`, and its header still names
`models/hgrunt01.mdl` ... `hgrunt03.mdl` as its sequence groups. The engine loads sequence groups
by the name stored in the header, so clients played Heavy Grunt animation data (25 bones) on the
Zombie Grunt skeleton (32 bones): bone corruption and a client crash. The fix was to
recompile the model from its sources as a single file (`zgrunt_fixed.mdl`: no `$externaltextures`,
no `$sequencegroupsize`).

Here the same fix follows from the pipeline: the sources are decompiled from `zgrunt.mdl` +
`zgruntt.mdl` + `zgrunt01-03.mdl` (the Zombie Grunt's own animation files, taken by file name)
and the QC builds one file with no sequence groups. `cyberimp.mdl` names `imp01.mdl` in the same
way (harmless, the files are identical) and comes out as a single file too. `pgrunt.mdl`, an
unused copy of the Zombie Grunt with other textures and the same wrong header, was removed from
`gamedir/` (2026-10-12; nothing referred to it).

## Renamed textures

Only names that changed by more than letter case are listed (every name is lower-cased).

| Model | Shipped name | Source name | Reason |
| --- | --- | --- | --- |
| hgrunt | `pmulticoté.bmp` | `pmulticote.bmp` | accent |
| p_ben | `pepomcotéjoueur.bmp` | `pepomcotejoueur.bmp` | accent |
| p_katana | `tronccoté.bmp` | `tronccote.bmp` | accent |
| p_katana | `null.bmp` | `p_katana-null.bmp` | placeholder name |
| p_katana | `troncdentcoté.bmp` | `troncdentcote.bmp` | accent |
| p_nm33 | `null.bmp` | `p_nm33-null.bmp` | placeholder name |
| p_rc28 | `Rccoté.bmp` | `rccote.bmp` | accent |
| p_rpg | `null.bmp` | `p_rpg-null.bmp` | placeholder name |
| p_shotgun | `pmulticoté.bmp` | `pmulticote.bmp` | accent |
| p_squeak | `null.bmp` | `p_squeak-null.bmp` | placeholder name |
| p_sword | `null.bmp` | `p_sword-null.bmp` | placeholder name |
| p_tripmine | `null.bmp` | `p_tripmine-null.bmp` | placeholder name |
| v_ben | `pepomcotéjoueur.bmp` | `pepomcotejoueur.bmp` | accent |
| v_canon | `null.bmp` | `v_canon-null.bmp` | placeholder name |
| v_canon | `pepomcotéjoueur.bmp` | `pepomcotejoueur.bmp` | accent |
| v_fa18 | `null.bmp` | `v_fa18-null.bmp` | placeholder name |
| v_k444 | `null.bmp` | `v_k444-null.bmp` | placeholder name |
| v_katana | `null.bmp` | `v_katana-null.bmp` | placeholder name |
| v_katana | `tronccoté.bmp` | `tronccote.bmp` | accent |
| v_katana | `troncdentcoté.bmp` | `troncdentcote.bmp` | accent |
| v_mini | `null.bmp` | `v_mini-null.bmp` | placeholder name |
| v_nm33 | `null.bmp` | `v_nm33-null.bmp` | placeholder name |
| v_pm66 | `null.bmp` | `v_pm66-null.bmp` | placeholder name |
| v_rc28 | `RCcoté.bmp` | `rccote.bmp` | accent |
| v_rpg | `null.bmp` | `v_rpg-null.bmp` | placeholder name |
| v_shotgun | `multicoté.bmp` | `multicote.bmp` | accent |
| v_shotgun | `multipoignée.bmp` | `multipoignee.bmp` | accent |
| v_shotgun | `null.bmp` | `v_shotgun-null.bmp` | placeholder name |
| v_squeak | `null.bmp` | `v_squeak-null.bmp` | placeholder name |
| v_sword | `null.bmp` | `v_sword-null.bmp` | placeholder name |
| v_tripmine | `null.bmp` | `v_tripmine-null.bmp` | placeholder name |
| w_nm33 | `null.bmp` | `w_nm33-null.bmp` | placeholder name |
| w_squeak | `null.bmp` | `w_squeak-null.bmp` | placeholder name |
| werewolf | `null.bmp` | `werewolf-null.bmp` | placeholder name |

## Inventory

125 `.mdl` files ship: 81 models, 15 external texture files and 22 sequence-group files that a
model uses, and 7 files that belong to no model (listed after the table).

Columns: *files* = main + texture file (T) + sequence-group files; *size* = all of them, KB;
*body* = body groups / sub-models; *used by* = game source files that name the model, and map entities whose key-values name the model path (scan of the stock and custom maps;
monsters and weapons are normally placed by class name, so this only counts explicit paths).

| Model | What | Source folder | Files | Size | Bones | Seq | Body | Tex | Ext. tex | Seq groups | Used by | Unusual |
| --- | --- | --- | --- | ---: | ---: | ---: | --- | ---: | --- | ---: | --- | --- |
| `ammobox.mdl` | Ammo box (ammo_reserve) | `props/ammobox/` | 1 + T | 120.5 | 1 | 1 | 1 / 1 | 4 | yes | 0 | dlls/ammocaisse.cpp; 2 map entities in 1 map | - |
| `apache.mdl` | Apache helicopter (monster_apache) | `monsters/apache/` | 1 + T | 134.6 | 9 | 1 | 1 / 1 | 23 | yes | 0 | dlls/apache.cpp; 12 map entities in 6 maps | unusual texture names (space, `&`, `+`, parentheses or double extension): `apche_wheel&gun1.bmp` |
| `artefact.mdl` | Artefact obelisk | `props/artefact/` | 1 | 57.5 | 1 | 1 | 1 / 1 | 3 | no | 0 | - | not named in the game code or in any scanned map |
| `barney.mdl` | Barney (monster_barney) | `monsters/barney/` | 1 + T + 3 seq | 394.6 | 24 | 71 | 2 / 4 | 26 | yes | 3 | dlls/barney.cpp; 2 map entities in 1 map | AX/AY/AZ/AXR/AYR/AZR motion flags on 6 sequences (kept: bits in the motion type only) |
| `cyberimp.mdl` | Copy of the imp under another name | `monsters/cyberimp/` | 1 + T + 1 seq | 349.2 | 21 | 24 | 1 / 1 | 1 | yes | 1 | - | not named in the game code or in any scanned map; main file byte-identical to imp.mdl; the header names `models/imp01.mdl` as sequence group file, and the engine loads by that name (not cyberimp01.mdl ...); AX/AY/AZ/AXR/AYR/AZR motion flags on 1 sequence (kept: bits in the motion type only) |
| `dark.mdl` | Dark scientist (monster_dark) | `monsters/dark/` | 1 + T + 7 seq | 697.8 | 22 | 128 | 3 / 7 | 29 | yes | 7 | dlls/dark.cpp; 2 map entities in 1 map | repeated sequence names: crouch_idle3 x2 (kept; files get _2, _3 ...); AX/AY/AZ/AXR/AYR/AZR motion flags on 11 sequences (kept: bits in the motion type only); unusual texture names (space, `&`, `+`, parentheses or double extension): `sci3(back2).bmp`, `sci3(l-leg).bmp`, `sci3(shoe).bmp` and 6 more |
| `dgrunt.mdl` | Dark grunt (dark_grunt) | `monsters/dgrunt/` | 1 + T | 663.2 | 24 | 82 | 3 / 10 | 20 | yes | 0 | dlls/darkgrunt.cpp; 4 map entities in 1 map | repeated sub-model names: heavyhead x4; AX/AY/AZ/AXR/AYR/AZR motion flags on 5 sequences (kept: bits in the motion type only) |
| `hgrunt.mdl` | Human grunt (monster_human_grunt, monster_grunt_hunter) | `monsters/hgrunt/` | 1 + T | 833.1 | 25 | 82 | 3 / 10 | 25 | yes | 0 | dlls/grunt_hunter.cpp, dlls/hgrunt.cpp; 2 map entities in 1 map | repeated sub-model names: heavyhead x4; AX/AY/AZ/AXR/AYR/AZR motion flags on 5 sequences (kept: bits in the motion type only) |
| `hunted.mdl` | Old 'morphbeta' player-style model at models/hunted.mdl | `misc/hunted/` | 1 | 465.0 | 26 | 77 | 1 / 1 | 11 | no | 0 | - | not the player model (that is player/hunted/hunted.mdl); not named in the game code or in any scanned map; sub-model name holds a path: `../dmatch/lowcount/morphbeta/morphbeta_ref` |
| `imp.mdl` | Imp (monster_imp, monster_imp_hunter) | `monsters/imp/` | 1 + T + 1 seq | 349.2 | 21 | 24 | 1 / 1 | 1 | yes | 1 | dlls/imp.cpp, dlls/imp_hunter.cpp; 2 map entities in 1 map | main file byte-identical to cyberimp.mdl; AX/AY/AZ/AXR/AYR/AZR motion flags on 1 sequence (kept: bits in the motion type only) |
| `islave.mdl` | Alien slave (monster_alien_slave, monster_hunter) | `monsters/islave/` | 1 + T + 2 seq | 427.1 | 35 | 30 | 1 / 1 | 2 | yes | 2 | dlls/hunter.cpp, dlls/hunter2.cpp; 2 map entities in 1 map | - |
| `minos.mdl` | Minotaur (monster_cyberminotor) | `monsters/minos/` | 1 + T | 316.2 | 25 | 9 | 1 / 1 | 1 | yes | 0 | dlls/minotaure.cpp | sequence shoot has 3 blends |
| `p_ben.mdl` | ben, player-held model | `weapons/ben/p_/` | 1 | 117.0 | 11 | 1 | 1 / 1 | 3 | no | 0 | dlls/ben.cpp | unusual texture names (space, `&`, `+`, parentheses or double extension): `canon+pompe.bmp` |
| `p_canon.mdl` | canon, player-held model | `weapons/canon/p_/` | 1 | 32.4 | 11 | 1 | 1 / 1 | 1 | no | 0 | dlls/canon.cpp | - |
| `p_fa18.mdl` | fa18, player-held model | `weapons/fa18/p_/` | 1 | 55.1 | 12 | 1 | 1 / 1 | 2 | no | 0 | dlls/fa18.cpp | - |
| `p_federal.mdl` | federal, player-held model | `weapons/federal/p_/` | 1 | 136.6 | 11 | 0 | 1 / 1 | 1 | no | 0 | dlls/federal.cpp | no sequence at all (the sources add a one-frame `idle`) |
| `p_glauncher.mdl` | glauncher, player-held model | `weapons/glauncher/p_/` | 1 | 243.9 | 11 | 0 | 1 / 1 | 2 | no | 0 | dlls/glauncher.cpp | no sequence at all (the sources add a one-frame `idle`) |
| `p_k444.mdl` | k444, player-held model | `weapons/k444/p_/` | 1 | 39.2 | 11 | 1 | 1 / 1 | 1 | no | 0 | dlls/444.cpp | - |
| `p_katana.mdl` | katana, player-held model | `weapons/katana/p_/` | 1 | 42.2 | 15 | 0 | 1 / 1 | 7 | no | 0 | dlls/chainsaw.cpp | no sequence at all (the sources add a one-frame `idle`) |
| `p_medikit.mdl` | medikit, player-held model | `weapons/medikit/p_/` | 1 | 9.7 | 12 | 1 | 1 / 1 | 1 | no | 0 | dlls/medikit.cpp | - |
| `p_mini.mdl` | mini, player-held model | `weapons/mini/p_/` | 1 | 42.3 | 11 | 1 | 1 / 1 | 17 | no | 0 | dlls/chaingun.cpp; 1 map entities in 1 map | - |
| `p_nm33.mdl` | nm33, player-held model | `weapons/nm33/p_/` | 1 | 41.5 | 13 | 1 | 1 / 1 | 1 | no | 0 | dlls/nm33.cpp | - |
| `p_p228.mdl` | p228, player-held model | `weapons/p228/p_/` | 1 | 37.2 | 12 | 1 | 1 / 1 | 1 | no | 0 | - | not named in the game code or in any scanned map |
| `p_pm66.mdl` | pm66, player-held model | `weapons/pm66/p_/` | 1 | 67.1 | 11 | 1 | 1 / 1 | 1 | no | 0 | dlls/pm66.cpp; 2 map entities in 2 maps | - |
| `p_rc28.mdl` | rc28, player-held model | `weapons/rc28/p_/` | 1 | 58.4 | 11 | 1 | 1 / 1 | 5 | no | 0 | dlls/rc28.cpp | - |
| `p_rifle.mdl` | rifle, player-held model | `weapons/rifle/p_/` | 1 | 269.0 | 13 | 1 | 1 / 1 | 1 | no | 0 | dlls/sw2.cpp | - |
| `p_rpg.mdl` | rpg, player-held model | `weapons/rpg/p_/` | 1 | 55.7 | 12 | 1 | 1 / 1 | 1 | no | 0 | dlls/rpg.cpp | - |
| `p_shotgun.mdl` | shotgun, player-held model | `weapons/shotgun/p_/` | 1 | 45.9 | 11 | 1 | 1 / 1 | 4 | no | 0 | dlls/shotgun.cpp, dlls/spa.cpp; 1 map entities in 1 map | - |
| `p_squeak.mdl` | squeak, player-held model | `weapons/squeak/p_/` | 1 | 247.5 | 12 | 1 | 1 / 1 | 1 | no | 0 | dlls/squeakgrenade.cpp | - |
| `p_sword.mdl` | sword, player-held model | `weapons/sword/p_/` | 1 | 39.5 | 11 | 1 | 1 / 1 | 7 | no | 0 | dlls/sword.cpp; 1 map entities in 1 map | - |
| `p_terror.mdl` | terror, player-held model | `weapons/terror/p_/` | 1 | 32.4 | 11 | 1 | 1 / 1 | 1 | no | 0 | - | not named in the game code or in any scanned map |
| `p_tripmine.mdl` | tripmine, player-held model | `weapons/tripmine/p_/` | 1 | 259.9 | 12 | 1 | 1 / 1 | 1 | no | 0 | dlls/tripmine.cpp | - |
| `p_warhammer.mdl` | warhammer, player-held model | `weapons/warhammer/p_/` | 1 | 72.4 | 11 | 1 | 1 / 1 | 1 | no | 0 | dlls/warhammer.cpp | unusual texture names (space, `&`, `+`, parentheses or double extension): `marteaufinal copy.bmp` |
| `p_wong.mdl` | Fist of Legend player-held model: bare hands | none | 1 | 0.7 | | | | | | | dlls/fistoflegend.cpp | **no sources**: intentionally empty (bare hands): nothing to decompile, no sources needed |
| `player/agent/agent.mdl` | Player model: agent (male) | `players/agent/` | 1 | 588.1 | 26 | 81 | 1 / 1 | 4 | no | 0 | dlls/br.cpp, dlls/coop.cpp, dlls/dm.cpp; 2 map entities in 1 map | - |
| `player/agentf/agentf.mdl` | Player model: agent (female) | `players/agentf/` | 1 | 523.7 | 37 | 81 | 1 / 1 | 2 | no | 0 | dlls/br.cpp, dlls/coop.cpp, dlls/dm.cpp; 1 map entities in 1 map | - |
| `player/hunted/hunted.mdl` | Player model: the hunted | `players/hunted/` | 1 | 588.1 | 26 | 81 | 1 / 1 | 4 | no | 0 | dlls/coop.cpp, dlls/dm.cpp | - |
| `robot.mdl` | Cyborg (monster_cyborg) | `monsters/robot/` | 1 + T | 312.6 | 22 | 6 | 1 / 1 | 1 | yes | 0 | dlls/cyborg.cpp; 2 map entities in 1 map | sequence shoot has 3 blends |
| `v_ben.mdl` | ben, view model | `weapons/ben/v_/` | 1 | 159.0 | 60 | 7 | 1 / 1 | 4 | no | 0 | dlls/ben.cpp | unusual texture names (space, `&`, `+`, parentheses or double extension): `canon+pompe.bmp` |
| `v_canon.mdl` | canon, view model | `weapons/canon/v_/` | 1 | 231.9 | 51 | 7 | 1 / 1 | 4 | no | 0 | dlls/canon.cpp | - |
| `v_fa18.mdl` | fa18, view model | `weapons/fa18/v_/` | 1 | 324.1 | 60 | 6 | 1 / 1 | 3 | no | 0 | dlls/fa18.cpp | - |
| `v_federal.mdl` | federal, view model | `weapons/federal/v_/` | 1 | 222.9 | 58 | 4 | 1 / 1 | 2 | no | 0 | dlls/federal.cpp | - |
| `v_glauncher.mdl` | glauncher, view model | `weapons/glauncher/v_/` | 1 | 137.6 | 58 | 6 | 1 / 1 | 3 | no | 0 | dlls/glauncher.cpp | - |
| `v_grenade.mdl` | grenade, view model | `weapons/grenade/v_/` | 1 | 86.8 | 49 | 8 | 2 / 2 | 7 | no | 0 | dlls/handgrenade.cpp | several body groups share the name `body`; unusual texture names (space, `&`, `+`, parentheses or double extension): `glove_handpak.bmp.bmp` |
| `v_k444.mdl` | k444, view model | `weapons/k444/v_/` | 1 | 203.2 | 63 | 8 | 1 / 1 | 2 | no | 0 | dlls/444.cpp | repeated sequence names: idle x2, reload x2, draw x2 (kept; files get _2, _3 ...) |
| `v_katana.mdl` | katana, view model | `weapons/katana/v_/` | 1 | 75.1 | 52 | 7 | 1 / 1 | 8 | no | 0 | dlls/chainsaw.cpp | unusual texture names (space, `&`, `+`, parentheses or double extension): `dent up.bmp` |
| `v_medikit.mdl` | medikit, view model | `weapons/medikit/v_/` | 1 | 145.6 | 47 | 6 | 1 / 1 | 8 | no | 0 | dlls/medikit.cpp | unusual texture names (space, `&`, `+`, parentheses or double extension): `glove_handpak.bmp.bmp` |
| `v_mini.mdl` | mini, view model | `weapons/mini/v_/` | 1 | 85.1 | 48 | 7 | 1 / 1 | 21 | no | 0 | dlls/chaingun.cpp | - |
| `v_nm33.mdl` | nm33, view model | `weapons/nm33/v_/` | 1 | 310.9 | 50 | 5 | 1 / 1 | 1 | no | 0 | dlls/nm33.cpp | - |
| `v_pm66.mdl` | pm66, view model | `weapons/pm66/v_/` | 1 | 280.1 | 51 | 6 | 1 / 1 | 2 | no | 0 | dlls/pm66.cpp | - |
| `v_rc28.mdl` | rc28, view model | `weapons/rc28/v_/` | 1 | 95.9 | 53 | 7 | 1 / 1 | 7 | no | 0 | dlls/rc28.cpp | - |
| `v_rifle.mdl` | rifle, view model | `weapons/rifle/v_/` | 1 | 303.5 | 48 | 6 | 1 / 1 | 2 | no | 0 | dlls/sw2.cpp | - |
| `v_rpg.mdl` | rpg, view model | `weapons/rpg/v_/` | 1 | 219.7 | 33 | 10 | 3 / 3 | 7 | no | 0 | dlls/rpg.cpp | several body groups share the name `body`; unusual texture names (space, `&`, `+`, parentheses or double extension): `glove_handpak.bmp.bmp` |
| `v_satchel.mdl` | satchel, view model | `weapons/satchel/v_/` | 1 | 80.3 | 29 | 4 | 2 / 2 | 9 | no | 0 | dlls/satchel.cpp | several body groups share the name `body`; unusual texture names (space, `&`, `+`, parentheses or double extension): `glove_handpak.bmp.bmp` |
| `v_satchel_radio.mdl` | satchel_radio, view model | `weapons/satchel_radio/v_/` | 1 | 60.0 | 30 | 5 | 2 / 2 | 9 | no | 0 | dlls/satchel.cpp | several body groups share the name `body`; unusual texture names (space, `&`, `+`, parentheses or double extension): `glove_handpak.bmp.bmp` |
| `v_shotgun.mdl` | shotgun, view model | `weapons/shotgun/v_/` | 1 | 98.2 | 56 | 10 | 1 / 1 | 11 | no | 0 | dlls/shotgun.cpp, dlls/spa.cpp | - |
| `v_squeak.mdl` | squeak, view model | `weapons/squeak/v_/` | 1 | 271.1 | 62 | 6 | 1 / 1 | 2 | no | 0 | dlls/squeakgrenade.cpp | - |
| `v_sword.mdl` | sword, view model | `weapons/sword/v_/` | 1 | 63.3 | 46 | 11 | 1 / 1 | 7 | no | 0 | dlls/sword.cpp | - |
| `v_tripmine.mdl` | tripmine, view model | `weapons/tripmine/v_/` | 1 | 321.5 | 46 | 9 | 2 / 4 | 5 | no | 0 | dlls/tripmine.cpp | - |
| `v_warhammer.mdl` | warhammer, view model | `weapons/warhammer/v_/` | 1 | 298.0 | 49 | 11 | 1 / 1 | 2 | no | 0 | dlls/warhammer.cpp | unusual texture names (space, `&`, `+`, parentheses or double extension): `marteaufinal copy.bmp` |
| `v_wong.mdl` | wong, view model | `weapons/wong/v_/` | 1 | 34.5 | 45 | 11 | 1 / 1 | 1 | no | 0 | dlls/fistoflegend.cpp | - |
| `w_ben.mdl` | ben, world model | `weapons/ben/w_/` | 1 | 17.3 | 1 | 1 | 1 / 1 | 1 | no | 0 | dlls/ben.cpp; 6 map entities in 4 maps | - |
| `w_canon.mdl` | canon, world model | `weapons/canon/w_/` | 1 | 17.3 | 1 | 1 | 1 / 1 | 1 | no | 0 | dlls/canon.cpp; 2 map entities in 1 map | - |
| `w_fa18.mdl` | fa18, world model | `weapons/fa18/w_/` | 1 | 53.4 | 1 | 1 | 1 / 1 | 2 | no | 0 | dlls/fa18.cpp; 6 map entities in 2 maps | - |
| `w_federal.mdl` | federal, world model | `weapons/federal/w_/` | 1 | 43.8 | 2 | 1 | 1 / 1 | 1 | no | 0 | dlls/federal.cpp; 4 map entities in 1 map | - |
| `w_glauncher.mdl` | glauncher, world model | `weapons/glauncher/w_/` | 1 | 19.7 | 1 | 1 | 1 / 1 | 1 | no | 0 | dlls/glauncher.cpp | - |
| `w_hache.mdl` | Texture-only file (1 texture, no bones, no bodies) | none | 1 | 24.9 | | | | | | | - | **no sources**: not a model: an orphan external-texture file under a model name |
| `w_k444.mdl` | K444 world model | `weapons/k444/w_/` | 1 | 2.9 | 1 | 1 | 1 / 1 | 1 | missing | 0 | dlls/444.cpp | **texture file does not ship**: the sources use stand-in textures (`dev_placeholder*.bmp`) |
| `w_katana.mdl` | katana, world model | `weapons/katana/w_/` | 1 | 26.1 | 1 | 1 | 1 / 1 | 4 | no | 0 | dlls/chainsaw.cpp | - |
| `w_medikit.mdl` | Medikit world model | `weapons/medikit/w_/` | 1 | 4.1 | 1 | 1 | 1 / 1 | 2 | missing | 0 | dlls/medikit.cpp | **texture file does not ship**: the sources use stand-in textures (`dev_placeholder*.bmp`) |
| `w_mini.mdl` | mini, world model | `weapons/mini/w_/` | 1 | 24.3 | 1 | 1 | 1 / 1 | 1 | no | 0 | dlls/chaingun.cpp | - |
| `w_nm33.mdl` | nm33, world model | `weapons/nm33/w_/` | 1 | 40.0 | 1 | 1 | 1 / 1 | 1 | no | 0 | dlls/nm33.cpp; 3 map entities in 1 map | - |
| `w_p228.mdl` | p228, world model | `weapons/p228/w_/` | 1 | 35.8 | 1 | 1 | 1 / 1 | 1 | no | 0 | - | not named in the game code or in any scanned map; main file byte-identical to w_rc28.mdl |
| `w_pm66.mdl` | pm66, world model | `weapons/pm66/w_/` | 1 | 24.1 | 1 | 1 | 1 / 1 | 1 | no | 0 | dlls/pm66.cpp; 3 map entities in 1 map | - |
| `w_rc28.mdl` | rc28, world model | `weapons/rc28/w_/` | 1 | 35.8 | 1 | 1 | 1 / 1 | 1 | no | 0 | dlls/rc28.cpp | main file byte-identical to w_p228.mdl |
| `w_rifle.mdl` | rifle, world model | `weapons/rifle/w_/` | 1 | 29.7 | 1 | 1 | 1 / 1 | 1 | no | 0 | dlls/sw2.cpp | - |
| `w_squeak.mdl` | Pitbull (monster_snark), world model | `weapons/squeak/w_/` | 1 + T | 250.8 | 14 | 4 | 1 / 1 | 1 | yes | 0 | dlls/squeakgrenade.cpp; 2 map entities in 1 map | - |
| `w_warhammer.mdl` | Warhammer world model | `weapons/warhammer/w_/` | 1 | 4.0 | 1 | 1 | 1 / 1 | 2 | missing | 0 | - | not named in the game code or in any scanned map; **texture file does not ship**: the sources use stand-in textures (`dev_placeholder*.bmp`) |
| `w_weaponbox.mdl` | Weapon box (weaponbox) and satchel world model | `weapons/weaponbox/w_/` | 1 | 21.0 | 1 | 1 | 1 / 1 | 1 | no | 0 | dlls/satchel.cpp, dlls/weapons.cpp | - |
| `werewolf.mdl` | Werewolf (monster_werewolf, monster_werewolf_hunter) | `monsters/werewolf/` | 1 + T + 2 seq | 361.7 | 25 | 30 | 1 / 1 | 1 | yes | 2 | dlls/werewolf.cpp, dlls/werewolf_hunter.cpp; 2 map entities in 1 map | repeated sequence names: idle1 x15, walk1 x5, dieheadshot x4 (kept; files get _2, _3 ...); sequence idle1 has 2 blends but no blend controller type |
| `zgrunt.mdl` | Zombie Grunt (monster_zombie_grunt) | `monsters/zgrunt/` | 1 + T + 3 seq | 494.4 | 32 | 82 | 3 / 10 | 48 | yes | 3 | dlls/zombiegrunt.cpp; 2 map entities in 1 map | Zombie Grunt crash fix applies; the header names `models/hgrunt01.mdl`, `models/hgrunt02.mdl`, `models/hgrunt03.mdl` as sequence group files, and the engine loads by that name (not zgrunt01.mdl ...); AX/AY/AZ/AXR/AYR/AZR motion flags on 5 sequences (kept: bits in the motion type only) |
| `zombie.mdl` | Zombie (monster_zombie, monster_zombie_hunter) | `monsters/zombie/` | 1 + T + 3 seq | 670.0 | 17 | 44 | 1 / 2 | 2 | yes | 3 | dlls/zombies.cpp, dlls/zombies_hunter.cpp; 2 map entities in 1 map | AX/AY/AZ/AXR/AYR/AZR motion flags on 5 sequences (kept: bits in the motion type only) |

Files that no model header refers to:

| File | What it is |
| --- | --- |
| `dgrunt01.mdl` | sequence-group file (IDSQ) that dgrunt.mdl's header does not use (dgrunt.mdl has no sequence groups) |
| `dgrunt02.mdl` | sequence-group file (IDSQ) that dgrunt.mdl's header does not use (dgrunt.mdl has no sequence groups) |
| `dgrunt03.mdl` | sequence-group file (IDSQ) that dgrunt.mdl's header does not use (dgrunt.mdl has no sequence groups) |
| `hgrunt01.mdl` | sequence-group file (IDSQ) that hgrunt.mdl's header does not use (hgrunt.mdl has no sequence groups) |
| `hgrunt02.mdl` | sequence-group file (IDSQ) that hgrunt.mdl's header does not use (hgrunt.mdl has no sequence groups) |
| `hgrunt03.mdl` | sequence-group file (IDSQ) that hgrunt.mdl's header does not use (hgrunt.mdl has no sequence groups) |
| `w_hache.mdl` | texture-only file (one texture, `crowbar.bmp` 192x127), not a model |

## Verification

`scripts/modelsrc.py verify` compiles every source folder with studiomdlPlusPlus into scratch and
compares the result with the shipped model. For a model that shipped in several files the
comparison is against the combined content of those files.

| | Models |
| --- | ---: |
| shipped models | 81 |
| decompiled into sources | 80 |
| rebuilt, equivalent | 74 |
| rebuilt, equivalent except for stand-in textures | 3 |
| rebuilt, intentional difference only (one-frame `idle` added) | 3 |
| rebuilt, with other differences | 0 |
| failed to compile | 0 |
| no sources (intentionally empty model) | 1 |

### How the comparison works

Compared, with the shipped value as the reference: eye position, `$bbox`, `$cbox`, model flags;
bone names, order, parents and controller links; bone controllers; hitboxes; attachments;
texture names (after the renames above), sizes, flags, pixels and palettes; skin families; body
groups, their base values and sub-models; every triangle of every sub-model (bone, vertex
position, texture, texel coordinates, vertex normal); every sequence (name, fps, flags, activity
and weight, events, frame count, blends and blend range, motion type, linear movement, nodes) and
the decoded animation of every bone in every frame; the transition table.

Tolerances: 0.01 units for boxes and attachments; 0.02 units / 0.002 rad for animation values plus
one quantization step of each file (animations are stored as 16-bit steps and the compiler picks
its own step size); 0.02 for normals; 0.5 units for the per-sequence bounding box, which the
compiler recomputes. Texel coordinates must match exactly, except on chrome textures (the
compiler stores none for them, see below). Byte identity is not expected.

Not compared: the internal model name, the order of vertices and triangle strips, per-bone
default values (covered by the animation comparison), unused header fields.

### Results

| Model | Result | Shipped (files, KB) | Rebuilt (KB) | Differences |
| --- | --- | ---: | ---: | --- |
| `ammobox.mdl` | equivalent | 2, 120.5 | 120.3 | - |
| `apache.mdl` | equivalent | 2, 134.6 | 134.4 | - |
| `artefact.mdl` | equivalent | 1, 57.5 | 57.5 | - |
| `barney.mdl` | equivalent | 5, 394.6 | 392.4 | - |
| `cyberimp.mdl` | equivalent | 3, 349.2 | 348.8 | - |
| `dark.mdl` | equivalent | 9, 697.8 | 694.8 | - |
| `dgrunt.mdl` | equivalent | 2, 663.2 | 662.4 | - |
| `hgrunt.mdl` | equivalent | 2, 833.1 | 832.8 | - |
| `hunted.mdl` | equivalent | 1, 465.0 | 464.0 | - |
| `imp.mdl` | equivalent | 3, 349.2 | 348.8 | - |
| `islave.mdl` | equivalent | 4, 427.1 | 426.4 | - |
| `minos.mdl` | equivalent | 2, 316.2 | 316.0 | - |
| `p_ben.mdl` | equivalent | 1, 117.0 | 117.0 | - |
| `p_canon.mdl` | equivalent | 1, 32.4 | 32.4 | - |
| `p_fa18.mdl` | equivalent | 1, 55.1 | 55.1 | - |
| `p_federal.mdl` | intentional difference | 1, 136.6 | 136.9 | intentional: one-frame 'idle' sequence added by the rules table (the shipped model has no sequence) |
| `p_glauncher.mdl` | intentional difference | 1, 243.9 | 244.2 | intentional: one-frame 'idle' sequence added by the rules table (the shipped model has no sequence) |
| `p_k444.mdl` | equivalent | 1, 39.2 | 39.2 | - |
| `p_katana.mdl` | intentional difference | 1, 42.2 | 42.5 | intentional: one-frame 'idle' sequence added by the rules table (the shipped model has no sequence) |
| `p_medikit.mdl` | equivalent | 1, 9.7 | 9.7 | - |
| `p_mini.mdl` | equivalent | 1, 42.3 | 42.3 | - |
| `p_nm33.mdl` | equivalent | 1, 41.5 | 41.5 | - |
| `p_p228.mdl` | equivalent | 1, 37.2 | 37.2 | - |
| `p_pm66.mdl` | equivalent | 1, 67.1 | 67.1 | - |
| `p_rc28.mdl` | equivalent | 1, 58.4 | 58.3 | - |
| `p_rifle.mdl` | equivalent | 1, 269.0 | 269.0 | - |
| `p_rpg.mdl` | equivalent | 1, 55.7 | 55.6 | - |
| `p_shotgun.mdl` | equivalent | 1, 45.9 | 45.9 | - |
| `p_squeak.mdl` | equivalent | 1, 247.5 | 247.5 | - |
| `p_sword.mdl` | equivalent | 1, 39.5 | 39.5 | - |
| `p_terror.mdl` | equivalent | 1, 32.4 | 32.4 | - |
| `p_tripmine.mdl` | equivalent | 1, 259.9 | 259.9 | - |
| `p_warhammer.mdl` | equivalent | 1, 72.4 | 72.4 | - |
| `p_wong.mdl` | no sources | 1, 0.7 | | intentionally empty (bare hands): nothing to decompile, no sources needed |
| `player/agent/agent.mdl` | equivalent | 1, 588.1 | 588.0 | - |
| `player/agentf/agentf.mdl` | equivalent | 1, 523.7 | 523.7 | - |
| `player/hunted/hunted.mdl` | equivalent | 1, 588.1 | 588.0 | - |
| `robot.mdl` | equivalent | 2, 312.6 | 312.1 | - |
| `v_ben.mdl` | equivalent | 1, 159.0 | 159.1 | - |
| `v_canon.mdl` | equivalent | 1, 231.9 | 232.0 | - |
| `v_fa18.mdl` | equivalent | 1, 324.1 | 324.0 | - |
| `v_federal.mdl` | equivalent | 1, 222.9 | 222.9 | - |
| `v_glauncher.mdl` | equivalent | 1, 137.6 | 137.5 | - |
| `v_grenade.mdl` | equivalent | 1, 86.8 | 85.0 | - |
| `v_k444.mdl` | equivalent | 1, 203.2 | 203.1 | - |
| `v_katana.mdl` | equivalent | 1, 75.1 | 75.2 | - |
| `v_medikit.mdl` | equivalent | 1, 145.6 | 145.5 | - |
| `v_mini.mdl` | equivalent | 1, 85.1 | 85.3 | - |
| `v_nm33.mdl` | equivalent | 1, 310.9 | 310.8 | - |
| `v_pm66.mdl` | equivalent | 1, 280.1 | 280.1 | - |
| `v_rc28.mdl` | equivalent | 1, 95.9 | 96.0 | - |
| `v_rifle.mdl` | equivalent | 1, 303.5 | 303.6 | - |
| `v_rpg.mdl` | equivalent | 1, 219.7 | 219.0 | - |
| `v_satchel.mdl` | equivalent | 1, 80.3 | 79.4 | - |
| `v_satchel_radio.mdl` | equivalent | 1, 60.0 | 60.0 | - |
| `v_shotgun.mdl` | equivalent | 1, 98.2 | 98.2 | - |
| `v_squeak.mdl` | equivalent | 1, 271.1 | 271.2 | - |
| `v_sword.mdl` | equivalent | 1, 63.3 | 63.4 | - |
| `v_tripmine.mdl` | equivalent | 1, 321.5 | 321.5 | - |
| `v_warhammer.mdl` | equivalent | 1, 298.0 | 298.1 | - |
| `v_wong.mdl` | equivalent | 1, 34.5 | 34.6 | - |
| `w_ben.mdl` | equivalent | 1, 17.3 | 17.3 | - |
| `w_canon.mdl` | equivalent | 1, 17.3 | 17.3 | - |
| `w_fa18.mdl` | equivalent | 1, 53.4 | 53.4 | - |
| `w_federal.mdl` | equivalent | 1, 43.8 | 43.8 | - |
| `w_glauncher.mdl` | equivalent | 1, 19.7 | 19.7 | - |
| `w_k444.mdl` | equivalent; **stand-in textures** | 1, 2.9 | 67.8 | - |
| `w_katana.mdl` | equivalent | 1, 26.1 | 26.1 | - |
| `w_medikit.mdl` | equivalent; **stand-in textures** | 1, 4.1 | 22.2 | - |
| `w_mini.mdl` | equivalent | 1, 24.3 | 24.3 | - |
| `w_nm33.mdl` | equivalent | 1, 40.0 | 40.0 | - |
| `w_p228.mdl` | equivalent | 1, 35.8 | 35.8 | - |
| `w_pm66.mdl` | equivalent | 1, 24.1 | 24.1 | - |
| `w_rc28.mdl` | equivalent | 1, 35.8 | 35.8 | - |
| `w_rifle.mdl` | equivalent | 1, 29.7 | 29.7 | - |
| `w_squeak.mdl` | equivalent | 2, 250.8 | 250.6 | - |
| `w_warhammer.mdl` | equivalent; **stand-in textures** | 1, 4.0 | 6.6 | - |
| `w_weaponbox.mdl` | equivalent | 1, 21.0 | 21.0 | - |
| `werewolf.mdl` | equivalent | 4, 361.7 | 360.7 | - |
| `zgrunt.mdl` | equivalent | 5, 494.4 | 493.5 | - |
| `zombie.mdl` | equivalent | 5, 670.0 | 668.9 | - |

Largest deviations among the equivalent models: animation position 0.0457 units (dark), animation
rotation 0.000096 rad (zombie), vertex normal 0.0000010 (v_nm33). Vertex and normal table sizes, triangle
counts, texture order and bone order are identical for every rebuilt model.

### Differences

No unintended difference.

**Intentional difference** (`p_federal`, `p_glauncher`, `p_katana`): the shipped models have no sequence at all; the sources add
a one-frame `idle` in the reference pose (rule `add_idle_sequence`).
Reasons: every stock Half-Life `p_` model and the other 18 SBO `p_` models with geometry have a
sequence (usually exactly this one-frame `idle`); the client's `StudioMergeBones` always reads
sequence 0 of the attached weapon model, and with no sequence it reads the sequence-group
record that sits at the same offset and computes bone poses from unrelated bytes (unused only
because every bone of these models exists in the player skeletons); and the compiler refuses
a QC without a sequence. Everything else in these three models is equivalent.

`w_k444`, `w_medikit` and `w_warhammer` are equivalent in everything the shipped file holds
(bones, hitboxes, meshes with their texel coordinates, sequences); their textures are stand-ins,
see "Stand-in textures".

### Compiler fixes this tree depends on

The first comparison (compiler build 286a2e2c) had 8 models with differences. Two causes were
bugs in studiomdlPlusPlus, fixed there on 2026-10-12, as was a third deviation from Valve's
compiler:

1. **`LY` motion extraction** (2dbb2e0; `dgrunt`, `hgrunt`, `zgrunt`: `strafeleft`, `straferight`):
   the sideways linear movement subtracted the start **z** from the end **y**
   (`motion.y = ppos[last_frame].y - ppos[start_frame].z`), a slip from a clean-up commit; Valve's
   `ExtractMotion` uses the same axis.
2. **Flags from the texture name `bright`** (257e572; `v_satchel_radio`, `detbutn_bright.bmp`): the
   compiler set flat shading + full bright for any name containing `bright`. Valve's studiomdl has
   no such rule (only `chrome`); it came with the code base this compiler started from, where it
   is marked as an XDM addition. Removed; `$texrendermode <texture> fullbright` remains.

3. **Motion options AX, AY, AZ, AXR, AYR, AZR** (`barney`, `cyberimp`, `dark`, `dgrunt`, `hgrunt`,
   `imp`, `zgrunt`, `zombie`): the compiler accepted and dropped them (removed upstream in 2025 as
   "unused"). Valve's studiomdl stores them as bits in the sequence's motion type and writes
   nothing else for them; the compiler now does the same, so the motion type is compared
   strictly and matches the shipped files. (Not counted among the 8: the first comparison
   tolerated the missing bits.)

The remaining cause was the three `p_` models without a sequence, see the intentional difference
above. (The compiler's "No sequences defined" check is its own; Valve's studiomdl has none. It
stays.)

Expected changes that are not counted as differences:

- Texture and sub-model names are lower case (and renamed as listed above); the sub-model name
  `../dmatch/lowcount/morphbeta/morphbeta_ref` of `hunted.mdl` becomes `morphbeta_ref`; repeated
  sub-model names get `_2`, `_3`, `_4` (`heavyhead` in dgrunt and hgrunt).
- Texel coordinates on chrome textures are not stored by the compiler. For `p_fa18`, `v_fa18`, `v_grenade`, `v_satchel`, `v_satchel_radio`, `w_fa18` the
  shipped file has real coordinates on textures that carry the chrome flag without the usual
  flat-shade flag (the flag was probably added to a compiled model). A renderer generates the
  coordinates of a chrome texture itself, so the stored ones should not matter while the flag is
  set (Valve's studiomdl stores none either; not verified in Valve's engine). The mesh files in
  `mesh/` keep the coordinates.
- The internal model name (a path on the original author's PC) becomes `<name>.mdl`.

## Stand-in textures (to do)

`w_k444.mdl`, `w_medikit.mdl` and `w_warhammer.mdl` have no textures inside and their texture
files (`w_k444t.mdl` ...) do not ship, so the original texture names, sizes and images are
unknown. Weapons are rarely placed in the world in SBO, which is probably why nobody noticed.
So that these three have sources that compile, the pipeline gives them generated stand-in
textures (rule `placeholder_textures`): a gray checker with a magenta frame and the word DEV,
named `dev_placeholder.bmp` (`dev_placeholder_2.bmp` for a second skin reference). One texture
per skin reference used by the meshes; each is the smallest power-of-two size that covers the
texel coordinates stored in those meshes (a power-of-two texture is not resampled by the engine):

| Model | Skin reference | Largest texel (s, t) | Stand-in |
| --- | ---: | --- | --- |
| `w_k444` | 0 | 248, 136 | `dev_placeholder.bmp` 256x256 |
| `w_medikit` | 0 | 71, 71 | `dev_placeholder.bmp` 128x128 |
| `w_medikit` | 1 | 54, 7 | `dev_placeholder_2.bmp` 64x8 |
| `w_warhammer` | 0 | 0, 63 | `dev_placeholder.bmp` 8x64 |
| `w_warhammer` | 1 | 0, 63 | `dev_placeholder_2.bmp` 8x64 |

- **TODO**: replace these three with real world models from the rotating-weapons project
  (the rotating-weapons plugin's meshes), then drop the `placeholder_textures` rules.
  **Done since this record** (merge `dfac270`): they are replaced by world models made from those meshes and
  the stand-in rule is gone; see "World models (`w_`)" in `README.md`.
- The shipped `w_k444.mdl`, `w_medikit.mdl`, `w_warhammer.mdl` in `gamedir/` are unchanged (still
  without textures). The rebuilt files with the stand-ins exist only in scratch.
- Every texel coordinate of `w_warhammer` is (0, 63), which is what a compiler writes for chrome
  textures: its real textures were probably 64x64 chrome. `w_medikit.mdl` is byte-identical to
  stock Half-Life's `w_medkit.mdl`, whose textures are in `valve/models/w_medkitt.mdl`.

## Engine limits for the merged models

Limits from `src/engine/studio.h` (same values as Valve's SDK): MAXSTUDIOSEQUENCES 2048, MAXSTUDIOSKINS 100,
MAXSTUDIOBONES 128, MAXSTUDIOVERTS 2048 per sub-model, MAXSTUDIOANIMATIONS (frames per sequence) 2048,
MAXSTUDIOEVENTS 1024, MAXSTUDIOGROUPS 16 (no longer relevant: one group). The compiler uses the same
numbers (`src/format/mdl.hpp`). There is no limit on the file size itself in `studio.h`.

| Model | Shipped files | Shipped KB | Rebuilt KB | Seq | Longest seq (frames) | Tex | Texture pixels (KB) | Largest texture | Bones | Most vertices in a sub-model |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| `ammobox` | main + T | 120.5 | 120.3 | 1 | 101 | 4 | 115.6 | 256x175 | 1 | 8 |
| `apache` | main + T | 134.6 | 134.4 | 1 | 7 | 23 | 85.1 | 124x80 | 9 | 506 |
| `barney` | main + T + 3 seq | 394.6 | 392.4 | 71 | 251 | 26 | 78.4 | 92x109 | 24 | 382 |
| `cyberimp` | main + T + 1 seq | 349.2 | 348.8 | 24 | 61 | 1 | 256.0 | 512x512 | 21 | 399 |
| `dark` | main + T + 7 seq | 697.8 | 694.8 | 128 | 206 | 29 | 95.6 | 96x97 | 22 | 241 |
| `dgrunt` | main + T | 663.2 | 662.4 | 82 | 181 | 20 | 305.8 | 496x238 | 24 | 221 |
| `hgrunt` | main + T | 833.1 | 832.8 | 82 | 181 | 25 | 461.1 | 504x256 | 25 | 221 |
| `imp` | main + T + 1 seq | 349.2 | 348.8 | 24 | 61 | 1 | 256.0 | 512x512 | 21 | 399 |
| `islave` | main + T + 2 seq | 427.1 | 426.4 | 30 | 65 | 2 | 244.0 | 512x416 | 35 | 295 |
| `minos` | main + T | 316.2 | 316.0 | 9 | 42 | 1 | 256.0 | 512x512 | 25 | 631 |
| `robot` | main + T | 312.6 | 312.1 | 6 | 45 | 1 | 256.0 | 512x512 | 22 | 684 |
| `w_squeak` | main + T | 250.8 | 250.6 | 4 | 101 | 1 | 233.0 | 488x489 | 14 | 254 |
| `werewolf` | main + T + 2 seq | 361.7 | 360.7 | 30 | 31 | 1 | 256.0 | 512x512 | 25 | 489 |
| `zgrunt` | main + T + 3 seq | 494.4 | 493.5 | 82 | 181 | 48 | 115.2 | 256x88 | 32 | 261 |
| `zombie` | main + T + 3 seq | 670.0 | 668.9 | 44 | 121 | 2 | 512.0 | 512x512 | 17 | 337 |

- Nothing is near a limit: at most 128 of 2048 sequences, 48 of 100 textures, 35 of 128 bones, 684 of
  2048 vertices, 251 of 2048 frames. The largest rebuilt file is 832.8 KB; the player models already
  ship as single files of 588.1 KB, so single files of this size are nothing new for the mod.
- **Verified**: every merged model compiles; the rebuilt file parses completely; every sequence
  decodes from the single file and matches the shipped animation (this also covers the 16-bit
  animation offsets, which are relative to each bone's entry and limit how far a sequence's data
  can reach); textures are identical in size, pixels and palette, so texture memory in the
  renderer should be unchanged (same images; only the file they are read from differs).
- **Behavior that changes**: sequence groups were loaded on demand through the engine cache;
  in a single file all animations are in memory with the model. That is the whole sequence data
  of the model (at most 408.1 KB in sequence-group files, `dark`), a saving that mattered in 1998.
- **Not verified**: loading and rendering in the engine (Steam Half-Life client, HLDS), and the
  compiler's behavior on Windows. The engine's own hard limits beyond `studio.h` (cache and hunk
  sizes) were not measured; stock Half-Life models larger than these load from single files.

## Flagged, not fixed

Known issues of the shipped models, not fixed. The sources reproduce the shipped models here.
`.smd` files are plain text and go into normal git.

- **`cyberimp.mdl`** is a byte-for-byte copy of `imp.mdl` and loads `imp01.mdl` (identical to
  `cyberimp01.mdl`). Not named in the game code.
- **Missing texture files** of `w_k444`, `w_medikit`, `w_warhammer`: see "Stand-in textures".
  `w_k444.mdl` and `w_medikit.mdl` are precached by the game code. `w_hache.mdl` is a texture-only
  file holding `crowbar.bmp` 192x127; it does not fit `w_warhammer` (two skin references).
- **Orphan files** (table above): `dgrunt01-03.mdl` and `hgrunt01-03.mdl` belong to no model;
  `hgrunt01-03.mdl` are what the shipped `zgrunt.mdl` wrongly loads.
- **Unusual texture names kept as shipped**: spaces (`marteaufinal copy.bmp`, `dent up.bmp`), a
  double extension (`glove_handpak.bmp.bmp`), `&`, `+` and parentheses (`apche_wheel&gun1.bmp`,
  `canon+pompe.bmp`, `sci3(back2).bmp` ...). They compile.
- **Other texture names shared by different images**: only `null.bmp` is renamed. 46 more names
  are used by several models for different images (mostly the same picture cropped differently
  for the `v_` and `p_` model: `skin.bmp`, `image3.bmp`, `bout_1.bmp`, `epee1.bmp` ...). Every model
  folder has its own `textures/`, so nothing collides; add names to `generic_texture_names` to
  rename more.
- **`dm_base.bmp`** in `v_medikit` was `DM_base.bmp`. The engine treats `DM_Base.bmp` specially on
  player models (team colors); this is a view model and the shipped spelling already differed
  from the engine's, so no effect is expected (not verified in the engine).
- **Glove textures with the chrome flag** in `v_grenade`, `v_satchel`, `v_satchel_radio`
  (`gloved_knuckle.bmp`, `glove_handpak.bmp.bmp`, `xbow_sleeve.bmp`, `gloved_sleeve.bmp`) and
  `fa18p_02.bmp`/`fa18_02.bmp`: chrome without flat shading, kept as shipped.
- **`werewolf.mdl`**: 15 sequences named `idle1`, 5 `walk1`, 4 `dieheadshot`; `idle1` (the first)
  has two blends and no blend controller. `dark.mdl` and `v_k444.mdl` repeat names too. The game
  finds the first sequence with a given name; the order is kept.
- **`hunted.mdl`** in the models root is an old `morphbeta` model that nothing names; the player
  model is `player/hunted/hunted.mdl`. `w_p228.mdl` and `w_rc28.mdl` are the same file.
- `p_wong.mdl` is intentionally empty (bare hands): no sources, nothing to fix.
- The `.bmp` portraits in `models/player/*/` are not model sources and are not part of this tree.

## Size of this tree

| Type | Files | MB |
| --- | ---: | ---: |
| `.smd` | 1504 | 88.4 |
| `.bmp` | 418 | 10.0 |
| `.qc` | 80 | 0.1 |
| total | 2002 | 98.6 |

`sdk/.gitattributes` already sends `*.bmp` to Git LFS, so the textures here go to LFS with no
change. The `.smd` files are text and are stored as normal git objects (the repo keeps every file
byte for byte, `* -text`); they are the bulk of the tree and compress well.
