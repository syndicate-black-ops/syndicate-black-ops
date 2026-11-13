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
#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"

enum w_squeak_e {
	WSQUEAK_IDLE1 = 0,
	WSQUEAK_FIDGET,
	WSQUEAK_JUMP,
	WSQUEAK_RUN,
};

enum squeak_e {
	SQUEAK_IDLE1 = 0,
	SQUEAK_FIDGETFIT,
	SQUEAK_FIDGETNIP,
	SQUEAK_DOWN,
	SQUEAK_UP,
	SQUEAK_THROW
};

#ifndef CLIENT_DLL

class CSqueakGrenade : public CGrenade
{
	void Spawn( void );
	void Precache( void );
	int  Classify( void );
	void EXPORT SuperBounceTouch( CBaseEntity *pOther );
	void EXPORT HuntThink( void );
	int  BloodColor( void ) { return BLOOD_COLOR_RED; }	// SBO (SDK: yellow)
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );

	virtual int		Save( CSave &save ); 
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	static float m_flNextBounceSoundTime;

	// CBaseEntity *m_pTarget;
	float m_flDie;
	Vector m_vecTarget;
	float m_flNextHunt;
	float m_flNextHit;
	Vector m_posPrev;
	EHANDLE m_hOwner;
	int  m_iMyClass;
};

float CSqueakGrenade::m_flNextBounceSoundTime = 0;

LINK_ENTITY_TO_CLASS( monster_snark, CSqueakGrenade );
TYPEDESCRIPTION	CSqueakGrenade::m_SaveData[] = 
{
	DEFINE_FIELD( CSqueakGrenade, m_flDie, FIELD_TIME ),
	DEFINE_FIELD( CSqueakGrenade, m_vecTarget, FIELD_VECTOR ),
	DEFINE_FIELD( CSqueakGrenade, m_flNextHunt, FIELD_TIME ),
	DEFINE_FIELD( CSqueakGrenade, m_flNextHit, FIELD_TIME ),
	DEFINE_FIELD( CSqueakGrenade, m_posPrev, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CSqueakGrenade, m_hOwner, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CSqueakGrenade, CGrenade );

#define SQUEEK_DETONATE_DELAY	30.0	// SBO (SDK: 15.0)

// SBO 2.0: the pitch of the pitbull's sounds falls with the life it has left (155 at the detonation, 95 with a full
// SQUEEK_DETONATE_DELAY) and every bite adds 2 seconds of life, with no limit: with more than 77.5 seconds left the
// pitch went negative, and the engine stops the server on a sound with a pitch outside 0-255 (SV_StartSound). A
// pitbull that keeps biting now sinks to SQUEAK_PITCH_MIN and stays there, and the life it can have left when it
// touches something is limited to what gives that pitch (65 seconds).
#define SQUEAK_PITCH_MAX		155.0
#define SQUEAK_PITCH_SCALE		60.0
#define SQUEAK_PITCH_MIN		25.0
#define SQUEAK_MAX_LIFE			( ( SQUEAK_PITCH_MAX - SQUEAK_PITCH_MIN ) * ( SQUEEK_DETONATE_DELAY / SQUEAK_PITCH_SCALE ) )

// higher pitch as squeeker gets closer to detonation time; never below flMin
static float SqueakPitch( float flLifeLeft, float flMin )
{
	float flpitch = SQUEAK_PITCH_MAX - SQUEAK_PITCH_SCALE * ( flLifeLeft / SQUEEK_DETONATE_DELAY );

	if ( flpitch < flMin )
		flpitch = flMin;
	if ( flpitch > 255 )	// a m_flDie far in the past; HuntThink removes the pitbull before that
		flpitch = 255;

	return flpitch;
}

int CSqueakGrenade :: Classify ( void )
{
	if (m_iMyClass != 0)
		return m_iMyClass; // protect against recursion

	if (m_hEnemy != NULL)
	{
		m_iMyClass = CLASS_INSECT; // no one cares about it
		switch( m_hEnemy->Classify( ) )
		{
			case CLASS_PLAYER:
			case CLASS_HUMAN_PASSIVE:
			case CLASS_HUMAN_MILITARY:
			case CLASS_ALIEN_MILITARY:	// SBO
			case CLASS_ALIEN_MONSTER:	// SBO
			case CLASS_ALIEN_PREDATOR:	// SBO
				m_iMyClass = 0;
				return CLASS_ALIEN_MILITARY; // barney's get mad, grunts get mad at it
		}
		m_iMyClass = 0;
	}

	return CLASS_ALIEN_BIOWEAPON;
}

void CSqueakGrenade :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_squeak.mdl");
	UTIL_SetSize(pev, Vector( -4, -4, 0), Vector(4, 4, 8));
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CSqueakGrenade::SuperBounceTouch );
	SetThink( &CSqueakGrenade::HuntThink );
	pev->nextthink = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time + 1E6;

	pev->flags |= FL_MONSTER;
	pev->takedamage		= DAMAGE_AIM;
	pev->health			= 100;	// SBO (SDK: gSkillData.snarkHealth)
	pev->gravity		= 0.5;
	pev->friction		= 0.5;

	pev->dmg = 20;	// SBO (SDK: gSkillData.snarkDmgPop)

	m_flDie = gpGlobals->time + SQUEEK_DETONATE_DELAY;

	m_flFieldOfView = 0; // 180 degrees
	ApplyCustomMonsterKeys();	// SBO 2.1 (Featureful SetMyFieldOfView): "field_of_view" of a placed Pitbull; it never runs MonsterInit

	if ( pev->owner )
		m_hOwner = Instance( pev->owner );

	m_flNextBounceSoundTime = gpGlobals->time;// reset each time a snark is spawned.

	pev->sequence = WSQUEAK_RUN;
	ResetSequenceInfo( );
}

