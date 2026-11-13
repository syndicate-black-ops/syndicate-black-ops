// Aurora particle system manager (Spirit of Half-Life, "02/08/02 November235: Particle System", LRC).
//
// SBO 2.0 port of halflife-updated-spirit (LogicAndTrick, spirit-1.8-bug-fixes, 2482306) cl_dll/particlemgr.cpp and
// cl_dll/particlemsg.cpp (CHudParticle::MsgFunc_Particle). docs/AURORA.md.

#include <math.h>	// before cl_util.h (its fabs, min and max macros)
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "parsemsg.h"
#include "particlemgr.h"
#include "particlesys.h"
#include "aurora_shared.h"
#include "aurora_editor.h"

extern vec3_t v_origin;							// view.cpp: the origin of the view being drawn
void HUD_GetEffectAngles( float *angles );		// tri.cpp: its angles

ParticleSystemManager g_ParticleSystems;

//=============================================================================
// Scripts and sprites, loaded once a map
//=============================================================================

struct AuroraScript
{
	std::string name;		// lower case
	std::string text;
	bool bFound;
	bool bBuilt;			// a system was built from it: its problems were printed
};

struct AuroraSprite
{
	std::string name;		// lower case
	HSPRITE hSprite;		// 0: not found
};

static std::vector<AuroraScript> s_Scripts;
static std::vector<AuroraSprite> s_Sprites;

static std::string Aurora_Lower( const char *sz )
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

static AuroraScript *Aurora_GetScript( const char *szName )
{
	const std::string name = Aurora_Lower( szName );
	for ( size_t i = 0; i < s_Scripts.size(); i++ )
	{
		if ( s_Scripts[i].name == name )
			return &s_Scripts[i];
	}

	AuroraScript script;
	script.name = name;
	script.bFound = false;
	script.bBuilt = false;

	int iLength = 0;
	byte *pFile = gEngfuncs.COM_LoadFile( (char *)name.c_str(), 5, &iLength );
	if ( pFile )
	{
		script.text.assign( (const char *)pFile, iLength );
		script.bFound = true;
		gEngfuncs.COM_FreeFile( pFile );
	}
	else
	{
		gEngfuncs.Con_DPrintf( "Aurora: particle script %s not found, the effect is not shown\n", name.c_str() );
	}

	s_Scripts.push_back( script );
	return &s_Scripts.back();
}

HSPRITE Aurora_LoadSprite( const char *szName )
{
	const std::string name = Aurora_Lower( szName );
	for ( size_t i = 0; i < s_Sprites.size(); i++ )
	{
		if ( s_Sprites[i].name == name )
			return s_Sprites[i].hSprite;
	}

	AuroraSprite sprite;
	sprite.name = name;
	sprite.hSprite = 0;

	// SPR_Load only for a file that is there
	int iLength = 0;
	byte *pFile = gEngfuncs.COM_LoadFile( (char *)name.c_str(), 5, &iLength );
	if ( pFile )
	{
		gEngfuncs.COM_FreeFile( pFile );
		if ( iLength > 0 )
			sprite.hSprite = SPR_Load( name.c_str() );
	}

	s_Sprites.push_back( sprite );
	return sprite.hSprite;
}

//=============================================================================
// ParticleSystemManager
//=============================================================================

ParticleSystemManager::ParticleSystemManager( void )
{
	m_pFirstSystem = NULL;
}

ParticleSystemManager::~ParticleSystemManager( void )
{
	ClearSystems();
}

void ParticleSystemManager::AddSystem( ParticleSystem *pNewSystem )
{
	pNewSystem->m_pNextSystem = m_pFirstSystem;
	m_pFirstSystem = pNewSystem;
}

ParticleSystem *ParticleSystemManager::FindSystem( int iId )
{
	if ( !iId )
		return NULL;	// one-shot copies have no id

	for ( ParticleSystem *pSys = m_pFirstSystem; pSys; pSys = pSys->m_pNextSystem )
	{
		if ( pSys->m_iId == iId )
			return pSys;
	}
	return NULL;
}

