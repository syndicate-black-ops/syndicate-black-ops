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

===== plats.cpp ========================================================

  spawn, think, and touch functions for trains, etc

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trains.h"
#include "saverestore.h"
#include "soundradius.h"	// SBO 2.0 (Featureful): "soundradius"

static void PlatSpawnInsideTrigger(entvars_t* pevPlatform);

#define SF_PLAT_TOGGLE		0x0001

class CBasePlatTrain : public CBaseToggle
{
public:
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void KeyValue( KeyValueData* pkvd);
	void Precache( void );

	// This is done to fix spawn flag collisions between this class and a derived class
	virtual BOOL IsTogglePlat( void ) { return (pev->spawnflags & SF_PLAT_TOGGLE) ? TRUE : FALSE; }

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BYTE	m_bMoveSnd;			// sound a plat makes while moving
	BYTE	m_bStopSnd;			// sound a plat makes when it stops
	float	m_volume;			// Sound volume

	// SBO 2.0, from Featureful (CBasePlatTrain, Half-Life 1 SDK license; no map sets these keys): "custommovesnd" /
	// "customstopsnd" sound files instead of the numbered sounds, "soundradius" (soundradius.h), "Fire On Start" /
	// "Fire On Stop" with their use types (0 Off, 1 On, 2 Toggle) when it starts or stops moving
	string_t m_iszCustomMoveSnd, m_iszCustomStopSnd;
	short	m_iSoundRadius;
	string_t m_iszFireOnStart, m_iszFireOnStop;
	BYTE	m_bFireOnStartState, m_bFireOnStopState;
	float	SoundAttenuation( void ) { return SoundRadiusAttenuation( m_iSoundRadius ); }
	void	FireOn( string_t iszTarget, BYTE bState )
	{
		if ( !FStringNull( iszTarget ) )
			FireTargets( STRING( iszTarget ), m_hActivator, this, bState == 0 ? USE_OFF : ( bState == 1 ? USE_ON : USE_TOGGLE ), 0 );
	}
	void	OnStartMoving( void ) { FireOn( m_iszFireOnStart, m_bFireOnStartState ); }
	void	OnStopMoving( void ) { FireOn( m_iszFireOnStop, m_bFireOnStopState ); }
};

TYPEDESCRIPTION	CBasePlatTrain::m_SaveData[] = 
{
	DEFINE_FIELD( CBasePlatTrain, m_bMoveSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CBasePlatTrain, m_bStopSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CBasePlatTrain, m_volume, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CBasePlatTrain, CBaseToggle );

void CBasePlatTrain :: KeyValue( KeyValueData *pkvd )
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
	else if (FStrEq(pkvd->szKeyName, "height"))
	{
		m_flHeight = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "rotation"))
	{
		m_vecFinalAngle.x = atof(pkvd->szValue);
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
	// SBO 2.0 (Featureful)
	else if (FStrEq(pkvd->szKeyName, "custommovesnd"))		{ m_iszCustomMoveSnd = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "customstopsnd"))		{ m_iszCustomStopSnd = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "soundradius"))		{ m_iSoundRadius = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonstart"))		{ m_iszFireOnStart = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonstart_triggerstate"))	{ m_bFireOnStartState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonstop"))			{ m_iszFireOnStop = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonstop_triggerstate"))	{ m_bFireOnStopState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "volume"))
	{
		m_volume = atof(pkvd->szValue);
		// SBO 2.0: "nan" / "inf" read as unset (0.85): a non-finite volume reached EMIT_SOUND (the engine turns
		// it into its volume byte: undefined for NaN)
		if ( !( m_volume > -1e30 && m_volume < 1e30 ) )
		{
			ALERT( at_console, "%s \"%s\": volume \"%s\" is not a number, using the default\n", STRING( pev->classname ), STRING( pev->targetname ), pkvd->szValue );
			m_volume = 0;
		}
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

#define noiseMoving noise
#define noiseArrived noise1

void CBasePlatTrain::Precache( void )
{
// set the plat's "in-motion" sound
	switch (m_bMoveSnd)
	{
	case	0:
		pev->noiseMoving = MAKE_STRING("common/null.wav");
		break;
	case	1:
		PRECACHE_SOUND ("plats/bigmove1.wav");
		pev->noiseMoving = MAKE_STRING("plats/bigmove1.wav");
		break;
	case	2:
		PRECACHE_SOUND ("plats/bigmove2.wav");
		pev->noiseMoving = MAKE_STRING("plats/bigmove2.wav");
		break;
	case	3:
		PRECACHE_SOUND ("plats/elevmove1.wav");
		pev->noiseMoving = MAKE_STRING("plats/elevmove1.wav");
		break;
	case	4:
		PRECACHE_SOUND ("plats/elevmove2.wav");
		pev->noiseMoving = MAKE_STRING("plats/elevmove2.wav");
		break;
	case	5:
		PRECACHE_SOUND ("plats/elevmove3.wav");
		pev->noiseMoving = MAKE_STRING("plats/elevmove3.wav");
		break;
	case	6:
		PRECACHE_SOUND ("plats/freightmove1.wav");
		pev->noiseMoving = MAKE_STRING("plats/freightmove1.wav");
		break;
	case	7:
		PRECACHE_SOUND ("plats/freightmove2.wav");
		pev->noiseMoving = MAKE_STRING("plats/freightmove2.wav");
		break;
	case	8:
		PRECACHE_SOUND ("plats/heavymove1.wav");
		pev->noiseMoving = MAKE_STRING("plats/heavymove1.wav");
		break;
	case	9:
		PRECACHE_SOUND ("plats/rackmove1.wav");
		pev->noiseMoving = MAKE_STRING("plats/rackmove1.wav");
		break;
	case	10:
		PRECACHE_SOUND ("plats/railmove1.wav");
		pev->noiseMoving = MAKE_STRING("plats/railmove1.wav");
		break;
	case	11:
		PRECACHE_SOUND ("plats/squeekmove1.wav");
		pev->noiseMoving = MAKE_STRING("plats/squeekmove1.wav");
		break;
	case	12:
		PRECACHE_SOUND ("plats/talkmove1.wav");
		pev->noiseMoving = MAKE_STRING("plats/talkmove1.wav");
		break;
	case	13:
		PRECACHE_SOUND ("plats/talkmove2.wav");
		pev->noiseMoving = MAKE_STRING("plats/talkmove2.wav");
		break;
	default:
		pev->noiseMoving = MAKE_STRING("common/null.wav");
		break;
	}

// set the plat's 'reached destination' stop sound
	switch (m_bStopSnd)
	{
	case	0:
		pev->noiseArrived = MAKE_STRING("common/null.wav");
		break;
	case	1:
		PRECACHE_SOUND ("plats/bigstop1.wav");
		pev->noiseArrived = MAKE_STRING("plats/bigstop1.wav");
		break;
	case	2:
		PRECACHE_SOUND ("plats/bigstop2.wav");
		pev->noiseArrived = MAKE_STRING("plats/bigstop2.wav");
		break;
	case	3:
		PRECACHE_SOUND ("plats/freightstop1.wav");
		pev->noiseArrived = MAKE_STRING("plats/freightstop1.wav");
		break;
	case	4:
		PRECACHE_SOUND ("plats/heavystop2.wav");
		pev->noiseArrived = MAKE_STRING("plats/heavystop2.wav");
		break;
	case	5:
		PRECACHE_SOUND ("plats/rackstop1.wav");
		pev->noiseArrived = MAKE_STRING("plats/rackstop1.wav");
		break;
	case	6:
		PRECACHE_SOUND ("plats/railstop1.wav");
		pev->noiseArrived = MAKE_STRING("plats/railstop1.wav");
		break;
	case	7:
		PRECACHE_SOUND ("plats/squeekstop1.wav");
		pev->noiseArrived = MAKE_STRING("plats/squeekstop1.wav");
		break;
	case	8:
		PRECACHE_SOUND ("plats/talkstop1.wav");
		pev->noiseArrived = MAKE_STRING("plats/talkstop1.wav");
		break;

	default:
		pev->noiseArrived = MAKE_STRING("common/null.wav");
		break;
	}

	// SBO 2.0 (Featureful): custom sound files replace the numbered sounds
	if ( !FStringNull( m_iszCustomMoveSnd ) )
	{
		PRECACHE_SOUND( (char *)STRING( m_iszCustomMoveSnd ) );
		pev->noiseMoving = m_iszCustomMoveSnd;
	}
	if ( !FStringNull( m_iszCustomStopSnd ) )
	{
		PRECACHE_SOUND( (char *)STRING( m_iszCustomStopSnd ) );
		pev->noiseArrived = m_iszCustomStopSnd;
	}
}

//
//====================== PLAT code ====================================================
//


#define noiseMovement noise
#define noiseStopMoving noise1

class CFuncPlat : public CBasePlatTrain
{
public:
	void Spawn( void );
	void Precache( void );
	void Setup( void );

	virtual void Blocked( CBaseEntity *pOther );


	void EXPORT PlatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	void	EXPORT CallGoDown( void ) { GoDown(); }
	void	EXPORT CallHitTop( void  ) { HitTop(); }
	void	EXPORT CallHitBottom( void ) { HitBottom(); }

	virtual void GoUp( void );
	virtual void GoDown( void );
	virtual void HitTop( void );
	virtual void HitBottom( void );
	void KeyValue( KeyValueData *pkvd );

	// SBO 2.1 (Spirit of Half-Life CBaseToggle::GetState): at the top On, at the bottom Off, going up Turn On, going
	// down Turn Off (func_platrot, func_trackchange and func_trackautochange too)
	virtual STATE GetState( CBaseEntity *pActivator ) { return ToggleStateAsState(); }

	// SBO 2.0, from Featureful (CFuncPlat): Fire On targets when it starts opening (going up) / closing (going down)
	// and when it is open (at the top) / closed (at the bottom)
	string_t m_iszFireOnOpening, m_iszFireOnClosing, m_iszFireOnOpened, m_iszFireOnClosed;
	BYTE	m_bFireOnOpeningState, m_bFireOnClosingState, m_bFireOnOpenedState, m_bFireOnClosedState;
};
LINK_ENTITY_TO_CLASS( func_plat, CFuncPlat );

void CFuncPlat :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "fireonopening"))				{ m_iszFireOnOpening = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonopening_triggerstate"))	{ m_bFireOnOpeningState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosing"))			{ m_iszFireOnClosing = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosing_triggerstate"))	{ m_bFireOnClosingState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonopened"))			{ m_iszFireOnOpened = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonopened_triggerstate"))	{ m_bFireOnOpenedState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosed"))			{ m_iszFireOnClosed = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "fireonclosed_triggerstate"))	{ m_bFireOnClosedState = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else
		CBasePlatTrain::KeyValue( pkvd );
}


// UNDONE: Need to save this!!! It needs class & linkage
class CPlatTrigger : public CBaseEntity
{
public:
	virtual int	ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; }
	void SpawnInsideTrigger( CFuncPlat *pPlatform );
	void Touch( CBaseEntity *pOther );
	EHANDLE m_hPlatform;	// halflife-updated (halflife #1725): was a raw pointer, read after the platform was removed
};



/*QUAKED func_plat (0 .5 .8) ? PLAT_LOW_TRIGGER
speed	default 150

Plats are always drawn in the extended position, so they will light correctly.

If the plat is the target of another trigger or button, it will start out disabled in
the extended position until it is trigger, when it will lower and become a normal plat.

If the "height" key is set, that will determine the amount the plat moves, instead of
being implicitly determined by the model's height.

Set "sounds" to one of the following:
1) base fast
2) chain slow
*/

