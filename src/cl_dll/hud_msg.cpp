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
//  hud_msg.cpp
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "r_efx.h"
#include "environment.h"
#include "aurora/particlemgr.h"
#include "worldtext.h"
#include "mirror.h"
#include "overlay.h"
#include "inventory.h"
#include "colorcorrection.h"
#include "map_music.h"
#include "worldscreen/worldscreen.h"

#define MAX_CLIENTS 32

extern BEAM *pBeam;
extern BEAM *pBeam2;

/// USER-DEFINED SERVER MESSAGE HANDLERS

int CHud :: MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf )
{
	ASSERT( iSize == 0 );

	// clear all hud data
	HUDLIST *pList = m_pHudList;

	while ( pList )
	{
		if ( pList->p )
			pList->p->Reset();
		pList = pList->pNext;
	}

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;

	// SBO 2.0: the server sends the fog with every ResetHUD (connect, respawn, start of a demo): the view is
	// somewhere else, no fade
	Fog_Snap();

	return 1;
}

void CAM_ToFirstPerson(void);

void CHud :: MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf )
{
	CAM_ToFirstPerson();
}

// SBO 2.0 (Featureful hud_msg.cpp): the player's suppressed capabilities (player_capabilities, trigger_camera)
int CHud :: MsgFunc_Capability( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iSuppressedCaps = READ_LONG();
	return 1;
}

// SBO 2.0 (Featureful): the movement code's capabilities on the client: the last Capability message
extern "C" int PM_GetSuppressedCapabilities( int playerIndex )
{
	return gHUD.m_iSuppressedCaps;
}

void CHud :: MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf )
{
	// prepare all hud data
	HUDLIST *pList = m_pHudList;

	m_iSuppressedCaps = 0;	// SBO 2.0: the server sends them again after InitHUD

	while (pList)
	{
		if ( pList->p )
			pList->p->InitHUDData();
		pList = pList->pNext;
	}

	//Probably not a good place to put this.
	pBeam = pBeam2 = NULL;

	// cs16-client: new server/map: no fog until the server's Fog, no weather until its ReceiveW
	Fog_Reset();
	FogZone_Clear();	// PROTOTYPE: the env_fogzone list comes with the HUD initialisation

	// SoHL (LRC): no 3D sky. SBO 2.0: the server sends the map's env_sky right after this (CBasePlayer::
	// UpdateClientData), with every HUD initialisation, so a player who joins later has it too.
	m_iSkyMode = SKY_OFF;

	g_Environment.Initialize();

	// SBO 2.0: this state is now the current map's (HUD_VidInit, cdll_int.cpp, starts it over on a new map)
	{
		extern char g_szMapStateLevel[64];
		const char *pszLevel = gEngfuncs.pfnGetLevelName();
		strncpy( g_szMapStateLevel, pszLevel ? pszLevel : "", sizeof( g_szMapStateLevel ) - 1 );
		g_szMapStateLevel[sizeof( g_szMapStateLevel ) - 1] = '\0';
	}

	// SBO 2.0: new server/map: the spectator timer shows the map's running time until the server's RoundTime
	ResetRoundTime();

	// SBO 2.0: new server/map: no AFK signs until the server's PlayerAfk
	PlayerAfk_Reset();

	// SBO 2.0: new server/map: the view model's own render properties until the server's ViewModel
	V_ResetViewModelRender();

	// SBO 2.0: new server/map: no env_particle systems until the server's Aurora messages
	Aurora_Reset();
	// SBO 2.0: new server/map: no env_mirror until the server's Mirror
	Mirror_Reset();
	// SBO 2.0: new server/map: no env_overlay until the server's Overlay
	Overlay_Reset();
	// SBO 2.0: new server/map, or fullupdate: no inventory items until the server's Inventory messages
	Inventory_Reset();

	// SBO 2.0: new server/map: no color_correction until the server's ColorCorr
	ColorCorr_Reset();
	// SBO 2.0: new server/map: no point_worldtext until the server's WorldText
	WorldText_Reset();
	// SBO 2.0: new server/map: the server's next MapMusic is run whatever it is
	MapMusic_Reset();
	// SBO 2.0: new server/map: no world screens until the server's WScreen (sent right after InitHUD)
	WorldScreen_Reset();
}

// SBO 2.0: spectator timer. Counter-Strike's RoundTime (cs16-client hud/timer.cpp: seconds left, counted down from
// the time the message arrived), with a mode: the time left, the time elapsed, or stopped.
void CHud :: ResetRoundTime( void )
{
	m_iRoundTimeMode = ROUNDTIME_ELAPSED;
	m_flRoundTimeBase = 0;	// the client's clock is the map's
}

int CHud :: MsgFunc_RoundTime( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iSeconds = READ_LONG();
	int iMode = READ_BYTE();

	if ( iSeconds < 0 )
		iSeconds = 0;

	// not m_flTime: that is the last frame drawn, on a new map still the previous map's
	float flNow = gEngfuncs.GetClientTime();

	switch ( iMode )
	{
	case ROUNDTIME_COUNTDOWN:
		m_flRoundTimeBase = flNow + iSeconds;
		break;

	case ROUNDTIME_ELAPSED:
		m_flRoundTimeBase = flNow - iSeconds;
		break;

	default:
		iMode = ROUNDTIME_STOPPED;
		m_flRoundTimeBase = iSeconds;
		break;
	}

	m_iRoundTimeMode = iMode;

	return 1;
}

