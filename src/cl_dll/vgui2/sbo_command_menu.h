//========= SBO VGUI2: command menus =========//
//
// Purpose: SBO's command menus: the standard one ("+commandmenu", commandmenu.txt) and the two spectator menus
//          (spectatormenu.txt = the spectator bar's Options button, spectcammenu.txt = its camera mode button).
//
// One implementation in two files:
//   sbo_command_menu.cpp        (SBO world)  the menus themselves: the menu file parser, the buttons and their
//                                            state (visible, armed, bounds), layout, key input, actions
//   sbo_command_menu_panel.cpp  (SDK world)  the VGUI2 panels that show the open menus and hand the mouse over
//
// The behaviour is the HL SDK's VGUI1 command menu (TeamFortressViewport::CreateCommandMenu, CCommandMenu,
// CommandButton and their handlers, as SBO 1.38 shipped them), which this replaces: same file syntax, same
// visibility rules, same positions (to the pixel), same keys, same commands. docs/dev/VGUI2_MIGRATION.md, "Command
// menus".
//
// Plain C++ only: safe to include from both header worlds.
//=================================================================//

#ifndef SBO_VGUI2_SBO_COMMAND_MENU_H
#define SBO_VGUI2_SBO_COMMAND_MENU_H

namespace sbo
{
typedef struct CvarHandle_s *CvarHandle;             //!< cvar_t * (sbo_bridge.h)
typedef struct CommandMenu_s *CommandMenuHandle;     //!< a menu (a root menu or a submenu)
typedef struct CommandButton_s *CommandButtonHandle; //!< a button of a menu

//-----------------------------------------------------------------------------
// The menus (TeamFortressViewport's command menu functions)
//-----------------------------------------------------------------------------
//! Forgets every menu (a new client viewport is about to create them again).
void CommandMenu_Reset();
//! TeamFortressViewport::CreateCommandMenu: reads the menu structure from a menu file and returns the index of the
//! new root menu. direction 0: the menu opens downwards from yOffset (standard menu); 1: upwards from yOffset above
//! the bottom of the screen (spectator menus: their look, too).
int CommandMenu_Create(const char *menuFile, int direction, int yOffset, bool flatDesign, float flButtonSizeX,
    float flButtonSizeY, int xOffset);
//! True once the last menu file read was read without an error (TeamFortressViewport::m_iInitialized): the command
//! menus work. (A file that doesn't exist doesn't change it.)
bool CommandMenu_IsInitialized();
//! ShowCommandMenu: opens the root menu `menuIndex`, or closes it when it is the open menu. Nothing while a
//! menu-stack menu is up or during the intermission. Reads the command's arguments as keys ("+commandmenu 4 2").
void CommandMenu_Show(int menuIndex);
//! HideCommandMenu: closes whatever is open and unarms every button.
void CommandMenu_Hide();
//! InputSignalHideCommandMenu ("-commandmenu"): closes unless the menu was opened less than 0.3 s ago (a tap).
void CommandMenu_InputSignalHide();
//! UpdateCommandMenu: recalculates the visible buttons and the positions of a root menu and its submenus.
void CommandMenu_Update(int menuIndex);
//! CCommandMenu::KeyInput on the open menu: the button bound to the key. A submenu button opens its submenu
//! (returns false); another button runs its action (returns true: the caller closes the menu). False without an
//! open menu or a button for the key.
bool CommandMenu_KeyInput(int keyNum);

//-----------------------------------------------------------------------------
// What the panels show (sbo_command_menu_panel.cpp)
//-----------------------------------------------------------------------------
//! How a button is drawn: the VGUI1 button class CreateCommandMenu made for it.
enum CommandButtonStyle
{
	COMMAND_BUTTON_NORMAL = 0,   //!< CommandButton: text scheme colours, SBO's green border
	COMMAND_BUTTON_TOGGLE,       //!< ToggleCommandButton: the same with a "checked" / "unchecked" image
	COMMAND_BUTTON_SPECT,        //!< SpectButton, TeamOnlyCommandButton: the spectator menus' look
	COMMAND_BUTTON_SPECT_TOGGLE, //!< SpectToggleButton: the same with a "checked" image
};

struct CommandButtonInfo
{
	CommandButtonHandle handle = nullptr;
	const char *text = "";  //!< the button's text (menu file text, #tokens localized from titles.txt), max 31 chars
	const char *token = ""; //!< the menu file's text as written, e.g. "#SBO_SpecMenu_OK" (max 31 chars)
	char boundKey = 0;      //!< key shown in front ("  <key>  text"); 0 = none, (char)255 = no prefix
	bool visible = false;   //!< RecalculateVisibles' result
	int x = 0, y = 0, wide = 0, tall = 0; //!< in the menu
	CommandMenuHandle subMenu = nullptr;  //!< opens on hover (the button itself isn't clickable)
	CvarHandle toggleCvar = nullptr;      //!< TOGGLE button: the cvar (checked while non-zero)
	int style = COMMAND_BUTTON_NORMAL;    //!< CommandButtonStyle
};

//! The open menu, nullptr if none. Its parents (GetCommandMenuParent) are shown too.
CommandMenuHandle GetOpenCommandMenu();
CommandMenuHandle GetCommandMenuParent(CommandMenuHandle menu);
//! Position (screen) and size as laid out by CommandMenu_Update.
void GetCommandMenuBounds(CommandMenuHandle menu, int &x, int &y, int &wide, int &tall);
//! The spectator look (black, VGUI1 alpha 64) instead of the text scheme's background: the spectator menus.
bool IsSpectatorStyleCommandMenu(CommandMenuHandle menu);
//! For vgui2_dumptree: the menu file's name, a submenu with the keys that lead to it ("spectatormenu.txt>4").
const char *GetCommandMenuName(CommandMenuHandle menu);
int GetCommandMenuButtonCount(CommandMenuHandle menu);
bool GetCommandButtonInfo(CommandMenuHandle menu, int index, CommandButtonInfo &out);
//! The button's armed (highlighted) state.
bool IsCommandButtonArmed(CommandButtonHandle button);
//! The cursor entered a button: arms it and its parents' buttons, unarms the menu's other buttons, opens its
//! submenu or makes its own menu the open one (closing deeper ones).
void CommandButtonCursorEntered(CommandButtonHandle button);
//! The cursor left it: unarmed, unless it has a submenu.
void CommandButtonCursorExited(CommandButtonHandle button);
//! A click: the button's action (command, cvar toggle, text window); nothing for a submenu button.
void CommandButtonPressed(CommandButtonHandle button);
} // namespace sbo

//! SBO -> panels: the open menu, a menu's layout or the spectator bars changed; show what is open now.
//! (sbo_command_menu_panel.cpp; does nothing without VGUI2.)
void VGui2_CommandMenu_Changed();

#endif // SBO_VGUI2_SBO_COMMAND_MENU_H
