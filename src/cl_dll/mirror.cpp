//=========================================================
// SBO 2.0: env_mirror, client side. Spirit of Half-Life 1.8 (G-Cont: cl_dll/ev_hldm.cpp EV_Mirror, the
// player_marker entity and its hook in StudioModelRenderer.cpp); docs/ENV_MIRROR.md.
//
// Spirit's server played a reliable global event once, when player 1 existed; SBO's sends the "Mirror" message to
// each player whenever his HUD is (re)initialised and to everybody when a mirror is switched (dlls/env_mirror.cpp).
// The list is cleared on a new map (VidInit) and on InitHUD, the server then sends the active mirrors again.
//
// Spirit drew the local player's reflection through player_marker, an invisible server entity at the mirror whose
// model (models/null.mdl) the client drew as player 1: single player only, and its own entity in every client's
// update. SBO adds a copy of the view player's entity itself, in HUD_CreateEntities, only in first person and
// only near a mirror that reflects players; StudioModelRenderer.cpp draws that copy's reflections only.
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_types.h"
#include "parsemsg.h"
#include "pm_shared.h"	// OBS_IN_EYE
#include "com_model.h"	// model_t type (sprites)
#include "r_efx.h"		// TEMPENTITY
#include "mirror.h"
#include "mirror_gl.h"
#include "triangleapi.h"
#include "r_studioint.h"	// IEngineStudio.GetViewInfo: the view being drawn

extern engine_studio_api_t IEngineStudio;

#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern "C" int CL_IsThirdPerson( void );
extern vec3_t v_origin;	// view.cpp: the last view origin (the view being drawn while the effects are)

// "Mirror" message flags (server: dlls/env_mirror.cpp)
#define MIRROR_FLAG_ON			0x01
#define MIRROR_FLAG_PLAYERS		0x02

static sbo_mirror_t	s_Mirrors[MAX_MIRRORS];
static int			s_iMirrors = 0;

static cl_entity_t	s_PlayerCopy;			// the view player's entity, for his reflection in first person
static int			s_iPlayerCopyIndex = 0;	// his index, 0 = no copy this frame
static int			s_fPlayerCopyLocal = 0;	// the copy is the local player (not the player watched in eye)

static float		s_vecLocalOrigin[3];		// the local player's predicted origin and view angles (Mirror_SetLocalView)
static float		s_vecLocalAngles[3];
static int			s_fLocalView = 0;

// Sprite entities and temp entities: per-frame visible-entity copies at the mirrored positions (no tempent, no
// server entity). The engine's list of visible entities is shared with everything else (GoldSrc: 512 entries,
// MAX_VISEDICTS; assumed for HL25), and a temp entity that finds it full dies, so the copies are capped low.
#define MIRROR_MAX_SPRITE_CANDIDATES	64	// sprite entities near a mirror noted per frame (HUD_AddEntity)
#define MIRROR_MAX_SPRITE_COPIES		16	// copies of sprite entities per frame
#define MIRROR_MAX_TEMPENT_COPIES		16	// copies of sprite temp entities per frame

static cl_entity_t	*s_pSpriteEnts[MIRROR_MAX_SPRITE_CANDIDATES];
static int			s_iSpriteEnts = 0;
static cl_entity_t	s_SpriteCopies[MIRROR_MAX_SPRITE_COPIES];
static cl_entity_t	s_TempEntCopies[MIRROR_MAX_TEMPENT_COPIES];

static void Mirror_Remove( int id )
{
	for ( int i = 0; i < s_iMirrors; i++ )
	{
		if ( s_Mirrors[i].id == id )
		{
			s_Mirrors[i] = s_Mirrors[--s_iMirrors];
			return;
		}
	}
}

