//=========================================================
// SBO 2.0: point_worldtext, client side without the engine: see worldtext_core.h. Everything here takes its input
// from the network or from a file and must hold against any bytes (tests/worldtext/run_test.sh).
//=========================================================

#include "worldtext_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

//---------------------------------------------------------
// Font metrics
//---------------------------------------------------------

const WTGlyph *WTFontMetrics::Find( int codepoint ) const
{
	int i = -1;

	if ( codepoint >= 0 && codepoint < 256 )
	{
		i = latin[codepoint];
	}
	else
	{
		int lo = 0, hi = (int)glyphs.size() - 1;
		while ( lo <= hi )
		{
			int mid = ( lo + hi ) / 2;
			if ( glyphs[mid].codepoint == codepoint )
			{
				i = mid;
				break;
			}
			if ( glyphs[mid].codepoint < codepoint )
				lo = mid + 1;
			else
				hi = mid - 1;
		}
	}

	if ( i < 0 )
		i = fallback;

	return i >= 0 ? &glyphs[i] : NULL;
}

// a whole decimal integer (optional sign), at most 9 digits
static bool ParseInt( const char *tok, int &out )
{
	const char *p = tok;
	bool neg = false;
	if ( *p == '-' || *p == '+' )
		neg = ( *p++ == '-' );
	if ( !*p )
		return false;
	long v = 0;
	int digits = 0;
	for ( ; *p; p++ )
	{
		if ( *p < '0' || *p > '9' || ++digits > 9 )
			return false;
		v = v * 10 + ( *p - '0' );
	}
	out = (int)( neg ? -v : v );
	return true;
}

static bool ParseFloat( const char *tok, float &out )
{
	char *end = NULL;
	double d = strtod( tok, &end );
	if ( end == tok || *end || !( d > -1.0e6 && d < 1.0e6 ) )
		return false;
	out = (float)d;
	return true;
}

#define WT_MAX_TOKENS	12
#define WT_MAX_TOKEN	32

