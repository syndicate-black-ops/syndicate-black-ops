// env_overlay client core (src/cl_dll/overlay_core.cpp): message parsing on good, cut and hostile input, the fades,
// the placement and the sprite frames. Run by tests/overlay/run_test.sh with ASan/UBSan.

#include "overlay_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static int s_iFailed = 0;

#define CHECK( cond ) do { if ( !( cond ) ) { printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond ); s_iFailed++; } } while ( 0 )
#define NEAR( a, b ) CHECK( fabs( (double)( a ) - (double)( b ) ) < 0.001 )

struct Writer
{
	std::vector<unsigned char> b;
	void Byte( int v ) { b.push_back( (unsigned char)v ); }
	void Short( int v ) { b.push_back( (unsigned char)( v & 0xFF ) ); b.push_back( (unsigned char)( ( v >> 8 ) & 0xFF ) ); }
	void String( const char *s ) { while ( *s ) b.push_back( (unsigned char)*s++ ); b.push_back( 0 ); }
};

// as the server (dlls/env_overlay.cpp CEnvOverlay::Send) writes it
static std::vector<unsigned char> OnMessage( int channel, int flags, const char *image, int x, int y, int scale, int r,
	int g, int b, int a, int fps, int fadein, int hold, int fadeout, int elapsed )
{
	Writer w;
	w.Byte( channel );
	w.Byte( flags | OVERLAY_FLAG_ON );
	w.String( image );
	w.Short( x );
	w.Short( y );
	w.Short( scale );
	w.Byte( r );
	w.Byte( g );
	w.Byte( b );
	w.Byte( a );
	w.Short( fps );
	w.Short( fadein );
	w.Short( hold );
	w.Short( fadeout );
	w.Short( elapsed );
	return w.b;
}

static void TestParse()
{
	overlay::Params p;

	std::vector<unsigned char> m = OnMessage( 3, OVERLAY_FLAG_LOOP | OVERLAY_FLAG_ADDITIVE, "sprites/hint.spr", 2500,
		-10000, 150, 10, 20, 30, 200, 1000, 50, 200, 100, 15 );
	CHECK( overlay::Parse( m.data(), (int)m.size(), p ) );
	CHECK( p.channel == 3 );
	CHECK( p.flags == ( OVERLAY_FLAG_ON | OVERLAY_FLAG_LOOP | OVERLAY_FLAG_ADDITIVE ) );
	CHECK( !strcmp( p.image, "sprites/hint.spr" ) );
	NEAR( p.x, 0.25 );
	NEAR( p.y, -1.0 );
	NEAR( p.scale, 1.5 );
	CHECK( p.r == 10 && p.g == 20 && p.b == 30 && p.a == 200 );
	NEAR( p.framerate, 10.0 );
	NEAR( p.fadein, 0.5 );
	NEAR( p.hold, 2.0 );
	NEAR( p.fadeout, 1.0 );
	NEAR( p.elapsed, 1.5 );

	// unsigned times above 327.67 s
	m = OnMessage( 1, OVERLAY_FLAG_STAY, "gfx/a.tga", 0, 0, 100, 0, 0, 0, 255, 0, 60000, 0, 0, 60000 );
	CHECK( overlay::Parse( m.data(), (int)m.size(), p ) );
	NEAR( p.fadein, 600.0 );
	NEAR( p.elapsed, 6000.0 );

	// a plain color needs full screen
	m = OnMessage( 1, OVERLAY_FLAG_FULLSCREEN, "", 0, 0, 100, 255, 0, 0, 128, 0, 0, 0, 0, 0 );
	CHECK( overlay::Parse( m.data(), (int)m.size(), p ) );
	CHECK( p.image[0] == 0 );
	m = OnMessage( 1, 0, "", 0, 0, 100, 255, 0, 0, 128, 0, 0, 0, 0, 0 );
	CHECK( !overlay::Parse( m.data(), (int)m.size(), p ) );

	// channels: 0 only for off, 9 never
	m = OnMessage( 0, 0, "sprites/a.spr", 0, 0, 100, 0, 0, 0, 255, 0, 0, 0, 0, 0 );
	CHECK( !overlay::Parse( m.data(), (int)m.size(), p ) );
	m = OnMessage( 9, 0, "sprites/a.spr", 0, 0, 100, 0, 0, 0, 255, 0, 0, 0, 0, 0 );
	CHECK( !overlay::Parse( m.data(), (int)m.size(), p ) );

	// off messages
	unsigned char off[2] = { 4, OVERLAY_FLAG_NOW };
	CHECK( overlay::Parse( off, 2, p ) && p.channel == 4 && p.flags == OVERLAY_FLAG_NOW );
	unsigned char offAll[2] = { 0, 0 };
	CHECK( overlay::Parse( offAll, 2, p ) && p.channel == 0 );
	CHECK( !overlay::Parse( off, 1, p ) );
	CHECK( !overlay::Parse( NULL, 0, p ) );

	// scale 0 or negative: 1
	m = OnMessage( 1, 0, "sprites/a.spr", 0, 0, -5, 0, 0, 0, 255, 0, 0, 0, 0, 0 );
	CHECK( overlay::Parse( m.data(), (int)m.size(), p ) );
	NEAR( p.scale, 1.0 );

	// a path longer than the buffer is cut, the rest of the message still read
	std::string longPath( 300, 'a' );
	m = OnMessage( 2, 0, longPath.c_str(), 0, 0, 100, 1, 2, 3, 4, 0, 0, 0, 0, 0 );
	CHECK( overlay::Parse( m.data(), (int)m.size(), p ) );
	CHECK( strlen( p.image ) == OVERLAY_MAX_PATH - 1 );
	CHECK( p.r == 1 && p.a == 4 );

	// every cut of a good message: refused, no read past the end (ASan)
	m = OnMessage( 5, OVERLAY_FLAG_FULLSCREEN, "sprites/hint.spr", 100, 200, 100, 1, 2, 3, 4, 5, 6, 7, 8, 9 );
	for ( size_t n = 0; n < m.size(); n++ )
	{
		std::vector<unsigned char> cut( m.begin(), m.begin() + n );
		CHECK( !overlay::Parse( cut.empty() ? NULL : cut.data(), (int)n, p ) || n == 2 );
	}

	// random bytes: never crashes, results stay in range
	srand( 1 );
	for ( int i = 0; i < 20000; i++ )
	{
		const int n = rand() % 120;
		std::vector<unsigned char> r( n );
		for ( int j = 0; j < n; j++ )
			r[j] = (unsigned char)rand();
		if ( n > 1 && ( i & 1 ) )
			r[1] |= OVERLAY_FLAG_ON;
		if ( overlay::Parse( r.empty() ? NULL : r.data(), n, p ) && ( p.flags & OVERLAY_FLAG_ON ) )
		{
			CHECK( p.channel >= 1 && p.channel <= OVERLAY_CHANNELS );
			CHECK( p.x >= -1.0f && p.x <= 1.0f && p.y >= -1.0f && p.y <= 1.0f );
			CHECK( p.scale > 0.0f );
			CHECK( strlen( p.image ) < OVERLAY_MAX_PATH );
			// the math on hostile values stays finite
			const float a = overlay::Alpha( p, 3.0f );
			CHECK( a == a );
			const int f = overlay::Frame( p, 1e6f, 7 );
			CHECK( f >= 0 && f < 7 );
		}
	}
}