void CFuncPlat :: Setup( void )
{
	//pev->noiseMovement = MAKE_STRING("plats/platmove1.wav");
	//pev->noiseStopMoving = MAKE_STRING("plats/platstop1.wav");

	if (m_flTLength == 0)
		m_flTLength = 80;
	if (m_flTWidth == 0)
		m_flTWidth = 10;
	
	pev->angles		= g_vecZero;

	pev->solid		= SOLID_BSP;
	pev->movetype	= MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);		// set size and link into world
	UTIL_SetSize(pev, pev->mins, pev->maxs);
	SET_MODEL(ENT(pev), STRING(pev->model) );

	// vecPosition1 is the top position, vecPosition2 is the bottom
	m_vecPosition1 = pev->origin;
	m_vecPosition2 = pev->origin;
	if (m_flHeight != 0)
		m_vecPosition2.z = pev->origin.z - m_flHeight;
	else
		m_vecPosition2.z = pev->origin.z - pev->size.z + 8;
	if (pev->speed == 0)
		pev->speed = 150;

	if ( m_volume == 0 )
		m_volume = 0.85;
}


void CFuncPlat :: Precache( )
{
	CBasePlatTrain::Precache();
	//PRECACHE_SOUND("plats/platmove1.wav");
	//PRECACHE_SOUND("plats/platstop1.wav");
	if ( !IsTogglePlat() )
		PlatSpawnInsideTrigger( pev );		// the "start moving" trigger
}


void CFuncPlat :: Spawn( )
{
	Setup();

	Precache();

	// If this platform is the target of some button, it starts at the TOP position,
	// and is brought down by that button.  Otherwise, it starts at BOTTOM.
	if ( !FStringNull(pev->targetname) )
	{
		UTIL_SetOrigin (pev, m_vecPosition1);
		m_toggle_state = TS_AT_TOP;
		SetUse( &CFuncPlat::PlatUse );
	}
	else
	{
		UTIL_SetOrigin (pev, m_vecPosition2);
		m_toggle_state = TS_AT_BOTTOM;
	}
}



static void PlatSpawnInsideTrigger(entvars_t* pevPlatform)
{
	GetClassPtr( (CPlatTrigger *)NULL)->SpawnInsideTrigger( GetClassPtr( (CFuncPlat *)pevPlatform ) );
}
		

//
// Create a trigger entity for a platform.
//
void CPlatTrigger :: SpawnInsideTrigger( CFuncPlat *pPlatform )
{
	m_hPlatform = pPlatform;
	// Create trigger entity, "point" it at the owning platform, give it a touch method
	pev->solid		= SOLID_TRIGGER;
	pev->movetype	= MOVETYPE_NONE;
	pev->origin = pPlatform->pev->origin;

	// Establish the trigger field's size
	Vector vecTMin = pPlatform->pev->mins + Vector ( 25 , 25 , 0 );
	Vector vecTMax = pPlatform->pev->maxs + Vector ( 25 , 25 , 8 );
	vecTMin.z = vecTMax.z - ( pPlatform->m_vecPosition1.z - pPlatform->m_vecPosition2.z + 8 );
	if (pPlatform->pev->size.x <= 50)
	{
		vecTMin.x = (pPlatform->pev->mins.x + pPlatform->pev->maxs.x) / 2;
		vecTMax.x = vecTMin.x + 1;
	}
	if (pPlatform->pev->size.y <= 50)
	{
		vecTMin.y = (pPlatform->pev->mins.y + pPlatform->pev->maxs.y) / 2;
		vecTMax.y = vecTMin.y + 1;
	}
	UTIL_SetSize ( pev, vecTMin, vecTMax );
}


//
// When the platform's trigger field is touched, the platform ???
//
void CPlatTrigger :: Touch( CBaseEntity *pOther )
{
	// Ignore touches by non-players
	entvars_t*	pevToucher = pOther->pev;
	if ( !FClassnameIs (pevToucher, "player") )
		return;

	// halflife-updated (halflife #1725): Platform was removed, remove trigger
	if ( m_hPlatform == NULL || !m_hPlatform->pev )
	{
		UTIL_Remove( this );
		return;
	}

	// Ignore touches by corpses
	if (!pOther->IsAlive())
		return;
	
	CFuncPlat *pPlatform = (CFuncPlat *)(CBaseEntity *)m_hPlatform;

	// Make linked platform go up/down.
	if (pPlatform->m_toggle_state == TS_AT_BOTTOM)
		pPlatform->GoUp();
	else if (pPlatform->m_toggle_state == TS_AT_TOP)
		pPlatform->pev->nextthink = pPlatform->pev->ltime + 1;// delay going down
}


//
// Used by SUB_UseTargets, when a platform is the target of a button.
// Start bringing platform down.
//
void CFuncPlat :: PlatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( IsTogglePlat() )
	{
		// Top is off, bottom is on
		BOOL on = (m_toggle_state == TS_AT_BOTTOM) ? TRUE : FALSE;

		if ( !ShouldToggle( useType, on ) )
			return;

		if (m_toggle_state == TS_AT_TOP)
			GoDown();
		else if ( m_toggle_state == TS_AT_BOTTOM )
			GoUp();
	}
	else
	{
		SetUse( NULL );

		if (m_toggle_state == TS_AT_TOP)
			GoDown();
	}
}


//
// Platform is at top, now starts moving down.
//
void CFuncPlat :: GoDown( void )
{
	if(pev->noiseMovement)
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMovement), m_volume, SoundAttenuation());

	ASSERT(m_toggle_state == TS_AT_TOP || m_toggle_state == TS_GOING_UP);
	m_toggle_state = TS_GOING_DOWN;
	SetMoveDone(&CFuncPlat::CallHitBottom);
	LinearMove(m_vecPosition2, pev->speed);

	OnStartMoving();	// SBO 2.0 (Featureful)
	FireOn( m_iszFireOnClosing, m_bFireOnClosingState );
}


//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncPlat :: HitBottom( void )
{
	if(pev->noiseMovement)
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMovement));

	if (pev->noiseStopMoving)
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, (char*)STRING(pev->noiseStopMoving), m_volume, SoundAttenuation());

	ASSERT(m_toggle_state == TS_GOING_DOWN);
	m_toggle_state = TS_AT_BOTTOM;

	OnStopMoving();	// SBO 2.0 (Featureful)
	FireOn( m_iszFireOnClosed, m_bFireOnClosedState );
}


//
// Platform is at bottom, now starts moving up
//
void CFuncPlat :: GoUp( void )
{
	if (pev->noiseMovement)
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMovement), m_volume, SoundAttenuation());
	
	ASSERT(m_toggle_state == TS_AT_BOTTOM || m_toggle_state == TS_GOING_DOWN);
	m_toggle_state = TS_GOING_UP;
	SetMoveDone(&CFuncPlat::CallHitTop);
	LinearMove(m_vecPosition1, pev->speed);

	OnStartMoving();	// SBO 2.0 (Featureful)
	FireOn( m_iszFireOnOpening, m_bFireOnOpeningState );
}


//
// Platform has hit top.  Pauses, then starts back down again.
//
void CFuncPlat :: HitTop( void )
{
	if(pev->noiseMovement)
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMovement));

	if (pev->noiseStopMoving)
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, (char*)STRING(pev->noiseStopMoving), m_volume, SoundAttenuation());
	
	ASSERT(m_toggle_state == TS_GOING_UP);
	m_toggle_state = TS_AT_TOP;

	OnStopMoving();	// SBO 2.0 (Featureful)
	FireOn( m_iszFireOnOpened, m_bFireOnOpenedState );

	if ( !IsTogglePlat() )
	{
		// After a delay, the platform will automatically start going down again.
		SetThink( &CFuncPlat::CallGoDown );
		pev->nextthink = pev->ltime + 3;
	}
}


void CFuncPlat :: Blocked( CBaseEntity *pOther )
{
	ALERT( at_aiconsole, "%s Blocked by %s\n", STRING(pev->classname), STRING(pOther->pev->classname) );
	// Hurt the blocker a little
	pOther->TakeDamage(pev, pev, 1, DMG_CRUSH);

	if(pev->noiseMovement)
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMovement));
	
	// Send the platform back where it came from
	ASSERT(m_toggle_state == TS_GOING_UP || m_toggle_state == TS_GOING_DOWN);
	if (m_toggle_state == TS_GOING_UP)
		GoDown();
	else if (m_toggle_state == TS_GOING_DOWN)
		GoUp ();
}


class CFuncPlatRot : public CFuncPlat
{
public:
	void Spawn( void );
	void SetupRotation( void );

	virtual void	GoUp( void );
	virtual void	GoDown( void );
	virtual void	HitTop( void );
	virtual void	HitBottom( void );
	
	void			RotMove( Vector &destAngle, float time );
	virtual BOOL	MoverTurnEndsAtStop( void ) { return TRUE; }	// SBO 2.0 mover prediction: HitTop / HitBottom stop the turn
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	Vector	m_end, m_start;
};
LINK_ENTITY_TO_CLASS( func_platrot, CFuncPlatRot );
TYPEDESCRIPTION	CFuncPlatRot::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncPlatRot, m_end, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncPlatRot, m_start, FIELD_VECTOR ),
};

IMPLEMENT_SAVERESTORE( CFuncPlatRot, CFuncPlat );


void CFuncPlatRot :: SetupRotation( void )
{
	if ( m_vecFinalAngle.x != 0 )		// This plat rotates too!
	{
		CBaseToggle :: AxisDir( pev );
		m_start	= pev->angles;
		m_end = pev->angles + pev->movedir * m_vecFinalAngle.x;
	}
	else
	{
		m_start = g_vecZero;
		m_end = g_vecZero;
	}
	if ( !FStringNull(pev->targetname) )	// Start at top
	{
		pev->angles = m_end;
	}
}


void CFuncPlatRot :: Spawn( void )
{
	CFuncPlat :: Spawn();
	SetupRotation();
}

void CFuncPlatRot :: GoDown( void )
{
	CFuncPlat :: GoDown();
	RotMove( m_start, pev->nextthink - pev->ltime );
}


//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncPlatRot :: HitBottom( void )
{
	CFuncPlat :: HitBottom();
	pev->avelocity = g_vecZero;
	pev->angles = m_start;
}


//
// Platform is at bottom, now starts moving up
//
void CFuncPlatRot :: GoUp( void )
{
	CFuncPlat :: GoUp();
	RotMove( m_end, pev->nextthink - pev->ltime );
}


//
// Platform has hit top.  Pauses, then starts back down again.
//
void CFuncPlatRot :: HitTop( void )
{
	CFuncPlat :: HitTop();
	pev->avelocity = g_vecZero;
	pev->angles = m_end;
}


void CFuncPlatRot :: RotMove( Vector &destAngle, float time )
{
	// set destdelta to the vector needed to move
	Vector vecDestDelta = destAngle - pev->angles;

	// Travel time is so short, we're practically there already;  so make it so.
	if ( time >= 0.1)
		pev->avelocity = vecDestDelta / time;
	else
	{
		pev->avelocity = vecDestDelta;
		pev->nextthink = pev->ltime + 1;
	}
}


//
//====================== TRAIN code ==================================================
//

// SBO 2.0, from Featureful (no map sets them): func_train flags 4 "Initially On" (a named train starts moving at
// once) and 64 "No default damage" (Damage 0 stays 0; Half-Life makes it 2)
#define SF_TRAIN_START_ON		4
#define SF_TRAIN_NO_DAMAGE		64

class CFuncTrain : public CBasePlatTrain
{
public:
	void Spawn( void );
	void Precache( void );
	void Activate( void );
	void OverrideReset( void );