// short id, byte flags; ON: byte axis, char side, coord mins x y z, coord maxs x y z, coord plane, short radius
static int MsgFunc_Mirror( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int id = READ_SHORT();
	int flags = READ_BYTE();

	if ( !( flags & MIRROR_FLAG_ON ) )
	{
		Mirror_Remove( id );
		return 1;
	}

	sbo_mirror_t m;
	m.id = id;
	m.drawPlayers = ( flags & MIRROR_FLAG_PLAYERS ) ? 1 : 0;
	m.axis = READ_BYTE();
	m.side = READ_CHAR();
	for ( int i = 0; i < 3; i++ )
		m.mins[i] = READ_COORD();
	for ( int i = 0; i < 3; i++ )
		m.maxs[i] = READ_COORD();
	for ( int i = 0; i < 3; i++ )
		m.center[i] = ( m.mins[i] + m.maxs[i] ) * 0.5f;
	m.plane = READ_COORD();
	m.radius = (float)READ_SHORT();

	if ( m.axis < 0 || m.axis > 2 )
		m.axis = 0;
	m.side = ( m.side > 0 ) ? 1 : ( ( m.side < 0 ) ? -1 : 0 );

	// the same mirror again (respawn, fullupdate): update it
	for ( int i = 0; i < s_iMirrors; i++ )
	{
		if ( s_Mirrors[i].id == id )
		{
			s_Mirrors[i] = m;
			return 1;
		}
	}

	if ( s_iMirrors >= MAX_MIRRORS )
	{
		gEngfuncs.Con_Printf( "env_mirror: more than %d active mirrors, entity %d ignored\n", MAX_MIRRORS, id );
		return 1;
	}

	s_Mirrors[s_iMirrors++] = m;
	return 1;
}

void Mirror_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "Mirror", MsgFunc_Mirror );
}

void Mirror_Reset( void )
{
	s_iMirrors = 0;
	s_iPlayerCopyIndex = 0;
	s_fLocalView = 0;
	s_iSpriteEnts = 0;
}

int Mirror_Count( void )
{
	return s_iMirrors;
}

const sbo_mirror_t *Mirror_Get( int i )
{
	return ( i >= 0 && i < s_iMirrors ) ? &s_Mirrors[i] : NULL;
}

bool Mirror_Reflects( const sbo_mirror_t *pMirror, const float *origin, const float *vieworg )
{
	float dx = origin[0] - pMirror->center[0];
	float dy = origin[1] - pMirror->center[1];
	float dz = origin[2] - pMirror->center[2];

	if ( dx * dx + dy * dy + dz * dz > pMirror->radius * pMirror->radius )
		return false;

	// the view's side; on the plane itself nothing is seen in the mirror
	float flView = vieworg[pMirror->axis] - pMirror->plane;
	if ( fabs( flView ) < 1.0f )
		return false;

	int iViewSide = ( flView > 0 ) ? 1 : -1;
	if ( pMirror->side && iViewSide != pMirror->side )
		return false;

	// the model must be on the view's side (a unit of slack: a monster's origin is at its feet, on a shiny floor's
	// face); a model behind the mirror would come out as a ghost in front of it
	if ( ( origin[pMirror->axis] - pMirror->plane ) * iViewSide < -1.0f )
		return false;

	// seen through the mirror: the line from the view to the reflected origin crosses the plane within the brush
	// (and a margin for the model's size); else the reflection would only be drawn behind a wall or a floor
	const int a = pMirror->axis;
	const float reflected = 2 * pMirror->plane - origin[a];
	const float denom = reflected - vieworg[a];
	if ( fabs( denom ) > 0.001f )
	{
		const float t = ( pMirror->plane - vieworg[a] ) / denom;
		for ( int i = 0; i < 3; i++ )
		{
			if ( i == a )
				continue;
			const float p = vieworg[i] + ( origin[i] - vieworg[i] ) * t;
			if ( p < pMirror->mins[i] - MIRROR_EDGE_MARGIN || p > pMirror->maxs[i] + MIRROR_EDGE_MARGIN )
				return false;
		}
	}

	return true;
}

static void Mirror_CreateSpriteCopies( void );

