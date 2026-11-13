//=========================================================
// SBO: co-operative game rules (co_ maps)
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
#include	"coop.h"
#include	"semiclip.h"
#include	"round_hooks.h"
#include	"gmmode.h"
#include	"inventory.h"

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgScoreInfo;
extern CVoiceGameMgr	g_VoiceGameMgr;

extern int g_bChangeLevelTouched;
extern int m_SpawnLocate;
extern string_t g_iszSpawnSet;	// maprules.cpp: the named spawn set multispawn made active
extern int g_bMissionOutcomePending;
extern int g_bMissionFailed;
extern int g_iMapRoundLimit;
extern int RoundLimit;
extern int Limitat;
extern int m_Rc28, m_K444, m_Canon, m_Pm66, m_Fa18, m_Federal, m_Spa, m_Ben, m_Sw, m_Nm33, m_Mini;

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );
extern void SpectatorStart_Joining( CBasePlayer *pPlayer );
extern void respawn( entvars_t *pev, BOOL fCopyCorpse );
extern void ShowVGUI( CBasePlayer *pPlayer, int iMenu );

// SBO 2.0: the players in the round and those still alive are counted when needed (CountRoundPlayers). SBO kept two
// counters (m_iParticip1, m_iLive1) that every death, disconnect, Game Master and spectator change had to edit; each
// missed or doubled edit lost or stalled a round. Only the number alive at the round start is kept: mp_hardcore's
// share is taken from it.
int m_iNumAliveAtStart;		// players alive when the round started (was m_iDeathLimit)
int m_iRoundsPlayed;
int limit;

// SBO 2.0 scoreboard mode info (SboModeInfo_*, multiplay_gamerules.cpp): the round in play or the last one played,
// 0 until the first round starts. Unlike m_iRoundsPlayed it isn't cleared when the round limit fails the mission, so
// the scoreboard keeps showing the last round through the mission end and intermission.
static int s_iRoundShown;
int GmEngage;

// SBO 2.0: the mission's outcome (g_bMissionFailed: 1 failed, 0 accomplished), kept from the start of the mission end
// (state 5) so a game_end / mission_failed / change_touch during it can't turn it around
static int s_iMissionFailed;

// SBO 2.0: when the mission ended (gpGlobals->time at BeginMissionEnd): mp_missionendtime counts from here
static float s_flMissionEndStart;

// shortest and longest mission end (mp_missionendtime), in seconds. The longest leaves room for a map vote with a
// runoff (production's vote plugin held the map end for up to 300 s); mp_chattime's 120 would not.
#define MIN_MISSION_END_TIME		1
#define MAX_MISSION_END_TIME		600

// SBO's mission end: 5 s from the result to the intermission, the players still alive playing on. A mission end
// held longer (mp_missionendtime above this) makes them observers at this time (MissionEndObservers).
#define STOCK_MISSION_END_TIME		5

// the players still alive were made observers in this mission end (once)
static BOOL s_bMissionEndObservers;

// when that was, and how long from then a moved player who has answered no menu waits for the radio (CanUseRadio).
// About what production's map vote took from the move: 12 s of countdown and 30 s of voting. So a mission end held
// without a vote, a vote that isn't shown to a player or one whose answers don't reach the game takes the radio
// for the time of a vote, not for good.
static float s_flMissionEndObserversTime;
#define MISSION_END_RADIO_WAIT		45

// SBO 2.0 log lines (LogRoundEvent, multiplay_gamerules.cpp): a "Round_Start" was written and its "Round_End" not
// yet, so every round gets exactly one of each whichever way it ends; and the mission's line was written
static BOOL s_bRoundLogged;
static BOOL s_bMissionLogged;

// SBO 2.0: seconds a player who leaves spectator mode while nobody is alive in a round stays out of the round
// accounting (CCoop::PlayerStopsSpectating). Production's spectate module: kSpectateSoloExitDelay.
#define SPECTATE_SOLO_EXIT_DELAY	3.0f

static BOOL SpectateExitsDue( void );
static void CoopLogRoundStart( void );
static void CoopLogRoundEnd( void );
static float MissionEndTime( void );

//=========================================================
//=========================================================
CCoop::CCoop()
{
	g_bChangeLevelTouched = 0;
	m_iRoundState = ROUND_PAUSE;
	m_iNumAliveAtStart = 0;
	m_iNextRoundStep = -1;
	g_bMissionOutcomePending = 0;
	g_bMissionFailed = 0;
	s_iMissionFailed = 0;
	s_flMissionEndStart = 0;
	s_bMissionEndObservers = FALSE;
	s_flMissionEndObserversTime = 0;
	m_iRoundsPlayed = 0;
	s_iRoundShown = 0;
	s_bRoundLogged = FALSE;
	s_bMissionLogged = FALSE;
	RoundLimit = 0;
	Limitat = 0;
	GmEngage = 0;
	m_SpawnLocate = 1;
	g_iszSpawnSet = iStringNull;	// SBO 2.1: no named spawn set, with the rotation
}

//=========================================================
//=========================================================
void CCoop::Think( void )
{
	g_VoiceGameMgr.Update(gpGlobals->frametime);

	// halflife SDK (CHalfLifeMultiplay::Think): once the game is over, only the intermission runs. SBO ran the round
	// states first, so after the mission end a new round started (counted on the scoreboard) and
	// respawned everyone during intermission.
	if ( g_fGameOver )   // someone else quit the game already
	{
		CHalfLifeMultiplay::Think();
		return;
	}

	RoundStatus_Update( m_iRoundState, m_iNextRoundStep, 1 );	// SBO 2.0: the loadout menu's NEXT ROUND IN
	HeavyLimit_Update();	// SBO 2.0: the loadout menu's FULL heavy weapons

	// SBO 2.0: a mission outcome (change_touch, game_end, mission_failed) ends the mission in every state a round is
	// played in or started from: 3, 1 (just lost; the mission outcome wins), 0 and 4 (between rounds). SBO acted on
	// it only in state 3, so an outcome between rounds first started a new round that respawned everyone. In the
	// mission end (5) the outcome is already decided: a change_touch then waits (g_bChangeLevelTouched stays set), game_end /
	// mission_failed are ignored.
	if ( m_iRoundState == ROUND_PAUSE || m_iRoundState == ROUND_WAITING || m_iRoundState == ROUND_OVER
		|| m_iRoundState == ROUND_ACTIVE || m_iRoundState == ROUND_SETUP )
	{
		if ( g_bChangeLevelTouched == 1 )	// SBO checked it at the end of Think, in every state
		{
			g_bMissionOutcomePending = 1;
			g_bMissionFailed = 0;
			UTIL_ClientPrintAll( HUD_PRINTCENTER, "SUCCESSFULL MISSION\n" );
			g_bChangeLevelTouched = 0;
		}

		if ( g_bMissionOutcomePending )
			BeginMissionEnd();
	}

	// SBO 2.0: a player who left spectator mode with nobody alive counts again (PlayerStopsSpectating): a round
	// waiting for players is checked now, not when the player left
	if ( SpectateExitsDue() && m_iRoundState == ROUND_WAITING )
		CheckRound();

	if ( m_iRoundState == ROUND_PAUSE || m_iRoundState == ROUND_WAITING )
	{
		m_iNumAliveAtStart = 0;

		if ( m_iNextRoundStep < gpGlobals->time )
			CheckRound();
	}

	if ( m_iRoundState == ROUND_SETUP )
	{
		m_iNumAliveAtStart = 0;
		SetupRound();
	}

	if ( m_iRoundState == ROUND_MISSION_END )
	{
		m_iNumAliveAtStart = 0;

		// mp_missionendtime is read every frame, as CHalfLifeMultiplay::Think reads mp_chattime in the intermission:
		// raising it holds the map end (a map vote), lowering it lets the game go on
		float flElapsed = gpGlobals->time - s_flMissionEndStart;

		if ( flElapsed >= MissionEndTime() )
		{
			g_bMissionOutcomePending = 0;
			g_bMissionFailed = s_iMissionFailed;
			MissionEnd();
		}
		else if ( !s_bMissionEndObservers && flElapsed >= STOCK_MISSION_END_TIME )
		{
			// the intermission is checked first: a mission end of SBO's length (mp_missionendtime 5 or less) ends
			// as it always did, with the players alive until the scoreboard. Only one held longer gets here.
			s_bMissionEndObservers = TRUE;
			MissionEndObservers();
		}
	}

	if ( m_iRoundState == ROUND_ACTIVE )
	{
		// (the mission outcome is checked above: SBO set state 5 here and then went on, so a round lost or emptied
		// in the same frame overwrote it)
		int iPlayers, iAlive;
		CountRoundPlayers( iPlayers, iAlive );

		if ( iPlayers < 1 )
		{
			CoopLogRoundEnd();	// everyone left the round: over, not lost
			m_iNumAliveAtStart = 0;
			m_iNextRoundStep = gpGlobals->time + 20;
			m_iRoundState = ROUND_PAUSE;
			return;
		}

		if ( iAlive <= 0 )
		{
			m_iNumAliveAtStart = 0;
			m_iNextRoundStep = 0;
			m_iRoundState = ROUND_OVER;
			return;
		}
	}

	if ( m_iRoundState == ROUND_OVER )
	{
		RoundWinner();
		m_iNumAliveAtStart = 0;
	}
}

