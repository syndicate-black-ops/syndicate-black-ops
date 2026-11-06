//=========================================================
// SBO: deathmatch / bounty hunt game rules (default mode)
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

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgScoreInfo;
extern CVoiceGameMgr	g_VoiceGameMgr;

extern int m_Changeniv;
extern int m_SpawnLocate;
extern cvar_t timeleft, fragsleft;

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );
extern void respawn( entvars_t *pev, BOOL fCopyCorpse );
extern void ShowVGUI( CBasePlayer *pPlayer, int iMenu );

int Bounty;		// someone is the hunted
int EndRound;

//=========================================================
//=========================================================
CDm::CDm()
{
	m_Changeniv = 0;
	m_iRoundState = 0;
	m_iParticip = 0;
	m_iLive = 0;
	m_iRoundTime = -1;
	Bounty = 0;
	EndRound = 1;
	m_SpawnLocate = 1;
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

	if ( m_iRoundState == 0 )
	{
		m_iParticip = 0;

		if ( m_iRoundTime < gpGlobals->time )
			CheckRound();
	}
	else if ( m_iRoundState == 4 )
	{
		m_iParticip = 0;
		SetupRound();
	}
	else if ( m_iRoundState == 3 )
	{
		if ( !Bounty )
		{
			m_iRoundTime = gpGlobals->time + 5;
			m_iRoundState = 0;
		}
	}
	else if ( m_iRoundState == 1 )
	{
		RoundWinner();
		m_iParticip = 0;
		m_iLive = 0;
	}

	if ( g_fGameOver )   // someone else quit the game already
	{
		CHalfLifeMultiplay::Think();
		return;
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
	pPlayer->m_iSbo2a8 = 0;

	pPlayer->pev->classname = MAKE_STRING( "observer" );
	pPlayer->pev->solid = SOLID_NOT;
	pPlayer->pev->takedamage = DAMAGE_NO;
	pPlayer->pev->movetype = MOVETYPE_NONE;
	pPlayer->pev->flags |= FL_NOTARGET;
	pPlayer->pev->effects |= EF_NODRAW;
	pPlayer->m_afPhysicsFlags |= PFLAG_OBSERVER;
	pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT | HIDEHUD_HEALTH;
	pPlayer->RemoveAllItems( TRUE );
	pPlayer->m_iSpectator = 1;

	CHalfLifeMultiplay::InitHUD( pPlayer );
}

//=========================================================
//=========================================================
void CDm::ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		pPlayer->m_iSbo2a8 = 1;

		if ( pPlayer->m_iSbo300 == 1 )
		{
			Bounty = 0;
			RoundWinner();
			pPlayer->m_iSbo300 = 0;
		}
	}

	CHalfLifeMultiplay::ClientDisconnected( pClient );
}

//=========================================================
//=========================================================
void CDm::RoundWinner( void )
{
	UTIL_ClientPrintAll( HUD_PRINTCENTER, "The HUNTED IS DEAD !!!\n" );
	EndRound = 0;
	m_iRoundTime = gpGlobals->time + 5;
	m_iRoundState = 0;
}

//=========================================================
//=========================================================
void CDm::CheckRound( void )
{
	m_iRoundState = 4;
	m_iRoundTime = 0;
}

