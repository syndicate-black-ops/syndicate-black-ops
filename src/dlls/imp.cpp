/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// bullsquid - big, spotty tentacle-mouthed meanie.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"nodes.h"
#include	"effects.h"
#include	"decals.h"
#include	"soundent.h"
#include	"game.h"
#include	"player.h"
#include	"sbo_monster.h"

#define		SQUID_SPRINT_DIST	256 // how close the squid has to get before starting to sprint and refusing to swerve



	

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_SQUID_HURTHOP = LAST_COMMON_SCHEDULE + 1,
	SCHED_SQUID_SMELLFOOD,
	SCHED_SQUID_SEECRAB,
	SCHED_SQUID_EAT,
	SCHED_SQUID_SNIFF_AND_EAT,
	SCHED_SQUID_WALLOW,
};

//=========================================================
// monster-specific tasks
//=========================================================
enum 
{
	TASK_SQUID_HOPTURN = LAST_COMMON_TASK + 1,
};

//=========================================================
// Bullsquid's spit projectile
//=========================================================
class CSquidSpit : public CBaseEntity
{
public:
	void Spawn( void );

	static void Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	void Touch( CBaseEntity *pOther );
	void EXPORT Animate( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int  m_maxFrame;
};

LINK_ENTITY_TO_CLASS( squidspit, CSquidSpit );

TYPEDESCRIPTION	CSquidSpit::m_SaveData[] = 
{
	DEFINE_FIELD( CSquidSpit, m_maxFrame, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CSquidSpit, CBaseEntity );

void CSquidSpit:: Spawn( void )
{
	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING( "squidspit" );
	
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "sprites/xfireball3.spr");
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->renderamt = 255;
	pev->frame = 0;
	pev->scale = RANDOM_FLOAT( 0.1, 2.2 );

	UTIL_SetSize( pev, Vector( 0, 0, 0), Vector(0, 0, 0) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
}

void CSquidSpit::Animate( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->frame++ )
	{
		if ( pev->frame > m_maxFrame )
		{
			pev->frame = 0;
		}
	}
}

void CSquidSpit::Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	CSquidSpit *pSpit = GetClassPtr( (CSquidSpit *)NULL );
	pSpit->Spawn();
	
	UTIL_SetOrigin( pSpit->pev, vecStart );
	pSpit->pev->velocity = vecVelocity;
	pSpit->pev->owner = ENT(pevOwner);

	pSpit->SetThink ( &CSquidSpit::Animate );
	pSpit->pev->nextthink = gpGlobals->time + 0.1;
}

void CSquidSpit :: Touch ( CBaseEntity *pOther )
{
	TraceResult tr;
	int		iPitch;

	// splat sound
	iPitch = RANDOM_FLOAT( 90, 110 );

	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "weapons/burn_fire1.wav", 1, 0.8, 0, iPitch );	

	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/burn_fire1.wav", 1, 0.8, 0, iPitch );	
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/burn_fire1.wav", 1, 0.8, 0, iPitch );	
		break;
	}

	if ( pOther->pev->takedamage )
	{
		if ( juniorpowmonst.value == 0 )
		{
			switch ( MonsterScalePlayers() )
			{
			case 1:
			case 2:
				pOther->TakeDamage ( pev, pev, 20, DMG_BURN );
				break;
			case 3:
			case 4:
				pOther->TakeDamage ( pev, pev, 30, DMG_BURN );
				break;
			case 5:
			case 6:
				pOther->TakeDamage ( pev, pev, 40, DMG_BURN );
				break;
			case 7:
			case 8:
				pOther->TakeDamage ( pev, pev, 50, DMG_BURN );
				break;
			default:
				pOther->TakeDamage ( pev, pev, 50, DMG_BURN );
				break;
			}
		}
		else if ( juniorpowmonst.value == 2 )
		{
			switch ( MonsterScalePlayers() )
			{
			case 1:
			case 2:
				pOther->TakeDamage ( pev, pev, 40, DMG_BURN );
				break;
			case 3:
			case 4:
				pOther->TakeDamage ( pev, pev, 50, DMG_BURN );
				break;
			case 5:
			case 6:
				pOther->TakeDamage ( pev, pev, 60, DMG_BURN );
				break;
			case 7:
			case 8:
				pOther->TakeDamage ( pev, pev, 70, DMG_BURN );
				break;
			default:
				pOther->TakeDamage ( pev, pev, 70, DMG_BURN );
				break;
			}
		}
		else if ( juniorpowmonst.value == 3 )
		{
			switch ( MonsterScalePlayers() )
			{
			case 1:
			case 2:
				pOther->TakeDamage ( pev, pev, 60, DMG_BURN );
				break;
			case 3:
			case 4:
				pOther->TakeDamage ( pev, pev, 70, DMG_BURN );
				break;
			case 5:
			case 6:
				pOther->TakeDamage ( pev, pev, 90, DMG_BURN );
				break;
			case 7:
			case 8:
				pOther->TakeDamage ( pev, pev, 150, DMG_BURN );
				break;
			default:
				pOther->TakeDamage ( pev, pev, 150, DMG_BURN );
				break;
			}
		}
		else
		{
			switch ( MonsterScalePlayers() )
			{
			case 1:
			case 2:
				pOther->TakeDamage ( pev, pev, 30, DMG_BURN );
				break;
			case 3:
			case 4:
				pOther->TakeDamage ( pev, pev, 40, DMG_BURN );
				break;
			case 5:
			case 6:
				pOther->TakeDamage ( pev, pev, 50, DMG_BURN );
				break;
			case 7:
			case 8:
				pOther->TakeDamage ( pev, pev, 60, DMG_BURN );
				break;
			default:
				pOther->TakeDamage ( pev, pev, 60, DMG_BURN );
				break;
			}
		}
	}

	SetThink ( &CSquidSpit::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		BSQUID_AE_SPIT		( 1 )
#define		BSQUID_AE_BITE		( 2 )
#define		BSQUID_AE_BLINK		( 3 )
#define		BSQUID_AE_TAILWHIP	( 4 )
#define		BSQUID_AE_HOP		( 5 )
#define		BSQUID_AE_THROW		( 6 )

class Cimp : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  ISoundMask( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void IdleSound( void );
	void PainSound( void );
	void DeathSound( void );
	void AlertSound ( void );
	void AttackSound( void );
	void StartTask ( Task_t *pTask );
	void RunTask ( Task_t *pTask );
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );
	BOOL CheckMeleeAttack2 ( float flDot, float flDist );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	void RunAI( void );
	BOOL FValidateHintType ( short sHint );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType ( int Type );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	int IRelationship ( CBaseEntity *pTarget );
	int IgnoreConditions ( void );
	MONSTERSTATE GetIdealState ( void );

	int	Save( CSave &save ); 
	int Restore( CRestore &restore );

	CUSTOM_SCHEDULES;
	static TYPEDESCRIPTION m_SaveData[];

	BOOL m_fCanThreatDisplay;// this is so the squid only does the "I see a headcrab!" dance one time. 

	float m_flLastHurtTime;// we keep track of this, because if something hurts a squid, it will forget about its love of headcrabs for a while.
	float m_flNextSpitTime;// last time the bullsquid used the spit attack.
};
LINK_ENTITY_TO_CLASS( monster_imp, Cimp );

