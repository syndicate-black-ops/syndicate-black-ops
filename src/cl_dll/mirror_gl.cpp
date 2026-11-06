//=========================================================
// SBO 2.0: env_mirror: an OpenGL user clip plane at the mirror's plane around the reflected copies
// (StudioModelRenderer.cpp StudioDrawMirrorCopies), so that a reflection never comes out in front of the mirror
// (a player close to it: his reflected arm and gun reached through into the room), and the front-face winding
// turned around for them (a reflection reverses the triangles' winding). docs/ENV_MIRROR.md.
//
// The GL functions are looked up in the current context, as the world screens do (branch feat/world-screens,
// worldscreen_gl.cpp), so no GL header or import library is needed:
//   Windows: wglGetCurrentContext / wglGetProcAddress, the GL 1.1 functions from opengl32.dll (the engine has it
//            loaded; looked up with GetModuleHandle, not linked)
//   Linux:   SDL_GL_GetCurrentContext / SDL_GL_GetProcAddress (SDL2 is linked already, inputw32.cpp)
// No context (software renderer: no reflections anyway) or a missing function: no clipping.
//=========================================================

#include "mirror_gl.h"

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#define MGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define MGL_APIENTRY
#endif

typedef unsigned int	MGLenum;
typedef unsigned char	MGLboolean;
typedef int				MGLint;
typedef double			MGLdouble;

#define MGL_CLIP_PLANE0		0x3000
#define MGL_MATRIX_MODE		0x0BA0
#define MGL_MODELVIEW		0x1700
#define MGL_FRONT_FACE		0x0B46
#define MGL_CW				0x0900
#define MGL_CCW				0x0901

static struct
{
	void		( MGL_APIENTRY *ClipPlane )( MGLenum plane, const MGLdouble *equation );
	void		( MGL_APIENTRY *GetClipPlane )( MGLenum plane, MGLdouble *equation );
	void		( MGL_APIENTRY *Enable )( MGLenum cap );
	void		( MGL_APIENTRY *Disable )( MGLenum cap );
	MGLboolean	( MGL_APIENTRY *IsEnabled )( MGLenum cap );
	void		( MGL_APIENTRY *GetIntegerv )( MGLenum pname, MGLint *params );
	void		( MGL_APIENTRY *MatrixMode )( MGLenum mode );
	void		( MGL_APIENTRY *PushMatrix )( void );
	void		( MGL_APIENTRY *PopMatrix )( void );
	void		( MGL_APIENTRY *LoadIdentity )( void );
	void		( MGL_APIENTRY *FrontFace )( MGLenum mode );
} qgl;

static void		*s_pContext = NULL;		// the context the pointers were looked up in
static bool		s_bAvailable = false;

// MirrorGL_Begin / End
static bool		s_bActive = false;
static MGLdouble	s_SavedPlane[4];

// MirrorGL_BeginWinding / EndWinding
static bool		s_bWinding = false;
static MGLint	s_SavedFrontFace = MGL_CCW;

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
			Resolve( qgl.ClipPlane, "glClipPlane" ) &&
			Resolve( qgl.GetClipPlane, "glGetClipPlane" ) &&
			Resolve( qgl.Enable, "glEnable" ) &&
			Resolve( qgl.Disable, "glDisable" ) &&
			Resolve( qgl.IsEnabled, "glIsEnabled" ) &&
			Resolve( qgl.GetIntegerv, "glGetIntegerv" ) &&
			Resolve( qgl.MatrixMode, "glMatrixMode" ) &&
			Resolve( qgl.PushMatrix, "glPushMatrix" ) &&
			Resolve( qgl.PopMatrix, "glPopMatrix" ) &&
			Resolve( qgl.LoadIdentity, "glLoadIdentity" ) &&
			Resolve( qgl.FrontFace, "glFrontFace" );
	}

	return s_bAvailable;
}

