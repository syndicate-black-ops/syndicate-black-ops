//=========================================================
// trigger_condition -- compares a keyvalue of an entity with a value or with a keyvalue of another entity and fires
// a "true" or a "false" target, once when triggered ("Cyclic") or, while on, every check interval.
//
// Sven Co-op's entity as SevenKewp reimplements it: ported from SevenKewp (https://github.com/wootguy/SevenKewp,
// Half-Life 1 SDK license) dlls/triggers/CTriggerCondition.cpp. The same class name, key names, spawnflag bits and
// comparator numbers, so Sven Co-op entity setups carry over. docs/LOGIC_ENTITIES.md.
//
// The values are read with CBaseEntity::GetKeyValue (keyvalue.h), as trigger_changevalue / trigger_copyvalue read
// them: entvars fields, custom keyvalues ($i_, $f_, $v_, $s_) and the keys a class answers itself.
//
// SBO changes (marked "SBO" below): the entity keeps the activator and caller of its last Use (SevenKewp keeps them
// on every entity); a result target that triggers it again is ignored (SevenKewp recursed until the stack
// overflowed); started on, it counts as on (a toggle turns it off; SevenKewp's first toggle turned it on again); a
// check interval below 0.01 s is 0.01 s; fixed buffers instead of std::string; "round_reset" (round_hooks.h).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"baselogic.h"
#include	"round_hooks.h"

#define SF_TCOND_START_OFF				1
#define SF_TCOND_DONT_USE_X				2
#define SF_TCOND_DONT_USE_Y				4
#define SF_TCOND_DONT_USE_Z				8
#define SF_TCOND_DONT_USE_W				16	// SevenKewp: unused
#define SF_TCOND_CYCLIC					32
#define SF_TCOND_KEEP_ACTIVATOR			64
#define SF_TCOND_IGNORE_INITIAL_RESULT	128

#define VEC_EQ_EPSILON 0.03125f

// SBO: the shortest check interval (SevenKewp: 0 = a think every frame, which the engine runs at most once a frame
// anyway)
#define TCOND_MIN_INTERVAL 0.01f

enum tcond_compare_types
{
	COMPARE_EQUAL,
	COMPARE_NEQUAL,
	COMPARE_LESS,
	COMPARE_GREATER,
	COMPARE_LEQUAL,
	COMPARE_GEQUAL,
	COMPARE_BITSET,

	COMPARE_TYPES,
};

enum tcond_constant_mode_check_behaviors
{
	MODE_WAIT_BOTH,			// only fire target when result changes
	MODE_WAIT_AFTER_FALSE,	// always fire true, only fire false when last result was true
	MODE_WAIT_AFTER_TRUE,	// always fire false, only fire true when last result was false
	MODE_WAIT_NEVER,		// always fire true/false

	CONSTANT_MODES
};

class CTriggerCondition : public CBaseLogic
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

	CKeyValue LoadKey( string_t entName, string_t keyName, const char *errorDesc );
	void Evaluate( void );
	void EvaluateKeys( void );
	BOOL Compare( const CKeyValue &monitorKey, const CKeyValue &compareKey );

	int getValueAsInt( const CKeyValue &key );
	float getValueAsFloat( const CKeyValue &key );
	const char *getValueAsString( const CKeyValue &key, char *buf );

	BOOL CompareFloats( const CKeyValue &monitorKey, const CKeyValue &compareKey );
	BOOL CompareInts( const CKeyValue &monitorKey, const CKeyValue &compareKey );
	BOOL CompareVectors( const CKeyValue &monitorKey, const CKeyValue &compareKey );
	BOOL CompareStrings( const CKeyValue &monitorKey, const CKeyValue &compareKey );

	int GetVectorDontUseFlags()
	{
		// "don't use flags" are 1 bit higher than the generic flags
		return ( pev->spawnflags & ( SF_TCOND_DONT_USE_X | SF_TCOND_DONT_USE_Y | SF_TCOND_DONT_USE_Z ) ) >> 1;
	}

	float CheckInterval( void ) { return m_checkInterval < TCOND_MIN_INTERVAL ? TCOND_MIN_INTERVAL : m_checkInterval; }

	string_t m_monitoredKey;
	string_t m_compareEntity;
	string_t m_compareKey;
	string_t m_compareValue;	// static value to use if no compare entity is set
	int m_checkType;
	int m_iCheckBehavior;		// for constant mode
	float m_checkInterval;

	BOOL isActive;
	BOOL m_checkedFirstResult;
	int m_lastResult;

	BOOL m_bRoundReset;		// SBO 2.0: "round_reset"
	BOOL m_bEvaluating;		// SBO: Evaluate runs (a result target that fires this entity again isn't followed)
};

