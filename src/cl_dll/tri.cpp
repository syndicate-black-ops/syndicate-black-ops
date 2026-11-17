//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "particleman.h"
#include "environment.h"
#include "aurora/particlemgr.h"
#include "worldtext.h"
#include "worldscreen/worldscreen.h"
#include "spray_anim.h"
#include "colorcorrection.h"	// SBO 2.0: color_correction
#include "fogzone_gl.h"	// SBO 2.0: env_fogzone per-pixel fog volume
#include "entity_glow.h"	// SBO 2.1: env_entity_glow
#include "waypoint.h"	// SBO 2.1: info_waypoint
#include "view.h"	// SoHL env_sky: EnvSky_ShowEntities
#include "env_sky_box.h"

#define DLLEXPORT __declspec( dllexport )

extern "C"
{
	void DLLEXPORT HUD_DrawNormalTriangles( void );
	void DLLEXPORT HUD_DrawTransparentTriangles( void );
};

extern vec3_t v_origin;	// view.cpp
extern vec3_t v_angles;	// view.cpp
extern float g_flGravity;	// particleman/IParticleMan_Active.cpp

FogParameters g_FogParameters;	// cs16-client tri.cpp: env_fog (Fog message, hud_msg.cpp). SBO 2.0: the fog on screen

// SBO 2.0: the server sends a new fog when the player enters or leaves an env_fogzone (and for the watched player of a
// spectator). It fades in from the fog on screen instead of replacing it at once.
#define FOG_FADE_TIME	1.0f	// seconds

static FogParameters g_FogTarget;		// the fog the server sent last
static FogParameters g_FogFadeFrom;		// the fog on screen when it arrived
static FogParameters g_FogFadeTo;		// g_FogTarget, or the old color without density when fading to no fog
static float g_flFogFadeStart;
static bool g_bFogFading = false;
static bool g_bFogSnap = true;			// no fade for the next Fog: nothing was drawn since InitHUD / ResetHUD

/*
=================
Fog_Reset

InitHUD (new server or map): no fog until the server's Fog, which applies at once.
=================
*/
void Fog_Reset( void )
{
	memset( &g_FogParameters, 0, sizeof(FogParameters) );
	g_FogTarget = g_FogParameters;
	g_bFogFading = false;
	g_bFogSnap = true;
}

/*
=================
Fog_Snap

ResetHUD: the server sends the fog with every HUD (re)initialisation (connect, respawn, StartObserver, start of a demo).
Fog messages that arrive from now until the next frame is drawn apply at once.
=================
*/
void Fog_Snap( void )
{
	g_bFogSnap = true;
}

/*
=================
Fog_Set

Fog message. The same fog again (every respawn sends the map's) changes nothing.
=================
*/
void Fog_Set( const FogParameters &fog )
{
	const bool bSame = ( fog.density == g_FogTarget.density && fog.color[0] == g_FogTarget.color[0]
		&& fog.color[1] == g_FogTarget.color[1] && fog.color[2] == g_FogTarget.color[2] );

	g_FogTarget = fog;

	if ( g_bFogSnap )
	{
		g_FogParameters = fog;
		g_bFogFading = false;
		return;
	}

	if ( bSame )
		return;		// on screen already, or fading to it

	// from what is on screen now, which is part of the way if a fade was running
	g_FogFadeFrom = g_FogParameters;
	g_FogFadeTo = fog;

	// to or from no fog: only the density fades, in the color of the fog that is there (no fog has no color)
	if ( g_FogFadeFrom.density <= 0.0f )
	{
		g_FogFadeFrom.density = 0.0f;
		VectorCopy( g_FogFadeTo.color, g_FogFadeFrom.color );
	}
	else if ( g_FogFadeTo.density <= 0.0f )
	{
		g_FogFadeTo.density = 0.0f;
		VectorCopy( g_FogFadeFrom.color, g_FogFadeTo.color );
	}

	g_flFogFadeStart = gEngfuncs.GetClientTime();
	g_bFogFading = true;
}

