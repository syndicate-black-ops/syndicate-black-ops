// Offline test of src/cl_dll/waypoint_core.cpp (tests/waypoint/run_test.sh): the "Waypoint" message (good, cut short
// at every byte, hostile values, random bytes) and the screen placement (on the screen, off its sides, behind the
// eye, straight behind, the edge kept free of the margins).
#include "waypoint_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static int s_iFailed = 0;
#define CHECK( x ) do { if ( !( x ) ) { printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); s_iFailed++; } } while ( 0 )

static void PutShort( std::vector<unsigned char> &b, int v ) { b.push_back( v & 255 ); b.push_back( ( v >> 8 ) & 255 ); }
static void PutFloat( std::vector<unsigned char> &b, float f )
{
	unsigned int u;
	memcpy( &u, &f, 4 );
	for ( int i = 0; i < 4; i++ )
		b.push_back( ( u >> ( 8 * i ) ) & 255 );
}

static std::vector<unsigned char> Message( int id, int flags, float x, int sprite, int follow, const char *label )
{
	std::vector<unsigned char> b;
	PutShort( b, id );
	b.push_back( flags );
	if ( !( flags & WAYPOINT_FLAG_ON ) )
		return b;
	PutFloat( b, x ); PutFloat( b, 20.0f ); PutFloat( b, -30.5f );
	PutShort( b, sprite );
	b.push_back( 255 ); b.push_back( 128 ); b.push_back( 0 );
	PutShort( b, follow );
	PutFloat( b, 0.0f ); PutFloat( b, 0.0f ); PutFloat( b, 48.0f );
	PutShort( b, 200 ); PutShort( b, 3000 );
	b.push_back( 0 );
	for ( const char *p = label; *p; p++ )
		b.push_back( (unsigned char)*p );
	b.push_back( 0 );
	return b;
}

static bool Near( float a, float b ) { return fabsf( a - b ) < 0.01f; }

