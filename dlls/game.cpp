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
#include "extdll.h"
#include "eiface.h"
#include "util.h"
#include "game.h"

cvar_t	displaysoundlist = {"displaysoundlist","0"};

// multiplayer server rules
cvar_t	fragsleft	= {"mp_fragsleft","0", FCVAR_SERVER | FCVAR_UNLOGGED };	  // Don't spam console/log files/users with this changing
cvar_t	timeleft	= {"mp_timeleft","0" , FCVAR_SERVER | FCVAR_UNLOGGED };	  // "      "

// multiplayer server rules
cvar_t	teamplay	= {"mp_teamplay","0", FCVAR_SERVER };
cvar_t	fraglimit	= {"mp_fraglimit","0", FCVAR_SERVER };
cvar_t	timelimit	= {"mp_timelimit","0", FCVAR_SERVER };
cvar_t	friendlyfire= {"mp_friendlyfire","0", FCVAR_SERVER };

// SBO server rules
cvar_t	easymode	= {"mp_easymode","0", FCVAR_SERVER };
cvar_t	ban			= {"mp_ban","0", FCVAR_SERVER };
cvar_t	gmmode		= {"mp_gmmode","0", FCVAR_SERVER };
cvar_t	XP			= {"mp_XP","0", FCVAR_SERVER };
cvar_t	hardcore	= {"mp_hardcore","0", FCVAR_SERVER };
cvar_t	juniorpowmonst = {"mp_juniorpowmonst","0", FCVAR_SERVER };
cvar_t	junior		= {"mp_junior","0", FCVAR_SERVER };
cvar_t	junioracmar	= {"mp_junioracmar","0", FCVAR_SERVER };
cvar_t	juniorpvmonst = {"mp_juniorpvmonst","0", FCVAR_SERVER };
cvar_t	makermon	= {"mp_makermon","0", FCVAR_SERVER };
cvar_t	speedmon	= {"mp_speedmon","0", FCVAR_SERVER };

cvar_t	falldamage	= {"mp_falldamage","0", FCVAR_SERVER };
cvar_t	weaponstay	= {"mp_weaponstay","0", FCVAR_SERVER };
cvar_t	forcerespawn= {"mp_forcerespawn","1", FCVAR_SERVER };
cvar_t	flashlight	= {"mp_flashlight","0", FCVAR_SERVER };
cvar_t	aimcrosshair= {"mp_autocrosshair","1", FCVAR_SERVER };
cvar_t	decalfrequency = {"decalfrequency","30", FCVAR_SERVER };
cvar_t	teamlist = {"mp_teamlist","hgrunt;scientist", FCVAR_SERVER };
cvar_t	teamoverride = {"mp_teamoverride","1" };
cvar_t	defaultteam = {"mp_defaultteam","0" };
cvar_t	allowmonsters={"mp_allowmonsters","0", FCVAR_SERVER };

cvar_t	allow_spectators = { "allow_spectators", "1.0", FCVAR_SERVER };		// 0 prevents players from being spectators

cvar_t  mp_chattime = {"mp_chattime","10", FCVAR_SERVER };

// Engine Cvars
cvar_t 	*g_psv_gravity = NULL;
cvar_t	*g_psv_aim = NULL;
cvar_t	*g_footsteps = NULL;

// SBO: the skill cvars (sk_*) are gone; skill.cfg values are read by name (CVAR_GET_FLOAT).

// Register your console variables here
// This gets called one time when the game is initialied
void GameDLLInit( void )
{
	// Register cvars here:

	g_psv_gravity = CVAR_GET_POINTER( "sv_gravity" );
	g_psv_aim = CVAR_GET_POINTER( "sv_aim" );
	g_footsteps = CVAR_GET_POINTER( "mp_footsteps" );

	CVAR_REGISTER (&displaysoundlist);

	CVAR_REGISTER (&teamplay);
	CVAR_REGISTER (&fraglimit);
	CVAR_REGISTER (&timelimit);
	CVAR_REGISTER (&allow_spectators);

	CVAR_REGISTER (&fragsleft);
	CVAR_REGISTER (&timeleft);

	CVAR_REGISTER (&friendlyfire);
	CVAR_REGISTER (&easymode);
	CVAR_REGISTER (&ban);
	CVAR_REGISTER (&gmmode);
	CVAR_REGISTER (&XP);
	CVAR_REGISTER (&hardcore);
	CVAR_REGISTER (&juniorpowmonst);
	CVAR_REGISTER (&junior);
	CVAR_REGISTER (&junioracmar);
	CVAR_REGISTER (&juniorpvmonst);
	CVAR_REGISTER (&makermon);
	CVAR_REGISTER (&speedmon);
	CVAR_REGISTER (&falldamage);
	CVAR_REGISTER (&weaponstay);
	CVAR_REGISTER (&forcerespawn);
	CVAR_REGISTER (&flashlight);
	CVAR_REGISTER (&aimcrosshair);
	CVAR_REGISTER (&decalfrequency);
	CVAR_REGISTER (&teamlist);
	CVAR_REGISTER (&teamoverride);
	CVAR_REGISTER (&defaultteam);
	CVAR_REGISTER (&allowmonsters);

	CVAR_REGISTER (&mp_chattime);

	SERVER_COMMAND( "exec skill.cfg\n" );
}

