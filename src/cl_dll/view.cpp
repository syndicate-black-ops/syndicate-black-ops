//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// view/refresh setup functions

#include "hud.h"
#include "cl_util.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"

#include "entity_state.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "in_defs.h" // PITCH YAW ROLL
#include "pm_movevars.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "event_api.h"
#include "pmtrace.h"
#include "screenfade.h"
#include "shake.h"
#include "hltv.h"
#include "parsemsg.h"
#include "mirror.h"	// SBO 2.0: env_mirror
#include "entity_types.h"	// SoHL env_sky: ET_NORMAL (EnvSky_AddEntity)
#include "view.h"
#include <string.h>
#include "colorcorrection.h"	// SBO 2.0: color_correction
#include "vgui2/vgui2_api.h"	// SBO 2.0: VGui2_GmPip, the GM players window's View
#include "mover_prediction.h"	// SBO 2.0: mover prediction (MoverPrediction_ViewOrigin)
#ifdef SBO_TEST_COMMANDS
#include "movewith_client.h"	// test_cl_mw_trace: MoveWith_TraceView
#endif

// Spectator Mode
extern "C" 
{
	float	vecNewViewAngles[3];
	int		iHasNewViewAngles;
	float	vecNewViewOrigin[3];
	int		iHasNewViewOrigin;
	int		iIsSpectator;
}

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

extern "C" 
{
	int CL_IsThirdPerson( void );
	void CL_CameraOffset( float *ofs );

	void DLLEXPORT V_CalcRefdef( struct ref_params_s *pparams );

	void PM_ParticleLine( float *start, float *end, int pcolor, float life, float vert);
	int		PM_GetVisEntInfo( int ent );
	int		PM_GetPhysEntInfo( int ent );
	void	InterpolateAngles(  float * start, float * end, float * output, float frac );
	void	NormalizeAngles( float * angles );
	float	Distance(const float * v1, const float * v2);
	float	AngleBetweenVectors(  const float * v1,  const float * v2 );

	float	vJumpOrigin[3];
	float	vJumpAngles[3];
}

void V_DropPunchAngle ( float frametime, float *ev_punchangle );
void VectorAngles( const float *forward, float *angles );

#include "r_studioint.h"
#include "com_model.h"

extern engine_studio_api_t IEngineStudio;

/*
The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.
*/

extern cvar_t	*cl_forwardspeed;
extern cvar_t	*chase_active;
extern cvar_t	*scr_ofsx, *scr_ofsy, *scr_ofsz;
extern cvar_t	*cl_vsmoothing;

#define	CAM_MODE_RELAX		1
#define CAM_MODE_FOCUS		2

vec3_t		v_origin, v_angles, v_cl_angles, v_sim_org, v_lastAngles;
vec3_t		v_client_aimangles;	// halflife-updated (halflife #1744): aim angles for predicted weapon events
float		v_frametime, v_lastDistance;	
float		v_cameraRelaxAngle	= 5.0f;
float		v_cameraFocusAngle	= 35.0f;
int			v_cameraMode = CAM_MODE_FOCUS;
qboolean	v_resetCamera = 1;

vec3_t ev_punchangle;

cvar_t	*scr_ofsx;
cvar_t	*scr_ofsy;
cvar_t	*scr_ofsz;

cvar_t	*v_centermove;
cvar_t	*v_centerspeed;

cvar_t	*cl_bobcycle;
cvar_t	*cl_bob;
cvar_t	*cl_bobtilt;	// halflife-updated
cvar_t	*cl_bobup;
cvar_t	*cl_waterdist;
cvar_t	*cl_chasedist;

// These cvars are not registered (so users can't cheat), so set the ->value field directly
// Register these cvars in V_Init() if needed for easy tweaking
cvar_t	v_iyaw_cycle		= {"v_iyaw_cycle", "2", 0, 2};
cvar_t	v_iroll_cycle		= {"v_iroll_cycle", "0.5", 0, 0.5};
cvar_t	v_ipitch_cycle		= {"v_ipitch_cycle", "1", 0, 1};
cvar_t	v_iyaw_level		= {"v_iyaw_level", "0.3", 0, 0.3};
cvar_t	v_iroll_level		= {"v_iroll_level", "0.1", 0, 0.1};
cvar_t	v_ipitch_level		= {"v_ipitch_level", "0.3", 0, 0.3};

float	v_idlescale;  // used by TFC for concussion grenade effect

//=============================================================================
/*
void V_NormalizeAngles( float *angles )
{
	int i;
	// Normalize angles
	for ( i = 0; i < 3; i++ )
	{
		if ( angles[i] > 180.0 )
		{
			angles[i] -= 360.0;
		}
		else if ( angles[i] < -180.0 )
		{
			angles[i] += 360.0;
		}
	}
}

/*
===================
V_InterpolateAngles

Interpolate Euler angles.
FIXME:  Use Quaternions to avoid discontinuities
Frac is 0.0 to 1.0 ( i.e., should probably be clamped, but doesn't have to be )
===================

void V_InterpolateAngles( float *start, float *end, float *output, float frac )
{
	int i;
	float ang1, ang2;
	float d;
	
	V_NormalizeAngles( start );
	V_NormalizeAngles( end );

	for ( i = 0 ; i < 3 ; i++ )
	{
		ang1 = start[i];
		ang2 = end[i];

		d = ang2 - ang1;
		if ( d > 180 )
		{
			d -= 360;
		}
		else if ( d < -180 )
		{	
			d += 360;
		}

		output[i] = ang1 + d * frac;
	}

	V_NormalizeAngles( output );
} */

// Quakeworld bob code, this fixes jitters in the mutliplayer since the clock (pparams->time) isn't quite linear
float V_CalcBob ( struct ref_params_s *pparams )
{
	static	double	bobtime;
	static float	bob;
	float	cycle;
	static float	lasttime;
	vec3_t	vel;
	

	if ( pparams->onground == -1 ||
		 pparams->time == lasttime )
	{
		// just use old value
		return bob;	
	}

	lasttime = pparams->time;

	bobtime += pparams->frametime;
	cycle = bobtime - (int)( bobtime / cl_bobcycle->value ) * cl_bobcycle->value;
	cycle /= cl_bobcycle->value;
	
	if ( cycle < cl_bobup->value )
	{
		cycle = M_PI * cycle / cl_bobup->value;
	}
	else
	{
		cycle = M_PI + M_PI * ( cycle - cl_bobup->value )/( 1.0 - cl_bobup->value );
	}

	// bob is proportional to simulated velocity in the xy plane
	// (don't count Z, or jumping messes it up)
	VectorCopy( pparams->simvel, vel );
	vel[2] = 0;

	bob = sqrt( vel[0] * vel[0] + vel[1] * vel[1] ) * cl_bob->value;
	bob = bob * 0.3 + bob * 0.7 * sin(cycle);
	bob = min( bob, 4 );
	bob = max( bob, -7 );
	return bob;
	
}

/*
===============
V_CalcRoll
Used by view and sv_user
===============
*/
extern cvar_t *cl_rollangle;
extern cvar_t *cl_rollspeed;

float V_CalcRoll (vec3_t angles, vec3_t velocity, float rollangle, float rollspeed )
{
    float   sign;
    float   side;
    float   value;
	vec3_t  forward, right, up;
    
	AngleVectors ( angles, forward, right, up );
    
	side = DotProduct (velocity, right);
    sign = side < 0 ? -1 : 1;
    side = fabs( side );
    
	value = rollangle;
    if (side < rollspeed)
	{
		side = side * value / rollspeed;
	}
    else
	{
		side = value;
	}
	return side * sign;
}

typedef struct pitchdrift_s
{
	float		pitchvel;
	int			nodrift;
	float		driftmove;
	double		laststop;
} pitchdrift_t;

static pitchdrift_t pd;

void V_StartPitchDrift( void )
{
	if ( pd.laststop == gEngfuncs.GetClientTime() )
	{
		return;		// something else is keeping it from drifting
	}

	if ( pd.nodrift || !pd.pitchvel )
	{
		pd.pitchvel = v_centerspeed->value;
		pd.nodrift = 0;
		pd.driftmove = 0;
	}
}

void V_StopPitchDrift ( void )
{
	pd.laststop = gEngfuncs.GetClientTime();
	pd.nodrift = 1;
	pd.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.
===============
*/
void V_DriftPitch ( struct ref_params_s *pparams )
{
	float		delta, move;

	if ( gEngfuncs.IsNoClipping() || !pparams->onground || pparams->demoplayback || pparams->spectator )
	{
		pd.driftmove = 0;
		pd.pitchvel = 0;
		return;
	}

	// don't count small mouse motion
	if (pd.nodrift)
	{
		if ( fabs( pparams->cmd->forwardmove ) < cl_forwardspeed->value )
			pd.driftmove = 0;
		else
			pd.driftmove += pparams->frametime;
	
		if ( pd.driftmove > v_centermove->value)
		{
			V_StartPitchDrift ();
		}
		return;
	}
	
	delta = pparams->idealpitch - pparams->cl_viewangles[PITCH];

	if (!delta)
	{
		pd.pitchvel = 0;
		return;
	}

	move = pparams->frametime * pd.pitchvel;
	pd.pitchvel += pparams->frametime * v_centerspeed->value;
	
	if (delta > 0)
	{
		if (move > delta)
		{
			pd.pitchvel = 0;
			move = delta;
		}
		pparams->cl_viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			pd.pitchvel = 0;
			move = -delta;
		}
		pparams->cl_viewangles[PITCH] -= move;
	}
}