	void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );


	void EXPORT Wait( void );
	virtual void EXPORT Next( void );	// SBO 2.0: virtual for env_modeltrain
	// SBO 2.1 (Spirit of Half-Life): On while it moves, Off while it stands (stopped, or waiting at a path_corner)
	virtual STATE GetState( CBaseEntity *pActivator ) { return pev->velocity != g_vecZero ? STATE_ON : STATE_OFF; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	// SBO 2.0, from Featureful (CFuncTrain): hooks for env_spritetrain / env_modeltrain, which reuse this path code.
	// For func_train they do what the code did before.
	virtual void TrainThink( void ) {}	// called every 0.1 s by the train's "trainthinker"
	virtual void OnStop( void );		// the train stops: move sound off, stop sound
	virtual BOOL StartsOn( void ) { return FBitSet( pev->spawnflags, SF_TRAIN_START_ON ) ? TRUE : FALSE; }	// "Initially On" (4)
	Vector DestinationVector( entvars_t *pevTarg ) { return pevTarg->origin - ( pev->mins + pev->maxs ) * 0.5; }
	void SetDefaultTrainValues( void );

	entvars_t	*m_pevCurrentTarget;
	int			m_sounds;
	BOOL		m_activated;

	// SBO 2.0, from Featureful (halflife-featureful dlls/plats.cpp CFuncTrain, Half-Life 1 SDK license):
	// "Obey Trigger Mode" 1: On starts the train, Off stops it, Toggle toggles. 0 (default): every use type toggles,
	// as in Half-Life.
	BOOL		m_iObeyTriggerMode;
	int			m_iPitch;	// SBO 2.0, from Featureful: "soundpitch" of the move sound (0: normal)

	// SBO 2.0, as Spirit of Half-Life's path_corner "turnspeed": the yaw rate (degrees a second) the last path_corner
	// with a "New Train rot. Speed" (yaw_speed, not 0) set; the train turns at it while it moves. 0: no corner set one,
	// avelocity is left as it is (Half-Life)
	float		m_flTurnSpeed;
	void		StopTurning( void ) { if ( m_flTurnSpeed != 0 ) pev->avelocity.y = 0; }

	// SBO 2.0 mover prediction (docs/MOVER_PREDICTION.md): the next segment's velocity, worked out once per segment in
	// Next() (not saved: Next() sets it again)
	BOOL MoverNextVelocity( Vector &vecVelocity );
	BOOL MoverTurnEndsAtStop( void ) { return m_flTurnSpeed != 0 && !m_fMoverNextVelocity; }	// OnStop -> StopTurning
	void MoverPredictNext( CBaseEntity *pTarg, const Vector &vecDest );
	Vector		m_vecMoverNextVelocity;
	BOOL		m_fMoverNextVelocity;
};


LINK_ENTITY_TO_CLASS( func_train, CFuncTrain );
TYPEDESCRIPTION	CFuncTrain::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTrain, m_sounds, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTrain, m_pevCurrentTarget, FIELD_EVARS ),
	DEFINE_FIELD( CFuncTrain, m_activated, FIELD_BOOLEAN ),
	DEFINE_FIELD( CFuncTrain, m_iObeyTriggerMode, FIELD_BOOLEAN ),
	DEFINE_FIELD( CFuncTrain, m_flTurnSpeed, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CFuncTrain, CBasePlatTrain );


void CFuncTrain :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "sounds"))
	{
		m_sounds = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iObeyTriggerMode"))	// SBO 2.0 (Featureful)
	{
		m_iObeyTriggerMode = atoi(pkvd->szValue) != 0;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "soundpitch"))	// SBO 2.0 (Featureful)
	{
		m_iPitch = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBasePlatTrain::KeyValue( pkvd );
}


void CFuncTrain :: Blocked( CBaseEntity *pOther )

{
	if ( gpGlobals->time < m_flActivateFinished)
		return;

	m_flActivateFinished = gpGlobals->time + 0.5;
	
	pOther->TakeDamage(pev, pev, pev->dmg, DMG_CRUSH);
}


void CFuncTrain :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SBO 2.0 (Featureful): "Obey Trigger Mode": On when moving, Off when stopped do nothing
	if ( m_iObeyTriggerMode && !ShouldToggle( useType, !FBitSet( pev->spawnflags, SF_TRAIN_WAIT_RETRIGGER ) ) )
		return;

	if ( pev->spawnflags & SF_TRAIN_WAIT_RETRIGGER )
	{
		// Move toward my target
		pev->spawnflags &= ~SF_TRAIN_WAIT_RETRIGGER;
		Next();
	}
	else
	{
		pev->spawnflags |= SF_TRAIN_WAIT_RETRIGGER;
		// Pop back to last target if it's available
		if ( pev->enemy )
			pev->target = pev->enemy->v.targetname;
		pev->nextthink = 0;
		pev->velocity = g_vecZero;
		// SBO 2.0 (Featureful ac445351): the move loop stops with the train, as at a path_corner (Wait)
		OnStop();
	}
}


void CFuncTrain :: Wait( void )
{
	// Fire the pass target if there is one
	if ( m_pevCurrentTarget->message )
	{
		FireTargets( STRING(m_pevCurrentTarget->message), this, this, USE_TOGGLE, 0 );
		if ( FBitSet( m_pevCurrentTarget->spawnflags, SF_CORNER_FIREONCE ) )
			m_pevCurrentTarget->message = 0;
	}
		
	// need pointer to LAST target.
	if ( FBitSet (m_pevCurrentTarget->spawnflags , SF_TRAIN_WAIT_RETRIGGER ) || ( pev->spawnflags & SF_TRAIN_WAIT_RETRIGGER ) )
    {
		pev->spawnflags |= SF_TRAIN_WAIT_RETRIGGER;
        // clear the sound channel.
		OnStop();
		pev->nextthink = 0;
        return;
    }
    
    // ALERT ( at_console, "%f\n", m_flWait );

	if (m_flWait != 0)
	{// -1 wait will wait forever!		
		pev->nextthink = pev->ltime + m_flWait;
		OnStop();
		SetThink( &CFuncTrain::Next );
	}
	else
	{
		Next();// do it RIGHT now!
	}
}


//
// Train next - path corner needs to change to next target 
//
void CFuncTrain :: Next( void )
{
	CBaseEntity	*pTarg;

	
	// now find our next target
	pTarg = GetNextTarget();

    if ( !pTarg )
	{
		OnStop();	// SBO 2.0: move sound off, stop sound (as before)
		return;
	}

	// Save last target in case we need to find it again
	pev->message = pev->target;

	pev->target = pTarg->pev->target;
	m_flWait = pTarg->GetDelay();

	if ( m_pevCurrentTarget && m_pevCurrentTarget->speed != 0 )
	{// don't copy speed from target if it is 0 (uninitialized)
        pev->speed = m_pevCurrentTarget->speed;
		ALERT( at_aiconsole, "Train %s speed to %4.2f\n", STRING(pev->targetname), pev->speed );
	}
	// SBO 2.0 (Spirit of Half-Life path_corner "turnspeed"): "New Train rot. Speed" sets the turn rate as "New Train
	// Speed" sets the speed; 0 leaves it as it is
	if ( m_pevCurrentTarget && m_pevCurrentTarget->yaw_speed != 0 )
		m_flTurnSpeed = m_pevCurrentTarget->yaw_speed;
	m_pevCurrentTarget = pTarg->pev;// keep track of this since path corners change our target for us.

    pev->enemy = pTarg->edict();//hack

	if(FBitSet(m_pevCurrentTarget->spawnflags, SF_CORNER_TELEPORT))
	{
		// Path corner has indicated a teleport to the next corner.
		SetBits(pev->effects, EF_NOINTERP);
		UTIL_SetOrigin(pev, DestinationVector(pTarg->pev));
		Wait(); // Get on with doing the next path corner.
	}
	else
	{
		// Normal linear move.
		
		// CHANGED this from CHAN_VOICE to CHAN_STATIC around OEM beta time because trains should
		// use CHAN_STATIC for their movement sounds to prevent sound field problems.
		// this is not a hack or temporary fix, this is how things should be. (sjb).
		if ( pev->noiseMovement )
			STOP_SOUND( edict(), CHAN_STATIC, (char*)STRING(pev->noiseMovement) );
		if ( pev->noiseMovement )	// SBO 2.0 (Featureful): sound radius and "soundpitch"
			EMIT_SOUND_DYN( ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMovement), m_volume, SoundAttenuation(), 0, m_iPitch > 0 ? m_iPitch : PITCH_NORM );
		ClearBits(pev->effects, EF_NOINTERP);
		SetMoveDone( &CFuncTrain::Wait );
		if ( m_flTurnSpeed != 0 )	// SBO 2.0 (Spirit): turns while it moves
			pev->avelocity.y = m_flTurnSpeed;
		// SBO 2.0 mover prediction: before LinearMove: an already reached corner runs Wait() / Next() again from
		// inside it, which sets its own
		MoverPredictNext( pTarg, DestinationVector(pTarg->pev) );
		LinearMove (DestinationVector(pTarg->pev), pev->speed);
		OnStartMoving();	// SBO 2.0 (Featureful)
	}
}

/*
=================
CFuncTrain :: MoverPredictNext

SBO 2.0 mover prediction: what Wait() and Next() will do when the train reaches pTarg (at vecDest). If it goes straight
on to the next corner, that segment's velocity is sent to the clients (endpos in AddToFullPack's state) so they don't
stop the train at the corner. A wait, a retrigger corner, a teleport corner or the end of the path is a stop.
=================
*/
void CFuncTrain :: MoverPredictNext( CBaseEntity *pTarg, const Vector &vecDest )
{
	m_fMoverNextVelocity = FALSE;

	if ( m_flWait != 0 || FBitSet( pTarg->pev->spawnflags, SF_TRAIN_WAIT_RETRIGGER ) || FBitSet( pev->spawnflags, SF_TRAIN_WAIT_RETRIGGER ) )
		return;

	if ( FStringNull( pTarg->pev->target ) )
		return;

	edict_t *pentNext = FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pTarg->pev->target ) );
	if ( FNullEnt( pentNext ) || FBitSet( pentNext->v.spawnflags, SF_CORNER_TELEPORT ) )
		return;

	// Next() copies the corner's speed when it is set
	float flSpeed = ( pTarg->pev->speed != 0 ) ? pTarg->pev->speed : pev->speed;
	Vector vecDelta = pentNext->v.origin - (pev->mins + pev->maxs) * 0.5 - vecDest;
	float flLength = vecDelta.Length();
	if ( flLength <= 0 || flSpeed <= 0 )
		return;

	m_vecMoverNextVelocity = vecDelta * ( flSpeed / flLength );
	m_fMoverNextVelocity = TRUE;
}

BOOL CFuncTrain :: MoverNextVelocity( Vector &vecVelocity )
{
	if ( !m_fMoverNextVelocity || m_pfnCallWhenMoveDone != static_cast <void (CBaseToggle::*)(void)> ( &CFuncTrain::Wait ) )
		return FALSE;

	vecVelocity = m_vecMoverNextVelocity;
	return TRUE;
}


void CFuncTrain :: Activate( void )
{
	// Not yet active, so teleport to first target
	if ( !m_activated )
	{
		m_activated = TRUE;
		edict_t		*pentTarg = FIND_ENTITY_BY_TARGETNAME (NULL, STRING(pev->target) );
		if ( FNullEnt( pentTarg ) )
		{
			// SBO 2.0: no first stop (a missing or empty "target") read through a NULL pointer
			ALERT( at_console, "%s \"%s\": no path_corner \"%s\"\n", STRING(pev->classname), STRING(pev->targetname), STRING(pev->target) );
			return;
		}
		entvars_t	*pevTarg = VARS( pentTarg );
		
		pev->target = pevTarg->target;
		m_pevCurrentTarget = pevTarg;// keep track of this since path corners change our target for us.
    
		UTIL_SetOrigin	(pev, DestinationVector(pevTarg) );
		
		if ( FStringNull(pev->targetname) || StartsOn() )	// SBO 2.0: StartsOn, env_*train "Initially On"
		{	// not triggered, so start immediately
			pev->nextthink = pev->ltime + 0.1;
			SetThink( &CFuncTrain::Next );
		}
		else
			pev->spawnflags |= SF_TRAIN_WAIT_RETRIGGER;
	}
}

/*QUAKED func_train (0 .5 .8) ?
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
sounds
1) ratchet metal
*/

