// The top-level "attachment" and "lifetime" of XashXT / PrimeXT scripts (src/cl_dll/aurora/particlesys.cpp), run
// through the Aurora manager's own message handler with the particle editor's engine shim
// (tools/aurora-editor/core/shim.cpp). A script without them behaves as before.
//
//   attachment N (counted from 1): the followed model's attachment N - 1, only while the entity's Attachment point
//                is None (-1); the entity's own choice wins; 0 is none
//   lifetime S:  a Spawn Use copy emits for S seconds, also over its main type's own lifetime; a system of the
//                entity itself stays on

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>

extern "C" {
void ae_init( int seed );
float *ae_params( void );
void ae_file_add( const char *name, const unsigned char *data, int len );
void ae_sprite_add( const char *name, int numframes );
void ae_message( int flags, int id, float ox, float oy, float oz, float ax, float ay, float az, int target,
	int attachment, const char *script );
int ae_frame( void );
int ae_stat( int which, int id );
int ae_dump( int id );
float *ae_dump_ptr( void );
const char *ae_log( void );
void ae_log_clear( void );
}

enum { MSG_ON = 1, MSG_ONESHOT = 2, MSG_FOLLOW = 8 };

// shim.cpp's parameter layout: time 0, camera 1..6, target 34 (visible), 35..37 origin, 38..40 angles,
// attachments 41..52
enum { P_TIME = 0, P_TARGET_X = 35, P_ATT0_X = 41 };

static int s_failures = 0;
static float s_time = 1.0f;

#define CHECK( cond, what ) do { if ( !( cond ) ) { printf( "FAIL %s (%s:%d)\n", what, __FILE__, __LINE__ ); \
	s_failures++; } else printf( "ok   %s\n", what ); } while ( 0 )

// XashXT's main section: no spray, so the only particle is the main one, which sits on the emitter
static std::string Script( const char *szTop, const char *szMainLife )
{
	return std::string( "\"particles\" \"10\"\n\"maintype\" \"src\"\n" ) + szTop +
		"\n{\n\"name\" \"src\"\n\"sprite\" \"sprites/test.spr\"\n\"lifetime\" \"" + szMainLife + "\"\n}\n";
}

static void Reset( void )
{
	ae_init( 1 );
	ae_sprite_add( "sprites/test.spr", 1 );
	float *p = ae_params();
	p[1] = -256; p[2] = 0; p[3] = 64;
	p[P_TARGET_X] = 100; p[P_TARGET_X + 1] = 200; p[P_TARGET_X + 2] = 300;
	for ( int i = 0; i < 4; i++ )
	{
		p[P_ATT0_X + i * 3] = 1000.0f * ( i + 1 );
		p[P_ATT0_X + i * 3 + 1] = 0;
		p[P_ATT0_X + i * 3 + 2] = 0;
	}
	s_time = 1.0f;
	p[P_TIME] = s_time;
}

static void AddScript( const char *szName, const std::string &text )
{
	ae_file_add( szName, (const unsigned char *)text.c_str(), (int)text.size() );
}

static void Run( float flSeconds )
{
	for ( float t = 0; t < flSeconds - 1e-4f; t += 0.05f )
	{
		s_time += 0.05f;
		ae_params()[P_TIME] = s_time;
		ae_frame();
	}
}

// x of the first particle alive in system id, or NAN
static float FirstX( int id )
{
	return ae_dump( id ) > 0 ? ae_dump_ptr()[0] : NAN;
}