//=========================================================
// SBO 2.0 mp_missionendtime: seconds from the mission's result to the intermission, kept within bounds the way
// GoToIntermission keeps mp_chattime (the cvar itself is corrected, so the server shows the value in use).
//=========================================================
static float MissionEndTime( void )
{
	if ( missionendtime.value < MIN_MISSION_END_TIME )
		CVAR_SET_FLOAT( "mp_missionendtime", MIN_MISSION_END_TIME );
	else if ( missionendtime.value > MAX_MISSION_END_TIME )
		CVAR_SET_FLOAT( "mp_missionendtime", MAX_MISSION_END_TIME );

	return missionendtime.value;
}

//=========================================================
// SBO 2.0: the mission is over (g_bMissionOutcomePending: game_end or change_touch accomplished it, mission_failed or the
// round limit failed it): MissionEnd after mp_missionendtime seconds (state 5). Keeps the outcome (s_iMissionFailed)
// for MissionEnd.
//=========================================================
void CCoop::BeginMissionEnd( void )
{
	// log: the round in play ends with the mission, then the outcome (once: a later game_end / mission_failed
	// doesn't get here again, the state is 5 from now on)
	CoopLogRoundEnd();
	if ( !s_bMissionLogged )
	{
		s_bMissionLogged = TRUE;
		LogWorldEvent( g_bMissionFailed ? "Failed_Mission" : "Successful_Mission" );
	}

	s_iMissionFailed = g_bMissionFailed;
	g_bMissionOutcomePending = 0;
	m_iNumAliveAtStart = 0;
	s_flMissionEndStart = gpGlobals->time;
	m_iNextRoundStep = -1;	// not used in the mission end: mp_missionendtime, counted from s_flMissionEndStart (Think)
	m_iRoundState = ROUND_MISSION_END;
	s_bMissionEndObservers = FALSE;

	GmMode_StopAll();	// SBO 2.0 GM mode ends with the mission (MrCalvin's plugin: mission success / failed)	// the players still alive play on for now (Think: STOCK_MISSION_END_TIME)

	// SBO 2.0: game_mission_success / game_mission_failed, last: the outcome is kept (s_iMissionFailed) and the state is
	// the mission end, so a hook that fires game_end or mission_failed changes nothing
	RoundHooks_MissionEnd( s_iMissionFailed );
}

//=========================================================
// SBO 2.0: a mission end held longer than SBO's 5 s (mp_missionendtime: a map vote). For those 5 s the players still
// alive play on, as they always did until the intermission; then (Think, once) every one of them becomes an
// observer, as the dead of the round are ("Dead Ops" on the scoreboard), from where they stand, so nobody runs
// around a finished map during the vote (production's vote plugin moved them to the observers 4.5 s after the
// mission end, when it started its map vote).
// Not a death: PlayerKilled isn't run, so no death notice, death count, XP or speed penalty, corpse or weapon box;
// StartObserver takes the weapons and lets go of tanks, trains, vehicles and cameras, as when a living player starts
// spectator mode (CBasePlayer::StartSpectating). The players stay the round's players (m_bRoundParticipant: the
// monsters' scaling), and no round check runs: the round is over.
// Left alone: the dead and dying (their death goes on to the observer camera as always), players waiting without a
// loadout or in the menus, spectator mode (m_bSpectating isn't touched: these observers play again on the next
// map), HLTV proxies. (Game Masters are none any more: BeginMissionEnd ended GM mode before this runs.)
// Nobody can spawn into the finished map afterwards: only CheckWho spawns a player alive, and no round is set up in
// the mission end (state 5); a late joiner or a player finishing a loadout becomes an observer as during a round.
//=========================================================
void CCoop::MissionEndObservers( void )
{
	s_flMissionEndObserversTime = gpGlobals->time;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !pPlayer || pPlayer->has_disconnected || ( pPlayer->pev->flags & FL_PROXY ) )
			continue;

		if ( pPlayer->m_bSpectating || pPlayer->m_bGameMaster == 1 || pPlayer->m_bNotInGame )
			continue;

		// in play: spawned by a round and still alive (dead, observing and waiting players are out of play)
		if ( pPlayer->m_bOutOfPlay || ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER ) || pPlayer->IsObserver()
			|| !pPlayer->IsAlive() )
			continue;

		pPlayer->m_iSpawnMode = 1;	// as CCoop::PlayerKilled leaves a dead round player: an observer if respawned
		pPlayer->m_bOutOfPlay = 1;
		pPlayer->m_bMissionEndObserver = TRUE;	// no radio until the player has voted (CanUseRadio)
		pPlayer->m_bMissionEndVoted = FALSE;

		if ( pPlayer->m_iTextMenu == 0 )
		{
			// a radio menu still open goes: its answer would pass for the vote's ("menuselect")
			pPlayer->m_iTextMenu = TEXTMENU_NONE;
			pPlayer->ShowMenu( 0, 0, FALSE, "" );
		}

		pPlayer->StartObserver( pPlayer->pev->origin + pPlayer->pev->view_ofs, pPlayer->pev->v_angle );
	}
}

//=========================================================
// SBO 2.0: the radio menu and its lines are for players in play (m_bOutOfPlay 0), as in SBO; and, in a held mission
// end (a map vote), for the players MissionEndObservers took out of play, once they have voted: they were alive
// when the map ended. No radio from the move to the observers, through the vote's countdown and while the vote
// menu is up, and the radio again for a player who has voted. (Production's vote plugin made them observers with
// StartObserver alone, which left SBO's flag at 0, and its radio plugin, which replaced the game's radio menu, was
// to keep it out of the voting menu.) Not the dead of the round, waiting players or late joiners, nor a player who
// has become a spectator or Game Master since (PlayerLeavesRound clears the mark; spectators: ClientCommand).
// "Voted" is what the game sees of it: the player's first "menuselect" after the move, which can't be the radio
// menu's (m_bMissionEndVoted, ClientCommand). A player who answers nothing waits MISSION_END_RADIO_WAIT.
//=========================================================
BOOL CCoop::CanUseRadio( CBasePlayer *pPlayer )
{
	if ( !pPlayer->m_bOutOfPlay )
		return TRUE;

	if ( m_iRoundState != ROUND_MISSION_END || !pPlayer->m_bMissionEndObserver )
		return FALSE;

	return pPlayer->m_bMissionEndVoted || gpGlobals->time - s_flMissionEndObserversTime >= MISSION_END_RADIO_WAIT;
}

//=========================================================
//=========================================================
int CCoop::IsCoop( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CCoop::InitHUD( CBasePlayer *pPlayer )
{
	pPlayer->m_bNotInGame = 0;
	pPlayer->m_bRoundParticipant = FALSE;	// SBO 2.0: joins the next round
	pPlayer->m_bMissionEndObserver = FALSE;
	pPlayer->m_iTextMenu = TEXTMENU_NONE;
	pPlayer->m_flLoadoutWaitStart = gpGlobals->time;	// SBO 2.0: mp_loadouttime counts from here (MOTD, briefing, menus)
	// SBO 2.0: SBO cleared g_bChangeLevelTouched here, so a player joining in the frame after another touched change_touch
	// wiped the mission success (the constructor clears it for each map)

	pPlayer->pev->classname = MAKE_STRING( "observer" );
	pPlayer->pev->solid = SOLID_NOT;
	pPlayer->pev->takedamage = DAMAGE_NO;
	pPlayer->pev->movetype = MOVETYPE_NONE;
	pPlayer->pev->flags |= FL_NOTARGET;
	pPlayer->pev->effects |= EF_NODRAW;
	pPlayer->m_afPhysicsFlags |= PFLAG_OBSERVER;
	pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT | HIDEHUD_HEALTH;
	pPlayer->RemoveAllItems( TRUE );
	pPlayer->m_iTeamKills = 0;
	pPlayer->m_bLoadoutReady = 0;
	pPlayer->m_bOutOfPlay = 1;
	SpectatorStart_Joining( pPlayer );	// SBO 2.0: nobody to watch: the view from an info_intermission point (observer.cpp)

	CHalfLifeMultiplay::InitHUD( pPlayer );
	RoundStatus_Resend();	// SBO 2.0: the round status again for everybody, this player included
	HeavyLimit_Resend( pPlayer );	// SBO 2.0: the heavy weapon limits to this player
}

//=========================================================
//=========================================================
void CCoop::ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		if ( pPlayer )
		{
			PlayerLeavesRound( pPlayer );

			pPlayer->m_bNotInGame = 1;

			// only the Game Master leaving frees the GM slot
			if ( pPlayer->m_bGameMaster == 1 )
			{
				GmMode_Stop( pPlayer );	// the preview goes with the player
				GmEngage = 0;
			}
			pPlayer->m_bGameMaster = 0;
		}
	}

	CHalfLifeMultiplay::ClientDisconnected( pClient );
}

