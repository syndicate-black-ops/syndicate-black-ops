// Host for the overview file parser of the client (src/cl_dll/hud_spectator.cpp), see run_test.sh.
// The functions under test are the client's own text, cut out of the source by run_test.sh into overview_code.inc;
// this file gives them the few things of the client and the engine they use.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <strings.h>

#define stricmp strcasecmp
#define max(a, b) (((a) > (b)) ? (a) : (b))
typedef int qboolean;
typedef unsigned char byte;

struct vec3_t
{
	float v[3];
	float &operator[]( int i ) { return v[i]; }
};

struct model_s { int numframes; };

#define OBS_ROAMING		3
#define OBS_MAP_FREE	5
#define OBS_MAP_CHASE	6

int g_iUser1, g_iUser2;

#include "overview_defs.inc"	// OVERVIEW_* and overviewInfo_t, from hud_spectator.h

// ---- the engine
static std::string g_root, g_level;
static int g_loads, g_frees, g_sprites;
static std::vector<void *> g_open;

static const char *Eng_GetLevelName() { return g_level.c_str(); }

// exactly length + 1 bytes, as the engine's COM_LoadFile( path, 5, NULL ): a read or write past the file is caught
static byte *Eng_LoadFile( char *path, int usehunk, int *pLength )
{
	FILE *f = fopen( ( g_root + "/" + path ).c_str(), "rb" );
	if ( !f )
		return NULL;
	fseek( f, 0, SEEK_END );
	long length = ftell( f );
	fseek( f, 0, SEEK_SET );
	byte *buf = (byte *)malloc( length + 1 );
	if ( fread( buf, 1, length, f ) != (size_t)length ) { fclose( f ); free( buf ); return NULL; }
	fclose( f );
	buf[length] = 0;
	if ( pLength )
		*pLength = (int)length;
	g_loads++;
	g_open.push_back( buf );
	return buf;
}

// only a pointer COM_LoadFile gave, and only once: anything else is the heap corruption this test is for
static void Eng_FreeFile( void *buffer )
{
	if ( !buffer )
		return;
	for ( size_t i = 0; i < g_open.size(); i++ )
	{
		if ( g_open[i] == buffer )
		{
			g_open.erase( g_open.begin() + i );
			g_frees++;
			free( buffer );
			return;
		}
	}
	printf( "FAIL: COM_FreeFile of a pointer that is not an open file buffer\n" );
	abort();
}

