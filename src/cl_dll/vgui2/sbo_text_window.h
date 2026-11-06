//========= SBO VGUI2 port: text window (MOTD) =========//
//
// Purpose: VGUI2 version of SBO's CMessageWindowPanel (src/cl_dll/vgui_MOTDWindow.cpp), the window
//          TeamFortressViewport::CreateTextWindow opens for the MOTD (MENU_INTRO), the map briefing
//          (MENU_MAPBRIEFING and SBO's menu 9), the class help and the spectator help. Same layout, colours and
//          fonts; the OK button does what the VGUI1 one did (close, "ShowVGUI 2", close the next menu), except for
//          MENU_MAPBRIEFING (the spectator Options menu's "Map Briefing"), where it only closes the window.
//
// Shown through SBO's VGUI1 menu stack (sbo_menu_proxy.h -> VGui2_TextWindow_Show/Hide); the viewport's KeyInput
// still closes it with Enter/Space/Esc.
//
// Text panel idea from BugfixedHL-Rebased src/game/client/vgui/client_motd.{h,cpp} (GPLv3): a RichText with the
// message converted to wide characters, CRLF -> LF. The look is SBO's, not BHL's Frame.
//
// SBO 2.0, HTML (docs/HTML_MOTD.md): with HTML on (sbo_html_panel.h), a MOTD that is an http(s) URL or HTML, and a
// map briefing with a maps/<map>.htm, are shown as a web page in place of the text (BHL's CClientMOTD::ActivateHtml).
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_TEXT_WINDOW_H
#define SBO_VGUI2_SBO_TEXT_WINDOW_H

#include <string>
#include <vector>
#include <algorithm>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include "sbo_ui_style.h"

#include "sbo_widgets.h"

class CSboHtmlPanel;

//! The new in-game menus' frame (1320 x 800 on the design's 1440 x 900 page, scaled): title bar, close button, the
//! green OK-style button, the content area (shared by the MOTD / map info window and the team menu's map briefing)
struct SboFrameLayout
{
	float k = 1.0f;
	int x = 0, y = 0, w = 0, h = 0;
	int close[4] = {}, ok[4] = {}, content[4] = {};
	int textMax = 0; //!< the text column's widest
};
void SboFrame_Layout(SboFrameLayout &f, int screenWide, int screenTall);
void SboFrame_Paint(vgui2::Panel *pPanel, const SboFrameLayout &f, const std::wstring &title, const std::wstring &okLabel);
//! 1 = the close button, 2 = the OK-style button, 0 = neither
int SboFrame_Hit(const SboFrameLayout &f, int x, int y);
//! The menus' UI font for a map's text (at least 12 px)
vgui2::HFont SboFrame_TextFont(const SboFrameLayout &f);
//! A mapper's text with its hard line breaks joined into paragraphs (lists, headings, short lines, drawings and
//! tables keep theirs); "" = an empty line
std::vector<std::wstring> SboReflowMapText(const std::wstring &text);

//-----------------------------------------------------------------------------
// The new in-game menus' text area: the lines wrapped to its width, the loadout menu's scroll bar (8 px, track #0b100c,
// thumb #3c5a3e, #8fdc7a while hovered or dragged), the wheel
//-----------------------------------------------------------------------------
class CSboScrollText : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboScrollText, vgui2::Panel);

public:
	explicit CSboScrollText(vgui2::Panel *pParent)
	    : BaseClass(pParent, "ScrollText")
	{
		SetPaintBackgroundEnabled(false);
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(false);
	}
	//! paragraphs: one per line (wrapped here); "" = an empty line
	void SetContent(const std::vector<std::wstring> &paras, vgui2::HFont font, Color color, int lineGap, int barW)
	{
		m_Paras = paras;
		m_hFont = font;
		m_Color = color;
		m_iGap = lineGap;
		m_iBarW = barW;
		m_iOffset = 0;
		m_iWrapW = -1;
		Repaint();
	}
	void Paint() override
	{
		const int W = GetWide(), H = GetTall();
		Rewrap();
		const int lineH = sbolook::FontTall(m_hFont) + m_iGap;
		for (size_t i = 0; i < m_Lines.size(); i++)
		{
			const int y = (int)i * lineH - m_iOffset;
			if (y + lineH < 0 || y > H)
				continue;
			sbolook::DrawText(m_hFont, m_Color, 0, y, m_Lines[i]);
		}
		if (Content() > H)
		{
			int mx, my;
			vgui2::input()->GetCursorPos(mx, my);
			ScreenToLocal(mx, my);
			const bool hot = m_bDrag || (mx >= W - m_iBarW && mx < W && my >= 0 && my < H);
			sbolook::FillRect(W - m_iBarW, 0, m_iBarW, H, Color(11, 16, 12, 255));
			int ty, th;
			Thumb(ty, th);
			const int b = std::max(1, m_iBarW / 4);
			sbolook::FillRect(W - m_iBarW + b, ty + b, m_iBarW - 2 * b, th - 2 * b, hot ? Color(143, 220, 122, 255) : Color(60, 90, 62, 255));
		}
	}
	void OnMouseWheeled(int delta) override
	{
		m_iOffset -= delta * (sbolook::FontTall(m_hFont) + m_iGap) * 3;
		Clamp();
		Repaint();
	}
	void OnMousePressed(vgui2::MouseCode code) override
	{
		int mx, my;
		vgui2::input()->GetCursorPos(mx, my);
		ScreenToLocal(mx, my);
		if (Content() <= GetTall() || mx < GetWide() - m_iBarW)
			return;
		int ty, th;
		Thumb(ty, th);
		if (my < ty || my >= ty + th)
		{
			m_iOffset = (my - th / 2) * (Content() - GetTall()) / std::max(1, GetTall() - th);
			Clamp();
		}
		m_bDrag = true;
		m_iDragY = my;
		m_iDragOffset = m_iOffset;
		vgui2::input()->SetMouseCapture(GetVPanel());
	}
	void OnMouseReleased(vgui2::MouseCode code) override
	{
		m_bDrag = false;
		vgui2::input()->SetMouseCapture(0);
	}
	void OnCursorMoved(int x, int y) override
	{
		if (m_bDrag)
		{
			int ty, th;
			Thumb(ty, th);
			m_iOffset = m_iDragOffset + (y - m_iDragY) * (Content() - GetTall()) / std::max(1, GetTall() - th);
			Clamp();
		}
		Repaint();
	}

