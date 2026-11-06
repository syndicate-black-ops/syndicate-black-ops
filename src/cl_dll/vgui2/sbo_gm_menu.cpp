//========= SBO VGUI2: Game Master mode guide =========//
//
// Purpose: see sbo_gm_menu.h.
//=================================================================//

#include <string.h>
#include <wchar.h>
#include <algorithm>

#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurface.h>

#include "sbo_bridge.h"
#include "sbo_gm_menu.h"
#include "sbo_gm_ui.h"
#include "sbo_menus.h"
#include "vgui2_api.h"

#include "../../game_shared/gm_catalog.h"

using namespace gmui;

namespace
{
std::wstring Loc(const char *token)
{
	wchar_t buf[2048];
	sboui::Localize(token, buf, sizeof(buf));
	return buf;
}

// the text wrapped to the width: one entry per line ("" a paragraph gap)
std::vector<std::wstring> Wrap(vgui2::HFont font, const std::wstring &text, int width)
{
	std::vector<std::wstring> lines;
	size_t start = 0;
	while (start <= text.size())
	{
		size_t end = text.find(L'\n', start);
		if (end == std::wstring::npos)
			end = text.size();
		std::wstring para = text.substr(start, end - start);
		std::wstring line;
		size_t p = 0;
		if (para.empty())
			lines.push_back(L"");
		while (p < para.size())
		{
			size_t sp = para.find(L' ', p);
			if (sp == std::wstring::npos)
				sp = para.size();
			std::wstring word = para.substr(p, sp - p);
			std::wstring tryLine = line.empty() ? word : line + L" " + word;
			if (!line.empty() && TextWide(font, tryLine.c_str()) > width)
			{
				lines.push_back(line);
				line = word;
			}
			else
				line = tryLine;
			p = sp + 1;
		}
		if (!line.empty())
			lines.push_back(line);
		start = end + 1;
	}
	return lines;
}
} // namespace

CSboGmMenu *CSboGmMenu::s_pInstance = nullptr;

CSboGmMenu *CSboGmMenu::Create(vgui2::Panel *pParent)
{
	if (!s_pInstance)
		s_pInstance = new CSboGmMenu(pParent);
	return s_pInstance;
}