void CSqueakGrenade::Precache( void )
{
	PRECACHE_MODEL("models/w_squeak.mdl");
	PRECACHE_SOUND("squeek/sqk_blast1.wav");
	PRECACHE_SOUND("common/bodysplat.wav");
	PRECACHE_SOUND("squeek/sqk_die1.wav");
}


void CSqueakGrenade :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->model = iStringNull;// make invisible
	SetThink( &CSqueakGrenade::SUB_Remove );
	SetTouch( NULL );
	pev->nextthink = gpGlobals->time + 0.1;

	// since squeak grenades never leave a body behind, clear out their takedamage now.
	// Squeaks do a bit of radius damage when they pop, and that radius damage will
	// continue to call this function unless we acknowledge the Squeak's death now. (sjb)
	pev->takedamage = DAMAGE_NO;

	// play squeek blast
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "squeek/sqk_blast1.wav", 1, 0.5, 0, PITCH_NORM);	

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, SMALL_EXPLOSION_VOLUME, 3.0 );

	UTIL_BloodDrips( pev->origin, g_vecZero, BloodColor(), 80 );

	if (m_hOwner != NULL)
		RadiusDamage ( pev, m_hOwner->pev, pev->dmg, CLASS_NONE, DMG_BLAST );
	else
		RadiusDamage ( pev, pev, pev->dmg, CLASS_NONE, DMG_BLAST );

	// reset owner so death message happens
	if (m_hOwner != NULL)
		pev->owner = m_hOwner->edict();

	CBaseMonster :: Killed( pevAttacker, GIB_ALWAYS );
}

void CSqueakGrenade :: GibMonster( void )
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "common/bodysplat.wav", 0.75, ATTN_NORM, 0, 200);		
}