void CFuncTrain :: Spawn( void )
{
	Precache();
	if (pev->speed == 0)
		pev->speed = 100;
	
	if ( FStringNull(pev->target) )
		ALERT(at_console, "FuncTrain with no target");
	
	if (pev->dmg == 0 && !FBitSet( pev->spawnflags, SF_TRAIN_NO_DAMAGE ))	// SBO 2.0 (Featureful): flag 64
		pev->dmg = 2;
	
	pev->movetype = MOVETYPE_PUSH;
	
	if ( FBitSet (pev->spawnflags, SF_TRACKTRAIN_PASSABLE) )
		pev->solid		= SOLID_NOT;
	else
		pev->solid		= SOLID_BSP;

	SET_MODEL( ENT(pev), STRING(pev->model) );
	UTIL_SetSize (pev, pev->mins, pev->maxs);
	UTIL_SetOrigin(pev, pev->origin);

	m_activated = FALSE;

	if ( m_volume == 0 )
		m_volume = 0.85;
}


void CFuncTrain::Precache( void )
{
	CBasePlatTrain::Precache();

#if 0  // obsolete
	// otherwise use preset sound
	switch (m_sounds)
	{
	case 0:
		pev->noise = 0;
		pev->noise1 = 0;
		break;

	case 1:
		PRECACHE_SOUND ("plats/train2.wav");
		PRECACHE_SOUND ("plats/train1.wav");
		pev->noise = MAKE_STRING("plats/train2.wav");
		pev->noise1 = MAKE_STRING("plats/train1.wav");
		break;

	case 2:
		PRECACHE_SOUND ("plats/platmove1.wav");
		PRECACHE_SOUND ("plats/platstop1.wav");
		pev->noise = MAKE_STRING("plats/platstop1.wav");
		pev->noise1 = MAKE_STRING("plats/platmove1.wav");
		break;
	}
#endif
}


void CFuncTrain::OverrideReset( void )
{
	CBaseEntity	*pTarg;

	// Are we moving?
	if ( pev->velocity != g_vecZero && pev->nextthink != 0 )
	{
		pev->target = pev->message;
		// now find our next target
		pTarg = GetNextTarget();
		if ( !pTarg )
		{
			pev->nextthink = 0;
			pev->velocity = g_vecZero;
		}
		else	// Keep moving for 0.1 secs, then find path_corner again and restart
		{
			SetThink( &CFuncTrain::Next );
			pev->nextthink = pev->ltime + 0.1;
		}
	}
}


// SBO 2.0: the train stops (Use off, a path_corner wait, the end of the path): move sound off, stop sound. One
// place for what Use, Wait and Next each did.
void CFuncTrain::OnStop( void )
{
	StopTurning();	// SBO 2.0 (Spirit): a stopped train does not turn
	if ( pev->noiseMovement )
		STOP_SOUND( edict(), CHAN_STATIC, (char*)STRING(pev->noiseMovement) );
	if ( pev->noiseStopMoving )
		EMIT_SOUND (ENT(pev), CHAN_VOICE, (char*)STRING(pev->noiseStopMoving), m_volume, SoundAttenuation());
	OnStopMoving();	// SBO 2.0 (Featureful)
}

// SBO 2.0, from Featureful (CFuncTrain::SetDefaultTrainValues): the defaults env_spritetrain / env_modeltrain share
// with func_train's Spawn
void CFuncTrain::SetDefaultTrainValues( void )
{
	if ( pev->speed == 0 )
		pev->speed = 100;
	if ( FStringNull( pev->target ) )
		ALERT( at_console, "%s with no target\n", STRING( pev->classname ) );
	if ( pev->dmg == 0 )
		pev->dmg = 2;
	m_activated = FALSE;
	if ( m_volume == 0 )
		m_volume = 0.85;
}

// SBO 2.0, from Featureful (halflife-featureful dlls/plats.cpp CTrainThinker, CSpriteTrain, CModelTrain, Half-Life 1
// SDK license): env_spritetrain and env_modeltrain, a sprite or a studio model that runs along path_corners with
// func_train's path code (speed, wait, pass targets, teleport corners, "Obey Trigger Mode"). The train's own think
// drives the path (MOVETYPE_PUSH, ltime), so a "trainthinker" helper calls TrainThink every 0.1 s: sprite frames,
// model animation. Save/restore left out (multiplayer).
#define SF_SPRITETRAIN_START_ON		4	// "Initially On"

class CTrainThinker : public CBaseEntity
{
public:
	void Spawn( void )
	{
		pev->effects = EF_NODRAW;
		pev->nextthink = gpGlobals->time;
		if ( pev->owner )
			SetThink( &CTrainThinker::TrainThinkerThink );
		else
			SetThink( &CBaseEntity::SUB_Remove );
	}
	void EXPORT TrainThinkerThink( void )
	{
		CBaseEntity *pOwner = pev->owner ? CBaseEntity::Instance( pev->owner ) : NULL;
		if ( pOwner && ( FClassnameIs( pOwner->pev, "env_spritetrain" ) || FClassnameIs( pOwner->pev, "env_modeltrain" ) ) )
		{
			( (CFuncTrain *)pOwner )->TrainThink();
			pev->nextthink = gpGlobals->time + 0.1;
		}
		else
		{
			SetThink( &CBaseEntity::SUB_Remove );	// the train is gone
			pev->nextthink = gpGlobals->time + 0.1;
		}
	}
};

LINK_ENTITY_TO_CLASS( trainthinker, CTrainThinker );

class CSpriteTrain : public CFuncTrain
{
public:
	void Spawn( void );
	void Precache( void );
	void TrainThink( void );
	BOOL StartsOn( void ) { return FBitSet( pev->spawnflags, SF_SPRITETRAIN_START_ON ) ? TRUE : FALSE; }

	float m_maxFrame;
	float m_lastTime;
};

LINK_ENTITY_TO_CLASS( env_spritetrain, CSpriteTrain );

void CSpriteTrain::Spawn( void )
{
	Precache();
	SetDefaultTrainValues();

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_PUSH;
	pev->effects = 0;

	if ( FStringNull( pev->model ) )
		ALERT( at_console, "Spawning %s without model!\n", STRING( pev->classname ) );
	else
		SET_MODEL( ENT( pev ), STRING( pev->model ) );

	// Opposing Force: no render mode is Additive, no render amount is 255, a negative one starts invisible
	if ( !pev->rendermode )
		pev->rendermode = kRenderTransAdd;
	if ( !pev->renderamt )
		pev->renderamt = 255;
	else if ( pev->renderamt < 0 )
		pev->renderamt = 0;
	m_maxFrame = (float)MODEL_FRAMES( pev->modelindex ) - 1;
	m_lastTime = gpGlobals->time;
	pev->frame = 0;

	UTIL_SetOrigin( pev, pev->origin );

	Create( "trainthinker", pev->origin, pev->angles, edict() );
}

void CSpriteTrain::Precache( void )
{
	if ( !FStringNull( pev->model ) )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
	if ( !FStringNull( pev->noiseMovement ) )
		PRECACHE_SOUND( (char *)STRING( pev->noiseMovement ) );
	if ( !FStringNull( pev->noiseStopMoving ) )
		PRECACHE_SOUND( (char *)STRING( pev->noiseStopMoving ) );
}

void CSpriteTrain::TrainThink( void )
{
	if ( pev->framerate && m_maxFrame > 1.0 )
	{
		pev->frame = fmod( pev->frame + pev->framerate * ( gpGlobals->time - m_lastTime ), m_maxFrame );
		m_lastTime = gpGlobals->time;
	}
}

class CModelTrain : public CFuncTrain
{
public:
	enum
	{
		TURN_INSTANT = 0,
		TURN_INSTANT_YAW_ONLY,
		TURN_NO,
	};

	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Next( void );
	void TrainThink( void );
	void OnStop( void );
	BOOL StartsOn( void ) { return FBitSet( pev->spawnflags, SF_SPRITETRAIN_START_ON ) ? TRUE : FALSE; }
	int GetIdleSequence( void );
	void AdvanceAnimation( void );

	string_t m_idleSequence;
	string_t m_moveSequence;
	float m_idleFramerate;
	float m_moveFramerate;
	int m_turnPolicy;
};

LINK_ENTITY_TO_CLASS( env_modeltrain, CModelTrain );

void CModelTrain::Spawn( void )
{
	Precache();
	SetDefaultTrainValues();

	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BBOX;

	if ( FStringNull( pev->model ) )
		ALERT( at_console, "Spawning %s without model!\n", STRING( pev->classname ) );
	else
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );

	pev->sequence = GetIdleSequence();
	ResetSequenceInfo();
	if ( m_idleFramerate )
		pev->framerate = m_idleFramerate;

	Create( "trainthinker", pev->origin, pev->angles, edict() );
}

void CModelTrain::Precache( void )
{
	if ( !FStringNull( pev->model ) )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
	if ( !FStringNull( pev->noiseMovement ) )
		PRECACHE_SOUND( (char *)STRING( pev->noiseMovement ) );
	if ( !FStringNull( pev->noiseStopMoving ) )
		PRECACHE_SOUND( (char *)STRING( pev->noiseStopMoving ) );
}

void CModelTrain::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "idle_sequence" ) )
	{
		m_idleSequence = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "move_sequence" ) )
	{
		m_moveSequence = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "idle_framerate" ) )
	{
		m_idleFramerate = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "move_framerate" ) )
	{
		m_moveFramerate = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "turn_policy" ) )
	{
		m_turnPolicy = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CFuncTrain::KeyValue( pkvd );
}

void CModelTrain::AdvanceAnimation( void )
{
	float flInterval = StudioFrameAdvance();
	DispatchAnimEvents( flInterval );

	// every sequence loops
	if ( m_fSequenceFinished )
	{
		pev->frame = fmod( pev->frame, 255.0f );
		ResetSequenceInfo();
		m_fSequenceFinished = TRUE;

		if ( pev->velocity == g_vecZero )
		{
			if ( m_idleFramerate )
				pev->framerate = m_idleFramerate;
		}
		else
		{
			if ( m_moveFramerate )
				pev->framerate = m_moveFramerate;
		}
	}
}

void CModelTrain::Next( void )
{
	CFuncTrain::Next();

	if ( m_pevCurrentTarget )
	{
		const Vector diff = m_pevCurrentTarget->origin - pev->origin;
		if ( diff != g_vecZero )
		{
			switch ( m_turnPolicy )
			{
			case TURN_INSTANT:
				pev->angles = UTIL_VecToAngles( diff );
				break;
			case TURN_INSTANT_YAW_ONLY:
				pev->angles.y = UTIL_VecToAngles( diff ).y;
				break;
			default:
				break;
			}

			if ( !FStringNull( m_moveSequence ) )
			{
				int sequence = LookupSequence( STRING( m_moveSequence ) );
				if ( pev->sequence != sequence && sequence != -1 )
				{
					pev->sequence = sequence;
					ResetSequenceInfo();
				}
			}
			if ( m_moveFramerate )
				pev->framerate = m_moveFramerate;
		}
	}
}

void CModelTrain::TrainThink( void )
{
	AdvanceAnimation();
}

void CModelTrain::OnStop( void )
{
	CFuncTrain::OnStop();

	int idleSequence = GetIdleSequence();
	if ( pev->sequence != idleSequence )
	{
		pev->sequence = idleSequence;
		ResetSequenceInfo();
	}
	if ( m_idleFramerate )
		pev->framerate = m_idleFramerate;
}

int CModelTrain::GetIdleSequence( void )
{
	int idleSequence = 0;
	if ( !FStringNull( m_idleSequence ) )
	{
		idleSequence = LookupSequence( STRING( m_idleSequence ) );
		if ( idleSequence < 0 )
		{
			ALERT( at_console, "%s: unknown sequence \"%s\"\n", STRING( pev->classname ), STRING( m_idleSequence ) );
			idleSequence = 0;
		}
	}
	return idleSequence;
}





// ---------------------------------------------------------------------
//
// Track Train
//
// ---------------------------------------------------------------------

