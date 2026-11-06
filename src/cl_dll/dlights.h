//=========================================================
// SBO 2.0 env_dlight, client side (dlights.cpp, docs/ENV_DLIGHT.md; Featureful's KeyedDLightManager): the
// "KeyedDLight" message and the keyed dynamic lights, kept on until the server switches them off, moved with the
// entity they follow.
//=========================================================

#if !defined( DLIGHTS_CLIENT_H )
#define DLIGHTS_CLIENT_H

void DLights_Init( void );				// hooks the message
void DLights_Reset( void );				// VidInit: a new map has none until the server sends them
void DLights_CreateEntities( void );	// HUD_CreateEntities: the lights follow their entities

#endif // DLIGHTS_CLIENT_H
