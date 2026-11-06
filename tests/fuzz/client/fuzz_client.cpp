// Client fuzz harness (tests/fuzz/client/run_test.sh): loads an AddressSanitizer / UBSan client.so on a stub client
// engine and feeds every user message the client hooks (gHUD and the rest, HOOK_MESSAGE -> pfnHookUserMsg) truncated,
// oversized and hostile payloads, as a malicious or broken server can send them, with HUD_Redraw / HUD_Frame in
// between (the handlers store what the draw code then indexes with).
//
// The stub engine: every cl_enginefunc_t slot returns 0 (cdecl: harmless for int / pointer / void functions), except
// the float ones, the ones whose result the client dereferences (cvars, entities, strings, sprites lists, files) and
// pfnHookUserMsg. VGUI2 is not started (the engine would call IClientVGUI::Initialize): handlers that need VGUI2
// panels are skipped with --skip (run_test.sh lists them).
//
//   fuzz_client <client.so> <gamedir> [--seconds N] [--runs N] [--seed N] [--iterations N] [--skip name,name] [--verbose]
#include <dlfcn.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "wrect.h"
#include "cl_dll.h"
#include "cl_entity.h"
#include "cvardef.h"
#include "pmtrace.h"
#undef min
#undef max

extern "C" void __sanitizer_set_death_callback( void ( *callback )( void ) );

static std::string s_gameDir, s_action;
static std::mt19937 s_rng;
static int Rand( int n ) { return n > 0 ? (int)( s_rng() % (unsigned)n ) : 0; }
static bool s_verbose;

static void OnDeath( void ) { fprintf( stderr, "\n*** sanitizer error during: %s\n", s_action.c_str() ); }

//------------------------------------------------------------------------------------------------- stub engine

static int Zero( void ) { return 0; }

static float FZero( void ) { return 0; }
static float s_time = 1.0f;
static float ClientTime( void ) { return s_time; }
static double AbsTime( void ) { return s_time; }
static float MaxSpeed( void ) { return 320; }
static float Gravity( void ) { return 800; }
static float RandomFloat( float lo, float hi ) { return lo + ( hi - lo ) * ( Rand( 10000 ) / 10000.0f ); }
static long RandomLong( long lo, long hi ) { return hi <= lo ? lo : lo + (long)( s_rng() % (unsigned long)( hi - lo + 1 ) ); }

static std::map<std::string, cvar_t *> s_cvars;
static cvar_t *Cvar( const char *name, const char *value )
{
	std::string k( name ? name : "" );
	auto it = s_cvars.find( k );
	if ( it != s_cvars.end() )
		return it->second;
	cvar_t *c = (cvar_t *)calloc( 1, sizeof( cvar_t ) );
	c->name = strdup( k.c_str() );
	c->string = strdup( value ? value : "0" );
	c->value = (float)atof( c->string );
	s_cvars[k] = c;
	return c;
}
static cvar_t *RegisterVariable( char *name, char *value, int ) { return Cvar( name, value ); }
static cvar_t *GetCvarPointer( const char *name ) { return Cvar( name, "0" ); }
static float GetCvarFloat( char *name ) { return Cvar( name, "0" )->value; }
static char *GetCvarString( char *name ) { return Cvar( name, "0" )->string; }
static void CvarSetValue( char *name, float v )
{
	char b[32];
	snprintf( b, sizeof( b ), "%g", v );
	cvar_t *c = Cvar( name, "0" );
	c->string = strdup( b );
	c->value = v;
}
static void CvarSet( char *name, char *v )
{
	cvar_t *c = Cvar( name, "0" );
	c->string = strdup( v ? v : "" );
	c->value = (float)atof( c->string );
}

static std::map<std::string, pfnUserMsgHook> s_msgs;
static int HookUserMsg( char *name, pfnUserMsgHook pfn )
{
	s_msgs[name] = pfn;
	return 1;
}

static int GetScreenInfo( SCREENINFO *p )
{
	if ( !p )
		return 0;
	p->iWidth = 1024;
	p->iHeight = 768;
	p->iFlags = 0;
	p->iCharHeight = 14;
	for ( int i = 0; i < 256; i++ )
		p->charWidths[i] = 8;
	return 1;
}