TYPEDESCRIPTION	Cimp::m_SaveData[] = 
{
	DEFINE_FIELD( Cimp, m_fCanThreatDisplay, FIELD_BOOLEAN ),
	DEFINE_FIELD( Cimp, m_flLastHurtTime, FIELD_TIME ),
	DEFINE_FIELD( Cimp, m_flNextSpitTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( Cimp, CBaseMonster );

//=========================================================
// IgnoreConditions 
//=========================================================
int Cimp::IgnoreConditions ( void )
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	if ( gpGlobals->time - m_flLastHurtTime <= 20 )
	{
		// haven't been hurt in 20 seconds, so let the squid care about stink. 
		iIgnore = bits_COND_SMELL | bits_COND_SMELL_FOOD;
	}

	if ( m_hEnemy != NULL )
	{
		if ( FClassnameIs( m_hEnemy->pev, "monster_headcrab" ) )
		{
			// (Unless after a tasty headcrab)
			iIgnore = bits_COND_SMELL | bits_COND_SMELL_FOOD;
		}
	}


	return iIgnore;
}

//=========================================================
// IRelationship - overridden for bullsquid so that it can
// be made to ignore its love of headcrabs for a while.
//=========================================================
int Cimp::IRelationship ( CBaseEntity *pTarget )
{
	return CBaseMonster :: IRelationship ( pTarget );
}

//=========================================================
// TakeDamage - overridden for bullsquid so we can keep track
// of how much time has passed since it was last injured
//=========================================================
int Cimp :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// SBO 2.0: invulnerable in its script ("m_takeDamagePolicy" 1): no damage and no XP for the hit
	if ( ScriptInvulnerable() )
		return 0;

	float flDist;
	Vector vecApex;

	if ( bitsDamageType & DMG_BLAST )
		flDamage *= 5;

	if ( IsAlive() )
	{
		CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );

		if ( pevAttacker )
		{
			CBasePlayer *pPlayer = NULL;
			if ( pAttacker && pAttacker->Classify() == CLASS_PLAYER )
				pPlayer = (CBasePlayer *)pAttacker;

			if ( pPlayer && XP.value == 1 )
			{
				SBO_XP_GAIN( pPlayer, flDamage * 1.5, 5000 );

				if ( SBO_XP( pPlayer ) >= 5000 )
				{
					SBO_XP( pPlayer ) = 0;
					pPlayer->SboLevelUp();	// SBO 2.0 (player.cpp): +5 level speed, +2 spawn health, the speed at once
					pPlayer->TakeHealth( RANDOM_FLOAT( 10, 20 ), DMG_GENERIC );
					pevAttacker->frags += 1;

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
					UTIL_HudMessage( pPlayer, hText, szText );
					Observer_LevelUp( pPlayer, hText, szText );	// SBO 2.0: and those watching the player

					SboScoreboard_SendScoreInfo( pPlayer );	// chat-vgui2: scoreboard team + level
				}
			}
		}
	}

	// if the squid is running, has an enemy, was hurt by the enemy, hasn't been hurt in the last 3 seconds, and isn't too close to the enemy,
	// it will swerve. (whew).
	if ( m_hEnemy != NULL && IsMoving() && pevAttacker == m_hEnemy->pev && gpGlobals->time - m_flLastHurtTime > 3 )
	{
		flDist = ( pev->origin - m_hEnemy->pev->origin ).Length2D();
		
		if ( flDist > SQUID_SPRINT_DIST )
		{
			flDist = ( pev->origin - m_Route[ m_iRouteIndex ].vecLocation ).Length2D();// reusing flDist. 

			if ( FTriangulate( pev->origin, m_Route[ m_iRouteIndex ].vecLocation, flDist * 0.5, m_hEnemy, &vecApex ) )
			{
				InsertWaypoint( vecApex, bits_MF_TO_DETOUR | bits_MF_DONT_SIMPLIFY );
			}
		}
	}

	if ( !FClassnameIs ( pevAttacker, "monster_headcrab" ) )
	{
		// don't forget about headcrabs if it was a headcrab that hurt the squid.
		m_flLastHurtTime = gpGlobals->time;
	}

	return CBaseMonster :: TakeDamage ( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL Cimp :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( IsMoving() && flDist >= 216 )
	{
		// squid will far too far behind if he stops running to spit at this distance from the enemy.
		return FALSE;
	}

	if ( flDist > 64 && flDist <= 4096 && flDot >= 0.5 && gpGlobals->time >= m_flNextSpitTime )
	{
		if ( m_hEnemy != NULL )
		{
			if ( fabs( pev->origin.z - m_hEnemy->pev->origin.z ) > 256 )
			{
				// don't try to spit at someone up really high or down really low.
				return FALSE;
			}
		}

		if ( IsMoving() )
		{
			// don't spit again for a long time, resume chasing enemy.
			m_flNextSpitTime = gpGlobals->time + 5;
		}
		else
		{
			// not moving, so spit again pretty soon.
			m_flNextSpitTime = gpGlobals->time + 0.5;
		}

		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CheckMeleeAttack1 - bullsquid is a big guy, so has a longer
// melee range than most monsters. This is the tailwhip attack
//=========================================================
BOOL Cimp :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	if ( m_hEnemy->pev->health <= 35 && flDist <= 85 && flDot >= 0.7 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckMeleeAttack2 - bullsquid is a big guy, so has a longer
// melee range than most monsters. This is the bite attack.
// this attack will not be performed if the tailwhip attack
// is valid.
//=========================================================
BOOL Cimp :: CheckMeleeAttack2 ( float flDot, float flDist )
{
	if ( flDist <= 85 && flDot >= 0.7 && !HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )		// The player & bullsquid can be as much as their bboxes 
	{										// apart (48 * sqrt(3)) and he can still attack (85 is a little more than 48*sqrt(3))
		return TRUE;
	}
	return FALSE;
}  

//=========================================================
//  FValidateHintType 
//=========================================================
BOOL Cimp :: FValidateHintType ( short sHint )
{
	int i;

	static short sSquidHints[] =
	{
		HINT_WORLD_HUMAN_BLOOD,
	};

	for ( i = 0 ; i < ARRAYSIZE ( sSquidHints ) ; i++ )
	{
		if ( sSquidHints[ i ] == sHint )
		{
			return TRUE;
		}
	}

	ALERT ( at_aiconsole, "Couldn't validate hint type" );
	return FALSE;
}

//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. In the base class implementation,
// monsters care about all sounds, but no scents.
//=========================================================
int Cimp :: ISoundMask ( void )
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_CARCASS	|
			bits_SOUND_MEAT		|
			bits_SOUND_GARBAGE	|
			bits_SOUND_PLAYER;
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	Cimp :: Classify ( void )
{
	return	CLASS_ALIEN_MILITARY;
}

//=========================================================
// IdleSound 
//=========================================================
#define SQUID_ATTN_IDLE	(float)1.5
void Cimp :: IdleSound ( void )
{
	switch ( RANDOM_LONG(0,4) )
	{
	case 0:	
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "bullchicken/bc_idle1.wav", 1, SQUID_ATTN_IDLE );	
		break;
	case 1:	
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "bullchicken/bc_idle2.wav", 1, SQUID_ATTN_IDLE );	
		break;
	case 2:	
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "bullchicken/bc_idle3.wav", 1, SQUID_ATTN_IDLE );	
		break;
	case 3:	
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "bullchicken/bc_idle4.wav", 1, SQUID_ATTN_IDLE );	
		break;
	case 4:	
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "bullchicken/bc_idle5.wav", 1, SQUID_ATTN_IDLE );	
		break;
	}
}

