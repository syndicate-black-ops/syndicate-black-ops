//=========================================================
// SBO 2.0 env_render fades, server side: see render_fade.h and docs/ENV_RENDER.md.
//
// Spirit of Half-Life and Featureful fade with a "render_fader" entity that rewrites renderamt / rendercolor / scale
// every "Fade Coarseness" seconds: an entity update per step for every client that sees it, and visible steps. Here
// the server sends one message per faded entity and the clients interpolate at their frame rate.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"render_fade.h"

extern int gmsgRenderFade;

#define RENDERFADE_MAX_ACTIVE	256		// fades running at once (more: the entity takes its end values at once)

struct render_fade_t
{
	EHANDLE	hEntity;
	int		id;
	int		flags;
	float	start;
	float	duration;
	float	startAmt, endAmt;
	Vector	startColor, endColor;
	float	startScale, endScale;
};

static render_fade_t s_Fades[ RENDERFADE_MAX_ACTIVE ];
static int s_iNextId = 1;
static int s_iScaleFades;	// slots holding a scale fade (RenderFade_ScaleNow looks only when there is one)

// counts the scale fades again after a slot changed
static void RenderFade_CountScaleFades( void )
{
	s_iScaleFades = 0;
	for ( int i = 0; i < RENDERFADE_MAX_ACTIVE; i++ )
	{
		if ( ( s_Fades[i].flags & RENDERFADE_SCALE ) && (CBaseEntity *)s_Fades[i].hEntity != NULL )
			s_iScaleFades++;
	}
}

static render_fade_t *RenderFade_Find( CBaseEntity *pTarget )
{
	for ( int i = 0; i < RENDERFADE_MAX_ACTIVE; i++ )
	{
		CBaseEntity *pEnt = s_Fades[i].hEntity;
		if ( pEnt && pEnt == pTarget )
			return &s_Fades[i];
	}
	return NULL;
}

static float Clamp( float f, float lo, float hi )
{
	if ( !( f >= lo ) )	// also NaN
		return lo;
	return f > hi ? hi : f;
}

static float RenderFade_Degree( const render_fade_t *f )
{
	if ( f->duration <= 0 )
		return 1;
	return Clamp( ( gpGlobals->time - f->start ) / f->duration, 0, 1 );
}

void RenderFade_Current( CBaseEntity *pTarget, float *pAmt, Vector *pColor, float *pScale )
{
	*pAmt = pTarget->pev->renderamt;
	*pColor = pTarget->pev->rendercolor;
	*pScale = pTarget->pev->scale;

	render_fade_t *f = RenderFade_Find( pTarget );
	if ( !f )
		return;

	float t = RenderFade_Degree( f );
	if ( f->flags & RENDERFADE_AMT )
		*pAmt = f->startAmt + ( f->endAmt - f->startAmt ) * t;
	if ( f->flags & RENDERFADE_COLOR )
		*pColor = f->startColor + ( f->endColor - f->startColor ) * t;
	if ( f->flags & RENDERFADE_SCALE )
		*pScale = f->startScale + ( f->endScale - f->startScale ) * t;
}

BOOL RenderFade_ScaleNow( const entvars_t *pev, float *pScale )
{
	if ( !s_iScaleFades || !pev || !pev->pContainingEntity )
		return FALSE;

	CBaseEntity *pTarget = CBaseEntity::Instance( (edict_t *)pev->pContainingEntity );
	render_fade_t *f = pTarget ? RenderFade_Find( pTarget ) : NULL;
	if ( !f || !( f->flags & RENDERFADE_SCALE ) )
		return FALSE;

	*pScale = f->startScale + ( f->endScale - f->startScale ) * RenderFade_Degree( f );
	return TRUE;
}

void RenderFade_Frame( void )
{
	if ( !s_iScaleFades )
		return;

	for ( int i = 0; i < RENDERFADE_MAX_ACTIVE; i++ )
	{
		render_fade_t *f = &s_Fades[i];
		CBaseEntity *pEnt = f->hEntity;
		if ( pEnt && ( f->flags & RENDERFADE_SCALE ) )
			UTIL_ModelScaleChanged( pEnt->pev );	// the box (and the hitbox cache key) at this moment's scale
	}
}

static void RenderFade_Send( CBaseEntity *pTarget, const render_fade_t *f )
{
	if ( !gmsgRenderFade )
		return;

	MESSAGE_BEGIN( MSG_ALL, gmsgRenderFade );
		WRITE_SHORT( pTarget->entindex() );
		WRITE_SHORT( pTarget->pev->modelindex );
		WRITE_BYTE( f ? f->flags : 0 );
		if ( f )
		{
			WRITE_LONG( (int)( f->start * 1000.0f ) );
			WRITE_LONG( (int)( f->duration * 1000.0f ) );
			if ( f->flags & RENDERFADE_AMT )
			{
				WRITE_BYTE( (int)Clamp( f->startAmt, 0, 255 ) );
				WRITE_BYTE( (int)Clamp( f->endAmt, 0, 255 ) );
			}
			if ( f->flags & RENDERFADE_COLOR )
			{
				for ( int i = 0; i < 3; i++ )
					WRITE_BYTE( (int)Clamp( f->startColor[i], 0, 255 ) );
				for ( int i = 0; i < 3; i++ )
					WRITE_BYTE( (int)Clamp( f->endColor[i], 0, 255 ) );
			}
			if ( f->flags & RENDERFADE_SCALE )
			{
				WRITE_LONG( (int)( Clamp( f->startScale, 0, RENDERFADE_MAX_SCALE ) * RENDERFADE_SCALE_UNIT ) );
				WRITE_LONG( (int)( Clamp( f->endScale, 0, RENDERFADE_MAX_SCALE ) * RENDERFADE_SCALE_UNIT ) );
			}
		}
	MESSAGE_END();
}

