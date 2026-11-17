//=========================================================
// SBO 2.1 env_sky: the map's 2D skybox on the env_sky faces while the 3D sky is off. docs/ENV_SKY.md.
//
// The compilers turn env_sky brushes into sky without faces, so the view has nothing there of its own: while the 3D
// sky is on, the sky room drawn before the view (view.cpp V_SkyViewStart) shows through. With it off (triggered Off,
// removed, every env_sky "Start off") nothing was drawn there and the last frame stayed on screen (hall of mirrors).
// The engine draws its skybox only around the polygons of `sky` faces, so the client draws the same skybox here, the
// way the engine does (Quake 2 / Half-Life GL R_DrawSkyBox: the six gfx/env/<skyname><rt|bk|lf|ft|up|dn>.tga images
// on a cube centered on the eye, with the engine's face order and orientation), on every pixel of the view that is
// still empty: after the world and the solid entities (HUD_DrawNormalTriangles), at the far end of the current depth
// range with the engine's depth test and no depth write, so it fills only what nothing drew this frame (also with the
// engine's alternating depth ranges). No fog, as on the engine's skybox. Maps without env_sky never get here.
//
// The GL functions are looked up in the current context as cl_dll/worldtext_gl.cpp does (no GL header or import
// library); the textures are made with overlay_gl.cpp.
//
// SBO 2.1 sky fog (EnvSkyBox_DrawSkyFog): while the env_sky that is on has a sky fog, the 2D skybox behind its sky
// room is fogged too, as if it stood ENVSKY_SKYBOX_FOG_DIST sky-room units away: a cube around the sky camera in the
// fog color, blended with the exp2 fog of that distance (env_sky "fogskybox" 2 Full; 1 Partial: ENVSKY_SKYBOX_FOG_PARTIAL
// of it, so the skybox picture still shows through a tint; 0 None: no skybox fog) over what lies beyond the cube (the skybox, which the engine
// draws with depth far out; the far end of the depth range did not work: the skybox writes depth there too). The cube
// is a quarter of the far plane in half size (at most 2048), so it stays inside the engine's skybox. Without it, at a medium or high density, the far scenery turns into solid fog-colored
// shapes against a clear sky. The map's own fog (env_fog, env_fogzone) still leaves the skybox clear.
//=========================================================

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "r_studioint.h"
#include "pm_defs.h"
#include "pm_movevars.h"
#include "env_sky_box.h"
#include "overlay_gl.h"
#include "worldtext_gl.h"
#include "vgui2/sbo_image_decode.h"

#ifdef _WIN32
#include <windows.h>
#define SGL_APIENTRY APIENTRY
#else
#include <SDL2/SDL_video.h>
#define SGL_APIENTRY
#endif

extern engine_studio_api_t IEngineStudio;
extern "C" playermove_t *pmove;	// pm_shared.c: its movevars carry the map's sky name

typedef unsigned int	SGLenum;
typedef unsigned char	SGLboolean;
typedef int				SGLint;
typedef float			SGLfloat;
typedef double			SGLdouble;

#define SGL_TEXTURE_2D			0x0DE1
#define SGL_TEXTURE_BINDING_2D	0x8069
#define SGL_TEXTURE_ENV			0x2300
#define SGL_TEXTURE_ENV_MODE	0x2200
#define SGL_REPLACE				0x1E01
#define SGL_DEPTH_TEST			0x0B71
#define SGL_DEPTH_RANGE			0x0B70
#define SGL_DEPTH_WRITEMASK		0x0B72
#define SGL_FOG					0x0B60
#define SGL_BLEND				0x0BE2
#define SGL_ALPHA_TEST			0x0BC0
#define SGL_CULL_FACE			0x0B44
#define SGL_QUADS				0x0007
#define SGL_FOG_COLOR			0x0B66
#define SGL_BLEND_SRC			0x0BE1
#define SGL_BLEND_DST			0x0BE0
#define SGL_SRC_ALPHA			0x0302
#define SGL_ONE_MINUS_SRC_ALPHA	0x0303
#define SGL_CURRENT_COLOR		0x0B00

