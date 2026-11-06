//========= SBO 2.0: server-driven menus, the window =========//
//
// Purpose: see sbo_server_menu.h. The look (docs/SERVER_MENUS.md) is
//          drawn for a 1280 x 800 screen and scaled, in the colors of sbo_ui_style.h.
//
//          Centered window (1000 x 660): title, the first header line in a box ("Label: value"), the other lines next
//          to it, Close; tabs with the search box at their right; the card grid (4 columns) and a side panel with the
//          highlighted item's picture or model preview, name, description, price line and the primary button; a key
//          hint at the bottom.
//          Popup window (MENU_POPUP): centered, 1100 x 720 (1000 x
//          620 with MENU_LARGE), no side panel: the grid takes the width (5 columns; MENU_LARGE from 3 columns up, all cards in view down to the smallest card that holds its lines; 16:9 pictures),
//          the menu's buttons in the bottom row; a click on a card opens the item popup (its map picture large, label,
//          ITEMLINE, text or the map's briefing, Back and the action).
//          Side window (470 wide, docked right; MENU_SIDE): title and Close, the header lines, tabs, search, the grid
//          (3 columns), the menu's buttons and the primary button. With MENU_PREVIEW the screen left of it shows the
//          model preview, turned by dragging.
//
//          Input as the new loadout menu (sbo_loadout_menu.cpp): the window takes the mouse, not the keyboard, so the
//          player's keys keep working (chat, console; Esc closes the window: OnThink); 1-9 pick the n-th visible item (the
//          menu's SlotInput, as the loadout's slot keys). The keyboard is taken only while the search box is typed in
//          (a click on it; Enter or a click elsewhere gives it back; the box is a text entry: drag to select, Ctrl+A).
//          The mouse: a click highlights (the preview follows), a double click or the primary button picks, the
//          wheel or the scroll bar (dragged) scrolls.
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <FileSystem.h>
#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>

#include "client_viewport.h"
#include "sbo_server_menu.h"
#include "sbo_server_menu_proto.h"
#include "vgui2_api.h" // VGui2_Spectator_SetSuppressed
#include "hud_chat.h"  // IsSafeChatURL, HudChat_OpenURL: the popup text's links open as chat links do
#include "client_steam_context.h"
#include "sbo_bridge.h" // sbo::GetScorePlayer (VOTERS: names and SteamIDs)
#include "sbo_map_picture.h"
#include "sbo_image_decode.h" // ITEMPICTURE files
#include "sbo_server_browser.h" // CSboBrowserEntry
#include "sbo_ui_style.h"
#include "sbo_widgets.h"
#include "avatar_image.h" // the scoreboard's Steam avatars (any SteamID: Steam fetches players who left too)
#include "../server_menu_preview.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace sbolook;
using namespace servermenu;

namespace
{
// the design's colors not in sbo_ui_style.h
const Color C_BADGE_BG(58, 47, 18, 255);    // #3a2f12
const Color C_BADGE_TEXT(227, 195, 107, 255); // #e3c26b
const Color C_DIM_TEXT(92, 111, 87, 255);   // #5c6f57
const Color C_SHADE(0, 0, 0, 110);
const Color C_HINT_BG(12, 15, 12, 180);     // rgba(12,15,12,0.7)
const Color C_BACKER = COL_BG_SELECTED;     // the 3D models' backer (#1d3a18), opaque
const Color C_COOL_SHADE(8, 10, 8, 150);    // a cooling item's card, greyed

struct Rect
{
	int x = 0, y = 0, w = 0, h = 0;
	bool Has(int px, int py) const { return w > 0 && h > 0 && px >= x && px < x + w && py >= y && py < y + h; }
};

//! A screen rectangle in fractions of the screen (the model preview's frames)
ServerMenuPreviewRect ScreenFrac(const Rect &r, int sw, int sh)
{
	const float fw = (float)std::max(1, sw), fh = (float)std::max(1, sh);
	return { r.x / fw, r.y / fh, (r.x + r.w) / fw, (r.y + r.h) / fh };
}

//! Fills r except the hole (the model preview shows through it)
void FillExcept(const Rect &r, const Rect &hole, Color c)
{
	if (hole.w <= 0 || hole.h <= 0)
	{
		FillRect(r.x, r.y, r.w, r.h, c);
		return;
	}
	const int x0 = std::max(r.x, hole.x), x1 = std::min(r.x + r.w, hole.x + hole.w);
	const int y0 = std::max(r.y, hole.y), y1 = std::min(r.y + r.h, hole.y + hole.h);
	if (x0 >= x1 || y0 >= y1)
	{
		FillRect(r.x, r.y, r.w, r.h, c);
		return;
	}
	FillRect(r.x, r.y, r.w, y0 - r.y, c);
	FillRect(r.x, y1, r.w, r.y + r.h - y1, c);
	FillRect(r.x, y0, x0 - r.x, y1 - y0, c);
	FillRect(x1, y0, r.x + r.w - x1, y1 - y0, c);
}

//! Fills r except the holes (the 3D models show through them): band by band between the holes' edges
void FillHoles(const Rect &r, const std::vector<Rect> &holes, Color c)
{
	std::vector<int> ys{ r.y, r.y + r.h };
	for (const Rect &h : holes)
	{
		ys.push_back(std::max(r.y, std::min(r.y + r.h, h.y)));
		ys.push_back(std::max(r.y, std::min(r.y + r.h, h.y + h.h)));
	}
	std::sort(ys.begin(), ys.end());
	ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
	for (size_t b = 0; b + 1 < ys.size(); b++)
	{
		const int y0 = ys[b], y1 = ys[b + 1];
		std::vector<std::pair<int, int>> cut;
		for (const Rect &h : holes)
		{
			if (h.w > 0 && h.h > 0 && h.y <= y0 && h.y + h.h >= y1)
				cut.push_back({ std::max(r.x, h.x), std::min(r.x + r.w, h.x + h.w) });
		}
		std::sort(cut.begin(), cut.end());
		int x = r.x;
		for (const auto &cx : cut)
		{
			if (cx.first > x)
				FillRect(x, y0, cx.first - x, y1 - y0, c);
			x = std::max(x, cx.second);
		}
		if (x < r.x + r.w)
			FillRect(x, y0, r.x + r.w - x, y1 - y0, c);
	}
}

//! Server text for the screen: a "#token" from the localization files, else the text without color codes
std::wstring Text(const std::string &s)
{
	if (!s.empty() && s[0] == '#' && g_pVGuiLocalize)
	{
		if (const wchar_t *w = g_pVGuiLocalize->Find(s.c_str()))
			return w;
	}
	return Widen(CleanName(s.c_str()).c_str());
}

std::wstring LowerW(std::wstring s)
{
	for (wchar_t &c : s)
		c = (wchar_t)towlower(c);
	return s;
}

void DrawX(int cx, int cy, int size, Color c)
{
	vgui2::surface()->DrawSetColor(c);
	const int h = size / 2;
	for (int i = -h; i <= h; i++)
	{
		vgui2::surface()->DrawFilledRect(cx + i, cy + i, cx + i + 2, cy + i + 1);
		vgui2::surface()->DrawFilledRect(cx + i, cy - i, cx + i + 2, cy - i + 1);
	}
}
//! A player's Steam avatar (AVATAR, HUDBAR), square, as the scoreboard draws it: the default "?" until Steam has it.
//! One image per SteamID, kept for the session (as the scoreboard's). The IDs come from the server: only individual
//! accounts (as CScorePanel::GetValidSteamID64), and at most AVATAR_IMAGES of them (each a Steam request and a
//! texture); past that a new one draws nothing.
constexpr size_t AVATAR_IMAGES = 256;
void DrawAvatar(unsigned long long steamID64, int x, int y, int size)
{
	static std::map<unsigned long long, CAvatarImage *> s_Avatars;
	if (!steamID64 || size <= 0 || ((steamID64 >> 52) & 0xF) != 1 || (steamID64 & 0xFFFFFFFFull) == 0)
		return;
	auto it = s_Avatars.find(steamID64);
	if (it == s_Avatars.end())
	{
		if (s_Avatars.size() >= AVATAR_IMAGES)
			return;
		CAvatarImage *pAvatar = new CAvatarImage();
		pAvatar->SetDrawFriend(false);
		pAvatar->SetAvatarSteamID(CSteamID((uint64)steamID64));
		it = s_Avatars.insert({ steamID64, pAvatar }).first;
	}
	CAvatarImage *pAvatar = it->second;
	pAvatar->SetPos(x, y);
	pAvatar->SetOffset(0, 0);
	pAvatar->SetSize(size, size);
	pAvatar->SetColor(Color(255, 255, 255, 255));
	pAvatar->Paint();
}

} // namespace

class CSboServerMenu;

//! The search box and the menu's text fields: a click on one gives the window the keyboard (the window takes only
//! the mouse otherwise)
class CSboServerMenuSearch : public CSboBrowserEntry
{
	DECLARE_CLASS_SIMPLE(CSboServerMenuSearch, CSboBrowserEntry);

public:
	CSboServerMenuSearch(CSboServerMenu *pMenu, const char *name = "ServerMenuSearch", const char *hint = "Type a name")
	    : BaseClass((vgui2::Panel *)nullptr, name, hint), m_pMenu(pMenu) {}
	void OnMousePressed(vgui2::MouseCode code) override;

private:
	CSboServerMenu *m_pMenu;
};

//-----------------------------------------------------------------------------
// The items (clipped and scrolled): painted and handled by the window
//-----------------------------------------------------------------------------
class CSboServerMenuArea : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboServerMenuArea, vgui2::Panel);

public:
	CSboServerMenuArea(CSboServerMenu *pMenu);
	void Paint() override;
	void PaintBackground() override {}
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseDoublePressed(vgui2::MouseCode code) override;
	void OnMouseReleased(vgui2::MouseCode code) override;
	void OnCursorMoved(int x, int y) override;
	void OnCursorExited() override;
	void OnMouseWheeled(int delta) override;

private:
	CSboServerMenu *m_pMenu;
};

//-----------------------------------------------------------------------------
// The item popup (MENU_POPUP): over the whole window, above the item area and the entries
//-----------------------------------------------------------------------------
class CSboServerMenuPopup : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboServerMenuPopup, vgui2::Panel);

public:
	CSboServerMenuPopup(CSboServerMenu *pMenu);
	void Paint() override;
	void PaintBackground() override {}
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseDoublePressed(vgui2::MouseCode code) override;
	void OnMouseReleased(vgui2::MouseCode code) override;
	void OnMouseWheeled(int delta) override;
	void OnCursorMoved(int x, int y) override;

private:
	CSboServerMenu *m_pMenu;
};

//-----------------------------------------------------------------------------
// Over everything else (the window, the item popup): an open dropdown's list and the confirm dialog
//-----------------------------------------------------------------------------
class CSboServerMenuOverlay : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboServerMenuOverlay, vgui2::Panel);

public:
	CSboServerMenuOverlay(CSboServerMenu *pMenu);
	void Paint() override;
	void PaintBackground() override {}
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnCursorMoved(int x, int y) override;

private:
	CSboServerMenu *m_pMenu;
};

class CSboServerMenu : public CSboMenuPanel
{
	DECLARE_CLASS_SIMPLE(CSboServerMenu, CSboMenuPanel);

public:
	static CSboServerMenu *Get();
	//! The window exists and is up (never creates it)
	static bool Shown() { return s_pInstance && s_pInstance->IsVisible(); }
	//! A docked window is up: its left edge (the HUD strip stays left of it), else -1
	int DockedLeft() { return IsVisible() && Side() ? m_rWin.x : -1; }

	void Show(int menu, int tab, int item);
	void Hide();
	void Changed(int menu);

	void Paint() override;
	void PaintBackground() override;
	void OnThink() override;
	void OnMousePressed(vgui2::MouseCode code) override;
	void OnMouseReleased(vgui2::MouseCode code) override;
	void OnCursorMoved(int x, int y) override;
	void OnKeyCodeTyped(vgui2::KeyCode code) override;
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
	MESSAGE_FUNC_PTR(OnTextNewLine, "TextNewLine", panel);
	bool SlotInput(int iSlot);
	//! VGui2_ServerMenu_KeyInput
	bool KeyInput(int key);


	// the item popup
	void PaintPopup();
	void PopupPressed(int x, int y, bool dbl);
	void PopupMoved(int x, int y);
	void PopupWheeled(int delta);
	void PopupReleased();
	//! The popup text's scroll bar thumb (screen coordinates; w 0 when all the text fits)
	Rect PopupThumb() const;

	// the dropdown list and the confirm dialog (the overlay)
	void PaintOverlay();
	void OverlayPressed(int x, int y);
	void OverlayMoved(int x, int y);

	// the item area
	void PaintArea();
	void AreaPressed(int x, int y, bool dbl);
	void AreaMoved(int x, int y);
	void AreaWheeled(int delta);
	void AreaReleased();
	void AreaDragged(int y);

protected:
	void LayoutMenu() override;

private:
	CSboServerMenu(vgui2::Panel *pParent);
	int P(float mock) const { return (int)lroundf(mock * m_flK); }
	vgui2::HFont Font(float px, int weight = 400) const;
	const Menu *M() const { return ServerMenu_Store().Get(m_iMenu); }
	//! LAYOUT_STARS: the compact centered star rating (MENU_SIDE, MENU_PREVIEW and MENU_POPUP do not apply)
	bool Stars() const { const Menu *m = M(); return m && m->layout == LAYOUT_STARS; }
	bool Side() const { const Menu *m = M(); return m && (m->flags & MENU_SIDE) && m->layout != LAYOUT_STARS; }
	bool HasPreview() const { const Menu *m = M(); return m && (m->flags & MENU_PREVIEW) && m->layout != LAYOUT_STARS; }
	//! LAYOUT_STARS: the window's layout, its paint (title, headers, the average row, buttons, hint) and the stars
	void LayoutStars();
	void PaintStars();
	void PaintStarArea();
	int m_iStarsY = 0;   //!< the average row's top and height (0: none)
	int m_iStarsH = 0;
	int m_iStarSize = 0; //!< a star's box in the item area

	void Refilter();
	void LayoutArea();
	void Highlight(int item, bool scrollTo);
	void SetTab(int tab);
	void Pick(int item);
	void Button(int button);
	void Back();
	//! A text field's Enter or button: "servermenu text <menu> <serial> <field> "<text>"", the field emptied
	void SubmitField(int field);
	void UpdatePreview();
	bool CanPick(int item) const;
	std::wstring PrimaryLabel() const;
	//! An item of the menu has an action (the side window keeps room for the primary button)
	bool AnyAction() const;
	//! The primary button is drawn and clickable: the highlighted item has an action
	bool PrimaryShown() const;
	void PaintCard(int item, const Rect &r);
	//! An ITEMPICTURE file in box, whole (its shape kept), cut to [0, clipW) x [0, clipH); the placeholder (the
	//! empty box) while the client has no such file
	void PaintFilePicture(const std::string &path, const Rect &box, bool off, int clipW, int clipH);
	struct FileImage
	{
		int texture = -1, wide = 0, tall = 0;
		float retryAt = 0.0f; //!< a missing file is looked for again from then (a download may bring it)
	};
	std::map<std::string, FileImage> m_FileImages;
	const FileImage *GetFileImage(const std::string &path);
	//! A dropdown's row (a menu control: a caption and the box; in the popup: the same) and its list
	void PaintSelect(int sel, const Rect &row, const Rect &box, bool hot);
	void OpenDrop(int sel, const Rect &box);
	void CloseOverlay();
	int SelectShown(int sel) const;
	//! The confirm step: the question for an item's pick or a button (with the item highlighted then)
	bool AskConfirm(int kind, int index, int item, const std::string &question);
	void ConfirmAnswer(bool yes);
	void SendPick(int item);
	void SendButton(int button, int item);
	//! box in coordinates clipped to [0, clipW) x [0, clipH) (the item area, or the window for the popup)
	void PaintMapPicture(const std::string &map, const Rect &box, bool off, int clipW, int clipH, bool large = false);
	void OpenPopup(int item);
	void ClosePopup();
	void LayoutPopup();
	//! The popup's text: a map item's briefing (maps/<map>.txt, as the joining screen reads it) if this client has
	//! the map, else the item's description
	const std::wstring &PopupText(const Item &it);
	//! The popup's header: the label, or with a full title (ITEMTITLE, shown whole in the text) that title cut to the
	//! label's length (a list row's label may carry more, such as its position)
	static std::wstring PopupHeader(const Item &it);
	void MapPictureDownloads();
	//! The card's own 3D model (MENU_PREVIEW): its player model (layer 0), else its first costume layer
	const Model *CardModel(int item, bool &player) const;
	bool CardPictures() const;
	int PictureTall() const { return m_iPictureTall; }
	//! MENU_POPUP: a click on an item opens its popup (a grid's or a list's, centered or docked)
	bool Popups() const { const Menu *m = M(); return m && (m->flags & MENU_POPUP) && m->layout != LAYOUT_STARS; }
	//! Popups in the centered window's own layout (no side panel, the buttons in the bottom row)
	bool CenteredPopups() const { return Popups() && !Side(); }
	//! The picture box of the n-th visible card in the item area's coordinates (scrolled)
	Rect CardBox(size_t n) const;
	//! The card's model is drawn: it has one and its box is wholly inside the item area
	bool CardModelShown(size_t n) const;
	//! The card's picture box on the screen, and its part inside the item area
	Rect CardScreenBox(size_t n) const;
	Rect CardScreenClip(size_t n) const;
	//! The big preview's frame on the screen (the centered window's box, or a frame left of the side window)
	Rect PreviewStage() const;
	void PaintRow(int item, const Rect &r);
	void PaintButton(const Rect &r, const std::wstring &label, bool primary, bool enabled, bool hot);
	void PaintHeader(int x, int y, int maxW, const std::string &line, vgui2::HFont font);

	static CSboServerMenu *s_pInstance;

	CSboServerMenuArea *m_pArea = nullptr;
	CSboMapPictures m_MapPictures; //!< MAPPICTURE cards: the map pictures, as the create server window's grid
	int m_iNewMapPictures = 0;     //!< map pictures made in this paint (a JPEG decode each; at most 2 a frame)
	std::vector<std::pair<Rect, std::wstring>> m_Tips; //!< this paint's hover texts (BADGES, VOTERS), area coordinates
	int m_iAreaMouseX = -1, m_iAreaMouseY = -1;        //!< the mouse in the item area (-1: outside)
	//! A card's badge row: the BADGES (else the STATE badge) from the left, the VOTERS bubbles from the right
	void PaintBadgeRow(const Item &it, int x, int y, int w, bool picked, Color backer);
	int m_iPictureTall = 0;        //!< a card's picture box (LayoutArea)
	CSboServerMenuPopup *m_pPopup = nullptr;
	CSboServerMenuOverlay *m_pOverlay = nullptr;
	int m_iDrop = -1;              //!< the dropdown whose list is open, -1 none
	Rect m_rDrop;                  //!< its list (window coordinates)
	std::vector<Rect> m_rDropOptions;
	int m_iDropHover = -1;
	int m_iConfirmKind = -1;       //!< CONFIRM_ITEM / CONFIRM_BUTTON while the dialog is up, -1 none
	int m_iConfirmIndex = -1, m_iConfirmItem = -1;
	std::wstring m_ConfirmText;
	Rect m_rConfirm, m_rConfirmYes, m_rConfirmNo;
	std::vector<std::wstring> m_ConfirmLines;
	int m_iOverlayX = -1, m_iOverlayY = -1;
	bool m_bConfirmNoFocus = false; //!< the keyboard's choice in the dialog (Yes unless moved to No)

	// keyboard navigation (KeyInput): what Tab and the arrows move between
	enum FocusKind
	{
		F_ITEMS,
		F_TABS,
		F_SEARCH,
		F_FIELD,
		F_SLIDER,
		F_TOGGLE,
		F_SELECT,
		F_BUTTON,
		F_BACK,
		F_PRIMARY,
		F_POP_SELECT, //!< in the item popup
		F_POP_BACK,
		F_POP_PRIMARY,
	};
	struct Focus
	{
		int kind = F_ITEMS, index = 0;
		bool operator==(const Focus &o) const { return kind == o.kind && index == o.index; }
	};
	Focus m_Focus, m_PopFocus;
	bool m_bKeyNav = false; //!< a key moved the focus since the last click: its ring is drawn
	bool m_bEscGuard = false; //!< the viewport's menu Esc guard is on (OnThink)
	std::vector<Focus> FocusList(bool popup) const;
	Rect FocusRect(const Focus &f) const;
	void MoveFocus(bool popup, int step);
	void ActivateFocus(bool popup, int key);
	void MoveHighlight(int dx, int dy);
	void FlipToggle(int t);
	void SendSlider(int sl, int value);
	void ChooseOption(int option);
	void PaintFocusRing(const Rect &r);
	Rect m_rSelects[MAX_MENU_SELECTS], m_rSelectBoxes[MAX_MENU_SELECTS];       //!< the menu's dropdowns (item NO_ITEM)
	Rect m_rPopSelects[MAX_MENU_SELECTS], m_rPopSelectBoxes[MAX_MENU_SELECTS]; //!< the popup item's dropdowns
	int m_iPopup = -1;             //!< the item whose popup is open, -1 none
	float m_flPopupAt = 0.0f;      //!< when it opened (a double click's second press lands on the popup)
	Rect m_rPop, m_rPopClose, m_rPopPicture, m_rPopBack, m_rPopPrimary;
	Rect m_rPopText;               //!< the popup's text area (its scroll bar at the right edge)
	std::vector<std::wstring> m_PopTitle; //!< the popup's label wrapped (never cut; LayoutPopup)
	int m_iPopTitleH = 0;          //!< its height (at least Close's)
	std::vector<std::wstring> m_PopLines; //!< the popup's text wrapped to the area (LayoutPopup)
	std::wstring m_PopWrapText;    //!< the text and width m_PopLines were wrapped from (wrapped again when they change)
	//! The popup text's links: each http(s) URL shown as its short label (the host and the path's start)
	struct PopLink
	{
		std::wstring label;
		std::string url;
	};
	std::vector<PopLink> m_PopLinks;
	std::vector<std::pair<Rect, size_t>> m_PopLinkRects; //!< this paint's link labels (screen) and their m_PopLinks
	//! A popup text line with its link labels drawn as links (and their rectangles kept for clicks)
	void PaintPopupLine(vgui2::HFont font, int x, int y, const std::wstring &line);
	int m_iPopWrapW = -1;
	int m_iPopScroll = 0;          //!< the first line shown
	int m_iPopShown = 0;           //!< lines the area shows
	bool m_bPopBarDrag = false;
	int m_iPopBarDragY = 0, m_iPopBarDragScroll = 0;
	std::string m_BriefingMap;     //!< PopupText's cache: the map read last and its text
	std::wstring m_Briefing;
	float m_flK = 1.0f;
	int m_iMenu = -1;
	int m_iSerial = -1;
	int m_iTab = 0;
	int m_iHighlight = -1;
	int m_iHover = -1;
	std::wstring m_Search; //!< the search box's text
	CSboBrowserEntry *m_pSearch = nullptr;
	CSboBrowserEntry *m_pFields[MAX_MENU_FIELDS] = {};
	Rect m_rFields[MAX_MENU_FIELDS], m_rFieldButtons[MAX_MENU_FIELDS];
	Rect m_rSliders[MAX_MENU_SLIDERS], m_rSliderTracks[MAX_MENU_SLIDERS];
	int m_iSliderDrag = -1;   //!< the slider being dragged (its value is shown from here until the release)
	int m_iSliderValue = 0;
	int SliderValueAt(int slider, int x) const;
	//! A value sent to the server and shown until it answers (its VALUE record) or 2 s pass (then the store's)
	struct Sent
	{
		bool active = false;
		int value = 0, updates = 0;
		float at = 0.0f;
	};
	Sent m_SliderSent[MAX_MENU_SLIDERS], m_ToggleSent[MAX_MENU_TOGGLES], m_SelectSent[MAX_MENU_SELECTS];
	Rect m_rToggles[MAX_MENU_TOGGLES];
	int SliderShown(int slider) const;
	bool ToggleShown(int toggle) const;
	static bool SentShows(const Sent &sent, int updates)
	{
		return sent.active && sent.updates == updates && ServerMenu_Now() - sent.at < 2.0f &&
		       ServerMenu_Now() >= sent.at;
	}
	bool m_bBarDrag = false;
	int m_iBarDragY = 0, m_iBarDragScroll = 0;
public:
	//! The keyboard to an entry (the search box, else the one clicked)
	void BeginSearchTyping(vgui2::Panel *pEntry = nullptr);
	void EndSearchTyping();

private:
	std::vector<int> m_Visible;
	std::vector<Rect> m_Cards; //!< in the area's coordinates before scrolling, one per m_Visible
	std::vector<int> m_CardSlots; //!< per m_Visible: its model's index in ServerMenuPreview_SetCards, -1 none
	int m_iScroll = 0, m_iContent = 0;
	int m_iColumns = 1;
	float m_flYaw = 0.0f;
	bool m_bDragging = false;
	int m_iDragX = 0;
	float m_flDragYaw = 0.0f;
	int m_iMouseX = -1, m_iMouseY = -1;

