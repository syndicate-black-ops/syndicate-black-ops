// Color correction client core (src/cl_dll/colorcorrection_core.cpp): message parsing, the .raw and .cube readers on
// good, cut and hostile input, the simple keys, the weights, the fades and the blend, and a seeded random fuzz pass over
// the readers and the message. Run by tests/colorcorrection/run_test.sh with ASan/UBSan. Argument: fuzz iterations
// (default 20000).

#include "colorcorrection_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static int s_iFailed = 0;

#define CHECK( cond ) do { if ( !( cond ) ) { printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond ); s_iFailed++; } } while ( 0 )
#define NEAR( a, b ) CHECK( fabs( (double)( a ) - (double)( b ) ) < 0.002 )

typedef std::vector<unsigned char> Bytes;

struct Writer
{
	Bytes b;
	void Byte( int v ) { b.push_back( (unsigned char)v ); }
	void Short( int v ) { b.push_back( (unsigned char)( v & 0xFF ) ); b.push_back( (unsigned char)( ( v >> 8 ) & 0xFF ) ); }
	void String( const char *s ) { while ( *s ) b.push_back( (unsigned char)*s++ ); b.push_back( 0 ); }
};

// as the server (dlls/colorcorrection.cpp CColorCorrectionBase::SendTo) writes a point entity
static Bytes PointMessage( int index, int flags, float maxW, float wNow, float fin, float fout, int x, int y, int z,
	int minF, int maxF, float sat, float con, float bri, float gam, int r, int g, int b, int amount, const char *file )
{
	Writer w;
	w.Short( index );
	w.Byte( flags );
	w.Short( (int)( maxW * COLORCORR_WEIGHT_UNIT + 0.5f ) );
	w.Short( (int)( wNow * COLORCORR_WEIGHT_UNIT + 0.5f ) );
	w.Short( (int)( fin * COLORCORR_TIME_UNIT + 0.5f ) );
	w.Short( (int)( fout * COLORCORR_TIME_UNIT + 0.5f ) );
	w.Short( x );
	w.Short( y );
	w.Short( z );
	w.Short( minF );
	w.Short( maxF );
	w.Short( (int)lroundf( sat * COLORCORR_KEY_UNIT ) );
	w.Short( (int)lroundf( con * COLORCORR_KEY_UNIT ) );
	w.Short( (int)lroundf( bri * COLORCORR_KEY_UNIT ) );
	w.Short( (int)lroundf( gam * COLORCORR_KEY_UNIT ) );
	w.Byte( r );
	w.Byte( g );
	w.Byte( b );
	w.Byte( amount );
	w.String( file );
	return w.b;
}

static Bytes VolumeMessage( int index, int flags, int model, const char *file )
{
	Writer w;
	w.Short( index );
	w.Byte( flags | COLORCORR_FLAG_VOLUME );
	w.Short( 5000 );
	w.Short( 0 );
	w.Short( 150 );
	w.Short( 150 );
	w.Short( model );
	w.Short( -10 );
	w.Short( 20 );
	w.Short( 30 );
	w.Short( 1000 );
	w.Short( 1000 );
	w.Short( 0 );
	w.Short( 1000 );
	w.Byte( 255 );
	w.Byte( 255 );
	w.Byte( 255 );
	w.Byte( 0 );
	w.String( file );
	return w.b;
}

