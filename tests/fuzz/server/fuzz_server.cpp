// Server fuzz harness (tests/fuzz/server/run_test.sh): loads an AddressSanitizer / UBSan build of hl.so on the stub
// engine (stub_engine.cpp) and feeds it what a malicious client or a broken map can send:
//
//   clients   ClientCommand with every command the game knows (read from the source by run_test.sh) and hostile
//             arguments, chat text, user info changes (names), impulses and buttons, kill / disconnect / reconnect,
//             with server frames in between (StartFrame, Pre/PostThink, UpdateClientData, entity thinks);
//   entities  every entity class (LINK_ENTITY_TO_CLASS) spawned with hostile key values (the map's keys mixed with
//             the keys the game reads, values from the map-entities corpus and generated ones), then Activate, Use,
//             Touch, Blocked and think frames.
//
// Each run is one fresh hl.so in a child process (fork), so an AddressSanitizer abort is one finding and the next
// run goes on. The parent prints a summary; exit status 1 if any child failed or the stub engine reported an
// engine-error condition.
//
//   fuzz_server <hl.so> <gamedir> <commands.txt> <keys.txt> <classes.txt> <values.txt> [options]
//     --mode clients|entities|all|cases (cases: the case files given as further arguments)   --map NAME   --seconds N   --runs N   --seed N   --iterations N   --verbose
//     cases only: --keep-going (a failed expect is reported and the case goes on; exit 5 at the end)
//                 --drop-unknown-classes (spawn of a class the hl.so lacks is dropped, as the engine does, "@@NOCLASS")
//                 --case-timeout N (seconds per case, default 120)
//
// The case runner is also the map tests' runner (tests/maps, scripts/run_map_tests.sh, docs/dev/MAP_TESTS.md): the same
// case files run on the 1.38 server .so and on develop's, and the observations (OBS lines) are compared.
#include "stub_engine.h"
#include "entity_state.h"	// clientdata_t

#define STRING( s ) ( g_stubGlobals.pStringBase + ( s ) )

#include <dlfcn.h>
#include <regex>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#ifdef FUZZ_NO_SANITIZER
// the plain runner (map tests, tests/maps): no sanitizer runtime (the 1.38 server .so cannot run under ASan)
extern "C" void __sanitizer_set_death_callback( void ( * )( void ) ) {}
#else
extern "C" void __sanitizer_set_death_callback( void ( *callback )( void ) );
#endif

static std::vector<std::string> s_commands, s_keys, s_classes, s_values;
static bool s_keepGoing, s_dropUnknownClasses;
static int s_caseTimeout = 120;
static std::mt19937 s_rng;
static int s_iterations = 300;

static int Rand( int n ) { return n > 0 ? (int)( s_rng() % (unsigned)n ) : 0; }
template <class T> static const T &Pick( const std::vector<T> &v ) { return v[Rand( (int)v.size() )]; }

static std::vector<std::string> ReadLines( const char *path )
{
	std::vector<std::string> out;
	std::ifstream f( path );
	std::string l;
	while ( std::getline( f, l ) )
	{
		if ( !l.empty() && l.back() == '\r' )
			l.pop_back();
		if ( !l.empty() && l[0] != '#' )
			out.push_back( l );
	}
	return out;
}

// hostile strings: numbers at the edges, long, format specifiers, control and high bytes, paths, quotes
static std::string HostileValue( void )
{
	static const char *fixed[] = { "", "0", "1", "-1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "15",
		"16", "19", "20", "31", "32", "33", "63", "64", "65", "100", "127", "128", "255", "256", "1000", "4096",
		"32767", "32768", "65535", "65536", "2147483647", "-2147483648", "4294967296", "99999999999999999999",
		"-99999999", "1e38", "-1e38", "1e999", "nan", "inf", "-inf", "0.5", "-0.0001", "0x7fffffff", "%s%s%s%s%n",
		"%d%x%p", "%", "\\", "\"", "\"\"", ";", "\n", "../../../../etc/passwd", "*0", "*9999", "!", "#",
		"models/player.mdl", "sprites/laserbeam.spr", "player", "worldspawn", "info_player_start", " ", "\x01\x02",
		"\x7f", "\xff\xfe\xfd", "\xc3\xa9", "^1red^x0f0^#ff0000", "\x07" "ff0000", "\x08" "ff00ff00", "1 2 3",
		"999999 999999 999999", "-99999 0 99999", "nan nan nan", "0 0", "a b c d e f", "{", "}", "//", "/*" };
	switch ( Rand( 6 ) )
	{
	case 0:
	case 1:
		return fixed[Rand( sizeof( fixed ) / sizeof( fixed[0] ) )];
	case 2:
		if ( !s_values.empty() )
			return Pick( s_values );
		return "1";
	case 3:
	{
		// long: around the buffer sizes the game uses (32, 64, 128, 190, 256, 512, 1024, 2048)
		static const int lens[] = { 31, 32, 33, 63, 64, 65, 127, 128, 129, 189, 190, 191, 255, 256, 257, 511, 512,
			513, 1023, 1024, 1025, 2047, 2048, 2049, 4100 };
		int n = lens[Rand( sizeof( lens ) / sizeof( lens[0] ) )];
		char c = "A%x\xe9 ;\\"[Rand( 7 )];
		return std::string( n, c );
	}
	case 4:
	{
		std::string s;
		int n = Rand( 40 );
		for ( int i = 0; i < n; i++ )
			s += (char)( 1 + Rand( 255 ) );
		return s;
	}
	default:
		return std::to_string( (int)s_rng() % ( Rand( 2 ) ? 20 : 100000 ) );
	}
}

static void SetAction( const char *fmt, ... )
{
	char buf[512];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	// printable, short: the action line goes with every report
	for ( char *p = buf; *p; p++ )
		if ( (unsigned char)*p < 32 || (unsigned char)*p > 126 )
			*p = '?';
	g_stubAction = buf;
}

static void OnDeath( void )
{
	fprintf( stderr, "\n*** sanitizer error during: %s\n", g_stubAction.c_str() );
}

//------------------------------------------------------------------------------------------------- server

static void *s_hGame;
static enginefuncs_t s_engfuncs;
static int s_connected[33];

static edict_t *Client( int i ) { return &g_stubEdicts[i]; }

static void Connect( int i, const char *name )
{
	edict_t *e = Client( i );
	SetAction( "connect %d", i );
	memset( &e->v, 0, sizeof( e->v ) );
	e->free = 0;
	e->v.pContainingEntity = e;
	e->v.netname = Stub_AllocString( name );
	char *info = Stub_InfoBuffer( e );
	info[0] = 0;
	Stub_SetInfo( info, "name", name );
	Stub_SetInfo( info, "model", "gign" );
	Stub_SetInfo( info, "cl_lw", "1" );
	Stub_SetInfo( info, "cl_autowepswitch", "1" );
	char reject[128] = "";
	g_dll.pfnClientConnect( e, name, "127.0.0.1:27005", reject );
	SetAction( "ClientPutInServer %d", i );
	g_dll.pfnClientPutInServer( e );
	s_connected[i] = 1;
}

static void Disconnect( int i )
{
	edict_t *e = Client( i );
	SetAction( "ClientDisconnect %d", i );
	g_dll.pfnClientDisconnect( e );
	// the engine frees the player's private data when the slot is reused or the map changes; the edict stays
	s_connected[i] = 0;
	if ( e->pvPrivateData )
	{
		if ( g_stubHaveNewDll && g_newDll.pfnOnFreeEntPrivateData )
			g_newDll.pfnOnFreeEntPrivateData( e );
		free( e->pvPrivateData );
		e->pvPrivateData = NULL;
	}
	memset( &e->v, 0, sizeof( e->v ) );
	e->v.pContainingEntity = e;
}

