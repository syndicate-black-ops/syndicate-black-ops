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
// SBO: werewolf (CWerewolf_hunter, derived from SDK islave.cpp via hunter.cpp; melee only, no beams)
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"squadmonster.h"
#include	"schedule.h"
#include	"effects.h"
#include	"weapons.h"
#include	"soundent.h"
#include	"player.h"
#include	"sbo_monster.h"

extern DLL_GLOBAL int		g_iSkillLevel;

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		ISLAVE_AE_CLAW		( 1 )
#define		ISLAVE_AE_CLAWRAKE	( 2 )
#define		ISLAVE_AE_ZAP_POWERUP	( 3 )
#define		ISLAVE_AE_ZAP_SHOOT		( 4 )
#define		ISLAVE_AE_ZAP_DONE		( 5 )

class CWerewolf_hunter : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int	 ISoundMask( void );
	int  Classify ( void );
	int  IRelationship( CBaseEntity *pTarget );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	void CallForHelp( char *szClassname, float flDist, EHANDLE hEnemy, Vector &vecLocation );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);

	void DeathSound( void );
	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );

	void Killed( entvars_t *pevAttacker, int iGib );

	void RunAI( void );
	void Look( int iDistance );
	int CheckEnemy( CBaseEntity *pEnemy );

    void StartTask ( Task_t *pTask );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType ( int Type );
	CUSTOM_SCHEDULES;

	int	Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iBravery;

	float m_flNextAttack;

	int	m_voicePitch;

	EHANDLE m_hDead;

	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];
	static const char *pPainSounds[];
	static const char *pDeathSounds[];
};
LINK_ENTITY_TO_CLASS( monster_werewolf_hunter, CWerewolf_hunter );


TYPEDESCRIPTION	CWerewolf_hunter::m_SaveData[] =
{
	DEFINE_FIELD( CWerewolf_hunter, m_iBravery, FIELD_INTEGER ),
	DEFINE_FIELD( CWerewolf_hunter, m_flNextAttack, FIELD_TIME ),

	DEFINE_FIELD( CWerewolf_hunter, m_voicePitch, FIELD_INTEGER ),

	DEFINE_FIELD( CWerewolf_hunter, m_hDead, FIELD_EHANDLE ),

};

IMPLEMENT_SAVERESTORE( CWerewolf_hunter, CSquadMonster );




const char *CWerewolf_hunter::pAttackHitSounds[] =
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CWerewolf_hunter::pAttackMissSounds[] =
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CWerewolf_hunter::pPainSounds[] =
{
	"werewolf/were_pain1.wav",
	"werewolf/were_pain2.wav",
};

const char *CWerewolf_hunter::pDeathSounds[] =
{
	"werewolf/were_die1.wav",
	"werewolf/were_die2.wav",
};

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CWerewolf_hunter :: Classify ( void )
{
	return	CLASS_ALIEN_MILITARY;
}


int CWerewolf_hunter::IRelationship( CBaseEntity *pTarget )
{
	if ( (pTarget->IsPlayer()) )
		if ( (pev->spawnflags & SF_MONSTER_WAIT_UNTIL_PROVOKED ) && ! (m_afMemory & bits_MEMORY_PROVOKED ))
			return R_NM;
	return CBaseMonster::IRelationship( pTarget );
}


void CWerewolf_hunter :: CallForHelp( char *szClassname, float flDist, EHANDLE hEnemy, Vector &vecLocation )
{
	// ALERT( at_aiconsole, "help " );

	// skip ones not on my netname
	if ( FStringNull( pev->netname ))
		return;

	CBaseEntity *pEntity = NULL;

	while ((pEntity = UTIL_FindEntityByString( pEntity, "netname", STRING( pev->netname ))) != NULL)
	{
		float d = (pev->origin - pEntity->pev->origin).Length();
		if (d < flDist)
		{
			CBaseMonster *pMonster = pEntity->MyMonsterPointer( );
			if (pMonster)
			{
				pMonster->m_afMemory |= bits_MEMORY_PROVOKED;
				pMonster->PushEnemy( hEnemy, vecLocation );
			}
		}
	}
}


