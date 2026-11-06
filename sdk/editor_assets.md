# Editor models and sprites

The models and icon sprites that `syndicate.fgd` shows for point entities in the map editor
(`studio("models/editor/...")`, `iconsprite("sprites/editor/...")`). The game does not load them and players do not
need them.

## Where they are

In the game folder, next to the FGD:

    Half-Life/
      syndicate/
        syndicate.fgd
        models/editor/*.mdl
        sprites/editor/*.spr

The FGD's paths resolve from the game folder, so an editor set up for `syndicate` finds them as they are. Game
updates bring new and changed ones.

## Content

- `models/editor/`: models from the goldsrc-fgd collection, and placeholders made by SBO (for example
  `vgui_screen.mdl`, `monster_generic.mdl`, `cycler.mdl` for those classes' `model` key)
- `sprites/editor/`: icons from the goldsrc-fgd collection and icons made by SBO

## Credits

The goldsrc-fgd collection's files come unchanged from the `models/editor/` and `sprites/editor/` folders of the
`goldsrc-fgd` project, which collected them for its SBO definition file. Some of them are third-party work; their
authors, sources and license terms are listed in the SBO developer docs (Editor assets).
