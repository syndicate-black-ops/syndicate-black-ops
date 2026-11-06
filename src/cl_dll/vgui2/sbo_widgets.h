//========= SBO VGUI2 port: SBO-styled VGUI2 widgets =========//
//
// Purpose: VGUI2 equivalents of the VGUI1 building blocks SBO's menus are made of (vgui_TeamFortressViewport.h,
//          vgui_CustomObjects.cpp; removed in SBO 2.0), so the menus look like the originals (docs/dev/VGUI2_MIGRATION.md):
//
//   VGUI1                                         VGUI2
//   CMenuPanel (full-screen, stacked menu)        CSboMenuPanel   (full-screen popup, VGUI1 backdrop alpha)
//   CTransparentPanel + LineBorder                CSboTransparentPanel
//   Label (+ scheme font / fg colour)             CSboLabel
//   TextPanel, ScrollPanel + TextPanel            CSboTextPanel   (RichText; optional VGUI1-style auto scrollbar)
//   CommandButton (+ SBO's green LineBorder)      CSboCommandButton
//   ColorButton (spectator bars)                  CSboColorButton
//   SpectButton / SpectToggleButton               CSboSpectButton (spectator command menus)
//   CImageLabel (gfx/vgui/<res>_<name>.tga)       CSboImagePanel, CSboTgaTexture (drawn inside another panel)
//
// Fonts and colours come from SBO's VGUI1 text scheme (sboui::GetTextSchemeFont, sbo::GetTextSchemeInfo) and are
// re-applied in ApplySchemeSettings, so a scheme reload never leaves the VGUI2 scheme's defaults on them.
// Positions: sboui::XRes / sboui::YRes (VGUI1's XRES/YRES).
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_WIDGETS_H
#define SBO_VGUI2_SBO_WIDGETS_H

#include <string>

#include <Color.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/URLLabel.h>

//-----------------------------------------------------------------------------
// VGUI1 CMenuPanel: a full-screen popup (child of CClientViewport) that darkens the screen with a VGUI1-alpha
// backdrop and holds a menu's controls. Shown/hidden by the menu's vgui2_api.h functions, which SBO's VGUI1 menu
// stack calls through a proxy (sbo_menu_proxy.h). While shown it takes the mouse (VGUI2 cursor); keyboard input
// stays with the engine, so SBO's TeamFortressViewport::KeyInput keeps handling the menu keys (Enter/Esc/digits).
//-----------------------------------------------------------------------------
class CSboMenuPanel : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CSboMenuPanel, vgui2::EditablePanel);

public:
	CSboMenuPanel(vgui2::Panel *pParent, const char *pName);

	//! CTransparentPanel's iTrans of the full-screen backdrop: VGUI1 alpha (0 = no backdrop at all, as in VGUI1;
	//! 1..254 = black with alpha 255 - iTrans; 255 = invisible).
	void SetBackdropTrans(int iTrans) { m_iBackdropTrans = iTrans; }

	void ShowMenu();
	void HideMenu();
	//! The engine's center text (a plugin's print_center, drawn over VGUI2) is cleared when this menu opens and
	//! dropped while it is up (VGui2_MenuHidesCenterText); on by default
	void SetHidesCenterText(bool bHides) { m_bHidesCenterText = bHides; }
	//! A shown menu that hides the center text
	static bool AnyHidesCenterText();
	virtual ~CSboMenuPanel();

	virtual void OnThink() override;
	virtual void PaintBackground() override;

protected:
	//! Places the controls exactly like the VGUI1 constructor did (sboui::XRes/YRes of the current screen). Called by
	//! ShowMenu after the fonts were (re)applied, and when the screen size changes while shown.
	virtual void LayoutMenu() = 0;

	//! Full-screen bounds (ShowMenu does this; for panels shown without ShowMenu).
	void UpdateScreenBounds();
	//! Mouse input on for the interactive descendants (buttons, scroll bars, scrollable text) and the panels that
	//! contain them. True if pPanel has any.
	static bool EnableControlsMouseInput(vgui2::Panel *pPanel);

private:

	int m_iBackdropTrans = 100;
	bool m_bHidesCenterText = true;
	int m_iScreenWide = 0;
	int m_iScreenTall = 0;
};

//-----------------------------------------------------------------------------
// VGUI1 CTransparentPanel (black, VGUI1-alpha iTrans; 0 = no background) with an optional 1-pixel LineBorder.
//-----------------------------------------------------------------------------
class CSboTransparentPanel : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboTransparentPanel, vgui2::Panel);

