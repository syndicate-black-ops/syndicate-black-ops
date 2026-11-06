//========= SBO VGUI2: the updater's dialog and engine glue =========//
//
// Purpose: the in-game side of the updater (src/cl_dll/updater/updater_client.h, docs/UPDATER.md):
//            * owns the updater: made in the client's Initialize (VGui2_Updater_Startup: an interrupted or deferred
//              install is finished or undone there, before any of SBO's UI loads its files), the cvar
//              cl_check_for_updates (archived, default 1; BugfixedHL-Rebased's name and meaning) and the commands
//              update_check and update_changelog (BHL's names) in HUD_Init, and pumped from HUD_Frame;
//            * the automatic check once at start (cl_check_for_updates 1), a few frames after the config ran (BHL);
//            * the dialog over the main menu (never while connected): "A new update is available!", the versions,
//              the release notes (markdown as styled text, links open in the Steam overlay), and Update / Skip this
//              version / Later; with "[no auto-update]" on the release, Open download page instead of Update; then
//              the download's progress (Cancel), Install and quit, and errors (nothing changed, Open download page,
//              Try again);
//            * the Version tab's view (update_checker.h).
// Two looks, as the other main-menu windows (cl_experimental_menus): 1 SBO's CSboWindow (sbo_settings_window.h, the
// server browser's look), 0 a vgui2::Frame in the classic ClientSourceScheme (as the old-style Advanced dialog).
// Both show the same content (MakeView), the same notes (CSboUpdateNotes) and the same progress bar (DrawProgress).
//
// Build settings (cmake/sbo_client.cmake): SBO_UPDATE_REPO (owner/name on GitHub), SBO_UPDATE_PUBKEY (the Ed25519
// key that signs manifest.json; empty: notice only), SBO_UPDATE_PRERELEASES (test builds also see pre-releases).
// Launch option -update_api_url <url> replaces https://api.github.com/repos/<repo> (tests).
//
// SDK world (hl1_source_sdk headers); engine calls through sbo_bridge.h.
//=================================================================//

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>

#include <FileSystem.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/ScrollBarSlider.h>

#include "client_steam_context.h"
#include "client_vgui.h"
#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_loading_screen.h"
#include "sbo_settings_window.h"
#include "sbo_ui_style.h"
#include "sbo_widgets.h"
#include "vgui2_api.h"

#include "../updater/update_checker.h"
#include "../updater/updater_install.h"
#include "../version/sbo_version.h"

#ifndef SBO_UPDATE_REPO
#define SBO_UPDATE_REPO ""
#endif
#ifndef SBO_UPDATE_PUBKEY
#define SBO_UPDATE_PUBKEY ""
#endif
#ifndef SBO_UPDATE_PRERELEASES
#define SBO_UPDATE_PRERELEASES 0
#endif

sbo_update::IHttp *SboUpdate_SteamHttp(); // sbo_update_http.cpp

using namespace sbolook;
using sbo_update::State;