//=========================================================
// PainSound 
//=========================================================
void Cimp :: PainSound ( void )
{
	int iPitch = RANDOM_LONG( 85, 120 );

	switch ( RANDOM_LONG(0,3) )
	{
	case 0:	
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "bullchicken/bc_pain1.wav", 1, ATTN_NORM, 0, iPitch );	
		break;
	case 1:	
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "bullchicken/bc_pain2.wav", 1, ATTN_NORM, 0, iPitch );	
		break;
	case 2:	
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "bullchicken/bc_pain3.wav", 1, ATTN_NORM, 0, iPitch );	
		break;
	case 3:	
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "bullchicken/bc_pain4.wav", 1, ATTN_NORM, 0, iPitch );	
		break;
	}
}

//=========================================================
// AlertSound
//=========================================================
void Cimp :: AlertSound ( void )
{
	int iPitch = RANDOM_LONG( 140, 160 );

	switch ( RANDOM_LONG ( 0, 1  ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "bullchicken/bc_idle1.wav", 1, ATTN_NORM, 0, iPitch );	
		break;
	case 1:
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "bullchicken/bc_idle2.wav", 1, ATTN_NORM, 0, iPitch );	
		break;
	}
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void Cimp :: SetYawSpeed ( void )
{
	pev->yaw_speed = 500;
	pev->framerate = 1.8;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void Cimp :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	short iDmg;

	if ( juniorpowmonst.value == 0 )
	{
		switch ( MonsterScalePlayers() )
		{
		case 1:
		case 2:
			iDmg = 10;
			break;
		case 3:
		case 4:
			iDmg = 15;
			break;
		case 5:
		case 6:
			iDmg = 20;
			break;
		case 7:
		case 8:
			iDmg = 30;
			break;
		default:
			iDmg = 30;
			break;
		}
	}
	else if ( juniorpowmonst.value == 2 )
	{
		switch ( MonsterScalePlayers() )
		{
		case 1:
		case 2:
			iDmg = 35;
			break;
		case 3:
		case 4:
			iDmg = 45;
			break;
		case 5:
		case 6:
			iDmg = 50;
			break;
		case 7:
		case 8:
			iDmg = 60;
			break;
		default:
			iDmg = 60;
			break;
		}
	}
	else if ( juniorpowmonst.value == 3 )
	{
		switch ( MonsterScalePlayers() )
		{
		case 1:
		case 2:
			iDmg = 50;
			break;
		case 3:
		case 4:
			iDmg = 60;
			break;
		case 5:
		case 6:
			iDmg = 70;
			break;
		case 7:
		case 8:
			iDmg = 80;
			break;
		default:
			iDmg = 80;
			break;
		}
	}
	else
	{
		switch ( MonsterScalePlayers() )
		{
		case 1:
		case 2:
			iDmg = 20;
			break;
		case 3:
		case 4:
			iDmg = 30;
			break;
		case 5:
		case 6:
			iDmg = 35;
			break;
		case 7:
		case 8:
			iDmg = 50;
			break;
		default:
			iDmg = 50;
			break;
		}
	}

	switch( pEvent->event )
	{
		case BSQUID_AE_SPIT:
		{
			// halflife-updated (bullsquid, VS#2834): the enemy can be cleared between the start of the
			// attack animation and this event.
			if ( m_hEnemy == NULL )
				break;

			Vector	vecSpitOffset;
			Vector	vecSpitDir;

			UTIL_MakeVectors ( pev->angles );

			// !!!HACKHACK - the spot at which the spit originates (in front of the mouth) was measured in 3ds and hardcoded here.
			// we should be able to read the position of bones at runtime for this info.
			vecSpitOffset = ( gpGlobals->v_right * 8 + gpGlobals->v_forward * 16 + gpGlobals->v_up * 64 );		
			vecSpitOffset = ( pev->origin + vecSpitOffset );
			vecSpitDir = ( ( m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs ) - vecSpitOffset ).Normalize();

			vecSpitDir.x += RANDOM_FLOAT( -0.05, 0.05 );
			vecSpitDir.y += RANDOM_FLOAT( -0.05, 0.05 );
			vecSpitDir.z += RANDOM_FLOAT( -0.05, 0 );


			// do stuff for this event.
			AttackSound();

			switch ( MonsterScalePlayers() )
			{
			case 1:
			case 2:
				CSquidSpit::Shoot( pev, vecSpitOffset, vecSpitDir * 900 );
				break;
			case 3:
			case 4:
				CSquidSpit::Shoot( pev, vecSpitOffset, vecSpitDir * 1100 );
				break;
			case 5:
			case 6:
				CSquidSpit::Shoot( pev, vecSpitOffset, vecSpitDir * 1300 );
				break;
			case 7:
			case 8:
				CSquidSpit::Shoot( pev, vecSpitOffset, vecSpitDir * 1500 );
				break;
			default:
				CSquidSpit::Shoot( pev, vecSpitOffset, vecSpitDir * 1500 );
				break;
			}
		}
		break;

		case BSQUID_AE_BITE:
		{
			// SOUND HERE!
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, iDmg, DMG_SLASH );
			
			if ( pHurt )
			{
				pHurt->pev->punchangle.z = -15;
				pHurt->pev->punchangle.x = -45;
				pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_forward * 100;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_up * 100;
			}
		}
		break;

		case BSQUID_AE_TAILWHIP:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, iDmg, DMG_CLUB | DMG_ALWAYSGIB );
			if ( pHurt ) 
			{
				pHurt->pev->punchangle.z = -20;
				pHurt->pev->punchangle.x = 20;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * 200;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_up * 100;
			}
		}
		break;

		case BSQUID_AE_BLINK:
		{
			// close eye. 
			pev->skin = 1;
		}
		break;

		case BSQUID_AE_HOP:
		{
			float flGravity = CVAR_GET_FLOAT( "sv_gravity" );

			// throw the squid up into the air on this frame.
			if ( FBitSet ( pev->flags, FL_ONGROUND ) )
			{
				pev->flags -= FL_ONGROUND;
			}

			// jump into air for 0.8 (24/30) seconds
//			pev->velocity.z += (0.875 * flGravity) * 0.5;
			pev->velocity.z += (0.625 * flGravity) * 0.5;
		}
		break;

		case BSQUID_AE_THROW:
			{
				int iPitch;

				// squid throws its prey IF the prey is a client. 
				CBaseEntity *pHurt = CheckTraceHullAttack( 70, 0, 0 );


				if ( pHurt )
				{
					// croonchy bite sound
					iPitch = RANDOM_FLOAT( 90, 110 );
					switch ( RANDOM_LONG( 0, 1 ) )
					{
					case 0:
						EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "bullchicken/bc_bite2.wav", 1, ATTN_NORM, 0, iPitch );	
						break;
					case 1:
						EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "bullchicken/bc_bite3.wav", 1, ATTN_NORM, 0, iPitch );	
						break;
					}

					
					pHurt->pev->punchangle.x = RANDOM_LONG(0,34) - 5;
					pHurt->pev->punchangle.z = RANDOM_LONG(0,49) - 25;
					pHurt->pev->punchangle.y = RANDOM_LONG(0,89) - 45;
		
					// screeshake transforms the viewmodel as well as the viewangle. No problems with seeing the ends of the viewmodels.
					UTIL_ScreenShake( pHurt->pev->origin, 25.0, 1.5, 0.7, 2 );

					if ( pHurt->IsPlayer() )
					{
						UTIL_MakeVectors( pev->angles );
						pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * 300 + gpGlobals->v_up * 300;
					}
				}
			}
		break;

		default:
			CBaseMonster::HandleAnimEvent( pEvent );
	}
}

