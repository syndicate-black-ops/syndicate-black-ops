//=========================================================
// SBO 2.0: client-side prediction of moving brush entities. docs/MOVER_PREDICTION.md.
//
// The engine runs HUD_PlayerMove once for every command the server has not acknowledged yet, starting from the last
// received state, with pmove->time = that frame's time + the msec of the commands before. Its physents are the
// entities of the last received frame, frozen: a mover is where it was then, not where the server will have it when
// the command runs (up to one round trip later). Here, client side only:
//  - every MOVETYPE_PUSH SOLID_BSP physent is placed at its pose L seconds after the received one, L = the time from
//    the first predicted command to this one, from the velocity / angular velocity and the stop the server sends
//    (dlls/client.cpp MoverPrediction_FillState: basevelocity, startpos, impacttime / starttime + iuser1, endpos);
//    a stop is exact, a mover without one is extrapolated, at most MP_MAX_EXTRAPOLATE;
//  - the player is carried / pushed by that pose change like the engine's SV_PushMove / SV_PushRotate do it on the
//    server (grounded on it: moved with it; inside its new pose: pushed out; still inside: the mover is blocked and
//    holds, losing that time like its ltime);
//  - news the client cannot know in advance (a move starts, is blocked, reverses, changes speed) would make the
//    predicted pose jump by up to velocity x round trip: the lead is then limited to what keeps the pose continuous
//    and grows back at twice real time (the rider catches up at double speed instead of snapping);
//  - the mover is drawn at its predicted pose while the local player rides it or is next to it (blended in and out),
//    so the platform stays under his feet;
//  - movewith children the client places on their parent (feat/movewith, iuser4) are not predicted themselves: they
//    are composed on their parent's predicted physent (MoveWith_PlayerMove, between the two phases below), so they
//    inherit its lead, catch-up and stops, and the player is carried and pushed by them like by any mover. A chain
//    (a parent and its children) moves as one body: a rider is carried by the part he stands on and not pushed by
//    the other parts of that chain, and a part he blocks holds the whole chain (its root's time stands still), as
//    the server's movewith does.
// The server's own movement code is untouched (pm_shared runs unchanged; this is only called with server == 0).
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_types.h"
#include "cl_entity.h"
#include "com_model.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "pm_shared.h"
#include "mover_prediction.h"
#include "movewith_client.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>

// the same as dlls/client.cpp
#define MOVER_STOP_LINEAR	1
#define MOVER_STOP_ANGULAR	2

#define MP_MAX_ENTS				2048	// AddToFullPack sends no entity index beyond (11 bits)
#define MP_MAX_LEAD				1.0		// s: never predict a mover further ahead than this
#define MP_MAX_EXTRAPOLATE		0.5		// s: a mover without a known stop moves on at its velocity at most this long
#define MP_CATCHUP_RATE			2.0		// the allowed lead grows at this x real time after a correction
#define MP_CONTINUITY_DIST		1.0		// units: the received data contradicts the shown pose by more: a correction
#define MP_CONTINUITY_ANGLE		1.0		// degrees, the same for angles
#define MP_CONTINUITY_TIME		0.03	// s: plus the mover's motion in this time; a received pose's age jitters by
										// some ms (server frames, packet timing), which is not news
#define MP_HISTORY_TIMEOUT		0.25	// s (real time): an older shown pose isn't compared with (gone from view, ...)
#define MP_DRAW_BLEND_TIME		0.25	// s: drawing a mover at its predicted pose blends in / out over this
#define MP_DRAW_NEAR			64.0	// units: the local player this near a mover's box sees it at its predicted pose
#define MP_CLOCK_SMOOTH		0.5		// s: the drawing clock takes up a change of the lead over about this long
#define MP_CLOCK_RESET		0.25	// s: a drawing clock further off than this (a lag spike, a new server) is set anew
#define MP_CLOCK_MAX_SHIFT	0.1		// s: the drawing clock shows a mover at most this far from its predicted pose
#define MP_VIEW_BLEND_TIME	0.1		// s: a rider's view takes up / lets go of the drawing clock's shift over this

struct mover_pose_t
{
	Vector	origin;
	Vector	angles;
};

struct mover_t
{
	int		modelindex;		// 0: unused; another model in the slot: another entity

	// the move as received (set up once per prediction pass)
	int		pass;			// the pass it was set up in
	mover_pose_t anchor;	// the received pose: at the pass's first command (lead 0)
	Vector	velocity;
	Vector	avelocity;
	Vector	velocityAfter;	// after the stop: going straight on (func_train corner); zero: it stops
	float	stopLead;		// s after the anchor it reaches its stop; < 0: no known stop
	int		stopAngular;	// the turning ends at the stop (an angular stop, or a linear one the turn ends with)
	double	anchorTau;		// the pass's first command time
	double	limitTau;		// the furthest (command) time it may be shown at; catches up after a correction
	double	rampReal;		// real time limitTau last grew

	// this pass
	float	blocked;		// s lost to the player blocking it, like the server's pusher ltime
	int		placed;			// s_command when candidate was set (this command)
	mover_pose_t candidate;	// this command's pose (MoverPrediction_PlaceMovers), committed by MoverPrediction_MovePlayer
	double	candidateEffTau;
	mover_pose_t passPose;	// at the previous command
	double	passTau;
	double	passEffTau;		// the time passPose is the mover's pose of

	// the last pose shown (the newest command of the last pass): continuity check and drawing
	int		hasShown;
	mover_pose_t shown;
	double	shownEffTau;
	double	shownReal;

	// a mover that stopped while the client showed it moving on: its pose slides from the pose shown to the received
	// one over MP_DRAW_BLEND_TIME instead of jumping back
	int		settling;
	mover_pose_t settleFrom;
	double	settleReal;

	float	drawBlend;		// 0: drawn where the engine interpolates it, 1: at the predicted pose
	double	drawReal;

