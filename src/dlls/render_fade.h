//=========================================================
// SBO 2.0 env_render fades, server side (render_fade.cpp, docs/ENV_RENDER.md). A fade sets the entity's end values
// in pev at once (late joiners, spectators and demos get the end state) and sends one "RenderFade" message; the
// clients draw the values in between (cl_dll/render_fade.cpp). No entity update per step, no delta field.
//=========================================================

#ifndef RENDER_FADE_H
#define RENDER_FADE_H

#include "render_fade_shared.h"

// Starts a fade of pTarget from what it shows now (its pev values, or the point an earlier fade has reached) to the
// given end values (only the fields in flags: RENDERFADE_*), over flDuration seconds. Writes the end values into
// pev. Returns the fade's id (0: no fade made; the end values are set anyway).
int RenderFade_Start( CBaseEntity *pTarget, int flags, float endAmt, const Vector &endColor, float endScale, float flDuration );

// pTarget's fade with this id is still the one running (no later fade or change replaced it)
BOOL RenderFade_IsCurrent( CBaseEntity *pTarget, int id );

// The fade is done (the env_render's end): forgets it; the model scale's collision box follows the end scale here
void RenderFade_Finish( CBaseEntity *pTarget, int id );

// An instant change of pTarget's render values: a fade running on it is stopped on the clients too
void RenderFade_Cancel( CBaseEntity *pTarget );

// ServerDeactivate: the map's fades are gone
void RenderFade_Reset( void );

// The scale pev's model is drawn at now when a scale fade runs on it (TRUE; *pScale set), else FALSE. The model
// scale's box and hitboxes follow it (util.cpp UTIL_ModelScale)
BOOL RenderFade_ScaleNow( const entvars_t *pev, float *pScale );

// StartFrame: the box of each entity in a scale fade follows the scale of this moment
void RenderFade_Frame( void );

// What pTarget shows now: its pev values, or the point its fade has reached
void RenderFade_Current( CBaseEntity *pTarget, float *pAmt, Vector *pColor, float *pScale );

#endif // RENDER_FADE_H
