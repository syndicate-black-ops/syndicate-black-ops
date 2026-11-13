// trigger_objective client core (src/cl_dll/objective_core.cpp): the "Objective" message on good, cut, hostile and
// random input; the list's order, the HUD rows, the fades and the notice queue. Run by tests/objective/run_test.sh with
// ASan/UBSan.

#include "objective_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static int s_iFailed = 0;

#define CHECK( cond ) do { if ( !( cond ) ) { printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond ); s_iFailed++; } } while ( 0 )

struct Writer
{
	std::vector<unsigned char> b;
	void Byte( int v ) { b.push_back( (unsigned char)v ); }
	void Short( int v ) { b.push_back( (unsigned char)( v & 0xFF ) ); b.push_back( (unsigned char)( ( v >> 8 ) & 0xFF ) ); }
	void String( const char *s ) { while ( *s ) b.push_back( (unsigned char)*s++ ); b.push_back( 0 ); }
};

// as the server (dlls/trigger_objective.cpp CTriggerObjective::Send) writes it
static std::vector<unsigned char> Msg( int slot, int flags, int order = 0, int seq = 1, int count = 0, int target = 0,
	const char *text = "Reach the evac lift", int parent = OBJECTIVE_NO_PARENT )
{
	Writer w;
	w.Byte( slot );
	w.Byte( flags );
	if ( ( flags & OBJECTIVE_STATE_MASK ) != OBJECTIVE_HIDDEN )
	{
		w.Short( order );
		w.Short( seq );
		w.Byte( count );
		w.Byte( target );
		w.Byte( parent );
		w.String( text );
	}
	return w.b;
}

static bool Apply( objective::List &list, const std::vector<unsigned char> &m )
{
	objective::Message msg;
	if ( !objective::Parse( m.data(), (int)m.size(), msg ) )
		return false;
	list.Apply( msg );
	return true;
}

static void TestParse()
{
	objective::Message m;
	std::vector<unsigned char> b = Msg( 5, OBJECTIVE_ACTIVE | OBJECTIVE_EV_NEW | OBJECTIVE_SILENT, 3, 7, 2, 3, "#OBJ_PUMPS" );
	CHECK( objective::Parse( b.data(), (int)b.size(), m ) );
	CHECK( m.slot == 5 && m.state == OBJECTIVE_ACTIVE && m.order == 3 && m.sequence == 7 );
	CHECK( m.count == 2 && m.target == 3 && !strcmp( m.text, "#OBJ_PUMPS" ) );
	CHECK( m.flags == ( OBJECTIVE_EV_NEW | OBJECTIVE_SILENT ) );
	CHECK( m.parent == -1 );
	b = Msg( 5, OBJECTIVE_ACTIVE, 0, 1, 0, 0, "part", 7 );
	CHECK( objective::Parse( b.data(), (int)b.size(), m ) && m.parent == 7 );
	b = Msg( 5, OBJECTIVE_ACTIVE, 0, 1, 0, 0, "self", 5 );
	CHECK( objective::Parse( b.data(), (int)b.size(), m ) && m.parent == -1 );
	b = Msg( 5, OBJECTIVE_ACTIVE, 0, 1, 0, 0, "bad", 40 );
	CHECK( objective::Parse( b.data(), (int)b.size(), m ) && m.parent == -1 );
	b = Msg( 5, OBJECTIVE_ACTIVE | OBJECTIVE_EV_NEW | OBJECTIVE_SILENT, 3, 7, 2, 3, "#OBJ_PUMPS" );

	// clear and hidden: nothing follows
	const unsigned char clear[] = { OBJECTIVE_SLOT_CLEAR };
	CHECK( objective::Parse( clear, 1, m ) && m.slot == OBJECTIVE_SLOT_CLEAR );
	const unsigned char hidden[] = { 4, OBJECTIVE_HIDDEN };
	CHECK( objective::Parse( hidden, 2, m ) && m.slot == 4 && m.state == OBJECTIVE_HIDDEN );

	// bad slot, bad state, empty, NULL
	const unsigned char badSlot[] = { OBJECTIVE_MAX, OBJECTIVE_HIDDEN };
	CHECK( !objective::Parse( badSlot, 2, m ) );
	const unsigned char badState[] = { 1, 3 };
	CHECK( !objective::Parse( badState, 2, m ) );
	CHECK( !objective::Parse( NULL, 0, m ) );
	CHECK( !objective::Parse( clear, 0, m ) );

	// every cut of a good message fails, without reading past the end (ASan: an exact-size copy)
	for ( size_t n = 1; n < b.size(); n++ )
	{
		unsigned char *cut = (unsigned char *)malloc( n );
		memcpy( cut, b.data(), n );
		CHECK( !objective::Parse( cut, (int)n, m ) );
		free( cut );
	}

	// a text longer than the buffer: cut, terminated
	std::string longText( 1000, 'x' );
	b = Msg( 0, OBJECTIVE_ACTIVE, 0, 1, 0, 0, longText.c_str() );
	CHECK( objective::Parse( b.data(), (int)b.size(), m ) );
	CHECK( strlen( m.text ) == OBJECTIVE_MAX_TEXT - 1 );

	// hostile numbers: order / sequence clamped, count above its target cut to it
	b = Msg( 1, OBJECTIVE_ACTIVE, -5, -9, 200, 3 );
	CHECK( objective::Parse( b.data(), (int)b.size(), m ) );
	CHECK( m.order == 0 && m.sequence == 0 && m.count == 3 );
}

