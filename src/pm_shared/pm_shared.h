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
// pm_shared.h
//
#if !defined( PM_SHAREDH )
#define PM_SHAREDH
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void PM_Init( struct playermove_s *ppmove );
void PM_Move ( struct playermove_s *ppmove, int server );
char PM_FindTextureType( char *name );
// halflife-updated: player hull sizes, shared by GetHullBounds (server) and HUD_GetHullBounds (client)
int PM_GetHullBounds( int hullnumber, float *mins, float *maxs );
// SBO 2.0: sv_spectatormaxspeed from the movevars (0 before the first move)
float PM_GetSpectatorMaxSpeed( void );
// SBO 2.0 seamless trigger_teleport: tele = ( turn, yaw before the turn, 0 ) (pev->vuser1 / clientdata vuser1);
// true if a move with this yaw was made before the client got the turn
int PM_TeleportStaleCmd( const float *tele, float yaw );

#ifdef __cplusplus
}
#endif

// SBO 2.0 (Featureful player_capabilities, docs/FEATUREFUL_GUIDE_ENTITIES.md): the suppressed capabilities of a
// player (bits below). The movement code gets them from PM_GetSuppressedCapabilities: on the server the player's own
// (player.cpp), on the client the last "Capability" message (hud_msg.cpp), as in Featureful.
#ifdef __cplusplus
extern "C" int PM_GetSuppressedCapabilities( int playerIndex );
#else
int PM_GetSuppressedCapabilities( int playerIndex );
#endif
// SBO 2.0 GM mode: a roaming Game Master's speed limit (server: the GM plugin's syndicate_gmmode_spectator_speed,
// GM_ROAM_SPEED without the plugin, gmmode.cpp; client: the last GmStatus, sbo_gm_hud.cpp); 0: sv_spectatormaxspeed
#ifdef __cplusplus
extern "C" float PM_GmRoamSpeed( void );
#else
float PM_GmRoamSpeed( void );
#endif

#define PLAYER_SUPPRESS_ATTACK		( 1 << 0 )
#define PLAYER_SUPPRESS_JUMP		( 1 << 1 )
#define PLAYER_SUPPRESS_DUCK		( 1 << 2 )
#define PLAYER_SUPPRESS_USE			( 1 << 3 )
#define PLAYER_SUPPRESS_STEP_SOUND	( 1 << 4 )
#define PLAYER_SUPPRESS_MOVEMENT	( 1 << 5 )

// Spectator Movement modes (stored in pev->iuser1, so the physics code can get at them)
#define OBS_NONE				0
#define OBS_CHASE_LOCKED		1
#define OBS_CHASE_FREE			2
#define OBS_ROAMING				3		
#define OBS_IN_EYE				4
#define OBS_MAP_FREE			5
#define OBS_MAP_CHASE			6

#endif