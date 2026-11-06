/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== lights.cpp ========================================================

  spawn and think functions for editor-placed lights

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "locus.h"	// SBO 2.0: trigger_lightstyle targets (!activator)

// SBO 2.0: light styles while the map runs (trigger_lightstyle, global_light_control; docs/LIGHT_STYLES.md).
// The engine keeps a pointer to each style's pattern (and sends it to every client that connects later), so every
// pattern given to LIGHT_STYLE here is a string that lives for the whole map.
#define LIGHT_MAX_STYLES	64		// the engine's MAX_LIGHTSTYLES: LIGHT_STYLE outside 0..63 writes past its table
#define LIGHT_MAX_PATTERN	63		// the clients' pattern buffer (MAX_STYLESTRING 64)

// One letter per brightness: the steps of a fade
static const char *s_szLetters[26] = { "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
	"p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z" };

// the patterns the world sets at the map start (CWorld::Precache, world.cpp): styles 0-12 and 63
static const char *Lights_WorldStyle( int iStyle )
{
	switch ( iStyle )
	{
	case 0:		return "m";
	case 1:		return "mmnmmommommnonmmonqnmmo";
	case 2:		return "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba";
	case 3:		return "mmmmmaaaaammmmmaaaaaabcdefgabcdefg";
	case 4:		return "mamamamamama";
	case 5:		return "jklmnopqrstuvwxyzyxwvutsrqponmlkj";
	case 6:		return "nmonqnmomnmomomno";
	case 7:		return "mmmaaaabcdefgmmmmaaaammmaamm";
	case 8:		return "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa";
	case 9:		return "aaaaaaaazzzzzzzz";
	case 10:	return "mmamammmmammamamaaamammma";
	case 11:	return "abcdefghijklmnopqrrqponmlkjihgfedcba";
	case 12:	return "mmnnmmnnnmmnn";
	case 63:	return "a";
	default:	return NULL;
	}
}

// Spirit of Half-Life's "Appearance" choices (halflife-updated-spirit dlls/lights.cpp GetStdLightStyle, Half-Life 1
// SDK license): 0-12 Half-Life's styles, 13 off, 14-16 fade in, 17-19 fade out
static const char *Lights_StdStyle( int iStyle )
{
	switch ( iStyle )
	{
	case 13:	return "a";
	case 14:	return "aabbccddeeffgghhiijjkkllmmmmmmmmmmmmmm";
	case 15:	return "abcdefghijklmmmmmmmmmmmmmmmmmmmmmmmmmm";
	case 16:	return "acegikmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm";
	case 17:	return "llkkjjiihhggffeeddccbbaaaaaaaaaaaaaaaa";
	case 18:	return "lkjihgfedcbaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	case 19:	return "kigecaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	default:
	{
		const char *p = ( iStyle >= 0 && iStyle <= 12 ) ? Lights_WorldStyle( iStyle ) : NULL;
		return p ? p : "m";
	}
	}
}

// SBO: a mapper's pattern made safe for the engine and the clients: letters a-z only (others are dropped, capitals
// lowered), at most LIGHT_MAX_PATTERN; empty: "m". A string that lives for the map.
static string_t Lights_CleanPattern( const char *szPattern )
{
	char buf[ LIGHT_MAX_PATTERN + 1 ];
	int n = 0;

	for ( const char *p = szPattern; p && *p && n < LIGHT_MAX_PATTERN; p++ )
	{
		char c = *p;
		if ( c >= 'A' && c <= 'Z' )
			c = c - 'A' + 'a';
		if ( c >= 'a' && c <= 'z' )
			buf[ n++ ] = c;
	}
	buf[n] = '\0';
	return ALLOC_STRING( n ? buf : "m" );
}

static void Lights_SetStyle( int iStyle, const char *szPattern )
{
	if ( iStyle < 0 || iStyle >= LIGHT_MAX_STYLES || !szPattern )
		return;
	LIGHT_STYLE( iStyle, (char *)szPattern );
}

