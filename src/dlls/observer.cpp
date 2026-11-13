//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Functionality for the observer chase camera
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
// observer.cpp
//
// SBO 1.38 shipped the older Ricochet-style observer: only OBS_CHASE_LOCKED, OBS_CHASE_FREE and OBS_ROAMING, no
// "specmode"/"follownext" commands, no first-person data. The six modes are back as in the halflife SDK 2.3
// (dlls/observer.cpp, halflife-updated), with ReGameDLL_CS's target checks (Observer_IsValidTarget, the
// m_bWasFollowing re-follow) and BugfixedHL-Rebased's bounded target search. docs/SPECTATOR.md has the details.
//
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"pm_shared.h"
#include	"gamerules.h"
#include	"coop.h"
#include	"br.h"
#include	"dm.h"
#include	"inventory.h"

extern DLL_GLOBAL BOOL g_fGameOver;
extern void respawn( entvars_t *pev, BOOL fCopyCorpse );
extern int gmsgCurWeapon;
extern int gmsgSetFOV;
extern int gmsgTeamInfo;
extern int gmsgSpectator;
extern int gmsgSpecHealth2;

// Player has become a spectator. Set it up.
// This was moved from player.cpp.
void CBasePlayer::StartObserver( Vector vecPosition, Vector vecViewAngle )
{
	// clear any clientside entities attached to this player
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_KILLPLAYERATTACHMENTS );
		WRITE_BYTE( (BYTE)entindex() );
	MESSAGE_END();

	// Holster weapon immediately, to allow it to cleanup
	if (m_pActiveItem)
		m_pActiveItem->Holster( );

	// SBO 2.0: a living player can get here too (spectator mode), and PreThink returns for observers before the
	// code that lets go of train / vehicle controls: func_tank, train, vehicle and the train HUD (player.cpp)
	ReleaseControls();

	// SBO 2.0: and of a trigger_camera's view, which the camera gives back to living players only (player.cpp). The
	// camera's FL_FROZEN (SF_CAMERA_PLAYER_TAKECONTROL) is cleared below.
	ResetViewEntity();

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, FALSE, 0);

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(0XFF);
		WRITE_BYTE(0xFF);
	MESSAGE_END();

	// reset FOV
	m_iFOV = m_iClientFOV = 0;
	pev->fov = m_iFOV;
	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
		WRITE_BYTE(0);
	MESSAGE_END();

	// SBO 2.0: drop the morph powers as CBasePlayer::Killed does (a living player starting spectator mode kept the
	// cybereye's screen tint and its light). Before PFLAG_OBSERVER is set: Cloak / Cybereye do nothing for observers.
	if ( m_iCloak == 1 )
		Cloak();
	if ( m_iCybereye == 1 )
		Cybereye();

	// Setup flags
	m_iHideHUD = (HIDEHUD_HEALTH | HIDEHUD_WEAPONS | HIDEHUD_FLASHLIGHT);	// SBO 2.0: no flashlight HUD either (PreThink)
	m_afPhysicsFlags |= PFLAG_OBSERVER;

	// SBO 2.0: silence the geiger counter now. CheckSuitUpdate (UpdateGeigerCounter) isn't run for observers, so the
	// client kept clicking with the last range sent. Precache (Spawn) makes the next spawn send the range again.
	m_flgeigerDelay = 0;
	UpdateGeigerCounter();
	pev->effects = EF_NODRAW;
	pev->view_ofs = g_vecZero;
	pev->angles = pev->v_angle = vecViewAngle;
	pev->fixangle = TRUE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NONE;
	// SBO 2.0: never frozen. PreThink freezes SBO's waiting players (PFLAG_OBSERVER without a mode in iuser1:
	// InitHUD, "Waiting for a carnage") and only unfreezes after its observer branch, which returns early once a mode
	// is set. A dead player gets here through CBasePlayer::Spawn, which resets pev->flags; a waiting player who starts
	// spectator mode comes here directly and kept FL_FROZEN, which zeroes the movement in PM_CheckParamters (no roaming)
	ClearBits( pev->flags, FL_FROZEN );
	ClearBits( m_afPhysicsFlags, PFLAG_DUCKING );
	ClearBits( pev->flags, FL_DUCKING );
	pev->deadflag = DEAD_RESPAWNABLE;
	pev->health = 1;
	
	// Clear out the status bar
	m_fInitHUD = TRUE;

	pev->team =  0;

	// Update Team Status
	if ( SboScoreboard_Active() )
	{
		// chat-vgui2: SBO modes keep observers on the scoreboard in their category (Dead Ops, No Loadout, ...)
		SboScoreboard_Update( this, SBO_SB_FORCE_TEAMINFO );
	}
	else
	{
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( ENTINDEX(edict()) );
		WRITE_STRING( "" );
	MESSAGE_END();
	}

	// Remove all the player's stuff
	RemoveAllItems( FALSE );

	// Move them to the new position
	UTIL_SetOrigin( pev, vecPosition );

	// Find a player to watch
	m_flNextObserverInput = 0;
	m_iObserverWeapon = 0;
	m_iSpecInfoTarget = -1;	// SBO 2.0: nothing sent to this client's spectator panel yet (Observer_SendTargetInfo)

	// ReGameDLL_CS: start over from "no mode", so Observer_SetMode doesn't skip the mode a restarted observer is
	// still in, then use the player's last mode (the halflife SDK 2.3; 1.38 always used OBS_CHASE_LOCKED)
	pev->iuser1 = OBS_NONE;
	Observer_SetMode( m_iObserverLastMode );

	// Tell all clients this player is now a spectator
	MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );  
		WRITE_BYTE( ENTINDEX( edict() ) );
		WRITE_BYTE( 1 );
	MESSAGE_END();
}

