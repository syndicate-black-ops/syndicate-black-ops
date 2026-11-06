/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/nowin.h"

typedef int BOOL;
#define TRUE	 1	
#define FALSE	0

// hack into header files that we can ship
typedef int qboolean;
typedef unsigned char byte;
#include "utils_mathlib.h"
#include "const.h"
#include "progdefs.h"
#include "edict.h"
#include "eiface.h"

#include "studio.h"

#ifndef ACTIVITY_H
#include "activity.h"
#endif

#include "activitymap.h"

#ifndef ANIMATION_H
#include "animation.h"
#endif

#ifndef SCRIPTEVENT_H
#include "scriptevent.h"
#endif

#ifndef ENGINECALLBACK_H
#include "enginecallback.h"
#endif

extern globalvars_t				*gpGlobals;

#pragma warning( disable : 4244 )

/*
SBO 2.0: env_sprite / cycler_sprite with a studio model (docs/MODEL_SCALE.md). The entity's studio header when its
model is a studio model: the name ends in ".mdl" and the engine has it as one (the header's ident); NULL otherwise.
A sprite is never asked for its data (the name test comes first).
*/
void *GetStudioModelPtr( edict_t *pent )
{
	if ( !pent || !pent->v.model || !pent->v.modelindex )
		return NULL;

	const char *pszName = gpGlobals->pStringBase + pent->v.model;
	size_t len = strlen( pszName );
	if ( len < 4 || pszName[len - 4] != '.' || ( pszName[len - 3] | 0x20 ) != 'm' || ( pszName[len - 2] | 0x20 ) != 'd' ||
		 ( pszName[len - 1] | 0x20 ) != 'l' )
		return NULL;

	studiohdr_t *pstudiohdr = (studiohdr_t *)GET_MODEL_PTR( pent );
	if ( !pstudiohdr || pstudiohdr->id != ( ( 'T' << 24 ) + ( 'S' << 16 ) + ( 'D' << 8 ) + 'I' ) )	// "IDST"
		return NULL;

	return pstudiohdr;
}

/*
SBO 2.0: the frame (0 .. 255 of the sequence) the client draws now for an entity animated by animtime and framerate
alone, as the client's StudioEstimateFrame works it out (looping sequences wrap, the others stop at the end)
*/
float StudioCurrentFrame( void *pmodel, entvars_t *pev )
{
	float flFrameRate = 0, flGroundSpeed = 0;

	GetSequenceInfo( pmodel, pev, &flFrameRate, &flGroundSpeed );

	float flFrame = pev->frame;
	if ( pev->animtime && gpGlobals->time > pev->animtime )
		flFrame += ( gpGlobals->time - pev->animtime ) * pev->framerate * flFrameRate;

	if ( GetSequenceFlags( pmodel, pev ) & STUDIO_LOOPING )
	{
		flFrame -= (int)( flFrame / 256.0 ) * 256.0;
		if ( flFrame < 0 )
			flFrame += 256.0;
	}
	else if ( flFrame > 255.0 )
		flFrame = 255.0;
	else if ( flFrame < 0 )
		flFrame = 0;

	return flFrame;
}



int ExtractBbox( void *pmodel, int sequence, float *mins, float *maxs )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return 0;

	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	
	mins[0] = pseqdesc[ sequence ].bbmin[0];
	mins[1] = pseqdesc[ sequence ].bbmin[1];
	mins[2] = pseqdesc[ sequence ].bbmin[2];

	maxs[0] = pseqdesc[ sequence ].bbmax[0];
	maxs[1] = pseqdesc[ sequence ].bbmax[1];
	maxs[2] = pseqdesc[ sequence ].bbmax[2];

	return 1;
}


int LookupActivity( void *pmodel, entvars_t *pev, int activity )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return 0;

	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	int weighttotal = 0;
	int seq = ACTIVITY_NOT_AVAILABLE;
	for (int i = 0; i < pstudiohdr->numseq; i++)
	{
		if (pseqdesc[i].activity == activity)
		{
			weighttotal += pseqdesc[i].actweight;
			if (!weighttotal || RANDOM_LONG(0,weighttotal-1) < pseqdesc[i].actweight)
				seq = i;
		}
	}

	return seq;
}


