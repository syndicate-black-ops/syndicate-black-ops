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
//
// func_vehicle / func_vehiclecontrols: Counter-Strike drivable vehicles.
//
// Ported from the HL25 SDK (ValveSoftware/halflife 0fc8913, dlls/vehicle.cpp) as cleaned up by
// halflife-updated (dlls/vehicle.cpp). SBO changes are marked "SBO:"; the client-side engine sound
// event is EV_VehiclePitchAdjust in cl_dll/ev_hldm.cpp (halflife-updated af9952e, halflife #3746).
//

#include <math.h>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "trains.h"
#include "saverestore.h"
#include "player.h"

static float Fix2( float angle )
{
	while ( angle < 0 )
		angle += 360;
	while ( angle > 360 )
		angle -= 360;

	return angle;
}

static void FixupAngles2( Vector &v )
{
	v.x = Fix2( v.x );
	v.y = Fix2( v.y );
	v.z = Fix2( v.z );
}

#define VEHICLE_STARTPITCH	60
#define VEHICLE_MAXPITCH	200
#define VEHICLE_MAXSPEED	1500	// approx max speed for sound pitch calculation

#define VEHICLE_DEFAULT_BRAKE_SOUND		"plats/vehicle_brake1.wav"
#define VEHICLE_DEFAULT_DECELERATION	20		// speed lost every 0.1 s without throttle (Half-Life)

//
// SBO 2.0, from Featureful (dlls/vehicle.cpp CFuncVehicleProxy, Half-Life 1 SDK license): func_vehicle "touch_proxy_name"
// makes one of these at the vehicle's origin. A brush entity touches no triggers; this point entity follows the
// vehicle and does (triggers that take "everything else" or name it in "netname": trigger_multiple / trigger_once
// flag 8, trigger_inout, trigger_jump). Not saved; removed with its vehicle.
//
class CFuncVehicleProxy : public CPointEntity
{
public:
	void Spawn( void );
	int ObjectCaps( void ) { return CPointEntity::ObjectCaps() | FCAP_DONT_SAVE; }
	void EXPORT ProxyThink( void );
};

LINK_ENTITY_TO_CLASS( func_vehicle_proxy, CFuncVehicleProxy );

