//=========================================================
// Entity parenting (movewith), client side: a child the server marks in entity_state_t::iuser4 is drawn on its
// parent's drawn (interpolated) pose. Its state carries a constant local pose, so it costs no network traffic while
// the parent moves, and it can't jitter against the parent at any ping. docs/MOVEWITH.md, server: dlls/movewith.cpp.
//
// HUD_AddEntity places it as each entity is added (a parent with a higher index than its child still has its last
// frame's pose then); HUD_CreateEntities places every such child again once all entities of the frame are in, parents
// first, before anything is drawn.
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "entity_types.h"
#include "movewith_shared.h"
#include "movewith_client.h"
#include "studio_attachments.h"
#include "pm_defs.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define MW_CL_MAX_ENTITIES	2048	// entity indices are sent with 11 bits
#define MW_CL_MAX_CHAIN		16
#define MW_OUT_OF_REACH		-262144.0f	// a z far below any map (the coordinates reach +-4096 .. 32768)

static int s_iComposed[ MW_CL_MAX_ENTITIES ];	// children placed this frame (entity indices)
static int s_iNumComposed = 0;
static bool s_bAnyComposed = false;	// a client-placed child came on this map: only then are physents patched (a map
									// that uses no movewith pays nothing in prediction and event traces)
static int s_iFrame = 1;
static int s_iDoneFrame[ MW_CL_MAX_ENTITIES ];	// s_iFrame when an entity was placed

// The local pose a child is drawn with: the engine's own interpolation of its states (ex_interp in the past, as its
// parent's drawn pose is), taken at its HUD_AddEntity, where the engine has just written it into origin / angles. A
// child mover (a door opening on a train) changes its local pose from update to update: drawn from the newest state
// it would step at the update rate, ex_interp ahead of its parent. A child not added yet this frame (placed early as
// another child's parent) uses its last frame's pose.
static float s_flLocalOrigin[ MW_CL_MAX_ENTITIES ][3];
static float s_flLocalAngles[ MW_CL_MAX_ENTITIES ][3];
static bool s_bLocalPose[ MW_CL_MAX_ENTITIES ];		// the two above hold a pose taken at a HUD_AddEntity

// The engine interpolates between the states it got in the last ex_interp seconds. When an entity starts or stops
// being placed on a parent, or gets another parent, those states mix world and local poses: for that long (plus an
// update) it is drawn from its newest state, not interpolated.
#define MW_SWITCH_MARGIN	0.1f	// seconds on top of ex_interp
// The studio renderer draws a placed child with the pose it was placed at (MoveWith_IsPlaced): the frame's draw pass
// each entity was last placed in
static int s_iPass = 1;								// this frame's pass (HUD_AddEntity .. HUD_CreateEntities)
static int s_iDrawPass = 0;							// the pass being drawn (after HUD_CreateEntities)
static int s_iPlacedPass[ MW_CL_MAX_ENTITIES ];

static int s_iLastLink[ MW_CL_MAX_ENTITIES ];		// iuser4 parent and attachment as last seen, 0 = not a child
static float s_flLinkTime[ MW_CL_MAX_ENTITIES ];	// client time it changed
extern "C" playermove_t *pmove;
static playermove_t *MW_Pmove( void ) { return pmove; }
static cvar_t *s_pInterp = NULL;					// ex_interp

static int MW_ParentIndex( const cl_entity_t *ent )
{
	return ent->curstate.iuser4 & MOVEWITH_STATE_PARENT_MASK;
}

// A child the server marked: a server entity above the players whose iuser4 names a parent that is not a player.
// Others carry other data in iuser4 and are left alone: players (a roaming Game Master's bit, the local player's own
// state), env_static props (their id) and the client's copies of entities (env_mirror), which are not the engine's
// entity of their index.
static bool MW_IsChild( cl_entity_t *ent )
{
	int iParent = ent ? MW_ParentIndex( ent ) : 0;
	if ( !iParent )
		return false;

	int maxClients = gEngfuncs.GetMaxClients();
	if ( ent->index <= maxClients || ent->index >= MW_CL_MAX_ENTITIES || iParent <= maxClients || ent->player )
		return false;
	return gEngfuncs.GetEntityByIndex( ent->index ) == ent;
}