static void TestParse( void )
{
	waypoint::Marker m;
	std::vector<unsigned char> b = Message( 37, WAYPOINT_FLAG_ON | WAYPOINT_FLAG_DISTANCE | WAYPOINT_FLAG_FOLLOW,
		100.0f, 12, 5, "Bomb site" );
	CHECK( b.size() <= WAYPOINT_MSG_MAX_BYTES );
	CHECK( waypoint::Parse( b.data(), (int)b.size(), m ) );
	CHECK( m.id == 37 && ( m.flags & WAYPOINT_FLAG_ON ) && ( m.flags & WAYPOINT_FLAG_DISTANCE ) );
	CHECK( Near( m.origin[0], 100.0f ) && Near( m.origin[1], 20.0f ) && Near( m.origin[2], -30.5f ) );
	CHECK( m.sprite == 12 && m.r == 255 && m.g == 128 && m.b == 0 );
	CHECK( m.follow == 5 && Near( m.offset[2], 48.0f ) );
	CHECK( !strcmp( m.label, "Bomb site" ) );
	CHECK( m.minRange == 200 && m.maxRange == 3000 && m.owner == 0 );
	CHECK( !waypoint::InRange( m, 100 ) && waypoint::InRange( m, 200 ) && waypoint::InRange( m, 3000 )
		&& !waypoint::InRange( m, 3001 ) );
	m.minRange = m.maxRange = 0;
	CHECK( waypoint::InRange( m, 0 ) && waypoint::InRange( m, 1e9f ) );
	// an owner past the last player: refused
	{
		std::vector<unsigned char> o = Message( 37, WAYPOINT_FLAG_ON, 1.0f, 0, 0, "x" );
		o[ o.size() - 3 ] = 33;
		CHECK( !waypoint::Parse( o.data(), (int)o.size(), m ) );
		o[ o.size() - 3 ] = 7;
		CHECK( waypoint::Parse( o.data(), (int)o.size(), m ) && m.owner == 7 );
	}

	// every cut is refused (the last byte is the label's end)
	for ( size_t n = 0; n < b.size(); n++ )
		CHECK( !waypoint::Parse( b.data(), (int)n, m ) );

	// off: id and flags only; id 0 off is the clear
	b = Message( 37, 0, 0, 0, 0, "" );
	CHECK( waypoint::Parse( b.data(), (int)b.size(), m ) && m.id == 37 && !( m.flags & WAYPOINT_FLAG_ON ) );
	b = Message( 0, 0, 0, 0, 0, "" );
	CHECK( waypoint::Parse( b.data(), (int)b.size(), m ) && m.id == 0 );
	// id 0 on, ids out of range, a position that is not a number: refused
	b = Message( 0, WAYPOINT_FLAG_ON, 1.0f, 0, 0, "" );
	CHECK( !waypoint::Parse( b.data(), (int)b.size(), m ) );
	b = Message( -5, WAYPOINT_FLAG_ON, 1.0f, 0, 0, "" );
	CHECK( !waypoint::Parse( b.data(), (int)b.size(), m ) );
	b = Message( 4000, WAYPOINT_FLAG_ON, 1.0f, 0, 0, "" );
	CHECK( !waypoint::Parse( b.data(), (int)b.size(), m ) );
	b = Message( 9, WAYPOINT_FLAG_ON, NAN, 0, 0, "" );
	CHECK( !waypoint::Parse( b.data(), (int)b.size(), m ) );
	b = Message( 9, WAYPOINT_FLAG_ON, 1e9f, 0, 0, "" );
	CHECK( !waypoint::Parse( b.data(), (int)b.size(), m ) );
	// a follow index out of range, or without its flag: no follow; a negative sprite: the default
	b = Message( 9, WAYPOINT_FLAG_ON | WAYPOINT_FLAG_FOLLOW, 1.0f, -3, 3000, "" );
	CHECK( waypoint::Parse( b.data(), (int)b.size(), m ) && m.follow == 0 && !( m.flags & WAYPOINT_FLAG_FOLLOW ) );
	CHECK( m.sprite == 0 );
	b = Message( 9, WAYPOINT_FLAG_ON, 1.0f, 0, 7, "" );
	CHECK( waypoint::Parse( b.data(), (int)b.size(), m ) && m.follow == 0 );
	// a label longer than the client keeps: cut, the message still read to its end
	std::string sLong( 300, 'x' );
	b = Message( 9, WAYPOINT_FLAG_ON, 1.0f, 0, 0, sLong.c_str() );
	CHECK( waypoint::Parse( b.data(), (int)b.size(), m ) && strlen( m.label ) == WAYPOINT_MAX_LABEL - 1 );
	// no terminator: refused
	b = Message( 9, WAYPOINT_FLAG_ON, 1.0f, 0, 0, "abc" );
	b.pop_back();
	CHECK( !waypoint::Parse( b.data(), (int)b.size(), m ) );
	CHECK( !waypoint::Parse( NULL, 10, m ) );

	// random bytes: no crash, anything read is in range
	srand( 1234 );
	for ( int i = 0; i < 20000; i++ )
	{
		unsigned char buf[ 128 ];
		const int n = rand() % (int)sizeof( buf );
		for ( int k = 0; k < n; k++ )
			buf[k] = rand() & 255;
		if ( waypoint::Parse( buf, n, m ) && ( m.flags & WAYPOINT_FLAG_ON ) )
		{
			CHECK( m.id > 0 && m.id < WAYPOINT_MAX_INDEX );
			CHECK( m.follow >= 0 && m.follow < WAYPOINT_MAX_INDEX );
			CHECK( strlen( m.label ) < WAYPOINT_MAX_LABEL );
		}
	}
}

