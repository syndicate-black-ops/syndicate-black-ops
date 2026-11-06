//=========================================================
// SBO: Bounty Killer game rules (default mode)
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
#include	"dm.h"
#include	"round_hooks.h"

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgScoreInfo;
extern CVoiceGameMgr	g_VoiceGameMgr;

extern int g_bChangeLevelTouched;
extern int m_SpawnLocate;
extern cvar_t timeleft, fragsleft;

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );
extern void SpectatorStart_Joining( CBasePlayer *pPlayer );
extern void respawn( entvars_t *pev, BOOL fCopyCorpse );
extern void ShowVGUI( CBasePlayer *pPlayer, int iMenu );

int Bounty;		// someone is the hunted
int EndRound;

// SBO 2.0 round hooks (round_hooks.h): game_round_start when a round starts, game_round_end once when it ends
// (RoundWinner also runs for a hunted who leaves, whatever the state)
static BOOL s_bDmRoundHooked;

static void DmRoundHookStart( void )
{
	if ( s_bDmRoundHooked )
		RoundHooks_End();	// never two starts without an end between them
	s_bDmRoundHooked = TRUE;
	RoundHooks_Start();
}

static void DmRoundHookEnd( void )
{
	if ( !s_bDmRoundHooked )
		return;
	s_bDmRoundHooked = FALSE;
	RoundHooks_End();
}

//=========================================================
//=========================================================
CDm::CDm()
{
	g_bChangeLevelTouched = 0;
	m_iRoundState = ROUND_PAUSE;
	m_iNextRoundStep = -1;
	Bounty = 0;
	EndRound = 1;
	m_SpawnLocate = 1;
	s_bDmRoundHooked = FALSE;	// SBO 2.0: a new map, no round yet
}

//=========================================================
//=========================================================
void CDm::Think( void )
{
	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining;

	g_VoiceGameMgr.Update(gpGlobals->frametime);

	// halflife SDK (CHalfLifeMultiplay::Think): once the game is over, only the intermission runs. SBO ran the round
	// states first, so after the frag/time limit a new bounty round picked a hunted and respawned everyone during
	// intermission.
	if ( g_fGameOver )   // someone else quit the game already
	{
		CHalfLifeMultiplay::Think();
		return;
	}

	if ( m_iRoundState == ROUND_PAUSE )
	{
		if ( m_iNextRoundStep < gpGlobals->time )
			CheckRound();
	}
	else if ( m_iRoundState == ROUND_SETUP )
	{
		SetupRound();
	}
	else if ( m_iRoundState == ROUND_ACTIVE )
	{
		if ( !Bounty )
		{
			m_iNextRoundStep = gpGlobals->time + 5;
			m_iRoundState = ROUND_PAUSE;
			DmRoundHookEnd();	// the hunted left
		}
	}
	else if ( m_iRoundState == ROUND_OVER )
	{
		RoundWinner();
	}

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
int CDm::IsDm( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CDm::InitHUD( CBasePlayer *pPlayer )
{
	pPlayer->m_bNotInGame = 0;

	pPlayer->pev->classname = MAKE_STRING( "observer" );
	pPlayer->pev->solid = SOLID_NOT;
	pPlayer->pev->takedamage = DAMAGE_NO;
	pPlayer->pev->movetype = MOVETYPE_NONE;
	pPlayer->pev->flags |= FL_NOTARGET;
	pPlayer->pev->effects |= EF_NODRAW;
	pPlayer->m_afPhysicsFlags |= PFLAG_OBSERVER;
	pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT | HIDEHUD_HEALTH;
	pPlayer->RemoveAllItems( TRUE );
	pPlayer->m_bOutOfPlay = 1;
	SpectatorStart_Joining( pPlayer );	// SBO 2.0: nobody to watch: the view from an info_intermission point (observer.cpp)

	CHalfLifeMultiplay::InitHUD( pPlayer );
}

//=========================================================
//=========================================================
void CDm::ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		pPlayer->m_bNotInGame = 1;

		if ( pPlayer->m_bHunted == 1 )
		{
			Bounty = 0;
			RoundWinner();
			pPlayer->m_bHunted = 0;
		}
	}

	CHalfLifeMultiplay::ClientDisconnected( pClient );
}

//=========================================================
// SBO 2.0 spectator mode on (CBasePlayer::StartSpectating): the hunted leaves the bounty round as on a disconnect
// (ClientDisconnected): the round ends and the next one, 5 s later, picks a new hunted
//=========================================================
void CDm::PlayerStartsSpectating( CBasePlayer *pPlayer )
{
	if ( pPlayer->m_bHunted == 1 )
	{
		Bounty = 0;
		RoundWinner();
		pPlayer->m_bHunted = 0;
	}
}

