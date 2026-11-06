// Force-included (/FI, -include on Linux) into every SBO client source by cmake/sbo_client.cmake.
//
// Windows:
// Modern Windows SDKs declare `HSPRITE` as a handle type (DECLARE_HANDLE in minwindef.h), which clashes with the
// HL SDK's `typedef int HSPRITE;` (engine/cdll_int.h). Include <windows.h> once, up front, with its HSPRITE renamed;
// every later #include <windows.h> in the HL code is then a no-op and HSPRITE stays the engine's int handle.
#ifndef SBO_COMPAT_H
#define SBO_COMPAT_H

#ifdef _WIN32
#define HSPRITE WINDOWS_HSPRITE
#include <windows.h>
#undef HSPRITE
#else
// Linux (client.so, docs/dev/LINUX_CLIENT.md): what the SDK 2.3 client code takes from <windows.h> and from MSVC, the
// set Valve's SDK has in common/port.h and in its Makefile. C runtime names (stricmp, _snprintf, ...) are -D
// defines in cmake/sbo_client.cmake, like the Linux server's.
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#ifdef __cplusplus
// before cl_util.h's min/max macros: particleman.h and environment.h include it after them, and libstdc++'s
// <vector> does not survive the macros
#include <vector>
#endif

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *HINSTANCE;
typedef void *HWND;
typedef struct POINT_s
{
	int x;
	int y;
} POINT;

#ifndef FALSE
#define FALSE 0
#define TRUE (!FALSE)
#endif
#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

// MSVC calling conventions: cdecl is the only one on Linux
#define __cdecl
#define _cdecl
// The engine finds the client's functions by name (dlsym): the build hides every symbol (-fvisibility=hidden)
// except the ones the code marks __declspec( dllexport ), the only __declspec in the client code.
#define __declspec( x ) __attribute__ ((visibility("default")))
#define _declspec( x ) __attribute__ ((visibility("default")))
#endif

#endif // SBO_COMPAT_H
