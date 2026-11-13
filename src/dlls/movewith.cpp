//=========================================================
// Entity parenting: "movewith" (Spirit of Half-Life) / "parent" (XashXT / PrimeXT), "parentflags", and
// trigger_changeparent. Mapper guide and design: docs/MOVEWITH.md.
//
// Written for SBO on the stock GoldSrc engine. The mapper keys follow XashXT / PrimeXT (read for ideas only, no code
// taken) and Spirit of Half-Life (LogicAndTrick halflife-updated spirit/spirit-1.8-bug-fixes 2482306, dlls/movewith.cpp:
// the key on every entity, children carried by velocity so the engine pushes riders). Spirit's problems are not
// repeated (docs/MOVEWITH.md "Where it comes from"):
//  - a child's pose is the parent's pose times a fixed local pose, rotation included, so children orbit a turning
//    parent (Spirit only copied the angular velocity);
//  - every server frame (StartFrame, the engine's frame time) a solid child gets the velocity that puts it exactly on
//    its target at the end of the frame (closed loop; nothing drifts, nothing is snapped, riders are pushed by the
//    engine), instead of open-loop velocity copying corrected by teleports;
//  - entities that are not parented are untouched: no think rewrite, nothing in PlayerPostThink;
//  - links are EHANDLEs, checked every frame.
//=========================================================

// STL before the SDK headers (their min / max macros)
#include	<vector>
#include	<algorithm>

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"movewith.h"
#include	"movewith_shared.h"
#include	"entity_state.h"
#include	"studio.h"	// numattachments (MW_HasAttachment)

#define MW_IDSTUDIOHEADER	( ( 'T' << 24 ) + ( 'S' << 16 ) + ( 'D' << 8 ) + 'I' )	// "IDST", a studio model's header

#include	<math.h>
#include	<time.h>
#include	<string.h>
#include	<ctype.h>
#include	<stdlib.h>

#ifndef MAX_EDICTS
#define MAX_EDICTS	2048
#endif

#define MW_MAX_DEPTH		16			// children of children of ... deeper chains are refused
#define MW_CATCHUP_SPEED	256.0f		// units/s: an error a child already has (after a block) is caught up at most this fast
#define MW_CATCHUP_ANGLE	180.0f		// degrees/s, per angle
#define MW_JUMP_DISTANCE	8.0f		// a parent this far (units) from where the engine was to move it has teleported
#define MW_JUMP_ANGLE		5.0f		// degrees, per angle
#define MW_ETERNAL_THINK	1.0e6f		// a child pusher's think time when it has none of its own: the engine moves it all frame
#define MW_BLOCK_INTERVAL	0.25f
#define MW_CUT_TIME			0.001f		// seconds: a held chain's move back is cut here by its think time (exact in any frame)
#define MW_SNAP_FRAMETIME	0.25f		// seconds: a longer frame places children instead of steering them		// seconds: a child forwards a block to its root at most this often

enum
{
	MW_KIND_PUSH = 0,	// solid brush: MOVETYPE_PUSH, steered by velocity, pushes riders
	MW_KIND_NOCLIP,		// solid point entity, or one with a dynamic light: MOVETYPE_NOCLIP, steered by velocity
	MW_KIND_POSITION,	// non-solid model / sprite / brush, point entity without a model: placed each frame (server);
						// the client places a sent one on its parent (iuser4, AddToFullPack)
	MW_KIND_TRIGGER,	// brush trigger, trigger_camera: placed each frame, position only
	MW_KIND_FOLLOW,		// sprite on an attachment of a studio model: the engine's MOVETYPE_FOLLOW (env_sprite's SetAttachment)
};

static const char *s_szKindName[] = { "pushed", "noclip", "placed", "trigger", "attachment" };

struct MoveWithNode
{
	BOOL inUse;
	EHANDLE hChild;
	EHANDLE hParent;
	int childIndex;
	string_t iszParent;		// the parent's name as the map gives it (without ".N")
	int attachment;			// 1 .. 4, 0 = the parent's origin
	int flags;				// parentflags
	int kind;
	int depth;				// 1 = child of a root
	BOOL orphan;			// the parent was removed: it stays where it is, for good (a round start links nothing)
	BOOL converted;			// MOVETYPE changed by us (ltime follows the server time)
	BOOL compose;			// may be placed by the client on its parent

	// the parent's pose when linked (the editor pose at the map start)
	Vector bindOrigin;
	mwmatrix_t bindRot;

	// the child's own pose in the frame of the parent as linked; moves only for child movers (CBaseToggle)
	Vector ownOrigin, ownAngles;
	Vector ownVelocity, ownAvelocity;
	float ownLtime;

	Vector lockOrigin, lockAngles;	// the child's world pose when linked (parentflags)

	// the local pose sent to the client: origin, and rotation as Quake angles (no studio pitch flip); for a child
	// mover, the own pose it was computed from
	Vector stateOrigin, stateAngles;
	Vector attachLocalOrigin;		// the local pose's origin, sent while the attachment is missing (model changed)
	BOOL attachMissing;				// MW_Attachment found the attachment gone (reported once)
	Vector sentOwnOrigin, sentOwnAngles;

	// this frame
	Vector endOrigin, endAngles;	// where the child will be at the end of this frame
	mwmatrix_t endRot;
	BOOL jumped;					// teleported this frame (its children follow at once)
	BOOL assigned;					// given a new place since the last frame (MoveWith_AssignOrigin): jumps

	// the parent's pose the engine was to reach at the end of the last frame (roots only; teleport detection)
	BOOL predValid;
	Vector predOrigin, predAngles;
	// a pusher parent's pose, motion and ltime at the start of the last frame: the engine moves a pusher by the time
	// its ltime advances, so that, not the frame time the game was given, says where it was to be now
	BOOL predPusher;
	Vector predBaseOrigin, predBaseAngles, predVelocity, predAvelocity;
	float predLtime;

	float nextBlock;
	float moveTime;					// this frame's movetime of a pushed child (MW_UpdateNode), for its riders

	// chain hold (a block anywhere in the chain): the frame's start, to undo the frame's advance of this node
	int rootIndex;			// the top of its chain
	BOOL held;				// its chain is held: own movement and think time frozen
	BOOL noOwnThink;		// a pusher without a think of its own: its own velocity doesn't move it (as the engine)
	Vector triggerCenter;	// a brush trigger: its box's centre in its own frame at the link (the box itself never turns)
	float heldLtime, heldNextthink;
	BOOL thinkSwapped;		// its think function is SUB_DoNothing while its held chain's move is cut (MW_SwapThink)
	BASEPTR savedThink;
	float startLtime;
	Vector startOwnOrigin, startOwnAngles;
};

// The top of a chain (a parent that is not a child), per entity index: its pose and ltime at the start of the frame,
// and the frame a part of its chain was blocked in. MoveWith_Blocked holds the whole chain: the next frame puts the
// root back where the frame started (as the engine does with a blocked pusher) and undoes every child's advance of its
// own movement, so nothing in the chain advances while a part of it is blocked, and the chain stays in step.
struct MoveWithRoot
{
	BOOL root;				// this entity was the top of a chain (set in MW_ParentPoses); never TRUE for an
							// entity no child moves with, so a stock mover is never taken for a chain root
	int frame;				// s_iFrame the start values were taken in (s_iFrame starts at 1: 0 is "never")
	Vector origin, angles;	// at the start of that frame
	float ltime;
	int heldFrame;			// s_iFrame a part of the chain was blocked in
	int revertedFrame;		// s_iFrame the root was moved back or held in (no teleport detection then)

	// a held chain: the root's own movement and timers, kept while it is held and given back on release
	BOOL held;
	BOOL cutting;			// this frame's move back is cut by its think time (MW_CUT_TIME)
	BOOL thinkSwapped;		// its think function is SUB_DoNothing while held (MW_SwapThink)
	BASEPTR savedThink;
	float holdUntil;
	Vector backOrigin, backAngles;	// where it goes back to (the start of the blocked frame)
	float frozenLtime;
	Vector savedVelocity, savedAvelocity;
	float savedNextthink;

	// what the hold wrote into the root last frame: anything else there now was its own code (a Use, its own Blocked,
	// train_setspeed, trigger_changevalue), and the hold then ends keeping that, not the state it saved
	BOOL wrote;
	Vector writtenVelocity, writtenAvelocity;
	float writtenNextthink;
};

#define MW_HOLD_TIME	0.1f	// seconds a blocked chain waits before it tries again

struct MoveWithEntry
{
	int serial;				// edict serial number this entry belongs to
	string_t iszMoveWith;	// "movewith"
	string_t iszParent;		// "parent"
	int parentflags;
	BOOL placed;
	Vector placedOrigin;	// before Spawn
	Vector placedAngles;	// before Spawn (0 when Spawn cleared them: the angles key was a move direction)
	int node;				// index in s_Nodes, -1 = none
	BOOL converted;			// its MOVETYPE was changed for a link (kept across trigger_changeparent)
};

static MoveWithEntry s_Entries[ MAX_EDICTS ];
static MoveWithRoot s_Roots[ MAX_EDICTS ];

// a rider carried by MW_CarryTurningRiders: the child pose it was carried to last frame (the target, not where the
// engine's move ended), so the next carry measures from the same pose and nothing adds up from frame to frame (closed
// loop, as the steering)
static int s_iCarryFrame[ MAX_EDICTS ];			// s_iFrame of the carry, 0 = none
static int s_iCarryGround[ MAX_EDICTS ];		// the child's entity index
static Vector s_vecCarryOrigin[ MAX_EDICTS ];	// the child's pose the rider was carried with
static Vector s_vecCarryAngles[ MAX_EDICTS ];
static Vector s_vecCarryLift[ MAX_EDICTS ];		// how far it was lifted clear of the child's half-done pose
static int s_iFrame = 1;
static std::vector<MoveWithNode> s_Nodes;
static std::vector<int> s_Order;	// node indices, parents before children
static BOOL s_bDirty = FALSE;
static BOOL s_bActive = FALSE;		// between ServerActivate and ServerDeactivate
static BOOL s_bFrameTimeChecked = FALSE;

// developer 2: the largest distance of a steered child from its target at the start of a frame (after the engine
// moved it), reported every 10 s (MW_Report)
static float s_flWorstError = 0;
static float s_flWorstAngle = 0;
static string_t s_iszWorst = 0;
static float s_flNextReport = 0;
static int s_iFrameTimeOff = 0;		// frames where a child's ltime advanced by a time other than the frametime given
static float s_flLastFrameTime = 0;
static float s_flPrevFrameStart = 0;	// gpGlobals->time at the last StartFrame
static float s_flWorstFrameTimeDiff = 0;	// largest |gpGlobals->frametime - the previous frame's real length| (report)
static BOOL s_bSnapFrame = FALSE;

static BOOL MW_Sendable( edict_t *pent );
static void MW_SoundCatchUp( edict_t *pSoundEnt );
static void MW_SoundFrame( void );

struct MoveWithSound
{
	EHANDLE hAmbient;
	EHANDLE hSoundEnt;
	BOOL pending;			// a catch-up is due (rate-limited)
	float nextRefresh;
	// the entity and sample its sound last started on (MoveWith_SoundStarted): its changes and its stop go there, also
	// once that entity is no longer sent or the ambient lost its parent; 0 = none
	int startedIndex;
	int startedSerial;
	char startedSample[ 64 ];
};

struct MoveWithSeen		// a sound entity: when each client last received it (AddToFullPack)
{
	int index;
	float lastSent[ 33 ];
};

static std::vector<MoveWithSound> s_Sounds;
static std::vector<MoveWithSeen> s_Seen;

#define MW_SOUND_REFRESH_INTERVAL	1.0f	// seconds: catch-ups of one sound at most this often
#define MW_SOUND_PVS_GAP			0.5f	// a sound entity not sent to a client for this long has left its PVS


//=========================================================
// math (game_shared/movewith_shared.h), with Vector results
//=========================================================

static Vector MW_RotateV( const mwmatrix_t m, const Vector &v )
{
	Vector r;
	MW_Rotate( m, v, r );
	return r;
}

static Vector MW_IRotateV( const mwmatrix_t m, const Vector &v )
{
	Vector r;
	MW_IRotate( m, v, r );
	return r;
}

static Vector MW_MatrixAnglesV( const mwmatrix_t m, const Vector &vecNear )
{
	Vector r;
	MW_MatrixAngles( m, vecNear, r );
	return r;
}

//=========================================================
// entities
//=========================================================

static BOOL MW_ModelEndsWith( edict_t *pent, const char *szExt )
{
	if ( !pent->v.model )
		return FALSE;

	const char *szModel = STRING( pent->v.model );
	size_t len = strlen( szModel ), ext = strlen( szExt );
	if ( len <= ext )
		return FALSE;
	for ( size_t i = 0; i < ext; i++ )
	{
		if ( tolower( (unsigned char)szModel[ len - ext + i ] ) != tolower( (unsigned char)szExt[i] ) )
			return FALSE;
	}
	return TRUE;
}

static BOOL MW_IsStudio( edict_t *pent ) { return MW_ModelEndsWith( pent, ".mdl" ); }

// attachment n (1 .. 4) exists on the studio model the entity has loaded. The engine's GET_ATTACHMENT reads the
// model's attachment and bone arrays without a bound, so nothing asks it for one this does not confirm. The name
// check comes first: the engine's GET_MODEL_PTR (Mod_Extradata) stops the server on a brush model.
static BOOL MW_HasAttachment( edict_t *pent, int n )
{
	if ( n < 1 || n > 4 || !pent->v.modelindex || !MW_IsStudio( pent ) )
		return FALSE;

	studiohdr_t *pHdr = (studiohdr_t *)GET_MODEL_PTR( pent );
	return pHdr && pHdr->id == MW_IDSTUDIOHEADER && n <= pHdr->numattachments;
}
static BOOL MW_IsSprite( edict_t *pent ) { return MW_ModelEndsWith( pent, ".spr" ); }
static BOOL MW_IsBrush( edict_t *pent ) { return pent->v.model && ( STRING( pent->v.model ) )[0] == '*'; }

// the rotation the entity is drawn and collided with
static void MW_EntityRot( edict_t *pent, const Vector &angles, mwmatrix_t m )
{
	Vector a = angles;

	if ( MW_IsStudio( pent ) )
		a.x = -a.x;
	MW_AngleMatrix( a, m );
}

static Vector MW_EntityAngles( edict_t *pent, const mwmatrix_t m, const Vector &vecNear )
{
	if ( MW_IsStudio( pent ) )
	{
		Vector a = MW_MatrixAnglesV( m, Vector( -vecNear.x, vecNear.y, vecNear.z ) );
		a.x = -a.x;
		return a;
	}

	return MW_MatrixAnglesV( m, vecNear );
}

static MoveWithEntry *MW_Entry( edict_t *pent )
{
	if ( !pent )
		return NULL;

	int i = ENTINDEX( pent );
	if ( i <= 0 || i >= MAX_EDICTS )
		return NULL;

	MoveWithEntry *e = &s_Entries[ i ];
	if ( e->serial != pent->serialnumber )
	{
		memset( e, 0, sizeof( *e ) );
		e->serial = pent->serialnumber;
		e->node = -1;
	}
	return e;
}

static MoveWithNode *MW_NodeOf( edict_t *pent )
{
	MoveWithEntry *e = MW_Entry( pent );

	if ( !e || e->node < 0 || e->node >= (int)s_Nodes.size() )
		return NULL;

	MoveWithNode *n = &s_Nodes[ e->node ];
	if ( !n->inUse || n->hChild.Get() != pent )
	{
		e->node = -1;
		return NULL;
	}
	return n;
}

// the top of the chain above a node (the first parent that is not itself a linked child)
static edict_t *MW_ChainRoot( MoveWithNode *n )
{
	edict_t *pRoot = n->hParent.Get();
	for ( int depth = 0; depth <= MW_MAX_DEPTH; depth++ )
	{
		MoveWithNode *pn = MW_NodeOf( pRoot );
		if ( !pn || pn->orphan || !pn->hParent.Get() )
			break;
		pRoot = pn->hParent.Get();
	}
	return pRoot;
}

static BOOL MW_Gone( edict_t *pent )
{
	return !pent || pent->free || !pent->pvPrivateData || ( pent->v.flags & FL_KILLME );
}

static const char *MW_Name( edict_t *pent )
{
	if ( !pent )
		return "?";
	if ( pent->v.targetname )
		return STRING( pent->v.targetname );
	return STRING( pent->v.classname );
}

// how far the engine moves a pusher this frame (SV_Physics_Pusher: up to its think time, nothing without one)
static float MW_PusherMovetime( entvars_t *pev, float frametime )
{
	float thinktime = pev->nextthink;

	if ( thinktime < pev->ltime + frametime )
	{
		float movetime = thinktime - pev->ltime;
		return ( movetime < 0 ) ? 0 : movetime;
	}
	return frametime;
}

// The length of this server frame, for the steering: the engine moves every pusher by it. Not gpGlobals->frametime:
// in StartFrame that holds the length of the last player command the server ran (SV_RunCmd sets it per command), not
// the frame's. The last frame's length (from the server time) is the measure; a frame much longer than the one
// before (a hitch) is still unknown here, so a held chain's move back is cut by its think time instead (MW_CUT_TIME).
static float s_flFrameTime = 0;

