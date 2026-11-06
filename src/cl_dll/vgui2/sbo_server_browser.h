//========= SBO VGUI2: server browser ("Find server") =========//
//
// Purpose: SBO's own server browser in the client, opened from the main menu ("Find server",
//          gamedir/syndicate/resource/gamemenu.res -> "engine gameui_serverbrowser", Aura's command name) or with the
//          console command gameui_serverbrowser. Layout and look:
//          docs/SERVER_BROWSER.md.
//
// Modelled on Aura's server browser (phoenixprojectsoftware/Aura-CL 48474e6, cl_dll/gameui/serverbrowser/, itself
// Valve's Source ServerBrowser ported to GoldSrc; HL SDK license): the same Steam calls (ISteamMatchmakingServers
// list requests per tab, PingServer-free list updates, ServerRules / PlayerDetails for the selected server,
// ISteamMatchmaking::AddFavoriteGame for favorites and history, "connect <address>"), hosted on the GameUI viewport
// like the Advanced options dialog (sbo_options.cpp). Written anew for the design's single-window layout (Aura's tabs
// are .res-driven PropertyPages); the drawing is SBO's own.
//
// Data (what the Steam queries give; nothing invented):
//   * the list asks Steam for app 70 (SteamUtils()->GetAppID()) servers with the filter gamedir=syndicate, and keeps
//     only answers whose game folder is syndicate (the LAN list has no filter);
//   * Mode: from the map prefix, as the server's InstallGameRules (gamerules.cpp): co_ Co-op, br_ Battle Royale,
//     sp_ none ("-"), any other Bounty Killer;
//   * Players: humans = A2S players minus bots, of the slot count; Bots: the A2S bot count;
//   * the right panel: Friendly fire from the rule mp_friendlyfire, Time left from mp_timeleft (only while it counts:
//     bounty and BR maps with mp_timelimit), the player names from the A2S player list (which has bots in it too:
//     A2S does not say which names are bots). Round, the players' levels and the spectator count are not in any
//     rule or query today: "-" or left out (docs/SERVER_BROWSER.md lists the server rules they would need).
//
// SDK world (hl1_source_sdk headers); engine calls through sbo_bridge.h.
//=================================================================//

#ifndef SBO_VGUI2_SBO_SERVER_BROWSER_H
#define SBO_VGUI2_SBO_SERVER_BROWSER_H

#include <stdint.h>
#include <string>
#include <vector>

#include <Color.h>
#include <steam/steam_api.h>
#include <steam/isteammatchmaking.h>
#include <steam/isteamhttp.h>
#include "sbo_a2s_rules.h"
#include "sbo_map_picture.h"
#include <vgui_controls/Button.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/TextEntry.h>

namespace vgui2
{
class Menu;
class ScrollBar;
} // namespace vgui2

//-----------------------------------------------------------------------------
// One server of the list (a copy of what Steam answered)
//-----------------------------------------------------------------------------
struct SboServerRow
{
	enum Mode
	{
		MODE_NONE = 0, //!< sp_ maps: no mode name
		MODE_COOP,
		MODE_BOUNTY,
		MODE_BR,
	};

	uint32_t ip = 0; //!< host byte order
	uint16_t connPort = 0;
	uint16_t queryPort = 0;
	std::string name;
	std::string map;
	int mode = MODE_NONE;
	int humans = 0;  //!< players minus bots
	int players = 0; //!< all players (humans and bots), for "Not full"
	int maxPlayers = 0;
	int bots = 0;
	int ping = 0;
	bool password = false;
	bool secure = false; //!< VAC-secured (gameserveritem_t::m_bSecure, A2S_INFO's VAC byte)
	int serverIndex = -1; //!< index in the Steam list request (RefreshServer)

	bool SameServer(const SboServerRow &o) const { return ip == o.ip && connPort == o.connPort; }
	//! "a.b.c.d:port"
	std::string Address() const;
};

//-----------------------------------------------------------------------------
// The list: header with sortable columns, rows, scroll bar (drawn here; the SDK ListPanel can't color one cell or
// set the design's row height)
//-----------------------------------------------------------------------------
class CSboServerListPanel : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboServerListPanel, vgui2::Panel);

