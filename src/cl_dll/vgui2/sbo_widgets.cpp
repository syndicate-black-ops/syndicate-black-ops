//========= SBO VGUI2 port: SBO-styled VGUI2 widgets =========//
//
// Purpose: see sbo_widgets.h.
//=================================================================//

#include <math.h>
#include <algorithm>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <tier2/tier2.h>
#include <FileSystem.h>
#include <vgui/Cursor.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/ScrollBarSlider.h>
#include <vgui_controls/TextImage.h>
#ifndef _WIN32
// sboui::ApplySymbolSubstitutes
#include <KeyValues.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui_controls/Frame.h>
#endif

#include "sbo_bridge.h"
#include "sbo_menus.h"
#include "sbo_widgets.h"
#include "hud_chat.h" // HudChat_OpenURL

namespace
{
Color SchemeColor(const unsigned char c[4])
{
	return Color(c[0], c[1], c[2], c[3]);
}

const Color TRANSPARENT_COLOR(0, 0, 0, 0);
} // namespace

//-----------------------------------------------------------------------------
// CSboMenuPanel
//-----------------------------------------------------------------------------
namespace
{
std::vector<CSboMenuPanel *> s_MenuPanels; // every CSboMenuPanel (AnyHidesCenterText)
}

CSboMenuPanel::~CSboMenuPanel()
{
	s_MenuPanels.erase(std::remove(s_MenuPanels.begin(), s_MenuPanels.end(), this), s_MenuPanels.end());
}

bool CSboMenuPanel::AnyHidesCenterText()
{
	for (CSboMenuPanel *p : s_MenuPanels)
	{
		if (p->m_bHidesCenterText && p->IsVisible())
			return true;
	}
	return false;
}

bool VGui2_MenuHidesCenterText()
{
	return CSboMenuPanel::AnyHidesCenterText();
}

CSboMenuPanel::CSboMenuPanel(vgui2::Panel *pParent, const char *pName)
    : BaseClass(pParent, pName)
{
	s_MenuPanels.push_back(this);
	// A popup gets mouse input although CClientViewport has none (same as the scoreboard / cursor helper).
	MakePopup(false, false);
	if (sboui::GetScheme())
		SetScheme(sboui::GetScheme());
	SetProportional(false); // layout in screen pixels through sboui::XRes/YRes, like VGUI1
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetPaintBackgroundEnabled(true);
	SetVisible(false);
}

void CSboMenuPanel::UpdateScreenBounds()
{
	vgui2::surface()->GetScreenSize(m_iScreenWide, m_iScreenTall);
	SetBounds(0, 0, m_iScreenWide, m_iScreenTall);
}

void CSboMenuPanel::ShowMenu()
{
	UpdateScreenBounds();

	// re-apply the widgets' text scheme fonts (the viewport, which reads the text scheme, may have been created after
	// this panel), then lay out
	InvalidateLayout(false, true);
	LayoutMenu();

	if (m_bHidesCenterText)
		sbo::ClearCenterText();
	SetVisible(true);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(false);
	// The engine's vgui2 clears mouse input on the children together with the popup's (HideMenu), so a button
	// stayed unclickable after the menu was shown again (vgui2_dumptree: OkButton without "M").
	EnableControlsMouseInput(this);
	MoveToFront();
	vgui2::surface()->CalculateMouseVisible();
}

bool CSboMenuPanel::EnableControlsMouseInput(vgui2::Panel *pPanel)
{
	bool bAny = false;
	for (int i = 0; i < pPanel->GetChildCount(); i++)
	{
		vgui2::Panel *pChild = pPanel->GetChild(i);
		if (!pChild)
			continue;
		CSboTextPanel *pText = dynamic_cast<CSboTextPanel *>(pChild);
		bool bControl = dynamic_cast<vgui2::Button *>(pChild) || dynamic_cast<vgui2::ScrollBar *>(pChild)
		    || dynamic_cast<vgui2::ScrollBarSlider *>(pChild) || (pText && pText->IsScrollable());
		// VGUI2 doesn't look for mouse targets inside a panel without mouse input, so a container (e.g. the GM
		// menu's background panel holding the Return button) needs it too.
		if (EnableControlsMouseInput(pChild) || bControl)
		{
			pChild->SetMouseInputEnabled(true);
			bAny = true;
		}
	}
	return bAny;
}

void CSboMenuPanel::HideMenu()
{
	SetVisible(false);
	SetMouseInputEnabled(false);
	vgui2::surface()->CalculateMouseVisible();
}