int RenderFade_Start( CBaseEntity *pTarget, int flags, float endAmt, const Vector &endColor, float endScale, float flDuration )
{
	float amt, scale;
	Vector color;
	RenderFade_Current( pTarget, &amt, &color, &scale );

	// the end values go into pev now: whoever gets the entity from now on (a late joiner, a spectator, a demo)
	// sees the end state
	if ( flags & RENDERFADE_AMT )
		pTarget->pev->renderamt = Clamp( endAmt, 0, 255 );
	if ( flags & RENDERFADE_COLOR )
		pTarget->pev->rendercolor = Vector( Clamp( endColor.x, 0, 255 ), Clamp( endColor.y, 0, 255 ), Clamp( endColor.z, 0, 255 ) );
	if ( flags & RENDERFADE_SCALE )
		pTarget->pev->scale = Clamp( endScale, 0, RENDERFADE_MAX_SCALE );

	render_fade_t *f = RenderFade_Find( pTarget );
	if ( !f )
	{
		for ( int i = 0; i < RENDERFADE_MAX_ACTIVE && !f; i++ )
		{
			if ( (CBaseEntity *)s_Fades[i].hEntity == NULL )
				f = &s_Fades[i];
		}
	}

	flDuration = Clamp( flDuration, 0, RENDERFADE_MAX_TIME );
	if ( !f || !flags || flDuration <= 0 )
	{
		if ( f && (CBaseEntity *)f->hEntity != NULL )
		{
			f->hEntity = NULL;
			RenderFade_CountScaleFades( );
			RenderFade_Send( pTarget, NULL );
		}
		if ( !f )
			ALERT( at_console, "env_render: more than %d fades at once, \"%s\" changes at once\n", RENDERFADE_MAX_ACTIVE, STRING( pTarget->pev->targetname ) );
		// changed at once: the collision box follows the scale now (no RenderFade_Finish for this one)
		if ( flags & RENDERFADE_SCALE )
			UTIL_ModelScaleChanged( pTarget->pev );
		return 0;
	}

	f->hEntity = pTarget;
	f->id = s_iNextId++;
	if ( s_iNextId <= 0 )
		s_iNextId = 1;
	f->flags = flags;
	f->start = gpGlobals->time;
	f->duration = flDuration;
	f->startAmt = Clamp( amt, 0, 255 );
	f->endAmt = pTarget->pev->renderamt;
	f->startColor = Vector( Clamp( color.x, 0, 255 ), Clamp( color.y, 0, 255 ), Clamp( color.z, 0, 255 ) );
	f->endColor = pTarget->pev->rendercolor;
	// Featureful: a scale of 0 is drawn as 1, so a scale fade from 0 starts at 1
	f->startScale = scale > 0 ? Clamp( scale, 0, RENDERFADE_MAX_SCALE ) : 1.0f;
	f->endScale = pTarget->pev->scale;
	RenderFade_CountScaleFades( );

	RenderFade_Send( pTarget, f );

	// the box and the hitboxes follow the drawn scale from now on (RenderFade_Frame); a fade that replaced an
	// earlier one starts from where that one was
	if ( flags & RENDERFADE_SCALE )
		UTIL_ModelScaleChanged( pTarget->pev );
	return f->id;
}

BOOL RenderFade_IsCurrent( CBaseEntity *pTarget, int id )
{
	render_fade_t *f = RenderFade_Find( pTarget );
	return f && f->id == id;
}

void RenderFade_Finish( CBaseEntity *pTarget, int id )
{
	render_fade_t *f = RenderFade_Find( pTarget );
	if ( !f || f->id != id )
		return;

	f->hEntity = NULL;	// the clients end it themselves at the same time
	RenderFade_CountScaleFades( );

	// Model scale (docs/ENV_RENDER.md "Scale and collision"): during the fade the box and the hitboxes followed the
	// drawn scale each server frame (RenderFade_Frame, UTIL_ModelScale); pev->scale has held the end scale since
	// the start (late joiners), and the box takes it here exactly
	if ( f->flags & RENDERFADE_SCALE )
		UTIL_ModelScaleChanged( pTarget->pev );
}

void RenderFade_Reset( void )
{
	for ( int i = 0; i < RENDERFADE_MAX_ACTIVE; i++ )
		s_Fades[i].hEntity = NULL;
	s_iScaleFades = 0;
}

void RenderFade_Cancel( CBaseEntity *pTarget )
{
	render_fade_t *f = RenderFade_Find( pTarget );
	if ( !f )
		return;
	f->hEntity = NULL;
	RenderFade_CountScaleFades( );
	RenderFade_Send( pTarget, NULL );

	// a scale fade stopped half way: the box and the hitboxes take the scale it has now (pev->scale)
	if ( f->flags & RENDERFADE_SCALE )
		UTIL_ModelScaleChanged( pTarget->pev );
}