// the frame's game time (case command "frametime"; 0.1 s unless a case sets it) and the buttons each client holds
// (case command "buttons": set in pev->button before PlayerPreThink, as the engine does from a usercmd)
static float s_frameTime = 0.1f;
static int s_buttons[33];

static void Frame( void )
{
	g_stubGlobals.time += s_frameTime;
	g_stubGlobals.frametime = s_frameTime;
	SetAction( "StartFrame" );
	g_dll.pfnStartFrame();
	for ( int i = 1; i <= g_stubMaxClients; i++ )
	{
		if ( !s_connected[i] || !Client( i )->pvPrivateData )
			continue;
		edict_t *e = Client( i );
		if ( i < 33 && s_buttons[i] )
			e->v.button = s_buttons[i];
		SetAction( "PlayerPreThink %d", i );
		g_dll.pfnPlayerPreThink( e );
		SetAction( "PlayerPostThink %d", i );
		g_dll.pfnPlayerPostThink( e );
		clientdata_t cd;
		memset( &cd, 0, sizeof( cd ) );
		SetAction( "UpdateClientData %d", i );
		g_dll.pfnUpdateClientData( e, 1, &cd );
	}
	for ( int i = g_stubMaxClients + 1; i < STUB_MAX_EDICTS; i++ )
	{
		edict_t *e = &g_stubEdicts[i];
		if ( e->free || !e->pvPrivateData )
			continue;
		float t = e->v.movetype == MOVETYPE_PUSH ? e->v.ltime : g_stubGlobals.time;
		if ( e->v.movetype == MOVETYPE_PUSH )
			e->v.ltime += s_frameTime;
		if ( e->v.nextthink > 0 && e->v.nextthink <= t + s_frameTime )
		{
			e->v.nextthink = 0;
			SetAction( "Think %s (%d)", STRING( e->v.classname ), i );
			g_dll.pfnThink( e );
		}
	}
	// FL_KILLME: the engine frees them after the physics frame
	for ( int i = g_stubMaxClients + 1; i < STUB_MAX_EDICTS; i++ )
	{
		edict_t *e = &g_stubEdicts[i];
		if ( !e->free && ( e->v.flags & FL_KILLME ) )
			Stub_FreeEdict( e );
	}
}

static bool LoadServer( const char *hl, const char *mapname, const std::vector<std::string> *worldKeys = NULL,
	bool activate = true )
{
	s_hGame = dlopen( hl, RTLD_NOW );
	if ( !s_hGame )
	{
		fprintf( stderr, "dlopen %s: %s\n", hl, dlerror() );
		return false;
	}
	Stub_Init( s_hGame, &s_engfuncs );
	auto give = (void ( * )( enginefuncs_t *, globalvars_t * ))dlsym( s_hGame, "GiveFnptrsToDll" );
	auto api2 = (int ( * )( DLL_FUNCTIONS *, int * ))dlsym( s_hGame, "GetEntityAPI2" );
	auto newapi = (int ( * )( NEW_DLL_FUNCTIONS *, int * ))dlsym( s_hGame, "GetNewDLLFunctions" );
	if ( !give || !api2 )
	{
		fprintf( stderr, "%s: no GiveFnptrsToDll / GetEntityAPI2\n", hl );
		return false;
	}
	give( &s_engfuncs, &g_stubGlobals );
	int ver = INTERFACE_VERSION;
	api2( &g_dll, &ver );
	if ( newapi )
	{
		int nver = NEW_DLL_FUNCTIONS_VERSION;
		g_stubHaveNewDll = newapi( &g_newDll, &nver ) != 0;
	}
	SetAction( "GameInit" );
	g_dll.pfnGameInit();

	// map load: the world, then a few entities a map always has
	g_stubLoading = true;
	g_stubGlobals.mapname = Stub_AllocString( mapname );
	edict_t *world = &g_stubEdicts[0];
	world->free = 0;
	world->v.classname = Stub_AllocString( "worldspawn" );
	// the engine's world model is maps/<map>.bsp: the game reads it for maps/map_compat.txt (its CRC, its entity lump)
	world->v.model = Stub_AllocString( ( std::string( "maps/" ) + mapname + ".bsp" ).c_str() );
	world->v.modelindex = 1;
	auto worldspawn = (void ( * )( entvars_t * ))dlsym( s_hGame, "worldspawn" );
	SetAction( "worldspawn" );
	worldspawn( &world->v );
	KeyValueData kvd = { (char *)"worldspawn", (char *)"skyname", (char *)"space", 0 };
	g_dll.pfnKeyValue( world, &kvd );
	// a case's "map <name> key value ...": the worldspawn's own keys
	for ( size_t k = 0; worldKeys && k + 1 < worldKeys->size(); k += 2 )
	{
		KeyValueData wk = { (char *)"worldspawn", (char *)( *worldKeys )[k].c_str(), (char *)( *worldKeys )[k + 1].c_str(), 0 };
		SetAction( "KeyValue worldspawn \"%.40s\"", wk.szKeyName );
		g_dll.pfnKeyValue( world, &wk );
	}
	g_dll.pfnSpawn( world );

	static const char *starts[] = { "info_player_start", "info_player_deathmatch", "info_player_coop" };
	for ( const char *s : starts )
		for ( int k = 0; k < 3; k++ )
		{
			SetAction( "spawn %s", s );
			edict_t *e = Stub_CreateNamed( s );
			if ( e )
			{
				e->v.origin = Vector( 64.0f * k, 0, 0 );
				g_dll.pfnSpawn( e );
			}
		}
	// a case activates the server itself ("activate"), after its spawns: they are part of the map load, as the
	// engine spawns the lump before ServerActivate (maps/map_compat.txt changes keys only until then)
	if ( activate )
	{
		SetAction( "ServerActivate" );
		g_dll.pfnServerActivate( g_stubEdicts, STUB_MAX_EDICTS, g_stubMaxClients );
		g_stubLoading = false;
	}
	return true;
}

//------------------------------------------------------------------------------------------------- clients

static void ClientCommandOnce( int i )
{
	edict_t *e = Client( i );
	std::vector<std::string> argv;
	argv.push_back( Rand( 20 ) ? Pick( s_commands ) : HostileValue() );
	int n = Rand( 4 );
	for ( int k = 0; k < n; k++ )
		argv.push_back( HostileValue() );
	std::string args;
	for ( size_t k = 1; k < argv.size(); k++ )
		args += ( k > 1 ? " " : "" ) + argv[k];
	if ( Rand( 8 ) == 0 )
		args = HostileValue();	// Cmd_Args doesn't have to match argv (quotes, the engine's own tokenizing)
	Stub_SetCommand( argv, args );
	SetAction( "ClientCommand %d \"%s\" argc %d args \"%.60s\"", i, argv[0].c_str(), (int)argv.size(), args.c_str() );
	g_dll.pfnClientCommand( e );
}