// Leave observer mode
void CBasePlayer::StopObserver( void )
{
	// Turn off spectator
	if ( pev->iuser1 || pev->iuser2 )
	{
		// Tell all clients this player is not a spectator anymore
		MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );  
			WRITE_BYTE( ENTINDEX( edict() ) );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	// ReGameDLL_CS CBasePlayer::Spawn: no observer state may survive a (re)spawn, else the client keeps the
	// spectator view of the old target. CBasePlayer::Spawn calls this before the mode's PlayerSpawn.
	pev->iuser1 = pev->iuser2 = pev->iuser3 = 0;
	m_hObserverTarget = NULL;
	m_iObserverWeapon = 0;
	m_bWasFollowing = FALSE;
	ResetViewEntity();

	// SBO 2.0: the view model is the player's own again, with its own "ViewModel" state
	if ( m_iViewModelRenderFrom )
	{
		m_iViewModelRenderFrom = 0;
		SendViewModelRender( this );
	}


	// chat-vgui2: no scoreboard update here; StopObserver runs inside the modes' spawn paths, and Spawn() updates
	// the category once the mode is done (SboScoreboard_Update after PlayerSpawn)
}

// ReGameDLL_CS CBasePlayer::Observer_IsValidTarget: only live players that are in the game can be watched
CBasePlayer *CBasePlayer::Observer_IsValidTarget( int iPlayerIndex )
{
	if ( iPlayerIndex < 1 || iPlayerIndex > gpGlobals->maxClients )
		return NULL;

	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( iPlayerIndex );

	if ( !pPlayer || pPlayer == this )
		return NULL;

	if ( pPlayer->has_disconnected )
		return NULL;

	// Don't spec observers, SBO's waiting players (PFLAG_OBSERVER / m_bOutOfPlay without a spectator mode) or
	// invisible players
	if ( pPlayer->IsObserver() || ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER ) || pPlayer->m_bOutOfPlay )
		return NULL;

	if ( pPlayer->pev->effects & EF_NODRAW )
		return NULL;

	// MOD AUTHORS: Add checks on target here.
	if ( !pPlayer->IsAlive() )
		return NULL;

	return pPlayer;
}

// SBO 2.0: a target that is no player was set by a plugin (m_hObserverTarget, e.g. a sentry gun's camera). The game
// never picks one itself, it keeps it in every mode while the entity exists, as long as the plugin leaves it there.
// The player-only parts (client data, weapon, FOV, the target panel) are skipped for it.
static BOOL Observer_IsEntityTarget( CBasePlayer *pObserver, CBaseEntity *pTarget )
{
	if ( !pTarget || pTarget == pObserver || pTarget->IsPlayer() )
		return FALSE;

	const int iIndex = ENTINDEX( pTarget->edict() );

	if ( iIndex <= gpGlobals->maxClients )
		return FALSE;

	return !( pTarget->pev->flags & FL_KILLME );
}

