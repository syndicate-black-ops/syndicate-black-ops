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

===== triggers.cpp ========================================================

  spawn and use functions for editor-placed triggers              

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "saverestore.h"
#include "trains.h"			// trigger_camera has train functionality
#include "gamerules.h"
#include "triggers.h"
#include "locus.h"		// SBO 2.0: Locus keys (trigger_push, env_render)
#include "render_fade.h"	// SBO 2.0: env_render fades drawn by the clients
#include "round_hooks.h"	// SBO 2.0: Logic_IsPlayerInPlay (trigger_inout)
#include "pm_shared.h"	// SBO 2.0: PLAYER_SUPPRESS_* (trigger_camera Freeze Player with Stop by player input)
#include "game.h"		// SBO 2.0: g_psv_gravity (trigger_jump)
#include "movewith.h"	// SBO 2.1: MoveWith_IsChild (trigger_camera with a parent)

#define	SF_TRIGGER_PUSH_START_OFF	2//spawnflag that makes trigger_push spawn turned OFF
#define SF_TRIGGER_HURT_TARGETONCE	1// Only fire hurt target once
#define	SF_TRIGGER_HURT_START_OFF	2//spawnflag that makes trigger_push spawn turned OFF
#define	SF_TRIGGER_HURT_NO_CLIENTS	8//spawnflag that makes trigger_push spawn turned OFF
#define SF_TRIGGER_HURT_CLIENTONLYFIRE	16// trigger hurt will only fire its target if it is hurting a client
#define SF_TRIGGER_HURT_CLIENTONLYTOUCH 32// only clients may touch this trigger.
// SBO 2.0, from Featureful (Half-Life 1 SDK license); no map sets these
#define SF_TRIGGER_HURT_FULL_DAMAGE_EVERY_HALF_SECOND	128	// the whole "dmg" every 0.5 s, not half of it
// SBO 2.0, from Featureful (no map sets them)
#define SF_TRIGGER_HURT_AFFECT_NON_MOVING_MONSTERS	64	// monsters standing still in the box are hurt too
#define SF_TRIGGER_HURT_IGNORE_ARMOR	256	// a player's armor takes no part
#define SF_TRIGGER_HURT_NO_PUNCH		512	// a player's view is not kicked

// featureful_triggers.cpp: Featureful's damage options (ignore armor, no punch, minimum health) around TakeDamage
void Featureful_TakeDamage( CBaseEntity *pTarget, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage,
	int bitsDamage, float flFloor, BOOL fIgnoreArmor, BOOL fNoPunch );
#define SF_TRIGGER_PUSH_NO_CLIENTS	8		// trigger_push: players are not pushed
#define SF_TRIGGER_PUSH_NO_MONSTERS	16		// trigger_push: monsters are not pushed

extern DLL_GLOBAL BOOL		g_fGameOver;

extern void SetMovedir(entvars_t* pev);
extern Vector VecBModelOrigin( entvars_t* pevBModel );

class CFrictionModifier : public CBaseEntity
{
public:
	void		Spawn( void );
	void		KeyValue( KeyValueData *pkvd );
	void EXPORT	ChangeFriction( CBaseEntity *pOther );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	static	TYPEDESCRIPTION m_SaveData[];

	float		m_frictionFraction;		// Sorry, couldn't resist this name :)
};

LINK_ENTITY_TO_CLASS( func_friction, CFrictionModifier );

// Global Savedata for changelevel friction modifier
TYPEDESCRIPTION	CFrictionModifier::m_SaveData[] = 
{
	DEFINE_FIELD( CFrictionModifier, m_frictionFraction, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE(CFrictionModifier,CBaseEntity);


// Modify an entity's friction
void CFrictionModifier :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	SET_MODEL(ENT(pev), STRING(pev->model));    // set size and link into world
	pev->movetype = MOVETYPE_NONE;
	SetTouch( &CFrictionModifier::ChangeFriction );
}


// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CFrictionModifier :: ChangeFriction( CBaseEntity *pOther )
{
	if ( pOther->pev->movetype != MOVETYPE_BOUNCEMISSILE && pOther->pev->movetype != MOVETYPE_BOUNCE )
		pOther->pev->friction = m_frictionFraction;
}



// Sets toucher's friction to m_frictionFraction (1.0 = normal friction)
void CFrictionModifier :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "modifier"))
	{
		m_frictionFraction = atof(pkvd->szValue) / 100.0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}


// This trigger will fire when the level spawns (or respawns if not fire once)
// It will check a global state before firing.  It supports delay and killtargets

#define SF_AUTO_FIREONCE		0x0001

class CAutoTrigger : public CBaseDelay
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Precache( void );
	void Think( void );

	int ObjectCaps( void ) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

private:
	int			m_globalstate;
	USE_TYPE	triggerType;
};
LINK_ENTITY_TO_CLASS( trigger_auto, CAutoTrigger );

TYPEDESCRIPTION	CAutoTrigger::m_SaveData[] = 
{
	DEFINE_FIELD( CAutoTrigger, m_globalstate, FIELD_STRING ),
	DEFINE_FIELD( CAutoTrigger, triggerType, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE(CAutoTrigger,CBaseDelay);

void CAutoTrigger::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "globalstate"))
	{
		m_globalstate = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "triggerstate"))
	{
		int type = atoi( pkvd->szValue );
		switch( type )
		{
		case 0:
			triggerType = USE_OFF;
			break;
		case 2:
			triggerType = USE_TOGGLE;
			break;
		default:
			triggerType = USE_ON;
			break;
		}
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}


void CAutoTrigger::Spawn( void )
{
	Precache();
}


void CAutoTrigger::Precache( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
}


void CAutoTrigger::Think( void )
{
	if ( !m_globalstate || gGlobalState.EntityGetState( m_globalstate ) == GLOBAL_ON )
	{
		SUB_UseTargets( this, triggerType, 0 );
		if ( pev->spawnflags & SF_AUTO_FIREONCE )
			UTIL_Remove( this );
	}
}



#define SF_RELAY_FIREONCE		0x0001

class CTriggerRelay : public CBaseDelay
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int ObjectCaps( void ) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

private:
	USE_TYPE	MyTriggerType( USE_TYPE useType );

	int			triggerType;	// SBO 2.0: the "triggerstate" key's number (Featureful); was the USE_TYPE
	// SBO 2.0, from Featureful: "Delay Before Reset": after firing, calls are ignored for this long; no map sets it
	float		m_flDelayBeforeReset;
	float		m_flResetTime;
	string_t	m_sMaster;	// SBO 2.1 (Half-Life Unified SDK): "master"; while it is locked, the relay does not fire
};
LINK_ENTITY_TO_CLASS( trigger_relay, CTriggerRelay );

// SBO 2.0, from Featureful (halflife-featureful dlls/triggers.cpp CTriggerRelay, Half-Life 1 SDK license):
// "Forward activator" (Sven Co-op) passes the activator the relay got on to its targets, "Pass caller as activator"
// its caller (wins over Forward activator); stock relays pass themselves. Off by default.
#define SF_RELAY_FORWARDACTIVATOR			0x0040
#define SF_RELAY_PASS_CALLER_AS_ACTIVATOR	0x0080

TYPEDESCRIPTION	CTriggerRelay::m_SaveData[] = 
{
	DEFINE_FIELD( CTriggerRelay, triggerType, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerRelay, m_flDelayBeforeReset, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerRelay, m_flResetTime, FIELD_TIME ),
	DEFINE_FIELD( CTriggerRelay, m_sMaster, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE(CTriggerRelay,CBaseDelay);

void CTriggerRelay::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "triggerstate"))
	{
		triggerType = atoi( pkvd->szValue );	// SBO 2.0: mapped at use (MyTriggerType), as Featureful
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flDelayBeforeReset"))	// SBO 2.0 (Featureful)
	{
		m_flDelayBeforeReset = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))	// SBO 2.1 (Half-Life Unified SDK)
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}


void CTriggerRelay::Spawn( void )
{
}




void CTriggerRelay::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SBO 2.1 (Half-Life Unified SDK): "master": while it is locked for the activator, the relay does not fire
	if ( !UTIL_IsMasterTriggered( m_sMaster, pActivator ) )
		return;

	// SBO 2.0 (Featureful): "Delay Before Reset"
	if ( m_flDelayBeforeReset > 0 && m_flResetTime > gpGlobals->time )
		return;

	// SBO 2.0 (Featureful): the activator the targets get
	CBaseEntity *targetActivator = this;
	if ( FBitSet( pev->spawnflags, SF_RELAY_PASS_CALLER_AS_ACTIVATOR ) )
		targetActivator = pCaller;
	else if ( FBitSet( pev->spawnflags, SF_RELAY_FORWARDACTIVATOR ) )
		targetActivator = pActivator;
	if ( !targetActivator )
		targetActivator = this;	// SBO: none passed in: the relay, as without the flags

	SUB_UseTargets( targetActivator, MyTriggerType( useType ), 0 );
	if ( m_flDelayBeforeReset > 0 )
		m_flResetTime = gpGlobals->time + m_flDelayBeforeReset;
	if ( pev->spawnflags & SF_RELAY_FIREONCE )
		UTIL_Remove( this );
}

// SBO 2.0, from Featureful: Trigger State -1 "Copy Input" (the use type the relay got), -2 "Reverse Input" (On and Off
// swapped, others copied). 0 Off, 2 Toggle, anything else On, as stock (a relay without the key fires Off: stock's
// triggerType was 0 = USE_OFF then too).
USE_TYPE CTriggerRelay::MyTriggerType( USE_TYPE useType )
{
	switch ( triggerType )
	{
	case -2:
		if ( useType == USE_OFF )
			return USE_ON;
		else if ( useType == USE_ON )
			return USE_OFF;
		return useType;
	case -1:
		return useType;
	case 0:
		return USE_OFF;
	case 2:
		return USE_TOGGLE;
	default:
		return USE_ON;
	}
}


//**********************************************************
// The Multimanager Entity - when fired, will fire up to 16 targets 
// at specified times.
// FLAG:		THREAD (create clones when triggered)
// FLAG:		CLONE (this is a clone for a threaded execution)

#define SF_MULTIMAN_CLONE		0x80000000
#define SF_MULTIMAN_THREAD		0x00000001
// SBO 2.0, from Featureful (Half-Life 1 SDK license); no map sets these bits
#define SF_MULTIMAN_ONLYONCE	0x00000008	// removed once it has fired all its targets
#define SF_MULTIMAN_INSTANT		0x00000400	// targets with delay 0 fire in the frame of the call, not the next think

class CMultiManager : public CBaseToggle
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn ( void );
	void EXPORT ManagerThink ( void );
	void EXPORT ManagerUse   ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

#if _DEBUG
	void EXPORT ManagerReport( void );
#endif

	BOOL		HasTarget( string_t targetname );
	// SBO 2.1: a target's delay changed at run time (trigger_changevalue / trigger_copyvalue)
	CKeyValue	GetKeyValue( const char *keyName );
	int			FindTarget( const char *keyName );
	void		SortTargets( int iFrom );
	BOOL		m_fSpawned;		// a target key now updates its entry instead of adding one
	BOOL		m_fResort;		// an entry already fired this run changed: sort the list when the run ends
	
	int ObjectCaps( void ) { return CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	// SBO 2.1 (Spirit of Half-Life): On from the trigger until it has fired its last target, Off while it waits to be
	// triggered. A multithreaded one runs in copies of itself: it is On while one of them runs
	virtual STATE GetState( CBaseEntity *pActivator )
	{
		if ( IsRunning() )
			return STATE_ON;
		if ( ShouldClone() && !FStringNull( pev->targetname ) )
		{
			CBaseEntity *pEntity = NULL;
			while ( ( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->targetname ) ) ) != NULL )
			{
				if ( pEntity != this && FClassnameIs( pEntity->pev, "multi_manager" ) && ( (CMultiManager *)pEntity )->IsClone()
					&& ( (CMultiManager *)pEntity )->IsRunning() )
					return STATE_ON;
			}
		}
		return STATE_OFF;
	}
	// running: the Use is off from the trigger until the last target fired (ManagerUse, ManagerThink)
	BOOL IsRunning( void ) { return m_pfnUse == NULL; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	int		m_cTargets;	// the total number of targets in this manager's fire list.
	int		m_index;	// Current target
	float	m_startTime;// Time we started firing
	int		m_iTargetName	[ MAX_MULTI_TARGETS ];// list if indexes into global string array
	float	m_flTargetDelay [ MAX_MULTI_TARGETS ];// delay (in seconds) from time of manager fire to target fire
private:
	inline BOOL IsClone( void ) { return (pev->spawnflags & SF_MULTIMAN_CLONE) ? TRUE : FALSE; }
	inline BOOL ShouldClone( void ) 
	{ 
		if ( IsClone() )
			return FALSE;

		return (pev->spawnflags & SF_MULTIMAN_THREAD) ? TRUE : FALSE; 
	}

	CMultiManager *Clone( void );
};
LINK_ENTITY_TO_CLASS( multi_manager, CMultiManager );

// Global Savedata for multi_manager
TYPEDESCRIPTION	CMultiManager::m_SaveData[] = 
{
	DEFINE_FIELD( CMultiManager, m_cTargets, FIELD_INTEGER ),
	DEFINE_FIELD( CMultiManager, m_index, FIELD_INTEGER ),
	DEFINE_FIELD( CMultiManager, m_startTime, FIELD_TIME ),
	DEFINE_ARRAY( CMultiManager, m_iTargetName, FIELD_STRING, MAX_MULTI_TARGETS ),
	DEFINE_ARRAY( CMultiManager, m_flTargetDelay, FIELD_FLOAT, MAX_MULTI_TARGETS ),
	DEFINE_FIELD( CMultiManager, m_fSpawned, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMultiManager, m_fResort, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE(CMultiManager,CBaseToggle);

void CMultiManager :: KeyValue( KeyValueData *pkvd )
{
	// UNDONE: Maybe this should do something like this:
	//CBaseToggle::KeyValue( pkvd );
	// if ( !pkvd->fHandled )
	// ... etc.

	if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( m_fSpawned && FindTarget( pkvd->szKeyName ) >= 0 )
	{
		// SBO 2.1: at run time a target already in the list ("name", or "name#n" for its next entries, as the map
		// editors name them) gets the new delay, and the list is sorted again: the targets not fired yet in a run
		// at once, the whole list when the run ends
		m_flTargetDelay[ FindTarget( pkvd->szKeyName ) ] = atof( pkvd->szValue );
		const BOOL fRunning = m_pfnThink != NULL && m_pfnUse == NULL;
		SortTargets( fRunning ? m_index : 0 );
		if ( fRunning )
		{
			m_fResort = TRUE;
			if ( m_index < m_cTargets )
				pev->nextthink = m_startTime + m_flTargetDelay[ m_index ];
		}
		pkvd->fHandled = TRUE;
	}
	else // add this field to the target list
	{
		// this assumes that additional fields are targetnames and their values are delay values.
		if ( m_cTargets < MAX_MULTI_TARGETS )
		{
			char tmp[128];

			UTIL_StripToken( pkvd->szKeyName, tmp, sizeof( tmp ) );
			m_iTargetName [ m_cTargets ] = ALLOC_STRING( tmp );
			m_flTargetDelay [ m_cTargets ] = atof (pkvd->szValue);
			m_cTargets++;
			pkvd->fHandled = TRUE;
		}
		else	// SBO 2.1 (Half-Life Unified SDK)
			ALERT( at_console, "%s \"%s\": too many targets (more than %d), \"%s\" left out\n", STRING( pev->classname ),
				STRING( pev->targetname ), MAX_MULTI_TARGETS, pkvd->szKeyName );
	}
}


void CMultiManager :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	SetUse ( &CMultiManager::ManagerUse );
	SetThink ( &CMultiManager::ManagerThink);

	SortTargets( 0 );
	m_fSpawned = TRUE;	// SBO 2.1
}

// the entry a run-time key names: "name" its first entry, "name#n" the one after n others of that name (Hammer's
// numbering of a repeated target); -1: none
int CMultiManager::FindTarget( const char *keyName )
{
	char tmp[128];
	UTIL_StripToken( keyName, tmp, sizeof( tmp ) );
	const char *pHash = strchr( keyName, '#' );
	int iSkip = pHash ? atoi( pHash + 1 ) : 0;

	for ( int i = 0; i < m_cTargets; i++ )
	{
		if ( FStrEq( tmp, STRING( m_iTargetName[i] ) ) && iSkip-- == 0 )
			return i;
	}
	return -1;
}

// SBO 2.1: a target's delay ("name" / "name#n") and "wait" read back
CKeyValue CMultiManager::GetKeyValue( const char *keyName )
{
	CKeyValue keyvalue = CBaseToggle::GetKeyValue( keyName );
	if ( keyvalue.keyType )
		return keyvalue;

	if ( FStrEq( keyName, "wait" ) )
	{
		keyvalue.keyType = KEY_TYPE_FLOAT;
		keyvalue.keyName = "wait";
		keyvalue.fVal = m_flWait;
		return keyvalue;
	}

	const int i = FindTarget( keyName );
	if ( i >= 0 )
	{
		keyvalue.keyType = KEY_TYPE_FLOAT;
		keyvalue.keyName = keyName;
		keyvalue.fVal = m_flTargetDelay[i];
	}
	return keyvalue;
}

// the targets from iFrom on by delay (Spawn: all of them)
void CMultiManager::SortTargets( int iFrom )
{
	// Sort targets
	// Quick and dirty bubble sort
	int swapped = 1;

	while ( swapped )
	{
		swapped = 0;
		for ( int i = iFrom + 1; i < m_cTargets; i++ )
		{
			if ( m_flTargetDelay[i] < m_flTargetDelay[i-1] )
			{
				// Swap out of order elements
				int name = m_iTargetName[i];
				float delay = m_flTargetDelay[i];
				m_iTargetName[i] = m_iTargetName[i-1];
				m_flTargetDelay[i] = m_flTargetDelay[i-1];
				m_iTargetName[i-1] = name;
				m_flTargetDelay[i-1] = delay;
				swapped = 1;
			}
		}
	}
}


BOOL CMultiManager::HasTarget( string_t targetname )
{ 
	for ( int i = 0; i < m_cTargets; i++ )
		if ( FStrEq(STRING(targetname), STRING(m_iTargetName[i])) )
			return TRUE;
	
	return FALSE;
}


// Designers were using this to fire targets that may or may not exist -- 
// so I changed it to use the standard target fire code, made it a little simpler.
void CMultiManager :: ManagerThink ( void )
{
	float	time;

	time = gpGlobals->time - m_startTime;
	while ( m_index < m_cTargets && m_flTargetDelay[ m_index ] <= time )
	{
		FireTargets( STRING( m_iTargetName[ m_index ] ), m_hActivator, this, USE_TOGGLE, 0 );
		m_index++;
	}

	if ( m_index >= m_cTargets )// have we fired all targets?
	{
		SetThink( NULL );
		if ( m_fResort )	// SBO 2.1: a delay changed while it ran
		{
			m_fResort = FALSE;
			SortTargets( 0 );
		}
		if ( IsClone() || FBitSet( pev->spawnflags, SF_MULTIMAN_ONLYONCE ) )	// SBO 2.0 (Featureful): Once only
		{
			UTIL_Remove( this );
			return;
		}
		SetUse ( &CMultiManager::ManagerUse );// allow manager re-use 
	}
	else
		pev->nextthink = m_startTime + m_flTargetDelay[ m_index ];
}

CMultiManager *CMultiManager::Clone( void )
{
	CMultiManager *pMulti = GetClassPtr( (CMultiManager *)NULL );

	edict_t *pEdict = pMulti->pev->pContainingEntity;
	memcpy( pMulti->pev, pev, sizeof(*pev) );
	pMulti->pev->pContainingEntity = pEdict;

	pMulti->pev->spawnflags |= SF_MULTIMAN_CLONE;	// (Instant is copied with the rest of pev)
	pMulti->m_cTargets = m_cTargets;
	pMulti->m_fSpawned = TRUE;	// SBO 2.1
	memcpy( pMulti->m_iTargetName, m_iTargetName, sizeof( m_iTargetName ) );
	memcpy( pMulti->m_flTargetDelay, m_flTargetDelay, sizeof( m_flTargetDelay ) );

	return pMulti;
}


// The USE function builds the time table and starts the entity thinking.
void CMultiManager :: ManagerUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// In multiplayer games, clone the MM and execute in the clone (like a thread)
	// to allow multiple players to trigger the same multimanager
	if ( ShouldClone() )
	{
		CMultiManager *pClone = Clone();
		pClone->ManagerUse( pActivator, pCaller, useType, value );
		return;
	}

	m_hActivator = pActivator;
	m_index = 0;
	m_startTime = gpGlobals->time;

	SetUse( NULL );// disable use until all targets have fired

	SetThink ( &CMultiManager::ManagerThink );
	// SBO 2.0 (Featureful): "Instant": the targets with delay 0 fire now
	if ( FBitSet( pev->spawnflags, SF_MULTIMAN_INSTANT ) && m_cTargets > 0 && m_flTargetDelay[0] <= 0 )
		ManagerThink();
	else
		pev->nextthink = gpGlobals->time;
}

#if _DEBUG
void CMultiManager :: ManagerReport ( void )
{
	int	cIndex;

	for ( cIndex = 0 ; cIndex < m_cTargets ; cIndex++ )
	{
		ALERT ( at_console, "%s %f\n", STRING(m_iTargetName[cIndex]), m_flTargetDelay[cIndex] );
	}
}
#endif

//***********************************************************


//
// Render parameters trigger
//
// This entity will copy its render parameters (renderfx, rendermode, rendercolor, renderamt)
// to its targets when triggered.
//