	// this frame's drawing, when it is drawn off the engine's pose (MoverPrediction_CreateEntities moves the other
	// players standing on it the same way)
	int		drawnFrame;
	mover_pose_t drawInterp;	// where the engine had it (interpolated)
	mover_pose_t drawPose;		// where it is drawn
	mover_pose_t smooth;		// its predicted pose at the drawing clock (MP_SmoothPose)
};

static int		s_drawFrame = 1;			// counts HUD_CreateEntities calls
static int		s_drawn[64];				// movers drawn off the engine's pose this frame
static int		s_numDrawn;

static mover_t s_movers[MP_MAX_ENTS];

// a composed movewith child's physent pose at the previous command of this pass (its move this command is the
// difference, as for a mover)
struct child_t
{
	int		pass;
	mover_pose_t pose;
};
static child_t s_children[MP_MAX_ENTS];
static int		s_command;		// counts HUD_PlayerMove calls (one per predicted command)
static const Vector s_vecZero( 0, 0, 0 );

static playermove_t *s_pmove;
static int		s_pass;
static double	s_passStartTau;
static double	s_lastTau = -1.0;
static double	s_lastCallReal = -1.0;
static int		s_groundEnt = -1;		// the entity the player stood on after the previous command (-1: none)
static int		s_lastGroundEnt = -1;	// after the newest command of the last pass (drawing)
static Vector	s_lastPlayerOrigin;
static double	s_lastPassReal = -1.0;
static double	s_debugReal;
// the pass that ran last, for the developer line (written once the pass is complete: the line at a command would show
// the lead of that command, which for the first command of a pass is always 0)
static int		s_passCommands, s_passMovers;
static float	s_passLastLead;
static double	s_passLastTau, s_passFrameTime;

// The drawing clock (docs/MOVER_PREDICTION.md "Drawing clock"). The newest command's time (pmove->time) is the
// received state's time plus the commands not acknowledged yet. From frame to frame it jumps back and forth by a
// command or more: a new state's server time and the commands it acknowledges don't add up (the server runs commands
// as they arrive, not on its own clock), and the number of commands not acknowledged changes with the packets'
// timing. A mover at its pose for that time, and the predicted rider on it, would be drawn in bursts (forward by two
// frames' motion, then nearly still). The prediction itself stays on the commands' times (what the server runs);
// only what is drawn moves on with real time: the clock is real time + the newest command's time - real time,
// averaged (s_clockOffset). A predicted mover is drawn at its pose for the clock, and the view of a player riding it
// is moved with it by the same change (MoverPrediction_ViewOrigin), so he stays where he stands on it.
static double	s_clockOffset;
static double	s_clockReal = -1.0;
static int		s_clockValid;
static int		s_clockFrame;
static float	s_clockShift;	// this frame: the clock - the newest command's time
// the mover the local player's view rides this frame (its chain root), and how much of the shift it takes
static int		s_viewMover = -1;
static float	s_viewBlend;
static double	s_viewReal;

static int		s_filterIndex;

#ifdef SBO_TEST_COMMANDS
// Test builds only: "test_cl_mp_trace <level> <entity index> | 0". Level 1: one MPT line per frame for that mover
// (where it is drawn and why); level 2: also one MPC line per predicted command. "test_cl_mp_off 1" turns mover
// prediction off on this client (the movewith-only behavior, to compare with).
static int		s_traceLevel;
static int		s_off;
static int		s_statCommands, s_statMovePlayer, s_statTests;	// per frame (MPT): predicted commands, carry / push checks, engine position tests

static void MP_OffCommand( void )
{
	s_off = ( gEngfuncs.Cmd_Argc() > 1 ) ? atoi( gEngfuncs.Cmd_Argv( 1 ) ) : 0;
	gEngfuncs.Con_Printf( "test_cl_mp_off: %d\n", s_off );
}
static int		s_traceIndex;

static void MP_TraceCommand( void )
{
	s_traceLevel = ( gEngfuncs.Cmd_Argc() > 1 ) ? atoi( gEngfuncs.Cmd_Argv( 1 ) ) : 0;
	s_traceIndex = ( gEngfuncs.Cmd_Argc() > 2 ) ? atoi( gEngfuncs.Cmd_Argv( 2 ) ) : 0;
	gEngfuncs.Con_Printf( "test_cl_mp_trace: level %d, entity %d\n", s_traceLevel, s_traceIndex );
}
#endif

// PM_PlayerTraceEx / PM_TestPlayerPositionEx filters (return 1 to ignore the physent)
static int MP_IgnoreAllButMover( physent_t *pe )
{
	return pe != &s_pmove->physents[s_filterIndex];
}

static int MP_IgnoreMover( physent_t *pe )
{
	return pe == &s_pmove->physents[s_filterIndex];
}

static float MP_AngleDiff( float a, float b )
{
	float d = fmod( a - b, 360.0f );
	if ( d > 180.0f )
		d -= 360.0f;
	else if ( d < -180.0f )
		d += 360.0f;
	return d;
}

/*
=================
MP_Evaluate

The mover's pose lead seconds after the anchor. Before the anchor (a negative lead: only the continuity check) the
motion is run backwards.
=================
*/
static void MP_Evaluate( const mover_t *m, float lead, mover_pose_t *pose )
{
	if ( lead > MP_MAX_LEAD )
		lead = MP_MAX_LEAD;

	if ( m->stopLead < 0 )
	{
		if ( lead > MP_MAX_EXTRAPOLATE )
			lead = MP_MAX_EXTRAPOLATE;

		pose->origin = m->anchor.origin + m->velocity * lead;
		pose->angles = m->anchor.angles + m->avelocity * lead;
		return;
	}

	if ( lead <= m->stopLead )
	{
		pose->origin = m->anchor.origin + m->velocity * lead;
		pose->angles = m->anchor.angles + m->avelocity * lead;
		return;
	}

	float after = lead - m->stopLead;
	if ( after > MP_MAX_EXTRAPOLATE )
		after = MP_MAX_EXTRAPOLATE;

	// the linear stop ends a rotation only when the server says so (func_platrot, a turning func_train that stops); a
	// func_train with avelocity spins on
	pose->origin = m->anchor.origin + m->velocity * m->stopLead + m->velocityAfter * after;
	pose->angles = m->anchor.angles + m->avelocity * ( m->stopAngular ? m->stopLead : m->stopLead + after );
}

