//========= SBO VGUI2: Game Master mode spawn window =========//
//
// Purpose: see sbo_gm_spawn_menu.h.
//=================================================================//

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <algorithm>

#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <KeyValues.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>

#include "client_vgui.h"
#include "client_viewport.h"
#include "sbo_bridge.h"
#include "sbo_gm_menu.h"
#include "sbo_gm_spawn_menu.h"
#include "sbo_gm_ui.h"
#include "sbo_menus.h"
#include "sbo_ui_style.h"
#include "vgui2_api.h"
#include "../server_menu_preview.h"

#include "../../game_shared/gm_catalog.h"

using namespace gmui;

namespace
{
// GmMode message state (game_shared/gm_catalog.h)
int s_iGmState = GM_STATE_OFF;
int s_iTeleInfo = 0; // GM_TP_* (pairs, entrance pending)
CSboGmFade *s_pFade = nullptr;
bool s_bPlacing = false;
int s_iPlaceEntry = -1; // the pick being placed (its price on the placement line)
int s_iPlaceVariant = -1;
int s_iPickVariant = -1; // the loadout last picked in the window
std::wstring s_PlaceLabel;

// The texts a plugin set for the cards ("GmEntry", game_shared/gm_catalog.h): per pick and slot
struct GmCardText
{
	std::wstring text;
	float end = 0;   // countdown end (HUD time), 0 = none
	float total = 0; // the countdown's length when it came (the clock's full turn)
};
GmCardText s_Cards[GM_CATALOG_COUNT][GM_PICK_MAX_VARIANT + 1][GM_TEXT_SLOTS];

void ClearCards()
{
	for (auto &entry : s_Cards)
		for (auto &variant : entry)
			for (auto &slot : variant)
				slot = GmCardText();
}

const GmCardText *Card(int entry, int variant, int slot)
{
	if (entry < 0 || entry >= GM_CATALOG_COUNT || variant < -1 || variant >= GM_PICK_MAX_VARIANT || slot < 0
	    || slot >= GM_TEXT_SLOTS)
		return nullptr;
	const float now = sbo::GetHudTime();
	const GmCardText *c = &s_Cards[entry][variant + 1][slot];
	if (c->text.empty() || (c->end > 0 && now >= c->end))
		c = &s_Cards[entry][0][slot];
	if (c->text.empty() || (c->end > 0 && now >= c->end))
		return nullptr;
	return c;
}

// The catalog's Teleporters tool
int TeleportersEntry()
{
	for (int i = 0; i < GM_CATALOG_COUNT; i++)
	{
		if (g_GmCatalog[i].iKind == GM_KIND_TOOL && g_GmCatalog[i].iTool == GM_TOOL_TELEPORTERS)
			return i;
	}
	return -1;
}

bool ContainsNoCase(const std::wstring &text, const std::wstring &part)
{
	if (part.empty())
		return true;
	for (size_t i = 0; i + part.size() <= text.size(); i++)
	{
		size_t j = 0;
		while (j < part.size() && towlower(text[i + j]) == towlower(part[j]))
			j++;
		if (j == part.size())
			return true;
	}
	return false;
}

// The engine's key name (Key_LookupBinding: "k", "f1") of a VGUI2 key
const char *KeyName(vgui2::KeyCode code)
{
	static char name[8];
	if (code >= vgui2::KEY_A && code <= vgui2::KEY_Z)
	{
		name[0] = (char)('a' + (code - vgui2::KEY_A));
		name[1] = 0;
		return name;
	}
	if (code >= vgui2::KEY_0 && code <= vgui2::KEY_9)
	{
		name[0] = (char)('0' + (code - vgui2::KEY_0));
		name[1] = 0;
		return name;
	}
	if (code >= vgui2::KEY_F1 && code <= vgui2::KEY_F12)
	{
		snprintf(name, sizeof(name), "f%d", 1 + (code - vgui2::KEY_F1));
		return name;
	}
	return nullptr;
}

const wchar_t *TabName(int tab)
{
	static const wchar_t *const s_Names[GM_CAT_COUNT] = { L"Tier I", L"Tier II", L"Tier III", L"Support", L"Teleporters" };
	return (tab >= 0 && tab < GM_CAT_COUNT) ? s_Names[tab] : L"";
}

// classic menus (Options > Advanced > "New in-game menus" off): the server's text menus instead of the windows
bool ClassicMenus()
{
	static sbo::CvarHandle s_Cvar = nullptr;
	if (!s_Cvar)
		s_Cvar = sbo::FindCvar("cl_new_ingame_menus");
	return s_Cvar && sbo::CvarGetFloat(s_Cvar) == 0.0f;
}

// a card's model: the monster's (its loadout's class), a support item's body of gmmode.mdl, the teleporter pad
bool CardModel(int type, int entry, int variant, ServerMenuPreviewLayer &l)
{
	memset(&l, 0, sizeof(l));
	if (type != 0 /* CARD_ENTRY */)
	{
		V_strncpy(l.path, GM_DISPLAY_MODEL_PATH, sizeof(l.path));
		l.body = 23;
		l.skin = 2;
		return true;
	}
	if (entry < 0 || entry >= GM_CATALOG_COUNT)
		return false;
	const GmCatalogEntry &e = g_GmCatalog[entry];
	if (e.iKind == GM_KIND_WEAPON)
	{
		V_strncpy(l.path, GM_DISPLAY_MODEL_PATH, sizeof(l.path));
		l.body = e.iBody;
		return true;
	}
	const char *cls = e.pszClass;
	int count = 0, weapons = -1;
	const GmVariant *v = GmCatalog_Variants(e.iVariants, count);
	if (v && variant >= 0 && variant < count)
	{
		cls = v[variant].pszClass;
		weapons = v[variant].iWeapons;
	}
	const char *model = GmCatalog_ModelForClass(cls);
	if (!model)
		return false;
	V_strncpy(l.path, model, sizeof(l.path));
	// the loadout's body and skin, as the placement preview in the world (gmmode.cpp GmSelectAppearance)
	int body, skin;
	GmCatalog_LoadoutLook(cls, weapons, body, skin);
	l.body = body >= 0 ? body : 0;
	l.skin = skin >= 0 ? skin : 0;
	return true;
}

ServerMenuPreviewRect Frac(int x, int y, int w, int h)
{
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	const float fw = (float)std::max(1, sw), fh = (float)std::max(1, sh);
	return { x / fw, y / fh, (x + w) / fw, (y + h) / fh };
}
} // namespace