// Find the next client in the game for this player to spectate
void CBasePlayer::Observer_FindNextPlayer( bool bReverse )
{
	// MOD AUTHORS: Modify the logic of this function if you want to restrict the observer to watching
	//				only a subset of the players. e.g. Make it check the target's team.

	// BugfixedHL-Rebased: start from our own index if the target is no player, so the search below always ends
	// (the SDK loop never gets back to an iStart above maxClients)
	int		iStart = 0;
	if ( m_hObserverTarget )
		iStart = ENTINDEX( m_hObserverTarget->edict() );
	if ( iStart < 1 || iStart > gpGlobals->maxClients )
		iStart = ENTINDEX( edict() );
	int	    iCurrent = iStart;
	m_hObserverTarget = NULL;
	int iDir = bReverse ? -1 : 1; 

	do
	{
		iCurrent += iDir;

		// Loop through the clients
		if (iCurrent > gpGlobals->maxClients)
			iCurrent = 1;
		if (iCurrent < 1)
			iCurrent = gpGlobals->maxClients;

		m_hObserverTarget = Observer_IsValidTarget( iCurrent );
		if ( m_hObserverTarget )
			break;

	} while ( iCurrent != iStart );

	// Did we find a target?
	if ( m_hObserverTarget )
	{
		// Move to the target
		UTIL_SetOrigin( pev, m_hObserverTarget->pev->origin );

		// Store the target in pev so the physics DLL can get to it
		if ( pev->iuser1 != OBS_ROAMING )
			pev->iuser2 = ENTINDEX( m_hObserverTarget->edict() );
	}
}

// Handle buttons in observer mode
void CBasePlayer::Observer_HandleButtons()
{
	// Slow down mouse clicks
	if ( m_flNextObserverInput > gpGlobals->time )
		return;

	// Jump changes from modes: Chase to Roaming
	if ( m_afButtonPressed & IN_JUMP )
	{
		if ( pev->iuser1 == OBS_CHASE_LOCKED )
			Observer_SetMode( OBS_CHASE_FREE );

		else if ( pev->iuser1 == OBS_CHASE_FREE )
			Observer_SetMode( OBS_IN_EYE );

		else if ( pev->iuser1 == OBS_IN_EYE )
			Observer_SetMode( OBS_ROAMING );

		else if ( pev->iuser1 == OBS_ROAMING )
			Observer_SetMode( OBS_MAP_FREE );

		else if ( pev->iuser1 == OBS_MAP_FREE )
			Observer_SetMode( OBS_MAP_CHASE );

		else
			Observer_SetMode( OBS_CHASE_LOCKED );	// SBO 2.0: back to the locked chase, as production's observer plugin (the SDK: OBS_CHASE_FREE)

		m_flNextObserverInput = gpGlobals->time + 0.2;
	}

	// Attack moves to the next player
	if ( m_afButtonPressed & IN_ATTACK )
	{
		Observer_FindNextPlayer( false );

		m_flNextObserverInput = gpGlobals->time + 0.2;
	}

	// Attack2 moves to the prev player
	if ( m_afButtonPressed & IN_ATTACK2 )
	{
		Observer_FindNextPlayer( true );

		m_flNextObserverInput = gpGlobals->time + 0.2;
	}
}

// halflife SDK 2.3 Observer_CheckTarget, with ReGameDLL_CS's checks: retarget when the watched player died, left,
// became an observer or went invisible, and follow again once there is a target after having lost it
void CBasePlayer::Observer_CheckTarget()
{
	if ( pev->iuser1 == OBS_ROAMING && !m_bWasFollowing )
		return;

	if ( m_bWasFollowing )
	{
		// lost the target before: back to the last mode as soon as anybody can be watched again
		Observer_FindNextPlayer( false );

		if ( m_hObserverTarget )
			Observer_SetMode( m_iObserverLastMode );

		return;
	}

	// SBO 2.0: a plugin's entity target stays (Observer_IsEntityTarget)
	if ( m_hObserverTarget != NULL && !( m_hObserverTarget->IsPlayer() ) )
	{
		if ( Observer_IsEntityTarget( this, m_hObserverTarget ) )
			return;

		m_hObserverTarget = NULL;
	}

	// try to find a target if we have no current one
	if ( m_hObserverTarget == NULL )
		Observer_FindNextPlayer( false );

	if ( m_hObserverTarget == NULL )
	{
		// SBO 2.0: the free overview needs no target (to look over the map with nobody to watch): stay in it
		if ( pev->iuser1 == OBS_MAP_FREE )
		{
			pev->iuser2 = 0;
			return;
		}

		// no target found at all
		int lastMode = pev->iuser1;

		Observer_SetMode( OBS_ROAMING );

		m_iObserverLastMode = lastMode;	// don't overwrite users lastmode
		m_bWasFollowing = TRUE;

		return;
	}

	CBasePlayer *target = (CBasePlayer *)UTIL_PlayerByIndex( ENTINDEX( m_hObserverTarget->edict() ) );

	if ( !target || target->has_disconnected || target->IsObserver() || ( target->m_afPhysicsFlags & PFLAG_OBSERVER ) )
	{
		Observer_FindNextPlayer( false );
		return;
	}

	if ( !Observer_IsValidTarget( ENTINDEX( target->edict() ) ) )
	{
		// ReGameDLL_CS: keep watching the victim while it dies (even if gibbed), 2 seconds after death change target
		BOOL bStillDying = ( target->pev->deadflag == DEAD_DYING ) ||
			( target->pev->deadflag == DEAD_DEAD && ( target->m_fDeadTime + 2.0 ) > gpGlobals->time );

		if ( !bStillDying )
			Observer_FindNextPlayer( false );
	}
}

