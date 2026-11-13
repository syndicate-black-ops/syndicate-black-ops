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

===== doors.cpp ========================================================

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "doors.h"
#include "soundradius.h"	// SBO 2.0 (Featureful): "soundradius"
#include "movewith.h"	// SBO 2.0: momentary_door as a child measures in its parent's frame


extern void SetMovedir(entvars_t* ev);

#define noiseMoving noise1
#define noiseArrived noise2
#define noiseReturnStop noise3	// SBO 2.0 (Featureful): the stop sound on the way back

class CBaseDoor : public CBaseToggle
{
public:
	void Spawn( void );
	void Precache( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void Blocked( CBaseEntity *pOther );
	virtual BOOL ShouldCollide( CBaseEntity *pOther );	// SBO 2.0 (Featureful): "ignore_corpses", "handle_tiny_creatures"
	// SBO 2.1 (Spirit of Half-Life CBaseToggle::GetState): open On, closed Off, opening Turn On, closing Turn Off. A
	// "Starts Open" door swapped its two positions at spawn, so its top is the closed one: the states turn around
	virtual STATE GetState( CBaseEntity *pActivator )
	{
		STATE state = ToggleStateAsState();
		if ( FBitSet( pev->spawnflags, SF_DOOR_START_OPEN ) )
		{
			switch ( state )
			{
			case STATE_ON:			return STATE_OFF;
			case STATE_OFF:			return STATE_ON;
			case STATE_TURN_ON:		return STATE_TURN_OFF;
			default:				return STATE_TURN_ON;
			}
		}
		return state;
	}


	virtual int	ObjectCaps( void ) 
	{ 
		if (pev->spawnflags & SF_ITEM_USE_ONLY)
		{
			// SBO 2.0 (Featureful 49f7d337): "directuse" 1 only when the player looks straight at it, 2 not through walls
			int caps = (CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE;
			if ( m_iDirectUse == PLAYER_USE_POLICY_DIRECT )
				caps |= FCAP_ONLYDIRECT_USE;
			else if ( m_iDirectUse == PLAYER_USE_POLICY_VISIBLE )
				caps |= FCAP_ONLYVISIBLE_USE;
			return caps;
		}
		else
			return (CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION);
	};
	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	virtual void SetToggleState( int state );

	// used to selectivly override defaults
	void EXPORT DoorTouch( CBaseEntity *pOther );

	// local functions
	int DoorActivate( BOOL fActivatedByUse = FALSE );
	void EXPORT DoorGoUp( void );
	void EXPORT DoorGoDown( void );
	void EXPORT DoorHitTop( void );
	void EXPORT DoorHitBottom( void );
	
	BYTE	m_bHealthValue;// some doors are medi-kit doors, they give players health
	
	BYTE	m_bMoveSnd;			// sound a door makes while moving
	BYTE	m_bStopSnd;			// sound a door makes when it stops

	locksound_t m_ls;			// door lock sounds
	
	BYTE	m_bLockedSound;		// ordinals from entity selection
	BYTE	m_bLockedSentence;	
	BYTE	m_bUnlockedSound;	
	BYTE	m_bUnlockedSentence;
	// SBO 2.1 (Featureful CBaseDoor, Half-Life Unified SDK names instead of numbers): a sound file / a sentence group
	// name instead of each numbered locked / unlocked sound and sentence; empty: the number
	string_t m_iszLockedSoundOverride, m_iszUnlockedSoundOverride, m_iszLockedSentenceOverride, m_iszUnlockedSentenceOverride;

	// SBO 2.0, from Featureful (halflife-featureful dlls/doors.cpp CBaseDoor, Half-Life 1 SDK license; Sven Co-op's
	// key): "Obey Trigger Mode" (m_iObeyTriggerMode) for func_door and func_door_rotating. 0 (default): every use type
	// activates the door when it stands still, as in Half-Life. 1: On opens, Off closes (Toggle activates), only when
	// it stands still. 2: also while it moves (On reverses a closing door, Off an opening one; Toggle always reverses).
	short	m_iObeyTriggerMode;

	// SBO 2.0, from Featureful (CBaseDoor, Half-Life 1 SDK license; no map sets these keys): "Fire On" targets when
	// the door starts or stops moving (either way), starts opening / closing, is open / closed, each with its use type
	// (0 Off, 1 On, 2 Toggle); "returnstopsnd" the stop sound on the way back (0: the same as Stop Sound, 255:
	// none); "return_speed" the speed back (0: Speed); "soundradius" how far its sounds carry (soundradius.h).
	// Custom sounds: "noise1" / "noise2" / "noise3" (moving / stop / stop on the way back) name a file instead of the
	// numbered sounds.
	string_t m_iszFireOnStart, m_iszFireOnStop, m_iszFireOnOpening, m_iszFireOnClosing, m_iszFireOnOpened, m_iszFireOnClosed;
	BYTE	m_bFireOnStartState, m_bFireOnStopState, m_bFireOnOpeningState, m_bFireOnClosingState, m_bFireOnOpenedState, m_bFireOnClosedState;
	BYTE	m_bReturnStopSnd;
	float	m_flReturnSpeed;
	short	m_iSoundRadius;

	void	FireOn( string_t iszTarget, BYTE bState );
	void	FireOnMove( BOOL fUp, BOOL fStart );
	float	SoundAttenuation( void ) { return SoundRadiusAttenuation( m_iSoundRadius ); }

	// SBO 2.0, from Featureful (CBaseDoor, Half-Life 1 SDK license; no map sets these keys): "directuse" (49f7d337,
	// with Use Only: 0 as Half-Life, 1 direct use only, 2 not through walls); "ignore_corpses" (30a71721: corpses
	// neither block nor stop the door); "instant_gib_corpses" (30a71721: a corpse that blocks it is gibbed at once);
	// "handle_tiny_creatures" (c1b65f78: 0 as Half-Life, 1 a hornet that blocks it is gibbed, 2 hornets pass through
	// it); "locked_play_on_use" (d44ed23a: triggered while its master is locked, it plays the locked sound);
	// "blocker_recheck" (3975acad: 1 when the crush damage killed the blocker or made it non-solid, the door goes on
	// instead of turning back; 0 / 2 it turns back, as Half-Life).
	short	m_iDirectUse;
	BOOL	m_fIgnoreCorpses;
	BOOL	m_fInstantGibCorpses;
	short	m_iHandleTinyCreatures;
	BOOL	m_fPlayLockedSoundOnUse;
	short	m_iBlockerRecheck;
};


TYPEDESCRIPTION	CBaseDoor::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseDoor, m_bHealthValue, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_bMoveSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_bStopSnd, FIELD_CHARACTER ),
	
