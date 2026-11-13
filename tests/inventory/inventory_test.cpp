// Offline test of src/cl_dll/inventory_core.cpp (tests/inventory/run_test.sh, ASan/UBSan).
#include "inventory_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static int s_iFailed = 0;
#define CHECK( c ) do { if ( !( c ) ) { printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); s_iFailed++; } } while ( 0 )

static std::vector<unsigned char> Msg( int count, const std::string &name, const std::string &icon, int flags )
{
	std::vector<unsigned char> v;
	v.push_back( count & 0xFF );
	v.push_back( ( count >> 8 ) & 0xFF );
	v.insert( v.end(), name.begin(), name.end() );
	v.push_back( 0 );
	v.insert( v.end(), icon.begin(), icon.end() );
	v.push_back( 0 );
	v.push_back( flags & 0xFF );
	return v;
}

static bool Feed( inventory::Lists &l, const std::vector<unsigned char> &v )
{
	// a copy of exactly its size on the heap: ASan sees any read past the end
	unsigned char *p = (unsigned char *)malloc( v.size() ? v.size() : 1 );
	if ( !v.empty() )
		memcpy( p, v.data(), v.size() );
	inventory::Msg m;
	const bool ok = inventory::Parse( p, (int)v.size(), m );
	if ( ok )
		inventory::Apply( l, m );
	free( p );
	return ok;
}

int main()
{
	inventory::Lists l;
	inventory::Clear( l );

	// good messages: give, update, a shared one, remove
	CHECK( Feed( l, Msg( 1, "keycard_red", "inv_card_red", 0 ) ) );
	CHECK( inventory::Used( l.own ) == 1 && l.own[0].count == 1 && !strcmp( l.own[0].icon, "inv_card_red" ) );
	CHECK( Feed( l, Msg( 3, "keycard_red", "inv_card_red", 0 ) ) );
	CHECK( inventory::Used( l.own ) == 1 && l.own[0].count == 3 );
	CHECK( Feed( l, Msg( 2, "coin", "", INVENTORY_FLAG_SHARED ) ) );
	CHECK( inventory::Used( l.shared ) == 1 && l.shared[0].count == 2 && inventory::Used( l.own ) == 1 );
	CHECK( Feed( l, Msg( 1, "a", "", 0 ) ) );
	CHECK( Feed( l, Msg( 1, "b", "", 0 ) ) );
	CHECK( Feed( l, Msg( 0, "a", "", 0 ) ) );	// removed: b moves up, order kept
	CHECK( inventory::Used( l.own ) == 2 && !strcmp( l.own[0].name, "keycard_red" ) && !strcmp( l.own[1].name, "b" ) );
	CHECK( Feed( l, Msg( 0, "nothing", "", 0 ) ) );	// removing an unknown item: no change
	CHECK( inventory::Used( l.own ) == 2 );

	// counts out of range are clamped: negative -> 0 (removed), 32767 kept
	CHECK( Feed( l, Msg( -5, "b", "", 0 ) ) );
	CHECK( inventory::Used( l.own ) == 1 );
	CHECK( Feed( l, Msg( 32767, "big", "", 0 ) ) );
	CHECK( l.own[1].count == 32767 );

	// long names and icons are cut to 31 characters
	const std::string longName( 300, 'n' ), longIcon( 5000, 'i' );
	CHECK( Feed( l, Msg( 1, longName, longIcon, 0 ) ) );
	CHECK( strlen( l.own[2].name ) == INVENTORY_NAME_LEN - 1 && strlen( l.own[2].icon ) == INVENTORY_NAME_LEN - 1 );

	// at most INVENTORY_SLOTS per list
	inventory::Clear( l );
	for ( int i = 0; i < 40; i++ )
		Feed( l, Msg( 1, "item" + std::to_string( i ), "", INVENTORY_FLAG_SHARED ) );
	CHECK( inventory::Used( l.shared ) == INVENTORY_SLOTS && inventory::Used( l.own ) == 0 );

	// cut and broken messages: nothing applied
	inventory::Clear( l );
	std::vector<unsigned char> good = Msg( 4, "keycard_blue", "inv_card_blue", 0 );
	for ( size_t n = 0; n < good.size(); n++ )
		CHECK( !Feed( l, std::vector<unsigned char>( good.begin(), good.begin() + n ) ) );
	CHECK( inventory::Used( l.own ) == 0 );
	CHECK( !Feed( l, Msg( 1, "", "icon", 0 ) ) );	// no name
	{
		inventory::Msg m;
		CHECK( !inventory::Parse( NULL, 10, m ) );
		const unsigned char noEnd[] = { 1, 0, 'a', 'b', 'c' };	// unterminated string
		CHECK( !inventory::Parse( noEnd, sizeof( noEnd ), m ) );
	}
	// extra bytes after a message are ignored
	good.push_back( 0xEE );
	CHECK( Feed( l, good ) );

	// random fuzz: never a crash or an out-of-bounds access (ASan / UBSan), lists stay consistent
	srand( 12345 );
	for ( int iter = 0; iter < 200000; iter++ )
	{
		std::vector<unsigned char> v( rand() % 96 );
		for ( size_t i = 0; i < v.size(); i++ )
			v[i] = (unsigned char)( rand() % 4 ? 'a' + rand() % 4 : rand() % 256 );
		if ( iter % 3 == 0 && v.size() > 4 )
			v[2 + rand() % ( v.size() - 2 )] = 0;
		Feed( l, v );
		for ( int s = 0; s < 2; s++ )
		{
			const inventory::Item *list = s ? l.shared : l.own;
			const int n = inventory::Used( list );
			for ( int i = 0; i < INVENTORY_SLOTS; i++ )
			{
				CHECK( strlen( list[i].name ) < INVENTORY_NAME_LEN && strlen( list[i].icon ) < INVENTORY_NAME_LEN );
				if ( i < n )
					CHECK( list[i].count >= 1 && list[i].count <= INVENTORY_COUNT_MAX );
				else
					CHECK( !list[i].name[0] );
			}
		}
		if ( s_iFailed )
			break;
	}

	if ( s_iFailed )
	{
		printf( "inventory_test: %d failed\n", s_iFailed );
		return 1;
	}
	printf( "inventory_test: all passed\n" );
	return 0;
}
