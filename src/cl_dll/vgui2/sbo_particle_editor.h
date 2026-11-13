//========= SBO VGUI2: the particle editor =========//
//
// Purpose: SBO's in-game editor for Aurora particle scripts (.aur, docs/AURORA.md "Editor"), opened with the console
//          command particleeditorui in single player, on a listen server the player hosts, or with sv_cheats 2
//          (the color correction editor's rule, SboToolsAllowed):
//            * the main window: the script's top-level keys (Particles, Main type, Light model, Kill condition), the
//              preview (on / off, In front, Place on the screen, Restart, the emitter's pitch and yaw), the list of
//              particle types (New, Delete, Up, Down; a double click opens one), Load and Save;
//            * a window per type: its name and every key of the web editor's key list (aurora_keys.h, from
//              tools/aurora-editor/core/aurora-keys.json), one group at a time, with the list's names, spans and
//              defaults; a right click on a slider takes the key out (the game's default);
//            * the sprite window: the sprites in the game's folders (SBO's, Half-Life's, downloaded); only the one
//              picked is loaded;
//            * Load / Save: aurora/<name>.aur, the text the game reads (aurora_doc.h keeps comments and other keys).
//          The preview is a real Aurora system (aurora/aurora_editor.h). Built from the generic tool controls in
//          controls/. SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_PARTICLE_EDITOR_H
#define SBO_VGUI2_SBO_PARTICLE_EDITOR_H

#include "vgui2_api.h" // VGui2_ParticleEditor_Init

#endif // SBO_VGUI2_SBO_PARTICLE_EDITOR_H