//=========================================================
// SBO 2.0: "ViewModel" (CBasePlayer::SetViewModelRender): in first person the client draws the watched player's
// weapon, so it gets the watched player's state, again whenever that changes. Only a state the game set is sent:
// with none (or out of first person) the spectator's client keeps what it had before, also a plugin's, and gets
// its own state back only if the watched player's had replaced it.
//=========================================================
void CBasePlayer::Observer_CheckViewModelRender()
{
	CBasePlayer *target = NULL;

	if ( pev->iuser1 == OBS_IN_EYE && m_hObserverTarget != NULL )
	{
		target = (CBasePlayer *)UTIL_PlayerByIndex( ENTINDEX( m_hObserverTarget->edict() ) );

		if ( target && !target->m_bViewModelRender )
			target = NULL;
	}

	if ( target )
	{
		const int iTarget = ENTINDEX( target->edict() );

		if ( m_iViewModelRenderFrom != iTarget || m_iViewModelRenderFromSerial != target->m_iViewModelRenderSerial )
		{
			m_iViewModelRenderFrom = iTarget;
			m_iViewModelRenderFromSerial = target->m_iViewModelRenderSerial;
			SendViewModelRender( target );
		}
	}
	else if ( m_iViewModelRenderFrom )
	{
		m_iViewModelRenderFrom = 0;
		SendViewModelRender( this );
	}
}

// halflife SDK 2.3 Observer_CheckProperties: in first person use the target's FOV and show its weapon
void CBasePlayer::Observer_CheckProperties()
{
	Observer_CheckViewModelRender();	// SBO 2.0

	// try to find a traget if we have no current one
	if ( pev->iuser1 == OBS_IN_EYE && m_hObserverTarget != NULL )
	{
		CBasePlayer *target = (CBasePlayer *)UTIL_PlayerByIndex( ENTINDEX( m_hObserverTarget->edict() ) );

		if ( !target )
			return;

		int weapon = ( target->m_pActiveItem != NULL ) ? target->m_pActiveItem->m_iId : 0;
		// use fov of tracked client
		if ( m_iFOV != target->m_iFOV || m_iObserverWeapon != weapon )
		{
			m_iFOV = target->m_iFOV;
			m_iClientFOV = m_iFOV;
			// write fov before wepon data, so zoomed crosshair is set correctly
			MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
				WRITE_BYTE( m_iFOV );
			MESSAGE_END();

			m_iObserverWeapon = weapon;
			//send weapon update
			MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
				WRITE_BYTE( 1 );	// 1 = current weapon, not on target
				WRITE_BYTE( m_iObserverWeapon );
				WRITE_BYTE( 0 );	// clip
			MESSAGE_END();
		}
	}
	else
	{
		// SBO 2.0: 0, the spectator's own FOV (default_fov), as in first person on an unzoomed player (the SDK: 90).
		// Over a map overview the watched player's FOV, which the client's first-person inset uses (the overview
		// itself keeps its own view: client CHud::Think, V_GetMapFreePosition); UpdateClientData sends it (SetFOV)
		CBasePlayer *target = NULL;
		if ( ( pev->iuser1 == OBS_MAP_FREE || pev->iuser1 == OBS_MAP_CHASE ) && m_hObserverTarget != NULL )
			target = (CBasePlayer *)UTIL_PlayerByIndex( ENTINDEX( m_hObserverTarget->edict() ) );
		m_iFOV = target ? target->m_iFOV : 0;

		if ( m_iObserverWeapon != 0 )
		{
			m_iObserverWeapon = 0;

			MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
				WRITE_BYTE( 1 );	// 1 = current weapon
				WRITE_BYTE( m_iObserverWeapon );
				WRITE_BYTE( 0 );	// clip
			MESSAGE_END();
		}
	}
}