int LookupActivityHeaviest( void *pmodel, entvars_t *pev, int activity )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if ( !pstudiohdr )
		return 0;

	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	int weight = 0;
	int seq = ACTIVITY_NOT_AVAILABLE;
	for (int i = 0; i < pstudiohdr->numseq; i++)
	{
		if (pseqdesc[i].activity == activity)
		{
			if ( pseqdesc[i].actweight > weight )
			{
				weight = pseqdesc[i].actweight;
				seq = i;
			}
		}
	}

	return seq;
}

void GetEyePosition ( void *pmodel, float *vecEyePosition )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;

	if ( !pstudiohdr )
	{
		ALERT ( at_console, "GetEyePosition() Can't get pstudiohdr ptr!\n" );
		return;
	}

	VectorCopy ( pstudiohdr->eyeposition, vecEyePosition );
}

int LookupSequence( void *pmodel, const char *label )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return 0;

	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	for (int i = 0; i < pstudiohdr->numseq; i++)
	{
		if (stricmp( pseqdesc[i].label, label ) == 0)
			return i;
	}

	return -1;
}


int IsSoundEvent( int eventNumber )
{
	if ( eventNumber == SCRIPT_EVENT_SOUND || eventNumber == SCRIPT_EVENT_SOUND_VOICE )
		return 1;
	return 0;
}


void SequencePrecache( void *pmodel, const char *pSequenceName )
{
	int index = LookupSequence( pmodel, pSequenceName );
	if ( index >= 0 )
	{
		studiohdr_t *pstudiohdr;
	
		pstudiohdr = (studiohdr_t *)pmodel;
		if ( !pstudiohdr || index >= pstudiohdr->numseq )
			return;

		mstudioseqdesc_t	*pseqdesc;
		mstudioevent_t		*pevent;

		pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + index;
		pevent = (mstudioevent_t *)((byte *)pstudiohdr + pseqdesc->eventindex);

		for (int i = 0; i < pseqdesc->numevents; i++)
		{
			// Don't send client-side events to the server AI
			if ( pevent[i].event >= EVENT_CLIENT )
				continue;

			// UNDONE: Add a callback to check to see if a sound is precached yet and don't allocate a copy
			// of it's name if it is.
			if ( IsSoundEvent( pevent[i].event ) )
			{
				if ( !strlen(pevent[i].options) )
				{
					ALERT( at_error, "Bad sound event %d in sequence %s :: %s (sound is \"%s\")\n", pevent[i].event, pstudiohdr->name, pSequenceName, pevent[i].options );
				}

				PRECACHE_SOUND( (char *)(gpGlobals->pStringBase + ALLOC_STRING(pevent[i].options) ) );
			}
		}
	}
}



void GetSequenceInfo( void *pmodel, entvars_t *pev, float *pflFrameRate, float *pflGroundSpeed )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return;

	mstudioseqdesc_t	*pseqdesc;

	if (pev->sequence < 0 || pev->sequence >= pstudiohdr->numseq) // halflife-updated (VS#3308): LookupSequence returns -1
	{
		*pflFrameRate = 0.0;
		*pflGroundSpeed = 0.0;
		return;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;

	if (pseqdesc->numframes > 1)
	{
		*pflFrameRate = 256 * pseqdesc->fps / (pseqdesc->numframes - 1);
		*pflGroundSpeed = sqrt( pseqdesc->linearmovement[0]*pseqdesc->linearmovement[0]+ pseqdesc->linearmovement[1]*pseqdesc->linearmovement[1]+ pseqdesc->linearmovement[2]*pseqdesc->linearmovement[2] );
		*pflGroundSpeed = *pflGroundSpeed * pseqdesc->fps / (pseqdesc->numframes - 1);
	}
	else
	{
		*pflFrameRate = 256.0;
		*pflGroundSpeed = 0.0;
	}
}