public:
	enum Column
	{
		COL_LOCK = 0,
		COL_VAC, //!< VAC-secured (A2S_INFO's VAC byte): Valve's shield, as the engine browser's secure column
		COL_NAME,
		COL_MODE,
		COL_MAP,
		COL_PLAYERS,
		COL_BOTS,
		COL_PING,
		COL_COUNT
	};

	struct Filter
	{
		int mode = 0;          //!< 0 all, else SboServerRow::Mode
		std::string map;       //!< lower case substring, empty = any
		int maxPing = 150;     //!< 0 = any
		bool hasPlayers = true;
		bool notFull = false;
		bool noPassword = false;
	};

	CSboServerListPanel(vgui2::Panel *pParent);

	void Clear();
	void AddOrUpdate(const SboServerRow &row);
	void SetFilter(const Filter &filter);
	void SetEmptyText(const char *text) { m_EmptyText = text ? text : ""; Repaint(); }
	int GetVisibleCount() const { return (int)m_Visible.size(); }
	//! Selected server (nullptr if none or filtered out).
	const SboServerRow *GetSelected() const;
	//! The selected server is in the list (filtered out counts as in it).
	bool HasSelectedRow() const;
	int RowCount() const { return (int)m_Rows.size(); }
	int VisibleCount() const { return (int)m_Visible.size(); }
	void ClearSelection() { m_bHasSelection = false; Repaint(); }

	void Paint() override;
	void PaintBackground() override;
	void PaintBorder() override;
	void PerformLayout() override;
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseDoublePressed(vgui2::MouseCode code) override;
	void OnMouseWheeled(int delta) override;
	void OnKeyCodeTyped(vgui2::KeyCode code) override;
	void OnCursorMoved(int x, int y) override;
	void OnMouseReleased(vgui2::MouseCode code) override;

	MESSAGE_FUNC_INT(OnSliderMoved, "ScrollBarSliderMoved", position);

private:
	void Rebuild();  //!< filter + sort m_Rows into m_Visible, keep the selection
	void UpdateScrollBar();
	void Select(int visibleIndex);
	int RowAt(int y); //!< visible index at panel y, -1 if none
	int HeaderTall();
	int RowTall();
	int RowsFit();
	void ColumnBounds(int col, int &x, int &w);
	int ContentWide(int col);
	int EdgeAt(int x);
	bool Passes(const SboServerRow &row) const;
	void PaintRows();

	std::vector<SboServerRow> m_Rows;
	std::vector<int> m_Visible;
	Filter m_Filter;
	int m_iSortColumn = -1; //!< -1: no sort (the order the servers arrived in) until a column header is clicked
	bool m_bSortAscending = true;
	bool m_bHasSelection = false;
	SboServerRow m_Selected; //!< identity (ip, port) of the selected server
	SboServerRow m_LastPress; //!< the row of the last press, for double clicks
	bool m_bLastPress = false;
	double m_flLastPressTime = 0.0;
	int m_iFirstRow = 0;
	int m_iDragEdge = -1;
	int m_iDragStartX = 0, m_iDragStartWide = 0;
	std::string m_EmptyText;
	vgui2::ScrollBar *m_pScrollBar = nullptr;
};

//-----------------------------------------------------------------------------
// Right panel: overview picture, name, mode / round / time left / friendly fire / address, player names. Runs the
// selected server's ServerRules and PlayerDetails queries.
//-----------------------------------------------------------------------------
class CSboServerInfoPanel : public vgui2::Panel, public ISteamMatchmakingPlayersResponse
{
	DECLARE_CLASS_SIMPLE(CSboServerInfoPanel, vgui2::Panel);

public:
	CSboServerInfoPanel(vgui2::Panel *pParent);
	~CSboServerInfoPanel();

	//! nullptr: nothing selected. Same server again: keeps the queried data, updates the row's values.
	void SetServer(const SboServerRow *pRow);

	void Paint() override;
	void PaintBackground() override;
	void PaintBorder() override;
	void OnThink() override;
	//! The player list scrolls: the wheel over the panel, a press or drag on its scroll bar
	void OnMouseWheeled(int delta) override;
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseReleased(vgui2::MouseCode code) override;
	void OnCursorMoved(int x, int y) override;

	// ISteamMatchmakingPlayersResponse
	void AddPlayerToList(const char *pchName, int nScore, float flTimePlayed) override;
	void PlayersFailedToRespond() override;
	void PlayersRefreshComplete() override;

private:
	void CancelQueries();
	void ApplyRule(const char *pchRule, const char *pchValue);
	void PaintInfo();

