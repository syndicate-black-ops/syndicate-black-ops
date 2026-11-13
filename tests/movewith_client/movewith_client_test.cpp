// Offline test of the movewith client composition (src/cl_dll/movewith_client.cpp, docs/MOVEWITH.md "Client
// composition") on a stub engine: the real code places children on their parents for drawing (HUD_AddEntity,
// HUD_CreateEntities), prediction physents (HUD_PlayerMove) and event traces, from the entity states the server's
// AddToFullPack writes. Built with ASan/UBSan by run_test.sh.
#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "entity_types.h"
#include "pm_defs.h"
#include "movewith_shared.h"
#include "movewith_client.h"
#include "studio_interp.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------------------------- the stub engine

#define NUM_ENTS	64
static cl_entity_t s_Ents[ NUM_ENTS ];
static bool s_bPresent[ NUM_ENTS ];
static int s_iMaxClients = 8;
static int s_iGetEntityCalls = 0;
static model_t s_Brush, s_Studio, s_Sprite;

static cl_entity_t *Stub_GetEntityByIndex( int i )
{
	s_iGetEntityCalls++;
	return ( i >= 0 && i < NUM_ENTS && s_bPresent[i] ) ? &s_Ents[i] : NULL;
}
static void Stub_Con_Printf( const char *fmt, ... ) { (void)fmt; }
static int Stub_AddCommand( const char *, void ( * )( void ) ) { return 1; }
static float s_flClientTime = 0;
static float Stub_GetClientTime( void ) { return s_flClientTime; }
static int Stub_Cmd_Argc( void ) { return 0; }
static const char *Stub_Cmd_Argv( int ) { return ""; }
static cl_entity_t *Stub_GetLocalPlayer( void ) { return &s_Ents[1]; }
static cl_entity_t *Stub_GetViewModel( void ) { return NULL; }
static int Stub_GetMaxClients( void ) { return s_iMaxClients; }
static cvar_t s_ExInterp = { (char *)"ex_interp", (char *)"0.1", 0, 0.1f, NULL };
static cvar_t *Stub_GetCvarPointer( const char *name ) { return strcmp( name, "ex_interp" ) ? NULL : &s_ExInterp; }
static double Stub_GetAbsoluteTime( void ) { return s_flClientTime; }
// the renderer's attachment in model space (StudioModelRenderer.cpp): every studio model's attachment 1 sits at
// (10, 0, -20) of its model frame
static bool Stub_Attachments = true;
bool StudioAttachments_ModelSpace( struct cl_entity_s *ent, int n, float *out )
{
	if ( !Stub_Attachments || !ent || !ent->model || ent->model->type != mod_studio || n != 0 )
		return false;
	out[0] = 10; out[1] = 0; out[2] = -20;
	return true;
}

mw_stub_enginefuncs_t gEngfuncs = { Stub_GetEntityByIndex, Stub_Con_Printf, Stub_AddCommand, Stub_GetClientTime,
	Stub_Cmd_Argc, Stub_Cmd_Argv, Stub_GetLocalPlayer, Stub_GetViewModel, Stub_GetMaxClients, Stub_GetCvarPointer,
	Stub_GetAbsoluteTime };

static playermove_t s_PlayerMove;
extern "C" { playermove_t *pmove = &s_PlayerMove; }

static int s_iFailures = 0;
static int s_iChecks = 0;

#define CHECK( cond, ... ) do { s_iChecks++; if ( !( cond ) ) { s_iFailures++; printf( "FAIL %s:%d: ", __FILE__, __LINE__ ); \
	printf( __VA_ARGS__ ); printf( "\n" ); } } while ( 0 )

static bool Near( const float *a, float x, float y, float z, float tol = 0.01f )
{
	return fabsf( a[0] - x ) <= tol && fabsf( a[1] - y ) <= tol && fabsf( a[2] - z ) <= tol;
}

