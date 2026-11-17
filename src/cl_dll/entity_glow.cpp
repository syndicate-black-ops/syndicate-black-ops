//=========================================================
// SBO 2.1 env_entity_glow, client side (entity_glow.h; docs/ENV_ENTITY_GLOW.md). Source SDK 2013's
// C_EnvEntityGlow / CGlowObjectManager (TF2's glows): each glow names its targets, a color, a mode and a style.
//
// What is drawn, and where from:
//  - studio models: the engine draws them through the client's studio renderer; for a glow target the renderer hands
//    over its header, the bone transforms of that draw and the submodels of its body parts (EntityGlow_Capture*), so
//    the glow has the model's exact pose, animation and scale. A model the engine did not draw in this view (out of
//    the view, out of the update) has no glow in it.
//  - brush entities: their faces from the map's .bsp file (entity_glow_bsp.cpp), read once per brush model, placed
//    with the entity's origin and angles as the engine draws it.
// The glow pass runs once per view, after the view's world and models (tri.cpp HUD_DrawTransparentTriangles): not
// in the sky room of a 3D sky, not on the spectator's overview map. The spectator's camera inset is a view of its
// own, with its own models.
//
// Cost: nothing while no glow is on. For a glow that is on, per view: one byte looked up per model drawn, the bone
// transforms of each target drawn copied, one depth copy, the targets drawn once more (twice for "Only when
// occluded") and, with a halo, two blurs at a quarter of the size. Mode 2 (Only when visible) targets far behind a
// wall are left out by two world traces.
//=========================================================

// the standard headers first: cl_util.h defines min / max macros
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <deque>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "com_model.h"
#include "studio.h"
#include "parsemsg.h"
#include "event_api.h"
#include "pmtrace.h"
#include "pm_defs.h"
#include "r_studioint.h"
#include "entity_glow.h"
#include "entity_glow_gl.h"
#include "entity_glow_bsp.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern engine_studio_api_t IEngineStudio;	// the view being drawn (GetViewInfo)

#define ENTGLOW_MAX_GLOWS		64		// glows that are on at once (more: ignored, with a developer line)
#define ENTGLOW_MAX_TARGETS		64		// a glow's targets (the server's limit: 32, 64 for !all)
#define ENTGLOW_MAX_CAPTURES	96		// glow target models kept per view
#define ENTGLOW_MAX_INDEX		2048	// entity indexes (MAX_EDICTS)
#define ENTGLOW_MAX_BRUSH_FACES	4096	// faces of one brush model drawn
#define ENTGLOW_WALL_SLACK		256.0f	// Mode 2: a target this far behind the wall the view's ray hits is left out

#define ENTGLOW_MIN_RENDERAMT	40		// a model or brush drawn fainter than this (FX amount) gets no glow

#define ENTGLOW_FLAG_ON			0x01	// "EntGlow" flags (server: dlls/env_entity_glow.cpp)

// GL primitives (entity_glow_gl.h)
#define EG_TRIANGLE_STRIP	5
#define EG_TRIANGLE_FAN		6
#define EG_POLYGON			9

struct entglow_t
{
	int id;					// the env_entity_glow's index
	int mode;				// ENTGLOW_GL_ALWAYS / OCCLUDED / VISIBLE
	int style;				// ENTGLOW_GL_OUTLINE / HALO
	float color[4];			// 0..1
	int count;
	int targets[ ENTGLOW_MAX_TARGETS ];
};

static entglow_t s_Glows[ ENTGLOW_MAX_GLOWS ];
static int s_iGlows = 0;
static unsigned char s_TargetRefs[ ENTGLOW_MAX_INDEX ];	// how many glows name the entity

// a glow target's model as drawn in this view
struct entglow_capture_t
{
	int index;
	studiohdr_t *header;
	int numbones;
	int numsub;
	mstudiomodel_t *sub[ MAXSTUDIOBODYPARTS ];
	float bones[ MAXSTUDIOBONES ][3][4];
	bool xformed;									// xyz filled in this view (once, for every glow and pass)
	std::vector<float> xyz[ MAXSTUDIOBODYPARTS ];	// each submodel's vertexes in world coordinates
};

