//========= SBO VGUI2 port: text window (MOTD) =========//
//
// Purpose: see sbo_text_window.h.
//=================================================================//

#include <stdlib.h>
#include <string.h>

#include <tier2/tier2.h>
#include <FileSystem.h>

#include "sbo_bridge.h"
#include "sbo_html_panel.h"
#include "sbo_menus.h"
#include "sbo_text_window.h"
#include "sbo_ui_style.h"
#include "vgui2_api.h"

#include <math.h>
#include <wctype.h>
#include <algorithm>
#include <vector>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurface.h>

using namespace sbolook;

namespace
{
// the new in-game menus' colors (the loadout design's; sbo_loadout_menu.cpp)
const Color T_WINDOW(10, 14, 11, 245), T_BORDER(46, 74, 48, 255), T_LINE(31, 50, 33, 255), T_GREEN(143, 220, 122, 255),
    T_SOFT(184, 198, 184, 255), T_DEPLOY(47, 107, 42, 255), T_DEPLOY_LINE(95, 174, 82, 255),
    T_DEPLOY_TEXT(240, 255, 240, 255);
// the loadout menu's frame: 1320 x 800 on the page; the text column at most 1000 wide (lines that stay readable)
constexpr float TW_WIN_W = 1320, TW_WIN_H = 800, TW_BAR_H = 88, TW_PAD = 28, TW_BUTTON_H = 48, TW_TEXT_MAX = 1000;

bool Has(const int *r, int x, int y)
{
	return x >= r[0] && x < r[0] + r[2] && y >= r[1] && y < r[1] + r[3];
}

//! A map text that is a drawing or a table keeps its line breaks: lines with runs of spaces inside them (columns),
//! box-drawing runs ("---", "===", "|", "+-") or few letters
bool LooksLikeArt(const std::vector<std::wstring> &lines)
{
	int text = 0, art = 0;
	for (const std::wstring &l : lines)
	{
		if (l.empty())
			continue;
		text++;
		size_t first = l.find_first_not_of(L' ');
		const std::wstring t = first == std::wstring::npos ? L"" : l.substr(first);
		int letters = 0;
		for (wchar_t c : t)
			letters += iswalpha(c) ? 1 : 0;
		if (t.find(L"   ") != std::wstring::npos || t.find(L"---") != std::wstring::npos ||
		    t.find(L"===") != std::wstring::npos || t.find(L'|') != std::wstring::npos ||
		    t.find(L"+-") != std::wstring::npos || letters * 2 < (int)t.size())
			art++;
	}
	return text > 0 && art * 4 >= text; // a quarter of the lines
}

//! A list item or heading line: never joined to the next
bool IsListOrHeading(const std::wstring &l)
{
	size_t i = l.find_first_not_of(L' ');
	if (i == std::wstring::npos)
		return true;
	const wchar_t c = l[i];
	if (c == L'-' || c == L'*' || c == L'#' || c == L'>' || c == 0x2022)
		return true;
	if (iswdigit(c))
		return true;
	return l.find(L" : ") != std::wstring::npos || (!l.empty() && l.back() == L':');
}

//! The mapper's text with its hard line breaks (often at ~50 characters) joined into paragraphs: a line goes on with
//! the next one (a space between) when it is long for this file (at least 60 % of its longest line), the next line
//! is not empty, and neither is a list item or heading. Empty lines stay paragraph breaks (runs of them become one),
//! short lines stay as they are, a drawing or table keeps all its breaks.
std::vector<std::wstring> ReflowImpl(const std::wstring &text)
{
	std::vector<std::wstring> in;
	size_t start = 0;
	while (start <= text.size())
	{
		size_t nl = text.find(L'\n', start);
		if (nl == std::wstring::npos)
			nl = text.size();
		std::wstring l = text.substr(start, nl - start);
		l.erase(std::remove(l.begin(), l.end(), L'\r'), l.end());
		while (!l.empty() && (l.back() == L' ' || l.back() == L'\t'))
			l.pop_back();
		in.push_back(l);
		start = nl + 1;
	}
	std::vector<std::wstring> out;
	if (LooksLikeArt(in))
	{
		for (const std::wstring &l : in)
		{
			if (l.empty() && (out.empty() || out.back().empty()))
				continue;
			out.push_back(l);
		}
	}
	else
	{
		size_t longest = 0;
		for (const std::wstring &l : in)
			longest = std::max(longest, l.size());
		const size_t joinAt = longest * 6 / 10;
		std::wstring para;
		for (size_t i = 0; i < in.size(); i++)
		{
			const std::wstring &l = in[i];
			if (l.empty())
			{
				if (!para.empty())
					out.push_back(para);
				para.clear();
				if (!out.empty() && !out.back().empty())
					out.push_back(L"");
				continue;
			}
			size_t lead = l.find_first_not_of(L' ');
			para += para.empty() ? l : L" " + l.substr(lead == std::wstring::npos ? 0 : lead);
			const bool next = i + 1 < in.size() && !in[i + 1].empty();
			const bool join = next && l.size() >= joinAt && !IsListOrHeading(l) && !IsListOrHeading(in[i + 1]);
			if (!join)
			{
				out.push_back(para);
				para.clear();
			}
		}
		if (!para.empty())
			out.push_back(para);
	}
	while (!out.empty() && out.back().empty())
		out.pop_back();
	return out;
}
} // namespace