// Flags to indicate masking off various render parameters that are normally copied to the targets
#define SF_RENDER_MASKFX	(1<<0)
#define SF_RENDER_MASKAMT	(1<<1)
#define SF_RENDER_MASKMODE	(1<<2)
#define SF_RENDER_MASKCOLOR	(1<<3)

// SBO 2.0, from Featureful / Spirit of Half-Life (halflife-featureful dlls/triggers.cpp CRenderFxManager,
// Half-Life 1 SDK license): "target" [LE] (also !activator, *player), "message" FX amount factor [LR], "m_fScale"
// scale (0 = no change), "frags" fade time, "netname" target after fading, flags 32 remove target / 64 remove self
// after the fade. The fade is drawn by the clients (render_fade.cpp): one message per entity, no render_fader
// entities. "armorvalue" (Fade Coarseness) is read and ignored: the clients blend every frame. docs/ENV_RENDER.md.
#define SF_RENDER_KILLTARGET	(1<<5)
#define SF_RENDER_ONLYONCE		(1<<6)

#define RENDER_MAX_FADED	64	// entities one env_render fades at once and remembers for the end of the fade

class CRenderFxManager : public CBaseEntity
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Affect( CBaseEntity *pTarget, BOOL bIsFirst, CBaseEntity *pActivator );
	void EXPORT FadeDoneThink( void );

	EHANDLE	m_hFaded[ RENDER_MAX_FADED ];
	int		m_iFadeId[ RENDER_MAX_FADED ];
	int		m_iFaded;
	EHANDLE	m_hFirstFaded;	// "Trigger after fading" is fired with it as activator (Featureful)
	BOOL	m_bFireAfterFade;
};

LINK_ENTITY_TO_CLASS( env_render, CRenderFxManager );


void CRenderFxManager :: Spawn ( void )
{
	pev->solid = SOLID_NOT;
}

void CRenderFxManager :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_fScale" ) )	// SBO 2.0 (Featureful / Spirit)
	{
		pev->scale = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CRenderFxManager :: Use ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->frags > 0 )
	{
		// a fade replaces the one this env_render was waiting for
		m_iFaded = 0;
		m_hFirstFaded = NULL;
		m_bFireAfterFade = FALSE;
		SetThink( NULL );
	}

	if (!FStringNull(pev->target))
	{
		// SBO 2.0 (Featureful): the target may be !activator / *locus / *player
		CBaseEntity *pTarget = Locus_FindEntity( NULL, STRING( pev->target ), pActivator );
		BOOL bFirst = TRUE;
		while ( pTarget )
		{
			Affect( pTarget, bFirst, pActivator );
			bFirst = FALSE;
			pTarget = Locus_FindEntity( pTarget, STRING( pev->target ), pActivator );
		}
	}

	if ( pev->frags > 0 && ( m_iFaded || m_bFireAfterFade ) )
	{
		SetThink( &CRenderFxManager::FadeDoneThink );
		pev->nextthink = gpGlobals->time + pev->frags;
	}
	else if ( pev->spawnflags & SF_RENDER_ONLYONCE )
	{
		SetThink( &CRenderFxManager::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CRenderFxManager :: Affect( CBaseEntity *pTarget, BOOL bIsFirst, CBaseEntity *pActivator )
{
	entvars_t *pevTarget = pTarget->pev;

	float fAmtFactor = 1;
	if ( pev->message && !FBitSet( pev->spawnflags, SF_RENDER_MASKAMT ) )
		TryCalcLocus_Ratio( pActivator, STRING( pev->message ), fAmtFactor );

	if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKFX ) )
		pevTarget->renderfx = pev->renderfx;
	if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKMODE ) )
	{
		// LRC - amt is often 0 when mode is normal. Set it to be fully visible, for fade purposes.
		if ( pev->frags > 0 && pevTarget->renderamt == 0 && pevTarget->rendermode == kRenderNormal )
			pevTarget->renderamt = 255;
		pevTarget->rendermode = pev->rendermode;
	}

	if ( !( pev->frags > 0 ) )
	{
		// not fading: as in Half-Life (plus the scale and the trigger)
		RenderFade_Cancel( pTarget );
		if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKAMT ) )
			pevTarget->renderamt = pev->renderamt * fAmtFactor;
		if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKCOLOR ) )
			pevTarget->rendercolor = pev->rendercolor;
		if ( pev->scale )
		{
			pevTarget->scale = pev->scale;
			// SBO 2.0 model scale: the collision box follows (as trigger_changevalue's "scale", keyvalue.cpp)
			UTIL_ModelScaleChanged( pevTarget );
		}

		if ( bIsFirst && !FStringNull( pev->netname ) )
			FireTargets( STRING( pev->netname ), pTarget, this, USE_TOGGLE, 0 );
		return;
	}

	// fade: the clients blend from what the entity shows now to these values
	int flags = 0;
	if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKAMT ) )
		flags |= RENDERFADE_AMT;
	if ( !FBitSet( pev->spawnflags, SF_RENDER_MASKCOLOR ) )
		flags |= RENDERFADE_COLOR;
	if ( pev->scale )
		flags |= RENDERFADE_SCALE;

	int id = RenderFade_Start( pTarget, flags, pev->renderamt * fAmtFactor, pev->rendercolor, pev->scale, pev->frags );

	if ( bIsFirst )
	{
		m_hFirstFaded = pTarget;
		m_bFireAfterFade = !FStringNull( pev->netname );
	}

	if ( m_iFaded < RENDER_MAX_FADED )
	{
		m_hFaded[ m_iFaded ] = pTarget;
		m_iFadeId[ m_iFaded ] = id;
		m_iFaded++;
	}
	else
		ALERT( at_console, "env_render \"%s\": more than %d targets; the others are not removed or finished\n", STRING( pev->targetname ), RENDER_MAX_FADED );
}

// the fade is over: Featureful's render_fader end (trigger after fading, remove target), and remove self
void CRenderFxManager :: FadeDoneThink( void )
{
	SetThink( NULL );

	for ( int i = 0; i < m_iFaded; i++ )
	{
		CBaseEntity *pTarget = m_hFaded[i];
		if ( !pTarget )
			continue;

		BOOL bCurrent = m_iFadeId[i] == 0 || RenderFade_IsCurrent( pTarget, m_iFadeId[i] );
		RenderFade_Finish( pTarget, m_iFadeId[i] );

		// a later fade or change of the entity took over: it decides
		// removed now: a think of its own (cyclers, monsters) or a brush mover's ltime would outlast a SUB_Remove
		// think. Never a player or the world.
		if ( bCurrent && ( pev->spawnflags & SF_RENDER_KILLTARGET ) && !pTarget->IsPlayer() && pTarget->entindex() > 0 )
			UTIL_Remove( pTarget );
	}

	CBaseEntity *pFirst = m_hFirstFaded;
	if ( m_bFireAfterFade && !FStringNull( pev->netname ) )
		FireTargets( STRING( pev->netname ), pFirst ? pFirst : this, this, USE_TOGGLE, 0 );

	m_iFaded = 0;
	m_hFirstFaded = NULL;
	m_bFireAfterFade = FALSE;

	if ( pev->spawnflags & SF_RENDER_ONLYONCE )
	{
		SetThink( &CRenderFxManager::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}



class CBaseTrigger : public CBaseToggle
{
public:
	void EXPORT TeleportTouch ( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT MultiTouch( CBaseEntity *pOther );
	void EXPORT HurtTouch ( CBaseEntity *pOther );
	void EXPORT CDAudioTouch ( CBaseEntity *pOther );
	void ActivateMultiTrigger( CBaseEntity *pActivator );
	void EXPORT MultiWaitOver( void );
	void EXPORT CounterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void InitTrigger( void );

	// SBO 2.1: On while a touch would set it off, Off while it is switched off or waits after firing ("Delay before
	// reset"); a trigger_once that fired is removed a moment later (Spirit of Half-Life gave triggers no state: they
	// read as On)
	virtual STATE GetState( CBaseEntity *pActivator )
	{
		if ( pev->solid == SOLID_NOT || m_pfnThink == static_cast<BASEPTR>( &CBaseTrigger::MultiWaitOver )
			|| m_pfnThink == static_cast<BASEPTR>( &CBaseEntity::SUB_Remove ) )
			return STATE_OFF;
		return STATE_ON;
	}
	virtual int	ObjectCaps( void ) { return CBaseToggle :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	// SBO 2.0, from Featureful (halflife-featureful dlls/triggers.cpp CBaseTrigger, Half-Life 1 SDK license):
	// "Obey Trigger Mode" (m_iObeyTriggerMode) 1: ToggleUse (trigger_hurt, trigger_push, trigger_monsterjump) turns the
	// trigger on for On, off for Off, and toggles for Toggle. 0 (default): every use type toggles, as in Half-Life.
	BOOL m_iObeyTriggerMode;
};

LINK_ENTITY_TO_CLASS( trigger, CBaseTrigger );

/*
================
InitTrigger
================
*/
void CBaseTrigger::InitTrigger( )
{
	// trigger angles are used for one-way touches.  An angle of 0 is assumed
	// to mean no restrictions, so use a yaw of 360 instead.
	if (pev->angles != g_vecZero)
		SetMovedir(pev);
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL(ENT(pev), STRING(pev->model));    // set size and link into world
	if ( CVAR_GET_FLOAT("showtriggers") == 0 )
		SetBits( pev->effects, EF_NODRAW );
}


//
// Cache user-entity-field values until spawn is called.
//

void CBaseTrigger :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "count"))
	{
		m_cTriggersLeft = (int) atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damagetype"))
	{
		m_bitsDamageInflict = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iObeyTriggerMode"))	// SBO 2.0 (Featureful)
	{
		m_iObeyTriggerMode = atoi(pkvd->szValue) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

class CTriggerHurt : public CBaseTrigger
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT RadiationThink( void );
	void EXPORT HurtNonMovingThink( void );	// SBO 2.0, from Featureful: flag 64
};

LINK_ENTITY_TO_CLASS( trigger_hurt, CTriggerHurt );

void CTriggerHurt::KeyValue( KeyValueData *pkvd )
{
	// SBO 2.0, from Featureful: "Untouchable entity name", kept in netname (HurtTouch); no map sets it
	if ( FStrEq( pkvd->szKeyName, "untouchable_ent_name" ) )
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	// SBO 2.0, from Featureful: "Gibbing policy" (0 default, 1 always gib, 2 never gib; kept in armortype:
	// Featureful kept it in impulse, which the multiplayer touch code also uses) and "Minimum health threshold"
	else if ( FStrEq( pkvd->szKeyName, "gib_policy" ) )
	{
		pev->armortype = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "min_health" ) )
	{
		pev->dmg_save = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}

//
// trigger_monsterjump
//
class CTriggerMonsterJump : public CBaseTrigger
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Touch( CBaseEntity *pOther );
	void Think( void );
};

LINK_ENTITY_TO_CLASS( trigger_monsterjump, CTriggerMonsterJump );


void CTriggerMonsterJump :: Spawn ( void )
{
	SetMovedir ( pev );
	
	InitTrigger ();

	pev->nextthink = 0;
	// SBO 2.0: the map's speed and height keys (FGD); 200 and 150 when unset or 0. 1.38 (as Half-Life) set 200 and
	// 150 whatever the map said; maps/map_compat.txt drops the keys on custom maps that set other values.
	if ( pev->speed == 0 )
		pev->speed = 200;
	if ( m_flHeight == 0 )
		m_flHeight = 150;

	if ( !FStringNull ( pev->targetname ) )
	{// if targetted, spawn turned off
		pev->solid = SOLID_NOT;
		UTIL_SetOrigin( pev, pev->origin ); // Unlink from trigger list
		SetUse( &CTriggerMonsterJump::ToggleUse );
	}
}


void CTriggerMonsterJump :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "height" ) )
	{
		m_flHeight = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}


void CTriggerMonsterJump :: Think( void )
{
	pev->solid = SOLID_NOT;// kill the trigger for now !!!UNDONE
	UTIL_SetOrigin( pev, pev->origin ); // Unlink from trigger list
	SetThink( NULL );
}

void CTriggerMonsterJump :: Touch( CBaseEntity *pOther )
{
	entvars_t *pevOther = pOther->pev;

	if ( !FBitSet ( pevOther->flags , FL_MONSTER ) ) 
	{// touched by a non-monster.
		return;
	}

	// SBO 2.0: "master": while it is locked for the monster, no throw (as trigger_hurt / trigger_jump). 1.38 (as
	// Half-Life) offered Master and never checked it.
	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	pevOther->origin.z += 1;
	
	if ( FBitSet ( pevOther->flags, FL_ONGROUND ) ) 
	{// clear the onground so physics don't bitch
		pevOther->flags &= ~FL_ONGROUND;
	}

	// toss the monster!
	pevOther->velocity = pev->movedir * pev->speed;
	pevOther->velocity.z += m_flHeight;
	pev->nextthink = gpGlobals->time;
}


//=====================================
//
// trigger_cdaudio - starts/stops cd audio tracks
//
class CTriggerCDAudio : public CBaseTrigger
{
public:
	void Spawn( void );

	void EXPORT CDAudioUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void PlayTrack( void );
	void EXPORT CDAudioTouch( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( trigger_cdaudio, CTriggerCDAudio );

//
// Changes tracks or stops CD when player touches
//
// !!!HACK - overloaded HEALTH to avoid adding new field
void CTriggerCDAudio :: CDAudioTouch ( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{// only clients may trigger these events
		return;
	}

	PlayTrack();
}

void CTriggerCDAudio :: Spawn( void )
{
	InitTrigger();
	SetTouch( &CTriggerCDAudio::CDAudioTouch );
	SetUse( &CTriggerCDAudio::CDAudioUse );
}

void CTriggerCDAudio::CDAudioUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	PlayTrack();
}

// SBO 2.0 (Featureful 3785caca): the track plays (or stops) for every player in the game, not only the player in
// the first slot; only to players in the server (UTIL_PlayerByIndex: put in the server; has_disconnected: still
// there), never to an empty or connecting slot
void PlayCDTrack( int iTrack )
{
	if ( iTrack < -1 || iTrack > 30 )
	{
		ALERT ( at_console, "TriggerCDAudio - Track %d out of range\n" );
		return;
	}

	char buffer[64];
	const char *commandStr = NULL;

	if ( iTrack == -1 )
	{
		commandStr = "cd pause\n";
	}
	else
	{
		sprintf( buffer, "cd play %3d\n", iTrack );
		commandStr = buffer;
	}

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pClient = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pClient && !pClient->has_disconnected )
		{
			CLIENT_COMMAND( pClient->edict(), (char *)commandStr );
		}
	}
}


void CTriggerCDAudio :: PlayTrack( void )
{
	SetTouch( NULL );
	SetUse( NULL );
	PlayCDTrack( (int)pev->health );
	UTIL_Remove( this );
}


// This plays a CD track when fired or when the player enters it's radius
class CTargetCDAudio : public CPointEntity
{
public:
	void			Spawn( void );
	void			KeyValue( KeyValueData *pkvd );

	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void			Think( void );
	void			Play( void );
};

LINK_ENTITY_TO_CLASS( target_cdaudio, CTargetCDAudio );

void CTargetCDAudio :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->scale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTargetCDAudio :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	if ( pev->scale > 0 )
		pev->nextthink = gpGlobals->time + 1.0;
}

void CTargetCDAudio::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Play();
}

// SBO 2.0 (Featureful 3785caca): plays when any player comes within the radius (Half-Life checked only the player in
// the first slot and stopped checking while that slot was empty)
void CTargetCDAudio::Think( void )
{
	pev->nextthink = gpGlobals->time + 0.5;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pClient = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pClient && !pClient->has_disconnected )
		{
			if ( (pClient->pev->origin - pev->origin).Length() <= pev->scale )
			{
				Play();
				break;
			}
		}
	}
}

void CTargetCDAudio::Play( void ) 
{ 
	PlayCDTrack( (int)pev->health );
	UTIL_Remove(this); 
}

//=====================================

//
// trigger_hurt - hurts anything that touches it. if the trigger has a targetname, firing it will toggle state
//
//int gfToggleState = 0; // used to determine when all radiation trigger hurts have called 'RadiationThink'

void CTriggerHurt :: Spawn( void )
{
	InitTrigger();
	SetTouch ( &CTriggerHurt::HurtTouch );

	if ( !FStringNull ( pev->targetname ) )
	{
		SetUse ( &CTriggerHurt::ToggleUse );
	}
	else
	{
		SetUse ( NULL );
	}

	if (m_bitsDamageInflict & DMG_RADIATION)
	{
		SetThink ( &CTriggerHurt::RadiationThink );
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.0, 0.5); 
	}
	else if ( FBitSet( pev->spawnflags, SF_TRIGGER_HURT_AFFECT_NON_MOVING_MONSTERS ) )	// SBO 2.0 (Featureful)
	{
		SetThink( &CTriggerHurt::HurtNonMovingThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}

	if ( FBitSet (pev->spawnflags, SF_TRIGGER_HURT_START_OFF) )// if flagged to Start Turned Off, make trigger nonsolid.
		pev->solid = SOLID_NOT;

	UTIL_SetOrigin( pev, pev->origin );		// Link into the list
}

// trigger hurt that causes radiation will do a radius
// check and set the player's geiger counter level
// according to distance from center of trigger

void CTriggerHurt :: RadiationThink( void )
{

	edict_t *pentPlayer;
	CBasePlayer *pPlayer = NULL;
	float flRange;
	entvars_t *pevTarget;
	Vector vecSpot1;
	Vector vecSpot2;
	Vector vecRange;
	Vector origin;
	Vector view_ofs;

	// check to see if a player is in pvs
	// if not, continue	

	// set origin to center of trigger so that this check works
	origin = pev->origin;
	view_ofs = pev->view_ofs;

	pev->origin = (pev->absmin + pev->absmax) * 0.5;
	pev->view_ofs = pev->view_ofs * 0.0;

	pentPlayer = FIND_CLIENT_IN_PVS(edict());

	pev->origin = origin;
	pev->view_ofs = view_ofs;

	// reset origin

	if (!FNullEnt(pentPlayer))
	{
 
		pPlayer = GetClassPtr( (CBasePlayer *)VARS(pentPlayer));

		pevTarget = VARS(pentPlayer);

		// get range to player;

		vecSpot1 = (pev->absmin + pev->absmax) * 0.5;
		vecSpot2 = (pevTarget->absmin + pevTarget->absmax) * 0.5;
		
		vecRange = vecSpot1 - vecSpot2;
		flRange = vecRange.Length();

		// if player's current geiger counter range is larger
		// than range to this trigger hurt, reset player's
		// geiger counter range 

		if (pPlayer->m_flgeigerRange >= flRange)
			pPlayer->m_flgeigerRange = flRange;
	}

	pev->nextthink = gpGlobals->time + 0.25;
}

// SBO 2.0, from Featureful (CTriggerHurt::HurtNonMovingMonsters, Half-Life 1 SDK license): flag 64 "Affect
// non-moving NPC's": a monster standing still touches no trigger, so every 0.5 s the monsters inside the trigger's
// box (its bounds: a non-box brush counts its whole box) that are not moving take the damage as if they touched it
void CTriggerHurt :: HurtNonMovingThink( void )
{
	pev->nextthink = gpGlobals->time + 0.5;
	if ( pev->dmgtime > gpGlobals->time )
		pev->nextthink = min( pev->nextthink, pev->dmgtime );
	if ( pev->solid == SOLID_NOT || pev->dmgtime > gpGlobals->time )
		return;

	CBaseEntity *pList[32];
	int count = UTIL_EntitiesInBox( pList, ARRAYSIZE( pList ), pev->absmin, pev->absmax, FL_MONSTER );
	for ( int i = 0; i < count; i++ )
	{
		CBaseMonster *pMonster = pList[i]->MyMonsterPointer();
		if ( pMonster && !pMonster->IsMoving() && !( pMonster->pev->flags & FL_KILLME ) )
			HurtTouch( pMonster );	// the touch code: its filters, the half-second clock, the target
	}
}

//
// ToggleUse - If this is the USE function for a trigger, its state will toggle every time it's fired
//
void CBaseTrigger :: ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SBO 2.0 (Featureful): "Obey Trigger Mode": On when on, Off when off do nothing
	if ( m_iObeyTriggerMode && !ShouldToggle( useType, pev->solid != SOLID_NOT ) )
		return;

	if (pev->solid == SOLID_NOT)
	{// if the trigger is off, turn it on
		pev->solid = SOLID_TRIGGER;
		
		// Force retouch
		gpGlobals->force_retouch++;
	}
	else
	{// turn the trigger off
		pev->solid = SOLID_NOT;
	}
	UTIL_SetOrigin( pev, pev->origin );
}

