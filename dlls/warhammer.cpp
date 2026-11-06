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

// SBO: Warhammer (CWarhammer), server only.

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"


#define	WARHAMMER_BODYHIT_VOLUME 128
#define	WARHAMMER_WALLHIT_VOLUME 512

class CWarhammer : public CBasePlayerWeapon
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
	int m_iSwing;
	int m_iWhiteSprite;
	int m_iGlowSprite;
	TraceResult m_trHit;
};

LINK_ENTITY_TO_CLASS( weapon_warhammer, CWarhammer );



enum warhammer_e {
	WARHAMMER_IDLE = 0,
	WARHAMMER_DRAW,
	WARHAMMER_HOLSTER,
	WARHAMMER_ATTACK1HIT,
	WARHAMMER_ATTACK1MISS,
	WARHAMMER_ATTACK2MISS,
	WARHAMMER_ATTACK2HIT,
	WARHAMMER_ATTACK3MISS,
	WARHAMMER_ATTACK3HIT
};


void CWarhammer::Spawn( )
{
	Precache( );
	m_iId = WEAPON_WARHAMMER;
	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}


void CWarhammer::Precache( void )
{
	PRECACHE_MODEL("models/v_warhammer.mdl");
	PRECACHE_MODEL("models/w_crowbar.mdl");
	PRECACHE_MODEL("models/p_warhammer.mdl");
	PRECACHE_SOUND("weapons/cbar_hit1.wav");
	PRECACHE_SOUND("weapons/cbar_hit2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod1.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod3.wav");
	PRECACHE_SOUND("weapons/cbar_miss1.wav");
	PRECACHE_SOUND("weapons/warhammer.wav");
	m_iWhiteSprite = PRECACHE_MODEL("sprites/white.spr");
	m_iGlowSprite = PRECACHE_MODEL("sprites/glow04.spr");
}

int CWarhammer::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 1;
	p->iId = WEAPON_WARHAMMER;
	p->iWeight = 5;
	return 1;
}

int CWarhammer::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CWarhammer::Deploy( )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 1;
	return DefaultDeploy( "models/v_warhammer.mdl", "models/p_warhammer.mdl", WARHAMMER_DRAW, "crowbar" );
}

void CWarhammer::Holster( void )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	SendWeaponAnim( WARHAMMER_HOLSTER );
}


void FindHullIntersectionWarhammer( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
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


void CWarhammer::PrimaryAttack()
{
	if (! Swing( 1 ))
	{
		SetThink( SwingAgain );
		pev->nextthink = gpGlobals->time + 1;
	}
}


void CWarhammer::Smack( )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}


void CWarhammer::SwingAgain( void )
{
	Swing( 0 );
}


int CWarhammer::Swing( int fFirst )
{
	int fDidHit = FALSE;

	TraceResult tr;

	UTIL_MakeVectors (m_pPlayer->pev->v_angle);
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 72;

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
				FindHullIntersectionWarhammer( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			// miss
			switch( (m_iSwing++) % 3 )
			{
			case 0:
				SendWeaponAnim( WARHAMMER_ATTACK1MISS ); break;
			case 1:
				SendWeaponAnim( WARHAMMER_ATTACK2MISS ); break;
			case 2:
				SendWeaponAnim( WARHAMMER_ATTACK3MISS ); break;
			}

			m_flNextPrimaryAttack = gpGlobals->time + 1.0f;
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, 50 + RANDOM_LONG(0,15));

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
			SendWeaponAnim( WARHAMMER_ATTACK1HIT ); break;
		case 1:
			SendWeaponAnim( WARHAMMER_ATTACK2HIT ); break;
		case 2:
			SendWeaponAnim( WARHAMMER_ATTACK3HIT ); break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		gSkillData.gargantuaHealth = RANDOM_FLOAT( 280, 516 );

		ClearMultiDamage( );

		if ( (m_flNextPrimaryAttack + 1 < gpGlobals->time ) || g_pGameRules->IsMultiplayer() )
		{
			// first swing does full damage
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.gargantuaHealth, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}
		else
		{
			// subsequent swings do the same
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.gargantuaHealth, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}	
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		m_flNextPrimaryAttack = gpGlobals->time + 1;

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// shockwave, glow and light where the warhammer is
				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
					WRITE_BYTE( TE_BEAMCYLINDER );
					WRITE_COORD( pev->origin.x );
					WRITE_COORD( pev->origin.y );
					WRITE_COORD( pev->origin.z );
					WRITE_COORD( pev->origin.x );
					WRITE_COORD( pev->origin.y );
					WRITE_COORD( pev->origin.z + 320 );
					WRITE_SHORT( m_iWhiteSprite );
					WRITE_BYTE( 0 );	// startframe
					WRITE_BYTE( 0 );	// framerate
					WRITE_BYTE( 15 );	// life
					WRITE_BYTE( 24 );	// width
					WRITE_BYTE( 0 );	// noise
					WRITE_BYTE( 145 );	// r
					WRITE_BYTE( 255 );	// g
					WRITE_BYTE( 192 );	// b
					WRITE_BYTE( 252 );	// brightness
					WRITE_BYTE( 0 );	// speed
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
					WRITE_BYTE( TE_GLOWSPRITE );
					WRITE_COORD( pev->origin.x );
					WRITE_COORD( pev->origin.y );
					WRITE_COORD( pev->origin.z );
					WRITE_SHORT( m_iGlowSprite );
					WRITE_BYTE( 15 );	// life
					WRITE_BYTE( 128 );	// scale
					WRITE_BYTE( 250 );	// brightness
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
					WRITE_BYTE( TE_DLIGHT );
					WRITE_COORD( pev->origin.x );
					WRITE_COORD( pev->origin.y );
					WRITE_COORD( pev->origin.z );
					WRITE_BYTE( 80 );	// radius
					WRITE_BYTE( 145 );	// r
					WRITE_BYTE( 255 );	// g
					WRITE_BYTE( 252 );	// b
					WRITE_BYTE( 2 );	// life
					WRITE_BYTE( 0 );	// decay
				MESSAGE_END();

				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/warhammer.wav", flVol, ATTN_NORM);

				m_pPlayer->m_iWeaponVolume = WARHAMMER_BODYHIT_VOLUME;
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

			// also play warhammer strike
			switch( RANDOM_LONG(0,1) )
			{
			case 0:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/cbar_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			}
		}

		// delay the decal a bit
		m_trHit = tr;

		SetThink( Smack );
		pev->nextthink = gpGlobals->time + 0.2;

		m_pPlayer->m_iWeaponVolume = flVol * WARHAMMER_WALLHIT_VOLUME;
	}
	return fDidHit;
}