// vgui_MOTDWindow.cpp
#define MOTD_TITLE_X       sboui::XRes(16)
#define MOTD_TITLE_Y       sboui::YRes(16)
#define MOTD_WINDOW_X      sboui::XRes(112)
#define MOTD_WINDOW_Y      sboui::YRes(80)
#define MOTD_WINDOW_SIZE_X sboui::XRes(424)
#define MOTD_WINDOW_SIZE_Y sboui::YRes(312)
// vgui_TeamFortressViewport.h
#define BUTTON_SIZE_Y      sboui::YRes(30)
#define CMENU_SIZE_X       sboui::XRes(160)

std::vector<std::wstring> SboReflowMapText(const std::wstring &text)
{
	return ReflowImpl(text);
}

//-----------------------------------------------------------------------------
// The new in-game menus' frame (shared by the MOTD / map info window and the team menu's map briefing)
//-----------------------------------------------------------------------------
void SboFrame_Layout(SboFrameLayout &f, int sw, int sh)
{
	f.k = std::min(sw / 1440.0f, sh / 900.0f);
	auto P = [&f](float v) { return (int)lroundf(v * f.k); };
	f.w = std::min(sw, P(TW_WIN_W));
	f.h = std::min(sh, P(TW_WIN_H));
	f.x = (sw - f.w) / 2;
	f.y = (sh - f.h) / 2;
	f.content[0] = f.x + P(TW_PAD);
	f.content[1] = f.y + P(TW_BAR_H) + P(20);
	f.content[2] = f.w - 2 * P(TW_PAD);
	f.content[3] = f.h - P(TW_BAR_H) - P(20) - P(16) - P(TW_BUTTON_H) - P(16);
	f.close[2] = f.close[3] = P(44);
	f.close[0] = f.x + f.w - P(TW_PAD) - f.close[2];
	f.close[1] = f.y + (P(TW_BAR_H) - f.close[3]) / 2;
	f.ok[2] = P(140);
	f.ok[3] = P(TW_BUTTON_H);
	f.ok[0] = f.x + f.w - P(TW_PAD) - f.ok[2];
	f.ok[1] = f.y + f.h - P(16) - f.ok[3];
	f.textMax = P(TW_TEXT_MAX);
}