static void TestMessage()
{
	colorcorr::Params p;
	Bytes m = PointMessage( 42, COLORCORR_FLAG_ENABLED | COLORCORR_FLAG_EXCLUSIVE, 0.8f, 0.25f, 1.5f, 2.0f, -100, 200,
		-300, 64, 512, 0.0f, 1.3f, -0.1f, 1.2f, 255, 200, 140, 128, "gfx/colorcorrection/sepia.raw" );
	CHECK( colorcorr::Parse( m.data(), (int)m.size(), p ) );
	CHECK( p.index == 42 );
	CHECK( p.flags == ( COLORCORR_FLAG_ENABLED | COLORCORR_FLAG_EXCLUSIVE ) );
	NEAR( p.maxWeight, 0.8f );
	NEAR( p.weightNow, 0.25f );
	NEAR( p.fadeIn, 1.5f );
	NEAR( p.fadeOut, 2.0f );
	NEAR( p.origin[0], -100 );
	NEAR( p.origin[1], 200 );
	NEAR( p.origin[2], -300 );
	NEAR( p.minFalloff, 64 );
	NEAR( p.maxFalloff, 512 );
	NEAR( p.saturation, 0.0f );
	NEAR( p.contrast, 1.3f );
	NEAR( p.brightness, -0.1f );
	NEAR( p.gamma, 1.2f );
	CHECK( p.tint[0] == 255 && p.tint[1] == 200 && p.tint[2] == 140 );
	NEAR( p.tintAmount, 128 / 255.0f );
	CHECK( !strcmp( p.file, "gfx/colorcorrection/sepia.raw" ) );

	// a weight now above the maximum is held to it
	m = PointMessage( 1, 1, 0.5f, 0.9f, 0, 0, 0, 0, 0, -1, -1, 1, 1, 0, 1, 255, 255, 255, 0, "" );
	CHECK( colorcorr::Parse( m.data(), (int)m.size(), p ) );
	NEAR( p.weightNow, 0.5f );

	// volume
	m = VolumeMessage( 7, COLORCORR_FLAG_ENABLED, 12, "gfx/colorcorrection/teal.cube" );
	CHECK( colorcorr::Parse( m.data(), (int)m.size(), p ) );
	CHECK( p.model == 12 && ( p.flags & COLORCORR_FLAG_VOLUME ) );
	NEAR( p.origin[0], -10 );
	NEAR( p.fadeIn, 1.5f );
	CHECK( p.minFalloff == -1.0f && p.maxFalloff == -1.0f );
	CHECK( !strcmp( p.file, "gfx/colorcorrection/teal.cube" ) );

	// removed: index and flags only
	Writer w;
	w.Short( 9 );
	w.Byte( COLORCORR_FLAG_REMOVED );
	CHECK( colorcorr::Parse( w.b.data(), (int)w.b.size(), p ) && p.index == 9 && ( p.flags & COLORCORR_FLAG_REMOVED ) );

	// every cut of a full message fails, never reads past it
	m = PointMessage( 3, 1, 1, 1, 0, 0, 0, 0, 0, -1, -1, 1, 1, 0, 1, 255, 255, 255, 0, "gfx/colorcorrection/a.raw" );
	for ( size_t cut = 0; cut < m.size(); cut++ )
	{
		Bytes c( m.begin(), m.begin() + cut );
		CHECK( !colorcorr::Parse( cut ? c.data() : NULL, (int)cut, p ) );
	}

	// index out of range
	m = PointMessage( 0, 1, 1, 1, 0, 0, 0, 0, 0, -1, -1, 1, 1, 0, 1, 255, 255, 255, 0, "" );
	CHECK( !colorcorr::Parse( m.data(), (int)m.size(), p ) );
	m = PointMessage( 4000, 1, 1, 1, 0, 0, 0, 0, 0, -1, -1, 1, 1, 0, 1, 255, 255, 255, 0, "" );
	CHECK( !colorcorr::Parse( m.data(), (int)m.size(), p ) );

	// paths the server would not precache are dropped; a long one is cut, then dropped
	const char *bad[] = { "../config.cfg", "gfx/colorcorrection/../../x.raw", "gfx/colorcorrection/a.tga",
		"maps/a.raw", "gfx/colorcorrection/c:\\a.raw", "gfx/colorcorrection/.raw" };
	for ( const char *b : bad )
	{
		m = PointMessage( 3, 1, 1, 1, 0, 0, 0, 0, 0, -1, -1, 1, 1, 0, 1, 255, 255, 255, 0, b );
		CHECK( colorcorr::Parse( m.data(), (int)m.size(), p ) );
		CHECK( p.file[0] == 0 );
	}
	std::string longName = "gfx/colorcorrection/" + std::string( 200, 'a' ) + ".raw";
	m = PointMessage( 3, 1, 1, 1, 0, 0, 0, 0, 0, -1, -1, 1, 1, 0, 1, 255, 255, 255, 0, longName.c_str() );
	CHECK( colorcorr::Parse( m.data(), (int)m.size(), p ) );
	CHECK( p.file[0] == 0 );
	CHECK( ColorCorr_ValidPath( "gfx/colorcorrection/Teal.CUBE" ) );
	CHECK( ColorCorr_ValidPath( "gfx/colorcorrection/sub/a.raw" ) );
}

