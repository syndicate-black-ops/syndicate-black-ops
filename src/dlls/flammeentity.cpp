//
// flammeentity.cpp
//
// SBO: map flame emitter (server side of the client's CHudFlamme, hud_flamme.cpp)
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"

extern int gmsgStateFlamme;

int gFUniqueId = 0;

class CFlammeEntity : public CBaseEntity
{
public:
	void Spawn( void );
	Vector GetPosition( void );
	void GetInformation( int &iParticules, int &iHauteur, int &iLargeur, int &iSize, int &iVitesse, int &iR, int &iG, int &iB, int &iOpacite, int &iState );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int GetUniqueId( void );

	int m_iParticules;
	int m_iHauteur;
	int m_iLargeur;
	int m_iSize;
	int m_iVitesse;
	int m_iR;
	int m_iG;
	int m_iB;
	int m_iOpacite;
	int m_iId;
	int m_iState;
};

LINK_ENTITY_TO_CLASS( Flamme_particules, CFlammeEntity );

void CFlammeEntity::Spawn( void )
{
	m_iId = GetUniqueId();
}

Vector CFlammeEntity::GetPosition( void )
{
	return pev->origin;
}

void CFlammeEntity::GetInformation( int &iParticules, int &iHauteur, int &iLargeur, int &iSize, int &iVitesse, int &iR, int &iG, int &iB, int &iOpacite, int &iState )
{
	iParticules = m_iParticules;
	iHauteur = m_iHauteur;
	iLargeur = m_iLargeur;
	iSize = m_iSize;
	iVitesse = m_iVitesse;
	iR = m_iR;
	iG = m_iG;
	iB = m_iB;
	iOpacite = m_iOpacite;
	iState = m_iState;
}

void CFlammeEntity::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "iParticules"))
	{
		m_iParticules = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "iHauteur"))
	{
		m_iHauteur = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "iLargeur"))
	{
		m_iLargeur = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "iSize"))
	{
		m_iSize = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "iVitesse"))
	{
		m_iVitesse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "FireColor"))
	{
		int color[4];
		UTIL_StringToIntArray( color, 4, pkvd->szValue );
		m_iR = color[0];
		m_iG = color[1];
		m_iB = color[2];
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "Opacite"))
	{
		m_iOpacite = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "state"))
	{
		m_iState = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}

	CBaseEntity::KeyValue( pkvd );
}

void CFlammeEntity::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgStateFlamme );
		WRITE_BYTE( m_iId );
	MESSAGE_END();

	m_iState ^= 1;
}

int CFlammeEntity::GetUniqueId( void )
{
	return gFUniqueId++;
}