int GetSequenceFlags( void *pmodel, entvars_t *pev )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if ( !pstudiohdr || pev->sequence < 0 || pev->sequence >= pstudiohdr->numseq ) // halflife-updated (VS#3308)
		return 0;

	mstudioseqdesc_t	*pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;

	return pseqdesc->flags;
}


int GetAnimationEvent( void *pmodel, entvars_t *pev, MonsterEvent_t *pMonsterEvent, float flStart, float flEnd, int index )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if ( !pstudiohdr || pev->sequence < 0 || pev->sequence >= pstudiohdr->numseq || !pMonsterEvent ) // halflife-updated (VS#3308)
		return 0;

	int events = 0;

	mstudioseqdesc_t	*pseqdesc;
	mstudioevent_t		*pevent;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;
	pevent = (mstudioevent_t *)((byte *)pstudiohdr + pseqdesc->eventindex);

	if (pseqdesc->numevents == 0 || index > pseqdesc->numevents )
		return 0;

	if (pseqdesc->numframes > 1)
	{
		flStart *= (pseqdesc->numframes - 1) / 256.0;
		flEnd *= (pseqdesc->numframes - 1) / 256.0;
	}
	else
	{
		flStart = 0;
		flEnd = 1.0;
	}

	for (; index < pseqdesc->numevents; index++)
	{
		// Don't send client-side events to the server AI
		if ( pevent[index].event >= EVENT_CLIENT )
			continue;

		if ( (pevent[index].frame >= flStart && pevent[index].frame < flEnd) || 
			((pseqdesc->flags & STUDIO_LOOPING) && flEnd >= pseqdesc->numframes - 1 && pevent[index].frame < flEnd - pseqdesc->numframes + 1) )
		{
			pMonsterEvent->event = pevent[index].event;
			pMonsterEvent->options = pevent[index].options;
			return index + 1;
		}
	}
	return 0;
}

float SetController( void *pmodel, entvars_t *pev, int iController, float flValue )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return flValue;

	mstudiobonecontroller_t	*pbonecontroller = (mstudiobonecontroller_t *)((byte *)pstudiohdr + pstudiohdr->bonecontrollerindex);

	// find first controller that matches the index
	int i; for (i = 0; i < pstudiohdr->numbonecontrollers; i++, pbonecontroller++)
	{
		if (pbonecontroller->index == iController)
			break;
	}
	if (i >= pstudiohdr->numbonecontrollers)
		return flValue;

	// wrap 0..360 if it's a rotational controller

	if (pbonecontroller->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
	{
		// ugly hack, invert value if end < start
		if (pbonecontroller->end < pbonecontroller->start)
			flValue = -flValue;

		// does the controller not wrap?
		if (pbonecontroller->start + 359.0 >= pbonecontroller->end)
		{
			if (flValue > ((pbonecontroller->start + pbonecontroller->end) / 2.0) + 180)
				flValue = flValue - 360;
			if (flValue < ((pbonecontroller->start + pbonecontroller->end) / 2.0) - 180)
				flValue = flValue + 360;
		}
		else
		{
			if (flValue > 360)
				flValue = flValue - (int)(flValue / 360.0) * 360.0;
			else if (flValue < 0)
				flValue = flValue + (int)((flValue / -360.0) + 1) * 360.0;
		}
	}

	int setting = 255 * (flValue - pbonecontroller->start) / (pbonecontroller->end - pbonecontroller->start);

	if (setting < 0) setting = 0;
	if (setting > 255) setting = 255;
	pev->controller[iController] = setting;

	return setting * (1.0 / 255.0) * (pbonecontroller->end - pbonecontroller->start) + pbonecontroller->start;
}