static int Lines( const objective::List &list, bool full, int max, int *slots, bool *parts, int &more )
{
	objective::Line l[ OBJECTIVE_MAX ];
	const int n = full ? list.FullLines( l, max, more ) : list.CompactLines( l, max, more );
	for ( int i = 0; i < n; i++ )
	{
		slots[ i ] = l[ i ].slot;
		parts[ i ] = l[ i ].part;
	}
	return n;
}

static void TestList()
{
	objective::List list;
	int slot, kind;
	float alpha;
	int sl[ OBJECTIVE_MAX ], more;
	bool pa[ OBJECTIVE_MAX ];

	// hidden -> active (NEW notice) -> completed (DONE notice, fades from the compact list)
	CHECK( Apply( list, Msg( 0, OBJECTIVE_ACTIVE | OBJECTIVE_EV_NEW, 0, 1 ) ) );
	CHECK( list.ActiveCount() == 1 );
	CHECK( list.Tick( 0.05f, true ) );	// the notice begins: sound
	CHECK( list.Notice( slot, kind, alpha ) && slot == 0 && kind == objective::NOTICE_NEW && alpha == 0.0f );
	CHECK( !list.Tick( 0.1f, true ) );
	CHECK( list.Notice( slot, kind, alpha ) && alpha > 0.0f && alpha < 1.0f );

	// the notice waits while the panel area can't be seen
	const float clock = list.Clock();
	CHECK( !list.Tick( 10.0f, false ) );
	CHECK( list.Clock() == clock );
	for ( int i = 0; i < 40; i++ )
		list.Tick( 0.1f, true );
	CHECK( !list.Notice( slot, kind, alpha ) );

	CHECK( Apply( list, Msg( 0, OBJECTIVE_COMPLETED | OBJECTIVE_EV_DONE, 0, 1 ) ) );
	CHECK( list.Tick( 0.05f, true ) );
	CHECK( list.Notice( slot, kind, alpha ) && slot == 0 && kind == objective::NOTICE_DONE );
	CHECK( Lines( list, false, 5, sl, pa, more ) == 1 && list.RowAlpha( 0 ) == 1.0f );
	for ( float t = 0.0f; t < objective::DONE_HOLD + objective::DONE_FADE * 0.5f - 0.01f; t += 0.25f )
		list.Tick( 0.25f, true );
	CHECK( list.RowAlpha( 0 ) > 0.0f && list.RowAlpha( 0 ) < 1.0f );
	list.Tick( 1.0f, true );
	list.Tick( 1.0f, true );
	CHECK( list.RowAlpha( 0 ) == 0.0f );
	CHECK( Lines( list, false, 5, sl, pa, more ) == 0 );
	CHECK( Lines( list, true, 20, sl, pa, more ) == 1 );

	// a silent one: no notice, also not when completed
	CHECK( Apply( list, Msg( 1, OBJECTIVE_ACTIVE | OBJECTIVE_EV_NEW | OBJECTIVE_SILENT, 0, 2 ) ) );
	CHECK( !list.Tick( 0.1f, true ) );
	CHECK( Apply( list, Msg( 1, OBJECTIVE_COMPLETED | OBJECTIVE_EV_DONE | OBJECTIVE_SILENT, 0, 2 ) ) );
	CHECK( !list.Tick( 0.1f, true ) );
	CHECK( !list.Notice( slot, kind, alpha ) );

	// a resend of a completed objective (late joiner): not on the compact list, in the full list
	CHECK( Apply( list, Msg( 2, OBJECTIVE_COMPLETED, 0, 0 ) ) );
	CHECK( list.RowAlpha( 2 ) == 0.0f );

	// groups: group 10 (order 0, seq 5) with parts 11 (order 2), 12 (order 1, a counter), 13 completed;
	// a plain objective 20 (order 1) comes first
	list.Reset();
	Apply( list, Msg( 10, OBJECTIVE_ACTIVE, 0, 5, 1, 3, "Open Door A" ) );
	Apply( list, Msg( 11, OBJECTIVE_ACTIVE, 2, 6, 0, 0, "Find the fuse", 10 ) );
	Apply( list, Msg( 12, OBJECTIVE_ACTIVE, 1, 7, 4, 10, "Kill 10 Feds", 10 ) );
	Apply( list, Msg( 13, OBJECTIVE_COMPLETED, 3, 8, 0, 0, "Read the note", 10 ) );
	Apply( list, Msg( 20, OBJECTIVE_ACTIVE, 1, 9, 0, 0, "First" ) );
	CHECK( list.ActiveCount() == 2 );
	int done = -1;
	CHECK( list.TopCount( done ) == 2 && done == 0 );
	CHECK( list.NewestActive() == 20 );
	int n = Lines( list, false, 5, sl, pa, more );
	CHECK( n == 4 && more == 0 );
	CHECK( n == 4 && sl[ 0 ] == 20 && !pa[ 0 ] && sl[ 1 ] == 10 && !pa[ 1 ] && sl[ 2 ] == 12 && pa[ 2 ] && sl[ 3 ] == 11 );
	n = Lines( list, true, 20, sl, pa, more );
	CHECK( n == 5 && sl[ 4 ] == 13 && pa[ 4 ] );	// the full list has the completed part too
	// caps: the newest get the lines first, shown in the list's order; parts that do not fit count as more
	n = Lines( list, false, 2, sl, pa, more );
	CHECK( n == 2 && sl[ 0 ] == 20 && sl[ 1 ] == 10 && more == 2 );	// the group's two parts did not fit
	n = Lines( list, false, 1, sl, pa, more );
	CHECK( n == 1 && more == 1 );
	// the group completed: it and its parts leave the compact list after the fade; the full list has it after the
	// active ones
	Apply( list, Msg( 10, OBJECTIVE_COMPLETED | OBJECTIVE_EV_DONE, 0, 5, 3, 3, "Open Door A" ) );
	n = Lines( list, false, 5, sl, pa, more );
	CHECK( n == 2 && sl[ 1 ] == 10 );
	n = Lines( list, true, 20, sl, pa, more );
	CHECK( n == 2 && sl[ 0 ] == 20 && sl[ 1 ] == 10 );
	// a part whose group is not known is listed on its own
	Apply( list, Msg( 30, OBJECTIVE_ACTIVE, 0, 10, 0, 0, "Orphan", 31 ) );
	CHECK( list.ActiveCount() == 2 );

	// hidden again (a round reset): gone; clear: everything gone
	Apply( list, Msg( 20, OBJECTIVE_HIDDEN ) );
	CHECK( !list.Get( 20 ).used );
	const int rev = list.Revision();
	const unsigned char clear[] = { OBJECTIVE_SLOT_CLEAR };
	objective::Message m;
	CHECK( objective::Parse( clear, 1, m ) );
	list.Apply( m );
	CHECK( list.ShownCount() == 0 && list.Revision() != rev );

	// the notice queue: more than it holds at once
	list.Reset();
	for ( int i = 0; i < OBJECTIVE_MAX; i++ )
		Apply( list, Msg( i, OBJECTIVE_ACTIVE | OBJECTIVE_EV_NEW, 0, i + 1 ) );
	int notices = 0;
	for ( int i = 0; i < 2000; i++ )
		notices += list.Tick( 0.05f, true );
	CHECK( notices == objective::NOTICE_QUEUE );
	CHECK( !list.Notice( slot, kind, alpha ) );

	// a notice whose objective is hidden before it is shown is dropped
	list.Reset();
	Apply( list, Msg( 3, OBJECTIVE_ACTIVE | OBJECTIVE_EV_NEW, 0, 1 ) );
	Apply( list, Msg( 3, OBJECTIVE_HIDDEN ) );
	CHECK( !list.Tick( 0.1f, true ) );

	// many active objectives: the full list caps and says how many more
	list.Reset();
	for ( int i = 0; i < OBJECTIVE_MAX; i++ )
		Apply( list, Msg( i, OBJECTIVE_ACTIVE, 0, i + 1 ) );
	n = Lines( list, true, 10, sl, pa, more );
	CHECK( n == 10 && more == OBJECTIVE_MAX - 10 );
	n = Lines( list, false, objective::COMPACT_LINES, sl, pa, more );
	CHECK( n == objective::COMPACT_LINES && more == OBJECTIVE_MAX - objective::COMPACT_LINES );
}

