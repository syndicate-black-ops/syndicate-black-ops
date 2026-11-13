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

===== subs.cpp ========================================================

  frequently used global functions

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "nodes.h"
#include "doors.h"
#include "movewith.h"	// SBO 2.0: a child mover's moves are in its parent's frame
#include "sbo_test.h"	// SBO_TEST_COMMANDS builds: the test layer (docs/dev/ENTITY_TESTS.md)
#include "alias.h"	// SBO 2.1: alias changes wait for the end of FireTargets

extern CGraph WorldGraph;

extern BOOL FEntIsVisible(entvars_t* pev, entvars_t* pevTarget);

extern DLL_GLOBAL int g_iSkillLevel;


// Landmark class
void CPointEntity :: Spawn( void )
{
	pev->solid = SOLID_NOT;
//	UTIL_SetSize(pev, g_vecZero, g_vecZero);
}


class CNullEntity : public CBaseEntity
{
public:
	void Spawn( void );
};


// Null Entity, remove on startup
void CNullEntity :: Spawn( void )
{
	REMOVE_ENTITY(ENT(pev));
}
LINK_ENTITY_TO_CLASS(info_null,CNullEntity);

class CBaseDMStart : public CPointEntity
{
public:
	void		KeyValue( KeyValueData *pkvd );
	BOOL		IsTriggered( CBaseEntity *pEntity );

private:
};

// These are the new entry points to entities. 
LINK_ENTITY_TO_CLASS(info_player_go2,CBaseDMStart);
LINK_ENTITY_TO_CLASS(info_player_go3,CBaseDMStart);
LINK_ENTITY_TO_CLASS(info_player_go4,CBaseDMStart);
LINK_ENTITY_TO_CLASS(info_player_go5,CBaseDMStart);
LINK_ENTITY_TO_CLASS(info_player_deathmatch,CBaseDMStart);
LINK_ENTITY_TO_CLASS(info_player_start,CPointEntity);
LINK_ENTITY_TO_CLASS(info_landmark,CPointEntity);

void CBaseDMStart::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "master"))
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

BOOL CBaseDMStart::IsTriggered( CBaseEntity *pEntity )
{
	BOOL master = UTIL_IsMasterTriggered( pev->netname, pEntity );

	return master;
}

// This updates global tables that need to know about entities being removed
void CBaseEntity::UpdateOnRemove( void )
{
	int	i;

	if ( FBitSet( pev->flags, FL_GRAPHED ) )
	{
	// this entity was a LinkEnt in the world node graph, so we must remove it from
	// the graph since we are removing it from the world.
		for ( i = 0 ; i < WorldGraph.m_cLinks ; i++ )
		{
			if ( WorldGraph.m_pLinkPool [ i ].m_pLinkEnt == pev )
			{
				// if this link has a link ent which is the same ent that is removing itself, remove it!
				WorldGraph.m_pLinkPool [ i ].m_pLinkEnt = NULL;
			}
		}
	}
	if ( pev->globalname )
		gGlobalState.EntitySetState( pev->globalname, GLOBAL_DEAD );
	MoveWith_EntityRemoved( edict() );	// SBO 2.0 movewith: the child ambient sounds playing on it stop with it
}

// Convenient way to delay removing oneself
void CBaseEntity :: SUB_Remove( void )
{
	UpdateOnRemove();
	if (pev->health > 0)
	{
		// this situation can screw up monsters who can't tell their entity pointers are invalid.
		pev->health = 0;
		ALERT( at_aiconsole, "SUB_Remove called on entity with health > 0\n");
	}

	REMOVE_ENTITY(ENT(pev));
}


// Convenient way to explicitly do nothing (passed to functions that require a method)
void CBaseEntity :: SUB_DoNothing( void )
{
}


// Global Savedata for Delay
TYPEDESCRIPTION	CBaseDelay::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseDelay, m_flDelay, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseDelay, m_iszKillTarget, FIELD_STRING ),
	DEFINE_FIELD( CBaseDelay, m_hActivator, FIELD_EHANDLE ),	// SBO 2.0 (Featureful c0d5b843, 1645afbe)
	DEFINE_FIELD( CBaseDelay, m_hDelayedCaller, FIELD_EHANDLE ),	// SBO 2.1
};