//=========================================================
//=========================================================
void CDm::SetupRound( void )
{
	m_iParticip = 0;
	EndRound = 1;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer && !pPlayer->m_iSbo2a8 )
		{
			m_iParticip++;

			if ( pPlayer->m_iSbo2e8 == 1 )
			{
				if ( pPlayer->pev->weapons )
					pPlayer->RemoveAllItems( TRUE );

				if ( !Bounty )
				{
					pPlayer->m_iSbo300 = 1;
					Bounty = 1;
				}

				pPlayer->m_iSbo2ec = 2;
				EntSelectSpawnPoint( pPlayer );
				pPlayer->Spawn();
			}
		}
	}

	if ( !Bounty )
	{
		m_iRoundTime = gpGlobals->time + 5;
		m_iRoundState = 0;
	}
	else
	{
		m_iRoundTime = gpGlobals->time + 0;
		m_iRoundState = 3;
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
		if ( !pPlayer->m_iSpectator )
		{
			pPlayer->m_iSbo318 = 0;
			pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Radio" );
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "malefemale" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2ac = 1;
			break;
		case 2:
			pPlayer->m_iSbo2ac = 2;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "blade" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2b0 = 1;
			break;
		case 2:
			pPlayer->m_iSbo2b0 = 2;
			break;
		case 3:
			pPlayer->m_iSbo2b0 = 3;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "light" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2b4 = 1;
			break;
		case 2:
			pPlayer->m_iSbo2b4 = 2;
			break;
		case 3:
			pPlayer->m_iSbo2b4 = 3;
			break;
		case 4:
			pPlayer->m_iSbo2b4 = 4;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "heavy" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2b8 = 1;
			break;
		case 2:
			pPlayer->m_iSbo2b8 = 2;
			break;
		case 3:
			pPlayer->m_iSbo2b8 = 3;
			break;
		case 4:
			pPlayer->m_iSbo2b8 = 4;
			break;
		case 5:
			pPlayer->m_iSbo2b8 = 5;
			break;
		case 6:
			pPlayer->m_iSbo2b8 = 6;
			break;
		case 7:
			pPlayer->m_iSbo2b8 = 7;
			break;
		case 8:
			pPlayer->m_iSbo2b8 = 8;
			break;
		case 9:
			pPlayer->m_iSbo2b8 = 9;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "armor" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2bc = 1;
			break;
		case 2:
			pPlayer->m_iSbo2bc = 2;
			break;
		case 3:
			pPlayer->m_iSbo2bc = 3;
			break;
		case 4:
			pPlayer->m_iSbo2bc = 0;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "cyberware" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2c0 = 1;
			break;
		case 2:
			pPlayer->m_iSbo2c0 = 2;
			break;
		case 3:
			pPlayer->m_iSbo2c0 = 3;
			break;
		case 4:
			pPlayer->m_iSbo2c0 = 4;
			break;
		case 5:
			pPlayer->m_iSbo2c0 = 5;
			break;
		case 6:
			pPlayer->m_iSbo2c0 = 6;
			break;
		}
		return TRUE;
	}

	if ( FStrEq( pcmd, "item" ) )
	{
		switch ( atoi( CMD_ARGV(1) ) )
		{
		case 1:
			pPlayer->m_iSbo2e8 = 1;
			pPlayer->m_iSbo2c4 = 1;
			if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
			{
				pPlayer->m_iSbo2ec = 2;
				pPlayer->m_iSpectator = 0;
				respawn( pPlayer->pev, FALSE );
			}
			break;
		case 2:
			pPlayer->m_iSbo2e8 = 1;
			pPlayer->m_iSbo2c4 = 2;
			if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
			{
				pPlayer->m_iSbo2ec = 2;
				pPlayer->m_iSpectator = 0;
				respawn( pPlayer->pev, FALSE );
			}
			break;
		case 3:
			pPlayer->m_iSbo2e8 = 1;
			pPlayer->m_iSbo2c4 = 3;
			if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
			{
				pPlayer->m_iSbo2ec = 2;
				pPlayer->m_iSpectator = 0;
				respawn( pPlayer->pev, FALSE );
			}
			break;
		case 4:
			pPlayer->m_iSbo2e8 = 1;
			pPlayer->m_iSbo2c4 = 4;
			if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
			{
				pPlayer->m_iSpectator = 0;
				pPlayer->m_iSbo2ec = 2;
				respawn( pPlayer->pev, FALSE );
			}
			break;
		}
		return TRUE;
	}

	return FALSE;
}

