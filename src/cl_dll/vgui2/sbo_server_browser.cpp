//========= SBO VGUI2: server browser ("Find server") =========//
//
// Purpose: see sbo_server_browser.h.
//=================================================================//

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <algorithm>
#include <vector>

#include <FileSystem.h>
#include <IBaseUI.h>
#include <KeyValues.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <IEngineVGui.h>
#include <vgui/Cursor.h>
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/IInputInternal.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/ScrollBar.h>

#include "client_steam_context.h"
#include "client_vgui.h" // VGUI2_ROOT_DIR
#include "client_viewport.h"
#include "hud_chat.h" // GetColorCodeLength
#include "sbo_a2s_rules.h"
#include "sbo_bridge.h"
#include "sbo_server_address.h"
#include "sbo_server_browser.h"
#include "sbo_loading_screen.h"
#include "sbo_image_decode.h"
#include "sbo_map_picture.h"
#include "sbo_ui_style.h"
#include "sbo_widgets.h"

#ifdef CreateFont
#undef CreateFont
#endif
#ifdef PostMessage
#undef PostMessage
#endif

static vgui2::DHANDLE<CSboServerBrowser> s_hBrowser;
static bool s_bOpenNextFrame = false;

//-----------------------------------------------------------------------------
// Look: the design's colors, sizes in design pixels (a 1280x800 screen) scaled to this screen, its fonts
// (sbo_ui_style.h, shared with the joining screen)
//-----------------------------------------------------------------------------
using namespace sbolook;

namespace
{
// the rules query follows the player query after this many seconds (a server's max_queries_sec window)
constexpr double RULES_DELAY = 1.1;
// the selected server's info is asked for again this often (players joining, map changes)
constexpr double SELECTED_REFRESH_S = 10.0;
// Steam's limit on Internet list requests (CSboServerBrowser::RefreshComplete): the wait shown after a limited answer,
// and how long after a list with servers an empty instant answer counts as the limit
constexpr double RATE_LIMIT_WAIT = 20.0;
constexpr double RATE_LIMIT_MEMORY = 120.0;
// an answer this fast with no server is the limit too (limited answers came in 0.13-0.17 s, lists in 0.38-1.12 s)
constexpr double RATE_LIMIT_INSTANT = 0.3;
// when Steam's Internet list last had servers (kept while the game runs: the limit outlives the window)
double s_flLastListed = -1.0;

// The design's window: 1200 x 720 on a 1280 x 800 page
constexpr int MOCK_WIDE = 1200;
constexpr int MOCK_TALL = 720;
// smallest window (design pixels) and the share of the screen width the game menu keeps on the left
constexpr int MIN_WIDE = 900;
constexpr int MIN_TALL = 560;
constexpr float MENU_SHARE = 0.24f;

AppId_t BrowserAppID()
{
	ISteamUtils *pUtils = ClientSteamContext().SteamUtils();
	const AppId_t app = pUtils ? pUtils->GetAppID() : 0;
	return app ? app : 70; // Half-Life
}

//! A password the console takes inside quotes: printable, no quote, no ';' (would end the command)
bool SafePassword(const char *pw)
{
	for (const char *c = pw; *c; c++)
	{
		if ((unsigned char)*c < 32 || *c == '"' || *c == ';')
			return false;
	}
	return strlen(pw) < 64;
}
} // namespace

std::string SboServerRow::Address() const
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%u.%u.%u.%u:%u", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255,
	    (unsigned)connPort);
	return buf;
}

//-----------------------------------------------------------------------------
// CSboBrowserButton
//-----------------------------------------------------------------------------
CSboBrowserButton::CSboBrowserButton(vgui2::Panel *pParent, const char *pName, const char *text, const char *command,
    Kind kind)
    : BaseClass(pParent, pName, text, pParent, command)
    , m_eKind(kind)
{
	SetPaintBackgroundEnabled(true);
	SetPaintBorderEnabled(true);
}

int CSboBrowserButton::GetContentWide()
{
	char text[128];
	GetText(text, sizeof(text));
	const vgui2::HFont font = m_eKind == KIND_TAB ? S().tab : S().button;
	const int pad = m_eKind == KIND_PRIMARY ? 32 : (m_eKind == KIND_TAB ? 18 : 20);
	return TextWide(font, Widen(text)) + 2 * S().M(pad);
}

void CSboBrowserButton::PaintBackground()
{
	const int w = GetWide(), h = GetTall();
	if (m_eKind == KIND_PRIMARY)
		FillRect(0, 0, w, h, IsEnabled() ? COL_BG_PRIMARY : COL_BG_INPUT);
	else if (m_eKind == KIND_TAB && m_bActive)
		FillRect(0, 0, w, h, COL_BG_SELECTED);
	else if (IsEnabled() && (IsArmed() || IsDepressed()))
		FillRect(0, 0, w, h, Color(29, 58, 24, 140)); // hover: a lighter selected-row green
}

void CSboBrowserButton::PaintBorder()
{
	const int w = GetWide(), h = GetTall();
	const Color c = m_eKind == KIND_PRIMARY && IsEnabled() ? COL_BORDER_PRIMARY : COL_BORDER;
	if (m_eKind == KIND_TAB)
	{
		// left, top, right: the tab row's line is its bottom border
		FillRect(0, 0, w, 1, c);
		FillRect(0, 0, 1, h, c);
		FillRect(w - 1, 0, 1, h, c);
		return;
	}
	OutlineRect(0, 0, w, h, c);
}

void CSboBrowserButton::Paint()
{
	char text[128];
	GetText(text, sizeof(text));
	std::wstring wtext = Widen(text);
	vgui2::HFont font = S().button;
	Color color = COL_TEXT;
	switch (m_eKind)
	{
	case KIND_PRIMARY:
		color = IsEnabled() ? COL_TEXT_PRIMARY : COL_TEXT_FAINT;
		break;
	case KIND_TAB:
		font = S().tab;
		color = m_bActive ? COL_TEXT_BRIGHT : COL_TEXT_DIM;
		break;
	case KIND_CLOSE:
		font = S().close;
		wtext = L"×"; // multiplication sign (the design's close glyph)
		break;
	default:
		color = IsEnabled() ? COL_TEXT : COL_TEXT_FAINT;
		break;
	}
	if (IsEnabled() && IsArmed() && m_eKind != KIND_PRIMARY)
		color = COL_TEXT_BRIGHT;
	const int tw = TextWide(font, wtext);
	DrawTextMid(font, color, (GetWide() - tw) / 2, 0, GetTall(), wtext);
}

//-----------------------------------------------------------------------------
// CSboBrowserCheck
//-----------------------------------------------------------------------------
CSboBrowserCheck::CSboBrowserCheck(vgui2::Panel *pParent, const char *pName, const char *text)
    : BaseClass(pParent, pName, text)
{
}

int CSboBrowserCheck::GetContentWide()
{
	char text[64];
	GetText(text, sizeof(text));
	return S().M(13) + S().M(6) + TextWide(S().body, Widen(text)) + S().M(4);
}

void CSboBrowserCheck::DoClick()
{
	m_bChecked = !m_bChecked;
	Repaint();
	PostActionSignal(new KeyValues("ButtonToggled", "state", m_bChecked ? 1 : 0));
	PlayButtonReleasedSound();
}

void CSboBrowserCheck::Paint()
{
	const int box = S().M(13), h = GetTall();
	const int y = (h - box) / 2;
	if (m_bChecked)
	{
		FillRect(0, y, box, box, COL_BORDER_PRIMARY);
		// check mark: two strokes of small squares
		const int t = box / 7 > 1 ? box / 7 : 1;
		vgui2::surface()->DrawSetColor(COL_TEXT_PRIMARY);
		const int x0 = box * 3 / 13, y0 = y + box * 7 / 13;
		const int n1 = box * 3 / 13, n2 = box * 5 / 13;
		for (int i = 0; i <= n1; i++)
			vgui2::surface()->DrawFilledRect(x0 + i, y0 + i, x0 + i + t + 1, y0 + i + t + 1);
		for (int i = 0; i <= n2; i++)
			vgui2::surface()->DrawFilledRect(x0 + n1 + i, y0 + n1 - i, x0 + n1 + i + t + 1, y0 + n1 - i + t + 1);
	}
	else
	{
		FillRect(0, y, box, box, COL_BG_INPUT);
		OutlineRect(0, y, box, box, IsArmed() ? COL_TEXT : COL_TEXT_DIM);
	}
	char text[64];
	GetText(text, sizeof(text));
	DrawTextMid(S().body, COL_TEXT_DIM, box + S().M(6), 0, h, Widen(text));
}

//-----------------------------------------------------------------------------
// CSboBrowserSelect
//-----------------------------------------------------------------------------
CSboBrowserSelect::CSboBrowserSelect(vgui2::Panel *pParent, const char *pName)
    : BaseClass(pParent, pName, "")
{
}

//! The open list of a drop-down, drawn in the look (VGUI2's Menu draws its scroll bar from GameUI's scheme: cut
//! off, then huge on Windows): on the select's window, below the select (above it when there is no room), as many
//! rows as fit; more scroll with the wheel, with a thin scroll bar painted at the right. It takes the mouse while
//! open: a click on a row chooses it, a click anywhere else closes the list. Esc closes, Enter chooses.
class CSboSelectPopup : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboSelectPopup, vgui2::Panel);

public:
	CSboSelectPopup(vgui2::Panel *pWindow, CSboBrowserSelect *pSelect)
	    : BaseClass(pWindow, "SelectPopup")
	    , m_pSelect(pSelect)
	{
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(true);
		m_iHover = pSelect->GetSelected();
	}

	void Open()
	{
		vgui2::Panel *pWindow = GetParent();
		const int rowTall = m_pSelect->GetTall();
		int sx = 0, sy = 0;
		m_pSelect->LocalToScreen(sx, sy);
		pWindow->ScreenToLocal(sx, sy);
		const int below = pWindow->GetTall() - (sy + rowTall) - S().M(6);
		const int above = sy - S().M(6);
		const int n = m_pSelect->GetItemCount();
		const bool down = below >= std::min(n, 6) * rowTall || below >= above;
		const int room = down ? below : above;
		m_iRows = std::max(1, std::min(n, room / rowTall));
		const int h = m_iRows * rowTall + 2;
		SetBounds(sx, down ? sy + rowTall : sy - h, m_pSelect->GetWide(), h);
		m_iFirst = std::max(0, std::min(m_pSelect->GetSelected() - m_iRows / 2, n - m_iRows));
		SetZPos(100);
		MoveToFront();
		SetVisible(true);
		RequestFocus();
		vgui2::input()->SetMouseCapture(GetVPanel());
	}

	void Close()
	{
		if (vgui2::input()->GetMouseCapture() == GetVPanel())
			vgui2::input()->SetMouseCapture(0);
		SetVisible(false);
		MarkForDeletion();
	}

	int RowAtCursor()
	{
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		ScreenToLocal(x, y);
		if (x < 0 || y < 1 || x >= GetWide() || y >= GetTall() - 1)
			return -1;
		const int row = m_iFirst + (y - 1) / m_pSelect->GetTall();
		return row < m_pSelect->GetItemCount() ? row : -1;
	}

	void OnMousePressed(vgui2::MouseCode code) override
	{
		const int row = code == vgui2::MOUSE_LEFT ? RowAtCursor() : -1;
		int x, y;
		vgui2::input()->GetCursorPos(x, y);
		ScreenToLocal(x, y);
		if (row < 0 && x >= 0 && y >= 0 && x < GetWide() && y < GetTall())
			return; // the scroll bar strip
		CSboBrowserSelect *pSelect = m_pSelect;
		// a press on the drop-down's own box closes the list: the box gets the release once the capture is gone and
		// would open it again (Button activates on release), so it ignores that one click
		int bx = 0, by = 0, cx = 0, cy = 0;
		pSelect->LocalToScreen(bx, by);
		vgui2::input()->GetCursorPos(cx, cy);
		if (row < 0 && cx >= bx && cy >= by && cx < bx + pSelect->GetWide() && cy < by + pSelect->GetTall())
			pSelect->SuppressClickUntil(vgui2::system()->GetCurrentTime() + 0.5);
		Close();
		if (row >= 0)
			pSelect->Choose(row);
	}

	void OnCursorMoved(int x, int y) override
	{
		const int row = RowAtCursor();
		if (row >= 0 && row != m_iHover)
		{
			m_iHover = row;
			Repaint();
		}
	}

	void OnMouseWheeled(int delta) override
	{
		const int n = m_pSelect->GetItemCount();
		m_iFirst = std::max(0, std::min(m_iFirst - delta * 2, n - m_iRows));
		Repaint();
	}

	void OnKeyCodePressed(vgui2::KeyCode code) override
	{
		const int n = m_pSelect->GetItemCount();
		if (code == vgui2::KEY_ESCAPE)
			Close();
		else if (code == vgui2::KEY_ENTER)
		{
			CSboBrowserSelect *pSelect = m_pSelect;
			const int row = m_iHover;
			Close();
			if (row >= 0 && row < n)
				pSelect->Choose(row);
		}
		else if (code == vgui2::KEY_UP || code == vgui2::KEY_DOWN)
		{
			m_iHover = std::max(0, std::min(n - 1, m_iHover + (code == vgui2::KEY_UP ? -1 : 1)));
			if (m_iHover < m_iFirst)
				m_iFirst = m_iHover;
			if (m_iHover >= m_iFirst + m_iRows)
				m_iFirst = m_iHover - m_iRows + 1;
			Repaint();
		}
	}
	void OnKeyCodeTyped(vgui2::KeyCode code) override {} // Esc must not reach the window (it would close it)

	void PaintBackground() override { FillRect(0, 0, GetWide(), GetTall(), COL_BG_INPUT); }
	void PaintBorder() override { OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER); }
	void Paint() override
	{
		const int rowTall = m_pSelect->GetTall(), n = m_pSelect->GetItemCount(), pad = S().M(8);
		const bool scroll = n > m_iRows;
		const int barW = scroll ? std::max(3, S().M(5)) : 0;
		for (int i = 0; i < m_iRows && m_iFirst + i < n; i++)
		{
			const int row = m_iFirst + i, y = 1 + i * rowTall;
			if (row == m_iHover)
				FillRect(1, y, GetWide() - 2 - barW, rowTall, COL_BG_SELECTED);
			const Color c = row == m_pSelect->GetSelected() ? COL_TEXT_BRIGHT : (row == m_iHover ? COL_TEXT_SELECTED : COL_TEXT);
			DrawTextMid(S().body, c, pad, y, rowTall, Widen(m_pSelect->GetItemText(row)), GetWide() - 2 * pad - barW);
		}
		if (scroll)
		{
			// thin scroll bar: the shown part of the list
			const int trackH = GetTall() - 4;
			const int thumbH = std::max(S().M(12), trackH * m_iRows / n);
			const int thumbY = 2 + (trackH - thumbH) * m_iFirst / std::max(1, n - m_iRows);
			FillRect(GetWide() - 2 - barW, 2, barW, trackH, COL_BG_WINDOW);
			FillRect(GetWide() - 2 - barW, thumbY, barW, thumbH, COL_BORDER_PRIMARY);
		}
	}

