//=========================================================
// SBO 2.0: game_playvideo, the picture on the screen (video_gl.h, docs/VIDEO_PLAYER.md).
//
// As Aura-CL's CVideoPlayer (CreateTextureIfNeeded / UploadFrame / Draw): one RGBA texture, filled with each new
// frame, drawn over the screen at the end of the HUD redraw in an orthographic projection of its own. Changes: the
// texture is a power of two in size with the picture in its top-left corner (no reliance on non-power-of-two
// textures), the picture keeps its proportions (black bars) instead of being stretched, the size comes from the GL
// viewport (real pixels, whatever the HUD scale), and the engine's texture binding is put back (it tracks it).
//
// The GL functions are looked up in the current context as cl_dll/studio_fullbright_gl.cpp does: Windows: the GL 1.1
// functions from opengl32.dll (the engine has it loaded; GetModuleHandle, not linked); Linux: SDL_GL_GetProcAddress
// (SDL2 is linked already, inputw32.cpp). A new context (video mode change) gets a new texture.
//
// Compiled with SBO's client flags (sbo_compat.h); no SBO headers.
//=========================================================

#include "video_gl.h"

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#define VGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define VGL_APIENTRY
#endif

namespace
{
typedef unsigned int	VGLenum;
typedef unsigned int	VGLuint;
typedef unsigned int	VGLbitfield;
typedef unsigned char	VGLboolean;
typedef int				VGLint;
typedef int				VGLsizei;
typedef float			VGLfloat;
typedef double			VGLdouble;

const VGLenum VGL_TEXTURE_2D			= 0x0DE1;
const VGLenum VGL_TEXTURE_BINDING_2D	= 0x8069;
const VGLenum VGL_VIEWPORT				= 0x0BA2;
const VGLenum VGL_RGBA					= 0x1908;
const VGLenum VGL_UNSIGNED_BYTE			= 0x1401;
const VGLenum VGL_TEXTURE_MIN_FILTER	= 0x2801;
const VGLenum VGL_TEXTURE_MAG_FILTER	= 0x2800;
const VGLenum VGL_TEXTURE_WRAP_S		= 0x2802;
const VGLenum VGL_TEXTURE_WRAP_T		= 0x2803;
const VGLenum VGL_LINEAR				= 0x2601;
const VGLenum VGL_CLAMP					= 0x2900;
const VGLenum VGL_TEXTURE_ENV			= 0x2300;
const VGLenum VGL_TEXTURE_ENV_MODE		= 0x2200;
const VGLenum VGL_REPLACE				= 0x1E01;
const VGLenum VGL_UNPACK_ALIGNMENT		= 0x0CF5;
const VGLenum VGL_UNPACK_ROW_LENGTH		= 0x0CF2;
const VGLenum VGL_DEPTH_TEST			= 0x0B71;
const VGLenum VGL_CULL_FACE				= 0x0B44;
const VGLenum VGL_ALPHA_TEST			= 0x0BC0;
const VGLenum VGL_BLEND					= 0x0BE2;
const VGLenum VGL_FOG					= 0x0B60;
const VGLenum VGL_SCISSOR_TEST			= 0x0C11;
const VGLenum VGL_PROJECTION			= 0x1701;
const VGLenum VGL_MODELVIEW				= 0x1700;
const VGLenum VGL_QUADS					= 0x0007;
const VGLbitfield VGL_CURRENT_BIT		= 0x00000001;
const VGLbitfield VGL_DEPTH_BUFFER_BIT	= 0x00000100;
const VGLbitfield VGL_TRANSFORM_BIT		= 0x00001000;
const VGLbitfield VGL_ENABLE_BIT		= 0x00002000;
const VGLbitfield VGL_COLOR_BUFFER_BIT	= 0x00004000;
const VGLbitfield VGL_TEXTURE_BIT		= 0x00040000;

struct
{
	void		( VGL_APIENTRY *GenTextures )( VGLsizei n, VGLuint *textures );
	void		( VGL_APIENTRY *DeleteTextures )( VGLsizei n, const VGLuint *textures );
	void		( VGL_APIENTRY *BindTexture )( VGLenum target, VGLuint texture );
	VGLboolean	( VGL_APIENTRY *IsTexture )( VGLuint texture );
	void		( VGL_APIENTRY *TexImage2D )( VGLenum target, VGLint level, VGLint internalformat, VGLsizei width,
					VGLsizei height, VGLint border, VGLenum format, VGLenum type, const void *pixels );
	void		( VGL_APIENTRY *TexSubImage2D )( VGLenum target, VGLint level, VGLint xoffset, VGLint yoffset,
					VGLsizei width, VGLsizei height, VGLenum format, VGLenum type, const void *pixels );
	void		( VGL_APIENTRY *TexParameteri )( VGLenum target, VGLenum pname, VGLint param );
	void		( VGL_APIENTRY *TexEnvi )( VGLenum target, VGLenum pname, VGLint param );
	void		( VGL_APIENTRY *PixelStorei )( VGLenum pname, VGLint param );
	void		( VGL_APIENTRY *GetIntegerv )( VGLenum pname, VGLint *params );
	void		( VGL_APIENTRY *Enable )( VGLenum cap );
	void		( VGL_APIENTRY *Disable )( VGLenum cap );
	void		( VGL_APIENTRY *PushAttrib )( VGLbitfield mask );
	void		( VGL_APIENTRY *PopAttrib )( void );
	void		( VGL_APIENTRY *MatrixMode )( VGLenum mode );
	void		( VGL_APIENTRY *PushMatrix )( void );
	void		( VGL_APIENTRY *PopMatrix )( void );
	void		( VGL_APIENTRY *LoadIdentity )( void );
	void		( VGL_APIENTRY *Ortho )( VGLdouble l, VGLdouble r, VGLdouble b, VGLdouble t, VGLdouble n, VGLdouble f );
	void		( VGL_APIENTRY *Color4f )( VGLfloat r, VGLfloat g, VGLfloat b, VGLfloat a );
	void		( VGL_APIENTRY *Begin )( VGLenum mode );
	void		( VGL_APIENTRY *End )( void );
	void		( VGL_APIENTRY *TexCoord2f )( VGLfloat s, VGLfloat t );
	void		( VGL_APIENTRY *Vertex2f )( VGLfloat x, VGLfloat y );
} qgl;

void		*s_pContext = NULL;	// the context the pointers were looked up in
bool		s_bAvailable = false;
VGLuint		s_iTexture = 0;		// in that context
int			s_iTexWide = 0, s_iTexTall = 0;
int			s_iPicWide = 0, s_iPicTall = 0;

void *CurrentContext( void )
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

void *GetProc( const char *name )
{
#ifdef _WIN32
	// GL 1.1 functions are exported by opengl32.dll itself
	HMODULE gl = GetModuleHandleA( "opengl32.dll" );
	return gl ? (void *)GetProcAddress( gl, name ) : NULL;
#else
	return SDL_GL_GetProcAddress( name );
#endif
}

template <typename T> bool Resolve( T &fn, const char *name )
{
	fn = (T)GetProc( name );
	return fn != NULL;
}

bool Ready( void )
{
	void *context = CurrentContext();
	if ( !context )
		return false;

	if ( context != s_pContext )
	{
		s_pContext = context;
		s_iTexture = 0;		// the old context's texture is gone with it
		s_iTexWide = s_iTexTall = s_iPicWide = s_iPicTall = 0;
		s_bAvailable =
			Resolve( qgl.GenTextures, "glGenTextures" ) &&
			Resolve( qgl.DeleteTextures, "glDeleteTextures" ) &&
			Resolve( qgl.BindTexture, "glBindTexture" ) &&
			Resolve( qgl.IsTexture, "glIsTexture" ) &&
			Resolve( qgl.TexImage2D, "glTexImage2D" ) &&
			Resolve( qgl.TexSubImage2D, "glTexSubImage2D" ) &&
			Resolve( qgl.TexParameteri, "glTexParameteri" ) &&
			Resolve( qgl.TexEnvi, "glTexEnvi" ) &&
			Resolve( qgl.PixelStorei, "glPixelStorei" ) &&
			Resolve( qgl.GetIntegerv, "glGetIntegerv" ) &&
			Resolve( qgl.Enable, "glEnable" ) &&
			Resolve( qgl.Disable, "glDisable" ) &&
			Resolve( qgl.PushAttrib, "glPushAttrib" ) &&
			Resolve( qgl.PopAttrib, "glPopAttrib" ) &&
			Resolve( qgl.MatrixMode, "glMatrixMode" ) &&
			Resolve( qgl.PushMatrix, "glPushMatrix" ) &&
			Resolve( qgl.PopMatrix, "glPopMatrix" ) &&
			Resolve( qgl.LoadIdentity, "glLoadIdentity" ) &&
			Resolve( qgl.Ortho, "glOrtho" ) &&
			Resolve( qgl.Color4f, "glColor4f" ) &&
			Resolve( qgl.Begin, "glBegin" ) &&
			Resolve( qgl.End, "glEnd" ) &&
			Resolve( qgl.TexCoord2f, "glTexCoord2f" ) &&
			Resolve( qgl.Vertex2f, "glVertex2f" );
	}
	return s_bAvailable;
}

int PowerOfTwo( int v )
{
	int p = 1;
	while ( p < v )
		p <<= 1;
	return p;
}

// with the texture bound
bool Upload( const unsigned char *rgba, int width, int height )
{
	if ( !s_iTexture || !qgl.IsTexture( s_iTexture ) || width > s_iTexWide || height > s_iTexTall )
	{
		if ( s_iTexture && qgl.IsTexture( s_iTexture ) )
			qgl.DeleteTextures( 1, &s_iTexture );
		s_iTexture = 0;
		qgl.GenTextures( 1, &s_iTexture );
		if ( !s_iTexture )
			return false;
		s_iTexWide = PowerOfTwo( width );
		s_iTexTall = PowerOfTwo( height );
		qgl.BindTexture( VGL_TEXTURE_2D, s_iTexture );
		qgl.TexImage2D( VGL_TEXTURE_2D, 0, VGL_RGBA, s_iTexWide, s_iTexTall, 0, VGL_RGBA, VGL_UNSIGNED_BYTE, NULL );
		// no mipmaps: the default minification filter would leave the texture incomplete (drawn untextured)
		qgl.TexParameteri( VGL_TEXTURE_2D, VGL_TEXTURE_MIN_FILTER, VGL_LINEAR );
		qgl.TexParameteri( VGL_TEXTURE_2D, VGL_TEXTURE_MAG_FILTER, VGL_LINEAR );
		qgl.TexParameteri( VGL_TEXTURE_2D, VGL_TEXTURE_WRAP_S, VGL_CLAMP );
		qgl.TexParameteri( VGL_TEXTURE_2D, VGL_TEXTURE_WRAP_T, VGL_CLAMP );
	}
	else
		qgl.BindTexture( VGL_TEXTURE_2D, s_iTexture );

	VGLint align = 4, rowLength = 0;
	qgl.GetIntegerv( VGL_UNPACK_ALIGNMENT, &align );
	qgl.GetIntegerv( VGL_UNPACK_ROW_LENGTH, &rowLength );
	qgl.PixelStorei( VGL_UNPACK_ALIGNMENT, 4 );
	qgl.PixelStorei( VGL_UNPACK_ROW_LENGTH, 0 );
	qgl.TexSubImage2D( VGL_TEXTURE_2D, 0, 0, 0, width, height, VGL_RGBA, VGL_UNSIGNED_BYTE, rgba );
	qgl.PixelStorei( VGL_UNPACK_ALIGNMENT, align );
	qgl.PixelStorei( VGL_UNPACK_ROW_LENGTH, rowLength );

	s_iPicWide = width;
	s_iPicTall = height;
	return true;
}
}

