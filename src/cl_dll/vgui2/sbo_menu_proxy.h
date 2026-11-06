//========= SBO VGUI2: menu-stack entries =========//
//
// Purpose: What SBO's TeamFortressViewport puts on its menu stack for a menu (docs/dev/VGUI2_MIGRATION.md).
//
// The viewport's menu logic (queueing in ShowVGUIMenu, HideTopMenu, the per-menu-ID keys in KeyInput,
// g_iVisibleMouse / UpdateCursorState, AllowedToPrintText) works on CViewportMenu objects
// (viewport.h); these are the ones of the VGUI2 menus: their Open() / Close() show / hide the
// VGUI2 panel (vgui2_api.h). (Until SBO 2.0's VGUI1 removal they were paint-less VGUI1 panels standing in for the
// VGUI1 menus, hence "proxy".)
//
// SBO world: implemented in sbo_menu_bridge.cpp. Plain declarations only.
//=================================================================//

#ifndef SBO_VGUI2_SBO_MENU_PROXY_H
#define SBO_VGUI2_SBO_MENU_PROXY_H

class CViewportMenu;

//! TeamFortressViewport::CreateTextWindow: a new menu for one text window (copies title and text, so the caller may
//! free them right away). iShadeFullscreen: darken the whole screen, else only the window. szHtmlFile: the map
//! briefing's .htm (shown as a web page when HTML is on) or NULL; bMOTD: the text is the MOTD (docs/HTML_MOTD.md).
CViewportMenu *SboVGui2_CreateTextWindowProxy(const char *szText, const char *szTitle, int iShadeFullscreen,
    const char *szHtmlFile, bool bMOTD);

//! TeamFortressViewport::ShowGmMenu: the GM menu (one per viewport).
CViewportMenu *SboVGui2_GetGmMenuProxy();

//! TeamFortressViewport::ShowTeamMenu: the team menu (one per viewport); forwards SlotInput.
CViewportMenu *SboVGui2_GetTeamMenuProxy();

//! TeamFortressViewport::ShowSyndBrief: the briefing (equipment) menu (one per viewport).
CViewportMenu *SboVGui2_GetBriefMenuProxy();

//! TeamFortressViewport::ShowVGUIMenu( MENU_SERVERMENU ): a plugin's server menu (one per viewport;
//! sbo_server_menu_bridge.cpp, docs/SERVER_MENUS.md)
CViewportMenu *SboVGui2_GetServerMenuProxy();

#endif // SBO_VGUI2_SBO_MENU_PROXY_H
