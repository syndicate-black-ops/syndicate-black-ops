//=========================================================
// SBO 2.0 env_dlight, client side: see dlights.h, game_shared/dlight_shared.h and docs/ENV_DLIGHT.md.
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) commit 30fc2f71:
// cl_dll/hud.cpp KeyedDLightManager, cl_dll/hud_msg.cpp MsgFunc_KeyedDLight, the Update call in cl_dll/entity.cpp
// HUD_CreateEntities. Each env_dlight that is on is one engine dlight under the env_dlight's entity index as key,
// alive until switched off.
//
// SBO changes: a light switched on again (the same key) takes its old slot instead of a second one; a slot whose
// dlight the engine took back (key 0, or past its die time) is freed; the lights are forgotten at VidInit.
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "cl_entity.h"
#include "r_efx.h"
#include "dlight.h"
#include "dlights.h"
#include "dlight_shared.h"

#include <string.h>

#define DLIGHTS_CLIENT_MAX	32		// the engine's dlights (MAX_DLIGHTS)

struct client_dlight_t
{
	dlight_t	*dl;		// NULL: free
	int			key;
	int			entindex;	// follow this entity (0: none)
};

static client_dlight_t s_Lights[ DLIGHTS_CLIENT_MAX ];

static client_dlight_t *DLights_Find( int key )
{
	for ( int i = 0; i < DLIGHTS_CLIENT_MAX; i++ )
	{
		if ( s_Lights[i].dl && s_Lights[i].key == key )
			return &s_Lights[i];
	}
	return NULL;
}

static void DLights_Remove( int key )
{
	client_dlight_t *l = DLights_Find( key );
	if ( !l )
		return;
	if ( l->dl->key == key )
		l->dl->die = gEngfuncs.GetClientTime();		// die instantly
	l->dl = NULL;
}

static int MsgFunc_KeyedDLight( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	const int iKey = READ_SHORT();
	const int iState = READ_BYTE();
	if ( iKey <= 0 )
		return 1;

	if ( iState == DLIGHT_MSG_OFF )
	{
		DLights_Remove( iKey );
	}
	else if ( iState == DLIGHT_MSG_POS )
	{
		Vector pos;
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		client_dlight_t *l = DLights_Find( iKey );
		if ( l && l->dl->key == iKey )
			l->dl->origin = pos;
	}
	else
	{
		Vector pos;
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		const int radius = READ_SHORT();
		const int r = READ_BYTE(), g = READ_BYTE(), b = READ_BYTE();
		int entindex = READ_SHORT();
		if ( entindex < 0 || entindex >= 4096 )		// SBO: an index from the network, checked
			entindex = 0;

		// never die (until switched off)
		dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( iKey );
		if ( !dl )
			return 1;
		dl->die = gEngfuncs.GetClientTime() + (float)1E6;
		dl->origin = pos;
		dl->radius = radius;
		dl->color.r = r;
		dl->color.g = g;
		dl->color.b = b;

		client_dlight_t *l = DLights_Find( iKey );
		for ( int i = 0; !l && i < DLIGHTS_CLIENT_MAX; i++ )
		{
			if ( !s_Lights[i].dl )
				l = &s_Lights[i];
		}
		if ( l )
		{
			l->dl = dl;
			l->key = iKey;
			l->entindex = entindex;
		}
	}
	return 1;
}

void DLights_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "KeyedDLight", MsgFunc_KeyedDLight );
}

void DLights_Reset( void )
{
	// the engine cleared its dlights with the old map: forget them, do not touch them
	memset( s_Lights, 0, sizeof( s_Lights ) );
}

void DLights_CreateEntities( void )
{
	const float flTime = gEngfuncs.GetClientTime();
	for ( int i = 0; i < DLIGHTS_CLIENT_MAX; i++ )
	{
		client_dlight_t *l = &s_Lights[i];
		if ( !l->dl )
			continue;
		// the engine gave the dlight to something else, or it ended
		if ( l->dl->key != l->key || l->dl->die < flTime )
		{
			l->dl = NULL;
			continue;
		}
		if ( l->entindex )
		{
			cl_entity_t *pEntity = gEngfuncs.GetEntityByIndex( l->entindex );
			if ( pEntity )
				l->dl->origin = pEntity->origin;
		}
	}
}
