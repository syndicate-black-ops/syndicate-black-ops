//=========================================================
// SBO 2.0 env_overlay: the .tga pictures as OpenGL textures (overlay_gl.cpp). The engine has no call that loads a
// .tga of any size for the triangle API, so overlay.cpp decodes the file and these put it into a texture of its own.
//=========================================================

#if !defined( OVERLAY_GL_H )
#define OVERLAY_GL_H

// The current GL context (NULL: none, e.g. the software renderer); a texture belongs to the context it was made in.
void *OverlayGL_Context( void );
// A new texture from wide x tall RGBA pixels, top row first; 0: no GL or it failed.
unsigned int OverlayGL_CreateTexture( const unsigned char *rgba, int wide, int tall );
void OverlayGL_DeleteTexture( unsigned int tex );
// Binds tex for the triangle API's next quads; returns the texture that was bound, for OverlayGL_Restore (the engine
// remembers what it bound last and skips binding it again).
unsigned int OverlayGL_Bind( unsigned int tex );
void OverlayGL_Restore( unsigned int previous );

#endif // OVERLAY_GL_H
