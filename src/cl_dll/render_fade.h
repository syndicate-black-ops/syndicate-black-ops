//=========================================================
// SBO 2.0 env_render fades, client side (render_fade.cpp, docs/ENV_RENDER.md): the "RenderFade" message, and the
// blended render values of a fading entity, set every frame in HUD_AddEntity.
//=========================================================

#if !defined( RENDER_FADE_CLIENT_H )
#define RENDER_FADE_CLIENT_H

void RenderFade_Init( void );		// hooks the message
void RenderFade_Reset( void );		// VidInit: no fades on a new map or server

// HUD_AddEntity: while the entity fades, its renderamt / rendercolor / scale of this frame
void RenderFade_AddEntity( struct cl_entity_s *ent );

#endif // RENDER_FADE_CLIENT_H
