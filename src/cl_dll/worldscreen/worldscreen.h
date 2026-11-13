//========= SBO world-space VGUI2 screens =========//
//
// Purpose: The map entity vgui_screen (server: src/dlls/vgui_screen.cpp) on the client: a VGUI2 panel, built from a
//          .res layout, rendered into an OpenGL texture (framebuffer object) and drawn on a quad in the 3D world,
//          like Source's vgui_screen. Players aim at it and click its buttons with the attack keys. Windows and
//          Linux (HL25, OpenGL). docs/WORLD_SCREENS.md.
//
// Pipeline:
//   1. The server's WScreen message (worldscreen.cpp) defines a screen: entity index, corner, angles, size, input
//      distance, flags, panel name. Each screen gets a slot (WORLDSCREEN_MAX); VGui2_WorldScreen_Create builds its
//      panel from ui/scripts/screens/<panelname>.res (vgui2/world_screen_panel.cpp). New map: all removed (InitHUD).
//   2. CWorldScreenHost (one per slot, a child of CClientViewport) paints its content panel into the slot's FBO
//      during the engine's VGUI2 paint pass (wsgl::BeginRender/EndRender): never onto the screen. It repaints at
//      worldscreen_rate Hz, at once after hover/click/control changes, only when a 3D view of the same frame saw
//      the screen (5.), and at most WORLDSCREEN_MAX_REPAINTS_PER_FRAME screens a frame.
//   3. HUD_DrawNormalTriangles (tri.cpp) -> WorldScreen_DrawWorld(): textured quads in world space (depth tested).
//   4. Interaction (like Source's C_VGuiScreen): WorldScreen_DrawWorld traces the view ray (v_origin / v_angles)
//      against the active screens; the nearest front-side hit whose plane is within the screen's input distance
//      is the aimed screen and gives the panel pixel -> VGui2_WorldScreen_SetCursor (the button under it is armed).
//      input.cpp IN_AttackDown / IN_Attack2Down -> WorldScreen_Attack: while aimed at a screen, the press and its
//      release go to the panel (left / right click) instead of firing. A button clicked (released over it) sends
//      its command to the server: "vgui_screen_command <entindex> <command>".
//      CHud::Redraw -> WorldScreen_Frame: the cursor at the center of the view (vgui2 CWorldScreenCursor) and no
//      crosshair while aimed at a screen.
//   Bound screens (vgui_screen "surface"): drawn on a brush entity's {screen faces instead of a rectangle; the
//      faces come from the .bsp (worldscreen_bsp.cpp) and follow the moving entity (worldscreen.cpp).
//   5. Visibility (worldscreen.cpp): per 3D view, a quad is drawn only from its front side, inside the view
//      frustum (triangle API WorldToScreen) and in the view's PVS (BoxInPVS). Seen that way, within
//      worldscreen_update_dist and not hidden by the world (traces at the update rate), it marks itself seen;
//      its host repaints only then (worldscreen::ConsumeSeen).
//   GL state: worldscreen_gl.h.
//
// Plain C++ only: included from both header worlds (SBO world: worldscreen.cpp, tri.cpp, hud.cpp, input.cpp, ...;
// SDK world: vgui2/world_screen_panel.cpp).
//===============================================================//

#ifndef SBO_WORLDSCREEN_H
#define SBO_WORLDSCREEN_H

#define WORLDSCREEN_MAX 16                     // screens a client shows at once (more in a map: ignored, console)
#define WORLDSCREEN_MAX_PIXELS 1024            // panel size limit (and the VGUI2 screen size)
#define WORLDSCREEN_DEFAULT_WIDE 512           // panel size without "wide" / "tall" in the layout
#define WORLDSCREEN_DEFAULT_TALL 256
#define WORLDSCREEN_MAX_REPAINTS_PER_FRAME 2   // screens repainted in one frame; the others wait a frame
#define WORLDSCREEN_MAX_MOVIE_DECODES 2        // movie screens decoded at once (the nearest seen); the others pause

//-----------------------------------------------------------------------------
// Called from SBO code (implemented in worldscreen.cpp, SBO world)
//-----------------------------------------------------------------------------
// hud.cpp CHud::Init: cvars, commands, the WScreen message, the VGUI2 part (if VGUI2 is up).
void WorldScreen_Init();
// hud_msg.cpp InitHUD (new map or server): removes all screens; the server sends the map's after it.
void WorldScreen_Reset();
// tri.cpp HUD_DrawNormalTriangles: aim + hover update, visibility, draws the screen quads.
void WorldScreen_DrawWorld();
// hud_redraw.cpp CHud::Redraw, once a frame: cursor and crosshair, the repaint budget.
void WorldScreen_Frame();
// input.cpp IN_Attack(2)Down / Up: button 0 = attack (left click), 1 = attack2 (right click). True: the screen
// took the press (or its release); the caller does nothing else (no shot).
bool WorldScreen_Attack(int button, bool down);
// ev_hldm.cpp, before a bullet decal: true when the point lies on a bound screen's {screen face of entity entindex
// (the face is invisible and the panel is drawn there: no decal).
bool WorldScreen_IsScreenFace(int entindex, const float *point);