void CSboMenuPanel::OnThink()
{
	BaseClass::OnThink();

	// The cursor follows the visible popups only when something asks the surface to recalculate. A menu opened
	// while the map is still loading (MOTD) is followed by the loading screen closing, which hid the cursor
	// again: re-check every frame while the menu is up and takes the mouse (the spectator bars are up during play
	// without it: nothing to do then).
	if (IsVisible() && IsMouseInputEnabled())
		vgui2::surface()->CalculateMouseVisible();

	int wide, tall;
	vgui2::surface()->GetScreenSize(wide, tall);
	if (wide != m_iScreenWide || tall != m_iScreenTall)
	{
		UpdateScreenBounds();
		LayoutMenu();
	}
}

void CSboMenuPanel::PaintBackground()
{
	// CTransparentPanel::paintBackground: nothing for iTrans 0
	if (!m_iBackdropTrans)
		return;

	int wide, tall;
	GetSize(wide, tall);
	vgui2::surface()->DrawSetColor(sboui::InvAlphaColor(0, 0, 0, m_iBackdropTrans));
	vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
}

//-----------------------------------------------------------------------------
// CSboTransparentPanel
//-----------------------------------------------------------------------------
CSboTransparentPanel::CSboTransparentPanel(vgui2::Panel *pParent, const char *pName, int iTrans)
    : BaseClass(pParent, pName)
    , m_iTrans(iTrans)
{
	SetPaintBackgroundEnabled(true);
	SetPaintEnabled(true);
}

void CSboTransparentPanel::SetLineBorder(Color color)
{
	m_bLineBorder = true;
	m_BorderColor = color;
}

void CSboTransparentPanel::PaintBackground()
{
	if (!m_iTrans)
		return;

	int wide, tall;
	GetSize(wide, tall);
	vgui2::surface()->DrawSetColor(sboui::InvAlphaColor(0, 0, 0, m_iTrans));
	vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
}

void CSboTransparentPanel::Paint()
{
	if (!m_bLineBorder)
		return;

	int wide, tall;
	GetSize(wide, tall);
	vgui2::surface()->DrawSetColor(m_BorderColor);
	vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);
}

//-----------------------------------------------------------------------------
// CSboLabel
//-----------------------------------------------------------------------------
CSboLabel::CSboLabel(vgui2::Panel *pParent, const char *pName, const char *pszTextScheme, Color fgColor)
    : BaseClass(pParent, pName, L"")
    , m_pszScheme(pszTextScheme)
    , m_Color(fgColor)
{
	SetPaintBackgroundEnabled(false);
	SetMouseInputEnabled(false);
}

// titles.txt texts (CHudTextMessage::BufferedLocaliseTextString) end with the line break before the closing "}":
// VGUI1 labels ignored it, a VGUI2 label centres two lines and pushes the text up (clipped at the top).
static void TrimTrailingSpace(wchar_t *s)
{
	size_t n = wcslen(s);
	while (n > 0 && (s[n - 1] == L'\n' || s[n - 1] == L'\r' || s[n - 1] == L' ' || s[n - 1] == L'\t'))
		s[--n] = 0;
}

void CSboLabel::SetPlainText(const char *text)
{
	wchar_t wbuf[1024];
	sboui::TextToWide(text, wbuf, sizeof(wbuf));
	TrimTrailingSpace(wbuf);
	SetText(wbuf);
}

void CSboLabel::SetColor(Color fgColor)
{
	m_bSchemeColor = false;
	m_Color = fgColor;
	SetFgColor(m_Color);
}

void CSboLabel::SetLabelAlignment(vgui2::Label::Alignment alignment)
{
	m_Alignment = alignment;
	SetContentAlignment(m_Alignment);
}

void CSboLabel::SetLineBorder(Color color)
{
	m_bLineBorder = true;
	m_BorderColor = color;
}

void CSboLabel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	vgui2::HFont font = sboui::GetTextSchemeFont(m_pszScheme);
	if (font != vgui2::INVALID_FONT)
		SetFont(font);

	if (m_bSchemeColor)
		sboui::GetTextSchemeFgColor(m_pszScheme, m_Color);

	SetFgColor(m_Color);
	SetBgColor(TRANSPARENT_COLOR);
	SetTextInset(0, 0);
	SetContentAlignment(m_Alignment);
}

void CSboLabel::Paint()
{
	BaseClass::Paint();

	if (!m_bLineBorder)
		return;

	int wide, tall;
	GetSize(wide, tall);
	vgui2::surface()->DrawSetColor(m_BorderColor);
	vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);
}