	DEFINE_FIELD( CBaseDoor, m_bLockedSound, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_bLockedSentence, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_bUnlockedSound, FIELD_CHARACTER ),	
	DEFINE_FIELD( CBaseDoor, m_bUnlockedSentence, FIELD_CHARACTER ),	
	DEFINE_FIELD( CBaseDoor, m_iszLockedSoundOverride, FIELD_STRING ),	// SBO 2.1
	DEFINE_FIELD( CBaseDoor, m_iszUnlockedSoundOverride, FIELD_STRING ),
	DEFINE_FIELD( CBaseDoor, m_iszLockedSentenceOverride, FIELD_STRING ),
	DEFINE_FIELD( CBaseDoor, m_iszUnlockedSentenceOverride, FIELD_STRING ),
	DEFINE_FIELD( CBaseDoor, m_iObeyTriggerMode, FIELD_SHORT ),
	DEFINE_FIELD( CBaseDoor, m_iDirectUse, FIELD_SHORT ),
	DEFINE_FIELD( CBaseDoor, m_fIgnoreCorpses, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBaseDoor, m_fInstantGibCorpses, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBaseDoor, m_iHandleTinyCreatures, FIELD_SHORT ),
	DEFINE_FIELD( CBaseDoor, m_fPlayLockedSoundOnUse, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBaseDoor, m_iBlockerRecheck, FIELD_SHORT ),

};

IMPLEMENT_SAVERESTORE( CBaseDoor, CBaseToggle );


#define DOOR_SENTENCEWAIT	6
#define DOOR_SOUNDWAIT		3
#define BUTTON_SOUNDWAIT	0.5

// play door or button locked or unlocked sounds. 
// pass in pointer to valid locksound struct. 
// if flocked is true, play 'door is locked' sound,
// otherwise play 'door is unlocked' sound
// NOTE: this routine is shared by doors and buttons

void PlayLockSounds(entvars_t *pev, locksound_t *pls, int flocked, int fbutton)
{
	// LOCKED SOUND
	
	// CONSIDER: consolidate the locksound_t struct (all entries are duplicates for lock/unlock)
	// CONSIDER: and condense this code.
	float flsoundwait;

	if (fbutton)
		flsoundwait = BUTTON_SOUNDWAIT;
	else
		flsoundwait = DOOR_SOUNDWAIT;

	if (flocked)
	{
		int fplaysound = (pls->sLockedSound && gpGlobals->time > pls->flwaitSound);
		int fplaysentence = (pls->sLockedSentence && !pls->bEOFLocked && gpGlobals->time > pls->flwaitSentence);
		float fvol;

		if (fplaysound && fplaysentence)
			fvol = 0.25;
		else
			fvol = 1.0;

		// if there is a locked sound, and we've debounced, play sound
		if (fplaysound)
		{
			// play 'door locked' sound
			EMIT_SOUND(ENT(pev), CHAN_ITEM, (char*)STRING(pls->sLockedSound), fvol, ATTN_NORM);
			pls->flwaitSound = gpGlobals->time + flsoundwait;
		}

		// if there is a sentence, we've not played all in list, and we've debounced, play sound
		if (fplaysentence)
		{
			// play next 'door locked' sentence in group
			int iprev = pls->iLockedSentence;
			
			pls->iLockedSentence = SENTENCEG_PlaySequentialSz(ENT(pev), STRING(pls->sLockedSentence), 
					  0.85, ATTN_NORM, 0, 100, pls->iLockedSentence, FALSE);
			pls->iUnlockedSentence = 0;

			// make sure we don't keep calling last sentence in list
			pls->bEOFLocked = (iprev == pls->iLockedSentence);
		
			pls->flwaitSentence = gpGlobals->time + DOOR_SENTENCEWAIT;
		}
	}
	else
	{
		// UNLOCKED SOUND

		int fplaysound = (pls->sUnlockedSound && gpGlobals->time > pls->flwaitSound);
		int fplaysentence = (pls->sUnlockedSentence && !pls->bEOFUnlocked && gpGlobals->time > pls->flwaitSentence);
		float fvol;

		// if playing both sentence and sound, lower sound volume so we hear sentence
		if (fplaysound && fplaysentence)
			fvol = 0.25;
		else
			fvol = 1.0;

		// play 'door unlocked' sound if set
		if (fplaysound)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, (char*)STRING(pls->sUnlockedSound), fvol, ATTN_NORM);
			pls->flwaitSound = gpGlobals->time + flsoundwait;
		}

		// play next 'door unlocked' sentence in group
		if (fplaysentence)
		{
			int iprev = pls->iUnlockedSentence;
			
			pls->iUnlockedSentence = SENTENCEG_PlaySequentialSz(ENT(pev), STRING(pls->sUnlockedSentence), 
					  0.85, ATTN_NORM, 0, 100, pls->iUnlockedSentence, FALSE);
			pls->iLockedSentence = 0;

			// make sure we don't keep calling last sentence in list
			pls->bEOFUnlocked = (iprev == pls->iUnlockedSentence);
			pls->flwaitSentence = gpGlobals->time + DOOR_SENTENCEWAIT;
		}
	}
}

//
// Cache user-entity-field values until spawn is called.
//