	// layout (screen pixels)
	Rect m_rWin, m_rClose, m_rSearch, m_rArea, m_rPrimary, m_rBack, m_rPreview, m_rPanel, m_rHint;
	std::vector<Rect> m_rTabs, m_rButtons;
	int m_iHeaderY = 0;
};

CSboServerMenu *CSboServerMenu::s_pInstance = nullptr;

//-----------------------------------------------------------------------------
// CSboServerMenuArea
//-----------------------------------------------------------------------------
CSboServerMenuArea::CSboServerMenuArea(CSboServerMenu *pMenu)
    : BaseClass(pMenu, "ServerMenuArea")
    , m_pMenu(pMenu)
{
	SetPaintBackgroundEnabled(false);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(false);
}

void CSboServerMenuArea::Paint() { m_pMenu->PaintArea(); }

void CSboServerMenuSearch::OnMousePressed(vgui2::MouseCode code)
{
	m_pMenu->BeginSearchTyping(this);
	BaseClass::OnMousePressed(code);
}

void CSboServerMenuArea::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	m_pMenu->AreaPressed(x, y, false);
}

void CSboServerMenuArea::OnMouseDoublePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	m_pMenu->AreaPressed(x, y, true);
}

void CSboServerMenuArea::OnCursorMoved(int x, int y) { m_pMenu->AreaMoved(x, y); }
void CSboServerMenuArea::OnCursorExited() { m_pMenu->AreaMoved(-1, -1); }
void CSboServerMenuArea::OnMouseReleased(vgui2::MouseCode code) { m_pMenu->AreaReleased(); }
void CSboServerMenuArea::OnMouseWheeled(int delta) { m_pMenu->AreaWheeled(delta); }

CSboServerMenuPopup::CSboServerMenuPopup(CSboServerMenu *pMenu)
    : BaseClass(pMenu, "ServerMenuPopup")
    , m_pMenu(pMenu)
{
	SetPaintBackgroundEnabled(false);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(false);
}

void CSboServerMenuPopup::Paint() { m_pMenu->PaintPopup(); }

void CSboServerMenuPopup::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	m_pMenu->ScreenToLocal(x, y);
	m_pMenu->PopupPressed(x, y, false);
}

void CSboServerMenuPopup::OnMouseDoublePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	m_pMenu->ScreenToLocal(x, y);
	m_pMenu->PopupPressed(x, y, true);
}

void CSboServerMenuPopup::OnMouseReleased(vgui2::MouseCode code) { m_pMenu->PopupReleased(); }
void CSboServerMenuPopup::OnMouseWheeled(int delta) { m_pMenu->PopupWheeled(delta); }

CSboServerMenuOverlay::CSboServerMenuOverlay(CSboServerMenu *pMenu)
    : BaseClass(pMenu, "ServerMenuOverlay")
    , m_pMenu(pMenu)
{
	SetPaintBackgroundEnabled(false);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(false);
}

void CSboServerMenuOverlay::Paint() { m_pMenu->PaintOverlay(); }

void CSboServerMenuOverlay::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	m_pMenu->ScreenToLocal(x, y);
	m_pMenu->OverlayPressed(x, y);
}

void CSboServerMenuOverlay::OnCursorMoved(int x, int y)
{
	LocalToScreen(x, y);
	m_pMenu->ScreenToLocal(x, y);
	m_pMenu->OverlayMoved(x, y);
}

void CSboServerMenuPopup::OnCursorMoved(int x, int y)
{
	LocalToScreen(x, y);
	m_pMenu->ScreenToLocal(x, y);
	m_pMenu->PopupMoved(x, y);
}

//-----------------------------------------------------------------------------
// CSboServerMenu
//-----------------------------------------------------------------------------
CSboServerMenu *CSboServerMenu::Get()
{
	if (!s_pInstance && g_pViewport)
		s_pInstance = new CSboServerMenu(g_pViewport);
	return s_pInstance;
}

