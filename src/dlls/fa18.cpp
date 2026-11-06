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
// SBO: FA-18 (CFa18)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"

enum fa18_e
{
	FA18_IDLE1 = 0,
	FA18_IDLE2,
	FA18_RELOAD,
	FA18_DEPLOY,
	FA18_FIRE1,
	FA18_FIRE2,
	FA18_FIRE3,
};

#define FA18_MAX_CLIP		50
#define FA18_DEFAULT_GIVE	50
#define FA18_MAX_CARRY		900
#define FA18_WEIGHT			10

LINK_ENTITY_TO_CLASS( weapon_fa18, CFa18 );


void CFa18::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_fa18");
	Precache( );
	SET_MODEL(ENT(pev), "models/w_fa18.mdl");
	m_iId = WEAPON_FA18;

	m_iDefaultAmmo = FA18_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CFa18::Precache( void )
{
	PRECACHE_MODEL("models/v_fa18.mdl");
	PRECACHE_MODEL("models/w_fa18.mdl");
	PRECACHE_MODEL("models/p_fa18.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell

	PRECACHE_SOUND("weapons/jack_fire1.wav");
	PRECACHE_SOUND("weapons/fa18_fire1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	m_usFa18_1 = PRECACHE_EVENT( 1, "events/fa18_1.sc" );
	m_usFa18_2 = PRECACHE_EVENT( 1, "events/fa18_2.sc" );
}

int CFa18::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "fa18m";
	p->iMaxAmmo1 = FA18_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = FA18_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_FA18;
	p->iWeight = FA18_WEIGHT;

	return 1;
}

int CFa18::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CFa18::Deploy( )
{
#ifndef CLIENT_DLL
	// SBO: an unused local in the 2005 server; only its stack slot survives (sub esp, 4).
	// Reconstructed: a dead float computed from a double needs a rounding temp (the frame slot); the constant 0.6
	// is known from the unreferenced double it left in .rodata (egcs -O1 keeps pool entries of deleted insns).
	float flTemp = gpGlobals->time + 0.6;
#endif
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;
	return DefaultDeploy( "models/v_fa18.mdl", "models/p_fa18.mdl", FA18_DEPLOY, "shotgun" );
}


void CFa18::PrimaryAttack()
{
	if (m_iClip <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

	float flSpreadX, flSpreadY, flSpreadZ;
	flSpreadX = 0.01745;
	flSpreadY = 0.01745;
	flSpreadZ = 0.01745;

	if ( m_pPlayer->pev->button & ( IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_LEFT | IN_RIGHT ) )
	{
		flSpreadX = 0.02618;
		flSpreadY = 0.02618;
		flSpreadZ = 0.02618;
	}

	if ( m_pPlayer->pev->button & IN_DUCK )
	{
		flSpreadX = 0.00873;
		flSpreadY = 0.00873;
		flSpreadZ = 0.00873;
	}

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/fa18_fire1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( flSpreadX, flSpreadY, flSpreadZ ), 8192, 6, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFa18_1, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.06;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.2;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT( 10, 15 );
}


void CFa18::SecondaryAttack( void )
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	if (m_iClip <= 3)
	{
		Reload( );
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip -= 4;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/jack_fire1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 4, vecSrc, vecAiming, VECTOR_CONE_8DEGREES, 8192, 6, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFa18_2, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.2;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT( 10, 15 );
}


void CFa18::Reload( void )
{
	DefaultReload( FA18_MAX_CLIP, FA18_RELOAD, 2.6 );
}


void CFa18::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	SendWeaponAnim( FA18_IDLE1 );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT( 10, 15 ); // how long till we do this again.
}