namespace
{
sbo_update::Updater *s_pUpdater = nullptr; // never deleted: its worker threads may outlive the client's shutdown
sbo::CvarHandle s_cvCheck = nullptr;
bool s_bWantWindow = false;
bool s_bAfterGame = false;   //!< the window waits for the menu after a game (the check finished while connected)
double s_flMenuSince = 0.0;  //!< since when the game is back at the menu with the window waiting (real time)
constexpr double MENU_SETTLE_SECONDS = 1.0;
bool s_bReopen = false;      //!< the menus' look changed with the window open: it opens again in the new look at once
int s_iReopenScroll = -1;    //!< the notes' scroll position to restore when it opens again

void UpdaterLog(const std::string &line)
{
	sbo::ConsolePrint((line + "\n").c_str());
}

void OpenUrl(const std::string &url)
{
	if (url.compare(0, 8, "https://") != 0)
		return;
	if (ClientSteamContext().SteamFriends())
		ClientSteamContext().SteamFriends()->ActivateGameOverlayToWebPage(url.c_str());
	else
		vgui2::system()->ShellExecute("open", url.c_str());
}

//-----------------------------------------------------------------------------
// The two looks: cl_experimental_menus 1 the new menus' look (CSboWindow), 0 the classic look of GameUI's dialogs
// (a vgui2::Frame in ClientSourceScheme, like the old-style Advanced dialog, sbo_options.cpp). Both show the same
// content, built here from the updater's state.
//-----------------------------------------------------------------------------
enum UpdateButton
{
	BTN_UPDATE,
	BTN_INSTALL,
	BTN_RETRY,
	BTN_PAGE,
	BTN_SKIP,
	BTN_LATER,
	BTN_CANCEL,
	BTN_DONE,
	BTN_COUNT
};

struct ButtonDef
{
	const char *text, *command;
	bool primary;
};

const ButtonDef BUTTONS[BTN_COUNT] = {{"Update", "Update", true}, {"Install and quit", "Install", true},
    {"Try again", "Update", true}, {"Open download page", "Page", false}, {"Skip this version", "Skip", false},
    {"Later", "Close", false}, {"Cancel", "Cancel", false}, {"Close", "Close", false}};

struct UpdateView
{
	std::string title;
	std::vector<std::string> lines;
	bool progress = false;
	bool notes = true;        //!< the release notes (not while checking, nor for "up to date" or a failed check)
	std::vector<int> buttons; //!< UpdateButton, laid out right to left
};

UpdateView MakeView(const sbo_update::Updater &u)
{
	UpdateView v;
	const State s = u.GetState();
	const sbo_update::CheckResult &r = u.GetResult();
	// a check under way, up to date or failed: no new version or notes (an earlier check's result stays in r)
	v.notes = s != State::Checking && s != State::Idle && s != State::Disabled && s != State::UpToDate &&
	          s != State::Failed;
	const std::string newest = v.notes && r.hasLatest ? sbo_update::VersionText(r.latest.version) : "";
	v.lines.push_back(std::string("Your version: ") + SboVersion_Number() + (newest.empty() ? "" : "    New version: " + newest));
	switch (s)
	{
	case State::UpdateAvailable:
		v.title = "A new update is available!";
		if (u.CanInstall())
			v.buttons = {BTN_LATER, BTN_SKIP, BTN_UPDATE};
		else
		{
			v.lines.push_back(r.latest.noAutoUpdate ? "This update has to be installed by hand: open the download page."
			                                        : "Get it from the download page.");
			v.buttons = {BTN_LATER, BTN_SKIP, BTN_PAGE};
		}
		break;
	case State::Preparing:
	case State::Downloading:
		v.title = "Downloading the update";
		v.progress = true;
		v.buttons = {BTN_CANCEL};
		break;
	case State::ReadyToInstall:
		v.title = "Ready to install";
		v.progress = true;
		v.lines.push_back(sbo::IsConnected() ? "Installing leaves the game you are in and quits. Start it again afterwards."
		                                     : "The game quits to install it. Start it again afterwards.");
		v.buttons = {BTN_CANCEL, BTN_INSTALL};
		break;
	case State::Installing:
		v.title = "Installing";
		v.progress = true;
		break;
	case State::Installed:
		v.title = "Update installed";
		v.lines.push_back("The game quits now. Start it again to play " + newest + ".");
		break;
	case State::InstallFailed:
		v.title = "The update failed";
		v.lines.push_back(u.GetError());
		v.buttons = {BTN_DONE, BTN_PAGE, BTN_RETRY};
		break;
	case State::UpToDate:
		v.title = "Syndicate Black Ops is up to date";
		v.buttons = {BTN_DONE};
		break;
	case State::Failed:
		v.title = "Update check failed";
		v.lines.push_back(u.GetError());
		v.buttons = {BTN_DONE};
		break;
	default:
		v.title = "Checking for updates";
		v.progress = s == State::Checking;
		v.buttons = {BTN_DONE};
		break;
	}
	return v;
}

bool NeedsRepaint(State s)
{
	return s == State::Checking || s == State::Downloading || s == State::Preparing || s == State::Installing;
}

//! The buttons' commands both windows share; false: the window's own (Close; Skip closes after this)
bool UpdateCommand(sbo_update::Updater &u, const char *command, bool &installWhenDisconnected)
{
	if (!Q_stricmp(command, "Update"))
		u.StartUpdate();
	else if (!Q_stricmp(command, "Install"))
	{
		if (sbo::IsConnected())
		{
			// a game this client plays or hosts ends first ("disconnect" also stops a listen server)
			sbo::ExecClientCmd("disconnect\n");
			installWhenDisconnected = true;
		}
		else
			u.Install();
	}
	else if (!Q_stricmp(command, "Page"))
		OpenUrl(u.GetResult().latest.pageUrl);
	else if (!Q_stricmp(command, "Skip"))
	{
		u.SkipVersion();
		return false;
	}
	else if (!Q_stricmp(command, "Cancel"))
		u.CancelUpdate();
	else
		return false;
	return true;
}

//! Every think of an open window: the install after the disconnect, the quit after "Installed"
void UpdateThink(sbo_update::Updater &u, bool &installWhenDisconnected, double &quitAt)
{
	if (installWhenDisconnected && !sbo::IsConnected())
	{
		installWhenDisconnected = false;
		u.Install();
	}
	if (u.GetState() == State::Installed && quitAt == 0.0)
		quitAt = vgui2::system()->GetCurrentTime() + 1.5; // "Installed" shown, then the game quits
	if (quitAt > 0.0 && vgui2::system()->GetCurrentTime() >= quitAt)
	{
		quitAt = -1.0;
		sbo::ExecClientCmd("quit\n");
	}
}

//! The progress bar's colors and the status line's font
struct BarLook
{
	Color bg, edgeLit, edgeDark, fill, text;
	vgui2::HFont font = 0;
	bool segments = false; //!< the classic bar: blocks with gaps (vgui2::ProgressBar's look)
	int inset = 1;         //!< the fill's distance from the edge
};

//! Height of DrawProgress's bar and status line
int ProgressTall(int barTall, int gap, vgui2::HFont font)
{
	return barTall + gap + FontTall(font);
}

void FillBar(int x, int y, int w, int h, const BarLook &look)
{
	if (w <= 0)
		return;
	if (!look.segments)
	{
		FillRect(x, y, w, h, look.fill);
		return;
	}
	const int seg = std::max(3, h * 3 / 4), step = seg + 2;
	for (int sx = x; sx < x + w; sx += step)
		FillRect(sx, y, std::min(seg, x + w - sx), h, look.fill);
}

//! The bar at (x, y), w wide, and the status line under it (inside the panel being painted)
void DrawProgress(const sbo_update::Updater &u, int x, int y, int w, int barTall, int gap, const BarLook &look)
{
	const State s = u.GetState();
	const sbo_update::Progress p = u.GetProgress();
	const int bh = barTall;
	FillRect(x, y, w, bh, look.bg);
	// the edges: lit at the bottom and right, dark at the top and left (an inset); one color in the new look
	FillRect(x, y, w, 1, look.edgeDark);
	FillRect(x, y, 1, bh, look.edgeDark);
	FillRect(x, y + bh - 1, w, 1, look.edgeLit);
	FillRect(x + w - 1, y, 1, bh, look.edgeLit);
	// waiting on the network with no count yet (the release list, the manifest and its signature): a highlight
	// sweeping back and forth; else the share done
	const bool sweep = s == State::Checking || (s == State::Preparing && p.checkTotal <= 0);
	float f = 0.0f;
	if (s == State::ReadyToInstall || s == State::Installing)
		f = 1.0f;
	else if (s == State::Preparing && p.checkTotal > 0)
		f = std::min(1.0f, (float)p.checkDone / (float)p.checkTotal);
	else if (s == State::Downloading && p.bytesTotal > 0)
		f = std::min(1.0f, (float)p.bytesDone / (float)p.bytesTotal);
	const int in = look.inset, inner = w - 2 * in;
	if (sweep && inner > 8)
	{
		// a quarter of the bar, eased at both ends, one way in 0.9 s
		const int seg = std::max(8, inner / 4);
		const double t = vgui2::system()->GetCurrentTime() / 0.9;
		const double phase = t - 2.0 * (double)(long long)(t / 2.0); // 0..2: out and back
		const double lin = phase < 1.0 ? phase : 2.0 - phase;
		const double ease = lin * lin * (3.0 - 2.0 * lin);
		FillBar(x + in + (int)((inner - seg) * ease), y + in, seg, bh - 2 * in, look);
	}
	else if (f > 0.0f)
		FillBar(x + in, y + in, std::max(1, (int)(inner * f)), bh - 2 * in, look);
	std::string status;
	char buf[256];
	if (s == State::Checking)
		status = "Checking for updates...";
	else if (s == State::Preparing && p.checkTotal <= 0)
		status = "Getting the list of files...";
	else if (s == State::Preparing)
	{
		snprintf(buf, sizeof(buf), "Checking files %d / %d", p.checkDone, p.checkTotal);
		status = buf;
	}
	else if (s == State::Downloading)
	{
		snprintf(buf, sizeof(buf), "File %d of %d, %.1f of %.1f MB: %s", std::min(p.filesDone + 1, p.filesTotal),
		    p.filesTotal, p.bytesDone / 1048576.0, p.bytesTotal / 1048576.0, p.current.c_str());
		status = buf;
	}
	else if (s == State::Installing)
		status = "Installing...";
	if (!status.empty())
		DrawText(look.font, look.text, x, y + bh + gap, Widen(status.c_str()), w);
}

//-----------------------------------------------------------------------------
// The notes' scroll bar arrows: a triangle drawn like the server browser's drop-down arrows (sbolook::DrawTriangle),
// not a Marlett glyph (clipped in a narrow bar, and not drawn on Linux). Classic: the scheme's scroll bar button
// colors with a raised edge.
//-----------------------------------------------------------------------------
class CSboScrollArrow : public vgui2::Button
{
	DECLARE_CLASS_SIMPLE(CSboScrollArrow, vgui2::Button);

public:
	CSboScrollArrow(vgui2::Panel *pParent, bool up, bool classic)
	    : BaseClass(pParent, up ? "UpButton" : "DownButton", "")
	    , m_bUp(up)
	    , m_bClassic(classic)
	{
	}
	void ApplySchemeSettings(vgui2::IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		if (m_bClassic)
		{
			m_Bg = GetSchemeColor("ScrollBarButton.BgColor", Color(90, 106, 80, 255), pScheme);
			m_Fg = GetSchemeColor("ScrollBarButton.FgColor", Color(216, 222, 211, 255), pScheme);
			m_FgArmed = GetSchemeColor("ScrollBarButton.ArmedFgColor", Color(255, 255, 255, 255), pScheme);
			m_Lit = GetSchemeColor("Border.Bright", Color(136, 145, 128, 255), pScheme);
			m_Dark = GetSchemeColor("Border.Dark", Color(40, 46, 34, 255), pScheme);
		}
	}
	void PaintBackground() override { FillRect(0, 0, GetWide(), GetTall(), m_bClassic ? m_Bg : COL_BG_INPUT); }
	void PaintBorder() override
	{
		const int w = GetWide(), h = GetTall();
		if (!m_bClassic)
		{
			OutlineRect(0, 0, w, h, COL_BORDER_LIST);
			return;
		}
		const bool down = IsDepressed();
		FillRect(0, 0, w, 1, down ? m_Dark : m_Lit);
		FillRect(0, 0, 1, h, down ? m_Dark : m_Lit);
		FillRect(0, h - 1, w, 1, down ? m_Lit : m_Dark);
		FillRect(w - 1, 0, 1, h, down ? m_Lit : m_Dark);
	}
	void Paint() override
	{
		const int w = GetWide(), h = GetTall();
		const bool hot = IsArmed() || IsDepressed();
		const Color c = m_bClassic ? (hot ? m_FgArmed : m_Fg) : (hot ? COL_TEXT_BRIGHT : COL_TEXT);
		DrawTriangle(w / 2, h / 2, std::max(3, w / 2 - (m_bClassic ? 3 : 1)), m_bUp, c);
	}

private:
	bool m_bUp, m_bClassic;
	Color m_Bg, m_Fg, m_FgArmed, m_Lit, m_Dark;
};

//-----------------------------------------------------------------------------
// Release notes: styled runs in a RichText. Classic: the scheme's font, text colors and inset border; headings and
// links in the bright text color, bold in the selected (gold) one, code in the dim one.
//-----------------------------------------------------------------------------
class CSboUpdateNotes : public vgui2::RichText
{
	DECLARE_CLASS_SIMPLE(CSboUpdateNotes, vgui2::RichText);

public:
	CSboUpdateNotes(vgui2::Panel *pParent, bool classic)
	    : BaseClass(pParent, "UpdateNotes")
	    , m_bClassic(classic)
	{
		SetVerticalScrollbar(true);
		SetUnusedScrollbarInvisible(true);
		SetPanelInteractive(true);
		SetCursor(vgui2::dc_arrow);
		// arrow buttons drawn by hand (the scheme's Marlett arrows were cut off at the new look's width, and Linux
		// draws no Marlett glyphs)
		for (int i = 0; i < 2; i++)
		{
			vgui2::Button *old = _vertScrollBar->GetButton(i);
			_vertScrollBar->SetButton(new CSboScrollArrow(_vertScrollBar, i == 0, classic), i);
			if (old)
				old->MarkForDeletion();
		}
	}

