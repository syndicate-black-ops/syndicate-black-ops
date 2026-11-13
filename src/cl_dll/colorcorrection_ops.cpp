//=========================================================
// SBO 2.0 color correction editor operations: see colorcorrection_ops.h.
//=========================================================

#include "colorcorrection_ops.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

namespace colorcorr
{
namespace ops
{
namespace
{
float Clamp01( float v )
{
	return v < 0.0f ? 0.0f : ( v > 1.0f ? 1.0f : v );
}

int ClampInt( int v, int lo, int hi )
{
	return v < lo ? lo : ( v > hi ? hi : v );
}

float Lerp( float a, float b, float t )
{
	return a + ( b - a ) * t;
}

float CurveAt( const Curve &c, float x )
{
	const int n = ClampInt( c.count, 1, sboctl::CURVE_MAX_POINTS );
	return Clamp01( sboctl::EvalMonotoneCurve( c.points, n, x ) );
}

float LevelsAt( const Levels &l, float x )
{
	const int inBlack = ClampInt( l.inBlack, 0, 254 );
	const int inWhite = ClampInt( l.inWhite, inBlack + 1, 255 );
	float v = Clamp01( ( x * 255.0f - inBlack ) / (float)( inWhite - inBlack ) );
	float gamma = l.gamma;
	if ( !( gamma >= 0.1f ) )	// also NaN
		gamma = 0.1f;
	if ( gamma > 9.99f )
		gamma = 9.99f;
	v = powf( v, 1.0f / gamma );
	const int outBlack = ClampInt( l.outBlack, 0, 255 ), outWhite = ClampInt( l.outWhite, 0, 255 );
	return Clamp01( ( outBlack + v * ( outWhite - outBlack ) ) / 255.0f );
}

// HSL lightness (color balance keeps it with Preserve Luminosity)
float Lightness( const float c[3] )
{
	float mx = c[0], mn = c[0];
	for ( int i = 1; i < 3; i++ )
	{
		mx = c[i] > mx ? c[i] : mx;
		mn = c[i] < mn ? c[i] : mn;
	}
	return 0.5f * ( mx + mn );
}

void RgbToHsl( const float c[3], float &h, float &s, float &l )
{
	float v;
	RgbToHsv( c, h, s, v );
	l = Lightness( c );
	float mx = c[0], mn = c[0];
	for ( int i = 1; i < 3; i++ )
	{
		mx = c[i] > mx ? c[i] : mx;
		mn = c[i] < mn ? c[i] : mn;
	}
	const float d = mx - mn;
	s = ( d <= 1e-6f ) ? 0.0f : d / ( 1.0f - fabsf( 2.0f * l - 1.0f ) + 1e-6f );
	s = Clamp01( s );
}

void HslToRgb( float h, float s, float l, float c[3] )
{
	const float chroma = ( 1.0f - fabsf( 2.0f * l - 1.0f ) ) * s;
	const float v = l + chroma * 0.5f;
	const float sv = v <= 1e-6f ? 0.0f : chroma / v;
	HsvToRgb( h, sv, v, c );
}

// GIMP's color balance transfer weights for one value 0 .. 1 (a = 0.25, b = 0.333, scale = 0.7)
void BalanceWeights( float v, float w[ RANGE_COUNT ] )
{
	const float a = 0.25f, b = 0.333f, scale = 0.7f;
	w[ RANGE_SHADOWS ] = Clamp01( ( v - b ) / -a + 0.5f ) * scale;
	w[ RANGE_MIDTONES ] = Clamp01( ( v - b ) / a + 0.5f ) * Clamp01( ( v + b - 1.0f ) / -a + 0.5f ) * scale;
	w[ RANGE_HIGHLIGHTS ] = Clamp01( ( v + b - 1.0f ) / a + 0.5f ) * scale;
}

void ApplyCurves( const Operation &op, float c[3] )
{
	for ( int i = 0; i < 3; i++ )
		c[i] = CurveAt( op.curves[ CH_RED + i ], CurveAt( op.curves[ CH_RGB ], c[i] ) );
}

void ApplyLevels( const Operation &op, float c[3] )
{
	for ( int i = 0; i < 3; i++ )
		c[i] = LevelsAt( op.levels[ CH_RED + i ], LevelsAt( op.levels[ CH_RGB ], c[i] ) );
}

void ApplySelectedHsv( const Operation &op, float c[3] )
{
	const float w = SelectionWeight( op, c );
	if ( w <= 0.0f )
		return;
	float h, s, v;
	RgbToHsv( c, h, s, v );
	if ( op.colorize )
	{
		h = (float)op.hue;
		s = Clamp01( 0.5f + ClampInt( op.saturation, -255, 255 ) / 510.0f );
	}
	else
	{
		h += (float)op.hue;
		s = Clamp01( s + ClampInt( op.saturation, -255, 255 ) / 255.0f );
	}
	h = fmodf( h, 360.0f );
	if ( h < 0.0f )
		h += 360.0f;
	v = Clamp01( v + ClampInt( op.value, -255, 255 ) / 255.0f );
	float out[3];
	HsvToRgb( h, s, v, out );
	for ( int i = 0; i < 3; i++ )
		c[i] = Lerp( c[i], out[i], w );
}

void ApplyBalance( const Operation &op, float c[3] )
{
	float h = 0, s = 0, l = 0;
	if ( op.preserveLuminosity )
		RgbToHsl( c, h, s, l );
	for ( int i = 0; i < 3; i++ )
	{
		float w[ RANGE_COUNT ];
		BalanceWeights( c[i], w );
		float add = 0.0f;
		for ( int r = 0; r < RANGE_COUNT; r++ )
			add += ClampInt( op.balance[r][i], -100, 100 ) / 100.0f * w[r];
		c[i] = Clamp01( c[i] + add );
	}
	if ( op.preserveLuminosity )
	{
		float h2, s2, l2;
		RgbToHsl( c, h2, s2, l2 );
		HslToRgb( h2, s2, l, c );
	}
}
} // namespace

const char *TypeName( int type )
{
	switch ( type )
	{
	case OP_CURVES:			return "Curves";
	case OP_LEVELS:			return "Levels";
	case OP_SELECTED_HSV:	return "Selected HSV";
	case OP_BALANCE:		return "Color Balance";
	case OP_LOOKUP:			return "Lookup";
	}
	return "?";
}

Operation Make( int type )
{
	Operation op;
	op.type = ( type >= 0 && type < OP_COUNT ) ? type : OP_CURVES;
	op.name = TypeName( op.type );
	return op;
}

void RgbToHsv( const float rgb[3], float &h, float &s, float &v )
{
	const float r = rgb[0], g = rgb[1], b = rgb[2];
	float mx = r > g ? r : g;
	mx = b > mx ? b : mx;
	float mn = r < g ? r : g;
	mn = b < mn ? b : mn;
	const float d = mx - mn;
	v = mx;
	s = mx > 1e-6f ? d / mx : 0.0f;
	if ( d <= 1e-6f )
		h = 0.0f;
	else if ( mx == r )
		h = 60.0f * fmodf( ( g - b ) / d, 6.0f );
	else if ( mx == g )
		h = 60.0f * ( ( b - r ) / d + 2.0f );
	else
		h = 60.0f * ( ( r - g ) / d + 4.0f );
	if ( h < 0.0f )
		h += 360.0f;
}

void HsvToRgb( float h, float s, float v, float rgb[3] )
{
	if ( !( h >= 0.0f && h < 360.0f ) )
	{
		h = fmodf( h, 360.0f );
		if ( h < 0.0f || !( h == h ) )
			h = ( h == h ) ? h + 360.0f : 0.0f;
	}
	s = Clamp01( s );
	v = Clamp01( v );
	const float c = v * s;
	const float x = c * ( 1.0f - fabsf( fmodf( h / 60.0f, 2.0f ) - 1.0f ) );
	const float m = v - c;
	float r = 0, g = 0, b = 0;
	const int sector = (int)( h / 60.0f ) % 6;
	switch ( sector )
	{
	case 0: r = c; g = x; break;
	case 1: r = x; g = c; break;
	case 2: g = c; b = x; break;
	case 3: g = x; b = c; break;
	case 4: r = x; b = c; break;
	default: r = c; b = x; break;
	}
	rgb[0] = r + m;
	rgb[1] = g + m;
	rgb[2] = b + m;
}

float SelectionWeight( const Operation &op, const float rgb[3] )
{
	float d;
	if ( op.selectMode == SELECT_HUE )
	{
		const float t[3] = { ClampInt( op.target[0], 0, 255 ) / 255.0f, ClampInt( op.target[1], 0, 255 ) / 255.0f,
			ClampInt( op.target[2], 0, 255 ) / 255.0f };
		float th, ts, tv, h, s, v;
		RgbToHsv( t, th, ts, tv );
		RgbToHsv( rgb, h, s, v );
		float dh = fabsf( h - th );
		if ( dh > 180.0f )
			dh = 360.0f - dh;
		d = dh * ( 255.0f / 180.0f );	// half way round the wheel = 255
		if ( s <= 1e-4f )
			d = 255.0f;					// a grey has no hue: never near one
	}
	else
	{
		const float dr = rgb[0] * 255.0f - ClampInt( op.target[0], 0, 255 );
		const float dg = rgb[1] * 255.0f - ClampInt( op.target[1], 0, 255 );
		const float db = rgb[2] * 255.0f - ClampInt( op.target[2], 0, 255 );
		d = sqrtf( dr * dr + dg * dg + db * db );
	}
	const float tol = (float)ClampInt( op.tolerance, 0, 255 ), fuzz = (float)ClampInt( op.fuzziness, 0, 255 );
	float w;
	if ( d <= tol )
		w = 1.0f;
	else if ( fuzz <= 0.0f || d >= tol + fuzz )
		w = 0.0f;
	else
		w = 1.0f - ( d - tol ) / fuzz;
	return op.invert ? 1.0f - w : w;
}

void SampleLut( const Lut &lut, const float in[3], float out[3] )
{
	const int n = lut.size;
	if ( n < 2 || lut.rgb.size() != (size_t)n * n * n * 3 )
	{
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
		return;
	}
	float f[3];
	int i0[3], i1[3];
	for ( int k = 0; k < 3; k++ )
	{
		const float p = Clamp01( in[k] ) * ( n - 1 );
		i0[k] = (int)p;
		if ( i0[k] > n - 2 )
			i0[k] = n - 2;
		i1[k] = i0[k] + 1;
		f[k] = p - i0[k];
	}
	for ( int ch = 0; ch < 3; ch++ )
	{
		float acc = 0.0f;
		for ( int corner = 0; corner < 8; corner++ )
		{
			const int r = ( corner & 1 ) ? i1[0] : i0[0];
			const int g = ( corner & 2 ) ? i1[1] : i0[1];
			const int b = ( corner & 4 ) ? i1[2] : i0[2];
			const float w = ( ( corner & 1 ) ? f[0] : 1.0f - f[0] ) * ( ( corner & 2 ) ? f[1] : 1.0f - f[1] ) *
				( ( corner & 4 ) ? f[2] : 1.0f - f[2] );
			acc += w * lut.rgb[ ( ( (size_t)b * n + g ) * n + r ) * 3 + ch ] / 255.0f;
		}
		out[ch] = acc;
	}
}

void ApplyOne( const Operation &op, float rgb[3] )
{
	if ( !op.enabled )
		return;
	const float t = ClampInt( op.blend, 0, 255 ) / 255.0f;
	if ( t <= 0.0f )
		return;
	float c[3] = { Clamp01( rgb[0] ), Clamp01( rgb[1] ), Clamp01( rgb[2] ) };
	switch ( op.type )
	{
	case OP_CURVES:			ApplyCurves( op, c ); break;
	case OP_LEVELS:			ApplyLevels( op, c ); break;
	case OP_SELECTED_HSV:	ApplySelectedHsv( op, c ); break;
	case OP_BALANCE:		ApplyBalance( op, c ); break;
	case OP_LOOKUP:
	{
		float in[3] = { c[0], c[1], c[2] };
		SampleLut( op.lut, in, c );
		break;
	}
	default:
		return;
	}
	for ( int i = 0; i < 3; i++ )
		rgb[i] = Clamp01( Lerp( Clamp01( rgb[i] ), c[i], t ) );
}

void ApplyAll( const std::vector<Operation> &ops, float rgb[3] )
{
	for ( const Operation &op : ops )
		ApplyOne( op, rgb );
}

void BuildLut( const std::vector<Operation> &ops, int size, Lut &out )
{
	if ( size < CUBE_MIN_SIZE )
		size = CUBE_MIN_SIZE;
	if ( size > CUBE_MAX_SIZE )
		size = CUBE_MAX_SIZE;
	out.size = size;
	out.rgb.resize( (size_t)size * size * size * 3 );
	size_t o = 0;
	for ( int b = 0; b < size; b++ )
		for ( int g = 0; g < size; g++ )
			for ( int r = 0; r < size; r++ )
			{
				float c[3] = { r / (float)( size - 1 ), g / (float)( size - 1 ), b / (float)( size - 1 ) };
				ApplyAll( ops, c );
				for ( int i = 0; i < 3; i++ )
					out.rgb[ o++ ] = (unsigned char)( Clamp01( c[i] ) * 255.0f + 0.5f );
			}
}

void LutImage( const Lut &lut, int cols, std::vector<unsigned char> &rgba, int &wide, int &tall )
{
	const int n = lut.size;
	if ( n < 2 || cols < 1 || lut.rgb.size() != (size_t)n * n * n * 3 )
	{
		wide = tall = 0;
		rgba.clear();
		return;
	}
	if ( cols > n )
		cols = n;
	const int rows = ( n + cols - 1 ) / cols;
	wide = n * cols;
	tall = n * rows;
	rgba.assign( (size_t)wide * tall * 4, 0 );
	for ( int b = 0; b < n; b++ )
	{
		const int tx = ( b % cols ) * n, ty = ( b / cols ) * n;
		for ( int g = 0; g < n; g++ )
			for ( int r = 0; r < n; r++ )
			{
				const unsigned char *src = &lut.rgb[ ( ( (size_t)b * n + g ) * n + r ) * 3 ];
				unsigned char *dst = &rgba[ ( (size_t)( ty + g ) * wide + tx + r ) * 4 ];
				dst[0] = src[0];
				dst[1] = src[1];
				dst[2] = src[2];
				dst[3] = 255;
			}
	}
}

bool CleanName( const char *in, char *out, int outSize )
{
	if ( !out || outSize <= 0 )
		return false;
	out[0] = 0;
	if ( !in )
		return false;
	// gfx/colorcorrection/ (20) + name + .raw (4) + terminator under COLORCORR_MAX_PATH (64)
	const int maxLen = COLORCORR_MAX_PATH - (int)( sizeof( COLORCORR_FOLDER ) - 1 ) - 4 - 1;
	int n = 0;
	for ( const char *p = in; *p; p++ )
	{
		char c = *p;
		if ( c >= 'A' && c <= 'Z' )
			c = (char)( c - 'A' + 'a' );
		const bool ok = ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-';
		if ( !ok || n >= maxLen || n >= outSize - 1 )
		{
			out[0] = 0;
			return false;
		}
		out[ n++ ] = c;
	}
	out[n] = 0;
	return n > 0;
}

//-----------------------------------------------------------------------------
// The operation file
//-----------------------------------------------------------------------------
namespace
{
const char *const TYPE_KEYS[ OP_COUNT ] = { "curves", "levels", "selected_hsv", "color_balance", "lookup" };

void Line( std::string &out, int depth, const char *key, const char *value )
{
	out.append( (size_t)depth, '\t' );
	out += '"';
	out += key;
	out += "\" \"";
	// names come from the player: no quote, backslash or line break inside a value
	for ( const char *p = value; *p; p++ )
		out += ( *p == '"' || *p == '\\' || *p == '\n' || *p == '\r' ) ? '\'' : *p;
	out += "\"\n";
}

void LineF( std::string &out, int depth, const char *key, const char *fmt, ... )
{
	char buf[ 512 ];
	va_list va;
	va_start( va, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, va );
	va_end( va );
	Line( out, depth, key, buf );
}

// A bounded tokenizer: quoted strings, { and }, // comments; nothing else
struct Tokens
{
	const char *p, *end;
	int line = 1;
	std::string tok;
	enum Kind { END, STRING, OPEN, CLOSE, BAD } kind = END;

