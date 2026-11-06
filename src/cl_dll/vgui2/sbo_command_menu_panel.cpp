//========= SBO VGUI2: command menus, the panels =========//
//
// Purpose: see sbo_command_menu.h. This file: the VGUI2 panels of the open command menu and its parent menus.
//
// One panel per menu (a root menu or a submenu), created the first time the menu opens, exactly as big as the
// menu's rows and placed where the menu was laid out (sbo::GetCommandMenuBounds); one button per menu button.
// vgui2_dumptree lists them ("CommandMenu <file>[>keys]") with their buttons ("Button <key> <token>"), bounds,
// visibility and mouse flag.
//
// Stacking, as it was before these panels existed:
//   * the spectator menus are children of CSboSpectatorPanel, after its bars, inset frame and target info box (so
//     they draw over those, and under every popup in front of the spectator panel: chat input, scoreboard, menus).
//     They take the mouse with the spectator panel (duck menu).
//   * the standard menu ("+commandmenu") was drawn by VGUI1, underneath all of VGUI2. Its panels are popups (the
//     client viewport itself takes no mouse input, so only a popup gets clicks) kept at the back of the client
//     viewport's children and of the popup list, the root menu at the very back and a submenu just in front of its
//     parent; they take mouse input over their own rows while SBO's cursor is on (g_iVisibleMouse: a command menu
//     with hud_capturemouse, as VGUI1). With hud_capturemouse 0 the menu has no cursor and is used with its keys.
//
// The buttons show the state the menus hold (visible, armed, bounds, toggle cvar) and hand the mouse to them
// (sbo::CommandButtonCursorEntered / Exited / Pressed); keys never come here (TeamFortressViewport::KeyInput ->
// sbo::CommandMenu_KeyInput).
//
// Look:
//   * the spectator menus (CSboSpectButton, sbo_widgets.h): unchanged from the panels that showed the VGUI1 menus
//   * the standard menu ("+commandmenu"), which VGUI1 drew until SBO 2.0: CommandButton / ToggleCommandButton /
//     CCommandMenu::paintBackground reproduced here (CSboStandardMenuButton)
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <tier1/utlvector.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>

#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_command_menu.h"
#include "sbo_menus.h"
#include "sbo_spectator_panel.h"
#include "sbo_widgets.h"

namespace
{
// CCommandMenu::paintBackground
const Color SPECT_MENU_BG = sboui::InvAlphaColor(0, 0, 0, 64);
const Color TRANSPARENT_COLOR(0, 0, 0, 0);

// vgui_TeamFortressViewport.h
constexpr int MAX_BUTTON_SIZE = 32;

Color SchemeColor(const unsigned char c[4])
{
	return Color(c[0], c[1], c[2], c[3]);
}

// titles.txt texts end with the line break before the closing brace, which VGUI1 labels did not show
void TrimTrailingSpace(char *s)
{
	size_t n = strlen(s);
	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t'))
		s[--n] = 0;
}

void ButtonName(const sbo::CommandButtonInfo &info, char *out, size_t n)
{
	char key[8] = "-";
	if (info.boundKey && (unsigned char)info.boundKey != 255)
	{
		key[0] = info.boundKey;
		key[1] = 0;
	}
	snprintf(out, n, "Button %s %s", key, info.token);
}
} // namespace

//-----------------------------------------------------------------------------
// What the menu panel needs from its buttons, whatever their look
//-----------------------------------------------------------------------------
class ISboCommandMenuButton
{
public:
	virtual vgui2::Panel *GetButtonPanel() = 0;
	//! The toggle's check mark follows its cvar.
	virtual void UpdateChecked() = 0;
};

