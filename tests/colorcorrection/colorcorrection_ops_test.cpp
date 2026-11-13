// Offline test of the color correction editor's operations (src/cl_dll/colorcorrection_ops.cpp): every operation's
// neutral settings change nothing, known settings give known colors, the table is a valid Source .raw that
// colorcorr::ReadRaw takes back, the curve math stays monotone, look names are cleaned, and random settings never
// leave 0 .. 1 or read out of bounds (ASan/UBSan, tests/colorcorrection/run_test.sh).

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "colorcorrection_ops.h"

using namespace colorcorr;
using namespace colorcorr::ops;

static int s_iFailed = 0;
#define CHECK( cond ) do { if ( !( cond ) ) { printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond ); s_iFailed++; } } while ( 0 )
#define NEAR( a, b, eps ) CHECK( fabs( (double)( a ) - (double)( b ) ) < ( eps ) )

static unsigned s_Seed = 12345;
static unsigned Rand()
{
	s_Seed = s_Seed * 1103515245u + 12345u;
	return ( s_Seed >> 8 ) & 0xffffff;
}

static bool IsIdentity( const Lut &lut )
{
	Lut id;
	IdentityLut( lut.size, id );
	for ( size_t i = 0; i < lut.rgb.size(); i++ )
		if ( abs( (int)lut.rgb[i] - (int)id.rgb[i] ) > 1 )
			return false;
	return true;
}

static void TestNeutral()
{
	for ( int t = 0; t < OP_COUNT; t++ )
	{
		std::vector<Operation> ops = { Make( t ) };
		CHECK( !strcmp( ops[0].name.c_str(), TypeName( t ) ) );
		Lut lut;
		BuildLut( ops, RAW_SIZE, lut );
		CHECK( lut.size == RAW_SIZE && (int)lut.rgb.size() == RAW_BYTES );
		// Selected HSV with no change, a Lookup without a file: nothing changes either
		CHECK( IsIdentity( lut ) );
	}
	Lut empty;
	BuildLut( {}, RAW_SIZE, empty );
	CHECK( IsIdentity( empty ) );
}