//-----------------------------------------------------------------------------
// CSboTextPanel
//-----------------------------------------------------------------------------
CSboTextPanel::CSboTextPanel(vgui2::Panel *pParent, const char *pName, const char *pszTextScheme, Color fgColor,
    bool bScrollable)
    : BaseClass(pParent, pName)
    , m_pszScheme(pszTextScheme)
    , m_Color(fgColor)
    , m_bScrollable(bScrollable)
{
	SetPanelInteractive(false); // VGUI1 text can't be selected
	SetCursor(vgui2::dc_arrow);
	SetVerticalScrollbar(bScrollable);
	if (bScrollable)
		SetUnusedScrollbarInvisible(true); // VGUI1 ScrollPanel auto mode: the bar only when the text doesn't fit
	SetMouseInputEnabled(bScrollable); // wheel / scrollbar
	sboui::ApplySymbolSubstitutes(this); // Linux: the scroll bar's arrow buttons
}

void CSboTextPanel::SetWideText(const wchar_t *text)
{
	m_Text = text ? text : L"";
	Reinsert();
}

void CSboTextPanel::Reinsert()
{
	// the colour goes into RichText's text stream, so resolve the scheme colour now
	if (m_bSchemeColor)
		sboui::GetTextSchemeFgColor(m_pszScheme, m_Color);

	SetText(L"");
	InsertColorChange(m_hOverrideFont != vgui2::INVALID_FONT ? m_OverrideColor : m_Color);
	InsertString(m_Text.c_str());
	GotoTextStart();
	m_bGotoStartPending = true;
	InvalidateLayout();
}

void CSboTextPanel::OnThink()
{
	BaseClass::OnThink();

	// RichText keeps a text "at the end" when its scroll bar was at the end before the new layout, and a previous
	// text that fitted without scrolling counts as that: a longer text set after it (the map briefing after a
	// short MOTD in the shared text window) opened scrolled to the bottom. Go to the start again once the new
	// text is laid out.
	if (m_bGotoStartPending)
	{
		m_bGotoStartPending = false;
		GotoTextStart();
	}
}

void CSboTextPanel::OnSizeChanged(int newWide, int newTall)
{
	BaseClass::OnSizeChanged(newWide, newTall);
	if (!m_Text.empty())
		Reinsert();
}

void CSboTextPanel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	vgui2::HFont font = m_hOverrideFont != vgui2::INVALID_FONT ? m_hOverrideFont : sboui::GetTextSchemeFont(m_pszScheme);
	if (font != vgui2::INVALID_FONT)
		SetFont(font);

	SetFgColor(m_hOverrideFont != vgui2::INVALID_FONT ? m_OverrideColor : m_Color);
	SetBgColor(TRANSPARENT_COLOR);
	SetBorder(nullptr);
	SetDrawOffsets(0, 0); // VGUI1 TextPanel draws from its top-left corner

	// the font may have changed: lay the text out again (scroll bar decision)
	if (!m_Text.empty())
		Reinsert();
}

void CSboTextPanel::SetStyleOverride(vgui2::HFont font, Color color)
{
	m_hOverrideFont = font;
	m_OverrideColor = color;
	InvalidateLayout(false, true); // ApplySchemeSettings again
}

//-----------------------------------------------------------------------------
// CSboCommandButton
//-----------------------------------------------------------------------------
CSboCommandButton::CSboCommandButton(vgui2::Panel *pParent, const char *pName, vgui2::Panel *pActionTarget,
    const char *command)
    : BaseClass(pParent, pName, L"", pActionTarget, command)
{
	DrawFocusBox(false);
	SetKeyBoardInputEnabled(false);
	RecalculateText();
}

void CSboCommandButton::SetCommandText(const wchar_t *text)
{
	wcsncpy(m_szMainText, text ? text : L"", sizeof(m_szMainText) / sizeof(m_szMainText[0]) - 1);
	m_szMainText[sizeof(m_szMainText) / sizeof(m_szMainText[0]) - 1] = 0;
	RecalculateText();
}

void CSboCommandButton::SetBoundKey(char key)
{
	m_cBoundKey = key;
	RecalculateText();
}

