//=========================================================
// trigger_entity_iterator -- fires its target once for every entity that matches a name and/or class filter (and
// is alive or dead), with that entity as !activator: "for every player", "for every grunt named wave2", ...
//
// Sven Co-op's entity as SevenKewp reimplements it: ported from SevenKewp (https://github.com/wootguy/SevenKewp,
// Half-Life 1 SDK license) dlls/triggers/CTriggerEntityIterator.cpp. The same class name, key names and values, so
// Sven Co-op entity setups carry over. docs/LOGIC_ENTITIES.md.
//
// SBO changes (marked "SBO" below):
//   * Players: only players in play are iterated (round_hooks.h Logic_IsPlayerInPlay): never spectators, Game
//     Masters, HLTV proxies, or players dead or waiting for a round (SBO's dead and waiting players are observers
//     that may have full health). "Only dead entities" therefore finds no players.
//   * Entities being removed (FL_KILLME) are skipped.
//   * Fixed buffers instead of std::vector; the targets are looked up for every iterated entity (SevenKewp looked
//     them up once per run); a target that triggers the iterator again while it runs is ignored (no endless
//     recursion), but turns it off; a toggle stops a repeating run (SevenKewp: only "Off" did); "Trigger at end of
//     each run" gets the iterator as activator when the original activator is gone; "round_reset" (round_hooks.h).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"baselogic.h"
#include	"round_hooks.h"

enum iterator_status_filter_types
{
	FILTER_STATUS_NONE,
	FILTER_STATUS_LIVING,
	FILTER_STATUS_DEAD
};

enum iterator_run_modes
{
	RUN_MODE_ONCE,
	RUN_MODE_ONCE_MULTITHREADED,
	RUN_MODE_TOGGLE,
	RUN_MODE_ONCE_MULTITHREADED_CHILD = 1337,	// this entity is the child of a multithreaded iterator, and should die after running
};

class CTriggerEntityIterator : public CBaseLogic
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void RoundReset( void );
	void EXPORT Iterate( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL Matches( CBaseEntity *pEntity );
	void FireFor( CBaseEntity *pEntity );
	void Stop( void );

	string_t m_name_filter;
	string_t m_classname_filter;
	string_t m_trigger_after_run;
	int m_status_filter;
	int m_triggerstate;
	int m_run_mode;
	int m_maximum_runs;
	float m_delay_between_triggers;
	float m_delay_between_runs;

	int m_nextIdx;
	int m_runCount;
	BOOL m_isRunning;

	BOOL m_bRoundReset;		// SBO 2.0: "round_reset"
	BOOL m_bIterating;		// SBO: Iterate runs
};

LINK_ENTITY_TO_CLASS( trigger_entity_iterator, CTriggerEntityIterator );