static void TestKnown()
{
	// curves: the RGB curve through (0.5, 0.75) lifts mid grey
	Operation c = Make( OP_CURVES );
	c.curves[ CH_RGB ].count = 3;
	c.curves[ CH_RGB ].points[1] = { 0.5f, 0.75f };
	c.curves[ CH_RGB ].points[2] = { 1.0f, 1.0f };
	float g[3] = { 0.5f, 0.5f, 0.5f };
	ApplyOne( c, g );
	NEAR( g[0], 0.75f, 1e-4 );
	// blend 0: nothing; disabled: nothing
	c.blend = 0;
	float g2[3] = { 0.5f, 0.5f, 0.5f };
	ApplyOne( c, g2 );
	NEAR( g2[0], 0.5f, 1e-6 );
	c.blend = 255;
	c.enabled = false;
	ApplyOne( c, g2 );
	NEAR( g2[0], 0.5f, 1e-6 );

	// levels: input white 128 doubles, output black 255 makes white
	Operation l = Make( OP_LEVELS );
	l.levels[ CH_RED ].inWhite = 128;
	float r[3] = { 0.25f, 0.25f, 0.25f };
	ApplyOne( l, r );
	NEAR( r[0], 0.25f * 255.0f / 128.0f, 0.01 );
	NEAR( r[1], 0.25f, 1e-4 );
	l.levels[ CH_RGB ].outBlack = 255;
	float k[3] = { 0, 0, 0 };
	ApplyOne( l, k );
	NEAR( k[2], 1.0f, 1e-4 );

	// selected HSV: pure red within tolerance turned green by +120 hue; blue far away untouched
	Operation s = Make( OP_SELECTED_HSV );
	s.target[0] = 255; s.target[1] = 0; s.target[2] = 0;
	s.tolerance = 40;
	s.hue = 120;
	float red[3] = { 1, 0, 0 }, blue[3] = { 0, 0, 1 };
	ApplyOne( s, red );
	ApplyOne( s, blue );
	NEAR( red[0], 0.0f, 1e-4 );
	NEAR( red[1], 1.0f, 1e-4 );
	NEAR( blue[2], 1.0f, 1e-4 );
	// invert: blue changes, red doesn't
	s.invert = true;
	float red2[3] = { 1, 0, 0 }, blue2[3] = { 0, 0, 1 };
	ApplyOne( s, red2 );
	ApplyOne( s, blue2 );
	NEAR( red2[0], 1.0f, 1e-4 );
	NEAR( blue2[0], 1.0f, 1e-4 );	// blue + 120 = red
	// fuzziness: half way through the fade, half the weight
	s.invert = false;
	s.tolerance = 0;
	s.fuzziness = 100;
	float near[3] = { 205.0f / 255.0f, 0, 0 };
	NEAR( SelectionWeight( s, near ), 0.5f, 0.01 );
	// hue mode: a grey is never selected
	s.selectMode = SELECT_HUE;
	s.tolerance = 200;
	float grey[3] = { 0.4f, 0.4f, 0.4f };
	CHECK( SelectionWeight( s, grey ) < 1.0f );

	// color balance: shadows toward red raise red in the darks, keep the highlights; preserve luminosity off
	Operation b = Make( OP_BALANCE );
	b.preserveLuminosity = false;
	b.balance[ RANGE_SHADOWS ][0] = 100;
	float dark[3] = { 0.1f, 0.1f, 0.1f }, light[3] = { 0.95f, 0.95f, 0.95f };
	ApplyOne( b, dark );
	ApplyOne( b, light );
	CHECK( dark[0] > 0.3f );
	NEAR( dark[1], 0.1f, 1e-4 );
	NEAR( light[0], 0.95f, 1e-3 );
	// with Preserve Luminosity the HSL lightness stays
	b.preserveLuminosity = true;
	b.balance[ RANGE_MIDTONES ][2] = 80;
	float mid[3] = { 0.5f, 0.4f, 0.3f };
	const float before = 0.5f * ( 0.5f + 0.3f );
	ApplyOne( b, mid );
	float mx = mid[0], mn = mid[0];
	for ( int i = 1; i < 3; i++ )
	{
		mx = mid[i] > mx ? mid[i] : mx;
		mn = mid[i] < mn ? mid[i] : mn;
	}
	NEAR( 0.5f * ( mx + mn ), before, 0.01 );

	// lookup: a table that swaps red and blue
	Operation look = Make( OP_LOOKUP );
	IdentityLut( 8, look.lut );
	for ( int bb = 0; bb < 8; bb++ )
		for ( int gg = 0; gg < 8; gg++ )
			for ( int rr = 0; rr < 8; rr++ )
			{
				unsigned char *p = &look.lut.rgb[ ( ( bb * 8 + gg ) * 8 + rr ) * 3 ];
				const unsigned char t = p[0];
				p[0] = p[2];
				p[2] = t;
			}
	float px[3] = { 1, 0, 0 };
	ApplyOne( look, px );
	NEAR( px[0], 0.0f, 0.01 );
	NEAR( px[2], 1.0f, 0.01 );

	// HSV round trip
	for ( int i = 0; i < 200; i++ )
	{
		float c3[3] = { ( Rand() % 1000 ) / 999.0f, ( Rand() % 1000 ) / 999.0f, ( Rand() % 1000 ) / 999.0f }, o[3];
		float h, sat, v;
		RgbToHsv( c3, h, sat, v );
		HsvToRgb( h, sat, v, o );
		for ( int j = 0; j < 3; j++ )
			NEAR( o[j], c3[j], 1e-4 );
	}
}

