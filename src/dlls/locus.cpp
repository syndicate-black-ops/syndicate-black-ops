//=========================================================
// SBO 2.0 Locus system and calc entities (locus.h, docs/LOCUS.md).
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license):
// dlls/locus.cpp (TryCalcLocus_*, calc_position, calc_ratio, calc_numfromvec, calc_vecfromnums, calc_subvelocity,
// calc_velocity_path, calc_velocity_polar, locus_variable, calc_eval_number), dlls/maprules.cpp (game_number),
// dlls/triggers.cpp (trigger_compare), dlls/util.cpp (UTIL_FindEntityByTargetname with an activator,
// UTIL_StringToRandomVector). Featureful took them from Spirit of Half-Life (Laurie Cheers). The Spirit of Half-Life
// 1.8 names (halflife-updated-spirit dlls/locus.cpp) are second class names of the same entities.
//
// SBO changes (marked "SBO" below), all against hostile or broken map data (servers load third-party maps):
//   * "a..b" with a long left side no longer overflows a 128-byte buffer (Featureful strncpy'd it unchecked).
//   * Calc entities that refer to each other (A uses B uses A) stop at a depth of 32 instead of recursing until the
//     stack overflows.
//   * Clamp "wrap" / "bounce" with a huge value or an empty range no longer loops forever.
//   * A NaN or infinite result is a failure (it would end up in an origin or a velocity).
//   * Attachments are read only from a studio model that has them (Featureful/Spirit read any entity's).
//   * Division by a zero length in calc_velocity_path is skipped; console notes with missing arguments fixed.
//   * locus_variable's marker entities stop being made when the map is close to the edict limit.
//   * "*player" is the activator when it is a player (as Featureful in multiplayer); spectators never activate.
//   * game_number and locus_variable take "round_reset" (round_hooks.h).
//=========================================================

// LOCUS_FUZZ (tests/locus): only the value parsing and the clamp, on stub headers
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"studio.h"
#include	"locus.h"

#define LOCUS_MAX_DEPTH		32		// SBO: calc entities in one evaluation
#define LOCUS_EDICT_MARGIN	64		// SBO: locus_variable makes no marker within this many edicts of the limit

static int s_iLocusDepth;

// SBO: one level of calc evaluation; refused past LOCUS_MAX_DEPTH (a loop between calc entities)
class CLocusDepth
{
public:
	CLocusDepth() { m_bOk = ++s_iLocusDepth <= LOCUS_MAX_DEPTH; if ( !m_bOk && s_iLocusDepth == LOCUS_MAX_DEPTH + 1 ) ALERT( at_console, "Locus: calc entities refer to each other in a loop (more than %d levels); value ignored\n", LOCUS_MAX_DEPTH ); }
	~CLocusDepth() { s_iLocusDepth--; }
	BOOL m_bOk;
};

#define LOCUS_GUARD()	CLocusDepth locusDepth; if ( !locusDepth.m_bOk ) return FALSE

static BOOL Locus_Finite( float f ) { return f == f && f < 1e30f && f > -1e30f; }
static BOOL Locus_FiniteVec( const Vector &v ) { return Locus_Finite( v.x ) && Locus_Finite( v.y ) && Locus_Finite( v.z ); }

BOOL Locus_IsLikelyNumber( const char *szText )
{
	return ( *szText >= '0' && *szText <= '9' ) || *szText == '-';
}

CBaseEntity *Locus_FindEntity( CBaseEntity *pStart, const char *szName, CBaseEntity *pActivator )
{
	if ( !szName || !szName[0] )
		return NULL;

	if ( FStrEq( szName, "*locus" ) || FStrEq( szName, "!activator" ) )
	{
		if ( pActivator && ( pStart == NULL || pActivator->eoffset() > pStart->eoffset() ) )
			return pActivator;
		return NULL;
	}

	if ( FStrEq( szName, "*player" ) )
	{
		// Featureful's EffectivePlayer in multiplayer: the activator, if it is a player
		if ( pActivator && pActivator->IsPlayer() && ( pStart == NULL || pActivator->eoffset() > pStart->eoffset() ) )
			return pActivator;
		return NULL;
	}

	return UTIL_FindEntityByTargetname( pStart, szName );
}

