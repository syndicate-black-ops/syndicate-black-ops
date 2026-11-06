//=========================================================
// trigger_sensor -- when triggered, finds the entities within a radius of its position (or of a Locus position) that
// pass its filters (line of sight, alive, damageable, class name, relationship, master) and fires its target for
// each of them, or only for the closest or the farthest, with that entity as !activator.
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) commit 30fc2f71
// "Add trigger_sensor" (2026-10-30), dlls/triggers.cpp CTriggerSensor. The same class name, key names and values,
// so Featureful entity setups carry over. docs/LOGIC_ENTITIES.md.
//
// SBO changes (marked "SBO" below):
//   * Players are sensed only while in play (round_hooks.h Logic_IsPlayerInPlay): never spectators, Game Masters,
//     HLTV proxies, or players dead or waiting for a round (SBO's dead and waiting players are observers that may
//     have full health). "Dead only" therefore finds no players.
//   * Entities being removed (FL_KILLME) and edicts without an entity are skipped (Featureful's loop stopped at an
//     edict without an entity).
//   * A target that triggers the sensor again while it fires is ignored (console note): no endless recursion.
//   * "Act as Classify" uses SBO's class table (CBaseMonster::IDefaultRelationship, classes 0-13).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"saverestore.h"
#include	"locus.h"
#include	"round_hooks.h"

class CTriggerSensor : public CPointEntity
{
	enum
	{
		LINEBLOCKED_NO,
		LINEBLOCKED_WALLS,
		LINEBLOCKED_WALLS_GLASS,
		LINEBLOCKED_WALLS_MONSTERS,
		LINEBLOCKED_WALLS_MONSTERS_GLASS,
	};
	enum
	{
		TRACE_TO_ORIGIN,
		TRACE_TO_CENTER,
		TRACE_TO_EYES,
		TRACE_TO_BODY,
	};
	enum
	{
		SENSE_ALL,
		SENSE_CLOSEST_ONLY,
		SENSE_FARTHEST_ONLY
	};
	enum
	{
		ALIVE_NO = -1,
		ALIVE_ANY = 0,
		ALIVE_YES,
	};
	enum
	{
		TAKE_DAMAGE_NO = -1,
		TAKE_DAMAGE_ANY = 0,
		TAKE_DAMAGE_YES,
		TAKE_DAMAGE_AIMABLE
	};
	enum
	{
		RELATIONSHIP_ANY,
		RELATIONSHIP_NEUTRAL,
		RELATIONSHIP_ENEMY,
		RELATIONSHIP_ENEMY_OR_NEUTRAL,
		RELATIONSHIP_ALLY,
		RELATIONSHIP_ALLY_OR_NEUTRAL
	};
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int Classify( void ) { return m_classify; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL Senses( CBaseEntity *pEntity, const Vector &pos, Vector &endPos );

	float m_radius;
	string_t m_position;
	int m_lineBlocked;
	int m_traceTo;
	int m_fireTriggerPolicy;
	int m_aliveFilter;
	int m_takeDamageFilter;
	string_t m_sensedMaster;
	string_t m_sensedClassname;
	int m_classify;
	int m_relationship;

	BOOL m_bFiring;		// SBO: Use is firing targets (not saved: it never spans a frame)
};

LINK_ENTITY_TO_CLASS( trigger_sensor, CTriggerSensor );

TYPEDESCRIPTION CTriggerSensor::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerSensor, m_radius, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerSensor, m_position, FIELD_STRING ),
	DEFINE_FIELD( CTriggerSensor, m_lineBlocked, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerSensor, m_traceTo, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerSensor, m_fireTriggerPolicy, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerSensor, m_aliveFilter, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerSensor, m_takeDamageFilter, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerSensor, m_sensedMaster, FIELD_STRING ),
	DEFINE_FIELD( CTriggerSensor, m_sensedClassname, FIELD_STRING ),
	DEFINE_FIELD( CTriggerSensor, m_classify, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerSensor, m_relationship, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTriggerSensor, CPointEntity );

