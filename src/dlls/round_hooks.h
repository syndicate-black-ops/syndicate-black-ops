//=========================================================
// SBO 2.0 round hooks for mappers (docs/LOGIC_ENTITIES.md "Rounds"). SBO's rounds (co-op, BR, Bounty Killer) respawn
// the players, not the map's entities, so a map's logic keeps its state from one round to the next. The hooks:
//
//   * "game_round_start": every entity with this name is fired (USE_TOGGLE, the world as activator and caller) at
//     each round start, after the round has spawned its players. ReGameDLL_CS's name (multiplay_gamerules.cpp
//     RestartRound); XDM fires the same event as "game_roundstart".
//   * "game_round_end": fired the same way when the round ends (lost, won, emptied, mission over). ReGameDLL_CS's
//     name (XDM: "game_roundend").
//   * "round_reset" 1 on a stateful SBO 2.0 logic entity (trigger_condition, trigger_random, trigger_timer,
//     trigger_entity_iterator, ...): CBaseEntity::RoundReset at each round start, before the players spawn, puts it
//     back to its state after the map load. ReGameDLL_CS resets every entity (CBaseEntity::Restart); SBO's rounds
//     keep the map as it is, so here it is the mapper's choice per entity.
//=========================================================

#ifndef ROUND_HOOKS_H
#define ROUND_HOOKS_H

#define ROUND_HOOK_START	"game_round_start"
#define ROUND_HOOK_END		"game_round_end"

// a round is about to spawn its players: RoundReset on every entity
void RoundHooks_Reset( void );
// the round has started (its players are spawned): fires game_round_start
void RoundHooks_Start( void );
// the round is over: fires game_round_end
void RoundHooks_End( void );

// A player that counts as one for the logic entities ("every player", player filters, counters): in the game and
// in play now (alive, not observing, not dead or waiting for a round), not a spectator, Game Master or HLTV proxy.
// FALSE for anything that isn't a player.
BOOL Logic_IsPlayerInPlay( CBaseEntity *pEntity );

#endif // ROUND_HOOKS_H
