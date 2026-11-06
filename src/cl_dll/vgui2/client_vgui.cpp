//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: IClientVGUI bootstrap. Ported from BugfixedHL-Rebased
//          src/game/client/client_vgui.cpp and cdll_int.cpp (ClientFactory).
//
// SDK world: compiled with hl1_source_sdk include paths. Talks to SBO only
// through sbo_bridge.h.
//=================================================================//

#include <stdio.h>
#include <string.h>

#include <tier0/dbg.h>
#include <tier1/interface.h>
#include <tier1/tier1.h>
#include <tier2/tier2.h>
#include <IEngineVGui.h>
#include <FileSystem.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>

#include "IGameClientExports.h"
#include "client_vgui.h"
#include "client_viewport.h"
#include "sbo_loading_screen.h"
#include "sbo_bridge.h"
#include "vgui2_api.h"

EXPOSE_SINGLE_INTERFACE(CClientVGUI, IClientVGUI, ICLIENTVGUI_NAME);

//-----------------------------------------------------------------------------
// The engine asks the client library for its interface factory through this
// export (BHL: cdll_int.cpp ClientFactory, also slot 43 of cldll_func_t when the
// client uses the "F" export). SBO's client exports its functions by name, so this
// is exported by name too. tier1/interface.cpp additionally exports CreateInterface.
//-----------------------------------------------------------------------------
DLL_EXPORT void *ClientFactory()
{
	return (void *)(Sys_GetFactoryThis());
}

//-----------------------------------------------------------------------------
// Purpose: Exports functions that are used by the gameUI for UI dialogs
// (main menu "Mute players"). Valve halflife / BugfixedHL-Rebased cdll_int.cpp
// CClientExports. GameUI looks GameClientExports001 up in the factories the
// engine hands it, which include this library's ClientFactory (above).
// It lives in the SDK world: the registrar must be tier1's InterfaceReg (the one
// Sys_GetFactoryThis/CreateInterface walks), not SBO's common/interface.h one.
// The player list itself comes from the engine (names, bots); the client only
// answers the mute state, shared with the scoreboard's mute (CVoiceStatus).
//-----------------------------------------------------------------------------
class CClientExports : public IGameClientExports
{
public:
	// returns the name of the server the user is connected to, if any
	virtual const char *GetServerHostName()
	{
		return sbo::GetServerName();
	}

	// ingame voice manipulation
	virtual bool IsPlayerGameVoiceMuted(int playerIndex)
	{
		return sbo::IsPlayerBlocked(playerIndex);
	}

	virtual void MutePlayerGameVoice(int playerIndex)
	{
		sbo::SetPlayerBlocked(playerIndex, true);
	}

	virtual void UnmutePlayerGameVoice(int playerIndex)
	{
		sbo::SetPlayerBlocked(playerIndex, false);
	}
};

EXPOSE_SINGLE_INTERFACE(CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION);

namespace vgui2
{
// vgui_controls asks the client for its default scheme; 0 = the engine's default.
HScheme VGui_GetDefaultScheme()
{
	return 0;
}
}

static bool s_bVGui2Available = false;
static void DumpTree_f();

void CClientVGUI::Initialize(CreateInterfaceFn *pFactories, int iNumFactories)
{
	ConnectTier1Libraries(pFactories, iNumFactories);
	ConnectTier2Libraries(pFactories, iNumFactories);

	if (!vgui2::VGui_InitInterfacesList("CLIENT", pFactories, iNumFactories))
	{
		Error("Failed to intialize VGUI2\n");
		Assert(false);
		return;
	}

	// Override proportional scale
	// The Anniversary Update changed the base resolution from 640x480 to 1280x720.
	// This causes all old UI to be down-scaled.
	vgui2::VGui_SetProportionalBaseCallback(&GetProportionalBase);

	new CClientViewport();
	new CGameUIViewport();

	s_bVGui2Available = true;
}

void CClientVGUI::Start()
{
	// BHL creates its VGUI2 viewport panels here. SBO's only VGUI2 panel (the chat)
	// is created from HUD_Init -> CHud::Init -> SboHud_InitChat, like BHL's CHudChat.
	// The joining screen exists from here, before the first client frame: a launch straight into a server
	// (+connect) opens GameUI's loading window before HUD_Frame ever runs.
	VGui2_LoadingScreen_Frame();
}

void CClientVGUI::SetParent(vgui2::VPANEL parent)
{
}

int CClientVGUI::UseVGUI1()
{
	// As BHL: every panel of the client is a VGUI2 one. (Until the VGUI1 removal SBO returned 1 and parented its
	// TeamFortressViewport to the engine's VGUI1 root.)
	return 0;
}

void CClientVGUI::HideScoreBoard()
{
	// BHL: g_pViewport->HideScoreBoard() (kept during intermission)
	if (!sbo::IsIntermission())
		VGui2_Score_Hide();
}

