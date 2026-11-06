//=========================================================
// trigger_random (+ Sven Co-op's trigger_random_time, trigger_random_unique) -- fires one of up to 16 targets at
//   random: once per trigger, or repeatedly at random delays ("Timed"); optionally each target once ("Unique"),
//   never the same twice in a row, a limit on the number of fires.
// trigger_timer -- fires its target repeatedly at random delays between a minimum and a maximum, optionally N times.
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license)
// dlls/triggers.cpp CTriggerRandom / CTriggerTimer: Featureful's trigger_random is the Sven Co-op entity (same class
// names, key names and spawnflag bits as SevenKewp's CTriggerRandom) plus its own keys. docs/LOGIC_ENTITIES.md.
//
// SBO changes (marked "SBO" below): "round_reset" (round_hooks.h); a target that triggers a (not timed)
// trigger_random again is ignored (Featureful recursed until the stack overflowed). An empty target slot between
// the targets fires nothing for that slot (as Featureful; Sven Co-op skips it). Featureful's "Scheduled order" /
// "Scheduled delays" flags are kept: their point (the same outcome after loading a save) doesn't apply to
// multiplayer, but they are harmless.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"round_hooks.h"

#define TRIGGER_RANDOM_MAX_COUNT 16

#define SF_TRIGGER_RANDOM_START_ON				1
#define SF_TRIGGER_RANDOM_ONCE					2
#define SF_TRIGGER_RANDOM_REUSABLE				4
#define SF_TRIGGER_RANDOM_TIMED					8
#define SF_TRIGGER_RANDOM_UNIQUE				16
#define SF_TRIGGER_RANDOM_DONT_REPEAT			32
#define SF_TRIGGER_RANDOM_PREDETERMINED			64
#define SF_TRIGGER_RANDOM_PREDETERMINED_TIMED	128

class CTriggerRandom : public CPointEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void RoundReset( void );
	void EXPORT TimedThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t ChooseTarget( void );
	float GetRandomDelay( void );
	int TargetCount( void );
	void DoUnique( void );
	void Setup( void );

	int RandomizeIndex( int low, int high )
	{
		if ( m_firstPreferredTarget > 0 && m_firstPreferredTarget <= TargetCount() )
		{
			const int result = m_firstPreferredTarget - 1;
			m_firstPreferredTarget = 0;
			// SBO: in Unique mode the targets are reordered as they are used: the preferred one must be within the
			// unused part
			if ( result >= low && result <= high )
				return result;
		}

		if ( HasPredeterminedTargets() )
		{
			const int index = UTIL_SharedRandomLong( m_randomSeed, low, high );
			m_randomSeed = UTIL_SharedRandomLong( m_randomSeed, 0, 1 << 15 );
			return index;
		}

		return RANDOM_LONG( low, high );
	}

	inline BOOL IsActive( void ) { return ( pev->spawnflags & SF_TRIGGER_RANDOM_START_ON ) != 0; }
	inline void SetActive( BOOL active )
	{
		if ( active )
			SetBits( pev->spawnflags, SF_TRIGGER_RANDOM_START_ON );
		else
			ClearBits( pev->spawnflags, SF_TRIGGER_RANDOM_START_ON );
	}

	BOOL HasPredeterminedTargets( void ) { return FBitSet( pev->spawnflags, SF_TRIGGER_RANDOM_PREDETERMINED ); }
	BOOL HasPrederminedDelays( void ) { return FBitSet( pev->spawnflags, SF_TRIGGER_RANDOM_PREDETERMINED_TIMED ); }

	int m_targetCount;
	string_t m_targets[ TRIGGER_RANDOM_MAX_COUNT ];
	int m_uniqueTargetsLeft;
	int m_triggerNumberLimit;
	int m_triggerCounter;
	float m_minDelay;
	float m_maxDelay;
	string_t m_lastTarget;
	string_t m_triggerOnLimit;
	string_t m_triggerOnExhaust;
	int m_firstPreferredTarget;

	unsigned int m_randomSeed;
	unsigned int m_delayRandomSeed;

	// SBO 2.0 "round_reset": the state after the map load
	BOOL m_bRoundReset;
	int m_iSpawnFlags;			// spawnflags after Spawn (Start on is the running state)
	int m_iTargetCountKey;		// target_count as the map set it
	string_t m_spawnTargets[ TRIGGER_RANDOM_MAX_COUNT ];	// target order as the map set it (Unique reorders)
	int m_spawnFirstTarget;

	BOOL m_bFiring;		// SBO: Use fires its target
};

