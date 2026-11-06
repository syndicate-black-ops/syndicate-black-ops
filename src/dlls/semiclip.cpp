//=========================================================
// Semiclip: players walk through each other for mp_semiclip_time seconds after a round start, and see each other
// fade while they do. docs/SEMICLIP.md
//
// Native port of production's AMX Mod X module ReSemiclipHL 2.3.9 (s1lent, Adidasman, rtxa; GPL v2; SBO's fork),
// src/main.cpp, with the settings of production's resemiclip.cfg built in (semiclip.h): every player whatever the
// team (resemiclip_team 0), the moving platform fix (resemiclip_patch 1) and the crouch rule (resemiclip_crouch 1)
// always on. The module's per player switch and pair overrides (its AMX Mod X natives) are not ported.
//
// The module needed ReHLDS for one hook, SV_CreatePacketEntities; that part is in AddToFullPack here, so this runs
// on any engine:
//   movement   Semiclip_PM_Move (pfnPM_Move): the players the mover passes through are taken out of the move's
//              physents on the server. The client predicts the same way without knowing about semiclip: its engine
//              builds its physents from the entity states it got, and
//   packets    Semiclip_AddToFullPack sends each client the other players as SOLID_NOT (and faded) while they are
//              non-solid for it.
//   platforms  Semiclip_ShouldCollide (pfnShouldCollide): players on a moving platform don't block each other.
//
// The server's own solidity never changes: bullets, melee, explosions, monsters and triggers hit players as ever.
//
// When the time is up (or mp_semiclip is turned off) nobody is left stuck: the pairs of players who are inside each
// other at that moment stay passable for each other until they are apart, then they are solid too (Semiclip_Expire).
// That is the intent of the upstream module (s1lent's ReSemiclip, PM_Move after the physent loop: GET_COLLIDE,
// SetDont), whose code for it could not be reached in the module's fork that servers ran; here it is kept per pair and by the real
// overlap of the two hulls, where upstream kept one flag per player and a 96 unit test.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "game.h"
#include "entity_state.h"
#include "pm_defs.h"
#include "semiclip.h"

extern "C" void PM_Move( struct playermove_s *ppmove, int server );

#define SEMICLIP_MAX_PLAYERS	32

// ReSemiclipHL player.h CGamePlayer, per player slot (1-based), about every other slot. Written by the player's
// own moves (Semiclip_AllowDontSolid), read by the packets and Semiclip_ShouldCollide. As in the module an entry
// stays as it is while the other player is out of the mover's reach (not in its physents), until the mover has no
// player near at all.
struct SemiclipPlayer
{
	bool	bNonSolid[SEMICLIP_MAX_PLAYERS + 1];	// m_bSolid: the other player is non-solid for this one
	bool	bCrouch[SEMICLIP_MAX_PLAYERS + 1];		// m_bCrouch: the crouch rule holds the pair solid
	float	flDiff[SEMICLIP_MAX_PLAYERS + 1];		// m_fDiff: 2D distance to the other player
};

static SemiclipPlayer s_Players[SEMICLIP_MAX_PLAYERS + 1];

static float s_flExpireTime;		// CConfig cfg_count: semiclip ends when the map time passes this (0 = not set)
static bool s_bExpired;				// semiclip is over and Semiclip_Expire has run (g_timedSemiclipStateCleared)

// after the end: the pairs that were inside each other then and have not been apart since (symmetric)
static bool s_bLinger[SEMICLIP_MAX_PLAYERS + 1][SEMICLIP_MAX_PLAYERS + 1];
static int s_iLingerPairs;

// the cvars' values at the last look (Semiclip_StartFrame)
static bool s_bLastEnabled;
static bool s_bLastEffects;
static float s_flLastTime;

// free-roaming spectators: each player's nearest other player, once a frame
static bool s_bNearestDirty = true;
static bool s_bNearestActive[SEMICLIP_MAX_PLAYERS + 1];
static float s_flNearestDiff[SEMICLIP_MAX_PLAYERS + 1];

static inline bool Semiclip_Enabled( void )
{
	return semiclip.value != 0;
}

static inline bool Semiclip_Effects( void )
{
	return semiclip_effects.value != 0;
}

// resemiclip_time's bounds (resemiclip_config.sma): 0 (never expires) to 180
static inline float Semiclip_Time( void )
{
	if ( semiclip_time.value < 0 )
		return 0;

	return ( semiclip_time.value > SEMICLIP_MAX_TIME ) ? SEMICLIP_MAX_TIME : semiclip_time.value;
}

