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
// SBO: NM33 sniper rifle (CNm33)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"

enum nm33_e
{
	NM33_IDLE = 0,
	NM33_ANIM1,
	NM33_ANIM2,
	NM33_RELOAD,
	NM33_DRAW,
};

#define NM33_MAX_CLIP		30
#define NM33_DEFAULT_GIVE	30
#define NM33_MAX_CARRY		900
#define NM33_WEIGHT			10

LINK_ENTITY_TO_CLASS( weapon_nm33, CNm33 );


void CNm33::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_nm33");
	Precache( );
	m_iId = WEAPON_NM33;
	SET_MODEL(ENT(pev), "models/w_nm33.mdl");

	m_iDefaultAmmo = NM33_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


int CNm33::AddToPlayer( CBasePlayer *pPlayer )
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


void CNm33::Precache( void )
{
	PRECACHE_SOUND("weapons/zoom.wav");
	PRECACHE_MODEL("models/w_nm33.mdl");
	PRECACHE_MODEL("models/v_nm33.mdl");
	PRECACHE_MODEL("models/p_nm33.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell

	PRECACHE_SOUND("weapons/nm33_fire1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	m_usNm33 = PRECACHE_EVENT( 1, "events/nm33.sc" );
}


int CNm33::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "nm33m";
	p->iMaxAmmo1 = NM33_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = NM33_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 3;
	p->iId = WEAPON_NM33;
	p->iFlags = 0;
	p->iWeight = NM33_WEIGHT;

	return 1;
}


BOOL CNm33::Deploy( )
{
	m_fFired = 0;
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;
	m_iZoomLevel = 1;
	m_fInZoom = 0;
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
	return DefaultDeploy( "models/v_nm33.mdl", "models/p_nm33.mdl", NM33_DRAW, "mp5" );
}


void CNm33::Holster( void )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.3;
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_iZoomLevel = 1;

	if ( m_iClip )
		SendWeaponAnim( NM33_DRAW );
}


void CNm33::ItemPostFrame( void )
{
	if ( m_pPlayer->pev->button & IN_JUMP )
	{
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.1;
		if ( ( m_pPlayer->pev->fov = m_pPlayer->m_iFOV != 0 ) != 0 )
		{
			m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
			m_iZoomLevel = 1;
			m_fInZoom = 0;
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}


void CNm33::PrimaryAttack()
{
	if ( m_fFired == 1 )
		return;

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

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/nm33_fire1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xf));

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( flSpreadX, flSpreadY, flSpreadZ ), 8192, 12, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usNm33, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_fFired = 1;
}


void CNm33::SecondaryAttack( void )
{
	if ( m_iZoomLevel == 0 )
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/zoom.wav", RANDOM_FLOAT(0.9, 1.0), ATTN_NORM);
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
		m_fInZoom = 0;
		m_iZoomLevel = 1;
		g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), m_pPlayer->m_iSboMaxSpeed );
	}
	else if ( m_iZoomLevel == 1 )
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/zoom.wav", RANDOM_FLOAT(0.9, 1.0), ATTN_NORM);
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 20;
		m_fInZoom = 1;
		m_iZoomLevel = 2;
		g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), 80 );
	}
	else if ( m_iZoomLevel == 2 )
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/zoom.wav", RANDOM_FLOAT(0.9, 1.0), ATTN_NORM);
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 10;
		m_fInZoom = 1;
		m_iZoomLevel = 0;
		g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), 80 );
	}

	(void)(pev->nextthink = UTIL_WeaponTimeBase() + 0.1);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.3;
}


void CNm33::Reload( void )
{
	g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), m_pPlayer->m_iSboMaxSpeed );
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_iZoomLevel = 1;
	m_fInZoom = 0;
	m_fFired = 0;
	DefaultReload( NM33_MAX_CLIP, NM33_RELOAD, 2.2 );
}


void CNm33::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );

	ResetEmptySound( );
	m_fFired = 0;

	if ( m_flTimeWeaponIdle < UTIL_WeaponTimeBase() )
	{
		SendWeaponAnim( NM33_IDLE );
		// a two-frame still: sent again only now and then (stock HL's idle time)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	}
}
