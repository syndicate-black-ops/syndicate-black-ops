//=========================================================
// SBO 2.0 env_overlay: .tga pictures as OpenGL textures. See overlay_gl.h.
//
// The GL functions are looked up in the current context as cl_dll/worldtext_gl.cpp does, so no GL header or import
// library is needed: Windows: the GL 1.1 functions from opengl32.dll (the engine has it loaded; GetModuleHandle, not
// linked); Linux: SDL_GL_GetProcAddress (SDL2 is linked already, inputw32.cpp).
//=========================================================

#include "overlay_gl.h"

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#define OGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define OGL_APIENTRY
#endif

typedef unsigned int	OGLenum;
typedef int				OGLint;
typedef int				OGLsizei;
typedef unsigned int	OGLuint;

#define OGL_TEXTURE_2D			0x0DE1
#define OGL_TEXTURE_BINDING_2D	0x8069
#define OGL_TEXTURE_MIN_FILTER	0x2801
#define OGL_TEXTURE_MAG_FILTER	0x2800
#define OGL_TEXTURE_WRAP_S		0x2802
#define OGL_TEXTURE_WRAP_T		0x2803
#define OGL_LINEAR				0x2601
#define OGL_CLAMP_TO_EDGE		0x812F
#define OGL_RGBA				0x1908
#define OGL_RGBA8				0x8058
#define OGL_UNSIGNED_BYTE		0x1401
#define OGL_UNPACK_ALIGNMENT	0x0CF5

static struct
{
	void	( OGL_APIENTRY *GenTextures )( OGLsizei n, OGLuint *textures );
	void	( OGL_APIENTRY *DeleteTextures )( OGLsizei n, const OGLuint *textures );
	void	( OGL_APIENTRY *BindTexture )( OGLenum target, OGLuint texture );
	void	( OGL_APIENTRY *TexImage2D )( OGLenum target, OGLint level, OGLint internalformat, OGLsizei width,
				OGLsizei height, OGLint border, OGLenum format, OGLenum type, const void *pixels );
	void	( OGL_APIENTRY *TexParameteri )( OGLenum target, OGLenum pname, OGLint param );
	void	( OGL_APIENTRY *GetIntegerv )( OGLenum pname, OGLint *params );
	void	( OGL_APIENTRY *PixelStorei )( OGLenum pname, OGLint param );
} qgl;

static void	*s_pContext = NULL;		// the context the pointers were looked up in
static bool	s_bAvailable = false;

void *OverlayGL_Context( void )
{
#ifdef _WIN32
	typedef HGLRC ( WINAPI *pfnGetCurrentContext )( void );
	static pfnGetCurrentContext s_pfn = NULL;
	if ( !s_pfn )
	{
		HMODULE gl = GetModuleHandleA( "opengl32.dll" );
		s_pfn = gl ? (pfnGetCurrentContext)GetProcAddress( gl, "wglGetCurrentContext" ) : NULL;
		if ( !s_pfn )
			return NULL;
	}
	return (void *)s_pfn();
#else
	return (void *)SDL_GL_GetCurrentContext();
#endif
}

static void *GetProc( const char *name )
{
#ifdef _WIN32
	HMODULE gl = GetModuleHandleA( "opengl32.dll" );
	if ( !gl )
		return NULL;
	// all GL 1.1: exported by opengl32.dll itself
	return (void *)GetProcAddress( gl, name );
#else
	return SDL_GL_GetProcAddress( name );
#endif
}

template <typename T> static bool Resolve( T &fn, const char *name )
{
	fn = (T)GetProc( name );
	return fn != NULL;
}

static bool Available( void )
{
	void *context = OverlayGL_Context();
	if ( !context )
		return false;

	if ( context != s_pContext )
	{
		s_pContext = context;
		s_bAvailable =
			Resolve( qgl.GenTextures, "glGenTextures" ) &&
			Resolve( qgl.DeleteTextures, "glDeleteTextures" ) &&
			Resolve( qgl.BindTexture, "glBindTexture" ) &&
			Resolve( qgl.TexImage2D, "glTexImage2D" ) &&
			Resolve( qgl.TexParameteri, "glTexParameteri" ) &&
			Resolve( qgl.GetIntegerv, "glGetIntegerv" ) &&
			Resolve( qgl.PixelStorei, "glPixelStorei" );
	}

	return s_bAvailable;
}

unsigned int OverlayGL_CreateTexture( const unsigned char *rgba, int wide, int tall )
{
	if ( !rgba || wide < 1 || tall < 1 || !Available() )
		return 0;

	OGLint previous = 0, alignment = 4;
	qgl.GetIntegerv( OGL_TEXTURE_BINDING_2D, &previous );
	qgl.GetIntegerv( OGL_UNPACK_ALIGNMENT, &alignment );

	OGLuint tex = 0;
	qgl.GenTextures( 1, &tex );
	if ( tex )
	{
		qgl.BindTexture( OGL_TEXTURE_2D, tex );
		qgl.PixelStorei( OGL_UNPACK_ALIGNMENT, 1 );
		qgl.TexParameteri( OGL_TEXTURE_2D, OGL_TEXTURE_MIN_FILTER, OGL_LINEAR );
		qgl.TexParameteri( OGL_TEXTURE_2D, OGL_TEXTURE_MAG_FILTER, OGL_LINEAR );
		qgl.TexParameteri( OGL_TEXTURE_2D, OGL_TEXTURE_WRAP_S, OGL_CLAMP_TO_EDGE );
		qgl.TexParameteri( OGL_TEXTURE_2D, OGL_TEXTURE_WRAP_T, OGL_CLAMP_TO_EDGE );
		qgl.TexImage2D( OGL_TEXTURE_2D, 0, OGL_RGBA8, wide, tall, 0, OGL_RGBA, OGL_UNSIGNED_BYTE, rgba );
		qgl.PixelStorei( OGL_UNPACK_ALIGNMENT, alignment );
	}

	qgl.BindTexture( OGL_TEXTURE_2D, (OGLuint)previous );
	return tex;
}

void OverlayGL_DeleteTexture( unsigned int tex )
{
	if ( tex && Available() )
	{
		OGLuint t = tex;
		qgl.DeleteTextures( 1, &t );
	}
}

unsigned int OverlayGL_Bind( unsigned int tex )
{
	if ( !Available() )
		return 0;

	OGLint previous = 0;
	qgl.GetIntegerv( OGL_TEXTURE_BINDING_2D, &previous );
	qgl.BindTexture( OGL_TEXTURE_2D, tex );
	return (unsigned int)previous;
}

void OverlayGL_Restore( unsigned int previous )
{
	if ( Available() )
		qgl.BindTexture( OGL_TEXTURE_2D, previous );
}