void SboFrame_Paint(vgui2::Panel *pPanel, const SboFrameLayout &f, const std::wstring &title, const std::wstring &okLabel)
{
	auto P = [&f](float v) { return (int)lroundf(v * f.k); };
	FillRect(f.x, f.y, f.w, f.h, T_WINDOW);
	AddBarlowFonts();
	const vgui2::HFont titleFont = MakeFontFace("Barlow Condensed SemiBold", std::max(14, P(30 * 1.2f)), 400);
	DrawTextMid(titleFont, T_GREEN, f.x + P(TW_PAD), f.y, P(TW_BAR_H), title, f.close[0] - f.x - 2 * P(TW_PAD));
	FillRect(f.x, f.y + P(TW_BAR_H), f.w, 1, T_LINE);
	int mx, my;
	vgui2::input()->GetCursorPos(mx, my);
	pPanel->ScreenToLocal(mx, my);
	OutlineRect(f.close[0], f.close[1], f.close[2], f.close[3], Has(f.close, mx, my) ? T_GREEN : T_BORDER);
	const vgui2::HFont closeFont = MakeFont(std::max(14, P(20 * 1.2f)), 400);
	const std::wstring x = L"\x00d7";
	DrawTextMid(closeFont, T_GREEN, f.close[0] + (f.close[2] - TextWide(closeFont, x)) / 2, f.close[1], f.close[3], x);
	FillRect(f.ok[0], f.ok[1], f.ok[2], f.ok[3], Has(f.ok, mx, my) ? Color(56, 124, 50, 255) : T_DEPLOY);
	OutlineRect(f.ok[0], f.ok[1], f.ok[2], f.ok[3], T_DEPLOY_LINE);
	const vgui2::HFont okFont = MakeFont(std::max(15, (int)lroundf(std::max(17.0f * f.k, 12.0f) * 1.2f)), 700);
	DrawTextMid(okFont, T_DEPLOY_TEXT, f.ok[0] + (f.ok[2] - TextWide(okFont, okLabel)) / 2, f.ok[1], f.ok[3], okLabel);
	OutlineRect(f.x, f.y, f.w, f.h, T_BORDER);
}

int SboFrame_Hit(const SboFrameLayout &f, int x, int y)
{
	if (Has(f.close, x, y))
		return 1;
	if (Has(f.ok, x, y))
		return 2;
	return 0;
}

vgui2::HFont SboFrame_TextFont(const SboFrameLayout &f)
{
	return MakeFont(std::max(15, (int)lroundf(std::max(15.0f * f.k, 12.0f) * 1.2f)), 400);
}

CSboTextWindow *CSboTextWindow::s_pInstance = nullptr;

CSboTextWindow *CSboTextWindow::Create(vgui2::Panel *pParent)
{
	if (!s_pInstance)
		s_pInstance = new CSboTextWindow(pParent);
	return s_pInstance;
}

