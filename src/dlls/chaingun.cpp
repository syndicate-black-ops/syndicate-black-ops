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
// SBO: minigun (CChaingun)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"

enum chaingun_e
{
	CHAINGUN_IDLE1 = 0,
	CHAINGUN_IDLE2,
	CHAINGUN_SPINUP,
	CHAINGUN_SPINDOWN,
	CHAINGUN_FIRE,
	CHAINGUN_DRAW,
	CHAINGUN_RELOAD,
};

// SBO 2.0: the clip is 500 on the client too, as on the server and in the description (#SBO_Item_chaingun). 1.38's
// client predicted 300 (from 2003; the 2005 server raised it to 500), so its predicted reload and clip were wrong.
#define CHAINGUN_MAX_CLIP		500
#define CHAINGUN_DEFAULT_GIVE	300
#define CHAINGUN_MAX_CARRY		900
#define CHAINGUN_WEIGHT			10

LINK_ENTITY_TO_CLASS( weapon_chaingun, CChaingun );


void CChaingun::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_chaingun");
	Precache( );
	SET_MODEL(ENT(pev), "models/w_mini.mdl");
	m_iId = WEAPON_CHAINGUN;

	m_iDefaultAmmo = CHAINGUN_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CChaingun::Precache( void )
{
	PRECACHE_MODEL("models/v_mini.mdl");
	PRECACHE_MODEL("models/w_mini.mdl");
	PRECACHE_MODEL("models/p_mini.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell

	PRECACHE_SOUND("weapons/gat_fire1.wav");
	PRECACHE_SOUND("weapons/gat_spinup.wav");
	PRECACHE_SOUND("weapons/gat_spindown.wav");
	PRECACHE_SOUND("weapons/357_cock1.wav");

	m_usChaingun = PRECACHE_EVENT( 1, "events/chaingun.sc" );
}

int CChaingun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "chainm";
	p->iMaxAmmo1 = CHAINGUN_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = CHAINGUN_MAX_CLIP;
	p->iSlot = 3;
	p->iPosition = 2;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_CHAINGUN;
	p->iWeight = CHAINGUN_WEIGHT;

	return 1;
}

int CChaingun::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CChaingun::Deploy( )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;
	return DefaultDeploy( "models/v_mini.mdl", "models/p_mini.mdl", CHAINGUN_DRAW, "mp5" );
}


void CChaingun::PrimaryAttack( void )
{
	// SBO 2.0: an empty clip neither fires nor leaves the looping fire sequence (the client's event starts it) running
	// while the button is held: a spinning barrel spins down, as on release; after that the empty click, once per
	// press (WeaponIdle resets it), as the other guns do.
	if ( m_iClip <= 0 )
	{
		if ( m_fInAttack )
		{
			SendWeaponAnim( CHAINGUN_SPINDOWN );
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gat_spindown.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
			m_fInAttack = 0;
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1;
		}
		else
		{
			PlayEmptySound( );
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;
		}
		m_flTimeWeaponIdle = m_flNextPrimaryAttack;
		return;
	}

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;

	if ( m_fInAttack == 0 && m_iClip > 0 )
	{
		SendWeaponAnim( CHAINGUN_SPINUP );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gat_spinup.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.4;
		m_fInAttack = 1;
	}
	else
	{
		slowdown( );

		if ( m_iShots == 0 )
			m_iShots = 5;

		if ( m_iShots < 22 )
			m_iShots++;

		m_fInAttack = 1;

		if ( TRUE && m_iClip > 0 )
			ChaingunFire( 0.04, 1 / (float)m_iShots, TRUE, TRUE );
	}
}


void CChaingun::ChaingunFire( float flSpread, float flCycleTime, int fUseAutoAim, int iShots )
{
	if (m_iClip <= 0)
	{
		SendWeaponAnim( CHAINGUN_IDLE1 );
		PlayEmptySound( );
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;
		return;
	}

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_iClip--;

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gat_fire1.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming;

	if ( fUseAutoAim )
	{
		vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	}
	else
	{
		vecAiming = gpGlobals->v_forward;
	}

	Vector vecDir = m_pPlayer->FireBulletsPlayer( 3, vecSrc, vecAiming, VECTOR_CONE_20DEGREES, 8192, 8, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
	flags = FEV_NOTHOST;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usChaingun, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	// halflife #3252: counted from the time the shot was due, so the gun fires at this rate at any frame rate
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.04 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}


void CChaingun::slowdown( void )
{
	if ( m_iClip )
	{
		Vector vecDir = Vector( m_pPlayer->pev->velocity.x, m_pPlayer->pev->velocity.y, 0 ).Normalize();

		if ( m_pPlayer->pev->velocity.Length2D() > 50 )
			m_pPlayer->pev->velocity = Vector( vecDir.x * 50, vecDir.y * 50, m_pPlayer->pev->velocity.z );
	}
}


void CChaingun::Reload( void )
{
	if ( m_iClip <= 99 )
	{
		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 0 )
			return;

		DefaultReload( CHAINGUN_MAX_CLIP, CHAINGUN_RELOAD, 1.5 );
		SetThink( &CChaingun::BringUp );
		pev->nextthink = UTIL_WeaponTimeBase() + 1.5;
	}
}


void CChaingun::BringUp( void )
{
	SendWeaponAnim( CHAINGUN_DRAW );
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.0;
}


void CChaingun::WeaponIdle( void )
{
	m_flSpinTime = 0;
	m_iShots = 0;

	if ( m_fInAttack == 1 )
	{
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1;
		m_flTimeWeaponIdle = m_flNextPrimaryAttack;
		SendWeaponAnim( CHAINGUN_SPINDOWN );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gat_spindown.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		m_fInAttack = 0;
	}
	else
	{
		if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
			return;

		ResetEmptySound( );

		m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

		int iAnim;
		switch ( UTIL_SharedRandomLong( m_pPlayer->random_seed, 0, 1 ) )
		{
		case 0:	
			iAnim = CHAINGUN_IDLE1;	
			break;
		
		default:
		case 1:
			iAnim = CHAINGUN_IDLE2;
			break;
		}

		SendWeaponAnim( iAnim );

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 ); // how long till we do this again.
	}
}
