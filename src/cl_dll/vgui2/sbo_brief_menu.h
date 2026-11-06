//========= SBO VGUI2 port: briefing (equipment) menu =========//
//
// Purpose: VGUI2 version of SBO's CSyndBrief (src/cl_dll/vgui_SyndBrief.cpp, server menu 10, opened by the team
//          menu's Go! through "ShowVGUI 10"). Seven columns of item buttons (blades, light weapons, assault weapons,
//          cyber tech., equipment, armor, agent sex), a picture (gfx/vgui/<320|640>_<item>.tga) and a description
//          (#SBO_Item_<item>, the old vgui/<item>.txt) of the item under the cursor, Cancel and "Go !".
//
// Same behaviour as VGUI1:
//   * cursor over an item: its picture and description (CSyndBrief::SetActiveInfo); nothing is armed by hovering
//   * click on an item: the choice goes into the viewport (TeamFortressViewport::m_iBlade ... through
//     sbo::SetBriefChoice), only that button of the column is armed, then HideCommandMenu (CMenuHandler_<category>)
//   * "Go !": "blade n", "light n", "heavy n", "item n", "cyberware n", "armor n", "malefemale n", then HideTopMenu
//     (CMenuHandler_Go)
//   * Cancel: HideTopMenu, ClientCmd( "" ), HideTopMenu (also closes the menu queued next), disarm
//   * no keys (TeamFortressViewport::KeyInput has none for menu 10; the proxy's SlotInput returns false)
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_BRIEF_MENU_H
#define SBO_VGUI2_SBO_BRIEF_MENU_H

#include "sbo_brief_item.h"
#include "sbo_widgets.h"

class CSboBriefMenu : public CSboMenuPanel, public ISboBriefInfoTarget
{
	DECLARE_CLASS_SIMPLE(CSboBriefMenu, CSboMenuPanel);

public:
	enum
	{
		NUM_CATEGORIES = 7,     //!< columns, in CSyndBrief's creation order
		MAX_CATEGORY_ITEMS = 9, //!< assault weapons
		NUM_ITEMS = 32,         //!< SetActiveInfo 1..32
	};

	//! Creates the panel once (VGui2_Menus_Init).
	static CSboBriefMenu *Create(vgui2::Panel *pParent);
	static CSboBriefMenu *Get() { return s_pInstance; }

	//! ShowSyndBrief + CMenuPanel::Open: arms the viewport's current choices and shows the menu.
	void Open();

	virtual void SetActiveInfo(int iInput) override;
	virtual void OnCommand(const char *command) override;

protected:
	virtual void LayoutMenu() override;

private:
	CSboBriefMenu(vgui2::Panel *pParent);

	//! CMenuHandler_<category>::actionPerformed
	void Choose(int iCategory, int iChoice);
	//! CMenuHandler_Go::actionPerformed
	void Go();
	//! The item pictures for SBO's current resolution (LoadTGAForRes: 320_ below 1024 pixels width, else 640_).
	void LoadImages();

	static CSboBriefMenu *s_pInstance;

	CSboTransparentPanel *m_pBackgroundPanel = nullptr;
	CSboTransparentPanel *m_pCategoryPanel[NUM_CATEGORIES] = {};
	CSboLabel *m_pCategoryLabel[NUM_CATEGORIES] = {};
	CSboBriefItemButton *m_pItemButton[NUM_CATEGORIES][MAX_CATEGORY_ITEMS] = {};

	CSboTransparentPanel *m_pButtonPanel = nullptr;
	CSboCommandButton *m_pCancelButton = nullptr;
	CSboCommandButton *m_pGoButton = nullptr;

	CSboTransparentPanel *m_pInfoPanel = nullptr;
	CSboTransparentPanel *m_pScrollPanel = nullptr; //!< the CTFScrollPanel without scrollbars: clips the text
	CSboTextPanel *m_pInfoText = nullptr;

	CSboTransparentPanel *m_pPicturePanel = nullptr;
	CSboTransparentPanel *m_pImagePanel = nullptr; //!< 320x320 (screen wider than 800) or 200x200, centred
	CSboImagePanel *m_pImage[NUM_ITEMS] = {};

	int m_iImageRes = 0; //!< 320 / 640: the TGA set loaded (0 = none yet)
};

#endif // SBO_VGUI2_SBO_BRIEF_MENU_H
