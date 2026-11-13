//=========================================================
// SBO 2.0 env_dlight / env_elight, client side: see dlights.h, game_shared/dlight_shared.h and docs/ENV_DLIGHT.md.
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) commit 30fc2f71:
// cl_dll/hud.cpp KeyedDLightManager, cl_dll/hud_msg.cpp MsgFunc_KeyedDLight, the Update call in cl_dll/entity.cpp
// HUD_CreateEntities. Each env_dlight that is on is one engine dlight under the env_dlight's entity index as key,
// alive until switched off.
//
// SBO changes: a light switched on again (the same key) takes its old slot instead of a second one; a slot whose
// dlight the engine took back (key 0, or past its die time) is freed; the lights are forgotten at VidInit; a light
// is only made when this table has room for it (else it could never be switched off).
//
// SBO 2.0 env_elight: the same keyed light as an engine entity light (CL_AllocElight: lights studio models only,
// as Spirit of Half-Life's TE_ELIGHT), following an entity's origin or one of its attachments 1-4 (from the frame
// before, where the engine last drew it). The engine's elights are shared with other effects: at most
// ELIGHTS_CLIENT_MAX are env_elights, and one whose slot an effect took is made again, at most every
// ELIGHT_RETRY_TIME seconds, so two lights never take the same slot from each other every frame.
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "cl_entity.h"
#include "com_model.h"
#include "r_efx.h"
#include "dlight.h"
#include "dlights.h"
#include "dlight_shared.h"

#include <string.h>

#define DLIGHTS_CLIENT_MAX	32		// the engine's dlights (MAX_DLIGHTS)
#define ELIGHTS_CLIENT_MAX	32		// half the engine's 64 elights (MAX_ELIGHTS): the rest for the engine's effects
#define ELIGHT_MAX_ATTACHMENT	4	// cl_entity_t attachment[ 4 ]
#define ELIGHT_RETRY_TIME	0.5f

struct client_dlight_t
{
	dlight_t	*dl;		// NULL: free
	int			key;
	int			entindex;	// follow this entity (0: none)
	// SBO 2.0 env_elight
	bool		elight;
	int			attachment;	// 0: the entity's origin, 1-4: its attachment
	Vector		origin;		// to make it again when an effect took its slot
	float		radius;
	color24		color;
	float		retry;		// not before this time
};

static client_dlight_t s_Lights[ DLIGHTS_CLIENT_MAX + ELIGHTS_CLIENT_MAX ];

#define LIGHTS_TABLE	( (int)( sizeof( s_Lights ) / sizeof( s_Lights[0] ) ) )

static client_dlight_t *DLights_Find( int key )
{
	for ( int i = 0; i < LIGHTS_TABLE; i++ )
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

// a free table slot for a new light of this kind (NULL: as many as allowed are on)
static client_dlight_t *DLights_FreeSlot( bool elight )
{
	int iCount = 0;
	client_dlight_t *pFree = NULL;
	for ( int i = 0; i < LIGHTS_TABLE; i++ )
	{
		if ( !s_Lights[i].dl )
		{
			if ( !pFree )
				pFree = &s_Lights[i];
		}
		else if ( s_Lights[i].elight == elight )
			iCount++;
	}
	if ( iCount >= ( elight ? ELIGHTS_CLIENT_MAX : DLIGHTS_CLIENT_MAX ) )
		return NULL;
	return pFree;
}

// the engine light for a table slot, filled in (NULL: none to be had)
static dlight_t *DLights_Alloc( client_dlight_t *l )
{
	dlight_t *dl = l->elight ? gEngfuncs.pEfxAPI->CL_AllocElight( l->key ) : gEngfuncs.pEfxAPI->CL_AllocDlight( l->key );
	if ( !dl )
		return NULL;
	// never die (until switched off)
	dl->die = gEngfuncs.GetClientTime() + (float)1E6;
	dl->origin = l->origin;
	dl->radius = l->radius;
	dl->color = l->color;
	dl->decay = 0;
	return dl;
}

static int MsgFunc_KeyedDLight( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	const int iKey = READ_SHORT();
	const int iRead = READ_BYTE();
	const int iState = iRead & ~DLIGHT_MSG_ELIGHT;
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
		if ( l )
		{
			l->origin = pos;
			if ( l->dl->key == iKey )
				l->dl->origin = pos;
		}
	}
	else
	{
		const bool bELight = ( iRead & DLIGHT_MSG_ELIGHT ) != 0;
		Vector pos;
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		const int radius = READ_SHORT();
		const int r = READ_BYTE(), g = READ_BYTE(), b = READ_BYTE();
		int entindex = READ_SHORT();
		if ( entindex < 0 || entindex >= 4096 )		// SBO: an index from the network, checked
			entindex = 0;
		int attachment = bELight ? READ_BYTE() : 0;
		if ( attachment < 0 || attachment > ELIGHT_MAX_ATTACHMENT )
			attachment = 0;

		client_dlight_t *l = DLights_Find( iKey );
		if ( l && l->elight != bELight )
		{
			DLights_Remove( iKey );		// the other kind under this key (an index reused): out first
			l = NULL;
		}
		if ( !l )
			l = DLights_FreeSlot( bELight );
		if ( !l )
			return 1;		// as many as the engine can take are on

		l->key = iKey;
		l->entindex = entindex;
		l->elight = bELight;
		l->attachment = attachment;
		l->origin = pos;
		l->radius = (float)radius;
		l->color.r = r;
		l->color.g = g;
		l->color.b = b;
		l->retry = 0;
		l->dl = DLights_Alloc( l );
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
	for ( int i = 0; i < LIGHTS_TABLE; i++ )
	{
		client_dlight_t *l = &s_Lights[i];
		if ( !l->dl )
			continue;
		if ( l->entindex )
		{
			cl_entity_t *pEntity = gEngfuncs.GetEntityByIndex( l->entindex );
			if ( pEntity )
			{
				l->origin = pEntity->origin;
				// SBO 2.0 env_elight: an attachment, where the engine drew the model last (a model it has drawn)
				if ( l->attachment && pEntity->model && pEntity->model->type == mod_studio )
				{
					const Vector &att = pEntity->attachment[ l->attachment - 1 ];
					if ( att.x != 0 || att.y != 0 || att.z != 0 )
						l->origin = att;
				}
			}
		}
		// the engine gave the light to something else, or it ended
		if ( l->dl->key != l->key || l->dl->die < flTime )
		{
			if ( !l->elight )
			{
				l->dl = NULL;
				continue;
			}
			// SBO 2.0 env_elight: stays on until the server switches it off: made again when an elight is free
			if ( flTime < l->retry )
				continue;
			l->retry = flTime + ELIGHT_RETRY_TIME;
			dlight_t *dl = DLights_Alloc( l );
			if ( !dl )
				continue;
			l->dl = dl;
		}
		l->dl->origin = l->origin;
	}
}
