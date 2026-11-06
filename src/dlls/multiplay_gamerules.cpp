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
// teamplay_gamerules.cpp
//
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include	"teamplay_gamerules.h"	// SBO: the game-mode headers (as in gamerules.cpp); evidence: their 4 inline
#include	"coop.h"					// methods put Think's last_frags at .728 (local static names count function definitions)
#include	"br.h"
#include	"dm.h"
 
#include	"monsters.h"	// skill.h; HITGROUP_HEAD (kill feed headshots)
#include	"game.h"
#include	"items.h"
#include	"voice_gamemgr.h"
#include	"hltv.h"
#include	"pm_shared.h"	// OBS_* spectator modes
#include	"gmmode.h"	// SBO 2.0: a Game Master's monster in the kill feed

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgScoreInfo;
extern int gmsgMOTD;
extern int gmsgServerName;
extern int gmsgSpectator;
extern int gmsgPlayerAfk;

extern int g_teamplay;

#define ITEM_RESPAWN_TIME	30
#define WEAPON_RESPAWN_TIME	20
#define AMMO_RESPAWN_TIME	20

float g_flIntermissionStartTime = 0;

CVoiceGameMgr	g_VoiceGameMgr;

class CMultiplayGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	virtual bool		CanPlayerHearPlayer(CBasePlayer *pListener, CBasePlayer *pTalker)
	{
		if ( g_teamplay )
		{
			if ( g_pGameRules->PlayerRelationship( pListener, pTalker ) != GR_TEAMMATE )
			{
				return false;
			}
		}

		return true;
	}
};
static CMultiplayGameMgrHelper g_GameMgrHelper;

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

CHalfLifeMultiplay :: CHalfLifeMultiplay()
{
	g_VoiceGameMgr.Init(&g_GameMgrHelper, gpGlobals->maxClients);

	RefreshSkillData();
	SboScoreboard_Reset();	// chat-vgui2: new map
	SboModeInfo_Reset();	// SBO 2.0: scoreboard round / frag limit line
	RoundTime_Reset();	// SBO 2.0: spectator timer
	m_flIntermissionEndTime = 0;
	g_flIntermissionStartTime = 0;
	
	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that 
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	if ( IS_DEDICATED_SERVER() )
	{
		// dedicated server
		char *servercfgfile = (char *)CVAR_GET_STRING( "servercfgfile" );

		if ( servercfgfile && servercfgfile[0] )
		{
			char szCommand[256];
			
			ALERT( at_console, "Executing dedicated server config file\n" );
			sprintf( szCommand, "exec %s\n", servercfgfile );
			SERVER_COMMAND( szCommand );
		}
	}
	else
	{
		// listen server
		char *lservercfgfile = (char *)CVAR_GET_STRING( "lservercfgfile" );

		if ( lservercfgfile && lservercfgfile[0] )
		{
			char szCommand[256];
			
			ALERT( at_console, "Executing listen server config file\n" );
			sprintf( szCommand, "exec %s\n", lservercfgfile );
			SERVER_COMMAND( szCommand );
		}
	}
}

BOOL CHalfLifeMultiplay::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if(g_VoiceGameMgr.ClientCommand(pPlayer, pcmd))
		return TRUE;

	return CGameRules::ClientCommand(pPlayer, pcmd);
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::RefreshSkillData( void )
{
// load all default values
	CGameRules::RefreshSkillData();

// override some values for multiplay.

	// suitcharger
	gSkillData.suitchargerCapacity = 30;

	gSkillData.slaveHealth = RANDOM_FLOAT( 12, 29 );
	gSkillData.slaveDmgClaw = RANDOM_FLOAT( 20, 40 );
	gSkillData.slaveDmgClawrake = RANDOM_FLOAT( 21, 56 );
	gSkillData.bullsquidDmgBite = RANDOM_FLOAT( 26, 43 );
	gSkillData.headcrabDmgBite = RANDOM_FLOAT( 38, 60 );
	gSkillData.hgruntHealth = RANDOM_FLOAT( 30, 52 );
	gSkillData.hgruntDmgKick = RANDOM_FLOAT( 20, 37 );
	gSkillData.gargantuaDmgStomp = RANDOM_FLOAT( 26, 41 );
	gSkillData.hassassinHealth = RANDOM_FLOAT( 150, 270 );
	gSkillData.headcrabHealth = RANDOM_FLOAT( 88, 126 );
	gSkillData.bullsquidDmgWhip = RANDOM_FLOAT( 88, 172 );
	gSkillData.bullsquidDmgSpit = RANDOM_FLOAT( 12, 25 );
	gSkillData.houndeyeDmgBlast = RANDOM_FLOAT( 8, 15 );

	if ( m_Mode == MODE_COOP )
		gSkillData.hgruntShotgunPellets = RANDOM_FLOAT( 16, 29 );
	else
		gSkillData.hgruntShotgunPellets = RANDOM_FLOAT( 24, 38 );

	gSkillData.gargantuaDmgFire = RANDOM_FLOAT( 222, 280 );
	gSkillData.gargantuaHealth = RANDOM_FLOAT( 280, 516 );
	gSkillData.gargantuaDmgSlash = RANDOM_FLOAT( 44, 110 );
	gSkillData.hgruntGrenadeSpeed = RANDOM_FLOAT( 30, 45 );
	gSkillData.bigmommaDmgSlash = RANDOM_FLOAT( 250, 400 );
	gSkillData.bigmommaDmgBlast = 250;
	gSkillData.bullsquidHealth = 280;
	gSkillData.bigmommaRadiusBlast = 200;
	gSkillData.barneyHealth = 230;
	gSkillData.bullsquidHealth = 280;
}

// longest the intermission can last, in seconds
#define MAX_INTERMISSION_TIME		120

extern cvar_t timeleft, fragsleft;
extern cvar_t round_status, roundlimit_status;	// SBO 2.0: mp_round, mp_roundlimit (SboModeInfo_Frame)

extern cvar_t mp_chattime;

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: Think ( void )
{
	g_VoiceGameMgr.Update(gpGlobals->frametime);

	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining = 0;

	if ( g_fGameOver )   // someone else quit the game already
	{
		// bounds check
		int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
		if ( time < 1 )
			CVAR_SET_STRING( "mp_chattime", "1" );
		else if ( time > MAX_INTERMISSION_TIME )
			CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

		m_flIntermissionEndTime = g_flIntermissionStartTime + mp_chattime.value;

		// check to see if we should change levels now
		// SBO 2.0: the next map starts mp_chattime after the intermission began, whether or not a player pressed a
		// key (ReGameDLL with REGAMEDLL_FIXES, IsMultiplayer()). The HL SDK waits here for a key press
		// (m_iEndIntermissionButtonHit) or MAX_INTERMISSION_TIME (120 s)
		if ( m_flIntermissionEndTime < gpGlobals->time )
			ChangeLevel(); // intermission is over

		return;
	}

	if ( m_Mode == MODE_DM || m_Mode == MODE_BR )
	{
		float flTimeLimit = timelimit.value * 60;
		float flFragLimit = fraglimit.value;

		time_remaining = (int)(flTimeLimit ? ( flTimeLimit - gpGlobals->time ) : 0);
		
		if ( flTimeLimit != 0 && gpGlobals->time >= flTimeLimit )
		{
			GoToIntermission();
			return;
		}

		if ( flFragLimit )
		{
			int bestfrags = 9999;
			int remain;

			// check if any player is over the frag limit
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

				if ( pPlayer && pPlayer->pev->frags >= flFragLimit )
				{
					GoToIntermission();
					return;
				}


				if ( pPlayer )
				{
					remain = flFragLimit - pPlayer->pev->frags;
					if ( remain < bestfrags )
					{
						bestfrags = remain;
					}
				}

			}
			frags_remaining = bestfrags;
		}
	}

	// Updates when frags change
	if ( frags_remaining != last_frags )
	{
		g_engfuncs.pfnCvar_DirectSet( &fragsleft, UTIL_VarArgs( "%i", frags_remaining ) );
	}

	// Updates once per second
	if ( timeleft.value != last_time )
	{
		g_engfuncs.pfnCvar_DirectSet( &timeleft, UTIL_VarArgs( "%i", time_remaining ) );
	}

	last_frags = frags_remaining;
	last_time  = time_remaining;
}


//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsMultiplayer( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsDeathmatch( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsCoOp( void )
{
	return gpGlobals->coop;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	if ( !pWeapon->CanDeploy() )
	{
		// that weapon can't deploy anyway.
		return FALSE;
	}

	if ( !pPlayer->m_pActiveItem )
	{
		// player doesn't have an active item!
		return TRUE;
	}

	if ( !pPlayer->m_pActiveItem->CanHolster() )
	{
		// can't put away the active item.
		return FALSE;
	}

	// halflife-updated: cl_autowepswitch
	//Never switch
	if ( pPlayer->m_iAutoWepSwitch == 0 )
	{
		return FALSE;
	}

	//Only switch if not attacking
	if ( pPlayer->m_iAutoWepSwitch == 2 && (pPlayer->m_afButtonLast & (IN_ATTACK | IN_ATTACK2)) != 0 )
	{
		return FALSE;
	}

	if ( pWeapon->iWeight() > pPlayer->m_pActiveItem->iWeight() )
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CHalfLifeMultiplay :: GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon )
{

	CBasePlayerItem *pCheck;
	CBasePlayerItem *pBest;// this will be used in the event that we don't find a weapon in the same category.
	int iBestWeight;
	int i;

	iBestWeight = -1;// no weapon lower than -1 can be autoswitched to
	pBest = NULL;

	if ( !pCurrentWeapon->CanHolster() )
	{
		// can't put this gun away right now, so can't switch.
		return FALSE;
	}

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pCheck = pPlayer->m_rgpPlayerItems[ i ];

		while ( pCheck )
		{
			// halflife-updated (2353ca4, HL25): never auto-switch to these (the RPG: no rocket at point-blank range)
			if ( pCheck->iFlags() & ITEM_FLAG_NOAUTOSWITCHTO )
			{
				pCheck = pCheck->m_pNext;
				continue;
			}

			if ( pCheck->iWeight() > -1 && pCheck->iWeight() == pCurrentWeapon->iWeight() && pCheck != pCurrentWeapon )
			{
				// this weapon is from the same category. 
				if ( pCheck->CanDeploy() )
				{
					if ( pPlayer->SwitchWeapon( pCheck ) )
					{
						return TRUE;
					}
				}
			}
			else if ( pCheck->iWeight() > iBestWeight && pCheck != pCurrentWeapon )// don't reselect the weapon we're trying to get rid of
			{
				//ALERT ( at_console, "Considering %s\n", STRING( pCheck->pev->classname ) );
				// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
				// that the player was using. This will end up leaving the player with his heaviest-weighted 
				// weapon. 
				if ( pCheck->CanDeploy() )
				{
					// if this weapon is useable, flag it as the best
					iBestWeight = pCheck->iWeight();
					pBest = pCheck;
				}
			}

			pCheck = pCheck->m_pNext;
		}
	}

	// if we make it here, we've checked all the weapons and found no useable 
	// weapon in the same catagory as the current weapon. 
	
	// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always 
	// at least get the crowbar, but ya never know.
	if ( !pBest )
	{
		return FALSE;
	}

	pPlayer->SwitchWeapon( pBest );

	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	g_VoiceGameMgr.ClientConnected(pEntity);
	return TRUE;
}

