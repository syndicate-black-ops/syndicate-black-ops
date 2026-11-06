//========= SBO VGUI2: Game Master mode spawn window =========//
//
// Purpose: The client side of SBO 2.0's Game Master mode (server: src/dlls/gmmode.cpp; docs/GM_MODE.md):
//   * CSboGmSpawnMenu: the spawn window, GM screen 2:
//     header "Spawn" with the points and the tier, tabs Tier I / Tier II / Tier III / Support / Teleporters, a
//     search box, a grid of four columns of cards (the entry's model turning, its name, its cost and a tag: "Ready",
//     a cooldown "[s]s", or "Tier [n]" on a locked card) and the right panel with the highlighted entry's large
//     model, "Tier - cost - cooldown", "Kills pay you" and "Place in the world". The plugin's numbers come
//     as card texts (GmEntry); without a plugin the cards show the names and "Ready". The catalog is
//     game_shared/gm_catalog.h, which the server checks every pick against ("gm_pick <entry> <loadout> <count>",
//     "gm_tp ..."). With classic menus (cl_new_ingame_menus 0) the server shows the same lists as text menus.
//   * CSboGmFade: the black screen behind the GM intro window (the plugin's black box model).
//   * the "GmMode" and "GmEntry" messages and the client command "gmon" (the GM key: the server's quick menu, a
//     second press the spawn window; else the server's "gmon"), "gmmenu", "gm_menu", "gm_spawnmenu".
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_GM_SPAWN_MENU_H
#define SBO_VGUI2_SBO_GM_SPAWN_MENU_H

#include <string>
#include <vector>
#include <Color.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/TextEntry.h>

class CSboGmSpawnMenu;

// The search box: the window's keys go to the window
class CSboGmTextEntry : public vgui2::TextEntry
{
	DECLARE_CLASS_SIMPLE(CSboGmTextEntry, vgui2::TextEntry);

public:
	CSboGmTextEntry(CSboGmSpawnMenu *pMenu, const char *pName);

	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	virtual void OnKeyCodeTyped(vgui2::KeyCode code) override;
	virtual void PaintBackground() override;
	void SetPlaceholder(const wchar_t *text) { m_Placeholder = text; }
	virtual void Paint() override;

private:
	CSboGmSpawnMenu *m_pMenu;
	std::wstring m_Placeholder;
};

class CSboGmSpawnMenu : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CSboGmSpawnMenu, vgui2::EditablePanel);

public:
	static CSboGmSpawnMenu *Create(vgui2::Panel *pParent);
	static CSboGmSpawnMenu *Get() { return s_pInstance; }

	//! entry: the catalog entry to open on (its tab, highlighted); GM_CATALOG_COUNT + tab: that tab; -1 the last tab
	void Open(int entry = -1);
	//! The server's state changed (teleporter pairs): the Teleporters tab follows
	void StateChanged();
	void Close();
	bool IsOpen() const { return m_bOpen; }

	//! Keys from the text boxes (and the window). True if taken.
	bool HandleKey(vgui2::KeyCode code);

	virtual void OnThink() override;
	virtual void Paint() override;
	virtual void PaintBackground() override;
	virtual void OnMousePressed(vgui2::MouseCode code) override;
	virtual void OnMouseDoublePressed(vgui2::MouseCode code) override;
	virtual void OnMouseReleased(vgui2::MouseCode code) override;
	virtual void OnCursorMoved(int x, int y) override;
	virtual void OnMouseWheeled(int delta) override;
	virtual void OnKeyCodeTyped(vgui2::KeyCode code) override;

	MESSAGE_FUNC(OnSearchChanged, "TextChanged");

private:
	CSboGmSpawnMenu(vgui2::Panel *pParent);

	enum CardType
	{
		CARD_ENTRY = 0,  //!< a catalog entry (monster or support item)
		CARD_TP_PLACE,   //!< Place Teleporter Pair / Exit
		CARD_TP_PAIR,    //!< a pair (pair: 1..)
		CARD_TP_CLEAR,   //!< Clear All Teleporters
	};

	struct Card
	{
		int type = CARD_ENTRY;
		int entry = -1;
		int pair = 0;
		std::wstring name;
	};

	struct Rect
	{
		int x = 0, y = 0, w = 0, h = 0;
		bool Hit(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
	};

	void Layout();
	void Rebuild();
	void UpdatePreviews();
	void Place();
	void SelectCard(int index);
	void MoveSelection(int dx, int dy);
	bool IsCloseKey(vgui2::KeyCode code);
	int Variant() const; //!< the highlighted entry's loadout (-1 none)
	int SelectedCost() const; //!< the highlighted card's cost in points from the budget plugin (0 none)
	bool CanAfford() const;   //!< false: the highlighted card would be refused (cost, lock, cooldown, limit): Place greys out
	void PaintCard(int index, const Rect &r);
	void PaintPanel();
	void Button(const Rect &r, const wchar_t *label, bool primary, float px, bool enabled = true);

	static CSboGmSpawnMenu *s_pInstance;

	bool m_bOpen = false;
	int m_iTab = 0;
	int m_iSelected = 0;
	int m_iScrollRow = 0;
	int m_iVariant[64] = {}; //!< per entry: the loadout chosen in the right panel
	bool m_bConfirmClear = false;
	bool m_bConfirmRemove = false;
	std::vector<Card> m_Cards;
	float m_flYaw = 0;
	bool m_bDragging = false;
	int m_iDragX = 0;

	Rect m_rWin, m_rClose, m_rTabs[5], m_rSearch, m_rGrid, m_rPanel, m_rPreview, m_rPlace;
	Rect m_rActions[3]; //!< the right panel's teleporter / confirm buttons
	std::vector<Rect> m_rVariants;
	std::vector<Rect> m_rCards; //!< per visible card (m_iScrollRow * 4 + i)

	CSboGmTextEntry *m_pSearch = nullptr;
};

// The black screen behind the intro window
class CSboGmFade : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE(CSboGmFade, vgui2::Panel);

public:
	CSboGmFade(vgui2::Panel *pParent);

	virtual void OnThink() override;
	virtual void PaintBackground() override;

private:
	float m_flAlpha = 0.0f;
	double m_flLastTime = 0.0;
};

#endif // SBO_VGUI2_SBO_GM_SPAWN_MENU_H
