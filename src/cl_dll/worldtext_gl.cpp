//=========================================================
// SBO 2.0: point_worldtext: the depth of the letters. docs/POINT_WORLDTEXT.md.
//
// The texts are drawn in HUD_DrawNormalTriangles (tri.cpp), after the world and the solid entities and before the
// engine's transparent entities (rendermode texture, additive, ...: drawn without depth writes). The letters are
// blended (the triangle API's kRenderTransAlpha, no depth write), so a transparent entity in front of a text is
// blended over it (seen through as through glass; opaque at FX amount 255), and one behind it would be blended over
// it too: a depth-only copy of the letters (no color, alpha test) puts their solid pixels into the depth buffer, so
// what is behind a letter stays behind it.
//
// The engine's state in that pass varies (docs/WORLD_SCREENS.md, "GL state"): in some places of a map texture unit 1
// (the lightmap unit) is the active one, with 2D texturing on. The triangle API would then bind the glyphs on unit 1
// and draw them through whatever unit 0 has (seen: the letters in broken blocks). WorldTextGL_Begin makes unit 0 the
// active unit with unit 1 off for the texts, and WorldTextGL_End puts back exactly what it read. The engine caches the
// active unit and each unit's texture (GL_SelectTexture, GL_Bind), so the state found when unit 0 is already the
// active unit and unit 1 is off (the usual case) is left as it is, as in the transparent pass.
//
// The GL functions are looked up in the current context as cl_dll/mirror_gl.cpp does, so no GL header or import
// library is needed: Windows: the GL 1.1 functions from opengl32.dll (the engine has it loaded; GetModuleHandle, not
// linked); Linux: SDL_GL_GetProcAddress (SDL2 is linked already, inputw32.cpp).
//=========================================================

#include "worldtext_gl.h"

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#define WGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define WGL_APIENTRY
#endif

typedef unsigned int	WGLenum;
typedef unsigned char	WGLboolean;
typedef int				WGLint;
typedef float			WGLfloat;

#define WGL_ALPHA_TEST			0x0BC0
#define WGL_ALPHA_TEST_FUNC		0x0BC1
#define WGL_ALPHA_TEST_REF		0x0BC2
#define WGL_DEPTH_WRITEMASK		0x0B72
#define WGL_COLOR_WRITEMASK		0x0C23
#define WGL_GREATER				0x0204
#define WGL_TEXTURE_2D			0x0DE1
#define WGL_TEXTURE_BINDING_2D	0x8069
#define WGL_TEXTURE_ENV			0x2300
#define WGL_TEXTURE_ENV_MODE	0x2200
#define WGL_ACTIVE_TEXTURE		0x84E0
#define WGL_TEXTURE0			0x84C0
#define WGL_TEXTURE1			0x84C1

static struct
{
	void		( WGL_APIENTRY *Enable )( WGLenum cap );
	void		( WGL_APIENTRY *Disable )( WGLenum cap );
	WGLboolean	( WGL_APIENTRY *IsEnabled )( WGLenum cap );
	void		( WGL_APIENTRY *GetIntegerv )( WGLenum pname, WGLint *params );
	void		( WGL_APIENTRY *GetFloatv )( WGLenum pname, WGLfloat *params );
	void		( WGL_APIENTRY *GetBooleanv )( WGLenum pname, WGLboolean *params );
	void		( WGL_APIENTRY *AlphaFunc )( WGLenum func, WGLfloat ref );
	void		( WGL_APIENTRY *DepthMask )( WGLboolean flag );
	void		( WGL_APIENTRY *ColorMask )( WGLboolean r, WGLboolean g, WGLboolean b, WGLboolean a );
	void		( WGL_APIENTRY *BindTexture )( WGLenum target, unsigned int texture );
	void		( WGL_APIENTRY *GetTexEnviv )( WGLenum target, WGLenum pname, WGLint *params );
	void		( WGL_APIENTRY *TexEnvi )( WGLenum target, WGLenum pname, WGLint param );
	void		( WGL_APIENTRY *ActiveTexture )( WGLenum texture );	// GL 1.3 / ARB_multitexture; NULL without
} qgl;

static void		*s_pContext = NULL;		// the context the pointers were looked up in
static bool		s_bAvailable = false;

// WorldTextGL_DepthBegin / End
static bool			s_bActive = false;
static WGLboolean	s_bAlphaTest, s_bDepthMask, s_ColorMask[4];
static WGLint		s_iAlphaFunc;
static WGLfloat		s_flAlphaRef;