// While its chain's move back is cut by its think time, the engine calls the entity's think when the move ends: its
// own think function waits (SUB_DoNothing) and comes back when the hold ends, unless its own code set another since.
static void MW_SwapThink( edict_t *pent, BOOL &swapped, BASEPTR &saved )
{
	if ( swapped )
		return;
	CBaseEntity *pEntity = CBaseEntity::Instance( pent );
	if ( !pEntity )
		return;
	saved = pEntity->m_pfnThink;
	pEntity->m_pfnThink = &CBaseEntity::SUB_DoNothing;
	swapped = TRUE;
}

static void MW_RestoreThink( edict_t *pent, BOOL &swapped, BASEPTR &saved )
{
	if ( !swapped )
		return;
	swapped = FALSE;
	CBaseEntity *pEntity = ( !pent || pent->free || !pent->pvPrivateData ) ? NULL : CBaseEntity::Instance( pent );
	if ( pEntity && pEntity->m_pfnThink == &CBaseEntity::SUB_DoNothing )
		pEntity->m_pfnThink = saved;
}

//=========================================================
// keys and the editor pose
//=========================================================

BOOL MoveWith_KeyValue( edict_t *pent, KeyValueData *pkvd )
{
	BOOL bMoveWith = FStrEq( pkvd->szKeyName, "movewith" );
	BOOL bParent = FStrEq( pkvd->szKeyName, "parent" );

	if ( !bMoveWith && !bParent && !FStrEq( pkvd->szKeyName, "parentflags" ) )
		return FALSE;

	// env_particle follows an entity on the client already: the key names that entity ("target"), unless the
	// map gives a target (docs/AURORA.md)
	if ( pkvd->szClassName && FStrEq( pkvd->szClassName, "env_particle" ) )
	{
		if ( ( bMoveWith || bParent ) && pkvd->szValue[0] && FStringNull( pent->v.target ) )
			pent->v.target = ALLOC_STRING( pkvd->szValue );
		return TRUE;
	}

	MoveWithEntry *e = MW_Entry( pent );
	if ( !e )
		return FALSE;

	if ( bMoveWith )
		e->iszMoveWith = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : 0;
	else if ( bParent )
		e->iszParent = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : 0;
	else
		e->parentflags = atoi( pkvd->szValue );
	return TRUE;
}

void MoveWith_PreSpawn( edict_t *pent )
{
	MoveWithEntry *e = MW_Entry( pent );

	if ( !e )
		return;
	e->placed = TRUE;
	e->placedOrigin = pent->v.origin;
	e->placedAngles = pent->v.angles;
}

void MoveWith_PostSpawn( edict_t *pent )
{
	MoveWithEntry *e = MW_Entry( pent );

	// Spawn cleared the angles: they were a move direction (SetMovedir: func_door, func_button, func_plat...), and
	// the entity is placed unturned
	if ( e && e->placed && pent->v.angles == g_vecZero )
		e->placedAngles = g_vecZero;
}

//=========================================================
// links
//=========================================================

static int MW_AllocNode( void )
{
	for ( int i = 0; i < (int)s_Nodes.size(); i++ )
	{
		if ( !s_Nodes[i].inUse )
			return i;
	}
	s_Nodes.push_back( MoveWithNode() );
	return (int)s_Nodes.size() - 1;
}

static void MW_FreeNode( int i )
{
	MoveWithNode &n = s_Nodes[i];
	edict_t *pent = n.hChild.Get();

	if ( pent )
	{
		MoveWithEntry *e = MW_Entry( pent );
		if ( e && e->node == i )
			e->node = -1;
	}
	n.inUse = FALSE;
	n.hChild = NULL;
	n.hParent = NULL;
	s_bDirty = TRUE;
}

// the depth of a node: 1 under a root; 0 when the chain loops (guarded)
static int MW_Depth( MoveWithNode *n )
{
	int depth = 0;

	while ( n && !n->orphan && depth <= MW_MAX_DEPTH + 1 )
	{
		depth++;
		n = MW_NodeOf( n->hParent.Get() );
	}
	return depth;
}

static void MW_SortNodes( void )
{
	s_Order.clear();
	for ( int i = 0; i < (int)s_Nodes.size(); i++ )
	{
		if ( !s_Nodes[i].inUse )
			continue;
		s_Nodes[i].depth = MW_Depth( &s_Nodes[i] );
		s_Order.push_back( i );
	}
	std::stable_sort( s_Order.begin(), s_Order.end(), []( int a, int b ) { return s_Nodes[a].depth < s_Nodes[b].depth; } );
	s_bDirty = FALSE;
}

static void MW_StopEntity( edict_t *pent )
{
	pent->v.velocity = g_vecZero;
	pent->v.avelocity = g_vecZero;
}

// child classes whose own movement is in world coordinates: they can be parents, not children
static BOOL MW_RefusedChildClass( edict_t *pent )
{
	static const char *s_szRefused[] = { "func_train", "func_tracktrain", "func_rotating", "func_pendulum",
		"func_vehicle", "env_modeltrain", "env_spritetrain", NULL };

	for ( int i = 0; s_szRefused[i]; i++ )
	{
		if ( FClassnameIs( &pent->v, s_szRefused[i] ) )
			return TRUE;
	}
	return FALSE;
}

// classes each player's game draws from a copy it is sent once (a place, a plane or a volume): moving the server's
// entity would move nothing the players see. point_worldtext and a vgui_screen without "surface" follow a parent on
// the clients instead (MoveWith_ClientLink).
static BOOL MW_ClientCopyClass( edict_t *pent )
{
	static const char *s_szCopied[] = { "env_mirror", "env_static", "func_rainvolume", "func_snowvolume",
		"color_correction_volume", "env_fogzone", NULL };

	for ( int i = 0; s_szCopied[i]; i++ )
	{
		if ( FClassnameIs( &pent->v, s_szCopied[i] ) )
			return TRUE;
	}
	return FALSE;
}

// a parent, or one above it, can turn: a rotating or swinging brush, a train on a track, a vehicle, a model (monsters
// and models turn), or anything with a turning ("angular") move. A plain lift or sliding door never turns.
static BOOL MW_ChainCanTurn( edict_t *pParent )
{
	static const char *s_szTurning[] = { "func_rotating", "func_door_rotating", "func_pendulum", "func_tracktrain",
		"func_vehicle", "func_platrot", "func_rot_button", "momentary_rot_button", "func_tank", "func_tanklaser",
		"func_tankrocket", "func_tankmortar", NULL };

	edict_t *p = pParent;
	for ( int depth = 0; p && depth <= MW_MAX_DEPTH; depth++ )
	{
		if ( MW_IsStudio( p ) )
			return TRUE;
		for ( int i = 0; s_szTurning[i]; i++ )
			if ( FClassnameIs( &p->v, s_szTurning[i] ) )
				return TRUE;
		MoveWithNode *pn = MW_NodeOf( p );
		p = ( pn && !pn->orphan ) ? pn->hParent.Get() : NULL;
	}
	return FALSE;
}

// the parent's pose for a link: its editor pose at the map start, else where it is
static void MW_ParentBindPose( edict_t *pParent, BOOL bMapStart, Vector &origin, Vector &angles )
{
	MoveWithEntry *e = MW_Entry( pParent );

	if ( bMapStart && e && e->placed )
	{
		origin = e->placedOrigin;
		angles = e->placedAngles;
	}
	else
	{
		origin = pParent->v.origin;
		angles = pParent->v.angles;
	}
}

// local pose of the child (own pose in the parent's link frame)
static void MW_LocalPose( MoveWithNode &n, edict_t *pChild, const Vector &ownOrigin, const Vector &ownAngles, Vector &localOrigin, mwmatrix_t localRot )
{
	mwmatrix_t own;

	localOrigin = MW_IRotateV( n.bindRot, ownOrigin - n.bindOrigin );
	MW_EntityRot( pChild, ownAngles, own );
	MW_MatTMul( n.bindRot, own, localRot );
}

static BOOL MW_LinkNode( edict_t *pChild, edict_t *pParent, int iAttachment, BOOL bMapStart, const Vector *pLocalOrigin, const mwmatrix_t *pLocalRot )
{
	MoveWithEntry *e = MW_Entry( pChild );
	if ( !e )
		return FALSE;

	if ( pChild == pParent )
	{
		ALERT( at_console, "movewith: %s (%s) can't move with itself\n", MW_Name( pChild ), STRING( pChild->v.classname ) );
		return FALSE;
	}

	int mt = pChild->v.movetype;
	if ( ( pChild->v.flags & ( FL_CLIENT | FL_MONSTER | FL_FAKECLIENT ) ) || mt == MOVETYPE_WALK || mt == MOVETYPE_STEP
		|| mt == MOVETYPE_FLY || mt == MOVETYPE_TOSS || mt == MOVETYPE_BOUNCE || mt == MOVETYPE_FLYMISSILE
		|| mt == MOVETYPE_PUSHSTEP )
	{
		// monsters, players, func_pushable, things that fall or fly
		ALERT( at_console, "movewith: %s (%s) can't move with %s, because it moves on its own, but it can ride along by standing on a moving object\n",
			MW_Name( pChild ), STRING( pChild->v.classname ), MW_Name( pParent ) );
		return FALSE;
	}
	if ( MW_ClientCopyClass( pChild ) )
	{
		ALERT( at_console, "movewith: %s (%s) can't move with %s, because each player's game draws it from a copy it gets once\n",
			MW_Name( pChild ), STRING( pChild->v.classname ), MW_Name( pParent ) );
		return FALSE;
	}
	if ( MW_RefusedChildClass( pChild ) )
	{
		ALERT( at_console, "movewith: %s (%s) can't move with %s, because it moves along its own path or axis, but it can be a parent\n",
			MW_Name( pChild ), STRING( pChild->v.classname ), MW_Name( pParent ) );
		return FALSE;
	}

	// the parent chain must not come back to the child, nor be too deep
	int depth = 1;
	for ( MoveWithNode *p = MW_NodeOf( pParent ); p && !p->orphan; p = MW_NodeOf( p->hParent.Get() ) )
	{
		if ( p->hChild.Get() == pChild || p->hParent.Get() == pChild )
		{
			ALERT( at_console, "movewith: %s (%s) isn't linked to %s, because that would make a loop of parents\n", MW_Name( pChild ),
				STRING( pChild->v.classname ), MW_Name( pParent ) );
			return FALSE;
		}
		if ( ++depth > MW_MAX_DEPTH )
		{
			ALERT( at_console, "movewith: %s (%s) isn't linked, because its chain of parents would be deeper than %d\n", MW_Name( pChild ),
				STRING( pChild->v.classname ), MW_MAX_DEPTH );
			return FALSE;
		}
	}

	MoveWithNode *pOld = MW_NodeOf( pChild );
	int i = pOld ? e->node : MW_AllocNode();
	MoveWithNode &n = s_Nodes[i];
	string_t iszParent = pOld ? pOld->iszParent : 0;

	memset( &n, 0, sizeof( n ) );
	n.inUse = TRUE;
	n.hChild.Set( pChild );
	n.hParent.Set( pParent );

	// a parent that is (or becomes) the top of a chain starts with no hold state of an earlier entity at its index (R8)
	int iParentIndex = ENTINDEX( pParent );
	if ( iParentIndex > 0 && iParentIndex < MAX_EDICTS && !MW_NodeOf( pParent ) && s_Roots[ iParentIndex ].frame < s_iFrame - 1 )
		memset( &s_Roots[ iParentIndex ], 0, sizeof( s_Roots[ iParentIndex ] ) );
	n.childIndex = ENTINDEX( pChild );
	n.iszParent = iszParent ? iszParent : pParent->v.targetname;
	n.flags = e->parentflags;
	e->node = i;

	if ( iAttachment && !MW_HasAttachment( pParent, iAttachment ) )
	{
		ALERT( at_console, "movewith: %s follows the origin of %s, because that model has no attachment %d\n",
			MW_Name( pChild ), MW_Name( pParent ), iAttachment );
		iAttachment = 0;
	}

	// the kind of child
	BOOL bBrush = MW_IsBrush( pChild );
	BOOL bHasModel = pChild->v.modelindex && pChild->v.model;
	int solid = pChild->v.solid;

	// a button that turns about its own axis keeps its own movement on the parent also when it isn't solid
	// (momentary_rot_button, func_rot_button "Not solid"): steered as a pusher, which moves it without pushing
	BOOL bOwnTurn = FClassnameIs( &pChild->v, "momentary_rot_button" ) || FClassnameIs( &pChild->v, "func_rot_button" );
	if ( bBrush )
	{
		if ( solid == SOLID_TRIGGER )
			n.kind = MW_KIND_TRIGGER;
		else if ( solid == SOLID_NOT && pChild->v.skin == 0 && !bOwnTurn )
			n.kind = MW_KIND_POSITION;
		else
			n.kind = MW_KIND_PUSH;
	}
	else if ( iAttachment && MW_IsSprite( pChild ) && solid == SOLID_NOT )
		n.kind = MW_KIND_FOLLOW;
	else if ( solid == SOLID_BBOX || solid == SOLID_SLIDEBOX || solid == SOLID_BSP
		|| ( bHasModel && ( pChild->v.effects & ( EF_BRIGHTLIGHT | EF_DIMLIGHT | EF_LIGHT ) ) ) )
		n.kind = MW_KIND_NOCLIP;
	else
		n.kind = MW_KIND_POSITION;

	// trigger_camera: rides with the parent by position only; its aim, turning and target tracking stay its own, and it
	// ignores its own path while it has a parent (triggers.cpp). It may sit on an attachment.
	BOOL bCamera = FClassnameIs( pChild, "trigger_camera" );
	if ( bCamera )
		n.kind = MW_KIND_TRIGGER;

	if ( iAttachment && n.kind != MW_KIND_POSITION && n.kind != MW_KIND_FOLLOW && !bCamera )
	{
		ALERT( at_console, "movewith: %s follows the origin of %s, because only a sprite or a model that isn't solid can sit on an attachment\n",
			MW_Name( pChild ), MW_Name( pParent ) );
		iAttachment = 0;
	}
	n.attachment = iAttachment;
	// sent with its local pose and placed by the client: visual children, and solid children too (their world
	// position stays on the server for collision; the client draws them and writes them into its prediction
	// physents from the parent): no network traffic while the parent moves
	n.compose = ( n.kind == MW_KIND_POSITION || n.kind == MW_KIND_PUSH || n.kind == MW_KIND_NOCLIP ) && bHasModel && !n.flags;

	if ( n.kind == MW_KIND_PUSH && pChild->v.movetype != MOVETYPE_PUSH )
	{
		e->converted = TRUE;
		pChild->v.movetype = MOVETYPE_PUSH;
		pChild->v.ltime = gpGlobals->time;
	}
	else if ( n.kind == MW_KIND_NOCLIP )
	{
		e->converted = TRUE;
		pChild->v.movetype = MOVETYPE_NOCLIP;
	}
	else if ( n.kind == MW_KIND_FOLLOW )
	{
		// as CSprite::SetAttachment: the engine draws the sprite on attachment "body" of entity "skin"
		pChild->v.skin = ENTINDEX( pParent );
		pChild->v.body = iAttachment;
		pChild->v.aiment = pParent;
		pChild->v.movetype = MOVETYPE_FOLLOW;
	}
	n.converted = e->converted;

	// a solid brush child without an origin brush (its origin was the editor's world origin, far from its brushes) on
	// a chain that can turn: the engine turns a pusher about its origin and pushes what it bumps into along that far
	// arc. Riders are carried by movewith (MW_CarryTurningRiders); anything else would be pushed the wrong way.
	if ( n.kind == MW_KIND_PUSH && bBrush && e->placed && e->placedOrigin == g_vecZero && MW_ChainCanTurn( pParent ) )
		ALERT( at_console, "movewith: %s has no origin brush, so anything it bumps into while it turns is pushed the wrong way. Give it an origin brush\n",
			MW_Name( pChild ) );

	// the link frame and the child's own pose in it
	Vector bindOrigin, bindAngles;
	MW_ParentBindPose( pParent, bMapStart, bindOrigin, bindAngles );
	n.bindOrigin = bindOrigin;
	MW_EntityRot( pParent, bindAngles, n.bindRot );

	if ( pLocalOrigin && pLocalRot )
	{
		// re-link keeping the old local pose: the own pose is where that local pose puts it now
		mwmatrix_t own;
		n.ownOrigin = n.bindOrigin + MW_RotateV( n.bindRot, *pLocalOrigin );
		MW_MatMul( n.bindRot, *pLocalRot, own );
		n.ownAngles = MW_EntityAngles( pChild, own, pChild->v.angles );
	}
	else
	{
		n.ownOrigin = pChild->v.origin;
		n.ownAngles = pChild->v.angles;
	}
	n.ownLtime = ( n.kind == MW_KIND_PUSH ) ? pChild->v.ltime : gpGlobals->time;	// MW_OwnClock
	n.lockOrigin = pChild->v.origin;
	n.lockAngles = pChild->v.angles;

	// a brush trigger is placed by its box's centre: its origin is often far from its brushes (no origin brush: the
	// editor's world origin), and the box keeps its axes while the parent turns
	if ( n.kind == MW_KIND_TRIGGER )
	{
		mwmatrix_t childRot;
		MW_EntityRot( pChild, pChild->v.angles, childRot );
		n.triggerCenter = MW_IRotateV( childRot, ( pChild->v.mins + pChild->v.maxs ) * 0.5f );
	}

	Vector localOrigin;
	mwmatrix_t localRot;
	MW_LocalPose( n, pChild, n.ownOrigin, n.ownAngles, localOrigin, localRot );
	n.attachLocalOrigin = localOrigin;
	n.stateOrigin = iAttachment ? g_vecZero : localOrigin;
	n.stateAngles = MW_MatrixAnglesV( localRot, g_vecZero );
	n.sentOwnOrigin = n.ownOrigin;
	n.sentOwnAngles = n.ownAngles;

	n.endOrigin = pChild->v.origin;
	n.endAngles = pChild->v.angles;
	MW_EntityRot( pChild, n.endAngles, n.endRot );
	n.nextBlock = 0;
	s_bDirty = TRUE;

	ALERT( at_aiconsole, "movewith: %s (%s, #%d) moves with %s (#%d)%s (%s)\n", MW_Name( pChild ), STRING( pChild->v.classname ),
		ENTINDEX( pChild ), MW_Name( pParent ), ENTINDEX( pParent ), iAttachment ? " (attachment)" : "", s_szKindName[ n.kind ] );
	return TRUE;
}

