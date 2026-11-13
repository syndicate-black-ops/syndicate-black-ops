//========= SBO VGUI2: joining screen (connecting, downloading, loading) =========//
//
// Purpose: see sbo_loading_screen.h.
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <algorithm>

#include <FileSystem.h>
#include <IEngineVGui.h>
#include <KeyValues.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <vgui/ILocalize.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Frame.h>

#include "client_steam_context.h"
#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_create_server.h"
#include "sbo_loading_screen.h"
#include "sbo_ui_style.h"

#ifdef PostMessage
#undef PostMessage
#endif
#ifdef SendMessage
#undef SendMessage
#endif

using namespace sbolook;

static vgui2::DHANDLE<CSboLoadingScreen> s_hScreen;

namespace
{
// the rules query follows the player query after this many seconds (a server's max_queries_sec window; as the browser)
constexpr double RULES_DELAY = 1.1;
// the browser's row counts for a connect made this recently
constexpr double PENDING_MAX_AGE = 30.0;
// map briefing: at most this many bytes read, this many lines shown
constexpr unsigned BRIEFING_MAX_BYTES = 64 * 1024;
constexpr int TIPS_MAX = 64;
// fallback to GameUI's window: this many ticks (each one comes right before an engine paint) and this much time
// without one paint of this screen
constexpr int FALLBACK_TICKS = 12;
constexpr double FALLBACK_SECONDS = 0.5;
// no GameUI window, not in the game: the engine counts as disconnected after this long (Cancel, an error closed); the
// screen goes away after GAP_MAX with no GameUI window even while connected (a safety net, no stage is that long)
constexpr double DISCONNECTED_END = 0.3;
constexpr double GAP_MAX = 20.0;
// a screen started by the browser's connect before GameUI's first window: the engine may take this long to start
constexpr double PREDIALOG_MAX = 3.0;

const char *const s_StepNow[CSboLoadingScreen::STAGE_COUNT] = {
	"Connecting to the server", "Receiving server info", "Downloading map files", "Loading game resources",
	"Entering the game" };
const char *const s_StepDone[CSboLoadingScreen::STAGE_COUNT] = {
	"Connected", "Server info received", "Map files ready", "Game resources loaded", "Entering the game" };

//! GameUI's localized text for token starts the label's text (the part before its first %s1 argument)
bool StartsWithToken(const std::wstring &text, const char *token)
{
	if (text.empty() || !g_pVGuiLocalize)
		return false;
	const wchar_t *loc = g_pVGuiLocalize->Find(token);
	if (!loc || !loc[0])
		return false;
	size_t n = 0;
	while (loc[n] && loc[n] != L'%')
		n++;
	while (n > 0 && loc[n - 1] == L' ')
		n--;
	return n > 0 && text.size() >= n && !wcsncmp(text.c_str(), loc, n);
}

//! The engine's stage text -> stage (-1 unknown)
int StageOfText(const std::wstring &text)
{
	static const struct
	{
		const char *token;
		int stage;
	} s_Map[] = {
		{ "#GameUI_EstablishingConnection", CSboLoadingScreen::STAGE_CONNECT },
		{ "#GameUI_RetryingConnectionToServer", CSboLoadingScreen::STAGE_CONNECT },
		{ "#GameUI_RetryingConnectionToServer2", CSboLoadingScreen::STAGE_CONNECT },
		{ "#GameUI_RetryingConnectionToServer3", CSboLoadingScreen::STAGE_CONNECT },
		{ "#GameUI_LoadingSecurityModule", CSboLoadingScreen::STAGE_CONNECT },
		// a game this client starts (`map`): the server loads first, then the client connects (the steps stay in order)
		{ "#GameUI_StartingServer", CSboLoadingScreen::STAGE_CONNECT },
		{ "#GameUI_DownloadingSecurityModule", CSboLoadingScreen::STAGE_CONNECT },
		{ "#GameUI_ParseServerInfo", CSboLoadingScreen::STAGE_SERVERINFO },
		{ "#GameUI_VerifyingAndDownloading", CSboLoadingScreen::STAGE_DOWNLOAD },
		{ "#GameUI_VerifyingResources", CSboLoadingScreen::STAGE_DOWNLOAD },
		{ "#GameUI_CheckCRCs", CSboLoadingScreen::STAGE_DOWNLOAD },
		{ "#GameUI_PrecacheResources", CSboLoadingScreen::STAGE_LOAD },
		{ "#GameUI_PrecachingResources", CSboLoadingScreen::STAGE_LOAD },
		{ "#GameUI_LoadingFilename", CSboLoadingScreen::STAGE_LOAD },
		{ "#GameUI_ParseBaseline", CSboLoadingScreen::STAGE_ENTER },
	};
	for (const auto &m : s_Map)
	{
		if (StartsWithToken(text, m.token))
			return m.stage;
	}
	return -1;
}

//! A panel named name under panel (depth-limited, any visibility), 0 if none
vgui2::VPANEL FindNamed(vgui2::VPANEL panel, const char *name, int depth)
{
	if (!panel || depth > 4)
		return 0;
	const char *n = vgui2::ipanel()->GetName(panel);
	if (n && !Q_stricmp(n, name))
		return panel;
	const int count = vgui2::ipanel()->GetChildCount(panel);
	for (int i = 0; i < count; i++)
	{
		if (vgui2::VPANEL found = FindNamed(vgui2::ipanel()->GetChild(panel, i), name, depth + 1))
			return found;
	}
	return 0;
}

//! The child panel of dialog named name (any visibility), 0 if none
vgui2::VPANEL FindChild(vgui2::VPANEL dialog, const char *name)
{
	const int count = vgui2::ipanel()->GetChildCount(dialog);
	for (int i = 0; i < count; i++)
	{
		vgui2::VPANEL child = vgui2::ipanel()->GetChild(dialog, i);
		const char *n = vgui2::ipanel()->GetName(child);
		if (n && !Q_stricmp(n, name))
			return child;
	}
	return 0;
}

//! A label's text (Label::RequestInfo "GetText"), works across modules
std::wstring LabelText(vgui2::VPANEL label)
{
	std::wstring out;
	if (!label)
		return out;
	KeyValues *kv = new KeyValues("GetText");
	if (vgui2::ipanel()->RequestInfo(label, kv))
	{
		const wchar_t *w = kv->GetWString("text", L"");
		if (w)
			out = w;
	}
	kv->deleteThis();
	// trim
	while (!out.empty() && (out.back() == L' ' || out.back() == L'\n' || out.back() == L'\r'))
		out.pop_back();
	size_t s = 0;
	while (s < out.size() && out[s] == L' ')
		s++;
	return out.substr(s);
}

//! GameUI's window in its error layout (LoadingDialogError*.res): no progress bar, the reason in InfoLabel
bool DialogShowsError(vgui2::VPANEL dialog)
{
	const vgui2::VPANEL progress = FindChild(dialog, "Progress");
	return progress && !vgui2::ipanel()->IsVisible(progress) && !LabelText(FindChild(dialog, "InfoLabel")).empty();
}

bool ReadTextFile(const char *path, std::string &out, unsigned maxBytes)
{
	out.clear();
	if (!g_pFullFileSystem || !g_pFullFileSystem->FileExists(path))
		return false;
	FileHandle_t fh = g_pFullFileSystem->Open(path, "rb");
	if (!fh)
		return false;
	unsigned size = g_pFullFileSystem->Size(fh);
	if (size > maxBytes)
		size = maxBytes;
	out.resize(size);
	if (size > 0 && g_pFullFileSystem->Read(&out[0], (int)size, fh) != (int)size)
		out.clear();
	g_pFullFileSystem->Close(fh);
	return !out.empty();
}

//! Text lines of a file: control characters out, trimmed (UTF-8 kept)
std::vector<std::string> SplitLines(const std::string &text)
{
	std::vector<std::string> lines;
	std::string cur;
	for (char c : text)
	{
		if (c == '\n')
		{
			lines.push_back(cur);
			cur.clear();
		}
		else if ((unsigned char)c >= 32 || c == '\t')
			cur += c == '\t' ? ' ' : c;
	}
	lines.push_back(cur);
	for (std::string &l : lines)
	{
		while (!l.empty() && l.back() == ' ')
			l.pop_back();
		size_t s = 0;
		while (s < l.size() && l[s] == ' ')
			s++;
		l = l.substr(s);
	}
	return lines;
}

//! The texture scaled to cover x, y, w, h (keeping its shape, centered); the panel being painted clips what is over
void DrawCover(int texture, int tw, int th, int x, int y, int w, int h)
{
	const float f = std::max((float)w / tw, (float)h / th);
	const int dw = (int)ceilf(tw * f), dh = (int)ceilf(th * f);
	const int dx = x + (w - dw) / 2, dy = y + (h - dh) / 2;
	vgui2::surface()->DrawSetTexture(texture);
	vgui2::surface()->DrawSetColor(255, 255, 255, 255);
	vgui2::surface()->DrawTexturedRect(dx, dy, dx + dw, dy + dh);
}

std::string MapOfLevel(const char *level)
{
	// "maps/co_siege.bsp" -> "co_siege"
	if (!level || !level[0])
		return "";
	const char *base = strrchr(level, '/');
	base = base ? base + 1 : level;
	std::string s = base;
	const size_t dot = s.rfind('.');
	if (dot != std::string::npos)
		s.erase(dot);
	return s;
}
} // namespace

