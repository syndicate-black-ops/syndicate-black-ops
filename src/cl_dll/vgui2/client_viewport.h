//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: Minimal VGUI2 viewports for SBO.
//
// CClientViewport: cut-down BugfixedHL-Rebased vgui/client_viewport.{h,cpp}.
//   Full-screen, input-less EditablePanel under the engine's PANEL_CLIENTDLL; the
//   parent of the VGUI2 chat. All of BHL's VGUI2 menus/scoreboard/MOTD/spectator
//   panels are left out; SBO's own VGUI2 panels (scoreboard, kill feed, the menus
//   of docs/dev/VGUI2_MIGRATION.md) are popups under this panel.
//
// CGameUIViewport: the "prevent ESC from opening GameUI" part of BHL's
//   gameui/gameui_viewport.{h,cpp} (used while the chat input line is open).
//=================================================================//

#ifndef SBO_VGUI2_CLIENT_VIEWPORT_H
#define SBO_VGUI2_CLIENT_VIEWPORT_H

#include <vgui_controls/EditablePanel.h>

class CClientViewport : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CClientViewport, vgui2::EditablePanel);

public:
	CClientViewport();

	void ActivateClientUI();
	void HideClientUI();

	virtual void OnThink() override;
};

//-----------------------------------------------------------------------------
// The VGUI2 surface owns the cursor and hides it unless a VGUI2 popup accepts mouse input. This 1x1, paint-less
// popup accepts mouse input while SBO's viewport wants the cursor (g_iVisibleMouse: a menu-stack menu, the
// spectator's duck menu, a command menu with hud_capturemouse), so the cursor shows for all of them alike, as it
// did when the viewport was VGUI1 (hence the name).
//-----------------------------------------------------------------------------
class CMenuCursorHelper : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CMenuCursorHelper, vgui2::Panel);

public:
	CMenuCursorHelper(vgui2::Panel *pParent);
	void Update(bool bWantCursor);

private:
	bool m_bActive = false;
};

class CGameUIViewport : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CGameUIViewport, vgui2::EditablePanel);

public:
	CGameUIViewport();

	/**
	 * If enabled, prevents ESC key from showing GameUI.
	 * In reality, hides GameUI whenever it is enabled.
	 */
	void PreventEscapeToShow(bool state);

	/**
	 * game_playvideo (video/video_player.cpp): while on, Esc does not open GameUI (it is hidden again at once, as
	 * PreventEscapeToShow does) and the press is remembered for TakeVideoEscape. HL25's engine opens GameUI on Esc
	 * before the client's HUD_Key_Event sees the key.
	 */
	void SetVideoEscapeGuard(bool state);
	//! True once after Esc opened GameUI while the guard was on.
	bool TakeVideoEscape();
	//! The same while a server menu window is up (sbo_server_menu.cpp): Esc goes to the window, not the game menu
	void SetMenuEscapeGuard(bool state);
	bool TakeMenuEscape();

	//! Called once per client frame (HUD_Frame -> VGui2_Frame).
	void Frame();

	virtual void OnThink() override;

private:
	bool m_bPreventEscape = false;
	bool m_bVideoEscapeGuard = false;
	bool m_bVideoEscapePressed = false;
	bool m_bMenuEscapeGuard = false;
	bool m_bMenuEscapePressed = false;
	int m_iDelayedPreventEscapeFrame = 0;
	int m_iFrameCount = 1;
	CMenuCursorHelper *m_pMenuCursor = nullptr;
};

extern CClientViewport *g_pViewport;
extern CGameUIViewport *g_pGameUIViewport;

// client_vgui.cpp
void VGui2_RegisterDebugCommands();

#endif
