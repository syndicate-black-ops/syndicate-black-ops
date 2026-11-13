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
// SBO: double-barrel shotgun (CCanon)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"

enum canon_e {
	CANON_IDLE = 0,
	CANON_FIRE,
	CANON_FIRE2,
	CANON_RELOAD,
	CANON_DRAW,
	CANON_HOLSTER,
	CANON_IDLE2,
};

#define CANON_MAX_CLIP		2
#define CANON_DEFAULT_GIVE	2
#define CANON_MAX_CARRY		900
#define CANON_WEIGHT		8

LINK_ENTITY_TO_CLASS( weapon_canon, CCanon );

void CCanon::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_canon");
	Precache( );
	m_iId = WEAPON_CANON;
	SET_MODEL(ENT(pev), "models/w_canon.mdl");

	m_iDefaultAmmo = CANON_DEFAULT_GIVE;

	FallInit();// get ready to fall
}


void CCanon::Precache( void )
{
	PRECACHE_MODEL("models/v_canon.mdl");
	PRECACHE_MODEL("models/w_canon.mdl");
	PRECACHE_MODEL("models/p_canon.mdl");

	m_iShell = PRECACHE_MODEL ("models/shotgunshell.mdl");// shotgun shell

	PRECACHE_SOUND ("weapons/2can_fire1.wav");
	PRECACHE_SOUND ("weapons/2can_fire2.wav");

	m_usCanon1 = PRECACHE_EVENT( 1, "events/canon1.sc" );
	m_usCanon2 = PRECACHE_EVENT( 1, "events/canon2.sc" );
}

int CCanon::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}


int CCanon::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "canonm";
	p->iMaxAmmo1 = CANON_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = CANON_MAX_CLIP;
	p->iSlot = 1;
	p->iPosition = 2;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_CANON;
	p->iWeight = CANON_WEIGHT;

	return 1;
}



BOOL CCanon::Deploy( )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.95;
	return DefaultDeploy( "models/v_canon.mdl", "models/p_canon.mdl", CANON_DRAW, "shotgun" );
}

void CCanon::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		Reload( );
		if (m_iClip == 0)
			PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/2can_fire1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0x1f));

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	Vector vecDir = m_pPlayer->FireBulletsPlayer( 6, vecSrc, vecAiming, VECTOR_CONE_8DEGREES, 2048, 11, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usCanon1, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	// halflife #3252: counted from the time the shot was due, so the gun fires at this rate at any frame rate
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.5 );
	m_flNextSecondaryAttack = GetNextAttackDelay( 0.75 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
}


void CCanon::SecondaryAttack( void )
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	if (m_iClip <= 1)
	{
		Reload( );
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip -= 2;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/2can_fire2.wav", RANDOM_FLOAT(0.98, 1.0), ATTN_NORM, 0, 85 + RANDOM_LONG(0,0x1f));

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	Vector vecDir = m_pPlayer->FireBulletsPlayer( 12, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, 2048, 11, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usCanon2, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	// halflife #3252: counted from the time the shot was due, so the gun fires at this rate at any frame rate
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.5 );
	m_flNextSecondaryAttack = GetNextAttackDelay( 0.75 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
}


void CCanon::Reload( void )
{
	int iResult;

	if (m_iClip == 0)
		iResult = DefaultReload( CANON_MAX_CLIP, CANON_RELOAD, 1.67 );
	else
		iResult = DefaultReload( CANON_MAX_CLIP, CANON_RELOAD, 1.67 );

	if (iResult)
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	}
}


void CCanon::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	switch ( UTIL_SharedRandomLong( m_pPlayer->random_seed, 0, 1 ) )
	{
	case 0:	
		iAnim = CANON_IDLE2;	
		break;
	
	default:
	case 1:
		iAnim = CANON_IDLE2;
		break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 ); // how long till we do this again.
}