extern int gmsgSayText;
extern int gmsgGameMode;

void CHalfLifeMultiplay :: UpdateGameMode( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, NULL, pPlayer->edict() );
		WRITE_BYTE( SboScoreboard_Active() ? 1 : 0 );  // chat-vgui2: SBO modes: teams (scoreboard categories); else none
	MESSAGE_END();
}

void CHalfLifeMultiplay :: InitHUD( CBasePlayer *pl )
{
	// notify other clients of player joining the game
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s has joined the game\n", 
		( pl->pev->netname && STRING(pl->pev->netname)[0] != 0 ) ? STRING(pl->pev->netname) : "unconnected" ) );

	pl->m_iXP = 0;
	pl->m_iSboTotalXP = 0;	// chat-vgui2: cumulative scoreboard XP restarts on join / map change
	pl->m_iSboSpeed = 300;
	pl->m_iSboHealth = 100;

	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" entered the game\n",  
			STRING( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "model" ) );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" entered the game\n",  
			STRING( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			GETPLAYERUSERID( pl->edict() ) );
	}

	SendHUDState( pl, TRUE );
}

//=========================================================
// "fullupdate" (CBasePlayer::ForceClientDllUpdate; the engine sends the command when a demo recording starts): the
// client state InitHUD sent once, to this client again, so a demo started in mid-game has the scoreboard (game mode,
// categories, scores, XP, round / frag limit line), the spectator timer, who spectates and who is AFK. Messages
// only: nothing of the game changes, nobody is told that somebody joined, no welcome window.
//=========================================================
void CHalfLifeMultiplay :: ResendHUD( CBasePlayer *pl )
{
	SendHUDState( pl, FALSE );
}

// bJoin: the client enters the game (InitHUD): also puts the player on everybody's scoreboard and sends the MOTD.
void CHalfLifeMultiplay :: SendHUDState( CBasePlayer *pl, BOOL bJoin )
{
	UpdateGameMode( pl );

	// chat-vgui2: SBO modes: full scoreboard sync (categories as teams, coop level), see SboScoreboard_*
	BOOL bSboScoreboard = SboScoreboard_Active();
	if ( bSboScoreboard )
	{
		if ( bJoin )
			SboScoreboard_ClientInit( pl );
		else
			SboScoreboard_ClientResend( pl );
	}
	else
	{
	// sending just one score makes the hud scoreboard active;  otherwise
	// it is just disabled for single play
	MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
		WRITE_BYTE( ENTINDEX(pl->edict()) );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
	MESSAGE_END();
	}

	SboModeInfo_ClientInit( pl );	// SBO 2.0: scoreboard round / frag limit line
	RoundTime_ClientInit( pl );	// SBO 2.0: spectator timer

	if ( bJoin )
		SendMOTDToClient( pl->edict() );

	// loop through all active players and send their score info to the new client
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		// FIXME:  Probably don't need to cast this just to read m_iDeaths
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			if ( !bSboScoreboard )
			{
			MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
				WRITE_BYTE( i );	// client number
				WRITE_SHORT( plr->pev->frags );
				WRITE_SHORT( plr->m_iDeaths );
				WRITE_SHORT( 0 );
				WRITE_SHORT( GetTeamIndex( plr->m_szTeamName ) + 1 );
			MESSAGE_END();
			}

			MESSAGE_BEGIN( MSG_ONE, gmsgSpectator, NULL, pl->edict() );
				WRITE_BYTE( i );
				WRITE_BYTE( plr->pev->iuser1 != 0 );
			MESSAGE_END();

			// SBO 2.0: the players with the AFK sign (the client forgot them all with InitHUD)
			if ( plr->m_bAfkMarked )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgPlayerAfk, NULL, pl->edict() );
					WRITE_BYTE( i );
					WRITE_BYTE( 1 );
				MESSAGE_END();
			}
		}
	}

	if ( g_fGameOver )
	{
		MESSAGE_BEGIN( MSG_ONE, SVC_INTERMISSION, NULL, pl->edict() );
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		if ( pPlayer )
		{
			pPlayer->m_bNotInGame = 1;

			// Tell all clients this player isn't a spectator anymore
			MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );  
				WRITE_BYTE( ENTINDEX(pClient) );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			FireTargets( "game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0 );

			// team match?
			if ( g_teamplay )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected\n",  
					STRING( pPlayer->pev->netname ), 
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" ) );
			}
			else
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%i>\" disconnected\n",  
					STRING( pPlayer->pev->netname ), 
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					GETPLAYERUSERID( pPlayer->edict() ) );
			}

			pPlayer->RemoveAllItems( TRUE );// destroy all of the players weapons and items

			// Tell all clients this player isn't a spectator anymore
			MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );  
				WRITE_BYTE( ENTINDEX(pClient) );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			// by index, not by classname: SBO renames its observers to "observer", so a "player" search missed
			// exactly the spectators (ClientDisconnect set has_disconnected, so this player is no target any more)
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *client = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( !client || !client->pev )
					continue;
				if ( client == pPlayer )
					continue;

				// If a spectator was chasing this player, move him/her onto the next player
				if ( client->m_hObserverTarget == pPlayer && client->IsObserver() )
				{
					// the next player in the same mode; nobody else to watch: roaming, and follow again once
					// somebody is (ReGameDLL_CS m_bWasFollowing). The SDK set the mode again here, which printed
					// the mode's text although the mode stayed.
					client->m_hObserverTarget = NULL;
					client->Observer_CheckTarget();
				}
			}
		}
	}
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	// subtract off the speed at which a player is allowed to fall without being hurt,
	// so damage will be based on speed beyond that, not the entire fall
	pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
	return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerThink( CBasePlayer *pPlayer )
{
	if ( g_fGameOver )
	{
		// check for button presses
		if ( pPlayer->m_afButtonPressed & ( IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP ) )
			m_iEndIntermissionButtonHit = TRUE;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}
}

//=========================================================
// SBO: turn a player into an observer
//=========================================================
void CHalfLifeMultiplay :: CheckObserver( CBasePlayer *pPlayer )
{
	pPlayer->pev->classname = MAKE_STRING( "observer" );
	pPlayer->pev->solid = SOLID_NOT;
	pPlayer->pev->takedamage = DAMAGE_NO;
	pPlayer->pev->movetype = MOVETYPE_NONE;
	pPlayer->pev->flags |= FL_NOTARGET;
	pPlayer->pev->effects |= EF_NODRAW;
	pPlayer->m_afPhysicsFlags |= PFLAG_OBSERVER;
	pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT | HIDEHUD_HEALTH;
	pPlayer->m_bOutOfPlay = 1;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerSpawn( CBasePlayer *pPlayer )
{
	BOOL		addDefault;
	CBaseEntity	*pWeaponEntity = NULL;

	// halflife-updated: Ensure the player switches to the spawn weapons regardless of cl_autowepswitch
	const int originalAutoWepSwitch = pPlayer->m_iAutoWepSwitch;
	pPlayer->m_iAutoWepSwitch = 1;

	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);
	
	addDefault = TRUE;

	while ( pWeaponEntity = UTIL_FindEntityByClassname( pWeaponEntity, "game_player_equip" ))
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = FALSE;
	}

	if ( addDefault )
	{
		pPlayer->GiveNamedItem( "weapon_wong" );
	}

	pPlayer->m_iAutoWepSwitch = originalAutoWepSwitch;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return TRUE;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerSpawnTime( CBasePlayer *pPlayer )
{
	return gpGlobals->time;//now!
}

BOOL CHalfLifeMultiplay :: AllowAutoTargetCrosshair( void )
{
	return ( aimcrosshair.value != 0 );
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeMultiplay :: IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return 1;
}


//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
void CHalfLifeMultiplay :: PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	DeathNotice( pVictim, pKiller, pInflictor );

	pVictim->m_iDeaths += 1;


	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );
	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;

	if ( pVictim->pev == pKiller )  
	{  // killed self
		// SBO 2.0: no points lost on br_ maps (CBr::PlayerKilled ends here; owner: a loss only held the match up).
		// 1.38: -5 there too; sp_ (teamplay) keeps it
		if ( m_Mode != MODE_BR )
			pKiller->frags -= 5;
	}
	else if ( ktmp && ktmp->IsPlayer() )
	{
		// if a player dies in a deathmatch game and the killer is a client, award the killer some points
		pKiller->frags += IPointsForKill( peKiller, pVictim );
		
		FireTargets( "game_playerkill", ktmp, ktmp, USE_TOGGLE, 0 );
	}

	// update the scores
	// killed scores
	SboScoreboard_SendScoreInfo( pVictim );	// chat-vgui2: scoreboard team + level

	// killers score, if it's a player
	CBaseEntity *ep = CBaseEntity::Instance( pKiller );
	if ( ep && ep->Classify() == CLASS_PLAYER )
	{
		CBasePlayer *PK = (CBasePlayer*)ep;

		SboScoreboard_SendScoreInfo( PK );	// chat-vgui2: scoreboard team + level

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}
#ifndef HLDEMO_BUILD
	if ( pVictim->HasNamedPlayerItem("weapon_satchel") )
	{
		DeactivateSatchels( pVictim );
	}
#endif
}

