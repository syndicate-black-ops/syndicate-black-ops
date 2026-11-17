//=========================================================
// SBO 2.1 info_waypoint, client side: the HUD markers the server sends in the "Waypoint" message
// (dlls/info_waypoint.cpp, game_shared/waypoint_shared.h). docs/INFO_WAYPOINT.md.
//=========================================================

#ifndef SBO_CL_DLL_WAYPOINT_H
#define SBO_CL_DLL_WAYPOINT_H

void Waypoint_Init( void );		// hooks the message
void Waypoint_VidInit( void );	// new map: no markers; the icon and arrow sprites loaded again
void Waypoint_Reset( void );	// new map or server (InitHUD): no markers until the server sends them

// tri.cpp HUD_DrawTransparentTriangles: a view was drawn, iView 0 the main view (not the sky room or an overview map),
// 1 a spectator's first-person inset: each marker's place in it, projected with this view
void Waypoint_ViewDrawn( int iView );

// HUD_Redraw, every frame: the markers placed by this frame's main view, drawn with the HUD elements (bShow false
// while the HUD is hidden: none)
void Waypoint_Redraw( float flTime, bool bShow );

#endif // SBO_CL_DLL_WAYPOINT_H