//=========================================================
// ALertSound - scream
//=========================================================
void CWerewolf_hunter :: AlertSound( void )
{
	if ( m_hEnemy != NULL )
	{
		SENTENCEG_PlayRndSz(ENT(pev), "WERE_ALERT", 0.85, ATTN_NORM, 0, m_voicePitch);

		CallForHelp( "monster_werewolf_hunter", 512, m_hEnemy, m_vecEnemyLKP );
	}
}

//=========================================================
// IdleSound
//=========================================================
void CWerewolf_hunter :: IdleSound( void )
{
	if (RANDOM_LONG( 0, 2 ) == 0)
	{
		SENTENCEG_PlayRndSz(ENT(pev), "WERE_IDLE", 0.85, ATTN_NORM, 0, m_voicePitch);
	}
}

//=========================================================
// PainSound
//=========================================================
void CWerewolf_hunter :: PainSound( void )
{
	if (RANDOM_LONG( 0, 2 ) == 0)
	{
		EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pPainSounds[ RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1) ], 1.0, ATTN_NORM, 0, m_voicePitch );
	}
}

//=========================================================
// DieSound
//=========================================================

void CWerewolf_hunter :: DeathSound( void )
{
	EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pDeathSounds[ RANDOM_LONG(0,ARRAYSIZE(pDeathSounds)-1) ], 1.0, ATTN_NORM, 0, m_voicePitch );
}


//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards.
//=========================================================
int CWerewolf_hunter :: ISoundMask ( void)
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_DANGER	|
			bits_SOUND_PLAYER;
}


void CWerewolf_hunter::Killed( entvars_t *pevAttacker, int iGib )
{
	CSquadMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CWerewolf_hunter :: SetYawSpeed ( void )
{
	pev->yaw_speed = 2000;
	pev->framerate = 1.5;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CWerewolf_hunter :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	short iDamage;

	if ( juniorpowmonst.value == 0 )
	{
		switch ( m_iParticip1 )
		{
		case 1: case 2: iDamage = 10; break;
		case 3: case 4: iDamage = 15; break;
		case 5: case 6: iDamage = 20; break;
		case 7: case 8: iDamage = 30; break;
		default: iDamage = 30; break;
		}
	}
	else if ( juniorpowmonst.value == 2 )
	{
		switch ( m_iParticip1 )
		{
		case 1: case 2: iDamage = 40; break;
		case 3: case 4: iDamage = 50; break;
		case 5: case 6: iDamage = 55; break;
		case 7: case 8: iDamage = 65; break;
		default: iDamage = 65; break;
		}
	}
	else if ( juniorpowmonst.value == 3 )
	{
		switch ( m_iParticip1 )
		{
		case 1: case 2: iDamage = 55; break;
		case 3: case 4: iDamage = 65; break;
		case 5: case 6: iDamage = 75; break;
		case 7: case 8: iDamage = 100; break;
		default: iDamage = 100; break;
		}
	}
	else
	{
		switch ( m_iParticip1 )
		{
		case 1: case 2: iDamage = 20; break;
		case 3: case 4: iDamage = 25; break;
		case 5: case 6: iDamage = 30; break;
		case 7: case 8: iDamage = 40; break;
		default: iDamage = 40; break;
		}
	}

	// ALERT( at_console, "event %d : %f\n", pEvent->event, pev->frame );
	switch( pEvent->event )
	{
		case ISLAVE_AE_CLAW:
		{
			// SOUND HERE!
			CBaseEntity *pHurt = CheckTraceHullAttack( 100, iDamage, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = -12;
					pHurt->pev->punchangle.x = 10;
					pHurt->pev->punchangle.y = -10;
				}
				// Play a random attack hit sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, m_voicePitch );
			}
			else
			{
				// Play a random attack miss sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, m_voicePitch );
			}
		}
		break;

		case ISLAVE_AE_CLAWRAKE:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 100, iDamage, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = -18;
					pHurt->pev->punchangle.x = 5;
				}
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, m_voicePitch );
			}
			else
			{
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, m_voicePitch );
			}
		}
		break;

		default:
			CSquadMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// CheckRangeAttack1 - normal beam attack
