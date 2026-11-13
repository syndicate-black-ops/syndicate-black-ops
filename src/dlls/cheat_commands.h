//=========================================================
// SBO 2.0: the Half-Life Unified SDK's cheat and entity debug commands (cheat_god, ent_create, npc_create, ...)
// and cheat_speed. Client commands, sv_cheats 1 only. docs/CHEAT_COMMANDS.md
//=========================================================

#ifndef CHEAT_COMMANDS_H
#define CHEAT_COMMANDS_H

class CBasePlayer;

// ClientCommand (client.cpp): TRUE when pcmd is one of these commands (run, or refused without sv_cheats)
BOOL Cheat_ClientCommand( CBasePlayer *pPlayer, const char *pcmd );

// GiveFnptrsToDll (h_export.cpp): notes every model, sound, generic file and event the map precaches, so
// ent_create / npc_create can refuse a class whose resources the map has not loaded (cheat_commands.cpp)
void Cheat_HookEngine( struct enginefuncs_s *pFuncs );

// ServerDeactivate (client.cpp): the next map precaches its own resources
void Cheat_ServerDeactivate( void );

// SBO 2.1: TRUE when the map has precached this sound (the precache table, case-insensitive). A key changed at run
// time may name only such a sound (ambient_generic "message"): a new precache would stop the server.
BOOL Cheat_SoundPrecached( const char *name );

#endif // CHEAT_COMMANDS_H
