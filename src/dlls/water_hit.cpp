//=========================================================
// SBO 2.1 water splashes of server-fired bullets (water_hit.h). The world's water is found with POINT_CONTENTS, which
// only reads the world; water brush entities (func_water, a door with liquid contents) are tested by their bounding
// boxes. A bullet whose start and end are both in water or both out of it gets no splash, as on the client.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "water_hit.h"
#include "sbo_test.h"	// SBO_TEST_COMMANDS builds: test_bulletbench

extern int gmsgWaterHit;

#define WATERHIT_BRUSHES_MAX	64

static EHANDLE	s_hWaterBrushes[ WATERHIT_BRUSHES_MAX ];
static int		s_cWaterBrushes;
static BOOL		s_fWaterBrushesFound;

static BOOL WaterHit_IsLiquid( int iContents )
{
	return iContents <= CONTENTS_WATER && iContents >= CONTENTS_LAVA;
}

void WaterHit_Reset( void )
{
	s_cWaterBrushes = 0;
	s_fWaterBrushesFound = FALSE;
}

// once per map, on the first shot: the brush entities whose contents are a liquid
static void WaterHit_FindBrushes( void )
{
	s_fWaterBrushesFound = TRUE;
	s_cWaterBrushes = 0;
	for ( int i = gpGlobals->maxClients + 1; i < gpGlobals->maxEntities && s_cWaterBrushes < WATERHIT_BRUSHES_MAX; i++ )
	{
		edict_t *pEdict = INDEXENT( i );
		if ( !pEdict || pEdict->free || !pEdict->v.model )
			continue;
		const char *pszModel = STRING( pEdict->v.model );
		if ( pszModel[ 0 ] != '*' )
			continue;
		if ( pEdict->v.solid != SOLID_NOT || !WaterHit_IsLiquid( pEdict->v.skin ) )
			continue;
		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );
		if ( pEntity )
			s_hWaterBrushes[ s_cWaterBrushes++ ] = pEntity;
	}
}

static BOOL WaterHit_InBox( const Vector &vecPoint, const Vector &vecMins, const Vector &vecMaxs )
{
	return vecPoint.x >= vecMins.x && vecPoint.x <= vecMaxs.x && vecPoint.y >= vecMins.y && vecPoint.y <= vecMaxs.y
		&& vecPoint.z >= vecMins.z && vecPoint.z <= vecMaxs.z;
}

// the point where the world's contents change between vecStart and vecEnd, to 4 units (bounded bisection)
static Vector WaterHit_WorldSurface( const Vector &vecStart, const Vector &vecEnd )
{
	Vector vecBegin = vecStart;
	Vector vecFinish = vecEnd;
	const int iBeginContents = UTIL_PointContents( vecBegin );
	for ( int iStep = 0; iStep < 32 && ( vecFinish - vecBegin ).Length() > 4.0f; iStep++ )
	{
		const Vector vecHalf = ( vecBegin + vecFinish ) * 0.5f;
		if ( UTIL_PointContents( vecHalf ) == iBeginContents )
			vecBegin = vecHalf;
		else
			vecFinish = vecHalf;
	}
	return vecBegin;
}

CWaterHits::CWaterHits( const Vector &vecSrc, BOOL fEnabled )
{
	m_vecSrc = vecSrc;
	m_fEnabled = fEnabled;
#ifdef SBO_TEST_COMMANDS
	if ( Test_WaterHitOff() )	// test builds only: test_bulletbench's blocks without the check
		m_fEnabled = FALSE;
#endif
	m_fSrcInWorldLiquid = FALSE;
	m_cHits = 0;
	if ( !m_fEnabled )
		return;
	m_fSrcInWorldLiquid = WaterHit_IsLiquid( UTIL_PointContents( vecSrc ) );
	if ( !s_fWaterBrushesFound )
		WaterHit_FindBrushes();
}

void CWaterHits::Add( const Vector &vecEnd, edict_t *pHit )
{
	if ( m_fEnabled && m_cHits < MAT_WATERHIT_MAX )
	{
		const int iHit = pHit ? ENTINDEX( pHit ) : 0;
		m_iHitPlayer[ m_cHits ] = iHit >= 1 && iHit <= gpGlobals->maxClients ? iHit : 0;
	}

	if ( !m_fEnabled || m_cHits >= MAT_WATERHIT_MAX )
		return;

	// the world's water
	if ( WaterHit_IsLiquid( UTIL_PointContents( vecEnd ) ) != m_fSrcInWorldLiquid )
	{
		m_vecHits[ m_cHits++ ] = WaterHit_WorldSurface( m_vecSrc, vecEnd );
		return;
	}

	// water brush entities, where they are now (a moving one too): the bullet starts inside the bounding box and
	// ends outside it, or the other way round; then a trace against the brush's own shape from the outside end
	// finds the surface, and a bullet that only crossed the box beside the brush splashes nowhere
	for ( int i = 0; i < s_cWaterBrushes; i++ )
	{
		CBaseEntity *pWater = s_hWaterBrushes[ i ];
		if ( !pWater )		// removed
			continue;
		const Vector vecMins = pWater->pev->absmin;
		const Vector vecMaxs = pWater->pev->absmax;
		const BOOL fSrcIn = WaterHit_InBox( m_vecSrc, vecMins, vecMaxs );
		if ( fSrcIn == WaterHit_InBox( vecEnd, vecMins, vecMaxs ) )
			continue;
		const Vector vecOut = fSrcIn ? vecEnd : m_vecSrc;
		const Vector vecIn = fSrcIn ? m_vecSrc : vecEnd;
		TraceResult tr;
		g_engfuncs.pfnTraceModel( vecOut, vecIn, point_hull, pWater->edict(), &tr );
		if ( tr.fStartSolid || tr.flFraction >= 1.0f )
			continue;
		m_vecHits[ m_cHits++ ] = tr.vecEndPos;
		return;
	}
}

void CWaterHits::Send( void )
{
	if ( m_cHits <= 0 || !gmsgWaterHit )
		return;

	MESSAGE_BEGIN( MSG_PVS, gmsgWaterHit, m_vecHits[ 0 ] );
		WRITE_BYTE( m_cHits );
		for ( int i = 0; i < m_cHits; i++ )
		{
			WRITE_COORD( m_vecHits[ i ].x );
			WRITE_COORD( m_vecHits[ i ].y );
			WRITE_COORD( m_vecHits[ i ].z );
			WRITE_BYTE( m_iHitPlayer[ i ] );
		}
	MESSAGE_END();
}