//=========================================================
// Deathnotice. 
//=========================================================
void CHalfLifeMultiplay::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor )
{
	// Work out what killed the player, and send a message to all clients about it
	CBaseEntity *Killer = CBaseEntity::Instance( pKiller );

	const char *killer_weapon_name = "world";		// by default, the player is killed by the world
	int killer_index = 0;
	
	// Hack to fix name change
	char *tau = "tau_cannon";
	char *gluon = "gluon gun";

	if ( pKiller->flags & FL_CLIENT )
	{
		killer_index = ENTINDEX(ENT(pKiller));
		
		if ( pevInflictor )
		{
			if ( pevInflictor == pKiller )
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				CBasePlayer *pPlayer = (CBasePlayer*)CBaseEntity::Instance( pKiller );
				
				if ( pPlayer->m_pActiveItem )
				{
					killer_weapon_name = pPlayer->m_pActiveItem->pszName();
				}
			}
			else
			{
				killer_weapon_name = SboKillfeed_InflictorCode( pevInflictor );  // it's just that easy
			}
		}
	}
	else
	{
		killer_weapon_name = SboKillfeed_InflictorCode( pevInflictor );
	}

	// strip the monster_* or weapon_* from the inflictor's classname
	if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		killer_weapon_name += 7;
	else if ( strncmp( killer_weapon_name, "monster_", 8 ) == 0 )
		killer_weapon_name += 8;
	else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		killer_weapon_name += 5;

	// SBO 2.0 GM mode: a Game Master's monster (or what it fired) killed: the kill feed's d_gm icon
	if ( !( pKiller->flags & FL_CLIENT ) && GmMode_KillerOwner( pKiller, pevInflictor ) )
		killer_weapon_name = "gm";

	// SBO 2.0: headshot flag (CS 1.6 DeathMsg), appended after HL's fields
	int iHeadshot = SboKillfeed_Headshot( pVictim );

	// chat-vgui2: SboDeathMsg (NPC killer name) before the stock DeathMsg
	SboKillfeed_PlayerDeath( pVictim, pKiller, pevInflictor, killer_weapon_name, iHeadshot );

	// the kill feed's name for an NPC killer (NULL: a player, or the world); the log line below uses it too
	char szNpcKiller[96];
	const char *pszNpcKiller = SboKillfeed_KillerNpcName( pKiller, pevInflictor, szNpcKiller, sizeof( szNpcKiller ) );

	MESSAGE_BEGIN( MSG_ALL, gmsgDeathMsg );
		WRITE_BYTE( killer_index );						// the killer
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );		// the victim
		WRITE_STRING( killer_weapon_name );		// what they were killed by (should this be a string?)
		WRITE_BYTE( iHeadshot );		// SBO 2.0: headshot (CS 1.6 sends it as the 3rd field; appended here so that HL's
										// order, which plugins read as arguments 1-3, stays the same)
	MESSAGE_END();

	// replace the code names with the 'real' names
	if ( !strcmp( killer_weapon_name, "egon" ) )
		killer_weapon_name = gluon;
	else if ( !strcmp( killer_weapon_name, "gauss" ) )
		killer_weapon_name = tau;

	if ( pVictim->pev == pKiller )  
	{
		// killed self

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( ENT(pKiller) ), "model" ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" killed \"%s<%i><%s><%i>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				GETPLAYERUSERID( ENT(pKiller) ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );
		}
	}
	else if ( pszNpcKiller )
	{
		// killed by an NPC: not a suicide. The NPC has no userid / authid, so it is named like the kill feed names it
		// ("Fed"); no weapon (it would be the monster's class name)

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" was killed by \"%s\"\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				pszNpcKiller );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" was killed by \"%s\"\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				pszNpcKiller );		
		}
	}
	else
	{ 
		// killed by the world

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}

	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE ( 9 );	// command length in bytes
		WRITE_BYTE ( DRC_CMD_EVENT );	// player killed
		WRITE_SHORT( ENTINDEX(pVictim->edict()) );	// index number of primary entity
		if (pevInflictor)
			WRITE_SHORT( ENTINDEX(ENT(pevInflictor)) );	// index number of secondary entity
		else
			WRITE_SHORT( ENTINDEX(ENT(pKiller)) );	// index number of secondary entity
		WRITE_LONG( 7 | DRC_FLAG_DRAMATIC);   // eventflags (priority and flags)
	MESSAGE_END();

//  Print a standard message
	// TODO: make this go direct to console
	return; // just remove for now
