//=========================================================
// SBO 2.0 test layer: test_* commands that drive a server and record a trace of what the game does, for the
// automated entity tests (docs/dev/ENTITY_TESTS.md, tests/entity/). Compiled in only with SBO_TEST_COMMANDS (CMake
// option of the same name, off in release builds); without it every SBO_TEST_CALL is empty and sbo_test.cpp is
// not built.
//=========================================================

#ifndef SBO_TEST_H
#define SBO_TEST_H

#ifdef SBO_TEST_COMMANDS

class CBaseEntity;

void Test_HookEngine( enginefuncs_t *pFuncs );		// GiveFnptrsToDll: wraps a few engine functions (trace, seed)
void Test_GameInit( void );							// GameDLLInit: registers the server commands
void Test_StartFrame( void );						// StartFrame, first thing: steps, fake players, trace
void Test_ServerActivate( void );
void Test_ServerDeactivate( void );
BOOL Test_ClientCommand( edict_t *pEntity, const char *pcmd );	// a player's test_* command (sv_cheats 1)
void Test_TraceFire( const char *pszTarget, CBaseEntity *pActivator, CBaseEntity *pCaller, int useType, float value );
void Test_TraceUse( CBaseEntity *pTarget, CBaseEntity *pActivator, CBaseEntity *pCaller, int useType, float value );
BOOL Test_ShotLuckOff( void );						// test_shotluck 0: no random perfect / failed hits (combat.cpp)

#define SBO_TEST_CALL( x )	x

#else

#define SBO_TEST_CALL( x )

#endif

#endif // SBO_TEST_H