void Locus_StringToRandomVector( float *pVector, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;
	float pAltVec[3];

	strncpy( tempString, pString, sizeof( tempString ) - 1 );
	tempString[ sizeof( tempString ) - 1 ] = '\0';
	pstr = pfront = tempString;

	for ( j = 0; j < 3; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atof( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if ( !*pstr )
			break;
		pstr++;
		pfront = pstr;
	}
	if ( j < 2 )
	{
		for ( j = j + 1; j < 3; j++ )
			pVector[j] = 0;
	}
	else if ( *pstr == '.' )
	{
		pstr++;
		if ( *pstr != '.' )
			return;
		pstr++;
		if ( *pstr != ' ' )
			return;

		UTIL_StringToVector( pAltVec, pstr );

		pVector[0] = RANDOM_FLOAT( pVector[0], pAltVec[0] );
		pVector[1] = RANDOM_FLOAT( pVector[1], pAltVec[1] );
		pVector[2] = RANDOM_FLOAT( pVector[2], pAltVec[2] );
	}
}

static const char *Locus_Classname( CBaseEntity *pEntity ) { return pEntity ? STRING( pEntity->pev->classname ) : ""; }
static const char *Locus_Targetname( CBaseEntity *pEntity ) { return pEntity ? STRING( pEntity->pev->targetname ) : ""; }

BOOL TryCalcLocus_Position( CBaseEntity *pEntity, CBaseEntity *pLocus, const char *szText, Vector &result, BOOL showError )
{
	LOCUS_GUARD();

	if ( !szText )
		return FALSE;

	Vector tmp;
	BOOL ok = FALSE;

	if ( Locus_IsLikelyNumber( szText ) )
	{
		Locus_StringToRandomVector( tmp, szText );
		ok = TRUE;
	}
	else
	{
		CBaseEntity *pCalc = Locus_FindEntity( NULL, szText, pLocus );
		if ( pCalc )
			ok = pCalc->CalcPosition( pLocus, &tmp );
		else if ( showError )
			ALERT( at_console, "%s \"%s\" has bad or missing [LP] value \"%s\"\n", Locus_Classname( pEntity ), Locus_Targetname( pEntity ), szText );
	}

	if ( !ok || !Locus_FiniteVec( tmp ) )
		return FALSE;
	result = tmp;
	return TRUE;
}

BOOL TryCalcLocus_Velocity( CBaseEntity *pEntity, CBaseEntity *pLocus, const char *szText, Vector &result, BOOL showError )
{
	LOCUS_GUARD();

	if ( !szText )
		return FALSE;

	Vector tmp;
	BOOL ok = FALSE;

	if ( Locus_IsLikelyNumber( szText ) )
	{
		Locus_StringToRandomVector( tmp, szText );
		ok = TRUE;
	}
	else
	{
		CBaseEntity *pCalc = Locus_FindEntity( NULL, szText, pLocus );
		if ( pCalc )
			ok = pCalc->CalcVelocity( pLocus, &tmp );
		else if ( showError )
			ALERT( at_console, "%s \"%s\" has bad or missing [LV] value \"%s\"\n", Locus_Classname( pEntity ), Locus_Targetname( pEntity ), szText );
	}

	if ( !ok || !Locus_FiniteVec( tmp ) )
		return FALSE;
	result = tmp;
	return TRUE;
}

static BOOL TryCalcLocus_RatioNonRandom( CBaseEntity *pLocus, const char *szText, float &result, BOOL showError )
{
	if ( Locus_IsLikelyNumber( szText ) )
	{
		result = atof( szText );	// assume it's a float
		return TRUE;
	}

	CBaseEntity *pCalc = Locus_FindEntity( NULL, szText, pLocus );
	if ( pCalc )
		return pCalc->CalcRatio( pLocus, &result );

	if ( showError )
		ALERT( at_console, "Bad or missing [LR] value \"%s\"\n", szText );
	return FALSE;
}

BOOL TryCalcLocus_Ratio( CBaseEntity *pLocus, const char *szText, float &result, BOOL showError )
{
	LOCUS_GUARD();

	if ( !szText )
		return FALSE;

	float value = 0;
	BOOL ok = FALSE;
	BOOL bRange = FALSE;

	for ( int i = 0; szText[i]; i++ )
	{
		if ( szText[i] == '.' && szText[i + 1] == '.' )
		{
			bRange = TRUE;

			// SBO: the left side must fit the buffer (Featureful copied it unchecked)
			char szComponentName[128];
			if ( i >= (int)sizeof( szComponentName ) )
				return FALSE;
			memcpy( szComponentName, szText, i );
			szComponentName[i] = '\0';

			float A = 0, B = 0;
			BOOL bA = TryCalcLocus_RatioNonRandom( pLocus, szComponentName, A, showError );
			BOOL bB = TryCalcLocus_RatioNonRandom( pLocus, &szText[i + 2], B, showError );

			if ( bA && bB )
				value = RANDOM_FLOAT( A, B );
			else if ( bA )
				value = A;
			else if ( bB )
				value = B;
			ok = bA || bB;
			break;
		}
	}

	if ( !bRange )
		ok = TryCalcLocus_RatioNonRandom( pLocus, szText, value, showError );

	if ( !ok || !Locus_Finite( value ) )
		return FALSE;
	result = value;
	return TRUE;
}

BOOL TryCalcLocus_Color( CBaseEntity *pEntity, CBaseEntity *pLocus, const char *szText, Vector &result, BOOL showError )
{
	LOCUS_GUARD();

	if ( !szText )
		return FALSE;

	if ( Locus_IsLikelyNumber( szText ) )
	{
		Vector tmp;
		Locus_StringToRandomVector( tmp, szText );
		if ( !Locus_FiniteVec( tmp ) )
			return FALSE;
		result = tmp;
		return TRUE;
	}

	CBaseEntity *pCalc = Locus_FindEntity( NULL, szText, pLocus );
	if ( pCalc )
	{
		result = pCalc->pev->rendercolor;
		return TRUE;
	}

	if ( showError )
		ALERT( at_console, "%s \"%s\" has bad or missing color value \"%s\"\n", Locus_Classname( pEntity ), Locus_Targetname( pEntity ), szText );
	return FALSE;
}

#ifndef LOCUS_FUZZ

// SBO: an attachment of a studio model that has it (Featureful: any CBaseAnimating; Spirit: any entity)
static BOOL Locus_GetAttachment( CBaseEntity *pEntity, int iAttachment, Vector &origin, Vector &angles )
{
	const char *model = STRING( pEntity->pev->model );
	size_t len = model ? strlen( model ) : 0;

	if ( !pEntity->pev->modelindex || len < 4 || stricmp( model + len - 4, ".mdl" ) )
		return FALSE;

	studiohdr_t *pStudio = (studiohdr_t *)GET_MODEL_PTR( pEntity->edict() );
	if ( !pStudio || iAttachment < 0 || iAttachment >= pStudio->numattachments )
		return FALSE;

	GET_ATTACHMENT( pEntity->edict(), iAttachment, origin, angles );
	return TRUE;
}

//=============================================
// calc_position (Spirit of Half-Life 1.8: calc_posfroment)
//=============================================

class CCalcPosition : public CPointEntity
{
public:
	BOOL CalcPosition( CBaseEntity *pLocus, Vector *outVector );
};

LINK_ENTITY_TO_CLASS( calc_position, CCalcPosition );
LINK_ENTITY_TO_CLASS( calc_posfroment, CCalcPosition );	// Spirit of Half-Life 1.8

BOOL CCalcPosition::CalcPosition( CBaseEntity *pLocus, Vector *outVector )
{
	LOCUS_GUARD();

	CBaseEntity *pSubject = Locus_FindEntity( NULL, STRING( pev->netname ), pLocus );
	if ( !pSubject )
	{
		ALERT( at_console, "%s \"%s\" failed to find target entity \"%s\"\n", STRING( pev->classname ), STRING( pev->targetname ), STRING( pev->netname ) );
		return FALSE;
	}

	Vector vecOffset = g_vecZero;

	if ( pev->message )
	{
		if ( !TryCalcLocus_Velocity( this, pLocus, STRING( pev->message ), vecOffset ) )
		{
			ALERT( at_console, "%s \"%s\" failed, bad LV \"%s\"\n", STRING( pev->classname ), STRING( pev->targetname ), STRING( pev->message ) );
			return FALSE;
		}
	}

	Vector vecPosition;
	Vector vecJunk;

	switch ( pev->impulse )
	{
	case 1: // eyes
		*outVector = vecOffset + pSubject->EyePosition();
		break;
	case 2: // top
		*outVector = vecOffset + pSubject->pev->origin + Vector(
			( pSubject->pev->mins.x + pSubject->pev->maxs.x ) / 2,
			( pSubject->pev->mins.y + pSubject->pev->maxs.y ) / 2,
			pSubject->pev->maxs.z );
		break;
	case 3: // centre
		*outVector = vecOffset + pSubject->pev->origin + Vector(
			( pSubject->pev->mins.x + pSubject->pev->maxs.x ) / 2,
			( pSubject->pev->mins.y + pSubject->pev->maxs.y ) / 2,
			( pSubject->pev->mins.z + pSubject->pev->maxs.z ) / 2 );
		break;
	case 4: // bottom
		*outVector = vecOffset + pSubject->pev->origin + Vector(
			( pSubject->pev->mins.x + pSubject->pev->maxs.x ) / 2,
			( pSubject->pev->mins.y + pSubject->pev->maxs.y ) / 2,
			pSubject->pev->mins.z );
		break;
	case 5:
	case 6:
	case 7:
	case 8:
		if ( !Locus_GetAttachment( pSubject, pev->impulse - 5, vecPosition, vecJunk ) )
		{
			ALERT( at_console, "%s \"%s\": \"%s\" has no attachment %d\n", STRING( pev->classname ), STRING( pev->targetname ), STRING( pSubject->pev->classname ), pev->impulse - 4 );
			return FALSE;
		}
		*outVector = vecOffset + vecPosition;
		break;
	case 9:
		*outVector = vecOffset + pSubject->pev->origin + Vector(
			RANDOM_FLOAT( pSubject->pev->mins.x, pSubject->pev->maxs.x ),
			RANDOM_FLOAT( pSubject->pev->mins.y, pSubject->pev->maxs.y ),
			RANDOM_FLOAT( pSubject->pev->mins.z, pSubject->pev->maxs.z ) );
		break;
	case 10:
		if ( pSubject == this )
		{
			ALERT( at_console, "Recursion: %s refers to its own CalcPosition\n", STRING( pev->targetname ) );
			return FALSE;
		}
		if ( !pSubject->CalcPosition( pLocus, outVector ) )
			return FALSE;
		break;
	case 11: // body (Featureful 30fc2f71): the point monsters aim at
		*outVector = vecOffset + pSubject->BodyTarget( pSubject->pev->origin );
		break;
	default:
		*outVector = vecOffset + pSubject->pev->origin;
		break;
	}
	return TRUE;
}

#endif // LOCUS_FUZZ

//=======================================================

// SBO: Featureful's loops, bounded: "wrap" and "bounce" of a huge value against a small range, or an empty range,
// looped (nearly) forever
#define LOCUS_CLAMP_STEPS	64

static BOOL ClampToMinMax( CBaseEntity *pLocus, float &fBasis, string_t iszMin, string_t iszMax, int clampPolicy )
{
	if ( !FStringNull( iszMin ) )
	{
		float fMin = 0;
		if ( !TryCalcLocus_Ratio( pLocus, STRING( iszMin ), fMin ) )
			return FALSE;

		if ( !FStringNull( iszMax ) )
		{
			float fMax = 0;
			if ( !TryCalcLocus_Ratio( pLocus, STRING( iszMax ), fMax ) )
				return FALSE;

			if ( fBasis >= fMin && fBasis <= fMax )
				return TRUE;

			const float range = fMax - fMin;
			int steps = 0;

			switch ( clampPolicy )
			{
			case 1: // wrap
				if ( range > 0 )
				{
					while ( fBasis < fMin && steps++ < LOCUS_CLAMP_STEPS )
						fBasis += range;
					while ( fBasis > fMax && steps++ < LOCUS_CLAMP_STEPS )
						fBasis -= range;
					if ( fBasis >= fMin && fBasis <= fMax )
						return TRUE;
					// far outside: the same result in closed form
					float t = fmodf( fBasis - fMin, range );
					if ( t < 0 )
						t += range;
					fBasis = fMin + t;
					return Locus_Finite( fBasis );
				}
				break;	// empty or reversed range: clamp
			case 2: // bounce (Featureful's reflections, step by step)
				if ( range > 0 )
				{
					while ( ( fBasis < fMin || fBasis > fMax ) && steps++ < LOCUS_CLAMP_STEPS )
					{
						if ( fBasis < fMin )
							fBasis = fMin + fMax - fBasis;
						else
							fBasis = fMax + fMax - fBasis;
					}
					if ( fBasis >= fMin && fBasis <= fMax )
						return TRUE;
				}
				break;	// too far outside: clamp
			default:
				break;
			}

			if ( fBasis < fMin )
				fBasis = fMin;
			else
				fBasis = fMax;
			return TRUE;
		}

		if ( fBasis < fMin )
			fBasis = fMin; // crop to nearest value
	}
	else if ( !FStringNull( iszMax ) )
	{
		float fMax = 0;
		if ( !TryCalcLocus_Ratio( pLocus, STRING( iszMax ), fMax ) )
			return FALSE;

		if ( fBasis > fMax )
			fBasis = fMax; // crop to nearest value
	}
	return TRUE;
}

enum
{
	CALCRATIO_TRANSFORM_NONE = 0,
	CALCRATIO_TRANSFORM_REVERSED,
	CALCRATIO_TRANSFORM_NEGATIVE,
	CALCRATIO_TRANSFORM_RECIPROCAL,
	CALCRATIO_TRANSFORM_SQUARE,
	CALCRATIO_TRANSFORM_INVERSESQUARE,
	CALCRATIO_TRANSFORM_SQUAREROOT,
	CALCRATIO_TRANSFORM_COSINE,
	CALCRATIO_TRANSFORM_SINE,
	CALCRATIO_TRANSFORM_TANGENT,
	CALCRATIO_TRANSFORM_ACOSINE,
	CALCRATIO_TRANSFORM_ASINE,
	CALCRATIO_TRANSFORM_ATANGENT,
};

enum
{
	CALC_CASTTOINT_NO = 0,
	CALC_CASTTOINT_ROUND = 1,
	CALC_CASTTOINT_FLOOR = 2,
	CALC_CASTTOINT_CEIL = 3,
	CALC_CASTTOINT_TRUNC = 4,
};

static float ApplyCastMode( float f, int castMode )
{
	switch ( castMode )
	{
	case CALC_CASTTOINT_ROUND:	return (float)floor( f + 0.5 );	// halfway away from zero for positives, as round()
	case CALC_CASTTOINT_FLOOR:	return (float)floor( f );
	case CALC_CASTTOINT_CEIL:	return (float)ceil( f );
	case CALC_CASTTOINT_TRUNC:	return f < 0 ? (float)ceil( f ) : (float)floor( f );
	case CALC_CASTTOINT_NO:
	default:
		return f;
	}
}

#ifndef LOCUS_FUZZ

//=============================================
// calc_ratio (Spirit of Half-Life 1.8: calc_numfromnum)
//=============================================

class CCalcRatio : public CPointEntity
{
public:
	BOOL CalcRatio( CBaseEntity *pLocus, float *outResult );

	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t m_iszMin;
	string_t m_iszMax;
	int m_baseCastMode;
	int m_resultCastMode;
};

LINK_ENTITY_TO_CLASS( calc_ratio, CCalcRatio );
LINK_ENTITY_TO_CLASS( calc_numfromnum, CCalcRatio );	// Spirit of Half-Life 1.8

TYPEDESCRIPTION	CCalcRatio::m_SaveData[] =
{
	DEFINE_FIELD( CCalcRatio, m_iszMin, FIELD_STRING ),
	DEFINE_FIELD( CCalcRatio, m_iszMax, FIELD_STRING ),
	DEFINE_FIELD( CCalcRatio, m_baseCastMode, FIELD_INTEGER ),
	DEFINE_FIELD( CCalcRatio, m_resultCastMode, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CCalcRatio, CPointEntity );

void CCalcRatio::Spawn( void )
{
	CPointEntity::Spawn();
	m_iszMin = pev->noise;
	pev->noise = iStringNull;
	m_iszMax = pev->noise1;
	pev->noise1 = iStringNull;
}

void CCalcRatio::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "base_cast_mode" ) )
	{
		m_baseCastMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "result_cast_mode" ) )
	{
		m_resultCastMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

BOOL CCalcRatio::CalcRatio( CBaseEntity *pLocus, float *outResult )
{
	LOCUS_GUARD();

	float fBasis = 0;
	if ( !TryCalcLocus_Ratio( pLocus, STRING( pev->target ), fBasis ) )
		return FALSE;

	fBasis = ApplyCastMode( fBasis, m_baseCastMode );

	switch ( pev->impulse )
	{
	case CALCRATIO_TRANSFORM_REVERSED:		fBasis = 1 - fBasis; break;
	case CALCRATIO_TRANSFORM_NEGATIVE:		fBasis = -fBasis; break;
	case CALCRATIO_TRANSFORM_RECIPROCAL:	fBasis = 1 / fBasis; break;
	case CALCRATIO_TRANSFORM_SQUARE:		fBasis = fBasis * fBasis; break;
	case CALCRATIO_TRANSFORM_INVERSESQUARE:	fBasis = 1 / ( fBasis * fBasis ); break;
	case CALCRATIO_TRANSFORM_SQUAREROOT:	fBasis = sqrt( fBasis ); break;
	case CALCRATIO_TRANSFORM_COSINE:		fBasis = cos( fBasis * ( M_PI / 180.0f ) ); break;
	case CALCRATIO_TRANSFORM_SINE:			fBasis = sin( fBasis * ( M_PI / 180.0f ) ); break;
	case CALCRATIO_TRANSFORM_TANGENT:		fBasis = tan( fBasis * ( M_PI / 180.0f ) ); break;
	case CALCRATIO_TRANSFORM_ACOSINE:		fBasis = acos( fBasis ) * ( 180.0f / M_PI ); break;
	case CALCRATIO_TRANSFORM_ASINE:			fBasis = asin( fBasis ) * ( 180.0f / M_PI ); break;
	case CALCRATIO_TRANSFORM_ATANGENT:		fBasis = atan( fBasis ) * ( 180.0f / M_PI ); break;
	}

	// SBO: 1/0, sqrt(-1), acos(2): no value (Featureful passed inf / NaN on)
	if ( !Locus_Finite( fBasis ) )
		return FALSE;

	if ( !FStringNull( pev->netname ) )
	{
		float fTmp = 0;
		if ( !TryCalcLocus_Ratio( pLocus, STRING( pev->netname ), fTmp ) )
			return FALSE;
		fBasis += fTmp;
	}
	if ( !FStringNull( pev->message ) )
	{
		float fTmp = 1;
		if ( !TryCalcLocus_Ratio( pLocus, STRING( pev->message ), fTmp ) )
			return FALSE;
		fBasis = fBasis * fTmp;
	}

	if ( !ClampToMinMax( pLocus, fBasis, m_iszMin, m_iszMax, (int)pev->frags ) )
		return FALSE;

	fBasis = ApplyCastMode( fBasis, m_resultCastMode );
	if ( !Locus_Finite( fBasis ) )
		return FALSE;
	*outResult = fBasis;
	return TRUE;
}

enum
{
	CALCVECTOR_VELOCITY = 0,
	CALCVECTOR_POSITION = 1,
	CALCVECTOR_COLOR = 2,
};

static BOOL TryCalcLocus_Vector( CBaseEntity *pEntity, CBaseEntity *pLocus, const char *szText, Vector &result, int requestedVectorType )
{
	switch ( requestedVectorType )
	{
	case CALCVECTOR_POSITION:
		return TryCalcLocus_Position( pEntity, pLocus, szText, result );
	case CALCVECTOR_COLOR:
		return TryCalcLocus_Color( pEntity, pLocus, szText, result );
	case CALCVECTOR_VELOCITY:
	default:
		return TryCalcLocus_Velocity( pEntity, pLocus, szText, result );
	}
}

//=============================================
// calc_numfromvec
//=============================================

class CCalcNumFromVec : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd )
	{
		if ( FStrEq( pkvd->szKeyName, "vector_type" ) )
		{
			pev->weapons = atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else
			CPointEntity::KeyValue( pkvd );
	}
	BOOL CalcRatio( CBaseEntity *pLocus, float *outResult );
};

