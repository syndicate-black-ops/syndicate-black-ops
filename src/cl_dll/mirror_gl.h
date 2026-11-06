//=========================================================
// SBO 2.0: env_mirror: OpenGL user clip plane at the mirror's plane and the reversed front-face winding for the
// reflected copies (mirror_gl.cpp).
//=========================================================

#if !defined( MIRROR_GL_H )
#define MIRROR_GL_H

// Before the copies: false when there is no GL context, a function is missing or the engine uses the clip plane
// itself (then the copies are drawn unclipped).
bool MirrorGL_Begin( void );
// Before each copy: clip at plane on axis (0 X, 1 Y, 2 Z), keeping keepSide * ( p[axis] - plane ) >= 0 (the side
// behind the mirror as seen from the view: -1 or 1); the plane is in world space, put into eye space with the
// view being drawn (origin, forward, right, up: IEngineStudio.GetViewInfo).
void MirrorGL_SetPlane( int axis, float plane, int keepSide, const float *vieworg, const float *forward,
	const float *right, const float *up );
// After the copies: clip plane off, its previous equation restored.
void MirrorGL_End( void );

// Around the copies: the front-face winding turned around (glFrontFace), so that face culling keeps the reflected
// triangles that face the view; false without a GL context or the function (then culling must be off).
// MirrorGL_EndWinding restores the winding that was set.
bool MirrorGL_BeginWinding( void );
void MirrorGL_EndWinding( void );

#endif // MIRROR_GL_H
