//========= SBO VGUI2 port: spectator UI =========//
//
// Purpose: SBO's spectator UI (docs/dev/VGUI2_MIGRATION.md, phase 4):
//          * the black top / bottom bars, map name / spectator count, timer, the target player's name, the
//            Options / < / > / camera mode buttons (shown with duck) and the picture-in-picture frame (the HL SDK's
//            VGUI1 SpectatorPanel: same layout numbers, colours, fonts and TGAs);
//          * the watched player's info box above the bottom bar (CSboSpectatorInfoPanel, sbo_spectator_info.h): SBO
//            2.0.
//          The command menus the Options and camera mode buttons open (spectatormenu.txt, spectcammenu.txt) are
//          sbo_command_menu.h's.
//
// Not a menu-stack menu, so no proxy. SBO's SpectatorPanel (src/cl_dll/spectator_panel.cpp) holds the state and
// keeps all of its callers and logic (hud_spectator.cpp, UpdateSpectatorPanel, m_menuVisible, isVisible(),
// UpdateCursorState); this panel draws that state (VGui2_Spectator_SetState) and hands the buttons' clicks back
// (sbo_bridge.h SpectatorPanelAction).
//
// Mouse: the panel takes mouse input only while the VGUI1 m_menuVisible is set (duck): bars without the buttons leave
// mouse look alone. The cursor itself follows SBO's g_iVisibleMouse (CMenuCursorHelper), which
// UpdateCursorState sets for m_menuVisible.
//
// Structure of the inset frame idea from BugfixedHL-Rebased src/game/client/vgui/spectator_panel.cpp (GPLv3); the
// look is SBO's VGUI1 one.
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_SPECTATOR_PANEL_H
#define SBO_VGUI2_SBO_SPECTATOR_PANEL_H

#include "sbo_bridge.h"
#include "sbo_widgets.h"
#include "vgui2_api.h"

class CSboFilledPanel;
class CSboSpectatorInfoPanel;

class CSboSpectatorPanel : public CSboMenuPanel
{
	DECLARE_CLASS_SIMPLE(CSboSpectatorPanel, CSboMenuPanel);

public:
	//! Creates the panel once (VGui2_Menus_Init).
	static CSboSpectatorPanel *Create(vgui2::Panel *pParent);
	static CSboSpectatorPanel *Get() { return s_pInstance; }

	//! SpectatorPanel::SyncVGui2: shows / hides / updates the bars.
	void SetState(const SboSpectatorState &state);
	//! While a server menu is up: the bars and their texts are not shown (they drew over its window)
	void SetSuppressed(bool bSuppressed);
	//! The bars are up (the spectator command menus are only shown with them).
	bool IsShown() const { return m_bShown; }
	//! The command menus are children of this panel (sbo_command_menu_panel.cpp): their buttons take the mouse
	//! while the panel does (the duck menu).
	void EnableCommandMenuMouseInput()
	{
		if (IsMouseInputEnabled())
			EnableControlsMouseInput(this);
	}

	virtual void OnCommand(const char *command) override;
	virtual void OnThink() override;

protected:
	virtual void LayoutMenu() override;

private:
	CSboSpectatorPanel(vgui2::Panel *pParent);

	//! SpectatorPanel::Update: map name / spectator count, timer, separator (top right).
	void LayoutStatus(int screenWide, int offset);
	//! Mouse input on while the duck menu is up (and the bars shown), else off.
	void UpdateMouseInput();

	static CSboSpectatorPanel *s_pInstance;

	CSboTransparentPanel *m_pTopBar = nullptr;
	CSboLabel *m_pExtraInfo = nullptr;
	CSboImagePanel *m_pTimerImage = nullptr;
	CSboLabel *m_pCurrentTime = nullptr;
	CSboFilledPanel *m_pSeparator = nullptr;
	CSboImagePanel *m_pBanner = nullptr;

	CSboTransparentPanel *m_pBottomBar = nullptr;
	CSboColorButton *m_pOptionButton = nullptr;
	CSboColorButton *m_pCamButton = nullptr;
	CSboColorButton *m_pPrevButton = nullptr;
	CSboColorButton *m_pNextButton = nullptr;
	CSboLabel *m_pBottomLabel = nullptr;

	CSboColorButton *m_pInsetButton = nullptr;

	CSboSpectatorInfoPanel *m_pTargetInfo = nullptr; //!< the watched player's state, above the bottom bar

	// the last state (texts compared, so labels only change when the text does)
	bool m_bShown = false;
	bool m_bSuppressed = false;
	bool m_bMenuVisible = false;
	bool m_bInsetVisible = false;
	int m_iInset[4] = {};
	bool m_bStatusVisible = false;
	bool m_bBannerVisible = false;
	char m_szBanner[128] = "";
	char m_szOptions[64] = "";
	char m_szCam[64] = "";
	char m_szBottom[128] = "";
	char m_szExtraInfo[64] = "";
	char m_szCurrentTime[32] = "";
};

#endif // SBO_VGUI2_SBO_SPECTATOR_PANEL_H
