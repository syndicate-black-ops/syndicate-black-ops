//=========================================================
// SBO 2.0: motion_manager and trigger_motion from the Featureful entity guide (see
// docs/FEATUREFUL_GUIDE_ENTITIES.md). Ported from Featureful (https://github.com/FreeSlave/halflife-featureful,
// Half-Life 1 SDK license) dlls/triggers.cpp, from Spirit of Half-Life (Laurie Cheers). Multiplayer: no save/restore;
// a player target counts only while he is in play (never a spectator). Without movewith (tabled), positions, angles
// and velocities are set on the entity alone.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "round_hooks.h"
#include "locus.h"

#define SF_MOTION_DEBUG					1
#define SF_MOTION_REQUIRES_ACTIVATOR	( 1 << 24 )

static void Motion_PrintVectors( const char *text, const Vector &oldVec, const Vector &newVec )
{
	ALERT( at_console, "%s from %g %g %g to %g %g %g\n", text, oldVec.x, oldVec.y, oldVec.z, newVec.x, newVec.y, newVec.z );
}

// a target the motion entities may move: anything but a player out of play (spectator, dead, waiting)
static BOOL Motion_TargetOk( CBaseEntity *pTarget )
{
	if ( !pTarget )
		return FALSE;
	if ( pTarget->IsPlayer() && !Logic_IsPlayerInPlay( pTarget ) )
		return FALSE;
	return TRUE;
}

static BOOL Motion_IsActivatorName( string_t isz )
{
	if ( FStringNull( isz ) )
		return FALSE;
	const char *s = STRING( isz );
	return !strcmp( s, "!activator" ) || !strcmp( s, "*locus" ) || !strcmp( s, "*player" );
}

//=========================================================
// trigger_motion: once per trigger, sets or changes the position, angles, velocity and angular velocity of "target"
// (a name or !activator). Position: 0 set [LP], 1 add offset [LV]. Angles: 0 set (face a direction) [LV], 1 rotate
// by a direction's angles [LV], 2 rotate by values (Y Z X). Velocity: 0 set [LV], 1 add [LV], 2 rotate by a
// direction's angles [LV], 3 rotate by values. AVelocity: 0 set, 1 add (Y Z X; "a .. b" picks at random). "Debug"
// (1) prints each change. A player's velocity changed on the server costs one prediction correction.
//=========================================================
class CTriggerMotion : public CPointEntity
{
public:
	enum { POSMODE_SET = 0, POSMODE_ADDOFFSET };
	enum { ANGMOD_SET = 0, ANGMOD_ROTATE, ANGMOD_ROTATE_BY_VALUES };
	enum { VELMODE_SET = 0, VELMODE_ADD, VELMODE_ROTATE, VELMODE_ROTATE_BY_VALUES };
	enum { AVELMODE_SET = 0, AVELMODE_ADD };

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );

	string_t m_iszPosition;
	int m_iPosMode;
	string_t m_iszAngles;
	int m_iAngMode;
	string_t m_iszVelocity;
	int m_iVelMode;
	string_t m_iszAVelocity;
	int m_iAVelMode;
};

LINK_ENTITY_TO_CLASS( trigger_motion, CTriggerMotion );

void CTriggerMotion::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszPosition" ) )
		m_iszPosition = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iPosMode" ) )
		m_iPosMode = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszAngles" ) )
		m_iszAngles = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iAngMode" ) )
		m_iAngMode = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszVelocity" ) )
		m_iszVelocity = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iVelMode" ) )
		m_iVelMode = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszAVelocity" ) )
		m_iszAVelocity = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iAVelMode" ) )
		m_iAVelMode = atoi( pkvd->szValue );
	else
	{
		CPointEntity::KeyValue( pkvd );
		return;
	}
	pkvd->fHandled = TRUE;
}

