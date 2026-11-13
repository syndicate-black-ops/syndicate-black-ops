// Speeds up a dedicated server's clock for the entity tests (LD_PRELOAD into hlds_linux; tests/entity/run.py):
// clock_gettime and gettimeofday run ET_TIMESCALE times faster than the real clock from the first call on.
// The game library sees nothing of it: gpGlobals->time and the frame times are simply the engine's.
// ET_HITCH="<p>,<ms>" (optional) makes the server hitch as on a loaded machine: every 10 ms of real time, with
// probability p, the clock jumps ahead by up to <ms> milliseconds (of game time), so that frame is that much longer.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static double s_scale = -1;
static int (*s_clock_gettime)( clockid_t, struct timespec * );
static int (*s_gettimeofday)( struct timeval *, void * );
static double s_base[ 16 ];		// per clock id: real time of the first call
static int s_haveBase[ 16 ];
static double s_hitchP = 0, s_hitchMax = 0;	// ET_HITCH
static double s_hitch = 0;					// seconds the clock jumped so far
static double s_hitchNext = 0;				// real time of the next draw

static void Init( void )
{
	if ( s_scale >= 0 )
		return;
	const char *s = getenv( "ET_TIMESCALE" );
	s_scale = s ? atof( s ) : 1;
	if ( s_scale <= 0 )
		s_scale = 1;
	const char *h = getenv( "ET_HITCH" );
	if ( h )
	{
		s_hitchP = atof( h );
		const char *c = strchr( h, ',' );
		s_hitchMax = c ? atof( c + 1 ) / 1000.0 : 0;
		srand( 12345 );
	}
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
	if ( s_hitchP > 0 && t >= s_hitchNext )
	{
		s_hitchNext = t + 0.01;
		if ( rand() < s_hitchP * RAND_MAX )
			s_hitch += s_hitchMax * ( rand() / (double)RAND_MAX );
	}
	return s_base[ id ] + ( t - s_base[ id ] ) * s_scale + s_hitch;
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