//-----------------------------------------------------------------------------
// A button with the spectator menus' look (SpectButton, SpectToggleButton, TeamOnlyCommandButton).
//-----------------------------------------------------------------------------
class CSboSpectMenuButton : public CSboSpectButton, public ISboCommandMenuButton
{
	DECLARE_CLASS_SIMPLE(CSboSpectMenuButton, CSboSpectButton);

public:
	CSboSpectMenuButton(vgui2::Panel *pParent, const char *pName, const sbo::CommandButtonInfo &info, bool bSpectMenu)
	    : BaseClass(pParent, pName)
	    , m_hButton(info.handle)
	    , m_hToggleCvar(info.style == sbo::COMMAND_BUTTON_SPECT_TOGGLE ? info.toggleCvar : nullptr)
	{
		wchar_t wbuf[64];
		if (bSpectMenu)
		{
			// the menu file's #token from sbo_ui_<language>.txt; else the menu's text (titles.txt or plain)
			if (info.token[0] == '#' && g_pVGuiLocalize && g_pVGuiLocalize->Find(info.token))
				sboui::Localize(info.token, wbuf, sizeof(wbuf));
			else
				sboui::TextToWide(info.text, wbuf, sizeof(wbuf));
		}
		else
		{
			// in the standard menu (a TEAMn button): the VGUI1 text, "  <key>  text" cut at 31 characters
			char text[MAX_BUTTON_SIZE];
			strncpy(text, info.text, sizeof(text) - 1);
			text[sizeof(text) - 1] = 0;
			if ((unsigned char)info.boundKey != 255)
				text[MAX_BUTTON_SIZE - 1 - 5] = 0;
			TrimTrailingSpace(text);
			sboui::TextToWide(text, wbuf, sizeof(wbuf));
		}
		SetCommandText(wbuf, info.boundKey);

		SetToggle(info.style == sbo::COMMAND_BUTTON_SPECT_TOGGLE);
		SetSubMenuArrow(info.subMenu != nullptr);
		UpdateChecked();
	}

	virtual vgui2::Panel *GetButtonPanel() override { return this; }

	virtual void UpdateChecked() override
	{
		if (m_hToggleCvar)
			SetChecked(sbo::CvarGetFloat(m_hToggleCvar) != 0.0f);
	}

	virtual bool IsSpectArmed() override { return sbo::IsCommandButtonArmed(m_hButton); }

	virtual void OnCursorEntered() override
	{
		BaseClass::OnCursorEntered();
		sbo::CommandButtonCursorEntered(m_hButton);
	}

	virtual void OnCursorExited() override
	{
		BaseClass::OnCursorExited();
		sbo::CommandButtonCursorExited(m_hButton);
	}

	virtual void FireActionSignal() override
	{
		// may close this menu (a command button: CommandMenu_Hide -> VGui2_CommandMenu_Changed)
		sbo::CommandButtonPressed(m_hButton);
	}

private:
	sbo::CommandButtonHandle m_hButton;
	sbo::CvarHandle m_hToggleCvar;
};

//-----------------------------------------------------------------------------
// A button of the standard command menu: VGUI1 CommandButton / ToggleCommandButton (vgui_CustomObjects.cpp) as
// CreateCommandMenu set them up.
//   * font "CommandMenu Text" (Scheme::sf_primary3), text west aligned: "  <key>  text", "     text" without a
//     key, cut at 31 characters (CommandButton::RecalculateText)
//   * text in the text scheme's FgColor (Scheme::sc_primary1), armed: FgColorArmed (sc_secondary2) on a filled
//     BgColorArmed (sc_primary2) background; SBO's green 1-pixel border (41 107 34) always
//   * a submenu button: the "arrow" image (CreateSubMenu) at CMENU_SIZE_X - image wide, vertically centred in
//     BUTTON_SIZE_Y
//   * a TOGGLE button: the "checked" image while its cvar is non-zero, else "unchecked", at the width of
//     "     text" in the default font (the button's text and font when ToggleCommandButton's constructor ran),
//     vertically centred; none if the cvar doesn't exist
//-----------------------------------------------------------------------------
class CSboStandardMenuButton : public vgui2::Button, public ISboCommandMenuButton
{
	DECLARE_CLASS_SIMPLE(CSboStandardMenuButton, vgui2::Button);

public:
	CSboStandardMenuButton(vgui2::Panel *pParent, const char *pName, const sbo::CommandButtonInfo &info)
	    : BaseClass(pParent, pName, L"")
	    , m_hButton(info.handle)
	    , m_bToggle(info.style == sbo::COMMAND_BUTTON_TOGGLE)
	    , m_hToggleCvar(info.style == sbo::COMMAND_BUTTON_TOGGLE ? info.toggleCvar : nullptr)
	    , m_bArrow(info.subMenu != nullptr)
	{
		DrawFocusBox(false);
		SetKeyBoardInputEnabled(false);
		SetPaintBackgroundEnabled(true);

		strncpy(m_szMainText, info.text, sizeof(m_szMainText) - 1);
		m_szMainText[sizeof(m_szMainText) - 1] = 0;

		// CommandButton::RecalculateText
		char szBuf[128];
		if (info.boundKey != 0)
		{
			if ((unsigned char)info.boundKey == 255)
				strcpy(szBuf, m_szMainText);
			else
				snprintf(szBuf, sizeof(szBuf), "  %c  %s", info.boundKey, m_szMainText);
		}
		else
			snprintf(szBuf, sizeof(szBuf), "     %s", m_szMainText);
		szBuf[MAX_BUTTON_SIZE - 1] = 0;
		TrimTrailingSpace(szBuf);

		wchar_t wbuf[128];
		sboui::TextToWide(szBuf, wbuf, sizeof(wbuf));
		SetText(wbuf);
	}