void CBaseDoor::KeyValue( KeyValueData *pkvd )
{

	if (FStrEq(pkvd->szKeyName, "skin"))//skin is used for content type
	{
		pev->skin = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "movesnd"))
	{
		m_bMoveSnd = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "stopsnd"))
	{
		m_bStopSnd = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "healthvalue"))
	{
		m_bHealthValue = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "locked_sound"))
	{
		m_bLockedSound = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "locked_sentence"))
	{
		m_bLockedSentence = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "unlocked_sound"))
	{
		m_bUnlockedSound = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "unlocked_sentence"))
	{
		m_bUnlockedSentence = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	// SBO 2.1 (Featureful)
	else if (FStrEq(pkvd->szKeyName, "locked_sound_override"))	{ m_iszLockedSoundOverride = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "unlocked_sound_override"))	{ m_iszUnlockedSoundOverride = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "locked_sentence_override"))	{ m_iszLockedSentenceOverride = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "unlocked_sentence_override"))	{ m_iszUnlockedSentenceOverride = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "m_iObeyTriggerMode"))	// SBO 2.0 (Featureful)
	{
		m_iObeyTriggerMode = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	// SBO 2.0, from Featureful: Fire On keys, return stop sound and speed, sound radius
	else if (FStrEq(pkvd->szKeyName, "fireonstart"))			{ m_iszFireOnStart = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonstart_triggerstate"))	{ m_bFireOnStartState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonstop"))				{ m_iszFireOnStop = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonstop_triggerstate"))	{ m_bFireOnStopState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonopening"))			{ m_iszFireOnOpening = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonopening_triggerstate"))	{ m_bFireOnOpeningState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosing"))			{ m_iszFireOnClosing = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosing_triggerstate"))	{ m_bFireOnClosingState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonopened"))			{ m_iszFireOnOpened = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonopened_triggerstate"))	{ m_bFireOnOpenedState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosed"))			{ m_iszFireOnClosed = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosed_triggerstate"))	{ m_bFireOnClosedState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "returnstopsnd"))			{ m_bReturnStopSnd = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "return_speed"))			{ m_flReturnSpeed = atof(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "soundradius"))			{ m_iSoundRadius = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "directuse"))				{ m_iDirectUse = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "ignore_corpses"))			{ m_fIgnoreCorpses = atoi(pkvd->szValue) != 0; pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "instant_gib_corpses"))	{ m_fInstantGibCorpses = atoi(pkvd->szValue) != 0; pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "handle_tiny_creatures"))	{ m_iHandleTinyCreatures = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "locked_play_on_use"))		{ m_fPlayLockedSoundOnUse = atoi(pkvd->szValue) != 0; pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "blocker_recheck"))		{ m_iBlockerRecheck = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "WaveHeight"))
	{
		pev->scale = atof(pkvd->szValue) * (1.0/8.0);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

/*QUAKED func_door (0 .5 .8) ? START_OPEN x DOOR_DONT_LINK TOGGLE
if two doors touch, they are assumed to be connected and operate as a unit.

TOGGLE causes the door to wait in both the start and end states for a trigger event.

START_OPEN causes the door to move to its destination when spawned, and operate in reverse.
It is used to temporarily or permanently close off an area when triggered (not usefull for
touch or takedamage doors).

"angle"         determines the opening direction
"targetname"	if set, no touch field will be spawned and a remote button or trigger
				field activates the door.
"health"        if set, door must be shot open
"speed"         movement speed (100 default)
"wait"          wait before returning (3 default, -1 = never return)
"lip"           lip remaining at end of move (8 default)
"dmg"           damage to inflict when blocked (2 default)
"sounds"
0)      no sound
1)      stone
2)      base
3)      stone chain
4)      screechy metal
*/

LINK_ENTITY_TO_CLASS( func_door, CBaseDoor );
//
// func_water - same as a door. 
//
LINK_ENTITY_TO_CLASS( func_water, CBaseDoor );


void CBaseDoor::Spawn( )
{
	Precache();
	SetMovedir (pev);

	if ( pev->skin == 0 )
	{//normal door
		if ( FBitSet (pev->spawnflags, SF_DOOR_PASSABLE) )
			pev->solid		= SOLID_NOT;
		else
			pev->solid		= SOLID_BSP;
	}
	else
	{// special contents
		pev->solid		= SOLID_NOT;
		SetBits( pev->spawnflags, SF_DOOR_SILENT );	// water is silent for now
	}

	pev->movetype	= MOVETYPE_PUSH;
	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL( ENT(pev), STRING(pev->model) );
	
	if (pev->speed == 0)
		pev->speed = 100;
	
	m_vecPosition1	= pev->origin;
	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	m_vecPosition2	= m_vecPosition1 + (pev->movedir * (fabs( pev->movedir.x * (pev->size.x-2) ) + fabs( pev->movedir.y * (pev->size.y-2) ) + fabs( pev->movedir.z * (pev->size.z-2) ) - m_flLip));
	ASSERTSZ(m_vecPosition1 != m_vecPosition2, "door start/end positions are equal");
	if ( FBitSet (pev->spawnflags, SF_DOOR_START_OPEN) )
	{	// swap pos1 and pos2, put door at pos2
		UTIL_SetOrigin(pev, m_vecPosition2);
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = pev->origin;
	}

	m_toggle_state = TS_AT_BOTTOM;
	
	// if the door is flagged for USE button activation only, use NULL touch function
	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) )
	{
		SetTouch ( NULL );
	}
	else // touchable button
		SetTouch( &CBaseDoor::DoorTouch );
}
 

void CBaseDoor :: SetToggleState( int state )
{
	if ( state == TS_AT_TOP )
		UTIL_SetOrigin( pev, m_vecPosition2 );
	else
		UTIL_SetOrigin( pev, m_vecPosition1 );
}