/*
	char	szText[ 128 ];

	if ( pKiller->flags & FL_MONSTER )
	{
		// killed by a monster
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was killed by a monster.\n" );
		return;
	}

	if ( pKiller == pVictim->pev )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " commited suicide.\n" );
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		strcpy ( szText, STRING( pKiller->netname ) );

		strcat( szText, " : " );
		strcat( szText, killer_weapon_name );
		strcat( szText, " : " );

		strcat ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, "\n" );
	}
	else if ( FClassnameIs ( pKiller, "worldspawn" ) )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " fell or drowned or something.\n" );
	}
	else if ( pKiller->solid == SOLID_BSP )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was mooshed.\n" );
	}
	else
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " died mysteriously.\n" );
	}

	UTIL_ClientPrintAll( szText );
*/
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
void CHalfLifeMultiplay :: PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHalfLifeMultiplay :: FlWeaponRespawnTime( CBasePlayerItem *pWeapon )
{
	if ( weaponstay.value > 0 )
	{
		// make sure it's only certain weapons
		if ( !(pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
		{
			return gpGlobals->time + 0;		// weapon respawns almost instantly
		}
	}

	return gpGlobals->time + WEAPON_RESPAWN_TIME;
}

// when we are within this close to running out of entities,  items 
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE	100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHalfLifeMultiplay :: FlWeaponTryRespawn( CBasePlayerItem *pWeapon )
{
	if ( pWeapon && pWeapon->m_iId && (pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
	{
		if ( NUMBER_OF_ENTITIES() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE) )
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime( pWeapon );
	}

	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay :: VecWeaponRespawnSpot( CBasePlayerItem *pWeapon )
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHalfLifeMultiplay :: WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	return GR_WEAPON_RESPAWN_NO;
}

//=========================================================
// CanHaveWeapon - returns FALSE if the player is not allowed
// to pick up this weapon
//=========================================================
BOOL CHalfLifeMultiplay::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if ( weaponstay.value > 0 )
	{
		if ( pItem->iFlags() & ITEM_FLAG_LIMITINWORLD )
			return CGameRules::CanHavePlayerItem( pPlayer, pItem );

		// check if the player already has this weapon
		for ( int i = 0 ; i < MAX_ITEM_TYPES ; i++ )
		{
			CBasePlayerItem *it = pPlayer->m_rgpPlayerItems[i];

			while ( it != NULL )
			{
				if ( it->m_iId == pItem->m_iId )
				{
					return FALSE;
				}

				it = it->m_pNext;
			}
		}
	}

	return CGameRules::CanHavePlayerItem( pPlayer, pItem );
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem )
{
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::ItemShouldRespawn( CItem *pItem )
{
	return GR_ITEM_RESPAWN_NO;
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeMultiplay::FlItemRespawnTime( CItem *pItem )
{
	return gpGlobals->time + ITEM_RESPAWN_TIME;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecItemRespawnSpot( CItem *pItem )
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotAmmo( CBasePlayer *pPlayer, char *szName, int iCount )
{
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsAllowedToSpawn( CBaseEntity *pEntity )
{
//	if ( pEntity->pev->flags & FL_MONSTER )
//		return FALSE;

	return TRUE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::AmmoShouldRespawn( CBasePlayerAmmo *pAmmo )
{
	return GR_AMMO_RESPAWN_NO;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlAmmoRespawnTime( CBasePlayerAmmo *pAmmo )
{
	return gpGlobals->time + AMMO_RESPAWN_TIME;
}

//=========================================================
//=========================================================
Vector CHalfLifeMultiplay::VecAmmoRespawnSpot( CBasePlayerAmmo *pAmmo )
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlHealthChargerRechargeTime( void )
{
	return 60;
}


float CHalfLifeMultiplay::FlHEVChargerRechargeTime( void )
{
	return 30;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	if ( m_Mode == MODE_BR )
		return GR_PLR_DROP_GUN_ALL;

	return GR_PLR_DROP_GUN_NO;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_AMMO_ACTIVE;
}

edict_t *CHalfLifeMultiplay::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = CGameRules::GetPlayerSpawnSpot( pPlayer );	
	if ( IsMultiplayer() && pentSpawnSpot->v.target )
	{
		FireTargets( STRING(pentSpawnSpot->v.target), pPlayer, pPlayer, USE_TOGGLE, 0 );
	}

	return pentSpawnSpot;
}


//=========================================================
//=========================================================
int CHalfLifeMultiplay::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	// half life deathmatch has only enemies
	return GR_NOTTEAMMATE;
}

BOOL CHalfLifeMultiplay :: PlayFootstepSounds( CBasePlayer *pl, float fvol )
{
	if ( g_footsteps && g_footsteps->value == 0 )
		return FALSE;

	if ( pl->IsOnLadder() || pl->pev->velocity.Length2D() > 220 )
		return TRUE;  // only make step sounds in multiplayer if the player is moving fast enough

	return FALSE;
}

BOOL CHalfLifeMultiplay :: FAllowFlashlight( void ) 
{ 
	return flashlight.value != 0; 
}

//=========================================================
//======== CHalfLifeMultiplay private functions ===========
#define INTERMISSION_TIME		6

void CHalfLifeMultiplay :: GoToIntermission( void )
{
	if ( g_fGameOver )
		return;  // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();

	// bounds check
	int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
	if ( time < 1 )
		CVAR_SET_STRING( "mp_chattime", "1" );
	else if ( time > MAX_INTERMISSION_TIME )
		CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

	m_flIntermissionEndTime = gpGlobals->time + ( (int)mp_chattime.value );
	g_flIntermissionStartTime = gpGlobals->time;

	g_fGameOver = TRUE;
	m_iEndIntermissionButtonHit = FALSE;
}

#define MAX_RULE_BUFFER 1024

typedef struct mapcycle_item_s
{
	struct mapcycle_item_s *next;

	char mapname[ 32 ];
	int  minplayers, maxplayers;
	char rulebuffer[ MAX_RULE_BUFFER ];
} mapcycle_item_t;

typedef struct mapcycle_s
{
	struct mapcycle_item_s *items;
	struct mapcycle_item_s *next_item;
} mapcycle_t;

/*
==============
DestroyMapCycle

Clean up memory used by mapcycle when switching it
==============
*/
void DestroyMapCycle( mapcycle_t *cycle )
{
	mapcycle_item_t *p, *n, *start;
	p = cycle->items;
	if ( p )
	{
		start = p;
		p = p->next;
		while ( p != start )
		{
			n = p->next;
			delete p;
			p = n;
		}
		
		delete cycle->items;
	}
	cycle->items = NULL;
	cycle->next_item = NULL;
}

static char com_token[ 1500 ];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int             c;
	int             len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting( char *buffer )
{
	char *p;

	p = buffer;
	while ( *p && *p!='\n')
	{
		if ( !isspace( *p ) || isalnum( *p ) )
			return 1;

		p++;
	}

	return 0;
}



/*
==============
ReloadMapCycleFile


Parses mapcycle.txt file into mapcycle_t structure
==============
*/
int ReloadMapCycleFile( char *filename, mapcycle_t *cycle )
{
	char szBuffer[ MAX_RULE_BUFFER ];
	char szMap[ 32 ];
	int length;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( filename, &length );
	int hasbuffer;
	mapcycle_item_s *item, *newlist = NULL, *next;

	if ( pFileList && length )
	{
		// the first map name in the file becomes the default
		while ( 1 )
		{
			hasbuffer = 0;
			memset( szBuffer, 0, MAX_RULE_BUFFER );

			pFileList = COM_Parse( pFileList );
			if ( strlen( com_token ) <= 0 )
				break;

			// halflife-updated (1c202f5): bounded copy, an over-long token in mapcycle.txt overflowed the stack
			strncpy( szMap, com_token, sizeof( szMap ) );
			szMap[ sizeof( szMap ) - 1 ] = '\0';

			// Any more tokens on this line?
			if ( COM_TokenWaiting( pFileList ) )
			{
				pFileList = COM_Parse( pFileList );
				if ( strlen( com_token ) > 0 )
				{
					hasbuffer = 1;
					strncpy( szBuffer, com_token, sizeof( szBuffer ) );
					szBuffer[ sizeof( szBuffer ) - 1 ] = '\0';
				}
			}

			// Check map
			if ( IS_MAP_VALID( szMap ) )
			{
				// Create entry
				char *s;

				item = new mapcycle_item_s;

				strcpy( item->mapname, szMap );

				item->minplayers = 0;
				item->maxplayers = 0;

				memset( item->rulebuffer, 0, MAX_RULE_BUFFER );

				if ( hasbuffer )
				{
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "minplayers" );
					if ( s && s[0] )
					{
						item->minplayers = atoi( s );
						item->minplayers = max( item->minplayers, 0 );
						item->minplayers = min( item->minplayers, gpGlobals->maxClients );
					}
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "maxplayers" );
					if ( s && s[0] )
					{
						item->maxplayers = atoi( s );
						item->maxplayers = max( item->maxplayers, 0 );
						item->maxplayers = min( item->maxplayers, gpGlobals->maxClients );
					}

					// Remove keys
					//
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "minplayers" );
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "maxplayers" );

					strcpy( item->rulebuffer, szBuffer );
				}

				item->next = cycle->items;
				cycle->items = item;
			}
			else
			{
				ALERT( at_console, "Skipping %s from mapcycle, not a valid map\n", szMap );
			}

		}

		FREE_FILE( aFileList );
	}

	// Fixup circular list pointer
	item = cycle->items;

	// Reverse it to get original order
	while ( item )
	{
		next = item->next;
		item->next = newlist;
		newlist = item;
		item = next;
	}
	cycle->items = newlist;
	item = cycle->items;

	// Didn't parse anything
	if ( !item )
	{
		return 0;
	}

	while ( item->next )
	{
		item = item->next;
	}
	item->next = cycle->items;
	
	cycle->next_item = item->next;

	return 1;
}

/*
==============
CountPlayers

Determine the current # of active players on the server for map cycling logic
==============
*/
int CountPlayers( void )
{
	int	num = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

		if ( pEnt )
		{
			num = num + 1;
		}
	}

	return num;
}

/*
==============
ExtractCommandString

Parse commands/key value pairs to issue right after map xxx command is issued on server
 level transition
==============
*/
void ExtractCommandString( char *s, char *szCommand )
{
	// Now make rules happen
	char	pkey[512];
	char	value[512];	// use two buffers so compares
								// work without stomping on each other
	char	*o;
	
	if ( *s == '\\' )
		s++;

	while (1)
	{
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		strcat( szCommand, pkey );
		if ( strlen( value ) > 0 )
		{
			strcat( szCommand, " " );
			strcat( szCommand, value );
		}
		strcat( szCommand, "\n" );

		if (!*s)
			return;
		s++;
	}
}

/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
void CHalfLifeMultiplay :: ChangeLevel( void )
{
	static char szPreviousMapCycleFile[ 256 ];
	static mapcycle_t mapcycle;

	char szNextMap[32];
	char szFirstMapInList[32];
	char szCommands[ 1500 ];
	char szRules[ 1500 ];
	int minplayers = 0, maxplayers = 0;
	strcpy( szFirstMapInList, "hldm1" );  // the absolute default level is hldm1

	int	curplayers;
	BOOL do_cycle = TRUE;

	// find the map to change to
	char *mapcfile = (char*)CVAR_GET_STRING( "mapcyclefile" );
	ASSERT( mapcfile != NULL );

	szCommands[ 0 ] = '\0';
	szRules[ 0 ] = '\0';

	curplayers = CountPlayers();

	// Has the map cycle filename changed?
	if ( stricmp( mapcfile, szPreviousMapCycleFile ) )
	{
		strcpy( szPreviousMapCycleFile, mapcfile );

		DestroyMapCycle( &mapcycle );

		if ( !ReloadMapCycleFile( mapcfile, &mapcycle ) || ( !mapcycle.items ) )
		{
			ALERT( at_console, "Unable to load map cycle file %s\n", mapcfile );
			do_cycle = FALSE;
		}
	}

	if ( do_cycle && mapcycle.items )
	{
		BOOL keeplooking = FALSE;
		BOOL found = FALSE;
		mapcycle_item_s *item;

		// Assume current map
		strcpy( szNextMap, STRING(gpGlobals->mapname) );
		strcpy( szFirstMapInList, STRING(gpGlobals->mapname) );

		// Traverse list
		for ( item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next )
		{
			keeplooking = FALSE;

			ASSERT( item != NULL );

			if ( item->minplayers != 0 )
			{
				if ( curplayers >= item->minplayers )
				{
					found = TRUE;
					minplayers = item->minplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( item->maxplayers != 0 )
			{
				if ( curplayers <= item->maxplayers )
				{
					found = TRUE;
					maxplayers = item->maxplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( keeplooking )
				continue;

			found = TRUE;
			break;
		}

		if ( !found )
		{
			item = mapcycle.next_item;
		}			
		
		// Increment next item pointer
		mapcycle.next_item = item->next;

		// Perform logic on current item
		strcpy( szNextMap, item->mapname );

		ExtractCommandString( item->rulebuffer, szCommands );
		strcpy( szRules, item->rulebuffer );
	}

	if ( !IS_MAP_VALID(szNextMap) )
	{
		strcpy( szNextMap, szFirstMapInList );
	}

	g_fGameOver = TRUE;

	ALERT( at_console, "CHANGE LEVEL: %s\n", szNextMap );
	if ( minplayers || maxplayers )
	{
		ALERT( at_console, "PLAYER COUNT:  min %i max %i current %i\n", minplayers, maxplayers, curplayers );
	}
	if ( strlen( szRules ) > 0 )
	{
		ALERT( at_console, "RULES:  %s\n", szRules );
	}
	
	CHANGE_LEVEL( szNextMap, NULL );
	if ( strlen( szCommands ) > 0 )
	{
		SERVER_COMMAND( szCommands );
	}
}

#define MAX_MOTD_CHUNK	  60
// SBO 2.0: 3072 (was 1536, the HL SDK / halflife-updated / ReGameDLL value), BugfixedHL-Rebased's
// MAX_UNICODE_MOTD_LENGTH (MAX_MOTD_LENGTH * 2). Must match MAX_MOTD_LENGTH in cl_dll/hud.h (the client's
// m_szMOTD). Up to 52 chunks of 60 + 4 bytes = ~3.3 KB of reliable data per client in InitHUD's frame.
#define MAX_MOTD_LENGTH   3072

void CHalfLifeMultiplay :: SendMOTDToClient( edict_t *client )
{
	// read from the MOTD.txt file
	int length, char_count = 0;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( (char *)CVAR_GET_STRING( "motdfile" ), &length );

	// send the server name
	MESSAGE_BEGIN( MSG_ONE, gmsgServerName, NULL, client );
		WRITE_STRING( CVAR_GET_STRING("hostname") );
	MESSAGE_END();

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	while ( pFileList && *pFileList && char_count < MAX_MOTD_LENGTH )
	{
		char chunk[MAX_MOTD_CHUNK+1];
		
		if ( strlen( pFileList ) < MAX_MOTD_CHUNK )
		{
			strcpy( chunk, pFileList );
		}
		else
		{
			strncpy( chunk, pFileList, MAX_MOTD_CHUNK );
			chunk[MAX_MOTD_CHUNK] = 0;		// strncpy doesn't always append the null terminator
		}

		char_count += strlen( chunk );
		if ( char_count < MAX_MOTD_LENGTH )
			pFileList = aFileList + char_count; 
		else
			*pFileList = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgMOTD, NULL, client );
			WRITE_BYTE( *pFileList ? FALSE : TRUE );	// FALSE means there is still more message to come
			WRITE_STRING( chunk );
		MESSAGE_END();
	}

	FREE_FILE( aFileList );
}
	

//=========================================================
// chat-vgui2 branch: SBO scoreboard categories (native port of the 1.38 scoreboard
// plugin, AMX Mod X). In the SBO modes (co_ coop, br_ battle royale, and the default Bounty Killer
// mode CDm, the plugin's "BK" maps) every player is put in one of five scoreboard "teams"; clients colour names
// (scoreboard, chat, death notices) by team number: 1 blue, 2 red, 3 yellow, 4 green, else grey.
//
//   1 "Game Master"     coop/br: GM mode on (gmmode 1 and m_bGameMaster == 1, coop's "gmon")
//     "Hunted"          Bounty Killer: the hunted (m_bHunted == 1) while in play (the scoreboard lists it with the
//                       alive players, marked; the team number still gives the blue name in chat and kill feed)
//   5 "Spectators"      spectator mode (m_bSpectating, observer.cpp; the plugin read the spectate-bypass module's
//                       pev->iuser4 & (1<<15))
//   4 "Alive Ops"       in play (alive, not an observer)
//                       (Bounty Killer: in play, not the hunted)
//   3 "No Loadout"      never in play this map and no loadout chosen yet (m_iSpawnMode == 0). Not in Bounty Killer:
//                       there such a player is in 2 (the label is still sent, the category stays empty)
//   2 "Dead Ops"        everything else: dead, or waiting in observer mode between rounds / with a loadout
//
// HLTV proxies get no team (they still get the full scoreboard). Teamplay (sp_ maps) keeps its model teams: the
// whole system is off there. pev->team and m_szTeamName are set to the category (as the plugin did); in the SBO
// modes nothing in the game logic reads them (non-teamplay rules: GetTeamIndex -1, PlayerRelationship
// GR_NOTTEAMMATE, UTIL_TeamsMatch TRUE, g_teamplay is never set).
//
// The ScoreInfo "frags" column shows the coop level, (m_iSboSpeed - 300) / 5 (the plugin's m_iHpBonus, +5 per
// level-up), in coop; frags in br, Bounty Killer and for Game Masters.
// The ScoreInfo "playerclass" short (0 in Half-Life deathmatch, unused by SBO) is 1 for Bounty Killer's hunted
// (m_bHunted == 1), in play or dead, else 0: the role stays with a hunted who killed himself or was killed by the
// world, and the team number alone (2 while dead) does not say so.
//
// Messages: TeamNames (5 labels; team 1's label is "" while no GM is active, as the plugin did), TeamInfo
// (player, category name) and ScoreInfo (player, level/frags, deaths, 0, team). GameMode 1 (UpdateGameMode) so the
// scoreboard groups by team. Sent: full sync to a client in InitHUD; on change for one player (spawn, death,
// observer start/stop, GM toggle, any ScoreInfo the game sends); a per-frame check (StartFrame) catches state
// changed elsewhere (hunted hand-over, round transitions); TeamNames when the number
// of GMs goes between 0 and non-zero; an empty TeamInfo on disconnect.
//=========================================================
extern int gmsgTeamInfo;
extern int gmsgTeamNames;
extern int gmsgSboXP;

#define SBO_BASE_LEVEL_BONUS		300
#define SBO_LEVEL_BONUS_STEP		5
#define SBO_SB_MAXPLAYERS			32

static const char *s_pszSboTeams[6] = { "", "Game Master", "Dead Ops", "No Loadout", "Alive Ops", "Spectators" };
static const char *s_pszSboTeamsBK[6] = { "", "Hunted", "Dead Ops", "No Loadout", "Alive Ops", "Spectators" };

static BOOL	s_bSboInit[SBO_SB_MAXPLAYERS + 1];		// InitHUD done (in the game, scoreboard synced)
static BOOL	s_bSboSpawned[SBO_SB_MAXPLAYERS + 1];	// seen in play this map (the plugin's g_bHasSpawned)
static int	s_iSboTeam[SBO_SB_MAXPLAYERS + 1];		// last sent team / score / deaths (-1/-32768: none)
static int	s_iSboScore[SBO_SB_MAXPLAYERS + 1];
static int	s_iSboDeaths[SBO_SB_MAXPLAYERS + 1];
static int	s_iSboClass[SBO_SB_MAXPLAYERS + 1];		// last sent ScoreInfo playerclass (Bounty Killer: 1 = the hunted)
static int	s_iSboXP[SBO_SB_MAXPLAYERS + 1];		// coop: last sent XP (m_iSboTotalXP; INT_MIN: none)
static int	s_iSboGMLabel = -1;						// team 1 label last broadcast: 0 "", 1 "Game Master"

static void SboScoreboard_ResetSlot( int i )
{
	s_bSboInit[i] = FALSE;
	s_bSboSpawned[i] = FALSE;
	s_iSboTeam[i] = -1;
	s_iSboScore[i] = -32768;
	s_iSboDeaths[i] = -32768;
	s_iSboClass[i] = 0;
	s_iSboXP[i] = INT_MIN;	// not -32768: the XP can go negative (CCoop::PlayerKilled's penalty)
}

void SboScoreboard_Reset( void )
{
	for ( int i = 0; i <= SBO_SB_MAXPLAYERS; i++ )
		SboScoreboard_ResetSlot( i );
	s_iSboGMLabel = -1;
}

BOOL SboScoreboard_Active( void )
{
	return m_Mode == MODE_COOP || m_Mode == MODE_BR || m_Mode == MODE_DM;
}

static BOOL SboScoreboard_IsBK( void )
{
	return m_Mode == MODE_DM;
}

BOOL SboScoreboard_InPlay( CBasePlayer *pPlayer )
{
	// SBO's waiting/dead players sit in observer mode (classname "observer", PFLAG_OBSERVER, iuser1), often with
	// full health and deadflag DEAD_NO, so IsAlive() alone isn't enough. m_bOutOfPlay is the modes' own "not in
	// the round" flag (0 only after a real spawn; the round logic counts live players with it).
	return pPlayer->IsAlive() && !( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER ) && !pPlayer->pev->iuser1
		&& !pPlayer->m_bOutOfPlay && !FClassnameIs( pPlayer->pev, "observer" );
}

static BOOL SboScoreboard_IsGM( CBasePlayer *pPlayer )
{
	return !SboScoreboard_IsBK() && pPlayer->m_bGameMaster == 1;	// SBO 2.0: also a Game Master an admin made with mp_gmmode 0 (gm_toggle)
}

static int SboScoreboard_GetTeam( CBasePlayer *pPlayer )
{
	int i = pPlayer->entindex();

	if ( pPlayer->pev->flags & FL_PROXY )
		return 0;

	if ( SboScoreboard_IsGM( pPlayer ) )
		return 1;

	if ( pPlayer->m_bSpectating )
		return 5;

	if ( SboScoreboard_InPlay( pPlayer ) )
	{
		if ( i >= 1 && i <= SBO_SB_MAXPLAYERS && s_bSboInit[i] )
			s_bSboSpawned[i] = TRUE;

		if ( SboScoreboard_IsBK() && pPlayer->m_bHunted == 1 )
			return 1;
		return 4;
	}

	// Bounty Killer has no "No Loadout": a player who has not chosen a loadout yet is with the dead
	if ( !SboScoreboard_IsBK() && !( i >= 1 && i <= SBO_SB_MAXPLAYERS && s_bSboSpawned[i] ) && pPlayer->m_iSpawnMode == 0 )
		return 3;

	return 2;
}

static const char *SboScoreboard_TeamName( int iTeam )
{
	if ( iTeam < 0 || iTeam > 5 )
		iTeam = 0;
	return SboScoreboard_IsBK() ? s_pszSboTeamsBK[iTeam] : s_pszSboTeams[iTeam];
}

static int SboScoreboard_GetScore( CBasePlayer *pPlayer )
{
	if ( m_Mode != MODE_COOP || SboScoreboard_IsGM( pPlayer ) )
		return (int)pPlayer->pev->frags;

	return ( pPlayer->m_iSboSpeed - SBO_BASE_LEVEL_BONUS ) / SBO_LEVEL_BONUS_STEP;
}

// ScoreInfo playerclass: Bounty Killer: 1 = the hunted, alive or dead (see the header)
static int SboScoreboard_GetClass( CBasePlayer *pPlayer )
{
	return ( SboScoreboard_IsBK() && pPlayer->m_bHunted == 1 ) ? 1 : 0;
}

static void SboScoreboard_WriteTeamInfo( int iDest, edict_t *pTo, int iClient, const char *pszTeam )
{
	MESSAGE_BEGIN( iDest, gmsgTeamInfo, NULL, pTo );
		WRITE_BYTE( iClient );
		WRITE_STRING( pszTeam );
	MESSAGE_END();
}

static void SboScoreboard_WriteScoreInfo( int iDest, edict_t *pTo, int iClient, int iScore, int iDeaths, int iClass, int iTeam )
{
	MESSAGE_BEGIN( iDest, gmsgScoreInfo, NULL, pTo );
		WRITE_BYTE( iClient );
		WRITE_SHORT( iScore );
		WRITE_SHORT( iDeaths );
		WRITE_SHORT( iClass );
		WRITE_SHORT( iTeam );
	MESSAGE_END();
}

// "SboXP" (byte client, long XP): coop XP (m_iSboTotalXP, cumulative; SBO_XP_GAIN stops each gain at the
// level-up) for the new clients' scoreboard XP column. Old clients don't hook it (registered, so the engine skips it).
static void SboScoreboard_WriteXP( int iDest, edict_t *pTo, int iClient, int iXP )
{
	MESSAGE_BEGIN( iDest, gmsgSboXP, NULL, pTo );
		WRITE_BYTE( iClient );
		WRITE_LONG( iXP );
	MESSAGE_END();
}

static void SboScoreboard_WriteTeamNames( int iDest, edict_t *pTo, int iGMLabel )
{
	MESSAGE_BEGIN( iDest, gmsgTeamNames, NULL, pTo );
		WRITE_BYTE( 5 );
		WRITE_STRING( SboScoreboard_IsBK() ? s_pszSboTeamsBK[1] : ( iGMLabel ? s_pszSboTeams[1] : "" ) );
		WRITE_STRING( SboScoreboard_TeamName( 2 ) );
		WRITE_STRING( SboScoreboard_TeamName( 3 ) );
		WRITE_STRING( SboScoreboard_TeamName( 4 ) );
		WRITE_STRING( SboScoreboard_TeamName( 5 ) );
	MESSAGE_END();
}

// Recompute a player's category/score; broadcast TeamInfo when the team changed and ScoreInfo when anything
// changed (iForce: SBO_SB_FORCE_TEAMINFO / SBO_SB_FORCE_SCOREINFO send regardless).
void SboScoreboard_Update( CBasePlayer *pPlayer, int iForce )
{
	if ( !pPlayer || !SboScoreboard_Active() )
		return;

	int i = pPlayer->entindex();
	if ( i < 1 || i > SBO_SB_MAXPLAYERS || !s_bSboInit[i] )
		return;

	int iTeam = SboScoreboard_GetTeam( pPlayer );
	int iScore = SboScoreboard_GetScore( pPlayer );
	int iDeaths = pPlayer->m_iDeaths;
	int iClass = SboScoreboard_GetClass( pPlayer );

	if ( iTeam != s_iSboTeam[i] || ( iForce & SBO_SB_FORCE_TEAMINFO ) )
	{
		const char *pszTeam = SboScoreboard_TeamName( iTeam );

		pPlayer->pev->team = iTeam;
		strncpy( pPlayer->m_szTeamName, pszTeam, TEAM_NAME_LENGTH );
		pPlayer->m_szTeamName[TEAM_NAME_LENGTH - 1] = 0;

		SboScoreboard_WriteTeamInfo( MSG_ALL, NULL, i, pszTeam );
		iForce |= SBO_SB_FORCE_SCOREINFO;
	}

	if ( iTeam != s_iSboTeam[i] || iScore != s_iSboScore[i] || iDeaths != s_iSboDeaths[i] || iClass != s_iSboClass[i]
		|| ( iForce & SBO_SB_FORCE_SCOREINFO ) )
		SboScoreboard_WriteScoreInfo( MSG_ALL, NULL, i, iScore, iDeaths, iClass, iTeam );

	s_iSboTeam[i] = iTeam;
	s_iSboScore[i] = iScore;
	s_iSboDeaths[i] = iDeaths;
	s_iSboClass[i] = iClass;

	// coop XP: on change (XP is gained/lost in the monsters' TakeDamage, coop death, InitHUD; the per-frame check
	// catches all of them)
	if ( m_Mode == MODE_COOP && pPlayer->m_iSboTotalXP != s_iSboXP[i] )
	{
		s_iSboXP[i] = pPlayer->m_iSboTotalXP;
		SboScoreboard_WriteXP( MSG_ALL, NULL, i, s_iSboXP[i] );
	}
}

// Replaces the game's "MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo ) ..." blocks (score changes after kills, heals, ...).
void SboScoreboard_SendScoreInfo( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	int i = pPlayer->entindex();

	if ( !SboScoreboard_Active() )
	{
		// teamplay: the original message
		SboScoreboard_WriteScoreInfo( MSG_ALL, NULL, i, (int)pPlayer->pev->frags, pPlayer->m_iDeaths, 0,
			g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
		return;
	}

	if ( i >= 1 && i <= SBO_SB_MAXPLAYERS && s_bSboInit[i] )
		SboScoreboard_Update( pPlayer, SBO_SB_FORCE_SCOREINFO );
	else
		SboScoreboard_WriteScoreInfo( MSG_ALL, NULL, i, SboScoreboard_GetScore( pPlayer ), pPlayer->m_iDeaths,
			SboScoreboard_GetClass( pPlayer ),
			SboScoreboard_GetTeam( pPlayer ) );
}

void SboScoreboard_ClientPutInServer( CBasePlayer *pPlayer )
{
	int i = pPlayer ? pPlayer->entindex() : 0;
	if ( i >= 1 && i <= SBO_SB_MAXPLAYERS )
		SboScoreboard_ResetSlot( i );
}

void SboScoreboard_ClientDisconnected( edict_t *pEntity )
{
	int i = ENTINDEX( pEntity );
	if ( i < 1 || i > SBO_SB_MAXPLAYERS )
		return;

	BOOL bWasIn = s_bSboInit[i];
	SboScoreboard_ResetSlot( i );

	if ( !SboScoreboard_Active() || !bWasIn )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pEntity );
	if ( pPlayer )
	{
		pPlayer->pev->team = 0;
		pPlayer->m_szTeamName[0] = 0;
	}

	// clear the scoreboard entry
	SboScoreboard_WriteTeamInfo( MSG_ALL, NULL, i, "" );
}

// Full scoreboard for a client entering the game (CHalfLifeMultiplay::InitHUD).
// The category names and every player's row as last sent, to one client. bSelf: the client's own row too.
static void SboScoreboard_SendRows( CBasePlayer *pPlayer, BOOL bSelf )
{
	int iNew = pPlayer->entindex();
	edict_t *pTo = pPlayer->edict();
	int iGMLabel = 0;

	for ( int i = 1; i <= gpGlobals->maxClients && i <= SBO_SB_MAXPLAYERS; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && s_bSboInit[i] && SboScoreboard_IsGM( plr ) )
			iGMLabel = 1;
	}
	if ( SboScoreboard_IsGM( pPlayer ) )
		iGMLabel = 1;

	SboScoreboard_WriteTeamNames( MSG_ONE, pTo, iGMLabel );

	// everyone else, as last sent
	for ( int j = 1; j <= gpGlobals->maxClients && j <= SBO_SB_MAXPLAYERS; j++ )
	{
		if ( ( j == iNew && !bSelf ) || !s_bSboInit[j] || s_iSboTeam[j] < 0 )
			continue;
		if ( !UTIL_PlayerByIndex( j ) )
			continue;

		SboScoreboard_WriteTeamInfo( MSG_ONE, pTo, j, SboScoreboard_TeamName( s_iSboTeam[j] ) );
		SboScoreboard_WriteScoreInfo( MSG_ONE, pTo, j, s_iSboScore[j], s_iSboDeaths[j], s_iSboClass[j], s_iSboTeam[j] );
		if ( m_Mode == MODE_COOP && s_iSboXP[j] != INT_MIN )
			SboScoreboard_WriteXP( MSG_ONE, pTo, j, s_iSboXP[j] );
	}
}

// "fullupdate" (CHalfLifeMultiplay::ResendHUD): the scoreboard as the others have it, to this client only. Nothing
// is reset or recomputed (ClientInit starts the player's row again: a dead player would be back under "No Loadout").
void SboScoreboard_ClientResend( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !SboScoreboard_Active() )
		return;

	SboScoreboard_SendRows( pPlayer, TRUE );

	// HLTV: isn't on the scoreboard; the one ScoreInfo activates the client's scoreboard (as in ClientInit)
	if ( pPlayer->pev->flags & FL_PROXY )
		SboScoreboard_WriteScoreInfo( MSG_ONE, pPlayer->edict(), pPlayer->entindex(), 0, 0, 0, 0 );
}

void SboScoreboard_ClientInit( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !SboScoreboard_Active() )
		return;

	int iNew = pPlayer->entindex();
	edict_t *pTo = pPlayer->edict();

	SboScoreboard_SendRows( pPlayer, FALSE );

	if ( iNew < 1 || iNew > SBO_SB_MAXPLAYERS )
		return;

	if ( pPlayer->pev->flags & FL_PROXY )
	{
		// HLTV: gets the scoreboard, isn't on it. The one ScoreInfo still activates the client's scoreboard.
		SboScoreboard_WriteScoreInfo( MSG_ONE, pTo, iNew, 0, 0, 0, 0 );
		return;
	}

	// the new player, to everyone (including itself)
	SboScoreboard_ResetSlot( iNew );
	s_bSboInit[iNew] = TRUE;
	SboScoreboard_Update( pPlayer, SBO_SB_FORCE_TEAMINFO | SBO_SB_FORCE_SCOREINFO );
}

// Per-frame check (StartFrame): catches category changes made outside the hooked paths.
void SboScoreboard_Frame( void )
{
	if ( !SboScoreboard_Active() )
		return;

	int nGM = 0;

	for ( int i = 1; i <= gpGlobals->maxClients && i <= SBO_SB_MAXPLAYERS; i++ )
	{
		if ( !s_bSboInit[i] )
			continue;

		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pPlayer )
			continue;

		SboScoreboard_Update( pPlayer, 0 );

		if ( SboScoreboard_IsGM( pPlayer ) )
			nGM++;
	}

	int iGMLabel = nGM > 0 ? 1 : 0;
	if ( iGMLabel != s_iSboGMLabel )
	{
		s_iSboGMLabel = iGMLabel;
		// Nobody to tell while no client is connected (each client gets the state in its ClientInit). The engine
		// also runs StartFrame while it spawns the first map, before a client has received the user message list:
		// a broadcast then only prints "Unknown User Msg".
		if ( !SboScoreboard_IsBK() && CountPlayers() > 0 )
			SboScoreboard_WriteTeamNames( MSG_ALL, NULL, iGMLabel );
	}
}