// ReSemiclipHL main.cpp IsSemiclipTimedOut
static inline bool Semiclip_TimedOut( void )
{
	return Semiclip_Time() > 0 && s_flExpireTime > 0 && gpGlobals->time > s_flExpireTime;
}

// semiclip runs for everybody: on and the time not up
static inline bool Semiclip_Active( void )
{
	return Semiclip_Enabled() && !Semiclip_TimedOut();
}

// ReSemiclipHL main.cpp CGamePlayer::Player_Clear
static void Semiclip_ClearPlayer( int iPlayer )
{
	memset( &s_Players[iPlayer], 0, sizeof( s_Players[iPlayer] ) );
}

// ReSemiclipHL main.cpp ClearAllPlayerSemiclipState
static void Semiclip_ClearAllPlayers( void )
{
	memset( s_Players, 0, sizeof( s_Players ) );
}

static void Semiclip_ClearLinger( void )
{
	memset( s_bLinger, 0, sizeof( s_bLinger ) );
	s_iLingerPairs = 0;
}

// ReSemiclipHL main.cpp IsPlayerEntityValidForSemiclipVisuals: a connected, living player who is no observer
static inline bool Semiclip_IsValidForVisuals( edict_t *pEntity )
{
	if ( !pEntity || !pEntity->pvPrivateData )
		return false;

	if ( !( pEntity->v.flags & ( FL_CLIENT | FL_FAKECLIENT ) ) )
		return false;

	return pEntity->v.deadflag == DEAD_NO && pEntity->v.iuser1 == 0;
}

// Are the two players inside each other? Their movement hulls (standing or ducked, by FL_DUCKING) at their
// origins; hulls that only touch (side by side, one standing on the other) are not.
static inline bool Semiclip_HullsOverlap( entvars_t *pevA, entvars_t *pevB )
{
	const Vector &vecSizeA = ( pevA->flags & FL_DUCKING ) ? VEC_DUCK_HULL_MAX : VEC_HULL_MAX;
	const Vector &vecSizeB = ( pevB->flags & FL_DUCKING ) ? VEC_DUCK_HULL_MAX : VEC_HULL_MAX;

	return fabs( pevA->origin.x - pevB->origin.x ) < vecSizeA.x + vecSizeB.x
		&& fabs( pevA->origin.y - pevB->origin.y ) < vecSizeA.y + vecSizeB.y
		&& fabs( pevA->origin.z - pevB->origin.z ) < vecSizeA.z + vecSizeB.z;
}

// the pair has been apart (or one of them is out of play): solid for each other from now on
static void Semiclip_EndLinger( int iPlayer, int iOther )
{
	if ( !s_bLinger[iPlayer][iOther] )
		return;

	s_bLinger[iPlayer][iOther] = s_bLinger[iOther][iPlayer] = false;
	s_Players[iPlayer].bNonSolid[iOther] = s_Players[iOther].bNonSolid[iPlayer] = false;
	s_Players[iPlayer].bCrouch[iOther] = s_Players[iOther].bCrouch[iPlayer] = false;
	s_iLingerPairs--;
	s_bNearestDirty = true;
}

// Semiclip is over (the time is up, or mp_semiclip went to 0 while it ran). ReSemiclipHL main.cpp
// ClearTimedSemiclipStateIfNeeded, which made everybody solid; here the pairs inside each other at this moment
// stay passable for each other until they are apart (Semiclip_Move, Semiclip_StartFrame). Every other pair is
// solid from now on, and no pair is added later: nobody can start to walk through anybody.
static void Semiclip_Expire( void )
{
	edict_t *pEdicts[SEMICLIP_MAX_PLAYERS + 1];
	int i, j;

	Semiclip_ClearLinger();

	for ( i = 1; i <= SEMICLIP_MAX_PLAYERS; i++ )
	{
		pEdicts[i] = ( i <= gpGlobals->maxClients ) ? INDEXENT( i ) : NULL;

		if ( !Semiclip_IsValidForVisuals( pEdicts[i] ) )
			pEdicts[i] = NULL;
	}

	for ( i = 1; i < SEMICLIP_MAX_PLAYERS; i++ )
	{
		if ( !pEdicts[i] )
			continue;

		for ( j = i + 1; j <= SEMICLIP_MAX_PLAYERS; j++ )
		{
			if ( pEdicts[j] && Semiclip_HullsOverlap( &pEdicts[i]->v, &pEdicts[j]->v ) )
			{
				s_bLinger[i][j] = s_bLinger[j][i] = true;
				s_iLingerPairs++;
			}
		}
	}

	// the pairs that go on keep their state (non-solid, crouch rule), every other pair loses it
	for ( i = 1; i <= SEMICLIP_MAX_PLAYERS; i++ )
	{
		for ( j = 1; j <= SEMICLIP_MAX_PLAYERS; j++ )
		{
			if ( !s_bLinger[i][j] )
			{
				s_Players[i].bNonSolid[j] = false;
				s_Players[i].bCrouch[j] = false;
				s_Players[i].flDiff[j] = 0;
			}
		}
	}

	s_bExpired = true;
	s_bNearestDirty = true;
}