// When touched, a hurt trigger does DMG points of damage each half-second
void CBaseTrigger :: HurtTouch ( CBaseEntity *pOther )
{
	float fldmg;

	if ( !pOther->pev->takedamage )
		return;

	if ( (pev->spawnflags & SF_TRIGGER_HURT_CLIENTONLYTOUCH) && !pOther->IsPlayer() )
	{
		// this trigger is only allowed to touch clients, and this ain't a client.
		return;
	}

	if ( (pev->spawnflags & SF_TRIGGER_HURT_NO_CLIENTS) && pOther->IsPlayer() )
		return;

	// SBO 2.0, from Featureful: "Untouchable entity name" (netname): an entity of this class name or name is not hurt
	if ( !FStringNull( pev->netname ) && ( FClassnameIs( pOther->pev, STRING( pev->netname ) )
		|| ( !FStringNull( pOther->pev->targetname ) && FStrEq( STRING( pOther->pev->targetname ), STRING( pev->netname ) ) ) ) )
		return;

	// SBO 2.0: "master": while it is locked for the toucher, no damage and no target (as trigger_multiple)
	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	// HACKHACK -- In multiplayer, players touch this based on packet receipt.
	// So the players who send packets later aren't always hurt.  Keep track of
	// how much time has passed and whether or not you've touched that player
	if ( g_pGameRules->IsMultiplayer() )
	{
		if ( pev->dmgtime > gpGlobals->time )
		{
			if ( gpGlobals->time != pev->pain_finished )
			{// too early to hurt again, and not same frame with a different entity
				if ( pOther->IsPlayer() )
				{
					int playerMask = 1 << (pOther->entindex() - 1);

					// If I've already touched this player (this time), then bail out
					if ( pev->impulse & playerMask )
						return;

					// Mark this player as touched
					// BUGBUG - There can be only 32 players!
					pev->impulse |= playerMask;
				}
				else
				{
					return;
				}
			}
		}
		else
		{
			// New clock, "un-touch" all players
			pev->impulse = 0;
			if ( pOther->IsPlayer() )
			{
				int playerMask = 1 << (pOther->entindex() - 1);

				// Mark this player as touched
				// BUGBUG - There can be only 32 players!
				pev->impulse |= playerMask;
			}
		}
	}
	else	// Original code -- single player
	{
		if ( pev->dmgtime > gpGlobals->time && gpGlobals->time != pev->pain_finished )
		{// too early to hurt again, and not same frame with a different entity
			return;
		}
	}



	// If this is time_based damage (poison, radiation), override the pev->dmg with a 
	// default for the given damage type.  Monsters only take time-based damage
	// while touching the trigger.  Player continues taking damage for a while after
	// leaving the trigger

	fldmg = pev->dmg * 0.5;	// 0.5 seconds worth of damage, pev->dmg is damage/second

	// SBO 2.0 (Featureful): "Full dmg every 0.5 seconds"
	if ( FBitSet( pev->spawnflags, SF_TRIGGER_HURT_FULL_DAMAGE_EVERY_HALF_SECOND ) )
		fldmg = pev->dmg;


	// JAY: Cut this because it wasn't fully realized.  Damage is simpler now.
#if 0
	switch (m_bitsDamageInflict)
	{
	default: break;
	case DMG_POISON:		fldmg = POISON_DAMAGE/4; break;
	case DMG_NERVEGAS:		fldmg = NERVEGAS_DAMAGE/4; break;
	case DMG_RADIATION:		fldmg = RADIATION_DAMAGE/4; break;
	case DMG_PARALYZE:		fldmg = PARALYZE_DAMAGE/4; break; // UNDONE: cut this? should slow movement to 50%
	case DMG_ACID:			fldmg = ACID_DAMAGE/4; break;
	case DMG_SLOWBURN:		fldmg = SLOWBURN_DAMAGE/4; break;
	case DMG_SLOWFREEZE:	fldmg = SLOWFREEZE_DAMAGE/4; break;
	}
#endif

	if ( fldmg < 0 )
	{
		// HL25: don't heal dead players in multiplayer
		BOOL bApplyHeal = TRUE;

		if ( g_pGameRules->IsMultiplayer() && pOther->IsPlayer() )
		{
			bApplyHeal = pOther->pev->deadflag == DEAD_NO;
		}

		if ( bApplyHeal )
		{
			pOther->TakeHealth( -fldmg, m_bitsDamageInflict );
		}
	}
	else
	{
		// SBO 2.0, from Featureful: gibbing policy, Ignore Armor (256), No Camera Punch (512), minimum health
		int bitsDamage = m_bitsDamageInflict;
		if ( pev->armortype == 1 )
			bitsDamage |= DMG_ALWAYSGIB;
		else if ( pev->armortype == 2 )
			bitsDamage |= DMG_NEVERGIB;
		Featureful_TakeDamage( pOther, pev, pev, fldmg, bitsDamage, pev->dmg_save,
			FBitSet( pev->spawnflags, SF_TRIGGER_HURT_IGNORE_ARMOR ), FBitSet( pev->spawnflags, SF_TRIGGER_HURT_NO_PUNCH ) );
	}

	// Store pain time so we can get all of the other entities on this frame
	pev->pain_finished = gpGlobals->time;

	// Apply damage every half second
	pev->dmgtime = gpGlobals->time + 0.5;// half second delay until this trigger can hurt toucher again

  
	
	// SBO 2.0: a Kill target alone fires too (Half-Life ran SUB_UseTargets, and with it the kill target, only with a
	// target)
	if ( pev->target || m_iszKillTarget )
	{
		// trigger has a target it wants to fire. 
		if ( pev->spawnflags & SF_TRIGGER_HURT_CLIENTONLYFIRE )
		{
			// if the toucher isn't a client, don't fire the target!
			if ( !pOther->IsPlayer() )
			{
				return;
			}
		}

		SUB_UseTargets( pOther, USE_TOGGLE, 0 );
		if ( pev->spawnflags & SF_TRIGGER_HURT_TARGETONCE )
		{
			pev->target = 0;
			m_iszKillTarget = 0;	// SBO 2.0: once means the kill target too
		}
	}
}


/*QUAKED trigger_multiple (.5 .5 .5) ? notouch
Variable sized repeatable trigger.  Must be targeted at one or more entities.
If "health" is set, the trigger must be killed to activate each time.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings. (.2 default)
If notouch is set, the trigger is only fired by other entities, not by touching.
NOTOUCH has been obsoleted by trigger_relay!
sounds
1)      secret
2)      beep beep
3)      large switch
4)
NEW
if a trigger has a NETNAME, that NETNAME will become the TARGET of the triggered object.
*/
class CTriggerMultiple : public CBaseTrigger
{
public:
	void Spawn( void );
	void Precache( void );
};

LINK_ENTITY_TO_CLASS( trigger_multiple, CTriggerMultiple );

//=========================================================
// trigger_inout -- fires on entering and on leaving, for each entity (the entity is the activator).
//
// Featureful's entity (halflife-featureful dlls/triggers.cpp CTriggerInOut, from Spirit of Half-Life; Half-Life 1
// SDK license): the same keys (target, m_iszAltTarget, m_iszBothTarget, master, netname "triggered only by
// entity") and flags (1 monsters, 2 no clients, 4 pushables, 8 everything else). docs/STATE_LOGIC.md.
// SBO: the entities inside are kept in the trigger itself (Featureful made an "inout_register" edict for each);
// a player who leaves play inside (dies, turns spectator or Game Master, disconnects) has left; at most
// INOUT_MAX_INSIDE entities are tracked at once.
//=========================================================

#define SF_INOUT_EVERYTHING		8	// Featureful SF_TRIGGER_EVERYTHING
#define INOUT_MAX_INSIDE		64

class CTriggerInOut : public CBaseTrigger
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Touch( CBaseEntity *pOther );
	void Think( void );
	BOOL CanTouch( CBaseEntity *pOther );
	void FireOnEntry( CBaseEntity *pOther );
	void FireOnLeaving( CBaseEntity *pOther );
	// SBO 2.1 (Spirit of Half-Life): On while an entity it counts is inside, Off while it is empty
	STATE GetState( CBaseEntity *pActivator )
	{
		for ( int i = 0; i < m_iInside; i++ )
			if ( m_hInside[i] != NULL )
				return STATE_ON;
		return STATE_OFF;
	}

	string_t m_iszAltTarget;
	string_t m_iszBothTarget;
	EHANDLE m_hInside[ INOUT_MAX_INSIDE ];
	int m_iInside;
};

LINK_ENTITY_TO_CLASS( trigger_inout, CTriggerInOut );

void CTriggerInOut::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszAltTarget" ) )
	{
		m_iszAltTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszBothTarget" ) )
	{
		m_iszBothTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}

void CTriggerInOut::Spawn( void )
{
	InitTrigger();
	m_iInside = 0;
	pev->nextthink = -1;
}

// Featureful CBaseTrigger::CanTouch (trigger_inout, trigger_jump, trigger_safe_fall): "netname" an entity's class
// name or name, else the flags 1 monsters, 2 no clients, 4 pushables, 8 everything else. SBO: a player counts only
// while in play (never a spectator, Game Master, dead or waiting player); a dying monster never
static BOOL Featureful_CanTouch( entvars_t *pev, CBaseEntity *pOther )
{
	entvars_t *pevToucher = pOther->pev;

	if ( pevToucher->flags & FL_KILLME )
		return FALSE;
	if ( ( pevToucher->flags & FL_MONSTER ) && !pOther->IsPlayer() && pevToucher->deadflag != DEAD_NO )
		return FALSE;

	if ( pOther->IsPlayer() && !Logic_IsPlayerInPlay( pOther ) )
		return FALSE;

	if ( FStringNull( pev->netname ) )
	{
		if ( pevToucher->flags & FL_CLIENT )
			return !FBitSet( pev->spawnflags, SF_TRIGGER_NOCLIENTS );
		else if ( pevToucher->flags & FL_MONSTER )
			return FBitSet( pev->spawnflags, SF_TRIGGER_ALLOWMONSTERS );
		else if ( FClassnameIs( pevToucher, "func_pushable" ) )
			return FBitSet( pev->spawnflags, SF_TRIGGER_PUSHABLES );
		else
			return FBitSet( pev->spawnflags, SF_INOUT_EVERYTHING );
	}

	// "netname": only an entity with this class name or targetname
	return FClassnameIs( pevToucher, STRING( pev->netname ) )
		|| ( !FStringNull( pevToucher->targetname ) && FStrEq( STRING( pevToucher->targetname ), STRING( pev->netname ) ) );
}

BOOL CTriggerInOut::CanTouch( CBaseEntity *pOther )
{
	return Featureful_CanTouch( pev, pOther );
}

void CTriggerInOut::Touch( CBaseEntity *pOther )
{
	if ( !CanTouch( pOther ) )
		return;

	for ( int i = 0; i < m_iInside; i++ )
	{
		if ( (CBaseEntity *)m_hInside[i] == pOther )
			return;	// already inside
	}

	if ( m_iInside >= INOUT_MAX_INSIDE )
		return;	// full: entities beyond the limit are not tracked

	m_hInside[ m_iInside++ ] = pOther;
	FireOnEntry( pOther );

	if ( pev->nextthink <= 0 && m_iInside )
		pev->nextthink = gpGlobals->time + 0.1;
}

// every 0.1 s (Featureful): who has left
void CTriggerInOut::Think( void )
{
	int kept = 0;

	for ( int i = 0; i < m_iInside; i++ )
	{
		CBaseEntity *pEnt = m_hInside[i];

		if ( !pEnt )
			continue;	// removed: gone without a "leaving" (Featureful)

		BOOL bLeft = ( pEnt->pev->flags & FL_KILLME ) || !Intersects( pEnt )
			|| ( pEnt->IsPlayer() && !Logic_IsPlayerInPlay( pEnt ) );

		if ( bLeft )
		{
			m_hInside[i] = NULL;
			FireOnLeaving( pEnt );
		}
		else
			m_hInside[ kept++ ] = pEnt;
	}

	// (a target fired above may add an entity at the end through Touch: the loop runs to the current count, so it
	// is checked and kept like the others)
	for ( int i = kept; i < m_iInside; i++ )
		m_hInside[i] = NULL;
	m_iInside = kept;

	pev->nextthink = m_iInside ? gpGlobals->time + 0.1 : -1;
}

void CTriggerInOut::FireOnEntry( CBaseEntity *pOther )
{
	if ( UTIL_IsMasterTriggered( m_sMaster, pOther ) )
	{
		if ( m_iszBothTarget )
			FireTargets( STRING( m_iszBothTarget ), pOther, this, USE_ON, 0 );
		if ( pev->target )
			FireTargets( STRING( pev->target ), pOther, this, USE_TOGGLE, 0 );
	}
}

void CTriggerInOut::FireOnLeaving( CBaseEntity *pEnt )
{
	if ( UTIL_IsMasterTriggered( m_sMaster, pEnt ) )
	{
		if ( m_iszBothTarget )
			FireTargets( STRING( m_iszBothTarget ), pEnt, this, USE_OFF, 0 );
		if ( m_iszAltTarget )
			FireTargets( STRING( m_iszAltTarget ), pEnt, this, USE_TOGGLE, 0 );
	}
}

//=========================================================
// trigger_safe_fall -- an entity falling through it takes no fall damage when it lands (players)
//
// Featureful's entity (halflife-featureful dlls/triggers.cpp CTriggerSafeFall, commit e8ed979, 2026-10-12; Half-Life 1
// SDK license; the Quake II PSX mod's idea). Keys: master. SBO: only players in play; besides the damage, SBO's
// broken legs and the splat sound of a bad fall are skipped too (CBasePlayer::PostThink, m_bSafeFall).
//=========================================================

class CTriggerSafeFall : public CBaseTrigger
{
public:
	void Spawn( void )
	{
		InitTrigger();
	}
	void Touch( CBaseEntity *pOther )
	{
		if ( !pOther || !pOther->IsPlayer() || FBitSet( pOther->pev->flags, FL_ONGROUND ) )
			return;
		if ( !Logic_IsPlayerInPlay( pOther ) )	// SBO
			return;
		if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
			return;
		( (CBasePlayer *)pOther )->m_bSafeFall = TRUE;
	}
};

LINK_ENTITY_TO_CLASS( trigger_safe_fall, CTriggerSafeFall );

//=========================================================
// trigger_jump -- a jump pad: launches the toucher on a ballistic path to the jump target (Ricochet's idea)
//
// Featureful's entity (halflife-featureful dlls/triggers.cpp CTriggerJump, commit 9e435cf, 2026-10-13; Half-Life 1
// SDK license). Keys: target (the jump target [LP]), height (the apex above the toucher; 0 = automatic),
// trigger_on_jump (fired with the toucher as activator), master, netname "triggered only by entity"; flags 1/2/4/8
// as trigger_inout (players by default).
// SBO: only players in play and living monsters are launched; no sound (Featureful plays its soundscripts
// TriggerJump.Self / .Other, empty by default; SBO has no soundscripts). The launch is server-side, as Featureful
// (no pm_shared code): the client gets the new velocity with the next update.
//=========================================================

class CTriggerJump : public CBaseTrigger
{
public:
	void Spawn( void )
	{
		InitTrigger();
		SetTouch( &CTriggerJump::JumpTouch );
	}
	void KeyValue( KeyValueData *pkvd )
	{
		if ( FStrEq( pkvd->szKeyName, "height" ) )
		{
			m_flHeight = atof( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else if ( FStrEq( pkvd->szKeyName, "trigger_on_jump" ) )
		{
			m_triggerOnJump = ALLOC_STRING( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else
			CBaseTrigger::KeyValue( pkvd );
	}
	void EXPORT JumpTouch( CBaseEntity *pOther );

	float m_flHeight;
	string_t m_triggerOnJump;
};

LINK_ENTITY_TO_CLASS( trigger_jump, CTriggerJump );

void CTriggerJump::JumpTouch( CBaseEntity *pOther )
{
	if ( FStringNull( pev->target ) )
		return;

	if ( !Featureful_CanTouch( pev, pOther ) )
		return;

	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	// Don't touch again immediately
	if ( pOther->m_flTouchedByJumpPad > gpGlobals->time )
		return;

	float flGravity = g_psv_gravity ? g_psv_gravity->value : 800;

	if ( pOther->pev->gravity > 0.0f )
		flGravity *= pOther->pev->gravity;
	if ( !( flGravity > 0 ) )
		return;	// SBO: no gravity, no ballistic jump (Featureful divided by it)

	Vector vecTargetOrg;
	if ( !TryCalcLocus_Position( this, pOther, STRING( pev->target ), vecTargetOrg ) )
		return;

	float height = m_flHeight;
	if ( height <= 0.0f )
	{
		if ( vecTargetOrg.z >= pOther->pev->origin.z )
			height = vecTargetOrg.z - pOther->pev->origin.z;
		else
			height = 0;
		height += pOther->pev->maxs.z + 16.0f;
	}

	// get a rough idea of how high to launch
	Vector vecMidPoint = pOther->pev->origin + ( vecTargetOrg - pOther->pev->origin ) * 0.5f;
	vecMidPoint.z = pOther->pev->origin.z + height;

	// How high should we travel to reach the apex
	float distance1 = ( vecMidPoint.z - pOther->pev->origin.z );
	float distance2 = ( vecMidPoint.z - vecTargetOrg.z );

	if ( distance1 < 0 )
		distance1 = 0;
	if ( distance2 < 0 )
		distance2 = 0;

	// How long will it take to travel this distance
	float time1 = sqrt( distance1 / ( 0.5f * flGravity ) );
	float time2 = sqrt( distance2 / ( 0.5f * flGravity ) );
	if ( time1 < 0.1f )
		return;

	// how hard to launch to get there in time.
	Vector vecTargetVel = ( vecTargetOrg - pOther->pev->origin ) / ( time1 + time2 );
	vecTargetVel.z = flGravity * time1;

	// don't affect the toucher again for a bit
	pOther->m_flTouchedByJumpPad = gpGlobals->time + 0.2f;
	pOther->pev->velocity = vecTargetVel;
	ClearBits( pOther->pev->flags, FL_ONGROUND );
	if ( pOther->IsPlayer() )
		( (CBasePlayer *)pOther )->SetAnimation( PLAYER_JUMP );

	if ( !FStringNull( m_triggerOnJump ) )
		FireTargets( STRING( m_triggerOnJump ), pOther, this, USE_TOGGLE, 0 );
}


// SBO 2.0 (Featureful 8840453f): the "noise" ActivateMultiTrigger plays (trigger_once too). It was never precached,
// so it did not play
void CTriggerMultiple :: Precache( void )
{
	if ( !FStringNull( pev->noise ) )
		PRECACHE_SOUND( (char *)STRING( pev->noise ) );
}

void CTriggerMultiple :: Spawn( void )
{
	Precache();

	if (m_flWait == 0)
		m_flWait = 0.2;

	InitTrigger();

	ASSERTSZ(pev->health == 0, "trigger_multiple with health");
//	UTIL_SetOrigin(pev, pev->origin);
//	SET_MODEL( ENT(pev), STRING(pev->model) );
//	if (pev->health > 0)
//		{
//		if (FBitSet(pev->spawnflags, SPAWNFLAG_NOTOUCH))
//			ALERT(at_error, "trigger_multiple spawn: health and notouch don't make sense");
//		pev->max_health = pev->health;
//UNDONE: where to get pfnDie from?
//		pev->pfnDie = multi_killed;
//		pev->takedamage = DAMAGE_YES;
//		pev->solid = SOLID_BBOX;
//		UTIL_SetOrigin(pev, pev->origin);  // make sure it links into the world
//		}
//	else
		{
			SetTouch( &CTriggerMultiple::MultiTouch );
		}
	}


/*QUAKED trigger_once (.5 .5 .5) ? notouch
Variable sized trigger. Triggers once, then removes itself.  You must set the key "target" to the name of another object in the level that has a matching
"targetname".  If "health" is set, the trigger must be killed to activate.
If notouch is set, the trigger is only fired by other entities, not by touching.
if "killtarget" is set, any objects that have a matching "target" will be removed when the trigger is fired.
if "angle" is set, the trigger will only fire when someone is facing the direction of the angle.  Use "360" for an angle of 0.
sounds
1)      secret
2)      beep beep
3)      large switch
4)
*/
class CTriggerOnce : public CTriggerMultiple
{
public:
	void Spawn( void );
};

LINK_ENTITY_TO_CLASS( trigger_once, CTriggerOnce );
void CTriggerOnce::Spawn( void )
{
	m_flWait = -1;
	
	CTriggerMultiple :: Spawn();
}



void CBaseTrigger :: MultiTouch( CBaseEntity *pOther )
{
	entvars_t	*pevToucher;

	pevToucher = pOther->pev;

	// SBO 2.0, from Featureful (CBaseTrigger::CanTouch): flag 8 "Everything else": any other entity too (a
	// func_vehicle's touch proxy, grenades, items, ...); not a player, monster, pushable or entity being removed.
	// No map sets the flag on these triggers
	const BOOL fEverythingElse = FBitSet( pev->spawnflags, SF_INOUT_EVERYTHING )
		&& !( pevToucher->flags & ( FL_CLIENT | FL_MONSTER | FL_KILLME ) ) && !FClassnameIs( pevToucher, "func_pushable" )
		&& pOther->pev->solid != SOLID_TRIGGER;

	// Only touch clients, monsters, or pushables (depending on flags)
	if ( ((pevToucher->flags & FL_CLIENT) && !(pev->spawnflags & SF_TRIGGER_NOCLIENTS)) ||
		 ((pevToucher->flags & FL_MONSTER) && (pev->spawnflags & SF_TRIGGER_ALLOWMONSTERS)) ||
		 (pev->spawnflags & SF_TRIGGER_PUSHABLES) && FClassnameIs(pevToucher,"func_pushable") ||
		 fEverythingElse )
	{

#if 0
		// if the trigger has an angles field, check player's facing direction
		if (pev->movedir != g_vecZero)
		{
			UTIL_MakeVectors( pevToucher->angles );
			if ( DotProduct( gpGlobals->v_forward, pev->movedir ) < 0 )
				return;         // not facing the right way
		}
#endif
		
		ActivateMultiTrigger( pOther );
	}
}


//
// the trigger was just touched/killed/used
// self.enemy should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
//
void CBaseTrigger :: ActivateMultiTrigger( CBaseEntity *pActivator )
{
	if (pev->nextthink > gpGlobals->time)
		return;         // still waiting for reset time

	if (!UTIL_IsMasterTriggered(m_sMaster,pActivator))
		return;

	if (FClassnameIs(pev, "trigger_secret"))
	{
		if ( pev->enemy == NULL || !FClassnameIs(pev->enemy, "player"))
			return;
		gpGlobals->found_secrets++;
	}

	if (!FStringNull(pev->noise))
		EMIT_SOUND(ENT(pev), CHAN_VOICE, (char*)STRING(pev->noise), 1, ATTN_NORM);

// don't trigger again until reset
// pev->takedamage = DAMAGE_NO;

	m_hActivator = pActivator;
	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );

	if ( pev->message && pActivator->IsPlayer() )
	{
		UTIL_ShowMessage( STRING(pev->message), pActivator );
//		CLIENT_PRINTF( ENT( pActivator->pev ), print_center, STRING(pev->message) );
	}

	if (m_flWait > 0)
	{
		SetThink( &CTriggerOnce::MultiWaitOver );
		pev->nextthink = gpGlobals->time + m_flWait;
	}
	else
	{
		// we can't just remove (self) here, because this is a touch function
		// called while C code is looping through area links...
		SetTouch( NULL );
		pev->nextthink = gpGlobals->time + 0.1;
		SetThink(  &CTriggerOnce::SUB_Remove );
	}
}


// the wait time has passed, so set back up for another activation
void CBaseTrigger :: MultiWaitOver( void )
{
//	if (pev->max_health)
//		{
//		pev->health		= pev->max_health;
//		pev->takedamage	= DAMAGE_YES;
//		pev->solid		= SOLID_BBOX;
//		}
	SetThink( NULL );
}


// ========================= COUNTING TRIGGER =====================================

//
// GLOBALS ASSUMED SET:  g_eoActivator
//
void CBaseTrigger::CounterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_cTriggersLeft--;
	m_hActivator = pActivator;

	if (m_cTriggersLeft < 0)
		return;
	
	BOOL fTellActivator =
		(m_hActivator != 0) &&
		FClassnameIs(m_hActivator->pev, "player") &&
		!FBitSet(pev->spawnflags, SPAWNFLAG_NOMESSAGE);
	if (m_cTriggersLeft != 0)
	{
		if (fTellActivator)
		{
			// UNDONE: I don't think we want these Quakesque messages
			switch (m_cTriggersLeft)
			{
			case 1:		ALERT(at_console, "Only 1 more to go...");		break;
			case 2:		ALERT(at_console, "Only 2 more to go...");		break;
			case 3:		ALERT(at_console, "Only 3 more to go...");		break;
			default:	ALERT(at_console, "There are more to go...");	break;
			}
		}
		return;
	}

	// !!!UNDONE: I don't think we want these Quakesque messages
	if (fTellActivator)
		ALERT(at_console, "Sequence completed!");
	
	ActivateMultiTrigger( m_hActivator );
}


/*QUAKED trigger_counter (.5 .5 .5) ? nomessage
Acts as an intermediary for an action that takes multiple inputs.
If nomessage is not set, it will print "1 more.. " etc when triggered and
"sequence complete" when finished.  After the counter has been triggered "cTriggersLeft"
times (default 2), it will fire all of it's targets and remove itself.
*/
class CTriggerCounter : public CBaseTrigger
{
public:
	void Spawn( void );
};
LINK_ENTITY_TO_CLASS( trigger_counter, CTriggerCounter );

void CTriggerCounter :: Spawn( void )
{
	// By making the flWait be -1, this counter-trigger will disappear after it's activated
	// (but of course it needs cTriggersLeft "uses" before that happens).
	m_flWait = -1;

	if (m_cTriggersLeft == 0)
		m_cTriggersLeft = 2;
	SetUse( &CTriggerCounter::CounterUse );
}

// ====================== TRIGGER_CHANGELEVEL ================================

class CTriggerVolume : public CPointEntity	// Derive from point entity so this doesn't move across levels
{
public:
	void		Spawn( void );
};

LINK_ENTITY_TO_CLASS( trigger_transition, CTriggerVolume );

// Define space that travels across a level transition
void CTriggerVolume :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL(ENT(pev), STRING(pev->model));    // set size and link into world
	pev->model = NULL;
	pev->modelindex = 0;
}


// Fires a target after level transition and then dies
class CFireAndDie : public CBaseDelay
{
public:
	void Spawn( void );
	void Precache( void );
	void Think( void );
	int ObjectCaps( void ) { return CBaseDelay::ObjectCaps() | FCAP_FORCE_TRANSITION; }	// Always go across transitions
};
LINK_ENTITY_TO_CLASS( fireanddie, CFireAndDie );

void CFireAndDie::Spawn( void )
{
	pev->classname = MAKE_STRING("fireanddie");
	// Don't call Precache() - it should be called on restore
}


void CFireAndDie::Precache( void )
{
	// This gets called on restore
	pev->nextthink = gpGlobals->time + m_flDelay;
}


void CFireAndDie::Think( void )
{
	SUB_UseTargets( this, USE_TOGGLE, 0 );
	UTIL_Remove( this );
}


#define SF_CHANGELEVEL_USEONLY		0x0002
class CChangeLevel : public CBaseTrigger
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT UseChangeLevel ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT TriggerChangeLevel( void );
	void EXPORT ExecuteChangeLevel( void );
	void EXPORT TouchChangeLevel( CBaseEntity *pOther );
	void ChangeLevelNow( CBaseEntity *pActivator );

	static edict_t *FindLandmark( const char *pLandmarkName );
	static int ChangeList( LEVELLIST *pLevelList, int maxList );
	static int AddTransitionToList( LEVELLIST *pLevelList, int listCount, const char *pMapName, const char *pLandmarkName, edict_t *pentLandmark );
	static int InTransitionVolume( CBaseEntity *pEntity, char *pVolumeName );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	char m_szMapName[cchMapNameMost];		// trigger_changelevel only:  next map
	char m_szLandmarkName[cchMapNameMost];		// trigger_changelevel only:  landmark on next map
	int		m_changeTarget;
	float	m_changeTargetDelay;
};
LINK_ENTITY_TO_CLASS( trigger_changelevel, CChangeLevel );

// Global Savedata for changelevel trigger
TYPEDESCRIPTION	CChangeLevel::m_SaveData[] = 
{
	DEFINE_ARRAY( CChangeLevel, m_szMapName, FIELD_CHARACTER, cchMapNameMost ),
	DEFINE_ARRAY( CChangeLevel, m_szLandmarkName, FIELD_CHARACTER, cchMapNameMost ),
	DEFINE_FIELD( CChangeLevel, m_changeTarget, FIELD_STRING ),
	DEFINE_FIELD( CChangeLevel, m_changeTargetDelay, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE(CChangeLevel,CBaseTrigger);

//
// Cache user-entity-field values until spawn is called.
//

void CChangeLevel :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "map"))
	{
		if (strlen(pkvd->szValue) >= cchMapNameMost)
			ALERT( at_error, "Map name '%s' too long (32 chars)\n", pkvd->szValue );
		strncpy(m_szMapName, pkvd->szValue, cchMapNameMost - 1);	// said it was too long, then overflowed the entity
		m_szMapName[cchMapNameMost - 1] = 0;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "landmark"))
	{
		if (strlen(pkvd->szValue) >= cchMapNameMost)
			ALERT( at_error, "Landmark name '%s' too long (32 chars)\n", pkvd->szValue );
		strncpy(m_szLandmarkName, pkvd->szValue, cchMapNameMost - 1);
		m_szLandmarkName[cchMapNameMost - 1] = 0;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "changetarget"))
	{
		m_changeTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "changedelay"))
	{
		m_changeTargetDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}