static void ClientOnce( int i )
{
	edict_t *e = Client( i );
	switch ( Rand( 12 ) )
	{
	case 0:
	{
		// the engine cuts names to 31 bytes before ClientUserInfoChanged (SV_ExtractFromUserinfo, MAX_NAME 32)
		std::string name = HostileValue().substr( 0, 31 );
		Stub_SetInfo( Stub_InfoBuffer( e ), "name", name.c_str() );
		SetAction( "ClientUserInfoChanged %d name \"%.60s\"", i, name.c_str() );
		g_dll.pfnClientUserInfoChanged( e, Stub_InfoBuffer( e ) );
		break;
	}
	case 1:
	{
		static const char *keys[] = { "model", "cl_autowepswitch", "topcolor", "_vgui_menus", "cl_lw" };
		const char *k = keys[Rand( 5 )];
		std::string v = HostileValue();
		Stub_SetInfo( Stub_InfoBuffer( e ), k, v.c_str() );
		SetAction( "ClientUserInfoChanged %d %s \"%.60s\"", i, k, v.c_str() );
		g_dll.pfnClientUserInfoChanged( e, Stub_InfoBuffer( e ) );
		break;
	}
	case 2:
		e->v.impulse = Rand( 256 );
		e->v.button = (int)s_rng();
		break;
	case 3:
		if ( Rand( 4 ) == 0 )
		{
			SetAction( "ClientKill %d", i );
			g_dll.pfnClientKill( e );
		}
		break;
	case 4:
	{
		// chat: say / say_team with hostile text, and the raw form
		std::vector<std::string> argv = { Rand( 2 ) ? "say" : "say_team" };
		std::string text = HostileValue();
		if ( Rand( 3 ) == 0 )
			text = std::string( Rand( 2 ) ? "/spec" : ".spectate" );
		argv.push_back( text );
		Stub_SetCommand( argv, Rand( 2 ) ? "\"" + text + "\"" : text );
		SetAction( "ClientCommand %d %s \"%.60s\"", i, argv[0].c_str(), text.c_str() );
		g_dll.pfnClientCommand( e );
		break;
	}
	default:
		ClientCommandOnce( i );
		break;
	}
}

static void RunClients( void )
{
	int nClients = 1 + Rand( g_stubMaxClients );
	for ( int i = 1; i <= nClients; i++ )
		Connect( i, ( "Fuzz" + std::to_string( i ) ).c_str() );
	for ( int k = 0; k < 10; k++ )
		Frame();

	for ( int it = 0; it < s_iterations; it++ )
	{
		int i = 1 + Rand( nClients );
		if ( !s_connected[i] )
		{
			if ( Rand( 4 ) == 0 )
				Connect( i, ( "Re" + std::to_string( i ) ).c_str() );
			continue;
		}
		if ( Rand( 60 ) == 0 )
		{
			Disconnect( i );
			continue;
		}
		ClientOnce( i );
		if ( Rand( 3 ) == 0 )
			Frame();
	}
	for ( int k = 0; k < 20; k++ )
		Frame();
}

//------------------------------------------------------------------------------------------------- entities

static void RunEntities( void )
{
	Connect( 1, "Fuzz1" );
	Connect( 2, "Fuzz2" );
	for ( int k = 0; k < 5; k++ )
		Frame();

	// with the edict's serial number: a freed edict gets reused by another entity (a beam, a gib), which the harness
	// must not Use or remove in the old one's place
	std::vector<std::pair<edict_t *, int>> spawned;
	auto alive = []( const std::pair<edict_t *, int> &p ) { return !p.first->free && p.first->serialnumber == p.second && p.first->pvPrivateData; };
	for ( int it = 0; it < s_iterations; it++ )
	{
		const std::string &cls = Pick( s_classes );
		if ( cls == "worldspawn" || cls == "player" )
			continue;
		g_stubLoading = true;	// as map entities: precaches allowed (classes made at run time are precached by their makers)
		SetAction( "create %s", cls.c_str() );
		edict_t *e = Stub_CreateNamed( cls.c_str() );
		if ( !e )
			continue;
		int nkv = Rand( 12 );
		for ( int k = 0; k < nkv; k++ )
		{
			std::string key = Rand( 4 ) ? Pick( s_keys ) : HostileValue();
			std::string value = HostileValue();
			g_stubMapStrings.insert( value );
			KeyValueData kvd = { (char *)cls.c_str(), (char *)key.c_str(), (char *)value.c_str(), 0 };
			SetAction( "KeyValue %s \"%.40s\" \"%.60s\"", cls.c_str(), key.c_str(), value.c_str() );
			g_dll.pfnKeyValue( e, &kvd );
		}
		e->v.origin = Vector( (float)Rand( 512 ), (float)Rand( 512 ), (float)Rand( 128 ) );
		SetAction( "Spawn %s", cls.c_str() );
		if ( e->free )
			continue;
		if ( g_dll.pfnSpawn( e ) < 0 )
		{
			Stub_FreeEdict( e );
			continue;
		}
		g_stubLoading = false;
		// an entity that removed itself in Spawn: its edict is free, and the next entity made (a test_effect's beam)
		// gets it with the same serial number; kept here, the harness would remove that one as if it were this
		if ( e->free )
			continue;
		spawned.push_back( { e, e->serialnumber } );

		for ( int k = 0; k < 3; k++ )
		{
			if ( spawned.empty() )
				break;
			const auto &sp = spawned[Rand( (int)spawned.size() )];
			if ( !alive( sp ) )
				continue;
			edict_t *t = sp.first;
			edict_t *other = Client( 1 + Rand( 2 ) );
			switch ( Rand( 4 ) )
			{
			case 0:
				SetAction( "Use %s by player", STRING( t->v.classname ) );
				g_dll.pfnUse( t, other );
				break;
			case 1:
				SetAction( "Touch %s by player", STRING( t->v.classname ) );
				g_dll.pfnTouch( t, other );
				break;
			case 2:
				SetAction( "Blocked %s by player", STRING( t->v.classname ) );
				g_dll.pfnBlocked( t, other );
				break;
			default:
				SetAction( "Use %s by world", STRING( t->v.classname ) );
				g_dll.pfnUse( t, &g_stubEdicts[0] );
				break;
			}
		}
		Frame();
		// keep the edict count bounded: remove old ones now and then as a killtarget would
		if ( spawned.size() > 200 || Rand( 10 ) == 0 )
		{
			int idx = Rand( (int)spawned.size() );
			edict_t *t = spawned[idx].first;
			// not what a player carries (a weapon picked up has an owner / aiment): only map entities a killtarget hits
			if ( alive( spawned[idx] ) && !t->v.owner && !t->v.aiment )
			{
				SetAction( "UTIL_Remove %s", STRING( t->v.classname ) );
				t->v.flags |= FL_KILLME;
			}
			spawned.erase( spawned.begin() + idx );
		}
	}
	for ( int k = 0; k < 30; k++ )
		Frame();
}

//------------------------------------------------------------------------------------------------- cases