// Quake's COM_Parse, as the engine's COM_ParseFile: token is the caller's 1024 bytes
static char *Eng_ParseFile( char *data, char *token )
{
	int c, len = 0;
	token[0] = 0;
	if ( !data )
		return NULL;
skipwhite:
	while ( ( c = (unsigned char)*data ) <= ' ' )
	{
		if ( c == 0 )
			return NULL;
		data++;
	}
	if ( c == '/' && data[1] == '/' )
	{
		while ( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}
	if ( c == '\"' )
	{
		data++;
		while ( 1 )
		{
			c = (unsigned char)*data++;
			if ( c == '\"' || !c )
			{
				token[len] = 0;
				return c ? data : data - 1;
			}
			if ( len < 1023 )
				token[len++] = c;
		}
	}
	if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' )
	{
		token[0] = c;
		token[1] = 0;
		return data + 1;
	}
	do
	{
		if ( len < 1023 )
			token[len++] = c;
		data++;
		c = (unsigned char)*data;
		if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' )
			break;
	} while ( c > 32 );
	token[len] = 0;
	return data;
}

// a sprite for every image that is there (.bmp / .tga), NULL otherwise, as the engine's loader
static model_s *Eng_LoadMapSprite( const char *name )
{
	static model_s sprites[64];
	size_t n = strlen( name );
	if ( n < 5 || ( strcasecmp( name + n - 4, ".bmp" ) && strcasecmp( name + n - 4, ".tga" ) ) )
		return NULL;
	FILE *f = fopen( ( g_root + "/" + name ).c_str(), "rb" );
	if ( !f )
		return NULL;
	fclose( f );
	model_s *s = &sprites[g_sprites++ % 64];
	s->numframes = 48;
	return s;
}

static void Eng_Printf( const char *fmt, ... ) {}

static struct
{
	const char *(*pfnGetLevelName)();
	byte *(*COM_LoadFile)( char *, int, int * );
	char *(*COM_ParseFile)( char *, char * );
	void (*COM_FreeFile)( void * );
	model_s *(*LoadMapSprite)( const char * );
	void (*Con_Printf)( const char *, ... );
} gEngfuncs = { Eng_GetLevelName, Eng_LoadFile, Eng_ParseFile, Eng_FreeFile, Eng_LoadMapSprite, Eng_Printf };

// ---- the part of CHudSpectator the functions use, with a guard after each array they write
#define GUARD 0x5B0C0DE5u
class CHudSpectator
{
public:
	bool ParseOverviewFile();
	void LoadMapSprites();
	bool IsLayerBelowView( int layer );

	unsigned		guard0;
	overviewInfo_t	m_OverviewData;
	unsigned		guard1;
	struct model_s	*m_MapSprite[OVERVIEW_MAX_LAYERS];
	unsigned		guard2;
	bool			m_bLayerBelowView[OVERVIEW_MAX_LAYERS];
	unsigned		guard3;
	float			m_mapZoom;
	vec3_t			m_mapOrigin;
	vec3_t			m_vecMainViewOrigin;
	bool			m_bWatchedHeightKnown;
	int				m_iWatchedPlayer;
	float			m_flWatchedHeight;
	int				m_iLayerViewKey;

	bool GuardsOk() const { return guard0 == GUARD && guard1 == GUARD && guard2 == GUARD && guard3 == GUARD; }
};

#include "overview_code.inc"	// ParseOverviewFile, LoadMapSprites, IsLayerBelowView, from hud_spectator.cpp

// ---- the test: <root> <map> [<expected layers drawn, or -1 for any>]
int main( int argc, char **argv )
{
	if ( argc < 3 )
		return 2;

	g_root = argv[1];
	g_level = std::string( "maps/" ) + argv[2] + ".bsp";
	int expect = argc > 3 ? atoi( argv[3] ) : -1;

	// on the heap, exactly sized: ASan sees a write past the object too
	CHudSpectator *spec = (CHudSpectator *)calloc( 1, sizeof( CHudSpectator ) );
	spec->guard0 = spec->guard1 = spec->guard2 = spec->guard3 = GUARD;

	// the client's order (CHudSpectator::Reset), twice: a second map load must behave the same
	for ( int pass = 0; pass < 2; pass++ )
	{
		bool parsed = spec->ParseOverviewFile();
		spec->LoadMapSprites();

		// every view, every height around the layers
		for ( g_iUser1 = 1; g_iUser1 <= OBS_MAP_CHASE; g_iUser1++ )
		{
			for ( int known = 0; known < 2; known++ )
			{
				spec->m_bWatchedHeightKnown = known != 0;
				spec->m_iWatchedPlayer = known;
				for ( float h = -5000.0f; h <= 5000.0f; h += 97.0f )
				{
					spec->m_flWatchedHeight = spec->m_vecMainViewOrigin[2] = h;
					for ( int layer = 0; layer < max( spec->m_OverviewData.layers, 1 ) && spec->m_OverviewData.layers > 1; layer++ )
						spec->IsLayerBelowView( layer );
				}
			}
		}

		if ( !spec->GuardsOk() )
		{
			printf( "FAIL %s: a write past an array of CHudSpectator\n", argv[2] );
			return 1;
		}
		if ( spec->m_OverviewData.layers < 0 || spec->m_OverviewData.layers > OVERVIEW_MAX_LAYERS )
		{
			printf( "FAIL %s: %d layers\n", argv[2], spec->m_OverviewData.layers );
			return 1;
		}
		for ( int i = 1; i < spec->m_OverviewData.layers; i++ )
		{
			if ( spec->m_OverviewData.layersHeights[i] < spec->m_OverviewData.layersHeights[i-1] )
			{
				printf( "FAIL %s: layers not sorted by HEIGHT\n", argv[2] );
				return 1;
			}
		}
		// a file read to its end is given back, once (a file with a syntax error is not, as in the SDK)
		if ( parsed && !g_open.empty() )
		{
			printf( "FAIL %s: file buffer not freed\n", argv[2] );
			return 1;
		}
		if ( expect >= 0 && spec->m_OverviewData.layers != expect )
		{
			printf( "FAIL %s: %d layers drawn, expected %d\n", argv[2], spec->m_OverviewData.layers, expect );
			return 1;
		}
		for ( size_t i = 0; i < g_open.size(); i++ )
			free( g_open[i] );
		g_open.clear();
	}

	printf( "ok   %-28s layers %d  loads %d frees %d\n", argv[2], spec->m_OverviewData.layers, g_loads, g_frees );
	free( spec );
	return 0;
}