bool WT_ParseFontMetrics( const char *data, int len, WTFontMetrics &out, std::string &err )
{
	out = WTFontMetrics();
	out.em = 0;
	out.ascent = out.descent = 0;
	out.spacingUnit = 0;
	out.frameW = out.frameH = 0;
	out.frames = 0;
	out.fallback = -1;
	for ( int i = 0; i < 256; i++ )
		out.latin[i] = -1;

	if ( !data || len <= 0 )
	{
		err = "empty file";
		return false;
	}

	int version = 0;
	int pos = 0;
	while ( pos < len )
	{
		// one line, tokens split on blanks, "//" starts a comment
		char tokens[WT_MAX_TOKENS][WT_MAX_TOKEN + 1];
		int ntok = 0;
		bool comment = false;
		while ( pos < len && data[pos] != '\n' )
		{
			char c = data[pos];
			if ( comment || c == ' ' || c == '\t' || c == '\r' || c == '\0' )
			{
				pos++;
				continue;
			}
			if ( c == '/' && pos + 1 < len && data[pos + 1] == '/' )
			{
				comment = true;
				continue;
			}
			// a token
			int n = 0;
			bool tooLong = false;
			while ( pos < len && data[pos] != '\n' && data[pos] != ' ' && data[pos] != '\t' && data[pos] != '\r'
				&& data[pos] != '\0' && !( data[pos] == '/' && pos + 1 < len && data[pos + 1] == '/' ) )
			{
				if ( n < WT_MAX_TOKEN )
				{
					if ( ntok < WT_MAX_TOKENS )
						tokens[ntok][n] = data[pos];
					n++;
				}
				else
					tooLong = true;
				pos++;
			}
			if ( ntok < WT_MAX_TOKENS )
			{
				tokens[ntok][n] = '\0';
				if ( tooLong )
					tokens[ntok][0] = '\0';	// never a valid value
				ntok++;
			}
		}
		pos++;	// the '\n'

		if ( !ntok )
			continue;

		const char *key = tokens[0];
		if ( !strcmp( key, "glyph" ) )
		{
			int v[8];
			float adv;
			if ( ntok < 10 )
				continue;
			bool ok = true;
			for ( int i = 0; i < 8 && ok; i++ )
				ok = ParseInt( tokens[1 + i], v[i] );
			if ( !ok || !ParseFloat( tokens[9], adv ) )
				continue;
			if ( (int)out.glyphs.size() >= WT_FONT_MAX_GLYPHS )
				continue;
			WTGlyph g;
			g.codepoint = v[0];
			g.frame = v[1];
			g.x = v[2];
			g.y = v[3];
			g.w = v[4];
			g.h = v[5];
			g.left = v[6];
			g.top = v[7];
			g.advance = adv;
			out.glyphs.push_back( g );
		}
		else if ( ntok >= 2 )
		{
			int a = 0, b = 0;
			if ( !strcmp( key, "worldtext" ) )
				ParseInt( tokens[1], version );
			else if ( !strcmp( key, "name" ) )
				out.name = tokens[1];
			else if ( !strcmp( key, "em" ) && ParseInt( tokens[1], a ) )
				out.em = a;
			else if ( !strcmp( key, "ascent" ) && ParseInt( tokens[1], a ) )
				out.ascent = a;
			else if ( !strcmp( key, "descent" ) && ParseInt( tokens[1], a ) )
				out.descent = a;
			else if ( !strcmp( key, "spacingunit" ) && ParseInt( tokens[1], a ) )
				out.spacingUnit = a;
			else if ( !strcmp( key, "frames" ) && ParseInt( tokens[1], a ) )
				out.frames = a;
			else if ( !strcmp( key, "frame" ) && ntok >= 3 && ParseInt( tokens[1], a ) && ParseInt( tokens[2], b ) )
			{
				out.frameW = a;
				out.frameH = b;
			}
		}
	}

	if ( version != 1 )
	{
		err = "not a version 1 world text font";
		return false;
	}
	if ( out.em < 4 || out.em > 1024 )
	{
		err = "em out of range";
		return false;
	}
	if ( out.frameW < 1 || out.frameW > 1024 || out.frameH < 1 || out.frameH > 1024 )
	{
		err = "frame size out of range";
		return false;
	}
	if ( out.frames < 1 || out.frames > WT_FONT_MAX_FRAMES )
	{
		err = "frame count out of range";
		return false;
	}
	if ( out.spacingUnit < 1 || out.spacingUnit > 4096 )
		out.spacingUnit = out.em;

	// glyphs that don't fit their frame, or are out of range, are dropped
	std::vector<WTGlyph> keep;
	for ( size_t i = 0; i < out.glyphs.size(); i++ )
	{
		const WTGlyph &g = out.glyphs[i];
		if ( g.codepoint < 0 || g.codepoint > 0x10FFFF || g.frame < 0 || g.frame >= out.frames )
			continue;
		if ( g.w < 0 || g.h < 0 || g.x < 0 || g.y < 0 || g.x > out.frameW - g.w || g.y > out.frameH - g.h )
			continue;
		if ( g.left < -4096 || g.left > 4096 || g.top < -4096 || g.top > 4096 )
			continue;
		if ( !( g.advance >= -4096.0f && g.advance <= 4096.0f ) )
			continue;
		keep.push_back( g );
	}

	// sorted, one glyph per code point (the first)
	std::stable_sort( keep.begin(), keep.end(), []( const WTGlyph &a, const WTGlyph &b ) { return a.codepoint < b.codepoint; } );
	out.glyphs.clear();
	for ( size_t i = 0; i < keep.size(); i++ )
	{
		if ( !out.glyphs.empty() && out.glyphs.back().codepoint == keep[i].codepoint )
			continue;
		out.glyphs.push_back( keep[i] );
	}

	if ( out.glyphs.empty() )
	{
		err = "no glyphs";
		return false;
	}

	for ( size_t i = 0; i < out.glyphs.size(); i++ )
	{
		if ( out.glyphs[i].codepoint < 256 )
			out.latin[out.glyphs[i].codepoint] = (int)i;
	}
	out.fallback = out.latin['?'];
	return true;
}