//=========================================================
// Spawn
//=========================================================
void Cimp :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/imp.mdl");
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;

	if ( juniorpvmonst.value == 0 )
		pev->health = 250;
	else if ( juniorpvmonst.value == 1 )
		pev->health = 500;
	else if ( juniorpvmonst.value == 2 )
		pev->health = 750;
	else
		pev->health = 1400;

	m_flFieldOfView		= 0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	m_fCanThreatDisplay	= TRUE;
	m_flNextSpitTime = gpGlobals->time;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void Cimp :: Precache()
{
	PRECACHE_MODEL("models/imp.mdl");
	
	PRECACHE_MODEL("sprites/xfireball3.spr");// spit projectile.
	

	PRECACHE_SOUND("zombie/claw_miss2.wav");// because we use the basemonster SWIPE animation event

	PRECACHE_SOUND("bullchicken/bc_attack2.wav");
	PRECACHE_SOUND("bullchicken/bc_attack3.wav");
	
	PRECACHE_SOUND("bullchicken/bc_die1.wav");
	PRECACHE_SOUND("bullchicken/bc_die2.wav");
	PRECACHE_SOUND("bullchicken/bc_die3.wav");
	
	PRECACHE_SOUND("bullchicken/bc_idle1.wav");
	PRECACHE_SOUND("bullchicken/bc_idle2.wav");
	PRECACHE_SOUND("bullchicken/bc_idle3.wav");
	PRECACHE_SOUND("bullchicken/bc_idle4.wav");
	PRECACHE_SOUND("bullchicken/bc_idle5.wav");
	
	PRECACHE_SOUND("bullchicken/bc_pain1.wav");
	PRECACHE_SOUND("bullchicken/bc_pain2.wav");
	PRECACHE_SOUND("bullchicken/bc_pain3.wav");
	PRECACHE_SOUND("bullchicken/bc_pain4.wav");
	
	PRECACHE_SOUND("bullchicken/bc_attackgrowl.wav");
	PRECACHE_SOUND("bullchicken/bc_attackgrowl2.wav");
	PRECACHE_SOUND("bullchicken/bc_attackgrowl3.wav");

	PRECACHE_SOUND("weapons/burn_fire1.wav");

	PRECACHE_SOUND("bullchicken/bc_bite2.wav");
	PRECACHE_SOUND("bullchicken/bc_bite3.wav");

	PRECACHE_SOUND("weapons/burn_fire1.wav");
	PRECACHE_SOUND("weapons/burn_fire1.wav");

}	