static void Reset( void )
{
	memset( s_Ents, 0, sizeof( s_Ents ) );
	memset( s_bPresent, 0, sizeof( s_bPresent ) );
	memset( &s_PlayerMove, 0, sizeof( s_PlayerMove ) );
	s_Brush.type = mod_brush;
	s_Studio.type = mod_studio;
	s_Sprite.type = mod_sprite;
	for ( int i = 0; i < NUM_ENTS; i++ )
		s_Ents[i].index = i;
	s_iMaxClients = 8;
	s_flClientTime = 0;
	MoveWith_VidInit();
}

// an entity received in packet `msg` with state origin / angles; drawn there (the engine's interpolation copies the
// state to origin / angles: callers move ent->origin for an interpolated frame)
static cl_entity_t *Ent( int i, model_t *model, float x, float y, float z, float yaw = 0, int iuser4 = 0, int msg = 1 )
{
	cl_entity_t *e = &s_Ents[i];
	s_bPresent[i] = true;
	e->model = model;
	e->curstate.messagenum = msg;
	e->curstate.origin[0] = x; e->curstate.origin[1] = y; e->curstate.origin[2] = z;
	e->curstate.angles[0] = 0; e->curstate.angles[1] = yaw; e->curstate.angles[2] = 0;
	e->curstate.iuser4 = iuser4;
	e->curstate.solid = SOLID_BSP;
	VectorCopy( e->curstate.origin, e->origin );
	VectorCopy( e->curstate.angles, e->angles );
	return e;
}

// one client frame: HUD_AddEntity for each present entity (players as ET_PLAYER), then HUD_CreateEntities;
// returns the number of entities the code refused to draw
static int Frame( void )
{
	int hidden = 0;
	for ( int i = 1; i < NUM_ENTS; i++ )
	{
		if ( !s_bPresent[i] )
			continue;
		int type = ( i <= s_iMaxClients ) ? ET_PLAYER : ET_NORMAL;
		if ( !MoveWith_AddEntity( type, &s_Ents[i] ) )
			hidden++;
	}
	MoveWith_CreateEntities();
	return hidden;
}

// physents of the present solid entities, as the engine builds them (from the entity states: origin / angles)
static void BuildPhysents( void )
{
	s_PlayerMove.numphysent = 0;
	for ( int i = 1; i < NUM_ENTS; i++ )
	{
		if ( !s_bPresent[i] || s_Ents[i].curstate.solid == SOLID_NOT )
			continue;
		physent_t *pe = &s_PlayerMove.physents[ s_PlayerMove.numphysent++ ];
		memset( pe, 0, sizeof( *pe ) );
		pe->info = i;
		pe->solid = s_Ents[i].curstate.solid;
		pe->player = ( i <= s_iMaxClients ) ? i : 0;
		VectorCopy( s_Ents[i].curstate.origin, pe->origin );
		VectorCopy( s_Ents[i].curstate.angles, pe->angles );
	}
}

// ---------------------------------------------------------------------------------------------- tests

// R7: a map with no client-placed child costs nothing in prediction and event traces (not one engine call)
static void Test_NoComposedNoCost( void )
{
	Reset();
	Ent( 1, &s_Studio, 0, 0, 36 );
	for ( int i = 10; i < 30; i++ )
		Ent( i, &s_Brush, (float)i * 10, 0, 0 );
	Frame();
	BuildPhysents();
	s_iGetEntityCalls = 0;
	MoveWith_PlayerMove( &s_PlayerMove );
	MoveWith_EventPhysents();
	CHECK( s_iGetEntityCalls == 0, "no composed child on the map: prediction asked the engine %d times", s_iGetEntityCalls );
}

