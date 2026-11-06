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
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to 
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "com_model.h"
#include "saverestore.h"
#include "player.h"
#include "spectator.h"
#include "client.h"
#include "soundent.h"
#include "gamerules.h"
#include "game.h"
#include "customentity.h"
#include "sbo_test.h"	// SBO_TEST_COMMANDS builds: the test layer (docs/dev/ENTITY_TESTS.md)
#include "weapons.h"
#include "weaponinfo.h"
#include "usercmd.h"
#include "netadr.h"
#include "pm_shared.h"	// OBS_* spectator modes
#include "tutor.h"
#include "game_messagebox.h"
#include "semiclip.h"
#include "map_compat.h"	// SBO 2.0
#include "render_fade.h"	// SBO 2.0

extern BOOL g_bAlwaysSend[ MAX_EDICTS ];	// SBO 2.0: env_model.cpp
#include "vgui_screen.h"	// SBO 2.0: world screens
#include "version_gate.h"	// SBO 2.0: the version gate
#include "gmmode.h"

#ifndef _WIN32
#define _snprintf snprintf
#endif

extern DLL_GLOBAL ULONG		g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL		g_fGameOver;
extern BOOL g_fSaveRestored;	// SBO: cbase.cpp, DispatchRestore
extern DLL_GLOBAL int		g_iSkillLevel;
extern DLL_GLOBAL ULONG		g_ulFrameCount;

extern void CopyToBodyQue(entvars_t* pev);
extern int giPrecacheGrunt;
extern int gmsgSayText;
extern void ShowVGUI( CBasePlayer *pPlayer, int iMenu );

extern int g_teamplay;

void LinkUserMessages( void );
void KillFeedIcons_Reset( void );	// SBO 2.0: killfeed_icons.cpp

/*
 * used by kill command and disconnect command
 * ROBIN: Moved here from player.cpp, to allow multiple player models
 */
void set_suicide_frame(entvars_t* pev)
{       
	if (!FStrEq(STRING(pev->model), "models/player.mdl"))
		return; // allready gibbed

//	pev->frame		= $deatha11;
	pev->solid		= SOLID_NOT;
	pev->movetype	= MOVETYPE_TOSS;
	pev->deadflag	= DEAD_DEAD;
	pev->nextthink	= -1;
}


/*
===========
ClientConnect

called when a player connects to a server
============
*/
static void Spectate_ClientConnect( edict_t *pEntity, const char *pszAddress );

BOOL ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ]  )
{	
	// SBO 2.0 version gate (version_gate.cpp): first, so nothing below runs for a client that is refused
	if ( !VersionGate_ClientConnect( pEntity, pszName, pszAddress, szRejectReason ) )
		return FALSE;

	Spectate_ClientConnect( pEntity, pszAddress );	// SBO 2.0 spectator mode: the address, for a LAN client's identity

	return g_pGameRules->ClientConnected( pEntity, pszName, pszAddress, szRejectReason );

// a client connecting during an intermission can cause problems
//	if (intermission_running)
//		ExitIntermission ();

}


//=========================================================
// SBO 2.0 spectator mode (CBasePlayer::StartSpectating / StopSpectating, observer.cpp), persistent: a spectator
// stays one through rounds, deaths, map changes, reconnects and server restarts until toggling it off. Two records,
// kept in the game DLL's memory for the server's lifetime (the DLL stays loaded through map changes; no cvar):
//   - per player slot, the userid of the client spectating in it. The engine keeps its clients across a changelevel
//     (no ClientDisconnect, the same userid; only the player entity is new). Covers every client, also LAN ones and
//     players whose identity isn't known (yet).
//   - the identities (Spectate_GetId) of the players spectating, for a reconnect (a new userid, maybe another slot).
//     The Steam / WON ids among them are also kept in <gamedir>/spectators.txt for a server restart, as production's
//     spectate module kept its list (spectators.ini): read once when the DLL starts (Spectate_LoadIds, GameDLLInit),
//     written whenever one of them enters or leaves the mode (Spectate_SaveIds), so a crash loses nothing.
// ClientPutInServer restores the mode from either; UpdateClientData starts it again after the mode's InitHUD, and
// asks again for an identity Steam hasn't validated yet (Spectate_CheckPendingId).
//=========================================================
#define SPECTATE_TOGGLE_DELAY	3.0f	// seconds between two toggles (anti-spam)
#define SPECTATE_MAX_IDS		256		// identities remembered; when full, the oldest is forgotten
#define SPECTATE_ID_LEN			96
#define SPECTATE_FILE			"spectators.txt"	// in the game directory

static int s_iSpectatorUserId[ 32 + 1 ];		// per slot: userid of the client spectating in it, 0 = none
static char s_szClientAddress[ 32 + 1 ][ 64 ];	// per slot: the client's address without the port (ClientConnect)
static char s_szSpectatorIds[ SPECTATE_MAX_IDS ][ SPECTATE_ID_LEN ];	// identities of spectating players
static int s_iSpectatorIds;

enum
{
	SPECTATE_ID_NONE,		// no identity: not kept past a disconnect
	SPECTATE_ID_PENDING,	// not known yet (STEAM_ID_PENDING)
	SPECTATE_ID_OK
};

// an id of the player's own ("STEAM_0:1:2345", "VALVE_0:1:2345"), not one several clients share ("STEAM_ID_LAN",
// "VALVE_ID_LAN", "STEAM_ID_PENDING", ...). Only these are stable enough for the file.
static BOOL Spectate_IsSteamId( const char *pszId )
{
	return ( !strncmp( pszId, "STEAM_", 6 ) || !strncmp( pszId, "VALVE_", 6 ) ) && !strstr( pszId, "_ID_" )
		&& !strchr( pszId, ' ' );
}

static void Spectate_ClientConnect( edict_t *pEntity, const char *pszAddress )
{
	int iSlot = ENTINDEX( pEntity );

	if ( iSlot < 1 || iSlot > 32 )
		return;

	char *psz = s_szClientAddress[iSlot];
	strncpy( psz, pszAddress ? pszAddress : "", sizeof( s_szClientAddress[0] ) - 1 );
	psz[ sizeof( s_szClientAddress[0] ) - 1 ] = '\0';

	char *pszPort = strrchr( psz, ':' );	// "192.168.1.2:27005"; the listen server's host is "loopback"
	if ( pszPort )
		*pszPort = '\0';
}

// The player's identity across reconnects:
//   - a Steam or WON id ("STEAM_0:1:2345", "VALVE_0:1:2345"): that id;
//   - "STEAM_ID_PENDING" / "VALVE_ID_PENDING" (Steam hasn't validated the client yet): pending;
//   - bots (FL_FAKECLIENT, "BOT") and HLTV proxies (FL_PROXY, "HLTV"): none;
//   - any other id is shared by several clients (LAN servers give every client "STEAM_ID_LAN" / "VALVE_ID_LAN"; the
//     listen server's host, "UNKNOWN"): the address and the name, "<address> <name>", so two game instances on one
//     machine stay apart. A player who renames before reconnecting starts playing.
static int Spectate_GetId( CBasePlayer *pPlayer, char *pszId, int iSize )
{
	if ( pPlayer->pev->flags & ( FL_FAKECLIENT | FL_PROXY ) )
		return SPECTATE_ID_NONE;

	const char *pszAuth = GETPLAYERAUTHID( pPlayer->edict() );
	if ( !pszAuth )
		pszAuth = "";

	if ( !strcmp( pszAuth, "BOT" ) || !strcmp( pszAuth, "HLTV" ) )
		return SPECTATE_ID_NONE;

	if ( strstr( pszAuth, "_ID_PENDING" ) )
		return SPECTATE_ID_PENDING;

	if ( Spectate_IsSteamId( pszAuth ) )
	{
		_snprintf( pszId, iSize, "%s", pszAuth );
	}
	else
	{
		int iSlot = pPlayer->entindex();

		if ( iSlot < 1 || iSlot > 32 || !s_szClientAddress[iSlot][0] || !STRING( pPlayer->pev->netname )[0] )
			return SPECTATE_ID_NONE;

		_snprintf( pszId, iSize, "%s %s", s_szClientAddress[iSlot], STRING( pPlayer->pev->netname ) );
	}

	pszId[iSize - 1] = '\0';
	return SPECTATE_ID_OK;
}

static int Spectate_FindId( const char *pszId )
{
	for ( int i = 0; i < s_iSpectatorIds; i++ )
	{
		if ( !strcmp( s_szSpectatorIds[i], pszId ) )
			return i;
	}
	return -1;
}

// a new identity at the end of the list. TRUE if the file's content changed with it (Spectate_SaveIds).
static BOOL Spectate_AddId( const char *pszId )
{
	BOOL bFile = Spectate_IsSteamId( pszId );

	if ( s_iSpectatorIds == SPECTATE_MAX_IDS )	// full: forget the oldest
	{
		if ( Spectate_IsSteamId( s_szSpectatorIds[0] ) )
			bFile = TRUE;

		memmove( s_szSpectatorIds[0], s_szSpectatorIds[1], sizeof( s_szSpectatorIds[0] ) * ( SPECTATE_MAX_IDS - 1 ) );
		s_iSpectatorIds--;
	}

	strncpy( s_szSpectatorIds[s_iSpectatorIds], pszId, SPECTATE_ID_LEN - 1 );
	s_szSpectatorIds[s_iSpectatorIds][SPECTATE_ID_LEN - 1] = '\0';
	s_iSpectatorIds++;

	return bFile;
}

// <gamedir>/spectators.txt: the Steam / WON ids of the list, one per line, oldest first. Not the "<address> <name>"
// identities of LAN clients and the listen server's host: they aren't stable (nor are bots, HLTV proxies and pending
// ids, which never get into the list). Written whenever such an id enters or leaves the list, and only then.
static void Spectate_SaveIds( void )
{
	char szFile[MAX_PATH];

	GET_GAME_DIR( szFile );
	strcat( szFile, "/" SPECTATE_FILE );

	FILE *fp = fopen( szFile, "w" );

	if ( !fp )
	{
		ALERT( at_console, "Couldn't write %s\n", szFile );
		return;
	}

	fprintf( fp, "// Players in spectator mode (spectate, /spec). Written by the game when a player enters or leaves the mode, read at server start. One Steam id per line.\n" );

	for ( int i = 0; i < s_iSpectatorIds; i++ )
	{
		if ( Spectate_IsSteamId( s_szSpectatorIds[i] ) )
			fprintf( fp, "%s\n", s_szSpectatorIds[i] );
	}

	fclose( fp );
}

