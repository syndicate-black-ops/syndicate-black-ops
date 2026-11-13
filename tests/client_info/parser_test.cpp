// Client info parsing test (tests/client_info/run_test.sh): the parsing of src/dlls/client_info.cpp, built alone.
#define CLIENT_INFO_TEST
#include "../../src/dlls/client_info.cpp"

#include <stdlib.h>
#include <random>
#include <string>

static int s_fail;

#define BAD "Bad CVAR request"

// cl: the cl_sbo_os answer (NULL: none came)
static void ExpectOs( const char *what, const char *hl25, const char *xash, const char *gl, const char *cl,
	const char *os, const char *engine, const char *renderer )
{
	const char *answers[ CIQ_COUNT ];
	answers[ CIQ_HL25 ] = hl25;
	answers[ CIQ_XASH ] = xash;
	answers[ CIQ_OPENGL ] = gl;
	answers[ CIQ_OS ] = cl;
	client_info_t info;
	memset( &info, 0x5a, sizeof( info ) );	// every field must be written
	ClientInfo_Classify( answers, &info );
	const char *gotOs = ClientInfo_SystemName( info.system );
	const char *gotEngine = ClientInfo_EngineName( info.engine );
	const char *gotRenderer = ClientInfo_RendererName( info.renderer );
	if ( strcmp( gotOs, os ) || strcmp( gotEngine, engine ) || strcmp( gotRenderer, renderer ) )
	{
		printf( "FAIL: %s -> %s / %s / %s, expected %s / %s / %s\n", what, gotOs, gotEngine, gotRenderer, os, engine,
			renderer );
		s_fail++;
	}
}

static void Expect( const char *what, const char *hl25, const char *xash, const char *gl, const char *os,
	const char *engine, const char *renderer )
{
	ExpectOs( what, hl25, xash, gl, NULL, os, engine, renderer );
}

static void ExpectVersion( const char *userinfo, const char *want )
{
	client_info_t info;
	memset( &info, 0x5a, sizeof( info ) );
	ClientInfo_SetVersion( &info, userinfo );
	char out[ 48 ];
	ClientInfo_VersionName( &info, out, sizeof( out ) );
	if ( strcmp( out, want ) )
	{
		printf( "FAIL: version \"%s\" -> \"%s\", expected \"%s\"\n", userinfo ? userinfo : "(null)", out, want );
		s_fail++;
	}
}

