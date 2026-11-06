// Material registry test (src/pm_shared/pm_materials.c, docs/MATERIALS.md): unit tests, the game's own files, a
// fuzz run of every parser, and the 1.38 check over map BSPs. Built by tests/materials/run_test.sh with
// AddressSanitizer and UBSan.
//
//   materials_test <gamedir> [--fuzz SECONDS]
//   materials_test --maps <hl materials.txt> <gamedir> <bsp>...   (the 1.38 check, see run_test.sh)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "pm_materials.h"

static int g_failures;
static int g_logLines;
static int g_quiet;

#define CHECK( cond ) do { if ( !( cond ) ) { printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond ); g_failures++; } } while ( 0 )

static char g_logText[ 1024 ];		// the log lines since it was last cleared (as long as they fit)

static void TestLog( const char *text )
{
	size_t n = strlen( g_logText );

	g_logLines++;
	snprintf( g_logText + n, sizeof( g_logText ) - n, "%s", text );
	if ( !g_quiet )
		printf( "  log: %s", text );
}

static unsigned char *ReadFile( const char *path, int *length )
{
	FILE *f = fopen( path, "rb" );
	unsigned char *data;
	long n;

	if ( !f )
		return NULL;
	fseek( f, 0, SEEK_END );
	n = ftell( f );
	fseek( f, 0, SEEK_SET );
	data = (unsigned char *)malloc( n > 0 ? n : 1 );
	if ( n > 0 && fread( data, 1, n, f ) != (size_t)n )
	{
		fclose( f );
		free( data );
		return NULL;
	}
	fclose( f );
	*length = (int)n;
	return data;
}

static char g_gamedir[ 1024 ];

static const char *g_valveList;		// Half-Life's sound/materials.txt (the engine's fallback to valve/)

static unsigned char *GameLoad( const char *path, int *length )
{
	char full[ 2048 ];
	unsigned char *data;

	snprintf( full, sizeof( full ), "%s/%s", g_gamedir, path );
	data = ReadFile( full, length );
	if ( !data && g_valveList && !strcmp( path, MAT_TEXTURE_LIST ) )
		data = ReadFile( g_valveList, length );
	return data;
}

// an in-memory file system for the map file tests
static const char *g_memFiles[ 16 ][ 2 ];

static unsigned char *MemLoad( const char *path, int *length )
{
	int i;

	for ( i = 0; i < 16 && g_memFiles[ i ][ 0 ]; i++ )
	{
		if ( !strcmp( g_memFiles[ i ][ 0 ], path ) )
		{
			int n = (int)strlen( g_memFiles[ i ][ 1 ] );
			unsigned char *data = (unsigned char *)malloc( n ? n : 1 );

			memcpy( data, g_memFiles[ i ][ 1 ], n );
			*length = n;
			return data;
		}
	}
	return NULL;
}

static int g_filesSeen;

static void CountFile( const char *path, void *ctx )
{
	(void)path; (void)ctx;
	g_filesSeen++;
}

static int g_fxSeen;
static char g_fxNames[ 512 ];

static void CountFxFile( const char *path, void *ctx )
{
	size_t n = strlen( g_fxNames );

	(void)ctx;
	snprintf( g_fxNames + n, sizeof( g_fxNames ) - n, "%s ", path );
	g_fxSeen++;
}

static void CountFx( const mat_fx_t *fx, void *ctx )
{
	MAT_ForEachFxFile( fx, CountFxFile, ctx );
}

static void GameFree( unsigned char *data )
{
	free( data );
}

static int ParseList( const char *text, int layer )
{
	return MAT_ParseTextureList( text, (int)strlen( text ), layer, "test" );
}

static int ParseTypes( const char *text )
{
	return MAT_ParseTypes( text, (int)strlen( text ), "test" );
}

// ---------------------------------------------------------------------------------------------------------------

static void TestBuiltins( void )
{
	const mat_step_t *step;
	const mat_hit_t *hit;

	MAT_Reset();

	hit = MAT_GetHit( 'C' );
	CHECK( hit->sounds.count == 2 && !strcmp( hit->sounds.name[ 0 ], "player/pl_step1.wav" ) );
	CHECK( hit->volume == 0.9f && hit->volume_bar == 0.6f && hit->attenuation == 0.8f );
	CHECK( MAT_GetHit( 'V' )->sounds.count == 2 && !strcmp( MAT_GetHit( 'V' )->sounds.name[ 1 ], "player/pl_duct1.wav" ) );
	CHECK( MAT_GetHit( 'V' )->volume == 0.5f );
	CHECK( MAT_GetHit( 'F' )->attenuation == 1.0f && MAT_GetHit( 'F' )->volume == 1.0f );
	CHECK( MAT_GetHit( 'P' )->sparks == 1 && MAT_GetHit( 'Y' )->sparks == 0 );
	CHECK( MAT_GetHit( 'S' )->volume_bar == 0.0f );
	CHECK( MAT_GetHit( 'Q' ) == MAT_GetHit( 'C' ) );			// undefined letter: concrete
	CHECK( MAT_GetHit( 0 ) == MAT_GetHit( 'C' ) );
	CHECK( MAT_GetHit( 'A' ) == MAT_GetHit( 'C' ) );			// new letters come from the data file

	step = MAT_GetStep( 'D' );
	CHECK( step->walk_volume == 0.25f && step->run_volume == 0.55f && step->walk_time == 400 && step->run_time == 300 );
	step = MAT_GetStep( 'V' );
	CHECK( step->walk_volume == 0.4f && step->run_volume == 0.7f );
	step = MAT_GetStep( 'T' );
	CHECK( step->right.count == 5 && !strcmp( step->right.name[ 4 ], "player/pl_tile5.wav" ) );
	CHECK( MAT_GetStep( 'W' ) == MAT_GetStep( 'C' ) );			// HL: wood, glass, computer walk like concrete
	CHECK( MAT_GetStep( 'Y' ) == MAT_GetStep( 'C' ) );
	CHECK( MAT_GetStep( 'p' ) == MAT_GetStep( 'C' ) );
	CHECK( MAT_GetStep( 'm' ) != MAT_GetStep( 'C' ) );			// lower case letters work
	CHECK( MAT_GetLadderStep()->walk_volume == 0.35f && MAT_GetLadderStep()->run_time == 350 );
	CHECK( MAT_GetWadeStep()->skip_steps == 1 && MAT_GetWadeStep()->walk_time == 600 );
	CHECK( !strcmp( MAT_GetWadeStep()->right.name[ 1 ], "player/pl_wade2.wav" ) );
	CHECK( !strcmp( MAT_GetSloshStep()->left.name[ 0 ], "player/pl_slosh2.wav" ) );

	MAT_SetUsable( 'M', 0 );
	CHECK( MAT_GetStep( 'M' ) == MAT_GetStep( 'C' ) );
	CHECK( MAT_GetHit( 'M' ) == MAT_GetHit( 'C' ) );
	MAT_SetUsable( 'M', 1 );
	CHECK( MAT_GetStep( 'M' ) != MAT_GetStep( 'C' ) );
}

static void TestStrip( void )
{
	char out[ CBTEXTURENAMEMAX ];

	MAT_StripTextureName( "+0~LIGHT1", out );	CHECK( !strcmp( out, "LIGHT1" ) );
	MAT_StripTextureName( "-1ROCK", out );		CHECK( !strcmp( out, "ROCK" ) );
	MAT_StripTextureName( "{FENCE", out );		CHECK( !strcmp( out, "FENCE" ) );
	MAT_StripTextureName( "!WATER", out );		CHECK( !strcmp( out, "WATER" ) );
	MAT_StripTextureName( "+", out );			CHECK( !strcmp( out, "+" ) );
	MAT_StripTextureName( "{", out );			CHECK( !strcmp( out, "" ) );
	MAT_StripTextureName( "", out );			CHECK( !strcmp( out, "" ) );
	MAT_StripTextureName( NULL, out );			CHECK( !strcmp( out, "" ) );
	MAT_StripTextureName( "ABCDEFGHIJKLMNOPQRSTUVWXYZ", out );
	CHECK( !strcmp( out, "ABCDEFGHIJKLMNO" ) );
}