CSboServerMenu::CSboServerMenu(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboServerMenu")
{
	SetBackdropTrans(0); // painted here (PaintBackground)
	m_pArea = new CSboServerMenuArea(this);
	m_pPopup = new CSboServerMenuPopup(this);
	m_pPopup->SetVisible(false);
	m_pOverlay = new CSboServerMenuOverlay(this);
	m_pOverlay->SetVisible(false);
	m_pSearch = new CSboServerMenuSearch(this);
	m_pSearch->SetParent(this);
	m_pSearch->SetMaximumCharCount(32);
	m_pSearch->SendNewLine(true);
	m_pSearch->AddActionSignalTarget(this);
	if (vgui2::HScheme h = BrowserScheme())
		m_pSearch->SetScheme(h);
	m_pSearch->SetVisible(false);
	for (int f = 0; f < MAX_MENU_FIELDS; f++)
	{
		m_pFields[f] = new CSboServerMenuSearch(this, f ? "ServerMenuField1" : "ServerMenuField0", "");
		m_pFields[f]->SetParent(this);
		m_pFields[f]->SetMaximumCharCount(LEN_FIELD_TEXT);
		m_pFields[f]->SendNewLine(true);
		m_pFields[f]->AddActionSignalTarget(this);
		if (vgui2::HScheme h = BrowserScheme())
			m_pFields[f]->SetScheme(h);
		m_pFields[f]->SetVisible(false);
	}
}

vgui2::HFont CSboServerMenu::Font(float px, int weight) const
{
	// the loadout menu's rule: no text below 12 pixels; the rows make room for it
	const float MIN_EM = 12.0f;
	const float em = std::max(px * m_flK, std::min(px, MIN_EM));
	return MakeFontFace(SboUiFontFace(), std::max(6, (int)lroundf(em * 1.2f)), weight);
}

void CSboServerMenu::Show(int menu, int tab, int item)
{
	const Menu *m = ServerMenu_Store().Get(menu);
	if (!m)
		return;
	const bool fresh = !IsVisible() || menu != m_iMenu;
	m_iMenu = menu;
	m_iSerial = m->serial;
	m_iTab = m->tabs.empty() ? 0 : std::max(0, std::min(tab, (int)m->tabs.size() - 1));
	if (fresh)
	{
		m_Search.clear();
		m_pSearch->SetText("");
		for (CSboBrowserEntry *pField : m_pFields)
			pField->SetText("");
		for (Sent &sent : m_SliderSent)
			sent = Sent();
		for (Sent &sent : m_ToggleSent)
			sent = Sent();
		for (Sent &sent : m_SelectSent)
			sent = Sent();
		CloseOverlay();
		m_Focus = Focus();
		m_bKeyNav = false;
		m_iScroll = 0;
		m_flYaw = 0.0f;
		m_bDragging = false;
		ClosePopup();
	}
	if (!IsVisible())
	{
		ShowMenu(); // the mouse only, as the loadout menu
		VGui2_Spectator_SetSuppressed(true); // the spectator bars and texts go while a server menu is up
	}
	LayoutMenu();
	// the server's highlighted item if it is on this tab, else the first one there
	m_iHighlight = -1;
	Highlight(item >= 0 && item < (int)m->items.size() && std::find(m_Visible.begin(), m_Visible.end(), item) !=
	          m_Visible.end() ? item : (m_Visible.empty() ? -1 : m_Visible[0]), true);
	UpdatePreview();
}

void CSboServerMenu::Hide()
{
	if (!IsVisible())
		return;
	EndSearchTyping();
	// closed while the model or the scroll bar was dragged (Esc, the server's CLOSE): the mouse capture goes too,
	// else the hidden window keeps the mouse from the next menu
	if (m_bDragging || m_bBarDrag || m_iSliderDrag >= 0)
		vgui2::input()->SetMouseCapture(0);
	m_bDragging = false;
	m_bBarDrag = false;
	m_iSliderDrag = -1;
	m_iMouseX = m_iMouseY = -1; // no hover (highlight, hover text) left for the next opening
	m_iAreaMouseX = m_iAreaMouseY = -1;
	CloseOverlay();
	ClosePopup();
	if (m_bEscGuard && g_pGameUIViewport)
		g_pGameUIViewport->SetMenuEscapeGuard(false);
	m_bEscGuard = false;
	HideMenu();
	VGui2_Spectator_SetSuppressed(false);
	ServerMenuPreviewSpec spec;
	memset(&spec, 0, sizeof(spec));
	ServerMenuPreview_Set(spec);
	ServerMenuPreview_SetCards(nullptr, 0);
	ServerMenuPreview_SetHideViewModel(false);
}

void CSboServerMenu::Changed(int menu)
{
	if (!IsVisible() || menu != m_iMenu)
		return;
	const Menu *m = M();
	if (!m)
	{
		ServerMenu_PlayerClosed();
		return;
	}
	if (m->serial != m_iSerial)
	{
		// a new definition: in place, keeping the tab, the search and the highlighted item where they still exist
		m_iSerial = m->serial;
		m_iTab = m->tabs.empty() ? 0 : std::min(m_iTab, (int)m->tabs.size() - 1);
		CloseOverlay(); // its dropdown or question may be gone
	}
	// a question taken back (CONFIRM ""): the dialog goes
	if (m_iConfirmKind == CONFIRM_ITEM && (m_iConfirmIndex >= (int)m->items.size() || m->items[m_iConfirmIndex].confirm.empty()))
		CloseOverlay();
	else if (m_iConfirmKind == CONFIRM_BUTTON &&
	         (m_iConfirmIndex >= (int)m->buttonConfirms.size() || m->buttonConfirms[m_iConfirmIndex].empty()))
		CloseOverlay();
	const int keep = m_iHighlight;
	LayoutMenu();
	if (m_iPopup >= 0 && (m_iPopup >= (int)m->items.size() || !m->items[m_iPopup].defined ||
	                      (m->items[m_iPopup].flags & ITEM_HIDDEN) || !Popups()))
		ClosePopup();
	if (std::find(m_Visible.begin(), m_Visible.end(), keep) == m_Visible.end())
		Highlight(m_Visible.empty() ? -1 : m_Visible[0], true);
	UpdatePreview();
}

void CSboServerMenu::Refilter()
{
	m_Visible.clear();
	const Menu *m = M();
	if (!m)
		return;
	const std::wstring search = LowerW(m_Search);
	for (int i = 0; i < (int)m->items.size(); i++)
	{
		const Item &it = m->items[i];
		if (!it.defined || (it.flags & ITEM_HIDDEN))
			continue;
		if (!m->tabs.empty() && it.tab != m_iTab && !(m_iTab == 0 && (m->flags & MENU_ALLTAB)))
			continue;
		if (!search.empty() && LowerW(Text(it.label)).find(search) == std::wstring::npos)
			continue;
		m_Visible.push_back(i);
	}
	if (m->layout == LAYOUT_STARS && m_Visible.size() > (size_t)MAX_STARS)
		m_Visible.resize(MAX_STARS); // a star rating shows at most MAX_STARS stars
}

void CSboServerMenu::LayoutMenu()
{
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	m_flK = std::min(sw / (float)MOCK_SCREEN_WIDE, sh / (float)MOCK_SCREEN_TALL);
	const Menu *m = M();
	if (!m)
		return;
	if (m->layout == LAYOUT_STARS)
	{
		LayoutStars();
		return;
	}
	const bool side = (m->flags & MENU_SIDE) != 0;
	const bool preview = (m->flags & MENU_PREVIEW) != 0;

	if (side)
	{
		m_rWin.w = P(470);
		m_rWin.x = sw - P(24) - m_rWin.w;
		m_rWin.y = P(24);
		m_rWin.h = sh - P(48);
	}
	else
	{
		// with popups (no side panel): the designs' 1100 x 720 (nominations) and 1000 x 620 (large cards: a vote)
		const bool popups = (m->flags & MENU_POPUP) != 0, large = (m->flags & MENU_LARGE) != 0;
		m_rWin.w = std::min(sw - P(16), P(popups && !large ? 1100 : 1000));
		m_rWin.h = std::min(sh - P(16), P(popups ? (large ? 620 : 720) : 660));
		m_rWin.x = (sw - m_rWin.w) / 2;
		m_rWin.y = (sh - m_rWin.h) / 2;
	}
	// the docked window is compact (the design's "Radio menu open": a smaller title and header, less padding)
	const int padX = P(side ? 18 : 24), padY = P(side ? 14 : 20), gap = P(side ? 9 : 14);
	const int innerX = m_rWin.x + padX, innerW = m_rWin.w - 2 * padX;
	int y = m_rWin.y + padY;

	// title row: Close
	const int titleH = std::max(P(side ? 28 : 32), FontTall(Font(side ? 22.0f : 28.0f)));
	m_rClose = Rect();
	if (!(m->flags & MENU_NOCLOSE))
		m_rClose = { m_rWin.x + m_rWin.w - padX - P(side ? 28 : 32), y + (titleH - P(side ? 28 : 32)) / 2,
			P(side ? 28 : 32), P(side ? 28 : 32) };
	y += titleH + gap;

	// side: the header lines under the title
	m_iHeaderY = y;
	if (side)
	{
		const int lineH = FontTall(Font(12.0f));
		for (const std::string &h : m->headers)
		{
			if (!h.empty())
				y += lineH + P(3);
		}
		if (y != m_iHeaderY)
			y += gap - P(4);
	}

	// tabs (wrapped in the side window), the search box at their right in the centered window
	m_rTabs.clear();
	m_rSearch = Rect();
	const vgui2::HFont tabFont = Font(side ? 13.0f : 14.0f);
	const int tabH = P(side ? 36 : 40);
	const int searchW = side ? 0 : P(260);
	if (!m->tabs.empty())
	{
		int tx = innerX, ty = y;
		const int maxX = innerX + innerW - searchW;
		for (const std::string &t : m->tabs)
		{
			const int w = TextWide(tabFont, Text(t)) + 2 * P(side ? 12 : 18);
			if (tx > innerX && tx + w > maxX && side)
			{
				tx = innerX;
				ty += tabH + P(2);
			}
			m_rTabs.push_back({ tx, ty, w, tabH });
			tx += w + P(2);
		}
		y = ty + tabH + gap;
	}
	if (m->flags & MENU_SEARCH)
	{
		if (side)
		{
			m_rSearch = { innerX, y, innerW, P(30) };
			y += P(30) + gap;
		}
		else
		{
			const int sy = m->tabs.empty() ? y : m_rTabs.back().y + tabH - P(36);
			m_rSearch = { innerX + innerW - searchW, sy, searchW, P(30) };
			if (m->tabs.empty())
				y += P(30) + gap;
		}
	}

	// the docked window: the sliders, the switches, then the text fields (the design's order); centered: the fields
	// first
	auto layoutFields = [&]() {
	// the text fields: a row each, its caption, the entry and its button (side: under the search box; centered:
	// under the tabs)
	for (int f = 0; f < MAX_MENU_FIELDS; f++)
	{
		m_rFields[f] = m_rFieldButtons[f] = Rect();
		if (f >= (int)m->fields.size())
		{
			m_pFields[f]->SetVisible(false);
			continue;
		}
		const Field &fd = m->fields[f];
		const int rowH = P(side ? 30 : 34);
		const vgui2::HFont capFont = Font(13.0f), btnFont = Font(13.0f);
		const int cw = fd.label.empty() ? 0 : TextWide(capFont, Text(fd.label)) + P(8);
		const int bw = fd.button.empty() ? 0 : TextWide(btnFont, Text(fd.button)) + 2 * P(16);
		m_rFields[f] = { innerX, y, innerW, rowH };
		if (bw)
			m_rFieldButtons[f] = { innerX + innerW - bw, y, bw, rowH };
		const int ex = innerX + cw, ew = std::max(P(40), innerW - cw - (bw ? bw + P(8) : 0));
		m_pFields[f]->SetBounds(ex, y, ew, rowH);
		m_pFields[f]->SetFont(Font(13.0f));
		m_pFields[f]->SetPlaceholder(fd.hint.c_str());
		m_pFields[f]->SetMaximumCharCount(fd.maxLength);
		m_pFields[f]->SetVisible(true);
		m_pFields[f]->SetMouseInputEnabled(true);
		y += rowH + gap;
	}

	};
	auto layoutSliders = [&]() {
	// the sliders: a row each, its caption, the track and the value
	for (int sl = 0; sl < MAX_MENU_SLIDERS; sl++)
	{
		m_rSliders[sl] = m_rSliderTracks[sl] = Rect();
		if (sl >= (int)m->sliders.size())
			continue;
		const vgui2::HFont f = Font(13.0f);
		const int rowH = P(side ? 22 : 30);
		const int cw = m->sliders[sl].label.empty() ? 0 : TextWide(f, Text(m->sliders[sl].label)) + P(12);
		const int vw = TextWide(f, L"65535") + P(12);
		m_rSliders[sl] = { innerX, y, innerW, rowH };
		m_rSliderTracks[sl] = { innerX + cw, y, std::max(P(40), innerW - cw - vw), rowH };
		y += rowH + gap;
	}

	};
	auto layoutToggles = [&]() {
	// the toggles: a row each, its caption and the switch at the right
	// (the docked window: a grid of 2 columns of compact rows, a small switch at each one's right)
	for (int t = 0; t < MAX_MENU_TOGGLES; t++)
	{
		m_rToggles[t] = Rect();
		if (t >= (int)m->toggles.size())
			continue;
		if (side)
		{
			const int colGap = P(14), cw = (innerW - colGap) / 2, rh = P(22);
			m_rToggles[t] = { innerX + (t % 2) * (cw + colGap), y, cw, rh };
			if (t % 2 == 1 || t + 1 == (int)m->toggles.size())
				y += rh + (t + 1 < (int)m->toggles.size() ? P(6) : gap);
			continue;
		}
		m_rToggles[t] = { innerX, y, innerW, P(30) };
		y += P(30) + gap;
	}

	};
	auto layoutSelects = [&]() {
	// the dropdowns (item NO_ITEM): a row each, its caption and the box at the right (as wide as its longest
	// option, at most 60% of the row)
	for (int sl = 0; sl < MAX_MENU_SELECTS; sl++)
	{
		m_rSelects[sl] = m_rSelectBoxes[sl] = Rect();
		if (sl >= (int)m->selects.size() || !m->selects[sl].defined || m->selects[sl].item != NO_ITEM)
			continue;
		const Select &d = m->selects[sl];
		const vgui2::HFont f = Font(13.0f);
		const int rowH = P(side ? 26 : 30);
		int widest = 0;
		for (const std::string &o : d.options)
			widest = std::max(widest, TextWide(f, Text(o)));
		const int bw = std::min(innerW * 6 / 10, std::max(P(120), widest + P(40)));
		m_rSelects[sl] = { innerX, y, innerW, rowH };
		m_rSelectBoxes[sl] = { innerX + innerW - bw, y, bw, rowH };
		y += rowH + gap;
	}

	};
	if (side)
	{
		layoutSliders();
		layoutToggles();
		layoutSelects();
		layoutFields();
	}
	else
	{
		layoutFields();
		layoutSliders();
		layoutToggles();
		layoutSelects();
	}

	// the search entry (its "Search" caption is painted)
	if (m_rSearch.w)
	{
		const int cw = TextWide(Font(13.0f), L"Search") + P(8);
		Rect box = m_rSearch;
		if (side)
		{
			box.x += cw;
			box.w -= cw;
		}
		m_pSearch->SetBounds(box.x, box.y, box.w, box.h);
		m_pSearch->SetFont(Font(13.0f));
		m_pSearch->SetVisible(true);
		m_pSearch->SetMouseInputEnabled(true);
	}
	else
		m_pSearch->SetVisible(false);

	// bottom: the footer (side: buttons; centered: the key hint)
	const int bottom = m_rWin.y + m_rWin.h - padY;
	m_rButtons.clear();
	m_rPrimary = m_rBack = m_rHint = Rect();
	const vgui2::HFont buttonFont = Font(14.0f);
	int areaBottom;
	if (side)
	{
		const int bh = P(44);
		const int by = bottom - bh;
		int bx = innerX + innerW;
		// no item with an action (a list only to read, e.g. a radio queue): no primary button, no room for it
		if (AnyAction())
		{
			const int pw = std::max(P(96), TextWide(buttonFont, PrimaryLabel()) + 2 * P(24));
			bx -= pw;
			m_rPrimary = { bx, by, pw, bh };
		}
		else
			bx += P(10); // the menu's buttons end at the right edge
		for (int b = (int)m->buttons.size() - 1; b >= 0; b--)
		{
			const int w = TextWide(buttonFont, Text(m->buttons[b])) + 2 * P(20);
			bx -= P(10) + w;
			m_rButtons.insert(m_rButtons.begin(), { bx, by, w, bh });
		}
		if (m->flags & MENU_BACK)
			m_rBack = { innerX, by, TextWide(buttonFont, L"Back") + 2 * P(20), bh };
		areaBottom = by - gap;
	}
	else
	{
		const int hintH = FontTall(Font(12.0f));
		m_rHint = { innerX, bottom - hintH, innerW, hintH };
		areaBottom = m_rHint.y - gap;
		if (Popups() && !m->buttons.empty())
		{
			// no side panel: the menu's buttons in the bottom row, at its right; the key hint at its left
			const int bh = P(36);
			int bx = innerX + innerW + P(10);
			for (int b = (int)m->buttons.size() - 1; b >= 0; b--)
			{
				const int w = TextWide(buttonFont, Text(m->buttons[b])) + 2 * P(20);
				bx -= P(10) + w;
				m_rButtons.insert(m_rButtons.begin(), { bx, bottom - bh, w, bh });
			}
			m_rHint = { innerX, bottom - bh, std::max(0, bx - innerX - P(10)), bh };
			areaBottom = bottom - bh - gap;
		}
		if (m->flags & MENU_BACK)
		{
			const int bh = P(32);
			m_rBack = { innerX, areaBottom - bh, TextWide(buttonFont, L"Back") + 2 * P(20), bh };
			areaBottom = m_rBack.y - gap;
		}
	}

	// the item area, and the centered window's side panel (280) with its picture / preview box (180)
	m_rPanel = Rect();
	m_rPreview = Rect();
	int areaW = innerW;
	if (!side && !Popups())
	{
		const int pw = P(280);
		m_rPanel = { innerX + innerW - pw, y, pw, areaBottom - y };
		areaW = innerW - pw - P(20);
		const int pad = P(14);
		// the software renderer draws no preview model: no empty box for it (the panel's texts move up)
		m_rPreview = { m_rPanel.x + pad, m_rPanel.y + pad, m_rPanel.w - 2 * pad,
			preview && !ServerMenuPreview_CanClip() ? 0 : P(180) };
		const int bh = P(44);
		m_rPrimary = { m_rPanel.x + pad, m_rPanel.y + m_rPanel.h - pad - bh, m_rPanel.w - 2 * pad, bh };
		// the menu's own buttons above the primary one
		int byy = m_rPrimary.y;
		for (int b = (int)m->buttons.size() - 1; b >= 0; b--)
		{
			byy -= P(8) + P(36);
			m_rButtons.insert(m_rButtons.begin(), { m_rPrimary.x, byy, m_rPrimary.w, P(36) });
		}
		// a short panel (many controls above it, a small screen): the picture box gives up its height first, so the
		// name, a line of text and the price line stay above the buttons
		const int below = P(10) + FontTall(Font(20.0f)) + P(10) + FontTall(Font(13.0f)) + P(40);
		const int room = byy - P(8) - m_rPreview.y - below;
		if (m_rPreview.h > room)
			m_rPreview.h = room >= P(60) ? room : 0;
	}
	else if (preview)
		m_rPreview = ServerMenuPreview_CanClip() ? Rect{ 0, 0, m_rWin.x, sh } : Rect(); // the screen left of the
		                                                                                 // window; none on software
	m_rArea = { innerX, y, areaW, std::max(0, areaBottom - y) };
	m_pArea->SetBounds(m_rArea.x, m_rArea.y, m_rArea.w, m_rArea.h);
	m_pArea->SetVisible(true);
	LayoutArea();
}

void CSboServerMenu::LayoutArea()
{
	Refilter();
	m_Cards.clear();
	const Menu *m = M();
	if (!m)
		return;
	const bool side = (m->flags & MENU_SIDE) != 0;
	const int scrollW = P(8);
	const int w = m_rArea.w - scrollW - P(4);
	if (m->layout == LAYOUT_STARS)
	{
		// one row of stars; each one's box takes half the gap on either side, so the mouse between two stars is
		// over one of them (no flicker of the lit row)
		const int gap = P(10);
		m_iColumns = std::max(1, (int)m_Visible.size());
		for (size_t n = 0; n < m_Visible.size(); n++)
			m_Cards.push_back({ (int)n * (m_iStarSize + gap) - (n ? gap / 2 : 0), 0,
				m_iStarSize + (n ? gap / 2 : 0) + (n + 1 < m_Visible.size() ? gap / 2 : 0), m_iStarSize });
		m_iContent = 0; // never scrolls
		m_iScroll = 0;
		return;
	}
	if (m->layout == LAYOUT_GRID)
	{
		const bool large = (m->flags & MENU_LARGE) != 0;
		const int gap = P(side ? 10 : 12);
		const bool pictures = CardPictures();
		const int pad = P(side ? 7 : 8);
		const int nameH = FontTall(Font(side ? 13.0f : 14.0f));
		const int badgeH = FontTall(Font(11.0f)) + P(4);
		bool meters = false;
		for (int i : m_Visible)
			meters = meters || m->items[i].meter != METER_NONE;
		// a popup menu's or a large card's picture keeps the map pictures' shape (16:9)
		const bool wide = Popups() || large;
		auto cardTall = [&](int columns, int &cardW, int &picH) {
			cardW = std::max(1, (w - gap * (columns - 1)) / columns);
			picH = wide ? std::max(1, (cardW - 2 * pad) * 9 / 16) : P(side ? 84 : 100);
			return pad + (pictures ? picH + P(6) : 0) + nameH + P(6) + (meters ? P(4) + P(6) : 0) + badgeH + pad;
		};
		m_iColumns = large ? 3 : (side ? 3 : (Popups() ? 5 : 4));
		if (large)
		{
			// MENU_LARGE (a vote): the fewest columns (at least 3) that show every card without scrolling, the
			// cards shrinking with each column; never narrower than the smallest card that still holds its lines
			// (the name beside "88 votes · 100%", the bar, two badges beside two voter bubbles and "+N"). Past that
			// width the cards wrap into more rows and the grid scrolls (a click on one opens its large picture)
			const vgui2::HFont priceFont = Font(12.0f), badgeFont = Font(11.0f);
			const int lineMin = TextWide(Font(side ? 13.0f : 14.0f), L"co_omega17") + P(6) +
			                    TextWide(priceFont, L"88 votes \u00b7 100%");
			const int badgeMin = TextWide(badgeFont, L"Winning") + TextWide(badgeFont, L"Your vote") + 4 * P(6) + P(4) +
			                     2 * (badgeH + P(3)) + TextWide(badgeFont, L"+88") + P(7);
			const int minCardW = 2 * pad + std::max(lineMin, badgeMin);
			const int most = std::max(3, (int)m_Visible.size());
			m_iColumns = 1;
			for (int columns = 1; columns <= most; columns++)
			{
				int cw, ph;
				const int ch = cardTall(columns, cw, ph);
				if (columns > 1 && cw < minCardW)
					break; // narrower than the smallest card: the last width that holds them, with scrolling
				m_iColumns = columns;
				const int rows = ((int)m_Visible.size() + columns - 1) / columns;
				if (columns >= 3 && rows * ch + (rows - 1) * gap <= m_rArea.h)
					break;
			}
		}
		int cardW;
		const int cardH = cardTall(m_iColumns, cardW, m_iPictureTall);
		for (size_t n = 0; n < m_Visible.size(); n++)
		{
			const int col = (int)n % m_iColumns, row = (int)n / m_iColumns;
			m_Cards.push_back({ col * (cardW + gap), row * (cardH + gap), cardW, cardH });
		}
		m_iContent = m_Cards.empty() ? 0 : m_Cards.back().y + cardH;
	}
	else
	{
		m_iColumns = 1;
		// the docked window's rows are denser (the design's queue)
		const int rowH = side ? std::max(P(30), FontTall(Font(13.0f)) + P(10)) : std::max(P(36), FontTall(Font(14.0f)) + P(12));
		for (size_t n = 0; n < m_Visible.size(); n++)
			m_Cards.push_back({ 0, (int)n * (rowH + P(2)), w, rowH });
		m_iContent = m_Cards.empty() ? 0 : m_Cards.back().y + rowH;
	}
	m_iScroll = std::max(0, std::min(m_iScroll, m_iContent - m_rArea.h));
}

void CSboServerMenu::Highlight(int item, bool scrollTo)
{
	m_iHighlight = item;
	if (!scrollTo)
		return;
	for (size_t n = 0; n < m_Visible.size(); n++)
	{
		if (m_Visible[n] != item)
			continue;
		const Rect &r = m_Cards[n];
		if (r.y < m_iScroll)
			m_iScroll = r.y;
		else if (r.y + r.h > m_iScroll + m_rArea.h)
			m_iScroll = r.y + r.h - m_rArea.h;
		m_iScroll = std::max(0, std::min(m_iScroll, m_iContent - m_rArea.h));
	}
}

void CSboServerMenu::SetTab(int tab)
{
	const Menu *m = M();
	if (!m || m->tabs.empty())
		return;
	m_iTab = (tab + (int)m->tabs.size()) % (int)m->tabs.size();
	m_iScroll = 0;
	LayoutArea();
	Highlight(m_Visible.empty() ? -1 : m_Visible[0], true);
}

bool CSboServerMenu::CanPick(int item) const
{
	const Menu *m = M();
	return m && item >= 0 && item < (int)m->items.size() && m->items[item].defined &&
	       !(m->items[item].flags & (ITEM_DISABLED | ITEM_HIDDEN)) && m->items[item].CooldownLeft(ServerMenu_Now()) <= 0.0f;
}

bool CSboServerMenu::AnyAction() const
{
	const Menu *m = M();
	if (!m)
		return false;
	for (const Item &it : m->items)
	{
		if (it.defined && !it.action.empty())
			return true;
	}
	return false;
}

bool CSboServerMenu::PrimaryShown() const
{
	const Menu *m = M();
	return m && m_rPrimary.w > 0 && m_iHighlight >= 0 && m_iHighlight < (int)m->items.size() &&
	       !m->items[m_iHighlight].action.empty();
}

std::wstring CSboServerMenu::PrimaryLabel() const
{
	const Menu *m = M();
	if (m && m_iHighlight >= 0 && m_iHighlight < (int)m->items.size() && !m->items[m_iHighlight].action.empty())
		return Text(m->items[m_iHighlight].action);
	return L"Select";
}

void CSboServerMenu::Pick(int item)
{
	if (!CanPick(item))
		return;
	const Menu *m = M();
	if (AskConfirm(CONFIRM_ITEM, item, item, m->items[item].confirm))
		return;
	SendPick(item);
}

void CSboServerMenu::SendPick(int item)
{
	char what[32];
	snprintf(what, sizeof(what), "pick %d", item);
	ServerMenu_Send(what);
}

void CSboServerMenu::Button(int button)
{
	const Menu *m = M();
	if (!m || !m->ButtonEnabled(button))
		return; // greyed (BUTTONSTATE): nothing is sent
	const int item = m_iHighlight >= 0 ? m_iHighlight : NO_ITEM;
	if (button >= 0 && button < (int)m->buttonConfirms.size() &&
	    AskConfirm(CONFIRM_BUTTON, button, item, m->buttonConfirms[button]))
		return;
	SendButton(button, item);
}

void CSboServerMenu::SendButton(int button, int item)
{
	char what[32];
	snprintf(what, sizeof(what), "button %d %d", button, item);
	ServerMenu_Send(what);
}

void CSboServerMenu::Back()
{
	ServerMenu_Send("back");
}

void CSboServerMenu::SubmitField(int field)
{
	const Menu *m = M();
	if (!m || field < 0 || field >= (int)m->fields.size() || field >= MAX_MENU_FIELDS)
		return;
	char text[LEN_FIELD_TEXT * 4 + 1];
	m_pFields[field]->GetText(text, sizeof(text));
	const std::string clean = CleanFieldText(text, (size_t)m->fields[field].maxLength);
	if (clean.empty())
		return;
	char what[LEN_FIELD_TEXT + 32];
	snprintf(what, sizeof(what), "text %d \"%s\"", field, clean.c_str());
	ServerMenu_Send(what);
	m_pFields[field]->SetText("");
	EndSearchTyping();
}

//! The model preview: the highlighted item's layers over the ones of the picked items
void CSboServerMenu::UpdatePreview()
{
	ServerMenuPreviewSpec spec;
	memset(&spec, 0, sizeof(spec));
	const Menu *m = M();
	// the software renderer draws no preview model at all (docs/SERVER_MENUS.md "Software renderer"): the window shows
	// the backers and names
	// the software renderer leaves the view model out while a menu with a preview is up, as the preview does on the
	// hardware renderer
	ServerMenuPreview_SetHideViewModel(IsVisible() && m && (m->flags & MENU_PREVIEW) && !ServerMenuPreview_CanClip());
	if (!IsVisible() || !m || !(m->flags & MENU_PREVIEW) || m_rPreview.w <= 0 || m_rPreview.h <= 0 ||
	    !ServerMenuPreview_CanClip())
	{
		ServerMenuPreview_Set(spec);
		ServerMenuPreview_SetCards(nullptr, 0);
		return;
	}
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	spec.active = true;
	const Rect stage = PreviewStage();
	spec.frame = ScreenFrac(stage, sw, sh);
	spec.backer[0] = C_BACKER.r();
	spec.backer[1] = C_BACKER.g();
	spec.backer[2] = C_BACKER.b();
	spec.yaw = m_flYaw;
	// the visible cards' own models, turning slowly
	ServerMenuCardSpec cards[SERVER_MENU_MAX_CARD_MODELS];
	int nCards = 0;
	m_CardSlots.assign(m_Visible.size(), -1);
	const float turn = fmodf((float)vgui2::system()->GetCurrentTime() * 40.0f, 360.0f);
	for (size_t n = 0; n < m_Visible.size() && nCards < SERVER_MENU_MAX_CARD_MODELS; n++)
	{
		bool player;
		const Model *mdl = CardModel(m_Visible[n], player);
		if (!mdl || !CardModelShown(n))
			continue;
		m_CardSlots[n] = nCards;
		ServerMenuCardSpec &c = cards[nCards++];
		memset(&c, 0, sizeof(c));
		c.frame = ScreenFrac(CardScreenBox(n), sw, sh);
		c.clip = ScreenFrac(CardScreenClip(n), sw, sh);
		c.yaw = turn;
		c.playerModel = player;
		V_strncpy(c.model.path, m->ModelPath(*mdl).c_str(), sizeof(c.model.path));
		c.model.body = m->variant ? mdl->body1 : mdl->body;
		c.model.skin = mdl->skin;
		c.model.sequence = m->variant ? mdl->sequence1 : mdl->sequence;
	}
	ServerMenuPreview_SetCards(cards, nCards);
	for (int layer = 0; layer < MODEL_LAYERS; layer++)
	{
		const Model *pick = nullptr;
		if (m_iHighlight >= 0 && m_iHighlight < (int)m->items.size() && m->items[m_iHighlight].models[layer].set)
			pick = &m->items[m_iHighlight].models[layer];
		for (int i = 0; !pick && i < (int)m->items.size(); i++)
		{
			if ((m->items[i].flags & ITEM_PICKED) && m->items[i].models[layer].set)
				pick = &m->items[i].models[layer];
		}
		if (!pick)
			continue;
		ServerMenuPreviewLayer &l = layer == 0 ? spec.base : spec.layers[layer - 1];
		V_strncpy(l.path, m->ModelPath(*pick).c_str(), sizeof(l.path));
		l.body = m->variant ? pick->body1 : pick->body;
		l.skin = pick->skin;
		l.sequence = m->variant ? pick->sequence1 : pick->sequence;
	}
	ServerMenuPreview_Set(spec);
}

void CSboServerMenu::OnThink()
{
	BaseClass::OnThink();
	// HL25 opens GameUI on Esc before the client sees the key: while the window is up, GameUI is hidden again at once
	// and the Esc goes to the window (KeyInput: the popup, a dropdown or the dialog first, else the window closes)
	const bool guard = IsVisible();
	if (g_pGameUIViewport)
	{
		if (guard != m_bEscGuard)
			g_pGameUIViewport->SetMenuEscapeGuard(guard);
		m_bEscGuard = guard;
		if (guard && g_pGameUIViewport->TakeMenuEscape())
		{
			// typing in the search box or a text field: the Esc only gives the keys back (OnKeyCodeTyped)
			if (IsKeyBoardInputEnabled())
				EndSearchTyping();
			else
				KeyInput(SMKEY_ESCAPE);
		}
	}
	if (IsVisible())
	{
		UpdatePreview();
		MapPictureDownloads();
	}
}

void CSboServerMenu::MapPictureDownloads()
{
	if (m_MapPictures.Poll())
		m_pArea->Repaint();
	const Menu *m = M();
	if (!m || m->layout != LAYOUT_GRID || m_MapPictures.IsDownloading())
		return;
	// the picture of a map without one, one download at a time: the highlighted card first, then the cards in view
	// (a picture is asked for only once it was made by a paint)
	if (m_iHighlight >= 0 && m_iHighlight < (int)m->items.size() && !m->items[m_iHighlight].map.empty()
	    && m_MapPictures.Has(m->items[m_iHighlight].map))
		m_MapPictures.StartDownload(m->items[m_iHighlight].map);
	for (size_t n = 0; n < m_Visible.size() && n < m_Cards.size() && !m_MapPictures.IsDownloading(); n++)
	{
		const Rect &c = m_Cards[n];
		const std::string &map = m->items[m_Visible[n]].map;
		if (map.empty() || c.y - m_iScroll + c.h < 0 || c.y - m_iScroll > m_rArea.h)
			continue;
		if (m_MapPictures.Has(map))
			m_MapPictures.StartDownload(map);
	}
}

//-----------------------------------------------------------------------------
// Painting
//-----------------------------------------------------------------------------
void CSboServerMenu::PaintBackground()
{
	const Menu *m = M();
	if (!m || Side())
		return; // the side window leaves the screen as it is
	// the centered window darkens the screen around it (not the model preview's box)
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	FillExcept({ 0, 0, sw, m_rWin.y }, Rect(), C_SHADE);
	FillExcept({ 0, m_rWin.y + m_rWin.h, sw, sh - m_rWin.y - m_rWin.h }, Rect(), C_SHADE);
	FillExcept({ 0, m_rWin.y, m_rWin.x, m_rWin.h }, Rect(), C_SHADE);
	FillExcept({ m_rWin.x + m_rWin.w, m_rWin.y, sw - m_rWin.x - m_rWin.w, m_rWin.h }, Rect(), C_SHADE);
}

void CSboServerMenu::PaintButton(const Rect &r, const std::wstring &label, bool primary, bool enabled, bool hot)
{
	if (r.w <= 0)
		return;
	if (primary)
		FillRect(r.x, r.y, r.w, r.h, enabled ? COL_BG_PRIMARY : COL_BG_SELECTED);
	else if (hot && enabled)
		FillRect(r.x, r.y, r.w, r.h, COL_BG_SELECTED);
	OutlineRect(r.x, r.y, r.w, r.h, primary && enabled ? COL_BORDER_PRIMARY : COL_BORDER);
	const vgui2::HFont f = Font(primary ? 15.0f : 14.0f);
	const Color c = !enabled ? COL_TEXT_FAINT : (primary ? COL_TEXT_PRIMARY : COL_TEXT);
	const int tw = std::min(TextWide(f, label), r.w - P(8));
	DrawTextMid(f, c, r.x + (r.w - tw) / 2, r.y, r.h, label, r.w - P(8));
}

void CSboServerMenu::PaintHeader(int x, int y, int maxW, const std::string &line, vgui2::HFont font)
{
	// "Label: value": the label dim, the value bright (the design's "Wearing: Santa")
	const std::wstring w = Text(line);
	const size_t colon = w.find(L": ");
	if (colon == std::wstring::npos)
	{
		DrawText(font, COL_TEXT_DIM, x, y, w, maxW);
		return;
	}
	const std::wstring label = w.substr(0, colon + 2);
	const int lw = DrawText(font, COL_TEXT_DIM, x, y, label, maxW);
	DrawText(font, COL_TEXT_SELECTED, x + lw, y, w.substr(colon + 2), std::max(1, maxW - lw));
}

void CSboServerMenu::Paint()
{
	const Menu *m = M();
	if (!m)
		return;
	if (Stars())
	{
		PaintStars();
		return;
	}
	const bool side = Side();
	const bool preview = HasPreview();
	const int padX = P(side ? 18 : 24), padY = P(side ? 14 : 20);
	const int innerX = m_rWin.x + padX, innerW = m_rWin.w - 2 * padX;

	// the window, opaque everywhere (whatever HUD panel lies beneath never shows in it). Hardware renderer: the model
	// frames are painted over it from their copies. Software renderer: the 3D view cannot be copied, so the preview's
	// box and the cards show their backer without a model.
	FillRect(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, COL_BG_WINDOW);
	const bool software = preview && !ServerMenuPreview_CanClip(); // the preview's backer without a model
	if (!side && preview && (ServerMenuPreview_HasModel() || software))
	{
		FillRect(m_rPreview.x, m_rPreview.y, m_rPreview.w, m_rPreview.h, C_BACKER);
		if (ServerMenuPreview_CanClip())
		{
			int sw, sh;
			vgui2::surface()->GetScreenSize(sw, sh);
			ServerMenuPreview_PaintCopy(SERVER_MENU_MAX_CARD_MODELS, ScreenFrac(PreviewStage(), sw, sh));
		}
	}
	OutlineRect(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, COL_BORDER);

	// title and Close
	const vgui2::HFont titleFont = Font(side ? 22.0f : 28.0f);
	const int titleH = std::max(P(side ? 28 : 32), FontTall(titleFont));
	const int titleMaxW = (m_rClose.w ? m_rClose.x - P(12) : innerX + innerW) - innerX;
	DrawTextMid(titleFont, COL_TEXT, innerX, m_rWin.y + padY, titleH, Text(m->title), titleMaxW);
	if (m_rClose.w)
	{
		const bool hot = m_rClose.Has(m_iMouseX, m_iMouseY);
		if (hot)
			FillRect(m_rClose.x, m_rClose.y, m_rClose.w, m_rClose.h, COL_BG_SELECTED);
		OutlineRect(m_rClose.x, m_rClose.y, m_rClose.w, m_rClose.h, COL_BORDER);
		DrawX(m_rClose.x + m_rClose.w / 2, m_rClose.y + m_rClose.h / 2, P(12), COL_TEXT);
	}

	// header lines
	if (side)
	{
		const vgui2::HFont f = Font(12.0f);
		int y = m_iHeaderY;
		for (const std::string &h : m->headers)
		{
			if (h.empty())
				continue;
			PaintHeader(innerX, y, innerW, h, f);
			y += FontTall(f) + P(3);
		}
	}
	else
	{
		// the first line in the box left of Close ("Spawns 12"), the others dim left of it
		int right = (m_rClose.w ? m_rClose.x : innerX + innerW) - P(16);
		const int ty = m_rWin.y + padY;
		const std::wstring first = Text(m->headers[0]);
		if (!first.empty())
		{
			const vgui2::HFont small = Font(13.0f), big = Font(22.0f);
			std::wstring label, value = first;
			const size_t colon = first.find(L": ");
			if (colon != std::wstring::npos)
			{
				label = first.substr(0, colon);
				value = first.substr(colon + 2);
			}
			const int bw = (label.empty() ? 0 : TextWide(small, label) + P(8)) + TextWide(big, value) + 2 * P(14);
			const int bh = std::max(P(36), FontTall(big) + P(8));
			Rect box{ right - bw, ty + (titleH - bh) / 2, bw, bh };
			FillRect(box.x, box.y, box.w, box.h, COL_BG_SELECTED);
			OutlineRect(box.x, box.y, box.w, box.h, COL_BORDER_PRIMARY);
			int x = box.x + P(14);
			if (!label.empty())
				x += DrawTextMid(small, COL_TEXT_DIM, x, box.y, box.h, label) + P(8);
			DrawTextMid(big, COL_TEXT_PRIMARY, x, box.y, box.h, value);
			right = box.x - P(16);
		}
		const vgui2::HFont f = Font(13.0f);
		for (int i = MAX_HEADERS - 1; i >= 1; i--)
		{
			const std::wstring w = Text(m->headers[i]);
			if (w.empty())
				continue;
			const int tw = std::min(TextWide(f, w), std::max(0, right - innerX - P(240)));
			if (tw <= 0)
				break;
			DrawTextMid(f, COL_TEXT_DIM, right - tw, ty, titleH, w, tw);
			right -= tw + P(16);
		}
	}

	// tabs
	if (!m_rTabs.empty())
	{
		const vgui2::HFont f = Font(side ? 13.0f : 14.0f);
		const int lineY = m_rTabs.back().y + m_rTabs.back().h;
		FillRect(innerX, lineY, innerW, 1, COL_BORDER);
		for (size_t t = 0; t < m_rTabs.size() && t < m->tabs.size(); t++)
		{
			const Rect &r = m_rTabs[t];
			const bool on = (int)t == m_iTab;
			if (on)
				FillRect(r.x, r.y, r.w, r.h, COL_BG_SELECTED);
			// border without the bottom edge
			FillRect(r.x, r.y, r.w, 1, COL_BORDER);
			FillRect(r.x, r.y, 1, r.h, COL_BORDER);
			FillRect(r.x + r.w - 1, r.y, 1, r.h, COL_BORDER);
			const std::wstring label = Text(m->tabs[t]);
			const int tw = TextWide(f, label);
			const bool hot = r.Has(m_iMouseX, m_iMouseY);
			DrawTextMid(f, on ? COL_TEXT_BRIGHT : (hot ? COL_TEXT : COL_TEXT_DIM), r.x + (r.w - tw) / 2, r.y, r.h,
				label);
		}
	}

	// text fields: the caption and the button (the entry paints itself)
	for (int f = 0; f < MAX_MENU_FIELDS && f < (int)m->fields.size(); f++)
	{
		const Field &fd = m->fields[f];
		if (!fd.label.empty())
			DrawTextMid(Font(13.0f), COL_TEXT_DIM, m_rFields[f].x, m_rFields[f].y, m_rFields[f].h, Text(fd.label));
		if (m_rFieldButtons[f].w)
			PaintButton(m_rFieldButtons[f], Text(fd.button), true, true, m_rFieldButtons[f].Has(m_iMouseX, m_iMouseY));
	}

	// sliders: the caption, the track filled to the value (the dragged value while dragging), the value
	for (int sl = 0; sl < MAX_MENU_SLIDERS && sl < (int)m->sliders.size(); sl++)
	{
		const Slider &d = m->sliders[sl];
		const Rect &row = m_rSliders[sl], &tr = m_rSliderTracks[sl];
		const vgui2::HFont f = Font(side ? 12.0f : 13.0f);
		const int value = m_iSliderDrag == sl ? m_iSliderValue : SliderShown(sl);
		if (!d.label.empty())
			DrawTextMid(f, COL_TEXT_DIM, row.x, row.y, row.h, Text(d.label));
		const int th = std::max(4, P(6)), ty = tr.y + (tr.h - th) / 2;
		const float frac = d.max > d.min ? (value - d.min) / (float)(d.max - d.min) : 1.0f;
		FillRect(tr.x, ty, tr.w, th, COL_BG_SELECTED);
		FillRect(tr.x, ty, (int)(tr.w * frac), th, COL_BORDER_PRIMARY);
		const int kw = std::max(3, P(4)), kx = tr.x + (int)((tr.w - kw) * frac);
		const int kin = P(side ? 4 : 6);
		FillRect(kx, tr.y + kin, kw, tr.h - 2 * kin, (m_iSliderDrag == sl || tr.Has(m_iMouseX, m_iMouseY)) ?
			COL_TEXT_PRIMARY : COL_TEXT_LIGHT);
		wchar_t t[16];
		swprintf(t, sizeof(t) / sizeof(t[0]), L"%d", value);
		DrawTextMid(f, COL_TEXT_LIGHT, row.x + row.w - TextWide(f, t), row.y, row.h, t);
	}

	// toggles: the caption and a switch (on: filled, its knob right)
	for (int t = 0; t < MAX_MENU_TOGGLES && t < (int)m->toggles.size(); t++)
	{
		const Rect &row = m_rToggles[t];
		const bool on = ToggleShown(t), hot = row.Has(m_iMouseX, m_iMouseY);
		const vgui2::HFont f = Font(side ? 12.0f : 13.0f);
		// the docked window's compact rows: a small switch
		const int sw2 = P(side ? 28 : 44), sh2 = P(side ? 14 : 22), sx = row.x + row.w - sw2, sy = row.y + (row.h - sh2) / 2;
		DrawTextMid(f, hot ? COL_TEXT : COL_TEXT_DIM, row.x, row.y, row.h, Text(m->toggles[t].label),
			std::max(1, row.w - sw2 - P(8)));
		FillRect(sx, sy, sw2, sh2, on ? COL_BG_PRIMARY : COL_BG_INPUT);
		OutlineRect(sx, sy, sw2, sh2, on ? COL_BORDER_PRIMARY : COL_BORDER);
		const int kp = P(side ? 3 : 4), k = sh2 - 2 * kp;
		FillRect(on ? sx + sw2 - kp - k : sx + kp, sy + kp, k, k, on ? COL_TEXT_PRIMARY : COL_TEXT_FAINT);
	}

	// dropdowns: the caption and the box with the choice and an arrow
	for (int sl = 0; sl < MAX_MENU_SELECTS && sl < (int)m->selects.size(); sl++)
	{
		if (m_rSelects[sl].w)
			PaintSelect(sl, m_rSelects[sl], m_rSelectBoxes[sl], m_rSelectBoxes[sl].Has(m_iMouseX, m_iMouseY) || m_iDrop == sl);
	}

	// search
	if (m_rSearch.w)
	{
		const vgui2::HFont f = Font(13.0f);
		const std::wstring caption = L"Search";
		const int cw = TextWide(f, caption);
		DrawTextMid(f, COL_TEXT_DIM, m_rSearch.x - (side ? 0 : cw + P(8)), m_rSearch.y, m_rSearch.h, caption);
	}

	// nothing to show
	if (m_Visible.empty())
	{
		const vgui2::HFont f = Font(13.0f);
		DrawText(f, COL_TEXT_DIM, m_rArea.x, m_rArea.y + P(8), m_Search.empty() ? L"Nothing here" : L"No match");
	}

	const Item *hi = (m_iHighlight >= 0 && m_iHighlight < (int)m->items.size()) ? &m->items[m_iHighlight] : nullptr;

	// centered: the side panel
	if (m_rPanel.w)
	{
		OutlineRect(m_rPanel.x, m_rPanel.y, m_rPanel.w, m_rPanel.h, COL_BORDER_LIST);
		const int pad = P(14);
		if (m_rPreview.h > 0)
			OutlineRect(m_rPreview.x - 1, m_rPreview.y - 1, m_rPreview.w + 2, m_rPreview.h + 2, COL_BORDER_LIST);
		if (!preview && hi && !hi->image.empty())
			PaintFilePicture(hi->image, m_rPreview, false, GetWide(), GetTall()); // the item's picture, large
		else if (!preview && hi && !hi->map.empty())
			PaintMapPicture(hi->map, m_rPreview, false, GetWide(), GetTall());
		else if (!preview)
			FillRect(m_rPreview.x, m_rPreview.y, m_rPreview.w, m_rPreview.h, COL_BG_INPUT);
		else if (!ServerMenuPreview_HasModel() && !software)
		{
			FillRect(m_rPreview.x, m_rPreview.y, m_rPreview.w, m_rPreview.h, COL_BG_INPUT);
			DrawTextMid(Font(11.0f), C_DIM_TEXT, m_rPreview.x + P(8), m_rPreview.y, m_rPreview.h, L"No preview");
		}
		int y = m_rPreview.y + m_rPreview.h + P(10);
		if (hi)
		{
			const vgui2::HFont nameFont = Font(20.0f), body = Font(13.0f);
			DrawText(nameFont, COL_TEXT_SELECTED, m_rPanel.x + pad, y, Text(hi->label), m_rPanel.w - 2 * pad);
			y += FontTall(nameFont) + P(10);
			// the description, wrapped by words
			const std::wstring text = Text(hi->text);
			const int maxW = m_rPanel.w - 2 * pad;
			size_t start = 0;
			const int lineH = FontTall(body) + P(4);
			const int limitY = m_rPrimary.y - (int)m_rButtons.size() * P(44) - P(40);
			while (start < text.size() && y + lineH < limitY)
			{
				size_t end = start, lastSpace = std::wstring::npos;
				while (end < text.size() && TextWide(body, text.substr(start, end - start + 1)) <= maxW)
				{
					if (text[end] == L' ')
						lastSpace = end;
					end++;
				}
				if (end < text.size() && lastSpace != std::wstring::npos && lastSpace > start)
					end = lastSpace + 1;
				if (end == start)
					end = start + 1;
				DrawText(body, COL_TEXT_LIGHT, m_rPanel.x + pad, y, text.substr(start, end - start));
				y += lineH;
				start = end;
			}
			if (!hi->price.empty())
			{
				const int py = m_rPrimary.y - (int)m_rButtons.size() * P(44) - P(30);
				DrawText(body, COL_TEXT_DIM, m_rPanel.x + pad, py, L"Cost");
				const std::wstring price = Text(hi->price);
				DrawText(body, C_BADGE_TEXT, m_rPanel.x + m_rPanel.w - pad - TextWide(body, price), py, price);
			}
		}
	}

	// buttons
	for (size_t b = 0; b < m_rButtons.size() && b < m->buttons.size(); b++)
		PaintButton(m_rButtons[b], Text(m->buttons[b]), false, m->ButtonEnabled((int)b), m_rButtons[b].Has(m_iMouseX, m_iMouseY));
	if (m_rBack.w)
		PaintButton(m_rBack, L"Back", false, true, m_rBack.Has(m_iMouseX, m_iMouseY));
	// the primary button only for a highlighted item with an action (none: no dead "Select")
	if (PrimaryShown())
		PaintButton(m_rPrimary, PrimaryLabel(), true, CanPick(m_iHighlight), m_rPrimary.Has(m_iMouseX, m_iMouseY));

	// centered: the key hint
	if (m_rHint.w)
	{
		const vgui2::HFont f = Font(12.0f);
		// the window takes only the mouse (the keys stay the player's): a double click or 1-9 picks, Esc closes
		std::wstring hint = Popups() ? L"Click or Enter for details · Double click or 1-9 picks · Tab and arrows move · Esc closes"
		                             : L"Double click, Enter or 1-9 picks · Tab and arrows move · Esc closes";
		// a narrow window (a small screen, the buttons beside it): the short hint
		if (TextWide(f, hint) > m_rHint.w)
			hint = L"Enter or 1-9 picks · Esc closes";
		DrawTextMid(f, COL_TEXT_DIM, m_rHint.x, m_rHint.y, m_rHint.h, hint, m_rHint.w);
	}

	// the keyboard's focus (Tab, arrows): a ring around the control (the items show their highlight)
	if (m_iPopup < 0 && m_iConfirmKind < 0 && m_iDrop < 0 && m_Focus.kind != F_ITEMS)
		PaintFocusRing(FocusRect(m_Focus));

	// the hover text of the button, slider or switch under the mouse, above it, kept on the screen: from the cursor
	// as it is now and only while it is over the window itself (not a stale position from before the window opened,
	// nor over the item area, an entry or the popup)
	if (vgui2::input()->GetMouseOver() == GetVPanel() && m_iPopup < 0)
	{
		int mx, my;
		vgui2::input()->GetCursorPos(mx, my);
		ScreenToLocal(mx, my);
		m_iMouseX = mx;
		m_iMouseY = my;
		std::wstring tip;
		Rect at;
		for (size_t b = 0; b < m_rButtons.size() && b < m->buttonTips.size() && tip.empty(); b++)
		{
			if (m_rButtons[b].Has(m_iMouseX, m_iMouseY) && !m->buttonTips[b].empty())
				tip = Text(m->buttonTips[b]), at = m_rButtons[b];
		}
		for (int sl = 0; sl < MAX_MENU_SLIDERS && sl < (int)m->sliders.size() && tip.empty(); sl++)
		{
			if (m_rSliders[sl].Has(m_iMouseX, m_iMouseY) && !m->sliders[sl].tip.empty() && m_iSliderDrag < 0)
				tip = Text(m->sliders[sl].tip), at = m_rSliders[sl];
		}
		for (int t = 0; t < MAX_MENU_TOGGLES && t < (int)m->toggles.size() && tip.empty(); t++)
		{
			if (m_rToggles[t].Has(m_iMouseX, m_iMouseY) && !m->toggles[t].tip.empty())
				tip = Text(m->toggles[t].tip), at = m_rToggles[t];
		}
		for (int sl = 0; sl < MAX_MENU_SELECTS && sl < (int)m->selects.size() && tip.empty() && m_iDrop < 0; sl++)
		{
			if (m_rSelects[sl].Has(m_iMouseX, m_iMouseY) && !m->selects[sl].tip.empty())
				tip = Text(m->selects[sl].tip), at = m_rSelects[sl];
		}
		if (!tip.empty())
		{
			int sw, sh;
			vgui2::surface()->GetScreenSize(sw, sh);
			const vgui2::HFont f = Font(12.0f);
			const int tw = std::min(sw - P(8), TextWide(f, tip) + 2 * P(8)), th = FontTall(f) + P(8);
			const int tx = std::max(P(4), std::min(sw - tw - P(4), m_iMouseX - tw / 2));
			int ty = at.y - th - P(4);
			if (ty < 0)
				ty = at.y + at.h + P(4);
			FillRect(tx, ty, tw, th, COL_BG_INPUT);
			OutlineRect(tx, ty, tw, th, COL_BORDER);
			DrawTextMid(f, COL_TEXT_LIGHT, tx + P(8), ty, th, tip, tw - 2 * P(8));
		}
	}

	// side + preview: the model's frame (its backer is drawn by the renderer), the hint under it, or "No preview"
	// (software renderer: the frame's backer painted here, with the highlighted item's name as the hint)
	if (side && preview && (ServerMenuPreview_HasModel() || software))
	{
		const Rect st = PreviewStage();
		if (st.w > 0 && st.h > 0)
		{
			if (software)
				FillRect(st.x, st.y, st.w, st.h, C_BACKER);
			OutlineRect(st.x - 1, st.y - 1, st.w + 2, st.h + 2, COL_BORDER_LIST);
		}
	}
	if (side && preview && m_rPreview.w > P(200) && !(software && !hi))
	{
		const vgui2::HFont f = Font(13.0f);
		std::wstring hint = ServerMenuPreview_HasModel() ? L"Drag to turn" : L"No preview";
		if (software)
			hint = hi ? Text(hi->label) : std::wstring();
		const int tw = TextWide(f, hint);
		const int bw = tw + 2 * P(16), bh = FontTall(f) + P(12);
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		const int bx = m_rPreview.x + (m_rPreview.w - bw) / 2, by = (int)(sh * 0.835f);
		FillRect(bx, by, bw, bh, C_HINT_BG);
		DrawTextMid(f, COL_TEXT_LIGHT, bx + P(16), by, bh, hint);
	}
}

const Model *CSboServerMenu::CardModel(int item, bool &player) const
{
	player = false;
	const Menu *m = M();
	if (!m || !(m->flags & MENU_PREVIEW) || item < 0 || item >= (int)m->items.size())
		return nullptr;
	for (int layer = 0; layer < MODEL_LAYERS; layer++)
	{
		if (m->items[item].models[layer].set)
		{
			player = layer == 0;
			return &m->items[item].models[layer];
		}
	}
	return nullptr;
}

bool CSboServerMenu::CardPictures() const
{
	const Menu *m = M();
	if (!m || m->layout != LAYOUT_GRID)
		return false;
	bool player;
	for (int i : m_Visible)
	{
		if (m->items[i].picture != PICTURE_NONE || !m->items[i].map.empty() || !m->items[i].image.empty() ||
		    CardModel(i, player))
			return true;
	}
	return false;
}

Rect CSboServerMenu::CardBox(size_t n) const
{
	if (n >= m_Cards.size())
		return Rect();
	const Rect &c = m_Cards[n];
	const int pad = P(Side() ? 7 : 8);
	return { c.x + pad, c.y - m_iScroll + pad, c.w - 2 * pad, PictureTall() };
}

bool CSboServerMenu::CardModelShown(size_t n) const
{
	bool player;
	if (n >= m_Visible.size() || !CardModel(m_Visible[n], player) || !CardPictures())
		return false;
	const Rect b = CardBox(n);
	// the hardware renderer cuts a model to the visible part of its box and the window paints its copy; the software
	// renderer's 3D view cannot be copied into the opaque window: no card models there (the card shows its backer)
	if (!ServerMenuPreview_CanClip())
		return false;
	return b.y + b.h > 0 && b.y < m_rArea.h;
}

Rect CSboServerMenu::CardScreenBox(size_t n) const
{
	Rect b = CardBox(n);
	b.x += m_rArea.x;
	b.y += m_rArea.y;
	return b;
}

Rect CSboServerMenu::CardScreenClip(size_t n) const
{
	const Rect b = CardScreenBox(n);
	const int y0 = std::max(b.y, m_rArea.y), y1 = std::min(b.y + b.h, m_rArea.y + m_rArea.h);
	return { b.x, y0, b.w, std::max(0, y1 - y0) };
}

Rect CSboServerMenu::PreviewStage() const
{
	if (!Side())
		return m_rPreview;
	// the side window: a frame in the screen left of it, above the "Drag to turn" hint
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	const int top = (int)(sh * 0.1f), bottom = (int)(sh * 0.815f);
	const int w = std::max(0, std::min(m_rPreview.w - 2 * P(32), (int)(sh * 0.5f)));
	return { m_rPreview.x + (m_rPreview.w - w) / 2, top, w, bottom - top };
}

//! A MAPPICTURE card's box (item area coordinates): the map's picture covering it, cut to the box and the item area
//! (a textured polygon is not clipped by the surface on Windows); the placeholder or nothing yet: the map name in it.
//! off (not allowed, cooling down): greyed.
void CSboServerMenu::PaintMapPicture(const std::string &map, const Rect &box, bool off, int clipW, int clipH, bool large)
{
	FillRect(box.x, box.y, box.w, box.h, COL_BG_INPUT);
	// at most two new pictures per paint (a JPEG decode each): the grid fills in over a few frames
	const CSboMapPictures::Picture *pic = nullptr;
	if (m_MapPictures.Has(map) || m_iNewMapPictures < 2)
	{
		if (!m_MapPictures.Has(map))
			m_iNewMapPictures++;
		pic = m_MapPictures.Get(map);
	}
	else
		m_pArea->Repaint();
	if (pic && pic->texture != -1 && pic->wide > 0 && pic->tall > 0 && box.w > 0 && box.h > 0)
	{
		const bool cover = pic->isMapImage || pic->isPlaceholder; // an overview is shown whole
		const float f = cover ? std::max((float)box.w / pic->wide, (float)box.h / pic->tall)
		                      : std::min((float)box.w / pic->wide, (float)box.h / pic->tall);
		const float dw = pic->wide * f, dh = pic->tall * f;
		const float dx = box.x + (box.w - dw) / 2, dy = box.y + (box.h - dh) / 2;
		const float x0 = std::max({ (float)box.x, dx, 0.0f }), y0 = std::max({ (float)box.y, dy, 0.0f });
		const float x1 = std::min({ (float)(box.x + box.w), dx + dw, (float)clipW });
		const float y1 = std::min({ (float)(box.y + box.h), dy + dh, (float)clipH });
		if (x1 > x0 && y1 > y0)
		{
			const float s0 = (x0 - dx) / dw, s1 = (x1 - dx) / dw, t0 = (y0 - dy) / dh, t1 = (y1 - dy) / dh;
			vgui2::VGuiVertex v[4] = { vgui2::VGuiVertex((int)x0, (int)y0, s0, t0),
				vgui2::VGuiVertex((int)x1, (int)y0, s1, t0), vgui2::VGuiVertex((int)x1, (int)y1, s1, t1),
				vgui2::VGuiVertex((int)x0, (int)y1, s0, t1) };
			vgui2::surface()->DrawSetTexture(pic->texture);
			// greyed (off): the picture drawn darker (a translucent fill over it drew opaque after the picture)
			if (off)
				vgui2::surface()->DrawSetColor(90, 96, 90, 255);
			else
				vgui2::surface()->DrawSetColor(255, 255, 255, 255);
			vgui2::surface()->DrawTexturedPolygon(v, 4);
		}
	}
	if (!pic || pic->texture == -1 || pic->isPlaceholder)
	{
		// the placeholder (or nothing yet): the map name in the middle, as the create server grid shows it
		const vgui2::HFont f = Font(large ? 20.0f : 12.0f);
		const std::wstring name = Text(map);
		const int nw = std::min(TextWide(f, name), box.w - P(12));
		const int ny = box.y + (box.h - FontTall(f)) / 2;
		if (ny >= 0 && ny + FontTall(f) <= clipH)
			DrawText(f, COL_TEXT_DIM, box.x + (box.w - nw) / 2, ny, name, box.w - P(12));
	}
}

void CSboServerMenu::PaintCard(int item, const Rect &r)
{
	const Menu *m = M();
	const Item &it = m->items[item];
	const bool side = Side();
	const bool on = item == m_iHighlight;
	const float cooldown = it.CooldownLeft(ServerMenu_Now());
	const bool off = (it.flags & ITEM_DISABLED) != 0 || cooldown > 0.0f;
	const bool hot = item == m_iHover;
	const int pad = P(side ? 7 : 8);
	const bool pictures = CardPictures();
	const size_t n = std::find(m_Visible.begin(), m_Visible.end(), item) - m_Visible.begin();
	const bool model = pictures && CardModelShown(n);
	const Rect box{ r.x + pad, r.y + pad, r.w - 2 * pad, PictureTall() };
	if (on)
		FillHoles(r, model ? std::vector<Rect>{ box } : std::vector<Rect>(), COL_BG_SELECTED);
	// cooling down: the card greyed (its model too) under its dim texts and the clock
	if (cooldown > 0.0f)
		FillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, C_COOL_SHADE);
	OutlineRect(r.x, r.y, r.w, r.h, on ? COL_BORDER_PRIMARY : (hot ? COL_BORDER : COL_BORDER_LIST));
	int y = r.y + pad;
	if (pictures)
	{
		// the card's 3D model: its frame's copy over the backer (hardware renderer); a card with a model that is not
		// drawn (the software renderer) shows the bare backer, a card without one the empty box
		bool player;
		if (!model && !it.image.empty())
			PaintFilePicture(it.image, box, off, m_rArea.w, m_rArea.h);
		else if (!model && !it.map.empty())
			PaintMapPicture(it.map, box, off, m_rArea.w, m_rArea.h);
		else if (!model)
			FillRect(box.x, box.y, box.w, box.h, CardModel(item, player) ? C_BACKER : COL_BG_INPUT);
		else if (ServerMenuPreview_CanClip())
		{
			// opaque in the backer's color, then the model's frame from its copy (hardware renderer)
			FillRect(box.x, box.y, box.w, box.h, C_BACKER);
			if (n < m_CardSlots.size() && m_CardSlots[n] >= 0)
			{
				int sw, sh;
				vgui2::surface()->GetScreenSize(sw, sh);
				ServerMenuPreview_PaintCopy(m_CardSlots[n], ScreenFrac(CardScreenClip(n), sw, sh));
			}
		}
		OutlineRect(box.x - 1, box.y - 1, box.w + 2, box.h + 2, COL_BORDER_LIST);
		y += box.h + P(6);
	}
	const vgui2::HFont nameFont = Font(side ? 13.0f : 14.0f), priceFont = Font(12.0f), badgeFont = Font(11.0f);
	const std::wstring price = Text(it.price);
	const int priceW = price.empty() ? 0 : TextWide(priceFont, price);
	const Color nameColor = off ? C_DIM_TEXT : (on ? COL_TEXT_SELECTED : COL_TEXT_LIGHT);
	int labelX = r.x + pad;
	if (it.avatar)
	{
		// the item's player (AVATAR) before its name
		const int as = FontTall(nameFont);
		DrawAvatar(it.avatar, labelX, y, as);
		labelX += as + P(6);
	}
	DrawText(nameFont, nameColor, labelX, y, Text(it.label), r.x + r.w - pad - labelX - (priceW ? priceW + P(6) : 0));
	if (priceW)
		DrawText(priceFont, off ? C_DIM_TEXT : C_BADGE_TEXT, r.x + r.w - pad - priceW,
			y + FontTall(nameFont) - FontTall(priceFont), price);
	y += FontTall(nameFont) + P(6);
	// the bar (METER): every card keeps its row when any card of the menu has one
	bool meters = false;
	for (int i : m_Visible)
		meters = meters || m->items[i].meter != METER_NONE;
	if (meters)
	{
		const int bw = r.w - 2 * pad, bh = P(4);
		FillRect(r.x + pad, y, bw, bh, COL_BG_SELECTED);
		if (it.meter != METER_NONE && it.meter > 0)
			FillRect(r.x + pad, y, std::max(1, bw * it.meter / 100), bh, off ? C_DIM_TEXT : COL_TEXT);
		y += bh + P(6);
	}
	(void)badgeFont;
	PaintBadgeRow(it, r.x + pad, y, r.w - 2 * pad, (it.flags & ITEM_PICKED) != 0, on ? COL_BG_SELECTED : COL_BG_WINDOW);
	// cooling down: the clock over its picture box (or the whole card)
	if (cooldown > 0.0f)
	{
		const Rect face = pictures ? box : Rect{ r.x, r.y, r.w, r.h };
		const int radius = std::max(P(8), std::min(face.w, face.h) * 36 / 100);
		DrawCooldownClock(face.x + face.w / 2, face.y + face.h / 2, radius, cooldown, it.cooldownTotal,
			Font(side ? 17.0f : 19.0f, 700));
	}
}