TYPEDESCRIPTION	CFuncTrackTrain::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTrackTrain, m_ppath, FIELD_CLASSPTR ),
	DEFINE_FIELD( CFuncTrackTrain, m_length, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_height, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_speed, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_dir, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_startSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_controlMins, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTrackTrain, m_controlMaxs, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTrackTrain, m_sounds, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTrackTrain, m_flVolume, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_flBank, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_oldSpeed, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CFuncTrackTrain, CBaseEntity );
LINK_ENTITY_TO_CLASS( func_tracktrain, CFuncTrackTrain );

void CFuncTrackTrain :: KeyValue( KeyValueData *pkvd )
{
	// SBO 2.0 (Featureful)
	if (FStrEq(pkvd->szKeyName, "custommovesnd"))
	{
		m_iszCustomMoveSnd = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "customstopsnd"))
	{
		m_iszCustomStopSnd = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "soundradius"))
	{
		m_iSoundRadius = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "wheels"))
	{
		m_length = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "height"))
	{
		m_height = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "startspeed"))
	{
		m_startSpeed = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sounds"))
	{
		m_sounds = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "volume"))
	{
		m_flVolume = (float) (atoi(pkvd->szValue));
		m_flVolume *= 0.1;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "bank"))
	{
		m_flBank = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}


void CFuncTrackTrain :: NextThink( float thinkTime, BOOL alwaysThink )
{
	if ( alwaysThink )
		pev->flags |= FL_ALWAYSTHINK;
	else
		pev->flags &= ~FL_ALWAYSTHINK;

	pev->nextthink = thinkTime;
}


void CFuncTrackTrain :: Blocked( CBaseEntity *pOther )
{
	entvars_t	*pevOther = pOther->pev;

	// Blocker is on-ground on the train
	if ( FBitSet( pevOther->flags, FL_ONGROUND ) && VARS(pevOther->groundentity) == pev )
	{
		float deltaSpeed = fabs(pev->speed);
		if ( deltaSpeed > 50 )
			deltaSpeed = 50;
		if ( !pevOther->velocity.z )
			pevOther->velocity.z += deltaSpeed;
		return;
	}
	else
		pevOther->velocity = (pevOther->origin - pev->origin ).Normalize() * pev->dmg;

	ALERT( at_aiconsole, "TRAIN(%s): Blocked by %s (dmg:%.2f)\n", STRING(pev->targetname), STRING(pOther->pev->classname), pev->dmg );
	if ( pev->dmg <= 0 )
		return;
	// we can't hurt this thing, so we're not concerned with it
	pOther->TakeDamage(pev, pev, pev->dmg, DMG_CRUSH);
}


void CFuncTrackTrain :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType != USE_SET )
	{
		if ( !ShouldToggle( useType, (pev->speed != 0) ) )
			return;

		if ( pev->speed == 0 )
		{
			pev->speed = m_speed * m_dir;
			
			Next();
		}
		else
		{
			pev->speed = 0;
			pev->velocity = g_vecZero;
			pev->avelocity = g_vecZero;
			StopSound();
			SetThink( NULL );
		}
	}
	else
	{
		float delta = value;

		delta = ((int)(pev->speed * 4) / (int)m_speed)*0.25 + 0.25 * delta;
		if ( delta > 1 )
			delta = 1;
		else if ( delta < -1 )
			delta = -1;
		if ( pev->spawnflags & SF_TRACKTRAIN_FORWARDONLY )
		{
			if ( delta < 0 )
				delta = 0;
		}
		pev->speed = m_speed * delta;
		Next();	
		ALERT( at_aiconsole, "TRAIN(%s), speed to %.2f\n", STRING(pev->targetname), pev->speed );
	}
}


static float Fix( float angle )
{
	while ( angle < 0 )
		angle += 360;
	while ( angle > 360 )
		angle -= 360;

	return angle;
}


static void FixupAngles( Vector &v )
{
	v.x = Fix( v.x );
	v.y = Fix( v.y );
	v.z = Fix( v.z );
}

#define TRAIN_STARTPITCH	60
#define TRAIN_MAXPITCH		200
#define TRAIN_MAXSPEED		1000	// approx max speed for sound pitch calculation

void CFuncTrackTrain :: StopSound( void )
{
	// if sound playing, stop it
	if (m_soundPlaying && pev->noise)
	{
		unsigned short us_encode;
		unsigned short us_sound  = ( ( unsigned short )( m_sounds ) & 0x0007 ) << 12;

		us_encode = us_sound;

		// SBO 2.0 (Featureful): a custom move sound is the server's own sound, stopped here
		if ( FStringNull( m_iszCustomMoveSnd ) )
			PLAYBACK_EVENT_FULL( FEV_RELIABLE | FEV_UPDATE, edict(), m_usAdjustPitch, 0.0, 
				(float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, us_encode, 0, 1, 0 );
		else
			STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise));
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, FStringNull( m_iszCustomStopSnd ) ? "plats/ttrain_brake1.wav" : (char*)STRING( m_iszCustomStopSnd ),
			m_flVolume, SoundRadiusAttenuation( m_iSoundRadius ), 0, 100);
	}

	m_soundPlaying = 0;
}

// update pitch based on speed, start sound if not playing
// NOTE: when train goes through transition, m_soundPlaying should go to 0, 
// which will cause the looped sound to restart.

void CFuncTrackTrain :: UpdateSound( void )
{
	float flpitch;
	
	if (!pev->noise)
		return;

	// halflife-updated: fabs instead of abs((int)x), which truncated the speed
	flpitch = TRAIN_STARTPITCH + (fabs(pev->speed) * (TRAIN_MAXPITCH - TRAIN_STARTPITCH) / TRAIN_MAXSPEED);

	if (!m_soundPlaying)
	{
		// play startup sound for train
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "plats/ttrain_start1.wav", m_flVolume, SoundRadiusAttenuation( m_iSoundRadius ), 0, 100);
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, SoundRadiusAttenuation( m_iSoundRadius ), 0, (int) flpitch);
		m_soundPlaying = 1;
	} 
	else if ( !FStringNull( m_iszCustomMoveSnd ) )
	{
		// SBO 2.0 (Featureful): a custom move sound is re-pitched by the server (the train event plays Sounds 1-6)
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, SoundRadiusAttenuation( m_iSoundRadius ), SND_CHANGE_PITCH, (int) flpitch);
	}
	else
	{
/*
		// update pitch
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, (int) flpitch);
*/
		// volume 0.0 - 1.0 - 6 bits
		// m_sounds 3 bits
		// flpitch = 6 bits
		// 15 bits total

		unsigned short us_encode;
		unsigned short us_sound  = ( ( unsigned short )( m_sounds ) & 0x0007 ) << 12;
		unsigned short us_pitch  = ( ( unsigned short )( flpitch / 10.0 ) & 0x003f ) << 6;
		unsigned short us_volume = ( ( unsigned short )( m_flVolume * 40.0 ) & 0x003f );

		us_encode = us_sound | us_pitch | us_volume;

		PLAYBACK_EVENT_FULL( FEV_RELIABLE | FEV_UPDATE, edict(), m_usAdjustPitch, 0.0, 
			(float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, us_encode, 0, 0, 0 );
	}
}


void CFuncTrackTrain :: Next( void )
{
	float time = 0.5;

	if ( !pev->speed )
	{
		ALERT( at_aiconsole, "TRAIN(%s): Speed is 0\n", STRING(pev->targetname) );
		StopSound();
		return;
	}

//	if ( !m_ppath )
//		m_ppath = CPathTrack::Instance(FIND_ENTITY_BY_TARGETNAME( NULL, STRING(pev->target) ));
	if ( !m_ppath )
	{	
		ALERT( at_aiconsole, "TRAIN(%s): Lost path\n", STRING(pev->targetname) );
		StopSound();
		return;
	}

	UpdateSound();

	Vector nextPos = pev->origin;

	nextPos.z -= m_height;
	CPathTrack *pnext = m_ppath->LookAhead( &nextPos, pev->speed * 0.1, 1 );
	nextPos.z += m_height;

	pev->velocity = (nextPos - pev->origin) * 10;
	Vector nextFront = pev->origin;

	nextFront.z -= m_height;
	if ( m_length > 0 )
		m_ppath->LookAhead( &nextFront, m_length, 0 );
	else
		m_ppath->LookAhead( &nextFront, 100, 0 );
	nextFront.z += m_height;

	Vector delta = nextFront - pev->origin;
	Vector angles = UTIL_VecToAngles( delta );
	// The train actually points west
	angles.y += 180;

	// !!!  All of this crap has to be done to make the angles not wrap around, revisit this.
	FixupAngles( angles );
	FixupAngles( pev->angles );

	if ( !pnext || (delta.x == 0 && delta.y == 0) )
		angles = pev->angles;

	float vy, vx;
	if ( !(pev->spawnflags & SF_TRACKTRAIN_NOPITCH) )
		vx = UTIL_AngleDistance( angles.x, pev->angles.x );
	else
		vx = 0;
	vy = UTIL_AngleDistance( angles.y, pev->angles.y );

	pev->avelocity.y = vy * 10;
	pev->avelocity.x = vx * 10;

	if ( m_flBank != 0 )
	{
		if ( pev->avelocity.y < -5 )
			pev->avelocity.z = UTIL_AngleDistance( UTIL_ApproachAngle( -m_flBank, pev->angles.z, m_flBank*2 ), pev->angles.z);
		else if ( pev->avelocity.y > 5 )
			pev->avelocity.z = UTIL_AngleDistance( UTIL_ApproachAngle( m_flBank, pev->angles.z, m_flBank*2 ), pev->angles.z);
		else
			pev->avelocity.z = UTIL_AngleDistance( UTIL_ApproachAngle( 0, pev->angles.z, m_flBank*4 ), pev->angles.z) * 4;
	}
		
	if ( pnext )
	{
		if ( pnext != m_ppath )
		{
			CPathTrack *pFire;
			if ( pev->speed >= 0 )
				pFire = pnext;
			else 
				pFire = m_ppath;

			m_ppath = pnext;
			// Fire the pass target if there is one
			if ( pFire->pev->message )
			{
				FireTargets( STRING(pFire->pev->message), this, this, USE_TOGGLE, 0 );
				if ( FBitSet( pFire->pev->spawnflags, SF_PATH_FIREONCE ) )
					pFire->pev->message = 0;
			}

			if ( pFire->pev->spawnflags & SF_PATH_DISABLE_TRAIN )
				pev->spawnflags |= SF_TRACKTRAIN_NOCONTROL;
			
			// Don't override speed if under user control
			if ( pev->spawnflags & SF_TRACKTRAIN_NOCONTROL )
			{
				if ( pFire->pev->speed != 0 )
				{// don't copy speed from target if it is 0 (uninitialized)
					pev->speed = pFire->pev->speed;
					ALERT( at_aiconsole, "TrackTrain %s speed to %4.2f\n", STRING(pev->targetname), pev->speed );
				}
			}

		}
		SetThink( &CFuncTrackTrain::Next );
		NextThink( pev->ltime + time, TRUE );
	}
	else	// end of path, stop
	{
		StopSound();
		pev->velocity = (nextPos - pev->origin);
		pev->avelocity = g_vecZero;
		float distance = pev->velocity.Length();
		m_oldSpeed = pev->speed;


		pev->speed = 0;
		
		// Move to the dead end
		
		// Are we there yet?
		if ( distance > 0 )
		{
			// no, how long to get there?
			time = distance / m_oldSpeed;
			pev->velocity = pev->velocity * (m_oldSpeed / distance);
			SetThink( &CFuncTrackTrain::DeadEnd );
			NextThink( pev->ltime + time, FALSE );
		}
		else
		{
			DeadEnd();
		}
	}
}


