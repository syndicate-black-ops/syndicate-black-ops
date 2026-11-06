//
// meteoentity.cpp
//
// SBO: weather particle entity (rain/snow)
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"

class CMeteoEntity : public CBaseEntity
{
public:
	void GetInformation( int &iParticules, int &iSize, int &iVitesse, int &iHauteur, int &iType );
	void KeyValue( KeyValueData *pkvd );

	int m_iParticules;
	int m_iSize;
	int m_iVitesse;
	int m_iHauteur;
	int m_iType;
};

LINK_ENTITY_TO_CLASS( meteo_particules, CMeteoEntity );

void CMeteoEntity::GetInformation( int &iParticules, int &iSize, int &iVitesse, int &iHauteur, int &iType )
{
	iParticules = m_iParticules;
	iSize = m_iSize;
	iVitesse = m_iVitesse;
	iHauteur = m_iHauteur;
	iType = m_iType;
}

void CMeteoEntity::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "iParticules"))
	{
		m_iParticules = atoi(pkvd->szValue);
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
	else if (FStrEq(pkvd->szKeyName, "iHauteur"))
	{
		m_iHauteur = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "iType"))
	{
		m_iType = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}

	CBaseEntity::KeyValue( pkvd );
}
