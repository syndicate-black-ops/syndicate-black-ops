//=========================================================
// SBO 2.0 player head scale, client side (head_scale.cpp, docs/MODEL_SCALE.md "Head scale"): the "HeadScale"
// message; StudioDrawPlayer scales the head bones (game_shared/head_scale_shared.h).
//=========================================================

#if !defined( HEAD_SCALE_CLIENT_H )
#define HEAD_SCALE_CLIENT_H

void HeadScale_Init( void );		// hooks the message
void HeadScale_Reset( void );		// VidInit: normal heads on a new map or server (the server sends the others)
float HeadScale_Get( int iPlayer );	// player entity index 1 .. 32: his head scale (1: normal)

#endif // HEAD_SCALE_CLIENT_H
