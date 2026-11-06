//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <assert.h>
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include "studio_util.h"
#include "r_studioint.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
#define SBO_SERVER_MENU_PREVIEW_ENGINE
#include "server_menu_preview.h"	// SBO 2.0: server menus' model preview

extern engine_studio_api_t IEngineStudio;

//
// Override the StudioModelRender virtual member functions here to implement custom bone
// setup, blending, etc.
//

// Global engine <-> studio model rendering code interface
extern engine_studio_api_t IEngineStudio;

// The renderer object, created on the stack.
CGameStudioModelRenderer g_StudioRenderer;
/*
====================
CGameStudioModelRenderer

====================
*/
CGameStudioModelRenderer::CGameStudioModelRenderer( void )
{
}

////////////////////////////////////
// Hooks to class implementation
////////////////////////////////////

/*
====================
R_StudioDrawPlayer

====================
*/
int R_StudioDrawPlayer( int flags, entity_state_t *pplayer )
{
	// SoHL 1.8 (StudioModelRenderer.cpp StudioDrawModel): no models in the env_sky sky room pass (view.cpp
	// V_SkyViewStart). The models the server sends are around the view, and the sky room's camera would see the
	// ones in line with the sky room's own sky; the view's pass draws them.
	if ( gHUD.m_bSkyDrawPass )
		return 0;

	return g_StudioRenderer.StudioDrawPlayer( flags, pplayer );
}

/*
====================
R_StudioDrawModel

====================
*/
int R_StudioDrawModel( int flags )
{
	// SoHL 1.8: none in the env_sky sky room pass (see R_StudioDrawPlayer); this includes the view model
	if ( gHUD.m_bSkyDrawPass )
		return 0;

	// SBO 2.0: server menus' model preview (server_menu_preview.h): its entity, and no view model in front of it
	cl_entity_t *pEnt = IEngineStudio.GetCurrentEntity();
	if ( ServerMenuPreview_IsEntity( pEnt ) )
		return g_StudioRenderer.StudioDrawPreview( flags );
	if ( pEnt == gEngfuncs.GetViewModel() && ServerMenuPreview_HidesViewModel() )
		return 0;

	return g_StudioRenderer.StudioDrawModel( flags );
}

/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	g_StudioRenderer.Init();
}

// The simple drawing interface we'll pass back to the engine
r_studio_interface_t studio =
{
	STUDIO_INTERFACE_VERSION,
	R_StudioDrawModel,
	R_StudioDrawPlayer,
};

/*
====================
HUD_GetStudioModelInterface

Export this function for the engine to use the studio renderer class to render objects.
====================
*/
#define DLLEXPORT __declspec( dllexport )
extern "C" int DLLEXPORT HUD_GetStudioModelInterface( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio )
{
	if ( version != STUDIO_INTERFACE_VERSION )
		return 0;

	// Point the engine to our callbacks
	*ppinterface = &studio;

	// Copy in engine helper functions
	memcpy( &IEngineStudio, pstudio, sizeof( IEngineStudio ) );

	// Initialize local variables, etc.
	R_StudioInit();

	// Success
	return 1;
}