//-----------------------------------------------------------------------------
// gmui: the state the HUD and the windows share
//-----------------------------------------------------------------------------
namespace gmui
{
int GetState()
{
	return s_iGmState;
}

bool IsPlacing()
{
	return s_bPlacing && s_iGmState == GM_STATE_ACTIVE;
}

const std::wstring &PlaceLabel()
{
	return s_PlaceLabel;
}

int PlaceEntry()
{
	return s_iPlaceEntry;
}

int PlaceVariant()
{
	return s_iPlaceVariant;
}

std::wstring CardText(int entry, int variant, int slot)
{
	const GmCardText *c = Card(entry, variant, slot);
	if (!c)
		return std::wstring();
	std::wstring line = c->text;
	const size_t token = line.find(L"" GM_TEXT_TOKEN);
	if (token != std::wstring::npos)
		line.replace(token, wcslen(L"" GM_TEXT_TOKEN), std::to_wstring(SecondsLeft(c->end)));
	return line;
}

int CardSeconds(int entry, int variant)
{
	const GmCardText *c = Card(entry, variant, GM_TEXT_STATUS);
	return c ? SecondsLeft(c->end) : 0;
}

float CardCountdown(int entry, int variant, float &total)
{
	total = 0;
	const GmCardText *c = Card(entry, variant, GM_TEXT_STATUS);
	if (!c || c->end <= 0)
		return 0;
	const float left = c->end - sbo::GetHudTime();
	if (left <= 0)
		return 0;
	total = std::max(c->total, left);
	return left;
}
} // namespace gmui

//-----------------------------------------------------------------------------
// CSboGmTextEntry
//-----------------------------------------------------------------------------
CSboGmTextEntry::CSboGmTextEntry(CSboGmSpawnMenu *pMenu, const char *pName)
    : BaseClass(pMenu, pName)
    , m_pMenu(pMenu)
{
	SetMaximumCharCount(32);
	SetCatchEnterKey(true);
	SetAllowNonAsciiCharacters(true);
	SetPaintBorderEnabled(false);
}

void CSboGmTextEntry::ApplySchemeSettings(vgui2::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetFont(Font(13));
	SetFgColor(GREEN);
	SetBgColor(INPUT_BG);
	SetSelectionBgColor(SEL_BORDER);
	SetBorder(nullptr);
}

void CSboGmTextEntry::PaintBackground()
{
	// TextEntry draws its background color and its text here; the design's 1px border on top
	BaseClass::PaintBackground();
	int w, h;
	GetSize(w, h);
	Outline(0, 0, w, h, BORDER);
}

void CSboGmTextEntry::Paint()
{
	BaseClass::Paint();
	if (m_Placeholder.empty() || GetTextLength() > 0)
		return;
	int w, h;
	GetSize(w, h);
	vgui2::HFont f = Font(13);
	Text(f, S(8), (h - FontTall(f)) / 2, FAINT, m_Placeholder.c_str());
}

void CSboGmTextEntry::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (m_pMenu && m_pMenu->HandleKey(code))
		return;
	BaseClass::OnKeyCodeTyped(code);
}

//-----------------------------------------------------------------------------
// CSboGmSpawnMenu
//-----------------------------------------------------------------------------
CSboGmSpawnMenu *CSboGmSpawnMenu::s_pInstance = nullptr;

CSboGmSpawnMenu *CSboGmSpawnMenu::Create(vgui2::Panel *pParent)
{
	if (!s_pInstance)
		s_pInstance = new CSboGmSpawnMenu(pParent);
	return s_pInstance;
}