//=========================================================
// SBO 2.0 spectator mode off (CBasePlayer::StopSpectating): back in at once with the loadout, as the loadout menu's
// last step ("item") spawns a joining player; without a loadout the team menu opens (briefing, loadout menus)
//=========================================================
void CDm::PlayerStopsSpectating( CBasePlayer *pPlayer )
{
	if ( pPlayer->m_bLoadoutReady == 1 )
	{
		pPlayer->m_iSpawnMode = 2;
		pPlayer->m_bOutOfPlay = 0;
		PlayerJoins( pPlayer );
	}
	else
	{
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, "You left spectator mode: choose a loadout to join the game.\n" );
		ShowVGUI( pPlayer, 2 );	// MENU_TEAM
	}
}

//=========================================================
// SBO 2.0: a player with a loadout joins (the loadout menu's "item" step, spectator mode off). With nobody the hunted
// and no other player in the game, the player is the round's first: the round starts now (SetupRound on the next
// Think, after the menu's remaining commands are in) and spawns the player once, as the hunted. SBO spawned the
// player as a bounty killer and, when the round started (up to 5 s later), again as the hunted.
//=========================================================
void CDm::PlayerJoins( CBasePlayer *pPlayer )
{
	if ( !Bounty && !g_fGameOver )
	{
		BOOL bOthers = FALSE;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pOther = (CBasePlayer *)UTIL_PlayerByIndex( i );

			// the players SetupRound spawns; spectators sit out
			if ( pOther && pOther != pPlayer && !pOther->m_bNotInGame && !pOther->m_bSpectating && pOther->m_bLoadoutReady == 1 )
			{
				bOthers = TRUE;
				break;
			}
		}

		if ( !bOthers )
		{
			m_iRoundState = ROUND_SETUP;
			return;
		}
	}

	respawn( pPlayer->pev, FALSE );
}

//=========================================================
//=========================================================
void CDm::RoundWinner( void )
{
	UTIL_ClientPrintAll( HUD_PRINTCENTER, "The HUNTED IS DEAD !!!\n" );
	EndRound = 0;
	m_iNextRoundStep = gpGlobals->time + 5;
	m_iRoundState = ROUND_PAUSE;
	DmRoundHookEnd();
}

//=========================================================
//=========================================================
void CDm::CheckRound( void )
{
	m_iRoundState = ROUND_SETUP;
	m_iNextRoundStep = 0;
}

//=========================================================
//=========================================================
void CDm::SetupRound( void )
{
	EndRound = 1;

	// SBO 2.0 round hooks: a round starts if anyone spawns (the first of them becomes the hunted). The map's
	// "round_reset" entities go back to their state before anyone spawns (spawn points fire their targets).
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer && !pPlayer->m_bNotInGame && !pPlayer->m_bSpectating && pPlayer->m_bLoadoutReady == 1 )
		{
			RoundHooks_Reset();
			break;
		}
	}

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer && !pPlayer->m_bNotInGame && !pPlayer->m_bSpectating )	// SBO 2.0: spectators sit out
		{
			if ( pPlayer->m_bLoadoutReady == 1 )
			{
				if ( pPlayer->pev->weapons )
					pPlayer->RemoveAllItems( TRUE );

				if ( !Bounty )
				{
					pPlayer->m_bHunted = 1;
					Bounty = 1;
				}

				pPlayer->m_iSpawnMode = 2;
				EntSelectSpawnPoint( pPlayer );
				pPlayer->Spawn();
			}
		}
	}

	if ( !Bounty )
	{
		m_iNextRoundStep = gpGlobals->time + 5;
		m_iRoundState = ROUND_PAUSE;
	}
	else
	{
		m_iNextRoundStep = gpGlobals->time + 0;
		m_iRoundState = ROUND_ACTIVE;
		DmRoundHookStart();
	}
}