//-----------------------------------------------------------------------------
// The big map picture: its own panel, so the picture cropped to the box is clipped there
//-----------------------------------------------------------------------------
class CSboLoadingPicture : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboLoadingPicture, vgui2::Panel);

public:
	CSboLoadingPicture(CSboLoadingScreen *pScreen)
	    : BaseClass(pScreen, "MapPicture")
	    , m_pScreen(pScreen)
	{
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
	}
	void PaintBackground() override {}
	void PaintBorder() override {}
	void Paint() override { m_pScreen->PaintPicture(0, 0, GetWide(), GetTall()); }

private:
	CSboLoadingScreen *m_pScreen;
};

//-----------------------------------------------------------------------------
// CSboLoadingScreen
//-----------------------------------------------------------------------------
CSboLoadingScreen::CSboLoadingScreen(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboLoadingScreen")
{
	// a Frame on the GameUI viewport, as the server browser (never a child of GameUI's panels); title bar, grips and
	// moving off (drawn here)
	SetTitleBarVisible(false);
	SetMoveable(false);
	SetSizeable(false);
	SetMenuButtonVisible(false);
	SetCloseButtonVisible(false);
	SetProportional(true); // the style's scale helper (screen height / 480)
	SetScheme(BrowserScheme());
	UpdateStyle(this);
	SetVisible(false);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(true);
	SetPaintBackgroundEnabled(true);
	m_pCancel = new CSboBrowserButton(this, "Cancel", "Cancel", "Cancel", CSboBrowserButton::KIND_NORMAL);
	m_pPicture = new CSboLoadingPicture(this);
	// every IVGui::RunFrame: the engine runs one at the start of each paint (see the header). A tick is posted in one
	// RunFrame and handled in the next one's message dispatch, before that paint; the very first RunFrame has none
	// yet, so one is posted here (a launch straight into a server opens GameUI's window before the first paint).
	vgui2::ivgui()->AddTickSignal(GetVPanel(), 0);
	vgui2::ivgui()->PostMessage(GetVPanel(), new KeyValues("Tick"), GetVPanel());
}

void CSboLoadingScreen::OnTick()
{
	VGui2_CreateServer_ApplyPending();
	Frame();
}

CSboLoadingScreen::~CSboLoadingScreen()
{
	CancelQueries();
}

void CSboLoadingScreen::SetPendingServer(const SboServerRow &row)
{
	m_Pending = row;
	m_flPendingTime = vgui2::system()->GetCurrentTime();
	// the browser connects: the screen takes over at once, so no frame shows the menu between the browser closing
	// and GameUI's first loading window (a connect that never starts ends it after PREDIALOG_MAX)
	if (!m_bActive && SboExperimentalMenus())
	{
		Begin(0);
		Frame();
	}
}

void CSboLoadingScreen::CancelQueries()
{
	if (ISteamMatchmakingServers *pServers = ClientSteamContext().SteamMatchmakingServers())
	{
		if (m_hPing != HSERVERQUERY_INVALID)
			pServers->CancelServerQuery(m_hPing);
		if (m_hPlayers != HSERVERQUERY_INVALID)
			pServers->CancelServerQuery(m_hPlayers);
	}
	m_hPing = m_hPlayers = HSERVERQUERY_INVALID;
	m_Rules.Cancel();
	m_flRulesAt = 0.0;
}

void CSboLoadingScreen::Begin(vgui2::VPANEL dialog)
{
	const double now = vgui2::system()->GetCurrentTime();
	m_bActive = true;
	m_bFailed = false;
	m_nPaintsAtClaim = m_nPaints;
	m_nTicksNoPaint = 0;
	m_flClaimTime = now;
	m_bHudSinceDialog = false;
	m_flDisconnectedSince = -1.0;
	m_bSawDialog = dialog != 0;
	m_flLastDialog = now;
	// the same server again within a few seconds (GameUI's next window after downloads, a retry): keep the data
	unsigned ip = 0;
	unsigned short port = 0;
	bool loopback = false;
	sbo::GetServerAddress(ip, port, loopback);
	if (now - m_flEndTime < 5.0 && m_bHaveAddress && sbo::IsConnected() && !loopback && ip && ip == m_Server.ip)
	{
		ResetStyle(this);
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		SetBounds(0, 0, sw, sh);
		m_hDialog = dialog;
		m_hDialogH = dialog ? vgui2::ivgui()->PanelToHandle(dialog) : 0;
		m_bDropped = false;
		InvalidateLayout(true);
		Activate();
		return;
	}
	m_flBeginTime = now;
	m_iStage = STAGE_CONNECT;
	m_bError = false;
	// a connect shows GameUI's window with its progress first (this screen sees it before every paint), so a window
	// that is in the error layout at once is a game the server ended: "Disconnected", not "Could not join"
	m_bDropped = dialog && DialogShowsError(dialog);
	for (int i = 0; i < 4; i++)
		m_rPlayers[i] = m_rAbout[i] = 0;
	m_Info.clear();
	m_File.clear();
	m_TimeLeft.clear();
	m_LastFile.clear();
	m_nFiles = 0;
	m_flFilePercent = -1.0f;
	ResetServer();
	m_BeginLevel = MapOfLevel(sbo::GetLevelName()); // the level before this connect (not this connect's map)
	m_BeginHostMap.clear();
	if (sbo::CvarHandle h = sbo::FindCvar("HostMap"))
		m_BeginHostMap = sbo::CvarGetString(h);
	m_bBeginHadAddr = sbo::IsConnected() && sbo::GetServerAddress(m_uBeginIp, m_uBeginPort, m_bBeginLoopback);

	// connected from the browser: its row (name, map, players) until the server answers again; started from Create
	// server: the map it starts (the local game's other data come from its cvars)
	if (m_flPendingTime >= 0.0 && m_flBeginTime - m_flPendingTime < PENDING_MAX_AGE && m_Pending.ip)
	{
		m_Server = m_Pending;
		m_bHaveAddress = true;
		SetMap(m_Server.map);
	}
	else if (m_flPendingTime >= 0.0 && m_flBeginTime - m_flPendingTime < PENDING_MAX_AGE && !m_Pending.map.empty())
	{
		m_bHaveAddress = true;
		m_bLocal = true;
		SetMap(m_Pending.map);
	}
	m_flPendingTime = -1.0;

	PickTip();
	ResetStyle(this); // the browser may have made the look smaller for its narrow-screen case
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	SetBounds(0, 0, sw, sh);
	m_hDialog = dialog;
	m_hDialogH = dialog ? vgui2::ivgui()->PanelToHandle(dialog) : 0;
	InvalidateLayout(true);
	Activate(); // visible, in front, keyboard focus
}

void CSboLoadingScreen::ResetServer()
{
	CancelQueries();
	m_bHaveAddress = m_bLocal = m_bHaveInfo = m_bInfoFailed = false;
	m_iFriendlyFire = -1;
	m_Players.clear();
	m_iPlayersScroll = 0;
	m_bPlayersDone = false;
	m_Server = SboServerRow();
	m_Map.clear();
	m_Briefing.clear();
}

void CSboLoadingScreen::Release(bool toDialog)
{
	// the paint and the input back: to GameUI's window while it is up, else to nobody (the engine's normal paint)
	// a handle, not the VPANEL: GameUI may have deleted that window since
	const vgui2::VPANEL last = m_hLastDialogH ? vgui2::ivgui()->HandleToPanel(m_hLastDialogH) : 0;
	const vgui2::VPANEL to = toDialog && last && vgui2::ipanel()->IsVisible(last) ? last : 0;
	vgui2::surface()->RestrictPaintToSinglePanel(to);
	if (vgui2::input()->GetAppModalSurface() == GetVPanel())
	{
		if (to)
			vgui2::input()->SetAppModalSurface(to);
		else
			vgui2::input()->ReleaseAppModalSurface();
	}
}

void CSboLoadingScreen::End()
{
	if (!m_bFailed)
		Release(false);
	m_hLastDialog = 0;
	m_hLastDialogH = 0;
	m_bActive = false;
	m_flEndTime = vgui2::system()->GetCurrentTime();
	SetVisible(false);
	m_hDialog = 0;
	m_hDialogH = 0;
	CancelQueries();
	SetVisible(false);
}

void CSboLoadingScreen::StartQueries()
{
	ISteamMatchmakingServers *pServers = ClientSteamContext().SteamMatchmakingServers();
	if (!pServers || !m_Server.ip || m_hPing != HSERVERQUERY_INVALID)
		return;
	m_hPing = pServers->PingServer(m_Server.ip, m_Server.queryPort ? m_Server.queryPort : m_Server.connPort, this);
}

void CSboLoadingScreen::Frame()
{
	const double now = vgui2::system()->GetCurrentTime();
	// the game draws its HUD (gHUD.m_flTime moves only in HUD_Redraw)
	const float hudTime = sbo::GetHudTime();
	const bool hudDrawn = hudTime != m_flHudTime;
	m_flHudTime = hudTime;

	// cl_experimental_menus 0: GameUI's own window, as before (read at the start of each connect)
	if (!m_bActive && !SboExperimentalMenus())
		return;

	// GameUI's loading dialog, in use (its own visible flag)
	vgui2::VPANEL dialog = 0;
	if (g_pEngineVGui)
		dialog = FindNamed(g_pEngineVGui->GetPanel(PANEL_GAMEUIDLL), "LoadingDialog", 0);
	if (dialog && !vgui2::ipanel()->IsVisible(dialog))
		dialog = 0;
	if (dialog)
	{
		m_flLastDialog = now;
		m_bHudSinceDialog = false;
		m_flDisconnectedSince = -1.0;
		m_bSawDialog = true;
	}
	else if (m_bActive)
	{
		// GameUI closes its window between stages and opens another, and reconnects after downloads, a map change
		// and retry: the screen stays up until the game is entered or the connect is over
		if (hudDrawn && m_bSawDialog)
			m_bHudSinceDialog = true;
		if (sbo::IsConnected())
			m_flDisconnectedSince = -1.0;
		else if (m_flDisconnectedSince < 0.0)
			m_flDisconnectedSince = now;
		const bool disconnected = m_flDisconnectedSince >= 0.0 &&
		    now - m_flDisconnectedSince > (m_bSawDialog ? DISCONNECTED_END : PREDIALOG_MAX);
		if (m_bHudSinceDialog || disconnected || now - m_flLastDialog > GAP_MAX)
			End();
		if (!m_bActive)
			return;
	}
	if (!dialog && !m_bActive)
		return;
	if (!m_bActive)
		Begin(dialog);
	if (dialog)
	{
		m_hLastDialog = dialog;
		m_hLastDialogH = vgui2::ivgui()->PanelToHandle(dialog);
	}
	m_hDialog = dialog; // 0 in a gap: no texts to read, the last ones stay
	m_hDialogH = dialog ? vgui2::ivgui()->PanelToHandle(dialog) : 0;
	if (m_bFailed)
		return; // this screen did not paint: GameUI's window has the paint and the input back for this connect

	// While it connects the engine paints only one panel's tree (BaseUISurface::PaintTraverse's restricted panel:
	// GameUI's loading window) and that window is app-modal. This screen takes both, so it is painted (at every
	// stage, the map load too) and gets Esc / Cancel; GameUI's window keeps running untouched, only not painted.
	// Only if it really does not paint (ticks, each right before an engine paint, without one paint of the screen)
	// both go back to GameUI's window, shown as before. A long load without paints does not run ticks either.
	if (m_nPaints != m_nPaintsAtClaim)
	{
		m_nPaintsAtClaim = m_nPaints;
		m_nTicksNoPaint = 0;
		m_flClaimTime = now;
	}
	else if (++m_nTicksNoPaint >= FALLBACK_TICKS && now - m_flClaimTime > FALLBACK_SECONDS)
	{
		Release(true);
		m_bFailed = true;
		SetVisible(false);
		sbo::DevPrint("Joining screen: not drawn, GameUI's loading window is shown for this connect\n");
		return;
	}
	if (!IsVisible())
		SetVisible(true);
	MoveToFront();
	vgui2::surface()->RestrictPaintToSinglePanel(GetVPanel()); // the surface has no getter: set on every tick
	if (vgui2::input()->GetAppModalSurface() != GetVPanel())
		vgui2::input()->SetAppModalSurface(GetVPanel());

	// the server: the address the engine connects to, read only while it is connected (right after a "disconnect"
	// it still names the server left, e.g. Create server started from a game on another server). A different address
	// than the screen's (a new connect, Create server's local game, a connect made from the console) starts its data
	// afresh: name, map, players and queries are the new target's.
	{
		unsigned ip = 0;
		unsigned short port = 0;
		bool loopback = false;
		// the connection the engine had when this connect began (still there for a moment when Create server or the
		// browser start one from inside a game) is not the target until the new server info has come
		if (sbo::IsConnected() && sbo::GetServerAddress(ip, port, loopback) &&
		    !(m_bBeginHadAddr && loopback == m_bBeginLoopback && (loopback || (ip == m_uBeginIp && port == m_uBeginPort)) &&
		        m_iStage < STAGE_DOWNLOAD))
		{
			const bool same = m_bHaveAddress && loopback == m_bLocal &&
			                  (loopback || (ip == m_Server.ip && port == m_Server.connPort));
			if (!same)
			{
				if (m_bHaveAddress)
					ResetServer(); // the screen had another server's data
				m_bHaveAddress = true;
				m_bLocal = loopback;
				if (!loopback)
				{
					m_Server.ip = ip;
					m_Server.connPort = m_Server.queryPort = port;
				}
			}
		}
	}
	// a game this client starts from the console (`map`, a listen server's `changelevel`): the engine spawns the server
	// and loads the map before the client connects, so neither the address nor the level name tell anything until the
	// load is nearly done; the command itself (or `map`'s HostMap cvar) names the map from the first paint (the texts
	// of GameUI's window are from the last tick)
	if (!m_bError && m_Map.empty() && !(m_bHaveAddress && !m_bLocal))
	{
		const std::string map = StartingMap();
		if (!map.empty())
		{
			m_bHaveAddress = m_bLocal = true;
			SetMap(map);
		}
	}
	if (m_bHaveAddress && !m_bLocal && !m_bHaveInfo && !m_bInfoFailed)
		StartQueries();
	if (m_bLocal)
	{
		// a game this client hosts: its name and slots from the server's own cvars (Create server sets them before
		// "map"), the local player
		if (sbo::CvarHandle h = sbo::FindCvar("hostname"))
			m_Server.name = CleanName(sbo::CvarGetString(h));
		if (sbo::CvarHandle h = sbo::FindCvar("maxplayers"))
			m_Server.maxPlayers = std::max(1, (int)sbo::CvarGetFloat(h));
		m_Server.humans = m_Server.players = 1;
	}

	// the map: the server's (A2S) or the one Create server starts, else the engine's level once it is the new one (the
	// level name still names the old map until the new server info: taken when it changed or once the server info
	// stage has passed)
	if (m_Map.empty())
	{
		const std::string level = MapOfLevel(sbo::GetLevelName());
		if (!level.empty() && (level != m_BeginLevel || m_iStage >= STAGE_DOWNLOAD))
			SetMap(level);
	}

	if (m_flRulesAt > 0.0 && now >= m_flRulesAt)
	{
		m_flRulesAt = 0.0;
		m_Rules.Start(m_Server.ip, m_Server.queryPort ? m_Server.queryPort : m_Server.connPort, now);
	}
	if (m_Rules.IsRunning() && m_Rules.Poll(now) && m_Rules.Succeeded())
	{
		for (const auto &rule : m_Rules.GetRules())
		{
			if (!Q_stricmp(rule.first.c_str(), "mp_friendlyfire"))
				m_iFriendlyFire = atof(rule.second.c_str()) != 0.0 ? 1 : 0;
		}
	}

	if (m_Pictures.Poll())
		Repaint();
	if (!m_Map.empty() && !m_Pictures.IsDownloading())
		m_Pictures.StartDownload(Lower(m_Map));

	ReadDialog();
	m_pCancel->SetText(m_bError ? "Close" : "Cancel");
	InvalidateLayout();
	Repaint();
}

void CSboLoadingScreen::ReadDialog()
{
	// GameUI deletes its window between stages: the VPANEL can be gone by the time this runs (Paint comes after the
	// tick that found it), so the panel is taken from its handle, which reads 0 once the panel is deleted
	const vgui2::VPANEL dialogPanel = m_hDialogH ? vgui2::ivgui()->HandleToPanel(m_hDialogH) : 0;
	if (!dialogPanel)
		return;
	const vgui2::VPANEL progress = FindChild(dialogPanel, "Progress");
	const vgui2::VPANEL progress2 = FindChild(dialogPanel, "Progress2");
	m_Info = LabelText(FindChild(dialogPanel, "InfoLabel"));
	// the error layout (LoadingDialogError*.res): no progress bar, the reason in InfoLabel
	m_bError = progress && !vgui2::ipanel()->IsVisible(progress) && !m_Info.empty();
	if (m_bError)
		return;
	const int stage = StageOfText(m_Info);
	if (stage >= 0)
		m_iStage = stage; // the engine's current stage (a reconnect after downloads starts again at Connecting)
	const bool dual = progress2 && vgui2::ipanel()->IsVisible(progress2);
	if (dual)
	{
		if (m_iStage < STAGE_DOWNLOAD)
			m_iStage = STAGE_DOWNLOAD;
		const vgui2::VPANEL fileLabel = FindChild(dialogPanel, "SecondaryProgressLabel");
		std::wstring file = fileLabel && vgui2::ipanel()->IsVisible(fileLabel) ? LabelText(fileLabel) : L"";
		if (file == L"Label")
			file.clear(); // the .res placeholder
		// the engine writes "Downloading file 'x'" and then "File 'x'" for the same file: the name between the quotes
		const size_t q1 = file.find(L'\''), q2 = q1 == std::wstring::npos ? q1 : file.rfind(L'\'');
		if (q1 != std::wstring::npos && q2 > q1 + 1)
			file = file.substr(q1 + 1, q2 - q1 - 1);
		if (!file.empty() && file != m_LastFile)
		{
			m_LastFile = file;
			m_nFiles++;
		}
		m_File = file;
		const vgui2::VPANEL timeLabel = FindChild(dialogPanel, "TimeRemainingLabel");
		m_TimeLeft = timeLabel && vgui2::ipanel()->IsVisible(timeLabel) ? LabelText(timeLabel) : L"";
	}
	else
	{
		m_File.clear();
		m_TimeLeft.clear();
	}
	// the current file's percent: the engine's scr_downloading (-1 when no download runs)
	m_flFilePercent = -1.0f;
	if (sbo::CvarHandle cvar = sbo::FindCvar("scr_downloading"))
	{
		const float pct = sbo::CvarGetFloat(cvar);
		if (dual && pct >= 0.0f && pct <= 100.0f)
			m_flFilePercent = pct;
	}
}

std::string CSboLoadingScreen::StartingMap() const
{
	auto clean = [](std::string m) {
		// "maps/co_x.bsp" or "co_x" -> "co_x" (only the .bsp goes: map names may have dots)
		if (!Q_strnicmp(m.c_str(), "maps/", 5))
			m.erase(0, 5);
		if (m.size() > 4 && !Q_stricmp(m.c_str() + m.size() - 4, ".bsp"))
			m.resize(m.size() - 4);
		return Lower(m);
	};
	// the console command being run: the engine paints its loading window from inside `map` / `changelevel` (HL25
	// Host_Map_f, Host_Changelevel_f), and the engine's command arguments are still that command's
	if (sbo::Cmd_Argc() >= 2)
	{
		const char *cmd = sbo::Cmd_Argv(0);
		if (cmd && (!Q_stricmp(cmd, "map") || !Q_stricmp(cmd, "changelevel")))
			return clean(sbo::Cmd_Argv(1) ? sbo::Cmd_Argv(1) : "");
	}
	// `map` sets the engine's HostMap cvar to its map right before it spawns the server, after GameUI's "Starting
	// server" (Host_Map_f); until it changed it still names an earlier game's map
	if (StartsWithToken(m_Info, "#GameUI_StartingServer"))
	{
		if (sbo::CvarHandle h = sbo::FindCvar("HostMap"))
		{
			const std::string hostMap = sbo::CvarGetString(h);
			if (!hostMap.empty() && hostMap != m_BeginHostMap)
				return clean(hostMap);
		}
	}
	return "";
}

void CSboLoadingScreen::SetMap(const std::string &map)
{
	if (map.empty() || map == m_Map || !SafeMapName(map))
		return;
	m_Map = map;
	m_Server.map = map;
	m_Server.mode = ModeOfMap(map.c_str());
	LoadBriefing();
}

void CSboLoadingScreen::LoadBriefing()
{
	m_Briefing.clear();
	m_iAboutScroll = 0;
	std::string text;
	char path[128];
	snprintf(path, sizeof(path), "maps/%s.txt", Lower(m_Map).c_str());
	if (!ReadTextFile(path, text, BRIEFING_MAX_BYTES))
		return;
	// the map's own text (the MOTD-style briefing SBO maps ship): its first paragraphs
	for (const std::string &line : SplitLines(text))
	{
		if (m_Briefing.size() >= 400) // the whole text scrolls; a sane cap
			break;
		if (line.empty() && (m_Briefing.empty() || m_Briefing.back().empty()))
			continue;
		m_Briefing.push_back(Widen(CleanName(line.c_str()).c_str()));
	}
	while (!m_Briefing.empty() && m_Briefing.back().empty())
		m_Briefing.pop_back();
}

void CSboLoadingScreen::PickTip()
{
	m_Tip.clear();
	std::string text;
	if (!ReadTextFile("resource/loadingtips.txt", text, 32 * 1024))
		return;
	std::vector<std::string> tips;
	for (const std::string &line : SplitLines(text))
	{
		if (!line.empty() && line.compare(0, 2, "//") != 0 && (int)tips.size() < TIPS_MAX)
			tips.push_back(line);
	}
	if (tips.empty())
		return;
	static unsigned s_uSeed = (unsigned)time(nullptr);
	s_uSeed = s_uSeed * 1103515245u + 12345u;
	m_Tip = Widen(tips[(s_uSeed >> 16) % tips.size()].c_str());
}

//-----------------------------------------------------------------------------
// A2S answers
//-----------------------------------------------------------------------------
void CSboLoadingScreen::ServerResponded(gameserveritem_t &server)
{
	m_hPing = HSERVERQUERY_INVALID;
	m_bHaveInfo = true;
	m_Server.name = CleanName(server.GetName());
	m_Server.players = server.m_nPlayers;
	m_Server.bots = server.m_nBotPlayers;
	m_Server.humans = std::max(0, server.m_nPlayers - server.m_nBotPlayers);
	m_Server.maxPlayers = server.m_nMaxPlayers;
	m_Server.ping = server.m_nPing;
	m_Server.password = server.m_bPassword;
	if (server.m_NetAdr.GetQueryPort())
		m_Server.queryPort = server.m_NetAdr.GetQueryPort();
	std::string map = Lower(server.m_szMap);
	if (!map.empty() && map != m_Map)
	{
		m_Map.clear();
		SetMap(map);
	}
	// then the player list, then the rules (one query at a time, as the browser)
	if (ISteamMatchmakingServers *pServers = ClientSteamContext().SteamMatchmakingServers())
		m_hPlayers = pServers->PlayerDetails(m_Server.ip, m_Server.queryPort ? m_Server.queryPort : m_Server.connPort, this);
	Repaint();
}

void CSboLoadingScreen::ServerFailedToRespond()
{
	m_hPing = HSERVERQUERY_INVALID;
	m_bInfoFailed = true; // the browser's row (if any) and the engine's texts stay
}

void CSboLoadingScreen::AddPlayerToList(const char *pchName, int nScore, float flTimePlayed)
{
	std::string name = CleanName(pchName);
	if (!name.empty() && m_Players.size() < 64)
		m_Players.push_back(name);
}

void CSboLoadingScreen::PlayersFailedToRespond()
{
	m_hPlayers = HSERVERQUERY_INVALID;
	m_bPlayersDone = true;
	m_flRulesAt = vgui2::system()->GetCurrentTime() + RULES_DELAY;
}

void CSboLoadingScreen::PlayersRefreshComplete()
{
	m_hPlayers = HSERVERQUERY_INVALID;
	m_bPlayersDone = true;
	m_flRulesAt = vgui2::system()->GetCurrentTime() + RULES_DELAY;
	Repaint();
}

//-----------------------------------------------------------------------------
// Input
//-----------------------------------------------------------------------------
void CSboLoadingScreen::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "Cancel"))
	{
		// GameUI's own button command on its dialog: "Cancel" while loading, "Close" for an error
		const vgui2::VPANEL dialogPanel = m_hDialogH ? vgui2::ivgui()->HandleToPanel(m_hDialogH) : 0;
		if (dialogPanel)
			vgui2::ivgui()->PostMessage(dialogPanel, new KeyValues("Command", "command", m_bError ? "Close" : "Cancel"), GetVPanel());
		else
			sbo::ExecClientCmd("disconnect\n"); // between two GameUI windows: what GameUI's Cancel runs
		return;
	}
	BaseClass::OnCommand(command);
}

