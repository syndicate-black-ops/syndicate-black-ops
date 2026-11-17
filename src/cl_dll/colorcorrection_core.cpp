//=========================================================
// SBO 2.0 color_correction / color_correction_volume, client side: message, lookup tables, simple keys, weights,
// blend. See colorcorrection_core.h.
//=========================================================

#include "colorcorrection_core.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace
{
struct Reader
{
	const unsigned char	*data;
	int					size;
	int					pos;
	bool				bad;

	int Byte()
	{
		if ( pos + 1 > size )
		{
			bad = true;
			return 0;
		}
		return data[pos++];
	}

	int Short()		// signed
	{
		if ( pos + 2 > size )
		{
			bad = true;
			return 0;
		}
		const int v = data[pos] | ( data[pos + 1] << 8 );
		pos += 2;
		return v >= 32768 ? v - 65536 : v;
	}

	int Word()		// unsigned
	{
		return Short() & 0xFFFF;
	}

	// a string cut to len - 1 bytes; the rest of it is skipped
	void String( char *out, int len )
	{
		int n = 0;
		for ( ;; )
		{
			if ( pos >= size )
			{
				bad = true;
				break;
			}
			const unsigned char c = data[pos++];
			if ( !c )
				break;
			if ( n < len - 1 )
				out[n++] = (char)c;
		}
		out[n] = 0;
	}
};

float Clamp01( float f )
{
	if ( !( f > 0.0f ) )
		return 0.0f;
	return f > 1.0f ? 1.0f : f;
}

void SetError( char *err, int errLen, const char *text )
{
	if ( err && errLen > 0 )
		snprintf( err, (size_t)errLen, "%s", text );
}

bool HasExtension( const char *path, const char *ext )
{
	const size_t len = strlen( path ), n = strlen( ext );
	if ( len < n )
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
}

unsigned char ToByte( float f )
{
	return (unsigned char)( Clamp01( f ) * 255.0f + 0.5f );
}
} // namespace