/*QUAKED trigger_changelevel (0.5 0.5 0.5) ? NO_INTERMISSION
When the player touches this, he gets sent to the map listed in the "map" variable.  Unless the NO_INTERMISSION flag is set, the view will go to the info_intermission spot and display stats.
*/

void CChangeLevel :: Spawn( void )
{
	if ( FStrEq( m_szMapName, "" ) )
		ALERT( at_console, "a trigger_changelevel doesn't have a map" );

	if ( FStrEq( m_szLandmarkName, "" ) )
		ALERT( at_console, "trigger_changelevel to %s doesn't have a landmark", m_szMapName );

	// halflife-updated: warn mappers about a changelevel to the current map
	if ( 0 == stricmp( m_szMapName, STRING( gpGlobals->mapname ) ) )
	{
		ALERT( at_error, "trigger_changelevel points to the current map (%s), which does not work\n", STRING( gpGlobals->mapname ) );
	}


	if (!FStringNull ( pev->targetname ) )
	{
		SetUse ( &CChangeLevel::UseChangeLevel );
	}
	InitTrigger();
	if ( !(pev->spawnflags & SF_CHANGELEVEL_USEONLY) )
		SetTouch( &CChangeLevel::TouchChangeLevel );
//	ALERT( at_console, "TRANSITION: %s (%s)\n", m_szMapName, m_szLandmarkName );
}


void CChangeLevel :: ExecuteChangeLevel( void )
{
	MESSAGE_BEGIN( MSG_ALL, SVC_CDTRACK );
		WRITE_BYTE( 3 );
		WRITE_BYTE( 3 );
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();
}


FILE_GLOBAL char st_szNextMap[cchMapNameMost];
FILE_GLOBAL char st_szNextSpot[cchMapNameMost];

edict_t *CChangeLevel :: FindLandmark( const char *pLandmarkName )
{
	edict_t	*pentLandmark;

	pentLandmark = FIND_ENTITY_BY_TARGETNAME( NULL, pLandmarkName );
	while ( !FNullEnt( pentLandmark ) )
	{
		// Found the landmark
		if ( FClassnameIs( pentLandmark, "info_landmark" ) )
			return pentLandmark;
		else
			pentLandmark = FIND_ENTITY_BY_TARGETNAME( pentLandmark, pLandmarkName );
	}
	ALERT( at_error, "Can't find landmark %s\n", pLandmarkName );
	return NULL;
}


//=========================================================
// CChangeLevel :: Use - allows level transitions to be 
// triggered by buttons, etc.
//
//=========================================================
void CChangeLevel :: UseChangeLevel ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	ChangeLevelNow( pActivator );
}

void CChangeLevel :: ChangeLevelNow( CBaseEntity *pActivator )
{
	edict_t	*pentLandmark;
	LEVELLIST	levels[16];

	ASSERT(!FStrEq(m_szMapName, ""));

	// Don't work in deathmatch
	if ( g_pGameRules->IsDeathmatch() )
		return;

	// Some people are firing these multiple times in a frame, disable
	if ( gpGlobals->time == pev->dmgtime )
		return;

	pev->dmgtime = gpGlobals->time;


	CBaseEntity *pPlayer = CBaseEntity::Instance( g_engfuncs.pfnPEntityOfEntIndex( 1 ) );
	if ( !InTransitionVolume( pPlayer, m_szLandmarkName ) )
	{
		ALERT( at_aiconsole, "Player isn't in the transition volume %s, aborting\n", m_szLandmarkName );
		return;
	}

	// Create an entity to fire the changetarget
	if ( m_changeTarget )
	{
		CFireAndDie *pFireAndDie = GetClassPtr( (CFireAndDie *)NULL );
		if ( pFireAndDie )
		{
			// Set target and delay
			pFireAndDie->pev->target = m_changeTarget;
			pFireAndDie->m_flDelay = m_changeTargetDelay;
			pFireAndDie->pev->origin = pPlayer->pev->origin;
			// Call spawn
			DispatchSpawn( pFireAndDie->edict() );
		}
	}
	// This object will get removed in the call to CHANGE_LEVEL, copy the params into "safe" memory
	strcpy(st_szNextMap, m_szMapName);

	m_hActivator = pActivator;
	SUB_UseTargets( pActivator, USE_TOGGLE, 0 );
	st_szNextSpot[0] = 0;	// Init landmark to NULL

	// look for a landmark entity		
	pentLandmark = FindLandmark( m_szLandmarkName );
	if ( !FNullEnt( pentLandmark ) )
	{
		strcpy(st_szNextSpot, m_szLandmarkName);
		gpGlobals->vecLandmarkOffset = VARS(pentLandmark)->origin;
	}
//	ALERT( at_console, "Level touches %d levels\n", ChangeList( levels, 16 ) );
	ALERT( at_console, "CHANGE LEVEL: %s %s\n", st_szNextMap, st_szNextSpot );
	CHANGE_LEVEL( st_szNextMap, st_szNextSpot );
}

//
// GLOBALS ASSUMED SET:  st_szNextMap
//
void CChangeLevel :: TouchChangeLevel( CBaseEntity *pOther )
{
	if (!FClassnameIs(pOther->pev, "player"))
		return;

	ChangeLevelNow( pOther );
}


// Add a transition to the list, but ignore duplicates 
// (a designer may have placed multiple trigger_changelevels with the same landmark)
int CChangeLevel::AddTransitionToList( LEVELLIST *pLevelList, int listCount, const char *pMapName, const char *pLandmarkName, edict_t *pentLandmark )
{
	int i;

	if ( !pLevelList || !pMapName || !pLandmarkName || !pentLandmark )
		return 0;

	for ( i = 0; i < listCount; i++ )
	{
		if ( pLevelList[i].pentLandmark == pentLandmark && strcmp( pLevelList[i].mapName, pMapName ) == 0 )
			return 0;
	}
	strcpy( pLevelList[listCount].mapName, pMapName );
	strcpy( pLevelList[listCount].landmarkName, pLandmarkName );
	pLevelList[listCount].pentLandmark = pentLandmark;
	pLevelList[listCount].vecLandmarkOrigin = VARS(pentLandmark)->origin;

	return 1;
}

int BuildChangeList( LEVELLIST *pLevelList, int maxList )
{
	return CChangeLevel::ChangeList( pLevelList, maxList );
}


int CChangeLevel::InTransitionVolume( CBaseEntity *pEntity, char *pVolumeName )
{
	edict_t	*pentVolume;


	if ( pEntity->ObjectCaps() & FCAP_FORCE_TRANSITION )
		return 1;

	// If you're following another entity, follow it through the transition (weapons follow the player)
	if ( pEntity->pev->movetype == MOVETYPE_FOLLOW )
	{
		if ( pEntity->pev->aiment != NULL )
			pEntity = CBaseEntity::Instance( pEntity->pev->aiment );
	}

	int inVolume = 1;	// Unless we find a trigger_transition, everything is in the volume

	pentVolume = FIND_ENTITY_BY_TARGETNAME( NULL, pVolumeName );
	while ( !FNullEnt( pentVolume ) )
	{
		CBaseEntity *pVolume = CBaseEntity::Instance( pentVolume );
		
		if ( pVolume && FClassnameIs( pVolume->pev, "trigger_transition" ) )
		{
			if ( pVolume->Intersects( pEntity ) )	// It touches one, it's in the volume
				return 1;
			else
				inVolume = 0;	// Found a trigger_transition, but I don't intersect it -- if I don't find another, don't go!
		}
		pentVolume = FIND_ENTITY_BY_TARGETNAME( pentVolume, pVolumeName );
	}

	return inVolume;
}


// We can only ever move 512 entities across a transition
#define MAX_ENTITY 512

// This has grown into a complicated beast
// Can we make this more elegant?
// This builds the list of all transitions on this level and which entities are in their PVS's and can / should
// be moved across.
int CChangeLevel::ChangeList( LEVELLIST *pLevelList, int maxList )
{
	edict_t	*pentChangelevel, *pentLandmark;
	int			i, count;

	count = 0;

	// Find all of the possible level changes on this BSP
	pentChangelevel = FIND_ENTITY_BY_STRING( NULL, "classname", "trigger_changelevel" );
	if ( FNullEnt( pentChangelevel ) )
		return 0;
	while ( !FNullEnt( pentChangelevel ) )
	{
		CChangeLevel *pTrigger;
		
		pTrigger = GetClassPtr((CChangeLevel *)VARS(pentChangelevel));
		if ( pTrigger )
		{
			// Find the corresponding landmark
			pentLandmark = FindLandmark( pTrigger->m_szLandmarkName );
			if ( pentLandmark )
			{
				// Build a list of unique transitions
				if ( AddTransitionToList( pLevelList, count, pTrigger->m_szMapName, pTrigger->m_szLandmarkName, pentLandmark ) )
				{
					count++;
					if ( count >= maxList )		// FULL!!
						break;
				}
			}
		}
		pentChangelevel = FIND_ENTITY_BY_STRING( pentChangelevel, "classname", "trigger_changelevel" );
	}

	if ( gpGlobals->pSaveData && ((SAVERESTOREDATA *)gpGlobals->pSaveData)->pTable )
	{
		CSave saveHelper( (SAVERESTOREDATA *)gpGlobals->pSaveData );

		for ( i = 0; i < count; i++ )
		{
			int j, entityCount = 0;
			CBaseEntity *pEntList[ MAX_ENTITY ];
			int			 entityFlags[ MAX_ENTITY ];

			// Follow the linked list of entities in the PVS of the transition landmark
			edict_t *pent = UTIL_EntitiesInPVS( pLevelList[i].pentLandmark );

			// Build a list of valid entities in this linked list (we're going to use pent->v.chain again)
			while ( !FNullEnt( pent ) )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance(pent);
				if ( pEntity )
				{
//					ALERT( at_console, "Trying %s\n", STRING(pEntity->pev->classname) );
					int caps = pEntity->ObjectCaps();
					if ( !(caps & FCAP_DONT_SAVE) )
					{
						int flags = 0;

						// If this entity can be moved or is global, mark it
						if ( caps & FCAP_ACROSS_TRANSITION )
							flags |= FENTTABLE_MOVEABLE;
						if ( pEntity->pev->globalname && !pEntity->IsDormant() )
							flags |= FENTTABLE_GLOBAL;
						if ( flags )
						{
							pEntList[ entityCount ] = pEntity;
							entityFlags[ entityCount ] = flags;
							entityCount++;
							if ( entityCount > MAX_ENTITY )
								ALERT( at_error, "Too many entities across a transition!" );
						}
//						else
//							ALERT( at_console, "Failed %s\n", STRING(pEntity->pev->classname) );
					}
//					else
//						ALERT( at_console, "DON'T SAVE %s\n", STRING(pEntity->pev->classname) );
				}
				pent = pent->v.chain;
			}

			for ( j = 0; j < entityCount; j++ )
			{
				// Check to make sure the entity isn't screened out by a trigger_transition
				if ( entityFlags[j] && InTransitionVolume( pEntList[j], pLevelList[i].landmarkName ) )
				{
					// Mark entity table with 1<<i
					int index = saveHelper.EntityIndex( pEntList[j] );
					// Flag it with the level number
					saveHelper.EntityFlagsSet( index, entityFlags[j] | (1<<i) );
				}
//				else
//					ALERT( at_console, "Screened out %s\n", STRING(pEntList[j]->pev->classname) );

			}
		}
	}

	return count;
}

/*
go to the next level for deathmatch
only called if a time or frag limit has expired
*/
void NextLevel( void )
{
	edict_t* pent;
	CChangeLevel *pChange;
	
	// find a trigger_changelevel
	pent = FIND_ENTITY_BY_CLASSNAME(NULL, "trigger_changelevel");
	
	// go back to start if no trigger_changelevel
	if (FNullEnt(pent))
	{
		gpGlobals->mapname = ALLOC_STRING("start");
		pChange = GetClassPtr( (CChangeLevel *)NULL );
		strcpy(pChange->m_szMapName, "start");
	}
	else
		pChange = GetClassPtr( (CChangeLevel *)VARS(pent));
	
	strcpy(st_szNextMap, pChange->m_szMapName);
	g_fGameOver = TRUE;
	
	if (pChange->pev->nextthink < gpGlobals->time)
	{
		pChange->SetThink( &CChangeLevel::ExecuteChangeLevel );
		pChange->pev->nextthink = gpGlobals->time + 0.1;
	}
}


// ============================== LADDER =======================================

class CLadder : public CBaseTrigger
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Precache( void );
};
LINK_ENTITY_TO_CLASS( func_ladder, CLadder );


void CLadder :: KeyValue( KeyValueData *pkvd )
{
	CBaseTrigger::KeyValue( pkvd );
}


//=========================================================
// func_ladder - makes an area vertically negotiable
//=========================================================
void CLadder :: Precache( void )
{
	// Do all of this in here because we need to 'convert' old saved games
	pev->solid = SOLID_NOT;
	pev->skin = CONTENTS_LADDER;
	if ( CVAR_GET_FLOAT("showtriggers") == 0 )
	{
		pev->rendermode = kRenderTransTexture;
		pev->renderamt = 0;
	}
	pev->effects &= ~EF_NODRAW;
}


void CLadder :: Spawn( void )
{
	Precache();

	SET_MODEL(ENT(pev), STRING(pev->model));    // set size and link into world
	pev->movetype = MOVETYPE_PUSH;
}


// ========================== A TRIGGER THAT PUSHES YOU ===============================

class CTriggerPush : public CBaseTrigger
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Touch( CBaseEntity *pOther );
	// SBO 2.1: "angles" / "speed" changed at run time (trigger_changevalue / trigger_copyvalue)
	CKeyValue GetKeyValue( const char *keyName );
	void RuntimeKeyValueChanged( const char *keyName );

	// SBO 2.0, from Featureful / Spirit of Half-Life: "m_iszPushSpeed" speed factor [LR], "m_iszPushVel" push
	// direction [LV] (instead of the angles; times the speed), computed for each toucher
	string_t m_iszPushSpeed;
	string_t m_iszPushVel;
};
LINK_ENTITY_TO_CLASS( trigger_push, CTriggerPush );


