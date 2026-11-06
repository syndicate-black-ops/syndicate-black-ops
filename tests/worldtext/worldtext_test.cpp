// point_worldtext client parser and layout test (tests/worldtext/run_test.sh): src/cl_dll/worldtext_core.cpp alone,
// with AddressSanitizer and UndefinedBehaviorSanitizer. Feeds it the shipped font metrics, round trips of the
// server's messages (long texts in pieces), and hostile input: truncated and random messages, missing and foreign
// pieces, hostile lengths, every kind of bad UTF-8, broken metrics files. Exit 1 on a failed check (a sanitizer
// error aborts).

#include "worldtext_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static int s_iFailures = 0;
static int s_iChecks = 0;

#define CHECK( cond ) do { s_iChecks++; if ( !( cond ) ) { s_iFailures++; \
	fprintf( stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond ); } } while ( 0 )

// deterministic random numbers
static unsigned s_uSeed = 12345;
static unsigned Rand( void )
{
	s_uSeed = s_uSeed * 1103515245u + 12345u;
	return ( s_uSeed >> 8 ) & 0xFFFFFF;
}

static std::string ReadFile( const std::string &path )
{
	FILE *f = fopen( path.c_str(), "rb" );
	if ( !f )
	{
		fprintf( stderr, "cannot read %s\n", path.c_str() );
		exit( 2 );
	}
	std::string s;
	char buf[4096];
	size_t n;
	while ( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 )
		s.append( buf, n );
	fclose( f );
	return s;
}

//---------------------------------------------------------
// The server's messages (dlls/point_worldtext.cpp SendTo, written again here without the engine)
//---------------------------------------------------------

struct Msg
{
	bool part;
	std::vector<unsigned char> data;
};

static void PutByte( std::vector<unsigned char> &d, int v ) { d.push_back( (unsigned char)v ); }
static void PutShort( std::vector<unsigned char> &d, int v ) { PutByte( d, v & 255 ); PutByte( d, ( v >> 8 ) & 255 ); }
static void PutLong( std::vector<unsigned char> &d, float f )
{
	unsigned v;
	memcpy( &v, &f, 4 );
	for ( int i = 0; i < 4; i++ )
		PutByte( d, ( v >> ( 8 * i ) ) & 255 );
}
static void PutString( std::vector<unsigned char> &d, const std::string &s )
{
	d.insert( d.end(), s.begin(), s.end() );
	PutByte( d, 0 );
}

static std::vector<Msg> ServerMessages( int id, const std::string &text, int flags = WORLDTEXT_FLAG_ON )
{
	std::vector<Msg> out;
	size_t pos = 0, left = text.size();
	while ( left > WORLDTEXT_MAIN_TEXT_BYTES )
	{
		size_t n = left - WORLDTEXT_MAIN_TEXT_BYTES;
		if ( n > WORLDTEXT_PART_BYTES )
			n = WORLDTEXT_PART_BYTES;
		Msg m;
		m.part = true;
		PutShort( m.data, id );
		PutString( m.data, text.substr( pos, n ) );
		CHECK( m.data.size() <= 192 );
		out.push_back( m );
		pos += n;
		left -= n;
	}
	Msg m;
	m.part = false;
	PutByte( m.data, flags );
	PutShort( m.data, id );
	if ( flags & WORLDTEXT_FLAG_ON )
	{
		PutLong( m.data, 100.5f );
		PutLong( m.data, -200.25f );
		PutLong( m.data, 64.0f );
		PutShort( m.data, 0 );
		PutShort( m.data, 16384 );
		PutShort( m.data, 0 );
		PutLong( m.data, 12.0f );
		PutLong( m.data, 0.0f );
		PutLong( m.data, 0.0f );
		for ( int i = 0; i < 4; i++ )
			PutByte( m.data, 200 );
		PutByte( m.data, 1 );
		PutByte( m.data, 8 );
		CHECK( m.data.size() == 3 + WORLDTEXT_HEADER_BYTES - 3 );
		PutString( m.data, text.substr( pos ) );
	}
	CHECK( m.data.size() <= 192 );
	out.push_back( m );
	return out;
}

static bool Deliver( WTStore &store, const std::vector<Msg> &msgs, WTState &state, bool &on )
{
	bool ok = true;
	for ( size_t i = 0; i < msgs.size(); i++ )
	{
		if ( msgs[i].part )
			store.OnPart( msgs[i].data.data(), (int)msgs[i].data.size() );
		else
			ok = store.OnMain( msgs[i].data.data(), (int)msgs[i].data.size(), state, on );
	}
	return ok;
}