// a submodel's triangles as one index list (its triangle strips and fans unrolled), made once per model; checked
// against the model's name, size and vertex count before use (the engine's model cache can reuse an address)
struct entglow_tris_t
{
	const mstudiomodel_t *sub;
	char name[64];
	int length;
	int numverts;
	std::vector<unsigned short> indices;
};
static std::vector<entglow_tris_t> s_Tris;

static entglow_capture_t s_Captures[ ENTGLOW_MAX_CAPTURES ];
static int s_iCaptures = 0;

// faces of the brush models glowed on this map ("*N"), read from the .bsp file once each
struct entglow_brush_t
{
	int submodel;
	bool loaded;
	std::vector<float> xyz;
	std::vector<int> counts;
};
static std::deque<entglow_brush_t> s_Brushes;	// a deque: the entries stay where they are as it grows
static char s_szBrushLevel[ 128 ] = "";


// the view being drawn (IEngineStudio.GetViewInfo: the engine's r_origin, vpn), set by EntityGlow_Draw
static vec3_t s_vecViewOrg, s_vecViewForward;

// drawn nearly invisible (a cloaked Assassin, an invisible brush) or not drawn at all
static bool EntityGlow_Hidden( cl_entity_t *ent )
{
	if ( ent->curstate.effects & EF_NODRAW )
		return true;
	return ent->curstate.rendermode != kRenderNormal && ent->curstate.renderamt < ENTGLOW_MIN_RENDERAMT;
}

static void EntityGlow_RebuildRefs( void )
{
	memset( s_TargetRefs, 0, sizeof( s_TargetRefs ) );
	for ( int i = 0; i < s_iGlows; i++ )
	{
		for ( int t = 0; t < s_Glows[i].count; t++ )
		{
			const int index = s_Glows[i].targets[t];
			if ( s_TargetRefs[ index ] < 255 )
				s_TargetRefs[ index ]++;
		}
	}
}

static void EntityGlow_Remove( int id )
{
	for ( int i = 0; i < s_iGlows; i++ )
	{
		if ( s_Glows[i].id == id )
		{
			s_Glows[i] = s_Glows[ --s_iGlows ];
			return;
		}
	}
}

// short id, byte flags; ON: byte mode, byte style, byte r g b a, byte count, count x short entity index
static int MsgFunc_EntGlow( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	const int id = READ_SHORT();
	const int flags = READ_BYTE();

	if ( !( flags & ENTGLOW_FLAG_ON ) )
	{
		EntityGlow_Remove( id );
		EntityGlow_RebuildRefs();
		return 1;
	}

	entglow_t g;
	g.id = id;
	g.mode = READ_BYTE();
	g.style = READ_BYTE();
	for ( int i = 0; i < 4; i++ )
		g.color[i] = READ_BYTE() / 255.0f;
	int count = READ_BYTE();
	if ( count > ENTGLOW_MAX_TARGETS )
		count = ENTGLOW_MAX_TARGETS;
	g.count = 0;
	for ( int i = 0; i < count; i++ )
	{
		const int index = READ_SHORT();
		if ( index > 0 && index < ENTGLOW_MAX_INDEX )
			g.targets[ g.count++ ] = index;
	}
	if ( g.mode < ENTGLOW_GL_ALWAYS || g.mode > ENTGLOW_GL_VISIBLE )
		g.mode = ENTGLOW_GL_ALWAYS;
	if ( g.style != ENTGLOW_GL_HALO )
		g.style = ENTGLOW_GL_OUTLINE;

	// the same glow again (a change, respawn, fullupdate): update it
	int slot = -1;
	for ( int i = 0; i < s_iGlows; i++ )
	{
		if ( s_Glows[i].id == id )
			slot = i;
	}
	if ( slot < 0 )
	{
		if ( s_iGlows >= ENTGLOW_MAX_GLOWS )
		{
			gEngfuncs.Con_DPrintf( "env_entity_glow: more than %d glows on, entity %d ignored\n", ENTGLOW_MAX_GLOWS, id );
			return 1;
		}
		slot = s_iGlows++;
	}
	s_Glows[ slot ] = g;
	EntityGlow_RebuildRefs();
	return 1;
}

void EntityGlow_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "EntGlow", MsgFunc_EntGlow );
}