// the game DLL starts (GameDLLInit: once per server start, before the first map): the spectators of the last run
void Spectate_LoadIds( void )
{
	char szFile[MAX_PATH];
	char szLine[256];

	GET_GAME_DIR( szFile );
	strcat( szFile, "/" SPECTATE_FILE );

	FILE *fp = fopen( szFile, "r" );

	if ( !fp )
		return;	// nobody spectating, or the first start

	while ( fgets( szLine, sizeof( szLine ), fp ) )
	{
		char *psz = szLine;

		while ( *psz == ' ' || *psz == '\t' )
			psz++;

		int iLen = strlen( psz );

		while ( iLen > 0 && ( psz[iLen - 1] == '\n' || psz[iLen - 1] == '\r' || psz[iLen - 1] == ' ' || psz[iLen - 1] == '\t' ) )
			psz[--iLen] = '\0';

		// the comment line, and anything that is no id the game would have written
		if ( iLen < 1 || iLen >= SPECTATE_ID_LEN || !Spectate_IsSteamId( psz ) )
			continue;

		if ( Spectate_FindId( psz ) < 0 )
			Spectate_AddId( psz );	// more than SPECTATE_MAX_IDS lines: the last ones stay
	}

	fclose( fp );
}

// keep the player's spectator mode for a changelevel (slot + userid), a reconnect and a server restart (identity)
static void Spectate_Remember( CBasePlayer *pPlayer )
{
	int i = pPlayer->entindex();

	if ( i >= 1 && i <= 32 )
		s_iSpectatorUserId[i] = pPlayer->m_bSpectating ? GETPLAYERUSERID( pPlayer->edict() ) : 0;

	char szId[SPECTATE_ID_LEN];
	int iState = Spectate_GetId( pPlayer, szId, sizeof( szId ) );

	if ( iState == SPECTATE_ID_PENDING )
		pPlayer->m_bSpectateIdPending = TRUE;	// recorded once known (Spectate_CheckPendingId)

	if ( iState != SPECTATE_ID_OK )
		return;

	int iFound = Spectate_FindId( szId );
	BOOL bFile = FALSE;

	if ( pPlayer->m_bSpectating && iFound < 0 )
	{
		bFile = Spectate_AddId( szId );
	}
	else if ( !pPlayer->m_bSpectating && iFound >= 0 )
	{
		memmove( s_szSpectatorIds[iFound], s_szSpectatorIds[iFound + 1], sizeof( s_szSpectatorIds[0] ) * ( s_iSpectatorIds - iFound - 1 ) );
		s_iSpectatorIds--;
		bFile = Spectate_IsSteamId( szId );
	}

	if ( bFile )
		Spectate_SaveIds();
}

// a new player entity (connect, reconnect, changelevel): was this client or player spectating? An identity Steam
// hasn't validated yet is asked for again every frame (Spectate_CheckPendingId).
static void Spectate_Restore( CBasePlayer *pPlayer )
{
	int iSlot = pPlayer->entindex();

	if ( iSlot < 1 || iSlot > 32 )
		return;

	pPlayer->m_bSpectating = s_iSpectatorUserId[iSlot] != 0 && s_iSpectatorUserId[iSlot] == GETPLAYERUSERID( pPlayer->edict() );
	pPlayer->m_bSpectateIdPending = FALSE;
	pPlayer->m_bSpectateMenuExit = FALSE;

	char szId[SPECTATE_ID_LEN];
	int iState = Spectate_GetId( pPlayer, szId, sizeof( szId ) );

	if ( iState == SPECTATE_ID_OK && Spectate_FindId( szId ) >= 0 )
		pPlayer->m_bSpectating = TRUE;
	else if ( iState == SPECTATE_ID_PENDING && !pPlayer->m_bSpectating )
		pPlayer->m_bSpectateIdPending = TRUE;

	s_iSpectatorUserId[iSlot] = pPlayer->m_bSpectating ? GETPLAYERUSERID( pPlayer->edict() ) : 0;
}

// called every frame while the player's identity is pending (CBasePlayer::UpdateClientData). Once Steam has
// validated the client: a reconnecting spectator spectates again (before the mode's InitHUD: UpdateClientData starts
// it there), and a toggle made meanwhile is recorded for the identity.
void Spectate_CheckPendingId( CBasePlayer *pPlayer )
{
	char szId[SPECTATE_ID_LEN];
	int iState = Spectate_GetId( pPlayer, szId, sizeof( szId ) );

	if ( iState == SPECTATE_ID_PENDING )
		return;

	pPlayer->m_bSpectateIdPending = FALSE;

	if ( iState == SPECTATE_ID_OK && !pPlayer->m_bSpectating && Spectate_FindId( szId ) >= 0 )
	{
		if ( !pPlayer->m_fGameHUDInitialized )
		{
			pPlayer->m_bSpectating = TRUE;
		}
		else
		{
			pPlayer->StartSpectating();
			ClientPrint( pPlayer->pev, HUD_PRINTTALK, "You are spectating. Say /spectate to join the game.\n" );
		}
	}

	Spectate_Remember( pPlayer );
}

// back in the game ("spectate" again, or the selection menu's "Go !"): the mode's way in (StopSpectating), and the
// identity is forgotten
static void Spectate_Leave( CBasePlayer *pPlayer )
{
	pPlayer->m_bSpectateMenuExit = FALSE;
	pPlayer->StopSpectating();
	if ( g_fGameOver )
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, "You left spectator mode: you play on the next map.\n" );

	pPlayer->m_flNextSpectateToggle = gpGlobals->time + SPECTATE_TOGGLE_DELAY;
	Spectate_Remember( pPlayer );
}

// A spectator confirmed the selection menu: its "Go !" button sends blade, light, heavy, item, cyberware, armor and
// malefemale at once (cl_dll CMenuHandler_Go, CSboBriefMenu::Go), also with nothing changed; "Cancel" sends nothing.
// "item" is the step that puts a waiting player in the game (CCoop / CDm::ClientCommand), so for a spectator it
// leaves spectator mode like "spectate" does. Called from CBasePlayer::UpdateClientData (PreThink) on the player's
// next move: the commands the menu sends after "item" come in ahead of it, so the player joins (Bounty Killer: spawns
// at once) with the whole loadout. The toggle delay doesn't hold this back (the menu is no way to spam the mode: getting in again
// is the toggle, which waits), but it starts again.
void Spectate_MenuConfirmed( CBasePlayer *pPlayer )
{
	pPlayer->m_bSpectateMenuExit = FALSE;

	if ( pPlayer->m_bSpectating )
		Spectate_Leave( pPlayer );
}

// SBO 2.0 AFK: a player in play gave no input for mp_afk_spectate_time seconds (CBasePlayer::AfkThink). Spectator
// mode as through "spectate": the same round accounting (StartSpectating), kept across map changes and reconnects,
// left with "spectate" or the selection menu's "Go !". No toggle delay: the player may be back the next second.
// Everybody else is told in chat, as the plugin did; the player gets the line that says how to come back instead.
// Not written to the server log (neither is "spectate").
void Spectate_MoveAfk( CBasePlayer *pPlayer )
{
	if ( pPlayer->m_bSpectating )
		return;

	pPlayer->StartSpectating();
	ClientPrint( pPlayer->pev, HUD_PRINTTALK, "You were moved to spectator mode for being AFK. Say /spectate to join the game.\n" );
	Spectate_Remember( pPlayer );

	char text[256];
	_snprintf( text, sizeof( text ), "%s was moved to spectator mode for being AFK longer than %d seconds\n",
		UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ), (int)afk_spectate_time.value );
	text[sizeof( text ) - 1] = '\0';

	for ( int iClient = 1; iClient <= gpGlobals->maxClients; iClient++ )
	{
		CBasePlayer *client = (CBasePlayer *)UTIL_PlayerByIndex( iClient );

		if ( !client || !client->pev || client->has_disconnected || client == pPlayer || !client->IsNetClient() )
			continue;

		MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, client->pev );
			WRITE_BYTE( pPlayer->entindex() );
			WRITE_STRING( text );
		MESSAGE_END();
	}
}

// "spectate" in the console, /spec, /spectate, .spec, .spectate in chat
static void Spectate_Toggle( CBasePlayer *pPlayer )
{
	if ( pPlayer->pev->flags & FL_PROXY )
		return;

	if ( pPlayer->m_flNextSpectateToggle > gpGlobals->time )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, "Wait a moment before switching spectator mode again.\n" );
		return;
	}

	if ( !pPlayer->m_bSpectating )
	{
		// a Game Master runs the game; turning GM mode off ("gm mode off" in the GM menu) comes first
		if ( pPlayer->m_bGameMaster == 1 )
		{
			GmMode_Notice( pPlayer, "A Game Master can't spectate: turn GM mode off first." );
			return;
		}

		pPlayer->StartSpectating();
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, g_fGameOver ? "You will spectate on the next map. Say /spectate to join the game.\n"
			: "You are spectating. Say /spectate to join the game.\n" );
	}
	else
	{
		Spectate_Leave( pPlayer );
		return;
	}

	pPlayer->m_flNextSpectateToggle = gpGlobals->time + SPECTATE_TOGGLE_DELAY;
	Spectate_Remember( pPlayer );
}

// say text that toggles spectator mode: /spec, /spectate, .spec, .spectate (any case, surrounding spaces allowed)
static BOOL Spectate_IsChatCommand( const char *p )
{
	while ( *p == ' ' )
		p++;

	int len = (int)strlen( p );
	while ( len > 0 && p[len - 1] == ' ' )
		len--;

	if ( len < 1 || ( p[0] != '/' && p[0] != '.' ) )
		return FALSE;

	return ( len == 5 && !strnicmp( p + 1, "spec", 4 ) ) || ( len == 9 && !strnicmp( p + 1, "spectate", 8 ) );
}

// commands a spectator can't use: the team menu (ShowVGUI 2, the MOTD's OK; the way into a round on a new map), the
// BR go menu, radio and Game Master menus (menuselect), radio, GM mode. Like a dead player, a spectator keeps the
// selection menu (changemodel, ShowVGUI 10: the equipment / weapon menu) and its steps: confirming it ("Go !", the
// "item" step: CCoop / CDm::ClientCommand) leaves spectator mode with that loadout (Spectate_MenuConfirmed).
static BOOL Spectate_BlocksCommand( const char *pcmd )
{
	if ( FStrEq( pcmd, "ShowVGUI" ) )
		return atoi( CMD_ARGV(1) ) != 10;

	static const char *s_pszBlocked[] = { "menuselect", "radio", "gmon" };

	for ( int i = 0; i < (int)ARRAYSIZE( s_pszBlocked ); i++ )
	{
		if ( FStrEq( pcmd, s_pszBlocked[i] ) )
			return TRUE;
	}
	return FALSE;
}

