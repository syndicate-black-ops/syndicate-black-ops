//=========================================================
// SBO 2.0 round hooks for mappers: see round_hooks.h and docs/LOGIC_ENTITIES.md "Rounds".
// Event names from ReGameDLL_CS (multiplay_gamerules.cpp: FireTargets("game_round_start" / "game_round_end")).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"gamerules.h"
#include	"round_hooks.h"

void RoundHooks_Reset( void )
{
	// every entity of the map. INDEXENT gives NULL for a free slot; Instance NULL for an edict without a class
	// (a client slot nobody uses)
	for ( int i = 1; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pEdict = INDEXENT( i );

		if ( !pEdict || pEdict->free || ( pEdict->v.flags & FL_KILLME ) )
			continue;

		CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );

		if ( pEntity )
			pEntity->RoundReset();
	}
}

static void RoundHooks_Fire( const char *pszEvent )
{
	CBaseEntity *pWorld = CBaseEntity::Instance( INDEXENT( 0 ) );

	// the world as activator and caller, as XDM does: entities that use their activator get a valid one (!activator
	// of the logic entities is the world then)
	FireTargets( pszEvent, pWorld, pWorld, USE_TOGGLE, 0 );
}

void RoundHooks_Start( void )
{
	RoundHooks_Fire( ROUND_HOOK_START );
}

void RoundHooks_End( void )
{
	// a trigger_camera that still has a player's view (and froze him) lets go at the round end; a camera that
	// game_round_end fires takes it again
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->m_hViewEntity != NULL )
		{
			pPlayer->ResetViewEntity();
			pPlayer->EnableControl( TRUE );
		}
	}

	RoundHooks_Fire( ROUND_HOOK_END );
}

BOOL Logic_IsPlayerInPlay( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return FALSE;

	CBasePlayer *pPlayer = (CBasePlayer *)pEntity;

	// IsRoundCandidate: in the game (InitHUD done, not disconnected), not a Game Master, HLTV proxy or spectator;
	// SboScoreboard_InPlay: alive and in play now (SBO's dead and waiting players are observers with full health)
	return IsRoundCandidate( pPlayer ) && !pPlayer->m_bGameMaster && SboScoreboard_InPlay( pPlayer );
}