LINK_ENTITY_TO_CLASS( trigger_random, CTriggerRandom );

// Sven Co-op compatibility
LINK_ENTITY_TO_CLASS( trigger_random_time, CTriggerRandom );
LINK_ENTITY_TO_CLASS( trigger_random_unique, CTriggerRandom );

TYPEDESCRIPTION	CTriggerRandom::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerRandom, m_targetCount, FIELD_INTEGER ),
	DEFINE_ARRAY( CTriggerRandom, m_targets, FIELD_STRING, TRIGGER_RANDOM_MAX_COUNT ),
	DEFINE_FIELD( CTriggerRandom, m_uniqueTargetsLeft, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerRandom, m_triggerNumberLimit, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerRandom, m_triggerCounter, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerRandom, m_minDelay, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerRandom, m_maxDelay, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerRandom, m_lastTarget, FIELD_STRING ),
	DEFINE_FIELD( CTriggerRandom, m_triggerOnLimit, FIELD_STRING ),
	DEFINE_FIELD( CTriggerRandom, m_triggerOnExhaust, FIELD_STRING ),
	DEFINE_FIELD( CTriggerRandom, m_firstPreferredTarget, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerRandom, m_randomSeed, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerRandom, m_delayRandomSeed, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerRandom, m_bRoundReset, FIELD_BOOLEAN ),
	DEFINE_FIELD( CTriggerRandom, m_iSpawnFlags, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerRandom, m_iTargetCountKey, FIELD_INTEGER ),
	DEFINE_ARRAY( CTriggerRandom, m_spawnTargets, FIELD_STRING, TRIGGER_RANDOM_MAX_COUNT ),
	DEFINE_FIELD( CTriggerRandom, m_spawnFirstTarget, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTriggerRandom, CPointEntity );

