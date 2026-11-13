//=========================================================
// env_state  -- an on / off state, optionally with a delay to turn on and off, usable as a master
// calc_state -- AND / OR / NAND / NOR / XOR / NXOR of two masters; a master itself
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license)
// dlls/buttons.cpp CEnvState (from Spirit of Half-Life, Laurie Cheers) and CCalcState. docs/STATE_LOGIC.md.
//
// SBO changes (marked "SBO" below): calc_state entities that refer to each other in a loop stop at a depth of 32
// (Featureful recursed until the stack overflowed); env_state takes "round_reset" (round_hooks.h); targets fired
// from env_state's own think get the state as activator, as in Featureful.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"locus.h"

#define SF_ENVSTATE_START_ON		1
#define SF_ENVSTATE_DEBUG			2

// SBO 2.1: the names of the entity states (cbase.h STATE), for console notes and the test layer
const char *GetStringForState( STATE state )
{
	switch ( state )
	{
	case STATE_ON:			return "On";
	case STATE_OFF:			return "Off";
	case STATE_TURN_ON:		return "Turn On";
	case STATE_TURN_OFF:	return "Turn Off";
	case STATE_IN_USE:		return "In Use";
	default:				return "Unknown";
	}
}

// SBO: calc_state, multi_watcher and watcher_count read each other's results; entities that use each other in a loop
// would recurse until the stack overflows, so the reading stops 32 levels deep (one count for all three)
#define STATELOGIC_MAX_DEPTH 32

static int s_iStateLogicDepth;

static BOOL StateLogic_Enter( CBaseEntity *pEntity )
{
	if ( s_iStateLogicDepth >= STATELOGIC_MAX_DEPTH )
	{
		if ( s_iStateLogicDepth == STATELOGIC_MAX_DEPTH )
			ALERT( at_console, "%s \"%s\": the entities it reads refer to each other in a loop; counted as false\n",
				STRING( pEntity->pev->classname ), STRING( pEntity->pev->targetname ) );
		return FALSE;
	}
	s_iStateLogicDepth++;
	return TRUE;
}

static void StateLogic_Leave( void )
{
	s_iStateLogicDepth--;
}

static const char *UseTypeToString( USE_TYPE useType )
{
	switch ( useType )
	{
	case USE_OFF:		return "USE_OFF";
	case USE_ON:		return "USE_ON";
	case USE_SET:		return "USE_SET";
	case USE_TOGGLE:	return "USE_TOGGLE";
	default:			return "USE_UNKNOWN";
	}
}

//=========================================================
// env_state
//=========================================================