// the parent index of a child this client places on its parent (mover prediction: such a child is predicted through
// its parent, and its chain moves as one body); 0 for any other entity
int MoveWith_ClientParent( cl_entity_t *ent )
{
	return MW_IsChild( ent ) ? MW_ParentIndex( ent ) : 0;
}

static bool MW_IsStudio( const cl_entity_t *ent )
{
	return ent->model && ent->model->type == mod_studio;
}

// the rotation the entity is drawn with
static void MW_EntityRot( const cl_entity_t *ent, const float *angles, mwmatrix_t m )
{
	float a[3] = { angles[0], angles[1], angles[2] };

	if ( MW_IsStudio( ent ) )
		a[0] = -a[0];
	MW_AngleMatrix( a, m );
}

// Places ent on its parent. false: the parent isn't in this frame (the child is not drawn).
// Where a child on attachment n (1 .. 4) of a studio parent goes: the attachment in the parent's model space for its
// animation this frame, put on the parent's pose this frame (parentRot: its drawn rotation). The attachments the
// renderer stores in the parent are from its last draw: last frame's pose, and for a parent that is itself a placed
// child, its local pose (a model on a placed helicopter's attachment stayed at a point near the world's origin).
static bool MW_AttachmentOrigin( cl_entity_t *parent, int n, mwmatrix_t parentRot, float *origin )
{
	float local[3], offset[3];
	if ( StudioAttachments_ModelSpace( parent, n - 1, local ) )
	{
		MW_Rotate( parentRot, local, offset );
		VectorAdd( parent->origin, offset, origin );
		return true;
	}
	VectorCopy( parent->attachment[ n - 1 ], origin );	// a model following another one's bones: the engine's
	return false;
}

static bool MW_Compose( cl_entity_t *ent, int depth )
{
	int index = ent->index;

	if ( index > 0 && index < MW_CL_MAX_ENTITIES && s_iDoneFrame[ index ] == s_iFrame )
		return true;
	if ( depth > MW_CL_MAX_CHAIN )
		return false;

	int iParent = MW_ParentIndex( ent );
	cl_entity_t *parent = gEngfuncs.GetEntityByIndex( iParent );

	// the parent must have come in the same packet as the child
	if ( !parent || parent == ent || !parent->model || parent->curstate.messagenum != ent->curstate.messagenum )
		return false;

	// a composed parent is placed first
	if ( MW_IsChild( parent ) && !MW_Compose( parent, depth + 1 ) )
		return false;

	// its local pose as the engine interpolated it (R4), else the newest state
	const float *localOrigin = ent->curstate.origin, *localAngles = ent->curstate.angles;
	if ( index > 0 && index < MW_CL_MAX_ENTITIES && s_bLocalPose[ index ] )
	{
		localOrigin = s_flLocalOrigin[ index ];
		localAngles = s_flLocalAngles[ index ];
	}

	mwmatrix_t parentRot, localRot, rot;
	MW_EntityRot( parent, parent->angles, parentRot );
	MW_AngleMatrix( localAngles, localRot );	// Quake angles, no studio flip (movewith_shared.h)
	MW_MatMul( parentRot, localRot, rot );

	int iAttachment = ( ent->curstate.iuser4 >> MOVEWITH_STATE_ATTACHMENT_SHIFT ) & MOVEWITH_STATE_ATTACHMENT_MASK;
	if ( iAttachment >= 1 && iAttachment <= 4 && MW_IsStudio( parent ) )
		MW_AttachmentOrigin( parent, iAttachment, parentRot, ent->origin );
	else
	{
		float offset[3];
		MW_Rotate( parentRot, localOrigin, offset );
		VectorAdd( parent->origin, offset, ent->origin );
	}

	float nearAngles[3] = { ent->angles[0], ent->angles[1], ent->angles[2] };
	bool bStudio = MW_IsStudio( ent );
	if ( bStudio )
		nearAngles[0] = -nearAngles[0];
	MW_MatrixAngles( rot, nearAngles, ent->angles );
	if ( bStudio )
		ent->angles[0] = -ent->angles[0];

	if ( index > 0 && index < MW_CL_MAX_ENTITIES )
	{
		s_iDoneFrame[ index ] = s_iFrame;
		s_iPlacedPass[ index ] = s_iPass;
	}
	return true;
}

