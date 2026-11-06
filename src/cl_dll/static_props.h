//=========================================================
// SBO 2.0 env_static, client side (static_props.cpp, docs/ENV_STATIC.md): the props the server sends in the
// "StaticProp" message, drawn as client-only entities every frame (HUD_CreateEntities).
//=========================================================

#if !defined( STATIC_PROPS_CLIENT_H )
#define STATIC_PROPS_CLIENT_H

void StaticProps_Init( void );			// hooks the message
void StaticProps_Reset( void );			// VidInit: a new map or server has no props until the server sends them
void StaticProps_CreateEntities( void );	// HUD_CreateEntities: the visible props in the view's PVS

#endif // STATIC_PROPS_CLIENT_H
