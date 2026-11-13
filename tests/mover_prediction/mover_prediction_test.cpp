// Offline test of mover prediction with movewith children (src/cl_dll/mover_prediction.cpp, src/cl_dll/movewith_client.cpp;
// docs/MOVER_PREDICTION.md "Merging with movewith") on a stub engine: a player standing on a solid child the client
// composes on a predicted parent is carried by it in the prediction, as the server's engine carries him. The
// commands run as HUD_PlayerMove runs them (cl_dll/cdll_int.cpp): place the movers, compose the children, move the
// player, then the player's own move (here: none) and MoverPrediction_PostMove. Built with ASan/UBSan by run_test.sh.
#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "entity_types.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "movewith_shared.h"
#include "movewith_client.h"
#include "mover_prediction.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------------------------- the stub engine

#define NUM_ENTS	64
static cl_entity_t s_Ents[ NUM_ENTS ];
static bool s_bPresent[ NUM_ENTS ];
static Vector s_Mins[ NUM_ENTS ], s_Maxs[ NUM_ENTS ];	// each entity's box about its origin (no rotation needed here)
static model_t s_Brush;
static model_t s_Models[ NUM_ENTS ];	// each brush entity's model, with its box as the model's bounds
static double s_flReal = 100.0;

static cl_entity_t *Stub_GetEntityByIndex( int i ) { return ( i >= 0 && i < NUM_ENTS && s_bPresent[i] ) ? &s_Ents[i] : NULL; }
static void Stub_Con_Printf( const char *fmt, ... ) { (void)fmt; }
static char s_szLastDPrint[512];
static void Stub_Con_DPrintf( const char *fmt, ... )
{
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( s_szLastDPrint, sizeof( s_szLastDPrint ), fmt, ap );
	va_end( ap );
}
static int Stub_AddCommand( const char *, void ( * )( void ) ) { return 1; }
// the engine's client time: the same for all commands of one frame; the absolute time runs on between them
static double s_flAbsDrift = 0;
static float Stub_GetClientTime( void ) { return (float)s_flReal; }
static double Stub_GetAbsoluteTime( void ) { s_flAbsDrift += 0.00001; return s_flReal + s_flAbsDrift; }
static int Stub_Cmd_Argc( void ) { return 0; }
static const char *Stub_Cmd_Argv( int ) { return ""; }
static cl_entity_t *Stub_GetLocalPlayer( void ) { return &s_Ents[1]; }
static cl_entity_t *Stub_GetViewModel( void ) { return NULL; }
static int Stub_GetMaxClients( void ) { return 8; }
static void Stub_AngleVectors( const float *angles, float *forward, float *right, float *up )
{
	const double d2r = M_PI / 180.0;
	double sy = sin( angles[1] * d2r ), cy = cos( angles[1] * d2r );
	double sp = sin( angles[0] * d2r ), cp = cos( angles[0] * d2r );
	double sr = sin( angles[2] * d2r ), cr = cos( angles[2] * d2r );
	if ( forward ) { forward[0] = cp * cy; forward[1] = cp * sy; forward[2] = -sp; }
	if ( right ) { right[0] = -sr * sp * cy + cr * sy; right[1] = -sr * sp * sy - cr * cy; right[2] = -sr * cp; }
	if ( up ) { up[0] = cr * sp * cy + sr * sy; up[1] = cr * sp * sy - sr * cy; up[2] = cr * cp; }
}

// cvars movewith reads (ex_interp): none, so it uses its defaults
static cvar_t *Stub_GetCvarPointer( const char * ) { return NULL; }

mp_stub_enginefuncs_t gEngfuncs = { Stub_GetEntityByIndex, Stub_Con_Printf, Stub_Con_DPrintf, Stub_AddCommand,
	Stub_GetClientTime, Stub_GetAbsoluteTime, Stub_Cmd_Argc, Stub_Cmd_Argv, Stub_GetLocalPlayer, Stub_GetViewModel,
	Stub_GetMaxClients, Stub_AngleVectors, Stub_GetCvarPointer };

static playermove_t s_PlayerMove;
extern "C" { playermove_t *pmove = &s_PlayerMove; }

static const Vector PLAYER_MINS( -16, -16, -36 ), PLAYER_MAXS( 16, 16, 36 );

// the physent slot whose box (not filtered out) overlaps a player standing at pos, or -1
static int TestPosition( float *pos, int ( *pfnIgnore )( physent_t *pe ) )
{
	for ( int i = 1; i < s_PlayerMove.numphysent; i++ )
	{
		physent_t *pe = &s_PlayerMove.physents[i];
		if ( pe->player || pe->solid == SOLID_NOT || ( pfnIgnore && pfnIgnore( pe ) ) )
			continue;
		Vector mins = Vector( pe->origin ) + s_Mins[ pe->info ], maxs = Vector( pe->origin ) + s_Maxs[ pe->info ];
		bool overlap = true;
		for ( int k = 0; k < 3; k++ )
		{
			if ( pos[k] + PLAYER_MAXS[k] <= mins[k] || pos[k] + PLAYER_MINS[k] >= maxs[k] )
				overlap = false;
		}
		if ( overlap )
			return i;
	}
	return -1;
}

static int Stub_TestPlayerPositionEx( float *pos, pmtrace_t *ptrace, int ( *pfnIgnore )( physent_t *pe ) )
{
	return TestPosition( pos, pfnIgnore );
}

