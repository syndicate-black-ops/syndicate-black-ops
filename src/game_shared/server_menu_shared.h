//========= SBO 2.0: server-driven menus, protocol =========//
//
// Purpose: the "ServerMenu" user message (protocol version 4) that AMX Mod X plugins send through the server menu API
//          plugin (server_menu_api.sma, include server_menu.inc; docs/SERVER_MENUS.md), and the "servermenu" client
//          command the client answers with. The client parser is src/cl_dll/vgui2/sbo_server_menu_proto.cpp; the
//          window is sbo_server_menu.cpp. The game's server DLL only registers the message (LinkUserMessages); the API
//          plugin sends it and handles the command. The Pawn include repeats these numbers (tests/server_menu checks them).
//
//          One message holds one or more records back to back; a record never crosses messages. Every record is a
//          byte op, a byte length (the bytes that follow, at most MSG_BYTES - 2), then its fields. A client skips the
//          records it does not know and the bytes after the fields it knows, so a newer server's records and fields
//          reach an older client harmlessly. Strings are UTF-8, NUL-terminated; the client cuts them to the lengths
//          below.
//
//          Versions: the server's HELLO says the protocol it speaks; the client answers with the one it reads
//          ("servermenu hello <version> <on>"). The API sends a player only what his version reads (the smaller of
//          the two) and leaves newer records out or falls back for him. Version 1 had no record lengths: a version 1
//          peer and a newer one never get past the hello. Version 3 reads item labels of LEN_LABEL bytes; a version 2
//          client reads LEN_LABEL_V2 (it cuts a longer one there itself), and the API cuts them for it to
//          LEN_LABEL_V2 with "..." at the end. Version 4 reads LAYOUT_STARS (a star rating) and STARS; an older client
//          reads LAYOUT_STARS as LAYOUT_LIST (one row per star), and the API sends it LAYOUT_LIST and no STARS.
//
// Shared by the client and the tests. Plain C++, no engine headers.
//=================================================================//

#ifndef SBO_GAME_SHARED_SERVER_MENU_SHARED_H
#define SBO_GAME_SHARED_SERVER_MENU_SHARED_H

#define SERVERMENU_MSG_NAME "ServerMenu"
#define SERVERMENU_CMD_NAME "servermenu"

