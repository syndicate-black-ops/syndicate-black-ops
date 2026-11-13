/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// flashlight.cpp
//
// implementation of CHudFlashlight class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "pm_defs.h"
#include "event_api.h"
#include "r_efx.h"
#include "movewith_client.h"
#include "flashlight_light.h"

#include <string.h>
#include <stdio.h>



#ifdef SBO_TEST_COMMANDS
static void Flashlight_TraceCommand( void );
#endif

DECLARE_MESSAGE(m_Flash, FlashBat)
DECLARE_MESSAGE(m_Flash, Flashlight)

#define BAT_NAME "sprites/%d_flashlight.spr"

int CHudFlashlight::Init(void)
{
	m_fFade = 0;
	m_fOn = 0;

	HOOK_MESSAGE(Flashlight);
	HOOK_MESSAGE(FlashBat);
#ifdef SBO_TEST_COMMANDS
	gEngfuncs.pfnAddCommand( "test_cl_flashlight", Flashlight_TraceCommand );
#endif

	m_iFlags |= HUD_ACTIVE;

	gHUD.AddHudElem(this);

	return 1;
};

void CHudFlashlight::Reset(void)
{
	m_fFade = 0;
	m_fOn = 0;
}

int CHudFlashlight::VidInit(void)
{
	int HUD_flash_empty = gHUD.GetSpriteIndex( "flash_empty" );
	int HUD_flash_full = gHUD.GetSpriteIndex( "flash_full" );
	int HUD_flash_beam = gHUD.GetSpriteIndex( "flash_beam" );

	m_hSprite1 = gHUD.GetSprite(HUD_flash_empty);
	m_hSprite2 = gHUD.GetSprite(HUD_flash_full);
	m_hBeam = gHUD.GetSprite(HUD_flash_beam);
	m_prc1 = &gHUD.GetSpriteRect(HUD_flash_empty);
	m_prc2 = &gHUD.GetSpriteRect(HUD_flash_full);
	m_prcBeam = &gHUD.GetSpriteRect(HUD_flash_beam);
	m_iWidth = m_prc2->right - m_prc2->left;

	return 1;
};

int CHudFlashlight:: MsgFunc_FlashBat(const char *pszName,  int iSize, void *pbuf )
{

	
	BEGIN_READ( pbuf, iSize );
	int x = READ_BYTE();
	m_iBat = x;
	m_flBat = ((float)x)/100.0;

	return 1;
}

int CHudFlashlight:: MsgFunc_Flashlight(const char *pszName,  int iSize, void *pbuf )
{

	BEGIN_READ( pbuf, iSize );
	m_fOn = READ_BYTE();
	int x = READ_BYTE();
	m_iBat = x;
	m_flBat = ((float)x)/100.0;

	return 1;
}

int CHudFlashlight::Draw(float flTime)
{
	if ( gHUD.m_iHideHUDDisplay & ( HIDEHUD_FLASHLIGHT | HIDEHUD_ALL ) )
		return 1;

	// SBO 2.0: spectators have no flashlight, and the watched player's isn't shown either (in first person the
	// weapon bits, with the suit, are the target's)
	if ( g_iUser1 || gEngfuncs.IsSpectateOnly() )
		return 1;

	int r, g, b, x, y, a;
	wrect_t rc;

	if (!(gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT)) ))
		return 1;

	if (m_fOn)
		a = 225;
	else
		a = MIN_ALPHA;

	if (m_flBat < 0.20)
		UnpackRGB(r,g,b, RGB_REDISH);
	else
		UnpackRGB(r,g,b, RGB_BLUEISH);

	ScaleColors(r, g, b, a);

	y = (m_prc1->bottom - m_prc2->top)/2;
	x = gHUD.GetWidth() - m_iWidth - m_iWidth/2 ;	// HUD scaling: HUD units

	// Draw the flashlight casing
	SPR_Set(m_hSprite1, r, g, b );
	SPR_DrawAdditive( 0,  x, y, m_prc1);

	if ( m_fOn )
	{  // draw the flashlight beam
		x = gHUD.GetWidth() - m_iWidth/2;

		SPR_Set( m_hBeam, r, g, b );
		SPR_DrawAdditive( 0, x, y, m_prcBeam );
	}

	// draw the flashlight energy level
	x = gHUD.GetWidth() - m_iWidth - m_iWidth/2 ;
	int iOffset = m_iWidth * (1.0 - m_flBat);
	if (iOffset < m_iWidth)
	{
		rc = *m_prc2;
		rc.left += iOffset;

		SPR_Set(m_hSprite2, r, g, b );
		SPR_DrawAdditive( 0, x + iOffset, y, &rc);
	}


	return 1;
}