void CSboCommandButton::RecalculateText()
{
	// CommandButton::RecalculateText: "  <key>  text", "     text" without a key, the bare text for key 255
	wchar_t buf[160] = L"";
	const int maxChars = sizeof(buf) / sizeof(buf[0]);
	if (m_cBoundKey != (char)255)
	{
		if (m_cBoundKey != 0)
		{
			wchar_t prefix[] = L"  ?  ";
			prefix[2] = (wchar_t)(unsigned char)m_cBoundKey;
			wcscpy(buf, prefix);
		}
		else
			wcscpy(buf, L"     ");
	}
	wcsncat(buf, m_szMainText, maxChars - wcslen(buf) - 1);
	SetText(buf);
}

void CSboCommandButton::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// CommandButton::paint / paintBackground with the viewport's Scheme colours (vgui_TeamFortressViewport.cpp:
	// sc_primary1 = fg, sc_secondary2 = armed fg, sc_primary2 = armed bg of "Primary Button Text")
	sbo::TextSchemeInfo info;
	if (sbo::GetTextSchemeInfo(sboui::DEFAULT_TEXT_SCHEME, info))
	{
		SetDefaultColor(SchemeColor(info.fg), TRANSPARENT_COLOR);
		SetArmedColor(SchemeColor(info.armedFg), SchemeColor(info.armedBg));
		SetDepressedColor(SchemeColor(info.armedFg), SchemeColor(info.armedBg));
		SetSelectedColor(SchemeColor(info.fg), TRANSPARENT_COLOR);
	}

	vgui2::HFont font = sboui::GetTextSchemeFont(sboui::DEFAULT_TEXT_SCHEME);
	if (font != vgui2::INVALID_FONT)
		SetFont(font);

	SetTextInset(0, 0);
	SetContentAlignment(m_Alignment);
}

void CSboCommandButton::Paint()
{
	BaseClass::Paint();

	// SBO's CommandButton::paint: setBorder( new LineBorder( Color( 41, 107, 34, 0 ) ) )
	int wide, tall;
	GetSize(wide, tall);
	vgui2::surface()->DrawSetColor(sboui::SBO_GREEN);
	vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);
}

void CSboCommandButton::OnCursorExited()
{
	// VGUI1 CommandButton with bNoHighlight: nothing disarms it when the cursor leaves
	if (m_bStickyArmed)
		return;

	BaseClass::OnCursorExited();
}

vgui2::IBorder *CSboCommandButton::GetBorder(bool depressed, bool armed, bool selected, bool keyfocus)
{
	return nullptr; // the line is drawn in Paint
}

//-----------------------------------------------------------------------------
// CSboColorButton
//-----------------------------------------------------------------------------
CSboColorButton::CSboColorButton(vgui2::Panel *pParent, const char *pName, vgui2::Panel *pActionTarget,
    const char *command)
    : BaseClass(pParent, pName, L"", pActionTarget, command)
{
	DrawFocusBox(false);
	SetKeyBoardInputEnabled(false);
	SetPaintBackgroundEnabled(true);
}

void CSboColorButton::SetColors(Color unarmedText, Color armedText, Color unarmedBorder, Color armedBorder)
{
	m_UnarmedText = unarmedText;
	m_ArmedText = armedText;
	m_UnarmedBorder = unarmedBorder;
	m_ArmedBorder = armedBorder;
	InvalidateLayout(false, true);
}

void CSboColorButton::SetPlainText(const char *text)
{
	wchar_t wbuf[256];
	sboui::TextToWide(text, wbuf, sizeof(wbuf));
	TrimTrailingSpace(wbuf);
	SetText(wbuf);
}

void CSboColorButton::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// ColorButton::paint: the armed / unarmed colour, no background (paintBackground only draws the outline)
	SetDefaultColor(m_UnarmedText, TRANSPARENT_COLOR);
	SetArmedColor(m_ArmedText, TRANSPARENT_COLOR);
	SetDepressedColor(m_ArmedText, TRANSPARENT_COLOR);
	SetSelectedColor(m_UnarmedText, TRANSPARENT_COLOR);

	vgui2::HFont font = sboui::GetTextSchemeFont(sboui::DEFAULT_TEXT_SCHEME);
	if (font != vgui2::INVALID_FONT)
		SetFont(font);

	SetTextInset(0, 0);
	SetContentAlignment(vgui2::Label::a_center);
}

void CSboColorButton::PaintBackground()
{
	int wide, tall;
	GetSize(wide, tall);
	vgui2::surface()->DrawSetColor(IsArmed() ? m_ArmedBorder : m_UnarmedBorder);
	vgui2::surface()->DrawOutlinedRect(0, 0, wide, tall);
}

vgui2::IBorder *CSboColorButton::GetBorder(bool depressed, bool armed, bool selected, bool keyfocus)
{
	return nullptr; // the outline is drawn in PaintBackground
}