public:
	CSboTransparentPanel(vgui2::Panel *pParent, const char *pName, int iTrans);

	void SetTrans(int iTrans) { m_iTrans = iTrans; }
	//! LineBorder colour (VGUI2 alpha); SBO's menus use sboui::SBO_GREEN.
	void SetLineBorder(Color color);
	void RemoveLineBorder() { m_bLineBorder = false; }

	virtual void PaintBackground() override;
	virtual void Paint() override;

private:
	int m_iTrans;
	bool m_bLineBorder = false;
	Color m_BorderColor;
};

//-----------------------------------------------------------------------------
// VGUI1 Label: west-aligned text in a VGUI1 text scheme font and a fixed colour, no background.
//-----------------------------------------------------------------------------
class CSboLabel : public vgui2::Label
{
	DECLARE_CLASS_SIMPLE(CSboLabel, vgui2::Label);

public:
	//! pszTextScheme: text scheme whose font is used (string literal / static, kept by pointer).
	CSboLabel(vgui2::Panel *pParent, const char *pName, const char *pszTextScheme, Color fgColor);

	//! Sets text from SBO/server data (UTF-8 or Latin-1, see sboui::TextToWide); never localized.
	void SetPlainText(const char *text);
	//! The colour is the text scheme's FgColor instead (VGUI1: setFgColor( pSchemes->getFgColor( hScheme ) )),
	//! resolved whenever the scheme is applied; fgColor of the constructor is the fallback.
	void UseTextSchemeFgColor() { m_bSchemeColor = true; }
	//! VGUI1 setFgColor after construction (VGUI2 alpha).
	void SetColor(Color fgColor);
	//! VGUI1 setContentAlignment (default a_west).
	void SetLabelAlignment(vgui2::Label::Alignment alignment);
	//! VGUI1 setBorder( new LineBorder( color ) ): 1-pixel outline (VGUI2 alpha).
	void SetLineBorder(Color color);

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void Paint() override;

private:
	const char *m_pszScheme;
	Color m_Color;
	bool m_bSchemeColor = false;
	vgui2::Label::Alignment m_Alignment = vgui2::Label::a_west;
	bool m_bLineBorder = false;
	Color m_BorderColor;
};

//-----------------------------------------------------------------------------
// VGUI1 TextPanel (word-wrapped multi-line text, top-left aligned). bScrollable: the text sits in a VGUI1-style
// ScrollPanel (CTFScrollPanel) whose vertical scrollbar shows only when the text doesn't fit (mouse wheel works).
//-----------------------------------------------------------------------------
class CSboTextPanel : public vgui2::RichText
{
	DECLARE_CLASS_SIMPLE(CSboTextPanel, vgui2::RichText);

public:
	CSboTextPanel(vgui2::Panel *pParent, const char *pName, const char *pszTextScheme, Color fgColor, bool bScrollable);

	//! Replaces the text (scrolled to the top).
	void SetWideText(const wchar_t *text);
	//! The colour is the text scheme's FgColor instead (see CSboLabel). Resolved when text is set, so set the text
	//! while SBO's viewport exists (e.g. when the menu opens).
	void UseTextSchemeFgColor() { m_bSchemeColor = true; }

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	//! Another font and color than the text scheme's (the new in-game menus' map info text); INVALID_FONT: the text
	//! scheme's again
	void SetStyleOverride(vgui2::HFont font, Color color);

	bool IsScrollable() const { return m_bScrollable; }

	virtual void OnSizeChanged(int newWide, int newTall) override;
	virtual void OnThink() override;

private:
	//! Re-inserts the current text: RichText decides about its scroll bar from the size and font in effect when the
	//! text is inserted, and menus set their text before their layout/scheme is applied.
	void Reinsert();

	const char *m_pszScheme;
	Color m_Color;
	bool m_bScrollable;
	bool m_bSchemeColor = false;
	vgui2::HFont m_hOverrideFont = vgui2::INVALID_FONT;
	Color m_OverrideColor;
	//! New text: go to its start once more after RichText has laid it out (see OnThink).
	bool m_bGotoStartPending = false;
	std::wstring m_Text;
};

//-----------------------------------------------------------------------------
// VGUI1 CommandButton (vgui_CustomObjects.cpp) with SBO's green LineBorder: "Primary Button Text" font and colours
// (text fg, armed: armed fg on armed bg, no background otherwise), armed while the cursor is over it, text left
// aligned behind the bound key ("  <key>  text") or five spaces. Clicking sends `command` to the action target.
//-----------------------------------------------------------------------------
class CSboCommandButton : public vgui2::Button
{
	DECLARE_CLASS_SIMPLE(CSboCommandButton, vgui2::Button);

public:
	CSboCommandButton(vgui2::Panel *pParent, const char *pName, vgui2::Panel *pActionTarget, const char *command);