void CFuncVehicleProxy::Spawn( void )
{
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
	UTIL_SetOrigin( pev, pev->origin );

	if ( FNullEnt( pev->owner ) )
	{
		REMOVE_ENTITY( edict() );
		return;
	}

	SetThink( &CFuncVehicleProxy::ProxyThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CFuncVehicleProxy::ProxyThink( void )
{
	CBaseEntity *pVehicle = FNullEnt( pev->owner ) ? NULL : CBaseEntity::Instance( pev->owner );
	if ( !pVehicle )
	{
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	pev->velocity = pVehicle->pev->velocity;
	UTIL_SetOrigin( pev, pVehicle->pev->origin );
	pev->nextthink = gpGlobals->time + 0.05;
}

TYPEDESCRIPTION CFuncVehicle::m_SaveData[] =
{
	DEFINE_FIELD( CFuncVehicle, m_ppath, FIELD_CLASSPTR ),
	DEFINE_FIELD( CFuncVehicle, m_length, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncVehicle, m_width, FIELD_FLOAT ),			// SBO: not saved by HL25
	DEFINE_FIELD( CFuncVehicle, m_height, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncVehicle, m_speed, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncVehicle, m_dir, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncVehicle, m_startSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncVehicle, m_controlMins, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncVehicle, m_controlMaxs, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncVehicle, m_sounds, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncVehicle, m_acceleration, FIELD_INTEGER ),	// SBO: not saved by HL25
	DEFINE_FIELD( CFuncVehicle, m_flVolume, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncVehicle, m_flBank, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncVehicle, m_oldSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncVehicle, m_reverseSpeed, FIELD_FLOAT ),			// SBO 2.0 (Featureful)
	DEFINE_FIELD( CFuncVehicle, m_deceleration, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncVehicle, m_stopSoundWhenAtHalt, FIELD_BOOLEAN ),
	DEFINE_FIELD( CFuncVehicle, m_touchProxyName, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CFuncVehicle, CBaseEntity );
LINK_ENTITY_TO_CLASS( func_vehicle, CFuncVehicle );

void CFuncVehicle::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "length" ) )
	{
		m_length = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "width" ) )
	{
		m_width = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "height" ) )
	{
		m_height = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "startspeed" ) )
	{
		m_startSpeed = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sounds" ) )
	{
		m_sounds = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "volume" ) )
	{
		m_flVolume = (float)( atoi( pkvd->szValue ) );
		m_flVolume *= 0.1;

		// SBO (as ReGameDLL_CS): the client event packs the volume into 6 bits (volume * 40), keep it in range
		if ( m_flVolume > 1.0 )
			m_flVolume = 1.0;
		else if ( m_flVolume < 0.0 )
			m_flVolume = 0.0;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "bank" ) )
	{
		m_flBank = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "acceleration" ) )
	{
		m_acceleration = atoi( pkvd->szValue );
		if ( m_acceleration < 1 )
			m_acceleration = 1;
		else if ( m_acceleration > 10 )
			m_acceleration = 10;
		pkvd->fHandled = TRUE;
	}
	// SBO 2.0, from Featureful
	else if ( FStrEq( pkvd->szKeyName, "reverse_speed" ) )
	{
		m_reverseSpeed = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "deceleration" ) )
	{
		m_deceleration = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "stop_sound_at_halt" ) )
	{
		m_stopSoundWhenAtHalt = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "brake_sound" ) || FStrEq( pkvd->szKeyName, "ignition_sound" )
		|| FStrEq( pkvd->szKeyName, "touch_proxy_name" ) )
	{
		// SBO: read at spawn only; a sound named at run time (trigger_changevalue) would not be precached
		if ( !FStringNull( pev->noise ) )
			ALERT( at_console, "func_vehicle \"%s\": key \"%s\" can't be changed at run time\n", STRING( pev->targetname ), pkvd->szKeyName );
		else if ( FStrEq( pkvd->szKeyName, "brake_sound" ) )
			pev->noise1 = ALLOC_STRING( pkvd->szValue );
		else if ( FStrEq( pkvd->szKeyName, "ignition_sound" ) )
			pev->noise3 = ALLOC_STRING( pkvd->szValue );	// played by CBasePlayer::PlayerUse
		else
			m_touchProxyName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CFuncVehicle::NextThink( float thinkTime, BOOL alwaysThink )
{
	if ( alwaysThink )
		pev->flags |= FL_ALWAYSTHINK;
	else
		pev->flags &= ~FL_ALWAYSTHINK;

	pev->nextthink = thinkTime;
}

void CFuncVehicle::Blocked( CBaseEntity *pOther )
{
	entvars_t *pevOther = pOther->pev;

	// Blocker is on-ground on the vehicle
	if ( FBitSet( pevOther->flags, FL_ONGROUND ) && VARS( pevOther->groundentity ) == pev )
	{
		// HL25 had unreachable train code after this return (push the rider up by the vehicle speed)
		pevOther->velocity = pev->velocity;
		return;
	}
	else
	{
		pevOther->velocity = ( pevOther->origin - pev->origin ).Normalize() * pev->dmg;
		pevOther->velocity.z += 300;

		// slow down vehicle
		pev->velocity = pev->velocity * 0.85;
	}

	// SBO: at_aiconsole instead of at_console, this fires every frame while something blocks the vehicle
	ALERT( at_aiconsole, "VEHICLE(%s): Blocked by %s (dmg:%.2f)\n", STRING( pev->targetname ), STRING( pOther->pev->classname ), pev->dmg );

	UTIL_MakeVectors( pev->angles );
	Vector vFrontLeft, vFrontRight, vBackLeft, vBackRight;
	Vector forward = ( gpGlobals->v_forward * -1 ) * ( m_length / 2 );	// for some stupid reason I have to invert this...
	Vector right = ( gpGlobals->v_right * -1 ) * ( m_width / 2 );

	vFrontLeft = pev->origin + forward - right;
	vFrontRight = pev->origin + forward + right;
	vBackLeft = pev->origin - forward - right;
	vBackRight = pev->origin - forward + right;

	Vector vOrigin = pOther->pev->origin;

	float minx, miny, minz;
	float maxx, maxy, maxz;

	minx = fmin( vFrontLeft.x, vBackRight.x );
	maxx = fmax( vFrontLeft.x, vBackRight.x );
	miny = fmin( vFrontLeft.y, vBackRight.y );
	maxy = fmax( vFrontLeft.y, vBackRight.y );
	minz = pev->origin.z;
	maxz = pev->origin.z + fabs( pev->mins.z - pev->maxs.z ) * 2;

	// Check if the target is out of the bounds of the vehicle...
	// (attacker = the vehicle; CBasePlayer::Killed credits the kill to the driver, as HL25's PlayerKilled does)
	if ( ( ( vOrigin.x < minx ) || ( vOrigin.x > maxx ) ) || ( ( vOrigin.y < miny ) || ( vOrigin.y > maxy ) ) || ( ( vOrigin.z < minz ) || ( vOrigin.z > maxz ) ) )
		pOther->TakeDamage( pev, pev, 150, DMG_CRUSH );
}

void CFuncVehicle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType != USE_SET )
	{
		if ( !ShouldToggle( useType, ( pev->speed != 0 ) ) )
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
		if ( value < 10 )	// It's either forward or backward
		{
			if ( ( value < 0 ) && ( pev->speed > 145 ) )
			{
				StopSound();
			}

			float delta = value;
			float flSpeedRatio;

			// SBO 2.0 (Featureful): "reverse_speed": braking and reversing scale to it, down to -1 x it;
			// without it to -0.35 x the forward speed (Half-Life)
			const BOOL fCustomReverse = m_reverseSpeed > 0;
			const float flReverseSpeed = fCustomReverse ? m_reverseSpeed : m_speed;

			if ( delta > 0 )	// accelerating
			{
				flSpeedRatio = ( pev->speed / m_speed );

				if ( pev->speed < 0 )
					delta = flSpeedRatio + ( 0.015 / 3 ) + ( m_acceleration * 0.0005 );
				else if ( pev->speed < 10 )
					delta = flSpeedRatio + ( 0.015 / 7 ) + ( m_acceleration * 0.0006 );
				else if ( pev->speed < 20 )
					delta = flSpeedRatio + ( 0.02 / 6 ) + ( m_acceleration * 0.0007 );
				else if ( pev->speed < 30 )
					delta = flSpeedRatio + ( 0.025 / 6 ) + ( m_acceleration * 0.0007 );
				else if ( pev->speed < 45 )
					delta = flSpeedRatio + ( 0.02 / 5 ) + ( m_acceleration * 0.0007 );
				else if ( pev->speed < 60 )
					delta = flSpeedRatio + ( 0.019 / 5 ) + ( m_acceleration * 0.0008 );
				else if ( pev->speed < 80 )
					delta = flSpeedRatio + ( 0.018 / 4 ) + ( m_acceleration * 0.0008 );
				else if ( pev->speed < 100 )
					delta = flSpeedRatio + ( 0.017 / 4 ) + ( m_acceleration * 0.0009 );
				else if ( pev->speed < 150 )
					delta = flSpeedRatio + ( 0.016 / 6 ) + ( m_acceleration * 0.0008 );
				else if ( pev->speed < 225 )
					delta = flSpeedRatio + ( 0.016 / 7 ) + ( m_acceleration * 0.0007 );
				else if ( pev->speed < 300 )
					delta = flSpeedRatio + ( 0.015 / 8 ) + ( m_acceleration * 0.0006 );
				else if ( pev->speed < 400 )
					delta = flSpeedRatio + ( 0.013 / 9 ) + ( m_acceleration * 0.0005 );
				else if ( pev->speed < 550 )
					delta = flSpeedRatio + ( 0.012 / 10 ) + ( m_acceleration * 0.0005 );
				else if ( pev->speed < 800 )
					delta = flSpeedRatio + ( 0.011 / 12 ) + ( m_acceleration * 0.0005 );
			}
			else if ( delta < 0 )	// braking
			{
				flSpeedRatio = pev->speed / flReverseSpeed;

				if ( flSpeedRatio > 0 )
					delta = flSpeedRatio - 0.05 / 4;
				else if ( ( flSpeedRatio <= 0 ) && ( flSpeedRatio > -0.05 ) )	// Just started accelerating
					delta = flSpeedRatio - 0.03 / 4;
				else if ( ( flSpeedRatio <= 0.05 ) && ( flSpeedRatio > -0.1 ) )
					delta = flSpeedRatio - 0.04 / 4;
				else if ( ( flSpeedRatio <= 0.15 ) && ( flSpeedRatio > -0.15 ) )
					delta = flSpeedRatio - 0.05 / 4;
				else if ( ( flSpeedRatio <= 0.15 ) && ( flSpeedRatio > -0.22 ) )
					delta = flSpeedRatio - 0.055 / 4;
				else if ( ( flSpeedRatio <= 0.22 ) && ( flSpeedRatio > -0.3 ) )
					delta = flSpeedRatio - 0.07 / 4;
				else if ( flSpeedRatio <= 0.3 )
					delta = flSpeedRatio - 0.05 / 4;
			}

			if ( delta > 1 )
				delta = 1;
			else if ( fCustomReverse ? delta < -1 : delta < -0.35 )
				delta = fCustomReverse ? -1 : -0.35;

			if ( value < 0 && fCustomReverse )
				pev->speed = flReverseSpeed * delta;
			else
				pev->speed = m_speed * delta;

			Next();

			m_flAcceleratorDecay = gpGlobals->time + 0.25;
		}
		else
		{
			if ( m_flCanTurnNow < gpGlobals->time )
			{
				if ( value == 20 )	// turn left!
				{
					m_iTurnAngle += 1;
					m_flSteeringWheelDecay = gpGlobals->time + 0.075;
					if ( m_iTurnAngle > 8 )
						m_iTurnAngle = 8;
				}
				else if ( value == 30 )	// turn right
				{
					m_iTurnAngle -= 1;
					m_flSteeringWheelDecay = gpGlobals->time + 0.075;
					if ( m_iTurnAngle < -8 )
						m_iTurnAngle = -8;
				}

				m_flCanTurnNow = gpGlobals->time + 0.05;
			}
		}
	}
}