CSboGmSpawnMenu::CSboGmSpawnMenu(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboGmSpawnMenu")
{
	MakePopup(false, false);
	if (sboui::GetScheme())
		SetScheme(sboui::GetScheme());
	SetProportional(false);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetPaintBackgroundEnabled(true);
	SetVisible(false);

	m_pSearch = new CSboGmTextEntry(this, "Search");
	m_pSearch->SetPlaceholder(L"Name");
	m_pSearch->AddActionSignalTarget(this);

	for (int &v : m_iVariant)
		v = 0;
	vgui2::ivgui()->AddTickSignal(GetVPanel());
}

void CSboGmSpawnMenu::StateChanged()
{
	if (m_bOpen && m_iTab == GM_CAT_TELEPORTERS)
		Rebuild();
}

void CSboGmSpawnMenu::Open(int entry)
{
	if (s_iGmState != GM_STATE_ACTIVE)
		return;

	// classic menus: the server's text menus show the same lists
	if (ClassicMenus())
	{
		int tab = (entry >= GM_CATALOG_COUNT && entry < GM_CATALOG_COUNT + GM_CAT_COUNT) ? entry - GM_CATALOG_COUNT
		    : (entry >= 0 && entry < GM_CATALOG_COUNT) ? g_GmCatalog[entry].iCategory : -1;
		char cmd[64];
		snprintf(cmd, sizeof(cmd), tab >= 0 ? "cmd gm_textmenu spawn %d" : "cmd gm_textmenu spawn", tab);
		sbo::ExecClientCmd(cmd);
		return;
	}
	if (m_bOpen)
		return;

	// opening the window ends a placement (the plugin: /gmspawn cleared the preview)
	if (s_bPlacing)
		sbo::ExecClientCmd("cmd gm_cancel");
	GmUi_ClosePlayers();

	m_bOpen = true;
	m_bConfirmClear = m_bConfirmRemove = false;
	m_iScrollRow = 0;
	m_pSearch->SetText(L"");

	int select = -1;
	if (entry >= GM_CATALOG_COUNT && entry < GM_CATALOG_COUNT + GM_CAT_COUNT)
		m_iTab = entry - GM_CATALOG_COUNT;
	else if (entry >= 0 && entry < GM_CATALOG_COUNT)
	{
		m_iTab = g_GmCatalog[entry].iCategory;
		select = entry;
	}

	Rebuild();
	m_iSelected = 0;
	for (size_t i = 0; i < m_Cards.size(); i++)
		if (select >= 0 && m_Cards[i].type == CARD_ENTRY && m_Cards[i].entry == select)
			m_iSelected = (int)i;

	int wide, tall;
	vgui2::surface()->GetScreenSize(wide, tall);
	SetBounds(0, 0, wide, tall);
	Layout();
	SetVisible(true);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(true);
	m_pSearch->SetMouseInputEnabled(true);
	m_pSearch->SetKeyBoardInputEnabled(true);
	MoveToFront();
	m_pSearch->RequestFocus();

	if (g_pGameUIViewport)
		g_pGameUIViewport->PreventEscapeToShow(true);

	sbo::ReleaseMouseForCursor();
	vgui2::input()->SetCursorPos(wide / 2, tall / 2);
	vgui2::surface()->CalculateMouseVisible();
}

void CSboGmSpawnMenu::Close()
{
	if (!m_bOpen)
		return;

	m_bOpen = false;
	m_bDragging = false;
	if (g_pGameUIViewport)
		g_pGameUIViewport->PreventEscapeToShow(false);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetVisible(false);
	ServerMenuPreviewSpec off;
	memset(&off, 0, sizeof(off));
	ServerMenuPreview_Set(off);
	ServerMenuPreview_SetCards(nullptr, 0);
	vgui2::surface()->CalculateMouseVisible();
}

// the board's layout: a 1000 x 620 window, padding 20 / 24, gap 14 (design px, scaled with the screen height)
void CSboGmSpawnMenu::Layout()
{
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	m_rWin = { (sw - S(1000)) / 2, (sh - S(620)) / 2, S(1000), S(620) };
	const int px = m_rWin.x + S(24), py = m_rWin.y + S(20), innerW = m_rWin.w - S(48);

	m_rClose = { px + innerW - S(32), py, S(32), S(32) };

	const int tabsY = py + S(32) + S(14);
	vgui2::HFont f13 = Font(13);
	int x = px;
	for (int i = 0; i < GM_CAT_COUNT; i++)
	{
		const int w = TextWide(f13, TabName(i)) + S(28);
		m_rTabs[i] = { x, tabsY, w, S(34) };
		x += w + S(6);
	}
	m_rSearch = { px + innerW - S(200), tabsY, S(200), S(34) };
	m_pSearch->SetBounds(m_rSearch.x, m_rSearch.y, m_rSearch.w, m_rSearch.h);

	const int bodyY = tabsY + S(34) + S(14);
	const int bodyH = m_rWin.y + m_rWin.h - S(20) - bodyY;
	m_rPanel = { px + innerW - S(267), bodyY, S(267), bodyH };
	m_rGrid = { px, bodyY, innerW - S(267) - S(16), bodyH };

	const int cx = m_rPanel.x + S(17), cw = S(250);
	m_rPreview = { cx, bodyY, cw, S(150) };
	m_rPlace = { cx, bodyY + bodyH - S(44), cw, S(44) };
}

void CSboGmSpawnMenu::Rebuild()
{
	m_Cards.clear();
	wchar_t search[64] = L"";
	m_pSearch->GetText(search, sizeof(search));
	const std::wstring part = search;

	// the search finds in every tab; else the tab's cards
	for (int i = 0; i < GM_CATALOG_COUNT; i++)
	{
		const GmCatalogEntry &e = g_GmCatalog[i];
		if (e.iKind == GM_KIND_TOOL)
			continue;
		if (part.empty() ? e.iCategory != m_iTab : !ContainsNoCase(Widen(e.pszName), part))
			continue;
		Card c;
		c.type = CARD_ENTRY;
		c.entry = i;
		c.name = Widen(e.pszName);
		m_Cards.push_back(c);
	}

	if (m_iTab == GM_CAT_TELEPORTERS || !part.empty())
	{
		std::vector<Card> tp;
		Card place;
		place.type = CARD_TP_PLACE;
		place.entry = TeleportersEntry();
		place.name = (s_iTeleInfo & GM_TP_ENTRANCE_PENDING) ? L"Place Teleporter Exit" : L"Place Teleporter";
		tp.push_back(place);
		for (int p = 1; p <= (s_iTeleInfo & GM_TP_PAIRS_MASK); p++)
		{
			Card pair;
			pair.type = CARD_TP_PAIR;
			pair.pair = p;
			wchar_t buf[48];
			swprintf(buf, 48, L"Teleporter Pair %d", p);
			pair.name = buf;
			tp.push_back(pair);
		}
		Card clear;
		clear.type = CARD_TP_CLEAR;
		clear.name = L"Clear All Teleporters";
		tp.push_back(clear);
		for (const Card &c : tp)
			if (part.empty() || ContainsNoCase(c.name, part))
				m_Cards.push_back(c);
	}

	if (m_iSelected >= (int)m_Cards.size())
		m_iSelected = m_Cards.empty() ? 0 : (int)m_Cards.size() - 1;
}

int CSboGmSpawnMenu::SelectedCost() const
{
	if (!HasEconomy() || m_iSelected < 0 || m_iSelected >= (int)m_Cards.size())
		return 0;
	const Card &card = m_Cards[m_iSelected];
	if (card.type != CARD_ENTRY && card.type != CARD_TP_PLACE)
		return 0;
	const int v = card.type == CARD_TP_PLACE ? ((s_iTeleInfo & GM_TP_ENTRANCE_PENDING) ? GM_TP_ROLE_EXIT : GM_TP_ROLE_ENTRANCE)
	                                         : Variant();
	const std::wstring price = CardText(card.entry, v, GM_TEXT_PRICE);
	return price.empty() ? 0 : std::max(0, (int)wcstol(price.c_str(), nullptr, 10));
}

bool CSboGmSpawnMenu::CanAfford() const
{
	// false when the pick would be refused: more than the points, or the plugin's reason on the card (a locked tier,
	// a cooldown, a limit)
	if (!HasEconomy() || m_iSelected < 0 || m_iSelected >= (int)m_Cards.size())
		return true;
	const Card &card = m_Cards[m_iSelected];
	if (card.type == CARD_ENTRY || card.type == CARD_TP_PLACE)
	{
		const int v = card.type == CARD_TP_PLACE ? ((s_iTeleInfo & GM_TP_ENTRANCE_PENDING) ? GM_TP_ROLE_EXIT : GM_TP_ROLE_ENTRANCE)
		                                         : Variant();
		if (!CardText(card.entry, v, GM_TEXT_TIER).empty() || !CardText(card.entry, v, GM_TEXT_STATUS).empty())
			return false;
	}
	return GetStatus().maxPoints <= 0 || SelectedCost() <= GetStatus().points;
}

int CSboGmSpawnMenu::Variant() const
{
	if (m_iSelected < 0 || m_iSelected >= (int)m_Cards.size() || m_Cards[m_iSelected].type != CARD_ENTRY)
		return -1;
	const int entry = m_Cards[m_iSelected].entry;
	int count = 0;
	if (!GmCatalog_Variants(g_GmCatalog[entry].iVariants, count) || count <= 0)
		return -1;
	return std::min(std::max(0, m_iVariant[entry & 63]), count - 1);
}

void CSboGmSpawnMenu::Place()
{
	if (m_iSelected < 0 || m_iSelected >= (int)m_Cards.size())
		return;
	const Card &c = m_Cards[m_iSelected];
	char cmd[96];
	switch (c.type)
	{
	case CARD_ENTRY:
		s_iPickVariant = Variant();
		snprintf(cmd, sizeof(cmd), "cmd gm_pick %d %d", c.entry, s_iPickVariant);
		break;
	case CARD_TP_PLACE:
		snprintf(cmd, sizeof(cmd), "cmd gm_tp place");
		break;
	default:
		return;
	}
	Close();
	sbo::ExecClientCmd(cmd);
}

void CSboGmSpawnMenu::SelectCard(int index)
{
	if (index < 0 || index >= (int)m_Cards.size())
		return;
	if (index != m_iSelected)
		m_bConfirmClear = m_bConfirmRemove = false;
	m_iSelected = index;
}

void CSboGmSpawnMenu::MoveSelection(int dx, int dy)
{
	const int count = (int)m_Cards.size();
	if (count <= 0)
		return;
	SelectCard(std::min(std::max(0, m_iSelected + dx + dy * 4), count - 1));
	// keep it in view
	const int row = m_iSelected / 4;
	if (row < m_iScrollRow)
		m_iScrollRow = row;
	const int visibleRows = std::max(1, (int)m_rCards.size() / 4);
	if (row >= m_iScrollRow + visibleRows)
		m_iScrollRow = row - visibleRows + 1;
}

bool CSboGmSpawnMenu::IsCloseKey(vgui2::KeyCode code)
{
	const char *keyName = KeyName(code);
	if (!keyName)
		return false;
	// a letter or digit closes only while the search box is empty and has no focus for typing
	if ((code >= vgui2::KEY_A && code <= vgui2::KEY_Z) || (code >= vgui2::KEY_0 && code <= vgui2::KEY_9))
	{
		wchar_t search[8] = L"";
		m_pSearch->GetText(search, sizeof(search));
		if (search[0])
			return false;
	}
	static const char *const commands[] = { "gmon", "gmmenu", "gm_menu", "gm_spawnmenu" };
	for (const char *command : commands)
	{
		const char *bound = sbo::LookupKeyBinding(command);
		if (bound && !_stricmp(bound, keyName))
			return true;
	}
	return false;
}

bool CSboGmSpawnMenu::HandleKey(vgui2::KeyCode code)
{
	if (!m_bOpen)
		return false;

	switch (code)
	{
	case vgui2::KEY_ESCAPE:
		Close();
		return true;
	case vgui2::KEY_ENTER:
	case vgui2::KEY_PAD_ENTER:
		Place();
		return true;
	case vgui2::KEY_UP: MoveSelection(0, -1); return true;
	case vgui2::KEY_DOWN: MoveSelection(0, 1); return true;
	case vgui2::KEY_LEFT: MoveSelection(-1, 0); return true;
	case vgui2::KEY_RIGHT: MoveSelection(1, 0); return true;
	case vgui2::KEY_PAGEUP: MoveSelection(0, -3); return true;
	case vgui2::KEY_PAGEDOWN: MoveSelection(0, 3); return true;
	case vgui2::KEY_TAB:
	{
		const bool back = vgui2::input()->IsKeyDown(vgui2::KEY_LSHIFT) || vgui2::input()->IsKeyDown(vgui2::KEY_RSHIFT);
		m_pSearch->SetText(L"");
		m_iTab = (m_iTab + (back ? GM_CAT_COUNT - 1 : 1)) % GM_CAT_COUNT;
		m_iSelected = 0;
		m_iScrollRow = 0;
		Rebuild();
		return true;
	}
	default:
		break;
	}

	if (IsCloseKey(code))
	{
		Close();
		return true;
	}
	return false;
}

void CSboGmSpawnMenu::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (HandleKey(code))
		return;
	BaseClass::OnKeyCodeTyped(code);
}