//=========================================================
// SBO 2.0: the watched player's state for the spectator panel (client: cl_dll/vgui2/sbo_spectator_info.cpp), the
// native version of what the metamod-spectator-info plugin printed as HUD text. "SpecHealth2" (Counter-Strike's
// name for its health-of-the-target message; SBO's carries more): byte player, short health, short armor, byte
// weapon id (WeaponList id, 0 = none), byte flags (SPECINFO_*), byte BR rounds won on this map (0 outside BR), short
// view model (model index of the target's pev->viewmodel, 0 = none). Sent to this observer only, and only when the target
// or one of the values changed since the last one; the client keeps the last one. The rest of the panel (name,
// level / points, XP) is what every client already has for the scoreboard.
// Nothing is sent while there is no target (roaming): the client shows the panel only for the player in its
// iuser2, and what it holds for a player stays what was sent last.
//=========================================================
void CBasePlayer::Observer_SendTargetInfo()
{
	if ( pev->iuser1 == OBS_ROAMING || m_hObserverTarget == NULL )
		return;

	int iTarget = ENTINDEX( m_hObserverTarget->edict() );
	CBasePlayer *target = (CBasePlayer *)UTIL_PlayerByIndex( iTarget );

	if ( !target )
		return;

	// (a future plugin API that hides a player's state from spectators belongs here: return, or send blank values)

	// as the player's own Health / Battery messages (CBasePlayer::UpdateClientData): 1 while alive below 1 hp
	int iHealth = (int)( target->pev->health < 0.0f ? 0.0f : ( target->pev->health > 32767.0f ? 32767.0f : target->pev->health ) );
	if ( target->pev->health > 0.0f && target->pev->health <= 1.0f )
		iHealth = 1;
	int iArmor = (int)( target->pev->armorvalue < 0.0f ? 0.0f : ( target->pev->armorvalue > 32767.0f ? 32767.0f : target->pev->armorvalue ) );
	int iWeapon = ( target->m_pActiveItem != NULL ) ? target->m_pActiveItem->m_iId : 0;
	int iFlags = 0;

	// broken legs are the speed they leave (player.h SBO_LEGS_BROKEN_SPEED); 0: a mode that never set the speed
	if ( target->IsAlive() && target->m_iSboMaxSpeed > 0 && target->m_iSboMaxSpeed < SBO_LEGS_BROKEN_SPEED )
		iFlags |= SPECINFO_LEGS_BROKEN;

	int iBrWins = target->m_iBrRoundWins > 255 ? 255 : target->m_iBrRoundWins;

	// the target's view model, as UpdateClientData sends it in first person: the client's first-person inset over
	// another main view draws it (view.cpp V_CalcSpectatorRefdef). The SDK's client guessed it from the player's
	// weapon model ("p_" -> "v_"), which a plugin that draws the third-person weapon itself leaves empty (production's
	// packed_models_sbo), so the inset had no weapon. The last one sent is kept per observer here, not in the player
	// class (no class layout change for the AMXX gamedata); a new target is always sent (m_iSpecInfoTarget).
	static int s_iSpecInfoViewModel[32 + 1];	// the engine's client limit (MAX_CLIENTS)
	int iViewModel = FStringNull( target->pev->viewmodel ) ? 0 : MODEL_INDEX( STRING( target->pev->viewmodel ) );
	int &iSentViewModel = s_iSpecInfoViewModel[entindex() <= 32 ? entindex() : 0];

	if ( iTarget == m_iSpecInfoTarget && iHealth == m_iSpecInfoHealth && iArmor == m_iSpecInfoArmor
		&& iWeapon == m_iSpecInfoWeapon && iFlags == m_iSpecInfoFlags && iBrWins == m_iSpecInfoBrWins
		&& iViewModel == iSentViewModel )
		return;

	m_iSpecInfoTarget = iTarget;
	m_iSpecInfoHealth = iHealth;
	m_iSpecInfoArmor = iArmor;
	m_iSpecInfoWeapon = iWeapon;
	m_iSpecInfoFlags = iFlags;
	m_iSpecInfoBrWins = iBrWins;
	iSentViewModel = iViewModel;

	MESSAGE_BEGIN( MSG_ONE, gmsgSpecHealth2, NULL, pev );
		WRITE_BYTE( iTarget );
		WRITE_SHORT( iHealth );
		WRITE_SHORT( iArmor );
		WRITE_BYTE( iWeapon );
		WRITE_BYTE( iFlags );
		WRITE_BYTE( iBrWins );
		WRITE_SHORT( iViewModel );
	MESSAGE_END();
}