// "name" or "name.N" (N = 1 .. 4: attachment N of a studio model, XashXT's form), resolved as every targeting
// entity's names are (UTIL_FindEntityByTargetname: "*alias", "group.member", "name*" (the first match), and
// !activator / !caller where an activator is known). The exact name wins, then "name.N".
static edict_t *MW_FindParent( string_t iszName, int &iAttachment, CBaseEntity *pActivator = NULL, CBaseEntity *pCaller = NULL )
{
	const char *szName = STRING( iszName );
	CBaseEntity *pFound = UTIL_FindEntityByTargetname( NULL, szName, pActivator, pCaller );

	iAttachment = 0;
	if ( pFound && !FNullEnt( pFound->edict() ) )
		return pFound->edict();

	size_t len = strlen( szName );
	if ( len > 2 && szName[len - 2] == '.' && szName[len - 1] >= '1' && szName[len - 1] <= '4' )
	{
		char szBase[ 128 ];
		if ( len - 2 >= sizeof( szBase ) )
			return NULL;
		memcpy( szBase, szName, len - 2 );
		szBase[ len - 2 ] = 0;
		pFound = UTIL_FindEntityByTargetname( NULL, szBase, pActivator, pCaller );
		if ( pFound && !FNullEnt( pFound->edict() ) )
		{
			iAttachment = szName[len - 1] - '0';
			return pFound->edict();
		}
	}
	return NULL;
}

static void MW_Unlink( edict_t *pChild, BOOL bStop )
{
	MoveWithEntry *e = MW_Entry( pChild );
	MoveWithNode *n = MW_NodeOf( pChild );

	if ( !n )
		return;

	if ( bStop && ( n->kind == MW_KIND_PUSH || n->kind == MW_KIND_NOCLIP ) )
		MW_StopEntity( pChild );
	if ( n->kind == MW_KIND_FOLLOW )
	{
		pChild->v.aiment = NULL;
		pChild->v.movetype = MOVETYPE_NONE;
		pChild->v.skin = 0;
		pChild->v.body = 0;
	}
	MW_FreeNode( e->node );
}

//=========================================================
// map start
//=========================================================

void MoveWith_ServerActivate( void )
{
	s_Nodes.clear();
	s_Order.clear();
	s_Sounds.clear();
	s_Seen.clear();
	memset( s_Roots, 0, sizeof( s_Roots ) );
	memset( s_iCarryFrame, 0, sizeof( s_iCarryFrame ) );
	s_iFrame = 1;
	s_bDirty = TRUE;
	s_bActive = TRUE;
	s_bFrameTimeChecked = FALSE;
	s_flNextReport = 0;
	s_flLastFrameTime = 0;
	s_flPrevFrameStart = 0;

	int iLinked = 0;
	for ( int i = 1; i < gpGlobals->maxEntities && i < MAX_EDICTS; i++ )
	{
		edict_t *pent = INDEXENT( i );

		if ( MW_Gone( pent ) )
			continue;

		MoveWithEntry *e = MW_Entry( pent );
		e->node = -1;
		string_t iszName = e->iszMoveWith ? e->iszMoveWith : e->iszParent;	// "movewith" wins
		if ( !iszName )
			continue;

		int iAttachment;
		edict_t *pParent = MW_FindParent( iszName, iAttachment );
		if ( !pParent )
		{
			ALERT( at_console, "movewith: %s (%s) found no entity named \"%s\" to move with\n", MW_Name( pent ), STRING( pent->v.classname ), STRING( iszName ) );
			continue;
		}
		if ( MW_LinkNode( pent, pParent, iAttachment, TRUE, NULL, NULL ) )
		{
			s_Nodes[ e->node ].iszParent = iszName;
			iLinked++;
		}
	}

	if ( iLinked )
		ALERT( at_console, "movewith: %d entities move with a parent\n", iLinked );
}

// where a node's target is with its parent at origin / rotation (attachment: the parent's attachment point)
static void MW_Target( MoveWithNode &n, edict_t *pChild, const Vector &ownOrigin, const Vector &ownAngles,
	const Vector &parentOrigin, const mwmatrix_t parentRot, Vector &origin, mwmatrix_t rot )
{
	Vector localOrigin;
	mwmatrix_t localRot;

	MW_LocalPose( n, pChild, ownOrigin, ownAngles, localOrigin, localRot );
	origin = parentOrigin + MW_RotateV( parentRot, localOrigin );
	MW_MatMul( parentRot, localRot, rot );
}

// parentflags: a frozen axis keeps the child's world value from the link
static void MW_ApplyLocks( MoveWithNode &n, edict_t *pChild, Vector &origin, mwmatrix_t rot, Vector &angles )
{
	if ( !n.flags )
		return;

	if ( n.flags & PARENT_FROZEN_POS_X ) origin.x = n.lockOrigin.x;
	if ( n.flags & PARENT_FROZEN_POS_Y ) origin.y = n.lockOrigin.y;
	if ( n.flags & PARENT_FROZEN_POS_Z ) origin.z = n.lockOrigin.z;

	if ( n.flags & ( PARENT_FROZEN_ROT_X | PARENT_FROZEN_ROT_Y | PARENT_FROZEN_ROT_Z ) )
	{
		if ( n.flags & PARENT_FROZEN_ROT_X ) angles.x = n.lockAngles.x;
		if ( n.flags & PARENT_FROZEN_ROT_Y ) angles.y = n.lockAngles.y;
		if ( n.flags & PARENT_FROZEN_ROT_Z ) angles.z = n.lockAngles.z;
		MW_EntityRot( pChild, angles, rot );
	}
}

// the parent's attachment (studio models only), or FALSE
static BOOL MW_Attachment( MoveWithNode &n, edict_t *pParent, Vector &origin )
{
	if ( !n.attachment )
		return FALSE;

	// checked every time: the parent's model can change while the map runs
	if ( !MW_HasAttachment( pParent, n.attachment ) )
	{
		if ( !n.attachMissing )
			ALERT( at_console, "movewith: %s follows the origin of %s until its model has attachment %d again\n",
				MW_Name( n.hChild.Get() ), MW_Name( pParent ), n.attachment );
		n.attachMissing = TRUE;
		return FALSE;
	}
	n.attachMissing = FALSE;

	Vector angles;
	GET_ATTACHMENT( pParent, n.attachment - 1, origin, angles );
	return TRUE;
}

static void MW_PlaceNow( MoveWithNode &n, edict_t *pChild, edict_t *pParent )
{
	Vector parentOrigin = pParent->v.origin, origin;
	mwmatrix_t parentRot, rot;

	MW_EntityRot( pParent, pParent->v.angles, parentRot );
	MW_Target( n, pChild, n.ownOrigin, n.ownAngles, parentOrigin, parentRot, origin, rot );
	MW_Attachment( n, pParent, origin );
	Vector angles = MW_EntityAngles( pChild, rot, pChild->v.angles );
	MW_ApplyLocks( n, pChild, origin, rot, angles );

	if ( n.kind == MW_KIND_FOLLOW )
		return;

	if ( n.kind != MW_KIND_TRIGGER )
		pChild->v.angles = angles;
	UTIL_SetOrigin( &pChild->v, origin );
	if ( n.kind == MW_KIND_PUSH || n.kind == MW_KIND_NOCLIP )
		MW_StopEntity( pChild );
	n.endOrigin = pChild->v.origin;
	n.endAngles = pChild->v.angles;
	MW_EntityRot( pChild, n.endAngles, n.endRot );
	n.predValid = FALSE;
}

void MoveWith_PostActivate( void )
{
	if ( s_bDirty )
		MW_SortNodes();

	// Activate moved some parents (a func_train to its first path_corner, a func_tracktrain onto its path):
	// put every child at its place, parents first. Nobody is in the map yet.
	for ( size_t k = 0; k < s_Order.size(); k++ )
	{
		MoveWithNode &n = s_Nodes[ s_Order[k] ];
		edict_t *pChild = n.hChild.Get(), *pParent = n.hParent.Get();

		if ( !n.inUse || MW_Gone( pChild ) || MW_Gone( pParent ) )
			continue;
		MW_PlaceNow( n, pChild, pParent );
	}
}

void MoveWith_ServerDeactivate( void )
{
	s_Nodes.clear();
	s_Order.clear();
	s_Sounds.clear();
	s_Seen.clear();
	s_bActive = FALSE;
	memset( s_Entries, 0, sizeof( s_Entries ) );
	for ( int i = 0; i < MAX_EDICTS; i++ )
		s_Entries[i].node = -1;
}

//=========================================================
// every frame
//=========================================================

// the parent's pose at the end of this frame, and now
static void MW_ParentPoses( MoveWithNode &n, edict_t *pParent, float frametime, Vector &endOrigin, mwmatrix_t endRot,
	Vector &nowOrigin, mwmatrix_t nowRot, BOOL &bJumped )
{
	entvars_t *pevParent = &pParent->v;
	MoveWithNode *pn = MW_NodeOf( pParent );

	nowOrigin = pevParent->origin;
	MW_EntityRot( pParent, pevParent->angles, nowRot );
	bJumped = FALSE;

	if ( pn && !pn->orphan )
	{
		// a child itself: where it was sent this frame (it was steered before us)
		endOrigin = pn->endOrigin;
		MW_MatCopy( pn->endRot, endRot );
		bJumped = pn->jumped;
		return;
	}

	// a root: its start of the frame (to put it back if its chain gets blocked), then as the engine will move it
	int iRoot = ENTINDEX( pParent );
	BOOL bReverted = FALSE;
	if ( iRoot > 0 && iRoot < MAX_EDICTS )
	{
		MoveWithRoot &r = s_Roots[ iRoot ];
		r.root = TRUE;
		if ( r.frame != s_iFrame )
		{
			r.frame = s_iFrame;
			r.origin = pevParent->origin;
			r.angles = pevParent->angles;
			r.ltime = pevParent->ltime;
		}
		bReverted = ( r.revertedFrame == s_iFrame );
	}

	float movetime = 0;
	if ( pevParent->movetype == MOVETYPE_PUSH )
		movetime = MW_PusherMovetime( pevParent, frametime );
	else if ( pevParent->movetype == MOVETYPE_NOCLIP || pevParent->movetype == MOVETYPE_FLY || pevParent->movetype == MOVETYPE_FLYMISSILE )
		movetime = frametime;

	Vector endAngles = pevParent->angles + pevParent->avelocity * movetime;
	endOrigin = pevParent->origin + pevParent->velocity * movetime;
	MW_EntityRot( pParent, endAngles, endRot );

	// teleported since the last frame (not where the engine was to move it)? Without a prediction (the first frame
	// after a link: the engine may have run frames between the link and now, e.g. at map load) the child is placed.
	if ( n.predValid && n.predPusher && pevParent->movetype == MOVETYPE_PUSH )
	{
		float moved = pevParent->ltime - n.predLtime;
		if ( moved >= 0 && moved < 1.0f )
		{
			n.predOrigin = n.predBaseOrigin + n.predVelocity * moved;
			n.predAngles = n.predBaseAngles + n.predAvelocity * moved;
		}
	}
	if ( !n.predValid )
		bJumped = TRUE;
	else if ( !bReverted &&  ( ( pevParent->origin - n.predOrigin ).Length() > MW_JUMP_DISTANCE
		|| fabsf( MW_AngleDiff( pevParent->angles.x, n.predAngles.x ) ) > MW_JUMP_ANGLE
		|| fabsf( MW_AngleDiff( pevParent->angles.y, n.predAngles.y ) ) > MW_JUMP_ANGLE
		|| fabsf( MW_AngleDiff( pevParent->angles.z, n.predAngles.z ) ) > MW_JUMP_ANGLE ) )
		bJumped = TRUE;

	n.predValid = TRUE;
	n.predOrigin = endOrigin;
	n.predAngles = endAngles;
	n.predPusher = ( pevParent->movetype == MOVETYPE_PUSH );
	n.predBaseOrigin = pevParent->origin;
	n.predBaseAngles = pevParent->angles;
	n.predVelocity = pevParent->velocity;
	n.predAvelocity = pevParent->avelocity;
	n.predLtime = pevParent->ltime;
}

// the own movement of a child mover, up to the child's ltime now. Not while its chain is held: the engine still
// advances the child's ltime in a held frame (it is moved, by the steering, without its think coming), and an
// update to a client (AddToFullPack) between two frames would otherwise move its own pose on.
static float MW_OwnClock( const MoveWithNode &n, entvars_t *pev )
{
	// a pusher moves by its own ltime (the engine stops it while blocked); everything else by the server time
	return ( n.kind == MW_KIND_PUSH ) ? pev->ltime : gpGlobals->time;
}

static void MW_AdvanceOwn( MoveWithNode &n, entvars_t *pev )
{
	float now = MW_OwnClock( n, pev );

	if ( n.held || n.noOwnThink )
	{
		n.ownLtime = now;
		return;
	}

	float dt = now - n.ownLtime;

	if ( dt > 0 && ( n.ownVelocity != g_vecZero || n.ownAvelocity != g_vecZero ) )
	{
		n.ownOrigin = n.ownOrigin + n.ownVelocity * dt;
		n.ownAngles = n.ownAngles + n.ownAvelocity * dt;
	}
	n.ownLtime = now;
}

static void MW_ClampLength( Vector &v, float max )
{
	float len = v.Length();

	if ( len > max && len > 0 )
		v = v * ( max / len );
}

