# Kill feed icons (`d_<weapon>`)

Sources of the kill feed (death notice) icons: one PNG per icon, a white picture on transparency. The weapon
icons are 192 pixels high, face right (toward the victim) and are made from the owner's weapon artwork (PSD files,
not in the repository); `tank.png` is the minigun on a tripod (`func_tank`). `headshot.png` is Counter-Strike
1.6's `d_headshot` (`cstrike/sprites/640hud1.spr`, 36x16), kept pixel for pixel. `gm.png` (`d_gm`, a Game
Master's monster's kill: the artifact pyramid with an eye) is drawn by hand: it is its own source, and
`killfeed-sources` keeps it. Every icon points at the victim, to the right (`flop` in the table mirrors artwork that
faces left). Weapons without the owner's artwork keep their old icon: the crossbow
(`d_crossbow`, `d_bolt`) uses Half-Life's, from `320hud1` / `640hud1`.

The game uses `sprites/killfeed.spr` (one 256x128 sheet, used by the 320 and 640 sets) and the `d_<name>` lines
of `sprites/hud.txt`. `<name>` is the code the server sends in `DeathMsg` / `SboDeathMsg`: the weapon's classname
without `weapon_`, or the projectile's / entity's classname without `monster_` / `func_` (`rpg_rocket`,
`grenade`, `satchel`, `tripmine`, `snark`, `tank`). Every icon is 16 pixels high, as Half-Life's.

## Regenerate

Needs Python 3 and ImageMagick (`convert`).

```
# sheet + hud.txt from the PNGs here (the usual case)
scripts/gen_hud_weapon_sprites.py killfeed [--review <dir>]

# PNGs from the artwork (only when the artwork or the KILLFEED table changes);
# --cstrike <Counter-Strike's cstrike folder> also cuts headshot.png again
scripts/gen_hud_weapon_sprites.py killfeed-sources <art-dir> [--cstrike <dir>]

# a larger sprite set (HL25: 1280 = 2x, 2560 = 3x), not shipped: writes <dir>/killfeed_1280.spr, prints its
# hud.txt lines
scripts/gen_hud_weapon_sprites.py killfeed --scale 2 --set 1280 --out <dir>
```

The `KILLFEED` table in the script lists every icon, its source and options (mirror, crop, cut-outs), and which
entries share a picture (`grenade` = hand grenade, `rpg_rocket` = RPG, `tanklaser` = tank). Add a new icon at the
end of the table, run `killfeed`, and check the contact sheet from `--review` (every `d_` icon at 1x and 4x, white
and HUD orange). Running it again gives the same files. Only weapons with the owner's artwork get an icon.

## Plugin icons (`sbo_kf_custom_v<N>.spr`)

`quadbarrel.png`, `hhgrenade.png` and `sentry.png` are the server plugins' icons. They are
not in `sprites/killfeed.spr`: they go in one sheet of their own, `sprites/sbo_kf_custom_v<N>.spr`, that ships with
the plugins (amxmodx-plugins `server/syndicate/killfeed_icons/sprites/`) and is downloaded from the server. Their
sources are made from the plugins' model meshes: the side silhouette of the SMD reference mesh, with a few drawn
details (the quad barrel's barrel line, stock break, trigger guard and muzzle bands), listed in the script's
`KF_CUSTOM` table.

```
# PNGs from the models (only when a model or the KF_CUSTOM table changes)
scripts/gen_hud_weapon_sprites.py killfeed-custom-sources <amxmodx-plugins checkout>

# the sheet and the rectangles in include/killfeed_icons.inc; every change gets the next version number
scripts/gen_hud_weapon_sprites.py killfeed-custom --version <N> \
    --out <amxmodx-plugins>/server/syndicate/killfeed_icons/sprites --inc <amxmodx-plugins>/include/killfeed_icons.inc
```