namespace servermenu
{
constexpr int PROTOCOL_VERSION = 4;
constexpr int PROTOCOL_MIN = 2; //!< the oldest version this client and the API speak (version 1 had no record lengths)

//! The engine's user message payload limit (MAX_USER_MSG_DATA, 192; stock HLDS stops with Host_Error past it). The
//! API plugin packs records up to MSG_BYTES.
constexpr int MSG_LIMIT = 192;
constexpr int MSG_BYTES = 188;

// Record ops
enum Op
{
	OP_HELLO = 1,    //!< byte version
	OP_DEFINE = 2,   //!< byte menu, short serial, byte layout, byte flags, short items, byte tabs, byte headers,
	                 //!< byte pictures, byte buttons, byte model paths, byte fields, byte sliders, byte toggles,
	                 //!< string title
	OP_TAB = 3,      //!< byte menu, byte tab, string label
	OP_PICTURE = 4,  //!< byte menu, byte picture, byte columns, byte rows, string path (.tga)
	OP_ITEM = 5,     //!< byte menu, short item, byte tab, byte picture (255 none), byte cell, byte flags,
	                 //!< string label, string price line, string badge, string action
	OP_ITEMTEXT = 6, //!< byte menu, short item, string description (also live)
	OP_HEADER = 7,   //!< byte menu, byte line, string text (also a live update)
	OP_END = 8,      //!< byte menu, short serial: the definition is complete
	OP_OPEN = 9,     //!< byte menu, short serial, byte tab, short highlighted item
	OP_STATE = 10,   //!< byte menu, short item, byte flags, string price line, string badge, string action (live)
	OP_CLOSE = 11,   //!< byte menu (CLOSE_ANY: whatever is open)
	OP_MODEL = 12,   //!< byte menu, short item, byte layer, byte model path (MODELPATH), short body, byte skin,
	                 //!< short sequence, short body (variant 1), short sequence (variant 1)
	OP_VARIANT = 13, //!< byte menu, byte variant (0 or 1): which body / sequence of the MODEL records to show
	OP_BUTTON = 14,  //!< byte menu, byte button, string label, string tooltip ("" none)
	OP_MODELPATH = 15, //!< byte menu, byte model path, string path (.mdl): the menu's model files, each sent once
	                   //!< (the MODEL records name them by number), before the items
	OP_LABEL = 16,   //!< byte menu, short item, string label (live)
	OP_COOLDOWN = 17, //!< byte menu, short item, short tenths of a second left (0: none), short tenths of the whole
	                  //!< cooldown (the clock's full turn; less than the time left: the time left) (live). The
	                  //!< client counts down itself; the item cannot be picked until it ends
	OP_HUDBAR = 18,  //!< byte flags (HUDBAR_*), short seconds played, short track seconds (0: no progress bar),
	                 //!< short seconds the up-next line shows (0: while the strip shows), long avatar SteamID64 low
	                 //!< 32 bits, long high 32 bits (0: the equalizer sign), long up-next avatar SteamID64 low 32
	                 //!< bits, long high 32 bits (0: none), string title, string sub line, string up-next line: the
	                 //!< HUD strip (one per client, not a menu; HUDBAR_SHOW off hides)
	OP_FIELD = 19,   //!< byte menu, byte field, byte max length, string label, string placeholder, string button:
	                 //!< a text field whose text the player sends ("servermenu text")
	OP_SLIDER = 20,  //!< byte menu, byte slider, short min, short max, short step, short value, string label, string
	                 //!< tooltip: a
	                 //!< slider (0..65535, min <= max, step >= 1) whose value the player sends on release
	                 //!< ("servermenu slider")
	OP_SLIDERVALUE = 21, //!< byte menu, byte slider, short value (live; clamped to the slider's range and step;
	                     //!< also the API's answer to "servermenu slider": the value it kept)
	OP_TOGGLE = 22,  //!< byte menu, byte toggle, byte on (0 / 1), string label, string tooltip: an on / off switch the
	                 //!< player flips
	                 //!< ("servermenu toggle")
	OP_TOGGLEVALUE = 23, //!< byte menu, byte toggle, byte on (live; also the API's answer to "servermenu toggle": the
	                     //!< state it kept, so a refused flip goes back)
	OP_AVATAR = 24,  //!< byte menu, short item, long SteamID64 low 32 bits, long high 32 bits (0: none): the item
	                 //!< shows that player's Steam avatar before its label (live)
	OP_MAPPICTURE = 25, //!< byte menu, short item, string map name ("" none): the item's card shows that map's
	                    //!< picture (the client's map pictures: gfx/mapimages/<map>.jpg, else downloaded; the
	                    //!< placeholder with the map name until then) (live)
	OP_METER = 26,   //!< byte menu, short item, byte percent (0..100; METER_NONE: none): a bar under the card's name
	                 //!< line, e.g. a vote's share (live)
	OP_ITEMLINE = 27, //!< byte menu, short item, string line: the item popup's line under its picture (MENU_POPUP;
	                  //!< e.g. "Co-op  Nominated by X") (live)
	OP_BADGES = 28,  //!< byte menu, short item, byte count (0..MAX_BADGES), then per badge: byte style (BADGE_*), string
	                 //!< text, string tooltip: the card's badges in a row, each with its hover text (live; replaces
	                 //!< the STATE badge while count > 0)
	OP_VOTERS = 29,  //!< byte menu, short item, byte count (0..MAX_VOTERS), then count x byte client index (1..32):
	                 //!< the players shown as avatar bubbles on the card (names and avatars from the client's own
	                 //!< player info; a client index without a player: the default bubble without a name) (live)
	OP_ITEMTITLE = 30, //!< byte menu, short item, string title ("" none): the item popup's title in full in place of
	                   //!< the label (which is cut at LEN_LABEL), e.g. a long track title (live)
	OP_SELECT = 31,  //!< byte menu, byte select (< MAX_MENU_SELECTS), short item (NO_ITEM: the menu's controls, else
	                 //!< that item's popup), byte options (1..MAX_SELECT_OPTIONS), byte selected, string label, string
	                 //!< tooltip: a dropdown whose choice the player sends ("servermenu select"); in a definition
	OP_SELECTOPTION = 32, //!< byte menu, byte select, byte option, string label: one of its options; in a definition
	OP_SELECTVALUE = 33, //!< byte menu, byte select, byte selected (live; also the API's answer to "servermenu select":
	                     //!< the choice it kept, so a refused one goes back)
	OP_CONFIRM = 34, //!< byte menu, byte kind (CONFIRM_ITEM, CONFIRM_BUTTON), short index, string question ("" none):
	                 //!< the client asks it in a Yes / No dialog before it sends that item's pick or that button
	                 //!< (No or Esc: nothing is sent) (live)
	OP_ITEMPICTURE = 35, //!< byte menu, short item, string path ("" none; ValidPicturePath: a .tga under gfx/ or
	                     //!< sprites/, which the server puts in its download list): the item's picture on its card,
	                     //!< row and popup; a file the client does not have: the placeholder (live)
	OP_BUTTONSTATE = 36, //!< byte menu, byte button, byte flags (BUTTON_*): one of the menu's buttons greyed (it sends
	                     //!< nothing) or enabled again (live; a definition starts with every button enabled)
	OP_STARS = 37,   //!< byte menu, byte tenths (the average in tenths of a star, 0..10 x MAX_STARS; STARS_NONE: none),
	                 //!< string text ("" none): a LAYOUT_STARS menu's read-only star row, partly filled to the
	                 //!< average, with its text after it (e.g. "4.3 - 44 ratings") (live; version 4)
	OP_LAST = OP_STARS, //!< the last op this version knows (later ones are skipped)
};

// CONFIRM kinds
constexpr int CONFIRM_ITEM = 0;
constexpr int CONFIRM_BUTTON = 1;

// BUTTONSTATE flags
constexpr int BUTTON_DISABLED = 1; //!< greyed: a click, its key or a confirm's Yes sends nothing
constexpr int BUTTON_FLAGS_ALL = 1;

// BADGES styles
constexpr int BADGE_PLAIN = 0; //!< the STATE badge's look
constexpr int BADGE_GREEN = 1; //!< picked (as an ITEM_PICKED badge)
constexpr int BADGE_GOLD = 2;  //!< leading, the winner
constexpr int BADGE_DIM = 3;   //!< a side note
constexpr int BADGE_STYLES = 4;

// HUDBAR flags
constexpr int HUDBAR_SHOW = 1;     //!< shown (else hidden)
constexpr int HUDBAR_NEXT_GONE = 2; //!< the up-next line's time is over: not shown
constexpr int HUDBAR_NOTICE = 4;    //!< the extra line is a notice (a message), not the next track: no "Up next" label
constexpr int HUDBAR_FLAGS_ALL = 7;

// DEFINE layout
enum Layout
{
	LAYOUT_LIST = 0,
	LAYOUT_GRID = 1,
	LAYOUT_STARS = 2, //!< version 4: a star rating, a compact centered window: the items are stars in a row (item n =
	                  //!< n + 1 stars, at most MAX_STARS). The mouse over a star lights it and the ones before it and
	                  //!< shows its label under the row; a click or key 1..n picks it ("servermenu pick"). An
	                  //!< ITEM_PICKED star (the player's rating) is lit while nothing is hovered. The header lines show
	                  //!< centered above, the STARS row under them. An older client reads it as LAYOUT_LIST
};

// DEFINE flags
constexpr int MENU_SIDE = 1;     //!< docked on the right; the screen stays visible on the left
constexpr int MENU_SEARCH = 2;   //!< a search box (client side filter)
constexpr int MENU_BACK = 4;     //!< a Back button ("servermenu back")
constexpr int MENU_NOCLOSE = 8;  //!< no Close button (Esc still closes)
constexpr int MENU_PREVIEW = 16; //!< the model preview: the player's own model with the MODEL layers, turned by
                                 //!< dragging; left of a side window, else in the side panel
constexpr int MENU_POPUP = 32;   //!< centered without the side panel; a click on an item opens its popup (its map
                                 //!< picture large, label, ITEMLINE, text, Back and the primary action); a double
                                 //!< click or 1-9 still picks
constexpr int MENU_LARGE = 64;   //!< large cards, 3 to 6 columns: the fewest that fit them all (else 4, or 5 with MENU_POPUP)
constexpr int MENU_ALLTAB = 128; //!< the first tab shows the items of every tab
constexpr int MENU_FLAGS_ALL = 255;

// ITEM / STATE flags
constexpr int ITEM_DISABLED = 1; //!< greyed, cannot be picked
constexpr int ITEM_PICKED = 2;   //!< the one in use (green badge; its MODEL layers are worn in the preview)
constexpr int ITEM_HIDDEN = 4;   //!< not shown
constexpr int ITEM_FLAGS_ALL = 7;

// MODEL layers: 0 = the player model itself (a skin), 1..3 = models drawn on the player's bones (costumes)
constexpr int MODEL_LAYERS = 4;
constexpr int MODEL_SAME = 0xFFFF; //!< a variant 1 body / sequence equal to variant 0's

constexpr int CLOSE_ANY = 255;
constexpr int PICTURE_NONE = 255;
constexpr int METER_NONE = 255;
constexpr int STARS_NONE = 255;
constexpr int MAX_STARS = 10; //!< items of a LAYOUT_STARS menu drawn as stars (more are not shown)

// Caps (the client keeps no more; the API plugin refuses to build past them)
constexpr int MAX_SERVER_MENUS = 32;
constexpr int MAX_MENU_ITEMS = 256;
constexpr int MAX_TABS = 8;
constexpr int MAX_HEADERS = 4;
constexpr int MAX_PICTURES = 16;
constexpr int MAX_SHEET_CELLS = 16; //!< columns and rows of a picture sheet
constexpr int MAX_MENU_BUTTONS = 3;
constexpr int MAX_MENU_FIELDS = 2;
constexpr int MAX_MENU_SLIDERS = 2;
constexpr int MAX_MENU_TOGGLES = 4;
constexpr int MAX_MENU_SELECTS = 4;   //!< dropdowns of one menu (its controls and its items' popups together)
constexpr int MAX_SELECT_OPTIONS = 16; //!< options of one dropdown
constexpr int MAX_BADGES = 3;  //!< BADGES per item
constexpr int MAX_VOTERS = 32; //!< VOTERS per item
constexpr int MAX_TEXT_BYTES = 131072; //!< all strings of all menus of this client (a 256-item menu with labels and
                                        //!< descriptions at their longest is about 63 KB)
constexpr int MAX_PICTURE_FILE = 1 << 20;
constexpr int MAX_PICTURE_SIDE = 1024;
constexpr int MAX_PREVIEW_MODELS = 64; //!< distinct model files the preview loads per map
constexpr int MAX_MODEL_PATHS = 64;    //!< model paths of one menu (MODELPATH)
constexpr int MAX_COOLDOWN_TENTHS = 65535; //!< a COOLDOWN's longest time, in tenths of a second (about 109 minutes)

// String lengths (bytes, without the NUL)
constexpr int LEN_TITLE = 63;
constexpr int LEN_TAB = 23;
constexpr int LEN_PATH = 63;
constexpr int LEN_LABEL = 119;  //!< an ITEM / LABEL label (version 3): a full-width list row at 1920 x 1080 (the
                                 //!< centered list with popups, about 1380 px at 14 px x 1.35) draws about 120
                                 //!< characters of plain text; and an ITEM with every string at its longest stays
                                 //!< within MSG_BYTES (185 bytes)
constexpr int LEN_LABEL_V2 = 47; //!< the label a version 2 client reads (the API cuts labels to it for such a client)
constexpr int LEN_PRICE = 23;
constexpr int LEN_BADGE = 15;
constexpr int LEN_ACTION = 15;
constexpr int LEN_TEXT = 127;
constexpr int LEN_HEADER = 95;
constexpr int LEN_BUTTON = 23;
constexpr int LEN_HUD_TITLE = 63;
constexpr int LEN_HUD_LINE = 47;   //!< the HUD strip's sub line and up-next line
constexpr int LEN_FIELD_LABEL = 23;
constexpr int LEN_FIELD_HINT = 31; //!< the placeholder
constexpr int LEN_SLIDER_LABEL = 23;
constexpr int LEN_TOGGLE_LABEL = 31;
constexpr int LEN_ITEMLINE = 63;
constexpr int LEN_BADGE_TIP = 31; //!< a BADGES tooltip
constexpr int LEN_CONTROL_TIP = 63; //!< a button's, slider's or switch's tooltip
constexpr int LEN_ITEMTITLE = 127; //!< an ITEMTITLE
constexpr int LEN_MAP = 31;      //!< a MAPPICTURE map name (lower case letters, digits, '_', '-', '.')
constexpr int LEN_FIELD_TEXT = 127; //!< what the player types (the field's max length is at most this)
constexpr int LEN_SELECT_LABEL = 23;
constexpr int LEN_OPTION = 23;   //!< a dropdown option
constexpr int LEN_CONFIRM = 95;  //!< a CONFIRM question
constexpr int LEN_STARS = 47;    //!< a STARS text

// The client's answers (one command, numbers only):
//   servermenu hello <version> <on>
//   servermenu pick <menu> <serial> <item>
//   servermenu button <menu> <serial> <button> <item>   (item 65535: none highlighted)
//   servermenu back <menu> <serial>
//   servermenu close <menu> <serial>
//   servermenu text <menu> <serial> <field> "<text>"     (no quotes, semicolons or control characters in text)
//   servermenu slider <menu> <serial> <slider> <value>  (on release of a drag; the value in the slider's range)
//   servermenu toggle <menu> <serial> <toggle> <0|1>    (a click: the state the player wants)
//   servermenu select <menu> <serial> <select> <option> (a dropdown's option the player chose)
constexpr int NO_ITEM = 65535;
} // namespace servermenu

#endif // SBO_GAME_SHARED_SERVER_MENU_SHARED_H
