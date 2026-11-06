// Aurora editor: the engine behind gEngfuncs for SBO's own particlesys.cpp / particlemgr.cpp / parsemsg.cpp,
// compiled unchanged, plus the C API the editor (JavaScript, through WebAssembly) and the native parity test call.
//
// What the engine gives the game code here (GoldSrc behaviour unless said otherwise):
//  - COM_ParseFile: the engine's tokenizer (whitespace and bytes >= 128 skipped, // comments, "quoted" tokens,
//    { } ( ) ' : single tokens);
//  - RandomFloat: Numerical Recipes ran1, as the engine (seeded, so a run can be repeated);
//  - the client clock, the view (v_origin, HUD_GetEffectAngles), a test scene for PM_PointContents and the point
//    trace (floor, water box, wall box), LightAtPoint (the editor's ambient light and lamp), one followed target;
//  - the triangle API: RenderMode / Color4f keep the engine's state rules (Color4f premultiplies by alpha unless the
//    mode is kRenderTransAlpha; texture and color modes leave the depth mask as it was) and each quad is recorded
//    for the WebGL renderer with that state.
// docs: tools/aurora-editor/README.md

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <algorithm>

#include "aurora_engine.h"
#include "particlesys.h"
#include "particlemgr.h"
#include "aurora_shared.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define AE_API extern "C" EMSCRIPTEN_KEEPALIVE
#else
#define AE_API extern "C"
#endif

//=============================================================================
// math (mathlib)
//=============================================================================

void VectorMA( const float *veca, float scale, const float *vecb, float *vecc )
{
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}