	virtual vgui2::Panel *GetButtonPanel() override { return this; }

	virtual void UpdateChecked() override
	{
		if (m_hToggleCvar)
			m_bChecked = sbo::CvarGetFloat(m_hToggleCvar) != 0.0f;
	}

	bool IsMenuArmed() { return sbo::IsCommandButtonArmed(m_hButton); }

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);

		// the viewport's Scheme colours: sc_primary1 = fg, sc_secondary2 = armed fg, sc_primary2 = armed bg of
		// "Primary Button Text"
		sbo::TextSchemeInfo info;
		if (sbo::GetTextSchemeInfo(sboui::DEFAULT_TEXT_SCHEME, info))
		{
			m_Fg = SchemeColor(info.fg);
			m_ArmedFg = SchemeColor(info.armedFg);
			m_ArmedBg = SchemeColor(info.armedBg);
		}

		// the armed look follows the menu's state (IsMenuArmed), not VGUI2's hover / pressed states
		SetDefaultColor(m_Fg, TRANSPARENT_COLOR);
		SetArmedColor(m_Fg, TRANSPARENT_COLOR);
		SetDepressedColor(m_Fg, TRANSPARENT_COLOR);
		SetSelectedColor(m_Fg, TRANSPARENT_COLOR);

		// CCommandMenu::AddButton / CreateCommandMenu: setFont( Scheme::sf_primary3 ) = "CommandMenu Text"
		vgui2::HFont font = sboui::GetTextSchemeFont("CommandMenu Text");
		if (font != vgui2::INVALID_FONT)
			SetFont(font);

		SetTextInset(0, 0);
		SetContentAlignment(vgui2::Label::a_west); // CommandButton::Init

		// the images (resolution dependent: 320_ / 640_)
		if (m_bToggle)
		{
			m_Checked.Load("checked");
			m_Unchecked.Load("unchecked");
		}
		if (m_bArrow)
			m_Arrow.Load("arrow");
	}

	virtual void PerformLayout() override
	{
		BaseClass::PerformLayout();

		int wide, tall;
		GetSize(wide, tall);

		if (m_bToggle)
		{
			int textWide = 0, textTall = 0;
			vgui2::HFont font = sboui::GetTextSchemeFont(sboui::DEFAULT_TEXT_SCHEME);
			if (font != vgui2::INVALID_FONT)
			{
				char szBuf[64];
				snprintf(szBuf, sizeof(szBuf), "     %s", m_szMainText);
				szBuf[MAX_BUTTON_SIZE - 1] = 0;
				wchar_t wbuf[64];
				sboui::TextToWide(szBuf, wbuf, sizeof(wbuf));
				vgui2::surface()->GetTextSize(font, wbuf, textWide, textTall);
			}
			m_iImageX = textWide;
			m_iCheckedY = (tall - m_Checked.GetTall()) / 2;
			m_iUncheckedY = (tall - m_Unchecked.GetTall()) / 2;
		}

		if (m_bArrow)
		{
			m_iArrowX = sboui::XRes(160) - m_Arrow.GetWide();
			m_iArrowY = (sboui::YRes(30) - m_Arrow.GetTall()) / 2;
		}
	}

	virtual void PaintBackground() override
	{
		// CommandButton::paintBackground: the armed highlight
		if (!IsMenuArmed())
			return;

		int wide, tall;
		GetSize(wide, tall);
		vgui2::surface()->DrawSetColor(m_ArmedBg);
		vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
	}

	virtual void Paint() override
	{
		SetFgColor(IsMenuArmed() ? m_ArmedFg : m_Fg);
		BaseClass::Paint();

		if (m_bToggle && m_hToggleCvar)
		{
			if (m_bChecked)
				m_Checked.Draw(m_iImageX, m_iCheckedY);
			else
				m_Unchecked.Draw(m_iImageX, m_iUncheckedY);
		}
		if (m_bArrow)
			m_Arrow.Draw(m_iArrowX, m_iArrowY);

		// SBO's CommandButton::paint: setBorder( new LineBorder( Color( 41, 107, 34, 0 ) ) )
		int wide, tall;
		GetSize(wide, tall);
		vgui2::surface()->DrawSetColor(sboui::SBO_GREEN);
		vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);
	}

	virtual vgui2::IBorder *GetBorder(bool depressed, bool armed, bool selected, bool keyfocus) override
	{
		return nullptr; // the line is drawn in Paint
	}

	virtual void OnCursorEntered() override
	{
		BaseClass::OnCursorEntered();
		sbo::CommandButtonCursorEntered(m_hButton);
	}

	virtual void OnCursorExited() override
	{
		BaseClass::OnCursorExited();
		sbo::CommandButtonCursorExited(m_hButton);
	}

	virtual void FireActionSignal() override
	{
		sbo::CommandButtonPressed(m_hButton);
	}

