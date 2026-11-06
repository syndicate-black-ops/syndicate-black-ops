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

// SBO: Medikit (CMedikit)

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#ifndef CLIENT_DLL
#include "sbo_monster.h"
#endif


#define	MEDIKIT_BODYHIT_VOLUME 128
#define	MEDIKIT_WALLHIT_VOLUME 512

LINK_ENTITY_TO_CLASS( weapon_medikit, CMedikit );



enum medikit_e {
	MEDIKIT_IDLE = 0,
	MEDIKIT_ANIM1,
	MEDIKIT_ANIM2,
	MEDIKIT_ANIM3,
	MEDIKIT_ANIM4,
	MEDIKIT_DRAW,
};


void CMedikit::Spawn( )
{
	Precache( );
	m_iId = WEAPON_MEDIKIT;
	SET_MODEL(ENT(pev), "models/w_medikit.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}


void CMedikit::Precache( void )
{
	PRECACHE_MODEL("models/v_medikit.mdl");
	PRECACHE_MODEL("models/w_medikit.mdl");
	PRECACHE_MODEL("models/p_medikit.mdl");
	PRECACHE_SOUND("items/medshot4.wav");

	m_usMedikit = PRECACHE_EVENT ( 1, "events/medikit.sc" );
}

int CMedikit::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 4;
	p->iId = WEAPON_MEDIKIT;
	p->iWeight = 1;
	return 1;
}

int CMedikit::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CMedikit::Deploy( )
{
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;
	return DefaultDeploy( "models/v_medikit.mdl", "models/p_medikit.mdl", MEDIKIT_DRAW, "trip" );
}

void CMedikit::Holster( void )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( MEDIKIT_DRAW );
}


void FindHullIntersectionMedikit( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
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


void CMedikit::PrimaryAttack()
{
	if (! Swing( 1 ))
	{
		SetThink( &CMedikit::SwingAgain );
		// SBO 2.0: kept on purpose. UTIL_WeaponTimeBase() is 0 on the server with client weapons, so
		// "+ 4" is in the past and the retry (and Smack, in Swing) run on the next frame: 1.38's effective timing.
		// halflife-updated's crowbar uses gpGlobals->time here; only its fix for the retry's duplicate swing event
		// is taken (Swing).
		pev->nextthink = UTIL_WeaponTimeBase() + 4;
	}
}


void CMedikit::Smack( )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}


void CMedikit::SwingAgain( void )
{
	Swing( 0 );
}


int CMedikit::Swing( int fFirst )
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
				FindHullIntersectionMedikit( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}
#endif

	// halflife-updated (crowbar.cpp, halflife #2296 / #3230): the swing event once, not again from the retry
	// (SwingAgain, the next frame), which made other players see the swing twice
	if ( fFirst )
	{
		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usMedikit,
		0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0,
		0.0, 0, 0.0 );
	}

	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			// miss
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 4;
		}
	}
	else
	{
#ifndef CLIENT_DLL
		// SBO: server only (the 2003 client only sets the think time and volume)
		fDidHit = TRUE;
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		switch( ((m_iSwing++) % 2) + 1 )
		{
		case 0:
			SendWeaponAnim( MEDIKIT_ANIM2 ); break;
		case 1:
			SendWeaponAnim( MEDIKIT_ANIM2 ); break;
		case 2:
			SendWeaponAnim( MEDIKIT_ANIM2 ); break;
		}

		// heal
		if ( pEntity->pev->health < 100 )
		{
			// SBO 2.0: XP only for a living player the heal gave health to (not the world, a monster or a corpse)
			BOOL fLivingPlayer = pEntity->IsPlayer() && pEntity->IsAlive();
			float flOldHealth = pEntity->pev->health;
			pEntity->TakeHealth( RANDOM_FLOAT( 10, 100 ), DMG_GENERIC );

			if ( fLivingPlayer && pEntity->pev->health > flOldHealth )
			{
				SBO_XP_GAIN( m_pPlayer, RANDOM_FLOAT( 500, 1000 ), XP.value == 1 ? 7500 : INT_MAX );

				if ( SBO_XP( m_pPlayer ) >= 7500 && XP.value == 1 )
				{
					SBO_XP( m_pPlayer ) = 0;
					m_pPlayer->SboLevelUp();	// SBO 2.0 (player.cpp): +5 level speed, +2 spawn health, the speed at once
					m_pPlayer->TakeHealth( RANDOM_FLOAT( 10, 20 ), DMG_GENERIC );
					m_pPlayer->pev->frags += 1;

					char szText[204];
					hudtextparms_t hText;

					sprintf( szText, "LEVEL UP\n" );
					memset( &hText, 0, sizeof( hText ) );
					hText.channel = 1;
					hText.x = -1;
					hText.y = 0.3;
					hText.effect = 0;
					hText.r1 = hText.g1 = hText.b1 = 255;
					hText.a1 = 255;
					hText.r2 = hText.g2 = hText.b2 = 255;
					hText.a2 = 255;
					hText.fadeinTime = 0.2;
					hText.fadeoutTime = 0.5;
					hText.holdTime = 1.5;
					hText.fxTime = 0.5;
					UTIL_HudMessage( m_pPlayer, hText, szText );
					Observer_LevelUp( m_pPlayer, hText, szText );	// SBO 2.0: and those watching the player

					SboScoreboard_SendScoreInfo( m_pPlayer );	// chat-vgui2: scoreboard team + level
				}
			}
		}

		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 2;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 4;

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE && pEntity->Classify() != CLASS_PLAYER )
			{
				switch( RANDOM_LONG(0,2) )
				{
				case 0:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "items/medshot4.wav", 1, ATTN_NORM); break;
				case 1:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "items/medshot4.wav", 1, ATTN_NORM); break;
				case 2:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "items/medshot4.wav", 1, ATTN_NORM); break;
				}
				m_pPlayer->m_iWeaponVolume = MEDIKIT_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
					  return TRUE;
				// SBO: SDK's `else flVol = 0.1;` removed (flVol is unused; the dead store would leave 0.1f in .rodata)

				fHitWorld = FALSE;
			}
		}

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CROWBAR);

			if ( g_pGameRules->IsMultiplayer() )
			{
				fvolbar = 1;
			}

			switch( RANDOM_LONG(0,1) )
			{
			case 0:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "items/medshot4.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "items/medshot4.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			}
		}

		// delay the decal a bit
		m_trHit = tr;

		SetThink( &CMedikit::Smack );