void EntityGlow_Reset( void )
{
	s_iGlows = 0;
	memset( s_TargetRefs, 0, sizeof( s_TargetRefs ) );
	s_iCaptures = 0;
	s_Tris.clear();
}

int EntityGlow_Capture( struct cl_entity_s *pEntity, void *pHeader, const float (*pBones)[3][4] )
{
	if ( !s_iGlows || !pEntity || !pHeader || !pBones )
		return 0;
	int index = pEntity->index;
	if ( index <= 0 || index >= ENTGLOW_MAX_INDEX )
		return 0;
	// an entity that follows a target (a player's attached model, packed_models' entity) glows as part of it
	if ( !s_TargetRefs[ index ] && pEntity->curstate.movetype == MOVETYPE_FOLLOW && pEntity->curstate.aiment > 0
		&& pEntity->curstate.aiment < ENTGLOW_MAX_INDEX && s_TargetRefs[ pEntity->curstate.aiment ] )
		index = pEntity->curstate.aiment;
	if ( !s_TargetRefs[ index ] || pEntity == gEngfuncs.GetViewModel() )
		return 0;
	// the glow never shows what the game hides: a cloaked or nearly invisible model has none
	if ( EntityGlow_Hidden( pEntity ) )
		return 0;

	studiohdr_t *pHdr = (studiohdr_t *)pHeader;

	// drawn twice in a row (fullbright textures, the glow shell): once is enough
	for ( int i = 0; i < s_iCaptures; i++ )
	{
		if ( s_Captures[i].index == index && s_Captures[i].header == pHdr )
			return 0;
	}
	if ( s_iCaptures >= ENTGLOW_MAX_CAPTURES )
		return 0;

	entglow_capture_t &c = s_Captures[ s_iCaptures++ ];
	c.index = index;
	c.header = pHdr;
	c.numbones = pHdr->numbones;
	if ( c.numbones < 0 )
		c.numbones = 0;
	if ( c.numbones > MAXSTUDIOBONES )
		c.numbones = MAXSTUDIOBONES;
	memcpy( c.bones, pBones, sizeof( float ) * 12 * c.numbones );

	// the submodel of each body part, as the engine's StudioSetupModel picks it from the entity's body value (the
	// renderer's m_pSubModel is not a pointer into this header on HL25)
	c.numsub = 0;
	c.xformed = false;
	const byte *base = (const byte *)pHdr;
	const mstudiobodyparts_t *parts = (const mstudiobodyparts_t *)( base + pHdr->bodypartindex );
	const int body = pEntity->curstate.body;
	for ( int i = 0; i < pHdr->numbodyparts && c.numsub < MAXSTUDIOBODYPARTS; i++ )
	{
		if ( parts[i].nummodels <= 0 || parts[i].base <= 0 )
			continue;
		const int index = ( body / parts[i].base ) % parts[i].nummodels;
		c.sub[ c.numsub++ ] = (mstudiomodel_t *)( base + parts[i].modelindex ) + index;
	}
	return 1;
}

void EntityGlow_SkipView( void )
{
	s_iCaptures = 0;
}

// --- drawing ---

static const entglow_tris_t *EntityGlow_Tris( const studiohdr_t *hdr, const mstudiomodel_t *sub )
{
	for ( size_t i = 0; i < s_Tris.size(); i++ )
	{
		const entglow_tris_t &t = s_Tris[i];
		if ( t.sub == sub && t.length == hdr->length && t.numverts == sub->numverts && !strncmp( t.name, hdr->name, 64 ) )
			return &t;
	}
	// the slot of a model this address no longer holds is made again
	for ( size_t i = 0; i < s_Tris.size(); i++ )
	{
		if ( s_Tris[i].sub == sub )
		{
			s_Tris.erase( s_Tris.begin() + i );
			break;
		}
	}

	entglow_tris_t t;
	t.sub = sub;
	memcpy( t.name, hdr->name, 64 );
	t.length = hdr->length;
	t.numverts = sub->numverts;
	const byte *base = (const byte *)hdr;
	const mstudiomesh_t *meshes = (const mstudiomesh_t *)( base + sub->meshindex );
	for ( int m = 0; m < sub->nummesh; m++ )
	{
		// the triangle commands: a count (> 0 a strip, < 0 a fan), then per corner vertex, normal, s, t; 0 ends
		const short *cmd = (const short *)( base + meshes[m].triindex );
		int n;
		while ( ( n = *( cmd++ ) ) != 0 )
		{
			const bool fan = n < 0;
			if ( fan )
				n = -n;
			int first = -1, prev2 = -1, prev1 = -1;
			for ( int k = 0; k < n; k++, cmd += 4 )
			{
				int v = cmd[0];
				if ( v < 0 || v >= sub->numverts )
					v = 0;
				// a fan: first, previous, this; a strip: the two before, this (the winding is not needed here)
				if ( k == 0 )
					first = v;
				if ( k >= 2 )
				{
					t.indices.push_back( (unsigned short)( fan ? first : prev2 ) );
					t.indices.push_back( (unsigned short)prev1 );
					t.indices.push_back( (unsigned short)v );
				}
				prev2 = prev1;
				prev1 = v;
			}
		}
	}
	s_Tris.push_back( t );
	return &s_Tris.back();
}

