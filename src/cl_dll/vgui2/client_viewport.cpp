//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: Minimal VGUI2 viewports (see client_viewport.h).
//          Ported from BugfixedHL-Rebased vgui/client_viewport.cpp and
//          gameui/gameui_viewport.cpp.
//
// SDK world: compiled with hl1_source_sdk include paths.
//=================================================================//

#define TIER2_GAMEUI_INTERNALS // g_pGameConsole (the console is GameUI too: the menu Esc guard leaves it up)
#include <IBaseUI.h>
#include <IEngineVGui.h>
#include <IGameConsole.h>
#include <tier2/tier2.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_loading_screen.h"
#include "sbo_options_window.h"
#include "sbo_create_server.h"
#include "sbo_options.h"
#include "sbo_server_browser.h"
#include "sbo_server_menu.h"

CClientViewport *g_pViewport = nullptr;
CGameUIViewport *g_pGameUIViewport = nullptr;

//-----------------------------------------------------------------------------
// CClientViewport
//-----------------------------------------------------------------------------
CClientViewport::CClientViewport()
    : BaseClass(nullptr, "CClientViewport")
{
	Assert(!g_pViewport);
	g_pViewport = this;

	SetParent(g_pEngineVGui->GetPanel(PANEL_CLIENTDLL));

	SetSize(10, 10); // Quiet "parent not sized yet" spew
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetProportional(true);
	SetPaintBackgroundEnabled(false);

	// BHL loads ui/resource/clientscheme.res here (its HUD fonts and colors).
	// SBO's VGUI2 layer only hosts the chat, which uses chatscheme.res.
	SetScheme(vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/chatscheme.res", "ChatScheme"));
}

void CClientViewport::ActivateClientUI()
{
	// BHL enables mouse input on the viewport here; SBO's viewport has no
	// interactive VGUI2 children besides the chat popup, which manages its own input.
}

void CClientViewport::HideClientUI()
{
}

void CClientViewport::OnThink()
{
	// Fill the whole screen
	int wide, tall;
	int rootWide, rootTall;
	GetSize(wide, tall);
	g_pVGuiPanel->GetSize(GetVParent(), rootWide, rootTall);

	if (wide != rootWide || tall != rootTall)
	{
		SetBounds(0, 0, rootWide, rootTall);
		InvalidateLayout(true, true);
	}

	BaseClass::OnThink();
}

//-----------------------------------------------------------------------------
// CMenuCursorHelper
//-----------------------------------------------------------------------------
CMenuCursorHelper::CMenuCursorHelper(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboMenuCursorHelper")
{
	MakePopup(false, false);
	SetBounds(0, 0, 1, 1);
	SetPaintEnabled(false);
	SetPaintBackgroundEnabled(false);
	SetPaintBorderEnabled(false);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetVisible(false);
}

void CMenuCursorHelper::Update(bool bWantCursor)
{
	if (bWantCursor == m_bActive)
		return;

	m_bActive = bWantCursor;
	SetVisible(bWantCursor);
	SetMouseInputEnabled(bWantCursor);
	g_pVGuiSurface->CalculateMouseVisible();
}

//-----------------------------------------------------------------------------
// CGameUIViewport
//-----------------------------------------------------------------------------
CGameUIViewport::CGameUIViewport()
    : BaseClass(nullptr, "ClientGameUIViewport")
{
	Assert(!g_pGameUIViewport);
	g_pGameUIViewport = this;

	SetParent(g_pEngineVGui->GetPanel(PANEL_GAMEUIDLL));
	// Dialogs under this viewport (Advanced options) use the classic Steam look of the engine's menus; BHL's copy of
	// GameUI's scheme (gameui_viewport.cpp, ui/resource/ClientSourceScheme.res).
	SetScheme(vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/clientsourcescheme.res", "ClientSourceScheme"));
	SetProportional(false);
	SetSize(0, 0);
}

void CGameUIViewport::Frame()
{
	m_iFrameCount++;

	// Show the cursor while SBO's viewport wants it (see CMenuCursorHelper).
	if (!m_pMenuCursor && g_pViewport)
		m_pMenuCursor = new CMenuCursorHelper(g_pViewport);
	if (m_pMenuCursor)
		m_pMenuCursor->Update(sbo::IsMenuCursorVisible());

	VGui2_MenusStyle_Frame(); // before the open hooks: a style change opens windows on the next frame
	VGui2_Options_Frame();
	VGui2_ServerBrowser_Frame();
	VGui2_OptionsWindow_Frame();
	VGui2_CreateServer_Frame();
	VGui2_LoadingScreen_Frame(); // SBO's joining screen while GameUI's loading dialog is up
	VGui2_ServerMenu_Frame(); // server menus: cl_new_ingame_menus changes
}

void CGameUIViewport::PreventEscapeToShow(bool state)
{
	if (state)
	{
		m_bPreventEscape = true;
		m_iDelayedPreventEscapeFrame = 0;
	}
	else
	{
		// PreventEscapeToShow(false) may be called the same frame that ESC was pressed
		// and CGameUIViewport::OnThink won't hide GameUI
		// So the change is delayed by one frame
		m_bPreventEscape = false;
		m_iDelayedPreventEscapeFrame = m_iFrameCount;
	}
}

void CGameUIViewport::SetVideoEscapeGuard(bool state)
{
	m_bVideoEscapeGuard = state;
	m_bVideoEscapePressed = false;
}

void CGameUIViewport::SetMenuEscapeGuard(bool state)
{
	m_bMenuEscapeGuard = state;
	m_bMenuEscapePressed = false;
}

bool CGameUIViewport::TakeMenuEscape()
{
	const bool pressed = m_bMenuEscapePressed;
	m_bMenuEscapePressed = false;
	return pressed;
}

bool CGameUIViewport::TakeVideoEscape()
{
	const bool pressed = m_bVideoEscapePressed;
	m_bVideoEscapePressed = false;
	return pressed;
}

void CGameUIViewport::OnThink()
{
	BaseClass::OnThink();

	// game_playvideo: this panel thinks only while GameUI is shown, so GameUI coming up while a video plays is Esc;
	// the same while a server menu window is up (its Esc closes its popup, dropdown or dialog, else the window). The
	// console opens GameUI too: a server menu leaves it up (the console key is not Esc)
	const bool menuGuard = m_bMenuEscapeGuard && !(g_pGameConsole && g_pGameConsole->IsConsoleVisible());
	if (m_bVideoEscapeGuard || menuGuard)
	{
		const char *levelName = sbo::GetLevelName();
		if (levelName && levelName[0])
		{
			g_pBaseUI->HideGameUI();
			g_pVGuiSurface->CalculateMouseVisible();
			m_bVideoEscapePressed = m_bVideoEscapeGuard;
			m_bMenuEscapePressed = menuGuard;
			return;
		}
	}

	if (m_bPreventEscape || m_iDelayedPreventEscapeFrame == m_iFrameCount)
	{
		const char *levelName = sbo::GetLevelName();

		if (levelName && levelName[0])
		{
			g_pBaseUI->HideGameUI();

			// Hiding GameUI doesn't update the mouse cursor
			g_pVGuiSurface->CalculateMouseVisible();
		}
		else
		{
			// Disconnected from the server while prevent escape is enabled
			// Disable it
			m_bPreventEscape = false;
			m_iDelayedPreventEscapeFrame = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// game_playvideo (vgui2_api.h)
//-----------------------------------------------------------------------------
void VGui2_Video_SetEscapeGuard(bool state)
{
	if (g_pGameUIViewport)
		g_pGameUIViewport->SetVideoEscapeGuard(state);
}

bool VGui2_Video_TakeEscape()
{
	return g_pGameUIViewport && g_pGameUIViewport->TakeVideoEscape();
}
