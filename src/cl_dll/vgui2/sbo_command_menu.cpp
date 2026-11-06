//========= SBO VGUI2: command menus, the menus themselves =========//
//
// Purpose: see sbo_command_menu.h. This file: the menu file parser, the buttons and their state, layout, key input
//          and actions. No VGUI: the panels (sbo_command_menu_panel.cpp) draw what this holds.
//
// A port of the HL SDK's VGUI1 command menu as SBO 1.38 had it (vgui_TeamFortressViewport.cpp: CCommandMenu,
// TeamFortressViewport::CreateCommandMenu / CreateSubMenu / CreateCustomButton / ShowCommandMenu / ...;
// vgui_CustomObjects.cpp: CommandButton; vgui_TeamFortressViewport.h: the button classes and CMenuHandler_*).
// The arithmetic is kept as it was written there (floats truncated to ints where the VGUI1 calls did), so every
// menu and button lands on the same pixel. The names of the originals are in the comments.
//
// SBO world: compiled with SBO's normal client include paths. Must not include any Source SDK header.
//=================================================================//

#include "hud.h"
#include "cl_util.h"
#include "viewport.h"

#include <stdlib.h>
#include <string.h>

#include "sbo_command_menu.h"

namespace
{
const int MAX_MAPNAME_LENGTH = 256; // MAX_MAPNAME
const int COMMAND_LENGTH = 128;     // cCommandLength

struct CmdMenu;

// What a button does (the VGUI1 action signals)
enum
{
	ACTION_NONE = 0,   // a submenu button
	ACTION_COMMAND,    // CMenuHandler_StringCommand
	ACTION_TOGGLE,     // CMenuHandler_ToggleCvar
	ACTION_TEXTWINDOW, // CMenuHandler_TextWindow (CUSTOM buttons)
};

// When a button is valid (the VGUI1 classes' IsNotValid)
enum
{
	VALID_ALWAYS = 0,
	VALID_TEAM,       // TeamOnlyCommandButton
	VALID_MAP,        // MapButton
	VALID_SERVERINFO, // ClassButton( 0, ... ): the "!SERVERINFO" custom button
};

// CommandButton and its subclasses
struct CmdButton
{
	char mainText[MAX_BUTTON_SIZE]; // m_sMainText
	char token[MAX_BUTTON_SIZE];    // the menu file's text
	char boundKey;                  // m_cBoundKey
	int playerClass;                // m_iPlayerClass (CommandButton( iPlayerClass, ... ) only)
	CmdMenu *subMenu;                  // m_pSubMenu
	CmdMenu *parentMenu;               // m_pParentMenu
	bool visible;
	bool armed;
	int x, y, wide, tall;
	int style;                      // sbo::CommandButtonStyle

	int valid;
	int teamNum;                        // VALID_TEAM
	char mapName[MAX_MAPNAME_LENGTH];   // VALID_MAP: "maps/<map>.bsp"

	int action;
	char command[COMMAND_LENGTH];   // ACTION_COMMAND
	cvar_t *cvar;                   // TOGGLE buttons (also what the check mark shows)
	int textWindow;                 // ACTION_TEXTWINDOW: the menu to show

	CmdButton(const char *text, int x_, int y_, int wide_, int tall_)
	{
		memset(this, 0, sizeof(*this));
		x = x_;
		y = y_;
		wide = wide_;
		tall = tall_;
		visible = true; // a VGUI1 panel is visible until told otherwise
		style = sbo::COMMAND_BUTTON_NORMAL;
		SetText(text);
	}

	// CommandButton::setText
	void SetText(const char *text)
	{
		strncpy(mainText, text, MAX_BUTTON_SIZE);
		mainText[MAX_BUTTON_SIZE - 1] = 0;
	}

	bool IsNotValid();
	void CursorEntered();
	void CursorExited();
	void FireActionSignal();
};

// CCommandMenu
struct CmdMenu
{
	CmdMenu *parent;                 // m_pParentMenu
	int xOffset, yOffset;         // m_iXOffset, m_iYOffset
	CmdButton *buttons[MAX_BUTTONS]; // m_aButtons
	int numButtons;               // m_iButtons
	int direction;                // m_iDirection: opens top to bottom (0) or bottom to top (1)
	float flButtonSizeY;          // m_flButtonSizeY
	int spectCmdMenu;             // m_iSpectCmdMenu
	int pos[2], size[2];          // the VGUI1 panel's bounds
	char name[96];

