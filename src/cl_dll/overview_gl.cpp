//=========================================================
// SBO 2.0: the spectator's map overview keeps its own field of view (CHudSpectator::DrawOverview). The engine draws
// a whole frame, the overview and a picture-in-picture inset, with one field of view: the one the client hands it in
// HUD_UpdateClientData (CHud::m_iRenderFOV). When a first-person view of a zoomed player is in the frame, that is the
// player's zoom; around the overview the projection's x and y rows are divided by the zoom's scale, so the overview
// is drawn as at the spectator's own field of view (same near and far planes: moving the camera back instead put the
// map behind the far plane).
//
// The GL functions are looked up in the current context, as env_mirror does (mirror_gl.cpp):
//   Windows: GL 1.1 functions from opengl32.dll (the engine has it loaded; GetModuleHandle, not linked)
//   Linux:   SDL_GL_GetCurrentContext / SDL_GL_GetProcAddress (SDL2 is linked already)
// No context (software renderer) or a missing function: nothing changes.
//=========================================================

#include "overview_gl.h"

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
typedef float			OGLfloat;

#define OGL_MATRIX_MODE			0x0BA0
#define OGL_PROJECTION			0x1701
#define OGL_PROJECTION_MATRIX	0x0BA7

static struct
{
	void	( OGL_APIENTRY *GetIntegerv )( OGLenum pname, OGLint *params );
	void	( OGL_APIENTRY *GetFloatv )( OGLenum pname, OGLfloat *params );
	void	( OGL_APIENTRY *MatrixMode )( OGLenum mode );
	void	( OGL_APIENTRY *PushMatrix )( void );
	void	( OGL_APIENTRY *PopMatrix )( void );
	void	( OGL_APIENTRY *LoadMatrixf )( const OGLfloat *m );
} qgl;

static void		*s_pContext = NULL;		// the context the pointers were looked up in
static bool		s_bAvailable = false;
static bool		s_bActive = false;		// between OverviewGL_Begin and OverviewGL_End

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
	// GL 1.1 functions only: opengl32.dll exports them itself
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

static bool Available( void )
{
	void *context = CurrentContext();
	if ( !context )
		return false;

	if ( context != s_pContext )
	{
		s_pContext = context;
		s_bAvailable =
			Resolve( qgl.GetIntegerv, "glGetIntegerv" ) &&
			Resolve( qgl.GetFloatv, "glGetFloatv" ) &&
			Resolve( qgl.MatrixMode, "glMatrixMode" ) &&
			Resolve( qgl.PushMatrix, "glPushMatrix" ) &&
			Resolve( qgl.PopMatrix, "glPopMatrix" ) &&
			Resolve( qgl.LoadMatrixf, "glLoadMatrixf" );
	}

	return s_bAvailable;
}

bool OverviewGL_Begin( float scale )
{
	if ( s_bActive || !( scale > 0.0f ) || !Available() )
		return false;

	OGLint mode = 0;
	qgl.GetIntegerv( OGL_MATRIX_MODE, &mode );

	OGLfloat m[16];
	qgl.GetFloatv( OGL_PROJECTION_MATRIX, m );

	// column-major: rows 0 and 1 (clip x and y) are m[0,4,8,12] and m[1,5,9,13]; w stays, so x and y on the
	// screen (NDC) are divided by scale about the view's center
	for ( int i = 0; i < 16; i += 4 )
	{
		m[i] /= scale;
		m[i + 1] /= scale;
	}

	qgl.MatrixMode( OGL_PROJECTION );
	qgl.PushMatrix();
	qgl.LoadMatrixf( m );
	qgl.MatrixMode( (OGLenum)mode );

	s_bActive = true;
	return true;
}

void OverviewGL_End( void )
{
	if ( !s_bActive )
		return;
	s_bActive = false;

	OGLint mode = 0;
	qgl.GetIntegerv( OGL_MATRIX_MODE, &mode );
	qgl.MatrixMode( OGL_PROJECTION );
	qgl.PopMatrix();
	qgl.MatrixMode( (OGLenum)mode );
}