static inline void Semiclip_ExpireIfNeeded( void )
{
	if ( !s_bExpired && !Semiclip_Active() )
		Semiclip_Expire();
}

// the pairs left over from the end, once a frame: apart, or one of them dead, gone or an observer?
static void Semiclip_CheckLinger( void )
{
	for ( int i = 1; i < SEMICLIP_MAX_PLAYERS && s_iLingerPairs > 0; i++ )
	{
		edict_t *pPlayer = NULL;

		for ( int j = i + 1; j <= SEMICLIP_MAX_PLAYERS; j++ )
		{
			if ( !s_bLinger[i][j] )
				continue;

			if ( !pPlayer )
				pPlayer = INDEXENT( i );

			edict_t *pOther = INDEXENT( j );

			if ( !Semiclip_IsValidForVisuals( pPlayer ) || !Semiclip_IsValidForVisuals( pOther )
				|| !Semiclip_HullsOverlap( &pPlayer->v, &pOther->v ) )
				Semiclip_EndLinger( i, j );
		}
	}
}

// SBO 2.0 GM mode (gmmode.cpp): a teleporter put a player inside another one. Like the pairs left inside each other
// when semiclip ends, they pass through each other until they are apart (production's GM mode plugin set a 2 s pair
// override, resemiclip_set_pair_override). Nothing to do while semiclip is on for everybody, or if they don't overlap.
void Semiclip_PassUntilApart( edict_t *pA, edict_t *pB )
{
	int i = ENTINDEX( pA ), j = ENTINDEX( pB );

	if ( i < 1 || i > SEMICLIP_MAX_PLAYERS || j < 1 || j > SEMICLIP_MAX_PLAYERS || i == j )
		return;
	if ( Semiclip_Active() || s_bLinger[i][j] )
		return;
	if ( !Semiclip_IsValidForVisuals( pA ) || !Semiclip_IsValidForVisuals( pB ) || !Semiclip_HullsOverlap( &pA->v, &pB->v ) )
		return;

	s_bLinger[i][j] = s_bLinger[j][i] = true;
	s_iLingerPairs++;
	s_bNearestDirty = true;
}

// semiclip starts (again) for everybody
static void Semiclip_Restart( void )
{
	Semiclip_ClearLinger();
	s_bExpired = false;
}

// ReSemiclipHL amxx_natives.cpp native_resemiclip_set_time, as resemiclip_config.sma called it at every map start
// and whenever one of its cvars changed: the time runs anew from now
static void Semiclip_ApplySettings( void )
{
	s_bLastEnabled = Semiclip_Enabled();
	s_bLastEffects = Semiclip_Effects();
	s_flLastTime = Semiclip_Time();

	if ( s_flLastTime > 0 && s_bLastEnabled )
		s_flExpireTime = gpGlobals->time + s_flLastTime;
	else
		s_flExpireTime = 0;

	// turned off: over as when the time is up (Semiclip_ExpireIfNeeded), and what is left of an earlier end goes on
	if ( s_bLastEnabled )
		Semiclip_Restart();
}

void Semiclip_ServerActivate( void )
{
	Semiclip_ClearAllPlayers();
	Semiclip_ClearLinger();
	s_bExpired = true;		// nothing to end yet
	Semiclip_ApplySettings();
	s_bNearestDirty = true;
}

// ReSemiclipHL main.cpp ServerDeactivate_Post
void Semiclip_ServerDeactivate( void )
{
	Semiclip_ClearAllPlayers();
	Semiclip_ClearLinger();
}