void CBaseDoor::Precache( void )
{
	char *pszSound;

	// SBO 2.0 (Featureful): a sound file the map names in noise1 / noise2 replaces the numbered sound
	string_t iszCustomMove = pev->noiseMoving, iszCustomStop = pev->noiseArrived;

// set the door's "in-motion" sound
	switch (m_bMoveSnd)
	{
	case	0:
		pev->noiseMoving = ALLOC_STRING("common/null.wav");
		break;
	case	1:
		PRECACHE_SOUND ("doors/doormove1.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove1.wav");
		break;
	case	2:
		PRECACHE_SOUND ("doors/doormove2.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove2.wav");
		break;
	case	3:
		PRECACHE_SOUND ("doors/doormove3.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove3.wav");
		break;
	case	4:
		PRECACHE_SOUND ("doors/doormove4.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove4.wav");
		break;
	case	5:
		PRECACHE_SOUND ("doors/doormove5.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove5.wav");
		break;
	case	6:
		PRECACHE_SOUND ("doors/doormove6.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove6.wav");
		break;
	case	7:
		PRECACHE_SOUND ("doors/doormove7.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove7.wav");
		break;
	case	8:
		PRECACHE_SOUND ("doors/doormove8.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove8.wav");
		break;
	case	9:
		PRECACHE_SOUND ("doors/doormove9.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove9.wav");
		break;
	case	10:
		PRECACHE_SOUND ("doors/doormove10.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove10.wav");
		break;
	default:
		pev->noiseMoving = ALLOC_STRING("common/null.wav");
		break;
	}

// set the door's 'reached destination' stop sound
	switch (m_bStopSnd)
	{
	case	0:
		pev->noiseArrived = ALLOC_STRING("common/null.wav");
		break;
	case	1:
		PRECACHE_SOUND ("doors/doorstop1.wav");
		pev->noiseArrived = ALLOC_STRING("doors/doorstop1.wav");
		break;
	case	2:
		PRECACHE_SOUND ("doors/doorstop2.wav");
		pev->noiseArrived = ALLOC_STRING("doors/doorstop2.wav");
		break;
	case	3:
		PRECACHE_SOUND ("doors/doorstop3.wav");
		pev->noiseArrived = ALLOC_STRING("doors/doorstop3.wav");
		break;
	case	4:
		PRECACHE_SOUND ("doors/doorstop4.wav");
		pev->noiseArrived = ALLOC_STRING("doors/doorstop4.wav");
		break;
	case	5:
		PRECACHE_SOUND ("doors/doorstop5.wav");
		pev->noiseArrived = ALLOC_STRING("doors/doorstop5.wav");
		break;
	case	6:
		PRECACHE_SOUND ("doors/doorstop6.wav");
		pev->noiseArrived = ALLOC_STRING("doors/doorstop6.wav");
		break;
	case	7:
		PRECACHE_SOUND ("doors/doorstop7.wav");
		pev->noiseArrived = ALLOC_STRING("doors/doorstop7.wav");
		break;
	case	8:
		PRECACHE_SOUND ("doors/doorstop8.wav");
		pev->noiseArrived = ALLOC_STRING("doors/doorstop8.wav");
		break;
	default:
		pev->noiseArrived = ALLOC_STRING("common/null.wav");
		break;
	}

	// SBO 2.0 (Featureful): custom sounds, and the stop sound on the way back (noise3, or Return Stop Sound 1-8;
	// 255 none; 0 / empty: the Stop Sound)
	if ( !FStringNull( iszCustomMove ) )
	{
		PRECACHE_SOUND( (char *)STRING( iszCustomMove ) );
		pev->noiseMoving = iszCustomMove;
	}
	if ( !FStringNull( iszCustomStop ) )
	{
		PRECACHE_SOUND( (char *)STRING( iszCustomStop ) );
		pev->noiseArrived = iszCustomStop;
	}
	if ( !FStringNull( pev->noiseReturnStop ) )
		PRECACHE_SOUND( (char *)STRING( pev->noiseReturnStop ) );
	else if ( m_bReturnStopSnd >= 1 && m_bReturnStopSnd <= 8 )
	{
		static const char *s_pszStops[] = { "doors/doorstop1.wav", "doors/doorstop2.wav", "doors/doorstop3.wav",
			"doors/doorstop4.wav", "doors/doorstop5.wav", "doors/doorstop6.wav", "doors/doorstop7.wav", "doors/doorstop8.wav" };
		PRECACHE_SOUND( (char *)s_pszStops[ m_bReturnStopSnd - 1 ] );
		pev->noiseReturnStop = MAKE_STRING( s_pszStops[ m_bReturnStopSnd - 1 ] );
	}
	else if ( m_bReturnStopSnd == 255 )
		pev->noiseReturnStop = MAKE_STRING( "common/null.wav" );

	// get door button sounds, for doors which are directly 'touched' to open

	// SBO 2.1 (Featureful): a sound file instead of the number (only the one used is loaded)
	if ( !FStringNull( m_iszLockedSoundOverride ) && STRING( m_iszLockedSoundOverride )[0] )
	{
		PRECACHE_SOUND( (char *)STRING( m_iszLockedSoundOverride ) );
		m_ls.sLockedSound = m_iszLockedSoundOverride;
	}
	else if (m_bLockedSound)
	{
		pszSound = ButtonSound( (int)m_bLockedSound );
		PRECACHE_SOUND(pszSound);
		m_ls.sLockedSound = ALLOC_STRING(pszSound);
	}

	if ( !FStringNull( m_iszUnlockedSoundOverride ) && STRING( m_iszUnlockedSoundOverride )[0] )
	{
		PRECACHE_SOUND( (char *)STRING( m_iszUnlockedSoundOverride ) );
		m_ls.sUnlockedSound = m_iszUnlockedSoundOverride;
	}
	else if (m_bUnlockedSound)
	{
		pszSound = ButtonSound( (int)m_bUnlockedSound );
		PRECACHE_SOUND(pszSound);
		m_ls.sUnlockedSound = ALLOC_STRING(pszSound);
	}

	// get sentence group names, for doors which are directly 'touched' to open

	switch (m_bLockedSentence)
	{
		case 1: m_ls.sLockedSentence = ALLOC_STRING("NA"); break; // access denied
		case 2: m_ls.sLockedSentence = ALLOC_STRING("ND"); break; // security lockout
		case 3: m_ls.sLockedSentence = ALLOC_STRING("NF"); break; // blast door
		case 4: m_ls.sLockedSentence = ALLOC_STRING("NFIRE"); break; // fire door
		case 5: m_ls.sLockedSentence = ALLOC_STRING("NCHEM"); break; // chemical door
		case 6: m_ls.sLockedSentence = ALLOC_STRING("NRAD"); break; // radiation door
		case 7: m_ls.sLockedSentence = ALLOC_STRING("NCON"); break; // gen containment
		case 8: m_ls.sLockedSentence = ALLOC_STRING("NH"); break; // maintenance door
		case 9: m_ls.sLockedSentence = ALLOC_STRING("NG"); break; // broken door
		
		default: m_ls.sLockedSentence = 0; break;
	}

	switch (m_bUnlockedSentence)
	{
		case 1: m_ls.sUnlockedSentence = ALLOC_STRING("EA"); break; // access granted
		case 2: m_ls.sUnlockedSentence = ALLOC_STRING("ED"); break; // security door
		case 3: m_ls.sUnlockedSentence = ALLOC_STRING("EF"); break; // blast door
		case 4: m_ls.sUnlockedSentence = ALLOC_STRING("EFIRE"); break; // fire door
		case 5: m_ls.sUnlockedSentence = ALLOC_STRING("ECHEM"); break; // chemical door
		case 6: m_ls.sUnlockedSentence = ALLOC_STRING("ERAD"); break; // radiation door
		case 7: m_ls.sUnlockedSentence = ALLOC_STRING("ECON"); break; // gen containment
		case 8: m_ls.sUnlockedSentence = ALLOC_STRING("EH"); break; // maintenance door
		
		default: m_ls.sUnlockedSentence = 0; break;
	}

	// SBO 2.1 (Featureful): sentence group names (sentences.txt) instead of the numbered sentences
	if ( !FStringNull( m_iszLockedSentenceOverride ) && STRING( m_iszLockedSentenceOverride )[0] )
		m_ls.sLockedSentence = m_iszLockedSentenceOverride;
	if ( !FStringNull( m_iszUnlockedSentenceOverride ) && STRING( m_iszUnlockedSentenceOverride )[0] )
		m_ls.sUnlockedSentence = m_iszUnlockedSentenceOverride;
}

//
// Doors not tied to anything (e.g. button, another door) can be touched, to make them activate.
//
void CBaseDoor::DoorTouch( CBaseEntity *pOther )
{
	entvars_t*	pevToucher = pOther->pev;
	
	// Ignore touches by anything but players
	if (!FClassnameIs(pevToucher, "player"))
		return;

	// If door has master, and it's not ready to trigger, 
	// play 'locked' sound

	if (m_sMaster && !UTIL_IsMasterTriggered(m_sMaster, pOther))
		PlayLockSounds(pev, &m_ls, TRUE, FALSE);
	
	// If door is somebody's target, then touching does nothing.
	// You have to activate the owner (e.g. button).
	
	if (!FStringNull(pev->targetname))
	{
		// play locked sound
		PlayLockSounds(pev, &m_ls, TRUE, FALSE);
		return; 
	}
	
	m_hActivator = pOther;// remember who activated the door

	if (DoorActivate( ))
		SetTouch( NULL ); // Temporarily disable the touch function, until movement is finished.
}


//
// Used by SUB_UseTargets, when a door is the target of a button.
//
void CBaseDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	// if not ready to be used, ignore "use" command.
	BOOL atEndPosition = m_toggle_state == TS_AT_BOTTOM || ( FBitSet(pev->spawnflags, SF_DOOR_NO_AUTO_RETURN) && m_toggle_state == TS_AT_TOP );
	BOOL shouldActivate;

	// SBO 2.0 (Featureful): "Obey Trigger Mode"
	if ( m_iObeyTriggerMode == 1 )
	{
		shouldActivate = atEndPosition && ( useType == USE_TOGGLE || ( m_toggle_state == TS_AT_BOTTOM && useType == USE_ON )
			|| ( m_toggle_state == TS_AT_TOP && useType == USE_OFF ) );
	}
	else if ( m_iObeyTriggerMode == 2 )
	{
		if ( useType == USE_TOGGLE )
			shouldActivate = TRUE;
		else if ( atEndPosition )
			shouldActivate = ( m_toggle_state == TS_AT_BOTTOM && useType == USE_ON ) || ( m_toggle_state == TS_AT_TOP && useType == USE_OFF );
		else
			shouldActivate = ( m_toggle_state == TS_GOING_DOWN && useType == USE_ON ) || ( m_toggle_state == TS_GOING_UP && useType == USE_OFF );
	}
	else
		shouldActivate = atEndPosition;	// Half-Life

	if ( shouldActivate )
		DoorActivate( TRUE );
}