//=========================================================
// SBO 2.0: the local player's flashlight light. The engine places it in CL_LinkPlayers (hw.so CL_PlayerFlashlight)
// with a trace against the prediction's physents as they are at that moment: after a frame's packets were read they
// hold every entity's received state, and a moving object's part that client.dll draws on its parent (movewith) is
// there at its local pose, far from where it is drawn, so on frames without a predicted command the light missed it
// and on the others hit it (it flickered on the Move With lift). Here the same light from the same trace, against
// the parts where they are drawn (MoveWith_EventPhysents), as the egon beam and bullet events trace.
//=========================================================

static int s_iFlashlightOn;	// the local player's EF_DIMLIGHT, as last received

#ifdef SBO_TEST_COMMANDS
// Test builds only: "test_cl_flashlight 1 | 0" logs, each frame the light is on, where it goes and where the same
// trace against the received states (no movewith placement, as the engine traces) ends ("MWF <time> <x y z> <ent>
// received <x y z> <ent>")
static int s_iFlashlightTrace;
static void Flashlight_TraceCommand( void )
{
	s_iFlashlightTrace = gEngfuncs.Cmd_Argc() > 1 ? atoi( gEngfuncs.Cmd_Argv( 1 ) ) : 0;
}
#endif

void Flashlight_ProcessPlayerState( struct entity_state_s *dst )
{
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	if ( !local || dst->number != local->index )
		return;
	s_iFlashlightOn = ( dst->effects & EF_DIMLIGHT ) != 0;
	dst->effects &= ~EF_DIMLIGHT;
}

void Flashlight_CreateEntities( void )
{
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	if ( !s_iFlashlightOn || !local || !local->model )
		return;

	// as the engine: from the eye, 2000 units along the view, a point trace that hits studio models by their boxes
	vec3_t angles, forward, right, up, viewheight, start, end;
	gEngfuncs.GetViewAngles( (float *)angles );
	AngleVectors( angles, forward, right, up );
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( viewheight );
	VectorAdd( local->origin, viewheight, start );
	VectorMA( start, 2000, forward, end );

	pmtrace_t tr;
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( local->index - 1 );
	MoveWith_EventPhysents();	// SBO 2.0 movewith: placed solid children where they are drawn
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( start, end, PM_STUDIO_BOX, -1, &tr );
	vec3_t org;
	VectorCopy( tr.endpos, org );
	physent_t *pe = tr.ent > 0 ? gEngfuncs.pEventAPI->EV_GetPhysent( tr.ent ) : NULL;
	if ( pe && pe->studiomodel )
		VectorCopy( pe->origin, org );	// a studio model: on its origin, as the engine
#ifdef SBO_TEST_COMMANDS
	int iHit = pe ? pe->info : 0;
#endif
#ifdef SBO_TEST_COMMANDS
	if ( s_iFlashlightTrace )
	{
		pmtrace_t tr0;
		gEngfuncs.pEventAPI->EV_PopPMStates();
		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetSolidPlayers( local->index - 1 );
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( start, end, PM_STUDIO_BOX, -1, &tr0 );
		gEngfuncs.Con_Printf( "MWF %.4f %.1f %.1f %.1f %d received %.1f %.1f %.1f %d\n", gEngfuncs.GetClientTime(), org[0], org[1],
			org[2], iHit, tr0.endpos[0], tr0.endpos[1], tr0.endpos[2],
			tr0.ent > 0 && gEngfuncs.pEventAPI->EV_GetPhysent( tr0.ent ) ? gEngfuncs.pEventAPI->EV_GetPhysent( tr0.ent )->info : 0 );
	}
#endif
	gEngfuncs.pEventAPI->EV_PopPMStates();

	dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( 1 );
	if ( !dl )
		return;
	float dist = tr.fraction * 2000.0f;
	int c = 255;
	if ( dist > 500.0f )
		c = (int)( ( 500.0f / dist ) * ( 500.0f / dist ) * 255.0f );
	VectorCopy( org, dl->origin );
	dl->radius = 80;
	dl->color.r = dl->color.g = dl->color.b = c;
	dl->die = gEngfuncs.GetClientTime() + 0.2f;
}