/* 
============================================================================== 
						VIEW RENDERING 
============================================================================== 
*/ 

/*
==================
V_CalcGunAngle
==================
*/
void V_CalcGunAngle ( struct ref_params_s *pparams )
{	
	cl_entity_t *viewent;
	
	viewent = gEngfuncs.GetViewModel();
	if ( !viewent )
		return;

	viewent->angles[YAW]   =  pparams->viewangles[YAW]   + pparams->crosshairangle[YAW];
	viewent->angles[PITCH] = -pparams->viewangles[PITCH] + pparams->crosshairangle[PITCH] * 0.25;
	viewent->angles[ROLL]  -= v_idlescale * sin(pparams->time*v_iroll_cycle.value) * v_iroll_level.value;
	
	// don't apply all of the v_ipitch to prevent normally unseen parts of viewmodel from coming into view.
	viewent->angles[PITCH] -= v_idlescale * sin(pparams->time*v_ipitch_cycle.value) * (v_ipitch_level.value * 0.5);
	viewent->angles[YAW]   -= v_idlescale * sin(pparams->time*v_iyaw_cycle.value) * v_iyaw_level.value;

	// HL25: the latched view model origin/angles are now set at the end of V_CalcNormalRefdef (after bob/shake)
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle ( struct ref_params_s *pparams )
{
	pparams->viewangles[ROLL] += v_idlescale * sin(pparams->time*v_iroll_cycle.value) * v_iroll_level.value;
	pparams->viewangles[PITCH] += v_idlescale * sin(pparams->time*v_ipitch_cycle.value) * v_ipitch_level.value;
	pparams->viewangles[YAW] += v_idlescale * sin(pparams->time*v_iyaw_cycle.value) * v_iyaw_level.value;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll ( struct ref_params_s *pparams )
{
	float		side;
	cl_entity_t *viewentity;
	
	viewentity = gEngfuncs.GetEntityByIndex( pparams->viewentity );
	if ( !viewentity )
		return;

	side = V_CalcRoll ( viewentity->angles, pparams->simvel, cl_rollangle->value, cl_rollspeed->value );	// halflife-updated (halflife #1544)

	pparams->viewangles[ROLL] += side;

	if ( pparams->health <= 0 && ( pparams->viewheight[2] != 0 ) )
	{
		// only roll the view if the player is dead and the viewheight[2] is nonzero 
		// this is so deadcam in multiplayer will work.
		pparams->viewangles[ROLL] = 80;	// dead view angle
		return;
	}
}


/*
==================
V_CalcIntermissionRefdef

==================
*/
void V_CalcIntermissionRefdef ( struct ref_params_s *pparams )
{
	cl_entity_t	*ent, *view;
	float		old;

	// ent is the player model ( visible when out of body )
	ent = gEngfuncs.GetLocalPlayer();
	
	// view is the weapon model (only visible from inside body )
	view = gEngfuncs.GetViewModel();

	VectorCopy ( pparams->simorg, pparams->vieworg );
	VectorCopy ( pparams->cl_viewangles, pparams->viewangles );

	view->model = NULL;

	// allways idle in intermission
	old = v_idlescale;
	v_idlescale = 1;

	V_AddIdle ( pparams );

	if ( gEngfuncs.IsSpectateOnly() )
	{
		// in HLTV we must go to 'intermission' position by ourself
		VectorCopy( gHUD.m_Spectator.m_cameraOrigin, pparams->vieworg );
		VectorCopy( gHUD.m_Spectator.m_cameraAngles, pparams->viewangles );
	}

	v_idlescale = old;

	v_cl_angles = pparams->cl_viewangles;
	v_origin = pparams->vieworg;
	v_angles = pparams->viewangles;
}

#define ORIGIN_BACKUP 64
#define ORIGIN_MASK ( ORIGIN_BACKUP - 1 )

typedef struct 
{
	float Origins[ ORIGIN_BACKUP ][3];
	float OriginTime[ ORIGIN_BACKUP ];

	float Angles[ ORIGIN_BACKUP ][3];
	float AngleTime[ ORIGIN_BACKUP ];

	int CurrentOrigin;
	int CurrentAngle;
} viewinterp_t;

/*
==================
V_CalcRefdef

==================
*/
void V_CalcNormalRefdef ( struct ref_params_s *pparams )
{
	cl_entity_t		*ent, *view;
	int				i;
	vec3_t			angles;
	float			bob, waterOffset;
	static viewinterp_t		ViewInterp;

	static float oldz = 0;
	static float lasttime;

	vec3_t camAngles, camForward, camRight, camUp;
	cl_entity_t *pwater;

	V_DriftPitch ( pparams );

	if ( gEngfuncs.IsSpectateOnly() )
	{
		ent = gEngfuncs.GetEntityByIndex( g_iUser2 );
	}
	else
	{
		// ent is the player model ( visible when out of body )
		ent = gEngfuncs.GetLocalPlayer();
	}
	
	// view is the weapon model (only visible from inside body )
	view = gEngfuncs.GetViewModel();

	// transform the view offset by the model's matrix to get the offset from
	// model origin for the view
	bob = V_CalcBob ( pparams );

	// refresh position
	VectorCopy ( pparams->simorg, pparams->vieworg );
	pparams->vieworg[2] += ( bob );
	VectorAdd( pparams->vieworg, pparams->viewheight, pparams->vieworg );

	VectorCopy ( pparams->cl_viewangles, pparams->viewangles );

	gEngfuncs.V_CalcShake();
	gEngfuncs.V_ApplyShake( pparams->vieworg, pparams->viewangles, 1.0 );

	// never let view origin sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	// FIXME, we send origin at 1/128 now, change this?
	// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
	
	pparams->vieworg[0] += 1.0/32;
	pparams->vieworg[1] += 1.0/32;
	pparams->vieworg[2] += 1.0/32;

	// Check for problems around water, move the viewer artificially if necessary 
	// -- this prevents drawing errors in GL due to waves

	waterOffset = 0;
	if ( pparams->waterlevel >= 2 )
	{
		int		i, contents, waterDist, waterEntity;
		vec3_t	point;
		waterDist = cl_waterdist->value;

		if ( pparams->hardware )
		{
			waterEntity = gEngfuncs.PM_WaterEntity( pparams->simorg );
			if ( waterEntity >= 0 && waterEntity < pparams->max_entities )
			{
				pwater = gEngfuncs.GetEntityByIndex( waterEntity );
				if ( pwater && ( pwater->model != NULL ) )
				{
					waterDist += ( pwater->curstate.scale * 16 );	// Add in wave height
				}
			}
		}
		else
		{
			waterEntity = 0;	// Don't need this in software
		}
		
		VectorCopy( pparams->vieworg, point );

		// Eyes are above water, make sure we're above the waves
		if ( pparams->waterlevel == 2 )	
		{
			point[2] -= waterDist;
			for ( i = 0; i < waterDist; i++ )
			{
				contents = gEngfuncs.PM_PointContents( point, NULL );
				if ( contents > CONTENTS_WATER )
					break;
				point[2] += 1;
			}
			waterOffset = (point[2] + waterDist) - pparams->vieworg[2];
		}
		else
		{
			// eyes are under water.  Make sure we're far enough under
			point[2] += waterDist;

			for ( i = 0; i < waterDist; i++ )
			{
				contents = gEngfuncs.PM_PointContents( point, NULL );
				if ( contents <= CONTENTS_WATER )
					break;
				point[2] -= 1;
			}
			waterOffset = (point[2] - waterDist) - pparams->vieworg[2];
		}
	}

	pparams->vieworg[2] += waterOffset;
	
	V_CalcViewRoll ( pparams );
	
	V_AddIdle ( pparams );

	// offsets
	VectorCopy( pparams->cl_viewangles, angles );

	AngleVectors ( angles, pparams->forward, pparams->right, pparams->up );

	// don't allow cheats in multiplayer
	if ( pparams->maxclients <= 1 )
	{
		for ( i=0 ; i<3 ; i++ )
		{
			pparams->vieworg[i] += scr_ofsx->value*pparams->forward[i] + scr_ofsy->value*pparams->right[i] + scr_ofsz->value*pparams->up[i];
		}
	}
	
	// Treating cam_ofs[2] as the distance
	if( CL_IsThirdPerson() )
	{
		vec3_t ofs;

		ofs[0] = ofs[1] = ofs[2] = 0.0;

		CL_CameraOffset( (float *)&ofs );

		VectorCopy( ofs, camAngles );
		camAngles[ ROLL ]	= 0;

		AngleVectors( camAngles, camForward, camRight, camUp );

		for ( i = 0; i < 3; i++ )
		{
			pparams->vieworg[ i ] += -ofs[2] * camForward[ i ];
		}
	}
	
	// Give gun our viewangles
	VectorCopy ( pparams->cl_viewangles, view->angles );
	
	// set up gun position
	V_CalcGunAngle ( pparams );

	// Use predicted origin as view origin.
	VectorCopy ( pparams->simorg, view->origin );      
	view->origin[2] += ( waterOffset );
	VectorAdd( view->origin, pparams->viewheight, view->origin );

	// Let the viewmodel shake at about 10% of the amplitude
	gEngfuncs.V_ApplyShake( view->origin, view->angles, 0.9 );

	for ( i = 0; i < 3; i++ )
	{
		view->origin[ i ] += bob * 0.4 * pparams->forward[ i ];
	}
	view->origin[2] += bob;

	// halflife-updated: cl_bobtilt (default 0). The tilt used to be overwritten before rendering (see the latched
	// copy at the end of V_CalcNormalRefdef), so 0 keeps the look players are used to.
	if ( cl_bobtilt->value )
	{
		// throw in a little tilt.
		view->angles[YAW]   -= bob * 0.5;
		view->angles[ROLL]  -= bob * 1;
		view->angles[PITCH] -= bob * 0.3;
	}

	// pushing the view origin down off of the same X/Z plane as the ent's origin will give the
	// gun a very nice 'shifting' effect when the player looks up/down. If there is a problem
	// with view model distortion, this may be a cause. (SJB). 
	view->origin[2] -= 1;

	// fudge position around to keep amount of weapon visible
	// roughly equal with different FOV
	if (pparams->viewsize == 110)
	{
		view->origin[2] += 1;
	}
	else if (pparams->viewsize == 100)
	{
		view->origin[2] += 2;
	}
	else if (pparams->viewsize == 90)
	{
		view->origin[2] += 1;
	}
	else if (pparams->viewsize == 80)
	{
		view->origin[2] += 0.5;
	}

	// Add in the punchangle, if any
	VectorAdd ( pparams->viewangles, pparams->punchangle, pparams->viewangles );

	// Include client side punch, too
	VectorAdd ( pparams->viewangles, (float *)&ev_punchangle, pparams->viewangles);

	V_DropPunchAngle ( pparams->frametime, (float *)&ev_punchangle );

	// SBO 2.0 seamless trigger_teleport: the predicted origin jumped much farther than the speed carries it in
	// this frame, a teleport, not a step: no stair smoothing from the old height (the view would glide up to
	// 18 units after a landmark teleport to a higher place)
	{
		static float lastsimorg[3];
		vec3_t delta;

		VectorSubtract( pparams->simorg, lastsimorg, delta );
		float flMoved = Length( delta );
		float flExpected = Length( pparams->simvel ) * max( pparams->frametime, 0.0f );
		if ( flMoved > 64.0f && flMoved > 2.0f * flExpected + 32.0f )
			oldz = pparams->simorg[2];
		VectorCopy( pparams->simorg, lastsimorg );
	}

	// smooth out stair step ups
#if 1
	if ( !pparams->smoothing && pparams->onground && pparams->simorg[2] - oldz > 0)
	{
		float steptime;
		
		steptime = pparams->time - lasttime;
		if (steptime < 0)
	//FIXME		I_Error ("steptime < 0");
			steptime = 0;

		oldz += steptime * 150;
		if (oldz > pparams->simorg[2])
			oldz = pparams->simorg[2];
		if (pparams->simorg[2] - oldz > 18)
			oldz = pparams->simorg[2]- 18;
		pparams->vieworg[2] += oldz - pparams->simorg[2];
		view->origin[2] += oldz - pparams->simorg[2];
	}
	else
	{
		oldz = pparams->simorg[2];
	}
#endif

	{
		static float lastorg[3];
		vec3_t delta;

		VectorSubtract( pparams->simorg, lastorg, delta );

		if ( Length( delta ) != 0.0 )
		{
			VectorCopy( pparams->simorg, ViewInterp.Origins[ ViewInterp.CurrentOrigin & ORIGIN_MASK ] );
			ViewInterp.OriginTime[ ViewInterp.CurrentOrigin & ORIGIN_MASK ] = pparams->time;
			ViewInterp.CurrentOrigin++;

			VectorCopy( pparams->simorg, lastorg );
		}
	}

	// Smooth out whole view in multiplayer when on trains, lifts
	if ( cl_vsmoothing && cl_vsmoothing->value &&
		( pparams->smoothing && ( pparams->maxclients > 1 ) ) )
	{
		int foundidx;
		int i;
		float t;

		if ( cl_vsmoothing->value < 0.0 )
		{
			gEngfuncs.Cvar_SetValue( "cl_vsmoothing", 0.0 );
		}

		t = pparams->time - cl_vsmoothing->value;

		for ( i = 1; i < ORIGIN_MASK; i++ )
		{
			foundidx = ViewInterp.CurrentOrigin - 1 - i;
			if ( ViewInterp.OriginTime[ foundidx & ORIGIN_MASK ] <= t )
				break;
		}

		if ( i < ORIGIN_MASK &&  ViewInterp.OriginTime[ foundidx & ORIGIN_MASK ] != 0.0 )
		{
			// Interpolate
			vec3_t delta;
			double frac;
			double dt;
			vec3_t neworg;

			dt = ViewInterp.OriginTime[ (foundidx + 1) & ORIGIN_MASK ] - ViewInterp.OriginTime[ foundidx & ORIGIN_MASK ];
			if ( dt > 0.0 )
			{
				frac = ( t - ViewInterp.OriginTime[ foundidx & ORIGIN_MASK] ) / dt;
				frac = min( 1.0, frac );
				VectorSubtract( ViewInterp.Origins[ ( foundidx + 1 ) & ORIGIN_MASK ], ViewInterp.Origins[ foundidx & ORIGIN_MASK ], delta );
				VectorMA( ViewInterp.Origins[ foundidx & ORIGIN_MASK ], frac, delta, neworg );

				// Dont interpolate large changes
				if ( Length( delta ) < 64 )
				{
					VectorSubtract( neworg, pparams->simorg, delta );

					VectorAdd( pparams->simorg, delta, pparams->simorg );
					VectorAdd( pparams->vieworg, delta, pparams->vieworg );
					VectorAdd( view->origin, delta, view->origin );

				}
			}
		}
	}

	// Store off v_angles before munging for third person
	v_angles = pparams->viewangles;
	v_client_aimangles = pparams->cl_viewangles;
	v_lastAngles = pparams->viewangles;
//	v_cl_angles = pparams->cl_viewangles;	// keep old user mouse angles !
	if ( CL_IsThirdPerson() )
	{
		VectorCopy( camAngles, pparams->viewangles);
	}

	// halflife-updated (halflife #3075): apply this at all times, not only in third person, so the local player's
	// model has the right pitch wherever it is drawn (seen through a trigger_camera or another view entity)
	{
		float pitch = pparams->viewangles[ 0 ];

		// Normalize angles
		if ( pitch > 180 ) 
			pitch -= 360.0;
		else if ( pitch < -180 )
			pitch += 360;

		// Player pitch is inverted
		pitch /= -3.0;

		// Slam local player's pitch value
		ent->angles[ 0 ] = pitch;
		ent->curstate.angles[ 0 ] = pitch;
		ent->prevstate.angles[ 0 ] = pitch;
		ent->latched.prevangles[ 0 ] = pitch;
	}

	// override all previous settings if the viewent isn't the client
	if ( pparams->viewentity > pparams->maxclients )
	{
		cl_entity_t *viewentity;
		viewentity = gEngfuncs.GetEntityByIndex( pparams->viewentity );
		if ( viewentity )
		{
			VectorCopy( viewentity->origin, pparams->vieworg );
			VectorCopy( viewentity->angles, pparams->viewangles );

			// Store off overridden viewangles
			v_angles = pparams->viewangles;
		}
	}

	// HL25: Update the latched view origin/angles here, this was
	// previously done in V_CalcGunAngle but that happens
	// before a bunch of other stuff happens, which nukes
	// a bunch of the viewbob fx.
	VectorCopy( view->origin, view->curstate.origin );
	VectorCopy( view->origin, view->latched.prevorigin );
	VectorCopy( view->angles, view->curstate.angles );
	VectorCopy( view->angles, view->latched.prevangles );

	lasttime = pparams->time;

	v_origin = pparams->vieworg;

	// SBO 2.0: env_mirror: the local player's reflection in first person is drawn where the view is (mirror.cpp)
	if ( !g_iUser1 )
		Mirror_SetLocalView( pparams->simorg, pparams->cl_viewangles );
}

void V_SmoothInterpolateAngles( float * startAngle, float * endAngle, float * finalAngle, float degreesPerSec )
{
	float absd,frac,d,threshhold;
	
	NormalizeAngles( startAngle );
	NormalizeAngles( endAngle );

	for ( int i = 0 ; i < 3 ; i++ )
	{
		d = endAngle[i] - startAngle[i];

		if ( d > 180.0f )
		{
			d -= 360.0f;
		}
		else if ( d < -180.0f )
		{	
			d += 360.0f;
		}

		absd = fabs(d);

		if ( absd > 0.01f )
		{
			frac = degreesPerSec * v_frametime;

			threshhold= degreesPerSec / 4;

			if ( absd < threshhold )
			{
				float h = absd / threshhold;
				h *= h;
				frac*= h;  // slow down last degrees
			}

			if ( frac >  absd )
			{
				finalAngle[i] = endAngle[i];
			}
			else
			{
				if ( d>0)
					finalAngle[i] = startAngle[i] + frac;
				else
					finalAngle[i] = startAngle[i] - frac;
			}
		}
		else
		{
			finalAngle[i] = endAngle[i];
		}

	}

	NormalizeAngles( finalAngle );
}

// Get the origin of the Observer based around the target's position and angles
void V_GetChaseOrigin( float * angles, float * origin, float distance, float * returnvec )
{
	vec3_t	vecEnd;
	vec3_t	forward;
	vec3_t	vecStart;
	pmtrace_t * trace;
	int maxLoops = 8;

	int ignoreent = -1;	// first, ignore no entity
	
	cl_entity_t	 *	ent = NULL;
	
	// Trace back from the target using the player's view angles
	AngleVectors(angles, forward, NULL, NULL);
	
	VectorScale(forward,-1,forward);

	VectorCopy( origin, vecStart );

	VectorMA(vecStart, distance , forward, vecEnd);

	while ( maxLoops > 0)
	{
		trace = gEngfuncs.PM_TraceLine( vecStart, vecEnd, PM_TRACELINE_PHYSENTSONLY, 2, ignoreent );

		// WARNING! trace->ent is is the number in physent list not the normal entity number

		if ( trace->ent <= 0)
			break;	// we hit the world or nothing, stop trace

		ent = gEngfuncs.GetEntityByIndex( PM_GetPhysEntInfo( trace->ent ) );

		if ( ent == NULL )
			break;

		// hit non-player solid BSP , stop here
		if ( ent->curstate.solid == SOLID_BSP && !ent->player ) 
			break;

		// if close enought to end pos, stop, otherwise continue trace
		if( Distance(trace->endpos, vecEnd ) < 1.0f )
		{
			break;
		}
		else
		{
			ignoreent = trace->ent;	// ignore last hit entity
			VectorCopy( trace->endpos, vecStart);
		}

		maxLoops--;
	}  

/*	if ( ent )
	{
		gEngfuncs.Con_Printf("Trace loops %i , entity %i, model %s, solid %i\n",(8-maxLoops),ent->curstate.number, ent->model->name , ent->curstate.solid ); 
	} */

	VectorMA( trace->endpos, 4, trace->plane.normal, returnvec );

	v_lastDistance = Distance(trace->endpos, origin);	// real distance without offset
}

/*void V_GetDeathCam(cl_entity_t * ent1, cl_entity_t * ent2, float * angle, float * origin)
{
	float newAngle[3]; float newOrigin[3]; 

	float distance = 168.0f;

	v_lastDistance+= v_frametime * 96.0f;	// move unit per seconds back

	if ( v_resetCamera )
		v_lastDistance = 64.0f;

	if ( distance > v_lastDistance )
		distance = v_lastDistance;

	VectorCopy(ent1->origin, newOrigin);

	if ( ent1->player )
		newOrigin[2]+= 17; // head level of living player

	// get new angle towards second target
	if ( ent2 )
	{
		VectorSubtract( ent2->origin, ent1->origin, newAngle );
		VectorAngles( newAngle, newAngle );
		newAngle[0] = -newAngle[0];
	}
	else
	{
		// if no second target is given, look down to dead player
		newAngle[0] = 90.0f;
		newAngle[1] = 0.0f;
		newAngle[2] = 0;
	}

	// and smooth view
	V_SmoothInterpolateAngles( v_lastAngles, newAngle, angle, 120.0f );
			
	V_GetChaseOrigin( angle, newOrigin, distance, origin );

	VectorCopy(angle, v_lastAngles);
}*/

void V_GetSingleTargetCam(cl_entity_t * ent1, float * angle, float * origin)
{
	float newAngle[3]; float newOrigin[3]; 
	
	int flags 	   = gHUD.m_Spectator.m_iObserverFlags;

	// see is target is a dead player
	qboolean deadPlayer = ent1->player && (ent1->curstate.solid == SOLID_NOT);
	
	float dfactor   = ( flags & DRC_FLAG_DRAMATIC )? -1.0f : 1.0f;

	float distance = 112.0f + ( 16.0f * dfactor ); // get close if dramatic;
	
	// go away in final scenes or if player just died
	if ( flags & DRC_FLAG_FINAL )
		distance*=2.0f;	
	else if ( deadPlayer )
		distance*=1.5f;	

	// let v_lastDistance float smoothly away
	v_lastDistance+= v_frametime * 32.0f;	// move unit per seconds back

	if ( distance > v_lastDistance )
		distance = v_lastDistance;
	
	VectorCopy(ent1->origin, newOrigin);

	if ( ent1->player )
	{
		if ( deadPlayer )  
			newOrigin[2]+= 2;	//laying on ground
		else
			newOrigin[2]+= 17; // head level of living player
			
	}
	else
		newOrigin[2]+= 8;	// object, tricky, must be above bomb in CS

	// we have no second target, choose view direction based on
	// show front of primary target
	VectorCopy(ent1->angles, newAngle);

	// show dead players from front, normal players back
	if ( flags & DRC_FLAG_FACEPLAYER )
		newAngle[1]+= 180.0f;


	newAngle[0]+= 12.5f * dfactor; // lower angle if dramatic

	// if final scene (bomb), show from real high pos
	if ( flags & DRC_FLAG_FINAL )
		newAngle[0] = 22.5f; 

	// choose side of object/player			
	if ( flags & DRC_FLAG_SIDE )
		newAngle[1]+=22.5f;
	else
		newAngle[1]-=22.5f;

	V_SmoothInterpolateAngles( v_lastAngles, newAngle, angle, 120.0f );

	// HACK, if player is dead don't clip against his dead body, can't check this
	V_GetChaseOrigin( angle, newOrigin, distance, origin );
}

float MaxAngleBetweenAngles(  float * a1, float * a2 )
{
	float d, maxd = 0.0f;

	NormalizeAngles( a1 );
	NormalizeAngles( a2 );

	for ( int i = 0 ; i < 3 ; i++ )
	{
		d = a2[i] - a1[i];
		if ( d > 180 )
		{
			d -= 360;
		}
		else if ( d < -180 )
		{	
			d += 360;
		}

		d = fabs(d);

		if ( d > maxd )
			maxd=d;
	}

	return maxd;
}

void V_GetDoubleTargetsCam(cl_entity_t	 * ent1, cl_entity_t * ent2,float * angle, float * origin)
{
	float newAngle[3]; float newOrigin[3]; float tempVec[3];

	int flags 	   = gHUD.m_Spectator.m_iObserverFlags;

	float dfactor   = ( flags & DRC_FLAG_DRAMATIC )? -1.0f : 1.0f;

	float distance = 112.0f + ( 16.0f * dfactor ); // get close if dramatic;
	
	// go away in final scenes or if player just died
	if ( flags & DRC_FLAG_FINAL )
		distance*=2.0f;	
	
	// let v_lastDistance float smoothly away
	v_lastDistance+= v_frametime * 32.0f;	// move unit per seconds back

	if ( distance > v_lastDistance )
		distance = v_lastDistance;

	VectorCopy(ent1->origin, newOrigin);

	if ( ent1->player )
		newOrigin[2]+= 17; // head level of living player
	else
		newOrigin[2]+= 8;	// object, tricky, must be above bomb in CS

	// get new angle towards second target
	VectorSubtract( ent2->origin, ent1->origin, newAngle );

	VectorAngles( newAngle, newAngle );
	newAngle[0] = -newAngle[0];

	// set angle diffrent in Dramtaic scenes
	newAngle[0]+= 12.5f * dfactor; // lower angle if dramatic
			
	if ( flags & DRC_FLAG_SIDE )
		newAngle[1]+=22.5f;
	else
		newAngle[1]-=22.5f;

	float d = MaxAngleBetweenAngles( v_lastAngles, newAngle );

	if ( ( d < v_cameraFocusAngle) && ( v_cameraMode == CAM_MODE_RELAX ) )
	{
		// difference is to small and we are in relax camera mode, keep viewangles
		VectorCopy(v_lastAngles, newAngle );
	}
	else if ( (d < v_cameraRelaxAngle) && (v_cameraMode == CAM_MODE_FOCUS) )
	{
		// we catched up with our target, relax again
		v_cameraMode = CAM_MODE_RELAX;
	}
	else
	{
		// target move too far away, focus camera again
		v_cameraMode = CAM_MODE_FOCUS;
	}

	// and smooth view, if not a scene cut
	if ( v_resetCamera || (v_cameraMode == CAM_MODE_RELAX) )
	{
		VectorCopy( newAngle, angle );
	}
	else
	{
		V_SmoothInterpolateAngles( v_lastAngles, newAngle, angle, 180.0f );
	}

	V_GetChaseOrigin( newAngle, newOrigin, distance, origin );

	// move position up, if very close at target
	if ( v_lastDistance < 64.0f )
		origin[2]+= 16.0f*( 1.0f - (v_lastDistance / 64.0f ) );

	// calculate angle to second target
	VectorSubtract( ent2->origin, origin, tempVec );
	VectorAngles( tempVec, tempVec );
	tempVec[0] = -tempVec[0];

	/* take middle between two viewangles
	InterpolateAngles( newAngle, tempVec, newAngle, 0.5f); */
	
	

}

void V_GetDirectedChasePosition(cl_entity_t	 * ent1, cl_entity_t * ent2,float * angle, float * origin)
{

	if ( v_resetCamera )
	{
		v_lastDistance = 4096.0f;
		// v_cameraMode = CAM_MODE_FOCUS;
	}

	if ( ( ent2 == (cl_entity_t*)0xFFFFFFFF ) || ( ent1->player && (ent1->curstate.solid == SOLID_NOT) ) )
	{
		// we have no second target or player just died
		V_GetSingleTargetCam(ent1, angle, origin);
	}
	else if ( ent2 )
	{
		// keep both target in view
		V_GetDoubleTargetsCam( ent1, ent2, angle, origin );
	}
	else
	{
		// second target disappeard somehow (dead)

		// keep last good viewangle
		float newOrigin[3];

		int flags 	   = gHUD.m_Spectator.m_iObserverFlags;

		float dfactor   = ( flags & DRC_FLAG_DRAMATIC )? -1.0f : 1.0f;

		float distance = 112.0f + ( 16.0f * dfactor ); // get close if dramatic;
	
		// go away in final scenes or if player just died
		if ( flags & DRC_FLAG_FINAL )
			distance*=2.0f;	
	
		// let v_lastDistance float smoothly away
		v_lastDistance+= v_frametime * 32.0f;	// move unit per seconds back

		if ( distance > v_lastDistance )
			distance = v_lastDistance;
		
		VectorCopy(ent1->origin, newOrigin);

		if ( ent1->player )
			newOrigin[2]+= 17; // head level of living player
		else
			newOrigin[2]+= 8;	// object, tricky, must be above bomb in CS

		V_GetChaseOrigin( angle, newOrigin, distance, origin );
	}

	VectorCopy(angle, v_lastAngles);
}

void V_GetChasePos(int target, float * cl_angles, float * origin, float * angles)
{
	cl_entity_t	 *	ent = NULL;
	
	if ( target ) 
	{
		ent = gEngfuncs.GetEntityByIndex( target );
	};
	
	if (!ent)
	{
		// just copy a save in-map position
		VectorCopy ( vJumpAngles, angles );
		VectorCopy ( vJumpOrigin, origin );
		return;
	}
	
	
	
	if ( gHUD.m_Spectator.m_autoDirector->value )
	{
		if ( g_iUser3 )
			V_GetDirectedChasePosition( ent, gEngfuncs.GetEntityByIndex( g_iUser3 ),
				angles, origin );
		else
			V_GetDirectedChasePosition( ent, ( cl_entity_t*)0xFFFFFFFF,
				angles, origin );
	}
	else
	{
		if ( cl_angles == NULL )	// no mouse angles given, use entity angles ( locked mode )
		{
			VectorCopy ( ent->angles, angles);
			angles[0]*=-1;
		}
		else
			VectorCopy ( cl_angles, angles);


		VectorCopy ( ent->origin, origin);
		
		origin[2]+= 28; // DEFAULT_VIEWHEIGHT - some offset

		V_GetChaseOrigin( angles, origin, cl_chasedist->value, origin );
	}

	v_resetCamera = false;	
}

void V_ResetChaseCam()
{
	v_resetCamera = true;
}


void V_GetInEyePos(int target, float * origin, float * angles )
{
	if ( !target)
	{
		// just copy a save in-map position
		VectorCopy ( vJumpAngles, angles );
		VectorCopy ( vJumpOrigin, origin );
		return;
	};


	cl_entity_t	 * ent = gEngfuncs.GetEntityByIndex( target );

	if ( !ent )
		return;

	VectorCopy ( ent->origin, origin );
	VectorCopy ( ent->angles, angles );

	angles[PITCH]*=-3.0f;	// see CL_ProcessEntityUpdate()

	if ( ent->curstate.solid == SOLID_NOT )
	{
		angles[ROLL] = 80;	// dead view angle
		origin[2]+= -8 ; // PM_DEAD_VIEWHEIGHT
	}
	else if (ent->curstate.usehull == 1 )
		origin[2]+= 12; // VEC_DUCK_VIEW;
	else
		// exacty eye position can't be caluculated since it depends on
		// client values like cl_bobcycle, this offset matches the default values
		origin[2]+= 28; // DEFAULT_VIEWHEIGHT
}

void V_GetMapFreePosition( float * cl_angles, float * origin, float * angles )
{
	vec3_t forward;
	vec3_t zScaledTarget;

	VectorCopy(cl_angles, angles);

	// modify angles since we don't wanna see map's bottom
	angles[0] = 51.25f + 38.75f*(angles[0]/90.0f);

	zScaledTarget[0] = gHUD.m_Spectator.m_mapOrigin[0];
	zScaledTarget[1] = gHUD.m_Spectator.m_mapOrigin[1];
	zScaledTarget[2] = gHUD.m_Spectator.m_mapOrigin[2] * (( 90.0f - angles[0] ) / 90.0f );
	

	AngleVectors(angles, forward, NULL, NULL);

	VectorNormalize(forward);

	VectorMA(zScaledTarget, -( 4096.0f / gHUD.m_Spectator.m_mapZoom ), forward , origin);
}

void V_GetMapChasePosition(int target, float * cl_angles, float * origin, float * angles)
{
	vec3_t forward;

	if ( target )
	{
		cl_entity_t	 *	ent = gEngfuncs.GetEntityByIndex( target );

		if ( gHUD.m_Spectator.m_autoDirector->value )
		{
			// this is done to get the angles made by director mode
			V_GetChasePos(target, cl_angles, origin, angles);
			VectorCopy(ent->origin, origin);
			
			// keep fix chase angle horizontal
			angles[0] = 45.0f;
		}
		else
		{
			VectorCopy(cl_angles, angles);
			VectorCopy(ent->origin, origin);

			// modify angles since we don't wanna see map's bottom
			angles[0] = 51.25f + 38.75f*(angles[0]/90.0f);
		}
	}
	else
	{
		// keep out roaming position, but modify angles
		VectorCopy(cl_angles, angles);
		angles[0] = 51.25f + 38.75f*(angles[0]/90.0f);
	}

	origin[2] *= (( 90.0f - angles[0] ) / 90.0f );
	angles[2] = 0.0f;	// don't roll angle (if chased player is dead)

	AngleVectors(angles, forward, NULL, NULL);

	VectorNormalize(forward);

	VectorMA(origin, -1536, forward, origin); 
}

int V_FindViewModelByWeaponModel(int weaponindex)
{
	// SBO: the SDK mapped a fixed table of Half-Life weapons (and its loop never stopped on an unknown model:
	// "while ( modelmap[i] != NULL )" read the {NULL, NULL} terminator). Every SBO weapon follows the same naming
	// rule, so derive the view model from the player model: "models/p_<name>.mdl" -> "models/v_<name>.mdl".
	// EV_FindModelIndex returns 0 if that model isn't precached (no view model then).
	struct model_s * weaponModel = IEngineStudio.GetModelByIndex( weaponindex );

	if ( !weaponModel )
		return 0;

	char viewName[64];
	strncpy( viewName, weaponModel->name, sizeof( viewName ) - 1 );
	viewName[sizeof( viewName ) - 1] = 0;

	char *slash = strrchr( viewName, '/' );
	char *base = slash ? slash + 1 : viewName;
	if ( ( base[0] != 'p' && base[0] != 'P' ) || base[1] != '_' )
		return 0;

	base[0] = 'v';
	return gEngfuncs.pEventAPI->EV_FindModelIndex( viewName );
}


/*
==================
V_CalcSpectatorRefdef

==================
*/
void V_CalcSpectatorRefdef ( struct ref_params_s * pparams )
{
	static vec3_t			velocity ( 0.0f, 0.0f, 0.0f);

	static int lastWeaponModelIndex = 0;
	static int lastViewModelIndex = 0;
		
	cl_entity_t	 * ent = gEngfuncs.GetEntityByIndex( g_iUser2 );
	
	pparams->onlyClientDraw = false;

	// refresh position
	VectorCopy ( pparams->simorg, v_sim_org );

	// get old values
	VectorCopy ( pparams->cl_viewangles, v_cl_angles );
	VectorCopy ( pparams->viewangles, v_angles );
	VectorCopy ( pparams->vieworg, v_origin );

	if (  ( g_iUser1 == OBS_IN_EYE || gHUD.m_Spectator.InsetMode() == INSET_IN_EYE ) && ent )
	{
		// calculate player velocity
		float timeDiff = ent->curstate.msg_time - ent->prevstate.msg_time;

		if ( timeDiff > 0 )
		{
			vec3_t distance;
			VectorSubtract(ent->prevstate.origin, ent->curstate.origin, distance);
			VectorScale(distance, 1/timeDiff, distance );

			velocity[0] = velocity[0]*0.9f + distance[0]*0.1f;
			velocity[1] = velocity[1]*0.9f + distance[1]*0.1f;
			velocity[2] = velocity[2]*0.9f + distance[2]*0.1f;
			
			VectorCopy(velocity, pparams->simvel);
		}

		// predict missing client data and set weapon model ( in HLTV mode or inset in eye mode )
		// SBO: also for a first-person inset while the main view isn't first person (e.g. a map overview): the
		// server sends the target's client data only in OBS_IN_EYE, so without this the inset used the spectator's
		// own view height (looked crouched) and had no weapon. This is the SDK's own HLTV path.
		if ( gEngfuncs.IsSpectateOnly() || g_iUser1 != OBS_IN_EYE )
		{
			V_GetInEyePos( g_iUser2, pparams->simorg, pparams->cl_viewangles );

			pparams->health = 1;

			cl_entity_t	 * gunModel = gEngfuncs.GetViewModel();

			// SBO 2.0: the target's own view model, which the server sends with its state ("SpecHealth2"). The SDK's
			// guess from the weapon model below finds none when a plugin draws the third-person weapon itself and
			// leaves the player's weapon model empty (production's packed_models_sbo): no weapon in the inset then.
			// The guess stays for HLTV and a server that doesn't send it.
			int iViewModel = gEngfuncs.IsSpectateOnly() ? -1 : VGui2_SpecTargetViewModel( g_iUser2 );

			if ( iViewModel >= 0 )
			{
				if ( lastViewModelIndex != iViewModel )
				{
					lastViewModelIndex = iViewModel;
					if ( lastViewModelIndex )
						gEngfuncs.pfnWeaponAnim(0,0);	// reset weapon animation
				}
				lastWeaponModelIndex = -1;	// the guess runs again if the server's model goes away
			}
			else if ( lastWeaponModelIndex != ent->curstate.weaponmodel )
			{
				// weapon model changed

				lastWeaponModelIndex = ent->curstate.weaponmodel;
				lastViewModelIndex = V_FindViewModelByWeaponModel( lastWeaponModelIndex );
				if ( lastViewModelIndex )
				{
					gEngfuncs.pfnWeaponAnim(0,0);	// reset weapon animation
				}
				else
				{
					// model not found
					gunModel->model = NULL;	// disable weapon model
					lastWeaponModelIndex = lastViewModelIndex = 0;
				}
			}

			if ( lastViewModelIndex )
			{
				gunModel->model = IEngineStudio.GetModelByIndex( lastViewModelIndex );
				gunModel->curstate.modelindex = lastViewModelIndex;
				gunModel->curstate.frame = 0;
				gunModel->curstate.colormap = 0; 
				gunModel->index = g_iUser2;
			}
			else
			{
				gunModel->model = NULL;	// disable weaopn model
			}
		}
		else
		{
			// only get viewangles from entity
			VectorCopy ( ent->angles, pparams->cl_viewangles );
			pparams->cl_viewangles[PITCH]*=-3.0f;	// see CL_ProcessEntityUpdate()
		}
	}
	else
	{
		// SBO 2.0: no first-person view, main or inset: no view model. Every frame (as V_CalcIntermissionRefdef
		// does), not once: after a first-person inset the weapon and arms came back and were drawn floating, at
		// the place of the last first-person view, in the chase inset that followed.
		gEngfuncs.GetViewModel()->model = NULL;
		lastWeaponModelIndex = lastViewModelIndex = 0;
	}

	v_frametime = pparams->frametime;

	if ( pparams->nextView == 0 )
	{
		// first renderer cycle, full screen

		switch ( g_iUser1 )
		{
			case OBS_CHASE_LOCKED:	V_GetChasePos( g_iUser2, NULL, v_origin, v_angles );
									break;

			case OBS_CHASE_FREE:	V_GetChasePos( g_iUser2, v_cl_angles, v_origin, v_angles );
									break;

			case OBS_ROAMING	:	VectorCopy (v_cl_angles, v_angles);
									VectorCopy (v_sim_org, v_origin);

									// Steam SDK: override values if the HLTV director camera is active
									// (DRC_CMD_CAMERA target / DRC_CMD_WAYPOINTS path)
									gHUD.m_Spectator.GetDirectorCamera(v_origin, v_angles);
									break;

			case OBS_IN_EYE		:   V_CalcNormalRefdef ( pparams );
									break;
				
			case OBS_MAP_FREE  :	pparams->onlyClientDraw = true;
									V_GetMapFreePosition( v_cl_angles, v_origin, v_angles );
									break;

			case OBS_MAP_CHASE  :	pparams->onlyClientDraw = true;
									V_GetMapChasePosition( g_iUser2, v_cl_angles, v_origin, v_angles );
									break;
		}

		int iPip, px, py, pw, ph;
		if ( gHUD.m_Spectator.InsetMode() || VGui2_GmPip( iPip, px, py, pw, ph ) )	// SBO 2.0: not while the inset has no target to show
			pparams->nextView = 1;	// force a second renderer view

		gHUD.m_Spectator.m_iDrawCycle = 0;

	}
	else
	{
		// second renderer cycle, inset window

		// SBO 2.0 GM mode: the players window's View: its player in chase locked, in the window's picture area
		int iPip, px, py, pw, ph;
		if ( VGui2_GmPip( iPip, px, py, pw, ph ) )
		{
			pparams->viewport[0] = px;
			pparams->viewport[1] = py;
			pparams->viewport[2] = pw;
			pparams->viewport[3] = ph;
			pparams->nextView = 0;
			V_GetChasePos( iPip, NULL, v_origin, v_angles );
			gHUD.m_Spectator.m_iDrawCycle = 1;
			VectorCopy( v_cl_angles, pparams->cl_viewangles );
			VectorCopy( v_angles, pparams->viewangles );
			VectorCopy( v_origin, pparams->vieworg );
			return;
		}

		// set inset parameters
		pparams->viewport[0] = XRES(gHUD.m_Spectator.m_OverviewData.insetWindowX);	// change viewport to inset window
		pparams->viewport[1] = YRES(gHUD.m_Spectator.m_OverviewData.insetWindowY);
		pparams->viewport[2] = XRES(gHUD.m_Spectator.m_OverviewData.insetWindowWidth);
		pparams->viewport[3] = YRES(gHUD.m_Spectator.m_OverviewData.insetWindowHeight);
		pparams->nextView	 = 0;	// on further view

		// override some settings in certain modes
		switch ( gHUD.m_Spectator.InsetMode() )
		{
			case INSET_CHASE_LOCKED : V_GetChasePos( g_iUser2, NULL, v_origin, v_angles );
									break;

			case INSET_CHASE_FREE : V_GetChasePos( g_iUser2, v_cl_angles, v_origin, v_angles );
									break;	

			case INSET_IN_EYE	 :	// SBO 2.0: simorg is already the target's eye position here (V_GetInEyePos above, the
									// inset path when the main view is not first person): the spectator's own view
									// height on top of it put the inset camera up to 28 units above the eyes
									if ( gEngfuncs.IsSpectateOnly() || g_iUser1 != OBS_IN_EYE )
										VectorClear( pparams->viewheight );
									V_CalcNormalRefdef ( pparams );
									break;

			case INSET_MAP_FREE  :	pparams->onlyClientDraw = true;
									V_GetMapFreePosition( v_cl_angles, v_origin, v_angles );
									break;

			case INSET_MAP_CHASE  :	pparams->onlyClientDraw = true;

									if ( g_iUser1 == OBS_ROAMING )
										V_GetMapChasePosition( 0, v_cl_angles, v_origin, v_angles );
									else
										V_GetMapChasePosition( g_iUser2, v_cl_angles, v_origin, v_angles );

									break;
		}

		gHUD.m_Spectator.m_iDrawCycle = 1;
	}

	// write back new values into pparams
	VectorCopy ( v_cl_angles, pparams->cl_viewangles );
	VectorCopy ( v_angles, pparams->viewangles )
	VectorCopy ( v_origin, pparams->vieworg );

}



/*
==================
V_SkyViewStart / V_SkyViewDone

SoHL (LRC, cl_dll/view.cpp V_CalcNormalRefdef; AJH in SoHL 1.8: the scale) env_sky, the 3D sky. A view is drawn twice
while the sky is on: first from the sky camera in the map's sky room (with the view's angles and field of view), then
the view itself. The engine calls V_CalcRefdef again while nextView is set and clears only the depth buffer between
the two, so where the view has no face (the env_sky tool texture leaves none) the sky room drawn first stays on screen.
The first pass draws the whole sky room PVS from its camera: the sky room's world and the entities in the sky room
(EnvSky_AddEntity below), none of the client's own effects (gHUD.m_bSkyDrawPass). See docs/ENV_SKY.md.
SBO 2.0: around every view that draws the world, not in V_CalcNormalRefdef only: first and third person,
every spectator camera, the picture-in-picture inset, intermission. The view is worked out once, before the sky room
is drawn, and set back as it was for its own pass (SoHL ran the first-person view code a second time).
==================
*/

/*
==================
EnvSky_AddEntity, EnvSky_IsSkyEntity, EnvSky_HideEntities / EnvSky_ShowEntities

SoHL 1.9 (g-cont): the server sends the entities in the sky room to every player (dlls/client.cpp AddToFullPack)
and the sky pass draws them. SoHL drew them in both passes, so the view also showed them where they are, above the
map, through its env_sky faces; and SoHL 1.8 (StudioModelRenderer.cpp StudioDrawModel) drew in the sky pass only
the models marked kRenderFxEntInPVS, because the others are around the view and would be drawn into the sky.
SBO 2.0: the server marks the state of a sky entity with kRenderFxSkyEntityBit (common/const.h); this takes the bit
off again before anything reads renderfx (the mapper's render FX stay as they are) and lists the entity for the
frame. The sky pass draws the sky entities and no other model (GameStudioModelRenderer.cpp R_StudioDrawModel); the
view draws no sky entity: models are left out there too, and brush entities and sprites, which the engine draws
itself from one entity list for both passes, are moved behind the view's camera (out of its view) for the view's
pass and put back after it (HUD_DrawTransparentTriangles).
==================
*/
#define ENVSKY_MAX_ENTITIES	512		// the engine's visible entity list

static bool s_bSkyEntity[ MAX_EDICTS ];		// by entity index: its last state from the server was a sky entity's
static int s_iSkyEntityMsg[ MAX_EDICTS ];	// the state (messagenum) that had the bit
static cl_entity_t *s_pSkyEnts[ ENVSKY_MAX_ENTITIES ];	// this frame's sky entities
static int s_iSkyEnts = 0;
static bool s_bSkyEntsNewFrame = true;		// the next HUD_AddEntity starts the next frame's list
static bool s_bSkyEntsHidden = false;		// moved out of the view being drawn
static vec3_t s_vecSkyEntsHidden;			// where to
static vec3_t s_vecSkyEntOrigin[ ENVSKY_MAX_ENTITIES ];			// and where each was
static vec3_t s_vecSkyEntAttachment[ ENVSKY_MAX_ENTITIES ][ 4 ];	// with its attachments (sprites on them)

static bool EnvSky_AtHidden( const float *v )
{
	return v[ 0 ] == s_vecSkyEntsHidden[ 0 ] && v[ 1 ] == s_vecSkyEntsHidden[ 1 ] && v[ 2 ] == s_vecSkyEntsHidden[ 2 ];
}

void EnvSky_ShowEntities( void )
{
	if ( !s_bSkyEntsHidden )
		return;

	s_bSkyEntsHidden = false;

	for ( int i = 0; i < s_iSkyEnts; i++ )
	{
		cl_entity_t *ent = s_pSkyEnts[ i ];

		// unless the engine has set it again for a new frame
		if ( EnvSky_AtHidden( ent->origin ) )
			VectorCopy( s_vecSkyEntOrigin[ i ], ent->origin );

		for ( int j = 0; j < 4; j++ )
		{
			if ( EnvSky_AtHidden( ent->attachment[ j ] ) )
				VectorCopy( s_vecSkyEntAttachment[ i ][ j ], ent->attachment[ j ] );
		}
	}
}

static void EnvSky_HideEntities( const struct ref_params_s *pparams )
{
	if ( !s_iSkyEnts || s_bSkyEntsHidden )
		return;

	// far behind the camera: outside the view's frustum, whatever the entity's size
	vec3_t forward;
	AngleVectors( pparams->viewangles, forward, NULL, NULL );
	VectorMA( pparams->vieworg, -65536.0f, forward, s_vecSkyEntsHidden );

	for ( int i = 0; i < s_iSkyEnts; i++ )
	{
		cl_entity_t *ent = s_pSkyEnts[ i ];

		VectorCopy( ent->origin, s_vecSkyEntOrigin[ i ] );
		VectorCopy( s_vecSkyEntsHidden, ent->origin );

		for ( int j = 0; j < 4; j++ )
		{
			VectorCopy( ent->attachment[ j ], s_vecSkyEntAttachment[ i ][ j ] );
			VectorCopy( s_vecSkyEntsHidden, ent->attachment[ j ] );
		}
	}

	s_bSkyEntsHidden = true;
}

// HUD_AddEntity (entity.cpp), every entity of every frame
void EnvSky_AddEntity( int type, struct cl_entity_s *ent )
{
	if ( s_bSkyEntsNewFrame )
	{
		EnvSky_ShowEntities();	// in case a view was not ended (HUD_DrawTransparentTriangles not called)
		s_iSkyEnts = 0;
		s_bSkyEntsNewFrame = false;
	}

	const int index = ent->index;

	if ( type != ET_NORMAL || index <= 0 || index >= MAX_EDICTS )
		return;

	// the bit comes with each state from the server (messagenum); between two states curstate keeps it taken off
	if ( ent->curstate.renderfx & kRenderFxSkyEntityBit )
	{
		ent->curstate.renderfx &= ~kRenderFxSkyEntityBit;
		s_bSkyEntity[ index ] = true;
		s_iSkyEntityMsg[ index ] = ent->curstate.messagenum;
	}
	else if ( ent->curstate.messagenum != s_iSkyEntityMsg[ index ] )
		s_bSkyEntity[ index ] = false;

	if ( s_bSkyEntity[ index ] && s_iSkyEnts < ENVSKY_MAX_ENTITIES )
		s_pSkyEnts[ s_iSkyEnts++ ] = ent;
}

// HUD_CreateEntities (entity.cpp), after the frame's HUD_AddEntity calls
void EnvSky_CreateEntities( void )
{
	if ( s_bSkyEntsNewFrame )
		s_iSkyEnts = 0;		// no entity this frame

	s_bSkyEntsNewFrame = true;
}

// GameStudioModelRenderer.cpp: draw this model in the sky pass (and only there)?
bool EnvSky_IsSkyEntity( const struct cl_entity_s *ent )
{
	const int index = ent->index;

	return index > 0 && index < MAX_EDICTS && s_bSkyEntity[ index ] && ent == gEngfuncs.GetEntityByIndex( index );
}

// HUD_VidInit (hud.cpp): a new map
void EnvSky_Reset( void )
{
	EnvSky_ShowEntities();
	memset( s_bSkyEntity, 0, sizeof( s_bSkyEntity ) );
	memset( s_iSkyEntityMsg, 0, sizeof( s_iSkyEntityMsg ) );
	s_iSkyEnts = 0;
	s_bSkyEntsNewFrame = true;
}
static bool s_bSkyViewPending = false;	// the sky room is being drawn: the next call is the view itself
static vec3_t s_vecSkyViewOrigin;		// that view's origin
static int s_iSkyViewNext = 0;			// and its nextView (1: the spectator's inset follows)

void RenderFog( void );		// tri.cpp
void RenderSkyFog( void );	// tri.cpp

static void V_SkyViewStart( struct ref_params_s *pparams )
{
	// the spectator's overview map draws no world
	if ( gHUD.m_iSkyMode != SKY_ON || pparams->onlyClientDraw )
	{
		EnvSky_HideEntities( pparams );	// no sky pass (the sky was just switched off): no sky entity either
		return;
	}

	EnvSky_ShowEntities();

	VectorCopy( pparams->vieworg, s_vecSkyViewOrigin );
	s_iSkyViewNext = pparams->nextView;

	// SoHL 1.8: with a scale the sky camera moves 1/scale of the view's offset from the map origin (parallax)
	if ( gHUD.m_iSkyScale > 0 )
		VectorMA( gHUD.m_vecSkyPos, 1.0f / gHUD.m_iSkyScale, s_vecSkyViewOrigin, pparams->vieworg );
	else
		VectorCopy( gHUD.m_vecSkyPos, pparams->vieworg );

	pparams->nextView = 1;
	s_bSkyViewPending = true;
	gHUD.m_bSkyDrawPass = true;

	// SoHL 1.8: no fog in the sky room, as on the skybox (the fog the Fog message sets leaves the skybox clear)
	RenderSkyFog();
}

static bool V_SkyViewDone( struct ref_params_s *pparams )
{
	if ( !s_bSkyViewPending )
		return false;

	s_bSkyViewPending = false;
	gHUD.m_bSkyDrawPass = false;

	// the view's fog for its triangles and transparent entities (the engine turns it on after the world and models;
	// those get it from the sky room's end, tri.cpp HUD_DrawTransparentTriangles)
	RenderFog();

	// a new frame (the engine starts each one with nextView 0): the last one stopped after its sky room
	if ( !pparams->nextView )
		return false;

	VectorCopy( s_vecSkyViewOrigin, pparams->vieworg );
	pparams->nextView = s_iSkyViewNext;

	// the sky entities were drawn in the sky; the view leaves them out
	EnvSky_HideEntities( pparams );
	return true;
}

/*
==================
SBO 2.0: "ViewModel" message (ValveSoftware/halflife issue #199, Arkshine's proposal; XDM's cl_msg_fx.cpp
__MsgFunc_ViewModel reads the same layout): the server or a plugin sets the render properties of the view model.
byte rendermode, byte renderfx, byte r, byte g, byte b, byte renderamt, byte skin, byte body (docs/VIEWMODEL.md).
The proposal writes the values into the view model entity once; here they are kept and put on it in every
V_CalcRefdef, so nothing the engine writes into it in between (a weapon animation's body) wins over them.
A message without data, or with every value 0, ends the override: the view model gets back what it had without
it (with what the engine changed in it meanwhile). 7 bytes (XDM: no body) leave the body alone.
Cleared for a new server / map (InitHUD, VidInit). Applies to whatever view model is drawn: the player's own, or
the watched player's when spectating in first person (the server sends the watched player's state then).
==================
*/
struct viewmodel_render_t
{
	int rendermode;
	int renderfx;
	color24 rendercolor;
	int renderamt;
	int skin;
	int body;
};

static bool s_bViewModelRender = false;		// a ViewModel override is set
static bool s_bViewModelRenderBody = false;	// and it has a body
static viewmodel_render_t s_ViewModelRender;	// the override
static bool s_bViewModelRenderApplied = false;	// the view model has it
static viewmodel_render_t s_ViewModelRenderBase;	// what the view model has without it (restored at the end)
static viewmodel_render_t s_ViewModelRenderWritten;	// what was put on the view model last

static void V_GetViewModelRender( const cl_entity_t *view, viewmodel_render_t *state )
{
	state->rendermode = view->curstate.rendermode;
	state->renderfx = view->curstate.renderfx;
	state->rendercolor = view->curstate.rendercolor;
	state->renderamt = view->curstate.renderamt;
	state->skin = view->curstate.skin;
	state->body = view->curstate.body;
}

static void V_SetViewModelRender( cl_entity_t *view, const viewmodel_render_t *state )
{
	view->curstate.rendermode = state->rendermode;
	view->curstate.renderfx = state->renderfx;
	view->curstate.rendercolor = state->rendercolor;
	view->curstate.renderamt = state->renderamt;
	view->curstate.skin = state->skin;
	view->curstate.body = state->body;
}

static void V_ApplyViewModelRender( void )
{
	cl_entity_t *view = gEngfuncs.GetViewModel();

	if ( !view )
		return;

	if ( s_bViewModelRenderApplied )
	{
		// what the engine changed since the last write (a weapon animation's body) is the view model's own state now
		viewmodel_render_t current;
		V_GetViewModelRender( view, &current );

		if ( current.rendermode != s_ViewModelRenderWritten.rendermode )
			s_ViewModelRenderBase.rendermode = current.rendermode;
		if ( current.renderfx != s_ViewModelRenderWritten.renderfx )
			s_ViewModelRenderBase.renderfx = current.renderfx;
		if ( current.rendercolor.r != s_ViewModelRenderWritten.rendercolor.r
			|| current.rendercolor.g != s_ViewModelRenderWritten.rendercolor.g
			|| current.rendercolor.b != s_ViewModelRenderWritten.rendercolor.b )
			s_ViewModelRenderBase.rendercolor = current.rendercolor;
		if ( current.renderamt != s_ViewModelRenderWritten.renderamt )
			s_ViewModelRenderBase.renderamt = current.renderamt;
		if ( current.skin != s_ViewModelRenderWritten.skin )
			s_ViewModelRenderBase.skin = current.skin;
		if ( current.body != s_ViewModelRenderWritten.body )
			s_ViewModelRenderBase.body = current.body;
	}
	else if ( s_bViewModelRender )
	{
		V_GetViewModelRender( view, &s_ViewModelRenderBase );
	}

	if ( s_bViewModelRender )
	{
		viewmodel_render_t state = s_ViewModelRender;

		if ( !s_bViewModelRenderBody )
			state.body = s_ViewModelRenderBase.body;

		V_SetViewModelRender( view, &state );
		s_ViewModelRenderWritten = state;
		s_bViewModelRenderApplied = true;
	}
	else if ( s_bViewModelRenderApplied )
	{
		V_SetViewModelRender( view, &s_ViewModelRenderBase );
		s_bViewModelRenderApplied = false;
	}
}

static int V_MsgFunc_ViewModel( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	// no data: the end of the override; fewer than 7 bytes: a layout this client doesn't know, ignored
	if ( iSize == 0 )
	{
		s_bViewModelRender = false;
		return 1;
	}

	if ( iSize < 7 )
		return 1;

	viewmodel_render_t state;
	state.rendermode = READ_BYTE();
	state.renderfx = READ_BYTE();
	state.rendercolor.r = READ_BYTE();
	state.rendercolor.g = READ_BYTE();
	state.rendercolor.b = READ_BYTE();
	state.renderamt = READ_BYTE();
	state.skin = READ_BYTE();
	state.body = ( iSize >= 8 ) ? READ_BYTE() : 0;

	// every value 0 is the view model's default: the end of the override (what a plugin sends to "reset")
	if ( !state.rendermode && !state.renderfx && !state.rendercolor.r && !state.rendercolor.g && !state.rendercolor.b
		&& !state.renderamt && !state.skin && !state.body )
	{
		s_bViewModelRender = false;
		return 1;
	}

	s_ViewModelRender = state;
	s_bViewModelRenderBody = ( iSize >= 8 );
	s_bViewModelRender = true;
	return 1;
}

// a new server / map (the InitHUD message, VidInit): no override; the view model gets back its own state
void V_ResetViewModelRender( void )
{
	s_bViewModelRender = false;
	V_ApplyViewModelRender();
}
void DLLEXPORT V_CalcRefdef( struct ref_params_s *pparams )
{
	// SBO 2.0: color_correction: the view before this one, if it had no view model to grade it after
	ColorCorr_Flush();

	// SoHL env_sky: the sky room of this view is drawn, now the view itself (worked out in the call before)
	if ( V_SkyViewDone( pparams ) )
		return;

	// the first call of a frame is the main view (a second one with nextView set is the spectator PIP inset)
	const bool bMainView = ( pparams->nextView == 0 );

	// SBO 2.0: which view the draw calls that follow belong to (tri.cpp: no client effects on the overview map)
	gHUD.m_Spectator.m_bOverviewDrawPass = false;
	gHUD.m_Spectator.m_bInsetDrawPass = false;

	// intermission / finale rendering
	if ( pparams->intermission )
	{	
		V_CalcIntermissionRefdef ( pparams );	
	}
	else if ( pparams->spectator || g_iUser1 )	// g_iUser true if in spectator mode
	{
		V_CalcSpectatorRefdef ( pparams );	

		// XDM view.cpp V_CalcRefdefOverview (m_OverviewMapDrawPass): only the map modes set onlyClientDraw
		gHUD.m_Spectator.m_bOverviewDrawPass = ( pparams->onlyClientDraw != 0 );
		gHUD.m_Spectator.m_bInsetDrawPass = !bMainView;
	}
	else if ( !pparams->paused )
	{
		// SBO 2.0 mover prediction: a rider's view moves with the mover he stands on as it is drawn
		MoverPrediction_ViewOrigin( pparams->simorg );
		V_CalcNormalRefdef ( pparams );
	}

	// SBO 2.0: the "ViewModel" message's render properties, over what the engine and the views above set
	V_ApplyViewModelRender();

	// halflife-updated PR #245 (HUD scaling): the scaled crosshair is drawn at the centre of the main view
	if ( bMainView )
	{
		HUD_SetRefParams( pparams );

		// XDM view.cpp V_CalcRefdefWorld (g_vecViewOrigin): the main camera picks the layers of the inset overview
		VectorCopy( pparams->vieworg, gHUD.m_Spectator.m_vecMainViewOrigin );

		// SBO 2.0: and the watched player the floor of the full-screen overview
		gHUD.m_Spectator.UpdateWatchedHeight();
	}
#ifdef SBO_TEST_COMMANDS
	if ( bMainView )
		MoveWith_TraceView( pparams->simorg, pparams->vieworg );	// test_cl_mw_trace: the view each frame
#endif

	// SBO 2.0: color_correction: the weights of the draws that follow come from this view's origin (not the sky room's)
	ColorCorr_SetView( pparams->vieworg, gHUD.m_Spectator.m_bOverviewDrawPass, !bMainView );

	// SoHL env_sky: the sky room first, then this view
	V_SkyViewStart( pparams );

/*
// Example of how to overlay the whole screen with red at 50 % alpha
#define SF_TEST
#if defined SF_TEST
	{
		screenfade_t sf;
		gEngfuncs.pfnGetScreenFade( &sf );

		sf.fader = 255;
		sf.fadeg = 0;
		sf.fadeb = 0;
		sf.fadealpha = 128;
		sf.fadeFlags = FFADE_STAYOUT | FFADE_OUT;

		gEngfuncs.pfnSetScreenFade( &sf );
	}
#endif
*/
}

/*
=============
V_DropPunchAngle

=============
*/
void V_DropPunchAngle ( float frametime, float *ev_punchangle )
{
	float	len;
	
	len = VectorNormalize ( ev_punchangle );
	len -= (10.0 + len * 0.5) * frametime;
	len = max( len, 0.0 );
	VectorScale ( ev_punchangle, len, ev_punchangle );
}

/*
=============
V_PunchAxis

Client side punch effect
=============
*/
void V_PunchAxis( int axis, float punch )
{
	ev_punchangle[ axis ] = punch;
}

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	gEngfuncs.pfnAddCommand ("centerview", V_StartPitchDrift );

	scr_ofsx			= gEngfuncs.pfnRegisterVariable( "scr_ofsx","0", 0 );
	scr_ofsy			= gEngfuncs.pfnRegisterVariable( "scr_ofsy","0", 0 );
	scr_ofsz			= gEngfuncs.pfnRegisterVariable( "scr_ofsz","0", 0 );

	v_centermove		= gEngfuncs.pfnRegisterVariable( "v_centermove", "0.15", 0 );
	v_centerspeed		= gEngfuncs.pfnRegisterVariable( "v_centerspeed","500", 0 );

	cl_bobcycle			= gEngfuncs.pfnRegisterVariable( "cl_bobcycle","0.8", 0 );// best default for my experimental gun wag (sjb)
	cl_bob				= gEngfuncs.pfnRegisterVariable( "cl_bob","0.01", FCVAR_ARCHIVE );// best default for my experimental gun wag (sjb); HL25: archived
	cl_bobtilt			= gEngfuncs.pfnRegisterVariable( "cl_bobtilt","0", FCVAR_ARCHIVE );	// halflife-updated
	cl_bobup			= gEngfuncs.pfnRegisterVariable( "cl_bobup","0.5", 0 );
	cl_waterdist		= gEngfuncs.pfnRegisterVariable( "cl_waterdist","4", 0 );
	cl_chasedist		= gEngfuncs.pfnRegisterVariable( "cl_chasedist","112", 0 );

	gEngfuncs.pfnHookUserMsg( "ViewModel", V_MsgFunc_ViewModel );	// SBO 2.0: halflife issue #199
}


//#define TRACE_TEST
#if defined( TRACE_TEST )

extern float in_fov;
/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	float	a;
	float	x;

	if (fov_x < 1 || fov_x > 179)
		fov_x = 90;	// error, set to 90

	x = width/tan(fov_x/360*M_PI);

	a = atan (height/x);

	a = a*360/M_PI;

	return a;
}