#endif
		pev->nextthink = UTIL_WeaponTimeBase() + 4;

		m_pPlayer->m_iWeaponVolume = MEDIKIT_WALLHIT_VOLUME;
	}
	return fDidHit;
}


void CMedikit::SecondaryAttack( void )
{
#ifndef CLIENT_DLL
	// SBO: server only, heal yourself (the 2003 client's is empty)
	if ( m_pPlayer->pev->health < 100 )
	{
		float flOldHealth = m_pPlayer->pev->health;
		m_pPlayer->TakeHealth( RANDOM_FLOAT( 10, 100 ), DMG_GENERIC );
		SendWeaponAnim( MEDIKIT_ANIM2 );
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "items/medshot4.wav", 1, ATTN_NORM);

		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 2;
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2;

		g_engfuncs.pfnSetClientMaxspeed( ENT(m_pPlayer->pev), m_pPlayer->m_iSboMaxSpeed );

		// SBO 2.0: XP only when the heal raised the health
		if ( m_pPlayer->pev->health > flOldHealth )
		{
			SBO_XP_GAIN( m_pPlayer, RANDOM_FLOAT( 500, 1000 ), XP.value == 1 ? 7500 : INT_MAX );

			if ( SBO_XP( m_pPlayer ) >= 7500 && XP.value == 1 )
			{
				SBO_XP( m_pPlayer ) = 0;
				m_pPlayer->SboLevelUp();	// SBO 2.0 (player.cpp): +5 level speed, +2 spawn health, the speed at once
				m_pPlayer->TakeHealth( RANDOM_FLOAT( 10, 20 ), DMG_GENERIC );
				m_pPlayer->pev->frags += 1;

				char szText[204];
				hudtextparms_t hText;

				sprintf( szText, "LEVEL UP\n" );
				memset( &hText, 0, sizeof( hText ) );
				hText.channel = 1;
				hText.x = -1;
				hText.y = 0.3;
				hText.effect = 0;
				hText.r1 = hText.g1 = hText.b1 = 255;
				hText.a1 = 255;
				hText.r2 = hText.g2 = hText.b2 = 255;
				hText.a2 = 255;
				hText.fadeinTime = 0.2;
				hText.fadeoutTime = 0.5;
				hText.holdTime = 1.5;
				hText.fxTime = 0.5;
				UTIL_HudMessage( m_pPlayer, hText, szText );
				Observer_LevelUp( m_pPlayer, hText, szText );	// SBO 2.0: and those watching the player

				SboScoreboard_SendScoreInfo( m_pPlayer );	// chat-vgui2: scoreboard team + level
			}
		}
	}

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 4;
#endif
}