static int MP_PoseEqual( const mover_pose_t *a, const mover_pose_t *b )
{
	return a->origin == b->origin && a->angles == b->angles;
}

/*
=================
MP_StopLead

When the received move reaches its stop: linear: the stop point's coordinate along the direction of motion
(impacttime); angular: the angle still to turn on the fastest turning axis (starttime), so a turn of more than 360
degrees stops where it should. < 0: no known stop.
=================
*/
static float MP_StopLead( const mover_t *m, const entity_state_t *state )
{
	if ( ( state->iuser1 & MOVER_STOP_LINEAR ) )
	{
		float speed = m->velocity.Length();
		if ( speed <= 0 )
			return -1;

		float lead = ( state->impacttime - DotProduct( m->anchor.origin, m->velocity ) / speed ) / speed;
		return ( lead > 0 ) ? lead : 0;
	}

	if ( ( state->iuser1 & MOVER_STOP_ANGULAR ) )
	{
		int axis = 0;
		for ( int i = 1; i < 3; i++ )
		{
			if ( fabs( m->avelocity[i] ) > fabs( m->avelocity[axis] ) )
				axis = i;
		}

		float rate = fabs( m->avelocity[axis] );
		if ( rate <= 0 )
			return -1;

		// the angle still to turn on that axis, signed (the server sends it with the anchor's angles)
		float left = state->starttime;
		if ( m->avelocity[axis] < 0 )
			left = -left;
		if ( left < 0 )
			left = 0;

		return left / rate;
	}

	return -1;
}

/*
=================
MP_SetupMover

Once per prediction pass: the move as last received, and whether it agrees with the pose shown last frame.
=================
*/
static void MP_SetupMover( mover_t *m, cl_entity_t *ent, double real )
{
	const entity_state_t *state = &ent->curstate;

	if ( m->modelindex != state->modelindex )
	{
		memset( m, 0, sizeof( *m ) );
		m->modelindex = state->modelindex;
	}

	// the move received before this one (a reversal is told from it)
	Vector prevVelocity = m->velocity, prevAvelocity = m->avelocity;

	m->pass = s_pass;
	m->anchor.origin = state->origin;
	m->anchor.angles = state->angles;
	m->velocity = state->basevelocity;
	m->avelocity = state->startpos;
	m->velocityAfter = ( state->iuser1 & MOVER_STOP_LINEAR ) ? Vector( state->endpos ) : s_vecZero;
	m->stopLead = MP_StopLead( m, state );
	m->stopAngular = ( state->iuser1 & MOVER_STOP_ANGULAR ) != 0;	// alone, or with the linear stop: the turn ends there
	m->anchorTau = s_passStartTau;
	m->blocked = 0;
	m->passPose = m->anchor;
	m->passTau = s_passStartTau;
	m->passEffTau = s_passStartTau;

	float dtReal = ( m->rampReal > 0 ) ? (float)( real - m->rampReal ) : 0.0f;
	if ( dtReal < 0 )
		dtReal = 0;
	else if ( dtReal > 0.1f )
		dtReal = 0.1f;
	m->rampReal = real;

	if ( !m->hasShown || real - m->shownReal > MP_HISTORY_TIMEOUT )
	{
		// nothing shown recently to stay continuous with: start from the received pose
		m->limitTau = m->anchorTau;
		return;
	}

	// does the move as received now pass through the pose shown last frame, at that pose's time?
	mover_pose_t check;
	float lc = (float)( m->shownEffTau - m->anchorTau );
	MP_Evaluate( m, lc, &check );

	float dist = ( check.origin - m->shown.origin ).Length();
	float angle = 0;
	for ( int i = 0; i < 3; i++ )
	{
		float d = fabs( MP_AngleDiff( check.angles[i], m->shown.angles[i] ) );
		if ( d > angle )
			angle = d;
	}

	float distTol = MP_CONTINUITY_DIST + m->velocity.Length() * MP_CONTINUITY_TIME;
	float angleTol = MP_CONTINUITY_ANGLE + m->avelocity.Length() * MP_CONTINUITY_TIME;
	if ( dist <= distTol && angle <= angleTol )
	{
		m->limitTau += dtReal * MP_CATCHUP_RATE;
	}
	else
	{
		// news (started, blocked, reversed, new speed): continue from the lead nearest to the pose shown. It may be
		// before the anchor (negative): a mover that reversed was shown ahead along its old direction, and it goes on
		// from there along its new one, catching up at MP_CATCHUP_RATE, instead of jumping back
		float best = 0;
		float speed2 = DotProduct( m->velocity, m->velocity );
		float aspeed2 = DotProduct( m->avelocity, m->avelocity );
		if ( speed2 > 0 )
		{
			best = DotProduct( m->shown.origin - m->anchor.origin, m->velocity ) / speed2;
		}
		else if ( aspeed2 > 0 )
		{
			Vector d;
			for ( int i = 0; i < 3; i++ )
				d[i] = MP_AngleDiff( m->shown.angles[i], m->anchor.angles[i] );
			best = DotProduct( d, m->avelocity ) / aspeed2;
		}
		else if ( !m->settling )
		{
			// it stopped: slide from the pose shown to where it stands (once: a slide under way goes on, it is not
			// started again from where it got to)
			m->settling = 1;
			m->settleFrom = m->shown;
			m->settleReal = real;
		}

		if ( best > lc )
			best = lc;
		// before the received pose only when the mover reversed (it was shown ahead along its old direction); any
		// other news (a start, a new speed, the small mismatch of a received pose's time) starts at the received pose
		// at the earliest, else the lead would drift backwards pass after pass
		bool reversed = DotProduct( m->velocity, prevVelocity ) < 0 || DotProduct( m->avelocity, prevAvelocity ) < 0;
		if ( best < ( reversed ? -MP_MAX_LEAD : 0.0f ) )
			best = reversed ? -MP_MAX_LEAD : 0.0f;

		m->limitTau = m->anchorTau + best;
	}

	if ( m->limitTau > m->anchorTau + MP_MAX_LEAD )
		m->limitTau = m->anchorTau + MP_MAX_LEAD;
}