float SetBlending( void *pmodel, entvars_t *pev, int iBlender, float flValue )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr || pev->sequence < 0 || pev->sequence >= pstudiohdr->numseq) // halflife-updated (VS#3308)
		return flValue;

	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;

	if (pseqdesc->blendtype[iBlender] == 0)
		return flValue;

	if (pseqdesc->blendtype[iBlender] & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
	{
		// ugly hack, invert value if end < start
		if (pseqdesc->blendend[iBlender] < pseqdesc->blendstart[iBlender])
			flValue = -flValue;

		// does the controller not wrap?
		if (pseqdesc->blendstart[iBlender] + 359.0 >= pseqdesc->blendend[iBlender])
		{
			if (flValue > ((pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender]) / 2.0) + 180)
				flValue = flValue - 360;
			if (flValue < ((pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender]) / 2.0) - 180)
				flValue = flValue + 360;
		}
	}

	int setting = 255 * (flValue - pseqdesc->blendstart[iBlender]) / (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]);

	if (setting < 0) setting = 0;
	if (setting > 255) setting = 255;

	pev->blending[iBlender] = setting;

	return setting * (1.0 / 255.0) * (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]) + pseqdesc->blendstart[iBlender];
}




int FindTransition( void *pmodel, int iEndingAnim, int iGoalAnim, int *piDir )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return iGoalAnim;

	mstudioseqdesc_t	*pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	// bail if we're going to or from a node 0
	if (pseqdesc[iEndingAnim].entrynode == 0 || pseqdesc[iGoalAnim].entrynode == 0)
	{
		return iGoalAnim;
	}

	int	iEndNode;

	// ALERT( at_console, "from %d to %d: ", pEndNode->iEndNode, pGoalNode->iStartNode );

	if (*piDir > 0)
	{
		iEndNode = pseqdesc[iEndingAnim].exitnode;
	}
	else
	{
		iEndNode = pseqdesc[iEndingAnim].entrynode;
	}

	if (iEndNode == pseqdesc[iGoalAnim].entrynode)
	{
		*piDir = 1;
		return iGoalAnim;
	}

	byte *pTransition = ((byte *)pstudiohdr + pstudiohdr->transitionindex);

	int iInternNode = pTransition[(iEndNode-1)*pstudiohdr->numtransitions + (pseqdesc[iGoalAnim].entrynode-1)];

	if (iInternNode == 0)
		return iGoalAnim;

	int i;

	// look for someone going
	for (i = 0; i < pstudiohdr->numseq; i++)
	{
		if (pseqdesc[i].entrynode == iEndNode && pseqdesc[i].exitnode == iInternNode)
		{
			*piDir = 1;
			return i;
		}
		if (pseqdesc[i].nodeflags)
		{
			if (pseqdesc[i].exitnode == iEndNode && pseqdesc[i].entrynode == iInternNode)
			{
				*piDir = -1;
				return i;
			}
		}
	}

	ALERT( at_console, "error in transition graph" );
	return iGoalAnim;
}

void SetBodygroup( void *pmodel, entvars_t *pev, int iGroup, int iValue )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return;

	if (iGroup > pstudiohdr->numbodyparts)
		return;

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex) + iGroup;

	if (iValue >= pbodypart->nummodels)
		return;

	int iCurrent = (pev->body / pbodypart->base) % pbodypart->nummodels;

	pev->body = (pev->body - (iCurrent * pbodypart->base) + (iValue * pbodypart->base));
}


int GetBodygroup( void *pmodel, entvars_t *pev, int iGroup )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return 0;

	if (iGroup > pstudiohdr->numbodyparts)
		return 0;

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex) + iGroup;

	if (pbodypart->nummodels <= 1)
		return 0;

	int iCurrent = (pev->body / pbodypart->base) % pbodypart->nummodels;

	return iCurrent;
}

