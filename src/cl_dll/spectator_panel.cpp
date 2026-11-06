//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// spectator_panel.cpp: implementation of the SpectatorPanel class.
//
//////////////////////////////////////////////////////////////////////

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "pm_shared.h"
#include "viewport.h"
#include "spectator_panel.h"
#include "vgui2/vgui2_api.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SpectatorPanel::SpectatorPanel()
{
	m_bVisible = false;
	m_bBannerVisible = false;
	m_menuVisible = false;
	m_insetVisible = false;
	m_iBottomColor[0] = m_iBottomColor[1] = m_iBottomColor[2] = m_iBottomColor[3] = 0;
	m_szVGui2BottomText[0] = 0;
	m_szVGui2ExtraInfo[0] = 0;
	strcpy( m_szVGui2CurrentTime, "00:00" );
	m_szVGui2CamText[0] = 0;
	m_szVGui2OptionsText[0] = 0;
	strcpy( m_szVGui2Banner, "banner" );
}

SpectatorPanel::~SpectatorPanel()
{

}

void SpectatorPanel::ActionSignal(int cmd)
{
	switch (cmd)
	{
		case SPECTATOR_PANEL_CMD_NONE :			break;

		case SPECTATOR_PANEL_CMD_OPTIONS :		gViewPort->ShowCommandMenu( gViewPort->m_SpectatorOptionsMenu );
												break;

		case SPECTATOR_PANEL_CMD_NEXTPLAYER :	gHUD.m_Spectator.FindNextPlayer(true);
												break;

		case SPECTATOR_PANEL_CMD_PREVPLAYER :	gHUD.m_Spectator.FindNextPlayer(false);
												break;

		case SPECTATOR_PANEL_CMD_HIDEMENU	:	ShowMenu(false); 
												break;

		case SPECTATOR_PANEL_CMD_CAMERA :		gViewPort->ShowCommandMenu( gViewPort->m_SpectatorCameraMenu );
												break;
		
		case SPECTATOR_PANEL_CMD_TOGGLE_INSET : gHUD.m_Spectator.SetModes( -1, 
													gHUD.m_Spectator.ToggleInset(false) );
												break;
		

		default : 	gEngfuncs.Con_DPrintf("Unknown SpectatorPanel ActionSingal %i.\n",cmd); break;
	}

}


void SpectatorPanel::Initialize()
{
	// the bottom label starts in the text scheme's colour (VGUI1: setFgColor( Scheme::sc_primary1 ), which the
	// viewport set to "Primary Button Text"'s FgColor); UpdateSpectatorPanel sets the player / GUI colour
	CSchemeManager * pSchemes = gViewPort->GetSchemeManager();
	pSchemes->getFgColor( pSchemes->getSchemeHandle( "Primary Button Text" ), m_iBottomColor[0], m_iBottomColor[1], m_iBottomColor[2], m_iBottomColor[3] );

	strncpy( m_szVGui2OptionsText, CHudTextMessage::BufferedLocaliseTextString( "#SPECT_OPTIONS" ), sizeof( m_szVGui2OptionsText ) - 1 );
	m_szVGui2OptionsText[ sizeof( m_szVGui2OptionsText ) - 1 ] = 0;
	strncpy( m_szVGui2CamText, CHudTextMessage::BufferedLocaliseTextString( "#CAM_OPTIONS" ), sizeof( m_szVGui2CamText ) - 1 );
	m_szVGui2CamText[ sizeof( m_szVGui2CamText ) - 1 ] = 0;

	m_menuVisible = false;
	m_insetVisible = false;
	m_bBannerVisible = false;
}

void SpectatorPanel::SetBottomColor( int r, int g, int b, int a )
{
	m_iBottomColor[0] = r;
	m_iBottomColor[1] = g;
	m_iBottomColor[2] = b;
	m_iBottomColor[3] = a;
}