static Bytes Text( const std::string &s )
{
	return Bytes( s.begin(), s.end() );
}

static std::string IdentityCube( int n, const char *header = "" )
{
	std::string s = std::string( "# a comment\nTITLE \"identity\"\n" ) + header + "LUT_3D_SIZE " + std::to_string( n ) + "\n";
	char line[64];
	for ( int b = 0; b < n; b++ )
		for ( int g = 0; g < n; g++ )
			for ( int r = 0; r < n; r++ )
			{
				snprintf( line, sizeof( line ), "%.6f %.6f %.6f\n", r / (double)( n - 1 ), g / (double)( n - 1 ),
					b / (double)( n - 1 ) );
				s += line;
			}
	return s;
}

static void TestRaw()
{
	colorcorr::Lut lut;
	char err[160];
	Bytes raw( colorcorr::RAW_BYTES );
	for ( size_t i = 0; i < raw.size(); i++ )
		raw[i] = (unsigned char)( i * 7 );
	CHECK( colorcorr::ReadRaw( raw.data(), (int)raw.size(), lut, err, sizeof( err ) ) );
	CHECK( lut.size == 32 && lut.rgb == raw );
	CHECK( !colorcorr::ReadRaw( raw.data(), (int)raw.size() - 1, lut, err, sizeof( err ) ) );
	CHECK( strstr( err, "98304" ) != NULL );
	CHECK( !colorcorr::ReadRaw( raw.data(), 0, lut, err, sizeof( err ) ) );
	CHECK( !colorcorr::ReadRaw( NULL, colorcorr::RAW_BYTES, lut, err, sizeof( err ) ) );
	// by extension
	CHECK( colorcorr::ReadTable( "gfx/colorcorrection/x.RAW", raw.data(), (int)raw.size(), lut, err, sizeof( err ) ) );
	CHECK( !colorcorr::ReadTable( "gfx/colorcorrection/x.tga", raw.data(), (int)raw.size(), lut, err, sizeof( err ) ) );
}