void CFuncVehicle::StopSound( void )
{
	// if sound playing, stop it
	if ( m_soundPlaying && pev->noise )
	{
		// SBO 2.0 (Featureful 6e82302b): the sound number as it is (EV_VehiclePitchAdjust); Featureful kept the
		// packed form here, which the client no longer reads, so its stop never stopped the engine sound
		PLAYBACK_EVENT_FULL( FEV_RELIABLE | FEV_UPDATE, edict(), m_usAdjustPitch, 0.0,
			(float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, m_sounds, 0, 1, 0 );
	}

	m_soundPlaying = 0;
}

// update pitch based on speed, start sound if not playing
// NOTE: when vehicle goes through transition, m_soundPlaying should go to 0,
// which will cause the looped sound to restart.
void CFuncVehicle::UpdateSound( void )
{
	float flpitch;

	if ( !pev->noise )
		return;

	// SBO: fabs, HL25's abs() truncated the speed to an int
	flpitch = VEHICLE_STARTPITCH + ( fabs( pev->speed ) * ( VEHICLE_MAXPITCH - VEHICLE_STARTPITCH ) / VEHICLE_MAXSPEED );

	if ( flpitch > VEHICLE_MAXPITCH )
		flpitch = VEHICLE_MAXPITCH;

	if ( !m_soundPlaying )
	{
		// play startup sound for vehicle; SBO 2.0 (Featureful): "brake_sound" replaces it and plays for every engine sound
		if ( m_sounds < 5 || !FStringNull( pev->noise1 ) )
			EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, FStringNull( pev->noise1 ) ? VEHICLE_DEFAULT_BRAKE_SOUND : STRING( pev->noise1 ), m_flVolume, ATTN_NORM, 0, 100 );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_STATIC, (char *)STRING( pev->noise ), m_flVolume, ATTN_NORM, 0, (int)flpitch );
		m_soundPlaying = 1;
	}
	else
	{
		// SBO 2.0 (Featureful 6e82302b): volume in fparam1, sound number in iparam1, pitch in iparam2. The HL SDK
		// packed them into one 15-bit iparam1 (pitch / 10 in 6 bits), so the pitch moved in steps of 10 and the
		// engine sound glitched at each step
		PLAYBACK_EVENT_FULL( FEV_UPDATE, edict(), m_usAdjustPitch, 0.0,
			(float *)&g_vecZero, (float *)&g_vecZero, m_flVolume, 0.0, m_sounds, (int)flpitch, 0, 0 );
	}
}