//-----------------------------------------------------------------------------
// CSboTgaTexture
//-----------------------------------------------------------------------------
CSboTgaTexture::~CSboTgaTexture()
{
	if (m_iTexture != -1 && vgui2::surface())
		vgui2::surface()->DeleteTextureByID(m_iTexture);
}

bool CSboTgaTexture::Load(const char *pszImageName)
{
	// GetTGANameForRes: 320_ below 1024 pixels screen width, else 640_
	int screenWide, screenTall;
	sbo::GetScreenSize(screenWide, screenTall);

	char path[256];
	snprintf(path, sizeof(path), "gfx/vgui/%d_%s", screenWide < 1024 ? 320 : 640, pszImageName);

	char file[260];
	snprintf(file, sizeof(file), "%s.tga", path);
	if (!g_pFullFileSystem->FileExists(file))
	{
		// CImageLabel::LoadImage fallback: resolution independent, path relative to the mod
		snprintf(path, sizeof(path), "%s", pszImageName);
		char *ext = strrchr(path, '.');
		if (ext && !stricmp(ext, ".tga"))
			*ext = 0;
		snprintf(file, sizeof(file), "%s.tga", path);
		if (!g_pFullFileSystem->FileExists(file))
		{
			m_bLoaded = false;
			m_iWide = m_iTall = 1;
			return false;
		}
	}

	if (m_iTexture == -1)
		m_iTexture = vgui2::surface()->CreateNewTextureID();

	vgui2::surface()->DrawSetTextureFile(m_iTexture, path, true, false);
	vgui2::surface()->DrawGetTextureSize(m_iTexture, m_iWide, m_iTall);
	m_bLoaded = true;
	return true;
}

void CSboTgaTexture::Draw(int x, int y) const
{
	if (!m_bLoaded)
		return;

	vgui2::surface()->DrawSetTexture(m_iTexture);
	vgui2::surface()->DrawSetColor(255, 255, 255, 255);
	vgui2::surface()->DrawTexturedRect(x, y, x + m_iWide, y + m_iTall);
}

//-----------------------------------------------------------------------------
// CSboSpectButton
//-----------------------------------------------------------------------------
namespace
{
// SpectButton::paint / paintBackground (VGUI1 alpha)
const Color SPECT_TEXT = sboui::InvAlphaColor(143, 143, 54, 15);
const Color SPECT_TEXT_ARMED = sboui::InvAlphaColor(194, 202, 54, 0);
const Color SPECT_ARMED_BAR = sboui::InvAlphaColor(143, 143, 54, 125);
} // namespace

CSboSpectButton::CSboSpectButton(vgui2::Panel *pParent, const char *pName)
    : BaseClass(pParent, pName, L"")
{
	DrawFocusBox(false);
	SetKeyBoardInputEnabled(false);
	SetPaintBackgroundEnabled(true);
}

void CSboSpectButton::SetCommandText(const wchar_t *text, char boundKey)
{
	wcsncpy(m_szMainText, text ? text : L"", sizeof(m_szMainText) / sizeof(m_szMainText[0]) - 1);
	m_szMainText[sizeof(m_szMainText) / sizeof(m_szMainText[0]) - 1] = 0;

	// CommandButton::RecalculateText
	wchar_t buf[96] = L"";
	if (boundKey != (char)255)
	{
		if (boundKey != 0)
		{
			wchar_t prefix[] = L"  ?  ";
			prefix[2] = (wchar_t)(unsigned char)boundKey;
			wcscpy(buf, prefix);
		}
		else
			wcscpy(buf, L"     ");
	}
	wcsncat(buf, m_szMainText, sizeof(buf) / sizeof(buf[0]) - wcslen(buf) - 1);
	SetText(buf);
	InvalidateLayout();
}

void CSboSpectButton::SetToggle(bool bToggle)
{
	m_bToggle = bToggle;
	InvalidateLayout(false, true);
}

void CSboSpectButton::SetSubMenuArrow(bool bArrow)
{
	m_bArrow = bArrow;
	InvalidateLayout(false, true);
}