// A studio model child placed this frame: the renderer draws it with origin and angles (StudioSetUpTransform). The
// engine's own pose for it is its local pose: curstate.angles (read for MOVETYPE_NONE), and for an entity the game
// marks EFLAG_SLERP (a model with an animation time, as env_sprite's) the engine sets MOVETYPE_STEP, whose studio
// interpolation goes back from origin toward the last state's origin, the local pose: the model was drawn near
// its local pose (where it started, or near the map's origin) instead of on its parent.
bool MoveWith_IsPlaced( const cl_entity_t *ent )
{
	if ( !s_bAnyComposed || !ent || ent->index <= 0 || ent->index >= MW_CL_MAX_ENTITIES )
		return false;
	if ( s_iPlacedPass[ ent->index ] != s_iDrawPass )
		return false;
	// the engine's entity of that index, or a client copy of it (env_mirror's reflection: same model and link)
	cl_entity_t *real = gEngfuncs.GetEntityByIndex( ent->index );
	return real == ent || ( real && real->model == ent->model && real->curstate.iuser4 == ent->curstate.iuser4 );
}

bool MoveWith_PlaceOnParent( int iParent, const float *localOrigin, const float *localAngles, float *origin, float *angles )
{
	int index = iParent & MOVEWITH_STATE_PARENT_MASK;
	cl_entity_t *parent = index > 0 ? gEngfuncs.GetEntityByIndex( index ) : NULL;
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();

	// the parent must be in the current packet (drawn this frame)
	if ( !parent || !local || !parent->model || parent->curstate.messagenum != local->curstate.messagenum )
		return false;
	if ( MW_IsChild( parent ) && !MW_Compose( parent, 1 ) )
		return false;

	mwmatrix_t parentRot, localRot, rot;
	MW_EntityRot( parent, parent->angles, parentRot );
	MW_AngleMatrix( localAngles, localRot );
	MW_MatMul( parentRot, localRot, rot );

	int iAttachment = ( iParent >> MOVEWITH_STATE_ATTACHMENT_SHIFT ) & MOVEWITH_STATE_ATTACHMENT_MASK;
	if ( iAttachment >= 1 && iAttachment <= 4 && MW_IsStudio( parent ) )
		MW_AttachmentOrigin( parent, iAttachment, parentRot, origin );
	else
	{
		float offset[3];
		MW_Rotate( parentRot, localOrigin, offset );
		VectorAdd( parent->origin, offset, origin );
	}

	float nearAngles[3] = { angles[0], angles[1], angles[2] };
	MW_MatrixAngles( rot, nearAngles, angles );
	return true;
}

// how long the engine's interpolation may still mix poses of before and after a link change
static float MW_SwitchTime( void )
{
	float flInterp = s_pInterp ? s_pInterp->value : 0.1f;
	if ( flInterp < 0 || flInterp > 1.0f )
		flInterp = 0.1f;
	return flInterp + MW_SWITCH_MARGIN;
}

int MoveWith_AddEntity( int type, cl_entity_t *ent )
{
	if ( type != ET_NORMAL || !ent )
		return 1;
	bool bChild = MW_IsChild( ent );
	if ( !bChild && !s_bAnyComposed )
		return 1;	// a map without movewith: nothing to do
	int index = ent->index;
	if ( index <= 0 || index >= MW_CL_MAX_ENTITIES || gEngfuncs.GetEntityByIndex( index ) != ent )
		return 1;	// a client-made copy (env_static, env_mirror): not the engine's entity of its index

	// C4: a link that changed lately: the newest state, not the engine's mix of world and local poses
	int iLink = bChild ? ( ent->curstate.iuser4 & ( MOVEWITH_STATE_PARENT_MASK | ( MOVEWITH_STATE_ATTACHMENT_MASK << MOVEWITH_STATE_ATTACHMENT_SHIFT ) ) ) : 0;
	float flNow = gEngfuncs.GetClientTime();
	if ( iLink != s_iLastLink[ index ] )
	{
		s_iLastLink[ index ] = iLink;
		s_flLinkTime[ index ] = flNow;
	}
	if ( flNow >= s_flLinkTime[ index ] && flNow - s_flLinkTime[ index ] < MW_SwitchTime() )
	{
		VectorCopy( ent->curstate.origin, ent->origin );
		VectorCopy( ent->curstate.angles, ent->angles );
	}
	if ( !bChild )
		return 1;

	// R4: the local pose the engine has just interpolated
	VectorCopy( ent->origin, s_flLocalOrigin[ index ] );
	VectorCopy( ent->angles, s_flLocalAngles[ index ] );
	s_bLocalPose[ index ] = true;

	s_bAnyComposed = true;
	if ( !MW_Compose( ent, 0 ) )
		return 0;	// its parent isn't here: nothing to draw it on

	if ( s_iNumComposed < MW_CL_MAX_ENTITIES )
		s_iComposed[ s_iNumComposed++ ] = ent->index;
	return 1;
}