/*
=================
Fog_Update

Every frame, from RenderFog: moves g_FogParameters along a running fade.
=================
*/
static void Fog_Update( void )
{
	g_bFogSnap = false;		// this fog is on screen now: the next change fades

	if ( !g_bFogFading )
		return;

	const float flFrac = ( gEngfuncs.GetClientTime() - g_flFogFadeStart ) / FOG_FADE_TIME;

	// done (or the clock went back: new map, demo)
	if ( flFrac < 0.0f || flFrac >= 1.0f )
	{
		g_FogParameters = g_FogTarget;
		g_bFogFading = false;
		return;
	}

	for ( int i = 0; i < 3; i++ )
		g_FogParameters.color[i] = g_FogFadeFrom.color[i] + ( g_FogFadeTo.color[i] - g_FogFadeFrom.color[i] ) * flFrac;

	g_FogParameters.density = g_FogFadeFrom.density + ( g_FogFadeTo.density - g_FogFadeFrom.density ) * flFrac;
	g_FogParameters.affectsSkyBox = g_FogTarget.affectsSkyBox;
}

/*
=================
env_fogzone PROTOTYPE (docs/WEATHER.md "env_fogzone prototype")

The engine has one fog for the whole view, so a fog volume is drawn in two parts:
 - haze: the fog of the zone's box as a volume, per pixel (fogzone_gl.cpp): the part of the view ray inside the box
   and in front of what the view shows there (the view's depth), alpha 1 - exp( -( length * density )^2 ) (the
   engine's fog curve). No edges at the box's outline from any angle; what is inside the zone is fogged by what lies
   in front of it. Without GLSL or a usable depth copy (FogZoneGL_Why), the far faces of the box are drawn instead, as
   a grid of quads whose vertices get the alpha of the ray from the eye to them.
 - fog: with the eye inside, the engine's fog goes from the map's own to the zone's by how deep the eye is in the zone
   (distance to the nearest face one can walk through), and the haze fades out by the same amount.
The zone's box is the brush model's bounding box. Tuning constants:
=================
*/
#define FOGZONE_CLIENT_SIDE		1		// 0: off (dlls/player.cpp SendFog has the same switch for the old way)
#define FOGZONE_MAX				32
#define FOGZONE_GRID			8		// quads per face edge
#define FOGZONE_INSET			1.0f	// the far-face haze sits this far inside the box (walls at the box's faces)
#define FOGZONE_FULL_DEPTH		128.0f	// units inside an open face at which the fog has the zone's full density...
#define FOGZONE_FULL_DEPTH_FRAC	0.5f	// ...or this part of the box's smallest half-size, whichever is less
#define FOGZONE_SMOOTH_TIME		0.25f	// seconds: the strength follows the depth with this time constant
#define FOGZONE_OPEN_PROBE		8.0f	// a face is "open" if the point this far outside it is not solid
#define FOGZONE_MAX_ALPHA		0.95f

struct fogzone_t
{
	bool used;
	bool active;
	float mins[3], maxs[3];
	float color[3];		// 0..255
	float density;
	bool facesChecked;	// open[] is set (FogZone_CheckFaces, on the first draw after a FogZone message)
	bool open[6];		// min x, y, z, max x, y, z: open air behind the face (the fog thins out toward it)
};

static fogzone_t g_FogZones[FOGZONE_MAX];
static int g_iFogZoneInside = -1;		// the zone the main view's eye is in (or was last), -1: none yet
static float g_flFogZoneStrength = 0.0f;	// 0..1, smoothed: how much of that zone's fog is on
static float g_flFogZoneTime = 0.0f;
static HSPRITE g_hFogZoneSprite = 0;

/*
=================
FogZone_VidInit

The haze's sprite, loaded on every VidInit like the HUD's own sprites (CHud::VidInit). The engine frees its HUD
sprite list on a disconnect, so a handle kept from an earlier connection named a freed slot: GetSpritePointer gave
NULL and SpriteTexture crashed in the engine (R_TriangleSpriteTexture) the second time a map with an env_fogzone was
loaded in one game session (co_sbo_showcase: retry, or a listen server and then a dedicated one).
=================
*/
void FogZone_VidInit( void )
{
	g_hFogZoneSprite = SPR_Load( "sprites/white.spr" );
}

void FogZone_Clear( void )
{
	memset( g_FogZones, 0, sizeof( g_FogZones ) );
	g_iFogZoneInside = -1;
	g_flFogZoneStrength = 0.0f;
}