private:
	sbo::CommandButtonHandle m_hButton;
	char m_szMainText[MAX_BUTTON_SIZE];
	bool m_bToggle;
	sbo::CvarHandle m_hToggleCvar;
	bool m_bChecked = false;
	bool m_bArrow;
	Color m_Fg, m_ArmedFg, m_ArmedBg;
	CSboTgaTexture m_Checked, m_Unchecked, m_Arrow;
	int m_iImageX = 0, m_iCheckedY = 0, m_iUncheckedY = 0;
	int m_iArrowX = 0, m_iArrowY = 0;
};

//-----------------------------------------------------------------------------
// One menu (a root menu or a submenu): a popup as big as the menu's rows.
//-----------------------------------------------------------------------------
class CSboCommandMenuPanel : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboCommandMenuPanel, vgui2::Panel);

public:
	//! bPopup: a standard menu (popup under the client viewport); else a spectator menu (child of the spectator panel)
	CSboCommandMenuPanel(vgui2::Panel *pParent, const char *pName, sbo::CommandMenuHandle hMenu, bool bPopup)
	    : BaseClass(pParent, pName)
	    , m_hMenu(hMenu)
	    , m_bPopup(bPopup)
	{
		if (m_bPopup)
		{
			MakePopup(false, false);
			SetProportional(false); // screen pixels, like VGUI1
			SetMouseInputEnabled(false);
		}
		if (sboui::GetScheme())
			SetScheme(sboui::GetScheme());
		SetPaintBackgroundEnabled(true);
		SetKeyBoardInputEnabled(false);
		SetVisible(false);

		m_bSpectStyle = sbo::IsSpectatorStyleCommandMenu(hMenu);

		// the buttons never change after the menu file was read: created once
		const int count = sbo::GetCommandMenuButtonCount(hMenu);
		for (int i = 0; i < count; i++)
		{
			sbo::CommandButtonInfo info;
			if (!sbo::GetCommandButtonInfo(hMenu, i, info))
				continue;

			char name[96];
			ButtonName(info, name, sizeof(name));

			// the spectator menus: every button has the spectator look (also a CUSTOM, MAP or TEAMn one, as before)
			ISboCommandMenuButton *pButton;
			if (m_bSpectStyle || info.style == sbo::COMMAND_BUTTON_SPECT || info.style == sbo::COMMAND_BUTTON_SPECT_TOGGLE)
				pButton = new CSboSpectMenuButton(this, name, info, m_bSpectStyle);
			else
				pButton = new CSboStandardMenuButton(this, name, info);

			if (m_bPopup)
				pButton->GetButtonPanel()->SetMouseInputEnabled(false);
			m_Buttons.AddToTail(pButton);
			m_ButtonIndex.AddToTail(i);
		}
	}

	sbo::CommandMenuHandle GetHandle() const { return m_hMenu; }
	bool IsPopupMenu() const { return m_bPopup; }

	//! A standard menu stays behind everything else of the client viewport (VGUI1 drew it under all of VGUI2).
	void MoveBehindEverything()
	{
		if (!m_bPopup)
			return;

		vgui2::ipanel()->MoveToBack(GetVPanel());
		vgui2::surface()->MovePopupToBack(GetVPanel());
	}

	//! Position, size and visible buttons as the menu has them now.
	void LayoutFromMenu()
	{
		int x, y, wide, tall;
		sbo::GetCommandMenuBounds(m_hMenu, x, y, wide, tall);
		SetBounds(x, y, wide, tall);

		for (int i = 0; i < m_Buttons.Count(); i++)
		{
			sbo::CommandButtonInfo info;
			if (!sbo::GetCommandButtonInfo(m_hMenu, m_ButtonIndex[i], info))
				continue;

			vgui2::Panel *pPanel = m_Buttons[i]->GetButtonPanel();
			pPanel->SetBounds(info.x, info.y, info.wide, info.tall);
			pPanel->SetVisible(info.visible);
			m_Buttons[i]->UpdateChecked();
		}
	}

	//! Standard menu: mouse input on the menu and every button while SBO's cursor is on.
	void UpdateMouseInput()
	{
		if (!m_bPopup)
			return;

		const bool bWant = sbo::IsMenuCursorVisible();
		if (bWant == m_bMouseInput && bWant == IsMouseInputEnabled())
			return;

		m_bMouseInput = bWant;
		SetMouseInputEnabled(bWant);
		for (int i = 0; i < m_Buttons.Count(); i++)
			m_Buttons[i]->GetButtonPanel()->SetMouseInputEnabled(bWant);
		vgui2::surface()->CalculateMouseVisible();
	}

	void ShowMenuPanel()
	{
		LayoutFromMenu();

		if (m_bPopup)
		{
			// fonts, colours and images (again) the first time and after a change of resolution; no file loads
			// otherwise. (A spectator menu gets them with the spectator panel, when the bars appear.)
			int screenWide, screenTall;
			sbo::GetScreenSize(screenWide, screenTall);
			if (screenWide != m_iSchemeScreenWide || screenTall != m_iSchemeScreenTall)
			{
				m_iSchemeScreenWide = screenWide;
				m_iSchemeScreenTall = screenTall;
				InvalidateLayout(false, true);
			}
		}

		SetVisible(true);

		if (m_bPopup)
		{
			// the engine's vgui2 clears mouse input on a popup's children along with the popup's
			m_bMouseInput = !sbo::IsMenuCursorVisible();
			UpdateMouseInput();
		}
	}

	void HideMenuPanel()
	{
		if (!IsVisible())
			return;

		SetVisible(false);
		if (m_bPopup)
		{
			SetMouseInputEnabled(false);
			m_bMouseInput = false;
			vgui2::surface()->CalculateMouseVisible();
		}
	}

	virtual void OnThink() override;

	virtual void PaintBackground() override
	{
		// CCommandMenu::paintBackground: black (VGUI1 alpha 64) for the spectator menus, else the scheme's
		// background (sc_primary3 = "Primary Button Text" BgColor)
		Color color = SPECT_MENU_BG;
		if (!m_bSpectStyle)
		{
			sbo::TextSchemeInfo info;
			if (sbo::GetTextSchemeInfo(sboui::DEFAULT_TEXT_SCHEME, info))
				color = SchemeColor(info.bg);
		}

		int wide, tall;
		GetSize(wide, tall);
		vgui2::surface()->DrawSetColor(color);
		vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
	}