//=========================================================
// SBO 2.0: a player leaves the round (disconnect, Game Master mode on): no longer a participant, so the counts
// (CountRoundPlayers) leave them out, and the round is checked without them. SBO took one off its player counter for
// everyone, also players the round never counted (joined mid-round, no loadout, Game Masters), so the numbers drifted
// and the round ended or stalled wrongly (production's round_counter_fix plugin patched the counters every frame).
//=========================================================
void CCoop::PlayerLeavesRound( CBasePlayer *pPlayer )
{
	BOOL bParticipant = pPlayer->m_bRoundParticipant;

	pPlayer->m_bRoundParticipant = FALSE;
	pPlayer->m_bMissionEndObserver = FALSE;	// no radio in the mission end any more (CanUseRadio)
	pPlayer->m_flRoundJoinTime = 0;	// no longer on the way back from spectator mode (PlayerStopsSpectating)

	if ( m_iRoundState == ROUND_ACTIVE )
	{
		if ( bParticipant )
			CheckRoundOver();
		return;
	}

	// SBO called CheckRound in every other state: a disconnect or GM toggle during the mission end (5) started a new
	// round instead of finishing the mission, one in state 1 skipped the lost round's RoundWinner, and one in the
	// pause after a lost round cut it short.
	switch ( m_iRoundState )
	{
	case ROUND_WAITING:		// waiting for players: maybe for this one
		CheckRound();
		break;
	case ROUND_PAUSE:		// the pause after a lost round runs its time
	case ROUND_OVER:		// round lost: RoundWinner runs next frame
	case ROUND_SETUP:		// SetupRound runs next frame and counts again
	case ROUND_MISSION_END:	// mission end: the intermission follows
		break;
	}
}

//=========================================================
// SBO 2.0 team limits of the heavy weapons (loadout m_iLoadoutHeavy; the "heavy" command): 3 grenade launchers, 2
// nuclear launchers. Counted from the loadouts of the players in the game, pExcept (the player asking) left out. A
// spectator holds none until joining again (PlayerStopsSpectating). SBO kept two counters (m_iCoopLive: grenade
// launchers, m_iCoopParticip: nuclear launchers) that leaked: a disconnect, the teamkiller ban and cyberware 5
// cleared the loadout without giving the weapon back, and a refused switch from one launcher to the other gave back
// the launcher the player kept.
//=========================================================
#define HEAVY_GLAUNCHER		7
#define HEAVY_NUKE			9
#define MAX_TEAM_GLAUNCHERS	3
#define MAX_TEAM_NUKES		2

static int CountHeavyWeapon( int iHeavy, CBasePlayer *pExcept )
{
	int iCount = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !pPlayer || pPlayer == pExcept || pPlayer->has_disconnected || pPlayer->m_bSpectating )
			continue;

		if ( pPlayer->m_iLoadoutHeavy == iHeavy )
			iCount++;
	}

	return iCount;
}

// May pPlayer take (or keep) heavy weapon iHeavy? Tells the player when the team has no more of it.
static BOOL CanTakeHeavyWeapon( CBasePlayer *pPlayer, int iHeavy, int iMsgDest, const char *pszSuffix )
{
	if ( iHeavy == HEAVY_GLAUNCHER && CountHeavyWeapon( HEAVY_GLAUNCHER, pPlayer ) >= MAX_TEAM_GLAUNCHERS )
	{
		ClientPrint( pPlayer->pev, iMsgDest, UTIL_VarArgs( "3 grenade launchers max / team%s", pszSuffix ) );
		return FALSE;
	}

	if ( iHeavy == HEAVY_NUKE && CountHeavyWeapon( HEAVY_NUKE, pPlayer ) >= MAX_TEAM_NUKES )
	{
		ClientPrint( pPlayer->pev, iMsgDest, UTIL_VarArgs( "2 Nuclear launchers max / team%s", pszSuffix ) );
		return FALSE;
	}

	return TRUE;
}

//=========================================================
// SBO 2.0 "HeavyLimit" (byte grenade launchers taken, byte max, byte nuclear launchers taken, byte max): the team
// limits for the new loadout menu (a full weapon is grayed out with FULL). Each player gets the counts without
// themselves, as CanTakeHeavyWeapon counts for them, so a launcher the player already holds is never full for them.
// Checked 4 times a second from CCoop::Think, sent to a player (HUD initialized) when that player's counts change,
// and again after the player's InitHUD (HeavyLimit_Resend). The server stays the authority: the "heavy" command
// still refuses a full one. Bounty Killer and BR have no limit and never send it.
//=========================================================
extern int gmsgHeavyLimit;
#define HEAVYLIMIT_CLIENTS	32
static int s_iHeavyLimitSent[HEAVYLIMIT_CLIENTS + 1];	// last sent + 1 (glaunchers << 8 | nukes), 0 = send again
static float s_flHeavyLimitNext = 0, s_flHeavyLimitMapTime = 0;

void HeavyLimit_Resend( CBasePlayer *pPlayer )
{
	const int i = pPlayer ? pPlayer->entindex() : 0;
	if ( i >= 1 && i <= HEAVYLIMIT_CLIENTS )
		s_iHeavyLimitSent[i] = 0;
}

void HeavyLimit_Update( void )
{
	if ( !gmsgHeavyLimit )
		return;
	if ( gpGlobals->time < s_flHeavyLimitMapTime )	// a new map: everybody again
	{
		for ( int i = 0; i <= HEAVYLIMIT_CLIENTS; i++ )
			s_iHeavyLimitSent[i] = 0;
		s_flHeavyLimitNext = 0;
	}
	s_flHeavyLimitMapTime = gpGlobals->time;
	if ( gpGlobals->time < s_flHeavyLimitNext )
		return;
	s_flHeavyLimitNext = gpGlobals->time + 0.25;

	const int iClients = min( gpGlobals->maxClients, HEAVYLIMIT_CLIENTS );
	const int iGl = CountHeavyWeapon( HEAVY_GLAUNCHER, NULL ), iNuke = CountHeavyWeapon( HEAVY_NUKE, NULL );
	for ( int i = 1; i <= iClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pPlayer || pPlayer->has_disconnected || !pPlayer->m_fGameHUDInitialized || ( pPlayer->pev->flags & FL_FAKECLIENT ) )
		{
			s_iHeavyLimitSent[i] = 0;
			continue;
		}
		// CountHeavyWeapon's rule: a spectator holds none
		const BOOL bHolds = !pPlayer->m_bSpectating;
		const int iOthersGl = iGl - ( bHolds && pPlayer->m_iLoadoutHeavy == HEAVY_GLAUNCHER ? 1 : 0 );
		const int iOthersNuke = iNuke - ( bHolds && pPlayer->m_iLoadoutHeavy == HEAVY_NUKE ? 1 : 0 );
		const int iKey = ( ( iOthersGl << 8 ) | iOthersNuke ) + 1;
		if ( iKey == s_iHeavyLimitSent[i] )
			continue;
		s_iHeavyLimitSent[i] = iKey;
		MESSAGE_BEGIN( MSG_ONE, gmsgHeavyLimit, NULL, pPlayer->pev );
			WRITE_BYTE( min( iOthersGl, 255 ) );
			WRITE_BYTE( MAX_TEAM_GLAUNCHERS );
			WRITE_BYTE( min( iOthersNuke, 255 ) );
			WRITE_BYTE( MAX_TEAM_NUKES );
		MESSAGE_END();
	}
}

//=========================================================
// SBO 2.0: a player the rounds count: in the game (InitHUD done, not disconnected), not a Game Master (both are
// m_bNotInGame), not an HLTV proxy (SBO counted proxies as players without a loadout, so rounds never started), not in
// spectator mode (m_bSpectating, observer.cpp), and not in the 3 s after leaving it with nobody alive
// (m_flRoundJoinTime, CCoop::PlayerStopsSpectating).
//=========================================================
BOOL IsRoundCandidate( CBasePlayer *pPlayer )
{
	return pPlayer && !pPlayer->m_bNotInGame && !pPlayer->has_disconnected && !( pPlayer->pev->flags & FL_PROXY )
		&& !pPlayer->m_bSpectating && pPlayer->m_flRoundJoinTime <= gpGlobals->time;
}

//=========================================================
// SBO 2.0: the players of the round in play (co-op and BR) and those of them still alive, counted from the players
// themselves every time a decision needs them, as ReGameDLL_CS does (CHalfLifeMultiplay::InitializePlayerCounts).
// A round player is one CheckWho spawned (m_bRoundParticipant) who still counts (IsRoundCandidate: not disconnected,
// Game Master, spectator); alive = in play (m_bOutOfPlay 0). Whoever takes a player out of the round only has to
// change the player's state before the round is checked.
//=========================================================
void CountRoundPlayers( int &iPlayers, int &iAlive )
{
	iPlayers = 0;
	iAlive = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !IsRoundCandidate( pPlayer ) || !pPlayer->m_bRoundParticipant )
			continue;

		iPlayers++;

		if ( !pPlayer->m_bOutOfPlay )
			iAlive++;
	}
}