	//! CommandButton::setText: the text without the key prefix.
	void SetCommandText(const wchar_t *text);
	//! CommandButton::setBoundKey: 0 = none (five spaces), (char)255 = no prefix at all, else "  <key>  ".
	void SetBoundKey(char key);
	//! CommandButton( ..., bNoHighlight = true ) + CHandler_MenuButtonOver (team/class menus): armed when the cursor
	//! enters, but leaving doesn't disarm it (only SetArmed(false), e.g. another button of the menu being armed).
	void SetStickyArmed(bool bSticky) { m_bStickyArmed = bSticky; }
	//! CommandButton::setContentAlignment after construction (default a_west, e.g. a_center for the briefing's
	//! Cancel / Go ! buttons). The key prefix stays part of the text, as in VGUI1.
	void SetCommandAlignment(vgui2::Label::Alignment alignment) { m_Alignment = alignment; }

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void OnCursorExited() override;
	virtual void Paint() override;
	virtual vgui2::IBorder *GetBorder(bool depressed, bool armed, bool selected, bool keyfocus) override;

private:
	void RecalculateText();

	wchar_t m_szMainText[128] = {};
	char m_cBoundKey = 0;
	bool m_bStickyArmed = false;
	vgui2::Label::Alignment m_Alignment = vgui2::Label::a_west;
};

//-----------------------------------------------------------------------------
// VGUI1 ColorButton (vgui_TeamFortressViewport.h) as the spectator bars use it: setBoundKey( (char)255 ) (no key
// prefix), centred text in the VGUI1 default font, no background, the text and a 1-pixel outline in an armed and an
// unarmed colour; armed while the cursor is over it.
//-----------------------------------------------------------------------------
class CSboColorButton : public vgui2::Button
{
	DECLARE_CLASS_SIMPLE(CSboColorButton, vgui2::Button);

public:
	CSboColorButton(vgui2::Panel *pParent, const char *pName, vgui2::Panel *pActionTarget, const char *command);

	//! setUnArmedColor / setArmedColor / setUnArmedBorderColor / setArmedBorderColor (VGUI2 alpha).
	void SetColors(Color unarmedText, Color armedText, Color unarmedBorder, Color armedBorder);
	//! Sets the text from SBO data (titles.txt, UTF-8 or Latin-1); never localized.
	void SetPlainText(const char *text);

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void PaintBackground() override;
	virtual vgui2::IBorder *GetBorder(bool depressed, bool armed, bool selected, bool keyfocus) override;

private:
	Color m_UnarmedText, m_ArmedText, m_UnarmedBorder, m_ArmedBorder;
};

//-----------------------------------------------------------------------------
// A VGUI1 TGA (LoadTGAForRes / CImageLabel: gfx/vgui/<320|640>_<name>.tga, 320 below 1024 pixels screen width,
// else "<name>" relative to the mod) as a texture that a panel draws itself, unscaled.
//-----------------------------------------------------------------------------
class CSboTgaTexture
{
public:
	~CSboTgaTexture();

	//! Loads the image for SBO's current resolution. False if no file was found (then nothing is drawn).
	bool Load(const char *pszImageName);
	bool IsLoaded() const { return m_bLoaded; }
	//! Image size; 1x1 if none (CImageLabel::getImageWide/Tall).
	int GetWide() const { return m_iWide; }
	int GetTall() const { return m_iTall; }
	//! Draws it at (x, y) of the panel being painted.
	void Draw(int x, int y) const;

private:
	int m_iTexture = -1;
	int m_iWide = 1;
	int m_iTall = 1;
	bool m_bLoaded = false;
};

//-----------------------------------------------------------------------------
// VGUI1 SpectButton / SpectToggleButton (the spectator command menus): "CommandMenu Text" font, text left aligned
// behind the key prefix ("  <key>  text"), unarmed text 143 143 54 (VGUI1 alpha 15), armed text 194 202 54 on a
// 143 143 54 (VGUI1 alpha 125) bar 5 pixels in from both sides; no border. A TOGGLE button shows the "checked" TGA
// while checked; a button with a submenu the "arrow" TGA (CreateSubMenu).
//-----------------------------------------------------------------------------
class CSboSpectButton : public vgui2::Button
{
	DECLARE_CLASS_SIMPLE(CSboSpectButton, vgui2::Button);

public:
	CSboSpectButton(vgui2::Panel *pParent, const char *pName);

	//! CommandButton::setText + setBoundKey: the text without the prefix; key 0 = five spaces, (char)255 = none.
	void SetCommandText(const wchar_t *text, char boundKey);
	//! SpectToggleButton: shows the "checked" image while SetChecked(true).
	void SetToggle(bool bToggle);
	void SetChecked(bool bChecked) { m_bChecked = bChecked; }
	//! CreateSubMenu's "arrow" image.
	void SetSubMenuArrow(bool bArrow);

