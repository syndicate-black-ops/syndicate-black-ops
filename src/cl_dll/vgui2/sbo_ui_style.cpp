//========= SBO VGUI2: the server browser's look, shared by SBO's GameUI windows =========//
//
// Purpose: see sbo_ui_style.h.
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <FileSystem.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "client_vgui.h" // VGUI2_ROOT_DIR
#include "hud_chat.h"    // GetColorCodeLength
#include "sbo_server_browser.h" // SboServerRow::Mode
#include "sbo_ui_style.h"
#include "sbo_widgets.h"
#include "sbo_bridge.h"
#include "sbo_cvar.h"
#include "sbo_script_file.h" // sbo_script::ConfigValue

#include <IEngineVGui.h>
#include <KeyValues.h>
#include <vgui/IPanel.h>
#include <vgui/IVGui.h>

#ifdef PostMessage
#undef PostMessage
#endif

#ifdef CreateFont
#undef CreateFont
#endif

namespace sbolook
{
#ifdef _WIN32
constexpr const char FONT_FACE[] = "Tahoma"; // the design's font
#else
constexpr const char FONT_FACE[] = "Liberation Sans";
#endif

static Style s_Style;
static float s_flStyleScale = -1.0f;

vgui2::HScheme BrowserScheme()
{
	static vgui2::HScheme s_hScheme = 0;
	if (!s_hScheme)
		s_hScheme = vgui2::scheme()->LoadSchemeFromFile(VGUI2_ROOT_DIR "resource/serverbrowserscheme.res", "SboServerBrowserScheme");
	return s_hScheme;
}

const char *SboUiFontFace()
{
	return FONT_FACE;
}

vgui2::HFont MakeFont(int tall, int weight)
{
	return MakeFontFace(FONT_FACE, tall, weight);
}

vgui2::HFont MakeFontFace(const char *face, int tall, int weight)
{
	struct Cached
	{
		const char *face;
		int tall, weight;
		vgui2::HFont font;
	};
	static Cached s_Fonts[128];
	static int s_nFonts = 0;
	for (int i = 0; i < s_nFonts; i++)
	{
		if (s_Fonts[i].tall == tall && s_Fonts[i].weight == weight && !strcmp(s_Fonts[i].face, face))
			return s_Fonts[i].font;
	}
	// The engine has no call to free a font: they are kept, one per size (as sbo_hud_menu.cpp)
	vgui2::HFont font = vgui2::surface()->CreateFont();
	if (font == vgui2::INVALID_FONT)
		return font;
	vgui2::surface()->AddGlyphSetToFont(font, face, tall, weight, 0, 0, vgui2::ISurface::FONTFLAG_ANTIALIAS, 0, 0);
	if (s_nFonts < (int)(sizeof(s_Fonts) / sizeof(s_Fonts[0])))
		s_Fonts[s_nFonts++] = { face, tall, weight, font }; // faces are string literals
	return font;
}

void AddBarlowFonts()
{
	// Barlow Condensed SemiBold (SIL Open Font License, resource/fonts/ofl-barlow.txt), the loadout design's title
	// face ("Barlow Condensed SemiBold" is the file's own family name on every system)
	static bool s_bDone = false;
	if (s_bDone)
		return;
	s_bDone = true;
	vgui2::surface()->AddCustomFontFile("resource/fonts/barlowcondensed-semibold.ttf");
}

//! The scale for this screen: the design's window in the same share of the screen height (through the scale
//! helper, 480 base lines = 800 design lines), narrower screens by their width.
void UpdateStyle(vgui2::Panel *pProportional)
{
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	int baseW, baseH;
	vgui2::VGui_GetProportionalBase(baseW, baseH);
	float scale = sboui::GetProportionalScale(pProportional) * (float)baseH / (float)MOCK_SCREEN_TALL;
	const float byWidth = (float)sw / (float)MOCK_SCREEN_WIDE;
	if (byWidth < scale)
		scale = byWidth;
	if (scale < 0.5f)
		scale = 0.5f;
	if (scale == s_flStyleScale)
		return;
	s_flStyleScale = scale;
	SetStyleScale(scale);
}

//! The look at this scale (fonts made once per size)
void SetStyleScale(float scale)
{
	if (scale < 0.5f)
		scale = 0.5f;
	s_Style.scale = scale;
	// legible at every resolution: no font below a minimum pixel height on small screens (1024 x
	// 768 and the like scale the windows down; the text stops shrinking there and the rows, measured from the fonts,
	// make room). A font's tall is its line height: 15 = text of about 12 pixels.
	auto font = [](int tall, int minTall) { return MakeFont(std::max(tall, minTall), 400); };
	s_Style.title = font(s_Style.M(30), 22);
	s_Style.tab = font(s_Style.M(14), 15);
	s_Style.body = font(s_Style.M(13), 15);
	s_Style.small = font(s_Style.M(12), 14);
	s_Style.row = font(s_Style.M(14), 15);
	s_Style.name = font(s_Style.M(16), 16);
	s_Style.button = font(s_Style.M(15), 15);
	s_Style.close = font(s_Style.M(18), 16);
}

const Style &S()
{
	return s_Style;
}

void ResetStyle(vgui2::Panel *pProportional)
{
	s_flStyleScale = -1.0f;
	UpdateStyle(pProportional);
}

//-----------------------------------------------------------------------------
// Drawing helpers (inside the panel being painted)
//-----------------------------------------------------------------------------
void FillRect(int x, int y, int w, int h, Color c)
{
	vgui2::surface()->DrawSetColor(c);
	vgui2::surface()->DrawFilledRect(x, y, x + w, y + h);
}

void OutlineRect(int x, int y, int w, int h, Color c)
{
	vgui2::surface()->DrawSetColor(c);
	vgui2::surface()->DrawOutlinedRect(x, y, x + w, y + h);
}

std::wstring Widen(const char *utf8)
{
	wchar_t buf[512];
	Q_UTF8ToUnicode(utf8 ? utf8 : "", buf, sizeof(buf));
	return buf;
}

int TextWide(vgui2::HFont font, const std::wstring &text)
{
	int w = 0, h = 0;
	if (!text.empty())
		vgui2::surface()->GetTextSize(font, text.c_str(), w, h);
	return w;
}

//! Draws text with its top at y; cut with "..." to maxWide (0 = no limit). Returns the drawn width.
int DrawText(vgui2::HFont font, Color color, int x, int y, const std::wstring &text, int maxWide)
{
	std::wstring s = text;
	if (maxWide > 0 && TextWide(font, s) > maxWide)
	{
		const std::wstring dots = L"...";
		while (!s.empty() && TextWide(font, s + dots) > maxWide)
			s.pop_back();
		s += dots;
	}
	if (s.empty())
		return 0;
	vgui2::surface()->DrawSetTextFont(font);
	vgui2::surface()->DrawSetTextColor(color);
	vgui2::surface()->DrawSetTextPos(x, y);
	vgui2::surface()->DrawPrintText(s.c_str(), (int)s.size());
	return TextWide(font, s);
}

int FontTall(vgui2::HFont font)
{
	return vgui2::surface()->GetFontTall(font);
}

//! Text vertically centered in [y, y + h)
int DrawTextMid(vgui2::HFont font, Color color, int x, int y, int h, const std::wstring &text, int maxWide)
{
	return DrawText(font, color, x, y + (h - FontTall(font)) / 2, text, maxWide);
}

//! The design's column headers are upper case with 0.04 em letter spacing: drawn per character.
int DrawSpacedUpper(vgui2::HFont font, Color color, int x, int y, const char *text)
{
	const int spacing = (int)(FontTall(font) * 0.04f + 0.5f);
	int cx = x;
	vgui2::surface()->DrawSetTextFont(font);
	vgui2::surface()->DrawSetTextColor(color);
	for (const char *c = text; *c; c++)
	{
		wchar_t ch = (wchar_t)toupper((unsigned char)*c);
		vgui2::surface()->DrawSetTextPos(cx, y);
		vgui2::surface()->DrawUnicodeChar(ch);
		cx += vgui2::surface()->GetCharacterWidth(font, ch) + spacing;
	}
	return cx - x;
}

int SpacedUpperWide(vgui2::HFont font, const char *text)
{
	const int spacing = (int)(FontTall(font) * 0.04f + 0.5f);
	int w = 0;
	for (const char *c = text; *c; c++)
		w += vgui2::surface()->GetCharacterWidth(font, (wchar_t)toupper((unsigned char)*c)) + spacing;
	return w;
}

//! A filled triangle pointing up or down, drawn with rows of rectangles (no symbol font; docs/dev/LINUX_CLIENT.md)
void DrawTriangle(int cx, int cy, int size, bool up, Color c)
{
	vgui2::surface()->DrawSetColor(c);
	const int rows = size / 2 + 1;
	for (int i = 0; i < rows; i++)
	{
		const int half = up ? i : rows - 1 - i;
		const int y = cy - rows / 2 + i;
		vgui2::surface()->DrawFilledRect(cx - half, y, cx + half + 1, y + 1);
	}
}

//! Small padlock (the lock column), w wide
void DrawPadlock(int x, int y, int w, Color c)
{
	const int bodyH = w * 3 / 4, shackleH = w / 2, t = w / 6 > 1 ? w / 6 : 1;
	vgui2::surface()->DrawSetColor(c);
	// shackle
	vgui2::surface()->DrawFilledRect(x + w / 5, y, x + w - w / 5, y + t);
	vgui2::surface()->DrawFilledRect(x + w / 5, y, x + w / 5 + t, y + shackleH);
	vgui2::surface()->DrawFilledRect(x + w - w / 5 - t, y, x + w - w / 5, y + shackleH);
	// body
	vgui2::surface()->DrawFilledRect(x, y + shackleH, x + w, y + shackleH + bodyH);
}

//! Color codes (^1, ^xRGB, ^#RRGGBB) and control characters out of a server or player name
std::string CleanName(const char *s)
{
	std::string out;
	if (!s)
		return out;
	while (*s)
	{
		const int code = GetColorCodeLength(s);
		if (code > 0)
		{
			s += code;
			continue;
		}
		if ((unsigned char)*s >= 32)
			out += *s;
		s++;
	}
	return out;
}

int ModeOfMap(const char *map)
{
	// gamerules.cpp InstallGameRules
	if (!strncmp(map, "co_", 3))
		return SboServerRow::MODE_COOP;
	if (!strncmp(map, "br_", 3))
		return SboServerRow::MODE_BR;
	if (!strncmp(map, "sp_", 3))
		return SboServerRow::MODE_NONE;
	return SboServerRow::MODE_BOUNTY;
}

const char *ModeName(int mode)
{
	switch (mode)
	{
	case SboServerRow::MODE_COOP:
		return "Co-op";
	case SboServerRow::MODE_BOUNTY:
		return "Bounty Killer";
	case SboServerRow::MODE_BR:
		return "Battle Royale";
	default:
		return "-";
	}
}

Color PingColor(int ping)
{
	// the design: under 80 green, under 150 yellow, else orange
	if (ping < 80)
		return COL_TEXT_BRIGHT;
	if (ping < 150)
		return COL_PING_MID;
	return COL_PING_BAD;
}

//! A map name usable in a file path: letters, digits, '_', '-', '.', no "..", short
bool SafeMapName(const std::string &map)
{
	if (map.empty() || map.size() > 60 || map.find("..") != std::string::npos)
		return false;
	for (char c : map)
	{
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'
		      || c == '.'))
			return false;
	}
	return true;
}