static std::string RandomText( size_t len )
{
	std::string s;
	for ( size_t i = 0; i < len; i++ )
		s.push_back( (char)( 1 + Rand() % 255 ) );	// no 0: a string can't hold one
	return s;
}

//---------------------------------------------------------
// Checks
//---------------------------------------------------------

static void CheckLayout( const WTFontMetrics &font, const WTLayout &l, size_t ncp )
{
	CHECK( l.quads.size() <= ncp );
	CHECK( l.frameStart[0] == 0 );
	for ( int f = 0; f < WT_FONT_MAX_FRAMES; f++ )
		CHECK( l.frameStart[f] <= l.frameStart[f + 1] );
	CHECK( l.frameStart[WT_FONT_MAX_FRAMES] == (int)l.quads.size() );
	for ( int f = 0; f < WT_FONT_MAX_FRAMES; f++ )
	{
		for ( int k = l.frameStart[f]; k < l.frameStart[f + 1]; k++ )
		{
			const WTQuad &q = l.quads[k];
			CHECK( q.frame == f && f < font.frames );
			CHECK( isfinite( q.x0 ) && isfinite( q.x1 ) && isfinite( q.y0 ) && isfinite( q.y1 ) );
			CHECK( q.s0 >= 0 && q.s1 <= 1 && q.t0 >= 0 && q.t1 <= 1 && q.s0 <= q.s1 && q.t0 <= q.t1 );
		}
	}
	CHECK( isfinite( l.minX ) && isfinite( l.maxX ) && isfinite( l.minY ) && isfinite( l.maxY ) );
}

static void TestFont( const std::string &path, WTFontMetrics &font )
{
	std::string data = ReadFile( path );
	std::string err;
	CHECK( WT_ParseFontMetrics( data.data(), (int)data.size(), font, err ) );
	CHECK( font.glyphs.size() >= 191 );
	CHECK( font.fallback >= 0 );
	CHECK( font.Find( 'A' ) && font.Find( 'A' )->codepoint == 'A' );
	CHECK( font.Find( 0x2192 ) && font.Find( 0x2192 )->codepoint == 0x2192 );		// an arrow
	CHECK( font.Find( 0x1F600 ) && font.Find( 0x1F600 )->codepoint == '?' );		// not in the font: '?'
	CHECK( font.Find( -5 ) && font.Find( 0x7FFFFFFF ) );
	printf( "font %s: %d glyphs, em %d, %d frames\n", font.name.c_str(), (int)font.glyphs.size(), font.em, font.frames );

	// every truncation of the file, and random damage: never a crash; a parsed font is always consistent
	for ( size_t n = 0; n <= data.size(); n += ( n < 600 ? 1 : 37 ) )
	{
		WTFontMetrics f;
		if ( WT_ParseFontMetrics( data.data(), (int)n, f, err ) )
			CHECK( f.frames >= 1 && f.frames <= WT_FONT_MAX_FRAMES && !f.glyphs.empty() );
	}
	for ( int iter = 0; iter < 3000; iter++ )
	{
		std::string d = data;
		const int edits = 1 + Rand() % 20;
		for ( int e = 0; e < edits; e++ )
		{
			size_t at = Rand() % d.size();
			switch ( Rand() % 4 )
			{
			case 0: d[at] = (char)( Rand() & 255 ); break;
			case 1: d.insert( at, std::string( 1 + Rand() % 300, (char)( '0' + Rand() % 10 ) ) ); break;
			case 2: d.erase( at, Rand() % 50 ); break;
			default: d.insert( at, " -999999999 99999999999 1e300 nan inf // x " ); break;
			}
			if ( d.empty() )
				d = "x";
		}
		WTFontMetrics f;
		if ( WT_ParseFontMetrics( d.data(), (int)d.size(), f, err ) )
		{
			CHECK( f.em >= 4 && f.em <= 1024 && f.frames >= 1 && f.frames <= WT_FONT_MAX_FRAMES );
			for ( size_t g = 0; g < f.glyphs.size(); g++ )
			{
				const WTGlyph &gl = f.glyphs[g];
				CHECK( gl.frame >= 0 && gl.frame < f.frames && gl.x + gl.w <= f.frameW && gl.y + gl.h <= f.frameH );
			}
			// and it lays out
			std::vector<int> cps;
			const char *t = "Hello\nworld \xE2\x86\x92 \x80 test";
			WT_DecodeText( (const unsigned char *)t, (int)strlen( t ), cps, WORLDTEXT_MAX_TEXT );
			WTLayout l;
			WT_Layout( f, cps, 3, -2, WORLDTEXT_FLAG_CENTER, false, l );
			CheckLayout( f, l, cps.size() );
		}
	}

	// hostile files
	const char *bad[] = {
		"", "worldtext 1", "worldtext 1\nem 64\nframe 256 256\nframes 1\n",
		"worldtext 1\nem 64\nframe 256 256\nframes 99\nglyph 63 0 0 0 10 10 0 10 5\n",
		"worldtext 1\nem 0\nframe 256 256\nframes 1\nglyph 63 0 0 0 10 10 0 10 5\n",
		"worldtext 1\nem 64\nframe 256 256\nframes 1\nglyph 63 0 250 250 10 10 0 10 5\n",
		"worldtext 1\nem 64\nframe 256 256\nframes 1\nglyph 63 0 -5 0 10 10 0 10 5\n",
		"worldtext 1\nem 64\nframe 256 256\nframes 1\nglyph 63 1 0 0 10 10 0 10 5\n",
		"worldtext 1\nem 64\nframe 256 256\nframes 1\nglyph 63 0 0 0 10 10 0 10 nan\n",
		"worldtext 2\nem 64\nframe 256 256\nframes 1\nglyph 63 0 0 0 10 10 0 10 5\n",
	};
	for ( size_t i = 0; i < sizeof( bad ) / sizeof( bad[0] ); i++ )
	{
		WTFontMetrics f;
		CHECK( !WT_ParseFontMetrics( bad[i], (int)strlen( bad[i] ), f, err ) );
	}
	{
		// long lines, many tokens, NULs inside, no final newline
		std::string s = "worldtext 1\nem 64\nframe 256 256\nframes 1\n";
		s += std::string( 5000, 'a' ) + "\n" + std::string( 300, ' ' );
		for ( int i = 0; i < 100; i++ )
			s += "glyph ";
		s += "\nglyph 63 0 0 0 10 10 0 10 5 extra tokens here and there and more and more\n";
		s += std::string( "glyph 65 0 0 0 10 10 0 10 5" ) + '\0' + "junk\nglyph 66 0 0 0 1 1 0 1 1";
		WTFontMetrics f;
		CHECK( WT_ParseFontMetrics( s.data(), (int)s.size(), f, err ) );
		CHECK( f.Find( 'B' ) && f.Find( 'B' )->codepoint == 'B' );
		CHECK( WT_ParseFontMetrics( NULL, 10, f, err ) == false );
		CHECK( WT_ParseFontMetrics( s.data(), -1, f, err ) == false );
	}
}

