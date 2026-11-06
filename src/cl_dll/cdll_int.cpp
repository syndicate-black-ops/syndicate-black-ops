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
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//

#include "hud.h"
#include "cl_util.h"
#include "netadr.h"
#include "discord/discord_rpc.h"

extern "C"
{
#include "pm_shared.h"
}

#include <string.h>
#include "viewport.h"
#include "vgui2/vgui2_api.h"

#include "triangleapi.h"
#include "particleman.h"
#include "IParticleMan_Active.h"
#include "environment.h"

#define DLLEXPORT __declspec( dllexport )


cl_enginefunc_t gEngfuncs;
CHud gHUD;
TeamFortressViewport *gViewPort = NULL;
IParticleMan *g_pParticleMan = NULL;	// cs16-client: particle manager, compiled in (particleman/)

void CL_LoadParticleMan( void );
void CL_UnloadParticleMan( void );

void InitInput (void);
void EV_HookEvents( void );
void IN_Commands( void );

/*
========================== 
    Initialize

Called when the DLL is first loaded.
==========================
*/
extern "C" 
{
int		DLLEXPORT Initialize( cl_enginefunc_t *pEnginefuncs, int iVersion );
int		DLLEXPORT HUD_VidInit( void );
void	DLLEXPORT HUD_Init( void );
int		DLLEXPORT HUD_Redraw( float flTime, int intermission );
int		DLLEXPORT HUD_UpdateClientData( client_data_t *cdata, float flTime );
void	DLLEXPORT HUD_Reset ( void );
void	DLLEXPORT HUD_PlayerMove( struct playermove_s *ppmove, int server );
void	DLLEXPORT HUD_PlayerMoveInit( struct playermove_s *ppmove );
char	DLLEXPORT HUD_PlayerMoveTexture( char *name );
int		DLLEXPORT HUD_ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size );
int		DLLEXPORT HUD_GetHullBounds( int hullnumber, float *mins, float *maxs );
void	DLLEXPORT HUD_Frame( double time );
void	DLLEXPORT HUD_VoiceStatus(int entindex, qboolean bTalking);
void	DLLEXPORT HUD_DirectorMessage( int iSize, void *pbuf );
}

/*
================================
HUD_GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int DLLEXPORT HUD_GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	// halflife-updated (halflife #1703): the old code assigned the local pointers instead of copying the vectors
	return PM_GetHullBounds( hullnumber, mins, maxs );
}

/*
================================
HUD_ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int	DLLEXPORT HUD_ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size )
{
	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

void DLLEXPORT HUD_PlayerMoveInit( struct playermove_s *ppmove )
{
	PM_Init( ppmove );
}

char DLLEXPORT HUD_PlayerMoveTexture( char *name )
{
	return PM_FindTextureType( name );
}

void DLLEXPORT HUD_PlayerMove( struct playermove_s *ppmove, int server )
{
#ifdef SBO_TEST_COMMANDS
	Materials_TestFrame();	// test_cl_materials: footsteps printed by pm_shared.c
#endif
	PM_Move( ppmove, server );
}

int DLLEXPORT Initialize( cl_enginefunc_t *pEnginefuncs, int iVersion )
{
	gEngfuncs = *pEnginefuncs;

	if (iVersion != CLDLL_INTERFACE_VERSION)
		return 0;

	memcpy(&gEngfuncs, pEnginefuncs, sizeof(cl_enginefunc_t));

	// keep the engine's full table (messagemode hooks need entries past SDK 2.3's struct)
	VGui2_SetEngineFuncs(pEnginefuncs);

	// SBO 2.0 updater (docs/UPDATER.md): an install that was interrupted, or that waits for files that were in use,
	// is finished (or undone) now, before anything of SBO's UI opens its files
	VGui2_Updater_Startup();

	EV_HookEvents();

	CL_LoadParticleMan();

	return 1;
}


/*
==========================
	HUD_VidInit

Called when the game initializes
and whenever the vid_mode is changed
so the HUD can reinitialize itself.
==========================
*/

// SBO 2.0: the map the client's fog, fog zones and weather belong to: set by InitHUD (hud_msg.cpp), which starts
// them over for the map the server is on, and by HUD_VidInit when it starts them over for a new map
char g_szMapStateLevel[64];

