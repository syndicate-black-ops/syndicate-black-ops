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
#ifndef ITEMS_H
#define ITEMS_H


#define SF_ITEM_TOUCH_ONLY	128	// Featureful: picked up by touch only (the default)

class CItem : public CBaseEntity
{
public:
	void	Spawn( void );
	CBaseEntity*	Respawn( void );
	void	EXPORT ItemTouch( CBaseEntity *pOther );
	// SBO 2.0 (Featureful CItem): flag 256 "USE only" picks the item up with +use instead of touch;
	// flag 128 "TOUCH only" is the default behavior
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void );
	void	TouchOrUse( CBaseEntity *pOther );
	void	EXPORT Materialize( void );
	virtual BOOL MyTouch( CBasePlayer *pPlayer ) { return FALSE; };
	// SBO 2.0 object hints (Featureful CPickup): no hint while the item waits to respawn
	virtual BOOL CanShowObjectHint( void ) { return !FBitSet( pev->effects, EF_NODRAW ); }
};

#endif // ITEMS_H