void Semiclip_StartFrame( void )
{
	s_bNearestDirty = true;

	// resemiclip_config.sma hook_resemiclip_cvar_changed
	if ( Semiclip_Enabled() != s_bLastEnabled || Semiclip_Effects() != s_bLastEffects || Semiclip_Time() != s_flLastTime )
		Semiclip_ApplySettings();

	Semiclip_ExpireIfNeeded();

	if ( s_iLingerPairs > 0 && !Semiclip_Active() )
		Semiclip_CheckLinger();
}

// ReSemiclipHL main.cpp ClientDisconnect
void Semiclip_ClientDisconnect( edict_t *pEntity )
{
	int iPlayer = ENTINDEX( pEntity );

	if ( iPlayer >= 1 && iPlayer <= SEMICLIP_MAX_PLAYERS )
	{
		for ( int i = 1; i <= SEMICLIP_MAX_PLAYERS; i++ )
			Semiclip_EndLinger( iPlayer, i );

		Semiclip_ClearPlayer( iPlayer );
	}
}

// ReSemiclipHL main.cpp OnRoundStartDetected. The module took a round start from the game's "ROUND START" text;
// the game rules call this where they print it (CCoop::CheckWho, CBr::CheckWho).
void Semiclip_RoundStart( void )
{
	Semiclip_ClearAllPlayers();

	if ( Semiclip_Time() > 0 )
		s_flExpireTime = gpGlobals->time + Semiclip_Time();
	else
		s_flExpireTime = 0;

	Semiclip_Restart();
	s_bExpired = !Semiclip_Enabled();	// off: nothing runs, nothing to end
}

// ReSemiclipHL main.cpp WriteString_Post, "FAILED ROUND" (CCoop::RoundWinner): the time is unset, so the next move
// of a living player starts it again (Semiclip_Move)
void Semiclip_RoundFailed( void )
{
	s_flExpireTime = 0;

	if ( Semiclip_Enabled() )
		Semiclip_Restart();		// it runs again from here
}

// a connected player's edict, or NULL
static inline edict_t *Semiclip_PlayerEdict( int iPlayer )
{
	if ( iPlayer < 1 || iPlayer > gpGlobals->maxClients || iPlayer > SEMICLIP_MAX_PLAYERS )
		return NULL;

	edict_t *pEdict = INDEXENT( iPlayer );

	if ( !pEdict || !pEdict->pvPrivateData || !( pEdict->v.flags & ( FL_CLIENT | FL_FAKECLIENT ) ) )
		return NULL;

	return pEdict;
}

// ReSemiclipHL main.cpp allowDontSolid: is the player in this physent non-solid for the mover? Also keeps the
// pair's state for the packets.
static bool Semiclip_AllowDontSolid( edict_t *pHost, int host, physent_t *pe )
{
	if ( !( pHost->v.flags & ( FL_CLIENT | FL_FAKECLIENT ) ) )
		return false;

	int ent = pe->info;		// the physent's edict number
	edict_t *pEntity = Semiclip_PlayerEdict( ent );

	if ( !pEntity )
		return false;

	SemiclipPlayer *pPlayer = &s_Players[host];
	SemiclipPlayer *pObject = &s_Players[ent];
	entvars_t *pevHost = &pHost->v;
	entvars_t *pevEnt = &pEntity->v;

	pPlayer->flDiff[ent] = ( pevHost->origin - pevEnt->origin ).Length2D();
	pPlayer->bNonSolid[ent] = ( Semiclip_Effects() || pPlayer->flDiff[ent] < SEMICLIP_DISTANCE );

	// resemiclip_crouch: a crouching player can stand on a crouching player (and get on by crouch-jumping). The
	// pair stays solid from the moment the higher one is at least FLOAT_CROUCH above while both crouch, until
	// they are level again.
	if ( pPlayer->bNonSolid[ent] )
	{
		float fDiff = fabs( pevHost->origin.z - pevEnt->origin.z );

		if ( fDiff < SEMICLIP_FLOAT_CROUCH && pPlayer->bCrouch[ent] && pObject->bCrouch[host] )
		{
			pPlayer->bCrouch[ent] = false;
			pObject->bCrouch[host] = false;
		}

		if ( fDiff >= SEMICLIP_FLOAT_CROUCH
			&& !pPlayer->bCrouch[ent]
			&& ( pevHost->button & IN_DUCK )
			&& ( ( pevEnt->button & IN_DUCK ) || ( pevEnt->flags & FL_DUCKING ) ) )
		{
			pPlayer->bCrouch[ent] = true;
			pObject->bCrouch[host] = true;
			return pPlayer->bNonSolid[ent] = false;
		}
		else if ( ( pPlayer->bCrouch[ent] || pObject->bCrouch[host] )
			&& ( pevHost->groundentity == pEntity || pevEnt->groundentity == pHost || fDiff >= SEMICLIP_FLOAT_CROUCH ) )
		{
			return pPlayer->bNonSolid[ent] = false;
		}
	}

	return pPlayer->bNonSolid[ent];
}