	CmdMenu(CmdMenu *pParentMenu, int direction_, int x, int y, int wide, int tall)
	{
		memset(this, 0, sizeof(*this));
		parent = pParentMenu;
		xOffset = x;
		yOffset = y;
		direction = direction_;
		pos[0] = x;
		pos[1] = y;
		size[0] = wide;
		size[1] = tall;
	}

	void AddButton(CmdButton *pButton);
	bool RecalculateVisibles(int iNewYPos, bool bHideAll);
	void RecalculatePositions(int iYOffset);
	CmdButton *FindButtonWithSubmenu(CmdMenu *pSubMenu);
	void ClearButtonsOfArmedState();
	bool KeyInput(int keyNum);
};

// TeamFortressViewport's command menu members
CmdMenu *s_pMenus[MAX_MENUS];    // m_pCommandMenus
int s_iNumMenus = 0;          // m_iNumMenus
CmdMenu *s_pCurrentMenu = NULL;  // m_pCurrentCommandMenu
float s_flMenuOpenTime = 0;   // m_flMenuOpenTime
bool s_bInitialized = false;  // m_iInitialized

// Used for Class specific buttons (sTFClasses)
const char *const s_pszClasses[] =
{
	"",
	"SCOUT",
	"SNIPER",
	"SOLDIER",
	"DEMOMAN",
	"MEDIC",
	"HWGUY",
	"PYRO",
	"SPY",
	"ENGINEER",
	"CIVILIAN",
};

// TeamFortressViewport::SetCurrentCommandMenu
void SetCurrentMenu(CmdMenu *pNewMenu)
{
	s_pCurrentMenu = pNewMenu;

	// the panels show the open menu and its parents (CCommandMenu::MakeVisible)
	VGui2_CommandMenu_Changed();
}

//-----------------------------------------------------------------------------
// CmdButton
//-----------------------------------------------------------------------------
bool CmdButton::IsNotValid()
{
	switch (valid)
	{
	case VALID_TEAM:
		// TeamOnlyCommandButton: only displayed if the player is on team X
		return g_iTeamNumber != teamNum;

	case VALID_MAP:
	{
		// MapButton: does it match the current map name?
		const char *level = gEngfuncs.pfnGetLevelName();
		if (!level)
			return true;

		return strcmp(mapName, level) != 0;
	}

	case VALID_SERVERINFO:
	{
		// ClassButton( 0, ... )::IsNotValid: class 0 is never an illegal class (sTFValidClassInts[0] is 0), which
		// leaves the check of the player's current class (the SDK did it only with hud_classautokill; SBO 2.0 has no
		// such cvar, its default 1 is kept)
		if (gViewPort)
		{
			if ((gViewPort->IsRandomPC() && 0 == PC_RANDOM) || (!gViewPort->IsRandomPC() && (0 == g_iPlayerClass)))
				return true;
		}

		return false;
	}
	}

	return false;
}

// CommandButton::cursorEntered: highlights the current button, and all it's parent menus
void CmdButton::CursorEntered()
{
	// unarm all the other buttons in this menu
	CmdMenu *containingMenu = parentMenu;
	if (containingMenu)
	{
		containingMenu->ClearButtonsOfArmedState();

		// make all our higher buttons armed
		CmdMenu *pCParent = containingMenu->parent;
		if (pCParent)
		{
			CmdButton *pParentButton = pCParent->FindButtonWithSubmenu(containingMenu);

			if (pParentButton)
				pParentButton->CursorEntered();
		}
	}

	// arm ourselves
	armed = true;
}

// CommandButton::cursorExited
void CmdButton::CursorExited()
{
	// only clear ourselves if we have do not have a containing menu
	// only stay armed if we have a sub menu
	// the buttons only unarm themselves when another button is armed instead
	if (!parentMenu || !subMenu)
		armed = false;
}

// Button::fireActionSignal: the button's action signals
void CmdButton::FireActionSignal()
{
	switch (action)
	{
	case ACTION_COMMAND:
		// CMenuHandler_StringCommand
		gEngfuncs.pfnClientCmd(command);
		sbo::CommandMenu_Hide();
		break;

	case ACTION_TOGGLE:
		// CMenuHandler_ToggleCvar. SBO 2.0: through the engine, so the cvar's string changes with its value and an
		// archived cvar is saved to config.cfg with the new value (the SDK wrote only cvar->value: the setting
		// was back at the next start, and "spec_drawnames" in the console still printed the old value).
		if (cvar)
			gEngfuncs.Cvar_SetValue(cvar->name, cvar->value ? 0.0f : 1.0f);

		if (gViewPort)
			gViewPort->UpdateSpectatorPanel();
		break;

	case ACTION_TEXTWINDOW:
		// CMenuHandler_TextWindow
		sbo::CommandMenu_Hide();
		if (gViewPort)
			gViewPort->ShowVGUIMenu(textWindow);
		break;
	}
}

//-----------------------------------------------------------------------------
// CmdMenu
//-----------------------------------------------------------------------------
// CCommandMenu::AddButton
void CmdMenu::AddButton(CmdButton *pButton)
{
	if (numButtons >= MAX_BUTTONS)
		return;

	buttons[numButtons] = pButton;
	numButtons++;

	// give the button a default key binding
	if (numButtons < 10)
		pButton->boundKey = numButtons + '0';
	else if (numButtons == 10)
		pButton->boundKey = '0';
}

// CCommandMenu::KeyInput: tries to find a button that has a key bound to the input, and presses the button if
// found. Returns true if the command menu should close, false otherwise
bool CmdMenu::KeyInput(int keyNum)
{
	// loop through all our buttons looking for one bound to keyNum
	for (int i = 0; i < numButtons; i++)
	{
		if (!buttons[i]->IsNotValid())
		{
			if (buttons[i]->boundKey == keyNum)
			{
				// hit the button
				if (buttons[i]->subMenu)
				{
					// open the sub menu
					SetCurrentMenu(buttons[i]->subMenu);
					return false;
				}
				else
				{
					// run the bound command
					buttons[i]->FireActionSignal();
					return true;
				}
			}
		}
	}

	return false;
}

// CCommandMenu::ClearButtonsOfArmedState: clears the current menus buttons of any armed (highlighted) state, and
// all their sub buttons
void CmdMenu::ClearButtonsOfArmedState()
{
	for (int i = 0; i < numButtons; i++)
	{
		buttons[i]->armed = false;

		if (buttons[i]->subMenu)
			buttons[i]->subMenu->ClearButtonsOfArmedState();
	}
}

// CCommandMenu::FindButtonWithSubmenu
CmdButton *CmdMenu::FindButtonWithSubmenu(CmdMenu *pSubMenu)
{
	for (int i = 0; i < numButtons; i++)
	{
		if (buttons[i]->subMenu == pSubMenu)
			return buttons[i];
	}

	return NULL;
}

// CCommandMenu::RecalculateVisibles: recalculate the visible buttons
bool CmdMenu::RecalculateVisibles(int iYOffset, bool bHideAll)
{
	int i, iCurrentY = 0;
	int iVisibleButtons = 0;

	// Cycle through all the buttons in this menu, and see which will be visible
	for (i = 0; i < numButtons; i++)
	{
		int iClass = buttons[i]->playerClass;

		if ((iClass && iClass != g_iPlayerClass) || (buttons[i]->IsNotValid()) || bHideAll)
		{
			buttons[i]->visible = false;
			if (buttons[i]->subMenu != NULL)
				buttons[i]->subMenu->RecalculateVisibles(0, true);
		}
		else
		{
			// If it's got a submenu, force it to check visibilities
			if (buttons[i]->subMenu != NULL)
			{
				if (!buttons[i]->subMenu->RecalculateVisibles(0, false))
				{
					// The submenu had no visible buttons, so don't display this button
					buttons[i]->visible = false;
					continue;
				}
			}

			buttons[i]->visible = true;
			iVisibleButtons++;
		}
	}

	// Set Size
	size[1] = (int)((iVisibleButtons * (flButtonSizeY - 1)) + 1);

	if (iYOffset)
		yOffset = iYOffset;

	for (i = 0; i < numButtons; i++)
	{
		if (buttons[i]->visible)
		{
			if (buttons[i]->subMenu != NULL)
				buttons[i]->subMenu->RecalculateVisibles(iCurrentY + yOffset, false);

			// Make sure it's at the right Y position
			buttons[i]->x = 0;
			if (direction)
				buttons[i]->y = (int)((iVisibleButtons - 1) * (flButtonSizeY - 1) - iCurrentY);
			else
				buttons[i]->y = iCurrentY;

			iCurrentY += (flButtonSizeY - 1);
		}
	}

	return iVisibleButtons ? true : false;
}

// CCommandMenu::RecalculatePositions: make sure all submenus can fit on the screen
void CmdMenu::RecalculatePositions(int iYOffset)
{
	int iTop;
	int iAdjust = 0;

	yOffset += iYOffset;

	if (direction)
		iTop = ScreenHeight - (yOffset + size[1]);
	else
		iTop = yOffset;

	if (iTop < 0)
		iTop = 0;

	// Calculate if this is going to fit onscreen, and shuffle it up if it won't
	int iBottom = iTop + size[1];

	if (iBottom > ScreenHeight)
	{
		// Move in increments of button sizes
		while (iAdjust < (iBottom - ScreenHeight))
		{
			iAdjust += flButtonSizeY - 1;
		}

		iTop -= iAdjust;

		// Make sure it doesn't move off the top of the screen (the menu's too big to fit it all)
		if (iTop < 0)
		{
			iAdjust -= (0 - iTop);
			iTop = 0;
		}
	}

	pos[1] = iTop;

	// We need to force all menus below this one to update their positions now, because they
	// might have submenus riding off buttons in this menu that have just shifted.
	for (int i = 0; i < numButtons; i++)
	{
		// CommandButton::UpdateSubMenus
		if (buttons[i]->subMenu)
			buttons[i]->subMenu->RecalculatePositions(iAdjust);
	}
}

// TeamFortressViewport::CreateSubMenu. pButton is NULL for a CUSTOM button SBO doesn't know that is followed by a
// submenu (the SDK code crashed there): the submenu is read and can't be reached.
CmdMenu *CreateSubMenu(CmdButton *pButton, CmdMenu *pParentMenu, int iYOffset)
{
	int iXPos = 0;
	int iYPos = 0;
	int iWide = CMENU_SIZE_X;
	int iTall = 0;
	int iDirection = 0;

	if (pParentMenu)
	{
		iXPos = s_pCurrentMenu->xOffset + (CMENU_SIZE_X - 1);
		iYPos = s_pCurrentMenu->yOffset + iYOffset;
		iDirection = pParentMenu->direction;
	}

	CmdMenu *pMenu = new CmdMenu(pParentMenu, iDirection, iXPos, iYPos, iWide, iTall);
	if (pButton)
		pButton->subMenu = pMenu;
	pMenu->flButtonSizeY = s_pCurrentMenu->flButtonSizeY;
	// a submenu has its parent's style (the spectator Options > Settings submenu: the flat spectator look)
	pMenu->spectCmdMenu = pParentMenu ? pParentMenu->spectCmdMenu : 0;

	_snprintf(pMenu->name, sizeof(pMenu->name) - 1, "%s>%c", pParentMenu ? pParentMenu->name : "",
	    (pButton && pButton->boundKey) ? pButton->boundKey : '-');
	return pMenu;
}

// TeamFortressViewport::CreateCustomButton. SBO: only the Map Briefing and Server Info buttons are left
CmdButton *CreateCustomButton(char *pButtonText, char *pButtonName)
{
	CmdButton *pButton = NULL;

	// Map Briefing
	if (!strcmp(pButtonName, "!MAPBRIEFING"))
	{
		pButton = new CmdButton(pButtonText, 0, BUTTON_SIZE_Y * s_pCurrentMenu->numButtons, CMENU_SIZE_X, BUTTON_SIZE_Y);
		pButton->action = ACTION_TEXTWINDOW;
		pButton->textWindow = MENU_MAPBRIEFING;
	}
	// Server Info
	else if (!strcmp(pButtonName, "!SERVERINFO"))
	{
		pButton = new CmdButton(pButtonText, 0, BUTTON_SIZE_Y * s_pCurrentMenu->numButtons, CMENU_SIZE_X, BUTTON_SIZE_Y);
		pButton->valid = VALID_SERVERINFO;
		pButton->action = ACTION_TEXTWINDOW;
		pButton->textWindow = MENU_INTRO;
	}

	return pButton;
}

CmdMenu *HandleToMenu(sbo::CommandMenuHandle h) { return (CmdMenu *)h; }
CmdButton *HandleToButton(sbo::CommandButtonHandle h) { return (CmdButton *)h; }
} // namespace