// the world: open, or a ceiling at s_flCeiling (a player's top stops there), or a wall at x s_flWallX (his +x side
// stops there)
static float s_flCeiling = 1e6f;
static float s_flWallX = 1e6f;
static pmtrace_t Stub_PlayerTraceEx( float *start, float *end, int traceFlags, int ( *pfnIgnore )( physent_t *pe ) )
{
	pmtrace_t tr;
	memset( &tr, 0, sizeof( tr ) );
	tr.fraction = 1.0f;
	VectorCopy( end, tr.endpos );
	tr.ent = -1;
	float f = 1.0f;
	float top = s_flCeiling - PLAYER_MAXS[2];
	if ( end[2] > top && end[2] != start[2] )
		f = fminf( f, ( top - start[2] ) / ( end[2] - start[2] ) );
	float side = s_flWallX - PLAYER_MAXS[0];
	if ( end[0] > side && end[0] != start[0] )
		f = fminf( f, ( side - start[0] ) / ( end[0] - start[0] ) );
	if ( f < 1.0f )
	{
		if ( f < 0 )
			f = 0;
		tr.fraction = f;
		for ( int k = 0; k < 3; k++ )
			tr.endpos[k] = start[k] + ( end[k] - start[k] ) * f;
		tr.ent = 0;
	}
	return tr;
}

static int s_iFailures = 0;
static int s_iChecks = 0;

#define CHECK( cond, ... ) do { s_iChecks++; if ( !( cond ) ) { s_iFailures++; printf( "FAIL %s:%d: ", __FILE__, __LINE__ ); \
	printf( __VA_ARGS__ ); printf( "\n" ); } } while ( 0 )

static void Reset( void )
{
	memset( s_Ents, 0, sizeof( s_Ents ) );
	memset( s_bPresent, 0, sizeof( s_bPresent ) );
	memset( &s_PlayerMove, 0, sizeof( s_PlayerMove ) );
	s_Brush.type = mod_brush;
	for ( int i = 0; i < NUM_ENTS; i++ )
		s_Ents[i].index = i;
	MoveWith_VidInit();
	MoverPrediction_Reset();
	s_PlayerMove.PM_PlayerTraceEx = Stub_PlayerTraceEx;
	s_PlayerMove.PM_TestPlayerPositionEx = Stub_TestPlayerPositionEx;
	s_PlayerMove.movetype = MOVETYPE_WALK;
	s_flCeiling = 1e6f;
	s_flWallX = 1e6f;
}

// a brush entity received with this state; box about its origin
static cl_entity_t *Brush( int i, Vector origin, Vector mins, Vector maxs, Vector velocity, int iuser4 = 0 )
{
	cl_entity_t *e = &s_Ents[i];
	s_bPresent[i] = true;
	s_Models[i].type = mod_brush;
	VectorCopy( mins, s_Models[i].mins );
	VectorCopy( maxs, s_Models[i].maxs );
	e->model = &s_Models[i];
	e->curstate.messagenum = 1;
	e->curstate.modelindex = 10 + i;
	e->curstate.movetype = MOVETYPE_PUSH;
	e->curstate.solid = SOLID_BSP;
	e->curstate.origin = origin;
	e->curstate.basevelocity = velocity;
	e->curstate.iuser4 = iuser4;
	e->origin = origin;
	e->curstate.mins = mins;
	e->curstate.maxs = maxs;
	s_Mins[i] = mins;
	s_Maxs[i] = maxs;
	return e;
}

// the physents the engine builds from the received states (once per frame; every command of the frame reuses them)
static void BuildPhysents( void )
{
	s_PlayerMove.numphysent = 1;	// slot 0: the world
	memset( &s_PlayerMove.physents[0], 0, sizeof( physent_t ) );
	for ( int i = 9; i < NUM_ENTS; i++ )
	{
		if ( !s_bPresent[i] )
			continue;
		physent_t *pe = &s_PlayerMove.physents[ s_PlayerMove.numphysent++ ];
		memset( pe, 0, sizeof( *pe ) );
		pe->info = i;
		pe->solid = s_Ents[i].curstate.solid;
		pe->movetype = s_Ents[i].curstate.movetype;
		pe->model = s_Ents[i].model;
		pe->origin = s_Ents[i].curstate.origin;
		pe->angles = s_Ents[i].curstate.angles;
	}
}

static int SlotOf( int index )
{
	for ( int i = 1; i < s_PlayerMove.numphysent; i++ )
		if ( s_PlayerMove.physents[i].info == index )
			return i;
	return -1;
}

// one frame's drawing (movewith learns that the map has client-placed children)
static void DrawFrame( void )
{
	for ( int i = 9; i < NUM_ENTS; i++ )
		if ( s_bPresent[i] )
			MoveWith_AddEntity( ET_NORMAL, &s_Ents[i] );
	MoveWith_CreateEntities();
}

