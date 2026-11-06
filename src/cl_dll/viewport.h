//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef TEAMFORTRESSVIEWPORT_H
#define TEAMFORTRESSVIEWPORT_H

// custom scheme handling
#include "vgui2/sbo_text_scheme.h"

#define TF_DEFS_ONLY
#include "tf_defs.h"

class SpectatorPanel;
class CViewportMenu;

void ScaleColors( int &r, int &g, int &b, int a );
extern char *sTFClassSelection[];
extern int iTeamColors[5][3];
extern int iNumberOfTeamColors;

#define MAX_SERVERNAME_LENGTH	32

// Command Menu positions (the menus themselves: vgui2/sbo_command_menu.cpp)
#define MAX_MENUS				80
#define MAX_BUTTONS				100

#define BUTTON_SIZE_Y			YRES(30)
#define CMENU_SIZE_X			XRES(160)

#define CMENU_TOP				(BUTTON_SIZE_Y * 4)

#define MAX_TEAMNAME_SIZE		64
// chat-vgui2: TeamNames labels kept (team 1..MAX_TEAMNAMES). Was m_sTeamNames[5], indexed 1..count: the 5 SBO
// scoreboard categories overflowed it.
#define MAX_TEAMNAMES			16
#define MAX_BUTTON_SIZE			32

//==============================================================================
// SBO 2.0: the client's menu and player data manager. It was the VGUI1 viewport panel of the HL SDK; nothing of
// it is a panel any more (docs/dev/VGUI2_MIGRATION.md, "VGUI1 removal"): the menu stack (ShowVGUIMenu, HideTopMenu,
// m_pCurrentMenu), key input, the cursor state, the spectator panel's update, the user messages of the menus and of
// the shared player / team data. The menus themselves are VGUI2 panels (src/cl_dll/vgui2).
class TeamFortressViewport
{
private:
	float		 m_flSpectatorPanelLastUpdated;
	int			 m_iCurrentTeamNumber;
	int			 m_iCurrentPlayerClass;
	int			 m_iUser1;
	int			 m_iUser2;
	int			 m_iUser3;

	// VGUI Menus
	CViewportMenu*	 ShowTeamMenu( void );
	void		 CreateSpectatorMenu( void );
	// SBO menus (created in the constructor, shown by ShowVGUIMenu). Private: their decorated names (AAE) put them
	// in SBO's incremental-link thunk slots.
	CViewportMenu*	 ShowSyndBrief( void );
	CViewportMenu*	 ShowGmMenu( void );
	
	// Scheme handler
	CSchemeManager m_SchemeManager;

	// MOTD
	int		m_iGotAllMOTD;
	char	m_szMOTD[ MAX_MOTD_LENGTH ];

	int					m_iAllowSpectators;

	// Data for specific sections of the Command Menu
	int			m_iValidClasses[5];
	int			m_iIsFeigning;
	int			m_iIsSettingDetpack;
	int			m_iNumberOfTeams;
	int			m_iBuildState;
	int			m_iRandomPC;
	char		m_sTeamNames[MAX_TEAMNAMES + 1][MAX_TEAMNAME_SIZE];

	char		m_sMapName[64];
public:
	TeamFortressViewport(int x,int y,int wide,int tall);
	void Initialize( void );

	void UpdateCursorState( void );
	void UpdateCommandMenu(int menuIndex);
	void UpdateOnPlayerInfo( void );
	void UpdateSpectatorPanel( void );

	int	 KeyInput( int down, int keynum, const char *pszCurrentBinding );
	void InputPlayerSpecial( void );
	void GetAllPlayersInfo( void );
	void DeathMsg( int killer, int victim );

	// The command menus (commandmenu.txt, spectatormenu.txt, spectcammenu.txt): vgui2/sbo_command_menu.cpp
	void ShowCommandMenu(int menuIndex);
	void InputSignalHideCommandMenu( void );
	void HideCommandMenu( void );
	void SetCurrentMenu( CViewportMenu *pMenu );

	void ShowScoreBoard( void );
	void HideScoreBoard( void );
	bool IsScoreBoardVisible( void );

	bool AllowedToPrintText( void );

	void ShowVGUIMenu( int iMenu );
	void HideVGUIMenu( void );
	void HideTopMenu( void );

	CViewportMenu* CreateTextWindow( int iTextToShow );