//---------------------------------------------------------
// Text decoding
//---------------------------------------------------------

// Windows-1252 0x80 .. 0x9F (0 = undefined)
static const unsigned short s_cp1252[32] =
{
	0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
	0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178
};

void WT_DecodeText( const unsigned char *s, int len, std::vector<int> &out, int maxOut )
{
	out.clear();
	if ( !s )
		return;

	int i = 0;
	while ( i < len && (int)out.size() < maxOut )
	{
		unsigned c = s[i];
		if ( c < 0x80 )
		{
			out.push_back( (int)c );
			i++;
			continue;
		}

		// a UTF-8 sequence?
		int n = ( c & 0xE0 ) == 0xC0 ? 1 : ( ( c & 0xF0 ) == 0xE0 ? 2 : ( ( c & 0xF8 ) == 0xF0 ? 3 : 0 ) );
		if ( n && i + n < len )
		{
			unsigned cp = c & ( 0x3F >> n );
			bool ok = true;
			for ( int k = 1; ok && k <= n; k++ )
			{
				unsigned cc = s[i + k];
				if ( ( cc & 0xC0 ) != 0x80 )
					ok = false;
				else
					cp = ( cp << 6 ) | ( cc & 0x3F );
			}
			// no overlong forms, surrogates or code points past Unicode
			static const unsigned minimum[4] = { 0, 0x80, 0x800, 0x10000 };
			if ( ok && cp >= minimum[n] && cp <= 0x10FFFF && !( cp >= 0xD800 && cp <= 0xDFFF ) )
			{
				out.push_back( (int)cp );
				i += n + 1;
				continue;
			}
		}

		// one Windows-1252 byte
		if ( c >= 0xA0 )
			out.push_back( (int)c );
		else
			out.push_back( s_cp1252[c - 0x80] ? (int)s_cp1252[c - 0x80] : '?' );
		i++;
	}
}

//---------------------------------------------------------
// Layout
//---------------------------------------------------------

// not std::min / std::max: windows.h's min and max macros may be defined
static inline float WTMin( float a, float b ) { return a < b ? a : b; }
static inline float WTMax( float a, float b ) { return a > b ? a : b; }

static float SaneFloat( float f, float lo, float hi )
{
	if ( !( f >= lo ) )
		return f > hi ? hi : lo;	// NaN: lo
	return f > hi ? hi : f;
}