static void TestDecode( void )
{
	std::vector<int> cps;
	struct { const char *in; std::vector<int> out; } cases[] = {
		{ "A", { 'A' } },
		{ "\xC3\xA9", { 0xE9 } },						// é (UTF-8)
		{ "\xE9", { 0xE9 } },							// é (Windows-1252)
		{ "\x80", { 0x20AC } },							// € (Windows-1252)
		{ "\x81", { '?' } },							// undefined in Windows-1252
		{ "\xE2\x86\x92", { 0x2192 } },					// →
		{ "\xF0\x9F\x98\x80", { 0x1F600 } },			// 4 bytes
		{ "\xC0\xAF", { 0xC0, 0xAF } },					// overlong: two Latin-1 characters
		{ "\xED\xA0\x80", { 0xED, '?' , '?' } },		// a surrogate: not UTF-8 (0xA0 is Latin-1 though)
		{ "\xE2\x86", { 0xE2, 0x2020 } },				// cut short at the end
		{ "\xF4\x90\x80\x80", { 0xF4, '?', 0x20AC, 0x20AC } },	// past U+10FFFF
	};
	// fix the surrogate case: 0xA0 and 0x80 are Windows-1252 / Latin-1
	cases[8].out = { 0xED, 0xA0, 0x20AC };
	for ( size_t i = 0; i < sizeof( cases ) / sizeof( cases[0] ); i++ )
	{
		WT_DecodeText( (const unsigned char *)cases[i].in, (int)strlen( cases[i].in ), cps, 100 );
		if ( cps != cases[i].out )
		{
			fprintf( stderr, "decode case %d:", (int)i );
			for ( size_t k = 0; k < cps.size(); k++ )
				fprintf( stderr, " %X", cps[k] );
			fprintf( stderr, "\n" );
		}
		CHECK( cps == cases[i].out );
	}

	// the cap, and random bytes
	std::string big( 5000, 'x' );
	WT_DecodeText( (const unsigned char *)big.data(), (int)big.size(), cps, WORLDTEXT_MAX_TEXT );
	CHECK( cps.size() == WORLDTEXT_MAX_TEXT );
	WT_DecodeText( NULL, 10, cps, 10 );
	CHECK( cps.empty() );
	for ( int iter = 0; iter < 20000; iter++ )
	{
		std::string s;
		const int n = Rand() % 40;
		for ( int k = 0; k < n; k++ )
			s.push_back( (char)( ( Rand() % 3 ) ? ( 0x80 + Rand() % 128 ) : Rand() % 256 ) );
		WT_DecodeText( (const unsigned char *)s.data(), (int)s.size(), cps, 30 );
		CHECK( cps.size() <= 30 && cps.size() <= s.size() );
		for ( size_t k = 0; k < cps.size(); k++ )
			CHECK( cps[k] >= 0 && cps[k] <= 0x10FFFF && !( cps[k] >= 0xD800 && cps[k] <= 0xDFFF ) );
	}
}

