/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
//  cdll_dll.h

// this file is included by both the game-dll and the client-dll,

#ifndef CDLL_DLL_H
#define CDLL_DLL_H

// SBO 2.1 (Half-Life Unified SDK, HLU #98): weapon ids 0..63. The engine's local_state_t has 64 weapon_data_t entries.
// A player's weapons: ids 0..30 and the suit (31) in pev->weapons, ids 32..63 in CBasePlayer::m_iWeaponBitsHigh.
#define MAX_WEAPONS		64

// SBO 2.1 (Half-Life Unified SDK): 10 weapon slots (categories), one per number key; slots above 5 are drawn only
// when the player holds a weapon there or in a higher one. A player's item buckets are indexed by iItemSlot (1..10).
#define MAX_WEAPON_SLOTS		10	// hud item selection slots
#define MAX_ALWAYS_VISIBLE_WEAPON_SLOTS	5	// the slots the weapon menu always shows
#define MAX_ITEM_TYPES			11	// hud item selection slots

#define MAX_ITEMS				5	// hard coded item types

#define	HIDEHUD_WEAPONS		( 1<<0 )
#define	HIDEHUD_FLASHLIGHT	( 1<<1 )
#define	HIDEHUD_ALL			( 1<<2 )
#define 	HIDEHUD_HEALTH		( 1<<3 )

#define	MAX_AMMO_TYPES	32		// ???
#define MAX_AMMO_SLOTS  32		// not really slots

#define HUD_PRINTNOTIFY		1
#define HUD_PRINTCONSOLE	2
#define HUD_PRINTTALK		3
#define HUD_PRINTCENTER		4


#define WEAPON_SUIT			31

#endif