void WT_Layout( const WTFontMetrics &font, const std::vector<int> &text, float spacingX, float spacingY, int flags,
	bool billboard, WTLayout &out )
{
	out.quads.clear();
	out.lines = 0;
	out.minX = out.maxX = out.minY = out.maxY = 0.0f;
	for ( int f = 0; f <= WT_FONT_MAX_FRAMES; f++ )
		out.frameStart[f] = 0;

	if ( font.em <= 0 || font.frameW <= 0 || font.frameH <= 0 )
		return;

	// Source: spacing in the font's pixels (1/spacingUnit of the text size here)
	const float unit = (float)font.em / (float)( font.spacingUnit > 0 ? font.spacingUnit : font.em );
	const float stepX = SaneFloat( spacingX, -4096.0f, 4096.0f ) * unit;
	const float lineStep = (float)font.em + SaneFloat( spacingY, -4096.0f, 4096.0f ) * unit;

	const WTGlyph *pSpace = font.Find( ' ' );
	const float spaceAdvance = ( pSpace && pSpace->codepoint == ' ' ) ? pSpace->advance : font.em * 0.3f;

	std::vector<WTQuad> quads;
	std::vector<float> lineWidth;
	std::vector<int> lineOfQuad;
	quads.reserve( text.size() );

	float pen = 0.0f, baseline = 0.0f, width = 0.0f;
	int line = 0;

	for ( size_t i = 0; i < text.size(); i++ )
	{
		const int cp = text[i];
		if ( cp == '\n' )
		{
			lineWidth.push_back( width );
			pen = width = 0.0f;
			baseline -= lineStep;
			line++;
			continue;
		}
		if ( cp == '\t' )
		{
			pen += 4.0f * ( spaceAdvance + stepX );
			width = pen;
			continue;
		}
		if ( cp < 0x20 || ( cp >= 0x7F && cp < 0xA0 ) )
			continue;	// control characters

		const WTGlyph *g = font.Find( cp );
		if ( !g )
			continue;

		if ( g->w > 0 && g->h > 0 )
		{
			WTQuad q;
			q.x0 = pen + g->left;
			q.x1 = q.x0 + g->w;
			q.y0 = baseline + g->top;
			q.y1 = q.y0 - g->h;
			q.s0 = (float)g->x / font.frameW;
			q.t0 = (float)g->y / font.frameH;
			q.s1 = (float)( g->x + g->w ) / font.frameW;
			q.t1 = (float)( g->y + g->h ) / font.frameH;
			q.frame = (short)g->frame;
			q.index = (short)( i < 32767 ? i : 32767 );
			quads.push_back( q );
			lineOfQuad.push_back( line );
		}

		pen += g->advance + stepX;
		width = pen;
	}
	lineWidth.push_back( width );
	out.lines = line + 1;

	// centering: each line on the origin (spawnflag), or the whole block (Source's facing orientations)
	float blockWidth = 0.0f;
	for ( size_t l = 0; l < lineWidth.size(); l++ )
		blockWidth = WTMax( blockWidth, lineWidth[l] );

	for ( size_t k = 0; k < quads.size(); k++ )
	{
		float shift = 0.0f;
		if ( flags & WORLDTEXT_FLAG_CENTER )
			shift = -0.5f * lineWidth[lineOfQuad[k]];
		else if ( billboard )
			shift = -0.5f * blockWidth;
		quads[k].x0 += shift;
		quads[k].x1 += shift;
	}

	// bounds
	for ( size_t k = 0; k < quads.size(); k++ )
	{
		const WTQuad &q = quads[k];
		if ( !k )
		{
			out.minX = q.x0;
			out.maxX = q.x1;
			out.minY = q.y1;
			out.maxY = q.y0;
		}
		out.minX = WTMin( out.minX, q.x0 );
		out.maxX = WTMax( out.maxX, q.x1 );
		out.minY = WTMin( out.minY, q.y1 );
		out.maxY = WTMax( out.maxY, q.y0 );
	}

	// sorted by frame (counting sort, stable)
	int count[WT_FONT_MAX_FRAMES + 1] = { 0 };
	for ( size_t k = 0; k < quads.size(); k++ )
		count[quads[k].frame + 1]++;
	for ( int f = 0; f < WT_FONT_MAX_FRAMES; f++ )
		count[f + 1] += count[f];
	for ( int f = 0; f <= WT_FONT_MAX_FRAMES; f++ )
		out.frameStart[f] = count[f];
	out.quads.resize( quads.size() );
	for ( size_t k = 0; k < quads.size(); k++ )
		out.quads[count[quads[k].frame]++] = quads[k];
}

//---------------------------------------------------------
// Messages
//---------------------------------------------------------

namespace
{
struct Reader
{
	const unsigned char *p;
	int size, pos;
	bool bad;

	Reader( const void *buf, int n ) : p( (const unsigned char *)buf ), size( buf ? n : 0 ), pos( 0 ), bad( false ) {}

	int Byte( void )
	{
		if ( pos + 1 > size )
		{
			bad = true;
			return 0;
		}
		return p[pos++];
	}

	int Short( void )
	{
		int lo = Byte();
		int hi = Byte();
		return (short)( lo | ( hi << 8 ) );
	}

	float LongFloat( void )
	{
		unsigned v = 0;
		for ( int i = 0; i < 4; i++ )
			v |= (unsigned)Byte() << ( 8 * i );
		float f;
		memcpy( &f, &v, sizeof( f ) );
		return f;
	}