std::string Lower(std::string s)
{
	for (char &c : s)
		c = (char)tolower((unsigned char)c);
	return s;
}

namespace
{
const Color C_COOL_FACE(11, 14, 11, 215); // the cooldown clock's drained part (COL_BG_INPUT)
const Color C_COOL_LEFT(46, 95, 38, 235); // the time left (COL_BG_PRIMARY)
} // namespace

// A cooldown clock (sbo_ui_style.h): a disk whose bright part is the time left (from 12 o'clock, clockwise; it drains as the hand
//! moves round), a rim, and the seconds left in its middle ("1:05" from a minute up). Drawn in horizontal runs.
void DrawCooldownClock(int cx, int cy, int radius, float left, float total, vgui2::HFont font)
{
	if (radius <= 0)
		return;
	const float frac = total > 0.0f ? std::max(0.0f, std::min(1.0f, left / total)) : 1.0f;
	// the hand: the part from 12 o'clock to it, clockwise, is drained; from the hand on to 12 o'clock is left
	const float hand = (1.0f - frac) * 2.0f * (float)M_PI;
	const float rOut = radius + 0.5f, rIn = radius - 1.5f;
	enum { NONE, FACE, LEFT, RIM };
	for (int dy = -radius; dy <= radius; dy++)
	{
		int runStart = -radius, runKind = NONE;
		for (int dx = -radius; dx <= radius + 1; dx++)
		{
			int kind = NONE;
			if (dx <= radius)
			{
				const float d = sqrtf((float)(dx * dx + dy * dy));
				if (d <= rOut)
				{
					if (d > rIn)
						kind = RIM;
					else
					{
						// clockwise from 12 o'clock (the screen's y grows downwards)
						float a = atan2f((float)dx, (float)-dy);
						if (a < 0.0f)
							a += 2.0f * (float)M_PI;
						kind = a >= hand ? LEFT : FACE;
					}
				}
			}
			if (kind == runKind)
				continue;
			if (runKind != NONE)
			{
				const Color c = runKind == RIM ? COL_BORDER_PRIMARY : (runKind == LEFT ? C_COOL_LEFT : C_COOL_FACE);
				FillRect(cx + runStart, cy + dy, dx - runStart, 1, c);
			}
			runKind = kind;
			runStart = dx;
		}
	}
	const int secs = (int)ceilf(left);
	wchar_t text[16];
	if (secs >= 60)
		swprintf(text, sizeof(text) / sizeof(text[0]), L"%d:%02d", secs / 60, secs % 60);
	else
		swprintf(text, sizeof(text) / sizeof(text[0]), L"%d", secs);
	const int tw = TextWide(font, text);
	DrawTextMid(font, COL_TEXT_PRIMARY, cx - tw / 2, cy - radius, 2 * radius + 1, text);
}

} // namespace sbolook

