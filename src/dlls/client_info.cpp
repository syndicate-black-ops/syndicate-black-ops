//=========================================================
// SBO 2.0 per-player client info (docs/dev/CLIENT_INFO.md). For each connection the server keeps:
//   - the SBO client version: the version gate's userinfo key (VERSION_GATE_KEY, version_gate_shared.h);
//   - engine, renderer and OS: from cvar queries (QueryClientCvarValue2, answers in
//     NEW_DLL_FUNCTIONS::pfnCvarValue2). The answers go to the server only: no userinfo space, nothing sent to other
//     players. The queries go out on the first frame after ClientPutInServer, not inside it: Valve's engines clear
//     the client's reliable buffer right after ClientPutInServer (SV_SendServerinfo / SV_WriteSpawn), so a query
//     written there never reaches the client (ReHLDS clears it before the call instead).
// When every answer is in, or CLIENT_INFO_WAIT seconds after the queries, one server log line:
//   "name<uid><STEAM_0:1:2><uid>" client "SBO 2.1.0" "unknown" "HL25" "OpenGL"
// A changelevel keeps it (same slot, same userid): one line per connection. Bots are BOT at once, without queries.
//
// The approach (cvar existence tells the client apart) and the enums are SevenKewp's (dlls/player/player_const.h,
// CBasePlayer::QueryClientType / HandleClientCvarResponse; HL SDK license). The cvars were checked against
// Valve's engine binaries (HL25 hw.dll / sw.dll / hw.so, legacy hw.dll / sw.dll):
//   sv_allow_shaders  in every HL25 engine, in no legacy one
//   gl_fog            in every hw (OpenGL) engine, in no sw (software) one
//   host_ver          in no Valve engine (Xash3D's build string: "<build> <version> <os> <arch> <commit>")
//   cl_sbo_os         SBO's own client cvar (client_os_shared.h): "windows" or "linux", the OS the client was built for
// The OS: no engine cvar differs between Valve's Windows and Linux engines (their cvar tables are the same), and
// SevenKewp's m_mousethread_sleep is a cvar of Valve's client.dll, which SBO's client.dll replaces. So SBO's client
// names it in cl_sbo_os; Xash3D keeps its host_ver OS. A client without cl_sbo_os (older builds) stays "unknown".
// CLIENT_INFO_TEST builds only the parsing (tests/client_info, ASan/UBSan).
//=========================================================

#ifndef CLIENT_INFO_TEST
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"version_gate_shared.h"
#endif

#include	<stdio.h>
#include	<string.h>
#include	"client_info.h"
#include	"client_os_shared.h"

static const char *const s_szQueryCvar[ CIQ_COUNT ] =
{
	"sv_allow_shaders",	// CIQ_HL25
	"host_ver",			// CIQ_XASH
	"gl_fog",			// CIQ_OPENGL
	CLIENT_OS_CVAR,		// CIQ_OS
};

const char *ClientInfo_QueryCvar( int query )
{
	if ( query < 0 || query >= CIQ_COUNT )
		return NULL;
	return s_szQueryCvar[ query ];
}

bool ClientInfo_CvarExists( const char *answer )
{
	if ( !answer || !answer[ 0 ] )
		return false;
	return strncmp( answer, "Bad CVAR request", 16 ) != 0;
}

// Xash3D's host_ver: the third space-separated word is the OS ("win32", "linux", "android", ...)
static HL_CLIENT_SYSTEM XashSystem( const char *hostVer )
{
	const char *s = hostVer;
	for ( int word = 0; word < 2; word++ )
	{
		while ( *s == ' ' )
			s++;
		while ( *s && *s != ' ' )
			s++;
	}
	while ( *s == ' ' )
		s++;

	char os[ 16 ];
	int n = 0;
	while ( s[ n ] && s[ n ] != ' ' && n < (int)sizeof( os ) - 1 )
	{
		os[ n ] = s[ n ];
		n++;
	}
	os[ n ] = 0;

	if ( !strcmp( os, "win32" ) || !strcmp( os, "win64" ) || !strcmp( os, "windows" ) )
		return CLIENT_SYSTEM_WINDOWS;
	if ( !strcmp( os, "linux" ) )
		return CLIENT_SYSTEM_LINUX;
	return CLIENT_SYSTEM_NOT_CHECKED;
}

