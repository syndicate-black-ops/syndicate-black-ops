// SBO: change_touch, a model brush that sets g_bChangeLevelTouched when a player touches it
// (read by the coop/dm/br game rules).

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "activity.h"
#include "animation.h"
#include "gamerules.h"

int g_bChangeLevelTouched;

// SBO 2.0: CBaseAnimating (was CBaseEntity) for the idle animation
class CChangeTouch : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT Touch( CBaseEntity *pOther );
	void EXPORT IdleThink( void );
	void SetIdleSequence( void );
};

LINK_ENTITY_TO_CLASS( change_touch, CChangeTouch );

void CChangeTouch::Spawn( void )
{
	// SBO 2.0: no model, the editor placeholder or a file the server doesn't have ends the map at the precache below
	if ( UTIL_MapperModelUnusable( pev, STRING( pev->model ) ) )
	{
		REMOVE_ENTITY( ENT( pev ) );
		return;
	}

	Precache();
	SET_MODEL( ENT(pev), STRING(pev->model) );
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize( pev, Vector(-16, -16, 0), Vector(16, 16, 72) );
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	SetTouch( &CChangeTouch::Touch );

	// SBO 2.0: the model (the G-Man in every map) stood frozen on the first frame of its first sequence: no
	// sequence, framerate or animation time was ever set. It idles now as a monster does.
	InitBoneControllers();
	pev->sequence = -1;
	SetIdleSequence();
	SetThink( &CChangeTouch::IdleThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

// One of the model's idle sequences (ACT_IDLE, picked by their weights as for a monster: mostly the plain idles of
// gman.mdl, now and then one of its gestures); the first sequence for a model without any
void CChangeTouch::SetIdleSequence( void )
{
	int iSequence = LookupActivity( ACT_IDLE );

	if ( iSequence == ACTIVITY_NOT_AVAILABLE )
		iSequence = 0;

	// a looping sequence that is picked again just keeps running
	if ( iSequence == pev->sequence && m_fSequenceLoops )
	{
		m_fSequenceFinished = FALSE;
		return;
	}

	pev->sequence = iSequence;
	pev->frame = 0;
	ResetSequenceInfo();
}

void CChangeTouch::IdleThink( void )
{
	StudioFrameAdvance();

	if ( m_fSequenceFinished )
		SetIdleSequence();

	pev->nextthink = gpGlobals->time + 0.1;
}


void CChangeTouch::Precache( void )
{
	PRECACHE_MODEL( (char *)STRING(pev->model) );
}

void CChangeTouch::Touch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer() && g_bChangeLevelTouched == 0 )
		g_bChangeLevelTouched = 1;
}