// A case file (tests/fuzz/server/cases/*.txt) is a fixed reproducer: one command per line, tokens as COM_Parse reads
// them ("quoted strings" keep spaces), # comments.
//   map <name>                       first line: the map name (picks the game mode)
//   connect <slot> [name]            ClientConnect + ClientPutInServer
//   connectonly <slot>               ClientConnect only (a client still loading)
//   disconnect <slot>
//   spawn <class> [key value]...     a map entity with these key values; the next index in the case's list (0, 1, ...)
//   activate                         ServerActivate (Activate for every entity), end of the map load
//   use <n> [player <slot>|world]    Use on the n-th spawned entity
//   touch <n> <slot>                 the player touches it
//   cmd <slot> <argv0> [args...]     a client command
//   frames <count>
//   expect <text>                    (the rest of the line, quotes and all) the game printed <text> (ALERT / SERVER_PRINT) since the start; else the case fails
//   hangs: a case that runs more than 120 s (--case-timeout) fails
// Map-test commands (tests/maps, docs/dev/MAP_TESTS.md); entity numbers <n> are the case's spawn order as above:
//   cvar <name> <value>              set a server cvar (mp_makermon 1, ...)
//   fire <targetname> [world]        Use every entity with that name; activator and caller client 1 (or the world)
//   touchall <class> [slot]          client <slot> (default 1) touches every entity of <class>, 3 frames apart
//   useent <n> <m>|world             Use entity n with entity m (or the world) as activator and caller
//   useclient <n> <slot>             Use entity n by client <slot>
//   touchent <n> <m>                 Touch( n, m ): entity m touches entity n (a trigger_hurt n kills monster m)
//   touchclient <n> <slot>           client <slot> touches entity n
//   touchclass <n> <class>           every live entity of <class> touches entity n
//   touchref <ref> <slot>            client <slot> touches the entity <ref> (targetname or classname)
//   blockclient <n> <slot>           entity n is blocked by client <slot> (Blocked: a mover's "dmg")
//   frametime <seconds>              the game time of each following frame (default 0.1: 10 frames a second)
//   buttons <slot> <bits>            client <slot> holds these IN_* bits from now on (0: none), like a usercmd
//   mark <text>                      a marker in the log ("@@MARK <text>"; console "[t=..] MARK <text>")
// Observations: each prints one "OBS ..." line on stderr; the map tests compare them between two hl.so builds.
//   count <class>                    live entities of <class>
//   dump <n> / dumpclass <class> / dumpent <targetname>   classname, name, health, deadflag, solid, effects, ...
//   soundcount <targetname> <channel>  EMIT_SOUNDs on <channel> by the entities with that name so far (STUB_LOG_SOUND=1)
//   expectn <N> <text>               <text> printed exactly N times;  expectnot <text>: never printed
//   expectmin / expectmax <N> <text>  <text> printed at least / at most N times (a random rate)
//   expectcount <class> <N>          N live entities of <class>
//   expectdump <class|targetname> <text>  a live entity of that class (or name) has <text> in its DUMP line
//                                    (e.g. "health=85.0", "origin=1000.0 1000.0")
//   field <ref> <field>...           the entity's entvars fields (origin, avelocity, speed, body, skin, scale, ...: FieldText)
//   expectfield <ref> <field> <value> [tol]  the field has that value (numbers / vectors within tol, default 0.01);
//                                    "expectfield globals cdAudioTrack <n>" reads gpGlobals
//   expectcvar <name> <value>        a cvar's value (a worldspawn key's cvar, ...)
//   expectrange <ref> <field> <lo> <hi>  a number field within [lo, hi] (a random choice)
//   expectbits <ref> <field> <mask>  every bit of mask is set in the field; expectnobits: none of them is
//   expectre <regex> / expectnotre <regex>  some console line matches the regular expression (ECMAScript) / none does
// <ref>: a spawned entity's number, a targetname or a classname (the first live one).
// Setup: "map <name> key value ..." gives the worldspawn those keys; "brushsize x y z" gives the "*N" brush models spawned
// after it that size (the stub has no BSP; default none). STUB_LOG_SOUND also logs EMIT_AMBIENT_SOUND ("AMBIENT #e
// <sample> vol att pitch flags") and the volume, attenuation, pitch and flags of each SOUND; light styles are logged
// as LIGHTSTYLE <n> "<pattern>"
// A failed expect* ends the case with exit 5, or with --keep-going is reported and the case goes on (exit 5 at the end).
// CASE_DUMP=1 prints the console log at the end of the case (and at a failed expect).
static std::vector<std::string> Tokens( const std::string &line )
{
	std::vector<std::string> t;
	size_t i = 0;
	while ( i < line.size() )
	{
		while ( i < line.size() && (unsigned char)line[i] <= ' ' )
			i++;
		if ( i >= line.size() || line[i] == '#' )
			break;
		std::string tok;
		if ( line[i] == '"' )
		{
			i++;
			while ( i < line.size() && line[i] != '"' )
				tok += line[i++];
			i++;
		}
		else
			while ( i < line.size() && (unsigned char)line[i] > ' ' )
				tok += line[i++];
		t.push_back( tok );
	}
	return t;
}

static int s_failedExpects;

static void Obs( const char *fmt, ... )
{
	char buf[1024];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	fprintf( stderr, "OBS %s\n", buf );
}

static void DumpConsole( void )
{
	if ( getenv( "CASE_DUMP" ) )
	{
		fputs( "----- console -----\n", stderr );
		fwrite( g_stubConsole.data(), 1, g_stubConsole.size(), stderr );
	}
}

// an expect* result: the observation line; a failure ends the case (exit 5) unless --keep-going
static bool ExpectResult( bool ok, const std::string &what, const std::string &failure )
{
	Obs( "%s: %s", what.c_str(), ok ? "ok" : failure.c_str() );
	if ( ok )
		return true;
	fprintf( stderr, "case: %s\n", failure.c_str() );
	DumpConsole();
	s_failedExpects++;
	return s_keepGoing;
}

static bool LiveEdict( const edict_t *e ) { return !e->free && e->pvPrivateData && !( e->v.flags & FL_KILLME ); }

static int CountClass( const char *cls )
{
	int n = 0;
	for ( int i = 1; i < STUB_MAX_EDICTS; i++ )
		if ( LiveEdict( &g_stubEdicts[i] ) && !strcmp( STRING( g_stubEdicts[i].v.classname ), cls ) )
			n++;
	return n;
}

static std::string DumpText( const edict_t *e )
{
	char buf[512];
	snprintf( buf, sizeof( buf ), "DUMP #%d %s tn=%s health=%.1f deadflag=%d solid=%d effects=%d flags=%d killme=%d origin=%.1f %.1f %.1f",
		(int)( e - g_stubEdicts ), STRING( e->v.classname ), STRING( e->v.targetname ), e->v.health, e->v.deadflag,
		e->v.solid, e->v.effects, e->v.flags, ( e->v.flags & FL_KILLME ) ? 1 : 0, e->v.origin.x, e->v.origin.y,
		e->v.origin.z );
	return buf;
}

static void DumpEdict( const edict_t *e )
{
	Obs( "%s", DumpText( e ).c_str() );
}

// a field of an entity's entvars as text: numbers "%g", vectors "x y z", strings as they are; NULL: no such field
static bool FieldText( const edict_t *e, const std::string &f, std::string &out )
{
	char buf[256];
	const entvars_t &v = e->v;
	auto z = []( double x ) { return x == 0 ? 0.0 : x; };	// no "-0"
	auto num = [&]( double x ) { snprintf( buf, sizeof( buf ), "%g", z( x ) ); out = buf; return true; };
	auto vec = [&]( const vec3_t &x ) { snprintf( buf, sizeof( buf ), "%g %g %g", z( x[0] ), z( x[1] ), z( x[2] ) ); out = buf; return true; };
	auto str = [&]( string_t s ) { out = s ? STRING( s ) : ""; return true; };
	if ( f == "origin" ) return vec( v.origin );
	if ( f == "angles" ) return vec( v.angles );
	if ( f == "velocity" ) return vec( v.velocity );
	if ( f == "avelocity" ) return vec( v.avelocity );
	if ( f == "movedir" ) return vec( v.movedir );
	if ( f == "rendercolor" ) return vec( v.rendercolor );
	if ( f == "mins" ) return vec( v.mins );
	if ( f == "maxs" ) return vec( v.maxs );
	if ( f == "health" ) return num( v.health );
	if ( f == "max_health" ) return num( v.max_health );
	if ( f == "speed" ) return num( v.speed );
	if ( f == "dmg" ) return num( v.dmg );
	if ( f == "scale" ) return num( v.scale );
	if ( f == "frame" ) return num( v.frame );
	if ( f == "framerate" ) return num( v.framerate );
	if ( f == "renderamt" ) return num( v.renderamt );
	if ( f == "rendermode" ) return num( v.rendermode );
	if ( f == "renderfx" ) return num( v.renderfx );
	if ( f == "body" ) return num( v.body );
	if ( f == "skin" ) return num( v.skin );
	if ( f == "sequence" ) return num( v.sequence );
	if ( f == "movetype" ) return num( v.movetype );
	if ( f == "solid" ) return num( v.solid );
	if ( f == "effects" ) return num( v.effects );
	if ( f == "spawnflags" ) return num( v.spawnflags );
	if ( f == "flags" ) return num( v.flags );
	if ( f == "weapons" ) return num( v.weapons );
	if ( f == "impulse" ) return num( v.impulse );
	if ( f == "friction" ) return num( v.friction );
	if ( f == "gravity" ) return num( v.gravity );
	if ( f == "armorvalue" ) return num( v.armorvalue );
	if ( f == "takedamage" ) return num( v.takedamage );
	if ( f == "deadflag" ) return num( v.deadflag );
	if ( f == "waterlevel" ) return num( v.waterlevel );
	if ( f == "watertype" ) return num( v.watertype );
	if ( f == "modelindex" ) return num( v.modelindex );
	if ( f == "yaw_speed" ) return num( v.yaw_speed );
	if ( f == "ideal_yaw" ) return num( v.ideal_yaw );
	if ( f == "ltime" ) return num( v.ltime );
	if ( f == "nextthink" ) return num( v.nextthink );
	if ( f == "animtime" ) return num( v.animtime );
	if ( f == "model" ) return str( v.model );
	if ( f == "target" ) return str( v.target );
	if ( f == "targetname" ) return str( v.targetname );
	if ( f == "netname" ) return str( v.netname );
	if ( f == "message" ) return str( v.message );
	if ( f == "noise" ) return str( v.noise );
	if ( f == "noise1" ) return str( v.noise1 );
	if ( f == "noise2" ) return str( v.noise2 );
	if ( f == "noise3" ) return str( v.noise3 );
	return false;
}

