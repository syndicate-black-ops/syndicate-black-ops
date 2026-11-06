// Stub GoldSrc engine for the server fuzz harness (tests/fuzz/server/run_test.sh): enough of enginefuncs_t to load
// hl.so, spawn a world, connect players and run frames, without a map, a network or a real engine.
//
// What the stubs check: the conditions on which a stock Valve engine (HLDS, HL25) stops the server with Sys_Error or
// Host_Error (user message sizes, sound parameters, precaches outside the spawn functions, models that were never
// precached, no free edicts, a message started inside another one, ...). Each one is reported as "ENGINE:" with the
// action that caused it (fuzz_server.cpp), so a hl.so bug that would crash a real server shows up here as well as the
// memory errors that AddressSanitizer / UBSan find.
#pragma once

// the C++ library first: extdll.h defines min / max as macros
#include <algorithm>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "extdll.h"
#include "eiface.h"
#undef min
#undef max

#define STUB_MAX_EDICTS 2048	// liblist.gam "edicts" "2048"

extern edict_t *g_stubEdicts;
extern int g_stubMaxClients;
extern globalvars_t g_stubGlobals;
extern DLL_FUNCTIONS g_dll;
extern NEW_DLL_FUNCTIONS g_newDll;
extern bool g_stubHaveNewDll;
extern bool g_stubLoading;	// precaches are allowed (the engine's ss_loading)
extern int g_stubReports;	// engine-error conditions seen
extern std::string g_stubAction;	// what the harness is doing (printed with reports and AddressSanitizer errors)
extern bool g_stubVerbose;
extern std::string g_stubGameDir;
extern std::string g_stubConsole;	// <repo>/gamedir/syndicate: LoadFileForMe reads the shipped files

// the command the next ClientCommand sees (Cmd_Argc / Cmd_Argv / Cmd_Args)
void Stub_SetCommand( const std::vector<std::string> &argv, const std::string &args );

void Stub_Init( void *hGameDll, enginefuncs_t *pEngfuncs );
edict_t *Stub_AllocEdict( void );
void Stub_FreeEdict( edict_t *e );
edict_t *Stub_CreateNamed( const char *classname );
int Stub_AllocString( const char *s );
char *Stub_InfoBuffer( edict_t *e );
void Stub_SetInfo( char *buffer, const char *key, const char *value );
void Stub_Report( const char *fmt, ... );
// key values the harness gave the entities: a precache / model error on one of them is the map's fault (the stock engine
// stops a server on a missing or empty model the map names), counted apart from the game's own (Stub_MapReport)
extern std::set<std::string> g_stubMapStrings;
extern int g_stubMapReports;
const std::vector<std::pair<std::string, void (*)( void )>> &Stub_ServerCommands( void );