void ClientInfo_Classify( const char *const answers[ CIQ_COUNT ], client_info_t *out )
{
	out->system = CLIENT_SYSTEM_NOT_CHECKED;
	out->engine = CLIENT_ENGINE_NOT_CHECKED;
	out->renderer = CLIENT_RENDERER_NOT_CHECKED;

	const char *xash = answers[ CIQ_XASH ];
	const char *hl25 = answers[ CIQ_HL25 ];
	if ( xash && ClientInfo_CvarExists( xash ) )
	{
		out->engine = CLIENT_ENGINE_HL_XASH;
		out->system = XashSystem( xash );
	}
	else if ( xash && hl25 )
		out->engine = ClientInfo_CvarExists( hl25 ) ? CLIENT_ENGINE_HL_LATEST : CLIENT_ENGINE_HL_LEGACY;

	// SBO's client names its OS (Xash3D: host_ver above)
	const char *os = answers[ CIQ_OS ];
	if ( out->engine != CLIENT_ENGINE_HL_XASH && os )
	{
		if ( !strcmp( os, CLIENT_OS_WINDOWS ) )
			out->system = CLIENT_SYSTEM_WINDOWS;
		else if ( !strcmp( os, CLIENT_OS_LINUX ) )
			out->system = CLIENT_SYSTEM_LINUX;
	}

	if ( answers[ CIQ_OPENGL ] )
		out->renderer = ClientInfo_CvarExists( answers[ CIQ_OPENGL ] ) ? CLIENT_RENDERER_OPENGL : CLIENT_RENDERER_SOFTWARE;
}

void ClientInfo_SetVersion( client_info_t *out, const char *userinfoValue )
{
	int n = 0;
	if ( userinfoValue )
	{
		for ( const char *s = userinfoValue; *s && n < CLIENT_INFO_VERSION_LEN - 1; s++ )
		{
			const char c = *s;
			if ( ( c >= '0' && c <= '9' ) || ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '.' || c == '+'
				|| c == '-' || c == '_' || c == '@' )
				out->version[ n++ ] = c;
			else
				out->version[ n++ ] = '_';	// nothing that could break the log line's quoting
		}
	}
	out->version[ n ] = 0;
	out->mod = n ? CLIENT_MOD_SBO : CLIENT_MOD_NONE;
}

const char *ClientInfo_SystemName( HL_CLIENT_SYSTEM v )
{
	switch ( v )
	{
	case CLIENT_SYSTEM_WINDOWS:	return "Windows";
	case CLIENT_SYSTEM_LINUX:	return "Linux";
	case CLIENT_SYSTEM_NONE:	return "none";
	default:					return "unknown";
	}
}

const char *ClientInfo_EngineName( HL_CLIENT_ENGINE_VERSION v )
{
	switch ( v )
	{
	case CLIENT_ENGINE_HL_LATEST:	return "HL25";
	case CLIENT_ENGINE_HL_LEGACY:	return "legacy";
	case CLIENT_ENGINE_HL_XASH:		return "Xash3D";
	case CLIENT_ENGINE_BOT:			return "bot";
	default:						return "unknown";
	}
}

const char *ClientInfo_RendererName( HL_CLIENT_RENDERER v )
{
	switch ( v )
	{
	case CLIENT_RENDERER_OPENGL:	return "OpenGL";
	case CLIENT_RENDERER_SOFTWARE:	return "software";
	case CLIENT_RENDERER_NONE:		return "none";
	default:						return "unknown";
	}
}

void ClientInfo_VersionName( const client_info_t *info, char *out, int outSize )
{
	if ( outSize <= 0 )
		return;
	switch ( info->mod )
	{
	case CLIENT_MOD_SBO:	snprintf( out, outSize, "SBO %s", info->version ); break;
	case CLIENT_MOD_NONE:	snprintf( out, outSize, "none" ); break;
	case CLIENT_MOD_BOT:	snprintf( out, outSize, "bot" ); break;
	default:				snprintf( out, outSize, "unknown" ); break;
	}
}

#ifndef CLIENT_INFO_TEST

extern int g_teamplay;

// Seconds after ClientPutInServer before the line is written with what has come
#define CLIENT_INFO_WAIT	30.0f

// Request IDs of the game's queries: a plugin's queries (AMX Mod X query_client_cvar) reach pfnCvarValue2 too,
// with their own IDs, and are left alone
#define CLIENT_INFO_REQUEST_BASE	0x53424F00	// "SBO\0"

#define CLIENT_INFO_ANSWER_LEN	64

struct client_info_slot_t
{
	int userid;			// the connection this is for (0: empty slot)
	bool pending;		// waiting for the answers, line not written
	bool toSend;		// the queries go out on the next frame
	float deadline;		// gpGlobals->time for the line without every answer
	bool answered[ CIQ_COUNT ];
	char answer[ CIQ_COUNT ][ CLIENT_INFO_ANSWER_LEN ];
	client_info_t info;
};