// a child is drawn and predicted on its parent: parent at (100 0 0) turned 90, child's local pose (10 0 0)
static void Test_Compose( void )
{
	Reset();
	Ent( 1, &s_Studio, 0, 0, 36 );
	Ent( 10, &s_Brush, 100, 0, 0, 90 );
	cl_entity_t *child = Ent( 11, &s_Brush, 10, 0, 0, 0, 10 );
	CHECK( Frame() == 0, "nothing hidden" );
	CHECK( Near( child->origin, 100, 10, 0 ), "child drawn at %g %g %g, expected 100 10 0", child->origin[0], child->origin[1], child->origin[2] );
	CHECK( fabsf( child->angles[1] - 90 ) < 0.01f, "child yaw %g, expected 90", child->angles[1] );
	BuildPhysents();
	MoveWith_PlayerMove( &s_PlayerMove );
	physent_t *pe = NULL;
	for ( int i = 0; i < s_PlayerMove.numphysent; i++ )
		if ( s_PlayerMove.physents[i].info == 11 )
			pe = &s_PlayerMove.physents[i];
	CHECK( pe && Near( pe->origin, 100, 10, 0 ), "child physent at %g %g %g, expected 100 10 0", pe ? pe->origin[0] : 0, pe ? pe->origin[1] : 0, pe ? pe->origin[2] : 0 );
	// a parent missing from the packet hides the child
	s_Ents[10].curstate.messagenum = 0;
	CHECK( Frame() == 1, "child of a parent not in this packet is hidden" );
}

static physent_t *PhysentOf( int index )
{
	for ( int i = 0; i < s_PlayerMove.numphysent; i++ )
		if ( s_PlayerMove.physents[i].info == index )
			return &s_PlayerMove.physents[i];
	return NULL;
}

// C3 / R6: iuser4 that is not movewith's is left alone. A player's state carries other data there (a roaming Game
// Master's bit, the local player's own state from HUD_TxferLocalOverrides), an env_static prop its id, and an
// env_mirror copy is a copy of another entity's state: none of them is a server entity above maxclients.
static void Test_ForeignIuser4( void )
{
	Reset();
	cl_entity_t *player = Ent( 1, &s_Studio, 0, 0, 36, 0, 1 );		// GM bit
	cl_entity_t *other = Ent( 2, &s_Studio, 50, 0, 36, 0, 3 );		// a player with iuser4 3
	Ent( 10, &s_Brush, 100, 0, 0, 90 );
	Ent( 11, &s_Brush, 10, 0, 0, 0, 10 );			// a real child, so the map has one
	CHECK( Frame() == 0, "nothing hidden" );
	CHECK( Near( player->origin, 0, 0, 36 ) && Near( other->origin, 50, 0, 36 ), "players not moved" );

	// a client-made copy (env_static prop, mirror copy): not the engine's entity of its index
	cl_entity_t copy;
	memset( &copy, 0, sizeof( copy ) );
	copy.index = 11;
	copy.model = &s_Brush;
	copy.curstate.messagenum = 1;
	copy.curstate.iuser4 = 10;
	copy.origin[0] = 300; copy.origin[1] = 300;
	CHECK( MoveWith_AddEntity( ET_NORMAL, &copy ) == 1, "a copy is drawn" );
	CHECK( Near( copy.origin, 300, 300, 0 ), "a copy keeps its origin (got %g %g %g)", copy.origin[0], copy.origin[1], copy.origin[2] );
	cl_entity_t prop;
	memset( &prop, 0, sizeof( prop ) );
	prop.index = 0;
	prop.model = &s_Studio;
	prop.curstate.iuser4 = 7;	// an env_static prop id
	prop.origin[0] = 400;
	CHECK( MoveWith_AddEntity( ET_NORMAL, &prop ) == 1, "an env_static prop is drawn" );
	CHECK( Near( prop.origin, 400, 0, 0 ), "an env_static prop keeps its origin" );
	MoveWith_CreateEntities();

	// prediction / event traces: player physents untouched
	BuildPhysents();
	physent_t *pe1 = PhysentOf( 1 ), *pe2 = PhysentOf( 2 );
	MoveWith_PlayerMove( &s_PlayerMove );
	MoveWith_EventPhysents();
	CHECK( pe1 && Near( pe1->origin, 0, 0, 36 ), "local player physent unchanged (got %g %g %g)", pe1 ? pe1->origin[0] : 0, pe1 ? pe1->origin[1] : 0, pe1 ? pe1->origin[2] : 0 );
	CHECK( pe2 && Near( pe2->origin, 50, 0, 36 ), "other player physent unchanged (got %g %g %g)", pe2 ? pe2->origin[0] : 0, pe2 ? pe2->origin[1] : 0, pe2 ? pe2->origin[2] : 0 );
	physent_t *pc = PhysentOf( 11 );
	CHECK( pc && Near( pc->origin, 100, 10, 0 ), "the real child physent is composed" );
}

