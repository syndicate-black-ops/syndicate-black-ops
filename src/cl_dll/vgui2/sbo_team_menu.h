//========= SBO VGUI2 port: team menu (map readme) =========//
//
// Purpose: VGUI2 version of SBO's CTeamMenuPanel (src/cl_dll/vgui_teammenu.cpp, MENU_TEAM), which SBO uses as the
//          map readme: the server opens it after the MOTD's OK ("ShowVGUI 2"); it shows the map's name and its
//          description (maps/<map>.txt, loaded as is) and a "Go!" button that sends "ShowVGUI 10" (the briefing) and
//          closes the menu. Same layout, colours and fonts.
//          SBO 2.0: with HTML on (sbo_html_panel.h), a maps/<map>.htm is shown as a web page in place of the text
//          (docs/HTML_MOTD.md).
//
// Only what the VGUI1 menu shows on this branch is built: its team buttons 1-4, the team info texts, Spectate and
// Cancel are never visible there (vgui_teammenu.cpp, CTeamMenuPanel::Update). The keys still work like VGUI1's:
// 5 (and "+jump") = Go!, 6 = the hidden Spectate button ("spectate"); TeamFortressViewport::KeyInput sends them
// through the proxy's SlotInput (sbo_menu_proxy.h -> VGui2_TeamMenu_SlotInput).
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_TEAM_MENU_H
#define SBO_VGUI2_SBO_TEAM_MENU_H

#include "sbo_widgets.h"
#include "sbo_text_window.h" // SboFrame_*, CSboScrollText

class CSboHtmlPanel;

class CSboTeamMenu : public CSboMenuPanel
{
	DECLARE_CLASS_SIMPLE(CSboTeamMenu, CSboMenuPanel);

public:
	//! Creates the panel once (VGui2_Menus_Init).
	static CSboTeamMenu *Create(vgui2::Panel *pParent);
	static CSboTeamMenu *Get() { return s_pInstance; }

	//! CTeamMenuPanel::Open (Update + show).
	void Open();
	//! CTeamMenuPanel::Initialize: new level, the map's name and text are loaded again.
	void Initialize();
	//! HideMenu, and the web page (if any) emptied.
	void Close();
	//! CTeamMenuPanel::SlotInput: 5 = Go!, 6 = Spectate. True if handled.
	bool SlotInput(int iSlot);

	virtual void OnCommand(const char *command) override;
	virtual void OnThink() override;
	virtual void Paint() override;
	virtual void OnMousePressed(vgui2::MouseCode code) override;

protected:
	virtual void LayoutMenu() override;

private:
	CSboTeamMenu(vgui2::Panel *pParent);

	//! The map part of CTeamMenuPanel::Update: map name and maps/<map>.txt, once per level (as soon as the engine
	//! knows the level name).
	void UpdateMapInfo();
	//! The web page (maps/<map>.htm) instead of the text, if HTML is on and the map has one; else the text.
	void UpdateHtml();

	static CSboTeamMenu *s_pInstance;

	CSboLabel *m_pTitle = nullptr;
	CSboTransparentPanel *m_pTeamWindow = nullptr;
	CSboLabel *m_pMapTitle = nullptr;
	CSboTransparentPanel *m_pScrollPanel = nullptr; //!< the CTFScrollPanel without scrollbars: clips the text
	CSboTextPanel *m_pBriefing = nullptr;
	CSboCommandButton *m_pGoButton = nullptr;

	bool m_bUpdatedMapName = false;
	bool m_bHtml = false; //!< the page is shown (the client's one CSboHtmlPanel is in m_pScrollPanel)
	char m_szHtmlFile[256] = {}; //!< "maps/<map>.htm" of the current level (UpdateMapInfo)
	// "New in-game menus" (cl_new_ingame_menus 1, read in Open): the new frame (sbo_text_window.h SboFrame_*), the
	// map's text reflowed in the menus' UI font with the new scroll bar, Go! as the frame's green button
	bool m_bNewLook = false;
	SboFrameLayout m_Frame;
	CSboScrollText *m_pScrollText = nullptr;
	std::wstring m_wBriefing, m_wMapName;
};

#endif // SBO_VGUI2_SBO_TEAM_MENU_H