void CClientVGUI::HideAllVGUIMenu()
{
}

void CClientVGUI::ActivateClientUI()
{
	if (g_pViewport)
		g_pViewport->ActivateClientUI();
}

void CClientVGUI::HideClientUI()
{
	if (g_pViewport)
		g_pViewport->HideClientUI();
}

void CClientVGUI::Shutdown()
{
	// Warning! Only called for CS & CZ
	// Do not use!
}

void CClientVGUI::GetProportionalBase(int &wide, int &tall)
{
	// Pre-HL25 values
	wide = 640;
	tall = 480;
}

//-----------------------------------------------------------------------------
// SBO -> VGUI2 API (vgui2_api.h)
//-----------------------------------------------------------------------------
bool VGui2_IsAvailable()
{
	return s_bVGui2Available;
}

bool VGui2_GameDirHasFile(const char *file)
{
	if (!g_pFullFileSystem || !file || !file[0])
		return true;

	// the file as the engine finds it on its search paths: the game folder first, then the fallbacks (valve)
	char path[1024];
	if (!g_pFullFileSystem->GetLocalPath(file, path, sizeof(path)))
		return false; // nowhere

	// ".../<gamedir>/<file>" (either slash, any case: Windows paths)
	const char *gamedir = sbo::GetGameDirectory();
	if (!gamedir || !gamedir[0])
		return true;
	char want[300];
	snprintf(want, sizeof(want), "/%s/%s", gamedir, file);
	const size_t lenPath = strlen(path), lenWant = strlen(want);
	if (lenPath < lenWant)
		return false;
	const char *tail = path + lenPath - lenWant;
	for (size_t i = 0; i < lenWant; i++)
	{
		char a = tail[i] == '\\' ? '/' : tail[i];
		char b = want[i];
		if (a >= 'A' && a <= 'Z')
			a = (char)(a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z')
			b = (char)(b - 'A' + 'a');
		if (a != b)
			return false;
	}
	return true;
}

bool VGui2_IsCursorVisible()
{
	return s_bVGui2Available && g_pVGuiSurface && g_pVGuiSurface->IsCursorVisible();
}

void VGui2_Frame()
{
	if (g_pGameUIViewport)
		g_pGameUIViewport->Frame();
}

//-----------------------------------------------------------------------------
// vgui2_dumptree [all] -- simplified BHL vgui_dumptree (plain console output).
//-----------------------------------------------------------------------------
static void DumpPanel(vgui2::VPANEL panel, int offset, bool bParentVisible, bool bDumpAll)
{
	constexpr int INDENT_WIDTH = 4;
	char indent[256];

	memset(indent, ' ', sizeof(indent));
	if (offset * INDENT_WIDTH >= (int)sizeof(indent))
		offset = (sizeof(indent) - 1) / INDENT_WIDTH;
	indent[offset * INDENT_WIDTH] = '\0';

	int wide, tall, x, y;
	g_pVGuiPanel->GetSize(panel, wide, tall);
	g_pVGuiPanel->GetPos(panel, x, y);

	bool bIsVisible = g_pVGuiPanel->IsVisible(panel) && bParentVisible;

	const char *name = g_pVGuiPanel->GetName(panel);
	if (!name)
		name = "< null name >";
	else if (!name[0])
		name = "< no name >";

	char line[512];
	snprintf(line, sizeof(line), "%s%s [%s %d x %d] @ (%d; %d) [%s%s%s]%s\n", indent,
	    name,
	    g_pVGuiPanel->GetClassName(panel),
	    wide, tall, x, y,
	    g_pVGuiPanel->IsKeyBoardInputEnabled(panel) ? "K" : "",
	    g_pVGuiPanel->IsMouseInputEnabled(panel) ? "M" : "",
	    g_pVGuiPanel->IsPopup(panel) ? "P" : "",
	    bIsVisible ? " [Vis]" : "");
	sbo::ConsolePrint(line);

	if (bIsVisible || bDumpAll)
	{
		int count = g_pVGuiPanel->GetChildCount(panel);
		for (int i = 0; i < count; i++)
			DumpPanel(g_pVGuiPanel->GetChild(panel, i), offset + 1, bIsVisible, bDumpAll);
	}
}

static void DumpTree_f()
{
	if (!g_pEngineVGui)
		return;

	bool bDumpAll = sbo::Cmd_Argc() > 1 && !strcmp(sbo::Cmd_Argv(1), "all");
	DumpPanel(g_pEngineVGui->GetPanel(PANEL_ROOT), 0, true, bDumpAll);
}

// Called from VGui2_Chat_Init (HUD_Init), when the engine's command system is
// certainly available to the client.
void VGui2_RegisterDebugCommands()
{
	static bool s_bDone = false;
	if (s_bDone)
		return;
	s_bDone = true;
	sbo::AddCommand("vgui2_dumptree", DumpTree_f);
}