	// bytes up to the terminator (or the end of the message), at most max - 1 kept
	void String( std::string &out, int max )
	{
		out.clear();
		while ( pos < size )
		{
			unsigned char c = p[pos++];
			if ( !c )
				return;
			if ( (int)out.size() < max - 1 )
				out.push_back( (char)c );
		}
	}
};
}

static float Finite( float f, float lo, float hi )
{
	if ( !( f >= lo && f <= hi ) )
		return 0.0f;	// NaN, infinity, out of range
	return f;
}

bool WT_ParseMain( const void *buf, int size, WTState &state )
{
	Reader r( buf, size );

	state.flags = r.Byte();
	state.id = r.Short();
	state.text.clear();
	state.parent = 0;
	if ( r.bad )
		return false;

	if ( !( state.flags & WORLDTEXT_FLAG_ON ) )
		return true;

	for ( int i = 0; i < 3; i++ )
		state.origin[i] = Finite( r.LongFloat(), -131072.0f, 131072.0f );
	for ( int i = 0; i < 3; i++ )
		state.angles[i] = ( r.Short() & 0xFFFF ) * ( 360.0f / 65536.0f );
	state.size = Finite( r.LongFloat(), 0.0f, 4096.0f );
	state.spacingX = Finite( r.LongFloat(), -4096.0f, 4096.0f );
	state.spacingY = Finite( r.LongFloat(), -4096.0f, 4096.0f );
	for ( int i = 0; i < 4; i++ )
		state.color[i] = (unsigned char)r.Byte();
	state.orientation = r.Byte();
	state.font = r.Byte();
	if ( r.bad )
		return false;

	if ( state.orientation > WORLDTEXT_ORIENT_UPRIGHT )
		state.orientation = WORLDTEXT_ORIENT_FIXED;
	if ( state.font >= WORLDTEXT_FONT_COUNT )
		state.font = 0;

	if ( state.flags & WORLDTEXT_FLAG_PARENT )
	{
		state.parent = r.Short() & 0xFFFF;
		for ( int i = 0; i < 3; i++ )
			state.localOrigin[i] = Finite( r.LongFloat(), -131072.0f, 131072.0f );
		for ( int i = 0; i < 3; i++ )
			state.localAngles[i] = ( r.Short() & 0xFFFF ) * ( 360.0f / 65536.0f );
		if ( r.bad )
			return false;
	}

	r.String( state.text, WORLDTEXT_MAX_TEXT );
	return true;
}

bool WT_ParsePart( const void *buf, int size, int &id, std::string &piece )
{
	Reader r( buf, size );
	id = r.Short();
	if ( r.bad )
		return false;
	r.String( piece, WORLDTEXT_MAX_TEXT );
	return true;
}

bool WTStore::OnPart( const void *buf, int size )
{
	int id;
	std::string piece;
	if ( !WT_ParsePart( buf, size, id, piece ) )
		return false;

	if ( id != m_iPendingId )
	{
		m_iPendingId = id;
		m_Pending.clear();
	}

	const size_t room = WORLDTEXT_MAX_TEXT - 1;
	if ( m_Pending.size() < room )
		m_Pending.append( piece, 0, room - m_Pending.size() );
	return true;
}

bool WTStore::OnMain( const void *buf, int size, WTState &state, bool &on )
{
	on = false;
	if ( !WT_ParseMain( buf, size, state ) )
	{
		Clear();
		return false;
	}

	on = ( state.flags & WORLDTEXT_FLAG_ON ) != 0;

	if ( on && m_iPendingId == state.id && !m_Pending.empty() )
	{
		std::string text = m_Pending;
		const size_t room = WORLDTEXT_MAX_TEXT - 1;
		if ( text.size() < room )
			text.append( state.text, 0, room - text.size() );
		state.text.swap( text );
	}

	// the pieces belong to this message (or to none)
	Clear();
	return true;
}