//=========================================================
BOOL CWerewolf_hunter :: CheckRangeAttack1 ( float flDot, float flDist )
{
	return FALSE;
}

//=========================================================
// CheckRangeAttack2 - check bravery and try to resurect dead comrades
//=========================================================
BOOL CWerewolf_hunter :: CheckRangeAttack2 ( float flDot, float flDist )
{
	return FALSE;
}


//=========================================================
// StartTask
//=========================================================
void CWerewolf_hunter :: StartTask ( Task_t *pTask )
{
	CSquadMonster :: StartTask ( pTask );
}


//=========================================================
// Spawn
//=========================================================
void CWerewolf_hunter :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/werewolf.mdl");
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;
	if ( juniorpvmonst.value == 0 )
		pev->health		= 500;
	else if ( juniorpvmonst.value == 1 )
		pev->health		= 900;
	else if ( juniorpvmonst.value == 2 )
		pev->health		= 1500;
	else
		pev->health		= 2500;
	pev->view_ofs		= Vector ( 0, 0, 64 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_FULL;
	m_MonsterState		= MONSTERSTATE_ALERT;
	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_RANGE_ATTACK2 | bits_CAP_DOORS_GROUP;

	m_voicePitch		= RANDOM_LONG( 85, 110 );

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CWerewolf_hunter :: Precache()
{
	int i;

	PRECACHE_MODEL("models/werewolf.mdl");
	PRECACHE_SOUND("zombie/zo_pain2.wav");
	PRECACHE_SOUND("headcrab/hc_headbite.wav");
	PRECACHE_SOUND("weapons/cbar_miss1.wav");

	for ( i = 0; i < ARRAYSIZE( pAttackHitSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackHitSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackMissSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackMissSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
		PRECACHE_SOUND((char *)pPainSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pDeathSounds ); i++ )
		PRECACHE_SOUND((char *)pDeathSounds[i]);

	UTIL_PrecacheOther( "test_effect" );
}


//=========================================================
// TakeDamage - get provoked when injured
//=========================================================

int CWerewolf_hunter :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// don't slash one of your own
	if ((bitsDamageType & DMG_SLASH) && pevAttacker && IRelationship( Instance(pevAttacker) ) < R_DL)
		return 0;

	m_afMemory |= bits_MEMORY_PROVOKED;

	if ( IsAlive() && pev->health <= 200 && RANDOM_LONG( 1, 10 ) <= 2 )
	{
		if ( juniorpvmonst.value == 0 )
			pev->health += 150;
		else if ( juniorpvmonst.value == 1 )
			pev->health += 300;
		else if ( juniorpvmonst.value == 2 )
			pev->health += 350;
		else
			pev->health += 550;
	}

	if ( bitsDamageType & DMG_BLAST )
		flDamage *= 10;

	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );

	if ( IsAlive() && pevAttacker )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pevAttacker );

		if ( pPlayer && pPlayer->Classify() == CLASS_PLAYER && XP.value == 1 )
		{
			SBO_XP( pPlayer ) += flDamage;

			if ( SBO_XP( pPlayer ) >= 5000 )
			{
				SBO_XP( pPlayer ) = 0;
				SBO_XP2( pPlayer ) += 5;
				SBO_XP3( pPlayer ) += 2;
				SBO_XP4( pPlayer ) += 10;
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

				MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
					WRITE_BYTE( ENTINDEX( pPlayer->edict() ) );
					WRITE_SHORT( pPlayer->pev->frags );
					WRITE_SHORT( SBO_DEATHS( pPlayer ) );
					WRITE_SHORT( 0 );
					WRITE_SHORT( 0 );
				MESSAGE_END();
			}
		}
	}

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}


