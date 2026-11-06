//=========================================================
// SBO 2.0: point_worldtext, client side: the texts the server sends ("WorldText", dlls/point_worldtext.cpp), drawn
// in the world with the triangle API from the font atlas sprites (sprites/worldtext/<font>.spr). docs/POINT_WORLDTEXT.md.
//=========================================================

#if !defined( WORLDTEXT_H )
#define WORLDTEXT_H

void WorldText_Init( void );		// hooks the messages
void WorldText_VidInit( void );		// new map or video restart: no texts, fonts loaded again
void WorldText_Reset( void );		// InitHUD: no texts until the server sends them again
void WorldText_DrawFrame( void );	// HUD_DrawNormalTriangles (not in the sky or overview pass)

#endif // WORLDTEXT_H