IMPLEMENT_SAVERESTORE( CBaseDelay, CBaseEntity );

void CBaseDelay :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "delay"))
	{
		m_flDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "killtarget"))
	{
		m_iszKillTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseEntity::KeyValue( pkvd );
	}
}


/*
==============================
SUB_UseTargets

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Removes all entities with a targetname that match self.killtarget,
and removes them, so some events can remove other triggers.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function (if they have one)

==============================
*/
void CBaseEntity :: SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value )
{
	//
	// fire targets
	//
	if (!FStringNull(pev->target))
	{
		FireTargets( STRING(pev->target), pActivator, this, useType, value );
	}
}


// SBO 2.0 (Half-Life Unified SDK): the target may end in '*' (every entity whose name starts with the rest) or be
// "!activator" / "!caller" (UTIL_FindEntityByTargetname; docs/ENTITY_TRIGGERING.md)
void FireTargets( const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	edict_t *pentTarget = NULL;
	if ( !targetName )
		return;

	ALERT( at_aiconsole, "Firing: (%s)\n", targetName );
	SBO_TEST_CALL( Test_TraceFire( targetName, pActivator, pCaller, useType, value ) );

	CBaseEntity *pSelected;
	BOOL bSelector = UTIL_TargetSelector( targetName, pActivator, pCaller, &pSelected );
	size_t nameLen = strlen( targetName );
	// SBO 2.1: an alias ("*name") may name the entity firing too, as a wildcard may
	BOOL bWildcard = nameLen && ( targetName[ nameLen - 1 ] == '*' || targetName[0] == '*' );

	Alias_FireTargetsBegin();	// SBO 2.1 (Spirit): alias changes made by these targets wait until the firing is done

	for (;;)
	{
		CBaseEntity *pTarget;
		if ( bSelector )
		{
			pTarget = pSelected;	// one entity (or none)
			pSelected = NULL;
			if ( !pTarget )
				break;

			// SBO: the caller is the entity firing ("!caller" is itself; a trigger_relay is its own "!activator"):
			// using it again would repeat without end (the Unified SDK recursed until the server crashed)
			if ( pTarget == pCaller )
			{
				ALERT( at_aiconsole, "%s \"%s\": target %s is itself, not triggered\n", STRING( pCaller->pev->classname ),
					STRING( pCaller->pev->targetname ), targetName );
				break;
			}
		}
		else
		{
			pentTarget = FIND_ENTITY_BY_TARGETNAME(pentTarget, targetName);
			if (FNullEnt(pentTarget))
				break;
			pTarget = CBaseEntity::Instance( pentTarget );

			// SBO 2.1: a wildcard never matches the entity firing (a relay "room1_relay" targeting "room1_*" would
			// trigger itself without end), as the selectors above
			if ( bWildcard && pTarget && pTarget == pCaller )
			{
				ALERT( at_aiconsole, "%s \"%s\": target %s matches itself, not triggered\n", STRING( pCaller->pev->classname ),
					STRING( pCaller->pev->targetname ), targetName );
				continue;
			}
		}

		if ( pTarget && !(pTarget->pev->flags & FL_KILLME) )	// Don't use dying ents
		{
			ALERT( at_aiconsole, "Found: %s, firing (%s)\n", STRING(pTarget->pev->classname), targetName );
			SBO_TEST_CALL( Test_TraceUse( pTarget, pActivator, pCaller, useType, value ) );
			pTarget->Use( pActivator, pCaller, useType, value );
		}
	}

	Alias_FireTargetsDone();
}

LINK_ENTITY_TO_CLASS( DelayedUse, CBaseDelay );


