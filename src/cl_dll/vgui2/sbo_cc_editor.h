//========= SBO VGUI2: the color correction editor =========//
//
// Purpose: SBO's in-game Color Correction Tools (after Source's colorcorrectionui), opened with the console command
//          colorcorrectionui in single player, on a listen server the player hosts, or with sv_cheats 2
//          (docs/COLOR_CORRECTION.md "Editor"):
//            * the main window: Enable (the editor's look on the view), Enable Entities (the map's color_correction
//              entities too), Lookup View, Blend (how strongly the look is shown), the ordered list of operations
//              (New, Delete, Up, Down; a click on an icon turns an operation on or off; a double click opens it) and
//              Save;
//            * a window per operation: Curves, Levels, Selected HSV, Color Balance, Lookup
//              (colorcorrection_ops.h); every change shows at once on the view;
//            * the lookup view: the table as Source shows it, its blue slices side by side;
//            * Save: gfx/colorcorrection/<name>.raw in the mod folder, the Source .raw file a color_correction or
//              color_correction_volume reads with its Lookup Table File key.
//          Built from the generic tool controls in controls/ (sbo_tool_window.h, sbo_value_controls.h,
//          sbo_list_controls.h, sbo_graph_controls.h). SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_CC_EDITOR_H
#define SBO_VGUI2_SBO_CC_EDITOR_H

#include "vgui2_api.h" // VGui2_CcEditor_Init

#endif // SBO_VGUI2_SBO_CC_EDITOR_H
