//=========================================================
// SBO 2.0 env_overlay: a picture (.spr, animated or not, or .tga) or a plain color drawn on the screens of players in
// play, under the HUD. docs/ENV_OVERLAY.md (mapper guide, message format for plugins); message format in
// game_shared/overlay_shared.h.
//=========================================================

#ifndef ENV_OVERLAY_H
#define ENV_OVERLAY_H

class CBasePlayer;

// env_overlay spawnflags
#define SF_OVERLAY_ALLPLAYERS	1	// every player in play; default: the activator (a player in play)
#define SF_OVERLAY_FULLSCREEN	2	// stretched over the whole screen
#define SF_OVERLAY_STARTON		4	// on when the map starts (for every player in play)
#define SF_OVERLAY_LOOP			8	// fade in, hold, fade out, again, until turned off

// The player's overlays again: the client's HUD was (re)initialized (InitHUD, a "fullupdate")
void Overlay_SendAll( CBasePlayer *pPlayer );
// Every server frame: players who left play lose their overlays, players who entered play get the ones that are on
// for everybody, timed overlays that ended are forgotten
void Overlay_StartFrame( void );
void Overlay_ClientPutInServer( CBasePlayer *pPlayer );
void Overlay_ClientDisconnected( edict_t *pClient );
void Overlay_ServerDeactivate( void );

#endif // ENV_OVERLAY_H