class CEnvState : public CPointEntity
{
public:
	void	Spawn( void );
	void	Think( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	RoundReset( void );
	// SBO 2.1: "noise1" / "noise2" (fire when on / off) are target names, not sounds: they may change at run time
	BOOL	RuntimeKeyNeedsPrecache( const char *keyName );
	CKeyValue GetKeyValue( const char *keyName );
	void	RuntimeKeyValueChanged( const char *keyName );
	int		ObjectCaps( void ) { return CPointEntity::ObjectCaps() | FCAP_MASTER; }

	BOOL	IsLockedByMaster( void ) { return !UTIL_IsMasterTriggered( m_sMaster, NULL ); }
	BOOL	IsTriggered( CBaseEntity *pActivator ) { return m_iState == STATE_ON; }
	STATE	GetState( CBaseEntity *pActivator ) { return (STATE)m_iState; }	// SBO 2.1: Turn On / Turn Off while it waits

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int			m_iState;
	float		m_fTurnOnTime;
	float		m_fTurnOffTime;
	string_t	m_sMaster;
	string_t	m_fireWhenOn;
	string_t	m_fireWhenOff;
	BOOL		m_bRoundReset;	// SBO 2.0: "round_reset"
};

TYPEDESCRIPTION CEnvState::m_SaveData[] =
{
	DEFINE_FIELD( CEnvState, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvState, m_fTurnOnTime, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvState, m_fTurnOffTime, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvState, m_sMaster, FIELD_STRING ),
	DEFINE_FIELD( CEnvState, m_fireWhenOn, FIELD_STRING ),
	DEFINE_FIELD( CEnvState, m_fireWhenOff, FIELD_STRING ),
	DEFINE_FIELD( CEnvState, m_bRoundReset, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CEnvState, CPointEntity );

LINK_ENTITY_TO_CLASS( env_state, CEnvState );

void CEnvState::Spawn( void )
{
	CPointEntity::Spawn();

	if ( pev->spawnflags & SF_ENVSTATE_START_ON )
		m_iState = STATE_ON;
	else
		m_iState = STATE_OFF;

	// "noise1" / "noise2": fire when on / off (Spirit's keys)
	if ( pev->noise1 )
	{
		m_fireWhenOn = pev->noise1;
		pev->noise1 = iStringNull;
	}
	if ( pev->noise2 )
	{
		m_fireWhenOff = pev->noise2;
		pev->noise2 = iStringNull;
	}
}

// SBO 2.1: Spawn moves "noise1" / "noise2" into m_fireWhenOn / m_fireWhenOff; a value written at run time
// (trigger_changevalue / trigger_copyvalue) is not refused as a sound and moves there too, and GetKeyValue reads them
BOOL CEnvState::RuntimeKeyNeedsPrecache( const char *keyName )
{
	return stricmp( keyName, "noise1" ) && stricmp( keyName, "noise2" ) && CPointEntity::RuntimeKeyNeedsPrecache( keyName );
}

CKeyValue CEnvState::GetKeyValue( const char *keyName )
{
	const BOOL fOn = !stricmp( keyName, "noise1" );
	if ( fOn || !stricmp( keyName, "noise2" ) )
	{
		CKeyValue keyvalue = g_emptyKeyValue;
		keyvalue.keyType = KEY_TYPE_STRING;
		keyvalue.keyName = fOn ? "noise1" : "noise2";
		keyvalue.sVal = fOn ? m_fireWhenOn : m_fireWhenOff;
		return keyvalue;
	}
	return CPointEntity::GetKeyValue( keyName );
}

void CEnvState::RuntimeKeyValueChanged( const char *keyName )
{
	if ( !stricmp( keyName, "noise1" ) )
	{
		m_fireWhenOn = pev->noise1;
		pev->noise1 = iStringNull;
	}
	else if ( !stricmp( keyName, "noise2" ) )
	{
		m_fireWhenOff = pev->noise2;
		pev->noise2 = iStringNull;
	}
}

// SBO 2.0: "round_reset" 1: back to its state after the map load (a pending turn on / off is dropped, nothing fires)
void CEnvState::RoundReset( void )
{
	if ( !m_bRoundReset )
		return;

	m_iState = ( pev->spawnflags & SF_ENVSTATE_START_ON ) ? STATE_ON : STATE_OFF;
	pev->nextthink = -1;
}

void CEnvState::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "turnontime" ) )
	{
		m_fTurnOnTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "turnofftime" ) )
	{
		m_fTurnOffTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "master" ) )
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
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

