// game_playvideo decoder test (docs/VIDEO_PLAYER.md): src/cl_dll/video/video_decoder.cpp with the vendored libogg,
// libtheora and stb_vorbis, built with AddressSanitizer + UndefinedBehaviorSanitizer by tests/video/run_test.sh.
// Plays the sample and test files to the end (frame count, sound length, picture colors), then feeds truncated,
// bit-flipped and random data, and the name checks of game_shared/video_shared.h.
#include "video_decoder.h"
#include "video_shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

static int s_failures = 0;
#define CHECK( cond, ... ) do { if ( !( cond ) ) { printf( "FAIL %s:%d: ", __FILE__, __LINE__ ); printf( __VA_ARGS__ ); printf( "\n" ); s_failures++; } } while ( 0 )

static std::vector<unsigned char> ReadFile( const char *path )
{
	std::vector<unsigned char> d;
	FILE *f = fopen( path, "rb" );
	if ( !f )
	{
		printf( "FAIL cannot open %s\n", path );
		s_failures++;
		return d;
	}
	unsigned char buf[65536];
	size_t n;
	while ( ( n = fread( buf, 1, sizeof( buf ), f ) ) > 0 )
		d.insert( d.end(), buf, buf + n );
	fclose( f );
	return d;
}

struct PlayResult
{
	bool opened;
	int frames, pictures, width, height, rate;
	long audioFrames;
};

// Plays at 1/30 s steps, as a 30 fps client would, sound pulled as the player does
static PlayResult Play( const std::vector<unsigned char> &data, const std::vector<unsigned char> *audio = NULL,
	std::vector<unsigned char> *firstPicture = NULL )
{
	PlayResult r = { false, 0, 0, 0, 0, 0, 0 };
	CVideoDecoder dec;
	if ( !dec.Open( data.empty() ? NULL : &data[0], data.size() ) )
		return r;
	r.opened = true;
	if ( audio )
		dec.OpenAudioFile( audio->empty() ? NULL : &(*audio)[0], audio->size() );
	r.width = dec.Width();
	r.height = dec.Height();
	r.rate = dec.AudioRate();
	short pcm[2048 * 2];
	for ( int step = 0; step < 30 * 600 && !dec.IsFinished(); step++ )
	{
		if ( dec.DecodeToTime( step / 30.0 ) )
		{
			r.pictures++;
			if ( firstPicture && firstPicture->empty() )
				firstPicture->assign( dec.RGBA(), dec.RGBA() + (size_t)dec.Width() * dec.Height() * 4 );
		}
		int n;
		while ( r.rate && r.audioFrames < (long)( step + 1 ) * r.rate / 30 + 4096 && ( n = dec.ReadAudio( pcm, 2048 ) ) > 0 )
			r.audioFrames += n;
	}
	short tail[2048 * 2];
	int n;
	while ( r.rate && ( n = dec.ReadAudio( tail, 2048 ) ) > 0 )
		r.audioFrames += n;
	r.frames = dec.FramesDecoded();
	return r;
}

static void WritePPM( const char *path, const std::vector<unsigned char> &rgba, int w, int h )
{
	FILE *f = fopen( path, "wb" );
	if ( !f )
		return;
	fprintf( f, "P6\n%d %d\n255\n", w, h );
	for ( int i = 0; i < w * h; i++ )
		fwrite( &rgba[(size_t)i * 4], 1, 3, f );
	fclose( f );
}