private:
	sbo::CommandMenuHandle m_hMenu;
	bool m_bPopup;
	bool m_bSpectStyle = true;
	bool m_bMouseInput = false;
	int m_iSchemeScreenWide = 0, m_iSchemeScreenTall = 0;
	CUtlVector<ISboCommandMenuButton *> m_Buttons;
	CUtlVector<int> m_ButtonIndex; //!< the menu's button index of each
};

namespace
{
CUtlVector<CSboCommandMenuPanel *> s_MenuPanels;

CSboCommandMenuPanel *FindMenuPanel(sbo::CommandMenuHandle hMenu)
{
	for (int i = 0; i < s_MenuPanels.Count(); i++)
	{
		if (s_MenuPanels[i]->GetHandle() == hMenu)
			return s_MenuPanels[i];
	}
	return nullptr;
}

CSboCommandMenuPanel *FindOrCreateMenuPanel(sbo::CommandMenuHandle hMenu)
{
	CSboCommandMenuPanel *pPanel = FindMenuPanel(hMenu);
	if (pPanel)
		return pPanel;

	// first time this menu opens. A spectator menu: a child of the spectator panel, after the children it has
	// (bars, inset frame, target info box, menus opened earlier). The standard menu: a popup of the client viewport.
	char name[128];
	snprintf(name, sizeof(name), "CommandMenu %s", sbo::GetCommandMenuName(hMenu));
	const bool bSpect = sbo::IsSpectatorStyleCommandMenu(hMenu) && CSboSpectatorPanel::Get();
	pPanel = new CSboCommandMenuPanel(bSpect ? (vgui2::Panel *)CSboSpectatorPanel::Get() : (vgui2::Panel *)g_pViewport,
	    name, hMenu, !bSpect);
	s_MenuPanels.AddToTail(pPanel);
	return pPanel;
}

// The open standard menus behind everything: the deepest submenu first, so the root ends at the very back and each
// submenu just in front of its parent.
void RestackStandardMenus()
{
	for (sbo::CommandMenuHandle hMenu = sbo::GetOpenCommandMenu(); hMenu; hMenu = sbo::GetCommandMenuParent(hMenu))
	{
		CSboCommandMenuPanel *pPanel = FindMenuPanel(hMenu);
		if (pPanel && pPanel->IsVisible())
			pPanel->MoveBehindEverything();
	}
}
} // namespace