//-----------------------------------------------------------------------------
// cl_experimental_menus
//-----------------------------------------------------------------------------
// On for a new install; a player's saved 0 wins (SavedCvarValue, then config.cfg)
#define SBO_EXPERIMENTAL_MENUS_DEFAULT "1"

static CSboCvar cl_experimental_menus("cl_experimental_menus", SBO_EXPERIMENTAL_MENUS_DEFAULT);
// cl_new_ingame_menus (default 1): the new loadout menu, MOTD and map info windows
static CSboCvar cl_new_ingame_menus("cl_new_ingame_menus", "1");
static bool s_bMenusCvarRegistered = false;

// The player's saved value of an archived cvar: its line in config.cfg (the mod folder, where the engine writes it).
// HL25 runs config.cfg only after the game's start-up (CEngine::Load: Sys_InitGame, then ForceReloadProfile appends
// "exec config.cfg" to the command buffer), so a command line's +connect (valve.rc's stuffcmds, already in the buffer)
// runs first: GameUI's loading window opens and is painted inside that connect, while the cvars still have their
// defaults. Read here, the setting holds from the start.
static bool SavedCvarValue(const char *name, std::string &value)
{
	if (!g_pFullFileSystem)
		return false;
	FileHandle_t fh = g_pFullFileSystem->Open("config.cfg", "rb", "GAMECONFIG");
	if (!fh)
		return false;
	std::string text;
	const unsigned size = g_pFullFileSystem->Size(fh);
	if (size > 0 && size <= 1024 * 1024)
	{
		text.resize(size);
		if (g_pFullFileSystem->Read(&text[0], (int)size, fh) != (int)size)
			text.clear();
	}
	g_pFullFileSystem->Close(fh);
	return sbo_script::ConfigValue(text.data(), text.size(), name, value);
}

