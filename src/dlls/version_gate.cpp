//=========================================================
// SBO 2.0 version gate (docs/VERSION_GATE.md): ClientConnect refuses a client whose userinfo has no game version
// (VERSION_GATE_KEY, version_gate_shared.h) or one older than VERSION_GATE_MIN, with a one-line reason the engine
// prints in the client's console and shows in its disconnect dialog. Works on every engine: the userinfo comes with
// the connect packet and the reject reason is part of the game DLL interface.
//
// Always let in: bots (FL_FAKECLIENT), HLTV proxies (userinfo "*hltv" 1, set by the engine's proxy type) and a
// listen server's own player (address "loopback").
//
// The version is "x.y.z" (digits only per part); a suffix after the third part that starts with '+' or '-'
// ("+build", "-test.3") is ignored, anything else that does not parse counts as no version.
// VERSION_GATE_TEST builds only the parser (tests/version_gate, ASan/UBSan).
//=========================================================

#ifndef VERSION_GATE_TEST
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"version_gate.h"
#include	"version_gate_shared.h"
#include	"sbo_version_gen.h"	// generated in the build tree (cmake/sbo_version.cmake): SBO_VERSION
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

// The oldest client version this server lets in: raise it for a release that breaks the client / server protocol,
// not for every patch. Same x.y.z format as the key.
#define VERSION_GATE_MIN	"2.1.0"

// No "//" or ":" in the reason: a stock engine also prints it with a stufftext "echo", whose parser drops everything
// after "//" and splits at ":".
#define VERSION_GATE_URL	"sbo.sbotnas.io"

// "x.y.z" (or "x.y.z+anything", "x.y.z-anything") -> out[0..2]. False for anything else: empty, missing parts, a part over 9999,
// other characters before the end of the third part.
static bool VersionGate_Parse( const char *s, int out[ 3 ] )
{
	if ( !s )
		return false;

	for ( int part = 0; part < 3; part++ )
	{
		int value = 0, digits = 0;
		while ( *s >= '0' && *s <= '9' )
		{
			if ( ++digits > 4 )
				return false;
			value = value * 10 + ( *s - '0' );
			s++;
		}
		if ( !digits )
			return false;
		out[ part ] = value;

		if ( part < 2 )
		{
			if ( *s != '.' )
				return false;
			s++;
		}
	}

	return *s == '\0' || *s == '+' || *s == '-';
}

// True when version parses and is at least minimum (which must parse).
static bool VersionGate_AtLeast( const char *version, const char *minimum )
{
	int v[ 3 ], m[ 3 ];
	if ( !VersionGate_Parse( version, v ) || !VersionGate_Parse( minimum, m ) )
		return false;

	for ( int i = 0; i < 3; i++ )
	{
		if ( v[ i ] != m[ i ] )
			return v[ i ] > m[ i ];
	}
	return true;
}

#ifndef VERSION_GATE_TEST
BOOL VersionGate_ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	if ( !pEntity || ( pEntity->v.flags & FL_FAKECLIENT ) )
		return TRUE;	// bots

	if ( !IS_DEDICATED_SERVER() && pszAddress && !strcmp( pszAddress, "loopback" ) )
		return TRUE;	// the listen server's own player

	char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer( pEntity );
	if ( !infobuffer )
		return TRUE;

	if ( atoi( g_engfuncs.pfnInfoKeyValue( infobuffer, "*hltv" ) ) == 1 )
		return TRUE;	// HLTV proxy (TYPE_PROXY)

	const char *pszVersion = g_engfuncs.pfnInfoKeyValue( infobuffer, VERSION_GATE_KEY );
	if ( VersionGate_AtLeast( pszVersion, VERSION_GATE_MIN ) )
		return TRUE;

	snprintf( szRejectReason, 128, "This server runs Syndicate Black Ops " SBO_VERSION ". Update your game at " VERSION_GATE_URL );

	char szVersion[ 32 ];
	snprintf( szVersion, sizeof( szVersion ), "%s", ( pszVersion && pszVersion[ 0 ] ) ? pszVersion : "none" );
	UTIL_LogPrintf( "\"%s\" (%s) refused: game version %s, the server needs %s or newer\n",
		pszName ? pszName : "", pszAddress ? pszAddress : "", szVersion, VERSION_GATE_MIN );

	return FALSE;
}
#endif
