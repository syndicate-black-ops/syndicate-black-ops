// Aurora editor parity test, native side: runs one script through SBO's particle code and the shim for N frames
// and prints every particle's origin and size each frame. tests/parity.mjs runs the same through the
// WebAssembly build and compares (docs: tools/aurora-editor/README.md).
//
//   aurora-native <script.aur> <frames> <frametime> <seed> <follow 0|1> [sprite:frames ...]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>

extern "C" {
void ae_init( int seed );
float *ae_params( void );
void ae_file_add( const char *name, const unsigned char *data, int len );
void ae_sprite_add( const char *name, int numframes );
void ae_message( int flags, int id, float ox, float oy, float oz, float ax, float ay, float az, int target,
	int attachment, const char *script );
int ae_frame( void );
int ae_dump( int id );
float *ae_dump_ptr( void );
const char *ae_log( void );
}

int main( int argc, char **argv )
{
	if ( argc < 6 )
	{
		fprintf( stderr, "usage: aurora-native <script.aur> <frames> <frametime> <seed> <follow> [sprite:frames ...]\n" );
		return 2;
	}

	FILE *f = fopen( argv[1], "rb" );
	if ( !f )
	{
		fprintf( stderr, "cannot open %s\n", argv[1] );
		return 1;
	}
	std::vector<unsigned char> text;
	unsigned char buf[4096];
	size_t n;
	while ( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 )
		text.insert( text.end(), buf, buf + n );
	fclose( f );

	const int frames = atoi( argv[2] );
	const float dt = (float)atof( argv[3] );
	const int seed = atoi( argv[4] );
	const bool follow = atoi( argv[5] ) != 0;

	ae_init( seed );
	for ( int i = 6; i < argc; i++ )
	{
		char name[256];
		strncpy( name, argv[i], sizeof( name ) - 1 );
		name[sizeof( name ) - 1] = 0;
		char *colon = strrchr( name, ':' );
		int nf = 1;
		if ( colon )
		{
			*colon = 0;
			nf = atoi( colon + 1 );
		}
		ae_sprite_add( name, nf );
	}
	ae_file_add( "aurora/parity.aur", text.empty() ? NULL : &text[0], (int)text.size() );

	float *p = ae_params();
	// the shim's parameter layout (shim.cpp): time, camera, floor at 0
	p[0] = 1.0f;
	p[1] = -256; p[2] = 0; p[3] = 64;
	p[4] = 0; p[5] = 0; p[6] = 0;
	p[7] = 1; p[8] = 0;

	for ( int frame = 0; frame < frames; frame++ )
	{
		const float t = 1.0f + frame * dt;
		p[0] = t;
		if ( follow )
		{
			// target 2 moving and turning at a steady rate (plain float arithmetic: the same on both sides)
			const float s = t - 1.0f;
			p[35] = s * 64.0f; p[36] = s * -24.0f; p[37] = 48;
			p[38] = 0; p[39] = s * 45.0f + 0.5f; p[40] = 0;
			if ( frame == 0 )
				ae_message( 1 | 8, 1, 0, 0, 16, 0, 0, 0, 2, -1, "aurora/parity.aur" );
		}
		else if ( frame == 0 )
			ae_message( 1, 1, 0, 0, 64, 0, 0, 0, 0, -1, "aurora/parity.aur" );

		ae_frame();
		const int count = ae_dump( 1 );
		const float *d = ae_dump_ptr();
		printf( "F %d %d\n", frame, count );
		for ( int i = 0; i < count; i++ )
			printf( "%.5f %.5f %.5f %.5f\n", d[i * 4], d[i * 4 + 1], d[i * 4 + 2], d[i * 4 + 3] );
	}
	fprintf( stderr, "%s", ae_log() );
	return 0;
}