static void MW_UpdateNode( MoveWithNode &n, edict_t *pChild, edict_t *pParent, float frametime )
{
	entvars_t *pev = &pChild->v;
	Vector parentEnd, parentNow;
	mwmatrix_t parentEndRot, parentNowRot;
	BOOL bJumped;

	MW_ParentPoses( n, pParent, frametime, parentEnd, parentEndRot, parentNow, parentNowRot, bJumped );
	n.jumped = n.assigned;	// its children follow a new place at once
	n.assigned = FALSE;
	n.rootIndex = ENTINDEX( MW_ChainRoot( &n ) );

	if ( n.kind == MW_KIND_FOLLOW )
	{
		n.endOrigin = pev->origin;
		n.endAngles = pev->angles;
		MW_EntityRot( pChild, n.endAngles, n.endRot );
		return;
	}

	// a pusher moves the whole frame unless its own think comes first; give it a think time that never comes when
	// it has none (its think function is never called by it)
	float movetime = frametime;
	BOOL bCut = FALSE;
	if ( n.kind == MW_KIND_PUSH )
	{
		if ( n.converted && n.ownVelocity == g_vecZero && n.ownAvelocity == g_vecZero )
			pev->ltime = gpGlobals->time;	// thinks set from the server time (func_illusionary...) stay right
		if ( pev->nextthink <= pev->ltime )
			pev->nextthink = pev->ltime + MW_ETERNAL_THINK;
		// The engine moves a pusher only up to its think time, and not at all without one: a velocity left on it
		// when its think stopped (a func_tank let go of mid-turn) moves nothing. The think time it has here is ours
		// (above) when it has none of its own: its own movement stops as the engine would stop it.
		n.noOwnThink = ( pev->nextthink - pev->ltime ) >= MW_ETERNAL_THINK * 0.5f;
		movetime = MW_PusherMovetime( pev, frametime );
		if ( !n.converted && n.ownVelocity == g_vecZero && s_flLastFrameTime > 0 && n.ownLtime > 0
			&& fabsf( ( pev->ltime - n.ownLtime ) - s_flLastFrameTime ) > 0.0005f && pev->ltime - n.ownLtime > 0 )
			s_iFrameTimeOff++;	// the engine moved it by another time than StartFrame was given (developer 2 report)
		if ( movetime < frametime * 0.25f )
			movetime = frametime;	// its think comes at once: it catches up in the next frame
		// its chain is held and its root goes back this frame: its move is cut at the same time (exact)
		MoveWithRoot *pr = ( n.rootIndex > 0 && n.rootIndex < MAX_EDICTS ) ? &s_Roots[ n.rootIndex ] : NULL;
		if ( pr && pr->held && pr->cutting )
		{
			MW_SwapThink( pChild, n.thinkSwapped, n.savedThink );
			pev->nextthink = pev->ltime + MW_CUT_TIME;
			movetime = MW_CUT_TIME;
			bCut = TRUE;
		}
		else if ( n.thinkSwapped && !( pr && pr->held ) )
			MW_RestoreThink( pChild, n.thinkSwapped, n.savedThink );
		MW_AdvanceOwn( n, pev );
		n.startLtime = pev->ltime;
		n.startOwnOrigin = n.ownOrigin;
		n.startOwnAngles = n.ownAngles;
	}
	else
		MW_AdvanceOwn( n, pev );	// a velocity given to the child (trigger_motion, trigger_changevalue)

	// own pose now and at the end of this frame (a held chain: it stays where the blocked frame started)
	Vector ownEndOrigin = n.ownOrigin, ownEndAngles = n.ownAngles;
	if ( !n.held && !n.noOwnThink )
	{
		ownEndOrigin = ownEndOrigin + n.ownVelocity * movetime;
		ownEndAngles = ownEndAngles + n.ownAvelocity * movetime;
	}

	Vector targetEnd, targetNow;
	mwmatrix_t rotEnd, rotNow;
	MW_Target( n, pChild, ownEndOrigin, ownEndAngles, parentEnd, parentEndRot, targetEnd, rotEnd );
	MW_Target( n, pChild, n.ownOrigin, n.ownAngles, parentNow, parentNowRot, targetNow, rotNow );
	if ( n.attachment && MW_Attachment( n, pParent, targetNow ) )
		targetEnd = targetNow;

	Vector anglesEnd = MW_EntityAngles( pChild, rotEnd, pev->angles );
	Vector anglesNow = MW_EntityAngles( pChild, rotNow, pev->angles );
	MW_ApplyLocks( n, pChild, targetEnd, rotEnd, anglesEnd );
	MW_ApplyLocks( n, pChild, targetNow, rotNow, anglesNow );

	switch ( n.kind )
	{
	case MW_KIND_POSITION:
		pev->angles = anglesEnd;
		if ( ( targetEnd - pev->origin ).Length() > 0.01f )
			UTIL_SetOrigin( pev, targetEnd );
		n.endOrigin = targetEnd;
		n.endAngles = anglesEnd;
		MW_MatCopy( rotEnd, n.endRot );
		return;

	case MW_KIND_TRIGGER:
		if ( pev->movetype == MOVETYPE_NOCLIP )
			pev->velocity = g_vecZero;	// a trigger_camera: the engine would move it off its place
		// the box's centre where the parent's turn takes it (a turned parent took an origin far from the brushes
		// round its own arc, the box off its brushes: a player's Use missed a riding func_tankcontrols)
		targetEnd = targetEnd + MW_RotateV( rotEnd, n.triggerCenter ) - ( pev->mins + pev->maxs ) * 0.5f;
		if ( ( targetEnd - pev->origin ).Length() > 0.01f )
			UTIL_SetOrigin( pev, targetEnd );
		n.endOrigin = targetEnd;
		n.endAngles = pev->angles;
		MW_EntityRot( pChild, n.endAngles, n.endRot );
		return;
	}

	// pushed and noclip children: steered by velocity
	n.moveTime = 0;
	if ( bJumped || s_bSnapFrame || n.jumped )
	{
		// the parent teleported (a func_train teleport corner): so does the child
		pev->angles = anglesEnd;
		UTIL_SetOrigin( pev, targetEnd );
		MW_StopEntity( pChild );
		n.jumped = TRUE;
		n.endOrigin = targetEnd;
		n.endAngles = anglesEnd;
		MW_MatCopy( rotEnd, n.endRot );
		return;
	}

	// error the child already has (blocked earlier), caught up at a limited speed; the rest follows the parent
	Vector err = targetNow - pev->origin;
	float flErr = err.Length();
	float flErrA = MW_AnglesDistance( anglesNow, pev->angles );
	if ( flErr > s_flWorstError || flErrA > s_flWorstAngle )
	{
		s_flWorstError = fmaxf( s_flWorstError, flErr );
		s_flWorstAngle = fmaxf( s_flWorstAngle, flErrA );
		s_iszWorst = pev->targetname ? pev->targetname : pev->classname;
	}
	if ( !bCut )
		MW_ClampLength( err, MW_CATCHUP_SPEED * frametime );	// a cut move ends exactly at its target
	Vector velocity = ( targetEnd - targetNow + err ) / movetime;

	Vector avelocity;
	for ( int i = 0; i < 3; i++ )
	{
		float errA = MW_AngleDiff( anglesNow[i], pev->angles[i] );
		float maxA = bCut ? 360.0f : MW_CATCHUP_ANGLE * frametime;
		if ( errA > maxA ) errA = maxA;
		else if ( errA < -maxA ) errA = -maxA;
		avelocity[i] = ( MW_AngleDiff( anglesEnd[i], anglesNow[i] ) + errA ) / movetime;
	}

	// no motion below what the network can show: keeps a still child still (no deltas, no pusher checks)
	if ( velocity.Length() * movetime < 0.001f )
		velocity = g_vecZero;
	if ( fabsf( avelocity.x * movetime ) < 0.0001f && fabsf( avelocity.y * movetime ) < 0.0001f && fabsf( avelocity.z * movetime ) < 0.0001f )
		avelocity = g_vecZero;

	pev->velocity = velocity;
	pev->avelocity = avelocity;
	n.moveTime = movetime;

	n.endOrigin = pev->origin + velocity * movetime;
	n.endAngles = pev->angles + avelocity * movetime;
	MW_EntityRot( pChild, n.endAngles, n.endRot );
}

//=========================================================
// riders
//=========================================================

// A rider (anything standing on a solid part of a chain). The engine moves pushers one after another in entity order,
// and a pusher moves what stands on it and pushes what it runs into. A rider standing on one part and touching another
// that moves earlier in the frame would be pushed by that part first and then carried by its own ground: it would move
// twice (it slips along) or, pushed into a ground that has not moved yet, block the chain. The parts that move as one
// body this frame (no own movement between them, no attachment, no axis locks) are one group; for the frame's physics
// a rider of the group stands on its first-moving part, which carries it exactly as its own ground would, and the
// parts after it move into space the rider has already left. Its own ground is given back before it moves again
// (MoveWith_CmdStart for a player, the next StartFrame for anything else).
struct MoveWithRider
{
	EHANDLE hRider;
	EHANDLE hGround;	// what it stands on
	EHANDLE hCarrier;	// the part it was given for this frame's physics
	int savedMovetype;	// a carried rider that isn't a player: its movetype, while its physics frame has MOVETYPE_NONE
};

static std::vector<MoveWithRider> s_Riders;
static int s_iGroupFrame[ MAX_EDICTS ];	// s_iFrame the two below are for
static int s_iGroupTop[ MAX_EDICTS ];	// the top of the group an entity moves rigidly with
static int s_iGroupFirst[ MAX_EDICTS ];	// per group top: the group's lowest entity index (the engine moves it first)
static BOOL s_bGroupMulti[ MAX_EDICTS ];	// per group top: more than one part

#define MW_RIDER_GAP	0.09f		// units: a rider closer than this to a part behind it ...
#define MW_RIDER_NUDGE	0.125f		// ... is moved this far ahead

static void MW_GiveBackGround( MoveWithRider &r )
{
	CBaseEntity *pRider = r.hRider;
	if ( pRider && r.hCarrier.Get() && pRider->pev->groundentity == r.hCarrier.Get() )
		pRider->pev->groundentity = r.hGround.Get();
	// its movetype back, unless its own think set another one meanwhile (a monster that died)
	if ( pRider && r.savedMovetype && pRider->pev->movetype == MOVETYPE_NONE )
		pRider->pev->movetype = r.savedMovetype;
	r.savedMovetype = 0;
}

static void MW_GiveBackGrounds( void )
{
	for ( size_t i = 0; i < s_Riders.size(); i++ )
		MW_GiveBackGround( s_Riders[i] );
	s_Riders.clear();
}

void MoveWith_CmdStart( edict_t *pPlayer )
{
	if ( s_Riders.empty() )
		return;
	for ( size_t i = 0; i < s_Riders.size(); i++ )
	{
		if ( s_Riders[i].hRider.Get() == pPlayer )
		{
			MW_GiveBackGround( s_Riders[i] );
			s_Riders[i].hRider = NULL;
		}
	}
}

#ifdef SBO_TEST_COMMANDS
static BOOL s_bTestRidersOff;	// test_mw_riders 0: the engine's own rider handling, for comparison
#endif

// the parts of a group in entity order (the engine's order): the next one after index "after", 0 at the end
static int MW_GroupNext( int top, int after )
{
	int best = 0;
	if ( top > after && s_iGroupFrame[top] == s_iFrame && s_iGroupTop[top] == top )
		best = top;
	for ( size_t k = 0; k < s_Order.size(); k++ )
	{
		MoveWithNode &n = s_Nodes[ s_Order[k] ];
		if ( !n.inUse || n.orphan || n.kind != MW_KIND_PUSH )
			continue;
		edict_t *pc = n.hChild.Get();
		if ( MW_Gone( pc ) )
			continue;
		int c = ENTINDEX( pc );
		if ( c > after && ( !best || c < best ) && c < MAX_EDICTS && s_iGroupFrame[c] == s_iFrame && s_iGroupTop[c] == top )
			best = c;
	}
	return best;
}

// the first part of a group a rider would run into within dist along dir (0: none); its ground is not counted
static int MW_RiderTouches( edict_t *pRider, const Vector &dir, float dist, int top, int ground )
{
	TraceResult tr;
	TRACE_MONSTER_HULL( pRider, pRider->v.origin, pRider->v.origin + dir * dist, dont_ignore_monsters, pRider, &tr );
	if ( ( tr.flFraction >= 1.0f && !tr.fStartSolid ) || FNullEnt( tr.pHit ) )
		return 0;
	int hit = ENTINDEX( tr.pHit );
	if ( hit <= 0 || hit >= MAX_EDICTS || hit == ground || s_iGroupFrame[hit] != s_iFrame || s_iGroupTop[hit] != top )
		return 0;
	return hit;
}

static void MW_RiderGrounds( void )
{
#ifdef SBO_TEST_COMMANDS
	if ( s_bTestRidersOff )
		return;
#endif
	// the groups (parents first: a part that moves rigidly with its parent is in its parent's group)
	BOOL bAny = FALSE;
	for ( size_t k = 0; k < s_Order.size(); k++ )
	{
		MoveWithNode &n = s_Nodes[ s_Order[k] ];
		if ( !n.inUse || n.orphan || n.kind != MW_KIND_PUSH )
			continue;
		edict_t *pChild = n.hChild.Get();
		edict_t *pParent = n.hParent.Get();
		if ( MW_Gone( pChild ) || MW_Gone( pParent ) )
			continue;
		int c = ENTINDEX( pChild ), p = ENTINDEX( pParent );
		if ( c <= 0 || c >= MAX_EDICTS || p <= 0 || p >= MAX_EDICTS )
			continue;
		if ( s_iGroupFrame[p] != s_iFrame )
		{
			// a top: a root pusher (or a parent that is not linked by a solid link)
			s_iGroupFrame[p] = s_iFrame;
			s_iGroupTop[p] = p;
			s_iGroupFirst[p] = p;
			s_bGroupMulti[p] = FALSE;
		}
		BOOL bRigid = pParent->v.movetype == MOVETYPE_PUSH && !n.jumped && !n.attachment && !n.flags
			&& ( n.held || ( n.ownVelocity == g_vecZero && n.ownAvelocity == g_vecZero ) );
		int top = bRigid ? s_iGroupTop[p] : c;
		s_iGroupFrame[c] = s_iFrame;
		s_iGroupTop[c] = top;
		if ( top == c )
		{
			s_iGroupFirst[c] = c;
			s_bGroupMulti[c] = FALSE;
		}
		else
		{
			if ( c < s_iGroupFirst[top] )
				s_iGroupFirst[top] = c;
			s_bGroupMulti[top] = TRUE;
			bAny = TRUE;
		}
	}
	if ( !bAny )
		return;

	// its riders: carried this frame by the part that keeps them clear of the others
	for ( int i = 1; i < gpGlobals->maxEntities && i < MAX_EDICTS; i++ )
	{
		edict_t *pent = INDEXENT( i );
		if ( !pent || pent->free || !( pent->v.flags & FL_ONGROUND ) || FNullEnt( pent->v.groundentity ) )
			continue;
		if ( pent->v.movetype == MOVETYPE_PUSH || pent->v.movetype == MOVETYPE_NONE || pent->v.movetype == MOVETYPE_NOCLIP
			|| pent->v.movetype == MOVETYPE_FOLLOW )
			continue;
		int g = ENTINDEX( pent->v.groundentity );
		if ( g <= 0 || g >= MAX_EDICTS || s_iGroupFrame[g] != s_iFrame )
			continue;
		int top = s_iGroupTop[g];
		int first = s_iGroupFirst[top];
		if ( !s_bGroupMulti[top] || first <= 0 || first >= MAX_EDICTS )
			continue;

		// the parts it touches ahead of it and behind it, in its ground's direction of motion, within this frame's move
		Vector v = pent->v.groundentity->v.velocity;
		float flMove = v.Length() * s_flFrameTime;
		if ( flMove < 0.001f || pent->v.groundentity->v.avelocity != g_vecZero )
			continue;	// still, or turning (its riders are carried round the turning part as usual)
		Vector dir = v * ( 1.0f / v.Length() );
		int ahead = MW_RiderTouches( pent, dir, flMove + MW_RIDER_NUDGE, top, g );
		int behind = MW_RiderTouches( pent, -dir, flMove + MW_RIDER_NUDGE, top, g );
#ifdef SBO_TEST_COMMANDS
		if ( CVAR_GET_FLOAT( "developer" ) >= 3 )
			ALERT( at_console, "MWG %.4f rider %d ground %d first %d ahead %d behind %d move %.3f\n", gpGlobals->time, i, g, first, ahead, behind, flMove );
#endif
		if ( !ahead && !behind )
			continue;

		// close against a part behind it: the parts' moves can differ by a hair (a parent that stops mid-frame,
		// the engine's frame time), and a part that ends up a hair inside it pushes it by a whole frame's move. It is
		// kept MW_RIDER_NUDGE clear (moved once, by at most that much).
		if ( behind )
		{
			TraceResult tr;
			TRACE_MONSTER_HULL( pent, pent->v.origin - dir * MW_RIDER_GAP, pent->v.origin - dir * MW_RIDER_GAP, dont_ignore_monsters, pent, &tr );
			if ( tr.fStartSolid && !FNullEnt( tr.pHit ) && ENTINDEX( tr.pHit ) == behind )
			{
				Vector to = pent->v.origin + dir * MW_RIDER_NUDGE;
				TRACE_MONSTER_HULL( pent, pent->v.origin, to, dont_ignore_monsters, pent, &tr );
				if ( !tr.fStartSolid && !tr.fAllSolid && tr.flFraction >= 1.0f )
					UTIL_SetOrigin( &pent->v, to );
			}
		}

		// the part that carries it this frame: one that moves as its ground does, after every part ahead of it
		// (they make room first) and before every part behind it (they move into the room it left). Its own ground
		// when that already fits; else the engine's own handling.
		if ( ( !ahead || ahead < g ) && ( !behind || g < behind ) )
			continue;
		int carrier = 0;
		for ( int c = MW_GroupNext( top, 0 ); c; c = MW_GroupNext( top, c ) )
		{
			edict_t *pc = INDEXENT( c );
			if ( ( !ahead || c >= ahead ) && ( !behind || c <= behind ) && pc->v.avelocity == g_vecZero
				&& ( pc->v.velocity - v ).Length() * s_flFrameTime < 0.002f )
			{
				carrier = c;
				break;
			}
		}
		if ( !carrier || carrier == g )
			continue;
		MoveWithRider r;
		r.hRider.Set( pent );
		r.hGround.Set( pent->v.groundentity );
		r.hCarrier.Set( INDEXENT( carrier ) );
		r.savedMovetype = 0;
		pent->v.groundentity = INDEXENT( carrier );
		s_Riders.push_back( r );
	}
}

// Riders (players, monsters, func_pushable, items) of a child that turns and moves in the same frame. The engine
// moves such a pusher in two steps (SV_Physics_Pusher): it turns it about its origin (SV_PushRotate), moving what
// stands on it along a traced arc about that origin, and then moves it (SV_PushMove). On a child without an origin
// brush the origin is far from the brushes, so the turn alone moves a rider far along that arc, into walls or off
// the child, and the move does not bring it back. And the turn takes FL_ONGROUND off every rider that isn't a
// player, so the move no longer carries those at all: with an origin brush they slide off at the child's speed. Such
// a rider is carried here instead, before the frame's physics: put where the child's motion this frame takes it (the
// same rigid motion; the turn's yaw as the engine gives it), and for this frame it stands on the world, so the engine
// leaves it alone. Its ground is given back at the next StartFrame (MW_GiveBackGrounds), or for a player before its
// next move (MoveWith_CmdStart). A rider whose way there is not clear is left to the engine.
// How far up a carried rider must be put, at its end-of-frame spot "to", to be clear of the child in its half-done
// pose (turned to a1, not yet moved): the child is put there for the test and back. Vector( 0, 0, -1 ): no clear
// spot found (at most the child's whole move this frame, and a quarter unit).
static Vector MW_CarryLift( edict_t *pent, edict_t *pGround, const Vector &to, const Vector &a1, const Vector &o1 )
{
	Vector savedAngles = pGround->v.angles;
	pGround->v.angles = a1;
	UTIL_SetOrigin( &pGround->v, pGround->v.origin );

	float down = pGround->v.origin.z - o1.z;	// how far the child goes down this frame (its half-done pose is that high)
	float flMove = ( o1 - pGround->v.origin ).Length();
	float steps[3] = { 0, ( down > 0 ? down : 0 ) + 0.03125f, flMove + 0.25f };
	Vector lift( 0, 0, -1 );
	for ( int k = 0; k < 3; k++ )
	{
		Vector at = to + Vector( 0, 0, steps[k] );
		TraceResult tr;
		TRACE_MONSTER_HULL( pent, at, at, dont_ignore_monsters, pent, &tr );
		if ( !( tr.fStartSolid && tr.pHit == pGround ) )
		{
			lift = Vector( 0, 0, steps[k] );
			break;
		}
	}

	pGround->v.angles = savedAngles;
	UTIL_SetOrigin( &pGround->v, pGround->v.origin );
	return lift;
}