	void Next()
	{
		for ( ;; )
		{
			while ( p < end && ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ) )
			{
				if ( *p == '\n' )
					line++;
				p++;
			}
			if ( p + 1 < end && p[0] == '/' && p[1] == '/' )
			{
				while ( p < end && *p != '\n' )
					p++;
				continue;
			}
			break;
		}
		tok.clear();
		if ( p >= end || !*p )
		{
			kind = END;
			return;
		}
		if ( *p == '{' || *p == '}' )
		{
			kind = *p == '{' ? OPEN : CLOSE;
			p++;
			return;
		}
		if ( *p != '"' )
		{
			kind = BAD;
			return;
		}
		p++;
		while ( p < end && *p != '"' && *p != '\n' && *p )
		{
			if ( tok.size() < 1024 )	// a 16-point curve is about 550 characters
				tok += *p;
			p++;
		}
		if ( p >= end || *p != '"' )
		{
			kind = BAD;
			return;
		}
		p++;
		kind = STRING;
	}
};

int Int( const std::string &s, int lo, int hi )
{
	const long v = strtol( s.c_str(), NULL, 10 );
	return v < lo ? lo : ( v > hi ? hi : (int)v );
}

float Float( const char *s, float lo, float hi, float def )
{
	char *e = NULL;
	const float v = strtof( s, &e );
	if ( e == s || !( v == v ) )
		return def;
	return v < lo ? lo : ( v > hi ? hi : v );
}

// "a b c ..." into up to n ints; returns how many
int Ints( const std::string &s, int *out, int n, int lo, int hi )
{
	const char *p = s.c_str();
	int k = 0;
	while ( k < n )
	{
		char *e = NULL;
		const long v = strtol( p, &e, 10 );
		if ( e == p )
			break;
		out[ k++ ] = v < lo ? lo : ( v > hi ? hi : (int)v );
		p = e;
	}
	return k;
}

bool Fail( char *err, int errLen, int line, const char *what )
{
	if ( err && errLen > 0 )
		snprintf( err, errLen, "line %d: %s", line, what );
	return false;
}

void ReadKey( Operation &op, const std::string &key, const std::string &val )
{
	const char *k = key.c_str();
	if ( key == "type" )
	{
		for ( int t = 0; t < OP_COUNT; t++ )
			if ( val == TYPE_KEYS[t] )
				op.type = t;
	}
	else if ( key == "name" )
		op.name = val;
	else if ( key == "enabled" )
		op.enabled = Int( val, 0, 1 ) != 0;
	else if ( key == "blend" )
		op.blend = Int( val, 0, 255 );
	else if ( !strncmp( k, "curve", 5 ) && k[5] >= '0' && k[5] < '0' + CH_COUNT && !k[6] )
	{
		Curve &c = op.curves[ k[5] - '0' ];
		const char *p = val.c_str();
		int n = 0;
		float last = -1.0f;
		while ( n < sboctl::CURVE_MAX_POINTS )
		{
			char *e1 = NULL, *e2 = NULL;
			float x = strtof( p, &e1 );
			if ( e1 == p )
				break;
			float y = strtof( e1, &e2 );
			if ( e2 == e1 || !( x == x ) || !( y == y ) )
				break;
			x = Clamp01( x );
			y = Clamp01( y );
			if ( x <= last )
				break;			// points must rise
			c.points[ n ].x = x;
			c.points[ n ].y = y;
			last = x;
			n++;
			p = e2;
		}
		if ( n >= 2 )
			c.count = n;
	}
	else if ( !strncmp( k, "levels", 6 ) && k[6] >= '0' && k[6] < '0' + CH_COUNT && !k[7] )
	{
		Levels &l = op.levels[ k[6] - '0' ];
		const char *p = val.c_str();
		char *e = NULL;
		l.inBlack = (int)strtol( p, &e, 10 ); p = e;
		l.inWhite = (int)strtol( p, &e, 10 ); p = e;
		l.gamma = Float( p, 0.1f, 9.99f, 1.0f ); strtof( p, &e ); p = e;
		l.outBlack = (int)strtol( p, &e, 10 ); p = e;
		l.outWhite = (int)strtol( p, &e, 10 );
		l.inBlack = ClampInt( l.inBlack, 0, 254 );
		l.inWhite = ClampInt( l.inWhite, l.inBlack + 1, 255 );
		l.outBlack = ClampInt( l.outBlack, 0, 255 );
		l.outWhite = ClampInt( l.outWhite, 0, 255 );
	}
	else if ( key == "selectmode" )
		op.selectMode = Int( val, 0, SELECT_COUNT - 1 );
	else if ( key == "target" )
		Ints( val, op.target, 3, 0, 255 );
	else if ( key == "tolerance" )
		op.tolerance = Int( val, 0, 255 );
	else if ( key == "fuzziness" )
		op.fuzziness = Int( val, 0, 255 );
	else if ( key == "hue" )
		op.hue = Int( val, -360, 360 );
	else if ( key == "saturation" )
		op.saturation = Int( val, -255, 255 );
	else if ( key == "value" )
		op.value = Int( val, -255, 255 );
	else if ( key == "colorize" )
		op.colorize = Int( val, 0, 1 ) != 0;
	else if ( key == "invert" )
		op.invert = Int( val, 0, 1 ) != 0;
	else if ( !strncmp( k, "balance", 7 ) && k[7] >= '0' && k[7] < '0' + RANGE_COUNT && !k[8] )
		Ints( val, op.balance[ k[7] - '0' ], 3, -100, 100 );
	else if ( key == "preserve" )
		op.preserveLuminosity = Int( val, 0, 1 ) != 0;
	else if ( key == "file" )
		op.file = val;
}
} // namespace

void WriteOpsText( const std::vector<Operation> &ops, int blend, std::string &out )
{
	out = "// SBO color correction editor (colorcorrectionui): the operations of the look in the .raw of the same name.\n"
		"// Only the editor reads this file; maps use the .raw.\n\"ColorCorrectionOps\"\n{\n";
	LineF( out, 1, "version", "1" );
	LineF( out, 1, "blend", "%d", ClampInt( blend, 0, 255 ) );
	for ( const Operation &op : ops )
	{
		out += "\t\"op\"\n\t{\n";
		const int type = ( op.type >= 0 && op.type < OP_COUNT ) ? op.type : OP_CURVES;
		Line( out, 2, "type", TYPE_KEYS[ type ] );
		Line( out, 2, "name", op.name.c_str() );
		LineF( out, 2, "enabled", "%d", op.enabled ? 1 : 0 );
		LineF( out, 2, "blend", "%d", ClampInt( op.blend, 0, 255 ) );
		switch ( type )
		{
		case OP_CURVES:
			for ( int ch = 0; ch < CH_COUNT; ch++ )
			{
				std::string v;
				const Curve &c = op.curves[ch];
				for ( int i = 0; i < ClampInt( c.count, 0, sboctl::CURVE_MAX_POINTS ); i++ )
				{
					char b[ 64 ];
					snprintf( b, sizeof( b ), "%s%.9g %.9g", i ? " " : "", c.points[i].x, c.points[i].y );
					v += b;
				}
				char key[ 32 ];
				snprintf( key, sizeof( key ), "curve%d", ch );
				Line( out, 2, key, v.c_str() );
			}
			break;
		case OP_LEVELS:
			for ( int ch = 0; ch < CH_COUNT; ch++ )
			{
				const Levels &l = op.levels[ch];
				char key[ 32 ];
				snprintf( key, sizeof( key ), "levels%d", ch );
				LineF( out, 2, key, "%d %d %.9g %d %d", l.inBlack, l.inWhite, l.gamma, l.outBlack, l.outWhite );
			}
			break;
		case OP_SELECTED_HSV:
			LineF( out, 2, "selectmode", "%d", op.selectMode );
			LineF( out, 2, "target", "%d %d %d", op.target[0], op.target[1], op.target[2] );
			LineF( out, 2, "tolerance", "%d", op.tolerance );
			LineF( out, 2, "fuzziness", "%d", op.fuzziness );
			LineF( out, 2, "hue", "%d", op.hue );
			LineF( out, 2, "saturation", "%d", op.saturation );
			LineF( out, 2, "value", "%d", op.value );
			LineF( out, 2, "colorize", "%d", op.colorize ? 1 : 0 );
			LineF( out, 2, "invert", "%d", op.invert ? 1 : 0 );
			break;
		case OP_BALANCE:
			for ( int r = 0; r < RANGE_COUNT; r++ )
			{
				char key[ 32 ];
				snprintf( key, sizeof( key ), "balance%d", r );
				LineF( out, 2, key, "%d %d %d", op.balance[r][0], op.balance[r][1], op.balance[r][2] );
			}
			LineF( out, 2, "preserve", "%d", op.preserveLuminosity ? 1 : 0 );
			break;
		case OP_LOOKUP:
			Line( out, 2, "file", op.file.c_str() );
			break;
		}
		out += "\t}\n";
	}
	out += "}\n";
}

bool ReadOpsText( const char *text, int len, std::vector<Operation> &ops, int &blend, char *err, int errLen )
{
	ops.clear();
	blend = 255;
	if ( !text || len <= 0 )
		return Fail( err, errLen, 0, "empty" );
	Tokens t;
	t.p = text;
	t.end = text + len;
	t.Next();
	if ( t.kind != Tokens::STRING || t.tok != "ColorCorrectionOps" )
		return Fail( err, errLen, t.line, "not a color correction operation file" );
	t.Next();
	if ( t.kind != Tokens::OPEN )
		return Fail( err, errLen, t.line, "{ expected" );
	for ( ;; )
	{
		t.Next();
		if ( t.kind == Tokens::CLOSE )
			return true;
		if ( t.kind != Tokens::STRING )
			return Fail( err, errLen, t.line, "a key or } expected" );
		const std::string key = t.tok;
		t.Next();
		if ( t.kind == Tokens::STRING )
		{
			if ( key == "blend" )
				blend = Int( t.tok, 0, 255 );
			continue;
		}
		if ( t.kind != Tokens::OPEN )
			return Fail( err, errLen, t.line, "a value or { expected" );
		Operation op;
		bool typed = false;
		for ( ;; )
		{
			t.Next();
			if ( t.kind == Tokens::CLOSE )
				break;
			if ( t.kind != Tokens::STRING )
				return Fail( err, errLen, t.line, "a key or } expected" );
			const std::string k = t.tok;
			t.Next();
			if ( t.kind != Tokens::STRING )
				return Fail( err, errLen, t.line, "a value expected" );
			if ( k == "type" )
			{
				const int before = op.type;
				op.type = -1;
				ReadKey( op, k, t.tok );
				if ( op.type < 0 )
				{
					op.type = before;
					continue;
				}
				// a new operation of that type with its defaults; the keys after it fill it in
				const std::string name = op.name;
				op = Make( op.type );
				if ( !name.empty() )
					op.name = name;
				typed = true;
				continue;
			}
			ReadKey( op, k, t.tok );
		}
		if ( key == "op" && typed )
		{
			if ( (int)ops.size() >= OPS_MAX )
				return Fail( err, errLen, t.line, "too many operations" );
			ops.push_back( op );
		}
	}
}
} // namespace ops
} // namespace colorcorr