static void TestCube()
{
	colorcorr::Lut lut, ident;
	char err[160];
	for ( int n : { 2, 17, 33 } )
	{
		Bytes t = Text( IdentityCube( n ) );
		CHECK( colorcorr::ReadCube( t.data(), (int)t.size(), lut, err, sizeof( err ) ) );
		CHECK( lut.size == n );
		colorcorr::IdentityLut( n, ident );
		CHECK( lut.rgb == ident.rgb );
	}
	// red fastest: entry 1 of a 2-cube is red 1
	{
		Bytes t = Text( IdentityCube( 2 ) );
		CHECK( colorcorr::ReadCube( t.data(), (int)t.size(), lut, err, sizeof( err ) ) );
		CHECK( lut.rgb[3] == 255 && lut.rgb[4] == 0 && lut.rgb[5] == 0 );
	}
	// CRLF line ends, tabs, domain lines, no newline at the end
	{
		std::string s = IdentityCube( 2, "DOMAIN_MIN 0 0 0\r\nDOMAIN_MAX 1.0 1.0 1.0\r\n" );
		for ( size_t i = 0; ( i = s.find( '\n', i ) ) != std::string::npos; i += 2 )
			s.replace( i, 1, "\r\n" );
		s.resize( s.size() - 2 );
		Bytes t = Text( s );
		CHECK( colorcorr::ReadCube( t.data(), (int)t.size(), lut, err, sizeof( err ) ) );
	}
	// values outside 0 .. 1 are clamped
	{
		std::string s = "LUT_3D_SIZE 2\n";
		for ( int i = 0; i < 8; i++ )
			s += "-1 2 0.5\n";
		Bytes t = Text( s );
		CHECK( colorcorr::ReadCube( t.data(), (int)t.size(), lut, err, sizeof( err ) ) );
		CHECK( lut.rgb[0] == 0 && lut.rgb[1] == 255 && lut.rgb[2] == 128 );
	}
	// hostile and broken files
	const char *bad[] = {
		"",
		"LUT_3D_SIZE 2\n",										// no values
		"0 0 0\nLUT_3D_SIZE 2\n",								// values first
		"LUT_3D_SIZE 1\n0 0 0\n",								// too small
		"LUT_3D_SIZE 65\n",										// too big
		"LUT_3D_SIZE 100000000\n",
		"LUT_3D_SIZE -3\n",
		"LUT_3D_SIZE 2.5\n",
		"LUT_3D_SIZE 2\nLUT_3D_SIZE 2\n",
		"LUT_1D_SIZE 1024\n",
		"LUT_3D_SIZE 2\nDOMAIN_MIN -1 0 0\n",
		"LUT_3D_SIZE 2\nDOMAIN_MAX 2 2 2\n",
		"LUT_3D_SIZE 2\nFOO 1\n",
		"LUT_3D_SIZE 2\n0 0\n",
		"LUT_3D_SIZE 2\n0 0 0 0\n",
		"LUT_3D_SIZE 2\nnan 0 0\n",
		"LUT_3D_SIZE 2\n1e999 0 0\n",
		"LUT_3D_SIZE 2\n0x 0 0\n",
		"LUT_3D_SIZE\n",
	};
	for ( const char *b : bad )
	{
		Bytes t = Text( b );
		CHECK( !colorcorr::ReadCube( t.empty() ? NULL : t.data(), (int)t.size(), lut, err, sizeof( err ) ) );
		CHECK( err[0] != 0 );
	}
	// too many values, a keyword after the values, a zero byte, a line too long
	{
		Bytes t = Text( IdentityCube( 2 ) + "0 0 0\n" );
		CHECK( !colorcorr::ReadCube( t.data(), (int)t.size(), lut, err, sizeof( err ) ) );
		t = Text( IdentityCube( 2 ) + "TITLE \"x\"\nLUT_3D_SIZE 2\n" );
		CHECK( !colorcorr::ReadCube( t.data(), (int)t.size(), lut, err, sizeof( err ) ) );
		t = Text( IdentityCube( 2 ) );
		t[30] = 0;
		CHECK( !colorcorr::ReadCube( t.data(), (int)t.size(), lut, err, sizeof( err ) ) );
		t = Text( "LUT_3D_SIZE 2\n" + std::string( 10000, ' ' ) + "0 0 0\n" );
		CHECK( !colorcorr::ReadCube( t.data(), (int)t.size(), lut, err, sizeof( err ) ) );
		CHECK( strstr( err, "line too long" ) != NULL );
	}
	// a cut file: every prefix fails or (at a line end of the full table) succeeds, never reads past
	{
		Bytes full = Text( IdentityCube( 3 ) );
		for ( size_t cut = 1; cut < full.size(); cut += 3 )
		{
			Bytes c( full.begin(), full.begin() + cut );
			colorcorr::ReadCube( c.data(), (int)c.size(), lut, err, sizeof( err ) );
		}
	}
	// the error buffer may be tiny or missing
	{
		Bytes t = Text( "LUT_3D_SIZE 99\n" );
		char small[4];
		CHECK( !colorcorr::ReadCube( t.data(), (int)t.size(), lut, small, sizeof( small ) ) );
		CHECK( !colorcorr::ReadCube( t.data(), (int)t.size(), lut, NULL, 0 ) );
	}
}

