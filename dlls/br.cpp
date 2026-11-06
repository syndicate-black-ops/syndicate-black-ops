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

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgScoreInfo;
extern CVoiceGameMgr	g_VoiceGameMgr;

extern int m_Changeniv;
extern int m_SpawnLocate;
extern cvar_t timeleft, fragsleft;

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );
extern void respawn( entvars_t *pev, BOOL fCopyCorpse );

//=========================================================
//=========================================================
CBr::CBr()
{
	m_Changeniv = 0;
	m_iRoundState = 0;
	m_iParticip = 0;
	m_iLive = 0;
	m_iRoundTime = -1;
	m_SpawnLocate = 1;
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

	if ( m_iRoundState == 0 )
	{
		m_iParticip = 0;
		m_iLive = 0;

		if ( m_iRoundTime < gpGlobals->time )
			CheckRound();
	}

	if ( m_iRoundState == 4 )
	{
		m_iParticip = 0;
		m_iLive = 0;
		SetupRound();
	}

	if ( m_iRoundState == 3 )
	{
		if ( m_iParticip < 2 )
		{
			m_iParticip = 0;
			m_iLive = 0;
			m_iRoundTime = gpGlobals->time + 10;
			m_iRoundState = 0;
			return;
		}

		if ( m_iLive <= 1 )
		{
			m_iParticip = 0;
			m_iLive = 0;
			m_iRoundTime = 0;
			m_iRoundState = 1;
			return;
		}
	}

	if ( m_iRoundState == 1 )
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
int CBr::IsBr( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CBr::InitHUD( CBasePlayer *pPlayer )
{
	pPlayer->m_iSbo2a8 = 0;
	m_Changeniv = 0;

	pPlayer->pev->classname = MAKE_STRING( "observer" );
	pPlayer->pev->solid = SOLID_NOT;
	pPlayer->pev->takedamage = DAMAGE_NO;
	pPlayer->pev->movetype = MOVETYPE_NONE;
	pPlayer->pev->flags |= FL_NOTARGET;
	pPlayer->pev->effects |= EF_NODRAW;
	pPlayer->m_afPhysicsFlags |= PFLAG_OBSERVER;
	pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT | HIDEHUD_HEALTH;
	pPlayer->RemoveAllItems( TRUE );
	pPlayer->m_iSbo2e8 = 0;

	if ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER )
	{
		pPlayer->m_iSbo318 = 1;
		pPlayer->ShowMenu( 0x3ff, 0, FALSE, "#Br" );
	}

	CHalfLifeMultiplay::InitHUD( pPlayer );
}

//=========================================================
//=========================================================
void CBr::ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		pPlayer->m_iSbo2a8 = 1;

		if ( m_iRoundState == 3 )
		{
			m_iParticip--;
			if ( !pPlayer->m_iSpectator )
				m_iLive--;
			CheckRoundOver();
		}
		else
		{
			m_iParticip--;
			CheckRound();
		}
	}

	CHalfLifeMultiplay::ClientDisconnected( pClient );
}

//=========================================================
//=========================================================
void CBr::RoundWinner( void )
{
	UTIL_ClientPrintAll( HUD_PRINTCENTER, "ROUND OVER\n" );

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer && !pPlayer->m_iSbo2a8 && !pPlayer->m_iSpectator )
		{
			pPlayer->pev->frags += 3;

			MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
				WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
				WRITE_SHORT( pPlayer->pev->frags );
				WRITE_SHORT( pPlayer->m_iDeaths );
				WRITE_SHORT( 0 );
				WRITE_SHORT( 0 );
			MESSAGE_END();
		}
	}

	m_iRoundTime = gpGlobals->time + 5;
	m_iRoundState = 0;
}

//=========================================================
//=========================================================
void CBr::CheckRound( void )
{
	m_iRoundState = 4;
	m_iRoundTime = 0;
}

//=========================================================
//=========================================================
void CBr::SetupRound( void )
{
	m_iParticip = 0;
	m_iLive = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer && !pPlayer->m_iSbo2a8 )
		{
			if ( pPlayer->pev->weapons )
				pPlayer->RemoveAllItems( TRUE );

			m_iParticip++;

			if ( pPlayer->m_iSbo2e8 == 1 )
				m_iLive++;
		}
	}

	if ( m_iLive < m_iParticip )
	{
		m_iParticip = 0;
		m_iLive = 0;
		m_iRoundTime = gpGlobals->time + 10;
		m_iRoundState = 0;
	}
	else
	{
		CheckWho();
	}
}

