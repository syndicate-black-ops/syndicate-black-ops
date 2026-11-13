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
// ammohistory.h
//

#include <stdint.h>
#include <vector>

// SBO 2.1 (Half-Life Unified SDK): a bucket holds any number of weapons, at any position 0..255 (WeaponList's byte),
// kept in position order; positions need not follow each other

class WeaponsResource
{
private:
	// Information about weapons & ammo
	WEAPON		rgWeapons[MAX_WEAPONS];	// Weapons Array

	// counts of weapons * ammo
	std::vector<WEAPON*>	rgSlots[MAX_WEAPON_SLOTS];	// the weapons held in each slot (category), in position order
	int			riAmmo[MAX_AMMO_TYPES];							// count of each ammo type

public:
	void Init( void )
	{
		memset( rgWeapons, 0, sizeof rgWeapons );
		Reset();
	}

	void Reset( void )
	{
		iOldWeaponBits = 0;
		for ( int i = 0; i < MAX_WEAPON_SLOTS; i++ )
			rgSlots[i].clear();
		memset( riAmmo, 0, sizeof riAmmo );
	}

///// WEAPON /////
	uint64_t	iOldWeaponBits;	// SBO 2.1: weapon ids 0..63 (CHudAmmo::Think)

	// Robustness: ids come from network messages (CurWeapon, WeapPickup); callers handle NULL
	WEAPON *GetWeapon( int iId ) { return ( iId >= 0 && iId < MAX_WEAPONS ) ? &rgWeapons[iId] : NULL; }
	void AddWeapon( WEAPON *wp ) 
	{ 
		// SBO 2.1: a WeaponList sent again (a plugin moving a weapon to another slot or position) takes a held
		// weapon out of its old bucket and puts it in the new one
		WEAPON *pOld = &rgWeapons[ wp->iId ];
		BOOL fHeld = FALSE;
		for ( int i = 0; i < MAX_WEAPON_SLOTS; i++ )
		{
			for ( size_t j = 0; j < rgSlots[i].size(); j++ )
			{
				if ( rgSlots[i][j] == pOld )
				{
					rgSlots[i].erase( rgSlots[i].begin() + j );
					fHeld = TRUE;
					break;
				}
			}
		}
		rgWeapons[ wp->iId ] = *wp;	
		LoadWeaponSprites( &rgWeapons[ wp->iId ] );
		if ( fHeld )
			PickupWeapon( &rgWeapons[ wp->iId ] );
	}

	// SBO 2.1 (Half-Life Unified SDK): sorted insertion, so searches find the weapons in position order; a second
	// weapon on a taken slot and position is not added
	void PickupWeapon( WEAPON *wp )
	{
		if ( wp->iSlot < 0 || wp->iSlot >= MAX_WEAPON_SLOTS || GetWeaponSlot( wp->iSlot, wp->iSlotPos ) )
			return;
		std::vector<WEAPON*> &bucket = rgSlots[ wp->iSlot ];
		size_t i = 0;
		while ( i < bucket.size() && bucket[i]->iSlotPos <= wp->iSlotPos )
			i++;
		bucket.insert( bucket.begin() + i, wp );
	}

	void DropWeapon( WEAPON *wp )
	{
		if ( wp->iSlot < 0 || wp->iSlot >= MAX_WEAPON_SLOTS )
			return;
		std::vector<WEAPON*> &bucket = rgSlots[ wp->iSlot ];
		for ( size_t i = 0; i < bucket.size(); i++ )
		{
			if ( bucket[i] == wp )
			{
				bucket.erase( bucket.begin() + i );
				return;
			}
		}
	}

	void DropAllWeapons( void )
	{
		for ( int i = 0; i < MAX_WEAPONS; i++ )
		{
			if ( rgWeapons[i].iId )
				DropWeapon( &rgWeapons[i] );
		}
	}

	WEAPON* GetWeaponSlot( int slot, int pos )
	{
		if ( slot < 0 || slot >= MAX_WEAPON_SLOTS )
			return NULL;
		for ( size_t i = 0; i < rgSlots[slot].size(); i++ )
		{
			if ( rgSlots[slot][i]->iSlotPos == pos )
				return rgSlots[slot][i];
		}
		return NULL;
	}

	// the highest position held in a slot, -1 for an empty slot (Half-Life Unified SDK)
	int GetHighestPositionInSlot( int slot ) const
	{
		if ( slot < 0 || slot >= MAX_WEAPON_SLOTS || rgSlots[slot].empty() )
			return -1;
		return rgSlots[slot].back()->iSlotPos;
	}

	void LoadWeaponSprites( WEAPON* wp );
	void LoadAllWeaponSprites( void );
	WEAPON* GetFirstPos( int iSlot );
	void SelectSlot( int iSlot, int fAdvance, int iDirection );
	WEAPON* GetNextActivePos( int iSlot, int iSlotPos );

	int HasAmmo( WEAPON *p );

///// AMMO /////
	AMMO GetAmmo( int iId ) { return iId; }

	void SetAmmo( int iId, int iCount ) { if ( iId >= 0 && iId < MAX_AMMO_TYPES ) riAmmo[ iId ] = iCount;	}	// Robustness: AmmoX index is a byte

	int CountAmmo( int iId );

	HSPRITE* GetAmmoPicFromWeapon( int iAmmoId, wrect_t& rect );
};

extern WeaponsResource gWR;


#define MAX_HISTORY 12
enum {
	HISTSLOT_EMPTY,
	HISTSLOT_AMMO,
	HISTSLOT_WEAP,
	HISTSLOT_ITEM,
};

class HistoryResource
{
private:
	struct HIST_ITEM {
		int type;
		float DisplayTime;  // the time at which this item should be removed from the history
		int iCount;
		int iId;
	};

	HIST_ITEM rgAmmoHistory[MAX_HISTORY];

public:

	void Init( void )
	{
		Reset();
	}

	void Reset( void )
	{
		memset( rgAmmoHistory, 0, sizeof rgAmmoHistory );
		iCurrentHistorySlot = 0;	// halflife-updated (#223): reset the current history icon slot too
	}

	int iHistoryGap;
	int iCurrentHistorySlot;

	void AddToHistory( int iType, int iId, int iCount = 0 );
	void AddToHistory( int iType, const char *szName, int iCount = 0 );

	void CheckClearHistory( void );
	int DrawAmmoHistory( float flTime );
};

extern HistoryResource gHR;