// The ground pGround turns and moves this frame, by m seconds of its motion: a pushed child, or (for a rider that
// isn't a player) the root of a chain. The engine turns a pusher's riders in a step of its own before it moves it
// (SV_PushRotate), and that step takes FL_ONGROUND off a rider that doesn't walk: the move after it no longer carries
// it, so a monster on a car going round a curve slides to the car's back wall. Only on a map with movewith links
// (this runs only then) and only for the top of a chain.
static BOOL MW_TurningGround( edict_t *pGround, BOOL bPlayer, float &m )
{
	if ( MW_Gone( pGround ) || pGround->v.avelocity == g_vecZero || pGround->v.velocity == g_vecZero )
		return FALSE;
	MoveWithNode *n = MW_NodeOf( pGround );
	if ( n && !n->orphan )
	{
		m = n->moveTime;
		return n->kind == MW_KIND_PUSH && m > 0;
	}
	int g = ENTINDEX( pGround );
	if ( bPlayer || g <= 0 || g >= MAX_EDICTS || !s_Roots[g].root || pGround->v.movetype != MOVETYPE_PUSH )
		return FALSE;
	m = MW_PusherMovetime( &pGround->v, s_flFrameTime );
	return m > 0;
}

static void MW_CarryTurningRiders( void )
{
	BOOL bAny = FALSE;
	for ( size_t k = 0; k < s_Order.size() && !bAny; k++ )
	{
		MoveWithNode &n = s_Nodes[ s_Order[k] ];
		edict_t *pc = n.hChild.Get();
		float m;
		bAny = n.inUse && !n.orphan && ( ( n.kind == MW_KIND_PUSH && n.moveTime > 0 && !MW_Gone( pc )
			&& pc->v.avelocity != g_vecZero && pc->v.velocity != g_vecZero )
			|| ( n.rootIndex > 0 && n.rootIndex < MAX_EDICTS && MW_TurningGround( INDEXENT( n.rootIndex ), FALSE, m ) ) );
	}
	if ( !bAny )
		return;

	for ( int i = 1; i < gpGlobals->maxEntities && i < MAX_EDICTS; i++ )
	{
		// its ground: the engine's last push took FL_ONGROUND off it, and its own physics may only give it back
		// after the child has moved (entity order), so the ground entity and a short trace down decide
		edict_t *pent = INDEXENT( i );
		if ( !pent || pent->free || FNullEnt( pent->v.groundentity ) || pent->v.solid == SOLID_NOT || pent->v.solid == SOLID_TRIGGER )
			continue;
		int mt = pent->v.movetype;
		if ( mt == MOVETYPE_PUSH || mt == MOVETYPE_NONE || mt == MOVETYPE_NOCLIP || mt == MOVETYPE_FOLLOW )
			continue;
		edict_t *pGround = pent->v.groundentity;
		BOOL bPlayer = ( pent->v.flags & FL_CLIENT ) != 0;
		float m;
		if ( !MW_TurningGround( pGround, bPlayer, m ) )
			continue;
		TraceResult trDown;
		TRACE_MONSTER_HULL( pent, pent->v.origin, pent->v.origin - Vector( 0, 0, 2 ), dont_ignore_monsters, pent, &trDown );
		if ( trDown.pHit != pGround )
			continue;	// not standing on it (in the air, or on something else)

		// the ground's motion this frame: from its pose now to its pose at the end of the frame. A rider carried
		// last frame is measured from the pose it was carried with then (a rider that moved itself, a walking
		// player, keeps that move)
		int g = ENTINDEX( pGround );
		Vector a0 = pGround->v.angles, o0 = pGround->v.origin, from = pent->v.origin;
		if ( s_iCarryFrame[i] == s_iFrame - 1 && s_iCarryGround[i] == g )
		{
			a0 = s_vecCarryAngles[i];
			o0 = s_vecCarryOrigin[i];
			from = from - s_vecCarryLift[i];	// last frame's lift off again
		}
		Vector a1 = pGround->v.angles + pGround->v.avelocity * m, o1 = pGround->v.origin + pGround->v.velocity * m;
		mwmatrix_t r0, r1;
		MW_EntityRot( pGround, a0, r0 );
		MW_EntityRot( pGround, a1, r1 );
		Vector to = o1 + MW_RotateV( r1, MW_IRotateV( r0, from - o0 ) );

		// its way there clear of everything but the child it stands on (traced 1 unit up, and without the child: a
		// child that turns while it goes down is, now, where the rider will be at the end of the frame)
		TraceResult tr;
		Vector up( 0, 0, 1 );
		int groundSolid = pGround->v.solid;
		pGround->v.solid = SOLID_NOT;
		TRACE_MONSTER_HULL( pent, pent->v.origin + up, to + up, dont_ignore_monsters, pent, &tr );
		pGround->v.solid = groundSolid;
		if ( tr.fStartSolid || tr.fAllSolid || tr.flFraction < 1.0f )
			continue;

		// The engine moves the child in two steps: it turns it where it is, then moves it. A child that turns while
		// it goes down at the rider is, after the turn, partly where the rider now stands: the turn would find the
		// rider inside it and block, holding the chain every frame. So the rider is lifted just clear of that half-done
		// pose (the child is put there for the test and back), and stays a normal entity for everything else: other
		// movers push it and block on it as on any rider. Next frame the lift is taken off again (above).
		// A rider that isn't a player sits the frame's push out instead (MOVETYPE_NONE, which both push steps skip;
		// back at the next StartFrame): the lift alone still left some holds on a seesaw (t18: 10 of 15 degrees), as
		// such a rider's own physics can put it back on the child before the child moves. A player is never given a
		// stand-in movetype: plugins, triggers and other movers read it.
		Vector lift = bPlayer ? MW_CarryLift( pent, pGround, to, a1, o1 ) : g_vecZero;
		if ( lift.z < 0 )
			continue;	// no clear spot: left to the engine
		int savedMovetype = 0;
		if ( !bPlayer )
		{
			savedMovetype = pent->v.movetype;
			pent->v.movetype = MOVETYPE_NONE;
		}
		UTIL_SetOrigin( &pent->v, to + lift );
		s_vecCarryLift[i] = lift;
		s_iCarryFrame[i] = s_iFrame;
		s_iCarryGround[i] = g;
		s_vecCarryOrigin[i] = o1;
		s_vecCarryAngles[i] = a1;
		if ( pent->v.flags & FL_CLIENT )
		{
			// a player's view turns with it, as SV_PushRotate does it (an added angle, svc_addangle)
			pent->v.fixangle = 2;
			pent->v.avelocity.y += MW_AngleDiff( a1.y, a0.y );
		}
		else
			pent->v.angles.y += MW_AngleDiff( a1.y, a0.y );	// as SV_PushRotate turns a rider that isn't a player
		pent->v.flags |= FL_ONGROUND;	// standing (on the world, this frame): its own physics leaves it where it is
		MoveWithRider r;
		r.hRider.Set( pent );
		r.hGround.Set( pGround );
		r.hCarrier.Set( INDEXENT( 0 ) );
		r.savedMovetype = savedMovetype;
		pent->v.groundentity = INDEXENT( 0 );
		s_Riders.push_back( r );
	}
}

#ifdef SBO_TEST_COMMANDS
// Test builds only (SBO_TEST_COMMANDS): "test_mw_trace a,b,... | off" logs the server origins, ltime and ground entity
// of the listed entities (index, targetname or class name) once per frame ("MWS ..."; developer 2)
static int s_iSTrace[ 8 ];
static int s_iNumSTrace = 0;

static void MW_TraceCommand( void )
{
	s_iNumSTrace = 0;
	// the engine's tokenizer splits at commas: the whole argument string, items separated by commas or spaces
	const char *sz = CMD_ARGC() > 1 ? CMD_ARGS() : "off";
	if ( !strncmp( sz, "off", 3 ) )
	{
		ALERT( at_console, "test_mw_trace off\n" );
		return;
	}
	while ( *sz && s_iNumSTrace < 8 )
	{
		while ( *sz == ' ' || *sz == ',' || *sz == '"' ) sz++;
		if ( !*sz )
			break;
		if ( isdigit( *sz ) )
			s_iSTrace[ s_iNumSTrace++ ] = atoi( sz );
		else
		{
			// a name: the entity of that targetname, else the first of that class
			char name[ 64 ];
			int k = 0;
			while ( sz[k] && sz[k] != ',' && sz[k] != ' ' && sz[k] != '"' && k < 63 ) { name[k] = sz[k]; k++; }
			name[k] = 0;
			edict_t *pFound = FIND_ENTITY_BY_STRING( NULL, "targetname", name );
			if ( FNullEnt( pFound ) )
				pFound = FIND_ENTITY_BY_STRING( NULL, "classname", name );
			s_iSTrace[ s_iNumSTrace++ ] = FNullEnt( pFound ) ? 0 : ENTINDEX( pFound );
		}
		while ( *sz && *sz != ',' && *sz != ' ' ) sz++;
	}
	ALERT( at_console, "test_mw_trace: %d entities\n", s_iNumSTrace );
}

static void MW_ServerTrace( void )
{
	if ( CVAR_GET_FLOAT( "developer" ) < 2 )
		return;
	if ( s_iNumSTrace < 2 )
		return;
	char line[ 768 ];
	int n = snprintf( line, sizeof( line ), "MWS %.4f %.4f", gpGlobals->time, gpGlobals->frametime );
	for ( int i = 0; i < s_iNumSTrace && n < (int)sizeof( line ) - 96; i++ )
	{
		if ( s_iSTrace[i] <= 0 || s_iSTrace[i] >= gpGlobals->maxEntities )
			continue;
		edict_t *e = INDEXENT( s_iSTrace[i] );
		if ( !e || e->free )
			continue;
		n += snprintf( line + n, sizeof( line ) - n, " %d %.2f %.2f %.2f %.2f %.3f %d", s_iSTrace[i], e->v.origin[0], e->v.origin[1],
			e->v.origin[2], ( e->v.flags & FL_ONGROUND ? 1.0f : 0.0f ) + e->v.movetype * 10.0f + e->v.solid * 100.0f, e->v.ltime, e->v.groundentity ? ENTINDEX( e->v.groundentity ) : -1 );
	}
	ALERT( at_console, "%s\n", line );
	if ( s_iSTrace[0] > 0 && s_iSTrace[0] < gpGlobals->maxEntities && !INDEXENT( s_iSTrace[0] )->free )
	{
		entvars_t *p0 = &INDEXENT( s_iSTrace[0] )->v;
		ALERT( at_console, "MWR %.4f %d nextthink %.4f ltime %.4f velocity %.2f %.2f %.2f\n", gpGlobals->time, s_iSTrace[0],
			p0->nextthink, p0->ltime, p0->velocity.x, p0->velocity.y, p0->velocity.z );
	}
}

// a traced player's usercmd (CmdStart, CmdEnd): where its own move took it ("MWC <time> start|end <index> origin
// velocity ground-flag ground msec"; developer 2)
void MoveWith_TraceCmd( edict_t *pPlayer, int bEnd, int msec )
{
	if ( !pPlayer || CVAR_GET_FLOAT( "developer" ) < 2 )
		return;
	int index = ENTINDEX( pPlayer );
	BOOL bTraced = FALSE;
	for ( int i = 0; i < s_iNumSTrace; i++ )
		bTraced |= ( s_iSTrace[i] == index );
	if ( !bTraced )
		return;
	ALERT( at_console, "MWC %.4f %s %d %.2f %.2f %.2f vel %.2f %.2f %.2f g %d %d msec %d\n", gpGlobals->time, bEnd ? "end" : "start",
		index, pPlayer->v.origin.x, pPlayer->v.origin.y, pPlayer->v.origin.z, pPlayer->v.velocity.x, pPlayer->v.velocity.y,
		pPlayer->v.velocity.z, ( pPlayer->v.flags & FL_ONGROUND ) ? 1 : 0, pPlayer->v.groundentity ? ENTINDEX( pPlayer->v.groundentity ) : -1,
		msec );
}

// a touch with a traced player on either side (the engine's push of him, SV_PushEntity, touches what its trace hits):
// "MWX <time> <touched> <other> <player origin> <player flags> <player ground>"; developer 2
void MoveWith_TraceTouch( edict_t *pTouched, edict_t *pOther )
{
	if ( !pTouched || !pOther || s_iNumSTrace < 1 || CVAR_GET_FLOAT( "developer" ) < 2 )
		return;
	int a = ENTINDEX( pTouched ), b = ENTINDEX( pOther );
	edict_t *pPlayer = NULL;
	for ( int i = 0; i < s_iNumSTrace; i++ )
	{
		if ( s_iSTrace[i] == a && ( pTouched->v.flags & FL_CLIENT ) )
			pPlayer = pTouched;
		else if ( s_iSTrace[i] == b && ( pOther->v.flags & FL_CLIENT ) )
			pPlayer = pOther;
	}
	if ( !pPlayer )
		return;
	ALERT( at_console, "MWX %.4f %d %s %d %s pl %.2f %.2f %.2f flags %d g %d\n", gpGlobals->time, a, STRING( pTouched->v.classname ), b,
		STRING( pOther->v.classname ), pPlayer->v.origin.x, pPlayer->v.origin.y, pPlayer->v.origin.z, pPlayer->v.flags,
		pPlayer->v.groundentity ? ENTINDEX( pPlayer->v.groundentity ) : -1 );
}

// "test_mw_riders 0|1": the chain rider handling off (the engine's own order) or on
static void MW_RidersCommand( void )
{
	if ( CMD_ARGC() > 1 )
		s_bTestRidersOff = !atoi( CMD_ARGV( 1 ) );
	ALERT( at_console, "test_mw_riders %d\n", s_bTestRidersOff ? 0 : 1 );
}

// "test_mw_cost on|off|print": the CPU time movewith's own code takes in StartFrame and in AddToFullPack, per server
// frame ("test_mw_cost: <frames> frames, StartFrame <ms> ms/frame, AddToFullPack <ms> ms/frame (<calls> calls)")
static BOOL s_bCost = FALSE;
static double s_flCostStart = 0, s_flCostPack = 0;
static int s_iCostFrames = 0, s_iCostPackCalls = 0;

static double MW_Now( void )
{
#ifdef _WIN32
	return (double)clock() / CLOCKS_PER_SEC;
#else
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return ts.tv_sec + ts.tv_nsec * 1e-9;
#endif
}

static void MW_CostCommand( void )
{
	const char *a = CMD_ARGC() > 1 ? CMD_ARGV( 1 ) : "print";
	if ( !strcmp( a, "on" ) || !strcmp( a, "off" ) )
	{
		s_bCost = !strcmp( a, "on" );
		s_flCostStart = s_flCostPack = 0;
		s_iCostFrames = s_iCostPackCalls = 0;
	}
	int n = s_iCostFrames > 0 ? s_iCostFrames : 1;
	char line[ 160 ];
	snprintf( line, sizeof( line ), "test_mw_cost: %d frames, StartFrame %.4f ms/frame, AddToFullPack %.4f ms/frame (%d calls)\n",
		s_iCostFrames, s_flCostStart * 1000.0 / n, s_flCostPack * 1000.0 / n, s_iCostPackCalls );
	g_engfuncs.pfnServerPrint( line );	// also with developer 0
}

static void MW_TestCommands( void )
{
	static BOOL s_bRegistered = FALSE;
	if ( s_bRegistered )
		return;
	s_bRegistered = TRUE;
	g_engfuncs.pfnAddServerCommand( "test_mw_trace", MW_TraceCommand );
	g_engfuncs.pfnAddServerCommand( "test_mw_riders", MW_RidersCommand );
	g_engfuncs.pfnAddServerCommand( "test_mw_cost", MW_CostCommand );
}
#endif // SBO_TEST_COMMANDS

static void MW_FreeBlockers( void );

static void MW_StartFrame( void );

void MoveWith_StartFrame( void )
{
#ifdef SBO_TEST_COMMANDS
	MW_TestCommands();
	MW_ServerTrace();
	double t0 = s_bCost ? MW_Now() : 0;
#endif
	MW_StartFrame();
#ifdef SBO_TEST_COMMANDS
	if ( s_bCost )
	{
		s_flCostStart += MW_Now() - t0;
		s_iCostFrames++;
	}
#endif
}

