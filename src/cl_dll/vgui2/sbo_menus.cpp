//========= SBO VGUI2 port: menu infrastructure =========//
//
// Purpose: see sbo_menus.h. Also the vgui2_api.h entry point VGui2_Menus_Init.
//=================================================================//

#include <stdio.h>
#include <string.h>

#include <tier2/tier2.h>
#include <FileSystem.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_brief_menu.h"
#include "sbo_gm_menu.h"
#include "sbo_html_panel.h"
#include "sbo_hud_menu.h"
#include "sbo_menus.h"
#include "sbo_spectator_panel.h"
#include "sbo_team_menu.h"
#include "sbo_text_window.h"
#include "vgui2_api.h"

namespace
{
bool s_bInitialized = false;
vgui2::HScheme s_hScheme = 0;

// VGUI1 text scheme fonts made with the VGUI2 surface (fonts can't be destroyed, so they are kept for the session)
struct CachedFont
{
	char name[48];
	int tall;
	int weight;
	vgui2::HFont font;
};
CachedFont s_Fonts[32];
int s_iNumFonts = 0;

// Length of the valid UTF-8 sequence at p (1..4), 0 if p doesn't start one.
int Utf8SequenceLength(const unsigned char *p)
{
	int len;
	if (p[0] < 0x80)
		return 1;
	else if ((p[0] & 0xE0) == 0xC0 && p[0] >= 0xC2)
		len = 2;
	else if ((p[0] & 0xF0) == 0xE0)
		len = 3;
	else if ((p[0] & 0xF8) == 0xF0 && p[0] <= 0xF4)
		len = 4;
	else
		return 0;

	for (int i = 1; i < len; i++)
	{
		if ((p[i] & 0xC0) != 0x80)
			return 0;
	}
	return len;
}

bool IsValidUtf8(const unsigned char *p)
{
	while (*p)
	{
		int len = Utf8SequenceLength(p);
		if (!len)
			return false;
		p += len;
	}
	return true;
}
} // namespace