void AngleVectors( const float *angles, float *forward, float *right, float *up )
{
	const double d2r = 3.14159265358979323846 * 2 / 360;
	float angle = (float)( angles[1] * d2r );
	const float sy = (float)sin( angle ), cy = (float)cos( angle );
	angle = (float)( angles[0] * d2r );
	const float sp = (float)sin( angle ), cp = (float)cos( angle );
	angle = (float)( angles[2] * d2r );
	const float sr = (float)sin( angle ), cr = (float)cos( angle );

	if ( forward )
	{
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if ( right )
	{
		right[0] = ( -1 * sr * sp * cy + -1 * cr * -sy );
		right[1] = ( -1 * sr * sp * sy + -1 * cr * cy );
		right[2] = -1 * sr * cp;
	}
	if ( up )
	{
		up[0] = ( cr * sp * cy + -sr * -sy );
		up[1] = ( cr * sp * sy + -sr * cy );
		up[2] = cr * cp;
	}
}

//=============================================================================
// editor state, written by the caller through ae_params()
//=============================================================================

enum
{
	P_TIME = 0,
	P_CAM_X, P_CAM_Y, P_CAM_Z, P_CAM_PITCH, P_CAM_YAW, P_CAM_ROLL,
	P_FLOOR_ON, P_FLOOR_Z,
	P_WATER_ON, P_WATER_MINX, P_WATER_MINY, P_WATER_MINZ, P_WATER_MAXX, P_WATER_MAXY, P_WATER_MAXZ,
	P_WALL_ON, P_WALL_MINX, P_WALL_MINY, P_WALL_MINZ, P_WALL_MAXX, P_WALL_MAXY, P_WALL_MAXZ,
	P_LIGHT_R, P_LIGHT_G, P_LIGHT_B,
	P_LAMP_ON, P_LAMP_X, P_LAMP_Y, P_LAMP_Z, P_LAMP_RADIUS, P_LAMP_R, P_LAMP_G, P_LAMP_B,
	P_TARGET_VISIBLE, P_TARGET_X, P_TARGET_Y, P_TARGET_Z, P_TARGET_PITCH, P_TARGET_YAW, P_TARGET_ROLL,
	P_ATT0_X,	// 4 attachments x 3
	P_PVS = P_ATT0_X + 12,
	P_TARGET_STUDIO,
	P_COUNT
};

static float g_Params[64];

vec3_t v_origin;

void HUD_GetEffectAngles( float *angles )
{
	angles[0] = g_Params[P_CAM_PITCH];
	angles[1] = g_Params[P_CAM_YAW];
	angles[2] = g_Params[P_CAM_ROLL];
}

int HUD_IsHardware( void )
{
	return 1;
}

//=============================================================================
// console
//=============================================================================

static std::string g_Log;

static void Shim_Con_Printf( const char *fmt, ... )
{
	char sz[2048];
	va_list args;
	va_start( args, fmt );
	vsnprintf( sz, sizeof( sz ), fmt, args );
	va_end( args );
	if ( g_Log.size() < 256 * 1024 )
		g_Log += sz;
}

//=============================================================================
// random (Numerical Recipes ran1, as the engine's RandomFloat / RandomLong)
//=============================================================================

#define IA 16807
#define IM 2147483647
#define IQ 127773
#define IR 2836
#define NTAB 32
#define NDIV ( 1 + ( IM - 1 ) / NTAB )
#define AM ( 1.0 / IM )
#define EPS 1.2e-7
#define RNMX ( 1.0 - EPS )

static int s_idum = -1;
static int s_iy = 0;
static int s_iv[NTAB];

static void SeedRandomNumberGenerator( int lSeed )
{
	s_idum = ( lSeed > 0 ) ? -lSeed : lSeed;
	if ( s_idum == 0 )
		s_idum = -1;
	s_iy = 0;
}

static int ran1( void )
{
	int j, k;

	if ( s_idum <= 0 || !s_iy )
	{
		if ( -( s_idum ) < 1 )
			s_idum = 1;
		else
			s_idum = -( s_idum );
		for ( j = NTAB + 7; j >= 0; j-- )
		{
			k = ( s_idum ) / IQ;
			s_idum = IA * ( s_idum - k * IQ ) - IR * k;
			if ( s_idum < 0 )
				s_idum += IM;
			if ( j < NTAB )
				s_iv[j] = s_idum;
		}
		s_iy = s_iv[0];
	}
	k = ( s_idum ) / IQ;
	s_idum = IA * ( s_idum - k * IQ ) - IR * k;
	if ( s_idum < 0 )
		s_idum += IM;
	j = s_iy / NDIV;
	s_iy = s_iv[j];
	s_iv[j] = s_idum;

	return s_iy;
}

static float fran1( void )
{
	float temp = (float)( AM * ran1() );
	if ( temp > RNMX )
		return (float)RNMX;
	return temp;
}

static float Shim_RandomFloat( float flLow, float flHigh )
{
	const float fl = fran1();
	return ( fl * ( flHigh - flLow ) ) + flLow;
}

static int Shim_RandomLong( int lLow, int lHigh )
{
	if ( lHigh <= lLow )
		return lLow;
	const unsigned int x = (unsigned int)( lHigh - lLow + 1 );
	return lLow + (int)( (unsigned int)ran1() % x );
}

//=============================================================================
// COM_ParseFile (the engine's tokenizer)
//=============================================================================

#define AE_MAX_TOKEN 1024

static char *Shim_COM_ParseFile( char *data, char *token )
{
	int len = 0;
	int c;
	token[0] = 0;

	if ( !data )
		return NULL;

skipwhite:
	// signed: bytes >= 128 are below ' ' and skipped as whitespace, as in the engine
	while ( ( c = *(signed char *)data ) <= ' ' )
	{
		if ( c == 0 )
			return NULL;
		data++;
	}

	if ( c == '/' && data[1] == '/' )
	{
		while ( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

	if ( c == '\"' )
	{
		data++;
		while ( 1 )
		{
			c = *(signed char *)data;
			if ( c == '\"' || !c )
			{
				token[len] = 0;
				// the engine steps past the end of the text after an unterminated quote; here it stops on the 0
				return c ? data + 1 : data;
			}
			data++;
			if ( len < AE_MAX_TOKEN - 1 )
				token[len++] = (char)c;
		}
	}

	if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' )
	{
		token[len++] = (char)c;
		token[len] = 0;
		return data + 1;
	}

	do
	{
		if ( len < AE_MAX_TOKEN - 1 )
			token[len++] = (char)c;
		data++;
		c = *(signed char *)data;
		if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' )
			break;
	} while ( c > 32 );

	token[len] = 0;
	return data;
}

//=============================================================================
// files (scripts; sprites are registered with their frame count)
//=============================================================================

static std::string Shim_Lower( const char *sz )
{
	std::string s( sz );
	for ( size_t i = 0; i < s.size(); i++ )
	{
		if ( s[i] >= 'A' && s[i] <= 'Z' )
			s[i] = s[i] - 'A' + 'a';
		else if ( s[i] == '\\' )
			s[i] = '/';
	}
	return s;
}

struct ShimFile
{
	std::string name;
	std::vector<byte> data;
};

struct ShimSprite
{
	std::string name;
	model_s model;
};

static std::vector<ShimFile> g_Files;
static std::vector<ShimSprite> g_Sprites;	// HSPRITE = index + 1 (in SPR_Load order)
static std::vector<ShimSprite> g_SpriteDefs;	// what the editor registered (name, frame count)

static byte *Shim_COM_LoadFile( char *path, int usehunk, int *pLength )
{
	(void)usehunk;
	const std::string name = Shim_Lower( path );
	if ( pLength )
		*pLength = 0;

	for ( size_t i = 0; i < g_Files.size(); i++ )
	{
		if ( g_Files[i].name != name )
			continue;
		const size_t len = g_Files[i].data.size();
		byte *p = (byte *)malloc( len + 1 );
		if ( len )
			memcpy( p, &g_Files[i].data[0], len );
		p[len] = 0;
		if ( pLength )
			*pLength = (int)len;
		return p;
	}

	for ( size_t i = 0; i < g_SpriteDefs.size(); i++ )
	{
		if ( g_SpriteDefs[i].name != name )
			continue;
		// a sprite: its bytes are in the browser; the game only checks that it is there
		byte *p = (byte *)malloc( 2 );
		p[0] = 'I';
		p[1] = 0;
		if ( pLength )
			*pLength = 1;
		return p;
	}
	return NULL;
}

static void Shim_COM_FreeFile( void *buffer )
{
	free( buffer );
}

static HSPRITE Shim_SPR_Load( const char *szPicName )
{
	const std::string name = Shim_Lower( szPicName );
	for ( size_t i = 0; i < g_Sprites.size(); i++ )
	{
		if ( g_Sprites[i].name == name )
			return (HSPRITE)( i + 1 );
	}
	for ( size_t i = 0; i < g_SpriteDefs.size(); i++ )
	{
		if ( g_SpriteDefs[i].name == name )
		{
			g_Sprites.push_back( g_SpriteDefs[i] );
			return (HSPRITE)g_Sprites.size();
		}
	}
	return 0;
}

static const struct model_s *Shim_GetSpritePointer( HSPRITE hSprite )
{
	if ( hSprite < 1 || hSprite > (int)g_Sprites.size() )
		return NULL;
	return &g_Sprites[hSprite - 1].model;
}

//=============================================================================
// the test scene: contents, point trace, light
//=============================================================================

struct ShimBox
{
	float mins[3], maxs[3];
};

static int Shim_Solids( ShimBox *boxes )
{
	int n = 0;
	if ( g_Params[P_FLOOR_ON] != 0 )
	{
		boxes[n].mins[0] = boxes[n].mins[1] = -1.0e6f;
		boxes[n].mins[2] = -1.0e6f;
		boxes[n].maxs[0] = boxes[n].maxs[1] = 1.0e6f;
		boxes[n].maxs[2] = g_Params[P_FLOOR_Z];
		n++;
	}
	if ( g_Params[P_WALL_ON] != 0 )
	{
		for ( int i = 0; i < 3; i++ )
		{
			boxes[n].mins[i] = g_Params[P_WALL_MINX + i];
			boxes[n].maxs[i] = g_Params[P_WALL_MAXX + i];
		}
		n++;
	}
	return n;
}

static bool Shim_InBox( const float *p, const float *mins, const float *maxs )
{
	return p[0] >= mins[0] && p[0] <= maxs[0] && p[1] >= mins[1] && p[1] <= maxs[1] && p[2] >= mins[2] && p[2] <= maxs[2];
}

static int Shim_PointContents( float *point, int *truecontents )
{
	ShimBox boxes[4];
	const int n = Shim_Solids( boxes );
	int iContents = CONTENTS_EMPTY;

	for ( int i = 0; i < n; i++ )
	{
		if ( Shim_InBox( point, boxes[i].mins, boxes[i].maxs ) )
			iContents = CONTENTS_SOLID;
	}

	if ( iContents == CONTENTS_EMPTY && g_Params[P_WATER_ON] != 0 &&
		Shim_InBox( point, &g_Params[P_WATER_MINX], &g_Params[P_WATER_MAXX] ) )
		iContents = CONTENTS_WATER;

	if ( truecontents )
		*truecontents = iContents;
	return iContents;
}

static int g_iTraceHull = 0;

static void Shim_EV_SetTraceHull( int hull )
{
	g_iTraceHull = hull;
}

#define DIST_EPSILON ( 0.03125f )

// a point trace against the scene's solid boxes (the engine's point hull: the hit is pulled back by DIST_EPSILON)
static void Shim_EV_PlayerTrace( float *start, float *end, int traceFlags, int ignore_pe, struct pmtrace_s *tr )
{
	(void)traceFlags;
	(void)ignore_pe;
	memset( tr, 0, sizeof( *tr ) );
	tr->fraction = 1;
	tr->endpos = Vector( end );
	tr->ent = -1;

	ShimBox boxes[4];
	const int n = Shim_Solids( boxes );

	bool bStartSolid = false, bEndSolid = false;
	for ( int i = 0; i < n; i++ )
	{
		if ( Shim_InBox( start, boxes[i].mins, boxes[i].maxs ) )
			bStartSolid = true;
		if ( Shim_InBox( end, boxes[i].mins, boxes[i].maxs ) )
			bEndSolid = true;
	}

	if ( bStartSolid )
	{
		tr->startsolid = 1;
		if ( bEndSolid )
		{
			tr->allsolid = 1;
			tr->fraction = 0;
			tr->endpos = Vector( start );
		}
		return;
	}

	float bestFrac = 1;
	Vector bestNormal;
	bool bHit = false;

	for ( int b = 0; b < n; b++ )
	{
		// entering the box: the latest entry over the three slabs, before the earliest exit
		float tEnter = -1e30f, tExit = 1e30f;
		int enterAxis = -1;
		float enterSign = 0;
		bool bMiss = false;
		for ( int i = 0; i < 3; i++ )
		{
			const float d = end[i] - start[i];
			if ( d == 0 )
			{
				if ( start[i] < boxes[b].mins[i] || start[i] > boxes[b].maxs[i] )
				{
					bMiss = true;
					break;
				}
				continue;
			}
			float t1 = ( boxes[b].mins[i] - start[i] ) / d;
			float t2 = ( boxes[b].maxs[i] - start[i] ) / d;
			float sign = -1;	// entering through the min face: normal points to -axis
			if ( t1 > t2 )
			{
				const float t = t1;
				t1 = t2;
				t2 = t;
				sign = 1;
			}
			if ( t1 > tEnter )
			{
				tEnter = t1;
				enterAxis = i;
				enterSign = sign;
			}
			if ( t2 < tExit )
				tExit = t2;
		}
		if ( bMiss || enterAxis < 0 || tEnter > tExit || tEnter < 0 || tEnter > 1 )
			continue;

		const float len = ( Vector( end ) - Vector( start ) ).Length();
		float frac = tEnter - ( len > 0 ? DIST_EPSILON / len : 0 );
		if ( frac < 0 )
			frac = 0;
		if ( frac < bestFrac )
		{
			bestFrac = frac;
			bestNormal = Vector( 0, 0, 0 );
			bestNormal[enterAxis] = enterSign;
			bHit = true;
		}
	}

	if ( bHit )
	{
		tr->fraction = bestFrac;
		tr->endpos = Vector( start ) + ( Vector( end ) - Vector( start ) ) * bestFrac;
		tr->plane.normal = bestNormal;
		tr->ent = 0;
	}
}

static void Shim_LightAtPoint( float *pos, float *value )
{
	for ( int i = 0; i < 3; i++ )
		value[i] = g_Params[P_LIGHT_R + i];

	if ( g_Params[P_LAMP_ON] != 0 && g_Params[P_LAMP_RADIUS] > 0 )
	{
		const Vector d = Vector( pos ) - Vector( &g_Params[P_LAMP_X] );
		const float f = 1 - d.Length() / g_Params[P_LAMP_RADIUS];
		if ( f > 0 )
		{
			for ( int i = 0; i < 3; i++ )
				value[i] += g_Params[P_LAMP_R + i] * f;
		}
	}
	for ( int i = 0; i < 3; i++ )
	{
		if ( value[i] > 255 )
			value[i] = 255;
		if ( value[i] < 0 )
			value[i] = 0;
	}
}

static int Shim_BoxInPVS( float *mins, float *maxs )
{
	(void)mins;
	(void)maxs;
	return g_Params[P_PVS] != 0 ? 1 : 0;
}

//=============================================================================
// entities: 1 = the local player, 2 = the followed target
//=============================================================================

static cl_entity_t g_LocalPlayer;
static cl_entity_t g_Target;
static model_s g_TargetModel;
static model_s g_PlayerModel;

static cl_entity_t *Shim_GetLocalPlayer( void )
{
	return &g_LocalPlayer;
}

static cl_entity_t *Shim_GetEntityByIndex( int idx )
{
	if ( idx == 1 )
		return &g_LocalPlayer;
	if ( idx == 2 )
		return &g_Target;
	return NULL;
}

static void Shim_UpdateEntities( void )
{
	g_PlayerModel.type = mod_studio;
	g_PlayerModel.numframes = 1;
	g_LocalPlayer.index = 1;
	g_LocalPlayer.model = &g_PlayerModel;
	g_LocalPlayer.curstate.messagenum = 100;
	g_LocalPlayer.origin = v_origin;

	g_TargetModel.type = g_Params[P_TARGET_STUDIO] != 0 ? mod_studio : mod_brush;
	g_TargetModel.numframes = 1;
	g_Target.index = 2;
	g_Target.model = &g_TargetModel;
	// a target not in this frame's packet has an older message number
	g_Target.curstate.messagenum = g_Params[P_TARGET_VISIBLE] != 0 ? 100 : 99;
	g_Target.origin = Vector( &g_Params[P_TARGET_X] );
	g_Target.angles = Vector( &g_Params[P_TARGET_PITCH] );
	for ( int i = 0; i < 4; i++ )
		g_Target.attachment[i] = Vector( &g_Params[P_ATT0_X + i * 3] );
}

static float Shim_GetClientTime( void )
{
	return g_Params[P_TIME];
}

//=============================================================================
// user messages
//=============================================================================

static pfnUserMsgHook g_pfnAurora = NULL;

static int Shim_HookUserMsg( const char *szMsgName, pfnUserMsgHook pfn )
{
	if ( !strcmp( szMsgName, "Aurora" ) )
		g_pfnAurora = pfn;
	return 1;
}

//=============================================================================
// the triangle API: the engine's state rules, quads recorded for the renderer
//=============================================================================

// one quad: 4 x (x y z u v), r g b a (what reaches GL: after Color4f's rule), sprite, frame, rendermode, depth mask
#define AE_QUAD_FLOATS 28

static std::vector<float> g_Quads;
static int g_iRenderMode = kRenderNormal;
static int g_bDepthMask = 1;
static float g_Color[4] = { 1, 1, 1, 1 };
static int g_iSprite = 0;
static int g_iFrame = 0;
static int g_iPrim = -1;
static int g_iVerts = 0;
static float g_Verts[4][5];
static float g_TexCoord[2];

static void Shim_RenderMode( int mode )
{
	g_iRenderMode = mode;
	switch ( mode )
	{
	case kRenderNormal:
		g_bDepthMask = 1;
		break;
	case kRenderTransAlpha:
	case kRenderTransAdd:
	case kRenderGlow:
		g_bDepthMask = 0;
		break;
	default:	// kRenderTransColor, kRenderTransTexture: the depth mask stays as it was
		break;
	}
}

static void Shim_Color4f( float r, float g, float b, float a )
{
	if ( g_iRenderMode == kRenderTransAlpha )
	{
		// glColor4ub( r * 255.9, g * 255.9, b * 255.9, a * 255 ): quantised to bytes
		g_Color[0] = (float)(int)( r * 255.9f ) / 255.0f;
		g_Color[1] = (float)(int)( g * 255.9f ) / 255.0f;
		g_Color[2] = (float)(int)( b * 255.9f ) / 255.0f;
		g_Color[3] = (float)(int)( a * 255.0f ) / 255.0f;
	}
	else
	{
		g_Color[0] = r * a;
		g_Color[1] = g * a;
		g_Color[2] = b * a;
		g_Color[3] = 1;
	}
}

static void Shim_Color4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	g_Color[0] = r / 255.0f;
	g_Color[1] = g / 255.0f;
	g_Color[2] = b / 255.0f;
	g_Color[3] = a / 255.0f;
}

static void Shim_Begin( int prim )
{
	g_iPrim = prim;
	g_iVerts = 0;
}

static void Shim_TexCoord2f( float u, float v )
{
	g_TexCoord[0] = u;
	g_TexCoord[1] = v;
}

static void Shim_Vertex3f( float x, float y, float z )
{
	if ( g_iPrim != TRI_QUADS )
		return;

	float *pv = g_Verts[g_iVerts];
	pv[0] = x;
	pv[1] = y;
	pv[2] = z;
	pv[3] = g_TexCoord[0];
	pv[4] = g_TexCoord[1];

	if ( ++g_iVerts < 4 )
		return;
	g_iVerts = 0;

	for ( int i = 0; i < 4; i++ )
		for ( int j = 0; j < 5; j++ )
			g_Quads.push_back( g_Verts[i][j] );
	for ( int i = 0; i < 4; i++ )
		g_Quads.push_back( g_Color[i] );
	g_Quads.push_back( (float)g_iSprite );
	g_Quads.push_back( (float)g_iFrame );
	g_Quads.push_back( (float)g_iRenderMode );
	g_Quads.push_back( (float)g_bDepthMask );
}

static void Shim_Vertex3fv( float *p )
{
	Shim_Vertex3f( p[0], p[1], p[2] );
}

static void Shim_End( void )
{
	g_iPrim = -1;
	g_iVerts = 0;
}

static void Shim_Brightness( float )
{
}

static void Shim_CullFace( int )
{
}

static int Shim_SpriteTexture( struct model_s *pSpriteModel, int frame )
{
	g_iSprite = 0;
	for ( size_t i = 0; i < g_Sprites.size(); i++ )
	{
		if ( &g_Sprites[i].model == pSpriteModel )
			g_iSprite = (int)i + 1;
	}
	if ( !g_iSprite )
		return 0;
	g_iFrame = frame;
	return 1;
}

static triangleapi_s g_TriAPI;
static event_api_s g_EventAPI;
cl_enginefunc_t gEngfuncs;

static void Shim_Setup( void )
{
	memset( &g_TriAPI, 0, sizeof( g_TriAPI ) );
	g_TriAPI.version = 1;
	g_TriAPI.RenderMode = Shim_RenderMode;
	g_TriAPI.Begin = Shim_Begin;
	g_TriAPI.End = Shim_End;
	g_TriAPI.Color4f = Shim_Color4f;
	g_TriAPI.Color4ub = Shim_Color4ub;
	g_TriAPI.TexCoord2f = Shim_TexCoord2f;
	g_TriAPI.Vertex3fv = Shim_Vertex3fv;
	g_TriAPI.Vertex3f = Shim_Vertex3f;
	g_TriAPI.Brightness = Shim_Brightness;
	g_TriAPI.CullFace = Shim_CullFace;
	g_TriAPI.SpriteTexture = Shim_SpriteTexture;
	g_TriAPI.BoxInPVS = Shim_BoxInPVS;
	g_TriAPI.LightAtPoint = Shim_LightAtPoint;

	g_EventAPI.EV_SetTraceHull = Shim_EV_SetTraceHull;
	g_EventAPI.EV_PlayerTrace = Shim_EV_PlayerTrace;

	memset( &gEngfuncs, 0, sizeof( gEngfuncs ) );
	gEngfuncs.pfnRandomFloat = Shim_RandomFloat;
	gEngfuncs.pfnRandomLong = Shim_RandomLong;
	gEngfuncs.Con_Printf = Shim_Con_Printf;
	gEngfuncs.COM_ParseFile = Shim_COM_ParseFile;
	gEngfuncs.COM_LoadFile = Shim_COM_LoadFile;
	gEngfuncs.COM_FreeFile = Shim_COM_FreeFile;
	gEngfuncs.GetEntityByIndex = Shim_GetEntityByIndex;
	gEngfuncs.GetLocalPlayer = Shim_GetLocalPlayer;
	gEngfuncs.GetClientTime = Shim_GetClientTime;
	gEngfuncs.PM_PointContents = Shim_PointContents;
	gEngfuncs.GetSpritePointer = Shim_GetSpritePointer;
	gEngfuncs.pfnHookUserMsg = Shim_HookUserMsg;
	gEngfuncs.pfnSPR_Load = Shim_SPR_Load;
	gEngfuncs.pTriAPI = &g_TriAPI;
	gEngfuncs.pEventAPI = &g_EventAPI;
}

//=============================================================================
// the C API
//=============================================================================

AE_API void ae_init( int seed )
{
	Shim_Setup();
	SeedRandomNumberGenerator( seed );
	memset( g_Params, 0, sizeof( g_Params ) );
	g_Params[P_PVS] = 1;
	g_Params[P_TARGET_VISIBLE] = 1;
	g_Params[P_TARGET_STUDIO] = 1;
	g_Params[P_LIGHT_R] = g_Params[P_LIGHT_G] = g_Params[P_LIGHT_B] = 255;
	Aurora_Init();
	Aurora_VidInit();
}

AE_API void ae_seed( int seed )
{
	SeedRandomNumberGenerator( seed );
}

AE_API float *ae_params( void )
{
	return g_Params;
}

// scripts (and any other file the game would open)
AE_API void ae_file_add( const char *name, const byte *data, int len )
{
	const std::string lname = Shim_Lower( name );
	for ( size_t i = 0; i < g_Files.size(); i++ )
	{
		if ( g_Files[i].name == lname )
		{
			g_Files[i].data.assign( data, data + len );
			return;
		}
	}
	ShimFile f;
	f.name = lname;
	f.data.assign( data, data + len );
	g_Files.push_back( f );
}

AE_API void ae_file_clear( void )
{
	g_Files.clear();
}

// a sprite the game may load: its name and frame count (the browser has its pixels)
AE_API void ae_sprite_add( const char *name, int numframes )
{
	const std::string lname = Shim_Lower( name );
	for ( size_t i = 0; i < g_SpriteDefs.size(); i++ )
	{
		if ( g_SpriteDefs[i].name == lname )
		{
			g_SpriteDefs[i].model.numframes = numframes;
			return;
		}
	}
	ShimSprite s;
	memset( &s.model, 0, sizeof( s.model ) );
	s.name = lname;
	strncpy( s.model.name, lname.c_str(), sizeof( s.model.name ) - 1 );
	s.model.type = mod_sprite;
	s.model.numframes = numframes;
	g_SpriteDefs.push_back( s );
}

AE_API void ae_sprite_clear( void )
{
	g_SpriteDefs.clear();
}

// a new map: no systems, the script and sprite caches emptied (the next build reads the files again)
AE_API void ae_vidinit( void )
{
	Aurora_VidInit();
	g_Sprites.clear();
}

// the name of the sprite behind a handle the quads carry
AE_API const char *ae_sprite_name( int hSprite )
{
	if ( hSprite < 1 || hSprite > (int)g_Sprites.size() )
		return "";
	return g_Sprites[hSprite - 1].name.c_str();
}

// the "Aurora" message as env_particle writes it (aurora_shared.h)
AE_API void ae_message( int flags, int id, float ox, float oy, float oz, float ax, float ay, float az, int target,
	int attachment, const char *script )
{
	if ( !g_pfnAurora )
		return;

	std::vector<byte> buf;
	buf.push_back( (byte)flags );
	buf.push_back( (byte)( id & 0xff ) );
	buf.push_back( (byte)( ( id >> 8 ) & 0xff ) );

	if ( !( flags & AURORA_MSG_REMOVE ) )
	{
		const float origin[3] = { ox, oy, oz };
		for ( int i = 0; i < 3; i++ )
		{
			byte b[4];
			memcpy( b, &origin[i], 4 );
			buf.insert( buf.end(), b, b + 4 );
		}
		const float angles[3] = { ax, ay, az };
		for ( int i = 0; i < 3; i++ )
		{
			const int s = (int)( angles[i] * 8.0f );	// WRITE_COORD
			buf.push_back( (byte)( s & 0xff ) );
			buf.push_back( (byte)( ( s >> 8 ) & 0xff ) );
		}
		if ( flags & AURORA_MSG_FOLLOW )
		{
			buf.push_back( (byte)( target & 0xff ) );
			buf.push_back( (byte)( ( target >> 8 ) & 0xff ) );
			buf.push_back( (byte)(signed char)attachment );
		}
		const size_t len = strlen( script );
		buf.insert( buf.end(), script, script + len );
		buf.push_back( 0 );
	}

	g_pfnAurora( "Aurora", (int)buf.size(), &buf[0] );
}

// one client frame at the clock in ae_params(): update and draw every system; returns the number of quads
AE_API int ae_frame( void )
{
	v_origin = Vector( &g_Params[P_CAM_X] );
	Shim_UpdateEntities();
	g_Quads.clear();
	g_bDepthMask = 1;	// the manager leaves kRenderNormal set after each frame
	Aurora_DrawFrame();
	return (int)( g_Quads.size() / AE_QUAD_FLOATS );
}

AE_API float *ae_quads( void )
{
	return g_Quads.empty() ? NULL : &g_Quads[0];
}

AE_API int ae_quad_floats( void )
{
	return AE_QUAD_FLOATS;
}

AE_API const char *ae_log( void )
{
	return g_Log.c_str();
}

AE_API void ae_log_clear( void )
{
	g_Log.clear();
}

static int Shim_CountParticles( ParticleSystem *pSys )
{
	int n = 0;
	for ( particle *p = pSys->m_pActiveParticle; p; p = p->nextpart )
		for ( particle *l = p; l; l = l->m_pOverlay )
			n++;
	return n;
}

// 0 particles alive in all systems, 1 allocated in all systems, 2 systems, 3 particles alive in the system of
// that id, 4 one-shot copies, 5 the system of that id exists and is on
AE_API int ae_stat( int which, int id )
{
	switch ( which )
	{
	case 0: return g_iAuroraActiveParticles;
	case 1: return g_iAuroraAllocatedParticles;
	case 2:
	{
		int n = 0;
		for ( ParticleSystem *p = g_ParticleSystems.m_pFirstSystem; p; p = p->m_pNextSystem )
			n++;
		return n;
	}
	case 3:
	{
		int n = 0;
		for ( ParticleSystem *p = g_ParticleSystems.m_pFirstSystem; p; p = p->m_pNextSystem )
			if ( p->m_iId == id )
				n += Shim_CountParticles( p );
		return n;
	}
	case 4: return g_ParticleSystems.CountOneShots();
	case 5:
	{
		ParticleSystem *p = g_ParticleSystems.FindSystem( id );
		return p && p->IsOn() ? 1 : 0;
	}
	case 6:
	{
		// particles alive in one-shot copies
		int n = 0;
		for ( ParticleSystem *p = g_ParticleSystems.m_pFirstSystem; p; p = p->m_pNextSystem )
			if ( p->m_bOneShot )
				n += Shim_CountParticles( p );
		return n;
	}
	}
	return 0;
}

// parity test: the origins of the particles alive in a system (x y z each), in list order
static std::vector<float> g_Dump;

AE_API int ae_dump( int id )
{
	g_Dump.clear();
	for ( ParticleSystem *p = g_ParticleSystems.m_pFirstSystem; p; p = p->m_pNextSystem )
	{
		if ( p->m_iId != id )
			continue;
		for ( particle *part = p->m_pActiveParticle; part; part = part->nextpart )
		{
			g_Dump.push_back( part->origin.x );
			g_Dump.push_back( part->origin.y );
			g_Dump.push_back( part->origin.z );
			g_Dump.push_back( part->m_fSize );
		}
	}
	return (int)g_Dump.size() / 4;
}

AE_API float *ae_dump_ptr( void )
{
	return g_Dump.empty() ? NULL : &g_Dump[0];
}
