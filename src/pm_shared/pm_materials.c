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

// SBO 2.0 material registry (pm_materials.h, docs/MATERIALS.md). Shared by the client and the server DLL; no
// engine calls: files come in through a loader, messages go out through a log function.
//
// Ported from halflife-featureful's MaterialRegistry (game_shared/tex_materials.cpp, FreeSlave, HL SDK license):
// a table of material types keyed by letter (step sounds per foot, walk/run volume and time, hit sounds and
// volumes), ladder and wade steps, built-in Half-Life defaults that a data file can change or extend. The texture
// list keeps Half-Life's sound/materials.txt format. Written again in C (pm_shared.c is C), with a bounded
// tokenizer instead of rapidjson.
//
// Every parser here reads data from maps and downloads: lengths are checked before every read, no recursion,
// nothing is written past a fixed buffer, and a broken file is refused without changing the registry.

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "pm_materials.h"

#define MAT_TYPES			26		// 'A' .. 'Z'
#define MAT_TOKEN_LEN		128
#define MAT_LOG_LIMIT		16		// messages per file
#define MAT_ATTN_NORM		0.8f
#define MAT_BSP_VERSION		30
#define MAT_BSP_LUMPS		15
#define MAT_BSP_TEXTURES	2		// lump index

typedef struct mat_texture_s
{
	char	name[ CBTEXTURENAMEMAX ];
	char	letter;
	int		layer;
	int		seq;
} mat_texture_t;

typedef struct mat_texfx_s
{
	char		name[ CBTEXTURENAMEMAX ];
	mat_fx_t	fx;
} mat_texfx_t;

typedef struct mat_types_s
{
	mat_type_t	type[ MAT_TYPES ];
	mat_step_t	ladder;
	mat_step_t	wade;
	int			texfxCount;
	mat_texfx_t	texfx[ MAT_TEXFX_MAX ];
} mat_types_t;

static mat_texture_t	g_matTextures[ MAT_TEXTURES_MAX ];
static int				g_matTextureCount;
static int				g_matSeq;
static mat_types_t		g_matTypes;
static mat_types_t		g_matScratch;		// MAT_ParseTypes works on this copy
static int				g_matUsable[ MAT_TYPES ];
static int				g_matLoaded;		// built-in types set up
static int				g_matFilesLoaded;	// MAT_LoadGlobal ran
static mat_log_fn		g_matLog;
static int				g_matLogCount;

static void MAT_Log( const char *fmt, ... )
{
	char text[ 256 ];
	va_list args;

	if ( !g_matLog || g_matLogCount >= MAT_LOG_LIMIT )
		return;

	if ( ++g_matLogCount == MAT_LOG_LIMIT )
	{
		g_matLog( "materials: more messages left out\n" );
		return;
	}

	va_start( args, fmt );
	vsnprintf( text, sizeof( text ), fmt, args );
	va_end( args );
	text[ sizeof( text ) - 1 ] = 0;
	g_matLog( text );
}

void MAT_SetLog( mat_log_fn log )
{
	g_matLog = log;
}

static int MAT_Lower( int c )
{
	return ( c >= 'A' && c <= 'Z' ) ? c + ( 'a' - 'A' ) : c;
}

// case-insensitive (ASCII) compare of at most n characters (n < 0: all)
static int MAT_Compare( const char *a, const char *b, int n )
{
	int i;

	for ( i = 0; n < 0 || i < n; i++ )
	{
		int ca = MAT_Lower( (unsigned char)a[ i ] );
		int cb = MAT_Lower( (unsigned char)b[ i ] );

		if ( ca != cb )
			return ca < cb ? -1 : 1;
		if ( !ca )
			return 0;
	}
	return 0;
}

static void MAT_CopyString( char *dest, const char *src, int size )
{
	int i;

	for ( i = 0; i < size - 1 && src[ i ]; i++ )
		dest[ i ] = src[ i ];
	dest[ i ] = 0;
}

static int MAT_LetterIndex( char letter )
{
	int c = (unsigned char)letter;

	if ( c >= 'a' && c <= 'z' )
		c -= 'a' - 'A';
	if ( c < 'A' || c > 'Z' )
		return -1;
	return c - 'A';
}

// ---------------------------------------------------------------------------------------------------------------
// built-in types: Half-Life's sounds, volumes and step times (pm_shared.c PM_PlayStepSound / PM_UpdateStepSound,
// sound.cpp TEXTURETYPE_PlaySound, ev_hldm.cpp EV_HLDM_PlayTextureSound of the HL SDK)
// ---------------------------------------------------------------------------------------------------------------

static void MAT_SetSounds( mat_sounds_t *list, const char *a, const char *b, const char *c, const char *d, const char *e )
{
	const char *names[ 5 ];
	int i;

	names[ 0 ] = a; names[ 1 ] = b; names[ 2 ] = c; names[ 3 ] = d; names[ 4 ] = e;
	list->count = 0;
	for ( i = 0; i < 5 && names[ i ]; i++ )
		MAT_CopyString( list->name[ list->count++ ], names[ i ], MAT_SOUND_LEN );
}

static void MAT_StepDefaults( mat_step_t *step )
{
	memset( step, 0, sizeof( *step ) );
	step->walk_volume = 0.2f;
	step->walk_time = 400;
	step->run_volume = 0.5f;
	step->run_time = 300;
}

static void MAT_FxDefaults( mat_fx_t *fx );

static mat_type_t *MAT_NewType( mat_types_t *types, char letter, const char *name )
{
	mat_type_t *type = &types->type[ MAT_LetterIndex( letter ) ];

	memset( type, 0, sizeof( *type ) );
	type->defined = 1;
	MAT_CopyString( type->name, name, MAT_NAME_LEN );
	MAT_StepDefaults( &type->step );
	type->hit.volume = 0.9f;
	type->hit.volume_bar = 0.5f;
	type->hit.attenuation = MAT_ATTN_NORM;
	MAT_FxDefaults( &type->fx );
	return type;
}

static void MAT_SetHit( mat_type_t *type, float volume, float volume_bar )
{
	type->has_hit = 1;
	type->hit.volume = volume;
	type->hit.volume_bar = volume_bar;
}

