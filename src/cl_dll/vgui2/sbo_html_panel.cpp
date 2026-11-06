//========= SBO: HTML MOTD and map briefings, web page panel =========//
//
// Purpose: see sbo_html_panel.h.
//=================================================================//

#include <string.h>

#include <tier1/strtools.h>
#include <tier2/tier2.h>
#include <FileSystem.h>
#include <vgui_controls/PHandle.h>

#include "client_steam_context.h"
#include "sbo_cvar.h"
#include "sbo_html.h"
#include "sbo_html_panel.h"

namespace
{
// Counter-Strike's client cvar: 1 = MOTDs (and SBO's map briefings) as plain text only
CSboCvar cl_disablehtmlmotd("cl_disablehtmlmotd", "0");
bool s_bCvarRegistered = false;

// the client's one panel (sbo_html_panel.h); deleted with the window that holds it at shutdown
vgui2::DHANDLE<CSboHtmlPanel> s_hPanel;

// BHL CClientMOTD's file name: written in the mod folder for an HTML MOTD (the briefings are opened where they are)
const char TEMP_FILE[] = "motd_temp.html";

// "file:///" + the local path (BHL); a path that starts with '/' (Linux) without its own first '/'
void MakeFileURL(const char *localPath, char *out, size_t outSize)
{
	if (localPath[0] == '/')
		localPath++;
	V_snprintf(out, (int)outSize, "file:///%s", localPath);
}
} // namespace

void SboHtml_Init()
{
	if (s_bCvarRegistered)
		return;
	s_bCvarRegistered = true;
	cl_disablehtmlmotd.Register();
}

bool SboHtml_Enabled()
{
	if (s_bCvarRegistered && cl_disablehtmlmotd.GetBool())
		return false;
	return SteamAPI_IsAvailable();
}

CSboHtmlPanel *CSboHtmlPanel::Acquire(vgui2::Panel *pParent)
{
	if (!SboHtml_Enabled())
		return nullptr;

	CSboHtmlPanel *pPanel = s_hPanel.Get();
	if (!pPanel)
	{
		// vgui2::HTML gets the HTML surface through its own CSteamAPIContext, which needs SteamAPI_InitSafe
		// (idempotent)
		ClientSteamContext().Activate();
		pPanel = new CSboHtmlPanel(pParent, "Html");
		s_hPanel = pPanel;
	}
	else if (pPanel->GetParent() != pParent)
	{
		// the previous window's page goes; the new window loads its own (nothing of it carries over)
		pPanel->Clear();
		pPanel->SetParent(pParent);
	}
	return pPanel;
}

CSboHtmlPanel *CSboHtmlPanel::GetIfOwnedBy(vgui2::Panel *pParent)
{
	CSboHtmlPanel *pPanel = s_hPanel.Get();
	return (pPanel && pPanel->GetParent() == pParent) ? pPanel : nullptr;
}

void CSboHtmlPanel::Release(vgui2::Panel *pParent)
{
	CSboHtmlPanel *pPanel = GetIfOwnedBy(pParent);
	if (!pPanel)
		return;
	pPanel->Clear();
	pPanel->SetVisible(false);
}

CSboHtmlPanel::CSboHtmlPanel(vgui2::Panel *pParent, const char *pName)
    : BaseClass(pParent, pName)
{
}

CSboHtmlPanel::~CSboHtmlPanel()
{
	RemoveTempFile();
}

bool CSboHtmlPanel::OpenMOTD(const char *text)
{
	if (!text)
		return false;

	if (sbohtml::IsURL(text))
	{
		char url[1024];
		if (!sbohtml::CopyURL(text, url, sizeof(url)))
			return false;
		RemoveTempFile();
		OpenURL(url, nullptr);
		return true;
	}

	if (!sbohtml::IsHtml(text))
		return false;

	RemoveTempFile();

	// BHL: written as it came, opened from the mod folder ("GAMECONFIG": where config.cfg is written)
	FileHandle_t hFile = g_pFullFileSystem->Open(TEMP_FILE, "w", "GAMECONFIG");
	if (hFile == FILESYSTEM_INVALID_HANDLE)
		return false;
	g_pFullFileSystem->Write(text, (int)strlen(text), hFile);
	g_pFullFileSystem->Close(hFile);
	m_bFileWritten = true;

	return OpenLocalFile(TEMP_FILE);
}

bool CSboHtmlPanel::OpenGameFile(const char *path)
{
	if (!path || !path[0] || !g_pFullFileSystem->FileExists(path))
		return false;
	RemoveTempFile();
	return OpenLocalFile(path);
}

bool CSboHtmlPanel::OpenLocalFile(const char *path)
{
	char localPath[1024];
	if (!g_pFullFileSystem->GetLocalPath(path, localPath, sizeof(localPath)) || !localPath[0])
		return false;

	char url[1024 + 16];
	MakeFileURL(localPath, url, sizeof(url));
	OpenURL(url, nullptr);
	return true;
}

void CSboHtmlPanel::Clear()
{
	OpenURL("", nullptr);
	RemoveTempFile();
}

void CSboHtmlPanel::RemoveTempFile()
{
	if (!m_bFileWritten)
		return;
	m_bFileWritten = false;
	if (g_pFullFileSystem->FileExists(TEMP_FILE))
		g_pFullFileSystem->RemoveFile(TEMP_FILE, "GAMECONFIG");
}