void CSboSpectButton::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// CCommandMenu::AddButton / CreateCommandMenu: setFont( Scheme::sf_primary3 ) = "CommandMenu Text"
	vgui2::HFont font = sboui::GetTextSchemeFont("CommandMenu Text");
	if (font != vgui2::INVALID_FONT)
		SetFont(font);

	SetDefaultColor(SPECT_TEXT, TRANSPARENT_COLOR);
	SetArmedColor(SPECT_TEXT, TRANSPARENT_COLOR);
	SetDepressedColor(SPECT_TEXT, TRANSPARENT_COLOR);
	SetSelectedColor(SPECT_TEXT, TRANSPARENT_COLOR);
	SetTextInset(0, 0);
	SetContentAlignment(vgui2::Label::a_west); // CommandButton::Init

	// the images (resolution dependent: 320_ / 640_)
	if (m_bToggle)
		m_Checked.Load("checked");
	if (m_bArrow)
		m_Arrow.Load("arrow");
}

void CSboSpectButton::PerformLayout()
{
	BaseClass::PerformLayout();

	int wide, tall;
	GetSize(wide, tall);

	if (m_bToggle)
	{
		// SpectToggleButton constructor: x = the width of the text it had then ("     text", no key bound yet) in
		// the default VGUI1 font (the "CommandMenu Text" font is set afterwards), vertically centred
		int textWide = 0, textTall = 0;
		vgui2::HFont font = sboui::GetTextSchemeFont(sboui::DEFAULT_TEXT_SCHEME);
		if (font != vgui2::INVALID_FONT)
		{
			wchar_t buf[80] = L"     ";
			wcsncat(buf, m_szMainText, sizeof(buf) / sizeof(buf[0]) - wcslen(buf) - 1);
			vgui2::surface()->GetTextSize(font, buf, textWide, textTall);
		}
		m_iCheckedX = textWide;
		m_iCheckedY = (tall - m_Checked.GetTall()) / 2;
	}

	if (m_bArrow)
	{
		// CreateSubMenu: ( CMENU_SIZE_X - image wide, ( BUTTON_SIZE_Y - image tall ) / 2 ); BUTTON_SIZE_Y even in
		// the spectator menus' half-height buttons
		m_iArrowX = sboui::XRes(160) - m_Arrow.GetWide();
		m_iArrowY = (sboui::YRes(30) - m_Arrow.GetTall()) / 2;
	}
}

void CSboSpectButton::PaintBackground()
{
	if (!IsSpectArmed())
		return;

	int wide, tall;
	GetSize(wide, tall);
	vgui2::surface()->DrawSetColor(SPECT_ARMED_BAR);
	vgui2::surface()->DrawFilledRect(5, 0, wide - 5, tall);
}

void CSboSpectButton::Paint()
{
	SetFgColor(IsSpectArmed() ? SPECT_TEXT_ARMED : SPECT_TEXT);
	BaseClass::Paint();

	if (m_bToggle && m_bChecked)
		m_Checked.Draw(m_iCheckedX, m_iCheckedY);
	if (m_bArrow)
		m_Arrow.Draw(m_iArrowX, m_iArrowY);
}

vgui2::IBorder *CSboSpectButton::GetBorder(bool depressed, bool armed, bool selected, bool keyfocus)
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// CSboImagePanel
//-----------------------------------------------------------------------------
CSboImagePanel::CSboImagePanel(vgui2::Panel *pParent, const char *pName, const char *pszImageName)
    : BaseClass(pParent, pName)
{
	SetPaintBackgroundEnabled(false);
	if (pszImageName)
		SetImage(pszImageName);
}

bool CSboImagePanel::SetImage(const char *pszImageName)
{
	if (!m_Image.Load(pszImageName))
		return false;

	SetSize(m_Image.GetWide(), m_Image.GetTall());
	return true;
}

void CSboImagePanel::GetImageSize(int &wide, int &tall) const
{
	wide = m_Image.GetWide();
	tall = m_Image.GetTall();
}

void CSboImagePanel::Paint()
{
	m_Image.Draw(0, 0);
}

//-----------------------------------------------------------------------------
// CSboLinkLabel
//-----------------------------------------------------------------------------
CSboLinkLabel::CSboLinkLabel(vgui2::Panel *pParent, const char *pName, const char *text, const char *url)
    : BaseClass(pParent, pName, text, url)
{
}

void CSboLinkLabel::SetLinkFont(vgui2::HFont font)
{
	m_hLinkFont = font;
	if (font)
		SetFont(font);
}

void CSboLinkLabel::OnMousePressed(vgui2::MouseCode code)
{
	if (code == vgui2::MOUSE_LEFT && GetURL())
		HudChat_OpenURL(GetURL());
}

void CSboLinkLabel::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme); // the hand cursor
	vgui2::HFont font = m_hLinkFont ? m_hLinkFont : pScheme->GetFont("Default", IsProportional());
	if (font)
		SetFont(font);
}

