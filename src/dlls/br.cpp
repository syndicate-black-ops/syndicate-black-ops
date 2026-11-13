//=========================================================
// SBO: battle royale game rules (br_ maps)
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include	"skill.h"
#include	"game.h"
#include	"items.h"
#include	"voice_gamemgr.h"
#include	"br.h"
#include	"semiclip.h"
#include	"round_hooks.h"

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgScoreInfo;
extern CVoiceGameMgr	g_VoiceGameMgr;

extern int g_bChangeLevelTouched;
extern int m_SpawnLocate;
extern string_t g_iszSpawnSet;	// maprules.cpp: the named spawn set multispawn made active
extern cvar_t timeleft, fragsleft;

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );
extern void SpectatorStart_Joining( CBasePlayer *pPlayer );
extern void respawn( entvars_t *pev, BOOL fCopyCorpse );

// SBO 2.0 log lines (LogRoundEvent, multiplay_gamerules.cpp; docs/ROUNDS.md "Log lines"). BR has no round limit (the
// frag or time limit ends the map), so its rounds are counted here, for the log only: a round is one CheckWho that
// put at least two players in play.
static int s_iRound;				// rounds started on this map
static BOOL s_bRoundLogged;			// "Round_Start" written, its "Round_End" not yet
static BOOL s_bMapWinnerLogged;		// "BR_Map_Winner" written

// Rounds won on this map, by player slot. The name and SteamID are kept from the win (the winner may be gone when
// the map ends), the user id tells a new player in the slot from the one who won.
#define BR_MAX_PLAYERS		32
#define BR_WINNER_NAME		32
#define BR_WINNER_AUTHID	64
static struct
{
	int		iWins;
	int		iUserId;
	char	szName[BR_WINNER_NAME];
	char	szAuthId[BR_WINNER_AUTHID];
} s_Winners[BR_MAX_PLAYERS + 1];

// SBO 2.0: the round in play or the last one played, 0 before the first (the server rule mp_round, SboModeInfo_Rules)
int BrGetRound( void )
{
	return s_iRound;
}

static void BrLogRoundStart( void )
{
	s_iRound++;
	s_bRoundLogged = TRUE;
	LogRoundEvent( "Round_Start", s_iRound, 0 );
	RoundHooks_Start( s_iRound );	// SBO 2.0: game_round_start and game_round_<N>, the round's players spawned
}

static void BrLogRoundEnd( void )
{
	if ( !s_bRoundLogged )
		return;

	s_bRoundLogged = FALSE;
	LogRoundEvent( "Round_End", s_iRound, 0 );
	RoundHooks_End();	// SBO 2.0: game_round_end, once per round as the log line
}

// The round's last survivor: World triggered "BR_Round_Winner" (round "N", winner "name", authid "id", wins "W"),
// or winner "none" when nobody or more than one player is left
static void BrLogRoundWinner( void )
{
	if ( !s_bRoundLogged )
		return;

	CBasePlayer *pWinner = NULL;
	int iAlive = 0;

	for ( int i = 1; i <= gpGlobals->maxClients && i <= BR_MAX_PLAYERS; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		// the round's players still in play (those RoundWinner gives its 3 frags to); never spectators
		if ( !IsRoundCandidate( pPlayer ) || !pPlayer->m_bRoundParticipant || pPlayer->m_bOutOfPlay )
			continue;

		pWinner = pPlayer;
		iAlive++;
	}

	if ( iAlive != 1 )
	{
		LogWorldEvent( "BR_Round_Winner", UTIL_VarArgs( " (round \"%d\", winner \"none\")", s_iRound ) );
		return;
	}

	int iSlot = pWinner->entindex();
	int iUserId = GETPLAYERUSERID( pWinner->edict() );

	if ( s_Winners[iSlot].iUserId != iUserId )
	{
		s_Winners[iSlot].iWins = 0;
		s_Winners[iSlot].iUserId = iUserId;
	}
	s_Winners[iSlot].iWins++;
	pWinner->m_iBrRoundWins = s_Winners[iSlot].iWins;	// the same count on the player, for spectators (SpecHealth2)
	strncpy( s_Winners[iSlot].szName, STRING( pWinner->pev->netname ), BR_WINNER_NAME - 1 );
	s_Winners[iSlot].szName[BR_WINNER_NAME - 1] = 0;
	strncpy( s_Winners[iSlot].szAuthId, GETPLAYERAUTHID( pWinner->edict() ), BR_WINNER_AUTHID - 1 );
	s_Winners[iSlot].szAuthId[BR_WINNER_AUTHID - 1] = 0;

	LogWorldEvent( "BR_Round_Winner", UTIL_VarArgs( " (round \"%d\", winner \"%s\", authid \"%s\", wins \"%d\")",
		s_iRound, s_Winners[iSlot].szName, s_Winners[iSlot].szAuthId, s_Winners[iSlot].iWins ) );
}