// ReSemiclipHL main.cpp PM_Move (a Metamod hook in front of the game's): takes the players the mover passes through
// out of the move's physents. The module's code after the loop, which kept players near each other non-solid once
// the time was up, could not be reached in the module's fork servers ran (the function had returned by then); what it was for
// is done per pair here: after the end only the pairs still inside each other since then (s_bLinger) are passable.
static void Semiclip_Move( playermove_t *pm )
{
	if ( !Semiclip_Enabled() && s_iLingerPairs <= 0 && s_bExpired )
		return;

	// the dead and observers move as ever
	if ( pm->deadflag != DEAD_NO || pm->dead || pm->spectator )
		return;

	int host = pm->player_index + 1;

	if ( host < 1 || host > gpGlobals->maxClients || host > SEMICLIP_MAX_PLAYERS )
		return;

	edict_t *pHost = INDEXENT( host );

	if ( !pHost || !pHost->pvPrivateData )
		return;

	// the time is not set (after "FAILED ROUND"): it starts with the first move
	if ( Semiclip_Enabled() && Semiclip_Time() > 0 && s_flExpireTime <= 0 )
	{
		s_flExpireTime = gpGlobals->time + Semiclip_Time();
		Semiclip_Restart();
	}

	const bool bActive = Semiclip_Active();

	if ( !bActive )
	{
		Semiclip_ExpireIfNeeded();

		if ( s_iLingerPairs <= 0 )
			return;
	}

	int j, numphysent;

	// the first player among the physents; the ones before it stay where they are
	for ( numphysent = 0; numphysent < pm->numphysent; numphysent++ )
	{
		if ( pm->physents[numphysent].player )
			break;
	}

	if ( numphysent >= pm->numphysent )
	{
		Semiclip_ClearPlayer( host );	// no player near
		return;
	}

	for ( j = numphysent; j < pm->numphysent; j++ )
	{
		bool bPass = false;

		if ( pm->physents[j].player )
		{
			if ( bActive )
			{
				bPass = Semiclip_AllowDontSolid( pHost, host, &pm->physents[j] );
			}
			else
			{
				// after the end: only a player the mover has been inside of since then, and only while it still is
				int ent = pm->physents[j].info;

				if ( ent >= 1 && ent <= SEMICLIP_MAX_PLAYERS && s_bLinger[host][ent] )
				{
					edict_t *pEntity = Semiclip_PlayerEdict( ent );

					if ( Semiclip_IsValidForVisuals( pEntity ) && Semiclip_HullsOverlap( &pHost->v, &pEntity->v ) )
						bPass = Semiclip_AllowDontSolid( pHost, host, &pm->physents[j] );
					else
						Semiclip_EndLinger( host, ent );
				}
			}
		}

		if ( !bPass )
			pm->physents[numphysent++] = pm->physents[j];
	}

	pm->numphysent = numphysent;
}

void Semiclip_PM_Move( struct playermove_s *ppmove, int server )
{
	Semiclip_Move( ppmove );
	PM_Move( ppmove, server );
}

// ReSemiclipHL main.cpp ShouldCollide, the lambda isOnMovingPlatform
static inline bool Semiclip_IsOnMovingPlatform( edict_t *pEntity )
{
	if ( !pEntity || !pEntity->v.groundentity )
		return false;

	entvars_t *pevGround = &pEntity->v.groundentity->v;

	if ( pevGround->movetype != MOVETYPE_PUSH && pevGround->movetype != MOVETYPE_PUSHSTEP )
		return false;

	return pevGround->velocity != g_vecZero || pevGround->avelocity != g_vecZero;
}

