//=========================================================
// SBO 2.0 env_overlay, client side (overlay.cpp): the "Overlay" message, drawn under the HUD. docs/ENV_OVERLAY.md.
//=========================================================

#if !defined( OVERLAY_H )
#define OVERLAY_H

void Overlay_Init( void );				// HUD_Init: hooks the message
void Overlay_Reset( void );				// InitHUD (new server or map): nothing shown until the server sends it
void Overlay_VidInit( void );			// a new map: also frees the .tga pictures of the last one
void Overlay_Redraw( float flTime );	// HUD_Redraw, before the HUD

#endif // OVERLAY_H
