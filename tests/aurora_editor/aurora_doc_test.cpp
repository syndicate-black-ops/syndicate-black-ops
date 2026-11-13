// Offline test of the particle editor's script document (src/cl_dll/aurora/aurora_doc.cpp): it reads the keys and
// types the game reads (the block rules of particlesys.cpp), a change rewrites only what it touches (comments and
// other keys stay), adding / removing / renaming / moving types keeps the text valid, every shipped script survives
// a round of edits, and random text never reads out of bounds (ASan/UBSan, tests/aurora_editor/run_test.sh).

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "aurora_doc.h"
#include "aurora_keys.h"

using namespace auroradoc;

static int s_iFailed = 0;
#define CHECK( cond ) do { if ( !( cond ) ) { printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond ); s_iFailed++; } } while ( 0 )

static unsigned s_Seed = 4242;
static unsigned Rand()
{
	s_Seed = s_Seed * 1103515245u + 12345u;
	return ( s_Seed >> 8 ) & 0xffffff;
}

static bool Has( const Doc &d, int block, const char *key, const char *value )
{
	const std::string *v = d.Get( block, key );
	return v && *v == value;
}

static void TestParse()
{
	Doc d;
	d.SetText( "// comment \"particles\" \"5\"\n\"particles\" \"100\"\n\"maintype\" \"a\"\n{\n\"name\" \"a\" // x\n"
	           "\"spraytype\" \"b\"\n}\n{ \"name\" \"b\" \"startsize\" 14\" \"lifetime\" \"1..2\" }\n" );
	CHECK( d.TypeCount() == 2 );
	CHECK( Has( d, -1, "particles", "100" ) );
	CHECK( Has( d, -1, "MAINTYPE", "a" ) );
	CHECK( d.TypeName( 0 ) == "a" && d.TypeName( 1 ) == "b" );
	CHECK( Has( d, 1, "startsize", "14\"" ) ); // a quote inside a word is part of it (Spirit's sparker.aur)
	CHECK( Has( d, 1, "lifetime", "1..2" ) );

	// a key with no value before a brace is skipped, the brace keeps the structure
	d.SetText( "{ \"name\" \"a\" \"gravity\" }{ \"name\" \"b\" }" );
	CHECK( d.TypeCount() == 2 && d.TypeName( 1 ) == "b" && !d.Get( 0, "gravity" ) );
	// an unclosed block ends at the next '{'
	d.SetText( "{ \"name\" \"a\"\n{ \"name\" \"b\" }" );
	CHECK( d.TypeCount() == 2 && d.TypeName( 0 ) == "a" && d.Type( 0 ).close == std::string::npos );
	// the last of a key counts
	d.SetText( "{ \"name\" \"a\" \"name\" \"c\" }" );
	CHECK( d.TypeName( 0 ) == "c" && d.FindType( "C" ) == 0 && d.FindType( "a" ) < 0 );
	// an unterminated quote ends at the end
	d.SetText( "\"particles\" \"10" );
	CHECK( Has( d, -1, "particles", "10" ) );
	d.SetText( "" );
	CHECK( d.TypeCount() == 0 && d.TopPairs().empty() );
}

static void TestEdits()
{
	Doc d;
	const std::string base = "// top comment\n\"particles\" \"100\"\n\n{\n\t\"name\" \"a\" // keep me\n\t\"gravity\" \"40\"\n}\n";
	d.SetText( base );
	d.Set( 0, "gravity", "-300" );
	CHECK( d.Text() == "// top comment\n\"particles\" \"100\"\n\n{\n\t\"name\" \"a\" // keep me\n\t\"gravity\" \"-300\"\n}\n" );
	d.Set( 0, "lifetime", "2" );
	CHECK( Has( d, 0, "lifetime", "2" ) && d.Text().find( "\t\"lifetime\" \"2\"\n}" ) != std::string::npos );
	d.Remove( 0, "lifetime" );
	d.Set( 0, "gravity", "40" );
	CHECK( d.Text() == base );
	d.Set( -1, "maintype", "a" );
	CHECK( Has( d, -1, "maintype", "a" ) && d.Text().find( "\"particles\" \"100\"\n\"maintype\" \"a\"" ) != std::string::npos );
	d.Remove( -1, "maintype" );
	CHECK( d.Text() == base );
	// a top-level key into a script with none: before the first type
	d.SetText( "{ \"name\" \"a\" }\n" );
	d.Set( -1, "particles", "7" );
	CHECK( Has( d, -1, "particles", "7" ) && d.TypeCount() == 1 && d.Text().find( "\"particles\" \"7\"" ) == 0 );
	// quotes and line breaks never get into a value
	d.Set( 0, "sprite", "spr\"ites/x\n.spr" );
	CHECK( Has( d, 0, "sprite", "sprites/x.spr" ) );
	// a block on one line
	d.SetText( "{ \"name\" \"a\" }" );
	d.Set( 0, "gravity", "1" );
	CHECK( Has( d, 0, "gravity", "1" ) && d.TypeCount() == 1 );
	d.Remove( 0, "gravity" );
	CHECK( !d.Get( 0, "gravity" ) && d.TypeName( 0 ) == "a" );
	// an unclosed block
	d.SetText( "{ \"name\" \"a\"" );
	d.Set( 0, "gravity", "1" );
	CHECK( Has( d, 0, "gravity", "1" ) );
	// duplicates: Set changes the last, Remove takes all
	d.SetText( "{ \"name\" \"a\" \"gravity\" \"1\" \"gravity\" \"2\" }" );
	d.Set( 0, "gravity", "3" );
	CHECK( d.Text() == "{ \"name\" \"a\" \"gravity\" \"1\" \"gravity\" \"3\" }" );
	d.Remove( 0, "gravity" );
	CHECK( !d.Get( 0, "gravity" ) );
}

