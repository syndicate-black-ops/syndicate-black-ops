//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// spectator_panel.h: interface for the SpectatorPanel class.
//
// SBO 2.0: the state of the spectator UI only. The bars, labels and buttons are drawn by the VGUI2 panel
// (vgui2/sbo_spectator_panel.cpp), which gets this state through SyncVGui2; the VGUI1 widgets this class used to
// own are gone. Its callers (hud_spectator.cpp, the viewport, the spectator command menus) and its logic are
// unchanged.
//////////////////////////////////////////////////////////////////////

#ifndef SPECTATORPANEL_H
#define SPECTATORPANEL_H

#define SPECTATOR_PANEL_CMD_NONE				0

#define SPECTATOR_PANEL_CMD_OPTIONS				1
#define	SPECTATOR_PANEL_CMD_PREVPLAYER			2
#define SPECTATOR_PANEL_CMD_NEXTPLAYER			3
#define	SPECTATOR_PANEL_CMD_HIDEMENU			4
#define	SPECTATOR_PANEL_CMD_TOGGLE_INSET		5
#define SPECTATOR_PANEL_CMD_CAMERA				6

class SpectatorPanel
{

public:
	SpectatorPanel();
	virtual ~SpectatorPanel();

	void			ActionSignal(int cmd);

public:
	void Initialize();
	void Update();

public:

	void EnableInsetView(bool isEnabled);
	void ShowMenu(bool isVisible);

	// the bars are shown (spectating, hud_draw, no intermission)
	bool			isVisible() { return m_bVisible; }
	void			setVisible( bool bVisible ) { m_bVisible = bVisible; }

	// colour of the bottom label (the watched player's name), VGUI1 convention: alpha 0 = opaque
	void			SetBottomColor( int r, int g, int b, int a );

	// hands the state to the VGUI2 panel (on changes and with UpdateSpectatorPanel, about once a second)
	void			SyncVGui2();

	bool			m_menuVisible;
	bool			m_insetVisible;

	// what the VGUI2 widgets show
	char			m_szVGui2BottomText[128];
	char			m_szVGui2ExtraInfo[64];
	char			m_szVGui2CurrentTime[32];	// the timer
	char			m_szVGui2CamText[64];
	char			m_szVGui2OptionsText[64];
	char			m_szVGui2Banner[128];	// the top banner's image (DRC_CMD_BANNER)

private:
	bool			m_bVisible;
	bool			m_bBannerVisible;	// the top banner (HLTV only)
	int				m_iBottomColor[4];
};

#endif // !defined SPECTATORPANEL_H