//=========================================================
// The number of players the monsters scale their accuracy and damage by (hgrunt.cpp, imp.cpp, ...: tiers 1-2, 3-4,
// 5-6, 7 and more; every other value takes the top tier): the players of the co-op round.
// SBO read its player counter (m_iParticip1), which was 0 whenever no round was in play: before the first round,
// in the pause after a lost round, during the mission end, with only a Game Master on the server. 0 fell through
// to the top tier, so the monsters hit hardest when nobody, or only the survivors of a lost round, were there.
// SBO 2.0: the round's players are counted in every round state (they stay the round's players until the next
// round starts), and nobody counts as the lowest tier.
// BR and Bounty Killer have no co-op round: 0 as in SBO, which is the top tier on their maps.
//=========================================================
int MonsterScalePlayers( void )
{
	if ( m_Mode != MODE_COOP )
		return 0;

	int iPlayers, iAlive;
	CountRoundPlayers( iPlayers, iAlive );

	return ( iPlayers < 1 ) ? 1 : iPlayers;
}

//=========================================================
// SBO 2.0 spectator mode on (CBasePlayer::StartSpectating). Spectators count for nothing, so unlike a disconnect
// (PlayerLeavesRound) the round goes on as if the player had never been in it: also the base of mp_hardcore's share
// (m_iNumAliveAtStart, the players alive at the round start) loses them. Leaving alive is no loss, and an earlier death in
// this round no longer counts. The round still ends when, without the player, nobody is alive (the last survivor
// leaves the dead: FAILED ROUND), nobody is left at all (back to waiting; the round number stays used, else a team
// could dodge the round limit by spectating), or the deaths already reach mp_hardcore's share of the smaller round.
// A team-limited heavy weapon (grenade / nuclear launcher) is freed for the others until the player joins again.
//=========================================================
void CCoop::PlayerStartsSpectating( CBasePlayer *pPlayer )
{
	pPlayer->m_flRoundJoinTime = 0;	// spectating again within the 3 s after leaving (PlayerStopsSpectating)

	if ( m_iRoundState == ROUND_ACTIVE && pPlayer->m_bRoundParticipant )
	{
		pPlayer->m_bRoundParticipant = FALSE;	// out of the counts from here on

		if ( m_iNumAliveAtStart > 0 )
			m_iNumAliveAtStart--;

		int iPlayers, iAlive;
		CountRoundPlayers( iPlayers, iAlive );

		// a loss needs a death: in a small round mp_hardcore's share rounds down to 0 (one death loses it), which
		// CheckRoundOver would read as lost with nobody dead. Nobody alive / nobody left: Think, this frame.
		if ( iAlive < m_iNumAliveAtStart )
			CheckRoundOver();
	}
	else
	{
		PlayerLeavesRound( pPlayer );	// not in the round in play: a round waiting for players
	}
}

//=========================================================
// SBO 2.0 spectator mode off (CBasePlayer::StopSpectating): the player joins like a late joiner. With a loadout the
// next round spawns them (and a round waiting for players is checked again at once); without one the team menu
// opens (then the briefing and the loadout menus, as after the MOTD) and mp_loadouttime counts from now.
// The player keeps watching until then. A grenade / nuclear launcher chosen before or while spectating counts toward
// the team's limit again; with none left, no heavy weapon (as the menu refuses it).
// With nobody alive in a round (no round in play, or its last players are dead: where SBO's live counter m_iLive1 was
// 0) the player only counts again 3 s later, as with production's spectate module (ShouldDelaySpectatorExit,
// kSpectateSoloExitDelay; "a single player would respawn immediately after leaving", "solo spectator exit round
// start race condition"): until then no round candidate (IsRoundCandidate), so not spawned or waited for through
// mp_loadouttime; a round start that falls in those 3 s waits for the player (SetupRound), and a round waiting for
// players is checked when they are over (Think, SpectateExitsDue), not at once. The messages come at once.
//=========================================================
void CCoop::PlayerStopsSpectating( CBasePlayer *pPlayer )
{
	int iPlayers, iAlive;
	CountRoundPlayers( iPlayers, iAlive );

	if ( m_iRoundState != ROUND_ACTIVE || iAlive <= 0 )
		pPlayer->m_flRoundJoinTime = gpGlobals->time + SPECTATE_SOLO_EXIT_DELAY;

	if ( !CanTakeHeavyWeapon( pPlayer, pPlayer->m_iLoadoutHeavy, HUD_PRINTTALK, ": choose another heavy weapon (changemodel).\n" ) )
		pPlayer->m_iLoadoutHeavy = 0;

	if ( pPlayer->m_bLoadoutReady == 1 )
	{
		// a spectator who was never in play on this map (joined or changed map spectating) and confirmed the
		// selection menu: waiting for a round with a loadout, as the menu's last step ("item") leaves a late joiner
		// (m_iSpawnMode 1). Left at 0 the scoreboard kept the player under "No Loadout" until the round spawned them.
		if ( pPlayer->m_iSpawnMode == 0 )
			pPlayer->m_iSpawnMode = 1;

		ClientPrint( pPlayer->pev, HUD_PRINTTALK, "You left spectator mode: you join the next round.\n" );

		// (a round waiting for players is checked 3 s from now: Think, SpectateExitsDue. Outside a round in play
		// m_flRoundJoinTime is always set above.)
	}
	else
	{
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, "You left spectator mode: choose a loadout to join the next round.\n" );
		pPlayer->m_flLoadoutWaitStart = gpGlobals->time;
		ShowVGUI( pPlayer, 2 );	// MENU_TEAM
	}
}

//=========================================================
// SBO 2.0: the players whose 3 s after leaving spectator mode are over (PlayerStopsSpectating) are round candidates
// again. TRUE if there is one: Think checks a round waiting for players.
//=========================================================
static BOOL SpectateExitsDue( void )
{
	BOOL bDue = FALSE;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !pPlayer || pPlayer->m_flRoundJoinTime <= 0 || pPlayer->m_flRoundJoinTime > gpGlobals->time )
			continue;

		pPlayer->m_flRoundJoinTime = 0;

		if ( IsRoundCandidate( pPlayer ) )
			bDue = TRUE;
	}

	return bDue;
}

//=========================================================
// SBO 2.0 Game Master mode (gmmode.cpp, docs/GM_MODE.md): "gmon" and the gm_toggle server command, as the GM
// mode plugin (enter_gm_mode): mp_gmmode 1, a living or dead player (as SBO's gmon; the plugin's "only while alive"
// rule stays in the plugin), never a spectator (client.cpp also blocks their "gmon") or an HLTV proxy, not in the
// mission end; one Game Master at a time (GmEngage; by design). The player leaves the round (CheckRoundOver counts
// the players again, so a dead player leaving changes no count) and becomes an observer where they are (a dead
// observer stays one and roams); the intro window comes up over a black screen. The loadout stays.
//=========================================================
BOOL CCoop::GameMasterOn( CBasePlayer *pPlayer, BOOL bAdmin )
{
	// mp_gmmode gates a player's own gmon only; an admin's gm_toggle (amx_gm, the dice reward) works on any co-op map
	if ( ( gmmode.value != 1 && !bAdmin ) || pPlayer->m_bGameMaster || GmEngage == 1 || pPlayer->m_bSpectating
		|| ( pPlayer->pev->flags & FL_PROXY ) || pPlayer->has_disconnected )
		return FALSE;

	// the mission is over (BeginMissionEnd ended GM mode for everybody, as the plugin's mission success / failed
	// did): no new Game Master until the next map
	if ( m_iRoundState == ROUND_MISSION_END || g_fGameOver )
		return FALSE;

	// the spawn catalog is precached on every co-op map (gmmode.cpp GmMode_Precache); a crash guard only
	if ( !GmMode_Ready() )
		return FALSE;

	PlayerLeavesRound( pPlayer );
	Inventory_ReturnAll( pPlayer );	// SBO 2.0 inventory: a Game Master holds no items; his go back to their placed spots

	GmEngage = 1;

	pPlayer->m_bGameMaster = 1;
	pPlayer->m_iSpawnMode = 1;	// a respawn (none is due) would make an observer
	pPlayer->m_bOutOfPlay = 1;
	pPlayer->m_bNotInGame = 1;
	pPlayer->m_iTextMenu = TEXTMENU_NONE;

	GmMode_Start( pPlayer );
	SboScoreboard_Update( pPlayer, SBO_SB_FORCE_TEAMINFO );	// chat-vgui2: scoreboard "Game Master" (TeamNames label: next frame)
	return TRUE;
}

