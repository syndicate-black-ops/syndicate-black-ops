//=========================================================
// SBO 2.0 scaled break gibs, client side: the "BreakModel" user message (func_breakable / func_pushable
// "gibscale", func_break.cpp CBreakable::BreakModel). The server sends it only for a scale above 0; without one it
// sends the engine's TE_BREAKMODEL as before.
//
// Ported from Featureful (FreeSlave/halflife-featureful 0a0160c4: cl_dll/cl_fx.cpp FX_BreakModel, cl_msg_fx.cpp
// __MsgFunc_BreakModel, game_shared/studio_getters GetOverallBodyNum; Half-Life 1 SDK license). The gibs are made
// as the engine's TE_BREAKMODEL makes them, with the model scale set.
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "cl_entity.h"
#include "com_model.h"
#include "r_efx.h"
#include "studio.h"
#include "break_gibs.h"

#define SHARD_VOLUME	12.0f

// the number of body combinations of a studio model (every body group's models multiplied)
static int BreakGibs_BodyCount( void *pmodel )
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;
	if ( !pstudiohdr )
		return 1;

	int bodies = 1;
	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)( (byte *)pstudiohdr + pstudiohdr->bodypartindex );
	for ( int j = 0; j < pstudiohdr->numbodyparts; j++ )
		bodies *= pbodypart[j].nummodels;
	return bodies > 0 ? bodies : 1;
}

static void BreakGibs_Make( const Vector &pos, const Vector &size, const Vector &dir, float random, float life, int count,
	int modelIndex, int flags, float customScale )
{
	model_t *pmodel = gEngfuncs.hudGetModelByIndex( modelIndex );
	if ( !pmodel )
		return;

	const int type = flags & BREAK_TYPEMASK;

	if ( count == 0 )
		count = (int)( ( size[0] * size[1] + size[1] * size[2] + size[2] * size[0] ) / ( 3 * SHARD_VOLUME * SHARD_VOLUME ) );
	if ( count > 100 )
		count = 100;

	const float clientTime = gEngfuncs.GetClientTime();

	int bodyNum = 1;
	if ( pmodel->type == mod_studio )
		bodyNum = BreakGibs_BodyCount( pmodel->cache.data );

	for ( int i = 0; i < count; i++ )
	{
		Vector vecSpot;
		int j = 0;

		for ( ; j < 32; j++ )
		{
			// fill up the box with stuff
			vecSpot[0] = pos[0] + gEngfuncs.pfnRandomFloat( -0.5f, 0.5f ) * size[0];
			vecSpot[1] = pos[1] + gEngfuncs.pfnRandomFloat( -0.5f, 0.5f ) * size[1];
			vecSpot[2] = pos[2] + gEngfuncs.pfnRandomFloat( -0.5f, 0.5f ) * size[2];

			if ( gEngfuncs.PM_PointContents( vecSpot, NULL ) != CONTENTS_SOLID )
				break;
		}

		if ( j == 32 )
			continue;

		TEMPENTITY *pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc( vecSpot, pmodel );
		if ( !pTemp )
			return;

		// keep track of break_type, so we know how to play sound on collision
		pTemp->hitSound = type;
		pTemp->frameMax = pmodel->numframes - 1;

		if ( pmodel->type == mod_sprite )
			pTemp->entity.curstate.frame = gEngfuncs.pfnRandomLong( 0, (int)pTemp->frameMax );
		else if ( pmodel->type == mod_studio )
			pTemp->entity.curstate.body = gEngfuncs.pfnRandomLong( 0, bodyNum - 1 );

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		if ( gEngfuncs.pfnRandomLong( 0, 255 ) < 200 )
		{
			pTemp->flags |= FTENT_ROTATE;
			pTemp->entity.baseline.angles[0] = gEngfuncs.pfnRandomFloat( -256, 255 );
			pTemp->entity.baseline.angles[1] = gEngfuncs.pfnRandomFloat( -256, 255 );
			pTemp->entity.baseline.angles[2] = gEngfuncs.pfnRandomFloat( -256, 255 );
		}

		if ( ( gEngfuncs.pfnRandomLong( 0, 255 ) < 100 ) && ( flags & BREAK_SMOKE ) )
			pTemp->flags |= FTENT_SMOKETRAIL;

		if ( ( type == BREAK_GLASS ) || ( flags & BREAK_TRANS ) )
		{
			pTemp->entity.curstate.rendermode = kRenderTransTexture;
			pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 128;
		}
		else
		{
			pTemp->entity.curstate.rendermode = kRenderNormal;
			pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 255;
		}

		pTemp->entity.baseline.origin[0] = dir[0] + gEngfuncs.pfnRandomFloat( -random, random );
		pTemp->entity.baseline.origin[1] = dir[1] + gEngfuncs.pfnRandomFloat( -random, random );
		pTemp->entity.baseline.origin[2] = dir[2] + gEngfuncs.pfnRandomFloat( 0, random );

		pTemp->entity.curstate.scale = customScale > 0 ? customScale : 1.0f;

		pTemp->die = clientTime + life + gEngfuncs.pfnRandomFloat( 0.0f, 1.0f );
	}
}

// TE_BREAKMODEL's fields (coords: position, size, velocity; byte random / 10, short model, byte count, byte life in
// 0.1 s, byte flags), then short scale * 100
static int MsgFunc_BreakModel( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	Vector pos, size, dir;
	for ( int i = 0; i < 3; i++ )
		pos[i] = READ_COORD();
	for ( int i = 0; i < 3; i++ )
		size[i] = READ_COORD();
	for ( int i = 0; i < 3; i++ )
		dir[i] = READ_COORD();
	const float random = READ_BYTE() * 10.0f;
	const int modelIndex = READ_SHORT();
	const int count = READ_BYTE();
	const float life = READ_BYTE() * 0.1f;
	const int flags = READ_BYTE();
	const float scale = READ_SHORT() / 100.0f;

	BreakGibs_Make( pos, size, dir, random, life, count, modelIndex, flags, scale );
	return 1;
}

void BreakGibs_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "BreakModel", MsgFunc_BreakModel );
}