static void MP_TransformPoint( const Vector &point, const mover_pose_t *from, const mover_pose_t *to, Vector &out )
{
	if ( from->angles == to->angles )
	{
		out = point + ( to->origin - from->origin );
		return;
	}

	// the engine's rotated brush model clipping: local = ( d.forward, -d.right, d.up )
	Vector forward, right, up;
	Vector d = point - from->origin;
	AngleVectors( from->angles, forward, right, up );
	Vector local( DotProduct( d, forward ), -DotProduct( d, right ), DotProduct( d, up ) );

	AngleVectors( to->angles, forward, right, up );
	out = to->origin + forward * local.x - right * local.y + up * local.z;
}

/*
=================
MP_MovePlayer

What SV_PushMove / SV_PushRotate do to the player when the pusher moves from one pose to the next: standing on it,
he is moved with it; otherwise, if its new pose overlaps him, he is pushed by its move. The push is traced against
everything but the mover. If he is still inside the mover, it is blocked: returns 0, the player is not moved.
=================
*/
// Can the mover touch the player on its way from one pose to the other? Its box swept over both poses (a turned
// mover: its bounding sphere) against his. A no skips the engine's position test, which walks every physent: on a map
// with hundreds of moving children that test, once per child and command, cost more than the rest of the frame
// (co_movewith_test at 300 ms: 15-25 fps).
static int MP_NearPlayer( const physent_t *pe, const mover_pose_t *from, const mover_pose_t *to )
{
	if ( !pe->model )
		return 1;
	Vector mins = pe->model->mins, maxs = pe->model->maxs;
	if ( mins == maxs )
		return 1;	// no bounds known: the full test
	if ( from->angles != s_vecZero || to->angles != s_vecZero )
	{
		float r = mins.Length();
		if ( maxs.Length() > r )
			r = maxs.Length();
		mins = Vector( -r, -r, -r );
		maxs = Vector( r, r, r );
	}

	Vector pmins = s_pmove->player_mins[ s_pmove->usehull & 3 ], pmaxs = s_pmove->player_maxs[ s_pmove->usehull & 3 ];
	if ( pmins == pmaxs )
	{
		pmins = Vector( -16, -16, -36 );
		pmaxs = Vector( 16, 16, 36 );
	}
	for ( int k = 0; k < 3; k++ )
	{
		float lo = fminf( from->origin[k], to->origin[k] ) + mins[k] - 2;
		float hi = fmaxf( from->origin[k], to->origin[k] ) + maxs[k] + 2;
		if ( s_pmove->origin[k] + pmaxs[k] < lo || s_pmove->origin[k] + pmins[k] > hi )
			return 0;
	}
	return 1;
}

static int MP_MovePlayer( int index, const mover_pose_t *from, const mover_pose_t *to, int grounded )
{
	pmtrace_t trace;

	s_filterIndex = index;

#ifdef SBO_TEST_COMMANDS
	s_statMovePlayer++;
#endif
	if ( !grounded && !MP_NearPlayer( &s_pmove->physents[index], from, to ) )
		return 1;	// too far to touch him
#ifdef SBO_TEST_COMMANDS
	if ( !grounded )
		s_statTests++;
#endif
	if ( !grounded && s_pmove->PM_TestPlayerPositionEx( s_pmove->origin, &trace, MP_IgnoreAllButMover ) != index )
		return 1;	// not touched

	Vector start = s_pmove->origin;
	Vector end;
	MP_TransformPoint( start, from, to, end );

	trace = s_pmove->PM_PlayerTraceEx( start, end, PM_NORMAL, MP_IgnoreMover );

	Vector moved = start;
	if ( trace.fraction > 0 && !trace.allsolid )
		moved = trace.endpos;

	if ( s_pmove->PM_TestPlayerPositionEx( moved, &trace, MP_IgnoreAllButMover ) == index )
		return 0;	// blocked

	s_pmove->origin = moved;
	return 1;
}

static int MP_PlayerInteracts( const playermove_t *ppmove )
{
	if ( ppmove->dead || ppmove->spectator || ppmove->iuser1 )
		return 0;

	if ( ppmove->movetype == MOVETYPE_NOCLIP || ppmove->movetype == MOVETYPE_NONE || ppmove->movetype == MOVETYPE_FOLLOW )
		return 0;

	return ppmove->PM_PlayerTraceEx != NULL && ppmove->PM_TestPlayerPositionEx != NULL;
}

// the root of a client-placed movewith chain (the entity itself when it isn't such a child)
static int MP_ChainRoot( int index )
{
	for ( int depth = 0; depth < 16 && index > 0 && index < MP_MAX_ENTS; depth++ )
	{
		cl_entity_t *ent = gEngfuncs.GetEntityByIndex( index );
		int parent = ent ? MoveWith_ClientParent( ent ) : 0;
		if ( !parent )
			break;
		index = parent;
	}
	return index;
}

static int MP_IsMoverPhysent( const physent_t *pe )
{
	return !pe->player && pe->movetype == MOVETYPE_PUSH && pe->solid == SOLID_BSP && pe->model && pe->info > 0
		&& pe->info < MP_MAX_ENTS;
}