// One client frame's prediction pass at a steady high ping: real time advances, the received states are the same,
// the player starts again from his received state, and the same 30 commands of 10 ms are predicted. Mover prediction
// grows its lead with real time (a mover seen for the first time starts at lead 0), so the frames before the one
// measured are warm-up. Returns the number of commands that started inside an entity in the last pass.
static int Command( int msec, int standOn );
static int Passes( int frames, Vector start, int standOn, double startMs )
{
	int inside = 0;
	for ( int f = 0; f < frames; f++ )
	{
		s_flReal += 0.05;
		BuildPhysents();
		s_PlayerMove.origin = start;
		s_PlayerMove.onground = SlotOf( standOn );
		s_PlayerMove.time = (float)startMs;
		inside = 0;
		for ( int k = 0; k < 30; k++ )
		{
			if ( Command( 10, standOn ) >= 0 )
				inside++;
		}
	}
	return inside;
}

// one predicted command, as cl_dll/cdll_int.cpp HUD_PlayerMove runs it; the player stands still (PM_Move left out),
// on the entity under his feet. Returns the physent slot he started the command inside of (-1: none).
static int Command( int msec, int standOn )
{
#ifdef MP_OLD_API
	MoverPrediction_PreMove( &s_PlayerMove );
	MoveWith_PlayerMove( &s_PlayerMove );
#else
	MoverPrediction_PlaceMovers( &s_PlayerMove );
	MoveWith_PlayerMove( &s_PlayerMove );
	MoverPrediction_MovePlayer( &s_PlayerMove );
#endif
	int inside = TestPosition( s_PlayerMove.origin, NULL );

	// his own move: none; he stays on what is under his feet (the deck's top within 1 unit)
	int slot = SlotOf( standOn );
	physent_t *pe = &s_PlayerMove.physents[ slot ];
	float top = pe->origin[2] + s_Maxs[ standOn ][2];
	bool over = s_PlayerMove.origin[0] > pe->origin[0] + s_Mins[ standOn ][0] - 16
		&& s_PlayerMove.origin[0] < pe->origin[0] + s_Maxs[ standOn ][0] + 16;
	s_PlayerMove.onground = ( over && fabsf( s_PlayerMove.origin[2] + PLAYER_MINS[2] - top ) < 1.0f ) ? slot : -1;
	MoverPrediction_PostMove( &s_PlayerMove );

	s_PlayerMove.time += msec;
	return inside;
}

// the drawing of other riders (R27) is newer than some of the sources this test is run against: a no-op stands in
extern "C++" __attribute__(( weak )) void MoverPrediction_CreateEntities( void ) {}
// a studio parent's attachments (movewith_client.cpp): the brush movers here have none
extern "C++" __attribute__(( weak )) bool StudioAttachments_ModelSpace( struct cl_entity_s *, int, float * ) { return false; }
// the drawing clock's view (the same): older sources leave the view where the prediction put it
extern "C++" __attribute__(( weak )) void MoverPrediction_ViewOrigin( float * ) {}

// a mover's predicted physent pose after one pass of `n` commands of 10 ms starting at startMs; the player stands far
// away (he doesn't interact with it)
static physent_t *PassFarAway( int moverIndex, double startMs, int n = 30 )
{
	BuildPhysents();
	s_PlayerMove.origin = Vector( 5000, 5000, 36 );
	s_PlayerMove.onground = -1;
	s_PlayerMove.time = (float)startMs;
	for ( int k = 0; k < n; k++ )
	{
		MoverPrediction_PlaceMovers( &s_PlayerMove );
		MoveWith_PlayerMove( &s_PlayerMove );
		MoverPrediction_MovePlayer( &s_PlayerMove );
		MoverPrediction_PostMove( &s_PlayerMove );
		s_PlayerMove.time += 10;
	}
	return &s_PlayerMove.physents[ SlotOf( moverIndex ) ];
}

// ---------------------------------------------------------------------------------------------- the tests

// A deck (a solid child, iuser4 = its parent) on a parent that moves +x at 200 u/s; the player stands on the deck.
// 30 commands of 10 ms in a prediction pass (after warm-up frames): the prediction carries him about 0.29 s x 200 = 58 units, and he never
// starts a command inside the deck.
static void TestDeckCarries( void )
{
	Reset();
	Brush( 10, Vector( 0, 0, 0 ), Vector( -8, -8, -40 ), Vector( 8, 8, -24 ), Vector( 200, 0, 0 ) );		// the parent
	Brush( 11, Vector( 0, 0, 0 ), Vector( -64, -64, -16 ), Vector( 64, 64, 0 ), Vector( 0, 0, 0 ), 10 );	// the deck, local pose 0
	DrawFrame();
	int inside = Passes( 10, Vector( 0, 0, 36 ), 11, 1000 );	// standing on the deck's top
	physent_t *deck = &s_PlayerMove.physents[ SlotOf( 11 ) ];
	CHECK( fabsf( deck->origin[0] - 58.0f ) < 1.0f, "the deck is composed on the predicted parent: x %.2f, expected 58", deck->origin[0] );
	CHECK( fabsf( s_PlayerMove.origin[0] - 58.0f ) < 3.0f, "the player on the deck is carried: x %.2f, expected about 58", s_PlayerMove.origin[0] );
	CHECK( inside == 0, "the player started %d commands inside the deck", inside );
}