class CLight : public CPointEntity
{
public:
	virtual void	KeyValue( KeyValueData* pkvd ); 
	virtual void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	// SBO 2.0, from Spirit of Half-Life (halflife-updated-spirit dlls/lights.cpp CLight / CLightFader): what
	// trigger_lightstyle and global_light_control change. The fade runs on the light itself (Spirit made a
	// "lightfader" entity per fade).
	int		Style( void ) { return m_iStyle; }
	BOOL	Switchable( void ) { return m_iStyle >= 32 && m_iStyle < LIGHT_MAX_STYLES; }
	void	SetStyle( const char *szPattern );		// the pattern now (a string that lives for the map)
	const char *GetStyle( void ) { return m_pszCurrent ? m_pszCurrent : "m"; }
	void	SetCorrectStyle( void );				// back to its own: its pattern when on, "a" when off
	void	StartFade( const char *szTo, float flTime, int iWait );
	void	StartWait( int iWait );
	void	EXPORT FadeThink( void );
	void	EXPORT WaitThink( void );

private:
	int		m_iStyle;
	int		m_iszPattern;

	const char *m_pszCurrent;	// the pattern this light's style has now
	const char *m_pszFadeTo;	// the pattern at the end of the fade
	int		m_cFadeFrom, m_cFadeTo;
	float	m_flFadeStart, m_flFadeEnd;
	int		m_iWait;			// seconds after the fade before the light's own style comes back (-1: never)
};
LINK_ENTITY_TO_CLASS( light, CLight );

