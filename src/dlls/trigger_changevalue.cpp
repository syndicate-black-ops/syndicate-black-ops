//=========================================================
// trigger_changevalue -- changes an entity keyvalue
// trigger_copyvalue   -- copies a keyvalue of one entity into a keyvalue of others
//
// Sven Co-op's entities as SevenKewp reimplements them: ported from SevenKewp (https://github.com/wootguy/SevenKewp,
// Half-Life 1 SDK license) dlls/triggers/CTriggerChangeValue.cpp. The same class names, key names, spawnflag bits
// and operation numbers, so Sven Co-op entity setups carry over. docs/TRIGGER_CHANGEVALUE.md.
//
// The new value is computed from the target's current value (CBaseEntity::GetKeyValue, keyvalue.h) and written
// with a key dispatch, as the map loader writes keys: an entvars field, a custom keyvalue or the class's KeyValue().
//
// SBO changes (marked "SBO" below): model and sound keys are refused (no precache at run time); integer division
// and modulo by zero leave the value alone instead of crashing the server; values are cut to
// KEYVALUE_MAX_VALUE_LEN; the trigonometric behaviour defaults to degrees when the key is missing (SevenKewp:
// multipliers of zero); Opposing Force's class names are not among the named constants.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"baselogic.h"
#include	"locus.h"

#define SF_TCVAL_DONT_USE_X 1
#define SF_TCVAL_DONT_USE_Y 2
#define SF_TCVAL_DONT_USE_Z 4
#define SF_TCVAL_CONSTANT 8					// trigger_copyvalue: copy again every "dmg" seconds
#define SF_TCVAL_START_ON 16				// trigger_copyvalue: constant mode on from the start
#define SF_TCVAL_INVERT_TARGET_VALUE 32
#define SF_TCVAL_INVERT_SOURCE_VALUE 64
#define SF_TCVAL_MULTIPLE_DESTINATIONS 128	// trigger_copyvalue: every entity with the name (else the first)

enum changevalue_operations
{
	CVAL_OP_REPLACE = 0,
	CVAL_OP_ADD = 1,
	CVAL_OP_MUL = 2,
	CVAL_OP_SUB = 3,
	CVAL_OP_DIV = 4,
	CVAL_OP_AND = 5,
	CVAL_OP_OR = 6,
	CVAL_OP_NAND = 7,
	CVAL_OP_NOR = 8,
	CVAL_OP_DIR2ANG = 9,
	CVAL_OP_ANG2DIR = 10,
	CVAL_OP_APPEND = 11,
	CVAL_OP_MOD = 12,
	CVAL_OP_XOR = 13,
	CVAL_OP_NXOR = 14,
	CVAL_OP_CLEAR_BITS = 15,	// SBO 2.0, from Featureful: old & ~new
	CVAL_OP_POW = 16,
	CVAL_OP_SIN = 17,
	CVAL_OP_COS = 18,
	CVAL_OP_TAN = 19,
	CVAL_OP_ARCSIN = 20,
	CVAL_OP_ARCCOS = 21,
	CVAL_OP_ARCTAN = 22,
	CVAL_OP_COT = 23,
	CVAL_OP_ARCCOT = 24,
	CVAL_OP_MIN = 30,			// SBO 2.0, from Featureful: min(old, new)
	CVAL_OP_MAX = 31,			// SBO 2.0, from Featureful: max(old, new)
};

// SBO 2.0, from Featureful: "value_type" (Treat source value as) of trigger_changevalue: the new value is a literal,
// or a Locus key (docs/LOCUS.md) computed from the activator when the entity fires
enum changevalue_value_types
{
	CVAL_VALUE_LITERAL = 0,
	CVAL_VALUE_LOCUS_RATIO = 1,
	CVAL_VALUE_LOCUS_POSITION = 2,
	CVAL_VALUE_LOCUS_VELOCITY = 3,
};

enum changevalue_trig_behavior
{
	CVAL_TRIG_DEGREES,
	CVAL_TRIG_RADIANS
};

class CTriggerChangeValue : public CBaseLogic
{
public:
	CTriggerChangeValue( void );

	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT TimedThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int OperateInteger( int sourceVal, int targetVal );
	float OperateFloat( float sourceVal, float targetVal );
	Vector OperateVector( CKeyValue &keyvalue );
	const char *OperateString( const char *sourceVal, const char *targetVal );
	const char *Operate( CKeyValue &keyvalue );
	void HandleTarget( CBaseEntity *ent );
	BOOL LoadSourceValues( void );	// FALSE: a Locus value failed (nothing is changed)
	BOOL LoadSpecialStaticValue( const char *val, int &specialVal );
	void ChangeValues( void );