	bool m_bHasServer = false;
	SboServerRow m_Server;
	int m_iFriendlyFire = -1; //!< -1 unknown
	int m_iTimeLeft = -1;     //!< mp_timeleft, seconds; -1 unknown or not counting
	int m_iAmxTimeLeft = -1;  //!< amx_timeleft (AMX Mod X's map vote, "MM:SS"), seconds; -1 unknown or not counting
	int m_iRound = -1;        //!< mp_round; -1 unknown, 0 no round yet / the mode counts none
	int m_iRoundLimit = 0;    //!< mp_roundlimit; 0 none
	struct PlayerLine
	{
		std::string name;
		int seconds; //!< connected for (A2S)
	};
	std::vector<PlayerLine> m_Players;
	bool m_bPlayersDone = false;
	// the player list's scrolling: the first row shown; the list's area and the bar as last painted
	int m_iPlayerFirst = 0;
	int m_iListTop = 0, m_iListBottom = 0, m_iRowsFit = 0;
	int m_iBarX = 0, m_iBarW = 0;
	bool m_bDraggingBar = false;
	void ScrollPlayersTo(int first);
	void DragBarTo(int y);
	sbo_a2s::Query m_RulesQuery; //!< A2S_RULES (sbo_a2s_rules.h: why not Steam's ServerRules)
	HServerQuery m_hPlayers = HSERVERQUERY_INVALID;
	double m_flRulesAt = 0.0; //!< when to ask for the rules (after the player list), 0 = not pending

	CSboMapPictures m_Pictures; //!< the map picture (sbo_map_picture.h)
};

//-----------------------------------------------------------------------------
// Controls in the design's look
//-----------------------------------------------------------------------------
class CSboBrowserButton : public vgui2::Button
{
	DECLARE_CLASS_SIMPLE(CSboBrowserButton, vgui2::Button);

public:
	enum Kind
	{
		KIND_NORMAL,
		KIND_PRIMARY, //!< Connect
		KIND_TAB,
		KIND_CLOSE,
	};
	CSboBrowserButton(vgui2::Panel *pParent, const char *pName, const char *text, const char *command, Kind kind);
	void SetActiveTab(bool active) { m_bActive = active; Repaint(); }
	//! The released border in every state: vgui2::Button switches to the scheme's ButtonDepressedBorder (inset
	//! "2 1 1 1") while pressed, and Paint() runs inside the border's insets, so the box, label and text moved 2 px
	//! right and 1 px down on a click. The border is still set (the panel paints its own frame in PaintBorder, which
	//! VGUI2 calls only while a border is set).
	vgui2::IBorder *GetBorder(bool, bool, bool, bool) override { return BaseClass::GetBorder(false, false, false, false); }
	int GetContentWide(); //!< text width + the design's side padding

	void Paint() override;
	void PaintBackground() override;
	void PaintBorder() override;

private:
	Kind m_eKind;
	bool m_bActive = false;
};

class CSboBrowserCheck : public vgui2::Button
{
	DECLARE_CLASS_SIMPLE(CSboBrowserCheck, vgui2::Button);

public:
	CSboBrowserCheck(vgui2::Panel *pParent, const char *pName, const char *text);
	bool IsChecked() const { return m_bChecked; }
	void SetChecked(bool checked) { m_bChecked = checked; Repaint(); }
	//! box + gap + label + a little room: the label never clips, ticked or not
	int GetContentWide();
	//! The released border in every state: vgui2::Button switches to the scheme's ButtonDepressedBorder (inset
	//! "2 1 1 1") while pressed, and Paint() runs inside the border's insets, so the box, label and text moved 2 px
	//! right and 1 px down on a click. The border is still set (the panel paints its own frame in PaintBorder, which
	//! VGUI2 calls only while a border is set).
	vgui2::IBorder *GetBorder(bool, bool, bool, bool) override { return BaseClass::GetBorder(false, false, false, false); }
	void DoClick() override; //!< toggles, posts "ButtonToggled"
	void Paint() override;
	void PaintBackground() override {}
	void PaintBorder() override {}

private:
	bool m_bChecked = false;
};

