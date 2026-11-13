//=========================================================
// SBO 2.0 color_correction / color_correction_volume (docs/COLOR_CORRECTION.md): the "ColorCorr" user message, server
// dlls/colorcorrection.cpp, client cl_dll/colorcorrection.cpp. One reliable message per entity: at HUD init (every
// entity, the first one with COLORCORR_FLAG_FIRST) and when an entity changes (turned on or off, its master, a key
// changed by trigger_changevalue). The client works out the weights from its own view and grades the 3D view.
//
//   SHORT  entity index (1 .. 2047)
//   BYTE   COLORCORR_FLAG_* flags
//   with COLORCORR_FLAG_REMOVED: nothing more (the client forgets the entity)
//   SHORT  maximum weight * COLORCORR_WEIGHT_UNIT (0 .. 1)
//   SHORT  weight now * COLORCORR_WEIGHT_UNIT (point entity: where its fade is; volume: unused, 0)
//   SHORT  fade in time * COLORCORR_TIME_UNIT (unsigned; volume: its fade time)
//   SHORT  fade out time * COLORCORR_TIME_UNIT (unsigned; volume: its fade time)
//   point entity:  SHORT x, SHORT y, SHORT z (whole units), SHORT min falloff, SHORT max falloff (whole units, -1 =
//                  no falloff)
//   volume:        SHORT brush model index, SHORT x, SHORT y, SHORT z (the entity's origin, whole units)
//   SHORT  saturation * COLORCORR_KEY_UNIT (signed)
//   SHORT  contrast * COLORCORR_KEY_UNIT (signed)
//   SHORT  brightness * COLORCORR_KEY_UNIT (signed)
//   SHORT  gamma * COLORCORR_KEY_UNIT (signed)
//   BYTE   tint red, BYTE tint green, BYTE tint blue
//   BYTE   tint amount * 255
//   STRING lookup table file ("" = none): gfx/colorcorrection/<name>.raw or .cube
//=========================================================

#ifndef COLORCORRECTION_SHARED_H
#define COLORCORRECTION_SHARED_H

#define COLORCORR_MSG_NAME		"ColorCorr"

#define COLORCORR_FLAG_ENABLED	1		// on (and its master, if any, lets it)
#define COLORCORR_FLAG_VOLUME	2		// a color_correction_volume (else a point color_correction)
#define COLORCORR_FLAG_EXCLUSIVE	4		// while it has weight, it takes that share from the others
#define COLORCORR_FLAG_FIRST	8		// the first of the full list sent at HUD init: the client forgets the others
#define COLORCORR_FLAG_REMOVED	16		// the entity is gone (killtarget)

#define COLORCORR_WEIGHT_UNIT	10000.0f	// weights 0 .. 1
#define COLORCORR_TIME_UNIT		100.0f		// fade times 0 .. 655.35 s
#define COLORCORR_KEY_UNIT		1000.0f		// saturation, contrast, brightness, gamma: -32.768 .. 32.767

#define COLORCORR_MAX_PATH		64		// file path with its terminator
#define COLORCORR_FOLDER		"gfx/colorcorrection/"

#define COLORCORR_MAX_ENTITIES	64		// entities a client keeps

#include <string.h>

// A lookup table path the server precaches and the client reads: gfx/colorcorrection/<name>.raw or .cube (any case),
// shorter than COLORCORR_MAX_PATH, no "..", ':' or '\\'.
inline bool ColorCorr_ValidPath( const char *path )
{
	if ( !path )
		return false;
	const size_t len = strlen( path );
	const size_t folder = sizeof( COLORCORR_FOLDER ) - 1;
	if ( len >= COLORCORR_MAX_PATH || len < folder + 5 || strncmp( path, COLORCORR_FOLDER, folder ) != 0 )
		return false;
	if ( strstr( path, ".." ) || strchr( path, ':' ) || strchr( path, '\\' ) )
		return false;
	auto EndsWith = [&]( const char *ext ) {
		const size_t n = strlen( ext );
		if ( len < folder + 1 + n )
			return false;
		for ( size_t i = 0; i < n; i++ )
		{
			char c = path[len - n + i];
			if ( c >= 'A' && c <= 'Z' )
				c = (char)( c - 'A' + 'a' );
			if ( c != ext[i] )
				return false;
		}
		return true;
	};
	return EndsWith( ".raw" ) || EndsWith( ".cube" );
}

#endif // COLORCORRECTION_SHARED_H
