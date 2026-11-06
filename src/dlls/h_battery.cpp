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
/*

===== h_battery.cpp ========================================================

  battery-related code

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "skill.h"
#include "gamerules.h"

class CRecharge : public CBaseToggle
{
public:
	void Spawn( );
	void Precache( void );
	void EXPORT Off(void);
	void EXPORT Recharge(void);
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return (CBaseToggle :: ObjectCaps() | FCAP_CONTINUOUS_USE) & ~FCAP_ACROSS_TRANSITION; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	float m_flNextCharge; 
	int		m_iReactivate ; // DeathMatch Delay until reactvated
	int		m_iJuice;
	int		m_iOn;			// 0 = off, 1 = startup, 2 = going
	float   m_flSoundTime;

	// SBO 2.0 (Featureful CWallCharger): targets fired on the first charge given, on the first use once
	// empty, and when recharged; custom sounds in noise (loop), noise1 (denied), noise2 (start), noise3 (recharge)
	string_t m_iszTriggerOnFirstUse;
	string_t m_iszTriggerOnEmpty;
	string_t m_iszTriggerOnRecharged;

	// SBO 2.0, from Featureful (CWallCharger b3b353c4, Sven Co-op's key): "CustomJuice" the armor it
	// holds; 0 / empty: the skill's suit charger capacity. Featureful keeps it in pev->health; SBO in its own field, so a
	// map's plain "health" key changes nothing
	int		m_iCustomJuice;
	int		ChargerCapacity( void ) { return m_iCustomJuice > 0 ? m_iCustomJuice : (int)gSkillData.suitchargerCapacity; }
	const char *LoopSound( void ) { return FStringNull( pev->noise ) ? "items/suitcharge1.wav" : STRING( pev->noise ); }
	const char *DenySound( void ) { return FStringNull( pev->noise1 ) ? "items/suitchargeno1.wav" : STRING( pev->noise1 ); }
	const char *StartSound( void ) { return FStringNull( pev->noise2 ) ? "items/suitchargeok1.wav" : STRING( pev->noise2 ); }

	// SBO 2.0 object hints (Featureful CWallCharger): depleted, or the player has no suit (Use refuses him)
	virtual BOOL IsUsefulToDisplayHint( CBaseEntity *pPlayer ) { return m_iJuice > 0 && ( pPlayer->pev->weapons & ( 1 << WEAPON_SUIT ) ); }
};

TYPEDESCRIPTION CRecharge::m_SaveData[] =
{
	DEFINE_FIELD( CRecharge, m_flNextCharge, FIELD_TIME ),
	DEFINE_FIELD( CRecharge, m_iReactivate, FIELD_INTEGER),
	DEFINE_FIELD( CRecharge, m_iJuice, FIELD_INTEGER),
	DEFINE_FIELD( CRecharge, m_iCustomJuice, FIELD_INTEGER),
	DEFINE_FIELD( CRecharge, m_iOn, FIELD_INTEGER),
	DEFINE_FIELD( CRecharge, m_iszTriggerOnFirstUse, FIELD_STRING ),
	DEFINE_FIELD( CRecharge, m_iszTriggerOnEmpty, FIELD_STRING ),
	DEFINE_FIELD( CRecharge, m_iszTriggerOnRecharged, FIELD_STRING ),
	DEFINE_FIELD( CRecharge, m_flSoundTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CRecharge, CBaseEntity );

LINK_ENTITY_TO_CLASS(func_recharge, CRecharge);


void CRecharge::KeyValue( KeyValueData *pkvd )
{
	if (	FStrEq(pkvd->szKeyName, "style") ||
				FStrEq(pkvd->szKeyName, "height") ||
				FStrEq(pkvd->szKeyName, "value1") ||
				FStrEq(pkvd->szKeyName, "value2") ||
				FStrEq(pkvd->szKeyName, "value3"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "dmdelay"))
	{
		m_iReactivate = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TriggerOnFirstUse"))
	{
		m_iszTriggerOnFirstUse = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TriggerOnEmpty"))
	{
		m_iszTriggerOnEmpty = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TriggerOnRecharged"))
	{
		m_iszTriggerOnRecharged = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "CustomLoopSound"))
	{
		pev->noise = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "CustomDeniedSound"))
	{
		pev->noise1 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "CustomStartSound"))
	{
		pev->noise2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "CustomJuice"))
	{
		m_iCustomJuice = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "CustomRechargeSound"))
	{
		pev->noise3 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CRecharge::Spawn()
{
	Precache( );

	pev->solid		= SOLID_BSP;
	pev->movetype	= MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);		// set size and link into world
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	SET_MODEL(ENT(pev), STRING(pev->model) );
	m_iJuice = ChargerCapacity();	// SBO 2.0 (Featureful): "CustomJuice"
	pev->frame = 0;			
}

void CRecharge::Precache()
{
	PRECACHE_SOUND("items/suitcharge1.wav");
	PRECACHE_SOUND("items/suitchargeno1.wav");
	PRECACHE_SOUND("items/suitchargeok1.wav");
	if ( !FStringNull( pev->noise ) )  PRECACHE_SOUND( (char *)STRING( pev->noise ) );
	if ( !FStringNull( pev->noise1 ) ) PRECACHE_SOUND( (char *)STRING( pev->noise1 ) );
	if ( !FStringNull( pev->noise2 ) ) PRECACHE_SOUND( (char *)STRING( pev->noise2 ) );
	if ( !FStringNull( pev->noise3 ) ) PRECACHE_SOUND( (char *)STRING( pev->noise3 ) );
}


void CRecharge::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	// if it's not a player, ignore
	if (!FClassnameIs(pActivator->pev, "player"))
		return;

	// if there is no juice left, turn it off
	if (m_iJuice <= 0)
	{
		if ( !FStringNull( m_iszTriggerOnEmpty ) && pev->frame == 0 )
			FireTargets( STRING( m_iszTriggerOnEmpty ), this, this, USE_TOGGLE, 0 );
		pev->frame = 1;			
		Off();
	}

	// if the player doesn't have the suit, or there is no juice left, make the deny noise
	if ((m_iJuice <= 0) || (!(pActivator->pev->weapons & (1<<WEAPON_SUIT))))
	{
		if (m_flSoundTime <= gpGlobals->time)
		{
			m_flSoundTime = gpGlobals->time + 0.62;
			EMIT_SOUND(ENT(pev), CHAN_ITEM, DenySound(), 0.85, ATTN_NORM );
		}
		return;
	}

	pev->nextthink = pev->ltime + 0.25;
	SetThink(&CRecharge::Off);

	// Time to recharge yet?

	if (m_flNextCharge >= gpGlobals->time)
		return;

	// Make sure that we have a caller
	if (!pActivator)
		return;

	m_hActivator = pActivator;

	//only recharge the player

	if (!m_hActivator->IsPlayer() )
		return;
	
	// Play the on sound or the looping charging sound
	if (!m_iOn)
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, StartSound(), 0.85, ATTN_NORM );
		m_flSoundTime = 0.56 + gpGlobals->time;
	}
	if ((m_iOn == 1) && (m_flSoundTime <= gpGlobals->time))
	{
		m_iOn++;
		EMIT_SOUND(ENT(pev), CHAN_STATIC, LoopSound(), 0.85, ATTN_NORM );
	}


	// charge the player
	if (m_hActivator->pev->armorvalue < 100)
	{
		m_iJuice--;
		m_hActivator->pev->armorvalue += 1;
		if ( !FStringNull( m_iszTriggerOnFirstUse ) )
		{
			FireTargets( STRING( m_iszTriggerOnFirstUse ), this, this, USE_TOGGLE, 0 );
			m_iszTriggerOnFirstUse = iStringNull;
		}

		if (m_hActivator->pev->armorvalue > 100)
			m_hActivator->pev->armorvalue = 100;
	}

	// govern the rate of charge
	m_flNextCharge = gpGlobals->time + 0.1;
}

void CRecharge::Recharge(void)
{
	if ( !FStringNull( m_iszTriggerOnRecharged ) )
		FireTargets( STRING( m_iszTriggerOnRecharged ), this, this, USE_TOGGLE, 0 );
	// stock plays no recharge sound here; only a custom one
	if ( !FStringNull( pev->noise3 ) )
		EMIT_SOUND(ENT(pev), CHAN_ITEM, STRING( pev->noise3 ), 0.85, ATTN_NORM );
	m_iJuice = ChargerCapacity();	// SBO 2.0 (Featureful): "CustomJuice"
	pev->frame = 0;			
	SetThink( &CRecharge::SUB_DoNothing );
}

void CRecharge::Off(void)
{
	// Stop looping sound.
	if (m_iOn > 1)
		STOP_SOUND( ENT(pev), CHAN_STATIC, LoopSound() );

	m_iOn = 0;

	if ((!m_iJuice) &&  ( ( m_iReactivate = g_pGameRules->FlHEVChargerRechargeTime() ) > 0) )
	{
		pev->nextthink = pev->ltime + m_iReactivate;
		SetThink(&CRecharge::Recharge);
	}
	else
		SetThink( &CRecharge::SUB_DoNothing );
}