bool MirrorGL_Begin( void )
{
	s_bActive = false;

	if ( !Available() )
		return false;

	// a clip plane the engine has on: leave it alone (no clipping of the reflections)
	if ( qgl.IsEnabled( MGL_CLIP_PLANE0 ) )
		return false;

	qgl.GetClipPlane( MGL_CLIP_PLANE0, s_SavedPlane );	// eye space, restored in MirrorGL_End
	s_bActive = true;
	return true;
}

void MirrorGL_SetPlane( int axis, float plane, int keepSide, const float *vieworg, const float *forward,
	const float *right, const float *up )
{
	if ( !s_bActive )
		return;

	// Kept: keepSide * ( p[axis] - plane ) >= 0, p in world space. glClipPlane stores the plane in eye space through
	// the modelview current at the call, which is not reliably the view matrix here (the engine's studio drawing
	// has its own), so the plane is put into eye space from the view being drawn and set with an identity
	// modelview. Eye space (GL): x = right, y = up, z = -forward, origin at vieworg. For an eye point e the world
	// point is p = vieworg + e.x right + e.y up - e.z forward, so n . p + d = (n.right, n.up, -n.forward) . e +
	// (n . vieworg + d).
	MGLdouble n[3] = { 0, 0, 0 };
	n[axis] = keepSide;
	MGLdouble d = -keepSide * (MGLdouble)plane;

	MGLdouble eq[4];
	eq[0] = n[0] * right[0] + n[1] * right[1] + n[2] * right[2];
	eq[1] = n[0] * up[0] + n[1] * up[1] + n[2] * up[2];
	eq[2] = -( n[0] * forward[0] + n[1] * forward[1] + n[2] * forward[2] );
	eq[3] = n[0] * vieworg[0] + n[1] * vieworg[1] + n[2] * vieworg[2] + d;

	MGLint mode = MGL_MODELVIEW;
	qgl.GetIntegerv( MGL_MATRIX_MODE, &mode );
	if ( mode != MGL_MODELVIEW )
		qgl.MatrixMode( MGL_MODELVIEW );
	qgl.PushMatrix();
	qgl.LoadIdentity();
	qgl.ClipPlane( MGL_CLIP_PLANE0, eq );
	qgl.PopMatrix();
	if ( mode != MGL_MODELVIEW )
		qgl.MatrixMode( (MGLenum)mode );

	qgl.Enable( MGL_CLIP_PLANE0 );
}

void MirrorGL_End( void )
{
	if ( !s_bActive )
		return;

	s_bActive = false;
	qgl.Disable( MGL_CLIP_PLANE0 );

	// the plane's previous equation back exactly: it was read in eye space, so set it with an identity modelview
	MGLint mode = MGL_MODELVIEW;
	qgl.GetIntegerv( MGL_MATRIX_MODE, &mode );
	if ( mode != MGL_MODELVIEW )
		qgl.MatrixMode( MGL_MODELVIEW );
	qgl.PushMatrix();
	qgl.LoadIdentity();
	qgl.ClipPlane( MGL_CLIP_PLANE0, s_SavedPlane );
	qgl.PopMatrix();
	if ( mode != MGL_MODELVIEW )
		qgl.MatrixMode( (MGLenum)mode );
}

bool MirrorGL_BeginWinding( void )
{
	s_bWinding = false;

	if ( !Available() )
		return false;

	// the engine culls GL_FRONT with the counter-clockwise front face (its visible triangles are clockwise); the
	// reflected triangles are counter-clockwise, so the front face becomes the other one while they are drawn
	s_SavedFrontFace = MGL_CCW;
	qgl.GetIntegerv( MGL_FRONT_FACE, &s_SavedFrontFace );
	qgl.FrontFace( ( s_SavedFrontFace == MGL_CW ) ? MGL_CCW : MGL_CW );
	s_bWinding = true;
	return true;
}

void MirrorGL_EndWinding( void )
{
	if ( !s_bWinding )
		return;

	s_bWinding = false;
	qgl.FrontFace( (MGLenum)s_SavedFrontFace );
}