static void MW_StartFrame( void )
{
	// a map where nothing moves with a parent: nothing to do and nothing touched (s_Nodes only grows while a map runs)
	if ( !s_bActive || s_Nodes.empty() )
		return;

	MW_GiveBackGrounds();
	MW_FreeBlockers();

	// the engine's frame time for this frame (it moves every pusher by it); never clamped: a child must move as far
	// as its parent. A long frame (map load, a server hitch) puts every child at its place instead of steering it.
	float frametime = ( s_flPrevFrameStart > 0 && gpGlobals->time > s_flPrevFrameStart ) ? gpGlobals->time - s_flPrevFrameStart
		: gpGlobals->frametime;
	if ( frametime <= 0 )
		return;
	s_flFrameTime = frametime;
	if ( s_flPrevFrameStart > 0 && s_flLastFrameTime > 0 )
		s_flWorstFrameTimeDiff = fmaxf( s_flWorstFrameTimeDiff, fabsf( s_flLastFrameTime - ( gpGlobals->time - s_flPrevFrameStart ) ) );
	s_flPrevFrameStart = gpGlobals->time;
	s_bSnapFrame = ( frametime > MW_SNAP_FRAMETIME );

	if ( s_bDirty )
		MW_SortNodes();

	// Held chains (MoveWith_Blocked): the frame after a block, the root goes back by physics to where the blocked frame
	// started (the engine carries its riders back with it; nothing is teleported) and the chain then waits MW_HOLD_TIME
	// with its timers paused: the root's and every child mover's ltime and think time stay frozen and their own movement
	// stops. On release everything continues from the same moment. A root that thinks every frame (func_vehicle) keeps
	// its own rule (it slows down) and is not held.
	s_iFrame++;
	for ( size_t k = 0; k < s_Order.size(); k++ )
	{
		MoveWithNode &n = s_Nodes[ s_Order[k] ];
		if ( !n.inUse || n.orphan || n.rootIndex <= 0 || n.rootIndex >= MAX_EDICTS )
			continue;
		MoveWithRoot &r = s_Roots[ n.rootIndex ];
		edict_t *pRoot = INDEXENT( n.rootIndex );
		// a removed root: nothing of its hold may pass to a later entity at that index (R8); its chain's held child
		// movers are released below (not a pusher any more)
		if ( MW_Gone( pRoot ) && ( r.root || r.held ) )
			memset( &r, 0, sizeof( r ) );
		BOOL bRootPusher = !MW_Gone( pRoot ) && pRoot->v.movetype == MOVETYPE_PUSH && !( pRoot->v.flags & FL_ALWAYSTHINK );

		if ( r.revertedFrame != s_iFrame && bRootPusher )
		{
			entvars_t *pevRoot = &pRoot->v;
			if ( r.heldFrame == s_iFrame - 1 && r.frame == s_iFrame - 1 )
			{
				// blocked in the last frame: (start or extend a hold) and go back to that frame's start
				if ( !r.held )
				{
					r.held = TRUE;
					r.wrote = FALSE;
					r.savedVelocity = pevRoot->velocity;
					r.savedAvelocity = pevRoot->avelocity;
					r.savedNextthink = pevRoot->nextthink;
					r.frozenLtime = r.ltime;
					MW_SwapThink( pRoot, r.thinkSwapped, r.savedThink );
				}
				r.backOrigin = r.origin;
				r.backAngles = r.angles;
				r.holdUntil = gpGlobals->time + MW_HOLD_TIME;
				if ( CVAR_GET_FLOAT( "developer" ) >= 2 )
					ALERT( at_aiconsole, "movewith: chain of %s held (blocked)\n", MW_Name( pRoot ) );
			}

			// (a cut move back ends in a think the engine calls, which clears the think time: that one is ours)
			if ( r.held && r.wrote && ( ( pevRoot->nextthink != r.writtenNextthink && !( r.cutting && pevRoot->nextthink == 0 ) )
				|| ( pevRoot->velocity - r.writtenVelocity ).Length() > 0.001f
				|| ( pevRoot->avelocity - r.writtenAvelocity ).Length() > 0.001f ) )
			{
				// its own code moved, stopped or turned it during the hold (C1, R9): the hold ends here, keeping that
				r.held = FALSE;
				r.wrote = FALSE;
				r.cutting = FALSE;
				MW_RestoreThink( pRoot, r.thinkSwapped, r.savedThink );
				r.revertedFrame = s_iFrame;
				if ( CVAR_GET_FLOAT( "developer" ) >= 2 )
					ALERT( at_aiconsole, "movewith: chain of %s released: its own code changed its movement\n", MW_Name( pRoot ) );
			}
			else if ( r.held )
			{
				r.revertedFrame = s_iFrame;
				// (back where it should be, or, if something keeps it from getting there, a second later anyway)
				if ( gpGlobals->time >= r.holdUntil && ( ( pevRoot->origin - r.backOrigin ).Length() < 0.5f
					|| gpGlobals->time >= r.holdUntil + 1.0f ) )
				{
					// release: its own movement and timers from where they were stopped. A speed changed during the
					// hold (trigger_changevalue on speed) takes effect where the root's own code reads it (a func_train
					// at its next corner), as without a hold.
					r.held = FALSE;
					r.wrote = FALSE;
					r.cutting = FALSE;
					MW_RestoreThink( pRoot, r.thinkSwapped, r.savedThink );
					pevRoot->velocity = r.savedVelocity;
					pevRoot->avelocity = r.savedAvelocity;
					pevRoot->ltime = r.frozenLtime;
					pevRoot->nextthink = r.savedNextthink;
					if ( CVAR_GET_FLOAT( "developer" ) >= 2 )
						ALERT( at_aiconsole, "movewith: chain of %s released at %.2f (%.2f from its place)\n", MW_Name( pRoot ),
							gpGlobals->time, ( pevRoot->origin - r.backOrigin ).Length() );
				}
				else
				{
					// held: back to where the blocked frame started, then still. The move back is cut by the think
					// time, as the engine stops any pusher's move there: it ends exactly at that place whatever this
					// frame's length (a velocity for an expected frame length overshoots in a longer frame and swings).
					// Still: its think time out of reach.
					Vector vecBack = r.backOrigin - pevRoot->origin;
					Vector vecBackA;
					for ( int a = 0; a < 3; a++ )
						vecBackA[a] = MW_AngleDiff( r.backAngles[a], pevRoot->angles[a] );
					pevRoot->ltime = r.frozenLtime;
					r.cutting = vecBack.Length() > 0.001f || fabsf( vecBackA.x ) > 0.001f || fabsf( vecBackA.y ) > 0.001f
						|| fabsf( vecBackA.z ) > 0.001f;
					if ( r.cutting )
					{
						pevRoot->velocity = vecBack / MW_CUT_TIME;
						pevRoot->avelocity = vecBackA / MW_CUT_TIME;
						pevRoot->nextthink = r.frozenLtime + MW_CUT_TIME;
					}
					else
					{
						pevRoot->velocity = g_vecZero;
						pevRoot->avelocity = g_vecZero;
						pevRoot->nextthink = r.frozenLtime + MW_ETERNAL_THINK;
					}
					r.wrote = TRUE;
					r.writtenVelocity = pevRoot->velocity;
					r.writtenAvelocity = pevRoot->avelocity;
					r.writtenNextthink = pevRoot->nextthink;
				}
			}
		}

		// the children of a held chain: own movement and think time frozen (given back on release)
		edict_t *pChild = n.hChild.Get();
		BOOL bHeld = bRootPusher && r.held;
		if ( n.kind == MW_KIND_PUSH && !n.converted && !MW_Gone( pChild ) )
		{
			if ( bHeld && !n.held )
			{
				n.held = TRUE;
				n.heldLtime = n.startLtime;
				n.heldNextthink = pChild->v.nextthink;
				n.ownOrigin = n.startOwnOrigin;
				n.ownAngles = n.startOwnAngles;
			}
			if ( n.held )
			{
				if ( bHeld )
				{
					pChild->v.ltime = n.heldLtime;
					n.ownLtime = n.heldLtime;
					if ( n.heldNextthink > n.heldLtime && n.heldNextthink < n.heldLtime + MW_ETERNAL_THINK * 0.5f )
						pChild->v.nextthink = n.heldLtime + MW_ETERNAL_THINK;	// its pending think waits
				}
				else
				{
					n.held = FALSE;
					pChild->v.ltime = n.heldLtime;
					n.ownLtime = n.heldLtime;
					if ( n.heldNextthink > n.heldLtime && n.heldNextthink < n.heldLtime + MW_ETERNAL_THINK * 0.5f )
						pChild->v.nextthink = n.heldNextthink;
				}
			}
		}
	}

	for ( size_t k = 0; k < s_Order.size(); k++ )
	{
		int i = s_Order[k];
		MoveWithNode &n = s_Nodes[i];

		if ( !n.inUse )
			continue;

		edict_t *pChild = n.hChild.Get();
		if ( MW_Gone( pChild ) )
		{
			MW_FreeNode( i );	// a removed child leaves the hierarchy
			continue;
		}
		if ( n.orphan )
			continue;

		edict_t *pParent = n.hParent.Get();
		if ( MW_Gone( pParent ) )
		{
			// a removed parent: its children stop where they are and stay there
			ALERT( at_aiconsole, "movewith: %s lost its parent %s\n", MW_Name( pChild ), STRING( n.iszParent ) );
			n.orphan = TRUE;
			n.hParent = NULL;
			if ( n.kind == MW_KIND_PUSH || n.kind == MW_KIND_NOCLIP )
				MW_StopEntity( pChild );
			s_bDirty = TRUE;
			continue;
		}

		MW_UpdateNode( n, pChild, pParent, frametime );
	}

	MW_RiderGrounds();
	MW_CarryTurningRiders();

	if ( !s_Sounds.empty() )
		MW_SoundFrame();

	s_flLastFrameTime = frametime;
	if ( gpGlobals->time >= s_flNextReport )
	{
		if ( s_flNextReport > 0 && CVAR_GET_FLOAT( "developer" ) >= 2 )
			ALERT( at_aiconsole, "movewith: last 10 s: largest child error %.3f units, %.3f degrees (%s); frame time mismatches: %d, largest frametime error %.4f s\n",
				s_flWorstError, s_flWorstAngle, s_iszWorst ? STRING( s_iszWorst ) : "-", s_iFrameTimeOff, s_flWorstFrameTimeDiff );
		s_flWorstFrameTimeDiff = 0;
		s_flNextReport = gpGlobals->time + 10;
		s_flWorstError = s_flWorstAngle = 0;
		s_iszWorst = 0;
		s_iFrameTimeOff = 0;
	}
}

//=========================================================
// rounds, blocking, network
//=========================================================

// what blocked a chain this frame (MoveWith_Blocked): freed in the next frame when it is only a hair inside a part
static std::vector<EHANDLE> s_Blockers;

static void MW_AddBlocker( CBaseEntity *pOther )
{
	if ( !pOther || pOther->pev->movetype == MOVETYPE_PUSH )
		return;
	for ( size_t i = 0; i < s_Blockers.size(); i++ )
		if ( s_Blockers[i].Get() == pOther->edict() )
			return;
	EHANDLE h;
	h.Set( pOther->edict() );
	s_Blockers.push_back( h );
}

// A blocker that is inside a part of a chain by a hair only (the rounding of two moves, a part that went a hair too
// far): every move of the part pushes it, it stays stuck, and the chain would be held for good. It is moved out by
// the least of a few small steps (at most 1/4 unit), as the player movement code frees a stuck player.
// A player caught inside a part of a chain (too close behind a gun as the boat turns, the gun's brushes inside him),
// or a monster riding the chain caught so, holds the chain for good: every move of the part finds him in it, and he
// can't walk out of a solid. He rides the chain, he isn't something in its way: he is moved out to the nearest clear
// spot (up to 48 units) and the chain goes on.
static BOOL MW_RidesChainOf( edict_t *pent, edict_t *pPart )
{
	edict_t *pGround = pent->v.groundentity;
	if ( FNullEnt( pGround ) || pGround->free || FNullEnt( pPart ) )
		return FALSE;
	MoveWithNode *ng = MW_NodeOf( pGround ), *np = MW_NodeOf( pPart );
	edict_t *rootGround = ( ng && !ng->orphan ) ? MW_ChainRoot( ng ) : pGround;
	edict_t *rootPart = ( np && !np->orphan ) ? MW_ChainRoot( np ) : pPart;
	return rootGround == rootPart;
}

static void MW_FreeBlockers( void )
{
	static const float s_flSteps[] = { 0.03125f, 0.0625f, 0.125f, 0.25f, 1, 2, 4, 8, 12, 16, 24, 32, 48 };
	static const Vector s_vecDirs[] = { Vector( 0, 0, 1 ), Vector( 1, 0, 0 ), Vector( -1, 0, 0 ), Vector( 0, 1, 0 ),
		Vector( 0, -1, 0 ), Vector( 0, 0, -1 ), Vector( 0.7071f, 0.7071f, 0 ), Vector( 0.7071f, -0.7071f, 0 ),
		Vector( -0.7071f, 0.7071f, 0 ), Vector( -0.7071f, -0.7071f, 0 ) };
	static const int s_iHairSteps = 4, s_iHairDirs = 6;	// anything else stuck by a hair only
	for ( size_t i = 0; i < s_Blockers.size(); i++ )
	{
		edict_t *pent = s_Blockers[i].Get();
		if ( FNullEnt( pent ) || pent->free || pent->v.solid == SOLID_NOT || pent->v.solid == SOLID_TRIGGER )
			continue;
		TraceResult tr;
		TRACE_MONSTER_HULL( pent, pent->v.origin, pent->v.origin, dont_ignore_monsters, pent, &tr );
		if ( !tr.fStartSolid || FNullEnt( tr.pHit ) )
			continue;
		int hit = ENTINDEX( tr.pHit );
		if ( !MW_NodeOf( tr.pHit ) && !( hit > 0 && hit < MAX_EDICTS && s_Roots[ hit ].root && s_Roots[ hit ].frame >= s_iFrame - 1 ) )
			continue;	// stuck in something else: not ours to fix
		// a player caught in it (stuck, he may have lost his ground) or a monster riding the chain
		BOOL bRider = ( pent->v.flags & FL_CLIENT ) || MW_RidesChainOf( pent, tr.pHit );
		int iSteps = bRider ? (int)ARRAYSIZE( s_flSteps ) : s_iHairSteps;
		int iDirs = bRider ? (int)ARRAYSIZE( s_vecDirs ) : s_iHairDirs;
		BOOL bFreed = FALSE;
		for ( int k = 0; k < iSteps && !bFreed; k++ )
		{
			for ( int d = 0; d < iDirs && !bFreed; d++ )
			{
				Vector to = pent->v.origin + s_vecDirs[d] * s_flSteps[k];
				TRACE_MONSTER_HULL( pent, to, to, dont_ignore_monsters, pent, &tr );
				if ( !tr.fStartSolid && !tr.fAllSolid )
				{
					UTIL_SetOrigin( &pent->v, to );
					bFreed = TRUE;
					if ( CVAR_GET_FLOAT( "developer" ) >= 2 || ( bRider && s_flSteps[k] >= 1 ) )
						ALERT( bRider ? at_console : at_aiconsole, "movewith: freed %s (#%d) from %s by %.3f units%s\n", STRING( pent->v.classname ),
							ENTINDEX( pent ), MW_Name( INDEXENT( hit ) ), s_flSteps[k], bRider ? " (it rides the chain)" : "" );
				}
			}
		}
	}
	s_Blockers.clear();
}

BOOL MoveWith_Blocked( CBaseEntity *pEntity, CBaseEntity *pOther )
{
	// a map where nothing moves with a parent: the stock Blocked, nothing recorded
	if ( !pEntity || !s_bActive || s_Nodes.empty() )
		return FALSE;

	MoveWithNode *n = MW_NodeOf( pEntity->edict() );
	if ( !n || n->orphan )
	{
		// the root of a chain, blocked itself: the engine put it back, but its children were steered to where it
		// was to be. The chain is held as for a blocked child (they go back with it); the root's own Blocked runs.
		int iSelf = ENTINDEX( pEntity->edict() );
		if ( iSelf > 0 && iSelf < MAX_EDICTS && s_Roots[ iSelf ].root && s_Roots[ iSelf ].frame == s_iFrame )
		{
			s_Roots[ iSelf ].heldFrame = s_iFrame;
			MW_AddBlocker( pOther );
			if ( CVAR_GET_FLOAT( "developer" ) >= 2 )
				ALERT( at_aiconsole, "movewith: chain root %s (#%d) blocked by %s (#%d) at %.2f\n", MW_Name( pEntity->edict() ), iSelf,
					pOther ? STRING( pOther->pev->classname ) : "?", pOther ? ENTINDEX( pOther->edict() ) : 0, gpGlobals->time );
		}
		return FALSE;
	}

	if ( CVAR_GET_FLOAT( "developer" ) >= 2 )
		ALERT( at_aiconsole, "movewith: %s (#%d) blocked by %s (#%d) at %.2f\n", MW_Name( pEntity->edict() ), ENTINDEX( pEntity->edict() ),
			pOther ? STRING( pOther->pev->classname ) : "?", pOther ? ENTINDEX( pOther->edict() ) : 0, gpGlobals->time );

	// The whole chain reacts as one: its root gets the block and applies its own rule (a door or plat reverses, a
	// train, tracktrain, vehicle or rotating part waits and hurts by its "dmg"), and nothing in the chain advances
	// this frame: MoveWith_StartFrame puts the root back and undoes the children's own movement (a hoist rising on a
	// moving crane waits too; it never reverses alone, which would leave the chain out of step).
	edict_t *pRoot = MW_ChainRoot( n );
	CBaseEntity *pRootEntity = CBaseEntity::Instance( pRoot );
	if ( !pRootEntity || pRootEntity == pEntity )
		return FALSE;

	int iRoot = ENTINDEX( pRoot );
	if ( iRoot > 0 && iRoot < MAX_EDICTS )
		s_Roots[ iRoot ].heldFrame = s_iFrame;
	MW_AddBlocker( pOther );

	if ( gpGlobals->time >= n->nextBlock )
	{
		n->nextBlock = gpGlobals->time + MW_BLOCK_INTERVAL;
		pRootEntity->Blocked( pOther );
	}
	return TRUE;
}