// The map is over (frag or time limit): the player with the most rounds won.
//   World triggered "BR_Map_Winner" (winner "name", authid "id", wins "W", rounds "N")
//   ... (winner "draw", wins "W", rounds "N") when several players share the most wins
//   ... (winner "none", rounds "N") when no round had a winner
static void BrLogMapWinner( void )
{
	if ( s_bMapWinnerLogged )
		return;

	s_bMapWinnerLogged = TRUE;

	BrLogRoundEnd();	// a round the limit cut short

	int iWinner = 0;
	int iMostWins = 0;
	BOOL fDraw = FALSE;

	for ( int i = 1; i <= BR_MAX_PLAYERS; i++ )
	{
		if ( s_Winners[i].iWins <= 0 )
			continue;

		if ( s_Winners[i].iWins > iMostWins )
		{
			iMostWins = s_Winners[i].iWins;
			iWinner = i;
			fDraw = FALSE;
		}
		else if ( s_Winners[i].iWins == iMostWins )
		{
			fDraw = TRUE;
		}
	}

	if ( !iWinner )
		LogWorldEvent( "BR_Map_Winner", UTIL_VarArgs( " (winner \"none\", rounds \"%d\")", s_iRound ) );
	else if ( fDraw )
		LogWorldEvent( "BR_Map_Winner", UTIL_VarArgs( " (winner \"draw\", wins \"%d\", rounds \"%d\")", iMostWins, s_iRound ) );
	else
		LogWorldEvent( "BR_Map_Winner", UTIL_VarArgs( " (winner \"%s\", authid \"%s\", wins \"%d\", rounds \"%d\")",
			s_Winners[iWinner].szName, s_Winners[iWinner].szAuthId, iMostWins, s_iRound ) );
}

//=========================================================
//=========================================================
CBr::CBr()
{
	g_bChangeLevelTouched = 0;
	m_iRoundState = ROUND_PAUSE;
	m_iNextRoundStep = -1;
	m_SpawnLocate = 1;
	g_iszSpawnSet = iStringNull;	// SBO 2.1: no named spawn set, with the rotation
	s_iRound = 0;
	s_bRoundLogged = FALSE;
	s_bMapWinnerLogged = FALSE;
	memset( s_Winners, 0, sizeof( s_Winners ) );
}