//=========================================================
// DeathSound
//=========================================================
void Cimp :: DeathSound ( void )
{
	switch ( RANDOM_LONG(0,2) )
	{
	case 0:	
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "bullchicken/bc_die1.wav", 1, ATTN_NORM );	
		break;
	case 1:
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "bullchicken/bc_die2.wav", 1, ATTN_NORM );	
		break;
	case 2:
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "bullchicken/bc_die3.wav", 1, ATTN_NORM );	
		break;
	}
}

//=========================================================
// AttackSound
//=========================================================
void Cimp :: AttackSound ( void )
{
	switch ( RANDOM_LONG(0,1) )
	{
	case 0:
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "bullchicken/bc_attack2.wav", 1, ATTN_NORM );	
		break;
	case 1:
		EMIT_SOUND( ENT(pev), CHAN_WEAPON, "bullchicken/bc_attack3.wav", 1, ATTN_NORM );	
		break;
	}
}


//========================================================
// RunAI - overridden for bullsquid because there are things
// that need to be checked every think.
//========================================================
void Cimp :: RunAI ( void )
{
	// first, do base class stuff
	CBaseMonster :: RunAI();

	if ( pev->skin != 0 )
	{
		// close eye if it was open.
		pev->skin = 0; 
	}

	if ( RANDOM_LONG(0,39) == 0 )
	{
		pev->skin = 1;
	}

	if ( m_hEnemy != NULL && m_Activity == ACT_RUN )
	{
		// chasing enemy. Sprint for last bit
		if ( (pev->origin - m_hEnemy->pev->origin).Length2D() < SQUID_SPRINT_DIST )
		{
			pev->framerate = 1.25;
		}
	}

}