void CSboGmSpawnMenu::OnSearchChanged()
{
	m_iSelected = 0;
	m_iScrollRow = 0;
	Rebuild();
}

void CSboGmSpawnMenu::OnMouseWheeled(int delta)
{
	const int rows = ((int)m_Cards.size() + 3) / 4;
	const int visibleRows = std::max(1, (int)m_rCards.size() / 4);
	m_iScrollRow = std::min(std::max(0, m_iScrollRow - delta), std::max(0, rows - visibleRows));
}

void CSboGmSpawnMenu::OnCursorMoved(int x, int y)
{
	if (!m_bDragging)
		return;
	int sx, sy;
	vgui2::input()->GetCursorPos(sx, sy);
	m_flYaw += (sx - m_iDragX) * 0.6f;
	m_iDragX = sx;
}

void CSboGmSpawnMenu::OnMouseReleased(vgui2::MouseCode code)
{
	m_bDragging = false;
}

void CSboGmSpawnMenu::OnMousePressed(vgui2::MouseCode code)
{
	if (code != vgui2::MOUSE_LEFT)
		return;
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);

	if (m_rClose.Hit(x, y))
	{
		Close();
		return;
	}
	for (int i = 0; i < GM_CAT_COUNT; i++)
	{
		if (m_rTabs[i].Hit(x, y))
		{
			m_pSearch->SetText(L"");
			m_iTab = i;
			m_iSelected = 0;
			m_iScrollRow = 0;
			m_bConfirmClear = m_bConfirmRemove = false;
			Rebuild();
			return;
		}
	}
	for (size_t i = 0; i < m_rCards.size(); i++)
	{
		if (m_rCards[i].Hit(x, y))
		{
			SelectCard(m_iScrollRow * 4 + (int)i);
			return;
		}
	}
	if (m_rPreview.Hit(x, y))
	{
		m_bDragging = true;
		vgui2::input()->GetCursorPos(m_iDragX, y);
		return;
	}
	for (size_t i = 0; i < m_rVariants.size(); i++)
	{
		if (m_rVariants[i].Hit(x, y) && m_iSelected < (int)m_Cards.size())
		{
			m_iVariant[m_Cards[m_iSelected].entry & 63] = (int)i;
			return;
		}
	}
	if (m_iSelected < 0 || m_iSelected >= (int)m_Cards.size())
		return;
	const Card &c = m_Cards[m_iSelected];
	if ((c.type == CARD_ENTRY || c.type == CARD_TP_PLACE) && m_rPlace.Hit(x, y))
	{
		if (CanAfford())
			Place();
		return;
	}
	char cmd[64];
	if (c.type == CARD_TP_PAIR)
	{
		if (m_bConfirmRemove)
		{
			if (m_rActions[0].Hit(x, y))
			{
				snprintf(cmd, sizeof(cmd), "cmd gm_tp remove %d", c.pair);
				sbo::ExecClientCmd(cmd);
				m_bConfirmRemove = false;
			}
			else if (m_rActions[1].Hit(x, y))
				m_bConfirmRemove = false;
			return;
		}
		for (int i = 0; i < 3; i++)
		{
			if (!m_rActions[i].Hit(x, y))
				continue;
			if (i == 2)
			{
				m_bConfirmRemove = true;
				return;
			}
			snprintf(cmd, sizeof(cmd), "cmd gm_tp goto %d %d", c.pair, i);
			Close();
			sbo::ExecClientCmd(cmd);
			return;
		}
	}
	if (c.type == CARD_TP_CLEAR)
	{
		if (m_bConfirmClear)
		{
			if (m_rActions[0].Hit(x, y))
			{
				sbo::ExecClientCmd("cmd gm_tp clear");
				m_bConfirmClear = false;
			}
			else if (m_rActions[1].Hit(x, y))
				m_bConfirmClear = false;
		}
		else if (m_rActions[0].Hit(x, y))
			m_bConfirmClear = true;
	}
}

void CSboGmSpawnMenu::OnMouseDoublePressed(vgui2::MouseCode code)
{
	OnMousePressed(code);
	int x, y;
	vgui2::input()->GetCursorPos(x, y);
	ScreenToLocal(x, y);
	for (size_t i = 0; i < m_rCards.size(); i++)
	{
		if (m_rCards[i].Hit(x, y))
		{
			Place();
			return;
		}
	}
}