LINK_ENTITY_TO_CLASS( calc_numfromvec, CCalcNumFromVec );

BOOL CCalcNumFromVec::CalcRatio( CBaseEntity *pLocus, float *outResult )
{
	LOCUS_GUARD();

	if ( FStringNull( pev->target ) )
	{
		ALERT( at_console, "No base vector given for %s \"%s\"\n", STRING( pev->classname ), STRING( pev->targetname ) );
		return FALSE;
	}

	Vector vecA;
	if ( !TryCalcLocus_Vector( this, pLocus, STRING( pev->target ), vecA, pev->weapons ) )
	{
		ALERT( at_console, "%s: Couldn't get the vector value from %s\n", STRING( pev->classname ), STRING( pev->target ) );
		return FALSE;
	}

	switch ( pev->impulse )
	{
	case 0: *outResult = vecA.x; return TRUE;
	case 1: *outResult = vecA.y; return TRUE;
	case 2: *outResult = vecA.z; return TRUE;
	case 3: *outResult = vecA.Length(); return TRUE;
	case 4: *outResult = UTIL_VecToAngles( vecA ).x; return TRUE;	// pitch
	case 5: *outResult = UTIL_VecToAngles( vecA ).y; return TRUE;	// yaw
	}

	ALERT( at_console, "%s \"%s\" doesn't understand mode %d\n", STRING( pev->classname ), STRING( pev->targetname ), pev->impulse );
	return FALSE;
}

