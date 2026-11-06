//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: see client_steam_context.h (BugfixedHL-Rebased client_steam_context.cpp, Valve code).
// SDK world.
//=================================================================//
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <tier0/platform.h>
#include <stdio.h>
#include "client_steam_context.h"
#include "sbo_bridge.h"

static CClientSteamContext g_ClientSteamContext;
CClientSteamContext &ClientSteamContext()
{
	return g_ClientSteamContext;
}

CClientSteamContext::CClientSteamContext()
{
}

//-----------------------------------------------------------------------------
// Purpose: Unload the steam3 engine
//-----------------------------------------------------------------------------
void CClientSteamContext::Shutdown()
{
	if (!m_bActive)
		return;

	m_bActive = false;
	m_bLoggedOn = false;
#if !defined(NO_STEAM)
	Clear(); // Steam API context shutdown
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the steam3 connection
//-----------------------------------------------------------------------------
void CClientSteamContext::Activate()
{
	if (m_bActive)
		return;

	m_bActive = true;

#if !defined(NO_STEAM)
	if (!SteamAPI_IsAvailable())
		return;

	SteamAPI_InitSafe(); // ignore failure, that will fall out later when they don't get a valid logon cookie
	Init(); // Steam API context init

	m_bLoggedOn = (SteamUser() && SteamUtils() && SteamUser()->BLoggedOn());
	char buf[64];
	snprintf(buf, sizeof(buf), "CClientSteamContext logged on = %d\n", m_bLoggedOn ? 1 : 0);
	sbo::ConsolePrint(buf);
#endif
}

bool CClientSteamContext::IsSteamPipeEngine()
{
	static int s_iResult = -1;
	if (s_iResult == -1)
	{
#ifdef _WIN32
		HMODULE hSDL = nullptr;
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "SDL2.dll", &hSDL);
		s_iResult = hSDL ? 1 : 0;
#else
		s_iResult = 1;
#endif
	}
	return s_iResult == 1;
}
