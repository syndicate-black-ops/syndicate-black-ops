//=========================================================
// SBO 2.0: animated sprays: the frames packed in a player's tempdecal.wad. docs/ANIMATED_SPRAYS.md.
//
// The file is another player's upload, forwarded by the server: every offset and length is checked, and every frame
// is decoded once in Parse, so SprayAnim_Decode cannot fail on a file Parse accepted.
//
// The animation block starts at the first offset from the lump start that is a multiple of 4, after the palette
// (2-byte color count, 3 bytes per color), and ends inside the lump. Little-endian:
//   "SANM", u8 version (1), u8 frame count (2 .. 64), u16 0
//   u16 delay in ms, per frame
//   per frame after the first: u16 length, then the frame's codes
// A frame is coded as the changes from the frame before, in pixel order; each code byte c:
//   0x00 - 0x7F: c + 1 indices follow (copied)
//   0x80 - 0xBF: the next (c & 0x3F) + 1 pixels stay as they are
//   0xC0 - 0xFF: the next (c & 0x3F) + 1 pixels get the index in the next byte
// The codes cover the frame's pixels exactly.
//=========================================================

#include "spray_anim_core.h"

#include <string.h>

static unsigned int ReadU16( const unsigned char *p )
{
	return p[0] | ( p[1] << 8 );
}

static int ReadS32( const unsigned char *p )
{
	return (int)( (unsigned int)p[0] | ( (unsigned int)p[1] << 8 ) | ( (unsigned int)p[2] << 16 )
		| ( (unsigned int)p[3] << 24 ) );
}

static bool DecodeFrame( const unsigned char *code, int length, unsigned char *pixels, int count )
{
	int pos = 0, i = 0;

	while ( i < length )
	{
		const int c = code[i++];
		const int n = ( c < 0x80 ) ? c + 1 : ( c & 0x3F ) + 1;

		if ( n > count - pos )
			return false;

		if ( c < 0x80 )
		{
			if ( n > length - i )
				return false;
			memcpy( pixels + pos, code + i, n );
			i += n;
		}
		else if ( c >= 0xC0 )
		{
			if ( i >= length )
				return false;
			memset( pixels + pos, code[i++], n );
		}
		pos += n;
	}

	return pos == count;
}

bool SprayAnim_Parse( const unsigned char *file, int size, SprayAnimInfo &info )
{
	memset( &info, 0, sizeof( info ) );

	// the WAD3 header and the one lump, as the engine reads them (Draw_CustomCacheWadInit)
	if ( !file || size < 12 + 32 || memcmp( file, "WAD3", 4 ) != 0 || ReadS32( file + 4 ) != 1 )
		return false;

	const int infotable = ReadS32( file + 8 );
	if ( infotable < 12 || infotable > size - 32 )
		return false;

	const unsigned char *entry = file + infotable;
	const int filepos = ReadS32( entry );
	const int disksize = ReadS32( entry + 4 );
	if ( filepos < 12 || disksize < 40 || disksize > infotable - filepos )
		return false;

	// the miptex (Draw_ValidateCustomLogo): first mip right after the header, the others after it
	const unsigned char *lump = file + filepos;
	const int width = ReadS32( lump + 16 );
	const int height = ReadS32( lump + 20 );
	if ( width <= 0 || height <= 0 || width > SPRAYANIM_MAX_SIDE || height > SPRAYANIM_MAX_SIDE )
		return false;

	const int pixels = width * height;
	const int mipsize = pixels + ( pixels >> 2 ) + ( pixels >> 4 ) + ( pixels >> 6 );
	if ( ReadS32( lump + 24 ) != 40 )
		return false;

	int palcount_at = 40 + mipsize;
	if ( palcount_at + 2 > disksize )
		return false;

	const int colors = (int)ReadU16( lump + palcount_at );
	if ( colors != 256 )
		return false;	// index 255 is the transparent color: a palette of fewer colors has none

	int block = palcount_at + 2 + 3 * colors;
	block = ( block + 3 ) & ~3;
	if ( block + 8 > disksize || memcmp( lump + block, "SANM", 4 ) != 0 || lump[block + 4] != 1 )
		return false;

	const int frames = lump[block + 5];
	if ( frames < 2 || frames > SPRAYANIM_MAX_FRAMES )
		return false;

	int pos = block + 8;
	if ( frames * 2 > disksize - pos )
		return false;

	info.width = width;
	info.height = height;
	info.frames = frames;
	info.frame0 = lump + 40;
	info.palette = lump + palcount_at + 2;

	for ( int i = 0; i < frames; i++, pos += 2 )
	{
		int delay = (int)ReadU16( lump + pos );
		if ( delay < SPRAYANIM_MIN_DELAY )
			delay = SPRAYANIM_DEFAULT_DELAY;
		else if ( delay > SPRAYANIM_MAX_DELAY )
			delay = SPRAYANIM_MAX_DELAY;
		info.delay[i] = delay;
		info.totalTime += delay;
	}

	for ( int i = 1; i < frames; i++ )
	{
		if ( 2 > disksize - pos )
			return false;
		const int length = (int)ReadU16( lump + pos );
		pos += 2;
		if ( length > disksize - pos )
			return false;
		info.data[i] = lump + pos;
		info.length[i] = length;
		pos += length;
	}

	// every frame decodes to exactly the image's pixels
	static unsigned char s_Check[SPRAYANIM_MAX_SIDE * SPRAYANIM_MAX_SIDE];
	memcpy( s_Check, info.frame0, pixels );
	for ( int i = 1; i < frames; i++ )
	{
		if ( !DecodeFrame( info.data[i], info.length[i], s_Check, pixels ) )
		{
			memset( &info, 0, sizeof( info ) );
			return false;
		}
	}

	return true;
}

bool SprayAnim_Decode( const SprayAnimInfo &info, int frame, unsigned char *pixels )
{
	if ( frame < 1 || frame >= info.frames )
		return false;

	return DecodeFrame( info.data[frame], info.length[frame], pixels, info.width * info.height );
}

int SprayAnim_FrameAt( const SprayAnimInfo &info, unsigned int time )
{
	if ( info.frames < 2 || info.totalTime <= 0 )
		return 0;

	int t = (int)( time % (unsigned int)info.totalTime );
	for ( int i = 0; i < info.frames; i++ )
	{
		if ( t < info.delay[i] )
			return i;
		t -= info.delay[i];
	}

	return info.frames - 1;
}