private:
	CSboBrowserSelect *m_pSelect;
	int m_iFirst = 0, m_iRows = 1, m_iHover = 0;
};

void CSboBrowserSelect::DoClick()
{
	if (vgui2::system()->GetCurrentTime() < m_flSuppressClickUntil)
	{
		m_flSuppressClickUntil = 0.0; // the release of the press that just closed the list
		return;
	}
	if (m_hPopup.Get())
	{
		static_cast<CSboSelectPopup *>(m_hPopup.Get())->Close();
		m_hPopup = nullptr;
		return;
	}
	if (m_Items.empty())
		return;
	// the list goes on the select's window (a Frame) so nothing of the window draws over it
	vgui2::Panel *pWindow = GetParent();
	while (pWindow && !dynamic_cast<vgui2::Frame *>(pWindow) && pWindow->GetParent())
		pWindow = pWindow->GetParent();
	if (!pWindow)
		return;
	CSboSelectPopup *pPopup = new CSboSelectPopup(pWindow, this);
	m_hPopup = pPopup;
	pPopup->Open();
}

void CSboBrowserSelect::Choose(int i)
{
	if (i >= 0 && i < (int)m_Items.size() && i != m_iSelected)
	{
		m_iSelected = i;
		Repaint();
		PostActionSignal(new KeyValues("SelectChanged"));
	}
}

void CSboBrowserSelect::OnCommand(const char *command)
{
	if (!strncmp(command, "select ", 7))
	{
		const int i = atoi(command + 7);
		if (i >= 0 && i < (int)m_Items.size() && i != m_iSelected)
		{
			m_iSelected = i;
			Repaint();
			PostActionSignal(new KeyValues("SelectChanged"));
		}
		return;
	}
	BaseClass::OnCommand(command);
}

void CSboBrowserSelect::PaintBackground()
{
	FillRect(0, 0, GetWide(), GetTall(), COL_BG_INPUT);
}

void CSboBrowserSelect::PaintBorder()
{
	OutlineRect(0, 0, GetWide(), GetTall(), IsArmed() ? COL_TEXT : COL_BORDER);
}

int CSboBrowserSelect::GetContentWide()
{
	const int pad = S().M(8), arrow = S().M(8);
	int longest = 0;
	for (const std::string &item : m_Items)
		longest = std::max(longest, TextWide(S().body, Widen(item.c_str())));
	return pad + longest + pad + arrow + pad;
}

void CSboBrowserSelect::Paint()
{
	const int pad = S().M(8), arrow = S().M(8);
	if (m_iSelected >= 0 && m_iSelected < (int)m_Items.size())
		DrawTextMid(S().body, COL_TEXT, pad, 0, GetTall(), Widen(m_Items[m_iSelected].c_str()), GetWide() - 2 * pad - arrow);
	DrawTriangle(GetWide() - pad - arrow / 2, GetTall() / 2, arrow, false, COL_TEXT);
}

//-----------------------------------------------------------------------------
// CSboBrowserEntry
//-----------------------------------------------------------------------------
CSboBrowserEntry::CSboBrowserEntry(vgui2::Panel *pParent, const char *pName, const char *placeholder)
    : BaseClass(pParent, pName)
    , m_Placeholder(placeholder ? placeholder : "")
{
}

void CSboBrowserEntry::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetFont(S().body);
	SetFgColor(COL_TEXT);
	SetBgColor(COL_BG_INPUT);
	SetBorder(nullptr);
	SetPaintBorderEnabled(true);
	SetSelectionTextColor(COL_TEXT_PRIMARY);
	SetSelectionBgColor(COL_BG_PRIMARY);
	SetDisabledBgColor(COL_BG_INPUT);
}

void CSboBrowserEntry::PaintBorder()
{
}

void CSboBrowserEntry::PaintBackground()
{
	FillRect(0, 0, GetWide(), GetTall(), COL_BG_INPUT); // TextEntry draws its text here, not its background
	BaseClass::PaintBackground();
}

void CSboBrowserEntry::Paint()
{
	BaseClass::Paint();
	OutlineRect(0, 0, GetWide(), GetTall(), HasFocus() ? COL_TEXT : COL_BORDER); // no scheme border: drawn here
	if (!m_Placeholder.empty() && GetTextLength() == 0 && !HasFocus())
	{
		// the HTML placeholder: dim text where the typed text starts
		DrawTextMid(S().body, COL_TEXT_FAINT, S().M(8) - 3, 0, GetTall(), Widen(m_Placeholder.c_str()));
	}
}

//-----------------------------------------------------------------------------
// CSboServerListPanel
//-----------------------------------------------------------------------------
// Column widths of the design's grid: 28px | 1fr | 120px | 200px | 90px | 60px | 60px, gap 8, padding 12
// two presses on the same row within this many seconds = Connect (the Windows / desktop default double-click time)
static constexpr double DOUBLE_CLICK_TIME = 0.5;
// Map column: at most this wide (design pixels) unless dragged wider; Server at least NAME_MIN
static constexpr int MAP_MAX = 200;
// room after a content-sized column's widest text (design pixels), besides the 8-pixel gap
static constexpr int COLUMN_ROOM = 14;
static constexpr int NAME_MIN = 120;
static const char *const s_ColumnName[CSboServerListPanel::COL_COUNT] = { "", "", "Server", "Mode", "Map", "Players", "Bots", "Ping" };
// Widths the player dragged, in design pixels so they scale with the resolution (0 = fit the content); kept in
// serverbrowser.vdf ("columns") between games, as the engine's browser keeps its column widths
static int s_ColumnMock[CSboServerListPanel::COL_COUNT] = {};
static void SaveSettings();

//! The list request's callbacks in the console at developer 1 or more ("Find server: ...")
static void BrowserLog(const char *fmt, ...)
{
	static sbo::CvarHandle s_hDeveloper = nullptr;
	if (!s_hDeveloper)
		s_hDeveloper = sbo::FindCvar("developer");
	if (!s_hDeveloper || sbo::CvarGetFloat(s_hDeveloper) < 1.0f)
		return;
	char text[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, sizeof(text), fmt, ap);
	va_end(ap);
	char line[560];
	snprintf(line, sizeof(line), "Find server: %s\n", text);
	sbo::ConsolePrint(line);
}

//! Valve's VAC shield (resource/icon_vac.tga, the icon of GameUI's connecting window), read from valve/ by the
//! engine's file search: SBO ships no copy (docs/SERVER_BROWSER.md, "The engine's VAC icon"). The file is 64x64 with
//! the shield in its top-left 32x32, alpha 0 or 255 only, and the shield's anti-aliased edge baked in against black
//! (opaque dark grey pixels around the white). Drawn as it is, that edge is a dark fringe on the list's green-black.
//! So the file is decoded here (sbo_image, as the map pictures) and turned into coverage: the opaque pixels within
//! two pixels of the transparent outside and darker than the white are the edge, white at a coverage of their
//! brightness; the rest of the shield (the white and the grey "VAC") is covered fully. The coverage is scaled down
//! to the drawn size (area average) and mixed with the row's background on the CPU into an opaque texture, as the
//! infinity sign: the client's surface draws textures without blending (alpha 0 or full). One texture per drawn size
//! and background (a normal and a selected row).
namespace
{
struct VacShield
{
	bool tried = false;
	int w = 0, h = 0;
	std::vector<float> premul; //!< w * h * 3, color times coverage (0..255)
	std::vector<float> cover;  //!< w * h, 0..1
};

const VacShield &LoadVacShield()
{
	static VacShield s;
	if (s.tried)
		return s;
	s.tried = true;
	std::vector<uint8_t> file;
	if (FileHandle_t fh = g_pFullFileSystem->Open("resource/icon_vac.tga", "rb"))
	{
		const unsigned size = g_pFullFileSystem->Size(fh);
		if (size > 0 && size <= sbo_image::MAX_FILE_BYTES)
		{
			file.resize(size);
			if (g_pFullFileSystem->Read(file.data(), (int)size, fh) != (int)size)
				file.clear();
		}
		g_pFullFileSystem->Close(fh);
	}
	std::vector<uint8_t> rgba;
	int iw = 0, ih = 0;
	if (file.empty() || !sbo_image::DecodeTgaScaled(file.data(), file.size(), 256, 256, rgba, iw, ih))
		return s;
	auto A = [&](int x, int y) { return rgba[((size_t)y * iw + x) * 4 + 3]; };
	// the visible part (alpha above 0)
	int x0 = iw, y0 = ih, x1 = -1, y1 = -1;
	for (int py = 0; py < ih; py++)
		for (int px = 0; px < iw; px++)
			if (A(px, py))
			{
				x0 = std::min(x0, px);
				y0 = std::min(y0, py);
				x1 = std::max(x1, px);
				y1 = std::max(y1, py);
			}
	if (x1 < x0 || y1 < y0)
		return s;
	// distance (in steps, at most 3) of each pixel from the transparent outside; outside the file counts as outside
	std::vector<int> dist((size_t)iw * ih, 3);
	for (int py = 0; py < ih; py++)
		for (int px = 0; px < iw; px++)
			if (!A(px, py))
				dist[(size_t)py * iw + px] = 0;
	for (int pass = 1; pass <= 2; pass++)
		for (int py = 0; py < ih; py++)
			for (int px = 0; px < iw; px++)
			{
				int &d = dist[(size_t)py * iw + px];
				if (d < 3)
					continue;
				for (int dy = -1; dy <= 1; dy++)
					for (int dx = -1; dx <= 1; dx++)
					{
						const int nx = px + dx, ny = py + dy;
						const int nd = (nx < 0 || ny < 0 || nx >= iw || ny >= ih) ? 0 : dist[(size_t)ny * iw + nx];
						if (nd == pass - 1)
							d = pass;
					}
			}
	s.w = x1 - x0 + 1;
	s.h = y1 - y0 + 1;
	s.premul.assign((size_t)s.w * s.h * 3, 0.0f);
	s.cover.assign((size_t)s.w * s.h, 0.0f);
	for (int py = 0; py < s.h; py++)
		for (int px = 0; px < s.w; px++)
		{
			const int sx = x0 + px, sy = y0 + py;
			const uint8_t *c = &rgba[((size_t)sy * iw + sx) * 4];
			const size_t o = (size_t)py * s.w + px;
			if (!c[3])
				continue;
			const float lum = (c[0] + c[1] + c[2]) / 3.0f;
			const int d = dist[(size_t)sy * iw + sx];
			if (d >= 1 && d <= 2 && lum < 250.0f)
			{
				// the edge, white over black in the file: white at the coverage of its brightness
				s.cover[o] = lum / 255.0f;
				s.premul[o * 3 + 0] = s.premul[o * 3 + 1] = s.premul[o * 3 + 2] = lum;
			}
			else
			{
				s.cover[o] = 1.0f;
				s.premul[o * 3 + 0] = c[0];
				s.premul[o * 3 + 1] = c[1];
				s.premul[o * 3 + 2] = c[2];
			}
		}
	return s;
}
} // namespace

static void DrawVacIcon(int x, int y, int w, Color bg)
{
	const VacShield &sh = LoadVacShield();
	if (sh.w <= 0 || w <= 0 || w > 256)
		return;
	int dw = w, dh = w;
	if (sh.w > sh.h)
		dh = std::max(1, w * sh.h / sh.w);
	else
		dw = std::max(1, w * sh.w / sh.h);
	struct Cached
	{
		int dw, dh, bg, texture;
	};
	static std::vector<Cached> s_Cache;
	const int bgKey = (bg.r() << 16) | (bg.g() << 8) | bg.b();
	int texture = -1;
	for (const Cached &c : s_Cache)
	{
		if (c.dw == dw && c.dh == dh && c.bg == bgKey)
			texture = c.texture;
	}
	if (texture == -1)
	{
		// area average of the coverage and the premultiplied color into dw x dh, mixed with bg; opaque where covered
		std::vector<uint8_t> out((size_t)dw * dh * 4, 0);
		for (int ty = 0; ty < dh; ty++)
			for (int tx = 0; tx < dw; tx++)
			{
				const float fx0 = (float)tx * sh.w / dw, fx1 = (float)(tx + 1) * sh.w / dw;
				const float fy0 = (float)ty * sh.h / dh, fy1 = (float)(ty + 1) * sh.h / dh;
				float cov = 0.0f, pr = 0.0f, pg = 0.0f, pb = 0.0f, area = 0.0f;
				for (int sy = (int)fy0; sy < (int)ceilf(fy1) && sy < sh.h; sy++)
					for (int sx = (int)fx0; sx < (int)ceilf(fx1) && sx < sh.w; sx++)
					{
						const float ax = std::min(fx1, (float)sx + 1) - std::max(fx0, (float)sx);
						const float ay = std::min(fy1, (float)sy + 1) - std::max(fy0, (float)sy);
						const float a = ax * ay;
						if (a <= 0.0f)
							continue;
						const size_t o = (size_t)sy * sh.w + sx;
						cov += sh.cover[o] * a;
						pr += sh.premul[o * 3 + 0] * a;
						pg += sh.premul[o * 3 + 1] * a;
						pb += sh.premul[o * 3 + 2] * a;
						area += a;
					}
				if (area <= 0.0f)
					continue;
				cov /= area;
				if (cov < 0.02f)
					continue; // outside: transparent
				uint8_t *t = &out[((size_t)ty * dw + tx) * 4];
				const float rest = 1.0f - std::min(cov, 1.0f);
				t[0] = (uint8_t)std::min(255.0f, pr / area + bg.r() * rest + 0.5f);
				t[1] = (uint8_t)std::min(255.0f, pg / area + bg.g() * rest + 0.5f);
				t[2] = (uint8_t)std::min(255.0f, pb / area + bg.b() * rest + 0.5f);
				t[3] = 255;
			}
		texture = vgui2::surface()->CreateNewTextureID(true);
		vgui2::surface()->DrawSetTextureRGBA(texture, out.data(), dw, dh, false, true); // pixel for pixel
		s_Cache.push_back({ dw, dh, bgKey, texture });
	}
	vgui2::surface()->DrawSetTexture(texture);
	vgui2::surface()->DrawSetColor(255, 255, 255, 255);
	const int ox = x + (w - dw) / 2, oy = y + (w - dh) / 2;
	vgui2::surface()->DrawTexturedRect(ox, oy, ox + dw, oy + dh);
}