// random and mutated messages: no crash, no sanitizer report, the list stays in bounds
static void TestFuzz()
{
	srand( 1234 );
	objective::List list;
	int rows[ OBJECTIVE_MAX ];
	for ( int i = 0; i < 200000; i++ )
	{
		std::vector<unsigned char> b;
		if ( i & 1 )
		{
			const int len = rand() % 160;
			for ( int k = 0; k < len; k++ )
				b.push_back( (unsigned char)rand() );
		}
		else
		{
			b = Msg( rand() % 40, rand() & 0xFF, rand(), rand(), rand() & 0xFF, rand() & 0xFF, "fuzz", rand() % 40 );
			if ( !b.empty() && ( rand() & 3 ) == 0 )
				b[ rand() % b.size() ] = (unsigned char)rand();
			if ( !b.empty() && ( rand() & 3 ) == 0 )
				b.resize( rand() % b.size() );
		}
		unsigned char *copy = (unsigned char *)malloc( b.size() + 1 );
		if ( !b.empty() )
			memcpy( copy, b.data(), b.size() );
		objective::Message m;
		if ( objective::Parse( copy, (int)b.size(), m ) )
		{
			CHECK( m.slot == OBJECTIVE_SLOT_CLEAR || ( m.slot >= 0 && m.slot < OBJECTIVE_MAX ) );
			CHECK( strlen( m.text ) < OBJECTIVE_MAX_TEXT );
			list.Apply( m );
		}
		free( copy );
		list.Tick( ( rand() % 100 ) / 100.0f, rand() & 1 );
		objective::Line lines[ OBJECTIVE_MAX ];
		int more = 0;
		for ( int full = 0; full < 2; full++ )
		{
			const int max = rand() % ( OBJECTIVE_MAX + 1 );
			const int n = full ? list.FullLines( lines, max, more ) : list.CompactLines( lines, max, more );
			CHECK( n >= 0 && n <= max && more >= 0 && more <= OBJECTIVE_MAX );
			for ( int k = 0; k < n; k++ )
				CHECK( lines[ k ].slot >= 0 && lines[ k ].slot < OBJECTIVE_MAX );
		}
		const int nr = list.AllRows( rows, OBJECTIVE_MAX );
		CHECK( nr >= 0 && nr <= OBJECTIVE_MAX );
		int slot, kind;
		float alpha;
		if ( list.Notice( slot, kind, alpha ) )
			CHECK( slot >= 0 && slot < OBJECTIVE_MAX && alpha >= 0.0f && alpha <= 1.0f );
	}
}

int main()
{
	TestParse();
	TestList();
	TestFuzz();
	if ( s_iFailed )
	{
		printf( "objective_test: %d check(s) failed\n", s_iFailed );
		return 1;
	}
	printf( "objective_test: all checks passed\n" );
	return 0;
}