//
// Causes the door to "do its thing", i.e. start moving, and cascade activation.
//
int CBaseDoor::DoorActivate( BOOL fActivatedByUse )
{
	if (!UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
	{
		// SBO 2.0 (Featureful d44ed23a): "locked_play_on_use"
		if ( fActivatedByUse && m_fPlayLockedSoundOnUse )
			PlayLockSounds(pev, &m_ls, TRUE, FALSE);
		return 0;
	}

	// SBO 2.0 (Featureful): Obey Trigger Mode 2 also closes a "Toggle" door that is opening
	if (FBitSet(pev->spawnflags, SF_DOOR_NO_AUTO_RETURN) && ( m_toggle_state == TS_AT_TOP || ( m_iObeyTriggerMode == 2 && m_toggle_state == TS_GOING_UP ) ))
	{// door should close
		DoorGoDown();
	}
	else
	{// door should open

		if ( m_hActivator != NULL && m_hActivator->IsPlayer() )
		{// give health if player opened the door (medikit)
		// VARS( m_eoActivator )->health += m_bHealthValue;
	
			m_hActivator->TakeHealth( m_bHealthValue, DMG_GENERIC );

		}

		// play door unlock sounds
		PlayLockSounds(pev, &m_ls, FALSE, FALSE);
		
		DoorGoUp();
	}

	return 1;
}

extern Vector VecBModelOrigin( entvars_t* pevBModel );

//
// Starts the door going to its "up" position (simply ToggleData->vecPosition2).
//
void CBaseDoor::DoorGoUp( void )
{
	entvars_t	*pevActivator;

	// It could be going-down, if blocked.
	ASSERT(m_toggle_state == TS_AT_BOTTOM || m_toggle_state == TS_GOING_DOWN);

	// emit door moving and stop sounds on CHAN_STATIC so that the multicast doesn't
	// filter them out and leave a client stuck with looping door sounds!
	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving), 1, SoundAttenuation());

	m_toggle_state = TS_GOING_UP;
	
	SetMoveDone( &CBaseDoor::DoorHitTop );
	if ( FClassnameIs(pev, "func_door_rotating"))		// !!! BUGBUG Triggered doors don't work with this yet
	{
		float	sign = 1.0;

		if ( m_hActivator != NULL )
		{
			pevActivator = m_hActivator->pev;
			
			if ( !FBitSet( pev->spawnflags, SF_DOOR_ONEWAY ) && pev->movedir.y ) 		// Y axis rotation, move away from the player
			{
				Vector vec = pevActivator->origin - pev->origin;
				Vector angles = pevActivator->angles;
				angles.x = 0;
				angles.z = 0;
				UTIL_MakeVectors (angles);
	//			Vector vnext = (pevToucher->origin + (pevToucher->velocity * 10)) - pev->origin;
				UTIL_MakeVectors ( pevActivator->angles );
				Vector vnext = (pevActivator->origin + (gpGlobals->v_forward * 10)) - pev->origin;
				if ( (vec.x*vnext.y - vec.y*vnext.x) < 0 )
					sign = -1.0;
			}
		}
		AngularMove(m_vecAngle2*sign, pev->speed);
	}
	else
		LinearMove(m_vecPosition2, pev->speed);

	FireOnMove( TRUE, TRUE );	// SBO 2.0 (Featureful)
}

// SBO 2.0, from Featureful: the Fire On targets, with the door's activator
void CBaseDoor::FireOn( string_t iszTarget, BYTE bState )
{
	if ( FStringNull( iszTarget ) )
		return;
	USE_TYPE useType = bState == 0 ? USE_OFF : ( bState == 1 ? USE_ON : USE_TOGGLE );
	FireTargets( STRING( iszTarget ), m_hActivator, this, useType, 0 );
}

// fUp: moving to / at position 2 ("up": open, or closed with Starts Open); fStart: starting to move, else stopped
void CBaseDoor::FireOnMove( BOOL fUp, BOOL fStart )
{
	BOOL fOpening = FBitSet( pev->spawnflags, SF_DOOR_START_OPEN ) ? !fUp : fUp;
	if ( fStart )
	{
		FireOn( m_iszFireOnStart, m_bFireOnStartState );
		if ( fOpening )
			FireOn( m_iszFireOnOpening, m_bFireOnOpeningState );
		else
			FireOn( m_iszFireOnClosing, m_bFireOnClosingState );
	}
	else
	{
		FireOn( m_iszFireOnStop, m_bFireOnStopState );
		if ( fOpening )
			FireOn( m_iszFireOnOpened, m_bFireOnOpenedState );
		else
			FireOn( m_iszFireOnClosed, m_bFireOnClosedState );
	}
}


//
// The door has reached the "up" position.  Either go back down, or wait for another activation.
//
void CBaseDoor::DoorHitTop( void )
{
	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
	{
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving) );
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseArrived), 1, SoundAttenuation());
	}

	ASSERT(m_toggle_state == TS_GOING_UP);
	m_toggle_state = TS_AT_TOP;
	
	// toggle-doors don't come down automatically, they wait for refire.
	if (FBitSet(pev->spawnflags, SF_DOOR_NO_AUTO_RETURN))
	{
		// Re-instate touch method, movement is complete
		if ( !FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) )
			SetTouch( &CBaseDoor::DoorTouch );
	}
	else
	{
		// In flWait seconds, DoorGoDown will fire, unless wait is -1, then door stays open
		pev->nextthink = pev->ltime + m_flWait;
		SetThink( &CBaseDoor::DoorGoDown );

		if ( m_flWait == -1 )
		{
			pev->nextthink = -1;
		}
	}

	// Fire the close target (if startopen is set, then "top" is closed) - netname is the close target
	if ( pev->netname && (pev->spawnflags & SF_DOOR_START_OPEN) )
		FireTargets( STRING(pev->netname), m_hActivator, this, USE_TOGGLE, 0 );

	FireOnMove( TRUE, FALSE );	// SBO 2.0 (Featureful)

	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 ); // this isn't finished
}


//
// Starts the door going to its "down" position (simply ToggleData->vecPosition1).
//
void CBaseDoor::DoorGoDown( void )
{
	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving), 1, SoundAttenuation());
	
#ifdef DOOR_ASSERT
	ASSERT(m_toggle_state == TS_AT_TOP);
#endif // DOOR_ASSERT
	m_toggle_state = TS_GOING_DOWN;

	SetMoveDone( &CBaseDoor::DoorHitBottom );
	// SBO 2.0 (Featureful): "return_speed"
	float flSpeed = m_flReturnSpeed > 0 ? m_flReturnSpeed : pev->speed;
	if ( FClassnameIs(pev, "func_door_rotating"))//rotating door
		AngularMove( m_vecAngle1, flSpeed);
	else
		LinearMove( m_vecPosition1, flSpeed);

	FireOnMove( FALSE, TRUE );	// SBO 2.0 (Featureful)
}