void Mirror_CreateEntities( void )
{
	s_iPlayerCopyIndex = 0;

	if ( !s_iMirrors )
	{
		s_iSpriteEnts = 0;
		return;
	}

	Mirror_CreateSpriteCopies();

	// third person (or a spectator's chase view of himself): the engine draws the player, his reflections with him
	if ( CL_IsThirdPerson() )
		return;

	cl_entity_t *pLocal = gEngfuncs.GetLocalPlayer();
	if ( !pLocal )
		return;

	// the player whose eyes the view is: ourselves, or the player watched in eye; other spectator modes see no
	// player through his own eyes (the engine draws the watched player, reflections included)
	int iIndex;
	if ( g_iUser1 )
	{
		if ( g_iUser1 != OBS_IN_EYE || g_iUser2 < 1 || g_iUser2 > gEngfuncs.GetMaxClients() )
			return;
		iIndex = g_iUser2;
	}
	else
	{
		iIndex = pLocal->index;
	}

	cl_entity_t *pPlayer = gEngfuncs.GetEntityByIndex( iIndex );
	if ( !pPlayer || !pPlayer->model || !pPlayer->player )
		return;

	// not in our update (out of the PVS), or not drawn (dead and gone, spectating)
	if ( pPlayer != pLocal && pPlayer->curstate.messagenum < pLocal->curstate.messagenum )
		return;
	if ( pPlayer->curstate.effects & EF_NODRAW )
		return;

	// only near a mirror that shows him (the renderer checks every mirror again with the exact view origin)
	int i;
	for ( i = 0; i < s_iMirrors; i++ )
	{
		if ( s_Mirrors[i].drawPlayers && Mirror_Reflects( &s_Mirrors[i], pPlayer->origin, pPlayer->origin ) )
			break;
	}
	if ( i == s_iMirrors )
		return;

	// a plain entity for the engine (index 0, not a player): it hands it to StudioDrawModel, which recognises it
	// (Mirror_PlayerCopyIndex) and draws the player's reflections from his player state
	s_PlayerCopy = *pPlayer;
	s_PlayerCopy.index = 0;
	s_PlayerCopy.player = 0;

	gEngfuncs.CL_CreateVisibleEntity( ET_NORMAL, &s_PlayerCopy );
	s_iPlayerCopyIndex = iIndex;
	s_fPlayerCopyLocal = ( pPlayer == pLocal );
}

void Mirror_SetLocalView( const float *origin, const float *viewangles )
{
	for ( int i = 0; i < 3; i++ )
	{
		s_vecLocalOrigin[i] = origin[i];
		s_vecLocalAngles[i] = viewangles[i];
	}
	s_fLocalView = 1;
}

int Mirror_PlayerCopyPredicted( float *origin, float *angles )
{
	if ( !s_iPlayerCopyIndex || !s_fPlayerCopyLocal || !s_fLocalView )
		return 0;

	float pitch = s_vecLocalAngles[0];
	if ( pitch > 180 )
		pitch -= 360;
	else if ( pitch < -180 )
		pitch += 360;

	for ( int i = 0; i < 3; i++ )
		origin[i] = s_vecLocalOrigin[i];
	angles[0] = pitch / -3.0f;	// as view.cpp gives the local player model in third person
	angles[1] = s_vecLocalAngles[1];
	angles[2] = 0;
	return 1;
}

int Mirror_PlayerCopyIndex( const struct cl_entity_s *ent )
{
	return ( s_iPlayerCopyIndex && ent == &s_PlayerCopy ) ? s_iPlayerCopyIndex : 0;
}

//=========================================================
// Reflections of what is not a studio model (studio models, temp entity gibs and shells included, are reflected
// by StudioModelRenderer.cpp)
//=========================================================

void Mirror_ReflectPoint( const sbo_mirror_t *pMirror, const float *in, float *out )
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	out[pMirror->axis] = 2 * pMirror->plane - in[pMirror->axis];
}