// WorldTextGL_Begin / End
static bool			s_bUnitsChanged = false;
static WGLint		s_iActiveTexture, s_iTexture0, s_iTexEnv0;
static WGLboolean	s_bTex2D0, s_bTex2D1;

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
	HMODULE gl = GetModuleHandleA( "opengl32.dll" );
	if ( !gl )
		return NULL;

	// GL 1.1 functions are exported by opengl32.dll itself; wglGetProcAddress only knows extensions
	FARPROC p = GetProcAddress( gl, name );
	if ( !p )
	{
		typedef PROC ( WINAPI *pfnGetProcAddress )( LPCSTR );
		pfnGetProcAddress wglGet = (pfnGetProcAddress)GetProcAddress( gl, "wglGetProcAddress" );
		PROC q = wglGet ? wglGet( name ) : NULL;
		// some drivers return small integers instead of NULL
		if ( q == (PROC)1 || q == (PROC)2 || q == (PROC)3 || q == (PROC)-1 )
			q = NULL;
		p = (FARPROC)q;
	}
	return (void *)p;
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
	void *context = CurrentContext();
	if ( !context )
		return false;

	if ( context != s_pContext )
	{
		s_pContext = context;
		s_bAvailable =
			Resolve( qgl.Enable, "glEnable" ) &&
			Resolve( qgl.Disable, "glDisable" ) &&
			Resolve( qgl.IsEnabled, "glIsEnabled" ) &&
			Resolve( qgl.GetIntegerv, "glGetIntegerv" ) &&
			Resolve( qgl.GetFloatv, "glGetFloatv" ) &&
			Resolve( qgl.GetBooleanv, "glGetBooleanv" ) &&
			Resolve( qgl.AlphaFunc, "glAlphaFunc" ) &&
			Resolve( qgl.DepthMask, "glDepthMask" ) &&
			Resolve( qgl.ColorMask, "glColorMask" ) &&
			Resolve( qgl.BindTexture, "glBindTexture" ) &&
			Resolve( qgl.GetTexEnviv, "glGetTexEnviv" ) &&
			Resolve( qgl.TexEnvi, "glTexEnvi" );
		if ( s_bAvailable && !Resolve( qgl.ActiveTexture, "glActiveTexture" ) )
			Resolve( qgl.ActiveTexture, "glActiveTextureARB" );
	}

	return s_bAvailable;
}

bool WorldTextGL_DepthBegin( void )
{
	s_bActive = false;

	if ( !Available() )
		return false;

	s_bAlphaTest = qgl.IsEnabled( WGL_ALPHA_TEST );
	qgl.GetIntegerv( WGL_ALPHA_TEST_FUNC, &s_iAlphaFunc );
	qgl.GetFloatv( WGL_ALPHA_TEST_REF, &s_flAlphaRef );
	qgl.GetBooleanv( WGL_DEPTH_WRITEMASK, &s_bDepthMask );
	qgl.GetBooleanv( WGL_COLOR_WRITEMASK, s_ColorMask );

	qgl.ColorMask( 0, 0, 0, 0 );
	qgl.DepthMask( 1 );
	qgl.Enable( WGL_ALPHA_TEST );
	qgl.AlphaFunc( WGL_GREATER, 0.5f );

	s_bActive = true;
	return true;
}

void WorldTextGL_DepthEnd( void )
{
	if ( !s_bActive )
		return;

	s_bActive = false;
	qgl.AlphaFunc( (WGLenum)s_iAlphaFunc, s_flAlphaRef );
	if ( !s_bAlphaTest )
		qgl.Disable( WGL_ALPHA_TEST );
	qgl.DepthMask( s_bDepthMask );
	qgl.ColorMask( s_ColorMask[0], s_ColorMask[1], s_ColorMask[2], s_ColorMask[3] );
}

void WorldTextGL_Begin( void )
{
	s_bUnitsChanged = false;

	if ( !Available() || !qgl.ActiveTexture )
		return;	// one texture unit: nothing to change

	qgl.GetIntegerv( WGL_ACTIVE_TEXTURE, &s_iActiveTexture );
	qgl.ActiveTexture( WGL_TEXTURE1 );
	s_bTex2D1 = qgl.IsEnabled( WGL_TEXTURE_2D );
	if ( s_iActiveTexture == WGL_TEXTURE0 && !s_bTex2D1 )
	{
		qgl.ActiveTexture( WGL_TEXTURE0 );
		return;
	}

	if ( s_bTex2D1 )
		qgl.Disable( WGL_TEXTURE_2D );
	qgl.ActiveTexture( WGL_TEXTURE0 );
	s_bTex2D0 = qgl.IsEnabled( WGL_TEXTURE_2D );
	qgl.GetIntegerv( WGL_TEXTURE_BINDING_2D, &s_iTexture0 );
	qgl.GetTexEnviv( WGL_TEXTURE_ENV, WGL_TEXTURE_ENV_MODE, &s_iTexEnv0 );
	if ( !s_bTex2D0 )
		qgl.Enable( WGL_TEXTURE_2D );
	s_bUnitsChanged = true;
}

void WorldTextGL_End( void )
{
	if ( !s_bUnitsChanged )
		return;

	s_bUnitsChanged = false;
	qgl.ActiveTexture( WGL_TEXTURE0 );
	qgl.BindTexture( WGL_TEXTURE_2D, (unsigned int)s_iTexture0 );
	qgl.TexEnvi( WGL_TEXTURE_ENV, WGL_TEXTURE_ENV_MODE, s_iTexEnv0 );
	if ( !s_bTex2D0 )
		qgl.Disable( WGL_TEXTURE_2D );
	if ( s_bTex2D1 )
	{
		qgl.ActiveTexture( WGL_TEXTURE1 );
		qgl.Enable( WGL_TEXTURE_2D );
	}
	qgl.ActiveTexture( (WGLenum)s_iActiveTexture );
}
