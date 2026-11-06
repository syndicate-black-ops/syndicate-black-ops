//========= SBO VGUI2 port: briefing (equipment) menu =========//
//
// Purpose: see sbo_brief_menu.h.
//=================================================================//

#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "sbo_bridge.h"
#include "sbo_brief_menu.h"
#include "sbo_loadout_menu.h"
#include "sbo_menus.h"
#include "vgui2_api.h"

namespace
{
struct BriefItem
{
	const char *name;  //!< vgui/<name>.txt -> #SBO_Item_<name> (description), #SBO_Brief_Item_<name> (button)
	const char *image; //!< CImageLabel name: gfx/vgui/<320|640>_<image>.tga
};

// One column of CSyndBrief's constructor: CTransparentPanel( 255, x, y, wide, tall ) with a "Title Font" label at
// (0, YRES(5)) and CommandButtons of buttonWide x YRES(20) at (0, YRES(33 + 23 * i)).
struct BriefCategory
{
	int choice;             //!< sbo::BriefChoice
	const char *titleToken;
	float x, y, wide, tall; //!< 640x480 units (XRES/YRES)
	float buttonWide;
	int numItems;
	BriefItem items[CSboBriefMenu::MAX_CATEGORY_ITEMS];
};

// CSyndBrief's creation order, which is also the order of the SetActiveInfo numbers (CHandler_MenuButtonOver):
// blades 1-3, light 4-7, assault 8-16, cyber tech. 17-22, equipment 23-26, armor 27-30, agent 31-32.
const BriefCategory s_Categories[CSboBriefMenu::NUM_CATEGORIES] = {
	{ sbo::BRIEF_BLADE, "#SBO_Brief_Title_Blades", 5, -5, 180, 99, 180, 3,
	    { { "hammer", "hammer" }, { "chainsaw", "chainsaw" }, { "sword", "sword" } } },
	{ sbo::BRIEF_LIGHT, "#SBO_Brief_Title_Light", 5, 105, 180, 122, 180, 4,
	    { { "rc28", "rc28" }, { "k444", "k444" }, { "canon", "canon" }, { "pm66", "pm66" } } },
	{ sbo::BRIEF_HEAVY, "#SBO_Brief_Title_Assault", 5, 238, 180, 237, 180, 9,
	    { { "fa18", "fa18" }, { "federal", "federal" }, { "sw", "sw" }, { "nm33", "nm" }, { "spa", "spaauto" },
	        { "ben", "ben" }, { "glauncher", "glauncher" }, { "chaingun", "chaingun" }, { "lr", "lr" } } },
	{ sbo::BRIEF_CYBERWARE, "#SBO_Brief_Title_Cyber", 485, 232, 150, 168, 150, 6,
	    { { "legs", "legs" }, { "targeting", "targeting" }, { "medikit", "medikit" }, { "adrenaline", "adrenaline" },
	        { "assassin", "assassin" }, { "eye", "eye" } } },
	{ sbo::BRIEF_ITEM, "#SBO_Brief_Title_Equipment", 485, -5, 150, 122, 150, 4,
	    { { "grenade", "grenade" }, { "tripmine", "tripmine" }, { "satchel", "satchel" }, { "pit", "pit" } } },
	{ sbo::BRIEF_ARMOR, "#SBO_Brief_Title_Armor", 485, 114, 150, 122, 150, 4,
	    { { "skin", "skin" }, { "torso", "torso" }, { "skull", "skull" }, { "noarmor", "noarmor" } } },
	// button 1 "Female agent" = malefemale 1
	{ sbo::BRIEF_MALEFEMALE, "#SBO_Brief_Title_Agent", 485, 399, 150, 76, 150, 2,
	    { { "female", "female" }, { "male", "male" } } },
};

// CMenuHandler_Go: one command per choice, in this order
const struct
{
	int choice;
	const char *format;
} s_GoCommands[] = {
	{ sbo::BRIEF_BLADE, "blade %d" },
	{ sbo::BRIEF_LIGHT, "light %d" },
	{ sbo::BRIEF_HEAVY, "heavy %d" },
	{ sbo::BRIEF_ITEM, "item %d" },
	{ sbo::BRIEF_CYBERWARE, "cyberware %d" },
	{ sbo::BRIEF_ARMOR, "armor %d" },
	{ sbo::BRIEF_MALEFEMALE, "malefemale %d" },
};

//! SetActiveInfo number (1..32) -> category and item index; false if out of range.
bool InfoToItem(int iInput, int &iCategory, int &iItem)
{
	int first = 1;
	for (int c = 0; c < CSboBriefMenu::NUM_CATEGORIES; c++)
	{
		if (iInput >= first && iInput < first + s_Categories[c].numItems)
		{
			iCategory = c;
			iItem = iInput - first;
			return true;
		}
		first += s_Categories[c].numItems;
	}
	return false;
}
} // namespace