void CBaseDelay :: SUB_UseTargets( CBaseEntity *pActivator, USE_TYPE useType, float value )
{
	//
	// exit immediatly if we don't have a target or kill target
	//
	if (FStringNull(pev->target) && !m_iszKillTarget)
		return;

	//
	// check for a delay
	//
	if (m_flDelay != 0)
	{
		// create a temp object to fire at a later time
		CBaseDelay *pTemp = GetClassPtr( (CBaseDelay *)NULL);
		pTemp->pev->classname = MAKE_STRING("DelayedUse");

		pTemp->pev->nextthink = gpGlobals->time + m_flDelay;

		pTemp->SetThink( &CBaseDelay::DelayThink );
		
		// Save the useType
		pTemp->pev->button = (int)useType;
		pTemp->m_iszKillTarget = m_iszKillTarget;
		pTemp->m_flDelay = 0; // prevent "recursion"
		pTemp->pev->target = pev->target;

		// SBO 2.0 (Featureful c0d5b843): any activator is kept (Half-Life kept only a player, in pev->owner)
		pTemp->m_hActivator = pActivator;
		pTemp->m_hDelayedCaller = this;	// SBO 2.1: CMultiSource::Use toggles this entity's slot

		return;
	}

	SUB_KillTargets( pActivator );

	//
	// fire targets
	//
	if (!FStringNull(pev->target))
	{
		FireTargets( STRING(pev->target), pActivator, this, useType, value );
	}
}

// SBO 2.0: removes the KillTarget entities (SUB_UseTargets, trigger_changetarget): "!activator" / "!caller" (this
// entity), a name or a wildcard; a player is never removed, and a wildcard never matches this entity
void CBaseDelay :: SUB_KillTargets( CBaseEntity *pActivator )
{
	if ( m_iszKillTarget )
	{
		edict_t *pentKillTarget = NULL;
		const char *szKillTarget = STRING(m_iszKillTarget);

		ALERT( at_aiconsole, "KillTarget: %s\n", szKillTarget );

		// SBO 2.0 (Half-Life Unified SDK): "!activator" / "!caller" (the caller is this entity) and wildcards (the
		// FIND_ENTITY_BY_TARGETNAME search); a player is never removed (UTIL_IsRemovableEntity)
		CBaseEntity *pSelected;
		if ( UTIL_TargetSelector( szKillTarget, pActivator, this, &pSelected ) )
		{
			if ( pSelected && !pSelected->IsPlayer() )
			{
				UTIL_Remove( pSelected );
				ALERT( at_aiconsole, "killing %s\n", STRING( pSelected->pev->classname ) );
			}
			else if ( pSelected )
				ALERT( at_aiconsole, "Can't kill \"%s\": players are never removed\n", STRING( pSelected->pev->classname ) );
		}
		else
		{
			size_t killLen = strlen( szKillTarget );
			BOOL bWildcard = killLen && ( szKillTarget[ killLen - 1 ] == '*' || szKillTarget[0] == '*' );	// SBO 2.1: or an alias
			pentKillTarget = FIND_ENTITY_BY_TARGETNAME( NULL, szKillTarget );
			while ( !FNullEnt(pentKillTarget) )
			{
				CBaseEntity *pKill = CBaseEntity::Instance( pentKillTarget );
				// SBO 2.1: a wildcard never matches the entity firing (it would remove itself), as in FireTargets
				if ( bWildcard && pKill == this )
					ALERT( at_aiconsole, "%s \"%s\": killtarget %s matches itself, not removed\n", STRING( pev->classname ),
						STRING( pev->targetname ), szKillTarget );
				else if ( pKill && pKill->IsPlayer() )
					ALERT( at_aiconsole, "Can't kill \"%s\": players are never removed\n", STRING( pentKillTarget->v.classname ) );
				else
				{
					UTIL_Remove( pKill );
					ALERT( at_aiconsole, "killing %s\n", STRING( pentKillTarget->v.classname ) );
				}
				pentKillTarget = FIND_ENTITY_BY_TARGETNAME( pentKillTarget, szKillTarget );
			}
		}
	}
}


/*
void CBaseDelay :: SUB_UseTargetsEntMethod( void )
{
	SUB_UseTargets(pev);
}
*/

/*
QuakeEd only writes a single float for angles (bad idea), so up and down are
just constant angles.
*/
void SetMovedir( entvars_t *pev )
{
	if (pev->angles == Vector(0, -1, 0))
	{
		pev->movedir = Vector(0, 0, 1);
	}
	else if (pev->angles == Vector(0, -2, 0))
	{
		pev->movedir = Vector(0, 0, -1);
	}
	else
	{
		UTIL_MakeVectors(pev->angles);
		pev->movedir = gpGlobals->v_forward;
	}
	
	pev->angles = g_vecZero;
}




