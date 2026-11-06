// Offline test of the map compatibility file parsers (src/dlls/map_compat.cpp, docs/MAP_COMPAT.md), built with
// ASan/UBSan by run_test.sh: the rules file, the CRC-32, the BSP header and entity lump scan, and random / damaged
// input for all of them. Arguments: the game's maps/map_compat.txt, then optional .bsp files whose rules are
// checked against their real CRC and entity lump.
#define MAP_COMPAT_FUZZ
#include "map_compat.cpp"

#include <stdio.h>
#include <vector>
#include <string>
#include <random>

static int s_iFailures;
#define CHECK( x ) do { if ( !( x ) ) { printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); s_iFailures++; } } while ( 0 )

static std::vector<unsigned char> ReadFile( const char *path )
{
	std::vector<unsigned char> v;
	FILE *f = fopen( path, "rb" );
	if ( !f ) return v;
	unsigned char buf[65536];
	size_t n;
	while ( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 ) v.insert( v.end(), buf, buf + n );
	fclose( f );
	return v;
}

static void TestLines( void )
{
	mc_rule_t r;
	CHECK( MC_ParseLine( "", &r ) == 0 );
	CHECK( MC_ParseLine( "   // comment", &r ) == 0 );
	CHECK( MC_ParseLine( "co_x 0123abcd drop func_door * m_iObeyTriggerMode", &r ) == 1 );
	CHECK( r.action == MC_DROP && r.crc == 0x0123abcdu && r.selType == MC_SEL_ALL );
	CHECK( MC_ParseLine( "co_x 0123ABCD set func_door #12 speed \"100 0\"", &r ) == 1 );
	CHECK( r.action == MC_SET && r.selType == MC_SEL_INDEX && r.selIndex == 12 && !strcmp( r.value, "100 0" ) );
	CHECK( MC_ParseLine( "co_x 0123abcd drop func_door door1 wait", &r ) == 1 && r.selType == MC_SEL_NAME );
	CHECK( MC_ParseLine( "co_x 0123abc drop func_door * k", &r ) == -1 );		// 7 hex digits
	CHECK( MC_ParseLine( "co_x 0123abcdd drop func_door * k", &r ) == -1 );		// 9
	CHECK( MC_ParseLine( "co_x 0123abcg drop func_door * k", &r ) == -1 );
	CHECK( MC_ParseLine( "co_x 0123abcd drop func_door * k extra", &r ) == -1 );
	CHECK( MC_ParseLine( "co_x 0123abcd set func_door * k", &r ) == -1 );
	CHECK( MC_ParseLine( "co_x 0123abcd kill func_door * k", &r ) == -1 );
	CHECK( MC_ParseLine( "co_x 0123abcd drop func_door # k", &r ) == -1 );
	CHECK( MC_ParseLine( "co_x 0123abcd drop func_door #99999999999 k", &r ) == -1 );
	CHECK( MC_ParseLine( "co_x 0123abcd drop \"func_door * k", &r ) == -1 );
	CHECK( MC_ParseLine( "a b c d e f g h i", &r ) == -1 );
	std::string longTok( 200, 'x' );
	CHECK( MC_ParseLine( ( "co_x 0123abcd drop " + longTok + " * k" ).c_str(), &r ) == -1 );
	CHECK( MC_Crc32( (const unsigned char *)"123456789", 9 ) == 0xCBF43926u );	// the CRC-32 check value
}

static void TestLump( void )
{
	const char *lump =
		"{ \"classname\" \"worldspawn\" }\n"
		"{ \"classname\" \"func_door\" \"targetname\" \"d1\" }\n"
		"{ \"classname\" \"func_door\" }\n"
		"{ \"targetname\" \"d1\" \"classname\" \"func_door\" }\n";
	const char *rules =
		"m 00000000 drop func_door d1 wait\n"
		"m 00000000 drop func_door #2 wait\n"
		"m 00000000 drop func_door * wait\n";
	int bad = -1;
	CHECK( MC_ParseRules( rules, strlen( rules ), "M", &bad ) == 3 && bad == 0 );
	MC_ResolveSelectors( lump, strlen( lump ) );
	CHECK( s_Rules[0].numOrdinals == 2 && s_Rules[0].ordinals[0] == 0 && s_Rules[0].ordinals[1] == 2 );
	CHECK( s_Rules[1].numOrdinals == 1 && s_Rules[1].ordinals[0] == 1 );
	CHECK( s_Rules[2].numOrdinals == 0 );
}