int CHud :: GetRoundTime( float *pflNextChange )
{
	float flNow = gEngfuncs.GetClientTime();
	float flNextChange = 0;
	int iSeconds;

	if ( m_iRoundTimeMode == ROUNDTIME_COUNTDOWN )
	{
		// rounded up: 0:00 when the time is up, not a second before
		float flLeft = m_flRoundTimeBase - flNow;

		if ( flLeft > 0 )
		{
			iSeconds = (int)ceil( flLeft );
			flNextChange = flLeft - ( iSeconds - 1 );
		}
		else
			iSeconds = 0;
	}
	else if ( m_iRoundTimeMode == ROUNDTIME_ELAPSED )
	{
		float flElapsed = flNow - m_flRoundTimeBase;

		if ( flElapsed < 0 )
			flElapsed = 0;

		iSeconds = (int)flElapsed;
		flNextChange = ( iSeconds + 1 ) - flElapsed;
	}
	else
		iSeconds = (int)m_flRoundTimeBase;

	if ( pflNextChange )
		*pflNextChange = flNextChange;

	return iSeconds;
}


// cs16-client (hud_msg.cpp): env_fog colour and density (ReGameDLL_CS CBasePlayer::UpdateClientData)
int CHud :: MsgFunc_Fog( const char *pszName, int iSize, void *pbuf )
{
	// SBO 2.0: read into a local, Fog_Set (tri.cpp) fades to it (cs16-client: straight into g_FogParameters)
	FogParameters fog;
	memset( &fog, 0, sizeof(FogParameters));

	BEGIN_READ( pbuf, iSize );

	fog.affectsSkyBox = false;
	fog.color[0] = READ_BYTE();
	fog.color[1] = READ_BYTE();
	fog.color[2] = READ_BYTE();

	union
	{
		float f;
		char b[4];
	} density;

	density.b[0] = READ_BYTE();
	density.b[1] = READ_BYTE();
	density.b[2] = READ_BYTE();
	density.b[3] = READ_BYTE();

	fog.density = density.f;

	Fog_Set( fog );

	return 1;
}

// SoHL (LRC; AJH in 1.8: the scale): env_sky, the 3D sky (dlls/effects.cpp CEnvSky::SendState)
int CHud :: MsgFunc_SetSky( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iSkyMode = READ_BYTE();
	m_vecSkyPos.x = READ_COORD();
	m_vecSkyPos.y = READ_COORD();
	m_vecSkyPos.z = READ_COORD();
	m_iSkyScale = READ_BYTE();

	return 1;
}

// PROTOTYPE (docs/WEATHER.md "env_fogzone prototype"): one env_fogzone (dlls/triggers.cpp FogZone_Send): byte index,
// byte flags (1 active, 2 first of a full list), 6 coords (box), 3 bytes color, the density float as 4 bytes
int CHud :: MsgFunc_FogZone( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	const int iIndex = READ_BYTE();
	const int iFlags = READ_BYTE();

	float mins[3], maxs[3], color[3];
	int i;
	for ( i = 0; i < 3; i++ )
		mins[i] = READ_COORD();
	for ( i = 0; i < 3; i++ )
		maxs[i] = READ_COORD();
	for ( i = 0; i < 3; i++ )
		color[i] = READ_BYTE();

	union
	{
		float f;
		char b[4];
	} density;

	density.b[0] = READ_BYTE();
	density.b[1] = READ_BYTE();
	density.b[2] = READ_BYTE();
	density.b[3] = READ_BYTE();

	if ( iFlags & 2 )
		FogZone_Clear();

	FogZone_Set( iIndex, ( iFlags & 1 ) != 0, mins, maxs, color, density.f );

	return 1;
}

int CHud :: MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_Teamplay = READ_BYTE();

	return 1;
}


int CHud :: MsgFunc_Damage(const char *pszName, int iSize, void *pbuf )
{
	int		armor, blood;
	Vector	from;
	int		i;
	float	count;
	
	BEGIN_READ( pbuf, iSize );
	armor = READ_BYTE();
	blood = READ_BYTE();

	for (i=0 ; i<3 ; i++)
		from[i] = READ_COORD();

	count = (blood * 0.5) + (armor * 0.5);

	if (count < 10)
		count = 10;

	// TODO: kick viewangles,  show damage visually

	return 1;
}

int CHud :: MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iConcussionEffect = READ_BYTE();
	if (m_iConcussionEffect)
		this->m_StatusIcons.EnableIcon("dmg_concuss",255,160,0);
	else
		this->m_StatusIcons.DisableIcon("dmg_concuss");
	return 1;
}

// SBO: glow shell on the view model, colored by team (never hooked)
int CHud :: MsgFunc_GlowShell( const char *pszName, int iSize, void *pbuf )
{
	int iOn;
	int iTeam;
	float color[3];	// SBO bug: color[2] overlaps iTeam
	cl_entity_t *pViewModel;

	BEGIN_READ( pbuf, iSize );
	iOn = READ_BYTE();
	iTeam = READ_BYTE();

	switch ( iTeam )
	{
	default:
		color[0] = 255;
		color[1] = 0;
		color[2] = 0;
		break;
	case 2:
		color[0] = 0;
		color[1] = 255;
		color[2] = 0;
		break;
	case 3:
		color[0] = 0;
		color[1] = 0;
		color[2] = 255;
		break;
	}

	pViewModel = gEngfuncs.GetViewModel();

	if ( iOn )
	{
		pViewModel->curstate.renderfx = kRenderFxGlowShell;
		pViewModel->curstate.rendercolor.r = color[0];
		pViewModel->curstate.rendercolor.g = color[1];
		pViewModel->curstate.rendercolor.b = color[2];
		pViewModel->curstate.renderamt = 0;
	}
	else
	{
		pViewModel->curstate.renderfx = 0;
	}

	return 1;
}
