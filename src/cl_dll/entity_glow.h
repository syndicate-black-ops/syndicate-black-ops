//=========================================================
// SBO 2.1 env_entity_glow, client side: the glows the server sends in the "EntGlow" message (dlls/env_entity_glow.cpp)
// and the pass that draws them after the view's world and models (tri.cpp HUD_DrawTransparentTriangles,
// entity_glow_gl.cpp). docs/ENV_ENTITY_GLOW.md.
//=========================================================

#ifndef SBO_CL_DLL_ENTITY_GLOW_H
#define SBO_CL_DLL_ENTITY_GLOW_H

struct cl_entity_s;

void EntityGlow_Init( void );	// hooks the message
void EntityGlow_Reset( void );	// new map or server (VidInit, InitHUD): no glow until the server sends them

// StudioModelRenderer.cpp StudioRenderFinal_Hardware: a studio model as the engine draws it in this view. TRUE when
// the entity is a glow target: its header (studiohdr_t), its bone transforms and the submodels its body value picks are
// kept for the glow pass.
int EntityGlow_Capture( struct cl_entity_s *pEntity, void *pHeader, const float (*pBones)[3][4] );

// tri.cpp HUD_DrawTransparentTriangles: the glows of the view just drawn (its world and models are in the depth
// buffer). Skip: a view with no glow (the sky room, the spectator's overview map); the models it drew are forgotten.
void EntityGlow_Draw( void );
void EntityGlow_SkipView( void );

#endif // SBO_CL_DLL_ENTITY_GLOW_H