//=========================================================
// Leaving GM mode (Misc > Leave GM Mode, the gm_toggle server command, the mission end), as the GM mode plugin
// (exit_gm_mode): the observer mode ends (StopObserver) and the player is dead where the Game Master was
// (force_player_round_death: a Game Master is never alive, so no kill), then respawns as a dead player does (an
// observer until the next round, which spawns them with their loadout). Everybody is told.
//=========================================================
void CCoop::GameMasterOff( CBasePlayer *pPlayer, BOOL bMissionEnd )
{
	GmMode_Stop( pPlayer );	// SBO 2.0 GM mode: the placement preview, the tools, the roaming speed
	pPlayer->m_bGameMaster = 0;
	GmEngage = 0;

	pPlayer->m_iSpawnMode = 1;
	pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
	pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
	pPlayer->m_bOutOfPlay = 1;
	pPlayer->m_bNotInGame = 0;
	pPlayer->m_bRoundParticipant = FALSE;

	// sma exit_gm_mode: StopObserver, then force_player_round_death (not alive: deadflag DEAD_DEAD, health 0)
	pPlayer->StopObserver();
	pPlayer->pev->deadflag = DEAD_DEAD;
	pPlayer->pev->health = 0;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->m_fDeadTime = gpGlobals->time;
	SboScoreboard_Update( pPlayer );	// chat-vgui2: leaves "Game Master"

	// the mission ends (BeginMissionEnd ends GM mode, as the plugin did): no next round to join
	if ( !bMissionEnd && m_iRoundState == ROUND_WAITING && pPlayer->m_bLoadoutReady == 1 )
		CheckRound();
}

//=========================================================
// SBO 2.0 mp_loadouttime: does this player, who has no loadout yet, still hold up the round start? SBO waited for
// every player without a loadout (m_bLoadoutReady) forever, so one idle player stopped the game for everybody (production
// ran the loadout_autoready plugin: 20 s per round after a player joined, 45 s for the first round of a map).
// The wait is per player and counts from the player's own last chance to choose, not from the round setup:
// m_flLoadoutWaitStart is set when the player gets in the game (InitHUD: MOTD, briefing and loadout menus come
// next) and again at every loadout menu step (LoadoutMenuActivity), so a slow player who is still clicking through
// the menus is waited for; only one who has not touched them for mp_loadouttime seconds is left out. A player left
// out isn't touched (no strip, no respawn, stays an observer with "No Loadout" on the scoreboard), joins a round in
// play as soon as they pick a loadout, like any late joiner, and is spawned by the next round.
// mp_loadouttime 0 waits for everybody, as SBO did. pflUntil: when the wait for this player ends (mp_loadouttime > 0).
//=========================================================
BOOL LoadoutWaitBlocksRound( CBasePlayer *pPlayer, float *pflUntil )
{
	if ( pPlayer->m_bLoadoutReady == 1 )
		return FALSE;

	if ( loadouttime.value <= 0 )
		return TRUE;

	if ( pPlayer->m_flLoadoutWaitStart <= 0 )	// in the game but no InitHUD yet: starts now
		pPlayer->m_flLoadoutWaitStart = gpGlobals->time;

	float flUntil = pPlayer->m_flLoadoutWaitStart + loadouttime.value;

	if ( gpGlobals->time >= flUntil )
		return FALSE;

	if ( pflUntil )
		*pflUntil = flUntil;
	return TRUE;
}

//=========================================================
// SBO 2.0: the player made progress in the MOTD / briefing / loadout menus (client commands ShowVGUI, malefemale,
// blade, light, heavy, armor, cyberware): mp_loadouttime starts again for a player who has no loadout yet.
//=========================================================
void LoadoutMenuActivity( CBasePlayer *pPlayer )
{
	if ( pPlayer && pPlayer->m_bLoadoutReady != 1 )
		pPlayer->m_flLoadoutWaitStart = gpGlobals->time;
}

//=========================================================
// SBO 2.0 "RoundStatus" (byte state, short seconds): the co-op / BR round as the new loadout menu shows it (NEXT ROUND
// IN m:ss, "Waiting for players", "Round in progress"). Worked out from the round state and the players as SetupRound
// counts them, sent to everybody when it changes (checked 4 times a second from the rules' Think) and again to all
// after a player's InitHUD (RoundStatus_Resend). Bounty Killer (no rounds) never sends it. Nothing is sent while no
// client is in the game: the rules' Think runs while the engine loads the map, before any client has the user message
// list, and stock HLDS then prints "PF_MessageEnd_I: Unknown User Msg"; the first client's InitHUD sends it.
//   ROUND_ACTIVE: in progress. ROUND_PAUSE: the next round's check at m_iNextRoundStep.
//   ROUND_WAITING: the round starts when the last player it waits for has a loadout or that player's mp_loadouttime
//   wait runs out (LoadoutWaitBlocksRound), if then enough players are ready (iNeedReady: co-op 1, BR 2): the
//   countdown runs to the latest of those waits; with nobody to wait for that long (mp_loadouttime 0, or too few
//   players) it is "Waiting for players".
//=========================================================
extern int gmsgRoundStatus;
extern int CountPlayers( void );
static int s_iRoundStatusSent = -1, s_iRoundStatusEnd = -1;
static float s_flRoundStatusNext = 0, s_flRoundStatusMapTime = 0;

void RoundStatus_Resend( void )
{
	s_iRoundStatusSent = -1;
}

void RoundStatus_Update( int iRoundState, int iNextRoundStep, int iNeedReady )
{
	if ( !gmsgRoundStatus )
		return;
	if ( gpGlobals->time < s_flRoundStatusMapTime )	// a new map
	{
		s_iRoundStatusSent = -1;
		s_flRoundStatusNext = 0;
	}
	s_flRoundStatusMapTime = gpGlobals->time;
	if ( CountPlayers() == 0 )
		return;
	if ( gpGlobals->time < s_flRoundStatusNext && s_iRoundStatusSent >= 0 )
		return;
	s_flRoundStatusNext = gpGlobals->time + 0.25;

	int iState = ROUNDSTATUS_NONE;
	float flEnd = 0;
	if ( iRoundState == ROUND_ACTIVE )
		iState = ROUNDSTATUS_PLAYING;
	else if ( iRoundState == ROUND_PAUSE && iNextRoundStep > 0 )
	{
		iState = ROUNDSTATUS_NEXT;
		flEnd = iNextRoundStep;
	}
	else if ( iRoundState == ROUND_WAITING || iRoundState == ROUND_SETUP )
	{
		int iReady = 0, iLimited = 0, iUnlimited = 0;
		float flLatest = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->m_flRoundJoinTime > gpGlobals->time )
			{
				iLimited++;
				flLatest = max( flLatest, pPlayer->m_flRoundJoinTime );
				continue;
			}
			if ( !IsRoundCandidate( pPlayer ) )
				continue;
			if ( pPlayer->m_bLoadoutReady == 1 )
			{
				iReady++;
				continue;
			}
			float flUntil = -1;
			if ( LoadoutWaitBlocksRound( pPlayer, &flUntil ) )
			{
				if ( flUntil > 0 )
				{
					iLimited++;
					flLatest = max( flLatest, flUntil );
				}
				else
					iUnlimited++;
			}
		}
		if ( iUnlimited > 0 || ( iLimited == 0 && iReady < iNeedReady ) )
			iState = ROUNDSTATUS_WAITING;
		else if ( iLimited > 0 )
		{
			iState = ROUNDSTATUS_NEXT;
			flEnd = (int)flLatest + 1;	// the check after the last wait ends (SetupRound: m_iNextRoundStep = (int)flRetry + 1)
		}
		else
		{
			iState = ROUNDSTATUS_NEXT;	// everyone ready: the next check starts it
			flEnd = iNextRoundStep;
		}
	}
	const int iEnd = iState == ROUNDSTATUS_NEXT ? (int)ceil( flEnd ) : -1;
	if ( iState == s_iRoundStatusSent && iEnd == s_iRoundStatusEnd )
		return;
	s_iRoundStatusSent = iState;
	s_iRoundStatusEnd = iEnd;
	const int iSeconds = iEnd >= 0 ? max( 0, min( 32767, (int)ceil( iEnd - gpGlobals->time ) ) ) : 0;
	MESSAGE_BEGIN( MSG_ALL, gmsgRoundStatus );
		WRITE_BYTE( iState );
		WRITE_SHORT( iSeconds );
	MESSAGE_END();
}

//=========================================================
//=========================================================
void CCoop::RoundWinner( void )
{
	GmMode_RoundEnd();	// SBO 2.0 GM mode: the NPC orders end with the round (the selections stay)
	CoopLogRoundEnd();	// before the round limit clears m_iRoundsPlayed; "Failed_Mission" follows from BeginMissionEnd

	if ( m_iRoundsPlayed >= CoopRoundLimit() )
	{
		g_bMissionFailed = 1;
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "FAILED MISSION\n" );
		m_iRoundsPlayed = 0;
		BeginMissionEnd();
	}
	else
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "FAILED ROUND\n" );
		Semiclip_RoundFailed();	// SBO 2.0 semiclip: as the module did at this text
		m_iNextRoundStep = gpGlobals->time + 5;
		m_iRoundState = ROUND_PAUSE;
		RoundHooks_RoundFailed();	// SBO 2.0: game_round_failed, after the round's game_round_end
	}
}

//=========================================================
// Number of rounds a co-op mission gets: when a round is lost, RoundWinner fails the mission once m_iRoundsPlayed
// reaches it. A max_round entity (RoundLimit 1) sets it to its nbmaxround (1.5 times that in mp_easymode 1:
// SBO compared m_iRoundsPlayed >= g_iMapRoundLimit * 1.5); maps without one get 7 (10 in mp_easymode 1).
// Read every time: mp_easymode may change mid-map.
//=========================================================
int CoopRoundLimit( void )
{
	if ( RoundLimit == 1 )
		return ( easymode.value == 1 ) ? ( g_iMapRoundLimit * 3 + 1 ) / 2 : g_iMapRoundLimit;	// smallest n >= g_iMapRoundLimit * 1.5

	return ( easymode.value == 1 ) ? 10 : 7;
}

