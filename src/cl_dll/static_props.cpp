//=========================================================
// SBO 2.0 env_static, client side: see static_props.h, game_shared/static_props_shared.h and docs/ENV_STATIC.md.
//
// Written for SBO (XDM draws its env_static with its own render system). Each prop is a cl_entity_t of this file,
// handed to the engine every frame with CL_CreateVisibleEntity while its box is in the view's PVS
// (TriAPI BoxInPVS). The model is loaded by name (IEngineStudio.Mod_ForName), so it takes no slot in the precache
// list the server sends; the server offers the file for download (PRECACHE_GENERIC). No model: no prop.
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "const.h"
#include "entity_types.h"
#include "cl_entity.h"
#include "com_model.h"
#include "studio.h"
#include "triangleapi.h"
#include "r_studioint.h"
#include "static_props.h"
#include "static_props_shared.h"

#include <string.h>

extern engine_studio_api_t IEngineStudio;

struct client_static_model_t
{
	char		name[64];
	model_t		*model;
	BOOL		tried;		// Mod_ForName was called (a missing file is not looked for every frame)
	vec3_t		mins, maxs;	// the model's box at scale 1
};

struct client_static_prop_t
{
	BOOL		used;
	BOOL		visible;
	int			model;
	cl_entity_t	ent;
	vec3_t		absmin, absmax;	// for the PVS test
};

static client_static_model_t s_Models[ STATICPROP_MAX_MODELS ];
static client_static_prop_t s_Props[ STATICPROP_MAX_PROPS ];
static int s_iHighest;	// props in use are below this index

static void StaticProps_Clear( void )
{
	for ( int i = 0; i < STATICPROP_MAX_MODELS; i++ )
	{
		s_Models[i].name[0] = '\0';
		s_Models[i].model = NULL;
		s_Models[i].tried = FALSE;
	}
	for ( int i = 0; i < STATICPROP_MAX_PROPS; i++ )
		s_Props[i].used = FALSE;
	s_iHighest = 0;
}

// the model of a prop, loaded the first time it is drawn; NULL if the file is missing or not a studio model
static model_t *StaticProps_Model( int i )
{
	client_static_model_t *m = &s_Models[i];

	if ( !m->tried )
	{
		m->tried = TRUE;
		m->model = NULL;
		if ( m->name[0] && IEngineStudio.Mod_ForName )
		{
			model_t *mod = IEngineStudio.Mod_ForName( m->name, 0 );
			if ( mod && mod->type == mod_studio )
			{
				m->model = mod;

				// the box: the studio header's clipping box, else its movement hull, else 64 units around
				studiohdr_t *pHdr = IEngineStudio.Mod_Extradata ? (studiohdr_t *)IEngineStudio.Mod_Extradata( mod ) : NULL;
				bool bBox = false;
				if ( pHdr )
				{
					if ( pHdr->bbmin[0] < pHdr->bbmax[0] )
					{
						VectorCopy( pHdr->bbmin, m->mins );
						VectorCopy( pHdr->bbmax, m->maxs );
						bBox = true;
					}
					else if ( pHdr->min[0] < pHdr->max[0] )
					{
						VectorCopy( pHdr->min, m->mins );
						VectorCopy( pHdr->max, m->maxs );
						bBox = true;
					}
				}
				if ( !bBox )
				{
					m->mins[0] = m->mins[1] = m->mins[2] = -64;
					m->maxs[0] = m->maxs[1] = m->maxs[2] = 64;
				}
			}
			else
				gEngfuncs.Con_Printf( "env_static: model \"%s\" missing or not a studio model\n", m->name );
		}
	}
	return m->model;
}

static void StaticProps_UpdateBox( client_static_prop_t *p )
{
	const client_static_model_t *m = &s_Models[ p->model ];
	float scale = p->ent.curstate.scale > 0 ? p->ent.curstate.scale : 1.0f;

	// the model may be turned any way: a cube around its box's farthest corner
	float r = 0;
	for ( int k = 0; k < 3; k++ )
	{
		float a = fabs( m->mins[k] ), b = fabs( m->maxs[k] );
		r += ( a > b ? a : b ) * ( a > b ? a : b );
	}
	r = sqrt( r ) * scale;

	for ( int k = 0; k < 3; k++ )
	{
		p->absmin[k] = p->ent.origin[k] - r;
		p->absmax[k] = p->ent.origin[k] + r;
	}
}