/*
=================
MoverPrediction_PlaceMovers

Phase 1, before movewith composes its children: every pushing physent (not a client-placed child) is put at its pose
for this command. The player is not touched yet.
=================
*/
void MoverPrediction_PlaceMovers( struct playermove_s *ppmove )
{
	s_command++;

	if ( ppmove->server )
		return;
#ifdef SBO_TEST_COMMANDS
	if ( s_off )
		return;
#endif

	// observers move through everything; their view of the movers is the engine's
	if ( ppmove->spectator || ppmove->iuser1 )
		return;

	s_pmove = ppmove;

	double tau = ppmove->time * 0.001;
	double real = gEngfuncs.GetAbsoluteTime();

	// a new prediction pass: a new client frame (the client time, which is the same for every command the engine
	// replays in one frame; the absolute time is a running clock and differs between those calls), or the engine
	// predicts again from the last received state
	double frameTime = gEngfuncs.GetClientTime();
	if ( frameTime != s_lastCallReal || tau < s_lastTau )
	{
		// developer 1: once a second while something moves, the last complete pass: its commands, the time base on
		// this engine and the lead of its newest command
		if ( s_passMovers && real - s_debugReal > 1.0 )
		{
			s_debugReal = real;
			gEngfuncs.Con_DPrintf( "MoverPrediction: %d moving, pass of %d commands, first %.3f, last %.3f, client time %.3f, lead %.3f\n",
				s_passMovers, s_passCommands, s_passStartTau, s_passLastTau, s_passFrameTime, s_passLastLead );
		}
		s_passCommands = 0;
		s_passMovers = 0;
		s_passLastLead = 0;
		s_passFrameTime = frameTime;
		s_pass++;
		s_passStartTau = tau;
		s_groundEnt = -1;
		s_lastPassReal = real;
	}
	s_lastCallReal = frameTime;
	s_lastTau = tau;

#ifdef SBO_TEST_COMMANDS
	s_statCommands++;
	if ( s_traceLevel >= 2 )
		gEngfuncs.Con_Printf( "MPC %.4f %.4f pass %d tau %.4f msec %d ft %.4f org %.2f %.2f %.2f\n", real, frameTime, s_pass,
			tau, ppmove->cmd.msec, ppmove->frametime, ppmove->origin[0], ppmove->origin[1], ppmove->origin[2] );
#endif

	for ( int i = 1; i < ppmove->numphysent; i++ )
	{
		physent_t *pe = &ppmove->physents[i];
		if ( !MP_IsMoverPhysent( pe ) )
			continue;

		cl_entity_t *ent = gEngfuncs.GetEntityByIndex( pe->info );
		if ( !ent )
			continue;

		// a movewith child the client places on its parent is sent with its local pose: it is composed on its parent
		// (MoveWith_PlayerMove), not predicted from its own state
		if ( MoveWith_ClientParent( ent ) )
			continue;

		mover_t *m = &s_movers[pe->info];
		if ( m->pass != s_pass || m->modelindex != ent->curstate.modelindex )
			MP_SetupMover( m, ent, real );

		float lead = (float)( tau - m->anchorTau ) - m->blocked;
		float limit = (float)( m->limitTau - m->anchorTau );
		if ( lead > limit )
			lead = limit;
		if ( lead < -MP_MAX_LEAD )
			lead = -MP_MAX_LEAD;

		MP_Evaluate( m, lead, &m->candidate );
		m->candidateEffTau = m->anchorTau + lead;

		if ( m->settling )
		{
			float f = (float)( ( real - m->settleReal ) / MP_DRAW_BLEND_TIME );
			if ( f >= 1 || m->velocity != s_vecZero || m->avelocity != s_vecZero )
			{
				m->settling = 0;
			}
			else
			{
				if ( f < 0 )
					f = 0;
				for ( int k = 0; k < 3; k++ )
				{
					m->candidate.origin[k] = m->settleFrom.origin[k] + ( m->candidate.origin[k] - m->settleFrom.origin[k] ) * f;
					m->candidate.angles[k] = m->settleFrom.angles[k] + MP_AngleDiff( m->candidate.angles[k], m->settleFrom.angles[k] ) * f;
				}
			}
		}
		m->placed = s_command;

		pe->origin = m->candidate.origin;
		pe->angles = m->candidate.angles;
	}
}

// a chain held: its root's candidate goes back to the previous command's pose, its time stands still for this step
static void MP_HoldChain( int root, double tau )
{
	if ( root <= 0 || root >= MP_MAX_ENTS )
		return;

	mover_t *m = &s_movers[root];
	if ( m->placed != s_command )
		return;		// not a predicted mover (a monster, a cycler...): nothing to hold on the client

	m->blocked += (float)( tau - m->passTau );
	m->candidate = m->passPose;
	m->candidateEffTau = m->passEffTau;

	for ( int i = 1; i < s_pmove->numphysent; i++ )
	{
		physent_t *pe = &s_pmove->physents[i];
		if ( pe->info == root && MP_IsMoverPhysent( pe ) )
		{
			pe->origin = m->candidate.origin;
			pe->angles = m->candidate.angles;
		}
	}
}