static struct
{
	void		( SGL_APIENTRY *Enable )( SGLenum cap );
	void		( SGL_APIENTRY *Disable )( SGLenum cap );
	SGLboolean	( SGL_APIENTRY *IsEnabled )( SGLenum cap );
	void		( SGL_APIENTRY *GetIntegerv )( SGLenum pname, SGLint *params );
	void		( SGL_APIENTRY *GetFloatv )( SGLenum pname, SGLfloat *params );
	void		( SGL_APIENTRY *GetBooleanv )( SGLenum pname, SGLboolean *params );
	void		( SGL_APIENTRY *DepthMask )( SGLboolean flag );
	void		( SGL_APIENTRY *DepthRange )( SGLdouble zNear, SGLdouble zFar );
	void		( SGL_APIENTRY *BindTexture )( SGLenum target, unsigned int texture );
	void		( SGL_APIENTRY *GetTexEnviv )( SGLenum target, SGLenum pname, SGLint *params );
	void		( SGL_APIENTRY *TexEnvi )( SGLenum target, SGLenum pname, SGLint param );
	void		( SGL_APIENTRY *Begin )( SGLenum mode );
	void		( SGL_APIENTRY *End )( void );
	void		( SGL_APIENTRY *TexCoord2f )( SGLfloat s, SGLfloat t );
	void		( SGL_APIENTRY *Vertex3f )( SGLfloat x, SGLfloat y, SGLfloat z );
	void		( SGL_APIENTRY *Color4f )( SGLfloat r, SGLfloat g, SGLfloat b, SGLfloat a );
	void		( SGL_APIENTRY *BlendFunc )( SGLenum sfactor, SGLenum dfactor );
} qgl;

static void	*s_pContext = NULL;		// the context the pointers and textures belong to
static bool	s_bAvailable = false;