static void TestTextureList( void )
{
	int i;
	char big[ 64 * 1100 ];
	char matched[ CBTEXTURENAMEMAX ];
	int layer;

	MAT_Reset();
	CHECK( ParseList( "// comment\r\nM METAL1\r\nd dirt1\r\n  T\tTILE_A  trailing\r\n", 0 ) == 3 );
	CHECK( MAT_FindTextureType( "metal1" ) == 'M' );
	CHECK( MAT_FindTextureType( "DIRT1" ) == 'D' );
	CHECK( MAT_FindTextureType( "tile_a" ) == 'T' );
	CHECK( MAT_FindTextureType( "nothing" ) == 'C' );
	CHECK( MAT_FindTextureType( "" ) == 'C' );
	CHECK( MAT_FindTextureType( NULL ) == 'C' );

	// no space after the letter, no name, a name of 16 characters: refused
	g_quiet = 1;
	CHECK( ParseList( "MX NAME\nM\nM \n1 NUM\nM ABCDEFGHIJKLMNOP\n", 0 ) == 0 );
	g_quiet = 0;

	// last line without a newline, NUL bytes end a line
	MAT_Reset();
	CHECK( MAT_ParseTextureList( "W LAST", 6, 0, "test" ) == 1 && MAT_FindTextureType( "LAST" ) == 'W' );
	MAT_Reset();	// a whole file whose last entry has no newline (Featureful fixed Valve's reader skipping it)
	CHECK( MAT_ParseTextureList( "M FIRST\r\nT SECOND\r\nW LASTLINE", 29, 0, "test" ) == 3
		&& MAT_FindTextureType( "LASTLINE" ) == 'W' );
	CHECK( MAT_ParseTextureList( "G NUL1\0G NUL2\n", 14, 0, "test" ) == 2 && MAT_FindTextureType( "NUL2" ) == 'G' );

	// /* */ blocks over lines (Valve's file ends with one)
	MAT_Reset();
	CHECK( ParseList( "/*\n\tcase CHAR_TEX: x\n M INSIDE\n*/\nM OUTSIDE\n/* one line */\nD AFTER\n/**/\nD AFTER2\n", 0 ) == 3 );
	CHECK( MAT_FindTextureType( "INSIDE" ) == 'C' && MAT_FindTextureType( "AFTER2" ) == 'D' );

	// whole names and HL's 12 characters
	MAT_Reset();
	ParseList( "M ABCDEFGHIJKLM\nW ABCDEFGHIJKLX\nT SHORT12CHARS\nG TWELVECHARSX\n", 0 );
	CHECK( MAT_FindTextureType( "ABCDEFGHIJKLM" ) == 'M' );
	CHECK( MAT_FindTextureType( "ABCDEFGHIJKLX" ) == 'W' );
	CHECK( MAT_FindTextureType( "ABCDEFGHIJKLZ" ) == 'M' );		// HL: equal on 12, the first of the file
	CHECK( MAT_FindTextureType( "ABCDEFGHIJKL" ) == 'M' );
	CHECK( MAT_FindTextureType( "TWELVECHARSX" ) == 'G' );
	CHECK( MAT_FindTextureType( "TWELVECHARSXYZ" ) == 'G' );	// a longer texture name matched on 12 (HL)
	CHECK( MAT_FindTextureType( "TWELVECHARS" ) == 'C' );

	// duplicates: first in a file, a later layer overrides
	MAT_Reset();
	ParseList( "G GENERIC015V\nM GENERIC015V\n", 0 );
	CHECK( MAT_FindTextureType( "generic015v" ) == 'G' );
	ParseList( "Y GENERIC015V\nY NEWONE\n", 1 );
	CHECK( MAT_FindTextureMatch( "GENERIC015V", matched, &layer ) == 'Y' && layer == 1 && !strcmp( matched, "GENERIC015V" ) );
	CHECK( MAT_FindTextureType( "NEWONE" ) == 'Y' && MAT_TextureCount() == 2 );
	CHECK( MAT_FindTextureMatch( "nope", matched, &layer ) == 'C' && layer == -1 && !matched[ 0 ] );

	// the table holds MAT_TEXTURES_MAX entries
	MAT_Reset();
	big[ 0 ] = 0;
	{
		int n = 0;

		for ( i = 0; i < 1100; i++ )
			n += sprintf( big + n, "M TEX%05d\n", i );
	}
	g_quiet = 1;
	ParseList( big, 0 );
	g_quiet = 0;
	CHECK( MAT_TextureCount() == MAT_TEXTURES_MAX );
	CHECK( MAT_FindTextureType( "TEX00000" ) == 'M' && MAT_FindTextureType( "TEX01023" ) == 'M' );
	CHECK( MAT_FindTextureType( "TEX01024" ) == 'C' );
	ParseList( "W TEX00005\n", 1 );		// an override needs no room
	CHECK( MAT_FindTextureType( "TEX00005" ) == 'W' );
}

static void TestTypes( void )
{
	MAT_Reset();
	CHECK( ParseTypes(
		"// types\n"
		"material a\n{\n name \"grass\"\n step_right { \"player/pl_grass1.wav\" \"player/pl_grass3.wav\" }\n"
		" step_left { player/pl_grass2.wav player/pl_grass4.wav }\n walk_volume 0.3 run_time 250\n"
		" hit { \"debris/a.wav\" } hit_volume 2 hit_attenuation -1\n future_key { 1 2 3 } other 5\n}\n"
		"ladder { walk_volume .5 }\n"
		"material M { hit_volume 0.25 }\n" ) == 1 );
	CHECK( MAT_GetType( 'A' ) && !strcmp( MAT_GetType( 'A' )->name, "grass" ) );
	CHECK( MAT_GetStep( 'A' )->walk_volume == 0.3f && MAT_GetStep( 'A' )->run_time == 250 && MAT_GetStep( 'A' )->walk_time == 400 );
	CHECK( !strcmp( MAT_GetStep( 'A' )->left.name[ 1 ], "player/pl_grass4.wav" ) );
	CHECK( MAT_GetHit( 'A' )->volume == 1.0f && MAT_GetHit( 'A' )->attenuation == 0.0f );	// clamped
	CHECK( MAT_GetLadderStep()->walk_volume == 0.5f && MAT_GetLadderStep()->right.count == 2 );
	CHECK( MAT_GetHit( 'M' )->volume == 0.25f && MAT_GetHit( 'M' )->sounds.count == 2 );	// other keys kept

	// errors: nothing of the file is used
	g_quiet = 1;
	{
		static const char *bad[] = {
			"material A { step_right { \"x.wav\" } }",										// no step_left
			"material A { hit_volume 0.5 }",												// hit values without sounds
			"material A { hit { \"../evil.wav\" } }",
			"material A { hit { \"/abs.wav\" } }",
			"material A { hit { \"a\\\\b.wav\" } }",
			"material A { hit { \"music.mp3\" } }",
			"material A { hit { \"a//b.wav\" } }",
			"material A { hit { \"a.wav\" \"b.wav\" \"c.wav\" \"d.wav\" \"e.wav\" \"f.wav\" \"g.wav\" } }",
			"material A { hit { \"a.wav\" } hit_volume inf }",
			"material A { hit { \"a.wav\" } hit_volume nan }",
			"material A { hit { \"a.wav\" } hit_volume 1e5 }",
			"material A { hit { \"a.wav\" } hit_volume }",
			"material A { name { } }",
			"material A { unknown { { } } }",
			"material AB { }",
			"material 1 { }",
			"material { }",
			"material A",
			"material A {",
			"material A { name \"unterminated }",
			"material A { name \"line\nbreak\" }",
			"stuff",
			"\"material\" A { }",
			"}",
			"material C { step_right { } }",
			"ladder { step_left { } }",
			"material A { name \"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\" }",
			NULL
		};
		int i;

		for ( i = 0; bad[ i ]; i++ )
		{
			MAT_Reset();
			if ( ParseTypes( bad[ i ] ) != 0 || MAT_GetType( 'A' ) != NULL )
			{
				printf( "FAIL: accepted bad types file %d: %s\n", i, bad[ i ] );
				g_failures++;
			}
			CHECK( MAT_GetLadderStep()->left.count == 2 && MAT_GetStep( 'C' )->right.count == 2 );
		}
		MAT_Reset();
		CHECK( MAT_ParseTypes( "material A { name \"x\0\" }", 23, "test" ) == 0 );
	}
	g_quiet = 0;

	// impact effects and texture blocks
	MAT_Reset();
	CHECK( ParseTypes(
		"material W { fx_debris { \"sprites/a.spr\" \"sprites/b.spr\" } fx_debris_count 9 fx_debris_color 1 2 300\n"
		" fx_smoke 1 fx_smoke_color 10 20 30 fx_decal \"{bhitwood\" fx_ricochet { \"debris/r.wav\" } }\n"
		"texture \"{GLASS1\" { fx_sparks 1 }\n"
		"texture glass1 { fx_decal none }\n" ) == 1 );
	CHECK( MAT_GetFx( 'W', "X" )->defined && MAT_GetFx( 'W', "X" )->debris_sprites == 2 && MAT_GetFx( 'W', NULL )->debris_count == 8 );
	CHECK( MAT_GetFx( 'W', NULL )->debris_color[ 2 ] == 255.0f && !strcmp( MAT_GetFx( 'W', NULL )->decal, "{bhitwood" ) );
	CHECK( MAT_GetFx( 'W', NULL )->ricochet.count == 1 );
	CHECK( !MAT_GetFx( 'C', NULL )->defined && !MAT_GetFx( 'Q', NULL )->defined && !MAT_GetFx( 0, "" )->defined );
	CHECK( MAT_GetFx( 'W', "GLASS1" )->defined && !MAT_GetFx( 'W', "GLASS1" )->sparks );	// the later block replaces
	CHECK( !strcmp( MAT_GetFx( 'C', "glass1" )->decal, "none" ) );
	g_quiet = 1;
	{
		static const char *badfx[] = {
			"material W { fx_debris { \"sprites/a.wav\" } }",
			"material W { fx_debris { \"../a.spr\" } }",
			"material W { fx_debris { \"a1.spr\" \"a2.spr\" \"a3.spr\" \"a4.spr\" \"a5.spr\" } }",
			"material W { fx_debris_color 1 2 }",
			"material W { fx_decal \"waytoolongdecalname\" }",
			"material W { fx_decal \"a b\" }",
			"material W { fx_ricochet { \"x.spr\" } }",
			"texture { fx_sparks 1 }",
			"texture ABCDEFGHIJKLMNOP { }",
			"texture A { fx_sparks }",
			NULL
		};
		char many[ 4096 ];
		int i, n = 0;

		for ( i = 0; badfx[ i ]; i++ )
		{
			MAT_Reset();
			if ( ParseTypes( badfx[ i ] ) != 0 || MAT_GetFx( 'W', "A" )->defined )
			{
				printf( "FAIL: accepted bad effects %d: %s\n", i, badfx[ i ] );
				g_failures++;
			}
		}
		for ( i = 0; i < 33; i++ )
			n += snprintf( many + n, sizeof( many ) - n, "texture T%d { fx_smoke 1 }\n", i );
		MAT_Reset();
		CHECK( ParseTypes( many ) == 0 );		// 33 texture blocks: refused
	}
	g_quiet = 0;

	CHECK( MAT_IsSafeSoundPath( "player/pl_step1.wav" ) );
	CHECK( MAT_IsSafeSoundPath( "a.WAV" ) );
	CHECK( !MAT_IsSafeSoundPath( ".wav" ) );
	CHECK( !MAT_IsSafeSoundPath( "x y.wav" ) );
	CHECK( !MAT_IsSafeSoundPath( "./a.wav" ) );
	CHECK( !MAT_IsSafeSoundPath( "a/../b.wav" ) );
	CHECK( !MAT_IsSafeSoundPath( "C:x.wav" ) );
	CHECK( !MAT_IsSafeSoundPath( NULL ) );
	CHECK( !MAT_IsSafeSoundPath( "0123456789012345678901234567890123456789012345678901234567890.wav" ) );
}