void CTriggerPush :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszPushSpeed" ) )	// SBO 2.0 (Featureful)
	{
		m_iszPushSpeed = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszPushVel" ) )	// SBO 2.0 (Featureful)
	{
		m_iszPushVel = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}


/*QUAKED trigger_push (.5 .5 .5) ? TRIG_PUSH_ONCE
Pushes the player
*/

void CTriggerPush :: Spawn( )
{
	if ( pev->angles == g_vecZero )
		pev->angles.y = 360;
	InitTrigger();

	if (pev->speed == 0)
		pev->speed = 100;

	if ( FBitSet (pev->spawnflags, SF_TRIGGER_PUSH_START_OFF) )// if flagged to Start Turned Off, make trigger nonsolid.
		pev->solid = SOLID_NOT;

	SetUse( &CTriggerPush::ToggleUse );

	UTIL_SetOrigin( pev, pev->origin );		// Link into the list
}

// SBO 2.1: Spawn turns "angles" into the push direction (movedir) and clears them (a trigger's brush must not turn),
// so the angles read back are those of the direction (Up and Down: -90 / 90 pitch, as MakeVectors reads them)
CKeyValue CTriggerPush :: GetKeyValue( const char *keyName )
{
	if ( !stricmp( keyName, "angles" ) )
	{
		CKeyValue keyvalue = g_emptyKeyValue;
		Vector vecAngles = UTIL_VecToAngles( pev->movedir );
		vecAngles.x = -vecAngles.x;	// VecToAngles' pitch is up-positive, the entity angles' down-positive
		keyvalue.keyType = KEY_TYPE_VECTOR;
		keyvalue.keyName = "angles";
		vecAngles.CopyToArray( keyvalue.vVal );
		return keyvalue;
	}
	return CBaseTrigger::GetKeyValue( keyName );
}

// SBO 2.1: new angles give a new push direction, as in Spawn (angles 0 0 0 push along yaw 360); speed 0 is 100
void CTriggerPush :: RuntimeKeyValueChanged( const char *keyName )
{
	if ( !stricmp( keyName, "angles" ) )
	{
		if ( pev->angles == g_vecZero )
			pev->angles.y = 360;
		SetMovedir( pev );
	}
	else if ( FStrEq( keyName, "speed" ) && pev->speed == 0 )
		pev->speed = 100;
}


void CTriggerPush :: Touch( CBaseEntity *pOther )
{
	entvars_t* pevToucher = pOther->pev;

	// UNDONE: Is there a better way than health to detect things that have physics? (clients/monsters)
	switch( pevToucher->movetype )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
	case MOVETYPE_NOCLIP:
	case MOVETYPE_FOLLOW:
		return;
	}

	// SBO 2.0, from Featureful (Half-Life 1 SDK license): flags 8 "No clients", 16 "No monsters"; no map sets them
	if ( FBitSet( pev->spawnflags, SF_TRIGGER_PUSH_NO_CLIENTS ) && FBitSet( pevToucher->flags, FL_CLIENT ) )
		return;
	if ( FBitSet( pev->spawnflags, SF_TRIGGER_PUSH_NO_MONSTERS ) && FBitSet( pevToucher->flags, FL_MONSTER ) )
		return;

	// SBO 2.0: "master": while it is locked for the toucher, no push (as trigger_hurt / trigger_jump /
	// trigger_monsterjump). 1.38 (as Half-Life) offered Master and never checked it; no 1.38 map sets one.
	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	// SBO 2.0 (Featureful): the push, with the Locus keys; without them pev->speed * pev->movedir as in Half-Life
	Vector vecPushDir = pev->movedir;
	if ( !FStringNull( m_iszPushVel ) )
	{
		if ( !TryCalcLocus_Velocity( this, pOther, STRING( m_iszPushVel ), vecPushDir ) )
			return;
	}
	if ( !FStringNull( m_iszPushSpeed ) )
	{
		float factor;
		if ( TryCalcLocus_Ratio( pOther, STRING( m_iszPushSpeed ), factor ) )
			vecPushDir = vecPushDir * factor;
	}

	if ( pevToucher->solid != SOLID_NOT && pevToucher->solid != SOLID_BSP )
	{
		// Instant trigger, just transfer velocity and remove
		if (FBitSet(pev->spawnflags, SF_TRIG_PUSH_ONCE))
		{
			pevToucher->velocity = pevToucher->velocity + (pev->speed * vecPushDir);
			if ( pevToucher->velocity.z > 0 )
				pevToucher->flags &= ~FL_ONGROUND;
			UTIL_Remove( this );
		}
		else
		{	// Push field, transfer to base velocity
			Vector vecPush = (pev->speed * vecPushDir);
			if ( pevToucher->flags & FL_BASEVELOCITY )
				vecPush = vecPush +  pevToucher->basevelocity;

			pevToucher->basevelocity = vecPush;

			pevToucher->flags |= FL_BASEVELOCITY;
//			ALERT( at_console, "Vel %f, base %f\n", pevToucher->velocity.z, pevToucher->basevelocity.z );
		}
	}
}


//======================================
// teleport trigger
//
//

// trigger_teleport spawnflags (1 monsters, 2 no clients, 4 pushables: SF_TRIGGER_*); the numbers are
// halflife-featureful's (128, 256, 512) so its maps work, 1024 is SBO's
#define SF_TELEPORT_RELATIVE		128	// stock teleport: keep the offset from the trigger's origin (origin brush)
#define SF_TELEPORT_KEEPANGLES		256	// stock teleport: do not turn to the destination's angles
#define SF_TELEPORT_KEEPVELOCITY	512	// stock teleport: do not stop
#define SF_TELEPORT_OBJECTS			1024	// landmark teleport: also grenades, rockets, items, gibs
// SBO 2.0, from Featureful (Half-Life 1 SDK license); no map sets these bits
#define SF_TELEPORT_RANDOM_DESTINATION				64	// trigger_teleport: one of the entities named Target, at random
#define SF_TELEPORT_DESTINATION_TRIGGER_ON_ARRIVAL	32	// info_teleport_destination: fires its Target at each arrival

// SBO 2.0 (Featureful CTriggerTeleport::GetTeleportTarget): the destination; with "Random destination" each entity
// named Target is as likely (reservoir pick over all of them)
static edict_t *Teleport_FindTarget( entvars_t *pev )
{
	// Half-Life Unified SDK: no target, no teleport (an empty name matched any entity whose name is an empty string,
	// such as one renamed to "" at run time: a seemingly random spot)
	if ( FStringNull( pev->target ) || !STRING( pev->target )[0] )
		return NULL;

	if ( !FBitSet( pev->spawnflags, SF_TELEPORT_RANDOM_DESTINATION ) )
		return FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) );

	int total = 0;
	CBaseEntity *pPick = NULL;
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->target ) ) ) != NULL )
	{
		total++;
		if ( RANDOM_LONG( 0, total - 1 ) < 1 )
			pPick = pEntity;
	}
	return pPick ? pPick->edict() : NULL;
}

// SBO 2.0 (Featureful): an info_teleport_destination with "Trigger on arrival" fires its Target (activator: the
// entity that arrived)
static void Teleport_Arrived( CBaseEntity *pTeleport, edict_t *pentTarget, CBaseEntity *pOther )
{
	if ( FBitSet( pentTarget->v.spawnflags, SF_TELEPORT_DESTINATION_TRIGGER_ON_ARRIVAL )
		&& !FStringNull( pentTarget->v.target ) && FClassnameIs( pentTarget, "info_teleport_destination" ) )
		FireTargets( STRING( pentTarget->v.target ), pOther, pTeleport, USE_TOGGLE, 0 );
}

void CBaseTrigger :: TeleportTouch( CBaseEntity *pOther )
{
	entvars_t* pevToucher = pOther->pev;
	edict_t	*pentTarget = NULL;

	// Only teleport monsters or clients
	if ( !FBitSet( pevToucher->flags, FL_CLIENT|FL_MONSTER ) )
		return;
    
	if (!UTIL_IsMasterTriggered(m_sMaster, pOther))
		return;
 	
	if ( !( pev->spawnflags & SF_TRIGGER_ALLOWMONSTERS ) )
	{// no monsters allowed!
		if ( FBitSet( pevToucher->flags, FL_MONSTER ) )
		{
			return;
		}
	}

	if ( ( pev->spawnflags & SF_TRIGGER_NOCLIENTS ) )
	{// no clients allowed
		if ( pOther->IsPlayer() )
		{
			return;
		}
	}
	
	pentTarget = Teleport_FindTarget( pev );	// SBO 2.0 (Featureful): Random destination
	if (FNullEnt(pentTarget))
	   return;	
	
	Vector tmp = VARS( pentTarget )->origin;

	// halflife-featureful (Sven Co-op flag): Relative Teleport, the toucher keeps its offset from the trigger's
	// origin (the trigger needs an origin brush). Only with the flag, so existing maps are unchanged.
	if ( FBitSet( pev->spawnflags, SF_TELEPORT_RELATIVE ) )
	{
		Vector vecSubject = pOther->pev->origin;
		if ( pOther->IsPlayer() )
			vecSubject.z += pOther->pev->mins.z;
		tmp = tmp + ( vecSubject - pev->origin );
	}

	if ( pOther->IsPlayer() )
	{
		tmp.z -= pOther->pev->mins.z;// make origin adjustments in case the teleportee is a player. (origin in center, not at feet)
	}

	tmp.z++;

	pevToucher->flags &= ~FL_ONGROUND;
	
	UTIL_SetOrigin( pevToucher, tmp );

	// halflife-featureful (Sven Co-op flags): Keep angles / Keep velocity
	if ( !FBitSet( pev->spawnflags, SF_TELEPORT_KEEPANGLES ) )
	{
		pevToucher->angles = pentTarget->v.angles;

		if ( pOther->IsPlayer() )
		{
			pevToucher->v_angle = pentTarget->v.angles;
		}

		pevToucher->fixangle = TRUE;
	}

	if ( !FBitSet( pev->spawnflags, SF_TELEPORT_KEEPVELOCITY ) )
	{
		pevToucher->velocity = pevToucher->basevelocity = g_vecZero;
	}

	Teleport_Arrived( this, pentTarget, pOther );
}


//======================================
// SBO 2.0 seamless teleport (Spirit of Half-Life 1.x by Laurie Cheers, as in halflife-planckepoch, Spirit 1.8
// and halflife-featureful): with "message" naming a landmark, the toucher keeps its place relative to the
// landmark, arriving at the same place relative to the target, turned by the yaw difference between the two
// (target yaw - landmark yaw): position, velocity, facing and the player's view, so two identical rooms or
// corridors join without a visible jump. Without "message" trigger_teleport is the stock teleport.
//
// Multiplayer: the view is turned with the engine's relative turn (fixangle 2: svc_addangle by avelocity yaw),
// which keeps the mouse movement the client made while the turn was on its way. The moves the client sent
// before it got the turn still carry the old yaw: pev->vuser1 ( turn, yaw before, 0 ) goes to the client in
// the clientdata, and the shared movement code (PM_TeleportStaleCmd) turns those moves on both sides, so the
// server and the client's prediction agree. Remote clients get EF_NOINTERP for a moment (no slide across).
//
#define TELEPORT_NOINTERP_TIME	0.1		// >= one update at the lowest cl_updaterate (10)

class CTriggerTeleport : public CBaseTrigger
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT StockTouch( CBaseEntity *pOther );
	void EXPORT InactiveUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT SeamlessTouch( CBaseEntity *pOther );

	// SBO 2.0, from Featureful: "Start Inactive" (teleport_start_inactive 1): it teleports nothing until it is
	// triggered (On / Toggle turns it on, Off off again); without the key a trigger_teleport is always on
	BOOL m_fInactive;
	void EXPORT NoInterpThink( void );

private:
	BOOL CanTeleport( CBaseEntity *pOther );
	void HoldNoInterp( CBaseEntity *pOther );

	struct noInterp_t
	{
		EHANDLE	hEntity;
		float	flUntil;
	};
	enum { NOINTERP_SLOTS = 16 };
	noInterp_t m_NoInterp[NOINTERP_SLOTS];
};
LINK_ENTITY_TO_CLASS( trigger_teleport, CTriggerTeleport );

void CTriggerTeleport :: Spawn( void )
{
	InitTrigger();

	if ( FStringNull( pev->message ) )
		SetTouch( &CTriggerTeleport::StockTouch );
	else
		SetTouch( &CTriggerTeleport::SeamlessTouch );

	if ( m_fInactive )
	{
		SetUse( &CTriggerTeleport::InactiveUse );
		if ( FStringNull( pev->targetname ) )
			ALERT( at_console, "trigger_teleport: Start Inactive without a name: it never teleports\n" );
	}
}

void CTriggerTeleport :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "teleport_start_inactive" ) )
	{
		m_fInactive = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}

void CTriggerTeleport :: InactiveUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ShouldToggle( useType, !m_fInactive ) )
		m_fInactive = !m_fInactive;
}

void CTriggerTeleport :: StockTouch( CBaseEntity *pOther )
{
	if ( !m_fInactive )
		TeleportTouch( pOther );
}

BOOL CTriggerTeleport :: CanTeleport( CBaseEntity *pOther )
{
	entvars_t *pevToucher = pOther->pev;

	if ( m_fInactive )
		return FALSE;

	if ( pOther->IsPlayer() )
	{
		if ( FBitSet( pev->spawnflags, SF_TRIGGER_NOCLIENTS ) )
			return FALSE;
		CBasePlayer *pPlayer = (CBasePlayer *)pOther;
		if ( pPlayer->IsObserver() || !pPlayer->IsAlive() )
			return FALSE;
	}
	else if ( FBitSet( pevToucher->flags, FL_MONSTER ) )
	{
		if ( !FBitSet( pev->spawnflags, SF_TRIGGER_ALLOWMONSTERS ) )
			return FALSE;
	}
	else
	{
		// thrown and flying things: grenades, rockets, dropped items, gibs
		if ( !FBitSet( pev->spawnflags, SF_TELEPORT_OBJECTS ) )
			return FALSE;
		if ( pevToucher->solid == SOLID_BSP || pevToucher->solid == SOLID_TRIGGER )
			return FALSE;
		switch ( pevToucher->movetype )
		{
		case MOVETYPE_TOSS:
		case MOVETYPE_BOUNCE:
		case MOVETYPE_FLY:
		case MOVETYPE_FLYMISSILE:
		case MOVETYPE_BOUNCEMISSILE:
			break;
		default:
			return FALSE;
		}
	}

	return UTIL_IsMasterTriggered( m_sMaster, pOther );
}

void CTriggerTeleport :: SeamlessTouch( CBaseEntity *pOther )
{
	if ( !CanTeleport( pOther ) )
		return;

	edict_t *pentTarget = Teleport_FindTarget( pev );	// SBO 2.0 (Featureful): Random destination
	CBaseEntity *pTarget = FNullEnt( pentTarget ) ? NULL : CBaseEntity::Instance( pentTarget );
	CBaseEntity *pLandmark = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );
	if ( !pTarget || !pLandmark )
	{
		ALERT( at_console, "trigger_teleport: target \"%s\" or landmark \"%s\" missing\n", STRING( pev->target ), STRING( pev->message ) );
		SetTouch( NULL );
		return;
	}

	entvars_t *pevToucher = pOther->pev;

	// The turn in whole degrees, -180 < turn <= 180: the client and the server use the same number
	// (the clientdata carries whole degrees).
	float flTurn = floor( fmod( pTarget->pev->angles.y - pLandmark->pev->angles.y, 360.0f ) + 0.5f );
	if ( flTurn > 180 )
		flTurn -= 360;
	else if ( flTurn <= -180 )
		flTurn += 360;

	float flRad = flTurn * ( M_PI / 180.0 );
	float c = cos( flRad ), s = sin( flRad );
	if ( fabs( c ) < 1e-6 ) c = 0;	// exact quarter turns
	if ( fabs( s ) < 1e-6 ) s = 0;

	Vector vecOffset = pevToucher->origin - pLandmark->pev->origin;
	Vector vecNew = pTarget->pev->origin + Vector( vecOffset.x * c - vecOffset.y * s, vecOffset.x * s + vecOffset.y * c, vecOffset.z );

	if ( flTurn != 0 )
	{
		Vector v = pevToucher->velocity;
		pevToucher->velocity = Vector( v.x * c - v.y * s, v.x * s + v.y * c, v.z );
		v = pevToucher->basevelocity;
		pevToucher->basevelocity = Vector( v.x * c - v.y * s, v.x * s + v.y * c, v.z );
		pevToucher->angles.y = UTIL_AngleMod( pevToucher->angles.y + flTurn );
	}

	if ( pOther->IsPlayer() )
	{
		// Seamless: no fade, no punch, the speed and the ground stay as they are (the floor is the same)
		UTIL_SetOrigin( pevToucher, vecNew );

		if ( flTurn != 0 )
			( (CBasePlayer *)pOther )->TeleportTurned( flTurn );
	}
	else
	{
		CBaseMonster *pMonster = pOther->MyMonsterPointer();
		if ( pMonster )
		{
			pMonster->pev->ideal_yaw = UTIL_AngleMod( pMonster->pev->ideal_yaw + flTurn );
			pMonster->RouteClear();	// the route was in the other place
		}

		pevToucher->flags &= ~FL_ONGROUND;
		UTIL_SetOrigin( pevToucher, vecNew );
	}

	HoldNoInterp( pOther );
	Teleport_Arrived( this, pTarget->edict(), pOther );
}

// EF_NOINTERP: the other clients do not interpolate the toucher from the old place to the new one. It has to
// stay on until every client got an update with it, then it is cleared here.
void CTriggerTeleport :: HoldNoInterp( CBaseEntity *pOther )
{
	if ( FBitSet( pOther->pev->effects, EF_NOINTERP ) )
	{
		// already held (by this or another teleport): extend it here if this trigger holds it
		for ( int i = 0; i < NOINTERP_SLOTS; i++ )
		{
			if ( m_NoInterp[i].hEntity == pOther )
				m_NoInterp[i].flUntil = gpGlobals->time + TELEPORT_NOINTERP_TIME;
		}
		return;
	}

	int iSlot = 0;
	for ( int i = 0; i < NOINTERP_SLOTS; i++ )
	{
		if ( m_NoInterp[i].hEntity == NULL )
		{
			iSlot = i;
			break;
		}
		if ( m_NoInterp[i].flUntil < m_NoInterp[iSlot].flUntil )
			iSlot = i;
	}

	CBaseEntity *pOld = m_NoInterp[iSlot].hEntity;
	if ( pOld )
		ClearBits( pOld->pev->effects, EF_NOINTERP );	// all slots in use: the oldest lets go early

	SetBits( pOther->pev->effects, EF_NOINTERP );
	m_NoInterp[iSlot].hEntity = pOther;
	m_NoInterp[iSlot].flUntil = gpGlobals->time + TELEPORT_NOINTERP_TIME;

	SetThink( &CTriggerTeleport::NoInterpThink );
	pev->nextthink = gpGlobals->time + TELEPORT_NOINTERP_TIME;
}

void CTriggerTeleport :: NoInterpThink( void )
{
	BOOL fHolding = FALSE;

	for ( int i = 0; i < NOINTERP_SLOTS; i++ )
	{
		CBaseEntity *pEntity = m_NoInterp[i].hEntity;
		if ( !pEntity )
			continue;
		if ( m_NoInterp[i].flUntil <= gpGlobals->time )
		{
			ClearBits( pEntity->pev->effects, EF_NOINTERP );
			m_NoInterp[i].hEntity = NULL;
		}
		else
		{
			fHolding = TRUE;
		}
	}

	if ( fHolding )
		pev->nextthink = gpGlobals->time + 0.05;
	else
		SetThink( NULL );
}


LINK_ENTITY_TO_CLASS( info_teleport_destination, CPointEntity );



class CTriggerSave : public CBaseTrigger
{
public:
	void Spawn( void );
	void EXPORT SaveTouch( CBaseEntity *pOther );
};
LINK_ENTITY_TO_CLASS( trigger_autosave, CTriggerSave );

void CTriggerSave::Spawn( void )
{
	if ( g_pGameRules->IsDeathmatch() )
	{
		REMOVE_ENTITY( ENT(pev) );
		return;
	}

	InitTrigger();
	SetTouch( &CTriggerSave::SaveTouch );
}

void CTriggerSave::SaveTouch( CBaseEntity *pOther )
{
	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	// Only save on clients
	if ( !pOther->IsPlayer() )
		return;
    
	SetTouch( NULL );
	UTIL_Remove( this );
	SERVER_COMMAND( "autosave\n" );
}

#define SF_ENDSECTION_USEONLY		0x0001

class CTriggerEndSection : public CBaseTrigger
{
public:
	void Spawn( void );
	void EXPORT EndSectionTouch( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT EndSectionUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};
LINK_ENTITY_TO_CLASS( trigger_endsection, CTriggerEndSection );


void CTriggerEndSection::EndSectionUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Only save on clients
	if ( pActivator && !pActivator->IsNetClient() )
		return;
    
	SetUse( NULL );

	if ( pev->message )
	{
		g_engfuncs.pfnEndSection(STRING(pev->message));
	}
	UTIL_Remove( this );
}

void CTriggerEndSection::Spawn( void )
{
	if ( g_pGameRules->IsDeathmatch() )
	{
		REMOVE_ENTITY( ENT(pev) );
		return;
	}

	InitTrigger();

	SetUse ( &CTriggerEndSection::EndSectionUse );
	// If it is a "use only" trigger, then don't set the touch function.
	if ( ! (pev->spawnflags & SF_ENDSECTION_USEONLY) )
		SetTouch( &CTriggerEndSection::EndSectionTouch );
}