	//! The scroll bar's position (a line index), and one to apply once the next text is laid out (-1: the start)
	int GetScroll() const { return _vertScrollBar ? _vertScrollBar->GetValue() : 0; }
	void RestoreScroll(int value)
	{
		m_iRestoreScroll = value;
		m_iRestoreTries = 0;
	}

	void SetRuns(const std::vector<sbo_update::TextRun> &runs)
	{
		m_Runs = runs;
		Reinsert();
	}

	void ApplySchemeSettings(vgui2::IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		if (m_bClassic)
		{
			const vgui2::HFont font = pScheme->GetFont("Default", IsProportional());
			SetFont(font);
			SetUnderlineFont(font);
			m_Text = GetSchemeColor("RichText.TextColor", Color(216, 222, 211, 255), pScheme);
			m_Bright = GetSchemeColor("Label.TextBrightColor", Color(255, 255, 255, 255), pScheme);
			m_Bold = GetSchemeColor("Label.SelectedTextColor", Color(196, 181, 80, 255), pScheme);
			m_Code = GetSchemeColor("Label.TextDullColor", Color(160, 170, 149, 255), pScheme);
			m_Dim = m_Code;
			SetFgColor(m_Text);
			SetBgColor(GetSchemeColor("RichText.BgColor", Color(62, 70, 55, 255), pScheme));
			SetBorder(pScheme->GetBorder("DepressedBorder"));
			SetDrawOffsets(6, 4);
			// the scheme's thumb (ControlBG) is nearly the track's color (ControlDarkBG): the lit border color instead
			m_Thumb = GetSchemeColor("Border.Bright", Color(136, 145, 128, 255), pScheme);
		}
		else
		{
			SetFont(S().body);
			SetUnderlineFont(S().body); // links: the scheme's underline font is larger than the text around them
			m_Text = COL_TEXT_LIGHT;
			m_Bright = COL_TEXT_BRIGHT;
			m_Bold = COL_TEXT_SELECTED;
			m_Code = COL_PING_MID;
			m_Dim = COL_TEXT_DIM;
			SetFgColor(COL_TEXT_LIGHT);
			SetBgColor(COL_BG_INPUT);
			SetBorder(nullptr);
			SetDrawOffsets(S().M(10), S().M(8));
		}
		Reinsert();
	}