//========================================================
// AI Schedules Specific to this monster
//=========================================================

// primary range attack
Task_t	tlSquidRangeAttack1[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE	},
};

Schedule_t	slSquidRangeAttack1[] =
{
	{ 
		tlSquidRangeAttack1,
		ARRAYSIZE ( tlSquidRangeAttack1 ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED,
		0,
		"Squid Range Attack1"
	},
};

// Chase enemy schedule
Task_t tlSquidChaseEnemy1[] = 
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_RANGE_ATTACK1	},// !!!OEM - this will stop nasty squid oscillation.
	{ TASK_GET_PATH_TO_ENEMY,	(float)0					},
	{ TASK_RUN_PATH,			(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0					},
};

Schedule_t slSquidChaseEnemy[] =
{
	{ 
		tlSquidChaseEnemy1,
		ARRAYSIZE ( tlSquidChaseEnemy1 ),
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_SMELL_FOOD		|
		bits_COND_CAN_RANGE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_CAN_MELEE_ATTACK2	|
		bits_COND_TASK_FAILED		|
		bits_COND_HEAR_SOUND,
		
		bits_SOUND_DANGER			|
		bits_SOUND_MEAT,
		"Squid Chase Enemy"
	},
};

Task_t tlSquidHurtHop[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_SOUND_WAKE,			(float)0		},
	{ TASK_SQUID_HOPTURN,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},// in case squid didn't turn all the way in the air.
};

Schedule_t slSquidHurtHop[] =
{
	{
		tlSquidHurtHop,
		ARRAYSIZE ( tlSquidHurtHop ),
		0,
		0,
		"SquidHurtHop"
	}
};

Task_t tlSquidSeeCrab[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_SOUND_WAKE,			(float)0			},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_EXCITED	},
	{ TASK_FACE_ENEMY,			(float)0			},
};

Schedule_t slSquidSeeCrab[] =
{
	{
		tlSquidSeeCrab,
		ARRAYSIZE ( tlSquidSeeCrab ),
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE,
		0,
		"SquidSeeCrab"
	}
};

