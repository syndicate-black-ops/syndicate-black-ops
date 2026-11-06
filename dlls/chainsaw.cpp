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

// SBO: Chainsaw (CChainsaw), server only. Uses the katana models.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"


#define	CHAINSAW_BODYHIT_VOLUME 128
#define	CHAINSAW_WALLHIT_VOLUME 512

class CChainsaw : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 1; }
	void EXPORT SwingAgain( void );
	void EXPORT Smack( void );
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	int Swing( int fFirst );
	BOOL Deploy( void );
	void Holster( void );	// SBO: non-virtual, hides Holster(int)
	void WeaponIdle( void );
	int m_iSwing;
	int m_fIdleSound;
	TraceResult m_trHit;
};

LINK_ENTITY_TO_CLASS( weapon_chainsaw, CChainsaw );



enum chainsaw_e {
	CHAINSAW_IDLE = 0,
	CHAINSAW_DRAW,
	CHAINSAW_HOLSTER,
	CHAINSAW_ATTACK1,
	CHAINSAW_ATTACK2,
	CHAINSAW_ATTACK3,
	CHAINSAW_ATTACK4
};


void CChainsaw::Spawn( )
{
	Precache( );
	m_iId = WEAPON_CHAINSAW;
	SET_MODEL(ENT(pev), "models/w_katana.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}


void CChainsaw::Precache( void )
{
	PRECACHE_MODEL("models/v_katana.mdl");
	PRECACHE_MODEL("models/w_katana.mdl");
	PRECACHE_MODEL("models/p_katana.mdl");
	PRECACHE_SOUND("weapons/chain_deploy.wav");
	PRECACHE_SOUND("weapons/chain_idle.wav");
	PRECACHE_SOUND("weapons/chain_holster.wav");
	PRECACHE_SOUND("weapons/chain_hit1.wav");
	PRECACHE_SOUND("weapons/chain_hit2.wav");
	PRECACHE_SOUND("weapons/chain_miss.wav");
	PRECACHE_SOUND("weapons/chain_hitbody1.wav");
	PRECACHE_SOUND("weapons/chain_hitbody2.wav");
}

int CChainsaw::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 2;
	p->iId = WEAPON_CHAINSAW;
	p->iWeight = 5;
	return 1;
}

int CChainsaw::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CChainsaw::Deploy( )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 1;
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/chain_deploy.wav", 1, ATTN_NORM, 0, PITCH_NORM);
	m_fIdleSound = 1;
	// SBO: dead stores; they leave a frame slot and the unreferenced doubles 0.52/0.55 in .rodata (egcs -O1
	// keeps constant-pool entries of deleted insns). Exact original expression unknown.
	float flUnused = gpGlobals->time + 0.52;
	flUnused = gpGlobals->time + 0.55;
	return DefaultDeploy( "models/v_katana.mdl", "models/p_katana.mdl", CHAINSAW_DRAW, "crowbar" );
}

void CChainsaw::Holster( void )
{
	STOP_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/chain_idle.wav");
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/chain_holster.wav", 1, ATTN_NORM, 0, PITCH_NORM);
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	m_flTimeWeaponIdle = gpGlobals->time + 0.5;
	m_fIdleSound = 0;
	SendWeaponAnim( CHAINSAW_HOLSTER );
}


void FindHullIntersectionChainsaw( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	int			i, j, k;
	float		distance;
	float		*minmaxs[2] = {mins, maxs};
	TraceResult tmpTrace;
	Vector		vecHullEnd = tr.vecEndPos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if ( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}


void CChainsaw::PrimaryAttack()
{
	if (! Swing( 1 ))
	{
		m_fIdleSound = 0;
		SetThink( SwingAgain );
		pev->nextthink = gpGlobals->time + 0.5;
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;
	}
}


void CChainsaw::Smack( )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}


void CChainsaw::SwingAgain( void )
{
	Swing( 0 );
}


int CChainsaw::Swing( int fFirst )
{
	int fDidHit = FALSE;

	TraceResult tr;

	UTIL_MakeVectors (m_pPlayer->pev->v_angle);
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 56;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersectionChainsaw( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			switch( (m_iSwing++) % 3 )
			{
			case 0:
				SendWeaponAnim( CHAINSAW_ATTACK3 ); break;
			case 1:
				SendWeaponAnim( CHAINSAW_ATTACK1 ); break;
			case 2:
				SendWeaponAnim( CHAINSAW_ATTACK2 ); break;
			case 3:
				SendWeaponAnim( CHAINSAW_ATTACK4 ); break;
			}

			// miss
			m_flNextPrimaryAttack = gpGlobals->time + 0.5;
			m_flTimeWeaponIdle = gpGlobals->time + 0.5;
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/chain_miss.wav", 1, ATTN_NORM, 0, PITCH_NORM);

			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		// hit
		fDidHit = TRUE;
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		switch( ((m_iSwing++) % 2) + 1 )
		{
		case 0:
			SendWeaponAnim( CHAINSAW_ATTACK3 ); break;
		case 1:
			SendWeaponAnim( CHAINSAW_ATTACK1 ); break;
		case 2:
			SendWeaponAnim( CHAINSAW_ATTACK2 ); break;
		case 3:
			SendWeaponAnim( CHAINSAW_ATTACK4 ); break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		gSkillData.gargantuaDmgSlash = RANDOM_FLOAT( 44, 110 );

		ClearMultiDamage( );

		if ( (m_flNextPrimaryAttack + 1 < gpGlobals->time ) || g_pGameRules->IsMultiplayer() )
		{
			// first swing does full damage
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.gargantuaDmgSlash, gpGlobals->v_forward, &tr, DMG_CLUB );
		}
		else
		{
			// subsequent swings do the same
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.gargantuaDmgSlash, gpGlobals->v_forward, &tr, DMG_CLUB );
		}
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				switch( RANDOM_LONG(0,2) )
				{
				case 0:
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/chain_hitbody1.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
				case 1:
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/chain_hitbody1.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
				case 2:
					EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/chain_hitbody1.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
				}
				m_pPlayer->m_iWeaponVolume = CHAINSAW_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
					  return TRUE;
				else
					  flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CROWBAR);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer,
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play chainsaw strike
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/chain_hit1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
		}

		// delay the decal a bit
		m_trHit = tr;

		SetThink( Smack );
		pev->nextthink = gpGlobals->time + 0.5;
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;

		m_pPlayer->m_iWeaponVolume = flVol * CHAINSAW_WALLHIT_VOLUME;
		m_fIdleSound = 1;
	}
	return fDidHit;
}


void CChainsaw::WeaponIdle( void )
{
	if ( m_fIdleSound )
	{
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/chain_idle.wav", 1, 0.3, 0, 110);
		m_fIdleSound = 0;
	}

	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	SendWeaponAnim( CHAINSAW_IDLE );
	m_flTimeWeaponIdle = gpGlobals->time + 3.0f;
}
