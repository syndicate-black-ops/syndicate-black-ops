//========= SBO: Discord Rich Presence =========//
//
// Purpose: Game glue for Discord Rich Presence, see discord_rpc.h.
//==============================================//

// C++ headers first: SBO's headers define macros (min/max, ...) that break them
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <chrono>
#include <string>
#ifndef _WIN32
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "discord_ipc.h"

#include "hud.h"
#include "cl_util.h"
#include "viewport.h"
#include "net_api.h"

#include "discord_rpc.h"

#ifndef SBO_DISCORD_APP_ID
#define SBO_DISCORD_APP_ID ""
#endif

// Art asset key of the large image (Discord developer portal -> Rich Presence -> Art Assets).
#define SBO_DISCORD_IMAGE "sbo"

// How often the presence is rebuilt from the game state (the IPC client sends only changes, rate-limited).
#define REBUILD_INTERVAL 1.0

static cvar_t *cl_discord_rpc;
static discord::IpcClient s_ipc;
static bool s_active;
static double s_nextRebuild;
static char s_map[64];
static long long s_mapStart; // unix time the current map was entered

static void LogToConsole(const char *msg)
{
	gEngfuncs.pfnConsolePrint(msg);
}

// Keeps only an "ip:port" prefix (digits, dots, colon), like OpenAG's sanitize_address: the join secret comes from
// another player's Discord and ends up in a console command.
static std::string SanitizeAddress(const std::string &s)
{
	size_t n = 0;
	while (n < s.size() && ((s[n] >= '0' && s[n] <= '9') || s[n] == '.' || s[n] == ':'))
		n++;
	return s.substr(0, n);
}

// The server's address as other players can reach it, "" for none (not connected, or a listen server we host:
// the engine only knows "loopback" then).
static std::string JoinableServerAddress()
{
	if (!gEngfuncs.pNetAPI)
		return "";
	net_status_t status;
	memset(&status, 0, sizeof(status));
	gEngfuncs.pNetAPI->Status(&status);
	if (!status.connected)
		return "";
	const char *adr = gEngfuncs.pNetAPI->AdrToString(&status.remote_address);
	std::string s = SanitizeAddress(adr ? adr : "");
	if (s.empty() || s.find(':') == std::string::npos || !strncmp(s.c_str(), "127.", 4) || !strncmp(s.c_str(), "0.", 2))
		return "";
	return s;
}

// Discord events (subscribed in DiscordRpc_Init): a friend clicked "Join" on our profile (we get our own join
// secret back on the joining side), or asked to join (accepted automatically, like OpenAG: SBO servers are public).
static void OnDiscordEvent(const char *evt, const std::string &json)
{
	if (!strcmp(evt, "ACTIVITY_JOIN"))
	{
		std::string adr = SanitizeAddress(discord::JsonField(json, "secret"));
		if (adr.empty())
			return;
		char cmd[96];
		snprintf(cmd, sizeof(cmd), "connect %s\n", adr.c_str());
		gEngfuncs.pfnConsolePrint("Discord: joining a friend's game.\n");
		gEngfuncs.pfnClientCmd(cmd);
	}
	else if (!strcmp(evt, "ACTIVITY_JOIN_REQUEST"))
	{
		std::string id = discord::JsonField(json, "id"); // data.user.id
		std::string name = discord::JsonField(json, "username");
		if (id.empty())
			return;
		s_ipc.SendCommand("SEND_ACTIVITY_JOIN_INVITE", "{\"user_id\":\"" + discord::JsonEscape(id.c_str()) + "\"}");
		char msg[192];
		snprintf(msg, sizeof(msg), "Discord: %s asked to join your game (accepted).\n", name.c_str());
		gEngfuncs.pfnConsolePrint(msg);
	}
}

