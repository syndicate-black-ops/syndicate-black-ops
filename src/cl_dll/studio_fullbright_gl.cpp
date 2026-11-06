//=========================================================
// SBO 2.0: fullbright model textures (STUDIO_NF_FULLBRIGHT, docs/MODEL_FULLBRIGHT.md).
//
// Valve's GoldSrc engine (Steam Half-Life 25th anniversary included) does not draw the flag: a fullbright texture is
// lit like any other. StudioModelRenderer.cpp (StudioRenderFullbright) draws such a model in two passes, as
// halflife-featureful does (from BlueNightHawk's halflife_fullbright): the model lit with its fullbright textures
// hidden, then the fullbright textures at full light with the others hidden. A texture is hidden by pointing it, for
// the pass, at the texture made here and marking it additive: it adds nothing to the screen.
//
// The texture is 1 x 1, black with alpha 0 (nothing is added with either additive blend function, with fog too). Its
// name comes from glGenTextures, as featureful's does; it is made again in a new context (video mode change).
//
// The GL functions are looked up in the current context as cl_dll/mirror_gl.cpp does (branch feat/env-mirror), so
// no GL header or import library is needed: Windows: the GL 1.1 functions from opengl32.dll (the engine has it
// loaded; GetModuleHandle, not linked); Linux: SDL_GL_GetProcAddress (SDL2 is linked already, inputw32.cpp).
//=========================================================

#include "studio_fullbright_gl.h"

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#define FGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define FGL_APIENTRY
#endif

typedef unsigned int	FGLenum;
typedef unsigned int	FGLuint;
typedef unsigned char	FGLboolean;
typedef int				FGLint;
typedef int				FGLsizei;

#define FGL_TEXTURE_2D				0x0DE1
#define FGL_TEXTURE_BINDING_2D		0x8069
#define FGL_RGBA					0x1908
#define FGL_UNSIGNED_BYTE			0x1401
#define FGL_TEXTURE_MIN_FILTER		0x2801
#define FGL_TEXTURE_MAG_FILTER		0x2800
#define FGL_TEXTURE_WRAP_S			0x2802
#define FGL_TEXTURE_WRAP_T			0x2803
#define FGL_NEAREST					0x2600
#define FGL_CLAMP					0x2900

static struct
{
	void		( FGL_APIENTRY *GenTextures )( FGLsizei n, FGLuint *textures );
	void		( FGL_APIENTRY *BindTexture )( FGLenum target, FGLuint texture );
	void		( FGL_APIENTRY *TexImage2D )( FGLenum target, FGLint level, FGLint internalformat, FGLsizei width,
					FGLsizei height, FGLint border, FGLenum format, FGLenum type, const void *pixels );
	void		( FGL_APIENTRY *TexParameteri )( FGLenum target, FGLenum pname, FGLint param );
	void		( FGL_APIENTRY *GetIntegerv )( FGLenum pname, FGLint *params );
	FGLboolean	( FGL_APIENTRY *IsTexture )( FGLuint texture );
} qgl;

static void		*s_pContext = NULL;		// the context the pointers were looked up in
static bool		s_bAvailable = false;
static FGLuint	s_iTexture = 0;			// the hidden texture's name in that context

static void *CurrentContext( void )
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
	// GL 1.1 functions are exported by opengl32.dll itself
	HMODULE gl = GetModuleHandleA( "opengl32.dll" );
	return gl ? (void *)GetProcAddress( gl, name ) : NULL;
#else
	return SDL_GL_GetProcAddress( name );
#endif
}

template <typename T> static bool Resolve( T &fn, const char *name )
{
	fn = (T)GetProc( name );
	return fn != NULL;
}

unsigned int StudioFullbrightGL_HiddenTexture( void )
{
	void *context = CurrentContext();
	if ( !context )
		return 0;

	if ( context != s_pContext )
	{
		s_pContext = context;
		s_iTexture = 0;
		s_bAvailable =
			Resolve( qgl.GenTextures, "glGenTextures" ) &&
			Resolve( qgl.BindTexture, "glBindTexture" ) &&
			Resolve( qgl.TexImage2D, "glTexImage2D" ) &&
			Resolve( qgl.TexParameteri, "glTexParameteri" ) &&
			Resolve( qgl.GetIntegerv, "glGetIntegerv" ) &&
			Resolve( qgl.IsTexture, "glIsTexture" );
	}

	if ( !s_bAvailable )
		return 0;

	if ( !s_iTexture || !qgl.IsTexture( s_iTexture ) )
	{
		static const unsigned char pixel[4] = { 0, 0, 0, 0 };

		s_iTexture = 0;
		qgl.GenTextures( 1, &s_iTexture );
		if ( !s_iTexture )
			return 0;

		// the engine keeps track of the bound texture: put its binding back afterwards
		FGLint previous = 0;
		qgl.GetIntegerv( FGL_TEXTURE_BINDING_2D, &previous );

		qgl.BindTexture( FGL_TEXTURE_2D, s_iTexture );
		qgl.TexImage2D( FGL_TEXTURE_2D, 0, FGL_RGBA, 1, 1, 0, FGL_RGBA, FGL_UNSIGNED_BYTE, pixel );
		// no mipmaps: the default minification filter would leave the texture incomplete (drawn untextured)
		qgl.TexParameteri( FGL_TEXTURE_2D, FGL_TEXTURE_MIN_FILTER, FGL_NEAREST );
		qgl.TexParameteri( FGL_TEXTURE_2D, FGL_TEXTURE_MAG_FILTER, FGL_NEAREST );
		qgl.TexParameteri( FGL_TEXTURE_2D, FGL_TEXTURE_WRAP_S, FGL_CLAMP );
		qgl.TexParameteri( FGL_TEXTURE_2D, FGL_TEXTURE_WRAP_T, FGL_CLAMP );

		qgl.BindTexture( FGL_TEXTURE_2D, (FGLuint)previous );

		if ( !qgl.IsTexture( s_iTexture ) )
		{
			s_iTexture = 0;
			return 0;
		}
	}

	return s_iTexture;
}