CSboServerListPanel::CSboServerListPanel(vgui2::Panel *pParent)
    : BaseClass(pParent, "ServerList")
{
	m_pScrollBar = new vgui2::ScrollBar(this, "ScrollBar", true);
	m_pScrollBar->SetVisible(false);
	m_pScrollBar->AddActionSignalTarget(this);
	SetKeyBoardInputEnabled(true);
	SetMouseInputEnabled(true);
}

int CSboServerListPanel::HeaderTall()
{
	return S().M(8) * 2 + FontTall(S().small);
}

int CSboServerListPanel::RowTall()
{
	return S().M(9) * 2 + FontTall(S().row);
}

int CSboServerListPanel::RowsFit()
{
	const int n = (GetTall() - HeaderTall() - 1) / RowTall();
	return n > 0 ? n : 1;
}

// Column widths: Server takes what is left; the others fit their heading and the widest value in the list
// (Map at most MAP_MAX), unless the player dragged a heading edge (m_iUserWide).
int CSboServerListPanel::ContentWide(int col)
{
	if (s_ColumnMock[col] > 0)
		return S().M(s_ColumnMock[col]);
	const vgui2::HFont row = S().row, head = S().small;
	int w = SpacedUpperWide(head, s_ColumnName[col]);
	if (col == m_iSortColumn)
		w += S().M(4) + S().M(8); // the sort arrow
	switch (col)
	{
	case COL_LOCK:
	case COL_VAC:
		return S().M(16);
	case COL_MODE:
		for (int i = 0; i < (int)m_Visible.size(); i++)
			w = std::max(w, TextWide(row, Widen(ModeName(m_Rows[m_Visible[i]].mode))));
		break;
	case COL_MAP:
	{
		int m = 0;
		for (int i = 0; i < (int)m_Visible.size(); i++)
			m = std::max(m, TextWide(row, Widen(m_Rows[m_Visible[i]].map.c_str())));
		w = std::max(w, std::min(m, S().M(MAP_MAX)));
		break;
	}
	case COL_PLAYERS:
		w = std::max(w, TextWide(row, L"88 / 88"));
		break;
	case COL_BOTS:
	case COL_PING:
		w = std::max(w, TextWide(row, L"888"));
		break;
	}
	return w + S().M(COLUMN_ROOM);
}

void CSboServerListPanel::ColumnBounds(int col, int &x, int &w)
{
	const int pad = S().M(12), gap = S().M(8);
	int fixed = 0;
	for (int i = 0; i < COL_COUNT; i++)
		fixed += i == COL_NAME ? 0 : ContentWide(i);
	int inner = GetWide() - 2 * pad - (COL_COUNT - 1) * gap;
	if (m_pScrollBar->IsVisible())
		inner -= m_pScrollBar->GetWide();
	const int flex = std::max(inner - fixed, S().M(NAME_MIN));
	x = pad;
	for (int i = 0; i < COL_COUNT; i++)
	{
		w = i == COL_NAME ? flex : ContentWide(i);
		if (i == col)
			return;
		x += w + gap;
	}
}

// The heading edge under x (the right edge of a column, Server's moves the next one), -1 if none
int CSboServerListPanel::EdgeAt(int x)
{
	const int grab = std::max(4, S().M(5)), gap = S().M(8);
	for (int col = COL_NAME; col < COL_COUNT - 1; col++)
	{
		int cx, cw;
		ColumnBounds(col, cx, cw);
		const int edge = cx + cw + gap / 2;
		if (x >= edge - grab && x <= edge + grab)
			return col;
	}
	return -1;
}

void CSboServerListPanel::OnCursorMoved(int x, int y)
{
	if (m_iDragEdge >= 0)
	{
		const int dx = x - m_iDragStartX;
		// the Server column's edge sizes the column after it the other way; any other edge its own column
		const int col = m_iDragEdge == COL_NAME ? COL_MODE : m_iDragEdge;
		const int sign = m_iDragEdge == COL_NAME ? -1 : 1;
		const int px = std::max(S().M(24), m_iDragStartWide + sign * dx);
		s_ColumnMock[col] = std::max(24, (int)(px / std::max(S().scale, 0.01f)));
		Repaint();
		return;
	}
	SetCursor(y < HeaderTall() && EdgeAt(x) >= 0 ? vgui2::dc_sizewe : vgui2::dc_arrow);
}

void CSboServerListPanel::OnMouseReleased(vgui2::MouseCode code)
{
	if (m_iDragEdge >= 0)
	{
		m_iDragEdge = -1;
		vgui2::input()->SetMouseCapture(0);
		SaveSettings(); // the dragged width
	}
}

void CSboServerListPanel::Clear()
{
	m_Rows.clear();
	m_Visible.clear();
	m_iFirstRow = 0;
	UpdateScrollBar();
	Repaint();
}

void CSboServerListPanel::AddOrUpdate(const SboServerRow &row)
{
	bool found = false;
	for (SboServerRow &r : m_Rows)
	{
		if (r.SameServer(row))
		{
			r = row;
			found = true;
			break;
		}
	}
	if (!found)
		m_Rows.push_back(row);
	Rebuild();
}

void CSboServerListPanel::SetFilter(const Filter &filter)
{
	m_Filter = filter;
	m_Filter.map = Lower(m_Filter.map);
	m_iFirstRow = 0;
	Rebuild();
}

bool CSboServerListPanel::Passes(const SboServerRow &row) const
{
	if (m_Filter.mode && row.mode != m_Filter.mode)
		return false;
	if (!m_Filter.map.empty() && Lower(row.map).find(m_Filter.map) == std::string::npos)
		return false;
	if (m_Filter.maxPing && row.ping >= m_Filter.maxPing)
		return false;
	if (m_Filter.hasPlayers && row.humans <= 0)
		return false;
	if (m_Filter.notFull && row.players >= row.maxPlayers)
		return false;
	if (m_Filter.noPassword && row.password)
		return false;
	return true;
}

void CSboServerListPanel::Rebuild()
{
	m_Visible.clear();
	for (int i = 0; i < (int)m_Rows.size(); i++)
	{
		if (Passes(m_Rows[i]))
			m_Visible.push_back(i);
	}
	const int col = m_iSortColumn;
	if (col < 0)
	{
		// no column chosen (the default): the servers in the order they arrived
		UpdateScrollBar();
		Repaint();
		return;
	}
	const bool asc = m_bSortAscending;
	const std::vector<SboServerRow> &rows = m_Rows;
	std::stable_sort(m_Visible.begin(), m_Visible.end(), [&rows, col, asc](int ia, int ib) {
		const SboServerRow &a = rows[ia], &b = rows[ib];
		int d = 0;
		switch (col)
		{
		case COL_LOCK:
			d = (int)a.password - (int)b.password;
			break;
		case COL_VAC:
			d = (int)a.secure - (int)b.secure;
			break;
		case COL_NAME:
			d = Q_stricmp(a.name.c_str(), b.name.c_str());
			break;
		case COL_MODE:
			d = a.mode - b.mode;
			break;
		case COL_MAP:
			d = Q_stricmp(a.map.c_str(), b.map.c_str());
			break;
		case COL_PLAYERS:
			d = a.humans - b.humans;
			break;
		case COL_BOTS:
			d = a.bots - b.bots;
			break;
		case COL_PING:
			d = a.ping - b.ping;
			break;
		}
		if (d == 0)
			d = Q_stricmp(a.name.c_str(), b.name.c_str());
		return asc ? d < 0 : d > 0;
	});
	UpdateScrollBar();
	Repaint();
}

const SboServerRow *CSboServerListPanel::GetSelected() const
{
	if (!m_bHasSelection)
		return nullptr;
	for (int i : m_Visible)
	{
		if (m_Rows[i].SameServer(m_Selected))
			return &m_Rows[i];
	}
	return nullptr;
}

bool CSboServerListPanel::HasSelectedRow() const
{
	if (!m_bHasSelection)
		return false;
	for (const SboServerRow &r : m_Rows)
	{
		if (r.SameServer(m_Selected))
			return true;
	}
	return false;
}

void CSboServerListPanel::Select(int visibleIndex)
{
	if (visibleIndex < 0 || visibleIndex >= (int)m_Visible.size())
		return;
	m_Selected = m_Rows[m_Visible[visibleIndex]];
	m_bHasSelection = true;
	// keep it in view
	if (visibleIndex < m_iFirstRow)
		m_iFirstRow = visibleIndex;
	else if (visibleIndex >= m_iFirstRow + RowsFit())
		m_iFirstRow = visibleIndex - RowsFit() + 1;
	UpdateScrollBar();
	Repaint();
	PostActionSignal(new KeyValues("ServerSelected"));
}

void CSboServerListPanel::UpdateScrollBar()
{
	const int fit = RowsFit();
	const int count = (int)m_Visible.size();
	const int maxFirst = std::max(0, count - fit);
	if (m_iFirstRow > maxFirst)
		m_iFirstRow = maxFirst;
	const bool show = count > fit;
	if (show != m_pScrollBar->IsVisible())
	{
		m_pScrollBar->SetVisible(show);
		InvalidateLayout();
	}
	if (show)
	{
		m_pScrollBar->SetRange(0, count);
		m_pScrollBar->SetRangeWindow(fit);
		m_pScrollBar->SetValue(m_iFirstRow);
	}
}

void CSboServerListPanel::PerformLayout()
{
	BaseClass::PerformLayout();
	const int sbw = S().M(12);
	m_pScrollBar->SetBounds(GetWide() - sbw - 1, HeaderTall() + 1, sbw, GetTall() - HeaderTall() - 2);
	UpdateScrollBar();
}

void CSboServerListPanel::OnSliderMoved(int position)
{
	m_iFirstRow = position;
	Repaint();
}

int CSboServerListPanel::RowAt(int y)
{
	if (y < HeaderTall())
		return -1;
	const int i = m_iFirstRow + (y - HeaderTall()) / RowTall();
	return i < (int)m_Visible.size() ? i : -1;
}

void CSboServerListPanel::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	RequestFocus();
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	if (y < HeaderTall() && EdgeAt(x) >= 0)
	{
		// drag a heading edge: column width
		m_iDragEdge = EdgeAt(x);
		const int col = m_iDragEdge == COL_NAME ? COL_MODE : m_iDragEdge;
		m_iDragStartWide = ContentWide(col);
		m_iDragStartX = x;
		vgui2::input()->SetMouseCapture(GetVPanel());
		return;
	}
	if (y < HeaderTall())
	{
		// header: sort by that column, again = the other direction
		for (int col = 0; col < COL_COUNT; col++)
		{
			int cx, cw;
			ColumnBounds(col, cx, cw);
			if (x >= cx && x < cx + cw + S().M(8))
			{
				if (m_iSortColumn == col)
					m_bSortAscending = !m_bSortAscending;
				else
				{
					m_iSortColumn = col;
					// numbers: most first, except ping; text: A first
					m_bSortAscending = !(col == COL_PLAYERS || col == COL_BOTS);
				}
				Rebuild();
				break;
			}
		}
		return;
	}
	const int row = RowAt(y);
	if (row >= 0)
	{
		// a double click (Connect) is two presses on the same server within DOUBLE_CLICK_TIME, timed here: the
		// engine's double-press message also came for presses on different rows seconds apart
		const double now = vgui2::system()->GetCurrentTime();
		const SboServerRow &r = m_Rows[m_Visible[row]];
		const bool dbl = m_bLastPress && m_LastPress.SameServer(r) && now - m_flLastPressTime < DOUBLE_CLICK_TIME;
		Select(row);
		m_LastPress = r;
		m_bLastPress = !dbl; // a third press starts over
		m_flLastPressTime = now;
		if (dbl)
			PostActionSignal(new KeyValues("ServerActivated"));
	}
}

void CSboServerListPanel::OnMouseDoublePressed(vgui2::MouseCode code)
{
	// the second press of a double click: handled as a press (OnMousePressed times double clicks itself)
	OnMousePressed(code);
}

void CSboServerListPanel::OnMouseWheeled(int delta)
{
	m_iFirstRow -= delta * 3;
	if (m_iFirstRow < 0)
		m_iFirstRow = 0;
	UpdateScrollBar();
	Repaint();
}

void CSboServerListPanel::OnKeyCodeTyped(vgui2::KeyCode code)
{
	int cur = -1;
	for (int i = 0; i < (int)m_Visible.size(); i++)
	{
		if (m_bHasSelection && m_Rows[m_Visible[i]].SameServer(m_Selected))
			cur = i;
	}
	if (code == vgui2::KEY_DOWN)
		Select(cur + 1);
	else if (code == vgui2::KEY_UP)
		Select(cur > 0 ? cur - 1 : 0);
	else if (code == vgui2::KEY_ENTER && cur >= 0)
		PostActionSignal(new KeyValues("ServerActivated"));
	else
		BaseClass::OnKeyCodeTyped(code);
}

void CSboServerListPanel::PaintBackground()
{
}

void CSboServerListPanel::PaintBorder()
{
}

void CSboServerListPanel::Paint()
{
	PaintRows();
	// the frame: drawn here, Panel::PaintTraverse calls PaintBorder only for a panel with a scheme border
	OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER_LIST);
}