void FogZone_Set( int iIndex, bool bActive, const float *mins, const float *maxs, const float *color, float flDensity )
{
	if ( iIndex < 0 || iIndex >= FOGZONE_MAX )
		return;

	fogzone_t &zone = g_FogZones[iIndex];
	zone.used = true;
	zone.active = bActive;
	VectorCopy( mins, zone.mins );
	VectorCopy( maxs, zone.maxs );

	// the message has the entity's absmin / absmax, which the engine (SV_LinkEdict, cbase.cpp SetObjectCollisionBox)
	// pads by 1 unit on every side: the brush's own box is 1 unit in. Without this, the far faces sat exactly on the
	// walls of a room the zone fills and lost the depth test (no haze from outside at all).
	for ( int i = 0; i < 3; i++ )
	{
		if ( zone.maxs[i] - zone.mins[i] > 2.0f )
		{
			zone.mins[i] += 1.0f;
			zone.maxs[i] -= 1.0f;
		}
	}
	VectorCopy( color, zone.color );
	zone.density = flDensity;
	zone.facesChecked = false;
}

// Which faces of the zone's box have open air behind them: of 5 x 5 points 8 units outside the face (inset from its
// edges), more than half not solid. A face against a wall, floor or ceiling keeps the fog's full density up to it, so
// a zone that fills a room looks the same as without the thinning; a doorway in a wall does not open the face.
static void FogZone_CheckFaces( fogzone_t &zone )
{
	zone.facesChecked = true;
	for ( int iAxis = 0; iAxis < 3; iAxis++ )
	{
		const int iU = ( iAxis + 1 ) % 3, iV = ( iAxis + 2 ) % 3;
		for ( int iSide = 0; iSide < 2; iSide++ )
		{
			int iOpen = 0;
			for ( int u = 0; u < 5; u++ )
			{
				for ( int v = 0; v < 5; v++ )
				{
					vec3_t vecProbe;
					vecProbe[iAxis] = iSide ? zone.maxs[iAxis] + FOGZONE_OPEN_PROBE : zone.mins[iAxis] - FOGZONE_OPEN_PROBE;
					vecProbe[iU] = zone.mins[iU] + ( zone.maxs[iU] - zone.mins[iU] ) * ( 0.1f + 0.2f * u );
					vecProbe[iV] = zone.mins[iV] + ( zone.maxs[iV] - zone.mins[iV] ) * ( 0.1f + 0.2f * v );
					if ( gEngfuncs.PM_PointContents( vecProbe, NULL ) != CONTENTS_SOLID )
						iOpen++;
				}
			}
			zone.open[iSide * 3 + iAxis] = iOpen > 12;
		}
	}
}

// How much of the zone's fog an eye at this point gets, 0 (outside, or at an open face) .. 1; -1: not in the box
static float FogZone_DepthStrength( const fogzone_t &zone, const float *eye )
{
	int i;
	for ( i = 0; i < 3; i++ )
	{
		if ( eye[i] < zone.mins[i] || eye[i] > zone.maxs[i] )
			return -1.0f;
	}

	float flSmallest = zone.maxs[0] - zone.mins[0];
	for ( i = 1; i < 3; i++ )
	{
		if ( zone.maxs[i] - zone.mins[i] < flSmallest )
			flSmallest = zone.maxs[i] - zone.mins[i];
	}

	float flFull = flSmallest * 0.5f * FOGZONE_FULL_DEPTH_FRAC;
	if ( flFull > FOGZONE_FULL_DEPTH )
		flFull = FOGZONE_FULL_DEPTH;
	if ( flFull < 1.0f )
		flFull = 1.0f;

	// the nearest face one can leave the zone through: a face with a wall, floor or ceiling behind it doesn't count
	float flDepth = flFull;
	for ( i = 0; i < 3; i++ )
	{
		for ( int iSide = 0; iSide < 2; iSide++ )
		{
			const float flDist = iSide ? zone.maxs[i] - eye[i] : eye[i] - zone.mins[i];
			if ( flDist >= flDepth )
				continue;

			vec3_t vecProbe;
			VectorCopy( eye, vecProbe );
			vecProbe[i] = iSide ? zone.maxs[i] + FOGZONE_OPEN_PROBE : zone.mins[i] - FOGZONE_OPEN_PROBE;
			if ( gEngfuncs.PM_PointContents( vecProbe, NULL ) == CONTENTS_SOLID )
				continue;

			flDepth = flDist;
		}
	}

	return flDepth / flFull;
}

