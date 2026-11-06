//========= SBO VGUI2 port: menus, SBO side =========//
//
// Purpose: SBO world half of the VGUI2 menus (docs/dev/VGUI2_MIGRATION.md):
//          * the menu-stack entries of the VGUI2 menus (sbo_menu_proxy.h)
//          * the sbo_bridge.h functions the VGUI2 menus use (VGUI1 text scheme, screen size, HideTopMenu, files,
//            the briefing's choices, the spectator panel's actions and command menus)
//
// Compiled with SBO's normal client include paths (SDK 2.3). Must not include any Source SDK header.
//=================================================================//

#include "hud.h"
#include "cl_util.h"
#include "viewport.h"
#include "spectator_panel.h"

#include <stdlib.h>
#include <string.h>

#include "sbo_bridge.h"
#include "sbo_menu_proxy.h"
#include "sbo_server_menu.h"
#include "vgui2_api.h"

namespace
{
char *CopyString( const char *psz )
{
	size_t len = strlen( psz ) + 1;
	char *pCopy = (char *)malloc( len );
	if ( pCopy )
		memcpy( pCopy, psz, len );
	return pCopy;
}

//-----------------------------------------------------------------------------
// A menu on the viewport's menu stack: its content is a VGUI2 panel, shown when the menu becomes the top one
// (Open) and hidden when it leaves the stack (Close).
//-----------------------------------------------------------------------------
class CVGui2MenuProxy : public CViewportMenu
{
public:
	virtual void Open( void )
	{
		CViewportMenu::Open();
		ShowVGui2();
	}

	virtual void Close( void )
	{
		HideVGui2();
		CViewportMenu::Close();
	}

protected:
	virtual void ShowVGui2() = 0;
	virtual void HideVGui2() = 0;
};
// A text window (MOTD, map briefing, help). One per CreateTextWindow call, never deleted (like the SDK's panel:
// HideTopMenu still uses the menu after Close()); the text copies are freed on Close.
class CVGui2TextWindowProxy : public CVGui2MenuProxy
{
public:
	CVGui2TextWindowProxy( const char *szText, const char *szTitle, int iShadeFullscreen, const char *szHtmlFile,
		bool bMOTD )
	{
		m_pszText = CopyString( szText ? szText : "" );
		m_pszTitle = CopyString( szTitle ? szTitle : "" );
		m_pszHtmlFile = szHtmlFile ? CopyString( szHtmlFile ) : NULL;
		m_bShadeFullscreen = iShadeFullscreen != 0;
		m_bMOTD = bMOTD;
	}

protected:
	virtual void ShowVGui2()
	{
		// SBO 2.0: the map briefing opened on its own (MENU_MAPBRIEFING: the spectator Options menu's entry) is only
		// read, its OK just closes it. This menu's own ID (ShowVGUIMenu sets it before Open), so a queued window
		// keeps its own behavior
		VGui2_TextWindow_Show( m_pszTitle ? m_pszTitle : "", m_pszText ? m_pszText : "", m_bShadeFullscreen,
			GetMenuID() == MENU_MAPBRIEFING, m_pszHtmlFile, m_bMOTD );
	}