void CSqueakGrenade::HuntThink( void )
{
	// ALERT( at_console, "think\n" );

	if (!IsInWorld())
	{
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}
	
	StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	// explode when ready
	if (gpGlobals->time >= m_flDie)
	{
		g_vecAttackDir = pev->velocity.Normalize( );
		pev->health = -1;
		Killed( pev, 0 );
		return;
	}

	// float
	if (pev->waterlevel != 0)
	{
		if (pev->movetype == MOVETYPE_BOUNCE)
		{
			pev->movetype = MOVETYPE_FLY;
		}
		pev->velocity = pev->velocity * 0.9;
		pev->velocity.z += 8.0;
	}
	else if (pev->movetype == MOVETYPE_FLY)	// halflife-updated (halflife #3175): was an assignment
	{
		pev->movetype = MOVETYPE_BOUNCE;
	}

	// return if not time to hunt
	if (m_flNextHunt > gpGlobals->time)
		return;

	m_flNextHunt = gpGlobals->time + 2.0;
	
	CBaseEntity *pOther = NULL;
	Vector vecDir;
	TraceResult tr;

	Vector vecFlat = pev->velocity;
	vecFlat.z = 0;
	vecFlat = vecFlat.Normalize( );

	UTIL_MakeVectors( pev->angles );

	if (m_hEnemy == NULL || !m_hEnemy->IsAlive())
	{
		// find target, bounce a bit towards it.
		Look( 512 );
		m_hEnemy = BestVisibleEnemy( );
	}

	// squeek if it's about time blow up
	if ((m_flDie - gpGlobals->time <= 0.5) && (m_flDie - gpGlobals->time >= 0.3))
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "squeek/sqk_die1.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG(0,0x3F));
		CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 256, 0.25 );
	}

	// higher pitch as squeeker gets closer to detonation time
	float flpitch = SqueakPitch( m_flDie - gpGlobals->time, 80 );

	if (m_hEnemy != NULL)
	{
		if (FVisible( m_hEnemy ))
		{
			vecDir = m_hEnemy->EyePosition() - pev->origin;
			m_vecTarget = vecDir.Normalize( );
		}

		float flVel = pev->velocity.Length();
		float flAdj = 50.0 / (flVel + 10.0);

		if (flAdj > 1.2)
			flAdj = 1.2;
		
		// ALERT( at_console, "think : enemy\n");

		// ALERT( at_console, "%.0f %.2f %.2f %.2f\n", flVel, m_vecTarget.x, m_vecTarget.y, m_vecTarget.z );

		pev->velocity = pev->velocity * flAdj + m_vecTarget * 300;
	}

	if (pev->flags & FL_ONGROUND)
	{
		pev->avelocity = Vector( 0, 0, 0 );
	}
	else
	{
		if (pev->avelocity == Vector( 0, 0, 0))
		{
			pev->avelocity.x = RANDOM_FLOAT( -100, 100 );
			pev->avelocity.z = RANDOM_FLOAT( -100, 100 );
		}
	}

	if ((pev->origin - m_posPrev).Length() < 1.0)
	{
		pev->velocity.x = RANDOM_FLOAT( -100, 100 );
		pev->velocity.y = RANDOM_FLOAT( -100, 100 );
	}
	m_posPrev = pev->origin;

	pev->angles = UTIL_VecToAngles( pev->velocity );
	pev->angles.z = 0;
	pev->angles.x = 0;
}