// Chat name prefix (Host_Say) by scoreboard category, so chat and scoreboard always agree: Dead Ops "*DEAD* ",
// Spectators "*SPEC* ", No Loadout "*NOLOAD* ", Game Master "*GM* "; Alive Ops and Bounty Killer's Hunted
// (in play) none. Bounty Killer has no No Loadout (SboScoreboard_GetTeam): "*DEAD* " there. Teamplay (sp_, no categories): "*DEAD* " for dead/observing players only.
const char *SboScoreboard_ChatPrefix( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return "";

	if ( !SboScoreboard_Active() )
	{
		if ( pPlayer->m_bSpectating )	// SBO 2.0 spectator mode
			return "*SPEC* ";
		if ( !pPlayer->IsAlive() || ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER ) || pPlayer->pev->iuser1 )
			return "*DEAD* ";
		return "";
	}

	SboScoreboard_Update( pPlayer );	// the category the scoreboard shows (broadcast first if it just changed)

	switch ( SboScoreboard_GetTeam( pPlayer ) )
	{
	case 1:	return SboScoreboard_IsBK() ? "" : "*GM* ";	// Bounty Killer: Hunted (in play)
	case 2:	return "*DEAD* ";
	case 3:	return "*NOLOAD* ";
	case 5:	return "*SPEC* ";
	default:	return "";	// 4 Alive Ops, 0 HLTV
	}
}

