//=========================================================
// SBO 2.0 color_correction: the OpenGL pass (colorcorrection_gl.cpp). docs/COLOR_CORRECTION.md "How it is drawn".
//=========================================================

#ifndef SBO_CL_DLL_COLORCORRECTION_GL_H
#define SBO_CL_DLL_COLORCORRECTION_GL_H

#include <vector>

namespace colorcorr
{
struct Lut;
}

//! The GL context with GLSL 1.20, 3D textures and the functions used here. False on the software renderer, D3D or an
//! old driver; ColorCorrGL_Why says why.
bool ColorCorrGL_Available();
const char *ColorCorrGL_Why();
//! The current GL context (a new one means the tables' textures are gone: ColorCorrGL_Available forgets them)
void *ColorCorrGL_Context();
//! Bumped whenever a new context was found (tables made in an older one are gone)
int ColorCorrGL_Generation();

//! The viewport now (the view being drawn); false without the GL path
bool ColorCorrGL_GetViewport( int vp[4] );

//! A table as a 3D texture (0: not made)
unsigned int ColorCorrGL_CreateLut( const colorcorr::Lut &lut );
void ColorCorrGL_DeleteLut( unsigned int tex );

//! Grades what is drawn inside the viewport vp (x, y, width, height in window pixels from the bottom left) of the
//! framebuffer being drawn to: the picture times identity, plus each table times its weight. sizes: each table's size.
//! Every GL value it changes is read first and put back exactly. False: nothing done.
bool ColorCorrGL_Apply( const int vp[4], float identity, int count, const unsigned int *tex, const int *sizes,
	const float *weights );

//! The next ColorCorrGL_Apply also reads the view it copied (before grading) into out: RGB, bottom row first,
//! wide x tall (the viewport). Null: no copy. Needs glGetTexImage (GL 1.1); left empty without it.
void ColorCorrGL_SetSnapshot( std::vector<unsigned char> *out, int *wide, int *tall );

//! Debug checks (the client turns them on with developer): glGetError around each pass
void ColorCorrGL_SetDebug( bool on );
//! The GL error found after the last pass (0: none); engineError: one the engine had left before it
unsigned int ColorCorrGL_LastError( unsigned int *engineError );
//! The GL error found right after the last ColorCorrGL_CreateLut (debug checks on; 0: none)
unsigned int ColorCorrGL_LastTableError();
//! Tables alive (made and not deleted) in this context, and how many of the pass's own objects were made in all
//! contexts (program, screen texture, identity table, resolve framebuffer: once per context)
void ColorCorrGL_Counts( int *liveTables, int *objectsMade );

#endif // SBO_CL_DLL_COLORCORRECTION_GL_H