//=========================================================
//=========================================================
void CBr::Think( void )
{
	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining;

	g_VoiceGameMgr.Update(gpGlobals->frametime);

	// halflife SDK (CHalfLifeMultiplay::Think): once the game is over, only the intermission runs. SBO ran the round
	// states first, so after the frag/time limit a new round respawned everyone and RoundWinner gave +3 frags during
	// intermission.
	if ( g_fGameOver )   // someone else quit the game already
	{
		CHalfLifeMultiplay::Think();
		return;
	}

	RoundStatus_Update( m_iRoundState, m_iNextRoundStep, 2 );	// SBO 2.0: the loadout menu's NEXT ROUND IN (a round needs two)

	if ( m_iRoundState == ROUND_PAUSE || m_iRoundState == ROUND_WAITING )
	{
		if ( m_iNextRoundStep < gpGlobals->time )
			CheckRound();
	}

	if ( m_iRoundState == ROUND_SETUP )
		SetupRound();

	if ( m_iRoundState == ROUND_ACTIVE )
	{
		// SBO 2.0: counted from the players (CountRoundPlayers, coop.cpp); SBO kept counters (m_iParticip, m_iLive)
		int iPlayers, iAlive;
		CountRoundPlayers( iPlayers, iAlive );

		if ( iPlayers < 2 )
		{
			BrLogRoundEnd();	// the round's players left: over without a winner
			m_iNextRoundStep = gpGlobals->time + 10;
			m_iRoundState = ROUND_PAUSE;
			return;
		}

		if ( iAlive <= 1 )
		{
			m_iNextRoundStep = 0;
			m_iRoundState = ROUND_OVER;
			return;
		}
	}

	if ( m_iRoundState == ROUND_OVER )
		RoundWinner();

	float flTimeLimit = timelimit.value * 60;
	float flFragLimit = fraglimit.value;

	time_remaining = (int)(flTimeLimit ? ( flTimeLimit - gpGlobals->time ) : 0);
	
	if ( flTimeLimit != 0 && gpGlobals->time >= flTimeLimit )
	{
		BrLogMapWinner();
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
				BrLogMapWinner();
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
int CBr::IsBr( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CBr::InitHUD( CBasePlayer *pPlayer )
{
	pPlayer->m_bNotInGame = 0;
	pPlayer->m_bRoundParticipant = FALSE;	// SBO 2.0: joins the next round
	pPlayer->m_flLoadoutWaitStart = gpGlobals->time;	// SBO 2.0: mp_loadouttime counts from here (the go menu)
	g_bChangeLevelTouched = 0;

	pPlayer->pev->classname = MAKE_STRING( "observer" );
	pPlayer->pev->solid = SOLID_NOT;
	pPlayer->pev->takedamage = DAMAGE_NO;
	pPlayer->pev->movetype = MOVETYPE_NONE;
	pPlayer->pev->flags |= FL_NOTARGET;
	pPlayer->pev->effects |= EF_NODRAW;
	pPlayer->m_afPhysicsFlags |= PFLAG_OBSERVER;
	pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT | HIDEHUD_HEALTH;
	pPlayer->RemoveAllItems( TRUE );
	pPlayer->m_bLoadoutReady = 0;
	SpectatorStart_Joining( pPlayer );	// SBO 2.0: nobody to watch: the view from an info_intermission point (observer.cpp)

	// SBO 2.0: no go menu for a spectator (spectator mode kept from the last map: UpdateClientData starts it next)
	if ( ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER ) && !pPlayer->m_bSpectating )
	{
		pPlayer->m_iTextMenu = 1;
		pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Br" );
	}

	CHalfLifeMultiplay::InitHUD( pPlayer );
	RoundStatus_Resend();	// SBO 2.0: the round status again for everybody, this player included
}

//=========================================================
//=========================================================
void CBr::ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		if ( pPlayer )
		{
			PlayerLeavesRound( pPlayer );
			pPlayer->m_bNotInGame = 1;
		}
	}

	CHalfLifeMultiplay::ClientDisconnected( pClient );
}

//=========================================================
// SBO 2.0: a player leaves the round (disconnect); see CCoop::PlayerLeavesRound. SBO took one off its player counter
// for everyone, also players the round never counted (joined mid-round, still at the go menu).
//=========================================================
void CBr::PlayerLeavesRound( CBasePlayer *pPlayer )
{
	BOOL bParticipant = pPlayer->m_bRoundParticipant;

	pPlayer->m_bRoundParticipant = FALSE;	// out of the counts from here on

	if ( m_iRoundState == ROUND_ACTIVE )
	{
		if ( bParticipant )
			CheckRoundOver();
		return;
	}

	// SBO called CheckRound in every other state: a disconnect in state 1 skipped the round's RoundWinner, and one in
	// the pause after a round cut it short.
	if ( m_iRoundState == ROUND_WAITING )
		CheckRound();
}