	void PerformLayout() override
	{
		// the new look: a narrow bar like the server browser's list (not the scheme's proportional width); set before
		// RichText lays out the text beside it. Classic: the scheme's bar.
		if (_vertScrollBar && !m_bClassic)
		{
			_vertScrollBar->SetWide(std::max(12, S().M(14)));
			// a visible track the whole height, the thumb (sized by RichText to the visible share) in the frame color
			if (vgui2::ScrollBarSlider *slider = _vertScrollBar->GetSlider())
			{
				slider->SetPaintBackgroundEnabled(true);
				slider->SetBgColor(COL_BG_SELECTED);
				slider->SetFgColor(COL_BORDER);
			}
		}
		else if (_vertScrollBar)
		{
			// classic: the scheme's bar with a thumb that stands out from the track (set here, after the slider's own
			// scheme settings)
			if (vgui2::ScrollBarSlider *slider = _vertScrollBar->GetSlider())
				slider->SetFgColor(m_Thumb);
		}
		BaseClass::PerformLayout();
	}

	void OnSizeChanged(int w, int h) override
	{
		BaseClass::OnSizeChanged(w, h);
		Reinsert();
	}

	void Paint() override
	{
		// RichText breaks its lines only in OnThink: a window made and painted in one frame (or painted with no
		// think after its notes got their size) would draw every line on one row. Break them here when they are
		// still due (once the notes have a width; nothing to do otherwise).
		if (GetWide() > 0)
			BaseClass::OnThink();
		BaseClass::Paint();
	}

	void PaintBorder() override
	{
		if (m_bClassic)
			BaseClass::PaintBorder();
		else
			OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER_LIST);
	}

	void OnThink() override
	{
		BaseClass::OnThink();
		// RichText keeps a text "at the end" when it was scrolled there before the new layout (the notes opened a few
		// lines down, their first heading hidden): to the start again once the new text is laid out, as
		// CSboTextPanel does
		if (m_bGotoStartPending)
		{
			m_bGotoStartPending = false;
			GotoTextStart();
		}
		// reopened in the other look: back where it was, once the text is laid out (the bar has its range)
		if (m_iRestoreScroll > 0 && _vertScrollBar)
		{
			int lo = 0, hi = 0;
			_vertScrollBar->GetRange(lo, hi);
			if (hi > 0 || ++m_iRestoreTries > 30)
			{
				_vertScrollBar->SetValue(m_iRestoreScroll);
				m_iRestoreScroll = -1;
			}
		}
	}