void CTriggerMotion::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = FStringNull( pev->target ) ? NULL : Locus_FindEntity( NULL, STRING( pev->target ), pActivator );
	if ( !pTarget )
	{
		ALERT( at_console, "%s \"%s\" can't find entity \"%s\" to affect\n", STRING( pev->classname ), STRING( pev->targetname ), STRING( pev->target ) );
		return;
	}
	if ( !Motion_TargetOk( pTarget ) )
		return;

	const BOOL debug = ( pev->spawnflags & SF_MOTION_DEBUG ) ? TRUE : FALSE;
	if ( debug )
		ALERT( at_console, "DEBUG: trigger_motion affects %s \"%s\":\n", STRING( pTarget->pev->classname ), STRING( pTarget->pev->targetname ) );

	Vector vecTemp = g_vecZero;
	Vector vecOld;

	if ( !FStringNull( m_iszPosition ) )
	{
		if ( m_iPosMode == POSMODE_ADDOFFSET )
		{
			if ( TryCalcLocus_Velocity( this, pActivator, STRING( m_iszPosition ), vecTemp ) )
			{
				vecOld = pTarget->pev->origin;
				UTIL_SetOrigin( pTarget->pev, vecOld + vecTemp );
				if ( debug )
					Motion_PrintVectors( "DEBUG: Set origin", vecOld, pTarget->pev->origin );
			}
		}
		else if ( TryCalcLocus_Position( this, pActivator, STRING( m_iszPosition ), vecTemp ) )
		{
			if ( debug )
				Motion_PrintVectors( "DEBUG: Set origin", pTarget->pev->origin, vecTemp );
			UTIL_SetOrigin( pTarget->pev, vecTemp );
		}
		pTarget->pev->flags &= ~FL_ONGROUND;
	}

	vecTemp = g_vecZero;
	if ( !FStringNull( m_iszAngles ) )
	{
		vecOld = pTarget->pev->angles;
		switch ( m_iAngMode )
		{
		case ANGMOD_SET:
			if ( TryCalcLocus_Velocity( this, pActivator, STRING( m_iszAngles ), vecTemp ) )
				pTarget->pev->angles = UTIL_VecToAngles( vecTemp );
			break;
		case ANGMOD_ROTATE:
			// Featureful read the velocity key here; the angles key is the one this mode names
			if ( TryCalcLocus_Velocity( this, pActivator, STRING( m_iszAngles ), vecTemp ) )
				pTarget->pev->angles = pTarget->pev->angles + UTIL_VecToAngles( vecTemp );
			break;
		case ANGMOD_ROTATE_BY_VALUES:
			Locus_StringToRandomVector( (float *)vecTemp, STRING( m_iszAngles ) );
			pTarget->pev->angles = pTarget->pev->angles + vecTemp;
			break;
		}
		if ( debug )
			Motion_PrintVectors( "DEBUG: Set angles", vecOld, pTarget->pev->angles );
	}

	vecTemp = g_vecZero;
	if ( !FStringNull( m_iszVelocity ) )
	{
		vecOld = pTarget->pev->velocity;
		switch ( m_iVelMode )
		{
		case VELMODE_SET:
			if ( TryCalcLocus_Velocity( this, pActivator, STRING( m_iszVelocity ), vecTemp ) )
				pTarget->pev->velocity = vecTemp;
			break;
		case VELMODE_ADD:
			if ( TryCalcLocus_Velocity( this, pActivator, STRING( m_iszVelocity ), vecTemp ) )
				pTarget->pev->velocity = vecOld + vecTemp;
			break;
		case VELMODE_ROTATE:
		case VELMODE_ROTATE_BY_VALUES:
		{
			Vector vecVelAngles;
			if ( m_iVelMode == VELMODE_ROTATE )
			{
				if ( !TryCalcLocus_Velocity( this, pActivator, STRING( m_iszVelocity ), vecTemp ) )
					break;
				vecVelAngles = UTIL_VecToAngles( vecTemp ) + UTIL_VecToAngles( vecOld );
			}
			else
			{
				Locus_StringToRandomVector( (float *)vecTemp, STRING( m_iszVelocity ) );
				vecVelAngles = vecTemp + UTIL_VecToAngles( vecOld );
			}
			UTIL_MakeVectors( vecVelAngles );
			pTarget->pev->velocity = vecOld.Length() * gpGlobals->v_forward;
			pTarget->pev->velocity.z = -pTarget->pev->velocity.z;	// VecToAngles reverses the pitch
			break;
		}
		}
		if ( debug )
			Motion_PrintVectors( "DEBUG: Set velocity", vecOld, pTarget->pev->velocity );
	}

	if ( !FStringNull( m_iszAVelocity ) )
	{
		vecTemp = g_vecZero;
		Locus_StringToRandomVector( (float *)vecTemp, STRING( m_iszAVelocity ) );
		vecOld = pTarget->pev->avelocity;
		pTarget->pev->avelocity = ( m_iAVelMode == AVELMODE_ADD ) ? vecOld + vecTemp : vecTemp;
		if ( debug )
			Motion_PrintVectors( "DEBUG: Set avelocity", vecOld, pTarget->pev->avelocity );
	}
}