void CSboLoadingScreen::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ESCAPE)
		OnCommand("Cancel");
	else
		BaseClass::OnKeyCodeTyped(code);
}

void CSboLoadingScreen::OnScreenSizeChanged(int oldWide, int oldTall)
{
	BaseClass::OnScreenSizeChanged(oldWide, oldTall);
	ResetStyle(this);
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	SetBounds(0, 0, sw, sh);
	InvalidateLayout(true);
}

//-----------------------------------------------------------------------------
// Layout and drawing: the design's 1280 x 800 screen, 40 from the edges
//-----------------------------------------------------------------------------
void CSboLoadingScreen::PerformLayout()
{
	BaseClass::PerformLayout();
	const Style &st = S();
	const int W = GetWide(), H = GetTall();
	if (m_bDropped)
	{
		// the Disconnected window: centered, the caption, the title, up to four lines of the reason, Close at the
		// bottom right (the new menus' window look)
		const int lineTall = FontTall(st.body) + st.M(4);
		const int bw = m_pCancel->GetContentWide() + 2 * st.M(8), bh = st.M(44);
		const int boxW = std::min(W - 2 * st.M(16), st.M(560));
		const int boxH = st.M(24) + FontTall(st.body) + st.M(4) + FontTall(st.title) + st.M(16) + 4 * lineTall +
		                 st.M(16) + bh + st.M(20);
		m_rBox[0] = (W - boxW) / 2;
		m_rBox[1] = std::max(0, (H - boxH) / 2);
		m_rBox[2] = boxW;
		m_rBox[3] = boxH;
		m_pPicture->SetVisible(false);
		m_pCancel->SetBounds(m_rBox[0] + boxW - st.M(24) - bw, m_rBox[1] + boxH - st.M(20) - bh, bw, bh);
		return;
	}
	const int pad = st.M(40), boxPadY = st.M(14), boxPadX = st.M(16);
	// bottom box: two progress lines, the tip under them
	const int lineTall = FontTall(st.body);
	const int barsTall = lineTall + st.M(6) + st.M(12) + st.M(10) + lineTall + st.M(6) + st.M(6);
	const int tipTall = m_Tip.empty() ? 0 : st.M(10) + 1 + st.M(10) + lineTall;
	m_iBottomTall = boxPadY + barsTall + tipTall + boxPadY;
	m_iBottomY = H - pad - m_iBottomTall;
	m_iMainY = pad + FontTall(st.body) + st.M(4) + FontTall(st.title) + st.M(6) + FontTall(st.tab) + st.M(16);
	const int width = W - 2 * pad, gap = st.M(16);
	const int sideW = (width - gap) * 10 / 26, picW = width - gap - sideW;
	const int mainTall = m_iBottomY - gap - m_iMainY;
	m_pPicture->SetBounds(pad, m_iMainY, picW, std::max(0, mainTall));
	m_pPicture->SetVisible(mainTall > st.M(60));
	const int bw = m_pCancel->GetContentWide() + 2 * st.M(8), bh = st.M(44);
	m_pCancel->SetBounds(W - pad - boxPadX - bw, m_iBottomY + boxPadY + (barsTall - bh) / 2, bw, bh);
}