//! A drop-down: shows the chosen item, opens a menu of the items; posts "SelectChanged" to its action targets.
class CSboBrowserSelect : public vgui2::Button
{
	DECLARE_CLASS_SIMPLE(CSboBrowserSelect, vgui2::Button);

public:
	CSboBrowserSelect(vgui2::Panel *pParent, const char *pName);
	void AddItem(const char *text) { m_Items.push_back(text); }
	void ClearItems() { m_Items.clear(); m_iSelected = 0; Repaint(); }
	int GetItemCount() const { return (int)m_Items.size(); }
	const char *GetItemText(int i) const { return i >= 0 && i < (int)m_Items.size() ? m_Items[i].c_str() : ""; }
	void SetSelected(int i) { m_iSelected = i; Repaint(); }
	int GetSelected() const { return m_iSelected; }
	//! Width that shows the longest item and the arrow (text padding + item + gap + arrow + padding)
	int GetContentWide();
	//! The list chose item i (posts "SelectChanged" when it differs)
	void Choose(int i);
	//! The list closed on a press on this box: the click that press ends does not open it again
	void SuppressClickUntil(double t) { m_flSuppressClickUntil = t; }

	//! The released border in every state: vgui2::Button switches to the scheme's ButtonDepressedBorder (inset
	//! "2 1 1 1") while pressed, and Paint() runs inside the border's insets, so the box, label and text moved 2 px
	//! right and 1 px down on a click. The border is still set (the panel paints its own frame in PaintBorder, which
	//! VGUI2 calls only while a border is set).
	vgui2::IBorder *GetBorder(bool, bool, bool, bool) override { return BaseClass::GetBorder(false, false, false, false); }
	void DoClick() override;
	void OnCommand(const char *command) override;
	void Paint() override;
	void PaintBackground() override;
	void PaintBorder() override;

private:
	std::vector<std::string> m_Items;
	int m_iSelected = 0;
	vgui2::DHANDLE<vgui2::Panel> m_hPopup; //!< the open list (CSboSelectPopup, sbo_server_browser.cpp)
	double m_flSuppressClickUntil = 0.0;
};

class CSboBrowserEntry : public vgui2::TextEntry
{
	DECLARE_CLASS_SIMPLE(CSboBrowserEntry, vgui2::TextEntry);

public:
	CSboBrowserEntry(vgui2::Panel *pParent, const char *pName, const char *placeholder);
	void SetPlaceholder(const char *placeholder) { m_Placeholder = placeholder ? placeholder : ""; }
	void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	void PaintBackground() override;
	void PaintBorder() override;
	void Paint() override;

private:
	std::string m_Placeholder;
};

class CSboServerBrowser;

//-----------------------------------------------------------------------------
// Asks servers for their info itself (PingServer = A2S_INFO), one at a time: the Internet list's numbers come from
// Steam's server list and can be minutes old; the selected server is asked again every few seconds on every tab.
//-----------------------------------------------------------------------------
class CSboServerPinger : public ISteamMatchmakingPingResponse
{
public:
	~CSboServerPinger();
	void Queue(const SboServerRow &row);
	void Cancel(); //!< the query running and the queue
	void ServerResponded(gameserveritem_t &server) override;
	void ServerFailedToRespond() override;

	CSboServerBrowser *m_pBrowser = nullptr;

private:
	void Next();
	std::vector<SboServerRow> m_Queue;
	SboServerRow m_Current;
	HServerQuery m_hQuery = HSERVERQUERY_INVALID;
};

//-----------------------------------------------------------------------------
// The window
//-----------------------------------------------------------------------------
class CSboServerBrowser : public vgui2::Frame, public ISteamMatchmakingServerListResponse
{
	DECLARE_CLASS_SIMPLE(CSboServerBrowser, vgui2::Frame);

public:
	enum Tab
	{
		TAB_INTERNET = 0,
		TAB_FAVORITES,
		TAB_HISTORY,
		TAB_FRIENDS,
		TAB_LAN,
		TAB_COUNT
	};

	CSboServerBrowser(vgui2::Panel *pParent);
	~CSboServerBrowser();

	void Open();
	//! A new query of the tab (the old one released: no more callbacks from it). keepRows: the list shown stays until
	//! the new answer has a server (Refresh); else it is emptied at once (another tab)
	void StartRefresh(bool keepRows = false);
	//! Connect to the server (password: the server's, or empty to keep the "password" cvar as it is).
	void ConnectTo(const SboServerRow &row, const char *password);
	//! Adds ip:port to the Steam favorites (Add server dialog).
	void AddFavorite(uint32_t ip, uint16_t connPort, uint16_t queryPort);
	//! A server's new numbers (list request or CSboServerPinger)
	void UpdateRow(const SboServerRow &row);