int DLLEXPORT HUD_VidInit( void )
{
	gHUD.VidInit();

	// SBO 2.0: a new map starts with no fog, no fog zones and no weather until the server's messages (InitHUD resets
	// them too). This does not rely on InitHUD alone: on the playtest of 2026-10-17 the previous map's fog and rain
	// stayed on a map without either. A video restart on the same map keeps them (the level is the same), and so does
	// a VidInit after the new map's InitHUD (the state is that map's already). env_sky: CHud::VidInit (SKY_OFF).
	const char *pszLevel = gEngfuncs.pfnGetLevelName();
	if ( !pszLevel )
		pszLevel = "";
	const bool bNewMap = strcmp( pszLevel, g_szMapStateLevel ) != 0;
	if ( bNewMap )
	{
		Fog_Reset();
		FogZone_Clear();
		strncpy( g_szMapStateLevel, pszLevel, sizeof( g_szMapStateLevel ) - 1 );
		g_szMapStateLevel[sizeof( g_szMapStateLevel ) - 1] = '\0';
	}

	// The client viewport (menu stack, key input, cursor state, player / team data; viewport.cpp):
	// created with the first VidInit, reset on every later one (new map, video restart). It was a VGUI1 panel
	// under the engine's VGUI1 root (VGui_Startup); SBO 2.0 has no VGUI1 (IClientVGUI::UseVGUI1 returns 0).
	if ( gViewPort )
		gViewPort->Initialize();
	else
		gViewPort = new TeamFortressViewport( 0, 0, ScreenWidth, ScreenHeight );

	// cs16-client: drop all particles (new map / video restart) and keep the current weather. SBO 2.0: only on the
	// same map; a new map has none until its ReceiveW (Initialize: the saved weather is dropped too)
	if ( g_pParticleMan )
	{
		g_pParticleMan->ResetParticles();
		if ( bNewMap )
		{
			g_Environment.Initialize();
		}
		else
		{
			g_Environment.Reset();
			g_Environment.RestoreWeather();
		}
	}

	return 1;
}

/*
==========================
	HUD_Init

Called whenever the client connects
to a server.  Reinitializes all 
the hud variables.
==========================
*/

void DLLEXPORT HUD_Init( void )
{
	InitInput();
	gHUD.Init();
	DiscordRpc_Init();
	VGui2_Updater_Init();
}


/*
==========================
	HUD_Redraw

called every screen frame to
redraw the HUD.
===========================
*/

int DLLEXPORT HUD_Redraw( float time, int intermission )
{
	// halflife-updated PR #245: with hud_scale the crosshair is drawn (scaled) here instead of by the engine
	CL_DrawCrosshair( intermission );

	gHUD.Redraw( time, intermission );

	return 1;
}


/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/

int DLLEXPORT HUD_UpdateClientData(client_data_t *pcldata, float flTime )
{
	IN_Commands();

	return gHUD.UpdateClientData(pcldata, flTime );
}

/*
==========================
	HUD_Reset

Called at start and end of demos to restore to "non"HUD state.
==========================
*/

void DLLEXPORT HUD_Reset( void )
{
	gHUD.VidInit();
}

/*
==========================
HUD_Frame

Called by engine every frame that client .dll is loaded
==========================
*/

void DLLEXPORT HUD_Frame( double time )
{
	// the viewport's per-frame work (was the paint of its VGUI1 panel): command menu after a team change, the
	// spectator panel's update
	if ( gViewPort )
		gViewPort->Frame();

	GetClientVoiceMgr()->Frame(time);
	DiscordRpc_Frame();

	VGui2_Frame();
	VGui2_Updater_Frame();

	// SBO 2.0: SBO ships no config.cfg (unpacking a build never overwrites a player's settings) but default.cfg. On the
	// first frame (the engine has run its startup config by then: syndicate/config.cfg, or Half-Life's from valve/ when
	// the mod has none), a game folder without a config.cfg of its own gets SBO's binds once, over the player's
	// Half-Life settings (and SBO's name and running speed, docs/DEFAULT_BINDS.md). Nothing is printed. The engine
	// writes syndicate/config.cfg on quit, so the next start has one.
	static bool s_bDefaultConfigChecked = false;
	if ( !s_bDefaultConfigChecked )
	{
		s_bDefaultConfigChecked = true;
		if ( !VGui2_GameDirHasFile( "config.cfg" ) )
		{
			gEngfuncs.pfnClientCmd( "exec default.cfg\n" );
		}
	}
}


/*
==========================
HUD_VoiceStatus

Called when a player starts or stops talking.
==========================
*/

void DLLEXPORT HUD_VoiceStatus(int entindex, qboolean bTalking)
{
	GetClientVoiceMgr()->UpdateSpeakerStatus(entindex, bTalking);
}

/*
==========================
HUD_DirectorEvent

Called when a director event message was received
==========================
*/

void DLLEXPORT HUD_DirectorMessage( int iSize, void *pbuf )
{
	 gHUD.m_Spectator.DirectorMessage( iSize, pbuf );
}


/*
==========================
CL_LoadParticleMan / CL_UnloadParticleMan

cs16-client: the particle manager (weather) is compiled into the client instead of Valve's cl_dlls/particleman.dll.
SBO: needs the Steam engine's triangle API additions (BoxInPVS, LightAtPoint, GetMatrix); without them no particles.
==========================
*/
void CL_LoadParticleMan( void )
{
	triangleapi_t *pTri = gEngfuncs.pTriAPI;
	if ( !pTri || !pTri->BoxInPVS || !pTri->LightAtPoint || !pTri->GetMatrix )
	{
		gEngfuncs.Con_DPrintf( "Particle manager: engine triangle API too old, weather disabled\n" );
		return;
	}

	g_pParticleMan = new IParticleMan_Active();
	if ( g_pParticleMan )
	{
		g_pParticleMan->SetUp( &gEngfuncs );
	}
}

void CL_UnloadParticleMan( void )
{
	g_Environment.Clear();

	if ( g_pParticleMan )
	{
		g_pParticleMan->ResetParticles();
		delete g_pParticleMan;
		g_pParticleMan = NULL;
	}
}