	void OnTextClicked(const wchar_t *text) override
	{
		char url[1024];
		Q_UnicodeToUTF8(text, url, sizeof(url));
		OpenUrl(url);
	}

private:
	void Reinsert()
	{
		SetText(L"");
		bool lineStart = true;
		int indent = 0;
		const int indentStep = m_bClassic ? 16 : S().M(16);
		for (const sbo_update::TextRun &r : m_Runs)
		{
			if (lineStart && r.indent != indent)
			{
				indent = r.indent;
				InsertIndentChange(indent * indentStep);
			}
			Color c = m_Text;
			switch (r.style)
			{
			case sbo_update::RunStyle::Heading: c = m_Bright; break;
			case sbo_update::RunStyle::Bold: c = m_Bold; break;
			case sbo_update::RunStyle::Code: c = m_Code; break;
			case sbo_update::RunStyle::Link: c = m_Bright; break;
			case sbo_update::RunStyle::Dim: c = m_Dim; break;
			default: break;
			}
			InsertColorChange(c);
			const std::wstring w = Widen(r.text.c_str());
			if (r.style == sbo_update::RunStyle::Link)
			{
				InsertClickableTextStart(r.url.c_str());
				InsertString(w.c_str());
				InsertClickableTextEnd();
			}
			else
				InsertString(w.c_str());
			lineStart = !r.text.empty() && r.text.back() == '\n';
			if (lineStart && indent != 0)
			{
				indent = 0;
				InsertIndentChange(0);
			}
		}
		GotoTextStart();
		m_bGotoStartPending = true;
	}

	std::vector<sbo_update::TextRun> m_Runs;
	bool m_bGotoStartPending = false;
	int m_iRestoreScroll = -1, m_iRestoreTries = 0;
	bool m_bClassic;
	Color m_Text, m_Bright, m_Bold, m_Code, m_Dim;
	Color m_Thumb; //!< classic: the scroll bar thumb
};

//-----------------------------------------------------------------------------
// The window in the new menus' look
//-----------------------------------------------------------------------------
class CSboUpdateWindow : public CSboWindow
{
	DECLARE_CLASS_SIMPLE(CSboUpdateWindow, CSboWindow);

public:
	explicit CSboUpdateWindow(vgui2::Panel *pParent)
	    : BaseClass(pParent, "SboUpdateWindow", "A new update is available!", "update", 640, 560)
	{
		m_pNotes = new CSboUpdateNotes(this, false);
		// bottom buttons are laid out right to left in m_Bottom's order; each state picks its set (Rebuild)
		for (int i = 0; i < BTN_COUNT; i++)
		{
			m_pButtons[i] = AddBottomButton(BUTTONS[i].text, BUTTONS[i].command, BUTTONS[i].primary);
			m_pButtons[i]->SetVisible(false);
		}
		m_Bottom.clear();
	}

	void Open() override
	{
		BaseClass::Open();
		m_iState = -1;
		Rebuild();
	}

	CSboUpdateNotes *Notes() const { return m_pNotes; }

	void OnThink() override
	{
		BaseClass::OnThink();
		sbo_update::Updater *u = s_pUpdater;
		if (!u)
			return;
		if ((int)u->GetState() != m_iState)
			Rebuild();
		UpdateThink(*u, m_bInstallWhenDisconnected, m_flQuitAt);
		if (NeedsRepaint(u->GetState()))
			Repaint();
	}

	void OnCommand(const char *command) override
	{
		sbo_update::Updater *u = s_pUpdater;
		if (u && UpdateCommand(*u, command, m_bInstallWhenDisconnected))
			return;
		BaseClass::OnCommand(!Q_stricmp(command, "Skip") ? "Close" : command);
	}

	void Paint() override
	{
		BaseClass::Paint();
		sbo_update::Updater *u = s_pUpdater;
		if (!u)
			return;
		const Style &st = S();
		const int x = m_iTextX, w = m_iTextW;
		int y = m_iTextY;
		const int line = FontTall(st.body) + st.M(4);
		for (const std::string &l : m_View.lines)
		{
			DrawText(st.body, COL_TEXT_LIGHT, x, y, Widen(l.c_str()), w);
			y += line;
		}
		const State s = u->GetState();
		if (s == State::Checking || s == State::Downloading || s == State::Preparing || s == State::Installing ||
		    s == State::ReadyToInstall)
		{
			BarLook look;
			look.bg = COL_BG_INPUT;
			look.edgeLit = look.edgeDark = COL_BORDER_LIST;
			look.fill = COL_BORDER_PRIMARY;
			look.text = COL_TEXT_DIM;
			look.font = st.small ? st.small : st.body;
			DrawProgress(*u, x, y + st.M(4), w, st.M(10), st.M(8), look);
		}
	}

protected:
	void LayoutContent(int x, int y, int w, int h) override
	{
		const Style &st = S();
		m_iTextX = x;
		m_iTextY = y;
		m_iTextW = w;
		const int line = FontTall(st.body) + st.M(4);
		int top = y + (int)m_View.lines.size() * line + st.M(10);
		if (m_View.progress)
			top += st.M(10) + st.M(12) + FontTall(st.small ? st.small : st.body) + st.M(8);
		m_pNotes->SetBounds(x, top, w, std::max(st.M(60), y + h - top));
	}

private:
	void Rebuild()
	{
		sbo_update::Updater *u = s_pUpdater;
		if (!u)
			return;
		m_iState = (int)u->GetState();
		m_View = MakeView(*u);
		m_Title = m_View.title;
		for (CSboBrowserButton *b : m_pButtons)
			b->SetVisible(false);
		m_Bottom.clear();
		for (int b : m_View.buttons)
		{
			m_pButtons[b]->SetVisible(true);
			m_Bottom.push_back(m_pButtons[b]);
		}
		m_pNotes->SetRuns(m_View.notes ? u->ChangelogRuns() : std::vector<sbo_update::TextRun>());
		m_pNotes->SetVisible(m_View.notes);
		InvalidateLayout();
		Repaint();
	}