private:
	int Content() { Rewrap(); return (int)m_Lines.size() * (sbolook::FontTall(m_hFont) + m_iGap); }
	void Clamp() { m_iOffset = std::max(0, std::min(m_iOffset, std::max(0, Content() - GetTall()))); }
	void Thumb(int &ty, int &th)
	{
		const int H = GetTall(), C = std::max(1, Content());
		th = std::max(m_iBarW * 3, H * H / C);
		ty = C > H ? (H - th) * m_iOffset / (C - H) : 0;
	}
	void Rewrap()
	{
		const int w = GetWide() - m_iBarW - m_iBarW;
		if (w == m_iWrapW)
			return;
		m_iWrapW = w;
		m_Lines.clear();
		for (const std::wstring &p : m_Paras)
		{
			if (p.empty())
			{
				m_Lines.push_back(L"");
				continue;
			}
			// word wrap (a word wider than the line is cut)
			size_t pos = 0;
			while (pos < p.size())
			{
				size_t lastFit = pos, end = pos;
				while (end <= p.size())
				{
					size_t next = p.find(L' ', end);
					if (next == std::wstring::npos)
						next = p.size();
					if (sbolook::TextWide(m_hFont, p.substr(pos, next - pos)) > w && lastFit > pos)
						break;
					lastFit = next;
					if (next >= p.size())
						break;
					end = next + 1;
				}
				m_Lines.push_back(p.substr(pos, lastFit - pos));
				pos = lastFit;
				while (pos < p.size() && p[pos] == L' ')
					pos++;
			}
		}
	}

	std::vector<std::wstring> m_Paras, m_Lines;
	vgui2::HFont m_hFont = 0;
	Color m_Color;
	int m_iGap = 2, m_iBarW = 8, m_iOffset = 0, m_iWrapW = -1;
	bool m_bDrag = false;
	int m_iDragY = 0, m_iDragOffset = 0;
};


class CSboTextWindow : public CSboMenuPanel
{
	DECLARE_CLASS_SIMPLE(CSboTextWindow, CSboMenuPanel);

public:
	//! Creates the panel once (VGui2_Menus_Init).
	static CSboTextWindow *Create(vgui2::Panel *pParent);
	static CSboTextWindow *Get() { return s_pInstance; }

	//! CMessageWindowPanel( szMOTD, szTitle, iShadeFullscreen, ... )
	//! bCloseOnly: OK only closes the window, no "ShowVGUI 2"
	//! htmlFile: a game file shown as a web page instead of text when HTML is on (the map briefing's .htm), or null
	//! bMOTD: text is the MOTD (a URL or HTML in it is shown as a web page when HTML is on)
	void ShowText(const char *title, const char *text, bool bShadeFullscreen, bool bCloseOnly, const char *htmlFile,
	    bool bMOTD);
	//! HideMenu, and the web page (if any) emptied.
	void HideText();

	virtual void OnCommand(const char *command) override;
	virtual void OnThink() override;
	virtual void Paint() override;
	virtual void OnMousePressed(vgui2::MouseCode code) override;

protected:
	virtual void LayoutMenu() override;

private:
	CSboTextWindow(vgui2::Panel *pParent);

	static CSboTextWindow *s_pInstance;

	CSboTransparentPanel *m_pBackgroundPanel = nullptr;
	CSboLabel *m_pTitle = nullptr;
	CSboTextPanel *m_pText = nullptr;
	CSboCommandButton *m_pOkButton = nullptr;

	bool m_bCloseOnly = false; // of the text shown now (ShowText)
	bool m_bHtml = false;      // the page is shown (the client's one CSboHtmlPanel is in this window)
	bool m_bMOTD = false;      // of the text shown now
	// "New in-game menus" (cl_new_ingame_menus 1, read in ShowText): the loadout menu's frame (title bar, close
	// button, OK button, the browser's scroll bar); the MOTD's content as before, other texts in the menus' UI font
	bool m_bNewLook = false;
	CSboScrollText *m_pScrollText = nullptr; //!< the new look's text (all texts but a web page)
	std::wstring m_wText;
	float m_flK = 1.0f;
	int m_iWinX = 0, m_iWinY = 0, m_iWinW = 0, m_iWinH = 0;
	int m_rClose[4] = {}, m_rOk[4] = {};
	std::string m_Title;
};

#endif // SBO_VGUI2_SBO_TEXT_WINDOW_H