void CSqueakGrenade::SuperBounceTouch( CBaseEntity *pOther )
{
	float	flpitch;

	TraceResult tr = UTIL_GetGlobalTrace( );

	// don't hit the guy that launched this grenade
	if ( pev->owner && pOther->edict() == pev->owner )
		return;

	// at least until we've bounced once
	pev->owner = NULL;

	pev->angles.x = 0;
	pev->angles.z = 0;

	// avoid bouncing too much
	if (m_flNextHit > gpGlobals->time)
		return;

	// SBO 2.0: no more life left than the lowest pitch stands for (a bite below adds its 2 seconds on top)
	if ( m_flDie > gpGlobals->time + SQUEAK_MAX_LIFE )
		m_flDie = gpGlobals->time + SQUEAK_MAX_LIFE;

	// higher pitch as squeeker gets closer to detonation time
	flpitch = SqueakPitch( m_flDie - gpGlobals->time, SQUEAK_PITCH_MIN );

	// halflife-updated (e9986c0, HL25): not unbreakable glass (FL_WORLDBRUSH func_breakable with takedamage)
	if ( pOther->pev->takedamage && m_flNextAttack < gpGlobals->time && !( pOther->pev->flags & FL_WORLDBRUSH ) )
	{
		// attack!

		// make sure it's me who has touched them
		if (tr.pHit == pOther->edict())
		{
			// and it's not another squeakgrenade
			if (tr.pHit->v.modelindex != pev->modelindex)
			{
				// ALERT( at_console, "hit enemy\n");
				ClearMultiDamage( );
				pOther->TraceAttack(pev, 15, gpGlobals->v_forward, &tr, DMG_SLASH );	// SBO (SDK: gSkillData.snarkDmgBite)
				if (m_hOwner != NULL)
					ApplyMultiDamage( pev, m_hOwner->pev );
				else
					ApplyMultiDamage( pev, pev );

				pev->dmg += 5; // add more explosion damage (SBO; SDK: gSkillData.snarkDmgPop)
				m_flDie += 2.0; // add more life

				// make bite sound
				EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "monster/grrr.wav", 1.0, ATTN_NORM, 0, (int)flpitch);
				m_flNextAttack = gpGlobals->time + 0.5;
			}
		}
		else
		{
			// ALERT( at_console, "been hit\n");
		}
	}

	m_flNextHit = gpGlobals->time + 0.1;
	m_flNextHunt = gpGlobals->time;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// in multiplayer, we limit how often snarks can make their bounce sounds to prevent overflows.
		if ( gpGlobals->time < m_flNextBounceSoundTime )
		{
			// too soon!
			return;
		}
	}

	if (!(pev->flags & FL_ONGROUND))
	{
		// play bounce sound
		float flRndSound = RANDOM_FLOAT ( 0 , 1 );

		if ( flRndSound <= 0.33 )
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "monster/grrr.wav", 1, ATTN_NORM, 0, (int)flpitch);		
		else if (flRndSound <= 0.66)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "monster/ouaf.wav", 1, ATTN_NORM, 0, (int)flpitch);
		else 
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "monster/grrr.wav", 1, ATTN_NORM, 0, (int)flpitch);
		CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 256, 0.25 );
	}
	else
	{
		// skittering sound
		CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 100, 0.1 );
	}

	m_flNextBounceSoundTime = gpGlobals->time + 0.5;// half second.
}

#endif

LINK_ENTITY_TO_CLASS( weapon_snark, CSqueak );


void CSqueak::Spawn( )
{
	Precache( );
	m_iId = WEAPON_SNARK;
	SET_MODEL(ENT(pev), "models/w_sqknest.mdl");

	FallInit();//get ready to fall down.

	m_iDefaultAmmo = SNARK_DEFAULT_GIVE;
		
	pev->sequence = 1;
	pev->animtime = gpGlobals->time;
	pev->framerate = 1.0;
}


void CSqueak::Precache( void )
{
	PRECACHE_MODEL("models/w_sqknest.mdl");
	PRECACHE_MODEL("models/v_squeak.mdl");
	PRECACHE_MODEL("models/p_squeak.mdl");
	UTIL_PrecacheOther("monster_snark");
	PRECACHE_SOUND("monster/ouaf.wav");	// SBO
	PRECACHE_SOUND("monster/grrr.wav");	// SBO

	m_usSnarkFire = PRECACHE_EVENT ( 1, "events/snarkfire.sc" );
}


int CSqueak::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Snarks";
	p->iMaxAmmo1 = SNARK_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_SNARK;
	p->iWeight = SNARK_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}



BOOL CSqueak::Deploy( )
{
	// play hunt sound
	float flRndSound = RANDOM_FLOAT ( 0 , 1 );

	if ( flRndSound <= 0.5 )
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "monster/grrr.wav", 1, ATTN_NORM, 0, 100);
	else
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "monster/ouaf.wav", 1, ATTN_NORM, 0, 100);

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	m_pPlayer->m_iSboDeployFlag = 0;

	return DefaultDeploy( "models/v_squeak.mdl", "models/p_squeak.mdl", SQUEAK_UP, "squeak" );
}


