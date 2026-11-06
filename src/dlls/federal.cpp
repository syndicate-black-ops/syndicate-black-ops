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
// SBO: Federal sniper rifle (CFederal)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"

enum federal_e
{
	FEDERAL_IDLE = 0,
	FEDERAL_RELOAD,
	FEDERAL_DRAW,
	FEDERAL_FIRE1,
};

#define FEDERAL_MAX_CLIP		30
#define FEDERAL_DEFAULT_GIVE	30
#define FEDERAL_MAX_CARRY		900
#define FEDERAL_WEIGHT			10

LINK_ENTITY_TO_CLASS( weapon_federal, CFederal );


void CFederal::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_federal");
	Precache( );
	m_iId = WEAPON_FEDERAL;
	SET_MODEL(ENT(pev), "models/w_federal.mdl");

	m_iDefaultAmmo = FEDERAL_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


int CFederal::AddToPlayer( CBasePlayer *pPlayer )
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


void CFederal::Precache( void )
{
	PRECACHE_SOUND("weapons/zoom.wav");

	PRECACHE_MODEL("models/w_federal.mdl");
	PRECACHE_MODEL("models/v_federal.mdl");
	PRECACHE_MODEL("models/p_federal.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell

	PRECACHE_SOUND("weapons/federal_fire1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	m_usFederal = PRECACHE_EVENT( 1, "events/federal.sc" );
}

int CFederal::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "federalm";
	p->iMaxAmmo1 = FEDERAL_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = FEDERAL_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 1;
	p->iId = WEAPON_FEDERAL;
	p->iFlags = 0;
	p->iWeight = FEDERAL_WEIGHT;

	return 1;
}


void CFederal::ItemPostFrame( void )
{
	if ( m_pPlayer->pev->button & IN_JUMP )
	{
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.1;

		if ( ( m_pPlayer->pev->fov = m_pPlayer->m_iFOV != 0 ) != 0 )
		{
			m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
			m_fInZoom = 0;
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}


BOOL CFederal::Deploy( )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;
	m_fInZoom = 0;
	return DefaultDeploy( "models/v_federal.mdl", "models/p_federal.mdl", FEDERAL_DRAW, "shotgun" );
}


void CFederal::Holster( void )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	if ( m_fInZoom )
	{
		SecondaryAttack( );
	}

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if ( m_iClip )
		SendWeaponAnim( FEDERAL_DRAW );
}


void CFederal::PrimaryAttack()
{
	if (m_iClip <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;
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
		flSpreadX = 0;
		flSpreadY = 0;
		flSpreadZ = 0;
	}

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/federal_fire1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( flSpreadX, flSpreadY, flSpreadZ ), 8192, 13, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFederal, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.05;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 10;
}


void CFederal::SecondaryAttack( void )
{
	if ( m_fInZoom )
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/zoom.wav", RANDOM_FLOAT(0.9, 1.0), ATTN_NORM);
		g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), m_pPlayer->m_iSboMaxSpeed );
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
		m_fInZoom = 0;
	}
	else
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/zoom.wav", RANDOM_FLOAT(0.9, 1.0), ATTN_NORM);
		g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), 80 );
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 30;
		m_fInZoom = 1;
	}

	pev->nextthink = UTIL_WeaponTimeBase() + 0.1;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.3;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
}


void CFederal::Reload( void )
{
	if ( m_fInZoom )
	{
		SecondaryAttack( );
	}

	if ( DefaultReload( FEDERAL_MAX_CLIP, FEDERAL_RELOAD, 3.0, 8 ) )
	{
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "items/cliprelease1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xf));
	}
}


void CFederal::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );

	ResetEmptySound( );

	if ( m_flTimeWeaponIdle < UTIL_WeaponTimeBase() )
		SendWeaponAnim( FEDERAL_IDLE );
}