/*
===========
ClientDisconnect

called when a player disconnects from a server

GLOBALS ASSUMED SET:  g_fGameOver
============
*/
void ClientDisconnect( edict_t *pEntity )
{
	SboScoreboard_ClientDisconnected( pEntity );	// chat-vgui2: clear the scoreboard entry
	Tutor_ClientDisconnected( pEntity );	// CS:CZ tutor window: the slot's queue and env_tutor show counts
	MsgBox_ClientDisconnected( pEntity );	// game_messagebox: the slot's open boxes
	Semiclip_ClientDisconnect( pEntity );	// SBO 2.0 semiclip: the slot's state

	// SBO 2.0 spectator mode: the next client in this slot starts clean; the player's identity stays remembered for
	// a reconnect (Spectate_Remember)
	int iSlot = ENTINDEX( pEntity );
	if ( iSlot >= 1 && iSlot <= 32 )
		s_iSpectatorUserId[iSlot] = 0;

	// SBO 2.0 AFK: the sign goes with the player (the next client in this slot has none)
	if ( pEntity->pvPrivateData )
		GetClassPtr((CBasePlayer *)&pEntity->v)->AfkSetMarked( FALSE );

	// SBO 2.0 head scale: the next client in this slot has a normal head
	if ( pEntity->pvPrivateData )
		GetClassPtr((CBasePlayer *)&pEntity->v)->SetHeadScale( 1.0f );

	if (g_fGameOver)
		return;

	char text[256];
	sprintf( text, "%s has left the game\n", UTIL_ChatPlayerName( STRING(pEntity->v.netname), pEntity ) );
	MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
		WRITE_BYTE( ENTINDEX(pEntity) );
		WRITE_STRING( text );
	MESSAGE_END();

	CSound *pSound;
	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( pEntity ) );
	{
		// since this client isn't around to think anymore, reset their sound. 
		if ( pSound )
		{
			pSound->Reset();
		}
	}

// since the edict doesn't get deleted, fix it so it doesn't interfere.
	pEntity->v.takedamage = DAMAGE_NO;// don't attract autoaim
	pEntity->v.solid = SOLID_NOT;// nonsolid
	UTIL_SetOrigin ( &pEntity->v, pEntity->v.origin );

	// ReGameDLL_CS: the edict stays, so mark it before the game rules move this player's spectators on
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE( pEntity );
	if ( pPlayer )
	{
		pPlayer->has_disconnected = TRUE;

		// halflife-updated (halflife #2594): stop controlling a func_tank on disconnect (crash); also the train /
		// vehicle controls (the vehicle's driver)
		pPlayer->ReleaseControls();

		// a trigger_camera that still runs for this player must not hand the view to this slot when it ends
		pPlayer->m_hViewEntity = NULL;
	}


	g_pGameRules->ClientDisconnected( pEntity );
}


// called by ClientKill and DeadThink
void respawn(entvars_t* pev, BOOL fCopyCorpse)
{
	// SBO: every game mode has multiplayer rules, also in a one-player session ("maxplayers 1", where the engine
	// clears gpGlobals->deathmatch): ask the rules, so the player is respawned and the map isn't restarted ("reload")
	if ( g_pGameRules->IsMultiplayer() )
	{
		if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			CopyToBodyQue(pev);
		}

		// respawn player
		GetClassPtr( (CBasePlayer *)pev)->Spawn( );
	}
	else
	{       // restart the entire server
		SERVER_COMMAND("reload\n");
	}
}

/*
============
ClientKill

Player entered the suicide command

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
============
*/
void ClientKill( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;

	CBasePlayer *pl = (CBasePlayer*) CBasePlayer::Instance( pev );

	if ( pl->m_afPhysicsFlags & PFLAG_OBSERVER )
		return;

	if ( pl->m_fNextSuicideTime > gpGlobals->time )
		return;  // prevent suiciding too ofter

	pl->m_fNextSuicideTime = gpGlobals->time + 1;  // don't let them suicide for 5 seconds after suiciding

	// have the player kill themself
	pev->health = 0;
	pl->Killed( pev, GIB_NEVER );

//	pev->modelindex = g_ulModelIndexPlayer;
//	pev->frags -= 2;		// extra penalty
//	respawn( pev );
}

/*
===========
ClientPutInServer

called each time a player is spawned
============
*/
void ClientPutInServer( edict_t *pEntity )
{
	CBasePlayer *pPlayer;

	entvars_t *pev = &pEntity->v;

	pPlayer = GetClassPtr((CBasePlayer *)pev);
	pPlayer->SetCustomDecalFrames(-1); // Assume none;

	// halflife-updated: cl_autowepswitch. The connect-time userinfo reaches ClientUserInfoChanged before the player
	// exists (no private data), so read it here for the first spawn.
	pPlayer->SetPrefsFromUserinfo( g_engfuncs.pfnGetInfoKeyBuffer( pEntity ) );

	SboScoreboard_ClientPutInServer( pPlayer );	// chat-vgui2: not on the scoreboard until InitHUD

	Tutor_ClientPutInServer( pPlayer );	// CS:CZ tutor window (before Spawn)
	MsgBox_ClientPutInServer( pPlayer );	// game_messagebox: no open boxes on a new connection / map

	// spectator modes: a new client is a target again (ReGameDLL_CS ClientPutInServer), and starts observing in
	// the mode SBO 1.38 always used (the halflife SDK 2.3 sets OBS_ROAMING at the first InitHUD)
	pPlayer->has_disconnected = FALSE;
	pPlayer->m_iObserverLastMode = OBS_CHASE_LOCKED;

	// A new client gets the game's InitHUD (ReGameDLL_CS ClientPutInServer). The edict keeps the previous client's
	// CBasePlayer when a bot plugin calls the "player" export on a slot a player just left (jk_botti does not free
	// the private data): with the old m_fGameHUDInitialized the bot never got InitHUD, so it never entered the game
	// (no log line, no mode InitHUD) and stayed off every scoreboard category (SboScoreboard_ClientInit).
	pPlayer->m_fGameHUDInitialized = FALSE;

	// SBO 2.0: still spectating after a changelevel (same slot, same userid) or a reconnect (same identity):
	// UpdateClientData starts it again after the mode's InitHUD. Anybody else starts clean.
	Spectate_Restore( pPlayer );

	// SBO 2.0 AFK: a new client starts clean
	pPlayer->m_flAfkTime = 0;
	pPlayer->m_flAfkLastThink = 0;
	pPlayer->m_flAfkNextLog = 0;
	pPlayer->m_bAfkWasInPlay = FALSE;
	pPlayer->m_bAfkInput = FALSE;
	pPlayer->m_bAfkMarked = FALSE;

	pPlayer->m_iTextMenu = TEXTMENU_NONE;	// no game text menu on screen yet

	// Allocate a CBasePlayer for pev, and call spawn
	pPlayer->Spawn() ;

	// Reset interpolation during first frame
	pPlayer->pev->effects |= EF_NOINTERP;
}

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
// chat-vgui2 branch: SBO's chat rules are replaced. Every message (say and say_team alike, say_team is kept only so
// old binds work) goes to every player in every game mode, whatever their state (dead, alive, observer, round or
// vote state). The text starts with \x02 so the client colours the sender's name (BugfixedHL-Rebased format); there
// is no "(TEAM)" prefix and no trailing short any more; the "*DEAD*"-style tag is now the scoreboard category's.
// Player-typed colour codes (^0-^9, ^xRGB, ^#RRGGBB, \x01-\x08) pass through; other control characters are replaced by spaces. Flood
// protection is BugfixedHL-Rebased's.
//
void Host_Say( edict_t *pEntity, int teamonly )
{
	CBasePlayer *client;
	int		j;
	char	*p;
	char	*pc;
	char	text[190];		// SayText: 1 byte + string (incl. terminator) must fit in the 192-byte user message limit
	char    szTemp[256];
	const char *cpSay = "say";
	const char *cpSayTeam = "say_team";
	const char *pcmd = CMD_ARGV(0);
	const int cmdc = CMD_ARGC();

	// We can get a raw string now, without the "say " prepended
	if ( cmdc == 0 )
		return;

	entvars_t *pev = &pEntity->v;
	CBasePlayer* player = GetClassPtr((CBasePlayer *)pev);

	// Flood check (BugfixedHL-Rebased). The engine's real time is used instead of gpGlobals->time, which stops
	// during a pause.
	float asyncTime = g_engfuncs.pfnTime();

	if ( player->m_flNextChatTime > asyncTime )
	{
		if ( player->m_iChatFlood >= CHAT_FLOOD )
		{
			// SBO 2.0: the dropped message's sender is told in the console, as AMX Mod X's antiflood plugin did
			// (chkFlood: client_print print_notify, "** %L **" STOP_FLOOD)
			ClientPrint( pev, HUD_PRINTNOTIFY, "** Stop flooding the server! **\n" );
			player->m_flNextChatTime = asyncTime + CHAT_INTERVAL + CHAT_PENALTY;
			return;
		}

		player->m_iChatFlood++;
	}
	else if ( player->m_iChatFlood )
	{
		player->m_iChatFlood--;
	}

	player->m_flNextChatTime = asyncTime + CHAT_INTERVAL;

	if ( !stricmp( pcmd, cpSay ) || !stricmp( pcmd, cpSayTeam ) )
	{
		if ( cmdc > 1 )
		{
			_snprintf( szTemp, sizeof(szTemp), "%s", (char *)CMD_ARGS() );
		}
		else
		{
			// say with a blank message, nothing to do
			return;
		}
	}
	else  // Raw text, need to prepend argv[0]
	{
		if ( cmdc > 1 )
		{
			_snprintf( szTemp, sizeof(szTemp), "%s %s", ( char * )pcmd, (char *)CMD_ARGS() );
		}
		else
		{
			// Just a one word command, use the first word...sigh
			_snprintf( szTemp, sizeof(szTemp), "%s", ( char * )pcmd );
		}
	}
	szTemp[ sizeof(szTemp) - 1 ] = 0;
	p = szTemp;

	// Keep the message protocol intact: \x00 can't arrive here (it ends the string); the colour codes \x01-\x08
	// stay; every other control character (tab, newline, carriage return, ..., DEL) becomes a space.
	for ( pc = p; *pc != 0; pc++ )
	{
		unsigned char c = (unsigned char)*pc;
		if ( ( c >= 0x09 && c < 0x20 ) || c == 0x7f )
			*pc = ' ';
	}

	// remove empty spaces at the end
	for ( pc = p + (int)strlen( p ) - 1; pc >= p && *pc == ' '; pc-- )
		*pc = 0;

	// remove surrounding quotes if present
	int len = (int)strlen( p );
	if ( len >= 2 && p[0] == '"' && p[len - 1] == '"' )
	{
		p[len - 1] = 0;
		p++;
	}
	else if ( len >= 1 && p[0] == '"' )
	{
		p++;	// unbalanced leading quote (the engine's say command adds them in pairs)
	}

	// remove empty spaces at the end
	for ( pc = p + (int)strlen( p ) - 1; pc >= p && *pc == ' '; pc-- )
		*pc = 0;

	// make sure the text has content
	for ( pc = p; pc != NULL && *pc != 0; pc++ )
	{
		unsigned char c = (unsigned char)*pc;
		if ( c >= 0x80 ||	// UTF-8 symbol
			 ( isprint( c ) && !isspace( c ) ) )
		{
			pc = NULL;	// we've found an alphanumeric character,  so text is valid
			break;
		}
	}
	if ( pc != NULL )
		return;  // no character found, so say nothing

	// SBO 2.0 spectator mode: /spec, /spectate, .spec, .spectate toggle it; like a command, the line isn't shown
	if ( Spectate_IsChatCommand( p ) )
	{
		Spectate_Toggle( player );
		return;
	}

	// SBO 2.0 GM mode: /gmspawn and /gmmenu open a Game Master's menu (the owner's GM mode plugin: the main menu;
	// here the quick menu); like a command, the line isn't shown
	if ( player->m_bGameMaster && ( !stricmp( p, "/gmspawn" ) || !stricmp( p, "/gmmenu" ) ) )
	{
		GmMode_ClientCommand( player, "gm_quick" );
		return;
	}

	// "\x02<prefix><name>: " (the sender index is the message's first byte): the client colours the name by the
	// sender's scoreboard team (SboScoreboard_*: 1 Game Master/Hunted blue, 2 Dead Ops red, 3 No Loadout yellow, 4 Alive
	// Ops green, 5 Spectators grey), so chat and scoreboard always agree. The prefix is the category
	// tag ("*DEAD* ", "*SPEC* ", "*NOLOAD* ", "*GM* ", none while alive; SboScoreboard_ChatPrefix), plain chat text:
	// the clients colour only the name (hud_chat.cpp / saytext.cpp skip the tag when they look for it).
	_snprintf( text, sizeof(text), "%c%s%s: ", 2, SboScoreboard_ChatPrefix( player ), STRING( pEntity->v.netname ) );
	text[ sizeof(text) - 1 ] = 0;

	j = sizeof(text) - 2 - strlen(text);  // -2 for /n and null terminator
	if ( j < 0 )
		j = 0;
	if ( (int)strlen(p) > j )
	{
		// don't cut a UTF-8 sequence in half
		while ( j > 0 && ( (unsigned char)p[j] & 0xC0 ) == 0x80 )
			j--;
		p[j] = 0;
	}

	strcat( text, p );
	strcat( text, "\n" );

	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	// SBO 2.0: by player index, not by classname "player": SBO's waiting players (InitHUD, "Waiting for a carnage")
	// are renamed "observer" and got no chat, also persistent spectators on a new map (production's syndicate_chat_fix)
	for ( int iClient = 1; iClient <= gpGlobals->maxClients; iClient++ )
	{
		client = (CBasePlayer *)UTIL_PlayerByIndex( iClient );

		if ( !client || !client->pev || client->has_disconnected )
			continue;
		
		if ( client->edict() == pEntity )
			continue;

		if ( !(client->IsNetClient()) )	// Not a client ? (should never be true)
			continue;

		// can the receiver hear the sender? or has he muted him?
		if ( g_VoiceGameMgr.PlayerHasBlockedPlayer( client, player ) )
			continue;

		MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, client->pev );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();
	}

	// print to the sending client
	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, &pEntity->v );
		WRITE_BYTE( ENTINDEX(pEntity) );
		WRITE_STRING( text );
	MESSAGE_END();

	// echo to the server console, dedicated servers only: on a listen server that is the player's own console, which
	// already gets the client's timestamped copy (chat appeared twice). Colour markup is stripped for the console.
	if ( IS_DEDICATED_SERVER() )
	{
		char plain[sizeof(text)];
		const char *in = text;
		char *out = plain;
		while ( *in )
		{
			if ( *in == 7 ) { in++; for ( int h = 0; h < 6 && *in; h++ ) in++; continue; }
			if ( *in == 8 ) { in++; for ( int h = 0; h < 8 && *in; h++ ) in++; continue; }
			if ( *in >= 1 && *in <= 6 ) { in++; continue; }
			*out++ = *in++;
		}
		*out = 0;
		g_engfuncs.pfnServerPrint( plain );
	}

	char * temp;
	if ( teamonly )
		temp = "say_team";
	else
		temp = "say";
	
	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" %s \"%s\"\n", 
			STRING( pEntity->v.netname ), 
			GETPLAYERUSERID( pEntity ),
			GETPLAYERAUTHID( pEntity ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pEntity ), "model" ),
			temp,
			p );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" %s \"%s\"\n", 
			STRING( pEntity->v.netname ), 
			GETPLAYERUSERID( pEntity ),
			GETPLAYERAUTHID( pEntity ),
			GETPLAYERUSERID( pEntity ),
			temp,
			p );
	}
}