//=========================================================
// SBO 2.0: pPlayer gained a level (the monsters' TakeDamage, medikit.cpp). The "LEVEL UP" HUD text the player gets
// (textparms, pMessage: the caller's) also goes to everybody watching the player in the locked chase, the free chase
// or first person: dead players and players in spectator mode alike. Not in roaming and not in the map overviews,
// whatever the last target was. Production's syndicate_observer_fixes plugin sent it (notify_level_up_watchers; the
// same text and HUD parameters as the game's) to the dead players whose target the player was, in every mode.
//=========================================================
void Observer_LevelUp( CBasePlayer *pPlayer, const hudtextparms_t &textparms, const char *pMessage )
{
	if ( !pPlayer )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pObserver = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !pObserver || pObserver == pPlayer || pObserver->has_disconnected )
			continue;

		if ( pObserver->pev->iuser1 != OBS_CHASE_LOCKED && pObserver->pev->iuser1 != OBS_CHASE_FREE
			&& pObserver->pev->iuser1 != OBS_IN_EYE )
			continue;

		if ( (CBaseEntity *)pObserver->m_hObserverTarget != pPlayer )
			continue;

		UTIL_HudMessage( pObserver, textparms, pMessage );
	}
}

// Attempt to change the observer mode (halflife SDK 2.3)
void CBasePlayer::Observer_SetMode( int iMode )
{
	// Just abort if we're changing to the mode we're already in
	if ( iMode == pev->iuser1 )
		return;

	int iOldMode = pev->iuser1;

	// is valid mode ?
	if ( iMode < OBS_CHASE_LOCKED || iMode > OBS_MAP_CHASE )
		iMode = OBS_IN_EYE; // now it is

	// verify observer target again; SBO 2.0: a plugin's entity target stays (Observer_IsEntityTarget)
	if ( m_hObserverTarget != NULL && !Observer_IsEntityTarget( this, m_hObserverTarget ) )
		m_hObserverTarget = Observer_IsValidTarget( ENTINDEX( m_hObserverTarget->edict() ) );	// ReGameDLL_CS

	// set spectator mode
	pev->iuser1 = iMode;

	// if we are not roaming, we need a valid target to track
	if ( ( iMode != OBS_ROAMING ) && ( m_hObserverTarget == NULL ) )
	{
		Observer_FindNextPlayer( false );

		// if we didn't find a valid target switch to roaming. SBO 2.0: not from the free overview, which needs no
		// target: with nobody to watch the player can still look over the map (the SDK fell back to roaming)
		if ( m_hObserverTarget == NULL && iMode != OBS_MAP_FREE )
		{
			pev->iuser1 = OBS_ROAMING;

			// SBO 2.0: already roaming, so nothing changes: no text, and the last mode and a pending re-follow
			// (m_bWasFollowing) stay. As in 1.38, where Jump with nobody to watch did nothing; the SDK printed
			// "No Target" and the roaming mode's text again on every press.
			if ( iOldMode == OBS_ROAMING )
				return;

			ClientPrint( pev, HUD_PRINTCENTER, "#Spec_NoTarget" );
		}
	}

	// set target if not roaming (none in the free overview with nobody to watch)
	if ( pev->iuser1 == OBS_ROAMING || m_hObserverTarget == NULL )
		pev->iuser2 = 0;
	else
		pev->iuser2 = ENTINDEX( m_hObserverTarget->edict() );

	pev->iuser3 = 0;	// clear second target from death cam

	// print spepctaor mode on client screen, if it changed (an invalid mode number can end in the current mode)

	if ( pev->iuser1 != iOldMode )
	{
		char modemsg[16];
		sprintf( modemsg, "#Spec_Mode%i", pev->iuser1 );
		ClientPrint( pev, HUD_PRINTCENTER, modemsg );
	}

	m_iObserverLastMode = iMode;
	m_bWasFollowing = FALSE;	// ReGameDLL_CS
}