//=========================================================
// SBO 2.0 scoreboard (SboModeInfo_*): current round (0 before the first round starts) and the round limit.
//=========================================================
void CoopGetRoundInfo( int &iRound, int &iRoundLimit )
{
	iRound = s_iRoundShown;
	iRoundLimit = CoopRoundLimit();
}

//=========================================================
// SBO 2.0 log lines: World triggered "Round_Start" / "Round_End" (round "N" of "M"), M = CoopRoundLimit
//=========================================================
static void CoopLogRoundStart( void )
{
	CoopLogRoundEnd();	// never two starts without an end between them

	s_bRoundLogged = TRUE;
	LogRoundEvent( "Round_Start", s_iRoundShown, CoopRoundLimit() );
}

static void CoopLogRoundEnd( void )
{
	if ( !s_bRoundLogged )
		return;

	s_bRoundLogged = FALSE;
	LogRoundEvent( "Round_End", s_iRoundShown, CoopRoundLimit() );
	RoundHooks_End();	// SBO 2.0: game_round_end, once per round as the log line
}

//=========================================================
//=========================================================
void CCoop::CheckRound( void )
{
	m_iRoundState = ROUND_SETUP;
	m_iNextRoundStep = 0;
}

//=========================================================
//=========================================================
void CCoop::MissionEnd( void )
{
	// SBO 2.0: a failed mission ends like an accomplished one: intermission, then the next map of the map cycle. SBO
	// stripped the players and opened a "Reload map?" vote here (round state 2, ShowVGUI 18, vote_yes / vote_no,
	// ReloadMap); it never worked and is gone.
	g_bMissionOutcomePending = 0;
	g_bMissionFailed = 0;
	m_iRoundState = ROUND_PAUSE;
	GoToIntermission();
}

//=========================================================
//=========================================================
void CCoop::SetupRound( void )
{
	m_iNumAliveAtStart = 0;

	int iReady = 0;		// players with a loadout: the round would spawn them
	int iWaitFor = 0;	// players without a loadout the round waits for (mp_loadouttime)
	float flRetry = gpGlobals->time + 20;

	// SBO 2.0: only counts; CheckWho strips the players it spawns. SBO stripped every player here, also while the
	// round went on waiting, which left mp_hardcore survivors of the last round alive without weapons.
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		// SBO 2.0: a player in the 3 s after leaving spectator mode (PlayerStopsSpectating) isn't counted yet; the
		// round waits for those 3 s to end, so the player is in it
		if ( pPlayer && pPlayer->m_flRoundJoinTime > gpGlobals->time )
		{
			iWaitFor++;
			if ( pPlayer->m_flRoundJoinTime < flRetry )
				flRetry = pPlayer->m_flRoundJoinTime;
			continue;
		}

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

	// SBO 2.0: nobody to play (empty server, map start before anyone joined, only Game Masters) waits like an
	// unready player does. SBO ran CheckWho anyway, which counted an empty round every 20 s: the first real round
	// was already round 2 or later and the mission failed early (the register_rounds plugin's CheckWho hooks undid it).
	if ( iReady < 1 || iWaitFor > 0 )
	{
		m_iNumAliveAtStart = 0;
		m_iNextRoundStep = (int)flRetry + 1;	// every 20 s (SBO), sooner when a player's mp_loadouttime runs out
		m_iRoundState = ROUND_WAITING;
	}
	else
	{
		CheckWho();
	}
}

//=========================================================
//=========================================================
void CCoop::CheckWho( void )
{
	int iSpawned = 0;

	// SBO 2.0 round hooks: the last round's game_round_end first (if not fired yet), then the map's "round_reset"
	// entities go back to their state before anyone spawns (spawn points fire their targets at the spawn)
	CoopLogRoundEnd();
	RoundHooks_Reset();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		// SBO 2.0: the round's participants are the players it spawns (m_bRoundParticipant). SBO also counted and
		// stripped the players without a loadout; with mp_loadouttime the round can start without them, and they
		// are left alone (no strip, not in the round's player count, which also scales the monsters' accuracy and damage).
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

	m_iRoundsPlayed++;
	s_iRoundShown = m_iRoundsPlayed;
	GmMode_RoundStart();	// SBO 2.0 GM mode: "Killed by your <monster>" is per round
	CoopLogRoundStart();
	RoundHooks_Start( s_iRoundShown );	// SBO 2.0: game_round_start and game_round_<N>, the round's players spawned

	m_iNumAliveAtStart = iSpawned;
	m_iNextRoundStep = gpGlobals->time + 0;
	m_iRoundState = ROUND_ACTIVE;
}

//=========================================================
//=========================================================
void CCoop::CheckRoundOver( void )
{
	int iPlayers, iAlive;
	CountRoundPlayers( iPlayers, iAlive );

	if ( iPlayers >= 1 )
	{
		int hardcore = CVAR_GET_FLOAT( "mp_hardcore" );

		if ( iAlive <= m_iNumAliveAtStart - hardcore * m_iNumAliveAtStart / 100 )
		{
			m_iNumAliveAtStart = 0;
			m_iNextRoundStep = 0;
			m_iRoundState = ROUND_OVER;
		}
	}
}