/*
===========
ClientCommand
called each time a player uses a "cmd" command
============
*/

// Use CMD_ARGV,  CMD_ARGV, and CMD_ARGC to get pointers the character string command.
void ClientCommand( edict_t *pEntity )
{
	const char *pcmd = CMD_ARGV(0);
	const char *pstr;

	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	SBO_TEST_CALL( if ( Test_ClientCommand( pEntity, pcmd ) ) return; )	// test builds: test_* with sv_cheats 1

	entvars_t *pev = &pEntity->v;

	// SBO 2.0 AFK: a command (chat, a menu, a bind) is the player's doing. Not the two the client's voice code
	// sends by itself (voice_status.cpp UpdateServerState), nor the one the engine sends when a demo recording starts.
	if ( !FStrEq( pcmd, "vban" ) && !FStrEq( pcmd, "VModEnable" ) && !FStrEq( pcmd, "fullupdate" ) )
		GetClassPtr((CBasePlayer *)pev)->AfkActivity( 0, pcmd );

	// SBO 2.0 spectator mode: no join, go, radio or GM menus while spectating
	if ( GetClassPtr((CBasePlayer *)pev)->m_bSpectating && Spectate_BlocksCommand( pcmd ) )
	{
		if ( FStrEq( pcmd, "gmon" ) )
			GmMode_Notice( GetClassPtr((CBasePlayer *)pev), "Spectators can't be Game Master: say /spectate to join the game first." );
		return;
	}

	if ( FStrEq(pcmd, "say" ) )
	{
		Host_Say( pEntity, 0 );
	}
	else if ( FStrEq(pcmd, "fullupdate" ) )
	{
		// HL SDK: the engine sends this when a demo recording starts, so the demo gets the client state that was
		// sent before it (SBO 1.38 had no such branch: demos had no HUD). ReGameDLL_CS's limit: not every frame.
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);
		if ( gpGlobals->time >= pPlayer->m_flNextFullUpdate )
		{
			pPlayer->m_flNextFullUpdate = gpGlobals->time + 0.6f;
			pPlayer->ForceClientDllUpdate();
		}
	}
	else if ( FStrEq(pcmd, "spectate" ) )	// SBO 2.0 spectator mode on/off (also the team menu's hidden key 6)
	{
		Spectate_Toggle( GetClassPtr((CBasePlayer *)pev) );
	}
	else if ( FStrEq(pcmd, "vgui_screen_command" ) )	// SBO 2.0: a button on a world screen (vgui_screen.cpp)
	{
		VGuiScreen_ClientCommand( GetClassPtr((CBasePlayer *)pev) );
	}
	else if ( FStrEq(pcmd, "say_team" ) )
	{
		// chat-vgui2: say_team (unhandled in 1.38: "Unknown command") now behaves exactly like say, so old binds
		// work; Host_Say's teamonly only selects the "say_team" log keyword
		Host_Say( pEntity, 1 );
	}
	else if ( MsgBox_BlocksWeaponCommand( GetClassPtr((CBasePlayer *)pev), pcmd ) )
	{
		// game_messagebox: no weapon change while a box is open
	}
	else if ( FStrEq(pcmd, "use" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->SelectItem((char *)CMD_ARGV(1));
	}
	else if (((pstr = strstr(pcmd, "weapon_")) != NULL)  && (pstr == pcmd))
	{
		GetClassPtr((CBasePlayer *)pev)->SelectItem(pcmd);
	}
	else if (FStrEq(pcmd, "lastinv" ))
	{
		GetClassPtr((CBasePlayer *)pev)->SelectLastItem();
	}
	else if ( FStrEq(pcmd, "cloak" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->Cloak();
	}
	else if ( FStrEq(pcmd, "cybereye" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->Cybereye();
	}
	else if ( FStrEq(pcmd, "specmode" ) )	// new spectator mode (halflife SDK 2.3; the client's spec_mode sends it)
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer->IsObserver() )
		{
			int iMode = atoi( CMD_ARGV(1) );
			BOOL bWasRoaming = ( pPlayer->pev->iuser1 == OBS_ROAMING );

			pPlayer->Observer_SetMode( iMode );

			// a mode that needs a target, asked for with nobody to watch: the roaming player stays roaming, say why
			// (Observer_SetMode prints nothing when nothing changes, so that Jump stays silent)
			if ( bWasRoaming && iMode != OBS_ROAMING && pPlayer->pev->iuser1 == OBS_ROAMING )
				ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "#Spec_NoTarget" );
		}
	}
	else if ( FStrEq(pcmd, "follownext" ) )	// follow next player (halflife SDK 2.3; the spectator panel's < and >)
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer->IsObserver() )
			pPlayer->Observer_FindNextPlayer( atoi( CMD_ARGV(1) ) ? true : false );
	}
	else if ( MsgBox_ClientCommand( GetClassPtr((CBasePlayer *)pev), pcmd ) )
	{
		// game_messagebox: "close_messagebox <id>", the player closed his newest box (primary attack)
	}
	else if ( FStrEq(pcmd, "ShowVGUI" ) )
	{
		if ( CMD_ARGC() >= 1 )
		{
			if ( g_pGameRules->GetPartie() != 2 || atoi( CMD_ARGV(1) ) != 10 )
				ShowVGUI( GetClassPtr((CBasePlayer *)pev), atoi( CMD_ARGV(1) ) );

			LoadoutMenuActivity( GetClassPtr((CBasePlayer *)pev) );	// SBO 2.0 mp_loadouttime: MOTD -> briefing -> loadout
		}
	}
	else if ( g_pGameRules->ClientCommand( GetClassPtr((CBasePlayer *)pev), pcmd ) )
	{
		// MenuSelect returns true only if the command is properly handled,  so don't print a warning

		// SBO 2.0 spectator mode: the selection menu's "Go !" during intermission, when players don't think
		// (CBasePlayer::PreThink): nothing to join, the player plays on the next map
		if ( g_fGameOver && GetClassPtr((CBasePlayer *)pev)->m_bSpectateMenuExit )
			Spectate_MenuConfirmed( GetClassPtr((CBasePlayer *)pev) );

		// the loadout menu's "Go !" from a dead or waiting player during intermission: joins at once, as SBO did
		if ( g_fGameOver && GetClassPtr((CBasePlayer *)pev)->m_bLoadoutJoinPending )
			GetClassPtr((CBasePlayer *)pev)->LoadoutJoin();
	}
	else
	{
		// tell the user they entered an unknown command
		char command[128];

		// check the length of the command (prevents crash)
		// max total length is 192 ...and we're adding a string below ("Unknown command: %s\n")
		strncpy( command, pcmd, 127 );
		command[127] = '\0';

		// HL25: First parse the name and remove any %'s
		for ( char *pApersand = command; pApersand != NULL && *pApersand != 0; pApersand++ )
		{
			// Replace it with a space
			if ( *pApersand == '%' )
				*pApersand = ' ';
		}


		// tell the user they entered an unknown command
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs( "Unknown command: %s\n", command ) );
	}
}