	virtual void HideVGui2()
	{
		VGui2_TextWindow_Hide();

		free( m_pszText );
		free( m_pszTitle );
		free( m_pszHtmlFile );
		m_pszText = m_pszTitle = m_pszHtmlFile = NULL;
	}

private:
	char *m_pszText;
	char *m_pszTitle;
	char *m_pszHtmlFile; //!< the briefing's .htm (web page when HTML is on), or NULL
	bool m_bShadeFullscreen;
	bool m_bMOTD;
};

// CGmVgui (gmvgui.cpp)
class CVGui2GmMenuProxy : public CVGui2MenuProxy
{
protected:
	virtual void ShowVGui2() { VGui2_GmMenu_Show(); }
	virtual void HideVGui2() { VGui2_GmMenu_Hide(); }
};

// CTeamMenuPanel (vgui_teammenu.cpp). TeamFortressViewport::SlotInput / KeyInput hand the digit keys (and "+jump"
// as 5) to the current menu, i.e. to this proxy.
class CVGui2TeamMenuProxy : public CVGui2MenuProxy
{
public:
	virtual bool SlotInput( int iSlot ) { return VGui2_TeamMenu_SlotInput( iSlot ); }

protected:
	virtual void ShowVGui2() { VGui2_TeamMenu_Show(); }
	virtual void HideVGui2() { VGui2_TeamMenu_Hide(); }
};

// CSyndBrief (vgui_SyndBrief.cpp), the briefing / equipment menu. VGUI1 takes no keys; the new loadout menu
// (cl_new_ingame_menus 1) takes 1-7 for its slots.
class CVGui2BriefMenuProxy : public CVGui2MenuProxy
{
public:
	virtual bool SlotInput( int iSlot ) { return VGui2_BriefMenu_SlotInput( iSlot ); }

protected:
	virtual void ShowVGui2() { VGui2_BriefMenu_Show(); }
	virtual void HideVGui2() { VGui2_BriefMenu_Hide(); }
};

// A plugin's server menu (sbo_server_menu.cpp); the bridge knows which menu and tells the server when it closes
class CVGui2ServerMenuProxy : public CVGui2MenuProxy
{
public:
	virtual bool SlotInput( int iSlot ) { return VGui2_ServerMenu_SlotInput( iSlot ); }
protected:
	virtual void ShowVGui2() { ServerMenu_ProxyOpen(); }
	virtual void HideVGui2() { ServerMenu_ProxyClose(); }
};

// Like the SDK viewport's own m_pTeamMenu, ...: one menu object per viewport, reused with Reset().
template <class T> CViewportMenu *GetViewportProxy()
{
	static T *s_pProxy = NULL;
	static TeamFortressViewport *s_pViewport = NULL;

	if ( !s_pProxy || s_pViewport != gViewPort )
	{
		s_pProxy = new T();
		s_pViewport = gViewPort;
	}

	s_pProxy->Reset();
	return s_pProxy;
}
} // namespace

CViewportMenu *SboVGui2_CreateTextWindowProxy( const char *szText, const char *szTitle, int iShadeFullscreen,
	const char *szHtmlFile, bool bMOTD )
{
	return new CVGui2TextWindowProxy( szText, szTitle, iShadeFullscreen, szHtmlFile, bMOTD );
}

CViewportMenu *SboVGui2_GetGmMenuProxy()
{
	return GetViewportProxy<CVGui2GmMenuProxy>();
}

CViewportMenu *SboVGui2_GetTeamMenuProxy()
{
	return GetViewportProxy<CVGui2TeamMenuProxy>();
}

CViewportMenu *SboVGui2_GetBriefMenuProxy()
{
	return GetViewportProxy<CVGui2BriefMenuProxy>();
}

CViewportMenu *SboVGui2_GetServerMenuProxy()
{
	return GetViewportProxy<CVGui2ServerMenuProxy>();
}