// "<n>" (a spawned entity), "world", a targetname or a classname: the first live entity that matches
static edict_t *FindRef( const std::vector<edict_t *> &ents, const std::string &r )
{
	if ( r == "world" )
		return &g_stubEdicts[0];
	if ( !r.empty() && isdigit( (unsigned char)r[0] ) )
	{
		size_t i = (size_t)atoi( r.c_str() );
		return i < ents.size() && LiveEdict( ents[i] ) ? ents[i] : NULL;
	}
	for ( int pass = 0; pass < 2; pass++ )
		for ( int i = 1; i < STUB_MAX_EDICTS; i++ )
		{
			edict_t *e = &g_stubEdicts[i];
			if ( LiveEdict( e ) && !strcmp( STRING( pass ? e->v.classname : e->v.targetname ), r.c_str() ) )
				return e;
		}
	return NULL;
}

// <got> equals <want>: as numbers (1 or 3 of them) within <tol>, else as text
static bool SameValue( const std::string &got, const std::string &want, float tol )
{
	float g[3], w[3];
	int ng = sscanf( got.c_str(), "%f %f %f", &g[0], &g[1], &g[2] ), nw = sscanf( want.c_str(), "%f %f %f", &w[0], &w[1], &w[2] );
	if ( ng > 0 && ng == nw )
	{
		for ( int k = 0; k < ng; k++ )
			if ( fabsf( g[k] - w[k] ) > tol )
				return false;
		return true;
	}
	return got == want;
}

// the rest of the line after the first <skip> tokens (quotes and all)
static std::string RestOfLine( const std::string &line, int skip )
{
	size_t i = 0;
	for ( int k = 0; k < skip; k++ )
	{
		while ( i < line.size() && (unsigned char)line[i] <= ' ' )
			i++;
		while ( i < line.size() && (unsigned char)line[i] > ' ' )
			i++;
	}
	while ( i < line.size() && (unsigned char)line[i] <= ' ' )
		i++;
	return line.substr( i );
}

// SOUND lines (STUB_LOG_SOUND) of edict <index> on channel <ch>
static int CountSounds( int index, const std::string &ch )
{
	std::string tag = "SOUND #" + std::to_string( index ) + " ", chan = " ch" + ch + " ";
	int n = 0;
	for ( size_t pos = 0; ( pos = g_stubConsole.find( tag, pos ) ) != std::string::npos; pos += tag.size() )
	{
		size_t eol = g_stubConsole.find( '\n', pos );
		if ( g_stubConsole.substr( pos, eol == std::string::npos ? std::string::npos : eol - pos ).find( chan ) != std::string::npos )
			n++;
	}
	return n;
}

