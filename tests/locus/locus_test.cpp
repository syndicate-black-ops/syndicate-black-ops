// Offline test of the Locus value parsing (src/dlls/locus.cpp built with LOCUS_FUZZ on tests/locus/stubs), with
// ASan/UBSan (run_test.sh): literal numbers and vectors, "a..b" ranges, entity names, calc entities that refer to
// each other in a loop, the clamp's wrap / bounce with huge values and empty ranges, and random hostile strings.
#include "locus.cpp"

#include <stdarg.h>
#include <random>
#include <string>

const char *g_pStringBase = "\0";
static std::mt19937 s_Rng( 99 );
static int s_iFailures;
#define CHECK( x ) do { if ( !( x ) ) { printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x ); s_iFailures++; } } while ( 0 )

void ALERT( ALERT_TYPE, const char *fmt, ... ) { char buf[1024]; va_list a; va_start( a, fmt ); vsnprintf( buf, sizeof( buf ), fmt, a ); va_end( a ); }
float RANDOM_FLOAT( float lo, float hi ) { return lo + ( hi - lo ) * ( s_Rng() % 1000 ) / 1000.0f; }

void UTIL_StringToVector( float *pVector, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int j;
	strncpy( tempString, pString, sizeof( tempString ) );
	tempString[ sizeof( tempString ) - 1 ] = '\0';
	pstr = pfront = tempString;
	for ( j = 0; j < 3; j++ )
	{
		pVector[j] = atof( pfront );
		while ( *pstr && *pstr != ' ' ) pstr++;
		if ( !*pstr ) break;
		pstr++;
		pfront = pstr;
	}
	if ( j < 2 )
		for ( j = j + 1; j < 3; j++ ) pVector[j] = 0;
}

// test entities: "num7" reports 7, "loopA" / "loopB" ask each other, "nan" reports NaN, "self" asks itself
class CTestEnt : public CBaseEntity
{
public:
	const char *name;
	BOOL CalcRatio( CBaseEntity *pLocus, float *out )
	{
		if ( !strcmp( name, "num7" ) ) { *out = 7; return TRUE; }
		if ( !strcmp( name, "nan" ) ) { *out = NAN; return TRUE; }
		if ( !strcmp( name, "loopA" ) ) return TryCalcLocus_Ratio( pLocus, "loopB", *out );
		if ( !strcmp( name, "loopB" ) ) return TryCalcLocus_Ratio( pLocus, "loopA..loopA", *out );
		if ( !strcmp( name, "self" ) ) return TryCalcLocus_Ratio( pLocus, "self", *out );
		return FALSE;
	}
	BOOL CalcPosition( CBaseEntity *pLocus, Vector *out ) { return TryCalcLocus_Position( this, pLocus, "loopA", *out ); }
};

static entvars_t s_vars[5];
static CTestEnt s_ents[5];
static const char *s_names[5] = { "num7", "nan", "loopA", "loopB", "self" };

CBaseEntity *UTIL_FindEntityByTargetname( CBaseEntity *pStart, const char *szName, CBaseEntity *, CBaseEntity * )
{
	if ( pStart ) return NULL;
	for ( int i = 0; i < 5; i++ )
		if ( !strcmp( szName, s_names[i] ) ) return &s_ents[i];
	return NULL;
}

// a string table for ClampToMinMax's string_t arguments
static std::string s_Table;
static string_t S( const char *s ) { s_Table += '\0'; size_t at = s_Table.size(); s_Table += s; s_Table += '\0'; g_pStringBase = s_Table.data(); return (string_t)at; }