void CTriggerSensor::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "radius" ) )
	{
		m_radius = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "position" ) )
	{
		m_position = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "line_blocked" ) )
	{
		m_lineBlocked = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trace_to" ) )
	{
		m_traceTo = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fire_trigger_policy" ) )
	{
		m_fireTriggerPolicy = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "alive_filter" ) )
	{
		m_aliveFilter = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "takedamage_filter" ) )
	{
		m_takeDamageFilter = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sensed_master" ) )
	{
		m_sensedMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sensed_classname" ) )
	{
		m_sensedClassname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "classify" ) )
	{
		m_classify = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "relationship_filter" ) )
	{
		m_relationship = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

// Featureful CBaseMonster::IsFullyAlive: alive and not dying (a monster's IsAlive is true while it dies)
static BOOL Sensor_IsFullyAlive( CBaseEntity *pEntity )
{
	if ( pEntity->pev->deadflag != DEAD_NO || pEntity->pev->health <= 0 )
		return FALSE;

	CBaseMonster *pMonster = pEntity->MyMonsterPointer();
	if ( pMonster && pMonster->HasMemory( bits_MEMORY_KILLED ) )
		return FALSE;

	return TRUE;
}

// The filters of Featureful's loop body; endPos: the point traced to (and measured to for closest / farthest)
BOOL CTriggerSensor::Senses( CBaseEntity *pEntity, const Vector &pos, Vector &endPos )
{
	if ( pEntity == this || ( pEntity->pev->flags & FL_KILLME ) )	// SBO: FL_KILLME
		return FALSE;

	// SBO: a player is sensed only while in play (never a spectator, Game Master, dead or waiting player)
	if ( pEntity->IsPlayer() && !Logic_IsPlayerInPlay( pEntity ) )
		return FALSE;

	switch ( m_takeDamageFilter )
	{
	case TAKE_DAMAGE_NO:
		if ( pEntity->pev->takedamage != DAMAGE_NO )
			return FALSE;
		break;
	case TAKE_DAMAGE_YES:
		if ( !( pEntity->pev->takedamage > DAMAGE_NO ) )
			return FALSE;
		break;
	case TAKE_DAMAGE_AIMABLE:
		if ( pEntity->pev->takedamage != DAMAGE_AIM )
			return FALSE;
		break;
	default:
		break;
	}

	switch ( m_aliveFilter )
	{
	case ALIVE_NO:
		if ( pEntity->pev->deadflag == DEAD_NO )
			return FALSE;
		break;
	case ALIVE_YES:
		if ( !Sensor_IsFullyAlive( pEntity ) )
			return FALSE;
		break;
	}

	if ( !FStringNull( m_sensedClassname ) )
	{
		if ( FStringNull( pEntity->pev->classname ) || !FStrEq( STRING( m_sensedClassname ), STRING( pEntity->pev->classname ) ) )
			return FALSE;
	}

	if ( m_relationship != RELATIONSHIP_ANY )
	{
		const int entityClassify = pEntity->Classify();
		const int relToEntity = CBaseMonster::IDefaultRelationship( m_classify, entityClassify );
		const int relToSensor = CBaseMonster::IDefaultRelationship( entityClassify, m_classify );

		BOOL passedRelationshipFilter = TRUE;

		switch ( m_relationship )
		{
		case RELATIONSHIP_NEUTRAL:
			passedRelationshipFilter = relToEntity == R_NO && relToSensor == R_NO;
			break;
		case RELATIONSHIP_ENEMY:
			passedRelationshipFilter = relToEntity >= R_DL || relToSensor >= R_DL;
			break;
		case RELATIONSHIP_ENEMY_OR_NEUTRAL:
			passedRelationshipFilter = relToEntity >= R_NO || relToSensor >= R_NO;
			break;
		case RELATIONSHIP_ALLY:
			passedRelationshipFilter = ( relToEntity == R_AL && relToSensor == R_AL ) ||
						( relToEntity == R_AL && relToSensor == R_NO ) ||
						( relToEntity == R_NO && relToSensor == R_AL );
			break;
		case RELATIONSHIP_ALLY_OR_NEUTRAL:
			passedRelationshipFilter = ( relToEntity == R_NO && relToSensor == R_NO ) ||
						( relToEntity == R_AL && relToSensor == R_AL ) ||
						( relToEntity == R_AL && relToSensor == R_NO ) ||
						( relToEntity == R_NO && relToSensor == R_AL );
			break;
		default:
			break;
		}

		if ( !passedRelationshipFilter )
			return FALSE;
	}

	if ( !FStringNull( m_sensedMaster ) )
	{
		if ( !UTIL_IsMasterTriggered( m_sensedMaster, pEntity ) )
			return FALSE;
	}

	IGNORE_MONSTERS ignoreMonsters = dont_ignore_monsters;
	IGNORE_GLASS ignoreGlass = dont_ignore_glass;
	BOOL doTrace = FALSE;

	switch ( m_lineBlocked )
	{
	case LINEBLOCKED_WALLS:
		ignoreMonsters = ignore_monsters;
		ignoreGlass = ignore_glass;
		doTrace = TRUE;
		break;
	case LINEBLOCKED_WALLS_GLASS:
		ignoreMonsters = ignore_monsters;
		ignoreGlass = dont_ignore_glass;
		doTrace = TRUE;
		break;
	case LINEBLOCKED_WALLS_MONSTERS:
		ignoreMonsters = dont_ignore_monsters;
		ignoreGlass = ignore_glass;
		doTrace = TRUE;
		break;
	case LINEBLOCKED_WALLS_MONSTERS_GLASS:
		ignoreMonsters = dont_ignore_monsters;
		ignoreGlass = dont_ignore_glass;
		doTrace = TRUE;
		break;
	default:
		break;
	}

	endPos = pEntity->pev->origin;
	switch ( m_traceTo )
	{
	case TRACE_TO_CENTER:
		endPos = pEntity->Center();
		break;
	case TRACE_TO_EYES:
		endPos = pEntity->EyePosition();
		break;
	case TRACE_TO_BODY:
		endPos = pEntity->BodyTarget( pos );
		break;
	}

	if ( doTrace )
	{
		TraceResult tr;
		UTIL_TraceLine( pos, endPos, ignoreMonsters, ignoreGlass, pEntity->edict(), &tr );
		if ( tr.flFraction != 1.0f )
			return FALSE;
	}

	return TRUE;
}

void CTriggerSensor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SBO: a target that triggers the sensor while it fires: ignored (it would sense again inside itself and recurse
	// until the stack overflows)
	if ( m_bFiring )
	{
		ALERT( at_console, "%s \"%s\": triggered by its own target, ignored\n", STRING( pev->classname ), STRING( pev->targetname ) );
		return;
	}

	Vector pos = pev->origin;
	if ( !FStringNull( m_position ) )
	{
		if ( !TryCalcLocus_Position( this, pActivator, STRING( m_position ), pos ) )
			return;
	}

	CBaseEntity *pBestEntity = NULL;	// nothing fires while closest / farthest searches
	float bestDistance = 0.0f;
	if ( m_fireTriggerPolicy == SENSE_CLOSEST_ONLY )
		bestDistance = 8192.0f;

	m_bFiring = TRUE;

	// SBO: the engine's sphere search directly, so an edict without an entity is skipped instead of ending the search
	edict_t *pentEntity = NULL;
	while ( !FNullEnt( pentEntity = FIND_ENTITY_IN_SPHERE( pentEntity, pos, m_radius ) ) )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( pentEntity );
		if ( !pEntity )
			continue;

		Vector endPos;
		if ( !Senses( pEntity, pos, endPos ) )
			continue;

		switch ( m_fireTriggerPolicy )
		{
		case SENSE_CLOSEST_ONLY:
		{
			const float distance = ( endPos - pos ).Length();
			if ( distance < bestDistance )
			{
				bestDistance = distance;
				pBestEntity = pEntity;
			}
		}
			break;
		case SENSE_FARTHEST_ONLY:
		{
			const float distance = ( endPos - pos ).Length();
			if ( distance >= bestDistance )
			{
				bestDistance = distance;
				pBestEntity = pEntity;
			}
		}
			break;
		default:
			if ( !FStringNull( pev->target ) )
				FireTargets( STRING( pev->target ), pEntity, this, USE_TOGGLE, 0 );
			break;
		}
	}

	if ( pBestEntity && !FStringNull( pev->target ) )
		FireTargets( STRING( pev->target ), pBestEntity, this, USE_TOGGLE, 0 );

	m_bFiring = FALSE;
}