int hitent = -1;

void V_Move( int mx, int my )
{
	float fov;
	float fx, fy;
	float dx, dy;
	float c_x, c_y;
	float dX, dY;
	vec3_t forward, up, right;
	vec3_t newangles;

	vec3_t farpoint;
	pmtrace_t tr;

	fov = CalcFov( in_fov, (float)ScreenWidth, (float)ScreenHeight );

	c_x = (float)ScreenWidth / 2.0;
	c_y = (float)ScreenHeight / 2.0;

	dx = (float)mx - c_x;
	dy = (float)my - c_y;

	// Proportion we moved in each direction
	fx = dx / c_x;
	fy = dy / c_y;

	dX = fx * in_fov / 2.0 ;
	dY = fy * fov / 2.0;

	newangles = v_angles;

	newangles[ YAW ] -= dX;
	newangles[ PITCH ] += dY;

	// Now rotate v_forward around that point
	AngleVectors ( newangles, forward, right, up );

	farpoint = v_origin + 8192 * forward;

	// Trace
	tr = *(gEngfuncs.PM_TraceLine( (float *)&v_origin, (float *)&farpoint, PM_TRACELINE_PHYSENTSONLY, 2 /*point sized hull*/, -1 ));

	if ( tr.fraction != 1.0 && tr.ent != 0 )
	{
		hitent = PM_GetPhysEntInfo( tr.ent );
		PM_ParticleLine( (float *)&v_origin, (float *)&tr.endpos, 5, 1.0, 0.0 );
	}
	else
	{
		hitent = -1;
	}
}

#endif