CSboTextWindow::CSboTextWindow(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboTextWindow")
{
	m_pBackgroundPanel = new CSboTransparentPanel(this, "Background", 100);
	m_pBackgroundPanel->SetLineBorder(sboui::SBO_GREEN);

	// VGUI1: setFont( "Title Font" ) immediately overridden by setFont( Scheme::sf_primary1 ) = "Primary Button Text"
	m_pTitle = new CSboLabel(this, "Title", sboui::DEFAULT_TEXT_SCHEME, sboui::SBO_GREEN);

	m_pText = new CSboTextPanel(this, "Text", "Briefing Text", sboui::SBO_GREEN, true);

	m_pOkButton = new CSboCommandButton(this, "OkButton", this, "ok");
	wchar_t wbuf[64];
	sboui::Localize("#SBO_TextWindow_OK", wbuf, sizeof(wbuf));
	m_pOkButton->SetCommandText(wbuf);
	m_pScrollText = new CSboScrollText(this);
	m_pScrollText->SetVisible(false);
}

void CSboTextWindow::ShowText(const char *title, const char *text, bool bShadeFullscreen, bool bCloseOnly,
    const char *htmlFile, bool bMOTD)
{
	m_bCloseOnly = bCloseOnly;
	m_bMOTD = bMOTD;
	m_Title = title;
	m_bNewLook = SboNewIngameMenus();
	m_pBackgroundPanel->SetVisible(!m_bNewLook);
	m_pTitle->SetVisible(!m_bNewLook);
	m_pOkButton->SetVisible(!m_bNewLook);
	// the scroll bar in the browser's colors in the new look; the MOTD's text as before, other texts in the UI font
	m_pText->SetScheme(m_bNewLook && BrowserScheme() ? BrowserScheme() : sboui::GetScheme());
	m_pText->SetStyleOverride(vgui2::INVALID_FONT, Color());

	// CMenuPanel( iShadeFullscreen ? 100 : 255 ) around CTransparentPanel( iShadeFullscreen ? 255 : 100 ): either
	// the whole screen or only the window is darkened
	SetBackdropTrans(bShadeFullscreen ? 100 : 255);
	m_pBackgroundPanel->SetTrans(bShadeFullscreen ? 255 : 100);

	m_pTitle->SetPlainText(title);

	size_t len = strlen(text) + 1;
	wchar_t *wtext = (wchar_t *)malloc(len * sizeof(wchar_t));
	if (wtext)
	{
		sboui::TextToWide(text, wtext, (int)(len * sizeof(wchar_t)));
		m_pText->SetWideText(wtext);
		m_wText = wtext;
		free(wtext);
	}

	// SBO 2.0: the web page instead of the text (BHL CClientMOTD::ActivateHtml), if HTML is on and there is one.
	// The client's one page panel comes into this window (sbo_html_panel.h).
	bool bHtml = false;
	if ((bMOTD || (htmlFile && htmlFile[0])) && SboHtml_Enabled())
	{
		CSboHtmlPanel *pHtml = CSboHtmlPanel::Acquire(this);
		if (pHtml)
		{
			if (htmlFile && htmlFile[0])
				bHtml = pHtml->OpenGameFile(htmlFile);
			if (!bHtml && bMOTD)
				bHtml = pHtml->OpenMOTD(text);
		}
	}
	if (!bHtml)
		CSboHtmlPanel::Release(this);
	m_bHtml = bHtml;
	m_pText->SetVisible(!bHtml && !m_bNewLook);
	m_pScrollText->SetVisible(!bHtml && m_bNewLook);
	if (CSboHtmlPanel *pHtml = CSboHtmlPanel::GetIfOwnedBy(this))
		pHtml->SetVisible(bHtml);

	m_pOkButton->SetArmed(false);
	ShowMenu();
	// links, scroll bars and the page's mouse wheel
	if (bHtml)
		CSboHtmlPanel::GetIfOwnedBy(this)->SetMouseInputEnabled(true);
}

void CSboTextWindow::HideText()
{
	HideMenu();
	CSboHtmlPanel::Release(this);
	m_bHtml = false;
}

void CSboTextWindow::OnThink()
{
	BaseClass::OnThink();
	if (m_bNewLook && IsVisible())
		Repaint(); // the buttons' hover

	// the map readme took the page panel while this window is still up: its text instead
	if (m_bHtml && !CSboHtmlPanel::GetIfOwnedBy(this))
	{
		m_bHtml = false;
		m_pText->SetVisible(!m_bNewLook);
		m_pScrollText->SetVisible(m_bNewLook);
	}
}

void CSboTextWindow::LayoutMenu()
{
	if (m_bNewLook)
	{
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		m_flK = std::min(sw / 1440.0f, sh / 900.0f);
		auto P = [this](float v) { return (int)lroundf(v * m_flK); };
		m_iWinW = std::min(sw, P(TW_WIN_W));
		m_iWinH = std::min(sh, P(TW_WIN_H));
		m_iWinX = (sw - m_iWinW) / 2;
		m_iWinY = (sh - m_iWinH) / 2;
		const int cx = m_iWinX + P(TW_PAD), cy = m_iWinY + P(TW_BAR_H) + P(20);
		const int cw = m_iWinW - 2 * P(TW_PAD), ch = m_iWinH - P(TW_BAR_H) - P(20) - P(16) - P(TW_BUTTON_H) - P(16);
		m_pText->SetBounds(cx, cy, cw, ch);
		// the web page (MOTD) gets the whole area, the text a readable column with the scroll bar at its right
		if (CSboHtmlPanel *pHtml = CSboHtmlPanel::GetIfOwnedBy(this))
			pHtml->SetBounds(cx, cy, cw, ch);
		m_pScrollText->SetBounds(cx, cy, cw, ch); // the full width: the scroll bar at the content area's right edge
		std::vector<std::wstring> paras;
		if (m_bMOTD)
		{
			// the MOTD's text as the old window shows it: its lines, its font and color
			size_t start = 0;
			while (start <= m_wText.size())
			{
				size_t nl = m_wText.find(L'\n', start);
				if (nl == std::wstring::npos)
					nl = m_wText.size();
				std::wstring l = m_wText.substr(start, nl - start);
				l.erase(std::remove(l.begin(), l.end(), L'\r'), l.end());
				paras.push_back(l);
				start = nl + 1;
			}
			const vgui2::HFont f = sboui::GetTextSchemeFont("Briefing Text");
			m_pScrollText->SetContent(paras, f != vgui2::INVALID_FONT ? f : MakeFont(P(18), 400), sboui::SBO_GREEN, P(2), std::max(6, P(8)));
		}
		else
		{
			// the mapper's text, its paragraphs joined again (Reflow), in the menus' UI font (at least 12 px)
			const int tall = std::max(15, (int)lroundf(std::max(15.0f * m_flK, 12.0f) * 1.2f));
			m_pScrollText->SetContent(SboReflowMapText(m_wText), MakeFont(tall, 400), T_SOFT, P(4), std::max(6, P(8)));
		}
		m_rClose[0] = m_iWinX + m_iWinW - P(TW_PAD) - P(44);
		m_rClose[1] = m_iWinY + (P(TW_BAR_H) - P(44)) / 2;
		m_rClose[2] = m_rClose[3] = P(44);
		m_rOk[2] = P(140);
		m_rOk[3] = P(TW_BUTTON_H);
		m_rOk[0] = m_iWinX + m_iWinW - P(TW_PAD) - m_rOk[2];
		m_rOk[1] = m_iWinY + m_iWinH - P(16) - m_rOk[3];
		return;
	}

	m_pBackgroundPanel->SetBounds(MOTD_WINDOW_X, MOTD_WINDOW_Y, MOTD_WINDOW_SIZE_X, MOTD_WINDOW_SIZE_Y);

	int iXSize, iYSize, iXPos, iYPos;
	m_pBackgroundPanel->GetPos(iXPos, iYPos);
	m_pBackgroundPanel->GetSize(iXSize, iYSize);

	m_pTitle->SetPos(iXPos + MOTD_TITLE_X, iYPos + MOTD_TITLE_Y);
	m_pTitle->SizeToContents();

	// the ScrollPanel: CTFScrollPanel( iXPos + XRES(16), iYPos + MOTD_TITLE_Y*2 + YRES(16), ..., iYSize - (YRES(48) +
	// BUTTON_SIZE_Y*2) ). Here the text goes down to 8 units above the OK button instead.
	const int textY = MOTD_TITLE_Y * 2 + sboui::YRes(16);
	m_pText->SetBounds(iXPos + sboui::XRes(16), iYPos + textY, iXSize - sboui::XRes(32),
	    iYSize - textY - sboui::YRes(16) - BUTTON_SIZE_Y - sboui::YRes(8));
	if (CSboHtmlPanel *pHtml = CSboHtmlPanel::GetIfOwnedBy(this))
		pHtml->SetBounds(iXPos + sboui::XRes(16), iYPos + textY, iXSize - sboui::XRes(32),
		    iYSize - textY - sboui::YRes(16) - BUTTON_SIZE_Y - sboui::YRes(8));

	m_pOkButton->SetBounds(iXPos + sboui::XRes(16), iYPos + iYSize - sboui::YRes(16) - BUTTON_SIZE_Y, CMENU_SIZE_X,
	    BUTTON_SIZE_Y);
}

void CSboTextWindow::Paint()
{
	BaseClass::Paint();
	if (!m_bNewLook)
		return;
	auto P = [this](float v) { return (int)lroundf(v * m_flK); };
	FillRect(m_iWinX, m_iWinY, m_iWinW, m_iWinH, T_WINDOW);
	// title bar: the title in the loadout menu's title face, the close button
	AddBarlowFonts();
	const vgui2::HFont titleFont = MakeFontFace("Barlow Condensed SemiBold", std::max(14, P(30 * 1.2f)), 400);
	DrawTextMid(titleFont, T_GREEN, m_iWinX + P(TW_PAD), m_iWinY, P(TW_BAR_H), Widen(m_Title.c_str()), m_rClose[0] - m_iWinX - 2 * P(TW_PAD));
	FillRect(m_iWinX, m_iWinY + P(TW_BAR_H), m_iWinW, 1, T_LINE);
	int mx, my;
	vgui2::input()->GetCursorPos(mx, my);
	ScreenToLocal(mx, my);
	OutlineRect(m_rClose[0], m_rClose[1], m_rClose[2], m_rClose[3], Has(m_rClose, mx, my) ? T_GREEN : T_BORDER);
	const vgui2::HFont closeFont = MakeFont(std::max(14, P(20 * 1.2f)), 400);
	const std::wstring x = L"\x00d7";
	DrawTextMid(closeFont, T_GREEN, m_rClose[0] + (m_rClose[2] - TextWide(closeFont, x)) / 2, m_rClose[1], m_rClose[3], x);
	// OK: the loadout menu's DEPLOY button
	FillRect(m_rOk[0], m_rOk[1], m_rOk[2], m_rOk[3], Has(m_rOk, mx, my) ? Color(56, 124, 50, 255) : T_DEPLOY);
	OutlineRect(m_rOk[0], m_rOk[1], m_rOk[2], m_rOk[3], T_DEPLOY_LINE);
	const vgui2::HFont okFont = MakeFont(std::max(15, (int)lroundf(std::max(17.0f * m_flK, 12.0f) * 1.2f)), 700);
	DrawTextMid(okFont, T_DEPLOY_TEXT, m_rOk[0] + (m_rOk[2] - TextWide(okFont, L"OK")) / 2, m_rOk[1], m_rOk[3], L"OK");
	OutlineRect(m_iWinX, m_iWinY, m_iWinW, m_iWinH, T_BORDER);
}

void CSboTextWindow::OnMousePressed(vgui2::MouseCode code)
{
	if (!m_bNewLook || code != vgui2::MOUSE_LEFT)
	{
		BaseClass::OnMousePressed(code);
		return;
	}
	int mx, my;
	vgui2::input()->GetCursorPos(mx, my);
	ScreenToLocal(mx, my);
	// the close button does what OK does (the menus that follow the MOTD still open)
	if (Has(m_rClose, mx, my) || Has(m_rOk, mx, my))
		OnCommand("ok");
}

void CSboTextWindow::OnCommand(const char *command)
{
	if (!stricmp(command, "ok"))
	{
		// SBO 2.0: the map briefing from the spectator Options menu (MENU_MAPBRIEFING) only closes, as Enter / Space /
		// Esc do: no team / weapon selection after it. Read before HideTopMenu: the next queued text window sets
		// its own value in ShowText
		if (m_bCloseOnly)
		{
			sbo::HideTopMenu();
			return;
		}

		// The VGUI1 OK button's two action signals, in order:
		// CMenuHandler_TextWindow( HIDE_TEXTWINDOW ): close this window (the next queued menu opens) ...
		sbo::HideTopMenu();
		// ... CMenuHandler_StringCommand( "ShowVGUI 2", true ) (SBO): the command, then HideTopMenu once more
		sbo::ExecClientCmd("ShowVGUI 2");
		sbo::HideTopMenu();
		return;
	}

	BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
void VGui2_TextWindow_Show(const char *title, const char *text, bool shadeFullscreen, bool closeOnly,
    const char *htmlFile, bool isMOTD)
{
	if (CSboTextWindow::Get())
		CSboTextWindow::Get()->ShowText(title ? title : "", text ? text : "", shadeFullscreen, closeOnly, htmlFile,
		    isMOTD);
}

void VGui2_TextWindow_Hide()
{
	if (CSboTextWindow::Get())
		CSboTextWindow::Get()->HideText();
}

bool VGui2_HtmlFileAvailable(const char *path)
{
	return path && path[0] && SboHtml_Enabled() && g_pFullFileSystem && g_pFullFileSystem->FileExists(path);
}