// ---------------------------------------------------------------------------------------------------------------
// BSP texture lump
// ---------------------------------------------------------------------------------------------------------------

static void PutInt( unsigned char *p, int v )
{
	p[ 0 ] = (unsigned char)v; p[ 1 ] = (unsigned char)( v >> 8 ); p[ 2 ] = (unsigned char)( v >> 16 ); p[ 3 ] = (unsigned char)( v >> 24 );
}

// version 30, texture lump at 124 with three miptex headers (one missing)
static int MakeBsp( unsigned char *buf, int size )
{
	int lump = 4 + 15 * 8, n = 3, i, pos;

	memset( buf, 0, size );
	PutInt( buf, 30 );
	PutInt( buf + 4 + 2 * 8, lump );
	pos = 4 + 4 * n;
	PutInt( buf + lump, n );
	for ( i = 0; i < n; i++ )
	{
		if ( i == 1 )
		{
			PutInt( buf + lump + 4 + i * 4, -1 );
			continue;
		}
		PutInt( buf + lump + 4 + i * 4, pos );
		snprintf( (char *)buf + lump + pos, 16, i == 0 ? "+0~METALX" : "{GRASSY" );
		pos += 40;
	}
	PutInt( buf + 4 + 2 * 8 + 4, pos );
	return lump + pos;
}

static int g_bspNames;
static char g_bspLast[ 64 ];

static void BspCallback( const char *name, void *ctx )
{
	(void)ctx;
	g_bspNames++;
	snprintf( g_bspLast, sizeof( g_bspLast ), "%s", name );
	CHECK( strlen( name ) <= 16 );
}

static void TestBsp( void )
{
	unsigned char buf[ 1024 ];
	int length = MakeBsp( buf, sizeof( buf ) );

	g_bspNames = 0;
	CHECK( MAT_ForEachBspTexture( buf, length, BspCallback, NULL ) == 2 && g_bspNames == 2 );
	CHECK( !strcmp( g_bspLast, "{GRASSY" ) );

	CHECK( MAT_ForEachBspTexture( buf, 100, BspCallback, NULL ) == -1 );		// shorter than the header
	CHECK( MAT_ForEachBspTexture( buf, length - 1, BspCallback, NULL ) == -1 );	// lump past the end
	CHECK( MAT_ForEachBspTexture( NULL, length, BspCallback, NULL ) == -1 );

	PutInt( buf, 29 );
	CHECK( MAT_ForEachBspTexture( buf, length, BspCallback, NULL ) == -1 );
	length = MakeBsp( buf, sizeof( buf ) );
	PutInt( buf + 4 + 16, -5 );
	CHECK( MAT_ForEachBspTexture( buf, length, BspCallback, NULL ) == -1 );
	length = MakeBsp( buf, sizeof( buf ) );
	PutInt( buf + 124, 0x7fffffff );
	CHECK( MAT_ForEachBspTexture( buf, length, BspCallback, NULL ) == -1 );
	length = MakeBsp( buf, sizeof( buf ) );
	PutInt( buf + 124 + 4, 0x7ffffff0 );			// miptex offset past the lump: skipped
	g_bspNames = 0;
	CHECK( MAT_ForEachBspTexture( buf, length, BspCallback, NULL ) == 1 );
	length = MakeBsp( buf, sizeof( buf ) );
	memset( buf + 124 + 16, 'A', 16 );				// a name without terminator
	g_bspNames = 0;
	CHECK( MAT_ForEachBspTexture( buf, length, BspCallback, NULL ) == 2 );
}

// ---------------------------------------------------------------------------------------------------------------
// the game's files
// ---------------------------------------------------------------------------------------------------------------

