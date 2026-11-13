//========= SBO VGUI2: Game Master mode, the GM screens' look =========//
//
// Purpose: what the GM mode screens share (docs/GM_MODE.md "Screen and menus"; four 1280x720
//          screens):
//   * the palette and Tahoma at the design's pixel sizes, scaled with the screen height (720 lines = 1:1);
//   * drawing helpers (text, boxes, dashed boxes);
//   * the Game Master's state the screens show: the GmMode message's state and placement (sbo_gm_spawn_menu.cpp),
//     the GmStatus strip numbers and the GmPlayers rows (sbo_gm_hud.cpp), the plugin's card texts (GmEntry).
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_GM_UI_H
#define SBO_VGUI2_SBO_GM_UI_H

#include <string>
#include <vector>
#include <Color.h>
#include <vgui/VGUI2.h>

#include "../../game_shared/gm_catalog.h" // GM_ROAM_SPEED, GM_STATUS_*

namespace gmui
{
// the design's palette
const Color BG(12, 15, 12, 245);          // rgba(12,15,12,0.96): windows
const Color BG_STRIP(12, 15, 12, 209);    // 0.82: the status strip
const Color BG_PANEL(12, 15, 12, 184);    // 0.72: nearest players
const Color BG_HINTS(12, 15, 12, 179);    // 0.7: key hints
const Color BG_HINT(12, 15, 12, 191);     // 0.75: the placement line
const Color BORDER(0x3c, 0x7a, 0x32, 255);
const Color BORDER_DIM(0x2c, 0x5a, 0x25, 255);
const Color BORDER_LOCKED(0x23, 0x2b, 0x22, 255);
const Color GREEN(0x7f, 0xd6, 0x5f, 255);     // titles, buttons
const Color GREEN_LIGHT(0xa6, 0xec, 0x8a, 255); // "Ready", living dots, "[n] selected"
const Color HINT_TEXT(0xc6, 0xf2, 0xb0, 255);
const Color BRIGHT(0xe6, 0xf5, 0xdf, 255);
const Color DIM(0x9f, 0xb8, 0x97, 255);
const Color VALUE(0xc9, 0xd9, 0xc3, 255);
const Color FAINT(0x6f, 0x8a, 0x68, 255);
const Color ACCENT(0xe3, 0xc2, 0x6b, 255);
const Color SEL_BG(0x1d, 0x3a, 0x18, 255);
const Color SEL_BORDER(0x5f, 0xae, 0x45, 255);
const Color BUTTON_BG(0x2e, 0x5f, 0x26, 255);
const Color PREVIEW_BG(0x1d, 0x2a, 0x1b, 255);
const Color MAP_BG(0x1a, 0x22, 0x19, 255);
const Color BAR_FILL(0x5f, 0xae, 0x45, 255);  // the energy bar
const Color BAR_RED(0xd6, 0x5f, 0x5f, 255);   // near empty; a cost over the energy
const Color INPUT_BG(0x0b, 0x0e, 0x0b, 255);

//! design pixels (at 720 lines) -> screen pixels
int S(float px);
//! the key hints bar at the bottom center (the GM key hints; the tool windows' mouse look note): its height, and its
//! top for a bar standing on bottom (the screen's height, or the top of what is drawn there)
int HintsBarTall();
int HintsBarTop(int bottom);
//! Tahoma (Linux: DejaVu Sans) at design px size; bold for the key names
vgui2::HFont Font(float px, bool bold = false);
int TextWide(vgui2::HFont font, const wchar_t *text);
int FontTall(vgui2::HFont font);
//! draws text at (x, y) (top left) in one call, as every row (no letter spacing); returns the width drawn
int Text(vgui2::HFont font, int x, int y, const Color &color, const wchar_t *text);
//! the energy bar (with the budget plugin): the points filling the bar empty -> max (green, amber under 25 %, red
//! under 10 %), the next refill as a faint sliver after the fill, cost > 0: the cost as an amber chunk at the end of the
//! fill (red, past the fill, when it is more than the points); returns the fill's end x
int EnergyBar(int x, int y, int w, int h, int cost);
//! a window's close button: the box and a centered X drawn as two strokes (every GM window)
void CloseButton(int x, int y, int w, int h);
void Fill(int x, int y, int w, int h, const Color &color);
void Outline(int x, int y, int w, int h, const Color &color, int thickness = 1);
void Dashed(int x, int y, int w, int h, const Color &color, int dash, int gap);
std::wstring Widen(const char *utf8);

// the Game Master's state (sbo_gm_spawn_menu.cpp, from the GmMode message)
int GetState();          //!< GM_STATE_*
bool IsPlacing();
const std::wstring &PlaceLabel(); //!< what is being placed, with the loadout
int PlaceEntry();
int PlaceVariant();
//! a plugin card text (GM_TEXT_*), countdown resolved ("" none); the variant's, else the entry's
std::wstring CardText(int entry, int variant, int slot);
//! seconds left of the status line's countdown (0: none) and its whole length (the cooldown clock)
int CardSeconds(int entry, int variant);
float CardCountdown(int entry, int variant, float &total);

// the GmStatus message (sbo_gm_hud.cpp)
struct Status
{
	bool received = false;
	int flags = 0;  //!< GM_STATUS_*
	int points = 0;
	int tier = 0;
	float tierEnd = 0;  //!< HUD time, 0 none
	float spawnEnd = 0;
	int alive = 0;
	int selected = 0;
	int roamSpeed = GM_ROAM_SPEED; //!< pm_shared PM_GmRoamSpeed (0: sv_spectatormaxspeed)
	int maxPoints = 0;          //!< the energy bar's maximum (0: no bar)
	int refill = 0;             //!< points of the next refill
	float refillInterval = 0;   //!< seconds between refills
	float refillEnd = 0;        //!< HUD time of the next refill, 0 none
	int killPop = 0;            //!< the last kill reward ("+N" over the bar)
	float killPopTime = -100;   //!< HUD time it came
};
const Status &GetStatus();
bool HasEconomy();
//! the guide's title from the server (the GM plugin's syndicate_gmmode_intro_title), "" for the game's own
const std::wstring &GuideTitle();
void SetGuideTitle(const std::wstring &title);
int SecondsLeft(float end);

// the GmPlayers message (sbo_gm_hud.cpp)
struct PlayerRow
{
	int index = 0;
	bool alive = false;
	int health = 0;
	int meters = 0;
	float pos[3] = { 0, 0, 0 };
	int weapon = 255;   //!< catalog entry
	int killedBy = 255; //!< catalog entry of the Game Master's monster that killed him this round
};
const std::vector<PlayerRow> &GetPlayers();

// the GmMonsters message (sbo_gm_hud.cpp): the Game Master's monsters on the players window's map
struct MonsterRow
{
	int entity = 0;
	float pos[3] = {};
	int entry = 255;
	int health = 0; //!< percent
	int flags = 0;  //!< GM_MONSTER_*
};
const std::vector<MonsterRow> &GetMonsters();
std::wstring PlayerName(int index);

//! the key bound to a command, upper case ("K"), or the fallback
std::wstring KeyFor(const char *command, const wchar_t *fallback);
//! how to open the players window in plain words, with the player's own key ("Press K, then choose option 3")
std::wstring PlayersPath();
//! the nearest players panel: its row pitch and its height with that many rows (the kill feed goes under it)
int NearestPitch();
int NearestPanelTall(int rows);

//! a GM window (spawn, players, guide) is open: the HUD strip, panels and hints are hidden under it
bool AnyWindowOpen();
} // namespace gmui

// the players window and the HUD (sbo_gm_hud.cpp)
void GmUi_OpenPlayers(int highlight);
void GmUi_ClosePlayers();
bool GmUi_PlayersOpen();
void GmUi_Init();
void GmUi_Reset();
//! a GmNotice line came: its popup over every window
void GmUi_ShowNotice();

#endif // SBO_VGUI2_SBO_GM_UI_H