static void TestTypes()
{
	Doc d;
	d.SetText( NewScriptText() );
	CHECK( d.TypeCount() == 2 && d.FindType( "puff" ) == 1 && Has( d, -1, "maintype", "source" ) );
	const int c = d.AddType( d.UniqueTypeName( "puff" ) );
	CHECK( c == 2 && d.TypeName( 2 ) == "puff2" );
	d.SwapTypes( 1 );
	CHECK( d.TypeName( 1 ) == "puff2" && d.TypeName( 2 ) == "puff" && Has( d, 2, "sprite", "sprites/steam1.spr" ) );
	d.SwapTypes( 1 );
	CHECK( d.TypeName( 1 ) == "puff" && d.TypeName( 2 ) == "puff2" );
	d.RenameType( 1, "smoke" );
	CHECK( d.TypeName( 1 ) == "smoke" && Has( d, 0, "spraytype", "smoke" ) );
	d.RenameType( 0, "emitter" );
	CHECK( Has( d, -1, "maintype", "emitter" ) );
	d.DeleteType( 2 );
	CHECK( d.TypeCount() == 2 && d.FindType( "puff2" ) < 0 );
	d.DeleteType( 0 );
	CHECK( d.TypeCount() == 1 && d.TypeName( 0 ) == "smoke" );
	d.DeleteType( 0 );
	CHECK( d.TypeCount() == 0 && Has( d, -1, "particles", "60" ) );
	d.SetText( "" );
	CHECK( d.AddType( "x" ) == 0 && d.TypeName( 0 ) == "x" );
}

static void TestNumbers()
{
	CHECK( FormatNumber( 0.5f, 2 ) == "0.5" );
	CHECK( FormatNumber( 40.0f, 0 ) == "40" );
	CHECK( FormatNumber( -300.0f, 1 ) == "-300" );
	CHECK( FormatNumber( -0.0001f, 2 ) == "0" );
	CHECK( FormatNumber( NAN, 2 ) == "0" );
	float a, b;
	bool r;
	CHECK( ParseRange( "0..360", a, b, r ) && r && a == 0 && b == 360 );
	CHECK( ParseRange( "40..-40", a, b, r ) && r && a == 40 && b == -40 );
	CHECK( ParseRange( " 2.5", a, b, r ) && !r && a == 2.5f && b == 2.5f );
	CHECK( !ParseRange( "abc", a, b, r ) && a == 0 );
	CHECK( ParseRange( "1e999", a, b, r ) && a == 0 );
	char name[64];
	CHECK( CleanScriptName( "My_Smoke-2", name, sizeof( name ) ) && !strcmp( name, "my_smoke-2" ) );
	CHECK( !CleanScriptName( "", name, sizeof( name ) ) );
	CHECK( !CleanScriptName( "../x", name, sizeof( name ) ) );
	CHECK( !CleanScriptName( "a b", name, sizeof( name ) ) );
	CHECK( !CleanScriptName( "0123456789012345678901234567890123456789", name, sizeof( name ) ) );
}

static void TestKeyTable()
{
	// every key once per scope, sliders with a span, enums with choices
	for ( int i = 0; i < AURORA_TYPE_KEY_COUNT; i++ )
	{
		const AuroraKeyDef &k = AURORA_TYPE_KEYS[i];
		for ( int j = i + 1; j < AURORA_TYPE_KEY_COUNT; j++ )
			CHECK( strcmp( k.key, AURORA_TYPE_KEYS[j].key ) );
		if ( k.kind == AURKEY_RANGE || k.kind == AURKEY_INT )
			CHECK( k.uiMax > k.uiMin );
		if ( k.kind == AURKEY_ENUM )
			CHECK( k.choices && k.choices[0] != 0 );
		bool group = false;
		for ( int g = 0; g < AURORA_KEY_GROUP_COUNT; g++ )
			group = group || !strcmp( k.group, AURORA_KEY_GROUPS[g].id );
		CHECK( group );
	}
	CHECK( AURORA_TOP_KEY_COUNT == 4 && AURORA_TYPE_KEY_COUNT >= 30 );
}

