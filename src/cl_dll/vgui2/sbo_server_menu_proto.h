//========= SBO 2.0: server-driven menus, client store =========//
//
// Purpose: the client's half of the "ServerMenu" protocol (src/game_shared/server_menu_shared.h, docs/SERVER_MENUS.md):
//          the bounded parser and the menus a server defined for this player. No engine and no VGUI2: the window
//          (sbo_server_menu.cpp) reads the store, the message hook (sbo_server_menu_bridge.cpp) feeds it, and
//          tests/server_menu runs it alone under AddressSanitizer and UndefinedBehaviorSanitizer.
//
//          Rules (everything from the network is checked here):
//          * every count and index is checked against the caps; a record past them is dropped
//          * a definition is kept "pending" until its END; a pending menu is never shown; a broken record drops it
//          * all strings of all menus together stay under MAX_TEXT_BYTES; each is cut to its length (UTF-8 safe)
//            and control characters are removed
//          * every record carries its length: a record of an op this client does not know (a newer server's) is
//            skipped, and so are the bytes a newer server appends to a known one; a record that reads past its
//            length is dropped alone; a length past the message's end stops the message
//
// Plain C++ (both header worlds include it).
//=================================================================//

#ifndef SBO_VGUI2_SBO_SERVER_MENU_PROTO_H
#define SBO_VGUI2_SBO_SERVER_MENU_PROTO_H

#include <stddef.h>
#include <string>
#include <vector>

#include "../../game_shared/server_menu_shared.h"

namespace servermenu
{
struct Model
{
	bool set = false;
	int path = 0; //!< index into the menu's modelPaths (checked by ValidModelPath when set)
	int body = 0, skin = 0, sequence = 0;
	int body1 = 0, sequence1 = 0; //!< variant 1
};

struct Badge
{
	int style = BADGE_PLAIN;
	std::string text, tip;
};

struct Item
{
	bool defined = false;
	int tab = 0;
	int picture = PICTURE_NONE;
	int cell = 0;
	int flags = 0;
	std::string label, price, badge, action, text;
	Model models[MODEL_LAYERS];
	float cooldownEnd = 0.0f;   //!< the client time (Parse's now) the cooldown ends at; 0 none
	float cooldownTotal = 0.0f; //!< its whole length in seconds (the clock's full turn)
	unsigned long long avatar = 0; //!< AVATAR: a player's SteamID64 whose avatar the item shows (0: none)
	int meter = METER_NONE; //!< METER: the bar's percent (0..100), METER_NONE none
	std::vector<Badge> badges;     //!< BADGES (shown instead of badge while not empty)
	std::vector<int> voters;       //!< VOTERS: client indices 1..32
	std::string line;       //!< ITEMLINE: the popup's line under the picture
	std::string title;      //!< ITEMTITLE: the popup's title in full ("": the label)
	std::string map; //!< MAPPICTURE: the map whose picture the card shows (lower case, ValidMapName; "" none)
	std::string image;   //!< ITEMPICTURE: a .tga the card, row and popup show (ValidPicturePath; "" none)
	std::string confirm; //!< CONFIRM: the question asked before its pick ("" none)

	//! Seconds of the cooldown left at the client time now (0: none, the item can be picked as its flags say)
	float CooldownLeft(float now) const { return cooldownEnd > now ? cooldownEnd - now : 0.0f; }
};

struct Picture
{
	std::string path; //!< checked by ValidPicturePath ("" = none)
	int columns = 1, rows = 1;
};

struct Field
{
	std::string label, hint, button; //!< hint: the placeholder; button: its own button's label ("" = Enter only)
	int maxLength = LEN_FIELD_TEXT;
};

struct Slider
{
	std::string label, tip;
	int min = 0, max = 100, step = 1, value = 0;
	int updates = 0; //!< SLIDERVALUE records so far (the window keeps a dragged value until the server answers)

	//! v clamped to min..max and snapped to the step (from min)
	int Clamp(int v) const
	{
		if (v <= min)
			return min;
		if (v >= max)
			return max;
		const int snapped = min + (v - min + step / 2) / step * step;
		return snapped > max ? max : snapped;
	}
};

struct Toggle
{
	std::string label, tip;
	bool on = false;
	int updates = 0; //!< TOGGLEVALUE records so far (the window shows a flip until the server answers)
};

//! A dropdown (SELECT): in the menu's controls (item NO_ITEM) or in an item's popup
struct Select
{
	bool defined = false;
	int item = NO_ITEM;
	std::string label, tip;
	std::vector<std::string> options;
	int value = 0;
	int updates = 0; //!< SELECTVALUE records so far (the window shows a choice until the server answers)
};

//! The HUD strip (HUDBAR): title, sub line, up-next line and a progress bar the client counts itself
struct HudBar
{
	bool shown = false;
	std::string title, sub, next;
	float start = 0.0f;     //!< the client time the track started at
	float length = 0.0f;    //!< seconds; 0 = no progress bar
	float nextUntil = 0.0f; //!< the client time the up-next line goes (0: while the strip shows)
	bool nextShown = false;
	bool notice = false;    //!< HUDBAR_NOTICE: the extra line is a message, drawn without the "Up next" label
	unsigned long long avatar = 0; //!< a player's SteamID64 shown in place of the equalizer sign (0: none)
	unsigned long long nextAvatar = 0; //!< the up-next line's player (a round bubble before it; 0: none)
};

struct Menu
{
	bool complete = false; //!< END arrived: may be shown
	int serial = 0;
	int layout = LAYOUT_LIST;
	int flags = 0;
	int variant = 0;
	std::string title;
	std::vector<std::string> tabs;
	std::vector<Picture> pictures;
	std::vector<std::string> buttons;
	std::vector<std::string> buttonTips; //!< per button its tooltip ("" none)
	std::vector<int> buttonFlags;        //!< BUTTONSTATE: per button its BUTTON_* flags (0: enabled)
	std::vector<Field> fields;
	std::vector<Slider> sliders;
	std::vector<Toggle> toggles;
	std::vector<Select> selects;          //!< SELECT: up to MAX_MENU_SELECTS (only defined ones are shown)
	std::vector<std::string> buttonConfirms; //!< CONFIRM: per button its question ("" none)
	std::vector<std::string> modelPaths; //!< MODELPATH: each checked by ValidModelPath ("" = not sent or bad)
	std::string headers[MAX_HEADERS];
	std::vector<Item> items;
	int starsTenths = STARS_NONE; //!< STARS: the read-only row's average in tenths of a star (STARS_NONE: none)
	std::string starsText;        //!< STARS: its text ("" none)
	int winner = NO_ITEM;         //!< WINNER: the item that won (NO_ITEM: none)