static char *COM_ParseFile( char *data, char *token );
static byte *COM_LoadFile( char *path, int, int *len );

// sprites/hud.txt: "<count>" then "<name> <res> <sprite> <x> <y> <w> <h>" per entry, read with COM_Parse (the engine's
// SPR_GetList)
static client_sprite_t *SPR_GetList( char *psz, int *piCount )
{
	*piCount = 0;
	int len = 0;
	char *file = (char *)COM_LoadFile( psz, 5, &len );
	if ( !file )
		return NULL;
	char tok[1024];
	char *p = COM_ParseFile( file, tok );
	int n = atoi( tok );
	if ( !p || n <= 0 || n > 4096 )
	{
		free( file );
		return NULL;
	}
	client_sprite_t *list = (client_sprite_t *)calloc( n, sizeof( client_sprite_t ) );
	int i = 0;
	for ( ; i < n && p; i++ )
	{
		client_sprite_t &s = list[i];
		char t[7][1024];
		int k = 0;
		for ( ; k < 7 && p; k++ )
			p = COM_ParseFile( p, t[k] );
		if ( k < 7 || !t[6][0] )
			break;
		snprintf( s.szName, sizeof( s.szName ), "%s", t[0] );
		s.iRes = atoi( t[1] );
		snprintf( s.szSprite, sizeof( s.szSprite ), "%s", t[2] );
		s.rc.left = atoi( t[3] );
		s.rc.top = atoi( t[4] );
		s.rc.right = s.rc.left + atoi( t[5] );
		s.rc.bottom = s.rc.top + atoi( t[6] );
	}
	free( file );
	*piCount = i;
	return list;
}
static HSPRITE SPR_Load( const char * ) { return 1 + Rand( 100 ); }
static int SPR_Frames( HSPRITE ) { return 1; }
static int SPR_Size( HSPRITE, int ) { return 16; }

static cl_entity_t s_entities[2048];
static cl_entity_t *GetLocalPlayer( void ) { return &s_entities[1]; }
static cl_entity_t *GetViewModel( void ) { return &s_entities[0]; }
static cl_entity_t *GetEntityByIndex( int i ) { return ( i >= 0 && i < 2048 ) ? &s_entities[i] : NULL; }
static int GetMaxClients( void ) { return 32; }
static void GetPlayerInfo( int i, hud_player_info_t *p )
{
	if ( !p )
		return;
	memset( p, 0, sizeof( *p ) );
	if ( i < 1 || i > 32 )
		return;
	// the name inside a block laid out like the engine's player_info_t (sbo_bridge.cpp reads the Steam ID next to it)
	static char blocks[33][1024];
	char *name = blocks[i] + 4 + 256;
	snprintf( name, 32, "Player%d", i );
	p->name = name;
	p->model = (char *)"gign";
	p->thisplayer = i == 1;
	p->ping = 50;
}
static const char *EmptyString( ... ) { return ""; }
static char *CmdArgv( int ) { return (char *)""; }
static const char *GameDir( void ) { return "syndicate"; }
static const char *LevelName( void ) { return "maps/co_fuzz.bsp"; }
static pmtrace_t s_trace;
static pmtrace_t *TraceLine( float *, float *end, int, int, int )
{
	memset( &s_trace, 0, sizeof( s_trace ) );
	s_trace.fraction = 1;
	s_trace.endpos[0] = end[0]; s_trace.endpos[1] = end[1]; s_trace.endpos[2] = end[2];
	return &s_trace;
}
static void AngleVectors( const float *a, float *f, float *r, float *u )
{
	if ( f ) { f[0] = 1; f[1] = 0; f[2] = 0; }
	if ( r ) { r[0] = 0; r[1] = -1; r[2] = 0; }
	if ( u ) { u[0] = 0; u[1] = 0; u[2] = 1; }
	(void)a;
}
static byte *COM_LoadFile( char *path, int, int *len )
{
	if ( len )
		*len = 0;
	if ( !path || strstr( path, ".." ) )
		return NULL;
	std::string p = s_gameDir + "/" + path;
	FILE *f = fopen( p.c_str(), "rb" );
	if ( !f )
		return NULL;
	fseek( f, 0, SEEK_END );
	long n = ftell( f );
	fseek( f, 0, SEEK_SET );
	byte *b = (byte *)malloc( n + 1 );
	if ( fread( b, 1, n, f ) != (size_t)n )
		n = 0;
	b[n] = 0;
	fclose( f );
	if ( len )
		*len = (int)n;
	return b;
}
static void COM_FreeFile( void *b ) { free( b ); }
// the engine's COM_Parse: token at most 1023 characters (the client passes 1024-byte buffers)
static char *COM_ParseFile( char *data, char *token )
{
	token[0] = 0;
	if ( !data )
		return NULL;
	int len = 0;
	for ( ;; )
	{
		while ( *data && (unsigned char)*data <= ' ' )
			data++;
		if ( !*data )
			return NULL;
		if ( data[0] == '/' && data[1] == '/' )
		{
			while ( *data && *data != '\n' )
				data++;
			continue;
		}
		break;
	}
	if ( *data == '"' )
	{
		data++;
		while ( *data && *data != '"' )
		{
			if ( len < 1023 )
				token[len++] = *data;
			data++;
		}
		token[len] = 0;
		return *data ? data + 1 : data;
	}
	if ( strchr( "{}()':", *data ) )
	{
		token[0] = *data;
		token[1] = 0;
		return data + 1;
	}
	while ( *data && (unsigned char)*data > ' ' && !strchr( "{}()':", *data ) )
	{
		if ( len < 1023 )
			token[len++] = *data;
		data++;
	}
	token[len] = 0;
	return data;
}

