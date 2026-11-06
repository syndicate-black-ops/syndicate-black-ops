// Animated spray reader test (tests/spray_anim/run_test.sh): src/cl_dll/spray_anim_core.cpp alone, with
// AddressSanitizer and UndefinedBehaviorSanitizer. Reads the converter's sprays (tests/spray_anim/make_case.py): the
// engine's checks of a tempdecal.wad pass, every frame decodes to what the converter made, a still spray is not
// animated. Then hostile input (another player's upload): every truncation, byte changes, random files, random codes.
// Exit 1 on a failed check (a sanitizer error aborts).
//   spray_anim_test <dir with test.wad, test.frames, still.wad, big.wad>

#include "spray_anim_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static int s_iFailures = 0;
static int s_iChecks = 0;

#define CHECK( cond ) do { s_iChecks++; if ( !( cond ) ) { s_iFailures++; \
	fprintf( stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond ); } } while ( 0 )

static unsigned s_uSeed = 12345;
static unsigned Rand( void )
{
	s_uSeed = s_uSeed * 1103515245u + 12345u;
	return ( s_uSeed >> 8 ) & 0xFFFFFF;
}

static std::vector<unsigned char> ReadFile( const std::string &path )
{
	FILE *f = fopen( path.c_str(), "rb" );
	if ( !f )
	{
		fprintf( stderr, "cannot read %s\n", path.c_str() );
		exit( 2 );
	}
	std::vector<unsigned char> v;
	unsigned char buf[4096];
	size_t n;
	while ( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 )
		v.insert( v.end(), buf, buf + n );
	fclose( f );
	return v;
}

static int S32( const unsigned char *p )
{
	return (int)( p[0] | ( p[1] << 8 ) | ( p[2] << 16 ) | ( (unsigned)p[3] << 24 ) );
}

// What the engine checks before it draws a spray (HL25 hw.dll / HLDS: COM_CreateCustomization's size range,
// Draw_CustomCacheWadInit, Draw_ValidateCustomLogo)
static bool EngineAccepts( const std::vector<unsigned char> &f )
{
	const int size = (int)f.size();
	if ( size < 1024 || size > 20480 || memcmp( &f[0], "WAD3", 4 ) || S32( &f[4] ) != 1 )
		return false;
	const int infotable = S32( &f[8] );
	if ( infotable < 1 || infotable + 32 != size )
		return false;
	const unsigned char *e = &f[infotable];
	const int filepos = S32( e ), disksize = S32( e + 4 ), lsize = S32( e + 8 );
	if ( lsize != disksize || lsize < 1 || filepos < 12 || lsize + filepos > infotable )
		return false;
	const unsigned char *mip = &f[filepos];
	const int w = S32( mip + 16 ), h = S32( mip + 20 );
	if ( w <= 0 || h <= 0 || w > 256 || h > 256 )
		return false;
	int pix = w * h;
	const int msize = pix + ( pix >> 2 ) + ( pix >> 4 ) + ( pix >> 6 );
	for ( int i = 0; i < 3; i++, pix >>= 2 )
		if ( S32( mip + 24 + 4 * i ) + pix != S32( mip + 28 + 4 * i ) )
			return false;
	const int colors = mip[40 + msize] | ( mip[41 + msize] << 8 );
	if ( colors > 256 || colors + 2 * ( colors + 1 ) + msize + S32( mip + 24 ) > disksize )
		return false;
	return true;
}

static void TestConverted( const std::string &dir )
{
	const std::vector<unsigned char> wad = ReadFile( dir + "/test.wad" );
	const std::vector<unsigned char> expected = ReadFile( dir + "/test.frames" );

	CHECK( EngineAccepts( wad ) );

	SprayAnimInfo info;
	CHECK( SprayAnim_Parse( &wad[0], (int)wad.size(), info ) );
	CHECK( info.width == 64 && info.height == 64 );
	const int pixels = info.width * info.height;
	CHECK( info.frames >= 2 && (size_t)info.frames * pixels == expected.size() );
	if ( (size_t)info.frames * pixels != expected.size() )
		return;

	std::vector<unsigned char> frame( info.frame0, info.frame0 + pixels );
	CHECK( !memcmp( &frame[0], &expected[0], pixels ) );
	for ( int k = 1; k < info.frames; k++ )
	{
		CHECK( SprayAnim_Decode( info, k, &frame[0] ) );
		CHECK( !memcmp( &frame[0], &expected[k * pixels], pixels ) );
	}
	CHECK( !SprayAnim_Decode( info, 0, &frame[0] ) && !SprayAnim_Decode( info, info.frames, &frame[0] ) );

	// timing: the GIF's delays (5 + k centiseconds), looping
	CHECK( info.delay[0] == 50 && info.delay[1] == 60 );
	CHECK( SprayAnim_FrameAt( info, 0 ) == 0 && SprayAnim_FrameAt( info, 49 ) == 0 );
	CHECK( SprayAnim_FrameAt( info, 50 ) == 1 && SprayAnim_FrameAt( info, 109 ) == 1 );
	CHECK( SprayAnim_FrameAt( info, info.totalTime - 1 ) == info.frames - 1 );
	CHECK( SprayAnim_FrameAt( info, info.totalTime ) == 0 );
	CHECK( SprayAnim_FrameAt( info, 0xFFFFFFFFu ) >= 0 );

	// a still spray made by the converter is a normal spray
	const std::vector<unsigned char> still = ReadFile( dir + "/still.wad" );
	CHECK( EngineAccepts( still ) );
	CHECK( !SprayAnim_Parse( &still[0], (int)still.size(), info ) );

	// frames left out to fit: still within the engine's limit and readable
	const std::vector<unsigned char> big = ReadFile( dir + "/big.wad" );
	CHECK( EngineAccepts( big ) );
	CHECK( SprayAnim_Parse( &big[0], (int)big.size(), info ) && info.frames >= 2 );
}

