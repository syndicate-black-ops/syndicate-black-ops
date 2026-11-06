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
// SBO: cyber minotaur (minos.mdl), cut down from SDK hgrunt.cpp (copy of cyborg.cpp); fires bullets
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"animation.h"
#include	"squadmonster.h"
#include	"weapons.h"
#include	"talkmonster.h"
#include	"soundent.h"
#include	"effects.h"
#include	"customentity.h"
#include	"player.h"
#include	"sbo_monster.h"
#include	"round_hooks.h"	// SBO 2.0: Logic_IsPlayerInPlay (BestVisibleEnemy)

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define CYBORG_AE_SHOOT			( 1 )
#define CYBORG_AE_KICK			( 2 )
#define CYBORG_AE_STEP			( 3 )
#define CYBORG_AE_STOMP			( 4 )
#define CYBORG_AE_FALL			( 5 )

class CMinotaure : public CSquadMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed ( void );
	int  Classify ( void );
	int  IRelationship( CBaseEntity *pTarget );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL FCanCheckAttacks ( void );
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	Vector GetGunPosition( void );
	void Shoot ( void );
	void DeathSound( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	void RunAI( void );
	void Look( int iDistance );
	int CheckEnemy( CBaseEntity *pEnemy );
	CBaseEntity *BestVisibleEnemy( void );
	CBaseEntity *ChooseEnemy( BOOL fVisibleOnly );

	int Save( CSave &save );
	int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	// SBO: leftovers of CHGrunt's fields (names unknown); m_iBrassShell (0x330, set in Precache since SBO 2.0; 1.38
	// never set it, so its shells were TE_MODEL index 0: none drawn), m_voicePitch is at 0x334
	float m_flNextGrenadeCheck;
	float m_flNextPainTime;
	BOOL m_fStanding;
	int m_iBrassShell;
	int m_voicePitch;

	// SBO 2.0: the last player BestVisibleEnemy picked in sight, and when; kept as his enemy for
	// MINOS_LAST_PLAYER_KEEP seconds while he sees nobody
	EHANDLE m_hLastPlayer;
	float m_flLastPlayerSeen;
};

LINK_ENTITY_TO_CLASS( monster_cyberminotor, CMinotaure );