namespace
{
const Color C_GOLD_BG(92, 72, 14, 255);   // a BADGE_GOLD badge (leading, winner)
const Color C_GOLD_TEXT(255, 214, 102, 255);
const Color C_DIM_BG(28, 34, 28, 255);    // a BADGE_DIM badge
const Color C_BUBBLE(46, 58, 46, 255);    // a voter bubble without an avatar

//! A Steam account the client can show an avatar of (as CScorePanel::GetValidSteamID64): an individual account
unsigned long long ValidSteamID64(unsigned long long sid)
{
	return ((sid >> 52) & 0xF) == 1 && (sid & 0xFFFFFFFFull) != 0 ? sid : 0;
}

//! The square's corners outside the circle covered by the backer: a round bubble over a square avatar. One texture per
//! size and backer color (a few per session): the backer with the alpha of its coverage outside the circle, from
//! 4 x 4 samples per pixel, so the edge is smooth
void RoundCorners(int x, int y, int size, Color backer)
{
	if (size <= 0 || size > 256)
		return;
	static std::map<unsigned long long, int> s_Masks;
	const unsigned long long key = ((unsigned long long)size << 32) | ((unsigned)backer.r() << 16) |
	                               ((unsigned)backer.g() << 8) | (unsigned)backer.b();
	auto it = s_Masks.find(key);
	if (it == s_Masks.end())
	{
		if (s_Masks.size() >= 32)
			return;
		std::vector<unsigned char> rgba((size_t)size * size * 4);
		const float r = size / 2.0f;
		for (int py = 0; py < size; py++)
		{
			for (int px = 0; px < size; px++)
			{
				int outside = 0;
				for (int sy = 0; sy < 4; sy++)
				{
					for (int sx = 0; sx < 4; sx++)
					{
						const float dx = px + (sx + 0.5f) / 4.0f - r, dy = py + (sy + 0.5f) / 4.0f - r;
						if (dx * dx + dy * dy > r * r)
							outside++;
					}
				}
				unsigned char *p = &rgba[((size_t)py * size + px) * 4];
				p[0] = (unsigned char)backer.r();
				p[1] = (unsigned char)backer.g();
				p[2] = (unsigned char)backer.b();
				p[3] = (unsigned char)(outside * 255 / 16);
			}
		}
		const int texture = vgui2::surface()->CreateNewTextureID(true);
		vgui2::surface()->DrawSetTextureRGBA(texture, rgba.data(), size, size, true, true);
		it = s_Masks.insert({ key, texture }).first;
	}
	vgui2::surface()->DrawSetTexture(it->second);
	vgui2::surface()->DrawSetColor(255, 255, 255, 255);
	vgui2::surface()->DrawTexturedRect(x, y, x + size, y + size);
}
} // namespace