void CTriggerEndSection::EndSectionTouch( CBaseEntity *pOther )
{
	// Only save on clients
	if ( !pOther->IsNetClient() )
		return;
    
	SetTouch( NULL );

	if (pev->message)
	{
		g_engfuncs.pfnEndSection(STRING(pev->message));
	}
	UTIL_Remove( this );
}

void CTriggerEndSection :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "section"))
	{
//		m_iszSectionName = ALLOC_STRING( pkvd->szValue );
		// Store this in message so we don't have to write save/restore for this ent
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseTrigger::KeyValue( pkvd );
}


//=========================================================
// trigger_gravity -- a player touching it gets its gravity (a multiplier of sv_gravity); players only (Half-Life)
//
// SBO 2.1: "master" (while it is locked for the toucher, nothing happens); a named trigger turns on and off when fired
// (On turns it on, Off turns it off, Toggle toggles), flag 2 "Start Off" (as trigger_hurt and trigger_push); flag 4
// "Reset on leave": a player in play inside is tracked (as trigger_inout), and his gravity goes back to 1 when he
// leaves the brush, leaves play (dies, turns spectator or Game Master, disconnects), or the trigger is turned off or
// removed while he is inside; unless something else changed it meanwhile (e.g. another trigger_gravity he walked
// into). Without the flag a player keeps the gravity until something else changes it, as in Half-Life
// (CBasePlayer::Spawn sets 1 on every spawn and new round). docs/FEATUREFUL_OPTIONS.md "Other entities".
// Not saved (multiplayer only, as trigger_inout): the list of players inside is rebuilt by the next touch.
//=========================================================

#define SF_TRIGGER_GRAVITY_START_OFF		2
#define SF_TRIGGER_GRAVITY_RESET_ON_LEAVE	4
#define GRAVITY_MAX_INSIDE				32	// players (the engine's MAX_CLIENTS)

class CTriggerGravity : public CBaseTrigger
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void UpdateOnRemove( void );
	void EXPORT GravityTouch( CBaseEntity *pOther );
	void EXPORT GravityThink( void );

private:
	void ResetPlayer( CBaseEntity *pPlayer );
	void ResetAll( void );

	EHANDLE m_hInside[ GRAVITY_MAX_INSIDE ];
	int m_iInside;
};
LINK_ENTITY_TO_CLASS( trigger_gravity, CTriggerGravity );

void CTriggerGravity::Spawn( void )
{
	InitTrigger();
	SetTouch( &CTriggerGravity::GravityTouch );
	SetThink( &CTriggerGravity::GravityThink );
	m_iInside = 0;
	pev->nextthink = -1;

	if ( FBitSet( pev->spawnflags, SF_TRIGGER_GRAVITY_START_OFF ) )
	{
		pev->solid = SOLID_NOT;
		UTIL_SetOrigin( pev, pev->origin );
	}
}

void CTriggerGravity::GravityTouch( CBaseEntity *pOther )
{
	// Only save on clients
	if ( !pOther->IsPlayer() )
		return;

	if ( !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	if ( !FBitSet( pev->spawnflags, SF_TRIGGER_GRAVITY_RESET_ON_LEAVE ) )
	{
		pOther->pev->gravity = pev->gravity;
		return;
	}

	// "Reset on leave": only players in play, each tracked until he leaves
	if ( !Logic_IsPlayerInPlay( pOther ) )
		return;

	pOther->pev->gravity = pev->gravity;

	for ( int i = 0; i < m_iInside; i++ )
	{
		if ( (CBaseEntity *)m_hInside[i] == pOther )
			return;	// already inside
	}

	if ( m_iInside >= GRAVITY_MAX_INSIDE )
		return;

	m_hInside[ m_iInside++ ] = pOther;

	if ( pev->nextthink <= 0 )
		pev->nextthink = gpGlobals->time + 0.1;
}

// every 0.1 s (as trigger_inout): who has left
void CTriggerGravity::GravityThink( void )
{
	int kept = 0;

	for ( int i = 0; i < m_iInside; i++ )
	{
		CBaseEntity *pEnt = m_hInside[i];

		if ( !pEnt )
			continue;	// gone

		if ( !Intersects( pEnt ) || !Logic_IsPlayerInPlay( pEnt ) )
			ResetPlayer( pEnt );
		else
			m_hInside[ kept++ ] = pEnt;
	}

	for ( int i = kept; i < m_iInside; i++ )
		m_hInside[i] = NULL;
	m_iInside = kept;

	pev->nextthink = m_iInside ? gpGlobals->time + 0.1 : -1;
}

void CTriggerGravity::ResetPlayer( CBaseEntity *pPlayer )
{
	// only the gravity this trigger gave: another trigger_gravity he is in now keeps its own
	if ( pPlayer->pev->gravity == pev->gravity )
		pPlayer->pev->gravity = 1.0;
}

void CTriggerGravity::ResetAll( void )
{
	for ( int i = 0; i < m_iInside; i++ )
	{
		CBaseEntity *pEnt = m_hInside[i];

		if ( pEnt )
			ResetPlayer( pEnt );
		m_hInside[i] = NULL;
	}
	m_iInside = 0;
	pev->nextthink = -1;
}

// SBO 2.1: On turns it on, Off turns it off, Toggle toggles
void CTriggerGravity::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	BOOL bOn = pev->solid != SOLID_NOT;

	if ( !ShouldToggle( useType, bOn ) )
		return;

	if ( bOn )
	{
		pev->solid = SOLID_NOT;
		ResetAll();	// "Reset on leave": the players inside have left
	}
	else
	{
		pev->solid = SOLID_TRIGGER;
		gpGlobals->force_retouch++;
	}
	UTIL_SetOrigin( pev, pev->origin );
}

// SBO 2.1: removed (killtarget) with players inside: they have left
void CTriggerGravity::UpdateOnRemove( void )
{
	ResetAll();
	CBaseTrigger::UpdateOnRemove();
}







// this is a really bad idea.
class CTriggerChangeTarget : public CBaseDelay
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ChangeThink( void );	// SBO 2.0: "Delay before trigger"
	void ChangeTarget( CBaseEntity *pActivator );

	int ObjectCaps( void ) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

private:
	int		m_iszNewTarget;
};
LINK_ENTITY_TO_CLASS( trigger_changetarget, CTriggerChangeTarget );

TYPEDESCRIPTION	CTriggerChangeTarget::m_SaveData[] = 
{
	DEFINE_FIELD( CTriggerChangeTarget, m_iszNewTarget, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE(CTriggerChangeTarget,CBaseDelay);

void CTriggerChangeTarget::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iszNewTarget"))
	{
		m_iszNewTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerChangeTarget::Spawn( void )
{
}


// SBO 2.0: "Delay before trigger" (the FGD's Targetx base): the new target is set that many seconds after the trigger,
// as a delayed SUB_UseTargets fires (Half-Life set it at once and never read the delay). The change is the same each
// time, so a trigger while one is pending changes nothing more. "KillTarget" (Targetx too) removes its entities at
// the same moment, before the change, as SUB_UseTargets does (Half-Life never read it).
void CTriggerChangeTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_flDelay > 0 )
	{
		if ( m_pfnThink != NULL )
			return;
		m_hActivator = pActivator;
		SetThink( &CTriggerChangeTarget::ChangeThink );
		pev->nextthink = gpGlobals->time + m_flDelay;
		return;
	}

	ChangeTarget( pActivator );
}

void CTriggerChangeTarget::ChangeThink( void )
{
	SetThink( NULL );
	ChangeTarget( m_hActivator );
}

void CTriggerChangeTarget::ChangeTarget( CBaseEntity *pActivator )
{
	SUB_KillTargets( pActivator );	// as SUB_UseTargets: !activator / !caller, never a player or itself

	CBaseEntity *pTarget = UTIL_FindEntityByString( NULL, "targetname", STRING( pev->target ) );

	if (pTarget)
	{
		pTarget->pev->target = m_iszNewTarget;
		CBaseMonster *pMonster = pTarget->MyMonsterPointer( );
		if (pMonster)
		{
			pMonster->m_pGoalEnt = NULL;
		}
	}
}




#define SF_CAMERA_PLAYER_POSITION	1
#define SF_CAMERA_PLAYER_TARGET		2
#define SF_CAMERA_PLAYER_TAKECONTROL 4
// SBO 2.0 (2026-11-07): "All Players", Sven Co-op's bit (SevenKewp dlls/triggers/CTriggerCamera.h
// SF_CAMERA_ALL_PLAYERS; Featureful keeps 8 reserved for it): every player in play gets the camera's view, not only
// the activator. No map sets it (scan 2026-11-07)
#define SF_CAMERA_ALL_PLAYERS		8
#define CAMERA_MAX_VIEWERS			32		// one per client slot (entity index - 1), MAX_CLIENTS
// SBO 2.0, from Featureful (halflife-featureful dlls/triggers.cpp CTriggerCamera, commits 633c971, 31a3028, 04cbb94,
// 2026-10-13; Half-Life 1 SDK license; Sven Co-op's "Instant Move" bit): no map sets these bits or "turnspeed"
#define SF_CAMERA_INSTANT_MOVE			64		// from path to path at the path's speed, no smoothing
#define SF_CAMERA_INSTANT_FOLLOW_TURN	(1<<16)	// always looks straight at its target (no turning speed)
// SBO 2.0, from Featureful (commits eb15e766, bd91053e, 1aaff55c, d6487b72, 49361a84; Half-Life 1 SDK license): no
// map sets these bits or the keys "m_iszTurnedOffTarget" / "stop_by_player_input". 256 "Player Invulnerable" and
// 1<<25 (the "stop_by_player_input" key) are SF_CAMERA_* in player.h, read by the player code
#define SF_CAMERA_IGNORE_HOLD_TIME				512		// runs until it is turned off, the hold time is ignored
#define SF_CAMERA_PLAYER_ALIVE_ONLY				1024	// only for a player in play; turns off when he leaves play
#define SF_CAMERA_DONT_FIRE_LOOK_TARGET			(1<<12)	// its look target is not fired at the end
#define SF_CAMERA_DONT_SLOW_DOWN_IF_NOT_FREEZE	(1<<13)	// without Freeze Player its speed is not damped every frame
#define SF_CAMERA_DONT_SKIP_FIRST_PATH_CORNER	(1<<14)	// travels to the first path corner (Half-Life starts at the second)
#define SF_CAMERA_INSTANT_INITIAL_TURN			(1<<15)	// faces its target at once when it starts
// SBO 2.0: 1<<17 "Hide HUD and crosshair" is SF_CAMERA_HIDE_HUD in player.h (the player code hides the HUD while the
// camera has the view)

class CTriggerCamera : public CBaseDelay
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT FollowTarget( void );
	void Move(void);
	void UpdateOnRemove( void );
	void ReleasePlayer( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	static	TYPEDESCRIPTION m_SaveData[];

	EHANDLE m_hPlayer;
	EHANDLE m_hTarget;
	CBaseEntity *m_pentPath;
	int	  m_sPath;
	float m_flWait;
	float m_flReturnTime;
	float m_flStopTime;
	float m_moveDistance;
	float m_targetSpeed;
	float m_initialSpeed;
	float m_acceleration;
	float m_deceleration;
	int	  m_state;

	// SBO 2.0, from Featureful: "Camera Rotation Speed" (turnspeed): how fast it turns to its target; 0 or less: 40,
	// Half-Life's
	float m_turnspeed;
	float GetTurnSpeed( void ) { return m_turnspeed > 0.0f ? m_turnspeed : 40.0f; }

	// SBO 2.0, from Featureful: "Target when turned off", fired (activator: the player) when the camera ends
	string_t m_iszTurnedOffTarget;
	int m_stopByPlayerInput;	// "Stop by player input": 1 sets SF_CAMERA_STOP_BY_PLAYER_INPUT_USE at spawn
	// SBO 2.0: the capabilities this camera took from its player instead of FL_FROZEN (Freeze Player with Stop by
	// player input): the engine drops a frozen client's buttons (SV_ParseMove), so +use never reached the game
	int m_iCapsTaken;
	void TakeControl( CBasePlayer *pPlayer, int &iCapsTaken );
	void GiveControl( CBasePlayer *pPlayer, int &iCapsTaken );

	// SBO 2.0 "All Players" (8): the players who got the view, by client slot, each with the capabilities the camera
	// took from him (Freeze Player with Stop by player input) and his connection's user id: a client who connects
	// to a slot that was left keeps the same edict (and EHANDLE), and is not the viewer. The ids are not saved:
	// 0 (after a restore) takes the slot's player
	EHANDLE m_hViewers[ CAMERA_MAX_VIEWERS ];
	int m_iViewerCaps[ CAMERA_MAX_VIEWERS ];
	int m_iViewerIds[ CAMERA_MAX_VIEWERS ];
	CBasePlayer *Viewer( int i );
	void TakeViewers( void );
	void ReleaseViewer( int i );
	void ReleaseViewers( void );
	BOOL ViewersInPlay( void );
};
LINK_ENTITY_TO_CLASS( trigger_camera, CTriggerCamera );

// Global Savedata for changelevel friction modifier
TYPEDESCRIPTION	CTriggerCamera::m_SaveData[] = 
{
	DEFINE_FIELD( CTriggerCamera, m_hPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( CTriggerCamera, m_iCapsTaken, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerCamera, m_hTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( CTriggerCamera, m_pentPath, FIELD_CLASSPTR ),
	DEFINE_FIELD( CTriggerCamera, m_sPath, FIELD_STRING ),
	DEFINE_FIELD( CTriggerCamera, m_flWait, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_flReturnTime, FIELD_TIME ),
	DEFINE_FIELD( CTriggerCamera, m_flStopTime, FIELD_TIME ),
	DEFINE_FIELD( CTriggerCamera, m_moveDistance, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_targetSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_initialSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_acceleration, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_deceleration, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_state, FIELD_INTEGER ),
	DEFINE_FIELD( CTriggerCamera, m_turnspeed, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerCamera, m_iszTurnedOffTarget, FIELD_STRING ),
	DEFINE_ARRAY( CTriggerCamera, m_hViewers, FIELD_EHANDLE, CAMERA_MAX_VIEWERS ),
	DEFINE_ARRAY( CTriggerCamera, m_iViewerCaps, FIELD_INTEGER, CAMERA_MAX_VIEWERS ),
};

IMPLEMENT_SAVERESTORE(CTriggerCamera,CBaseDelay);

void CTriggerCamera::Spawn( void )
{
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;							// Remove model & collisions
	pev->renderamt = 0;								// The engine won't draw this model if this is set to 0 and blending is on
	pev->rendermode = kRenderTransTexture;

	m_initialSpeed = pev->speed;
	if ( m_acceleration == 0 )
		m_acceleration = 500;
	if ( m_deceleration == 0 )
		m_deceleration = 500;

	// SBO 2.0 (Featureful): the key becomes a spawnflag, which the player code reads (CBasePlayer::PlayerUse)
	if ( m_stopByPlayerInput != 0 )
		pev->spawnflags |= SF_CAMERA_STOP_BY_PLAYER_INPUT_USE;
}


void CTriggerCamera :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "moveto"))
	{
		m_sPath = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "acceleration"))
	{
		m_acceleration = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "deceleration"))
	{
		m_deceleration = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "turnspeed"))	// SBO 2.0 (Featureful)
	{
		m_turnspeed = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "stop_by_player_input"))	// SBO 2.0 (Featureful)
	{
		m_stopByPlayerInput = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTurnedOffTarget"))	// SBO 2.0 (Featureful)
	{
		m_iszTurnedOffTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}



void CTriggerCamera::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SBO 2.0 "All Players" with "Stop by player input": a viewer's use key (CBasePlayer::PlayerUse turns the camera
	// off with the player as activator and caller) gives only him his own view back; the camera runs on for the
	// others and ends as it would
	if ( FBitSet( pev->spawnflags, SF_CAMERA_ALL_PLAYERS ) && FBitSet( pev->spawnflags, SF_CAMERA_STOP_BY_PLAYER_INPUT_USE )
		&& useType == USE_OFF && m_state && pActivator && pActivator == pCaller && pActivator->IsPlayer() )
	{
		const int i = pActivator->entindex() - 1;
		if ( i >= 0 && i < CAMERA_MAX_VIEWERS && Viewer( i ) == pActivator )
			ReleaseViewer( i );
		return;
	}

	if ( !ShouldToggle( useType, m_state ) )
		return;

	// Toggle state
	m_state = !m_state;
	if (m_state == 0)
	{
		m_flReturnTime = gpGlobals->time;
		return;
	}

	// SBO 2.0: without "All Players" the view goes to the player who fired the camera, if they are in play. A camera
	// fired by anything else (a monster, a timer, another entity, nothing) or by a player out of play (dead,
	// spectating, waiting) has no viewer: it moves, holds, ends and fires its targets, and touches no player.
	// "All Players": every player in play gets the view (TakeViewers); Start At Player and Follow Player use the
	// activator if they are a player in play, else the first player in play, else the player in client slot 1.
	const BOOL fAllPlayers = FBitSet( pev->spawnflags, SF_CAMERA_ALL_PLAYERS );
	CBasePlayer *pPlayer = NULL;	// without "All Players": the viewer, NULL for none
	if ( fAllPlayers )
	{
		if ( !Logic_IsPlayerInPlay( pActivator ) )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBaseEntity *pInPlay = UTIL_PlayerByIndex( i );
				if ( Logic_IsPlayerInPlay( pInPlay ) )
				{
					pActivator = pInPlay;
					break;
				}
			}
		}

		if ( !pActivator || !pActivator->IsPlayer() )
		{
			pActivator = CBaseEntity::Instance(g_engfuncs.pfnPEntityOfEntIndex( 1 ));

			// halflife-updated: no player in slot 1 (a free client slot has no entity)
			if ( !pActivator )
				return;
		}
	}
	else
	{
		pPlayer = Logic_IsPlayerInPlay( pActivator ) ? (CBasePlayer *)pActivator : NULL;
		pActivator = pPlayer;
	}

	// SBO 2.0 (Featureful): "Alive player only": nothing for a player out of play (dead, observing, waiting) or for a
	// camera with no viewer. Featureful leaves the camera "on" then; SBO turns it back off, so the next trigger starts it
	if ( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_ALIVE_ONLY ) && !Logic_IsPlayerInPlay( pActivator ) )
	{
		m_state = 0;
		return;
	}

	// a viewer the camera still has (turned off and on again before it let go) gets their own view back first
	CBasePlayer *pOldViewer = (CBasePlayer *)((CBaseEntity *)m_hPlayer);
	if ( !fAllPlayers && pOldViewer && (CBaseEntity *)pOldViewer->m_hViewEntity == this )
		ReleasePlayer();

	m_hPlayer = pActivator;

	m_flReturnTime = gpGlobals->time + m_flWait;
	pev->speed = m_initialSpeed;
	m_targetSpeed = m_initialSpeed;

	// Follow Player with no viewer looks at the camera's target
	if ( FBitSet (pev->spawnflags, SF_CAMERA_PLAYER_TARGET ) && pActivator )
	{
		m_hTarget = m_hPlayer;
	}
	else
	{
		m_hTarget = GetNextTarget();
	}

	// Nothing to look at! (a camera with no viewer turns back off, so the next trigger starts it)
	if ( m_hTarget == NULL )
	{
		if ( !pActivator )
			m_state = 0;
		return;
	}

	if ( pPlayer && FBitSet (pev->spawnflags, SF_CAMERA_PLAYER_TAKECONTROL ) )
	{
		TakeControl( pPlayer, m_iCapsTaken );
	}

	if ( m_sPath )
	{
		m_pentPath = Instance( FIND_ENTITY_BY_TARGETNAME ( NULL, STRING(m_sPath)) );
	}
	else
	{
		m_pentPath = NULL;
	}

	// SBO 2.1 (docs/MOVEWITH.md): a camera with a parent rides with it, so its position comes from the parent only
	// and its own path is not used; its aim, turning and target tracking stay its own
	const BOOL fRides = MoveWith_IsChild( this );
	if ( m_pentPath && fRides )
	{
		ALERT( at_console, "trigger_camera %s has a parent, so it ignores its path and rides with the parent\n",
			STRING( pev->targetname ) );
		m_pentPath = NULL;
	}

	m_flStopTime = gpGlobals->time;
	if ( m_pentPath )
	{
		if ( m_pentPath->pev->speed != 0 )
			m_targetSpeed = m_pentPath->pev->speed;
		
		m_flStopTime += m_pentPath->GetDelay();
	}

	// copy over player information (Start At Player with no viewer: the camera starts where it is)
	if (FBitSet (pev->spawnflags, SF_CAMERA_PLAYER_POSITION ) && pActivator )
	{
		// (a camera with a parent takes the player's view angles only: its place is on the parent)
		if ( !fRides )
			UTIL_SetOrigin( pev, pActivator->pev->origin + pActivator->pev->view_ofs );
		pev->angles.x = -pActivator->pev->angles.x;
		pev->angles.y = pActivator->pev->angles.y;
		pev->angles.z = 0;
		pev->velocity = fRides ? g_vecZero : pActivator->pev->velocity;
	}
	else
	{
		pev->velocity = Vector( 0, 0, 0 );

		// SBO 2.0 (Featureful): "Instant Initial Turn"
		if ( FBitSet( pev->spawnflags, SF_CAMERA_INSTANT_INITIAL_TURN ) )
		{
			Vector vecAnglesGoal = UTIL_VecToAngles( m_hTarget->pev->origin - pev->origin );
			vecAnglesGoal.x = -vecAnglesGoal.x;
			pev->angles = vecAnglesGoal;
		}
	}

	if ( pPlayer )
	{
		SET_VIEW( pPlayer->edict(), edict() );
		pPlayer->m_hViewEntity = this;	// halflife-updated; CBasePlayer::ResetViewEntity (death, observer)
	}
	else if ( fAllPlayers )
	{
		TakeViewers();
	}

	if ( pActivator )
		SET_MODEL(ENT(pev), STRING(pActivator->pev->model) );

	// follow the player down
	SetThink( &CTriggerCamera::FollowTarget );
	pev->nextthink = gpGlobals->time;

	// SBO 2.0 (Featureful): "Don't skip the first path corner": head for it; Half-Life passes it at once
	if ( m_pentPath && FBitSet( pev->spawnflags, SF_CAMERA_DONT_SKIP_FIRST_PATH_CORNER ) )
	{
		Vector delta = m_pentPath->pev->origin - pev->origin;
		m_moveDistance = delta.Length();
		pev->movedir = delta.Normalize();
	}
	else
	{
		m_moveDistance = 0;
	}
	Move();
}