	void PerformLayout() override;
	void PaintBackground() override;
	void PaintBorder() override;
	void Paint() override;
	void OnThink() override;
	void OnCommand(const char *command) override;
	void OnKeyCodeTyped(vgui2::KeyCode code) override;
	void OnScreenSizeChanged(int oldWide, int oldTall) override;
	void OnClose() override;

	// ISteamMatchmakingServerListResponse
	void ServerResponded(HServerListRequest hRequest, int iServer) override;
	void ServerFailedToRespond(HServerListRequest hRequest, int iServer) override;
	void RefreshComplete(HServerListRequest hRequest, EMatchMakingServerResponse response) override;

private:
	MESSAGE_FUNC(OnSelectChanged, "SelectChanged");
	MESSAGE_FUNC(OnServerSelected, "ServerSelected");
	MESSAGE_FUNC(OnServerActivated, "ServerActivated");
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
	MESSAGE_FUNC_PTR(OnButtonToggled, "ButtonToggled", panel);
	MESSAGE_FUNC_CHARPTR(OnJoinWithPassword, "JoinWithPassword", password);

	void SetTab(int tab);
	void ApplyFilter();
	void UpdateButtons();
	void ReleaseRequest();
	void SetStatus(const char *text, float seconds);
	void KeepOnScreen();
	void SaveBounds();
	void RefreshSelected();

	int m_iTab = TAB_INTERNET;
	HServerListRequest m_hRequest = nullptr;
	CSboServerPinger m_Pinger;
	bool m_bRefreshing = false;
	double m_flRefreshedAt = -1.0;
	double m_flRequestStart = 0.0;       //!< when the list request was made (the browser log at developer 1)
	int m_nResponded = 0, m_nFailed = 0; //!< the request's callbacks so far (the browser log)
	bool m_bKeepingRows = false;         //!< the rows shown are the previous answer's (Refresh), replaced at the first server
	double m_flLimitedUntil = -1.0;      //!< Steam limits Internet list requests until then (see RefreshComplete)
	AppId_t m_uAppID = 70;
	std::string m_Status; //!< a short message in place of "Refreshed n s ago"
	double m_flNextSelectedRefresh = 0.0;
	double m_flSaveAt = 0.0; //!< when to write serverbrowser.vdf (after a change), 0 = nothing to write //!< when to ask the selected server for its info again
	double m_flStatusUntil = 0.0;
	int m_iShownSeconds = -1;
	int m_iShownWait = 0;
	//! Seconds until Steam takes Internet list requests again (0: not limited, or another tab)
	int LimitedSeconds() const;
	std::string LimitedText() const;
	SboServerRow m_PendingConnect; //!< the server the password dialog is for
	// layout results PerformLayout leaves for Paint
	int m_iFilterRowY = 0, m_iFilterRowTall = 0, m_iStatusRight = 0, m_iBottomY = 0, m_iBottomTall = 0, m_iTabLineY = 0;

	CSboBrowserButton *m_pClose = nullptr;
	CSboBrowserButton *m_pTabs[TAB_COUNT] = {};
	CSboBrowserSelect *m_pModeFilter = nullptr;
	CSboBrowserEntry *m_pMapFilter = nullptr;
	CSboBrowserSelect *m_pPingFilter = nullptr;
	CSboBrowserCheck *m_pHasPlayers = nullptr;
	CSboBrowserCheck *m_pNotFull = nullptr;
	CSboBrowserCheck *m_pNoPassword = nullptr;
	CSboServerListPanel *m_pList = nullptr;
	CSboServerInfoPanel *m_pInfo = nullptr;
	CSboBrowserButton *m_pAddServer = nullptr;
	CSboBrowserButton *m_pAddFavorite = nullptr;
	CSboBrowserButton *m_pRefresh = nullptr;
	CSboBrowserButton *m_pConnect = nullptr;
};

//! Registers the gameui_serverbrowser console command (HUD_Init time).
void VGui2_RegisterServerBrowserCommand();
//! Opens the browser if gameui_serverbrowser asked for it (from CGameUIViewport::Frame, one frame after the command).
void VGui2_ServerBrowser_Frame();
//! cl_experimental_menus changed (VGui2_MenusStyle_Frame): closes the browser if it is open; true if it was
bool VGui2_ServerBrowser_CloseIfOpen();

#endif // SBO_VGUI2_SBO_SERVER_BROWSER_H