static void EntityGlow_DrawCapture( entglow_capture_t &c )
{
	const byte *base = (const byte *)c.header;
	for ( int s = 0; s < c.numsub; s++ )
	{
		const mstudiomodel_t *sub = c.sub[s];
		const int numverts = sub->numverts;
		if ( numverts <= 0 || numverts > MAXSTUDIOVERTS )
			continue;

		std::vector<float> &xyz = c.xyz[s];
		if ( !c.xformed )
		{
			xyz.resize( numverts * 3 );
			const vec3_t *verts = (const vec3_t *)( base + sub->vertindex );
			const byte *vertbone = base + sub->vertinfoindex;
			for ( int v = 0; v < numverts; v++ )
			{
				int bone = vertbone[v];
				if ( bone >= c.numbones )
					bone = 0;
				const float (*m)[4] = c.bones[ bone ];
				const float *p = verts[v];
				xyz[v * 3 + 0] = p[0] * m[0][0] + p[1] * m[0][1] + p[2] * m[0][2] + m[0][3];
				xyz[v * 3 + 1] = p[0] * m[1][0] + p[1] * m[1][1] + p[2] * m[1][2] + m[1][3];
				xyz[v * 3 + 2] = p[0] * m[2][0] + p[1] * m[2][1] + p[2] * m[2][2] + m[2][3];
			}
		}
		if ( (int)xyz.size() != numverts * 3 )
			continue;

		const entglow_tris_t *t = EntityGlow_Tris( c.header, sub );
		if ( t && !t->indices.empty() )
			EntityGlowGL_DrawIndexed( xyz.data(), numverts, t->indices.data(), (int)t->indices.size() );
	}
	c.xformed = true;
}

static entglow_brush_t *EntityGlow_Brush( int submodel )
{
	const char *level = gEngfuncs.pfnGetLevelName ? gEngfuncs.pfnGetLevelName() : NULL;
	if ( !level || !level[0] )
		return NULL;
	if ( strcmp( level, s_szBrushLevel ) )
	{
		s_Brushes.clear();
		strncpy( s_szBrushLevel, level, sizeof( s_szBrushLevel ) - 1 );
		s_szBrushLevel[ sizeof( s_szBrushLevel ) - 1 ] = 0;
	}
	for ( size_t i = 0; i < s_Brushes.size(); i++ )
	{
		if ( s_Brushes[i].submodel == submodel )
			return s_Brushes[i].loaded ? &s_Brushes[i] : NULL;
	}

	entglow_brush_t b;
	b.submodel = submodel;
	b.loaded = false;
	int size = 0;
	unsigned char *data = gEngfuncs.COM_LoadFile( (char *)level, 5, &size );
	if ( data )
	{
		b.loaded = EntityGlowBsp_ModelFaces( data, size, submodel, ENTGLOW_MAX_BRUSH_FACES, b.xyz, b.counts );
		gEngfuncs.COM_FreeFile( data );
	}
	if ( !b.loaded )
		gEngfuncs.Con_DPrintf( "env_entity_glow: can't read the faces of brush model *%d from %s\n", submodel, level );
	s_Brushes.push_back( b );
	return s_Brushes.back().loaded ? &s_Brushes.back() : NULL;
}