void CSboServerListPanel::PaintRows()
{
	const int w = GetWide();
	const int headerTall = HeaderTall();

	// header
	for (int col = 0; col < COL_COUNT; col++)
	{
		int x, cw;
		ColumnBounds(col, x, cw);
		const int tw = DrawSpacedUpper(S().small, COL_TEXT_DIM, x, S().M(8), s_ColumnName[col]);
		if (col == m_iSortColumn)
		{
			const int size = S().M(8);
			const int ax = x + (tw ? tw + S().M(4) : 0) + size / 2;
			DrawTriangle(ax, S().M(8) + FontTall(S().small) / 2, size, m_bSortAscending, COL_TEXT_DIM);
		}
	}
	// the edges a drag resizes (EdgeAt): a short divider in the header, as the column separators of Steam's list
	for (int col = COL_NAME; col < COL_COUNT - 1; col++)
	{
		int x, cw;
		ColumnBounds(col, x, cw);
		FillRect(x + cw + S().M(8) / 2, S().M(6), 1, headerTall - 2 * S().M(6), COL_BORDER_LIST);
	}
	FillRect(1, headerTall, w - 2, 1, COL_BORDER_LIST);

	if (m_Visible.empty())
	{
		// servers listed, all of them hidden by the filters: say so (not "No servers")
		const char *text = m_Rows.empty() ? m_EmptyText.c_str() : "No servers match the filters";
		if (*text)
			DrawText(S().row, COL_TEXT_DIM, S().M(12), headerTall + S().M(9), Widen(text));
		return;
	}

	const SboServerRow *pSel = GetSelected();
	const int rowTall = RowTall();
	const int fit = RowsFit() + 1;
	const int innerRight = m_pScrollBar->IsVisible() ? w - m_pScrollBar->GetWide() - 1 : w - 1;
	for (int n = 0; n < fit; n++)
	{
		const int vi = m_iFirstRow + n;
		if (vi >= (int)m_Visible.size())
			break;
		const SboServerRow &r = m_Rows[m_Visible[vi]];
		const int y = headerTall + 1 + n * rowTall;
		if (y >= GetTall() - 1)
			break;
		const bool selected = pSel && pSel->SameServer(r);
		if (selected)
			FillRect(1, y, innerRight - 1, std::min(rowTall, GetTall() - 1 - y), COL_BG_SELECTED);
		const Color main = selected ? COL_TEXT_SELECTED : COL_TEXT;
		int x, cw;

		ColumnBounds(COL_LOCK, x, cw);
		if (r.password)
		{
			const int lw = S().M(10);
			DrawPadlock(x, y + (rowTall - lw * 5 / 4) / 2, lw, COL_TEXT_DIM);
		}
		ColumnBounds(COL_VAC, x, cw);
		if (r.secure)
		{
			const int iw = S().M(16);
			DrawVacIcon(x, y + (rowTall - iw) / 2, iw, selected ? COL_BG_SELECTED : COL_BG_WINDOW);
		}
		ColumnBounds(COL_NAME, x, cw);
		DrawTextMid(S().row, main, x, y, rowTall, Widen(r.name.c_str()), cw);
		ColumnBounds(COL_MODE, x, cw);
		DrawTextMid(S().row, COL_TEXT_LIGHT, x, y, rowTall, Widen(ModeName(r.mode)), cw);
		ColumnBounds(COL_MAP, x, cw);
		DrawTextMid(S().row, COL_TEXT_LIGHT, x, y, rowTall, Widen(r.map.c_str()), cw);
		char buf[32];
		ColumnBounds(COL_PLAYERS, x, cw);
		snprintf(buf, sizeof(buf), "%d / %d", r.humans, r.maxPlayers);
		DrawTextMid(S().row, main, x, y, rowTall, Widen(buf), cw);
		ColumnBounds(COL_BOTS, x, cw);
		snprintf(buf, sizeof(buf), "%d", r.bots);
		DrawTextMid(S().row, COL_TEXT_DIM, x, y, rowTall, Widen(buf), cw);
		ColumnBounds(COL_PING, x, cw);
		snprintf(buf, sizeof(buf), "%d", r.ping);
		DrawTextMid(S().row, PingColor(r.ping), x, y, rowTall, Widen(buf), cw);
	}
}

//-----------------------------------------------------------------------------
// CSboServerInfoPanel
//-----------------------------------------------------------------------------
CSboServerInfoPanel::CSboServerInfoPanel(vgui2::Panel *pParent)
    : BaseClass(pParent, "ServerInfo")
{
	SetMouseInputEnabled(true); // the player list's wheel and scroll bar
}

void CSboServerInfoPanel::ScrollPlayersTo(int first)
{
	const int maxFirst = std::max(0, (int)m_Players.size() - std::max(1, m_iRowsFit));
	first = std::max(0, std::min(maxFirst, first));
	if (first != m_iPlayerFirst)
	{
		m_iPlayerFirst = first;
		Repaint();
	}
}

void CSboServerInfoPanel::DragBarTo(int y)
{
	const int n = (int)m_Players.size(), track = m_iListBottom - m_iListTop;
	if (n <= m_iRowsFit || track <= 0)
		return;
	ScrollPlayersTo((int)((float)(y - m_iListTop) / track * n - m_iRowsFit / 2.0f + 0.5f));
}

void CSboServerInfoPanel::OnMouseWheeled(int delta)
{
	if ((int)m_Players.size() > m_iRowsFit)
		ScrollPlayersTo(m_iPlayerFirst - delta * 3);
}

void CSboServerInfoPanel::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT || (int)m_Players.size() <= m_iRowsFit)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	// the bar and a little room either side of it
	if (x >= m_iBarX - S().M(6) && x < m_iBarX + m_iBarW + S().M(6) && y >= m_iListTop && y < m_iListBottom)
	{
		m_bDraggingBar = true;
		vgui2::input()->SetMouseCapture(GetVPanel());
		DragBarTo(y);
	}
}

void CSboServerInfoPanel::OnMouseReleased(vgui2::MouseCode code)
{
	if (m_bDraggingBar)
	{
		m_bDraggingBar = false;
		vgui2::input()->SetMouseCapture(0);
	}
}

void CSboServerInfoPanel::OnCursorMoved(int x, int y)
{
	if (m_bDraggingBar)
		DragBarTo(y);
}

CSboServerInfoPanel::~CSboServerInfoPanel()
{
	CancelQueries();
}

void CSboServerInfoPanel::CancelQueries()
{
	ISteamMatchmakingServers *pServers = ClientSteamContext().SteamMatchmakingServers();
	if (pServers)
	{
		if (m_hPlayers != HSERVERQUERY_INVALID)
			pServers->CancelServerQuery(m_hPlayers);
	}
	m_hPlayers = HSERVERQUERY_INVALID;
	m_RulesQuery.Cancel();
}

void CSboServerInfoPanel::SetServer(const SboServerRow *pRow)
{
	if (pRow && m_bHasServer && m_Server.SameServer(*pRow))
	{
		const bool changed = pRow->players != m_Server.players || pRow->bots != m_Server.bots || pRow->map != m_Server.map;
		m_Server = *pRow; // new ping / players; the queried data stays
		ISteamMatchmakingServers *pServers = ClientSteamContext().SteamMatchmakingServers();
		if (changed && pServers && m_hPlayers == HSERVERQUERY_INVALID && !m_RulesQuery.IsRunning() && m_flRulesAt == 0.0)
		{
			// someone joined or left (or the map changed): the player list again, then the rules
			m_Players.clear();
			m_bPlayersDone = false;
			m_hPlayers = pServers->PlayerDetails(m_Server.ip, m_Server.queryPort, this);
		}
		Repaint();
		return;
	}
	CancelQueries();
	m_bHasServer = pRow != nullptr;
	m_iFriendlyFire = m_iTimeLeft = m_iAmxTimeLeft = m_iRound = -1;
	m_iRoundLimit = 0;
	m_Players.clear();
	m_iPlayerFirst = 0;
	m_bPlayersDone = false;
	m_flRulesAt = 0.0;
	if (pRow)
	{
		m_Server = *pRow;
		// the map picture: only for the selected server, downloaded when the mod has none (GetPicture)
		if (m_Pictures.Get(Lower(m_Server.map)))
			m_Pictures.StartDownload(Lower(m_Server.map));
		ISteamMatchmakingServers *pServers = ClientSteamContext().SteamMatchmakingServers();
		if (pServers)
		{
			// one query at a time: servers limit queries per second per address (max_queries_sec 1 on SBO's
			// servers), so the rules are asked for when the player list is in (StartRulesQuery)
			m_hPlayers = pServers->PlayerDetails(m_Server.ip, m_Server.queryPort, this);
		}
	}
	Repaint();
}

//! The infinity sign in a w x h box at (x, y): a lemniscate of Bernoulli as an anti-aliased stroke of the given width
//! (pixels, may be fractional). The curve is sampled in 256 short segments; each pixel's coverage is its distance to
//! the nearest segment (kept per size). Drawn as one opaque filled rect per covered pixel in c mixed with the
//! background bg by the coverage: the client's surface draws translucent rects and textures without blending here
//! (alpha 0 or full; a texture and alpha-colored rects both gave a hard, bold sign).
static void DrawInfinity(int x, int y, int w, int h, float stroke, Color c, Color bg)
{
	struct Cached
	{
		int w, h, stroke10;
		std::vector<unsigned char> coverage;
	};
	static std::vector<Cached> s_Cache;
	if (w <= 2 || h <= 2 || w > 256 || h > 256)
		return;
	const int stroke10 = (int)lroundf(stroke * 10.0f);
	const Cached *pFound = nullptr;
	for (const Cached &cc : s_Cache)
	{
		if (cc.w == w && cc.h == h && cc.stroke10 == stroke10)
			pFound = &cc;
	}
	if (!pFound)
	{
		// the curve's centerline inside the box less half the stroke; x = a cos t / (1 + sin^2 t),
		// y = a sin t cos t / (1 + sin^2 t) (|y| at most a / (2 sqrt 2)), scaled to the box's height
		const float half = stroke * 0.5f;
		const float ax = w * 0.5f - half - 0.5f, ay = (h * 0.5f - half - 0.5f) * 2.8284271f;
		constexpr int SEGMENTS = 256;
		std::vector<float> px(SEGMENTS + 1), py(SEGMENTS + 1);
		for (int i = 0; i <= SEGMENTS; i++)
		{
			const float t = 6.2831853f * i / SEGMENTS, s = sinf(t), d = 1.0f + s * s;
			px[i] = w * 0.5f + ax * cosf(t) / d;
			py[i] = h * 0.5f + ay * s * cosf(t) / d;
		}
		Cached cc{ w, h, stroke10, std::vector<unsigned char>((size_t)w * h, 0) };
		for (int j = 0; j < h; j++)
		{
			for (int i = 0; i < w; i++)
			{
				const float cx = i + 0.5f, cy = j + 0.5f;
				float best = 1e9f;
				for (int k = 0; k < SEGMENTS; k++)
				{
					const float sx = px[k + 1] - px[k], sy = py[k + 1] - py[k];
					const float len2 = sx * sx + sy * sy;
					float u = len2 > 0.0f ? ((cx - px[k]) * sx + (cy - py[k]) * sy) / len2 : 0.0f;
					u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
					const float dx = cx - (px[k] + u * sx), dy = cy - (py[k] + u * sy);
					best = std::min(best, dx * dx + dy * dy);
				}
				float alpha = half - sqrtf(best) + 0.5f;
				alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
				cc.coverage[(size_t)j * w + i] = (unsigned char)lroundf(alpha * 255.0f);
			}
		}
		s_Cache.push_back(std::move(cc));
		pFound = &s_Cache.back();
	}
	for (int j = 0; j < h; j++)
	{
		for (int i = 0; i < w; i++)
		{
			const int a = pFound->coverage[(size_t)j * w + i];
			if (!a)
				continue;
			vgui2::surface()->DrawSetColor(bg.r() + (c.r() - bg.r()) * a / 255, bg.g() + (c.g() - bg.g()) * a / 255,
			    bg.b() + (c.b() - bg.b()) * a / 255, 255);
			vgui2::surface()->DrawFilledRect(x + i, y + j, x + i + 1, y + j + 1);
		}
	}
}

// The rules this panel shows (sbo_a2s_rules.cpp: Steam's ServerRules does not put GoldSrc's split replies together)
void CSboServerInfoPanel::ApplyRule(const char *pchRule, const char *pchValue)
{
	if (!Q_stricmp(pchRule, "mp_friendlyfire"))
		m_iFriendlyFire = atof(pchValue) != 0.0 ? 1 : 0;
	else if (!Q_stricmp(pchRule, "mp_timeleft"))
	{
		// the game updates it on maps with a time limit (CDm / CBr / CHalfLifeMultiplay::Think); 0 = not counting
		const int t = atoi(pchValue);
		m_iTimeLeft = t > 0 ? t : -1;
	}
	else if (!Q_stricmp(pchRule, "amx_timeleft"))
	{
		// AMX Mod X ("timeleft", Galileo): "MM:SS" of get_timeleft(), the mp_timelimit the map vote ends the map by,
		// also on co_ maps (the game itself never ends those by time); "00:00" = no time limit
		int m = 0, s = 0;
		const int t = sscanf(pchValue, "%d:%d", &m, &s) == 2 && m >= 0 && m < 100000 && s >= 0 && s < 60 ? m * 60 + s : 0;
		m_iAmxTimeLeft = t > 0 ? t : -1;
	}
	else if (!Q_stricmp(pchRule, "mp_round"))
	{
		// the game's round (co_: the scoreboard's; br_: the round in play or the last one); 0 = none yet
		const int r = atoi(pchValue);
		m_iRound = r > 0 && r < 100000 ? r : 0;
	}
	else if (!Q_stricmp(pchRule, "mp_roundlimit"))
	{
		const int r = atoi(pchValue); // co_ maps' round limit; 0 = none; -1 = rounds without end
		m_iRoundLimit = r == -1 ? -1 : (r > 0 && r < 100000 ? r : 0);
	}
}

void CSboServerInfoPanel::AddPlayerToList(const char *pchName, int nScore, float flTimePlayed)
{
	std::string name = CleanName(pchName);
	if (!name.empty() && m_Players.size() < 64)
		m_Players.push_back({ name, flTimePlayed > 0.0f && flTimePlayed < 1e7f ? (int)flTimePlayed : 0 });
}

void CSboServerInfoPanel::PlayersFailedToRespond()
{
	m_hPlayers = HSERVERQUERY_INVALID;
	m_bPlayersDone = true;
	m_flRulesAt = vgui2::system()->GetCurrentTime() + RULES_DELAY;
	Repaint();
}

void CSboServerInfoPanel::PlayersRefreshComplete()
{
	m_hPlayers = HSERVERQUERY_INVALID;
	m_bPlayersDone = true;
	m_flRulesAt = vgui2::system()->GetCurrentTime() + RULES_DELAY;
	Repaint();
}