void CFuncVehicle::CheckTurning( void )
{
	float maxspeed;
	TraceResult tr;

	// Check to see if he's trying to turn into a wall
	if ( m_iTurnAngle < 0 )	// He's trying to turn right
	{
		if ( pev->speed > 0 )
		{
			UTIL_TraceLine( m_vFrontRight, m_vFrontRight - ( gpGlobals->v_right * 16 ), ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );
			if ( tr.flFraction != 1.0 )
			{
				m_iTurnAngle = 1;
			}
		}
		else if ( pev->speed < 0 )
		{
			UTIL_TraceLine( m_vBackLeft, m_vBackLeft + ( gpGlobals->v_right * 16 ), ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );
			if ( tr.flFraction != 1.0 )
			{
				m_iTurnAngle = 1;
			}
		}
	}
	else if ( m_iTurnAngle > 0 )
	{
		if ( pev->speed > 0 )
		{
			UTIL_TraceLine( m_vFrontLeft, m_vFrontLeft + ( gpGlobals->v_right * 16 ), ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );
			if ( tr.flFraction != 1.0 )
			{
				m_iTurnAngle = -1;
			}
		}
		else if ( pev->speed < 0 )
		{
			UTIL_TraceLine( m_vBackRight, m_vBackRight - ( gpGlobals->v_right * 16 ), ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );
			if ( tr.flFraction != 1.0 )
			{
				m_iTurnAngle = -1;
			}
		}
	}

	if ( pev->speed > 0 )
	{
		// The time steps below are as in HL25/CS (only the first two can ever match).
		maxspeed = m_speed;

		if ( abs( m_iTurnAngle ) > 4 )	// Doing a hard turn
		{
			if ( m_flTurnStartTime == -1 )	// just started turning
			{
				m_flTurnStartTime = gpGlobals->time;
				maxspeed = (float)m_speed;
			}
			else if ( ( gpGlobals->time - m_flTurnStartTime ) >= 0.0 )
			{
				maxspeed = (float)m_speed * 0.98;
			}
			else if ( ( gpGlobals->time - m_flTurnStartTime ) > 0.3 )
			{
				maxspeed = (float)m_speed * 0.95;
			}
			else if ( ( gpGlobals->time - m_flTurnStartTime ) > 0.6 )
			{
				maxspeed = (float)m_speed * 0.9;
			}
			else if ( ( gpGlobals->time - m_flTurnStartTime ) > 0.8 )
			{
				maxspeed = (float)m_speed * 0.8;
			}
			else if ( ( gpGlobals->time - m_flTurnStartTime ) > 1 )
			{
				maxspeed = (float)m_speed * 0.7;
			}
			else if ( ( gpGlobals->time - m_flTurnStartTime ) > 1.2 )
			{
				maxspeed = (float)m_speed * 0.5;
			}
		}
		else if ( abs( m_iTurnAngle ) > 2 )
		{
			m_flTurnStartTime = -1;
			maxspeed = (float)m_speed * 0.9;
		}
		else
		{
			maxspeed = (float)m_speed;
			m_flTurnStartTime = -1;
		}

		if ( pev->speed > maxspeed )
		{
			pev->speed -= m_speed / 10;
		}
	}
}

