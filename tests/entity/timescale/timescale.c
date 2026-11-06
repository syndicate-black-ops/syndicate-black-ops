// Speeds up a dedicated server's clock for the entity tests (LD_PRELOAD into hlds_linux; tests/entity/run.py):
// clock_gettime and gettimeofday run ET_TIMESCALE times faster than the real clock from the first call on.
// The game library sees nothing of it: gpGlobals->time and the frame times are simply the engine's.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

static double s_scale = -1;
static int (*s_clock_gettime)( clockid_t, struct timespec * );
static int (*s_gettimeofday)( struct timeval *, void * );
static double s_base[ 16 ];		// per clock id: real time of the first call
static int s_haveBase[ 16 ];

static void Init( void )
{
	if ( s_scale >= 0 )
		return;
	const char *s = getenv( "ET_TIMESCALE" );
	s_scale = s ? atof( s ) : 1;
	if ( s_scale <= 0 )
		s_scale = 1;
	s_clock_gettime = dlsym( RTLD_NEXT, "clock_gettime" );
	s_gettimeofday = dlsym( RTLD_NEXT, "gettimeofday" );
}

static double Scale( int id, double t )
{
	if ( id < 0 || id >= 16 )
		return t;
	if ( !s_haveBase[ id ] )
	{
		s_haveBase[ id ] = 1;
		s_base[ id ] = t;
	}
	return s_base[ id ] + ( t - s_base[ id ] ) * s_scale;
}

int clock_gettime( clockid_t id, struct timespec *ts )
{
	Init();
	int r = s_clock_gettime( id, ts );
	if ( r == 0 && s_scale != 1 )
	{
		double t = Scale( (int)id, ts->tv_sec + ts->tv_nsec / 1e9 );
		ts->tv_sec = (time_t)t;
		ts->tv_nsec = (long)( ( t - (double)ts->tv_sec ) * 1e9 );
	}
	return r;
}

int gettimeofday( struct timeval *tv, void *tz )
{
	Init();
	int r = s_gettimeofday( tv, tz );
	if ( r == 0 && tv && s_scale != 1 )
	{
		double t = Scale( 15, tv->tv_sec + tv->tv_usec / 1e6 );
		tv->tv_sec = (time_t)t;
		tv->tv_usec = (long)( ( t - (double)tv->tv_sec ) * 1e6 );
	}
	return r;
}