// The engine links a moved entity with the box the game gives it (pfnSetAbsBox), and pushes (or is blocked by)
// whatever is inside that box. The stock box of a turned brush entity is a cube around its origin as big as its
// largest bound: for a child without an origin brush the origin is far from its brushes (where the editor's world
// origin went), so the cube can cover the whole map, and any entity stuck in something anywhere in it was pushed by
// the child's turn or blocked it, holding its chain. A child's box is the box around its turned bounds instead (the
// eight corners of mins / maxs, turned): it holds the brushes, as the stock box does, and nothing far from them.
BOOL MoveWith_CollisionBox( entvars_t *pev )
{
	if ( s_Nodes.empty() || pev->solid != SOLID_BSP || pev->angles == g_vecZero )
		return FALSE;

	edict_t *pent = ENT( pev );
	MoveWithNode *n = MW_NodeOf( pent );
	if ( !n || n->kind != MW_KIND_PUSH || !MW_IsBrush( pent ) )
		return FALSE;

	mwmatrix_t rot;
	MW_EntityRot( pent, pev->angles, rot );
	Vector lo( 1e30f, 1e30f, 1e30f ), hi( -1e30f, -1e30f, -1e30f );
	for ( int c = 0; c < 8; c++ )
	{
		Vector corner( ( c & 1 ) ? pev->maxs.x : pev->mins.x, ( c & 2 ) ? pev->maxs.y : pev->mins.y,
			( c & 4 ) ? pev->maxs.z : pev->mins.z );
		Vector p = MW_RotateV( rot, corner );
		for ( int i = 0; i < 3; i++ )
		{
			lo[i] = fminf( lo[i], p[i] );
			hi[i] = fmaxf( hi[i], p[i] );
		}
	}
	pev->absmin = pev->origin + lo - Vector( 1, 1, 1 );
	pev->absmax = pev->origin + hi + Vector( 1, 1, 1 );
	return TRUE;
}

static void MW_AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, unsigned char *pSet );

void MoveWith_AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, unsigned char *pSet )
{
#ifdef SBO_TEST_COMMANDS
	if ( s_bCost )
	{
		double t0 = MW_Now();
		MW_AddToFullPack( state, e, ent, host, pSet );
		s_flCostPack += MW_Now() - t0;
		s_iCostPackCalls++;
		return;
	}
#endif
	MW_AddToFullPack( state, e, ent, host, pSet );
}

static void MW_AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, unsigned char *pSet )
{
	if ( s_Nodes.empty() )
		return;


	// a sound entity of a child ambient_generic that comes back into a client's PVS: its looping sound is caught up
	for ( size_t i = 0; i < s_Seen.size(); i++ )
	{
		if ( s_Seen[i].index != e )
			continue;
		int h = ENTINDEX( host );
		if ( h >= 1 && h <= 32 )
		{
			if ( gpGlobals->time - s_Seen[i].lastSent[h] > MW_SOUND_PVS_GAP )
				MW_SoundCatchUp( ent );
			s_Seen[i].lastSent[h] = gpGlobals->time;
		}
		break;
	}

	MoveWithNode *n = MW_NodeOf( ent );
	if ( !n || n->orphan || !n->compose )
		return;

	// the client can place the child only on a parent it receives in the same packet
	edict_t *pParent = n->hParent.Get();
	if ( MW_Gone( pParent ) )
		return;

	if ( !MW_Sendable( pParent ) )
		return;
	int iParent = ENTINDEX( pParent );
	if ( pParent != host && pSet && !ENGINE_CHECK_VISIBILITY( pParent, pSet ) )
		return;

	// the local pose: constant, except for a child moving in its parent's frame (a door opening on a train, a
	// velocity or a new place given by trigger_motion or trigger_changevalue)
	if ( n->ownVelocity != g_vecZero || n->ownAvelocity != g_vecZero || n->ownOrigin != n->sentOwnOrigin
		|| n->ownAngles != n->sentOwnAngles )
	{
		Vector localOrigin;
		mwmatrix_t localRot;
		MW_AdvanceOwn( *n, &ent->v );
		MW_LocalPose( *n, ent, n->ownOrigin, n->ownAngles, localOrigin, localRot );
		n->stateOrigin = localOrigin;
		n->stateAngles = MW_MatrixAnglesV( localRot, n->stateAngles );
		n->sentOwnOrigin = n->ownOrigin;
		n->sentOwnAngles = n->ownAngles;
	}

	// an attachment the parent's model no longer has: on the parent's origin with the local pose instead
	int iAttachment = ( n->attachment && MW_HasAttachment( pParent, n->attachment ) ) ? n->attachment : 0;
	state->origin = ( n->attachment && !iAttachment ) ? n->attachLocalOrigin : n->stateOrigin;
	state->angles = n->stateAngles;
	state->iuser4 = iParent | ( iAttachment << MOVEWITH_STATE_ATTACHMENT_SHIFT );
}

// The baseline of a child the client places on its parent: the local pose and the link it is sent with
// (MW_AddToFullPack), not its world pose without a link. A client gets an entity that comes into its view as a delta
// from its baseline: with the world pose there, every placed child that came back into view at once (hundreds, when
// an eye comes up from under water) was sent with its origin, angles and link, and the update overflowed. The engine
// makes the baselines after the first server frame, so the links are made by then.
void MoveWith_CreateBaseline( struct entity_state_s *baseline, edict_t *ent )
{
	if ( s_Nodes.empty() )
		return;
	MoveWithNode *n = MW_NodeOf( ent );
	if ( !n || n->orphan || !n->compose )
		return;
	edict_t *pParent = n->hParent.Get();
	if ( MW_Gone( pParent ) || !MW_Sendable( pParent ) )
		return;
	int iAttachment = ( n->attachment && MW_HasAttachment( pParent, n->attachment ) ) ? n->attachment : 0;
	baseline->origin = ( n->attachment && !iAttachment ) ? n->attachLocalOrigin : n->stateOrigin;
	baseline->angles = n->stateAngles;
	baseline->iuser4 = ENTINDEX( pParent ) | ( iAttachment << MOVEWITH_STATE_ATTACHMENT_SHIFT );
}

//=========================================================
// ambient_generic (sound.cpp, CAmbientGeneric::EmitAmbient)
//=========================================================

// sent to the clients the way the game's AddToFullPack decides it (client.cpp, as the HL SDK): it leaves out an
// entity whose effects are exactly EF_NODRAW, not one with EF_NODRAW and another bit. The test must stay the same
// as there: a child is sent on its parent only when the parent is in the same update (suite movewith t11).
static BOOL MW_Sendable( edict_t *pent )
{
	int i = ENTINDEX( pent );
	return i > 0 && i < MAX_EDICTS && pent->v.modelindex && pent->v.model && pent->v.effects != EF_NODRAW
		&& !( pent->v.flags & FL_CLIENT );
}

// The entity a child ambient_generic's sound plays on: a sent entity of its hierarchy (the nearest sent ancestor, or a
// sent child of that ancestor that is closer to the ambient), chosen once and kept, so starts, changes and stops all
// go to the same entity and sample.
static Vector MW_Center( edict_t *pent )
{
	return ( pent->v.absmin + pent->v.absmax ) * 0.5f;
}

// pent is under pAncestor in the hierarchy
static BOOL MW_IsUnder( edict_t *pent, edict_t *pAncestor )
{
	MoveWithNode *n = MW_NodeOf( pent );

	for ( int depth = 0; n && !n->orphan && depth <= MW_MAX_DEPTH; depth++ )
	{
		edict_t *pParent = n->hParent.Get();
		if ( pParent == pAncestor )
			return TRUE;
		n = MW_NodeOf( pParent );
	}
	return FALSE;
}

static MoveWithSound *MW_SoundOf( edict_t *pent )
{
	for ( size_t i = 0; i < s_Sounds.size(); i++ )
	{
		if ( s_Sounds[i].hAmbient.Get() == pent )
			return &s_Sounds[i];
	}
	return NULL;
}

// another child ambient's sound with this sample plays on pSoundEnt
static BOOL MW_SoundShared( edict_t *pSoundEnt, edict_t *pAmbient, const char *szSample )
{
	if ( !szSample[0] )
		return FALSE;
	int index = ENTINDEX( pSoundEnt );
	for ( size_t i = 0; i < s_Sounds.size(); i++ )
	{
		MoveWithSound &o = s_Sounds[i];	// not const: EHANDLE::Get isn't
		edict_t *pOther = o.hAmbient.Get();
		if ( pOther && pOther != pAmbient && o.startedIndex == index && !stricmp( o.startedSample, szSample ) )
			return TRUE;
	}
	return FALSE;
}

// the live entity of a recorded start, or NULL (gone, or its slot holds another entity now)
static edict_t *MW_StartedEntity( const MoveWithSound &s )
{
	if ( s.startedIndex <= 0 || s.startedIndex >= gpGlobals->maxEntities )
		return NULL;
	edict_t *p = INDEXENT( s.startedIndex );
	if ( !p || p->free || p->serialnumber != s.startedSerial )
		return NULL;
	return p;
}

void MoveWith_SoundStarted( edict_t *pAmbient, edict_t *pSoundEnt, const char *szSample )
{
	MoveWithSound *s = MW_SoundOf( pAmbient );
	if ( !s || !pSoundEnt || !szSample )
		return;
	s->startedIndex = ENTINDEX( pSoundEnt );
	s->startedSerial = pSoundEnt->serialnumber;
	snprintf( s->startedSample, sizeof( s->startedSample ), "%s", szSample );
}

edict_t *MoveWith_SoundStartedOn( edict_t *pAmbient, const char *szSample )
{
	if ( s_Sounds.empty() || !szSample )
		return NULL;
	MoveWithSound *s = MW_SoundOf( pAmbient );
	if ( !s || stricmp( s->startedSample, szSample ) )
		return NULL;
	return MW_StartedEntity( *s );
}

void MoveWith_SoundStopped( edict_t *pAmbient )
{
	MoveWithSound *s = s_Sounds.empty() ? NULL : MW_SoundOf( pAmbient );
	if ( s )
	{
		s->startedIndex = 0;
		s->startedSample[0] = 0;
	}
}

// An entity is being removed (CBaseEntity::UpdateOnRemove), while it still exists: the sounds child ambients started on
// it stop now. The clients keep a static sound playing after its entity is gone, and no later stop could reach it.
void MoveWith_EntityRemoved( edict_t *pent )
{
	if ( s_Sounds.empty() || !pent )
		return;
	int index = ENTINDEX( pent );
	for ( size_t i = 0; i < s_Sounds.size(); i++ )
	{
		MoveWithSound &s = s_Sounds[i];
		if ( s.startedIndex != index || s.startedSerial != pent->serialnumber || !s.startedSample[0] )
			continue;
		EMIT_SOUND_DYN( pent, CHAN_STATIC, s.startedSample, 0, 0, SND_STOP, PITCH_NORM );
		if ( CVAR_GET_FLOAT( "developer" ) >= 2 )
			ALERT( at_aiconsole, "movewith: %s stopped with %s\n", s.startedSample, MW_Name( pent ) );
		s.startedIndex = 0;
		s.startedSample[0] = 0;
	}
}

edict_t *MoveWith_SoundEntity( edict_t *pent )
{
	if ( s_Nodes.empty() )
		return NULL;

	MoveWithNode *n = MW_NodeOf( pent );
	if ( !n || n->orphan )
		return NULL;

	MoveWithSound *s = MW_SoundOf( pent );
	edict_t *pKept = s ? s->hSoundEnt.Get() : NULL;
	if ( pKept && !MW_Gone( pKept ) && MW_Sendable( pKept ) )
		return pKept;

	// up the chain to the first entity the clients receive
	edict_t *pAncestor = n->hParent.Get();
	for ( int depth = 0; pAncestor && depth <= MW_MAX_DEPTH; depth++ )
	{
		if ( MW_Gone( pAncestor ) )
			return NULL;
		if ( MW_Sendable( pAncestor ) )
			break;
		MoveWithNode *pn = MW_NodeOf( pAncestor );
		pAncestor = ( pn && !pn->orphan ) ? pn->hParent.Get() : NULL;
	}
	if ( !pAncestor || !MW_Sendable( pAncestor ) )
		return NULL;

	// a sent entity under that ancestor that is nearer to the ambient keeps more of its place. The clients tell a
	// static sound by its entity and sample (a stop or a change reaches every sound of that pair), so an entity that
	// already plays the same sample for another child ambient is left for the next one (R10)
	const char *szSample = FStringNull( pent->v.message ) ? "" : STRING( pent->v.message );
	edict_t *pBest = MW_SoundShared( pAncestor, pent, szSample ) ? NULL : pAncestor;
	float flBest = pBest ? ( MW_Center( pAncestor ) - pent->v.origin ).Length() : 1.0e9f;
	for ( size_t i = 0; i < s_Nodes.size(); i++ )
	{
		edict_t *pOther = s_Nodes[i].hChild.Get();
		if ( !s_Nodes[i].inUse || s_Nodes[i].orphan || !pOther || pOther == pent || MW_Gone( pOther ) || !MW_Sendable( pOther ) )
			continue;
		float fl = ( MW_Center( pOther ) - pent->v.origin ).Length();
		if ( fl < flBest && MW_IsUnder( pOther, pAncestor ) && !MW_SoundShared( pOther, pent, szSample ) )
		{
			pBest = pOther;
			flBest = fl;
		}
	}
	if ( !pBest )
	{
		pBest = pAncestor;
		ALERT( at_console, "movewith: %s plays %s on %s, where another sound that moves with it plays it too, so turning one off or changing it changes both\n",
			MW_Name( pent ), szSample, MW_Name( pAncestor ) );
	}

	if ( !s )
	{
		s_Sounds.push_back( MoveWithSound() );
		s = &s_Sounds.back();
		memset( s, 0, sizeof( *s ) );
		s->hAmbient.Set( pent );
	}
	s->hSoundEnt.Set( pBest );

	int index = ENTINDEX( pBest );
	BOOL bSeen = FALSE;
	for ( size_t i = 0; i < s_Seen.size(); i++ )
		bSeen |= ( s_Seen[i].index == index );
	if ( !bSeen )
	{
		MoveWithSeen seen;
		memset( &seen, 0, sizeof( seen ) );
		seen.index = index;
		for ( int c = 0; c < 33; c++ )
			seen.lastSent[c] = -1000;
		s_Seen.push_back( seen );
	}
	return pBest;
}

// a catch-up for the sounds on this entity (NULL: all), at most once a second each (MoveWith_StartFrame sends it)
static void MW_SoundCatchUp( edict_t *pSoundEnt )
{
	for ( size_t i = 0; i < s_Sounds.size(); i++ )
	{
		if ( !pSoundEnt || s_Sounds[i].hSoundEnt.Get() == pSoundEnt )
			s_Sounds[i].pending = TRUE;
	}
}

static void MW_SoundFrame( void )
{
	for ( size_t i = 0; i < s_Sounds.size(); i++ )
	{
		MoveWithSound &s = s_Sounds[i];
		if ( !s.pending || gpGlobals->time < s.nextRefresh )
			continue;

		s.pending = FALSE;
		s.nextRefresh = gpGlobals->time + MW_SOUND_REFRESH_INTERVAL;
		CBaseEntity *pAmbient = CBaseEntity::Instance( s.hAmbient.Get() );
		if ( pAmbient && !MW_Gone( pAmbient->edict() ) )
			AmbientGeneric_MoveWithCatchUp( pAmbient );
	}
}

void MoveWith_ClientHUDInit( void )
{
	MW_SoundCatchUp( NULL );
}

BOOL MoveWith_HasParentKey( edict_t *pent )
{
	MoveWithEntry *e = MW_Entry( pent );
	return e && ( e->iszMoveWith || e->iszParent );
}

//=========================================================
// changing the parent (trigger_changeparent)
//=========================================================

BOOL MoveWith_SetParent( CBaseEntity *pChild, CBaseEntity *pParent, int iAttachment )
{
	if ( !pChild )
		return FALSE;

	edict_t *pentChild = pChild->edict();
	MW_Unlink( pentChild, TRUE );
	if ( !pParent )
		return TRUE;

	// keeps its world pose: linked from where both are now
	if ( !MW_LinkNode( pentChild, pParent->edict(), iAttachment, FALSE, NULL, NULL ) )
		return FALSE;

	MoveWithNode *n = MW_NodeOf( pentChild );
	if ( n )
		n->iszParent = pParent->pev->targetname;
	return TRUE;
}

//=========================================================
// child movers (CBaseToggle::LinearMove / AngularMove)
//=========================================================

static MoveWithNode *MW_MoverNode( CBaseEntity *pEntity )
{
	if ( !pEntity || s_Nodes.empty() )
		return NULL;

	MoveWithNode *n = MW_NodeOf( pEntity->edict() );
	if ( !n || n->orphan || n->kind != MW_KIND_PUSH )
		return NULL;
	return n;
}

BOOL MoveWith_IsChild( edict_t *pent )
{
	if ( !pent || s_Nodes.empty() )
		return FALSE;
	MoveWithNode *n = MW_NodeOf( pent );
	return n && !n->orphan;
}