void CBaseDelay::DelayThink( void )
{
	// The use type is cached (and stashed) in pev->button
	SUB_UseTargets( m_hActivator, (USE_TYPE)pev->button, 0 );
	REMOVE_ENTITY(ENT(pev));
}


// Global Savedata for Toggle
TYPEDESCRIPTION	CBaseToggle::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseToggle, m_toggle_state, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseToggle, m_flActivateFinished, FIELD_TIME ),
	DEFINE_FIELD( CBaseToggle, m_flMoveDistance, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseToggle, m_flWait, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseToggle, m_flLip, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseToggle, m_flTWidth, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseToggle, m_flTLength, FIELD_FLOAT ),
	DEFINE_FIELD( CBaseToggle, m_vecPosition1, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseToggle, m_vecPosition2, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseToggle, m_vecAngle1, FIELD_VECTOR ),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD( CBaseToggle, m_vecAngle2, FIELD_VECTOR ),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD( CBaseToggle, m_cTriggersLeft, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseToggle, m_flHeight, FIELD_FLOAT ),
	//DEFINE_FIELD( CBaseToggle, m_hActivator, FIELD_EHANDLE ), // SBO 2.0 (Featureful c0d5b843): now in CBaseDelay
	DEFINE_FIELD( CBaseToggle, m_pfnCallWhenMoveDone, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseToggle, m_vecFinalDest, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CBaseToggle, m_vecFinalAngle, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseToggle, m_sMaster, FIELD_STRING),
	DEFINE_FIELD( CBaseToggle, m_bitsDamageInflict, FIELD_INTEGER ),	// damage type inflicted
};
IMPLEMENT_SAVERESTORE( CBaseToggle, CBaseAnimating );


void CBaseToggle::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "lip"))
	{
		m_flLip = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "wait"))
	{
		m_flWait = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		m_sMaster = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "distance"))
	{
		m_flMoveDistance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

/*
=============
LinearMove

calculate pev->velocity and pev->nextthink to reach vecDest from
pev->origin traveling at flSpeed
===============
*/
void CBaseToggle ::  LinearMove( Vector	vecDest, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "LinearMove:  no speed is defined!");
//	ASSERTSZ(m_pfnCallWhenMoveDone != NULL, "LinearMove: no post-move function defined");
	
	m_vecFinalDest = vecDest;

	// SBO 2.0 movewith: a child mover moves its own pose, in the frame of its parent as placed (movewith.cpp); its
	// positions (m_vecPosition1/2) are the editor's
	Vector vecOrigin = pev->origin;
	MoveWith_GetOwnPose( this, &vecOrigin, NULL );

	// Already there?
	if (vecDest == vecOrigin)
	{
		LinearMoveDone();
		return;
	}

	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDest - vecOrigin;

	// divide vector length by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// set nextthink to trigger a call to LinearMoveDone when dest is reached
	pev->nextthink = pev->ltime + flTravelTime;
	SetThink( &CBaseToggle::LinearMoveDone );

	// scale the destdelta vector by the time spent traveling to get velocity
	MoveWith_SetOwnVelocity( this, vecDestDelta / flTravelTime );	// pev->velocity unless a child
}


/*
============
After moving, set origin to exact final destination, call "move done" function
============
*/
void CBaseToggle :: LinearMoveDone( void )
{
	MoveWith_SetOwnOrigin( this, m_vecFinalDest );	// UTIL_SetOrigin unless a child
	MoveWith_SetOwnVelocity( this, g_vecZero );
	pev->nextthink = -1;
	if ( m_pfnCallWhenMoveDone )
		(this->*m_pfnCallWhenMoveDone)();
}

BOOL CBaseToggle :: IsLockedByMaster( void )
{
	if (m_sMaster && !UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return TRUE;
	else
		return FALSE;
}

// HL25: moved here from CBaseMonster so scripted_sentence can target non-monsters (buttons).
// Monsters keep their rule through the virtual IsAllowedToSpeak.
void CBaseToggle::PlaySentence( const char *pszSentence, float duration, float volume, float attenuation )
{
	if ( pszSentence && IsAllowedToSpeak() )
	{
		if ( pszSentence[0] == '!' )
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, pszSentence, volume, attenuation, 0, PITCH_NORM );
		else
			SENTENCEG_PlayRndSz( edict(), pszSentence, volume, attenuation, 0, PITCH_NORM );
	}
}