static void Fuzz( const std::vector<unsigned char> &seedLump )
{
	std::mt19937 rng( 1234 );
	const char alphabet[] = "{}\" \t\r\n#*/abcdefABCDEF0123456789_-.";

	for ( int iter = 0; iter < 20000; iter++ )
	{
		size_t n = rng() % 2048;
		std::vector<char> buf( n ? n : 1 );
		for ( size_t i = 0; i < n; i++ )
			buf[i] = ( rng() % 4 ) ? alphabet[ rng() % ( sizeof( alphabet ) - 1 ) ] : (char)( rng() & 0xFF );
		int bad;
		MC_ParseRules( buf.data(), n, "abc", &bad );
		MC_ResolveSelectors( buf.data(), n );
		const char *lump; size_t lumpLen;
		if ( MC_EntityLump( (const unsigned char *)buf.data(), n, &lump, &lumpLen ) )
			MC_ResolveSelectors( lump, lumpLen );
	}

	// a real lump with damage: random bytes changed, cut short
	if ( !seedLump.empty() )
	{
		const char *rules = "m 00000000 drop func_door_rotating locked22 x\nm 00000000 drop func_door_rotating #13 x\n";
		for ( int iter = 0; iter < 2000; iter++ )
		{
			std::vector<char> lump( seedLump.begin(), seedLump.end() );
			int flips = 1 + rng() % 16;
			for ( int k = 0; k < flips; k++ )
				lump[ rng() % lump.size() ] = (char)( rng() & 0xFF );
			size_t cut = rng() % ( lump.size() + 1 );
			int bad;
			MC_ParseRules( rules, strlen( rules ), "m", &bad );
			MC_ResolveSelectors( lump.data(), cut );
		}
	}
}

int main( int argc, char **argv )
{
	TestLines();
	TestLump();

	std::vector<unsigned char> seedLump;

	if ( argc > 1 )
	{
		std::vector<unsigned char> file = ReadFile( argv[1] );
		CHECK( !file.empty() );

		for ( int a = 2; a < argc; a++ )
		{
			std::vector<unsigned char> bsp = ReadFile( argv[a] );
			std::string name = argv[a];
			size_t slash = name.find_last_of( '/' );
			if ( slash != std::string::npos ) name = name.substr( slash + 1 );
			if ( name.size() > 4 ) name = name.substr( 0, name.size() - 4 );

			int bad = 0;
			int count = MC_ParseRules( (const char *)file.data(), file.size(), name.c_str(), &bad );
			unsigned int crc = MC_Crc32( bsp.data(), bsp.size() );
			int matching = 0;
			for ( int i = 0; i < count; i++ )
				matching += s_Rules[i].crc == crc;
			printf( "%s: crc %08x, %d rule(s), %d for this file, %d malformed line(s)\n", name.c_str(), crc, count, matching, bad );
			CHECK( bad == 0 );
			CHECK( count > 0 && matching == count );

			const char *lump; size_t lumpLen;
			CHECK( MC_EntityLump( bsp.data(), bsp.size(), &lump, &lumpLen ) );
			if ( seedLump.empty() )
				seedLump.assign( lump, lump + lumpLen );

			// every name / index selector of the map's rules selects at least one entity of the real lump
			MC_ResolveSelectors( lump, lumpLen );
			for ( int i = 0; i < count; i++ )
			{
				if ( s_Rules[i].selType == MC_SEL_ALL )
					continue;
				printf( "  %s %s: %d entit%s\n", s_Rules[i].classname, s_Rules[i].selType == MC_SEL_NAME ? s_Rules[i].selName : "#",
					s_Rules[i].numOrdinals, s_Rules[i].numOrdinals == 1 ? "y" : "ies" );
				CHECK( s_Rules[i].numOrdinals > 0 );
			}

			// the corecity doors: the name selector finds the entities the database lists (locked22 twice)
			if ( name.compare( 0, 11, "co_corecity" ) == 0 )
			{
				const char *probe = "m 00000000 drop func_door_rotating locked22 x\n";
				MC_ParseRules( probe, strlen( probe ), "m", &bad );
				MC_ResolveSelectors( lump, lumpLen );
				printf( "  func_door_rotating named locked22: %d\n", s_Rules[0].numOrdinals );
				CHECK( s_Rules[0].numOrdinals == 2 );
			}
		}
	}

	Fuzz( seedLump );

	printf( s_iFailures ? "map_compat test: %d FAILED\n" : "map_compat test: OK\n", s_iFailures );
	return s_iFailures ? 1 : 0;
}
