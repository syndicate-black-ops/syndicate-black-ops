// Stub engine for tests/movewith_client (the real hud.h is not used): only what cl_dll/movewith_client.cpp calls.
#ifndef MW_STUB_HUD_H
#define MW_STUB_HUD_H
#include <string.h>
typedef float vec_t;
typedef vec_t vec3_t[3];
typedef int qboolean;
#include "const.h"
#include "cl_entity.h"
#include "cvardef.h"

struct mw_stub_enginefuncs_t
{
	cl_entity_t *( *GetEntityByIndex )( int idx );
	void ( *Con_Printf )( const char *fmt, ... );
	int ( *pfnAddCommand )( const char *name, void ( *function )( void ) );
	float ( *GetClientTime )( void );
	int ( *Cmd_Argc )( void );
	const char *( *Cmd_Argv )( int arg );
	cl_entity_t *( *GetLocalPlayer )( void );
	cl_entity_t *( *GetViewModel )( void );
	int ( *GetMaxClients )( void );
	struct cvar_s *( *pfnGetCvarPointer )( const char *name );
	double ( *GetAbsoluteTime )( void );
};
extern mw_stub_enginefuncs_t gEngfuncs;
#endif
