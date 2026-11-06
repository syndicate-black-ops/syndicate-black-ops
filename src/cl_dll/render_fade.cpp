//=========================================================
// SBO 2.0 env_render fades, client side: see render_fade.h, game_shared/render_fade_shared.h and docs/ENV_RENDER.md.
//
// The server (dlls/render_fade.cpp) sends one "RenderFade" message per faded entity and keeps the end values in the
// entity's state. While a fade runs, HUD_AddEntity draws the values between its start and its end for this frame's
// client time; after it, the entity's own state (the end values) is drawn. Written for SBO (Spirit of Half-Life and
// Featureful fade on the server, step by step).
//=========================================================

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "cl_entity.h"
#include "render_fade.h"
#include "render_fade_shared.h"

#define RENDERFADE_CLIENT_MAX	256

struct client_render_fade_t
{
	int		entindex;		// 0: free
	int		modelindex;
	int		flags;
	double	start;
	double	duration;
	int		startAmt, endAmt;
	int		startColor[3], endColor[3];
	float	startScale, endScale;
};

static client_render_fade_t s_Fades[ RENDERFADE_CLIENT_MAX ];
static int s_iActive;	// slots in use (0: HUD_AddEntity does nothing)

static client_render_fade_t *RenderFade_Slot( int entindex, bool bCreate )
{
	client_render_fade_t *pFree = NULL;

	for ( int i = 0; i < RENDERFADE_CLIENT_MAX; i++ )
	{
		if ( s_Fades[i].entindex == entindex )
			return &s_Fades[i];
		if ( !pFree && !s_Fades[i].entindex )
			pFree = &s_Fades[i];
	}

	if ( bCreate && pFree )
	{
		pFree->entindex = entindex;
		s_iActive++;
		return pFree;
	}
	return NULL;
}

static void RenderFade_Free( client_render_fade_t *f )
{
	if ( f->entindex )
	{
		f->entindex = 0;
		s_iActive--;
	}
}

static int MsgFunc_RenderFade( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int entindex = READ_SHORT();
	int modelindex = READ_SHORT();
	int flags = READ_BYTE();

	if ( entindex <= 0 || entindex >= 8192 )
		return 1;

	client_render_fade_t *f = RenderFade_Slot( entindex, flags != 0 );
	if ( !f )
		return 1;

	if ( !flags )
	{
		RenderFade_Free( f );	// stopped: the entity's own state is drawn
		return 1;
	}

	f->modelindex = modelindex;
	f->flags = flags & ( RENDERFADE_AMT | RENDERFADE_COLOR | RENDERFADE_SCALE );
	f->start = READ_LONG() / 1000.0;
	f->duration = READ_LONG() / 1000.0;
	if ( flags & RENDERFADE_AMT )
	{
		f->startAmt = READ_BYTE();
		f->endAmt = READ_BYTE();
	}
	if ( flags & RENDERFADE_COLOR )
	{
		for ( int i = 0; i < 3; i++ )
			f->startColor[i] = READ_BYTE();
		for ( int i = 0; i < 3; i++ )
			f->endColor[i] = READ_BYTE();
	}
	if ( flags & RENDERFADE_SCALE )
	{
		f->startScale = READ_LONG() / RENDERFADE_SCALE_UNIT;
		f->endScale = READ_LONG() / RENDERFADE_SCALE_UNIT;
	}

	// a short or damaged message, or nonsense: no fade
	if ( READ_OK() == false || !( f->duration > 0 ) || f->duration > RENDERFADE_MAX_TIME
		|| !( f->startScale >= 0 ) || !( f->endScale >= 0 ) )
		RenderFade_Free( f );

	return 1;
}

void RenderFade_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "RenderFade", MsgFunc_RenderFade );
}

void RenderFade_Reset( void )
{
	for ( int i = 0; i < RENDERFADE_CLIENT_MAX; i++ )
		s_Fades[i].entindex = 0;
	s_iActive = 0;
}

static int Lerp( int a, int b, float t )
{
	int v = (int)( a + ( b - a ) * t + 0.5f );
	return v < 0 ? 0 : ( v > 255 ? 255 : v );
}

void RenderFade_AddEntity( struct cl_entity_s *ent )
{
	if ( !s_iActive || !ent || ent->index <= 0 )
		return;

	client_render_fade_t *f = RenderFade_Slot( ent->index, false );
	if ( !f )
		return;

	// the entity index was reused by another entity (the faded one was removed)
	if ( ent->curstate.modelindex != f->modelindex )
	{
		RenderFade_Free( f );
		return;
	}

	double t = ( gEngfuncs.GetClientTime() - f->start ) / f->duration;
	if ( t >= 1.0 )
	{
		RenderFade_Free( f );	// done: the end values are the entity's own
		return;
	}
	if ( t < 0 )
		t = 0;

	if ( f->flags & RENDERFADE_AMT )
		ent->curstate.renderamt = Lerp( f->startAmt, f->endAmt, (float)t );
	if ( f->flags & RENDERFADE_COLOR )
	{
		ent->curstate.rendercolor.r = Lerp( f->startColor[0], f->endColor[0], (float)t );
		ent->curstate.rendercolor.g = Lerp( f->startColor[1], f->endColor[1], (float)t );
		ent->curstate.rendercolor.b = Lerp( f->startColor[2], f->endColor[2], (float)t );
	}
	if ( f->flags & RENDERFADE_SCALE )
		ent->curstate.scale = (float)( f->startScale + ( f->endScale - f->startScale ) * t );
}