	// Data Handlers
	int GetValidClasses(int iTeam) { return m_iValidClasses[iTeam]; };
	int GetNumberOfTeams() { return m_iNumberOfTeams; };
	int GetIsFeigning() { return m_iIsFeigning; };
	int GetIsSettingDetpack() { return m_iIsSettingDetpack; };
	int GetBuildState() { return m_iBuildState; };
	int IsRandomPC() { return m_iRandomPC; };
	char *GetTeamName( int iTeam ) { return m_sTeamNames[iTeam]; };
	int GetAllowSpectators() { return m_iAllowSpectators; };

	// Message Handlers
	int MsgFunc_ValClass(const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamNames(const char *pszName, int iSize, void *pbuf );
	int MsgFunc_Feign(const char *pszName, int iSize, void *pbuf );
	int MsgFunc_Detpack(const char *pszName, int iSize, void *pbuf );
	int MsgFunc_VGUIMenu(const char *pszName, int iSize, void *pbuf );
	int MsgFunc_MOTD( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_BuildSt( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_RandomPC( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ServerName( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ScoreInfo( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamScore( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamInfo( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_Spectator( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_AllowSpec( const char *pszName, int iSize, void *pbuf );

	// Input
	bool SlotInput( int iSlot );

	// Once per frame (was the VGUI1 panel's paintBackground): the command menu after a team / class change, the
	// spectator panel when the target or mode changed and once a second.
	void Frame( void );

	CSchemeManager *GetSchemeManager( void ) { return &m_SchemeManager; }

	void *operator new( size_t stAllocateBlock );

public:
	// VGUI Menus
	CViewportMenu	*m_pCurrentMenu;
	int						m_StandardMenu;	// root menu indexes (sbo::CommandMenu_Create)
	int						m_SpectatorOptionsMenu;
	int						m_SpectatorCameraMenu;
	// SBO 2.0: the teams in g_TeamInfo (were the VGUI1 scoreboard's: ScorePanel::m_iNumTeams, RebuildTeams, SortTeams)
	int				m_iNumTeams;
	void			RebuildTeams( void );
	void			RecalculateTeamScores( void );
	SpectatorPanel *		m_pSpectatorPanel;
	char			m_szServerName[ MAX_SERVERNAME_LENGTH ];
	// SBO
	// SBO: equipment chosen in the briefing menu (vgui2/sbo_brief_menu.cpp; sbo::Get/SetBriefChoice)
	int				m_iBlade;		// 0xae4
	int				m_iLight;		// 0xae8
	int				m_iHeavy;		// 0xaec
	int				m_iItem;		// 0xaf0
	int				m_iCyberware;	// 0xaf4
	int				m_iArmor;		// 0xaf8
	int				m_iMaleFemale;	// 0xafc
	int				m_iSBOInit;		// 0xb00, set once the defaults above are set (sbo_menu_bridge.cpp)
};

#define HIDE_TEXTWINDOW		0
#define SHOW_MAPBRIEFING	1
#define SHOW_CLASSDESC		2
#define SHOW_MOTD			3
#define SHOW_SPECHELP		4

//================================================================
// A menu on the viewport's menu stack (the HL SDK's CMenuPanel without the VGUI1 panel): the stack queues menus
// that arrive while one is open (m_pNextMenu) and knows them by their menu ID. What a menu shows is a VGUI2 panel,
// opened and closed by the subclasses in vgui2/sbo_menu_bridge.cpp (vgui2/sbo_menu_proxy.h).
class CViewportMenu
{
private:
	CViewportMenu *m_pNextMenu;
	int			m_iMenuID;
	int			m_iIsActive;
	float		m_flOpenTime;
public:
	CViewportMenu()
	{
		m_iMenuID = 0;
		Reset();
	}

	virtual ~CViewportMenu() {}

	virtual void Reset( void )
	{
		m_pNextMenu = NULL;
		m_iIsActive = false;
		m_flOpenTime = 0;
	}

	void SetNextMenu( CViewportMenu *pNextPanel )
	{
		if (m_pNextMenu)
			m_pNextMenu->SetNextMenu( pNextPanel );
		else
			m_pNextMenu = pNextPanel;
	}

	void SetMenuID( int iID )
	{
		m_iMenuID = iID;
	}

	void SetActive( int iState )
	{
		m_iIsActive = iState;
	}

	virtual void Open( void )
	{
		// Note the open time, so we can delay input for a bit
		m_flOpenTime = gHUD.m_flTime;
	}

	virtual void Close( void )
	{
		m_iIsActive = false;
	}

	CViewportMenu* GetNextMenu() { return m_pNextMenu; };
	int			GetMenuID() { return m_iMenuID; };
	int			IsActive() { return m_iIsActive; };
	float		GetOpenTime() { return m_flOpenTime; };

	// Numeric input
	virtual bool SlotInput( int iSlot ) { return false; };
};

#endif
