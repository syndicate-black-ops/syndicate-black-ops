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
class Cimp_baSpit : public CBaseEntity
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

LINK_ENTITY_TO_CLASS( imp_baspit, Cimp_baSpit );

TYPEDESCRIPTION	Cimp_baSpit::m_SaveData[] = 
{
	DEFINE_FIELD( Cimp_baSpit, m_maxFrame, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( Cimp_baSpit, CBaseEntity );

void Cimp_baSpit:: Spawn( void )
{
	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING( "imp_baspit" );
	
	pev->solid = SOLID_BBOX;
	SET_MODEL(ENT(pev), "sprites/xfireball3.spr");
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->renderamt = 255;
	pev->scale = RANDOM_FLOAT( 0.1, 2.2 );

	UTIL_SetSize( pev, Vector( 0, 0, 0), Vector(0, 0, 0) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
}

void Cimp_baSpit::Animate( void )
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

void Cimp_baSpit::Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	Cimp_baSpit *pSpit = GetClassPtr( (Cimp_baSpit *)NULL );
	pSpit->Spawn();
	
	UTIL_SetOrigin( pSpit->pev, vecStart );
	pSpit->pev->velocity = vecVelocity;
	pSpit->pev->owner = ENT(pevOwner);

	pSpit->SetThink ( &Cimp_baSpit::Animate );
	pSpit->pev->nextthink = gpGlobals->time + 0.1;
}

void Cimp_baSpit :: Touch ( CBaseEntity *pOther )
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
				pOther->TakeDamage ( pev, pev, 80, DMG_BURN );
				break;
			default:
				pOther->TakeDamage ( pev, pev, 80, DMG_BURN );
				break;
			}
		}
		else if ( juniorpowmonst.value == 3 )
		{
			switch ( MonsterScalePlayers() )
			{
			case 1:
			case 2:
				pOther->TakeDamage ( pev, pev, 50, DMG_BURN );
				break;
			case 3:
			case 4:
				pOther->TakeDamage ( pev, pev, 60, DMG_BURN );
				break;
			case 5:
			case 6:
				pOther->TakeDamage ( pev, pev, 80, DMG_BURN );
				break;
			case 7:
			case 8:
				pOther->TakeDamage ( pev, pev, 120, DMG_BURN );
				break;
			default:
				pOther->TakeDamage ( pev, pev, 120, DMG_BURN );
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

	SetThink ( &Cimp_baSpit::SUB_Remove );
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

class Cimp_hunter : public CBaseMonster
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
	void Look( int iDistance );
	int CheckEnemy( CBaseEntity *pEnemy );
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
LINK_ENTITY_TO_CLASS( monster_imp_hunter, Cimp_hunter );

TYPEDESCRIPTION	Cimp_hunter::m_SaveData[] = 
{
	DEFINE_FIELD( Cimp_hunter, m_fCanThreatDisplay, FIELD_BOOLEAN ),
	DEFINE_FIELD( Cimp_hunter, m_flLastHurtTime, FIELD_TIME ),
	DEFINE_FIELD( Cimp_hunter, m_flNextSpitTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( Cimp_hunter, CBaseMonster );

//=========================================================
// IgnoreConditions 
//=========================================================
int Cimp_hunter::IgnoreConditions ( void )
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
int Cimp_hunter::IRelationship ( CBaseEntity *pTarget )
{
	return CBaseMonster :: IRelationship ( pTarget );
}

//=========================================================
// TakeDamage - overridden for bullsquid so we can keep track
// of how much time has passed since it was last injured
//=========================================================
int Cimp_hunter :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
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
BOOL Cimp_hunter :: CheckRangeAttack1 ( float flDot, float flDist )
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
BOOL Cimp_hunter :: CheckMeleeAttack1 ( float flDot, float flDist )
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
BOOL Cimp_hunter :: CheckMeleeAttack2 ( float flDot, float flDist )
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
BOOL Cimp_hunter :: FValidateHintType ( short sHint )
{
	int i;

	static short simp_baHints[] =
	{
		HINT_WORLD_HUMAN_BLOOD,
	};

	for ( i = 0 ; i < ARRAYSIZE ( simp_baHints ) ; i++ )
	{
		if ( simp_baHints[ i ] == sHint )
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
int Cimp_hunter :: ISoundMask ( void )
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
int	Cimp_hunter :: Classify ( void )
{
	return	CLASS_ALIEN_MILITARY;
}

//=========================================================
// IdleSound 
//=========================================================
#define SQUID_ATTN_IDLE	(float)1.5
void Cimp_hunter :: IdleSound ( void )
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
void Cimp_hunter :: PainSound ( void )
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
void Cimp_hunter :: AlertSound ( void )
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
void Cimp_hunter :: SetYawSpeed ( void )
{
	pev->yaw_speed = 500;
	pev->framerate = 1.8;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void Cimp_hunter :: HandleAnimEvent( MonsterEvent_t *pEvent )
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
			vecSpitOffset = ( gpGlobals->v_right * 8 + gpGlobals->v_forward * 16 + gpGlobals->v_up * 68 );		
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
				Cimp_baSpit::Shoot( pev, vecSpitOffset, vecSpitDir * 900 );
				break;
			case 3:
			case 4:
				Cimp_baSpit::Shoot( pev, vecSpitOffset, vecSpitDir * 1100 );
				break;
			case 5:
			case 6:
				Cimp_baSpit::Shoot( pev, vecSpitOffset, vecSpitDir * 1300 );
				break;
			case 7:
			case 8:
				Cimp_baSpit::Shoot( pev, vecSpitOffset, vecSpitDir * 1500 );
				break;
			default:
				Cimp_baSpit::Shoot( pev, vecSpitOffset, vecSpitDir * 1500 );
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
void Cimp_hunter :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/imp.mdl");
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;

	if ( juniorpvmonst.value == 0 )
		pev->health = 200;
	else if ( juniorpvmonst.value == 1 )
		pev->health = 410;
	else if ( juniorpvmonst.value == 2 )
		pev->health = 680;
	else
		pev->health = 1200;

	m_flFieldOfView		= 0.2;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_COMBAT;

	m_fCanThreatDisplay	= TRUE;
	m_flNextSpitTime = gpGlobals->time;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void Cimp_hunter :: Precache()
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
void Cimp_hunter :: DeathSound ( void )
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
void Cimp_hunter :: AttackSound ( void )
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
// AI Schedules Specific to this monster
//=========================================================

// primary range attack
Task_t	tlimp_baRangeAttack1[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE	},
};

Schedule_t	slimp_baRangeAttack1[] =
{
	{ 
		tlimp_baRangeAttack1,
		ARRAYSIZE ( tlimp_baRangeAttack1 ), 
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_HEAVY_DAMAGE		|
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED,
		0,
		"imp_ba Range Attack1"
	},
};

// Chase enemy schedule
Task_t tlimp_baChaseEnemy1[] = 
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_RANGE_ATTACK1	},// !!!OEM - this will stop nasty squid oscillation.
	{ TASK_GET_PATH_TO_ENEMY,	(float)0					},
	{ TASK_RUN_PATH,			(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0					},
};

Schedule_t slimp_baChaseEnemy[] =
{
	{ 
		tlimp_baChaseEnemy1,
		ARRAYSIZE ( tlimp_baChaseEnemy1 ),
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
		"imp_ba Chase Enemy"
	},
};

Task_t tlimp_baHurtHop[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_SOUND_WAKE,			(float)0		},
	{ TASK_SQUID_HOPTURN,		(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},// in case squid didn't turn all the way in the air.
};

Schedule_t slimp_baHurtHop[] =
{
	{
		tlimp_baHurtHop,
		ARRAYSIZE ( tlimp_baHurtHop ),
		0,
		0,
		"imp_baHurtHop"
	}
};

Task_t tlimp_baSeeCrab[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_SOUND_WAKE,			(float)0			},
	{ TASK_PLAY_SEQUENCE,		(float)ACT_EXCITED	},
	{ TASK_FACE_ENEMY,			(float)0			},
};

Schedule_t slimp_baSeeCrab[] =
{
	{
		tlimp_baSeeCrab,
		ARRAYSIZE ( tlimp_baSeeCrab ),
		bits_COND_LIGHT_DAMAGE		|
		bits_COND_HEAVY_DAMAGE,
		0,
		"imp_baSeeCrab"
	}
};

// squid walks to something tasty and eats it.
Task_t tlimp_baEat[] =
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

Schedule_t slimp_baEat[] =
{
	{
		tlimp_baEat,
		ARRAYSIZE( tlimp_baEat ),
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_NEW_ENEMY	,
		
		// even though HEAR_SOUND/SMELL FOOD doesn't break this schedule, we need this mask
		// here or the monster won't detect these sounds at ALL while running this schedule.
		bits_SOUND_MEAT			|
		bits_SOUND_CARCASS,
		"imp_baEat"
	}
};

// this is a bit different than just Eat. We use this schedule when the food is far away, occluded, or behind
// the squid. This schedule plays a sniff animation before going to the source of food.
Task_t tlimp_baSniffAndEat[] =
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

Schedule_t slimp_baSniffAndEat[] =
{
	{
		tlimp_baSniffAndEat,
		ARRAYSIZE( tlimp_baSniffAndEat ),
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_NEW_ENEMY	,
		
		// even though HEAR_SOUND/SMELL FOOD doesn't break this schedule, we need this mask
		// here or the monster won't detect these sounds at ALL while running this schedule.
		bits_SOUND_MEAT			|
		bits_SOUND_CARCASS,
		"imp_baSniffAndEat"
	}
};

// squid does this to stinky things. 
Task_t tlimp_baWallow[] =
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

Schedule_t slimp_baWallow[] =
{
	{
		tlimp_baWallow,
		ARRAYSIZE( tlimp_baWallow ),
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_NEW_ENEMY	,
		
		// even though HEAR_SOUND/SMELL FOOD doesn't break this schedule, we need this mask
		// here or the monster won't detect these sounds at ALL while running this schedule.
		bits_SOUND_GARBAGE,

		"imp_baWallow"
	}
};

DEFINE_CUSTOM_SCHEDULES( Cimp_hunter ) 
{
	slimp_baRangeAttack1,
	slimp_baChaseEnemy,
	slimp_baHurtHop,
	slimp_baSeeCrab,
	slimp_baEat,
	slimp_baSniffAndEat,
	slimp_baWallow
};

IMPLEMENT_CUSTOM_SCHEDULES( Cimp_hunter, CBaseMonster );

//=========================================================
// GetSchedule 
//=========================================================
Schedule_t *Cimp_hunter :: GetSchedule( void )
{
	switch	( m_MonsterState )
	{
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

			break;
		}
	}

	return CBaseMonster :: GetSchedule();
}

//=========================================================
// GetScheduleOfType
//=========================================================
Schedule_t* Cimp_hunter :: GetScheduleOfType ( int Type ) 
{
	switch	( Type )
	{
	case SCHED_RANGE_ATTACK1:
		return &slimp_baRangeAttack1[ 0 ];
		break;
	case SCHED_SQUID_HURTHOP:
		return &slimp_baHurtHop[ 0 ];
		break;
	case SCHED_SQUID_SEECRAB:
		return &slimp_baSeeCrab[ 0 ];
		break;
	case SCHED_SQUID_EAT:
		return &slimp_baEat[ 0 ];
		break;
	case SCHED_SQUID_SNIFF_AND_EAT:
		return &slimp_baSniffAndEat[ 0 ];
		break;
	case SCHED_SQUID_WALLOW:
		return &slimp_baWallow[ 0 ];
		break;
	case SCHED_CHASE_ENEMY:
		return &slimp_baChaseEnemy[ 0 ];
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
void Cimp_hunter :: StartTask ( Task_t *pTask )
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
void Cimp_hunter :: RunTask ( Task_t *pTask )
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
MONSTERSTATE Cimp_hunter :: GetIdealState ( void )
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

//=========================================================
// RunAI - SBO: CBaseMonster::RunAI without the PVS check
//=========================================================
void Cimp_hunter :: RunAI ( void )
{
	if ( ( m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_ALERT ) && RANDOM_LONG(0,99) == 0 && !(pev->spawnflags & SF_MONSTER_GAG) ) // halflife-updated (VS#3076): gag is a spawnflag
	{
		IdleSound();
	}

	if ( m_MonsterState != MONSTERSTATE_NONE	&& 
		 m_MonsterState != MONSTERSTATE_PRONE   && 
		 m_MonsterState != MONSTERSTATE_DEAD )
	{
		Look( m_flDistLook );
		Listen();

		ClearConditions( IgnoreConditions() );

		GetEnemy();

		if ( m_hEnemy != NULL )
		{
			CheckEnemy( m_hEnemy );
		}

		CheckAmmo();
	}

	FCheckAITrigger();

	PrescheduleThink();

	MaintainSchedule();
}

//=========================================================
// Look - SBO: CBaseMonster::Look with a fixed 12000 unit
// range and no view cone / visibility checks; FL_NOTARGET
// entities are not seen
//=========================================================
void Cimp_hunter :: Look ( int iDistance )
{
	int	iSighted = 0;

	ClearConditions(bits_COND_SEE_HATE | bits_COND_SEE_DISLIKE | bits_COND_SEE_ENEMY | bits_COND_SEE_FEAR | bits_COND_SEE_NEMESIS | bits_COND_SEE_CLIENT);

	m_pLink = NULL;

	CBaseEntity	*pSightEnt = NULL;

	if ( !FBitSet( pev->spawnflags, SF_MONSTER_PRISONER ) )
	{
		CBaseEntity *pList[100];

		Vector delta = Vector( 12000, 12000, 12000 );

		int count = UTIL_EntitiesInBox( pList, 100, pev->origin - delta, pev->origin + delta, FL_CLIENT|FL_MONSTER );
		for ( int i = 0; i < count; i++ )
		{
			pSightEnt = pList[i];
			if ( pSightEnt != this												&& 
				 !FBitSet( pSightEnt->pev->spawnflags, SF_MONSTER_PRISONER )	&& 
				 pSightEnt->pev->health > 0 &&
				 pSightEnt->IsAlive() )
			{
				// SBO 2.1: "prisonerto" / "ignoredby", as in CBaseMonster::Look (this Look left them out)
				if ( LookSkips( pSightEnt ) )
					continue;
				// SBO 2.0: FL_NOTARGET entities are not seen, as in CBaseMonster::Look
				if ( IRelationship( pSightEnt ) != R_NO && !FBitSet( pSightEnt->pev->flags, FL_NOTARGET ) )
				{
					if ( pSightEnt->IsPlayer() )
					{
						if ( pev->spawnflags & SF_MONSTER_WAIT_TILL_SEEN )
						{
							CBaseMonster *pClient;

							pClient = pSightEnt->MyMonsterPointer();
							if ( pSightEnt && !pClient->FInViewCone( this ) )
							{
								continue;
							}
							else
							{
								pev->spawnflags &= ~SF_MONSTER_WAIT_TILL_SEEN;
							}
						}

						iSighted |= bits_COND_SEE_CLIENT;
					}

					pSightEnt->m_pLink = m_pLink;
					m_pLink = pSightEnt;

					iSighted |= bits_COND_SEE_ENEMY;

					switch ( IRelationship ( pSightEnt ) )
					{
					case	R_NM:
						iSighted |= bits_COND_SEE_NEMESIS;		
						break;
					case	R_HT:		
						iSighted |= bits_COND_SEE_HATE;		
						break;
					case	R_DL:
						iSighted |= bits_COND_SEE_DISLIKE;
						break;
					case	R_FR:
						iSighted |= bits_COND_SEE_FEAR;
						break;
					case    R_AL:
						break;
					default:
						ALERT ( at_aiconsole, "%s can't assess %s\n", STRING(pev->classname), STRING(pSightEnt->pev->classname ) );
						break;
					}
				}
			}
		}
	}
	
	SetConditions( iSighted );
}

//=========================================================
// CheckEnemy
//=========================================================
int Cimp_hunter :: CheckEnemy ( CBaseEntity *pEnemy )
{
	float	flDistToEnemy;
	int		iUpdatedLKP;// set this to TRUE if you update the EnemyLKP in this function.

	iUpdatedLKP = FALSE;
	ClearConditions ( bits_COND_ENEMY_FACING_ME );
	
	if ( !FVisible( pEnemy ) )
	{
		ASSERT(!HasConditions(bits_COND_SEE_ENEMY));
		SetConditions( bits_COND_ENEMY_OCCLUDED );
	}
	else
		ClearConditions( bits_COND_ENEMY_OCCLUDED );

	if ( !pEnemy->IsAlive() )
	{
		SetConditions ( bits_COND_ENEMY_DEAD );
		ClearConditions( bits_COND_SEE_ENEMY | bits_COND_ENEMY_OCCLUDED );
		return FALSE;
	}

	Vector vecEnemyPos = pEnemy->pev->origin;
	// distance to enemy's origin
	flDistToEnemy = ( vecEnemyPos - pev->origin ).Length();
	vecEnemyPos.z += pEnemy->pev->size.z * 0.5;
	// distance to enemy's head
	float flDistToEnemy2 = (vecEnemyPos - pev->origin).Length();
	if (flDistToEnemy2 < flDistToEnemy)
		flDistToEnemy = flDistToEnemy2;
	else
	{
		// distance to enemy's feet
		vecEnemyPos.z -= pEnemy->pev->size.z;
		float flDistToEnemy2 = (vecEnemyPos - pev->origin).Length();
		if (flDistToEnemy2 < flDistToEnemy)
			flDistToEnemy = flDistToEnemy2;
	}

	if ( HasConditions( bits_COND_SEE_ENEMY ) )
	{
		CBaseMonster *pEnemyMonster;

		iUpdatedLKP = TRUE;
		m_vecEnemyLKP = pEnemy->pev->origin;

		pEnemyMonster = pEnemy->MyMonsterPointer();

		if ( pEnemyMonster )
		{
			if ( pEnemyMonster->FInViewCone ( this ) )
			{
				SetConditions ( bits_COND_ENEMY_FACING_ME );
			}
			else
				ClearConditions( bits_COND_ENEMY_FACING_ME );
		}

		if (pEnemy->pev->velocity != Vector( 0, 0, 0))
		{
			// trail the enemy a bit
			m_vecEnemyLKP = m_vecEnemyLKP - pEnemy->pev->velocity * RANDOM_FLOAT( -0.05, 0 );
		}
		else
		{
			// UNDONE: use pev->oldorigin?
		}
	}
	else if ( !HasConditions(bits_COND_ENEMY_OCCLUDED|bits_COND_SEE_ENEMY) && ( flDistToEnemy <= 256 ) )
	{
		// if the enemy is not occluded, and unseen, that means it is behind or beside the monster.
		// if the enemy is near enough the monster, we go ahead and let the monster know where the
		// enemy is. 
		iUpdatedLKP = TRUE;
		m_vecEnemyLKP = pEnemy->pev->origin;
	}

	if ( flDistToEnemy >= m_flDistTooFar )
	{
		// enemy is very far away from monster
		SetConditions( bits_COND_ENEMY_TOOFAR );
	}
	else
		ClearConditions( bits_COND_ENEMY_TOOFAR );

	if ( FCanCheckAttacks() )	
	{
		CheckAttacks ( m_hEnemy, flDistToEnemy );
	}

	if ( m_movementGoal == MOVEGOAL_ENEMY )
	{
		for ( int i = m_iRouteIndex; i < ROUTE_SIZE; i++ )
		{
			if ( m_Route[ i ].iType == (bits_MF_IS_GOAL|bits_MF_TO_ENEMY) )
			{
				// UNDONE: Should we allow monsters to override this distance (80?)
				if ( (m_Route[ i ].vecLocation - m_vecEnemyLKP).Length() > 80 )
				{
					// Refresh
					FRefreshRoute();
					return iUpdatedLKP;
				}
			}
		}
	}

	return iUpdatedLKP;
}

