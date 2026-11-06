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

// SBO: ammo reserve box (CAmmoReserve, "ammo_reserve"), server only.
// Refills every SBO ammo type; with the limitator active the amounts come
// from the limitator globals (defined in limitator.cpp).

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"

extern int Limitat;
extern int m_Rc28;
extern int m_K444;
extern int m_Canon;
extern int m_Pm66;
extern int m_Federal;
extern int m_Fa18;
extern int m_Spa;
extern int m_Ben;
extern int m_Sw;
extern int m_Nm33;
extern int m_Mini;


class CAmmoReserve : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/ammobox.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/ammobox.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		// SBO: shape guessed from the code: both egcs and VC6 /O2 put the unlimited block first and
		// tail-merge the EMIT_SOUND; if/else in either order gives opposite layouts on the two compilers.
		if ( !Limitat )
			goto unlimited;
		pOther->GiveAmmo( m_Rc28, "rc28m", 900 );
		pOther->GiveAmmo( m_K444, "k444m", 900 );
		pOther->GiveAmmo( m_Canon, "canonm", 900 );
		pOther->GiveAmmo( m_Pm66, "pm66m", 900 );
		pOther->GiveAmmo( m_Fa18, "fa18m", 900 );
		pOther->GiveAmmo( m_Federal, "federalm", 900 );
		pOther->GiveAmmo( m_Spa, "spam", 900 );
		pOther->GiveAmmo( m_Ben, "benm", 900 );
		pOther->GiveAmmo( m_Sw, "swm", 900 );
		pOther->GiveAmmo( m_Nm33, "nm33m", 900 );
		pOther->GiveAmmo( m_Mini, "chainm", 900 );
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		return TRUE;
unlimited:
		pOther->GiveAmmo( 900, "rc28m", 900 );
		pOther->GiveAmmo( 900, "k444m", 900 );
		pOther->GiveAmmo( 900, "canonm", 900 );
		pOther->GiveAmmo( 900, "pm66m", 900 );
		pOther->GiveAmmo( 150, "fa18m", 900 );
		pOther->GiveAmmo( 180, "federalm", 900 );
		pOther->GiveAmmo( 200, "spam", 900 );
		pOther->GiveAmmo( 210, "benm", 900 );
		pOther->GiveAmmo( 100, "swm", 900 );
		pOther->GiveAmmo( 210, "nm33m", 900 );
		pOther->GiveAmmo( 500, "chainm", 900 );
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		return TRUE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_reserve, CAmmoReserve );