static void MAT_BuiltinTypes( mat_types_t *types )
{
	mat_type_t *t;

	memset( types, 0, sizeof( *types ) );

	t = MAT_NewType( types, CHAR_TEX_CONCRETE, "concrete" );
	t->has_step = 1;
	MAT_SetSounds( &t->step.right, "player/pl_step1.wav", "player/pl_step3.wav", NULL, NULL, NULL );
	MAT_SetSounds( &t->step.left, "player/pl_step2.wav", "player/pl_step4.wav", NULL, NULL, NULL );
	MAT_SetHit( t, 0.9f, 0.6f );
	MAT_SetSounds( &t->hit.sounds, "player/pl_step1.wav", "player/pl_step2.wav", NULL, NULL, NULL );

	t = MAT_NewType( types, CHAR_TEX_METAL, "metal" );
	t->has_step = 1;
	MAT_SetSounds( &t->step.right, "player/pl_metal1.wav", "player/pl_metal3.wav", NULL, NULL, NULL );
	MAT_SetSounds( &t->step.left, "player/pl_metal2.wav", "player/pl_metal4.wav", NULL, NULL, NULL );
	MAT_SetHit( t, 0.9f, 0.3f );
	MAT_SetSounds( &t->hit.sounds, "player/pl_metal1.wav", "player/pl_metal2.wav", NULL, NULL, NULL );

	t = MAT_NewType( types, CHAR_TEX_DIRT, "dirt" );
	t->has_step = 1;
	MAT_SetSounds( &t->step.right, "player/pl_dirt1.wav", "player/pl_dirt3.wav", NULL, NULL, NULL );
	MAT_SetSounds( &t->step.left, "player/pl_dirt2.wav", "player/pl_dirt4.wav", NULL, NULL, NULL );
	t->step.walk_volume = 0.25f;
	t->step.run_volume = 0.55f;
	MAT_SetHit( t, 0.9f, 0.1f );
	MAT_SetSounds( &t->hit.sounds, "player/pl_dirt1.wav", "player/pl_dirt2.wav", "player/pl_dirt3.wav", NULL, NULL );

	t = MAT_NewType( types, CHAR_TEX_VENT, "vent" );
	t->has_step = 1;
	MAT_SetSounds( &t->step.right, "player/pl_duct1.wav", "player/pl_duct3.wav", NULL, NULL, NULL );
	MAT_SetSounds( &t->step.left, "player/pl_duct2.wav", "player/pl_duct4.wav", NULL, NULL, NULL );
	t->step.walk_volume = 0.4f;
	t->step.run_volume = 0.7f;
	MAT_SetHit( t, 0.5f, 0.3f );
	MAT_SetSounds( &t->hit.sounds, "player/pl_duct1.wav", "player/pl_duct1.wav", NULL, NULL, NULL );

	t = MAT_NewType( types, CHAR_TEX_GRATE, "grate" );
	t->has_step = 1;
	MAT_SetSounds( &t->step.right, "player/pl_grate1.wav", "player/pl_grate3.wav", NULL, NULL, NULL );
	MAT_SetSounds( &t->step.left, "player/pl_grate2.wav", "player/pl_grate4.wav", NULL, NULL, NULL );
	MAT_SetHit( t, 0.9f, 0.5f );
	MAT_SetSounds( &t->hit.sounds, "player/pl_grate1.wav", "player/pl_grate4.wav", NULL, NULL, NULL );

	// HL plays pl_tile5 on one step in five, else tile1/tile3 (right) or tile2/tile4 (left): the same odds
	t = MAT_NewType( types, CHAR_TEX_TILE, "tile" );
	t->has_step = 1;
	MAT_SetSounds( &t->step.right, "player/pl_tile1.wav", "player/pl_tile3.wav", "player/pl_tile1.wav", "player/pl_tile3.wav", "player/pl_tile5.wav" );
	MAT_SetSounds( &t->step.left, "player/pl_tile2.wav", "player/pl_tile4.wav", "player/pl_tile2.wav", "player/pl_tile4.wav", "player/pl_tile5.wav" );
	MAT_SetHit( t, 0.8f, 0.2f );
	MAT_SetSounds( &t->hit.sounds, "player/pl_tile1.wav", "player/pl_tile3.wav", "player/pl_tile2.wav", "player/pl_tile4.wav", NULL );

	t = MAT_NewType( types, CHAR_TEX_SLOSH, "slosh" );
	t->has_step = 1;
	MAT_SetSounds( &t->step.right, "player/pl_slosh1.wav", "player/pl_slosh3.wav", NULL, NULL, NULL );
	MAT_SetSounds( &t->step.left, "player/pl_slosh2.wav", "player/pl_slosh4.wav", NULL, NULL, NULL );
	MAT_SetHit( t, 0.9f, 0.0f );
	MAT_SetSounds( &t->hit.sounds, "player/pl_slosh1.wav", "player/pl_slosh3.wav", "player/pl_slosh2.wav", "player/pl_slosh4.wav", NULL );

	t = MAT_NewType( types, CHAR_TEX_WOOD, "wood" );
	MAT_SetHit( t, 0.9f, 0.2f );
	MAT_SetSounds( &t->hit.sounds, "debris/wood1.wav", "debris/wood2.wav", "debris/wood3.wav", NULL, NULL );

	t = MAT_NewType( types, CHAR_TEX_GLASS, "glass" );
	MAT_SetHit( t, 0.8f, 0.2f );
	MAT_SetSounds( &t->hit.sounds, "debris/glass1.wav", "debris/glass2.wav", "debris/glass3.wav", NULL, NULL );

	t = MAT_NewType( types, CHAR_TEX_COMPUTER, "computer" );
	MAT_SetHit( t, 0.8f, 0.2f );
	MAT_SetSounds( &t->hit.sounds, "debris/glass1.wav", "debris/glass2.wav", "debris/glass3.wav", NULL, NULL );
	t->hit.sparks = 1;

	t = MAT_NewType( types, CHAR_TEX_FLESH, "flesh" );
	MAT_SetHit( t, 1.0f, 0.2f );
	t->hit.attenuation = 1.0f;
	MAT_SetSounds( &t->hit.sounds, "weapons/bullet_hit1.wav", "weapons/bullet_hit2.wav", NULL, NULL, NULL );

	MAT_StepDefaults( &types->ladder );
	MAT_SetSounds( &types->ladder.right, "player/pl_ladder1.wav", "player/pl_ladder3.wav", NULL, NULL, NULL );
	MAT_SetSounds( &types->ladder.left, "player/pl_ladder2.wav", "player/pl_ladder4.wav", NULL, NULL, NULL );
	types->ladder.walk_volume = types->ladder.run_volume = 0.35f;
	types->ladder.walk_time = types->ladder.run_time = 350;

	MAT_StepDefaults( &types->wade );
	MAT_SetSounds( &types->wade.right, "player/pl_wade1.wav", "player/pl_wade2.wav", NULL, NULL, NULL );
	MAT_SetSounds( &types->wade.left, "player/pl_wade3.wav", "player/pl_wade4.wav", NULL, NULL, NULL );
	types->wade.walk_volume = types->wade.run_volume = 0.65f;
	types->wade.walk_time = types->wade.run_time = 600;
	types->wade.skip_steps = 1;
}

void MAT_Reset( void )
{
	int i;

	memset( g_matTextures, 0, sizeof( g_matTextures ) );
	g_matTextureCount = 0;
	g_matSeq = 0;
	MAT_BuiltinTypes( &g_matTypes );
	for ( i = 0; i < MAT_TYPES; i++ )
		g_matUsable[ i ] = 1;
	g_matLoaded = 1;
}

int MAT_IsLoaded( void )
{
	return g_matFilesLoaded;
}

// ---------------------------------------------------------------------------------------------------------------
// texture names
// ---------------------------------------------------------------------------------------------------------------

void MAT_StripTextureName( const char *raw, char *out )
{
	out[ 0 ] = 0;
	if ( !raw )
		return;

	// strip leading '-0' or '+0~' or '{' or '!' (HL); only when the name has a second character
	if ( ( raw[ 0 ] == '-' || raw[ 0 ] == '+' ) && raw[ 1 ] )
		raw += 2;

	if ( raw[ 0 ] == '{' || raw[ 0 ] == '!' || raw[ 0 ] == '~' || raw[ 0 ] == ' ' )
		raw++;

	MAT_CopyString( out, raw, CBTEXTURENAMEMAX );
}