// The angles of an entity whose orientation is mirrored: its forward and up vectors reflected, the angles made
// from them again (a rotation cannot mirror, so an oriented sprite's image keeps its handedness)
static void Mirror_ReflectAngles( const sbo_mirror_t *pMirror, const float *in, float *out )
{
	vec3_t forward, right, up;
	gEngfuncs.pfnAngleVectors( in, forward, right, up );

	forward[pMirror->axis] = -forward[pMirror->axis];
	up[pMirror->axis] = -up[pMirror->axis];

	// right = forward x up (AngleVectors' convention)
	vec3_t r;
	r[0] = forward[1] * up[2] - forward[2] * up[1];
	r[1] = forward[2] * up[0] - forward[0] * up[2];
	r[2] = forward[0] * up[1] - forward[1] * up[0];

	float fz = forward[2] < -1 ? -1 : ( forward[2] > 1 ? 1 : forward[2] );
	out[0] = (float)( asin( -fz ) * 180.0 / M_PI );
	out[1] = (float)( atan2( forward[1], forward[0] ) * 180.0 / M_PI );
	out[2] = (float)( atan2( -r[2], up[2] ) * 180.0 / M_PI );
}

// A copy of a non-studio entity at its reflection, drawn by the engine like the original
static void Mirror_MakeCopy( cl_entity_t *pCopy, const cl_entity_t *pEnt, const sbo_mirror_t *pMirror )
{
	*pCopy = *pEnt;
	pCopy->index = 0;
	pCopy->curstate.aiment = 0;		// not attached: the engine would put it back on the original's attachment
	pCopy->curstate.movetype = MOVETYPE_NONE;

	Mirror_ReflectPoint( pMirror, pEnt->origin, pCopy->origin );
	VectorCopy( pCopy->origin, pCopy->curstate.origin );
	Mirror_ReflectAngles( pMirror, pEnt->angles, pCopy->angles );
	VectorCopy( pCopy->angles, pCopy->curstate.angles );
}

static bool Mirror_IsSprite( const cl_entity_t *pEnt )
{
	return pEnt && pEnt->model && pEnt->model->type == mod_sprite;
}

void Mirror_AddEntity( int type, struct cl_entity_s *ent )
{
	if ( !s_iMirrors || type == ET_BEAM || !Mirror_IsSprite( ent ) )
		return;
	if ( ent->curstate.effects & EF_NODRAW )
		return;

	// near a mirror (the sides are checked when the copy is made)
	int i;
	for ( i = 0; i < s_iMirrors; i++ )
	{
		const sbo_mirror_t *m = &s_Mirrors[i];
		float dx = ent->origin[0] - m->center[0], dy = ent->origin[1] - m->center[1], dz = ent->origin[2] - m->center[2];
		if ( dx * dx + dy * dy + dz * dz <= m->radius * m->radius )
			break;
	}
	if ( i == s_iMirrors )
		return;

	if ( s_iSpriteEnts < MIRROR_MAX_SPRITE_CANDIDATES )
		s_pSpriteEnts[s_iSpriteEnts++] = ent;
}

static void Mirror_CreateSpriteCopies( void )
{
	int n = 0;

	for ( int e = 0; e < s_iSpriteEnts; e++ )
	{
		cl_entity_t *pEnt = s_pSpriteEnts[e];

		for ( int i = 0; i < s_iMirrors && n < MIRROR_MAX_SPRITE_COPIES; i++ )
		{
			if ( !Mirror_Reflects( &s_Mirrors[i], pEnt->origin, v_origin ) )
				continue;

			Mirror_MakeCopy( &s_SpriteCopies[n], pEnt, &s_Mirrors[i] );
			gEngfuncs.CL_CreateVisibleEntity( ET_NORMAL, &s_SpriteCopies[n] );
			n++;
		}
	}

	s_iSpriteEnts = 0;	// noted again by the next frame's HUD_AddEntity calls
}