//=========================================================
//=========================================================
BOOL CDm::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if ( g_VoiceGameMgr.ClientCommand( pPlayer, pcmd ) )
		return TRUE;

	if ( FStrEq( pcmd, "menuselect" ) )
	{
		if ( CMD_ARGC() < 2 )
			return TRUE;

		int slot = atoi( CMD_ARGV(1) );

		return TRUE;
	}

	if ( FStrEq( pcmd, "changemodel" ) )
	{
		ShowVGUI( pPlayer, 10 );
		return TRUE;
	}

	if ( FStrEq( pcmd, "radio" ) )
	{
		if ( !pPlayer->m_bOutOfPlay )
		{
			pPlayer->m_iTextMenu = 0;
			pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Radio" );
		}
		return TRUE;
	}

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
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iLoadoutHeavy = 1;
			break;
		case 2:
			pPlayer->m_iLoadoutHeavy = 2;
			break;
		case 3:
			pPlayer->m_iLoadoutHeavy = 3;
			break;
		case 4:
			pPlayer->m_iLoadoutHeavy = 4;
			break;
		case 5:
			pPlayer->m_iLoadoutHeavy = 5;
			break;
		case 6:
			pPlayer->m_iLoadoutHeavy = 6;
			break;
		case 7:
			pPlayer->m_iLoadoutHeavy = 7;
			break;
		case 8:
			pPlayer->m_iLoadoutHeavy = 8;
			break;
		case 9:
			pPlayer->m_iLoadoutHeavy = 9;
			break;
		}
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
		// SBO 2.0 spectator mode: confirming the selection menu ("Go !") leaves spectator mode, as this step spawns
		// a waiting player. The spectator spawns (PlayerStopsSpectating) once the commands the menu sends after this
		// one (cyberware, armor, malefemale) are in: UpdateClientData, Spectate_MenuConfirmed.
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
			pPlayer->m_bLoadoutReady = 1;
			pPlayer->m_iLoadoutItem = iItem;

			// a dead or waiting player joins (PlayerLoadoutJoins) once the commands the menu sends after this one
			// (cyberware, armor, malefemale) are in: UpdateClientData. SBO spawned the player here, with the cyberware
			// saved before, so leaving Assassin gave one more Assassin life, then a spawn with the blade only (the
			// Assassin spawn clears the light, heavy and item picks).
			if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
				pPlayer->m_bLoadoutJoinPending = TRUE;
		}
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// SBO 2.0: a dead or waiting player confirmed the loadout menu ("item", ClientCommand) and all its commands are in
// (CBasePlayer::LoadoutJoin): joins with that loadout
//=========================================================
void CDm::PlayerLoadoutJoins( CBasePlayer *pPlayer )
{
	if ( !( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER ) || pPlayer->m_bSpectating )
		return;

	pPlayer->m_iSpawnMode = 2;
	pPlayer->m_bOutOfPlay = 0;
	PlayerJoins( pPlayer );
}