void CEnvState::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Spirit of Half-Life (CBaseEntity::ShouldToggle on GetState): turning on counts as on and turning off as off, so On
	// while it turns on and Off while it turns off are ignored, and Off while it turns on turns it around
	if ( !ShouldToggle( useType, m_iState == STATE_ON || m_iState == STATE_TURN_ON ) || IsLockedByMaster() )
	{
		if ( pev->spawnflags & SF_ENVSTATE_DEBUG )
		{
			if ( IsLockedByMaster() )
				ALERT( at_console, "DEBUG: env_state \"%s\" ignored trigger %s; locked by master \"%s\".\n", STRING( pev->targetname ), UseTypeToString( useType ), STRING( m_sMaster ) );
			else if ( useType == USE_ON )
				ALERT( at_console, "DEBUG: env_state \"%s\" ignored trigger USE_ON; already on\n", STRING( pev->targetname ) );
			else if ( useType == USE_OFF )
				ALERT( at_console, "DEBUG: env_state \"%s\" ignored trigger USE_OFF; already off\n", STRING( pev->targetname ) );
			else
				ALERT( at_console, "DEBUG: env_state \"%s\" ignored trigger %s.\n", STRING( pev->targetname ), UseTypeToString( useType ) );
		}
		return;
	}

	switch ( m_iState )
	{
	case STATE_ON:
	case STATE_TURN_ON:
		if ( m_fTurnOffTime )
		{
			m_iState = STATE_TURN_OFF;
			if ( pev->spawnflags & SF_ENVSTATE_DEBUG )
				ALERT( at_console, "DEBUG: env_state \"%s\" triggered; will turn off in %f seconds.\n", STRING( pev->targetname ), m_fTurnOffTime );
			pev->nextthink = gpGlobals->time + m_fTurnOffTime;
		}
		else
		{
			m_iState = STATE_OFF;
			if ( pev->spawnflags & SF_ENVSTATE_DEBUG )
				ALERT( at_console, "DEBUG: env_state \"%s\" triggered, turned off: firing \"%s\" and \"%s\".\n", STRING( pev->targetname ), STRING( pev->target ), STRING( m_fireWhenOff ) );
			pev->nextthink = -1;
			if ( pev->target )
				FireTargets( STRING( pev->target ), pActivator, this, USE_OFF, 0 );
			if ( m_fireWhenOff )
				FireTargets( STRING( m_fireWhenOff ), pActivator, this, USE_TOGGLE, 0 );
		}
		break;
	case STATE_OFF:
	case STATE_TURN_OFF:
		if ( m_fTurnOnTime )
		{
			m_iState = STATE_TURN_ON;
			if ( pev->spawnflags & SF_ENVSTATE_DEBUG )
				ALERT( at_console, "DEBUG: env_state \"%s\" triggered; will turn on in %f seconds.\n", STRING( pev->targetname ), m_fTurnOnTime );
			pev->nextthink = gpGlobals->time + m_fTurnOnTime;
		}
		else
		{
			m_iState = STATE_ON;
			if ( pev->spawnflags & SF_ENVSTATE_DEBUG )
				ALERT( at_console, "DEBUG: env_state \"%s\" triggered, turned on: firing \"%s\" and \"%s\".\n", STRING( pev->targetname ), STRING( pev->target ), STRING( m_fireWhenOn ) );
			pev->nextthink = -1;
			if ( pev->target )
				FireTargets( STRING( pev->target ), pActivator, this, USE_ON, 0 );
			if ( m_fireWhenOn )
				FireTargets( STRING( m_fireWhenOn ), pActivator, this, USE_TOGGLE, 0 );
		}
		break;
	default:
		break;
	}
}

void CEnvState::Think( void )
{
	if ( m_iState == STATE_TURN_ON )
	{
		m_iState = STATE_ON;
		if ( pev->spawnflags & SF_ENVSTATE_DEBUG )
			ALERT( at_console, "DEBUG: env_state \"%s\" turned itself on: firing \"%s\" and \"%s\".\n", STRING( pev->targetname ), STRING( pev->target ), STRING( m_fireWhenOn ) );
		if ( pev->target )
			FireTargets( STRING( pev->target ), this, this, USE_ON, 0 );
		if ( m_fireWhenOn )
			FireTargets( STRING( m_fireWhenOn ), this, this, USE_TOGGLE, 0 );
	}
	else if ( m_iState == STATE_TURN_OFF )
	{
		m_iState = STATE_OFF;
		if ( pev->spawnflags & SF_ENVSTATE_DEBUG )
			ALERT( at_console, "DEBUG: env_state \"%s\" turned itself off: firing \"%s\" and \"%s\".\n", STRING( pev->targetname ), STRING( pev->target ), STRING( m_fireWhenOff ) );
		if ( pev->target )
			FireTargets( STRING( pev->target ), this, this, USE_OFF, 0 );
		if ( m_fireWhenOff )
			FireTargets( STRING( m_fireWhenOff ), this, this, USE_TOGGLE, 0 );
	}
}