//=========================================================
// SBO 2.0: round and mission lines of the server log, in the "World triggered" form log parsers (HLstatsX) know from
// other games and in the exact text production's register_rounds plugin printed (docs/ROUNDS.md "Log lines"):
//   World triggered "Round_Start" (round "3" of "7")
// UTIL_LogPrintf puts the "L date - time: " in front and writes to the log file and the console.
//=========================================================
void LogWorldEvent( const char *pszEvent, const char *pszProperties )
{
	UTIL_LogPrintf( "World triggered \"%s\"%s\n", pszEvent, pszProperties );
}

// With the round: (round "N" of "M"), of "infinite" for a mode without a round limit, nothing for round 0
void LogRoundEvent( const char *pszEvent, int iRound, int iRoundLimit )
{
	char szRound[64];

	szRound[0] = 0;
	if ( iRound > 0 )
	{
		if ( iRoundLimit > 0 )
			sprintf( szRound, " (round \"%d\" of \"%d\")", iRound, iRoundLimit );
		else
			sprintf( szRound, " (round \"%d\" of \"infinite\")", iRound );
	}

	LogWorldEvent( pszEvent, szRound );
}

//=========================================================
// SBO 2.0: scoreboard mode info (SboModeInfo_*): the line under the player count, across from the map name.

//   co_ (CCoop):         "Round: current / limit" -- current 0 until the first round starts (CoopGetRoundInfo),
//                        limit as RoundWinner fails the mission (CoopRoundLimit: max_round, mp_easymode).
//   everything else:     mp_fraglimit, which ends bk_/br_ (CDm/CBr) and sp_ (teamplay) maps; the client shows
//                        "Frags left: N" (the limit minus the leading player's / sp_: team's frags, from its own
//                        scores); 0 (no frag limit) hides the line.
// "SboModeInfo" (byte type, short value1, short value2), types SBO_MODEINFO_* (player.h). Sent to a client in InitHUD
// and to everyone when a value changes (StartFrame: round start, cvar changes). Nothing is sent once the game is
// over: clients keep the last values through the end of the mission and intermission until the next map's InitHUD
// (clients joining during intermission get those last values too).
//=========================================================
extern int gmsgSboModeInfo;

static BOOL	s_bModeInfoValid;	// s_iModeInfo holds the values last broadcast
static int	s_iModeInfo[3];		// type, value1, value2

void SboModeInfo_Reset( void )
{
	s_bModeInfoValid = FALSE;
	// a new map: no round yet (mp_round, mp_roundlimit; SboModeInfo_Rules)
	g_engfuncs.pfnCvar_DirectSet( &round_status, "0" );
	g_engfuncs.pfnCvar_DirectSet( &roundlimit_status, "0" );
}

// The server rules mp_round / mp_roundlimit (server browsers), set when a value changes, like mp_timeleft:
//   co_ (CCoop):  the round shown on the scoreboard (CoopGetRoundInfo) and the round limit (CoopRoundLimit)
//   br_ (CBr):    the round in play or the last one (BrGetRound); BR has no round limit: 0
//   others:       0 and 0 (Bounty Killer and sp_ count no rounds)
// mp_roundlimit values: N > 0 = the rounds a mission gets; 0 = no round limit / not a round mode (the browser shows the
// round alone, or "-"); -1 = rounds without end (the browser shows "N / infinity"; for an infinite-rounds option, which
// only has to give iRoundLimit -1 here).
static void SboModeInfo_Rules( void )
{
	int iRound = 0, iRoundLimit = 0;
	if ( m_Mode == MODE_COOP )
		CoopGetRoundInfo( iRound, iRoundLimit );
	else if ( m_Mode == MODE_BR )
		iRound = BrGetRound();

	if ( (int)round_status.value != iRound )
		g_engfuncs.pfnCvar_DirectSet( &round_status, UTIL_VarArgs( "%i", iRound ) );
	if ( (int)roundlimit_status.value != iRoundLimit )
		g_engfuncs.pfnCvar_DirectSet( &roundlimit_status, UTIL_VarArgs( "%i", iRoundLimit ) );
}

static int SboModeInfo_Clamp( int iValue )
{
	return iValue < 0 ? 0 : ( iValue > 32767 ? 32767 : iValue );
}

static void SboModeInfo_Get( int *pInfo )
{
	if ( m_Mode == MODE_COOP )
	{
		int iRound, iRoundLimit;
		CoopGetRoundInfo( iRound, iRoundLimit );

		pInfo[0] = SBO_MODEINFO_ROUNDS;
		pInfo[1] = SboModeInfo_Clamp( iRound );
		pInfo[2] = SboModeInfo_Clamp( iRoundLimit );
		return;
	}

	// the game ends once someone's frags >= mp_fraglimit (a fractional limit needs the next whole frag)
	pInfo[0] = SBO_MODEINFO_FRAGLIMIT;
	pInfo[1] = fraglimit.value > 0 ? SboModeInfo_Clamp( (int)ceil( fraglimit.value ) ) : 0;
	pInfo[2] = 0;
}