static void TestMapFiles( void )
{
	static const char *files[][ 2 ] = {
		{ "materials/maps/a.txt", "#include materials/shared.txt\nM SHARED1\n#types \"materials/extra.txt\"\nC OWN1\n" },
		{ "materials/shared.txt", "W SHARED1\nW SHARED2\n#include materials/deeper.txt\n#include materials/shared.txt\n" },
		{ "materials/deeper.txt", "T DEEP1\nT SHARED2\n#include materials/deepest.txt\n" },
		{ "materials/deepest.txt", "T DEEPEST\n" },
		{ "materials/extra.txt", "material W { step_right { \"w1.wav\" } step_left { \"w2.wav\" } }\n"
			"material K { fx_debris { \"sprites/k1.spr\" \"sprites/k2.spr\" } fx_ricochet { \"k/ric1.wav\" } }\n"
			"texture \"{KGLASS\" { fx_debris { \"sprites/k3.spr\" } }\n" },
		{ "materials/maps/bad.txt", "#include ../../etc/passwd.txt\n#include materials/two.dots.txt\n#types /abs.txt\n#what x.txt\n#include\nG OK1\n" },
		{ "materials/maps/self.txt", "#include materials/maps/self.txt\nG SELF\n" },
		{ NULL, NULL }
	};
	int i;

	memset( g_memFiles, 0, sizeof( g_memFiles ) );
	for ( i = 0; files[ i ][ 0 ]; i++ )
	{
		g_memFiles[ i ][ 0 ] = files[ i ][ 0 ];
		g_memFiles[ i ][ 1 ] = files[ i ][ 1 ];
	}

	MAT_Reset();
	ParseList( "C SHARED2\nY GLOBAL1\n", 0 );
	g_filesSeen = 0;
	g_quiet = 1;
	CHECK( MAT_LoadMapFile( MemLoad, GameFree, "materials/maps/a.txt", CountFile, NULL ) == 4 );
	g_quiet = 0;
	CHECK( g_filesSeen == 4 );											// map, shared, deeper, types (deepest: too deep)
	CHECK( MAT_FindTextureType( "SHARED1" ) == 'M' );					// the map's own line beats its include
	CHECK( MAT_FindTextureType( "SHARED2" ) == 'W' );					// include beats the nested include
	CHECK( MAT_FindTextureType( "DEEP1" ) == 'T' );
	CHECK( MAT_FindTextureType( "DEEPEST" ) == 'C' );					// 3 deep: not read
	CHECK( MAT_FindTextureType( "GLOBAL1" ) == 'Y' );
	CHECK( MAT_FindTextureType( "OWN1" ) == 'C' );
	CHECK( MAT_GetStep( 'W' ) != MAT_GetStep( 'C' ) && !strcmp( MAT_GetStep( 'W' )->right.name[ 0 ], "w1.wav" ) );

	// the effect files a types file names (the server's download list): sprites as written, ricochets under sound/
	g_fxSeen = 0;
	g_fxNames[ 0 ] = 0;
	MAT_ForEachFx( CountFx, NULL );
	CHECK( g_fxSeen == 4 );
	CHECK( !strcmp( g_fxNames, "sprites/k1.spr sprites/k2.spr sound/k/ric1.wav sprites/k3.spr " ) );
	g_fxSeen = 0;
	MAT_ForEachFxFile( MAT_GetFx( 'C', NULL ), CountFxFile, NULL );		// no effect keys: nothing
	MAT_ForEachFxFile( NULL, CountFxFile, NULL );
	CHECK( g_fxSeen == 0 );

	MAT_Reset();
	g_quiet = 1;
	CHECK( MAT_LoadMapFile( MemLoad, GameFree, "materials/maps/bad.txt", NULL, NULL ) == 1 );
	CHECK( MAT_FindTextureType( "OK1" ) == 'G' );
	CHECK( MAT_LoadMapFile( MemLoad, GameFree, "materials/maps/self.txt", NULL, NULL ) == 1 );
	CHECK( MAT_LoadMapFile( MemLoad, GameFree, "materials/maps/missing.txt", NULL, NULL ) == -1 );
	CHECK( MAT_LoadMapFile( MemLoad, GameFree, "../materials/maps/a.txt", NULL, NULL ) == -1 );
	CHECK( MAT_LoadMapFile( MemLoad, GameFree, NULL, NULL, NULL ) == -1 );
	g_quiet = 0;

	CHECK( MAT_IsSafeDataPath( "materials/maps/co_iss-555-cargo.txt" ) );
	CHECK( !MAT_IsSafeDataPath( "materials/maps/a.b.txt" ) );
	CHECK( !MAT_IsSafeDataPath( "maps/a_materials.cfg" ) );
	CHECK( !MAT_IsSafeDataPath( "maps\\a.txt" ) );
	CHECK( !MAT_IsSafeDataPath( "/maps/a.txt" ) );
	CHECK( !MAT_IsSafeDataPath( "maps//a.txt" ) );
	CHECK( !MAT_IsSafeDataPath( "maps/0123456789012345678901234567890123456789012345678901234567.txt" ) );
}

// who wins (docs/MATERIALS.md "Who wins"): Valve's sound/materials.txt < materials/textures.txt < a map's file, whose
// own lines beat its includes; letters: materials/types.txt < the included files' blocks and #types < the map
// file's own blocks and #types, in file order. Inline blocks in the map file, mixed with texture lines.
static void TestPrecedence( void )
{
	static const char *files[][ 2 ] = {
		{ MAT_TEXTURE_LIST, "C VALVEONLY\nC BOTH\nC ALL3\nG INC\n" },
		{ MAT_TEXTURES_FILE, "M BOTH\nM ALL3\nT GLOBALONLY\n" },
		{ MAT_TYPES_FILE, "material K { step_right { \"g1.wav\" } step_left { \"g2.wav\" } hit { \"gh.wav\" }\n"
			"  fx_debris { \"sprites/g.spr\" } }\n"
			"material E { step_right { \"e1.wav\" } step_left { \"e2.wav\" } }\n" },
		{ "materials/maps/p.txt",
			"// a mapper's single file: tags, inline blocks, an include and a types file\n"
			"G ALL3\n"
			"material K\n"
			"{\n"
			"\tstep_right { \"m1.wav\" }   // the map's block beats its include's and types.txt\n"
			"\tstep_left { \"m2.wav\" } name \"a } b\"\n"		/* a brace inside a string does not end the block */
			"}\n"
			"texture \"{PGLASS\" { fx_sparks 1 } texture \"PMETAL\" { fx_decal none }\n"
			"#include materials/p_shared.txt\n"
			"#types materials/p_types.txt\n"
			"Y INC\n"
			"material Q\n"
			"{\n"
			"\tstep_right { \"q1.wav\" } step_left { \"q2.wav\" }\n"
			"\twalk_volume abc\n"		/* line 15: an error, this block is not used */
			"}\n"
			"x AFTER\n" },
		{ "materials/p_shared.txt", "W INC\nW ALL3\nmaterial K { step_right { \"s1.wav\" } step_left { \"s2.wav\" } hit { \"sh.wav\" } }\n"
			"material E { step_right { \"se1.wav\" } step_left { \"se2.wav\" } }\n" },
		{ "materials/p_types.txt", "material E { hit { \"pt.wav\" } }\n" },
		{ NULL, NULL }
	};
	int i, layer;
	char matched[ CBTEXTURENAMEMAX ];

	memset( g_memFiles, 0, sizeof( g_memFiles ) );
	for ( i = 0; files[ i ][ 0 ]; i++ )
	{
		g_memFiles[ i ][ 0 ] = files[ i ][ 0 ];
		g_memFiles[ i ][ 1 ] = files[ i ][ 1 ];
	}

	// the global files alone
	MAT_LoadGlobal( MemLoad, GameFree );
	CHECK( MAT_FindTextureType( "VALVEONLY" ) == 'C' );
	CHECK( MAT_FindTextureMatch( "BOTH", matched, &layer ) == 'M' && layer == MAT_LAYER_GLOBAL );	// textures.txt > Valve
	CHECK( MAT_FindTextureType( "GLOBALONLY" ) == 'T' );
	CHECK( !strcmp( MAT_GetStep( 'K' )->right.name[ 0 ], "g1.wav" ) );

	// with the map's file
	g_quiet = 1;
	g_logLines = 0;
	g_logText[ 0 ] = 0;
	CHECK( MAT_LoadMapFile( MemLoad, GameFree, "materials/maps/p.txt", NULL, NULL ) == 3 );
	g_quiet = 0;
	// the broken block, on its own line of the file, and nothing else
	CHECK( g_logLines == 2 && strstr( g_logText, "materials/maps/p.txt line 15:" ) && strstr( g_logText, "materials/maps/p.txt not used" ) );
	CHECK( MAT_FindTextureMatch( "ALL3", matched, &layer ) == 'G' && layer == MAT_LAYER_MAP + MAT_INCLUDE_DEPTH );	// map > include > textures.txt > Valve
	CHECK( MAT_FindTextureMatch( "INC", matched, &layer ) == 'Y' && layer == MAT_LAYER_MAP + MAT_INCLUDE_DEPTH );	// the map's own line, after its #include
	CHECK( MAT_FindTextureType( "BOTH" ) == 'M' );								// untouched by the map: textures.txt
	CHECK( MAT_FindTextureType( "AFTER" ) == 'X' );							// texture lines after the blocks are read
	CHECK( !strcmp( MAT_GetStep( 'K' )->right.name[ 0 ], "m1.wav" ) );			// map block > include block > types.txt
	CHECK( MAT_GetType( 'K' ) && !strcmp( MAT_GetType( 'K' )->name, "a } b" ) );
	CHECK( !strcmp( MAT_GetHit( 'K' )->sounds.name[ 0 ], "sh.wav" ) );			// the include's key the map did not name
	CHECK( MAT_GetFx( 'K', NULL )->debris_sprites == 1 && !strcmp( MAT_GetFx( 'K', NULL )->debris_sprite[ 0 ], "sprites/g.spr" ) );	// types.txt's key nobody changed
	CHECK( !strcmp( MAT_GetStep( 'E' )->right.name[ 0 ], "se1.wav" ) );		// the include's block > types.txt
	CHECK( !strcmp( MAT_GetHit( 'E' )->sounds.name[ 0 ], "pt.wav" ) );			// the map's #types
	CHECK( MAT_GetFx( 'C', "PGLASS" )->sparks && !strcmp( MAT_GetFx( 'C', "PMETAL" )->decal, "none" ) );	// two texture blocks on one line
	CHECK( MAT_GetType( 'Q' ) == NULL );										// the broken block: not used
}

