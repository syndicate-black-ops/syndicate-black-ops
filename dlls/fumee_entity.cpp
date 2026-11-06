//
// fumee_entity.cpp
//
// SBO: map smoke emitter (server side of the client's CHudFumee, hud_fumee.cpp)
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"

extern int gmsgStateFumee;

int gUniqueId = 0;

class CFumeeEntity : public CBaseEntity
{
public:
	Vector GetPosition( void );
	void GetInformation( int &iParticules, int &iHauteur, int &iLargeur, int &iSize, int &iVitesse, int &iMaxSize, int &iFrontWind, int &iSideWind, int &iR, int &iB, int &iG, int &bRalentissement, int &iMouvement, int &iBrightness, int &iState );
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int GetUniqueId( void );

	int m_iParticules;
	int m_iHauteur;
	int m_iLargeur;
	int m_iSize;
	int m_iVitesse;
	int m_iMaxSize;
	int m_iFrontWind;
	int m_iSideWind;
	int m_iR;
	int m_iG;
	int m_iB;
	int m_iBrightness;
	int m_bRalentissement;
	int m_iMouvement;
	int m_iId;
	int m_iState;
};

LINK_ENTITY_TO_CLASS( Fumee_particules, CFumeeEntity );

Vector CFumeeEntity::GetPosition( void )
{
	return pev->origin;
}

void CFumeeEntity::GetInformation( int &iParticules, int &iHauteur, int &iLargeur, int &iSize, int &iVitesse, int &iMaxSize, int &iFrontWind, int &iSideWind, int &iR, int &iB, int &iG, int &bRalentissement, int &iMouvement, int &iBrightness, int &iState )
{
	iParticules = m_iParticules;
	iHauteur = m_iHauteur;
	iLargeur = m_iLargeur;
	iSize = m_iSize;
	iVitesse = m_iVitesse;
	iMaxSize = m_iMaxSize;
	iFrontWind = m_iFrontWind;
	iSideWind = m_iSideWind;
	bRalentissement = m_bRalentissement;
	iMouvement = m_iMouvement;
	iR = m_iR;
	iB = m_iB;
	iG = m_iG;
	iBrightness = m_iBrightness;
	iState = m_iState;
}

void CFumeeEntity::Spawn( void )
{
	m_iId = GetUniqueId();
}

void CFumeeEntity::KeyValue( KeyValueData *pkvd )
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
	else if (FStrEq(pkvd->szKeyName, "iMaxSize"))
	{
		m_iMaxSize = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "iFrontWind"))
	{
		m_iFrontWind = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "iSideWind"))
	{
		m_iSideWind = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "SmokeColor"))
	{
		int color[4];
		UTIL_StringToIntArray( color, 4, pkvd->szValue );
		m_iR = color[0];
		m_iG = color[1];
		m_iB = color[2];
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "bRalentissement"))
	{
		m_bRalentissement = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "Mouvement"))
	{
		m_iMouvement = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "iBrightness"))
	{
		m_iBrightness = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "state"))
	{
		m_iState = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}

	CBaseEntity::KeyValue( pkvd );
}

void CFumeeEntity::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgStateFumee );
		WRITE_BYTE( m_iId );
	MESSAGE_END();

	m_iState ^= 1;
}

int CFumeeEntity::GetUniqueId( void )
{
	return gUniqueId++;
}
