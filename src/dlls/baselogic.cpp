//=========================================================
// CBaseLogic: see baselogic.h. Ported from SevenKewp (Half-Life 1 SDK license).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"baselogic.h"

CBaseEntity *CBaseLogic::FindLogicEntity( CBaseEntity *pStart, string_t targetname )
{
	if ( FStringNull( targetname ) )
		return NULL;

	if ( !strcmp( STRING( targetname ), "!activator" ) )
	{
		if ( pStart )
			return NULL;	// prevent infinite loops

		CBaseEntity *pActivator = m_hActivator;
		if ( !pActivator )
		{
			ALERT( at_console, "%s \"%s\": failed to find !activator. Using worldspawn instead.\n",
				STRING( pev->classname ), STRING( pev->targetname ) );
			return CBaseEntity::Instance( ENT( 0 ) );
		}

		return pActivator;
	}

	if ( !strcmp( STRING( targetname ), "!caller" ) )
	{
		if ( pStart )
			return NULL;	// prevent infinite loops

		CBaseEntity *pCaller = m_hCaller;
		if ( !pCaller )
		{
			ALERT( at_console, "%s \"%s\": failed to find !caller. Using worldspawn instead.\n",
				STRING( pev->classname ), STRING( pev->targetname ) );
			return CBaseEntity::Instance( ENT( 0 ) );
		}

		return pCaller;
	}

	return UTIL_FindEntityByTargetname( pStart, STRING( targetname ) );
}

void CBaseLogic::FireLogicTargets( const char *targetName, USE_TYPE useType, float value )
{
	if ( !targetName || !targetName[0] )
		return;

	ALERT( at_aiconsole, "Firing: (%s)\n", targetName );

	CBaseEntity *pActivator = m_hActivator;
	CBaseEntity *pCaller = m_hCaller;

	if ( !strcmp( targetName, "!activator" ) )
	{
		if ( pActivator )
			pActivator->Use( pActivator, this, useType, value );
	}
	else if ( !strcmp( targetName, "!caller" ) )
	{
		if ( pCaller )
			pCaller->Use( pActivator, this, useType, value );
	}
	else
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByTargetname( pEntity, targetName ) ) != NULL )
		{
			if ( pEntity != this && !( pEntity->pev->flags & FL_KILLME ) )
				pEntity->Use( pActivator, this, useType, value );
		}
	}
}

int CBaseLogic::FloatToInteger( float f, int flt2str_mode )
{
	switch ( flt2str_mode )
	{
	case FLT2STR_ROUND:	return (int)( f + 0.5f );
	case FLT2STR_CEIL:	return (int)ceil( f );
	case FLT2STR_FLOOR:
	default:
		return (int)f;
	}
}

float CBaseLogic::VectorToFloat( Vector v )
{
	int dont_use_coords = GetVectorDontUseFlags();

	if ( dont_use_coords == ( SF_LOGIC_DONT_USE_Y | SF_LOGIC_DONT_USE_Z ) )
		return v.x;
	if ( dont_use_coords == ( SF_LOGIC_DONT_USE_X | SF_LOGIC_DONT_USE_Z ) )
		return v.y;
	if ( dont_use_coords == ( SF_LOGIC_DONT_USE_X | SF_LOGIC_DONT_USE_Y ) )
		return v.z;

	if ( dont_use_coords & SF_LOGIC_DONT_USE_X )
		v.x = 0;
	if ( dont_use_coords & SF_LOGIC_DONT_USE_Y )
		v.y = 0;
	if ( dont_use_coords & SF_LOGIC_DONT_USE_Z )
		v.z = 0;

	return v.Length();
}

void CBaseLogic::FloatToString( float f, int flt2str_mode, char *out )
{
	switch ( flt2str_mode )
	{
	case FLT2STR_5DP:	snprintf( out, LOGIC_STRING_LEN, "%.5f", f ); break;
	case FLT2STR_4DP:	snprintf( out, LOGIC_STRING_LEN, "%.4f", f ); break;
	case FLT2STR_3DP:	snprintf( out, LOGIC_STRING_LEN, "%.3f", f ); break;
	case FLT2STR_2DP:	snprintf( out, LOGIC_STRING_LEN, "%.2f", f ); break;
	case FLT2STR_1DP:	snprintf( out, LOGIC_STRING_LEN, "%.1f", f ); break;
	case FLT2STR_ROUND:	snprintf( out, LOGIC_STRING_LEN, "%d", (int)( f + 0.5f ) ); break;
	case FLT2STR_CEIL:	snprintf( out, LOGIC_STRING_LEN, "%d", (int)ceil( f ) ); break;
	case FLT2STR_FLOOR:	snprintf( out, LOGIC_STRING_LEN, "%d", (int)f ); break;
	case FLT2STR_6DP:
	default:
		snprintf( out, LOGIC_STRING_LEN, "%f", f );
		break;
	}
}

void CBaseLogic::VectorToString( Vector v, int flt2str_mode, char *out )
{
	int dont_use_coords = GetVectorDontUseFlags();
	char part[ LOGIC_STRING_LEN ];

	out[0] = '\0';

	for ( int i = 0; i < 3; i++ )
	{
		if ( dont_use_coords & ( SF_LOGIC_DONT_USE_X << i ) )
			continue;

		FloatToString( v[i], flt2str_mode, part );

		if ( out[0] )
			strncat( out, " ", LOGIC_STRING_LEN - strlen( out ) - 1 );
		strncat( out, part, LOGIC_STRING_LEN - strlen( out ) - 1 );
	}
}