#ifdef SBO_TEST_COMMANDS
// Test builds only (SBO_TEST_COMMANDS; until the entity tests' planned test_cl_trace replaces it):
// "test_cl_mw_trace a,b,... | off" logs the drawn origins and yaw of the listed entity indices once per frame ("MWT ...")
static int s_iTrace[ 8 ];
static int s_iNumTrace = 0;

static void MW_TraceCommand( void )
{
	s_iNumTrace = 0;
	// the tokenizer splits at commas: every argument, numbers separated by commas or spaces
	for ( int a = 1; a < gEngfuncs.Cmd_Argc() && s_iNumTrace < 8; a++ )
	{
		const char *sz = gEngfuncs.Cmd_Argv( a );
		if ( !strcmp( sz, "off" ) )
		{
			s_iNumTrace = 0;
			break;
		}
		if ( *sz >= '0' && *sz <= '9' )
			s_iTrace[ s_iNumTrace++ ] = atoi( sz );
	}
	gEngfuncs.Con_Printf( "test_cl_mw_trace: %d entities\n", s_iNumTrace );
}

static void MW_Trace( void )
{
	if ( s_iNumTrace < 2 )
		return;

	char line[ 512 ];
	int n = snprintf( line, sizeof( line ), "MWT %.4f", gEngfuncs.GetClientTime() );
	for ( int i = 0; i < s_iNumTrace && n < (int)sizeof( line ) - 64; i++ )
	{
		cl_entity_t *e = gEngfuncs.GetEntityByIndex( s_iTrace[i] );
		if ( e )
			n += snprintf( line + n, sizeof( line ) - n, " %d %.2f %.2f %.2f %.2f", s_iTrace[i], e->origin[0], e->origin[1], e->origin[2], e->angles[1] );
	}
	gEngfuncs.Con_Printf( "%s R %.4f\n", line, gEngfuncs.GetAbsoluteTime() );

	// the prediction's physents of the traced entities as this frame left them (what the engine's flashlight and
	// other traces of the frame meet): "MWP <time> <index> <origin> <yaw> ..."
	playermove_t *pm = MW_Pmove();
	if ( !pm )
		return;
	n = snprintf( line, sizeof( line ), "MWP %.4f", gEngfuncs.GetClientTime() );
	for ( int i = 0; i < s_iNumTrace && n < (int)sizeof( line ) - 64; i++ )
	{
		for ( int k = 0; k < pm->numphysent && k < MAX_PHYSENTS; k++ )
		{
			if ( pm->physents[k].info != s_iTrace[i] )
				continue;
			const physent_t &pe = pm->physents[k];
			n += snprintf( line + n, sizeof( line ) - n, " %d %.2f %.2f %.2f %.2f", s_iTrace[i], pe.origin[0], pe.origin[1], pe.origin[2], pe.angles[1] );
			break;
		}
	}
	gEngfuncs.Con_Printf( "%s\n", line );
}

// the main view of each frame while test_cl_mw_trace runs (the camera the player sees from)
void MoveWith_TraceView( const float *simorg, const float *vieworg )
{
	if ( s_iNumTrace < 2 )
		return;
	gEngfuncs.Con_Printf( "MWV %.4f sim %.2f %.2f %.2f view %.2f %.2f %.2f R %.4f\n", gEngfuncs.GetClientTime(), simorg[0],
		simorg[1], simorg[2], vieworg[0], vieworg[1], vieworg[2], gEngfuncs.GetAbsoluteTime() );
}