namespace sbo
{
void CommandMenu_Reset()
{
	// the menus stay allocated: panels may still hold their handles
	s_iNumMenus = 0;
	s_bInitialized = false;
	s_flMenuOpenTime = 0;
	SetCurrentMenu(NULL);
}

//-----------------------------------------------------------------------------
// TeamFortressViewport::CreateCommandMenu: read the Command Menu structure from the txt file and create the menu.
// Returns Index of menu in s_pMenus
//-----------------------------------------------------------------------------
int CommandMenu_Create(const char *menuFile, int direction, int yOffset, bool flatDesign, float flButtonSizeX,
    float flButtonSizeY, int xOffset)
{
	// Create the root of this new Command Menu
	int newIndex = s_iNumMenus;

	// SBO 2.0: the SDK wrote past its menu array when the menu files held more than MAX_MENUS menus in total
	// (-1: no such menu, CommandMenu_Show / CommandMenu_Update do nothing with it)
	if (newIndex >= MAX_MENUS)
	{
		gEngfuncs.Con_Printf("Too many menus: %s not loaded\n", menuFile);
		return -1;
	}

	// This will be resized once we know how many items are in it
	s_pMenus[newIndex] = new CmdMenu(NULL, direction, xOffset, yOffset, flButtonSizeX, 300);
	s_pMenus[newIndex]->flButtonSizeY = flButtonSizeY;
	s_pMenus[newIndex]->spectCmdMenu = direction;
	strncpy(s_pMenus[newIndex]->name, menuFile, sizeof(s_pMenus[newIndex]->name) - 1);

	s_iNumMenus++;

	// Read Command Menu from the txt file
	char token[1024];
	int iFileLength = 0;
	char *pFileStart = (char *)gEngfuncs.COM_LoadFile((char *)menuFile, 5, &iFileLength);
	char *pfile = pFileStart;
	if (!pfile)
	{
		gEngfuncs.Con_DPrintf("Unable to open %s\n", menuFile);
		SetCurrentMenu(NULL);
		return newIndex;
	}

	// Now start parsing the menu structure
	s_pCurrentMenu = s_pMenus[newIndex];
	char szLastButtonText[32] = "file start";
	pfile = gEngfuncs.COM_ParseFile(pfile, token);
	while ((strlen(token) > 0) && (s_iNumMenus < MAX_MENUS))
	{
		// Keep looping until we hit the end of this menu
		while (token[0] != '}' && (strlen(token) > 0))
		{
			char cText[32] = "";
			char cBoundKey[32] = "";
			char cCustom[32] = "";
			char cCommand[COMMAND_LENGTH] = "";
			char szMap[MAX_MAPNAME_LENGTH] = "";
			int iPlayerClass = 0;
			int iCustom = false;
			int iTeamOnly = -1;
			int iToggle = 0;
			int iButtonY;
			bool bGetExtraToken = true;
			CmdButton *pButton = NULL;

			// We should never be here without a Command Menu
			if (!s_pCurrentMenu)
			{
				gEngfuncs.Con_Printf("Error in %s file after '%s'.\n", menuFile, szLastButtonText);
				s_bInitialized = false;
				return newIndex;
			}

			// token should already be the bound key, or the custom name
			strncpy(cCustom, token, 32);
			cCustom[31] = '\0';

			// See if it's a custom button
			if (!strcmp(cCustom, "CUSTOM"))
			{
				iCustom = true;

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			// See if it's a map
			else if (!strcmp(cCustom, "MAP"))
			{
				// Get the mapname
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
				strncpy(szMap, token, MAX_MAPNAME_LENGTH);
				szMap[MAX_MAPNAME_LENGTH - 1] = '\0';

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			else if (!strncmp(cCustom, "TEAM", 4)) // TEAM1, TEAM2, TEAM3, TEAM4
			{
				// make it a team only button
				iTeamOnly = atoi(cCustom + 4);

				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			else if (!strncmp(cCustom, "TOGGLE", 6))
			{
				iToggle = true;
				// Get the next token
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
			else
			{
				// See if it's a Class
				for (int i = 1; i <= PC_ENGINEER; i++)
				{
					if (!strcmp(token, s_pszClasses[i]))
					{
						// Save it off
						iPlayerClass = i;

						// Get the button text
						pfile = gEngfuncs.COM_ParseFile(pfile, token);
						break;
					}
				}
			}

			// Get the button bound key
			strncpy(cBoundKey, token, 32);
			cBoundKey[31] = '\0';

			// Get the button text
			pfile = gEngfuncs.COM_ParseFile(pfile, token);

			// the menu file's text (the spectator menus' panels look it up in sbo_ui_<language>.txt)
			char cTextToken[32];
			strncpy(cTextToken, token, 32);
			cTextToken[31] = '\0';

			// SBO: menu files hold #tokens, looked up in titles.txt like Valve's (the original copied them raw, so the
			// buttons showed "#Help_Menu"). A token missing from titles.txt is shown without its '#'; plain text as is.
			if (token[0] == '#')
			{
				const char *pszLocalised = gHUD.m_TextMessage.LookupString(token);
				strncpy(cText, pszLocalised != token ? pszLocalised : token + 1, 32);
			}
			else
			{
				strncpy(cText, token, 32);
			}
			cText[31] = '\0';

			// save off the last button text we've come across (for error reporting)
			strcpy(szLastButtonText, cText);

			// Get the button command
			pfile = gEngfuncs.COM_ParseFile(pfile, token);
			strncpy(cCommand, token, COMMAND_LENGTH);
			cCommand[COMMAND_LENGTH - 1] = '\0';

			iButtonY = (BUTTON_SIZE_Y - 1) * s_pCurrentMenu->numButtons;

			// Custom button handling
			if (iCustom)
			{
				pButton = CreateCustomButton(cText, cCommand);

				// SBO 2.0: in a spectator menu a CUSTOM button is a SpectButton like the menu's others (the SDK
				// built it with the standard menu's size; its spectator menus had none)
				if (pButton && direction == 1)
				{
					pButton->wide = (int)flButtonSizeX;
					pButton->tall = (int)flButtonSizeY;
					pButton->style = COMMAND_BUTTON_SPECT;
				}

				// Get the next token to see if we're a menu
				pfile = gEngfuncs.COM_ParseFile(pfile, token);

				if (token[0] == '{')
				{
					strcpy(cCommand, token);
				}
				else
				{
					bGetExtraToken = false;
				}
			}
			else if (szMap[0] != '\0')
			{
				// create a map button (MapButton)
				pButton = new CmdButton(cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY);
				pButton->valid = VALID_MAP;
				_snprintf(pButton->mapName, sizeof(pButton->mapName) - 1, "maps/%s.bsp", szMap);
			}
			else if (iTeamOnly != -1)
			{
				// button that only shows up if the player is on team iTeamOnly (TeamOnlyCommandButton)
				pButton = new CmdButton(cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY);
				pButton->valid = VALID_TEAM;
				pButton->teamNum = iTeamOnly;
				pButton->style = COMMAND_BUTTON_SPECT;
			}
			else if (iToggle && direction == 0)
			{
				// ToggleCommandButton
				pButton = new CmdButton(cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY);
				pButton->cvar = gEngfuncs.pfnGetCvarPointer(cCommand);
				pButton->style = COMMAND_BUTTON_TOGGLE;
			}
			else if (direction == 1)
			{
				// SpectToggleButton / SpectButton (a SpectButton ignores the player class)
				pButton = new CmdButton(cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY);
				if (iToggle)
				{
					pButton->cvar = gEngfuncs.pfnGetCvarPointer(cCommand);
					pButton->style = COMMAND_BUTTON_SPECT_TOGGLE;
				}
				else
					pButton->style = COMMAND_BUTTON_SPECT;
			}
			else
			{
				// normal button (CommandButton( iPlayerClass, ... ))
				pButton = new CmdButton(cText, xOffset, iButtonY, flButtonSizeX, flButtonSizeY);
				pButton->playerClass = iPlayerClass;
			}

			// add the button into the command menu
			if (pButton)
			{
				s_pCurrentMenu->AddButton(pButton);
				pButton->boundKey = cBoundKey[0];
				pButton->parentMenu = s_pCurrentMenu;
				strcpy(pButton->token, cTextToken);
			}

			// Find out if it's a submenu or a button we're dealing with
			if (cCommand[0] == '{')
			{
				if (s_iNumMenus >= MAX_MENUS)
				{
					gEngfuncs.Con_Printf("Too many menus in %s past '%s'\n", menuFile, szLastButtonText);
				}
				else
				{
					// Create the menu
					s_pMenus[s_iNumMenus] = CreateSubMenu(pButton, s_pCurrentMenu, iButtonY);
					s_pCurrentMenu = s_pMenus[s_iNumMenus];
					s_iNumMenus++;
				}
			}
			else if (!iCustom)
			{
				// Create the button and attach it to the current menu
				if (iToggle)
				{
					pButton->action = ACTION_TOGGLE;
					pButton->cvar = gEngfuncs.pfnGetCvarPointer(cCommand);
				}
				else
				{
					pButton->action = ACTION_COMMAND;
					strcpy(pButton->command, cCommand);
				}
			}

			// Get the next token
			if (bGetExtraToken)
			{
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
			}
		}

		// Move back up a menu (a '}' too many: the SDK read through a null menu here; the next button reports
		// the error above)
		if (s_pCurrentMenu)
			s_pCurrentMenu = s_pCurrentMenu->parent;

		pfile = gEngfuncs.COM_ParseFile(pfile, token);
	}

	SetCurrentMenu(NULL);
	gEngfuncs.COM_FreeFile(pFileStart);

	s_bInitialized = true;

	// developer 1: which file the engine delivered (a stale copy earlier in the search path, e.g. in
	// <mod>_addon, shows as another length / other button counts)
	gEngfuncs.Con_DPrintf("%s: %d bytes, %d menu%s:", menuFile, iFileLength, s_iNumMenus - newIndex,
	    (s_iNumMenus - newIndex) == 1 ? "" : "s");
	for (int i = newIndex; i < s_iNumMenus; i++)
		gEngfuncs.Con_DPrintf(" %s %d button%s%s", s_pMenus[i]->name, s_pMenus[i]->numButtons,
		    s_pMenus[i]->numButtons == 1 ? "" : "s", i + 1 < s_iNumMenus ? "," : "");
	gEngfuncs.Con_DPrintf("\n");

	return newIndex;
}

bool CommandMenu_IsInitialized()
{
	return s_bInitialized;
}

// TeamFortressViewport::ShowCommandMenu
void CommandMenu_Show(int menuIndex)
{
	if (!s_bInitialized || !gViewPort)
		return;

	if (menuIndex < 0 || menuIndex >= s_iNumMenus)
		return;

	//Already have a menu open.
	if (gViewPort->m_pCurrentMenu)
		return;

	// is the command menu open?
	if (s_pCurrentMenu == s_pMenus[menuIndex])
	{
		CommandMenu_Hide();
		return;
	}

	// Not visible while in intermission
	if (gHUD.m_iIntermission)
		return;

	// Recalculate visible menus
	CommandMenu_Update(menuIndex);
	gViewPort->HideVGUIMenu();

	SetCurrentMenu(s_pMenus[menuIndex]);
	s_flMenuOpenTime = gHUD.m_flTime;
	gViewPort->UpdateCursorState();

	// get command menu parameters
	for (int i = 2; i < gEngfuncs.Cmd_Argc(); i++)
	{
		const char *param = gEngfuncs.Cmd_Argv(i - 1);
		if (param)
		{
			// (a key after one that closed the menu: the SDK read through the null menu here)
			if (s_pCurrentMenu && s_pCurrentMenu->KeyInput(param[0]))
			{
				// kill the menu open time, since the key input is final
				CommandMenu_Hide();
			}
		}
	}
}

// TeamFortressViewport::InputSignalHideCommandMenu: handles the key input of "-commandmenu"
void CommandMenu_InputSignalHide()
{
	if (!s_bInitialized)
		return;

	// if they've just tapped the command menu key, leave it open
	if ((s_flMenuOpenTime + 0.3) > gHUD.m_flTime)
		return;

	CommandMenu_Hide();
}

// TeamFortressViewport::HideCommandMenu
void CommandMenu_Hide()
{
	if (!s_bInitialized)
		return;

	// the standard menu and the two spectator menus: every root menu
	for (int i = 0; i < s_iNumMenus; i++)
	{
		if (!s_pMenus[i]->parent)
			s_pMenus[i]->ClearButtonsOfArmedState();
	}

	s_flMenuOpenTime = 0.0f;
	SetCurrentMenu(NULL);
	if (gViewPort)
		gViewPort->UpdateCursorState();
}

// TeamFortressViewport::UpdateCommandMenu
void CommandMenu_Update(int menuIndex)
{
	if (menuIndex < 0 || menuIndex >= s_iNumMenus)
		return;

	s_pMenus[menuIndex]->RecalculateVisibles(0, false);
	s_pMenus[menuIndex]->RecalculatePositions(0);

	// an open menu is laid out again
	if (s_pCurrentMenu)
		VGui2_CommandMenu_Changed();
}

bool CommandMenu_KeyInput(int keyNum)
{
	return s_pCurrentMenu && s_pCurrentMenu->KeyInput(keyNum);
}

//-----------------------------------------------------------------------------
// For the panels
//-----------------------------------------------------------------------------
CommandMenuHandle GetOpenCommandMenu()
{
	return (CommandMenuHandle)s_pCurrentMenu;
}

CommandMenuHandle GetCommandMenuParent(CommandMenuHandle menu)
{
	return menu ? (CommandMenuHandle)HandleToMenu(menu)->parent : NULL;
}

void GetCommandMenuBounds(CommandMenuHandle menu, int &x, int &y, int &wide, int &tall)
{
	x = y = wide = tall = 0;
	if (!menu)
		return;

	CmdMenu *pMenu = HandleToMenu(menu);
	x = pMenu->pos[0];
	y = pMenu->pos[1];
	wide = pMenu->size[0];
	tall = pMenu->size[1];
}

bool IsSpectatorStyleCommandMenu(CommandMenuHandle menu)
{
	return menu && HandleToMenu(menu)->spectCmdMenu != 0;
}

const char *GetCommandMenuName(CommandMenuHandle menu)
{
	return menu ? HandleToMenu(menu)->name : "";
}

int GetCommandMenuButtonCount(CommandMenuHandle menu)
{
	return menu ? HandleToMenu(menu)->numButtons : 0;
}

bool GetCommandButtonInfo(CommandMenuHandle menu, int index, CommandButtonInfo &out)
{
	out = CommandButtonInfo();
	CmdMenu *pMenu = HandleToMenu(menu);
	if (!pMenu || index < 0 || index >= pMenu->numButtons)
		return false;

	CmdButton *pButton = pMenu->buttons[index];
	out.handle = (CommandButtonHandle)pButton;
	out.text = pButton->mainText;
	out.token = pButton->token;
	out.boundKey = pButton->boundKey;
	out.visible = pButton->visible;
	out.x = pButton->x;
	out.y = pButton->y;
	out.wide = pButton->wide;
	out.tall = pButton->tall;
	out.subMenu = (CommandMenuHandle)pButton->subMenu;
	out.toggleCvar = (CvarHandle)pButton->cvar;
	out.style = pButton->style;
	return true;
}

bool IsCommandButtonArmed(CommandButtonHandle button)
{
	return button && HandleToButton(button)->armed;
}

void CommandButtonCursorEntered(CommandButtonHandle button)
{
	CmdButton *pButton = HandleToButton(button);
	if (!pButton)
		return;

	// CHandler_CommandButtonHighlight (CommandButton::Init): unarm the menu's other buttons, arm the parents' ones
	pButton->CursorEntered();

	// CMenuHandler_PopupSubMenuInput: a submenu button opens its submenu, the others make their own menu the current
	// one (closing a submenu opened from it)
	CmdMenu *pMenu = pButton->subMenu ? pButton->subMenu : pButton->parentMenu;
	if (pMenu)
		SetCurrentMenu(pMenu);
	pButton->armed = true;
}

void CommandButtonCursorExited(CommandButtonHandle button)
{
	CmdButton *pButton = HandleToButton(button);
	if (pButton)
		pButton->CursorExited();
}

void CommandButtonPressed(CommandButtonHandle button)
{
	CmdButton *pButton = HandleToButton(button);

	// CommandButton::AddSubMenu: a submenu button isn't clickable (setMouseClickEnabled( MOUSE_LEFT, false ))
	if (!pButton || pButton->subMenu)
		return;

	pButton->FireActionSignal();
}
} // namespace sbo