	CSboUpdateNotes *m_pNotes;
	CSboBrowserButton *m_pButtons[BTN_COUNT];
	UpdateView m_View;
	int m_iState = -1;
	bool m_bInstallWhenDisconnected = false;
	double m_flQuitAt = 0.0;
	int m_iTextX = 0, m_iTextY = 0, m_iTextW = 0;
};

//-----------------------------------------------------------------------------
// The window in the classic look: a vgui2::Frame under the GameUI viewport, which has ClientSourceScheme (BHL's copy
// of GameUI's scheme, client_viewport.cpp), like the old-style Advanced dialog; the scheme's buttons, fonts, colors
// and borders, the bar and the lines drawn in its colors
//-----------------------------------------------------------------------------
class CSboUpdateDialogClassic : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE(CSboUpdateDialogClassic, vgui2::Frame);

public:
	explicit CSboUpdateDialogClassic(vgui2::Panel *pParent)
	    : BaseClass(pParent, "SboUpdateDialogClassic")
	{
		SetProportional(false);
		SetBounds(0, 0, 560, 440);
		SetMinimumSize(420, 300);
		SetSizeable(true);
		SetMoveable(true);
		SetMenuButtonVisible(false);
		SetMinimizeButtonVisible(false);
		SetMaximizeButtonVisible(false);
		SetDeleteSelfOnClose(true); // made again at the next open (the look may have changed)
		m_pNotes = new CSboUpdateNotes(this, true);
		for (int i = 0; i < BTN_COUNT; i++)
		{
			m_pButtons[i] = new vgui2::Button(this, "UpdateButton", BUTTONS[i].text, this, BUTTONS[i].command);
			m_pButtons[i]->SetVisible(false);
		}
	}

	void Open()
	{
		m_iState = -1;
		Rebuild();
		SetVisible(true);
		MoveToCenterOfScreen();
		Activate();
	}

	CSboUpdateNotes *Notes() const { return m_pNotes; }

	void ApplySchemeSettings(vgui2::IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		m_hFont = pScheme->GetFont("Default", IsProportional());
		m_Text = GetSchemeColor("Label.TextColor", Color(216, 222, 211, 255), pScheme);
		m_Look.bg = GetSchemeColor("TextEntry.BgColor", Color(62, 70, 55, 255), pScheme);
		m_Look.edgeLit = GetSchemeColor("Border.Bright", Color(136, 145, 128, 255), pScheme);
		m_Look.edgeDark = GetSchemeColor("Border.Dark", Color(40, 46, 34, 255), pScheme);
		m_Look.fill = GetSchemeColor("Label.SelectedTextColor", Color(196, 181, 80, 255), pScheme);
		m_Look.text = m_Text;
		m_Look.font = m_hFont;
		m_Look.segments = true;
		m_Look.inset = 2;
		sboui::ApplyCloseButtonSubstitute(this); // Linux: the close button's glyph (after the base class's Marlett font)
		InvalidateLayout();
	}

	void OnThink() override
	{
		BaseClass::OnThink();
		sbo_update::Updater *u = s_pUpdater;
		if (!u)
			return;
		if ((int)u->GetState() != m_iState)
			Rebuild();
		UpdateThink(*u, m_bInstallWhenDisconnected, m_flQuitAt);
		if (NeedsRepaint(u->GetState()))
			Repaint();
	}

	void OnCommand(const char *command) override
	{
		sbo_update::Updater *u = s_pUpdater;
		if (u && UpdateCommand(*u, command, m_bInstallWhenDisconnected))
			return;
		BaseClass::OnCommand(!Q_stricmp(command, "Skip") ? "Close" : command);
	}

	void PerformLayout() override
	{
		BaseClass::PerformLayout();
		int cx, cy, cw, ch;
		GetClientArea(cx, cy, cw, ch);
		const int pad = 8, bh = 24;
		m_iTextX = cx + pad;
		m_iTextY = cy + pad;
		m_iTextW = cw - 2 * pad;
		const int line = (m_hFont ? FontTall(m_hFont) : 14) + 4;
		int top = m_iTextY + (int)m_View.lines.size() * line + 4;
		m_iBarY = top;
		if (m_View.progress)
			top += ProgressTall(BAR_TALL, 6, m_hFont) + 10;
		const int by = cy + ch - pad - bh;
		int x = cx + cw - pad;
		for (int b : m_View.buttons)
		{
			int w = 0, h = 0;
			m_pButtons[b]->GetContentSize(w, h);
			w = std::max(w + 16, 80);
			x -= w;
			m_pButtons[b]->SetBounds(x, by, w, bh);
			x -= 6;
		}
		const int bottom = m_View.buttons.empty() ? cy + ch - pad : by - pad;
		m_pNotes->SetBounds(m_iTextX, top, m_iTextW, std::max(40, bottom - top));
	}

	void Paint() override
	{
		BaseClass::Paint();
		sbo_update::Updater *u = s_pUpdater;
		if (!u || !m_hFont)
			return;
		const int line = FontTall(m_hFont) + 4;
		int y = m_iTextY;
		for (size_t i = 0; i < m_View.lines.size(); i++)
		{
			DrawText(m_hFont, m_Text, m_iTextX, y, Widen(m_View.lines[i].c_str()), m_iTextW);
			y += line;
		}
		if (m_View.progress)
			DrawProgress(*u, m_iTextX, m_iBarY, m_iTextW, BAR_TALL, 6, m_Look);
	}