void VGui2_RegisterMenusCvar()
{
	if (s_bMenusCvarRegistered)
		return;
	s_bMenusCvarRegistered = true;
	cl_experimental_menus.Register();
	cl_new_ingame_menus.Register();
	// the saved settings at once (HUD_Init: before a command line's +connect runs; config.cfg sets the same later)
	for (const CSboCvar *cvar : { &cl_experimental_menus, &cl_new_ingame_menus })
	{
		std::string saved;
		if (SavedCvarValue(cvar->GetName(), saved))
			cvar->SetValue(saved.c_str());
	}
	SboVoiceGain_Restore();
}

//-----------------------------------------------------------------------------
// voice_maxgain / voice_avggain: the engine does not archive them (no FCVAR_ARCHIVE in HL25's hw.so), so the values
// set in Options > Voice (or the old Advanced dialog's Voice tab) are kept in voicegain.vdf and set again at start.
// Nothing is written while both are at HL25's defaults and no file exists.
//-----------------------------------------------------------------------------
static constexpr const char VOICEGAIN_FILE[] = "voicegain.vdf";

void SboVoiceGain_Save()
{
	sbo::CvarHandle hMax = sbo::FindCvar("voice_maxgain"), hAvg = sbo::FindCvar("voice_avggain");
	if (!hMax || !hAvg)
		return;
	const float maxGain = sbo::CvarGetFloat(hMax), avgGain = sbo::CvarGetFloat(hAvg);
	if (maxGain == SBO_VOICE_MAXGAIN_DEFAULT && avgGain == SBO_VOICE_AVGGAIN_DEFAULT &&
	    !g_pFullFileSystem->FileExists(VOICEGAIN_FILE))
		return;
	KeyValues *kv = new KeyValues("VoiceGain");
	kv->SetFloat("voice_maxgain", maxGain);
	kv->SetFloat("voice_avggain", avgGain);
	kv->SaveToFile(g_pFullFileSystem, VOICEGAIN_FILE, "GAMECONFIG");
	kv->deleteThis();
}