int main()
{
	s_Table.reserve( 1 << 20 );
	s_Table.assign( 1, '\0' );
	g_pStringBase = s_Table.data();
	for ( int i = 0; i < 5; i++ ) { s_ents[i].pev = &s_vars[i]; s_ents[i].name = s_names[i]; s_ents[i].m_iIndex = i + 1; }

	float f = -1;
	Vector v;
	CHECK( TryCalcLocus_Ratio( NULL, "5", f ) && f == 5 );
	CHECK( TryCalcLocus_Ratio( NULL, "-2.5", f ) && f == -2.5f );
	CHECK( TryCalcLocus_Ratio( NULL, "num7", f ) && f == 7 );
	CHECK( TryCalcLocus_Ratio( NULL, "1..3", f ) && f >= 1 && f <= 3 );
	CHECK( TryCalcLocus_Ratio( NULL, "num7..num7", f ) && f == 7 );
	CHECK( !TryCalcLocus_Ratio( NULL, "missing", f, FALSE ) );
	CHECK( !TryCalcLocus_Ratio( NULL, "nan", f ) );				// NaN is no value
	CHECK( !TryCalcLocus_Ratio( NULL, "loopA", f ) );			// a loop ends
	CHECK( !TryCalcLocus_Ratio( NULL, "self", f ) );
	CHECK( !TryCalcLocus_Position( NULL, NULL, "loopA", v ) );	// position -> ratio loop
	CHECK( !TryCalcLocus_Ratio( NULL, "1e39", f ) );			// infinite
	std::string longLeft( 300, 'a' );
	CHECK( !TryCalcLocus_Ratio( NULL, ( longLeft + "..5" ).c_str(), f ) );	// Featureful overflowed here
	CHECK( TryCalcLocus_Position( NULL, NULL, "1 2 3", v ) && v.x == 1 && v.y == 2 && v.z == 3 );
	CHECK( TryCalcLocus_Velocity( NULL, NULL, "0 0 0 .. 10 10 10", v ) && v.x >= 0 && v.x <= 10 );

	// clamp
	f = 1e30f; CHECK( ClampToMinMax( NULL, f, S( "0" ), S( "1" ), 1 ) && f >= 0 && f <= 1 );		// wrap, huge
	f = -1e30f; CHECK( ClampToMinMax( NULL, f, S( "0" ), S( "1" ), 1 ) && f >= 0 && f <= 1 );
	f = 5; CHECK( ClampToMinMax( NULL, f, S( "2" ), S( "2" ), 1 ) && f == 2 );						// empty range
	f = 1e30f; CHECK( ClampToMinMax( NULL, f, S( "0" ), S( "1" ), 2 ) && f >= 0 && f <= 1 );		// bounce, huge
	f = 12; CHECK( ClampToMinMax( NULL, f, S( "0" ), S( "10" ), 2 ) && f == 8 );					// Featureful's bounce
	f = 5; CHECK( ClampToMinMax( NULL, f, S( "3" ), S( "1" ), 1 ) );								// reversed range
	f = 5; CHECK( !ClampToMinMax( NULL, f, S( "loopA" ), S( "1" ), 0 ) );

	// random hostile strings
	const char alphabet[] = "0123456789.-+e ~*!abcAloopBnumself";
	for ( int iter = 0; iter < 200000; iter++ )
	{
		size_t n = s_Rng() % 300;
		std::string s;
		for ( size_t i = 0; i < n; i++ )
			s += ( s_Rng() % 8 ) ? alphabet[ s_Rng() % ( sizeof( alphabet ) - 1 ) ] : (char)( 1 + s_Rng() % 255 );
		if ( iter % 7 == 0 ) s = "loopA" + s;
		if ( iter % 11 == 0 ) s = s + "..num7";
		float r;
		TryCalcLocus_Ratio( NULL, s.c_str(), r, FALSE );
		TryCalcLocus_Position( NULL, NULL, s.c_str(), v, FALSE );
		TryCalcLocus_Velocity( NULL, NULL, s.c_str(), v, FALSE );
		TryCalcLocus_Color( NULL, NULL, s.c_str(), v, FALSE );
		float x = ( s_Rng() % 2 ) ? (float)( s_Rng() ) * 1e20f : (float)( (int)s_Rng() ) / 1000.0f;
		char lo[32], hi[32];
		snprintf( lo, sizeof( lo ), "%d", (int)( s_Rng() % 2000 ) - 1000 );
		snprintf( hi, sizeof( hi ), "%d", (int)( s_Rng() % 2000 ) - 1000 );
		if ( s_Table.size() > ( 1 << 19 ) ) s_Table.assign( 1, '\0' );
		ClampToMinMax( NULL, x, S( lo ), S( hi ), s_Rng() % 4 );
		ApplyCastMode( x, s_Rng() % 6 );
	}
	CHECK( s_iLocusDepth == 0 );

	printf( s_iFailures ? "locus test: %d FAILED\n" : "locus test: OK\n", s_iFailures );
	return s_iFailures ? 1 : 0;
}
