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
// SBO: PM66 pistol (CPm66)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"

enum pm66_e
{
	PM66_IDLE = 0,
	PM66_RELOAD,
	PM66_DRAW,
};

#define PM66_MAX_CLIP		50
#define PM66_DEFAULT_GIVE	50
#define PM66_MAX_CARRY		900
#define PM66_WEIGHT			8

LINK_ENTITY_TO_CLASS( weapon_pm66, CPm66 );


void CPm66::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_pm66");
	Precache( );
	m_iId = WEAPON_PM66;
	SET_MODEL(ENT(pev), "models/w_pm66.mdl");

	m_iDefaultAmmo = PM66_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


int CPm66::AddToPlayer( CBasePlayer *pPlayer )
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


void CPm66::Precache( void )
{
	PRECACHE_MODEL("models/w_pm66.mdl");
	PRECACHE_MODEL("models/v_pm66.mdl");
	PRECACHE_MODEL("models/p_pm66.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell

	PRECACHE_SOUND("weapons/pm66_fire1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	m_usPm66_2 = PRECACHE_EVENT( 1, "events/pm66_2.sc" );
	m_usPm66 = PRECACHE_EVENT( 1, "events/pm66.sc" );
}


int CPm66::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "pm66m";
	p->iMaxAmmo1 = PM66_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = PM66_MAX_CLIP;
	p->iSlot = 1;
	p->iPosition = 3;
	p->iId = WEAPON_PM66;
	p->iFlags = 0;
	p->iWeight = PM66_WEIGHT;

	return 1;
}


BOOL CPm66::Deploy( )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;
	return DefaultDeploy( "models/v_pm66.mdl", "models/p_pm66.mdl", PM66_DRAW, "onehanded" );
}


void CPm66::PrimaryAttack()
{
	if (m_iClip <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;
		return;
	}

	float flSpreadX, flSpreadY, flSpreadZ;
	flSpreadX = 0.02618;
	flSpreadY = 0.02618;
	flSpreadZ = 0.02618;

	if ( m_pPlayer->pev->button & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
	{
		flSpreadX = 0.03490;
		flSpreadY = 0.03490;
		flSpreadZ = 0.03490;
	}

	if ( m_pPlayer->pev->button & IN_DUCK )
	{
		flSpreadX = 0.00873;
		flSpreadY = 0.00873;
		flSpreadZ = 0.00873;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/pm66_fire1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( flSpreadX, flSpreadY, flSpreadZ ), 8192, 14, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usPm66, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.04;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT( 10, 15 );
}


void CPm66::SecondaryAttack()
{
	if (m_iClip <= 1)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;
		return;
	}

	float flSpreadX, flSpreadY, flSpreadZ;
	flSpreadX = 0.03490;
	flSpreadY = 0.03490;
	flSpreadZ = 0.03490;

	if ( m_pPlayer->pev->button & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
	{
		flSpreadX = 0.04362;
		flSpreadY = 0.04362;
		flSpreadZ = 0.04362;
	}

	if ( m_pPlayer->pev->button & IN_DUCK )
	{
		flSpreadX = 0.02618;
		flSpreadY = 0.02618;
		flSpreadZ = 0.02618;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip -= 2;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/pm66_fire1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 2, vecSrc, vecAiming, Vector( flSpreadX, flSpreadY, flSpreadZ ), 8192, 14, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usPm66_2, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.02;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT( 10, 15 );
}


void CPm66::Reload( void )
{
	if ( DefaultReload( PM66_MAX_CLIP, PM66_RELOAD, 1.5 ) )
	{
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "items/cliprelease1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xf));
	}
}


void CPm66::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );

	ResetEmptySound( );

	if ( m_flTimeWeaponIdle < UTIL_WeaponTimeBase() )
		SendWeaponAnim( PM66_IDLE );
}