// Every frame, from RenderFog: the zone the eye is in and its strength. Returns the zone (NULL: none) and the
// strength to use for this view.
static const fogzone_t *FogZone_Update( float &flStrength )
{
	flStrength = 0.0f;

#if !FOGZONE_CLIENT_SIDE
	return NULL;
#else
	int iBest = -1;
	float flBest = -1.0f;

	for ( int i = 0; i < FOGZONE_MAX; i++ )
	{
		// (a zone without density counts: it is a clear volume in a foggy map)
		if ( !g_FogZones[i].used || !g_FogZones[i].active )
			continue;

		const float fl = FogZone_DepthStrength( g_FogZones[i], v_origin );
		// overlapping zones: the one the eye is deeper in
		if ( fl > flBest )
		{
			flBest = fl;
			iBest = i;
		}
	}

	// the spectator's camera inset has another eye: what its position gives, not smoothed
	if ( gHUD.m_Spectator.m_bInsetDrawPass )
	{
		if ( iBest < 0 )
			return NULL;
		flStrength = flBest;
		return &g_FogZones[iBest];
	}

	const float flTime = gEngfuncs.GetClientTime();
	float flFrame = flTime - g_flFogZoneTime;
	g_flFogZoneTime = flTime;

	const float flTarget = iBest >= 0 ? flBest : 0.0f;

	if ( iBest >= 0 && iBest != g_iFogZoneInside )
	{
		// another zone: its fog starts from nothing (a zone left at an open face had no strength left)
		g_iFogZoneInside = iBest;
		g_flFogZoneStrength = 0.0f;
	}

	if ( flFrame < 0.0f || flFrame > 1.0f )
	{
		g_flFogZoneStrength = flTarget;		// first frame, new map, a pause: no catching up
	}
	else
	{
		float flStep = flFrame / FOGZONE_SMOOTH_TIME;
		if ( flStep > 1.0f )
			flStep = 1.0f;
		g_flFogZoneStrength += ( flTarget - g_flFogZoneStrength ) * flStep;
	}

	if ( g_iFogZoneInside < 0 || !g_FogZones[g_iFogZoneInside].used || !g_FogZones[g_iFogZoneInside].active )
	{
		g_flFogZoneStrength = 0.0f;
		return NULL;
	}

	flStrength = g_flFogZoneStrength;
	return flStrength > 0.001f ? &g_FogZones[g_iFogZoneInside] : NULL;
#endif
}

// The length of the ray eye -> point that lies inside the box (the point is on the box's far side)
static float FogZone_RayLength( const fogzone_t &zone, const float *eye, const float *point )
{
	float flEnter = 0.0f;	// as a fraction of eye -> point
	float flLength = 0.0f;

	for ( int i = 0; i < 3; i++ )
	{
		const float flDelta = point[i] - eye[i];
		flLength += flDelta * flDelta;

		if ( flDelta > -0.001f && flDelta < 0.001f )
			continue;

		const float fl1 = ( zone.mins[i] - eye[i] ) / flDelta;
		const float fl2 = ( zone.maxs[i] - eye[i] ) / flDelta;
		const float flNear = fl1 < fl2 ? fl1 : fl2;
		if ( flNear > flEnter )
			flEnter = flNear;
	}

	if ( flEnter >= 1.0f )
		return 0.0f;

	return sqrt( flLength ) * ( 1.0f - flEnter );
}

// How much of a zone's haze shows in this view: 1 - the strength of its fog the eye already has
static float FogZone_HazeScale( int iZone )
{
	const fogzone_t &zone = g_FogZones[iZone];
	if ( gHUD.m_Spectator.m_bInsetDrawPass )
	{
		const float fl = FogZone_DepthStrength( zone, v_origin );
		return fl > 0.0f ? 1.0f - fl : 1.0f;
	}
	return iZone == g_iFogZoneInside ? 1.0f - g_flFogZoneStrength : 1.0f;
}