void CSboLoadingScreen::PaintBackground()
{
	PaintBackdrop();
}

void CSboLoadingScreen::PaintBackdrop()
{
	const int W = GetWide(), H = GetTall();
	FillRect(0, 0, W, H, Color(16, 20, 16, 255)); // #101410
	const CSboMapPictures::Picture *pPic = m_Map.empty() ? nullptr : m_Pictures.Get(Lower(m_Map));
	if (pPic && pPic->texture != -1 && pPic->isMapImage && pPic->wide > 0 && pPic->tall > 0)
	{
		// the map picture filling the screen (cropped to its shape: the panel clips it)
		DrawCover(pPic->texture, pPic->wide, pPic->tall, 0, 0, W, H);
	}
	// the design's shade: rgba(10,14,10) 0.2 at the top, 0.85 at 60 %, 0.97 at the bottom
	const int bands = 48;
	for (int i = 0; i < bands; i++)
	{
		const float f = (i + 0.5f) / bands;
		const float a = f < 0.6f ? 0.2f + (0.85f - 0.2f) * (f / 0.6f) : 0.85f + (0.97f - 0.85f) * ((f - 0.6f) / 0.4f);
		const int y0 = H * i / bands, y1 = H * (i + 1) / bands;
		FillRect(0, y0, W, y1 - y0, Color(10, 14, 10, (int)(a * 255)));
	}
}

