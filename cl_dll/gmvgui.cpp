//
// gmvgui.cpp
//
// SBO: CGmVgui, the "GM Mode" information window (text from vgui\gm.txt)
//

#include "VGUI_Font.h"
#include "VGUI_TextPanel.h"
#include "VGUI_LineBorder.h"

#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "const.h"

#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"

CGmVgui::CGmVgui( int iTrans, int iRemoveMe, int x, int y, int wide, int tall ) : CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();

	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hBriefingText = pSchemes->getSchemeHandle( "Briefing Text" );

	Font *pTitleFont = pSchemes->getFont( hTitleScheme );
	Font *pTextFont = pSchemes->getFont( hBriefingText );

	int r, g, b, a;

	m_pBackgroundPanel = new CTransparentPanel( 125, XRES(145), YRES(115), XRES(350), YRES(250) );
	m_pBackgroundPanel->setParent( this );
	m_pBackgroundPanel->setBorder( new LineBorder( Color( 41, 107, 34, 0 ) ) );

	m_pTitle = new Label( "", XRES(130), YRES(5) );
	m_pTitle->setFont( pTitleFont );
	m_pTitle->setParent( m_pBackgroundPanel );
	pSchemes->getFgColor( hBriefingText, r, g, b, a );
	m_pTitle->setFgColor( 41, 107, 34, 0 );
	pSchemes->getBgColor( hBriefingText, r, g, b, a );
	m_pTitle->setBgColor( r, g, b, a );
	m_pTitle->setContentAlignment( vgui::Label::a_west );
	m_pTitle->setText( "GM Mode" );

	m_pReturnButton = new CommandButton( "", XRES(137.5), YRES(205), XRES(80), YRES(20) );
	m_pReturnButton->setContentAlignment( vgui::Label::a_west );
	m_pReturnButton->setParent( m_pBackgroundPanel );
	m_pReturnButton->setText( " Return" );
	m_pReturnButton->addActionSignal( new CMenuHandler_SendServeur( "gm", true ) );
	m_pReturnButton->addActionSignal( new CMenuHandler_EnleveHighLight( m_pReturnButton ) );

	m_pTextPanel = new TextPanel( "", XRES(10), YRES(60), XRES(630), YRES(120) );
	m_pTextPanel->setParent( m_pBackgroundPanel );
	pSchemes->getBgColor( hTitleScheme, r, g, b, a );
	m_pTextPanel->setBgColor( r, g, b, a );
	m_pTextPanel->setVisible( true );

	char *pfile = NULL;
	pfile = (char*)gEngfuncs.COM_LoadFile( "vgui\\gm.txt", 5, NULL );
	if ( !pfile )
		return;

	m_pTextPanel->setText( pfile );
}

void CGmVgui::Reset( void )
{
	CMenuPanel::Reset();
}

void CGmVgui::Initialize( void )
{
}