// The fallback haze: the far faces of every active zone as a grid of quads with per-vertex alpha (no GLSL, or the
// view's depth can't be copied here)
static void FogZone_DrawFaces( void )
{
#if FOGZONE_CLIENT_SIDE
	bool bBegun = false;

	for ( int iZone = 0; iZone < FOGZONE_MAX; iZone++ )
	{
		const fogzone_t &zone = g_FogZones[iZone];
		if ( !zone.used || !zone.active || zone.density <= 0.0f )
			continue;

		// the zone whose fog is on: the haze makes room for it
		const float flScale = FogZone_HazeScale( iZone );
		if ( flScale <= 0.01f )
			continue;

		for ( int iAxis = 0; iAxis < 3; iAxis++ )
		{
			const int iU = ( iAxis + 1 ) % 3, iV = ( iAxis + 2 ) % 3;

			for ( int iSide = 0; iSide < 2; iSide++ )
			{
				// a far face: the eye is on the inner side of its plane
				if ( iSide ? v_origin[iAxis] >= zone.maxs[iAxis] : v_origin[iAxis] <= zone.mins[iAxis] )
					continue;

				if ( !bBegun )
				{
					// FogZone_VidInit loads the sprite; no sprite model (not loaded): no haze, never a NULL to the engine
					struct model_s *pSprite = g_hFogZoneSprite
						? (struct model_s *)gEngfuncs.GetSpritePointer( g_hFogZoneSprite ) : NULL;
					if ( !pSprite || !gEngfuncs.pTriAPI->SpriteTexture( pSprite, 0 ) )
						return;

					// the one triangle API mode that takes a per-vertex alpha
					gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
					gEngfuncs.pTriAPI->CullFace( TRI_NONE );
					gEngfuncs.pTriAPI->Begin( TRI_QUADS );
					bBegun = true;
				}

				const float flPlane = iSide ? zone.maxs[iAxis] - FOGZONE_INSET : zone.mins[iAxis] + FOGZONE_INSET;
				float flAlpha[FOGZONE_GRID + 1][FOGZONE_GRID + 1];
				vec3_t vecPoint[FOGZONE_GRID + 1][FOGZONE_GRID + 1];
				int u, v;

				for ( u = 0; u <= FOGZONE_GRID; u++ )
				{
					for ( v = 0; v <= FOGZONE_GRID; v++ )
					{
						float *p = vecPoint[u][v];
						p[iAxis] = flPlane;
						p[iU] = zone.mins[iU] + ( zone.maxs[iU] - zone.mins[iU] ) * u / FOGZONE_GRID;
						p[iV] = zone.mins[iV] + ( zone.maxs[iV] - zone.mins[iV] ) * v / FOGZONE_GRID;

						const float flThick = FogZone_RayLength( zone, v_origin, p ) * zone.density;
						float fl = ( 1.0f - exp( -flThick * flThick ) ) * flScale;
						if ( fl > FOGZONE_MAX_ALPHA )
							fl = FOGZONE_MAX_ALPHA;
						flAlpha[u][v] = fl;
					}
				}

				for ( u = 0; u < FOGZONE_GRID; u++ )
				{
					for ( v = 0; v < FOGZONE_GRID; v++ )
					{
						static const int corner[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };

						if ( flAlpha[u][v] <= 0.004f && flAlpha[u + 1][v] <= 0.004f && flAlpha[u + 1][v + 1] <= 0.004f
							&& flAlpha[u][v + 1] <= 0.004f )
							continue;

						for ( int c = 0; c < 4; c++ )
						{
							const int cu = u + corner[c][0], cv = v + corner[c][1];
							gEngfuncs.pTriAPI->Color4f( zone.color[0] / 255.0f, zone.color[1] / 255.0f,
								zone.color[2] / 255.0f, flAlpha[cu][cv] );
							gEngfuncs.pTriAPI->TexCoord2f( 0.5f, 0.5f );
							gEngfuncs.pTriAPI->Vertex3fv( vecPoint[cu][cv] );
						}
					}
				}
			}
		}
	}

	if ( bBegun )
	{
		gEngfuncs.pTriAPI->End();
		gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	}
#endif
}

