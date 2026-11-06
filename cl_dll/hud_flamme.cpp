//
// hud_flamme.cpp
//
// SBO: implementation of CHudFlamme class (map flame emitters)
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

DECLARE_MESSAGE(m_Flamme, FlammePos)
DECLARE_MESSAGE(m_Flamme, PlayerSpawn)
DECLARE_MESSAGE(m_Flamme, StateFlamme)

int CHudFlamme::Init( void )
{
	HOOK_MESSAGE( FlammePos );
	HOOK_MESSAGE( PlayerSpawn );
	HOOK_MESSAGE( StateFlamme );

	m_bActive = false;

	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;

	return 1;
}

int CHudFlamme::VidInit( void )
{
	m_List.Clear();
	m_pEmetteur = NULL;
	VectorClear( m_vecPos );

	return 1;
}

int CHudFlamme::MsgFunc_PlayerSpawn( const char *pszName, int iSize, void *pbuf )
{
	gHUD.m_Flamme.m_List.Clear();
	gHUD.m_Fumee.m_List.Clear();
	gHUD.m_FallingPart.m_List.Clear();

	return 1;
}

int CHudFlamme::MsgFunc_FlammePos( const char *pszName, int iSize, void *pbuf )
{
	int iId;
	int bActive;

	BEGIN_READ( pbuf, iSize );
	m_vecPos.x = READ_COORD();
	m_vecPos.y = READ_COORD();
	m_vecPos.z = READ_COORD();
	m_iNumParticules = READ_SHORT();
	m_iSprite = READ_SHORT();
	m_iSpread = READ_SHORT();
	m_iHeight = READ_SHORT();
	m_iLife = READ_SHORT();
	m_flR = READ_SHORT();
	m_flG = READ_SHORT();
	m_flB = READ_SHORT();
	m_flA = READ_SHORT();
	iId = READ_SHORT();
	bActive = READ_SHORT();

	m_bActive = true;

	m_pEmetteur = new CParticulesEmetteur;
	m_pEmetteur->SetEmetteur( m_iNumParticules, m_iHeight, m_vecPos, m_iLife, m_iSprite, m_iSpread, m_flR, m_flG, m_flB, m_flA, bActive );
	m_pEmetteur->m_iId = iId;
	m_List.Ajouter( m_pEmetteur );

	return 1;
}

int CHudFlamme::MsgFunc_StateFlamme( const char *pszName, int iSize, void *pbuf )
{
	int iId;

	BEGIN_READ( pbuf, iSize );
	iId = READ_BYTE();
	m_List.ChangeState( iId );

	return 1;
}

int CHudFlamme::Draw( float flTime )
{
	return 1;
}

CHudFlamme::~CHudFlamme()
{
}

Vector CHudFlamme::GetPosition( void )
{
	return this->m_vecPos;
}