void CSboServerInfoPanel::OnThink()
{
	BaseClass::OnThink();
	if (m_Pictures.Poll())
		Repaint();
	// a map still waiting for its picture (another download was running when it was selected)
	if (m_bHasServer && !m_Pictures.IsDownloading())
		m_Pictures.StartDownload(Lower(m_Server.map));
	const double now = vgui2::system()->GetCurrentTime();
	if (m_flRulesAt > 0.0 && now >= m_flRulesAt)
	{
		m_flRulesAt = 0.0;
		if (m_bHasServer)
			m_RulesQuery.Start(m_Server.ip, m_Server.queryPort, now);
	}
	if (m_RulesQuery.IsRunning() && m_RulesQuery.Poll(now))
	{
		if (m_RulesQuery.Succeeded())
		{
			for (const auto &rule : m_RulesQuery.GetRules())
				ApplyRule(rule.first.c_str(), rule.second.c_str());
		}
		Repaint();
	}
}

void CSboServerInfoPanel::PaintBackground()
{
}

void CSboServerInfoPanel::PaintBorder()
{
}

void CSboServerInfoPanel::Paint()
{
	PaintInfo();
	OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER_LIST); // as the list's
}

void CSboServerInfoPanel::PaintInfo()
{
	const int pad = S().M(14), gap = S().M(12);
	const int w = GetWide() - 2 * pad;
	int y = pad;

	if (!m_bHasServer)
	{
		DrawText(S().body, COL_TEXT_DIM, pad, y, L"Select a server");
		return;
	}

	// overview picture: 150 high, the picture fitted in it; smaller on a short panel, so the player list keeps at
	// least PLAYER_ROWS_MIN rows (no picture when that leaves it under 60)
	constexpr int PLAYER_ROWS_MIN = 6;
	const int detailsTall = FontTall(S().name) + gap + 5 * (FontTall(S().body) + S().M(4)) + gap - S().M(4) +
	                        S().M(10) + FontTall(S().small) + S().M(4);
	const int listMin = PLAYER_ROWS_MIN * (FontTall(S().body) + S().M(4));
	int boxTall = std::min(S().M(150), GetTall() - pad - pad / 2 - detailsTall - listMin - gap);
	if (boxTall < S().M(60))
		boxTall = 0;
	if (boxTall > 0)
	{
		FillRect(pad, y, w, boxTall, COL_BG_INPUT);
		OutlineRect(pad, y, w, boxTall, COL_BORDER_LIST);
		const CSboMapPictures::Picture *pPic = m_Pictures.Get(Lower(m_Server.map)); // the files are named in lower case
		const int texture = pPic ? pPic->texture : -1;
		const int tw = pPic ? pPic->wide : 0, th = pPic ? pPic->tall : 0;
		if (texture != -1 && tw > 0 && th > 0)
		{
			const float fit = std::min((float)(w - 2) / tw, (float)(boxTall - 2) / th);
			const int dw = (int)(tw * fit), dh = (int)(th * fit);
			const int dx = pad + (w - dw) / 2, dy = y + (boxTall - dh) / 2;
			vgui2::surface()->DrawSetTexture(texture);
			vgui2::surface()->DrawSetColor(255, 255, 255, 255);
			vgui2::surface()->DrawTexturedRect(dx, dy, dx + dw, dy + dh);
		}
		if (texture == -1 || tw <= 0 || th <= 0 || (pPic && pPic->isPlaceholder))
		{
			// no picture (or SBO's placeholder): the map name in the middle
			std::wstring text = Widen(m_Server.map.c_str());
			const int textW = std::min(TextWide(S().small, text), w - 8);
			DrawTextMid(S().small, COL_TEXT_DIM, pad + (w - textW) / 2, y, boxTall, text, w - 8);
		}
		y += boxTall + gap;
	}

	// name
	DrawText(S().name, COL_TEXT, pad, y, Widen(m_Server.name.c_str()), w);
	y += FontTall(S().name) + gap;

	// details: two columns
	char buf[64];
	// labels as wide as the longest, the values get the rest (the design's two columns when the panel is 300 wide)
	int labelW = 0;
	for (const wchar_t *label : { L"Mode", L"Round", L"Time left", L"Friendly fire", L"Address" })
		labelW = std::max(labelW, TextWide(S().body, label));
	labelW = std::max(labelW + S().M(12), std::min((w - S().M(12)) / 2, labelW + S().M(40)));
	const int valueW = w - labelW;
	const int lineTall = FontTall(S().body) + S().M(4);
	auto line = [&](const char *label, const char *value) {
		DrawText(S().body, COL_TEXT_DIM, pad, y, Widen(label), labelW);
		DrawText(S().body, COL_TEXT_LIGHT, pad + labelW, y, Widen(value), valueW);
		y += lineTall;
	};
	line("Mode", ModeName(m_Server.mode));
	// the rules mp_round / mp_roundlimit: "3 / 7" as the scoreboard's co-op line, "3" without a limit (br_),
	// "3 / " and the infinity sign for rounds without end (limit -1)
	if (m_iRound >= 0 && m_iRoundLimit == -1)
	{
		snprintf(buf, sizeof(buf), "%d / ", m_iRound);
		const int textW = TextWide(S().body, Widen(buf));
		const int yLine = y;
		line("Round", buf);
		// drawn, not a font glyph: U+221E is not in every font the game may get for Tahoma (Linux), a box otherwise.
		// As tall as the digits, centered on them, with their stroke width (the body font's measured proportions:
		// digits INF_DIGIT_TOP..+INF_DIGIT_TALL of the font height, stroke INF_STROKE of it)
		constexpr float INF_DIGIT_TOP = 0.235f, INF_DIGIT_TALL = 0.57f, INF_STROKE = 0.07f;
		const float ft = (float)FontTall(S().body);
		const int signTall = std::max(5, (int)lroundf(ft * INF_DIGIT_TALL));
		const int signWide = std::max(9, (int)lroundf(signTall * 1.9f));
		DrawInfinity(pad + labelW + textW, yLine + (int)lroundf(ft * INF_DIGIT_TOP), signWide, signTall,
		    std::max(1.0f, ft * INF_STROKE), COL_TEXT_LIGHT, COL_BG_WINDOW);
	}
	else
	{
		if (m_iRound >= 0 && m_iRoundLimit > 0)
			snprintf(buf, sizeof(buf), "%d / %d", m_iRound, m_iRoundLimit);
		else if (m_iRound > 0)
			snprintf(buf, sizeof(buf), "%d", m_iRound);
		else
			snprintf(buf, sizeof(buf), "-");
		line("Round", buf);
	}
	// the game's mp_timeleft where it ends the map by time (not co_ maps: there it is never set and can be the last
	// map's), else AMX Mod X's amx_timeleft (its map vote's time limit)
	const int timeLeft = m_Server.mode != SboServerRow::MODE_COOP && m_iTimeLeft > 0 ? m_iTimeLeft : m_iAmxTimeLeft;
	if (timeLeft > 0)
		snprintf(buf, sizeof(buf), "%d:%02d", timeLeft / 60, timeLeft % 60);
	else
		snprintf(buf, sizeof(buf), "-");
	line("Time left", buf);
	line("Friendly fire", m_iFriendlyFire < 0 ? "-" : (m_iFriendlyFire ? "On" : "Off"));
	line("Address", m_Server.Address().c_str());
	y += gap - S().M(4);

	// players: the A2S player list (names; bots are in it too, A2S does not mark them)
	FillRect(pad, y, w, 1, COL_BORDER_LIST);
	y += S().M(10);
	if (m_Server.bots > 0)
		snprintf(buf, sizeof(buf), "Players and bots (%d)", (int)m_Players.size());
	else
		snprintf(buf, sizeof(buf), "Players (%d)", (int)m_Players.size());
	DrawSpacedUpper(S().small, COL_TEXT_DIM, pad, y, buf);
	// the time each has been connected (A2S); levels are left out
	const int lw = SpacedUpperWide(S().small, "Time");
	DrawSpacedUpper(S().small, COL_TEXT_DIM, pad + w - lw, y, "Time");
	y += FontTall(S().small) + S().M(4);

	const int nameTall = FontTall(S().body) + S().M(4);
	// the list fills the rest of the panel; more rows than fit scroll (wheel, the bar in the right padding)
	m_iListTop = y;
	m_iListBottom = GetTall() - pad / 2;
	m_iRowsFit = std::max(1, (m_iListBottom - m_iListTop + S().M(4)) / nameTall);
	m_iBarW = std::max(2, S().M(4));
	m_iBarX = pad + w + (pad - m_iBarW) / 2;
	if (m_Players.empty())
	{
		DrawText(S().body, COL_TEXT_DIM, pad, y, m_bPlayersDone ? L"-" : L"...");
		return;
	}
	const int n = (int)m_Players.size();
	m_iPlayerFirst = std::max(0, std::min(m_iPlayerFirst, n - m_iRowsFit));
	if (n > m_iRowsFit)
	{
		const int track = m_iListBottom - m_iListTop;
		const int thumb = std::max(S().M(16), track * m_iRowsFit / n);
		const int maxFirst = n - m_iRowsFit;
		const int thumbY = m_iListTop + (track - thumb) * m_iPlayerFirst / maxFirst;
		FillRect(m_iBarX, m_iListTop, m_iBarW, track, COL_BG_INPUT);
		FillRect(m_iBarX, thumbY, m_iBarW, thumb, m_bDraggingBar ? COL_TEXT : COL_BORDER);
	}
	for (int i = m_iPlayerFirst; i < n && i < m_iPlayerFirst + m_iRowsFit; i++)
	{
		const PlayerLine &pl = m_Players[i];
		char t[16];
		if (pl.seconds >= 3600)
			snprintf(t, sizeof(t), "%d:%02d:%02d", pl.seconds / 3600, pl.seconds / 60 % 60, pl.seconds % 60);
		else
			snprintf(t, sizeof(t), "%d:%02d", pl.seconds / 60, pl.seconds % 60);
		const std::wstring wt = Widen(t);
		const int tw = TextWide(S().body, wt);
		DrawText(S().body, COL_TEXT_LIGHT, pad, y, Widen(pl.name.c_str()), w - tw - S().M(12));
		DrawText(S().body, COL_TEXT_DIM, pad + w - tw, y, wt);
		y += nameTall;
	}
}

//-----------------------------------------------------------------------------
// Small dialogs: Add server, server password (SBO look, on the GameUI viewport)
//-----------------------------------------------------------------------------
namespace
{
class CSboBrowserDialog : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE(CSboBrowserDialog, vgui2::Frame);

public:
	CSboBrowserDialog(vgui2::Panel *pParent, const char *pName, const char *title, const char *okText, bool hidden)
	    : BaseClass(pParent, pName)
	    , m_Title(title)
	{
		SetTitleBarVisible(false);
		SetMoveable(false);
		SetSizeable(false);
		SetDeleteSelfOnClose(true);
		SetProportional(true);
		SetScheme(BrowserScheme());
		UpdateStyle(this);
		m_pEntry = new CSboBrowserEntry(this, "Entry", "");
		m_pEntry->SetTextHidden(hidden);
		m_pEntry->SetMaximumCharCount(hidden ? 63 : 127);
		m_pOk = new CSboBrowserButton(this, "Ok", okText, "Ok", CSboBrowserButton::KIND_PRIMARY);
		m_pCancel = new CSboBrowserButton(this, "Cancel", "Cancel", "Close", CSboBrowserButton::KIND_NORMAL);
		// sizes in the proportional base (640x480) through the scale helper, as Aura's dialogs
		SetSize(sboui::GetScaledValue(this, 260), sboui::GetScaledValue(this, 112));
		MoveToCenterOfScreen();
	}

	void SetMessage(const char *text)
	{
		m_Message = text ? text : "";
		Repaint();
	}

	void Activate() override
	{
		BaseClass::Activate();
		m_pEntry->RequestFocus();
	}

	void PerformLayout() override
	{
		BaseClass::PerformLayout();
		const int pad = S().M(20), w = GetWide();
		const int entryTall = FontTall(S().body) + 2 * S().M(6);
		const int titleTall = FontTall(S().name);
		m_pEntry->SetBounds(pad, pad + titleTall + S().M(10), w - 2 * pad, entryTall);
		const int bh = S().M(36);
		const int by = GetTall() - pad - bh;
		const int okW = m_pOk->GetContentWide(), cancelW = m_pCancel->GetContentWide();
		m_pOk->SetBounds(w - pad - okW, by, okW, bh);
		m_pCancel->SetBounds(w - pad - okW - S().M(10) - cancelW, by, cancelW, bh);
	}

	void PaintBackground() override { FillRect(0, 0, GetWide(), GetTall(), Color(12, 15, 12, 250)); }
	void PaintBorder() override { OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER); }
	void Paint() override
	{
		const int pad = S().M(20);
		DrawText(S().name, COL_TEXT, pad, pad, Widen(m_Title.c_str()), GetWide() - 2 * pad);
		if (!m_Message.empty())
		{
			int ex, ey, ew, eh;
			m_pEntry->GetBounds(ex, ey, ew, eh);
			DrawText(S().small, COL_PING_BAD, pad, ey + eh + S().M(6), Widen(m_Message.c_str()), GetWide() - 2 * pad);
		}
	}

	void OnKeyCodeTyped(vgui2::KeyCode code) override
	{
		if (code == vgui2::KEY_ESCAPE)
			Close();
		else if (code == vgui2::KEY_ENTER)
			OnCommand("Ok");
		else
			BaseClass::OnKeyCodeTyped(code);
	}

	MESSAGE_FUNC(OnTextNewLine, "TextNewLine") { OnCommand("Ok"); }

protected:
	std::string m_Title;
	std::string m_Message;
	CSboBrowserEntry *m_pEntry = nullptr;
	CSboBrowserButton *m_pOk = nullptr;
	CSboBrowserButton *m_pCancel = nullptr;
};

class CSboAddServerDialog : public CSboBrowserDialog
{
	DECLARE_CLASS_SIMPLE(CSboAddServerDialog, CSboBrowserDialog);

public:
	CSboAddServerDialog(CSboServerBrowser *pBrowser)
	    : BaseClass(g_pGameUIViewport, "SboAddServer", "Add server (address:port)", "Add", false)
	{
		m_hBrowser = pBrowser;
	}