TYPEDESCRIPTION	CMinotaure::m_SaveData[] =
{
	DEFINE_FIELD( CMinotaure, m_hLastPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( CMinotaure, m_flLastPlayerSeen, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CMinotaure, CSquadMonster );

// the muzzle (attachment 0, the muzzle flash bone) is ~200-330 units from his origin; one farther than this is not
// trusted. Without a usable one: the 1.38 offset (measured at yaw 90) in his own axes
#define MINOS_GUN_MAX_DIST		400
#define MINOS_GUN_FORWARD		186
#define MINOS_GUN_LEFT			32
#define MINOS_GUN_UP			126
// seconds he stays on the last player he saw while he sees no enemy
#define MINOS_LAST_PLAYER_KEEP	5.0

int CMinotaure::IRelationship( CBaseEntity *pTarget )
{
	if ( pTarget && FClassnameIs( pTarget->pev, "monster_human_grunt" ) )
	{
		return R_NM;
	}

	return CBaseMonster::IRelationship( pTarget );
}

//=========================================================
// FCanCheckAttacks - this is overridden for human grunts
// because they can throw/shoot grenades when they can't see their
// target and the base class doesn't check attacks if the monster
// cannot see its enemy.
//
// !!!BUGBUG - this gets called before a 3-round burst is fired
// which means that a friendly can still be hit with up to 2 rounds. 
// ALSO, grenades will not be tossed if there is a friendly in front,
// this is a bad bug. Friendly machine gun fire avoidance
// will unecessarily prevent the throwing of a grenade as well.
//=========================================================
BOOL CMinotaure :: FCanCheckAttacks ( void )
{
	if ( !HasConditions( bits_COND_ENEMY_TOOFAR ) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


//=========================================================
// CheckMeleeAttack1
//=========================================================
BOOL CMinotaure :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	// halflife-updated (VS#3176): pEnemy was read uninitialized when there is no enemy
	CBaseMonster *pEnemy = NULL;

	if ( m_hEnemy != NULL )
	{
		pEnemy = m_hEnemy->MyMonsterPointer();
	}

	if ( !pEnemy )
	{
		return FALSE;
	}

	if ( flDist <= 200 && flDot >= 0.7 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckRangeAttack1 - overridden for HGrunt, cause 
// FCanCheckAttacks() doesn't disqualify all attacks based
// on whether or not the enemy is occluded because unlike
// the base class, the HGrunt can attack when the enemy is
// occluded (throw grenade over wall, etc). We must 
// disqualify the machine gun attack if the enemy is occluded.
//=========================================================
BOOL CMinotaure :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && flDist <= 8192 && flDot >= 0.5 )
	{
		TraceResult	tr;

		if ( !m_hEnemy->IsPlayer() && flDist <= 200 )
		{
			// kick nonclients, but don't shoot at them.
			return FALSE;
		}

		Vector vecSrc = GetGunPosition();

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, m_hEnemy->BodyTarget(vecSrc), ignore_monsters, ignore_glass, ENT(pev), &tr);

		if ( tr.flFraction == 1.0 )
		{
			return TRUE;
		}
	}

	return FALSE;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CMinotaure :: SetYawSpeed ( void )
{
	pev->yaw_speed = 360;
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CMinotaure :: Classify ( void )
{
	return	CLASS_ALIEN_MILITARY;
}

//=========================================================
// GetGunPosition	return the end of the barrel
// SBO 2.0: the model's muzzle (attachment 0). 1.38 returned
// pev->origin + ( -32, 186, 126 ) in world axes, right only at
// yaw 90: his shells, bullets and CheckRangeAttack1's trace
// started up to ~380 units from his gun (behind him, in walls)
//=========================================================

Vector CMinotaure :: GetGunPosition( )
{
	Vector vecGun, vecAngles;
	GetAttachment( 0, vecGun, vecAngles );

	float flDist = ( vecGun - pev->origin ).Length();
	if ( flDist < 1 || flDist > MINOS_GUN_MAX_DIST )
	{
		// no usable attachment: the 1.38 offset turned by his yaw
		Vector vecForward, vecRight, vecUp;
		UTIL_MakeVectorsPrivate( Vector( 0, pev->angles.y, 0 ), vecForward, vecRight, vecUp );
		vecGun = pev->origin + vecForward * MINOS_GUN_FORWARD - vecRight * MINOS_GUN_LEFT;
		vecGun.z = pev->origin.z + MINOS_GUN_UP;
	}

	return vecGun;
}

//=========================================================
// Shoot
//=========================================================
void CMinotaure :: Shoot ( void )
{
	if (m_hEnemy == NULL)
	{
		return;
	}

	Vector vecShootOrigin = GetGunPosition();
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	UTIL_MakeVectors ( pev->angles );

	Vector	vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT(40,90) + gpGlobals->v_up * RANDOM_FLOAT(75,200) + gpGlobals->v_forward * RANDOM_FLOAT(-40,40);
	EjectBrass ( vecShootOrigin - vecShootDir * 24, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL); 

	if ( juniorpowmonst.value == 0 )
		FireBullets(2, vecShootOrigin, vecShootDir, VECTOR_CONE_20DEGREES, 8096, 18 );
	else if ( juniorpowmonst.value == 1 )
		FireBullets(3, vecShootOrigin, vecShootDir, VECTOR_CONE_15DEGREES, 8096, 18 );
	else if ( juniorpowmonst.value == 2 )
		FireBullets(4, vecShootOrigin, vecShootDir, VECTOR_CONE_15DEGREES, 8096, 18 );
	else
		FireBullets(4, vecShootOrigin, vecShootDir, VECTOR_CONE_10DEGREES, 8096, 18 );

	pev->effects |= EF_MUZZLEFLASH;

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CMinotaure :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	Vector	vecShootDir;
	Vector	vecShootOrigin;

	switch( pEvent->event )
	{
		case CYBORG_AE_SHOOT:
		{
			Shoot();

			EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/gat_fire1.wav", 1, ATTN_NORM );

			CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 384, 0.3 );
		}
		break;

		case CYBORG_AE_STEP:
			EMIT_SOUND( ENT(pev), CHAN_BODY, "monster/minoswalk.wav", 1, ATTN_NORM );
			break;

		case CYBORG_AE_STOMP:
			UTIL_ScreenShake( pev->origin, 4.0, 3.0, 1.0, 750 );
			EMIT_SOUND( ENT(pev), CHAN_BODY, "monster/minosfoot.wav", 1, ATTN_NORM );
			break;

		case CYBORG_AE_FALL:
			UTIL_ScreenShake( pev->origin, 4.0, 3.0, 1.0, 750 );
			EMIT_SOUND( ENT(pev), CHAN_BODY, "monster/minosdie.wav", 1, ATTN_NORM );
			break;

		case CYBORG_AE_KICK:
		{
			EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/cbar_hitbod3.wav", 1, ATTN_NORM );

			CBaseEntity *pHurt = CheckTraceHullAttack( 200, 100, DMG_SLASH );

			if ( pHurt )
			{
				pHurt->pev->punchangle.z = -15;
				pHurt->pev->punchangle.x = -45;
				pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_forward * 100;
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_up * 100;
			}
		}
		break;

		default:
			CSquadMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CMinotaure :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/minos.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 230));

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->effects		= 0;
	if ( juniorpvmonst.value == 0 )
		pev->health		= 10000;
	else if ( juniorpvmonst.value == 1 )
		pev->health		= 27000;
	else if ( juniorpvmonst.value == 2 )
		pev->health		= 50000;
	else
		pev->health		= 75000;
	m_flFieldOfView		= VIEW_FIELD_FULL;
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	m_fEnemyEluded		= FALSE;

	m_HackedGunPos = Vector ( 0, 0, 55 );

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CMinotaure :: Precache()
{
	PRECACHE_MODEL("models/minos.mdl");
	m_iBrassShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell (SBO 2.0, as CHGrunt)

	PRECACHE_SOUND( "weapons/cbar_hitbod3.wav" );
	PRECACHE_SOUND( "monster/minoswalk.wav" );
	PRECACHE_SOUND( "monster/minosdie.wav" );
	PRECACHE_SOUND( "monster/minosfoot.wav" );
	PRECACHE_SOUND( "weapons/sbarrel1.wav" );
	PRECACHE_SOUND( "zombie/claw_miss2.wav" );

	// get voice pitch
	if (RANDOM_LONG(0,1))
		m_voicePitch = 109 + RANDOM_LONG(0,7);
	else
		m_voicePitch = 100;
}

//=========================================================
// DeathSound 
//=========================================================
void CMinotaure :: DeathSound ( void )
{
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "monster/minosdie.wav", 1, ATTN_IDLE );
}

//=========================================================
// TakeDamage
//=========================================================
int CMinotaure :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if ( bitsDamageType & DMG_BLAST )
		flDamage *= 10;

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
				SBO_XP_GAIN( pPlayer, flDamage, 5000 );

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

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

//=========================================================
// RunAI - SBO: CBaseMonster::RunAI without the PVS check
//=========================================================
void CMinotaure :: RunAI ( void )
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

	// SBO 2.0: as CBaseMonster::RunAI. 1.38's copy lost this line, so once he was hit the damage conditions stayed
	// set and interrupted every range attack schedule before the shot: under fire he never shot back
	ClearConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE );
}