// The haze of every active zone, for the view at v_origin (HUD_DrawTransparentTriangles; not on the overview map):
// the per-pixel fog volume, or the far faces where it can't run. With developer 1 the console says once per GL context
// (and on a change) which of the two draws.
static void FogZone_Draw( void )
{
#if FOGZONE_CLIENT_SIDE
	static int s_iLoggedGeneration = -1;
	static int s_iLoggedPath = -1;	// 0: far faces, 1: per pixel

	int iZones[FOGZONE_MAX];
	int iCount = 0;

	for ( int iZone = 0; iZone < FOGZONE_MAX; iZone++ )
	{
		fogzone_t &zone = g_FogZones[iZone];
		if ( !zone.used || !zone.active || zone.density <= 0.0f || FogZone_HazeScale( iZone ) <= 0.01f )
			continue;

		// a box the view can't see (outside the PVS, the eye not in it) costs nothing, no depth copy
		if ( FogZone_DepthStrength( zone, v_origin ) < 0.0f && !gEngfuncs.pTriAPI->BoxInPVS( zone.mins, zone.maxs ) )
			continue;

		iZones[iCount++] = iZone;
	}

	if ( !iCount )
		return;

	const bool bPerPixel = FogZoneGL_Begin( v_origin );
	const int iPath = bPerPixel ? 1 : 0;
	if ( iPath != s_iLoggedPath || FogZoneGL_Generation() != s_iLoggedGeneration )
	{
		s_iLoggedPath = iPath;
		s_iLoggedGeneration = FogZoneGL_Generation();
		if ( bPerPixel )
			gEngfuncs.Con_DPrintf( "env_fogzone: %s\n", FogZoneGL_Describe() );
		else
			gEngfuncs.Con_DPrintf( "env_fogzone: far-face haze (fallback): %s\n", FogZoneGL_Why() );
	}

	if ( !bPerPixel )
	{
		FogZone_DrawFaces();
		return;
	}

	for ( int i = 0; i < iCount; i++ )
	{
		fogzone_t &zone = g_FogZones[iZones[i]];
		if ( !zone.facesChecked )
			FogZone_CheckFaces( zone );
		const float color[3] = { zone.color[0] / 255.0f, zone.color[1] / 255.0f, zone.color[2] / 255.0f };
		FogZoneGL_Zone( zone.mins, zone.maxs, color, zone.density, FogZone_HazeScale( iZones[i] ), zone.open );
	}

	FogZoneGL_End();
#endif
}

/*
=================
RenderFog

cs16-client tri.cpp: the env_fog of the map, off underwater (the engine has its own water fog).
SBO: "underwater" is the view's contents (cs16-client: the local player's waterlevel), which is right for spectators.
SBO 2.0: a fade runs on underwater too, so the fog is right when the view comes up.
SBO 2.0: the fog set here stays on for what the engine draws next, which is the world of the next view. On the
spectator's overview map (its camera is far above the map) "underwater" is kept from the last camera view.
=================
*/
void RenderFog( void )
{
	static bool bUnderwater = false;

	Fog_Update();

	FogParameters fog = g_FogParameters;

	if ( !gHUD.m_Spectator.m_bOverviewDrawPass )
	{
		// PROTOTYPE env_fogzone: from the map's fog to the zone's by how deep the eye is in the zone
		float flStrength;
		const fogzone_t *pZone = FogZone_Update( flStrength );
		if ( pZone )
		{
			if ( fog.density <= 0.0f )
			{
				VectorCopy( pZone->color, fog.color );	// no fog has no color
			}
			else if ( pZone->density > 0.0f )
			{
				for ( int i = 0; i < 3; i++ )
					fog.color[i] += ( pZone->color[i] - fog.color[i] ) * flStrength;
			}
			fog.density += ( pZone->density - fog.density ) * flStrength;
		}

		const int iContents = gEngfuncs.PM_PointContents( v_origin, NULL );
		bUnderwater = ( iContents == CONTENTS_WATER || iContents == CONTENTS_SLIME || iContents == CONTENTS_LAVA );
	}

	// SBO: FogParams is a Steam engine addition to the triangle API
	if ( gEngfuncs.pTriAPI->FogParams )
		gEngfuncs.pTriAPI->FogParams( fog.density, fog.affectsSkyBox );
	gEngfuncs.pTriAPI->Fog( fog.color, 100.0f, 2000.0f, !bUnderwater ? fog.density > 0.0f : 0 );
}

/*
=================
RenderSkyFog

SoHL 1.8 (cl_dll/view.cpp: "no fog in the env_sky"): the sky room of an env_sky is drawn before the view
(view.cpp V_SkyViewStart) and stands for its skybox, which the fog of the Fog message leaves clear. The engine draws a
world with the fog the view before it left on (HUD_DrawTransparentTriangles), so the view before the sky room ends with
this, and the sky room ends with the view's fog (RenderFog), which its world then has.
SBO 2.1 (Source's sky_camera fog): an env_sky with a fog of its own ("rendercolor", "density", SetSky) gets that fog
instead, in the same way as RenderFog sets the map's: the sky room's world, its entities and the env_static props in
it are fogged by their distance from the sky camera, in sky-room units; its 2D skybox stays clear, as the map's does.
=================
*/
void RenderSkyFog( void )
{
	const FogParameters &fog = gHUD.m_SkyFog;

	if ( fog.density > 0.0f )
	{
		// SBO: FogParams is a Steam engine addition to the triangle API (RenderFog)
		if ( gEngfuncs.pTriAPI->FogParams )
			gEngfuncs.pTriAPI->FogParams( fog.density, false );
		gEngfuncs.pTriAPI->Fog( (float *)fog.color, 100.0f, 2000.0f, 1 );
		return;
	}

	gEngfuncs.pTriAPI->Fog( g_FogParameters.color, 100.0f, 2000.0f, 0 );
}