static void TestRawRoundTrip()
{
	Operation c = Make( OP_SELECTED_HSV );
	c.colorize = true;
	c.hue = 100;
	c.tolerance = 255;
	std::vector<Operation> ops = { c };
	Lut lut, back;
	BuildLut( ops, RAW_SIZE, lut );
	char err[128];
	CHECK( ReadRaw( lut.rgb.data(), (int)lut.rgb.size(), back, err, sizeof( err ) ) );
	CHECK( back.size == RAW_SIZE && back.rgb == lut.rgb );

	std::vector<unsigned char> rgba;
	int w = 0, h = 0;
	LutImage( lut, 4, rgba, w, h );
	CHECK( w == 128 && h == 256 && rgba.size() == (size_t)w * h * 4 );
	// tile of blue 5: column 1, row 1; its red 3 / green 7 cell
	const unsigned char *px = &rgba[ ( ( 32 + 7 ) * w + 32 + 3 ) * 4 ];
	const unsigned char *src = &lut.rgb[ ( ( 5 * 32 + 7 ) * 32 + 3 ) * 3 ];
	CHECK( px[0] == src[0] && px[1] == src[1] && px[2] == src[2] && px[3] == 255 );
	Lut bad;
	LutImage( bad, 4, rgba, w, h );
	CHECK( w == 0 && h == 0 && rgba.empty() );
}

static void TestCurveMath()
{
	sboctl::CurvePoint p[4] = { { 0, 0 }, { 0.3f, 0.6f }, { 0.31f, 0.62f }, { 1, 1 } };
	float last = -1.0f;
	for ( int i = 0; i <= 1000; i++ )
	{
		const float y = sboctl::EvalMonotoneCurve( p, 4, i / 1000.0f );
		CHECK( y >= last - 1e-5f );
		CHECK( y >= -1e-4f && y <= 1.0001f );
		last = y;
	}
	NEAR( sboctl::EvalMonotoneCurve( p, 4, 0.3f ), 0.6f, 1e-5 );
	NEAR( sboctl::EvalMonotoneCurve( p, 1, 0.9f ), 0.0f, 1e-6 );
	NEAR( sboctl::EvalMonotoneCurve( NULL, 0, 0.9f ), 0.9f, 1e-6 );
}

static void TestNames()
{
	char out[64];
	CHECK( CleanName( "Night_Blue-2", out, sizeof( out ) ) && !strcmp( out, "night_blue-2" ) );
	CHECK( !CleanName( "", out, sizeof( out ) ) );
	CHECK( !CleanName( "../evil", out, sizeof( out ) ) && !out[0] );
	CHECK( !CleanName( "a b", out, sizeof( out ) ) );
	CHECK( !CleanName( "x.raw", out, sizeof( out ) ) );
	CHECK( !CleanName( "a\\b", out, sizeof( out ) ) );
	CHECK( !CleanName( NULL, out, sizeof( out ) ) );
	CHECK( CleanName( "abcdefghijabcdefghijabcdefghijabcdefghi", out, sizeof( out ) ) );	// 39
	CHECK( !CleanName( "abcdefghijabcdefghijabcdefghijabcdefghij", out, sizeof( out ) ) );	// 40
	CHECK( !CleanName( "abcdef", out, 4 ) );
	char path[ COLORCORR_MAX_PATH * 2 ];
	CleanName( "abcdefghijabcdefghijabcdefghijabcdefghi", out, sizeof( out ) );
	snprintf( path, sizeof( path ), "%s%s.raw", COLORCORR_FOLDER, out );
	CHECK( ColorCorr_ValidPath( path ) );
}