/*
========================
ClientUserInfoChanged

called after the player changes
userinfo - gives dll a chance to modify it before
it gets sent into the rest of the engine.
========================
*/
void ClientUserInfoChanged( edict_t *pEntity, char *infobuffer )
{
	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	if ( pEntity->v.netname && STRING(pEntity->v.netname)[0] != 0 && !FStrEq( STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" )) )
	{
		char sName[256];
		char *pName = g_engfuncs.pfnInfoKeyValue( infobuffer, "name" );
		strncpy( sName, pName, sizeof(sName) - 1 );
		sName[ sizeof(sName) - 1 ] = '\0';

		// First parse the name and remove any %'s
		for ( char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++ )
		{
			// Replace it with a space
			if ( *pApersand == '%' )
				*pApersand = ' ';
		}

		// Set the name
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX(pEntity), infobuffer, "name", sName );

		char text[256];
		sprintf( text, "%s changed name to %s\n", UTIL_ChatPlayerName( STRING(pEntity->v.netname), pEntity ), UTIL_ChatPlayerName( g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ), pEntity ) );
		MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n", 
				STRING( pEntity->v.netname ), 
				GETPLAYERUSERID( pEntity ), 
				GETPLAYERAUTHID( pEntity ),
				g_engfuncs.pfnInfoKeyValue( infobuffer, "model" ), 
				g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" changed name to \"%s\"\n", 
				STRING( pEntity->v.netname ), 
				GETPLAYERUSERID( pEntity ), 
				GETPLAYERAUTHID( pEntity ),
				GETPLAYERUSERID( pEntity ), 
				g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
	}

	// halflife-updated: client preferences (cl_autowepswitch); HLU does this in CHalfLifeMultiplay::ClientUserInfoChanged,
	// here it covers every SBO game rules class (CHalfLifeTeamplay's override returns early)
	GetClassPtr((CBasePlayer *)&pEntity->v)->SetPrefsFromUserinfo( infobuffer );

	g_pGameRules->ClientUserInfoChanged( GetClassPtr((CBasePlayer *)&pEntity->v), infobuffer );
}

static int g_serveractive = 0;

void ServerDeactivate( void )
{
	// It's possible that the engine will call this function more times than is necessary
	//  Therefore, only run it one time for each call to ServerActivate 
	if ( g_serveractive != 1 )
	{
		return;
	}

	g_serveractive = 0;

	// Peform any shutdown operations here...
	Tutor_ServerDeactivate();	// CS:CZ tutor window (env_tutor)
	MsgBox_ServerDeactivate();	// game_messagebox: the players' open boxes
	WorldText_Reset();		// SBO 2.0: point_worldtext: the map's texts (point_worldtext.cpp)
	KeyValue_ClearCustom();	// the map's custom keyvalues (keyvalue.cpp)
	MapCompat_MapUnloaded();	// SBO 2.0: the next map's keys go through maps/map_compat.txt again
	RenderFade_Reset();		// SBO 2.0: env_render fades of the map
	EnvStatic_Reset();		// SBO 2.0: env_static props of the map
	memset( g_bAlwaysSend, 0, sizeof( g_bAlwaysSend ) );	// SBO 2.0: env_model "Always send" of the map
	Semiclip_ServerDeactivate();	// SBO 2.0 semiclip
	KillFeedIcons_Reset();	// SBO 2.0: plugins' killfeed icons (killfeed_icons.cpp): set again on the next map
	SBO_TEST_CALL( Test_ServerDeactivate() );
	//
}

void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	int				i;
	CBaseEntity		*pClass;

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	g_serveractive = 1;

	MapCompat_MapLoaded();	// SBO 2.0: the map's entities are loaded; keys set from now on are never filtered

	// SBO 2.0: env_particle measures its offset from its target before the target's Activate moves it (func_train)
	EnvParticle_ServerActivate();

	// Clients have not been initialized yet
	for ( i = 0; i < edictCount; i++ )
	{
		if ( pEdictList[i].free )
			continue;
		
		// Clients aren't necessarily initialized until ClientPutInServer()
		// halflife-updated (halflife #3282): client edicts are 1..clientMax, not 0..clientMax-1
		if ( ( i > 0 && i <= clientMax ) || !pEdictList[i].pvPrivateData )
			continue;

		pClass = CBaseEntity::Instance( &pEdictList[i] );
		// Activate this entity if it's got a class & isn't dormant
		if ( pClass && !(pClass->pev->flags & FL_DORMANT) )
		{
			pClass->Activate();
		}
		else
		{
			ALERT( at_console, "Can't instance %s\n", STRING(pEdictList[i].v.classname) );
		}
	}

	// Link user messages here to make sure first client can get them...
	LinkUserMessages();

	Semiclip_ServerActivate();	// SBO 2.0 semiclip: mp_semiclip_time runs from the map start
	SBO_TEST_CALL( Test_ServerActivate() );
}


/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void PlayerPreThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PreThink( );
}

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void PlayerPostThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PostThink( );
}



void ParmsNewLevel( void )
{
}


void ParmsChangeLevel( void )
{
	// retrieve the pointer to the save data
	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;

	if ( pSaveData )
		pSaveData->connectionCount = BuildChangeList( pSaveData->levelList, MAX_LEVEL_CONNECTIONS );
}


//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
void StartFrame( void )
{
	// SBO: a saved game was loaded (a one-player session, "maxplayers 1": load, quick load, or the engine's "reload"
	// finding a save). It holds none of SBO's state, so start the map again, the way the map cycle changes maps.
	if ( g_fSaveRestored )
	{
		static char st_szMap[ 64 ];

		g_fSaveRestored = FALSE;
		strncpy( st_szMap, STRING( gpGlobals->mapname ), sizeof( st_szMap ) - 1 );
		st_szMap[ sizeof( st_szMap ) - 1 ] = 0;
		ALERT( at_console, "Saved games are not supported: restarting %s\n", st_szMap );
		CHANGE_LEVEL( st_szMap, NULL );
		return;
	}

	SBO_TEST_CALL( Test_StartFrame() );	// test builds: step files, test players, trace (sbo_test.cpp)

	if ( g_pGameRules )
		g_pGameRules->Think();

	RoundTime_Frame();	// SBO 2.0: spectator timer basis changed (mp_timelimit, game over)
	Semiclip_StartFrame();	// SBO 2.0 semiclip: its cvars changed
	RenderFade_Frame();	// SBO 2.0: env_render scale fades: the box follows the drawn scale
	GmMode_StartFrame();	// SBO 2.0 GM mode: weapon drops and teleporters on moving platforms

	if ( g_fGameOver )
		return;

	SboScoreboard_Frame();	// chat-vgui2: scoreboard categories changed outside the hooked paths
	SboModeInfo_Frame();	// SBO 2.0: scoreboard round / frag limit line changed (round start, cvars)

	Tutor_StartFrame();	// CS:CZ tutor window: the players' env_tutor queues
	MsgBox_StartFrame();	// game_messagebox: autoclose distance

	gpGlobals->teamplay = teamplay.value;
	g_ulFrameCount++;

	// halflife-updated (halflife #11): push sv_allowbunnyhopping changes to every player's physinfo ("bj", pm_shared)
	static bool s_bLastAllowBunnyHopping = false;
	const bool bAllowBunnyHopping = sv_allowbunnyhopping.value != 0;

	if ( bAllowBunnyHopping != s_bLastAllowBunnyHopping )
	{
		s_bLastAllowBunnyHopping = bAllowBunnyHopping;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

			if ( !pPlayer )
				continue;

			g_engfuncs.pfnSetPhysicsKeyValue( pPlayer->edict(), "bj", UTIL_dtos1( bAllowBunnyHopping ? 1 : 0 ) );
		}
	}

	// SBO 2.0: the same for sv_infinite_ammo ("ia", read by the client's weapon prediction in hl_weapons.cpp)
	static bool s_bLastInfiniteAmmo = false;
	const bool bInfiniteAmmo = sv_infinite_ammo.value != 0;

	if ( bInfiniteAmmo != s_bLastInfiniteAmmo )
	{
		s_bLastInfiniteAmmo = bInfiniteAmmo;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

			if ( !pPlayer )
				continue;

			g_engfuncs.pfnSetPhysicsKeyValue( pPlayer->edict(), "ia", UTIL_dtos1( bInfiniteAmmo ? 1 : 0 ) );
		}
	}
}