/*
=================
MoverPrediction_MovePlayer

Phase 2, after movewith composed its children on the placed movers: the player is carried / pushed by every pushing
physent whose pose changed since the previous command (a predicted mover or a composed child), the part he stands on
first. The other parts of the chain he stands on move with his ground and don't push him. A part that stays inside
him holds its whole chain for this step; then the chain is composed again.
=================
*/
void MoverPrediction_MovePlayer( struct playermove_s *ppmove )
{
	if ( ppmove->server || ppmove->spectator || ppmove->iuser1 || s_pmove != ppmove )
		return;
#ifdef SBO_TEST_COMMANDS
	if ( s_off )
		return;
#endif

	double tau = ppmove->time * 0.001;
	double real = gEngfuncs.GetAbsoluteTime();
	int interacts = MP_PlayerInteracts( ppmove );
	int movers = 0;
	float newestLead = 0;

	// where the player was before each chain first moved him this step: a chain that is then held gives that back,
	// as the server's blocked pusher puts back what it moved
	int chainRoot[16];
	Vector chainBefore[16];
	int numChains = 0;

	for ( int order = 0; order < 2 && interacts; order++ )
	{
		for ( int i = 1; i < ppmove->numphysent; i++ )
		{
			physent_t *pe = &ppmove->physents[i];
			if ( !MP_IsMoverPhysent( pe ) )
				continue;

			int grounded = ( s_groundEnt == pe->info );
			if ( ( order == 0 ) != ( grounded != 0 ) )
				continue;	// the ground first, then the rest

			cl_entity_t *ent = gEngfuncs.GetEntityByIndex( pe->info );
			if ( !ent )
				continue;

			mover_pose_t from, to;
			to.origin = pe->origin;
			to.angles = pe->angles;

			int root = pe->info;
			if ( MoveWith_ClientParent( ent ) )
			{
				child_t *c = &s_children[pe->info];
				if ( c->pass != s_pass )
					continue;	// the first command of the pass: no move yet
				from = c->pose;
				root = MP_ChainRoot( pe->info );
			}
			else
			{
				mover_t *m = &s_movers[pe->info];
				if ( m->placed != s_command )
					continue;
				from = m->passPose;
			}

			if ( MP_PoseEqual( &from, &to ) )
				continue;

			// The other parts of the chain he stands on move with his ground, which carried him first: they no longer
			// touch him, unless something cut his carry short (a wall). Then the part that runs into him pushes him,
			// and if he can't move, it is blocked and the whole chain holds, as on the server.
			int c = 0;
			while ( c < numChains && chainRoot[c] != root )
				c++;
			if ( c == numChains && numChains < 16 )
			{
				chainRoot[numChains] = root;
				chainBefore[numChains++] = ppmove->origin;
			}

			if ( !MP_MovePlayer( i, &from, &to, grounded ) )
			{
				// blocked: the chain's root stays and its time stands still for that step, its children follow, and
				// what the chain moved the player this step is undone
				if ( c < numChains )
					ppmove->origin = chainBefore[c];
				MP_HoldChain( root, tau );
				MoveWith_PlayerMove( ppmove );
			}
		}
	}

	// commit this command's poses
	for ( int i = 1; i < ppmove->numphysent; i++ )
	{
		physent_t *pe = &ppmove->physents[i];
		if ( !MP_IsMoverPhysent( pe ) )
			continue;

		cl_entity_t *ent = gEngfuncs.GetEntityByIndex( pe->info );
		if ( !ent )
			continue;

		if ( MoveWith_ClientParent( ent ) )
		{
			child_t *c = &s_children[pe->info];
			c->pass = s_pass;
			c->pose.origin = pe->origin;
			c->pose.angles = pe->angles;
			continue;
		}

		mover_t *m = &s_movers[pe->info];
		if ( m->placed != s_command )
			continue;

		m->passPose = m->candidate;
		m->passTau = tau;
		m->passEffTau = m->candidateEffTau;

		m->hasShown = 1;
		m->shown = m->candidate;
		m->shownEffTau = m->candidateEffTau;
		m->shownReal = real;

		if ( m->velocity != s_vecZero || m->avelocity != s_vecZero )
		{
			movers++;
			newestLead = (float)( m->candidateEffTau - m->anchorTau );
		}
	}

	// for the developer line of this pass (MoverPrediction_PlaceMovers prints it when the next pass starts)
	s_passCommands++;
	s_passLastTau = tau;
	if ( movers )
	{
		s_passMovers = movers;
		s_passLastLead = newestLead;
	}
}

void MoverPrediction_PostMove( struct playermove_s *ppmove )
{
	if ( ppmove->server || ppmove->spectator || ppmove->iuser1 )
		return;

	s_groundEnt = -1;
	if ( ppmove->onground > 0 && ppmove->onground < ppmove->numphysent )
		s_groundEnt = ppmove->physents[ppmove->onground].info;

	s_lastGroundEnt = s_groundEnt;
	s_lastPlayerOrigin = ppmove->origin;
}

#ifdef SBO_TEST_COMMANDS
static void MP_TraceFrame( const mover_t *m, const cl_entity_t *ent, double real, int current, const Vector &interp )
{
	if ( s_traceLevel < 1 || ent->index != s_traceIndex )
		return;

	gEngfuncs.Con_Printf( "MPT %.4f %.4f cur %d pass %d cmds %d first %.4f last %.4f anchor %.2f %.2f %.2f msg %.4f %d "
		"interp %.2f %.2f %.2f shown %.2f %.2f %.2f lead %.4f limit %.4f blend %.2f drawn %.2f %.2f %.2f vel %.1f %.1f %.1f "
		"shift %.4f view %.2f cmds %d carries %d tests %d\n",
		real, gEngfuncs.GetClientTime(), current, m->pass, s_passCommands, s_passStartTau, s_passLastTau,
		m->anchor.origin[0], m->anchor.origin[1], m->anchor.origin[2], ent->curstate.msg_time, ent->curstate.messagenum,
		interp[0], interp[1], interp[2], m->shown.origin[0], m->shown.origin[1], m->shown.origin[2],
		m->shownEffTau - m->anchorTau, m->limitTau - m->anchorTau, m->drawBlend, ent->origin[0], ent->origin[1],
		ent->origin[2], m->velocity[0], m->velocity[1], m->velocity[2], s_clockShift, s_viewBlend, s_statCommands, s_statMovePlayer, s_statTests );
	s_statCommands = s_statMovePlayer = s_statTests = 0;
}
#endif