void CBaseToggle::PlayScriptedSentence( const char *pszSentence, float duration, float volume, float attenuation, BOOL bConcurrent, CBaseEntity *pListener )
{
	PlaySentence( pszSentence, duration, volume, attenuation );
}

void CBaseToggle::SentenceStop( void )
{
	EMIT_SOUND( edict(), CHAN_VOICE, "common/null.wav", 1.0, ATTN_IDLE );
}

/*
=============
AngularMove

calculate pev->velocity and pev->nextthink to reach vecDest from
pev->origin traveling at flSpeed
Just like LinearMove, but rotational.
===============
*/
void CBaseToggle :: AngularMove( Vector vecDestAngle, float flSpeed )
{
	ASSERTSZ(flSpeed != 0, "AngularMove:  no speed is defined!");
//	ASSERTSZ(m_pfnCallWhenMoveDone != NULL, "AngularMove: no post-move function defined");
	
	m_vecFinalAngle = vecDestAngle;

	// SBO 2.0 movewith: a child mover turns its own pose (see LinearMove)
	Vector vecAngles = pev->angles;
	MoveWith_GetOwnPose( this, NULL, &vecAngles );

	// Already there?
	if (vecDestAngle == vecAngles)
	{
		AngularMoveDone();
		return;
	}

	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDestAngle - vecAngles;
	
	// divide by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// set nextthink to trigger a call to AngularMoveDone when dest is reached
	pev->nextthink = pev->ltime + flTravelTime;
	SetThink( &CBaseToggle::AngularMoveDone );

	// scale the destdelta vector by the time spent traveling to get velocity
	MoveWith_SetOwnAvelocity( this, vecDestDelta / flTravelTime );	// pev->avelocity unless a child
}


/*
============
After rotating, set angle to exact final angle, call "move done" function
============
*/
void CBaseToggle :: AngularMoveDone( void )
{
	MoveWith_SetOwnAngles( this, m_vecFinalAngle );	// pev->angles unless a child
	MoveWith_SetOwnAvelocity( this, g_vecZero );
	pev->nextthink = -1;
	if ( m_pfnCallWhenMoveDone )
		(this->*m_pfnCallWhenMoveDone)();
}


float CBaseToggle :: AxisValue( int flags, const Vector &angles )
{
	if ( FBitSet(flags, SF_DOOR_ROTATE_Z) )
		return angles.z;
	if ( FBitSet(flags, SF_DOOR_ROTATE_X) )
		return angles.x;

	return angles.y;
}


void CBaseToggle :: AxisDir( entvars_t *pev )
{
	if ( FBitSet(pev->spawnflags, SF_DOOR_ROTATE_Z) )
		pev->movedir = Vector ( 0, 0, 1 );	// around z-axis
	else if ( FBitSet(pev->spawnflags, SF_DOOR_ROTATE_X) )
		pev->movedir = Vector ( 1, 0, 0 );	// around x-axis
	else
		pev->movedir = Vector ( 0, 1, 0 );		// around y-axis
}


float CBaseToggle :: AxisDelta( int flags, const Vector &angle1, const Vector &angle2 )
{
	if ( FBitSet (flags, SF_DOOR_ROTATE_Z) )
		return angle1.z - angle2.z;
	
	if ( FBitSet (flags, SF_DOOR_ROTATE_X) )
		return angle1.x - angle2.x;

	return angle1.y - angle2.y;
}


/*
=============
FEntIsVisible

returns TRUE if the passed entity is visible to caller, even if not infront ()
=============
*/
	BOOL
FEntIsVisible(
	entvars_t*		pev,
	entvars_t*		pevTarget)
	{
	Vector vecSpot1 = pev->origin + pev->view_ofs;
	Vector vecSpot2 = pevTarget->origin + pevTarget->view_ofs;
	TraceResult tr;

	UTIL_TraceLine(vecSpot1, vecSpot2, ignore_monsters, ENT(pev), &tr);
	
	if (tr.fInOpen && tr.fInWater)
		return FALSE;                   // sight line crossed contents

	if (tr.flFraction == 1)
		return TRUE;

	return FALSE;
	}