static void TestLayout( const WTFontMetrics &font )
{
	std::vector<int> cps;
	WTLayout l;

	// one line: the pen moves by the advances; centered lines are symmetric about the origin
	const char *t = "Door";
	WT_DecodeText( (const unsigned char *)t, 4, cps, 100 );
	WT_Layout( font, cps, 0, 0, 0, false, l );
	CheckLayout( font, l, cps.size() );
	CHECK( l.quads.size() == 4 && l.lines == 1 );
	WTLayout c;
	WT_Layout( font, cps, 0, 0, WORLDTEXT_FLAG_CENTER, false, c );
	CHECK( fabsf( ( c.minX + c.maxX ) - ( l.minX + l.maxX - ( l.maxX - l.minX ) ) ) < font.em );

	// lines go down by em (+ spacing in 1/spacingunit of the size)
	t = "A\nA\n\nA";
	WT_DecodeText( (const unsigned char *)t, (int)strlen( t ), cps, 100 );
	WT_Layout( font, cps, 0, 0, 0, false, l );
	CHECK( l.lines == 4 && l.quads.size() == 3 );
	float ys[3];
	int n = 0;
	for ( size_t k = 0; k < l.quads.size(); k++ )
		ys[n++] = l.quads[k].y0;
	// sorted by frame: the same glyph, same frame, in text order
	CHECK( fabsf( ( ys[0] - ys[1] ) - font.em ) < 0.01f && fabsf( ( ys[1] - ys[2] ) - 2 * font.em ) < 0.01f );
	WT_Layout( font, cps, 0, (float)font.spacingUnit, 0, false, l );		// spacing = one text size more
	CHECK( fabsf( ( l.quads[0].y0 - l.quads[1].y0 ) - 2 * font.em ) < 0.01f );

	// hostile values: NaN, infinities, huge spacing, control characters, the longest text
	const float spacings[] = { NAN, INFINITY, -INFINITY, 1e30f, -1e30f, 0, -4096, 4096 };
	for ( size_t a = 0; a < 8; a++ )
	{
		for ( size_t b = 0; b < 8; b++ )
		{
			std::string s = RandomText( 1023 );
			WT_DecodeText( (const unsigned char *)s.data(), (int)s.size(), cps, WORLDTEXT_MAX_TEXT );
			WT_Layout( font, cps, spacings[a], spacings[b], ( a & 1 ) ? WORLDTEXT_FLAG_CENTER : 0, ( b & 1 ) != 0, l );
			CheckLayout( font, l, cps.size() );
		}
	}
	std::vector<int> weird = { -1, 0, 0x7FFFFFFF, '\n', '\t', 0x85, 0xD800, 0x10FFFF, 0x110000, ' ', '?' };
	WT_Layout( font, weird, 1, 1, 0, true, l );
	CheckLayout( font, l, weird.size() );
	std::vector<int> nl( WORLDTEXT_MAX_TEXT, '\n' );
	WT_Layout( font, nl, 0, 0, 0, false, l );
	CHECK( l.quads.empty() && l.lines == WORLDTEXT_MAX_TEXT + 1 );

	// an empty font structure lays out nothing
	WTFontMetrics empty;
	empty.em = 0;
	empty.frameW = empty.frameH = 0;
	empty.frames = 0;
	WT_Layout( empty, weird, 0, 0, 0, false, l );
	CHECK( l.quads.empty() );
}