// Parse either refuses the file or every frame of it decodes
static void ParseHostile( const unsigned char *data, int size )
{
	SprayAnimInfo info;
	if ( !SprayAnim_Parse( data, size, info ) )
		return;
	std::vector<unsigned char> frame( info.frame0, info.frame0 + info.width * info.height );
	for ( int k = 1; k < info.frames; k++ )
		CHECK( SprayAnim_Decode( info, k, &frame[0] ) );
	CHECK( SprayAnim_FrameAt( info, Rand() ) < info.frames );
}

static void TestHostile( const std::string &dir )
{
	const std::vector<unsigned char> wad = ReadFile( dir + "/test.wad" );

	// every truncation, in its own buffer so the sanitizer sees reads past the end
	for ( size_t n = 0; n < wad.size(); n++ )
	{
		std::vector<unsigned char> cut( wad.begin(), wad.begin() + n );
		ParseHostile( n ? &cut[0] : NULL, (int)n );
	}

	// changed bytes: anywhere, and in the animation block
	SprayAnimInfo info;
	SprayAnim_Parse( &wad[0], (int)wad.size(), info );
	const int blockStart = (int)( info.palette - &wad[0] ) + 768;
	for ( int round = 0; round < 20000; round++ )
	{
		std::vector<unsigned char> f = wad;
		const int changes = 1 + Rand() % 8;
		for ( int i = 0; i < changes; i++ )
		{
			const int at = ( round & 1 ) ? blockStart + (int)( Rand() % ( f.size() - 32 - blockStart ) )
				: (int)( Rand() % f.size() );
			f[at] = (unsigned char)Rand();
		}
		ParseHostile( &f[0], (int)f.size() );
	}

	// a lump that claims to be larger than the file, and lengths at their limits
	{
		std::vector<unsigned char> f = wad;
		const int entry = S32( &f[8] );
		f[entry + 4] = f[entry + 8] = 0xFF;
		f[entry + 5] = f[entry + 9] = 0xFF;
		ParseHostile( &f[0], (int)f.size() );
	}

	// random files with a valid header and random codes after the palette
	for ( int round = 0; round < 3000; round++ )
	{
		std::vector<unsigned char> f = wad;
		for ( size_t i = blockStart + 8; i < f.size() - 32; i++ )
			f[i] = (unsigned char)Rand();
		ParseHostile( &f[0], (int)f.size() );

		std::vector<unsigned char> r( 1 + Rand() % 21000 );
		for ( size_t i = 0; i < r.size(); i++ )
			r[i] = (unsigned char)Rand();
		if ( r.size() > 12 )
			memcpy( &r[0], "WAD3\1\0\0\0", 8 );
		ParseHostile( &r[0], (int)r.size() );
	}

	// a frame count of 1, 0 and above the limit
	for ( int frames : { 0, 1, SPRAYANIM_MAX_FRAMES + 1, 255 } )
	{
		std::vector<unsigned char> f = wad;
		f[blockStart + 5] = (unsigned char)frames;
		CHECK( !SprayAnim_Parse( &f[0], (int)f.size(), info ) );
	}
}

int main( int argc, char **argv )
{
	if ( argc < 2 )
	{
		fprintf( stderr, "usage: %s <case dir>\n", argv[0] );
		return 2;
	}

	TestConverted( argv[1] );
	TestHostile( argv[1] );

	printf( "spray_anim: %d checks, %d failed\n", s_iChecks, s_iFailures );
	return s_iFailures ? 1 : 0;
}