//=========================================================
// motion_manager: keeps moving or turning its targets every frame, through one "motion_thread" helper per target,
// until it is turned off (or the target or a needed activator is gone). Position: 0 set position [LP], 1 offset
// position (a speed, units per second) [LV], 2 set velocity [LV], 3 accelerate by [LV], 4 follow position (a
// velocity towards it) [LP]. Facing: 0 face a direction [LV] (a zero vector leaves the angles), 1 rotate by a
// direction's angles per second [LV], 2 rotate by values per second (Y Z X), 3 set angular velocity (Y Z X), 4 set
// angles to an entity's angles [LA]. Flags: 1 Debug, 16 Obey Trigger Mode (On starts, Off stops, Toggle switches;
// without it On adds threads, Off removes them, Toggle restarts), 32 Start On (an unnamed one starts by itself).
// "Set position" on a brush players stand on jerks the riders (the server moves it, nothing predicts it).
//=========================================================
class CMotionThread : public CPointEntity
{
public:
	enum { POSMODE_SET = 0, POSMODE_OFFSET, POSMODE_SETVEL, POSMODE_ACCELERATE, POSMODE_FOLLOW };
	enum { FACEMODE_DIRECTION = 0, FACEMODE_ROTATE, FACEMODE_ROTATE_BY_VALUES, FACEMODE_SET_ANGULAR_VELOCITY, FACEMODE_SET_ANGLES };

	void Spawn( void );
	void EXPORT MotionThink( void );

	string_t m_iszPosition;
	int m_iPosMode;
	string_t m_iszFacing;
	int m_iFaceMode;
	EHANDLE m_hLocus;
	EHANDLE m_hTarget;
	BOOL m_reportedPosError;
	BOOL m_reportedDirError;
};

LINK_ENTITY_TO_CLASS( motion_thread, CMotionThread );

void CMotionThread::Spawn( void )
{
	SetThink( &CMotionThread::MotionThink );
	pev->classname = MAKE_STRING( "motion_thread" );
}

