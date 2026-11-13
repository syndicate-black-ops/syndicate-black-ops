// Stub engine for tests/mover_prediction (the real hud.h is not used): what cl_dll/mover_prediction.cpp and
// cl_dll/movewith_client.cpp call. Both are compiled against this one header, with the client's Vector as vec3_t.
#ifndef MP_STUB_HUD_H
#define MP_STUB_HUD_H
#include <string.h>
#include "util_vector.h"
typedef int qboolean;
#include "const.h"
#include "cl_entity.h"
#include "cvardef.h"

struct mp_stub_enginefuncs_t
{
	cl_entity_t *( *GetEntityByIndex )( int idx );
	void ( *Con_Printf )( const char *fmt, ... );
	void ( *Con_DPrintf )( const char *fmt, ... );
	int ( *pfnAddCommand )( const char *name, void ( *function )( void ) );
	float ( *GetClientTime )( void );
	double ( *GetAbsoluteTime )( void );
	int ( *Cmd_Argc )( void );
	const char *( *Cmd_Argv )( int arg );
	cl_entity_t *( *GetLocalPlayer )( void );
	cl_entity_t *( *GetViewModel )( void );
	int ( *GetMaxClients )( void );
	void ( *pfnAngleVectors )( const float *angles, float *forward, float *right, float *up );
	cvar_t *( *pfnGetCvarPointer )( const char *name );
};
extern mp_stub_enginefuncs_t gEngfuncs;
#endif