//=========================================================
// SBO 2.0 spectator mode on (CBasePlayer::StartSpectating): leave the round, close the go menu
//=========================================================
void CBr::PlayerStartsSpectating( CBasePlayer *pPlayer )
{
	PlayerLeavesRound( pPlayer );

	if ( pPlayer->m_iTextMenu == 1 )
	{
		pPlayer->m_iTextMenu = TEXTMENU_NONE;
		pPlayer->ShowMenu( 0, 0, FALSE, "" );	// no valid slots: the client closes the menu
	}
}

//=========================================================
// SBO 2.0 spectator mode off (CBasePlayer::StopSpectating): like a late joiner. A player who had pressed go joins the
// next round (a round waiting for players is checked again at once); else the go menu opens and mp_loadouttime
// counts from now. The player keeps watching until then.
//=========================================================
void CBr::PlayerStopsSpectating( CBasePlayer *pPlayer )
{
	if ( pPlayer->m_bLoadoutReady == 1 )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, "You left spectator mode: you join the next round.\n" );

		if ( m_iRoundState == ROUND_WAITING )
			CheckRound();
	}
	else
	{
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, "You left spectator mode: press a key in the menu to join the next round.\n" );
		pPlayer->m_flLoadoutWaitStart = gpGlobals->time;
		pPlayer->m_iTextMenu = 1;
		pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Br" );
	}
}

//=========================================================
//=========================================================
void CBr::RoundWinner( void )
{
	BrLogRoundWinner();
	BrLogRoundEnd();

	UTIL_ClientPrintAll( HUD_PRINTCENTER, "ROUND OVER\n" );

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer && !pPlayer->m_bNotInGame && !pPlayer->m_bOutOfPlay )
		{
			pPlayer->pev->frags += 3;

			SboScoreboard_SendScoreInfo( pPlayer );	// chat-vgui2: scoreboard team + level
		}
	}

	m_iNextRoundStep = gpGlobals->time + 5;
	m_iRoundState = ROUND_PAUSE;
}

//=========================================================
//=========================================================
void CBr::CheckRound( void )
{
	m_iRoundState = ROUND_SETUP;
	m_iNextRoundStep = 0;
}

//=========================================================
//=========================================================
void CBr::SetupRound( void )
{
	int iReady = 0;		// players who pressed go
	int iWaitFor = 0;	// players still at the go menu the round waits for (mp_loadouttime)
	float flRetry = gpGlobals->time + 10;

	// SBO 2.0: only counts; CheckWho strips the players it spawns (SBO stripped everyone here, also while waiting).
	// SBO waited for every player still at the go menu forever; with mp_loadouttime only for a player who got in the
	// game less than that long ago (LoadoutWaitBlocksRound, coop.cpp; production's loadout_autoready plugin left out
	// BR players idle at the go menu after 30 s).
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !IsRoundCandidate( pPlayer ) )	// SBO 2.0: also skips HLTV proxies
			continue;

		if ( pPlayer->m_bLoadoutReady == 1 )
		{
			iReady++;
		}
		else
		{
			float flUntil = flRetry;

			if ( LoadoutWaitBlocksRound( pPlayer, &flUntil ) )
			{
				iWaitFor++;
				if ( flUntil < flRetry )
					flRetry = flUntil;	// check again when the wait for this player ends
			}
		}
	}

	if ( iWaitFor > 0 )
	{
		m_iNextRoundStep = (int)flRetry + 1;	// every 10 s (SBO), sooner when a player's mp_loadouttime runs out
		m_iRoundState = ROUND_WAITING;
	}
	else if ( iReady < 2 )
	{
		// SBO 2.0: a round needs two players. SBO started it anyway (CheckWho), Think took it back the next frame
		// for want of players and tried again 10 s later: a player alone on the map was stripped and respawned with
		// another weapon, to "ROUND START", every 10 seconds. Now the round waits; meanwhile the player may run
		// around the map, as SBO let them between its respawns (WaitingSpawn).
		WaitingSpawn();
		m_iNextRoundStep = (int)flRetry + 1;	// every 10 s (SBO); at once when a player presses go, joins or leaves
		m_iRoundState = ROUND_WAITING;
	}
	else
	{
		CheckWho();
	}
}