// as the engine places a brush model (R_RotateForEntity with Quake's pitch flip): yaw about Z, pitch about Y, roll
// about X, then the origin
static void EntityGlow_DrawBrush( cl_entity_t *ent, const entglow_brush_t &b )
{
	float m[3][3];
	const float yaw = ent->angles[1] * ( M_PI / 180.0f ), pitch = ent->angles[0] * ( M_PI / 180.0f ),
		roll = ent->angles[2] * ( M_PI / 180.0f );
	const float cy = cosf( yaw ), sy = sinf( yaw ), cp = cosf( pitch ), sp = sinf( pitch ), cr = cosf( roll ),
		sr = sinf( roll );
	// Rz(yaw) * Ry(pitch) * Rx(roll)
	m[0][0] = cy * cp;	m[0][1] = cy * sp * sr - sy * cr;	m[0][2] = cy * sp * cr + sy * sr;
	m[1][0] = sy * cp;	m[1][1] = sy * sp * sr + cy * cr;	m[1][2] = sy * sp * cr - cy * sr;
	m[2][0] = -sp;		m[2][1] = cp * sr;					m[2][2] = cp * cr;

	const float *p = b.xyz.data();
	for ( size_t f = 0; f < b.counts.size(); f++ )
	{
		EntityGlowGL_BeginPrimitive( EG_POLYGON );
		for ( int v = 0; v < b.counts[f]; v++, p += 3 )
		{
			float w[3];
			for ( int k = 0; k < 3; k++ )
				w[k] = m[k][0] * p[0] + m[k][1] * p[1] + m[k][2] * p[2] + ent->origin[k];
			EntityGlowGL_Vertex( w );
		}
		EntityGlowGL_EndPrimitive();
	}
}

// the entity is in this frame's update (not left over from an earlier one)
static bool EntityGlow_InUpdate( cl_entity_t *ent )
{
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	return ent && player && ent->curstate.messagenum == player->curstate.messagenum;
}

// Mode 2: a target far behind the wall the view's rays to its middle and to its top hit is not seen; left out
static bool EntityGlow_BehindWall( const float *mins, const float *maxs )
{
	float center[3], top[3];
	for ( int i = 0; i < 3; i++ )
		center[i] = top[i] = ( mins[i] + maxs[i] ) * 0.5f;
	top[2] = maxs[2] - 1.0f;

	const float *points[2] = { center, top };
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	for ( int i = 0; i < 2; i++ )
	{
		pmtrace_t tr;
		gEngfuncs.pEventAPI->EV_PlayerTrace( s_vecViewOrg, (float *)points[i], PM_WORLD_ONLY, -1, &tr );
		if ( tr.fraction >= 1.0f || tr.startsolid )
			return false;
		float d = 0.0f;
		for ( int k = 0; k < 3; k++ )
			d += ( points[i][k] - tr.endpos[k] ) * ( points[i][k] - tr.endpos[k] );
		if ( d < ENTGLOW_WALL_SLACK * ENTGLOW_WALL_SLACK )
			return false;
	}
	return true;
}

// a box wholly behind the view
static bool EntityGlow_BehindView( const float *mins, const float *maxs )
{
	const float *forward = s_vecViewForward;
	const float *eye = s_vecViewOrg;
	for ( int i = 0; i < 8; i++ )
	{
		const float p[3] = { ( i & 1 ) ? maxs[0] : mins[0], ( i & 2 ) ? maxs[1] : mins[1], ( i & 4 ) ? maxs[2] : mins[2] };
		if ( ( p[0] - eye[0] ) * forward[0] + ( p[1] - eye[1] ) * forward[1] + ( p[2] - eye[2] ) * forward[2] > 0.0f )
			return false;
	}
	return true;
}

// the target's box for the culls: the studio model's sent size around its origin, the brush model's box
static void EntityGlow_Box( cl_entity_t *ent, float *mins, float *maxs )
{
	for ( int i = 0; i < 3; i++ )
	{
		mins[i] = ent->origin[i] + ent->curstate.mins[i];
		maxs[i] = ent->origin[i] + ent->curstate.maxs[i];
		if ( maxs[i] - mins[i] < 2.0f )
		{
			mins[i] -= 16.0f;
			maxs[i] += 16.0f;
		}
	}
}