void CSboLinkLabel::Paint()
{
	BaseClass::Paint();
	vgui2::TextImage *pText = GetTextImage();
	if (!pText)
		return;
	int x, y, w, h;
	pText->GetPos(x, y);
	pText->GetContentSize(w, h);
	if (w <= 0 || h <= 0)
		return;
	// under the baseline (about four fifths of the font's height)
	const int lineY = std::min(GetTall() - 1, y + h * 4 / 5 + 1);
	vgui2::surface()->DrawSetColor(pText->GetColor());
	vgui2::surface()->DrawFilledRect(x, lineY, x + std::min(w, GetWide() - x), lineY + 1);
}

#ifndef _WIN32
//-----------------------------------------------------------------------------
// sboui::ApplySymbolSubstitutes (Linux; see sbo_widgets.h)
//-----------------------------------------------------------------------------
namespace
{
// The SDK's ScrollBarButton (vgui_controls/ScrollBar.cpp, private to that file) with the Marlett arrow character
// replaced by a triangle drawn with rectangles: same activation, colours, borders and mouse handling.
class CSboArrowButton : public vgui2::Button
{
	DECLARE_CLASS_SIMPLE(CSboArrowButton, vgui2::Button);

public:
	enum Direction
	{
		UP,
		DOWN,
		LEFT,
		RIGHT,
	};

	CSboArrowButton(const char *panelName, Direction dir)
	    : BaseClass(NULL, panelName, "")
	    , m_Direction(dir)
	{
		SetButtonActivationType(ACTIVATE_ONPRESSED);
		SetContentAlignment(vgui2::Label::a_center);
	}

	void OnMouseFocusTicked() override
	{
		// pass straight up to parent
		CallParentFunction(new KeyValues("MouseFocusTicked"));
	}

	void ApplySchemeSettings(vgui2::IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);

		SetDefaultBorder(pScheme->GetBorder("ScrollBarButtonBorder"));
		SetDepressedBorder(pScheme->GetBorder("ScrollBarButtonDepressedBorder"));

		SetDefaultColor(GetSchemeColor("ScrollBarButton.FgColor", pScheme), GetSchemeColor("ScrollBarButton.BgColor", pScheme));
		SetArmedColor(GetSchemeColor("ScrollBarButton.ArmedFgColor", pScheme), GetSchemeColor("ScrollBarButton.ArmedBgColor", pScheme));
		SetDepressedColor(GetSchemeColor("ScrollBarButton.DepressedFgColor", pScheme), GetSchemeColor("ScrollBarButton.DepressedBgColor", pScheme));
	}

	// Don't request focus.
	// This will keep cursor focus in main window in text entry windows.
	void OnMousePressed(vgui2::MouseCode code) override
	{
		if (!IsEnabled())
			return;

		if (!IsMouseClickEnabled(code))
			return;

		if (IsUseCaptureMouseEnabled())
		{
			SetSelected(true);
			Repaint();

			// lock mouse input to going to this button
			vgui2::input()->SetMouseCapture(GetVPanel());
		}
	}

	void OnMouseReleased(vgui2::MouseCode code) override
	{
		if (!IsEnabled())
			return;

		if (!IsMouseClickEnabled(code))
			return;

		if (IsUseCaptureMouseEnabled())
		{
			SetSelected(false);
			Repaint();

			// lock mouse input to going to this button
			vgui2::input()->SetMouseCapture(NULL);
		}

		if (vgui2::input()->GetMouseOver() == GetVPanel())
		{
			SetArmed(true);
		}
	}

	// a filled triangle in the button's text colour, centred: one row (column) of pixels per step
	void Paint() override
	{
		int wide, tall;
		GetSize(wide, tall);
		const int side = wide < tall ? wide : tall;
		const int rows = side / 4 + 1; // 16 pixel button: 5 rows, 9 pixels at the base
		const int cx = wide / 2, cy = tall / 2;

		vgui2::surface()->DrawSetColor(GetButtonFgColor());
		for (int i = 0; i < rows; i++)
		{
			switch (m_Direction)
			{
			case UP:
				vgui2::surface()->DrawFilledRect(cx - i, cy - rows / 2 + i, cx + i + 1, cy - rows / 2 + i + 1);
				break;
			case DOWN:
				vgui2::surface()->DrawFilledRect(cx - i, cy + (rows - 1) / 2 - i, cx + i + 1, cy + (rows - 1) / 2 - i + 1);
				break;
			case LEFT:
				vgui2::surface()->DrawFilledRect(cx - rows / 2 + i, cy - i, cx - rows / 2 + i + 1, cy + i + 1);
				break;
			case RIGHT:
				vgui2::surface()->DrawFilledRect(cx + (rows - 1) / 2 - i, cy - i, cx + (rows - 1) / 2 - i + 1, cy + i + 1);
				break;
			}
		}
	}

private:
	Direction m_Direction;
};