void CSboLoadingScreen::PaintPicture(int x, int y, int w, int h)
{
	FillRect(x, y, w, h, COL_BG_INPUT);
	const CSboMapPictures::Picture *pPic = m_Map.empty() ? nullptr : m_Pictures.Get(Lower(m_Map));
	if (pPic && pPic->texture != -1 && pPic->wide > 0 && pPic->tall > 0)
	{
		vgui2::surface()->DrawSetTexture(pPic->texture);
		vgui2::surface()->DrawSetColor(255, 255, 255, 255);
		if (pPic->isMapImage)
			DrawCover(pPic->texture, pPic->wide, pPic->tall, x + 1, y + 1, w - 2, h - 2); // fills the box, as the design's picture
		else
		{
			// the overview: whole, centered
			const float fit = std::min((float)(w - 2) / pPic->wide, (float)(h - 2) / pPic->tall);
			const int dw = (int)(pPic->wide * fit), dh = (int)(pPic->tall * fit);
			vgui2::surface()->DrawTexturedRect(x + (w - dw) / 2, y + (h - dh) / 2, x + (w - dw) / 2 + dw, y + (h - dh) / 2 + dh);
		}
	}
	OutlineRect(x, y, w, h, COL_BORDER_LIST);
	if (!m_Map.empty())
	{
		// the map name on a dark strip in the bottom left corner
		const std::wstring name = Widen(m_Map.c_str());
		const int px = S().M(8), py = S().M(3);
		const int tw = TextWide(S().small, name), th = FontTall(S().small);
		const int lx = x + S().M(12), ly = y + h - S().M(10) - th - 2 * py;
		FillRect(lx, ly, tw + 2 * px, th + 2 * py, Color(0, 0, 0, 115));
		DrawText(S().small, COL_TEXT_PRIMARY, lx + px, ly + py, name);
	}
}