void CSboGmSpawnMenu::OnThink()
{
	if (!m_bOpen)
		return;
	if (s_iGmState != GM_STATE_ACTIVE)
	{
		Close();
		return;
	}
	int sw, sh;
	vgui2::surface()->GetScreenSize(sw, sh);
	int w, h;
	GetSize(w, h);
	if (w != sw || h != sh)
		SetBounds(0, 0, sw, sh);
	Layout();
	// the visible cards: rows that fit the grid
	vgui2::HFont f14 = Font(14), f11 = Font(11);
	const int cardTall = 2 + S(8) * 2 + S(70) + S(6) * 2 + FontTall(f14) + FontTall(f11);
	const int rows = std::max(1, (m_rGrid.h + S(10)) / (cardTall + S(10)));
	const int cardW = (m_rGrid.w - 3 * S(10)) / 4;
	m_rCards.clear();
	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < 4; c++)
		{
			const int index = (m_iScrollRow + r) * 4 + c;
			if (index >= (int)m_Cards.size())
				break;
			m_rCards.push_back({ m_rGrid.x + c * (cardW + S(10)), m_rGrid.y + r * (cardTall + S(10)), cardW, cardTall });
		}
	}
	UpdatePreviews();
	Repaint();
}

// the cards' small models and the right panel's large one, drawn by the 3D view (server_menu_preview)
void CSboGmSpawnMenu::UpdatePreviews()
{
	ServerMenuPreviewSpec spec;
	memset(&spec, 0, sizeof(spec));
	if (!ServerMenuPreview_CanClip())
	{
		ServerMenuPreview_Set(spec);
		ServerMenuPreview_SetCards(nullptr, 0);
		return;
	}
	const float turn = fmodf((float)vgui2::system()->GetCurrentTime() * 40.0f, 360.0f);
	ServerMenuCardSpec cards[SERVER_MENU_MAX_CARD_MODELS];
	int n = 0;
	int count = 0;
	for (size_t i = 0; i < m_rCards.size() && n < SERVER_MENU_MAX_CARD_MODELS; i++)
	{
		const Card &card = m_Cards[m_iScrollRow * 4 + i];
		ServerMenuCardSpec &c = cards[n];
		memset(&c, 0, sizeof(c));
		const int cardVariant = card.type == CARD_ENTRY && GmCatalog_Variants(g_GmCatalog[card.entry].iVariants, count)
		    ? std::min(std::max(0, m_iVariant[card.entry & 63]), count - 1) : -1;
		if (!CardModel(card.type, card.entry, card.type == CARD_ENTRY ? cardVariant : 0, c.model))
			continue;
		const Rect &r = m_rCards[i];
		c.frame = Frac(r.x + 1 + S(8), r.y + 1 + S(8), r.w - 2 - S(16), S(70));
		c.clip = c.frame;
		c.yaw = turn;
		c.playerModel = true;
		n++;
	}
	ServerMenuPreview_SetCards(cards, n);

	if (m_iSelected >= 0 && m_iSelected < (int)m_Cards.size())
	{
		const Card &card = m_Cards[m_iSelected];
		if (CardModel(card.type, card.entry, Variant(), spec.base))
		{
			spec.active = true;
			spec.frame = Frac(m_rPreview.x, m_rPreview.y, m_rPreview.w, m_rPreview.h);
			spec.backer[0] = PREVIEW_BG.r();
			spec.backer[1] = PREVIEW_BG.g();
			spec.backer[2] = PREVIEW_BG.b();
			spec.yaw = turn + m_flYaw; // turning as the cards do; a drag turns it further
		}
	}
	ServerMenuPreview_Set(spec);
}

void CSboGmSpawnMenu::Button(const Rect &r, const wchar_t *label, bool primary, float px, bool enabled)
{
	// greyed out (not enough points): no fill, the locked border, faded text
	if (primary && enabled)
		Fill(r.x, r.y, r.w, r.h, BUTTON_BG);
	Outline(r.x, r.y, r.w, r.h, !enabled ? BORDER_LOCKED : (primary ? SEL_BORDER : BORDER));
	vgui2::HFont f = Font(px);
	Color c = primary ? BRIGHT : GREEN;
	if (!enabled)
		c[3] = 110;
	Text(f, r.x + (r.w - TextWide(f, label)) / 2, r.y + (r.h - FontTall(f)) / 2, c, label);
}

void CSboGmSpawnMenu::PaintBackground()
{
	if (!m_bOpen)
		return;
	Fill(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, BG);
	Outline(m_rWin.x, m_rWin.y, m_rWin.w, m_rWin.h, BORDER);
}

void CSboGmSpawnMenu::Paint()
{
	if (!m_bOpen)
		return;

	// header: "Spawn" 26px; "[points] pts", "Tier [n] - next tier in [s]s", X
	vgui2::HFont f26 = Font(26), f13 = Font(13), f16 = Font(16);
	const int px = m_rWin.x + S(24), py = m_rWin.y + S(20);
	Text(f26, px, py + (S(32) - FontTall(f26)) / 2, GREEN, L"Spawn");
	CloseButton(m_rClose.x, m_rClose.y, m_rClose.w, m_rClose.h);
	if (HasEconomy())
	{
		const Status &st = GetStatus();
		wchar_t tier[96], pts[32];
		const int next = SecondsLeft(st.tierEnd);
		if (st.tier > 0 && next > 0)
			swprintf(tier, 96, L"%ls \u00b7 next tier in %ds", TabName(st.tier - 1), next);
		else if (st.tier > 0)
			swprintf(tier, 96, L"%ls", TabName(st.tier - 1));
		else
			tier[0] = 0;
		const int ty = m_rClose.y + (m_rClose.h - FontTall(f13)) / 2;
		int x = m_rClose.x - S(16) - TextWide(f13, tier);
		Text(f13, x, ty, DIM, tier);
		if (st.maxPoints > 0)
		{
			// the design: a 240 x 12 bar with the selected card's cost as an amber chunk ("-15" over it), "96 / 140",
			// "-> 81"
			const int cost = SelectedCost();
			// after the cost: "-> 81", or "· need 104 more" in red when it is more than the points
			wchar_t after[48] = L"";
			if (cost > st.points)
				swprintf(after, 48, L"\u00b7 need %d more", cost - st.points);
			else if (cost > 0)
				swprintf(after, 48, L"\u2192 %d", st.points - cost);
			swprintf(pts, 32, L"%d / %d", st.points, st.maxPoints);
			if (after[0])
			{
				x -= S(16) + TextWide(f13, after);
				Text(f13, x, ty, cost > st.points ? BAR_RED : DIM, after); // red: more than there is
				x += S(16) - S(10);
			}
			x -= S(16) + TextWide(f13, pts);
			Text(f13, x, ty, BRIGHT, pts);
			const int bw = S(240), bh = S(12), bx = x - S(10) - bw, by = m_rClose.y + (m_rClose.h - bh) / 2;
			const int end = EnergyBar(bx, by, bw, bh, cost);
			if (cost > 0)
			{
				vgui2::HFont f11 = Font(11);
				wchar_t minus[16];
				swprintf(minus, 16, L"\u2212%d", cost);
				const int chunkMid = cost <= st.points ? end - (int)((float)(bw - 2) * cost / std::max(1, st.maxPoints) / 2)
				                                       : end + (int)((float)(bw - 2) * (cost - st.points) / std::max(1, st.maxPoints) / 2);
				Text(f11, chunkMid - TextWide(f11, minus) / 2, by - S(3) - FontTall(f11), cost <= st.points ? ACCENT : BAR_RED, minus);
			}
		}
		else
		{
			swprintf(pts, 32, L"%d pts", st.points);
			x -= S(16) + TextWide(f13, pts);
			Text(f13, x, ty, BRIGHT, pts);
		}
	}

	// tabs and "Search"
	wchar_t search[8] = L"";
	m_pSearch->GetText(search, sizeof(search));
	for (int i = 0; i < GM_CAT_COUNT; i++)
		Button(m_rTabs[i], TabName(i), i == m_iTab && !search[0], 13);
	Text(f13, m_rSearch.x - S(8) - TextWide(f13, L"Search"), m_rSearch.y + (m_rSearch.h - FontTall(f13)) / 2, DIM,
	    L"Search");

	for (size_t i = 0; i < m_rCards.size(); i++)
		PaintCard(m_iScrollRow * 4 + (int)i, m_rCards[i]);

	PaintPanel();
}

