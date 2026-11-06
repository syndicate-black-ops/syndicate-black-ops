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

// SBO: Fist of Legend (CWong)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"


#define	WONG_BODYHIT_VOLUME 128
#define	WONG_WALLHIT_VOLUME 512

LINK_ENTITY_TO_CLASS( weapon_wong, CWong );



enum wong_e {
	WONG_IDLE = 0,
	WONG_IDLE2,
	WONG_IDLE3,
	WONG_ATTACK1,
	WONG_ATTACK2,
	WONG_ATTACK3,
	WONG_ANIM6,
	WONG_ANIM7,
	WONG_ANIM8,
	WONG_DRAW,
	WONG_HOLSTER
};


void CWong::Spawn( )
{
	Precache( );
	m_iId = WEAPON_WONG;
	// no world model: bare fists
	m_iClip = -1;

	FallInit();// get ready to fall down.
	// a trigger while it falls, as an item: the engine's exact clip against a SOLID_BBOX entity reads its model,
	// and modelindex 0 has none on a stock engine (SV_SingleClipMoveToEntity)
	pev->solid = SOLID_TRIGGER;
	UTIL_SetOrigin( pev, pev->origin );
}


void CWong::Precache( void )
{
	PRECACHE_MODEL("models/v_wong.mdl");
	PRECACHE_MODEL("models/p_wong.mdl");
	PRECACHE_SOUND("weapons/wong_hit.wav");
	PRECACHE_SOUND("weapons/wong_hitbody1.wav");
	PRECACHE_SOUND("weapons/cbar_miss1.wav");

	m_usWong = PRECACHE_EVENT ( 1, "events/wong.sc" );
}

int CWong::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 0;
	p->iId = WEAPON_WONG;
	p->iWeight = 0;
	return 1;
}

int CWong::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CWong::Deploy( )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 1;
	return DefaultDeploy( "models/v_wong.mdl", "models/p_wong.mdl", WONG_DRAW, "crowbar" );
}

void CWong::Holster( void )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.3;
	SendWeaponAnim( WONG_HOLSTER );
}


void FindHullIntersectionWong( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
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


void CWong::PrimaryAttack()
{
	if (! Swing( 1 ))
	{
		SetThink( &CWong::SwingAgain );
		// halflife-updated (crowbar.cpp, halflife #2296 / #3230): a think time, not a weapon time. UTIL_WeaponTimeBase()
		// is 0 on the server with client weapons, so the retry ran on the next frame instead of 0.3 s later.
		pev->nextthink = gpGlobals->time + 0.3;
	}
}


void CWong::Smack( )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}


void CWong::SwingAgain( void )
{
	Swing( 0 );
}


int CWong::Swing( int fFirst )
{
	int fDidHit = FALSE;

	TraceResult tr;

	UTIL_MakeVectors (m_pPlayer->pev->v_angle);
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersectionWong( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}
#endif

	// halflife-updated (crowbar.cpp): the swing event once, not again from the retry (SwingAgain)
	if ( fFirst )
	{
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usWong, 
		0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0,
		0.0, 0, 0.0 );
	}

	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			// miss
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1;

			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		fDidHit = TRUE;
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		switch( ((m_iSwing++) % 2) + 1 )
		{
		case 0:
			SendWeaponAnim( WONG_ATTACK1 ); break;
		case 1:
			SendWeaponAnim( WONG_ATTACK2 ); break;
		case 2:
			SendWeaponAnim( WONG_ATTACK3 ); break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
		// SBO: server only (the 2003 client lacks the damage/sound code)
		gSkillData.houndeyeDmgBlast = RANDOM_FLOAT( 8, 15 );

		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/wong_hitbody1.wav", 1, ATTN_NORM, 0, PITCH_NORM);

		ClearMultiDamage( );

		if ( (m_flNextPrimaryAttack + 1 < UTIL_WeaponTimeBase() ) || g_pGameRules->IsMultiplayer() )
		{
			// first swing does full damage
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.houndeyeDmgBlast, gpGlobals->v_forward, &tr, DMG_CLUB );
		}
		else
		{
			// subsequent swings do the same
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.houndeyeDmgBlast, gpGlobals->v_forward, &tr, DMG_CLUB );
		}
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				m_pPlayer->m_iWeaponVolume = WONG_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
					  return TRUE;
				// SBO: SDK's `else flVol = 0.1;` removed (flVol is unused; the dead store would leave 0.1f in .rodata)

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

			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/wong_hit.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		}

		// delay the decal a bit
		m_trHit = tr;

		SetThink( &CWong::Smack );
#endif
		pev->nextthink = gpGlobals->time + 0.3;	// halflife-updated: the decal 0.3 s later (was the next frame, see PrimaryAttack)

		m_pPlayer->m_iWeaponVolume = WONG_WALLHIT_VOLUME;
	}
	return fDidHit;
}
