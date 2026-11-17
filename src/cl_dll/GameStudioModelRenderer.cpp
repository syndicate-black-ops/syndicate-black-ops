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
#include "view.h"	// SoHL env_sky: EnvSky_IsSkyEntity
#include "static_props.h"	// SBO 2.1: env_static "In the 3D sky" (StaticProps_IsSkyProp)
#include "colorcorrection.h"	// SBO 2.0: color_correction
#include "pm_shared.h"	// SBO 2.1: OBS_IN_EYE (the cloaked view model)

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
CloakViewAmount

SBO 2.1: while the player whose eyes the view is (the local player, or the one a spectator watches in first person)
is cloaked (the server draws him kRenderTransAlpha, CBasePlayer::CloakUpdate), his view model (gun, hands and arms)
is drawn at his render amount, never below CLOAK_VIEW_MIN so the gun's outline stays readable. The amount comes with
the player's entity state; it rises at once and falls at most CLOAK_VIEW_FALL a second here, which smooths the steps
of the updates. -1: not cloaked (normal drawing).
====================
*/
#define CLOAK_VIEW_MIN		40
#define CLOAK_VIEW_FALL		200.0f

static int CloakViewAmount( void )
{
	static float s_flAmount = -1;
	static float s_flTime = 0;

	const float flTime = gEngfuncs.GetClientTime();
	float flDelta = flTime - s_flTime;
	if ( flDelta < 0 || flDelta > 1.0f )
		flDelta = 0;
	s_flTime = flTime;

	// the player whose weapon this view draws: a spectator's first-person target, in the main view or in the
	// picture-in-picture inset (its first-person mode draws the target's weapon even when the main view is not)
	cl_entity_t *pPlayer = gEngfuncs.GetLocalPlayer();
	const bool bInsetInEye = gHUD.m_Spectator.m_bInsetDrawPass && gHUD.m_Spectator.InsetMode() == INSET_IN_EYE;
	if ( ( g_iUser1 == OBS_IN_EYE || bInsetInEye ) && g_iUser2 > 0 )
		pPlayer = gEngfuncs.GetEntityByIndex( g_iUser2 );

	if ( !pPlayer || pPlayer->curstate.rendermode != kRenderTransAlpha || pPlayer->curstate.renderamt >= 255 )
	{
		s_flAmount = -1;
		return -1;
	}

	const float flTarget = (float)pPlayer->curstate.renderamt;
	if ( s_flAmount < 0 || flTarget >= s_flAmount )
		s_flAmount = flTarget;
	else
	{
		s_flAmount -= CLOAK_VIEW_FALL * flDelta;
		if ( s_flAmount < flTarget )
			s_flAmount = flTarget;
	}

	const int iAmount = (int)s_flAmount;
	return iAmount < CLOAK_VIEW_MIN ? CLOAK_VIEW_MIN : iAmount;
}

/*
====================
R_StudioDrawModel

====================
*/
int R_StudioDrawModel( int flags )
{
	cl_entity_t *pEnt = IEngineStudio.GetCurrentEntity();

	// SoHL 1.8 (StudioDrawModel: only kRenderFxEntInPVS models while the sky is drawn): in the env_sky sky room pass
	// only the models in the sky room (view.cpp EnvSky_AddEntity), none of those around the view (see
	// R_StudioDrawPlayer), nor the view model; and the view does not draw the sky room's (they are in the sky)
	// SBO 2.1: env_static props flagged "In the 3D sky" count as sky room models (static_props.cpp)
	if ( gHUD.m_bSkyDrawPass != ( EnvSky_IsSkyEntity( pEnt ) || StaticProps_IsSkyProp( pEnt ) ) )
		return 0;

	// SBO 2.0: server menus' model preview (server_menu_preview.h): its entity, and no view model in front of it
	if ( ServerMenuPreview_IsEntity( pEnt ) )
		return g_StudioRenderer.StudioDrawPreview( flags );
	if ( pEnt == gEngfuncs.GetViewModel() && ServerMenuPreview_HidesViewModel() )
		return 0;

	// SBO 2.0: color_correction grades the view right after its view model, the last of its 3D drawing (the HUD, the
	// crosshair and the screen fades come later and keep their colors)
	if ( pEnt == gEngfuncs.GetViewModel() && ( flags & STUDIO_RENDER ) )
	{
		// SBO 2.1: the cloak fades the view model with the player (CloakViewAmount); its own state is put back after
		const int iCloak = CloakViewAmount();
		const int iMode = pEnt->curstate.rendermode, iAmount = pEnt->curstate.renderamt;
		if ( iCloak >= 0 )
		{
			// the engine set the view model's blend from the player's own state before calling this (kRenderTransAlpha
			// draws it solid): set it again (StudioSetRenderamt, the view model glass call of the studio interface)
			pEnt->curstate.rendermode = kRenderTransTexture;
			pEnt->curstate.renderamt = iCloak;
			if ( IEngineStudio.StudioSetRenderamt )
				IEngineStudio.StudioSetRenderamt( iCloak );
		}
		const int iResult = g_StudioRenderer.StudioDrawModel( flags );
		pEnt->curstate.rendermode = iMode;
		pEnt->curstate.renderamt = iAmount;
		ColorCorr_AfterViewModel();
		return iResult;
	}

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