void ParticleSystemManager::RemoveSystem( ParticleSystem *pSystem )
{
	ParticleSystem *pLast = NULL;
	for ( ParticleSystem *pSys = m_pFirstSystem; pSys; pLast = pSys, pSys = pSys->m_pNextSystem )
	{
		if ( pSys != pSystem )
			continue;

		if ( pLast )
			pLast->m_pNextSystem = pSys->m_pNextSystem;
		else
			m_pFirstSystem = pSys->m_pNextSystem;
		delete pSys;
		return;
	}
}

int ParticleSystemManager::CountOneShots( void )
{
	int iCount = 0;
	for ( ParticleSystem *pSys = m_pFirstSystem; pSys; pSys = pSys->m_pNextSystem )
	{
		if ( pSys->m_bOneShot )
			iCount++;
	}
	return iCount;
}

ParticleSystem *ParticleSystemManager::OldestOneShot( void )
{
	ParticleSystem *pOldest = NULL;
	for ( ParticleSystem *pSys = m_pFirstSystem; pSys; pSys = pSys->m_pNextSystem )
	{
		if ( pSys->m_bOneShot && ( !pOldest || pSys->m_flStopTime < pOldest->m_flStopTime ) )
			pOldest = pSys;
	}
	return pOldest;
}

static bool Aurora_FartherFirst( const ParticleSystem *a, const ParticleSystem *b )
{
	return a->m_fViewerDist > b->m_fViewerDist;
}

// blended particles don't use the z-buffer, so the systems are drawn farthest first (Spirit had this commented
// out). The particles of one system are drawn in the order they were made.
void ParticleSystemManager::SortSystems( const Vector &vecView )
{
	static std::vector<ParticleSystem *> s_List;
	s_List.clear();

	for ( ParticleSystem *pSys = m_pFirstSystem; pSys; pSys = pSys->m_pNextSystem )
	{
		pSys->CalculateDistance( vecView );
		s_List.push_back( pSys );
	}

	if ( s_List.size() < 2 )
		return;

	std::stable_sort( s_List.begin(), s_List.end(), Aurora_FartherFirst );

	for ( size_t i = 0; i + 1 < s_List.size(); i++ )
		s_List[i]->m_pNextSystem = s_List[i + 1];
	s_List.back()->m_pNextSystem = NULL;
	m_pFirstSystem = s_List[0];
}

void ParticleSystemManager::UpdateSystems( float frametime )
{
	ParticleSystem *pSystem = m_pFirstSystem;
	ParticleSystem *pLast = NULL;

	while ( pSystem )
	{
		if ( pSystem->UpdateSystem( frametime ) )
		{
			pLast = pSystem;
			pSystem = pSystem->m_pNextSystem;
		}
		else // delete this system (a one-shot copy that is done)
		{
			ParticleSystem *pNext = pSystem->m_pNextSystem;
			if ( pLast )
				pLast->m_pNextSystem = pNext;
			else
				m_pFirstSystem = pNext;
			delete pSystem;
			pSystem = pNext;
		}
	}
}

void ParticleSystemManager::DrawSystems( const Vector &vecView, const Vector &forward, const Vector &right, const Vector &up )
{
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );

	for ( ParticleSystem *pSystem = m_pFirstSystem; pSystem; pSystem = pSystem->m_pNextSystem )
		pSystem->DrawSystem( vecView, forward, right, up );

	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
}

void ParticleSystemManager::ClearSystems( void )
{
	ParticleSystem *pSystem = m_pFirstSystem;

	while ( pSystem )
	{
		ParticleSystem *pTemp = pSystem->m_pNextSystem;
		delete pSystem;
		pSystem = pTemp;
	}

	m_pFirstSystem = NULL;
}

//=============================================================================
// The "Aurora" message (aurora_shared.h)
//=============================================================================

// a script path the client will open: relative, inside the game directory
static bool Aurora_ValidScript( const char *sz )
{
	if ( !sz[0] || strlen( sz ) >= AURORA_MAX_SCRIPT )
		return false;
	if ( sz[0] == '/' || sz[0] == '\\' || strchr( sz, ':' ) || strstr( sz, ".." ) )
		return false;
	return true;
}

static ParticleSystem *Aurora_Build( int iId, const char *szScript )
{
	AuroraScript *pScript = Aurora_GetScript( szScript );

	g_bAuroraQuiet = pScript->bBuilt;
	ParticleSystem *pSystem = new ParticleSystem( iId, szScript, pScript->bFound ? pScript->text.c_str() : NULL );
	g_bAuroraQuiet = false;
	pScript->bBuilt = true;

	return pSystem;
}

