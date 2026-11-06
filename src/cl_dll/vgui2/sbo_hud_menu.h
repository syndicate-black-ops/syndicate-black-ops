//========= SBO VGUI2 port: HUD text menu =========//
//
// Purpose: VGUI2 drawing of the ShowMenu text menus (radio, server and AMXX plugin menus) with the look of SBO
//          Source's CHudMenu (syndicate-blackops-source src/game/client/menu.cpp): a rounded, translucent box on
//          the left, vertically centred, the menu text in the scheme's menu fonts, and a short highlight of the
//          chosen item before the menu goes away.
//
//          Only the drawing moves here. SBO's CHudMenu (menu.cpp) still owns everything else: the ShowMenu message,
//          localisation, valid slots, number keys -> menuselect, timeouts. It pushes the menu text in through
//          VGui2_HudMenu_* (vgui2_api.h); the engine-text drawing of the SDK is gone (SBO 2.0).
//
//          Colours and the hud_scale fonts: ui/resource/sboscheme.res (HudMenu.*); without hud_scale the fonts are
//          made in sbo_hud_menu.cpp (pixel heights by screen height).
//
// SDK world (hl1_source_sdk headers); SBO data through sbo_bridge.h.
//=================================================================//

#ifndef SBO_VGUI2_SBO_HUD_MENU_H
#define SBO_VGUI2_SBO_HUD_MENU_H

#include <wchar.h>
#include <Color.h>
#include <vgui_controls/EditablePanel.h>

class CSboHudMenu : public vgui2::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE(CSboHudMenu, vgui2::EditablePanel);

	static CSboHudMenu *Create(vgui2::Panel *pParent);
	static CSboHudMenu *Get() { return s_pInstance; }

	//! New menu: the full menu string as CHudMenu draws it (localised, key names swapped in, \w \d \y \r \R codes).
	void Open(const char *menuString);
	//! Menu gone (timeout, server closed it, new map): hide at once.
	void Close();
	//! Item 1..10 was chosen (menuselect sent): flash it, then fade out.
	void Select(int item);

	void OnThink() override;
	void PaintBackground() override;
	void Paint() override;
	void ApplySchemeSettings(vgui2::IScheme *pScheme) override;

private:
	CSboHudMenu(vgui2::Panel *pParent);

	static CSboHudMenu *s_pInstance;

	static constexpr int MAX_CHARS = 1024;
	static constexpr int MAX_LINES = 48;
	static constexpr int MAX_SEGMENTS = 160;

	//! Colour of a run of text (escape codes).
	enum TextColor : unsigned char
	{
		COLOR_DEFAULT, //!< \w: text or item colour of the line
		COLOR_DIM,     //!< \d
		COLOR_YELLOW,  //!< \y
		COLOR_RED,     //!< \r
	};

	struct Segment
	{
		int start = 0; //!< in m_wszText
		int len = 0;
		TextColor color = COLOR_DEFAULT;
		bool right = false; //!< after \R: right-aligned in the box
	};

	struct Line
	{
		int firstSeg = 0;
		int numSegs = 0;
		int item = 0;      //!< 1..10 if the line starts with "<digit>." (0 = 10), else 0
		int leftWide = 0;  //!< pixels of the left-aligned part
		int rightWide = 0; //!< pixels of the \R part
		int tall = 0;
		bool blank = false;
	};

	wchar_t m_wszText[MAX_CHARS] = {};
	Segment m_Segments[MAX_SEGMENTS];
	int m_iNumSegments = 0;
	Line m_Lines[MAX_LINES];
	int m_iNumLines = 0;

	// box contents in pixels (Layout)
	int m_iContentWide = 0;
	int m_iContentTall = 0;

	bool m_bOpen = false;
	int m_iSelected = 0;          //!< chosen item while it flashes, 0 = none
	float m_flSelectTime = 0;     //!< HUD time of the choice
	bool m_bProportional = false; //!< hud_scale: HudMenu.Scaled* fonts and HUD units; else pixel fonts by screen height
	int m_iPixelScreenTall = 0;   //!< without hud_scale: the screen height the fonts were made for
	int m_iPixelFontTall = 19;    //!< without hud_scale: their height in pixels

	vgui2::HFont m_hTextFont = vgui2::INVALID_FONT;
	vgui2::HFont m_hItemFont = vgui2::INVALID_FONT;
	vgui2::HFont m_hItemFontPulsing = vgui2::INVALID_FONT;

	Color m_TextColor, m_ItemColor, m_BoxColor, m_SelectedColor, m_SelectedBgColor;
	Color m_YellowColor, m_RedColor, m_DimColor;

	void Parse(const char *menuString);
	void Layout();
	bool ShouldPaint() const;
	//! 0..1 (fades out after a choice)
	float GetAlpha() const;
	vgui2::HFont GetLineFont(const Line &line) const;
	Color GetSegmentColor(const Line &line, const Segment &seg) const;
	int GetTextWide(vgui2::HFont font, const wchar_t *str, int len) const;
	void GetBoxBounds(int &x, int &y, int &wide, int &tall, int &border);
};

#endif // SBO_VGUI2_SBO_HUD_MENU_H
