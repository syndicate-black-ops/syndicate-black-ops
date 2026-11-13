//=========================================================
// SBO 2.0 env_overlay, client side: message, fades, placement. See overlay_core.h.
//=========================================================

#include "overlay_core.h"

#include <math.h>
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
		out[n] = '\0';
	}
};

bool Finite( float f )
{
	return f == f && f < 1e30f && f > -1e30f;
}
} // namespace

bool overlay::Parse( const unsigned char *data, int size, Params &out )
{
	memset( &out, 0, sizeof( out ) );
	if ( !data || size < 2 )
		return false;

	Reader r = { data, size, 0, false };
	out.channel = r.Byte();
	out.flags = r.Byte();
	out.scale = 1.0f;

	if ( out.channel > OVERLAY_CHANNELS )
		return false;

	if ( !( out.flags & OVERLAY_FLAG_ON ) )
		return !r.bad;

	if ( out.channel < 1 )
		return false;	// "on" needs one channel

	r.String( out.image, sizeof( out.image ) );
	const int x = r.Short();
	const int y = r.Short();
	const int scale = r.Short();
	out.r = r.Byte();
	out.g = r.Byte();
	out.b = r.Byte();
	out.a = r.Byte();
	out.framerate = r.Word() / OVERLAY_TIME_UNIT;
	out.fadein = r.Word() / OVERLAY_TIME_UNIT;
	out.hold = r.Word() / OVERLAY_TIME_UNIT;
	out.fadeout = r.Word() / OVERLAY_TIME_UNIT;
	out.elapsed = r.Word() / OVERLAY_ELAPSED_UNIT;

	if ( r.bad )
		return false;

	out.x = ( x <= -(int)OVERLAY_POS_UNIT ) ? -1.0f : ( x > (int)OVERLAY_POS_UNIT ? 1.0f : x / OVERLAY_POS_UNIT );
	out.y = ( y <= -(int)OVERLAY_POS_UNIT ) ? -1.0f : ( y > (int)OVERLAY_POS_UNIT ? 1.0f : y / OVERLAY_POS_UNIT );
	out.scale = scale > 0 ? scale / OVERLAY_SCALE_UNIT : 1.0f;

	// a plain color is drawn over the whole screen only
	if ( !out.image[0] && !( out.flags & OVERLAY_FLAG_FULLSCREEN ) )
		return false;

	return true;
}

// one pass of fade in, hold, fade out from s seconds on; hold < 0: no end after the fade in
static float Pass( float s, float fadein, float hold, float fadeout )
{
	if ( s < fadein )
		return s / fadein;
	s -= fadein;
	if ( hold < 0.0f || s < hold )
		return 1.0f;
	s -= hold;
	if ( s < fadeout )
		return 1.0f - s / fadeout;
	return -1.0f;
}

float overlay::Alpha( const Params &p, float t )
{
	if ( !Finite( t ) )
		return -1.0f;
	if ( t < 0.0f )
		t = 0.0f;

	if ( p.flags & OVERLAY_FLAG_LOOP )
	{
		const float cycle = p.fadein + p.hold + p.fadeout;
		if ( !( cycle > 0.0f ) )
			return 1.0f;
		const float a = Pass( fmodf( t, cycle ), p.fadein, p.hold, p.fadeout );
		return a < 0.0f ? 0.0f : a;
	}

	if ( p.flags & OVERLAY_FLAG_STAY )
		return Pass( t, p.fadein, -1.0f, 0.0f );

	return Pass( t, p.fadein, p.hold, p.fadeout );
}

float overlay::AlphaOff( const Params &p, float a0, float dt )
{
	if ( !Finite( dt ) || !Finite( a0 ) || ( p.flags & OVERLAY_FLAG_NOW ) || !( p.fadeout > 0.0f ) || dt >= p.fadeout )
		return -1.0f;
	if ( dt < 0.0f )
		dt = 0.0f;
	return a0 * ( 1.0f - dt / p.fadeout );
}

int overlay::Place( float pos, int size, int screen )
{
	int p;
	if ( pos == -1.0f )
		p = ( screen - size ) / 2;
	else if ( pos < 0.0f )
		p = (int)( ( 1.0f + pos ) * screen ) - size;
	else
		p = (int)( pos * screen );

	if ( size <= screen )
	{
		if ( p + size > screen )
			p = screen - size;
		else if ( p < 0 )
			p = 0;
	}
	return p;
}

int overlay::Frame( const Params &p, float t, int frames )
{
	if ( frames <= 1 || !( p.framerate > 0.0f ) || !Finite( t ) || t < 0.0f )
		return 0;
	const double f = floor( (double)t * p.framerate );
	return (int)fmod( f, (double)frames );
}