static client_info_slot_t s_slots[ 33 ];	// by entity index 1..32
static float s_flNextCheck;
static bool s_bToSend;	// a slot has toSend

static client_info_slot_t *Slot( const edict_t *pEntity )
{
	if ( !pEntity )
		return NULL;
	const int i = ENTINDEX( (edict_t *)pEntity );
	if ( i < 1 || i > gpGlobals->maxClients || i >= (int)( sizeof( s_slots ) / sizeof( s_slots[ 0 ] ) ) )
		return NULL;
	return &s_slots[ i ];
}

static void WriteLine( edict_t *pEntity, client_info_slot_t *s )
{
	s->pending = false;
	s->info.complete = 1;

	char szVersion[ 48 ];
	ClientInfo_VersionName( &s->info, szVersion, sizeof( szVersion ) );
	const char *pszSystem = ClientInfo_SystemName( s->info.system );
	const char *pszEngine = ClientInfo_EngineName( s->info.engine );
	const char *pszRenderer = ClientInfo_RendererName( s->info.renderer );

	// the player part as every other player log line (multiplay_gamerules.cpp "entered the game")
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" client \"%s\" \"%s\" \"%s\" \"%s\"\n",
			STRING( pEntity->v.netname ),
			GETPLAYERUSERID( pEntity ),
			GETPLAYERAUTHID( pEntity ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pEntity ), "model" ),
			szVersion, pszSystem, pszEngine, pszRenderer );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" client \"%s\" \"%s\" \"%s\" \"%s\"\n",
			STRING( pEntity->v.netname ),
			GETPLAYERUSERID( pEntity ),
			GETPLAYERAUTHID( pEntity ),
			GETPLAYERUSERID( pEntity ),
			szVersion, pszSystem, pszEngine, pszRenderer );
	}
}

static void Classify( client_info_slot_t *s )
{
	const char *answers[ CIQ_COUNT ];
	for ( int q = 0; q < CIQ_COUNT; q++ )
		answers[ q ] = s->answered[ q ] ? s->answer[ q ] : NULL;
	ClientInfo_Classify( answers, &s->info );
}

// A new wait for the slot's answers; sendQueries false only for the test layer
static void Start( edict_t *pEntity, client_info_slot_t *s, bool sendQueries )
{
	memset( s->answered, 0, sizeof( s->answered ) );
	memset( s->answer, 0, sizeof( s->answer ) );
	s->info.system = CLIENT_SYSTEM_NOT_CHECKED;
	s->info.engine = CLIENT_ENGINE_NOT_CHECKED;
	s->info.renderer = CLIENT_RENDERER_NOT_CHECKED;
	s->info.complete = 0;
	s->pending = true;
	s->deadline = gpGlobals->time + CLIENT_INFO_WAIT;

	if ( sendQueries )
	{
		s->toSend = true;
		s_bToSend = true;
	}
}

void ClientInfo_ClientPutInServer( edict_t *pEntity )
{
	client_info_slot_t *s = Slot( pEntity );
	if ( !s )
		return;

	// a changelevel: the same connection, already logged
	const int userid = GETPLAYERUSERID( pEntity );
	if ( s->userid == userid && userid && s->info.complete )
		return;

	memset( s, 0, sizeof( *s ) );
	s->userid = userid;

	if ( pEntity->v.flags & FL_FAKECLIENT )
	{
		s->info.mod = CLIENT_MOD_BOT;
		s->info.system = CLIENT_SYSTEM_NONE;
		s->info.engine = CLIENT_ENGINE_BOT;
		s->info.renderer = CLIENT_RENDERER_NONE;
		WriteLine( pEntity, s );
		return;
	}

	char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer( pEntity );
	ClientInfo_SetVersion( &s->info, infobuffer ? g_engfuncs.pfnInfoKeyValue( infobuffer, VERSION_GATE_KEY ) : NULL );

	Start( pEntity, s, true );
}

void ClientInfo_ClientDisconnect( edict_t *pEntity )
{
	client_info_slot_t *s = Slot( pEntity );
	if ( s )
		memset( s, 0, sizeof( *s ) );
}