void CSboLoadingScreen::PaintCard(int x, int y, int w, int h, const char *heading)
{
	FillRect(x, y, w, h, COL_BG_WINDOW);
	OutlineRect(x, y, w, h, COL_BORDER_LIST);
	DrawSpacedUpper(S().small, COL_TEXT_DIM, x + S().M(14), y + S().M(14), heading);
}

int CSboLoadingScreen::DrawWrapped(vgui2::HFont font, Color color, int x, int y, int w, const std::wstring &text, int maxLines)
{
	const int lineTall = FontTall(font) + S().M(4);
	int lines = 0;
	size_t pos = 0;
	while (pos < text.size() && lines < maxLines)
	{
		// the longest run of words that fits
		size_t end = pos, lastFit = pos;
		while (end <= text.size())
		{
			size_t next = text.find(L' ', end);
			if (next == std::wstring::npos)
				next = text.size();
			if (TextWide(font, text.substr(pos, next - pos)) > w)
				break;
			lastFit = next;
			if (next >= text.size())
				break;
			end = next + 1;
		}
		if (lastFit == pos)
		{
			// one word wider than the line: cut it
			lastFit = text.find(L' ', pos);
			if (lastFit == std::wstring::npos)
				lastFit = text.size();
		}
		std::wstring line = text.substr(pos, lastFit - pos);
		const bool more = lastFit < text.size();
		if (lines == maxLines - 1 && more)
			line += L" ...";
		DrawText(font, color, x, y + lines * lineTall, line, w);
		lines++;
		pos = lastFit;
		while (pos < text.size() && text[pos] == L' ')
			pos++;
	}
	return lines * lineTall;
}

std::vector<std::wstring> CSboLoadingScreen::WrapLines(vgui2::HFont font, const std::wstring &text, int w)
{
	std::vector<std::wstring> out;
	size_t pos = 0;
	while (pos < text.size())
	{
		size_t end = pos, lastFit = pos;
		while (end <= text.size())
		{
			size_t next = text.find(L' ', end);
			if (next == std::wstring::npos)
				next = text.size();
			if (TextWide(font, text.substr(pos, next - pos)) > w)
				break;
			lastFit = next;
			if (next >= text.size())
				break;
			end = next + 1;
		}
		if (lastFit == pos)
		{
			lastFit = text.find(L' ', pos);
			if (lastFit == std::wstring::npos)
				lastFit = text.size();
		}
		out.push_back(text.substr(pos, lastFit - pos));
		pos = lastFit;
		while (pos < text.size() && text[pos] == L' ')
			pos++;
	}
	return out;
}

int CSboLoadingScreen::PaintScrolled(const std::vector<std::wstring> &lines, int first, int x, int y, int w, int h,
    int lineTall, Color color, int *rect)
{
	const Style &st = S();
	const int fit = std::max(1, h / std::max(1, lineTall));
	const int total = (int)lines.size();
	first = std::max(0, std::min(first, std::max(0, total - fit)));
	const bool bar = total > fit;
	const int barW = st.M(6);
	for (int i = 0; i < fit && first + i < total; i++)
		DrawText(st.body, color, x, y + i * lineTall, lines[first + i], w - (bar ? barW + st.M(6) : 0));
	if (bar)
	{
		// the browser's list bar: a dark track, the thumb in the list border's green
		const int bx = x + w - barW;
		FillRect(bx, y, barW, h, COL_BG_INPUT);
		const int thumbH = std::max(st.M(12), h * fit / total);
		const int thumbY = y + (h - thumbH) * first / std::max(1, total - fit);
		FillRect(bx + 1, thumbY, barW - 2, thumbH, COL_BORDER_LIST);
	}
	rect[0] = x;
	rect[1] = y;
	rect[2] = w;
	rect[3] = h;
	return first;
}

void CSboLoadingScreen::OnMouseWheeled(int delta)
{
	int mx, my;
	vgui2::input()->GetCursorPos(mx, my);
	ScreenToLocal(mx, my);
	auto in = [mx, my](const int *r) { return r[2] > 0 && mx >= r[0] && mx < r[0] + r[2] && my >= r[1] && my < r[1] + r[3]; };
	if (in(m_rPlayers))
		m_iPlayersScroll = std::max(0, m_iPlayersScroll - delta);
	else if (in(m_rAbout))
		m_iAboutScroll = std::max(0, m_iAboutScroll - delta * 3);
	else
		return;
	Repaint();
}

void CSboLoadingScreen::PaintDropped()
{
	const Style &st = S();
	const int x = m_rBox[0], y0 = m_rBox[1], w = m_rBox[2], h = m_rBox[3];
	const int padX = st.M(24);
	FillRect(x, y0, w, h, COL_BG_WINDOW);
	OutlineRect(x, y0, w, h, COL_BORDER);
	int y = y0 + st.M(24);
	DrawSpacedUpper(st.body, COL_TEXT_DIM, x + padX, y, "Syndicate Black Ops");
	y += FontTall(st.body) + st.M(4);
	DrawText(st.title, COL_TEXT, x + padX, y, L"Disconnected", w - 2 * padX);
	y += FontTall(st.title) + st.M(16);
	// the reason, as GameUI wrote it ("You have been disconnected from the server. Reason: ...")
	std::wstring text = m_Info;
	std::replace(text.begin(), text.end(), L'\n', L' ');
	DrawWrapped(st.body, COL_TEXT_LIGHT, x + padX, y, w - 2 * padX, text, 4);
}