CSboBriefMenu *CSboBriefMenu::s_pInstance = nullptr;

CSboBriefMenu *CSboBriefMenu::Create(vgui2::Panel *pParent)
{
	if (!s_pInstance)
		s_pInstance = new CSboBriefMenu(pParent);
	return s_pInstance;
}

CSboBriefMenu::CSboBriefMenu(vgui2::Panel *pParent)
    : BaseClass(pParent, "SboBriefMenu")
{
	SetBackdropTrans(175); // CSyndBrief( 175, ... )

	wchar_t wbuf[256];
	char name[64];
	char command[32];

	// the whole screen once more: CTransparentPanel( 128, 0, 0, XRES(640), YRES(480) )
	m_pBackgroundPanel = new CSboTransparentPanel(this, "Background", 128);

	int iInfo = 1;
	for (int c = 0; c < NUM_CATEGORIES; c++)
	{
		const BriefCategory &cat = s_Categories[c];

		snprintf(name, sizeof(name), "Category%d", c);
		m_pCategoryPanel[c] = new CSboTransparentPanel(m_pBackgroundPanel, name, 255);

		// "Title Font" with that scheme's colours (its BgColor is transparent)
		m_pCategoryLabel[c] = new CSboLabel(m_pCategoryPanel[c], "Title", "Title Font", sboui::SBO_GREEN);
		m_pCategoryLabel[c]->UseTextSchemeFgColor();
		sboui::Localize(cat.titleToken, wbuf, sizeof(wbuf));
		m_pCategoryLabel[c]->SetText(wbuf);

		for (int i = 0; i < cat.numItems; i++, iInfo++)
		{
			snprintf(name, sizeof(name), "Item_%s", cat.items[i].name);
			snprintf(command, sizeof(command), "choose %d %d", c, i + 1);
			m_pItemButton[c][i] = new CSboBriefItemButton(m_pCategoryPanel[c], name, this, command, this, iInfo);

			char token[64];
			snprintf(token, sizeof(token), "#SBO_Brief_Item_%s", cat.items[i].name);
			sboui::Localize(token, wbuf, sizeof(wbuf));
			m_pItemButton[c][i]->SetCommandText(wbuf);
		}
	}

	m_pButtonPanel = new CSboTransparentPanel(m_pBackgroundPanel, "ButtonPanel", 255);

	// CommandButton( "Cancel", ..., false ): normal highlight; centred text (behind the five-space key gap)
	m_pCancelButton = new CSboCommandButton(m_pButtonPanel, "CancelButton", this, "cancel");
	m_pCancelButton->SetCommandAlignment(vgui2::Label::a_center);
	sboui::Localize("#SBO_Brief_Cancel", wbuf, sizeof(wbuf));
	m_pCancelButton->SetCommandText(wbuf);

	// CommandButton( "", ..., true ) + CHandler_ButtonHighlight: armed while the cursor is over it
	m_pGoButton = new CSboCommandButton(m_pButtonPanel, "GoButton", this, "go");
	m_pGoButton->SetCommandAlignment(vgui2::Label::a_center);
	sboui::Localize("#SBO_Brief_Go", wbuf, sizeof(wbuf));
	m_pGoButton->SetCommandText(wbuf);

	// description: green-bordered window, a CTFScrollPanel without scrollbars (clips), a TextPanel in "Briefing Text"
	// without setFgColor (a TextImage's default colour, opaque white)
	m_pInfoPanel = new CSboTransparentPanel(m_pBackgroundPanel, "InfoPanel", 255);
	m_pInfoPanel->SetLineBorder(sboui::SBO_GREEN);
	m_pScrollPanel = new CSboTransparentPanel(m_pInfoPanel, "ScrollPanel", 0);
	m_pInfoText = new CSboTextPanel(m_pScrollPanel, "InfoText", "Briefing Text", Color(255, 255, 255, 255), true);

	// picture: one CImageLabel per item in a centred box, only the active one visible
	m_pPicturePanel = new CSboTransparentPanel(m_pBackgroundPanel, "PicturePanel", 255);
	m_pImagePanel = new CSboTransparentPanel(m_pPicturePanel, "ImagePanel", 255);
	int iImage = 0;
	for (int c = 0; c < NUM_CATEGORIES; c++)
	{
		for (int i = 0; i < s_Categories[c].numItems; i++, iImage++)
		{
			snprintf(name, sizeof(name), "Image_%s", s_Categories[c].items[i].image);
			m_pImage[iImage] = new CSboImagePanel(m_pImagePanel, name);
			m_pImage[iImage]->SetVisible(false);
		}
	}
}