void CWerewolf_hunter::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	CSquadMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}


//=========================================================
// AI Schedules Specific to this monster
//=========================================================



// primary range attack
Task_t	tlWerewolf_hunterAttack1[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
};

Schedule_t	slWerewolf_hunterAttack1[] =
{
	{
		tlWerewolf_hunterAttack1,
		ARRAYSIZE ( tlWerewolf_hunterAttack1 ),
		bits_COND_CAN_MELEE_ATTACK1 |
		bits_COND_HEAR_SOUND |
		bits_COND_HEAVY_DAMAGE,

		bits_SOUND_DANGER,
		"Werewolf_hunter Range Attack1"
	},
};


DEFINE_CUSTOM_SCHEDULES( CWerewolf_hunter )
{
	slWerewolf_hunterAttack1,
};

IMPLEMENT_CUSTOM_SCHEDULES( CWerewolf_hunter, CSquadMonster );


//=========================================================
//=========================================================
Schedule_t *CWerewolf_hunter :: GetSchedule( void )
{
	if (pev->spawnflags)
	{
		pev->spawnflags = 0;
		return GetScheduleOfType( SCHED_RELOAD );
	}

	switch (m_MonsterState)
	{
	case MONSTERSTATE_COMBAT:
// dead enemy
		if ( HasConditions( bits_COND_ENEMY_DEAD ) )
		{
			// call base class, all code to handle dead enemies is centralized there.
			return CBaseMonster :: GetSchedule();
		}

		if (pev->health < 0 || m_iBravery < 0)
		{
			if (!HasConditions( bits_COND_CAN_MELEE_ATTACK1 ))
			{
				m_failSchedule = SCHED_CHASE_ENEMY;
				if (HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
				{
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
				if ( HasConditions ( bits_COND_SEE_ENEMY ) && HasConditions ( bits_COND_ENEMY_FACING_ME ) )
				{
					// ALERT( at_console, "exposed\n");
					return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );
				}
			}
		}
		break;
	}
	return CSquadMonster::GetSchedule( );
}


Schedule_t *CWerewolf_hunter :: GetScheduleOfType ( int Type )
{
	switch	( Type )
	{
	case SCHED_FAIL:
		if (HasConditions( bits_COND_CAN_MELEE_ATTACK1 ))
		{
			return CSquadMonster :: GetScheduleOfType( SCHED_MELEE_ATTACK1 ); ;
		}
		break;
	case SCHED_RANGE_ATTACK1:
		return slWerewolf_hunterAttack1;
	case SCHED_RANGE_ATTACK2:
		return slWerewolf_hunterAttack1;
	}
	return CSquadMonster :: GetScheduleOfType( Type );
}


//=========================================================
// RunAI - SBO: CBaseMonster::RunAI without the PVS check
//=========================================================
void CWerewolf_hunter :: RunAI ( void )
{
	if ( ( m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_ALERT ) && RANDOM_LONG(0,99) == 0 && !(pev->flags & SF_MONSTER_GAG) )
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
// range and no view cone / visibility checks
//=========================================================
void CWerewolf_hunter :: Look ( int iDistance )
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
				 pSightEnt->pev->health > 0 )
			{
				if ( IRelationship( pSightEnt ) != R_NO )
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
int CWerewolf_hunter :: CheckEnemy ( CBaseEntity *pEnemy )
{
	int iUpdatedLKP;

	iUpdatedLKP = CBaseMonster :: CheckEnemy ( m_hEnemy );
	
	if ( InSquad() && (CBaseEntity *)m_hEnemy == MySquadLeader()->m_hEnemy )
	{
		if ( iUpdatedLKP )
		{
			SquadPasteEnemyInfo();
		}
		else
		{
			SquadCopyEnemyInfo();
		}
	}

	return iUpdatedLKP;
}