int main( int argc, char **argv )
{
	if ( argc < 3 )
	{
		printf( "usage: video_test <sdk/examples/video> <tests/video/data> [out dir]\n" );
		return 2;
	}
	std::string vdir = argv[1], tdir = argv[2];
	std::vector<unsigned char> sample = ReadFile( ( vdir + "/sbo_sample.ogv" ).c_str() );
	std::vector<unsigned char> silent = ReadFile( ( tdir + "/sbo_test_silent.ogv" ).c_str() );
	std::vector<unsigned char> alt = ReadFile( ( tdir + "/sbo_test_alt.ogg" ).c_str() );

	// the sample: 480 x 270, 24 fps, 6 s, Vorbis 44.1 kHz stereo
	std::vector<unsigned char> pic;
	PlayResult r = Play( sample, NULL, &pic );
	CHECK( r.opened, "sample opens" );
	CHECK( r.width == 480 && r.height == 270, "sample size %dx%d", r.width, r.height );
	CHECK( r.frames >= 143 && r.frames <= 145, "sample frames %d (144)", r.frames );
	CHECK( r.pictures > 100, "sample pictures shown %d", r.pictures );
	CHECK( r.rate == 44100, "sample sound rate %d", r.rate );
	CHECK( r.audioFrames > 44100 * 5.9 && r.audioFrames < 44100 * 6.1, "sample sound frames %ld (6 s)", r.audioFrames );
	CHECK( !pic.empty(), "first picture" );
	if ( !pic.empty() && argc > 3 )
		WritePPM( ( std::string( argv[3] ) + "/sample_first.ppm" ).c_str(), pic, r.width, r.height );
	printf( "sample: %dx%d, %d frames, %d pictures, sound %d Hz %ld frames\n", r.width, r.height, r.frames, r.pictures,
		r.rate, r.audioFrames );

	// seeking (world screens: late joiners, a screen looked at again): the picture after SeekTo( t ) +
	// DecodeToTime( t ) is the one sequential decoding shows at t
	{
		CVideoDecoder seq;
		CHECK( seq.Open( &sample[0], sample.size() ), "seek: open" );
		double dur = seq.Duration();
		CHECK( dur > 5.95 && dur < 6.05, "duration %.3f (6 s)", dur );
		std::vector<unsigned long long> hashes;
		const double fps = seq.FrameRate();
		for ( int n = 0; n < 144 && !seq.IsFinished(); n++ )
		{
			seq.DecodeToTime( ( n + 0.5 ) / fps );
			unsigned long long h = 1469598103934665603ULL;
			const unsigned char *p = seq.RGBA();
			for ( size_t k = 0; k < (size_t)seq.Width() * seq.Height() * 4; k++ )
				h = ( h ^ p[k] ) * 1099511628211ULL;
			hashes.push_back( h );
		}
		const double times[] = { 0.0, 0.3, 1.5, 2.66, 3.2, 4.04, 5.9, 0.1 };
		CVideoDecoder sk;
		CHECK( sk.Open( &sample[0], sample.size() ), "seek: open 2" );
		for ( size_t i = 0; i < sizeof( times ) / sizeof( times[0] ); i++ )
		{
			const double t = times[i] + 0.5 / fps;
			CHECK( sk.SeekTo( t ), "SeekTo %.2f", t );
			sk.DecodeToTime( t );
			unsigned long long h = 1469598103934665603ULL;
			const unsigned char *p = sk.RGBA();
			for ( size_t k = 0; k < (size_t)sk.Width() * sk.Height() * 4; k++ )
				h = ( h ^ p[k] ) * 1099511628211ULL;
			const size_t n = (size_t)( t * fps );
			CHECK( n < hashes.size() && h == hashes[n], "seek to %.2f s: picture differs from frame %d", times[i], (int)n );
		}
		// sound: seek, position
		CHECK( sk.SeekAudio( 3.0 ), "SeekAudio 3.0" );
		short pcm[256];
		sk.ReadAudio( pcm, 64 );
		const double ap = sk.AudioPosition();
		CHECK( ap > 2.99 && ap < 3.05, "audio position %.3f after seek to 3.0", ap );
		printf( "seek: duration %.3f s, %d frames compared\n", dur, (int)( sizeof( times ) / sizeof( times[0] ) ) );
	}

	// no sound track: 320 x 180, 15 fps, 2 s
	r = Play( silent );
	CHECK( r.opened && r.width == 320 && r.height == 180, "silent opens %dx%d", r.width, r.height );
	CHECK( r.frames >= 29 && r.frames <= 31, "silent frames %d (30)", r.frames );
	CHECK( r.rate == 0 && r.audioFrames == 0, "silent has no sound (%d)", r.rate );

	// the separate sound file replaces the track: 22.05 kHz mono, 3 s (output stereo frames)
	r = Play( sample, &alt );
	CHECK( r.rate == 22050, "separate sound rate %d", r.rate );
	CHECK( r.audioFrames > 22050 * 2.9 && r.audioFrames < 22050 * 3.1, "separate sound frames %ld (3 s)", r.audioFrames );
	// ...and sound for a video without a track
	r = Play( silent, &alt );
	CHECK( r.rate == 22050, "separate sound for the silent video %d", r.rate );
	// not Vorbis as the sound file: the video's own track stays
	r = Play( sample, &silent );
	CHECK( r.rate == 44100, "a Theora file as sound keeps the track (%d)", r.rate );

	// not video
	CHECK( !Play( alt ).opened, "a Vorbis-only file is no video" );
	std::vector<unsigned char> empty;
	CHECK( !Play( empty ).opened, "empty data" );

	// truncated at many points: never crashes; opens only with the headers
	int opened = 0;
	for ( size_t cut = 0; cut < sample.size(); cut += ( cut < 20000 ? 97 : 9973 ) )
	{
		std::vector<unsigned char> t( sample.begin(), sample.begin() + cut );
		if ( Play( t ).opened )
			opened++;
	}
	printf( "truncated: %d of the cuts open\n", opened );

	// bit flips and random bytes
	srand( 12345 );
	for ( int i = 0; i < 300; i++ )
	{
		std::vector<unsigned char> t( i < 150 ? sample : silent );
		int flips = 1 + rand() % 64;
		for ( int k = 0; k < flips; k++ )
			t[(size_t)rand() % t.size()] ^= (unsigned char)( 1 << ( rand() % 8 ) );
		Play( t, i % 3 == 0 ? &alt : NULL );
		CVideoDecoder d2;
		if ( d2.Open( &t[0], t.size() ) )
		{
			for ( int k = 0; k < 4; k++ )
			{
				const double tt = ( rand() % 700 ) / 100.0;
				d2.SeekTo( tt );
				d2.DecodeToTime( tt );
				d2.SeekAudio( tt );
				short pcm[512];
				d2.ReadAudio( pcm, 256 );
			}
		}
	}
	for ( int i = 0; i < 50; i++ )
	{
		std::vector<unsigned char> t( 1 + rand() % 70000 );
		for ( size_t k = 0; k < t.size(); k++ )
			t[k] = (unsigned char)rand();
		if ( i % 2 )
			memcpy( &t[0], "OggS", t.size() < 4 ? t.size() : 4 );
		Play( t, &t );
	}
	// a garbage sound file
	{
		std::vector<unsigned char> g( alt );
		for ( size_t k = 100; k < g.size(); k += 7 )
			g[k] ^= 0x5a;
		Play( sample, &g );
	}

	// names (game_shared/video_shared.h): what the server precaches and the client loads
	struct { const char *name; bool video, ok; } names[] = {
		{ "sbo_sample.ogv", true, true }, { "intro/part_1.ogv", true, true }, { "a-b.c.ogv", true, false },
		{ "my.dir/x.ogv", true, false }, { "x.ogv.ogv", true, false }, { "talk.v2.ogg", false, false },	// one dot only (engine download filter)
		{ "intro.ogg", false, true }, { "intro.OGV", true, true },
		{ "", true, false }, { "../liblist.gam", true, false }, { "a/../b.ogv", true, false }, { "/abs.ogv", true, false },
		{ "a\\b.ogv", true, false }, { "c:x.ogv", true, false }, { "a//b.ogv", true, false }, { "x.ogg", true, false },
		{ "x.ogv", false, false }, { "x.ogv/", true, false }, { "with space.ogv", true, false }, { ".ogv", true, false },
		{ "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.ogv", true, false },	// 62: too long with video/
		{ "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.ogv", true, true },		// 57: video/ + 57 = 63
	};
	for ( size_t i = 0; i < sizeof( names ) / sizeof( names[0] ); i++ )
	{
		bool ok = VideoShared_IsValidName( names[i].name, names[i].video );
		CHECK( ok == names[i].ok, "name '%s' (%s): %d", names[i].name, names[i].video ? "video" : "sound", ok );
	}
	// the console error names the reason
	CHECK( VideoShared_NameError( "sbo_sample.ogv", true ) == NULL, "valid name has no error" );
	CHECK( VideoShared_NameError( "a-b.c.ogv", true ) && strstr( VideoShared_NameError( "a-b.c.ogv", true ), "more than one" ),
		"two dots: %s", VideoShared_NameError( "a-b.c.ogv", true ) );
	CHECK( VideoShared_NameError( "x.ogg", true ) && strstr( VideoShared_NameError( "x.ogg", true ), ".ogv" ), "wrong extension" );
	CHECK( VideoShared_NameError( NULL, true ) != NULL, "no name" );
	char path[VIDEO_MAX_PATH];
	CHECK( VideoShared_MakePath( "sbo_sample.ogv", true, path, sizeof( path ) ) && !strcmp( path, "video/sbo_sample.ogv" ),
		"path %s", path );
	CHECK( !VideoShared_MakePath( "../x.ogv", true, path, sizeof( path ) ) && path[0] == 0, "bad path is empty" );

	printf( s_failures ? "video_test: %d FAILED\n" : "video_test: all passed\n", s_failures );
	return s_failures ? 1 : 0;
}