bool VideoGL_Draw( const unsigned char *rgba, int width, int height, bool changed )
{
	if ( !rgba || width <= 0 || height <= 0 || !Ready() )
		return false;

	VGLint viewport[4] = { 0, 0, 0, 0 };
	qgl.GetIntegerv( VGL_VIEWPORT, viewport );
	const float screenWide = (float)viewport[2], screenTall = (float)viewport[3];
	if ( screenWide <= 0 || screenTall <= 0 )
		return false;

	// the engine keeps track of the bound texture: put its binding back afterwards
	VGLint previous = 0;
	qgl.GetIntegerv( VGL_TEXTURE_BINDING_2D, &previous );

	qgl.PushAttrib( VGL_ENABLE_BIT | VGL_COLOR_BUFFER_BIT | VGL_TEXTURE_BIT | VGL_CURRENT_BIT | VGL_DEPTH_BUFFER_BIT
		| VGL_TRANSFORM_BIT );

	bool ok = true;
	if ( changed || !s_iTexture || width != s_iPicWide || height != s_iPicTall || !qgl.IsTexture( s_iTexture ) )
		ok = Upload( rgba, width, height );

	qgl.MatrixMode( VGL_PROJECTION );
	qgl.PushMatrix();
	qgl.LoadIdentity();
	qgl.Ortho( 0, screenWide, screenTall, 0, -1, 1 );
	qgl.MatrixMode( VGL_MODELVIEW );
	qgl.PushMatrix();
	qgl.LoadIdentity();

	qgl.Disable( VGL_DEPTH_TEST );
	qgl.Disable( VGL_CULL_FACE );
	qgl.Disable( VGL_ALPHA_TEST );
	qgl.Disable( VGL_BLEND );
	qgl.Disable( VGL_FOG );
	qgl.Disable( VGL_SCISSOR_TEST );

	// black over the whole screen (the bars)
	qgl.Disable( VGL_TEXTURE_2D );
	qgl.Color4f( 0, 0, 0, 1 );
	qgl.Begin( VGL_QUADS );
	qgl.Vertex2f( 0, 0 );
	qgl.Vertex2f( screenWide, 0 );
	qgl.Vertex2f( screenWide, screenTall );
	qgl.Vertex2f( 0, screenTall );
	qgl.End();

	if ( ok )
	{
		// as large as fits, centered, proportions kept
		float scale = screenWide / width;
		if ( height * scale > screenTall )
			scale = screenTall / height;
		const float w = width * scale, h = height * scale;
		const float x = ( screenWide - w ) * 0.5f, y = ( screenTall - h ) * 0.5f;
		const float s = (float)width / s_iTexWide, t = (float)height / s_iTexTall;

		qgl.Enable( VGL_TEXTURE_2D );
		qgl.BindTexture( VGL_TEXTURE_2D, s_iTexture );
		qgl.TexEnvi( VGL_TEXTURE_ENV, VGL_TEXTURE_ENV_MODE, VGL_REPLACE );
		qgl.Color4f( 1, 1, 1, 1 );
		qgl.Begin( VGL_QUADS );
		qgl.TexCoord2f( 0, 0 ); qgl.Vertex2f( x, y );
		qgl.TexCoord2f( s, 0 ); qgl.Vertex2f( x + w, y );
		qgl.TexCoord2f( s, t ); qgl.Vertex2f( x + w, y + h );
		qgl.TexCoord2f( 0, t ); qgl.Vertex2f( x, y + h );
		qgl.End();
	}

	qgl.MatrixMode( VGL_MODELVIEW );
	qgl.PopMatrix();
	qgl.MatrixMode( VGL_PROJECTION );
	qgl.PopMatrix();

	qgl.PopAttrib();	// the matrix mode too (GL_TRANSFORM_BIT)
	qgl.BindTexture( VGL_TEXTURE_2D, (VGLuint)previous );
	return ok;
}

void VideoGL_Release( void )
{
	if ( s_iTexture && s_pContext && CurrentContext() == s_pContext && s_bAvailable && qgl.IsTexture( s_iTexture ) )
		qgl.DeleteTextures( 1, &s_iTexture );
	s_iTexture = 0;
	s_iTexWide = s_iTexTall = s_iPicWide = s_iPicTall = 0;
}