static void Fuzz( int iterations )
{
	for ( int i = 0; i < iterations / 20; i++ )
	{
		std::vector<Operation> ops;
		const int n = 1 + Rand() % 4;
		for ( int k = 0; k < n; k++ )
		{
			Operation op = Make( (int)( Rand() % ( OP_COUNT + 1 ) ) - ( Rand() % 2 ) );
			op.blend = (int)( Rand() % 600 ) - 200;
			op.enabled = Rand() % 5 != 0;
			for ( int ch = 0; ch < CH_COUNT; ch++ )
			{
				op.curves[ch].count = (int)( Rand() % 40 ) - 10;
				float x = 0.0f;
				for ( int j = 0; j < sboctl::CURVE_MAX_POINTS; j++ )
				{
					x += ( Rand() % 100 ) / 400.0f;
					op.curves[ch].points[j] = { x, ( (int)( Rand() % 300 ) - 100 ) / 100.0f };
				}
				op.levels[ch].inBlack = (int)( Rand() % 600 ) - 200;
				op.levels[ch].inWhite = (int)( Rand() % 600 ) - 200;
				op.levels[ch].gamma = ( (int)( Rand() % 2000 ) - 500 ) / 100.0f;
				op.levels[ch].outBlack = (int)( Rand() % 600 ) - 200;
				op.levels[ch].outWhite = (int)( Rand() % 600 ) - 200;
			}
			op.selectMode = (int)( Rand() % 3 );
			for ( int j = 0; j < 3; j++ )
				op.target[j] = (int)( Rand() % 600 ) - 200;
			op.tolerance = (int)( Rand() % 600 ) - 200;
			op.fuzziness = (int)( Rand() % 600 ) - 200;
			op.hue = (int)( Rand() % 4000 ) - 2000;
			op.saturation = (int)( Rand() % 1000 ) - 500;
			op.value = (int)( Rand() % 1000 ) - 500;
			op.colorize = Rand() % 2;
			op.invert = Rand() % 2;
			for ( int r = 0; r < RANGE_COUNT; r++ )
				for ( int j = 0; j < 3; j++ )
					op.balance[r][j] = (int)( Rand() % 600 ) - 300;
			op.preserveLuminosity = Rand() % 2;
			if ( Rand() % 2 )
			{
				const int sz = (int)( Rand() % 5 );
				op.lut.size = sz;
				op.lut.rgb.resize( Rand() % 3 ? (size_t)sz * sz * sz * 3 : Rand() % 50 );
				for ( auto &b : op.lut.rgb )
					b = (unsigned char)Rand();
			}
			ops.push_back( op );
		}
		Lut lut;
		BuildLut( ops, 2 + (int)( Rand() % 8 ), lut );
		CHECK( lut.rgb.size() == (size_t)lut.size * lut.size * lut.size * 3 );
		float c[3] = { ( Rand() % 1000 ) / 999.0f, ( Rand() % 1000 ) / 999.0f, ( Rand() % 1000 ) / 999.0f };
		ApplyAll( ops, c );
		for ( int j = 0; j < 3; j++ )
			CHECK( c[j] >= 0.0f && c[j] <= 1.0f );
	}
}

static Operation RandomOp()
{
	Operation op = Make( (int)( Rand() % OP_COUNT ) );
	op.name = Rand() % 2 ? "Name \"with\" quote\\" : "plain";
	op.blend = (int)( Rand() % 256 );
	op.enabled = Rand() % 4 != 0;
	for ( int ch = 0; ch < CH_COUNT; ch++ )
	{
		const int n = 2 + (int)( Rand() % ( sboctl::CURVE_MAX_POINTS - 1 ) );
		op.curves[ch].count = n;
		for ( int j = 0; j < n; j++ )
			op.curves[ch].points[j] = { j / (float)( n - 1 ), ( Rand() % 100001 ) / 100000.0f };
		op.levels[ch].inBlack = (int)( Rand() % 100 );
		op.levels[ch].inWhite = 150 + (int)( Rand() % 106 );
		op.levels[ch].gamma = 0.1f + ( Rand() % 9890 ) / 1000.0f;
		op.levels[ch].outBlack = (int)( Rand() % 256 );
		op.levels[ch].outWhite = (int)( Rand() % 256 );
	}
	op.selectMode = (int)( Rand() % SELECT_COUNT );
	for ( int j = 0; j < 3; j++ )
		op.target[j] = (int)( Rand() % 256 );
	op.tolerance = (int)( Rand() % 256 );
	op.fuzziness = (int)( Rand() % 256 );
	op.hue = (int)( Rand() % 721 ) - 360;
	op.saturation = (int)( Rand() % 511 ) - 255;
	op.value = (int)( Rand() % 511 ) - 255;
	op.colorize = Rand() % 2;
	op.invert = Rand() % 2;
	for ( int r = 0; r < RANGE_COUNT; r++ )
		for ( int j = 0; j < 3; j++ )
			op.balance[r][j] = (int)( Rand() % 201 ) - 100;
	op.preserveLuminosity = Rand() % 2;
	op.file = "look.raw";
	return op;
}