//=========================================================
// SBO 2.0 spectator mode: the player watches the game in the spectator modes above and takes no part in it until
// toggling the mode off again ("spectate", or /spec, /spectate, .spec, .spectate in chat: client.cpp). Spectators
// are no round candidates (IsRoundCandidate, CDm::SetupRound), so they never hold up a round start (nor through
// mp_loadouttime), aren't spawned or stripped by a round; they get no team, go, radio or Game
// Master menus (ClientCommand), and the scoreboard shows them as "Spectators". The selection (weapon) menu stays:
// its "Go !" leaves spectator mode with the loadout shown, "Cancel" doesn't (client.cpp Spectate_MenuConfirmed).
// They move, switch modes and targets like any observer (StartObserver). Like the halflife SDK's and CS's observers they are no targets for other spectators
// (Observer_IsValidTarget: m_bOutOfPlay).
// A living player stops playing without dying: no death, frag or XP penalty, no corpse; the weapons go
// (StartObserver), as for SBO's own observers. docs/SPECTATOR.md, docs/ROUNDS.md.
//=========================================================
static void SpectatorStart_NoTarget( CBasePlayer *pPlayer );

void CBasePlayer::StartSpectating( void )
{
	m_bSpectating = TRUE;

	if ( g_fGameOver )	// intermission: the next map starts the player as a spectator (client.cpp keeps the mode)
		return;

	// leave the round before the state changes below: the round code reads whether the player was alive
	// (m_bOutOfPlay). Spectators count for no game metric: co-op's round goes on as if the player had never been in
	// it (also mp_hardcore's base, unlike a disconnect), BR's as on a disconnect (no death limit there), the hunted
	// hands the bounty round back.
	Inventory_ReturnAll( this );	// SBO 2.0 inventory: a spectator holds no items; his go back to their placed spots

	switch ( m_Mode )
	{
	case MODE_COOP:
		((CCoop *)g_pGameRules)->PlayerStartsSpectating( this );

		break;
	case MODE_BR:
		((CBr *)g_pGameRules)->PlayerStartsSpectating( this );
		break;
	case MODE_DM:
		((CDm *)g_pGameRules)->PlayerStartsSpectating( this );
		break;
	}

	if ( m_iSpawnMode == 2 )
		m_iSpawnMode = 1;	// no longer in play: the modes' PlayerSpawn would spawn a live player
	m_bOutOfPlay = 1;	// the modes' "not in the round" flag

	// from where the player stands (dead, dying, waiting or alive); someone already in a spectator mode (a dead
	// player's camera) keeps it and the target. A waiting player (joining or reconnecting, in the menus: an observer
	// without a mode, not dead) with nobody to watch starts at an info_intermission point (SpectatorStart_NoTarget).
	if ( !IsObserver() )
	{
		const BOOL bWaiting = ( m_afPhysicsFlags & PFLAG_OBSERVER ) && pev->deadflag == DEAD_NO;

		StartObserver( pev->origin + pev->view_ofs, pev->v_angle );

		if ( bWaiting )
			SpectatorStart_NoTarget( this );
	}

	SboScoreboard_Update( this, SBO_SB_FORCE_TEAMINFO );	// "Spectators"
}

void CBasePlayer::StopSpectating( void )
{
	m_bSpectating = FALSE;

	if ( g_fGameOver )	// intermission: the next map starts the player as usual
		return;

	// back to the game the way the mode lets a player join: co-op and BR wait for the next round (with the loadout
	// / ready state the player had, else through the mode's menus), Bounty Killer spawns at once
	switch ( m_Mode )
	{
	case MODE_COOP:
		((CCoop *)g_pGameRules)->PlayerStopsSpectating( this );
		break;
	case MODE_BR:
		((CBr *)g_pGameRules)->PlayerStopsSpectating( this );
		break;
	case MODE_DM:
		((CDm *)g_pGameRules)->PlayerStopsSpectating( this );
		break;
	default:	// teamplay (sp_): no rounds, spawn as after a death
		respawn( pev, FALSE );
		break;
	}

	SboScoreboard_Update( this, SBO_SB_FORCE_TEAMINFO );
}

//=========================================================
// SBO 2.0: a dead or waiting player confirmed the loadout menu (its "item" step, CCoop / CDm::ClientCommand). Called
// from UpdateClientData (PreThink) on the player's next move, as Spectate_MenuConfirmed is: the commands the menu
// sends after "item" (cyberware, armor, malefemale) come in ahead of it, so the spawn has the whole new loadout.
//=========================================================
void CBasePlayer::LoadoutJoin( void )
{
	m_bLoadoutJoinPending = FALSE;

	switch ( m_Mode )
	{
	case MODE_COOP:
		((CCoop *)g_pGameRules)->PlayerLoadoutJoins( this );
		break;
	case MODE_DM:
		((CDm *)g_pGameRules)->PlayerLoadoutJoins( this );
		break;
	}
}