//=========================================================
//=========================================================
void CBr::CheckWho( void )
{
	m_iParticip = 0;
	m_iLive = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer && !pPlayer->m_iSbo2a8 )
		{
			if ( pPlayer->pev->weapons )
				pPlayer->RemoveAllItems( TRUE );

			m_iParticip++;

			if ( pPlayer->m_iSbo2e8 == 1 )
			{
				m_iLive++;
				pPlayer->m_iSbo2ec = 2;
				EntSelectSpawnPoint( pPlayer );
				pPlayer->StopObserver();
				pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
				pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
				pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
				pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
				pPlayer->m_iSpectator = 0;
				UTIL_ClientPrintAll( HUD_PRINTCENTER, "ROUND START\n" );
				pPlayer->Spawn();
			}
		}
	}

	m_iRoundTime = gpGlobals->time + 0;
	m_iRoundState = 3;
}

//=========================================================
//=========================================================
void CBr::CheckRoundOver( void )
{
	if ( m_iParticip >= 2 && m_iLive <= 1 )
	{
		m_iParticip = 0;
		m_iLive = 0;
		m_iRoundTime = 0;
		m_iRoundState = 1;
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

		switch ( pPlayer->m_iSbo318 )
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
					pPlayer->m_iSbo2ec = 1;
					pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
					pPlayer->m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
					pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
					pPlayer->m_afPhysicsFlags &= ~PFLAG_OBSERVER;
					pPlayer->m_iSpectator = 1;
					pPlayer->m_iSbo2e8 = 1;
					respawn( pPlayer->pev, FALSE );
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
	BOOL		addDefault;
	CBaseEntity	*pWeaponEntity = NULL;

	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);
	
	addDefault = TRUE;

	while ( pWeaponEntity = UTIL_FindEntityByClassname( pWeaponEntity, "game_player_equip" ))
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = FALSE;
	}

	if ( pPlayer->m_iSbo2ec == 0 )
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
		pPlayer->m_iSpectator = 1;
		addDefault = FALSE;
	}
	else if ( pPlayer->m_iSbo2ec == 1 )
	{
		pPlayer->m_iSpectator = 1;
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
		pPlayer->m_iSpectator = 0;
	}

	if ( addDefault )
		pPlayer->GiveNamedItem( "weapon_wong" );

	pPlayer->m_iSbo2f4 = gpGlobals->time + 2;
	pPlayer->pev->movetype = MOVETYPE_WALK;
	pPlayer->pev->fov = pPlayer->m_iFOV = 90;
	pPlayer->m_iSbo2d4 = 0;
	pPlayer->m_iCanCloak = 0;
	pPlayer->m_iCanCybereye = 0;
	pPlayer->m_iSbo2e0 = 0;
	pPlayer->pev->renderamt = 255;
	pPlayer->pev->renderfx = kRenderFxNone;
	pPlayer->pev->rendermode = kRenderNormal;
	pPlayer->pev->gravity = 1;
	pPlayer->m_iSboMaxSpeed = pPlayer->m_iSboSpeed;

	pPlayer->m_iSbo2c8 = RANDOM_FLOAT( 1, 3 );
	switch ( pPlayer->m_iSbo2c8 )
	{
	case 1:
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agentf" );
		break;
	case 2:
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agent" );
		break;
	case 3:
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "agent" );
		break;
	}

	pPlayer->m_iSbo2d0 = RANDOM_FLOAT( 1, 41 );
	switch ( pPlayer->m_iSbo2d0 )
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
		pPlayer->pev->fov = pPlayer->m_iFOV = 90;
		g_engfuncs.pfnSetClientMaxspeed( pPlayer->edict(), 500 );
		pPlayer->pev->armorvalue = 150;
		pPlayer->m_iSbo2d4 = 2;
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
		pPlayer->GiveNamedItem( "weapon_K444" );
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
	case 41:
		pPlayer->GiveNamedItem( "weapon_sword" );
		break;
	}
}

//=========================================================
//=========================================================
BOOL CBr::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	pPlayer->m_iSpectator = 1;

	edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );
	pPlayer->StartObserver( VARS(pentSpawnSpot)->origin, VARS(pentSpawnSpot)->angles );

	return FALSE;
}

//=========================================================
//=========================================================
int CBr::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	if ( m_iLive <= 1 )
		return 5;

	return 1;
}

//=========================================================
//=========================================================
void CBr::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	if ( m_iRoundState == 3 )
	{
		pVictim->m_iSbo2ec = 1;
		m_iLive--;
		pVictim->m_iSpectator = 1;
		CheckRoundOver();
	}

	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );
}