void Mirror_TempEntities( struct tempent_s *pActive, double client_time )
{
	if ( !s_iMirrors )
		return;

	int n = 0;

	for ( TEMPENTITY *pTemp = pActive; pTemp && n < MIRROR_MAX_TEMPENT_COPIES; pTemp = pTemp->next )
	{
		if ( pTemp->flags & FTENT_NOMODEL )
			continue;
		if ( pTemp->die < client_time )
			continue;
		if ( !Mirror_IsSprite( &pTemp->entity ) )
			continue;	// studio models: reflected by the studio renderer

		for ( int i = 0; i < s_iMirrors && n < MIRROR_MAX_TEMPENT_COPIES; i++ )
		{
			if ( !Mirror_Reflects( &s_Mirrors[i], pTemp->entity.origin, v_origin ) )
				continue;

			Mirror_MakeCopy( &s_TempEntCopies[n], &pTemp->entity, &s_Mirrors[i] );
			gEngfuncs.CL_CreateVisibleEntity( ET_TEMPENTITY, &s_TempEntCopies[n] );
			n++;
		}
	}
}

//=========================================================
// Effects the client draws itself with the triangle API (SBO's flame and smoke emitters, particleman: the weather;
// Aurora's env_particle when that branch is merged): the hook point. Usage, after drawing the effect's quads:
//
//   int list[MIRROR_EFFECT_MAX];
//   int n = Mirror_EffectMirrors( list );                // the mirrors the view being drawn looks into
//   for ( int k = 0; k < n; k++ )
//   {
//       const sbo_mirror_t *m = Mirror_Get( list[k] );
//       Mirror_EffectBegin( m );                          // clip plane at the mirror (if GL allows)
//       for each particle: if ( Mirror_Reflects( m, origin, Mirror_EffectViewOrigin() ) )   (radius, sides)
//           draw it again with every vertex through Mirror_ReflectPoint( m, v, v2 )
//       Mirror_EffectEnd();
//   }
//
// Reflecting the vertices (not only the center) mirrors the quad itself: the texture is mirror-inverted, as in a
// real mirror; face culling must be off for the copy (the triangle API's TRI_NONE), its winding is reversed.
//=========================================================

// the view being drawn (main view, spectator inset), taken when an effect asks for the mirrors
static float s_vecEffectOrigin[3], s_vecEffectForward[3], s_vecEffectRight[3], s_vecEffectUp[3];

const float *Mirror_EffectViewOrigin( void )
{
	return s_vecEffectOrigin;
}

int Mirror_EffectMirrors( int *list )
{
	int n = 0;

	if ( !s_iMirrors )
		return 0;

	IEngineStudio.GetViewInfo( s_vecEffectOrigin, s_vecEffectUp, s_vecEffectRight, s_vecEffectForward );

	for ( int i = 0; i < s_iMirrors && n < MIRROR_EFFECT_MAX; i++ )
	{
		const sbo_mirror_t *m = &s_Mirrors[i];
		float flView = s_vecEffectOrigin[m->axis] - m->plane;

		if ( fabs( flView ) < 1.0f )
			continue;
		if ( m->side && ( flView > 0 ? 1 : -1 ) != m->side )
			continue;

		list[n++] = i;
	}

	return n;
}

static int s_fEffectClip = 0;

void Mirror_EffectBegin( const sbo_mirror_t *pMirror )
{
	s_fEffectClip = MirrorGL_Begin() ? 1 : 0;
	if ( s_fEffectClip )
		MirrorGL_SetPlane( pMirror->axis, pMirror->plane, ( s_vecEffectOrigin[pMirror->axis] > pMirror->plane ) ? -1 : 1,
			s_vecEffectOrigin, s_vecEffectForward, s_vecEffectRight, s_vecEffectUp );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
}

void Mirror_EffectEnd( void )
{
	if ( s_fEffectClip )
		MirrorGL_End();
	s_fEffectClip = 0;
}