void CSboServerMenu::PaintBadgeRow(const Item &it, int x, int y, int w, bool picked, Color backer)
{
	const vgui2::HFont badgeFont = Font(11.0f);
	const int bh = FontTall(badgeFont) + P(4);
	int right = x + w;

	// the voters' bubbles from the right: as many as fit next to the badges, then "+N"
	if (!it.voters.empty())
	{
		const int size = bh, step = size + P(3);
		const int count = (int)it.voters.size();
		int room = std::max(0, (w / 2) / step); // at most half the row
		int shown = std::min(count, room);
		std::wstring more;
		if (shown < count)
		{
			shown = std::max(0, room - 1);
			wchar_t t[16];
			swprintf(t, sizeof(t) / sizeof(t[0]), L"+%d", count - shown);
			more = t;
		}
		if (!more.empty())
		{
			const int mw = TextWide(badgeFont, more) + P(4);
			right -= mw;
			DrawTextMid(badgeFont, COL_TEXT_DIM, right, y, bh, more);
			right -= P(3);
		}
		for (int v = shown - 1; v >= 0; v--)
		{
			const int client = it.voters[v];
			right -= size;
			sbo::ScorePlayer sp;
			sbo::GetScorePlayer(client, sp, CClientSteamContext::IsSteamPipeEngine());
			const unsigned long long sid = sp.connected ? ValidSteamID64(sp.steamID64) : 0;
			FillRect(right, y, size, size, C_BUBBLE); // the default bubble (bots, no Steam ID, a voter who left)
			if (sid)
				DrawAvatar(sid, right, y, size);
			RoundCorners(right, y, size, backer);
			if (sp.connected && sp.name && sp.name[0])
				m_Tips.push_back({ Rect{ right, y, size, size }, Widen(CleanName(sp.name).c_str()) });
			right -= P(3);
		}
	}

	// the badges from the left (BADGES, else the STATE badge)
	std::vector<Badge> badges = it.badges;
	if (badges.empty() && !it.badge.empty())
		badges.push_back({ picked ? BADGE_GREEN : BADGE_PLAIN, it.badge, std::string() });
	int bx = x;
	for (const Badge &b : badges)
	{
		const std::wstring text = Text(b.text);
		if (text.empty() || bx >= right)
			continue;
		const int bw = std::min(TextWide(badgeFont, text) + 2 * P(6), right - bx);
		Color bg = C_BADGE_BG, fg = C_BADGE_TEXT;
		if (b.style == BADGE_GREEN)
			bg = COL_BG_PRIMARY, fg = COL_TEXT_PRIMARY;
		else if (b.style == BADGE_GOLD)
			bg = C_GOLD_BG, fg = C_GOLD_TEXT;
		else if (b.style == BADGE_DIM)
			bg = C_DIM_BG, fg = COL_TEXT_DIM;
		FillRect(bx, y, bw, bh, bg);
		DrawText(badgeFont, fg, bx + P(6), y + P(2), text, bw - 2 * P(6));
		if (!b.tip.empty())
			m_Tips.push_back({ Rect{ bx, y, bw, bh }, Text(b.tip) });
		bx += bw + P(4);
	}
}

//-----------------------------------------------------------------------------
// LAYOUT_STARS: a star rating in a compact centered window (sized to its content): the title (Close at its right
// unless MENU_NOCLOSE), the header lines centered (the first one larger), the STARS row (small stars filled to the
// average and its text), the stars, the hovered star's label under them, the menu's buttons and the key hint
//-----------------------------------------------------------------------------

//! A five-pointed star (point up) of outer radius ro around (cx, cy), filled in horizontal runs (both renderers), only
//! its part in [clipX0, clipX1)
static void FillStar(float cx, float cy, float ro, int clipX0, int clipX1, Color c)
{
	if (ro <= 0.5f || clipX1 <= clipX0)
		return;
	float vx[10], vy[10];
	const float ri = ro * 0.47f;
	for (int k = 0; k < 10; k++)
	{
		const float a = (float)(-M_PI / 2 + k * M_PI / 5);
		const float r = (k & 1) ? ri : ro;
		vx[k] = cx + r * cosf(a);
		vy[k] = cy + r * sinf(a);
	}
	vgui2::surface()->DrawSetColor(c);
	const int y0 = (int)floorf(cy - ro), y1 = (int)ceilf(cy + ro);
	for (int y = y0; y <= y1; y++)
	{
		const float yc = y + 0.5f;
		float xs[10];
		int n = 0;
		for (int k = 0; k < 10; k++)
		{
			const int j = (k + 1) % 10;
			if ((vy[k] <= yc) != (vy[j] <= yc))
				xs[n++] = vx[k] + (yc - vy[k]) * (vx[j] - vx[k]) / (vy[j] - vy[k]);
		}
		std::sort(xs, xs + n);
		for (int i = 0; i + 1 < n; i += 2)
		{
			const int a = std::max(clipX0, (int)lroundf(xs[i])), b = std::min(clipX1, (int)lroundf(xs[i + 1]));
			if (b > a)
				vgui2::surface()->DrawFilledRect(a, y, b, y + 1);
		}
	}
}

//! A star in box (x, y, size): its rim and its inside, lit up to x + litW (0: empty, size: whole)
static void DrawStar(int x, int y, int size, int litW, Color rim, Color empty, Color lit, Color litRim)
{
	const float cx = x + size / 2.0f, cy = y + size * 0.53f, ro = size / 2.0f;
	const float rimW = std::max(1.0f, size / 16.0f);
	const int split = x + std::max(0, std::min(size, litW));
	FillStar(cx, cy, ro, x, split, litRim);
	FillStar(cx, cy, ro, split, x + size + 1, rim);
	FillStar(cx, cy, ro - 1.6f * rimW, x, split, lit);
	FillStar(cx, cy, ro - 1.6f * rimW, split, x + size + 1, empty);
}

void CSboServerMenu::LayoutStars()
{
	const Menu *m = M();
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	// nothing of the other layouts: no tabs, search, controls, side panel, preview, Back or primary button
	m_rTabs.clear();
	m_rSearch = m_rPrimary = m_rBack = m_rPanel = m_rPreview = Rect();
	m_pSearch->SetVisible(false);
	for (int f = 0; f < MAX_MENU_FIELDS; f++)
	{
		m_rFields[f] = m_rFieldButtons[f] = Rect();
		m_pFields[f]->SetVisible(false);
	}
	for (int i = 0; i < MAX_MENU_SLIDERS; i++)
		m_rSliders[i] = m_rSliderTracks[i] = Rect();
	for (int i = 0; i < MAX_MENU_TOGGLES; i++)
		m_rToggles[i] = Rect();
	for (int i = 0; i < MAX_MENU_SELECTS; i++)
		m_rSelects[i] = m_rSelectBoxes[i] = Rect();
	Refilter();

	const int padX = P(28), padY = P(20), gap = P(14);
	const vgui2::HFont titleFont = Font(24.0f), firstFont = Font(20.0f), lineFont = Font(13.0f);
	const vgui2::HFont labelFont = Font(15.0f), buttonFont = Font(14.0f), hintFont = Font(12.0f);
	const int stars = std::max(1, (int)m_Visible.size());
	m_iStarSize = P(46);
	const int starGap = P(10);
	const int rowW = stars * m_iStarSize + (stars - 1) * starGap;

	// the widest part sets the window's width (at least 440, at most the screen)
	const int closeW = (m->flags & MENU_NOCLOSE) ? 0 : P(32);
	int contentW = std::max(rowW, TextWide(titleFont, Text(m->title)) + 2 * (closeW + P(12)));
	for (int i = 0; i < MAX_HEADERS; i++)
	{
		if (!m->headers[i].empty())
			contentW = std::max(contentW, TextWide(i == 0 ? firstFont : lineFont, Text(m->headers[i])));
	}
	const bool average = m->starsTenths != STARS_NONE || !m->starsText.empty();
	const int smallStar = P(16);
	if (average)
		contentW = std::max(contentW, stars * (smallStar + P(3)) + P(8) + TextWide(lineFont, Text(m->starsText)));
	int buttonsW = 0;
	for (const std::string &b : m->buttons)
		buttonsW += TextWide(buttonFont, Text(b)) + 2 * P(20) + P(10);
	contentW = std::max(contentW, buttonsW);
	m_rWin.w = std::min(sw - P(16), std::max(P(440), contentW + 2 * padX));
	const int innerX = (sw - m_rWin.w) / 2 + padX, innerW = m_rWin.w - 2 * padX;

	// heights from the top of the window
	const int titleH = std::max(P(32), FontTall(titleFont));
	int y = padY + titleH + P(8);
	m_iHeaderY = y;
	for (int i = 0; i < MAX_HEADERS; i++)
	{
		if (!m->headers[i].empty())
			y += FontTall(i == 0 ? firstFont : lineFont) + P(4);
	}
	m_iStarsY = 0;
	m_iStarsH = 0;
	if (average)
	{
		y += P(4);
		m_iStarsY = y;
		m_iStarsH = std::max(smallStar, FontTall(lineFont));
		y += m_iStarsH;
	}
	y += gap + P(4);
	const int areaY = y;
	const int areaH = m_iStarSize + P(10) + FontTall(labelFont);
	y += areaH + gap;
	const int bh = P(36);
	const int bottomY = y;
	y += bh + padY;
	m_rWin.h = std::min(sh - P(16), y);
	m_rWin.x = (sw - m_rWin.w) / 2;
	m_rWin.y = (sh - m_rWin.h) / 2;

	// to the screen
	m_iHeaderY += m_rWin.y;
	if (m_iStarsY)
		m_iStarsY += m_rWin.y;
	m_rClose = Rect();
	if (closeW)
		m_rClose = { m_rWin.x + m_rWin.w - padX - closeW, m_rWin.y + padY + (titleH - closeW) / 2, closeW, closeW };
	m_rButtons.clear();
	int bx = innerX + innerW + P(10);
	for (int b = (int)m->buttons.size() - 1; b >= 0; b--)
	{
		const int w = TextWide(buttonFont, Text(m->buttons[b])) + 2 * P(20);
		bx -= P(10) + w;
		m_rButtons.insert(m_rButtons.begin(), { bx, m_rWin.y + bottomY, w, bh });
	}
	m_rHint = { innerX, m_rWin.y + bottomY, std::max(0, bx - innerX - P(10)), bh };
	(void)hintFont;
	m_rArea = { m_rWin.x + (m_rWin.w - rowW) / 2, m_rWin.y + areaY, rowW, areaH };
	m_pArea->SetBounds(m_rArea.x, m_rArea.y, m_rArea.w, m_rArea.h);
	m_pArea->SetVisible(true);
	LayoutArea();
}

void CSboServerMenu::PaintStars()
{
	const Menu *m = M();
	const int padY = P(20);
	FillRect(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, COL_BG_WINDOW);
	OutlineRect(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, COL_BORDER);

	// the title centered, Close at the right
	const vgui2::HFont titleFont = Font(24.0f), firstFont = Font(20.0f), lineFont = Font(13.0f);
	const int titleH = std::max(P(32), FontTall(titleFont));
	const int maxW = m_rWin.w - 2 * (P(28) + (m_rClose.w ? m_rClose.w + P(12) : 0));
	const std::wstring title = Text(m->title);
	const int tw = std::min(TextWide(titleFont, title), maxW);
	DrawTextMid(titleFont, COL_TEXT, m_rWin.x + (m_rWin.w - tw) / 2, m_rWin.y + padY, titleH, title, maxW);
	if (m_rClose.w)
	{
		if (m_rClose.Has(m_iMouseX, m_iMouseY))
			FillRect(m_rClose.x, m_rClose.y, m_rClose.w, m_rClose.h, COL_BG_SELECTED);
		OutlineRect(m_rClose.x, m_rClose.y, m_rClose.w, m_rClose.h, COL_BORDER);
		DrawX(m_rClose.x + m_rClose.w / 2, m_rClose.y + m_rClose.h / 2, P(12), COL_TEXT);
	}

	// the header lines centered: the first bright and larger (e.g. the map), the others dim
	const int innerW = m_rWin.w - 2 * P(28);
	int y = m_iHeaderY;
	for (int i = 0; i < MAX_HEADERS; i++)
	{
		if (m->headers[i].empty())
			continue;
		const vgui2::HFont f = i == 0 ? firstFont : lineFont;
		const std::wstring w = Text(m->headers[i]);
		const int lw = std::min(TextWide(f, w), innerW);
		DrawText(f, i == 0 ? COL_TEXT_SELECTED : COL_TEXT_DIM, m_rWin.x + (m_rWin.w - lw) / 2, y, w, innerW);
		y += FontTall(f) + P(4);
	}

	// the STARS row: small stars filled to the average, then its text
	if (m_iStarsH)
	{
		const int stars = std::max(1, (int)m_Visible.size());
		const int size = P(16), step = size + P(3);
		const std::wstring text = Text(m->starsText);
		const int textW = text.empty() ? 0 : TextWide(lineFont, text) + P(8);
		const int rowW = std::min(innerW, stars * step - P(3) + textW);
		int x = m_rWin.x + (m_rWin.w - rowW) / 2;
		const int sy = m_iStarsY + (m_iStarsH - size) / 2;
		const int tenths = m->starsTenths == STARS_NONE ? 0 : m->starsTenths;
		for (int s = 0; s < stars; s++)
		{
			const int lit = std::max(0, std::min(10, tenths - 10 * s)) * size / 10;
			DrawStar(x, sy, size, lit, COL_BORDER_LIST, COL_BG_INPUT, COL_TEXT, COL_BORDER_PRIMARY);
			x += step;
		}
		if (!text.empty())
			DrawTextMid(lineFont, COL_TEXT_LIGHT, x - P(3) + P(8), m_iStarsY, m_iStarsH, text,
				std::max(1, m_rWin.x + m_rWin.w - P(28) - (x + P(5))));
	}

	// buttons and the key hint
	for (size_t b = 0; b < m_rButtons.size() && b < m->buttons.size(); b++)
		PaintButton(m_rButtons[b], Text(m->buttons[b]), false, m->ButtonEnabled((int)b), m_rButtons[b].Has(m_iMouseX, m_iMouseY));
	if (m_rHint.w)
	{
		const vgui2::HFont f = Font(12.0f);
		wchar_t keys[16];
		swprintf(keys, sizeof(keys) / sizeof(keys[0]), L"1-%d", std::max(1, std::min(9, (int)m_Visible.size())));
		std::wstring hint = std::wstring(L"Click a star or press ") + keys + L" \u00b7 Esc closes";
		if (TextWide(f, hint) > m_rHint.w)
			hint = std::wstring(keys) + L" \u00b7 Esc closes";
		DrawTextMid(f, COL_TEXT_DIM, m_rHint.x, m_rHint.y, m_rHint.h, hint, m_rHint.w);
	}
	if (m_iConfirmKind < 0 && m_iDrop < 0 && m_Focus.kind != F_ITEMS)
		PaintFocusRing(FocusRect(m_Focus));

	// a button's hover text
	if (vgui2::input()->GetMouseOver() == GetVPanel())
	{
		int mx, my;
		vgui2::input()->GetCursorPos(mx, my);
		ScreenToLocal(mx, my);
		m_iMouseX = mx;
		m_iMouseY = my;
		for (size_t b = 0; b < m_rButtons.size() && b < m->buttonTips.size(); b++)
		{
			if (!m_rButtons[b].Has(m_iMouseX, m_iMouseY) || m->buttonTips[b].empty())
				continue;
			const vgui2::HFont f = Font(12.0f);
			const std::wstring tip = Text(m->buttonTips[b]);
			const int tw2 = TextWide(f, tip) + 2 * P(8), th = FontTall(f) + P(8);
			const int tx = std::max(P(4), m_iMouseX - tw2 / 2), ty = m_rButtons[b].y - th - P(4);
			FillRect(tx, ty, tw2, th, COL_BG_INPUT);
			OutlineRect(tx, ty, tw2, th, COL_BORDER);
			DrawTextMid(f, COL_TEXT_LIGHT, tx + P(8), ty, th, tip, tw2 - 2 * P(8));
			break;
		}
	}
}

void CSboServerMenu::PaintStarArea()
{
	const Menu *m = M();
	// lit up to: the star under the mouse, else the keyboard's (Tab / arrows on the stars), else the player's
	// own rating (ITEM_PICKED)
	int upTo = -1, named = -1;
	for (size_t n = 0; n < m_Visible.size(); n++)
	{
		if (m_Visible[n] == m_iHover && CanPick(m_iHover))
			upTo = named = (int)n;
	}
	if (upTo < 0 && m_bKeyNav && m_Focus.kind == F_ITEMS)
	{
		for (size_t n = 0; n < m_Visible.size(); n++)
		{
			if (m_Visible[n] == m_iHighlight)
				upTo = named = (int)n;
		}
	}
	bool picked = false;
	if (upTo < 0)
	{
		for (size_t n = 0; n < m_Visible.size(); n++)
		{
			if (m->items[m_Visible[n]].flags & ITEM_PICKED)
				upTo = (int)n, picked = true;
		}
	}
	const int gap = P(10);
	for (size_t n = 0; n < m_Visible.size() && n < m_Cards.size(); n++)
	{
		const Item &it = m->items[m_Visible[n]];
		const bool off = !CanPick(m_Visible[n]);
		const int x = (int)n * (m_iStarSize + gap);
		const bool lit = (int)n <= upTo && !off;
		DrawStar(x, 0, m_iStarSize, lit ? m_iStarSize : 0, off ? COL_BG_SELECTED : COL_BORDER, COL_BG_INPUT,
			picked ? COL_TEXT : COL_TEXT_BRIGHT, COL_BORDER_PRIMARY);
		(void)it;
	}
	// the hovered (or keyboard's) star's label under the row
	if (named >= 0)
	{
		const vgui2::HFont f = Font(15.0f);
		const std::wstring label = Text(m->items[m_Visible[named]].label);
		const int lw = std::min(TextWide(f, label), m_rArea.w);
		DrawText(f, COL_TEXT_SELECTED, (m_rArea.w - lw) / 2, m_iStarSize + P(10), label, m_rArea.w);
	}
}

void CSboServerMenu::PaintRow(int item, const Rect &r)
{
	const Item &it = M()->items[item];
	const bool on = item == m_iHighlight;
	const float cooldown = it.CooldownLeft(ServerMenu_Now());
	const bool off = (it.flags & ITEM_DISABLED) != 0 || cooldown > 0.0f;
	if (on)
		FillRect(r.x, r.y, r.w, r.h, COL_BG_SELECTED);
	if (cooldown > 0.0f)
		FillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, C_COOL_SHADE); // greyed under its dim texts
	OutlineRect(r.x, r.y, r.w, r.h, on ? COL_BORDER_PRIMARY : (item == m_iHover ? COL_BORDER : COL_BORDER_LIST));
	const vgui2::HFont f = Font(Side() ? 13.0f : 14.0f), small = Font(12.0f), badgeFont = Font(11.0f);
	const int pad = P(10);
	const std::wstring price = Text(it.price), badge = Text(it.badge);
	int right = r.x + r.w - pad;
	// cooling down: a small clock at the row's right end (drawn last, over the greyed row)
	const int clockRadius = std::max(P(6), r.h * 40 / 100), clockX = right - clockRadius;
	if (cooldown > 0.0f)
		right -= 2 * clockRadius + P(10);
	if (!price.empty())
	{
		const int pw = TextWide(small, price);
		DrawTextMid(small, off ? C_DIM_TEXT : C_BADGE_TEXT, right - pw, r.y, r.h, price);
		right -= pw + P(10);
	}
	if (!badge.empty())
	{
		const bool picked = (it.flags & ITEM_PICKED) != 0;
		const int bw = TextWide(badgeFont, badge) + 2 * P(6), bh = FontTall(badgeFont) + P(4);
		FillRect(right - bw, r.y + (r.h - bh) / 2, bw, bh, picked ? COL_BG_PRIMARY : C_BADGE_BG);
		DrawTextMid(badgeFont, picked ? COL_TEXT_PRIMARY : C_BADGE_TEXT, right - bw + P(6), r.y, r.h, badge);
		right -= bw + P(10);
	}
	int labelX = r.x + pad;
	if (it.avatar)
	{
		const int as = std::min(r.h - P(8), FontTall(f) + P(4));
		DrawAvatar(it.avatar, labelX, r.y + (r.h - as) / 2, as);
		labelX += as + P(8);
	}
	if (!it.image.empty())
	{
		// the item's picture (ITEMPICTURE) as a square icon
		const int is = std::min(r.h - P(8), FontTall(f) + P(8));
		PaintFilePicture(it.image, { labelX, r.y + (r.h - is) / 2, is, is }, off, m_rArea.w, m_rArea.h);
		labelX += is + P(8);
	}
	DrawTextMid(f, off ? C_DIM_TEXT : (on ? COL_TEXT_SELECTED : COL_TEXT_LIGHT), labelX, r.y, r.h, Text(it.label),
		std::max(1, right - labelX));
	if (cooldown > 0.0f)
	{
		DrawCooldownClock(clockX, r.y + r.h / 2, clockRadius, cooldown, it.cooldownTotal, Font(11.0f, 700));
	}
}

