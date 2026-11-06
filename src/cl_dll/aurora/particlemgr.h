// Aurora particle system manager (Spirit of Half-Life, "02/08/02 November235: Particle System", LRC).
//
// SBO 2.0 port of halflife-updated-spirit (LogicAndTrick, spirit-1.8-bug-fixes, 2482306) cl_dll/particlemgr.h and
// particlemsg.cpp: the systems are keyed by the env_particle's entity index, so a message for a system the client
// has already updates it (Spirit added a second system each time), the "Aurora" message also switches and removes
// them, and the scripts and sprites are loaded once a map. docs/AURORA.md.
#pragma once

class ParticleSystem;

#define AURORA_MAX_ONESHOTS		32		// one-shot copies (Spawn Use) at a time; the oldest goes for a new one

class ParticleSystemManager
{
public:
	ParticleSystemManager( void );
	~ParticleSystemManager( void );

	void AddSystem( ParticleSystem *pNewSystem );
	ParticleSystem *FindSystem( int iId );
	void RemoveSystem( ParticleSystem *pSystem );
	void UpdateSystems( float frametime );
	void DrawSystems( const Vector &vecView, const Vector &forward, const Vector &right, const Vector &up );
	void ClearSystems( void );
	void SortSystems( const Vector &vecView );
	int CountOneShots( void );
	ParticleSystem *OldestOneShot( void );

	ParticleSystem *m_pFirstSystem;
};

extern ParticleSystemManager g_ParticleSystems;

void Aurora_Init( void );		// CHud::Init: hooks the message
void Aurora_VidInit( void );	// CHud::VidInit: new map: no systems, scripts and sprites loaded again
void Aurora_Reset( void );		// InitHUD (new server or map): no systems until the server sends them
void Aurora_DrawFrame( void );	// HUD_DrawTransparentTriangles: update and draw, once a frame