static int MsgFunc_StaticProp( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int type = READ_BYTE();

	switch ( type )
	{
	case STATICPROP_CLEAR:
		StaticProps_Clear();
		break;

	case STATICPROP_MODEL:
	{
		int id = READ_BYTE();
		const char *name = READ_STRING();
		if ( !READ_OK() || id < 0 || id >= STATICPROP_MAX_MODELS )
			break;
		client_static_model_t *m = &s_Models[id];
		strncpy( m->name, name, sizeof( m->name ) - 1 );
		m->name[ sizeof( m->name ) - 1 ] = '\0';
		m->model = NULL;
		m->tried = FALSE;
		break;
	}

	case STATICPROP_PROP:
	{
		int id = READ_SHORT();
		int model = READ_BYTE();
		vec3_t origin, angles;
		for ( int k = 0; k < 3; k++ )
			origin[k] = READ_LONG() / 8.0f;
		for ( int k = 0; k < 3; k++ )
			angles[k] = READ_ANGLE();
		int rendermode = READ_BYTE();
		int renderfx = READ_BYTE();
		int r = READ_BYTE(), g = READ_BYTE(), b = READ_BYTE();
		int renderamt = READ_BYTE();
		int body = READ_BYTE();
		int skin = READ_BYTE();
		int sequence = READ_SHORT();
		float scale = READ_SHORT() / 256.0f;
		float framerate = READ_SHORT() / 256.0f;
		int visible = READ_BYTE();

		if ( !READ_OK() || id < 0 || id >= STATICPROP_MAX_PROPS || model < 0 || model >= STATICPROP_MAX_MODELS )
			break;

		client_static_prop_t *p = &s_Props[id];
		memset( p, 0, sizeof( *p ) );
		p->used = TRUE;
		p->visible = visible != 0;
		p->model = model;

		cl_entity_t *ent = &p->ent;
		VectorCopy( origin, ent->origin );
		VectorCopy( origin, ent->curstate.origin );
		VectorCopy( angles, ent->angles );
		VectorCopy( angles, ent->curstate.angles );
		ent->curstate.rendermode = rendermode;
		ent->curstate.renderfx = renderfx;
		ent->curstate.rendercolor.r = r;
		ent->curstate.rendercolor.g = g;
		ent->curstate.rendercolor.b = b;
		ent->curstate.renderamt = renderamt;
		ent->curstate.body = body;
		ent->curstate.skin = skin;
		ent->curstate.sequence = sequence;
		ent->curstate.scale = scale;
		ent->curstate.framerate = framerate;
		ent->curstate.animtime = gEngfuncs.GetClientTime();
		ent->curstate.solid = SOLID_NOT;
		ent->curstate.movetype = MOVETYPE_NONE;
		ent->index = 0;		// not a server entity
		ent->baseline = ent->curstate;
		ent->prevstate = ent->curstate;

		if ( id + 1 > s_iHighest )
			s_iHighest = id + 1;
		break;
	}

	case STATICPROP_VISIBLE:
	{
		int id = READ_SHORT();
		int visible = READ_BYTE();
		if ( READ_OK() && id >= 0 && id < STATICPROP_MAX_PROPS && s_Props[id].used )
		{
			s_Props[id].visible = visible != 0;
			if ( visible )
				s_Props[id].ent.curstate.animtime = gEngfuncs.GetClientTime();	// XDM: the animation starts again
		}
		break;
	}

	default:
		break;
	}

	return 1;
}

void StaticProps_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "StaticProp", MsgFunc_StaticProp );
}

void StaticProps_Reset( void )
{
	StaticProps_Clear();
}

void StaticProps_CreateEntities( void )
{
	for ( int i = 0; i < s_iHighest; i++ )
	{
		client_static_prop_t *p = &s_Props[i];
		if ( !p->used || !p->visible )
			continue;

		model_t *mod = StaticProps_Model( p->model );
		if ( !mod )
			continue;

		if ( p->ent.model != mod )
		{
			p->ent.model = mod;
			StaticProps_UpdateBox( p );
		}

		// only what the view can see through the map (the engine draws visible entities without a PVS test)
		if ( gEngfuncs.pTriAPI && gEngfuncs.pTriAPI->BoxInPVS && !gEngfuncs.pTriAPI->BoxInPVS( p->absmin, p->absmax ) )
			continue;

		gEngfuncs.CL_CreateVisibleEntity( ET_NORMAL, &p->ent );
	}
}