static colorcorr::Params Neutral()
{
	colorcorr::Params p = {};
	p.index = 1;
	p.maxWeight = 1;
	p.saturation = p.contrast = p.gamma = 1;
	p.tint[0] = p.tint[1] = p.tint[2] = 255;
	p.minFalloff = p.maxFalloff = -1;
	return p;
}

static void TestSimpleKeys()
{
	colorcorr::Params p = Neutral();
	CHECK( !colorcorr::HasSimpleKeys( p ) );
	float c[3] = { 0.2f, 0.5f, 0.9f };
	colorcorr::ApplySimpleKeys( p, c );
	NEAR( c[0], 0.2f );
	NEAR( c[1], 0.5f );
	NEAR( c[2], 0.9f );

	// black and white: every channel the luma
	p.saturation = 0;
	CHECK( colorcorr::HasSimpleKeys( p ) );
	float d[3] = { 1.0f, 0.0f, 0.0f };
	colorcorr::ApplySimpleKeys( p, d );
	NEAR( d[0], 0.2126f );
	NEAR( d[1], 0.2126f );
	NEAR( d[2], 0.2126f );

	// contrast around middle gray, brightness added, clamped
	p = Neutral();
	p.contrast = 2;
	float e[3] = { 0.25f, 0.5f, 0.9f };
	colorcorr::ApplySimpleKeys( p, e );
	NEAR( e[0], 0.0f );
	NEAR( e[1], 0.5f );
	NEAR( e[2], 1.0f );
	p = Neutral();
	p.brightness = 0.1f;
	float f[3] = { 0.0f, 0.5f, 0.95f };
	colorcorr::ApplySimpleKeys( p, f );
	NEAR( f[0], 0.1f );
	NEAR( f[1], 0.6f );
	NEAR( f[2], 1.0f );

	// gamma 2: the square root
	p = Neutral();
	p.gamma = 2;
	float g[3] = { 0.25f, 0.0f, 1.0f };
	colorcorr::ApplySimpleKeys( p, g );
	NEAR( g[0], 0.5f );
	NEAR( g[1], 0.0f );
	NEAR( g[2], 1.0f );

	// tint: multiplied in by its amount
	p = Neutral();
	p.tint[0] = 255;
	p.tint[1] = 0;
	p.tint[2] = 0;
	p.tintAmount = 0.5f;
	float h[3] = { 1.0f, 1.0f, 1.0f };
	colorcorr::ApplySimpleKeys( p, h );
	NEAR( h[0], 1.0f );
	NEAR( h[1], 0.5f );
	NEAR( h[2], 0.5f );
	// a white tint or amount 0 is no key
	p.tintAmount = 0;
	CHECK( !colorcorr::HasSimpleKeys( p ) );

	// hostile values: no NaN out
	p = Neutral();
	p.gamma = 0;
	p.contrast = -40;
	p.saturation = 30;
	float k[3] = { 0.3f, 0.6f, 0.1f };
	colorcorr::ApplySimpleKeys( p, k );
	for ( float v : k )
		CHECK( v >= 0.0f && v <= 1.0f );

	// tables: identity without keys, a 32^3 table, the file's size with a file
	colorcorr::Lut lut, ident, file;
	p = Neutral();
	colorcorr::BuildLut( p, NULL, lut );
	colorcorr::IdentityLut( 32, ident );
	CHECK( lut.size == 32 && lut.rgb == ident.rgb );
	p.saturation = 0;
	colorcorr::BuildLut( p, NULL, lut );
	CHECK( lut.size == 32 );
	const size_t red = ( 31 + 0 * 32 + 0 * 1024 ) * 3;	// pure red's cell
	CHECK( lut.rgb[red] == lut.rgb[red + 1] && lut.rgb[red + 1] == lut.rgb[red + 2] );
	CHECK( abs( (int)lut.rgb[red] - 54 ) <= 1 );	// 0.2126 * 255
	colorcorr::IdentityLut( 17, file );
	colorcorr::BuildLut( p, &file, lut );
	CHECK( lut.size == 17 );
	// a broken file table is not used
	file.rgb.resize( 10 );
	colorcorr::BuildLut( p, &file, lut );
	CHECK( lut.size == 32 );
}

