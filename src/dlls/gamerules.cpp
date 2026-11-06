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
//=========================================================
// GameRules.cpp
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include	"teamplay_gamerules.h"
#include	"coop.h"
#include	"br.h"
#include	"dm.h"
#include	"skill.h"
#include	"game.h"

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );

DLL_GLOBAL CGameRules*	g_pGameRules = NULL;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgMOTD;

int g_teamplay = 0;
int m_Mode;

//=========================================================
//=========================================================
BOOL CGameRules::CanHaveAmmo( CBasePlayer *pPlayer, const char *pszAmmoName, int iMaxCarry )
{
	int iAmmoIndex;

	if ( pszAmmoName )
	{
		iAmmoIndex = pPlayer->GetAmmoIndex( pszAmmoName );

		if ( iAmmoIndex > -1 )
		{
			if ( pPlayer->AmmoInventory( iAmmoIndex ) < iMaxCarry )
			{
				// player has room for more of this type of ammo
				return TRUE;
			}
		}
	}

	return FALSE;
}

//=========================================================
//=========================================================
edict_t *CGameRules :: GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );

	pPlayer->pev->origin = VARS(pentSpawnSpot)->origin + Vector(0,0,1);
	pPlayer->pev->v_angle  = g_vecZero;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->angles = VARS(pentSpawnSpot)->angles;
	pPlayer->pev->punchangle = g_vecZero;
	pPlayer->pev->fixangle = TRUE;
	
	return pentSpawnSpot;
}

//=========================================================
//=========================================================
BOOL CGameRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	// only living players can have items
	if ( pPlayer->pev->deadflag != DEAD_NO )
		return FALSE;

	if ( pWeapon->pszAmmo1() )
	{
		if ( !CanHaveAmmo( pPlayer, pWeapon->pszAmmo1(), pWeapon->iMaxAmmo1() ) )
		{
			// we can't carry anymore ammo for this gun. We can only 
			// have the gun if we aren't already carrying one of this type
			if ( pPlayer->HasPlayerItem( pWeapon ) )
			{
				return FALSE;
			}
		}
	}
	else
	{
		// weapon doesn't use ammo, don't take another if you already have it.
		if ( pPlayer->HasPlayerItem( pWeapon ) )
		{
			return FALSE;
		}
	}

	// note: will fall through to here if GetItemInfo doesn't fill the struct!
	return TRUE;
}

//=========================================================
// load the SkillData struct with the proper values based on the skill level.
//=========================================================
void CGameRules::RefreshSkillData ( void )
{
	int	iSkill;

	iSkill = (int)CVAR_GET_FLOAT("skill");
	g_iSkillLevel = iSkill;

	if ( iSkill < 1 )
	{
		iSkill = 3;
	}
	else if ( iSkill > 3 )
	{
		iSkill = 3; 
	}

	gSkillData.iSkillLevel = 3;

	ALERT ( at_console, "\nGAME SKILL LEVEL:%d\n",iSkill );
}

//=========================================================
// instantiate the proper game rules object
//=========================================================

CGameRules *InstallGameRules( void )
{
	// SBO 2.0: the shipped game.cfg sets nothing any more (the cvar defaults in game.cpp replace it). The exec stays,
	// as in the HL SDK: it runs at once, before server.cfg / listenserver.cfg, and before the engine's Steam init on the first map.
	SERVER_COMMAND( "exec game.cfg\n" );
	SERVER_EXECUTE( );

	// SBO 2.0: a server nobody named is not called "Half-Life" (the engine's default of hostname, which is what a listen
	// server started with +map keeps: listenserver.cfg sets no name, so that the Create Server dialog's name applies).
	// Checked here, before the server.cfg / listenserver.cfg exec queued by the game rules has run, so a hostname
	// in those files, or one typed after the map command, still applies afterwards; a name already set (Create
	// Server dialog, +hostname, the player's config) is not the engine's default and is left alone.
	if ( !strcmp( CVAR_GET_STRING( "hostname" ), "Half-Life" ) )
	{
		CVAR_SET_STRING( "hostname", "Syndicate Black Ops Server" );
	}

	m_Mode = 0;

	const char *szMapName = STRING( gpGlobals->mapname );
	if ( !strncmp( szMapName, "sp_", 3 ) )
	{
		m_Mode = MODE_TEAMPLAY;
		return new CHalfLifeTeamplay;
	}

	const char *szMapName2 = STRING( gpGlobals->mapname );
	if ( !strncmp( szMapName2, "co_", 3 ) )
	{
		m_Mode = MODE_COOP;
		return new CCoop;
	}

	const char *szMapName3 = STRING( gpGlobals->mapname );
	if ( !strncmp( szMapName3, "br_", 3 ) )
	{
		m_Mode = MODE_BR;
		return new CBr;
	}

	m_Mode = MODE_DM;
	return new CDm;
}

int CGameRules::GetPartie( void )
{
	return m_Mode;
}
