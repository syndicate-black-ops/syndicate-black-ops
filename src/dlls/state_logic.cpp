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

enum
{
	STATE_OFF,
	STATE_TURN_OFF,
	STATE_ON,
	STATE_TURN_ON,
};

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
	int		ObjectCaps( void ) { return CPointEntity::ObjectCaps() | FCAP_MASTER; }

	BOOL	IsLockedByMaster( void ) { return !UTIL_IsMasterTriggered( m_sMaster, NULL ); }
	BOOL	IsTriggered( CBaseEntity *pActivator ) { return m_iState == STATE_ON; }

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
	if ( !ShouldToggle( useType, m_iState == STATE_ON ) || IsLockedByMaster() )
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

#define CALCSTATE_MAX_DEPTH 32	// SBO

static int s_iCalcStateDepth;

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
	if ( s_iCalcStateDepth >= CALCSTATE_MAX_DEPTH )
	{
		if ( s_iCalcStateDepth == CALCSTATE_MAX_DEPTH )
			ALERT( at_console, "calc_state \"%s\": the operands refer to each other in a loop; counted as false\n", STRING( pev->targetname ) );
		return FALSE;
	}

	s_iCalcStateDepth++;
	BOOL result = CalcStateDepth( pActivator, isUse );
	s_iCalcStateDepth--;
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
