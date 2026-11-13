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
// SBO: K444 pistol (CK444)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"

enum k444_e
{
	K444_IDLE1 = 0,
	K444_IDLE2,
	K444_IDLE3,
	K444_SHOOT,
	K444_RELOAD,
	K444_SHOOT_EMPTY,
	K444_DRAW,
	K444_HOLSTER,
};

#define K444_MAX_CLIP		4
#define K444_DEFAULT_GIVE	7
#define K444_MAX_CARRY		900
#define K444_WEIGHT			8

LINK_ENTITY_TO_CLASS( weapon_k444, CK444 );


int CK444::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "K444m";
	p->iMaxAmmo1 = K444_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = K444_MAX_CLIP;
	p->iFlags = 0;
	p->iSlot = 1;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_K444;
	p->iWeight = K444_WEIGHT;

	return 1;
}

int CK444::AddToPlayer( CBasePlayer *pPlayer )
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

void CK444::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_k444");
	Precache( );
	m_iId = WEAPON_K444;
	SET_MODEL(ENT(pev), "models/w_k444.mdl");

	m_iDefaultAmmo = K444_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CK444::Precache( void )
{
	PRECACHE_MODEL("models/v_k444.mdl");
	PRECACHE_MODEL("models/w_k444.mdl");
	PRECACHE_MODEL("models/p_k444.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("weapons/k444_fire1.wav");

	m_usK444 = PRECACHE_EVENT( 1, "events/k444.sc" );
}

BOOL CK444::Deploy( )
{
	m_fFired = 0;
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;
	return DefaultDeploy( "models/v_k444.mdl", "models/p_k444.mdl", K444_DRAW, "onehanded" );
}

void CK444::Holster( void )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 10 + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 5 );
	SendWeaponAnim( K444_HOLSTER );
}

void CK444::PrimaryAttack( void )
{
	if ( m_fFired == 1 )
		return;

	if (m_iClip <= 0)
	{
		Reload( );
		if (m_iClip == 0)
			PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_iClip--;

	float flSpreadX, flSpreadY, flSpreadZ;
	flSpreadX = 0.00873;
	flSpreadY = 0.00873;
	flSpreadZ = 0.00873;

	if ( m_pPlayer->pev->button & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
	{
		flSpreadX = 0.02618;
		flSpreadY = 0.02618;
		flSpreadZ = 0.02618;
	}

	if ( m_pPlayer->pev->button & IN_DUCK )
	{
		flSpreadX = 0;
		flSpreadY = 0;
		flSpreadZ = 0;
	}

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/k444_fire1.wav", RANDOM_FLOAT(0.8, 0.9), 0.8);

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( flSpreadX, flSpreadY, flSpreadZ ), 8192, 10, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usK444, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.13;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_fFired = 1;
}


void CK444::Reload( void )
{
	DefaultReload( K444_MAX_CLIP, K444_RELOAD, 2.0 );
	m_fFired = 0;
}


void CK444::WeaponIdle( void )
{
	ResetEmptySound( );
	m_fFired = 0;

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	SendWeaponAnim( K444_IDLE1 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 31.0 / 15.0;	// the idle sequence's length: sent once per loop
}