static void SboModeInfo_Write( int iDest, edict_t *pTo )
{
	MESSAGE_BEGIN( iDest, gmsgSboModeInfo, NULL, pTo );
		WRITE_BYTE( s_iModeInfo[0] );
		WRITE_SHORT( s_iModeInfo[1] );
		WRITE_SHORT( s_iModeInfo[2] );
	MESSAGE_END();
}

// Per-frame check (StartFrame): broadcast when a value changed. Frozen once the game is over.
void SboModeInfo_Frame( void )
{
	if ( g_fGameOver || !g_pGameRules )
		return;

	SboModeInfo_Rules();

	int info[3];
	SboModeInfo_Get( info );

	if ( s_bModeInfoValid && !memcmp( info, s_iModeInfo, sizeof( info ) ) )
		return;

	memcpy( s_iModeInfo, info, sizeof( info ) );
	s_bModeInfoValid = TRUE;
	if ( CountPlayers() > 0 )	// see SboScoreboard_Frame
		SboModeInfo_Write( MSG_ALL, NULL );
}

// Client entering the game (CHalfLifeMultiplay::InitHUD): the values everyone has.
void SboModeInfo_ClientInit( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	SboModeInfo_Frame();	// bring everyone up to date first (no-op once the game is over)

	if ( !s_bModeInfoValid )
		return;

	SboModeInfo_Write( MSG_ONE, pPlayer->edict() );
}

//=========================================================
// SBO 2.0: spectator timer (RoundTime_*): the time next to the clock in the spectator panel's top bar.
//   mp_timelimit set, not a co_ map:  the time left. CDm::Think (bk_/dm maps), CBr::Think (br_) and
//                                     CHalfLifeTeamplay::Think (sp_) go to intermission once gpGlobals->time
//                                     reaches mp_timelimit * 60.
//   co_ maps, or mp_timelimit 0:      the time this map has been running (gpGlobals->time). CCoop::Think never
//                                     reads mp_timelimit: a mission ends by its outcome or round limit.
//   game over:                        stopped at the last value (0:00 after the time limit) through intermission.
//
// "RoundTime" (5 bytes; the name of Counter-Strike's round timer message, which carries a short of seconds left):
//   LONG   seconds   0 or more
//   BYTE   mode      ROUNDTIME_COUNTDOWN (0): seconds left; the client counts down to 0 and stays there
//                    ROUNDTIME_ELAPSED (1):   seconds elapsed; the client counts up
//                    ROUNDTIME_STOPPED (2):   the client shows the seconds as they are
// The client runs the clock itself from the moment the message arrives, so it is sent to a client in InitHUD and to
// everyone only when the basis changes (StartFrame: mp_timelimit changed, game over), never per second. Anything
// that sends the same message (an AMX Mod X plugin with its own time limit on co_ maps) sets what clients show
// until the next one from here.
//=========================================================
extern int gmsgRoundTime;

static BOOL		s_bRoundTimeValid;	// s_iRoundTimeMode / s_flRoundTimeBasis hold the basis last broadcast
static int		s_iRoundTimeMode;
static float	s_flRoundTimeBasis;	// countdown: end time; elapsed: start time (gpGlobals->time); stopped: seconds

void RoundTime_Reset( void )
{
	s_bRoundTimeValid = FALSE;
}

static void RoundTime_Get( int &iMode, float &flBasis )
{
	// the modes' Think: flTimeLimit = mp_timelimit * 60; flTimeLimit != 0 && gpGlobals->time >= flTimeLimit
	if ( m_Mode != MODE_COOP && timelimit.value != 0 )
	{
		iMode = ROUNDTIME_COUNTDOWN;
		flBasis = timelimit.value * 60;
		return;
	}

	iMode = ROUNDTIME_ELAPSED;
	flBasis = 0;
}

// The seconds the basis stands for right now (to the nearest second).
static int RoundTime_Seconds( int iMode, float flBasis )
{
	float flSeconds = flBasis;

	if ( iMode == ROUNDTIME_COUNTDOWN )
		flSeconds = flBasis - gpGlobals->time;
	else if ( iMode == ROUNDTIME_ELAPSED )
		flSeconds = gpGlobals->time - flBasis;

	if ( flSeconds <= 0 )
		return 0;
	if ( flSeconds >= 2147483000.0f )
		return 2147483000;

	return (int)( flSeconds + 0.5f );
}

static void RoundTime_Write( int iDest, edict_t *pTo )
{
	MESSAGE_BEGIN( iDest, gmsgRoundTime, NULL, pTo );
		WRITE_LONG( RoundTime_Seconds( s_iRoundTimeMode, s_flRoundTimeBasis ) );
		WRITE_BYTE( s_iRoundTimeMode );
	MESSAGE_END();
}

// Per-frame check (StartFrame): broadcast when the basis changed. Game over stops the clock at its last value.
void RoundTime_Frame( void )
{
	if ( !g_pGameRules )
		return;

	int iMode;
	float flBasis;

	if ( g_fGameOver )
	{
		if ( s_bRoundTimeValid && s_iRoundTimeMode == ROUNDTIME_STOPPED )
			return;

		if ( !s_bRoundTimeValid )
			RoundTime_Get( s_iRoundTimeMode, s_flRoundTimeBasis );

		flBasis = RoundTime_Seconds( s_iRoundTimeMode, s_flRoundTimeBasis );
		iMode = ROUNDTIME_STOPPED;
	}
	else
	{
		RoundTime_Get( iMode, flBasis );

		if ( s_bRoundTimeValid && iMode == s_iRoundTimeMode && flBasis == s_flRoundTimeBasis )
			return;
	}

	s_iRoundTimeMode = iMode;
	s_flRoundTimeBasis = flBasis;
	s_bRoundTimeValid = TRUE;
	if ( CountPlayers() > 0 )	// see SboScoreboard_Frame
		RoundTime_Write( MSG_ALL, NULL );
}

// Client entering the game (CHalfLifeMultiplay::InitHUD): the clock as it stands now.
void RoundTime_ClientInit( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	RoundTime_Frame();	// bring everyone up to date first

	if ( !s_bRoundTimeValid )
		return;

	RoundTime_Write( MSG_ONE, pPlayer->edict() );
}

//=========================================================
// chat-vgui2 branch: kill feed NPC names (SboKillfeed_*). Native port of the metamod-syndicate-killfeed plugin,
// limited to its NPC name table: no random names, no tank/laser entity rules, no per-map or killfeed.cfg files, no
// skf_override* commands, no "skf weapon_name" registrations, no proxy player slot (the plugin faked a player in a
// free slot to carry an NPC name in the stock DeathMsg; the "SboDeathMsg" message carries it now).
//
// The plugin's DeathMsg weapon rewrite (display names such as "Croon RC 28", "world" for non-player killers) isn't
// ported: DeathMsg keeps the stock weapon code, so the kill feed shows its d_<code> icon from hud.txt (d_grenade,
// d_satchel, d_tracktrain, ...) or the skull, never the weapon as text.
// mp_killfeed_npc (default 2): "SboDeathMsg" for NPC kills, with the plugin's NPC names:
//   1 = NPCs that kill players and NPCs killed by players; 2 = only NPCs that kill players (the plugin's scope);
//   0 = off. NPC kills NPC isn't reported (the plugin only saw DeathMsg, which is only sent for players).
//
// SboDeathMsg (variable size, MSG_ALL, registered last in LinkUserMessages):
//   BYTE   flags         SBO_KF_REPLACES_DEATHMSG: the stock DeathMsg for this victim follows right after this
//                        message (same frame); clients that show this one skip that DeathMsg's notice
//   BYTE   killer        player index 1..maxclients, or 0 = the NPC named below / no killer
//   STRING killer name   NPC display name ("" for a player or no killer)
//   BYTE   victim        player index, or 0 = the NPC named below
//   STRING victim name   NPC display name ("" for a player)
//   STRING weapon        DeathMsg-style weapon code for the icon (no "d_"; e.g. "rc28", "grenade", "grunt_hunter")
//   STRING weapon name   weapon for the console line ("" = none, no " with ..."); the code, like DeathMsg
// Old clients don't hook it (the engine drops unhooked user messages).
//=========================================================
extern int gmsgSboDeathMsg;
extern entvars_t *g_pevLastInflictor;

typedef struct
{
	const char *pszCode;
	const char *pszDisplay;
} sbo_kf_name_t;

// the plugin's kMonsterMappings: classname without "monster_" (also projectiles and other things that kill players)
static const sbo_kf_name_t s_SboKfNpcNames[] =
{
	{ "apache",				"Apache" },
	{ "osprey",				"Osprey" },
	{ "barney",				"Barney" },
	{ "cyborg",				"Big Cyborg" },
	{ "dark",				"Sectum Streuoum" },
	{ "plasma",				"Big Cyborg" },
	{ "gargantua",			"Gargantua" },
	{ "garg_stomp",			"Gargantua" },
	{ "grunt_hunter",		"Fed" },
	{ "grunt_hunter_repel",	"Fed" },
	{ "dark_grunt",			"Fed" },
	{ "human_grunt",		"Fed" },
	{ "human_assassin",		"Assassin" },
	{ "grunt_repel",		"Grunt" },
	{ "alien_slave",		"Demon Slave" },
	{ "hunter",				"Alien Slave" },
	{ "ichthyosaur",		"Ichthyosaur" },
	{ "imp",				"Imp" },
	{ "imp_hunter",			"Imp" },
	{ "squidspit",			"Imp" },
	{ "imp_baspit",			"Imp" },
	{ "cyberminotor",		"Big Streum" },
	{ "scientist",			"Sectum" },
	{ "turret",				"Auto Turret" },
	{ "miniturret",			"Mini Auto Turret" },
	{ "sentry",				"Sentry Gun" },
	{ "werewolf",			"Werewolf" },
	{ "werewolf_hunter",	"Werewolf" },
	{ "zombie_grunt",		"Zombie Grunt" },
	{ "zombie",				"Zombie" },
	{ "zombie_hunter",		"Zombie" },
	{ "mortar",				"Mortar" },
	{ "snark",				"Pitbull" },
	{ "tripmine",			"Tripmine" },
	{ "satchel",			"Satchel Charge" },
	{ "rpg_rocket",			"Rocket" },
	{ "hvr_rocket",			"Rocket" },
	{ "grenade",			"GRENAAAD!" },
	{ "xen_tree",			"Xen Tree" },
	{ "xen_spore",			"Xen Spore" },
};

static BOOL SboKillfeed_StrEqNoCase( const char *a, const char *b )
{
	for ( ; *a && *b; a++, b++ )
	{
		char ca = *a, cb = *b;
		if ( ca >= 'A' && ca <= 'Z' ) ca += 'a' - 'A';
		if ( cb >= 'A' && cb <= 'Z' ) cb += 'a' - 'A';
		if ( ca != cb )
			return FALSE;
	}
	return *a == *b;
}