//=========================================================
// SBO 2.0: no round yet (fewer than two players pressed go): a ready player who isn't in play (just pressed go, or
// died while waiting) is spawned as a round spawns its players, but only once: whoever is alive is left alone, with
// the weapon they have. Not a round: no "ROUND START", no round in the log, no round participants (nothing to win,
// no +3 frags); the round that starts when a second player is ready strips and spawns everybody as usual.
//=========================================================
void CBr::WaitingSpawn( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !IsRoundCandidate( pPlayer ) || pPlayer->m_bLoadoutReady != 1 )
			continue;

		pPlayer->m_bRoundParticipant = FALSE;

		if ( !pPlayer->m_bOutOfPlay )
			continue;	// alive on the map already

		if ( pPlayer->HasAnyWeaponBits() )
			pPlayer->RemoveAllItems( TRUE );

		pPlayer->m_iSpawnMode = 2;
		EntSelectSpawnPoint( pPlayer );
		pPlayer->StopObserver();
		pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
		pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
		pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
		pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
		pPlayer->m_bOutOfPlay = 0;
		pPlayer->Spawn();
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "Waiting for players...\n" );
	}
}

//=========================================================
//=========================================================
void CBr::CheckWho( void )
{
	int iSpawned = 0;

	// SBO 2.0 round hooks: the last round's game_round_end first (if not fired yet), then the map's "round_reset"
	// entities go back to their state before anyone spawns (spawn points fire their targets at the spawn).
	// SetupRound only gets here with two players or more: this is a round.
	BrLogRoundEnd();
	RoundHooks_Reset();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		// SBO 2.0: the round's participants are the players it spawns (m_bRoundParticipant). SBO also counted and
		// stripped the players still at the go menu; with mp_loadouttime the round can start without them, and they
		// are left alone.
		pPlayer->m_bRoundParticipant = FALSE;

		if ( IsRoundCandidate( pPlayer ) && pPlayer->m_bLoadoutReady == 1 )
		{
			if ( pPlayer->HasAnyWeaponBits() )
				pPlayer->RemoveAllItems( TRUE );

			iSpawned++;
			pPlayer->m_bRoundParticipant = TRUE;
			pPlayer->m_iSpawnMode = 2;
			EntSelectSpawnPoint( pPlayer );
			pPlayer->StopObserver();
			pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
			pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
			pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
			pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
			pPlayer->m_bOutOfPlay = 0;
			UTIL_ClientPrintAll( HUD_PRINTCENTER, "ROUND START\n" );
			pPlayer->Spawn();
		}
	}

	if ( iSpawned )
		Semiclip_RoundStart();	// SBO 2.0 semiclip: mp_semiclip_time runs from "ROUND START"

	m_iNextRoundStep = gpGlobals->time + 0;
	m_iRoundState = ROUND_ACTIVE;

	// log: a round needs two players (SetupRound doesn't get here with fewer)
	BrLogRoundEnd();
	if ( iSpawned >= 2 )
		BrLogRoundStart();
}


//=========================================================
//=========================================================
void CBr::CheckRoundOver( void )
{
	int iPlayers, iAlive;
	CountRoundPlayers( iPlayers, iAlive );

	if ( iPlayers >= 2 && iAlive <= 1 )
	{
		m_iNextRoundStep = 0;
		m_iRoundState = ROUND_OVER;
	}
}

