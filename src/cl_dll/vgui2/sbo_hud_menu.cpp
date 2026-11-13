//========= SBO VGUI2 port: HUD text menu =========//
//
// Purpose: see sbo_hud_menu.h. Look ported from SBO Source's CHudMenu (src/game/client/menu.cpp):
//   * box: DrawBox (rounded corners) at x 20, vertically centred, 20 pixels of border around the text
//     (Source: DrawBox( x - border/2, y - border/2, m_nMaxPixels + border, m_nHeight + border, m_BoxColor, ... ))
//   * text lines in MenuTextFont, item lines in MenuItemFont, colours MenuColor / ItemColor / MenuBoxBg
//   * the chosen item is drawn in MenuItemFontPulsing (Source: m_nSelectedItem + the overbright blur)
// GoldSrc differences:
//   * the menu string keeps GoldSrc's format: items are lines starting with "<digit>." (Source: "->1." markers),
//     the Half-Life 25 colour codes \w \d \y \r and \R (right-align the rest of the line) are honoured
//   * blank lines are kept as half-height gaps (Source drops them; GoldSrc menus use them as separators)
//   * the chosen item flashes on a highlight bar, then the menu fades out (Source hid it at once and pulsed it
//     through HudAnimations.txt, which SBO Source doesn't define)
//   * size follows the screen height like Valve's per-resolution VGUI2 fonts: without hud_scale the fonts are made
//     here from the height of the screen the menu covers (19 px up to 1199 lines, then the same share of the screen,
//     PixelFontTall) and made again when that height changes; the box grows with them. With hud_scale the menu is
//     part of the scaled HUD instead: the sboscheme.res HudMenu.Scaled* fonts are proportional (640x480 units) and
//     the box is in HUD units.
//=================================================================//

#include <string.h>
#include <algorithm>
#include <wchar.h>

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IPanel.h>
#include <tier2/tier2.h>

#include "client_vgui.h"
#include "sbo_bridge.h"
#include "sbo_hud_menu.h"
#include "sbo_menus.h"
#include "vgui2_api.h"
#include "sbo_gm_ui.h"
#include "../../game_shared/gm_catalog.h"

namespace
{
constexpr float SELECT_HOLD_TIME = 0.3f;  // the chosen item stays lit this long...
constexpr float SELECT_FADE_TIME = 0.25f; // ...then the menu fades out
constexpr int BOX_X = 20;                 // Source: x = 20, border = 20 (HUD units with hud_scale)
constexpr int BOX_BORDER = 20;
// Without hud_scale: the fonts in pixels. SBO Source's menu fonts are 15 px; BOX_X / BOX_BORDER grow with the font
// height against that, so the box keeps Source's proportions around the text.
constexpr int SOURCE_FONT_TALL = 15;
#ifdef _WIN32
constexpr const char PIXEL_FONT_FACE[] = "Arial";
#else
constexpr const char PIXEL_FONT_FACE[] = "Liberation Sans";
#endif
constexpr int PIXEL_TEXT_WEIGHT = 600;    // text and item lines
constexpr int PIXEL_PULSING_WEIGHT = 800; // the chosen item

// Without hud_scale: the font height for a screen of screenTall lines. 19 px up to 1199 lines (1680x1050,
// 1920x1080), above that about the share of the screen 19 px has at 1080 lines, in steps.
int PixelFontTall(int screenTall)
{
	if (screenTall < 1200)
		return 19;
	if (screenTall < 1440)
		return 21;
	if (screenTall < 1800)
		return 25;
	if (screenTall < 2160)
		return 32;
	if (screenTall < 2880)
		return 38;
	return 51;
}

// Without hud_scale: an antialiased PIXEL_FONT_FACE font, made once per height and weight (the engine has no call to
// free a font, so they are kept)
vgui2::HFont GetPixelFont(int tall, int weight)
{
	struct CachedFont
	{
		int tall;
		int weight;
		vgui2::HFont font;
	};
	static CachedFont s_Fonts[16];
	static int s_nFonts = 0;
	for (int i = 0; i < s_nFonts; i++)
	{
		if (s_Fonts[i].tall == tall && s_Fonts[i].weight == weight)
			return s_Fonts[i].font;
	}

	vgui2::HFont font = vgui2::surface()->CreateFont();
	if (font == vgui2::INVALID_FONT
	    || !vgui2::surface()->AddGlyphSetToFont(font, PIXEL_FONT_FACE, tall, weight, 0, 0,
	                                            vgui2::ISurface::FONTFLAG_ANTIALIAS, 0, 0))
		return vgui2::INVALID_FONT;

	if (s_nFonts < (int)(sizeof(s_Fonts) / sizeof(s_Fonts[0])))
		s_Fonts[s_nFonts++] = { tall, weight, font };
	return font;
}
} // namespace

