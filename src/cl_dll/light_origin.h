//=========================================================
// SBO 2.0 info_light_origin, client side (light_origin.cpp, docs/ENV_DLIGHT.md#info_light_origin): the
// "LightOrigin" message, and the point a studio model is lit from when its map entity names a marker.
//=========================================================

#if !defined( LIGHT_ORIGIN_CLIENT_H )
#define LIGHT_ORIGIN_CLIENT_H

void LightOrigin_Init( void );		// hooks the message
void LightOrigin_Reset( void );		// VidInit: a new map has none until the server sends them

// the point the model is lit from instead of its origin (FALSE: its own origin); the renderer, per model drawn
bool LightOrigin_Get( const struct cl_entity_s *ent, float *out );

#endif // LIGHT_ORIGIN_CLIENT_H