// One probe of CollisionDetection(): skid along steep walls, bounce off the rest, follow walkable slopes.
// bForward: the vehicle is moving forward (HL25's two blocks only differ by the sign of the wall test).
static void VehicleProbeHit( CFuncVehicle *pVehicle, const TraceResult &tr, bool bForward )
{
	entvars_t *pev = pVehicle->pev;
	float flDot = DotProduct( tr.vecPlaneNormal, gpGlobals->v_forward * -1 );

	if ( ( bForward ? ( flDot > -0.7 ) : ( flDot < 0.7 ) ) && ( tr.vecPlaneNormal.z < 0.1 ) )	// slightly vertical wall... skid along it
	{
		pVehicle->m_vSurfaceNormal = tr.vecPlaneNormal;
		pVehicle->m_vSurfaceNormal.z = 0;
		pev->speed = pev->speed * 0.99;
	}
	else if ( ( tr.vecPlaneNormal.z < 0.65 ) || ( tr.fStartSolid ) )
		pev->speed = pev->speed * -1;
	else
		pVehicle->m_vSurfaceNormal = tr.vecPlaneNormal;
}

void CFuncVehicle::CollisionDetection( void )
{
	TraceResult tr;

	// Check to see if there's anything in front of us
	if ( pev->speed < 0 )	// We're going backwards
	{
		// Check back left, back right, then back center portion of vehicle if we hit a wall
		UTIL_TraceLine( m_vBackLeft, m_vBackLeft + ( gpGlobals->v_forward * 16 ), ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );
		if ( tr.flFraction != 1.0 )
		{
			VehicleProbeHit( this, tr, false );

			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( pHit && ( pHit->Classify() == CLASS_VEHICLE ) )
				ALERT( at_aiconsole, "I hit another vehicle\n" );
			return;
		}

		UTIL_TraceLine( m_vBackRight, m_vBackRight + ( gpGlobals->v_forward * 16 ), ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );
		if ( tr.flFraction != 1.0 )
		{
			VehicleProbeHit( this, tr, false );
			return;
		}

		UTIL_TraceLine( m_vBack, m_vBack + ( gpGlobals->v_forward * 16 ), ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );
		if ( tr.flFraction != 1.0 )
			VehicleProbeHit( this, tr, false );
	}
	else if ( pev->speed > 0 )
	{
		// Check front left (also against monsters/players, as HL25), front right, then front center
		UTIL_TraceLine( m_vFrontLeft, m_vFrontLeft - ( gpGlobals->v_forward * 16 ), dont_ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );
		if ( tr.flFraction != 1.0 )
		{
			VehicleProbeHit( this, tr, true );
			return;
		}

		UTIL_TraceLine( m_vFrontRight, m_vFrontRight - ( gpGlobals->v_forward * 16 ), ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );
		if ( tr.flFraction != 1.0 )
		{
			VehicleProbeHit( this, tr, true );
			return;
		}

		UTIL_TraceLine( m_vFront, m_vFront - ( gpGlobals->v_forward * 16 ), ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );
		if ( tr.flFraction != 1.0 )
			VehicleProbeHit( this, tr, true );
	}
}

void CFuncVehicle::TerrainFollowing( void )
{
	TraceResult tr;

	// Get the normal of the surface we're right on top of
	UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, -1 * ( m_height + 48 ) ), ignore_monsters, dont_ignore_glass, ENT( pev ), &tr );
	if ( tr.flFraction != 1.0 )	// we hit something
	{
		m_vSurfaceNormal = tr.vecPlaneNormal;
	}
	else if ( tr.fInWater )
	{
		m_vSurfaceNormal = Vector( 0, 0, 1 );
	}
}