CSboHudMenu *CSboHudMenu::s_pInstance = nullptr;

CSboHudMenu *CSboHudMenu::Create(vgui2::Panel *pParent)
{
	if (!s_pInstance)
		s_pInstance = new CSboHudMenu(pParent);
	return s_pInstance;
}

CSboHudMenu::CSboHudMenu(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboHudMenu")
{
	s_pInstance = this;

	// keyboard only: the number keys stay with CHudMenu (slot commands -> menuselect)
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetPaintBackgroundEnabled(true);
	SetProportional(false); // fonts are chosen in ApplySchemeSettings (hud_scale)

	if (sboui::GetScheme())
		SetScheme(sboui::GetScheme());

	SetBounds(0, 0, 640, 480); // OnThink: the parent's (screen) size
	SetVisible(true);
}

void CSboHudMenu::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBgColor(Color(0, 0, 0, 0)); // the box is drawn in PaintBackground

	m_bProportional = sbo::IsHudScaled();

	if (m_bProportional)
	{
		// hud_scale: the scheme's proportional fonts (640x480 units)
		m_hTextFont = pScheme->GetFont("HudMenu.ScaledTextFont", true);
		m_hItemFont = pScheme->GetFont("HudMenu.ScaledItemFont", true);
		m_hItemFontPulsing = pScheme->GetFont("HudMenu.ScaledItemFontPulsing", true);
	}
	else
	{
		// pixel fonts for the height of the screen the menu covers (the parent; this panel follows it in OnThink)
		int parentWide, parentTall;
		g_pVGuiPanel->GetSize(GetVParent(), parentWide, parentTall);
		m_iPixelScreenTall = parentTall;
		m_iPixelFontTall = PixelFontTall(parentTall);
		m_hTextFont = GetPixelFont(m_iPixelFontTall, PIXEL_TEXT_WEIGHT);
		m_hItemFontPulsing = GetPixelFont(m_iPixelFontTall, PIXEL_PULSING_WEIGHT);
		// the face could not be made: the scheme's fonts at their 640x480 size in pixels
		if (m_hTextFont == vgui2::INVALID_FONT)
			m_hTextFont = pScheme->GetFont("HudMenu.ScaledTextFont", false);
		if (m_hItemFontPulsing == vgui2::INVALID_FONT)
			m_hItemFontPulsing = pScheme->GetFont("HudMenu.ScaledItemFontPulsing", false);
		m_hItemFont = m_hTextFont;
	}

	m_TextColor = pScheme->GetColor("HudMenu.TextColor", Color(47, 145, 60, 255));
	m_ItemColor = pScheme->GetColor("HudMenu.ItemColor", Color(47, 145, 60, 255));
	// the box: a nearly opaque dark green (Source's MenuBoxBg was 47 145 60 at 15) so the lines read over a bright scene
	m_BoxColor = pScheme->GetColor("HudMenu.BoxColor", Color(8, 24, 10, 230));
	m_SelectedColor = pScheme->GetColor("HudMenu.SelectedColor", Color(150, 255, 160, 255));
	m_SelectedBgColor = pScheme->GetColor("HudMenu.SelectedBgColor", Color(47, 145, 60, 70));
	m_YellowColor = pScheme->GetColor("HudMenu.YellowColor", Color(255, 210, 64, 255));
	m_RedColor = pScheme->GetColor("HudMenu.RedColor", Color(210, 24, 0, 255));
	m_DimColor = pScheme->GetColor("HudMenu.DimColor", Color(100, 100, 100, 255));

	Layout();
}

//-----------------------------------------------------------------------------
// Menu state (driven by CHudMenu)
//-----------------------------------------------------------------------------
void CSboHudMenu::Open(const char *menuString)
{
	Parse(menuString ? menuString : "");
	Layout();
	m_bOpen = m_iNumLines > 0;
	m_iSelected = 0;
}

void CSboHudMenu::Close()
{
	m_bOpen = false;
	m_iSelected = 0;
}