// The editor's Load: the operations read back build the very same table (byte for byte)
static void TestOpsFile()
{
	for ( int it = 0; it < 60; it++ )
	{
		std::vector<Operation> ops;
		const int n = 1 + (int)( Rand() % 5 );
		for ( int k = 0; k < n; k++ )
			ops.push_back( RandomOp() );
		const int blend = (int)( Rand() % 256 );
		std::string text;
		WriteOpsText( ops, blend, text );
		std::vector<Operation> back;
		int blendBack = -1;
		char err[ 128 ] = "";
		CHECK( ReadOpsText( text.data(), (int)text.size(), back, blendBack, err, sizeof( err ) ) );
		CHECK( blendBack == blend );
		CHECK( back.size() == ops.size() );
		if ( back.size() != ops.size() )
			continue;
		for ( size_t k = 0; k < ops.size(); k++ )
		{
			CHECK( back[k].type == ops[k].type );
			CHECK( back[k].enabled == ops[k].enabled && back[k].blend == ops[k].blend );
			CHECK( !strchr( back[k].name.c_str(), '"' ) && !strchr( back[k].name.c_str(), '\\' ) );
			if ( ops[k].type == OP_LOOKUP )
				CHECK( back[k].file == "look.raw" );
		}
		Lut a, b;
		BuildLut( ops, RAW_SIZE, a );
		BuildLut( back, RAW_SIZE, b );
		CHECK( a.rgb == b.rgb );
	}
	// cut short at every length, and hostile bytes: never a crash, never more than OPS_MAX operations
	std::vector<Operation> ops = { RandomOp(), RandomOp(), RandomOp() };
	std::string text;
	WriteOpsText( ops, 200, text );
	for ( size_t cut = 0; cut < text.size(); cut++ )
	{
		std::vector<Operation> back;
		int blend;
		char err[ 64 ];
		ReadOpsText( cut ? text.data() : NULL, (int)cut, back, blend, err, sizeof( err ) );
		CHECK( back.size() <= 3 );
	}
	std::string many = "\"ColorCorrectionOps\" {";
	for ( int i = 0; i < OPS_MAX + 5; i++ )
		many += " \"op\" { \"type\" \"levels\" \"levels0\" \"-5 999 nan -1 300\" \"curve0\" \"0 0 0 1 1 1\" }";
	many += " }";
	std::vector<Operation> back;
	int blend;
	char err[ 64 ];
	CHECK( !ReadOpsText( many.data(), (int)many.size(), back, blend, err, sizeof( err ) ) );
	CHECK( back.size() == (size_t)OPS_MAX );
	if ( !back.empty() )
	{
		CHECK( back[0].levels[0].inBlack == 0 && back[0].levels[0].inWhite == 255 && back[0].levels[0].gamma == 1.0f );
		CHECK( back[0].curves[0].count == 2 );	// curve points that don't rise are dropped
	}
	for ( int i = 0; i < 3000; i++ )
	{
		std::string junk = text;
		for ( int j = 0; j < 8; j++ )
			junk[ Rand() % junk.size() ] = (char)Rand();
		ReadOpsText( junk.data(), (int)junk.size(), back, blend, err, sizeof( err ) );
		for ( const Operation &op : back )
		{
			CHECK( op.type >= 0 && op.type < OP_COUNT );
			CHECK( op.blend >= 0 && op.blend <= 255 );
		}
	}
	CHECK( !ReadOpsText( "\"Other\" { }", 11, back, blend, err, sizeof( err ) ) );
}

int main( int argc, char **argv )
{
	const int iterations = argc > 1 ? atoi( argv[1] ) : 20000;
	TestNeutral();
	TestKnown();
	TestRawRoundTrip();
	TestCurveMath();
	TestNames();
	TestOpsFile();
	Fuzz( iterations );
	if ( s_iFailed )
	{
		printf( "colorcorrection ops: %d check(s) FAILED\n", s_iFailed );
		return 1;
	}
	printf( "colorcorrection ops: all checks passed (fuzz %d)\n", iterations / 20 );
	return 0;
}