void CFuncTrackTrain::DeadEnd( void )
{
	// Fire the dead-end target if there is one
	CPathTrack *pTrack, *pNext;

	pTrack = m_ppath;

	ALERT( at_aiconsole, "TRAIN(%s): Dead end ", STRING(pev->targetname) );
	// Find the dead end path node
	// HACKHACK -- This is bugly, but the train can actually stop moving at a different node depending on it's speed
	// so we have to traverse the list to it's end.
	if ( pTrack )
	{
		if ( m_oldSpeed < 0 )
		{
			do
			{
				pNext = pTrack->ValidPath( pTrack->GetPrevious(), TRUE );
				if ( pNext )
					pTrack = pNext;
			} while ( pNext );
		}
		else
		{
			do
			{
				pNext = pTrack->ValidPath( pTrack->GetNext(), TRUE );
				if ( pNext )
					pTrack = pNext;
			} while ( pNext );
		}
	}

	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	if ( pTrack )
	{
		ALERT( at_aiconsole, "at %s\n", STRING(pTrack->pev->targetname) );
		if ( pTrack->pev->netname )
			FireTargets( STRING(pTrack->pev->netname), this, this, USE_TOGGLE, 0 );
	}
	else
		ALERT( at_aiconsole, "\n" );
}


void CFuncTrackTrain :: SetControls( entvars_t *pevControls )
{
	Vector offset = pevControls->origin - pev->oldorigin;

	m_controlMins = pevControls->mins + offset;
	m_controlMaxs = pevControls->maxs + offset;
}


BOOL CFuncTrackTrain :: OnControls( entvars_t *pevTest )
{
	Vector offset = pevTest->origin - pev->origin;

	if ( pev->spawnflags & SF_TRACKTRAIN_NOCONTROL )
		return FALSE;

	// Transform offset into local coordinates
	UTIL_MakeVectors( pev->angles );
	Vector local;
	local.x = DotProduct( offset, gpGlobals->v_forward );
	local.y = -DotProduct( offset, gpGlobals->v_right );
	local.z = DotProduct( offset, gpGlobals->v_up );

	if ( local.x >= m_controlMins.x && local.y >= m_controlMins.y && local.z >= m_controlMins.z &&
		 local.x <= m_controlMaxs.x && local.y <= m_controlMaxs.y && local.z <= m_controlMaxs.z )
		 return TRUE;

	return FALSE;
}


void CFuncTrackTrain :: Find( void )
{
	m_ppath = CPathTrack::Instance(FIND_ENTITY_BY_TARGETNAME( NULL, STRING(pev->target) ));
	if ( !m_ppath )
		return;

	entvars_t *pevTarget = m_ppath->pev;
	if ( !FClassnameIs( pevTarget, "path_track" ) )
	{
		ALERT( at_error, "func_track_train must be on a path of path_track\n" );
		m_ppath = NULL;
		return;
	}

	Vector nextPos = pevTarget->origin;
	nextPos.z += m_height;

	Vector look = nextPos;
	look.z -= m_height;
	m_ppath->LookAhead( &look, m_length, 0 );
	look.z += m_height;

	pev->angles = UTIL_VecToAngles( look - nextPos );
	// The train actually points west
	pev->angles.y += 180;

	if ( pev->spawnflags & SF_TRACKTRAIN_NOPITCH )
		pev->angles.x = 0;
    UTIL_SetOrigin( pev, nextPos );
	NextThink( pev->ltime + 0.1, FALSE );
	SetThink( &CFuncTrackTrain::Next );
	pev->speed = m_startSpeed;

	UpdateSound();
}


void CFuncTrackTrain :: NearestPath( void )
{
	CBaseEntity *pTrack = NULL;
	CBaseEntity *pNearest = NULL;
	float dist, closest;

	closest = 1024;

	while ((pTrack = UTIL_FindEntityInSphere( pTrack, pev->origin, 1024 )) != NULL)
	{
		// filter out non-tracks
		if ( !(pTrack->pev->flags & (FL_CLIENT|FL_MONSTER)) && FClassnameIs( pTrack->pev, "path_track" ) )
		{
			dist = (pev->origin - pTrack->pev->origin).Length();
			if ( dist < closest )
			{
				closest = dist;
				pNearest = pTrack;
			}
		}
	}

	if ( !pNearest )
	{
		ALERT( at_console, "Can't find a nearby track !!!\n" );
		SetThink(NULL);
		return;
	}

	ALERT( at_aiconsole, "TRAIN: %s, Nearest track is %s\n", STRING(pev->targetname), STRING(pNearest->pev->targetname) );
	// If I'm closer to the next path_track on this path, then it's my real path
	pTrack = ((CPathTrack *)pNearest)->GetNext();
	if ( pTrack )
	{
		if ( (pev->origin - pTrack->pev->origin).Length() < (pev->origin - pNearest->pev->origin).Length() )
			pNearest = pTrack;
	}

	m_ppath = (CPathTrack *)pNearest;

	if ( pev->speed != 0 )
	{
		NextThink( pev->ltime + 0.1, FALSE );
		SetThink( &CFuncTrackTrain::Next );
	}
}


void CFuncTrackTrain::OverrideReset( void )
{
	NextThink( pev->ltime + 0.1, FALSE );
	SetThink( &CFuncTrackTrain::NearestPath );
}


CFuncTrackTrain *CFuncTrackTrain::Instance( edict_t *pent )
{ 
	if ( FClassnameIs( pent, "func_tracktrain" ) )
		return (CFuncTrackTrain *)GET_PRIVATE(pent);
	return NULL;
}

/*QUAKED func_train (0 .5 .8) ?
Trains are moving platforms that players can ride.
The targets origin specifies the min point of the train at each corner.
The train spawns at the first target it is pointing at.
If the train is the target of a button or trigger, it will not begin moving until activated.
speed	default 100
dmg		default	2
sounds
1) ratchet metal
*/

void CFuncTrackTrain :: Spawn( void )
{
	if ( pev->speed == 0 )
		m_speed = 100;
	else
		m_speed = pev->speed;
	
	pev->speed = 0;
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	pev->impulse = m_speed;

	m_dir = 1;

	if ( FStringNull(pev->target) )
		ALERT( at_console, "FuncTrain with no target" );

	if ( pev->spawnflags & SF_TRACKTRAIN_PASSABLE )
		pev->solid = SOLID_NOT;
	else
		pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	
	SET_MODEL( ENT(pev), STRING(pev->model) );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );

	// Cache off placed origin for train controls
	pev->oldorigin = pev->origin;

	m_controlMins = pev->mins;
	m_controlMaxs = pev->maxs;
	m_controlMaxs.z += 72;
// start trains on the next frame, to make sure their targets have had
// a chance to spawn/activate
	NextThink( pev->ltime + 0.1, FALSE );
	SetThink( &CFuncTrackTrain::Find );
	Precache();
}

void CFuncTrackTrain :: Precache( void )
{
	if (m_flVolume == 0.0)
		m_flVolume = 1.0;

	switch (m_sounds)
	{
	default:
		// no sound
		pev->noise = 0;
		break;
	case 1: PRECACHE_SOUND("plats/ttrain1.wav"); pev->noise = MAKE_STRING("plats/ttrain1.wav");break;
	case 2: PRECACHE_SOUND("plats/ttrain2.wav"); pev->noise = MAKE_STRING("plats/ttrain2.wav");break;
	case 3: PRECACHE_SOUND("plats/ttrain3.wav"); pev->noise = MAKE_STRING("plats/ttrain3.wav");break; 
	case 4: PRECACHE_SOUND("plats/ttrain4.wav"); pev->noise = MAKE_STRING("plats/ttrain4.wav");break;
	case 5: PRECACHE_SOUND("plats/ttrain6.wav"); pev->noise = MAKE_STRING("plats/ttrain6.wav");break;
	case 6: PRECACHE_SOUND("plats/ttrain7.wav"); pev->noise = MAKE_STRING("plats/ttrain7.wav");break;
	}

	PRECACHE_SOUND("plats/ttrain_brake1.wav");
	PRECACHE_SOUND("plats/ttrain_start1.wav");

	// SBO 2.0 (Featureful): custom move and brake sounds
	if ( !FStringNull( m_iszCustomMoveSnd ) )
	{
		PRECACHE_SOUND( (char *)STRING( m_iszCustomMoveSnd ) );
		pev->noise = m_iszCustomMoveSnd;
	}
	if ( !FStringNull( m_iszCustomStopSnd ) )
		PRECACHE_SOUND( (char *)STRING( m_iszCustomStopSnd ) );

	m_usAdjustPitch = PRECACHE_EVENT( 1, "events/train.sc" );
}

// This class defines the volume of space that the player must stand in to control the train
class CFuncTrainControls : public CBaseEntity
{
public:
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Spawn( void );
	void EXPORT Find( void );
};
LINK_ENTITY_TO_CLASS( func_traincontrols, CFuncTrainControls );


void CFuncTrainControls :: Find( void )
{
	edict_t *pTarget = NULL;

	do 
	{
		pTarget = FIND_ENTITY_BY_TARGETNAME( pTarget, STRING(pev->target) );
	} while ( !FNullEnt(pTarget) && !FClassnameIs(pTarget, "func_tracktrain") );

	if ( FNullEnt( pTarget ) )
	{
		ALERT( at_console, "No train %s\n", STRING(pev->target) );
		return;
	}

	CFuncTrackTrain *ptrain = CFuncTrackTrain::Instance(pTarget);
	ptrain->SetControls( pev );
	UTIL_Remove( this );
}


void CFuncTrainControls :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( ENT(pev), STRING(pev->model) );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );
	
	SetThink( &CFuncTrainControls::Find );
	pev->nextthink = gpGlobals->time;
}



// ----------------------------------------------------------------------------
//
// Track changer / Train elevator
//
// ----------------------------------------------------------------------------

#define SF_TRACK_ACTIVATETRAIN		0x00000001
#define SF_TRACK_RELINK				0x00000002
#define SF_TRACK_ROTMOVE			0x00000004
#define SF_TRACK_STARTBOTTOM		0x00000008
#define SF_TRACK_DONT_MOVE			0x00000010

//
// This entity is a rotating/moving platform that will carry a train to a new track.
// It must be larger in X-Y planar area than the train, since it must contain the
// train within these dimensions in order to operate when the train is near it.
//

typedef enum { TRAIN_SAFE, TRAIN_BLOCKING, TRAIN_FOLLOWING } TRAIN_CODE;

class CFuncTrackChange : public CFuncPlatRot
{
public:
	void Spawn( void );
	void Precache( void );

//	virtual void	Blocked( void );
	virtual void	EXPORT GoUp( void );
	virtual void	EXPORT GoDown( void );

	void			KeyValue( KeyValueData* pkvd );
	void			Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void			EXPORT Find( void );
	TRAIN_CODE		EvaluateTrain( CPathTrack *pcurrent );
	void			UpdateTrain( Vector &dest );
	virtual void	HitBottom( void );
	virtual void	HitTop( void );
	void			Touch( CBaseEntity *pOther );
	virtual void	UpdateAutoTargets( int toggleState );
	virtual	BOOL	IsTogglePlat( void ) { return TRUE; }

	void			DisableUse( void ) { m_use = 0; }
	void			EnableUse( void ) { m_use = 1; }
	int				UseEnabled( void ) { return m_use; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	virtual void	OverrideReset( void );


	CPathTrack		*m_trackTop;
	CPathTrack		*m_trackBottom;

	EHANDLE			m_hTrain;
	// NULL once the train is removed (killtarget): every use treats that as no train
	CFuncTrackTrain	*Train( void ) { return (CFuncTrackTrain *)(CBaseEntity *)m_hTrain; }

	int				m_trackTopName;
	int				m_trackBottomName;
	int				m_trainName;
	TRAIN_CODE		m_code;
	int				m_targetState;
	int				m_use;
};
LINK_ENTITY_TO_CLASS( func_trackchange, CFuncTrackChange );

TYPEDESCRIPTION	CFuncTrackChange::m_SaveData[] = 
{
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_trackTop, FIELD_CLASSPTR ),
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_trackBottom, FIELD_CLASSPTR ),
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_hTrain, FIELD_EHANDLE ),
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_trackTopName, FIELD_STRING ),
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_trackBottomName, FIELD_STRING ),
	DEFINE_GLOBAL_FIELD( CFuncTrackChange, m_trainName, FIELD_STRING ),
	DEFINE_FIELD( CFuncTrackChange, m_code, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTrackChange, m_targetState, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTrackChange, m_use, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CFuncTrackChange, CFuncPlatRot );