// a card: preview box (70), name (14px), cost (11px) left, tag right
void CSboGmSpawnMenu::PaintCard(int index, const Rect &r)
{
	const Card &card = m_Cards[index];
	const bool sel = index == m_iSelected;
	std::wstring cost, tag;
	Color tagColor = GREEN_LIGHT;
	bool locked = false;
	float clockLeft = 0, clockTotal = 0;
	if (card.type == CARD_ENTRY || card.type == CARD_TP_PLACE)
	{
		const int entry = card.entry;
		const int variant = card.type == CARD_TP_PLACE
		    ? ((s_iTeleInfo & GM_TP_ENTRANCE_PENDING) ? GM_TP_ROLE_EXIT : GM_TP_ROLE_ENTRANCE) : -1;
		const std::wstring price = CardText(entry, variant, GM_TEXT_PRICE);
		if (!price.empty())
			cost = price + L" pts";
		const std::wstring tier = CardText(entry, variant, GM_TEXT_TIER);
		const std::wstring status = CardText(entry, variant, GM_TEXT_STATUS);
		// a countdown (a cooldown, or the time to the tier's unlock): the server menus' cooldown clock on the card
		clockLeft = CardCountdown(entry, variant, clockTotal);
		if (!tier.empty())
		{
			const int n = _wtoi(tier.c_str());
			tag = (n >= 1 && n <= 3) ? TabName(n - 1) : L"Tier " + tier;
			tagColor = ACCENT;
			locked = true;
		}
		else if (clockLeft > 0)
		{
			tag = status; // the reason ("Cooldown 12s"); the clock over the picture shows the time running out
			tagColor = ACCENT;
		}
		else if (!status.empty())
		{
			tag = status;
			tagColor = ACCENT;
		}
		else
		{
			// never "Ready" when the click would be refused: more than the points
			const Status &st = GetStatus();
			const int need = price.empty() ? 0 : (int)wcstol(price.c_str(), nullptr, 10) - st.points;
			if (HasEconomy() && st.maxPoints > 0 && need > 0)
			{
				tag = L"Need " + std::to_wstring(need);
				tagColor = BAR_RED;
			}
			else
				tag = L"Ready";
		}
	}

	const float alpha = locked ? 0.55f : 1.0f;
	auto A = [alpha](Color c) {
		c[3] = (unsigned char)(c[3] * alpha);
		return c;
	};
	if (sel)
		Fill(r.x, r.y, r.w, r.h, SEL_BG);
	Outline(r.x, r.y, r.w, r.h, sel ? SEL_BORDER : (locked ? BORDER_LOCKED : BORDER_DIM));

	const Rect pv = { r.x + 1 + S(8), r.y + 1 + S(8), r.w - 2 - S(16), S(70) };
	Fill(pv.x, pv.y, pv.w, pv.h, A(PREVIEW_BG));
	// the model's frame, copied from the 3D view (its slot: the visible card's index)
	int slot = 0;
	for (int i = m_iScrollRow * 4; i < index; i++)
		slot++;
	if (ServerMenuPreview_CanClip())
		ServerMenuPreview_PaintCopy(slot, Frac(pv.x, pv.y, pv.w, pv.h));

	vgui2::HFont f14 = Font(14), f11 = Font(11);
	int y = pv.y + pv.h + S(6);
	Text(f14, pv.x, y, A(locked ? DIM : BRIGHT), card.name.c_str());
	y += FontTall(f14) + S(6);
	if (!cost.empty())
		Text(f11, pv.x, y, A(VALUE), cost.c_str());
	if (!tag.empty())
		Text(f11, pv.x + pv.w - TextWide(f11, tag.c_str()), y, A(tagColor), tag.c_str());

	// waiting: greyed, the clock over its preview box, draining to the end; at zero the card is ready again
	if (clockLeft > 0)
	{
		Fill(pv.x, pv.y, pv.w, pv.h, Color(8, 10, 8, 150)); // the picture greyed; the name, cost and reason stay readable
		const int radius = std::max(S(8), std::min(pv.w, pv.h) * 36 / 100);
		sbolook::DrawCooldownClock(pv.x + pv.w / 2, pv.y + pv.h / 2, radius, clockLeft, clockTotal, Font(13, true));
	}
}