// A lift (a solid child) rising at 100 u/s on its parent; the player stands on it: he rises with it and is never
// inside it.
static void TestLiftCarries( void )
{
	Reset();
	Brush( 10, Vector( 200, 0, 0 ), Vector( -8, -8, -8 ), Vector( 8, 8, 8 ), Vector( 0, 0, 100 ) );		// the parent, aside
	Brush( 11, Vector( -200, 0, 0 ), Vector( -64, -64, -16 ), Vector( 64, 64, 0 ), Vector( 0, 0, 0 ), 10 );	// local: 200 units -x of it
	DrawFrame();
	int inside = Passes( 10, Vector( 0, 0, 36 ), 11, 2000 );
	CHECK( fabsf( s_PlayerMove.origin[2] - 65.0f ) < 3.0f, "the player on the lift rises with it: z %.2f, expected about 65", s_PlayerMove.origin[2] );
	CHECK( inside == 0, "the player started %d commands inside the lift", inside );
}

// The deck and a second part of the same chain (a rail beside the player, moving with the deck): the rail is part of
// his ground's chain, so it doesn't push him; he is carried once, by the deck.
static void TestChainMovesAsOne( void )
{
	Reset();
	Brush( 10, Vector( 0, 0, 0 ), Vector( -8, -8, -40 ), Vector( 8, 8, -24 ), Vector( 200, 0, 0 ) );
	Brush( 11, Vector( 0, 0, 0 ), Vector( -64, -64, -16 ), Vector( 64, 64, 0 ), Vector( 0, 0, 0 ), 10 );
	Brush( 12, Vector( 0, 0, 0 ), Vector( -32, -8, 0 ), Vector( -16, 8, 48 ), Vector( 0, 0, 0 ), 10 );		// a rail behind him
	DrawFrame();
	int inside = Passes( 10, Vector( 0, 0, 36 ), 11, 3000 );
	CHECK( fabsf( s_PlayerMove.origin[0] - 58.0f ) < 3.0f, "carried once with the chain: x %.2f, expected about 58", s_PlayerMove.origin[0] );
	CHECK( inside == 0, "the player started %d commands inside a part of the chain", inside );
}

// The lift rises toward a low ceiling with the player on it: when he reaches the ceiling, the lift is blocked and
// holds (its time stands still), as the server's pusher is blocked by a rider stuck against the world. He is never
// inside the lift and never above the ceiling, and the lift's top stays at his feet.
static void TestLiftHeldByCeiling( void )
{
	Reset();
	// 400 u/s: 4 units a command; the ceiling leaves 18 units of rise, so the fifth step is cut short by the ceiling
	// and the lift, rising its full 4 units into him, is blocked. The step is undone: he stays on the lift's top
	Brush( 10, Vector( 200, 0, 0 ), Vector( -8, -8, -8 ), Vector( 8, 8, 8 ), Vector( 0, 0, 400 ) );
	Brush( 11, Vector( -200, 0, 0 ), Vector( -64, -64, -16 ), Vector( 64, 64, 0 ), Vector( 0, 0, 0 ), 10 );
	DrawFrame();
	s_flCeiling = 90;
	int inside = Passes( 10, Vector( 0, 0, 36 ), 11, 4000 );
	physent_t *lift = &s_PlayerMove.physents[ SlotOf( 11 ) ];
	CHECK( inside == 0, "the player started %d commands inside the held lift", inside );
	CHECK( s_PlayerMove.origin[2] <= 54.01f, "the player is not above the ceiling: z %.2f", s_PlayerMove.origin[2] );
	CHECK( fabsf( lift->origin[2] - ( s_PlayerMove.origin[2] - 36 ) ) < 0.5f, "the held lift's top (%.2f) stays at his feet (%.2f): not carried a step ahead of the hold",
		lift->origin[2], s_PlayerMove.origin[2] - 36 );
}

// R25: a mover whose turn ends at its linear stop (func_platrot; iuser1 = linear + angular) stops turning there: 100 u/s
// up to z 10 (0.1 s) while turning 90 degrees a second: at a lead of 0.29 s its yaw is 9, not 26
static void TestTurnEndsAtLinearStop( void )
{
	Reset();
	cl_entity_t *e = Brush( 10, Vector( 0, 0, 0 ), Vector( -32, -32, -8 ), Vector( 32, 32, 8 ), Vector( 0, 0, 100 ) );
	e->curstate.startpos[1] = 90;
	e->curstate.iuser1 = 3;
	e->curstate.impacttime = 10;
	physent_t *pe = NULL;
	for ( int f = 0; f < 10; f++ )
	{
		s_flReal += 0.05;
		pe = PassFarAway( 10, 5000 );
	}
	CHECK( fabsf( pe->origin[2] - 10.0f ) < 0.1f, "it stops at its linear stop: z %.2f", pe->origin[2] );
	CHECK( fabsf( pe->angles[1] - 9.0f ) < 0.2f, "its turn ends at the linear stop: yaw %.2f, expected 9", pe->angles[1] );
}

