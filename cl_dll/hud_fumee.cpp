//
// hud_fumee.cpp
//
// SBO: implementation of CHudFumee class (map smoke emitters)
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

DECLARE_MESSAGE(m_Fumee, FumeePos)
DECLARE_MESSAGE(m_Fumee, StateFumee)

int CHudFumee::Init( void )
{
	HOOK_MESSAGE( FumeePos );
	HOOK_MESSAGE( StateFumee );

	m_bActive = false;

	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE;

	return 1;
}

int CHudFumee::VidInit( void )
{
	m_List.Clear();
	m_pEmit = NULL;
	m_iUnknown20 = 0;
	VectorClear( m_vecPos );

	return 1;
}

int CHudFumee::MsgFunc_FumeePos( const char *pszName, int iSize, void *pbuf )
{
	int bActive;

	BEGIN_READ( pbuf, iSize );
	m_vecPos.x = READ_COORD();
	m_vecPos.y = READ_COORD();
	m_vecPos.z = READ_COORD();
	m_iNumParticules = READ_SHORT();
	m_iSprite = READ_SHORT();
	m_iParam8 = READ_SHORT();
	m_iHeight = READ_SHORT();
	m_iLife = READ_SHORT();
	m_iParam12 = READ_SHORT();
	m_iParam15 = READ_BYTE();
	m_iParam16 = READ_BYTE();
	m_iParam9 = READ_SHORT();
	m_iParam10 = READ_SHORT();
	m_iParam11 = READ_SHORT();
	m_iParam13 = READ_BYTE();
	m_iParam14 = READ_BYTE();
	m_flParam17 = READ_BYTE();
	m_iId = READ_BYTE();
	bActive = READ_BYTE();

	m_bActive = true;

	m_pEmit = new CSmokeEmit;
	m_pEmit->SetEmetteur( m_iNumParticules, m_iHeight, m_vecPos, m_iLife, m_iSprite, m_iParam8, m_iParam9, m_iParam10, m_iParam11, m_iParam12, m_iParam13, m_iParam14, m_iParam15, m_iParam16, m_flParam17, bActive );
	m_pEmit->m_iId = m_iId;
	m_List.Ajouter( m_pEmit );

	return 1;
}

int CHudFumee::MsgFunc_StateFumee( const char *pszName, int iSize, void *pbuf )
{
	int iId;

	BEGIN_READ( pbuf, iSize );
	iId = READ_BYTE();
	m_List.ChangeState( iId );

	return 1;
}

int CHudFumee::Draw( float flTime )
{
	return 1;
}

CHudFumee::~CHudFumee()
{
}

Vector CHudFumee::GetPosition( void )
{
	return this->m_vecPos;
}