#ifdef _WIN32
// Lets Discord start the game when a friend clicks "Join" and SBO isn't running (discord-rpc's Discord_Register):
// HKCU\Software\Classes\discord-<app id> -> "<hl.exe>" -game <gamedir>. Per user, no admin rights needed.
static void RegisterLaunchCommand()
{
	wchar_t exe[MAX_PATH];
	if (!GetModuleFileNameW(nullptr, exe, MAX_PATH))
		return;
	wchar_t game[64];
	const char *gamedir = gEngfuncs.pfnGetGameDirectory();
	MultiByteToWideChar(CP_UTF8, 0, gamedir && gamedir[0] ? gamedir : "syndicate", -1, game, 64);

	wchar_t key[128], cmd[MAX_PATH + 96], name[128];
	_snwprintf(key, 128, L"Software\\Classes\\discord-%hs", SBO_DISCORD_APP_ID);
	_snwprintf(cmd, MAX_PATH + 96, L"\"%s\" -game %s", exe, game);
	_snwprintf(name, 128, L"URL:Run game %hs protocol", SBO_DISCORD_APP_ID);
	RegSetKeyValueW(HKEY_CURRENT_USER, key, nullptr, REG_SZ, name, (DWORD)((wcslen(name) + 1) * sizeof(wchar_t)));
	RegSetKeyValueW(HKEY_CURRENT_USER, key, L"URL Protocol", REG_SZ, L"", sizeof(wchar_t));
	wchar_t sub[160];
	_snwprintf(sub, 160, L"%s\\DefaultIcon", key);
	RegSetKeyValueW(HKEY_CURRENT_USER, sub, nullptr, REG_SZ, exe, (DWORD)((wcslen(exe) + 1) * sizeof(wchar_t)));
	_snwprintf(sub, 160, L"%s\\shell\\open\\command", key);
	RegSetKeyValueW(HKEY_CURRENT_USER, sub, nullptr, REG_SZ, cmd, (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t)));
}
#else
// The same on Linux, the way discord-rpc does it (discord_register_linux.cpp): a desktop entry
// ~/.local/share/applications/discord-<app id>.desktop that handles the URL scheme discord-<app id>, made the
// scheme's default handler with xdg-mime. The command is the Windows one, "<game launcher>" -game <gamedir>: hl.sh
// next to the running hl_linux (it sets the library path the engine needs), or the executable itself without it.
// Per user. Nothing happens without $HOME; a missing xdg-mime only loses the association.
static void RegisterLaunchCommand()
{
	const char *home = getenv("HOME");
	if (!home || !home[0])
		return;

	char exe[1024];
	ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if (len <= 0)
		return;
	exe[len] = 0;
	std::string dir(exe);
	size_t slash = dir.rfind('/');
	if (slash == std::string::npos)
		return;
	dir.erase(slash);
	std::string launcher = dir + "/hl.sh";
	if (access(launcher.c_str(), X_OK) != 0)
		launcher = exe;

	// the game directory is a plain folder name; anything else falls back like the Windows code's default
	const char *gamedir = gEngfuncs.pfnGetGameDirectory();
	std::string game = gamedir && gamedir[0] ? gamedir : "syndicate";
	for (size_t i = 0; i < game.size(); i++)
	{
		if (!isalnum((unsigned char)game[i]) && game[i] != '_' && game[i] != '-')
		{
			game = "syndicate";
			break;
		}
	}

	// Exec: a quoted argument, with the characters the desktop entry specification wants escaped inside quotes
	std::string quoted;
	for (size_t i = 0; i < launcher.size(); i++)
	{
		char c = launcher[i];
		if (c == '"' || c == '`' || c == '$' || c == '\\')
			quoted += '\\';
		quoted += c;
		if (c == '%')
			quoted += '%'; // a literal percent sign is "%%" in Exec
	}

	std::string entry = "[Desktop Entry]\nName=Game " SBO_DISCORD_APP_ID "\nExec=\"" + quoted + "\" -game " + game
	    + "\nPath=" + dir + "\nType=Application\nNoDisplay=true\nCategories=Discord;Games;\n"
	      "MimeType=x-scheme-handler/discord-" SBO_DISCORD_APP_ID ";\n";

	std::string path = std::string(home) + "/.local";
	mkdir(path.c_str(), 0755);
	path += "/share";
	mkdir(path.c_str(), 0755);
	path += "/applications";
	mkdir(path.c_str(), 0755);
	path += "/discord-" SBO_DISCORD_APP_ID ".desktop";

	// unchanged since the last start: nothing to write, and no xdg-mime process
	std::string old;
	if (FILE *f = fopen(path.c_str(), "r"))
	{
		char buf[512];
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
			old.append(buf, n);
		fclose(f);
	}
	if (old == entry)
		return;

	FILE *f = fopen(path.c_str(), "w");
	if (!f)
		return;
	fwrite(entry.data(), 1, entry.size(), f);
	fclose(f);

	// discord-rpc: system(). The application ID is compiled in, nothing here comes from outside.
	if (system("xdg-mime default discord-" SBO_DISCORD_APP_ID ".desktop x-scheme-handler/discord-" SBO_DISCORD_APP_ID
	           " >/dev/null 2>&1") != 0)
		LogToConsole("Discord: could not register the join handler (xdg-mime).\n");
}
#endif

void DiscordRpc_Init()
{
	static const char *const s_events[] = {"ACTIVITY_JOIN", "ACTIVITY_JOIN_REQUEST", nullptr};
	cl_discord_rpc = CVAR_CREATE("cl_discord_rpc", "1", FCVAR_ARCHIVE);
	s_ipc.Init(SBO_DISCORD_APP_ID, LogToConsole, s_events, OnDiscordEvent);
	if (SBO_DISCORD_APP_ID[0])
		RegisterLaunchCommand();
}

// "maps/co_aemos.bsp" -> "co_aemos"
static void BaseMapName(const char *level, char *out, size_t outSize)
{
	const char *base = strrchr(level, '/');
	base = base ? base + 1 : level;
	strncpy(out, base, outSize - 1);
	out[outSize - 1] = 0;
	char *dot = strrchr(out, '.');
	if (dot)
		*dot = 0;
}