// R26: a mover moving +x at 100 u/s, predicted about 0.29 s ahead, reverses (or stops) on the server. Its shown
// pose changes by at most 3 x velocity x frame time per frame (its own motion plus the 2x catch-up), never jumps back
// by velocity x lead.
static void ReversalCase( bool stop )
{
	Reset();
	cl_entity_t *e = Brush( 10, Vector( 0, 0, 0 ), Vector( -32, -32, -8 ), Vector( 32, 32, 8 ), Vector( 100, 0, 0 ) );
	const double dt = 0.05;
	const int reverseAt = 15;
	double xAtReverse = 0;
	float lastShown = 0, worst = 0;
	for ( int f = 0; f < 30; f++ )
	{
		double serverT = dt * f;
		double x;
		if ( f < reverseAt )
		{
			x = 100 * serverT;
			xAtReverse = x;
		}
		else
		{
			x = stop ? xAtReverse : xAtReverse - 100 * ( serverT - dt * reverseAt );
			e->curstate.basevelocity = Vector( stop ? 0.0f : -100.0f, 0.0f, 0.0f );
		}
		e->curstate.origin = Vector( (float)x, 0.0f, 0.0f );
		s_flReal += dt;
		physent_t *pe = PassFarAway( 10, 1000 + serverT * 1000 );
		if ( f >= 10 )
		{
			float step = fabsf( pe->origin[0] - lastShown );
			if ( step > worst )
				worst = step;
		}
		lastShown = pe->origin[0];
	}
	CHECK( worst <= 3.05f * 100 * (float)dt, "%s: the shown pose moved %.1f units in one frame (limit %.1f)",
		stop ? "a stop" : "a reversal", worst, 3.05f * 100 * (float)dt );
}

// R32: a mover that stopped on the server while it was shown moving on slides to where it stands and is there 0.3 s
// later: its physent (what the player collides with) and where it is drawn equal the received pose
static void TestStopSettles( void )
{
	Reset();
	cl_entity_t *e = Brush( 10, Vector( 0, 0, 0 ), Vector( -32, -32, -8 ), Vector( 32, 32, 8 ), Vector( 100, 0, 0 ) );
	const double dt = 0.05;
	const int stopAt = 15;
	double xStop = 0;
	physent_t *pe = NULL;
	for ( int f = 0; f < stopAt + 6; f++ )		// 6 frames = 0.3 s after the stop
	{
		double serverT = dt * f;
		if ( f < stopAt )
			xStop = 100 * serverT;
		else
			e->curstate.basevelocity = Vector( 0.0f, 0.0f, 0.0f );
		e->curstate.origin = Vector( (float)xStop, 0.0f, 0.0f );
		s_flReal += dt;
		pe = PassFarAway( 10, 1000 + serverT * 1000 );
	}
	CHECK( fabsf( pe->origin[0] - (float)xStop ) < 0.05f, "0.3 s after the stop the physent is where the mover stands: x %.2f, received %.2f",
		pe->origin[0], xStop );
	// drawn: the local player standing on it, so it is drawn at its predicted (shown) pose; a few more frames
	s_bPresent[1] = true;
	s_Ents[1].player = 1;
	for ( int f = 0; f < 6; f++ )
	{
		s_flReal += dt;
		BuildPhysents();
		s_PlayerMove.origin = Vector( (float)xStop, 0.0f, 44.0f );	// on its top
		s_PlayerMove.onground = SlotOf( 10 );
		s_PlayerMove.time = (float)( 1000 + dt * ( stopAt + 6 + f ) * 1000 );
		for ( int k = 0; k < 30; k++ )
		{
			MoverPrediction_PlaceMovers( &s_PlayerMove );
			MoveWith_PlayerMove( &s_PlayerMove );
			MoverPrediction_MovePlayer( &s_PlayerMove );
			MoverPrediction_PostMove( &s_PlayerMove );
			s_PlayerMove.time += 10;
		}
		e->origin = e->curstate.origin;
		MoverPrediction_AddEntity( ET_NORMAL, e );
	}
	CHECK( fabsf( e->origin[0] - (float)xStop ) < 0.05f, "and it is drawn there: x %.2f, received %.2f", e->origin[0], xStop );
}

// R33: another player standing on a turned turntable (a mover drawn at its predicted pose) is drawn turned with it
static void TestOtherRiderOnTurntable( void )
{
	Reset();
	cl_entity_t *tt = Brush( 10, Vector( 0, 0, 0 ), Vector( -64, -64, -8 ), Vector( 64, 64, 0 ), Vector( 0, 0, 0 ) );
	tt->curstate.startpos[1] = 40;	// 40 degrees a second about yaw, no stop
	s_bPresent[1] = true;
	s_Ents[1].player = 1;
	cl_entity_t *other = &s_Ents[2];
	s_bPresent[2] = true;
	other->player = 1;
	other->model = &s_Brush;
	for ( int f = 0; f < 12; f++ )
	{
		s_flReal += 0.05;
		tt->curstate.angles = Vector( 0.0f, 30.0f + 2.0f * f, 0.0f );	// the server's turntable at the received time
		BuildPhysents();
		s_PlayerMove.origin = Vector( 120, 0, 36 );		// next to it
		s_PlayerMove.onground = -1;
		s_PlayerMove.time = (float)( 7000 + f * 50 );
		for ( int k = 0; k < 30; k++ )
		{
			MoverPrediction_PlaceMovers( &s_PlayerMove );
			MoveWith_PlayerMove( &s_PlayerMove );
			MoverPrediction_MovePlayer( &s_PlayerMove );
			MoverPrediction_PostMove( &s_PlayerMove );
			s_PlayerMove.time += 10;
		}
		tt->origin = tt->curstate.origin;
		tt->angles = tt->curstate.angles;
		other->origin = Vector( 40.0f, 0.0f, 36.0f );	// standing on its top, 40 units out
		MoverPrediction_AddEntity( ET_NORMAL, tt );
		MoverPrediction_CreateEntities();
	}
	float turned = tt->angles[1] - tt->curstate.angles[1];
	float dyaw = atan2f( other->origin[1], other->origin[0] ) * 180.0f / (float)M_PI;
	float r = sqrtf( other->origin[0] * other->origin[0] + other->origin[1] * other->origin[1] );
	CHECK( turned > 5, "the turntable is drawn at its predicted pose (%.1f degrees ahead of the engine's)", turned );
	CHECK( fabsf( dyaw - turned ) < 0.5f && fabsf( r - 40 ) < 0.5f, "the player on it is drawn turned with it: %.1f degrees round, radius %.1f (expected %.1f, 40)",
		dyaw, r, turned );
}