//=========================================================
//=========================================================
void CDm::PlayerSpawn( CBasePlayer *pPlayer )
{
	// halflife-updated: the round loadout always auto-selects, regardless of cl_autowepswitch
	const int originalAutoWepSwitch = pPlayer->m_iAutoWepSwitch;
	pPlayer->m_iAutoWepSwitch = 1;

	BOOL		addDefault = TRUE;

	if ( pPlayer->pev->weapons )
		pPlayer->RemoveAllItems( TRUE );

	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);

	pPlayer->StopObserver();
	ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "Kill them all !!!" );
	pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
	pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
	pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
	pPlayer->m_bOutOfPlay = 0;

	if ( pPlayer->m_bHunted == 1 )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "Run!!! you are the hunted!!" );
	}
	else
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "you are a bounty killer!" );
	}

	if ( addDefault )
		pPlayer->GiveNamedItem( "weapon_wong" );

	pPlayer->m_iSpawnProtectTime = gpGlobals->time + 1;
	pPlayer->pev->fov = pPlayer->m_iFOV = 0;	// SBO 2.0: the player's own FOV (default_fov), not 90
	pPlayer->pev->movetype = MOVETYPE_WALK;
	pPlayer->m_iArmorType = 0;

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
	pPlayer->m_iSboMaxSpeed = pPlayer->m_iSboSpeed;

	if ( pPlayer->m_bHunted == 1 )
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "hunted" );
	else
	{
		switch ( pPlayer->m_iLoadoutModel )
		{
		case 1:
			g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agentf" );
			break;
		case 2:
			g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agent" );
			break;
		default:
			g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agent" );
			break;
		}
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
		pPlayer->m_iSboMaxSpeed += 150;
		break;
	case 2:
		pPlayer->m_bTargeting = 1;
		break;
	case 3:
		pPlayer->GiveNamedItem( "weapon_medikit" );
		break;
	case 4:
		// SBO 2.0: Adrenaline Surge gives +75 health, as in co-op and as #SBO_Item_adrenaline says;
		// 1.38 set 150 here, the spawn health (m_iSboHealth, 100) + 50
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
		pPlayer->m_iSboMaxSpeed += 300;
		pPlayer->pev->health = 150;
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
		pPlayer->GiveAmmo( 30, "rc28m", 900 );
		break;
	case 2:
		pPlayer->GiveNamedItem( "weapon_k444" );
		pPlayer->GiveAmmo( 21, "K444m", 900 );
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->GiveNamedItem( "weapon_canon" );
		pPlayer->GiveAmmo( 10, "canonm", 900 );
		break;
	case 4:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->GiveNamedItem( "weapon_pm66" );
		pPlayer->GiveAmmo( 50, "pm66m", 900 );
		break;
	}

	switch ( pPlayer->m_iLoadoutHeavy )
	{
	case 1:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->GiveNamedItem( "weapon_fa18" );
		pPlayer->GiveAmmo( 50, "fa18m", 900 );
		break;
	case 2:
		pPlayer->m_iSboMaxSpeed -= 20;
		pPlayer->GiveNamedItem( "weapon_federal" );
		pPlayer->GiveAmmo( 30, "federalm", 900 );
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 45;
		pPlayer->GiveNamedItem( "weapon_sw" );
		pPlayer->GiveAmmo( 10, "swm", 900 );
		break;
	case 4:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->GiveNamedItem( "weapon_nm33" );
		pPlayer->GiveAmmo( 30, "nm33m", 900 );
		break;
	case 5:
		pPlayer->m_iSboMaxSpeed -= 20;
		pPlayer->GiveNamedItem( "weapon_spa" );
		pPlayer->GiveAmmo( 8, "spam", 900 );
		break;
	case 6:
		pPlayer->GiveNamedItem( "weapon_ben" );
		pPlayer->m_iSboMaxSpeed -= 20;
		pPlayer->GiveAmmo( 8, "benm", 900 );
		break;
	case 7:
		pPlayer->m_iSboMaxSpeed -= 60;
		pPlayer->GiveNamedItem( "weapon_glauncher" );
		pPlayer->GiveAmmo( 1, "ARgrenades", 20 );
		break;
	case 8:
		pPlayer->m_iSboMaxSpeed -= 80;
		pPlayer->GiveNamedItem( "weapon_chaingun" );
		pPlayer->GiveAmmo( 100, "chainm", 900 );
		break;
	case 9:
		pPlayer->m_iSboMaxSpeed -= 80;
		pPlayer->GiveNamedItem( "weapon_rpg" );
		break;
	}

	g_engfuncs.pfnSetClientMaxspeed( pPlayer->edict(), pPlayer->m_iSboMaxSpeed );

	pPlayer->m_iAutoWepSwitch = originalAutoWepSwitch;
}

//=========================================================
//=========================================================
BOOL CDm::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return TRUE;
}

//=========================================================
//=========================================================
int CDm::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return 1;
}

//=========================================================
//=========================================================
void CDm::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	DeathNotice( pVictim, pKiller, pInflictor );

	pVictim->m_iSpawnMode = 2;
	pVictim->m_bOutOfPlay = 0;

	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );
	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;

	if ( pVictim->pev == pKiller )  
	{  // killed self (`kill`, own grenade / rocket): no points lost (SBO 2.0, owner: a loss only held the match up;
	   // 1.38: -10)
	}
	else if ( ktmp && ktmp->IsPlayer() )
	{
		if ( pVictim->m_bHunted == 1 )
			pKiller->frags += 10;
		else if ( peKiller->m_bHunted == 1 )
			pKiller->frags += 2;
		else
			pKiller->frags += 1;

		FireTargets( "game_playerkill", ktmp, ktmp, USE_TOGGLE, 0 );

		if ( pVictim->m_bHunted == 1 )
		{
			pVictim->m_bHunted = 0;
			peKiller->m_bHunted = 1;
			ClientPrint( peKiller->pev, HUD_PRINTCENTER, "Run!!! you are the hunted!!" );
			g_engfuncs.pfnSetClientKeyValue( ENTINDEX( peKiller->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( peKiller->edict() ), "model", "hunted" );
			Bounty = 1;
		}
	}
	else
	{  // killed by the world or an NPC
		// SBO 2.0: a world death (fall, drowning, trigger_hurt, door: no NPC name, logged as a suicide "(world)") loses
		// no points, as a suicide; an NPC's kill still costs 5 (1.38: -5 for both)
		char szNpcKiller[64];
		if ( SboKillfeed_KillerNpcName( pKiller, pInflictor, szNpcKiller, sizeof( szNpcKiller ) ) )
			pVictim->pev->frags -= 5;
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
}
