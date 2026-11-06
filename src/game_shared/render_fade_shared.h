//=========================================================
// SBO 2.0 env_render fades (docs/ENV_RENDER.md): the "RenderFade" user message, server dlls/render_fade.cpp, client
// cl_dll/render_fade.cpp. One reliable message per faded entity; the client blends, the server keeps the end values.
//
//   SHORT  entity index
//   SHORT  modelindex at the start (the client ignores the fade on an entity that has another model: index reused)
//   BYTE   RENDERFADE_* flags (0: no fade, the client drops one it has for the entity)
//   only with flags:
//   LONG   start time, server time in ms
//   LONG   duration in ms
//   RENDERFADE_AMT:    BYTE start renderamt, BYTE end renderamt
//   RENDERFADE_COLOR:  3 BYTES start rendercolor, 3 BYTES end rendercolor
//   RENDERFADE_SCALE:  LONG start scale, LONG end scale (RENDERFADE_SCALE_UNIT per 1.0)
//=========================================================

#ifndef RENDER_FADE_SHARED_H
#define RENDER_FADE_SHARED_H

#define RENDERFADE_AMT		1
#define RENDERFADE_COLOR	2
#define RENDERFADE_SCALE	4

#define RENDERFADE_SCALE_UNIT	1000.0f
#define RENDERFADE_MAX_SCALE	1000.0f		// scales sent are clamped to 0 .. this
#define RENDERFADE_MAX_TIME		3600.0f		// longest fade, seconds

#endif // RENDER_FADE_SHARED_H