static void ConPrintf( char *fmt, ... )
{
	char b[4096];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( b, sizeof( b ), fmt, ap );
	va_end( ap );
	if ( s_verbose )
		fputs( b, stderr );
}
static void ConNPrintf( int, char *fmt, ... )
{
	char b[4096];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( b, sizeof( b ), fmt, ap );
	va_end( ap );
}

static void *s_interfaces[6][128];

static void InitEngine( cl_enginefunc_t *e )
{
	void **slots = (void **)e;
	for ( size_t i = 0; i < sizeof( *e ) / sizeof( void * ); i++ )
		slots[i] = (void *)Zero;
	for ( auto &t : s_interfaces )
	{
		for ( auto &p : t )
			p = (void *)Zero;
		t[0] = (void *)1;	// the version field of each API struct
	}
	e->pTriAPI = (struct triangleapi_s *)s_interfaces[0];
	e->pEfxAPI = (struct efx_api_s *)s_interfaces[1];
	e->pEventAPI = (struct event_api_s *)s_interfaces[2];
	e->pDemoAPI = (struct demo_api_s *)s_interfaces[3];
	e->pNetAPI = (struct net_api_s *)s_interfaces[4];
	e->pVoiceTweak = (struct IVoiceTweak_s *)s_interfaces[5];

	e->pfnSPR_Load = SPR_Load;
	e->pfnSPR_Frames = SPR_Frames;
	e->pfnSPR_Height = SPR_Size;
	e->pfnSPR_Width = SPR_Size;
	e->pfnSPR_GetList = SPR_GetList;
	e->pfnGetScreenInfo = GetScreenInfo;
	e->pfnRegisterVariable = RegisterVariable;
	e->pfnGetCvarFloat = GetCvarFloat;
	e->pfnGetCvarString = GetCvarString;
	e->pfnHookUserMsg = HookUserMsg;
	e->pfnGetPlayerInfo = GetPlayerInfo;
	e->pfnAngleVectors = AngleVectors;
	e->GetMaxClients = GetMaxClients;
	e->Cvar_SetValue = CvarSetValue;
	e->Cmd_Argv = CmdArgv;
	e->Con_Printf = ConPrintf;
	e->Con_DPrintf = ConPrintf;
	e->Con_NPrintf = ConNPrintf;
	e->PhysInfo_ValueForKey = (const char *( * )( const char * ))EmptyString;
	e->ServerInfo_ValueForKey = (const char *( * )( const char * ))EmptyString;
	e->GetClientMaxspeed = MaxSpeed;
	e->GetLocalPlayer = GetLocalPlayer;
	e->GetViewModel = GetViewModel;
	e->GetEntityByIndex = GetEntityByIndex;
	e->GetClientTime = ClientTime;
	e->PM_TraceLine = TraceLine;
	e->pfnRandomFloat = RandomFloat;
	e->pfnRandomLong = RandomLong;
	e->pfnGetGameDirectory = GameDir;
	e->pfnGetCvarPointer = GetCvarPointer;
	e->Key_LookupBinding = (const char *( * )( const char * ))EmptyString;
	e->pfnGetLevelName = LevelName;
	e->COM_LoadFile = COM_LoadFile;
	e->COM_ParseFile = COM_ParseFile;
	e->COM_FreeFile = COM_FreeFile;
	e->PlayerInfo_ValueForKey = (const char *( * )( int, const char * ))EmptyString;
	e->LocalPlayerInfo_ValueForKey = (const char *( * )( const char * ))EmptyString;
	e->hudGetClientOldTime = ClientTime;
	e->hudGetServerGravityValue = Gravity;
	e->GetAbsoluteTime = AbsTime;
	e->Cvar_Set = CvarSet;
	(void)FZero;
}

