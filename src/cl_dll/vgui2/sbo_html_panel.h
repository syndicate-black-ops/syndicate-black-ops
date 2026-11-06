//========= SBO: HTML MOTD and map briefings, web page panel =========//
//
// Purpose: the web page part of the MOTD window (sbo_text_window.cpp) and the map readme (sbo_team_menu.cpp),
//          docs/HTML_MOTD.md. BugfixedHL-Rebased's src/game/client/vgui/client_motd.cpp (GPLv3): a vgui2::HTML
//          (Steam's HTML surface, ISteamHTMLSurface: the Steam client's Chromium) next to the RichText; an HTML MOTD
//          is written to a file in the mod folder and opened as file:///<path>, a URL is opened as it is. The
//          vgui2::HTML defaults are kept (links, pop-ups, navigation, context menu), as in BHL.
//
//          Shown only when HTML is on (SboHtml_Enabled: cl_disablehtmlmotd 0, Counter-Strike's client cvar, and
//          the Steam API is there); otherwise the windows show their text as before.
//
//          One panel (one browser) for the whole client, moved into the window that shows a page (Acquire), as BHL
//          has one vgui2::HTML (the MOTD's). The SDK's vgui2::HTML takes every browser's Steam callbacks (page
//          paints, cursor, requests, scroll bars) without checking their browser handle, so with two panels each
//          drew the other's page as well: the spectator "Map Briefing" showed the map readme's hidden page (its
//          ticked objectives) while its clicks went to its own browser, which nobody saw.
//
// SDK world (hl1_source_sdk headers).
//=================================================================//

#ifndef SBO_VGUI2_SBO_HTML_PANEL_H
#define SBO_VGUI2_SBO_HTML_PANEL_H

#include <vgui_controls/HTML.h>

//! HUD_Init (VGui2_Menus_Init): registers cl_disablehtmlmotd.
void SboHtml_Init();

//! True if a web page can be shown: cl_disablehtmlmotd is 0 and the Steam API is available.
bool SboHtml_Enabled();

class CSboHtmlPanel : public vgui2::HTML
{
	DECLARE_CLASS_SIMPLE(CSboHtmlPanel, vgui2::HTML);

public:
	virtual ~CSboHtmlPanel();

	//! The client's one panel, moved into pParent (a window about to show a page; it lays the panel out). Created
	//! the first time if HTML is on (SboHtml_Enabled; starts the Steam context first). Null if HTML is off.
	static CSboHtmlPanel *Acquire(vgui2::Panel *pParent);
	//! The panel if pParent holds it now, else null (another window took it, or there is none).
	static CSboHtmlPanel *GetIfOwnedBy(vgui2::Panel *pParent);
	//! pParent closes: if it holds the panel, the page is emptied and the panel hidden.
	static void Release(vgui2::Panel *pParent);

	//! The MOTD text: an http(s) URL is loaded, HTML is written to the temp file and loaded from there. False if
	//! it is neither (or the file can't be written): show it as text.
	bool OpenMOTD(const char *text);
	//! A file of the game's file system (e.g. "maps/<map>.htm"), loaded from where it is. False if not found.
	bool OpenGameFile(const char *path);
	//! Empty page, temp file removed (BHL CClientMOTD::Reset).
	void Clear();

private:
	CSboHtmlPanel(vgui2::Panel *pParent, const char *pName);

	void RemoveTempFile();
	//! file:///<local path of path>; false if the file system has no local path for it.
	bool OpenLocalFile(const char *path);

	bool m_bFileWritten = false;
};

#endif // SBO_VGUI2_SBO_HTML_PANEL_H
