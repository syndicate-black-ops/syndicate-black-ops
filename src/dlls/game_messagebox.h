//=========================================================
// game_messagebox: a message box shown to players, closed by the player with the primary attack button.
// docs/MESSAGE_BOX.md (mapper guide, message format).
//
// Port of halflife-featureful's game_messagebox (FreeSlave/halflife-featureful 0542b0d, HL SDK license):
// dlls/maprules.cpp CGameMessageBox, the player's box list (dlls/player.cpp AddMessageBox / CloseMessageBox /
// ClearMessageBoxByIndex / RemoveMessageBoxGaps, the autoclose check in PostThink, the resend on HUD init) and the
// "close_messagebox" client command (dlls/client.cpp). SBO keeps the boxes of every player slot here instead of in
// CBasePlayer, sends the entity's own "message" text (featureful: a message ID of its messages.json) in as many
// user messages as it needs, and does not save open boxes (multiplayer only).
//=========================================================

#ifndef GAME_MESSAGEBOX_H
#define GAME_MESSAGEBOX_H

class CBasePlayer;

#define MSGBOX_MAX_PLAYERS		32
#define MSGBOX_MAX_BOXES		8		// featureful MAX_MESSAGE_BOXES: open boxes per player (a 9th drops the oldest)

// game_messagebox spawnflags
#define SF_MESSAGEBOX_ALLPLAYERS	1	// every player; default: the activator (a player)

// The player's open boxes: the client's HUD was (re)initialized (InitHUD after a connect / map change, a "fullupdate")
void MsgBox_SendAll( CBasePlayer *pPlayer );
// "close_messagebox <id>": the player closed his newest box. True if the command was this one.
BOOL MsgBox_ClientCommand( CBasePlayer *pPlayer, const char *pcmd );
// "lastinv", "use <weapon>", "weapon_<name>" while the player has an open box: ignore the command
BOOL MsgBox_BlocksWeaponCommand( CBasePlayer *pPlayer, const char *pcmd );
// Autoclose ("distance"): every server frame
void MsgBox_StartFrame( void );
void MsgBox_ClientPutInServer( CBasePlayer *pPlayer );
void MsgBox_ClientDisconnected( edict_t *pClient );
void MsgBox_ServerDeactivate( void );

#endif // GAME_MESSAGEBOX_H
