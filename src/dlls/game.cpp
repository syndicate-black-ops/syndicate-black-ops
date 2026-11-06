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
#include "sbo_test.h"	// SBO_TEST_COMMANDS builds: the test layer (docs/dev/ENTITY_TESTS.md)
#include "gmmode.h"

#include "sbo_version_gen.h"	// generated in the build tree (cmake/sbo_version.cmake)

cvar_t	displaysoundlist = {"displaysoundlist","0"};

// SBO 2.0: this library's build, for server operators (console, server rules) and plugins. The name and the flag
// are ReGameDLL_CS's (game.cpp: game_version, FCVAR_SERVER). The version as the client's Version tab shows it, then
// the branch it was built from:
//   CI build:     "2.0.0+1a2b3c4d develop"
//   local build:  "2.0.0+dirty@HEAD-1a2b3c4d feat/server-version"
// Meant to be read only: the engine has no read-only flag for a game cvar, nothing in the game reads it back.
#define GAME_VERSION_CVAR	"game_version"
cvar_t	game_version = { GAME_VERSION_CVAR, SBO_VERSION_STRING " " SBO_VERSION_BRANCH, FCVAR_SERVER };

// multiplayer server rules
cvar_t	fragsleft	= {"mp_fragsleft","0", FCVAR_SERVER | FCVAR_UNLOGGED };	  // Don't spam console/log files/users with this changing
cvar_t	timeleft	= {"mp_timeleft","0" , FCVAR_SERVER | FCVAR_UNLOGGED };	  // "      "
// SBO 2.0: status rules like the two above, for server browsers (A2S_RULES); set by the game only
// (multiplay_gamerules.cpp SboModeInfo_Frame), 0 at map start. mp_round: the co_/br_ round in play or the last one
// played (0 before the first; bk_ / sp_ count no rounds). mp_roundlimit: co_ maps' round limit (CoopRoundLimit), else 0.
cvar_t	round_status	= {"mp_round","0", FCVAR_SERVER | FCVAR_UNLOGGED };
cvar_t	roundlimit_status = {"mp_roundlimit","0", FCVAR_SERVER | FCVAR_UNLOGGED };

// SBO 2.0: the defaults here and below are the values of the shipped server.cfg, which were also the ones the old
// game.cfg forced on every map load, but for mp_timelimit (15 there) and mp_gmmode (1 there). game.cfg is now empty;
// server.cfg / listenserver.cfg list them for server operators
// multiplayer server rules
cvar_t	teamplay	= {"mp_teamplay","0", FCVAR_SERVER };
cvar_t	fraglimit	= {"mp_fraglimit","0", FCVAR_SERVER };
cvar_t	timelimit	= {"mp_timelimit","20", FCVAR_SERVER };
cvar_t	friendlyfire= {"mp_friendlyfire","1", FCVAR_SERVER };

// SBO server rules
cvar_t	easymode	= {"mp_easymode","1", FCVAR_SERVER };
cvar_t	ban			= {"mp_ban","0", FCVAR_SERVER };
cvar_t	gmmode		= {"mp_gmmode","0", FCVAR_SERVER };
cvar_t	XP			= {"mp_XP","1", FCVAR_SERVER };
cvar_t	hardcore	= {"mp_hardcore","100", FCVAR_SERVER };
cvar_t	loadouttime	= {"mp_loadouttime","45", FCVAR_SERVER };	// SBO 2.0: co_/br_ rounds stop waiting for a player without a loadout this many seconds after the player joined or last used the loadout menus (0 = wait forever)
cvar_t	afk_time	= {"mp_afk_time","60", FCVAR_SERVER };	// SBO 2.0: a player in play without any input for this many seconds gets the AFK sign above the head (0 turns it off; player.cpp AfkThink)
cvar_t	afk_spectate_time = {"mp_afk_spectate_time","0", FCVAR_SERVER };	// SBO 2.0: ... and after this many seconds is moved into spectator mode (0 turns it off)
// SBO 2.0: seconds between a co-op mission's result and the intermission (CCoop::Think, ROUND_MISSION_END). Read every
// frame of the mission end and counted from the result, like mp_chattime in the intermission: a map vote raises it
// to hold the map end and lowers it again to go on. 1 to 600. Above 5 (SBO's time), the players still alive become
// observers 5 s after the result.
cvar_t	missionendtime	= {"mp_missionendtime","5", FCVAR_SERVER };
// SBO 2.0 semiclip (semiclip.cpp, docs/SEMICLIP.md; production's ReSemiclip module and its resemiclip.cfg): players
// walk through each other for mp_semiclip_time seconds after a round start (0 = always), and with
// mp_semiclip_effects 1 fade by distance while they do (0: a fixed transparency within 200 units)
cvar_t	semiclip	= {"mp_semiclip","1", FCVAR_SERVER };
cvar_t	semiclip_time = {"mp_semiclip_time","15", FCVAR_SERVER };
cvar_t	semiclip_effects = {"mp_semiclip_effects","1", FCVAR_SERVER };