/*
=================
MP_UpdateClock

Once per frame, before the first mover is drawn: the drawing clock follows the newest command's time on average.
=================
*/
static void MP_UpdateClock( double real )
{
	if ( s_clockFrame == s_drawFrame )
		return;
	s_clockFrame = s_drawFrame;

	double sample = s_passLastTau - real;
	if ( !s_clockValid || fabs( sample - s_clockOffset ) > MP_CLOCK_RESET )
	{
		s_clockOffset = sample;
	}
	else
	{
		double k = ( real - s_clockReal ) / MP_CLOCK_SMOOTH;
		if ( k > 1 )
			k = 1;
		else if ( k < 0 )
			k = 0;
		s_clockOffset += ( sample - s_clockOffset ) * k;
	}
	s_clockReal = real;
	s_clockValid = 1;

	float shift = (float)( real + s_clockOffset - s_passLastTau );
	if ( shift > MP_CLOCK_MAX_SHIFT )
		shift = MP_CLOCK_MAX_SHIFT;
	else if ( shift < -MP_CLOCK_MAX_SHIFT )
		shift = -MP_CLOCK_MAX_SHIFT;
	s_clockShift = shift;
}

// the mover's pose at the drawing clock: its shown (newest command's) pose moved on by the clock's shift along the
// same move. Not while the player holds it or it settles (those poses are not a time on its move)
static void MP_SmoothPose( mover_t *m )
{
	m->smooth = m->shown;
	if ( m->blocked > 0 || m->settling )
		return;

	float lead = (float)( m->shownEffTau - m->anchorTau ) + s_clockShift;
	float limit = (float)( m->limitTau - m->anchorTau );
	if ( lead > limit )
		lead = limit;
	if ( lead < -MP_MAX_LEAD )
		lead = -MP_MAX_LEAD;
	MP_Evaluate( m, lead, &m->smooth );
}

/*
=================
MoverPrediction_AddEntity

The engine draws a mover where it was ex_interp before the last update; the predicted local player stands on it as
it will be when his newest command runs. While he rides it or is next to it, it is drawn at that pose, so it stays
under his feet / where he collides with it (blended in and out). Others see each other's movers as the engine draws
them.
=================
*/
void MoverPrediction_AddEntity( int type, struct cl_entity_s *ent )
{
#ifdef SBO_TEST_COMMANDS
	if ( s_off )
		return;
#endif
	if ( type != ET_NORMAL || !ent || ent->index <= 0 || ent->index >= MP_MAX_ENTS )
		return;

	if ( MoveWith_ClientParent( ent ) )
		return;		// a movewith child: placed on its (predicted) parent by movewith

	mover_t *m = &s_movers[ent->index];
	if ( !m->modelindex || m->modelindex != ent->curstate.modelindex )
		return;

	double real = gEngfuncs.GetAbsoluteTime();
	MP_UpdateClock( real );

	// only with this frame's prediction (none for an observer, with cl_predict 0, ...)
	int current = m->hasShown && real - m->shownReal < 0.05 && real - s_lastPassReal < 0.05;

	float target = 0;
	if ( current )
	{
		if ( s_lastGroundEnt == ent->index )
		{
			target = 1;
		}
		else if ( m->velocity != s_vecZero || m->avelocity != s_vecZero || m->drawBlend > 0 )
		{
			// near its box at the predicted pose (a rotating one: its bounding sphere)
			Vector mins = ent->curstate.mins, maxs = ent->curstate.maxs;
			if ( m->avelocity != s_vecZero || m->shown.angles != s_vecZero )
			{
				float r = mins.Length();
				if ( maxs.Length() > r )
					r = maxs.Length();
				mins = Vector( -r, -r, -r );
				maxs = Vector( r, r, r );
			}

			Vector absmin = m->shown.origin + mins;
			Vector absmax = m->shown.origin + maxs;
			const float nearDist = MP_DRAW_NEAR + 36;	// + the player's half size

			if ( s_lastPlayerOrigin.x > absmin.x - nearDist && s_lastPlayerOrigin.x < absmax.x + nearDist
				&& s_lastPlayerOrigin.y > absmin.y - nearDist && s_lastPlayerOrigin.y < absmax.y + nearDist
				&& s_lastPlayerOrigin.z > absmin.z - nearDist && s_lastPlayerOrigin.z < absmax.z + nearDist )
			{
				target = 1;
			}
		}
	}

	float dt = ( m->drawReal > 0 ) ? (float)( real - m->drawReal ) : 0.0f;
	m->drawReal = real;
	if ( dt < 0 || dt > 0.1f )
		dt = 0.1f;

	float step = dt / MP_DRAW_BLEND_TIME;
	if ( m->drawBlend < target )
	{
		m->drawBlend += step;
		if ( m->drawBlend > target )
			m->drawBlend = target;
	}
	else if ( m->drawBlend > target )
	{
		m->drawBlend -= step;
		if ( m->drawBlend < target )
			m->drawBlend = target;
	}

#ifdef SBO_TEST_COMMANDS
	Vector traceInterp = ent->origin;
#endif
	if ( m->drawBlend <= 0 || !current )
	{
		if ( !current )
			m->drawBlend = 0;
#ifdef SBO_TEST_COMMANDS
		MP_TraceFrame( m, ent, real, current, traceInterp );
#endif
		return;
	}

	float b = m->drawBlend;
	MP_SmoothPose( m );
	m->drawInterp.origin = ent->origin;
	m->drawInterp.angles = ent->angles;
	for ( int i = 0; i < 3; i++ )
	{
		ent->origin[i] += ( m->smooth.origin[i] - ent->origin[i] ) * b;
		ent->angles[i] += MP_AngleDiff( m->smooth.angles[i], ent->angles[i] ) * b;
	}
	m->drawPose.origin = ent->origin;
	m->drawPose.angles = ent->angles;
#ifdef SBO_TEST_COMMANDS
	MP_TraceFrame( m, ent, real, current, traceInterp );
#endif
	if ( m->drawnFrame != s_drawFrame && s_numDrawn < (int)( sizeof( s_drawn ) / sizeof( s_drawn[0] ) ) )
	{
		m->drawnFrame = s_drawFrame;
		s_drawn[ s_numDrawn++ ] = ent->index;
	}
}

