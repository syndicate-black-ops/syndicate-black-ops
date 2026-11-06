//========= SBO 2.0: server-driven menus, SBO side =========//
//
// Purpose: see sbo_server_menu.h. The "ServerMenu" message into the store, the answers to the server, the menu-stack
//          entry and the open / close rules (docs/SERVER_MENUS.md):
//          * HELLO is answered with "servermenu hello <version> <on>" (on: cl_new_ingame_menus), and again when the
//            switch changes; only then does the plugin send menus instead of its text menu
//          * OPEN is ignored while another menu is up (team, loadout, MOTD, ...: they always win), in demo playback,
//            with the switch off, past 10 OPEN / CLOSE a second, and once within a second of the player closing that
//            same menu (Esc always gets the player out)
//          * when the window closes for any reason but the server's CLOSE, the server hears "servermenu close"
//
// Compiled with SBO's normal client include paths (SDK 2.3). Must not include any Source SDK header.
//=================================================================//

// the store's header first: its standard headers before cl_util.h's min / max macros
#include "sbo_server_menu_proto.h"

#include "hud.h"
#include "cl_util.h"
#include "demo_api.h"
#include "viewport.h"
#include "sbo_bridge.h" // sbo::HudMenuShouldDraw

#include <stdio.h>
#include <string.h>

#include "sbo_server_menu.h"
#include "../server_menu_preview.h"

namespace
{
servermenu::Store s_Store;

int s_iRequestMenu = -1, s_iRequestTab = 0, s_iRequestItem = 0; //!< what the next ProxyOpen shows
int s_iShownMenu = -1, s_iShownSerial = 0;
bool s_bHelloSeen = false;  //!< the server spoke HELLO since the last InitHUD
bool s_bServerClosing = false;
float s_flClosedAt[servermenu::MAX_SERVER_MENUS];
bool s_bReopenIgnored[servermenu::MAX_SERVER_MENUS];
float s_flFloodStart = -100.0f;
int s_iFloodCount = 0;

float Now()
{
	return gEngfuncs.GetClientTime();
}

bool Playback()
{
	return gEngfuncs.pDemoAPI && gEngfuncs.pDemoAPI->IsPlayingback();
}

void SendHello()
{
	if (Playback())
		return;
	char cmd[64];
	snprintf(cmd, sizeof(cmd), SERVERMENU_CMD_NAME " hello %d %d\n", servermenu::PROTOCOL_VERSION,
		VGui2_ServerMenu_Enabled() ? 1 : 0);
	gEngfuncs.pfnClientCmd(cmd);
}

//! More than 10 OPEN / CLOSE a second are ignored
bool Flooded()
{
	const float now = Now();
	if (now - s_flFloodStart > 1.0f || now < s_flFloodStart)
	{
		s_flFloodStart = now;
		s_iFloodCount = 0;
	}
	return ++s_iFloodCount > 10;
}

bool ServerMenuOnTop()
{
	return gViewPort && gViewPort->m_pCurrentMenu && gViewPort->m_pCurrentMenu->GetMenuID() == MENU_SERVERMENU;
}

void CloseShown(bool byServer)
{
	if (!ServerMenuOnTop())
		return;
	s_bServerClosing = byServer;
	gViewPort->HideTopMenu();
	s_bServerClosing = false;
}

void Open(int menu, int tab, int item)
{
	if (!gViewPort || Playback() || !VGui2_ServerMenu_Enabled() || Flooded())
		return;
	const servermenu::Menu *m = s_Store.Get(menu);
	if (!m)
		return;
	// the player closed this menu just now: a plugin that reopens its menu to keep it up waits a second
	if (s_flClosedAt[menu] > 0.0f && Now() - s_flClosedAt[menu] < 1.0f && Now() >= s_flClosedAt[menu] &&
	    !s_bReopenIgnored[menu])
	{
		s_bReopenIgnored[menu] = true;
		return;
	}
	if (ServerMenuOnTop())
	{
		// another of the server's menus (or the same one again): replaced in place
		s_iShownMenu = menu;
		s_iShownSerial = m->serial;
		VGui2_ServerMenu_Show(menu, tab, item);
		return;
	}
	if (gViewPort->m_pCurrentMenu)
		return; // the game's own menus win
	s_iRequestMenu = menu;
	s_iRequestTab = tab;
	s_iRequestItem = item;
	gViewPort->ShowVGUIMenu(MENU_SERVERMENU);
	s_iRequestMenu = -1;
}

int MsgFunc_ServerMenu(const char *pszName, int iSize, void *pbuf)
{
	std::vector<servermenu::Event> events;
	s_Store.Parse((const unsigned char *)pbuf, iSize > 0 ? (size_t)iSize : 0, events, Now());
	if (s_Store.TakeErrorToReport())
		gEngfuncs.Con_Printf("%s", s_Store.LastError().c_str());
	for (const servermenu::Event &e : events)
	{
		switch (e.kind)
		{
		case servermenu::Event::HELLO:
			s_bHelloSeen = true;
			SendHello();
			break;
		case servermenu::Event::DEFINED:
			// the open menu defined again (a vote's runoff, smenu_commit): the window shows the new one in place, and
			// the answers carry its new serial (the API refuses the old one)
			if (e.a == s_iShownMenu)
			{
				if (const servermenu::Menu *m = s_Store.Get(e.a))
					s_iShownSerial = m->serial;
			}
			VGui2_ServerMenu_Changed(e.a);
			break;
		case servermenu::Event::CHANGED:
			VGui2_ServerMenu_Changed(e.a);
			break;
		case servermenu::Event::OPEN:
			Open(e.a, e.b, e.c);
			break;
		case servermenu::Event::HUDBAR:
			VGui2_HudBar_Changed();
			break;
		case servermenu::Event::CLOSE:
			if (s_iShownMenu >= 0 && (e.a == servermenu::CLOSE_ANY || e.a == s_iShownMenu) && !Flooded())
				CloseShown(true);
			break;
		}
	}
	return 1;
}
} // namespace