static int RunCase( const char *hl, const std::vector<std::string> &lines )
{
	std::vector<edict_t *> ents;
	bool loaded = false;
	auto spawned = [&]( const std::string &n ) -> edict_t * {
		size_t i = (size_t)atoi( n.c_str() );
		return i < ents.size() && !ents[i]->free && ents[i]->pvPrivateData ? ents[i] : NULL;
	};
	for ( const std::string &l : lines )
	{
		std::vector<std::string> t = Tokens( l );
		if ( t.empty() )
			continue;
		const std::string &c = t[0];
		if ( c == "map" && t.size() > 1 )
		{
			std::vector<std::string> worldKeys( t.begin() + 2, t.end() );
			if ( !LoadServer( hl, t[1].c_str(), &worldKeys, false ) )
				return 3;
			loaded = true;
			g_stubLoading = true;	// the case's spawns are part of the map load, until "activate"
			continue;
		}
		if ( !loaded )
		{
			fprintf( stderr, "case: \"map\" must come first\n" );
			return 2;
		}
		if ( c == "connect" && t.size() > 1 )
			Connect( atoi( t[1].c_str() ), t.size() > 2 ? t[2].c_str() : "Case" );
		else if ( c == "connectonly" && t.size() > 1 )
		{
			edict_t *e = Client( atoi( t[1].c_str() ) );
			e->free = 0;
			e->v.pContainingEntity = e;
			e->v.netname = Stub_AllocString( "Loading" );
			char reject[128] = "";
			SetAction( "ClientConnect %s (no ClientPutInServer)", t[1].c_str() );
			g_dll.pfnClientConnect( e, "Loading", "127.0.0.1:27005", reject );
			s_connected[atoi( t[1].c_str() )] = 1;
		}
		else if ( c == "disconnect" && t.size() > 1 )
			Disconnect( atoi( t[1].c_str() ) );
		else if ( c == "spawn" && t.size() > 1 )
		{
			SetAction( "create %s", t[1].c_str() );
			edict_t *e = Stub_CreateNamed( t[1].c_str() );
			if ( !e && s_dropUnknownClasses )
			{
				// the engine drops an entity whose class the game does not have ("Can't init")
				fprintf( stderr, "@@NOCLASS %s\n", t[1].c_str() );
				continue;	// not numbered: index commands (use, touchent, ...) count only spawned entities
			}
			if ( !e )
			{
				fprintf( stderr, "case: no class %s\n", t[1].c_str() );
				return 2;
			}
			for ( size_t k = 2; k + 1 < t.size(); k += 2 )
			{
				KeyValueData kvd = { (char *)t[1].c_str(), (char *)t[k].c_str(), (char *)t[k + 1].c_str(), 0 };
				SetAction( "KeyValue %s \"%.40s\" \"%.60s\"", t[1].c_str(), t[k].c_str(), t[k + 1].c_str() );
				g_dll.pfnKeyValue( e, &kvd );
			}
			SetAction( "Spawn %s", t[1].c_str() );
			if ( g_dll.pfnSpawn( e ) < 0 )
				Stub_FreeEdict( e );
			ents.push_back( e );
		}
		else if ( c == "activate" )
		{
			SetAction( "ServerActivate" );
			g_dll.pfnServerActivate( g_stubEdicts, STUB_MAX_EDICTS, g_stubMaxClients );
			g_stubLoading = false;
		}
		else if ( ( c == "use" || c == "touch" ) && t.size() > 1 )
		{
			size_t n = (size_t)atoi( t[1].c_str() );
			if ( n >= ents.size() || ents[n]->free || !ents[n]->pvPrivateData )
				continue;
			edict_t *other = &g_stubEdicts[0];
			if ( c == "touch" && t.size() > 2 )
				other = Client( atoi( t[2].c_str() ) );
			else if ( t.size() > 3 && t[2] == "player" )
				other = Client( atoi( t[3].c_str() ) );
			SetAction( "%s %s", c.c_str(), STRING( ents[n]->v.classname ) );
			if ( c == "use" )
				g_dll.pfnUse( ents[n], other );
			else
				g_dll.pfnTouch( ents[n], other );
		}
		else if ( c == "cmd" && t.size() > 2 )
		{
			std::vector<std::string> argv( t.begin() + 2, t.end() );
			std::string args;
			for ( size_t k = 1; k < argv.size(); k++ )
				args += ( k > 1 ? " " : "" ) + argv[k];
			Stub_SetCommand( argv, args );
			SetAction( "ClientCommand %s %s", t[1].c_str(), l.c_str() );
			g_dll.pfnClientCommand( Client( atoi( t[1].c_str() ) ) );
		}
		else if ( c == "expect" && t.size() > 1 )
		{
			// the rest of the line, as it is (quotes included)
			std::string want = RestOfLine( l, 1 );
			if ( !ExpectResult( g_stubConsole.find( want ) != std::string::npos, "expect " + want,
					"expected console text not printed: " + want ) )
				return 5;
		}
		else if ( c == "expectn" && t.size() > 2 )
		{
			std::string want = RestOfLine( l, 2 );
			int n = 0;
			for ( size_t pos = 0; ( pos = g_stubConsole.find( want, pos ) ) != std::string::npos; pos += want.size() )
				n++;
			if ( !ExpectResult( n == atoi( t[1].c_str() ), "expectn " + t[1] + " " + want,
					"\"" + want + "\" printed " + std::to_string( n ) + " times, expected " + t[1] ) )
				return 5;
		}
		else if ( ( c == "expectmin" || c == "expectmax" ) && t.size() > 2 )
		{
			// expectmin / expectmax <N> <text>: <text> printed at least / at most N times (a random rate)
			std::string want = RestOfLine( l, 2 );
			int n = 0;
			for ( size_t pos = 0; ( pos = g_stubConsole.find( want, pos ) ) != std::string::npos; pos += want.size() )
				n++;
			int lim = atoi( t[1].c_str() );
			if ( !ExpectResult( c == "expectmin" ? n >= lim : n <= lim, c + " " + t[1] + " " + want,
					"\"" + want + "\" printed " + std::to_string( n ) + " times, expected " + ( c == "expectmin" ? "at least " : "at most " ) + t[1] ) )
				return 5;
		}
		else if ( c == "expectnot" && t.size() > 1 )
		{
			std::string want = RestOfLine( l, 1 );
			if ( !ExpectResult( g_stubConsole.find( want ) == std::string::npos, "expectnot " + want,
					"console text printed but must not be: " + want ) )
				return 5;
		}
		else if ( c == "expectcount" && t.size() > 2 )
		{
			int n = CountClass( t[1].c_str() );
			if ( !ExpectResult( n == atoi( t[2].c_str() ), "expectcount " + t[1] + " " + t[2],
					"count " + t[1] + " = " + std::to_string( n ) + ", expected " + t[2] ) )
				return 5;
		}
		else if ( c == "expectdump" && t.size() > 2 )
		{
			// some live entity of the class (or with that targetname) has <text> in its DUMP line
			std::string want = RestOfLine( l, 2 ), seen;
			bool ok = false;
			for ( int i = 1; i < STUB_MAX_EDICTS && !ok; i++ )
			{
				edict_t *e = &g_stubEdicts[i];
				if ( LiveEdict( e ) && ( !strcmp( STRING( e->v.classname ), t[1].c_str() ) || !strcmp( STRING( e->v.targetname ), t[1].c_str() ) ) )
				{
					seen = DumpText( e );
					ok = seen.find( want ) != std::string::npos;
				}
			}
			if ( !ExpectResult( ok, "expectdump " + t[1] + " " + want,
					"no " + t[1] + " with \"" + want + "\" (last: " + ( seen.empty() ? "none" : seen ) + ")" ) )
				return 5;
		}
		else if ( c == "brushsize" && t.size() > 3 )
		{
			for ( int k = 0; k < 3; k++ )
				g_stubBrushSize[k] = (float)atof( t[1 + k].c_str() );
		}
		else if ( c == "field" && t.size() > 2 )
		{
			edict_t *e = FindRef( ents, t[1] );
			std::string line = "FIELD " + t[1];
			for ( size_t k = 2; k < t.size(); k++ )
			{
				std::string v;
				line += " " + t[k] + "=" + ( !e ? std::string( "(none)" ) : FieldText( e, t[k], v ) ? v : std::string( "(no field)" ) );
			}
			Obs( "%s", line.c_str() );
		}
		else if ( c == "expectfield" && t.size() > 3 )
		{
			// expectfield <ref> <field> "<value>" [tol]
			edict_t *e = FindRef( ents, t[1] );
			std::string got;
			bool have = e && FieldText( e, t[2], got );
			if ( t[1] == "globals" && t[2] == "cdAudioTrack" )	// gpGlobals (a worldspawn key)
			{
				got = std::to_string( g_stubGlobals.cdAudioTrack );
				have = true;
				e = &g_stubEdicts[0];
			}
			float tol = t.size() > 4 ? (float)atof( t[4].c_str() ) : 0.01f;
			if ( !ExpectResult( have && SameValue( got, t[3], tol ), "expectfield " + t[1] + " " + t[2] + " " + t[3],
					t[1] + "." + t[2] + " = " + ( !e ? std::string( "(no entity)" ) : have ? got : std::string( "(no field)" ) ) + ", expected " + t[3] ) )
				return 5;
		}
		else if ( ( c == "expectbits" || c == "expectnobits" ) && t.size() > 3 )
		{
			// expectbits <ref> <field> <mask>: every bit of mask is set in the (integer) field; expectnobits: none is
			edict_t *e = FindRef( ents, t[1] );
			std::string got;
			bool have = e && FieldText( e, t[2], got );
			long v = have ? (long)atof( got.c_str() ) : 0, mask = atol( t[3].c_str() );
			bool ok = have && ( c == "expectbits" ? ( v & mask ) == mask : ( v & mask ) == 0 );
			if ( !ExpectResult( ok, c + " " + t[1] + " " + t[2] + " " + t[3],
					t[1] + "." + t[2] + " = " + ( !e ? std::string( "(no entity)" ) : have ? got : std::string( "(no field)" ) ) + ", mask " + t[3] ) )
				return 5;
		}
		else if ( c == "expectrange" && t.size() > 4 )
		{
			// expectrange <ref> <field> <lo> <hi>: a number field within [lo, hi] (a random choice: the stub's
			// random numbers differ between the binaries, so the exact value is not compared)
			edict_t *e = FindRef( ents, t[1] );
			std::string got;
			bool have = e && FieldText( e, t[2], got );
			double v = have ? atof( got.c_str() ) : 0, lo = atof( t[3].c_str() ), hi = atof( t[4].c_str() );
			if ( !ExpectResult( have && v >= lo - 0.001 && v <= hi + 0.001, "expectrange " + t[1] + " " + t[2] + " " + t[3] + " " + t[4],
					t[1] + "." + t[2] + " = " + ( !e ? std::string( "(no entity)" ) : have ? got : std::string( "(no field)" ) ) + ", expected " + t[3] + " to " + t[4] ) )
				return 5;
		}
		else if ( ( c == "expectre" || c == "expectnotre" ) && t.size() > 1 )
		{
			// a console line matching the regular expression (ECMAScript, searched in each line) / none
			std::string want = RestOfLine( l, 1 );
			std::regex re( want );
			bool found = false;
			for ( size_t pos = 0; pos < g_stubConsole.size() && !found; )
			{
				size_t eol = g_stubConsole.find( '\n', pos );
				std::string line = g_stubConsole.substr( pos, eol == std::string::npos ? std::string::npos : eol - pos );
				found = std::regex_search( line, re );
				pos = eol == std::string::npos ? g_stubConsole.size() : eol + 1;
			}
			bool ok = c == "expectre" ? found : !found;
			if ( !ExpectResult( ok, c + " " + want, c == "expectre" ? "no console line matches " + want : "a console line matches " + want ) )
				return 5;
		}
		else if ( c == "expectcvar" && t.size() > 2 )
		{
			std::string got = s_engfuncs.pfnCVarGetString( t[1].c_str() );
			if ( !ExpectResult( SameValue( got, t[2], 0.01f ), "expectcvar " + t[1] + " " + t[2],
					t[1] + " = \"" + got + "\", expected " + t[2] ) )
				return 5;
		}
		else if ( c == "count" && t.size() > 1 )
			Obs( "COUNT %s %d", t[1].c_str(), CountClass( t[1].c_str() ) );
		else if ( c == "dump" && t.size() > 1 )
		{
			size_t a = (size_t)atoi( t[1].c_str() );
			if ( a < ents.size() && !ents[a]->free )
				DumpEdict( ents[a] );
			else
				Obs( "DUMP %s free", t[1].c_str() );
		}
		else if ( ( c == "dumpclass" || c == "dumpent" ) && t.size() > 1 )
		{
			for ( int i = 1; i < STUB_MAX_EDICTS; i++ )
			{
				edict_t *e = &g_stubEdicts[i];
				if ( !e->free && e->pvPrivateData
					&& !strcmp( STRING( c == "dumpclass" ? e->v.classname : e->v.targetname ), t[1].c_str() ) )
					DumpEdict( e );
			}
		}
		else if ( c == "soundcount" && t.size() > 2 )
		{
			for ( int i = 1; i < STUB_MAX_EDICTS; i++ )
			{
				edict_t *e = &g_stubEdicts[i];
				if ( !e->free && e->pvPrivateData && !strcmp( STRING( e->v.targetname ), t[1].c_str() ) )
					Obs( "SOUNDS %s %s ch%s %d", t[1].c_str(), STRING( e->v.classname ), t[2].c_str(), CountSounds( i, t[2] ) );
			}
		}
		else if ( c == "cvar" && t.size() > 2 )
			s_engfuncs.pfnCVarSetString( t[1].c_str(), t[2].c_str() );
		else if ( c == "mark" )
		{
			std::string text = RestOfLine( l, 1 );
			char tb[32];
			snprintf( tb, sizeof( tb ), "[t=%.1f] ", g_stubGlobals.time );
			g_stubConsole += std::string( tb ) + "MARK " + text + "\n";
			fprintf( stderr, "@@MARK %s\n", text.c_str() );
		}
		else if ( c == "fire" && t.size() > 1 )
		{
			edict_t *other = ( t.size() > 2 && t[2] == "world" ) ? &g_stubEdicts[0] : Client( 1 );
			fprintf( stderr, "@@FIRE %s\n", t[1].c_str() );
			std::vector<edict_t *> hit;
			for ( int k = 1; k < STUB_MAX_EDICTS; k++ )
			{
				edict_t *e = &g_stubEdicts[k];
				if ( !e->free && e->pvPrivateData && e->v.targetname && !strcmp( STRING( e->v.targetname ), t[1].c_str() ) )
					hit.push_back( e );
			}
			for ( edict_t *e : hit )
				if ( !e->free && e->pvPrivateData )	// an earlier Use can remove a later one
				{
					SetAction( "fire %s (%s)", t[1].c_str(), STRING( e->v.classname ) );
					g_dll.pfnUse( e, other );
				}
		}
		else if ( c == "touchall" && t.size() > 1 )
		{
			int slot = t.size() > 2 ? atoi( t[2].c_str() ) : 1;
			std::vector<edict_t *> hit;
			for ( int k = 1; k < STUB_MAX_EDICTS; k++ )
			{
				edict_t *e = &g_stubEdicts[k];
				if ( !e->free && e->pvPrivateData && e->v.classname && !strcmp( STRING( e->v.classname ), t[1].c_str() ) )
					hit.push_back( e );
			}
			int ord = 0;
			for ( edict_t *e : hit )
			{
				if ( e->free || !e->pvPrivateData )
					continue;
				fprintf( stderr, "@@TOUCH %s #%d %s -> %s\n", t[1].c_str(), ++ord, STRING( e->v.targetname ), STRING( e->v.target ) );
				SetAction( "touch %s", t[1].c_str() );
				g_dll.pfnTouch( e, Client( slot ) );
				for ( int f = 0; f < 3; f++ )
					Frame();
			}
		}
		else if ( c == "useent" && t.size() > 2 )
		{
			edict_t *a = spawned( t[1] );
			size_t b = (size_t)atoi( t[2].c_str() );
			edict_t *other = t[2] != "world" && b < ents.size() ? ents[b] : &g_stubEdicts[0];
			if ( a )
			{
				SetAction( "useent %s", STRING( a->v.classname ) );
				g_dll.pfnUse( a, other );
			}
		}
		else if ( c == "useclient" && t.size() > 2 )
		{
			if ( edict_t *a = spawned( t[1] ) )
			{
				SetAction( "useclient %s", STRING( a->v.classname ) );
				g_dll.pfnUse( a, Client( atoi( t[2].c_str() ) ) );
			}
		}
		else if ( c == "touchent" && t.size() > 2 )
		{
			edict_t *a = spawned( t[1] ), *b = spawned( t[2] );
			if ( a && b )
			{
				SetAction( "touchent %s %s", STRING( a->v.classname ), STRING( b->v.classname ) );
				g_dll.pfnTouch( a, b );
			}
			else
				g_stubConsole += "TOUCHENT skipped (freed)\n";
		}
		else if ( c == "touchclient" && t.size() > 2 )
		{
			if ( edict_t *a = spawned( t[1] ) )
			{
				SetAction( "touchclient %s", STRING( a->v.classname ) );
				g_dll.pfnTouch( a, Client( atoi( t[2].c_str() ) ) );
			}
		}
		else if ( c == "blockclient" && t.size() > 2 )
		{
			// a mover is blocked by the client (Blocked( n, client )): a door / rotating / train's "dmg"
			if ( edict_t *a = spawned( t[1] ) )
			{
				SetAction( "blockclient %s", STRING( a->v.classname ) );
				g_dll.pfnBlocked( a, Client( atoi( t[2].c_str() ) ) );
			}
		}
		else if ( c == "touchref" && t.size() > 2 )
		{
			// client <slot> touches the entity <ref> (a targetname or classname: one the case did not spawn, e.g. a can)
			if ( edict_t *a = FindRef( ents, t[1] ) )
			{
				SetAction( "touchref %s", STRING( a->v.classname ) );
				g_dll.pfnTouch( a, Client( atoi( t[2].c_str() ) ) );
			}
		}
		else if ( c == "touchclass" && t.size() > 2 )
		{
			edict_t *a = spawned( t[1] );
			for ( int i = 1; a && i < STUB_MAX_EDICTS; i++ )
			{
				edict_t *e = &g_stubEdicts[i];
				if ( !LiveEdict( e ) || strcmp( STRING( e->v.classname ), t[2].c_str() ) )
					continue;
				if ( a->free || !a->pvPrivateData )
					break;
				SetAction( "touchclass %s %s", STRING( a->v.classname ), t[2].c_str() );
				g_dll.pfnTouch( a, e );
			}
		}
		else if ( c == "frametime" && t.size() > 1 )
			s_frameTime = (float)atof( t[1].c_str() );
		else if ( c == "buttons" && t.size() > 2 )
		{
			int slot = atoi( t[1].c_str() );
			if ( slot > 0 && slot < 33 )
			{
				s_buttons[slot] = atoi( t[2].c_str() );
				Client( slot )->v.button = s_buttons[slot];
			}
		}
		else if ( c == "frames" && t.size() > 1 )
		{
			for ( int k = atoi( t[1].c_str() ); k > 0; k-- )
				Frame();
		}
		else
		{
			fprintf( stderr, "case: unknown line: %s\n", l.c_str() );
			return 2;
		}
	}
	DumpConsole();
	SetAction( "ServerDeactivate" );
	g_dll.pfnServerDeactivate();
	if ( s_failedExpects )
		return 5;
	return g_stubReports ? 4 : 0;
}