// plugin: case-sensitive prefix, only stripped if something follows it; the table lookup ignores case
static const char *SboKillfeed_StripPrefix( const char *psz, const char *pszPrefix )
{
	int iLen = strlen( pszPrefix );
	if ( !strncmp( psz, pszPrefix, iLen ) && psz[iLen] )
		return psz + iLen;
	return psz;
}

static const char *SboKillfeed_Find( const sbo_kf_name_t *pTable, int iCount, const char *pszCode )
{
	int i;
	for ( i = 0; i < iCount; i++ )
	{
		if ( SboKillfeed_StrEqNoCase( pTable[i].pszCode, pszCode ) )
			return pTable[i].pszDisplay;
	}
	return NULL;
}

// The plugin's NPC name for an entity: its classname without "monster_" in the table; an unlisted monster_* NPC gets
// the plugin's prettified fallback ("monster_bullchicken" -> "Bullchicken"). NULL for players and anything else.
static const char *SboKillfeed_NpcName( entvars_t *pev, char *pszBuf, int iBufLen )
{
	const char *pszClass, *pszCode, *pszName;
	int i;

	if ( !pev || ( pev->flags & FL_CLIENT ) || FStringNull( pev->classname ) )
		return NULL;

	pszClass = STRING( pev->classname );
	pszCode = SboKillfeed_StripPrefix( pszClass, "monster_" );
	pszName = SboKillfeed_Find( s_SboKfNpcNames, ARRAYSIZE( s_SboKfNpcNames ), pszCode );
	if ( pszName )
		return pszName;

	if ( pszCode == pszClass )
		return NULL;	// not a monster_*

	strncpy( pszBuf, pszCode, iBufLen - 1 );
	pszBuf[iBufLen - 1] = 0;
	for ( i = 0; pszBuf[i]; i++ )
	{
		if ( pszBuf[i] == '_' )
			pszBuf[i] = ' ';
		else if ( pszBuf[i] >= 'A' && pszBuf[i] <= 'Z' )
			pszBuf[i] += 'a' - 'A';
	}
	if ( pszBuf[0] >= 'a' && pszBuf[0] <= 'z' )
		pszBuf[0] -= 'a' - 'A';
	return pszBuf;
}

// The kill was a headshot (CS 1.6's DeathMsg flag, from m_LastHitGroup as in CS): the killing damage came through a
// trace (bullets, melee: ApplyMultiDamage, weapons.cpp g_pMultiDamageTarget) that hit the victim's head. Damage
// without a trace (explosions, claws, trigger_hurt) leaves m_LastHitGroup at an older hit, so it never counts.
extern CBaseEntity *g_pMultiDamageTarget;

int SboKillfeed_Headshot( CBaseEntity *pVictim )
{
	CBaseMonster *pMonster = pVictim ? pVictim->MyMonsterPointer() : NULL;
	return ( pMonster && g_pMultiDamageTarget == pVictim && pMonster->m_LastHitGroup == HITGROUP_HEAD ) ? 1 : 0;
}

// The DeathMsg weapon code of an inflictor that is not the killer (DeathNotice, SboKillfeed_NpcKilled): its classname
// (stripped of weapon_ / monster_ / func_ by the callers), as in HL. One exception: contact grenades
// (CGrenade::ShootContact / ShootContact2: the grenade launcher, also the MP5's and the grunts' M203) and hand grenades
// (ShootTimed) are both class "grenade"; a contact grenade still has its ExplodeTouch touch function when its blast
// kills, so it is sent as "glauncher" (d_glauncher) and hand grenades stay "grenade" (d_grenade). The classname
// itself is not changed (plugins and maps look for "grenade").
// The plasma bolt (class "plasma") is the SW2's (d_plasma, the plasma sniper's icon) only when a player fired it; a Big
// Cyborg's bolt (CCyborg fires the same CPlasma) is the NPC's own attack: it is sent as its shooter's classname
// ("monster_cyborg" -> "cyborg", no d_ icon: the skull, like the other NPCs' own attacks), or "world" without one.
const char *SboKillfeed_InflictorCode( entvars_t *pevInflictor )
{
	const char *pszClass = STRING( pevInflictor->classname );

	if ( !strcmp( pszClass, "plasma" ) )
	{
		entvars_t *pevOwner = pevInflictor->owner ? VARS( pevInflictor->owner ) : NULL;
		if ( !pevOwner )
			return "world";
		if ( !( pevOwner->flags & FL_CLIENT ) )
			return FStringNull( pevOwner->classname ) ? "world" : STRING( pevOwner->classname );
		return pszClass;
	}

	if ( !strcmp( pszClass, "grenade" ) )
	{
		CBaseEntity *pInflictor = CBaseEntity::Instance( pevInflictor );
		if ( pInflictor && pInflictor->m_pfnTouch
			== static_cast <void (CBaseEntity::*)(CBaseEntity *)> ( &CGrenade::ExplodeTouch ) )
			return "glauncher";
	}
	return pszClass;
}

static void SboKillfeed_Send( int iFlags, int iKiller, const char *pszKiller, int iVictim, const char *pszVictim,
	const char *pszWeapon, const char *pszWeaponName, int iHeadshot, int iKillerOwner = 0, int iVictimOwner = 0 )
{
	if ( !gmsgSboDeathMsg )
		return;

	MESSAGE_BEGIN( MSG_ALL, gmsgSboDeathMsg );
		WRITE_BYTE( iFlags );
		WRITE_BYTE( iKiller );
		WRITE_STRING( pszKiller );
		WRITE_BYTE( iVictim );
		WRITE_STRING( pszVictim );
		WRITE_STRING( pszWeapon );
		WRITE_STRING( pszWeaponName );
		WRITE_BYTE( iHeadshot );	// SBO 2.0: headshot (the client reads it only if present)
		WRITE_BYTE( iKillerOwner );	// SBO 2.0 GM mode: the Game Master whose monster the killer NPC is ("<GM>'s <monster>"), 0 none
		WRITE_BYTE( iVictimOwner );	// SBO 2.0 GM mode: the same for the victim NPC
	MESSAGE_END();
}

// The NPC name of what killed a player, for the kill feed and the log line (CHalfLifeMultiplay::DeathNotice); NULL if
// the killer is a player or has no NPC name (the world, a trigger_hurt, a door, ...).
// The killer (attacker) if it's an NPC: a grunt's grenade -> "Fed". Else the inflictor, the plugin's only source (the
// DeathMsg weapon is the inflictor's classname): an ownerless grenade -> "GRENAAAD!".
const char *SboKillfeed_KillerNpcName( entvars_t *pKiller, entvars_t *pevInflictor, char *pszBuf, int iBufLen )
{
	const char *pszNpc;

	if ( pKiller->flags & FL_CLIENT )
		return NULL;

	// SBO 2.0 GM mode: a Game Master's monster (or what it fired) is "<GM>'s <monster>" (MrCalvin's GM mode plugin)
	pszNpc = GmMode_KillerName( pKiller, pevInflictor, pszBuf, iBufLen );
	if ( pszNpc )
		return pszNpc;

	pszNpc = SboKillfeed_NpcName( pKiller, pszBuf, iBufLen );
	if ( !pszNpc && pevInflictor && pevInflictor != pKiller )
		pszNpc = SboKillfeed_NpcName( pevInflictor, pszBuf, iBufLen );
	return pszNpc;
}

// CHalfLifeMultiplay::DeathNotice, right before the DeathMsg: sends the SboDeathMsg for an NPC that killed the player
// (pszWeapon: the DeathMsg weapon code, already without weapon_/monster_/func_).
void SboKillfeed_PlayerDeath( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor, const char *pszWeapon,
	int iHeadshot )
{
	if ( pKiller->flags & FL_CLIENT )
		return;

	if ( mp_killfeed_npc.value != 0 )
	{
		char szBuf[96];
		const char *pszNpc = SboKillfeed_KillerNpcName( pKiller, pevInflictor, szBuf, sizeof( szBuf ) );

		if ( pszNpc )
			SboKillfeed_Send( SBO_KF_REPLACES_DEATHMSG, 0, pszNpc, ENTINDEX( pVictim->edict() ), "", pszWeapon, "",
				iHeadshot, GmMode_KillerOwner( pKiller, pevInflictor ) );
	}
}

// CBaseMonster::Killed (first call), CApache::Killed, COsprey::Killed, the turrets' death (CBaseTurret / CSentry
// TakeDamage): a player killed an NPC with a kill feed name (SboKillfeed_NpcName: a monster_* class, or a table entry
// such as dark_grunt, whose class has no monster_ prefix).
// The weapon is worked out like DeathNotice does for players (g_pevLastInflictor: CBaseMonster::TakeDamage and
// CBaseEntity::TakeDamage set it around Killed; none -> the attacker's current weapon).
void SboKillfeed_NpcKilled( CBaseEntity *pVictim, entvars_t *pevAttacker )
{
	char szBuf[64];
	const char *pszName, *pszWeapon;
	entvars_t *pevInflictor;

	if ( mp_killfeed_npc.value != 1 || !g_pGameRules || !g_pGameRules->IsMultiplayer() )
		return;
	if ( !pVictim || pVictim->IsPlayer() || !( pVictim->pev->flags & FL_MONSTER ) )
		return;
	if ( !pevAttacker || !( pevAttacker->flags & FL_CLIENT ) || pevAttacker == pVictim->pev )
		return;

	// SBO 2.0 GM mode: a Game Master's monster is "<GM>'s <monster>" as a victim too
	int iVictimOwner = GmMode_KillerOwner( pVictim->pev, NULL );
	pszName = iVictimOwner ? GmMode_KillerName( pVictim->pev, NULL, szBuf, sizeof( szBuf ) ) : NULL;
	if ( !pszName )
	{
		iVictimOwner = 0;
		pszName = SboKillfeed_NpcName( pVictim->pev, szBuf, sizeof( szBuf ) );
	}
	if ( !pszName )
		return;

	pszWeapon = "world";
	pevInflictor = g_pevLastInflictor;
	if ( !pevInflictor || pevInflictor == pevAttacker )
	{
		CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );
		if ( pAttacker && pAttacker->IsPlayer() && ( (CBasePlayer *)pAttacker )->m_pActiveItem )
			pszWeapon = ( (CBasePlayer *)pAttacker )->m_pActiveItem->pszName();
	}
	else if ( !FStringNull( pevInflictor->classname ) )
		pszWeapon = SboKillfeed_InflictorCode( pevInflictor );

	if ( strncmp( pszWeapon, "weapon_", 7 ) == 0 )
		pszWeapon += 7;
	else if ( strncmp( pszWeapon, "monster_", 8 ) == 0 )
		pszWeapon += 8;
	else if ( strncmp( pszWeapon, "func_", 5 ) == 0 )
		pszWeapon += 5;

	SboKillfeed_Send( 0, ENTINDEX( ENT( pevAttacker ) ), "", 0, pszName, pszWeapon,
		!strcmp( pszWeapon, "world" ) ? "" : pszWeapon, SboKillfeed_Headshot( pVictim ), 0, iVictimOwner );
}