	//! The armed look; default: VGUI2's own hover state. Overridden where the state lives elsewhere.
	virtual bool IsSpectArmed() { return IsArmed(); }

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void PerformLayout() override;
	virtual void PaintBackground() override;
	virtual void Paint() override;
	virtual vgui2::IBorder *GetBorder(bool depressed, bool armed, bool selected, bool keyfocus) override;

private:
	wchar_t m_szMainText[64] = {};
	bool m_bToggle = false;
	bool m_bChecked = false;
	bool m_bArrow = false;
	CSboTgaTexture m_Checked;
	CSboTgaTexture m_Arrow;
	int m_iCheckedX = 0, m_iCheckedY = 0;
	int m_iArrowX = 0, m_iArrowY = 0;
};

//-----------------------------------------------------------------------------
// VGUI1 CImageLabel / LoadTGAForRes: gfx/vgui/<320|640>_<name>.tga (320 below 1024 pixels screen width), drawn
// unscaled at the image's size. Falls back to "<name>" as a path relative to the mod (CImageLabel::LoadImage).
//-----------------------------------------------------------------------------
class CSboImagePanel : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboImagePanel, vgui2::Panel);

public:
	CSboImagePanel(vgui2::Panel *pParent, const char *pName, const char *pszImageName = nullptr);

	//! Loads the image for SBO's current resolution and sizes the panel to it. False if no file was found.
	bool SetImage(const char *pszImageName);
	//! Size of the loaded image (1x1 if none, like CImageLabel::getImageWide/Tall).
	void GetImageSize(int &wide, int &tall) const;

	virtual void Paint() override;

private:
	CSboTgaTexture m_Image;
};

//-----------------------------------------------------------------------------
// A clickable web link: VGUI2's URLLabel (hand cursor), opened as chat links are (HudChat_OpenURL: the Steam overlay,
// else the system browser). The text is drawn in the scheme's "Default" font, or the font given with SetLinkFont,
// and underlined here, so it looks like a link in any scheme (not every one has "DefaultUnderline").
//-----------------------------------------------------------------------------
class CSboLinkLabel : public vgui2::URLLabel
{
	DECLARE_CLASS_SIMPLE(CSboLinkLabel, vgui2::URLLabel);

public:
	CSboLinkLabel(vgui2::Panel *pParent, const char *pName, const char *text, const char *url);
	//! The text's font (0: the scheme's "Default")
	void SetLinkFont(vgui2::HFont font);

protected:
	virtual void OnMousePressed(vgui2::MouseCode code) override;
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void Paint() override;

private:
	vgui2::HFont m_hLinkFont = 0;
};

//-----------------------------------------------------------------------------
// Linux: substitutes for the "Marlett" symbol characters of the SDK's controls, which the engine's surface does not
// draw for the client's controls there (docs/dev/LINUX_CLIENT.md "Symbol font"; the SDK's own CheckButton draws its box
// with rectangles for the same reason). Covers what SBO's panels use: the arrow buttons of every ScrollBar below
// pRoot (replaced by buttons that draw a triangle; call once after the panel's controls exist) and a Frame's close
// button (a multiplication sign in a text font; call from the frame's ApplySchemeSettings, after the base class,
// which sets the Marlett font again). ApplyCloseButtonSubstitute: the close button only, for a frame whose scroll bars
// draw their own arrows. Nothing on Windows, where Marlett is drawn.
//-----------------------------------------------------------------------------
namespace sboui
{
#ifdef _WIN32
inline void ApplySymbolSubstitutes(vgui2::Panel *) {}
inline void ApplyCloseButtonSubstitute(vgui2::Panel *) {}
#else
void ApplySymbolSubstitutes(vgui2::Panel *pRoot);
void ApplyCloseButtonSubstitute(vgui2::Panel *pFrame);
#endif

//-----------------------------------------------------------------------------
// The two scale helpers of Aura's hl1_source_sdk fork (sabianroberts/hl1_source_sdk deb5910, "4K helpers":
// vgui2::Panel::GetProportionalScale / GetScaledValue, HL SDK license), as free functions so the SDK submodule stays
// unchanged. GetProportionalScale: screen height / the proportional base height (VGui_GetProportionalBase, 480) for
// a proportional panel, 1 for any other. GetScaledValue: a value in proportional-base units in screen pixels,
// rounded up.
//-----------------------------------------------------------------------------
float GetProportionalScale(vgui2::Panel *pPanel);
int GetScaledValue(vgui2::Panel *pPanel, int iValue);
} // namespace sboui

#endif // SBO_VGUI2_SBO_WIDGETS_H