// squid walks to something tasty and eats it.
Task_t tlSquidEat[] =
{
	{ TASK_STOP_MOVING,				(float)0				},
	{ TASK_EAT,						(float)10				},// this is in case the squid can't get to the food
	{ TASK_STORE_LASTPOSITION,		(float)0				},
	{ TASK_GET_PATH_TO_BESTSCENT,	(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_EAT,						(float)50				},
	{ TASK_GET_PATH_TO_LASTPOSITION,(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_CLEAR_LASTPOSITION,		(float)0				},
};

Schedule_t slSquidEat[] =
{
	{
		tlSquidEat,
		ARRAYSIZE( tlSquidEat ),
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_NEW_ENEMY	,
		
		// even though HEAR_SOUND/SMELL FOOD doesn't break this schedule, we need this mask
		// here or the monster won't detect these sounds at ALL while running this schedule.
		bits_SOUND_MEAT			|
		bits_SOUND_CARCASS,
		"SquidEat"
	}
};

// this is a bit different than just Eat. We use this schedule when the food is far away, occluded, or behind
// the squid. This schedule plays a sniff animation before going to the source of food.
Task_t tlSquidSniffAndEat[] =
{
	{ TASK_STOP_MOVING,				(float)0				},
	{ TASK_EAT,						(float)10				},// this is in case the squid can't get to the food
	{ TASK_PLAY_SEQUENCE,			(float)ACT_DETECT_SCENT },
	{ TASK_STORE_LASTPOSITION,		(float)0				},
	{ TASK_GET_PATH_TO_BESTSCENT,	(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			},
	{ TASK_EAT,						(float)50				},
	{ TASK_GET_PATH_TO_LASTPOSITION,(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_CLEAR_LASTPOSITION,		(float)0				},
};

Schedule_t slSquidSniffAndEat[] =
{
	{
		tlSquidSniffAndEat,
		ARRAYSIZE( tlSquidSniffAndEat ),
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_NEW_ENEMY	,
		
		// even though HEAR_SOUND/SMELL FOOD doesn't break this schedule, we need this mask
		// here or the monster won't detect these sounds at ALL while running this schedule.
		bits_SOUND_MEAT			|
		bits_SOUND_CARCASS,
		"SquidSniffAndEat"
	}
};

// squid does this to stinky things. 
Task_t tlSquidWallow[] =
{
	{ TASK_STOP_MOVING,				(float)0				},
	{ TASK_EAT,						(float)10				},// this is in case the squid can't get to the stinkiness
	{ TASK_STORE_LASTPOSITION,		(float)0				},
	{ TASK_GET_PATH_TO_BESTSCENT,	(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_INSPECT_FLOOR},
	{ TASK_EAT,						(float)50				},// keeps squid from eating or sniffing anything else for a while.
	{ TASK_GET_PATH_TO_LASTPOSITION,(float)0				},
	{ TASK_WALK_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_CLEAR_LASTPOSITION,		(float)0				},
};

Schedule_t slSquidWallow[] =
{
	{
		tlSquidWallow,
		ARRAYSIZE( tlSquidWallow ),
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_NEW_ENEMY	,
		
		// even though HEAR_SOUND/SMELL FOOD doesn't break this schedule, we need this mask
		// here or the monster won't detect these sounds at ALL while running this schedule.
		bits_SOUND_GARBAGE,

		"SquidWallow"
	}
};

DEFINE_CUSTOM_SCHEDULES( Cimp ) 
{
	slSquidRangeAttack1,
	slSquidChaseEnemy,
	slSquidHurtHop,
	slSquidSeeCrab,
	slSquidEat,
	slSquidSniffAndEat,
	slSquidWallow
};

IMPLEMENT_CUSTOM_SCHEDULES( Cimp, CBaseMonster );

//=========================================================
// GetSchedule 
//=========================================================
Schedule_t *Cimp :: GetSchedule( void )
{
	switch	( m_MonsterState )
	{
	case MONSTERSTATE_ALERT:
		{
			if ( HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE) )
			{
				return GetScheduleOfType ( SCHED_TAKE_COVER_FROM_ENEMY );
			}

			if ( HasConditions(bits_COND_HEAR_SOUND) )
			{
				CSound *pSound;

				pSound = PBestSound();

				if ( pSound && (pSound->m_iType & bits_SOUND_DANGER) )
				{
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
				}

				if ( pSound && (pSound->m_iType & bits_SOUND_COMBAT) )
				{
					m_afMemory |= bits_MEMORY_PROVOKED;

					if ( RANDOM_LONG( 1, 4 ) <= 2 )
						return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
					else
						return GetScheduleOfType( SCHED_INVESTIGATE_SOUND );
				}

				return GetScheduleOfType( SCHED_ALERT_FACE );
			}

			if ( HasConditions(bits_COND_SMELL_FOOD) )
			{
				CSound		*pSound;

				pSound = PBestScent();
				
				if ( pSound && (!FInViewCone ( &pSound->m_vecOrigin ) || !FVisible ( pSound->m_vecOrigin )) )
				{
					// scent is behind or occluded
					return GetScheduleOfType( SCHED_SQUID_SNIFF_AND_EAT );
				}

				// food is right out in the open. Just go get it.
				return GetScheduleOfType( SCHED_SQUID_EAT );
			}

			if ( HasConditions(bits_COND_SMELL) )
			{
				// there's something stinky. 
				CSound		*pSound;

				pSound = PBestScent();
				if ( pSound )
					return GetScheduleOfType( SCHED_SQUID_WALLOW);
			}

			break;
		}
	case MONSTERSTATE_COMBAT:
		{
// dead enemy
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			{
				// call base class, all code to handle dead enemies is centralized there.
				return CBaseMonster :: GetSchedule();
			}

			if ( HasConditions( bits_COND_CAN_RANGE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_RANGE_ATTACK1 );
			}

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			{
				return GetScheduleOfType ( SCHED_MELEE_ATTACK1 );
			}

			if ( HasConditions( bits_COND_CAN_MELEE_ATTACK2 ) )
			{
				return GetScheduleOfType ( SCHED_MELEE_ATTACK2 );
			}
			
			return GetScheduleOfType ( SCHED_CHASE_ENEMY );

			break;
		}
	}

	return CBaseMonster :: GetSchedule();
}

//=========================================================
// GetScheduleOfType
//=========================================================
Schedule_t* Cimp :: GetScheduleOfType ( int Type ) 
{
	switch	( Type )
	{
	case SCHED_RANGE_ATTACK1:
		return &slSquidRangeAttack1[ 0 ];
		break;
	case SCHED_SQUID_HURTHOP:
		return &slSquidHurtHop[ 0 ];
		break;
	case SCHED_SQUID_SEECRAB:
		return &slSquidSeeCrab[ 0 ];
		break;
	case SCHED_SQUID_EAT:
		return &slSquidEat[ 0 ];
		break;
	case SCHED_SQUID_SNIFF_AND_EAT:
		return &slSquidSniffAndEat[ 0 ];
		break;
	case SCHED_SQUID_WALLOW:
		return &slSquidWallow[ 0 ];
		break;
	case SCHED_CHASE_ENEMY:
		return &slSquidChaseEnemy[ 0 ];
		break;
	}

	return CBaseMonster :: GetScheduleOfType ( Type );
}

//=========================================================
// Start task - selects the correct activity and performs
// any necessary calculations to start the next task on the
// schedule.  OVERRIDDEN for bullsquid because it needs to
// know explicitly when the last attempt to chase the enemy
// failed, since that impacts its attack choices.
//=========================================================
void Cimp :: StartTask ( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_MELEE_ATTACK2:
		{
			switch ( RANDOM_LONG ( 0, 2 ) )
			{
			case 0:	
				EMIT_SOUND( ENT(pev), CHAN_VOICE, "bullchicken/bc_attackgrowl.wav", 1, ATTN_NORM );		
				break;
			case 1:	
				EMIT_SOUND( ENT(pev), CHAN_VOICE, "bullchicken/bc_attackgrowl2.wav", 1, ATTN_NORM );	
				break;
			case 2:	
				EMIT_SOUND( ENT(pev), CHAN_VOICE, "bullchicken/bc_attackgrowl3.wav", 1, ATTN_NORM );	
				break;
			}

			CBaseMonster :: StartTask ( pTask );
			break;
		}
	case TASK_SQUID_HOPTURN:
		{
			SetActivity ( ACT_HOP );
			MakeIdealYaw ( m_vecEnemyLKP );
			break;
		}
	case TASK_GET_PATH_TO_ENEMY:
		{
			// SBO: same guard as CBaseMonster::StartTask (the enemy may be gone)
			if ( m_hEnemy == NULL )
			{
				TaskFail();
				break;
			}

			if ( BuildRoute ( m_hEnemy->pev->origin, bits_MF_TO_ENEMY, m_hEnemy ) )
			{
				m_iTaskStatus = TASKSTATUS_COMPLETE;
			}
			else
			{
				ALERT ( at_aiconsole, "GetPathToEnemy failed!!\n" );
				TaskFail();
			}
			break;
		}
	default:
		{
			CBaseMonster :: StartTask ( pTask );
			break;
		}
	}
}

//=========================================================
// RunTask
//=========================================================
void Cimp :: RunTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_SQUID_HOPTURN:
		{
			MakeIdealYaw( m_vecEnemyLKP );
			ChangeYaw( pev->yaw_speed );

			if ( m_fSequenceFinished )
			{
				m_iTaskStatus = TASKSTATUS_COMPLETE;
			}
			break;
		}
	default:
		{
			CBaseMonster :: RunTask( pTask );
			break;
		}
	}
}


//=========================================================
// GetIdealState - Overridden for Bullsquid to deal with
// the feature that makes it lose interest in headcrabs for 
// a while if something injures it. 
//=========================================================
MONSTERSTATE Cimp :: GetIdealState ( void )
{
	int	iConditions;

	iConditions = IScheduleFlags();
	
	// If no schedule conditions, the new ideal state is probably the reason we're in here.
	switch ( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		/*
		COMBAT goes to ALERT upon death of enemy
		*/
		{
			if ( m_hEnemy != NULL && ( iConditions & bits_COND_LIGHT_DAMAGE || iConditions & bits_COND_HEAVY_DAMAGE ) && FClassnameIs( m_hEnemy->pev, "monster_headcrab" ) )
			{
				// if the squid has a headcrab enemy and something hurts it, it's going to forget about the crab for a while.
				m_hEnemy = NULL;
				m_IdealMonsterState = MONSTERSTATE_ALERT;
			}
			break;
		}
	}

	m_IdealMonsterState = CBaseMonster :: GetIdealState();

	return m_IdealMonsterState;
}