void CSboServerMenu::PaintArea()
{
	const Menu *m = M();
	if (!m)
		return;
	m_iNewMapPictures = 0;
	m_Tips.clear();
	if (m->layout == LAYOUT_STARS)
	{
		PaintStarArea();
		return;
	}
	for (size_t n = 0; n < m_Visible.size() && n < m_Cards.size(); n++)
	{
		Rect r = m_Cards[n];
		r.y -= m_iScroll;
		if (r.y + r.h < 0 || r.y > m_rArea.h)
			continue;
		if (m->layout == LAYOUT_GRID)
			PaintCard(m_Visible[n], r);
		else
			PaintRow(m_Visible[n], r);
	}
	// the hover text of the badge or voter under the mouse, above the cursor, kept inside the area
	for (const auto &tip : m_Tips)
	{
		if (!tip.first.Has(m_iAreaMouseX, m_iAreaMouseY) || tip.second.empty())
			continue;
		const vgui2::HFont f = Font(12.0f);
		const int tw = TextWide(f, tip.second) + 2 * P(8), th = FontTall(f) + P(8);
		const int tx = std::max(0, std::min(m_rArea.w - tw, m_iAreaMouseX - tw / 2));
		int ty = tip.first.y - th - P(4);
		if (ty < 0)
			ty = tip.first.y + tip.first.h + P(4);
		FillRect(tx, ty, tw, th, COL_BG_INPUT);
		OutlineRect(tx, ty, tw, th, COL_BORDER);
		DrawTextMid(f, COL_TEXT_LIGHT, tx + P(8), ty, th, tip.second, tw - 2 * P(8));
		break;
	}
	// scroll bar
	if (m_iContent > m_rArea.h && m_rArea.h > 0)
	{
		const int bw = P(8), bx = m_rArea.w - bw;
		FillRect(bx, 0, bw, m_rArea.h, COL_BG_INPUT);
		const int th = std::max(bw * 2, m_rArea.h * m_rArea.h / m_iContent);
		const int ty = (m_rArea.h - th) * m_iScroll / std::max(1, m_iContent - m_rArea.h);
		FillRect(bx + 2, ty + 2, bw - 4, th - 4, COL_BORDER);
	}
}

//-----------------------------------------------------------------------------
// Input
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// The item popup (MENU_POPUP): 600 wide
// over the darkened window: the label and Close, the item's map picture large, its ITEMLINE, its text, Back and the
// primary action
//-----------------------------------------------------------------------------
namespace
{
//! text wrapped by words to maxW (its own line breaks kept), at most maxLines lines (the last one cut with "...")
std::vector<std::wstring> WrapText(vgui2::HFont f, const std::wstring &text, int maxW, int maxLines)
{
	std::vector<std::wstring> lines;
	size_t start = 0;
	while (start <= text.size() && (int)lines.size() < maxLines)
	{
		const size_t nl = text.find(L'\n', start);
		const std::wstring para = text.substr(start, nl == std::wstring::npos ? std::wstring::npos : nl - start);
		size_t p = 0;
		if (para.empty())
			lines.push_back(L"");
		while (p < para.size() && (int)lines.size() < maxLines)
		{
			size_t end = p, lastSpace = std::wstring::npos;
			while (end < para.size() && TextWide(f, para.substr(p, end - p + 1)) <= maxW)
			{
				if (para[end] == L' ')
					lastSpace = end;
				end++;
			}
			if (end < para.size() && lastSpace != std::wstring::npos && lastSpace > p)
				end = lastSpace + 1;
			if (end == p)
				end = p + 1;
			lines.push_back(para.substr(p, end - p));
			p = end;
		}
		if (nl == std::wstring::npos)
			break;
		start = nl + 1;
	}
	while (!lines.empty() && lines.back().empty())
		lines.pop_back();
	return lines;
}

//! Widen for a text of any length (Widen converts at most 511 characters): in pieces cut at UTF-8 character starts
std::wstring WidenAll(const std::string &utf8)
{
	std::wstring out;
	size_t pos = 0;
	while (pos < utf8.size())
	{
		size_t end = std::min(utf8.size(), pos + 400);
		while (end < utf8.size() && end > pos && ((unsigned char)utf8[end] & 0xC0) == 0x80)
			end--;
		out += Widen(utf8.substr(pos, end - pos).c_str());
		pos = end;
	}
	return out;
}

//! The text with each http:// or https:// URL (one IsSafeChatURL accepts) replaced by a short label: the host without
//! "www." and the path's start, at most 32 characters with "…"; the labels and their URLs appended to links
template <class Link>
std::wstring Linkify(const std::wstring &text, std::vector<Link> &links)
{
	std::wstring out;
	size_t pos = 0;
	while (pos < text.size())
	{
		size_t at = std::wstring::npos;
		for (const wchar_t *scheme : { L"https://", L"http://" })
			at = std::min(at, text.find(scheme, pos));
		if (at == std::wstring::npos)
			break;
		size_t end = at;
		while (end < text.size() && text[end] > L' ' && text[end] < 0x7F && text[end] != L'"' && text[end] != L'<' &&
		       text[end] != L'>')
			end++;
		const std::wstring w = text.substr(at, end - at);
		std::string url(w.begin(), w.end()); // ASCII only (the loop above)
		out += text.substr(pos, at - pos);
		if (IsSafeChatURL(url.c_str()) && links.size() < 32)
		{
			std::wstring label = w.substr(w.find(L"://") + 3);
			if (label.compare(0, 4, L"www.") == 0)
				label = label.substr(4);
			while (!label.empty() && label.back() == L'/')
				label.pop_back();
			if (label.size() > 32)
				label = label.substr(0, 31) + L"\u2026";
			links.push_back({ label, url });
			out += label;
		}
		else
			out += w;
		pos = end;
	}
	out += text.substr(std::min(pos, text.size()));
	return out;
}

constexpr int POPUP_TEXT_LINES = 1000;   //!< the whole text (the joining screen's cap: 400 source lines), scrolled
constexpr int POPUP_SHOWN_LINES = 8;     //!< the text area's height in lines at most (less on a small screen)
constexpr unsigned BRIEFING_MAX_BYTES = 64 * 1024; //!< as the joining screen reads it
constexpr size_t POPUP_TITLE_BYTES = 47; //!< an ITEMTITLE longer than this is cut in the popup's header ("...") and
                                         //!< starts its text in full
} // namespace

std::wstring CSboServerMenu::PopupHeader(const Item &it)
{
	if (it.title.empty())
		return Text(it.label);
	std::string head = it.title;
	if (head.size() > POPUP_TITLE_BYTES)
	{
		size_t cut = POPUP_TITLE_BYTES;
		while (cut > 0 && ((unsigned char)head[cut] & 0xC0) == 0x80)
			cut--;
		head = head.substr(0, cut);
		while (!head.empty() && head.back() == ' ')
			head.pop_back();
		return Text(head) + L"\u2026";
	}
	return Text(head);
}

const std::wstring &CSboServerMenu::PopupText(const Item &it)
{
	if (it.map.empty())
	{
		// the full title (ITEMTITLE) first when the header had to cut it (PopupHeader), then the description, each of
		// its lines kept (Text drops control characters, '\n' too: the lines are converted one by one)
		m_BriefingMap.clear();
		m_Briefing.clear();
		const bool headerCut = it.title.size() > POPUP_TITLE_BYTES;
		std::string all = headerCut ? it.title + "\n" + it.text : it.text;
		size_t start = 0;
		while (start <= all.size())
		{
			const size_t nl = all.find('\n', start);
			const std::string line = all.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
			if (!m_Briefing.empty() || start > 0)
				m_Briefing += L"\n";
			m_Briefing += Text(line);
			if (nl == std::string::npos)
				break;
			start = nl + 1;
		}
		return m_Briefing;
	}
	if (m_BriefingMap == it.map)
		return m_Briefing;
	m_BriefingMap = it.map;
	m_Briefing.clear();
	// the map's own text (the briefing SBO maps ship, which the joining screen shows): this client's copy, if any
	char path[128];
	snprintf(path, sizeof(path), "maps/%s.txt", it.map.c_str());
	std::string text;
	if (g_pFullFileSystem && g_pFullFileSystem->FileExists(path))
	{
		if (FileHandle_t fh = g_pFullFileSystem->Open(path, "rb"))
		{
			const unsigned size = std::min(g_pFullFileSystem->Size(fh), BRIEFING_MAX_BYTES);
			text.resize(size);
			if (size > 0 && g_pFullFileSystem->Read(&text[0], (int)size, fh) != (int)size)
				text.clear();
			g_pFullFileSystem->Close(fh);
		}
	}
	std::string out;
	size_t start = 0;
	while (start < text.size())
	{
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();
		std::string line = CleanName(text.substr(start, end - start).c_str());
		while (!line.empty() && line.back() == ' ')
			line.pop_back();
		// paragraphs kept, runs of empty lines as one
		if (!line.empty() || (!out.empty() && out.compare(out.size() - std::min<size_t>(out.size(), 2), 2, "\n\n") != 0))
			out += line + "\n";
		start = end + 1;
	}
	m_Briefing = WidenAll(out);
	if (m_Briefing.find_first_not_of(L" \n") == std::wstring::npos)
		m_Briefing = Text(it.text); // no briefing here: the item's description
	return m_Briefing;
}

void CSboServerMenu::OpenPopup(int item)
{
	const Menu *m = M();
	if (!m || !Popups() || item < 0 || item >= (int)m->items.size() || !m->items[item].defined)
		return;
	m_iPopup = item;
	m_flPopupAt = ServerMenu_Now();
	m_PopFocus = { m->items[item].action.empty() ? F_POP_BACK : F_POP_PRIMARY, 0 };
	m_iPopScroll = 0;
	EndSearchTyping();
	LayoutPopup();
	m_pPopup->SetVisible(true);
	m_pPopup->MoveToFront();
}

//-----------------------------------------------------------------------------
// ITEMPICTURE files, dropdowns, the confirm dialog
//-----------------------------------------------------------------------------
const CSboServerMenu::FileImage *CSboServerMenu::GetFileImage(const std::string &path)
{
	if (path.empty())
		return nullptr;
	auto it = m_FileImages.find(path);
	if (it != m_FileImages.end() && (it->second.texture != -1 || ServerMenu_Now() < it->second.retryAt))
		return it->second.texture != -1 ? &it->second : nullptr;
	if (it == m_FileImages.end())
	{
		if (m_FileImages.size() >= 128)
			return nullptr; // a plugin naming ever more files: the placeholder
		it = m_FileImages.insert({ path, FileImage() }).first;
	}
	FileImage &img = it->second;
	img.retryAt = ServerMenu_Now() + 3.0f;
	std::vector<uint8_t> data, rgba;
	FileHandle_t fh = g_pFullFileSystem->FileExists(path.c_str()) ? g_pFullFileSystem->Open(path.c_str(), "rb") : nullptr;
	if (!fh)
		fh = g_pFullFileSystem->Open(path.c_str(), "rb", "GAMEDOWNLOAD"); // a FastDL download
	if (!fh)
		return nullptr;
	const unsigned size = g_pFullFileSystem->Size(fh);
	if (size > 0 && size <= (unsigned)MAX_PICTURE_FILE)
	{
		data.resize(size);
		if (g_pFullFileSystem->Read(data.data(), (int)size, fh) != (int)size)
			data.clear();
	}
	g_pFullFileSystem->Close(fh);
	int w = 0, h = 0;
	if (data.empty() || !sbo_image::DecodeTgaScaled(data.data(), data.size(), MAX_PICTURE_SIDE / 2, MAX_PICTURE_SIDE / 2,
	                                               rgba, w, h) || w <= 0 || h <= 0)
		return nullptr;
	img.texture = vgui2::surface()->CreateNewTextureID(true);
	vgui2::surface()->DrawSetTextureRGBA(img.texture, rgba.data(), w, h, true, true);
	img.wide = w;
	img.tall = h;
	return &img;
}

void CSboServerMenu::PaintFilePicture(const std::string &path, const Rect &box, bool off, int clipW, int clipH)
{
	if (box.w <= 0 || box.h <= 0)
		return;
	{
		// the box (also the placeholder), cut to the clip
		const int x0 = std::max(box.x, 0), y0 = std::max(box.y, 0);
		const int x1 = std::min(box.x + box.w, clipW), y1 = std::min(box.y + box.h, clipH);
		if (x1 > x0 && y1 > y0)
			FillRect(x0, y0, x1 - x0, y1 - y0, COL_BG_INPUT);
	}
	const FileImage *pic = GetFileImage(path);
	if (!pic)
		return;
	// whole, its shape kept, never larger than twice its size (an icon stays sharp)
	const float f = std::min({ (float)box.w / pic->wide, (float)box.h / pic->tall, 2.0f });
	const float dw = pic->wide * f, dh = pic->tall * f;
	const float dx = box.x + (box.w - dw) / 2, dy = box.y + (box.h - dh) / 2;
	const float x0 = std::max(dx, 0.0f), y0 = std::max(dy, 0.0f);
	const float x1 = std::min(dx + dw, (float)clipW), y1 = std::min(dy + dh, (float)clipH);
	if (x1 <= x0 || y1 <= y0)
		return;
	const float s0 = (x0 - dx) / dw, s1 = (x1 - dx) / dw, t0 = (y0 - dy) / dh, t1 = (y1 - dy) / dh;
	vgui2::VGuiVertex v[4] = { vgui2::VGuiVertex((int)x0, (int)y0, s0, t0), vgui2::VGuiVertex((int)x1, (int)y0, s1, t0),
		vgui2::VGuiVertex((int)x1, (int)y1, s1, t1), vgui2::VGuiVertex((int)x0, (int)y1, s0, t1) };
	vgui2::surface()->DrawSetTexture(pic->texture);
	if (off)
		vgui2::surface()->DrawSetColor(90, 96, 90, 255);
	else
		vgui2::surface()->DrawSetColor(255, 255, 255, 255);
	vgui2::surface()->DrawTexturedPolygon(v, 4);
}

int CSboServerMenu::SelectShown(int sel) const
{
	const Menu *m = M();
	if (!m || sel < 0 || sel >= (int)m->selects.size())
		return 0;
	const Select &d = m->selects[sel];
	const int v = SentShows(m_SelectSent[sel], d.updates) ? m_SelectSent[sel].value : d.value;
	return v >= 0 && v < (int)d.options.size() ? v : 0;
}

namespace
{
//! A small down-pointing triangle (a dropdown's arrow), its top centered at (cx, y)
void DrawArrowDown(int cx, int y, int size, Color c)
{
	for (int r = 0; r < size; r++)
		FillRect(cx - (size - r), y + r, 2 * (size - r), 1, c);
}
} // namespace

void CSboServerMenu::PaintSelect(int sel, const Rect &row, const Rect &box, bool hot)
{
	const Menu *m = M();
	if (!m || sel >= (int)m->selects.size())
		return;
	const Select &d = m->selects[sel];
	const vgui2::HFont f = Font(Side() ? 12.0f : 13.0f);
	if (!d.label.empty())
		DrawTextMid(f, hot ? COL_TEXT : COL_TEXT_DIM, row.x, row.y, row.h, Text(d.label), std::max(1, box.x - row.x - P(8)));
	FillRect(box.x, box.y, box.w, box.h, hot ? COL_BG_SELECTED : COL_BG_INPUT);
	OutlineRect(box.x, box.y, box.w, box.h, m_iDrop == sel ? COL_BORDER_PRIMARY : COL_BORDER);
	const int as = std::max(3, P(5));
	DrawArrowDown(box.x + box.w - P(14), box.y + (box.h - as) / 2, as, COL_TEXT_LIGHT);
	const int v = SelectShown(sel);
	if (v < (int)d.options.size())
		DrawTextMid(f, COL_TEXT_SELECTED, box.x + P(10), box.y, box.h, Text(d.options[v]), std::max(1, box.w - P(36)));
}

void CSboServerMenu::OpenDrop(int sel, const Rect &box)
{
	const Menu *m = M();
	if (!m || sel < 0 || sel >= (int)m->selects.size() || !m->selects[sel].defined)
		return;
	if (m_iDrop == sel)
	{
		CloseOverlay(); // a second click on the box closes its list
		return;
	}
	CloseOverlay();
	const int n = (int)m->selects[sel].options.size();
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	// under the box, else above it; rows as tall as the box, smaller when the screen is short
	int rowH = box.h;
	const int below = sh - P(8) - (box.y + box.h), above = box.y - P(8);
	if (n * rowH > below && n * rowH > above)
		rowH = std::max(FontTall(Font(12.0f)) + P(2), std::max(below, above) / std::max(1, n));
	const int h = n * rowH;
	const int y = h <= below || below >= above ? box.y + box.h : box.y - h;
	m_rDrop = { box.x, y, box.w, h };
	m_rDropOptions.clear();
	for (int o = 0; o < n; o++)
		m_rDropOptions.push_back({ box.x, y + o * rowH, box.w, rowH });
	m_iDrop = sel;
	m_iDropHover = SelectShown(sel);
	m_pOverlay->SetBounds(0, 0, GetWide(), GetTall());
	m_pOverlay->SetVisible(true);
	m_pOverlay->MoveToFront();
}

void CSboServerMenu::CloseOverlay()
{
	m_iDrop = -1;
	m_iDropHover = -1;
	m_iConfirmKind = -1;
	m_iConfirmIndex = m_iConfirmItem = -1;
	if (m_pOverlay)
		m_pOverlay->SetVisible(false);
}

bool CSboServerMenu::AskConfirm(int kind, int index, int item, const std::string &question)
{
	if (question.empty())
		return false;
	CloseOverlay();
	m_iConfirmKind = kind;
	m_iConfirmIndex = index;
	m_iConfirmItem = item;
	m_bConfirmNoFocus = false;
	m_ConfirmText = Text(question);
	// centered over the window (a docked one too), the question wrapped, No and Yes at the bottom right
	const int padX = P(22), padY = P(18), gap = P(14), bh = P(40);
	const int w = std::min(m_rWin.w - P(24), P(440)), innerW = w - 2 * padX;
	const vgui2::HFont f = Font(16.0f);
	m_ConfirmLines = WrapText(f, m_ConfirmText, innerW, 6);
	const int h = padY + (int)m_ConfirmLines.size() * FontTall(f) + gap + bh + padY;
	m_rConfirm = { m_rWin.x + (m_rWin.w - w) / 2, m_rWin.y + (m_rWin.h - h) / 2, w, h };
	const vgui2::HFont bf = Font(14.0f);
	const int yesW = std::max(P(90), TextWide(bf, L"Yes") + 2 * P(24)), noW = std::max(P(90), TextWide(bf, L"No") + 2 * P(24));
	const int by = m_rConfirm.y + h - padY - bh;
	m_rConfirmYes = { m_rConfirm.x + w - padX - yesW, by, yesW, bh };
	m_rConfirmNo = { m_rConfirmYes.x - P(10) - noW, by, noW, bh };
	m_pOverlay->SetBounds(0, 0, GetWide(), GetTall());
	m_pOverlay->SetVisible(true);
	m_pOverlay->MoveToFront();
	return true;
}

void CSboServerMenu::ConfirmAnswer(bool yes)
{
	const int kind = m_iConfirmKind, index = m_iConfirmIndex, item = m_iConfirmItem;
	CloseOverlay();
	if (!yes || kind < 0)
		return;
	const Menu *m = M();
	if (!m)
		return;
	if (kind == CONFIRM_ITEM)
	{
		if (!CanPick(index))
			return; // it changed meanwhile (disabled, cooling down)
		ClosePopup();
		SendPick(index);
	}
	else if (m->ButtonEnabled(index))
		SendButton(index, item); // greyed meanwhile: nothing
}

void CSboServerMenu::PaintOverlay()
{
	const Menu *m = M();
	if (!m)
		return;
	if (m_iConfirmKind >= 0)
	{
		FillRect(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, Color(0, 0, 0, 150));
		FillRect(m_rConfirm.x, m_rConfirm.y, m_rConfirm.w, m_rConfirm.h, COL_BG_WINDOW);
		OutlineRect(m_rConfirm.x, m_rConfirm.y, m_rConfirm.w, m_rConfirm.h, COL_BORDER_PRIMARY);
		const vgui2::HFont f = Font(16.0f);
		for (size_t l = 0; l < m_ConfirmLines.size(); l++)
			DrawText(f, COL_TEXT_SELECTED, m_rConfirm.x + P(22), m_rConfirm.y + P(18) + (int)l * FontTall(f), m_ConfirmLines[l]);
		PaintButton(m_rConfirmNo, L"No", false, true, m_rConfirmNo.Has(m_iOverlayX, m_iOverlayY));
		PaintButton(m_rConfirmYes, L"Yes", true, true, m_rConfirmYes.Has(m_iOverlayX, m_iOverlayY));
		PaintFocusRing(m_bConfirmNoFocus ? m_rConfirmNo : m_rConfirmYes);
		return;
	}
	if (m_iDrop < 0 || m_iDrop >= (int)m->selects.size())
		return;
	const Select &d = m->selects[m_iDrop];
	FillRect(m_rDrop.x, m_rDrop.y, m_rDrop.w, m_rDrop.h, COL_BG_WINDOW);
	const vgui2::HFont f = Font(Side() ? 12.0f : 13.0f);
	const int shown = SelectShown(m_iDrop);
	for (size_t o = 0; o < m_rDropOptions.size() && o < d.options.size(); o++)
	{
		const Rect &r = m_rDropOptions[o];
		if ((int)o == m_iDropHover)
			FillRect(r.x, r.y, r.w, r.h, COL_BG_SELECTED);
		DrawTextMid(f, (int)o == shown ? COL_TEXT_PRIMARY : COL_TEXT_LIGHT, r.x + P(10), r.y, r.h, Text(d.options[o]),
			std::max(1, r.w - P(20)));
	}
	OutlineRect(m_rDrop.x, m_rDrop.y, m_rDrop.w, m_rDrop.h, COL_BORDER_PRIMARY);
}

void CSboServerMenu::OverlayMoved(int x, int y)
{
	m_iOverlayX = x;
	m_iOverlayY = y;
	if (m_iDrop >= 0)
	{
		for (size_t o = 0; o < m_rDropOptions.size(); o++)
		{
			if (m_rDropOptions[o].Has(x, y))
				m_iDropHover = (int)o;
		}
	}
}