/*
SBO 2.0 model scale: hitboxes, server attachments and bone positions (docs/MODEL_SCALE.md "Collision box and
hitboxes").

The engine sets up a studio model's bones on the server through a blending interface: for the hitboxes a bullet
(a point trace) is tested against, and for GetAttachment / GetBonePosition. A game DLL may export
Server_GetBlendingInterface to replace it (Counter-Strike does, for its player blending; Metamod passes the export
on to the game DLL). The engine hands in its own interface (the one it uses unless the call returns 1), its bone
and rotation matrices and its server studio API (r_studioint.h server_studio_api_t, passed as
engine_studio_api_s).

SBO keeps the engine's own bone setup and only scales its result: SV_StudioSetupBonesScaled calls the engine's
function, so at scale 1 (every player without a head scale and every entity without the key) the bones, the hitboxes
and the attachments are exactly the engine's, at the cost of one call and two tests. A player's head scale
("headscale", head_scale.cpp) scales his head bones around the head bone's origin the same way (below). For a scaled entity the bones are then scaled around
the origin as the client draws them (StudioSetUpTransform): every bone's position moves to origin + scale x (its
position - origin) and

- for an attachment or a bone position (the engine asks for one bone and its parents) the bone's axes are
  multiplied by the scale: the attachment's offset from its bone scales, as the client's StudioCalcAttachments does;
- for the hitboxes (the engine asks for all bones, iBone -1) they are divided by it: the engine makes each hitbox's
  six planes from the bone's axes (the normals) with the distance axis . bone position + the hitbox's bbmin / bbmax
  (mstudiobbox_t, in model units), so axes of length 1/scale put the planes at scale x bbmin / bbmax from the
  scaled bone, around the same point. A trace that ends on such a plane gets a normal of length 1/scale:
  UTIL_TraceLine gives it length 1 again.

The scale is the one the clients get (UTIL_ModelScaleValue: entity_state_t.scale, 1/256 steps).
*/

#include "r_studioint.h"

#ifdef _WIN32
#define SV_BLENDING_EXPORT	extern "C" __declspec( dllexport )
#else
#define SV_BLENDING_EXPORT	extern "C"
#endif

// a scale as the clients get it in entity_state_t.scale (delta.lst: 16 bits, 1/256 steps, cut off) and draw it:
// 1 when that is 0 (client StudioModelScale). Also util.cpp's UTIL_ModelScale (the collision box)
float UTIL_ModelScaleValue( float flScale )
{
	if ( flScale <= 0.0f || flScale == 1.0f || flScale >= 16777216.0f )
		return 1.0f;

	flScale = (float)( (unsigned int)( flScale * 256.0f ) & 0xFFFF ) / 256.0f;

	return flScale > 0.0f ? flScale : 1.0f;
}

float UTIL_ModelScale( const entvars_t *pev );	// util.cpp
float HeadScale_ForEdict( const edict_t *pEdict );	// head_scale.cpp: a player's head scale, 1 for everything else

#include "../game_shared/head_scale_shared.h"

static int g_iHeadBoneCache = -1;	// HeadScale_FindBone: the head bone's index in the last model asked

static sv_blending_interface_t	*g_pEngineBlending;		// the engine's own bone setup
static server_studio_api_t		g_ServerStudio;
static float					(*g_pBoneTransform)[MAXSTUDIOBONES][3][4];
static int						g_iScaledBonesCalls;	// developer: the first call from the engine is printed

static void SV_StudioScaleBone( float (*bone)[4], const float *origin, float flAxisScale, float flScale )
{
	for ( int i = 0; i < 3; i++ )
	{
		bone[i][0] *= flAxisScale;
		bone[i][1] *= flAxisScale;
		bone[i][2] *= flAxisScale;
		bone[i][3] = origin[i] + ( bone[i][3] - origin[i] ) * flScale;
	}
}