void EntityGlow_Draw( void )
{
	static int s_iLoggedGeneration = -1;
	static int s_iLoggedOk = -1;

	if ( !s_iGlows || !HUD_IsHardware() )
	{
		EntityGlow_SkipView();
		return;
	}

	vec3_t up, right;
	IEngineStudio.GetViewInfo( s_vecViewOrg, up, right, s_vecViewForward );

	// what this view has to draw: the captured models of the targets, the targets that are brush models
	struct job_t { const entglow_t *glow; int capture; cl_entity_t *brush; const entglow_brush_t *faces; };
	job_t jobs[ 256 ];
	int iJobs = 0;
	const int iMaxJobs = (int)( sizeof( jobs ) / sizeof( jobs[0] ) );

	for ( int g = 0; g < s_iGlows && iJobs < iMaxJobs; g++ )
	{
		const entglow_t &glow = s_Glows[g];
		if ( glow.color[3] <= 0.0f || ( glow.color[0] + glow.color[1] + glow.color[2] ) <= 0.0f )
			continue;
		for ( int t = 0; t < glow.count && iJobs < iMaxJobs; t++ )
		{
			cl_entity_t *ent = gEngfuncs.GetEntityByIndex( glow.targets[t] );
			if ( !ent )
				continue;
			const bool bUpdated = ent->model && EntityGlow_InUpdate( ent );

			float mins[3], maxs[3];
			if ( bUpdated )
			{
				EntityGlow_Box( ent, mins, maxs );
				if ( EntityGlow_BehindView( mins, maxs ) )
					continue;
				if ( glow.mode == ENTGLOW_GL_VISIBLE && EntityGlow_BehindWall( mins, maxs ) )
					continue;
			}

			if ( bUpdated && ent->model->name[0] == '*' )
			{
				if ( EntityGlow_Hidden( ent ) )
					continue;
				const entglow_brush_t *faces = EntityGlow_Brush( atoi( ent->model->name + 1 ) );
				if ( faces && !faces->counts.empty() )
				{
					job_t j = { &glow, -1, ent, faces };
					jobs[ iJobs++ ] = j;
				}
				continue;
			}

			// studio models: what the engine drew of it in this view (also its weapon model and the models that
			// follow it, even when the target itself is not drawn)
			for ( int c = 0; c < s_iCaptures && iJobs < iMaxJobs; c++ )
			{
				if ( s_Captures[c].index == glow.targets[t] )
				{
					job_t j = { &glow, c, NULL, NULL };
					jobs[ iJobs++ ] = j;
				}
			}
		}
	}

	if ( !iJobs )
	{
		EntityGlow_SkipView();
		return;
	}

	const bool bOk = EntityGlowGL_Begin();
	if ( ( bOk ? 1 : 0 ) != s_iLoggedOk || EntityGlowGL_Generation() != s_iLoggedGeneration )
	{
		s_iLoggedOk = bOk ? 1 : 0;
		s_iLoggedGeneration = EntityGlowGL_Generation();
		if ( bOk )
			gEngfuncs.Con_DPrintf( "env_entity_glow: outline and halo pass (GLSL)\n" );
		else
			gEngfuncs.Con_DPrintf( "env_entity_glow: no glow drawn: %s\n", EntityGlowGL_Why() );
	}
	if ( !bOk )
	{
		EntityGlow_SkipView();
		return;
	}

	for ( int i = 0; i < iJobs; i++ )
	{
		const job_t &j = jobs[i];
		const int passes = ( j.glow->mode == ENTGLOW_GL_OCCLUDED ) ? 2 : 1;
		for ( int pass = 0; pass < passes; pass++ )
		{
			EntityGlowGL_Target( j.glow->style, j.glow->color, j.glow->mode, pass );
			if ( j.brush )
				EntityGlow_DrawBrush( j.brush, *j.faces );
			else
				EntityGlow_DrawCapture( s_Captures[ j.capture ] );
		}
	}

	EntityGlowGL_End();
	const unsigned int uError = EntityGlowGL_TakeError();
	if ( uError )
		gEngfuncs.Con_Printf( "env_entity_glow: GL error 0x%04X in the glow pass\n", uError );
	EntityGlow_SkipView();
}