static void TestMessages( void )
{
	WTStore store;
	WTState state;
	bool on;

	// round trips of every length the server can send
	for ( int len = 0; len < WORLDTEXT_MAX_TEXT; len += ( len < 400 ? 1 : 7 ) )
	{
		std::string text = RandomText( len );
		std::vector<Msg> msgs = ServerMessages( 42, text );
		CHECK( Deliver( store, msgs, state, on ) );
		CHECK( on && state.id == 42 && state.text == text && state.font == 8 && state.orientation == 1 );
		CHECK( state.origin[0] == 100.5f && state.origin[1] == -200.25f && fabsf( state.angles[1] - 90.0f ) < 0.01f );
	}

	// off
	std::vector<Msg> off = ServerMessages( 42, "", 0 );
	CHECK( Deliver( store, off, state, on ) && !on && state.id == 42 );

	// a missing piece: the text is shorter, nothing breaks
	std::string text = RandomText( 900 );
	std::vector<Msg> msgs = ServerMessages( 7, text );
	CHECK( msgs.size() == 6 );
	msgs.erase( msgs.begin() + 2 );
	CHECK( Deliver( store, msgs, state, on ) && on && state.text.size() == 900 - WORLDTEXT_PART_BYTES );

	// pieces of another text in between: dropped, this text gets only its own pieces after them
	msgs = ServerMessages( 7, text );
	std::vector<Msg> other = ServerMessages( 8, RandomText( 500 ) );
	msgs.insert( msgs.begin() + 3, other[0] );
	CHECK( Deliver( store, msgs, state, on ) && on && state.id == 7 && state.text.size() < text.size() );

	// pieces with no main message, then a main message of another id: not glued on
	other = ServerMessages( 9, RandomText( 600 ) );
	other.pop_back();
	for ( size_t i = 0; i < other.size(); i++ )
		store.OnPart( other[i].data.data(), (int)other[i].data.size() );
	msgs = ServerMessages( 10, "short" );
	CHECK( Deliver( store, msgs, state, on ) && state.text == "short" );

	// endless pieces: capped at the text limit
	Msg p = ServerMessages( 11, RandomText( 400 ) )[0];
	for ( int i = 0; i < 100; i++ )
		store.OnPart( p.data.data(), (int)p.data.size() );
	msgs = ServerMessages( 11, RandomText( 140 ) );
	CHECK( Deliver( store, msgs, state, on ) && state.text.size() == WORLDTEXT_MAX_TEXT - 1 );

	// every truncation of a main message: refused (or, past the header, a shorter text); never a crash
	Msg m = ServerMessages( 12, "Hello world" ).back();
	for ( size_t n = 0; n < m.data.size(); n++ )
	{
		bool ok = store.OnMain( m.data.data(), (int)n, state, on );
		CHECK( ok == ( n >= (size_t)WORLDTEXT_HEADER_BYTES ) );
	}
	CHECK( !store.OnMain( NULL, 50, state, on ) );
	CHECK( !store.OnMain( m.data.data(), -1, state, on ) );
	CHECK( !store.OnPart( m.data.data(), 1 ) );
	CHECK( store.OnPart( m.data.data(), 2 ) );

	// random messages
	for ( int iter = 0; iter < 50000; iter++ )
	{
		std::vector<unsigned char> d( Rand() % 260 );
		for ( size_t k = 0; k < d.size(); k++ )
			d[k] = (unsigned char)Rand();
		if ( d.size() > 0 && ( Rand() & 1 ) )
			d[0] |= WORLDTEXT_FLAG_ON;
		if ( Rand() % 3 == 0 )
			store.OnPart( d.data(), (int)d.size() );
		else if ( store.OnMain( d.data(), (int)d.size(), state, on ) && on )
		{
			CHECK( state.text.size() < WORLDTEXT_MAX_TEXT );
			CHECK( state.font >= 0 && state.font < WORLDTEXT_FONT_COUNT );
			CHECK( state.orientation >= 0 && state.orientation <= WORLDTEXT_ORIENT_UPRIGHT );
			CHECK( state.size >= 0 && state.size <= 4096 && isfinite( state.spacingX ) && isfinite( state.spacingY ) );
			for ( int c = 0; c < 3; c++ )
				CHECK( isfinite( state.origin[c] ) && state.angles[c] >= 0 && state.angles[c] < 360 );
		}
	}
}

int main( int argc, char **argv )
{
	if ( argc < 2 )
	{
		fprintf( stderr, "usage: %s <font.txt> ...\n", argv[0] );
		return 2;
	}

	TestDecode();
	TestMessages();
	for ( int i = 1; i < argc; i++ )
	{
		WTFontMetrics font;
		TestFont( argv[i], font );
		TestLayout( font );
	}

	printf( "%d checks, %d failed\n", s_iChecks, s_iFailures );
	return s_iFailures ? 1 : 0;
}