void ClientPrecache( void )
{
	// setup precaches always needed
	PRECACHE_SOUND("player/sprayer.wav");			// spray paint sound for PreAlpha

	// SBO
	PRECACHE_SOUND("weapons/ric7.wav");
	PRECACHE_SOUND("weapons/cloak.wav");
	PRECACHE_SOUND("monster/minoswalk.wav");
	PRECACHE_SOUND("monster/minosfoot.wav");
	PRECACHE_SOUND("radio/cover.wav");
	PRECACHE_SOUND("radio/follow.wav");
	PRECACHE_SOUND("radio/shoot.wav");
	PRECACHE_SOUND("radio/medic.wav");
	PRECACHE_SOUND("radio/help.wav");
	PRECACHE_SOUND("radio/wait.wav");
	PRECACHE_SOUND("radio/side.wav");
	PRECACHE_SOUND("radio/grenade.wav");
	PRECACHE_SOUND("radio/retreat.wav");
	PRECACHE_SOUND("radio/coverf.wav");
	PRECACHE_SOUND("radio/followf.wav");
	PRECACHE_SOUND("radio/shootf.wav");
	PRECACHE_SOUND("radio/medicf.wav");
	PRECACHE_SOUND("radio/helpf.wav");
	PRECACHE_SOUND("radio/waitf.wav");
	PRECACHE_SOUND("radio/sidef.wav");
	PRECACHE_SOUND("radio/grenadef.wav");
	PRECACHE_SOUND("radio/retreatf.wav");
	PRECACHE_MODEL("sprites/tele1.spr");

	// PRECACHE_SOUND("player/pl_jumpland2.wav");		// UNDONE: play 2x step sound
	
	PRECACHE_SOUND("player/pl_fallpain3.wav");		
	
	// SBO 2.0: the step, ladder, wade and texture hit sounds (HL's pl_step, pl_metal, pl_dirt, pl_duct, pl_grate,
	// pl_slosh, pl_tile, pl_ladder, pl_wade, debris/wood, debris/glass, buttons/spark5-6) are precached per map by
	// the material registry: TEXTURETYPE_PrecacheLevel (sound.cpp), from CWorld::Precache

	PRECACHE_SOUND("common/npc_step1.wav");		// NPC walk on concrete
	PRECACHE_SOUND("common/npc_step2.wav");
	PRECACHE_SOUND("common/npc_step3.wav");
	PRECACHE_SOUND("common/npc_step4.wav");

	PRECACHE_SOUND("player/pl_swim1.wav");		// breathe bubbles
	PRECACHE_SOUND("player/pl_swim2.wav");
	PRECACHE_SOUND("player/pl_swim3.wav");
	PRECACHE_SOUND("player/pl_swim4.wav");

	PRECACHE_SOUND("plats/train_use1.wav");		// use a train

	PRECACHE_SOUND( SOUND_FLASHLIGHT_ON );
	PRECACHE_SOUND( SOUND_FLASHLIGHT_OFF );

// player gib sounds
	PRECACHE_SOUND("common/bodysplat.wav");		               

// player pain sounds
	PRECACHE_SOUND("player/pl_pain5.wav");
	PRECACHE_SOUND("player/pl_pain6.wav");
	PRECACHE_SOUND("player/pl_pain7.wav");

	PRECACHE_MODEL("models/player.mdl");

	// hud sounds

	PRECACHE_SOUND("common/wpn_hudoff.wav");
	PRECACHE_SOUND("common/wpn_hudon.wav");
	PRECACHE_SOUND("common/wpn_moveselect.wav");
	PRECACHE_SOUND("common/wpn_select.wav");
	PRECACHE_SOUND("common/wpn_denyselect.wav");


	// geiger sounds

	PRECACHE_SOUND("player/geiger6.wav");
	PRECACHE_SOUND("player/geiger5.wav");
	PRECACHE_SOUND("player/geiger4.wav");
	PRECACHE_SOUND("player/geiger3.wav");
	PRECACHE_SOUND("player/geiger2.wav");
	PRECACHE_SOUND("player/geiger1.wav");

	// SBO: morph monsters
	UTIL_PrecacheOther("monster_imp");
	UTIL_PrecacheOther("monster_imp_hunter");
	UTIL_PrecacheOther("monster_alien_slave");
	UTIL_PrecacheOther("monster_hunter");
	UTIL_PrecacheOther("monster_dark");
	UTIL_PrecacheOther("dark_grunt");
	UTIL_PrecacheOther("monster_human_grunt");
	UTIL_PrecacheOther("monster_human_assassin");
	UTIL_PrecacheOther("monster_barney");
	UTIL_PrecacheOther("monster_cyberminotor");
	UTIL_PrecacheOther("monster_apache");
	UTIL_PrecacheOther("monster_gargantua");
	UTIL_PrecacheOther("monster_zombie_hunter");
	UTIL_PrecacheOther("monster_zombie_grunt");
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Syndicate-Black-Ops";
}

/*
================
Sys_Error

Engine is going to shut down, allows setting a breakpoint in game .dll to catch that occasion
================
*/
void Sys_Error( const char *error_string )
{
	// Default case, do nothing.  MOD AUTHORS:  Add code ( e.g., _asm { int 3 }; here to cause a breakpoint for debugging your game .dlls
}

/*
================
PlayerCustomization

A new player customization has been registered on the server
UNDONE:  This only sets the # of frames of the spray can logo
animation right now.
================
*/
void PlayerCustomization( edict_t *pEntity, customization_t *pCust )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (!pPlayer)
	{
		ALERT(at_console, "PlayerCustomization:  Couldn't get player!\n");
		return;
	}

	if (!pCust)
	{
		ALERT(at_console, "PlayerCustomization:  NULL customization!\n");
		return;
	}

	switch (pCust->resource.type)
	{
	case t_decal:
		pPlayer->SetCustomDecalFrames(pCust->nUserData2); // Second int is max # of frames.
		break;
	case t_sound:
	case t_skin:
	case t_model:
		// Ignore for now.
		break;
	default:
		ALERT(at_console, "PlayerCustomization:  Unknown customization type!\n");
		break;
	}
}

/*
================
SpectatorConnect

A spectator has joined the game
================
*/
void SpectatorConnect( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorConnect( );
}

/*
================
SpectatorConnect

A spectator has left the game
================
*/
void SpectatorDisconnect( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorDisconnect( );
}

/*
================
SpectatorConnect

A spectator has sent a usercmd
================
*/
void SpectatorThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorThink( );
}

////////////////////////////////////////////////////////
// PAS and PVS routines for client messaging
//

/*
================
SetupVisibility

A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
entity's origin is used.  Either is offset by the view_ofs to get the eye position.

From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
 override the actual PAS or PVS values, or use a different origin.

NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
================
*/
void SetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char **pvs, unsigned char **pas )
{
	Vector org;
	edict_t *pView = pClient;

	// Find the client's PVS
	if ( pViewEntity )
	{
		pView = pViewEntity;
	}

	// Tracking Spectators use the visibility of their target
	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );
	if ( pPlayer && (pPlayer->pev->iuser2 != 0) && (pPlayer->m_hObserverTarget != NULL) )
	{
		pView = pPlayer->m_hObserverTarget->edict();

		// SBO 2.0, from ReGameDLL_CS (client.cpp SetupVisibility): an observer with a target is moved to it every
		// frame. PM_SpectatorMove copies the target's origin only while the target is in the observer's move list,
		// so after the target teleported or respawned far away the observer's own origin stayed behind, and what
		// the engine plays by the client's origin (linear ambient_generic / ambient_music volume, every PAS sound)
		// was wrong (playtest 2026-10-17: no lounge music while spectating on co_sbo_showcase)
		if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
			UTIL_SetOrigin( pPlayer->pev, pPlayer->m_hObserverTarget->pev->origin );
	}

	if ( pClient->v.flags & FL_PROXY )
	{
		*pvs = NULL;	// the spectator proxy sees
		*pas = NULL;	// and hears everything
		return;
	}

	org = pView->v.origin + pView->v.view_ofs;
	if ( pView->v.flags & FL_DUCKING )
	{
		org = org + ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
	}

	*pvs = ENGINE_SET_PVS ( (float *)&org );
	*pas = ENGINE_SET_PAS ( (float *)&org );
}

#include "entity_state.h"

// SBO 2.0: model scale (docs/MODEL_SCALE.md): a studio model drawn bigger than its size (scale above 1). The engine
// links an entity to the PVS leaves its own box (pev->absmin / absmax) touches. UTIL_SetSize scales that box, but a
// model reaches out of its box (and some entities never set one: a func_rotating's .mdl, env_sprite), so such a
// model can be in view while those leaves are not; it is sent to every client instead (players are never)
static bool ScaledUpStudioModel( const edict_t *ent, int player )
{
	if ( player || ent->v.scale <= 1.0f || !ent->v.model )
		return false;

	const char *pszModel = STRING( ent->v.model );
	size_t len = strlen( pszModel );

	return len > 4 && !stricmp( pszModel + len - 4, ".mdl" );
}

