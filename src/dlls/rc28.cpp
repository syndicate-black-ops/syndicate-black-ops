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
// SBO: RC28 pistol (CRc28)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"


enum rc28_e {
	RC28_IDLE1 = 0,
	RC28_ANIM1,
	RC28_ANIM2,
	RC28_ANIM3,
	RC28_ANIM4,
	RC28_RELOAD,
	RC28_DRAW,
};

#define RC28_MAX_CLIP		30
#define RC28_DEFAULT_GIVE	30
#define RC28_MAX_CARRY		900
#define RC28_WEIGHT			8

LINK_ENTITY_TO_CLASS( weapon_rc28, CRc28 );

int CRc28::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "rc28m";
	p->iMaxAmmo1 = RC28_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = RC28_MAX_CLIP;
	p->iFlags = 0;
	p->iSlot = 1;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_RC28;
	p->iWeight = RC28_WEIGHT;

	return 1;
}

int CRc28::AddToPlayer( CBasePlayer *pPlayer )
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

void CRc28::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_rc28"); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_RC28;
	SET_MODEL(ENT(pev), "models/w_rc28.mdl");

	m_iDefaultAmmo = RC28_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CRc28::Precache( void )
{
	PRECACHE_MODEL("models/v_rc28.mdl");
	PRECACHE_MODEL("models/w_rc28.mdl");
	PRECACHE_MODEL("models/p_rc28.mdl");


	PRECACHE_SOUND ("weapons/rc28_fire1.wav");

	m_usRc28_1 = PRECACHE_EVENT( 1, "events/rc28_1.sc" );
	m_usRc28_2 = PRECACHE_EVENT( 1, "events/rc28_2.sc" );
}

BOOL CRc28::Deploy( )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;
	m_fFired = 0;
	return DefaultDeploy( "models/v_rc28.mdl", "models/p_rc28.mdl", RC28_DRAW, "onehanded" );
}


void CRc28::Holster( void )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 10 + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 5 );
	SendWeaponAnim( RC28_DRAW );
}

void CRc28::PrimaryAttack()
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

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation (SBO 2.0: missing in 1.38, other players saw no third person shot)
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/rc28_fire1.wav", RANDOM_FLOAT(0.8, 0.9), ATTN_NORM);

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( flSpreadX, flSpreadY, flSpreadZ ), 8192, 9, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usRc28_1, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.08;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_fFired = 1;
}


void CRc28::SecondaryAttack()
{
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
		flSpreadX = 0.01745;
		flSpreadY = 0.01745;
		flSpreadZ = 0.01745;
	}

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	// player "shoot" animation (SBO 2.0: missing in 1.38, other players saw no third person shot)
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/rc28_fire1.wav", RANDOM_FLOAT(0.8, 0.9), ATTN_NORM);

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( flSpreadX, flSpreadY, flSpreadZ ), 8192, 9, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usRc28_2, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	// SBO 2.0: the semi-automatic primary shot waits for it too (it holds the secondary back 0.25 s). 1.38 set only
	// the secondary time: with both buttons held the primary fired on the next frame, two shots at once.
	// halflife #3252: counted from the time the shot was due (GetNextAttackDelay)
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( 0.08 );
}


void CRc28::Reload( void )
{
	DefaultReload( RC28_MAX_CLIP, RC28_RELOAD, 2.0 );
}


void CRc28::WeaponIdle( void )
{
	m_fFired = 0;

	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	SendWeaponAnim( RC28_IDLE1 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 31.0 / 16.0;	// the idle sequence's length: sent once per loop
}