static void TestAlpha()
{
	overlay::Params p;
	memset( &p, 0, sizeof( p ) );
	p.fadein = 1.0f;
	p.hold = 2.0f;
	p.fadeout = 1.0f;

	// timed
	NEAR( overlay::Alpha( p, 0.0f ), 0.0 );
	NEAR( overlay::Alpha( p, 0.5f ), 0.5 );
	NEAR( overlay::Alpha( p, 2.0f ), 1.0 );
	NEAR( overlay::Alpha( p, 3.5f ), 0.5 );
	CHECK( overlay::Alpha( p, 4.01f ) < 0.0f );

	// stays after the fade in
	p.flags = OVERLAY_FLAG_STAY;
	NEAR( overlay::Alpha( p, 0.25f ), 0.25 );
	NEAR( overlay::Alpha( p, 100.0f ), 1.0 );

	// loop: the cycle again and again, never over
	p.flags = OVERLAY_FLAG_LOOP;
	NEAR( overlay::Alpha( p, 4.5f ), 0.5 );
	NEAR( overlay::Alpha( p, 4.0f * 10 + 2.0f ), 1.0 );
	CHECK( overlay::Alpha( p, 1e5f ) >= 0.0f );
	overlay::Params zero;
	memset( &zero, 0, sizeof( zero ) );
	zero.flags = OVERLAY_FLAG_LOOP;
	NEAR( overlay::Alpha( zero, 5.0f ), 1.0 );

	// no times at all, timed: over at once
	zero.flags = 0;
	CHECK( overlay::Alpha( zero, 0.0f ) < 0.0f );
	zero.flags = OVERLAY_FLAG_STAY;
	NEAR( overlay::Alpha( zero, 0.0f ), 1.0 );

	// turned off: from where it was over the fade-out time
	NEAR( overlay::AlphaOff( p, 0.8f, 0.5f ), 0.4 );
	CHECK( overlay::AlphaOff( p, 0.8f, 1.0f ) < 0.0f );
	CHECK( overlay::AlphaOff( zero, 1.0f, 0.0f ) < 0.0f );
	CHECK( overlay::Alpha( p, NAN ) < 0.0f );
}

static void TestPlace()
{
	CHECK( overlay::Place( -1.0f, 100, 1000 ) == 450 );		// centered
	CHECK( overlay::Place( 0.0f, 100, 1000 ) == 0 );
	CHECK( overlay::Place( 0.5f, 100, 1000 ) == 500 );
	CHECK( overlay::Place( 0.95f, 100, 1000 ) == 900 );		// kept on the screen
	CHECK( overlay::Place( -0.1f, 100, 1000 ) == 800 );		// right edge 10 % from the right
	CHECK( overlay::Place( -1.0f, 2000, 1000 ) == -500 );	// larger than the screen: centered, not clamped
}

static void TestFrame()
{
	overlay::Params p;
	memset( &p, 0, sizeof( p ) );
	CHECK( overlay::Frame( p, 5.0f, 10 ) == 0 );	// no frame rate: the first frame
	p.framerate = 10.0f;
	CHECK( overlay::Frame( p, 0.0f, 10 ) == 0 );
	CHECK( overlay::Frame( p, 0.55f, 10 ) == 5 );
	CHECK( overlay::Frame( p, 1.25f, 10 ) == 2 );	// loops
	CHECK( overlay::Frame( p, 1.25f, 1 ) == 0 );
}

int main()
{
	TestParse();
	TestAlpha();
	TestPlace();
	TestFrame();
	if ( s_iFailed )
	{
		printf( "overlay_test: %d failed\n", s_iFailed );
		return 1;
	}
	printf( "overlay_test: all passed\n" );
	return 0;
}
