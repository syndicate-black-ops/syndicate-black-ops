//========= SBO VGUI2: joining screen (connecting, downloading, loading) =========//
//
// Purpose: SBO's full-screen joining screen, shown in place of GameUI's small "Loading..." window (CLoadingDialog)
//          while the game connects to a server, downloads its files and loads the map. Layout and look: the
//          design in docs/LOADING_SCREEN.md, in the server browser's look (sbo_ui_style.h).
//
// How it works (nothing in the engine or GameUI is changed):
//   * When it runs: on VGUI2's tick (IVGui::AddTickSignal, every IVGui::RunFrame). The engine runs IVGui::RunFrame at
//     the start of every paint (CBaseUI::Paint: GameUI's RunFrame, IVGui::RunFrame, then the surface's paint), also
//     during the map load when no client frame runs. So the screen sees GameUI's dialog and takes the paint before
//     the first paint that would draw it, at every stage. (Client frames alone are not enough: GameUI opens its
//     dialog inside the engine's connect and load code, and the engine paints it before the next HUD_Frame.)
//   * It looks for GameUI's dialog, the visible panel named "LoadingDialog" under the GameUI panel. From the first
//     sight it covers the whole screen (a Frame on the GameUI viewport like the server browser) until the game is
//     entered (the HUD draws again), the engine is no longer connected (Cancel, an error closed, a disconnect), or
//     nothing has happened for a long time. The gaps between GameUI's stage windows, the reconnect after downloads
//     and the reconnect of a map change or retry stay covered.
//   * Paint: while it connects the engine paints only one panel's tree (the restricted panel of its surface,
//     BaseUISurface::PaintTraverse: GameUI's loading window), and that window is app-modal. The screen takes both
//     (ISurface::RestrictPaintToSinglePanel, IInput::SetAppModalSurface) on every tick, so it is painted at every
//     stage, the map load too, and gets Esc / Cancel. GameUI's window is never moved, resized, hidden or touched; it
//     keeps running, unpainted.
//   * Fallback: only if the screen really does not draw (ticks keep coming, each before an engine paint, and none
//     of them is followed by a paint of the screen: FALLBACK_TICKS ticks and FALLBACK_SECONDS) the paint and the
//     input go back to GameUI's window for the rest of that connect. A long blocking load (no ticks, no paints)
//     does not count.
//   * cl_experimental_menus 0: the screen is never shown (GameUI's own window, as before); read at each connect.
//     A +connect start runs before config.cfg: the saved value is set when the cvar is registered (sbo_ui_style.cpp).
//   * Never a child of GameUI's panels: GameUI's VGUI code takes every child of its window for one of its own Panels
//     (Panel::FindChildByName) and crashes on a panel of another module (seen with gdb on Linux).
//   * One session per server: a new GameUI window for the same server within seconds (a reconnect after downloads)
//     keeps the screen and its data.
//   * Texts: the dialog's own labels, read with VGUI2's GetText request (Label::RequestInfo, through
//     vgui2::ipanel()): InfoLabel (the stage: GameUI's localized #GameUI_* texts, compared with the localize table),
//     SecondaryProgressLabel (the file being downloaded), TimeRemainingLabel (GameUI's time left for that file).
//     Its progress bars cannot be read (ProgressBar has no request for its value): the overall bar shows the steps
//     done, the file bar the engine's scr_downloading cvar (the current file's percent) where it is set.
//     Error layout (the dialog's Progress bar hidden): the error text and Close. A window that is in the error layout
//     at first sight is a game the server ended (shut down, kick, time-out): a small "Disconnected" window instead
//     of the join steps.
//   * Server: the address from the engine (net_api Status), or the server browser's row when it connected; then the
//     browser's queries: A2S info (Steam PingServer: name, map, players, bots, ping), the player list (PlayerDetails)
//     and the rules (sbo_a2s::Query: mp_friendlyfire). A game this client hosts (loopback) has no queries.
//   * Map: the map picture (sbo_map_picture.h: shipped, downloaded, overview) as the background and the big picture; the
//     first lines of maps/<map>.txt (the map's briefing) when the client has the map.
//   * Tip: one line of resource/loadingtips.txt per join, at random.
//   * Cancel (and Esc): GameUI's own command, posted to its dialog ("Cancel"; "Close" for an error), as its button.
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_LOADING_SCREEN_H
#define SBO_VGUI2_SBO_LOADING_SCREEN_H

