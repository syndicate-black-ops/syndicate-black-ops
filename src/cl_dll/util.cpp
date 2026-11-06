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
//
// util.cpp
//
// implementation of class-less helper functions
//

#include "stdio.h"
#include "stdlib.h"
#include "math.h"

#include "hud.h"
#include "cl_util.h"
#include <string.h>

#include "const.h"
#include "com_model.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

vec3_t vec3_origin( 0, 0, 0 );

double sqrt(double x);

float Length(const float *v)
{
	int		i;
	float	length;
	
	length = 0;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = sqrt (length);		// FIXME

	return length;
}

void VectorAngles( const float *forward, float *angles )
{
	float	tmp, yaw, pitch;
	
	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = sqrt (forward[0]*forward[0] + forward[1]*forward[1]);
		pitch = (atan2(forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}
	
	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}

float VectorNormalize (float *v)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;

}

void VectorInverse ( float *v )
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void VectorScale (const float *in, float scale, float *out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}

void VectorMA (const float *veca, float scale, const float *vecb, float *vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}

HSPRITE LoadSprite(const char *pszName)
{
	int i;
	char sz[256]; 

	if (ScreenWidth < 640)
		i = 320;
	else
		i = 640;

	sprintf(sz, pszName, i);

	return SPR_Load(sz);
}

//
// SBO 2.0: HUD sprite slots.
//
// The engine keeps the sprites loaded with SPR_Load in one list of MAX_HUD_SPRITES entries (sprite, name, frame
// count) and a handle is the entry's index + 1. Loading a name that is already in the list returns its handle
// again; a load that fails returns 0 and leaves its entry free for the next new name. The list lives for as long
// as the client is connected, map changes included, and is freed on disconnect. One sprite more than the list
// holds is a fatal error ("cannot allocate more than 256 HUD sprites").
// GetSpritePointer returns the sprite of an entry (NULL for a free one or a handle out of range), so the list
// can be read back exactly; only the loads that failed have to be remembered here.
//
#define MAX_FAILED_SPRITES	64	// names remembered for the list; more than this are not listed

static char s_szFailedSprites[MAX_FAILED_SPRITES][64];
static int s_iFailedSprites = 0;

HSPRITE SPR_Load( const char *szPicName )
{
	const HSPRITE hSprite = gEngfuncs.pfnSPR_Load( szPicName );

	if ( !szPicName || !szPicName[0] )
		return hSprite;

	int i;
	for ( i = 0; i < s_iFailedSprites; i++ )
	{
		if ( !stricmp( s_szFailedSprites[i], szPicName ) )
			break;
	}

	if ( hSprite )
	{
		// loaded now: no longer a failed one
		if ( i < s_iFailedSprites )
		{
			s_iFailedSprites--;
			memmove( s_szFailedSprites[i], s_szFailedSprites[i + 1], ( s_iFailedSprites - i ) * sizeof( s_szFailedSprites[0] ) );
		}
	}
	else if ( i == s_iFailedSprites && s_iFailedSprites < MAX_FAILED_SPRITES )
	{
		strncpy( s_szFailedSprites[i], szPicName, sizeof( s_szFailedSprites[0] ) - 1 );
		s_szFailedSprites[i][sizeof( s_szFailedSprites[0] ) - 1] = 0;
		s_iFailedSprites++;
	}

	return hSprite;
}

// "hudspritelist": the engine's HUD sprite list, one line per slot in use, and the count against the limit
void HudSpriteList_f( void )
{
	int iUsed = 0;

	for ( int h = 1; h <= MAX_HUD_SPRITES; h++ )
	{
		const struct model_s *pModel = gEngfuncs.GetSpritePointer( h );
		if ( !pModel )
			continue;

		gEngfuncs.Con_Printf( "%3d  %s  %dx%d  %d\n", h, pModel->name,
			gEngfuncs.pfnSPR_Width( h, 0 ), gEngfuncs.pfnSPR_Height( h, 0 ), gEngfuncs.pfnSPR_Frames( h ) );
		iUsed++;
	}

	for ( int i = 0; i < s_iFailedSprites; i++ )
		gEngfuncs.Con_Printf( "  -  %s  failed to load\n", s_szFailedSprites[i] );

	if ( iUsed )
		gEngfuncs.Con_Printf( "(slot, file, width x height, frames)\n" );

	gEngfuncs.Con_Printf( "%d of %d HUD sprites in use, %d free", iUsed, MAX_HUD_SPRITES, MAX_HUD_SPRITES - iUsed );
	if ( s_iFailedSprites )
		gEngfuncs.Con_Printf( ", %d failed to load", s_iFailedSprites );
	if ( !iUsed )
		gEngfuncs.Con_Printf( " (the list is empty until a game is joined)" );
	gEngfuncs.Con_Printf( "\n" );
}