void CFuncTrackChange :: Spawn( void )
{
	Setup();
	if ( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ) )
		m_vecPosition2.z = pev->origin.z;

	SetupRotation();

	if ( FBitSet( pev->spawnflags, SF_TRACK_STARTBOTTOM ) )
	{
		UTIL_SetOrigin (pev, m_vecPosition2);
		m_toggle_state = TS_AT_BOTTOM;
		pev->angles = m_start;
		m_targetState = TS_AT_TOP;
	}
	else
	{
		UTIL_SetOrigin (pev, m_vecPosition1);
		m_toggle_state = TS_AT_TOP;
		pev->angles = m_end;
		m_targetState = TS_AT_BOTTOM;
	}

	EnableUse();
	pev->nextthink = pev->ltime + 2.0;
	SetThink( &CFuncTrackChange::Find );
	Precache();
}

void CFuncTrackChange :: Precache( void )
{
	// Can't trigger sound
	PRECACHE_SOUND( "buttons/button11.wav" );
	
	CFuncPlatRot::Precache();
}


// UNDONE: Filter touches before re-evaluating the train.
void CFuncTrackChange :: Touch( CBaseEntity *pOther )
{
#if 0
	TRAIN_CODE code;
	entvars_t *pevToucher = pOther->pev;
#endif
}



void CFuncTrackChange :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq(pkvd->szKeyName, "train") )
	{
		m_trainName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "toptrack") )
	{
		m_trackTopName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "bottomtrack") )
	{
		m_trackBottomName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CFuncPlatRot::KeyValue( pkvd );		// Pass up to base class
	}
}


void CFuncTrackChange::OverrideReset( void )
{
	pev->nextthink = pev->ltime + 1.0;
	SetThink( &CFuncTrackChange::Find );
}

void CFuncTrackChange :: Find( void )
{
	// Find track entities
	edict_t *target;

	target = FIND_ENTITY_BY_TARGETNAME( NULL, STRING(m_trackTopName) );
	if ( !FNullEnt(target) )
	{
		m_trackTop = CPathTrack::Instance( target );
		target = FIND_ENTITY_BY_TARGETNAME( NULL, STRING(m_trackBottomName) );
		if ( !FNullEnt(target) )
		{
			m_trackBottom = CPathTrack::Instance( target );
			target = FIND_ENTITY_BY_TARGETNAME( NULL, STRING(m_trainName) );
			if ( !FNullEnt(target) )
			{
				m_hTrain = CFuncTrackTrain::Instance( FIND_ENTITY_BY_TARGETNAME( NULL, STRING(m_trainName) ) );
				if ( !Train() )
				{
					ALERT( at_error, "Can't find train for track change! %s\n", STRING(m_trainName) );
					return;
				}
				Vector center = (pev->absmin + pev->absmax) * 0.5;
				m_trackBottom = m_trackBottom->Nearest( center );
				m_trackTop = m_trackTop->Nearest( center );
				UpdateAutoTargets( m_toggle_state );
				SetThink( NULL );
				return;
			}
			else
			{
				ALERT( at_error, "Can't find train for track change! %s\n", STRING(m_trainName) );
				target = FIND_ENTITY_BY_TARGETNAME( NULL, STRING(m_trainName) );
			}
		}
		else
			ALERT( at_error, "Can't find bottom track for track change! %s\n", STRING(m_trackBottomName) );
	}
	else
		ALERT( at_error, "Can't find top track for track change! %s\n", STRING(m_trackTopName) );
}



TRAIN_CODE CFuncTrackChange :: EvaluateTrain( CPathTrack *pcurrent )
{
	// Go ahead and work, we don't have anything to switch, so just be an elevator
	CFuncTrackTrain *pTrain = Train();
	if ( !pcurrent || !pTrain )
		return TRAIN_SAFE;

	if ( pTrain->m_ppath == pcurrent || (pcurrent->m_pprevious && pTrain->m_ppath == pcurrent->m_pprevious) ||
		 (pcurrent->m_pnext && pTrain->m_ppath == pcurrent->m_pnext) )
	{
		if ( pTrain->pev->speed != 0 )
			return TRAIN_BLOCKING;

		Vector dist = pev->origin - pTrain->pev->origin;
		float length = dist.Length2D();
		if ( length < pTrain->m_length )		// Empirically determined close distance
			return TRAIN_FOLLOWING;
		else if ( length > (150 + pTrain->m_length) )
			return TRAIN_SAFE;

		return TRAIN_BLOCKING;
	}
	
	return TRAIN_SAFE;
}


void CFuncTrackChange :: UpdateTrain( Vector &dest )
{
	CFuncTrackTrain *pTrain = Train();
	if ( !pTrain )
		return;

	float time = (pev->nextthink - pev->ltime);

	pTrain->pev->velocity = pev->velocity;
	pTrain->pev->avelocity = pev->avelocity;
	pTrain->NextThink( pTrain->pev->ltime + time, FALSE );

	// Attempt at getting the train to rotate properly around the origin of the trackchange
	if ( time <= 0 )
		return;

	Vector offset = pTrain->pev->origin - pev->origin;
	Vector delta = dest - pev->angles;
	// Transform offset into local coordinates
	UTIL_MakeInvVectors( delta, gpGlobals );
	Vector local;
	local.x = DotProduct( offset, gpGlobals->v_forward );
	local.y = DotProduct( offset, gpGlobals->v_right );
	local.z = DotProduct( offset, gpGlobals->v_up );

	local = local - offset;
	pTrain->pev->velocity = pev->velocity + (local * (1.0/time));
}

void CFuncTrackChange :: GoDown( void )
{
	if ( m_code == TRAIN_BLOCKING )
		return;

	// HitBottom may get called during CFuncPlat::GoDown(), so set up for that
	// before you call GoDown()

	UpdateAutoTargets( TS_GOING_DOWN );
	// If ROTMOVE, move & rotate
	if ( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ) )
	{
		// SBO 2.0: the move sound, Fire On Start and Close Start, as CFuncPlat::GoDown (Half-Life turned in silence)
		if ( pev->noiseMovement )
			EMIT_SOUND( ENT( pev ), CHAN_STATIC, (char *)STRING( pev->noiseMovement ), m_volume, SoundAttenuation() );
		SetMoveDone( &CFuncTrackChange::CallHitBottom );
		m_toggle_state = TS_GOING_DOWN;
		AngularMove( m_start, pev->speed );
		OnStartMoving();
		FireOn( m_iszFireOnClosing, m_bFireOnClosingState );
	}
	else
	{
		CFuncPlat :: GoDown();
		SetMoveDone( &CFuncTrackChange::CallHitBottom );
		RotMove( m_start, pev->nextthink - pev->ltime );
	}
	// Otherwise, rotate first, move second

	// If the train is moving with the platform, update it
	if ( m_code == TRAIN_FOLLOWING )
	{
		UpdateTrain( m_start );
		if ( Train() )
			Train()->m_ppath = NULL;
	}
}


//
// Platform is at bottom, now starts moving up
//
void CFuncTrackChange :: GoUp( void )
{
	if ( m_code == TRAIN_BLOCKING )
		return;

	// HitTop may get called during CFuncPlat::GoUp(), so set up for that
	// before you call GoUp();

	UpdateAutoTargets( TS_GOING_UP );
	if ( FBitSet( pev->spawnflags, SF_TRACK_DONT_MOVE ) )
	{
		// SBO 2.0: the move sound, Fire On Start and Open Start, as CFuncPlat::GoUp (Half-Life turned in silence)
		if ( pev->noiseMovement )
			EMIT_SOUND( ENT( pev ), CHAN_STATIC, (char *)STRING( pev->noiseMovement ), m_volume, SoundAttenuation() );
		m_toggle_state = TS_GOING_UP;
		SetMoveDone( &CFuncTrackChange::CallHitTop );
		AngularMove( m_end, pev->speed );
		OnStartMoving();
		FireOn( m_iszFireOnOpening, m_bFireOnOpeningState );
	}
	else
	{
		// If ROTMOVE, move & rotate
		CFuncPlat :: GoUp();
		SetMoveDone( &CFuncTrackChange::CallHitTop );
		RotMove( m_end, pev->nextthink - pev->ltime );
	}
	
	// Otherwise, move first, rotate second

	// If the train is moving with the platform, update it
	if ( m_code == TRAIN_FOLLOWING )
	{
		UpdateTrain( m_end );
		if ( Train() )
			Train()->m_ppath = NULL;
	}
}


// Normal track change
void CFuncTrackChange :: UpdateAutoTargets( int toggleState )
{
	if ( !m_trackTop || !m_trackBottom )
		return;

	if ( toggleState == TS_AT_TOP )
		ClearBits( m_trackTop->pev->spawnflags, SF_PATH_DISABLED );
	else
		SetBits( m_trackTop->pev->spawnflags, SF_PATH_DISABLED );

	if ( toggleState == TS_AT_BOTTOM )
		ClearBits( m_trackBottom->pev->spawnflags, SF_PATH_DISABLED );
	else
		SetBits( m_trackBottom->pev->spawnflags, SF_PATH_DISABLED );
}


void CFuncTrackChange :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_toggle_state != TS_AT_TOP && m_toggle_state != TS_AT_BOTTOM )
		return;

	// If train is in "safe" area, but not on the elevator, play alarm sound
	if ( m_toggle_state == TS_AT_TOP )
		m_code = EvaluateTrain( m_trackTop );
	else if ( m_toggle_state == TS_AT_BOTTOM )
		m_code = EvaluateTrain( m_trackBottom );
	else
		m_code = TRAIN_BLOCKING;
	if ( m_code == TRAIN_BLOCKING )
	{
		// Play alarm and return
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/button11.wav", 1, ATTN_NORM);
		return;
	}

	// Otherwise, it's safe to move
	// If at top, go down
	// at bottom, go up

	DisableUse();
	if (m_toggle_state == TS_AT_TOP)
		GoDown();
	else
		GoUp();
}


//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncTrackChange :: HitBottom( void )
{
	CFuncPlatRot :: HitBottom();
	if ( m_code == TRAIN_FOLLOWING && Train() )
	{
//		UpdateTrain();
		Train()->SetTrack( m_trackBottom );
	}
	SetThink( NULL );
	pev->nextthink = -1;

	UpdateAutoTargets( m_toggle_state );

	EnableUse();
}


//
// Platform has hit bottom.  Stops and waits forever.
//
void CFuncTrackChange :: HitTop( void )
{
	CFuncPlatRot :: HitTop();
	if ( m_code == TRAIN_FOLLOWING && Train() )
	{
//		UpdateTrain();
		Train()->SetTrack( m_trackTop );
	}
	
	// Don't let the plat go back down
	SetThink( NULL );
	pev->nextthink = -1;
	UpdateAutoTargets( m_toggle_state );
	EnableUse();
}



class CFuncTrackAuto : public CFuncTrackChange
{
public:
	void			Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void	UpdateAutoTargets( int toggleState );
};

LINK_ENTITY_TO_CLASS( func_trackautochange, CFuncTrackAuto );

// Auto track change
void CFuncTrackAuto :: UpdateAutoTargets( int toggleState )
{
	CPathTrack *pTarget, *pNextTarget;

	if ( !m_trackTop || !m_trackBottom )
		return;

	if ( m_targetState == TS_AT_TOP )
	{
		pTarget = m_trackTop->GetNext();
		pNextTarget = m_trackBottom->GetNext();
	}
	else
	{
		pTarget = m_trackBottom->GetNext();
		pNextTarget = m_trackTop->GetNext();
	}
	if ( pTarget )
	{
		ClearBits( pTarget->pev->spawnflags, SF_PATH_DISABLED );
		CFuncTrackTrain *pTrain = Train();
		if ( m_code == TRAIN_FOLLOWING && pTrain && pTrain->pev->speed == 0 )
			pTrain->Use( this, this, USE_ON, 0 );
	}

	if ( pNextTarget )
		SetBits( pNextTarget->pev->spawnflags, SF_PATH_DISABLED );

}