static void *GetProc( const char *name )
{
#ifdef _WIN32
	HMODULE gl = GetModuleHandleA( "opengl32.dll" );
	// all GL 1.1: exported by opengl32.dll itself
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

// The skybox: the engine's suffixes in the order it loads them, and which image each cube side shows
static const char *const s_szSuffix[ 6 ] = { "rt", "bk", "lf", "ft", "up", "dn" };
static const int s_iSkyTexOrder[ 6 ] = { 0, 2, 1, 3, 4, 5 };
// a cube side's s, t (-1 .. 1) and the distance to world x, y, z (1 s, 2 t, 3 the distance; minus: negated)
static const int s_iStToVec[ 6 ][ 3 ] =
{
	{ 3, -1, 2 },
	{ -3, 1, 2 },
	{ 1, 3, 2 },
	{ -1, -3, 2 },
	{ -2, -1, 3 },	// up
	{ 2, -1, -3 }	// down
};

#define SKYBOX_MAX_SIDE		1024	// a larger image is scaled down to fit
#define SKYBOX_DIST			64.0f	// the cube's half size: well inside the far plane, outside the near plane

static char		s_szSkyName[ 32 ];		// the name the textures are of ("" none loaded)
static unsigned	s_iTexture[ 6 ];		// by suffix; 0: that image is missing
static bool		s_bLoaded = false;

static bool Available( void )
{
	void *context = OverlayGL_Context();
	if ( !context )
		return false;

	if ( context != s_pContext )
	{
		// a new context (video restart): the textures went with the old one
		s_pContext = context;
		s_bLoaded = false;
		s_szSkyName[ 0 ] = '\0';
		memset( s_iTexture, 0, sizeof( s_iTexture ) );
		s_bAvailable =
			Resolve( qgl.Enable, "glEnable" ) &&
			Resolve( qgl.Disable, "glDisable" ) &&
			Resolve( qgl.IsEnabled, "glIsEnabled" ) &&
			Resolve( qgl.GetIntegerv, "glGetIntegerv" ) &&
			Resolve( qgl.GetFloatv, "glGetFloatv" ) &&
			Resolve( qgl.GetBooleanv, "glGetBooleanv" ) &&
			Resolve( qgl.DepthMask, "glDepthMask" ) &&
			Resolve( qgl.DepthRange, "glDepthRange" ) &&
			Resolve( qgl.BindTexture, "glBindTexture" ) &&
			Resolve( qgl.GetTexEnviv, "glGetTexEnviv" ) &&
			Resolve( qgl.TexEnvi, "glTexEnvi" ) &&
			Resolve( qgl.Begin, "glBegin" ) &&
			Resolve( qgl.End, "glEnd" ) &&
			Resolve( qgl.TexCoord2f, "glTexCoord2f" ) &&
			Resolve( qgl.Vertex3f, "glVertex3f" ) &&
			Resolve( qgl.Color4f, "glColor4f" ) &&
			Resolve( qgl.BlendFunc, "glBlendFunc" );
	}

	return s_bAvailable;
}

// the six images of the sky the map names (the engine's default "desert" without a name), once per name
static bool LoadSky( const char *name )
{
	if ( s_bLoaded && !strcmp( name, s_szSkyName ) )
		return true;

	for ( int i = 0; i < 6; i++ )
	{
		OverlayGL_DeleteTexture( s_iTexture[ i ] );
		s_iTexture[ i ] = 0;
	}
	strncpy( s_szSkyName, name, sizeof( s_szSkyName ) - 1 );
	s_szSkyName[ sizeof( s_szSkyName ) - 1 ] = '\0';
	s_bLoaded = true;

	int iLoaded = 0;
	for ( int i = 0; i < 6; i++ )
	{
		char path[ 96 ];
		snprintf( path, sizeof( path ), "gfx/env/%s%s.tga", s_szSkyName, s_szSuffix[ i ] );

		int len = 0;
		byte *data = gEngfuncs.COM_LoadFile( path, 5, &len );
		if ( !data )
		{
			gEngfuncs.Con_DPrintf( "env_sky: can't open \"%s\"\n", path );
			continue;
		}

		std::vector<uint8_t> rgba;
		int wide = 0, tall = 0;
		const bool ok = len > 0 && sbo_image::DecodeTgaScaled( data, (size_t)len, SKYBOX_MAX_SIDE, SKYBOX_MAX_SIDE,
			rgba, wide, tall );
		gEngfuncs.COM_FreeFile( data );

		if ( ok )
			s_iTexture[ i ] = OverlayGL_CreateTexture( rgba.data(), wide, tall );
		if ( s_iTexture[ i ] )
			iLoaded++;
		else
			gEngfuncs.Con_DPrintf( "env_sky: \"%s\" is not a 24 or 32 bit .tga\n", path );
	}

	return iLoaded > 0;
}

// a corner of a cube side, as the engine's MakeSkyVec makes it
static void SkyVertex( float s, float t, int axis, const float *origin )
{
	const float b[ 3 ] = { s * SKYBOX_DIST, t * SKYBOX_DIST, SKYBOX_DIST };
	float v[ 3 ];

	for ( int j = 0; j < 3; j++ )
	{
		const int k = s_iStToVec[ axis ][ j ];
		v[ j ] = ( k < 0 ? -b[ -k - 1 ] : b[ k - 1 ] ) + origin[ j ];
	}

	// texture coordinates: the image's top row at t -1 (the engine's 1 - t after the 0..1 range)
	qgl.TexCoord2f( ( s + 1.0f ) * 0.5f, 1.0f - ( t + 1.0f ) * 0.5f );
	qgl.Vertex3f( v[ 0 ], v[ 1 ], v[ 2 ] );
}

void EnvSkyBox_Draw( void )
{
	// only on a map with an env_sky (the server sends its state with every HUD initialisation) while it is off, in a
	// view of the world (not the sky room, not the spectator's overview map)
	if ( !gHUD.m_bEnvSkyMap || gHUD.m_iSkyMode == SKY_ON || gHUD.m_bSkyDrawPass
		|| gHUD.m_Spectator.m_bOverviewDrawPass || !HUD_IsHardware() || !pmove || !pmove->movevars )
		return;

	if ( !Available() )
		return;

	const char *name = pmove->movevars->skyName[ 0 ] ? pmove->movevars->skyName : "desert";
	if ( !LoadSky( name ) )
		return;

	float origin[ 3 ], up[ 3 ], right[ 3 ], forward[ 3 ];
	IEngineStudio.GetViewInfo( origin, up, right, forward );

	// the engine's state, put back below
	SGLfloat depthRange[ 2 ] = { 0.0f, 1.0f };
	SGLboolean depthMask = 1;
	SGLint texture = 0, texEnv = 0;
	qgl.GetFloatv( SGL_DEPTH_RANGE, depthRange );
	qgl.GetBooleanv( SGL_DEPTH_WRITEMASK, &depthMask );
	const SGLboolean depthTest = qgl.IsEnabled( SGL_DEPTH_TEST );
	const SGLboolean fog = qgl.IsEnabled( SGL_FOG );
	const SGLboolean blend = qgl.IsEnabled( SGL_BLEND );
	const SGLboolean alphaTest = qgl.IsEnabled( SGL_ALPHA_TEST );
	const SGLboolean cull = qgl.IsEnabled( SGL_CULL_FACE );

	WorldTextGL_Begin();	// texture unit 0 active, unit 1 off
	const SGLboolean texture2D = qgl.IsEnabled( SGL_TEXTURE_2D );
	qgl.GetIntegerv( SGL_TEXTURE_BINDING_2D, &texture );
	qgl.GetTexEnviv( SGL_TEXTURE_ENV, SGL_TEXTURE_ENV_MODE, &texEnv );

	// on the far end of the depth range: drawn only where the depth is still the cleared (or last frame's) value
	qgl.DepthRange( depthRange[ 1 ], depthRange[ 1 ] );
	qgl.DepthMask( 0 );
	qgl.Enable( SGL_DEPTH_TEST );
	qgl.Disable( SGL_FOG );
	qgl.Disable( SGL_BLEND );
	qgl.Disable( SGL_ALPHA_TEST );
	qgl.Disable( SGL_CULL_FACE );
	qgl.Enable( SGL_TEXTURE_2D );
	qgl.TexEnvi( SGL_TEXTURE_ENV, SGL_TEXTURE_ENV_MODE, SGL_REPLACE );

	for ( int i = 0; i < 6; i++ )
	{
		const unsigned tex = s_iTexture[ s_iSkyTexOrder[ i ] ];
		if ( !tex )
			continue;

		qgl.BindTexture( SGL_TEXTURE_2D, tex );
		qgl.Begin( SGL_QUADS );
		SkyVertex( -1.0f, -1.0f, i, origin );
		SkyVertex( -1.0f, 1.0f, i, origin );
		SkyVertex( 1.0f, 1.0f, i, origin );
		SkyVertex( 1.0f, -1.0f, i, origin );
		qgl.End();
	}

	qgl.TexEnvi( SGL_TEXTURE_ENV, SGL_TEXTURE_ENV_MODE, texEnv );
	qgl.BindTexture( SGL_TEXTURE_2D, (unsigned int)texture );
	if ( !texture2D )
		qgl.Disable( SGL_TEXTURE_2D );
	WorldTextGL_End();

	if ( cull )
		qgl.Enable( SGL_CULL_FACE );
	if ( alphaTest )
		qgl.Enable( SGL_ALPHA_TEST );
	if ( blend )
		qgl.Enable( SGL_BLEND );
	if ( fog )
		qgl.Enable( SGL_FOG );
	if ( !depthTest )
		qgl.Disable( SGL_DEPTH_TEST );
	qgl.DepthMask( depthMask );
	qgl.DepthRange( depthRange[ 0 ], depthRange[ 1 ] );
}

// SBO 2.1 sky fog: the 2D skybox of the sky pass fogged as if it stood this far from the sky camera (sky-room units)
#define ENVSKY_SKYBOX_FOG_DIST		2048.0f
#define ENVSKY_SKYBOX_FOG_PARTIAL	0.45f	// Skybox fog "Partial": this part of the Full amount

void EnvSkyBox_DrawSkyFog( void )
{
	// only in the sky pass, while the env_sky that is on has a sky fog (tri.cpp RenderSkyFog has set it)
	if ( !gHUD.m_bSkyDrawPass || gHUD.m_iSkyMode != SKY_ON || gHUD.m_SkyFog.density <= 0.0f || gHUD.m_iSkyboxFog <= 0
		|| !HUD_IsHardware() )
		return;

	if ( !Available() )
		return;

	// the engine's exp2 fog at that distance
	const float d = gHUD.m_SkyFog.density * ENVSKY_SKYBOX_FOG_DIST;
	const float flAlpha = ( 1.0f - expf( -d * d ) ) * ( gHUD.m_iSkyboxFog >= 2 ? 1.0f : ENVSKY_SKYBOX_FOG_PARTIAL );
	if ( flAlpha <= 0.0f )
		return;

	// the fog color as the engine set it (the same color the sky room's fog has)
	SGLfloat fogColor[ 4 ] = { gHUD.m_SkyFog.color[ 0 ] / 255.0f, gHUD.m_SkyFog.color[ 1 ] / 255.0f,
		gHUD.m_SkyFog.color[ 2 ] / 255.0f, 1.0f };
	const SGLboolean fog = qgl.IsEnabled( SGL_FOG );
	if ( fog )
		qgl.GetFloatv( SGL_FOG_COLOR, fogColor );

	float origin[ 3 ], up[ 3 ], right[ 3 ], forward[ 3 ];
	IEngineStudio.GetViewInfo( origin, up, right, forward );

	// the engine's state, put back below
	SGLfloat depthRange[ 2 ] = { 0.0f, 1.0f };
	SGLfloat color[ 4 ] = { 1.0f, 1.0f, 1.0f, 1.0f };
	SGLboolean depthMask = 1;
	SGLint blendSrc = 0, blendDst = 0;
	qgl.GetFloatv( SGL_DEPTH_RANGE, depthRange );
	qgl.GetFloatv( SGL_CURRENT_COLOR, color );
	qgl.GetBooleanv( SGL_DEPTH_WRITEMASK, &depthMask );
	qgl.GetIntegerv( SGL_BLEND_SRC, &blendSrc );
	qgl.GetIntegerv( SGL_BLEND_DST, &blendDst );
	const SGLboolean depthTest = qgl.IsEnabled( SGL_DEPTH_TEST );
	const SGLboolean blend = qgl.IsEnabled( SGL_BLEND );
	const SGLboolean alphaTest = qgl.IsEnabled( SGL_ALPHA_TEST );
	const SGLboolean cull = qgl.IsEnabled( SGL_CULL_FACE );

	WorldTextGL_Begin();	// texture unit 0 active, unit 1 off
	const SGLboolean texture2D = qgl.IsEnabled( SGL_TEXTURE_2D );

	// a real cube with the engine's depth test: drawn only where what the sky pass drew lies beyond it (its skybox)
	float flHalf = 2048.0f;
	if ( pmove && pmove->movevars && pmove->movevars->zmax > 0.0f )
		flHalf = fminf( flHalf, fmaxf( 128.0f, pmove->movevars->zmax * 0.25f ) );
	qgl.DepthMask( 0 );
	qgl.Enable( SGL_DEPTH_TEST );
	qgl.Disable( SGL_FOG );
	qgl.Disable( SGL_ALPHA_TEST );
	qgl.Disable( SGL_CULL_FACE );
	qgl.Disable( SGL_TEXTURE_2D );
	qgl.Enable( SGL_BLEND );
	qgl.BlendFunc( SGL_SRC_ALPHA, SGL_ONE_MINUS_SRC_ALPHA );
	qgl.Color4f( fogColor[ 0 ], fogColor[ 1 ], fogColor[ 2 ], flAlpha );

	for ( int i = 0; i < 6; i++ )
	{
		static const float st[ 4 ][ 2 ] = { { -1, -1 }, { -1, 1 }, { 1, 1 }, { 1, -1 } };
		qgl.Begin( SGL_QUADS );
		for ( int c = 0; c < 4; c++ )
		{
			const float b[ 3 ] = { st[ c ][ 0 ] * flHalf, st[ c ][ 1 ] * flHalf, flHalf };
			float v[ 3 ];
			for ( int j = 0; j < 3; j++ )
			{
				const int k = s_iStToVec[ i ][ j ];
				v[ j ] = ( k < 0 ? -b[ -k - 1 ] : b[ k - 1 ] ) + origin[ j ];
			}
			qgl.Vertex3f( v[ 0 ], v[ 1 ], v[ 2 ] );
		}
		qgl.End();
	}

	qgl.Color4f( color[ 0 ], color[ 1 ], color[ 2 ], color[ 3 ] );
	qgl.BlendFunc( (SGLenum)blendSrc, (SGLenum)blendDst );
	if ( texture2D )
		qgl.Enable( SGL_TEXTURE_2D );
	WorldTextGL_End();

	if ( cull )
		qgl.Enable( SGL_CULL_FACE );
	if ( alphaTest )
		qgl.Enable( SGL_ALPHA_TEST );
	if ( !blend )
		qgl.Disable( SGL_BLEND );
	if ( fog )
		qgl.Enable( SGL_FOG );
	if ( !depthTest )
		qgl.Disable( SGL_DEPTH_TEST );
	qgl.DepthMask( depthMask );
	qgl.DepthRange( depthRange[ 0 ], depthRange[ 1 ] );
}
