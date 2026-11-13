//========= SBO VGUI2: the server browser's look, shared by SBO's GameUI windows =========//
//
// Purpose: the colors, fonts, scale and drawing helpers of the server browser
//          (docs/SERVER_BROWSER.md), in one place for every SBO window drawn in that look:
//          the server browser (sbo_server_browser.cpp) and the joining screen (sbo_loading_screen.cpp).
//
//   Sizes: the designs are drawn for a 1280 x 800 screen; S().M(px) turns a design pixel into a screen pixel at the
//   current scale (UpdateStyle: the screen height through the proportional scale helper, narrower screens by width).
//   Fonts: one per size and weight (MakeFont), Tahoma on Windows (the design's), Liberation Sans on Linux.
//   Scheme: ui/resource/serverbrowserscheme.res (drop-down menus, text entries and scroll bars in these colors).
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_UI_STYLE_H
#define SBO_VGUI2_SBO_UI_STYLE_H

#include <math.h>
#include <string>
#include <vector>

#include <Color.h>
#include <vgui/VGUI2.h>

namespace vgui2
{
class Panel;
}

namespace sbolook
{
const Color COL_TEXT(127, 214, 95, 255);           // #7fd65f
const Color COL_TEXT_BRIGHT(166, 236, 138, 255);   // #a6ec8a (active tab, good ping)
const Color COL_TEXT_DIM(159, 184, 151, 255);      // #9fb897
const Color COL_TEXT_LIGHT(201, 217, 195, 255);    // #c9d9c3
const Color COL_TEXT_SELECTED(198, 242, 176, 255); // #c6f2b0
const Color COL_TEXT_FAINT(92, 111, 87, 255);      // #5c6f57 (the "·" separator; disabled buttons)
const Color COL_BORDER(60, 122, 50, 255);          // #3c7a32
const Color COL_BORDER_LIST(44, 90, 37, 255);      // #2c5a25
// rgba(12, 15, 12, 0.94) over the design's dark page; in the game the menu and the world are behind it, so nearly
// opaque to give the same dark window
const Color COL_BG_WINDOW(12, 15, 12, 250);
const Color COL_BG_INPUT(11, 14, 11, 255);         // #0b0e0b
const Color COL_BG_SELECTED(29, 58, 24, 255);      // #1d3a18 (selected row, active tab)
const Color COL_BG_PRIMARY(46, 95, 38, 255);       // #2e5f26 (Connect)
const Color COL_BORDER_PRIMARY(95, 174, 69, 255);  // #5fae45 (Connect border, check boxes)
const Color COL_TEXT_PRIMARY(230, 245, 223, 255);  // #e6f5df
const Color COL_PING_MID(227, 194, 107, 255);      // #e3c26b
const Color COL_PING_BAD(240, 160, 122, 255);      // #f0a07a

// The designs' page: a 1280 x 800 screen
constexpr int MOCK_SCREEN_WIDE = 1280;
constexpr int MOCK_SCREEN_TALL = 800;

struct Style
{
	float scale = 1.0f;
	vgui2::HFont title = 0, tab = 0, body = 0, small = 0, row = 0, name = 0, button = 0, close = 0;
	int M(int px) const { return (int)ceilf(px * scale); }
};

//! ui/resource/serverbrowserscheme.res, loaded once
vgui2::HScheme BrowserScheme();
//! The look's font at this pixel size and weight (made once, kept: the engine cannot free fonts)
vgui2::HFont MakeFont(int tall, int weight);
//! The same for another face (a string literal: kept by pointer)
vgui2::HFont MakeFontFace(const char *face, int tall, int weight);
//! Registers the shipped Barlow Condensed SemiBold file once (before the first MakeFontFace of that face)
void AddBarlowFonts();
//! The menus' UI face (Tahoma on Windows, Liberation Sans on Linux)
const char *SboUiFontFace();
//! The scale for this screen (pProportional: a proportional panel for the scale helper); fonts made on a change
void UpdateStyle(vgui2::Panel *pProportional);
//! UpdateStyle even when the screen did not change (after SetStyleScale shrank the look for a narrow screen)
void ResetStyle(vgui2::Panel *pProportional);
//! The look at this scale
void SetStyleScale(float scale);
const Style &S();

// Drawing (inside the panel being painted)
void FillRect(int x, int y, int w, int h, Color c);
void OutlineRect(int x, int y, int w, int h, Color c);
std::wstring Widen(const char *utf8);
int TextWide(vgui2::HFont font, const std::wstring &text);
//! Text with its top at y; cut with "..." to maxWide (0 = no limit). Returns the drawn width.
int DrawText(vgui2::HFont font, Color color, int x, int y, const std::wstring &text, int maxWide = 0);
int FontTall(vgui2::HFont font);
//! Word-wrapped lines of text at most maxW wide (a word longer than the line is cut where it must; at most 16)
std::vector<std::wstring> WrapLines(vgui2::HFont font, const std::string &utf8, int maxW);
//! Text vertically centered in [y, y + h)
int DrawTextMid(vgui2::HFont font, Color color, int x, int y, int h, const std::wstring &text, int maxWide = 0);
//! Upper case with 0.04 em letter spacing (the designs' small headings)
int DrawSpacedUpper(vgui2::HFont font, Color color, int x, int y, const char *text);
int SpacedUpperWide(vgui2::HFont font, const char *text);
//! A filled triangle pointing up or down (no symbol font; docs/dev/LINUX_CLIENT.md)
void DrawTriangle(int cx, int cy, int size, bool up, Color c);
//! A cooldown clock (the server menus' cooling cards, the GM spawn window's waiting cards): a disk whose bright part is
//! the time left (from 12 o'clock, clockwise; it drains as the hand moves round), a rim, and the seconds left in its
//! middle ("1:05" from a minute up). left / total in seconds.
void DrawCooldownClock(int cx, int cy, int radius, float left, float total, vgui2::HFont font);
//! Small padlock, w wide
void DrawPadlock(int x, int y, int w, Color c);

// Server data
//! Color codes (^1, ^xRGB, ^#RRGGBB) and control characters out of a server or player name
std::string CleanName(const char *s);
//! SboServerRow::Mode of a map name, as the server picks its rules (gamerules.cpp InstallGameRules)
int ModeOfMap(const char *map);
const char *ModeName(int mode);
Color PingColor(int ping);
//! A map name usable in a file path: letters, digits, '_', '-', '.', no "..", short
bool SafeMapName(const std::string &map);
std::string Lower(std::string s);
} // namespace sbolook