void ServerMenu_Init()
{
	gEngfuncs.pfnHookUserMsg((char *)SERVERMENU_MSG_NAME, MsgFunc_ServerMenu);
}

void ServerMenu_InitHUD()
{
	CloseShown(true);
	s_Store.Reset();
	s_bHelloSeen = false;
	s_iShownMenu = -1;
	for (int i = 0; i < servermenu::MAX_SERVER_MENUS; i++)
	{
		s_flClosedAt[i] = 0.0f;
		s_bReopenIgnored[i] = false;
	}
	ServerMenuPreview_Reset();
}

const servermenu::Store &ServerMenu_Store()
{
	return s_Store;
}

void ServerMenu_ProxyOpen()
{
	const servermenu::Menu *m = s_Store.Get(s_iRequestMenu);
	if (!m)
		return;
	s_iShownMenu = s_iRequestMenu;
	s_iShownSerial = m->serial;
	VGui2_ServerMenu_Show(s_iRequestMenu, s_iRequestTab, s_iRequestItem);
}

void ServerMenu_ProxyClose()
{
	VGui2_ServerMenu_Hide();
	if (s_iShownMenu >= 0 && !s_bServerClosing)
	{
		char cmd[64];
		snprintf(cmd, sizeof(cmd), SERVERMENU_CMD_NAME " close %d %d\n", s_iShownMenu, s_iShownSerial);
		gEngfuncs.pfnClientCmd(cmd);
		s_flClosedAt[s_iShownMenu] = Now();
		s_bReopenIgnored[s_iShownMenu] = false;
	}
	s_iShownMenu = -1;
}

void ServerMenu_PlayerClosed()
{
	CloseShown(false);
}

void ServerMenu_Send(const char *what)
{
	if (s_iShownMenu < 0 || !what)
		return;
	char cmd[96 + servermenu::LEN_FIELD_TEXT];
	// "pick 3" -> "servermenu pick <menu> <serial> 3"; "text 0 \"...\"": the text cleaned by the window
	// (CleanFieldText: no quotes, semicolons or control characters, so it stays one argument of this one command)
	const char *space = strchr(what, ' ');
	const int verb = space ? (int)(space - what) : (int)strlen(what);
	snprintf(cmd, sizeof(cmd), SERVERMENU_CMD_NAME " %.*s %d %d%s\n", verb, what, s_iShownMenu, s_iShownSerial,
		space ? space : "");
	gEngfuncs.pfnClientCmd(cmd);
}

void ServerMenu_EnabledChanged()
{
	if (!VGui2_ServerMenu_Enabled())
		CloseShown(false);
	if (s_bHelloSeen)
		SendHello();
}

bool ServerMenu_HudBarShouldDraw()
{
	// the HUD's own rules (hud_draw, HIDEHUD_ALL, intermission, connected, no full-screen team menu or MOTD), the
	// scoreboard, and no menu up (the game's menus, a centered server menu)
	if (!sbo::HudMenuShouldDraw())
		return false;
	// classic menus (New in-game menus off): never the strip, whatever a server sends (its plugin's old HUD text
	// shows instead; the hello tells the server the switch, again whenever it changes)
	if (!VGui2_ServerMenu_Enabled())
		return false;
	// a docked server menu leaves the strip up (it is drawn left of the window); a centered one hides it
	if (ServerMenu_HidesHud())
	{
		const servermenu::Menu *m = s_Store.Get(s_iShownMenu);
		return m && (m->flags & servermenu::MENU_SIDE);
	}
	return !(gViewPort && gViewPort->m_pCurrentMenu);
}

float ServerMenu_Now()
{
	return Now();
}

bool ServerMenu_HidesHud()
{
	return s_iShownMenu >= 0;
}
