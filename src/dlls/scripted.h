/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
#ifndef SCRIPTED_H
#define SCRIPTED_H

#ifndef SCRIPTEVENT_H
#include "scriptevent.h"
#endif

#define SF_SCRIPT_WAITTILLSEEN		1
#define SF_SCRIPT_EXITAGITATED		2
#define SF_SCRIPT_REPEATABLE		4
#define SF_SCRIPT_LEAVECORPSE		8
//#define SF_SCRIPT_INTERPOLATE		16 // don't use, old bug
#define SF_SCRIPT_NOINTERRUPT		32
#define SF_SCRIPT_OVERRIDESTATE		64
#define SF_SCRIPT_NOSCRIPTMOVEMENT	128
// SBO 2.0 (Featureful scripted.h)
#define SF_SCRIPT_APPLYNEWANGLES		512		// after the action, take the angles the animation ended with
#define SF_SCRIPT_AUTOSEARCH			1024	// search the radius at map start as if it had no name
#define SF_SCRIPT_TRY_ONCE				4096	// look for the monster once when triggered, not every second
#define SF_SCRIPT_DONT_RESET_HEAD		8192	// a talking monster keeps its head turn
#define SF_SCRIPT_FORCE_IDLE_LOOPING	16384	// loop the idle animation even when the model does not loop it
#define SF_SCRIPT_REMOVE_ON_INTERRUPTION	32768	// removed when its monster breaks out of it
#define SF_SCRIPT_DONT_DROP_TO_FLOOR	65536	// after the action, no drop to the floor
#define SF_SCRIPT_ALLOW_STUCK			131072	// after the action, keep the end position even in a solid
#define SF_SCRIPT_ALLOW_UNNAMED_IDLE	262144	// an unnamed script with an idle animation waits (plays the idle)
#define SF_SCRIPT_ALLOW_TAKEDAMAGE_TRIGGER	524288	// TriggerCondition Take Damage fires during a no-interrupt script

// SBO 2.0 (Featureful): "m_interruptionPolicy", "m_takeDamagePolicy", "m_searchPolicy", "required_state",
// "m_fTurnType", "m_iPriority"
enum { SCRIPT_INTERRUPTION_ANY_DAMAGE = 0, SCRIPT_INTERRUPTION_NONE, SCRIPT_INTERRUPTION_ONLY_DEATH };
enum { SCRIPT_TAKEDAMAGE_NORMAL = 0, SCRIPT_TAKEDAMAGE_INVULNERABLE, SCRIPT_TAKEDAMAGE_NONLETHAL };
enum { SCRIPT_SEARCH_NAME_THEN_CLASS = 0, SCRIPT_SEARCH_NAME_ONLY, SCRIPT_SEARCH_CLASS_ONLY };
enum { SCRIPT_REQUIRED_ANY = 0, SCRIPT_REQUIRED_IDLE, SCRIPT_REQUIRED_COMBAT, SCRIPT_REQUIRED_ALERT,
	   SCRIPT_REQUIRED_NONCOMBAT };
enum { SCRIPT_TURN_MATCH_ANGLE = 0, SCRIPT_TURN_FACE, SCRIPT_TURN_NO };
#define SCRIPT_PRIORITY_OVERRIDE	4	// takes a monster that plays another script

#define SCRIPT_BREAK_CONDITIONS		(bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE)

enum SS_INTERRUPT
{
	SS_INTERRUPT_IDLE = 0,
	SS_INTERRUPT_BY_NAME,
	SS_INTERRUPT_AI,
};

// when a monster finishes an AI scripted sequence, we can choose
// a schedule to place them in. These defines are the aliases to
// resolve worldcraft input to real schedules (sjb)
#define SCRIPT_FINISHSCHED_DEFAULT	0
#define SCRIPT_FINISHSCHED_AMBUSH	1

class CCineMonster : public CBaseMonster
{
public:
	void Spawn( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void Blocked( CBaseEntity *pOther );
	virtual void Touch( CBaseEntity *pOther );
	virtual int	 ObjectCaps( void ) { return (CBaseMonster :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	virtual void Activate( void );
	virtual void UpdateOnRemove( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	// void EXPORT CineSpawnThink( void );
	void EXPORT CineThink( void );
	void Pain( void );
	void Die( void );
	void DelayStart( int state );
	BOOL FindEntity( void );
	virtual void PossessEntity( void );

	void ReleaseEntity( CBaseMonster *pEntity );
	void CancelScript( void );
	virtual BOOL StartSequence( CBaseMonster *pTarget, int iszSeq, BOOL completeOnEmpty );
	virtual BOOL FCanOverrideState ( void );
	void SequenceDone ( CBaseMonster *pMonster );
	virtual void FixScriptMonsterSchedule( CBaseMonster *pMonster );
	BOOL	CanInterrupt( void );
	void	AllowInterrupt( BOOL fAllow );
	int		IgnoreConditions( void );

	int	m_iszIdle;		// string index for idle animation
	int	m_iszPlay;		// string index for scripted animation
	int m_iszEntity;	// entity that is wanted for this script
	int m_fMoveTo;
	int m_iFinishSchedule;
	float m_flRadius;		// range to search
	float m_flRepeat;	// repeat rate

	int m_iDelay;
	float m_startTime;

	int	m_saved_movetype;
	int	m_saved_solid;
	int m_saved_effects;
//	Vector m_vecOrigOrigin;
	BOOL m_interruptable;

	// SBO 2.0 (Featureful scripted_sequence): targets fired when the monster is possessed, after it moved to
	// the mark (Walk / Run) and when the action animation starts; activator = this script.
	// "master" (CBaseToggle::m_sMaster): the script waits while it is locked. Move to 7: teleport to the mark.
	string_t m_iszFireOnPossessed;
	string_t m_iszFireOnBegin;
	string_t m_iszFireOnAnimStart;
	BOOL m_fFiredOnAnimStart;
	void FireOnPossessed( void );
	void FireOnAnimStart( void );

	// SBO 2.0 (Featureful keys): see the enums above; "m_iRepeats" plays the action animation
	// that many more times; "moveto_radius" (Walk / Run) stops within this distance of the mark; "m_applySearchRadius"
	// 1 also a monster found by name must be within the Search Radius
	int m_iInterruptionPolicy;
	int m_iTakeDamagePolicy;
	int m_iSearchPolicy;
	int m_iApplySearchRadius;
	int m_iRequiredState;
	int m_iTurnType;
	int m_iPriority;
	int m_iRepeats;
	int m_iRepeatsLeft;
	float m_flMoveToRadius;
	BOOL AcceptsMonster( CBaseMonster *pTarget, int interruptLevel, BOOL fCheckRadius );
	BOOL ForcedNoInterruptions( void ) { return ( pev->spawnflags & SF_SCRIPT_NOINTERRUPT ) || m_iInterruptionPolicy == SCRIPT_INTERRUPTION_NONE; }
};

#define SCRIPT_MOVE_TELEPORT	7	// SBO 2.0 (Featureful): Move to position 7, teleport


class CCineAI : public CCineMonster
{
	BOOL StartSequence( CBaseMonster *pTarget, int iszSeq, BOOL completeOnEmpty );
	void PossessEntity( void );
	BOOL FCanOverrideState ( void );
	virtual void FixScriptMonsterSchedule( CBaseMonster *pMonster );
};


#endif		//SCRIPTED_H