namespace sbo
{
bool GetTextSchemeInfo( const char *schemeName, TextSchemeInfo &out )
{
	out = TextSchemeInfo();
	if ( !gViewPort )
		return false;

	CSchemeManager *pSchemes = gViewPort->GetSchemeManager();
	SchemeHandle_t h = pSchemes->getSchemeHandle( schemeName );

	const char *fontName = NULL;
	pSchemes->getFontInfo( h, fontName, out.fontSize, out.fontWeight );

	// The largest text scheme (1600_textscheme.txt) was written for 1600x1200; the menus' boxes (XRES/YRES) keep
	// growing with the screen above that, its font sizes did not. Above the screen a file was written for (4:3 of
	// its width) the font grows with the screen too, by the smaller of the two box factors so text still fits
	// its box: 1920x1080 as before, 2560x1440 x1.2, 3840x2160 x1.8.
	const int fileRes = pSchemes->getFileRes();
	if ( fileRes > 0 && out.fontSize > 0 )
	{
		float scale = min( (float)ScreenWidth / fileRes, (float)ScreenHeight / ( fileRes * 3 / 4 ) );
		if ( scale > 1.0f )
			out.fontSize = (int)( out.fontSize * scale + 0.5f );
	}
	strncpy( out.fontName, fontName ? fontName : "", sizeof( out.fontName ) - 1 );
	out.fontName[sizeof( out.fontName ) - 1] = 0;

	// CSchemeManager stores VGUI1 alpha (0 = opaque); hand out the file's convention (255 = opaque)
	int r, g, b, a;
	pSchemes->getFgColor( h, r, g, b, a );
	out.fg[0] = r; out.fg[1] = g; out.fg[2] = b; out.fg[3] = 255 - a;
	pSchemes->getBgColor( h, r, g, b, a );
	out.bg[0] = r; out.bg[1] = g; out.bg[2] = b; out.bg[3] = 255 - a;
	pSchemes->getFgArmedColor( h, r, g, b, a );
	out.armedFg[0] = r; out.armedFg[1] = g; out.armedFg[2] = b; out.armedFg[3] = 255 - a;
	pSchemes->getBgArmedColor( h, r, g, b, a );
	out.armedBg[0] = r; out.armedBg[1] = g; out.armedBg[2] = b; out.armedBg[3] = 255 - a;
	return true;
}

void GetScreenSize( int &wide, int &tall )
{
	wide = ScreenWidth;
	tall = ScreenHeight;
}

void HideTopMenu()
{
	if ( gViewPort )
		gViewPort->HideTopMenu();
}

char *LoadFile( const char *path )
{
	return (char *)gEngfuncs.COM_LoadFile( (char *)path, 5, NULL );
}

void FreeFile( char *pFile )
{
	if ( pFile )
		gEngfuncs.COM_FreeFile( pFile );
}

void HideCommandMenu()
{
	if ( gViewPort )
		gViewPort->HideCommandMenu();
}

namespace
{
int *BriefChoiceField( int choice )
{
	if ( !gViewPort )
		return NULL;

	// CSyndBrief's constructor: the defaults, once per viewport
	if ( gViewPort->m_iSBOInit != 1 )
	{
		gViewPort->m_iBlade = 1;
		gViewPort->m_iLight = 1;
		gViewPort->m_iHeavy = 1;
		gViewPort->m_iItem = 1;
		gViewPort->m_iCyberware = 1;
		gViewPort->m_iArmor = 1;
		gViewPort->m_iMaleFemale = 1;
		gViewPort->m_iSBOInit = 1;
	}

	switch ( choice )
	{
	case BRIEF_BLADE:		return &gViewPort->m_iBlade;
	case BRIEF_LIGHT:		return &gViewPort->m_iLight;
	case BRIEF_HEAVY:		return &gViewPort->m_iHeavy;
	case BRIEF_ITEM:		return &gViewPort->m_iItem;
	case BRIEF_CYBERWARE:	return &gViewPort->m_iCyberware;
	case BRIEF_ARMOR:		return &gViewPort->m_iArmor;
	case BRIEF_MALEFEMALE:	return &gViewPort->m_iMaleFemale;
	default:				return NULL;
	}
}
} // namespace

int GetBriefChoice( int choice )
{
	int *pField = BriefChoiceField( choice );
	return pField ? *pField : 1;
}

void SetBriefChoice( int choice, int value )
{
	int *pField = BriefChoiceField( choice );
	if ( pField )
		*pField = value;
}

//-----------------------------------------------------------------------------
// Spectator UI (sbo_spectator_panel.cpp)
//-----------------------------------------------------------------------------
void SpectatorPanelAction( int action )
{
	if ( gViewPort && gViewPort->m_pSpectatorPanel )
		gViewPort->m_pSpectatorPanel->ActionSignal( action );
}

//-----------------------------------------------------------------------------
// HUD text menu (sbo_hud_menu.cpp)
//-----------------------------------------------------------------------------
bool HudMenuShouldDraw()
{
	if ( !KillFeedShouldDraw() )
		return false;

	// CHudMenu::Draw: don't draw the menu if the scoreboard is being shown
	return !( gViewPort && gViewPort->IsScoreBoardVisible() );
}

bool IsHudScaled()
{
	return gHUD.ShouldScale();
}

float GetHudScale()
{
	return HUD_GetScale();
}

int HudToScreenX( int x )
{
	return HUD_ToScreenX( x );
}
} // namespace sbo
