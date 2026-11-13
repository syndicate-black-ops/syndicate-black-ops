// Offline test of the particle editor's sprite thumbnails (src/cl_dll/vgui2/sbo_image_decode.cpp DecodeSprScaled):
// every sprite given decodes to a picture inside the box, and each one cut short and with random bytes changed never
// reads out of bounds (ASan/UBSan, tests/aurora_editor/run_test.sh).

#include <stdio.h>
#include <string.h>

#include <vector>

#include "sbo_image_decode.h"

static unsigned s_Seed = 777;
static unsigned Rand()
{
	s_Seed = s_Seed * 1103515245u + 12345u;
	return ( s_Seed >> 8 ) & 0xffffff;
}

static std::vector<uint8_t> ReadFile( const char *path )
{
	std::vector<uint8_t> data;
	FILE *f = fopen( path, "rb" );
	if ( !f )
		return data;
	uint8_t buf[4096];
	size_t n;
	while ( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 )
		data.insert( data.end(), buf, buf + n );
	fclose( f );
	return data;
}

int main( int argc, char **argv )
{
	int failed = 0, decoded = 0;
	for ( int i = 1; i < argc; i++ )
	{
		std::vector<uint8_t> data = ReadFile( argv[i] );
		std::vector<uint8_t> rgba;
		int w = 0, h = 0;
		if ( !sbo_image::DecodeSprScaled( data.data(), data.size(), 36, 36, rgba, w, h ) )
		{
			printf( "FAIL: not decoded: %s\n", argv[i] );
			failed++;
			continue;
		}
		if ( w < 1 || h < 1 || w > 36 || h > 36 || ( w != 36 && h != 36 ) || rgba.size() != (size_t)w * h * 4 )
		{
			printf( "FAIL: size %dx%d for %s\n", w, h, argv[i] );
			failed++;
		}
		decoded++;
		// cut short and damaged
		for ( int k = 0; k < 20; k++ )
		{
			std::vector<uint8_t> bad( data.begin(), data.begin() + Rand() % ( data.size() + 1 ) );
			for ( int m = 0; m < 4 && !bad.empty(); m++ )
				bad[Rand() % bad.size()] = (uint8_t)Rand();
			sbo_image::DecodeSprScaled( bad.data(), bad.size(), 36, 36, rgba, w, h );
		}
	}
	const uint8_t junk[] = { 'I', 'D', 'S', 'P', 2, 0, 0, 0 };
	std::vector<uint8_t> rgba;
	int w, h;
	if ( sbo_image::DecodeSprScaled( junk, sizeof( junk ), 36, 36, rgba, w, h ) )
	{
		printf( "FAIL: a cut header decoded\n" );
		failed++;
	}
	if ( failed )
	{
		printf( "sprite thumbnails: %d FAILED\n", failed );
		return 1;
	}
	printf( "sprite thumbnails: all checks passed (%d sprites, each cut and damaged 20 times)\n", decoded );
	return 0;
}