/*
=================
HUD_GetEffectAngles

SBO 2.0: the angles the camera-facing effects turn to (flames, smoke, weather particles). The main view: the client's
view angles, as before. The spectator's camera inset has its own (in first person: the watched player's).
=================
*/
void HUD_GetEffectAngles( float *angles )
{
	if ( gHUD.m_Spectator.m_bInsetDrawPass )
	{
		VectorCopy( v_angles, angles );
	}
	else
	{
		gEngfuncs.GetViewAngles( angles );
	}
}

//#define TEST_IT
#if defined( TEST_IT )

/*
=================
Draw_Triangles

Example routine.  Draws a sprite offset from the player origin.
=================
*/
void Draw_Triangles( void )
{
	cl_entity_t *player;
	vec3_t org;

	// Load it up with some bogus data
	player = gEngfuncs.GetLocalPlayer();
	if ( !player )
		return;

	org = player->origin;

	org.x += 50;
	org.y += 50;

	if (gHUD.m_hsprCursor == 0)
	{
		char sz[256];
		sprintf( sz, "sprites/cursor.spr" );
		gHUD.m_hsprCursor = SPR_Load( sz );
	}

	if ( !gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( gHUD.m_hsprCursor ), 0 ))
	{
		return;
	}
	
	// Create a triangle, sigh
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	// Overload p->color with index into tracer palette, p->packedColor with brightness
	gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, 1.0 );
	// UNDONE: This gouraud shading causes tracers to disappear on some cards (permedia2)
	gEngfuncs.pTriAPI->Brightness( 1 );
	gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
	gEngfuncs.pTriAPI->Vertex3f( org.x, org.y, org.z );

	gEngfuncs.pTriAPI->Brightness( 1 );
	gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
	gEngfuncs.pTriAPI->Vertex3f( org.x, org.y + 50, org.z );

	gEngfuncs.pTriAPI->Brightness( 1 );
	gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
	gEngfuncs.pTriAPI->Vertex3f( org.x + 50, org.y + 50, org.z );

	gEngfuncs.pTriAPI->Brightness( 1 );
	gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
	gEngfuncs.pTriAPI->Vertex3f( org.x + 50, org.y, org.z );

	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
}

#else

// SBO: empty stand-in, called from HUD_DrawNormalTriangles
void Draw_Triangles( void )
{
}

