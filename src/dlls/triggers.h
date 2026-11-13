/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
// triggers.h -- trigger/environment classes used outside triggers.cpp (as in ReGameDLL_CS regamedll/dlls/triggers.h)

#ifndef TRIGGERS_H
#define TRIGGERS_H

// ReGameDLL_CS: env_fog. Fog colour = rendercolor, density 0-0.01 (0 outside that range); sent to each client with the
// Fog message (CBasePlayer::UpdateClientData) and drawn by the client (tri.cpp RenderFog). See docs/WEATHER.md.
// Spirit of Half-Life (LRC, dlls/effects.cpp CEnvFog): switched on and off by triggering; an env_fog without a name is
// always on, a named one starts off unless it has spawnflag 1 "Start active" (SF_FOG_ACTIVE).
#define SF_FOG_ACTIVE	0x0001

class CClientFog : public CBaseEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	// SBO 2.1 (Spirit of Half-Life env_fog): On while it is active (an unnamed one always is), Off while it is off
	STATE GetState( CBaseEntity *pActivator ) { return FBitSet( pev->spawnflags, SF_FOG_ACTIVE ) ? STATE_ON : STATE_OFF; }
	// SBO 2.1: a key changed at run time (trigger_changevalue / trigger_copyvalue): Spawn's rule again, and the
	// players in the game get the fog in the next frame (after the change's origin update)
	CKeyValue GetKeyValue( const char *keyName );
	void RuntimeKeyValueChanged( const char *keyName );
	void EXPORT ResendThink( void );

	int m_iStartDist;
	int m_iEndDist;
	float m_fDensity;
};

#define SF_FOGZONE_ACTIVE	0x0001

// XDM (dlls/environment.h): env_fogzone, a brush volume with its own fog (same keys as env_fog). A player in it gets
// its fog instead of the map's env_fog (CBasePlayer::UpdateFogZone). See docs/WEATHER.md.
class CEnvFogZone : public CClientFog
{
public:
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return CClientFog :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	BOOL IsPointInside( const Vector &vecPoint );	// in the brush of an active zone (free observers, who touch nothing)
};

extern BOOL g_fFogZones;	// the map has an env_fogzone (reset by CWorld::Precache)

class CBasePlayer;

// PROTOTYPE (docs/WEATHER.md "env_fogzone prototype"): the zones' boxes, colors, densities and states for the client,
// which draws their haze and their fog itself (cl_dll/tri.cpp). pOnly: one zone (toggled), else the whole list.
void FogZone_Send( CBasePlayer *pPlayer, CEnvFogZone *pOnly = NULL );
void WeatherVol_SendAll( CBasePlayer *pPlayer );	// SBO 2.0: env_rain / env_snow, func_rainvolume / func_snowvolume (triggers.cpp)

#endif // TRIGGERS_H