BOOL MoveWith_GetOwnPose( CBaseEntity *pEntity, Vector *pOrigin, Vector *pAngles )
{
	MoveWithNode *n = MW_MoverNode( pEntity );

	if ( !n )
		return FALSE;

	MW_AdvanceOwn( *n, pEntity->pev );
	if ( pOrigin )
		*pOrigin = n->ownOrigin;
	if ( pAngles )
		*pAngles = n->ownAngles;
	return TRUE;
}

Vector MoveWith_GetOwnAvelocity( CBaseEntity *pEntity )
{
	MoveWithNode *n = MW_MoverNode( pEntity );
	return n ? n->ownAvelocity : pEntity->pev->avelocity;
}

Vector MoveWith_GetOwnVelocity( CBaseEntity *pEntity )
{
	MoveWithNode *n = MW_MoverNode( pEntity );
	return n ? n->ownVelocity : pEntity->pev->velocity;
}

// the parent's current rotation and origin (a child with a parent), FALSE otherwise
static BOOL MW_ParentFrameNow( CBaseEntity *pEntity, MoveWithNode **ppNode, Vector &parentOrigin, mwmatrix_t parentRot )
{
	MoveWithNode *n = MW_MoverNode( pEntity );
	if ( !n )
		return FALSE;
	edict_t *pParent = n->hParent.Get();
	if ( MW_Gone( pParent ) )
		return FALSE;
	parentOrigin = pParent->v.origin;
	MW_EntityRot( pParent, pParent->v.angles, parentRot );
	*ppNode = n;
	return TRUE;
}

BOOL MoveWith_WorldToOwnDirection( CBaseEntity *pEntity, const Vector &vecWorld, Vector *pOwn )
{
	MoveWithNode *n;
	Vector parentOrigin;
	mwmatrix_t parentRot;

	if ( !pEntity || !MW_ParentFrameNow( pEntity, &n, parentOrigin, parentRot ) )
		return FALSE;

	// world = parent x link^-1 x own, so own = link x parent^-1 x world
	*pOwn = MW_RotateV( n->bindRot, MW_IRotateV( parentRot, vecWorld ) );
	return TRUE;
}

Vector MoveWith_ToParentFrame( CBaseEntity *pEntity, const Vector &vecWorld )
{
	MoveWithNode *n;
	Vector parentOrigin;
	mwmatrix_t parentRot;

	if ( !pEntity || !MW_ParentFrameNow( pEntity, &n, parentOrigin, parentRot ) )
		return vecWorld;
	return MW_IRotateV( parentRot, vecWorld - parentOrigin );
}

void MoveWith_SetOwnVelocity( CBaseEntity *pEntity, const Vector &vecVelocity )
{
	MoveWithNode *n = MW_MoverNode( pEntity );

	if ( !n )
	{
		pEntity->pev->velocity = vecVelocity;
		return;
	}
	MW_AdvanceOwn( *n, pEntity->pev );
	n->ownVelocity = vecVelocity;
}

void MoveWith_SetOwnAvelocity( CBaseEntity *pEntity, const Vector &vecAvelocity )
{
	MoveWithNode *n = MW_MoverNode( pEntity );

	if ( !n )
	{
		pEntity->pev->avelocity = vecAvelocity;
		return;
	}
	MW_AdvanceOwn( *n, pEntity->pev );
	n->ownAvelocity = vecAvelocity;
}

void MoveWith_SetOwnOrigin( CBaseEntity *pEntity, const Vector &vecOrigin )
{
	MoveWithNode *n = MW_MoverNode( pEntity );

	if ( !n )
	{
		UTIL_SetOrigin( pEntity->pev, vecOrigin );
		return;
	}
	MW_AdvanceOwn( *n, pEntity->pev );
	n->ownOrigin = vecOrigin;
}

void MoveWith_SetOwnAngles( CBaseEntity *pEntity, const Vector &vecAngles )
{
	MoveWithNode *n = MW_MoverNode( pEntity );

	if ( !n )
	{
		pEntity->pev->angles = vecAngles;
		return;
	}
	MW_AdvanceOwn( *n, pEntity->pev );
	n->ownAngles = vecAngles;
}

//=========================================================
// a child's pose and motion as other entities set them (trigger_motion, motion_manager, trigger_changevalue): a place
// in the world is where the child goes now, and it keeps that place on its parent; a velocity or angular velocity is
// its own, on top of its parent's motion (Spirit of Half-Life's UTIL_AssignOrigin / UTIL_SetVelocity)
//=========================================================

// a linked child of any kind but an attachment sprite (the engine places that one), with its parent here
static MoveWithNode *MW_PoseNode( CBaseEntity *pEntity, edict_t **ppParent )
{
	if ( !pEntity || s_Nodes.empty() )
		return NULL;

	MoveWithNode *n = MW_NodeOf( pEntity->edict() );
	if ( !n || n->orphan || n->kind == MW_KIND_FOLLOW )
		return NULL;
	edict_t *pParent = n->hParent.Get();
	if ( MW_Gone( pParent ) )
		return NULL;
	*ppParent = pParent;
	return n;
}

BOOL MoveWith_IsChild( CBaseEntity *pEntity )
{
	edict_t *pParent;
	return MW_PoseNode( pEntity, &pParent ) != NULL;
}

void MoveWith_AssignOrigin( CBaseEntity *pEntity, const Vector &vecOrigin )
{
	edict_t *pParent;
	MoveWithNode *n = MW_PoseNode( pEntity, &pParent );

	if ( !n )
	{
		UTIL_SetOrigin( pEntity->pev, vecOrigin );
		return;
	}

	mwmatrix_t parentRot;
	MW_EntityRot( pParent, pParent->v.angles, parentRot );
	MW_AdvanceOwn( *n, pEntity->pev );
	n->ownOrigin = n->bindOrigin + MW_RotateV( n->bindRot, MW_IRotateV( parentRot, vecOrigin - pParent->v.origin ) );
	n->lockOrigin = vecOrigin;	// parentflags: a kept axis takes the new value too

	// there at once, as UTIL_SetOrigin puts any other entity (a solid child is not slid there by its steering)
	UTIL_SetOrigin( pEntity->pev, vecOrigin );
	n->endOrigin = vecOrigin;
	n->assigned = TRUE;
}

void MoveWith_AssignAngles( CBaseEntity *pEntity, const Vector &vecAngles )
{
	edict_t *pParent;
	MoveWithNode *n = MW_PoseNode( pEntity, &pParent );

	pEntity->pev->angles = vecAngles;
	if ( !n )
		return;

	if ( n->kind != MW_KIND_TRIGGER )
	{
		// own rotation = link x parent^-1 x world
		mwmatrix_t parentRot, worldRot, rel, own;
		MW_EntityRot( pParent, pParent->v.angles, parentRot );
		MW_EntityRot( pEntity->edict(), vecAngles, worldRot );
		MW_MatTMul( parentRot, worldRot, rel );
		MW_MatMul( n->bindRot, rel, own );
		MW_AdvanceOwn( *n, pEntity->pev );
		n->ownAngles = MW_EntityAngles( pEntity->edict(), own, n->ownAngles );
	}
	n->lockAngles = vecAngles;
	n->endAngles = vecAngles;
	MW_EntityRot( pEntity->edict(), vecAngles, n->endRot );
}

Vector MoveWith_GetVelocity( CBaseEntity *pEntity )
{
	edict_t *pParent;
	MoveWithNode *n = MW_PoseNode( pEntity, &pParent );

	if ( !n )
		return pEntity->pev->velocity;

	// own velocity (link frame) as a world direction with the parent's turn now
	mwmatrix_t parentRot;
	MW_EntityRot( pParent, pParent->v.angles, parentRot );
	return MW_RotateV( parentRot, MW_IRotateV( n->bindRot, n->ownVelocity ) );
}

void MoveWith_SetVelocity( CBaseEntity *pEntity, const Vector &vecVelocity )
{
	edict_t *pParent;
	MoveWithNode *n = MW_PoseNode( pEntity, &pParent );

	if ( !n )
	{
		pEntity->pev->velocity = vecVelocity;
		return;
	}

	mwmatrix_t parentRot;
	MW_EntityRot( pParent, pParent->v.angles, parentRot );
	MW_AdvanceOwn( *n, pEntity->pev );
	n->ownVelocity = MW_RotateV( n->bindRot, MW_IRotateV( parentRot, vecVelocity ) );
}

Vector MoveWith_GetAvelocity( CBaseEntity *pEntity )
{
	edict_t *pParent;
	MoveWithNode *n = MW_PoseNode( pEntity, &pParent );
	return n ? n->ownAvelocity : pEntity->pev->avelocity;
}

void MoveWith_SetAvelocity( CBaseEntity *pEntity, const Vector &vecAvelocity )
{
	edict_t *pParent;
	MoveWithNode *n = MW_PoseNode( pEntity, &pParent );

	if ( !n )
	{
		pEntity->pev->avelocity = vecAvelocity;
		return;
	}
	MW_AdvanceOwn( *n, pEntity->pev );
	n->ownAvelocity = vecAvelocity;
}

// point_worldtext and a point-mode vgui_screen: the clients draw them from a copy, placed on the parent's drawn pose
// every frame. The parent must be an entity the clients receive. *piParent: as entity_state_t::iuser4 (parent index,
// attachment << MOVEWITH_STATE_ATTACHMENT_SHIFT); the local pose: world = parent x local (origin on the attachment
// when there is one; angles as Quake angles, movewith_shared.h). FALSE: not such a child (drawn where it is).
BOOL MoveWith_ClientLink( CBaseEntity *pEntity, int *piParent, Vector *pLocalOrigin, Vector *pLocalAngles )
{
	edict_t *pParent;
	MoveWithNode *n = MW_PoseNode( pEntity, &pParent );

	// linked even while the parent isn't sent (hidden, no model yet): each client places it on the parent once it
	// receives it, and draws it at the world pose sent with it until then (R49)
	if ( !n )
		return FALSE;

	Vector localOrigin;
	mwmatrix_t localRot;
	MW_AdvanceOwn( *n, pEntity->pev );
	MW_LocalPose( *n, pEntity->edict(), n->ownOrigin, n->ownAngles, localOrigin, localRot );
	int iAttachment = ( n->attachment && MW_HasAttachment( pParent, n->attachment ) ) ? n->attachment : 0;
	*piParent = ENTINDEX( pParent ) | ( iAttachment << MOVEWITH_STATE_ATTACHMENT_SHIFT );
	*pLocalOrigin = iAttachment ? g_vecZero : localOrigin;
	*pLocalAngles = MW_MatrixAnglesV( localRot, g_vecZero );
	return TRUE;
}

// run-time keys (keyvalue.cpp KeyValue_DispatchRuntime)
static BOOL MW_IsPoseKey( const char *szKeyName, int &which )
{
	static const char *s_szKeys[] = { "origin", "angles", "velocity", "avelocity", NULL };
	for ( int i = 0; s_szKeys[i]; i++ )
	{
		if ( !stricmp( szKeyName, s_szKeys[i] ) )
		{
			which = i;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL MoveWith_RuntimeKeyValue( edict_t *pent, const char *szKeyName, const char *szValue )
{
	int which;
	CBaseEntity *pEntity = CBaseEntity::Instance( pent );

	if ( !szKeyName || !MW_IsPoseKey( szKeyName, which ) || !MoveWith_IsChild( pEntity ) )
		return FALSE;

	Vector v = g_vecZero;
	UTIL_StringToVector( (float *)v, szValue ? szValue : "" );
	switch ( which )
	{
	case 0: MoveWith_AssignOrigin( pEntity, v ); break;
	case 1: MoveWith_AssignAngles( pEntity, v ); break;
	case 2: MoveWith_SetVelocity( pEntity, v ); break;
	default: MoveWith_SetAvelocity( pEntity, v ); break;
	}
	return TRUE;
}

BOOL MoveWith_GetKeyValue( CBaseEntity *pEntity, const char *szKeyName, Vector &vecValue )
{
	int which;

	if ( !szKeyName || !MW_IsPoseKey( szKeyName, which ) || which < 2 || !MoveWith_IsChild( pEntity ) )
		return FALSE;
	vecValue = ( which == 2 ) ? MoveWith_GetVelocity( pEntity ) : MoveWith_GetAvelocity( pEntity );
	return TRUE;
}

static BOOL MW_ResolveNewParent( string_t iszNewParent, const char *szWho, CBaseEntity **ppParent, int &iAttachment,
	CBaseEntity *pActivator = NULL, CBaseEntity *pCaller = NULL );
static BOOL MW_ChangeParent( CBaseEntity *pChild, CBaseEntity *pParent, int iAttachment, string_t iszNewParent );

void MoveWith_RuntimeKeyChanged( edict_t *pent, const char *szKeyName )
{
	if ( !s_bActive || !szKeyName )
		return;
	BOOL bMoveWith = !stricmp( szKeyName, "movewith" ), bParent = !stricmp( szKeyName, "parent" );
	if ( !bMoveWith && !bParent && stricmp( szKeyName, "parentflags" ) )
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance( pent );
	MoveWithEntry *e = MW_Entry( pent );
	if ( !pEntity || !e || FClassnameIs( pent, "env_particle" ) )	// env_particle: the key is its own "target"
		return;

	char szWho[ 160 ];
	snprintf( szWho, sizeof( szWho ), "%s %s, key %s", STRING( pent->v.classname ), STRING( pent->v.targetname ), szKeyName );

	if ( !bMoveWith && !bParent )
	{
		// "parentflags": the link it has now (from the map, trigger_changeparent or a key) stays, with its own
		// movement (a child door keeps opening); the frozen axes keep their world value from now on
		MoveWithNode *n = MW_NodeOf( pent );
		if ( !n || n->orphan )
			return;
		n->flags = e->parentflags;
		n->lockOrigin = pent->v.origin;
		n->lockAngles = pent->v.angles;
		// as MW_LinkNode: an axis lock is applied on the server, so the child is no longer placed by the clients
		n->compose = ( n->kind == MW_KIND_POSITION || n->kind == MW_KIND_PUSH || n->kind == MW_KIND_NOCLIP )
			&& pent->v.modelindex && pent->v.model && !n->flags;
		return;
	}

	// "movewith" / "parent": the name just written (empty: no parent)
	string_t iszName = bMoveWith ? e->iszMoveWith : e->iszParent;
	CBaseEntity *pParent;
	int iAttachment;
	if ( MW_ResolveNewParent( iszName, szWho, &pParent, iAttachment ) )
		MW_ChangeParent( pEntity, pParent, iAttachment, iszName );
}

//=========================================================
// trigger_changeparent (XashXT's name and key, as trigger_changetarget): "target" gets the parent "m_iszNewParent"
// ("name" or "name.N"; empty: no parent). The entity keeps where it is.
//=========================================================

class CTriggerChangeParent : public CBaseDelay
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

private:
	string_t m_iszNewParent;
};

LINK_ENTITY_TO_CLASS( trigger_changeparent, CTriggerChangeParent );

void CTriggerChangeParent::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszNewParent" ) )
	{
		m_iszNewParent = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : 0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerChangeParent::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
}

// The one way a parent changes while the map runs, for trigger_changeparent and a run-time "movewith" / "parent" /
// "parentflags" key (MoveWith_RuntimeKeyChanged): the new parent's name resolved once (iszNewParent "name" or
// "name.N"; 0 = no parent; FALSE with a console line when nothing has that name), then each entity changed
static BOOL MW_ResolveNewParent( string_t iszNewParent, const char *szWho, CBaseEntity **ppParent, int &iAttachment,
	CBaseEntity *pActivator, CBaseEntity *pCaller )
{
	*ppParent = NULL;
	iAttachment = 0;
	if ( !iszNewParent )
		return TRUE;

	edict_t *pent = MW_FindParent( iszNewParent, iAttachment, pActivator, pCaller );
	*ppParent = pent ? CBaseEntity::Instance( pent ) : NULL;
	if ( !*ppParent )
	{
		ALERT( at_console, "%s found no entity named \"%s\"\n", szWho, STRING( iszNewParent ) );
		return FALSE;
	}
	return TRUE;
}

// the entity keeps its world pose; FALSE: the link was refused (console line), and it has no parent now
static BOOL MW_ChangeParent( CBaseEntity *pChild, CBaseEntity *pParent, int iAttachment, string_t iszNewParent )
{
	if ( !MoveWith_SetParent( pChild, pParent, iAttachment ) )
		return FALSE;
	MoveWithNode *n = MW_NodeOf( pChild->edict() );
	if ( n && iszNewParent )
		n->iszParent = iszNewParent;
	// as for a run-time key: a class the clients draw from a copy resends it (point_worldtext, vgui_screen)
	pChild->RuntimeKeyValueChanged( "movewith" );
	return TRUE;
}

void CTriggerChangeParent::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FStringNull( pev->target ) )
		return;

	char szWho[ 160 ];
	snprintf( szWho, sizeof( szWho ), "trigger_changeparent %s", STRING( pev->targetname ) );
	CBaseEntity *pParent;
	int iAttachment;
	if ( !MW_ResolveNewParent( m_iszNewParent, szWho, &pParent, iAttachment, pActivator, this ) )
		return;

	// the targets as every targeting entity finds them (alias.cpp's trigger_changealias: activator, and this as caller)
	CBaseEntity *pTarget = NULL;
	while ( ( pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ), pActivator, this ) ) != NULL )
	{
		if ( !MW_Gone( pTarget->edict() ) )
			MW_ChangeParent( pTarget, pParent, iAttachment, m_iszNewParent );
	}
}