/*
=================
MoverPrediction_CreateEntities

HUD_CreateEntities, after every entity of the frame is in: another player standing on a mover that is drawn at its
predicted pose is drawn moved with it (by the same change from the engine's pose to the drawn one), so he stands on
it instead of sinking into a rising lift or floating above a sinking one. "Standing on": his drawn feet within a few
units of the mover's top at the engine's pose, inside its box. The engine draws both in the past together otherwise.
=================
*/
#define MP_STAND_TOLERANCE	4.0f

void MoverPrediction_CreateEntities( void )
{
	if ( s_numDrawn > 0 )
	{
		cl_entity_t *local = gEngfuncs.GetLocalPlayer();
		int maxClients = gEngfuncs.GetMaxClients();
		for ( int p = 1; p <= maxClients && p < MP_MAX_ENTS; p++ )
		{
			cl_entity_t *pl = gEngfuncs.GetEntityByIndex( p );
			if ( !pl || pl == local || !pl->player || !pl->model || ( local && pl->curstate.messagenum != local->curstate.messagenum ) )
				continue;

			float feet = pl->origin[2] + ( pl->curstate.usehull ? -18.0f : -36.0f );
			for ( int k = 0; k < s_numDrawn; k++ )
			{
				cl_entity_t *ent = gEngfuncs.GetEntityByIndex( s_drawn[k] );
				if ( !ent )
					continue;
				mover_t *m = &s_movers[ s_drawn[k] ];

				// on its top face at his x / y: his feet in the mover's own frame (at the engine's pose; the engine's
				// rotated brush clipping turns the model by forward, -right, up), inside its top and within a few
				// units of it. This holds for a turned or tilted mover too (a drawbridge deck)
				Vector forward, right, up;
				AngleVectors( m->drawInterp.angles, forward, right, up );
				Vector d = Vector( pl->origin[0], pl->origin[1], feet ) - m->drawInterp.origin;
				Vector local( DotProduct( d, forward ), -DotProduct( d, right ), DotProduct( d, up ) );
				// On a slope his box rests on its lowest edge, so the middle of his feet is above the top face by up to
				// 16 x the slope's sine on each axis (along the face's normal): the tolerance grows with the tilt
				Vector mins = ent->curstate.mins, maxs = ent->curstate.maxs;
				float tolerance = MP_STAND_TOLERANCE + 16.0f * ( fabs( up.x ) + fabs( up.y ) );
				if ( local.z < maxs.z - MP_STAND_TOLERANCE || local.z > maxs.z + tolerance || local.x < mins.x - 16
					|| local.x > maxs.x + 16 || local.y < mins.y - 16 || local.y > maxs.y + 16 )
					continue;

				// his feet move with the deck; he stays upright
				Vector moved;
				MP_TransformPoint( Vector( pl->origin[0], pl->origin[1], feet ), &m->drawInterp, &m->drawPose, moved );
				pl->origin = moved + Vector( 0.0f, 0.0f, pl->origin[2] - feet );
				break;
			}
		}
	}

	s_drawFrame++;
	s_numDrawn = 0;
}

/*
=================
MoverPrediction_ViewOrigin

V_CalcRefdef, before the view is worked out from it: the local player's predicted origin (pparams->simorg) while he
rides a mover drawn at the drawing clock is moved with it, by the change from its predicted pose to the one it is
drawn at, so he stays where he stands on it. Taken up and let go of over MP_VIEW_BLEND_TIME (he lands on it, jumps
off).
=================
*/
void MoverPrediction_ViewOrigin( float *origin )
{
	double real = gEngfuncs.GetAbsoluteTime();
	float dt = ( s_viewReal > 0 ) ? (float)( real - s_viewReal ) : 0.0f;
	s_viewReal = real;
	if ( dt < 0 || dt > 0.1f )
		dt = 0.1f;

#ifdef SBO_TEST_COMMANDS
	if ( s_off )
		return;
#endif

	// the mover under him (the root of the chain, for a movewith child he stands on), drawn this frame
	int root = ( s_lastGroundEnt > 0 && s_lastGroundEnt < MP_MAX_ENTS ) ? MP_ChainRoot( s_lastGroundEnt ) : -1;
	int target = 0;
	if ( root > 0 && root < MP_MAX_ENTS && s_movers[root].modelindex && s_movers[root].drawnFrame == s_drawFrame - 1
		&& s_movers[root].drawBlend > 0 )
	{
		s_viewMover = root;
		target = 1;
	}

	float step = dt / MP_VIEW_BLEND_TIME;
	s_viewBlend += target ? step : -step;
	if ( s_viewBlend > 1 )
		s_viewBlend = 1;
	if ( s_viewBlend <= 0 || s_viewMover <= 0 )
	{
		s_viewBlend = 0;
		return;
	}

	const mover_t *m = &s_movers[s_viewMover];
	Vector from( origin[0], origin[1], origin[2] ), moved;
	MP_TransformPoint( from, &m->shown, &m->smooth, moved );
	float b = s_viewBlend * m->drawBlend;
	for ( int i = 0; i < 3; i++ )
		origin[i] += ( moved[i] - origin[i] ) * b;
}

void MoverPrediction_Reset( void )
{
#ifdef SBO_TEST_COMMANDS
	static int s_registered = 0;
	if ( !s_registered )
	{
		s_registered = 1;
		gEngfuncs.pfnAddCommand( "test_cl_mp_trace", MP_TraceCommand );
		gEngfuncs.pfnAddCommand( "test_cl_mp_off", MP_OffCommand );
	}
#endif
	memset( s_movers, 0, sizeof( s_movers ) );
	for ( int i = 0; i < MP_MAX_ENTS; i++ )
		s_children[i].pass = 0;
	s_pass = 0;
	s_lastTau = -1.0;
	s_lastCallReal = -1.0;
	s_lastPassReal = -1.0;
	s_groundEnt = -1;
	s_lastGroundEnt = -1;
	s_clockValid = 0;
	s_clockReal = -1.0;
	s_clockShift = 0;
	s_viewMover = -1;
	s_viewBlend = 0;
	s_viewReal = -1.0;
}