// the mapper guide's example (docs/MATERIALS.md "Mapper guide", kept the same): loads cleanly over the game's files
static unsigned char *MemOrGameLoad( const char *path, int *length )
{
	unsigned char *data = MemLoad( path, length );

	return data ? data : GameLoad( path, length );
}

static void TestMapperExample( void )
{
	static const char example[] =
	"// materials/maps/co_mymap.txt: co_mymap's materials (docs/MATERIALS.md)\n"
	"// your own textures: letter, space, name (without { ! +0~)\n"
	"M MYMETALFLOOR\n"
	"A MYGRASS\n"
	"E MYCARPET_RED\n"
	"K MYALIENFLOOR\n"
	"// an SBO texture tagged again for this map only (materials/textures.txt has it as W)\n"
	"C 666PARQUET\n"
	"\n"
	"// letter K (custom) with your own sounds and impact effects\n"
	"material K\n"
	"{\n"
	"\tname \"alien floor\"\n"
	"\tstep_right { \"mymap/alien_step1.wav\" \"mymap/alien_step3.wav\" }\n"
	"\tstep_left { \"mymap/alien_step2.wav\" \"mymap/alien_step4.wav\" }\n"
	"\thit { \"mymap/alien_hit1.wav\" \"mymap/alien_hit2.wav\" }\n"
	"\thit_volume 0.8\n"
	"\tfx_debris { \"sprites/mymap/alien_chip.spr\" }\n"
	"\tfx_debris_count 3\n"
	"\tfx_smoke 1  fx_smoke_color 60 140 60\n"
	"\tfx_decal \"{bhitcrete\"\n"
	"\tfx_ricochet { \"mymap/alien_ric.wav\" }\n"
	"}\n"
	"\n"
	"// one texture's own effects: sparks and no decal on the glass panels\n"
	"texture \"{MYGLASS\" { fx_sparks 1 fx_decal none }\n"
		;
	char matched[ CBTEXTURENAMEMAX ];
	int layer;

	memset( g_memFiles, 0, sizeof( g_memFiles ) );
	g_memFiles[ 0 ][ 0 ] = "materials/maps/co_mymap.txt";
	g_memFiles[ 0 ][ 1 ] = example;
	MAT_LoadGlobal( GameLoad, GameFree );
	CHECK( MAT_FindTextureType( "666PARQUET" ) == 'W' );
	g_logLines = 0;
	CHECK( MAT_LoadMapFile( MemOrGameLoad, GameFree, "materials/maps/co_mymap.txt", NULL, NULL ) == 1 );
	CHECK( g_logLines == 0 );
	CHECK( MAT_FindTextureMatch( "666PARQUET", matched, &layer ) == 'C' && layer == MAT_LAYER_MAP + MAT_INCLUDE_DEPTH );
	CHECK( MAT_FindTextureType( "MYALIENFLOOR" ) == 'K' && MAT_FindTextureType( "MYCARPET_RED" ) == 'E' );
	CHECK( !strcmp( MAT_GetStep( 'K' )->right.name[ 1 ], "mymap/alien_step3.wav" ) && MAT_GetHit( 'K' )->volume == 0.8f );
	CHECK( !strcmp( MAT_GetFx( 'K', NULL )->debris_sprite[ 0 ], "sprites/mymap/alien_chip.spr" ) && MAT_GetFx( 'K', NULL )->debris_count == 3 );
	CHECK( MAT_GetFx( 'C', "MYGLASS" )->sparks && !strcmp( MAT_GetFx( 'C', "MYGLASS" )->decal, "none" ) );
	CHECK( MAT_GetStep( 'W' ) != MAT_GetStep( 'C' ) );		// the global types stay for the other letters
}