void CSboLoadingScreen::Paint()
{
	m_nPaints++;
	VGui2_CreateServer_ApplyPending(); // Create server's game cvars: the DLL is loaded, the server not yet spawned
	// during the map load the engine paints without running frames: the dialog's texts are read here too
	ReadDialog();
	if (m_bDropped)
	{
		PaintDropped();
		return;
	}

	const Style &st = S();
	const int W = GetWide(), H = GetTall();
	const int pad = st.M(40), gap = st.M(16);
	const int left = pad, right = W - pad, width = right - left;
	char buf[256];

	// head: caption, server name, facts
	int y = pad;
	DrawSpacedUpper(st.body, COL_TEXT_DIM, left, y, m_bError ? "Could not join" : (m_bLocal ? "Starting game" : "Joining server"));
	const int capW = SpacedUpperWide(st.body, "Syndicate Black Ops");
	DrawSpacedUpper(st.body, COL_TEXT_DIM, right - capW, y, "Syndicate Black Ops");
	y += FontTall(st.body) + st.M(4);
	std::string title = m_Server.name;
	if (title.empty())
		title = m_bLocal ? "Local game" : (m_Server.ip ? m_Server.Address() : "Connecting...");
	DrawText(st.title, COL_TEXT, left, y, Widen(title.c_str()), width - capW - gap);
	y += FontTall(st.title) + st.M(6);
	{
		std::vector<std::string> facts;
		if (!m_Map.empty())
		{
			if (m_Server.mode != SboServerRow::MODE_NONE)
				facts.push_back(ModeName(m_Server.mode));
			facts.push_back(m_Map);
		}
		if (m_bHaveInfo || m_Server.maxPlayers > 0)
		{
			if (m_Server.bots > 0)
				snprintf(buf, sizeof(buf), "%d / %d players, %d %s", m_Server.humans, m_Server.maxPlayers, m_Server.bots,
				    m_Server.bots == 1 ? "bot" : "bots");
			else
				snprintf(buf, sizeof(buf), "%d / %d %s", m_Server.humans, m_Server.maxPlayers,
				    m_Server.maxPlayers == 1 ? "player" : "players");
			facts.push_back(buf);
			if (m_Server.ping > 0)
			{
				snprintf(buf, sizeof(buf), "%d ms", m_Server.ping);
				facts.push_back(buf);
			}
		}
		if (m_iFriendlyFire >= 0)
			facts.push_back(m_iFriendlyFire ? "Friendly fire on" : "Friendly fire off");
		int x = left;
		const std::wstring dot = L"·";
		for (size_t i = 0; i < facts.size(); i++)
		{
			if (i > 0)
			{
				x += st.M(10);
				x += DrawText(st.tab, COL_TEXT_FAINT, x, y, dot);
				x += st.M(10);
			}
			x += DrawText(st.tab, COL_TEXT_LIGHT, x, y, Widen(facts[i].c_str()), right - x);
		}
	}
	// main: picture (its own panel, CSboLoadingPicture) | cards
	const int mainY = m_iMainY, mainTall = m_iBottomY - gap - mainY;
	const int sideW = (width - gap) * 10 / 26, picW = width - gap - sideW;
	if (mainTall > st.M(60))
	{

		const int sx = left + picW + gap, cardPad = st.M(14);
		const int innerW = sideW - 2 * cardPad;
		const int headTall = FontTall(st.small) + st.M(8);
		const int lineTall = FontTall(st.body) + st.M(5);

		// Progress: the five steps
		const int progTall = cardPad + headTall + STAGE_COUNT * lineTall + cardPad - st.M(5);
		PaintCard(sx, mainY, sideW, progTall, "Progress");
		int ly = mainY + cardPad + headTall;
		for (int i = 0; i < STAGE_COUNT; i++)
		{
			Color c = COL_TEXT_FAINT;
			const wchar_t *mark = L"○"; // white circle
			const char *label = s_StepNow[i];
			if (m_bError)
				c = i < m_iStage ? COL_TEXT_DIM : COL_TEXT_FAINT;
			if (i < m_iStage)
			{
				c = COL_TEXT_DIM;
				mark = L"✓"; // check mark
				label = s_StepDone[i];
			}
			else if (i == m_iStage)
			{
				c = m_bError ? COL_PING_BAD : COL_TEXT_BRIGHT;
				mark = m_bError ? L"×" : L"●"; // multiplication sign / black circle
			}
			std::wstring text = Widen(label);
			if (i == STAGE_DOWNLOAD && i == m_iStage && !m_bError && m_nFiles > 0)
			{
				snprintf(buf, sizeof(buf), " (file %d)", m_nFiles);
				text += Widen(buf);
			}
			const int markW = st.M(14);
			const int mw = TextWide(st.body, mark);
			DrawText(st.body, c, sx + cardPad + (markW - mw) / 2, ly, mark);
			DrawText(st.body, c, sx + cardPad + markW + st.M(8), ly, text, innerW - markW - st.M(8));
			ly += lineTall;
		}

		// Players on the server (bottom card), About this map (the rest)
		const int cardGap = st.M(12);
		std::wstring players;
		for (const std::string &name : m_Players)
		{
			if (!players.empty())
				players += L", ";
			players += Widen(name.c_str());
		}
		const int playersTall = cardPad + headTall + 3 * lineTall + cardPad - st.M(5);
		const int playersY = mainY + mainTall - playersTall;
		m_rPlayers[2] = m_rAbout[2] = 0;
		if (!m_bLocal)
		{
			PaintCard(sx, playersY, sideW, playersTall, m_Server.bots > 0 ? "Players and bots on the server" : "Players on the server");
			const std::wstring text = !players.empty() ? players : (m_bPlayersDone || m_bInfoFailed ? L"-" : L"...");
			// all the names, three lines at a time: the wheel (or a click on the bar) scrolls them
			std::vector<std::wstring> lines = WrapLines(st.body, text, innerW - st.M(12));
			m_iPlayersScroll = PaintScrolled(lines, m_iPlayersScroll, sx + cardPad, playersY + cardPad + headTall,
			    innerW, 3 * lineTall, lineTall, COL_TEXT_LIGHT, m_rPlayers);
		}
		const int aboutY = mainY + progTall + cardGap;
		const int aboutTall = (m_bLocal ? mainY + mainTall : playersY - cardGap) - aboutY;
		if (aboutTall > headTall + 2 * cardPad)
		{
			PaintCard(sx, aboutY, sideW, aboutTall, "About this map");
			int by = aboutY + cardPad + headTall;
			const int maxY = aboutY + aboutTall - cardPad;
			if (m_Briefing.empty())
				DrawText(st.body, COL_TEXT_FAINT, sx + cardPad, by,
				    m_Map.empty() ? L"..." : L"No description for this map", innerW);
			else
			{
				// the whole text, wrapped (an empty line stays one empty line); the wheel scrolls it
				std::vector<std::wstring> lines;
				for (const std::wstring &line : m_Briefing)
				{
					if (line.empty())
					{
						lines.push_back(L"");
						continue;
					}
					for (const std::wstring &l : WrapLines(st.body, line, innerW - st.M(12)))
						lines.push_back(l);
				}
				const int areaH = std::max(lineTall, (maxY - by) / lineTall * lineTall);
				m_iAboutScroll = PaintScrolled(lines, m_iAboutScroll, sx + cardPad, by, innerW, areaH, lineTall,
				    COL_TEXT_LIGHT, m_rAbout);
			}
		}
	}

	// bottom box: overall line, file line, tip
	const int bx = left, by0 = m_iBottomY, bw = width, bh = m_iBottomTall;
	FillRect(bx, by0, bw, bh, COL_BG_WINDOW);
	OutlineRect(bx, by0, bw, bh, COL_BORDER);
	const int boxPadX = st.M(16), boxPadY = st.M(14);
	const int colX = bx + boxPadX, colW = m_pCancel->GetXPos() - st.M(20) - colX;
	int ty = by0 + boxPadY;
	const int lineH = FontTall(st.body);
	if (m_bError)
	{
		// the reason, as GameUI wrote it (it may have several lines)
		std::wstring text = m_Info;
		std::replace(text.begin(), text.end(), L'\n', L' ');
		DrawWrapped(st.body, COL_PING_BAD, colX, ty, colW, text, 3);
	}
	else
	{
		// overall: the step, steps done of five
		std::wstring stepText = Widen(s_StepNow[m_iStage]);
		snprintf(buf, sizeof(buf), "Step %d of %d", m_iStage + 1, (int)STAGE_COUNT);
		const std::wstring right1 = Widen(buf);
		const int r1w = TextWide(st.body, right1);
		DrawText(st.body, COL_TEXT_LIGHT, colX, ty, stepText, colW - r1w - st.M(12));
		DrawText(st.body, COL_TEXT_DIM, colX + colW - r1w, ty, right1);
		ty += lineH + st.M(6);
		const int barTall = st.M(12);
		FillRect(colX, ty, colW, barTall, COL_BG_INPUT);
		const int fill = (colW - 2) * m_iStage / STAGE_COUNT;
		if (fill > 0)
			FillRect(colX + 1, ty + 1, fill, barTall - 2, COL_BORDER_PRIMARY);
		OutlineRect(colX, ty, colW, barTall, COL_BORDER);
		ty += barTall + st.M(10);

		// the file: its name, GameUI's time left, the engine's percent
		std::wstring fileText = m_File;
		if (fileText.empty())
			fileText = !m_Info.empty() ? m_Info : Widen(s_StepNow[m_iStage]);
		std::wstring right2 = m_TimeLeft;
		if (m_flFilePercent >= 0.0f)
		{
			snprintf(buf, sizeof(buf), "%d %%", (int)(m_flFilePercent + 0.5f));
			right2 = right2.empty() ? Widen(buf) : Widen(buf) + L" · " + right2;
		}
		const int r2w = TextWide(st.body, right2);
		DrawText(st.body, COL_TEXT_DIM, colX, ty, fileText, colW - r2w - st.M(12));
		if (!right2.empty())
			DrawText(st.body, COL_TEXT_DIM, colX + colW - r2w, ty, right2);
		ty += lineH + st.M(6);
		const int thin = st.M(6);
		FillRect(colX, ty, colW, thin, COL_BG_INPUT);
		if (m_flFilePercent >= 0.0f)
		{
			const int f = (int)((colW - 2) * m_flFilePercent / 100.0f);
			if (f > 0)
				FillRect(colX + 1, ty + 1, f, thin - 2, COL_TEXT);
		}
		else
		{
			// no percent from the engine: a moving segment, so a long step never looks like a hang
			const double t = vgui2::system()->GetCurrentTime() - m_flBeginTime;
			const int segW = colW / 5;
			const int pos = (int)(fmod(t * 0.6, 1.0) * (colW + segW)) - segW;
			const int x0 = std::max(colX + 1, colX + pos), x1 = std::min(colX + colW - 1, colX + pos + segW);
			if (x1 > x0)
				FillRect(x0, ty + 1, x1 - x0, thin - 2, COL_TEXT);
		}
		OutlineRect(colX, ty, colW, thin, COL_BORDER);
	}

	// tip: under both columns, after a line
	if (!m_Tip.empty())
	{
		const int tipY = by0 + bh - boxPadY - lineH;
		FillRect(colX, tipY - st.M(10) - 1, bw - 2 * boxPadX, 1, COL_BORDER_LIST);
		const int tw = DrawText(st.body, COL_TEXT, colX, tipY, L"Tip:");
		DrawText(st.body, COL_TEXT_DIM, colX + tw + st.M(5), tipY, m_Tip, bw - 2 * boxPadX - tw - st.M(5));
	}
	(void)H;
}