static void TestWeights()
{
	colorcorr::Params p = Neutral();
	p.origin[0] = 100;
	const float view0[3] = { 100, 0, 0 };
	const float view1[3] = { 100, 300, 0 };
	// no falloff
	NEAR( colorcorr::FalloffWeight( p, 0.7f, view1 ), 0.7f );
	// full to 100, none from 500, linear between
	p.minFalloff = 100;
	p.maxFalloff = 500;
	NEAR( colorcorr::FalloffWeight( p, 1.0f, view0 ), 1.0f );
	NEAR( colorcorr::FalloffWeight( p, 1.0f, view1 ), 0.5f );
	const float view2[3] = { 100, 900, 0 };
	NEAR( colorcorr::FalloffWeight( p, 1.0f, view2 ), 0.0f );
	// min == max: no falloff
	p.maxFalloff = 100;
	NEAR( colorcorr::FalloffWeight( p, 0.4f, view2 ), 0.4f );

	// fades
	NEAR( colorcorr::Approach( 0.0f, 1.0f, 0.5f, 1.0f ), 0.5f );
	NEAR( colorcorr::Approach( 0.0f, 1.0f, 0.5f, 3.0f ), 1.0f );
	NEAR( colorcorr::Approach( 1.0f, 0.0f, 2.0f, 0.25f ), 0.5f );
	NEAR( colorcorr::Approach( 1.0f, 0.0f, 0.0f, 0.0f ), 0.0f );	// no fade time: at once
	NEAR( colorcorr::Approach( 0.3f, 1.0f, 1.0f, -5.0f ), 0.3f );	// time went back: stays
	NEAR( colorcorr::FadeRate( 0.8f, 2.0f ), 0.4f );
	NEAR( colorcorr::FadeRate( 0.8f, 0.0f ), 0.0f );
}

static void TestBlend()
{
	int idx[ colorcorr::BLEND_MAX ];
	float w[ colorcorr::BLEND_MAX ];
	float identity;

	CHECK( colorcorr::Blend( NULL, NULL, 0, idx, w, identity ) == 0 );
	NEAR( identity, 1.0f );

	const float a[] = { 0.3f, 0.0f, 0.2f };
	CHECK( colorcorr::Blend( a, NULL, 3, idx, w, identity ) == 2 );
	CHECK( idx[0] == 0 && idx[1] == 2 );
	NEAR( identity, 0.5f );

	// over 1: scaled to 1
	const float b[] = { 0.9f, 0.6f };
	CHECK( colorcorr::Blend( b, NULL, 2, idx, w, identity ) == 2 );
	NEAR( w[0], 0.6f );
	NEAR( w[1], 0.4f );
	NEAR( identity, 0.0f );

	// the four heaviest of six, heaviest first
	const float c[] = { 0.05f, 0.1f, 0.3f, 0.02f, 0.2f, 0.15f };
	CHECK( colorcorr::Blend( c, NULL, 6, idx, w, identity ) == 4 );
	CHECK( idx[0] == 2 && idx[1] == 4 && idx[2] == 5 && idx[3] == 1 );
	NEAR( identity, 1.0f - 0.75f );

	// exclusive: the others keep (1 - E) of theirs
	const float d[] = { 0.5f, 1.0f };
	const bool ex[] = { true, false };
	CHECK( colorcorr::Blend( d, ex, 2, idx, w, identity ) == 2 );
	NEAR( w[0], 0.5f );
	NEAR( w[1], 0.5f );
	const float e[] = { 1.0f, 1.0f };
	CHECK( colorcorr::Blend( e, ex, 2, idx, w, identity ) == 1 );
	CHECK( idx[0] == 0 );
	NEAR( identity, 0.0f );

	// hostile weights
	const float f[] = { NAN, -3.0f, 50.0f, INFINITY };
	const int n = colorcorr::Blend( f, NULL, 4, idx, w, identity );
	CHECK( n >= 1 && n <= 4 );
	CHECK( identity >= 0.0f && identity <= 1.0f );
}