// every case in its own child (a fresh hl.so); the summary line per case
static int RunCases( const char *hl, const std::vector<std::string> &files )
{
	int failed = 0;
	for ( const std::string &f : files )
	{
		std::vector<std::string> lines = ReadLines( f.c_str() );
		fflush( stderr );
		pid_t pid = fork();
		if ( pid == 0 )
		{
			__sanitizer_set_death_callback( OnDeath );
			s_rng.seed( 1 );
			int rc = RunCase( hl, lines );
			fflush( stdout );
			fflush( stderr );
			_exit( rc );
		}
		int status = 0;
		time_t started = time( NULL );
		while ( waitpid( pid, &status, WNOHANG ) == 0 )
		{
			if ( time( NULL ) - started > s_caseTimeout )
			{
				fprintf( stderr, "*** case %s hangs: killed\n", f.c_str() );
				kill( pid, SIGABRT );
				usleep( 2000000 );
				kill( pid, SIGKILL );
				waitpid( pid, &status, 0 );
				break;
			}
			usleep( 10000 );
		}
		bool ok = WIFEXITED( status ) && WEXITSTATUS( status ) == 0;
		// exit 4: engine-error conditions (ENGINE:), 5: a failed expect, 2/3: a bad case / hl.so; a signal: a crash
		if ( ok )
			fprintf( stderr, "case %s: ok\n", f.c_str() );
		else if ( WIFEXITED( status ) )
			fprintf( stderr, "case %s: FAILED (exit %d)\n", f.c_str(), WEXITSTATUS( status ) );
		else
			fprintf( stderr, "case %s: FAILED (signal %d)\n", f.c_str(), WIFSIGNALED( status ) ? WTERMSIG( status ) : 0 );
		failed += !ok;
	}
	fprintf( stderr, "fuzz_server: %zu cases, %d failed\n", files.size(), failed );
	return failed ? 1 : 0;
}