namespace colorcorr
{
bool Parse( const unsigned char *data, int size, Params &out )
{
	memset( &out, 0, sizeof( out ) );
	if ( !data || size <= 0 )
		return false;

	Reader r = { data, size, 0, false };
	out.index = r.Short();
	out.flags = r.Byte();
	if ( r.bad || out.index < 1 || out.index > 2047 )
		return false;
	if ( out.flags & COLORCORR_FLAG_REMOVED )
		return true;

	out.maxWeight = Clamp01( r.Short() / COLORCORR_WEIGHT_UNIT );
	out.weightNow = Clamp01( r.Short() / COLORCORR_WEIGHT_UNIT );
	if ( out.weightNow > out.maxWeight )
		out.weightNow = out.maxWeight;
	out.fadeIn = r.Word() / COLORCORR_TIME_UNIT;
	out.fadeOut = r.Word() / COLORCORR_TIME_UNIT;
	if ( out.flags & COLORCORR_FLAG_VOLUME )
	{
		out.model = r.Short();
		out.origin[0] = (float)r.Short();
		out.origin[1] = (float)r.Short();
		out.origin[2] = (float)r.Short();
		out.minFalloff = out.maxFalloff = -1.0f;
	}
	else
	{
		out.origin[0] = (float)r.Short();
		out.origin[1] = (float)r.Short();
		out.origin[2] = (float)r.Short();
		out.minFalloff = (float)r.Short();
		out.maxFalloff = (float)r.Short();
	}
	out.saturation = r.Short() / COLORCORR_KEY_UNIT;
	out.contrast = r.Short() / COLORCORR_KEY_UNIT;
	out.brightness = r.Short() / COLORCORR_KEY_UNIT;
	out.gamma = r.Short() / COLORCORR_KEY_UNIT;
	out.tint[0] = r.Byte();
	out.tint[1] = r.Byte();
	out.tint[2] = r.Byte();
	out.tintAmount = r.Byte() / 255.0f;
	r.String( out.file, sizeof( out.file ) );
	if ( r.bad )
		return false;

	// a path the server could not have precached is not read
	if ( out.file[0] && !ColorCorr_ValidPath( out.file ) )
		out.file[0] = 0;
	return true;
}

bool ReadRaw( const unsigned char *data, int len, Lut &out, char *err, int errLen )
{
	if ( !data || len != RAW_BYTES )
	{
		char text[96];
		snprintf( text, sizeof( text ), "a .raw table is %d bytes (32x32x32 RGB), this one is %d", RAW_BYTES, len );
		SetError( err, errLen, text );
		return false;
	}
	out.size = RAW_SIZE;
	out.rgb.assign( data, data + RAW_BYTES );
	return true;
}

bool ReadTable( const char *path, const unsigned char *data, int len, Lut &out, char *err, int errLen )
{
	if ( path && HasExtension( path, ".raw" ) )
		return ReadRaw( data, len, out, err, errLen );
	SetError( err, errLen, "not a .raw file" );
	return false;
}

bool HasSimpleKeys( const Params &p )
{
	return p.saturation != 1.0f || p.contrast != 1.0f || p.brightness != 0.0f || p.gamma != 1.0f
		|| ( p.tintAmount > 0.0f && ( p.tint[0] != 255 || p.tint[1] != 255 || p.tint[2] != 255 ) );
}

void ApplySimpleKeys( const Params &p, float rgb[3] )
{
	const float lum = 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
	const float invGamma = p.gamma > 0.001f ? 1.0f / p.gamma : 1.0f;
	const float amount = Clamp01( p.tintAmount );
	for ( int c = 0; c < 3; c++ )
	{
		float v = lum + ( rgb[c] - lum ) * p.saturation;
		v = ( v - 0.5f ) * p.contrast + 0.5f;
		v += p.brightness;
		v = Clamp01( v );
		if ( invGamma != 1.0f && v > 0.0f )
			v = powf( v, invGamma );
		const float t = p.tint[c] / 255.0f;
		v = v * ( 1.0f - amount ) + v * t * amount;
		rgb[c] = Clamp01( v );
	}
}

void IdentityLut( int size, Lut &out )
{
	if ( size < LUT_MIN_SIZE )
		size = LUT_MIN_SIZE;
	if ( size > LUT_MAX_SIZE )
		size = LUT_MAX_SIZE;
	out.size = size;
	out.rgb.resize( (size_t)size * size * size * 3 );
	size_t i = 0;
	for ( int b = 0; b < size; b++ )
		for ( int g = 0; g < size; g++ )
			for ( int r = 0; r < size; r++ )
			{
				out.rgb[i++] = ToByte( r / (float)( size - 1 ) );
				out.rgb[i++] = ToByte( g / (float)( size - 1 ) );
				out.rgb[i++] = ToByte( b / (float)( size - 1 ) );
			}
}

void BuildLut( const Params &p, const Lut *file, Lut &out )
{
	const bool hasFile = file && file->size >= LUT_MIN_SIZE && file->size <= LUT_MAX_SIZE
		&& file->rgb.size() == (size_t)file->size * file->size * file->size * 3;
	if ( hasFile )
		out = *file;
	else
		IdentityLut( RAW_SIZE, out );

	if ( !HasSimpleKeys( p ) )
		return;
	for ( size_t i = 0; i + 2 < out.rgb.size(); i += 3 )
	{
		float rgb[3] = { out.rgb[i] / 255.0f, out.rgb[i + 1] / 255.0f, out.rgb[i + 2] / 255.0f };
		ApplySimpleKeys( p, rgb );
		out.rgb[i] = ToByte( rgb[0] );
		out.rgb[i + 1] = ToByte( rgb[1] );
		out.rgb[i + 2] = ToByte( rgb[2] );
	}
}

float FalloffWeight( const Params &p, float w, const float view[3] )
{
	if ( p.minFalloff < 0.0f || p.maxFalloff < 0.0f || p.minFalloff == p.maxFalloff )
		return w;
	const float dx = view[0] - p.origin[0], dy = view[1] - p.origin[1], dz = view[2] - p.origin[2];
	const float dist = sqrtf( dx * dx + dy * dy + dz * dz );
	float f = ( dist - p.minFalloff ) / ( p.maxFalloff - p.minFalloff );
	f = Clamp01( f );
	return w * ( 1.0f - f );
}

float Approach( float w0, float target, float rate, float dt )
{
	if ( rate <= 0.0f )
		return target;
	if ( !( dt > 0.0f ) )
		return w0;
	const float step = rate * dt;
	if ( w0 < target )
		return w0 + step >= target ? target : w0 + step;
	return w0 - step <= target ? target : w0 - step;
}

float FadeRate( float maxWeight, float time )
{
	if ( !( time > 0.0f ) || !( maxWeight > 0.0f ) )
		return 0.0f;
	return maxWeight / time;
}

int Blend( const float *weights, const bool *exclusive, int n, int outIndex[ BLEND_MAX ], float outWeight[ BLEND_MAX ],
	float &identity )
{
	identity = 1.0f;
	if ( !weights || n <= 0 )
		return 0;

	// the heaviest exclusive one
	int iExclusive = -1;
	float flExclusive = 0.0f;
	for ( int i = 0; i < n; i++ )
	{
		const float w = Clamp01( weights[i] );
		if ( exclusive && exclusive[i] && w > flExclusive )
		{
			flExclusive = w;
			iExclusive = i;
		}
	}

	int count = 0;
	for ( int i = 0; i < n; i++ )
	{
		float w = Clamp01( weights[i] );
		if ( iExclusive >= 0 && i != iExclusive )
			w *= 1.0f - flExclusive;
		if ( w < 0.001f )
			continue;

		// keep the BLEND_MAX heaviest, heaviest first
		int at = count;
		while ( at > 0 && outWeight[at - 1] < w )
			at--;
		if ( at >= BLEND_MAX )
			continue;
		const int last = count < BLEND_MAX ? count : BLEND_MAX - 1;
		for ( int j = last; j > at; j-- )
		{
			outWeight[j] = outWeight[j - 1];
			outIndex[j] = outIndex[j - 1];
		}
		outWeight[at] = w;
		outIndex[at] = i;
		if ( count < BLEND_MAX )
			count++;
	}

	float sum = 0.0f;
	for ( int i = 0; i < count; i++ )
		sum += outWeight[i];
	if ( sum > 1.0f )
	{
		for ( int i = 0; i < count; i++ )
			outWeight[i] /= sum;
		sum = 1.0f;
	}
	identity = 1.0f - sum;
	if ( identity < 0.0f )
		identity = 0.0f;
	return count;
}
} // namespace colorcorr