// R23, a carry cut short: a deck carries the player +x toward a world wall while a rail of the same chain behind him
// comes with it. When the wall stops him, the rail runs into him: it pushes him, he can't move, and the chain holds.
// He never ends up inside the rail, and the held chain's step is not left on him.
static void TestCarryCutByWall( void )
{
	Reset();
	Brush( 10, Vector( 0, 0, 0 ), Vector( -8, -8, -40 ), Vector( 8, 8, -24 ), Vector( 200, 0, 0 ) );
	Brush( 11, Vector( 0, 0, 0 ), Vector( -128, -64, -16 ), Vector( 128, 64, 0 ), Vector( 0, 0, 0 ), 10 );	// the deck
	Brush( 12, Vector( 0, 0, 0 ), Vector( -40, -8, 0 ), Vector( -17, 8, 48 ), Vector( 0, 0, 0 ), 10 );		// a rail behind him
	DrawFrame();
	s_flWallX = 40;		// 24 units of room in front of him
	int inside = Passes( 10, Vector( 0, 0, 36 ), 11, 8000 );
	physent_t *rail = &s_PlayerMove.physents[ SlotOf( 12 ) ];
	float railFront = rail->origin[0] + s_Maxs[12][0];
	CHECK( inside == 0, "the player started %d commands inside a part of the chain", inside );
	CHECK( s_PlayerMove.origin[0] <= 24.01f, "the wall stops him: x %.2f", s_PlayerMove.origin[0] );
	CHECK( railFront <= s_PlayerMove.origin[0] - 16 + 0.01f, "the rail stays behind him: its front %.2f, his back %.2f", railFront,
		s_PlayerMove.origin[0] - 16 );
}

// R34: another player standing on the low part of a tilted deck (a drawbridge turning about x, rolled 20 degrees) is
// drawn moved with it: what counts is the deck's top under his feet, not the box's highest corner
static void OtherRiderOnTiltedDeck( float tilt )
{
	Reset();
	cl_entity_t *deck = Brush( 10, Vector( 0, 0, 0 ), Vector( -64, 0, -8 ), Vector( 64, 200, 0 ), Vector( 0, 0, 0 ) );
	deck->curstate.startpos[2] = 10;	// rolling up at 10 degrees a second
	s_bPresent[1] = true;
	s_Ents[1].player = 1;
	cl_entity_t *other = &s_Ents[2];
	s_bPresent[2] = true;
	other->player = 1;
	other->model = &s_Brush;
	Vector feetLocal( 0, 40, 0 );		// 40 units out from the hinge, on the top face
	for ( int f = 0; f < 12; f++ )
	{
		s_flReal += 0.05;
		deck->curstate.angles = Vector( 0.0f, 0.0f, tilt + 0.5f * f );
		BuildPhysents();
		s_PlayerMove.origin = Vector( 0, -60, 36 );		// at the hinge, next to it
		s_PlayerMove.onground = -1;
		s_PlayerMove.time = (float)( 9000 + f * 50 );
		for ( int k = 0; k < 30; k++ )
		{
			MoverPrediction_PlaceMovers( &s_PlayerMove );
			MoveWith_PlayerMove( &s_PlayerMove );
			MoverPrediction_MovePlayer( &s_PlayerMove );
			MoverPrediction_PostMove( &s_PlayerMove );
			s_PlayerMove.time += 10;
		}
		deck->origin = deck->curstate.origin;
		deck->angles = deck->curstate.angles;
		float fw[3], rt[3], up[3];
		Stub_AngleVectors( deck->angles, fw, rt, up );
		// his box rests on its lowest edge: the middle of his feet is 16 x tan(slope) above the face, straight up
		float slope = acosf( fminf( 1.0f, fabsf( up[2] ) ) );
		Vector feet = Vector( fw ) * feetLocal.x - Vector( rt ) * feetLocal.y + Vector( up ) * feetLocal.z
			+ Vector( 0.0f, 0.0f, 16.0f * tanf( slope ) );
		other->origin = feet + Vector( 0, 0, 36 );
		MoverPrediction_AddEntity( ET_NORMAL, deck );
		MoverPrediction_CreateEntities();
	}
	float fw[3], rt[3], up[3];
	Stub_AngleVectors( deck->angles, fw, rt, up );
	float slopeNow = acosf( fminf( 1.0f, fabsf( up[2] ) ) );
	// his feet move with the deck (the point of the face under him, turned by the deck's predicted turn) and he stays
	// upright: expected from where he stood at the engine's pose, turned with the deck
	(void)slopeNow;
	float fwI[3], rtI[3], upI[3];
	Stub_AngleVectors( deck->curstate.angles, fwI, rtI, upI );
	float slopeI = acosf( fminf( 1.0f, fabsf( upI[2] ) ) );
	Vector feetI = Vector( fwI ) * feetLocal.x - Vector( rtI ) * feetLocal.y + Vector( upI ) * feetLocal.z
		+ Vector( 0.0f, 0.0f, 16.0f * tanf( slopeI ) );
	// in the deck's frame at the engine's pose, then out at the drawn pose
	Vector dI = feetI - Vector( deck->curstate.origin );
	Vector loc( DotProduct( dI, Vector( fwI ) ), -DotProduct( dI, Vector( rtI ) ), DotProduct( dI, Vector( upI ) ) );
	Vector expect = Vector( deck->origin ) + Vector( fw ) * loc.x - Vector( rt ) * loc.y + Vector( up ) * loc.z + Vector( 0, 0, 36 );
	float rolled = deck->angles[2] - deck->curstate.angles[2];
	CHECK( rolled > 1, "tilted %.0f: the deck is drawn at its predicted pose (%.1f degrees ahead)", tilt, rolled );
	CHECK( ( Vector( other->origin ) - expect ).Length() < 0.6f, "tilted %.0f: the player on its low part is drawn with it: off by %.2f",
		tilt, ( Vector( other->origin ) - expect ).Length() );
}