// first entry whose name is not below key (compared on n characters)
static int MAT_LowerBound( const char *key, int n )
{
	int lo = 0, hi = g_matTextureCount;

	while ( lo < hi )
	{
		int mid = lo + ( hi - lo ) / 2;

		if ( MAT_Compare( g_matTextures[ mid ].name, key, n ) < 0 )
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo;
}

static void MAT_AddTexture( const char *name, char letter, int layer )
{
	int pos = MAT_LowerBound( name, -1 );
	mat_texture_t *entry;

	if ( pos < g_matTextureCount && !MAT_Compare( g_matTextures[ pos ].name, name, -1 ) )
	{
		entry = &g_matTextures[ pos ];
		// a later file overrides; inside one file the first entry wins (as HL's sorted search found it for the
		// one duplicate of Valve's list, GENERIC015V)
		if ( layer > entry->layer )
		{
			entry->letter = letter;
			entry->layer = layer;
			entry->seq = g_matSeq++;
		}
		return;
	}

	if ( g_matTextureCount >= MAT_TEXTURES_MAX )
	{
		MAT_Log( "materials: more than %d textures, %s left out\n", MAT_TEXTURES_MAX, name );
		return;
	}

	memmove( &g_matTextures[ pos + 1 ], &g_matTextures[ pos ], ( g_matTextureCount - pos ) * sizeof( mat_texture_t ) );
	entry = &g_matTextures[ pos ];
	MAT_CopyString( entry->name, name, CBTEXTURENAMEMAX );
	entry->letter = letter;
	entry->layer = layer;
	entry->seq = g_matSeq++;
	g_matTextureCount++;
}

static const mat_texture_t *MAT_FindEntry( const char *name )
{
	char key[ CBTEXTURENAMEMAX ];
	const mat_texture_t *best = NULL;
	int pos;

	if ( !name )
		return NULL;

	MAT_CopyString( key, name, CBTEXTURENAMEMAX );

	// the whole name
	pos = MAT_LowerBound( key, -1 );
	if ( pos < g_matTextureCount && !MAT_Compare( g_matTextures[ pos ].name, key, -1 ) )
		return &g_matTextures[ pos ];

	// HL compared the first 12 characters of both names: textures that matched in HL still match. The entries
	// equal on 12 characters are next to each other in the sorted table.
	for ( pos = MAT_LowerBound( key, MAT_LEGACY_CHARS ); pos < g_matTextureCount; pos++ )
	{
		const mat_texture_t *entry = &g_matTextures[ pos ];

		if ( MAT_Compare( entry->name, key, MAT_LEGACY_CHARS ) )
			break;
		if ( !best || entry->layer > best->layer || ( entry->layer == best->layer && entry->seq < best->seq ) )
			best = entry;
	}
	return best;
}

char MAT_FindTextureType( const char *name )
{
	const mat_texture_t *entry = MAT_FindEntry( name );

	return entry ? entry->letter : CHAR_TEX_CONCRETE;
}

char MAT_FindTextureMatch( const char *name, char *matched, int *layer )
{
	const mat_texture_t *entry = MAT_FindEntry( name );

	if ( matched )
		MAT_CopyString( matched, entry ? entry->name : "", CBTEXTURENAMEMAX );
	if ( layer )
		*layer = entry ? entry->layer : -1;
	return entry ? entry->letter : CHAR_TEX_CONCRETE;
}

int MAT_TextureCount( void )
{
	return g_matTextureCount;
}

static int MAT_IsSpace( int c )
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

static int MAT_IsAlpha( int c )
{
	return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' );
}

typedef void ( *mat_directive_fn )( const char *word, const char *argument, const char *source, int line, void *ctx );
// an inline types block of a map's file: data[ 0 .. length ) from its first word through the line of its closing
// brace, starting on line `line`
typedef void ( *mat_block_fn )( const char *data, int length, const char *source, int line, void *ctx );

static int MAT_IsBlockWord( const char *data, int i, int end )
{
	static const char *words[] = { "material", "texture", "ladder", "wade" };
	int w;

	for ( w = 0; w < 4; w++ )
	{
		int n = (int)strlen( words[ w ] ), c;

		if ( end - i < n || MAT_Compare( data + i, words[ w ], n ) )
			continue;
		c = i + n < end ? (unsigned char)data[ i + n ] : ' ';
		if ( MAT_IsSpace( c ) || c == '{' || c == '"' || c == '/' )
			return 1;
	}
	return 0;
}

// the end of an inline block starting at data[ start ]: after the newline of the line that holds the brace closing
// it (or the end of the data). Strings end at their line's end and // comments are skipped, as in the tokenizer.
// *lines: the newlines passed.
static int MAT_BlockEnd( const char *data, int length, int start, int *lines )
{
	int i = start, depth = 0, opened = 0;

	*lines = 0;
	while ( i < length && data[ i ] != 0 )
	{
		char c = data[ i ];

		if ( c == '\n' )
		{
			( *lines )++;
			if ( opened && depth == 0 )
				return i + 1;
			i++;
		}
		else if ( c == '"' )
		{
			for ( i++; i < length && data[ i ] != '"' && data[ i ] != '\n' && data[ i ] != 0; i++ )
				;
			if ( i < length && data[ i ] == '"' )
				i++;
		}
		else if ( c == '/' && i + 1 < length && data[ i + 1 ] == '/' )
		{
			while ( i < length && data[ i ] != '\n' && data[ i ] != 0 )
				i++;
		}
		else
		{
			if ( c == '{' )
			{
				depth++;
				opened = 1;
			}
			else if ( c == '}' && depth > 0 )
				depth--;
			i++;
		}
	}
	return i;
}

// textures 0: the texture lines are skipped (a second pass over a map's file). block: the map files' inline types
// blocks (NULL: Half-Life's format only, as sound/materials.txt)
static int MAT_ParseList( const char *data, int length, int layer, const char *source, int textures,
	mat_directive_fn directive, mat_block_fn block, void *ctx )
{
	int pos = 0, line = 0, added = 0, comment = 0;

	g_matLogCount = 0;
	if ( !data || length <= 0 )
		return 0;
	if ( length > MAT_FILE_MAX )
	{
		MAT_Log( "materials: %s is larger than %d bytes, not read\n", source, MAT_FILE_MAX );
		return 0;
	}

	while ( pos < length )
	{
		int end = pos, i, start, n;
		int letter;

		// one line: up to '\n' (a NUL byte also ends it)
		while ( end < length && data[ end ] != '\n' && data[ end ] != 0 )
			end++;
		line++;

		i = pos;
		pos = end + 1;

		// /* ... */ over whole lines (Valve's file ends with commented-out code, which HL read as textures)
		if ( comment )
		{
			for ( ; i + 1 < end; i++ )
			{
				if ( data[ i ] == '*' && data[ i + 1 ] == '/' )
				{
					comment = 0;
					break;
				}
			}
			continue;
		}

		while ( i < end && MAT_IsSpace( (unsigned char)data[ i ] ) )
			i++;
		if ( i >= end )
			continue;

		if ( i + 1 < end && data[ i ] == '/' && data[ i + 1 ] == '*' )
		{
			for ( i += 2; i + 1 < end; i++ )
			{
				if ( data[ i ] == '*' && data[ i + 1 ] == '/' )
					break;
			}
			comment = i + 1 >= end;
			continue;
		}

		// SBO 2.0 map files: inline types blocks, handed over whole (they may span lines)
		if ( block && MAT_IsBlockWord( data, i, end ) )
		{
			int lines, blockEnd = MAT_BlockEnd( data, length, i, &lines );

			block( data + i, blockEnd - i, source, line, ctx );
			pos = blockEnd;
			line += lines - 1;		// the loop counts the line after the block's last one
			continue;
		}

		// SBO 2.0 per-map files: "#include <file>", "#types <file>" (HL and Sven Co-op skip these lines)
		if ( data[ i ] == '#' && directive )
		{
			char word[ 16 ], argument[ 80 ];
			int w = 0, a = 0;

			for ( i++; i < end && !MAT_IsSpace( (unsigned char)data[ i ] ) && w < (int)sizeof( word ) - 1; i++ )
				word[ w++ ] = data[ i ];
			word[ w ] = 0;
			while ( i < end && MAT_IsSpace( (unsigned char)data[ i ] ) )
				i++;
			if ( i < end && data[ i ] == '"' )
				i++;
			for ( ; i < end && data[ i ] != '"' && !MAT_IsSpace( (unsigned char)data[ i ] ); i++ )
			{
				if ( a >= (int)sizeof( argument ) - 1 )
				{
					a = 0;
					break;
				}
				argument[ a++ ] = data[ i ];
			}
			argument[ a ] = 0;
			directive( word, argument, source, line, ctx );
			continue;
		}

		// comments and anything that does not start with a letter (HL)
		if ( !MAT_IsAlpha( (unsigned char)data[ i ] ) || !textures )
			continue;

		letter = (unsigned char)data[ i++ ];
		if ( letter >= 'a' && letter <= 'z' )
			letter -= 'a' - 'A';

		// the letter stands alone ("C NAME")
		if ( i >= end || !MAT_IsSpace( (unsigned char)data[ i ] ) )
		{
			MAT_Log( "materials: %s line %d: no space after the material letter\n", source, line );
			continue;
		}
		while ( i < end && MAT_IsSpace( (unsigned char)data[ i ] ) )
			i++;

		start = i;
		while ( i < end && !MAT_IsSpace( (unsigned char)data[ i ] ) )
		{
			int c = (unsigned char)data[ i ];

			if ( c < 0x21 || c > 0x7e )
				break;
			i++;
		}
		n = i - start;

		if ( n <= 0 || ( i < end && !MAT_IsSpace( (unsigned char)data[ i ] ) ) )
		{
			MAT_Log( "materials: %s line %d: no texture name\n", source, line );
			continue;
		}
		if ( n > CBTEXTURENAMEMAX - 1 )
		{
			MAT_Log( "materials: %s line %d: texture name longer than %d characters\n", source, line, CBTEXTURENAMEMAX - 1 );
			continue;
		}

		{
			char name[ CBTEXTURENAMEMAX ];

			memcpy( name, data + start, n );
			name[ n ] = 0;
			MAT_AddTexture( name, (char)letter, layer );
			added++;
		}
	}

	return added;
}

int MAT_ParseTextureList( const char *data, int length, int layer, const char *source )
{
	return MAT_ParseList( data, length, layer, source, 1, NULL, NULL, NULL );
}

// ---------------------------------------------------------------------------------------------------------------
// types file: tokens are words, "quoted strings", { and }; // comments to the end of the line
// ---------------------------------------------------------------------------------------------------------------

typedef struct mat_parser_s
{
	const char	*data;
	int			length;
	int			pos;
	int			line;
	int			error;
	const char	*source;
	int			quoted;					// the last token was a quoted string
	char		token[ MAT_TOKEN_LEN ];
} mat_parser_t;

static void MAT_Error( mat_parser_t *p, const char *what )
{
	if ( !p->error )
		MAT_Log( "materials: %s line %d: %s\n", p->source, p->line, what );
	p->error = 1;
}

// next token into p->token; 0 at the end of the data or on an error
static int MAT_Token( mat_parser_t *p )
{
	int n = 0;

	p->token[ 0 ] = 0;
	p->quoted = 0;
	if ( p->error )
		return 0;

	for ( ;; )
	{
		while ( p->pos < p->length && ( MAT_IsSpace( (unsigned char)p->data[ p->pos ] ) || p->data[ p->pos ] == '\n' ) )
		{
			if ( p->data[ p->pos ] == '\n' )
				p->line++;
			p->pos++;
		}
		if ( p->pos + 1 < p->length && p->data[ p->pos ] == '/' && p->data[ p->pos + 1 ] == '/' )
		{
			while ( p->pos < p->length && p->data[ p->pos ] != '\n' )
				p->pos++;
			continue;
		}
		break;
	}

	if ( p->pos >= p->length )
		return 0;

	if ( p->data[ p->pos ] == 0 )
	{
		MAT_Error( p, "NUL byte" );
		return 0;
	}

	if ( p->data[ p->pos ] == '{' || p->data[ p->pos ] == '}' )
	{
		p->token[ 0 ] = p->data[ p->pos++ ];
		p->token[ 1 ] = 0;
		return 1;
	}

	if ( p->data[ p->pos ] == '"' )
	{
		p->pos++;
		while ( p->pos < p->length && p->data[ p->pos ] != '"' )
		{
			if ( p->data[ p->pos ] == '\n' || p->data[ p->pos ] == 0 )
			{
				MAT_Error( p, "string not closed on its line" );
				return 0;
			}
			if ( n >= MAT_TOKEN_LEN - 1 )
			{
				MAT_Error( p, "string too long" );
				return 0;
			}
			p->token[ n++ ] = p->data[ p->pos++ ];
		}
		if ( p->pos >= p->length )
		{
			MAT_Error( p, "string not closed" );
			return 0;
		}
		p->pos++;
		p->token[ n ] = 0;
		p->quoted = 1;
		return 1;
	}

	while ( p->pos < p->length )
	{
		int c = (unsigned char)p->data[ p->pos ];

		if ( MAT_IsSpace( c ) || c == '\n' || c == '{' || c == '}' || c == '"' || c == 0 )
			break;
		if ( c == '/' && p->pos + 1 < p->length && p->data[ p->pos + 1 ] == '/' )
			break;
		if ( n >= MAT_TOKEN_LEN - 1 )
		{
			MAT_Error( p, "word too long" );
			return 0;
		}
		p->token[ n++ ] = (char)c;
		p->pos++;
	}
	p->token[ n ] = 0;
	return 1;
}

static int MAT_IsBrace( mat_parser_t *p, char brace )
{
	return !p->quoted && p->token[ 0 ] == brace && !p->token[ 1 ];
}

static int MAT_Expect( mat_parser_t *p, char brace )
{
	if ( !MAT_Token( p ) || !MAT_IsBrace( p, brace ) )
	{
		MAT_Error( p, brace == '{' ? "'{' expected" : "'}' expected" );
		return 0;
	}
	return 1;
}

// a value: never a brace
static int MAT_Value( mat_parser_t *p )
{
	if ( !MAT_Token( p ) || MAT_IsBrace( p, '{' ) || MAT_IsBrace( p, '}' ) )
	{
		MAT_Error( p, "value expected" );
		return 0;
	}
	return 1;
}

// decimal number without exponent; inf / nan / garbage are errors
static int MAT_Number( mat_parser_t *p, float *out, float lo, float hi )
{
	const char *s;
	double value = 0.0, scale = 1.0;
	int negative = 0, digits = 0;

	if ( !MAT_Value( p ) )
		return 0;

	s = p->token;
	if ( *s == '-' || *s == '+' )
		negative = *s++ == '-';
	while ( *s >= '0' && *s <= '9' )
	{
		if ( value < 1e9 )
			value = value * 10.0 + ( *s - '0' );
		s++;
		digits++;
	}
	if ( *s == '.' )
	{
		s++;
		while ( *s >= '0' && *s <= '9' )
		{
			scale *= 0.1;
			if ( scale > 1e-9 )
				value += ( *s - '0' ) * scale;
			s++;
			digits++;
		}
	}
	if ( *s || !digits )
	{
		MAT_Error( p, "number expected" );
		return 0;
	}
	if ( negative )
		value = -value;
	if ( value < lo )
		value = lo;
	if ( value > hi )
		value = hi;
	*out = (float)value;
	return 1;
}

static int MAT_Int( mat_parser_t *p, int *out, int lo, int hi )
{
	float value;

	if ( !MAT_Number( p, &value, (float)lo, (float)hi ) )
		return 0;
	*out = (int)value;
	return 1;
}

int MAT_IsSafeSoundPath( const char *path )
{
	return MAT_IsSafeAssetPath( path, ".wav" );
}

int MAT_IsSafeAssetPath( const char *path, const char *extension )
{
	int n, i;

	if ( !path || !extension || strlen( extension ) != 4 )
		return 0;
	n = (int)strlen( path );
	if ( n < 5 || n > MAT_SOUND_LEN - 1 )
		return 0;
	if ( path[ 0 ] == '/' || path[ 0 ] == '.' )
		return 0;
	for ( i = 0; i < n; i++ )
	{
		int c = (unsigned char)path[ i ];

		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' || c == '.' || c == '/' ) )
			return 0;
		if ( c == '.' && path[ i + 1 ] == '.' )
			return 0;
		if ( c == '/' && path[ i + 1 ] == '/' )
			return 0;
	}
	return !MAT_Compare( path + n - 4, extension, -1 );
}

// { "a.wav" "b.wav" }
static int MAT_SoundList( mat_parser_t *p, mat_sounds_t *list )
{
	if ( !MAT_Expect( p, '{' ) )
		return 0;

	list->count = 0;
	for ( ;; )
	{
		if ( !MAT_Token( p ) )
		{
			MAT_Error( p, "'}' expected" );
			return 0;
		}
		if ( MAT_IsBrace( p, '}' ) )
			return 1;
		if ( MAT_IsBrace( p, '{' ) )
		{
			MAT_Error( p, "sound path expected" );
			return 0;
		}
		if ( list->count >= MAT_SOUNDS_MAX )
		{
			MAT_Error( p, "too many sounds in a list" );
			return 0;
		}
		if ( !MAT_IsSafeSoundPath( p->token ) )
		{
			MAT_Error( p, "bad sound path (relative to sound/, letters, digits, _ - . / only, .wav)" );
			return 0;
		}
		MAT_CopyString( list->name[ list->count++ ], p->token, MAT_SOUND_LEN );
	}
}

// skips the value of a key this version does not know: one token or one { } block (no nesting)
static int MAT_SkipValue( mat_parser_t *p )
{
	if ( !MAT_Token( p ) )
	{
		MAT_Error( p, "value expected" );
		return 0;
	}
	if ( MAT_IsBrace( p, '}' ) )
	{
		MAT_Error( p, "value expected" );
		return 0;
	}
	if ( !MAT_IsBrace( p, '{' ) )
		return 1;
	for ( ;; )
	{
		if ( !MAT_Token( p ) )
		{
			MAT_Error( p, "'}' expected" );
			return 0;
		}
		if ( MAT_IsBrace( p, '}' ) )
			return 1;
		if ( MAT_IsBrace( p, '{' ) )
		{
			MAT_Error( p, "nested block" );
			return 0;
		}
	}
}

// a step key of a material, ladder or wade block; 1 handled, 0 not a step key, -1 error
static int MAT_StepKey( mat_parser_t *p, const char *key, mat_step_t *step )
{
	if ( !MAT_Compare( key, "step_right", -1 ) )
		return MAT_SoundList( p, &step->right ) ? 1 : -1;
	if ( !MAT_Compare( key, "step_left", -1 ) )
		return MAT_SoundList( p, &step->left ) ? 1 : -1;
	if ( !MAT_Compare( key, "walk_volume", -1 ) )
		return MAT_Number( p, &step->walk_volume, 0.0f, 1.0f ) ? 1 : -1;
	if ( !MAT_Compare( key, "run_volume", -1 ) )
		return MAT_Number( p, &step->run_volume, 0.0f, 1.0f ) ? 1 : -1;
	if ( !MAT_Compare( key, "walk_time", -1 ) )
		return MAT_Int( p, &step->walk_time, 50, 5000 ) ? 1 : -1;
	if ( !MAT_Compare( key, "run_time", -1 ) )
		return MAT_Int( p, &step->run_time, 50, 5000 ) ? 1 : -1;
	if ( !MAT_Compare( key, "step_skip", -1 ) )
		return MAT_Int( p, &step->skip_steps, 0, 1 ) ? 1 : -1;
	return 0;
}

static void MAT_FxDefaults( mat_fx_t *fx )
{
	memset( fx, 0, sizeof( *fx ) );
	fx->debris_size = 5.0f;
	fx->debris_life = 8.0f;
	fx->debris_speed = 1.0f;
	fx->debris_gravity = 1.0f;
	fx->debris_damp_time = 0.3f;
	fx->debris_damp_speed = 0.85f;
	fx->debris_afterdamp_gravity = 1.0f;
	fx->debris_brightness = 200.0f;
	fx->debris_color[ 0 ] = fx->debris_color[ 1 ] = fx->debris_color[ 2 ] = 125.0f;
	fx->smoke_color[ 0 ] = fx->smoke_color[ 1 ] = fx->smoke_color[ 2 ] = 175.0f;
}

static int MAT_Color( mat_parser_t *p, float *color )
{
	return MAT_Number( p, &color[ 0 ], 0.0f, 255.0f ) && MAT_Number( p, &color[ 1 ], 0.0f, 255.0f ) &&
		MAT_Number( p, &color[ 2 ], 0.0f, 255.0f );
}

// { "sprites/a.spr" "sprites/b.spr" }
static int MAT_SpriteList( mat_parser_t *p, mat_fx_t *fx )
{
	if ( !MAT_Expect( p, '{' ) )
		return 0;

	fx->debris_sprites = 0;
	for ( ;; )
	{
		if ( !MAT_Token( p ) )
		{
			MAT_Error( p, "'}' expected" );
			return 0;
		}
		if ( MAT_IsBrace( p, '}' ) )
			return 1;
		if ( MAT_IsBrace( p, '{' ) )
		{
			MAT_Error( p, "sprite path expected" );
			return 0;
		}
		if ( fx->debris_sprites >= MAT_FX_SPRITES )
		{
			MAT_Error( p, "too many debris sprites (4 at most)" );
			return 0;
		}
		if ( !MAT_IsSafeAssetPath( p->token, ".spr" ) )
		{
			MAT_Error( p, "bad sprite path (letters, digits, _ - . / only, .spr)" );
			return 0;
		}
		MAT_CopyString( fx->debris_sprite[ fx->debris_sprites++ ], p->token, MAT_SOUND_LEN );
	}
}

// an impact effect key (fx_*) of a material or texture block; 1 handled, 0 not an effect key, -1 error
static int MAT_FxKey( mat_parser_t *p, const char *key, mat_fx_t *fx )
{
	int ok;

	if ( MAT_Compare( key, "fx_", 3 ) )
		return 0;
	key += 3;

	if ( !MAT_Compare( key, "debris", -1 ) )
		ok = MAT_SpriteList( p, fx );
	else if ( !MAT_Compare( key, "debris_count", -1 ) )
		ok = MAT_Int( p, &fx->debris_count, 0, 8 );
	else if ( !MAT_Compare( key, "debris_size", -1 ) )
		ok = MAT_Number( p, &fx->debris_size, 0.5f, 32.0f );
	else if ( !MAT_Compare( key, "debris_life", -1 ) )
		ok = MAT_Number( p, &fx->debris_life, 0.1f, 30.0f );
	else if ( !MAT_Compare( key, "debris_speed", -1 ) )
		ok = MAT_Number( p, &fx->debris_speed, 0.0f, 4.0f );
	else if ( !MAT_Compare( key, "debris_gravity", -1 ) )
		ok = MAT_Number( p, &fx->debris_gravity, -4.0f, 4.0f );
	else if ( !MAT_Compare( key, "debris_damp_time", -1 ) )
		ok = MAT_Number( p, &fx->debris_damp_time, 0.0f, 10.0f );
	else if ( !MAT_Compare( key, "debris_damp_speed", -1 ) )
		ok = MAT_Number( p, &fx->debris_damp_speed, 0.0f, 4.0f );
	else if ( !MAT_Compare( key, "debris_afterdamp_gravity", -1 ) )
		ok = MAT_Number( p, &fx->debris_afterdamp_gravity, -4.0f, 4.0f );
	else if ( !MAT_Compare( key, "debris_brightness", -1 ) )
		ok = MAT_Number( p, &fx->debris_brightness, 0.0f, 255.0f );
	else if ( !MAT_Compare( key, "debris_additive", -1 ) )
		ok = MAT_Int( p, &fx->debris_additive, 0, 1 );
	else if ( !MAT_Compare( key, "debris_color", -1 ) )
		ok = MAT_Color( p, fx->debris_color );
	else if ( !MAT_Compare( key, "smoke", -1 ) )
		ok = MAT_Int( p, &fx->smoke, 0, 1 );
	else if ( !MAT_Compare( key, "smoke_color", -1 ) )
		ok = MAT_Color( p, fx->smoke_color );
	else if ( !MAT_Compare( key, "sparks", -1 ) )
		ok = MAT_Int( p, &fx->sparks, 0, 1 );
	else if ( !MAT_Compare( key, "ricochet", -1 ) )
		ok = MAT_SoundList( p, &fx->ricochet );
	else if ( !MAT_Compare( key, "decal", -1 ) )
	{
		int i;

		ok = MAT_Value( p );
		for ( i = 0; ok && p->token[ i ]; i++ )
		{
			int c = (unsigned char)p->token[ i ];

			if ( c < 0x21 || c > 0x7e || c == '"' || c == '\\' )
				ok = 0;
		}
		if ( ok && ( !p->token[ 0 ] || strlen( p->token ) > MAT_DECAL_LEN - 1 ) )
			ok = 0;
		if ( !ok )
		{
			MAT_Error( p, "decal name expected (a decals.wad name, 15 characters at most)" );
			return -1;
		}
		MAT_CopyString( fx->decal, p->token, MAT_DECAL_LEN );
	}
	else
		return 0;

	if ( !ok )
		return -1;
	fx->defined = 1;
	return 1;
}

static int MAT_UnknownKey( mat_parser_t *p, const char *key )
{
	MAT_Log( "materials: %s line %d: unknown key \"%s\" ignored\n", p->source, p->line, key );
	return MAT_SkipValue( p );
}

static int MAT_MaterialBlock( mat_parser_t *p, mat_type_t *type )
{
	char key[ MAT_TOKEN_LEN ];
	int result;

	if ( !MAT_Expect( p, '{' ) )
		return 0;

	for ( ;; )
	{
		if ( !MAT_Token( p ) )
		{
			MAT_Error( p, "'}' expected" );
			return 0;
		}
		if ( MAT_IsBrace( p, '}' ) )
			return 1;
		if ( MAT_IsBrace( p, '{' ) )
		{
			MAT_Error( p, "key expected" );
			return 0;
		}
		MAT_CopyString( key, p->token, sizeof( key ) );

		result = MAT_StepKey( p, key, &type->step );
		if ( result < 0 )
			return 0;
		if ( result > 0 )
		{
			type->has_step = 1;
			continue;
		}

		result = MAT_FxKey( p, key, &type->fx );
		if ( result < 0 )
			return 0;
		if ( result > 0 )
			continue;

		if ( !MAT_Compare( key, "name", -1 ) )
		{
			if ( !MAT_Value( p ) )
				return 0;
			MAT_CopyString( type->name, p->token, MAT_NAME_LEN );
		}
		else if ( !MAT_Compare( key, "hit", -1 ) )
		{
			if ( !MAT_SoundList( p, &type->hit.sounds ) )
				return 0;
			type->has_hit = 1;
		}
		else if ( !MAT_Compare( key, "hit_volume", -1 ) )
		{
			if ( !MAT_Number( p, &type->hit.volume, 0.0f, 1.0f ) )
				return 0;
			type->has_hit = 1;
		}
		else if ( !MAT_Compare( key, "hit_volume_bar", -1 ) )
		{
			if ( !MAT_Number( p, &type->hit.volume_bar, 0.0f, 1.0f ) )
				return 0;
			type->has_hit = 1;
		}
		else if ( !MAT_Compare( key, "hit_attenuation", -1 ) )
		{
			if ( !MAT_Number( p, &type->hit.attenuation, 0.0f, 4.0f ) )
				return 0;
			type->has_hit = 1;
		}
		else if ( !MAT_Compare( key, "hit_sparks", -1 ) )
		{
			if ( !MAT_Int( p, &type->hit.sparks, 0, 1 ) )
				return 0;
			type->has_hit = 1;
		}
		else if ( !MAT_UnknownKey( p, key ) )
			return 0;
	}
}

static int MAT_StepBlock( mat_parser_t *p, mat_step_t *step )
{
	char key[ MAT_TOKEN_LEN ];
	int result;

	if ( !MAT_Expect( p, '{' ) )
		return 0;

	for ( ;; )
	{
		if ( !MAT_Token( p ) )
		{
			MAT_Error( p, "'}' expected" );
			return 0;
		}
		if ( MAT_IsBrace( p, '}' ) )
			return 1;
		if ( MAT_IsBrace( p, '{' ) )
		{
			MAT_Error( p, "key expected" );
			return 0;
		}
		MAT_CopyString( key, p->token, sizeof( key ) );

		result = MAT_StepKey( p, key, step );
		if ( result < 0 )
			return 0;
		if ( !result && !MAT_UnknownKey( p, key ) )
			return 0;
	}
}

// texture "NAME" { fx keys }: the effects of one texture, replacing its letter's
static int MAT_TextureBlock( mat_parser_t *p, mat_types_t *types )
{
	char key[ MAT_TOKEN_LEN ];
	char name[ CBTEXTURENAMEMAX ];
	mat_texfx_t *entry = NULL;
	int i, result;

	if ( !MAT_Value( p ) )
		return 0;
	if ( !p->token[ 0 ] || strlen( p->token ) > CBTEXTURENAMEMAX - 1 )
	{
		MAT_Error( p, "texture name expected (15 characters at most)" );
		return 0;
	}
	MAT_StripTextureName( p->token, name );

	for ( i = 0; i < types->texfxCount; i++ )
	{
		if ( !MAT_Compare( types->texfx[ i ].name, name, -1 ) )
			entry = &types->texfx[ i ];
	}
	if ( !entry )
	{
		if ( types->texfxCount >= MAT_TEXFX_MAX )
		{
			MAT_Error( p, "too many texture blocks (32 at most)" );
			return 0;
		}
		entry = &types->texfx[ types->texfxCount++ ];
		MAT_CopyString( entry->name, name, CBTEXTURENAMEMAX );
	}
	MAT_FxDefaults( &entry->fx );
	entry->fx.defined = 1;		// a texture block always has its own effects, even "none"

	if ( !MAT_Expect( p, '{' ) )
		return 0;
	for ( ;; )
	{
		if ( !MAT_Token( p ) )
		{
			MAT_Error( p, "'}' expected" );
			return 0;
		}
		if ( MAT_IsBrace( p, '}' ) )
			return 1;
		if ( MAT_IsBrace( p, '{' ) )
		{
			MAT_Error( p, "key expected" );
			return 0;
		}
		MAT_CopyString( key, p->token, sizeof( key ) );
		result = MAT_FxKey( p, key, &entry->fx );
		if ( result < 0 )
			return 0;
		if ( !result && !MAT_UnknownKey( p, key ) )
			return 0;
	}
}

static int MAT_StepUsable( const mat_step_t *step )
{
	return step->right.count > 0 && step->left.count > 0;
}

static int MAT_ParseTypesAt( const char *data, int length, const char *source, int firstLine );

int MAT_ParseTypes( const char *data, int length, const char *source )
{
	return MAT_ParseTypesAt( data, length, source, 1 );
}

static int MAT_ParseTypesAt( const char *data, int length, const char *source, int firstLine )
{
	mat_parser_t parser;
	mat_parser_t *p = &parser;
	int i;

	g_matLogCount = 0;
	if ( !data || length <= 0 )
		return 0;
	if ( length > MAT_FILE_MAX )
	{
		MAT_Log( "materials: %s is larger than %d bytes, not read\n", source, MAT_FILE_MAX );
		return 0;
	}

	memset( p, 0, sizeof( *p ) );
	p->data = data;
	p->length = length;
	p->line = firstLine;
	p->source = source;

	g_matScratch = g_matTypes;

	while ( MAT_Token( p ) )
	{
		if ( p->quoted || MAT_IsBrace( p, '{' ) || MAT_IsBrace( p, '}' ) )
		{
			MAT_Error( p, "\"material\", \"texture\", \"ladder\" or \"wade\" expected" );
			break;
		}

		if ( !MAT_Compare( p->token, "material", -1 ) )
		{
			int index;
			mat_type_t *type;

			if ( !MAT_Value( p ) )
				break;
			index = p->token[ 1 ] ? -1 : MAT_LetterIndex( p->token[ 0 ] );
			if ( index < 0 )
			{
				MAT_Error( p, "material letter A-Z expected" );
				break;
			}
			type = &g_matScratch.type[ index ];
			if ( !type->defined )
				MAT_NewType( &g_matScratch, (char)( 'A' + index ), "" );
			if ( !MAT_MaterialBlock( p, type ) )
				break;
			if ( type->has_step && !MAT_StepUsable( &type->step ) )
			{
				MAT_Error( p, "a material with steps needs step_right and step_left" );
				break;
			}
			if ( type->has_hit && type->hit.sounds.count <= 0 )
			{
				MAT_Error( p, "a material with hit values needs hit sounds" );
				break;
			}
		}
		else if ( !MAT_Compare( p->token, "texture", -1 ) )
		{
			if ( !MAT_TextureBlock( p, &g_matScratch ) )
				break;
		}
		else if ( !MAT_Compare( p->token, "ladder", -1 ) || !MAT_Compare( p->token, "wade", -1 ) )
		{
			mat_step_t *step = !MAT_Compare( p->token, "ladder", -1 ) ? &g_matScratch.ladder : &g_matScratch.wade;

			if ( !MAT_StepBlock( p, step ) )
				break;
			if ( !MAT_StepUsable( step ) )
			{
				MAT_Error( p, "ladder and wade need step_right and step_left" );
				break;
			}
		}
		else
		{
			MAT_Error( p, "\"material\", \"texture\", \"ladder\" or \"wade\" expected" );
			break;
		}
	}

	if ( p->error )
	{
		MAT_Log( "materials: %s not used\n", source );
		return 0;
	}

	// the default and flesh materials always have steps and hits
	for ( i = 0; i < MAT_TYPES; i++ )
	{
		if ( !g_matScratch.type[ i ].defined )
			continue;
		if ( g_matScratch.type[ i ].name[ 0 ] == 0 )
			MAT_CopyString( g_matScratch.type[ i ].name, "custom", MAT_NAME_LEN );
	}
	if ( !g_matScratch.type[ CHAR_TEX_CONCRETE - 'A' ].has_step || !g_matScratch.type[ CHAR_TEX_CONCRETE - 'A' ].has_hit )
	{
		MAT_Log( "materials: %s: material C must keep its steps and hit sounds; not used\n", source );
		return 0;
	}

	g_matTypes = g_matScratch;
	return 1;
}

// ---------------------------------------------------------------------------------------------------------------
// lookups
// ---------------------------------------------------------------------------------------------------------------

char MAT_DefaultType( void )
{
	return CHAR_TEX_CONCRETE;
}

char MAT_FleshType( void )
{
	return CHAR_TEX_FLESH;
}

char MAT_SloshType( void )
{
	return CHAR_TEX_SLOSH;
}

void MAT_SetUsable( char letter, int usable )
{
	int index = MAT_LetterIndex( letter );

	if ( index >= 0 )
		g_matUsable[ index ] = usable;
}

int MAT_IsUsable( char letter )
{
	int index = MAT_LetterIndex( letter );

	return index >= 0 && g_matUsable[ index ];
}

const mat_type_t *MAT_GetType( char letter )
{
	int index = MAT_LetterIndex( letter );

	if ( !g_matLoaded )
		MAT_Reset();
	if ( index < 0 || !g_matTypes.type[ index ].defined )
		return NULL;
	return &g_matTypes.type[ index ];
}

// a defined type that may be played (server: its sounds are precached)
static const mat_type_t *MAT_PlayableType( char letter )
{
	const mat_type_t *type = MAT_GetType( letter );

	if ( type && !MAT_IsUsable( letter ) )
		type = NULL;
	return type;
}

const mat_step_t *MAT_GetStep( char letter )
{
	const mat_type_t *type = MAT_PlayableType( letter );

	if ( type && type->has_step )
		return &type->step;
	return &MAT_GetType( CHAR_TEX_CONCRETE )->step;
}

const mat_hit_t *MAT_GetHit( char letter )
{
	const mat_type_t *type = MAT_PlayableType( letter );

	if ( type && type->has_hit )
		return &type->hit;
	return &MAT_GetType( CHAR_TEX_CONCRETE )->hit;
}

const mat_step_t *MAT_GetLadderStep( void )
{
	if ( !g_matLoaded )
		MAT_Reset();
	return &g_matTypes.ladder;
}

const mat_step_t *MAT_GetWadeStep( void )
{
	if ( !g_matLoaded )
		MAT_Reset();
	return &g_matTypes.wade;
}

const mat_step_t *MAT_GetSloshStep( void )
{
	return MAT_GetStep( CHAR_TEX_SLOSH );
}

const mat_fx_t *MAT_GetFx( char letter, const char *strippedName )
{
	static mat_fx_t none;		// all 0: the 1.38 effect
	const mat_type_t *type;
	int i;

	if ( !g_matLoaded )
		MAT_Reset();
	if ( strippedName && strippedName[ 0 ] )
	{
		for ( i = 0; i < g_matTypes.texfxCount; i++ )
		{
			if ( !MAT_Compare( g_matTypes.texfx[ i ].name, strippedName, -1 ) )
				return &g_matTypes.texfx[ i ].fx;
		}
	}
	type = MAT_GetType( letter );
	return type ? &type->fx : &none;
}

void MAT_ForEachFxFile( const mat_fx_t *fx, mat_file_fn onFile, void *ctx )
{
	char path[ MAT_SOUND_LEN + 8 ];
	int i;

	if ( !fx || !fx->defined || !onFile )
		return;
	for ( i = 0; i < fx->debris_sprites && i < MAT_FX_SPRITES; i++ )
		onFile( fx->debris_sprite[ i ], ctx );
	for ( i = 0; i < fx->ricochet.count && i < MAT_SOUNDS_MAX; i++ )
	{
		if ( snprintf( path, sizeof( path ), "sound/%s", fx->ricochet.name[ i ] ) < MAT_PATH_LEN )
			onFile( path, ctx );
	}
}

void MAT_ForEachFx( void ( *callback )( const mat_fx_t *fx, void *ctx ), void *ctx )
{
	int i;

	if ( !callback )
		return;
	if ( !g_matLoaded )
		MAT_Reset();
	for ( i = 0; i < MAT_TYPES; i++ )
	{
		if ( g_matTypes.type[ i ].defined )
			callback( &g_matTypes.type[ i ].fx, ctx );
	}
	for ( i = 0; i < g_matTypes.texfxCount; i++ )
		callback( &g_matTypes.texfx[ i ].fx, ctx );
}

// ---------------------------------------------------------------------------------------------------------------
// files
// ---------------------------------------------------------------------------------------------------------------

void MAT_LoadGlobal( mat_load_fn load, mat_free_fn release )
{
	unsigned char *data;
	int length = 0;

	MAT_Reset();
	g_matFilesLoaded = 1;
	if ( !load || !release )
		return;

	data = load( MAT_TEXTURE_LIST, &length );
	if ( data )
	{
		MAT_ParseTextureList( (const char *)data, length, MAT_LAYER_VALVE, MAT_TEXTURE_LIST );
		release( data );
	}

	length = 0;
	data = load( MAT_TEXTURES_FILE, &length );
	if ( data )
	{
		MAT_ParseTextureList( (const char *)data, length, MAT_LAYER_GLOBAL, MAT_TEXTURES_FILE );
		release( data );
	}

	length = 0;
	data = load( MAT_TYPES_FILE, &length );
	if ( data )
	{
		MAT_ParseTypes( (const char *)data, length, MAT_TYPES_FILE );
		release( data );
	}
}

int MAT_IsSafeDataPath( const char *path )
{
	int n, i, dots = 0;

	if ( !path )
		return 0;
	n = (int)strlen( path );
	if ( n < 5 || n > MAT_PATH_LEN - 1 )
		return 0;
	if ( path[ 0 ] == '/' || path[ 0 ] == '.' )
		return 0;
	for ( i = 0; i < n; i++ )
	{
		int c = (unsigned char)path[ i ];

		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' || c == '.' || c == '/' ) )
			return 0;
		if ( c == '/' && path[ i + 1 ] == '/' )
			return 0;
		if ( c == '.' )
			dots++;
	}
	// one dot: the engine does not send a file with more than one dot in its name to the clients
	return dots == 1 && !MAT_Compare( path + n - 4, ".txt", -1 );
}

typedef struct mat_mapload_s
{
	mat_load_fn		load;
	mat_free_fn		release;
	mat_file_fn		onFile;
	void			*ctx;
	int				depth;
	int				files;
	char			loaded[ MAT_MAP_FILES_MAX ][ MAT_PATH_LEN ];
} mat_mapload_t;

static int MAT_LoadListFile( mat_mapload_t *m, const char *path );

// a map file is read in two passes: 1 its texture lines and #include (the included files are read whole, both their
// passes), 2 its #types and inline blocks in file order. So a file's own types come after those of its includes.
typedef struct mat_mappass_s
{
	mat_mapload_t	*m;
	int				pass;
} mat_mappass_t;

static void MAT_MapBlock( const char *data, int length, const char *source, int line, void *ctx )
{
	if ( ( (mat_mappass_t *)ctx )->pass == 2 )
		MAT_ParseTypesAt( data, length, source, line );
}

static void MAT_MapDirective( const char *word, const char *argument, const char *source, int line, void *ctx )
{
	mat_mappass_t *pass = (mat_mappass_t *)ctx;
	mat_mapload_t *m = pass->m;
	int isTypes = !MAT_Compare( word, "types", -1 );

	if ( !isTypes && MAT_Compare( word, "include", -1 ) )
	{
		if ( pass->pass == 1 )
			MAT_Log( "materials: %s line %d: unknown #%s ignored\n", source, line, word );
		return;
	}
	if ( isTypes != ( pass->pass == 2 ) )
		return;		// #include in pass 1, #types in pass 2
	if ( !MAT_IsSafeDataPath( argument ) )
	{
		MAT_Log( "materials: %s line %d: #%s needs a file name (relative, letters, digits, _ - /, one dot, .txt)\n", source, line, word );
		return;
	}

	if ( isTypes )
	{
		unsigned char *data;
		int length = 0;

		if ( m->files >= MAT_MAP_FILES_MAX )
		{
			MAT_Log( "materials: %s line %d: more than %d files\n", source, line, MAT_MAP_FILES_MAX );
			return;
		}
		data = m->load( argument, &length );
		if ( !data )
		{
			MAT_Log( "materials: %s line %d: %s not found\n", source, line, argument );
			return;
		}
		MAT_CopyString( m->loaded[ m->files++ ], argument, MAT_PATH_LEN );
		if ( m->onFile )
			m->onFile( argument, m->ctx );
		MAT_ParseTypes( (const char *)data, length, argument );
		m->release( data );
		return;
	}

	if ( m->depth >= MAT_INCLUDE_DEPTH )
	{
		MAT_Log( "materials: %s line %d: #include nested more than %d deep\n", source, line, MAT_INCLUDE_DEPTH );
		return;
	}
	m->depth++;
	if ( MAT_LoadListFile( m, argument ) < 0 )
		MAT_Log( "materials: %s line %d: %s not found\n", source, line, argument );
	m->depth--;
}

// a map file (texture lines, directives, inline types blocks); the map's own file is the highest layer (it wins),
// its includes come below it
static int MAT_LoadListFile( mat_mapload_t *m, const char *path )
{
	unsigned char *data;
	int length = 0, added, i;
	mat_mappass_t pass;

	for ( i = 0; i < m->files; i++ )
	{
		if ( !MAT_Compare( m->loaded[ i ], path, -1 ) )
			return 0;		// already read (a file included twice, or a loop)
	}
	if ( m->files >= MAT_MAP_FILES_MAX )
	{
		MAT_Log( "materials: more than %d files, %s not read\n", MAT_MAP_FILES_MAX, path );
		return 0;
	}

	data = m->load( path, &length );
	if ( !data )
		return -1;
	MAT_CopyString( m->loaded[ m->files++ ], path, MAT_PATH_LEN );
	if ( m->onFile )
		m->onFile( path, m->ctx );
	pass.m = m;
	pass.pass = 1;
	added = MAT_ParseList( (const char *)data, length, MAT_LAYER_MAP + MAT_INCLUDE_DEPTH - m->depth, path, 1,
		MAT_MapDirective, MAT_MapBlock, &pass );
	pass.pass = 2;
	MAT_ParseList( (const char *)data, length, 0, path, 0, MAT_MapDirective, MAT_MapBlock, &pass );
	m->release( data );
	return added;
}

int MAT_LoadMapFile( mat_load_fn load, mat_free_fn release, const char *path, mat_file_fn onFile, void *ctx )
{
	static mat_mapload_t m;		// ~600 bytes, not on the stack of the movement code
	int added;

	if ( !load || !release || !MAT_IsSafeDataPath( path ) )
		return -1;

	memset( &m, 0, sizeof( m ) );
	m.load = load;
	m.release = release;
	m.onFile = onFile;
	m.ctx = ctx;
	added = MAT_LoadListFile( &m, path );
	return added < 0 ? -1 : m.files;
}

static int MAT_ReadInt( const unsigned char *data )
{
	return (int)( (unsigned int)data[ 0 ] | ( (unsigned int)data[ 1 ] << 8 ) | ( (unsigned int)data[ 2 ] << 16 ) | ( (unsigned int)data[ 3 ] << 24 ) );
}

int MAT_ForEachBspTexture( const unsigned char *data, int length, void ( *callback )( const char *name, void *ctx ), void *ctx )
{
	const unsigned char *lump;
	int offset, size, count, i, found = 0;

	if ( !data || length < 4 + MAT_BSP_LUMPS * 8 || !callback )
		return -1;
	if ( MAT_ReadInt( data ) != MAT_BSP_VERSION )
		return -1;

	offset = MAT_ReadInt( data + 4 + MAT_BSP_TEXTURES * 8 );
	size = MAT_ReadInt( data + 4 + MAT_BSP_TEXTURES * 8 + 4 );
	if ( offset < 0 || size < 4 || offset > length || size > length - offset )
		return -1;

	lump = data + offset;
	count = MAT_ReadInt( lump );
	if ( count < 0 || count > ( size - 4 ) / 4 )
		return -1;

	for ( i = 0; i < count; i++ )
	{
		int texture = MAT_ReadInt( lump + 4 + i * 4 );
		char name[ 17 ];

		// -1: a texture the compiler did not find
		if ( texture < 0 || texture > size - 16 )
			continue;
		memcpy( name, lump + texture, 16 );
		name[ 16 ] = 0;
		if ( !name[ 0 ] )
			continue;
		callback( name, ctx );
		found++;
	}
	return found;
}