void CTriggerRandom::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "min_delay" ) )
	{
		m_minDelay = atof( pkvd->szValue );
		if ( !( m_minDelay >= 0 ) )	// SBO: also NaN
			m_minDelay = 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "max_delay" ) )
	{
		m_maxDelay = atof( pkvd->szValue );
		if ( !( m_maxDelay >= 0 ) )
			m_maxDelay = 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_number" ) )
	{
		m_triggerNumberLimit = atoi( pkvd->szValue );
		if ( m_triggerNumberLimit < 0 )
			m_triggerNumberLimit = 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "target_count" ) )	// Sven Co-op compatibility
	{
		m_targetCount = atoi( pkvd->szValue );
		if ( m_targetCount < 0 )
			m_targetCount = 0;
		else if ( m_targetCount > TRIGGER_RANDOM_MAX_COUNT )
			m_targetCount = TRIGGER_RANDOM_MAX_COUNT;
		pkvd->fHandled = TRUE;
	}
	else if ( strncmp( pkvd->szKeyName, "target", 6 ) == 0 && isdigit( (unsigned char)pkvd->szKeyName[6] ) )
	{
		const int num = atoi( pkvd->szKeyName + 6 );
		if ( num <= 0 || num > TRIGGER_RANDOM_MAX_COUNT )
			return;

		m_targets[ num - 1 ] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_on_limit" ) )
	{
		m_triggerOnLimit = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_on_exhaust" ) )
	{
		m_triggerOnExhaust = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "first_target" ) )
	{
		m_firstPreferredTarget = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "round_reset" ) )	// SBO 2.0 (round_hooks.h)
	{
		m_bRoundReset = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTriggerRandom::Spawn( void )
{
	CPointEntity::Spawn();	// SBO: a point entity (Featureful's has no model or solid either)

	// Sven Co-op compatibility
	if ( FClassnameIs( pev, "trigger_random_time" ) )
	{
		SetBits( pev->spawnflags, SF_TRIGGER_RANDOM_TIMED );
	}
	if ( FClassnameIs( pev, "trigger_random_unique" ) )
	{
		SetBits( pev->spawnflags, SF_TRIGGER_RANDOM_UNIQUE );
		if ( FBitSet( pev->spawnflags, 1 ) )
		{
			SetBits( pev->spawnflags, SF_TRIGGER_RANDOM_REUSABLE );
			ClearBits( pev->spawnflags, 1 );
		}
	}

	// SBO 2.0: what RoundReset goes back to
	m_iSpawnFlags = pev->spawnflags;
	m_iTargetCountKey = m_targetCount;
	memcpy( m_spawnTargets, m_targets, sizeof( m_targets ) );
	m_spawnFirstTarget = m_firstPreferredTarget;

	Setup();
}

// Featureful's Spawn after the class name flags: counters, unique pool, seeds, the timed start
void CTriggerRandom::Setup( void )
{
	m_triggerCounter = 0;
	m_lastTarget = iStringNull;
	TargetCount();	// call in case it needs to be evaluated
	if ( FBitSet( pev->spawnflags, SF_TRIGGER_RANDOM_UNIQUE ) )
		m_uniqueTargetsLeft = TargetCount();

	SetThink( NULL );
	pev->nextthink = 0;

	if ( FBitSet( pev->spawnflags, SF_TRIGGER_RANDOM_TIMED ) )
	{
		if ( IsActive() )
		{
			SetThink( &CTriggerRandom::TimedThink );
			pev->nextthink = gpGlobals->time + GetRandomDelay() + 0.1;
		}
	}

	if ( HasPredeterminedTargets() )
		m_randomSeed = RANDOM_LONG( 0, ( 1 << 15 ) );
	if ( HasPrederminedDelays() )
		m_delayRandomSeed = RANDOM_LONG( 0, ( 1 << 15 ) );
}

// SBO 2.0: "round_reset" 1: back to the state after the map load (targets in their order, a full Unique pool, the
// fire count at 0, running only if it started on)
void CTriggerRandom::RoundReset( void )
{
	if ( !m_bRoundReset )
		return;

	pev->spawnflags = m_iSpawnFlags;
	m_targetCount = m_iTargetCountKey;
	memcpy( m_targets, m_spawnTargets, sizeof( m_targets ) );
	m_firstPreferredTarget = m_spawnFirstTarget;
	Setup();
}

void CTriggerRandom::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_TRIGGER_RANDOM_TIMED )
	{
		// Sven Co-op: the use type is ignored, every trigger toggles
		SetActive( !IsActive() );
		if ( IsActive() )
		{
			SetThink( &CTriggerRandom::TimedThink );
			pev->nextthink = gpGlobals->time + GetRandomDelay();
		}
		else
		{
			SetThink( NULL );
			pev->nextthink = 0;
			m_triggerCounter = 0;
		}
	}
	else
	{
		// SBO: a target that triggers this trigger_random again would recurse until the stack overflows
		if ( m_bFiring )
		{
			ALERT( at_console, "'%s' (%s): triggered by its own target, ignored\n", STRING( pev->targetname ),
				STRING( pev->classname ) );
			return;
		}

		string_t chosenTarget = ChooseTarget();
		if ( !FStringNull( chosenTarget ) )
		{
			m_bFiring = TRUE;
			FireTargets( STRING( chosenTarget ), pActivator, this, USE_TOGGLE, 0 );
			m_bFiring = FALSE;
		}
		DoUnique();
	}
}

void CTriggerRandom::TimedThink( void )
{
	if ( IsActive() )
	{
		string_t chosenTarget = ChooseTarget();
		if ( !FStringNull( chosenTarget ) )
			FireTargets( STRING( chosenTarget ), this, this, USE_TOGGLE, 0 );
		DoUnique();

		if ( m_triggerNumberLimit )
		{
			m_triggerCounter++;
			if ( m_triggerCounter >= m_triggerNumberLimit )
			{
				SetActive( FALSE );
				m_triggerCounter = 0;

				if ( !FStringNull( m_triggerOnLimit ) )
					FireTargets( STRING( m_triggerOnLimit ), this, this, USE_TOGGLE, 0 );
			}
		}

		if ( ( pev->spawnflags & SF_TRIGGER_RANDOM_UNIQUE ) && m_uniqueTargetsLeft <= 0 )
			SetActive( FALSE );
		if ( pev->spawnflags & SF_TRIGGER_RANDOM_ONCE )
			SetActive( FALSE );

		if ( IsActive() )
			pev->nextthink = gpGlobals->time + GetRandomDelay();
	}
}