static void TestOtherRiderOnTiltedDeck( void )
{
	OtherRiderOnTiltedDeck( 20 );
	OtherRiderOnTiltedDeck( 30 );
	OtherRiderOnTiltedDeck( 45 );
}

// The received pose is a little older than the pass's first command, by an amount that jitters (server frames, packet
// timing): a mover moving steadily at 150 u/s must still be predicted ahead (lead above 0), not ratchet backwards at
// every small mismatch
static void TestSteadyMismatchKeepsLead( void )
{
	Reset();
	cl_entity_t *e = Brush( 10, Vector( 0, 0, 0 ), Vector( -32, -32, -8 ), Vector( 32, 32, 8 ), Vector( 150, 0, 0 ) );
	physent_t *pe = NULL;
	double x = 0;
	for ( int f = 0; f < 40; f++ )
	{
		double serverT = 0.05 * f;
		// the received pose's age jitters by up to 16 ms from frame to frame (server frames, packet timing)
		double age = 0.008 * ( 1 + sin( f * 2.3 ) );
		x = 150 * ( serverT - age );
		e->curstate.origin = Vector( (float)x, 0.0f, 0.0f );
		s_flReal += 0.05;
		pe = PassFarAway( 10, 1000 + serverT * 1000 );
	}
	// the newest of 30 commands of 10 ms: about 0.29 s ahead of the received pose
	CHECK( pe->origin[0] > x + 150 * 0.2f, "predicted ahead: x %.1f, received %.1f (expected about %.1f)", pe->origin[0], x,
		x + 150 * 0.29 );
}

// The developer line reports a complete pass: 30 commands of 10 ms, the newest about 0.29 s ahead (it used to be
// printed at whichever command came first after a second, usually a pass's first, whose lead is always 0)
static void TestDebugLineReportsPass( void )
{
	Reset();
	Brush( 10, Vector( 0, 0, 0 ), Vector( -32, -32, -8 ), Vector( 32, 32, 8 ), Vector( 150, 0, 0 ) );
	cl_entity_t *e = &s_Ents[10];
	for ( int f = 0; f < 60; f++ )
	{
		double serverT = 0.05 * f;
		e->curstate.origin = Vector( (float)( 150 * serverT ), 0.0f, 0.0f );
		s_flReal += 0.05;
		PassFarAway( 10, 1000 + serverT * 1000 );
	}
	const char *p = strstr( s_szLastDPrint, "pass of 30 commands" );
	const char *l = strstr( s_szLastDPrint, "lead " );
	float lead = l ? (float)atof( l + 5 ) : -1;
	CHECK( p && fabsf( lead - 0.29f ) < 0.02f, "the developer line reports the whole pass: \"%s\"", s_szLastDPrint );
}

static void TestReversalNoSnap( void )
{
	ReversalCase( false );
	ReversalCase( true );
}

// R27: another player standing on a mover drawn at its predicted pose is drawn moved with it: a lift rising at 100 u/s,
// the local player next to it, a second player standing on its top at the engine's (interpolated) pose
static void TestOtherRiderDrawnOnMover( void )
{
	Reset();
	Brush( 10, Vector( 0, 0, 0 ), Vector( -64, -64, -16 ), Vector( 64, 64, 0 ), Vector( 0, 0, 100 ) );
	cl_entity_t *local = &s_Ents[1];
	s_bPresent[1] = true;
	local->player = 1;
	cl_entity_t *other = &s_Ents[2];
	s_bPresent[2] = true;
	other->player = 1;
	other->model = &s_Brush;
	cl_entity_t *lift = &s_Ents[10];
	for ( int f = 0; f < 12; f++ )
	{
		s_flReal += 0.05;
		lift->curstate.origin = Vector( 0.0f, 0.0f, 5.0f * f );	// the server's lift, 100 u/s, at the received time
		BuildPhysents();
		s_PlayerMove.origin = Vector( 100, 0, 36 );		// next to the lift
		s_PlayerMove.onground = -1;
		s_PlayerMove.time = (float)( 6000 + f * 50 );
		for ( int k = 0; k < 30; k++ )
		{
			MoverPrediction_PlaceMovers( &s_PlayerMove );
			MoveWith_PlayerMove( &s_PlayerMove );
			MoverPrediction_MovePlayer( &s_PlayerMove );
			MoverPrediction_PostMove( &s_PlayerMove );
			s_PlayerMove.time += 10;
		}
		lift->origin = lift->curstate.origin;		// the engine's interpolated pose: here the received one
		other->origin = Vector( 10.0f, 0.0f, lift->origin[2] + 36 );		// his feet on the lift's top at that pose
		MoverPrediction_AddEntity( ET_NORMAL, lift );
		MoverPrediction_CreateEntities();
	}
	float lifted = lift->origin[2] - lift->curstate.origin[2];
	CHECK( lifted > 20, "the lift is drawn at its predicted pose (%.1f above the engine's)", lifted );
	CHECK( fabsf( ( other->origin[2] - 36 ) - lift->origin[2] ) < 0.5f, "the other player is drawn standing on it: feet %.1f, top %.1f",
		other->origin[2] - 36, lift->origin[2] );
}


