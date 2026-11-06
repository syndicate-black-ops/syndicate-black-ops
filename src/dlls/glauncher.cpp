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

// SBO: grenade launcher (CGlauncher)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"

enum glauncher_e
{
	GLAUNCHER_LONGIDLE = 0,
	GLAUNCHER_IDLE1,
	GLAUNCHER_FIRE1,
	GLAUNCHER_FIRE2,
	GLAUNCHER_RELOAD,
	GLAUNCHER_DEPLOY,
};

#define GLAUNCHER_MAX_CLIP		2
#define GLAUNCHER_DEFAULT_GIVE	2
#define GLAUNCHER_MAX_CARRY		20
#define GLAUNCHER_WEIGHT		10

LINK_ENTITY_TO_CLASS( weapon_glauncher, CGlauncher );


void CGlauncher::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_glauncher");
	Precache( );
	SET_MODEL(ENT(pev), "models/w_glauncher.mdl");
	m_iId = WEAPON_GLAUNCHER;

	m_iDefaultAmmo = GLAUNCHER_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CGlauncher::Precache( void )
{
	PRECACHE_MODEL("models/v_glauncher.mdl");
	PRECACHE_MODEL("models/w_glauncher.mdl");
	PRECACHE_MODEL("models/p_glauncher.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell

	PRECACHE_SOUND("weapons/glauncher_fire1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	PRECACHE_MODEL("models/grenade.mdl");	// grenade

	PRECACHE_SOUND("weapons/357_cock1.wav");
}

int CGlauncher::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "ARgrenades";
	p->iMaxAmmo1 = GLAUNCHER_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = GLAUNCHER_MAX_CLIP;
	p->iSlot = 3;
	p->iPosition = 1;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLAUNCHER;
	p->iWeight = GLAUNCHER_WEIGHT;

	return 1;
}

int CGlauncher::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CGlauncher::Deploy( )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;
	return DefaultDeploy( "models/v_glauncher.mdl", "models/p_glauncher.mdl", GLAUNCHER_DEPLOY, "mp5" );
}


void CGlauncher::PrimaryAttack()
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
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
	m_pPlayer->m_flStopExtraSoundTime = UTIL_WeaponTimeBase() + 0.2;

	m_iClip--;

	if ( 1 )
	{
		SendWeaponAnim( RANDOM_LONG(0,2) + GLAUNCHER_FIRE1 );
		m_flNextAnimTime = UTIL_WeaponTimeBase() + 0.1;
	}

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/glauncher_fire1.wav", 0.8, ATTN_NORM);

 	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	// we don't add in player velocity anymore.
	CGrenade::ShootContact( m_pPlayer->pev, 
							m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16, 
							gpGlobals->v_forward * 800 );

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;

	m_pPlayer->pev->punchangle.x -= 5;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT( 10, 15 );
}


void CGlauncher::SecondaryAttack( void )
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
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
	m_pPlayer->m_flStopExtraSoundTime = UTIL_WeaponTimeBase() + 0.2;

	m_iClip--;

	if ( 1 )
	{
		SendWeaponAnim( RANDOM_LONG(0,2) + GLAUNCHER_FIRE2 );
		m_flNextAnimTime = UTIL_WeaponTimeBase() + 0.1;
	}

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/glauncher_fire1.wav", 0.8, ATTN_NORM);

 	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	// we don't add in player velocity anymore.
	CGrenade::ShootContact2( m_pPlayer->pev, 
							m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16, 
							gpGlobals->v_forward * 800 );

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;

	m_pPlayer->pev->punchangle.x -= 5;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT( 10, 15 );
}


void CGlauncher::Reload( void )
{
	// SBO 2.0: reloads with one grenade still loaded (1.38 passed 1 here, so only an empty launcher reloaded)
	DefaultReload( GLAUNCHER_MAX_CLIP, GLAUNCHER_RELOAD, 4.0 );
}


void CGlauncher::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:	
		iAnim = GLAUNCHER_LONGIDLE;	
		break;
	
	default:
	case 1:
		iAnim = GLAUNCHER_IDLE1;
		break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT( 10, 15 ); // how long till we do this again.
}
