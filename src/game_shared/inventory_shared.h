//=========================================================
// SBO 2.0 inventory (docs/INVENTORY.md): the "Inventory" user message, server dlls/inventory.cpp, client
// cl_dll/inventory.cpp (HUD row) and cl_dll/inventory_core.cpp (the message and the slot lists). One reliable
// message per change of one item's count; the whole state again after the client's HUD was (re)initialised (the
// send queue, dlls/client_sendqueue.h).
//
//   SHORT  count: 0 .. INVENTORY_COUNT_MAX (0 = the item is gone from that list)
//   STRING item name: 1 .. INVENTORY_NAME_LEN - 1 characters
//   STRING HUD icon: a sprite name of sprites/hud.txt, at most INVENTORY_NAME_LEN - 1 characters ("" = the
//          default icon, INVENTORY_DEFAULT_ICON)
//   BYTE   INVENTORY_FLAG_* flags
//
// The client keeps two lists (the player's own items and the team's shared items) of INVENTORY_SLOTS items each.
//=========================================================

#ifndef INVENTORY_SHARED_H
#define INVENTORY_SHARED_H

#define INVENTORY_MSG_NAME		"Inventory"

#define INVENTORY_SLOTS			16		// different items per player and in the shared pool
#define INVENTORY_NAME_LEN		32		// item names and icon names: at most 31 characters
#define INVENTORY_COUNT_MAX		32767	// an item's count

#define INVENTORY_FLAG_SHARED	1		// the team's shared pool (else the player's own items)

#define INVENTORY_DEFAULT_ICON	"inv_key"

// the longest message: count + two strings with their ends + flags
#define INVENTORY_MSG_MAX_BYTES	( 2 + INVENTORY_NAME_LEN + INVENTORY_NAME_LEN + 1 )

#endif // INVENTORY_SHARED_H