namespace sboui
{
int XRes(float x)
{
	int wide, tall;
	sbo::GetScreenSize(wide, tall);
	return (int)(x * ((float)wide / 640.0f) + 0.5f);
}

int YRes(float y)
{
	int wide, tall;
	sbo::GetScreenSize(wide, tall);
	return (int)(y * ((float)tall / 480.0f) + 0.5f);
}

#ifndef _WIN32
// Linux (docs/dev/LINUX_CLIENT.md "Fonts"): the text scheme files (<width>_textscheme.txt) have no platform conditionals
// like the .res schemes' [!$WINDOWS], so the Windows faces they name are mapped here to the free fonts the engine
// and the mod carry (resource/fonts, registered by the scheme files' CustomFontFiles): no text depends on the
// system's fonts. "Arial Bold" and "Arial Narrow Bold" are bold by their name on Windows; here the bold member of
// the family is asked for through the weight (there is no narrow one: "Arial Narrow Bold" becomes Liberation Sans
// Bold). Other names go to the system as they are.
static const char *LinuxFontName(const char *name, int &weight)
{
	if (!stricmp(name, "Arial"))
		return "Liberation Sans";
	if (!stricmp(name, "Arial Bold") || !stricmp(name, "Arial Narrow Bold"))
	{
		if (weight < 700)
			weight = 700;
		return "Liberation Sans";
	}
	if (!stricmp(name, "Lucida Console"))
		return "DejaVu Sans Mono";
	return name;
}
#endif

vgui2::HFont GetTextSchemeFont(const char *schemeName)
{
	sbo::TextSchemeInfo info;
	if (!sbo::GetTextSchemeInfo(schemeName, info) || !info.fontName[0] || info.fontSize <= 0)
		return vgui2::INVALID_FONT;

	for (int i = 0; i < s_iNumFonts; i++)
	{
		const CachedFont &f = s_Fonts[i];
		if (f.tall == info.fontSize && f.weight == info.fontWeight && !stricmp(f.name, info.fontName))
			return f.font;
	}

	// VGUI1 (vgui.dll) builds its fonts with GDI from the same name/height/weight. No antialiasing, like VGUI1's.
	vgui2::HFont font = vgui2::surface()->CreateFont();
#ifdef _WIN32
	vgui2::surface()->AddGlyphSetToFont(font, info.fontName, info.fontSize, info.fontWeight, 0, 0, 0, 0, 0);
#else
	int weight = info.fontWeight;
	const char *fontName = LinuxFontName(info.fontName, weight);
	vgui2::surface()->AddGlyphSetToFont(font, fontName, info.fontSize, weight, 0, 0, 0, 0, 0);
#endif

	if (s_iNumFonts < (int)(sizeof(s_Fonts) / sizeof(s_Fonts[0])))
	{
		CachedFont &f = s_Fonts[s_iNumFonts++];
		strncpy(f.name, info.fontName, sizeof(f.name) - 1);
		f.name[sizeof(f.name) - 1] = 0;
		f.tall = info.fontSize;
		f.weight = info.fontWeight;
		f.font = font;
	}

	return font;
}

bool GetTextSchemeFgColor(const char *schemeName, Color &out)
{
	sbo::TextSchemeInfo info;
	if (!sbo::GetTextSchemeInfo(schemeName, info))
		return false;

	out = Color(info.fg[0], info.fg[1], info.fg[2], info.fg[3]);
	return true;
}

vgui2::HScheme GetScheme()
{
	return s_hScheme;
}

void TextToWide(const char *text, wchar_t *out, int outSizeInBytes)
{
	const int maxChars = outSizeInBytes / (int)sizeof(wchar_t);
	if (maxChars <= 0)
		return;

	const unsigned char *p = (const unsigned char *)(text ? text : "");
	const bool bUtf8 = IsValidUtf8(p);
	int n = 0;

	while (*p && n < maxChars - 1)
	{
		unsigned int ch;
		if (bUtf8)
		{
			int len = Utf8SequenceLength(p);
			if (len == 1)
				ch = p[0];
			else if (len == 2)
				ch = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
			else if (len == 3)
				ch = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
			else
				ch = '?'; // outside the BMP: wchar_t is 16 bits on Windows
			p += len;
		}
		else
			ch = *p++; // Latin-1

		if (ch == '\r')
		{
			// "\r\n" -> "\n", lone "\r" -> "\n"
			if (*p == '\n')
				continue;
			ch = '\n';
		}

		out[n++] = (wchar_t)ch;
	}

	out[n] = 0;
}

void Localize(const char *token, wchar_t *out, int outSizeInBytes)
{
	const int maxChars = outSizeInBytes / (int)sizeof(wchar_t);
	if (maxChars <= 0)
		return;

	const wchar_t *str = g_pVGuiLocalize ? g_pVGuiLocalize->Find(token) : nullptr;
	if (!str)
	{
		TextToWide(token[0] == '#' ? token + 1 : token, out, outSizeInBytes);
		return;
	}

	// the localization reader may leave "\n" as two characters
	int n = 0;
	for (; *str && n < maxChars - 1; str++)
	{
		if (str[0] == L'\\' && str[1] == L'n')
		{
			out[n++] = L'\n';
			str++;
		}
		else
			out[n++] = *str;
	}
	out[n] = 0;
}
} // namespace sboui

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
bool VGui2_Menus_Init()
{
	if (s_bInitialized)
		return true;

	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;

	s_bInitialized = true;

	SboHtml_Init(); // cl_disablehtmlmotd (docs/HTML_MOTD.md)

	g_pVGuiLocalize->AddFile(g_pFullFileSystem, VGUI2_ROOT_DIR "resource/language/sbo_ui_%language%.txt");
	s_hScheme = vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/sboscheme.res", "SboScheme");

	// same order as before (popups created later are in front of the earlier ones until one is moved to the front)
	CSboTextWindow::Create(g_pViewport);
	CSboGmMenu::Create(g_pViewport);
	CSboTeamMenu::Create(g_pViewport);
	CSboBriefMenu::Create(g_pViewport);
	CSboSpectatorPanel::Create(g_pViewport);
	CSboHudMenu::Create(g_pViewport);
	return true;
}