void SpectatorPanel::ShowMenu(bool isVisible)
{
	// (the VGUI2 panel shows / hides and disarms the Options, camera, < and > buttons and moves the bottom label)

	if ( !isVisible )
	{
		gViewPort->HideCommandMenu();

		// if switching from visible menu to invisible menu, show help text
		if ( m_menuVisible && this->isVisible() )
		{
			char string[ 64 ];

			_snprintf( string, sizeof( string ) - 1, "%c%s", HUD_PRINTCENTER, CHudTextMessage::BufferedLocaliseTextString( "#Spec_Duck" ) );
			string[ sizeof( string ) - 1 ] = '\0';

			gHUD.m_TextMessage.MsgFunc_TextMsg( NULL, strlen( string ) + 1, string );
		}
	}

	m_menuVisible = isVisible;

	gViewPort->UpdateCursorState();

	SyncVGui2();
}


const char *GetSpectatorLabel ( int iMode )
{
	switch ( iMode )
	{
		case OBS_CHASE_LOCKED:
			return "#OBS_CHASE_LOCKED";

		case OBS_CHASE_FREE:
			return "#OBS_CHASE_FREE";

		case OBS_ROAMING:
			return "#OBS_ROAMING";
		
		case OBS_IN_EYE:
			return "#OBS_IN_EYE";

		case OBS_MAP_FREE:
			return "#OBS_MAP_FREE";

		case OBS_MAP_CHASE:
			return "#OBS_MAP_CHASE";

		case OBS_NONE:
		default:
			return "#OBS_NONE";
	}

	return "";
}

void SpectatorPanel::EnableInsetView(bool isEnabled)
{
	// the VGUI2 panel lays the top bar and the inset frame out (m_insetVisible, the overview's inset window);
	// show the banner only in real HLTV mode
	m_bBannerVisible = gEngfuncs.IsSpectateOnly() != 0;

	m_insetVisible = isEnabled;

	Update();

	strncpy( m_szVGui2CamText, CHudTextMessage::BufferedLocaliseTextString( GetSpectatorLabel( g_iUser1 ) ), sizeof( m_szVGui2CamText ) - 1 );
	m_szVGui2CamText[ sizeof( m_szVGui2CamText ) - 1 ] = 0;
	SyncVGui2();
}




void SpectatorPanel::Update()
{
	// the status (map name / spectator count, timer, separator; spec_drawstatus) is laid out by the VGUI2 panel
	SyncVGui2();
}

//-----------------------------------------------------------------------------
// The VGUI2 panel (vgui2/sbo_spectator_panel.cpp, docs/dev/VGUI2_MIGRATION.md) draws the spectator bars and menus;
// it gets its state from here.
//-----------------------------------------------------------------------------
void SpectatorPanel::SyncVGui2()
{
	SboSpectatorState state;
	state.visible = this->isVisible();
	state.menuVisible = m_menuVisible;
	state.insetVisible = m_insetVisible;
	state.insetX = gHUD.m_Spectator.m_OverviewData.insetWindowX;
	state.insetY = gHUD.m_Spectator.m_OverviewData.insetWindowY;
	state.insetWide = gHUD.m_Spectator.m_OverviewData.insetWindowWidth;
	state.insetTall = gHUD.m_Spectator.m_OverviewData.insetWindowHeight;
	state.statusVisible = gHUD.m_Spectator.m_drawstatus && gHUD.m_Spectator.m_drawstatus->value != 0;
	state.bannerVisible = m_bBannerVisible;
	state.bannerImage = m_szVGui2Banner;
	state.optionsText = m_szVGui2OptionsText;
	state.camText = m_szVGui2CamText;
	state.bottomText = m_szVGui2BottomText;

	state.bottomColor[0] = m_iBottomColor[0];
	state.bottomColor[1] = m_iBottomColor[1];
	state.bottomColor[2] = m_iBottomColor[2];
	state.bottomColor[3] = 255 - m_iBottomColor[3];	// VGUI1 alpha (0 = opaque) -> 255 = opaque

	state.extraInfo = m_szVGui2ExtraInfo;
	state.currentTime = m_szVGui2CurrentTime;

	VGui2_Spectator_SetState( state );
}