// the right panel: preview 150, name 18px, "Tier - cost - cooldown", "Kills pay you", the button, the hint
void CSboGmSpawnMenu::PaintPanel()
{
	Fill(m_rPanel.x, m_rPanel.y, 1, m_rPanel.h, BORDER_DIM);
	m_rVariants.clear();
	for (Rect &r : m_rActions)
		r = Rect();
	if (m_iSelected < 0 || m_iSelected >= (int)m_Cards.size())
		return;
	const Card &card = m_Cards[m_iSelected];

	Fill(m_rPreview.x, m_rPreview.y, m_rPreview.w, m_rPreview.h, PREVIEW_BG);
	if (ServerMenuPreview_CanClip())
		ServerMenuPreview_PaintCopy(SERVER_MENU_MAX_CARD_MODELS, Frac(m_rPreview.x, m_rPreview.y, m_rPreview.w, m_rPreview.h));
	if (card.type == CARD_ENTRY || card.type == CARD_TP_PLACE)
	{
		float total = 0;
		const int v = card.type == CARD_TP_PLACE
		    ? ((s_iTeleInfo & GM_TP_ENTRANCE_PENDING) ? GM_TP_ROLE_EXIT : GM_TP_ROLE_ENTRANCE) : Variant();
		const float left = CardCountdown(card.entry, v, total);
		if (left > 0)
			sbolook::DrawCooldownClock(m_rPreview.x + m_rPreview.w / 2, m_rPreview.y + m_rPreview.h / 2,
			    std::min(m_rPreview.w, m_rPreview.h) * 30 / 100, left, total, Font(17, true));
	}

	vgui2::HFont f18 = Font(18), f12 = Font(12), f13 = Font(13), f11 = Font(11);
	const int x = m_rPreview.x, w = m_rPreview.w, gap = S(10);
	int y = m_rPreview.y + m_rPreview.h + gap;

	std::wstring name = card.name;
	const int variant = Variant();
	int count = 0;
	const GmVariant *variants = card.type == CARD_ENTRY ? GmCatalog_Variants(g_GmCatalog[card.entry].iVariants, count) : nullptr;
	if (variants && variant >= 0)
		name += L" (" + Widen(variants[variant].pszName) + L")";
	Text(f18, x, y, BRIGHT, name.c_str());
	y += FontTall(f18) + gap;

	if (card.type == CARD_ENTRY || card.type == CARD_TP_PLACE)
	{
		const int entry = card.entry;
		const int v = card.type == CARD_TP_PLACE
		    ? ((s_iTeleInfo & GM_TP_ENTRANCE_PENDING) ? GM_TP_ROLE_EXIT : GM_TP_ROLE_ENTRANCE) : variant;
		std::wstring line;
		if (card.type == CARD_ENTRY && g_GmCatalog[entry].iKind == GM_KIND_MONSTER)
			line = TabName(g_GmCatalog[entry].iCategory); // "Tier I", as the tabs
		const std::wstring price = CardText(entry, v, GM_TEXT_PRICE);
		if (!price.empty())
			line += (line.empty() ? L"" : L" \u00b7 ") + price + L" pts";
		const std::wstring cooldown = CardText(entry, v, GM_TEXT_COOLDOWN);
		if (!cooldown.empty())
			line += (line.empty() ? L"" : L" \u00b7 ") + std::wstring(L"cooldown ") + cooldown + L"s";
		if (!line.empty())
		{
			Text(f12, x, y, DIM, line.c_str());
			y += FontTall(f12) + gap;
		}
		const std::wstring pay = CardText(entry, v, GM_TEXT_KILLPAY);
		if (!pay.empty())
		{
			Text(f12, x, y, VALUE, (L"Kills pay you " + pay + L" pts").c_str());
			y += FontTall(f12) + gap;
		}
		// the Fed's and the Zombie Grunt's loadouts (the plugin's loadout menus)
		if (variants)
		{
			int bx = x;
			for (int i = 0; i < count; i++)
			{
				const std::wstring label = Widen(variants[i].pszName);
				const int bw = TextWide(f11, label.c_str()) + S(12);
				if (bx + bw > x + w)
				{
					bx = x;
					y += S(24) + S(4);
				}
				Rect r = { bx, y, bw, S(24) };
				m_rVariants.push_back(r);
				Button(r, label.c_str(), i == variant, 11);
				bx += bw + S(4);
			}
			y += S(24) + gap;
		}
		Button(m_rPlace, L"Place in the world", true, 15, CanAfford());
		return;
	}

	// teleporter pairs and Clear All (the plugin's Manage Teleporters and its Yes / No)
	const int bh = S(44), by = m_rPlace.y + m_rPlace.h - bh;
	if (card.type == CARD_TP_PAIR)
	{
		if (m_bConfirmRemove)
		{
			Text(f13, x, by - bh - gap - FontTall(f13) - gap, DIM, L"Remove Teleporters?");
			m_rActions[0] = { x, by - bh - gap, w, bh };
			m_rActions[1] = { x, by, w, bh };
			Button(m_rActions[0], L"Yes", true, 15);
			Button(m_rActions[1], L"No", false, 15);
			return;
		}
		m_rActions[0] = { x, by - 2 * (bh + gap), w, bh };
		m_rActions[1] = { x, by - (bh + gap), w, bh };
		m_rActions[2] = { x, by, w, bh };
		Button(m_rActions[0], L"Go to Entrance", true, 15);
		Button(m_rActions[1], L"Go to Exit", false, 15);
		Button(m_rActions[2], L"Remove Teleporters", false, 15);
		return;
	}
	if (m_bConfirmClear)
	{
		Text(f13, x, by - bh - gap - FontTall(f13) - gap, DIM, L"Clear All Teleporters?");
		m_rActions[0] = { x, by - bh - gap, w, bh };
		m_rActions[1] = { x, by, w, bh };
		Button(m_rActions[0], L"Yes", true, 15);
		Button(m_rActions[1], L"No", false, 15);
		return;
	}
	m_rActions[0] = { x, by, w, bh };
	Button(m_rActions[0], L"Clear All Teleporters", true, 15);
}