void CSboBriefMenu::LoadImages()
{
	int screenWide, screenTall;
	sbo::GetScreenSize(screenWide, screenTall);
	int res = screenWide < 1024 ? 320 : 640;
	if (res == m_iImageRes)
		return;

	int iImage = 0;
	for (int c = 0; c < NUM_CATEGORIES; c++)
	{
		for (int i = 0; i < s_Categories[c].numItems; i++, iImage++)
		{
			// a missing file (noarmor: SBO ships no picture) shows nothing, like VGUI1
			m_pImage[iImage]->SetImage(s_Categories[c].items[i].image);
			m_pImage[iImage]->SetPos(0, 0);
		}
	}
	m_iImageRes = res;
}

void CSboBriefMenu::Open()
{
	// the viewport's choices (CSyndBrief armed them in its constructor and keeps them armed by its handlers)
	for (int c = 0; c < NUM_CATEGORIES; c++)
	{
		int choice = sbo::GetBriefChoice(s_Categories[c].choice);
		for (int i = 0; i < s_Categories[c].numItems; i++)
			m_pItemButton[c][i]->SetChosen(choice == i + 1);
	}

	m_pCancelButton->SetArmed(false);
	m_pGoButton->SetArmed(false);

	ShowMenu();
}

void CSboBriefMenu::LayoutMenu()
{
	using sboui::XRes;
	using sboui::YRes;

	m_pBackgroundPanel->SetBounds(XRes(0), YRes(0), XRes(640), YRes(480));

	for (int c = 0; c < NUM_CATEGORIES; c++)
	{
		const BriefCategory &cat = s_Categories[c];

		// The panel is as tall as its last button's bottom in 640x480 units (33 + 23 * (n - 1) + 20), but the two are
		// rounded on their own: YRes(y) + YRes(20) can be one pixel more than YRes(tall) (at 1050 pixels height:
		// Assault weapons 475 + 44 = 519 against 518, "Who are you ?" 123 + 44 = 167 against 166), and the panel then
		// clipped the last button's bottom border line. In VGUI1 too (same numbers). The panel has no background or
		// border of its own, so making it tall enough for its last button changes nothing else.
		int panelTall = YRes(cat.tall);
		if (cat.numItems > 0)
			panelTall = std::max(panelTall, YRes(33 + 23 * (cat.numItems - 1)) + YRes(20));
		m_pCategoryPanel[c]->SetBounds(XRes(cat.x), YRes(cat.y), XRes(cat.wide), panelTall);

		m_pCategoryLabel[c]->SetPos(XRes(0), YRes(5));
		m_pCategoryLabel[c]->SizeToContents();

		for (int i = 0; i < cat.numItems; i++)
			m_pItemButton[c][i]->SetBounds(XRes(0), YRes(33 + 23 * i), XRes(cat.buttonWide), YRes(20));
	}

	m_pButtonPanel->SetBounds(XRes(190), YRes(450), XRes(290), YRes(30));
	m_pCancelButton->SetBounds(XRes(0), YRes(5), XRes(143), YRes(20));
	m_pGoButton->SetBounds(XRes(147), YRes(5), XRes(143), YRes(20));

	m_pInfoPanel->SetBounds(XRes(190), YRes(330), XRes(290), YRes(122));
	m_pScrollPanel->SetBounds(XRes(2), YRes(2), XRes(290) - XRes(4), YRes(122) - YRes(4));
	// VGUI1: TextPanel( "", XRES(2), YRES(2), XRES(290) - XRES(22), YRES(122) + YRES(200) ), taller than the scroll
	// panel, which clipped it: long descriptions were cut off. Here the text fits the visible box and scrolls (scroll
	// bar when needed, mouse wheel).
	m_pInfoText->SetBounds(XRes(2), YRes(2), XRes(290) - XRes(8), YRes(122) - YRes(8));

	m_pPicturePanel->SetBounds(XRes(190), YRes(5), XRes(290), YRes(320));

	// 320x320 for screens wider than 800 pixels, else 200x200 (pixels, not XRES), centred in the picture panel. The
	// TGA set follows LoadTGAForRes instead (640_ from 1024 pixels on), so 801-1023 pixels show the 200x200 pictures
	// in the 320 box's top-left corner, and 320_chaingun (320x320) is clipped to 200x200 - as in VGUI1.
	int screenWide, screenTall;
	sbo::GetScreenSize(screenWide, screenTall);
	int box = screenWide > 800 ? 320 : 200;
	int pictureWide, pictureTall;
	m_pPicturePanel->GetSize(pictureWide, pictureTall);
	m_pImagePanel->SetBounds(pictureWide / 2 - box / 2, pictureTall / 2 - box / 2, box, box);

	LoadImages();
}

