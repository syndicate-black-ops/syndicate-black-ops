//=========== (C) Copyright 1996-2002 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Client DLL VGUI Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "parsemsg.h"
#include "pm_shared.h"
#include "../engine/keydefs.h"
#include "demo.h"
#include "demo_api.h"

#include "viewport.h"
#include "spectator_panel.h"
#include "vgui2/vgui2_api.h"
#include "vgui2/sbo_menu_proxy.h"
#include "vgui2/sbo_command_menu.h"

extern int g_iVisibleMouse;
int g_iPlayerClass;
int g_iTeamNumber;
int g_iUser1;
int g_iUser2;
int g_iUser3;

// The player and team data the scoreboard messages fill (ScoreInfo, TeamInfo, TeamScore, Spectator below). The
// VGUI2 scoreboard, the kill feed, the chat, the spectator panel and the team menu read them. They lived in
// vgui_ScorePanel.cpp with the VGUI1 scoreboard, which SBO 2.0 removed (docs/dev/SCOREBOARD_PORT.md).
hud_player_info_t	 g_PlayerInfoList[MAX_PLAYERS+1];	   // player info from the engine
extra_player_info_t  g_PlayerExtraInfo[MAX_PLAYERS+1];   // additional player info sent directly to the client dll
team_info_t			 g_TeamInfo[MAX_TEAMS+1];
int					 g_IsSpectator[MAX_PLAYERS+1];

void IN_ResetMouse( void );
void IN_ResetRelativeMouseState( void );


// Team Colors
int iNumberOfTeamColors = 5;
int iTeamColors[5][3] =
{
	{ 255, 170, 0 },	// HL orange (default)
	{ 125, 165, 210 },	// Blue
	{ 200, 90, 70 },	// Red
	{ 225, 205, 45 },	// Yellow
	{ 145, 215, 140 },	// Green
};


// class names (CreateTextWindow: the class help texts)
char *sTFClassSelection[] = 
{
	"civilian",
	"scout",
	"sniper",
	"soldier",
	"demoman",
	"medic",
	"hwguy",
	"pyro",
	"spy",
	"engineer",
	"randompc",
	"civilian",
};

//-----------------------------------------------------------------------------
// Purpose: Makes sure the memory allocated for TeamFortressViewport is nulled out
// Input  : stAllocateBlock - 
// Output : void *
//-----------------------------------------------------------------------------
void *TeamFortressViewport::operator new( size_t stAllocateBlock )
{
//	void *mem = Panel::operator new( stAllocateBlock );
	void *mem = ::operator new( stAllocateBlock );
	memset( mem, 0, stAllocateBlock );
	return mem;
}

//================================================================
TeamFortressViewport::TeamFortressViewport(int x,int y,int wide,int tall) : m_SchemeManager(wide,tall)
{
	gViewPort = this;
	m_iNumTeams = 0;
	m_pSpectatorPanel = NULL;
	m_pCurrentMenu = NULL;

	Initialize();

	// (The SDK set the VGUI1 App scheme's fonts and colours from the text scheme here; no VGUI1 widget is left to
	// use them. The VGUI2 widgets read the text scheme itself: sbo::GetTextSchemeInfo.)

	CreateSpectatorMenu();
	// Init command menus (vgui2/sbo_command_menu.cpp)
	sbo::CommandMenu_Reset();
	m_iCurrentTeamNumber = m_iUser1 = m_iUser2 = m_iUser3 = 0;

	m_StandardMenu = sbo::CommandMenu_Create("commandmenu.txt", 0, CMENU_TOP, false, CMENU_SIZE_X, BUTTON_SIZE_Y, 0 );
	m_SpectatorOptionsMenu = sbo::CommandMenu_Create("spectatormenu.txt", 1, YRES(32), true, CMENU_SIZE_X, BUTTON_SIZE_Y / 2, 0 );	// above bottom bar, flat design
	m_SpectatorCameraMenu = sbo::CommandMenu_Create("spectcammenu.txt", 1, YRES(32), true, XRES( 200 ), BUTTON_SIZE_Y / 2, ScreenWidth - ( XRES ( 200 ) + 15 ) );	// above bottom bar, flat design
}

//-----------------------------------------------------------------------------
// Purpose: Called everytime a new level is started. Viewport clears out it's data.
//-----------------------------------------------------------------------------
void TeamFortressViewport::Initialize( void )
{
	// Force each menu to Initialize
	VGui2_TeamMenu_Initialize();
	// Clear out scoreboard data (ScorePanel::Initialize)
	m_iNumTeams = 0;
	memset( g_PlayerExtraInfo, 0, sizeof g_PlayerExtraInfo );
	memset( g_TeamInfo, 0, sizeof g_TeamInfo );
	HideScoreBoard();
	VGui2_Score_Reset();
	if (m_pSpectatorPanel)
	{
		// Spectator menu doesn't need initializing
		m_pSpectatorPanel->setVisible( false );
		m_pSpectatorPanel->SyncVGui2();
	}

	// Make sure all menus are hidden
	HideVGUIMenu();
	HideCommandMenu();

	// Clear out some data
	m_iGotAllMOTD = true;
	m_iRandomPC = false;
	m_flSpectatorPanelLastUpdated = 0;

	// reset player info
	g_iPlayerClass = 0;
	g_iTeamNumber = 0;

	strcpy(m_sMapName, "");
	strcpy(m_szServerName, "");
	for (int i = 0; i < 5; i++)
		m_iValidClasses[i] = 0;
	for (int j = 0; j <= MAX_TEAMNAMES; j++)	// j: VC6 leaks the first loop's i into this scope
		strcpy(m_sTeamNames[j], "");

	// (the SDK set the VGUI1 cursor to "none" here; the cursor is VGUI2's: g_iVisibleMouse, CMenuCursorHelper)
}