//-----------------------------------------------------------------------------
// State shared with the VGUI2 panels (worldscreen.cpp, SBO world)
//-----------------------------------------------------------------------------
namespace worldscreen
{
//! worldscreen_debug value (0 off, 1 state changes + GL checks, 2 every render / draw).
int DebugLevel();
//! worldscreen_rate: texture updates per second (default 10).
float UpdateRate();
//! The slot's screen was seen by a 3D view (and is worth repainting) since the last call; clears it. Called by its
//! host once per VGUI2 paint, i.e. once per frame.
bool ConsumeSeen(int slot);
//! One of this frame's WORLDSCREEN_MAX_REPAINTS_PER_FRAME repaints for a due slot; false when they are used up or
//! kept for screens that waited longer (no content yet first, then the oldest lastRender: the host's clock, -1
//! never). A refused slot is served in a later frame by that order, so none starves.
bool ClaimRepaint(int slot, bool hasContent, double lastRender);
//! A button's command (checked, then sent to the server as vgui_screen_command).
void SendCommand(int slot, const char *command);
//! printf to the console (gEngfuncs.Con_Printf).
void Printf(const char *fmt, ...);
//! A note, not an error: printed with worldscreen_debug 1 or more, else only with developer 1 or 2 (Con_DPrintf).
void DebugPrintf(const char *fmt, ...);

// The world screen editor (vgui2/sbo_worldscreen_editor.cpp): which screen to preview a layout on, and the preview.
//! The nearest active panel screen whose front side the ray (unit dir) hits, at any distance; -1 if none.
int EditorSlotAt(const float *eye, const float *dir);
//! The screen in the middle of the view (EditorSlotAt along the view), -1 if none.
int EditorSlotInView();
//! The screen under a point of the game's screen (pixels), -1 if none.
int EditorSlotAtScreenPoint(int x, int y);
//! The slot's layout name ("" for a free slot or a movie screen) and entity index (0 for a free slot).
const char *EditorSlotPanel(int slot);
int EditorSlotEntity(int slot);
//! The world under a point of the game's screen (pixels): the point hit and the surface's normal (unit, toward the
//! eye). False when nothing is hit within 8192 units.
bool EditorTraceWall(int x, int y, float *point, float *normal);
//! A vgui_screen's origin (lower-left corner, 1 unit in front of the surface) and angles for a screen of width x
//! height units centered on point, flat on the surface with that normal (on a floor or ceiling: turned with the view)
void EditorWallPlace(const float *point, const float *normal, float width, float height, float *origin, float *angles);
//! The editor's own screen, only in this game (no entity, nothing sent to the server): placed (or moved) like a
//! vgui_screen at origin (lower-left corner) / angles / width / height, showing layoutText; its slot, or -1 when every
//! slot is taken. Removed by EditorRemoveLocal and with every map change (WorldScreen_Reset).
int EditorPlaceLocal(const float *origin, const float *angles, float width, float height, const char *panelName,
    const char *layoutText);
void EditorRemoveLocal();
//! Its slot, -1 if none
int EditorLocalSlot();
//! Shows the slot's screen built from layoutText (a layout's whole text) instead of its file; nullptr or "": from
//! its file again. False when the slot shows no panel or the text has no layout.
bool EditorPreview(int slot, const char *layoutText);
} // namespace worldscreen

//-----------------------------------------------------------------------------
// VGUI2 side (vgui2/world_screen_panel.cpp, SDK world)
//-----------------------------------------------------------------------------
//! The cursor panel. False without VGUI2.
bool VGui2_WorldScreen_Init();
//! Builds slot's panel from ui/scripts/screens/<panelName>.res (replacing an older one); pixel size from the
//! layout's "WorldScreen" section. False: no VGUI2 or no layout file (the screen is not shown).
bool VGui2_WorldScreen_Create(int slot, const char *panelName, int &pixelWide, int &pixelTall);
//! The same from a layout's text instead of its file (the world screen editor's preview); nullptr or "": from the
//! file. False: no VGUI2, or no layout in the text / no file.
bool VGui2_WorldScreen_CreateFromText(int slot, const char *panelName, const char *layoutText, int &pixelWide,
    int &pixelTall);
//! Removes slot's panel.
void VGui2_WorldScreen_Remove(int slot);
//! Number of renders of slot's panel so far (status).
int VGui2_WorldScreen_RenderCount(int slot);
//! Aim update: onScreen + panel pixel, or onScreen false (looked away / screen gone: no button armed).
void VGui2_WorldScreen_SetCursor(int slot, bool onScreen, int x, int y);
//! Click press or release; button 0 = left, 1 = right. A button is clicked by a left press and release over it.
void VGui2_WorldScreen_MouseButton(int slot, int button, bool down);
//! A control change from the server (WSCREEN_CONTROL_*).
void VGui2_WorldScreen_Control(int slot, int op, const char *control, const char *value);
//! The cursor at the center of the view.
void VGui2_WorldScreen_ShowCursor(bool show);

#endif // SBO_WORLDSCREEN_H