void CMotionThread::MotionThink( void )
{
	const BOOL debug = ( pev->spawnflags & SF_MOTION_DEBUG ) ? TRUE : FALSE;
	CBaseEntity *pTarget = m_hTarget;

	if ( ( m_hLocus == NULL && FBitSet( pev->spawnflags, SF_MOTION_REQUIRES_ACTIVATOR ) ) || !pTarget ||
		 ( pTarget->IsPlayer() && !Logic_IsPlayerInPlay( pTarget ) ) )
	{
		if ( debug )
			ALERT( at_console, "motion_thread \"%s\" expires\n", STRING( pev->targetname ) );
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time;
		return;
	}
	pev->nextthink = gpGlobals->time;	// every frame

	Vector vecTemp = g_vecZero;
	Vector vecOld;

	if ( !FStringNull( m_iszPosition ) )
	{
		switch ( m_iPosMode )
		{
		case POSMODE_SET:
			if ( TryCalcLocus_Position( this, m_hLocus, STRING( m_iszPosition ), vecTemp, !m_reportedPosError ) )
			{
				if ( debug && vecTemp != pTarget->pev->origin )
					Motion_PrintVectors( "DEBUG: Set origin", pTarget->pev->origin, vecTemp );
				UTIL_SetOrigin( pTarget->pev, vecTemp );
			}
			else
				m_reportedPosError = TRUE;
			pTarget->pev->flags &= ~FL_ONGROUND;
			break;
		case POSMODE_OFFSET:
			if ( TryCalcLocus_Velocity( this, m_hLocus, STRING( m_iszPosition ), vecTemp, !m_reportedPosError ) )
			{
				vecOld = pTarget->pev->origin;
				UTIL_SetOrigin( pTarget->pev, vecOld + gpGlobals->frametime * vecTemp );
				if ( debug && vecOld != pTarget->pev->origin )
					Motion_PrintVectors( "DEBUG: Set origin", vecOld, pTarget->pev->origin );
			}
			else
				m_reportedPosError = TRUE;
			pTarget->pev->flags &= ~FL_ONGROUND;
			break;
		case POSMODE_SETVEL:
			if ( TryCalcLocus_Velocity( this, m_hLocus, STRING( m_iszPosition ), vecTemp, !m_reportedPosError ) )
				pTarget->pev->velocity = vecTemp;
			else
				m_reportedPosError = TRUE;
			break;
		case POSMODE_ACCELERATE:
			if ( TryCalcLocus_Velocity( this, m_hLocus, STRING( m_iszPosition ), vecTemp, !m_reportedPosError ) )
				pTarget->pev->velocity = pTarget->pev->velocity + gpGlobals->frametime * vecTemp;
			else
				m_reportedPosError = TRUE;
			break;
		case POSMODE_FOLLOW:
			if ( TryCalcLocus_Position( this, m_hLocus, STRING( m_iszPosition ), vecTemp, !m_reportedPosError ) )
				pTarget->pev->velocity = vecTemp - pTarget->pev->origin;
			else
				m_reportedPosError = TRUE;
			break;
		}
	}

	if ( !FStringNull( m_iszFacing ) )
	{
		vecTemp = g_vecZero;
		vecOld = pTarget->pev->angles;
		switch ( m_iFaceMode )
		{
		case FACEMODE_DIRECTION:
			if ( TryCalcLocus_Velocity( this, m_hLocus, STRING( m_iszFacing ), vecTemp, !m_reportedDirError ) )
			{
				if ( vecTemp != g_vecZero )	// a zero vector leaves the angles
					pTarget->pev->angles = UTIL_VecToAngles( vecTemp );
			}
			else
				m_reportedDirError = TRUE;
			break;
		case FACEMODE_ROTATE:
			if ( TryCalcLocus_Velocity( this, m_hLocus, STRING( m_iszFacing ), vecTemp, !m_reportedDirError ) )
			{
				if ( vecTemp != g_vecZero )
					pTarget->pev->angles = pTarget->pev->angles + gpGlobals->frametime * UTIL_VecToAngles( vecTemp );
			}
			else
				m_reportedDirError = TRUE;
			break;
		case FACEMODE_ROTATE_BY_VALUES:
			Locus_StringToRandomVector( (float *)vecTemp, STRING( m_iszFacing ) );
			pTarget->pev->angles = pTarget->pev->angles + gpGlobals->frametime * vecTemp;
			break;
		case FACEMODE_SET_ANGULAR_VELOCITY:
			Locus_StringToRandomVector( (float *)vecTemp, STRING( m_iszFacing ) );
			pTarget->pev->avelocity = vecTemp;
			break;
		case FACEMODE_SET_ANGLES:
		{
			CBaseEntity *pCalc = Locus_FindEntity( NULL, STRING( m_iszFacing ), m_hLocus );
			if ( pCalc )
				pTarget->pev->angles = pCalc->pev->angles;
			else if ( !m_reportedDirError )
			{
				ALERT( at_console, "motion_manager: could not find \"%s\" to set angles for \"%s\"\n", STRING( m_iszFacing ), STRING( pTarget->pev->targetname ) );
				m_reportedDirError = TRUE;
			}
			break;
		}
		}
		if ( debug && vecOld != pTarget->pev->angles )
			Motion_PrintVectors( "DEBUG: Set angles", vecOld, pTarget->pev->angles );
	}
}

#define SF_MOTION_MANAGER_OBEY_TRIGGER_MODE	16
#define SF_MOTION_MANAGER_START_ON			32

class CMotionManager : public CPointEntity
{
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );
	void Affect( CBaseEntity *pTarget, CBaseEntity *pActivator );
	void AffectTargets( CBaseEntity *pActivator );
	void Activate( void );
	void UpdateOnRemove( void );
	void RemoveThreads( void );

	string_t m_iszPosition;
	int m_iPosMode;
	string_t m_iszFacing;
	int m_iFaceMode;
	BOOL m_activated;
	BOOL m_active;
};