void CSboServerMenu::OverlayPressed(int x, int y)
{
	const Menu *m = M();
	if (m_iConfirmKind >= 0)
	{
		if (m_rConfirmYes.Has(x, y))
			ConfirmAnswer(true);
		else if (m_rConfirmNo.Has(x, y) || !m_rConfirm.Has(x, y))
			ConfirmAnswer(false);
		return;
	}
	const int sel = m_iDrop;
	if (!m || sel < 0 || sel >= (int)m->selects.size())
	{
		CloseOverlay();
		return;
	}
	for (size_t o = 0; o < m_rDropOptions.size() && o < m->selects[sel].options.size(); o++)
	{
		if (m_rDropOptions[o].Has(x, y))
		{
			ChooseOption((int)o);
			return;
		}
	}
	CloseOverlay(); // a click outside the list: closed, nothing chosen
}

void CSboServerMenu::ClosePopup()
{
	m_iPopup = -1;
	if (m_bPopBarDrag)
		vgui2::input()->SetMouseCapture(0);
	m_bPopBarDrag = false;
	if (m_pPopup)
		m_pPopup->SetVisible(false);
}

void CSboServerMenu::LayoutPopup()
{
	const Menu *m = M();
	if (!m || m_iPopup < 0 || m_iPopup >= (int)m->items.size())
		return;
	const Item &it = m->items[m_iPopup];
	m_pPopup->SetBounds(0, 0, GetWide(), GetTall());
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	const int padX = P(22), padY = P(20), gap = P(12);
	const int w = std::min(sw - P(16), P(600)), innerW = w - 2 * padX;
	const int closeH = P(32);
	// the label in full: wrapped onto as many lines as it needs left of Close (and right of the avatar)
	const vgui2::HFont titleFont = Font(24.0f);
	const int titleW = std::max(P(40), innerW - closeH - P(12) - (it.avatar ? closeH + P(10) : 0));
	m_PopTitle = WrapText(titleFont, PopupHeader(it), titleW, 8);
	const int titleH = std::max(closeH, (int)m_PopTitle.size() * FontTall(titleFont));
	m_iPopTitleH = titleH;
	// a map's picture in its 16:9 shape; an item's own picture (ITEMPICTURE) in a box at most 200 tall
	const int picH = !it.map.empty() ? innerW * 9 / 16 : (!it.image.empty() ? std::min(innerW * 9 / 16, P(200)) : 0);
	const vgui2::HFont body = Font(13.0f);
	// the item's dropdowns (SELECT for this item): a row each above the buttons
	int selects = 0;
	for (const Select &d : M()->selects)
		selects += d.defined && d.item == m_iPopup;
	const int selRowH = P(32), selH = selects ? selects * (selRowH + gap) : 0;
	const int lineH = FontTall(body) + P(6);
	// the text wrapped left of its scroll bar; the area shows up to POPUP_SHOWN_LINES lines (what the screen leaves
	// room for) and scrolls through the rest
	const int textW = innerW - P(14);
	const std::wstring &text = PopupText(it);
	if (text != m_PopWrapText || textW != m_iPopWrapW)
	{
		m_PopWrapText = text;
		m_iPopWrapW = textW;
		m_PopLinks.clear();
		m_PopLines = WrapText(body, Linkify(text, m_PopLinks), textW, POPUP_TEXT_LINES);
	}
	const int fixedH = padY + titleH + gap + (picH ? picH + gap : 0) + (it.line.empty() ? 0 : FontTall(body) + gap) +
	                   selH + P(44) + padY;
	const int room = std::max(1, (sh - P(16) - fixedH - gap) / std::max(1, lineH));
	m_iPopShown = std::min((int)m_PopLines.size(), std::min(POPUP_SHOWN_LINES, room));
	m_iPopScroll = std::max(0, std::min(m_iPopScroll, (int)m_PopLines.size() - m_iPopShown));
	const int h = std::min(fixedH + (m_iPopShown ? m_iPopShown * lineH + gap : 0), sh - P(16));
	m_rPop = { (sw - w) / 2, (sh - h) / 2, w, h };
	const int textY = m_rPop.y + padY + titleH + gap + (picH ? picH + gap : 0) + (it.line.empty() ? 0 : FontTall(body) + gap);
	m_rPopText = m_iPopShown ? Rect{ m_rPop.x + padX, textY, innerW, m_iPopShown * lineH } : Rect();
	m_rPopClose = { m_rPop.x + w - padX - closeH, m_rPop.y + padY, closeH, closeH };
	m_rPopPicture = picH ? Rect{ m_rPop.x + padX, m_rPop.y + padY + titleH + gap, innerW, picH } : Rect();
	const vgui2::HFont buttonFont = Font(14.0f);
	const int by = m_rPop.y + h - padY - P(44);
	int sy = by - selH;
	for (int sl = 0; sl < MAX_MENU_SELECTS; sl++)
	{
		m_rPopSelects[sl] = m_rPopSelectBoxes[sl] = Rect();
		if (sl >= (int)M()->selects.size() || !M()->selects[sl].defined || M()->selects[sl].item != m_iPopup)
			continue;
		int widest = 0;
		for (const std::string &o : M()->selects[sl].options)
			widest = std::max(widest, TextWide(body, Text(o)));
		const int bw = std::min(innerW * 6 / 10, std::max(P(140), widest + P(40)));
		m_rPopSelects[sl] = { m_rPop.x + padX, sy, innerW, selRowH };
		m_rPopSelectBoxes[sl] = { m_rPop.x + padX + innerW - bw, sy, bw, selRowH };
		sy += selRowH + gap;
	}
	int bx = m_rPop.x + w - padX;
	m_rPopPrimary = Rect();
	if (!it.action.empty())
	{
		const int pw = TextWide(buttonFont, Text(it.action)) + 2 * P(24);
		bx -= pw;
		m_rPopPrimary = { bx, by, pw, P(44) };
		bx -= P(10);
	}
	const int backW = TextWide(buttonFont, L"Back") + 2 * P(20);
	m_rPopBack = { bx - backW, by, backW, P(44) };
}

void CSboServerMenu::PaintPopup()
{
	const Menu *m = M();
	if (!m || m_iPopup < 0 || m_iPopup >= (int)m->items.size())
		return;
	LayoutPopup(); // the item may have changed live (its action, its line)
	const Item &it = m->items[m_iPopup];
	const int padX = P(22), padY = P(20), gap = P(12);
	FillRect(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, Color(0, 0, 0, 140)); // the window darkened under it
	FillRect(m_rPop.x, m_rPop.y, m_rPop.w, m_rPop.h, COL_BG_WINDOW);
	OutlineRect(m_rPop.x, m_rPop.y, m_rPop.w, m_rPop.h, COL_BORDER_PRIMARY);
	const int innerX = m_rPop.x + padX, innerW = m_rPop.w - 2 * padX;
	const vgui2::HFont titleFont = Font(24.0f), body = Font(13.0f);
	int titleX = innerX;
	if (it.avatar)
	{
		// the item's player (AVATAR), as on its row
		const int as = m_rPopClose.h;
		DrawAvatar(it.avatar, titleX, m_rPopClose.y, as);
		titleX += as + P(10);
	}
	if (m_PopTitle.size() <= 1)
		DrawTextMid(titleFont, COL_TEXT_SELECTED, titleX, m_rPopClose.y, m_rPopClose.h,
			m_PopTitle.empty() ? std::wstring() : m_PopTitle[0]);
	else
	{
		// a long label: all of its lines, from the top
		for (size_t l = 0; l < m_PopTitle.size(); l++)
			DrawText(titleFont, COL_TEXT_SELECTED, titleX, m_rPop.y + padY + (int)l * FontTall(titleFont), m_PopTitle[l]);
	}
	if (m_rPopClose.Has(m_iMouseX, m_iMouseY))
		FillRect(m_rPopClose.x, m_rPopClose.y, m_rPopClose.w, m_rPopClose.h, COL_BG_SELECTED);
	OutlineRect(m_rPopClose.x, m_rPopClose.y, m_rPopClose.w, m_rPopClose.h, COL_BORDER);
	DrawX(m_rPopClose.x + m_rPopClose.w / 2, m_rPopClose.y + m_rPopClose.h / 2, P(12), COL_TEXT);
	int y = m_rPop.y + padY + m_iPopTitleH + gap;
	if (m_rPopPicture.w)
	{
		if (!it.map.empty())
			PaintMapPicture(it.map, m_rPopPicture, false, GetWide(), GetTall(), true);
		else
			PaintFilePicture(it.image, m_rPopPicture, false, GetWide(), GetTall());
		OutlineRect(m_rPopPicture.x - 1, m_rPopPicture.y - 1, m_rPopPicture.w + 2, m_rPopPicture.h + 2, COL_BORDER_LIST);
		y = m_rPopPicture.y + m_rPopPicture.h + gap;
	}
	if (!it.line.empty())
	{
		DrawText(body, COL_TEXT_DIM, innerX, y, Text(it.line), innerW);
		y += FontTall(body) + gap;
	}
	// the text from its first line shown; a scroll bar when it does not all fit
	const int lineH = FontTall(body) + P(6);
	m_PopLinkRects.clear();
	for (int l = 0; l < m_iPopShown && m_iPopScroll + l < (int)m_PopLines.size(); l++)
		PaintPopupLine(body, innerX, m_rPopText.y + l * lineH, m_PopLines[m_iPopScroll + l]);
	const Rect thumb = PopupThumb();
	if (thumb.w)
	{
		FillRect(thumb.x, m_rPopText.y, thumb.w, m_rPopText.h, COL_BG_INPUT);
		FillRect(thumb.x, thumb.y, thumb.w, thumb.h, m_bPopBarDrag || thumb.Has(m_iMouseX, m_iMouseY) ? COL_TEXT : COL_BORDER);
	}
	(void)y;
	for (int sl = 0; sl < MAX_MENU_SELECTS; sl++)
	{
		if (m_rPopSelects[sl].w)
			PaintSelect(sl, m_rPopSelects[sl], m_rPopSelectBoxes[sl],
				m_rPopSelectBoxes[sl].Has(m_iMouseX, m_iMouseY) || m_iDrop == sl);
	}
	PaintButton(m_rPopBack, L"Back", false, true, m_rPopBack.Has(m_iMouseX, m_iMouseY));
	if (m_rPopPrimary.w)
		PaintButton(m_rPopPrimary, Text(it.action), true, CanPick(m_iPopup), m_rPopPrimary.Has(m_iMouseX, m_iMouseY));
	if (m_iConfirmKind < 0 && m_iDrop < 0)
		PaintFocusRing(FocusRect(m_PopFocus));
}

void CSboServerMenu::PaintPopupLine(vgui2::HFont font, int x, int y, const std::wstring &line)
{
	size_t pos = 0;
	while (pos < line.size())
	{
		// the earliest link label in the rest of the line
		size_t at = std::wstring::npos, which = 0;
		for (size_t l = 0; l < m_PopLinks.size(); l++)
		{
			const size_t f = m_PopLinks[l].label.empty() ? std::wstring::npos : line.find(m_PopLinks[l].label, pos);
			if (f < at)
				at = f, which = l;
		}
		const std::wstring before = line.substr(pos, (at == std::wstring::npos ? line.size() : at) - pos);
		if (!before.empty())
			x += DrawText(font, COL_TEXT_LIGHT, x, y, before);
		if (at == std::wstring::npos)
			break;
		const std::wstring &label = m_PopLinks[which].label;
		const int w = TextWide(font, label), h = FontTall(font);
		const Rect r{ x, y, w, h };
		const bool hot = r.Has(m_iMouseX, m_iMouseY);
		DrawText(font, hot ? COL_TEXT_PRIMARY : COL_TEXT_BRIGHT, x, y, label);
		FillRect(x, y + h - 1, w, 1, hot ? COL_TEXT_PRIMARY : COL_BORDER_PRIMARY);
		m_PopLinkRects.push_back({ r, which });
		x += w;
		pos = at + label.size();
	}
}

void CSboServerMenu::PopupPressed(int x, int y, bool dbl)
{
	const int item = m_iPopup;
	if (item < 0)
		return;
	// the second press of the double click that opened it (outside the popup, on the card): the pick
	if (dbl && !m_rPop.Has(x, y) && ServerMenu_Now() - m_flPopupAt < 0.6f)
	{
		ClosePopup();
		Pick(item);
		return;
	}
	// a link in the text: opened as a chat link (the Steam overlay browser)
	for (const auto &link : m_PopLinkRects)
	{
		if (link.first.Has(x, y) && link.second < m_PopLinks.size())
		{
			HudChat_OpenURL(m_PopLinks[link.second].url.c_str());
			return;
		}
	}
	// the text's scroll bar: drag its thumb (a press on the track jumps there)
	const Rect thumb = PopupThumb();
	if (thumb.w && x >= thumb.x && x < thumb.x + thumb.w && y >= m_rPopText.y && y < m_rPopText.y + m_rPopText.h)
	{
		const int lines = (int)m_PopLines.size(), maxScroll = lines - m_iPopShown;
		if (y < thumb.y || y >= thumb.y + thumb.h)
			m_iPopScroll = std::max(0, std::min(maxScroll, (y - m_rPopText.y) * lines / std::max(1, m_rPopText.h) - m_iPopShown / 2));
		m_bPopBarDrag = true;
		m_iPopBarDragY = y;
		m_iPopBarDragScroll = m_iPopScroll;
		vgui2::input()->SetMouseCapture(m_pPopup->GetVPanel());
		return;
	}
	for (int sl = 0; sl < MAX_MENU_SELECTS; sl++)
	{
		if (m_rPopSelectBoxes[sl].Has(x, y))
		{
			OpenDrop(sl, m_rPopSelectBoxes[sl]);
			return;
		}
	}
	if (m_rPopPrimary.Has(x, y))
	{
		if (!CanPick(item))
			return;
		const std::string question = M()->items[item].confirm;
		// with a question the popup stays under the dialog (No goes back to it)
		if (!question.empty())
		{
			Pick(item);
			return;
		}
		ClosePopup();
		Pick(item);
		return;
	}
	// Back, Close or a click outside: the menu again
	if (m_rPopBack.Has(x, y) || m_rPopClose.Has(x, y))
	{
		ClosePopup();
		return;
	}
	if (!m_rPop.Has(x, y))
	{
		ClosePopup();
		// on the window's own controls (its buttons, toggles, sliders, fields, tabs, Close): that press acts at
		// once, with the item the popup was about still highlighted (an admin's Remove); in the list it only closes
		const Rect window = { 0, 0, GetWide(), GetTall() };
		if (window.Has(x, y) && !m_rArea.Has(x, y))
			OnMousePressed(vgui2::MOUSE_LEFT);
	}
}

Rect CSboServerMenu::PopupThumb() const
{
	const int lines = (int)m_PopLines.size();
	if (m_iPopShown <= 0 || lines <= m_iPopShown || m_rPopText.h <= 0)
		return Rect();
	const int bw = P(8), th = std::max(bw * 2, m_rPopText.h * m_iPopShown / lines);
	const int ty = m_rPopText.y + (m_rPopText.h - th) * m_iPopScroll / std::max(1, lines - m_iPopShown);
	return { m_rPopText.x + m_rPopText.w - bw, ty, bw, th };
}

void CSboServerMenu::PopupMoved(int x, int y)
{
	m_iMouseX = x;
	m_iMouseY = y;
	if (!m_bPopBarDrag)
		return;
	const int lines = (int)m_PopLines.size(), maxScroll = std::max(0, lines - m_iPopShown);
	const Rect thumb = PopupThumb();
	const int track = std::max(1, m_rPopText.h - thumb.h);
	m_iPopScroll = std::max(0, std::min(maxScroll, m_iPopBarDragScroll + (y - m_iPopBarDragY) * maxScroll / track));
}

void CSboServerMenu::PopupWheeled(int delta)
{
	const int maxScroll = std::max(0, (int)m_PopLines.size() - m_iPopShown);
	m_iPopScroll = std::max(0, std::min(maxScroll, m_iPopScroll - delta * 3));
}

void CSboServerMenu::PopupReleased()
{
	if (!m_bPopBarDrag)
		return;
	m_bPopBarDrag = false;
	vgui2::input()->SetMouseCapture(0);
}

void CSboServerMenu::AreaPressed(int x, int y, bool dbl)
{
	EndSearchTyping();
	// the scroll bar: drag its thumb (a press on the track jumps there)
	const int bw = P(8);
	if (m_iContent > m_rArea.h && x >= m_rArea.w - bw)
	{
		const int th = std::max(bw * 2, m_rArea.h * m_rArea.h / m_iContent);
		const int ty = (m_rArea.h - th) * m_iScroll / std::max(1, m_iContent - m_rArea.h);
		if (y < ty || y >= ty + th)
			m_iScroll = std::max(0, std::min(m_iContent - m_rArea.h,
				(y - th / 2) * (m_iContent - m_rArea.h) / std::max(1, m_rArea.h - th)));
		m_bBarDrag = true;
		m_iBarDragY = y;
		m_iBarDragScroll = m_iScroll;
		vgui2::input()->SetMouseCapture(m_pArea->GetVPanel());
		return;
	}
	for (size_t n = 0; n < m_Visible.size() && n < m_Cards.size(); n++)
	{
		Rect r = m_Cards[n];
		r.y -= m_iScroll;
		if (!r.Has(x, y))
			continue;
		Highlight(m_Visible[n], true);
		UpdatePreview();
		if (dbl || Stars())
			Pick(m_Visible[n]); // a star: one click rates
		else if (Popups())
			OpenPopup(m_Visible[n]);
		return;
	}
}

void CSboServerMenu::AreaReleased()
{
	if (!m_bBarDrag)
		return;
	m_bBarDrag = false;
	vgui2::input()->SetMouseCapture(0);
}

void CSboServerMenu::AreaDragged(int y)
{
	const int th = std::max(P(8) * 2, m_rArea.h * m_rArea.h / std::max(1, m_iContent));
	m_iScroll = m_iBarDragScroll + (y - m_iBarDragY) * (m_iContent - m_rArea.h) / std::max(1, m_rArea.h - th);
	m_iScroll = std::max(0, std::min(m_iScroll, m_iContent - m_rArea.h));
}

void CSboServerMenu::AreaMoved(int x, int y)
{
	if (m_bBarDrag)
	{
		AreaDragged(y);
		return;
	}
	m_iHover = -1;
	m_iAreaMouseX = x;
	m_iAreaMouseY = y;
	if (x < 0)
		return;
	for (size_t n = 0; n < m_Visible.size() && n < m_Cards.size(); n++)
	{
		Rect r = m_Cards[n];
		r.y -= m_iScroll;
		if (r.Has(x, y))
			m_iHover = m_Visible[n];
	}
}

void CSboServerMenu::AreaWheeled(int delta)
{
	const int step = m_Cards.empty() ? P(40) : m_Cards[0].h + P(10);
	m_iScroll = std::max(0, std::min(m_iScroll - delta * step, m_iContent - m_rArea.h));
}

void CSboServerMenu::OnMousePressed(vgui2::MouseCode code)
{
	m_bKeyNav = false;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	if (m_rSearch.Has(x, y))
	{
		BeginSearchTyping();
		return;
	}
	EndSearchTyping();
	// turning the model: a drag in the preview area (left or right button)
	if ((code == vgui2::MOUSE_LEFT || code == vgui2::MOUSE_RIGHT) && HasPreview() && m_rPreview.Has(x, y))
	{
		m_bDragging = true;
		m_iDragX = x;
		m_flDragYaw = m_flYaw;
		vgui2::input()->SetMouseCapture(GetVPanel());
		return;
	}
	if (code != vgui2::MOUSE_LEFT)
		return;
	const Menu *m = M();
	if (!m)
		return;
	if (m_rClose.Has(x, y))
	{
		ServerMenu_PlayerClosed();
		return;
	}
	for (size_t t = 0; t < m_rTabs.size(); t++)
	{
		if (m_rTabs[t].Has(x, y))
		{
			SetTab((int)t);
			UpdatePreview();
			return;
		}
	}
	for (int t = 0; t < MAX_MENU_TOGGLES && t < (int)m->toggles.size(); t++)
	{
		if (m_rToggles[t].Has(x, y))
		{
			FlipToggle(t);
			return;
		}
	}
	for (int sl = 0; sl < MAX_MENU_SLIDERS && sl < (int)m->sliders.size(); sl++)
	{
		if (m_rSliderTracks[sl].Has(x, y))
		{
			// dragged on the client; the server hears the value on the release
			m_iSliderDrag = sl;
			m_iSliderValue = SliderValueAt(sl, x);
			vgui2::input()->SetMouseCapture(GetVPanel());
			return;
		}
	}
	for (int sl = 0; sl < MAX_MENU_SELECTS; sl++)
	{
		if (m_rSelectBoxes[sl].Has(x, y))
		{
			OpenDrop(sl, m_rSelectBoxes[sl]);
			return;
		}
	}
	for (int f = 0; f < MAX_MENU_FIELDS; f++)
	{
		if (m_rFieldButtons[f].Has(x, y))
		{
			SubmitField(f);
			return;
		}
	}
	for (size_t b = 0; b < m_rButtons.size(); b++)
	{
		if (m_rButtons[b].Has(x, y))
		{
			Button((int)b);
			return;
		}
	}
	if (m_rBack.Has(x, y))
	{
		Back();
		return;
	}
	if (PrimaryShown() && m_rPrimary.Has(x, y))
		Pick(m_iHighlight);
}

int CSboServerMenu::SliderShown(int slider) const
{
	const Menu *m = M();
	if (!m || slider < 0 || slider >= (int)m->sliders.size())
		return 0;
	const Slider &d = m->sliders[slider];
	return slider < MAX_MENU_SLIDERS && SentShows(m_SliderSent[slider], d.updates) ? m_SliderSent[slider].value : d.value;
}

bool CSboServerMenu::ToggleShown(int toggle) const
{
	const Menu *m = M();
	if (!m || toggle < 0 || toggle >= (int)m->toggles.size())
		return false;
	const Toggle &t = m->toggles[toggle];
	return toggle < MAX_MENU_TOGGLES && SentShows(m_ToggleSent[toggle], t.updates) ? m_ToggleSent[toggle].value != 0 : t.on;
}

int CSboServerMenu::SliderValueAt(int slider, int x) const
{
	const Menu *m = M();
	if (!m || slider < 0 || slider >= (int)m->sliders.size())
		return 0;
	const Slider &d = m->sliders[slider];
	const Rect &tr = m_rSliderTracks[slider];
	const float frac = tr.w > 0 ? std::max(0.0f, std::min(1.0f, (x - tr.x) / (float)tr.w)) : 0.0f;
	return d.Clamp(d.min + (int)lroundf(frac * (d.max - d.min)));
}

void CSboServerMenu::OnMouseReleased(vgui2::MouseCode code)
{
	if (m_iSliderDrag >= 0)
	{
		const int slider = m_iSliderDrag;
		m_iSliderDrag = -1;
		vgui2::input()->SetMouseCapture(0);
		const Menu *m = M();
		if (m && slider < (int)m->sliders.size() && m_iSliderValue != SliderShown(slider))
			SendSlider(slider, m_iSliderValue);
		return;
	}
	if (m_bDragging)
	{
		m_bDragging = false;
		vgui2::input()->SetMouseCapture(0);
	}
}

