//========= SBO VGUI2 port (development branch chat-vgui2) =========//
//
// Purpose: Steam API context for the VGUI2 scoreboard (avatars, profile overlay).
//          Cut-down port of BugfixedHL-Rebased src/game/client/client_steam_context.{h,cpp}
//          (Valve code): Activate()/Shutdown() only; the logged-on callbacks/delegates
//          BHL has are not used by anything SBO ports.
//
// hl1_source_sdk is built with SOURCE_SDK_MIN_STEAM_API: client.dll links steam_api_min.lib
// (only the old-steam_api.dll exports the Half-Life engine has shipped for years) and resolves
// SteamAPI_(Un)RegisterCallResult at run time. SteamAPI_IsAvailable() is false when those
// are missing (very old engine): then SteamFriends()/SteamUtils() stay null and the scoreboard
// draws the default avatar.
//=================================================================//
#ifndef SBO_VGUI2_CLIENT_STEAM_CONTEXT_H
#define SBO_VGUI2_CLIENT_STEAM_CONTEXT_H

#include <steam/steamtypes.h>
#include <steam/steam_api.h>
#include <steam/steamclientpublic.h>

class CClientSteamContext : public CSteamAPIContext
{
public:
	CClientSteamContext();

	void Activate();
	void Shutdown();

	bool BLoggedOn() { return m_bLoggedOn; }

	//! SteamPipe (SDL2) engine, i.e. HL25 and every Steam build since 2013: player_info_t carries the player's
	//! SteamID (BHL CEnginePatches::IsSDLEngine: SDL2.dll is loaded).
	static bool IsSteamPipeEngine();

private:
	bool m_bActive = false;
	bool m_bLoggedOn = false;
};

CClientSteamContext &ClientSteamContext(); // singleton accessor

#endif // SBO_VGUI2_CLIENT_STEAM_CONTEXT_H