// the game's files: the global ones (SBO ships no sound/materials.txt: Half-Life's is used) and every map's file
static void TestGameFiles( void )
{
	char pattern[ 1200 ];
	FILE *list;
	int maps = 0;

	g_logLines = 0;
	MAT_LoadGlobal( GameLoad, GameFree );
	printf( "game files: %d textures in the global lists (%s from valve/ only with MAT_VALVE_LIST), %d log lines\n", MAT_TextureCount(), MAT_TEXTURE_LIST, g_logLines );
	CHECK( g_logLines == 0 );
	CHECK( MAT_GetType( 'A' ) && MAT_GetType( 'E' ) && MAT_GetType( 'R' ) && MAT_GetType( 'N' ) && MAT_GetType( 'X' ) && MAT_GetType( 'K' ) );
	CHECK( MAT_GetType( 'L' ) && MAT_GetType( 'Z' ) && MAT_GetType( 'I' ) );
	CHECK( MAT_GetType( 'X' ) && !MAT_GetType( 'X' )->has_step );
	CHECK( MAT_TextureCount() >= 300 );							// SBO's WAD tags (materials/textures.txt), every map
	CHECK( MAT_FindTextureType( "666GRASS2" ) == 'A' );
	CHECK( MAT_FindTextureType( "666TAPISSERIE5" ) == 'E' );
	CHECK( MAT_GetStep( 'W' ) != MAT_GetStep( 'C' ) );			// wood steps of their own
	CHECK( MAT_GetFx( 'C', NULL )->defined && MAT_GetFx( 'C', NULL )->debris_sprites == 3 );	// and impact effects
	CHECK( MAT_GetFx( 'M', NULL )->sparks && !MAT_GetFx( 'M', NULL )->debris_count );
	CHECK( !MAT_GetFx( 'F', NULL )->defined );						// flesh: the 1.38 effect

	// the global types file keeps Half-Life's sounds, volumes and step times of every Half-Life letter (only W gets
	// steps of its own; effects are new keys)
	{
		static mat_type_t builtin[ 26 ];
		int i;

		MAT_Reset();
		for ( i = 0; i < 26; i++ )
			if ( MAT_GetType( (char)( 'A' + i ) ) )
				builtin[ i ] = *MAT_GetType( (char)( 'A' + i ) );
		MAT_LoadGlobal( GameLoad, GameFree );
		for ( i = 0; i < 26; i++ )
		{
			const mat_type_t *now = MAT_GetType( (char)( 'A' + i ) );

			if ( !strchr( "CMDVGTSPYF", 'A' + i ) )
				continue;
			if ( !now || memcmp( &builtin[ i ].step, &now->step, sizeof( now->step ) ) || memcmp( &builtin[ i ].hit, &now->hit, sizeof( now->hit ) ) )
			{
				printf( "FAIL: materials/types.txt changes Half-Life's %c\n", 'A' + i );
				g_failures++;
			}
		}
		CHECK( !memcmp( &builtin[ 'W' - 'A' ].hit, &MAT_GetType( 'W' )->hit, sizeof( mat_hit_t ) ) );	// W's hits stay
	}

	snprintf( pattern, sizeof( pattern ), "ls '%s'/materials/maps/*.txt 2>/dev/null", g_gamedir );
	list = popen( pattern, "r" );
	while ( list && fgets( pattern, sizeof( pattern ), list ) )
	{
		char *name = strstr( pattern, "/materials/maps/" ) + 1;
		int files;

		name[ strcspn( name, "\r\n" ) ] = 0;
		MAT_LoadGlobal( GameLoad, GameFree );
		g_logLines = 0;
		files = MAT_LoadMapFile( GameLoad, GameFree, name, NULL, NULL );
		if ( files < 1 || g_logLines )
		{
			printf( "FAIL: %s: %d files, %d log lines\n", name, files, g_logLines );
			g_failures++;
		}
		maps++;
	}
	if ( list )
		pclose( list );
	printf( "game files: %d map files load cleanly (SBO ships none: every stock map uses the global files)\n", maps );
	CHECK( maps == 0 );

	MAT_LoadGlobal( GameLoad, GameFree );

	// every sprite and sound the game's effect data names exists in the game folder (or is Half-Life's ric1-5)
	{
		const char *letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		int i, j;

		for ( i = 0; letters[ i ]; i++ )
		{
			const mat_fx_t *fx = MAT_GetFx( letters[ i ], NULL );
			char path[ 256 ];
			unsigned char *data;
			int length;

			for ( j = 0; j < fx->debris_sprites; j++ )
			{
				data = GameLoad( fx->debris_sprite[ j ], &length );
				if ( !data || length < 4 || memcmp( data, "IDSP", 4 ) )
				{
					printf( "FAIL: %c: missing sprite %s\n", letters[ i ], fx->debris_sprite[ j ] );
					g_failures++;
				}
				free( data );
			}
			for ( j = 0; j < fx->ricochet.count; j++ )
			{
				snprintf( path, sizeof( path ), "sound/%s", fx->ricochet.name[ j ] );
				data = GameLoad( path, &length );
				if ( !data )
				{
					printf( "FAIL: %c: missing sound %s\n", letters[ i ], path );
					g_failures++;
				}
				free( data );
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// fuzzing: mutated copies of valid files into every parser
// ---------------------------------------------------------------------------------------------------------------

static unsigned int g_rng = 12345;

static unsigned int Rand( void )
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 17;
	g_rng ^= g_rng << 5;
	return g_rng;
}

static const char *g_dict[] = { "{", "}", "\"", "//", "/*", "*/", "\n", "\r\n", " ", "\t", "material", "ladder", "wade",
	"step_right", "step_left", "hit", "hit_volume", "walk_time", "name", "A", "C", "Z", "0.5", "-1", "1e9", "nan",
	"\"player/pl_step1.wav\"", "\"../x.wav\"", "M ", "C 666GRASS2", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "\0", "\xff", "+0~", NULL };

static int Mutate( const unsigned char *seed, int seedLength, unsigned char *out, int max )
{
	int length = seedLength < max ? seedLength : max;
	int edits = 1 + Rand() % 16, i;

	memcpy( out, seed, length );
	for ( i = 0; i < edits; i++ )
	{
		int pos = length ? (int)( Rand() % length ) : 0;

		switch ( Rand() % 5 )
		{
		case 0:		// flip a byte
			if ( length )
				out[ pos ] = (unsigned char)Rand();
			break;
		case 1:		// cut
			length = pos;
			break;
		case 2:		// insert a dictionary word
		{
			int n = 0;
			const char *word;

			while ( g_dict[ n ] )
				n++;
			word = g_dict[ Rand() % n ];
			n = (int)strlen( word );
			if ( !n )
				n = 1;
			if ( length + n <= max )
			{
				memmove( out + pos + n, out + pos, length - pos );
				memcpy( out + pos, word, n );
				length += n;
			}
			break;
		}
		case 3:		// delete a span
		{
			int n = (int)( Rand() % 32 );

			if ( pos + n > length )
				n = length - pos;
			memmove( out + pos, out + pos + n, length - pos - n );
			length -= n;
			break;
		}
		default:	// repeat a span
		{
			int n = (int)( Rand() % 64 );

			if ( pos + n > length )
				n = length - pos;
			if ( length + n <= max )
			{
				memmove( out + pos + n, out + pos, length - pos );
				length += n;
			}
			break;
		}
		}
	}
	return length;
}

static void FuzzName( const char *name, void *ctx )
{
	char stripped[ CBTEXTURENAMEMAX ];

	(void)ctx;
	MAT_StripTextureName( name, stripped );
	MAT_FindTextureType( stripped );
}

// every file a fuzzed map file names is another mutation of the same seed
static const unsigned char *g_fuzzSeed;
static int g_fuzzSeedLength;

static unsigned char *FuzzLoad( const char *path, int *length )
{
	static unsigned char buf[ 8192 ];
	unsigned char *data;

	(void)path;
	if ( Rand() % 4 == 0 )
		return NULL;
	*length = Mutate( g_fuzzSeed, g_fuzzSeedLength, buf, sizeof( buf ) );
	data = (unsigned char *)malloc( *length ? *length : 1 );
	memcpy( data, buf, *length );
	return data;
}

static void Fuzz( int seconds )
{
	int listLength = 0, typesLength = 0;
	unsigned char *list = GameLoad( MAT_TEXTURES_FILE, &listLength );
	unsigned char *types = GameLoad( MAT_TYPES_FILE, &typesLength );
	// a mapper's file: tag lines and inline blocks mixed (also the seed of the map files read by MAT_LoadMapFile)
	static const char mixed[] = "// co_fuzz\n#include materials/a.txt\n#types materials/b.txt\nM X1\nC 666PARQUET\n"
		"material K\n{\n\tname \"mine\"\n\tstep_right { \"a.wav\" } step_left { \"b.wav\" }\n\thit { \"c.wav\" }\n"
		"\tfx_debris { \"sprites/k.spr\" } fx_debris_count 3 fx_ricochet { \"k/r.wav\" }\n}\n"
		"texture \"{GLASS1\" { fx_sparks 1 fx_decal none } material A { walk_volume 0.3 }\n"
		"#include \"materials/c.txt\"\nladder\n{\n step_right { \"l1.wav\" } step_left { \"l2.wav\" }\n}\nE CARPET1\n";
	int sboTypesLength = (int)sizeof( mixed ) - 1;
	const unsigned char *sboTypes = (const unsigned char *)mixed;
	unsigned char bsp[ 1024 ];
	int bspLength = MakeBsp( bsp, sizeof( bsp ) );
	int max = 64 * 1024;
	unsigned char *buf = (unsigned char *)malloc( max );
	long runs = 0;
	time_t end = time( NULL ) + seconds;

	g_quiet = 1;
	g_rng = (unsigned int)time( NULL ) | 1;
	printf( "fuzz: %d s, seed %u\n", seconds, g_rng );

	while ( time( NULL ) < end )
	{
		int i, length;
		unsigned char *exact;

		for ( i = 0; i < 200; i++ )
		{
			char name[ 40 ];
			int n, k;

			MAT_Reset();

			// texture lists, both layers; the copy is exactly as long as the data, so ASan sees an overread
			length = Mutate( list, listLength, buf, max );
			exact = (unsigned char *)malloc( length ? length : 1 );
			memcpy( exact, buf, length );
			MAT_ParseTextureList( (const char *)exact, length, Rand() % 2, "fuzz" );
			free( exact );

			length = ( Rand() & 1 ) ? Mutate( types, typesLength, buf, max ) : Mutate( sboTypes, sboTypesLength, buf, max );
			exact = (unsigned char *)malloc( length ? length : 1 );
			memcpy( exact, buf, length );
			MAT_ParseTypes( (const char *)exact, length, "fuzz" );
			free( exact );

			// map files with #include / #types and inline blocks: each file read is a mutation of the mixed file
			g_fuzzSeed = sboTypes;
			g_fuzzSeedLength = sboTypesLength;
			MAT_LoadMapFile( FuzzLoad, GameFree, "materials/maps/fuzz.txt", NULL, NULL );

			// lookups with random names
			n = (int)( Rand() % 39 );
			for ( k = 0; k < n; k++ )
				name[ k ] = (char)( 1 + Rand() % 255 );
			name[ n ] = 0;
			FuzzName( name, NULL );
			MAT_GetStep( (char)Rand() );
			MAT_GetHit( (char)Rand() );

			// BSP texture lumps
			length = Mutate( bsp, bspLength, buf, sizeof( bsp ) + 256 );
			if ( Rand() % 4 == 0 && length >= 128 )
				PutInt( buf + 4 * ( Rand() % 32 ), (int)Rand() );
			exact = (unsigned char *)malloc( length ? length : 1 );
			memcpy( exact, buf, length );
			MAT_ForEachBspTexture( exact, length, FuzzName, NULL );
			free( exact );
			runs++;
		}
	}
	g_quiet = 0;
	printf( "fuzz: %ld runs\n", runs );
	free( buf );
	free( list );
	free( types );
}

// ---------------------------------------------------------------------------------------------------------------
// 1.38 check: HL SDK's PM_InitTextureTypes / PM_SortTextures / PM_FindTextureType (the client of 1.38) on
// Half-Life's file against the registry on SBO's files, for every texture of the given maps
// ---------------------------------------------------------------------------------------------------------------

#define HL_CTEXTURESMAX 512
#define HL_CBTEXTURENAMEMAX 13

static int hl_count;
static char hl_names[ HL_CTEXTURESMAX ][ HL_CBTEXTURENAMEMAX ];
static char hl_types[ HL_CTEXTURESMAX ];

static int HlCompare( const char *a, const char *b, int n )
{
	int i;

	for ( i = 0; n < 0 || i < n; i++ )
	{
		int ca = tolower( (unsigned char)a[ i ] ), cb = tolower( (unsigned char)b[ i ] );

		if ( ca != cb )
			return ca - cb;
		if ( !ca )
			return 0;
	}
	return 0;
}

static void HlLoad( const unsigned char *data, int size )
{
	int pos = 0;
	char buffer[ 512 ];

	hl_count = 0;
	while ( pos < size && hl_count < HL_CTEXTURESMAX )
	{
		int i = pos, last = size, n, j;

		// memfgets( ..., 511 )
		if ( last - pos > 510 )
			last = pos + 510;
		while ( i < last )
		{
			if ( data[ i++ ] == '\n' )
				break;
		}
		n = i - pos;
		memcpy( buffer, data + pos, n );
		buffer[ n ] = 0;
		pos = i;

		i = 0;
		while ( buffer[ i ] && isspace( (unsigned char)buffer[ i ] ) )
			i++;
		if ( !buffer[ i ] )
			continue;
		if ( buffer[ i ] == '/' || !isalpha( (unsigned char)buffer[ i ] ) )
			continue;
		hl_types[ hl_count ] = (char)toupper( (unsigned char)buffer[ i++ ] );
		while ( buffer[ i ] && isspace( (unsigned char)buffer[ i ] ) )
			i++;
		if ( !buffer[ i ] )
			continue;
		j = i;
		while ( buffer[ j ] && !isspace( (unsigned char)buffer[ j ] ) )
			j++;
		if ( !buffer[ j ] )
			continue;
		if ( j > HL_CBTEXTURENAMEMAX - 1 + i )
			j = HL_CBTEXTURENAMEMAX - 1 + i;
		buffer[ j ] = 0;
		strcpy( hl_names[ hl_count++ ], buffer + i );
	}

	// PM_SortTextures
	{
		int i, j;

		for ( i = 0; i < hl_count; i++ )
		{
			for ( j = i + 1; j < hl_count; j++ )
			{
				if ( HlCompare( hl_names[ i ], hl_names[ j ], -1 ) > 0 )
				{
					char t[ HL_CBTEXTURENAMEMAX ], c;

					memcpy( t, hl_names[ i ], sizeof( t ) );
					memcpy( hl_names[ i ], hl_names[ j ], sizeof( t ) );
					memcpy( hl_names[ j ], t, sizeof( t ) );
					c = hl_types[ i ]; hl_types[ i ] = hl_types[ j ]; hl_types[ j ] = c;
				}
			}
		}
	}
}

static char HlFind( const char *name )
{
	int left = 0, right = hl_count - 1;

	while ( left <= right )
	{
		int pivot = ( left + right ) / 2;
		int val = HlCompare( name, hl_names[ pivot ], HL_CBTEXTURENAMEMAX - 1 );

		if ( val == 0 )
			return hl_types[ pivot ];
		if ( val > 0 )
			left = pivot + 1;
		else
			right = pivot - 1;
	}
	return 'C';
}

// HL's sound per letter: steps (the letters with a step type) and hits
static int HlStepClass( char c )
{
	return strchr( "MDVGTS", c ) ? c : 'C';
}

static int HlHitClass( char c )
{
	if ( c == 'P' )
		return 'Y';			// same sounds; P adds sparks
	return strchr( "MDVGTSWYF", c ) ? c : 'C';
}

typedef struct
{
	const char *map;
	const char *mapFile;		// the map's materials file ("": none)
	int textures, changed, byGlobal, byMap, byWood;
} mapcheck_t;

static mapcheck_t *g_check;
static char g_seen[ 4096 ][ 17 ];
static int g_seenCount;

static void CheckName( const char *raw, void *ctx )
{
	char stripped[ CBTEXTURENAMEMAX ];
	char hlname[ HL_CBTEXTURENAMEMAX + 4 ];
	char matched[ CBTEXTURENAMEMAX ];
	char before, after;
	int layer, i;

	(void)ctx;
	for ( i = 0; i < g_seenCount; i++ )
	{
		if ( !HlCompare( g_seen[ i ], raw, -1 ) )
			return;
	}
	if ( g_seenCount < 4096 )
		snprintf( g_seen[ g_seenCount++ ], 17, "%s", raw );
	g_check->textures++;

	// HL: strip, strcpy, cut at 12
	{
		const char *p = raw;

		if ( *p == '-' || *p == '+' )
			p += 2;
		if ( *p == '{' || *p == '!' || *p == '~' || *p == ' ' )
			p++;
		snprintf( hlname, sizeof( hlname ), "%s", p );
		hlname[ HL_CBTEXTURENAMEMAX - 1 ] = 0;
	}
	before = HlFind( hlname );

	MAT_StripTextureName( raw, stripped );
	after = MAT_FindTextureMatch( stripped, matched, &layer );

	// same sounds as 1.38: letters that sound alike (HL gave unknown letters concrete; SBO's new letters have
	// their own sounds; W walks on wood now, on every map)
	if ( !strchr( "AERNXKLZIW", after ) && HlStepClass( before ) == HlStepClass( after ) && HlHitClass( before ) == HlHitClass( after ) )
		return;

	// every change must come from SBO's data (the material system applies to every map): the tag
	// of materials/textures.txt or of the map's own file, or W's steps of materials/types.txt
	{
		const char *why = layer >= MAT_LAYER_MAP ? "map file" : layer == MAT_LAYER_GLOBAL ? "textures.txt"
			: after == 'W' ? "types.txt W steps" : NULL;

		g_check->changed++;
		if ( layer >= MAT_LAYER_MAP )
			g_check->byMap++;
		else if ( layer == MAT_LAYER_GLOBAL )
			g_check->byGlobal++;
		else if ( after == 'W' )
			g_check->byWood++;
		printf( "  %-24s %-16s 1.38 %c -> %c (entry %s) %s\n", g_check->map, raw, before, after, matched[ 0 ] ? matched : "-",
			why ? why : "NOT FROM SBO'S DATA" );
		if ( !why )
			g_failures++;
	}
}

static int g_optinMaps;

// the map's materials file as the server picks it (sound.cpp TEXTURETYPE_Init): worldspawn "materials_file", else
// materials/maps/<map>.txt if the game has it
static void MapFileOf( const char *bsp, const unsigned char *data, int length, char *out )
{
	char name[ 45 ];
	const char *base = strrchr( bsp, '/' ) ? strrchr( bsp, '/' ) + 1 : bsp;
	int n, entOffset, entLength;
	unsigned char *probe;

	out[ 0 ] = 0;
	if ( data && length > 8 )
	{
		entOffset = data[ 4 ] | ( data[ 5 ] << 8 ) | ( data[ 6 ] << 16 ) | ( data[ 7 ] << 24 );
		entLength = data[ 8 ] | ( data[ 9 ] << 8 ) | ( data[ 10 ] << 16 ) | ( data[ 11 ] << 24 );
		if ( entOffset > 0 && entLength > 0 && entOffset < length && entLength <= length - entOffset )
		{
			char *ents = (char *)malloc( entLength + 1 ), *end, *key;

			memcpy( ents, data + entOffset, entLength );
			ents[ entLength ] = 0;
			end = strchr( ents, '}' );		// worldspawn is the first entity
			if ( end )
				*end = 0;
			key = strstr( ents, "\"materials_file\"" );
			if ( key && sscanf( key + 16, " \"%63[^\"]\"", out ) != 1 )
				out[ 0 ] = 0;
			free( ents );
			if ( out[ 0 ] )
				return;
		}
	}
	// the name without ".bsp", 44 characters at most: the path stays under 64 (the game refuses longer ones)
	n = (int)strlen( base );
	if ( n > 4 )
		n -= 4;
	if ( n > (int)sizeof( name ) - 1 )
		n = (int)sizeof( name ) - 1;
	memcpy( name, base, n );
	name[ n ] = 0;
	snprintf( out, MAT_PATH_LEN, MAT_MAP_FILE, name );
	n = 0;
	probe = GameLoad( out, &n );
	if ( probe )
		free( probe );
	else
		out[ 0 ] = 0;
}

static int CheckMaps( const char *hlFile, int count, char **bsps )
{
	unsigned char *data;
	int length = 0, i, total = 0, changed = 0, byGlobal = 0, byMap = 0, byWood = 0, mapsChanged = 0;

	data = ReadFile( hlFile, &length );
	if ( !data )
	{
		printf( "no %s\n", hlFile );
		return 1;
	}
	HlLoad( data, length );
	free( data );
	printf( "1.38 list: %d entries from %s\n", hl_count, hlFile );

	g_valveList = hlFile;
	MAT_LoadGlobal( GameLoad, GameFree );
	printf( "SBO global list: %d entries\n", MAT_TextureCount() );

	for ( i = 0; i < count; i++ )
	{
		mapcheck_t check;
		char mapFile[ MAT_PATH_LEN ];

		memset( &check, 0, sizeof( check ) );
		check.map = strrchr( bsps[ i ], '/' ) ? strrchr( bsps[ i ], '/' ) + 1 : bsps[ i ];
		g_check = &check;
		g_seenCount = 0;
		data = ReadFile( bsps[ i ], &length );
		MapFileOf( bsps[ i ], data, length, mapFile );
		check.mapFile = mapFile;
		MAT_LoadGlobal( GameLoad, GameFree );
		if ( mapFile[ 0 ] && MAT_LoadMapFile( GameLoad, GameFree, mapFile, NULL, NULL ) < 0 )
			mapFile[ 0 ] = 0;
		if ( mapFile[ 0 ] )
			g_optinMaps++;
		if ( !data || MAT_ForEachBspTexture( data, length, CheckName, NULL ) < 0 )
		{
			printf( "  %s: not a readable BSP\n", bsps[ i ] );
			g_failures++;
		}
		free( data );
		total += check.textures;
		changed += check.changed;
		byGlobal += check.byGlobal;
		byMap += check.byMap;
		byWood += check.byWood;
		mapsChanged += check.changed > 0;
		printf( "map %-28s textures %4d changed %4d (textures.txt %d, map file %d, W steps %d)\n", check.map,
			check.textures, check.changed, check.byGlobal, check.byMap, check.byWood );
	}
	printf( "maps %d (%d with a materials file, %d sound different), textures %d, changed %d (textures.txt %d, map file %d, W steps %d, other %d)\n",
		count, g_optinMaps, mapsChanged, total, changed, byGlobal, byMap, byWood, changed - byGlobal - byMap - byWood );
	return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// precache count: the sounds TEXTURETYPE_PrecacheLevel (sound.cpp) precaches for each map, the same selection
// ---------------------------------------------------------------------------------------------------------------

static char g_sounds[ 256 ][ MAT_SOUND_LEN ];
static int g_soundCount;

static void AddSound( const char *name )
{
	int i;

	for ( i = 0; i < g_soundCount; i++ )
	{
		if ( !HlCompare( g_sounds[ i ], name, -1 ) )
			return;
	}
	if ( g_soundCount < 256 )
		snprintf( g_sounds[ g_soundCount++ ], MAT_SOUND_LEN, "%s", name );
}

static void AddList( const mat_sounds_t *list )
{
	int i;

	for ( i = 0; i < list->count; i++ )
		AddSound( list->name[ i ] );
}

// the download list, the same rule as sound.cpp (TEXTURETYPE_DownloadFile / TEXTURETYPE_DownloadFxFile): every file
// the map's file pulls in and the effect files of the map's textures, except SBO's own files and the effect files
// SBO's own data names
static char g_downloads[ 64 ][ MAT_PATH_LEN ];
static int g_downloadCount;
static int g_mapOwnFiles;
static char g_sboFx[ 64 ][ MAT_PATH_LEN ];
static int g_sboFxCount = -1;

static int InNames( char names[][ MAT_PATH_LEN ], int count, const char *path )
{
	int i;

	for ( i = 0; i < count; i++ )
	{
		if ( !HlCompare( names[ i ], path, -1 ) )
			return 1;
	}
	return 0;
}

static void AddDownload( const char *path )
{
	if ( !InNames( g_downloads, g_downloadCount, path ) && g_downloadCount < 64 )
		snprintf( g_downloads[ g_downloadCount++ ], MAT_PATH_LEN, "%s", path );
}

static void DownloadFile( const char *path, void *ctx )
{
	(void)ctx;
	if ( !HlCompare( path, MAT_TEXTURE_LIST, -1 ) || !HlCompare( path, MAT_TEXTURES_FILE, -1 ) || !HlCompare( path, MAT_TYPES_FILE, -1 ) )
		return;
	g_mapOwnFiles = 1;
	AddDownload( path );
}

static void SboFxFile( const char *path, void *ctx )
{
	(void)ctx;
	if ( g_sboFxCount < 64 && !InNames( g_sboFx, g_sboFxCount, path ) )
		snprintf( g_sboFx[ g_sboFxCount++ ], MAT_PATH_LEN, "%s", path );
}

static void SboFx( const mat_fx_t *fx, void *ctx )
{
	MAT_ForEachFxFile( fx, SboFxFile, ctx );
}

static void DownloadFxFile( const char *path, void *ctx )
{
	(void)ctx;
	if ( !InNames( g_sboFx, g_sboFxCount, path ) )
		AddDownload( path );
}

static void MarkUsed( const char *name, void *ctx )
{
	char stripped[ CBTEXTURENAMEMAX ];
	char letter;

	MAT_StripTextureName( name, stripped );
	letter = MAT_FindTextureType( stripped );
	if ( letter >= 'A' && letter <= 'Z' )
		( (char *)ctx )[ letter - 'A' ] = 1;
	if ( g_mapOwnFiles )
		MAT_ForEachFxFile( MAT_GetFx( letter, stripped ), DownloadFxFile, NULL );
}

static int PrecacheCount( int count, char **bsps )
{
	int i, j, length;

	g_valveList = getenv( "MAT_VALVE_LIST" );
	// the effect files of the global data (sound.cpp gathers them after MAT_LoadGlobal at every map start)
	g_sboFxCount = 0;
	MAT_LoadGlobal( GameLoad, GameFree );
	MAT_ForEachFx( SboFx, NULL );
	printf( "global effect files (sprites + ricochet sounds): %d\n", g_sboFxCount );
	printf( "sounds downloads letters                    map\n" );
	for ( i = 0; i < count; i++ )
	{
		char used[ 26 ], letters[ 27 ], mapFile[ MAT_PATH_LEN ];
		unsigned char *data = ReadFile( bsps[ i ], &length );
		int n = 0;

		MAT_LoadGlobal( GameLoad, GameFree );
		MapFileOf( bsps[ i ], data, length, mapFile );
		g_downloadCount = 0;
		g_mapOwnFiles = 0;
		if ( mapFile[ 0 ] )
			MAT_LoadMapFile( GameLoad, GameFree, mapFile, DownloadFile, NULL );

		memset( used, 0, sizeof( used ) );
		used[ 'C' - 'A' ] = used[ 'F' - 'A' ] = used[ 'S' - 'A' ] = 1;
		if ( !data || MAT_ForEachBspTexture( data, length, MarkUsed, used ) < 0 )
			memset( used, 1, sizeof( used ) );
		free( data );

		g_soundCount = 0;
		for ( j = 0; j < 26; j++ )
		{
			const mat_type_t *type = MAT_GetType( (char)( 'A' + j ) );

			if ( !type || !used[ j ] )
				continue;
			letters[ n++ ] = (char)( 'A' + j );
			if ( type->has_step )
			{
				AddList( &type->step.right );
				AddList( &type->step.left );
			}
			if ( type->has_hit )
			{
				AddList( &type->hit.sounds );
				if ( type->hit.sparks )
				{
					AddSound( "buttons/spark5.wav" );
					AddSound( "buttons/spark6.wav" );
				}
			}
		}
		letters[ n ] = 0;
		AddList( &MAT_GetLadderStep()->right );
		AddList( &MAT_GetLadderStep()->left );
		AddList( &MAT_GetWadeStep()->right );
		AddList( &MAT_GetWadeStep()->left );
		printf( "%6d %9d %-26s %s", g_soundCount, g_downloadCount, letters, strrchr( bsps[ i ], '/' ) ? strrchr( bsps[ i ], '/' ) + 1 : bsps[ i ] );
		for ( j = 0; j < g_downloadCount; j++ )
			printf( " %s", g_downloads[ j ] );
		printf( "\n" );
	}
	return 0;
}

int main( int argc, char **argv )
{
	MAT_SetLog( TestLog );

	if ( argc >= 3 && !strcmp( argv[ 1 ], "--precache" ) )
	{
		snprintf( g_gamedir, sizeof( g_gamedir ), "%s", argv[ 2 ] );
		return PrecacheCount( argc - 3, argv + 3 );
	}

	if ( argc >= 4 && !strcmp( argv[ 1 ], "--maps" ) )
	{
		snprintf( g_gamedir, sizeof( g_gamedir ), "%s", argv[ 3 ] );
		g_quiet = 0;
		CheckMaps( argv[ 2 ], argc - 4, argv + 4 );
		printf( g_failures ? "FAILED: %d\n" : "OK\n", g_failures );
		return g_failures ? 1 : 0;
	}

	if ( argc < 2 )
	{
		printf( "usage: materials_test <gamedir> [--fuzz SECONDS] | --maps <hl materials.txt> <gamedir> <bsp>...\n" );
		return 2;
	}
	snprintf( g_gamedir, sizeof( g_gamedir ), "%s", argv[ 1 ] );

	TestBuiltins();
	TestStrip();
	TestTextureList();
	TestTypes();
	TestBsp();
	TestMapFiles();
	TestPrecedence();
	TestGameFiles();
	TestMapperExample();
	if ( argc >= 4 && !strcmp( argv[ 2 ], "--fuzz" ) )
		Fuzz( atoi( argv[ 3 ] ) );

	printf( g_failures ? "FAILED: %d\n" : "OK\n", g_failures );
	return g_failures ? 1 : 0;
}
