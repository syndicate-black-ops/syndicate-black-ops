//=========================================================
// SBO 2.0: game_playvideo, the picture on the screen (video_gl.cpp, docs/VIDEO_PLAYER.md). OpenGL through function
// pointers looked up in the current context, as cl_dll/studio_fullbright_gl.cpp does: no GL header or import library.
//=========================================================

#if !defined( VIDEO_GL_H )
#define VIDEO_GL_H

// Draws the whole screen black and the picture (width x height RGBA pixels, top row first) on it as large as fits,
// centered with its proportions kept. changed: the pixels are new since the last call (uploaded again; they are also
// uploaded again after a video mode change). False without a GL context or a function (nothing drawn).
bool VideoGL_Draw( const unsigned char *rgba, int width, int height, bool changed );

// Frees the texture (when the context that made it is still current).
void VideoGL_Release( void );

#endif // VIDEO_GL_H