	void OnCommand(const char *command) override
	{
		if (!Q_stricmp(command, "Ok"))
		{
			char text[128];
			m_pEntry->GetText(text, sizeof(text));
			uint32_t ip = 0;
			uint16_t port = 0;
			if (!sbo_address::Resolve(text, ip, port))
			{
				SetMessage("Could not find a server at that address");
				return;
			}
			if (m_hBrowser.Get())
				m_hBrowser->AddFavorite(ip, port, port); // GoldSrc servers answer queries on the game port
			Close();
			return;
		}
		BaseClass::OnCommand(command);
	}

private:
	vgui2::DHANDLE<CSboServerBrowser> m_hBrowser;
};

class CSboPasswordDialog : public CSboBrowserDialog
{
	DECLARE_CLASS_SIMPLE(CSboPasswordDialog, CSboBrowserDialog);

public:
	CSboPasswordDialog(CSboServerBrowser *pBrowser)
	    : BaseClass(g_pGameUIViewport, "SboServerPassword", "This server needs a password", "Connect", true)
	{
		m_hBrowser = pBrowser;
		// the engine sends the "password" cvar when it connects: start with it (masked), so Enter joins with it
		if (sbo::CvarHandle cvar = sbo::FindCvar("password"))
		{
			const char *current = sbo::CvarGetString(cvar);
			if (current && current[0] && SafePassword(current))
				m_pEntry->SetText(current);
		}
	}

	void OnCommand(const char *command) override
	{
		if (!Q_stricmp(command, "Ok"))
		{
			char text[64];
			m_pEntry->GetText(text, sizeof(text));
			if (!SafePassword(text))
			{
				SetMessage("The password can't contain quotes or ';'");
				return;
			}
			if (m_hBrowser.Get())
			{
				KeyValues *kv = new KeyValues("JoinWithPassword");
				kv->SetString("password", text);
				PostMessage(m_hBrowser.Get(), kv);
			}
			memset(text, 0, sizeof(text));
			Close();
			return;
		}
		BaseClass::OnCommand(command);
	}

private:
	vgui2::DHANDLE<CSboServerBrowser> m_hBrowser;
};
} // namespace

//-----------------------------------------------------------------------------
// CSboServerBrowser
//-----------------------------------------------------------------------------
static const char *const s_TabNames[CSboServerBrowser::TAB_COUNT] = { "Internet", "Favorites", "History", "Friends", "LAN" };
// Max ping choices; Any is the default (players are worldwide: no server is hidden by default)
static const int s_PingValues[] = { 0, 50, 100, 150 };
static const char *const s_PingNames[] = { "Any", "< 50", "< 100", "< 150" };

// The filter choices, kept for the game session (no cvar: a new one needs the owner's approval)
static struct
{
	int mode = 0;
	char map[32] = "";
	int ping = 0;
	bool hasPlayers = false;
	bool notFull = false;
	bool noPassword = false;
	int tab = 0;
} s_Remembered;

// Where the window was (moved / resized)
static struct
{
	bool valid = false;
	int x = 0, y = 0, wide = 0, tall = 0;
	int screenWide = 0, screenTall = 0; //!< the resolution they were saved at (0: unknown)
} s_Bounds;

// The window's place and the filter choices are kept in the mod folder between games (as the engine's browser keeps
// its own in platform/config/ServerBrowser.vdf); no cvar. Read once per game, written when they change.
static constexpr const char SETTINGS_FILE[] = "serverbrowser.vdf";
static constexpr const char SETTINGS_PATH_ID[] = "GAMECONFIG"; // the mod folder, where config.cfg is written

static void LoadSettings()
{
	static bool s_bLoaded = false;
	if (s_bLoaded)
		return;
	s_bLoaded = true;
	KeyValues *kv = new KeyValues("ServerBrowser");
	if (kv->LoadFromFile(g_pFullFileSystem, SETTINGS_FILE, SETTINGS_PATH_ID))
	{
		s_Remembered.tab = std::max(0, std::min(kv->GetInt("tab"), (int)CSboServerBrowser::TAB_COUNT - 1));
		s_Remembered.mode = std::max(0, std::min(kv->GetInt("mode"), 3));
		Q_strncpy(s_Remembered.map, kv->GetString("map"), sizeof(s_Remembered.map));
		s_Remembered.ping = std::max(0, std::min(kv->GetInt("ping"), (int)(sizeof(s_PingValues) / sizeof(s_PingValues[0])) - 1));
		s_Remembered.hasPlayers = kv->GetInt("hasplayers") != 0;
		s_Remembered.notFull = kv->GetInt("notfull") != 0;
		s_Remembered.noPassword = kv->GetInt("nopassword") != 0;
		if (KeyValues *w = kv->FindKey("window"))
		{
			s_Bounds.x = w->GetInt("x");
			s_Bounds.y = w->GetInt("y");
			s_Bounds.wide = w->GetInt("wide");
			s_Bounds.tall = w->GetInt("tall");
			s_Bounds.screenWide = w->GetInt("screenwide");
			s_Bounds.screenTall = w->GetInt("screentall");
			s_Bounds.valid = s_Bounds.wide > 0 && s_Bounds.tall > 0; // kept on the screen by KeepOnScreen
		}
		if (KeyValues *c = kv->FindKey("columns"))
		{
			for (int i = 0; i < CSboServerListPanel::COL_COUNT; i++)
			{
				const int w = s_ColumnName[i][0] ? c->GetInt(s_ColumnName[i]) : 0;
				s_ColumnMock[i] = i == CSboServerListPanel::COL_LOCK || i == CSboServerListPanel::COL_VAC ||
				                          i == CSboServerListPanel::COL_NAME
				                      ? 0
				                      : (w >= 24 && w <= 2000 ? w : 0); // Server takes what is left
			}
		}
	}
	kv->deleteThis();
}

static void SaveSettings()
{
	KeyValues *kv = new KeyValues("ServerBrowser");
	kv->SetInt("tab", s_Remembered.tab);
	kv->SetInt("mode", s_Remembered.mode);
	kv->SetString("map", s_Remembered.map);
	kv->SetInt("ping", s_Remembered.ping);
	kv->SetInt("hasplayers", s_Remembered.hasPlayers ? 1 : 0);
	kv->SetInt("notfull", s_Remembered.notFull ? 1 : 0);
	kv->SetInt("nopassword", s_Remembered.noPassword ? 1 : 0);
	if (s_Bounds.valid)
	{
		KeyValues *w = kv->FindKey("window", true);
		w->SetInt("x", s_Bounds.x);
		w->SetInt("y", s_Bounds.y);
		w->SetInt("wide", s_Bounds.wide);
		w->SetInt("tall", s_Bounds.tall);
		w->SetInt("screenwide", s_Bounds.screenWide);
		w->SetInt("screentall", s_Bounds.screenTall);
	}
	KeyValues *c = nullptr;
	for (int i = 0; i < CSboServerListPanel::COL_COUNT; i++)
	{
		if (s_ColumnMock[i] <= 0)
			continue;
		if (!c)
			c = kv->FindKey("columns", true);
		c->SetInt(s_ColumnName[i], s_ColumnMock[i]);
	}
	kv->SaveToFile(g_pFullFileSystem, SETTINGS_FILE, SETTINGS_PATH_ID);
	kv->deleteThis();
}