// The drawing clock (docs/MOVER_PREDICTION.md "Drawing clock"), as measured in game at 0 ms (2026-11-12): frames every
// 10 ms, a state from the server every 18 ms, and the engine predicting from the newest state the commands it has not
// acknowledged (1 or 2, so the newest command's time jumps back and forth against real time). A deck at 200 u/s with
// the local player standing on it: drawn and seen from his view, it moves on by 2 units a frame (not in bursts of 0
// and 4), and his view stays where he stands on it. Without the clock (the newest command's pose) the steps are the
// bursts, which the check on rawWorst shows.
static void TestDrawnSmoothAtJitter( void )
{
	Reset();
	cl_entity_t *deck = Brush( 10, Vector( 0, 0, 0 ), Vector( -64, -64, -16 ), Vector( 64, 64, 0 ), Vector( 200, 0, 0 ) );
	s_bPresent[1] = true;
	s_Ents[1].player = 1;
	const double frame = 0.010, packet = 0.018, base = 50.0;
	float lastDrawn = 0, lastView = 0, worst = 0, rawWorst = 0, lastRaw = 0, viewOffMin = 1e9f, viewOffMax = -1e9f;
	for ( int f = 0; f < 120; f++ )
	{
		double now = f * frame;
		s_flReal = 200.0 + now;
		double snap = floor( now / packet ) * packet;		// the newest state's server time (0 ms ping)
		int first = (int)ceil( snap / frame - 1e-9 );		// the first command not acknowledged by it
		int count = f - first + 1;
		if ( count < 1 )
			count = 1;
		deck->curstate.origin = Vector( (float)( 200.0 * snap ), 0.0f, 0.0f );
		BuildPhysents();
		s_PlayerMove.origin = Vector( (float)( 200.0 * snap ), 0.0f, 36.0f );	// the server's player, on the deck
		s_PlayerMove.onground = SlotOf( 10 );
		s_PlayerMove.time = (float)( ( base + snap ) * 1000.0 );
		for ( int k = 0; k < count; k++ )
			Command( 10, 10 );
		float raw = s_PlayerMove.origin[0];					// the predicted player: the newest command's pose
		deck->origin = deck->curstate.origin;				// the engine draws the ridden entity unlerped
		MoverPrediction_AddEntity( ET_NORMAL, deck );
		MoverPrediction_CreateEntities();
		float view[3] = { s_PlayerMove.origin[0], s_PlayerMove.origin[1], s_PlayerMove.origin[2] };
		MoverPrediction_ViewOrigin( view );
		if ( f >= 40 )
		{
			float step = deck->origin[0] - lastDrawn;
			worst = fmaxf( worst, fabsf( step - 2.0f ) );
			worst = fmaxf( worst, fabsf( ( view[0] - lastView ) - 2.0f ) );
			rawWorst = fmaxf( rawWorst, fabsf( ( raw - lastRaw ) - 2.0f ) );
			viewOffMin = fminf( viewOffMin, view[0] - deck->origin[0] );
			viewOffMax = fmaxf( viewOffMax, view[0] - deck->origin[0] );
		}
		lastDrawn = deck->origin[0];
		lastView = view[0];
		lastRaw = raw;
	}
	CHECK( rawWorst > 1.5f, "control: the newest command's pose moves in bursts (largest step error %.2f)", rawWorst );
	CHECK( worst < 0.6f, "the deck and the view move on by about 2 units a frame: largest step error %.2f", worst );
	CHECK( viewOffMax - viewOffMin < 0.3f, "the view stays where he stands on the deck: offset %.2f .. %.2f", viewOffMin,
		viewOffMax );
}

int main( void )
{
	TestDeckCarries();
	TestLiftCarries();
	TestChainMovesAsOne();
	TestLiftHeldByCeiling();
	TestTurnEndsAtLinearStop();
	TestReversalNoSnap();
	TestSteadyMismatchKeepsLead();
	TestDebugLineReportsPass();
	TestStopSettles();
	TestOtherRiderOnTurntable();
	TestCarryCutByWall();
	TestOtherRiderOnTiltedDeck();
	TestOtherRiderDrawnOnMover();
	TestDrawnSmoothAtJitter();
	printf( "%d checks, %d failed\n", s_iChecks, s_iFailures );
	return s_iFailures ? 1 : 0;
}