#include <string>
#include <vector>

#include <steam/steam_api.h>
#include <steam/isteammatchmaking.h>
#include <vgui_controls/Frame.h>

#include "sbo_a2s_rules.h"
#include "sbo_map_picture.h"
#include "sbo_server_browser.h"

class CSboLoadingScreen : public vgui2::Frame,
                          public ISteamMatchmakingPingResponse,
                          public ISteamMatchmakingPlayersResponse
{
	DECLARE_CLASS_SIMPLE(CSboLoadingScreen, vgui2::Frame);

public:
	enum Stage
	{
		STAGE_CONNECT = 0,
		STAGE_SERVERINFO,
		STAGE_DOWNLOAD,
		STAGE_LOAD,
		STAGE_ENTER,
		STAGE_COUNT
	};

	CSboLoadingScreen(vgui2::Panel *pParent);
	~CSboLoadingScreen();

	//! Once per client frame: shows / hides the screen with GameUI's dialog, starts the server queries.
	void Frame();
	//! The server browser connects to this server (its name and map are known at once).
	void SetPendingServer(const SboServerRow &row);
	//! A connect (or a disconnect's message) holds the screen now
	bool IsActive() const { return m_bActive; }

	void Paint() override;
	void PaintBackground() override;
	void PaintBorder() override {}
	void PerformLayout() override;
	void OnCommand(const char *command) override;
	void OnKeyCodeTyped(vgui2::KeyCode code) override;
	void OnScreenSizeChanged(int oldWide, int oldTall) override;
	void OnTick() override;

	//! The map picture box (painted by its child panel, in that panel's coordinates)
	void PaintPicture(int x, int y, int w, int h);

	// ISteamMatchmakingPingResponse (A2S info)
	void ServerResponded(gameserveritem_t &server) override;
	void ServerFailedToRespond() override;
	// ISteamMatchmakingPlayersResponse (A2S players)
	void AddPlayerToList(const char *pchName, int nScore, float flTimePlayed) override;
	void PlayersFailedToRespond() override;
	void PlayersRefreshComplete() override;

private:
	void Begin(vgui2::VPANEL dialog);
	void End();
	//! The server data (address, info, players, map, queries) cleared for a new target
	void ResetServer();
	void Release(bool toDialog);
	void CancelQueries();
	void StartQueries();
	//! The dialog's texts and layout (stage, file, time left, error); safe in Paint (no panel changes)
	void ReadDialog();
	void SetMap(const std::string &map);
	//! The map a game this client starts loads, before the client connects to it (`map` / a listen server's
	//! `changelevel` from the console): "" when not known
	std::string StartingMap() const;
	void LoadBriefing();
	void PickTip();
	void PaintBackdrop();
	void PaintDropped();
	void PaintCard(int x, int y, int w, int h, const char *heading);
	//! Word-wrapped lines, at most maxLines (the last one cut with "...")
	int DrawWrapped(vgui2::HFont font, Color color, int x, int y, int w, const std::wstring &text, int maxLines);
	//! The lines DrawWrapped would draw, all of them
	std::vector<std::wstring> WrapLines(vgui2::HFont font, const std::wstring &text, int w);
	//! A scrollable list of lines (the player list, the map's text): the lines from `first` that fit, a scroll bar in
	//! the browser's colors at the right when they do not all fit. Returns the first line actually shown (clamped).
	int PaintScrolled(const std::vector<std::wstring> &lines, int first, int x, int y, int w, int h, int lineTall,
	    Color color, int *rect);
	void OnMouseWheeled(int delta) override;
	int m_iPlayersScroll = 0, m_iAboutScroll = 0; //!< first line shown
	int m_rPlayers[4] = {}, m_rAbout[4] = {};      //!< the areas, for the wheel

	vgui2::VPANEL m_hDialog = 0;
	vgui2::HPanel m_hDialogH = 0, m_hLastDialogH = 0; //!< handles of the two (0 once GameUI deleted the window)
	bool m_bActive = false;
	double m_flLastDialog = -100.0; //!< when GameUI's loading window was last seen visible
	vgui2::VPANEL m_hLastDialog = 0; //!< the last one seen (gets the paint back if this screen fails)
	bool m_bFailed = false;          //!< this screen did not paint: GameUI's window is shown for this connect
	int m_nPaints = 0, m_nPaintsAtClaim = 0;
	int m_nTicksNoPaint = 0;         //!< ticks since this screen last painted
	double m_flClaimTime = 0.0;      //!< when this screen last painted (or took the paint)
	float m_flHudTime = 0.0f;        //!< gHUD.m_flTime at the last tick
	bool m_bHudSinceDialog = false;  //!< the game drew its HUD after GameUI's window was last seen: in the game
	bool m_bSawDialog = false;       //!< GameUI's window was seen in this session (not yet when the browser starts it)
	double m_flDisconnectedSince = -1.0; //!< when the engine was first seen not connected without GameUI's window
	double m_flEndTime = -100.0;    //!< when the last session ended
	double m_flBeginTime = 0.0;

	// from the dialog
	int m_iStage = STAGE_CONNECT;
	bool m_bError = false;
	//! GameUI's window was in its error layout at first sight: a game ended by the server (shut down, kicked, timed
	//! out), not a connect that failed. Shown as a small "Disconnected" window over the backdrop, no join steps.
	bool m_bDropped = false;
	int m_rBox[4] = {}; //!< the Disconnected window (x, y, w, h)
	std::wstring m_Info, m_File, m_TimeLeft;
	std::wstring m_LastFile;
	int m_nFiles = 0; //!< files whose download started (each new SecondaryProgressLabel text)
	float m_flFilePercent = -1.0f;

	// the server
	SboServerRow m_Pending;
	double m_flPendingTime = -1.0;
	SboServerRow m_Server;
	bool m_bHaveAddress = false, m_bLocal = false, m_bHaveInfo = false, m_bInfoFailed = false;
	HServerQuery m_hPing = HSERVERQUERY_INVALID, m_hPlayers = HSERVERQUERY_INVALID;
	sbo_a2s::Query m_Rules;
	double m_flRulesAt = 0.0;
	int m_iFriendlyFire = -1;
	std::vector<std::string> m_Players;
	bool m_bPlayersDone = false;

	// the map
	std::string m_Map;
	unsigned m_uBeginIp = 0;          //!< the engine's connection when the connect began (the server left)
	unsigned short m_uBeginPort = 0;
	bool m_bBeginLoopback = false, m_bBeginHadAddr = false;
	std::string m_BeginLevel; //!< the engine's level when the connect began (the old map until the new server info)
	std::string m_BeginHostMap; //!< the engine's HostMap cvar when the connect began (`map` sets it to its map)
	std::vector<std::wstring> m_Briefing;
	CSboMapPictures m_Pictures;
	std::wstring m_Tip;

	CSboBrowserButton *m_pCancel = nullptr;
	vgui2::Panel *m_pPicture = nullptr;
	int m_iMainY = 0, m_iBottomY = 0, m_iBottomTall = 0;
};

//! From CGameUIViewport::Frame
void VGui2_LoadingScreen_Frame();
//! From the server browser's connect
void VGui2_LoadingScreen_Connecting(const SboServerRow &row);
//! From Create server's start: the map the local game loads
void VGui2_LoadingScreen_StartingLocal(const char *map);
//! GameUI's loading / error window is up, or this screen holds a connect or a disconnect's message: both take the
//! paint and the input (app-modal), so no other window may open over them (the updater waits)
bool VGui2_LoadingScreen_DialogUp();

#endif // SBO_VGUI2_SBO_LOADING_SCREEN_H