LINK_ENTITY_TO_CLASS( trigger_condition, CTriggerCondition );

TYPEDESCRIPTION	CTriggerCondition::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerCondition, m_monitoredKey, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCondition, m_compareEntity, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCondition, m_compareKey, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCondition, m_compareValue, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCondition, m_checkType, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerCondition, m_iCheckBehavior, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerCondition, m_checkInterval, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCondition, isActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CTriggerCondition, m_checkedFirstResult, FIELD_BOOLEAN ),
	DEFINE_FIELD( CTriggerCondition, m_lastResult, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerCondition, m_bRoundReset, FIELD_BOOLEAN ),
	DEFINE_FIELD( CTriggerCondition, m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( CTriggerCondition, m_hCaller, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CTriggerCondition, CPointEntity );

void CTriggerCondition::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszValueName" ) )
	{
		m_monitoredKey = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszSourceName" ) )
	{
		m_compareEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszSourceKey" ) )
	{
		m_compareKey = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszCheckValue" ) )
	{
		m_compareValue = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iCheckType" ) )
	{
		m_checkType = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iCheckBehaviour" ) )
	{
		m_iCheckBehavior = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_fCheckInterval" ) )
	{
		m_checkInterval = atof( pkvd->szValue );
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

void CTriggerCondition::Spawn( void )
{
	CPointEntity::Spawn();

	isActive = FALSE;
	m_checkedFirstResult = FALSE;
	m_lastResult = -1;
	SetThink( NULL );
	pev->nextthink = 0;

	if ( !( pev->spawnflags & ( SF_TCOND_START_OFF | SF_TCOND_CYCLIC ) ) )
	{
		isActive = TRUE;	// SBO: on from the start (SevenKewp left it FALSE, so the first toggle turned it on)
		SetThink( &CTriggerCondition::TimedThink );
		pev->nextthink = gpGlobals->time;
	}
}

// SBO 2.0: "round_reset" 1: back to the state after the map load (on or off, no last result, no activator)
void CTriggerCondition::RoundReset( void )
{
	if ( !m_bRoundReset )
		return;

	m_hActivator = NULL;
	m_hCaller = NULL;
	Spawn();
}

void CTriggerCondition::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SevenKewp: FireTarget sets these on every entity it uses
	m_hActivator = pActivator;
	m_hCaller = pCaller;

	isActive = useType == USE_TOGGLE ? !isActive : useType == USE_ON;

	m_checkedFirstResult = FALSE;
	m_lastResult = -1;

	if ( pev->spawnflags & SF_TCOND_CYCLIC )
	{
		Evaluate();
	}
	else
	{
		if ( isActive )
		{
			Evaluate();
			if ( pev->flags & FL_KILLME )
				return;	// removed (no monitored entity or key)
			SetThink( &CTriggerCondition::TimedThink );
			pev->nextthink = gpGlobals->time + CheckInterval();
		}
		else
		{
			SetThink( NULL );
			pev->nextthink = 0;
		}
	}
}

void CTriggerCondition::TimedThink( void )
{
	Evaluate();

	if ( !( pev->flags & FL_KILLME ) )
		pev->nextthink = gpGlobals->time + CheckInterval();
}

CKeyValue CTriggerCondition::LoadKey( string_t entName, string_t keyName, const char *errorDesc )
{
	CKeyValue key = g_emptyKeyValue;

	CBaseEntity *pent = FindLogicEntity( NULL, entName );

	if ( !pent )
	{
		ALERT( at_console, "'%s' (%s): %s ent %s not found\n", STRING( pev->targetname ), STRING( pev->classname ),
			errorDesc, STRING( entName ) );
		return key;
	}

	key = pent->GetKeyValue( STRING( keyName ) );
	if ( !key.keyType )
	{
		ALERT( at_console, "'%s' (%s): %s ent key %s is invalid\n", STRING( pev->targetname ), STRING( pev->classname ),
			errorDesc, STRING( keyName ) );
	}

	return key;
}

void CTriggerCondition::Evaluate( void )
{
	// SBO: a result target that triggers this trigger_condition again would recurse until the stack overflows
	if ( m_bEvaluating )
	{
		ALERT( at_console, "'%s' (%s): triggered by its own result target, ignored\n", STRING( pev->targetname ),
			STRING( pev->classname ) );
		return;
	}

	m_bEvaluating = TRUE;
	EvaluateKeys();
	m_bEvaluating = FALSE;
}

void CTriggerCondition::EvaluateKeys( void )
{
	if ( FStringNull( pev->target ) || FStringNull( m_monitoredKey ) )
	{
		ALERT( at_console, "'%s' (%s): missing monitored entity/key. Removed entity.\n", STRING( pev->targetname ),
			STRING( pev->classname ) );
		SetThink( NULL );
		pev->nextthink = 0;
		UTIL_Remove( this );
		return;
	}
	if ( m_checkType < 0 || m_checkType >= COMPARE_TYPES )
	{
		ALERT( at_console, "'%s' (%s): invalid check type %d\n", STRING( pev->targetname ), STRING( pev->classname ),
			m_checkType );
		return;
	}
	if ( m_iCheckBehavior < 0 || m_iCheckBehavior >= CONSTANT_MODES )
	{
		ALERT( at_console, "'%s' (%s): invalid constant mode behavior %d\n", STRING( pev->targetname ),
			STRING( pev->classname ), m_iCheckBehavior );
		return;
	}

	CKeyValue monitorKey = LoadKey( pev->target, m_monitoredKey, "monitored" );
	if ( !monitorKey.keyType )
		return;

	CKeyValue compareKey = g_emptyKeyValue;

	if ( !FStringNull( m_compareEntity ) && !FStringNull( m_compareKey ) )
	{
		compareKey = LoadKey( m_compareEntity, m_compareKey, "compare" );
		if ( !compareKey.keyType )
			return;
	}
	else
	{
		// static value
		compareKey.keyName = "m_iszCheckValue";
		compareKey.keyOffset = 0;

		if ( !FStringNull( m_compareValue ) )
		{
			compareKey.keyType = KEY_TYPE_STRING;
			compareKey.sVal = m_compareValue;
		}
		else
		{
			compareKey.keyType = KEY_TYPE_INT;
			compareKey.iVal = 0;
		}
	}

	BOOL result = Compare( monitorKey, compareKey ) ? TRUE : FALSE;
	BOOL isCyclic = ( pev->spawnflags & SF_TCOND_CYCLIC ) != 0;
	BOOL ignoreFirstResult = ( pev->spawnflags & SF_TCOND_IGNORE_INITIAL_RESULT ) != 0;
	BOOL shouldFireResultTarget = !ignoreFirstResult || m_checkedFirstResult || isCyclic;

	if ( !isCyclic && shouldFireResultTarget )
	{
		// SevenKewp compares (bool)m_lastResult: the first check (-1) counts as a "true" last result
		BOOL lastResult = m_lastResult != 0;

		switch ( m_iCheckBehavior )
		{
		case MODE_WAIT_BOTH:
			shouldFireResultTarget = lastResult != result;
			break;
		case MODE_WAIT_AFTER_FALSE:
			shouldFireResultTarget = lastResult != result || result;
			break;
		case MODE_WAIT_AFTER_TRUE:
			shouldFireResultTarget = lastResult != result || !result;
			break;
		case MODE_WAIT_NEVER:
		default:
			break;
		}
	}

	m_checkedFirstResult = TRUE;
	m_lastResult = result;

	if ( shouldFireResultTarget )
	{
		EHANDLE h_oldActivator = m_hActivator;

		if ( !( pev->spawnflags & SF_TCOND_KEEP_ACTIVATOR ) )
			m_hActivator = this;

		if ( result && !FStringNull( pev->netname ) )
			FireLogicTargets( STRING( pev->netname ), USE_TOGGLE, 0.0f );
		else if ( !result && !FStringNull( pev->message ) )
			FireLogicTargets( STRING( pev->message ), USE_TOGGLE, 0.0f );

		m_hActivator = h_oldActivator;
	}
}

BOOL CTriggerCondition::Compare( const CKeyValue &monitorKey, const CKeyValue &compareKey )
{
	switch ( monitorKey.keyType )
	{
	case KEY_TYPE_FLOAT:
		return CompareFloats( monitorKey, compareKey );
	case KEY_TYPE_INT:
		return CompareInts( monitorKey, compareKey );
	case KEY_TYPE_VECTOR:
		return CompareVectors( monitorKey, compareKey );
	case KEY_TYPE_STRING:
		return CompareStrings( monitorKey, compareKey );
	default:
		ALERT( at_console, "'%s' (%s): cannot compare key %s (invalid type)\n", STRING( pev->targetname ),
			STRING( pev->classname ), monitorKey.keyName );
		return FALSE;
	}
}

BOOL CTriggerCondition::CompareFloats( const CKeyValue &monitorKey, const CKeyValue &compareKey )
{
	float v = getValueAsFloat( compareKey );

	switch ( m_checkType )
	{
	case COMPARE_EQUAL: return monitorKey.fVal == v;
	case COMPARE_NEQUAL: return monitorKey.fVal != v;
	case COMPARE_LESS: return monitorKey.fVal < v;
	case COMPARE_GREATER: return monitorKey.fVal > v;
	case COMPARE_LEQUAL: return monitorKey.fVal <= v;
	case COMPARE_GEQUAL: return monitorKey.fVal >= v;
	case COMPARE_BITSET:
	default:
		ALERT( at_console, "'%s' (%s): invalid compare type %d used on float key %s\n", STRING( pev->targetname ),
			STRING( pev->classname ), m_checkType, monitorKey.keyName );
		return FALSE;
	}
}

BOOL CTriggerCondition::CompareInts( const CKeyValue &monitorKey, const CKeyValue &compareKey )
{
	int v = getValueAsInt( compareKey );

	switch ( m_checkType )
	{
	case COMPARE_EQUAL: return monitorKey.iVal == v;
	case COMPARE_NEQUAL: return monitorKey.iVal != v;
	case COMPARE_LESS: return monitorKey.iVal < v;
	case COMPARE_GREATER: return monitorKey.iVal > v;
	case COMPARE_LEQUAL: return monitorKey.iVal <= v;
	case COMPARE_GEQUAL: return monitorKey.iVal >= v;
	case COMPARE_BITSET: return ( monitorKey.iVal & v ) != 0;
	default:
		ALERT( at_console, "'%s' (%s): invalid compare type %d used on int key %s\n", STRING( pev->targetname ),
			STRING( pev->classname ), m_checkType, monitorKey.keyName );
		return FALSE;
	}
}

// SBO: a float as an int without undefined behavior (a map's "1e30", NaN): clamped, NaN is 0
static int TCond_FloatToInt( float f )
{
	if ( !( f == f ) )
		return 0;
	if ( f >= 2147483520.0f )
		return 2147483647;
	if ( f <= -2147483648.0f )
		return (int)-2147483647 - 1;
	return (int)f;
}

// SevenKewp UTIL_StringIsVector: three space-separated parts
static BOOL TCond_StringIsVector( const char *pString )
{
	int j;
	const char *pstr = pString;

	for ( j = 0; j < 3; j++ )
	{
		while ( *pstr && *pstr != ' ' )
			pstr++;
		if ( !*pstr )
			break;
		pstr++;
	}

	return j > 2;
}

BOOL CTriggerCondition::CompareVectors( const CKeyValue &monitorKey, const CKeyValue &compareKey )
{
	Vector monitorVector( monitorKey.vVal[0], monitorKey.vVal[1], monitorKey.vVal[2] );
	Vector compareVector = g_vecZero;
	BOOL compareKeyIsVector = compareKey.keyType == KEY_TYPE_VECTOR;

	if ( compareKeyIsVector )
		compareVector = Vector( compareKey.vVal[0], compareKey.vVal[1], compareKey.vVal[2] );
	else if ( compareKey.keyType == KEY_TYPE_STRING && TCond_StringIsVector( STRING( compareKey.sVal ) ) )
	{
		compareKeyIsVector = TRUE;
		UTIL_StringToVector( compareVector, STRING( compareKey.sVal ) );
	}

	if ( pev->spawnflags & SF_TCOND_DONT_USE_X )
	{
		monitorVector.x = 0;
		compareVector.x = 0;
	}
	if ( pev->spawnflags & SF_TCOND_DONT_USE_Y )
	{
		monitorVector.y = 0;
		compareVector.y = 0;
	}
	if ( pev->spawnflags & SF_TCOND_DONT_USE_Z )
	{
		monitorVector.z = 0;
		compareVector.z = 0;
	}

	// lengths or single components are compared if the compare value is not a vector,
	// or if check type is anything but ==/!=
	float monitorFloat = VectorToFloat( monitorVector );
	float compareFloat = getValueAsFloat( compareKey );

	switch ( m_checkType )
	{
	case COMPARE_EQUAL:
		if ( compareKeyIsVector )
		{
			Vector d = monitorVector - compareVector;
			return fabs( d.x ) <= VEC_EQ_EPSILON && fabs( d.y ) <= VEC_EQ_EPSILON && fabs( d.z ) <= VEC_EQ_EPSILON;
		}
		return monitorFloat == compareFloat;
	case COMPARE_NEQUAL:
		if ( compareKeyIsVector )
		{
			Vector d = monitorVector - compareVector;
			return fabs( d.x ) > VEC_EQ_EPSILON || fabs( d.y ) > VEC_EQ_EPSILON || fabs( d.z ) > VEC_EQ_EPSILON;
		}
		return monitorFloat != compareFloat;
	case COMPARE_LESS: return monitorFloat < compareFloat;
	case COMPARE_GREATER: return monitorFloat > compareFloat;
	case COMPARE_LEQUAL: return monitorFloat <= compareFloat;
	case COMPARE_GEQUAL: return monitorFloat >= compareFloat;
	case COMPARE_BITSET:
	default:
		ALERT( at_console, "'%s' (%s): invalid compare type %d used on vector key %s\n", STRING( pev->targetname ),
			STRING( pev->classname ), m_checkType, monitorKey.keyName );
		return FALSE;
	}
}

BOOL CTriggerCondition::CompareStrings( const CKeyValue &monitorKey, const CKeyValue &compareKey )
{
	char buf[ LOGIC_STRING_LEN ];
	const char *monitorString = monitorKey.sVal ? STRING( monitorKey.sVal ) : "";

	switch ( m_checkType )
	{
	case COMPARE_EQUAL: return !strcmp( monitorString, getValueAsString( compareKey, buf ) );
	case COMPARE_NEQUAL: return strcmp( monitorString, getValueAsString( compareKey, buf ) ) != 0;
	case COMPARE_LESS:
	case COMPARE_GREATER:
	case COMPARE_LEQUAL:
	case COMPARE_GEQUAL:
	case COMPARE_BITSET:
	default:
		ALERT( at_console, "'%s' (%s): invalid compare type %d used on string key %s\n", STRING( pev->targetname ),
			STRING( pev->classname ), m_checkType, monitorKey.keyName );
		return FALSE;
	}
}

float CTriggerCondition::getValueAsFloat( const CKeyValue &key )
{
	switch ( key.keyType )
	{
	case KEY_TYPE_FLOAT:
		return key.fVal;
	case KEY_TYPE_INT:
		return key.iVal;
	case KEY_TYPE_VECTOR:
		return VectorToFloat( Vector( key.vVal[0], key.vVal[1], key.vVal[2] ) );
	case KEY_TYPE_STRING:
	{
		const char *s = key.sVal ? STRING( key.sVal ) : "";

		if ( TCond_StringIsVector( s ) )
		{
			Vector vTemp;
			UTIL_StringToVector( vTemp, s );
			return VectorToFloat( vTemp );
		}
		if ( s[0] == '~' )
			return ~TCond_FloatToInt( atof( s + 1 ) );
		return atof( s );
	}
	default:
		ALERT( at_console, "'%s' (%s): cannot convert key %s to float (invalid type)\n", STRING( pev->targetname ),
			STRING( pev->classname ), key.keyName );
		return 0;
	}
}

int CTriggerCondition::getValueAsInt( const CKeyValue &key )
{
	switch ( key.keyType )
	{
	case KEY_TYPE_FLOAT:
	case KEY_TYPE_INT:
	case KEY_TYPE_VECTOR:
		return TCond_FloatToInt( getValueAsFloat( key ) );
	case KEY_TYPE_STRING:
	{
		const char *s = key.sVal ? STRING( key.sVal ) : "";

		if ( TCond_StringIsVector( s ) )
			return TCond_FloatToInt( getValueAsFloat( key ) );
		if ( s[0] == '~' )
			return ~atoi( s + 1 );
		return atoi( s );	// atoi - the only reason getValueAsInt exists
	}
	default:
		ALERT( at_console, "'%s' (%s): cannot convert key %s to int (invalid type)\n", STRING( pev->targetname ),
			STRING( pev->classname ), key.keyName );
		return 0;
	}
}

// buf: LOGIC_STRING_LEN characters
const char *CTriggerCondition::getValueAsString( const CKeyValue &key, char *buf )
{
	switch ( key.keyType )
	{
	case KEY_TYPE_FLOAT:
		snprintf( buf, LOGIC_STRING_LEN, "%f", key.fVal );
		return buf;
	case KEY_TYPE_INT:
		snprintf( buf, LOGIC_STRING_LEN, "%d", key.iVal );
		return buf;
	case KEY_TYPE_VECTOR:
		VectorToString( Vector( key.vVal[0], key.vVal[1], key.vVal[2] ), FLT2STR_6DP, buf );
		return buf;
	case KEY_TYPE_STRING:
		return key.sVal ? STRING( key.sVal ) : "";
	default:
		ALERT( at_console, "'%s' (%s): cannot convert key %s to string (invalid type)\n", STRING( pev->targetname ),
			STRING( pev->classname ), key.keyName );
		return "";
	}
}