//=========================================================
// BestVisibleEnemy - SBO 2.0: CBaseMonster::BestVisibleEnemy
// (higher relationship first, then the nearest, 8192 cap)
// over only the entities of Look's list he can see (FVisible,
// as CheckEnemy tests). Look puts everything within 12000
// units in that list, through walls, and human grunts are his
// nemesis: in 1.38 he walked off after a grunt elsewhere on the
// map and ignored the players in front of him. Seeing nobody,
// he stays on the last player he saw for
// MINOS_LAST_PLAYER_KEEP seconds, then the base choice over the
// whole list stands. Players out of play (spectators, dead or
// waiting players, Game Masters) are never picked.
//=========================================================
CBaseEntity *CMinotaure :: BestVisibleEnemy ( void )
{
	CBaseEntity *pReturn = ChooseEnemy( TRUE );

	if ( pReturn )
	{
		if ( pReturn->IsPlayer() )
		{
			m_hLastPlayer = pReturn;
			m_flLastPlayerSeen = gpGlobals->time;
		}
		else
		{
			m_hLastPlayer = NULL;
		}
		return pReturn;
	}

	// nobody in sight: the last player he saw, for a while
	CBaseEntity *pLast = m_hLastPlayer;
	if ( pLast && Logic_IsPlayerInPlay( pLast ) && gpGlobals->time - m_flLastPlayerSeen <= MINOS_LAST_PLAYER_KEEP )
	{
		return pLast;
	}

	// then the base choice over the whole list
	return ChooseEnemy( FALSE );
}

// CBaseMonster::BestVisibleEnemy's choice over Look's list, players in play only; fVisibleOnly: only those he sees
CBaseEntity *CMinotaure :: ChooseEnemy ( BOOL fVisibleOnly )
{
	CBaseEntity	*pReturn = NULL;
	int			iNearest = 8192;// so first visible entity will become the closest.
	int			iBestRelationship = R_NO;

	for ( CBaseEntity *pNextEnt = m_pLink; pNextEnt != NULL; pNextEnt = pNextEnt->m_pLink )
	{
		if ( !pNextEnt->IsAlive() || ( pNextEnt->IsPlayer() && !Logic_IsPlayerInPlay( pNextEnt ) ) )
			continue;
		if ( fVisibleOnly && !FVisible( pNextEnt ) )
			continue;

		int iRelationship = IRelationship( pNextEnt );
		int iDist = ( pNextEnt->pev->origin - pev->origin ).Length();
		if ( iRelationship > iBestRelationship )
		{
			iBestRelationship = iRelationship;
			iNearest = iDist;
			pReturn = pNextEnt;
		}
		else if ( iRelationship == iBestRelationship && iDist <= iNearest )
		{
			iNearest = iDist;
			pReturn = pNextEnt;
		}
	}

	return pReturn;
}

//=========================================================
// Look - SBO: CBaseMonster::Look with a fixed 12000 unit
// range and no view cone / visibility checks; FL_NOTARGET
// entities are not seen
//=========================================================
void CMinotaure :: Look ( int iDistance )
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
int CMinotaure :: CheckEnemy ( CBaseEntity *pEnemy )
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