CSboGmMenu::CSboGmMenu(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboGmMenu")
{
	SetBackdropTrans(0); // no backdrop: the intro's black screen is the fade's, the help shows over the game
}

std::wstring CSboGmMenu::GuideText() const
{
	// "{gmon}" -> the player's primary key for gmon ("K"); a line starting "[pts]" only with the budget plugin
	const std::wstring text = Loc("#SBO_GM_Guide_Text");
	std::wstring out;
	size_t start = 0;
	while (start <= text.size())
	{
		size_t end = text.find(L'\n', start);
		if (end == std::wstring::npos)
			end = text.size();
		std::wstring line = text.substr(start, end - start);
		start = end + 1;
		static const std::wstring PTS = L"[pts]";
		if (line.compare(0, PTS.size(), PTS) == 0)
		{
			if (!HasEconomy())
				continue;
			line.erase(0, PTS.size());
		}
		for (size_t open = line.find(L'{'); open != std::wstring::npos; open = line.find(L'{', open))
		{
			const size_t close = line.find(L'}', open);
			if (close == std::wstring::npos)
				break;
			std::string cmd;
			for (size_t i = open + 1; i < close; i++)
				cmd += (char)line[i];
			const std::wstring key = KeyFor(cmd.c_str(), nullptr);
			line.replace(open, close - open + 1, key);
			open += key.size();
		}
		if (!out.empty())
			out += L"\n";
		out += line;
	}
	return out;
}

void CSboGmMenu::Open()
{
	m_iScroll = 0;
	m_bConfirmLeave = false;
	ShowMenu();
	SetKeyBoardInputEnabled(true);
	RequestFocus();
}

void CSboGmMenu::LayoutMenu()
{
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	m_rWin = { (sw - S(1000)) / 2, (sh - S(620)) / 2, S(1000), S(620) };
	const int px = m_rWin.x + S(24), py = m_rWin.y + S(20), innerW = m_rWin.w - S(48);
	m_rClose = { px + innerW - S(32), py, S(32), S(32) };
	const int textY = py + S(32) + S(20);
	const int bottom = m_rWin.y + m_rWin.h - S(20);
	m_rStart = { px + innerW - S(200), bottom - S(44), S(200), S(44) };
	m_rLeave = { px, bottom - S(44), S(200), S(44) };
	m_rYes = { px + S(200) + S(8), bottom - S(44), S(120), S(44) };
	m_rNo = { m_rYes.x + S(128), bottom - S(44), S(120), S(44) };
	m_rText = { px, textY, innerW, bottom - S(44) - S(20) - textY };
}

void CSboGmMenu::Button(const Rect &r, const wchar_t *label, bool primary, float px)
{
	if (primary)
		Fill(r.x, r.y, r.w, r.h, BUTTON_BG);
	Outline(r.x, r.y, r.w, r.h, primary ? SEL_BORDER : BORDER);
	vgui2::HFont f = Font(px);
	Text(f, r.x + (r.w - TextWide(f, label)) / 2, r.y + (r.h - FontTall(f)) / 2, primary ? BRIGHT : GREEN, label);
}

void CSboGmMenu::PaintBackground()
{
	LayoutMenu();
	Fill(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, BG);
	Outline(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, BORDER);
}

void CSboGmMenu::Paint()
{
	vgui2::HFont f26 = Font(26), f16 = Font(16), f15 = Font(15), f13 = Font(13);
	const int px = m_rWin.x + S(24), py = m_rWin.y + S(20);
	const std::wstring title = GuideTitle().empty() ? Loc("#SBO_GM_Title") : GuideTitle();
	Text(f26, px, py + (S(32) - FontTall(f26)) / 2, GREEN, title.c_str());
	CloseButton(m_rClose.x, m_rClose.y, m_rClose.w, m_rClose.h);

	// the text: 16 px bright, one line per point, a half line between them
	const std::vector<std::wstring> lines = Wrap(f16, GuideText(), m_rText.w);
	const int lineTall = FontTall(f16) + S(4);
	const int visible = std::max(1, m_rText.h / lineTall);
	m_iScroll = std::min(m_iScroll, std::max(0, (int)lines.size() - visible));
	int y = m_rText.y;
	for (int i = m_iScroll; i < (int)lines.size() && y + lineTall <= m_rText.y + m_rText.h; i++)
	{
		if (lines[i].empty())
		{
			y += lineTall / 2;
			continue;
		}
		Text(f16, m_rText.x, y, BRIGHT, lines[i].c_str());
		y += lineTall;
	}

	if (GetState() == GM_STATE_ACTIVE || GetState() == GM_STATE_INTRO)
	{
		Button(m_rLeave, Loc("#SBO_GM_Guide_Leave").c_str(), false, 15);
		if (m_bConfirmLeave)
		{
			Text(f13, m_rYes.x, m_rYes.y - FontTall(f13) - S(6), DIM, Loc("#SBO_GM_Guide_LeaveQuestion").c_str());
			Button(m_rYes, Loc("#SBO_GM_Guide_Yes").c_str(), true, 15);
			Button(m_rNo, Loc("#SBO_GM_Guide_No").c_str(), false, 15);
		}
	}
	Button(m_rStart, Loc(GetState() == GM_STATE_INTRO ? "#SBO_GM_Guide_Start" : "#SBO_GM_Guide_Close").c_str(), true, 15);
}

void CSboGmMenu::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	LayoutMenu();
	if (m_rStart.Hit(x, y) || m_rClose.Hit(x, y))
	{
		Finish();
		return;
	}
	if (m_rLeave.Hit(x, y))
	{
		m_bConfirmLeave = true;
		return;
	}
	if (m_bConfirmLeave && m_rYes.Hit(x, y))
	{
		m_bConfirmLeave = false;
		sbo::HideTopMenu(); // sends "gm" (VGui2_GmMenu_Hide), the Game Master starts, then leaves
		sbo::ExecClientCmd("cmd gm_leave");
		return;
	}
	if (m_bConfirmLeave && m_rNo.Hit(x, y))
		m_bConfirmLeave = false;
}

void CSboGmMenu::OnMouseWheeled(int delta)
{
	m_iScroll = std::max(0, m_iScroll - delta);
}

void CSboGmMenu::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ENTER || code == vgui2::KEY_PAD_ENTER || code == vgui2::KEY_ESCAPE)
	{
		Finish();
		return;
	}
	if (code == vgui2::KEY_DOWN || code == vgui2::KEY_UP)
	{
		m_iScroll = std::max(0, m_iScroll + (code == vgui2::KEY_DOWN ? 1 : -1));
		return;
	}
	BaseClass::OnKeyCodeTyped(code);
}

void CSboGmMenu::Finish()
{
	// "gm" is sent when the window closes (VGui2_GmMenu_Hide), also on Esc
	sbo::HideTopMenu();
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
void VGui2_GmMenu_Show()
{
	if (CSboGmMenu::Get())
		CSboGmMenu::Get()->Open();
}

void VGui2_GmMenu_Hide()
{
	if (!CSboGmMenu::Get())
		return;

	// SBO 2.0 GM mode: the intro (or help) is closed: the server lets the Game Master start (gmmode.cpp, "gm"; the
	// black screen behind goes, sbo_gm_spawn_menu.cpp)
	const bool bWasVisible = CSboGmMenu::Get()->IsVisible();
	CSboGmMenu::Get()->HideMenu();
	if (bWasVisible)
		sbo::ExecClientCmd("gm");
}
