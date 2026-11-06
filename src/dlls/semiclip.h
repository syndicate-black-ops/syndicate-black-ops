//=========================================================
// Semiclip: players walk through each other for mp_semiclip_time seconds after a round start, and see each other
// fade while they do. Native port of production's AMX Mod X module ReSemiclipHL 2.3.9 (s1lent, Adidasman, rtxa;
// GPL v2) with production's settings. docs/SEMICLIP.md
//=========================================================

#ifndef SEMICLIP_H
#define SEMICLIP_H

struct playermove_s;
struct entity_state_s;

// production's resemiclip.cfg (EU = US); only on / off, the time and the effects are cvars (game.cpp)
#define SEMICLIP_DISTANCE		200.0f	// resemiclip_distance: mp_semiclip_effects 0 only, reach of semiclip and of the fade
#define SEMICLIP_TRANSPARENCY	120		// resemiclip_transparency: mp_semiclip_effects 0 only, the fixed renderamt
#define SEMICLIP_MIN_AMOUNT		100.0f	// module's MIN_AMOUNT: lowest renderamt of the distance fade
#define SEMICLIP_FLOAT_CROUCH	35.9f	// module's FLOAT_CROUCH: height difference from which a crouched pair is solid
#define SEMICLIP_MAX_TIME		180.0f	// upper bound of resemiclip_time

void Semiclip_ServerActivate( void );					// map start
void Semiclip_ServerDeactivate( void );
void Semiclip_StartFrame( void );
void Semiclip_ClientDisconnect( edict_t *pEntity );
void Semiclip_RoundStart( void );						// "ROUND START" (co-op, BR)
void Semiclip_RoundFailed( void );						// "FAILED ROUND" (co-op)
void Semiclip_PassUntilApart( edict_t *pA, edict_t *pB );	// two players inside each other (GM teleporter exit)

void Semiclip_PM_Move( struct playermove_s *ppmove, int server );	// DLL_FUNCTIONS::pfnPM_Move
int Semiclip_ShouldCollide( edict_t *pentTouched, edict_t *pentOther );	// NEW_DLL_FUNCTIONS::pfnShouldCollide
void Semiclip_AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int player );

#endif // SEMICLIP_H