//------------------------------------------------------------------------------------------------- main

int main( int argc, char **argv )
{
	if ( argc < 7 )
	{
		fprintf( stderr, "usage: %s <hl.so> <gamedir> <commands.txt> <keys.txt> <classes.txt> <values.txt> "
			"[--mode clients|entities|all] [--seconds N] [--runs N] [--seed N] [--iterations N] [--verbose]\n", argv[0] );
		return 2;
	}
	const char *hl = argv[1];
	g_stubGameDir = argv[2];
	s_commands = ReadLines( argv[3] );
	s_keys = ReadLines( argv[4] );
	s_classes = ReadLines( argv[5] );
	s_values = ReadLines( argv[6] );
	std::string mode = "all", mapOverride;
	int seconds = 60, runs = 1000000;
	unsigned seed = (unsigned)time( NULL );
	for ( int i = 7; i < argc; i++ )
	{
		std::string a = argv[i];
		if ( a == "--mode" && i + 1 < argc ) mode = argv[++i];
		else if ( a == "--seconds" && i + 1 < argc ) seconds = atoi( argv[++i] );
		else if ( a == "--runs" && i + 1 < argc ) runs = atoi( argv[++i] );
		else if ( a == "--seed" && i + 1 < argc ) seed = (unsigned)strtoul( argv[++i], NULL, 10 );
		else if ( a == "--iterations" && i + 1 < argc ) s_iterations = atoi( argv[++i] );
		else if ( a == "--map" && i + 1 < argc ) mapOverride = argv[++i];
		else if ( a == "--verbose" ) g_stubVerbose = true;
		else if ( a == "--keep-going" ) s_keepGoing = true;
		else if ( a == "--drop-unknown-classes" ) s_dropUnknownClasses = true;
		else if ( a == "--case-timeout" && i + 1 < argc ) s_caseTimeout = atoi( argv[++i] );
	}
	if ( mode == "cases" )
	{
		std::vector<std::string> files;
		for ( int i = 7; i < argc; i++ )
			if ( std::string( argv[i] ).size() > 4 && std::string( argv[i] ).substr( std::string( argv[i] ).size() - 4 ) == ".txt" )
				files.push_back( argv[i] );
		return RunCases( hl, files );
	}
	if ( s_commands.empty() || s_keys.empty() || s_classes.empty() )
	{
		fprintf( stderr, "empty command / key / class list\n" );
		return 2;
	}
	fprintf( stderr, "fuzz_server: %zu commands, %zu keys, %zu classes, %zu values; mode %s, %d s, first seed %u\n",
		s_commands.size(), s_keys.size(), s_classes.size(), s_values.size(), mode.c_str(), seconds, seed );

	static const char *maps[] = { "co_fuzz", "dm_fuzz", "br_fuzz", "sp_fuzz" };
	time_t end = time( NULL ) + seconds;
	int done = 0, failed = 0, reported = 0;
	for ( int r = 0; r < runs && time( NULL ) < end; r++ )
	{
		unsigned runSeed = seed + r;
		const char *map = mapOverride.empty() ? maps[r % 4] : mapOverride.c_str();
		bool clients = mode == "clients" || ( mode == "all" && ( r / 4 ) % 2 == 0 );
		fflush( stderr );
		pid_t pid = fork();
		if ( pid == 0 )
		{
			__sanitizer_set_death_callback( OnDeath );
			s_rng.seed( runSeed );
			if ( !LoadServer( hl, map ) )
				_exit( 3 );
			if ( clients )
				RunClients();
			else
				RunEntities();
			SetAction( "ServerDeactivate" );
			g_dll.pfnServerDeactivate();
			_exit( g_stubReports ? 4 : 0 );
		}
		// a run that hangs (an endless loop in the game code) is a finding too
		int status = 0;
		time_t started = time( NULL );
		while ( waitpid( pid, &status, WNOHANG ) == 0 )
		{
			if ( time( NULL ) - started > 120 )
			{
				fprintf( stderr, "*** run %u (%s, %s) hangs: killed; reproduce: --seed %u --runs 1 --mode %s --map %s\n", runSeed,
					map, clients ? "clients" : "entities", runSeed, clients ? "clients" : "entities", map );
				// SIGABRT first: AddressSanitizer (handle_abort=1) prints where it hangs, the death callback the action
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
		if ( WIFEXITED( status ) && WEXITSTATUS( status ) == 4 )
		{
			reported++;
			fprintf( stderr, "--- run %u (%s, %s): engine-error conditions above\n", runSeed, map, clients ? "clients" : "entities" );
		}
		else if ( !WIFEXITED( status ) || WEXITSTATUS( status ) != 0 )
		{
			failed++;
			fprintf( stderr, "*** run %u (%s, %s) FAILED (status 0x%x); reproduce: --seed %u --runs 1 --mode %s --map %s\n",
				runSeed, map, clients ? "clients" : "entities", status, runSeed, clients ? "clients" : "entities", map );
		}
	}
	fprintf( stderr, "fuzz_server: %d runs, %d failed, %d with engine-error conditions\n", done, failed, reported );
	return ( failed || reported ) ? 1 : 0;
}