void CSboCommandMenuPanel::OnThink()
{
	BaseClass::OnThink();

	if (m_bPopup)
	{
		// the cursor state is set after the menu opens (ShowCommandMenu: UpdateCursorState) and follows
		// hud_capturemouse
		UpdateMouseInput();

		// a popup that was clicked or shown later must not end up behind the menu's own parent, nor the menu in
		// front of anything else: once per frame, from the root menu
		if (!sbo::GetCommandMenuParent(m_hMenu))
			RestackStandardMenus();
	}

	// the toggle images follow the cvar every frame (also toggled by a key or the console)
	for (int i = 0; i < m_Buttons.Count(); i++)
		m_Buttons[i]->UpdateChecked();
}

//-----------------------------------------------------------------------------
// SBO -> panels (sbo_command_menu.h)
//-----------------------------------------------------------------------------
void VGui2_CommandMenu_Changed()
{
	if (!VGui2_IsAvailable() || !g_pViewport)
		return;

	// the open menu and its parents, the root first (CCommandMenu::MakeVisible)
	sbo::CommandMenuHandle chain[32];
	int count = 0;
	for (sbo::CommandMenuHandle hMenu = sbo::GetOpenCommandMenu(); hMenu && count < 32;
	     hMenu = sbo::GetCommandMenuParent(hMenu))
		chain[count++] = hMenu;

	// the spectator menus belong to the spectator bars: not without them
	if (count > 0 && sbo::IsSpectatorStyleCommandMenu(chain[count - 1])
	    && !(CSboSpectatorPanel::Get() && CSboSpectatorPanel::Get()->IsShown()))
		count = 0;

	for (int i = 0; i < s_MenuPanels.Count(); i++)
	{
		bool bOpen = false;
		for (int j = 0; j < count; j++)
		{
			if (chain[j] == s_MenuPanels[i]->GetHandle())
				bOpen = true;
		}
		if (!bOpen)
			s_MenuPanels[i]->HideMenuPanel();
	}

	// (a spectator menu's child order is its creation order: the root opened first)
	for (int j = count - 1; j >= 0; j--)
		FindOrCreateMenuPanel(chain[j])->ShowMenuPanel();

	if (count > 0)
	{
		if (sbo::IsSpectatorStyleCommandMenu(chain[count - 1]))
		{
			// a menu created just now needs mouse input on its buttons (with the spectator panel's: the duck menu)
			if (CSboSpectatorPanel::Get())
				CSboSpectatorPanel::Get()->EnableCommandMenuMouseInput();
		}
		else
			RestackStandardMenus();
	}
}