//=============================================
// calc_vecfromnums
//=============================================

class CCalcVectorFromNums : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd )
	{
		if ( FStrEq( pkvd->szKeyName, "x_value" ) )
		{
			m_xValue = ALLOC_STRING( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else if ( FStrEq( pkvd->szKeyName, "y_value" ) )
		{
			m_yValue = ALLOC_STRING( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else if ( FStrEq( pkvd->szKeyName, "z_value" ) )
		{
			m_zValue = ALLOC_STRING( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else if ( FStrEq( pkvd->szKeyName, "base_vector" ) )
		{
			pev->netname = ALLOC_STRING( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else if ( FStrEq( pkvd->szKeyName, "vector_type" ) )
		{
			pev->weapons = atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else
			CPointEntity::KeyValue( pkvd );
	}

	BOOL CalcPosition( CBaseEntity *pLocus, Vector *outResult ) { return CalcVector( pLocus, outResult ); }
	BOOL CalcVelocity( CBaseEntity *pLocus, Vector *outResult ) { return CalcVector( pLocus, outResult ); }

	BOOL CalcVector( CBaseEntity *pLocus, Vector *outResult )
	{
		LOCUS_GUARD();

		Vector baseVector = g_vecZero;

		if ( !FStringNull( pev->netname ) )
		{
			if ( !TryCalcLocus_Vector( this, pLocus, STRING( pev->netname ), baseVector, pev->weapons ) )
				return FALSE;
		}

		string_t components[3] = { m_xValue, m_yValue, m_zValue };
		for ( int i = 0; i < 3; ++i )
		{
			if ( !FStringNull( components[i] ) )
			{
				float value;
				if ( !TryCalcLocus_Ratio( pLocus, STRING( components[i] ), value ) )
					return FALSE;
				baseVector[i] = value;
			}
		}
		*outResult = baseVector;
		return TRUE;
	}

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t m_xValue;
	string_t m_yValue;
	string_t m_zValue;
};

TYPEDESCRIPTION CCalcVectorFromNums::m_SaveData[] =
{
	DEFINE_FIELD( CCalcVectorFromNums, m_xValue, FIELD_STRING ),
	DEFINE_FIELD( CCalcVectorFromNums, m_yValue, FIELD_STRING ),
	DEFINE_FIELD( CCalcVectorFromNums, m_zValue, FIELD_STRING ),
};

LINK_ENTITY_TO_CLASS( calc_vecfromnums, CCalcVectorFromNums );
IMPLEMENT_SAVERESTORE( CCalcVectorFromNums, CPointEntity );

//=============================================
// calc_subvelocity (Spirit of Half-Life 1.8: calc_vecfroment)
//=============================================

#define SF_CALCVELOCITY_NORMALIZE	1
#define SF_CALCVELOCITY_SWAPZ		2
#define SF_CALCVELOCITY_DISCARDX	4
#define SF_CALCVELOCITY_DISCARDY	8
#define SF_CALCVELOCITY_DISCARDZ	16

class CCalcSubVelocity : public CPointEntity
{
	BOOL Convert( CBaseEntity *pLocus, Vector vecVel, Vector *outVector );
	BOOL ConvertAngles( CBaseEntity *pLocus, Vector vecAngles, Vector *outVector );
public:
	BOOL CalcVelocity( CBaseEntity *pLocus, Vector *outResult );

	void Spawn( void );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t m_iszFactor;
};

LINK_ENTITY_TO_CLASS( calc_subvelocity, CCalcSubVelocity );
LINK_ENTITY_TO_CLASS( calc_vecfroment, CCalcSubVelocity );	// Spirit of Half-Life 1.8

TYPEDESCRIPTION	CCalcSubVelocity::m_SaveData[] =
{
	DEFINE_FIELD( CCalcSubVelocity, m_iszFactor, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCalcSubVelocity, CPointEntity );

void CCalcSubVelocity::Spawn( void )
{
	CPointEntity::Spawn();
	m_iszFactor = pev->noise;
	pev->noise = iStringNull;
}

BOOL CCalcSubVelocity::CalcVelocity( CBaseEntity *pLocus, Vector *outResult )
{
	LOCUS_GUARD();

	// Note: this loses the original pLocus. This is how it's coded in all versions of SoHL
	pLocus = Locus_FindEntity( NULL, STRING( pev->netname ), pLocus );
	if ( !pLocus )
	{
		ALERT( at_console, "%s \"%s\" failed to find target entity \"%s\"\n", STRING( pev->classname ), STRING( pev->targetname ), STRING( pev->netname ) );
		return FALSE;
	}

	Vector vecAngles;
	Vector vecJunk;

	switch ( pev->impulse )
	{
	case 1: // angles
		return ConvertAngles( pLocus, pLocus->pev->angles, outResult );
	case 2: // v_angle
		return ConvertAngles( pLocus, pLocus->pev->v_angle, outResult );
	case 5:
	case 6:
	case 7:
	case 8:
		if ( !Locus_GetAttachment( pLocus, pev->impulse - 5, vecJunk, vecAngles ) )
		{
			ALERT( at_console, "%s \"%s\": \"%s\" has no attachment %d\n", STRING( pev->classname ), STRING( pev->targetname ), STRING( pLocus->pev->classname ), pev->impulse - 4 );
			return FALSE;
		}
		return ConvertAngles( pLocus, vecAngles, outResult );
	case 10:
	{
		if ( pLocus == this )
		{
			ALERT( at_console, "Recursion: %s refers to its own CalcVelocity\n", STRING( pev->targetname ) );
			return FALSE;
		}
		Vector calcedVelocity;
		if ( !pLocus->CalcVelocity( pLocus, &calcedVelocity ) )
			return FALSE;
		return Convert( pLocus, calcedVelocity, outResult );
	}
	default:
		return Convert( pLocus, pLocus->pev->velocity, outResult );
	}
}

BOOL CCalcSubVelocity::Convert( CBaseEntity *pLocus, Vector vecDir, Vector *outVector )
{
	if ( pev->spawnflags & SF_CALCVELOCITY_NORMALIZE )
		vecDir = vecDir.Normalize();

	float fRatio = 1;
	if ( m_iszFactor )
	{
		if ( !TryCalcLocus_Ratio( pLocus, STRING( m_iszFactor ), fRatio ) )
			return FALSE;
	}
	Vector vecOffset = g_vecZero;

	if ( pev->message )
	{
		if ( !TryCalcLocus_Velocity( this, pLocus, STRING( pev->message ), vecOffset ) )
			return FALSE;
	}

	Vector vecResult = vecOffset + ( vecDir * fRatio );

	if ( pev->spawnflags & SF_CALCVELOCITY_DISCARDX )
		vecResult.x = 0;
	if ( pev->spawnflags & SF_CALCVELOCITY_DISCARDY )
		vecResult.y = 0;
	if ( pev->spawnflags & SF_CALCVELOCITY_DISCARDZ )
		vecResult.z = 0;
	if ( pev->spawnflags & SF_CALCVELOCITY_SWAPZ )
		vecResult.z = -vecResult.z;

	*outVector = vecResult;
	return TRUE;
}

BOOL CCalcSubVelocity::ConvertAngles( CBaseEntity *pLocus, Vector vecAngles, Vector *outVector )
{
	UTIL_MakeVectors( vecAngles );
	return Convert( pLocus, gpGlobals->v_forward, outVector );
}

//=============================================
// calc_velocity_path (Spirit of Half-Life 1.8: calc_vecfrompos)
//=============================================

class CCalcVelocityPath : public CPointEntity
{
public:
	BOOL CalcVelocity( CBaseEntity *pLocus, Vector *outVector );

	void Spawn( void );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t m_iszFactor;
};

LINK_ENTITY_TO_CLASS( calc_velocity_path, CCalcVelocityPath );
LINK_ENTITY_TO_CLASS( calc_vecfrompos, CCalcVelocityPath );	// Spirit of Half-Life 1.8

TYPEDESCRIPTION	CCalcVelocityPath::m_SaveData[] =
{
	DEFINE_FIELD( CCalcVelocityPath, m_iszFactor, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCalcVelocityPath, CPointEntity );

void CCalcVelocityPath::Spawn( void )
{
	CPointEntity::Spawn();
	m_iszFactor = pev->noise;
	pev->noise = iStringNull;
}

BOOL CCalcVelocityPath::CalcVelocity( CBaseEntity *pLocus, Vector *outVector )
{
	LOCUS_GUARD();

	Vector vecStart = pev->origin;
	if ( !FStringNull( pev->target ) )
	{
		if ( !TryCalcLocus_Position( this, pLocus, STRING( pev->target ), vecStart ) )
			return FALSE;
	}

	Vector vecOffs = g_vecZero;
	float fFactor = 1;
	if ( m_iszFactor )
	{
		if ( !TryCalcLocus_Ratio( pLocus, STRING( m_iszFactor ), fFactor ) )
			return FALSE;
	}

	switch ( (int)pev->armorvalue )
	{
	case 0:
		if ( !TryCalcLocus_Position( this, pLocus, STRING( pev->netname ), vecOffs ) )
			return FALSE;
		vecOffs = vecOffs - vecStart;
		break;
	case 1:
		if ( !TryCalcLocus_Velocity( this, pLocus, STRING( pev->netname ), vecOffs ) )
			return FALSE;
		break;
	}

	if ( pev->health )
	{
		float len = vecOffs.Length();
		if ( len > 0 )	// SBO: no division by a zero length
		{
			switch ( (int)pev->health )
			{
			case 1: vecOffs = vecOffs / len; break;
			case 2: vecOffs = vecOffs / ( len * len ); break;
			case 3: vecOffs = vecOffs / ( len * len * len ); break;
			case 4: vecOffs = vecOffs * len; break;
			}
		}
	}

	vecOffs = vecOffs * fFactor;

	if ( pev->frags )
	{
		TraceResult tr;
		IGNORE_GLASS iIgnoreGlass = ignore_glass;
		IGNORE_MONSTERS iIgnoreMonsters = ignore_monsters;

		switch ( (int)pev->frags )
		{
		case 2:
			iIgnoreGlass = dont_ignore_glass;
			break;
		case 4:
			iIgnoreGlass = dont_ignore_glass;
			// fall through
		case 3:
			iIgnoreMonsters = dont_ignore_monsters;
			break;
		}

		UTIL_TraceLine( vecStart, vecStart + vecOffs, iIgnoreMonsters, iIgnoreGlass, NULL, &tr );
		vecOffs = tr.vecEndPos - vecStart;
	}

	*outVector = vecOffs;
	return TRUE;
}

//=============================================
// calc_velocity_polar (Spirit of Half-Life 1.8: calc_vecfromvec)
//=============================================

class CCalcVelocityPolar : public CPointEntity
{
public:
	BOOL CalcVelocity( CBaseEntity *pLocus, Vector *outResult );

	void Spawn( void );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t m_iszFactor;
};

LINK_ENTITY_TO_CLASS( calc_velocity_polar, CCalcVelocityPolar );
LINK_ENTITY_TO_CLASS( calc_vecfromvec, CCalcVelocityPolar );	// Spirit of Half-Life 1.8

TYPEDESCRIPTION	CCalcVelocityPolar::m_SaveData[] =
{
	DEFINE_FIELD( CCalcVelocityPolar, m_iszFactor, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCalcVelocityPolar, CPointEntity );

void CCalcVelocityPolar::Spawn( void )
{
	CPointEntity::Spawn();
	m_iszFactor = pev->noise;
	pev->noise = iStringNull;
}

BOOL CCalcVelocityPolar::CalcVelocity( CBaseEntity *pLocus, Vector *outResult )
{
	LOCUS_GUARD();

	Vector vecBasis = g_vecZero;
	TryCalcLocus_Velocity( this, pLocus, STRING( pev->netname ), vecBasis );
	Vector vecAngles = UTIL_VecToAngles( vecBasis ) + pev->angles;
	Vector vecOffset = g_vecZero;
	if ( pev->message )
	{
		if ( !TryCalcLocus_Velocity( this, pLocus, STRING( pev->message ), vecOffset ) )
			return FALSE;
	}

	float fFactor = 1;
	if ( m_iszFactor )
	{
		if ( !TryCalcLocus_Ratio( pLocus, STRING( m_iszFactor ), fFactor ) )
			return FALSE;
	}

	if ( !( pev->spawnflags & SF_CALCVELOCITY_NORMALIZE ) )
		fFactor = fFactor * vecBasis.Length();

	UTIL_MakeVectors( vecAngles );
	*outResult = ( gpGlobals->v_forward * fFactor ) + vecOffset;
	return TRUE;
}

//=============================================
// locus_variable, and the "mark" entities it makes
//=============================================

// Position marker
class CMark : public CPointEntity
{
public:
	BOOL CalcVelocity( CBaseEntity *pLocus, Vector *outVector ) { *outVector = pev->movedir; return TRUE; }
	BOOL CalcRatio( CBaseEntity *pLocus, float *outResult ) { *outResult = pev->frags; return TRUE; }
	void Think( void ) { SUB_Remove(); }
};

class CLocusVariable : public CPointEntity
{
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	RoundReset( void );
	BOOL	CalcVelocity( CBaseEntity *pLocus, Vector *outVector ) { *outVector = pev->movedir; return TRUE; }
	BOOL	CalcRatio( CBaseEntity *pLocus, float *outResult ) { *outResult = pev->frags; return TRUE; }

	void KeyValue( KeyValueData *pkvd );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t m_iszPosition;
	string_t m_iszVelocity;
	string_t m_iszRatio;
	string_t m_iszTargetName;
	string_t m_iszFireOnSpawn;
	float m_fDuration;

	BOOL m_bRoundReset;		// SBO 2.0: "round_reset"
	Vector m_vecSpawnOrigin;
	Vector m_vecSpawnMovedir;
};

TYPEDESCRIPTION	CLocusVariable::m_SaveData[] =
{
	DEFINE_FIELD( CLocusVariable, m_iszPosition, FIELD_STRING ),
	DEFINE_FIELD( CLocusVariable, m_iszVelocity, FIELD_STRING ),
	DEFINE_FIELD( CLocusVariable, m_iszRatio, FIELD_STRING ),
	DEFINE_FIELD( CLocusVariable, m_iszTargetName, FIELD_STRING ),
	DEFINE_FIELD( CLocusVariable, m_iszFireOnSpawn, FIELD_STRING ),
	DEFINE_FIELD( CLocusVariable, m_fDuration, FIELD_FLOAT ),
	DEFINE_FIELD( CLocusVariable, m_bRoundReset, FIELD_BOOLEAN ),
	DEFINE_FIELD( CLocusVariable, m_vecSpawnOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( CLocusVariable, m_vecSpawnMovedir, FIELD_VECTOR ),
};

IMPLEMENT_SAVERESTORE( CLocusVariable, CPointEntity );
LINK_ENTITY_TO_CLASS( locus_variable, CLocusVariable );

void CLocusVariable::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszPosition" ) )
	{
		m_iszPosition = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszVelocity" ) )
	{
		m_iszVelocity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszRatio" ) )
	{
		m_iszRatio = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszTargetName" ) )
	{
		m_iszTargetName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszFireOnSpawn" ) )
	{
		m_iszFireOnSpawn = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_fDuration" ) )
	{
		m_fDuration = atof( pkvd->szValue );
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

void CLocusVariable::Spawn( void )
{
	CPointEntity::Spawn();
	SetMovedir( pev );
	m_vecSpawnOrigin = pev->origin;
	m_vecSpawnMovedir = pev->movedir;
}

// SBO 2.0: "round_reset" 1: the stored position, vector and number back to the map's (number 0)
void CLocusVariable::RoundReset( void )
{
	if ( !m_bRoundReset )
		return;

	UTIL_SetOrigin( pev, m_vecSpawnOrigin );
	pev->movedir = m_vecSpawnMovedir;
	pev->frags = 0;
}

void CLocusVariable::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecPos = g_vecZero;
	Vector vecDir = g_vecZero;
	float fRatio = 0;
	if ( m_iszPosition )
		TryCalcLocus_Position( this, pActivator, STRING( m_iszPosition ), vecPos );
	if ( m_iszVelocity )
		TryCalcLocus_Velocity( this, pActivator, STRING( m_iszVelocity ), vecDir );
	if ( m_iszRatio )
		TryCalcLocus_Ratio( pActivator, STRING( m_iszRatio ), fRatio );

	if ( m_iszTargetName )
	{
		// SBO: a marker is an edict; none close to the limit (the engine stops the server when it runs out)
		if ( NUMBER_OF_ENTITIES() >= gpGlobals->maxEntities - LOCUS_EDICT_MARGIN )
		{
			ALERT( at_console, "locus_variable \"%s\": too many entities, no marker made\n", STRING( pev->targetname ) );
			return;
		}

		CMark *pMark = GetClassPtr( (CMark *)NULL );
		pMark->pev->classname = MAKE_STRING( "mark" );
		pMark->pev->origin = vecPos;
		pMark->pev->movedir = vecDir;
		pMark->pev->frags = fRatio;
		pMark->pev->targetname = m_iszTargetName;
		pMark->pev->nextthink = gpGlobals->time + m_fDuration;

		if ( m_iszFireOnSpawn )
			FireTargets( STRING( m_iszFireOnSpawn ), pMark, this, USE_TOGGLE, 0 );
	}
	else
	{
		UTIL_SetOrigin( pev, vecPos );
		pev->movedir = vecDir;
		pev->frags = fRatio;

		if ( m_iszFireOnSpawn )
			FireTargets( STRING( m_iszFireOnSpawn ), this, this, USE_TOGGLE, 0 );
	}
}

//=============================================
// calc_eval_number
//=============================================

class CCalcEvalNumber : public CPointEntity
{
public:
	enum
	{
		EVAL_ADD = 0,
		EVAL_SUBTRACT = 1,
		EVAL_MULTIPLY = 2,
		EVAL_DIVIDE = 3,
		EVAL_MOD = 4,
		EVAL_MIN = 5,
		EVAL_MAX = 6
	};

	enum
	{
		REPORTED_VECTOR_NONE = 0,
		REPORTED_VECTOR_X00 = 1,
		REPORTED_VECTOR_0Y0 = 2,
		REPORTED_VECTOR_00Z = 3,
	};

	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	BOOL CalcRatio( CBaseEntity *pLocus, float *outResult ) { return CalcEvalNumber( pLocus, *outResult ); }
	BOOL CalcVelocity( CBaseEntity *pLocus, Vector *outResult ) { return ReportVector( pLocus, *outResult ); }
	BOOL CalcPosition( CBaseEntity *pLocus, Vector *outResult ) { return ReportVector( pLocus, *outResult ); }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

protected:
	BOOL CalcEvalNumber( CBaseEntity *pActivator, float &result );
	BOOL DoOperation( float &result, float *operands, int operandCount, int operationId );
	BOOL ReportVector( CBaseEntity *pLocus, Vector &result );

	string_t m_left;
	string_t m_right;
	string_t m_third;
	int m_operation;
	string_t m_storeIn;
	string_t m_triggerOnFail;
	string_t m_iszMin;
	string_t m_iszMax;
	int m_clampPolicy;
	int m_vectorMode;
	int m_firstCastMode;
	int m_secondCastMode;
	int m_thirdCastMode;
	int m_resultCastMode;

	BOOL m_bFiring;		// SBO: Use fires its targets
};

TYPEDESCRIPTION CCalcEvalNumber::m_SaveData[] =
{
	DEFINE_FIELD( CCalcEvalNumber, m_left, FIELD_STRING ),
	DEFINE_FIELD( CCalcEvalNumber, m_right, FIELD_STRING ),
	DEFINE_FIELD( CCalcEvalNumber, m_third, FIELD_STRING ),
	DEFINE_FIELD( CCalcEvalNumber, m_operation, FIELD_INTEGER ),
	DEFINE_FIELD( CCalcEvalNumber, m_storeIn, FIELD_STRING ),
	DEFINE_FIELD( CCalcEvalNumber, m_triggerOnFail, FIELD_STRING ),
	DEFINE_FIELD( CCalcEvalNumber, m_iszMin, FIELD_STRING ),
	DEFINE_FIELD( CCalcEvalNumber, m_iszMax, FIELD_STRING ),
	DEFINE_FIELD( CCalcEvalNumber, m_clampPolicy, FIELD_INTEGER ),
	DEFINE_FIELD( CCalcEvalNumber, m_vectorMode, FIELD_INTEGER ),
	DEFINE_FIELD( CCalcEvalNumber, m_firstCastMode, FIELD_INTEGER ),
	DEFINE_FIELD( CCalcEvalNumber, m_secondCastMode, FIELD_INTEGER ),
	DEFINE_FIELD( CCalcEvalNumber, m_thirdCastMode, FIELD_INTEGER ),
	DEFINE_FIELD( CCalcEvalNumber, m_resultCastMode, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CCalcEvalNumber, CPointEntity );

LINK_ENTITY_TO_CLASS( calc_eval_number, CCalcEvalNumber );

void CCalcEvalNumber::KeyValue( KeyValueData *pkvd )
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
	else if ( FStrEq( pkvd->szKeyName, "third_operand" ) )
	{
		m_third = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "store_result" ) )
	{
		m_storeIn = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "min_value" ) )
	{
		m_iszMin = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "max_value" ) )
	{
		m_iszMax = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "clamp_policy" ) )
	{
		m_clampPolicy = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_on_fail" ) )
	{
		m_triggerOnFail = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "vector_mode" ) )
	{
		m_vectorMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "first_cast_mode" ) )
	{
		m_firstCastMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "second_cast_mode" ) )
	{
		m_secondCastMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "third_cast_mode" ) )
	{
		m_thirdCastMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "result_cast_mode" ) )
	{
		m_resultCastMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CCalcEvalNumber::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SBO: a target that triggers this entity again would recurse until the stack overflows
	if ( m_bFiring )
	{
		ALERT( at_console, "%s \"%s\": triggered by its own target, ignored\n", STRING( pev->classname ), STRING( pev->targetname ) );
		return;
	}

	m_bFiring = TRUE;

	float result;
	if ( CalcEvalNumber( pActivator, result ) )
	{
		if ( m_storeIn )
			FireTargets( STRING( m_storeIn ), pActivator, this, USE_SET, result );
		if ( pev->target )
			FireTargets( STRING( pev->target ), pActivator, this, USE_TOGGLE, 0 );
	}
	else
	{
		if ( m_triggerOnFail )
			FireTargets( STRING( m_triggerOnFail ), pActivator, this, USE_TOGGLE, 0 );
	}

	m_bFiring = FALSE;
}

BOOL CCalcEvalNumber::CalcEvalNumber( CBaseEntity *pActivator, float &result )
{
	LOCUS_GUARD();

	if ( FStringNull( m_left ) || FStringNull( m_right ) )
	{
		ALERT( at_console, "%s \"%s\" needs at least left and right operands defined\n", STRING( pev->classname ), STRING( pev->targetname ) );
		return FALSE;
	}

	float leftValue;
	float rightValue;

	if ( !TryCalcLocus_Ratio( pActivator, STRING( m_left ), leftValue ) )
		return FALSE;
	if ( !TryCalcLocus_Ratio( pActivator, STRING( m_right ), rightValue ) )
		return FALSE;

	leftValue = ApplyCastMode( leftValue, m_firstCastMode );
	rightValue = ApplyCastMode( rightValue, m_secondCastMode );

	int operandCount = 2;

	float thirdValue = 0.0f;
	if ( !FStringNull( m_third ) )
	{
		if ( !TryCalcLocus_Ratio( pActivator, STRING( m_third ), thirdValue ) )
			return FALSE;
		operandCount++;
		thirdValue = ApplyCastMode( thirdValue, m_thirdCastMode );
	}

	float operands[3] = { leftValue, rightValue, thirdValue };

	if ( !DoOperation( result, operands, operandCount, m_operation ) )
		return FALSE;

	if ( !ClampToMinMax( pActivator, result, m_iszMin, m_iszMax, m_clampPolicy ) )
		return FALSE;

	result = ApplyCastMode( result, m_resultCastMode );
	return Locus_Finite( result );	// SBO: an overflow is a failure
}

BOOL CCalcEvalNumber::DoOperation( float &result, float *operands, int operandCount, int operationId )
{
	if ( operandCount <= 0 )
		return FALSE;
	float value = operands[0];
	for ( int i = 1; i < operandCount; ++i )
	{
		float operand = operands[i];
		switch ( operationId )
		{
		case EVAL_ADD:
			value += operand;
			break;
		case EVAL_SUBTRACT:
			value -= operand;
			break;
		case EVAL_MULTIPLY:
			value *= operand;
			break;
		case EVAL_DIVIDE:
			if ( operand == 0.0f )
				return FALSE;
			value /= operand;
			break;
		case EVAL_MOD:
			if ( operand == 0.0f )
				return FALSE;
			value = fmodf( value, operand );
			break;
		case EVAL_MIN:
			value = value < operand ? value : operand;
			break;
		case EVAL_MAX:
			value = value > operand ? value : operand;
			break;
		default:
			ALERT( at_console, "%s: unknown operation id %d\n", STRING( pev->classname ), operationId );
			return FALSE;
		}
	}
	if ( !Locus_Finite( value ) )
		return FALSE;
	result = value;
	return TRUE;
}

BOOL CCalcEvalNumber::ReportVector( CBaseEntity *pLocus, Vector &result )
{
	float f;
	if ( !CalcEvalNumber( pLocus, f ) )
		return FALSE;

	switch ( m_vectorMode )
	{
	case REPORTED_VECTOR_X00:
		result = Vector( f, 0.0f, 0.0f );
		return TRUE;
	case REPORTED_VECTOR_0Y0:
		result = Vector( 0.0f, f, 0.0f );
		return TRUE;
	case REPORTED_VECTOR_00Z:
		result = Vector( 0.0f, 0.0f, f );
		return TRUE;
	case REPORTED_VECTOR_NONE:
	default:
		return FALSE;
	}
}

//=============================================
// game_number (Featureful dlls/maprules.cpp): a number variable. On / Toggle +1, Off -1, Set: the value.
//=============================================

class CGameNumber : public CPointEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void RoundReset( void );
	BOOL CalcRatio( CBaseEntity *pLocus, float *outResult ) { *outResult = pev->frags; return TRUE; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL m_bRoundReset;		// SBO 2.0: "round_reset"
	float m_flInitial;		// the map's value
};

LINK_ENTITY_TO_CLASS( game_number, CGameNumber );

TYPEDESCRIPTION CGameNumber::m_SaveData[] =
{
	DEFINE_FIELD( CGameNumber, m_bRoundReset, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGameNumber, m_flInitial, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CGameNumber, CPointEntity );

void CGameNumber::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "round_reset" ) )	// SBO 2.0 (round_hooks.h)
	{
		m_bRoundReset = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CGameNumber::Spawn( void )
{
	CPointEntity::Spawn();
	if ( !Locus_Finite( pev->frags ) )
		pev->frags = 0;
	m_flInitial = pev->frags;	// "frags": Initial Value
}

// SBO 2.0: "round_reset" 1: back to the Initial Value
void CGameNumber::RoundReset( void )
{
	if ( m_bRoundReset )
		pev->frags = m_flInitial;
}

void CGameNumber::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	switch ( useType )
	{
	case USE_ON:
	case USE_TOGGLE:
		pev->frags++;
		break;
	case USE_OFF:
		pev->frags--;
		break;
	case USE_SET:
		if ( Locus_Finite( value ) )	// SBO
			pev->frags = value;
		break;
	}
}

//=============================================
// trigger_compare (Featureful dlls/triggers.cpp): compares two [LR] numbers; also a master
//=============================================

enum
{
	COMPARISON_TEST_EQUAL = 0,
	COMPARISON_TEST_GTE,
	COMPARISON_TEST_GREATER,
	COMPARISON_TEST_LTE,
	COMPARISON_TEST_LESS,
	COMPARISON_TEST_NOT_EQUAL,
};

class CTriggerCompare : public CPointEntity
{
public:
	int ObjectCaps( void ) { return CPointEntity::ObjectCaps() | FCAP_MASTER; }
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	BOOL IsTriggered( CBaseEntity *pActivator );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t m_onComparisonPass;
	string_t m_onComparisonFail;

	string_t m_onEqual;
	string_t m_onNotEqual;
	string_t m_onLessThan;
	string_t m_onGreaterThan;

	string_t m_onFail;
	float m_value;
	float m_compareValue;
	string_t m_valueSource;
	string_t m_compareValueSource;
	int m_valueIsNumber;
	int m_compareValueIsNumber;

	BOOL m_bFiring;		// SBO: Use fires its targets

private:
	float CalcValue( float numValue, string_t valueSource, int valueIsNumber, BOOL &success, CBaseEntity *pActivator )
	{
		if ( valueIsNumber )
		{
			success = TRUE;
			return numValue;
		}
		float result = 0;
		success = !FStringNull( valueSource ) && TryCalcLocus_Ratio( pActivator, STRING( valueSource ), result );
		return result;
	}
	int DoCompareValues( float a, float b )
	{
		if ( a == b )
			return 0;
		return a < b ? -1 : 1;
	}
	BOOL IsComparisonPassed( int compareResult )
	{
		switch ( pev->impulse )
		{
		case COMPARISON_TEST_EQUAL:		return compareResult == 0;
		case COMPARISON_TEST_GREATER:	return compareResult > 0;
		case COMPARISON_TEST_GTE:		return compareResult >= 0;
		case COMPARISON_TEST_LESS:		return compareResult < 0;
		case COMPARISON_TEST_LTE:		return compareResult <= 0;
		case COMPARISON_TEST_NOT_EQUAL:	return compareResult != 0;
		default:
			ALERT( at_console, "Unknown test type in %s: %d\n", STRING( pev->classname ), pev->impulse );
			return FALSE;
		}
	}
};

LINK_ENTITY_TO_CLASS( trigger_compare, CTriggerCompare );

TYPEDESCRIPTION	CTriggerCompare::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerCompare, m_onComparisonPass, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCompare, m_onComparisonFail, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCompare, m_onEqual, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCompare, m_onNotEqual, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCompare, m_onLessThan, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCompare, m_onGreaterThan, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCompare, m_onFail, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCompare, m_value, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCompare, m_compareValue, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCompare, m_valueSource, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCompare, m_compareValueSource, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCompare, m_valueIsNumber, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerCompare, m_compareValueIsNumber, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CTriggerCompare, CPointEntity );

void CTriggerCompare::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "testtype" ) )
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "pass_target" ) )
	{
		m_onComparisonPass = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fail_target" ) )
	{
		m_onComparisonFail = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_on_equal" ) )
	{
		m_onEqual = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_on_not_equal" ) )
	{
		m_onNotEqual = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_on_less" ) )
	{
		m_onLessThan = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_on_greater" ) )
	{
		m_onGreaterThan = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "trigger_on_fail" ) )
	{
		m_onFail = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "initial_value" ) )
	{
		if ( Locus_IsLikelyNumber( pkvd->szValue ) )
		{
			m_value = atof( pkvd->szValue );
			m_valueIsNumber = 1;
		}
		else
		{
			m_valueSource = ALLOC_STRING( pkvd->szValue );
			m_valueIsNumber = 0;
		}
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "compare_value" ) )
	{
		if ( Locus_IsLikelyNumber( pkvd->szValue ) )
		{
			m_compareValue = atof( pkvd->szValue );
			m_compareValueIsNumber = 1;
		}
		else
		{
			m_compareValueSource = ALLOC_STRING( pkvd->szValue );
			m_compareValueIsNumber = 0;
		}
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTriggerCompare::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_bFiring )
	{
		ALERT( at_console, "%s \"%s\": triggered by its own target, ignored\n", STRING( pev->classname ), STRING( pev->targetname ) );
		return;
	}

	BOOL valueSuccess;
	const float baseValue = CalcValue( m_value, m_valueSource, m_valueIsNumber, valueSuccess, pActivator );
	BOOL compareValueSuccess;
	const float compareValue = CalcValue( m_compareValue, m_compareValueSource, m_compareValueIsNumber, compareValueSuccess, pActivator );

	m_bFiring = TRUE;

	if ( !valueSuccess || !compareValueSuccess )
	{
		if ( m_onFail )
			FireTargets( STRING( m_onFail ), pActivator, this, USE_TOGGLE, 0 );
		m_bFiring = FALSE;
		return;
	}

	const int compareResult = DoCompareValues( baseValue, compareValue );

	if ( m_onComparisonPass || m_onComparisonFail )
	{
		BOOL passed = IsComparisonPassed( compareResult );
		if ( passed && m_onComparisonPass )
			FireTargets( STRING( m_onComparisonPass ), pActivator, this, USE_TOGGLE, 0 );
		if ( !passed && m_onComparisonFail )
			FireTargets( STRING( m_onComparisonFail ), pActivator, this, USE_TOGGLE, 0 );
	}

	if ( compareResult == 0 && m_onEqual )
		FireTargets( STRING( m_onEqual ), pActivator, this, USE_TOGGLE, 0 );
	if ( compareResult != 0 && m_onNotEqual )
		FireTargets( STRING( m_onNotEqual ), pActivator, this, USE_TOGGLE, 0 );
	if ( compareResult < 0 && m_onLessThan )
		FireTargets( STRING( m_onLessThan ), pActivator, this, USE_TOGGLE, 0 );
	if ( compareResult > 0 && m_onGreaterThan )
		FireTargets( STRING( m_onGreaterThan ), pActivator, this, USE_TOGGLE, 0 );

	m_bFiring = FALSE;
}

BOOL CTriggerCompare::IsTriggered( CBaseEntity *pActivator )
{
	BOOL valueSuccess;
	const float baseValue = CalcValue( m_value, m_valueSource, m_valueIsNumber, valueSuccess, pActivator );
	BOOL compareValueSuccess;
	const float compareValue = CalcValue( m_compareValue, m_compareValueSource, m_compareValueIsNumber, compareValueSuccess, pActivator );

	if ( !valueSuccess || !compareValueSuccess )
		return FALSE;

	return IsComparisonPassed( DoCompareValues( baseValue, compareValue ) );
}

#endif // LOCUS_FUZZ