CSboServerBrowser::CSboServerBrowser(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboServerBrowser")
{
	SetTitleBarVisible(false); // drawn here; the frame's caption grip still moves it (made as tall as the header row)
	SetMoveable(true);
	SetSizeable(true);
	SetDeleteSelfOnClose(true);
	SetProportional(true); // GetProportionalScale: screen height / 480
	SetScheme(BrowserScheme());
	UpdateStyle(this);
	m_Pinger.m_pBrowser = this;

	m_uAppID = BrowserAppID();

	m_pClose = new CSboBrowserButton(this, "CloseButton", "", "Close", CSboBrowserButton::KIND_CLOSE);
	for (int i = 0; i < TAB_COUNT; i++)
	{
		char cmd[16];
		snprintf(cmd, sizeof(cmd), "tab %d", i);
		m_pTabs[i] = new CSboBrowserButton(this, "Tab", s_TabNames[i], cmd, CSboBrowserButton::KIND_TAB);
	}

	m_pModeFilter = new CSboBrowserSelect(this, "ModeFilter");
	m_pModeFilter->AddItem("All modes");
	m_pModeFilter->AddItem("Co-op");
	m_pModeFilter->AddItem("Bounty Killer");
	m_pModeFilter->AddItem("Battle Royale");
	m_pModeFilter->AddActionSignalTarget(this);
	m_pMapFilter = new CSboBrowserEntry(this, "MapFilter", "co_");
	m_pMapFilter->SetMaximumCharCount(31);
	m_pMapFilter->AddActionSignalTarget(this);
	m_pPingFilter = new CSboBrowserSelect(this, "PingFilter");
	for (const char *name : s_PingNames)
		m_pPingFilter->AddItem(name);
	m_pPingFilter->AddActionSignalTarget(this);
	m_pHasPlayers = new CSboBrowserCheck(this, "HasPlayers", "Has players");
	m_pHasPlayers->AddActionSignalTarget(this);
	m_pNotFull = new CSboBrowserCheck(this, "NotFull", "Not full");
	m_pNotFull->AddActionSignalTarget(this);
	m_pNoPassword = new CSboBrowserCheck(this, "NoPassword", "No password");
	m_pNoPassword->AddActionSignalTarget(this);

	m_pList = new CSboServerListPanel(this);
	m_pList->AddActionSignalTarget(this);
	m_pInfo = new CSboServerInfoPanel(this);

	m_pAddServer = new CSboBrowserButton(this, "AddServer", "Add server", "AddServer", CSboBrowserButton::KIND_NORMAL);
	m_pAddFavorite = new CSboBrowserButton(this, "AddFavorite", "Add to favorites", "AddFavorite", CSboBrowserButton::KIND_NORMAL);
	m_pRefresh = new CSboBrowserButton(this, "Refresh", "Refresh", "Refresh", CSboBrowserButton::KIND_NORMAL);
	m_pConnect = new CSboBrowserButton(this, "Connect", "Connect", "Connect", CSboBrowserButton::KIND_PRIMARY);

	sboui::ApplySymbolSubstitutes(this); // Linux: the list's scroll bar arrows

	// the choices made earlier (this game or a saved one)
	LoadSettings();
	m_pModeFilter->SetSelected(s_Remembered.mode);
	m_pMapFilter->SetText(s_Remembered.map);
	m_pPingFilter->SetSelected(s_Remembered.ping);
	m_pHasPlayers->SetChecked(s_Remembered.hasPlayers);
	m_pNotFull->SetChecked(s_Remembered.notFull);
	m_pNoPassword->SetChecked(s_Remembered.noPassword);

	ApplyFilter();
	SetTab(s_Remembered.tab);
}

CSboServerBrowser::~CSboServerBrowser()
{
	ReleaseRequest();
}

void CSboServerBrowser::ReleaseRequest()
{
	m_Pinger.Cancel();
	ISteamMatchmakingServers *pServers = ClientSteamContext().SteamMatchmakingServers();
	if (m_hRequest && pServers)
		pServers->ReleaseRequest(m_hRequest); // cancels it: no more callbacks
	m_hRequest = nullptr;
	m_bRefreshing = false;
}

//! Bounds of GameUI's main menu (a visible panel named or classed "...GameMenu..." under the GameUI panel)
static bool FindMenuIn(vgui2::VPANEL panel, int depth, int &x, int &y, int &w, int &h)
{
	if (!panel || depth > 5 || !g_pVGuiPanel->IsVisible(panel))
		return false;
	const char *name = g_pVGuiPanel->GetName(panel);
	const char *cls = g_pVGuiPanel->GetClassName(panel);
	if ((name && Q_stristr(name, "GameMenu")) || (cls && Q_stristr(cls, "GameMenu")))
	{
		g_pVGuiPanel->GetAbsPos(panel, x, y);
		g_pVGuiPanel->GetSize(panel, w, h);
		return w > 0 && h > 0;
	}
	const int count = g_pVGuiPanel->GetChildCount(panel);
	for (int i = 0; i < count; i++)
	{
		if (FindMenuIn(g_pVGuiPanel->GetChild(panel, i), depth + 1, x, y, w, h))
			return true;
	}
	return false;
}

static bool FindGameMenuBounds(int &x, int &y, int &w, int &h)
{
	return g_pEngineVGui && FindMenuIn(g_pEngineVGui->GetPanel(PANEL_GAMEUIDLL), 0, x, y, w, h);
}

void CSboServerBrowser::Open()
{
	ResetStyle(this); // the normal look for this screen (another window may have shrunk it)
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	SetMinimumSize(std::min(S().M(MIN_WIDE), sw), std::min(S().M(MIN_TALL), sh));
	if (s_Bounds.valid)
	{
		// saved at another resolution: the same place and size relative to the screen
		int x = s_Bounds.x, y = s_Bounds.y, w = s_Bounds.wide, h = s_Bounds.tall;
		if (s_Bounds.screenWide > 0 && s_Bounds.screenTall > 0 && (s_Bounds.screenWide != sw || s_Bounds.screenTall != sh))
		{
			x = x * sw / s_Bounds.screenWide;
			w = w * sw / s_Bounds.screenWide;
			y = y * sh / s_Bounds.screenTall;
			h = h * sh / s_Bounds.screenTall;
		}
		// never over the game menu: moved right of it, the player's size kept (wider than the room: at the right edge)
		int mx, my, mw, mh;
		const int menuRight = FindGameMenuBounds(mx, my, mw, mh) && mx + mw < sw / 2 ? mx + mw : (int)(sw * MENU_SHARE);
		if (x < menuRight)
			x = std::max(0, std::min(menuRight + S().M(24), sw - w));
		SetBounds(x, y, w, h);
	}
	else
	{
		// the design's size, but right of the game menu (its entries stay visible and usable, as with the engine's
		// browser): GameUI's menu panel where it can be found, else MENU_SHARE of the screen width
		int menuRight = (int)(sw * MENU_SHARE);
		int mx, my, mw, mh;
		if (FindGameMenuBounds(mx, my, mw, mh))
			menuRight = mx + mw;
		const int maxWide = sw - menuRight - S().M(24) - S().M(16);
		const int wide = std::max(std::min(S().M(MOCK_WIDE), maxWide), std::min(S().M(MIN_WIDE), sw));
		if (wide > maxWide && maxWide > 0)
		{
			// a narrow screen (1280x720 and the like): everything smaller, so the window fits beside the menu
			SetStyleScale(S().scale * (float)maxWide / (float)wide);
			SetMinimumSize(std::min(S().M(MIN_WIDE), sw), std::min(S().M(MIN_TALL), sh));
			const int tall2 = std::min(S().M(MOCK_TALL), sh);
			SetBounds(sw - maxWide - S().M(24), (sh - tall2) / 2, maxWide, tall2);
			KeepOnScreen();
			InvalidateLayout(true, true);
			Activate();
			return;
		}
		const int tall = std::min(S().M(MOCK_TALL), sh);
		SetBounds(sw - wide - S().M(24), (sh - tall) / 2, wide, tall);
	}
	KeepOnScreen();
	InvalidateLayout(true, true);
	Activate();
}

void CSboServerBrowser::KeepOnScreen()
{
	int sw, sh, x, y, w, h;
	vgui2::surface()->GetScreenSize(sw, sh);
	GetBounds(x, y, w, h);
	w = std::min(w, sw);
	h = std::min(h, sh);
	x = std::max(0, std::min(x, sw - w));
	y = std::max(0, std::min(y, sh - h));
	SetBounds(x, y, w, h);
}

void CSboServerBrowser::SaveBounds()
{
	GetBounds(s_Bounds.x, s_Bounds.y, s_Bounds.wide, s_Bounds.tall);
	vgui2::surface()->GetScreenSize(s_Bounds.screenWide, s_Bounds.screenTall);
	s_Bounds.valid = true;
}

void CSboServerBrowser::OnClose()
{
	SaveBounds();
	SaveSettings();
	BaseClass::OnClose();
}

void CSboServerBrowser::OnScreenSizeChanged(int oldWide, int oldTall)
{
	BaseClass::OnScreenSizeChanged(oldWide, oldTall);
	UpdateStyle(this);
	KeepOnScreen();
	InvalidateLayout(true, true);
}

void CSboServerBrowser::SetTab(int tab)
{
	if (tab < 0 || tab >= TAB_COUNT)
		return;
	m_iTab = tab;
	s_Remembered.tab = tab;
	m_flSaveAt = vgui2::system()->GetCurrentTime() + 1.0;
	for (int i = 0; i < TAB_COUNT; i++)
		m_pTabs[i]->SetActiveTab(i == tab);
	StartRefresh();
}

void CSboServerBrowser::StartRefresh(bool keepRows)
{
	ReleaseRequest();
	m_bKeepingRows = keepRows && m_pList->RowCount() > 0;
	if (!m_bKeepingRows)
	{
		m_pList->Clear();
		m_pInfo->SetServer(nullptr);
		m_flRefreshedAt = -1.0;
	}

	ClientSteamContext().Activate();
	ISteamMatchmakingServers *pServers = ClientSteamContext().SteamMatchmakingServers();
	if (!pServers)
	{
		m_pList->SetEmptyText("Steam is not available: start the game from Steam to find servers");
		UpdateButtons();
		return;
	}
	m_uAppID = BrowserAppID();

	// only SBO servers: the master server's game folder filter (NextClient / Steam's own browser filter the same way)
	static MatchMakingKeyValuePair_t s_Filters[1];
	strncpy(s_Filters[0].m_szKey, "gamedir", sizeof(s_Filters[0].m_szKey) - 1);
	strncpy(s_Filters[0].m_szValue, "syndicate", sizeof(s_Filters[0].m_szValue) - 1);
	MatchMakingKeyValuePair_t *pFilters = s_Filters;
	const uint32 nFilters = 1;

	switch (m_iTab)
	{
	case TAB_FAVORITES:
		m_hRequest = pServers->RequestFavoritesServerList(m_uAppID, &pFilters, nFilters, this);
		break;
	case TAB_HISTORY:
		m_hRequest = pServers->RequestHistoryServerList(m_uAppID, &pFilters, nFilters, this);
		break;
	case TAB_FRIENDS:
		m_hRequest = pServers->RequestFriendsServerList(m_uAppID, &pFilters, nFilters, this);
		break;
	case TAB_LAN:
		m_hRequest = pServers->RequestLANServerList(m_uAppID, this);
		break;
	default:
		m_hRequest = pServers->RequestInternetServerList(m_uAppID, &pFilters, nFilters, this);
		break;
	}
	m_bRefreshing = m_hRequest != nullptr;
	m_flRequestStart = vgui2::system()->GetCurrentTime();
	m_nResponded = m_nFailed = 0;
	BrowserLog("request tab %d app %u handle %p", m_iTab, m_uAppID, (void *)m_hRequest);
	m_pList->SetEmptyText(m_bRefreshing ? "Refreshing..." : "No servers");
	UpdateButtons();
	Repaint();
}

namespace
{
//! A Steam server item as a list row; false for a game folder other than syndicate
bool RowFromItem(const gameserveritem_t &item, SboServerRow &row)
{
	if (Q_stricmp(item.m_szGameDir, "syndicate"))
		return false;
	row.ip = item.m_NetAdr.GetIP();
	row.connPort = item.m_NetAdr.GetConnectionPort();
	row.queryPort = item.m_NetAdr.GetQueryPort();
	row.name = CleanName(item.GetName());
	if (row.name.empty())
		row.name = row.Address();
	char map[sizeof(item.m_szMap) + 1];
	Q_strncpy(map, item.m_szMap, sizeof(map));
	row.map = map;
	row.mode = ModeOfMap(map);
	row.players = std::max(0, item.m_nPlayers);
	row.bots = std::max(0, item.m_nBotPlayers);
	row.humans = std::max(0, row.players - row.bots);
	row.maxPlayers = std::max(0, item.m_nMaxPlayers);
	row.ping = std::max(0, item.m_nPing);
	row.password = item.m_bPassword;
	row.secure = item.m_bSecure;
	return true;
}
} // namespace

void CSboServerBrowser::ServerResponded(HServerListRequest hRequest, int iServer)
{
	ISteamMatchmakingServers *pServers = ClientSteamContext().SteamMatchmakingServers();
	if (hRequest != m_hRequest || !pServers)
	{
		BrowserLog("responded %d for an old request %p (current %p)", iServer, (void *)hRequest, (void *)m_hRequest);
		return;
	}
	m_nResponded++;
	if (m_bKeepingRows)
	{
		// the new answer's first server: the previous answer's rows go
		m_bKeepingRows = false;
		m_pList->Clear();
	}
	gameserveritem_t *pItem = pServers->GetServerDetails(hRequest, iServer);
	SboServerRow row;
	if (!pItem || !RowFromItem(*pItem, row))
		return; // the LAN list has no filter; any other game folder is not SBO
	row.serverIndex = iServer;
	UpdateRow(row);
	// The Internet list's numbers come from Steam's server list and can be old (a player who joined minutes ago
	// missing; the other tabs ask the servers): each server is asked once more itself (PingServer)
	if (m_iTab == TAB_INTERNET)
		m_Pinger.Queue(row);
}

void CSboServerBrowser::UpdateRow(const SboServerRow &row)
{
	m_pList->AddOrUpdate(row);
	// the selected server came back in this list (a refresh, another tab): its details again
	m_pInfo->SetServer(m_pList->GetSelected());
	UpdateButtons();
	Repaint();
}

//-----------------------------------------------------------------------------
// CSboServerPinger: one PingServer (A2S_INFO) at a time
//-----------------------------------------------------------------------------
CSboServerPinger::~CSboServerPinger()
{
	Cancel();
}

void CSboServerPinger::Cancel()
{
	ISteamMatchmakingServers *pServers = ClientSteamContext().SteamMatchmakingServers();
	if (m_hQuery != HSERVERQUERY_INVALID && pServers)
		pServers->CancelServerQuery(m_hQuery);
	m_hQuery = HSERVERQUERY_INVALID;
	m_Queue.clear();
}

void CSboServerPinger::Queue(const SboServerRow &row)
{
	for (const SboServerRow &r : m_Queue)
	{
		if (r.SameServer(row))
			return;
	}
	if (m_hQuery != HSERVERQUERY_INVALID && m_Current.SameServer(row))
		return;
	if (m_Queue.size() < 256)
		m_Queue.push_back(row);
	Next();
}

void CSboServerPinger::Next()
{
	ISteamMatchmakingServers *pServers = ClientSteamContext().SteamMatchmakingServers();
	if (m_hQuery != HSERVERQUERY_INVALID || m_Queue.empty() || !pServers)
		return;
	m_Current = m_Queue.front();
	m_Queue.erase(m_Queue.begin());
	m_hQuery = pServers->PingServer(m_Current.ip, m_Current.queryPort, this);
}

void CSboServerPinger::ServerResponded(gameserveritem_t &server)
{
	m_hQuery = HSERVERQUERY_INVALID;
	SboServerRow row;
	if (m_pBrowser && RowFromItem(server, row))
	{
		row.serverIndex = m_Current.serverIndex;
		m_pBrowser->UpdateRow(row);
	}
	Next();
}

void CSboServerPinger::ServerFailedToRespond()
{
	m_hQuery = HSERVERQUERY_INVALID;
	Next();
}

void CSboServerBrowser::ServerFailedToRespond(HServerListRequest hRequest, int iServer)
{
	// not listed (as GameUI's browser does with servers that never answered)
	if (hRequest == m_hRequest)
		m_nFailed++;
}

void CSboServerBrowser::RefreshComplete(HServerListRequest hRequest, EMatchMakingServerResponse response)
{
	BrowserLog("complete %p (current %p) response %d: %d responded, %d failed, %d rows, %d shown, %.2f s",
	    (void *)hRequest, (void *)m_hRequest, (int)response, m_nResponded, m_nFailed, m_pList->RowCount(),
	    m_pList->VisibleCount(), vgui2::system()->GetCurrentTime() - m_flRequestStart);
	if (hRequest != m_hRequest)
		return;
	m_bRefreshing = false;
	const double now = vgui2::system()->GetCurrentTime();
	// Steam limits how often a client may ask for the Internet list: after a few requests within a minute it answers
	// "no servers listed" (eNoServersListedOnMasterServer) at once, with none, for about 20 seconds (measured on the
	// Linux client 2026-11-02: six refreshes within 20 s, then 0.15 s answers with no server; 18 s later a full
	// list again). That answer, shortly after a list that had servers, is that limit: the rows shown stay, and the
	// browser says when to try again.
	const bool limited = m_iTab == TAB_INTERNET && response == eNoServersListedOnMasterServer && m_nResponded == 0 &&
	                     ((s_flLastListed >= 0.0 && now - s_flLastListed < RATE_LIMIT_MEMORY) ||
	                         now - m_flRequestStart < RATE_LIMIT_INSTANT);
	if (limited)
	{
		m_flLimitedUntil = now + RATE_LIMIT_WAIT;
		BrowserLog("Steam is limiting requests: rows kept, try again in %.0f s", RATE_LIMIT_WAIT);
	}
	else
	{
		if (m_iTab == TAB_INTERNET && m_nResponded > 0)
		{
			s_flLastListed = now;
			m_flLimitedUntil = -1.0;
		}
		if (m_bKeepingRows)
			m_pList->Clear(); // the new answer has no server
		m_flRefreshedAt = now;
	}
	m_bKeepingRows = false;
	m_pList->SetEmptyText(limited ? LimitedText().c_str() : "No servers");
	m_iShownWait = LimitedSeconds();
	// the selected server is not in this list: no selection (nothing highlighted, no details)
	if (!m_pList->HasSelectedRow())
		m_pList->ClearSelection();
	UpdateButtons();
	Repaint();
}

void CSboServerBrowser::ApplyFilter()
{
	CSboServerListPanel::Filter f;
	f.mode = m_pModeFilter->GetSelected(); // 0 all, then SboServerRow::MODE_COOP, _BOUNTY, _BR
	char map[32];
	m_pMapFilter->GetText(map, sizeof(map));
	f.map = map;
	const int ping = m_pPingFilter->GetSelected();
	f.maxPing = ping >= 0 && ping < (int)(sizeof(s_PingValues) / sizeof(s_PingValues[0])) ? s_PingValues[ping] : 0;
	f.hasPlayers = m_pHasPlayers->IsChecked();
	f.notFull = m_pNotFull->IsChecked();
	f.noPassword = m_pNoPassword->IsChecked();
	s_Remembered.mode = f.mode;
	Q_strncpy(s_Remembered.map, map, sizeof(s_Remembered.map));
	s_Remembered.ping = ping;
	s_Remembered.hasPlayers = f.hasPlayers;
	s_Remembered.notFull = f.notFull;
	s_Remembered.noPassword = f.noPassword;
	m_flSaveAt = vgui2::system()->GetCurrentTime() + 1.0;
	m_pList->SetFilter(f);
	m_pInfo->SetServer(m_pList->GetSelected());
	UpdateButtons();
	Repaint();
}

int CSboServerBrowser::LimitedSeconds() const
{
	if (m_iTab != TAB_INTERNET || m_flLimitedUntil < 0.0)
		return 0;
	const double left = m_flLimitedUntil - vgui2::system()->GetCurrentTime();
	return left > 0.0 ? (int)ceil(left) : 0;
}

std::string CSboServerBrowser::LimitedText() const
{
	char buf[96];
	snprintf(buf, sizeof(buf), "Steam is limiting requests, try again in %d s", LimitedSeconds());
	return buf;
}

void CSboServerBrowser::UpdateButtons()
{
	const bool sel = m_pList->GetSelected() != nullptr;
	m_pConnect->SetEnabled(sel);
	m_pAddFavorite->SetEnabled(sel && m_iTab != TAB_FAVORITES);
}

void CSboServerBrowser::SetStatus(const char *text, float seconds)
{
	m_Status = text ? text : "";
	m_flStatusUntil = vgui2::system()->GetCurrentTime() + seconds;
	Repaint();
}

void CSboServerBrowser::OnSelectChanged()
{
	ApplyFilter();
}

void CSboServerBrowser::OnTextChanged(vgui2::Panel *panel)
{
	ApplyFilter();
}

void CSboServerBrowser::OnButtonToggled(vgui2::Panel *panel)
{
	ApplyFilter();
}

void CSboServerBrowser::OnServerSelected()
{
	m_pInfo->SetServer(m_pList->GetSelected());
	UpdateButtons();
	RefreshSelected();
}

// The list's numbers are from the moment the list was asked for: the selected server is asked again when it is
// picked and every SELECTED_REFRESH_S seconds while it stays selected (ServerResponded updates its row and panel).
void CSboServerBrowser::RefreshSelected()
{
	m_flNextSelectedRefresh = vgui2::system()->GetCurrentTime() + SELECTED_REFRESH_S;
	if (const SboServerRow *pRow = m_pList->GetSelected())
		m_Pinger.Queue(*pRow);
}

void CSboServerBrowser::OnServerActivated()
{
	OnCommand("Connect");
}

void CSboServerBrowser::OnJoinWithPassword(const char *password)
{
	if (m_PendingConnect.ip)
		ConnectTo(m_PendingConnect, password ? password : "");
}

void CSboServerBrowser::AddFavorite(uint32_t ip, uint16_t connPort, uint16_t queryPort)
{
	ISteamMatchmaking *pMM = ClientSteamContext().SteamMatchmaking();
	if (!pMM)
	{
		SetStatus("Steam is not available", 4.0f);
		return;
	}
	pMM->AddFavoriteGame(m_uAppID, ip, connPort, queryPort, k_unFavoriteFlagFavorite, (uint32)time(nullptr));
	SetStatus("Added to favorites", 4.0f);
	if (m_iTab == TAB_FAVORITES)
		StartRefresh();
}

void CSboServerBrowser::ConnectTo(const SboServerRow &row, const char *password)
{
	if (!row.ip || !row.connPort)
		return;
	// Steam's history list (GameUI's browser adds the server it connects to the same way)
	if (ISteamMatchmaking *pMM = ClientSteamContext().SteamMatchmaking())
		pMM->AddFavoriteGame(m_uAppID, row.ip, row.connPort, row.queryPort, k_unFavoriteFlagHistory, (uint32)time(nullptr));

	char cmd[160];
	// a password other than the "password" cvar's: set it, the engine sends the cvar when it connects
	sbo::CvarHandle cvar = sbo::FindCvar("password");
	const char *current = cvar ? sbo::CvarGetString(cvar) : nullptr;
	if (password && password[0] && !(current && !strcmp(current, password)))
	{
		if (!SafePassword(password))
			return;
		snprintf(cmd, sizeof(cmd), "password \"%s\"\n", password);
		sbo::ExecClientCmd(cmd);
		memset(cmd, 0, sizeof(cmd));
	}
	snprintf(cmd, sizeof(cmd), "connect %s\n", row.Address().c_str());
	VGui2_LoadingScreen_Connecting(row); // the joining screen knows the server's name and map at once
	sbo::ExecClientCmd(cmd);
	m_PendingConnect = SboServerRow();
	Close();
}

void CSboServerBrowser::OnCommand(const char *command)
{
	if (!strncmp(command, "tab ", 4))
	{
		SetTab(atoi(command + 4));
		return;
	}
	if (!Q_stricmp(command, "Refresh"))
	{
		StartRefresh(true);
		return;
	}
	if (!Q_stricmp(command, "Connect"))
	{
		const SboServerRow *pRow = m_pList->GetSelected();
		if (!pRow)
			return;
		ReleaseRequest(); // stop querying while connecting (Aura / GameUI)
		if (pRow->password)
		{
			// the password prompt; an empty entry keeps the "password" cvar as it is
			m_PendingConnect = *pRow;
			CSboPasswordDialog *pDlg = new CSboPasswordDialog(this);
			pDlg->Activate();
			return;
		}
		ConnectTo(*pRow, "");
		return;
	}
	if (!Q_stricmp(command, "AddFavorite"))
	{
		if (const SboServerRow *pRow = m_pList->GetSelected())
			AddFavorite(pRow->ip, pRow->connPort, pRow->queryPort);
		return;
	}
	if (!Q_stricmp(command, "AddServer"))
	{
		CSboAddServerDialog *pDlg = new CSboAddServerDialog(this);
		pDlg->Activate();
		return;
	}
	BaseClass::OnCommand(command);
}

void CSboServerBrowser::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ESCAPE)
		Close();
	else
		BaseClass::OnKeyCodeTyped(code);
}

