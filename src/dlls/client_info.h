//=========================================================
// SBO 2.0 per-player client info (client_info.cpp, docs/dev/CLIENT_INFO.md): the SBO client version, OS, engine
// and renderer of every player, kept on the server for the connection and written to the server log once.
//
// The enums follow SevenKewp's dlls/player/player_const.h (HL_CLIENT_SYSTEM, HL_CLIENT_ENGINE_VERSION,
// HL_CLIENT_MOD_VERSION, HL_CLIENT_RENDERER; HL SDK license), with the values SBO can tell apart.
//=========================================================

#ifndef CLIENT_INFO_H
#define CLIENT_INFO_H

enum HL_CLIENT_SYSTEM
{
	CLIENT_SYSTEM_NOT_CHECKED,	// no answer (yet), or the engine does not tell
	CLIENT_SYSTEM_WINDOWS,
	CLIENT_SYSTEM_LINUX,
	CLIENT_SYSTEM_NONE,			// a bot: no client
};

enum HL_CLIENT_ENGINE_VERSION
{
	CLIENT_ENGINE_NOT_CHECKED,	// no answer (yet)
	CLIENT_ENGINE_HL_LATEST,	// Steam Half-Life since the 25th anniversary update (HL25)
	CLIENT_ENGINE_HL_LEGACY,	// Steam Half-Life before it (the steam_legacy beta)
	CLIENT_ENGINE_HL_XASH,		// Xash3D
	CLIENT_ENGINE_BOT,			// bots (FL_FAKECLIENT) have no client
};

enum HL_CLIENT_MOD_VERSION
{
	CLIENT_MOD_NOT_CHECKED,		// not read yet
	CLIENT_MOD_SBO,				// an SBO client: version holds its userinfo version
	CLIENT_MOD_NONE,			// no SBO version in the userinfo (a listen server's own player on an old build, HLTV)
	CLIENT_MOD_BOT,				// bots
};

enum HL_CLIENT_RENDERER
{
	CLIENT_RENDERER_NOT_CHECKED,	// no answer (yet)
	CLIENT_RENDERER_OPENGL,
	CLIENT_RENDERER_SOFTWARE,
	CLIENT_RENDERER_NONE,			// a bot
};

#define CLIENT_INFO_VERSION_LEN	32

struct client_info_t
{
	HL_CLIENT_MOD_VERSION mod;
	char version[ CLIENT_INFO_VERSION_LEN ];	// "2.1.0" (only [0-9A-Za-z.+-_@], cut at 31 characters), "" without one
	HL_CLIENT_SYSTEM system;
	HL_CLIENT_ENGINE_VERSION engine;
	HL_CLIENT_RENDERER renderer;
	int complete;	// 1: every answer is in, or the wait ran out (the log line is written)
};

// The cvar queries (QueryClientCvarValue2), one per client cvar whose existence tells something
enum CLIENT_INFO_QUERY
{
	CIQ_HL25,		// sv_allow_shaders: HL25 engines (hw and sw), not the legacy engine, not Xash3D
	CIQ_XASH,		// host_ver: Xash3D only; its value names the OS
	CIQ_OPENGL,		// gl_fog: the OpenGL engine (hw), not the software one (sw)
	CIQ_OS,			// cl_sbo_os: SBO's client (2.1.0 on) names its OS ("windows" / "linux"; client_os_shared.h)
	CIQ_COUNT
};

// --- parsing, also built alone for tests/client_info (CLIENT_INFO_TEST) ---

// The cvar a query asks for, NULL out of range
const char *ClientInfo_QueryCvar( int query );
// A query's answer names an existing cvar: anything but the engine's "Bad CVAR request" (a missing cvar) and an
// empty / NULL answer. "CVAR is privileged" (the cvar exists, its value is protected) counts as existing.
bool ClientInfo_CvarExists( const char *answer );
// answers[CIQ_COUNT]: each query's answer, NULL when it has not come. Fills system / engine / renderer; a value
// needing an answer that has not come stays NOT_CHECKED. The OS: Xash3D's host_ver, otherwise cl_sbo_os.
void ClientInfo_Classify( const char *const answers[ CIQ_COUNT ], client_info_t *out );
// The userinfo version (VERSION_GATE_KEY) into out->mod / out->version: SBO with the version, NONE when empty / NULL
void ClientInfo_SetVersion( client_info_t *out, const char *userinfoValue );

// Names for the log line and plugins: "Windows" / "Linux" / "unknown" / "none", "HL25" / "legacy" / "Xash3D" /
// "bot" / "unknown", "OpenGL" / "software" / "unknown" / "none"
const char *ClientInfo_SystemName( HL_CLIENT_SYSTEM v );
const char *ClientInfo_EngineName( HL_CLIENT_ENGINE_VERSION v );
const char *ClientInfo_RendererName( HL_CLIENT_RENDERER v );
// "SBO 2.1.0", "none" (no version), "bot", "unknown" (not read)
void ClientInfo_VersionName( const client_info_t *info, char *out, int outSize );

#ifndef CLIENT_INFO_TEST
// --- game hooks (client.cpp, cbase.cpp) ---
void ClientInfo_ClientPutInServer( edict_t *pEntity );	// bots: BOT at once; players: the version, the queries on the next frame
void ClientInfo_ClientDisconnect( edict_t *pEntity );	// the slot starts clean
void ClientInfo_Frame( void );	// StartFrame: writes the line for a player whose answers did not all come in time
void ClientInfo_CvarValue2( const edict_t *pEnt, int requestID, const char *pszCvarName, const char *pszValue );	// NEW_DLL_FUNCTIONS

// The player's info (slot 1..maxClients), NULL for an empty slot. For the game and plugins (a game DLL function).
const client_info_t *ClientInfo_Get( int slot );

#ifdef SBO_TEST_COMMANDS
// test layer (test_clientinfo): start the wait without asking the engine (test players have no network), feed an
// answer as the engine would, print the slot
void ClientInfo_TestStart( edict_t *pEntity );
void ClientInfo_TestReply( edict_t *pEntity, const char *pszCvarName, const char *pszValue );
void ClientInfo_TestPrint( edict_t *pEntity, char *out, int outSize );
#endif
#endif

#endif // CLIENT_INFO_H
