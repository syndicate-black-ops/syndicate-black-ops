//=========================================================
// SBO 2.0 client OS, shared by the client and the server (docs/dev/CLIENT_INFO.md).
//
// The client registers CLIENT_OS_CVAR with the OS it was built for (CLIENT_OS_WINDOWS / CLIENT_OS_LINUX) when
// client.dll starts (CHud::Init) and sets it back every HUD think if it was changed. Not archived, not in the userinfo:
// only the server's cvar query (client_info.cpp) reads it, because Valve's engines have the same cvars on Windows and
// Linux. Clients without it (older builds) answer "Bad CVAR request" and stay "unknown".
//=========================================================

#ifndef CLIENT_OS_SHARED_H
#define CLIENT_OS_SHARED_H

#define CLIENT_OS_CVAR		"cl_sbo_os"
#define CLIENT_OS_WINDOWS	"windows"
#define CLIENT_OS_LINUX		"linux"

#ifdef _WIN32
#define CLIENT_OS_VALUE		CLIENT_OS_WINDOWS
#else
#define CLIENT_OS_VALUE		CLIENT_OS_LINUX
#endif

#endif // CLIENT_OS_SHARED_H