//
// The door has reached the "down" position.  Back to quiescence.
//
void CBaseDoor::DoorHitBottom( void )
{
	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
	{
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving) );
		// SBO 2.0 (Featureful): the stop sound on the way back, if set
		string_t iszStop = FStringNull( pev->noiseReturnStop ) ? pev->noiseArrived : pev->noiseReturnStop;
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(iszStop), 1, SoundAttenuation());
	}

	ASSERT(m_toggle_state == TS_GOING_DOWN);
	m_toggle_state = TS_AT_BOTTOM;

	// Re-instate touch method, cycle is complete
	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) )
	{// use only door
		SetTouch ( NULL );
	}
	else // touchable door
		SetTouch( &CBaseDoor::DoorTouch );

	SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 ); // this isn't finished

	// Fire the close target (if startopen is set, then "top" is closed) - netname is the close target
	if ( pev->netname && !(pev->spawnflags & SF_DOOR_START_OPEN) )
		FireTargets( STRING(pev->netname), m_hActivator, this, USE_TOGGLE, 0 );

	FireOnMove( FALSE, FALSE );	// SBO 2.0 (Featureful)
}

// SBO 2.0 (Featureful CBaseDoor::ShouldCollide, 30a71721 / c1b65f78): through the engine's pfnShouldCollide
// (DispatchShouldCollide, cbase.cpp)
BOOL CBaseDoor::ShouldCollide( CBaseEntity *pOther )
{
	if ( m_fIgnoreCorpses && pOther->IsCorpse() )
		return FALSE;
	if ( m_iHandleTinyCreatures == HANDLE_TINY_CREATURES_DONTCOLLIDE && pOther->IsTinyCreature() )
		return FALSE;
	return TRUE;
}

void CBaseDoor::Blocked( CBaseEntity *pOther )
{
	edict_t	*pentTarget = NULL;
	CBaseDoor	*pDoor		= NULL;


	// Hurt the blocker a little.
	// SBO 2.0 (Featureful 30a71721 / c1b65f78 / 3975acad): "instant_gib_corpses", "handle_tiny_creatures" 1 (the
	// blocker takes its health plus one and is gibbed) and "blocker_recheck" 1
	BOOL fInstaGib = ( m_fInstantGibCorpses && pOther->IsCorpse() )
		|| ( m_iHandleTinyCreatures == HANDLE_TINY_CREATURES_CRUSH && pOther->IsTinyCreature() );
	if ( pev->dmg || fInstaGib )
	{
		// TakeDamage can free the blocker at once (a TakeDamage hook that removes it with REMOVE_ENTITY): the handle
		// checks the edict's serial number and private data, so a blocker freed there comes back NULL and the door
		// goes on, as for a blocker that is being removed
		EHANDLE hOther;
		hOther = pOther;
		if ( fInstaGib )
			pOther->TakeDamage( pev, pev, pOther->pev->health + 1, DMG_CRUSH | DMG_ALWAYSGIB );
		else
			pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );
		if ( hOther == NULL )
			return;

		// the blocker became non-solid or is being removed: the door goes on (Featureful tests the door's own
		// FL_KILLME there, which a door never has; SBO tests the blocker's)
		if ( m_iBlockerRecheck == 1 && ( pOther->pev->solid == SOLID_NOT || FBitSet( pOther->pev->flags, FL_KILLME ) ) )
			return;
	}

	// SBO 2.0 (Featureful bdd3e74e / f43c39cb): a satchel charge or an armed trip mine under the door goes off; the
	// door kept turning back from it, so one charge jammed it for good
	if ( pOther->HandleDoorBlockage( this ) )
		return;

	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast

	if (m_flWait >= 0)
	{
		if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
			STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving) );

		if (m_toggle_state == TS_GOING_DOWN)
		{
			DoorGoUp();
		}
		else
		{
			DoorGoDown();
		}
	}

	// Block all door pieces with the same targetname here.
	if ( !FStringNull ( pev->targetname ) )
	{
		for (;;)
		{
			pentTarget = FIND_ENTITY_BY_TARGETNAME(pentTarget, STRING(pev->targetname));

			if ( VARS( pentTarget ) != pev )
			{
				if (FNullEnt(pentTarget))
					break;

				if ( FClassnameIs ( pentTarget, "func_door" ) || FClassnameIs ( pentTarget, "func_door_rotating" ) )
				{
				
					pDoor = GetClassPtr( (CBaseDoor *) VARS(pentTarget) );

					if ( pDoor->m_flWait >= 0)
					{
						if (pDoor->pev->velocity == pev->velocity && pDoor->pev->avelocity == pev->velocity)
						{
							// this is the most hacked, evil, bastardized thing I've ever seen. kjb
							if ( FClassnameIs ( pentTarget, "func_door" ) )
							{// set origin to realign normal doors
								pDoor->pev->origin = pev->origin;
								pDoor->pev->velocity = g_vecZero;// stop!
							}
							else
							{// set angles to realign rotating doors
								pDoor->pev->angles = pev->angles;
								pDoor->pev->avelocity = g_vecZero;
							}
						}

						if ( pDoor->m_toggle_state == TS_GOING_DOWN)
							pDoor->DoorGoUp();
						else
							pDoor->DoorGoDown();
					}
				}
			}
		}
	}
}


/*QUAKED FuncRotDoorSpawn (0 .5 .8) ? START_OPEN REVERSE  
DOOR_DONT_LINK TOGGLE X_AXIS Y_AXIS
if two doors touch, they are assumed to be connected and operate as  
a unit.

TOGGLE causes the door to wait in both the start and end states for  
a trigger event.

START_OPEN causes the door to move to its destination when spawned,  
and operate in reverse.  It is used to temporarily or permanently  
close off an area when triggered (not usefull for touch or  
takedamage doors).

You need to have an origin brush as part of this entity.  The  
center of that brush will be
the point around which it is rotated. It will rotate around the Z  
axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"distance" is how many degrees the door will be rotated.
"speed" determines how fast the door moves; default value is 100.

REVERSE will cause the door to rotate in the opposite direction.

"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote  
button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"dmg"		damage to inflict when blocked (2 default)
"sounds"
0)	no sound
1)	stone
2)	base
3)	stone chain
4)	screechy metal
*/
class CRotDoor : public CBaseDoor
{
public:
	void Spawn( void );
	virtual void SetToggleState( int state );
};

LINK_ENTITY_TO_CLASS( func_door_rotating, CRotDoor );


void CRotDoor::Spawn( void )
{
	Precache();
	// set the axis of rotation
	CBaseToggle::AxisDir( pev );

	// check for clockwise rotation
	if ( FBitSet (pev->spawnflags, SF_DOOR_ROTATE_BACKWARDS) )
		pev->movedir = pev->movedir * -1;
	
	//m_flWait			= 2; who the hell did this? (sjb)
	m_vecAngle1	= pev->angles;
	m_vecAngle2	= pev->angles + pev->movedir * m_flMoveDistance;

	ASSERTSZ(m_vecAngle1 != m_vecAngle2, "rotating door start/end positions are equal");
	
	if ( FBitSet (pev->spawnflags, SF_DOOR_PASSABLE) )
		pev->solid		= SOLID_NOT;
	else
		pev->solid		= SOLID_BSP;

	pev->movetype	= MOVETYPE_PUSH;
	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), STRING(pev->model) );

	if (pev->speed == 0)
		pev->speed = 100;
	
