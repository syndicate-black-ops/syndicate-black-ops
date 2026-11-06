//========= SBO: Discord Rich Presence =========//
//
// Purpose: Shows what the player is doing in their Discord profile (server, game mode, map, player count, time on
//          the map), with "Join" / "Ask to Join" for friends on public servers (join secret = server address; join
//          requests are accepted automatically). Uses the Discord app's local RPC (discord_ipc.h); nothing is sent
//          anywhere else. Registers discord-<app id> (Windows: in HKCU; Linux: a desktop entry and xdg-mime)
//          so Discord can start the game for a join.
//
// cvar cl_discord_rpc (archived, default 1): 0 turns it off and clears the presence. Also in the "Advanced"
// dialog (VGUI2 builds).
//
// The Discord application ID is compiled in (CMake cache variable SBO_DISCORD_APP_ID); an empty ID disables the
// feature.
//
// Similar in scope to OpenAG's cl_dll/discord_integration.cpp (github.com/YaLTeR/OpenAG, which uses discord-rpc).
//==============================================//

#ifndef SBO_DISCORD_RPC_H
#define SBO_DISCORD_RPC_H

//! HUD_Init: registers cl_discord_rpc.
void DiscordRpc_Init();
//! HUD_Frame.
void DiscordRpc_Frame();

#endif // SBO_DISCORD_RPC_H