int main()
{
	// the query cvars
	if ( strcmp( ClientInfo_QueryCvar( CIQ_HL25 ), "sv_allow_shaders" ) || strcmp( ClientInfo_QueryCvar( CIQ_XASH ), "host_ver" )
		|| strcmp( ClientInfo_QueryCvar( CIQ_OPENGL ), "gl_fog" ) || strcmp( ClientInfo_QueryCvar( CIQ_OS ), "cl_sbo_os" ) || ClientInfo_QueryCvar( -1 ) || ClientInfo_QueryCvar( CIQ_COUNT ) )
	{
		printf( "FAIL: query cvars\n" );
		s_fail++;
	}

	// existence
	if ( ClientInfo_CvarExists( NULL ) || ClientInfo_CvarExists( "" ) || ClientInfo_CvarExists( BAD )
		|| !ClientInfo_CvarExists( "0" ) || !ClientInfo_CvarExists( "1" ) || !ClientInfo_CvarExists( "CVAR is privileged" ) )
	{
		printf( "FAIL: CvarExists\n" );
		s_fail++;
	}

	// engines (answers as each engine gives them)
	Expect( "HL25 OpenGL", "1", BAD, "1", "unknown", "HL25", "OpenGL" );
	Expect( "HL25 OpenGL, shaders off", "0", BAD, "0", "unknown", "HL25", "OpenGL" );
	Expect( "HL25 software", "1", BAD, BAD, "unknown", "HL25", "software" );
	Expect( "legacy OpenGL", BAD, BAD, "1", "unknown", "legacy", "OpenGL" );
	Expect( "legacy software", BAD, BAD, BAD, "unknown", "legacy", "software" );
	Expect( "Xash3D Linux", BAD, "4529 0.21 linux i386 b8c3e3a1", "1", "Linux", "Xash3D", "OpenGL" );
	Expect( "Xash3D Windows", BAD, "4529 0.21 win32 i386 b8c3e3a1", BAD, "Windows", "Xash3D", "software" );
	Expect( "Xash3D Windows 64", BAD, "4529 0.21 win64 amd64 b8c3e3a1", "1", "Windows", "Xash3D", "OpenGL" );
	Expect( "Xash3D Android", BAD, "4529 0.21 android arm 1", "1", "unknown", "Xash3D", "OpenGL" );
	Expect( "Xash3D short host_ver", BAD, "4529", "1", "unknown", "Xash3D", "OpenGL" );
	Expect( "Xash3D with sv_allow_shaders", "1", "4529 0.21 linux i386 1", "1", "Linux", "Xash3D", "OpenGL" );

	// the OS from SBO's client (cl_sbo_os); Xash3D keeps host_ver's
	ExpectOs( "HL25 Windows", "1", BAD, "1", "windows", "Windows", "HL25", "OpenGL" );
	ExpectOs( "HL25 Linux", "1", BAD, "1", "linux", "Linux", "HL25", "OpenGL" );
	ExpectOs( "legacy Windows software", BAD, BAD, BAD, "windows", "Windows", "legacy", "software" );
	ExpectOs( "older client: no cl_sbo_os", "1", BAD, "1", BAD, "unknown", "HL25", "OpenGL" );
	ExpectOs( "cl_sbo_os other value", "1", BAD, "1", "Linux ", "unknown", "HL25", "OpenGL" );
	ExpectOs( "cl_sbo_os empty", "1", BAD, "1", "", "unknown", "HL25", "OpenGL" );
	ExpectOs( "cl_sbo_os privileged", "1", BAD, "1", "CVAR is privileged", "unknown", "HL25", "OpenGL" );
	ExpectOs( "Xash3D: host_ver wins", BAD, "4529 0.21 win32 i386 1", "1", "linux", "Windows", "Xash3D", "OpenGL" );
	ExpectOs( "Xash3D Android: not cl_sbo_os", BAD, "4529 0.21 android arm 1", "1", "linux", "unknown", "Xash3D", "OpenGL" );
	ExpectOs( "only cl_sbo_os came", NULL, NULL, NULL, "linux", "Linux", "unknown", "unknown" );

	// missing answers (the wait ran out)
	Expect( "nothing", NULL, NULL, NULL, "unknown", "unknown", "unknown" );
	Expect( "no host_ver answer", "1", NULL, "1", "unknown", "unknown", "OpenGL" );
	Expect( "no sv_allow_shaders answer", NULL, BAD, NULL, "unknown", "unknown", "unknown" );
	Expect( "only host_ver, Xash3D", NULL, "1 2 linux", NULL, "Linux", "Xash3D", "unknown" );
	Expect( "empty answers", "", "", "", "unknown", "legacy", "software" );

	// the userinfo version
	ExpectVersion( "2.1.0", "SBO 2.1.0" );
	ExpectVersion( "2.1.0+1a2b3c4d", "SBO 2.1.0+1a2b3c4d" );
	ExpectVersion( "2.1.0+dirty@HEAD-1a2b3c4d", "SBO 2.1.0+dirty@HEAD-1a2b3c4d" );
	ExpectVersion( "", "none" );
	ExpectVersion( NULL, "none" );
	ExpectVersion( "2.1\"0 \\x", "SBO 2.1_0__x" );
	ExpectVersion( "123456789012345678901234567890123456789", "SBO 1234567890123456789012345678901" );

	// names of every value, in range and out
	for ( int v = -2; v < 8; v++ )
	{
		if ( !ClientInfo_SystemName( (HL_CLIENT_SYSTEM)v )[ 0 ] || !ClientInfo_EngineName( (HL_CLIENT_ENGINE_VERSION)v )[ 0 ]
			|| !ClientInfo_RendererName( (HL_CLIENT_RENDERER)v )[ 0 ] )
		{
			printf( "FAIL: empty name for %d\n", v );
			s_fail++;
		}
	}

	// random and hostile answers: must not crash (ASan/UBSan); the version never holds a quote, backslash or space
	std::mt19937 rng( 1234 );
	for ( int i = 0; i < 200000; i++ )
	{
		std::string s[ 5 ];
		for ( int k = 0; k < 5; k++ )
		{
			const int len = rng() % 80;
			for ( int j = 0; j < len; j++ )
			{
				const int r = rng() % 8;
				s[ k ] += r == 0 ? ' ' : r == 1 ? (char)( 1 + rng() % 255 ) : "linuxwindows 0.Bad CVAR request"[ rng() % 31 ];
			}
		}
		const char *answers[ CIQ_COUNT ];
		for ( int k = 0; k < CIQ_COUNT; k++ )
			answers[ k ] = ( rng() % 5 ) ? s[ k ].c_str() : NULL;
		client_info_t info;
		ClientInfo_Classify( answers, &info );
		ClientInfo_SetVersion( &info, s[ 4 ].c_str() );
		char out[ 48 ];
		ClientInfo_VersionName( &info, out, (int)( rng() % 48 ) + 1 );
		if ( strlen( info.version ) >= CLIENT_INFO_VERSION_LEN || strpbrk( info.version, "\"\\ \n" ) )
		{
			printf( "FAIL: version \"%s\"\n", info.version );
			s_fail++;
			break;
		}
	}

	if ( s_fail )
	{
		printf( "client_info parser: %d failures\n", s_fail );
		return 1;
	}
	printf( "client_info parser: all passed\n" );
	return 0;
}
