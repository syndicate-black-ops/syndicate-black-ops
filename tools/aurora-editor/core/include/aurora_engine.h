// Aurora editor: the engine surface that src/cl_dll/aurora/particlesys.cpp and particlemgr.cpp use, and nothing
// else. Every SDK header those files include (hud.h, cl_util.h, const.h, ...) is a stub in this folder that
// includes this file, so the game's own sources compile unchanged for the editor (WebAssembly) and for the native
// parity test. The functions behind gEngfuncs are in ../shim.cpp. Values of the constants are the HL SDK's.
#pragma once

#include <math.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

typedef unsigned char byte;
typedef int HSPRITE;
typedef int qboolean;
typedef float vec_t;

#ifndef stricmp
#define stricmp strcasecmp
#endif

class Vector
{
public:
	Vector() : x( 0 ), y( 0 ), z( 0 ) {}
	Vector( float X, float Y, float Z ) : x( X ), y( Y ), z( Z ) {}
	Vector( const float *v ) : x( v[0] ), y( v[1] ), z( v[2] ) {}

	Vector operator+( const Vector &v ) const { return Vector( x + v.x, y + v.y, z + v.z ); }
	Vector operator-( const Vector &v ) const { return Vector( x - v.x, y - v.y, z - v.z ); }
	Vector operator-() const { return Vector( -x, -y, -z ); }
	Vector operator*( float f ) const { return Vector( x * f, y * f, z * f ); }
	Vector operator/( float f ) const { return Vector( x / f, y / f, z / f ); }
	bool operator==( const Vector &v ) const { return x == v.x && y == v.y && z == v.z; }
	bool operator!=( const Vector &v ) const { return !( *this == v ); }
	float Length() const { return (float)sqrt( x * x + y * y + z * z ); }
	operator float *() { return &x; }
	operator const float *() const { return &x; }

	vec_t x, y, z;
};

inline Vector operator*( float f, const Vector &v ) { return v * f; }
inline float DotProduct( const Vector &a, const Vector &b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }

typedef Vector vec3_t;

void VectorMA( const float *veca, float scale, const float *vecb, float *vecc );
void AngleVectors( const float *angles, float *forward, float *right, float *up );

// const.h
#define kRenderNormal		0
#define kRenderTransColor	1
#define kRenderTransTexture	2
#define kRenderGlow			3
#define kRenderTransAlpha	4
#define kRenderTransAdd		5

#define CONTENTS_EMPTY		-1
#define CONTENTS_SOLID		-2
#define CONTENTS_WATER		-3
#define CONTENT_EMPTY		-1
#define CONTENT_SOLID		-2
#define CONTENT_WATER		-3

// triangleapi.h
#define TRI_FRONT	0
#define TRI_NONE	1
#define TRI_TRIANGLES		0
#define TRI_TRIANGLE_FAN	1
#define TRI_QUADS			2
#define TRI_POLYGON			3
#define TRI_LINES			4
#define TRI_TRIANGLE_STRIP	5
#define TRI_QUAD_STRIP		6

// com_model.h
typedef enum { mod_brush, mod_sprite, mod_alias, mod_studio } modtype_t;

struct model_s
{
	char name[64];
	int needload;
	modtype_t type;
	int numframes;
};
typedef struct model_s model_t;

// entity_state.h / cl_entity.h
struct entity_state_s
{
	int messagenum;
};
typedef struct entity_state_s entity_state_t;

struct cl_entity_s
{
	int index;
	entity_state_t curstate;
	vec3_t origin;
	vec3_t angles;
	vec3_t attachment[4];
	struct model_s *model;
};
typedef struct cl_entity_s cl_entity_t;

// pmtrace.h / pm_defs.h
struct pmplane_t
{
	vec3_t normal;
	float dist;
};

struct pmtrace_s
{
	qboolean allsolid;
	qboolean startsolid;
	qboolean inopen, inwater;
	float fraction;
	vec3_t endpos;
	pmplane_t plane;
	int ent;
	vec3_t deltavelocity;
	int hitgroup;
};
typedef struct pmtrace_s pmtrace_t;

#define PM_NORMAL			0x00000000
#define PM_STUDIO_IGNORE	0x00000001
#define PM_STUDIO_BOX		0x00000002
#define PM_GLASS_IGNORE		0x00000004
#define PM_WORLD_ONLY		0x00000008

typedef int ( *pfnUserMsgHook )( const char *pszName, int iSize, void *pbuf );

struct triangleapi_s
{
	int version;
	void ( *RenderMode )( int mode );
	void ( *Begin )( int primitiveCode );
	void ( *End )( void );
	void ( *Color4f )( float r, float g, float b, float a );
	void ( *Color4ub )( unsigned char r, unsigned char g, unsigned char b, unsigned char a );
	void ( *TexCoord2f )( float u, float v );
	void ( *Vertex3fv )( float *worldPnt );
	void ( *Vertex3f )( float x, float y, float z );
	void ( *Brightness )( float brightness );
	void ( *CullFace )( int style );
	int ( *SpriteTexture )( struct model_s *pSpriteModel, int frame );
	int ( *WorldToScreen )( float *world, float *screen );
	void ( *Fog )( float flFogColor[3], float flStart, float flEnd, int bOn );
	void ( *ScreenToWorld )( float *screen, float *world );
	void ( *GetMatrix )( const int pname, float *matrix );
	int ( *BoxInPVS )( float *mins, float *maxs );
	void ( *LightAtPoint )( float *pos, float *value );
	void ( *Color4fRendermode )( float r, float g, float b, float a, int rendermode );
	void ( *FogParams )( float flDensity, int iFogSkybox );
};
typedef struct triangleapi_s triangleapi_t;

struct event_api_s
{
	void ( *EV_SetTraceHull )( int hull );
	void ( *EV_PlayerTrace )( float *start, float *end, int traceFlags, int ignore_pe, struct pmtrace_s *tr );
};

struct cl_enginefuncs_s
{
	float ( *pfnRandomFloat )( float flLow, float flHigh );
	int ( *pfnRandomLong )( int lLow, int lHigh );
	void ( *Con_Printf )( const char *fmt, ... );
	char *( *COM_ParseFile )( char *data, char *token );
	byte *( *COM_LoadFile )( char *path, int usehunk, int *pLength );
	void ( *COM_FreeFile )( void *buffer );
	cl_entity_t *( *GetEntityByIndex )( int idx );
	cl_entity_t *( *GetLocalPlayer )( void );
	float ( *GetClientTime )( void );
	int ( *PM_PointContents )( float *point, int *truecontents );
	const struct model_s *( *GetSpritePointer )( HSPRITE hSprite );
	int ( *pfnHookUserMsg )( const char *szMsgName, pfnUserMsgHook pfn );
	HSPRITE ( *pfnSPR_Load )( const char *szPicName );
	triangleapi_s *pTriAPI;
	event_api_s *pEventAPI;
};
typedef struct cl_enginefuncs_s cl_enginefunc_t;

extern cl_enginefunc_t gEngfuncs;

// cl_util.h
#define SPR_Load ( *gEngfuncs.pfnSPR_Load )

// client functions particlemgr.cpp uses (view.cpp / tri.cpp / cdll_int.cpp in the game)
int HUD_IsHardware( void );