// SBO 2.0 map compatibility (map_compat.cpp, docs/MAP_COMPAT.md): 1 = maps/map_compat.txt applies to the maps it
// names; 0 = every map loads its keys unchanged. Read when a map starts loading (set it before the map change).
cvar_t	map_compat	= {"mp_map_compat","1", FCVAR_SERVER };
cvar_t	juniorpowmonst = {"mp_juniorpowmonst","1", FCVAR_SERVER };
cvar_t	junior		= {"mp_junior","0", FCVAR_SERVER };
cvar_t	junioracmar	= {"mp_junioracmar","1", FCVAR_SERVER };
cvar_t	juniorpvmonst = {"mp_juniorpvmonst","1", FCVAR_SERVER };
cvar_t	makermon	= {"mp_makermon","1", FCVAR_SERVER };	// SBO: 0 = half, 1 = unscaled, 2 = double, 3 = triple monstermaker counts; production servers run 1
cvar_t	speedmon	= {"mp_speedmon","1", FCVAR_SERVER };

cvar_t	falldamage	= {"mp_falldamage","0", FCVAR_SERVER };
cvar_t	weaponstay	= {"mp_weaponstay","0", FCVAR_SERVER };
cvar_t	forcerespawn= {"mp_forcerespawn","1", FCVAR_SERVER };
cvar_t	flashlight	= {"mp_flashlight","1", FCVAR_SERVER };
cvar_t	aimcrosshair= {"mp_autocrosshair","1", FCVAR_SERVER };
cvar_t	decalfrequency = {"decalfrequency","30", FCVAR_SERVER };
cvar_t	teamlist = {"mp_teamlist","hgrunt;scientist", FCVAR_SERVER };
cvar_t	teamoverride = {"mp_teamoverride","1" };
cvar_t	defaultteam = {"mp_defaultteam","0" };

cvar_t	allow_spectators = { "allow_spectators", "1.0", FCVAR_SERVER };		// 0 prevents players from being spectators

cvar_t  mp_chattime = {"mp_chattime","10", FCVAR_SERVER };

// halflife-updated (halflife #11): 1 disables the bunny hopping speed limiter (PM_PreventMegaBunnyJumping);
// default 0 keeps the limiter, i.e. SBO's movement as before
cvar_t	sv_allowbunnyhopping = { "sv_allowbunnyhopping", "0", FCVAR_SERVER };

// SBO 2.0: 1 = weapons use no ammo and never need a reload (InfiniteAmmo(), weapons.cpp). Like ReGameDLL_CS's
// mp_infinite_ammo 1; the client's weapon prediction gets it from the "ia" physinfo key.
cvar_t	sv_infinite_ammo = { "sv_infinite_ammo", "0", FCVAR_SERVER };

// HL25: func_pushable pulling (+use) speed per second instead of per frame (CPushable::Move); < 0 = per frame as before
cvar_t	sv_pushable_fixed_tick_fudge = { "sv_pushable_fixed_tick_fudge", "15" };

// chat-vgui2 branch: kill feed NPC names (SboKillfeed_*, multiplay_gamerules.cpp; the metamod-syndicate-killfeed plugin)
cvar_t	mp_killfeed_npc = {"mp_killfeed_npc","1" };	// 0 off, 1 NPC kills of players + player kills of NPCs, 2 NPC kills of players only

// Engine Cvars
cvar_t 	*g_psv_gravity = NULL;
cvar_t	*g_psv_aim = NULL;
cvar_t	*g_psv_allow_autoaim = NULL;	// HL25: engine cvar sv_allow_autoaim (NULL on engines without it)
cvar_t	*g_footsteps = NULL;

// SBO: the skill cvars (sk_*) are gone; skill.cfg values are read by name (CVAR_GET_FLOAT).

// Register your console variables here
// This gets called one time when the game is initialied
extern void Spectate_LoadIds( void );	// client.cpp

extern bool g_worldExists;	// cbase.cpp
extern void KillFeedIcons_Init( void );	// killfeed_icons.cpp