void CSboServerMenu::OnCursorMoved(int x, int y)
{
	m_iMouseX = x;
	m_iMouseY = y;
	if (m_iSliderDrag >= 0)
		m_iSliderValue = SliderValueAt(m_iSliderDrag, x);
	if (m_bDragging)
	{
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		// a drag across the area's width turns it once around
		m_flYaw = m_flDragYaw + (x - m_iDragX) * 360.0f / std::max(1, m_rPreview.w);
		m_flYaw = fmodf(m_flYaw, 360.0f);
	}
}

void CSboServerMenu::FlipToggle(int t)
{
	const Menu *m = M();
	if (!m || t < 0 || t >= (int)m->toggles.size())
		return;
	// flipped here at once; the server answers with the state it kept (a refused flip goes back)
	const bool want = !ToggleShown(t);
	m_ToggleSent[t] = { true, want ? 1 : 0, m->toggles[t].updates, ServerMenu_Now() };
	char what[32];
	snprintf(what, sizeof(what), "toggle %d %d", t, want ? 1 : 0);
	ServerMenu_Send(what);
}

void CSboServerMenu::SendSlider(int sl, int value)
{
	const Menu *m = M();
	if (!m || sl < 0 || sl >= (int)m->sliders.size())
		return;
	value = m->sliders[sl].Clamp(value);
	m_SliderSent[sl] = { true, value, m->sliders[sl].updates, ServerMenu_Now() };
	char what[32];
	snprintf(what, sizeof(what), "slider %d %d", sl, value);
	ServerMenu_Send(what);
}

void CSboServerMenu::ChooseOption(int option)
{
	const Menu *m = M();
	const int sel = m_iDrop;
	CloseOverlay();
	if (!m || sel < 0 || sel >= (int)m->selects.size() || option < 0 || option >= (int)m->selects[sel].options.size() ||
	    option == SelectShown(sel))
		return;
	// shown here at once; the server answers with the choice it kept (a refused one goes back)
	m_SelectSent[sel] = { true, option, m->selects[sel].updates, ServerMenu_Now() };
	char what[32];
	snprintf(what, sizeof(what), "select %d %d", sel, option);
	ServerMenu_Send(what);
}

//! What Tab and the arrows go through, in the window's order (the popup: its dropdowns, Back, its action)
std::vector<CSboServerMenu::Focus> CSboServerMenu::FocusList(bool popup) const
{
	std::vector<Focus> list;
	const Menu *m = M();
	if (!m)
		return list;
	if (popup)
	{
		for (int sl = 0; sl < MAX_MENU_SELECTS; sl++)
			if (m_rPopSelects[sl].w)
				list.push_back({ F_POP_SELECT, sl });
		list.push_back({ F_POP_BACK, 0 });
		if (m_rPopPrimary.w)
			list.push_back({ F_POP_PRIMARY, 0 });
		return list;
	}
	if (m_rTabs.size() > 1)
		list.push_back({ F_TABS, 0 });
	if (m_rSearch.w)
		list.push_back({ F_SEARCH, 0 });
	for (int f = 0; f < MAX_MENU_FIELDS; f++)
		if (m_rFields[f].w)
			list.push_back({ F_FIELD, f });
	for (int sl = 0; sl < MAX_MENU_SLIDERS; sl++)
		if (m_rSliders[sl].w)
			list.push_back({ F_SLIDER, sl });
	for (int t = 0; t < MAX_MENU_TOGGLES; t++)
		if (m_rToggles[t].w)
			list.push_back({ F_TOGGLE, t });
	for (int sl = 0; sl < MAX_MENU_SELECTS; sl++)
		if (m_rSelects[sl].w)
			list.push_back({ F_SELECT, sl });
	list.push_back({ F_ITEMS, 0 });
	for (size_t b = 0; b < m_rButtons.size(); b++)
		list.push_back({ F_BUTTON, (int)b });
	if (m_rBack.w)
		list.push_back({ F_BACK, 0 });
	if (PrimaryShown())
		list.push_back({ F_PRIMARY, 0 });
	return list;
}

Rect CSboServerMenu::FocusRect(const Focus &f) const
{
	switch (f.kind)
	{
	case F_TABS: return m_iTab < (int)m_rTabs.size() ? m_rTabs[m_iTab] : Rect();
	case F_SEARCH: return m_rSearch;
	case F_FIELD: return m_rFields[f.index];
	case F_SLIDER: return m_rSliders[f.index];
	case F_TOGGLE: return m_rToggles[f.index];
	case F_SELECT: return m_rSelectBoxes[f.index];
	case F_BUTTON: return f.index < (int)m_rButtons.size() ? m_rButtons[f.index] : Rect();
	case F_BACK: return m_rBack;
	case F_PRIMARY: return m_rPrimary;
	case F_POP_SELECT: return m_rPopSelectBoxes[f.index];
	case F_POP_BACK: return m_rPopBack;
	case F_POP_PRIMARY: return m_rPopPrimary;
	default: return Rect();
	}
}

void CSboServerMenu::PaintFocusRing(const Rect &r)
{
	if (!m_bKeyNav || r.w <= 0 || r.h <= 0)
		return;
	const int t = std::max(1, P(2));
	for (int i = 0; i < t; i++)
		OutlineRect(r.x - t + i - 1, r.y - t + i - 1, r.w + 2 * (t - i) + 2, r.h + 2 * (t - i) + 2, COL_TEXT_PRIMARY);
}

void CSboServerMenu::MoveFocus(bool popup, int step)
{
	const std::vector<Focus> list = FocusList(popup);
	if (list.empty())
		return;
	Focus &cur = popup ? m_PopFocus : m_Focus;
	size_t at = 0;
	while (at < list.size() && !(list[at] == cur))
		at++;
	if (at >= list.size())
		at = step > 0 ? list.size() - 1 : 0; // gone (a control the menu lost): from the end it stood at
	cur = list[(at + list.size() + (size_t)(step % (int)list.size() + (int)list.size())) % list.size()];
	m_bKeyNav = true;
}

void CSboServerMenu::MoveHighlight(int dx, int dy)
{
	if (m_Visible.empty())
		return;
	size_t n = std::find(m_Visible.begin(), m_Visible.end(), m_iHighlight) - m_Visible.begin();
	if (n >= m_Visible.size())
		n = 0;
	const int cols = std::max(1, M() && M()->layout != LAYOUT_LIST ? m_iColumns : 1);
	const int to = std::max(0, std::min((int)m_Visible.size() - 1, (int)n + dx + dy * cols));
	Highlight(m_Visible[to], true);
	UpdatePreview();
	m_pArea->Repaint();
}

void CSboServerMenu::ActivateFocus(bool popup, int key)
{
	const Menu *m = M();
	if (!m)
		return;
	const Focus f = popup ? m_PopFocus : m_Focus;
	switch (f.kind)
	{
	case F_ITEMS:
		if (m_iHighlight < 0)
			return;
		if (Popups())
			OpenPopup(m_iHighlight);
		else
			Pick(m_iHighlight);
		return;
	case F_SEARCH:
		BeginSearchTyping();
		return;
	case F_FIELD:
		BeginSearchTyping(m_pFields[f.index]);
		return;
	case F_TOGGLE:
		FlipToggle(f.index);
		return;
	case F_SELECT:
		OpenDrop(f.index, m_rSelectBoxes[f.index]);
		return;
	case F_POP_SELECT:
		OpenDrop(f.index, m_rPopSelectBoxes[f.index]);
		return;
	case F_BUTTON:
		Button(f.index);
		return;
	case F_BACK:
		Back();
		return;
	case F_PRIMARY:
		Pick(m_iHighlight);
		return;
	case F_POP_BACK:
		ClosePopup();
		return;
	case F_POP_PRIMARY:
		if (m_iPopup >= 0 && CanPick(m_iPopup))
		{
			const int item = m_iPopup;
			if (m->items[item].confirm.empty())
				ClosePopup();
			Pick(item);
		}
		return;
	default:
		return;
	}
}

//! Tab and Shift+Tab go through the controls and the items, the arrows move in the items (a grid: by rows and
//! columns), between the tabs, a slider's value or to the next control; Enter picks or presses, Space flips or
//! presses; Esc closes a dropdown, the dialog or the popup first, else the window
bool CSboServerMenu::KeyInput(int key)
{
	const Menu *m = M();
	if (!m)
		return false;
	if (m_iConfirmKind >= 0)
	{
		switch (key)
		{
		case SMKEY_ESCAPE: ConfirmAnswer(false); return true;
		case SMKEY_ENTER:
		case SMKEY_SPACE: ConfirmAnswer(!m_bConfirmNoFocus); return true;
		case SMKEY_TAB: case SMKEY_BACKTAB: case SMKEY_LEFT: case SMKEY_RIGHT: case SMKEY_UP: case SMKEY_DOWN:
			m_bConfirmNoFocus = !m_bConfirmNoFocus;
			m_bKeyNav = true;
			return true;
		}
		return false;
	}
	if (m_iDrop >= 0)
	{
		const int n = m_iDrop < (int)m->selects.size() ? (int)m->selects[m_iDrop].options.size() : 0;
		switch (key)
		{
		case SMKEY_ESCAPE: case SMKEY_TAB: case SMKEY_BACKTAB: CloseOverlay(); return true;
		case SMKEY_UP: case SMKEY_LEFT: m_iDropHover = std::max(0, m_iDropHover - 1); return true;
		case SMKEY_DOWN: case SMKEY_RIGHT: m_iDropHover = std::min(n - 1, m_iDropHover + 1); return true;
		case SMKEY_ENTER: case SMKEY_SPACE: ChooseOption(m_iDropHover); return true;
		}
		return false;
	}
	if (m_iPopup >= 0)
	{
		switch (key)
		{
		case SMKEY_ESCAPE: ClosePopup(); return true;
		case SMKEY_TAB: case SMKEY_RIGHT: MoveFocus(true, 1); return true;
		case SMKEY_BACKTAB: case SMKEY_LEFT: MoveFocus(true, -1); return true;
		case SMKEY_UP: case SMKEY_DOWN:
			// the text scrolls when it does not all fit, else the focus moves
			if ((int)m_PopLines.size() > m_iPopShown)
				PopupWheeled(key == SMKEY_UP ? 1 : -1);
			else
				MoveFocus(true, key == SMKEY_UP ? -1 : 1);
			m_bKeyNav = true;
			return true;
		case SMKEY_ENTER: case SMKEY_SPACE: m_bKeyNav = true; ActivateFocus(true, key); return true;
		}
		return false;
	}
	switch (key)
	{
	case SMKEY_ESCAPE:
		ServerMenu_PlayerClosed(); // as the Close button (the server hears "servermenu close")
		return true;
	case SMKEY_TAB: MoveFocus(false, 1); return true;
	case SMKEY_BACKTAB: MoveFocus(false, -1); return true;
	case SMKEY_ENTER: case SMKEY_SPACE:
		m_bKeyNav = true;
		ActivateFocus(false, key);
		return true;
	}
	// the arrows
	m_bKeyNav = true;
	const int dx = key == SMKEY_LEFT ? -1 : (key == SMKEY_RIGHT ? 1 : 0);
	const int dy = key == SMKEY_UP ? -1 : (key == SMKEY_DOWN ? 1 : 0);
	switch (m_Focus.kind)
	{
	case F_ITEMS:
		if (m->layout == LAYOUT_STARS)
		{
			// one row: left and right move along the stars, up and down to the other controls
			if (dx)
				MoveHighlight(dx, 0);
			else
				MoveFocus(false, dy);
			return true;
		}
		if (m->layout == LAYOUT_GRID || dy)
			MoveHighlight(m->layout == LAYOUT_GRID ? dx : 0, dy);
		else
			MoveFocus(false, dx);
		return true;
	case F_TABS:
		if (dx && !m->tabs.empty())
		{
			SetTab(std::max(0, std::min((int)m->tabs.size() - 1, m_iTab + dx)));
			UpdatePreview();
		}
		else
			MoveFocus(false, dy);
		return true;
	case F_SLIDER:
		if (dx && m_Focus.index < (int)m->sliders.size())
		{
			const Slider &sl = m->sliders[m_Focus.index];
			const int big = std::max(sl.step, (sl.max - sl.min) / 20 / std::max(1, sl.step) * sl.step);
			SendSlider(m_Focus.index, SliderShown(m_Focus.index) + dx * big);
		}
		else
			MoveFocus(false, dy);
		return true;
	default:
		MoveFocus(false, dx + dy);
		return true;
	}
}

//! Only while the search box is typed in (the window has the keyboard then): Esc gives the keys back
void CSboServerMenu::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == vgui2::KEY_ESCAPE)
	{
		EndSearchTyping();
		return;
	}
	BaseClass::OnKeyCodeTyped(code);
}

void CSboServerMenu::BeginSearchTyping(vgui2::Panel *pEntry)
{
	vgui2::Panel *p = pEntry ? pEntry : m_pSearch;
	if (!p->IsVisible())
		return;
	// as the loadout menu's name box: the window, then the entry, takes the keyboard
	SetKeyBoardInputEnabled(true);
	p->SetKeyBoardInputEnabled(true);
	MoveToFront();
	p->RequestFocus();
}

void CSboServerMenu::EndSearchTyping()
{
	// the keys are the player's again (chat, console, weapon slots)
	SetKeyBoardInputEnabled(false);
}

void CSboServerMenu::OnTextNewLine(vgui2::Panel *panel)
{
	for (int f = 0; f < MAX_MENU_FIELDS; f++)
	{
		if (panel == m_pFields[f])
		{
			SubmitField(f);
			return;
		}
	}
	EndSearchTyping();
}

void CSboServerMenu::OnTextChanged(vgui2::Panel *panel)
{
	if (panel != m_pSearch)
		return; // a text field: sent on Enter or its button
	char text[128];
	m_pSearch->GetText(text, sizeof(text));
	m_Search = Widen(text);
	LayoutArea();
	if (std::find(m_Visible.begin(), m_Visible.end(), m_iHighlight) == m_Visible.end())
		Highlight(m_Visible.empty() ? -1 : m_Visible[0], true);
	UpdatePreview();
}

//! 1-9 (the HUD's slot keys, through the menu stack's entry): the n-th item shown
bool CSboServerMenu::SlotInput(int iSlot)
{
	if (!IsVisible() || iSlot < 0 || iSlot > 8 || iSlot >= (int)m_Visible.size())
		return false;
	ClosePopup();
	Highlight(m_Visible[iSlot], true);
	UpdatePreview();
	Pick(m_Visible[iSlot]);
	return true;
}

//-----------------------------------------------------------------------------
// The HUD strip (HUDBAR),
// drawn for a 1280 x 800 screen and scaled: top center, 460 wide, an equalizer sign, the title, the sub line, the
// progress bar with "played / length" (counted here: nothing is sent while it runs), and the up-next line attached
// under it in the same panel (one background and border that grow by that line, and shrink back as it fades over
// its last second when the server gave it a time). Not drawn under the HUD's rules
// (ServerMenu_HudBarShouldDraw: hud_draw, the scoreboard, menus).
//-----------------------------------------------------------------------------
namespace
{
std::wstring Clock(int seconds)
{
	wchar_t t[24];
	seconds = std::max(0, seconds);
	if (seconds >= 3600)
		swprintf(t, sizeof(t) / sizeof(t[0]), L"%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
	else
		swprintf(t, sizeof(t) / sizeof(t[0]), L"%d:%02d", seconds / 60, seconds % 60);
	return t;
}

Color Alpha(Color c, float a)
{
	return Color(c.r(), c.g(), c.b(), (int)(c.a() * std::max(0.0f, std::min(1.0f, a))));
}
} // namespace

class CSboHudBar : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboHudBar, vgui2::Panel);

public:
	static CSboHudBar *Get()
	{
		if (!s_pInstance && g_pViewport)
			s_pInstance = new CSboHudBar(g_pViewport);
		return s_pInstance;
	}
	void OnThink() override
	{
		// the screen's size (the parent's); nothing to click
		int sw, sh;
		vgui2::surface()->GetScreenSize(sw, sh);
		SetBounds(0, 0, sw, sh);
	}
	void PaintBackground() override {}
	void Paint() override;

private:
	CSboHudBar(vgui2::Panel *pParent) : BaseClass(pParent, "SboHudBar")
	{
		SetKeyBoardInputEnabled(false);
		SetMouseInputEnabled(false);
		SetPaintBackgroundEnabled(false);
		SetVisible(true); // always: VGUI2 does not think invisible panels; nothing is painted without a strip
	}
	static CSboHudBar *s_pInstance;
};

CSboHudBar *CSboHudBar::s_pInstance = nullptr;

void CSboHudBar::Paint()
{
	const HudBar &h = ServerMenu_Store().Hud();
	if (!h.shown || !ServerMenu_HudBarShouldDraw())
		return;
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	const float k = std::min(sw / (float)MOCK_SCREEN_WIDE, sh / (float)MOCK_SCREEN_TALL);
	auto P = [k](float v) { return (int)lroundf(v * k); };
	auto F = [k](float px, int weight) {
		const float em = std::max(px * k, std::min(px, 12.0f));
		return MakeFontFace(SboUiFontFace(), std::max(6, (int)lroundf(em * 1.2f)), weight);
	};
	const float now = ServerMenu_Now();
	const vgui2::HFont titleFont = F(14.0f, 400), small = F(11.0f, 400), nextFont = F(12.0f, 400);

	// the strip: top center, or centered left of a docked server menu (it stays up with one)
	CSboServerMenu *pMenu = CSboServerMenu::Get();
	const int docked = pMenu ? pMenu->DockedLeft() : -1;
	const int room = docked > 0 ? docked : sw;
	const int w = std::min(room - P(16), P(460)), x = (room - w) / 2, padX = P(12), padY = P(8);
	if (w <= P(120))
		return;
	const bool bar = h.length > 0.0f;
	const int titleH = FontTall(titleFont), smallH = FontTall(small);
	const int innerH = titleH + (bar ? P(4) + P(3) : 0) + (h.sub.empty() ? 0 : P(4) + smallH);
	const int stripH = innerH + 2 * padY;
	// a Game Master's status strip stays clear: the bar goes under it
	Rect strip{ x, std::max(P(18), VGui2_GmStripBottom()), w, stripH };
	// the up-next line: attached under the title area, the panel taller by it; it shrinks away as it fades
	const bool next = h.nextShown && (h.nextUntil <= 0.0f || now < h.nextUntil);
	const float nextAlpha = !next ? 0.0f : (h.nextUntil > 0.0f ? std::min(1.0f, h.nextUntil - now) : 1.0f);
	const int nextFull = FontTall(nextFont) + 2 * P(6);
	const int nextH = next ? (int)lroundf(nextFull * nextAlpha) : 0;
	FillRect(strip.x, strip.y, strip.w, strip.h + nextH, C_HINT_BG);
	OutlineRect(strip.x, strip.y, strip.w, strip.h + nextH, COL_BORDER);
	// the equalizer sign (the design's 18 px icon: five bars)
	const int icon = P(18), ix = strip.x + padX, iy = strip.y + (strip.h - icon) / 2;
	const float bars[5][2] = { { 9, 15 }, { 6, 18 }, { 3, 21 }, { 7, 17 }, { 10, 14 } };
	if (h.avatar)
		DrawAvatar(h.avatar, ix, iy, icon); // a player's avatar (HUDBAR) in place of the sign
	else
	{
		for (int i = 0; i < 5; i++)
			FillRect(ix + P(1 + i * 4), iy + P(bars[i][0] * 0.75f), std::max(1, P(2)),
				std::max(1, P((bars[i][1] - bars[i][0]) * 0.75f)), COL_TEXT);
	}
	// played / length at the right
	int right = strip.x + strip.w - padX;
	const float played = std::max(0.0f, std::min(bar ? h.length : 1e9f, now - h.start));
	if (bar)
	{
		const std::wstring t = Clock((int)played) + L" / " + Clock((int)h.length);
		const int tw = TextWide(small, t);
		DrawTextMid(small, COL_TEXT_LIGHT, right - tw, strip.y, strip.h, t);
		right -= tw + P(12);
	}
	const int tx = ix + icon + P(12), tw = std::max(1, right - tx);
	int y = strip.y + padY;
	DrawText(titleFont, COL_TEXT_PRIMARY, tx, y, Text(h.title), tw);
	y += titleH;
	if (bar)
	{
		y += P(4);
		const int bh = std::max(2, P(3));
		FillRect(tx, y, tw, bh, COL_BG_SELECTED);
		FillRect(tx, y, (int)(tw * (played / h.length)), bh, COL_TEXT);
		y += P(3);
	}
	if (!h.sub.empty())
		DrawText(small, COL_TEXT_DIM, tx, y + P(4), Text(h.sub), tw);

	// up next: the panel's last line, under a divider, fading over the last second of its time
	if (nextH > 0)
	{
		const float a = nextAlpha;
		Rect box{ x, strip.y + strip.h, w, nextH };
		FillRect(box.x + 1, box.y, box.w - 2, 1, Alpha(COL_BORDER_LIST, a));
		if (box.h < FontTall(nextFont))
			return; // shrinking: the text is gone before the line is
		// the next track under its "Up next" label, or a notice (HUDBAR_NOTICE) on its own in the warm tint of the
		// design's message line; text too long for the line ends in "..." (DrawTextMid)
		int nx = box.x + padX;
		if (h.notice)
			FillRect(box.x + 1, box.y + 1, box.w - 2, box.h - 2, Alpha(Color(58, 47, 18, 140), a));
		else
		{
			const std::wstring label = L"Up next";
			DrawTextMid(nextFont, Alpha(COL_TEXT_DIM, a), box.x + padX, box.y, box.h, label);
			nx += TextWide(nextFont, label) + P(12);
		}
		if (h.nextAvatar && a > 0.5f)
		{
			// the next track's requester, a round bubble before its title
			const int bs = FontTall(nextFont), by = box.y + (box.h - bs) / 2;
			FillRect(nx, by, bs, bs, C_BUBBLE);
			DrawAvatar(h.nextAvatar, nx, by, bs);
			RoundCorners(nx, by, bs, Color(12, 15, 12, 255));
			nx += bs + P(8);
		}
		DrawTextMid(nextFont, Alpha(h.notice ? Color(230, 217, 176, 255) : COL_TEXT_LIGHT, a), nx, box.y, box.h,
			Text(h.next), std::max(1, box.x + box.w - padX - nx));
	}
}

//-----------------------------------------------------------------------------
// sbo_server_menu.h
//-----------------------------------------------------------------------------
void VGui2_ServerMenu_Show(int menu, int tab, int item)
{
	if (CSboServerMenu *p = CSboServerMenu::Get())
		p->Show(menu, tab, item);
}

void VGui2_ServerMenu_Hide()
{
	if (CSboServerMenu *p = CSboServerMenu::Get())
		p->Hide();
}

bool VGui2_ServerMenu_Enabled()
{
	return SboNewIngameMenus();
}

void VGui2_ServerMenu_Changed(int menu)
{
	if (CSboServerMenu *p = CSboServerMenu::Get())
		p->Changed(menu);
}

void VGui2_ServerMenu_Frame()
{
	static int s_iLast = -1;
	const int on = SboNewIngameMenus() ? 1 : 0;
	if (s_iLast >= 0 && on != s_iLast)
		ServerMenu_EnabledChanged();
	s_iLast = on;
}

void VGui2_HudBar_Changed()
{
	CSboHudBar::Get();
}

bool VGui2_ServerMenu_IsOpen()
{
	return CSboServerMenu::Shown();
}

bool VGui2_ServerMenu_KeyInput(int key)
{
	CSboServerMenu *p = CSboServerMenu::Get();
	return p && p->IsVisible() && p->KeyInput(key);
}

bool VGui2_ServerMenu_SlotInput(int iSlot)
{
	CSboServerMenu *p = CSboServerMenu::Get();
	return p && p->SlotInput(iSlot);
}