void CTriggerCamera::FollowTarget( )
{
	// SBO 2.0 (Featureful): "Ignore Hold Time" ends only when turned off; "Alive player only" also when the
	// player leaves play. pPlayer is NULL for a camera with no viewer, which runs all the same
	CBasePlayer *pPlayer = (CBasePlayer *)((CBaseEntity *)m_hPlayer);
	const BOOL fGoOff = FBitSet( pev->spawnflags, SF_CAMERA_IGNORE_HOLD_TIME ) ? !m_state : m_flReturnTime < gpGlobals->time;
	// ("All Players": when none of its viewers is in play any more)
	const BOOL fTurnOff = FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_ALIVE_ONLY )
		&& ( FBitSet( pev->spawnflags, SF_CAMERA_ALL_PLAYERS ) ? !ViewersInPlay() : !Logic_IsPlayerInPlay( pPlayer ) );

	if (m_hTarget == NULL || fGoOff || fTurnOff)
	{
		ReleasePlayer();

		if ( !FBitSet( pev->spawnflags, SF_CAMERA_DONT_FIRE_LOOK_TARGET ) )
			SUB_UseTargets( this, USE_TOGGLE, 0 );
		if ( !FStringNull( m_iszTurnedOffTarget ) )
		{
			// the player, while still connected; the camera itself with no viewer
			CBaseEntity *pActivator = ( !pPlayer || pPlayer->has_disconnected ) ? (CBaseEntity *)this : (CBaseEntity *)pPlayer;
			FireTargets( STRING( m_iszTurnedOffTarget ), pActivator, this, USE_TOGGLE, 0 );
		}
		pev->avelocity = Vector( 0, 0, 0 );
		m_state = 0;
		return;
	}

	Vector vecGoal = UTIL_VecToAngles( m_hTarget->pev->origin - pev->origin );
	vecGoal.x = -vecGoal.x;

	if ( !FBitSet( pev->spawnflags, SF_CAMERA_INSTANT_FOLLOW_TURN ) )
	{
		if (pev->angles.y > 360)
			pev->angles.y -= 360;

		if (pev->angles.y < 0)
			pev->angles.y += 360;

		float dx = vecGoal.x - pev->angles.x;
		float dy = vecGoal.y - pev->angles.y;

		if (dx < -180) 
			dx += 360;
		if (dx > 180) 
			dx = dx - 360;
		
		if (dy < -180) 
			dy += 360;
		if (dy > 180) 
			dy = dy - 360;

		// halflife-updated (halflife #2924): framerate-independent turn speed (0.01 = the think interval);
		// SBO 2.0 (Featureful): "Camera Rotation Speed" (40 by default)
		const float turnSpeed = GetTurnSpeed();
		pev->avelocity.x = dx * turnSpeed * 0.01;
		pev->avelocity.y = dy * turnSpeed * 0.01;
	}
	else
	{
		// SBO 2.0 (Featureful): "Instant Follow Turn"
		pev->angles = vecGoal;
	}


	if (!(FBitSet (pev->spawnflags, SF_CAMERA_PLAYER_TAKECONTROL)) && !FBitSet( pev->spawnflags, SF_CAMERA_DONT_SLOW_DOWN_IF_NOT_FREEZE ))
	{
		pev->velocity = pev->velocity * 0.8;
		if (pev->velocity.Length( ) < 10.0)
			pev->velocity = g_vecZero;
	}

	pev->nextthink = gpGlobals->time;

	Move();
}

// The player gets his own view back (and his controls, if the camera took them)
// Freeze Player. With "Stop by player input" the player keeps his buttons: movement, jump, duck and attack are taken
// (player_capabilities' suppression, predicted by the client) instead of FL_FROZEN, which makes the engine zero a
// client's buttons (SV_ParseMove), so his +use could never end the camera. Without it: FL_FROZEN, as Half-Life.
// iCapsTaken: the player's record of the capabilities taken (m_iCapsTaken, or his m_iViewerCaps with "All Players")
void CTriggerCamera::TakeControl( CBasePlayer *pPlayer, int &iCapsTaken )
{
	if ( !FBitSet( pev->spawnflags, SF_CAMERA_STOP_BY_PLAYER_INPUT_USE ) )
	{
		pPlayer->EnableControl( FALSE );
		return;
	}
	const int iFreeze = PLAYER_SUPPRESS_MOVEMENT | PLAYER_SUPPRESS_JUMP | PLAYER_SUPPRESS_DUCK | PLAYER_SUPPRESS_ATTACK;
	iCapsTaken = iFreeze & ~pPlayer->m_iSuppressedCaps;	// only what the player still had
	pPlayer->SetSuppressedCaps( pPlayer->m_iSuppressedCaps | iCapsTaken );
}

void CTriggerCamera::GiveControl( CBasePlayer *pPlayer, int &iCapsTaken )
{
	if ( !FBitSet( pev->spawnflags, SF_CAMERA_STOP_BY_PLAYER_INPUT_USE ) )
	{
		pPlayer->EnableControl( TRUE );
		return;
	}
	pPlayer->SetSuppressedCaps( pPlayer->m_iSuppressedCaps & ~iCapsTaken );
	iCapsTaken = 0;
}

// "All Players": viewer i while he is the same client, still connected; NULL otherwise
CBasePlayer *CTriggerCamera::Viewer( int i )
{
	CBasePlayer *pPlayer = (CBasePlayer *)((CBaseEntity *)m_hViewers[ i ]);
	if ( !pPlayer || pPlayer->has_disconnected )
		return NULL;
	if ( m_iViewerIds[ i ] != 0 && GETPLAYERUSERID( pPlayer->edict() ) != m_iViewerIds[ i ] )
		return NULL;
	return pPlayer;
}

// "All Players": every player in play gets the camera's view, and loses his controls with Freeze Player. A player
// who joins, respawns or comes into play later is not added
void CTriggerCamera::TakeViewers( void )
{
	for ( int i = 0; i < CAMERA_MAX_VIEWERS; i++ )
	{
		m_hViewers[ i ] = NULL;
		m_iViewerCaps[ i ] = 0;
		m_iViewerIds[ i ] = 0;

		CBasePlayer *pPlayer = i < gpGlobals->maxClients ? (CBasePlayer *)UTIL_PlayerByIndex( i + 1 ) : NULL;
		if ( !Logic_IsPlayerInPlay( pPlayer ) )
			continue;

		m_hViewers[ i ] = pPlayer;
		m_iViewerIds[ i ] = GETPLAYERUSERID( pPlayer->edict() );
		if ( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_TAKECONTROL ) )
			TakeControl( pPlayer, m_iViewerCaps[ i ] );
		SET_VIEW( pPlayer->edict(), edict() );
		pPlayer->m_hViewEntity = this;
	}
}

// "All Players": viewer i gets his own view back (and his controls, if the camera took them), as ReleasePlayer does
// for the one player: a viewer who died or started observing got the view back then (ResetViewEntity); one who
// left, or a new client in his slot, is not touched. A viewer another camera has taken since keeps that view, and
// its Freeze Player
void CTriggerCamera::ReleaseViewer( int i )
{
	CBasePlayer *pPlayer = Viewer( i );
	if ( pPlayer )
	{
		CBaseEntity *pView = pPlayer->m_hViewEntity;
		const BOOL fOtherView = pView && pView != this;

		if ( pPlayer->IsAlive() )
		{
			if ( !fOtherView )
				SET_VIEW( pPlayer->edict(), pPlayer->edict() );
			if ( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_TAKECONTROL )
				&& ( !fOtherView || FBitSet( pev->spawnflags, SF_CAMERA_STOP_BY_PLAYER_INPUT_USE ) ) )
				GiveControl( pPlayer, m_iViewerCaps[ i ] );
		}
		if ( pView == this )
			pPlayer->m_hViewEntity = NULL;
	}
	m_hViewers[ i ] = NULL;
	m_iViewerCaps[ i ] = 0;
	m_iViewerIds[ i ] = 0;
}

void CTriggerCamera::ReleaseViewers( void )
{
	for ( int i = 0; i < CAMERA_MAX_VIEWERS; i++ )
	{
		if ( (CBaseEntity *)m_hViewers[ i ] )
			ReleaseViewer( i );
	}
}

// "All Players" with "Alive player only": one of its viewers is still in play
BOOL CTriggerCamera::ViewersInPlay( void )
{
	for ( int i = 0; i < CAMERA_MAX_VIEWERS; i++ )
	{
		if ( Logic_IsPlayerInPlay( Viewer( i ) ) )
			return TRUE;
	}
	return FALSE;
}

void CTriggerCamera::ReleasePlayer( void )
{
	// SBO 2.0: a player who died or started observing got the view back then (CBasePlayer::ResetViewEntity),
	// and FL_FROZEN is theirs now (Killed, StartObserver, PreThink for waiting players); a player who left
	// is not touched (the edict stays, and may belong to a new client)
	if ( FBitSet( pev->spawnflags, SF_CAMERA_ALL_PLAYERS ) )
	{
		ReleaseViewers();
		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)((CBaseEntity *)m_hPlayer);
	if ( !pPlayer )
		return;

	if ( pPlayer->IsAlive() && !pPlayer->has_disconnected )
	{
		SET_VIEW( pPlayer->edict(), pPlayer->edict() );
		// SBO 2.0 (Featureful 057c8ae5): only a camera that took the controls (Freeze Player) gives them back
		if ( FBitSet( pev->spawnflags, SF_CAMERA_PLAYER_TAKECONTROL ) )
			GiveControl( pPlayer, m_iCapsTaken );
	}
	if ( (CBaseEntity *)pPlayer->m_hViewEntity == this )
		pPlayer->m_hViewEntity = NULL;
}

// SBO 2.0 (Featureful 057c8ae5): a camera removed (killtarget) while it has a player's view gives it back; the view
// stayed on the freed edict and Freeze Player kept the player frozen. Also a camera turned off in the same frame
// (m_state 0, its FollowTarget not run yet)
void CTriggerCamera::UpdateOnRemove( void )
{
	CBasePlayer *pPlayer = (CBasePlayer *)((CBaseEntity *)m_hPlayer);
	if ( m_state || ( pPlayer && (CBaseEntity *)pPlayer->m_hViewEntity == this ) || FBitSet( pev->spawnflags, SF_CAMERA_ALL_PLAYERS ) )
	{
		ReleasePlayer();
		m_state = 0;
	}
	CBaseDelay::UpdateOnRemove();
}

void CTriggerCamera::Move()
{
	// Not moving on a path, return
	if (!m_pentPath)
		return;

	// Subtract movement from the previous frame
	m_moveDistance -= pev->speed * gpGlobals->frametime;

	// Have we moved enough to reach the target?
	if ( m_moveDistance <= 0 )
	{
		// Fire the passtarget if there is one
		if ( m_pentPath->pev->message )
		{
			FireTargets( STRING(m_pentPath->pev->message), this, this, USE_TOGGLE, 0 );
			if ( FBitSet( m_pentPath->pev->spawnflags, SF_CORNER_FIREONCE ) )
				m_pentPath->pev->message = 0;
		}
		// Time to go to the next target
		m_pentPath = m_pentPath->GetNextTarget();

		// Set up next corner
		if ( !m_pentPath )
		{
			pev->velocity = g_vecZero;
		}
		else 
		{
			if ( m_pentPath->pev->speed != 0 )
				m_targetSpeed = m_pentPath->pev->speed;

			Vector delta = m_pentPath->pev->origin - pev->origin;
			m_moveDistance = delta.Length();
			pev->movedir = delta.Normalize();
			m_flStopTime = gpGlobals->time + m_pentPath->GetDelay();
		}
	}

	// SBO 2.0 (Featureful): "Instant Move": the path's speed at once, straight along the path
	if ( pev->spawnflags & SF_CAMERA_INSTANT_MOVE )
	{
		if ( m_pentPath )
		{
			pev->speed = m_targetSpeed;
			pev->velocity = pev->movedir * pev->speed;
		}
		return;
	}

	if ( m_flStopTime > gpGlobals->time )
		pev->speed = UTIL_Approach( 0, pev->speed, m_deceleration * gpGlobals->frametime );
	else
		pev->speed = UTIL_Approach( m_targetSpeed, pev->speed, m_acceleration * gpGlobals->frametime );

	float fraction = 2 * gpGlobals->frametime;
	pev->velocity = ((pev->movedir * pev->speed) * fraction) + (pev->velocity * (1-fraction));
}

//=========================================================
// Rain and snow (docs/WEATHER.md). The client draws them with the CS 1.6 weather (cs16-client environment.cpp); the
// server sends each one with the WeatherVol message: the whole-map weather (CWeather: env_rain / env_snow, and CS's
// func_rain / func_snow) and the brush volumes (CWeatherVolume: func_rainvolume / func_snowvolume) share its keys,
// its spawnflags (Featureful's numbers) and the message; only the brush flag tells the client a volume from a
// whole-map weather.
//  - Keys: "intensity" (drops / flakes per update), "update_period" (seconds), rain: raindrop_width / raindrop_height /
//    raindrop_min_speed / raindrop_max_speed / raindrop_life, snow: snowflake_min_size / snowflake_max_size /
//    snowflake_min_speed / snowflake_max_speed / snowflake_life. Left out (or 0): the CS weather's values, except a
//    volume's intensity (150 for snow too, Featureful's).
//  - Wind (Spirit of Half-Life's rain_modify keys, also read under Spirit's names m_flWindX / m_flWindY / m_flRandX /
//    m_flRandY): "wind_x" / "wind_y", a fixed sideways speed (units per second) of every drop / flake, which
//    replaces the CS wind (random, changing every 15-30 s) once either is given; "wind_rand_x" / "wind_rand_y", a
//    random extra -n..n per drop / flake, on the fixed wind or on the CS wind. All left out: the CS wind as before.
//    Spawnflag 4 (not affected by wind) still means no sideways speed at all.
//  - Spawnflags: 1 Start Active (always on without a name), 2 Allow indoor, 4 not affected by wind, rain 32 no wind
//    puffs / 64 no splashes / 128 no ripples, 256 goes through brush entities, 512 ignores water.
//  - Use: on / off / toggle; every client gets the change at once (one message, no per-frame traffic).
//  - A key or spawnflag changed at run time (trigger_changevalue / trigger_copyvalue: KeyValue_DispatchRuntime,
//    RuntimeKeyValueChanged) gets SetDefaults' defaults and limits, and every client gets the weather again in the
//    next frame (one message for all the keys changed in a frame).
//  - cl_weather (0-3) multiplies every intensity on the client (0: none).
//=========================================================
#define SF_WEATHER_ACTIVE			1
#define WEATHERVOL_BRUSH_FLAG		( 1 << 12 )	// the client's WEATHER_BRUSH_ENTITY
#define WEATHERVOL_FIXED_WIND		( 1 << 13 )	// the client's WEATHER_FIXED_WIND: wind_x / wind_y given
#define WEATHER_SENDFLAGS			( 2 | 4 | 32 | 64 | 128 | 256 | 512 )

extern int gmsgWeatherVol;

// what both kinds share: the keys, Use and the message. A CBaseTrigger for CS's brush env_rain / func_rain
// (InitTrigger, CWeather::Spawn).
class CWeatherBase : public CBaseTrigger
{
public:
	void KeyValue( KeyValueData *pkvd );
	CKeyValue GetKeyValue( const char *keyName );
	void RuntimeKeyValueChanged( const char *keyName );
	void EXPORT ResendThink( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual BOOL IsSnow( void ) = 0;
	virtual BOOL IsVolume( void ) = 0;
	virtual int DefaultIntensity( void ) = 0;
	virtual BOOL IsSent( void ) { return TRUE; }	// its clients get it (CWeather: named, or the map's unnamed one)
	void SetDefaults( void );
	// SBO 2.1 (Spirit of Half-Life env_rain): On while it is active (an unnamed one always is), Off while it is off
	STATE GetState( CBaseEntity *pActivator ) { return FBitSet( pev->spawnflags, SF_WEATHER_ACTIVE ) ? STATE_ON : STATE_OFF; }
	void Send( CBasePlayer *pPlayer );	// NULL: every client

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iIntensity;
	float m_flUpdatePeriod;
	float m_flSize1, m_flSize2;		// rain: width, height; snow: min, max size
	float m_flSpeed1, m_flSpeed2;	// min, max falling speed
	float m_flLife;
	float m_flWindX, m_flWindY;		// wind_x / wind_y: the fixed sideways speed (units per second)
	float m_flRandX, m_flRandY;		// wind_rand_x / wind_rand_y: random extra sideways speed, -n..n
	int m_iWindSet;					// 1 wind_x, 2 wind_y given: the fixed wind replaces the CS wind
};

TYPEDESCRIPTION CWeatherBase::m_SaveData[] =
{
	DEFINE_FIELD( CWeatherBase, m_iIntensity, FIELD_INTEGER ),
	DEFINE_FIELD( CWeatherBase, m_flUpdatePeriod, FIELD_FLOAT ),
	DEFINE_FIELD( CWeatherBase, m_flSize1, FIELD_FLOAT ),
	DEFINE_FIELD( CWeatherBase, m_flSize2, FIELD_FLOAT ),
	DEFINE_FIELD( CWeatherBase, m_flSpeed1, FIELD_FLOAT ),
	DEFINE_FIELD( CWeatherBase, m_flSpeed2, FIELD_FLOAT ),
	DEFINE_FIELD( CWeatherBase, m_flLife, FIELD_FLOAT ),
	DEFINE_FIELD( CWeatherBase, m_flWindX, FIELD_FLOAT ),
	DEFINE_FIELD( CWeatherBase, m_flWindY, FIELD_FLOAT ),
	DEFINE_FIELD( CWeatherBase, m_flRandX, FIELD_FLOAT ),
	DEFINE_FIELD( CWeatherBase, m_flRandY, FIELD_FLOAT ),
	DEFINE_FIELD( CWeatherBase, m_iWindSet, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CWeatherBase, CBaseTrigger );

void CWeatherBase::KeyValue( KeyValueData *pkvd )
{
	float f = atof( pkvd->szValue );
	if ( FStrEq( pkvd->szKeyName, "intensity" ) )
		m_iIntensity = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "update_period" ) )
		m_flUpdatePeriod = f;
	else if ( FStrEq( pkvd->szKeyName, "raindrop_width" ) || FStrEq( pkvd->szKeyName, "snowflake_min_size" ) )
		m_flSize1 = f;
	else if ( FStrEq( pkvd->szKeyName, "raindrop_height" ) || FStrEq( pkvd->szKeyName, "snowflake_max_size" ) )
		m_flSize2 = f;
	else if ( FStrEq( pkvd->szKeyName, "raindrop_min_speed" ) || FStrEq( pkvd->szKeyName, "snowflake_min_speed" ) )
		m_flSpeed1 = f;
	else if ( FStrEq( pkvd->szKeyName, "raindrop_max_speed" ) || FStrEq( pkvd->szKeyName, "snowflake_max_speed" ) )
		m_flSpeed2 = f;
	else if ( FStrEq( pkvd->szKeyName, "raindrop_life" ) || FStrEq( pkvd->szKeyName, "snowflake_life" ) )
		m_flLife = f;
	// wind_x / wind_y given with a value: the fixed wind (an empty value is left out)
	else if ( FStrEq( pkvd->szKeyName, "wind_x" ) || FStrEq( pkvd->szKeyName, "m_flWindX" ) )
	{
		m_flWindX = f;
		m_iWindSet = pkvd->szValue[0] ? ( m_iWindSet | 1 ) : ( m_iWindSet & ~1 );
	}
	else if ( FStrEq( pkvd->szKeyName, "wind_y" ) || FStrEq( pkvd->szKeyName, "m_flWindY" ) )
	{
		m_flWindY = f;
		m_iWindSet = pkvd->szValue[0] ? ( m_iWindSet | 2 ) : ( m_iWindSet & ~2 );
	}
	else if ( FStrEq( pkvd->szKeyName, "wind_rand_x" ) || FStrEq( pkvd->szKeyName, "m_flRandX" ) )
		m_flRandX = f;
	else if ( FStrEq( pkvd->szKeyName, "wind_rand_y" ) || FStrEq( pkvd->szKeyName, "m_flRandY" ) )
		m_flRandY = f;
	else
	{
		CBaseTrigger::KeyValue( pkvd );
		return;
	}
	pkvd->fHandled = TRUE;
}