// where the studio renderer draws the last traced entity, from its model transform ("MWD <time> <index> <drawn origin>
// <drawn yaw> <origin> <yaw> <state movetype>"; once per frame)
void MoveWith_TraceDraw( cl_entity_t *ent, float (*transform)[4] )
{
	static int s_iLastFrame = -1;
	if ( s_iNumTrace < 1 || !ent || ent->index != s_iTrace[ s_iNumTrace - 1 ] || s_iLastFrame == s_iFrame )
		return;
	s_iLastFrame = s_iFrame;
	float yaw = atan2f( transform[1][0], transform[0][0] ) * 57.2957795f;
	gEngfuncs.Con_Printf( "MWD %.4f %d %.2f %.2f %.2f %.1f %.2f %.2f %.2f %.1f %d A %.4f %.4f %.4f P %.2f %.2f\n", gEngfuncs.GetClientTime(), ent->index,
		transform[0][3], transform[1][3], transform[2][3], yaw, ent->origin[0], ent->origin[1], ent->origin[2], ent->angles[1],
		ent->curstate.movetype, ent->curstate.animtime, ent->latched.prevanimtime, ent->prevstate.animtime, ent->latched.prevorigin[0],
		ent->latched.prevorigin[1] );
}
#endif // SBO_TEST_COMMANDS

void MoveWith_CreateEntities( void )
{
	// every parent has its pose for this frame now: place the children again, parents first (MW_Compose recurses)
	s_iFrame++;
	for ( int i = 0; i < s_iNumComposed; i++ )
	{
		cl_entity_t *ent = gEngfuncs.GetEntityByIndex( s_iComposed[i] );

		if ( MW_IsChild( ent ) )
			MW_Compose( ent, 0 );
	}
	s_iNumComposed = 0;
	s_iFrame++;	// the next frame's HUD_AddEntity calls place everything anew
	s_iDrawPass = s_iPass;	// what is drawn now was placed in this pass
	if ( ++s_iPass <= 0 )
	{
		memset( s_iPlacedPass, 0, sizeof( s_iPlacedPass ) );
		s_iPass = 1;
	}

#ifdef SBO_TEST_COMMANDS
	MW_Trace();	// the frame's poses: every child on its parent's drawn pose
#endif
}

//=========================================================
// Prediction: a solid child is sent with its local pose, so the engine's physents (built from the entity states)
// have it at that local pose. Before the player's movement is predicted, put every such physent on its parent's
// physent (the pose the engine predicts against for the parent: its latest state, or what mover prediction made of
// it), or on the parent's latest state when the parent isn't solid.
//=========================================================

// per MW_PatchList call (s_iStamp): entity index -> physent slot of that array, and the slots already composed
static int s_iStamp = 0;
static int s_iPhysentStamp[ MW_CL_MAX_ENTITIES ];
static short s_iPhysentSlot[ MW_CL_MAX_ENTITIES ];
static int s_iPatchedStamp[ MAX_PHYSENTS ];

static void MW_PhysentPose( physent_t *list, int count, int slot, int depth, float *origin, float *angles );

static bool MW_ParentPose( physent_t *list, int count, cl_entity_t *ent, int depth, float *origin, float *angles, cl_entity_t **ppParent )
{
	int iParent = MW_ParentIndex( ent );
	cl_entity_t *parent = gEngfuncs.GetEntityByIndex( iParent );

	// as for drawing: the parent must have come in the same packet
	if ( !parent || parent == ent || !parent->model || parent->curstate.messagenum != ent->curstate.messagenum )
		return false;
	*ppParent = parent;

	int slot = ( iParent > 0 && iParent < MW_CL_MAX_ENTITIES && s_iPhysentStamp[ iParent ] == s_iStamp ) ? s_iPhysentSlot[ iParent ] : -1;
	if ( slot >= 0 && slot < count )
	{
		MW_PhysentPose( list, count, slot, depth + 1, origin, angles );
		return true;
	}
	VectorCopy( parent->curstate.origin, origin );
	VectorCopy( parent->curstate.angles, angles );
	return true;
}