//=======================================================================
// The command menus: vgui2/sbo_command_menu.cpp
void TeamFortressViewport::ShowCommandMenu(int menuIndex)
{
	sbo::CommandMenu_Show( menuIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Handles the key input of "-commandmenu"
// Input  : 
//-----------------------------------------------------------------------------
void TeamFortressViewport::InputSignalHideCommandMenu()
{
	sbo::CommandMenu_InputSignalHide();
}

//-----------------------------------------------------------------------------
// Purpose: Hides the command menu
//-----------------------------------------------------------------------------
void TeamFortressViewport::HideCommandMenu()
{
	sbo::CommandMenu_Hide();
}

//-----------------------------------------------------------------------------
// Purpose: Bring up the scoreboard
//-----------------------------------------------------------------------------
void TeamFortressViewport::ShowScoreBoard( void )
{
	// chat-vgui2: the VGUI2 scoreboard (BHL CScorePanel) replaces the VGUI1 one
	if ( VGui2_Score_IsActive() )
	{
		// SBO: also in a one-player session ("maxplayers 1"): every game mode has multiplayer rules (the SDK showed
		// no scoreboard when maxclients is 1)
		VGui2_Score_Show();
		return;
	}
	// SBO 2.0: the VGUI2 scoreboard is the only one. Without it (an engine without client VGUI2, a build without
	// SBO_VGUI2) there is no scoreboard.
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the scoreboard is up
//-----------------------------------------------------------------------------
bool TeamFortressViewport::IsScoreBoardVisible( void )
{
	if ( VGui2_Score_IsActive() )
		return VGui2_Score_IsVisible();

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Hide the scoreboard
//-----------------------------------------------------------------------------
void TeamFortressViewport::HideScoreBoard( void )
{
	// Prevent removal of scoreboard during intermission
	if ( gHUD.m_iIntermission )
		return;

	if ( VGui2_Score_IsActive() )
	{
		VGui2_Score_Hide();
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Activate's the player special ability
//			called when the player hits their "special" key
//-----------------------------------------------------------------------------
void TeamFortressViewport::InputPlayerSpecial( void )
{
	if (!sbo::CommandMenu_IsInitialized())
		return;

	if ( g_iPlayerClass == PC_ENGINEER || g_iPlayerClass == PC_SPY )
	{
		ShowCommandMenu( gViewPort->m_StandardMenu );

		sbo::CommandMenu_KeyInput( '7' );
	}
	else
	{
		// if it's any other class, just send the command down to the server
		ClientCmd( "_special" );
	}
}

void TeamFortressViewport::UpdateCommandMenu(int menuIndex)
{
	sbo::CommandMenu_Update( menuIndex );
}

void COM_FileBase ( const char *in, char *out);

void TeamFortressViewport::UpdateSpectatorPanel()
{
	m_iUser1 = g_iUser1;
	m_iUser2 = g_iUser2;
	m_iUser3 = g_iUser3;
		
	if (!m_pSpectatorPanel)
		return;

	float flNextTimerChange = 0;	// SBO 2.0: seconds until the timer shows another second

	// SBO 2.0 GM mode: a flying Game Master has his own HUD (key hints) instead of the bar (VGui2_GmHudOwnsScreen)
	if ( g_iUser1 && gHUD.m_pCvarDraw->value && !gHUD.m_iIntermission && !VGui2_GmHudOwnsScreen() )	// don't draw in dev_overview mode
	{
		char bottomText[128];
		char helpString2[128];
		char tempString[128];
		char * name;
		int player = 0;

		// check if spectator combinations are still valid
		gHUD.m_Spectator.CheckSettings();

		if ( !m_pSpectatorPanel->isVisible() )
		{
			m_pSpectatorPanel->setVisible( true );	// show spectator panel, but
			m_pSpectatorPanel->ShowMenu( false );	// dsiable all menus/buttons
			
			_snprintf( tempString, sizeof( tempString ) - 1, "%c%s", HUD_PRINTCENTER, CHudTextMessage::BufferedLocaliseTextString( "#Spec_Duck" ) );
			tempString[ sizeof( tempString ) - 1 ] = '\0';

			gHUD.m_TextMessage.MsgFunc_TextMsg( NULL, strlen( tempString ) + 1, tempString );
		}
		
		sprintf(bottomText,"#Spec_Mode%d", g_iUser1 );
		sprintf(helpString2,"#Spec_Mode%d", g_iUser1 );

		if ( gEngfuncs.IsSpectateOnly() )
			strcat(helpString2, " - HLTV");

		// check if we're locked onto a target, show the player's name
		if ( (g_iUser2 > 0) && (g_iUser2 <= gEngfuncs.GetMaxClients()) && (g_iUser1 != OBS_ROAMING) )
		{
			player = g_iUser2;
		}

		// special case in free map and inset off, don't show names
		if ( (g_iUser1 == OBS_MAP_FREE) && !gHUD.m_Spectator.m_pip->value )
			name = NULL;
		else
			name = g_PlayerInfoList[player].name;

		// create player & health string
		if ( player && name )
		{
			// SBO 2.0: the name without its ^0-^9 / ^xRGB / ^#RRGGBB color codes (as hud_colortext 2 shows them; with
			// hud_colortext 0 as typed), the same parser as the chat (GetColorCodeLength)
			strncpy( bottomText, VGui2_Chat_RemoveColorCodes( name ), sizeof( bottomText ) - 1 );
			bottomText[sizeof( bottomText ) - 1] = '\0';
		}

		// SBO 2.0: the bottom label always has the bars' colour. (The SDK showed the name in the player's team colour
		// in first person; in SBO a watched player is always alive, so that was always the "alive" green of the
		// scoreboard and the name changed colour with the camera mode for nothing.)
		m_pSpectatorPanel->SetBottomColor( 143, 143, 54, 0 );

		// add sting auto if we are in auto directed mode
		if ( gHUD.m_Spectator.m_autoDirector->value )
		{
			char tempString[128];
			sprintf(tempString, "#Spec_Auto %s", helpString2);
			strcpy( helpString2, tempString );
		}

		// the texts the VGUI2 panel shows, pushed by m_pSpectatorPanel->Update() below
		strncpy( m_pSpectatorPanel->m_szVGui2BottomText, CHudTextMessage::BufferedLocaliseTextString( bottomText ), sizeof( m_pSpectatorPanel->m_szVGui2BottomText ) - 1 );

		
		// update extra info field
		char szText[64];

		if ( gEngfuncs.IsSpectateOnly() )
		{
			// in HLTV mode show number of spectators
			_snprintf( szText, 63, "%s: %d", CHudTextMessage::BufferedLocaliseTextString( "#Spectators" ), gHUD.m_Spectator.m_iSpectatorNumber );
		}
		else
		{
			// otherwise show map name
			char szMapName[64];
			COM_FileBase( gEngfuncs.pfnGetLevelName(), szMapName );

			_snprintf ( szText, 63, "%s: %s",CHudTextMessage::BufferedLocaliseTextString( "#Spec_Map" ), szMapName );
		}

		szText[63] = 0;

		strncpy( m_pSpectatorPanel->m_szVGui2ExtraInfo, szText, sizeof( m_pSpectatorPanel->m_szVGui2ExtraInfo ) - 1 );
		
		// SBO 2.0: the timer (the SDK's round timer code was commented out, the label kept its "00:00"): the time
		// left before mp_timelimit ends the map, else the time the map has been running (CHud::MsgFunc_RoundTime)
		int timer = gHUD.GetRoundTime( &flNextTimerChange );

		if ( timer >= 60 * 60 )
			_snprintf ( szText, 63, "%02d:%02d:%02d", timer / ( 60 * 60 ), ( timer / 60 ) % 60, timer % 60 );
		else
			_snprintf ( szText, 63, "%02d:%02d", timer / 60, timer % 60 );

		szText[63] = 0;

		strncpy( m_pSpectatorPanel->m_szVGui2CurrentTime, szText, sizeof( m_pSpectatorPanel->m_szVGui2CurrentTime ) - 1 );

		// update spectator panel
		gViewPort->m_pSpectatorPanel->Update();
	}
	else
	{
		if ( m_pSpectatorPanel->isVisible() )
		{
			m_pSpectatorPanel->setVisible( false );
			m_pSpectatorPanel->ShowMenu( false );	// dsiable all menus/buttons
			m_pSpectatorPanel->Update();	// SBO 2.0: the VGUI2 bars follow at once (a Game Master back to flying)
		}
	}

	m_flSpectatorPanelLastUpdated = gHUD.m_flTime + 1.0; // update every seconds

	// SBO 2.0: sooner when the timer's next second comes first, so it ticks on the second
	if ( flNextTimerChange > 0 && flNextTimerChange < 1.0 )
		m_flSpectatorPanelLastUpdated = gHUD.m_flTime + flNextTimerChange;
}

//======================================================================================
// SBO menus
CViewportMenu* TeamFortressViewport::ShowSyndBrief( void )
{
	// the briefing / equipment menu (vgui2/sbo_brief_menu.cpp) through a proxy on the menu stack
	return SboVGui2_GetBriefMenuProxy();
}

CViewportMenu* TeamFortressViewport::ShowGmMenu( void )
{
	// the GM menu (vgui2/sbo_gm_menu.cpp) through a proxy on the menu stack
	return SboVGui2_GetGmMenuProxy();
}


//======================================================================
// Set the VGUI Menu
void TeamFortressViewport::SetCurrentMenu( CViewportMenu *pMenu )
{
	m_pCurrentMenu = pMenu;
	if ( m_pCurrentMenu )
	{
		// Don't open menus in demo playback
		if ( gEngfuncs.pDemoAPI->IsPlayingback() )
			return;

		m_pCurrentMenu->Open();
	}
}

//================================================================
// Text Window
CViewportMenu* TeamFortressViewport::CreateTextWindow( int iTextToShow )
{
	char sz[256];
	char *cText;
	char *pfile = NULL;
	// SBO 2.0: the map briefing as a web page, maps/<map>.htm next to the .txt (docs/HTML_MOTD.md)
	char szHtml[256];
	szHtml[0] = 0;
	static const int MAX_TITLE_LENGTH = 32;
	char cTitle[MAX_TITLE_LENGTH];

	if ( iTextToShow == SHOW_MOTD )
	{
		if (!m_szServerName || !m_szServerName[0])
			strcpy( cTitle, "Half-Life" );
		else
			strncpy( cTitle, m_szServerName, MAX_TITLE_LENGTH );
		cTitle[MAX_TITLE_LENGTH-1] = 0;
		cText = m_szMOTD;
	}
	else if ( iTextToShow == SHOW_MAPBRIEFING )
	{
		// Get the current mapname, and open it's map briefing text
		if (m_sMapName && m_sMapName[0])
		{
			strcpy( sz, "maps/");
			strcat( sz, m_sMapName );
			strcat( sz, ".txt" );
		}
		else
		{
			const char *level = gEngfuncs.pfnGetLevelName();
			if (!level)
				return NULL;

			strcpy( sz, level );
			char *ch = strchr( sz, '.' );
			*ch = '\0';
			strcat( sz, ".txt" );

			// pull out the map name
			strcpy( m_sMapName, level );
			ch = strchr( m_sMapName, '.' );
			if ( ch )
			{
				*ch = 0;
			}

			ch = strchr( m_sMapName, '/' );
			if ( ch )
			{
				// move the string back over the '/'
				memmove( m_sMapName, ch+1, strlen(ch)+1 );
			}
		}

		// sz is "maps/<map>.txt" here: the same name with ".htm" (3 letters: the engine downloads no other)
		strcpy( szHtml, sz );
		char *ext = strrchr( szHtml, '.' );
		if ( ext && !strcmp( ext, ".txt" ) )
			strcpy( ext, ".htm" );
		else
			szHtml[0] = 0;
		if ( szHtml[0] && !VGui2_HtmlFileAvailable( szHtml ) )
			szHtml[0] = 0;

		pfile = (char*)gEngfuncs.COM_LoadFile( sz, 5, NULL );

		if (!pfile && !szHtml[0])
			return NULL;

		cText = pfile ? pfile : (char *)"";

		strncpy( cTitle, m_sMapName, MAX_TITLE_LENGTH );
		cTitle[MAX_TITLE_LENGTH-1] = 0;
	}
	else if ( iTextToShow == SHOW_CLASSDESC )
	{
		switch ( g_iPlayerClass )
		{
		case PC_SCOUT:		cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_scout" ); 
							CHudTextMessage::LocaliseTextString( "#Title_scout", cTitle, MAX_TITLE_LENGTH ); break;
		case PC_SNIPER:		cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_sniper" );
							CHudTextMessage::LocaliseTextString( "#Title_sniper", cTitle, MAX_TITLE_LENGTH ); break;
		case PC_SOLDIER:	cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_soldier" );
							CHudTextMessage::LocaliseTextString( "#Title_soldier", cTitle, MAX_TITLE_LENGTH ); break;
		case PC_DEMOMAN:	cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_demoman" );
							CHudTextMessage::LocaliseTextString( "#Title_demoman", cTitle, MAX_TITLE_LENGTH ); break;
		case PC_MEDIC:		cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_medic" );
							CHudTextMessage::LocaliseTextString( "#Title_medic", cTitle, MAX_TITLE_LENGTH ); break;
		case PC_HVYWEAP:	cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_hwguy" );
							CHudTextMessage::LocaliseTextString( "#Title_hwguy", cTitle, MAX_TITLE_LENGTH ); break;
		case PC_PYRO:		cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_pyro" );
							CHudTextMessage::LocaliseTextString( "#Title_pyro", cTitle, MAX_TITLE_LENGTH ); break;
		case PC_SPY:		cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_spy" );
							CHudTextMessage::LocaliseTextString( "#Title_spy", cTitle, MAX_TITLE_LENGTH ); break;
		case PC_ENGINEER:	cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_engineer" );
							CHudTextMessage::LocaliseTextString( "#Title_engineer", cTitle, MAX_TITLE_LENGTH ); break;
		case PC_CIVILIAN:	cText = CHudTextMessage::BufferedLocaliseTextString( "#Help_civilian" );
							CHudTextMessage::LocaliseTextString( "#Title_civilian", cTitle, MAX_TITLE_LENGTH ); break;
		default:
			return NULL;
		}

		if ( g_iPlayerClass == PC_CIVILIAN )
		{
			sprintf(sz, "classes/long_civilian.txt");
		}
		else
		{
			sprintf(sz, "classes/long_%s.txt", sTFClassSelection[ g_iPlayerClass ]);
		}
		char *pfile = (char*)gEngfuncs.COM_LoadFile( sz, 5, NULL );
		if (pfile)
		{
			cText = pfile;
		}
	}
	else if ( iTextToShow == SHOW_SPECHELP )
	{
		CHudTextMessage::LocaliseTextString( "#Spec_Help_Title", cTitle, MAX_TITLE_LENGTH );
		cTitle[MAX_TITLE_LENGTH-1] = 0;
		
		char *pfile = CHudTextMessage::BufferedLocaliseTextString( "#Spec_Help_Text" );
		if ( pfile )
		{
			cText = pfile;
		}
	}

	// if we're in the game (ie. have selected a class), flag the menu to be only grayed in the dialog box, instead of full screen
	// the text window (vgui2/sbo_text_window.cpp) through a proxy on the menu stack
	CViewportMenu *pMOTDPanel = SboVGui2_CreateTextWindowProxy( cText, cTitle, g_iPlayerClass == PC_UNDEFINED,
		szHtml[0] ? szHtml : NULL, iTextToShow == SHOW_MOTD );

	if ( pfile )
		gEngfuncs.COM_FreeFile( pfile );

	return pMOTDPanel;
}

//================================================================
// VGUI Menus
void TeamFortressViewport::ShowVGUIMenu( int iMenu )
{
	CViewportMenu *pNewMenu = NULL;

	// Don't open menus in demo playback
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		return;

	// Don't open any menus except the MOTD during intermission
	// MOTD needs to be accepted because it's sent down to the client 
	// after map change, before intermission's turned off
	if ( gHUD.m_iIntermission && iMenu != MENU_INTRO )
		return;

	// SBO 2.0: server menus (docs/SERVER_MENUS.md): one is never queued behind another menu (the plugin opens it
	// again), and every other menu takes the screen from it (the client tells the server it closed)
	if ( iMenu == MENU_SERVERMENU && m_pCurrentMenu )
		return;
	if ( iMenu != MENU_SERVERMENU && m_pCurrentMenu && m_pCurrentMenu->GetMenuID() == MENU_SERVERMENU )
		HideTopMenu();

	// Don't create one if it's already in the list
	if (m_pCurrentMenu)
	{
		CViewportMenu *pMenu = m_pCurrentMenu;
		while (pMenu != NULL)
		{
			if (pMenu->GetMenuID() == iMenu)
				return;
			pMenu = pMenu->GetNextMenu();
		}
	}

	switch ( iMenu )
	{
	case MENU_TEAM:		
		pNewMenu = ShowTeamMenu(); 
		break;

	// Map Briefing removed now that it appears in the team menu
	case MENU_MAPBRIEFING:
		pNewMenu = CreateTextWindow( SHOW_MAPBRIEFING );
		break;

	case MENU_INTRO:
		pNewMenu = CreateTextWindow( SHOW_MOTD );
		break;

	case MENU_CLASSHELP:
		pNewMenu = CreateTextWindow( SHOW_CLASSDESC );
		break;

	case MENU_SPECHELP:
		pNewMenu = CreateTextWindow( SHOW_SPECHELP );
		break;

	// SBO menus
	case 9:
		pNewMenu = CreateTextWindow( SHOW_MAPBRIEFING );
		break;

	case 10:
		pNewMenu = ShowSyndBrief();
		break;

	case 19:
		pNewMenu = ShowGmMenu();
		break;

	case MENU_SERVERMENU:
		pNewMenu = SboVGui2_GetServerMenuProxy();
		break;

	default:
		break;
	}

	if (!pNewMenu)
		return;

	// Close the Command Menu if it's open
	HideCommandMenu();

	pNewMenu->SetMenuID( iMenu );
	pNewMenu->SetActive( true );

	// See if another menu is visible, and if so, cache this one for display once the other one's finished
	if (m_pCurrentMenu)
	{
		m_pCurrentMenu->SetNextMenu( pNewMenu );
	}
	else
	{
		m_pCurrentMenu = pNewMenu;
		m_pCurrentMenu->Open();
		UpdateCursorState();
	}
}

// Removes all VGUI Menu's onscreen
void TeamFortressViewport::HideVGUIMenu()
{
	while (m_pCurrentMenu)
	{
		HideTopMenu();
	}
}

// Remove the top VGUI menu, and bring up the next one
void TeamFortressViewport::HideTopMenu()
{
	if (m_pCurrentMenu)
	{
		// Close the top one
		m_pCurrentMenu->Close();

		// Bring up the next one
		gViewPort->SetCurrentMenu( m_pCurrentMenu->GetNextMenu() );
	}

	UpdateCursorState();
}

// Return TRUE if the HUD's allowed to print text messages
bool TeamFortressViewport::AllowedToPrintText( void )
{
	// Prevent text messages when fullscreen menus are up
	if ( m_pCurrentMenu && g_iPlayerClass == 0 )
	{
		int iId = m_pCurrentMenu->GetMenuID();
		if ( iId == MENU_TEAM || iId == MENU_INTRO || iId == MENU_CLASSHELP )
			return FALSE;
	}

	return TRUE;
}

//======================================================================================
// TEAM MENU
//======================================================================================
// Bring up the Team selection Menu
CViewportMenu* TeamFortressViewport::ShowTeamMenu()
{
	// Don't open menus in demo playback
	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
		return NULL;

	// the team menu = SBO's map readme (vgui2/sbo_team_menu.cpp) through a proxy on the menu stack
	return SboVGui2_GetTeamMenuProxy();
}

//======================================================================================
// SPECTATOR MENU
//======================================================================================
// Spectator "Menu" explaining the Spectator buttons
void TeamFortressViewport::CreateSpectatorMenu()
{
	// Create the Panel
	m_pSpectatorPanel = new SpectatorPanel();
	m_pSpectatorPanel->setVisible(false);
	m_pSpectatorPanel->Initialize();
}

//======================================================================================
// UPDATE HUD SECTIONS
//======================================================================================
// We've got an update on player info
// Recalculate any menus that use it.
void TeamFortressViewport::UpdateOnPlayerInfo()
{
	// what the VGUI1 scoreboard's update left for the others: fresh player info and the teams' totals
	GetAllPlayersInfo();
	RecalculateTeamScores();
}

//-----------------------------------------------------------------------------
// Purpose: Recalculate the existing teams in the match (g_TeamInfo; was ScorePanel::RebuildTeams)
//-----------------------------------------------------------------------------
void TeamFortressViewport::RebuildTeams( void )
{
	// clear out player counts from teams
	int i; for ( i = 1; i <= m_iNumTeams; i++ )
	{
		g_TeamInfo[i].players = 0;
	}

	// rebuild the team list
	GetAllPlayersInfo();
	m_iNumTeams = 0;
	for ( i = 1; i < MAX_PLAYERS; i++ )
	{
		if ( g_PlayerInfoList[i].name == NULL )
			continue;

		if ( g_PlayerExtraInfo[i].teamname[0] == 0 )
			continue; // skip over players who are not in a team

		// is this player in an existing team?
		int j; for ( j = 1; j <= m_iNumTeams; j++ )
		{
			if ( g_TeamInfo[j].name[0] == '\0' )
				break;

			if ( !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name ) )
				break;
		}

		if ( j > m_iNumTeams )
		{ // they aren't in a listed team, so make a new one
			// search through for an empty team slot
			for ( j = 1; j <= m_iNumTeams; j++ )
			{
				if ( g_TeamInfo[j].name[0] == '\0' )
					break;
			}

			// SBO 2.0: no slot left (the SDK wrote past g_TeamInfo with more than MAX_TEAMS team names)
			if ( j > MAX_TEAMS )
				continue;

			m_iNumTeams = max( j, m_iNumTeams );

			strncpy( g_TeamInfo[j].name, g_PlayerExtraInfo[i].teamname, MAX_TEAM_NAME );
			g_TeamInfo[j].players = 0;
		}

		g_TeamInfo[j].players++;
	}

	// clear out any empty teams
	for ( i = 1; i <= m_iNumTeams; i++ )
	{
		if ( g_TeamInfo[i].players < 1 )
			memset( &g_TeamInfo[i], 0, sizeof(team_info_t) );
	}

	RecalculateTeamScores();
}

//-----------------------------------------------------------------------------
// Purpose: The teams' totals from their players (the first part of ScorePanel::SortTeams, teamplay only)
//-----------------------------------------------------------------------------
void TeamFortressViewport::RecalculateTeamScores( void )
{
	if ( !gHUD.m_Teamplay )
		return;

	// clear out team scores
	int i; for ( i = 1; i <= m_iNumTeams; i++ )
	{
		if ( !g_TeamInfo[i].scores_overriden )
			g_TeamInfo[i].frags = g_TeamInfo[i].deaths = 0;
		g_TeamInfo[i].ping = g_TeamInfo[i].packetloss = 0;
	}

	// recalc the team scores
	for ( i = 1; i < MAX_PLAYERS; i++ )
	{
		if ( g_PlayerInfoList[i].name == NULL )
			continue; // empty player slot, skip

		if ( g_PlayerExtraInfo[i].teamname[0] == 0 )
			continue; // skip over players who are not in a team

		// find what team this player is in
		int j; for ( j = 1; j <= m_iNumTeams; j++ )
		{
			if ( !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name ) )
				break;
		}
		if ( j > m_iNumTeams )  // player is not in a team, skip to the next guy
			continue;

		if ( !g_TeamInfo[j].scores_overriden )
		{
			g_TeamInfo[j].frags += g_PlayerExtraInfo[i].frags;
			g_TeamInfo[j].deaths += g_PlayerExtraInfo[i].deaths;
		}

		g_TeamInfo[j].ping += g_PlayerInfoList[i].ping;
		g_TeamInfo[j].packetloss += g_PlayerInfoList[i].packetloss;

		if ( g_PlayerInfoList[i].thisplayer )
			g_TeamInfo[j].ownteam = TRUE;
		else
			g_TeamInfo[j].ownteam = FALSE;

		// Set the team's number (used for team colors)
		g_TeamInfo[j].teamnumber = g_PlayerExtraInfo[i].teamnumber;
	}

	// find team ping/packetloss averages
	for ( i = 1; i <= m_iNumTeams; i++ )
	{
		if ( g_TeamInfo[i].players > 0 )
		{
			g_TeamInfo[i].ping /= g_TeamInfo[i].players;  // use the average ping of all the players in the team as the teams ping
			g_TeamInfo[i].packetloss /= g_TeamInfo[i].players;  // use the average ping of all the players in the team as the teams ping
		}
	}
}

void TeamFortressViewport::UpdateCursorState()
{
	// Need cursor if any VGUI window is up
	if ( m_pSpectatorPanel->m_menuVisible || m_pCurrentMenu || GetClientVoiceMgr()->IsInSquelchMode() )
	{
		g_iVisibleMouse = true;
		return;
	}
	else if ( sbo::GetOpenCommandMenu() )
	{
		// commandmenu doesn't have cursor if hud_capturemouse is turned off
		if ( gHUD.m_pCvarStealMouse->value != 0.0f )
		{
			g_iVisibleMouse = true;
			return;
		}
	}

	// Don't reset mouse in demo playback
	if ( !gEngfuncs.pDemoAPI->IsPlayingback() )
	{
		IN_ResetMouse();
	}

	if ( g_iVisibleMouse )
	{
		// HL25: Clear any residual input so our camera doesn't jerk when dismissing the UI
		IN_ResetRelativeMouseState();
	}

	g_iVisibleMouse = false;
}

void TeamFortressViewport::GetAllPlayersInfo( void )
{
	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		GetPlayerInfo( i, &g_PlayerInfoList[i] );
	}
}

// Once per frame (HUD_Frame; was the VGUI1 viewport panel's paintBackground)
void TeamFortressViewport::Frame( void )
{
	// See if the command menu is visible	// See if the command menu is visible and needs recalculating due to some external change
	if ( g_iTeamNumber != m_iCurrentTeamNumber )
	{
		UpdateCommandMenu( m_StandardMenu );

		m_iCurrentTeamNumber = g_iTeamNumber;
	}

	if ( g_iPlayerClass != m_iCurrentPlayerClass )
	{
		UpdateCommandMenu( m_StandardMenu );

		m_iCurrentPlayerClass = g_iPlayerClass;
	}

	// See if the Spectator Menu needs to be update
	if (	( g_iUser1 != m_iUser1 || g_iUser2 != m_iUser2 ) ||
			( m_flSpectatorPanelLastUpdated < gHUD.m_flTime ) )
	{
		UpdateSpectatorPanel();
	}
}

//================================================================
// Number Key Input
bool TeamFortressViewport::SlotInput( int iSlot )
{
	// If there's a menu up, give it the input
	if ( m_pCurrentMenu )
		return m_pCurrentMenu->SlotInput( iSlot );

	return FALSE;
}

// Direct Key Input
int	TeamFortressViewport::KeyInput( int down, int keynum, const char *pszCurrentBinding )
{
	// SBO 2.0: the SDK opened the Team Menu (SBO: the map readme) here when a spectator pressed Enter, whatever the
	// key was bound to (halflife #2316). The key does what the player bound it to; the map readme is the
	// spectator menu's "Map Briefing" (spectatormenu.txt, "!MAPBRIEFING").

	// SBO 2.0 server menus: Shift for Shift+Tab
	static bool s_bShift = false;
	if ( keynum == K_SHIFT )
		s_bShift = down != 0;

	// Open Text Window?
	if (m_pCurrentMenu && gEngfuncs.Con_IsVisible() == false)
	{
		int iMenuID = m_pCurrentMenu->GetMenuID();

		// SBO 2.0: a server menu's keys (docs/SERVER_MENUS.md: Tab and the arrows move, Enter picks, Space flips; its
		// Esc comes through the viewport's menu Esc guard: the engine opens GameUI first); the other keys stay the player's
		if ( iMenuID == MENU_SERVERMENU && down )
		{
			int key = 0;
			switch ( keynum )
			{
			case K_ENTER: case K_KP_ENTER: key = SMKEY_ENTER; break;
			case K_SPACE: key = SMKEY_SPACE; break;
			case K_TAB: key = s_bShift ? SMKEY_BACKTAB : SMKEY_TAB; break;
			case K_UPARROW: key = SMKEY_UP; break;
			case K_DOWNARROW: key = SMKEY_DOWN; break;
			case K_LEFTARROW: key = SMKEY_LEFT; break;
			case K_RIGHTARROW: key = SMKEY_RIGHT; break;
			}
			if ( key && VGui2_ServerMenu_KeyInput( key ) )
				return 0;
		}

		// Get number keys as Input for the Team menu
		if (iMenuID == MENU_TEAM)
		{
			// Escape gets you out of the Team menu if the Cancel button is visible
			if ( keynum == K_ESCAPE )
			{
				if ( g_iTeamNumber )
				{
					HideTopMenu();
					return 0;
				}
			}

			for (int i = '0'; i <= '9'; i++)
			{
				if ( down && (keynum == i) )
				{
					SlotInput( i - '0' );
					return 0;
				}
			}
		}

		// Grab enter keys to close TextWindows
		if ( down && (keynum == K_ENTER || keynum == K_KP_ENTER || keynum == K_SPACE || keynum == K_ESCAPE) )
		{
			if ( iMenuID == MENU_MAPBRIEFING || iMenuID == MENU_INTRO || iMenuID == MENU_CLASSHELP )
			{
				HideTopMenu();
				return 0;
			}
		}

		// Grab jump key on Team Menu as autoassign
		if ( pszCurrentBinding && down && !strcmp(pszCurrentBinding, "+jump") )
		{
			if (iMenuID == MENU_TEAM)
			{
				// the current menu: the VGUI2 team menu's proxy
				m_pCurrentMenu->SlotInput(5);
				return 0;
			}
		}

	}

	// A click with another mouse button than the left one closes the command menu. (The SDK's VGUI1 viewport panel
	// did this for a click that landed on it, i.e. not on a menu: CViewPortInputHandler. Without VGUI1 the click is
	// this key event; a click on a VGUI2 panel doesn't come here.)
	if ( down && ( keynum == K_MOUSE2 || keynum == K_MOUSE3 ) && sbo::GetOpenCommandMenu() )
	{
		// send a message to close the command menu
		// this needs to be a message, since a direct call screws the timing
		gEngfuncs.pfnClientCmd( "ForceCloseCommandMenu\n" );
	}

	// if we're in a command menu, try hit one of it's buttons
	if ( down && sbo::GetOpenCommandMenu() )
	{
		// Escape hides the command menu
		if ( keynum == K_ESCAPE )
		{
			HideCommandMenu();
			return 0;
		}

		// only trap the number keys
		if ( keynum >= '0' && keynum <= '9' )
		{
			if ( sbo::CommandMenu_KeyInput(keynum) )
			{
				// a final command has been issued, so close the command menu
				HideCommandMenu();
			}

			return 0;
		}
	}

	return 1;
}

//================================================================
// Message Handlers
int TeamFortressViewport::MsgFunc_ValClass(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	
	for (int i = 0; i < 5; i++)
		m_iValidClasses[i] = READ_SHORT();

	// Force the menu to update
	UpdateCommandMenu( m_StandardMenu );

	return 1;
}

int TeamFortressViewport::MsgFunc_TeamNames(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	
	m_iNumberOfTeams = READ_BYTE();

	for (int i = 0; i < m_iNumberOfTeams; i++)
	{
		int teamNum = i + 1;

		// chat-vgui2: labels past MAX_TEAMNAMES are read and dropped (was an overflow of m_sTeamNames)
		if ( teamNum > MAX_TEAMNAMES )
		{
			READ_STRING();
			continue;
		}

		gHUD.m_TextMessage.LocaliseTextString( READ_STRING(), m_sTeamNames[teamNum], MAX_TEAMNAME_SIZE );
	}

	VGui2_Score_UpdateOnPlayerInfo( 0 );	// 0: labels/category order changed -> rebuild the sections if shown

	return 1;
}

int TeamFortressViewport::MsgFunc_Feign(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	
	m_iIsFeigning = READ_BYTE();

	// Force the menu to update
	UpdateCommandMenu( m_StandardMenu );

	return 1;
}

int TeamFortressViewport::MsgFunc_Detpack(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iIsSettingDetpack = READ_BYTE();

	// Force the menu to update
	UpdateCommandMenu( m_StandardMenu );

	return 1;
}

int TeamFortressViewport::MsgFunc_VGUIMenu(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iMenu = READ_BYTE();

	// Map briefing includes the name of the map (because it's sent down before the client knows what map it is)
	if (iMenu == MENU_MAPBRIEFING)
	{
		strncpy( m_sMapName, READ_STRING(), sizeof(m_sMapName) );
		m_sMapName[ sizeof(m_sMapName) - 1 ] = '\0';
	}

	// Bring up the menu6
	ShowVGUIMenu( iMenu );

	return 1;
}

int TeamFortressViewport::MsgFunc_MOTD( const char *pszName, int iSize, void *pbuf )
{
	if (m_iGotAllMOTD)
		m_szMOTD[0] = 0;

	BEGIN_READ( pbuf, iSize );

	m_iGotAllMOTD = READ_BYTE();

	int roomInArray = sizeof(m_szMOTD) - strlen(m_szMOTD) - 1;

	strncat( m_szMOTD, READ_STRING(), roomInArray >= 0 ? roomInArray : 0 );
	m_szMOTD[ sizeof(m_szMOTD)-1 ] = '\0';

	// don't show MOTD for HLTV spectators
	if ( m_iGotAllMOTD && !gEngfuncs.IsSpectateOnly() )
	{
		ShowVGUIMenu( MENU_INTRO );
	}

	return 1;
}

int TeamFortressViewport::MsgFunc_BuildSt( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iBuildState = READ_BYTE();

	// Force the menu to update
	UpdateCommandMenu( m_StandardMenu );

	return 1;
}

int TeamFortressViewport::MsgFunc_RandomPC( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iRandomPC = READ_BYTE();

	return 1;
}

int TeamFortressViewport::MsgFunc_ServerName( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	strncpy( m_szServerName, READ_STRING(), MAX_SERVERNAME_LENGTH );

	return 1;
}

int TeamFortressViewport::MsgFunc_ScoreInfo( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	short cl = READ_BYTE();
	short frags = READ_SHORT();
	short deaths = READ_SHORT();
	short playerclass = READ_SHORT();
	short teamnumber = READ_SHORT();

	if ( cl > 0 && cl <= MAX_PLAYERS )
	{
		g_PlayerExtraInfo[cl].frags = frags;
		g_PlayerExtraInfo[cl].deaths = deaths;
		g_PlayerExtraInfo[cl].playerclass = playerclass;
		g_PlayerExtraInfo[cl].teamnumber = teamnumber;

		//Dont go bellow 0!
		if ( g_PlayerExtraInfo[cl].teamnumber < 0 )
			 g_PlayerExtraInfo[cl].teamnumber = 0;

		UpdateOnPlayerInfo();
		VGui2_Score_UpdateOnPlayerInfo( cl );
	}

	return 1;
}

// Message handler for TeamScore message
// accepts three values:
//		string: team name
//		short: teams kills
//		short: teams deaths 
// if this message is never received, then scores will simply be the combined totals of the players.
int TeamFortressViewport::MsgFunc_TeamScore( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	char *TeamName = READ_STRING();

	// find the team matching the name
	int i; for ( i = 1; i <= m_iNumTeams; i++ )
	{
		if ( !stricmp( TeamName, g_TeamInfo[i].name ) )
			break;
	}

	if ( i > m_iNumTeams )
		return 1;

	// use this new score data instead of combined player scoresw
	g_TeamInfo[i].scores_overriden = TRUE;
	g_TeamInfo[i].frags = READ_SHORT();
	g_TeamInfo[i].deaths = READ_SHORT();

	return 1;
}

// Message handler for TeamInfo message
// accepts two values:
//		byte: client number
//		string: client team name
int TeamFortressViewport::MsgFunc_TeamInfo( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	short cl = READ_BYTE();
	
	if ( cl > 0 && cl <= MAX_PLAYERS )
	{  
		// set the players team
		strncpy( g_PlayerExtraInfo[cl].teamname, READ_STRING(), MAX_TEAM_NAME );
	}

	// rebuild the list of teams
	RebuildTeams();

	if ( cl > 0 && cl <= MAX_PLAYERS )
		VGui2_Score_UpdateOnPlayerInfo( cl );

	return 1;
}

void TeamFortressViewport::DeathMsg( int killer, int victim )
{
	VGui2_Score_DeathMsg( killer, victim );
}

int TeamFortressViewport::MsgFunc_Spectator( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	short cl = READ_BYTE();
	if ( cl > 0 && cl <= MAX_PLAYERS )
	{
		g_IsSpectator[cl] = READ_BYTE();
	}

	return 1;
}

int TeamFortressViewport::MsgFunc_AllowSpec( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iAllowSpectators = READ_BYTE();

	// Force the menu to update
	UpdateCommandMenu( m_StandardMenu );

	return 1;
}
