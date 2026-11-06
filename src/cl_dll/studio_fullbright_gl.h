//=========================================================
// SBO 2.0: fullbright model textures (STUDIO_NF_FULLBRIGHT, docs/MODEL_FULLBRIGHT.md): the OpenGL texture that
// hides a mesh in one of the two passes (StudioModelRenderer.cpp StudioRenderFullbright). studio_fullbright_gl.cpp.
//=========================================================

#if !defined( STUDIO_FULLBRIGHT_GL_H )
#define STUDIO_FULLBRIGHT_GL_H

// The GL name of a 1 x 1 texture with nothing in it (black, alpha 0) in the current context, made the first time it
// is asked for in a context. 0: no GL context (software renderer) or a GL function is missing: no fullbright pass.
unsigned int StudioFullbrightGL_HiddenTexture( void );

#endif // STUDIO_FULLBRIGHT_GL_H