//=========================================================
// SBO 2.0 spectator start: a map's info_intermission points (the halflife SDK's and DMC's intermission spots) are
// where a joining player, a waiting player and a new spectator start watching when there is nobody to watch, as the
// map's trigger_camera is CS's intro view behind the team menu (ReGameDLL_CS ClientPutInServer). Each start picks one
// of the points at random; the view looks at the point's target, else along its angles. With a player to watch, or
// on a map without a point, the start stays as it was. Dying changes nothing: the death camera keeps its 1.38 use of
// the points (StartDeathCam: the first four, the angles the point turned to its target), and the modes' observers
// after a death (FPlayerCanRespawn) start at a spawn point. docs/SPECTATOR.md "Spectator start".
//=========================================================
static BOOL SpectatorStart_Pick( Vector &vecOrigin, Vector &vecViewAngle )
{
	CBaseEntity *pSpot = NULL;
	int iCount = 0;

	while ( ( pSpot = UTIL_FindEntityByClassname( pSpot, "info_intermission" ) ) != NULL )
		iCount++;

	if ( !iCount )
		return FALSE;

	const int iPick = RANDOM_LONG( 0, iCount - 1 );

	pSpot = NULL;
	for ( int i = 0; i <= iPick; i++ )
		pSpot = UTIL_FindEntityByClassname( pSpot, "info_intermission" );

	if ( !pSpot )
		return FALSE;

	vecOrigin = pSpot->pev->origin;

	CBaseEntity *pTarget = FStringNull( pSpot->pev->target ) ? NULL
		: UTIL_FindEntityByTargetname( NULL, STRING( pSpot->pev->target ) );

	if ( pTarget )
	{
		// as CInfoIntermission::Think: the view's pitch is the vector's, inverted
		vecViewAngle = UTIL_VecToAngles( ( pTarget->Center() - vecOrigin ).Normalize() );
		vecViewAngle.x = -vecViewAngle.x;
		if ( vecViewAngle.x < -180 )
			vecViewAngle.x += 360;	// UTIL_VecToAngles gives 0..360: 45 down, not -315
	}
	else
	{
		vecViewAngle = pSpot->pev->angles;	// the "angles" key as the editor shows it: pitch (down positive), yaw
	}

	vecViewAngle.z = 0;
	return TRUE;
}

// nobody to watch: no player an observer could follow (Observer_IsValidTarget: alive and in play)
static BOOL SpectatorStart_NobodyToWatch( CBasePlayer *pPlayer )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		if ( pPlayer->Observer_IsValidTarget( i ) )
			return FALSE;
	}

	return TRUE;
}

// a joining player (the co-op, BR and Bounty Killer InitHUD) waits in the menus without a spectator mode, at the
// spawn point CBasePlayer::Spawn put it on; with nobody to watch, its eyes go on a picked point instead (the view is
// origin + view_ofs until a spectator mode starts)
void SpectatorStart_Joining( CBasePlayer *pPlayer )
{
	Vector vecOrigin, vecViewAngle;

	if ( !SpectatorStart_NobodyToWatch( pPlayer ) || !SpectatorStart_Pick( vecOrigin, vecViewAngle ) )
		return;

	UTIL_SetOrigin( pPlayer->pev, vecOrigin - pPlayer->pev->view_ofs );
	// held at the point as CS holds its intro view (ReGameDLL_CS ClientPutInServer: MOVETYPE_NOCLIP): the player
	// movement code turns a living player's MOVETYPE_NONE into MOVETYPE_WALK (PM_PlayerMove), and a walking waiting
	// player falls from the point to the floor. PreThink's FL_FROZEN zeroes the input; a spawn sets MOVETYPE_WALK.
	pPlayer->pev->movetype = MOVETYPE_NOCLIP;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->punchangle = g_vecZero;
	pPlayer->pev->angles = pPlayer->pev->v_angle = vecViewAngle;
	pPlayer->pev->fixangle = TRUE;
}

// an observer that StartObserver found no target for (StartSpectating): to a picked point
static void SpectatorStart_NoTarget( CBasePlayer *pPlayer )
{
	Vector vecOrigin, vecViewAngle;

	if ( pPlayer->m_hObserverTarget != NULL || !SpectatorStart_Pick( vecOrigin, vecViewAngle ) )
		return;

	UTIL_SetOrigin( pPlayer->pev, vecOrigin );
	pPlayer->pev->angles = pPlayer->pev->v_angle = vecViewAngle;
	pPlayer->pev->fixangle = TRUE;
}