// a solid child whose parent is not in the latest packet (packet-entity limit): it is not drawn, and prediction
// doesn't collide with it at its local pose either (it would be a phantom near the map origin)
static void Test_PhysentParentMissing( void )
{
	Reset();
	Ent( 1, &s_Studio, 0, 0, 36 );
	Ent( 10, &s_Brush, 100, 0, 0, 0, 0, 1 );
	Ent( 11, &s_Brush, 10, 0, 0, 0, 10, 2 );		// newer packet than its parent's
	Ent( 12, &s_Brush, 500, 0, 0, 0, 0, 2 );
	Ent( 13, &s_Brush, 10, 0, 0, 0, 12, 2 );		// control: parent in the same packet
	CHECK( Frame() == 1, "the child of the missing parent is hidden" );
	BuildPhysents();
	MoveWith_PlayerMove( &s_PlayerMove );
	physent_t *pe = PhysentOf( 11 ), *pc = PhysentOf( 13 );
	CHECK( pe && pe->solid == SOLID_NOT, "its physent is not solid (solid %d)", pe ? pe->solid : -1 );
	// R19: the engine's player trace (hw.so _PM_PlayerTrace) skips a SOLID_NOT physent only when its skin is set (a
	// water volume) and traces every other one as a box at its origin: the physent goes far out of reach
	CHECK( pe && pe->origin[2] < -100000.0f, "its physent is out of reach (z %g)", pe ? pe->origin[2] : 0 );
	CHECK( pc && pc->solid != SOLID_NOT && Near( pc->origin, 510, 0, 0 ), "the control child is composed and solid" );
}

// R4: a child mover (a door opening on a train) is drawn from the local pose the engine interpolated (ex_interp in the
// past, like its parent's drawn pose), not from its newest state, which runs ahead of the parent and steps at the
// update rate. The engine writes the interpolated pose into origin / angles just before HUD_AddEntity.
static void Test_InterpolatedLocalPose( void )
{
	Reset();
	Ent( 1, &s_Studio, 0, 0, 36 );
	Ent( 10, &s_Brush, 100, 0, 0, 90 );
	cl_entity_t *child = Ent( 11, &s_Brush, 10, 0, 0, 0, 10 );
	Frame();							// linked (its first frame)
	s_flClientTime = 1.0f;				// long after the link
	child->origin[0] = 5; child->origin[1] = 0; child->origin[2] = 0;	// the engine's interpolation: half way between
	child->angles[0] = 0; child->angles[1] = 20; child->angles[2] = 0;	// local x 0 and 10, yaw 20 (newest state: 10, 0)
	Frame();
	CHECK( Near( child->origin, 100, 5, 0 ), "child mover drawn at %g %g %g, expected 100 5 0 (interpolated local pose)", child->origin[0], child->origin[1], child->origin[2] );
	CHECK( fabsf( child->angles[1] - 110 ) < 0.01f, "child mover yaw %g, expected 110", child->angles[1] );
	// prediction keeps the newest state, as the engine's physents of every mover do
	BuildPhysents();
	MoveWith_PlayerMove( &s_PlayerMove );
	physent_t *pe = PhysentOf( 11 );
	CHECK( pe && Near( pe->origin, 100, 10, 0 ), "child physent at %g %g %g, expected 100 10 0", pe ? pe->origin[0] : 0, pe ? pe->origin[1] : 0, pe ? pe->origin[2] : 0 );
}

