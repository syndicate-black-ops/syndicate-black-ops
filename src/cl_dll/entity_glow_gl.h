//=========================================================
// SBO 2.1 env_entity_glow: the OpenGL pass (entity_glow_gl.cpp). docs/ENV_ENTITY_GLOW.md "How it is drawn".
//=========================================================

#ifndef SBO_CL_DLL_ENTITY_GLOW_GL_H
#define SBO_CL_DLL_ENTITY_GLOW_GL_H

enum
{
	ENTGLOW_GL_ALWAYS = 0,		// Source's modes (env_entity_glow "Mode")
	ENTGLOW_GL_OCCLUDED = 1,
	ENTGLOW_GL_VISIBLE = 2,
};

enum
{
	ENTGLOW_GL_OUTLINE = 0,		// Source's styles (env_entity_glow "Style")
	ENTGLOW_GL_HALO = 1,
};

//! Starts the pass for the view being drawn (the GL view matrices and viewport are the view's): copies the view's
//! depth (fogzone_gl.cpp) and makes the off-screen masks. Every GL value it changes is read first and put back by
//! EntityGlowGL_End. False: the pass can't run in this context (EntityGlowGL_Why says why); nothing was changed and
//! EntityGlowGL_End must not be called.
bool EntityGlowGL_Begin( void );

//! The glow the triangles that follow belong to: style (ENTGLOW_GL_OUTLINE / HALO), color r g b a 0..1, mode
//! (ENTGLOW_GL_ALWAYS / OCCLUDED / VISIBLE). pass 0: the glow's pixels; pass 1 (OCCLUDED only, after pass 0 of
//! the same geometry): clears the pixels where the geometry is seen, so a part hidden behind the target itself does
//! not count as hidden.
void EntityGlowGL_Target( int style, const float *color, int mode, int pass );

//! The geometry, in world coordinates: GL_TRIANGLE_STRIP (5), GL_TRIANGLE_FAN (6) or GL_POLYGON (9)
void EntityGlowGL_BeginPrimitive( int primitive );
void EntityGlowGL_Vertex( const float *xyz );
void EntityGlowGL_EndPrimitive( void );

//! A triangle list: numverts corners (3 floats each, world coordinates) and count indexes into them (3 per triangle).
//! Drawn from client memory with glDrawElements (the vertex array state is saved on the first one of a pass and put
//! back at EntityGlowGL_End), or with glBegin / glVertex where the driver has no vertex arrays.
void EntityGlowGL_DrawIndexed( const float *xyz, int numverts, const unsigned short *indices, int count );

//! Blurs the halos, adds the outlines and halos to the view outside the targets, and puts back every GL value
//! EntityGlowGL_Begin changed
void EntityGlowGL_End( void );

//! A GL error the pass left in the first 600 passes of this GL context (checked once per context), then 0
unsigned int EntityGlowGL_TakeError( void );

//! Why the pass can't run (empty when it can)
const char *EntityGlowGL_Why( void );
//! Bumped whenever a new GL context was found (the developer console line is printed again for it)
int EntityGlowGL_Generation( void );

#endif // SBO_CL_DLL_ENTITY_GLOW_GL_H
