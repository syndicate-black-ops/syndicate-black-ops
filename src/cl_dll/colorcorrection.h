//=========================================================
// SBO 2.0 color_correction / color_correction_volume, client side (colorcorrection.cpp): the "ColorCorr" message, the
// weights from the view, the grade of each real 3D view. docs/COLOR_CORRECTION.md.
//=========================================================

#if !defined( COLORCORRECTION_H )
#define COLORCORRECTION_H

void ColorCorr_Init( void );		// HUD_Init: hooks the message, the cvar mat_colorcorrection
void ColorCorr_Reset( void );		// InitHUD (new server or map): none until the server sends them
void ColorCorr_VidInit( void );		// a new map: also frees the tables' textures

// The frame's views (view.cpp, tri.cpp, GameStudioModelRenderer.cpp, hud_redraw.cpp, vgui2/client_viewport.cpp):
// V_CalcRefdef, first thing: a view drawn before this call and not graded yet (it had no view model) is graded now
void ColorCorr_Flush( void );
// V_CalcRefdef, once the view's origin is known (before its env_sky sky room): the view the next draws belong to.
// overview: the spectator's map (never graded); inset: the picture-in-picture view (its own fades for volumes)
void ColorCorr_SetView( const float *origin, bool overview, bool inset );
// HUD_DrawTransparentTriangles of a view (not the sky room): its 3D world is drawn
void ColorCorr_ViewDrawn( void );
// R_StudioDrawModel, after the view model: the view is graded here (HUD, crosshair and screen fades are drawn later)
void ColorCorr_AfterViewModel( void );
// CHud::Redraw, first thing, and CColorCorrMenuHook::Paint (the engine skips HUD_Redraw while the game menu
// is open): the frame's last view, if it was not graded yet (no view model). The second call of a frame does nothing.
void ColorCorr_FrameEnd( void );

// The in-game color correction editor (vgui2/sbo_cc_editor.cpp; docs/COLOR_CORRECTION.md "Editor"):
// its table (size^3 colors, red fastest, as colorcorr::Lut) mixed into the view at weight 0 .. 1 with the map's
// entities, as one more of them (Source mixes its tool's lookup the same way); entities false leaves the map's
// entities out. rgb null: no editor look. A new map (ColorCorr_VidInit) forgets it.
void ColorCorr_SetEditorLook( const unsigned char *rgb, int size, float weight, bool entities );
bool ColorCorr_HasEditorLook( void );
// A copy of the next 3D view as it was drawn, before any grade (the editor's color pick and preview picture)
void ColorCorr_RequestSnapshot( void );
// The latest copy: RGB, top row first; serial counts the copies made (0: none yet). False without one.
bool ColorCorr_GetSnapshot( const unsigned char **rgb, int *wide, int *tall, int *serial );

#endif // COLORCORRECTION_H