static int __MsgFunc_Aurora( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	const int iFlags = READ_BYTE();
	const int iId = READ_SHORT();

	if ( iFlags & AURORA_MSG_REMOVE )
	{
		ParticleSystem *pSystem = g_ParticleSystems.FindSystem( iId );
		if ( pSystem )
			g_ParticleSystems.RemoveSystem( pSystem );
		return 1;
	}

	AuroraPlacement placement;
	placement.origin.x = READ_FLOAT();
	placement.origin.y = READ_FLOAT();
	placement.origin.z = READ_FLOAT();
	placement.angles.x = READ_COORD();
	placement.angles.y = READ_COORD();
	placement.angles.z = READ_COORD();
	placement.iTarget = 0;
	placement.iAttachment = -1;

	if ( iFlags & AURORA_MSG_FOLLOW )
	{
		placement.iTarget = READ_SHORT();
		placement.iAttachment = READ_CHAR();
		if ( placement.iTarget < 0 )
			placement.iTarget = 0;
	}

	const char *szScript = READ_STRING();
	if ( !Aurora_ValidScript( szScript ) )
		return 1;

	const bool bOn = ( iFlags & AURORA_MSG_ON ) != 0;

	if ( iFlags & AURORA_MSG_ONESHOT )
	{
		if ( g_ParticleSystems.CountOneShots() >= AURORA_MAX_ONESHOTS )
			g_ParticleSystems.RemoveSystem( g_ParticleSystems.OldestOneShot() );

		ParticleSystem *pSystem = Aurora_Build( 0, szScript );
		pSystem->SetPlacement( placement );
		pSystem->StartOneShot( gEngfuncs.GetClientTime() );
		g_ParticleSystems.AddSystem( pSystem );
		return 1;
	}

	if ( !iId )
		return 1;

	// a system the client has already (the server sends them again on a demo start or "fullupdate", and on every
	// switch): the same script keeps its particles
	ParticleSystem *pSystem = g_ParticleSystems.FindSystem( iId );
	if ( pSystem && stricmp( pSystem->m_szFile, szScript ) )
	{
		g_ParticleSystems.RemoveSystem( pSystem );
		pSystem = NULL;
	}

	if ( !pSystem )
	{
		pSystem = Aurora_Build( iId, szScript );
		g_ParticleSystems.AddSystem( pSystem );
	}

	pSystem->SetPlacement( placement );
	pSystem->SetOn( bOn );

	return 1;
}

//=============================================================================
// Hooks
//=============================================================================

void Aurora_Init( void )
{
	gEngfuncs.pfnHookUserMsg( "Aurora", __MsgFunc_Aurora );
}

void Aurora_VidInit( void )
{
	g_ParticleSystems.ClearSystems();
	s_Scripts.clear();
	s_Sprites.clear();
}

void Aurora_Reset( void )
{
	g_ParticleSystems.ClearSystems();
}

void Aurora_DrawFrame( void )
{
	static float s_flLastTime = 0;

	const float flTime = gEngfuncs.GetClientTime();
	float flFrameTime = flTime - s_flLastTime;
	s_flLastTime = flTime;

	// the clock went back (new map, demo) or stood (paused): no step; a long frame is one tenth of a second
	if ( flFrameTime < 0 )
		flFrameTime = 0;
	else if ( flFrameTime > 0.1f )
		flFrameTime = 0.1f;

	AuroraEditor_Frame();	// the particle editor's preview (built and placed here: the view is known)

	if ( !g_ParticleSystems.m_pFirstSystem )
		return;

	// the software renderer: not drawn (as the weather); the systems wait
	if ( !HUD_IsHardware() )
		return;

	Vector vecAngles, forward, right, up;
	HUD_GetEffectAngles( vecAngles );
	AngleVectors( vecAngles, forward, right, up );
	const Vector vecView = v_origin;

	g_ParticleSystems.UpdateSystems( flFrameTime );
	g_ParticleSystems.SortSystems( vecView );
	g_ParticleSystems.DrawSystems( vecView, forward, right, up );
}