//=========================================================
//=========================================================
void CDm::PlayerSpawn( CBasePlayer *pPlayer )
{
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
	pPlayer->m_iSpectator = 0;

	if ( pPlayer->m_iSbo300 == 1 )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "Run!!! you are the hunted!!" );
	}
	else
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCENTER, "you are a bounty killer!" );
	}

	if ( addDefault )
		pPlayer->GiveNamedItem( "weapon_wong" );

	pPlayer->m_iSbo2f4 = gpGlobals->time + 1;
	pPlayer->pev->fov = pPlayer->m_iFOV = 90;
	pPlayer->pev->movetype = MOVETYPE_WALK;
	pPlayer->m_iSbo2d4 = 0;

	if ( pPlayer->m_iSbo2c0 == 5 )
		pPlayer->m_iCanCloak = 1;
	else
		pPlayer->m_iCanCloak = 0;

	if ( pPlayer->m_iSbo2c0 == 6 || pPlayer->m_iSbo2c0 == 5 )
		pPlayer->m_iCanCybereye = 1;
	else
		pPlayer->m_iCanCybereye = 0;

	if ( pPlayer->m_iSbo2c0 == 2 || pPlayer->m_iSbo2c0 == 5 )
		pPlayer->m_iSbo2e0 = 1;
	else
		pPlayer->m_iSbo2e0 = 0;

	pPlayer->pev->renderamt = 255;
	pPlayer->pev->renderfx = kRenderFxNone;
	pPlayer->pev->rendermode = kRenderNormal;
	pPlayer->pev->gravity = 1;
	pPlayer->m_iSboMaxSpeed = pPlayer->m_iSboSpeed;

	if ( pPlayer->m_iSbo300 == 1 )
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "hunted" );
	else
	{
		switch ( pPlayer->m_iSbo2ac )
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

	switch ( pPlayer->m_iSbo2bc )
	{
	case 1:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->pev->armorvalue = 50;
		pPlayer->m_iSbo2d4 = 3;
		break;
	case 2:
		pPlayer->m_iSboMaxSpeed -= 15;
		pPlayer->pev->armorvalue = 100;
		pPlayer->m_iSbo2d4 = 1;
		break;
	case 3:
		pPlayer->m_iSboMaxSpeed -= 30;
		pPlayer->pev->armorvalue = 100;
		pPlayer->m_iSbo2d4 = 2;
		break;
	}

	switch ( pPlayer->m_iSbo2c0 )
	{
	case 1:
		pPlayer->m_iSboMaxSpeed += 150;
		break;
	case 2:
		pPlayer->m_iSbo2e0 = 1;
		break;
	case 3:
		pPlayer->GiveNamedItem( "weapon_medikit" );
		break;
	case 4:
		pPlayer->pev->health = 150;
		break;
	case 5:
		pPlayer->m_iSbo2b4 = 0;
		pPlayer->m_iSbo2b8 = 0;
		pPlayer->m_iSbo2c0 = 5;
		pPlayer->m_iSbo2c4 = 0;
		pPlayer->GiveNamedItem( "weapon_K444" );
		pPlayer->m_iCanCloak = 1;
		pPlayer->m_iCanCybereye = 1;
		pPlayer->m_iSbo2e0 = 1;
		pPlayer->GiveNamedItem( "weapon_medikit" );
		pPlayer->pev->armorvalue = 50;
		pPlayer->m_iSbo2d4 = 3;
		pPlayer->m_iSboMaxSpeed += 300;
		pPlayer->pev->health = 150;
		pPlayer->GiveNamedItem( "weapon_handgrenade" );
		pPlayer->GiveNamedItem( "weapon_satchel" );
		break;
	case 6:
		pPlayer->m_iCanCybereye = 1;
		break;
	}

	switch ( pPlayer->m_iSbo2c4 )
	{
	case 1:
		pPlayer->GiveNamedItem( "weapon_handgrenade" );
		break;
	case 2:
		pPlayer->m_iSboMaxSpeed -= 6;
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

	switch ( pPlayer->m_iSbo2b0 )
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

	switch ( pPlayer->m_iSbo2b4 )
	{
	case 1:
		pPlayer->GiveNamedItem( "weapon_rc28" );
		pPlayer->GiveAmmo( 30, "rc28m", 900 );
		break;
	case 2:
		pPlayer->GiveNamedItem( "weapon_K444" );
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

	switch ( pPlayer->m_iSbo2b8 )
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

	pVictim->m_iSbo2ec = 2;
	m_iLive--;
	pVictim->m_iSpectator = 0;

	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );
	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;

	if ( pVictim->pev == pKiller )  
	{  // killed self
		pKiller->frags -= 10;
	}
	else if ( ktmp && ktmp->IsPlayer() )
	{
		if ( pVictim->m_iSbo300 == 1 )
			pKiller->frags += 10;
		else if ( peKiller->m_iSbo300 == 1 )
			pKiller->frags += 2;
		else
			pKiller->frags += 1;

		FireTargets( "game_playerkill", ktmp, ktmp, USE_TOGGLE, 0 );

		if ( pVictim->m_iSbo300 == 1 )
		{
			pVictim->m_iSbo300 = 0;
			peKiller->m_iSbo300 = 1;
			ClientPrint( peKiller->pev, HUD_PRINTCENTER, "Run!!! you are the hunted!!" );
			g_engfuncs.pfnSetClientKeyValue( ENTINDEX( peKiller->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( peKiller->edict() ), "model", "hunted" );
			Bounty = 1;
		}
	}
	else
	{  // killed by the world
		pVictim->pev->frags -= 5;
	}

	// update the scores
	// killed scores
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );
		WRITE_SHORT( pVictim->pev->frags );
		WRITE_SHORT( pVictim->m_iDeaths );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	// killers score, if it's a player
	CBaseEntity *ep = CBaseEntity::Instance( pKiller );
	if ( ep && ep->Classify() == CLASS_PLAYER )
	{
		CBasePlayer *PK = (CBasePlayer*)ep;

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(PK->edict()) );
			WRITE_SHORT( PK->pev->frags );
			WRITE_SHORT( PK->m_iDeaths );
			WRITE_SHORT( 0 );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}
}