TYPEDESCRIPTION	CLight::m_SaveData[] = 
{
	DEFINE_FIELD( CLight, m_iStyle, FIELD_INTEGER ),
	DEFINE_FIELD( CLight, m_iszPattern, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CLight, CPointEntity );


//
// Cache user-entity-field values until spawn is called.
//
void CLight :: KeyValue( KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "style"))
	{
		m_iStyle = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitch"))
	{
		pev->angles.x = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pattern"))
	{
		m_iszPattern = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CPointEntity::KeyValue( pkvd );
	}
}

void CLight::SetStyle( const char *szPattern )
{
	if ( !Switchable() || !szPattern )
		return;
	m_pszCurrent = szPattern;
	Lights_SetStyle( m_iStyle, szPattern );
}

void CLight::SetCorrectStyle( void )
{
	SetThink( NULL );
	pev->nextthink = 0;

	if ( FBitSet( pev->spawnflags, SF_LIGHT_START_OFF ) )
		SetStyle( "a" );
	else if ( m_iszPattern )
		SetStyle( STRING( m_iszPattern ) );
	else
		SetStyle( "m" );
}

// Spirit's lightfader: from the first letter of the current pattern to the first letter of the new one, a step
// every 0.1 s, then the new pattern
void CLight::StartFade( const char *szTo, float flTime, int iWait )
{
	if ( !Switchable() )
		return;

	m_pszFadeTo = szTo;
	m_cFadeFrom = GetStyle()[0];
	m_cFadeTo = szTo[0];
	if ( m_cFadeFrom < 'a' || m_cFadeFrom > 'z' )
		m_cFadeFrom = 'm';
	if ( m_cFadeTo < 'a' || m_cFadeTo > 'z' )
		m_cFadeTo = 'm';
	m_flFadeStart = gpGlobals->time;
	m_flFadeEnd = gpGlobals->time + flTime;
	m_iWait = iWait;
	SetThink( &CLight::FadeThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CLight::StartWait( int iWait )
{
	if ( iWait < 0 )
	{
		SetThink( NULL );
		pev->nextthink = 0;
		return;
	}
	SetThink( &CLight::WaitThink );
	pev->nextthink = gpGlobals->time + iWait;
}

void CLight::FadeThink( void )
{
	if ( m_flFadeEnd > gpGlobals->time && m_flFadeEnd > m_flFadeStart )
	{
		float t = ( gpGlobals->time - m_flFadeStart ) / ( m_flFadeEnd - m_flFadeStart );
		int c = m_cFadeFrom + (int)( ( m_cFadeTo - m_cFadeFrom ) * t );
		if ( c < 'a' ) c = 'a';
		if ( c > 'z' ) c = 'z';
		SetStyle( s_szLetters[ c - 'a' ] );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	// done: the new pattern, then (Hold time) the light's own again
	SetStyle( m_pszFadeTo );
	StartWait( m_iWait );
}

void CLight::WaitThink( void )
{
	SetCorrectStyle();
}

/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) LIGHT_START_OFF
Non-displayed light.
Default light value is 300
Default style is 0
If targeted, it will toggle between on or off.
*/

void CLight :: Spawn( void )
{
	if (FStringNull(pev->targetname))
	{       // inert light
		REMOVE_ENTITY(ENT(pev));
		return;
	}
	
	// SBO 2.0: a style outside the engine's table (a broken map) would write past it
	if (m_iStyle >= 32 && m_iStyle < LIGHT_MAX_STYLES)
	{
//		CHANGE_METHOD(ENT(pev), em_use, light_use);
		if (FBitSet(pev->spawnflags, SF_LIGHT_START_OFF))
			m_pszCurrent = "a";
		else if (m_iszPattern)
			m_pszCurrent = STRING( m_iszPattern );
		else
			m_pszCurrent = "m";
		LIGHT_STYLE(m_iStyle, (char *)m_pszCurrent);
	}
}


void CLight :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (m_iStyle >= 32 && m_iStyle < LIGHT_MAX_STYLES)	// SBO 2.0: see Spawn
	{
		if ( !ShouldToggle( useType, !FBitSet(pev->spawnflags, SF_LIGHT_START_OFF) ) )
			return;

		// SBO 2.0: a trigger_lightstyle fade or hold on it ends
		SetThink( NULL );
		pev->nextthink = 0;

		if (FBitSet(pev->spawnflags, SF_LIGHT_START_OFF))
		{
			if (m_iszPattern)
				m_pszCurrent = STRING( m_iszPattern );
			else
				m_pszCurrent = "m";
			LIGHT_STYLE(m_iStyle, (char *)m_pszCurrent);
			ClearBits(pev->spawnflags, SF_LIGHT_START_OFF);
		}
		else
		{
			m_pszCurrent = "a";
			LIGHT_STYLE(m_iStyle, (char *)m_pszCurrent);
			SetBits(pev->spawnflags, SF_LIGHT_START_OFF);
		}
	}
}

//
// shut up spawn functions for new spotlights
//
LINK_ENTITY_TO_CLASS( light_spot, CLight );


class CEnvLight : public CLight
{
public:
	void	KeyValue( KeyValueData* pkvd ); 
	void	Spawn( void );
};

LINK_ENTITY_TO_CLASS( light_environment, CEnvLight );

void CEnvLight::KeyValue( KeyValueData* pkvd )
{
	if (FStrEq(pkvd->szKeyName, "_light"))
	{
		int r = 0, g = 0, b = 0, v = 0, j;	// halflife-updated: defined values if sscanf reads fewer fields
		char szColor[64];
		j = sscanf( pkvd->szValue, "%d %d %d %d\n", &r, &g, &b, &v );
		if (j == 1)
		{
			g = b = r;
		}
		else if (j == 4)
		{
			r = r * (v / 255.0);
			g = g * (v / 255.0);
			b = b * (v / 255.0);
		}

		// simulate qrad direct, ambient,and gamma adjustments, as well as engine scaling
		r = pow( r / 114.0, 0.6 ) * 264;
		g = pow( g / 114.0, 0.6 ) * 264;
		b = pow( b / 114.0, 0.6 ) * 264;

		pkvd->fHandled = TRUE;
		sprintf( szColor, "%d", r );
		CVAR_SET_STRING( "sv_skycolor_r", szColor );
		sprintf( szColor, "%d", g );
		CVAR_SET_STRING( "sv_skycolor_g", szColor );
		sprintf( szColor, "%d", b );
		CVAR_SET_STRING( "sv_skycolor_b", szColor );
	}
	else
	{
		CLight::KeyValue( pkvd );
	}
}


void CEnvLight :: Spawn( void )
{
	char szVector[64];
	UTIL_MakeAimVectors( pev->angles );

	sprintf( szVector, "%f", gpGlobals->v_forward.x );
	CVAR_SET_STRING( "sv_skyvec_x", szVector );
	sprintf( szVector, "%f", gpGlobals->v_forward.y );
	CVAR_SET_STRING( "sv_skyvec_y", szVector );
	sprintf( szVector, "%f", gpGlobals->v_forward.z );
	CVAR_SET_STRING( "sv_skyvec_z", szVector );

	CLight::Spawn( );
}


//=========================================================
// SBO 2.0 light styles while the map runs (docs/LIGHT_STYLES.md). The engine sends every style's current pattern to
// a client that connects later (with the signon data), so late joiners and spectators see the current lighting.
//=========================================================

static CLight *Lights_AsLight( CBaseEntity *pEntity )
{
	if ( pEntity && ( FClassnameIs( pEntity->pev, "light" ) || FClassnameIs( pEntity->pev, "light_spot" )
		|| FClassnameIs( pEntity->pev, "light_environment" ) ) )
		return (CLight *)pEntity;
	return NULL;
}

// "round_reset": every style back to its map-load pattern: the world's styles (0-12, 63) and each named light's own
// (on or off as it is now); fades and holds end
static float s_flLightsRestored = -1;

static void Lights_RestoreAll( void )
{
	if ( s_flLightsRestored == gpGlobals->time )
		return;	// once per round start, however many entities ask
	s_flLightsRestored = gpGlobals->time;

	for ( int i = 0; i < LIGHT_MAX_STYLES; i++ )
	{
		const char *p = Lights_WorldStyle( i );
		if ( p )
			Lights_SetStyle( i, p );
	}

	static const char *classes[] = { "light", "light_spot", "light_environment" };
	for ( int k = 0; k < 3; k++ )
	{
		CBaseEntity *pEnt = NULL;
		while ( ( pEnt = UTIL_FindEntityByClassname( pEnt, classes[k] ) ) != NULL )
		{
			CLight *pLight = (CLight *)pEnt;
			if ( pLight->Switchable() )
				pLight->SetCorrectStyle();
		}
	}

	// the map start's own changes again: every global_light_control without a name applied itself at spawn
	CBaseEntity *pCtl = NULL;
	while ( ( pCtl = UTIL_FindEntityByClassname( pCtl, "global_light_control" ) ) != NULL )
	{
		if ( FStringNull( pCtl->pev->targetname ) )
			pCtl->Use( pCtl, pCtl, USE_TOGGLE, 0 );
	}
}

//=========================================================
// trigger_lightstyle -- gives its target lights another appearance, optionally fading to it, for a time or for good
//
// Spirit of Half-Life's entity (halflife-updated-spirit dlls/lights.cpp CTriggerLightstyle and CLightFader, Half-Life 1
// SDK license): keys target, appearance (Spirit: "style", renamed: docs/COMPILER_BUGS.md), pattern, m_iFade,
// m_iWait. SBO: the fade runs on the light (no "lightfader" entity), only named lights (switchable styles 32-63) are
// changed (Spirit also changed other entities' styles), the pattern is cleaned (letters a-z, at most 63), "round_reset".
//=========================================================

class CTriggerLightstyle : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void RoundReset( void ) { if ( m_bRoundReset ) Lights_RestoreAll(); }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int		m_iStyleChoice;
	string_t m_iszPattern;
	int		m_iFade;
	int		m_iWait;
	BOOL	m_bRoundReset;
};

LINK_ENTITY_TO_CLASS( trigger_lightstyle, CTriggerLightstyle );

TYPEDESCRIPTION CTriggerLightstyle::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerLightstyle, m_iStyleChoice, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerLightstyle, m_iszPattern, FIELD_STRING ),
	DEFINE_FIELD( CTriggerLightstyle, m_iFade, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerLightstyle, m_iWait, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerLightstyle, m_bRoundReset, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CTriggerLightstyle, CPointEntity );