//! voice_maxgain / voice_avggain (HL25 engine cvars, not archived): HL25's defaults, kept by the new Voice settings in
//! voicegain.vdf (the mod folder) and set again at every start (VGui2_RegisterMenusCvar)
constexpr float SBO_VOICE_MAXGAIN_DEFAULT = 5.0f;
constexpr float SBO_VOICE_AVGGAIN_DEFAULT = 0.5f;
void SboVoiceGain_Save();
void SboVoiceGain_Restore();

//-----------------------------------------------------------------------------
// "Experimental new menus" (cl_experimental_menus, archived; Advanced > General): 1 = SBO's server browser, joining
// screen, Options and Create server windows; 0 = GameUI's own menus and dialogs, as before. The default (1) is the one
// line SBO_EXPERIMENTAL_MENUS_DEFAULT in sbo_ui_style.cpp. Read when a menu entry is used (the window that opens) and
// at the start of each connect (the joining screen).
//-----------------------------------------------------------------------------
bool SboExperimentalMenus();
//! "New in-game menus" (cl_new_ingame_menus, archived, default 1; Advanced > General): the new loadout menu
//! (sbo_loadout_menu.cpp), MOTD and map info windows; 0 = the menus as before. Read when one of them opens.
bool SboNewIngameMenus();
//! From HUD_Init (VGui2_Chat_Init): registers cl_experimental_menus
void VGui2_RegisterMenusCvar();
//! GameUI's own dialog for a game menu command ("OpenServerBrowser", "OpenOptionsDialog",
//! "OpenCreateMultiplayerGameDialog"): the command posted to GameUI's game menu target (its "TaskBar" panel), as its
//! own menu entry does. False if that panel is not found.
bool SboOpenGameUIDialog(const char *command);

#endif // SBO_VGUI2_SBO_UI_STYLE_H