static void Attachment( void )
{
	const std::string xash = Script( "\"attachment\" \"2\"", "50" );

	// None (-1) on the entity: the script's attachment 2 is SBO's attachment 1 (x 2000)
	Reset();
	AddScript( "aurora/xash.aur", xash );
	ae_message( MSG_ON | MSG_FOLLOW, 5, 0, 0, 0, 0, 0, 0, 2, -1, "aurora/xash.aur" );
	Run( 0.2f );
	CHECK( FirstX( 5 ) == 2000.0f, "attachment 2 with the entity's None: on attachment 1 (counted from 0)" );

	// the entity's own attachment wins
	Reset();
	AddScript( "aurora/xash.aur", xash );
	ae_message( MSG_ON | MSG_FOLLOW, 5, 0, 0, 0, 0, 0, 0, 2, 0, "aurora/xash.aur" );
	Run( 0.2f );
	CHECK( FirstX( 5 ) == 1000.0f, "the entity's Attachment point 0 wins over the script's attachment" );

	// not following anything: the entity's point
	Reset();
	AddScript( "aurora/xash.aur", xash );
	ae_message( MSG_ON, 5, 7, 8, 9, 0, 0, 0, 0, -1, "aurora/xash.aur" );
	Run( 0.2f );
	CHECK( FirstX( 5 ) == 7.0f, "without Move with the script's attachment changes nothing" );

	// "attachment" "0" (XashXT: none) and a script without it: the followed entity's origin
	Reset();
	AddScript( "aurora/none.aur", Script( "\"attachment\" \"0\"", "50" ) );
	AddScript( "aurora/sbo.aur", Script( "", "50" ) );
	ae_message( MSG_ON | MSG_FOLLOW, 5, 0, 0, 0, 0, 0, 0, 2, -1, "aurora/none.aur" );
	ae_message( MSG_ON | MSG_FOLLOW, 6, 0, 0, 0, 0, 0, 0, 2, -1, "aurora/sbo.aur" );
	Run( 0.2f );
	CHECK( FirstX( 5 ) == 100.0f, "attachment 0 is none: the followed entity's origin" );
	CHECK( FirstX( 6 ) == 100.0f, "a script without attachment: the followed entity's origin, as before" );

	// out of range: ignored with a console line
	Reset();
	ae_log_clear();
	AddScript( "aurora/bad.aur", Script( "\"attachment\" \"9\"", "50" ) );
	ae_message( MSG_ON | MSG_FOLLOW, 5, 0, 0, 0, 0, 0, 0, 2, -1, "aurora/bad.aur" );
	Run( 0.2f );
	CHECK( FirstX( 5 ) == 100.0f, "attachment 9: ignored" );
	CHECK( strstr( ae_log(), "attachment out of range" ) != NULL, "attachment 9: a console line" );
}

static void Lifetime( void )
{
	// PrimeXT's weapon_hot.aur shape: the system's lifetime 0.6, its main type's 50
	Reset();
	AddScript( "aurora/prime.aur", Script( "\"lifetime\" \"0.6\"", "50" ) );
	ae_message( MSG_ON | MSG_ONESHOT, 0, 0, 0, 0, 0, 0, 0, 0, -1, "aurora/prime.aur" );
	Run( 0.4f );
	CHECK( ae_stat( 4, 0 ) == 1 && ae_dump( 0 ) == 1, "lifetime 0.6: the Spawn Use copy emits at 0.4 s" );
	Run( 0.5f );
	CHECK( ae_stat( 4, 0 ) == 0, "lifetime 0.6: the copy is gone at 0.9 s, its main type's 50 s does not count" );

	// no script lifetime: the main type's lifetime, as before
	Reset();
	AddScript( "aurora/sbo.aur", Script( "", "2" ) );
	ae_message( MSG_ON | MSG_ONESHOT, 0, 0, 0, 0, 0, 0, 0, 0, -1, "aurora/sbo.aur" );
	Run( 1.5f );
	CHECK( ae_stat( 4, 0 ) == 1 && ae_dump( 0 ) == 1, "no script lifetime: the copy still emits at 1.5 s (main type 2 s)" );
	Run( 1.0f );
	CHECK( ae_stat( 4, 0 ) == 0, "no script lifetime: the copy is gone at 2.5 s" );

	// the entity's own system: the entity switches it, the lifetime does not stop it
	Reset();
	AddScript( "aurora/prime.aur", Script( "\"lifetime\" \"0.6\"", "50" ) );
	ae_message( MSG_ON, 5, 0, 0, 0, 0, 0, 0, 0, -1, "aurora/prime.aur" );
	Run( 2.0f );
	CHECK( ae_stat( 5, 5 ) == 1 && ae_dump( 5 ) == 1, "lifetime 0.6 on the entity's own system: still on at 2 s" );

	// negative: not set
	Reset();
	AddScript( "aurora/neg.aur", Script( "\"lifetime\" \"-3\"", "2" ) );
	ae_message( MSG_ON | MSG_ONESHOT, 0, 0, 0, 0, 0, 0, 0, 0, -1, "aurora/neg.aur" );
	Run( 1.5f );
	CHECK( ae_stat( 4, 0 ) == 1, "lifetime -3: the main type's 2 s" );
}

int main( void )
{
	Attachment();
	Lifetime();
	printf( s_failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", s_failures );
	return s_failures ? 1 : 0;
}
