//=========================================================
// SBO 2.0 trigger_objective, client core: see objective_core.h.
//=========================================================

#include "objective_core.h"

#include <string.h>

namespace objective
{
namespace
{
struct Reader
{
	const unsigned char *data;
	int size;
	int pos;
	bool bad;

	int Byte( void )
	{
		if ( pos + 1 > size )
		{
			bad = true;
			return 0;
		}
		return data[ pos++ ];
	}

	int Short( void )
	{
		if ( pos + 2 > size )
		{
			bad = true;
			return 0;
		}
		const int v = (short)( data[ pos ] | ( data[ pos + 1 ] << 8 ) );
		pos += 2;
		return v;
	}

	// a terminated string; longer than the buffer: cut. No terminator in the message: bad.
	void String( char *out, int outSize )
	{
		int n = 0;
		while ( pos < size )
		{
			const char c = (char)data[ pos++ ];
			if ( !c )
			{
				out[ n ] = '\0';
				return;
			}
			if ( n < outSize - 1 )
				out[ n++ ] = c;
		}
		out[ n ] = '\0';
		bad = true;
	}
};

int Clamp( int v, int lo, int hi )
{
	return v < lo ? lo : ( v > hi ? hi : v );
}
} // namespace

bool Parse( const unsigned char *data, int size, Message &out )
{
	memset( &out, 0, sizeof( out ) );
	out.parent = -1;
	if ( !data || size <= 0 )
		return false;

	Reader r = { data, size, 0, false };
	out.slot = r.Byte();
	if ( out.slot == OBJECTIVE_SLOT_CLEAR )
		return true;
	if ( out.slot >= OBJECTIVE_MAX )
		return false;

	const int flags = r.Byte();
	if ( r.bad )
		return false;
	out.state = flags & OBJECTIVE_STATE_MASK;
	out.flags = flags & ~OBJECTIVE_STATE_MASK;
	if ( out.state > OBJECTIVE_COMPLETED )
		return false;
	if ( out.state == OBJECTIVE_HIDDEN )
		return true;

	out.order = Clamp( r.Short(), 0, OBJECTIVE_MAX_ORDER );
	out.sequence = Clamp( r.Short(), 0, 32767 );
	out.count = r.Byte();
	out.target = r.Byte();
	out.parent = r.Byte();
	r.String( out.text, sizeof( out.text ) );
	if ( r.bad )
		return false;
	if ( out.target > 0 && out.count > out.target )
		out.count = out.target;
	if ( out.parent >= OBJECTIVE_MAX || out.parent == out.slot )
		out.parent = -1;	// none (OBJECTIVE_NO_PARENT), or nonsense
	return true;
}

void List::Reset( void )
{
	memset( m_Entries, 0, sizeof( m_Entries ) );
	for ( int i = 0; i < OBJECTIVE_MAX; i++ )
	{
		m_Entries[ i ].doneTime = -1.0f;
		m_Entries[ i ].parent = -1;
	}
	m_iRevision = 0;
	m_flClock = 0.0f;
	m_iNotices = 0;
	m_flNoticeStart = -1.0f;
}

void List::DropNotice( int slot )
{
	int n = 0;
	for ( int i = 0; i < m_iNotices; i++ )
	{
		if ( m_Notices[ i ] == slot )
		{
			if ( i == 0 )
				m_flNoticeStart = -1.0f;
			continue;
		}
		m_NoticeKinds[ n ] = m_NoticeKinds[ i ];
		m_Notices[ n++ ] = m_Notices[ i ];
	}
	m_iNotices = n;
}

void List::Apply( const Message &msg )
{
	m_iRevision++;

	if ( msg.slot == OBJECTIVE_SLOT_CLEAR )
	{
		const float clock = m_flClock;
		const int revision = m_iRevision;
		Reset();
		m_flClock = clock;
		m_iRevision = revision;
		return;
	}
	if ( msg.slot < 0 || msg.slot >= OBJECTIVE_MAX )
		return;

	Entry &e = m_Entries[ msg.slot ];
	if ( msg.state == OBJECTIVE_HIDDEN )
	{
		memset( &e, 0, sizeof( e ) );
		e.doneTime = -1.0f;
		e.parent = -1;
		DropNotice( msg.slot );
		return;
	}

	const bool wasFading = e.used && e.state == OBJECTIVE_COMPLETED && e.doneTime >= 0.0f;
	e.used = true;
	e.state = msg.state;
	e.order = msg.order;
	e.sequence = msg.sequence;
	e.count = msg.count;
	e.target = msg.target;
	e.parent = msg.parent;
	e.silent = ( msg.flags & OBJECTIVE_SILENT ) != 0;
	strncpy( e.text, msg.text, sizeof( e.text ) - 1 );
	e.text[ sizeof( e.text ) - 1 ] = '\0';

	if ( e.state == OBJECTIVE_COMPLETED )
	{
		// completed now: OBJECTIVE COMPLETE, the line stays a while on the compact list; completed before (a resend):
		// only in the full list
		if ( msg.flags & OBJECTIVE_EV_DONE )
		{
			e.doneTime = m_flClock;
			DropNotice( msg.slot );
			if ( !e.silent )
				QueueNotice( msg.slot, NOTICE_DONE );
		}
		else if ( !wasFading )
			e.doneTime = -1.0f;
	}
	else
	{
		e.doneTime = -1.0f;
		if ( ( msg.flags & OBJECTIVE_EV_NEW ) && !e.silent )
		{
			DropNotice( msg.slot );
			QueueNotice( msg.slot, NOTICE_NEW );
		}
	}
}

void List::QueueNotice( int slot, int kind )
{
	if ( m_iNotices == NOTICE_QUEUE )
	{
		// full: the oldest waiting one (not the one shown) goes
		const int from = m_iNotices > 1 ? 1 : 0;
		for ( int i = from; i < m_iNotices - 1; i++ )
		{
			m_Notices[ i ] = m_Notices[ i + 1 ];
			m_NoticeKinds[ i ] = m_NoticeKinds[ i + 1 ];
		}
		m_iNotices--;
		if ( from == 0 )
			m_flNoticeStart = -1.0f;
	}
	m_Notices[ m_iNotices ] = slot;
	m_NoticeKinds[ m_iNotices ] = kind;
	m_iNotices++;
}

bool List::Tick( float dt, bool visible )
{
	if ( !visible || !( dt > 0.0f ) )
		return false;
	if ( dt > 1.0f )
		dt = 1.0f;	// a hitch (loading) doesn't skip a notice
	m_flClock += dt;

	bool started = false;
	while ( m_iNotices > 0 )
	{
		if ( m_flNoticeStart < 0.0f )
		{
			m_flNoticeStart = m_flClock;
			started = true;
			break;
		}
		if ( m_flClock - m_flNoticeStart < NOTICE_FADE_IN + NOTICE_HOLD + NOTICE_FADE_OUT )
			break;
		// over: the next one
		for ( int i = 0; i < m_iNotices - 1; i++ )
		{
			m_Notices[ i ] = m_Notices[ i + 1 ];
			m_NoticeKinds[ i ] = m_NoticeKinds[ i + 1 ];
		}
		m_iNotices--;
		m_flNoticeStart = -1.0f;
	}
	return started;
}

bool List::Notice( int &slot, int &kind, float &alpha ) const
{
	slot = -1;
	kind = NOTICE_NEW;
	alpha = 0.0f;
	if ( m_iNotices <= 0 || m_flNoticeStart < 0.0f )
		return false;
	const float t = m_flClock - m_flNoticeStart;
	if ( t < 0.0f || t >= NOTICE_FADE_IN + NOTICE_HOLD + NOTICE_FADE_OUT )
		return false;
	slot = m_Notices[ 0 ];
	kind = m_NoticeKinds[ 0 ];
	if ( t < NOTICE_FADE_IN )
		alpha = t / NOTICE_FADE_IN;
	else if ( t < NOTICE_FADE_IN + NOTICE_HOLD )
		alpha = 1.0f;
	else
		alpha = 1.0f - ( t - NOTICE_FADE_IN - NOTICE_HOLD ) / NOTICE_FADE_OUT;
	if ( alpha < 0.0f )
		alpha = 0.0f;
	return m_Entries[ slot ].used;
}

bool List::IsTop( int slot ) const
{
	const int p = m_Entries[ slot ].parent;
	return p < 0 || p >= OBJECTIVE_MAX || !m_Entries[ p ].used || m_Entries[ p ].parent >= 0;
}

int List::ActiveCount( void ) const
{
	int n = 0;
	for ( int i = 0; i < OBJECTIVE_MAX; i++ )
		n += m_Entries[ i ].used && m_Entries[ i ].state == OBJECTIVE_ACTIVE && IsTop( i );
	return n;
}

int List::ShownCount( void ) const
{
	int n = 0;
	for ( int i = 0; i < OBJECTIVE_MAX; i++ )
		n += m_Entries[ i ].used;
	return n;
}

int List::TopCount( int &done ) const
{
	int n = 0;
	done = 0;
	for ( int i = 0; i < OBJECTIVE_MAX; i++ )
	{
		if ( !m_Entries[ i ].used || !IsTop( i ) )
			continue;
		n++;
		done += m_Entries[ i ].state == OBJECTIVE_COMPLETED;
	}
	return n;
}

int List::NewestActive( void ) const
{
	int best = -1;
	for ( int i = 0; i < OBJECTIVE_MAX; i++ )
	{
		const Entry &e = m_Entries[ i ];
		if ( e.used && e.state == OBJECTIVE_ACTIVE && IsTop( i ) && ( best < 0 || e.sequence > m_Entries[ best ].sequence ) )
			best = i;
	}
	return best;
}

bool List::Before( int a, int b ) const
{
	const Entry &ea = m_Entries[ a ], &eb = m_Entries[ b ];
	const bool na = ea.order > 0, nb = eb.order > 0;
	if ( na != nb )
		return na;
	if ( na && ea.order != eb.order )
		return ea.order < eb.order;
	if ( ea.sequence != eb.sequence )
		return ea.sequence < eb.sequence;
	return a < b;
}

void List::Sort( int *slots, int n ) const
{
	for ( int i = 1; i < n; i++ )
	{
		const int s = slots[ i ];
		int j = i - 1;
		while ( j >= 0 && Before( s, slots[ j ] ) )
		{
			slots[ j + 1 ] = slots[ j ];
			j--;
		}
		slots[ j + 1 ] = s;
	}
}

int List::AllRows( int *slots, int max ) const
{
	int n = 0;
	for ( int i = 0; i < OBJECTIVE_MAX && n < max; i++ )
	{
		if ( m_Entries[ i ].used )
			slots[ n++ ] = i;
	}
	Sort( slots, n );
	return n;
}

float List::RowAlpha( int slot ) const
{
	if ( slot < 0 || slot >= OBJECTIVE_MAX || !m_Entries[ slot ].used )
		return 0.0f;
	const Entry &e = m_Entries[ slot ];
	if ( e.state != OBJECTIVE_COMPLETED )
		return 1.0f;
	if ( e.doneTime < 0.0f )
		return 0.0f;
	const float t = m_flClock - e.doneTime;
	if ( t < DONE_HOLD )
		return 1.0f;
	const float a = 1.0f - ( t - DONE_HOLD ) / DONE_FADE;
	return a > 0.0f ? a : 0.0f;
}

int List::Parts( int group, int *slots, int max ) const
{
	int n = 0;
	for ( int i = 0; i < OBJECTIVE_MAX && n < max; i++ )
	{
		if ( m_Entries[ i ].used && i != group && m_Entries[ i ].parent == group && !IsTop( i ) )
			slots[ n++ ] = i;
	}
	Sort( slots, n );
	return n;
}

int List::CompactLines( Line *out, int maxLines, int &more ) const
{
	more = 0;
	int top[ OBJECTIVE_MAX ], nTop = 0;
	for ( int i = 0; i < OBJECTIVE_MAX; i++ )
	{
		const Entry &e = m_Entries[ i ];
		if ( e.used && IsTop( i ) && ( e.state == OBJECTIVE_ACTIVE || RowAlpha( i ) > 0.0f ) )
			top[ nTop++ ] = i;
	}
	// the newest first get the lines (a completed line still fading counts as new as its completion)
	for ( int a = 1; a < nTop; a++ )
	{
		const int s = top[ a ];
		int b = a - 1;
		while ( b >= 0 && m_Entries[ top[ b ] ].sequence < m_Entries[ s ].sequence )
		{
			top[ b + 1 ] = top[ b ];
			b--;
		}
		top[ b + 1 ] = s;
	}
	int chosen[ OBJECTIVE_MAX ], nChosen = 0, used = 0;
	int partsShown[ OBJECTIVE_MAX ] = {};
	for ( int t = 0; t < nTop; t++ )
	{
		if ( used >= maxLines )
		{
			more += nTop - t;
			break;
		}
		chosen[ nChosen++ ] = top[ t ];
		used++;
		if ( m_Entries[ top[ t ] ].state != OBJECTIVE_ACTIVE )
			continue;	// a completed group leaves with its parts
		int parts[ OBJECTIVE_MAX ];
		const int np = Parts( top[ t ], parts, OBJECTIVE_MAX );
		for ( int k = 0; k < np; k++ )
		{
			if ( m_Entries[ parts[ k ] ].state != OBJECTIVE_ACTIVE && RowAlpha( parts[ k ] ) <= 0.0f )
				continue;
			if ( used < maxLines )
			{
				used++;
				partsShown[ top[ t ] ]++;
			}
			else
				more++;	// a part that did not fit
		}
	}
	// in the list's order
	Sort( chosen, nChosen );
	int n = 0;
	for ( int c = 0; c < nChosen && n < maxLines; c++ )
	{
		out[ n++ ] = { chosen[ c ], false };
		if ( !partsShown[ chosen[ c ] ] )
			continue;
		int parts[ OBJECTIVE_MAX ];
		const int np = Parts( chosen[ c ], parts, OBJECTIVE_MAX );
		int left = partsShown[ chosen[ c ] ];
		for ( int k = 0; k < np && left > 0 && n < maxLines; k++ )
		{
			if ( m_Entries[ parts[ k ] ].state == OBJECTIVE_ACTIVE || RowAlpha( parts[ k ] ) > 0.0f )
			{
				out[ n++ ] = { parts[ k ], true };
				left--;
			}
		}
	}
	return n;
}

int List::FullLines( Line *out, int maxLines, int &more ) const
{
	more = 0;
	int all[ OBJECTIVE_MAX ];
	const int nAll = AllRows( all, OBJECTIVE_MAX );
	int top[ OBJECTIVE_MAX ], nTop = 0;
	// active ones first, then completed
	for ( int pass = 0; pass < 2; pass++ )
	{
		for ( int i = 0; i < nAll; i++ )
		{
			if ( IsTop( all[ i ] ) && ( m_Entries[ all[ i ] ].state == OBJECTIVE_COMPLETED ) == ( pass == 1 ) )
				top[ nTop++ ] = all[ i ];
		}
	}

	int n = 0;
	for ( int t = 0; t < nTop; t++ )
	{
		if ( n >= maxLines )
		{
			more = nTop - t;
			break;
		}
		out[ n++ ] = { top[ t ], false };
		if ( m_Entries[ top[ t ] ].state != OBJECTIVE_ACTIVE )
			continue;
		int parts[ OBJECTIVE_MAX ];
		const int np = Parts( top[ t ], parts, OBJECTIVE_MAX );
		for ( int k = 0; k < np && n < maxLines; k++ )
			out[ n++ ] = { parts[ k ], true };
	}
	return n;
}
} // namespace objective
