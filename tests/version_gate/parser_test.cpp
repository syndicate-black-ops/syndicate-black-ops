// Version gate parser test (tests/version_gate/run_test.sh): the parser of src/dlls/version_gate.cpp, built alone.
#define VERSION_GATE_TEST
#include "../../src/dlls/version_gate.cpp"

#include <stdlib.h>
#include <random>
#include <string>

static int s_fail;

static void Expect( const char *version, bool want )
{
	const bool got = VersionGate_AtLeast( version, VERSION_GATE_MIN );
	if ( got != want )
	{
		printf( "FAIL: \"%s\" -> %d, expected %d\n", version ? version : "(null)", got, want );
		s_fail++;
	}
}

int main()
{
	// let in
	Expect( "2.1.0", true );
	Expect( "2.1.0+1a2b3c4d", true );
	Expect( "2.1.0+dirty@HEAD-1a2b3c4d", true );
	Expect( "2.1.0-test.3", true );
	Expect( "2.1.1", true );
	Expect( "2.2.0", true );
	Expect( "3.0.0", true );
	Expect( "10.0.0", true );
	Expect( "02.01.00", true );

	// refuse
	Expect( nullptr, false );
	Expect( "", false );
	Expect( "1.38", false );
	Expect( "1.3.8", false );
	Expect( "1.9.9-test", false );
	Expect( "1.99.99", false );
	Expect( "2.0.0", false );
	Expect( "2.0.0+0f5efe27", false );
	Expect( "2.0.1-test.3", false );
	Expect( "2.0.99", false );
	Expect( "2", false );
	Expect( "2.0", false );
	Expect( "2.0.", false );
	Expect( "2..0", false );
	Expect( ".2.0.0", false );
	Expect( "2.0.0.1", false );
	Expect( "2.0.0 ", false );
	Expect( " 2.0.0", false );
	Expect( "v2.0.0", false );
	Expect( "-2.0.0", false );
	Expect( "2.0.-1", false );
	Expect( "99999.0.0", false );
	Expect( "2.0.99999999999999999999", false );

	// random and hostile input: must not crash (ASan/UBSan)
	std::mt19937 rng( 1234 );
	const char alphabet[] = "0123456789.+-\\\" abc\x7f\xff";
	for ( int i = 0; i < 200000; i++ )
	{
		std::string s;
		const int len = rng() % 40;
		for ( int j = 0; j < len; j++ )
			s += alphabet[ rng() % ( sizeof( alphabet ) - 1 ) ];
		VersionGate_AtLeast( s.c_str(), VERSION_GATE_MIN );
		VersionGate_AtLeast( VERSION_GATE_MIN, s.c_str() );
	}

	if ( s_fail )
	{
		printf( "version_gate: %d failures\n", s_fail );
		return 1;
	}
	printf( "version_gate: all cases passed\n" );
	return 0;
}
