//========= SBO VGUI2: the world screen editor =========//
//
// Purpose: SBO's in-game editor for world screen layouts (ui/scripts/screens/<name>.res, docs/WORLD_SCREENS.md
//          "Editor"), opened with the console command worldscreeneditorui in single player, on a listen server the
//          player hosts, or with sv_cheats 2 (the other editors' rule, SboToolsAllowed):
//            * the main window: Load, Save, Undo, Add a control, Delete, To front, the screen in the world that shows
//              the changes (the one in view, or one picked with a click), and the layout itself at its real pixel
//              size, where a click selects a control, a drag moves it and the squares on its corners and edges resize it;
//            * the settings window: the selected control's settings in plain names (position, size, text, font, text
//              position, colors with the color wheel, command, picture, bar values, shown at the start);
//            * Load / Save: ui/scripts/screens/<name>.res, the text the game reads (worldscreen_layout_doc.h keeps
//              comments and other keys).
//          The world screens that show the layout (by name, and the one picked) are built from the editor's text
//          while it is open (worldscreen::EditorPreview), and from their files again when it closes. Animation
//          scripts (_anims.txt) are not edited here. Built from the generic tool controls in controls/. SDK world
//          (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_WORLDSCREEN_EDITOR_H
#define SBO_VGUI2_SBO_WORLDSCREEN_EDITOR_H

#include "vgui2_api.h" // VGui2_WorldScreenEditor_Init

#endif // SBO_VGUI2_SBO_WORLDSCREEN_EDITOR_H