//=========================================================
//=========================================================
BOOL CCoop::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if ( g_VoiceGameMgr.ClientCommand( pPlayer, pcmd ) )
		return TRUE;

	if ( FStrEq( pcmd, "menuselect" ) )
	{
		if ( CMD_ARGC() < 2 )
			return TRUE;

		int slot = atoi( CMD_ARGV(1) );

		// SBO 2.0: a key picks in the game's text menu on screen only, once. A "menuselect" with none of them up
		// answers a plugin's menu (the plugin sees the command first and lets it through), e.g. the map vote's: it is
		// no radio pick (SBO read every "menuselect" while no other game menu was up as one: a vote for map 2 said
		// "Follow Me!!"). A player the held mission end made an observer (MissionEndObservers) has voted with it, and
		// has the radio from then on (CanUseRadio).
		int iTextMenu = pPlayer->m_iTextMenu;

		pPlayer->m_iTextMenu = TEXTMENU_NONE;

		if ( iTextMenu == TEXTMENU_NONE )
		{
			if ( pPlayer->m_bMissionEndObserver )
				pPlayer->m_bMissionEndVoted = TRUE;
			return TRUE;
		}

		// SBO 2.0 GM mode's text menus (quick menu, Misc., orders, the classic menus' lists)
		if ( iTextMenu == TEXTMENU_GM )
		{
			GmMode_MenuSelect( pPlayer, slot );
			return TRUE;
		}

		switch ( iTextMenu )
		{
		// the radio: one sound from the speaker, heard by everyone near (ATTN_NORM), and the line to every player's
		// chat. SBO 2.0: the sound is emitted once per call on CHAN_AUTO, as production's syndicate_radio_commands
		// plugin did; SBO emitted it on CHAN_STATIC inside the chat loop, once per connected player, so N players
		// heard N copies stacked (playtest 2026-10-17: "voice lines too loud").
		case 0:
			if ( CanUseRadio( pPlayer ) )
			{
				if ( pPlayer->m_bMaleAgent == 1 )
				{
					if ( slot == 1 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/cover.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Cover Me !\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 2 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/follow.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Follow Me!!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 3 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/shoot.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Shoot Them !\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 4 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/medic.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Medic !!!!!!!!!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 5 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/help.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Help Me !!!!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 6 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/wait.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Wait Me !!!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 7 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/side.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Attack by side\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 8 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/grenade.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Grenade !!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 9 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/retreat.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Retreat !!!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
				}
				else
				{
					if ( slot == 1 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/coverf.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Cover Me !\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 2 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/followf.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Follow Me!!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 3 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/shootf.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Shoot Them !\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 4 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/medicf.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Medic !!!!!!!!!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 5 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/helpf.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Help Me !!!!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 6 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/waitf.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Wait Me !!!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 7 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/sidef.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Attack by side\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 8 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/grenadef.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Grenade !!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
					else if ( slot == 9 )
					{
						EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "radio/retreatf.wav", 1, ATTN_NORM );
						for ( int i = 0; i <= gpGlobals->maxClients; i++ )
						{
							CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

							if ( pEnt && pEnt->IsPlayer() )
							{
								UTIL_SayText( UTIL_VarArgs( "(RADIO) %s : Retreat !!!\n", UTIL_ChatPlayerName( STRING( pPlayer->pev->netname ), pPlayer->edict() ) ), pEnt );
							}
						}
					}
				}
			}
			break;
		}

		return TRUE;
	}

	if ( FStrEq( pcmd, "gmon" ) )
	{
		// SBO 2.0: a Game Master's own "gmon" (the GM key) opens the spawn window on the client and doesn't get here
		if ( !GameMasterOn( pPlayer ) && GmEngage == 1 && pPlayer->m_bGameMaster == 0 )
			GmMode_Notice( pPlayer, "there is already a gm" );
		return TRUE;
	}

	// SBO 2.0 GM mode: the spawn window's commands (gm_pick, gm_goto, gm_help, gm_leave) and the intro's "gm"
	if ( GmMode_ClientCommand( pPlayer, pcmd ) )
		return TRUE;

	if ( FStrEq( pcmd, "changemodel" ) )
	{
		ShowVGUI( pPlayer, 10 );
		return TRUE;
	}

	if ( FStrEq( pcmd, "radio" ) )
	{
		if ( CanUseRadio( pPlayer ) && m_Mode == MODE_COOP )
		{
			pPlayer->m_iTextMenu = 0;	// the next "menuselect" answers it
			pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Radio" );
		}
		return TRUE;
	}

	// SBO 2.0 mp_loadouttime: still choosing (the briefing's and loadout menus' steps; "item" is the last one)
	if ( FStrEq( pcmd, "malefemale" ) || FStrEq( pcmd, "blade" ) || FStrEq( pcmd, "light" ) || FStrEq( pcmd, "heavy" )
		|| FStrEq( pcmd, "armor" ) || FStrEq( pcmd, "cyberware" ) )
		LoadoutMenuActivity( pPlayer );

	if ( FStrEq( pcmd, "malefemale" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iLoadoutModel = 1;
			break;
		case 2:
			pPlayer->m_iLoadoutModel = 2;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "blade" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iLoadoutBlade = 1;
			break;
		case 2:
			pPlayer->m_iLoadoutBlade = 2;
			break;
		case 3:
			pPlayer->m_iLoadoutBlade = 3;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "light" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iLoadoutLight = 1;
			break;
		case 2:
			pPlayer->m_iLoadoutLight = 2;
			break;
		case 3:
			pPlayer->m_iLoadoutLight = 3;
			break;
		case 4:
			pPlayer->m_iLoadoutLight = 4;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "heavy" ) )
	{
		int iHeavy = atoi( CMD_ARGV(1) );

		if ( iHeavy < 1 || iHeavy > 9 )
			return TRUE;

		// SBO 2.0 spectator mode: a spectator holds none of the team's grenade / nuclear launchers: the choice is kept
		// and checked on joining again (PlayerStopsSpectating). Else the team limits apply (CanTakeHeavyWeapon); a
		// refused choice leaves the loadout as it was.
		if ( pPlayer->m_bSpectating || CanTakeHeavyWeapon( pPlayer, iHeavy, HUD_PRINTCENTER, "" ) )
			pPlayer->m_iLoadoutHeavy = iHeavy;

		return TRUE;
	}

	if ( FStrEq( pcmd, "armor" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iLoadoutArmor = 1;
			break;
		case 2:
			pPlayer->m_iLoadoutArmor = 2;
			break;
		case 3:
			pPlayer->m_iLoadoutArmor = 3;
			break;
		case 4:
			pPlayer->m_iLoadoutArmor = 0;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "cyberware" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iLoadoutCyberware = 1;
			break;
		case 2:
			pPlayer->m_iLoadoutCyberware = 2;
			break;
		case 3:
			pPlayer->m_iLoadoutCyberware = 3;
			break;
		case 4:
			pPlayer->m_iLoadoutCyberware = 4;
			break;
		case 5:
			pPlayer->m_iLoadoutCyberware = 5;
			break;
		case 6:
			pPlayer->m_iLoadoutCyberware = 6;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "item" ) )
	{
		// SBO 2.0 spectator mode: confirming the selection menu ("Go !") leaves spectator mode, as this step puts a
		// waiting player in the game. The loadout is complete (m_bLoadoutReady) and the player joins the next round
		// (PlayerStopsSpectating), once the commands the menu sends after this one are in (UpdateClientData,
		// Spectate_MenuConfirmed); the observer join below would have put a spectator back at a spawn point.
		if ( pPlayer->m_bSpectating )
		{
			int iItem = atoi( CMD_ARGV(1) );

			if ( iItem >= 1 && iItem <= 4 )
			{
				pPlayer->m_iLoadoutItem = iItem;
				pPlayer->m_bLoadoutReady = 1;
				pPlayer->m_bSpectateMenuExit = TRUE;
			}
			return TRUE;
		}

		int iItem = atoi( CMD_ARGV(1) );

		if ( iItem >= 1 && iItem <= 4 )
		{
			pPlayer->m_iLoadoutItem = iItem;

			// a dead or waiting player: the loadout is complete and the player joins (PlayerLoadoutJoins) once the
			// commands the menu sends after this one (cyberware, armor, malefemale) are in: UpdateClientData. SBO
			// respawned here, with the cyberware saved before, so leaving Assassin gave one more spawn with the blade
			// only (the Assassin spawn clears the light, heavy and item picks).
			if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
			{
				pPlayer->m_bLoadoutReady = 1;
				pPlayer->m_bLoadoutJoinPending = TRUE;
			}
		}
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// SBO 2.0: a dead or waiting player confirmed the loadout menu ("item", ClientCommand) and all its commands are in
// (CBasePlayer::LoadoutJoin): an observer at a spawn point until the next round, with that loadout
//=========================================================
void CCoop::PlayerLoadoutJoins( CBasePlayer *pPlayer )
{
	if ( !( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER ) || pPlayer->m_bSpectating )
		return;

	pPlayer->m_iSpawnMode = 1;
	pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
	pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
	pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
	pPlayer->m_bOutOfPlay = 1;
	pPlayer->m_bLoadoutReady = 1;
	respawn( pPlayer->pev, FALSE );
}

//=========================================================
//=========================================================
void CCoop::PlayerSpawn( CBasePlayer *pPlayer )
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

		if ( m_Mode == MODE_DM || m_Mode == MODE_BR || m_Mode == MODE_COOP )
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

	if ( ban.value == 1 && pPlayer->m_iTeamKills >= 5 )
	{
		pPlayer->RemoveAllItems( TRUE );
		pPlayer->pev->health = 3;
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "you are a teamKiller" );
		pPlayer->m_iLoadoutCyberware = 0;
		pPlayer->m_iLoadoutArmor = 0;
		pPlayer->m_iLoadoutItem = 0;
		pPlayer->m_iLoadoutLight = 0;
		pPlayer->m_iLoadoutHeavy = 0;
		pPlayer->m_iLoadoutBlade = 0;
		pPlayer->pev->weapons |= (1<<WEAPON_SUIT);
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "hunted" );
		pPlayer->m_iLoadoutModel = 0;
		pPlayer->m_bMaleAgent = 1;
	}

	if ( pPlayer->m_iLoadoutCyberware == 5 )
		pPlayer->m_iCanCloak = 1;
	else
		pPlayer->m_iCanCloak = 0;

	if ( pPlayer->m_iLoadoutCyberware == 6 || pPlayer->m_iLoadoutCyberware == 5 )
		pPlayer->m_iCanCybereye = 1;
	else
		pPlayer->m_iCanCybereye = 0;

	if ( pPlayer->m_iLoadoutCyberware == 2 || pPlayer->m_iLoadoutCyberware == 5 )
		pPlayer->m_bTargeting = 1;
	else
		pPlayer->m_bTargeting = 0;

	pPlayer->pev->renderamt = 255;
	pPlayer->pev->renderfx = kRenderFxNone;
	pPlayer->pev->rendermode = kRenderNormal;
	pPlayer->pev->gravity = 1;
	pPlayer->m_iSboMaxSpeed = SboLevelSpeed( pPlayer->m_iSboSpeed );	// SBO 2.0: below level -40, its speed (player.h)

	switch ( pPlayer->m_iLoadoutModel )
	{
	case 1:
		pPlayer->m_bMaleAgent = 0;
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agentf" );
		break;
	case 2:
		pPlayer->m_bMaleAgent = 1;
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agent" );
		break;
	default:
		pPlayer->m_bMaleAgent = 1;
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agent" );
		break;
	}

	// SBO 2.0: Assassin (cyberware 5) gives the armor skin, so its weight is the skin's. 1.38 applied the weight of the
	// armor picked before Assassin (kept in m_iLoadoutArmor): Assassin's speed changed with a pick the menu no longer shows.
	switch ( pPlayer->m_iLoadoutCyberware == 5 ? 1 : pPlayer->m_iLoadoutArmor )
	{
	case 1:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->pev->armorvalue = 50;
		pPlayer->m_iArmorType = 3;
		break;
	case 2:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->pev->armorvalue = 100;
		pPlayer->m_iArmorType = 1;
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->pev->armorvalue = 100;
		pPlayer->m_iArmorType = 2;
		break;
	}

	switch ( pPlayer->m_iLoadoutCyberware )
	{
	case 1:
		pPlayer->m_iSboMaxSpeed += 50;
		break;
	case 2:
		pPlayer->m_bTargeting = 1;
		break;
	case 3:
		pPlayer->GiveNamedItem( "weapon_medikit" );
		break;
	case 4:
		pPlayer->pev->health = pPlayer->m_iSboHealth + 75;
		break;
	case 5:
		pPlayer->m_iLoadoutLight = 0;
		pPlayer->m_iLoadoutHeavy = 0;
		pPlayer->m_iLoadoutCyberware = 5;
		pPlayer->m_iLoadoutItem = 0;
		pPlayer->GiveNamedItem( "weapon_k444" );
		pPlayer->m_iCanCloak = 1;
		pPlayer->m_iCanCybereye = 1;
		pPlayer->m_bTargeting = 1;
		pPlayer->GiveNamedItem( "weapon_medikit" );
		pPlayer->pev->armorvalue = 50;
		pPlayer->m_iArmorType = 3;
		pPlayer->m_iSboMaxSpeed += 25;
		pPlayer->pev->health = pPlayer->m_iSboHealth + 50;
		pPlayer->GiveNamedItem( "weapon_handgrenade" );
		pPlayer->GiveNamedItem( "weapon_satchel" );
		break;
	case 6:
		pPlayer->m_iCanCybereye = 1;
		break;
	}

	switch ( pPlayer->m_iLoadoutItem )
	{
	case 1:
		pPlayer->GiveNamedItem( "weapon_handgrenade" );
		break;
	case 2:
		// SBO 2.0: no speed penalty, as #SBO_Item_tripmine says ("Speed malus : 0"; 1.38 took 6 = 2%)
		pPlayer->GiveNamedItem( "weapon_tripmine" );
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->GiveNamedItem( "weapon_satchel" );
		break;
	case 4:
		pPlayer->GiveNamedItem( "weapon_snark" );
		break;
	}

	switch ( pPlayer->m_iLoadoutBlade )
	{
	case 1:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->GiveNamedItem( "weapon_warhammer" );
		break;
	case 2:
		pPlayer->GiveNamedItem( "weapon_chainsaw" );
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->GiveNamedItem( "weapon_sword" );
		break;
	}

	switch ( pPlayer->m_iLoadoutLight )
	{
	case 1:
		pPlayer->GiveNamedItem( "weapon_rc28" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Rc28, "rc28m", 900 );
		else
			pPlayer->GiveAmmo( 120, "rc28m", 900 );
		break;
	case 2:
		pPlayer->GiveNamedItem( "weapon_k444" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_K444, "K444m", 900 );
		else
			pPlayer->GiveAmmo( 140, "K444m", 900 );
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->GiveNamedItem( "weapon_canon" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Canon, "canonm", 900 );
		else
			pPlayer->GiveAmmo( 180, "canonm", 900 );
		break;
	case 4:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->GiveNamedItem( "weapon_pm66" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Pm66, "pm66m", 900 );
		else
			pPlayer->GiveAmmo( 150, "pm66m", 900 );
		break;
	}

	switch ( pPlayer->m_iLoadoutHeavy )
	{
	case 1:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->GiveNamedItem( "weapon_fa18" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Fa18, "fa18m", 900 );
		else
			pPlayer->GiveAmmo( 250, "fa18m", 900 );
		break;
	case 2:
		pPlayer->m_iSboMaxSpeed -= 20;
		pPlayer->GiveNamedItem( "weapon_federal" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Federal, "federalm", 900 );
		else
			pPlayer->GiveAmmo( 210, "federalm", 900 );
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 45;
		pPlayer->GiveNamedItem( "weapon_sw" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Sw, "swm", 900 );
		else
			pPlayer->GiveAmmo( 50, "swm", 900 );
		break;
	case 4:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->GiveNamedItem( "weapon_nm33" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Nm33, "nm33m", 900 );
		else
			pPlayer->GiveAmmo( 150, "nm33m", 900 );
		break;
	case 5:
		pPlayer->m_iSboMaxSpeed -= 20;
		pPlayer->GiveNamedItem( "weapon_spa" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Spa, "spam", 900 );
		else
			pPlayer->GiveAmmo( 160, "spam", 900 );
		break;
	case 6:
		pPlayer->m_iSboMaxSpeed -= 20;
		pPlayer->GiveNamedItem( "weapon_ben" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Ben, "benm", 900 );
		else
			pPlayer->GiveAmmo( 120, "benm", 900 );
		break;
	case 7:
		pPlayer->m_iSboMaxSpeed -= 60;
		pPlayer->GiveNamedItem( "weapon_glauncher" );
		pPlayer->GiveAmmo( 1, "ARgrenades", 20 );
		break;
	case 8:
		pPlayer->m_iSboMaxSpeed -= 80;
		pPlayer->GiveNamedItem( "weapon_chaingun" );
		if ( Limitat == 1 )
			pPlayer->GiveAmmo( m_Mini, "chainm", 900 );
		else
			pPlayer->GiveAmmo( 100, "chainm", 900 );
		break;
	case 9:
		pPlayer->m_iSboMaxSpeed -= 80;
		pPlayer->GiveNamedItem( "weapon_rpg" );
		break;
	}

	// SBO 2.0: a low level's speed less the loadout's weights can reach 0 or below, where the player can't move
	// (player.h SBO_MIN_MAXSPEED)
	if ( pPlayer->m_iSboMaxSpeed < SBO_MIN_MAXSPEED )
		pPlayer->m_iSboMaxSpeed = SBO_MIN_MAXSPEED;

	g_engfuncs.pfnSetClientMaxspeed( pPlayer->edict(), pPlayer->m_iSboMaxSpeed );

	pPlayer->m_iAutoWepSwitch = originalAutoWepSwitch;
}