// Game mode from the map prefix, as the server's InstallGameRules (gamerules.cpp) picks it.
static const char *GameMode(const char *map)
{
	if (!strncmp(map, "co_", 3))
		return "Co-op";
	if (!strncmp(map, "br_", 3))
		return "Battle Royale";
	if (!strncmp(map, "sp_", 3))
		return "Teamplay";
	return "Bounty Killer";
}

// Server name without SBO/BHL colour codes (^0-^9, ^xRGB with three hex digits, ^#RRGGBB with six) and control
// characters.
static std::string CleanName(const char *s)
{
	std::string out;
	for (; s && *s; s++)
	{
		if (s[0] == '^' && s[1] >= '0' && s[1] <= '9')
		{
			s++;
			continue;
		}
		if (s[0] == '^' && (s[1] == 'x' || s[1] == 'X') && isxdigit((unsigned char)s[2]) && isxdigit((unsigned char)s[3]) && isxdigit((unsigned char)s[4]))
		{
			s += 4;
			continue;
		}
		if (s[0] == '^' && s[1] == '#' && isxdigit((unsigned char)s[2]) && isxdigit((unsigned char)s[3]) && isxdigit((unsigned char)s[4])
		    && isxdigit((unsigned char)s[5]) && isxdigit((unsigned char)s[6]) && isxdigit((unsigned char)s[7]))
		{
			s += 7;
			continue;
		}
		if ((unsigned char)*s >= 0x20)
			out += *s;
	}
	while (!out.empty() && out.back() == ' ')
		out.pop_back();
	return out;
}

static int PlayerCount()
{
	int n = 0;
	for (int i = 1; i <= gEngfuncs.GetMaxClients(); i++)
	{
		hud_player_info_t info;
		memset(&info, 0, sizeof(info));
		GetPlayerInfo(i, &info);
		if (info.name && info.name[0])
			n++;
	}
	return n;
}

static std::string BuildActivity()
{
	using discord::JsonEscape;
	const char *level = gEngfuncs.pfnGetLevelName();
	std::string json = "{";

	if (!level || !level[0])
	{
		s_map[0] = 0;
		json += "\"details\":\"In the main menu\"";
		json += ",\"assets\":{\"large_image\":\"" SBO_DISCORD_IMAGE "\",\"large_text\":\"Syndicate Black Ops\"}";
		return json + "}";
	}

	char map[64];
	BaseMapName(level, map, sizeof(map));
	if (strcmp(map, s_map))
	{
		strcpy(s_map, map);
		s_mapStart = (long long)time(nullptr);
	}

	std::string server = CleanName(gViewPort ? gViewPort->m_szServerName : "");
	std::string state = GameMode(map);
	state += " | ";
	state += map;
	if (g_iUser1)
		state += " | Spectating";

	json += "\"details\":\"" + JsonEscape(server.empty() ? "Playing online" : server.c_str()) + "\"";
	json += ",\"state\":\"" + JsonEscape(state.c_str()) + "\"";
	char buf[160];
	snprintf(buf, sizeof(buf), ",\"timestamps\":{\"start\":%lld}", s_mapStart);
	json += buf;
	json += ",\"assets\":{\"large_image\":\"" SBO_DISCORD_IMAGE "\",\"large_text\":\"" + JsonEscape(map) + "\"}";
	int maxPlayers = gEngfuncs.GetMaxClients();
	std::string address = JoinableServerAddress();
	if (maxPlayers > 1)
	{
		// Same party id for everyone on the server; Discord groups them and shows "(n of max)".
		snprintf(buf, sizeof(buf), "\",\"size\":[%d,%d]}", PlayerCount(), maxPlayers);
		json += ",\"party\":{\"id\":\"sbo-" + JsonEscape(address.empty() ? map : address.c_str()) + buf;
		// Join secret = the server address: Discord offers "Join" / "Ask to Join" to friends (OnDiscordEvent).
		if (!address.empty())
			json += ",\"secrets\":{\"join\":\"" + JsonEscape(address.c_str()) + "\"}";
	}
	return json + "}";
}

void DiscordRpc_Frame()
{
	// HUD_Frame's argument is the frame time, not a clock: use a monotonic clock of our own.
	static const auto s_t0 = std::chrono::steady_clock::now();
	const double time = std::chrono::duration<double>(std::chrono::steady_clock::now() - s_t0).count();

	if (!cl_discord_rpc)
		return;

	if (cl_discord_rpc->value == 0 || !SBO_DISCORD_APP_ID[0])
	{
		if (s_active)
		{
			s_ipc.Disconnect();
			s_active = false;
		}
		return;
	}
	s_active = true;

	if (time >= s_nextRebuild)
	{
		s_nextRebuild = time + REBUILD_INTERVAL;
		s_ipc.SetActivity(BuildActivity());
	}
	s_ipc.Update(time);
}