void SboVoiceGain_Restore()
{
	KeyValues *kv = new KeyValues("VoiceGain");
	if (kv->LoadFromFile(g_pFullFileSystem, VOICEGAIN_FILE, "GAMECONFIG"))
	{
		const float maxGain = kv->GetFloat("voice_maxgain", SBO_VOICE_MAXGAIN_DEFAULT);
		const float avgGain = kv->GetFloat("voice_avggain", SBO_VOICE_AVGGAIN_DEFAULT);
		if (maxGain >= 1.0f && maxGain <= 10.0f && sbo::FindCvar("voice_maxgain"))
			sbo::CvarSetFloat("voice_maxgain", maxGain);
		if (avgGain >= 0.0f && avgGain <= 1.0f && sbo::FindCvar("voice_avggain"))
			sbo::CvarSetFloat("voice_avggain", avgGain);
	}
	kv->deleteThis();
}

bool SboNewIngameMenus()
{
	return s_bMenusCvarRegistered && cl_new_ingame_menus.GetBool();
}

bool SboExperimentalMenus()
{
	if (!s_bMenusCvarRegistered)
	{
		// before HUD_Init: the saved setting, else the default (the file read once)
		static int s_iSaved = -1;
		if (s_iSaved < 0)
		{
			std::string saved;
			s_iSaved = atof(SavedCvarValue("cl_experimental_menus", saved) ? saved.c_str() : SBO_EXPERIMENTAL_MENUS_DEFAULT) != 0.0 ? 1 : 0;
		}
		return s_iSaved != 0;
	}
	return cl_experimental_menus.GetBool();
}

namespace
{
vgui2::VPANEL FindPanelNamed(vgui2::VPANEL panel, const char *name, int depth)
{
	if (!panel || depth > 4)
		return 0;
	const char *n = vgui2::ipanel()->GetName(panel);
	if (n && !Q_stricmp(n, name))
		return panel;
	const int count = vgui2::ipanel()->GetChildCount(panel);
	for (int i = 0; i < count; i++)
	{
		if (vgui2::VPANEL found = FindPanelNamed(vgui2::ipanel()->GetChild(panel, i), name, depth + 1))
			return found;
	}
	return 0;
}
} // namespace

bool SboOpenGameUIDialog(const char *command)
{
	if (!g_pEngineVGui)
		return false;
	// GameUI's game menu entries post their command to CTaskbar (panel "TaskBar"), whose OnCommand opens the dialog
	vgui2::VPANEL taskbar = FindPanelNamed(g_pEngineVGui->GetPanel(PANEL_GAMEUIDLL), "TaskBar", 0);
	if (!taskbar)
		return false;
	vgui2::ivgui()->PostMessage(taskbar, new KeyValues("Command", "command", command), 0);
	return true;
}