void ClientInfo_Frame( void )
{
	if ( s_bToSend )
	{
		s_bToSend = false;
		for ( int i = 1; i <= gpGlobals->maxClients && i < (int)( sizeof( s_slots ) / sizeof( s_slots[ 0 ] ) ); i++ )
		{
			client_info_slot_t *s = &s_slots[ i ];
			if ( !s->toSend )
				continue;
			s->toSend = false;
			edict_t *pEntity = INDEXENT( i );
			if ( FNullEnt( pEntity ) || !s->pending )
				continue;
			for ( int q = 0; q < CIQ_COUNT; q++ )
				g_engfuncs.pfnQueryClientCvarValue2( pEntity, s_szQueryCvar[ q ], CLIENT_INFO_REQUEST_BASE + q );
			s->deadline = gpGlobals->time + CLIENT_INFO_WAIT;
		}
	}

	// the wait: once a second; a map change restarts the clock
	if ( gpGlobals->time < s_flNextCheck && s_flNextCheck - gpGlobals->time <= 1.0f )
		return;
	s_flNextCheck = gpGlobals->time + 1.0f;

	for ( int i = 1; i <= gpGlobals->maxClients && i < (int)( sizeof( s_slots ) / sizeof( s_slots[ 0 ] ) ); i++ )
	{
		client_info_slot_t *s = &s_slots[ i ];
		if ( !s->pending || s->toSend || gpGlobals->time < s->deadline )
			continue;
		edict_t *pEntity = INDEXENT( i );
		if ( FNullEnt( pEntity ) || !pEntity->pvPrivateData || !pEntity->v.netname )
		{
			s->pending = false;
			continue;
		}
		Classify( s );
		WriteLine( pEntity, s );
	}
}

static void Answer( edict_t *pEntity, client_info_slot_t *s, int q, const char *pszValue )
{
	if ( !s->pending || s->answered[ q ] )
		return;
	s->answered[ q ] = true;
	snprintf( s->answer[ q ], sizeof( s->answer[ q ] ), "%s", pszValue ? pszValue : "" );

	for ( int i = 0; i < CIQ_COUNT; i++ )
		if ( !s->answered[ i ] )
			return;

	Classify( s );
	WriteLine( pEntity, s );
}

void ClientInfo_CvarValue2( const edict_t *pEnt, int requestID, const char *pszCvarName, const char *pszValue )
{
	const int q = requestID - CLIENT_INFO_REQUEST_BASE;
	if ( q < 0 || q >= CIQ_COUNT || !pszCvarName || strcmp( pszCvarName, s_szQueryCvar[ q ] ) )
		return;	// not the game's query
	client_info_slot_t *s = Slot( pEnt );
	if ( !s )
		return;
	Answer( (edict_t *)pEnt, s, q, pszValue );
}

const client_info_t *ClientInfo_Get( int slot )
{
	if ( slot < 1 || slot > gpGlobals->maxClients || slot >= (int)( sizeof( s_slots ) / sizeof( s_slots[ 0 ] ) ) )
		return NULL;
	if ( !s_slots[ slot ].userid )
		return NULL;
	return &s_slots[ slot ].info;
}

#ifdef SBO_TEST_COMMANDS
void ClientInfo_TestStart( edict_t *pEntity )
{
	client_info_slot_t *s = Slot( pEntity );
	if ( !s )
		return;
	s->userid = GETPLAYERUSERID( pEntity );
	char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer( pEntity );
	ClientInfo_SetVersion( &s->info, infobuffer ? g_engfuncs.pfnInfoKeyValue( infobuffer, VERSION_GATE_KEY ) : NULL );
	Start( pEntity, s, false );
}

void ClientInfo_TestReply( edict_t *pEntity, const char *pszCvarName, const char *pszValue )
{
	for ( int q = 0; q < CIQ_COUNT; q++ )
		if ( pszCvarName && !strcmp( pszCvarName, s_szQueryCvar[ q ] ) )
			ClientInfo_CvarValue2( pEntity, CLIENT_INFO_REQUEST_BASE + q, pszCvarName, pszValue );
}

void ClientInfo_TestPrint( edict_t *pEntity, char *out, int outSize )
{
	client_info_slot_t *s = Slot( pEntity );
	if ( !s )
	{
		snprintf( out, outSize, "no slot" );
		return;
	}
	char szVersion[ 48 ];
	ClientInfo_VersionName( &s->info, szVersion, sizeof( szVersion ) );
	snprintf( out, outSize, "\"%s\" \"%s\" \"%s\" \"%s\" %s", szVersion, ClientInfo_SystemName( s->info.system ),
		ClientInfo_EngineName( s->info.engine ), ClientInfo_RendererName( s->info.renderer ),
		s->pending ? "pending" : ( s->info.complete ? "complete" : "empty" ) );
}
#endif

#endif // CLIENT_INFO_TEST