// C4: right after a child is linked, unlinked or re-parented, the engine's interpolation mixes its world and local
// poses (states of both kinds within ex_interp): for ex_interp plus one update it is drawn from its newest state
static void Test_LinkSwitch( void )
{
	Reset();
	Ent( 1, &s_Studio, 0, 0, 36 );
	Ent( 10, &s_Brush, 100, 0, 0, 90 );
	cl_entity_t *child = Ent( 11, &s_Brush, 300, 0, 0 );	// not linked yet: its world pose
	Frame();
	s_flClientTime = 1.0f;
	Frame();
	// linked: the state is the local pose now; the engine's interpolation is still near the world pose
	child->curstate.origin[0] = 10;
	child->curstate.iuser4 = 10;
	child->origin[0] = 250; child->origin[1] = 0; child->origin[2] = 0;
	s_flClientTime = 1.05f;
	Frame();
	CHECK( Near( child->origin, 100, 10, 0 ), "just linked: drawn at %g %g %g, expected 100 10 0 (newest state)", child->origin[0], child->origin[1], child->origin[2] );
	// later: the interpolated pose again
	s_flClientTime = 1.5f;
	child->origin[0] = 8; child->origin[1] = 0; child->origin[2] = 0;
	child->angles[0] = 0; child->angles[1] = 0; child->angles[2] = 0;
	Frame();
	CHECK( Near( child->origin, 100, 8, 0 ), "settled: drawn at %g %g %g, expected 100 8 0 (interpolated)", child->origin[0], child->origin[1], child->origin[2] );
	// unlinked: the world pose of its newest state, not the engine's mix with the local poses
	child->curstate.iuser4 = 0;
	child->curstate.origin[0] = 120; child->curstate.origin[1] = 10;
	child->origin[0] = 60; child->origin[1] = 5; child->origin[2] = 0;
	s_flClientTime = 1.55f;
	Frame();
	CHECK( Near( child->origin, 120, 10, 0 ), "just unlinked: drawn at %g %g %g, expected 120 10 0 (newest state)", child->origin[0], child->origin[1], child->origin[2] );
	// an entity that never had a parent keeps the engine's pose
	cl_entity_t *other = Ent( 12, &s_Brush, 400, 0, 0 );
	other->origin[0] = 390;
	Frame();
	CHECK( Near( other->origin, 390, 0, 0 ), "a plain entity keeps the engine's interpolation (got %g)", other->origin[0] );
}

// Walk bug 2: a studio model child (an env_sprite showing a .mdl) is marked placed for the frame's drawing, so the
// studio renderer draws it with its placed origin and angles; nothing else is marked (a plain studio entity, a
// child whose parent is missing, a later frame without placing)
static void Test_StudioPlaced( void )
{
	Reset();
	Ent( 1, &s_Studio, 0, 0, 36 );
	Ent( 10, &s_Brush, 100, 0, 0, 90 );
	cl_entity_t *heli = Ent( 11, &s_Studio, 0, 0, 104, 180, 10 );
	cl_entity_t *plain = Ent( 12, &s_Studio, 50, 50, 0 );
	cl_entity_t *orphan = Ent( 13, &s_Studio, 0, 0, 10, 0, 20 );	// parent 20 not here
	Frame();
	CHECK( MoveWith_IsPlaced( heli ), "a placed studio child is drawn at its placed pose" );
	CHECK( Near( heli->origin, 100, 0, 104 ) && fabsf( heli->angles[1] - 270 ) < 0.01f, "placed at %g %g %g yaw %g, expected 100 0 104 yaw 270",
		heli->origin[0], heli->origin[1], heli->origin[2], heli->angles[1] );
	CHECK( !MoveWith_IsPlaced( plain ), "a plain studio entity is not marked" );
	CHECK( !MoveWith_IsPlaced( orphan ), "a child whose parent is missing is not marked" );
	// a mirror's copy of the child (same index, model and link) is drawn the same way; another model at that index not
	cl_entity_t copy = *heli;
	CHECK( MoveWith_IsPlaced( &copy ), "a client copy of the placed child is marked" );
	copy.model = &s_Brush;
	CHECK( !MoveWith_IsPlaced( &copy ), "a copy with another model is not marked" );
	// its parent gone from the next packet: not placed, not marked
	s_Ents[10].curstate.messagenum = 0;
	Frame();
	CHECK( !MoveWith_IsPlaced( heli ), "not placed this frame: not marked" );
}

