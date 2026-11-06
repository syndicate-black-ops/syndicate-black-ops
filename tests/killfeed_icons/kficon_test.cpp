// tests/killfeed_icons/run_test.sh: KfIcon_ValidCode / KfIcon_ValidPath (game_shared/killfeed_icons_shared.h)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "killfeed_icons_shared.h"

static int s_fail = 0;
#define CHECK(c) do { if ( !(c) ) { printf( "FAIL %s:%d %s\n", __FILE__, __LINE__, #c ); s_fail++; } } while ( 0 )

int main()
{
	CHECK( KfIcon_ValidCode( "quadbarrel" ) );
	CHECK( KfIcon_ValidCode( "hhgrenade" ) );
	CHECK( KfIcon_ValidCode( "rpg_rocket" ) );
	CHECK( KfIcon_ValidCode( "a-b_C9" ) );
	CHECK( KfIcon_ValidCode( std::string( KFICON_CODE_MAX, 'x' ).c_str() ) );
	CHECK( !KfIcon_ValidCode( std::string( KFICON_CODE_MAX + 1, 'x' ).c_str() ) );
	CHECK( !KfIcon_ValidCode( "" ) );
	CHECK( !KfIcon_ValidCode( nullptr ) );
	CHECK( !KfIcon_ValidCode( "gluon gun" ) );
	CHECK( !KfIcon_ValidCode( "a\"b" ) );
	CHECK( !KfIcon_ValidCode( "a;quit" ) );
	CHECK( !KfIcon_ValidCode( "\xff\xfe" ) );

	CHECK( KfIcon_ValidPath( "sprites/sbo_kf_custom_v1.spr" ) );
	CHECK( KfIcon_ValidPath( "sprites/a/b.SPR" ) );
	CHECK( KfIcon_ValidPath( "sprites/x.spr" ) );
	CHECK( !KfIcon_ValidPath( "sprites/.spr" ) );
	CHECK( !KfIcon_ValidPath( "sprites/../liblist.gam.spr" ) );
	CHECK( !KfIcon_ValidPath( "sprites\\x.spr" ) );
	CHECK( !KfIcon_ValidPath( "models/x.spr" ) );
	CHECK( !KfIcon_ValidPath( "sprites/x.txt" ) );
	CHECK( !KfIcon_ValidPath( "sprites/x y.spr" ) );
	CHECK( !KfIcon_ValidPath( "sprites/c:x.spr" ) );
	CHECK( !KfIcon_ValidPath( "sprit" ) );
	CHECK( !KfIcon_ValidPath( "" ) );
	CHECK( !KfIcon_ValidPath( nullptr ) );
	std::string longp = "sprites/" + std::string( KFICON_PATH_LEN - 12, 'a' ) + ".spr";	// 64 characters
	CHECK( (int)longp.size() == KFICON_PATH_LEN );
	CHECK( !KfIcon_ValidPath( longp.c_str() ) );
	longp = "sprites/" + std::string( KFICON_PATH_LEN - 13, 'a' ) + ".spr";	// 63 characters: fits with its NUL
	CHECK( KfIcon_ValidPath( longp.c_str() ) );

	// random input: no read past the end (ASan), and every accepted value fits the buffers
	srand( 47 );
	for ( int i = 0; i < 200000; i++ )
	{
		const int n = rand() % 80;
		char *buf = (char *)malloc( n + 1 );
		for ( int k = 0; k < n; k++ )
			buf[k] = (char)( 1 + rand() % 255 );
		buf[n] = 0;
		if ( i & 1 )
			memcpy( buf, "sprites/", n < 8 ? n : 8 );
		if ( KfIcon_ValidCode( buf ) )
			CHECK( strlen( buf ) <= KFICON_CODE_MAX );
		if ( KfIcon_ValidPath( buf ) )
			CHECK( strlen( buf ) < KFICON_PATH_LEN );
		free( buf );
	}

	CHECK( KFICON_MSG_MAX <= 192 );
	printf( s_fail ? "FAILED: %d\n" : "PASS\n", s_fail );
	return s_fail ? 1 : 0;
}