//=========================================================
//=========================================================
BOOL CCoop::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	pPlayer->m_bOutOfPlay = 1;

	edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );
	pPlayer->StartObserver( VARS(pentSpawnSpot)->origin, VARS(pentSpawnSpot)->angles );

	return FALSE;
}

//=========================================================
//=========================================================
int CCoop::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return -5;
}

//=========================================================
//=========================================================
// The teamkill / self-kill penalty applies only when mp_XP is 1: without XP no monster or medikit gives a level-up,
// so a lost level or spawn health could never come back. (The 1.38 release's hl.dll and the production hl_i386.so
// do the same; the release hl_i386.so, built from another source snapshot, applied it always: NOTES.md, "Server
// binaries: release vs production".)
//
// The penalty: five levels (CBasePlayer::SboLevelPenalty: no lower limit, the speed effect stops at level -40), and
// the spawn health (m_iSboHealth) 25 lower, never below 10. The scoreboard XP (m_iSboTotalXP) follows the levels it
// takes away, 5000 XP (a monster level-up) per level lost, and may go negative, so the XP column and the level column
// agree. The level is the scoreboard's: (m_iSboSpeed - 300) / 5.
#define SBO_PENALTY_LEVEL( speed )	( ( (speed) - 300 ) / 5 )
#define SBO_PENALTY_XP_PER_LEVEL	5000

#define SBO_PENALTY_HEALTH			25
#define SBO_PENALTY_HEALTH_MIN		10

static void SboKillPenalty( CBasePlayer *pPlayer )
{
	if ( XP.value != 1 )
		return;

	int iOldSpeed = pPlayer->m_iSboSpeed;
	pPlayer->SboLevelPenalty();

	pPlayer->m_iSboHealth -= SBO_PENALTY_HEALTH;
	if ( pPlayer->m_iSboHealth < SBO_PENALTY_HEALTH_MIN )
		pPlayer->m_iSboHealth = SBO_PENALTY_HEALTH_MIN;

	pPlayer->m_iSboTotalXP -= ( SBO_PENALTY_LEVEL( iOldSpeed ) - SBO_PENALTY_LEVEL( pPlayer->m_iSboSpeed ) ) * SBO_PENALTY_XP_PER_LEVEL;
}

void CCoop::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	DeathNotice( pVictim, pKiller, pInflictor );
	GmMode_PlayerKilled( pVictim, pKiller, pInflictor );	// SBO 2.0 GM mode: a level every two players the GM's monsters kill

	if ( m_iRoundState == ROUND_ACTIVE )
	{
		// out of play before the round is checked: CheckRoundOver counts the players still alive
		pVictim->m_iSpawnMode = 1;
		pVictim->m_bOutOfPlay = 1;
		CheckRoundOver();
	}

	pVictim->m_iDeaths += 1;
	pVictim->m_iXP = pVictim->m_iXP / 2;

	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );
	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;

	if ( pVictim->pev == pKiller )
	{  // killed self
		if ( peKiller && peKiller->Classify() == CLASS_PLAYER )
		{
			if ( pKiller->frags <= 4 )
				pKiller->frags = 0;
			else
				pKiller->frags -= 5;

			SboKillPenalty( peKiller );
		}
	}
	else if ( ktmp && ktmp->IsPlayer() )
	{
		if ( peKiller && peKiller->Classify() == CLASS_PLAYER )
		{
			if ( pKiller->frags <= 4 )
				pKiller->frags = 0;
			else
				pKiller->frags -= 5;

			SboKillPenalty( peKiller );
		}

		peKiller->m_iTeamKills++;

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

	if ( pVictim->HasNamedPlayerItem("weapon_satchel") )
	{
		DeactivateSatchels( pVictim );
	}

}

//=========================================================
//=========================================================
BOOL CCoop::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if ( friendlyfire.value == 0 && pAttacker != pPlayer && pAttacker->IsPlayer() )
		return FALSE;

	return TRUE;
}
