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

#ifndef GAME_H
#define GAME_H

extern void GameDLLInit( void );


extern cvar_t	displaysoundlist;

// multiplayer server rules
extern cvar_t	teamplay;
extern cvar_t	fraglimit;
extern cvar_t	timelimit;
extern cvar_t	friendlyfire;
extern cvar_t	falldamage;
extern cvar_t	weaponstay;
extern cvar_t	forcerespawn;
extern cvar_t	flashlight;
extern cvar_t	aimcrosshair;
extern cvar_t	decalfrequency;
extern cvar_t	teamlist;
extern cvar_t	teamoverride;
extern cvar_t	defaultteam;

// SBO (game.cpp)
extern cvar_t	easymode;
extern cvar_t	ban;
extern cvar_t	gmmode;
extern cvar_t	XP;
extern cvar_t	hardcore;
extern cvar_t	afk_time;	// SBO 2.0: mp_afk_time (player.cpp AfkThink)
extern cvar_t	afk_spectate_time;	// SBO 2.0: mp_afk_spectate_time
extern cvar_t	loadouttime;	// SBO 2.0: mp_loadouttime (coop.cpp LoadoutWaitBlocksRound)
extern cvar_t	missionendtime;	// SBO 2.0: mp_missionendtime (coop.cpp MissionEndTime)
extern cvar_t	semiclip;	// SBO 2.0: mp_semiclip (semiclip.cpp)
extern cvar_t	semiclip_time;	// SBO 2.0: mp_semiclip_time
extern cvar_t	semiclip_effects;	// SBO 2.0: mp_semiclip_effects
extern cvar_t	map_compat;	// SBO 2.0: mp_map_compat (map_compat.cpp)
extern cvar_t	juniorpowmonst;
extern cvar_t	junior;
extern cvar_t	junioracmar;
extern cvar_t	juniorpvmonst;
extern cvar_t	makermon;
extern cvar_t	speedmon;
extern cvar_t	allow_spectators;
extern cvar_t	sv_allowbunnyhopping;	// halflife-updated: physinfo "bj" for pm_shared
extern cvar_t	sv_infinite_ammo;		// SBO 2.0: physinfo "ia" for the client's weapon prediction
extern cvar_t	mp_killfeed_npc;	// chat-vgui2: kill feed NPC names (SboKillfeed_*)

// Engine Cvars
extern cvar_t	*g_psv_gravity;
extern cvar_t	*g_psv_aim;
extern cvar_t	*g_psv_allow_autoaim;

// HL25: autoaim needs sv_aim and the engine's sv_allow_autoaim (treated as 1 if the engine lacks it)
inline bool UTIL_AutoaimAllowed( void )
{
	return g_psv_aim->value != 0 && ( !g_psv_allow_autoaim || g_psv_allow_autoaim->value != 0 );
}
extern cvar_t	*g_footsteps;

#endif		// GAME_H