// the current value of a weather key (trigger_changevalue's operations, trigger_copyvalue, calc entities)
CKeyValue CWeatherBase::GetKeyValue( const char *keyName )
{
	static const char *s_floatKeys[] = { "update_period", "raindrop_width", "snowflake_min_size", "raindrop_height",
		"snowflake_max_size", "raindrop_min_speed", "snowflake_min_speed", "raindrop_max_speed", "snowflake_max_speed",
		"raindrop_life", "snowflake_life", "wind_x", "m_flWindX", "wind_y", "m_flWindY", "wind_rand_x", "m_flRandX",
		"wind_rand_y", "m_flRandY" };
	const float floats[] = { m_flUpdatePeriod, m_flSize1, m_flSize1, m_flSize2, m_flSize2, m_flSpeed1, m_flSpeed1,
		m_flSpeed2, m_flSpeed2, m_flLife, m_flLife, m_flWindX, m_flWindX, m_flWindY, m_flWindY, m_flRandX, m_flRandX,
		m_flRandY, m_flRandY };

	CKeyValue keyvalue = g_emptyKeyValue;
	if ( FStrEq( keyName, "intensity" ) )
	{
		keyvalue.keyType = KEY_TYPE_INT;
		keyvalue.keyName = "intensity";
		keyvalue.iVal = m_iIntensity;
		return keyvalue;
	}
	for ( int i = 0; i < (int)ARRAYSIZE( s_floatKeys ); i++ )
	{
		if ( FStrEq( keyName, s_floatKeys[i] ) )
		{
			keyvalue.keyType = KEY_TYPE_FLOAT;
			keyvalue.keyName = s_floatKeys[i];
			keyvalue.fVal = floats[i];
			return keyvalue;
		}
	}
	return CBaseTrigger::GetKeyValue( keyName );
}

// trigger_changevalue / trigger_copyvalue changed a key or the spawnflags (keyvalue.cpp KeyValue_DispatchRuntime): the
// defaults and limits of Spawn, then every client gets the weather in the next frame (after HandleTarget's
// UTIL_SetOrigin, so a moved volume sends its new box; several keys changed in one frame make one message)
void CWeatherBase::RuntimeKeyValueChanged( const char *keyName )
{
	if ( FStringNull( pev->targetname ) )
		SetBits( pev->spawnflags, SF_WEATHER_ACTIVE );	// an unnamed one is always on
	SetDefaults();
	SetThink( &CWeatherBase::ResendThink );
	pev->nextthink = gpGlobals->time + 0.01;
}

void CWeatherBase::ResendThink( void )
{
	SetThink( NULL );
	if ( IsSent() )
		Send( NULL );
}

// a key left out or 0 (or below) gets the CS weather's value (cs16-client RainData / SnowData), the intensity the
// class's; clamped to what the client keeps. The wind keys have no default: left out is the CS wind.
void CWeatherBase::SetDefaults( void )
{
	const BOOL fSnow = IsSnow();
	if ( m_iIntensity <= 0 )
		m_iIntensity = DefaultIntensity();
	if ( m_iIntensity > 1000 )
		m_iIntensity = 1000;
	if ( m_flUpdatePeriod <= 0 )
		m_flUpdatePeriod = fSnow ? 0.5 : 0.3;
	if ( m_flUpdatePeriod < 0.05 )
		m_flUpdatePeriod = 0.05;
	if ( m_flSize1 <= 0 )
		m_flSize1 = fSnow ? 2.1 : 2;
	if ( m_flSize2 <= 0 )
		m_flSize2 = fSnow ? 2.6 : 80;
	if ( m_flSpeed1 <= 0 )
		m_flSpeed1 = fSnow ? 131 : 500;
	if ( m_flSpeed2 <= 0 )
		m_flSpeed2 = fSnow ? 210 : 1800;
	if ( m_flLife <= 0 )
		m_flLife = fSnow ? 3.0 : 1.0;
	// a spread is -n..n either way
	m_flRandX = fabs( m_flRandX );
	m_flRandY = fabs( m_flRandY );
}

// WeatherVol: short entity index, byte type (1 rain, 2 snow; 0 removes it), short flags (the client's weather
// flags: 1, the brush flag for a volume, the spawnflags it reads), short intensity, short update period x 100, 6
// coords (the entity's box: where a volume's drops fall; unused for a whole-map weather), 4 shorts: rain width,
// height, min and max speed / snow min size x 10, max size x 10, min and max speed (0: the client's CS default),
// short life x 100 (0: default), 4 signed shorts: wind x, wind y (units per second; used with the fixed-wind flag),
// random spread x, y (0: none). 39 bytes. pPlayer NULL: every client.
void CWeatherBase::Send( CBasePlayer *pPlayer )
{
	const BOOL fActive = FBitSet( pev->spawnflags, SF_WEATHER_ACTIVE ) != 0;
	const BOOL fSnow = IsSnow();
	// rounded, not truncated: 2.1 x 10 is 20.99 in float
	auto Clamp = []( float v ) { return (int)( v < 0 ? 0 : ( v > 32000 ? 32000 : v + 0.5f ) ); };
	auto ClampSigned = []( float v ) { return (int)( v < -32000 ? -32000 : ( v > 32000 ? 32000 : floor( v + 0.5f ) ) ); };
	if ( pPlayer )
		MESSAGE_BEGIN( MSG_ONE, gmsgWeatherVol, NULL, pPlayer->pev );
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgWeatherVol );
		WRITE_SHORT( entindex() );
		WRITE_BYTE( fActive ? ( fSnow ? 2 : 1 ) : 0 );
		WRITE_SHORT( ( IsVolume() ? WEATHERVOL_BRUSH_FLAG : 0 ) | ( m_iWindSet ? WEATHERVOL_FIXED_WIND : 0 ) | 1
			| ( pev->spawnflags & WEATHER_SENDFLAGS ) );
		WRITE_SHORT( m_iIntensity );
		WRITE_SHORT( Clamp( m_flUpdatePeriod * 100 ) );
		WRITE_COORD( pev->absmin.x );
		WRITE_COORD( pev->absmin.y );
		WRITE_COORD( pev->absmin.z );
		WRITE_COORD( pev->absmax.x );
		WRITE_COORD( pev->absmax.y );
		WRITE_COORD( pev->absmax.z );
		WRITE_SHORT( Clamp( fSnow ? m_flSize1 * 10 : m_flSize1 ) );
		WRITE_SHORT( Clamp( fSnow ? m_flSize2 * 10 : m_flSize2 ) );
		WRITE_SHORT( Clamp( m_flSpeed1 ) );
		WRITE_SHORT( Clamp( m_flSpeed2 ) );
		WRITE_SHORT( Clamp( m_flLife * 100 ) );
		WRITE_SHORT( ClampSigned( m_flWindX ) );
		WRITE_SHORT( ClampSigned( m_flWindY ) );
		WRITE_SHORT( Clamp( m_flRandX ) );
		WRITE_SHORT( Clamp( m_flRandY ) );
	MESSAGE_END();
}

void CWeatherBase::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// an unnamed one is always on
	if ( FStringNull( pev->targetname ) )
		return;
	BOOL fActive = FBitSet( pev->spawnflags, SF_WEATHER_ACTIVE ) != 0;
	if ( !ShouldToggle( useType, fActive ) )
		return;
	if ( fActive )
		ClearBits( pev->spawnflags, SF_WEATHER_ACTIVE );
	else
		SetBits( pev->spawnflags, SF_WEATHER_ACTIVE );
	Send( NULL );
}

//=========================================================
// ReGameDLL_CS (regamedll/dlls/triggers.cpp, triggers.h): CS 1.6 weather entities, rain or snow over the whole map
// around the view, wherever the sky is above. As in CS the brush of func_rain / func_snow (and of a brush env_rain /
// env_snow) does not limit where it falls. SBO 2.0: the keys and spawnflags of func_rainvolume / func_snowvolume
// (above); CS's have none.
//  - Without a name an entity is always on (as in CS, which has no Use), and only the map's first one counts: the
//    first unnamed env_rain / func_rain, else the first unnamed env_snow / func_snow (CS: rain wins). More unnamed
//    ones do nothing, as in CS.
//  - A named one is its own weather besides that one: off at the start unless Start Active is set, switched by Use.
//  - Keys left out: CS's rain (150 drops every 0.3 s within 400 units of the view) and snow (512 flakes every 0.5 s
//    within 1024 units).
//=========================================================
class CWeather : public CWeatherBase
{
public:
	void Spawn( void );
	BOOL IsSnow( void ) { return FClassnameIs( pev, "env_snow" ) || FClassnameIs( pev, "func_snow" ); }
	BOOL IsVolume( void ) { return FALSE; }
	int DefaultIntensity( void ) { return IsSnow() ? 512 : 150; }	// CS: 512 flakes / 150 drops per update
	BOOL IsSent( void );	// named, or the map's unnamed one
};

LINK_ENTITY_TO_CLASS( env_snow, CWeather );
LINK_ENTITY_TO_CLASS( func_snow, CWeather );
LINK_ENTITY_TO_CLASS( env_rain, CWeather );
LINK_ENTITY_TO_CLASS( func_rain, CWeather );

void CWeather :: Spawn( void )
{
	if ( FStringNull( pev->targetname ) )
		pev->spawnflags |= SF_WEATHER_ACTIVE;
	SetDefaults();

	// SBO: a point entity has no brush model for InitTrigger's SET_MODEL; it only has to exist
	if ( FStringNull( pev->model ) )
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->effects |= EF_NODRAW;
		return;
	}

	InitTrigger();
}

BOOL CWeather::IsSent( void )
{
	if ( !FStringNull( pev->targetname ) )
		return TRUE;
	static const char *s_classes[] = { "env_rain", "func_rain", "env_snow", "func_snow" };
	for ( int c = 0; c < 4; c++ )
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, s_classes[c] ) ) != NULL )
		{
			if ( FStringNull( pEntity->pev->targetname ) )
				return pEntity == this;
		}
	}
	return FALSE;
}

//=========================================================
// SBO 2.0, from Featureful (dlls/effects.cpp CFuncRainVolume / CFuncSnowVolume, Half-Life 1 SDK license): rain or snow
// only inside a brush, drawn by the clients with the CS 1.6 weather above (cs16-client environment.cpp, which already
// has Featureful's volume weather: WeatherData::IsVolume). Featureful's particle weather is not imported: the server
// sends the volume (WeatherVol) and the client spawns CS drops / flakes in it. docs/WEATHER.md "Rain and snow volumes".
//  - Keys and spawnflags: the weather keys above; intensity 150 when left out (snow too, Featureful's), update_period
//    0.3 rain, 0.5 snow.
//  - With a global env_rain / env_snow: both draw, the volume adds its own drops inside its brush.
//=========================================================
class CWeatherVolume : public CWeatherBase
{
public:
	void Spawn( void );
	BOOL IsSnow( void ) { return FClassnameIs( pev, "func_snowvolume" ); }
	BOOL IsVolume( void ) { return TRUE; }
	int DefaultIntensity( void ) { return 150; }	// Featureful's FGD, snow too
};

LINK_ENTITY_TO_CLASS( func_rainvolume, CWeatherVolume );
LINK_ENTITY_TO_CLASS( func_snowvolume, CWeatherVolume );

void CWeatherVolume::Spawn( void )
{
	// the brush gives the box; nothing solid, nothing drawn (as Featureful)
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	UTIL_SetOrigin( pev, pev->origin );
	pev->effects |= EF_NODRAW;
	if ( FStringNull( pev->targetname ) )
		pev->spawnflags |= SF_WEATHER_ACTIVE;

	// defaults (Featureful's FGD): 150 per update, rain every 0.3 s, snow every 0.5 s
	SetDefaults();
}

// the map's weather to one client (its HUD initialisation: connect, respawn, demo start), after its ReceiveW 0
// (CBasePlayer::SendWeatherInfo), which drops the whole-map weather the client had: the whole-map weather (the
// unnamed one and every named one), then every rain / snow volume. Inactive ones too, so a client that had one from
// before a respawn drops it.
void WeatherVol_SendAll( CBasePlayer *pPlayer )
{
	static const char *s_global[] = { "env_rain", "func_rain", "env_snow", "func_snow" };
	for ( int c = 0; c < 4; c++ )
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, s_global[c] ) ) != NULL )
		{
			if ( ( (CWeather *)pEntity )->IsSent() )
				( (CWeather *)pEntity )->Send( pPlayer );
		}
	}

	static const char *s_classes[] = { "func_rainvolume", "func_snowvolume" };
	for ( int c = 0; c < 2; c++ )
	{
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, s_classes[c] ) ) != NULL )
			( (CWeatherVolume *)pEntity )->Send( pPlayer );
	}
}

//=========================================================
// ReGameDLL_CS (regamedll/dlls/triggers.cpp): env_fog
//=========================================================
LINK_ENTITY_TO_CLASS( env_fog, CClientFog );

void CClientFog :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "density" ) )
	{
		m_fDensity = atof( pkvd->szValue );

		if ( m_fDensity < 0 || m_fDensity > 0.01 )
			m_fDensity = 0;

		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseEntity::KeyValue( pkvd );
	}
}

void CClientFog :: Spawn( void )
{
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;				// Remove model & collisions
	pev->renderamt = 0;				// The engine won't draw this model if this is set to 0 and blending is on
	pev->rendermode = kRenderTransTexture;

	// Spirit of Half-Life (CEnvFog::Spawn): a fog nothing can switch on is on
	if ( FStringNull( pev->targetname ) )
		SetBits( pev->spawnflags, SF_FOG_ACTIVE );
}

// SBO 2.1: "density" (KeyValue keeps it as a number in 0-0.01)
CKeyValue CClientFog :: GetKeyValue( const char *keyName )
{
	if ( FStrEq( keyName, "density" ) )
	{
		CKeyValue keyvalue = g_emptyKeyValue;
		keyvalue.keyType = KEY_TYPE_FLOAT;
		keyvalue.keyName = "density";
		keyvalue.fVal = m_fDensity;
		return keyvalue;
	}
	return CBaseEntity::GetKeyValue( keyName );
}

// SBO 2.1: trigger_changevalue / trigger_copyvalue changed a key (density, rendercolor, spawnflags, origin, ...):
// an unnamed fog or zone stays on (Spawn), and the players in the game get it again in the next frame, so several
// keys changed in one frame make one send and a moved zone sends its new box (HandleTarget's UTIL_SetOrigin runs
// after the key). Neither entity thinks otherwise.
void CClientFog :: RuntimeKeyValueChanged( const char *keyName )
{
	if ( FStringNull( pev->targetname ) )
		SetBits( pev->spawnflags, SF_FOG_ACTIVE );	// SF_FOGZONE_ACTIVE is the same bit
	SetThink( &CClientFog::ResendThink );
	pev->nextthink = gpGlobals->time + 0.01;
}

void CClientFog :: ResendThink( void )
{
	SetThink( NULL );
	const BOOL fZone = FClassnameIs( pev, "env_fogzone" );
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->m_fGameHUDInitialized || pPlayer->has_disconnected )
			continue;
		if ( fZone )
			FogZone_Send( pPlayer, (CEnvFogZone *)this );
		else
			pPlayer->SendFog();
	}
}

// Spirit of Half-Life (CEnvFog::Use: USE_ON, USE_OFF, USE_TOGGLE). Spirit sends the new fog to every client; SBO sends
// every player in the game his Fog again (CBasePlayer::SendFog: the first active env_fog, else none). A player in an
// env_fogzone keeps the zone's fog there (the client goes from the map's fog to the zone's by depth, tri.cpp). The
// client fades to the new fog over a second (tri.cpp Fog_Set); Spirit's fadein / fadeout / holdtime keys are not read.
void CClientFog :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, FBitSet( pev->spawnflags, SF_FOG_ACTIVE ) ) )
		return;

	if ( FBitSet( pev->spawnflags, SF_FOG_ACTIVE ) )
		ClearBits( pev->spawnflags, SF_FOG_ACTIVE );
	else
		SetBits( pev->spawnflags, SF_FOG_ACTIVE );

	// the others get the fog with their HUD (CBasePlayer::UpdateClientData)
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->m_fGameHUDInitialized && !pPlayer->has_disconnected )
			pPlayer->SendFog();
	}
}

//=========================================================
// XDM (dlls/environment.cpp): env_fogzone, the fog of a brush volume. XDM's zone is SOLID_NOT with CONTENTS_FOG as its
// skin, so its Touch never runs; SBO's is a trigger. Touch only notes the zone for the player's move; the Fog message
// goes out when the zone a player is in changes (CBasePlayer::UpdateFogZone). See docs/WEATHER.md.
//=========================================================
LINK_ENTITY_TO_CLASS( env_fogzone, CEnvFogZone );

BOOL g_fFogZones = FALSE;

void CEnvFogZone :: Spawn( void )
{
	// XDM: a zone nothing can switch on is on
	if ( FStringNull( pev->targetname ) )
		SetBits( pev->spawnflags, SF_FOGZONE_ACTIVE );

	// SBO: no brush, no volume (SET_MODEL needs a model)
	if ( FStringNull( pev->model ) )
	{
		ALERT( at_console, "env_fogzone without a brush at (%g %g %g)\n", pev->origin.x, pev->origin.y, pev->origin.z );
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->effects |= EF_NODRAW;
		return;
	}

	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( ENT(pev), STRING(pev->model) );    // set size and link into world
	if ( CVAR_GET_FLOAT("showtriggers") == 0 )
		SetBits( pev->effects, EF_NODRAW );

	g_fFogZones = TRUE;
}

// SBO: observers are not solid and touch no trigger, so CBasePlayer::UpdateFogZone asks for a free camera's position.
// A point trace against this entity's brush model alone starts solid inside the brush (not only inside its box).
BOOL CEnvFogZone :: IsPointInside( const Vector &vecPoint )
{
	if ( !FBitSet( pev->spawnflags, SF_FOGZONE_ACTIVE ) || pev->solid != SOLID_TRIGGER )
		return FALSE;

	if ( vecPoint.x < pev->absmin.x || vecPoint.y < pev->absmin.y || vecPoint.z < pev->absmin.z
		|| vecPoint.x > pev->absmax.x || vecPoint.y > pev->absmax.y || vecPoint.z > pev->absmax.z )
		return FALSE;

	TraceResult tr;
	UTIL_TraceModel( vecPoint, vecPoint, point_hull, edict(), &tr );

	return tr.fStartSolid != 0;
}

void CEnvFogZone :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, FBitSet( pev->spawnflags, SF_FOGZONE_ACTIVE ) ) )
		return;

	// the players in the zone notice with their next move: Touch stops or starts noting it (free observers with
	// their next check)
	if ( FBitSet( pev->spawnflags, SF_FOGZONE_ACTIVE ) )
		ClearBits( pev->spawnflags, SF_FOGZONE_ACTIVE );
	else
		SetBits( pev->spawnflags, SF_FOGZONE_ACTIVE );

	// PROTOTYPE: the clients draw the zone, tell them (the ones in the game: the others get the list with their HUD)
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->m_fGameHUDInitialized && !pPlayer->has_disconnected )
			FogZone_Send( pPlayer, this );
	}
}

// PROTOTYPE (docs/WEATHER.md "env_fogzone prototype"). FogZone: byte index (map order), byte flags (1 active,
// 2 first of a full list: the client forgets its list), coord x 6 (the brush model's bounding box: absmin, absmax),
// byte r g b, the density float as 4 bytes (as in Fog).
extern int gmsgFogZone;

void FogZone_Send( CBasePlayer *pPlayer, CEnvFogZone *pOnly )
{
	if ( !g_fFogZones || !pPlayer )
		return;

	CBaseEntity *pEntity = NULL;
	int iIndex = 0;
	BOOL fFirst = ( pOnly == NULL );

	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "env_fogzone" ) ) != NULL && iIndex < 255 )
	{
		CEnvFogZone *pZone = (CEnvFogZone *)pEntity;
		const int iThis = iIndex++;

		if ( pZone->pev->solid != SOLID_TRIGGER || ( pOnly && pZone != pOnly ) )
			continue;	// no brush / not the one asked for (the index still counts)

		union
		{
			float f;
			char b[4];
		} density;

		density.f = pZone->m_fDensity;

		auto ClampColor = []( float flValue ) { int i = (int)flValue; return i < 0 ? 0 : ( i > 255 ? 255 : i ); };

		MESSAGE_BEGIN( MSG_ONE, gmsgFogZone, NULL, pPlayer->pev );
			WRITE_BYTE( iThis );
			WRITE_BYTE( ( FBitSet( pZone->pev->spawnflags, SF_FOGZONE_ACTIVE ) ? 1 : 0 ) | ( fFirst ? 2 : 0 ) );
			WRITE_COORD( pZone->pev->absmin.x );
			WRITE_COORD( pZone->pev->absmin.y );
			WRITE_COORD( pZone->pev->absmin.z );
			WRITE_COORD( pZone->pev->absmax.x );
			WRITE_COORD( pZone->pev->absmax.y );
			WRITE_COORD( pZone->pev->absmax.z );
			WRITE_BYTE( ClampColor( pZone->pev->rendercolor[0] ) );
			WRITE_BYTE( ClampColor( pZone->pev->rendercolor[1] ) );
			WRITE_BYTE( ClampColor( pZone->pev->rendercolor[2] ) );
			WRITE_BYTE( density.b[0] );
			WRITE_BYTE( density.b[1] );
			WRITE_BYTE( density.b[2] );
			WRITE_BYTE( density.b[3] );
		MESSAGE_END();

		fFirst = FALSE;
	}
}

void CEnvFogZone :: Touch( CBaseEntity *pOther )
{
	if ( !FBitSet( pev->spawnflags, SF_FOGZONE_ACTIVE ) )
		return;

	if ( !pOther->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	// overlapping zones: the one the player is in already stays, otherwise the first one touched in this move
	if ( pPlayer->m_hFogZoneTouch == NULL || (CBaseEntity *)pPlayer->m_hFogZone == this )
		pPlayer->m_hFogZoneTouch = this;
}
