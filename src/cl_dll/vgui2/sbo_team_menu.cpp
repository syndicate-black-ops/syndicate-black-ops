//========= SBO VGUI2 port: team menu (map readme) =========//
//
// Purpose: see sbo_team_menu.h.
//=================================================================//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vgui/ISurface.h>

#include "sbo_bridge.h"
#include "sbo_html_panel.h"
#include "sbo_menus.h"
#include "sbo_team_menu.h"
#include "vgui2_api.h"

// vgui_teammenu.cpp
#define TEAMMENU_TITLE_X            sboui::XRes(40)
#define TEAMMENU_TITLE_Y            sboui::YRes(32)
#define TEAMMENU_TOPLEFT_BUTTON_X   sboui::XRes(40)
#define TEAMMENU_TOPLEFT_BUTTON_Y   sboui::YRes(80)
#define TEAMMENU_BUTTON_SIZE_X      sboui::XRes(124)
#define TEAMMENU_BUTTON_SIZE_Y      sboui::YRes(24)
#define TEAMMENU_WINDOW_X           sboui::XRes(176)
#define TEAMMENU_WINDOW_Y           sboui::YRes(80)
#define TEAMMENU_WINDOW_SIZE_X      sboui::XRes(424)
#define TEAMMENU_WINDOW_SIZE_Y      sboui::YRes(312)
#define TEAMMENU_WINDOW_TITLE_X     sboui::XRes(16)
#define TEAMMENU_WINDOW_TITLE_Y     sboui::YRes(16)
#define TEAMMENU_WINDOW_TEXT_X      sboui::XRes(16)
#define TEAMMENU_WINDOW_TEXT_Y      sboui::YRes(48)
#define TEAMMENU_WINDOW_TEXT_SIZE_Y sboui::YRes(178) // VGUI1 text height (the VGUI2 text uses the whole window)

CSboTeamMenu *CSboTeamMenu::s_pInstance = nullptr;

CSboTeamMenu *CSboTeamMenu::Create(vgui2::Panel *pParent)
{
	if (!s_pInstance)
		s_pInstance = new CSboTeamMenu(pParent);
	return s_pInstance;
}