static void SV_StudioSetupBonesScaled( struct model_s *pModel, float frame, int sequence, const vec3_t angles,
	const vec3_t origin, const byte *pcontroller, const byte *pblending, int iBone, const edict_t *pEdict )
{
	g_pEngineBlending->SV_StudioSetupBones( pModel, frame, sequence, angles, origin, pcontroller, pblending, iBone, pEdict );

	if ( !g_iScaledBonesCalls )
	{
		g_iScaledBonesCalls = 1;
		ALERT( at_console, "SV_StudioSetupBones: the engine sets up server bones through the game DLL (model scale)\n" );
	}

	if ( !pEdict )
		return;

	// only the classes whose box follows the scale (util.cpp UTIL_ModelScale): others keep stock hitboxes. During an
	// env_render scale fade it is the scale the clients draw now (render_fade.cpp RenderFade_ScaleNow)
	float flScale = UTIL_ModelScale( &pEdict->v );
	// a player's head ("headscale", head_scale.cpp; 1 for everything else)
	float flHead = HeadScale_ForEdict( pEdict );
	if ( flScale == 1.0f && flHead == 1.0f )
		return;

	studiohdr_t *pstudiohdr = (studiohdr_t *)g_ServerStudio.Mod_Extradata( pModel );
	if ( !pstudiohdr || pstudiohdr->numbones <= 0 || pstudiohdr->numbones > MAXSTUDIOBONES )
		return;

	float (*bones)[3][4] = *g_pBoneTransform;

	if ( iBone == -1 )
	{
		// the hitboxes: every bone
		if ( flScale != 1.0f )
		{
			float flInv = 1.0f / flScale;
			for ( int i = 0; i < pstudiohdr->numbones; i++ )
				SV_StudioScaleBone( bones[i], origin, flInv, flScale );
		}
		// the head and the bones below it around the head bone's origin, axes of length 1 / scale for the hitbox
		// planes as above (game_shared/head_scale_shared.h)
		if ( flHead != 1.0f )
			HeadScale_ScaleBones( bones, pstudiohdr, HeadScale_FindBone( pstudiohdr, &g_iHeadBoneCache ), flHead,
				1.0f / flHead, NULL );
		return;
	}

	// one bone and its parents (the engine sets up bone 0 for an index out of range)
	if ( iBone < -1 || iBone >= pstudiohdr->numbones )
		iBone = 0;

	bool bChain[ MAXSTUDIOBONES ];
	memset( bChain, 0, sizeof( bChain ) );

	mstudiobone_t *pbones = (mstudiobone_t *)( (byte *)pstudiohdr + pstudiohdr->boneindex );
	for ( int n = 0; iBone >= 0 && iBone < pstudiohdr->numbones && n < MAXSTUDIOBONES; n++ )
	{
		if ( flScale != 1.0f )
			SV_StudioScaleBone( bones[iBone], origin, flScale, flScale );
		bChain[iBone] = true;
		iBone = pbones[iBone].parent;
	}

	// an attachment or a bone position on the head: where the client draws it (axes times the scale)
	if ( flHead != 1.0f )
		HeadScale_ScaleBones( bones, pstudiohdr, HeadScale_FindBone( pstudiohdr, &g_iHeadBoneCache ), flHead, flHead,
			bChain );
}

static sv_blending_interface_t g_ScaledBlending =
{
	SV_BLENDING_INTERFACE_VERSION,
	SV_StudioSetupBonesScaled
};

SV_BLENDING_EXPORT int Server_GetBlendingInterface( int version, struct sv_blending_interface_s **ppinterface,
	struct engine_studio_api_s *pstudio, float (*rotationmatrix)[3][4], float (*bonetransform)[MAXSTUDIOBONES][3][4] )
{
	if ( version != SV_BLENDING_INTERFACE_VERSION || !ppinterface || !pstudio || !bonetransform )
		return 0;

	// the engine's interface: the one it passes in (its default). Asked again with ours already in place: keep
	// the one taken the first time
	sv_blending_interface_t *pEngine = *ppinterface;
	if ( pEngine == &g_ScaledBlending )
		pEngine = g_pEngineBlending;

	if ( !pEngine || pEngine->version != SV_BLENDING_INTERFACE_VERSION || !pEngine->SV_StudioSetupBones )
	{
		ALERT( at_console, "Server_GetBlendingInterface: no engine bone setup handed in; hitboxes stay at scale 1\n" );
		return 0;
	}

	// the server's studio API (four functions), passed as the client's type
	server_studio_api_t *pServerStudio = (server_studio_api_t *)pstudio;
	if ( !pServerStudio->Mod_Extradata )
		return 0;

	g_pEngineBlending = pEngine;
	g_ServerStudio = *pServerStudio;
	g_pBoneTransform = bonetransform;

	*ppinterface = &g_ScaledBlending;

	ALERT( at_console, "Server_GetBlendingInterface: server bone setup taken over (model scale hitboxes)\n" );
	return 1;
}