// the physent's world pose (composes it first if it is a placed child)
static void MW_PhysentPose( physent_t *list, int count, int slot, int depth, float *origin, float *angles )
{
	physent_t *pe = &list[ slot ];
	cl_entity_t *ent = ( pe->info > 0 && pe->info < MW_CL_MAX_ENTITIES && !pe->player ) ? gEngfuncs.GetEntityByIndex( pe->info ) : NULL;

	if ( s_iPatchedStamp[ slot ] != s_iStamp && MW_IsChild( ent ) && depth <= MW_CL_MAX_CHAIN )
	{
		s_iPatchedStamp[ slot ] = s_iStamp;
		float parentOrigin[3], parentAngles[3];
		cl_entity_t *parent;
		if ( !MW_ParentPose( list, count, ent, depth, parentOrigin, parentAngles, &parent ) )
		{
			// its parent isn't in the latest packet (the packet-entity limit): it isn't drawn, and nothing may collide
			// with it at its local pose either. SOLID_NOT alone doesn't do that: the engine's player trace skips a
			// non-solid physent only when it is a water volume (skin set, R19), so it goes far out of reach.
			pe->solid = SOLID_NOT;
			pe->origin[0] = pe->origin[1] = 0;
			pe->origin[2] = MW_OUT_OF_REACH;
		}
		else
		{
			mwmatrix_t parentRot, localRot, rot;
			float offset[3];
			MW_EntityRot( parent, parentAngles, parentRot );
			MW_AngleMatrix( ent->curstate.angles, localRot );
			MW_MatMul( parentRot, localRot, rot );
			MW_Rotate( parentRot, ent->curstate.origin, offset );
			VectorAdd( parentOrigin, offset, pe->origin );

			float nearAngles[3] = { pe->angles[0], pe->angles[1], pe->angles[2] };
			bool bStudio = MW_IsStudio( ent );
			if ( bStudio )
				nearAngles[0] = -nearAngles[0];
			MW_MatrixAngles( rot, nearAngles, pe->angles );
			if ( bStudio )
				pe->angles[0] = -pe->angles[0];
		}
	}
	VectorCopy( pe->origin, origin );
	VectorCopy( pe->angles, angles );
}

static void MW_PatchList( physent_t *list, int count )
{
	if ( count <= 0 || count > MAX_PHYSENTS )
		return;

	if ( ++s_iStamp <= 0 )
	{
		memset( s_iPhysentStamp, 0, sizeof( s_iPhysentStamp ) );
		memset( s_iPatchedStamp, 0, sizeof( s_iPatchedStamp ) );
		s_iStamp = 1;
	}
	for ( int i = 0; i < count; i++ )
	{
		if ( list[i].info > 0 && list[i].info < MW_CL_MAX_ENTITIES )
		{
			s_iPhysentStamp[ list[i].info ] = s_iStamp;
			s_iPhysentSlot[ list[i].info ] = (short)i;
		}
	}
	for ( int i = 0; i < count; i++ )
	{
		float origin[3], angles[3];
		if ( list[i].info > 0 && list[i].info < MW_CL_MAX_ENTITIES )
			MW_PhysentPose( list, count, i, 0, origin, angles );
	}
}

void MoveWith_PlayerMove( struct playermove_s *ppmove )
{
	if ( !s_bAnyComposed )
		return;
	// every call (once per predicted command): idempotent (composed from the entity states), and a parent's physent
	// may differ per command (mover prediction)
	MW_PatchList( ppmove->physents, ppmove->numphysent );
	MW_PatchList( ppmove->moveents, ppmove->nummoveent );
	MW_PatchList( ppmove->visents, ppmove->numvisent );
}

// client event traces (bullet impacts, decals): the engine traces against the same physents as the prediction;
// after EV_SetSolidPlayers the composed children are put where they are drawn again

void MoveWith_EventPhysents( void )
{
	if ( s_bAnyComposed && pmove )
		MoveWith_PlayerMove( pmove );
}

void MoveWith_VidInit( void )
{
#ifdef SBO_TEST_COMMANDS
	static bool s_bRegistered = false;
	if ( !s_bRegistered )
	{
		s_bRegistered = true;
		gEngfuncs.pfnAddCommand( "test_cl_mw_trace", MW_TraceCommand );
	}
#endif
	s_iNumComposed = 0;
	s_bAnyComposed = false;
	s_iFrame = 1;
	memset( s_iDoneFrame, 0, sizeof( s_iDoneFrame ) );
	memset( s_bLocalPose, 0, sizeof( s_bLocalPose ) );
	memset( s_iPlacedPass, 0, sizeof( s_iPlacedPass ) );
	s_iPass = 1;
	s_iDrawPass = 0;
	memset( s_iLastLink, 0, sizeof( s_iLastLink ) );
	for ( int i = 0; i < MW_CL_MAX_ENTITIES; i++ )
		s_flLinkTime[i] = -1.0e6f;
	s_pInterp = gEngfuncs.pfnGetCvarPointer( "ex_interp" );
}
