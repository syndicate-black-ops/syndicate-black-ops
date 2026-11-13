//=========================================================
// SBO 2.0 inventory (docs/INVENTORY.md): item_pickup, player_inventory, player_hasinventory and
// player_calc_ratio's "Inventory item count".
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) dlls/items.cpp
// CItemPickup, dlls/player.cpp CPlayerInventory / CPlayerHasInventory and the player's inventory slots, with the
// multiplayer model of SevenKewp's item_inventory (https://github.com/wootguy/SevenKewp, Half-Life 1 SDK license,
// dlls/item/CItemInventory.cpp): the picked-up item_pickup IS the item. It is hidden while somebody holds it and
// remembers its holder; it drops where its holder died and goes back to where the mapper placed it.
//
// Two lists of INVENTORY_SLOTS items (name + count): one per player (personal), and one map-wide pool (the
// "Shared (whole team)" flag) that every player and the world can use.
//=========================================================

#ifndef INVENTORY_H
#define INVENTORY_H

#include "../game_shared/inventory_shared.h"

class CBasePlayer;

// worldspawn: a new map has no items anywhere
void Inventory_MapStart( void );
// a new client in a slot: an empty personal list
void Inventory_ClientPutInServer( CBasePlayer *pPlayer );

// CBasePlayer::Killed, before anything is stripped: the personal items drop where he died (Keep on death: kept)
void Inventory_PlayerKilled( CBasePlayer *pPlayer );
// CBasePlayer::RemoveAllItems (every strip): his personal items go back to their placed spots, except the ones with
// Keep on death; what player_inventory gave him is gone
void Inventory_Strip( CBasePlayer *pPlayer );
// disconnect, spectator mode, Game Master: every personal item goes back to its placed spot at once
void Inventory_ReturnAll( CBasePlayer *pPlayer );
// ClientDisconnect: the same, without messages to the leaving client
void Inventory_ClientDisconnected( CBasePlayer *pPlayer );
// RoundHooks_Reset, before the round's players spawn: every personal item goes back home, every list is emptied
void Inventory_RoundReset( void );

// count of an item in a player's list (pPlayer NULL: the shared pool); 0 when not held
int Inventory_Count( CBasePlayer *pPlayer, const char *pszItem );

// test layer (SBO_TEST_COMMANDS): give (count > 0) or take (count < 0) an item; FALSE when refused
BOOL Inventory_TestChange( CBasePlayer *pPlayer, const char *pszItem, int iCount );

#endif // INVENTORY_H