void CFuncTrackAuto :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CPathTrack *pTarget;

	if ( !UseEnabled() )
		return;

	if ( m_toggle_state == TS_AT_TOP )
		pTarget = m_trackTop;
	else if ( m_toggle_state == TS_AT_BOTTOM )
		pTarget = m_trackBottom;
	else
		pTarget = NULL;

	// No activator (trigger_auto, timers): not the train, same as any other activator
	if ( pActivator && FClassnameIs( pActivator->pev, "func_tracktrain" ) )
	{
		m_code = EvaluateTrain( pTarget );
		// Safe to fire?
		if ( m_code == TRAIN_FOLLOWING && m_toggle_state != m_targetState )
		{
			DisableUse();
			if (m_toggle_state == TS_AT_TOP)
				GoDown();
			else
				GoUp();
		}
	}
	else
	{
		if ( pTarget )
			pTarget = pTarget->GetNext();
		// No train (Find failed or the train was removed): not on that path
		if ( pTarget && ( !Train() || Train()->m_ppath != pTarget ) && ShouldToggle( useType, m_targetState ) )
		{
			if ( m_targetState == TS_AT_TOP )
				m_targetState = TS_AT_BOTTOM;
			else
				m_targetState = TS_AT_TOP;
		}

		UpdateAutoTargets( m_targetState );
	}
}


// ----------------------------------------------------------
//
//
// pev->speed is the travel speed
// pev->health is current health
// pev->max_health is the amount to reset to each time it starts

#define FGUNTARGET_START_ON			0x0001

class CGunTarget : public CBaseMonster
{
public:
	void			Spawn( void );
	void			Activate( void );
	void EXPORT		Next( void );
	void EXPORT		Start( void );
	void EXPORT		Wait( void );
	void			Stop( void );

	int				BloodColor( void ) { return DONT_BLEED; }
	int				Classify( void ) { return CLASS_MACHINE; }
	int				TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void			Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	Vector			BodyTarget( const Vector &posSrc ) { return pev->origin; }

	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	// SBO 2.1: On from its start until it stops (triggered off, shot down or at the end of its path), also while it
	// waits at a corner; it is a monster to the game, but dead or alive does not apply
	virtual STATE	GetState( CBaseEntity *pActivator ) { return m_on ? STATE_ON : STATE_OFF; }
private:
	BOOL			m_on;
};


LINK_ENTITY_TO_CLASS( func_guntarget, CGunTarget );

TYPEDESCRIPTION	CGunTarget::m_SaveData[] = 
{
	DEFINE_FIELD( CGunTarget, m_on, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CGunTarget, CBaseMonster );


void CGunTarget::Spawn( void )
{
	pev->solid		= SOLID_BSP;
	pev->movetype	= MOVETYPE_PUSH;

	UTIL_SetOrigin(pev, pev->origin);
	SET_MODEL(ENT(pev), STRING(pev->model) );

	if ( pev->speed == 0 )
		pev->speed = 100;

	// Don't take damage until "on"
	pev->takedamage = DAMAGE_NO;
	pev->flags |= FL_MONSTER;

	m_on = FALSE;
	pev->max_health = pev->health;

	if ( pev->spawnflags & FGUNTARGET_START_ON )
	{
		SetThink( &CGunTarget::Start );
		pev->nextthink = pev->ltime + 0.3;
	}
}


void CGunTarget::Activate( void )
{
	CBaseEntity	*pTarg;

	// now find our next target
	pTarg = GetNextTarget();
	if ( pTarg )
	{
		m_hTargetEnt = pTarg;
		UTIL_SetOrigin( pev, pTarg->pev->origin - (pev->mins + pev->maxs) * 0.5 );
	}
}


void CGunTarget::Start( void )
{
	Use( this, this, USE_ON, 0 );
}


void CGunTarget::Next( void )
{
	SetThink( NULL );

	m_hTargetEnt = GetNextTarget();
	CBaseEntity *pTarget = m_hTargetEnt;
	
	if ( !pTarget )
	{
		Stop();
		return;
	}
	SetMoveDone( &CGunTarget::Wait );
	LinearMove( pTarget->pev->origin - (pev->mins + pev->maxs) * 0.5, pev->speed );
}


void CGunTarget::Wait( void )
{
	CBaseEntity *pTarget = m_hTargetEnt;
	
	if ( !pTarget )
	{
		Stop();
		return;
	}

	// Fire the pass target if there is one
	if ( pTarget->pev->message )
	{
		FireTargets( STRING(pTarget->pev->message), this, this, USE_TOGGLE, 0 );
		if ( FBitSet( pTarget->pev->spawnflags, SF_CORNER_FIREONCE ) )
			pTarget->pev->message = 0;
	}
		
	m_flWait = pTarget->GetDelay();

	pev->target = pTarget->pev->target;
	SetThink( &CGunTarget::Next );
	if (m_flWait != 0)
	{// -1 wait will wait forever!		
		pev->nextthink = pev->ltime + m_flWait;
	}
	else
	{
		Next();// do it RIGHT now!
	}
}


void CGunTarget::Stop( void )
{
	pev->velocity = g_vecZero;
	pev->nextthink = 0;
	pev->takedamage = DAMAGE_NO;
	m_on = FALSE;		// SBO 2.1: off, so the next trigger (On / Toggle) starts it
}


int	CGunTarget::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( pev->health > 0 )
	{
		pev->health -= flDamage;
		if ( pev->health <= 0 )
		{
			pev->health = 0;
			Stop();
			if ( pev->message )
				FireTargets( STRING(pev->message), this, this, USE_TOGGLE, 0 );
		}
	}
	return 0;
}


void CGunTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_on ) )
		return;

	if ( m_on )
	{
		Stop();
	}
	else
	{
		pev->takedamage = DAMAGE_AIM;
		m_hTargetEnt = GetNextTarget();
		if ( m_hTargetEnt == NULL )
			return;
		pev->health = pev->max_health;
		m_on = TRUE;	// SBO 2.1: on, so Off / Toggle stop it
		Next();			// (Stop sets it FALSE again if there is no next path_corner)
	}
}





//=========================================================
// train_setspeed -- changes a moving func_tracktrain's speed, at once or in steps of 0.1 s over a time
//
// SBO 2.1, from Spirit of Half-Life 1.9 (SpiritSource/dlls/triggers.cpp CTrainSetSpeed). SBO: the train is looked up
// when it is triggered (Spirit: once at spawn), the ramp ends exactly on the speed (Spirit stopped within 1 unit and
// could step past it), a new speed of 0 stops the train as its own Off does (sound off), "target" fires when the
// speed is reached for an instant change too, and the ramp stops when the train is stopped or gone.
//=========================================================

#define TRAINSETSPEED_INSTANT	0
#define TRAINSETSPEED_RAMP		1
#define TRAINSETSPEED_MAX		2000

class CTrainSetSpeed : public CPointEntity
{
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	EXPORT RampThink( void );
	int		ObjectCaps( void ) { return CPointEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CFuncTrackTrain *FindTrain( void );
	void	SetTrainSpeed( CFuncTrackTrain *pTrain, float flSpeed );
	void	Reached( void );

	int			m_iMode;
	float		m_flTime;
	float		m_flInterval;	// speed added every 0.1 s while it ramps
	float		m_flSpeed;		// the speed it sets, clamped
	EHANDLE		m_hTrain;
	EHANDLE		m_hActivator;
};

LINK_ENTITY_TO_CLASS( train_setspeed, CTrainSetSpeed );

TYPEDESCRIPTION CTrainSetSpeed::m_SaveData[] =
{
	DEFINE_FIELD( CTrainSetSpeed, m_iMode, FIELD_INTEGER ),
	DEFINE_FIELD( CTrainSetSpeed, m_flTime, FIELD_FLOAT ),
	DEFINE_FIELD( CTrainSetSpeed, m_flInterval, FIELD_FLOAT ),
	DEFINE_FIELD( CTrainSetSpeed, m_flSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CTrainSetSpeed, m_hTrain, FIELD_EHANDLE ),
	DEFINE_FIELD( CTrainSetSpeed, m_hActivator, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CTrainSetSpeed, CPointEntity );

void CTrainSetSpeed::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "time" ) )
	{
		m_flTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "mode" ) )
	{
		m_iMode = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "train" ) )
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTrainSetSpeed::Spawn( void )
{
	CPointEntity::Spawn();
	if ( m_flTime <= 0 )
		m_flTime = 10;
	m_flSpeed = pev->speed;
	if ( m_flSpeed > TRAINSETSPEED_MAX )
		m_flSpeed = TRAINSETSPEED_MAX;
	else if ( m_flSpeed < -TRAINSETSPEED_MAX )
		m_flSpeed = -TRAINSETSPEED_MAX;
}

// the first func_tracktrain with the name in "train"
CFuncTrackTrain *CTrainSetSpeed::FindTrain( void )
{
	CBaseEntity *pEntity = NULL;
	while ( !FStringNull( pev->netname ) && ( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->netname ) ) ) != NULL )
	{
		if ( FClassnameIs( pEntity->pev, "func_tracktrain" ) )
			return (CFuncTrackTrain *)pEntity;
	}
	return NULL;
}

// a running train takes the speed at its next move (Next: velocity, sound pitch); 0 stops it as its Off does
void CTrainSetSpeed::SetTrainSpeed( CFuncTrackTrain *pTrain, float flSpeed )
{
	if ( flSpeed == 0 )
		pTrain->Use( this, this, USE_OFF, 0 );
	else
		pTrain->pev->speed = flSpeed;
}

void CTrainSetSpeed::Reached( void )
{
	SetThink( NULL );
	pev->nextthink = 0;
	if ( !FStringNull( pev->target ) )
		FireTargets( STRING( pev->target ), m_hActivator, this, USE_TOGGLE, 0 );
}

void CTrainSetSpeed::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CFuncTrackTrain *pTrain = FindTrain();
	if ( !pTrain )
	{
		ALERT( at_console, "train_setspeed \"%s\": no func_tracktrain named \"%s\"\n", STRING( pev->targetname ), STRING( pev->netname ) );
		return;
	}
	if ( pTrain->pev->speed == 0 )
		return;	// a stopped train stays stopped (Spirit)

	m_hTrain = pTrain;
	m_hActivator = pActivator;

	if ( m_iMode == TRAINSETSPEED_RAMP )
	{
		m_flInterval = ( m_flSpeed - pTrain->pev->speed ) / ( m_flTime / 0.1f );
		SetThink( &CTrainSetSpeed::RampThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		SetThink( NULL );	// a ramp of this entity in progress ends
		SetTrainSpeed( pTrain, m_flSpeed );
		Reached();
	}
}

void CTrainSetSpeed::RampThink( void )
{
	CFuncTrackTrain *pTrain = (CFuncTrackTrain *)(CBaseEntity *)m_hTrain;
	if ( !pTrain || pTrain->pev->speed == 0 )
	{
		SetThink( NULL );	// the train is gone or was stopped: the ramp ends, nothing fires
		return;
	}

	float flNew = pTrain->pev->speed + m_flInterval;
	if ( flNew == 0 )
		flNew += m_flInterval;	// never 0 on the way: a train at speed 0 stops for good (CFuncTrackTrain::Next)
	if ( m_flInterval == 0 || ( m_flInterval > 0 && flNew >= m_flSpeed ) || ( m_flInterval < 0 && flNew <= m_flSpeed ) )
	{
		SetTrainSpeed( pTrain, m_flSpeed );
		Reached();
		return;
	}

	pTrain->pev->speed = flNew;
	pev->nextthink = gpGlobals->time + 0.1;
}