string_t CTriggerRandom::ChooseTarget( void )
{
	int chosenTargetIndex = 0;
	string_t chosenTarget = iStringNull;

	if ( pev->spawnflags & SF_TRIGGER_RANDOM_UNIQUE )
	{
		if ( m_uniqueTargetsLeft > 0 )
		{
			chosenTargetIndex = RandomizeIndex( 0, m_uniqueTargetsLeft - 1 );
			chosenTarget = m_targets[ chosenTargetIndex ];

			if ( chosenTarget == m_lastTarget && FBitSet( pev->spawnflags, SF_TRIGGER_RANDOM_DONT_REPEAT ) && m_uniqueTargetsLeft > 1 )
			{
				chosenTargetIndex = ( chosenTargetIndex + 1 ) % m_uniqueTargetsLeft;
				chosenTarget = m_targets[ chosenTargetIndex ];
			}

			m_targets[ chosenTargetIndex ] = m_targets[ m_uniqueTargetsLeft - 1 ];
			m_targets[ m_uniqueTargetsLeft - 1 ] = chosenTarget;

			m_lastTarget = chosenTarget;
			return chosenTarget;
		}
	}
	else
	{
		const int targetCount = TargetCount();
		if ( targetCount )
		{
			chosenTargetIndex = RandomizeIndex( 0, targetCount - 1 );
			chosenTarget = m_targets[ chosenTargetIndex ];
			if ( chosenTarget == m_lastTarget && FBitSet( pev->spawnflags, SF_TRIGGER_RANDOM_DONT_REPEAT ) && targetCount > 1 )
			{
				chosenTargetIndex = ( chosenTargetIndex + 1 ) % targetCount;
				chosenTarget = m_targets[ chosenTargetIndex ];
			}

			m_lastTarget = chosenTarget;
			return chosenTarget;
		}
	}
	return iStringNull;
}

float CTriggerRandom::GetRandomDelay( void )
{
	const float minDelay = m_minDelay;
	const float maxDelay = ( m_maxDelay > m_minDelay ? m_maxDelay : m_minDelay );
	if ( HasPrederminedDelays() )
	{
		const float delay = UTIL_SharedRandomFloat( m_delayRandomSeed, minDelay, maxDelay );
		m_delayRandomSeed = UTIL_SharedRandomLong( m_delayRandomSeed, 0, 1 << 15 );
		return delay;
	}
	return RANDOM_FLOAT( minDelay, maxDelay );
}

int CTriggerRandom::TargetCount( void )
{
	if ( m_targetCount )
		return m_targetCount;

	for ( int i = TRIGGER_RANDOM_MAX_COUNT - 1; i >= 0; --i )
	{
		if ( !FStringNull( m_targets[i] ) )
		{
			m_targetCount = i + 1;
			return m_targetCount;
		}
	}
	return 0;
}

void CTriggerRandom::DoUnique( void )
{
	if ( ( pev->spawnflags & SF_TRIGGER_RANDOM_UNIQUE ) && m_uniqueTargetsLeft > 0 )
	{
		m_uniqueTargetsLeft--;

		if ( !m_uniqueTargetsLeft )
		{
			if ( !FStringNull( m_triggerOnExhaust ) )
				FireTargets( STRING( m_triggerOnExhaust ), this, this, USE_TOGGLE, 0 );
			if ( pev->spawnflags & SF_TRIGGER_RANDOM_REUSABLE )
				m_uniqueTargetsLeft = TargetCount();
		}
	}
}

//=========================================================
// trigger_timer (Featureful)
//=========================================================

#define SF_TRIGGER_TIMER_START_ON						1
#define SF_TRIGGER_TIMER_NO_FIRST_DELAY					32
#define SF_TRIGGER_TIMER_FORWARD_ACTIVATOR				64
#define SF_TRIGGER_TIMER_PREDETERMINED_TIMED			128
#define SF_TRIGGER_TIMER_DONT_RESET_TRIGGER_COUNTER		256

class CTriggerTimer : public CPointEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void RoundReset( void );
	void EXPORT TimerThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float GetRandomDelay( void );
	void SetActive( BOOL active, CBaseEntity *pActivator = NULL );

	BOOL HasPredeterminedDelays( void ) { return FBitSet( pev->spawnflags, SF_TRIGGER_TIMER_PREDETERMINED_TIMED ); }

	int m_triggerNumberLimit;
	int m_triggerCounter;
	float m_minDelay;
	float m_maxDelay;
	BOOL m_active;
	string_t m_triggerOnLimit;
	unsigned int m_delayRandomSeed;
	EHANDLE m_hActivator;

	BOOL m_bRoundReset;		// SBO 2.0: "round_reset"
};

LINK_ENTITY_TO_CLASS( trigger_timer, CTriggerTimer );