// ReSemiclipHL main.cpp ShouldCollide (resemiclip_patch 1). The engine asks this for every entity a trace or a move
// of pentOther could hit; 0 = pass through. Two players of whom at least one rides a moving platform (func_train,
// func_plat, func_door, func_rotating, ...) don't block each other while they are non-solid for each other:
// otherwise the platform, which may not push a rider into another player, stops (the jam). For the same players and
// time, traces that ignore one of them (their bullets and melee) pass through the other.
int Semiclip_ShouldCollide( edict_t *pentTouched, edict_t *pentOther )
{
	// after the end only the pairs still inside each other have their non-solid state left (Semiclip_Expire)
	if ( !Semiclip_Active() )
	{
		Semiclip_ExpireIfNeeded();

		if ( s_iLingerPairs <= 0 )
			return 1;
	}

	if ( !Semiclip_IsOnMovingPlatform( pentTouched ) && !Semiclip_IsOnMovingPlatform( pentOther ) )
		return 1;

	if ( !pentTouched || !pentOther )
		return 1;

	if ( !( pentTouched->v.flags & ( FL_CLIENT | FL_FAKECLIENT ) ) )
		return 1;

	int touchedIdx = ENTINDEX( pentTouched );

	if ( touchedIdx < 1 || touchedIdx > gpGlobals->maxClients || touchedIdx > SEMICLIP_MAX_PLAYERS )
		return 1;

	if ( pentOther->v.flags & ( FL_CLIENT | FL_FAKECLIENT ) )
	{
		int otherIdx = ENTINDEX( pentOther );

		if ( otherIdx >= 1 && otherIdx <= gpGlobals->maxClients && otherIdx <= SEMICLIP_MAX_PLAYERS )
		{
			if ( s_Players[touchedIdx].bNonSolid[otherIdx] || s_Players[otherIdx].bNonSolid[touchedIdx] )
				return 0;
		}
	}

	if ( !pentTouched->pvPrivateData || !pentOther->pvPrivateData )
		return 1;

	// ignore owner's
	if ( pentTouched->v.owner == pentOther || pentOther->v.owner == pentTouched )
		return 1;

	// a player's grenade passes through the players who are non-solid for the thrower (the module compared the
	// entity's class with CGrenade's; its Counter-Strike C4 test read CS's class layout and is not ported)
	if ( !( ( pentTouched->v.flags | pentOther->v.flags ) & FL_KILLME )
		&& pentOther->v.owner && FClassnameIs( &pentOther->v, "grenade" ) )
	{
		int ownerIdx = ENTINDEX( pentOther->v.owner );

		if ( ownerIdx >= 1 && ownerIdx <= gpGlobals->maxClients && ownerIdx <= SEMICLIP_MAX_PLAYERS
			&& s_Players[ownerIdx].bNonSolid[touchedIdx] )
			return 0;
	}

	return 1;
}

// ReSemiclipHL main.cpp IsFreeRoamSemiclipVisuallyActive, for every player at once: the 2D distance to the nearest
// other living player (mp_semiclip_effects 0: within SEMICLIP_DISTANCE). The module searched per packet entity;
// the positions are the same for every packet of a frame. After the end: the nearest of the players it is still
// inside of.
static void Semiclip_UpdateNearest( void )
{
	edict_t *pEdicts[SEMICLIP_MAX_PLAYERS + 1];
	int i, j;
	const bool bActive = Semiclip_Active();

	for ( i = 1; i <= SEMICLIP_MAX_PLAYERS; i++ )
	{
		s_bNearestActive[i] = false;
		s_flNearestDiff[i] = 0;
		pEdicts[i] = ( i <= gpGlobals->maxClients ) ? INDEXENT( i ) : NULL;

		if ( !Semiclip_IsValidForVisuals( pEdicts[i] ) )
			pEdicts[i] = NULL;
	}

	for ( i = 1; i < SEMICLIP_MAX_PLAYERS; i++ )
	{
		if ( !pEdicts[i] )
			continue;

		for ( j = i + 1; j <= SEMICLIP_MAX_PLAYERS; j++ )
		{
			if ( !pEdicts[j] || ( !bActive && !s_bLinger[i][j] ) )
				continue;

			float flDiff = ( pEdicts[i]->v.origin - pEdicts[j]->v.origin ).Length2D();

			if ( !Semiclip_Effects() && flDiff >= SEMICLIP_DISTANCE )
				continue;

			if ( !s_bNearestActive[i] || flDiff < s_flNearestDiff[i] )
				s_flNearestDiff[i] = flDiff;

			if ( !s_bNearestActive[j] || flDiff < s_flNearestDiff[j] )
				s_flNearestDiff[j] = flDiff;

			s_bNearestActive[i] = s_bNearestActive[j] = true;
		}
	}

	s_bNearestDirty = false;
}

