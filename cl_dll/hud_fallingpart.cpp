//
// hud_fallingpart.cpp
//
// SBO: implementation of CHudFallingPart class (falling particles around the player)
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

DECLARE_MESSAGE(m_FallingPart, FallingPart)

int CHudFallingPart::Init( void )
{
	HOOK_MESSAGE( FallingPart );

	m_bActive = false;

	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;

	return 1;
}

int CHudFallingPart::VidInit( void )
{
	m_List.Clear();
	m_pPart = NULL;

	return 1;
}

int CHudFallingPart::MsgFunc_FallingPart( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iNumParticules = READ_SHORT();
	m_iSprite = READ_SHORT();
	m_iSpeed = READ_SHORT();
	m_iHeight = READ_SHORT();
	m_iParam5 = READ_BYTE();

	m_bActive = true;

	m_pPart = new CFallingPart;
	m_pPart->SetEmetteur( m_iNumParticules, m_iSpeed, m_iSprite, m_iHeight, m_iParam5 );
	m_List.Ajouter( m_pPart );

	return 1;
}

int CHudFallingPart::Draw( float flTime )
{
	return 1;
}

CHudFallingPart::~CHudFallingPart()
{
}