LINK_ENTITY_TO_CLASS( motion_manager, CMotionManager );

void CMotionManager::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszPosition" ) )
		m_iszPosition = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iPosMode" ) )
		m_iPosMode = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszFacing" ) )
		m_iszFacing = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iFaceMode" ) )
		m_iFaceMode = atoi( pkvd->szValue );
	else
	{
		CPointEntity::KeyValue( pkvd );
		return;
	}
	pkvd->fHandled = TRUE;
}

void CMotionManager::Activate( void )
{
	if ( m_activated )
		return;
	m_activated = TRUE;
	if ( FStringNull( pev->targetname ) || FBitSet( pev->spawnflags, SF_MOTION_MANAGER_START_ON ) )
		Use( this, this, USE_ON, 0 );
}

void CMotionManager::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FBitSet( pev->spawnflags, SF_MOTION_MANAGER_OBEY_TRIGGER_MODE ) )
	{
		if ( ShouldToggle( useType, m_active ) )
		{
			if ( m_active )
			{
				m_active = FALSE;
				RemoveThreads();
			}
			else
			{
				m_active = TRUE;
				AffectTargets( pActivator );
			}
		}
		return;
	}
	if ( useType == USE_OFF || useType == USE_TOGGLE )
		RemoveThreads();
	if ( useType == USE_ON || useType == USE_TOGGLE )
		AffectTargets( pActivator );
}

void CMotionManager::AffectTargets( CBaseEntity *pActivator )
{
	if ( FStringNull( pev->target ) )
		return;
	CBaseEntity *pTarget = Locus_FindEntity( NULL, STRING( pev->target ), pActivator );
	if ( !pTarget )
	{
		ALERT( at_console, "motion_manager \"%s\" can't find entity \"%s\" to affect\n", STRING( pev->targetname ), STRING( pev->target ) );
		return;
	}
	int iGuard = 0;
	while ( pTarget && iGuard++ < 64 )
	{
		if ( Motion_TargetOk( pTarget ) )
			Affect( pTarget, pActivator );
		pTarget = Locus_FindEntity( pTarget, STRING( pev->target ), pActivator );
	}
}

void CMotionManager::Affect( CBaseEntity *pTarget, CBaseEntity *pActivator )
{
	if ( pev->spawnflags & SF_MOTION_DEBUG )
		ALERT( at_console, "DEBUG: Creating motion_thread \"%s\" for %s \"%s\". Time: %g\n", STRING( pev->targetname ), STRING( pTarget->pev->classname ), STRING( pTarget->pev->targetname ), gpGlobals->time );

	CMotionThread *pThread = GetClassPtr( (CMotionThread *)NULL );
	if ( !pThread )
		return;
	pThread->pev->owner = edict();
	pThread->Spawn();
	pThread->pev->targetname = pev->targetname;
	pThread->m_hLocus = pActivator;
	pThread->m_hTarget = pTarget;
	pThread->m_iszPosition = m_iszPosition;
	pThread->m_iPosMode = m_iPosMode;
	pThread->m_iszFacing = m_iszFacing;
	pThread->m_iFaceMode = m_iFaceMode;
	pThread->pev->spawnflags = pev->spawnflags;
	pThread->pev->nextthink = gpGlobals->time;
	if ( Motion_IsActivatorName( m_iszPosition ) || Motion_IsActivatorName( m_iszFacing ) )
		pThread->pev->spawnflags |= SF_MOTION_REQUIRES_ACTIVATOR;
}

void CMotionManager::UpdateOnRemove( void )
{
	RemoveThreads();
	CPointEntity::UpdateOnRemove();
}

void CMotionManager::RemoveThreads( void )
{
	CBaseEntity *pThread = NULL;
	while ( ( pThread = UTIL_FindEntityByClassname( pThread, "motion_thread" ) ) != NULL )
	{
		if ( pThread->pev->owner == edict() )
		{
			pThread->SetThink( &CBaseEntity::SUB_Remove );
			pThread->pev->nextthink = gpGlobals->time;
		}
	}
}