void CSboHudMenu::Select(int item)
{
	if (!m_bOpen || m_iSelected)
		return;

	m_iSelected = item;
	m_flSelectTime = sbo::GetHudTime();
}

void CSboHudMenu::OnThink()
{
	BaseClass::OnThink();

	// cover the parent (the full-screen client viewport)
	int wide, tall, parentWide, parentTall;
	GetSize(wide, tall);
	g_pVGuiPanel->GetSize(GetVParent(), parentWide, parentTall);
	if (wide != parentWide || tall != parentTall)
		SetBounds(0, 0, parentWide, parentTall);

	// hud_scale switched, or without it the screen height: fonts for the new mode / height
	if (sbo::IsHudScaled() != m_bProportional || (!m_bProportional && parentTall != m_iPixelScreenTall))
		InvalidateLayout(false, true);

	if (m_bOpen && m_iSelected)
	{
		float t = sbo::GetHudTime() - m_flSelectTime;
		if (t < 0 || t >= SELECT_HOLD_TIME + SELECT_FADE_TIME)
			Close();
	}
}

//-----------------------------------------------------------------------------
// Text
//-----------------------------------------------------------------------------
void CSboHudMenu::Parse(const char *menuString)
{
	wchar_t wide[MAX_CHARS];
	sboui::TextToWide(menuString, wide, sizeof(wide));

	m_iNumSegments = 0;
	m_iNumLines = 0;
	int numChars = 0;

	TextColor color = COLOR_DEFAULT; // like the engine-text menu, a colour carries over to the next lines
	bool right = false;

	m_Lines[0] = Line();
	m_iNumLines = 1;

	const wchar_t *p = wide;
	while (*p)
	{
		if (*p == L'\\')
		{
			// HL25 ParseEscapeToken: backslash + one character, unknown codes are swallowed
			switch (p[1])
			{
			case L'\0': p++; continue;
			case L'w': color = COLOR_DEFAULT; break;
			case L'd': color = COLOR_DIM; break;
			case L'y': color = COLOR_YELLOW; break;
			case L'r': color = COLOR_RED; break;
			case L'R': right = true; break;
			}
			p += 2;
			continue;
		}

		if (*p == L'\n')
		{
			p++;
			right = false;
			if (m_iNumLines >= MAX_LINES)
				break;
			m_Lines[m_iNumLines] = Line();
			m_Lines[m_iNumLines].firstSeg = m_iNumSegments;
			m_iNumLines++;
			continue;
		}

		// a run of plain text
		const wchar_t *start = p;
		while (*p && *p != L'\\' && *p != L'\n')
			p++;

		int len = (int)(p - start);
		if (len > MAX_CHARS - 1 - numChars)
			len = MAX_CHARS - 1 - numChars;
		if (len <= 0 || m_iNumSegments >= MAX_SEGMENTS)
			continue;

		Segment &seg = m_Segments[m_iNumSegments++];
		seg.start = numChars;
		seg.len = len;
		seg.color = color;
		seg.right = right;
		memcpy(&m_wszText[numChars], start, len * sizeof(wchar_t));
		numChars += len;
		m_Lines[m_iNumLines - 1].numSegs++;
	}
	m_wszText[numChars] = 0;

	for (int i = 0; i < m_iNumLines; i++)
	{
		Line &line = m_Lines[i];

		// blank = nothing but spaces
		line.blank = true;
		for (int s = line.firstSeg; s < line.firstSeg + line.numSegs && line.blank; s++)
		{
			for (int c = 0; c < m_Segments[s].len; c++)
			{
				if (m_wszText[m_Segments[s].start + c] != L' ')
				{
					line.blank = false;
					break;
				}
			}
		}

		// item line: "<digit>." at its start (spaces allowed before), 0 = item 10
		line.item = 0;
		if (!line.blank)
		{
			int c = m_Segments[line.firstSeg].start;
			int end = m_Segments[line.firstSeg + line.numSegs - 1].start + m_Segments[line.firstSeg + line.numSegs - 1].len;
			while (c < end && m_wszText[c] == L' ')
				c++;
			if (c + 1 < end && m_wszText[c] >= L'0' && m_wszText[c] <= L'9' && m_wszText[c + 1] == L'.')
				line.item = m_wszText[c] == L'0' ? 10 : m_wszText[c] - L'0';
		}
	}

	// drop trailing blank lines (menu strings usually end with "\n")
	while (m_iNumLines > 0 && m_Lines[m_iNumLines - 1].blank)
		m_iNumLines--;
}