private:
	static constexpr int BAR_TALL = 16;

	void Rebuild()
	{
		sbo_update::Updater *u = s_pUpdater;
		if (!u)
			return;
		m_iState = (int)u->GetState();
		m_View = MakeView(*u);
		SetTitle(m_View.title.c_str(), true);
		for (vgui2::Button *b : m_pButtons)
			b->SetVisible(false);
		for (size_t i = 0; i < m_View.buttons.size(); i++)
		{
			vgui2::Button *b = m_pButtons[m_View.buttons[i]];
			b->SetVisible(true);
			// the leftmost button (the action) is the default, as in GameUI's dialogs
			b->SetAsDefaultButton(i + 1 == m_View.buttons.size() && BUTTONS[m_View.buttons[i]].primary);
		}
		m_pNotes->SetRuns(m_View.notes ? u->ChangelogRuns() : std::vector<sbo_update::TextRun>());
		m_pNotes->SetVisible(m_View.notes);
		InvalidateLayout();
		Repaint();
	}

	CSboUpdateNotes *m_pNotes;
	vgui2::Button *m_pButtons[BTN_COUNT];
	UpdateView m_View;
	BarLook m_Look;
	vgui2::HFont m_hFont = 0;
	Color m_Text;
	int m_iState = -1;
	bool m_bInstallWhenDisconnected = false;
	double m_flQuitAt = 0.0;
	int m_iTextX = 0, m_iTextY = 0, m_iTextW = 0, m_iBarY = 0;
};

vgui2::DHANDLE<CSboUpdateWindow> s_hWindow;
vgui2::DHANDLE<CSboUpdateDialogClassic> s_hClassic;

void UpdateCheck_f()
{
	if (!s_pUpdater || s_pUpdater->GetState() == State::Disabled)
	{
		UpdaterLog("Update checks are not available in this build");
		return;
	}
	UpdaterLog("Checking for updates...");
	sbo_update::CheckForUpdates();
}

void UpdateChangelog_f()
{
	if (!s_pUpdater || !s_pUpdater->GetResult().updateAvailable)
	{
		UpdaterLog(s_pUpdater && s_pUpdater->GetState() == State::UpToDate ? "The game is up to date."
		                                                              : "No update found (update_check checks).");
		return;
	}
	const std::string text = sbo_update::RunsToPlain(s_pUpdater->ChangelogRuns());
	// the console prints at most about 1 KB per call
	size_t pos = 0;
	while (pos < text.size())
	{
		size_t n = std::min<size_t>(900, text.size() - pos);
		if (pos + n < text.size())
		{
			const size_t nl = text.rfind('\n', pos + n);
			if (nl != std::string::npos && nl > pos)
				n = nl + 1 - pos;
		}
		sbo::ConsolePrint(text.substr(pos, n).c_str());
		pos += n;
	}
	sbo::ConsolePrint("\n");
}
} // namespace

//-----------------------------------------------------------------------------
// Engine glue (vgui2_api.h)
//-----------------------------------------------------------------------------
void VGui2_Updater_Startup()
{
	if (s_pUpdater)
		return;
	sbo_update::Config cfg;
	cfg.gameDir = sbo_update::GameDirOfModule((const void *)&VGui2_Updater_Startup);
	cfg.repo = SBO_UPDATE_REPO;
	cfg.publicKeyHex = SBO_UPDATE_PUBKEY;
	cfg.includePrereleases = SBO_UPDATE_PRERELEASES != 0;
	cfg.installedVersion = SboVersion_Number();
	if (const char *api = sbo::GetLaunchParm("-update_api_url"))
		if (!strncmp(api, "https://", 8) || !strncmp(api, "http://", 7))
			cfg.apiBase = api;
	s_pUpdater = new sbo_update::Updater(cfg, SboUpdate_SteamHttp(), UpdaterLog);
	if (cfg.gameDir.empty() && !cfg.repo.empty())
		UpdaterLog("Updater: the game folder was not found; update checks only");
	s_pUpdater->Startup();
}

void VGui2_Updater_Init()
{
	static bool s_bDone = false;
	if (s_bDone)
		return;
	s_bDone = true;
	s_cvCheck = sbo::RegisterCvar("cl_check_for_updates", "1", true);
	sbo::AddCommand("update_check", UpdateCheck_f);
	sbo::AddCommand("update_changelog", UpdateChangelog_f);
}

