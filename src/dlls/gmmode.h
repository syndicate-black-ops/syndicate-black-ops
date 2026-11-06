// SBO 2.0 Game Master mode (co-op): the Game Master's state, the spawn window's picks, the placement preview, the tools,
// the GM-only display entities and the kill hooks. Port of MrCalvin's Game Master Mode plugin
// (amxmodx-plugins server/syndicate/gmmode, syndicate_gmmode.sma) without its Energy Points, costs, unlock times,
// cooldowns and limits. Entering and leaving stay co-op's ("gmon", CCoop::GameMasterOn / GameMasterOff).
// docs/GM_MODE.md; the shared catalog: game_shared/gm_catalog.h.
#pragma once

class CBasePlayer;
struct clientdata_s;

void GmMode_Init( void );				// GameDLLInit: the gm_toggle server command
void GmMode_StartFrame( void );			// StartFrame: weapon drops and teleporters riding trains / platforms
void GmMode_Precache( void );			// CWorld::Precache (every co-op map, whatever mp_gmmode is)
BOOL GmMode_Allowed( void );			// a co-op map (mp_gmmode gates only a player's own gmon)
BOOL GmMode_Ready( void );				// and the spawn catalog was precached (every co-op map)

// CCoop::GameMasterOn, after the player left the round and got the Game Master flags: an observer where the player
// is, the intro (black screen behind the intro window) until the client closes it ("gm")
void GmMode_Start( CBasePlayer *pPlayer );
// CCoop::GameMasterOff / disconnect: the preview and the tool state go, the speed mark is cleared
void GmMode_Stop( CBasePlayer *pPlayer );
// every Game Master (BeginMissionEnd: the plugin ends GM mode with the mission)
void GmMode_StopAll( void );

void GmMode_Notice( CBasePlayer *pPlayer, const char *pszText );
BOOL GmMode_PipSend( edict_t *pEnt, edict_t *pHost );	// AddToFullPack: near the players window View's player	// "GmNotice": a short line under his crosshair
int GmMode_State( CBasePlayer *pPlayer );	// GM_STATE_* (game_shared/gm_catalog.h)
BOOL GmMode_ClientCommand( CBasePlayer *pPlayer, const char *pcmd );
void GmMode_Think( CBasePlayer *pPlayer );	// CBasePlayer::PreThink, for a Game Master instead of the observer buttons
void GmMode_UpdateClientData( CBasePlayer *pPlayer, struct clientdata_s *cd );

// AddToFullPack: FALSE = don't send this entity to this client (another Game Master's preview)
BOOL GmMode_ShouldSend( int iEntity, edict_t *pHost );
// AddToFullPack, the state filled: the Game Master's selected NPCs glow for that Game Master
void GmMode_AddToFullPack( struct entity_state_s *state, int e, edict_t *host );
// CCoop::RoundWinner: the NPC orders end with the round
void GmMode_RoundEnd( void );
void GmMode_RoundStart( void );			// round start: "Killed by your <monster>" is per round
void GmMode_MenuSelect( CBasePlayer *pPlayer, int iSlot );	// "menuselect" for a Game Master's text menu (TEXTMENU_GM)

// Doors on a Game Master's order (monsters.cpp): the route's capabilities, OpenDoorAndWait, a door in the way
class CBaseMonster;
class CBaseEntity;
BOOL GmMode_IsOrdered( CBaseMonster *pMonster );
int GmMode_RouteCapability( CBaseMonster *pMonster, int afCapability );
BOOL GmMode_MayOpenDoor( CBaseMonster *pMonster, CBaseEntity *pDoor );
BOOL GmMode_OpenDoorInWay( CBaseMonster *pMonster, CBaseEntity *pBlocker );

// CCoop::PlayerKilled: a Game Master's monster killed a player (the level every two kills)
void GmMode_PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
// The kill feed's killer for a Game Master's monster ("<GM>'s <monster>"), NULL for anything else
const char *GmMode_KillerName( entvars_t *pKiller, entvars_t *pInflictor, char *pszBuf, int iBufLen );
int GmMode_KillerOwner( entvars_t *pKiller, entvars_t *pInflictor );	// the Game Master (player index) whose monster it is, 0 none