void CSqueak::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	
	if ( !m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
	{
		m_pPlayer->pev->weapons &= ~(1<<WEAPON_SNARK);
		SetThink( &CBasePlayerItem::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}
	
	SendWeaponAnim( SQUEAK_DOWN );
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}


void CSqueak::PrimaryAttack()
{
	if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
	{
		// halflife-updated (e9986c0, HL25 snark): aiming down blends the throw toward the flat forward, a ducking
		// player's trace starts higher the more he aims down, and any free fraction places the snark. Before, the
		// trace along the full view direction often failed looking down or crouched, and nothing was thrown.
		Vector vecFlatAngles = m_pPlayer->pev->v_angle;
		vecFlatAngles.x = 0;
		UTIL_MakeVectors( vecFlatAngles );
		Vector vEntityForward = gpGlobals->v_forward;

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector forward = gpGlobals->v_forward;
		TraceResult tr;
		Vector trace_origin;

		// HACK HACK:  Ugly hacks to handle change in origin based on new physics code for players
		// Move origin up if crouched and start trace a bit outside of body ( 20 units instead of 16 )
		float flAimDownFraction = m_pPlayer->pev->v_angle.x > 0 ? m_pPlayer->pev->v_angle.x / 90.f : 0;
		trace_origin = m_pPlayer->pev->origin;
		if ( m_pPlayer->pev->flags & FL_DUCKING )
		{
			trace_origin = trace_origin - ( flAimDownFraction + 1 ) * ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
		}

		Vector vTraceForward = ( flAimDownFraction * vEntityForward ) + ( 1 - flAimDownFraction ) * forward;
		// find place to toss monster
		// SBO 2.0: the thrower is ignored. Looking up, the trace starts inside his own box (24 units above its center,
		// top at 36), so with NULL it started solid on him and nothing was thrown (1.38, HL25). The client's event
		// (EV_SnarkFire) already leaves the local player out of its trace.
		UTIL_TraceLine( trace_origin + vTraceForward * 24, trace_origin + forward * 60, dont_ignore_monsters, m_pPlayer->edict(), &tr );

	int flags;
#ifdef CLIENT_WEAPONS
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	    PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSnarkFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );

		if ( tr.fAllSolid == 0 && tr.fStartSolid == 0 && tr.flFraction > 0 )
		{
			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
			CBaseEntity *pSqueak = CBaseEntity::Create( "monster_snark", tr.vecEndPos, m_pPlayer->pev->v_angle, m_pPlayer->edict() );
			pSqueak->pev->velocity = vTraceForward * 200 + m_pPlayer->pev->velocity;
#endif

			// play hunt sound
			float flRndSound = RANDOM_FLOAT ( 0 , 1 );

			if ( flRndSound <= 0.5 )
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "monster/grrr.wav", 1, ATTN_NORM, 0, 105);
			else
				EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "monster/ouaf.wav", 1, ATTN_NORM, 0, 105);

			m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;

			if ( !InfiniteAmmo() )	// sv_infinite_ammo
				m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

			m_fJustThrown = 1;

			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
		}
	}
}


void CSqueak::SecondaryAttack( void )
{

}


void CSqueak::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if (m_fJustThrown)
	{
		m_fJustThrown = 0;

		if ( !m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] )
		{
			RetireWeapon();
			return;
		}

		SendWeaponAnim( SQUEAK_UP );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		return;
	}

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
	if (flRand <= 0.75)
	{
		iAnim = SQUEAK_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = SQUEAK_FIDGETFIT;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 70.0 / 16.0;
	}
	else
	{
		iAnim = SQUEAK_FIDGETNIP;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 80.0 / 16.0;
	}
	SendWeaponAnim( iAnim );
}

#endif