	size_t TextBytes() const;
	//! A button that exists and is not greyed (BUTTONSTATE)
	bool ButtonEnabled(int button) const
	{
		return button >= 0 && button < (int)buttons.size() &&
		       (button >= (int)buttonFlags.size() || !(buttonFlags[button] & BUTTON_DISABLED));
	}
	//! The model file of a layer that is set
	const std::string &ModelPath(const Model &m) const;
};

//! What a message did (in order), for the window
struct Event
{
	enum Kind
	{
		HELLO,   //!< a = protocol version the server speaks
		DEFINED, //!< a = menu: a new definition is complete (an open window shows it in place)
		CHANGED, //!< a = menu: a live update (HEADER, STATE, LABEL, ITEMTEXT, COOLDOWN, VARIANT, SLIDERVALUE, TOGGLEVALUE, AVATAR,
	         //!< MAPPICTURE, METER, ITEMLINE, BADGES, VOTERS, ITEMTITLE, SELECTVALUE, CONFIRM, ITEMPICTURE, BUTTONSTATE, STARS,
	         //!< WINNER) of a menu
		OPEN,    //!< a = menu, b = tab, c = highlighted item (checked: the menu is complete, the serial matches)
		CLOSE,   //!< a = menu or CLOSE_ANY
		HUDBAR,  //!< the HUD strip changed (Store::Hud)
	};
	Kind kind;
	int a = 0, b = 0, c = 0;
};

class Store
{
public:
	Store() { Reset(); }

	//! A new map, a reconnect or a fullupdate (InitHUD): every menu is dropped
	void Reset();

	//! One "ServerMenu" message. Appends to events; returns false if the message was cut short (bad record).
	//! now: the client time in seconds (a COOLDOWN ends at now plus its time left).
	bool Parse(const unsigned char *data, size_t size, std::vector<Event> &events, float now = 0.0f);

	//! A complete menu, or nullptr
	const Menu *Get(int menu) const;
	//! The HUD strip
	const HudBar &Hud() const { return m_Hud; }

	//! The one console line a broken message gives (once per map); "" when nothing was wrong
	const std::string &LastError() const { return m_LastError; }
	bool TakeErrorToReport();

	size_t TextBytes() const;

private:
	bool ParseRecord(int op, class Reader &r, std::vector<Event> &events, float now);
	Menu *Target(int menu); //!< the pending definition if there is one, else the complete menu, else nullptr
	void Fail(const char *why, int menu);
	bool Fits(size_t more) const;

	Menu m_Live[MAX_SERVER_MENUS];
	Menu m_Pending[MAX_SERVER_MENUS];
	bool m_bPending[MAX_SERVER_MENUS] = {};
	HudBar m_Hud;
	std::string m_LastError;
	bool m_bErrorReported = false;
	bool m_bErrorNew = false;
};

//! A picture path the client loads: lower-case letters, digits, '_', '-', '/', under gfx/ or sprites/, one dot, ending
//! in ".tga", no "..", no leading '/', no "//", at most LEN_PATH bytes
bool ValidPicturePath(const std::string &path);
//! A model path: letters, digits, '_', '-', '/', under models/, one dot, ending in ".mdl", the same other rules
bool ValidModelPath(const std::string &path);
//! A MAPPICTURE map name: lower-case letters, digits, '_', '-', '.', no "..", at most LEN_MAP bytes
bool ValidMapName(const std::string &map);
//! Control characters removed (keepNewlines: but '\n'), cut to max bytes on a UTF-8 character boundary
std::string CleanString(const char *s, size_t len, size_t max, bool keepNewlines = false);
//! A text field's text for "servermenu text": control characters, '"', ';' and '\\' removed, leading and trailing
//! spaces too, cut to max bytes on a UTF-8 character boundary
std::string CleanFieldText(const std::string &s, size_t max);
} // namespace servermenu

#endif // SBO_VGUI2_SBO_SERVER_MENU_PROTO_H