//=========================================================
//=========================================================
BOOL CBr::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if ( g_VoiceGameMgr.ClientCommand( pPlayer, pcmd ) )
		return TRUE;

	if ( FStrEq( pcmd, "menuselect" ) )
	{
		if ( CMD_ARGC() < 2 )
			return TRUE;

		int slot = atoi( CMD_ARGV(1) );

		// a key picks in the game's text menu on screen only, once; any other "menuselect" answers a plugin's menu
		int iTextMenu = pPlayer->m_iTextMenu;
		pPlayer->m_iTextMenu = TEXTMENU_NONE;

		switch ( iTextMenu )
		{
		case 1:
			switch ( slot )
			{
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
				if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
				{
					pPlayer->m_iSpawnMode = 1;
					pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
					pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
					pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
					pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
					pPlayer->m_bOutOfPlay = 1;
					pPlayer->m_bLoadoutReady = 1;
					respawn( pPlayer->pev, FALSE );

					// SBO 2.0: maybe the second player a waiting round needs (SBO: up to 10 s later)
					if ( m_iRoundState == ROUND_WAITING )
						CheckRound();
				}
				break;
			}
			break;
		}

		return TRUE;
	}

	return FALSE;
}

//=========================================================
//=========================================================
void CBr::PlayerSpawn( CBasePlayer *pPlayer )
{
	// halflife-updated: the round loadout always auto-selects, regardless of cl_autowepswitch
	const int originalAutoWepSwitch = pPlayer->m_iAutoWepSwitch;
	pPlayer->m_iAutoWepSwitch = 1;

	BOOL		addDefault;
	CBaseEntity	*pWeaponEntity = NULL;

	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);
	
	addDefault = TRUE;

	while ( pWeaponEntity = UTIL_FindEntityByClassname( pWeaponEntity, "game_player_equip" ))
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = FALSE;
	}

	if ( pPlayer->m_iSpawnMode == 0 )
	{
		pPlayer->StopObserver();
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "Waiting for a carnage..." );
		pPlayer->pev->classname = MAKE_STRING( "observer" );
		pPlayer->pev->solid = SOLID_NOT;
		pPlayer->pev->takedamage = DAMAGE_NO;
		pPlayer->pev->movetype = MOVETYPE_NONE;
		pPlayer->pev->flags |= FL_NOTARGET;
		pPlayer->pev->effects |= EF_NODRAW;
		pPlayer->m_afPhysicsFlags |= PFLAG_OBSERVER;
		pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT | HIDEHUD_HEALTH;
		pPlayer->m_bOutOfPlay = 1;
		addDefault = FALSE;
	}
	else if ( pPlayer->m_iSpawnMode == 1 )
	{
		pPlayer->m_bOutOfPlay = 1;
		edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );
		pPlayer->StartObserver( VARS(pentSpawnSpot)->origin, VARS(pentSpawnSpot)->angles );
	}
	else
	{
		pPlayer->StopObserver();
		pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
		pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
		pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
		pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
		pPlayer->m_bOutOfPlay = 0;
		// SBO 2.0: walking only for a player who spawns alive (HL SDK / ReGameDLL): an observer keeps StartObserver's
		// MOVETYPE_NONE and a waiting player the branch's, which the engine never pushes (SV_PushRotate turns a pushed
		// client's view with a rotating brush)
		pPlayer->pev->movetype = MOVETYPE_WALK;
	}

	if ( addDefault )
		pPlayer->GiveNamedItem( "weapon_wong" );

	pPlayer->m_iSpawnProtectTime = gpGlobals->time + 2;
	pPlayer->pev->fov = pPlayer->m_iFOV = 0;	// SBO 2.0: the player's own FOV (default_fov), not 90
	pPlayer->m_iArmorType = 0;
	pPlayer->m_iCanCloak = 0;
	pPlayer->m_iCanCybereye = 0;
	pPlayer->m_bTargeting = 0;
	pPlayer->pev->renderamt = 255;
	pPlayer->pev->renderfx = kRenderFxNone;
	pPlayer->pev->rendermode = kRenderNormal;
	pPlayer->pev->gravity = 1;
	pPlayer->m_iSboMaxSpeed = pPlayer->m_iSboSpeed;

	pPlayer->m_iBrModelRoll = RANDOM_LONG( 1, 2 );
	switch ( pPlayer->m_iBrModelRoll )
	{
	case 1:
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agentf" );
		break;
	case 2:
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agent" );
		break;
	}

	pPlayer->m_iBrBonusRoll = RANDOM_LONG( 1, 40 );
	switch ( pPlayer->m_iBrBonusRoll )
	{
	case 1:
		pPlayer->GiveNamedItem( "weapon_handgrenade" );
		break;
	case 2:
		pPlayer->GiveNamedItem( "weapon_tripmine" );
		break;
	case 3:
		pPlayer->GiveNamedItem( "weapon_satchel" );
		break;
	case 4:
		pPlayer->pev->fov = pPlayer->m_iFOV = 0;	// SBO 2.0: the player's own FOV (default_fov), not 90
		g_engfuncs.pfnSetClientMaxspeed( pPlayer->edict(), 500 );
		pPlayer->pev->armorvalue = 150;
		pPlayer->m_iArmorType = 2;
		break;
	case 5:
	case 6:
		pPlayer->GiveNamedItem( "weapon_warhammer" );
		break;
	case 7:
	case 8:
		pPlayer->GiveNamedItem( "weapon_chainsaw" );
		break;
	case 9:
	case 10:
		pPlayer->GiveNamedItem( "weapon_sword" );
		break;
	case 11:
	case 12:
	case 13:
		pPlayer->GiveNamedItem( "weapon_rc28" );
		break;
	case 14:
	case 15:
		pPlayer->GiveNamedItem( "weapon_k444" );
		break;
	case 16:
	case 17:
	case 18:
		pPlayer->GiveNamedItem( "weapon_canon" );
		break;
	case 19:
	case 20:
	case 21:
		pPlayer->GiveNamedItem( "weapon_pm66" );
		break;
	case 22:
	case 23:
		pPlayer->GiveNamedItem( "weapon_fa18" );
		break;
	case 24:
	case 25:
	case 26:
		pPlayer->GiveNamedItem( "weapon_spa" );
		break;
	case 27:
	case 28:
		pPlayer->GiveNamedItem( "weapon_sw" );
		break;
	case 29:
		pPlayer->GiveNamedItem( "weapon_glauncher" );
		break;
	case 30:
		pPlayer->GiveNamedItem( "weapon_chaingun" );
		break;
	case 31:
		pPlayer->GiveNamedItem( "weapon_rpg" );
		break;
	case 32:
	case 33:
		pPlayer->GiveNamedItem( "weapon_nm33" );
		break;
	case 34:
	case 35:
		pPlayer->GiveNamedItem( "weapon_federal" );
		break;
	case 36:
	case 37:
		pPlayer->GiveNamedItem( "weapon_ben" );
		break;
	case 38:
		pPlayer->GiveNamedItem( "weapon_medikit" );
		break;
	case 39:
	case 40:
		pPlayer->GiveNamedItem( "weapon_snark" );
		break;
	}

	pPlayer->m_iAutoWepSwitch = originalAutoWepSwitch;
}

//=========================================================
//=========================================================
BOOL CBr::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	pPlayer->m_bOutOfPlay = 1;

	edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );
	pPlayer->StartObserver( VARS(pentSpawnSpot)->origin, VARS(pentSpawnSpot)->angles );

	return FALSE;
}

//=========================================================
//=========================================================
int CBr::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	// 5 for the kill that leaves one player alive or fewer, and outside a round in play (SBO's live counter was 0
	// then). PlayerKilled has taken the victim out of play and checked the round before this is called.
	if ( m_iRoundState != ROUND_ACTIVE )
		return 5;

	int iPlayers, iAlive;
	CountRoundPlayers( iPlayers, iAlive );

	if ( iAlive <= 1 )
		return 5;

	return 1;
}

//=========================================================
//=========================================================
void CBr::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	if ( m_iRoundState == ROUND_ACTIVE )
	{
		// out of play before the round is checked: CheckRoundOver counts the players still alive
		pVictim->m_iSpawnMode = 1;
		pVictim->m_bOutOfPlay = 1;
		CheckRoundOver();
	}

	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );
}
