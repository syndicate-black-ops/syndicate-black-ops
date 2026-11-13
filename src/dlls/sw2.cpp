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
// SBO: SW sniper rifle (CSw)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"

enum sw_e
{
	SW_IDLE1 = 0,
	SW_IDLE2,
	SW_FIRE1,
	SW_FIRE2,
	SW_RELOAD,
	SW_ANIM5,
	SW_ANIM6,
	SW_DRAW,
	SW_HOLSTER,
};

#define SW_MAX_CLIP			5
#define SW_DEFAULT_GIVE		5
#define SW_MAX_CARRY		900
#define SW_WEIGHT			10

LINK_ENTITY_TO_CLASS( weapon_sw, CSw );


void CSw::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_sw");
	Precache( );
	m_iId = WEAPON_SW;
	SET_MODEL(ENT(pev), "models/w_rifle.mdl");

	m_iDefaultAmmo = SW_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


int CSw::AddToPlayer( CBasePlayer *pPlayer )
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


void CSw::Precache( void )
{
	PRECACHE_SOUND("weapons/zoom.wav");

	PRECACHE_MODEL("models/w_rifle.mdl");
	PRECACHE_MODEL("models/v_rifle.mdl");
	PRECACHE_MODEL("models/p_rifle.mdl");

	PRECACHE_SOUND("weapons/sw_fire1.wav");
	PRECACHE_SOUND("weapons/zong.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	m_usSw = PRECACHE_EVENT( 1, "events/sw.sc" );
}

int CSw::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "swm";
	p->iMaxAmmo1 = SW_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = SW_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 2;
	p->iId = WEAPON_SW;
	p->iFlags = 0;
	p->iWeight = SW_WEIGHT;

	return 1;
}


void CSw::ItemPostFrame( void )
{
	if ( m_pPlayer->pev->button & IN_JUMP )
	{
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.01;

		if ( ( m_pPlayer->pev->fov = m_pPlayer->m_iFOV != 0 ) != 0 )
		{
			m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
			m_iZoomState = 2;
			m_fInZoom = 0;
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}


BOOL CSw::Deploy( )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;
	m_iZoomState = 2;
	m_fInZoom = 0;
	return DefaultDeploy( "models/v_rifle.mdl", "models/p_rifle.mdl", SW_DRAW, "mp5" );
}


void CSw::Holster( void )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_fInZoom = 0;

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.3;

	if ( m_iClip )
		SendWeaponAnim( SW_HOLSTER );
}


void CSw::PrimaryAttack()
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

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/sw_fire1.wav", RANDOM_FLOAT(0.8, 0.9), ATTN_NORM);

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

#ifndef CLIENT_DLL
	// SBO: server only (the 2003 client has none of this)
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 8192, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr );
	vecAiming = tr.vecEndPos - vecSrc;
	vecAiming = vecAiming.Normalize();

	PLAYBACK_EVENT_FULL( 0, ENT(m_pPlayer->pev), m_usSw, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );

	if ( m_fInZoom == 1 )
	{
		Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( 0, 0, 0 ), 8192, BULLET_PLAYER_SW2, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_DLIGHT );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_BYTE( 50 );	// radius
			WRITE_BYTE( 138 );	// r
			WRITE_BYTE( 249 );	// g
			WRITE_BYTE( 227 );	// b
			WRITE_BYTE( 2 );	// life
			WRITE_BYTE( 0 );	// decay
		MESSAGE_END();

		m_flNextPrimaryAttack = GetNextAttackDelay( 1.0 );
	}
	else
	{
		CPlasma::Shoot( m_pPlayer->pev, vecSrc, vecAiming * 2000, 1 );
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.5 );
	}
#else
	if ( m_fInZoom == 1 )
		m_flNextPrimaryAttack = GetNextAttackDelay( 1.0 );
	else
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.5 );
#endif

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}


void CSw::SecondaryAttack( void )
{
	switch ( m_iZoomState )
	{
	case 1:
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
		g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), m_pPlayer->m_iSboMaxSpeed );
		m_fInZoom = 0;
		m_iZoomState = 2;
		break;
	case 2:
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 30;
		g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), 80 );
		m_fInZoom = 1;
		m_iZoomState = 3;
		break;
	case 3:
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 10;
		g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), 80 );
		m_fInZoom = 1;
		m_iZoomState = 1;
		break;
	default:
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
		g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), m_pPlayer->m_iSboMaxSpeed );
		m_fInZoom = 0;
		m_iZoomState = 2;
		break;
	}

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.3;
}


void CSw::Reload( void )
{
	g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), m_pPlayer->m_iSboMaxSpeed );
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_fInZoom = 0;
	m_iZoomState = 2;

	DefaultReload( SW_MAX_CLIP, SW_RELOAD, 2.2 );
}


void CSw::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_2DEGREES );

	ResetEmptySound( );

	if ( m_flTimeWeaponIdle < UTIL_WeaponTimeBase() )
	{
		// the idle sequence's length: sent once per loop
		if ( m_iClip )
		{
			SendWeaponAnim( SW_IDLE1 );
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0 / 10.0;
		}
		else
		{
			SendWeaponAnim( SW_IDLE2 );
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 16.0 / 25.0;
		}
	}
}