/*
AddToFullPack

Return 1 if the entity state has been filled in for the ent and the entity will be propagated to the client, 0 otherwise

state is the server maintained copy of the state info that is transmitted to the client
a MOD could alter values copied into state to send the "host" a different look for a particular entity update, etc.
e and ent are the entity that is being added to the update, if 1 is returned
host is the player's edict of the player whom we are sending the update to
player is 1 if the ent/e is a player and 0 otherwise
pSet is either the PAS or PVS that we previous set up.  We can use it to ask the engine to filter the entity against the PAS or PVS.
we could also use the pas/ pvs that we set in SetupVisibility, if we wanted to.  Caching the value is valid in that case, but still only for the current frame
*/
int AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet )
{
	int					i;

	// halflife-updated #191: Entities with an index greater than this will corrupt the client's heap because
	// the index is sent with only 11 bits of precision (2^11 == 2048).
	// So we don't send them, just like having too many entities would result
	// in the entity not being sent.
	if ( e >= MAX_EDICTS )
	{
		return 0;
	}


	// don't send if flagged for NODRAW and it's not the host getting the message
	if ( ( ent->v.effects == EF_NODRAW ) &&
		 ( ent != host ) )
		return 0;

	// Ignore ents without valid / visible models
	if ( !ent->v.modelindex || !STRING( ent->v.model ) )
		return 0;

	// SBO 2.0 GM mode: a Game Master's placement preview goes to that Game Master only
	if ( !GmMode_ShouldSend( e, host ) )
		return 0;

	// Don't send spectators to other players
	if ( ( ent->v.flags & FL_SPECTATOR ) && ( ent != host ) )
	{
		return 0;
	}

	// Ignore if not the host and not touching a PVS/PAS leaf
	// If pSet is NULL, then the test will always succeed and the entity will be added to the update
	if ( ent != host )
	{
		// SBO 2.0: env_model "Always send" (env_model.cpp g_bAlwaysSend; Featureful's EFLAG_ALWAYS_SEND)
		if ( !ENGINE_CHECK_VISIBILITY( (const struct edict_s *)ent, pSet ) && !ScaledUpStudioModel( ent, player )
			&& !g_bAlwaysSend[ e ] && !GmMode_PipSend( ent, host ) )
		{
			return 0;
		}
	}


	// Don't send entity to local client if the client says it's predicting the entity itself.
	if ( ent->v.flags & FL_SKIPLOCALHOST )
	{
		if ( ( hostflags & 1 ) && ( ent->v.owner == host ) )
			return 0;
	}
	
	if ( host->v.groupinfo )
	{
		UTIL_SetGroupTrace( host->v.groupinfo, GROUP_OP_AND );

		// Should always be set, of course
		if ( ent->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND )
			{
				if ( !(ent->v.groupinfo & host->v.groupinfo ) )
					return 0;
			}
			else if ( g_groupop == GROUP_OP_NAND )
			{
				if ( ent->v.groupinfo & host->v.groupinfo )
					return 0;
			}
		}

		UTIL_UnsetGroupTrace();
	}

	memset( state, 0, sizeof( *state ) );

	// Assign index so we can track this entity from frame to frame and
	//  delta from it.
	state->number	  = e;
	state->entityType = ENTITY_NORMAL;
	
	// Flag custom entities.
	if ( ent->v.flags & FL_CUSTOMENTITY )
	{
		state->entityType = ENTITY_BEAM;
	}

	// 
	// Copy state data
	//

	// Round animtime to nearest millisecond
	state->animtime   = (int)(1000.0 * ent->v.animtime ) / 1000.0;

	memcpy( state->origin, ent->v.origin, 3 * sizeof( float ) );
	memcpy( state->angles, ent->v.angles, 3 * sizeof( float ) );
	memcpy( state->mins, ent->v.mins, 3 * sizeof( float ) );
	memcpy( state->maxs, ent->v.maxs, 3 * sizeof( float ) );

	memcpy( state->startpos, ent->v.startpos, 3 * sizeof( float ) );
	memcpy( state->endpos, ent->v.endpos, 3 * sizeof( float ) );

	state->impacttime = ent->v.impacttime;
	state->starttime = ent->v.starttime;

	state->modelindex = ent->v.modelindex;
		
	state->frame      = ent->v.frame;

	state->skin       = ent->v.skin;
	state->effects    = ent->v.effects;

	// This non-player entity is being moved by the game .dll and not the physics simulation system
	//  make sure that we interpolate it's position on the client if it moves
	if ( !player &&
		 ent->v.animtime &&
		 ent->v.velocity[ 0 ] == 0 && 
		 ent->v.velocity[ 1 ] == 0 && 
		 ent->v.velocity[ 2 ] == 0 )
	{
		state->eflags |= EFLAG_SLERP;
	}

	state->scale	  = ent->v.scale;
	state->solid	  = ent->v.solid;
	state->colormap   = ent->v.colormap;

	state->movetype   = ent->v.movetype;
	state->sequence   = ent->v.sequence;
	state->framerate  = ent->v.framerate;
	state->body       = ent->v.body;

	for (i = 0; i < 4; i++)
	{
		state->controller[i] = ent->v.controller[i];
	}

	for (i = 0; i < 2; i++)
	{
		state->blending[i]   = ent->v.blending[i];
	}

	state->rendermode    = ent->v.rendermode;
	state->renderamt     = ent->v.renderamt; 
	state->renderfx      = ent->v.renderfx;
	state->rendercolor.r = ent->v.rendercolor.x;
	state->rendercolor.g = ent->v.rendercolor.y;
	state->rendercolor.b = ent->v.rendercolor.z;

	state->aiment = 0;
	if ( ent->v.aiment )
	{
		state->aiment = ENTINDEX( ent->v.aiment );
	}

	state->owner = 0;
	if ( ent->v.owner )
	{
		int owner = ENTINDEX( ent->v.owner );
		
		// Only care if owned by a player
		if ( owner >= 1 && owner <= gpGlobals->maxClients )
		{
			state->owner = owner;	
		}
	}

	// HACK:  Somewhat...
	// Class is overridden for non-players to signify a breakable glass object ( sort of a class? )
	if ( !player )
	{
		state->playerclass  = ent->v.playerclass;
	}

	// Special stuff for players only
	if ( player )
	{
		memcpy( state->basevelocity, ent->v.basevelocity, 3 * sizeof( float ) );

		state->weaponmodel  = MODEL_INDEX( STRING( ent->v.weaponmodel ) );
		state->gaitsequence = ent->v.gaitsequence;
		state->spectator = ent->v.flags & FL_SPECTATOR;
		state->friction     = ent->v.friction;

		state->gravity      = ent->v.gravity;
//		state->team			= ent->v.team;
//		
		state->usehull      = ( ent->v.flags & FL_DUCKING ) ? 1 : 0;
		state->health		= ent->v.health;
	}

	// HL25: tell the client which entities sound like flesh when hit (client bullet impact sounds,
	// EV_HLDM_PlayTextureSound). Needs eflags at 2 bits in delta.lst.
	CBaseEntity	*pEntity = static_cast<CBaseEntity*>( GET_PRIVATE( ent ) );
	if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		state->eflags |= EFLAG_FLESH_SOUND;
	else
		state->eflags &= ~EFLAG_FLESH_SOUND;

	// SBO 2.0 semiclip: the players host passes through are SOLID_NOT and faded in host's packet (semiclip.cpp)
	Semiclip_AddToFullPack( state, e, ent, host, player );
	GmMode_AddToFullPack( state, e, host );	// SBO 2.0 GM mode: the selected NPCs' glow, for their Game Master

	return 1;
}

// defaults for clientinfo messages
#define	DEFAULT_VIEWHEIGHT	28

/*
===================
CreateBaseline

Creates baselines used for network encoding, especially for player data since players are not spawned until connect time.
===================
*/
void CreateBaseline( int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, float *player_mins, float *player_maxs )
{
	baseline->origin		= entity->v.origin;
	baseline->angles		= entity->v.angles;
	baseline->frame			= entity->v.frame;
	baseline->skin			= (short)entity->v.skin;

	// render information
	baseline->rendermode	= (byte)entity->v.rendermode;
	baseline->renderamt		= (byte)entity->v.renderamt;
	baseline->rendercolor.r	= (byte)entity->v.rendercolor.x;
	baseline->rendercolor.g	= (byte)entity->v.rendercolor.y;
	baseline->rendercolor.b	= (byte)entity->v.rendercolor.z;
	baseline->renderfx		= (byte)entity->v.renderfx;

	if ( player )
	{
		baseline->mins			= Vector( player_mins );	// halflife-updated (VS#3078): the engine passes pointers
		baseline->maxs			= Vector( player_maxs );

		baseline->colormap		= eindex;
		baseline->modelindex	= playermodelindex;
		baseline->friction		= 1.0;
		baseline->movetype		= MOVETYPE_WALK;

		baseline->scale			= entity->v.scale;
		baseline->solid			= SOLID_SLIDEBOX;
		baseline->framerate		= 1.0;
		baseline->gravity		= 1.0;

	}
	else
	{
		baseline->mins			= entity->v.mins;
		baseline->maxs			= entity->v.maxs;

		baseline->colormap		= 0;
		baseline->modelindex	= entity->v.modelindex;//SV_ModelIndex(pr_strings + entity->v.model);
		baseline->movetype		= entity->v.movetype;

		baseline->scale			= entity->v.scale;
		baseline->solid			= entity->v.solid;
		baseline->framerate		= entity->v.framerate;
		baseline->gravity		= entity->v.gravity;
	}
}

typedef struct
{
	char name[32];
	int	 field;
} entity_field_alias_t;

#define FIELD_ORIGIN0			0
#define FIELD_ORIGIN1			1
#define FIELD_ORIGIN2			2
#define FIELD_ANGLES0			3
#define FIELD_ANGLES1			4
#define FIELD_ANGLES2			5

static entity_field_alias_t entity_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
	{ "angles[0]",			0 },
	{ "angles[1]",			0 },
	{ "angles[2]",			0 },
};

void Entity_FieldInit( struct delta_s *pFields )
{
	entity_field_alias[ FIELD_ORIGIN0 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN0 ].name );
	entity_field_alias[ FIELD_ORIGIN1 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN1 ].name );
	entity_field_alias[ FIELD_ORIGIN2 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN2 ].name );
	entity_field_alias[ FIELD_ANGLES0 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES0 ].name );
	entity_field_alias[ FIELD_ANGLES1 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES1 ].name );
	entity_field_alias[ FIELD_ANGLES2 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES2 ].name );
}

/*
==================
Entity_Encode

Callback for sending entity_state_t info over network. 
FIXME:  Move to script
==================
*/
void Entity_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if ( !initialized )
	{
		Entity_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer =  ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
	if ( localplayer )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}

	if ( ( t->impacttime != 0 ) && ( t->starttime != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );

		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES2 ].field );
	}

	if ( ( t->movetype == MOVETYPE_FOLLOW ) &&
		 ( t->aiment != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
	else if ( t->aiment != f->aiment )
	{
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
}

static entity_field_alias_t player_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
};

void Player_FieldInit( struct delta_s *pFields )
{
	player_field_alias[ FIELD_ORIGIN0 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN0 ].name );
	player_field_alias[ FIELD_ORIGIN1 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN1 ].name );
	player_field_alias[ FIELD_ORIGIN2 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN2 ].name );
}

/*
==================
Player_Encode

Callback for sending entity_state_t for players info over network. 
==================
*/
void Player_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if ( !initialized )
	{
		Player_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer =  ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
	if ( localplayer )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}

	if ( ( t->movetype == MOVETYPE_FOLLOW ) &&
		 ( t->aiment != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
	else if ( t->aiment != f->aiment )
	{
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
}

#define CUSTOMFIELD_ORIGIN0			0
#define CUSTOMFIELD_ORIGIN1			1
#define CUSTOMFIELD_ORIGIN2			2
#define CUSTOMFIELD_ANGLES0			3
#define CUSTOMFIELD_ANGLES1			4
#define CUSTOMFIELD_ANGLES2			5
#define CUSTOMFIELD_SKIN			6
#define CUSTOMFIELD_SEQUENCE		7
#define CUSTOMFIELD_ANIMTIME		8

entity_field_alias_t custom_entity_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
	{ "angles[0]",			0 },
	{ "angles[1]",			0 },
	{ "angles[2]",			0 },
	{ "skin",				0 },
	{ "sequence",			0 },
	{ "animtime",			0 },
};

void Custom_Entity_FieldInit( struct delta_s *pFields )
{
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_SKIN ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_SKIN ].name );
	custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].field= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].field= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].name );
}

/*
==================
Custom_Encode

Callback for sending entity_state_t info ( for custom entities ) over network. 
FIXME:  Move to script
==================
*/
void Custom_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int beamType;
	static int initialized = 0;

	if ( !initialized )
	{
		Custom_Entity_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	beamType = t->rendermode & 0x0f;
		
	if ( beamType != BEAM_POINTS && beamType != BEAM_ENTPOINT )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].field );
	}

	if ( beamType != BEAM_POINTS )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].field );
	}

	if ( beamType != BEAM_ENTS && beamType != BEAM_ENTPOINT )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_SKIN ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].field );
	}

	// animtime is compared by rounding first
	// see if we really shouldn't actually send it
	if ( (int)f->animtime == (int)t->animtime )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].field );
	}
}

/*
=================
RegisterEncoders

Allows game .dll to override network encoding of certain types of entities and tweak values, etc.
=================
*/
void RegisterEncoders( void )
{
	DELTA_ADDENCODER( "Entity_Encode", Entity_Encode );
	DELTA_ADDENCODER( "Custom_Encode", Custom_Encode );
	DELTA_ADDENCODER( "Player_Encode", Player_Encode );
}