#endif

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void DLLEXPORT HUD_DrawNormalTriangles( void )
{
	// SoHL env_sky: nothing of the client's own in the sky room drawn before the view (view.cpp V_SkyViewStart), but
	// SBO 2.1 the sky fog on its 2D skybox (env_sky_box.cpp)
	if ( gHUD.m_bSkyDrawPass )
	{
		EnvSkyBox_DrawSkyFog();
		return;
	}

	// SBO 2.0: the map image and the icons of the overview are not fogged (XDM tri.cpp: no fog in the overview pass);
	// RenderFog puts the fog back at the end of this view
	if ( gHUD.m_Spectator.m_bOverviewDrawPass )
		gEngfuncs.pTriAPI->Fog( g_FogParameters.color, 100.0f, 2000.0f, 0 );

	// SBO 2.0: the current frames of animated sprays (spray_anim.cpp), once per frame
	SprayAnim_Frame();

	EnvSkyBox_Draw();	// SBO 2.1: the 2D skybox on the env_sky faces while the 3D sky is off (env_sky_box.cpp)

	gHUD.m_Spectator.DrawOverview();
	
	Draw_Triangles();
	WorldScreen_DrawWorld();	// prototype: world-space VGUI2 screens

	// SBO 2.0: point_worldtext (worldtext.cpp): before the engine's transparent entities (drawn after this call,
	// without depth writes), so one in front of a text is blended over it: a box with rendermode texture hides it at
	// FX amount 255 and dims it when partly faded. Not on the overview map.
	if ( !gHUD.m_Spectator.m_bOverviewDrawPass )
		WorldText_DrawFrame();
}

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
void DLLEXPORT HUD_DrawTransparentTriangles( void )
{
	// The engine (HL25 hw.so R_RenderScene, R_DrawTEntitiesOnList) draws a view's world and models with the GL fog the
	// view before it left on; it switches the triangle API fog on only after them, and right after this call it leaves
	// the fog on if the triangle API fog is on, else off. So what is set here is the fog of the NEXT view's world.
	// SBO 2.0, env_sky: the next view after the sky room is its view, which gets the view's fog (RenderFog; the eye is
	// still the view's, v_origin). Nothing else of the client's own in the sky room (the effects are around the view
	// and run once per view).
	FogZoneGL_NewView();	// SBO 2.1: a new view: its depth is copied again (env_fogzone, env_entity_glow)

	if ( gHUD.m_bSkyDrawPass )
	{
		EntityGlow_SkipView();	// SBO 2.1: env_entity_glow: no glow in the sky room
		RenderFog();
		return;
	}

	// SoHL env_sky: this view's entities are drawn; the sky room's entities, moved out of it, go back (view.cpp)
	EnvSky_ShowEntities();

	// SBO 2.0: env_fogzone: the zones' fog, on this view's world and models (before color correction grades the view;
	// nothing of the 3D world on the spectator's overview map)
	if ( !gHUD.m_Spectator.m_bOverviewDrawPass )
		FogZone_Draw();

	// SBO 2.1: env_entity_glow: outlines and halos around this view's targets (after the fog zones' haze, so the
	// haze does not cover them; not on the spectator's overview map)
	if ( !gHUD.m_Spectator.m_bOverviewDrawPass )
		EntityGlow_Draw();
	else
		EntityGlow_SkipView();

	// SBO 2.1: info_waypoint: the main view places the HUD markers, and a spectator's first-person inset its own
	if ( !gHUD.m_Spectator.m_bOverviewDrawPass && !gHUD.m_Spectator.m_bInsetDrawPass )
		Waypoint_ViewDrawn( 0 );
	else if ( !gHUD.m_Spectator.m_bOverviewDrawPass && gHUD.m_Spectator.InsetMode() == INSET_IN_EYE )
		Waypoint_ViewDrawn( 1 );	// the first-person inset: the watched player's view

	// SBO 2.0: color_correction: this view's world is drawn (a view without a view model is graded after it)
	ColorCorr_ViewDrawn();

	// cs16-client: env_fog
	RenderFog();

	// SBO 2.0, env_sky: the next view is a sky room (every view that draws the world has one first), which has no fog
	// (SoHL 1.8, view.cpp V_SkyViewStart), or SBO 2.1 its env_sky's own. Before this fix the view's own world had none:
	// the fog was off after the sky room, and fog zones and env_fog showed on models and effects only.
	if ( gHUD.m_iSkyMode == SKY_ON )
		RenderSkyFog();

	// SBO 2.0: nothing of the 3D world on the spectator's overview map, full screen or inset (XDM tri.cpp
	// HUD_DrawTransparentTriangles: "don't duplicate in overview window"). Main view and inset are never both a
	// camera (CHudSpectator::CheckSettings), so the effects run once a frame, around the camera that shows them.
	static float flLastDrawn = 0.0f;
	static bool bSkipped = false;
	const float flTime = gEngfuncs.GetClientTime();

	if ( gHUD.m_Spectator.m_bOverviewDrawPass )
	{
		bSkipped = true;
		return;
	}

	// back from the overview alone (no camera inset): the weather stood still, so it starts again instead of
	// catching up in one step
	if ( bSkipped && g_pParticleMan && ( flTime - flLastDrawn > 0.5f || flTime < flLastDrawn ) )
		g_pParticleMan->ResetParticles();

	bSkipped = false;
	flLastDrawn = flTime;

	// SBO 2.0: the camera inset (the main view is the overview then): the particle manager got the client's angles
	// (HUD_TempEntUpdate)
	if ( gHUD.m_Spectator.m_bInsetDrawPass && g_pParticleMan )
		g_pParticleMan->SetVariables( g_flGravity, v_angles );

	// SBO: particle emitters
	gHUD.m_Flamme.m_List.Afficher( gHUD.m_flTimeDelta );
	gHUD.m_Fumee.m_List.Afficher( gHUD.m_flTimeDelta );

	// SBO 2.0: env_particle (Spirit of Half-Life's Aurora particle systems, aurora/particlemgr.cpp)
	Aurora_DrawFrame();

	// cs16-client: weather particles
	if ( g_pParticleMan )
	{
		g_pParticleMan->Update();
		g_Environment.Update();
	}
}