// seeded random mutations of good input: no crash, no sanitizer report
static void Fuzz( int iterations )
{
	unsigned int seed = 12345;
	auto Rand = [&]() { seed = seed * 1103515245u + 12345u; return ( seed >> 16 ) & 0x7FFF; };
	const Bytes cube = Text( IdentityCube( 4, "DOMAIN_MIN 0 0 0\n" ) );
	const Bytes msg = PointMessage( 5, 7, 1, 1, 1, 1, 1, 2, 3, 4, 5, 1, 1, 0, 1, 1, 2, 3, 4, "gfx/colorcorrection/f.cube" );
	const Bytes vol = VolumeMessage( 6, 1, 3, "gfx/colorcorrection/v.raw" );
	const char *tokens[] = { "LUT_3D_SIZE ", "DOMAIN_MAX ", "\n", "\r", " ", "#", "nan", "1e40", "-", "64", "\0", "TITLE" };
	colorcorr::Lut lut;
	colorcorr::Params p;
	char err[64];
	for ( int i = 0; i < iterations; i++ )
	{
		Bytes c = cube;
		const int edits = 1 + Rand() % 8;
		for ( int k = 0; k < edits && !c.empty(); k++ )
		{
			const size_t at = Rand() % c.size();
			switch ( Rand() % 4 )
			{
			case 0: c[at] = (unsigned char)Rand(); break;
			case 1: c.erase( c.begin() + at ); break;
			case 2: c.resize( at ); break;
			default:
			{
				const char *t = tokens[ Rand() % ( sizeof( tokens ) / sizeof( tokens[0] ) ) ];
				const size_t len = t[0] ? strlen( t ) : 1;
				c.insert( c.begin() + at, (const unsigned char *)t, (const unsigned char *)t + len );
			}
			}
		}
		if ( colorcorr::ReadCube( c.empty() ? NULL : c.data(), (int)c.size(), lut, err, sizeof( err ) ) )
			CHECK( lut.size >= 2 && lut.size <= 64 && lut.rgb.size() == (size_t)lut.size * lut.size * lut.size * 3 );

		Bytes m = ( i & 1 ) ? msg : vol;
		const int medits = 1 + Rand() % 4;
		for ( int k = 0; k < medits && !m.empty(); k++ )
		{
			const size_t at = Rand() % m.size();
			if ( Rand() % 3 )
				m[at] = (unsigned char)Rand();
			else
				m.resize( at );
		}
		if ( colorcorr::Parse( m.empty() ? NULL : m.data(), (int)m.size(), p ) )
		{
			CHECK( p.index >= 1 && p.index <= 2047 );
			CHECK( strlen( p.file ) < sizeof( p.file ) );
			CHECK( p.maxWeight >= 0.0f && p.maxWeight <= 1.0f );
			colorcorr::Lut table;
			colorcorr::BuildLut( p, NULL, table );
			CHECK( table.size == 32 );
			const float view[3] = { (float)( Rand() - 16384 ), 0, 0 };
			const float w = colorcorr::FalloffWeight( p, p.weightNow, view );
			CHECK( w >= 0.0f && w <= 1.0f );
		}

		Bytes raw( ( Rand() % 3 ) ? colorcorr::RAW_BYTES : Rand() % 200000 );
		colorcorr::ReadRaw( raw.empty() ? NULL : raw.data(), (int)raw.size(), lut, err, sizeof( err ) );
	}
}