// ReSemiclipHL main.cpp SV_CreatePacketEntities (the ReHLDS hook, which went through a client's finished packet;
// AddToFullPack fills that packet entity by entity) with GetSemiclipViewPlayer, GetObservedAlivePlayer and
// IsPairSemiclipVisuallyActive: host gets a player (and what is attached to one with MOVETYPE_FOLLOW) as SOLID_NOT
// and faded while that player is non-solid for the player whose view host has:
//   a living player    its own: the pairs its last moves found non-solid
//   an observer with a living target (iuser2)    the target's, and every other living player whatever the crouch
//                      rule or the target's reach say
//   an observer without one (free roaming)    every living player that has another one to pass through, faded by
//                      the distance to the nearest
//   the dead           nothing
// After the end the same, for the pairs still inside each other only.
// Runs for every entity of every client's packet: everything here is a few comparisons and table reads.
void Semiclip_AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int player )
{
	if ( !player && ( ent->v.movetype != MOVETYPE_FOLLOW || !ent->v.aiment ) )
		return;

	const bool bActive = Semiclip_Active();

	if ( !bActive && s_iLingerPairs <= 0 )
		return;

	// whose view
	edict_t *pViewPlayer = NULL;
	int iView = 0;

	if ( host->v.iuser1 != 0 )
	{
		iView = host->v.iuser2;
		pViewPlayer = Semiclip_PlayerEdict( iView );

		if ( pViewPlayer && pViewPlayer->v.deadflag != DEAD_NO )
			pViewPlayer = NULL;
	}

	if ( !pViewPlayer )
	{
		if ( host->v.deadflag == DEAD_NO )
		{
			pViewPlayer = host;
			iView = ENTINDEX( host );

			if ( iView < 1 || iView > SEMICLIP_MAX_PLAYERS )
				return;
		}
		else if ( host->v.iuser1 == 0 )
		{
			return;
		}
	}

	// which player
	edict_t *pObjectEdict;
	int iObject;

	if ( player )
	{
		if ( ent->v.deadflag != DEAD_NO )
			return;

		pObjectEdict = ent;
		iObject = e;
	}
	else
	{
		pObjectEdict = ent->v.aiment;
		iObject = ENTINDEX( pObjectEdict );
	}

	if ( iObject < 1 || iObject > gpGlobals->maxClients || iObject > SEMICLIP_MAX_PLAYERS )
		return;

	if ( pObjectEdict == host || pObjectEdict == pViewPlayer || !Semiclip_IsValidForVisuals( pObjectEdict ) )
		return;

	float flDiff;
	bool bApply;

	if ( pViewPlayer )
	{
		SemiclipPlayer *pPlayer = &s_Players[iView];

		if ( !bActive && !s_bLinger[iView][iObject] )
			return;

		flDiff = pPlayer->flDiff[iObject];
		bApply = pPlayer->bNonSolid[iObject];

		if ( !bApply && pViewPlayer != host )
		{
			flDiff = ( pViewPlayer->v.origin - pObjectEdict->v.origin ).Length2D();
			pPlayer->flDiff[iObject] = flDiff;
			bApply = ( Semiclip_Effects() || flDiff < SEMICLIP_DISTANCE );
		}
	}
	else
	{
		if ( s_bNearestDirty )
			Semiclip_UpdateNearest();

		flDiff = s_flNearestDiff[iObject];
		bApply = s_bNearestActive[iObject];
	}

	if ( !bApply )
		return;

	state->solid = SOLID_NOT;
	state->rendermode = kRenderTransAlpha;

	// mp_semiclip_effects 1: the nearer, the more transparent; renderamt goes to the clients in 8 bits
	if ( Semiclip_Effects() )
		state->renderamt = ( flDiff > SEMICLIP_MIN_AMOUNT ) ? ( ( flDiff < 255.0f ) ? (int)flDiff : 255 ) : (int)SEMICLIP_MIN_AMOUNT;
	else
		state->renderamt = SEMICLIP_TRANSPARENCY;
}