static void TestLocate( void )
{
	const float W = 1280, H = 720, M = 40;
	// on the screen: its projection, no arrow
	waypoint::Place p = waypoint::Locate( 0.5f, 0.5f, true, 0, 0, W, H, M, M, M );
	CHECK( !p.edge && Near( p.x, 960 ) && Near( p.y, 180 ) );
	p = waypoint::Locate( 0.0f, 0.0f, true, 0, 0, W, H, M, M, M );
	CHECK( !p.edge && Near( p.x, 640 ) && Near( p.y, 360 ) );
	// off the right side, level: right edge, kept free of the margin, arrow to the right
	p = waypoint::Locate( 3.0f, 0.0f, true, 0, 0, W, H, M, M, M );
	CHECK( p.edge && Near( p.x, W - M ) && Near( p.y, 360 ) && Near( p.angle, 0.0f ) );
	// off the top: top edge, arrow up (-pi/2)
	p = waypoint::Locate( 0.0f, 5.0f, true, 0, 0, W, H, M, M, M );
	CHECK( p.edge && Near( p.x, 640 ) && Near( p.y, M ) && Near( p.angle, (float)-M_PI / 2 ) );
	// off the bottom left corner: on the ray from the centre, inside the free rectangle
	p = waypoint::Locate( -4.0f, -4.0f, true, 0, 0, W, H, M, M, M );
	CHECK( p.edge && p.x >= M - 0.01f && p.y <= H - M + 0.01f && ( Near( p.x, M ) || Near( p.y, H - M ) ) );
	CHECK( p.angle > (float)M_PI / 2 && p.angle < (float)M_PI );
	// in the margin band but on the screen: on the edge (the icon and its arrow stay whole)
	p = waypoint::Locate( 0.99f, 0.0f, true, 0, 0, W, H, M, M, M );
	CHECK( p.edge && Near( p.x, W - M ) );
	// behind the eye, to the left: the left edge, arrow left; behind and above: the top edge
	p = waypoint::Locate( 0, 0, false, -100.0f, 0.0f, W, H, M, M, M );
	CHECK( p.edge && Near( p.x, M ) && Near( p.y, 360 ) && Near( fabsf( p.angle ), (float)M_PI ) );
	p = waypoint::Locate( 0, 0, false, 0.0f, 50.0f, W, H, M, M, M );
	CHECK( p.edge && Near( p.y, M ) && Near( p.x, 640 ) );
	// straight behind: the bottom edge, arrow down
	p = waypoint::Locate( 0, 0, false, 0.0f, 0.0f, W, H, M, M, M );
	CHECK( p.edge && Near( p.y, H - M ) && Near( p.angle, (float)M_PI / 2 ) );
	// a larger bottom margin (the HUD's row): straight behind stops above it, the top and the sides keep theirs
	p = waypoint::Locate( 0, 0, false, 0.0f, 0.0f, W, H, M, M, 120 );
	CHECK( p.edge && Near( p.y, H - 120 ) );
	p = waypoint::Locate( 0.0f, 0.85f, true, 0, 0, W, H, M, M, 120 );
	CHECK( !p.edge && Near( p.y, 54 ) );
	p = waypoint::Locate( 0.0f, -0.8f, true, 0, 0, W, H, M, M, 120 );
	CHECK( p.edge && Near( p.y, H - 120 ) );
	// a projection that is not a number (the spot in the eye's plane) and a margin larger than the screen: no NaN
	p = waypoint::Locate( INFINITY, NAN, true, 1.0f, 0.0f, W, H, M, M, M );
	CHECK( p.edge && Near( p.x, W - M ) );
	p = waypoint::Locate( 9.0f, 9.0f, true, 0, 0, 20, 20, M, M, M );
	CHECK( p.edge && Near( p.x, 10 ) && Near( p.y, 10 ) );
}

static void TestSpread( void )
{
	const float W = 1280, H = 720, M = 40;
	waypoint::Place p[3];
	// two on the right edge at the same height: the second goes below the first; one on the screen stays
	p[0] = waypoint::Locate( 3.0f, 0.0f, true, 0, 0, W, H, M, M, M );
	p[1] = waypoint::Locate( 3.0f, 0.01f, true, 0, 0, W, H, M, M, M );
	p[2] = waypoint::Locate( 0.0f, 0.0f, true, 0, 0, W, H, M, M, M );
	waypoint::Spread( p, 3, 60, 70, W, H, M, M, M );
	CHECK( Near( p[1].x, W - M ) && p[1].y >= p[0].y + 69.9f );
	CHECK( Near( p[2].x, 640 ) && Near( p[2].y, 360 ) );
	// on the bottom edge: moved right; never past the margins
	p[0] = waypoint::Locate( 0, 0, false, 0, 0, W, H, M, M, M );
	p[1] = waypoint::Locate( 0, 0, false, 0, 0, W, H, M, M, M );
	waypoint::Spread( p, 2, 60, 70, W, H, M, M, M );
	CHECK( Near( p[1].y, H - M ) && p[1].x >= p[0].x + 59.9f );
	p[0] = waypoint::Locate( 3.0f, -0.99f, true, 0, 0, W, H, M, M, M );
	p[1] = p[0];
	waypoint::Spread( p, 2, 60, 70, W, H, M, M, M );
	CHECK( p[1].y <= H - M + 0.01f && p[1].x <= W - M + 0.01f );
	waypoint::Spread( NULL, 5, 1, 1, W, H, M, M, M );
}

static void TestDistance( void )
{
	char sz[ 16 ];
	waypoint::FormatDistance( 0.0f, sz, sizeof( sz ) );
	CHECK( !strcmp( sz, "0 m" ) );
	waypoint::FormatDistance( 39.37f * 12.5f, sz, sizeof( sz ) );
	CHECK( !strcmp( sz, "12 m" ) );
	waypoint::FormatDistance( 1e12f, sz, sizeof( sz ) );
	CHECK( !strcmp( sz, "99999 m" ) );
	waypoint::FormatDistance( NAN, sz, sizeof( sz ) );
	CHECK( !strcmp( sz, "0 m" ) );
	waypoint::FormatDistance( 100.0f, sz, 3 );
	CHECK( strlen( sz ) == 2 );
}

int main( void )
{
	TestParse();
	TestLocate();
	TestSpread();
	TestDistance();
	if ( s_iFailed )
	{
		printf( "waypoint_test: %d FAILED\n", s_iFailed );
		return 1;
	}
	printf( "waypoint_test: all passed\n" );
	return 0;
}
