//=========================================================
// SBO 2.1 info_waypoint, client side: message and screen placement. See waypoint_core.h.
//=========================================================

#include "waypoint_core.h"

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

	float Float()
	{
		if ( pos + 4 > size )
		{
			bad = true;
			return 0.0f;
		}
		unsigned int bits = (unsigned int)data[pos] | ( (unsigned int)data[pos + 1] << 8 )
			| ( (unsigned int)data[pos + 2] << 16 ) | ( (unsigned int)data[pos + 3] << 24 );
		pos += 4;
		float f;
		memcpy( &f, &bits, sizeof( f ) );
		return f;
	}

	// a string of at most size - 1 bytes (the rest is skipped up to its end)
	void String( char *out, int outSize )
	{
		int n = 0;
		while ( true )
		{
			if ( pos >= size )
			{
				bad = true;
				break;
			}
			const char c = (char)data[pos++];
			if ( !c )
				break;
			if ( n < outSize - 1 )
				out[n++] = c;
		}
		out[n] = '\0';
	}
};

// a position no map reaches (or not a number): refused
bool Sane( float f )
{
	return f == f && f > -131072.0f && f < 131072.0f;
}
} // namespace

namespace waypoint
{
bool Parse( const unsigned char *data, int size, Marker &out )
{
	memset( &out, 0, sizeof( out ) );
	if ( !data || size <= 0 )
		return false;

	Reader r = { data, size, 0, false };
	out.id = r.Short();
	out.flags = r.Byte();
	if ( r.bad || out.id < 0 || out.id >= WAYPOINT_MAX_INDEX )
		return false;
	if ( !( out.flags & WAYPOINT_FLAG_ON ) )
		return true;

	for ( int i = 0; i < 3; i++ )
		out.origin[i] = r.Float();
	out.sprite = r.Short();
	out.r = r.Byte();
	out.g = r.Byte();
	out.b = r.Byte();
	out.follow = r.Short();
	for ( int i = 0; i < 3; i++ )
		out.offset[i] = r.Float();
	out.minRange = r.Short();
	out.maxRange = r.Short();
	out.owner = r.Byte();
	r.String( out.label, sizeof( out.label ) );
	if ( r.bad || out.id == 0 )
		return false;

	for ( int i = 0; i < 3; i++ )
	{
		if ( !Sane( out.origin[i] ) || !Sane( out.offset[i] ) )
			return false;
	}
	if ( out.sprite < 0 )
		out.sprite = 0;
	if ( out.minRange < 0 )
		out.minRange = 0;
	if ( out.maxRange < 0 )
		out.maxRange = 0;
	if ( out.owner > 32 )
		return false;
	if ( !( out.flags & WAYPOINT_FLAG_FOLLOW ) || out.follow <= 0 || out.follow >= WAYPOINT_MAX_INDEX )
	{
		out.follow = 0;
		out.flags &= ~WAYPOINT_FLAG_FOLLOW;
	}
	return true;
}

bool InRange( const Marker &m, float dist )
{
	if ( m.minRange > 0 && dist < (float)m.minRange )
		return false;
	if ( m.maxRange > 0 && dist > (float)m.maxRange )
		return false;
	return true;
}

Place Locate( float ndcX, float ndcY, bool front, float right, float up, float w, float h, float marginX,
	float marginTop, float marginBottom )
{
	Place p;
	const float cx = w * 0.5f, cy = h * 0.5f;
	float dx, dy;

	if ( front && isfinite( ndcX ) && isfinite( ndcY ) )
	{
		p.x = ( 1.0f + ndcX ) * cx;
		p.y = ( 1.0f - ndcY ) * cy;
		p.edge = false;
		p.angle = 0.0f;
		if ( p.x >= marginX && p.x <= w - marginX && p.y >= marginTop && p.y <= h - marginBottom )
			return p;
		dx = p.x - cx;
		dy = p.y - cy;
	}
	else
	{
		// behind the eye: where it is beside or above the view (screen y goes down)
		dx = isfinite( right ) ? right : 0.0f;
		dy = isfinite( up ) ? -up : 0.0f;
	}

	// straight behind (or no direction): the bottom edge
	if ( fabsf( dx ) < 1e-6f && fabsf( dy ) < 1e-6f )
	{
		dx = 0.0f;
		dy = 1.0f;
	}

	// on the ray from the centre toward the spot, where it meets the edge kept free of the margins
	float hx = cx - marginX, hy = dy < 0.0f ? cy - marginTop : ( h - marginBottom ) - cy;
	if ( hx < 0.0f )
		hx = 0.0f;
	if ( hy < 0.0f )
		hy = 0.0f;
	float t = 1e30f;
	if ( fabsf( dx ) > 1e-6f )
		t = hx / fabsf( dx );
	if ( fabsf( dy ) > 1e-6f && hy / fabsf( dy ) < t )
		t = hy / fabsf( dy );

	p.x = cx + dx * t;
	p.y = cy + dy * t;
	p.edge = true;
	p.angle = atan2f( dy, dx );
	return p;
}

void Spread( Place *places, int n, float sepX, float sepY, float w, float h, float marginX, float marginTop,
	float marginBottom )
{
	if ( !places || n <= 1 )
		return;
	if ( n > WAYPOINT_MAX )
		n = WAYPOINT_MAX;
	for ( int i = 1; i < n; i++ )
	{
		Place &p = places[i];
		if ( !p.edge )
			continue;
		const bool side = p.x <= marginX + 0.5f || p.x >= w - marginX - 0.5f;	// the left or right edge
		// a few passes: moved past one, it may now cover another
		for ( int pass = 0; pass < WAYPOINT_MAX; pass++ )
		{
			bool moved = false;
			for ( int j = 0; j < i; j++ )
			{
				const Place &q = places[j];
				if ( !q.edge || fabsf( p.x - q.x ) >= sepX || fabsf( p.y - q.y ) >= sepY )
					continue;
				if ( side )
					p.y = q.y + sepY;
				else
					p.x = q.x + sepX;
				moved = true;
			}
			if ( p.y > h - marginBottom )
				p.y = h - marginBottom;
			if ( p.x > w - marginX )
				p.x = w - marginX;
			if ( p.y < marginTop )
				p.y = marginTop;
			if ( p.x < marginX )
				p.x = marginX;
			if ( !moved )
				break;
		}
	}
}

void FormatDistance( float units, char *out, int size )
{
	if ( !out || size <= 0 )
		return;
	float m = units / WAYPOINT_UNITS_PER_METER;
	if ( !( m >= 0.0f ) )
		m = 0.0f;
	if ( m > 99999.0f )
		m = 99999.0f;
	snprintf( out, size, "%d m", (int)m );
}
} // namespace waypoint