CSboTeamMenu::CSboTeamMenu(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboTeamMenu")
{
	SetBackdropTrans(100); // CTeamMenuPanel( 100, ... )

	wchar_t wbuf[256];

	// "Map Briefing" in "Title Font" with that scheme's colours
	m_pTitle = new CSboLabel(this, "Title", "Title Font", sboui::SBO_GREEN);
	m_pTitle->UseTextSchemeFgColor();
	sboui::Localize("#SBO_TeamMenu_Title", wbuf, sizeof(wbuf));
	m_pTitle->SetText(wbuf);

	// the info window: CTransparentPanel( 255 ) (no background) with SBO's green LineBorder
	m_pTeamWindow = new CSboTransparentPanel(this, "TeamWindow", 255);
	m_pTeamWindow->SetLineBorder(sboui::SBO_GREEN);

	m_pMapTitle = new CSboLabel(m_pTeamWindow, "MapTitle", "Title Font", sboui::SBO_GREEN);
	m_pMapTitle->UseTextSchemeFgColor();

	// CTFScrollPanel with setScrollBarVisible( false, false ): the text can't be scrolled, the panel only clips it
	m_pScrollPanel = new CSboTransparentPanel(m_pTeamWindow, "ScrollPanel", 0);

	m_pBriefing = new CSboTextPanel(m_pScrollPanel, "Briefing", "Briefing Text", sboui::SBO_GREEN, true);
	m_pBriefing->UseTextSchemeFgColor();

	// button 5, "Go!": CommandButton( ..., bNoHighlight = true ) armed by CHandler_MenuButtonOver (stays armed)
	m_pGoButton = new CSboCommandButton(this, "GoButton", this, "go");
	m_pGoButton->SetStickyArmed(true);
	sboui::Localize("#SBO_TeamMenu_Go", wbuf, sizeof(wbuf));
	m_pGoButton->SetCommandText(wbuf);

	m_pScrollText = new CSboScrollText(m_pScrollPanel);
	m_pScrollText->SetVisible(false);
}

void CSboTeamMenu::Initialize()
{
	m_bUpdatedMapName = false;
	m_szHtmlFile[0] = 0;
}

void CSboTeamMenu::UpdateHtml()
{
	// the client's one page panel comes into this window (sbo_html_panel.h)
	bool bHtml = false;
	if (m_bUpdatedMapName && VGui2_HtmlFileAvailable(m_szHtmlFile))
	{
		const bool bHadIt = CSboHtmlPanel::GetIfOwnedBy(m_pScrollPanel) != nullptr;
		CSboHtmlPanel *pHtml = CSboHtmlPanel::Acquire(m_pScrollPanel);
		if (pHtml)
		{
			if (!bHadIt)
				LayoutMenu();
			bHtml = pHtml->OpenGameFile(m_szHtmlFile);
		}
	}
	if (!bHtml)
		CSboHtmlPanel::Release(m_pScrollPanel);
	m_bHtml = bHtml;

	m_pBriefing->SetVisible(!bHtml && !m_bNewLook);
	m_pScrollText->SetVisible(!bHtml && m_bNewLook);
	if (CSboHtmlPanel *pHtml = CSboHtmlPanel::GetIfOwnedBy(m_pScrollPanel))
	{
		pHtml->SetVisible(bHtml);
		// links, scroll bars and the page's mouse wheel; the windows around it need mouse input too (VGUI2 looks
		// for mouse targets only inside panels that have it), set by ShowMenu before the panel moved in
		if (bHtml)
		{
			m_pTeamWindow->SetMouseInputEnabled(true);
			m_pScrollPanel->SetMouseInputEnabled(true);
			pHtml->SetMouseInputEnabled(true);
		}
	}
}

void CSboTeamMenu::Close()
{
	HideMenu();
	CSboHtmlPanel::Release(m_pScrollPanel);
	m_bHtml = false;
}

void CSboTeamMenu::UpdateMapInfo()
{
	if (m_bUpdatedMapName)
		return;

	// gEngfuncs.pfnGetLevelName(): "maps/<map>.bsp"
	const char *level = sbo::GetLevelName();
	if (!level || !level[0])
		return;

	char sz[256];
	strncpy(sz, level, sizeof(sz) - 1);
	sz[sizeof(sz) - 1] = 0;

	// map name: after the first '/' (or '\'), up to the first '.'
	char szTitle[256];
	const char *start = strchr(sz, '/');
	if (!start)
		start = strchr(sz, '\\');
	strncpy(szTitle, start ? start + 1 : sz, sizeof(szTitle) - 1);
	szTitle[sizeof(szTitle) - 1] = 0;
	char *ch = strchr(szTitle, '.');
	if (ch)
		*ch = 0;
	m_pMapTitle->SetPlainText(szTitle);
	m_wMapName = sbolook::Widen(szTitle);

	// the map's text: the level path with ".txt" instead of ".bsp"
	ch = strchr(sz, '.');
	if (ch)
		*ch = 0;
	// SBO 2.0: the web page version next to it, ".htm" (3 letters: the engine downloads no other)
	snprintf(m_szHtmlFile, sizeof(m_szHtmlFile), "%s.htm", sz);
	strncat(sz, ".txt", sizeof(sz) - strlen(sz) - 1);

	char *pfile = sbo::LoadFile(sz);
	if (pfile)
	{
		size_t len = strlen(pfile) + 1;
		wchar_t *wtext = (wchar_t *)malloc(len * sizeof(wchar_t));
		if (wtext)
		{
			sboui::TextToWide(pfile, wtext, (int)(len * sizeof(wchar_t)));
			m_pBriefing->SetWideText(wtext);
			m_wBriefing = wtext;
			free(wtext);
		}
		sbo::FreeFile(pfile);
	}

	m_bUpdatedMapName = true;
}

void CSboTeamMenu::Open()
{
	// "No description for this map" until a map's text is loaded. VGUI1 set it only in the constructor, so a map
	// without a .txt file showed the previous map's text.
	if (!m_bUpdatedMapName)
	{
		wchar_t wbuf[256];
		sboui::Localize("#SBO_TeamMenu_NoDescription", wbuf, sizeof(wbuf));
		m_pBriefing->SetWideText(wbuf);
		m_wBriefing = wbuf;
	}
	m_bNewLook = SboNewIngameMenus();

	// CTeamMenuPanel::Open: Update, then show
	UpdateMapInfo();
	ShowMenu();
	UpdateHtml();
}

void CSboTeamMenu::LayoutMenu()
{
	m_pTitle->SetVisible(!m_bNewLook);
	m_pMapTitle->SetVisible(!m_bNewLook);
	m_pGoButton->SetVisible(!m_bNewLook);
	m_pTeamWindow->SetLineBorder(m_bNewLook ? Color(0, 0, 0, 0) : sboui::SBO_GREEN);
	if (m_bNewLook)
	{
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		SboFrame_Layout(m_Frame, sw, sh);
		const int *c = m_Frame.content;
		m_pTeamWindow->SetBounds(c[0], c[1], c[2], c[3]);
		m_pScrollPanel->SetBounds(0, 0, c[2], c[3]);
		if (CSboHtmlPanel *pHtml = CSboHtmlPanel::GetIfOwnedBy(m_pScrollPanel))
			pHtml->SetBounds(0, 0, c[2], c[3]);
		m_pBriefing->SetVisible(false);
		m_pScrollText->SetBounds(0, 0, c[2], c[3]); // the full width: the scroll bar at the right edge
		m_pScrollText->SetVisible(!m_bHtml);
		m_pScrollText->SetContent(SboReflowMapText(m_wBriefing), SboFrame_TextFont(m_Frame), Color(184, 198, 184, 255),
		    std::max(2, (int)(4 * m_Frame.k)), std::max(6, (int)(8 * m_Frame.k)));
		return;
	}
	m_pScrollText->SetVisible(false);
	m_pTitle->SetPos(TEAMMENU_TITLE_X, TEAMMENU_TITLE_Y);
	m_pTitle->SizeToContents();

	m_pTeamWindow->SetBounds(TEAMMENU_WINDOW_X, TEAMMENU_WINDOW_Y, TEAMMENU_WINDOW_SIZE_X, TEAMMENU_WINDOW_SIZE_Y);

	m_pMapTitle->SetPos(TEAMMENU_WINDOW_TITLE_X, TEAMMENU_WINDOW_TITLE_Y);
	m_pMapTitle->SizeToContents();

	// VGUI1: TEAMMENU_WINDOW_TEXT_SIZE_Y (178) tall, leaving the window's lower part empty. The text uses the whole
	// window down to a 16-unit margin (the Go! button is outside the window).
	const int textTall = TEAMMENU_WINDOW_SIZE_Y - TEAMMENU_WINDOW_TEXT_Y - sboui::YRes(16);
	m_pScrollPanel->SetBounds(TEAMMENU_WINDOW_TEXT_X, TEAMMENU_WINDOW_TEXT_Y,
	    TEAMMENU_WINDOW_SIZE_X - (TEAMMENU_WINDOW_TEXT_X * 2), textTall);

	// VGUI1: TextPanel( "", 0, 0, TEAMMENU_WINDOW_SIZE_X - TEAMMENU_WINDOW_TEXT_X, ... ), wider than its scroll panel
	// (right edge clipped) and taller texts cut off. Here the text fits the scroll panel and scrolls (scroll bar when
	// needed, mouse wheel).
	m_pBriefing->SetBounds(0, 0, TEAMMENU_WINDOW_SIZE_X - (TEAMMENU_WINDOW_TEXT_X * 2), textTall);
	if (CSboHtmlPanel *pHtml = CSboHtmlPanel::GetIfOwnedBy(m_pScrollPanel))
		pHtml->SetBounds(0, 0, TEAMMENU_WINDOW_SIZE_X - (TEAMMENU_WINDOW_TEXT_X * 2), textTall);

	// CTeamMenuPanel::Update: the team buttons 1-4 are hidden, so Go! moves to the first button position
	m_pGoButton->SetBounds(TEAMMENU_TOPLEFT_BUTTON_X, TEAMMENU_TOPLEFT_BUTTON_Y, TEAMMENU_BUTTON_SIZE_X,
	    TEAMMENU_BUTTON_SIZE_Y);
}

void CSboTeamMenu::OnThink()
{
	BaseClass::OnThink();

	// the spectator "Map Briefing" or the MOTD took the page panel while this window is still up: the text instead
	if (m_bHtml && !CSboHtmlPanel::GetIfOwnedBy(m_pScrollPanel))
	{
		m_bHtml = false;
		m_pBriefing->SetVisible(!m_bNewLook);
		m_pScrollText->SetVisible(m_bNewLook);
	}
	if (m_bNewLook && IsVisible())
		Repaint(); // the buttons' hover

	// CTeamMenuPanel::paintBackground: get the map's text up as soon as the level name is known
	if (IsVisible() && !m_bUpdatedMapName)
	{
		UpdateMapInfo();
		if (m_bUpdatedMapName)
		{
			LayoutMenu();
			UpdateHtml();
		}
	}
}

bool CSboTeamMenu::SlotInput(int iSlot)
{
	// Go!
	if (iSlot == 5)
	{
		OnCommand("go");
		return true;
	}

	// Spectate: hidden, but VGUI1 fires its action signal anyway (CMenuHandler_StringCommand( "spectate", true ))
	if (iSlot == 6)
	{
		sbo::ExecClientCmd("spectate");
		sbo::HideTopMenu();
		return true;
	}

	// the team buttons 1-4 are never visible
	return false;
}

void CSboTeamMenu::Paint()
{
	BaseClass::Paint();
	if (m_bNewLook)
		SboFrame_Paint(this, m_Frame, L"Map briefing \x00b7 " + m_wMapName, L"Go!");
}

void CSboTeamMenu::OnMousePressed(vgui2::MouseCode code)
{
	if (!m_bNewLook || code != vgui2::MOUSE_LEFT)
	{
		BaseClass::OnMousePressed(code);
		return;
	}
	int mx, my;
	vgui2::input()->GetCursorPos(mx, my);
	ScreenToLocal(mx, my);
	// Go! (the close button too: the loadout menu comes next either way, as in the old menu)
	if (SboFrame_Hit(m_Frame, mx, my))
		OnCommand("go");
}

void CSboTeamMenu::OnCommand(const char *command)
{
	if (!stricmp(command, "go"))
	{
		// CMenuHandler_SendServeur( "ShowVGUI 10", true ): the briefing, and close this menu
		sbo::ExecClientCmd("ShowVGUI 10");
		sbo::HideTopMenu();
		return;
	}

	BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
void VGui2_TeamMenu_Show()
{
	if (CSboTeamMenu::Get())
		CSboTeamMenu::Get()->Open();
}

void VGui2_TeamMenu_Hide()
{
	if (CSboTeamMenu::Get())
		CSboTeamMenu::Get()->Close();
}

bool VGui2_TeamMenu_SlotInput(int iSlot)
{
	return CSboTeamMenu::Get() && CSboTeamMenu::Get()->SlotInput(iSlot);
}

void VGui2_TeamMenu_Initialize()
{
	if (CSboTeamMenu::Get())
		CSboTeamMenu::Get()->Initialize();
}