//-----------------------------------------------------------------------------
// CSboGmFade: black under every popup while the GM intro window is open
//-----------------------------------------------------------------------------
CSboGmFade::CSboGmFade(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboGmFade")
{
	SetPaintBackgroundEnabled(true);
	SetPaintEnabled(false);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetZPos(-100); // under the viewport's other children too
	SetVisible(false);
	vgui2::ivgui()->AddTickSignal(GetVPanel());
}

void CSboGmFade::OnThink()
{
	const double now = vgui2::system()->GetCurrentTime();
	float frameTime = (float)(now - m_flLastTime);
	m_flLastTime = now;
	if (frameTime < 0.0f || frameTime > 0.25f)
		frameTime = 0.0f;

	// the intro (state 1) only; the guide shown again later (Misc. > Help) is a window over the game
	const bool want = s_iGmState == GM_STATE_INTRO;

	if (want)
		m_flAlpha += frameTime * 255.0f / 0.15f;
	else
		m_flAlpha -= frameTime * 255.0f / 0.4f;
	if (m_flAlpha > 255.0f)
		m_flAlpha = 255.0f;
	if (m_flAlpha < 0.0f)
		m_flAlpha = 0.0f;

	const bool visible = m_flAlpha > 0.0f;
	if (visible != IsVisible())
		SetVisible(visible);

	if (visible)
	{
		int wide, tall;
		vgui2::surface()->GetScreenSize(wide, tall);
		SetBounds(0, 0, wide, tall);
	}
}

void CSboGmFade::PaintBackground()
{
	int wide, tall;
	GetSize(wide, tall);
	vgui2::surface()->DrawSetColor(Color(0, 0, 0, (int)m_flAlpha));
	vgui2::surface()->DrawFilledRect(0, 0, wide, tall);
}

//-----------------------------------------------------------------------------
// Message and commands
//-----------------------------------------------------------------------------
namespace
{
struct CGmReader
{
	const unsigned char *data;
	int size;
	int pos;

	int ReadByte()
	{
		if (pos + 1 > size)
		{
			pos = size + 1;
			return -1;
		}
		return data[pos++];
	}

	void ReadString(char *buf, int bufSize)
	{
		int n = 0;
		while (pos < size)
		{
			char c = (char)data[pos++];
			if (!c)
				break;
			if (n < bufSize - 1)
				buf[n++] = c;
		}
		buf[n] = '\0';
	}
};

// "GmMode": byte state, byte event, byte entry, byte teleporters, string label (server gmmode.cpp GmSendState)
int MsgFunc_GmMode(const char *pszName, int iSize, void *pbuf)
{
	CGmReader reader = { (const unsigned char *)pbuf, iSize, 0 };
	const int state = reader.ReadByte();
	const int event = reader.ReadByte();
	const int entry = reader.ReadByte();
	const int teleInfo = reader.ReadByte();
	char label[128];
	reader.ReadString(label, sizeof(label));
	if (state < 0 || event < 0)
		return 1;

	s_iGmState = state;
	if (teleInfo >= 0)
		s_iTeleInfo = teleInfo;
	CSboGmSpawnMenu *pMenu = CSboGmSpawnMenu::Get();

	if (state == GM_STATE_OFF)
		ClearCards(); // the plugin's texts go with GM mode
	if (state == GM_STATE_INTRO)
		SetGuideTitle(Widen(label)); // the GM plugin's syndicate_gmmode_intro_title ("": the game's title)
	if (state != GM_STATE_ACTIVE)
	{
		s_bPlacing = false;
		if (pMenu)
			pMenu->Close();
		GmUi_ClosePlayers();
		return 1;
	}

	switch (event)
	{
	case GM_EVENT_OPEN_MENU:
		s_bPlacing = false;
		if (pMenu)
			pMenu->Open(entry);
		break;
	case GM_EVENT_OPEN_PLAYERS:
		if (ClassicMenus())
			sbo::ExecClientCmd("cmd gm_textmenu players");
		else
		{
			if (pMenu)
				pMenu->Close();
			GmUi_OpenPlayers(entry);
		}
		break;
	case GM_EVENT_PLACE:
		s_bPlacing = true;
		s_iPlaceEntry = entry < GM_CATALOG_COUNT ? entry : -1;
		if (entry == TeleportersEntry())
			s_iPlaceVariant = (s_iTeleInfo & GM_TP_ENTRANCE_PENDING) ? GM_TP_ROLE_EXIT : GM_TP_ROLE_ENTRANCE;
		else if (entry < GM_CATALOG_COUNT && g_GmCatalog[entry].iVariants != GM_VARIANTS_NONE)
			s_iPlaceVariant = s_iPickVariant;
		else
			s_iPlaceVariant = -1;
		s_PlaceLabel = Widen(label);
		break;
	case GM_EVENT_PLACE_END:
		s_bPlacing = false;
		break;
	}

	if (pMenu)
		pMenu->StateChanged();
	return 1;
}

// "GmEntry": byte entry, byte variant + 1, byte slot, short seconds, string text (server gmmode.cpp gm_entry_text)
int MsgFunc_GmEntry(const char *pszName, int iSize, void *pbuf)
{
	CGmReader reader = { (const unsigned char *)pbuf, iSize, 0 };
	const int entry = reader.ReadByte();
	const int variant = reader.ReadByte() - 1;
	const int slot = reader.ReadByte();
	const int lo = reader.ReadByte();
	const int hi = reader.ReadByte();
	char text[GM_TEXT_LEN];
	reader.ReadString(text, sizeof(text));
	if (hi < 0 || entry < 0 || entry >= GM_CATALOG_COUNT || variant < -1 || variant >= GM_PICK_MAX_VARIANT || slot < 0
	    || slot >= GM_TEXT_SLOTS)
		return 1;
	int seconds = lo | (hi << 8);
	if (seconds > 32767)
		seconds = 0;

	GmCardText &card = s_Cards[entry][variant + 1][slot];
	card.text = Widen(text);
	card.end = seconds > 0 ? sbo::GetHudTime() + seconds : 0;
	card.total = (float)seconds;
	return 1;
}

// The GM key (default bind K "gmon"): a Game Master's quick menu (the server's; pressed again with it up: the spawn
// window), the open window closes; anybody else asks the server to become one
void Cmd_GmOn()
{
	if (s_iGmState == GM_STATE_ACTIVE)
	{
		if (CSboGmSpawnMenu::Get() && CSboGmSpawnMenu::Get()->IsOpen())
			CSboGmSpawnMenu::Get()->Close();
		else if (GmUi_PlayersOpen())
			GmUi_ClosePlayers();
		else
			sbo::ExecClientCmd("cmd gm_key");
		return;
	}
	if (s_iGmState == GM_STATE_INTRO)
		return;
	sbo::ExecClientCmd("cmd gmon");
}

// the plugin's menu commands (gmmenu, gm_menu, gm_spawnmenu): the spawn window, for a Game Master
void Cmd_GmMenu()
{
	if (s_iGmState != GM_STATE_ACTIVE || !CSboGmSpawnMenu::Get())
		return;
	if (CSboGmSpawnMenu::Get()->IsOpen())
		CSboGmSpawnMenu::Get()->Close();
	else
		CSboGmSpawnMenu::Get()->Open();
}
} // namespace

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
bool VGui2_GmSpawn_Init()
{
	if (CSboGmSpawnMenu::Get())
		return true;

	if (!VGui2_IsAvailable() || !g_pViewport)
		return false;

	s_pFade = new CSboGmFade(g_pViewport);
	GmUi_Init();
	CSboGmSpawnMenu *pMenu = CSboGmSpawnMenu::Create(g_pViewport);
	pMenu->MakeReadyForUse();

	sbo::HookUserMsg("GmMode", &MsgFunc_GmMode);
	sbo::HookUserMsg("GmEntry", &MsgFunc_GmEntry);
	sbo::AddCommand("gmon", &Cmd_GmOn);
	sbo::AddCommand("gmmenu", &Cmd_GmMenu);
	sbo::AddCommand("gm_menu", &Cmd_GmMenu);
	sbo::AddCommand("gm_spawnmenu", &Cmd_GmMenu);
	return true;
}

void VGui2_GmSpawn_Reset()
{
	s_iGmState = GM_STATE_OFF;
	s_iTeleInfo = 0;
	s_bPlacing = false;
	ClearCards();
	GmUi_Reset();
	if (CSboGmSpawnMenu::Get())
		CSboGmSpawnMenu::Get()->Close();
}

bool VGui2_GmSpawn_IsOpen()
{
	// a GM window or the selection box: the mouse buttons and keys are VGUI2's
	return (CSboGmSpawnMenu::Get() && CSboGmSpawnMenu::Get()->IsOpen()) || GmUi_PlayersOpen() || VGui2_GmBoxSelecting();
}