int GetWeaponData( struct edict_s *player, struct weapon_data_s *info )
{
#if defined( CLIENT_WEAPONS )
	int i;
	weapon_data_t *item;
	entvars_t *pev = &player->v;
	CBasePlayer *pl = ( CBasePlayer *) CBasePlayer::Instance( pev );
	CBasePlayerWeapon *gun;
	
	ItemInfo II;

	memset( info, 0, 32 * sizeof( weapon_data_t ) );

	if ( !pl )
		return 1;

	// go through all of the weapons and make a list of the ones to pack
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( pl->m_rgpPlayerItems[ i ] )
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem *pPlayerItem = pl->m_rgpPlayerItems[ i ];

			while ( pPlayerItem )
			{
				gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();
				if ( gun && gun->UseDecrement() )
				{
					// Get The ID.
					memset( &II, 0, sizeof( II ) );
					gun->GetItemInfo( &II );

					if ( II.iId >= 0 && II.iId < 32 )
					{
						item = &info[ II.iId ];
					 	
						item->m_iId						= II.iId;
						item->m_iClip					= gun->m_iClip;

						item->m_flTimeWeaponIdle		= max( gun->m_flTimeWeaponIdle, -0.001 );
						item->m_flNextPrimaryAttack		= max( gun->m_flNextPrimaryAttack, -0.001 );
						item->m_flNextSecondaryAttack	= max( gun->m_flNextSecondaryAttack, -0.001 );
						item->m_fInReload				= gun->m_fInReload;
						item->m_fInSpecialReload		= gun->m_fInSpecialReload;
						item->fuser1					= max( gun->pev->fuser1, -0.001 );
						item->fuser2					= gun->m_flStartThrow;
						item->fuser3					= gun->m_flReleaseThrow;
						item->iuser1					= gun->m_chargeReady;
						item->iuser2					= gun->m_fInAttack;
						item->iuser3					= gun->m_fireState;
						
											
//						item->m_flPumpTime				= max( gun->m_flPumpTime, -0.001 );
					}
				}
				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}
#else
	memset( info, 0, 32 * sizeof( weapon_data_t ) );
#endif
	return 1;
}

/*
=================
UpdateClientData

Data sent to current client only
engine sets cd to 0 before calling.
=================
*/
void UpdateClientData ( const struct edict_s *ent, int sendweapons, struct clientdata_s *cd )
{
	if ( !ent || !ent->pvPrivateData )
		return;
	entvars_t *		pev	= (entvars_t *)&ent->v;
	CBasePlayer *	pl	= ( CBasePlayer *) CBasePlayer::Instance( pev );
	entvars_t *		pevOrg = NULL;

	// SBO 2.0 impulse 212: a max speed the game wrote since PreThink is not sent, the fast one is (player.cpp)
	if ( pl )
		pl->FastMoveUpdate();

	// halflife SDK 2.3: if user is spectating different player in First person, override some vars, so the client
	// sees the target's view (origin, view height, punch, health, view model, fov) on any engine. Only a player
	// target: a plugin may set m_hObserverTarget to any entity (a camera info_target), and the code below reads it
	// as a CBasePlayer (m_pActiveItem); the observer then gets its own data, as in SBO 1.38
	if ( pl && pl->pev->iuser1 == OBS_IN_EYE )
	{
		if ( pl->m_hObserverTarget && pl->m_hObserverTarget->IsPlayer() )
		{
			pevOrg = pev;
			pev = pl->m_hObserverTarget->pev;
			pl = ( CBasePlayer *) CBasePlayer::Instance( pev );
		}
	}

	cd->flags			= pev->flags;
	cd->health			= pev->health;

	cd->viewmodel		= MODEL_INDEX( STRING( pev->viewmodel ) );

	cd->waterlevel		= pev->waterlevel;
	cd->watertype		= pev->watertype;
	cd->weapons			= pev->weapons;

	// Vectors
	cd->origin			= pev->origin;
	cd->velocity		= pev->velocity;
	cd->view_ofs		= pev->view_ofs;
	cd->punchangle		= pev->punchangle;

	cd->bInDuck			= pev->bInDuck;
	cd->flTimeStepSound = pev->flTimeStepSound;
	cd->flDuckTime		= pev->flDuckTime;
	cd->flSwimTime		= pev->flSwimTime;
	cd->waterjumptime	= pev->teleport_time;

	strcpy( cd->physinfo, ENGINE_GETPHYSINFO( ent ) );

	cd->maxspeed		= pev->maxspeed;
	// halflife-updated (halflife #3044 / #3045): m_iFOV, the FOV the server tracks and sends with SetFOV. Code that
	// sets only m_iFOV (1.38's leg injuries did) would otherwise leave a zoomed player the zoom with cl_lw 1.
	cd->fov				= pl ? pl->m_iFOV : pev->fov;
	cd->weaponanim		= pev->weaponanim;

	cd->pushmsec		= pev->pushmsec;

	// Spectator mode
	if ( pevOrg != NULL )
	{
		// don't use spec vars from chased player
		cd->iuser1			= pevOrg->iuser1;
		cd->iuser2			= pevOrg->iuser2;
	}
	else
	{
		cd->iuser1			= pev->iuser1;
		cd->iuser2			= pev->iuser2;
	}

	// SBO 2.0 seamless trigger_teleport: the turn for the client's prediction (PM_TeleportStaleCmd), the
	// player's own, also while watching someone in eye view
	cd->vuser1			= ( pevOrg != NULL ) ? pevOrg->vuser1 : pev->vuser1;

	// SBO 2.0 GM mode: the Game Master's roaming speed for the client's prediction (pm_shared)
	if ( pl )
		GmMode_UpdateClientData( pl, cd );

#if defined( CLIENT_WEAPONS )
	if ( sendweapons )
	{
		if ( pl )
		{
			cd->m_flNextAttack	= pl->m_flNextAttack;
			cd->fuser2			= pl->m_flNextAmmoBurn;
			cd->fuser3			= pl->m_flAmmoStartCharge;
			cd->ammo_rockets	= pl->ammo_rockets;
			

			if ( pl->m_pActiveItem )
			{
				CBasePlayerWeapon *gun;
				gun = (CBasePlayerWeapon *)pl->m_pActiveItem->GetWeaponPtr();
				if ( gun && gun->UseDecrement() )
				{
					ItemInfo II;
					memset( &II, 0, sizeof( II ) );
					gun->GetItemInfo( &II );

					cd->m_iId = II.iId;

					cd->vuser3.z	= gun->m_iSecondaryAmmoType;
					cd->vuser4.x	= gun->m_iPrimaryAmmoType;
					// ReGameDLL: a weapon without ammo has ammo type -1 (melee, medikit); no m_rgAmmo[-1] read, its count is 0
					// (the client's HUD_WeaponsPostThink sends the same)
					cd->vuser4.y	= ( gun->m_iPrimaryAmmoType >= 0 && gun->m_iPrimaryAmmoType < MAX_AMMO_SLOTS ) ? pl->m_rgAmmo[gun->m_iPrimaryAmmoType] : 0;
					cd->vuser4.z	= ( gun->m_iSecondaryAmmoType >= 0 && gun->m_iSecondaryAmmoType < MAX_AMMO_SLOTS ) ? pl->m_rgAmmo[gun->m_iSecondaryAmmoType] : 0;
					
					if ( pl->m_pActiveItem->m_iId == WEAPON_RPG )
					{
						cd->vuser2.y = ( ( CRpg * )pl->m_pActiveItem)->m_fSpotActive;
						cd->vuser2.z = ( ( CRpg * )pl->m_pActiveItem)->m_cActiveRockets;
					}
				}
			}
		}
	}
#endif
}

/*
=================
CmdStart

We're about to run this usercmd for the specified player.  We can set up groupinfo and masking here, etc.
This is the time to examine the usercmd for anything extra.  This call happens even if think does not.
=================
*/
void CmdStart( const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed )
{
	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = ( CBasePlayer *) CBasePlayer::Instance( pev );

	if( !pl )
		return;

	if ( pl->pev->groupinfo != 0 )
	{
		UTIL_SetGroupTrace( pl->pev->groupinfo, GROUP_OP_AND );
	}

	pl->random_seed = random_seed;

	pl->AfkCheckInput( cmd );	// SBO 2.0: mp_afk_time, mp_afk_spectate_time
	pl->TeleportCheckCmd( cmd );	// SBO 2.0 seamless trigger_teleport: a move sent before the turn
}

/*
=================
CmdEnd

Each cmdstart is exactly matched with a cmd end, clean up any group trace flags, etc. here
=================
*/
void CmdEnd ( const edict_t *player )
{
	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = ( CBasePlayer *) CBasePlayer::Instance( pev );

	if( !pl )
		return;
	if ( pl->pev->groupinfo != 0 )
	{
		UTIL_UnsetGroupTrace();
	}
}

/*
================================
ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int	ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size )
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

/*
================================
GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	// halflife-updated (halflife #1703): the old code assigned the local pointers instead of copying the vectors
	return PM_GetHullBounds( hullnumber, mins, maxs );
}

/*
================================
CreateInstancedBaselines

Create pseudo-baselines for items that aren't placed in the map at spawn time, but which are likely
to be created during play ( e.g., grenades, ammo packs, projectiles, corpses, etc. )
================================
*/
void CreateInstancedBaselines ( void )
{
	int iret = 0;
	entity_state_t state;

	memset( &state, 0, sizeof( state ) );

	// Create any additional baselines here for things like grendates, etc.
	// iret = ENGINE_INSTANCE_BASELINE( pc->pev->classname, &state );

	// Destroy objects.
	//UTIL_Remove( pc );
}

/*
================================
InconsistentFile

One of the ENGINE_FORCE_UNMODIFIED files failed the consistency check for the specified player
 Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
================================
*/
int	InconsistentFile( const edict_t *player, const char *filename, char *disconnect_message )
{
	// Server doesn't care?
	if ( CVAR_GET_FLOAT( "mp_consistency" ) != 1 )
		return 0;

	// Default behavior is to kick the player
	sprintf( disconnect_message, "Server is enforcing file consistency for %s\n", filename );

	// Kick now with specified disconnect message.
	return 1;
}

/*
================================
AllowLagCompensation

 The game .dll should return 1 if lag compensation should be allowed ( could also just set
  the sv_unlag cvar.
 Most games right now should return 0, until client-side weapon prediction code is written
  and tested for them ( note you can predict weapons, but not do lag compensation, too, 
  if you want.
================================
*/
int AllowLagCompensation( void )
{
	return 1;
}