//=========================================================
// calc_state
//=========================================================


class CCalcState : public CPointEntity
{
public:
	enum
	{
		STATE_LOGIC_AND = 0,
		STATE_LOGIC_OR,
		STATE_LOGIC_NAND,
		STATE_LOGIC_NOR,
		STATE_LOGIC_XOR,
		STATE_LOGIC_NXOR
	};

	static const char *OperationDisplayName( int operationId )
	{
		switch ( operationId )
		{
		case STATE_LOGIC_AND:	return "AND";
		case STATE_LOGIC_OR:	return "OR";
		case STATE_LOGIC_NAND:	return "NAND";
		case STATE_LOGIC_NOR:	return "NOR";
		case STATE_LOGIC_XOR:	return "XOR";
		case STATE_LOGIC_NXOR:	return "NXOR";
		default:				return "Invalid";
		}
	}

	enum
	{
		STATE_FALLBACK_ERROR,
		STATE_FALLBACK_OFF,
		STATE_FALLBACK_ON
	};

	void KeyValue( KeyValueData *pkvd );
	BOOL IsTriggered( CBaseEntity *pActivator ) { return CalcState( pActivator, FALSE ); }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int	ObjectCaps( void ) { return CPointEntity::ObjectCaps() | FCAP_MASTER; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

protected:
	BOOL CalcState( CBaseEntity *pActivator, BOOL isUse );
	BOOL CalcStateDepth( CBaseEntity *pActivator, BOOL isUse );
	BOOL DoOperation( BOOL leftOp, BOOL rightOp, int operationId );
	BOOL Operand( string_t name, int fallback, CBaseEntity *pActivator, BOOL *pResult, const char *side );

	string_t m_left;
	string_t m_right;
	int m_operation;
	int m_leftFallback;
	int m_rightFallback;
	string_t m_fireWhenFalse;
	string_t m_fireWhenTrue;
};

TYPEDESCRIPTION CCalcState::m_SaveData[] =
{
	DEFINE_FIELD( CCalcState, m_left, FIELD_STRING ),
	DEFINE_FIELD( CCalcState, m_right, FIELD_STRING ),
	DEFINE_FIELD( CCalcState, m_operation, FIELD_INTEGER ),
	DEFINE_FIELD( CCalcState, m_leftFallback, FIELD_INTEGER ),
	DEFINE_FIELD( CCalcState, m_rightFallback, FIELD_INTEGER ),
	DEFINE_FIELD( CCalcState, m_fireWhenFalse, FIELD_STRING ),
	DEFINE_FIELD( CCalcState, m_fireWhenTrue, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCalcState, CPointEntity );

LINK_ENTITY_TO_CLASS( calc_state, CCalcState );

void CCalcState::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "operation" ) )
	{
		m_operation = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "left_operand" ) )
	{
		m_left = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "right_operand" ) )
	{
		m_right = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "left_fallback" ) )
	{
		m_leftFallback = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "right_fallback" ) )
	{
		m_rightFallback = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fire_when_false" ) )
	{
		m_fireWhenFalse = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fire_when_true" ) )
	{
		m_fireWhenTrue = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CCalcState::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CalcState( pActivator, TRUE );
}

// one operand: the named entity's master state (!activator / *player allowed), or the fallback
BOOL CCalcState::Operand( string_t name, int fallback, CBaseEntity *pActivator, BOOL *pResult, const char *side )
{
	CBaseEntity *pEnt = Locus_FindEntity( NULL, STRING( name ), pActivator );
	if ( pEnt )
	{
		*pResult = pEnt->IsTriggered( pActivator ) ? TRUE : FALSE;
		return TRUE;
	}

	if ( fallback == STATE_FALLBACK_OFF )
		*pResult = FALSE;
	else if ( fallback == STATE_FALLBACK_ON )
		*pResult = TRUE;
	else
	{
		ALERT( at_console, "%s \"%s\": %s operand '%s' doesn't exist!\n", STRING( pev->classname ), STRING( pev->targetname ), side, STRING( name ) );
		return FALSE;
	}
	return TRUE;
}

BOOL CCalcState::CalcState( CBaseEntity *pActivator, BOOL isUse )
{
	// SBO: calc_state entities used as each other's operands would recurse until the stack overflows
	if ( !StateLogic_Enter( this ) )
		return FALSE;
	BOOL result = CalcStateDepth( pActivator, isUse );
	StateLogic_Leave();
	return result;
}

BOOL CCalcState::CalcStateDepth( CBaseEntity *pActivator, BOOL isUse )
{
	if ( FStringNull( m_left ) || FStringNull( m_right ) )
	{
		ALERT( at_console, "%s \"%s\" needs both left and right operands defined\n", STRING( pev->classname ), STRING( pev->targetname ) );
		return FALSE;
	}

	BOOL leftOp, rightOp;

	if ( !Operand( m_left, m_leftFallback, pActivator, &leftOp, "left" ) )
		return FALSE;
	if ( isUse && FBitSet( pev->spawnflags, SF_ENVSTATE_DEBUG ) )
		ALERT( at_console, "%s '%s': left operand ('%s') evaluated to %s\n", STRING( pev->classname ), STRING( pev->targetname ), STRING( m_left ), leftOp ? "true" : "false" );

	if ( !Operand( m_right, m_rightFallback, pActivator, &rightOp, "right" ) )
		return FALSE;
	if ( isUse && FBitSet( pev->spawnflags, SF_ENVSTATE_DEBUG ) )
		ALERT( at_console, "%s '%s': right operand ('%s') evaluated to %s\n", STRING( pev->classname ), STRING( pev->targetname ), STRING( m_right ), rightOp ? "true" : "false" );

	const BOOL result = DoOperation( leftOp, rightOp, m_operation );

	if ( isUse && FBitSet( pev->spawnflags, SF_ENVSTATE_DEBUG ) )
		ALERT( at_console, "%s '%s': operation %s evaluated to %s\n", STRING( pev->classname ), STRING( pev->targetname ), OperationDisplayName( m_operation ), result ? "true" : "false" );

	if ( isUse )
	{
		if ( pev->target )
			FireTargets( STRING( pev->target ), pActivator, this, result ? USE_ON : USE_OFF, 0 );
		if ( m_fireWhenFalse && !result )
			FireTargets( STRING( m_fireWhenFalse ), pActivator, this, USE_TOGGLE, 0 );
		if ( m_fireWhenTrue && result )
			FireTargets( STRING( m_fireWhenTrue ), pActivator, this, USE_TOGGLE, 0 );
	}
	return result;
}

BOOL CCalcState::DoOperation( BOOL leftOp, BOOL rightOp, int operationId )
{
	switch ( operationId )
	{
	case STATE_LOGIC_AND:	return leftOp && rightOp;
	case STATE_LOGIC_OR:	return leftOp || rightOp;
	case STATE_LOGIC_NAND:	return !( leftOp && rightOp );
	case STATE_LOGIC_NOR:	return !( leftOp || rightOp );
	case STATE_LOGIC_XOR:	return ( leftOp != 0 ) != ( rightOp != 0 );
	case STATE_LOGIC_NXOR:	return ( leftOp != 0 ) == ( rightOp != 0 );
	default:
		ALERT( at_console, "%s: unknown operation id %d\n", STRING( pev->classname ), operationId );
		return FALSE;
	}
}

//=========================================================
// multi_watcher -- watches the states of up to MAX_MULTI_TARGETS entities and combines them with AND / OR / NAND /
// NOR / XOR / XNOR; fires its target when the result changes; a master itself
//
// Spirit of Half-Life 1.9 (SpiritSource/dlls/triggers.cpp CStateWatcher). SBO: the Turn Off flag (64) watches Turn Off
// (Spirit tested the Turn On flag for it); the last result is a member (Spirit kept it in spawnflag 512); names are
// found with Locus_FindEntity (!activator, *player); watchers that watch each other in a loop stop at 32 levels.
//=========================================================

#define SF_SWATCHER_SENDTOGGLE		1
#define SF_SWATCHER_DONTSEND_ON		2
#define SF_SWATCHER_DONTSEND_OFF	4
#define SF_SWATCHER_NOTON			8
#define SF_SWATCHER_OFF				16
#define SF_SWATCHER_TURN_ON			32
#define SF_SWATCHER_TURN_OFF		64
#define SF_SWATCHER_IN_USE			128

enum
{
	SWATCHER_LOGIC_AND = 0,
	SWATCHER_LOGIC_OR,
	SWATCHER_LOGIC_NAND,
	SWATCHER_LOGIC_NOR,
	SWATCHER_LOGIC_XOR,
	SWATCHER_LOGIC_XNOR,
};

class CStateWatcher : public CPointEntity
{
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	EXPORT WatchThink( void );
	int		ObjectCaps( void ) { return ( CPointEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ) | FCAP_MASTER; }
	BOOL	IsTriggered( CBaseEntity *pActivator ) { return EvalLogic( pActivator ); }
	STATE	GetState( CBaseEntity *pActivator ) { return EvalLogic( pActivator ) ? STATE_ON : STATE_OFF; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL	EvalLogic( CBaseEntity *pActivator );
	BOOL	EvalLogicDepth( CBaseEntity *pActivator );

	int			m_fLogic;
	int			m_cTargets;
	string_t	m_iTargetName[ MAX_MULTI_TARGETS ];
	BOOL		m_fValid;	// the result of the last check (Spirit: spawnflag 512)
};

LINK_ENTITY_TO_CLASS( multi_watcher, CStateWatcher );

TYPEDESCRIPTION CStateWatcher::m_SaveData[] =
{
	DEFINE_FIELD( CStateWatcher, m_fLogic, FIELD_INTEGER ),
	DEFINE_FIELD( CStateWatcher, m_cTargets, FIELD_INTEGER ),
	DEFINE_ARRAY( CStateWatcher, m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_FIELD( CStateWatcher, m_fValid, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CStateWatcher, CPointEntity );

void CStateWatcher::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_fLogic" ) )
	{
		m_fLogic = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
		return;
	}

	// "m_iszWatch" names one entity; every other key is the name of a watched entity, as multi_manager's keys are
	// the names it fires (a "#n" suffix makes duplicates); the value is not used
	char tmp[128];
	const char *pszName = pkvd->szValue;
	if ( !FStrEq( pkvd->szKeyName, "m_iszWatch" ) )
	{
		UTIL_StripToken( pkvd->szKeyName, tmp, sizeof( tmp ) );
		pszName = tmp;
	}
	pkvd->fHandled = TRUE;
	if ( !pszName[0] )
		return;
	if ( m_cTargets < MAX_MULTI_TARGETS )
		m_iTargetName[ m_cTargets++ ] = ALLOC_STRING( pszName );
	else
		ALERT( at_console, "%s \"%s\": too many entities to watch (more than %d), \"%s\" left out\n", STRING( pev->classname ),
			STRING( pev->targetname ), MAX_MULTI_TARGETS, pszName );
}

void CStateWatcher::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	m_fValid = FALSE;
	// only a watcher that notifies a target checks by itself (Spirit); as a master it checks when asked
	if ( !FStringNull( pev->target ) )
	{
		SetThink( &CStateWatcher::WatchThink );
		pev->nextthink = gpGlobals->time + 0.5;
	}
}

void CStateWatcher::WatchThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	const BOOL fValid = EvalLogic( NULL );
	if ( fValid == m_fValid )
		return;
	m_fValid = fValid;

	if ( FBitSet( pev->spawnflags, fValid ? SF_SWATCHER_DONTSEND_ON : SF_SWATCHER_DONTSEND_OFF ) )
		return;
	USE_TYPE useType = FBitSet( pev->spawnflags, SF_SWATCHER_SENDTOGGLE ) ? USE_TOGGLE : ( fValid ? USE_ON : USE_OFF );
	SUB_UseTargets( this, useType, 0 );
}

BOOL CStateWatcher::EvalLogic( CBaseEntity *pActivator )
{
	if ( !StateLogic_Enter( this ) )
		return FALSE;
	BOOL result = EvalLogicDepth( pActivator );
	StateLogic_Leave();
	return result;
}

BOOL CStateWatcher::EvalLogicDepth( CBaseEntity *pActivator )
{
	BOOL xorgot = FALSE;

	for ( int i = 0; i < m_cTargets; i++ )
	{
		CBaseEntity *pEntity = Locus_FindEntity( NULL, STRING( m_iTargetName[i] ), pActivator );
		if ( !pEntity )
			continue;	// a missing entity is not tested (Spirit)

		BOOL b = FALSE;
		switch ( pEntity->GetState( pActivator ) )
		{
		case STATE_ON:			b = !FBitSet( pev->spawnflags, SF_SWATCHER_NOTON ); break;
		case STATE_OFF:			b = FBitSet( pev->spawnflags, SF_SWATCHER_OFF ) != 0; break;
		case STATE_TURN_ON:		b = FBitSet( pev->spawnflags, SF_SWATCHER_TURN_ON ) != 0; break;
		case STATE_TURN_OFF:	b = FBitSet( pev->spawnflags, SF_SWATCHER_TURN_OFF ) != 0; break;
		case STATE_IN_USE:		b = FBitSet( pev->spawnflags, SF_SWATCHER_IN_USE ) != 0; break;
		}

		if ( b )
		{
			switch ( m_fLogic )
			{
			case SWATCHER_LOGIC_OR:		return TRUE;
			case SWATCHER_LOGIC_NOR:	return FALSE;
			case SWATCHER_LOGIC_XOR:
				if ( xorgot )
					return FALSE;
				xorgot = TRUE;
				break;
			case SWATCHER_LOGIC_XNOR:
				if ( xorgot )
					return TRUE;
				xorgot = TRUE;
				break;
			}
		}
		else
		{
			switch ( m_fLogic )
			{
			case SWATCHER_LOGIC_AND:	return FALSE;
			case SWATCHER_LOGIC_NAND:	return TRUE;
			}
		}
	}

	switch ( m_fLogic )
	{
	case SWATCHER_LOGIC_AND:
	case SWATCHER_LOGIC_NOR:	return TRUE;
	case SWATCHER_LOGIC_XOR:	return xorgot;
	case SWATCHER_LOGIC_XNOR:	return !xorgot;
	default:					return FALSE;	// OR, NAND
	}
}

//=========================================================
// watcher_count -- counts the entities of one name that are not Off, compares the count with a number, and fires
// targets when the count rises or falls and when the comparison turns true or false; a master itself (on while the
// comparison holds)
//
// Spirit of Half-Life (halflife-updated-spirit dlls/triggers.cpp CWatcherCount; Spirit 1.9's version never fired its
// on / off targets). SBO: "Fire at startup" fires the on or the off target by the comparison (Spirit: by count <
// number); "Fire on changing state" gets On / Off (Spirit: Toggle); the state is the comparison (Spirit's GetState
// read a flag watcher_count never set).
//=========================================================

#define SF_WRCOUNT_FIRESTART	1

enum
{
	WRCOUNT_EQUAL = 0,
	WRCOUNT_GREATER,
	WRCOUNT_LESS,
	WRCOUNT_NOT_EQUAL,
	WRCOUNT_LESS_EQUAL,
	WRCOUNT_GREATER_EQUAL,
};

class CWatcherCount : public CPointEntity
{
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	EXPORT CountThink( void );
	int		ObjectCaps( void ) { return ( CPointEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ) | FCAP_MASTER; }
	BOOL	IsTriggered( CBaseEntity *pActivator ) { return Compare( Count() ); }
	STATE	GetState( CBaseEntity *pActivator ) { return Compare( Count() ) ? STATE_ON : STATE_OFF; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int		Count( void );
	BOOL	Compare( int iCount );
	void	Fire( string_t iszTarget, USE_TYPE useType ) { if ( !FStringNull( iszTarget ) ) FireTargets( STRING( iszTarget ), this, this, useType, 0 ); }

	int		m_iMode;
	int		m_iCount;		// the count at the last check
	BOOL	m_fStarted;		// the first check ran
};

LINK_ENTITY_TO_CLASS( watcher_count, CWatcherCount );

TYPEDESCRIPTION CWatcherCount::m_SaveData[] =
{
	DEFINE_FIELD( CWatcherCount, m_iMode, FIELD_INTEGER ),
	DEFINE_FIELD( CWatcherCount, m_iCount, FIELD_INTEGER ),
	DEFINE_FIELD( CWatcherCount, m_fStarted, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CWatcherCount, CPointEntity );

void CWatcherCount::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iMode" ) )
	{
		m_iMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CWatcherCount::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	m_iCount = 0;
	m_fStarted = FALSE;
	SetThink( &CWatcherCount::CountThink );
	pev->nextthink = gpGlobals->time + 0.5;
}

// the entities named "noise" whose state is not Off (a living monster, an open door, a lit light, ...)
int CWatcherCount::Count( void )
{
	if ( FStringNull( pev->noise ) )
		return 0;
	if ( !StateLogic_Enter( this ) )
		return 0;
	int iCount = 0;
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->noise ) ) ) != NULL )
	{
		if ( pEntity->GetState( NULL ) != STATE_OFF )
			iCount++;
	}
	StateLogic_Leave();
	return iCount;
}

