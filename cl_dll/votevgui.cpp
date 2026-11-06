//
// votevgui.cpp
//
// SBO: CVoteVgui, the map vote window ("Reload map?", vote_yes / vote_no),
// and CMusicRemote, an unused Winamp remote control.
//

#include <windows.h>

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

CVoteVgui::CVoteVgui( int iTrans, int iRemoveMe, int x, int y, int wide, int tall ) : CMenuPanel( iTrans, iRemoveMe, x, y, wide, tall )
{
	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();

	SchemeHandle_t hTitleScheme = pSchemes->getSchemeHandle( "Title Font" );
	SchemeHandle_t hBriefingText = pSchemes->getSchemeHandle( "Briefing Text" );

	Font *pTitleFont = pSchemes->getFont( hTitleScheme );
	Font *pTextFont = pSchemes->getFont( hBriefingText );

	int r, g, b, a;

	m_pBackgroundPanel = new CTransparentPanel( 125, XRES(185), YRES(172), XRES(270), YRES(135) );
	m_pBackgroundPanel->setParent( this );
	m_pBackgroundPanel->setBorder( new LineBorder( Color( 41, 107, 34, 0 ) ) );

	m_pTitle = new Label( "", XRES(85), YRES(5) );
	m_pTitle->setFont( pTitleFont );
	m_pTitle->setParent( m_pBackgroundPanel );
	pSchemes->getFgColor( hBriefingText, r, g, b, a );
	m_pTitle->setFgColor( 41, 107, 34, 0 );
	pSchemes->getBgColor( hBriefingText, r, g, b, a );
	m_pTitle->setBgColor( r, g, b, a );
	m_pTitle->setContentAlignment( vgui::Label::a_west );
	m_pTitle->setText( "SBO Winamp" );

	m_pNote = new Label( "", XRES(5), YRES(110) );
	m_pNote->setFont( pTextFont );
	m_pNote->setParent( m_pBackgroundPanel );
	pSchemes->getFgColor( hBriefingText, r, g, b, a );
	m_pNote->setFgColor( r, g, b, a );
	pSchemes->getBgColor( hBriefingText, r, g, b, a );
	m_pNote->setBgColor( r, g, b, a );
	m_pNote->setContentAlignment( vgui::Label::a_west );
	m_pNote->setText( "Note: You only have 10 seconds to vote" );

	m_pYesButton = new CommandButton( "", XRES(52.5), YRES(60), XRES(80), YRES(20) );
	m_pYesButton->setContentAlignment( vgui::Label::a_west );
	m_pYesButton->setParent( m_pBackgroundPanel );
	m_pYesButton->setText( "  Yes" );
	m_pYesButton->addActionSignal( new CMenuHandler_SendServeur( "vote_yes", true ) );
	m_pYesButton->addActionSignal( new CMenuHandler_EnleveHighLight( m_pYesButton ) );

	m_pNoButton = new CommandButton( "", XRES(137.5), YRES(60), XRES(80), YRES(20) );
	m_pNoButton->setContentAlignment( vgui::Label::a_west );
	m_pNoButton->setParent( m_pBackgroundPanel );
	m_pNoButton->setText( "   No" );
	m_pNoButton->addActionSignal( new CMenuHandler_SendServeur( "vote_no", true ) );
	m_pNoButton->addActionSignal( new CMenuHandler_EnleveHighLight( m_pNoButton ) );
}

void CVoteVgui::Initialize( void )
{
}

void CVoteVgui::Reset( void )
{
	CMenuPanel::Reset();
}

void CVoteVgui::Open( void )
{
	setVisible( true );
	m_iOpenTime = gHUD.m_flTime;
	m_pTitle->setText( "Reload map?" );
}

// Called every frame (TeamFortressViewport::UpdateVoteMenu) while the window is visible.
// After 10 seconds the vote defaults to "yes".
void CVoteVgui::Update( void )
{
	m_iCurrentTime = gHUD.m_flTime;
	if ( m_iCurrentTime - m_iOpenTime >= 10 )
		m_pYesButton->fireActionSignal();
}

//=============================================================================
// SBO: Winamp remote control (unused: nothing creates a CMusicRemote). Class and method names are hash-tuned
// stand-ins that put the functions in SBO's incremental-link thunk slots (the original names are unknown).
//=============================================================================
#define WINAMP_PLAY		40045
#define WINAMP_STOP		40047
#define WINAMP_PAUSE	40046
#define WINAMP_NEXT		40048
#define WINAMP_CLOSE	40001

class CMusicRemote
{
public:
	char	*m_pszStatus;
	char	m_szTitle[256];

	CMusicRemote( void );
	void	ProcessCommandType( int iCommand );
	char	*TitreChanson( void );
	bool	IsFoundWindow( void );
};

HWND g_hWinamp;

CMusicRemote::CMusicRemote( void )
{
	g_hWinamp = NULL;
	g_hWinamp = FindWindow( "Winamp v1.x", NULL );
}

void CMusicRemote::ProcessCommandType( int iCommand )
{
	if ( !g_hWinamp )
		return;

	switch ( iCommand )
	{
	case 1:
		PostMessage( g_hWinamp, WM_COMMAND, WINAMP_PLAY, 0 );
		m_pszStatus = "Playing";
		break;
	case 2:
		PostMessage( g_hWinamp, WM_COMMAND, WINAMP_STOP, 0 );
		m_pszStatus = "Stop";
		break;
	case 3:
		PostMessage( g_hWinamp, WM_COMMAND, WINAMP_PAUSE, 0 );
		m_pszStatus = "Pause";
		break;
	case 4:
		PostMessage( g_hWinamp, WM_COMMAND, WINAMP_NEXT, 0 );
		m_pszStatus = "Playing";
		break;
	case 5:
		PostMessage( g_hWinamp, WM_COMMAND, 40144, 0 );
		m_pszStatus = "Playing";
		break;
	case 7:
		PostMessage( g_hWinamp, WM_COMMAND, WINAMP_CLOSE, 0 );
		m_pszStatus = "Stopped";
		break;
	case 6:
		PostMessage( g_hWinamp, WM_USER, 1, 252 );
		break;
	}
}

char *CMusicRemote::TitreChanson( void )
{
	GetWindowText( g_hWinamp, m_szTitle, 255 );
	return m_szTitle;
}

bool CMusicRemote::IsFoundWindow( void )
{
	if ( g_hWinamp )
		return true;

	return false;
}