// The studio renderer's step fraction (studio_interp.h) with the animation times a client got for a monster riding a
// train on a dedicated server: 37.5932 -> 37.5987 -> 37.5932, an unchanged time decoded a few ms apart. The renderer
// adds (fraction - 1) times the last move to the drawn origin: it was -9 and +17 (the monster 50-250 units off)
static void Test_StepFraction( void )
{
	float f = StudioStepFraction( 37.6423f, 37.5987f, 37.5932f );
	CHECK( f == 0.0f, "5.5 ms interval: fraction %g, expected 0 (no new update)", f );
	f = StudioStepFraction( 37.6700f, 37.5932f, 37.5987f );
	CHECK( f == 0.0f, "-5.5 ms interval: fraction %g, expected 0 (no new update)", f );
	f = StudioStepFraction( 10.05f, 10.0f, 9.9f );
	CHECK( fabsf( f - 0.5f ) < 0.001f, "a think 0.1 s after the last: fraction %g, expected 0.5", f );
	f = StudioStepFraction( 11.5f, 10.0f, 9.9f );
	CHECK( f == 0.0f, "1.5 s after the last update: fraction %g, expected 0", f );
}

// a model on attachment 1 of a studio model that is itself placed on a parent: on the attachment where that model is
// drawn this frame (its pose on its parent), not where the renderer last left its attachments (its local pose)
static void Test_ModelOnPlacedAttachment( void )
{
	Reset();
	Ent( 1, &s_Studio, 0, 0, 36 );
	Ent( 10, &s_Brush, 1000, 500, 0, 90 );						// the turning parent at yaw 90
	cl_entity_t *heli = Ent( 11, &s_Studio, 100, 0, 200, 0, 10 );	// on it, local (100, 0, 200)
	for ( int i = 0; i < 4; i++ )
	{
		heli->attachment[i][0] = 110; heli->attachment[i][1] = 0; heli->attachment[i][2] = 180;	// its local pose's
	}
	cl_entity_t *kid = Ent( 12, &s_Studio, 0, 0, 0, 0, 11 | ( 1 << MOVEWITH_STATE_ATTACHMENT_SHIFT ) );
	CHECK( Frame() == 0, "nothing hidden" );
	CHECK( Near( heli->origin, 1000, 600, 200 ), "helicopter drawn at %g %g %g, expected 1000 600 200", heli->origin[0], heli->origin[1], heli->origin[2] );
	// the attachment (10, 0, -20) of the helicopter turned to yaw 90: (0, 10, -20) from it
	CHECK( Near( kid->origin, 1000, 610, 180 ), "model on the attachment drawn at %g %g %g, expected 1000 610 180", kid->origin[0], kid->origin[1], kid->origin[2] );
	CHECK( fabsf( kid->angles[1] - 90 ) < 0.01f, "model yaw %g, expected 90", kid->angles[1] );
}

// the same on a parent that isn't a child (a monster, a model moved by its own code): where it is drawn this frame,
// not where its last draw left its attachments
static void Test_ModelOnMovingAttachment( void )
{
	Reset();
	Ent( 1, &s_Studio, 0, 0, 36 );
	cl_entity_t *mon = Ent( 11, &s_Studio, 500, 0, 100, 180 );
	for ( int i = 0; i < 4; i++ )
	{
		mon->attachment[i][0] = 300; mon->attachment[i][1] = 0; mon->attachment[i][2] = 80;	// last frame's draw
	}
	cl_entity_t *kid = Ent( 12, &s_Studio, 0, 0, 0, 0, 11 | ( 1 << MOVEWITH_STATE_ATTACHMENT_SHIFT ) );
	CHECK( Frame() == 0, "nothing hidden" );
	CHECK( Near( kid->origin, 490, 0, 80 ), "model on a moving parent's attachment drawn at %g %g %g, expected 490 0 80", kid->origin[0], kid->origin[1], kid->origin[2] );
}

int main( void )
{
	Test_ModelOnMovingAttachment();
	Test_ModelOnPlacedAttachment();
	Test_StepFraction();
	Test_StudioPlaced();
	Test_InterpolatedLocalPose();
	Test_LinkSwitch();
	Test_ForeignIuser4();
	Test_PhysentParentMissing();
	Test_NoComposedNoCost();
	Test_Compose();
	printf( "movewith_client: %d checks, %d failures\n", s_iChecks, s_iFailures );
	return s_iFailures ? 1 : 0;
}