// a table file read for one map, then a different file under the same name on the next map (a server changed it):
// the second read stands on its own; missing files (no data) fail cleanly
static void TestFileChanges()
{
	colorcorr::Lut a, b, table;
	char err[160];
	Bytes raw( colorcorr::RAW_BYTES, 10 );
	CHECK( colorcorr::ReadTable( "gfx/colorcorrection/x.raw", raw.data(), (int)raw.size(), a, err, sizeof( err ) ) );
	Bytes cube = Text( IdentityCube( 5 ) );
	CHECK( colorcorr::ReadTable( "gfx/colorcorrection/x.cube", cube.data(), (int)cube.size(), b, err, sizeof( err ) ) );
	CHECK( a.size == 32 && b.size == 5 && b.rgb.size() == 5 * 5 * 5 * 3 );
	// the same Lut object reused for a shorter / failed read keeps a consistent state
	Bytes bad = Text( "LUT_3D_SIZE 3\n0 0 0\n" );
	CHECK( !colorcorr::ReadTable( "gfx/colorcorrection/x.cube", bad.data(), (int)bad.size(), b, err, sizeof( err ) ) );
	CHECK( b.rgb.size() == (size_t)b.size * b.size * b.size * 3 );
	CHECK( !colorcorr::ReadTable( "gfx/colorcorrection/x.raw", NULL, 0, a, err, sizeof( err ) ) );
	CHECK( !colorcorr::ReadTable( "gfx/colorcorrection/x.cube", NULL, 0, a, err, sizeof( err ) ) );
	CHECK( !colorcorr::ReadTable( NULL, raw.data(), (int)raw.size(), a, err, sizeof( err ) ) );
	CHECK( a.rgb.size() == (size_t)a.size * a.size * a.size * 3 );
	colorcorr::Params p = Neutral();
	p.saturation = 0;
	colorcorr::BuildLut( p, &a, table );
	CHECK( table.size == 32 );
	colorcorr::BuildLut( p, &b, table );
	CHECK( table.size == 5 && table.rgb.size() == 5 * 5 * 5 * 3 );
	// a Lut whose size and data disagree (never made by the readers) is not used
	colorcorr::Lut liar;
	liar.size = 64;
	liar.rgb.resize( 3 );
	colorcorr::BuildLut( p, &liar, table );
	CHECK( table.size == 32 );
}

// messages of random bytes and random lengths (not mutations of good ones), every flag combination
static void FuzzRandomMessages( int iterations )
{
	unsigned int seed = 777;
	auto Rand = [&]() { seed = seed * 1103515245u + 12345u; return ( seed >> 16 ) & 0x7FFF; };
	colorcorr::Params p;
	for ( int i = 0; i < iterations; i++ )
	{
		Bytes m( Rand() % 260 );
		for ( auto &c : m )
			c = (unsigned char)Rand();
		if ( m.size() >= 3 )
		{
			m[0] = (unsigned char)( 1 + Rand() % 200 );
			m[1] = 0;
			m[2] = (unsigned char)( i & 31 );
		}
		if ( colorcorr::Parse( m.empty() ? NULL : m.data(), (int)m.size(), p ) )
		{
			CHECK( strlen( p.file ) < sizeof( p.file ) );
			CHECK( !p.file[0] || ColorCorr_ValidPath( p.file ) );
			CHECK( p.weightNow <= p.maxWeight );
		}
	}
}

int main( int argc, char **argv )
{
	const int iterations = argc > 1 ? atoi( argv[1] ) : 20000;
	TestMessage();
	TestRaw();
	TestCube();
	TestSimpleKeys();
	TestWeights();
	TestBlend();
	TestFileChanges();
	Fuzz( iterations );
	FuzzRandomMessages( iterations );
	if ( s_iFailed )
	{
		printf( "colorcorrection: %d check(s) FAILED\n", s_iFailed );
		return 1;
	}
	printf( "colorcorrection: all checks passed (fuzz %d)\n", iterations );
	return 0;
}