TYPEDESCRIPTION	CTriggerEntityIterator::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerEntityIterator, m_name_filter, FIELD_STRING ),
	DEFINE_FIELD( CTriggerEntityIterator, m_classname_filter, FIELD_STRING ),
	DEFINE_FIELD( CTriggerEntityIterator, m_trigger_after_run, FIELD_STRING ),
	DEFINE_FIELD( CTriggerEntityIterator, m_status_filter, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerEntityIterator, m_triggerstate, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerEntityIterator, m_run_mode, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerEntityIterator, m_maximum_runs, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerEntityIterator, m_delay_between_triggers, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerEntityIterator, m_delay_between_runs, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerEntityIterator, m_nextIdx, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerEntityIterator, m_runCount, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerEntityIterator, m_isRunning, FIELD_BOOLEAN ),
	DEFINE_FIELD( CTriggerEntityIterator, m_bRoundReset, FIELD_BOOLEAN ),
	DEFINE_FIELD( CTriggerEntityIterator, m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( CTriggerEntityIterator, m_hCaller, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CTriggerEntityIterator, CPointEntity );

void CTriggerEntityIterator::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "name_filter" ) )
	{
		m_name_filter = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "classname_filter" ) )
	{
		m_classname_filter = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_after_run" ) )
	{
		m_trigger_after_run = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "status_filter" ) )
	{
		m_status_filter = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "triggerstate" ) )
	{
		// Sven Co-op: 0 off, 1 on, 2 toggle
		m_triggerstate = atoi( pkvd->szValue );

		if ( m_triggerstate < 0 || m_triggerstate >= 2 )
			m_triggerstate = USE_TOGGLE;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "run_mode" ) )
	{
		m_run_mode = atoi( pkvd->szValue );
		// SBO: the child mode is only for the copies a multithreaded iterator makes
		if ( m_run_mode != RUN_MODE_ONCE_MULTITHREADED && m_run_mode != RUN_MODE_TOGGLE )
			m_run_mode = RUN_MODE_ONCE;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "maximum_runs" ) )
	{
		m_maximum_runs = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "delay_between_triggers" ) )
	{
		m_delay_between_triggers = atof( pkvd->szValue );
		if ( !( m_delay_between_triggers >= 0 ) )	// SBO: negative or NaN
			m_delay_between_triggers = 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "delay_between_runs" ) )
	{
		m_delay_between_runs = atof( pkvd->szValue );
		if ( !( m_delay_between_runs >= 0 ) )
			m_delay_between_runs = 0;
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

void CTriggerEntityIterator::Spawn( void )
{
	CPointEntity::Spawn();
}

void CTriggerEntityIterator::Stop( void )
{
	m_isRunning = FALSE;
	SetThink( NULL );
	pev->nextthink = 0;
}

// SBO 2.0: "round_reset" 1: a run in progress stops (a multithreaded run's copies are removed)
void CTriggerEntityIterator::RoundReset( void )
{
	if ( !m_bRoundReset )
		return;

	if ( m_run_mode == RUN_MODE_ONCE_MULTITHREADED_CHILD )
	{
		Stop();
		UTIL_Remove( this );
		return;
	}

	Stop();
	m_runCount = 0;
	m_nextIdx = 1;
	m_hActivator = NULL;
	m_hCaller = NULL;
}

BOOL CTriggerEntityIterator::Matches( CBaseEntity *pEntity )
{
	entvars_t *vars = pEntity->pev;

	if ( pEntity == this || ( vars->flags & FL_KILLME ) )
		return FALSE;

	// SBO: a player is iterated only while in play (never a spectator, Game Master, dead or waiting player)
	if ( pEntity->IsPlayer() )
	{
		if ( !Logic_IsPlayerInPlay( pEntity ) )
			return FALSE;
	}

	if ( m_status_filter == FILTER_STATUS_LIVING && vars->deadflag != DEAD_NO )
		return FALSE;
	if ( m_status_filter == FILTER_STATUS_DEAD && vars->deadflag == DEAD_NO )
		return FALSE;
	if ( !FStringNull( m_name_filter ) && strcmp( STRING( m_name_filter ), STRING( vars->targetname ) ) )
		return FALSE;
	if ( !FStringNull( m_classname_filter ) && strcmp( STRING( m_classname_filter ), STRING( vars->classname ) ) )
		return FALSE;

	return TRUE;
}

// SevenKewp: the iterated entity is the activator of the targets; the target names "!activator" / "!caller" are the
// iterator's own activator and caller
void CTriggerEntityIterator::FireFor( CBaseEntity *pEntity )
{
	const char *target = STRING( pev->target );
	USE_TYPE useType = (USE_TYPE)m_triggerstate;

	CBaseEntity *pTarget;
	if ( UTIL_TargetSelector( target, m_hActivator, m_hCaller, &pTarget ) )
	{
		if ( pTarget )
			pTarget->Use( pEntity, this, useType, 0.0f );
	}
	else
		FireTargets( target, pEntity, this, useType, 0.0f );
}

void CTriggerEntityIterator::Iterate( void )
{
	BOOL iterateAll = m_delay_between_triggers == 0;
	BOOL foundAnEnt = FALSE;

	m_bIterating = TRUE;

	int i;
	for ( i = m_nextIdx > 0 ? m_nextIdx : 1; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pEdict = INDEXENT( i );

		if ( !pEdict || pEdict->free )
			continue;

		CBaseEntity *ent = CBaseEntity::Instance( pEdict );
		if ( !ent || !Matches( ent ) )
			continue;

		if ( !iterateAll && foundAnEnt )
		{
			// make sure there is another entity to use next time to prevent an Iterate() call doing nothing
			m_nextIdx = i;
			break;
		}

		FireFor( ent );
		foundAnEnt = TRUE;

		// SBO: a target removed this iterator, or stopped the run
		if ( ( pev->flags & FL_KILLME ) || !m_bIterating )
		{
			m_bIterating = FALSE;
			return;
		}
	}

	m_bIterating = FALSE;

	BOOL finishedRun = i >= gpGlobals->maxEntities;

	if ( finishedRun )
	{
		if ( !FStringNull( m_trigger_after_run ) )
		{
			// SBO: an activator that is gone is the iterator (stock entities use their activator without a check)
			CBaseEntity *pActivator = m_hActivator;
			FireTargets( STRING( m_trigger_after_run ), pActivator ? pActivator : this, this, USE_TOGGLE, 0.0f );
		}

		if ( pev->flags & FL_KILLME )
			return;

		if ( m_run_mode == RUN_MODE_ONCE_MULTITHREADED_CHILD )
		{
			Stop();
			UTIL_Remove( this );
			return;
		}

		if ( m_run_mode == RUN_MODE_TOGGLE && m_isRunning && ( !m_maximum_runs || ++m_runCount < m_maximum_runs ) )
		{
			m_nextIdx = 1;
			SetThink( &CTriggerEntityIterator::Iterate );
			pev->nextthink = gpGlobals->time + m_delay_between_runs;
		}
		else
			Stop();
	}
	else
	{
		m_isRunning = TRUE;
		SetThink( &CTriggerEntityIterator::Iterate );
		pev->nextthink = gpGlobals->time + m_delay_between_triggers;
	}
}

void CTriggerEntityIterator::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SBO: a target that triggers the iterator while it iterates: ignored (it would restart the run inside itself
	// and recurse until the stack overflows). Turning it off still works: the run stops after the current entity.
	if ( m_bIterating )
	{
		if ( useType == USE_OFF || ( useType != USE_ON && m_run_mode != RUN_MODE_ONCE_MULTITHREADED_CHILD ) )
		{
			Stop();
			m_bIterating = FALSE;
		}
		return;
	}

	BOOL shouldToggleOff = ( m_isRunning && useType != USE_ON ) || useType == USE_OFF;
	if ( shouldToggleOff && m_run_mode != RUN_MODE_ONCE_MULTITHREADED_CHILD )
	{
		Stop();
		return;
	}

	if ( FStringNull( pev->target ) )
	{
		ALERT( at_console, "'%s' (%s): no target specified\n", STRING( pev->targetname ), STRING( pev->classname ) );
		return;
	}

	// SevenKewp: FireTarget sets these on every entity it uses
	m_hActivator = pActivator;
	m_hCaller = pCaller;

	if ( m_run_mode == RUN_MODE_ONCE_MULTITHREADED )
	{
		CTriggerEntityIterator *child = (CTriggerEntityIterator *)CBaseEntity::Create(
			"trigger_entity_iterator", pev->origin, pev->angles );

		if ( child )
		{
			child->pev->target = pev->target;
			child->m_name_filter = m_name_filter;
			child->m_classname_filter = m_classname_filter;
			child->m_trigger_after_run = m_trigger_after_run;
			child->m_status_filter = m_status_filter;
			child->m_triggerstate = m_triggerstate;
			child->m_delay_between_triggers = m_delay_between_triggers;
			child->m_run_mode = RUN_MODE_ONCE_MULTITHREADED_CHILD;
			child->m_bRoundReset = m_bRoundReset;

			child->Use( pActivator, pCaller, USE_ON, 0.0f );
		}
	}
	else
	{
		m_isRunning = TRUE;
		m_runCount = 0;
		m_nextIdx = 1;
		Iterate();
	}
}