TYPEDESCRIPTION	CTriggerTimer::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerTimer, m_triggerNumberLimit, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerTimer, m_triggerCounter, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerTimer, m_minDelay, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerTimer, m_maxDelay, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerTimer, m_active, FIELD_BOOLEAN ),
	DEFINE_FIELD( CTriggerTimer, m_triggerOnLimit, FIELD_STRING ),
	DEFINE_FIELD( CTriggerTimer, m_delayRandomSeed, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerTimer, m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( CTriggerTimer, m_bRoundReset, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CTriggerTimer, CPointEntity );

void CTriggerTimer::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "min_delay" ) )
	{
		m_minDelay = atof( pkvd->szValue );
		if ( !( m_minDelay >= 0 ) )	// SBO: also NaN
			m_minDelay = 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "max_delay" ) )
	{
		m_maxDelay = atof( pkvd->szValue );
		if ( !( m_maxDelay >= 0 ) )
			m_maxDelay = 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_number" ) )
	{
		m_triggerNumberLimit = atoi( pkvd->szValue );
		if ( m_triggerNumberLimit < 0 )
			m_triggerNumberLimit = 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_on_limit" ) )
	{
		m_triggerOnLimit = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "round_reset" ) )	// SBO 2.0 (round_hooks.h)
	{
		m_bRoundReset = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTriggerTimer::Spawn( void )
{
	CPointEntity::Spawn();	// SBO: a point entity

	m_triggerCounter = 0;
	m_active = FALSE;
	m_hActivator = NULL;
	pev->nextthink = 0;
	SetThink( &CTriggerTimer::TimerThink );

	if ( HasPredeterminedDelays() )
		m_delayRandomSeed = RANDOM_LONG( 0, ( 1 << 15 ) );

	if ( pev->spawnflags & SF_TRIGGER_TIMER_START_ON )
	{
		SetActive( TRUE );
		pev->nextthink += 0.1;	// some little delay of spawn
	}
}

// SBO 2.0: "round_reset" 1: back to the state after the map load (count at 0, running only if it started on)
void CTriggerTimer::RoundReset( void )
{
	if ( m_bRoundReset )
		Spawn();
}

void CTriggerTimer::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	switch ( useType )
	{
	case USE_OFF:
		SetActive( FALSE );
		break;
	case USE_ON:
		SetActive( TRUE, pActivator );
		break;
	default:
		SetActive( !m_active, pActivator );
		break;
	}
}

void CTriggerTimer::TimerThink( void )
{
	if ( m_active )
	{
		CBaseEntity *pActivator = this;
		if ( FBitSet( pev->spawnflags, SF_TRIGGER_TIMER_FORWARD_ACTIVATOR ) && m_hActivator != NULL )
			pActivator = m_hActivator;	// SBO: an activator that is gone (a player who left) is the timer

		if ( !FStringNull( pev->target ) )
			FireTargets( STRING( pev->target ), pActivator, this, USE_TOGGLE, 0 );

		if ( m_triggerNumberLimit )
		{
			m_triggerCounter++;
			if ( m_triggerCounter >= m_triggerNumberLimit )
			{
				SetActive( FALSE );
				m_triggerCounter = 0;
				if ( !FStringNull( m_triggerOnLimit ) )
					FireTargets( STRING( m_triggerOnLimit ), pActivator, this, USE_TOGGLE, 0 );
				return;
			}
		}

		// SBO: a target that turned this timer off (and on again) has set the next think already
		if ( m_active && pev->nextthink <= gpGlobals->time )
			pev->nextthink = gpGlobals->time + GetRandomDelay();
	}
}

float CTriggerTimer::GetRandomDelay( void )
{
	const float minDelay = m_minDelay;
	const float maxDelay = ( m_maxDelay > m_minDelay ? m_maxDelay : m_minDelay );
	if ( HasPredeterminedDelays() )
	{
		const float delay = UTIL_SharedRandomFloat( m_delayRandomSeed, minDelay, maxDelay );
		m_delayRandomSeed = UTIL_SharedRandomLong( m_delayRandomSeed, 0, 1 << 15 );
		return delay;
	}
	return RANDOM_FLOAT( minDelay, maxDelay );
}

void CTriggerTimer::SetActive( BOOL active, CBaseEntity *pActivator )
{
	if ( m_active == active )
		return;
	m_active = active;
	if ( m_active )
	{
		m_hActivator = pActivator;
		if ( FBitSet( pev->spawnflags, SF_TRIGGER_TIMER_NO_FIRST_DELAY ) )
			pev->nextthink = gpGlobals->time;
		else
			pev->nextthink = gpGlobals->time + GetRandomDelay();
	}
	else
	{
		m_hActivator = NULL;
		if ( !FBitSet( pev->spawnflags, SF_TRIGGER_TIMER_DONT_RESET_TRIGGER_COUNTER ) )
			m_triggerCounter = 0;
	}
}
