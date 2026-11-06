//========= SBO 2.0: server-driven menus =========//
//
// Purpose: a plugin's menu in the new in-game look, described by the server ("ServerMenu" message,
//          src/game_shared/server_menu_shared.h; docs/SERVER_MENUS.md), shown only while "New in-game menus" is on
//          (cl_new_ingame_menus 1). Off, the client tells the server so, and the plugin shows its text menu.
//
//          sbo_server_menu_bridge.cpp (SBO world): the message, the store (sbo_server_menu_proto.h), the hello, the
//          menu-stack entry (MENU_SERVERMENU: the cursor, "other menus win"), the open / close rules.
//          sbo_server_menu.cpp (SDK world): the window: a list or a card grid, tabs, search, header lines, badges,
//          buttons, the side layout and the model preview (server_menu_preview.h).
//
// Plain declarations (both header worlds include it).
//=================================================================//

#ifndef SBO_VGUI2_SBO_SERVER_MENU_H
#define SBO_VGUI2_SBO_SERVER_MENU_H

namespace servermenu
{
class Store;
}

// SBO world (sbo_server_menu_bridge.cpp)
//! From VGui2 initialization: hooks "ServerMenu"
void ServerMenu_Init();
//! InitHUD (a new map, a reconnect, a fullupdate): every menu is dropped, the window closed
void ServerMenu_InitHUD();
//! The menus this server defined for us
const servermenu::Store &ServerMenu_Store();
//! The menu stack's entry was opened / closed (sbo_menu_bridge.cpp's proxy)
void ServerMenu_ProxyOpen();
void ServerMenu_ProxyClose();
//! The window: the player closed it (Esc, Close)
void ServerMenu_PlayerClosed();
//! The window: a "servermenu ..." command for the shown menu ("pick 3", "back", "button 1 3")
void ServerMenu_Send(const char *what);
//! The window is up: the HUD and the crosshair are not drawn (hud_redraw.cpp, worldscreen.cpp)
bool ServerMenu_HidesHud();
//! cl_new_ingame_menus changed (VGui2_ServerMenu_Frame): a new hello; off closes the window
void ServerMenu_EnabledChanged();
//! The client time the store's cooldowns are counted in (seconds)
float ServerMenu_Now();
//! The HUD strip (HUDBAR) may be drawn now: the HUD's rules, no scoreboard, no menu up
bool ServerMenu_HudBarShouldDraw();

// SDK world (sbo_server_menu.cpp)
//! Shows a complete menu (opens the window or replaces its menu in place)
void VGui2_ServerMenu_Show(int menu, int tab, int item);
void VGui2_ServerMenu_Hide();
//! cl_new_ingame_menus
bool VGui2_ServerMenu_Enabled();
//! A definition or live update of this menu arrived
void VGui2_ServerMenu_Changed(int menu);
//! Keys 1-9 while the window is up (the menu stack entry's SlotInput, 0-based): the n-th item shown. True when used.
bool VGui2_ServerMenu_SlotInput(int iSlot);
//! Once per frame (CGameUIViewport::Frame): watches cl_new_ingame_menus
void VGui2_ServerMenu_Frame();
//! The HUD strip changed (ServerMenu_Store().Hud()): its panel exists from now on
void VGui2_HudBar_Changed();

#endif // SBO_VGUI2_SBO_SERVER_MENU_H