vgui2::HFont CSboHudMenu::GetLineFont(const Line &line) const
{
	return line.item ? m_hItemFont : m_hTextFont;
}

int CSboHudMenu::GetTextWide(vgui2::HFont font, const wchar_t *str, int len) const
{
	int w = 0;
	for (int i = 0; i < len; i++)
		w += vgui2::surface()->GetCharacterWidth(font, str[i]);
	return w;
}

void CSboHudMenu::Layout()
{
	m_iContentWide = 0;
	m_iContentTall = 0;

	if (m_hTextFont == vgui2::INVALID_FONT)
		return; // scheme not applied yet

	for (int i = 0; i < m_iNumLines; i++)
	{
		Line &line = m_Lines[i];
		vgui2::HFont font = GetLineFont(line);
		int fontTall = vgui2::surface()->GetFontTall(font);

		line.leftWide = line.rightWide = 0;
		for (int s = line.firstSeg; s < line.firstSeg + line.numSegs; s++)
		{
			const Segment &seg = m_Segments[s];
			int w = GetTextWide(font, &m_wszText[seg.start], seg.len);
			(seg.right ? line.rightWide : line.leftWide) += w;
		}

		// the pulsing (bolder) font of a chosen item must fit too
		if (line.item)
		{
			int pulsing = 0;
			for (int s = line.firstSeg; s < line.firstSeg + line.numSegs; s++)
				pulsing += GetTextWide(m_hItemFontPulsing, &m_wszText[m_Segments[s].start], m_Segments[s].len);
			int w = line.leftWide + line.rightWide;
			if (pulsing > w)
				line.leftWide += pulsing - w;
		}

		int wide = line.leftWide;
		if (line.rightWide)
			wide += fontTall + line.rightWide; // a gap between the parts
		if (wide > m_iContentWide)
			m_iContentWide = wide;

		line.tall = line.blank ? fontTall / 2 : fontTall;
		m_iContentTall += line.tall;
	}
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------
bool CSboHudMenu::ShouldPaint() const
{
	return m_bOpen && m_iNumLines > 0 && sbo::HudMenuShouldDraw();
}

float CSboHudMenu::GetAlpha() const
{
	if (!m_iSelected)
		return 1.0f;

	float t = sbo::GetHudTime() - m_flSelectTime - SELECT_HOLD_TIME;
	if (t <= 0)
		return 1.0f;
	float a = 1.0f - t / SELECT_FADE_TIME;
	return a < 0 ? 0.0f : a;
}

void CSboHudMenu::GetBoxBounds(int &x, int &y, int &wide, int &tall, int &border)
{
	if (m_bProportional)
	{
		// hud_scale: HUD units
		border = (int)(BOX_BORDER * sbo::GetHudScale() + 0.5f);
		x = sbo::HudToScreenX(BOX_X) - border / 2;
	}
	else
	{
		// pixels, grown with the font against Source's 15 px
		float scale = (float)m_iPixelFontTall / SOURCE_FONT_TALL;
		border = (int)(BOX_BORDER * scale + 0.5f);
		x = (int)(BOX_X * scale + 0.5f) - border / 2;
	}

	// Source: content at x 20, box border/2 around it, vertically centred
	wide = m_iContentWide + border;
	tall = m_iContentTall + border;
	y = (GetTall() - tall) / 2;
}

Color CSboHudMenu::GetSegmentColor(const Line &line, const Segment &seg) const
{
	switch (seg.color)
	{
	case COLOR_DIM: return m_DimColor;
	case COLOR_YELLOW: return m_YellowColor;
	case COLOR_RED: return m_RedColor;
	default: return line.item ? m_ItemColor : m_TextColor;
	}
}

// SBO 2.0 GM mode: a Game Master's text menus in the GM screens' look (board 1 "GM QUICK MENU", board 4
// "ORDERS"): no box, at 16 / 220 (design px), the first line a gold title, the lines 15 px bright,
// \d dim, a blank line a 6 px gap, 4 px between the lines
static bool GmStyle()
{
	return gmui::GetState() == GM_STATE_ACTIVE;
}

void CSboHudMenu::PaintBackground()
{
	if (GmStyle())
		return;
	// box and highlight bar here, text in Paint(): text rendering disables alpha blending afterwards (BHL kill feed)
	if (!ShouldPaint())
		return;

	int x, y, wide, tall, border;
	GetBoxBounds(x, y, wide, tall, border);
	float alpha = GetAlpha();

	DrawBox(x, y, wide, tall, m_BoxColor, alpha);

	if (m_iSelected)
	{
		int lineY = y + border / 2;
		for (int i = 0; i < m_iNumLines; i++)
		{
			const Line &line = m_Lines[i];
			if (line.item == m_iSelected)
			{
				Color c = m_SelectedBgColor;
				c[3] = (unsigned char)(c[3] * alpha);
				vgui2::surface()->DrawSetColor(c);
				vgui2::surface()->DrawFilledRect(x + border / 4, lineY, x + wide - border / 4, lineY + line.tall);
				break;
			}
			lineY += line.tall;
		}
	}
}

void CSboHudMenu::Paint()
{
	if (!ShouldPaint())
		return;

	if (GmStyle())
	{
		const float alpha = GetAlpha();
		// the design's metrics: the title's middle at 225 (of 720), the first line's at 252, a line every 18, a blank
		// line 10 more
		vgui2::HFont title = gmui::Font(13), item = gmui::Font(15);
		int cy = gmui::S(225);
		const int x0 = gmui::S(16);
		bool first = true, afterTitle = false;
		for (int i = 0; i < m_iNumLines; i++)
		{
			const Line &line = m_Lines[i];
			if (line.blank)
			{
				cy += gmui::S(10);
				continue;
			}
			const bool isTitle = first;
			if (!isTitle)
				// a line every 18 (the first 27 under the title with its blank line); never closer than the font
				cy += std::max(gmui::S(afterTitle ? 17 : 18), gmui::FontTall(item) + gmui::S(2));
			afterTitle = isTitle;
			first = false;
			const bool selected = m_iSelected && line.item == m_iSelected;
			vgui2::HFont font = isTitle ? title : item;
			const int y = cy - gmui::FontTall(font) / 2;
			int x = x0;
			for (int sgi = line.firstSeg; sgi < line.firstSeg + line.numSegs; sgi++)
			{
				const Segment &seg = m_Segments[sgi];
				Color c = isTitle ? gmui::ACCENT : (seg.color == COLOR_DIM ? gmui::DIM : gmui::BRIGHT);
				if (selected)
					c = gmui::GREEN_LIGHT;
				c[3] = (unsigned char)(c[3] * alpha);
				std::wstring text(&m_wszText[seg.start], seg.len);
				x += gmui::Text(font, x, y, c, text.c_str());
			}
		}
		return;
	}

	int boxX, boxY, boxWide, boxTall, border;
	GetBoxBounds(boxX, boxY, boxWide, boxTall, border);
	float alpha = GetAlpha();

	int contentX = boxX + border / 2;
	int y = boxY + border / 2;

	for (int i = 0; i < m_iNumLines; i++)
	{
		const Line &line = m_Lines[i];
		bool selected = m_iSelected && line.item == m_iSelected;
		vgui2::HFont font = selected ? m_hItemFontPulsing : GetLineFont(line);

		vgui2::surface()->DrawSetTextFont(font);

		int x = contentX;
		int rightX = contentX + m_iContentWide - line.rightWide;

		for (int s = line.firstSeg; s < line.firstSeg + line.numSegs; s++)
		{
			const Segment &seg = m_Segments[s];
			Color c = selected ? m_SelectedColor : GetSegmentColor(line, seg);
			c[3] = (unsigned char)(c[3] * alpha);
			vgui2::surface()->DrawSetTextColor(c);

			int &px = seg.right ? rightX : x;
			for (int ch = 0; ch < seg.len; ch++)
			{
				wchar_t wc = m_wszText[seg.start + ch];
				vgui2::surface()->DrawSetTextPos(px, y);
				vgui2::surface()->DrawUnicodeChar(wc);
				px += vgui2::surface()->GetCharacterWidth(font, wc);
			}
		}

		y += line.tall;
	}
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
void VGui2_HudMenu_Open(const char *menuString)
{
	if (CSboHudMenu::Get())
		CSboHudMenu::Get()->Open(menuString);
}

void VGui2_HudMenu_Close()
{
	if (CSboHudMenu::Get())
		CSboHudMenu::Get()->Close();
}

void VGui2_HudMenu_Select(int item)
{
	if (CSboHudMenu::Get())
		CSboHudMenu::Get()->Select(item);
}