// DOOR_START_OPEN is to allow an entity to be lighted in the closed position
// but spawn in the open position
	if ( FBitSet (pev->spawnflags, SF_DOOR_START_OPEN) )
	{	// swap pos1 and pos2, put door at pos2, invert movement direction
		pev->angles = m_vecAngle2;
		// SBO 2.0 (Featureful 638a8e70, door_rotating_starts_open_fix): save the open angle, not the closed one;
		// Half-Life left both angles at the closed position, so the door closed once and never reopened
		Vector vecSav = m_vecAngle2;
		m_vecAngle2 = m_vecAngle1;
		m_vecAngle1 = vecSav;
		pev->movedir = pev->movedir * -1;
	}

	m_toggle_state = TS_AT_BOTTOM;

	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) )
	{
		SetTouch ( NULL );
	}
	else // touchable button
		SetTouch( &CRotDoor::DoorTouch );
}


void CRotDoor :: SetToggleState( int state )
{
	if ( state == TS_AT_TOP )
		pev->angles = m_vecAngle2;
	else
		pev->angles = m_vecAngle1;

	UTIL_SetOrigin( pev, pev->origin );
}


class CMomentaryDoor : public CBaseToggle
{
public:
	void	Spawn( void );
	void Precache( void );

	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return CBaseToggle :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	// SBO 2.0 (Featureful 7af3f63b): a satchel charge or an armed trip mine under it goes off
	void	Blocked( CBaseEntity *pOther ) { pOther->HandleDoorBlockage( this ); }

	// SBO 2.1 (Spirit of Half-Life: On while it moves): opening Turn On, closing Turn Off; at rest On when open at all,
	// Off when closed. "Starts Open" swapped the two positions at spawn: there 0 is open and 1 closed. A child of a
	// moving parent (movewith) moves when its own movement on the parent does, not when the parent carries it.
	virtual STATE GetState( CBaseEntity *pActivator )
	{
		const BOOL fStartOpen = FBitSet( pev->spawnflags, SF_DOOR_START_OPEN ) != 0;
		if ( MoveWith_GetOwnVelocity( this ) != g_vecZero )
			return ( m_fLastDirection != 0 ) != fStartOpen ? STATE_TURN_ON : STATE_TURN_OFF;
		const float flOpen = fStartOpen ? 1.0f - m_fLastPos : m_fLastPos;
		return flOpen > 0 ? STATE_ON : STATE_OFF;
	}

	// halflife-updated (halflife #3265): stop the move sound once the door has arrived
	void EXPORT DoorMoveDone( void );
	void EXPORT StopMoveSound( void );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BYTE	m_bMoveSnd;			// sound a door makes while moving	

	// SBO 2.0, from Featureful (CMomentaryDoor, Half-Life 1 SDK license): "stopsnd" the stop sound (Half-Life read
	// the key and ignored it), the "Fire On" targets of func_door (start / stop moving, start opening / closing,
	// fully open / closed, each with its use type; the activator is the one who moved it last) and "soundradius".
	BYTE	m_bStopSnd;
	float	m_fLastPos;			// the last position asked for, 0 (closed) .. 1 (open)
	BOOL	m_fLastDirection;	// TRUE: the last move was an opening one
	short	m_iSoundRadius;
	string_t m_iszFireOnStart, m_iszFireOnStop, m_iszFireOnOpening, m_iszFireOnClosing, m_iszFireOnOpened, m_iszFireOnClosed;
	BYTE	m_bFireOnStartState, m_bFireOnStopState, m_bFireOnOpeningState, m_bFireOnClosingState, m_bFireOnOpenedState, m_bFireOnClosedState;

	void	FireOn( string_t iszTarget, BYTE bState );
	float	SoundAttenuation( void ) { return SoundRadiusAttenuation( m_iSoundRadius ); }
};

LINK_ENTITY_TO_CLASS( momentary_door, CMomentaryDoor );

TYPEDESCRIPTION	CMomentaryDoor::m_SaveData[] = 
{
	DEFINE_FIELD( CMomentaryDoor, m_bMoveSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CMomentaryDoor, m_bStopSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CMomentaryDoor, m_fLastPos, FIELD_FLOAT ),
	DEFINE_FIELD( CMomentaryDoor, m_fLastDirection, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMomentaryDoor, m_iSoundRadius, FIELD_SHORT ),
	DEFINE_FIELD( CMomentaryDoor, m_iszFireOnStart, FIELD_STRING ),
	DEFINE_FIELD( CMomentaryDoor, m_iszFireOnStop, FIELD_STRING ),
	DEFINE_FIELD( CMomentaryDoor, m_iszFireOnOpening, FIELD_STRING ),
	DEFINE_FIELD( CMomentaryDoor, m_iszFireOnClosing, FIELD_STRING ),
	DEFINE_FIELD( CMomentaryDoor, m_iszFireOnOpened, FIELD_STRING ),
	DEFINE_FIELD( CMomentaryDoor, m_iszFireOnClosed, FIELD_STRING ),
	DEFINE_FIELD( CMomentaryDoor, m_bFireOnStartState, FIELD_CHARACTER ),
	DEFINE_FIELD( CMomentaryDoor, m_bFireOnStopState, FIELD_CHARACTER ),
	DEFINE_FIELD( CMomentaryDoor, m_bFireOnOpeningState, FIELD_CHARACTER ),
	DEFINE_FIELD( CMomentaryDoor, m_bFireOnClosingState, FIELD_CHARACTER ),
	DEFINE_FIELD( CMomentaryDoor, m_bFireOnOpenedState, FIELD_CHARACTER ),
	DEFINE_FIELD( CMomentaryDoor, m_bFireOnClosedState, FIELD_CHARACTER ),
};

IMPLEMENT_SAVERESTORE( CMomentaryDoor, CBaseToggle );

void CMomentaryDoor::Spawn( void )
{
	SetMovedir (pev);

	pev->solid		= SOLID_BSP;
	pev->movetype	= MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL( ENT(pev), STRING(pev->model) );
	
	if (pev->speed == 0)
		pev->speed = 100;
	if (pev->dmg == 0)
		pev->dmg = 2;
	
	m_vecPosition1	= pev->origin;
	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	m_vecPosition2	= m_vecPosition1 + (pev->movedir * (fabs( pev->movedir.x * (pev->size.x-2) ) + fabs( pev->movedir.y * (pev->size.y-2) ) + fabs( pev->movedir.z * (pev->size.z-2) ) - m_flLip));
	ASSERTSZ(m_vecPosition1 != m_vecPosition2, "door start/end positions are equal");

	if ( FBitSet (pev->spawnflags, SF_DOOR_START_OPEN) )
	{	// swap pos1 and pos2, put door at pos2
		UTIL_SetOrigin(pev, m_vecPosition2);
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = pev->origin;
	}
	SetTouch( NULL );
	
	Precache();
}
	