void CFuncVehicle::Next( void )
{
	Vector vGravityVector = g_vecZero;

	UTIL_MakeVectors( pev->angles );

	Vector forward = ( gpGlobals->v_forward * -1 ) * ( m_length / 2 );	// for some stupid reason I have to invert this...
	Vector right = ( gpGlobals->v_right * -1 ) * ( m_width / 2 );
	Vector up = ( gpGlobals->v_up ) * 16;

	m_vFrontLeft = pev->origin + forward - right + up;
	m_vFrontRight = pev->origin + forward + right + up;
	m_vFront = pev->origin + forward + up;
	m_vBackLeft = pev->origin - forward - right + up;
	m_vBackRight = pev->origin - forward + right + up;
	m_vBack = pev->origin - forward + up;

	m_vSurfaceNormal = g_vecZero;

	// Check to see if we're hitting something...and alter the vehicles velocity.
	CheckTurning();

	float time = 0.1;

	if ( m_flSteeringWheelDecay < gpGlobals->time )
	{
		m_flSteeringWheelDecay = gpGlobals->time + 0.1;
		if ( m_iTurnAngle < 0 )
			m_iTurnAngle++;
		else if ( m_iTurnAngle > 0 )
			m_iTurnAngle--;
	}

	if ( m_flAcceleratorDecay < gpGlobals->time )
	{
		m_flAcceleratorDecay = gpGlobals->time + 0.1;
		// SBO 2.0 (Featureful): "deceleration"
		const int deceleration = m_deceleration > 0 ? m_deceleration : VEHICLE_DEFAULT_DECELERATION;
		if ( pev->speed < 0 )
		{
			pev->speed += deceleration;
			if ( pev->speed > 0 )
				pev->speed = 0;
		}
		else if ( pev->speed > 0 )
		{
			pev->speed -= deceleration;
			if ( pev->speed < 0 )
				pev->speed = 0;
		}
	}

	if ( !pev->speed )
	{
		m_iTurnAngle = 0;
		pev->avelocity = g_vecZero;
		pev->velocity = g_vecZero;

		SetThink( &CFuncVehicle::Next );
		NextThink( pev->ltime + time, TRUE );

		// SBO 2.0 (Featureful): "stop_sound_at_halt": the engine sound stops while it stands with nobody at the
		// wheel (it starts again with the next move)
		if ( m_stopSoundWhenAtHalt && !GetDriver() )
			StopSound();
		return;
	}

	TerrainFollowing();
	CollisionDetection();

	if ( m_vSurfaceNormal != g_vecZero )
	{
		m_vVehicleDirection = CrossProduct( m_vSurfaceNormal, gpGlobals->v_forward );
		m_vVehicleDirection = CrossProduct( m_vSurfaceNormal, m_vVehicleDirection );

		Vector vTargetAngle, vAngle;
		float vx, vy;

		vTargetAngle = UTIL_VecToAngles( m_vVehicleDirection );
		vTargetAngle.y += 180;
		vAngle = pev->angles;

		if ( m_iTurnAngle != 0 )
			vTargetAngle.y += m_iTurnAngle;

		FixupAngles2( vTargetAngle );
		FixupAngles2( vAngle );

		vx = UTIL_AngleDistance( vTargetAngle.x, vAngle.x );
		vy = UTIL_AngleDistance( vTargetAngle.y, vAngle.y );

		if ( vx > 10 )
			vx = 10;
		else if ( vx < -10 )
			vx = -10;
		if ( vy > 10 )
			vy = 10;
		else if ( vy < -10 )
			vy = -10;
		pev->avelocity.y = (int)( vy * 10 );
		pev->avelocity.x = (int)( vx * 10 );

		// reset launch time to -1
		m_flLaunchTime = -1;
		m_flLastNormalZ = m_vSurfaceNormal.z;
	}
	else
	{
		time = 0.1;
		if ( m_flLaunchTime == -1 )
		{
			m_flLaunchTime = gpGlobals->time;
			vGravityVector = Vector( 0, 0, 0 );
			pev->velocity = pev->velocity * 1.5;
		}
		else
		{
			float flAirTime;

			flAirTime = gpGlobals->time - m_flLaunchTime;

			vGravityVector = Vector( 0, 0, flAirTime * -35 );
			if ( vGravityVector.z < -400 )
				vGravityVector.z = -400;
		}

		m_vVehicleDirection = gpGlobals->v_forward * -1;
	}

	if ( m_flUpdateSound < gpGlobals->time )
	{
		UpdateSound();
		m_flUpdateSound = gpGlobals->time + 1;
	}

	if ( m_vSurfaceNormal != g_vecZero )
	{
		pev->velocity = pev->speed * m_vVehicleDirection.Normalize();
	}
	else
	{
		pev->velocity = pev->velocity + vGravityVector;
	}

	SetThink( &CFuncVehicle::Next );
	NextThink( pev->ltime + time, TRUE );
}

void CFuncVehicle::DeadEnd( void )
{
	// Fire the dead-end target if there is one
	CPathTrack *pTrack, *pNext;

	pTrack = m_ppath;

	ALERT( at_aiconsole, "VEHICLE(%s): Dead end ", STRING( pev->targetname ) );
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
		ALERT( at_aiconsole, "at %s\n", STRING( pTrack->pev->targetname ) );
		if ( pTrack->pev->netname )
			FireTargets( STRING( pTrack->pev->netname ), this, this, USE_TOGGLE, 0 );
	}
	else
		ALERT( at_aiconsole, "\n" );
}