//------------------------------------------------------------------------------------------------- payloads

static std::vector<unsigned char> Payload( void )
{
	std::vector<unsigned char> b;
	int n;
	switch ( Rand( 6 ) )
	{
	case 0: n = 0; break;
	case 1: n = 1 + Rand( 4 ); break;
	case 2: n = 192; break;	// the engine's largest user message
	default: n = Rand( 64 ); break;
	}
	for ( int i = 0; i < n; i++ )
	{
		switch ( Rand( 6 ) )
		{
		case 0: b.push_back( 0 ); break;
		case 1: b.push_back( 0xff ); break;
		case 2: b.push_back( (unsigned char)Rand( 34 ) ); break;	// player indices, small counts
		case 3: b.push_back( (unsigned char)( 'A' + Rand( 26 ) ) ); break;
		default: b.push_back( (unsigned char)s_rng() ); break;
		}
	}
	return b;
}

//------------------------------------------------------------------------------------------------- main

int main( int argc, char **argv )
{
	if ( argc < 3 )
	{
		fprintf( stderr, "usage: %s <client.so> <gamedir> [--seconds N] [--runs N] [--seed N] [--iterations N] [--skip a,b] [--verbose]\n", argv[0] );
		return 2;
	}
	const char *so = argv[1];
	s_gameDir = argv[2];
	int seconds = 60, runs = 1000000, iterations = 2000;
	unsigned seed = (unsigned)time( NULL );
	std::set<std::string> skip;
	for ( int i = 3; i < argc; i++ )
	{
		std::string a = argv[i];
		if ( a == "--seconds" && i + 1 < argc ) seconds = atoi( argv[++i] );
		else if ( a == "--runs" && i + 1 < argc ) runs = atoi( argv[++i] );
		else if ( a == "--seed" && i + 1 < argc ) seed = (unsigned)strtoul( argv[++i], NULL, 10 );
		else if ( a == "--iterations" && i + 1 < argc ) iterations = atoi( argv[++i] );
		else if ( a == "--verbose" ) s_verbose = true;
		else if ( a == "--skip" && i + 1 < argc )
		{
			std::string l = argv[++i];
			size_t p = 0;
			while ( p <= l.size() )
			{
				size_t q = l.find( ',', p );
				if ( q == std::string::npos ) q = l.size();
				if ( q > p ) skip.insert( l.substr( p, q - p ) );
				p = q + 1;
			}
		}
	}
	time_t end = time( NULL ) + seconds;
	int done = 0, failed = 0;
	for ( int r = 0; r < runs && time( NULL ) < end; r++ )
	{
		unsigned runSeed = seed + r;
		fflush( stderr );
		pid_t pid = fork();
		if ( pid == 0 )
		{
			__sanitizer_set_death_callback( OnDeath );
			s_rng.seed( runSeed );
			void *h = dlopen( so, RTLD_NOW );
			if ( !h )
			{
				fprintf( stderr, "dlopen %s: %s\n", so, dlerror() );
				_exit( 3 );
			}
			auto init = (int ( * )( cl_enginefunc_t *, int ))dlsym( h, "Initialize" );
			auto hudInit = (void ( * )( void ))dlsym( h, "HUD_Init" );
			auto vidInit = (int ( * )( void ))dlsym( h, "HUD_VidInit" );
			auto redraw = (int ( * )( float, int ))dlsym( h, "HUD_Redraw" );
			auto frame = (void ( * )( double ))dlsym( h, "HUD_Frame" );
			if ( !init || !hudInit || !vidInit || !redraw )
			{
				fprintf( stderr, "missing client exports\n" );
				_exit( 3 );
			}
			static cl_enginefunc_t eng;
			InitEngine( &eng );
			s_action = "Initialize";
			if ( !init( &eng, CLDLL_INTERFACE_VERSION ) )
				_exit( 3 );
			s_action = "HUD_Init";
			hudInit();
			s_action = "HUD_VidInit";
			vidInit();
			std::vector<std::pair<std::string, pfnUserMsgHook>> msgs;
			for ( auto &m : s_msgs )
				if ( !skip.count( m.first ) )
					msgs.push_back( m );
			if ( r == 0 )
			{
				fprintf( stderr, "fuzz_client: %zu user messages hooked, %zu fuzzed:", s_msgs.size(), msgs.size() );
				for ( auto &m : msgs )
					fprintf( stderr, " %s", m.first.c_str() );
				fprintf( stderr, "\n" );
			}
			for ( int it = 0; it < iterations && !msgs.empty(); it++ )
			{
				auto &m = msgs[Rand( (int)msgs.size() )];
				std::vector<unsigned char> p = Payload();
				// exactly sized: one byte past the payload is out of bounds for AddressSanitizer
				unsigned char *buf = (unsigned char *)malloc( p.size() ? p.size() : 1 );
				if ( !p.empty() )
					memcpy( buf, p.data(), p.size() );
				char hex[64] = "";
				for ( size_t k = 0; k < p.size() && k < 20; k++ )
					snprintf( hex + k * 3, 4, "%02x ", p[k] );
				char act[256];
				snprintf( act, sizeof( act ), "%s (%zu bytes: %s%s)", m.first.c_str(), p.size(), hex, p.size() > 20 ? "..." : "" );
				s_action = act;
				m.second( m.first.c_str(), (int)p.size(), buf );
				free( buf );
				if ( Rand( 4 ) == 0 )
				{
					s_time += 0.05f;
					snprintf( act, sizeof( act ), "HUD_Redraw after %s", m.first.c_str() );
					s_action = act;
					redraw( s_time, Rand( 10 ) == 0 );
					if ( frame )
						frame( s_time );
				}
			}
			_exit( 0 );
		}
		int status = 0;
		time_t started = time( NULL );
		while ( waitpid( pid, &status, WNOHANG ) == 0 )
		{
			if ( time( NULL ) - started > 120 )
			{
				fprintf( stderr, "*** run %u hangs: killed; reproduce: --seed %u --runs 1\n", runSeed, runSeed );
				kill( pid, SIGABRT );
				for ( int k = 0; k < 50 && waitpid( pid, &status, WNOHANG ) == 0; k++ )
					usleep( 100000 );
				kill( pid, SIGKILL );
				waitpid( pid, &status, 0 );
				failed++;
				break;
			}
			usleep( 10000 );
		}
		done++;
		if ( !WIFEXITED( status ) || WEXITSTATUS( status ) != 0 )
		{
			failed++;
			fprintf( stderr, "*** run %u FAILED (status 0x%x); reproduce: --seed %u --runs 1\n", runSeed, status, runSeed );
		}
	}
	fprintf( stderr, "fuzz_client: %d runs, %d failed\n", done, failed );
	return failed ? 1 : 0;
}