static std::string ReadFile( const char *path )
{
	std::string s;
	FILE *f = fopen( path, "rb" );
	if ( !f )
		return s;
	char buf[4096];
	size_t n;
	while ( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 )
		s.append( buf, n );
	fclose( f );
	return s;
}

// a shipped script: setting every key it has to its own value changes nothing; a full round of edits stays valid
static void TestScript( const char *path )
{
	const std::string text = ReadFile( path );
	if ( text.empty() )
	{
		printf( "FAIL can't read %s\n", path );
		s_iFailed++;
		return;
	}
	Doc d;
	d.SetText( text );
	const int types = d.TypeCount();
	CHECK( types > 0 );
	for ( int b = -1; b < d.TypeCount(); b++ )
	{
		const std::vector<Pair> pairs = b < 0 ? d.TopPairs() : d.Type( b ).pairs;
		for ( const Pair &p : pairs )
		{
			const std::string *v = d.Get( b, p.key.c_str() );
			if ( v && v->find( '"' ) == std::string::npos )
				d.Set( b, p.key.c_str(), *v );
		}
	}
	// only quotes may have been added (unquoted words are written quoted)
	std::string a, c;
	for ( char ch : text )
		if ( ch != '"' )
			a += ch;
	for ( char ch : d.Text() )
		if ( ch != '"' )
			c += ch;
	if ( a != c )
	{
		printf( "FAIL %s: setting the keys to their own values changed the text\n", path );
		s_iFailed++;
	}
	for ( int b = 0; b < d.TypeCount(); b++ )
	{
		d.Set( b, "gravity", "12.5" );
		d.Set( b, "drawcondition", "water" );
		d.Remove( b, "drawcondition" );
	}
	d.Set( -1, "particles", "321" );
	CHECK( d.TypeCount() == types );
	for ( int b = 0; b < d.TypeCount(); b++ )
		CHECK( Has( d, b, "gravity", "12.5" ) && !d.Get( b, "drawcondition" ) );
	CHECK( Has( d, -1, "particles", "321" ) );
	const int n = d.AddType( d.UniqueTypeName( "extra" ) );
	d.SwapTypes( n - 1 );
	CHECK( d.TypeCount() == types + 1 && d.TypeName( n - 1 ).compare( 0, 5, "extra" ) == 0 );
	d.DeleteType( n - 1 );
	CHECK( d.TypeCount() == types );
}

// random text and random edits: no read out of bounds, the structure stays consistent (in broken text an edit may
// read differently, as the game would read it: a stray quote or an empty key shifts the pairs)
static void TestFuzz( int rounds )
{
	static const char *const parts[] = { "{", "}", "\"", "\"name\"", "\"a\"", "\"gravity\"", "\"1..2\"", " ", "\n",
		"//", "x", "(", ":", "\"maintype\"", "\"spraytype\"", "\t", "\xff", "..", "\"\"" };
	for ( int r = 0; r < rounds; r++ )
	{
		std::string t;
		const int n = Rand() % 40;
		for ( int i = 0; i < n; i++ )
			t += parts[Rand() % ( sizeof( parts ) / sizeof( parts[0] ) )];
		Doc d;
		d.SetText( t );
		for ( int e = 0; e < 8; e++ )
		{
			const int b = (int)( Rand() % ( d.TypeCount() + 1 ) ) - 1;
			switch ( Rand() % 7 )
			{
			case 0: d.Set( b, "gravity", "5" ); break;
			case 1: d.Remove( b, "name" ); break;
			case 2: d.AddType( "t" ); break;
			case 3: if ( b >= 0 ) d.DeleteType( b ); break;
			case 4: if ( b >= 0 ) d.SwapTypes( b ); break;
			case 5: if ( b >= 0 ) d.RenameType( b, "r" ); break;
			default: d.Set( b, "maintype", "a" ); break;
			}
			for ( int k = 0; k < d.TypeCount(); k++ )
			{
				const Block &blk = d.Type( k );
				CHECK( blk.open < d.Text().size() && blk.end <= d.Text().size() );
				for ( const Pair &p : blk.pairs )
					CHECK( p.keyBeg < p.valBeg && p.valEnd <= d.Text().size() );
			}
		}
	}
}

int main( int argc, char **argv )
{
	TestParse();
	TestEdits();
	TestTypes();
	TestNumbers();
	TestKeyTable();
	int scripts = 0;
	for ( int i = 1; i < argc; i++ )
	{
		TestScript( argv[i] );
		scripts++;
	}
	TestFuzz( 20000 );
	if ( s_iFailed )
	{
		printf( "aurora editor: %d checks FAILED\n", s_iFailed );
		return 1;
	}
	printf( "aurora editor: all checks passed (%d scripts, fuzz 20000)\n", scripts );
	return 0;
}