void CFuncVehicle::SetControls( entvars_t *pevControls )
{
	Vector offset = pevControls->origin - pev->oldorigin;

	m_controlMins = pevControls->mins + offset;
	m_controlMaxs = pevControls->maxs + offset;
}

BOOL CFuncVehicle::OnControls( entvars_t *pevTest )
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

void CFuncVehicle::Find( void )
{
	m_ppath = CPathTrack::Instance( FIND_ENTITY_BY_TARGETNAME( NULL, STRING( pev->target ) ) );
	if ( !m_ppath )
		return;

	entvars_t *pevTarget = m_ppath->pev;
	if ( !FClassnameIs( pevTarget, "path_track" ) )
	{
		ALERT( at_console, "func_vehicle must be on a path of path_track\n" );
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
	// The vehicle actually points west
	pev->angles.y += 180;

	if ( pev->spawnflags & SF_TRACKTRAIN_NOPITCH )
		pev->angles.x = 0;
	UTIL_SetOrigin( pev, nextPos );
	NextThink( pev->ltime + 0.1, FALSE );
	SetThink( &CFuncVehicle::Next );
	pev->speed = m_startSpeed;

	UpdateSound();
}

void CFuncVehicle::NearestPath( void )
{
	CBaseEntity *pTrack = NULL;
	CBaseEntity *pNearest = NULL;
	float dist, closest;

	closest = 1024;

	while ( ( pTrack = UTIL_FindEntityInSphere( pTrack, pev->origin, 1024 ) ) != NULL )
	{
		// filter out non-tracks
		if ( !( pTrack->pev->flags & ( FL_CLIENT | FL_MONSTER ) ) && FClassnameIs( pTrack->pev, "path_track" ) )
		{
			dist = ( pev->origin - pTrack->pev->origin ).Length();
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
		SetThink( NULL );
		return;
	}

	ALERT( at_aiconsole, "VEHICLE: %s, Nearest track is %s\n", STRING( pev->targetname ), STRING( pNearest->pev->targetname ) );
	// If I'm closer to the next path_track on this path, then it's my real path
	pTrack = ( (CPathTrack *)pNearest )->GetNext();
	if ( pTrack )
	{
		if ( ( pev->origin - pTrack->pev->origin ).Length() < ( pev->origin - pNearest->pev->origin ).Length() )
			pNearest = pTrack;
	}

	m_ppath = (CPathTrack *)pNearest;

	if ( pev->speed != 0 )
	{
		NextThink( pev->ltime + 0.1, FALSE );
		SetThink( &CFuncVehicle::Next );
	}
}

void CFuncVehicle::OverrideReset( void )
{
	NextThink( pev->ltime + 0.1, FALSE );
	SetThink( &CFuncVehicle::NearestPath );
}

CFuncVehicle *CFuncVehicle::Instance( edict_t *pent )
{
	if ( pent && FClassnameIs( pent, "func_vehicle" ) )
		return (CFuncVehicle *)GET_PRIVATE( pent );
	return NULL;
}

int CFuncVehicle::Classify( void )
{
	return CLASS_VEHICLE;
}

void CFuncVehicle::SetDriver( CBasePlayer *pDriver )
{
	m_hDriver = pDriver;
}

void CFuncVehicle::ReleaseDriver( CBasePlayer *pDriver )
{
	// SBO: HL25 cleared m_pDriver whenever anyone on the controls let go, even if someone else drove
	if ( (CBaseEntity *)m_hDriver == (CBaseEntity *)pDriver )
		m_hDriver = NULL;
}

// The player credited with what the vehicle does (HL25: the kill, see CBasePlayer::Killed): the last
// player who took the controls, while still connected, alive and controlling this vehicle.
CBasePlayer *CFuncVehicle::GetDriver( void )
{
	CBaseEntity *pEntity = m_hDriver;

	if ( !pEntity || !pEntity->IsPlayer() || !pEntity->IsAlive() )
		return NULL;

	// has_disconnected: ClientDisconnect leaves the edict (and its flags) as they were
	CBasePlayer *pDriver = (CBasePlayer *)pEntity;
	if ( pDriver->has_disconnected || !( pDriver->m_afPhysicsFlags & PFLAG_ONTRAIN ) || pDriver->pev->groundentity != edict() )
		return NULL;

	return pDriver;
}

void CFuncVehicle::Spawn( void )
{
	if ( pev->speed == 0 )
		m_speed = 165;
	else
		m_speed = pev->speed;

	if ( m_sounds == 0 )
		m_sounds = 3;

	ALERT( at_aiconsole, "VEHICLE(%s): m_speed = %f\n", STRING( pev->targetname ), m_speed );

	pev->speed = 0;
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	pev->impulse = m_speed;

	// SBO: HL25/CS always forced 5 here, which made the "acceleration" keyvalue (1-10) dead
	if ( m_acceleration == 0 )
		m_acceleration = 5;

	m_dir = 1;
	m_flTurnStartTime = -1;

	if ( FStringNull( pev->target ) )
		ALERT( at_console, "func_vehicle with no target\n" );

	if ( pev->spawnflags & SF_TRACKTRAIN_PASSABLE )
		pev->solid = SOLID_NOT;
	else
		pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );

	// Cache off placed origin for vehicle controls
	pev->oldorigin = pev->origin;

	m_controlMins = pev->mins;
	m_controlMaxs = pev->maxs;
	m_controlMaxs.z += 72;
	// start vehicles on the next frame, to make sure their targets have had
	// a chance to spawn/activate
	NextThink( pev->ltime + 0.1, FALSE );
	SetThink( &CFuncVehicle::Find );
	Precache();
}

void CFuncVehicle::Precache( void )
{
	if ( m_flVolume == 0.0 )
		m_flVolume = 1.0;

	switch ( m_sounds )
	{
	case 1:
		PRECACHE_SOUND( "plats/vehicle1.wav" );
		pev->noise = MAKE_STRING( "plats/vehicle1.wav" );
		break;
	case 2:
		PRECACHE_SOUND( "plats/vehicle2.wav" );
		pev->noise = MAKE_STRING( "plats/vehicle2.wav" );
		break;
	case 3:
		PRECACHE_SOUND( "plats/vehicle3.wav" );
		pev->noise = MAKE_STRING( "plats/vehicle3.wav" );
		break;
	case 4:
		PRECACHE_SOUND( "plats/vehicle4.wav" );
		pev->noise = MAKE_STRING( "plats/vehicle4.wav" );
		break;
	case 5:
		PRECACHE_SOUND( "plats/vehicle6.wav" );
		pev->noise = MAKE_STRING( "plats/vehicle6.wav" );
		break;
	case 6:
		PRECACHE_SOUND( "plats/vehicle7.wav" );
		pev->noise = MAKE_STRING( "plats/vehicle7.wav" );
		break;
	default:
		// SBO: the client event only knows 1-6 and would stay silent for the 7 HL25 plays here
		m_sounds = 6;
		PRECACHE_SOUND( "plats/vehicle7.wav" );
		pev->noise = MAKE_STRING( "plats/vehicle7.wav" );
		break;
	}

	PRECACHE_SOUND( VEHICLE_DEFAULT_BRAKE_SOUND );

	// SBO 2.0 (Featureful): the mapper's brake and ignition sounds
	if ( !FStringNull( pev->noise1 ) )
		PRECACHE_SOUND( (char *)STRING( pev->noise1 ) );
	if ( !FStringNull( pev->noise3 ) )
		PRECACHE_SOUND( (char *)STRING( pev->noise3 ) );

	// SBO 2.0 (Featureful): the touch proxy (once: Precache runs from Spawn only)
	if ( !FStringNull( m_touchProxyName ) && m_hVehicleProxy == NULL )
	{
		CBaseEntity *pProxy = CBaseEntity::Create( "func_vehicle_proxy", pev->origin, pev->angles, edict() );
		if ( pProxy )
		{
			pProxy->pev->targetname = m_touchProxyName;
			m_hVehicleProxy = pProxy;
		}
	}

	// SBO: HL25 precaches this in ClientPrecache() for every map; only maps with a vehicle need it
	// (CBasePlayer::PlayerUse plays it when a player takes the controls)
	PRECACHE_SOUND( "plats/vehicle_ignition.wav" );

	m_usAdjustPitch = PRECACHE_EVENT( 1, "events/vehicle.sc" );
}

// SBO 2.0 (Featureful): the touch proxy goes with its vehicle
void CFuncVehicle::UpdateOnRemove( void )
{
	if ( m_hVehicleProxy != NULL )
	{
		UTIL_Remove( m_hVehicleProxy );
		m_hVehicleProxy = NULL;
	}

	CBaseEntity::UpdateOnRemove();
}


// This class defines the volume of space that the player must stand in to control the vehicle
class CFuncVehicleControls : public CBaseEntity
{
public:
	virtual int	ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Spawn( void );
	void EXPORT Find( void );
};
LINK_ENTITY_TO_CLASS( func_vehiclecontrols, CFuncVehicleControls );

void CFuncVehicleControls::Find( void )
{
	edict_t *pTarget = NULL;

	do
	{
		pTarget = FIND_ENTITY_BY_TARGETNAME( pTarget, STRING( pev->target ) );
	} while ( !FNullEnt( pTarget ) && !FClassnameIs( pTarget, "func_vehicle" ) );

	if ( FNullEnt( pTarget ) )
	{
		ALERT( at_console, "No vehicle %s\n", STRING( pev->target ) );
		return;
	}

	CFuncVehicle *pvehicle = CFuncVehicle::Instance( pTarget );
	if ( pvehicle )
		pvehicle->SetControls( pev );
	UTIL_Remove( this );
}

void CFuncVehicleControls::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CFuncVehicleControls::Find );
	pev->nextthink = gpGlobals->time;
}