//-----------------------------------------------------------------------------
// Hooks
//-----------------------------------------------------------------------------
namespace
{
//! Watches for GameUI's loading window from the first client frame, on VGUI2's tick (before every engine paint), and
//! makes the joining screen only when a connect shows that window. Until then nothing of the screen exists: no popup,
//! no scheme, no fonts, nothing in the panel tree while the game starts in its main menu. The watcher never paints.
class CSboLoadingWatch : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboLoadingWatch, vgui2::Panel);

public:
	explicit CSboLoadingWatch(vgui2::Panel *pParent)
	    : BaseClass(pParent, "SboLoadingWatch")
	{
		SetVisible(false);
		SetPaintEnabled(false);
		SetPaintBackgroundEnabled(false);
		SetPaintBorderEnabled(false);
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
		// as the screen did: the first RunFrame has no tick yet, so one is posted (a launch straight into a server
		// opens GameUI's window before the first paint)
		vgui2::ivgui()->AddTickSignal(GetVPanel(), 0);
		vgui2::ivgui()->PostMessage(GetVPanel(), new KeyValues("Tick"), GetVPanel());
	}

	void OnTick() override
	{
		if (s_hScreen.Get() || !g_pGameUIViewport || !g_pEngineVGui || !SboExperimentalMenus())
			return; // a made screen runs on its own tick
		vgui2::VPANEL dialog = FindNamed(g_pEngineVGui->GetPanel(PANEL_GAMEUIDLL), "LoadingDialog", 0);
		if (!dialog || !vgui2::ipanel()->IsVisible(dialog))
			return;
		s_hScreen = new CSboLoadingScreen(g_pGameUIViewport);
		s_hScreen->Frame(); // this tick, before the paint (the screen's own tick comes with the next RunFrame)
	}
};

vgui2::DHANDLE<CSboLoadingWatch> s_hWatch;
} // namespace

void VGui2_LoadingScreen_Frame()
{
	// the watcher exists from the first client frame; the screen is made when a connect needs it
	if (!g_pGameUIViewport)
		return;
	if (!s_hWatch.Get())
		s_hWatch = new CSboLoadingWatch(g_pGameUIViewport);
}

bool VGui2_LoadingScreen_DialogUp()
{
	if (s_hScreen.Get() && s_hScreen->IsActive())
		return true;
	if (!g_pEngineVGui)
		return false;
	const vgui2::VPANEL dialog = FindNamed(g_pEngineVGui->GetPanel(PANEL_GAMEUIDLL), "LoadingDialog", 0);
	return dialog && vgui2::ipanel()->IsVisible(dialog);
}

void VGui2_LoadingScreen_StartingLocal(const char *map)
{
	if (!g_pGameUIViewport || !map)
		return;
	if (!s_hScreen.Get())
		s_hScreen = new CSboLoadingScreen(g_pGameUIViewport);
	SboServerRow row; // no address: a game this client hosts
	row.map = map;
	s_hScreen->SetPendingServer(row);
}

void VGui2_LoadingScreen_Connecting(const SboServerRow &row)
{
	if (!g_pGameUIViewport)
		return;
	if (!s_hScreen.Get())
		s_hScreen = new CSboLoadingScreen(g_pGameUIViewport);
	s_hScreen->SetPendingServer(row);
}