// The font of the close button's sign: bold, larger than the scheme's text fonts, still inside the 18 pixel button.
vgui2::HFont CloseButtonFont()
{
	static vgui2::HFont s_hFont = vgui2::INVALID_FONT;
	if (s_hFont == vgui2::INVALID_FONT)
	{
		s_hFont = vgui2::surface()->CreateFont();
		vgui2::surface()->AddGlyphSetToFont(s_hFont, "Liberation Sans", 22, 700, 0, 0,
		    vgui2::ISurface::FONTFLAG_ANTIALIAS, 0, 0);
	}
	return s_hFont;
}

void ReplaceScrollBarButton(vgui2::ScrollBar *pBar, int index, CSboArrowButton::Direction dir, const char *name)
{
	vgui2::Button *pOld = pBar->GetButton(index);
	if (!pOld || dynamic_cast<CSboArrowButton *>(pOld))
		return;

	pBar->SetButton(new CSboArrowButton(name, dir), index); // takes the old one off the bar, does not delete it
	pOld->MarkForDeletion();
}
// a Frame's title bar close button (Marlett "r"): a multiplication sign in a text font
void SubstituteCloseButton(vgui2::Button *pClose)
{
	pClose->SetFont(CloseButtonFont());
	pClose->SetText(L"\u00D7");
	pClose->SetContentAlignment(vgui2::Label::a_center);
	pClose->SetTextInset(0, 0);
}
} // namespace

void sboui::ApplyCloseButtonSubstitute(vgui2::Panel *pFrame)
{
	if (!pFrame)
		return;
	if (vgui2::Button *pClose = dynamic_cast<vgui2::Button *>(pFrame->FindChildByName("frame_close")))
		SubstituteCloseButton(pClose);
}

void sboui::ApplySymbolSubstitutes(vgui2::Panel *pRoot)
{
	if (!pRoot)
		return;

	for (int i = 0; i < pRoot->GetChildCount(); i++)
	{
		vgui2::Panel *pChild = pRoot->GetChild(i);
		if (!pChild)
			continue;

		if (vgui2::ScrollBar *pBar = dynamic_cast<vgui2::ScrollBar *>(pChild))
		{
			// the SDK's ScrollBar: "t" / "u" on a vertical bar, "w" / "4" on a horizontal one
			if (pBar->IsVertical())
			{
				ReplaceScrollBarButton(pBar, 0, CSboArrowButton::UP, "UpButton");
				ReplaceScrollBarButton(pBar, 1, CSboArrowButton::DOWN, "DownButton");
			}
			else
			{
				ReplaceScrollBarButton(pBar, 0, CSboArrowButton::LEFT, NULL);
				ReplaceScrollBarButton(pBar, 1, CSboArrowButton::RIGHT, NULL);
			}
			pBar->InvalidateLayout();
			continue;
		}

		// vgui2::Frame's title bar close button (Marlett "r")
		if (dynamic_cast<vgui2::Frame *>(pRoot) && pChild->GetName() && !strcmp(pChild->GetName(), "frame_close"))
		{
			if (vgui2::Button *pClose = dynamic_cast<vgui2::Button *>(pChild))
				SubstituteCloseButton(pClose);
			continue;
		}

		ApplySymbolSubstitutes(pChild);
	}
}
#endif

//-----------------------------------------------------------------------------
// Scale helpers (sbo_widgets.h; Aura's hl1_source_sdk Panel.cpp)
//-----------------------------------------------------------------------------
float sboui::GetProportionalScale(vgui2::Panel *pPanel)
{
	float scale = 1.0f;
	if (pPanel && pPanel->IsProportional())
	{
		int screenW, screenH;
		vgui2::surface()->GetScreenSize(screenW, screenH);

		int proW, proH;
		vgui2::VGui_GetProportionalBase(proW, proH);

		scale = ((float)(screenH) / (float)(proH));
	}
	return scale;
}

int sboui::GetScaledValue(vgui2::Panel *pPanel, int iValue)
{
	return (int)(ceil(iValue * GetProportionalScale(pPanel)));
}
