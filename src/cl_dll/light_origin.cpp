//=========================================================
// SBO 2.0 info_light_origin, client side: see light_origin.h, game_shared/light_origin_shared.h and
// docs/ENV_DLIGHT.md#info_light_origin.
//
// From the Trinity renderer SDK (Half-Life 1 SDK license; cl_dll/StudioModelRenderer.cpp, the light point from
// pExtraData->lightorigin): a map entity's model is lit from the marker's position. The engine samples the lightmap
// under the point it is given (IEngineStudio.StudioDynamicLight); the renderer gives it the marker's position for
// that call only (CStudioModelRenderer::StudioDynamicLight_At). The server sends one point per entity; env_static
// props carry theirs in their own message (static_props.cpp).
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "cl_entity.h"
#include "light_origin.h"
#include "static_props.h"

#include <string.h>

#define LIGHTORIGIN_CLIENT_MAX_EDICTS	4096

struct client_light_origin_t
{
	short	modelindex;		// 0: none
	vec3_t	origin;
};

static client_light_origin_t s_Origins[ LIGHTORIGIN_CLIENT_MAX_EDICTS ];
static int s_iCount;	// entries set: none, nothing to look up per model

static int MsgFunc_LightOrigin( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	const int index = READ_SHORT();
	const int modelindex = READ_SHORT();
	vec3_t origin;
	origin[0] = READ_COORD();
	origin[1] = READ_COORD();
	origin[2] = READ_COORD();
	if ( !READ_OK() || index <= 0 || index >= LIGHTORIGIN_CLIENT_MAX_EDICTS || modelindex <= 0 || modelindex > 32767 )
		return 1;
	if ( !s_Origins[ index ].modelindex )
		s_iCount++;
	s_Origins[ index ].modelindex = (short)modelindex;
	VectorCopy( origin, s_Origins[ index ].origin );
	return 1;
}

void LightOrigin_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "LightOrigin", MsgFunc_LightOrigin );
}

void LightOrigin_Reset( void )
{
	memset( s_Origins, 0, sizeof( s_Origins ) );
	s_iCount = 0;
}

bool LightOrigin_Get( const cl_entity_t *ent, float *out )
{
	if ( !ent )
		return false;
	if ( ent->index == 0 )
		return StaticProps_LightOrigin( ent, out ) != FALSE;
	if ( !s_iCount || ent->index < 0 || ent->index >= LIGHTORIGIN_CLIENT_MAX_EDICTS )
		return false;
	const client_light_origin_t *l = &s_Origins[ ent->index ];
	// the entity in this slot still has the model it had when the server sent the point
	if ( !l->modelindex || l->modelindex != ent->curstate.modelindex )
		return false;
	VectorCopy( l->origin, out );
	return true;
}