void VGui2_Updater_Frame()
{
	if (!s_pUpdater)
		return;
	s_pUpdater->Frame();
	static int s_iFrames = 0;
	if (s_iFrames < 1000)
		s_iFrames++;
	if (s_iFrames == 5)
	{
		// the config has run: the start's check, and the previous version's backups go once this one is up
		s_pUpdater->CleanupAfterStart();
		if (s_cvCheck && sbo::CvarGetFloat(s_cvCheck) != 0.0f)
			s_pUpdater->Check(false);
	}
	if (s_pUpdater->TakeShowRequest())
		s_bWantWindow = true;
	// never over a connect's or a disconnect's window (GameUI's loading / error window, the joining screen): they hold
	// the paint and the input (app-modal), so a window opened then was never drawn and took the clicks and keys meant
	// for their Close (the dev server shut down during a game: the player could not leave the message)
	if (s_bWantWindow && VGui2_LoadingScreen_DialogUp())
	{
		s_flMenuSince = 0.0;
		return;
	}
	// the dialog over the main menu only: the start's check that finished during a game (a +connect launch) waits for
	// the menu after the disconnect, and for a second at it: "quit" disconnects too, and the frames before the game
	// closes must not show it (a manual check opens it at once)
	if (s_bWantWindow && !s_bReopen && !s_pUpdater->CheckWasManual())
	{
		if (sbo::IsConnected())
		{
			s_bAfterGame = true;
			s_flMenuSince = 0.0;
			return;
		}
		if (s_bAfterGame)
		{
			const double now = vgui2::system()->GetCurrentTime();
			if (s_flMenuSince <= 0.0)
				s_flMenuSince = now;
			if (now - s_flMenuSince < MENU_SETTLE_SECONDS)
				return;
		}
	}
	if (s_bWantWindow && g_pGameUIViewport)
	{
		s_bWantWindow = false;
		s_bAfterGame = false;
		s_flMenuSince = 0.0;
		const bool reopen = s_bReopen;
		const int scroll = s_iReopenScroll;
		s_bReopen = false;
		s_iReopenScroll = -1;
		// the look of the other main-menu windows (cl_experimental_menus, read when the window opens); the other
		// look's window closes
		if (!SboExperimentalMenus())
		{
			if (s_hWindow.Get() && s_hWindow->IsVisible())
				s_hWindow->Close();
			// the classic look: ClientSourceScheme, loaded by the GameUI viewport
			if (!s_hClassic.Get())
				s_hClassic = new CSboUpdateDialogClassic(g_pGameUIViewport);
			s_hClassic->Open();
			if (reopen)
				s_hClassic->Notes()->RestoreScroll(scroll);
		}
		// the new look (serverbrowserscheme.res) must be there: the engine's scheme loader ends the game when a scheme
		// file is missing, and the updater must work on an install that lacks files
		else if (!g_pFullFileSystem || !g_pFullFileSystem->FileExists(VGUI2_ROOT_DIR "resource/serverbrowserscheme.res"))
			UpdaterLog("Updater: the update window cannot open (" VGUI2_ROOT_DIR
			           "resource/serverbrowserscheme.res is missing); update_changelog prints what is new");
		else
		{
			if (s_hClassic.Get() && s_hClassic->IsVisible())
				s_hClassic->Close();
			if (!s_hWindow.Get())
				s_hWindow = new CSboUpdateWindow(g_pGameUIViewport);
			s_hWindow->Open();
			if (reopen)
				s_hWindow->Notes()->RestoreScroll(scroll);
		}
	}
}

void VGui2_Updater_MenusStyleChanged()
{
	// as the other main-menu windows (VGui2_MenusStyle_Frame): the open window closes and opens again in the new look
	// on the next frame (VGui2_Updater_Frame); the updater keeps its state, so it shows the same page
	int scroll = -1;
	bool open = false;
	if (s_hWindow.Get() && s_hWindow->IsVisible())
	{
		scroll = s_hWindow->Notes()->GetScroll();
		s_hWindow->Close();
		open = true;
	}
	if (s_hClassic.Get() && s_hClassic->IsVisible())
	{
		scroll = s_hClassic->Notes()->GetScroll();
		s_hClassic->Close();
		open = true;
	}
	if (!open)
		return;
	s_bWantWindow = true;
	s_bReopen = true;
	s_iReopenScroll = scroll;
}

//-----------------------------------------------------------------------------
// Version tab (update_checker.h)
//-----------------------------------------------------------------------------
namespace sbo_update
{
bool IsEnabled()
{
	return s_pUpdater && s_pUpdater->GetState() != State::Disabled;
}

void CheckForUpdates()
{
	if (s_pUpdater)
		s_pUpdater->Check(true);
}

State GetState()
{
	return s_pUpdater ? s_pUpdater->GetState() : State::Disabled;
}

const char *GetLatestVersion()
{
	static std::string s;
	s = s_pUpdater && s_pUpdater->GetResult().hasLatest ? VersionText(s_pUpdater->GetResult().latest.version) : "";
	return s.c_str();
}

const char *GetError()
{
	return s_pUpdater ? s_pUpdater->GetError().c_str() : "";
}

Updater *Instance()
{
	return s_pUpdater;
}

std::string StatusText()
{
	switch (GetState())
	{
	case State::Disabled: return "Latest version: update checks are not available in this build";
	case State::Idle: return "Latest version: not checked";
	case State::Checking: return "Latest version: checking...";
	case State::UpToDate:
		return std::string("Latest version: ") + (*GetLatestVersion() ? GetLatestVersion() : SboVersion_Number()) + " (up to date)";
	case State::Failed: return std::string("Latest version: check failed (") + GetError() + ")";
	case State::Installed: return std::string("Latest version: ") + GetLatestVersion() + " (installed: start the game again)";
	default: return std::string("Latest version: ") + GetLatestVersion() + " (update available)";
	}
}
} // namespace sbo_update
