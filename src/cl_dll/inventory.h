//=========================================================
// SBO 2.0 inventory, client side (inventory.cpp): the "Inventory" message and the HUD row of the local player's
// items and the team's shared items. docs/INVENTORY.md.
//=========================================================

#if !defined( INVENTORY_CLIENT_H )
#define INVENTORY_CLIENT_H

void Inventory_Init( void );			// HUD_Init: hooks the message
void Inventory_Reset( void );			// InitHUD (new server or map): no items until the server sends them
void Inventory_VidInit( void );			// hud.txt's sprites loaded again (the items stay: a new map clears them at InitHUD)
void Inventory_Redraw( float flTime );	// HUD_Redraw, with the HUD elements

#endif // INVENTORY_CLIENT_H
