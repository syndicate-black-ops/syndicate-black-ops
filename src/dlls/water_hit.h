//=========================================================
// SBO 2.1 water splashes of server-fired bullets (monsters, the Apache, turrets, mounted guns): CBaseEntity::FireBullets
// collects the points where its bullets cross a water surface and sends them in one "WaterHit" message to the PVS of
// the first one. The client draws them like the splashes of a player's own shots (cl_dll/materials.cpp).
//=========================================================

#ifndef WATER_HIT_H
#define WATER_HIT_H

#include "pm_materials.h"	// MAT_WATERHIT_MAX

// CWorld::Precache: a new map; its water brushes are looked up again on the first shot
void WaterHit_Reset( void );

// the water entry points of one FireBullets call
class CWaterHits
{
public:
	// fEnabled FALSE: nothing is checked or sent
	CWaterHits( const Vector &vecSrc, BOOL fEnabled );

	// a bullet from the source to vecEnd that hit pHit (NULL: nothing): its entry point if it crosses a water surface
	// (at most MAT_WATERHIT_MAX), with the player it hit, whose client draws nothing for it
	void Add( const Vector &vecEnd, edict_t *pHit );

	// one message with every entry point, nothing if there is none
	void Send( void );

	int Count( void ) const { return m_cHits; }

private:
	Vector	m_vecSrc;
	BOOL	m_fEnabled;
	BOOL	m_fSrcInWorldLiquid;
	Vector	m_vecHits[ MAT_WATERHIT_MAX ];
	int		m_iHitPlayer[ MAT_WATERHIT_MAX ];	// the player the bullet hit (entity index), 0 none
	int		m_cHits;
};

#endif // WATER_HIT_H