BOOL CWatcherCount::Compare( int iCount )
{
	const int n = pev->impulse;
	switch ( m_iMode )
	{
	case WRCOUNT_GREATER:		return iCount > n;
	case WRCOUNT_LESS:			return iCount < n;
	case WRCOUNT_NOT_EQUAL:		return iCount != n;
	case WRCOUNT_LESS_EQUAL:	return iCount <= n;
	case WRCOUNT_GREATER_EQUAL:	return iCount >= n;
	default:					return iCount == n;
	}
}

void CWatcherCount::CountThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	const int iCount = Count();
	const BOOL fOn = Compare( iCount );

	if ( !m_fStarted )
	{
		m_fStarted = TRUE;
		m_iCount = iCount;
		if ( FBitSet( pev->spawnflags, SF_WRCOUNT_FIRESTART ) )
			Fire( fOn ? pev->netname : pev->message, USE_TOGGLE );
		return;
	}

	if ( iCount == m_iCount )
		return;

	const BOOL fWasOn = Compare( m_iCount );
	const BOOL fUp = iCount > m_iCount;
	m_iCount = iCount;

	Fire( fUp ? pev->noise1 : pev->noise2, USE_TOGGLE );
	if ( fOn != fWasOn )
	{
		Fire( fOn ? pev->netname : pev->message, USE_TOGGLE );
		Fire( pev->target, fOn ? USE_ON : USE_OFF );
	}
}