void CSboBriefMenu::SetActiveInfo(int iInput)
{
	int iCategory, iItem;
	if (!InfoToItem(iInput, iCategory, iItem))
		return;

	// nothing is shown before the first item is hovered (VGUI1: empty text, all pictures hidden); the last item
	// stays shown when the menu is opened again
	char token[64];
	snprintf(token, sizeof(token), "#SBO_Item_%s", s_Categories[iCategory].items[iItem].name);
	static wchar_t s_wText[2048];
	sboui::Localize(token, s_wText, sizeof(s_wText));
	m_pInfoText->SetWideText(s_wText); // also scrolls to the top (setScrollValue( 0, 0 ))

	for (int i = 0; i < NUM_ITEMS; i++)
		m_pImage[i]->SetVisible(i == iInput - 1);
}

void CSboBriefMenu::Choose(int iCategory, int iChoice)
{
	if (iCategory < 0 || iCategory >= NUM_CATEGORIES)
		return;

	const BriefCategory &cat = s_Categories[iCategory];
	sbo::SetBriefChoice(cat.choice, iChoice);

	// arm only the chosen button of the column
	for (int i = 0; i < cat.numItems; i++)
		m_pItemButton[iCategory][i]->SetChosen(iChoice == i + 1);

	// m_iCloseVGUIMenu is false
	sbo::HideCommandMenu();
}

void CSboBriefMenu::Go()
{
	for (const auto &cmd : s_GoCommands)
	{
		char sz[32];
		snprintf(sz, sizeof(sz), cmd.format, sbo::GetBriefChoice(cmd.choice));
		sbo::ExecClientCmd(sz);
	}

	// CMenuHandler_Go( 1 )
	sbo::HideTopMenu();
}

void CSboBriefMenu::OnCommand(const char *command)
{
	int iCategory, iChoice;
	if (sscanf(command, "choose %d %d", &iCategory, &iChoice) == 2)
	{
		Choose(iCategory, iChoice);
		return;
	}

	if (!stricmp(command, "go"))
	{
		Go();
		return;
	}

	if (!stricmp(command, "cancel"))
	{
		// CMenuHandler_TextWindow( HIDE_TEXTWINDOW ), CMenuHandler_StringCommand( "", true ),
		// CMenuHandler_EnleveHighLight
		sbo::HideTopMenu();
		sbo::ExecClientCmd("");
		sbo::HideTopMenu();
		m_pCancelButton->SetArmed(false);
		return;
	}

	BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
void VGui2_BriefMenu_Show()
{
	// "New in-game menus": the new loadout menu instead (sbo_loadout_menu.cpp), same commands and flow
	if (VGui2_LoadoutMenu_Wanted())
	{
		VGui2_LoadoutMenu_Show();
		return;
	}
	if (CSboBriefMenu::Get())
		CSboBriefMenu::Get()->Open();
}

void VGui2_BriefMenu_Hide()
{
	VGui2_LoadoutMenu_Hide();
	if (CSboBriefMenu::Get())
		CSboBriefMenu::Get()->HideMenu();
}

bool VGui2_BriefMenu_SlotInput(int iSlot)
{
	return VGui2_LoadoutMenu_SlotInput(iSlot);
}