// SBO 2.0 (Featureful 7a20f521): server command "point_contents <x> <y> <z>" prints the contents type at a point.
// Featureful prints with ALERT( at_console ), which GoldSrc shows only with developer 1: the same text goes to the
// server console with pfnServerPrint here.
static void PrintPointContents( void )
{
	if ( !g_worldExists )
	{
		g_engfuncs.pfnServerPrint( "The map is not running!\n" );
		return;
	}

	const int argc = CMD_ARGC();
	if ( argc < 4 )
	{
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( "Usage: %s <x> <y> <z>\n", CMD_ARGV( 0 ) ) );
		return;
	}

	const float x = atof( CMD_ARGV( 1 ) );
	const float y = atof( CMD_ARGV( 2 ) );
	const float z = atof( CMD_ARGV( 3 ) );

	const int contents = UTIL_PointContents( Vector( x, y, z ) );
	const char *contentsStr = NULL;
	switch ( contents )
	{
	case CONTENTS_EMPTY:
		contentsStr = "empty";
		break;
	case CONTENTS_SOLID:
		contentsStr = "solid";
		break;
	case CONTENTS_WATER:
		contentsStr = "water";
		break;
	case CONTENTS_SLIME:
		contentsStr = "slime";
		break;
	case CONTENTS_LAVA:
		contentsStr = "lava";
		break;
	case CONTENTS_SKY:
		contentsStr = "sky";
		break;
	case CONTENTS_LADDER:
		contentsStr = "ladder";
		break;
	}

	if ( contentsStr )
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( "Point contents at (%g, %g, %g) is %s\n", x, y, z, contentsStr ) );
	else
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( "Point contents at (%g, %g, %g) is %d\n", x, y, z, contents ) );
}

void GameDLLInit( void )
{
	Spectate_LoadIds();	// SBO 2.0 spectator mode: the spectators of the server's last run (spectators.txt)
	SBO_TEST_CALL( Test_GameInit() );	// test builds: the test_* commands (sbo_test.cpp)

	// Register cvars here:

	g_psv_gravity = CVAR_GET_POINTER( "sv_gravity" );
	g_psv_aim = CVAR_GET_POINTER( "sv_aim" );
	g_psv_allow_autoaim = CVAR_GET_POINTER( "sv_allow_autoaim" );
	g_footsteps = CVAR_GET_POINTER( "mp_footsteps" );

	CVAR_REGISTER (&displaysoundlist);
	CVAR_REGISTER (&game_version);	// SBO 2.0

	// SBO 2.0: the build in the server console at startup (ReGameDLL_CS: "ReGameDLL version: ..." in GameDLLInit)
	g_engfuncs.pfnServerPrint( "SBO " SBO_VERSION_STRING " " SBO_VERSION_BRANCH " (built " SBO_BUILD_DATE ")\n" );

	CVAR_REGISTER (&teamplay);
	CVAR_REGISTER (&fraglimit);
	CVAR_REGISTER (&timelimit);
	CVAR_REGISTER (&allow_spectators);

	CVAR_REGISTER (&fragsleft);
	CVAR_REGISTER (&timeleft);
	CVAR_REGISTER (&round_status);
	CVAR_REGISTER (&roundlimit_status);

	CVAR_REGISTER (&friendlyfire);
	CVAR_REGISTER (&easymode);
	CVAR_REGISTER (&ban);
	CVAR_REGISTER (&gmmode);
	GmMode_Init();	// SBO 2.0: the gm_toggle server command (gmmode.cpp)
	CVAR_REGISTER (&XP);
	CVAR_REGISTER (&hardcore);
	CVAR_REGISTER (&loadouttime);	// SBO 2.0
	CVAR_REGISTER (&afk_time);	// SBO 2.0
	CVAR_REGISTER (&afk_spectate_time);	// SBO 2.0
	CVAR_REGISTER (&missionendtime);	// SBO 2.0
	CVAR_REGISTER (&semiclip);	// SBO 2.0
	CVAR_REGISTER (&semiclip_time);	// SBO 2.0
	CVAR_REGISTER (&semiclip_effects);	// SBO 2.0
	CVAR_REGISTER (&map_compat);	// SBO 2.0: mp_map_compat
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

	CVAR_REGISTER (&mp_chattime);
	CVAR_REGISTER (&sv_allowbunnyhopping);	// halflife-updated
	CVAR_REGISTER (&sv_infinite_ammo);
	CVAR_REGISTER (&sv_pushable_fixed_tick_fudge);	// HL25
	CVAR_REGISTER (&mp_killfeed_npc);	// chat-vgui2

	g_engfuncs.pfnAddServerCommand( (char *)"point_contents", PrintPointContents );	// SBO 2.0 (Featureful 7a20f521)
	KillFeedIcons_Init();	// SBO 2.0: server command killfeed_icon (killfeed_icons.cpp)

	SERVER_COMMAND( "exec skill.cfg\n" );
}

