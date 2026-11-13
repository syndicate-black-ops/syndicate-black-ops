//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#if !defined ( VIEWH )
#define VIEWH 
#pragma once

void V_StartPitchDrift( void );
void V_StopPitchDrift( void );

// SoHL 1.9 env_sky: the entities in the 3D sky's sky room, drawn in the sky pass only (view.cpp)
struct cl_entity_s;
void EnvSky_AddEntity( int type, struct cl_entity_s *ent );	// HUD_AddEntity
void EnvSky_CreateEntities( void );						// HUD_CreateEntities
bool EnvSky_IsSkyEntity( const struct cl_entity_s *ent );	// the studio model renderer
void EnvSky_ShowEntities( void );						// the end of a view (HUD_DrawTransparentTriangles)
void EnvSky_Reset( void );								// a new map (HUD_VidInit)

#endif // !VIEWH