void CMomentaryDoor::Precache( void )
{

// set the door's "in-motion" sound
	// SBO 2.1 (Featureful func_door "noise1" / "noise2", Half-Life Unified SDK names instead of numbers): "noise1" /
	// "noise2" sound files instead of the numbered "movesnd" / "stopsnd"; empty: the numbers
	if ( !FStringNull( pev->noiseMoving ) && STRING( pev->noiseMoving )[0] )
		PRECACHE_SOUND( (char *)STRING( pev->noiseMoving ) );
	else
	switch (m_bMoveSnd)
	{
	case	0:
		pev->noiseMoving = ALLOC_STRING("common/null.wav");
		break;
	case	1:
		PRECACHE_SOUND ("doors/doormove1.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove1.wav");
		break;
	case	2:
		PRECACHE_SOUND ("doors/doormove2.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove2.wav");
		break;
	case	3:
		PRECACHE_SOUND ("doors/doormove3.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove3.wav");
		break;
	case	4:
		PRECACHE_SOUND ("doors/doormove4.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove4.wav");
		break;
	case	5:
		PRECACHE_SOUND ("doors/doormove5.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove5.wav");
		break;
	case	6:
		PRECACHE_SOUND ("doors/doormove6.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove6.wav");
		break;
	case	7:
		PRECACHE_SOUND ("doors/doormove7.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove7.wav");
		break;
	case	8:
		PRECACHE_SOUND ("doors/doormove8.wav");
		pev->noiseMoving = ALLOC_STRING("doors/doormove8.wav");
		break;
	default:
		pev->noiseMoving = ALLOC_STRING("common/null.wav");
		break;
	}

	// SBO 2.0, from Featureful: the stop sound ("stopsnd"; 0: none, as before)
	static const char *s_stopSounds[] = { "doors/doorstop1.wav", "doors/doorstop2.wav", "doors/doorstop3.wav",
		"doors/doorstop4.wav", "doors/doorstop5.wav", "doors/doorstop6.wav", "doors/doorstop7.wav", "doors/doorstop8.wav" };
	if ( !FStringNull( pev->noiseArrived ) && STRING( pev->noiseArrived )[0] )
		PRECACHE_SOUND( (char *)STRING( pev->noiseArrived ) );
	else if ( m_bStopSnd >= 1 && m_bStopSnd <= 8 )
	{
		PRECACHE_SOUND( (char *)s_stopSounds[ m_bStopSnd - 1 ] );
		pev->noiseArrived = MAKE_STRING( s_stopSounds[ m_bStopSnd - 1 ] );
	}
}

// SBO 2.0, from Featureful: the Fire On targets, with the activator of the last move (func_door's use types)
void CMomentaryDoor::FireOn( string_t iszTarget, BYTE bState )
{
	if ( FStringNull( iszTarget ) )
		return;
	USE_TYPE useType = bState == 0 ? USE_OFF : ( bState == 1 ? USE_ON : USE_TOGGLE );
	FireTargets( STRING( iszTarget ), m_hActivator, this, useType, 0 );
}

void CMomentaryDoor::KeyValue( KeyValueData *pkvd )
{

	if (FStrEq(pkvd->szKeyName, "movesnd"))
	{
		m_bMoveSnd = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "stopsnd"))
	{
		m_bStopSnd = atoi(pkvd->szValue);	// SBO 2.0, from Featureful (Half-Life ignored it)
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "healthvalue"))
	{
//		m_bHealthValue = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	// SBO 2.0, from Featureful: Fire On targets and sound radius
	else if (FStrEq(pkvd->szKeyName, "fireonstart"))			{ m_iszFireOnStart = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonstart_triggerstate"))	{ m_bFireOnStartState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonstop"))			{ m_iszFireOnStop = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonstop_triggerstate"))	{ m_bFireOnStopState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonopening"))			{ m_iszFireOnOpening = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonopening_triggerstate"))	{ m_bFireOnOpeningState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosing"))			{ m_iszFireOnClosing = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosing_triggerstate"))	{ m_bFireOnClosingState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonopened"))			{ m_iszFireOnOpened = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonopened_triggerstate"))	{ m_bFireOnOpenedState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosed"))			{ m_iszFireOnClosed = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosed_triggerstate"))	{ m_bFireOnClosedState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "soundradius"))			{ m_iSoundRadius = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else
		CBaseToggle::KeyValue( pkvd );
}

void CMomentaryDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType != USE_SET )		// Momentary buttons will pass down a float in here
		return;

	m_hActivator = pActivator;	// SBO 2.0, from Featureful: the activator of the Fire On targets

	if ( value > 1.0 )
		value = 1.0;
	if ( value < 0.0 )
		value = 0.0;

	// SBO 2.0, from Featureful: a move in the other direction than the last one fires Fire On Start and Fire On
	// Open Start / Close Start
	BOOL fOpening = value > m_fLastPos;
	if ( value != m_fLastPos && fOpening != m_fLastDirection )
	{
		m_fLastDirection = fOpening;
		FireOn( m_iszFireOnStart, m_bFireOnStartState );
		if ( fOpening )
			FireOn( m_iszFireOnOpening, m_bFireOnOpeningState );
		else
			FireOn( m_iszFireOnClosing, m_bFireOnClosingState );
	}

	Vector move = m_vecPosition1 + (value * (m_vecPosition2 - m_vecPosition1));
	
	Vector vecOrigin = pev->origin;
	MoveWith_GetOwnPose( this, &vecOrigin, NULL );	// SBO 2.0 movewith: a child's own pose
	Vector delta = move - vecOrigin;
	float speed = delta.Length() * 10;

	if ( speed != 0 )
	{
		// This entity only thinks when it moves, so if it's thinking, it's in the process of moving
		// play the sound when it starts moving
		if ( pev->nextthink < pev->ltime || pev->nextthink == 0 )
			EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving), 1, SoundAttenuation());
		// halflife-updated (halflife #3265): If we already moving to designated point, return
		else if ( move == m_vecFinalDest )
			return;

		m_fLastPos = value;
		SetMoveDone( &CMomentaryDoor::DoorMoveDone );
		LinearMove( move, speed );
	}

}

//
// halflife-updated (halflife #3265): The door has reached needed position.
//
void CMomentaryDoor::DoorMoveDone( void )
{
	// Stop sounds at the next think, rather than here as another
	// Use call might immediately follow the end of this move
	// This think function will be replaced by LinearMove if that happens.
	SetThink( &CMomentaryDoor::StopMoveSound );
	pev->nextthink = pev->ltime + 0.1;

	// SBO 2.0, from Featureful: Fire On Stop, and Open End / Close End at the ends
	FireOn( m_iszFireOnStop, m_bFireOnStopState );
	if ( m_fLastPos >= 1.0 )
		FireOn( m_iszFireOnOpened, m_bFireOnOpenedState );
	else if ( m_fLastPos <= 0.0 )
		FireOn( m_iszFireOnClosed, m_bFireOnClosedState );
}

void CMomentaryDoor::StopMoveSound( void )
{
	STOP_SOUND( ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving) );
	if ( !FStringNull( pev->noiseArrived ) )	// "stopsnd" (SBO 2.0); none by default
		EMIT_SOUND( ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseArrived), 1, SoundAttenuation() );
	SetThink( NULL );
}