void CTriggerLightstyle::KeyValue( KeyValueData *pkvd )
{
	// "appearance", not Spirit's "style": sdHLCSG (and other compilers with Spirit's change) give every named
	// non-light entity with a nonzero "style" a switchable light style (32-63), which replaced the choice
	if ( FStrEq( pkvd->szKeyName, "appearance" ) )
	{
		m_iStyleChoice = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "pattern" ) )
	{
		m_iszPattern = pkvd->szValue[0] ? Lights_CleanPattern( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iFade" ) )
	{
		m_iFade = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iWait" ) )
	{
		m_iWait = atoi( pkvd->szValue );
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

void CTriggerLightstyle::Spawn( void )
{
	CPointEntity::Spawn();
	if ( m_iFade < 0 )
		m_iFade = 0;
}

void CTriggerLightstyle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FStringNull( pev->target ) )
		return;

	const char *szPattern = m_iszPattern ? STRING( m_iszPattern ) : Lights_StdStyle( m_iStyleChoice );

	CBaseEntity *pTarget = NULL;
	while ( ( pTarget = Locus_FindEntity( pTarget, STRING( pev->target ), pActivator ) ) != NULL )
	{
		CLight *pLight = Lights_AsLight( pTarget );
		if ( !pLight || !pLight->Switchable() )
		{
			ALERT( at_console, "trigger_lightstyle \"%s\": \"%s\" is not a named light\n", STRING( pev->targetname ), STRING( pTarget->pev->classname ) );
			continue;
		}

		if ( m_iFade > 0 )
			pLight->StartFade( szPattern, (float)m_iFade, m_iWait );
		else
		{
			pLight->SetStyle( szPattern );
			pLight->StartWait( m_iWait );
		}
	}
}

//=========================================================
// global_light_control -- sets a light pattern on the map's default style (0: every light without a style) and/or
// all custom styles, or on the style of named lights
//
// Sven Co-op's entity as SevenKewp has it (dlls/triggers/CGlobalLightControl.cpp, Half-Life 1 SDK license): keys
// pattern, target_entity, flags 65536 also custom styles, 131072 not the default style; without a name it applies
// at the map start. SBO: the pattern is cleaned (letters a-z, at most 63); "round_reset".
//=========================================================

#define SF_GLIT_AFFECT_CUSTOM_STYLES	65536	// apply to all lights that have a custom style
#define SF_GLIT_DONT_AFFECT_DEFAULT		131072	// don't apply to non-styled lights

class CGlobalLightControl : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void RoundReset( void ) { if ( m_bRoundReset ) Lights_RestoreAll(); }

	string_t m_pattern;
	string_t m_targetEnt;
	BOOL m_bRoundReset;
};

LINK_ENTITY_TO_CLASS( global_light_control, CGlobalLightControl );

void CGlobalLightControl::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "pattern" ) )
	{
		m_pattern = Lights_CleanPattern( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "target_entity" ) )
	{
		m_targetEnt = ALLOC_STRING( pkvd->szValue );
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

void CGlobalLightControl::Spawn( void )
{
	CPointEntity::Spawn();

	// start on if it can't be toggled
	if ( !pev->targetname )
		Use( this, this, USE_TOGGLE, 0.0f );
}

void CGlobalLightControl::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	const char *pattern = m_pattern ? STRING( m_pattern ) : "m";

	if ( m_targetEnt )
	{
		// the named lights' styles (a style several lights share changes for all of them)
		CBaseEntity *pTarget = NULL;
		while ( ( pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( m_targetEnt ) ) ) != NULL )
		{
			CLight *pLight = Lights_AsLight( pTarget );
			if ( pLight && pLight->Switchable() )
				pLight->SetStyle( pattern );
		}
	}
	else
	{
		if ( pev->spawnflags & SF_GLIT_AFFECT_CUSTOM_STYLES )
		{
			for ( int i = 1; i < LIGHT_MAX_STYLES; i++ )
				Lights_SetStyle( i, pattern );
		}

		if ( !( pev->spawnflags & SF_GLIT_DONT_AFFECT_DEFAULT ) )
			Lights_SetStyle( 0, pattern );
	}
}