	int GetVectorDontUseFlags() { return pev->spawnflags & ( SF_TCVAL_DONT_USE_X | SF_TCVAL_DONT_USE_Y | SF_TCVAL_DONT_USE_Z ); }

	string_t m_iszKeyName;
	string_t m_iszNewValue;
	int m_iAppendSpaces;
	int m_iOperation;
	int m_iValueType;	// changevalue_value_types

	string_t m_iszDstKeyName;
	string_t m_iszSrcKeyName;
	int m_iFloatConversion;

	// source values loaded from source entity or static keyvalue
	Vector m_vSrc;
	float m_fSrc;
	int m_iSrc;
	const char *m_sSrc;

	float m_trigIn;		// multiplier for trigonemtric function input values (degrees -> degrees/radians)
	float m_trigOut;	// multiplier for trigonemtric function output values (radians -> degrees/radians)

	BOOL isCopyValue;	// this entitiy is a trigger_copyvalue not a trigger_changevalue
	BOOL isActive;
};

LINK_ENTITY_TO_CLASS( trigger_changevalue, CTriggerChangeValue );
LINK_ENTITY_TO_CLASS( trigger_copyvalue, CTriggerChangeValue );

TYPEDESCRIPTION	CTriggerChangeValue::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerChangeValue, m_iszKeyName, FIELD_STRING ),
	DEFINE_FIELD( CTriggerChangeValue, m_iszNewValue, FIELD_STRING ),
	DEFINE_FIELD( CTriggerChangeValue, m_iAppendSpaces, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerChangeValue, m_iOperation, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerChangeValue, m_iValueType, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerChangeValue, m_iszDstKeyName, FIELD_STRING ),
	DEFINE_FIELD( CTriggerChangeValue, m_iszSrcKeyName, FIELD_STRING ),
	DEFINE_FIELD( CTriggerChangeValue, m_iFloatConversion, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerChangeValue, m_trigIn, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerChangeValue, m_trigOut, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerChangeValue, isCopyValue, FIELD_BOOLEAN ),
	DEFINE_FIELD( CTriggerChangeValue, isActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CTriggerChangeValue, m_hActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( CTriggerChangeValue, m_hCaller, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CTriggerChangeValue, CPointEntity );

// SBO: degrees unless the map says otherwise (Sven Co-op's default; SevenKewp leaves both multipliers at zero when
// the key is missing)
CTriggerChangeValue::CTriggerChangeValue( void )
{
	m_trigIn = M_PI / 180.0f;
	m_trigOut = 180.0f / M_PI;
	m_sSrc = "";
}

void CTriggerChangeValue::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszValueName" ) )
	{
		m_iszKeyName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszDstValueName" ) )
	{
		m_iszDstKeyName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszNewValue" ) )
	{
		m_iszNewValue = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iAppendSpaces" ) )
	{
		m_iAppendSpaces = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszValueType" ) )
	{
		m_iOperation = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "value_type" ) )
	{
		m_iValueType = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_trigonometricBehaviour" ) )
	{
		int behavior = atoi( pkvd->szValue );

		if ( behavior == CVAL_TRIG_DEGREES )
		{
			// trig functions work with radians, so input/output values need conversion
			m_trigIn = M_PI / 180.0f;
			m_trigOut = 180.0f / M_PI;
		}
		else if ( behavior == CVAL_TRIG_RADIANS )
		{
			m_trigIn = 1.0f;
			m_trigOut = 1.0f;
		}
		else
			ALERT( at_console, "trigger_changevalue: invalid trig behavior %d\n", behavior );

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszSrcValueName" ) )
	{
		m_iszSrcKeyName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iFloatConversion" ) )
	{
		m_iFloatConversion = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTriggerChangeValue::Spawn( void )
{
	CPointEntity::Spawn();

	isCopyValue = !strcmp( STRING( pev->classname ), "trigger_copyvalue" );

	if ( !isCopyValue )
	{
		// disable copyvalue-specific settings
		pev->spawnflags &= ~( SF_TCVAL_CONSTANT | SF_TCVAL_MULTIPLE_DESTINATIONS | SF_TCVAL_START_ON );
		pev->netname = 0;
		pev->dmg = 0;
		m_iszSrcKeyName = 0;
		m_iFloatConversion = 0;
	}
	else
	{
		// disable changevalue-specific settings
		m_iszKeyName = m_iszDstKeyName;
		m_iszNewValue = 0;
	}

	if ( ( pev->spawnflags & SF_TCVAL_START_ON ) && ( pev->spawnflags & SF_TCVAL_CONSTANT ) )
	{
		isActive = TRUE;
		SetThink( &CTriggerChangeValue::TimedThink );
		pev->nextthink = gpGlobals->time + pev->dmg;
	}
}

int CTriggerChangeValue::OperateInteger( int sourceVal, int targetVal )
{
	if ( pev->spawnflags & SF_TCVAL_INVERT_TARGET_VALUE )
		targetVal = -targetVal;
	if ( pev->spawnflags & SF_TCVAL_INVERT_SOURCE_VALUE )
		sourceVal = -sourceVal;

	// SBO: the server must survive a map's division by zero
	if ( ( m_iOperation == CVAL_OP_DIV || m_iOperation == CVAL_OP_MOD ) && sourceVal == 0 )
	{
		ALERT( at_console, "%s \"%s\": division by zero\n", STRING( pev->classname ), STRING( pev->targetname ) );
		return targetVal;
	}

	switch ( m_iOperation )
	{
	case CVAL_OP_REPLACE: return sourceVal;
	case CVAL_OP_ADD: return targetVal + sourceVal;
	case CVAL_OP_MUL: return targetVal * sourceVal;
	case CVAL_OP_SUB: return targetVal - sourceVal;
	case CVAL_OP_DIV: return targetVal / sourceVal;
	case CVAL_OP_POW: return (int)pow( (double)targetVal, (double)sourceVal );
	case CVAL_OP_MOD: return targetVal % sourceVal;
	case CVAL_OP_AND: return targetVal & sourceVal;
	case CVAL_OP_OR: return targetVal | sourceVal;
	case CVAL_OP_XOR: return targetVal ^ sourceVal;
	case CVAL_OP_NAND: return ~( targetVal & sourceVal );
	case CVAL_OP_NOR: return ~( targetVal | sourceVal );
	case CVAL_OP_NXOR: return ~( targetVal ^ sourceVal );
	case CVAL_OP_CLEAR_BITS: return targetVal & ~sourceVal;
	case CVAL_OP_MIN: return min( targetVal, sourceVal );
	case CVAL_OP_MAX: return max( targetVal, sourceVal );
	case CVAL_OP_SIN: return (int)( sin( sourceVal * m_trigIn ) * m_trigOut );
	case CVAL_OP_COS: return (int)( cos( sourceVal * m_trigIn ) * m_trigOut );
	case CVAL_OP_TAN: return (int)( tan( sourceVal * m_trigIn ) * m_trigOut );
	case CVAL_OP_COT: return (int)( ( 1.0f / tan( sourceVal * m_trigIn ) ) * m_trigOut );
	case CVAL_OP_ARCSIN: return (int)( asin( sourceVal * m_trigIn ) * m_trigOut );
	case CVAL_OP_ARCCOS: return (int)( acos( sourceVal * m_trigIn ) * m_trigOut );
	case CVAL_OP_ARCTAN: return (int)( atan( sourceVal * m_trigIn ) * m_trigOut );
	case CVAL_OP_ARCCOT: return (int)( atan( 1.0f / ( sourceVal * m_trigIn ) ) * m_trigOut );
	default:
		ALERT( at_console, "%s \"%s\": invalid operation %d on integer value\n", STRING( pev->classname ),
			STRING( pev->targetname ), m_iOperation );
		return targetVal;
	}
}

float CTriggerChangeValue::OperateFloat( float sourceVal, float targetVal )
{
	if ( pev->spawnflags & SF_TCVAL_INVERT_TARGET_VALUE )
		targetVal = -targetVal;
	if ( pev->spawnflags & SF_TCVAL_INVERT_SOURCE_VALUE )
		sourceVal = -sourceVal;

	// SBO: the modulo is an integer one (and a float divided by zero would write "inf" into the key)
	if ( ( m_iOperation == CVAL_OP_MOD && (int)sourceVal == 0 ) || ( m_iOperation == CVAL_OP_DIV && sourceVal == 0 ) )
	{
		ALERT( at_console, "%s \"%s\": division by zero\n", STRING( pev->classname ), STRING( pev->targetname ) );
		return targetVal;
	}

	switch ( m_iOperation )
	{
	case CVAL_OP_REPLACE: return sourceVal;
	case CVAL_OP_ADD: return targetVal + sourceVal;
	case CVAL_OP_MUL: return targetVal * sourceVal;
	case CVAL_OP_SUB: return targetVal - sourceVal;
	case CVAL_OP_DIV: return targetVal / sourceVal;
	case CVAL_OP_POW: return (float)pow( (double)targetVal, (double)sourceVal );
	case CVAL_OP_MOD: return (float)( (int)targetVal % (int)sourceVal );
	case CVAL_OP_AND: return (float)( (int)targetVal & (int)sourceVal );
	case CVAL_OP_OR: return (float)( (int)targetVal | (int)sourceVal );
	case CVAL_OP_XOR: return (float)( (int)targetVal ^ (int)sourceVal );
	case CVAL_OP_NAND: return (float)( ~( (int)targetVal & (int)sourceVal ) );
	case CVAL_OP_NOR: return (float)( ~( (int)targetVal | (int)sourceVal ) );
	case CVAL_OP_NXOR: return (float)( ~( (int)targetVal ^ (int)sourceVal ) );
	case CVAL_OP_CLEAR_BITS: return (float)( (int)targetVal & ~(int)sourceVal );
	case CVAL_OP_MIN: return min( targetVal, sourceVal );
	case CVAL_OP_MAX: return max( targetVal, sourceVal );
	case CVAL_OP_SIN: return sin( sourceVal * m_trigIn ) * m_trigOut;
	case CVAL_OP_COS: return cos( sourceVal * m_trigIn ) * m_trigOut;
	case CVAL_OP_TAN: return tan( sourceVal * m_trigIn ) * m_trigOut;
	case CVAL_OP_COT: return ( 1.0f / tan( sourceVal * m_trigIn ) ) * m_trigOut;
	case CVAL_OP_ARCSIN: return asin( sourceVal * m_trigIn ) * m_trigOut;
	case CVAL_OP_ARCCOS: return acos( sourceVal * m_trigIn ) * m_trigOut;
	case CVAL_OP_ARCTAN: return atan( sourceVal * m_trigIn ) * m_trigOut;
	case CVAL_OP_ARCCOT: return atan( 1.0f / ( sourceVal * m_trigIn ) ) * m_trigOut;
	default:
		ALERT( at_console, "%s \"%s\": invalid operation %d on float value\n", STRING( pev->classname ),
			STRING( pev->targetname ), m_iOperation );
		return targetVal;
	}
}

Vector CTriggerChangeValue::OperateVector( CKeyValue &keyvalue )
{
	Vector vTemp( keyvalue.vVal[0], keyvalue.vVal[1], keyvalue.vVal[2] );

	switch ( m_iOperation )
	{
	case CVAL_OP_DIR2ANG:
		return UTIL_VecToAngles( vTemp.Normalize() );
	case CVAL_OP_ANG2DIR:
		UTIL_MakeVectors( vTemp );
		return gpGlobals->v_forward;
	default:
		if ( !( pev->spawnflags & SF_TCVAL_DONT_USE_X ) )
			vTemp[0] = OperateFloat( m_vSrc[0], keyvalue.vVal[0] );
		if ( !( pev->spawnflags & SF_TCVAL_DONT_USE_Y ) )
			vTemp[1] = OperateFloat( m_vSrc[1], keyvalue.vVal[1] );
		if ( !( pev->spawnflags & SF_TCVAL_DONT_USE_Z ) )
			vTemp[2] = OperateFloat( m_vSrc[2], keyvalue.vVal[2] );
		return vTemp;
	}
}

// The results live in one buffer: the value is dispatched before the next one is computed
static char s_szResult[ KEYVALUE_MAX_VALUE_LEN ];

const char *CTriggerChangeValue::Operate( CKeyValue &keyvalue )
{
	switch ( keyvalue.keyType )
	{
	case KEY_TYPE_FLOAT:
		snprintf( s_szResult, sizeof( s_szResult ), "%f", OperateFloat( m_fSrc, keyvalue.fVal ) );
		return s_szResult;
	case KEY_TYPE_INT:
		snprintf( s_szResult, sizeof( s_szResult ), "%d", OperateInteger( m_iSrc, keyvalue.iVal ) );
		return s_szResult;
	case KEY_TYPE_VECTOR:
	{
		Vector vTemp = OperateVector( keyvalue );
		snprintf( s_szResult, sizeof( s_szResult ), "%f %f %f", vTemp.x, vTemp.y, vTemp.z );
		return s_szResult;
	}
	case KEY_TYPE_STRING:
		return OperateString( m_sSrc, keyvalue.sVal ? STRING( keyvalue.sVal ) : "" );
	default:
		ALERT( at_console, "%s \"%s\": operation on keyvalue %s not allowed\n", STRING( pev->classname ),
			STRING( pev->targetname ), keyvalue.keyName );
		return "";
	}
}

const char *CTriggerChangeValue::OperateString( const char *sourceVal, const char *targetVal )
{
	static char spaces[ 256 ];

	int appendCnt = m_iAppendSpaces > 0 ? min( (int)sizeof( spaces ) - 1, m_iAppendSpaces ) : 0;
	memset( spaces, ' ', appendCnt );
	spaces[ appendCnt ] = 0;

	switch ( m_iOperation )
	{
	case CVAL_OP_REPLACE:
		snprintf( s_szResult, sizeof( s_szResult ), "%s%s", sourceVal, spaces );
		return s_szResult;
	case CVAL_OP_APPEND:
		snprintf( s_szResult, sizeof( s_szResult ), "%s%s%s", targetVal, sourceVal, spaces );
		return s_szResult;
	default:
		ALERT( at_console, "%s \"%s\": invalid operation %d on string value\n", STRING( pev->classname ),
			STRING( pev->targetname ), m_iOperation );
		return targetVal;
	}
}

void CTriggerChangeValue::HandleTarget( CBaseEntity *pent )
{
	if ( !pent )
		return;

	const char *szKeyName = STRING( m_iszKeyName );
	const char *szValue = m_sSrc;

	CKeyValue keyvalue = pent->GetKeyValue( szKeyName );

	if ( keyvalue.keyType )
	{
		szValue = Operate( keyvalue );
	}
	else if ( m_iOperation != CVAL_OP_REPLACE )
	{
		ALERT( at_console, "%s \"%s\": keyvalue '%s' in entity '%s' can only be replaced\n", STRING( pev->classname ),
			STRING( pev->targetname ), szKeyName, STRING( pent->pev->classname ) );
	}

	// using this for private fields. SBO: KeyValue_DispatchRuntime refuses model and sound keys.
	KeyValue_DispatchRuntime( pent->edict(), szKeyName, szValue );
	UTIL_SetOrigin( pent->pev, pent->pev->origin );	// in case any changes to solid/origin were made
}

// "FL_ONGROUND", "MOVETYPE_FLY", ... for a static new value (trigger_changevalue), also with a leading "~"
BOOL CTriggerChangeValue::LoadSpecialStaticValue( const char *val, int &specialVal )
{
	static const struct
	{
		const char *name;
		int value;
	} special_vals[] =
	{
		{"FL_FLY", FL_FLY},
		{"FL_SWIM", FL_SWIM},
		{"FL_CONVEYOR", FL_CONVEYOR},
		{"FL_CLIENT", FL_CLIENT},
		{"FL_INWATER", FL_INWATER},
		{"FL_MONSTER", FL_MONSTER},
		{"FL_GODMODE", FL_GODMODE},
		{"FL_NOTARGET", FL_NOTARGET},
		{"FL_SKIPLOCALHOST", FL_SKIPLOCALHOST},
		{"FL_ONGROUND", FL_ONGROUND},
		{"FL_PARTIALGROUND", FL_PARTIALGROUND},
		{"FL_WATERJUMP", FL_WATERJUMP},
		{"FL_FROZEN", FL_FROZEN},
		{"FL_FAKECLIENT", FL_FAKECLIENT},
		{"FL_DUCKING", FL_DUCKING},
		{"FL_FLOAT", FL_FLOAT},

		{"IN_ATTACK", IN_ATTACK},
		{"IN_JUMP", IN_JUMP},
		{"IN_DUCK", IN_DUCK},
		{"IN_FORWARD", IN_FORWARD},
		{"IN_BACK", IN_BACK},
		{"IN_USE", IN_USE},
		{"IN_CANCEL", IN_CANCEL},
		{"IN_LEFT", IN_LEFT},
		{"IN_RIGHT", IN_RIGHT},
		{"IN_MOVELEFT", IN_MOVELEFT},
		{"IN_MOVERIGHT", IN_MOVERIGHT},
		{"IN_ATTACK2", IN_ATTACK2},
		{"IN_RUN", IN_RUN},
		{"IN_RELOAD", IN_RELOAD},
		{"IN_ALT1", IN_ALT1},
		{"IN_SCORE", IN_SCORE},

		{"DEAD_NO", DEAD_NO},
		{"DEAD_DYING", DEAD_DYING},
		{"DEAD_DEAD", DEAD_DEAD},
		{"DEAD_RESPAWNABLE", DEAD_RESPAWNABLE},
		{"DEAD_DISCARDBODY", DEAD_DISCARDBODY},

		{"DAMAGE_NO", DAMAGE_NO},
		{"DAMAGE_YES", DAMAGE_YES},
		{"DAMAGE_AIM", DAMAGE_AIM},

		{"SOLID_NOT", SOLID_NOT},
		{"SOLID_TRIGGER", SOLID_TRIGGER},
		{"SOLID_BBOX", SOLID_BBOX},
		{"SOLID_SLIDEBOX", SOLID_SLIDEBOX},
		{"SOLID_BSP", SOLID_BSP},

		// pev->waterlevel
		{"WATERLEVEL_DRY", 0},
		{"WATERLEVEL_FEET", 1},
		{"WATERLEVEL_WAIST", 2},
		{"WATERLEVEL_HEAD", 3},

		{"MOVETYPE_NONE", MOVETYPE_NONE},
		{"MOVETYPE_WALK", MOVETYPE_WALK},
		{"MOVETYPE_STEP", MOVETYPE_STEP},
		{"MOVETYPE_FLY", MOVETYPE_FLY},
		{"MOVETYPE_TOSS", MOVETYPE_TOSS},
		{"MOVETYPE_PUSH", MOVETYPE_PUSH},
		{"MOVETYPE_NOCLIP", MOVETYPE_NOCLIP},
		{"MOVETYPE_FLYMISSILE", MOVETYPE_FLYMISSILE},
		{"MOVETYPE_BOUNCE", MOVETYPE_BOUNCE},
		{"MOVETYPE_BOUNCEMISSILE", MOVETYPE_BOUNCEMISSILE},
		{"MOVETYPE_FOLLOW", MOVETYPE_FOLLOW},
		{"MOVETYPE_PUSHSTEP", MOVETYPE_PUSHSTEP},

		{"EF_BRIGHTFIELD", EF_BRIGHTFIELD},
		{"EF_MUZZLEFLASH", EF_MUZZLEFLASH},
		{"EF_BRIGHTLIGHT", EF_BRIGHTLIGHT},
		{"EF_DIMLIGHT", EF_DIMLIGHT},
		{"EF_INVLIGHT", EF_INVLIGHT},
		{"EF_NOINTERP", EF_NOINTERP},
		{"EF_LIGHT", EF_LIGHT},
		{"EF_NODRAW", EF_NODRAW},

		{"KRENDERFXNONE", kRenderFxNone},
		{"KRENDERFXPULSESLOW", kRenderFxPulseSlow},
		{"KRENDERFXPULSEFAST", kRenderFxPulseFast},
		{"KRENDERFXPULSESLOWWIDE", kRenderFxPulseSlowWide},
		{"KRENDERFXPULSEFASTWIDE", kRenderFxPulseFastWide},
		{"KRENDERFXFADESLOW", kRenderFxFadeSlow},
		{"KRENDERFXFADEFAST", kRenderFxFadeFast},
		{"KRENDERFXSOLIDSLOW", kRenderFxSolidSlow},
		{"KRENDERFXSOLIDFAST", kRenderFxSolidFast},
		{"KRENDERFXSTROBESLOW", kRenderFxStrobeSlow},
		{"KRENDERFXSTROBEFAST", kRenderFxStrobeFast},
		{"KRENDERFXSTROBEFASTER", kRenderFxStrobeFaster},
		{"KRENDERFXFLICKERFAST", kRenderFxFlickerFast},
		{"KRENDERFXNODISSIPATION", kRenderFxNoDissipation},
		{"KRENDERFXDISTORT", kRenderFxDistort},
		{"KRENDERFXHOLOGRAM", kRenderFxHologram},
		{"KRENDERFXDEADPLAYER", kRenderFxDeadPlayer},
		{"KRENDERFXEXPLODE", kRenderFxExplode},
		{"KRENDERFXGLOWSHELL", kRenderFxGlowShell},
		{"KRENDERFXCLAMPMINSCALE", kRenderFxClampMinScale},

		{"KRENDERNORMAL", kRenderNormal},
		{"KRENDERTRANSCOLOR", kRenderTransColor},
		{"KRENDERTRANSTEXTURE", kRenderTransTexture},
		{"KRENDERGLOW", kRenderGlow},
		{"KRENDERTRANSALPHA", kRenderTransAlpha},
		{"KRENDERTRANSADD", kRenderTransAdd},

		// SBO: without Opposing Force's CLASS_XRACE_* / CLASS_ALIEN_RACE_X*
		{"CLASS_NONE", CLASS_NONE},
		{"CLASS_MACHINE", CLASS_MACHINE},
		{"CLASS_PLAYER", CLASS_PLAYER},
		{"CLASS_HUMAN_PASSIVE", CLASS_HUMAN_PASSIVE},
		{"CLASS_HUMAN_MILITARY", CLASS_HUMAN_MILITARY},
		{"CLASS_ALIEN_MILITARY", CLASS_ALIEN_MILITARY},
		{"CLASS_ALIEN_PASSIVE", CLASS_ALIEN_PASSIVE},
		{"CLASS_ALIEN_MONSTER", CLASS_ALIEN_MONSTER},
		{"CLASS_ALIEN_PREY", CLASS_ALIEN_PREY},
		{"CLASS_ALIEN_PREDATOR", CLASS_ALIEN_PREDATOR},
		{"CLASS_INSECT", CLASS_INSECT},
		{"CLASS_PLAYER_ALLY", CLASS_PLAYER_ALLY},
		{"CLASS_PLAYER_BIOWEAPON", CLASS_PLAYER_BIOWEAPON},
		{"CLASS_ALIEN_BIOWEAPON", CLASS_ALIEN_BIOWEAPON},
		{"CLASS_BARNACLE", CLASS_BARNACLE},
	};

	for ( int i = 0; i < (int)( sizeof( special_vals ) / sizeof( special_vals[0] ) ); i++ )
	{
		if ( !stricmp( special_vals[i].name, val ) )
		{
			specialVal = special_vals[i].value;
			return TRUE;
		}
	}

	return FALSE;
}

BOOL CTriggerChangeValue::LoadSourceValues( void )
{
	// the source as text, until the keyvalues are sent
	static char temp[ LOGIC_STRING_LEN ];

	// set up source values
	m_iSrc = 0;
	m_fSrc = 0;
	m_sSrc = "";
	m_vSrc = g_vecZero;

	if ( isCopyValue )
	{
		CBaseEntity *pent = FindLogicEntity( NULL, pev->netname );
		if ( pent )
		{
			CKeyValue srcKey = pent->GetKeyValue( STRING( m_iszSrcKeyName ) );
			if ( srcKey.keyType )
			{
				switch ( srcKey.keyType )
				{
				case KEY_TYPE_FLOAT:
					m_fSrc = srcKey.fVal;
					m_iSrc = FloatToInteger( m_fSrc, m_iFloatConversion );
					m_vSrc = Vector( srcKey.fVal, srcKey.fVal, srcKey.fVal );
					FloatToString( srcKey.fVal, m_iFloatConversion, temp );
					m_sSrc = temp;
					break;
				case KEY_TYPE_INT:
					m_fSrc = srcKey.iVal;
					m_iSrc = FloatToInteger( m_fSrc, m_iFloatConversion );
					m_vSrc = Vector( srcKey.iVal, srcKey.iVal, srcKey.iVal );
					snprintf( temp, sizeof( temp ), "%d", srcKey.iVal );
					m_sSrc = temp;
					break;
				case KEY_TYPE_VECTOR:
				{
					Vector vSrc( srcKey.vVal[0], srcKey.vVal[1], srcKey.vVal[2] );
					m_fSrc = VectorToFloat( vSrc );
					m_iSrc = FloatToInteger( m_fSrc, m_iFloatConversion );
					m_vSrc = vSrc;
					VectorToString( vSrc, m_iFloatConversion, temp );
					m_sSrc = temp;
					break;
				}
				case KEY_TYPE_STRING:
					m_fSrc = 0;
					m_iSrc = 0;
					m_vSrc = g_vecZero;
					m_sSrc = srcKey.sVal ? STRING( srcKey.sVal ) : "";
					break;
				default:
					ALERT( at_console, "'%s' (%s): operation on keyvalue %s not allowed\n",
						STRING( pev->targetname ), STRING( pev->classname ), srcKey.keyName );
					return TRUE;
				}
			}
		}
	}
	else if ( !FStringNull( m_iszNewValue ) && m_iValueType >= CVAL_VALUE_LOCUS_RATIO && m_iValueType <= CVAL_VALUE_LOCUS_VELOCITY )
	{
		// SBO 2.0, from Featureful: a Locus value, computed from the activator now
		CBaseEntity *pLocus = m_hActivator;
		if ( m_iValueType == CVAL_VALUE_LOCUS_RATIO )
		{
			float flRatio = 0;
			if ( !TryCalcLocus_Ratio( pLocus, STRING( m_iszNewValue ), flRatio ) )
				return FALSE;
			m_fSrc = flRatio;
			m_iSrc = (int)flRatio;
			m_vSrc = Vector( flRatio, flRatio, flRatio );
			snprintf( temp, sizeof( temp ), "%g", flRatio );
		}
		else
		{
			Vector vec;
			BOOL ok = m_iValueType == CVAL_VALUE_LOCUS_POSITION ? TryCalcLocus_Position( this, pLocus, STRING( m_iszNewValue ), vec )
				: TryCalcLocus_Velocity( this, pLocus, STRING( m_iszNewValue ), vec );
			if ( !ok )
				return FALSE;
			m_vSrc = vec;
			m_fSrc = VectorToFloat( vec );
			m_iSrc = (int)m_fSrc;
			snprintf( temp, sizeof( temp ), "%g %g %g", vec.x, vec.y, vec.z );
		}
		m_sSrc = temp;
	}
	else if ( !FStringNull( m_iszNewValue ) )
	{
		// trigger_changevalue static value
		const char *cNewVal = STRING( m_iszNewValue );
		UTIL_StringToVector( m_vSrc, cNewVal );

		if ( m_vSrc.y != 0 || m_vSrc.z != 0 )
		{
			// length of vector used when assigning a vector to a float/int
			m_fSrc = VectorToFloat( m_vSrc );
			m_iSrc = (int)m_fSrc;
		}
		else
		{
			// single value used
			if ( cNewVal[0] == '~' )
			{
				cNewVal = cNewVal + 1;

				int specialVal = 0;
				if ( LoadSpecialStaticValue( cNewVal, specialVal ) )
				{
					m_fSrc = ~specialVal;
					m_iSrc = ~specialVal;
				}
				else
				{
					m_fSrc = ~( (int)atof( cNewVal ) );
					m_iSrc = ~atoi( cNewVal );
				}
			}
			else
			{
				int specialVal = 0;
				if ( LoadSpecialStaticValue( cNewVal, specialVal ) )
				{
					m_fSrc = specialVal;
					m_iSrc = specialVal;
				}
				else
				{
					m_fSrc = atof( cNewVal );
					m_iSrc = atoi( cNewVal );
				}
			}

			m_vSrc = Vector( m_fSrc, m_fSrc, m_fSrc );
		}

		m_sSrc = STRING( m_iszNewValue );
	}
	return TRUE;
}

void CTriggerChangeValue::ChangeValues( void )
{
	if ( isCopyValue )
	{
		if ( FStringNull( pev->netname ) )
		{
			ALERT( at_console, "'%s' (%s): missing source entity\n", STRING( pev->targetname ), STRING( pev->classname ) );
			return;
		}
		if ( FStringNull( m_iszSrcKeyName ) )
		{
			ALERT( at_console, "'%s' (%s): missing source key\n", STRING( pev->targetname ), STRING( pev->classname ) );
			return;
		}
	}

	// SevenKewp checks this for trigger_changevalue only; a trigger_copyvalue without a destination key would
	// dispatch an empty key name
	if ( FStringNull( m_iszKeyName ) || !STRING( m_iszKeyName )[0] )
	{
		ALERT( at_console, "'%s' (%s): missing key name\n", STRING( pev->targetname ), STRING( pev->classname ) );
		return;
	}

	if ( !LoadSourceValues() )
		return;

	if ( !FStringNull( pev->target ) )
	{
		const char *target = STRING( pev->target );

		if ( !strcmp( target, "!activator" ) )
		{
			HandleTarget( m_hActivator );
		}
		else if ( !strcmp( target, "!caller" ) )
		{
			HandleTarget( m_hCaller );
		}
		else
		{
			CBaseEntity *ent = NULL;

			while ( ( ent = UTIL_FindEntityByTargetname( ent, target ) ) != NULL )
			{
				HandleTarget( ent );

				if ( isCopyValue && !( pev->spawnflags & SF_TCVAL_MULTIPLE_DESTINATIONS ) )
					break;
			}
		}
	}

	if ( !FStringNull( pev->message ) )
		FireLogicTargets( STRING( pev->message ), USE_TOGGLE, 0.0f );
}

void CTriggerChangeValue::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SevenKewp: FireTarget sets these on every entity it uses
	m_hActivator = pActivator;
	m_hCaller = pCaller;

	ChangeValues();

	if ( pev->spawnflags & SF_TCVAL_CONSTANT )
	{
		isActive = useType == USE_TOGGLE ? !isActive : ( useType != USE_OFF );

		if ( isActive )
		{
			SetThink( &CTriggerChangeValue::TimedThink );
			pev->nextthink = gpGlobals->time + pev->dmg;
		}
		else
		{
			SetThink( NULL );
			pev->nextthink = 0;
		}
	}
}

void CTriggerChangeValue::TimedThink( void )
{
	ChangeValues();
	pev->nextthink = gpGlobals->time + pev->dmg;
}