void CSboServerBrowser::OnThink()
{
	BaseClass::OnThink();
	{
		int x, y, w, h;
		GetBounds(x, y, w, h);
		if (!s_Bounds.valid || x != s_Bounds.x || y != s_Bounds.y || w != s_Bounds.wide || h != s_Bounds.tall)
		{
			SaveBounds();
			m_flSaveAt = vgui2::system()->GetCurrentTime() + 1.0; // written once the moving / resizing stops
		}
		if (m_flSaveAt > 0.0 && vgui2::system()->GetCurrentTime() >= m_flSaveAt)
		{
			m_flSaveAt = 0.0;
			SaveSettings();
		}
	}
	if (vgui2::system()->GetCurrentTime() >= m_flNextSelectedRefresh)
		RefreshSelected();
	// "Refreshed n s ago": repaint when the number changes
	const double now = vgui2::system()->GetCurrentTime();
	const int secs = m_flRefreshedAt >= 0.0 ? (int)(now - m_flRefreshedAt) : -1;
	const bool statusOn = !m_Status.empty() && now < m_flStatusUntil;
	const int wait = LimitedSeconds();
	if (secs != m_iShownSeconds || wait != m_iShownWait || (!statusOn && !m_Status.empty()))
	{
		if (!statusOn)
			m_Status.clear();
		m_iShownSeconds = secs;
		if (wait != m_iShownWait && !m_bRefreshing)
			m_pList->SetEmptyText(wait > 0 ? LimitedText().c_str() : "No servers");
		m_iShownWait = wait;
		Repaint();
	}
}

void CSboServerBrowser::PerformLayout()
{
	BaseClass::PerformLayout();
	const Style &st = S();
	const int W = GetWide(), H = GetTall();
	const int padX = st.M(24), padY = st.M(20), gap = st.M(14);
	int y = padY;

	// header row: title, "Syndicate Black Ops · n servers", close button (32 x 32)
	const int headerTall = std::max(FontTall(st.title), st.M(32));
	const int closeSize = st.M(32);
	m_pClose->SetBounds(W - padX - closeSize, y + (headerTall - closeSize) / 2, closeSize, closeSize);
	y += headerTall + gap;

	// tabs: 40 high, 2 apart
	int x = padX;
	const int tabTall = st.M(40);
	for (int i = 0; i < TAB_COUNT; i++)
	{
		const int tw = m_pTabs[i]->GetContentWide();
		m_pTabs[i]->SetBounds(x, y, tw, tabTall);
		x += tw + st.M(2);
	}
	y += tabTall + 1 + gap;

	// filters: label + control groups 12 apart, 6 inside
	const int ctrlTall = FontTall(st.body) + 2 * st.M(6) + 2;
	const int g12 = st.M(12), g6 = st.M(6);
	x = padX;
	x += TextWide(st.body, L"Mode") + g6;
	m_pModeFilter->SetBounds(x, y, st.M(130), ctrlTall);
	x += st.M(130) + g12;
	x += TextWide(st.body, L"Map") + g6;
	m_pMapFilter->SetBounds(x, y, st.M(140) + 2 * st.M(8), ctrlTall);
	x += st.M(140) + 2 * st.M(8) + g12;
	x += TextWide(st.body, L"Max ping") + g6;
	m_pPingFilter->SetBounds(x, y, st.M(84), ctrlTall);
	x += st.M(84) + g12;
	m_iFilterRowY = y;
	m_iFilterRowTall = ctrlTall;
	// the check boxes go on a second row when they do not fit (small screens: the fonts keep a minimum size)
	for (CSboBrowserCheck *pCheck : { m_pHasPlayers, m_pNotFull, m_pNoPassword })
	{
		const int cw = pCheck->GetContentWide();
		if (x + cw > W - padX && x > padX)
		{
			x = padX;
			y += ctrlTall + g6;
		}
		pCheck->SetBounds(x, y, cw, ctrlTall);
		x += cw + g12;
	}
	y += ctrlTall + gap;

	// bottom buttons: 44 high
	const int bh = st.M(44);
	const int by = H - padY - bh;
	x = padX;
	for (CSboBrowserButton *pButton : { m_pAddServer, m_pAddFavorite })
	{
		const int bw = pButton->GetContentWide();
		pButton->SetBounds(x, by, bw, bh);
		x += bw + st.M(10);
	}
	x = W - padX;
	for (CSboBrowserButton *pButton : { m_pConnect, m_pRefresh })
	{
		const int bw = pButton->GetContentWide();
		x -= bw;
		pButton->SetBounds(x, by, bw, bh);
		x -= st.M(10);
	}
	m_iStatusRight = x;
	m_iBottomY = by;
	m_iBottomTall = bh;

	// list | 16 | info (300 wide)
	const int mainTall = by - gap - y;
	// the right panel grows with the window (300 of the design's 1200)
	const int infoW = std::max(st.M(240), W * 300 / MOCK_WIDE);
	m_pList->SetBounds(padX, y, W - 2 * padX - st.M(16) - infoW, mainTall);
	m_pInfo->SetBounds(W - padX - infoW, y, infoW, mainTall);
	m_iTabLineY = m_pTabs[0]->GetTall() + m_pTabs[0]->GetYPos();

	// drag the window by its header row (the frame's caption grip, 23 pixels high by default)
	if (vgui2::Panel *pCaption = FindChildByName("frame_caption"))
		pCaption->SetBounds(0, 0, m_pClose->GetXPos() - st.M(8), padY + headerTall);
}

void CSboServerBrowser::PaintBackground()
{
	FillRect(0, 0, GetWide(), GetTall(), COL_BG_WINDOW);
}

void CSboServerBrowser::PaintBorder()
{
	OutlineRect(0, 0, GetWide(), GetTall(), COL_BORDER);
}

void CSboServerBrowser::Paint()
{
	const Style &st = S();
	const int W = GetWide();
	const int padX = st.M(24), padY = st.M(20);
	const int headerTall = std::max(FontTall(st.title), st.M(32));

	// title
	DrawTextMid(st.title, COL_TEXT, padX, padY, headerTall, L"Find Servers");

	// "Syndicate Black Ops · n servers", 16 left of the close button
	char buf[64];
	const int shown = m_pList->GetVisibleCount();
	snprintf(buf, sizeof(buf), shown == 1 ? "%d server" : "%d servers", shown);
	const std::wstring game = L"Syndicate Black Ops", dot = L"·", count = Widen(buf);
	const int g6 = st.M(6);
	int x = m_pClose->GetXPos() - st.M(16) - TextWide(st.body, count);
	DrawTextMid(st.body, COL_TEXT_DIM, x, padY, headerTall, count);
	x -= g6 + TextWide(st.body, dot);
	DrawTextMid(st.body, COL_TEXT_FAINT, x, padY, headerTall, dot);
	x -= g6 + TextWide(st.body, game);
	DrawTextMid(st.body, COL_TEXT_DIM, x, padY, headerTall, game);

	// the line under the tabs
	FillRect(padX, m_iTabLineY, W - 2 * padX, 1, COL_BORDER);

	// filter labels
	const int g = st.M(6);
	DrawTextMid(st.body, COL_TEXT_DIM, m_pModeFilter->GetXPos() - g - TextWide(st.body, L"Mode"), m_iFilterRowY, m_iFilterRowTall, L"Mode");
	DrawTextMid(st.body, COL_TEXT_DIM, m_pMapFilter->GetXPos() - g - TextWide(st.body, L"Map"), m_iFilterRowY, m_iFilterRowTall, L"Map");
	DrawTextMid(st.body, COL_TEXT_DIM, m_pPingFilter->GetXPos() - g - TextWide(st.body, L"Max ping"), m_iFilterRowY, m_iFilterRowTall, L"Max ping");

	// status left of Refresh
	std::wstring status;
	if (!m_Status.empty())
		status = Widen(m_Status.c_str());
	else if (!m_bRefreshing && LimitedSeconds() > 0)
		status = Widen(LimitedText().c_str());
	else if (m_bRefreshing)
		status = L"Refreshing...";
	else if (m_iShownSeconds >= 0)
	{
		snprintf(buf, sizeof(buf), "Refreshed %d s ago", m_iShownSeconds);
		status = Widen(buf);
	}
	if (!status.empty())
		DrawTextMid(st.small, COL_TEXT_DIM, m_iStatusRight - TextWide(st.small, status), m_iBottomY, m_iBottomTall, status);

	// resize grip: three short diagonals in the bottom right corner
	const int H = GetTall(), gs = st.M(4);
	vgui2::surface()->DrawSetColor(COL_BORDER);
	for (int line = 1; line <= 3; line++)
	{
		const int len = line * gs;
		for (int i = 0; i < len; i++)
			vgui2::surface()->DrawFilledRect(W - 3 - len + i, H - 3 - i - 1, W - 3 - len + i + 1, H - 3 - i);
	}
}

//-----------------------------------------------------------------------------
// gameui_serverbrowser: the game menu runs it as "engine gameui_serverbrowser", which hides GameUI. Show GameUI again
// and open the browser on the next frame (as gameui_cl_open_adv_options, sbo_options.cpp). cl_experimental_menus 0:
// GameUI's own server browser instead (its "OpenServerBrowser" menu command).
//-----------------------------------------------------------------------------
static void ServerBrowser_f()
{
	if (!g_pGameUIViewport)
		return;
	g_pBaseUI->ActivateGameUI();
	if (!SboExperimentalMenus())
	{
		SboOpenGameUIDialog("OpenServerBrowser");
		return;
	}
	s_bOpenNextFrame = true;
}

void VGui2_ServerBrowser_Frame()
{
	if (!s_bOpenNextFrame || !g_pGameUIViewport)
		return;
	s_bOpenNextFrame = false;
	if (!s_hBrowser.Get())
		s_hBrowser = new CSboServerBrowser(g_pGameUIViewport);
	s_hBrowser->Open();
}

bool VGui2_ServerBrowser_CloseIfOpen()
{
	CSboServerBrowser *p = s_hBrowser.Get();
	if (!p || !p->IsVisible())
		return false;
	s_hBrowser = nullptr; // it deletes itself on close; the next open makes a new one
	p->Close();
	return true;
}

void VGui2_RegisterServerBrowserCommand()
{
	static bool s_bDone = false;
	if (s_bDone)
		return;
	s_bDone = true;
	sbo::AddCommand("gameui_serverbrowser", ServerBrowser_f); // Aura's command name
}
