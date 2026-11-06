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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "effects.h"
#include "weapons.h"
#include "explode.h"

#include "player.h"
#include "monsters.h"	// SBO 2.0: R_DL (m_iClass)
#include "guide_entities.h"	// SBO 2.0: monster_target, scripted_tanksequence
#include "locus.h"
#include "round_hooks.h"


#define SF_TANK_ACTIVE			0x0001
#define SF_TANK_PLAYER			0x0002
#define SF_TANK_HUMANS			0x0004
#define SF_TANK_ALIENS			0x0008
#define SF_TANK_LINEOFSIGHT		0x0010
#define SF_TANK_CANCONTROL		0x0020
#define SF_TANK_LASERSPOT		0x0040	// SBO 2.0, from Featureful (Spirit's LRC): a laser dot where the barrel points
#define SF_TANK_MATCHTARGET		0x0080	// SBO 2.0, from Featureful (Spirit's LRC): aim at the point the player looks at
#define SF_TANK_SMOKE_ON_EMPTY	0x0100	// SBO 2.0, from Featureful: a smoke puff at the barrel when the last bullet is fired
#define SF_TANK_SOUNDON			0x8000

enum TANKBULLET
{
	TANK_BULLET_NONE = 0,
	TANK_BULLET_9MM = 1,
	TANK_BULLET_MP5 = 2,
	TANK_BULLET_12MM = 3,
};

//			Custom damage
//			env_laser (duration is 0.5 rate of fire)
//			rockets
//			explosion?

void GuideTankSequence_Facing( CBaseEntity *pSequence );	// SBO 2.0: scripted_tanksequence (below)

class CFuncTank : public CBaseEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Think( void );
	void	TrackTarget( void );

	virtual void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	virtual Vector UpdateTargetPosition( CBaseEntity *pTarget )
	{
		return pTarget->BodyTarget( pev->origin );
	}

	void	StartRotSound( void );
	void	StopRotSound( void );

	// Bmodels don't go across transitions
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	inline BOOL IsActive( void ) { return (pev->spawnflags & SF_TANK_ACTIVE)?TRUE:FALSE; }
	inline void TankActivate( void ) { pev->spawnflags |= SF_TANK_ACTIVE; pev->nextthink = pev->ltime + 0.1; m_fireLast = 0; }
	inline void TankDeactivate( void ) { pev->spawnflags &= ~SF_TANK_ACTIVE; m_fireLast = 0; StopRotSound(); }
	inline BOOL CanFire( void ) { return (gpGlobals->time - m_lastSightTime) < m_persist; }
	BOOL		InRange( float range );

	// Acquire a target. SBO 2.0: the tank scans the players itself (see FindTarget)
	edict_t		*FindTarget( void );
	BOOL		TargetVisible( edict_t *pTarget, const Vector &barrelEnd );
	BOOL		TargetInYawRange( edict_t *pTarget );

	void		TankTrace( const Vector &vecStart, const Vector &vecForward, const Vector &vecSpread, TraceResult &tr );

	Vector		BarrelPosition( void )
	{
		Vector forward, right, up;
		UTIL_MakeVectorsPrivate( pev->angles, forward, right, up );
		return pev->origin + (forward * m_barrelPos.x) + (right * m_barrelPos.y) + (up * m_barrelPos.z);
	}

	void		AdjustAnglesForBarrel( Vector &angles, float distance );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL OnControls( entvars_t *pevTest );
	BOOL StartControl( CBasePlayer* pController );
	void StopControl( void );
	void ControllerPostFrame( void );
	// SBO 2.0 (Featureful c8ebe744, BugfixedHL): what a tank firing at nothing does when its controller leaves
	virtual void StopFire( void ) {}

	// SBO 2.0, from Featureful / Spirit (CFuncTank::StartSequence / StopSequence): a scripted_tanksequence steers
	// the tank (guide entities, docs/FEATUREFUL_GUIDE_ENTITIES.md)
	void StartSequence( CBaseEntity *pSequence );
	void StopSequence( void );
	BOOL TrackSequence( Vector &angles );	// the sequence's aim; FALSE: none this think
	void SequenceFire( void );				// one shot (the "once at end" shot, or a sequence's fire)
	CBasePlayer *Controller( void ) { return m_pController; }
	float		MaxRange( void ) { return m_maxRange; }
	EHANDLE		m_hSequence;
	EHANDLE		m_hSequenceEnemy;
	BOOL		m_bSeqFire;		// the sequence wants it to fire (Featureful's SF_TANK_SEQFIRE)
	float		m_flSeqNextAttack;


protected:
	CBasePlayer* m_pController;
	float		m_flNextAttack;
	Vector		m_vecControllerUsePos;
	
	float		m_yawCenter;	// "Center" yaw
	float		m_yawRate;		// Max turn rate to track targets
	float		m_yawRange;		// Range of turning motion (one-sided: 30 is +/- 30 degress from center)
								// Zero is full rotation
	float		m_yawTolerance;	// Tolerance angle

	float		m_pitchCenter;	// "Center" pitch
	float		m_pitchRate;	// Max turn rate on pitch
	float		m_pitchRange;	// Range of pitch motion as above
	float		m_pitchTolerance;	// Tolerance angle

	float		m_fireLast;		// Last time I fired
	float		m_fireRate;		// How many rounds/second
	float		m_lastSightTime;// Last time I saw target
	float		m_persist;		// Persistence of firing (how long do I shoot when I can't see)
	float		m_minRange;		// Minimum range to aim/track
	float		m_maxRange;		// Max range to aim/track

	Vector		m_barrelPos;	// Length of the freakin barrel
	float		m_spriteScale;	// Scale of any sprites we shoot
	int			m_iszSpriteSmoke;
	int			m_iszSpriteFlash;
	TANKBULLET	m_bulletType;	// Bullet type
	int			m_iBulletDamage; // 0 means use Bullet type's default damage
	
	Vector		m_sightOrigin;	// Last sight of target
	int			m_spread;		// firing spread
	int			m_iszMaster;	// Master entity (game_team_master or multisource)
	EHANDLE		m_hTarget;		// SBO 2.0: the player the tank is after (FindTarget)

	// SBO 2.0, from Featureful (CFuncTank, Half-Life 1 SDK license; no map sets these): "m_iszLocusFire" fired at
	// each shot with a Locus point at the barrel's end (its velocity: the barrel's direction), "smokerendermode" the
	// render mode of the smoke sprite (0: Texture Alpha)
	string_t	m_iszLocusFire;
	int			m_iSmokeRenderMode;
	EHANDLE		m_hFireProxy;
	void		UpdateOnRemove( void );

	// SBO 2.0, from Featureful: "player_weapon_volume" how loud a player's shots with it are for the monsters (0
	// LOUD_GUN_VOLUME, HL; -1 silent); flag 64 "Laser Spot": a laser dot where the barrel points while it is in use
	int			m_iPlayerWeaponVolume;
	EHANDLE		m_hSpot;
	void		UpdateSpot( void );

public:
	// SBO 2.0, from Featureful (CFuncTank, Half-Life 1 SDK license; no map sets these):
	// "bulletCount" the shots it has (0 or less: no limit); empty, it clicks (weapons/357_cock1.wav) instead of
	// firing. "firemaster" it fires only while this master is on (it still aims). "extra_barrels" the tanks with
	// this name (inactive ones) turn with it and fire when it fires. "m_iClass" whom it shoots at: 0 players
	// (FindTarget), otherwise as a monster of that class (FindClassTarget) and that class to the monsters
	int			m_bulletCount;
	float		m_flEmptySoundTime;
	string_t	m_iszFireMaster;
	string_t	m_iszExtraBarrels;
	int			m_iTankClass;
	inline BOOL HaveBullets( void ) { return m_bulletCount != 0; }
	void		RemoveBullet( void );
	void		TryFire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker, CBaseEntity *pController );
	void		FireBarrels( BOOL controlledOrScripted, const Vector &forward, entvars_t *pevAttacker, CBaseEntity *pController );
	void		ResetBarrelsTimers( void );
	void		UpdateFireLast( void ) { m_fireLast = gpGlobals->time - ( 1 / m_fireRate ) - 0.01; }
	virtual void TankSpecialThink( void ) {}	// the laser tank's beam off (an extra barrel does not think itself)
	edict_t		*FindClassTarget( void );
	int			Classify( void ) { return m_iTankClass; }
};


TYPEDESCRIPTION	CFuncTank::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTank, m_yawCenter, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_yawRate, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_yawRange, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_yawTolerance, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchCenter, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchRate, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchRange, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_pitchTolerance, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_fireLast, FIELD_TIME ),
	DEFINE_FIELD( CFuncTank, m_fireRate, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_lastSightTime, FIELD_TIME ),
	DEFINE_FIELD( CFuncTank, m_persist, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_minRange, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_maxRange, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_barrelPos, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTank, m_spriteScale, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTank, m_iszSpriteSmoke, FIELD_STRING ),
	DEFINE_FIELD( CFuncTank, m_iszSpriteFlash, FIELD_STRING ),
	DEFINE_FIELD( CFuncTank, m_bulletType, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTank, m_sightOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTank, m_spread, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTank, m_pController, FIELD_CLASSPTR ),
	DEFINE_FIELD( CFuncTank, m_vecControllerUsePos, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTank, m_flNextAttack, FIELD_TIME ),
	DEFINE_FIELD( CFuncTank, m_iBulletDamage, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTank, m_hTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( CFuncTank, m_iszMaster, FIELD_STRING ),
	DEFINE_FIELD( CFuncTank, m_iPlayerWeaponVolume, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTank, m_bulletCount, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncTank, m_iszFireMaster, FIELD_STRING ),
	DEFINE_FIELD( CFuncTank, m_iszExtraBarrels, FIELD_STRING ),
	DEFINE_FIELD( CFuncTank, m_iTankClass, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CFuncTank, CBaseEntity );

static Vector gTankSpread[] =
{
	Vector( 0, 0, 0 ),		// perfect
	Vector( 0.025, 0.025, 0.025 ),	// small cone
	Vector( 0.05, 0.05, 0.05 ),  // medium cone
	Vector( 0.1, 0.1, 0.1 ),	// large cone
	Vector( 0.25, 0.25, 0.25 ),	// extra-large cone
};
#define MAX_FIRING_SPREADS ARRAYSIZE(gTankSpread)

// SBO 2.0 (Featureful): an "extra_barrels" entity that is a tank
static CFuncTank *GetFuncTankPointer( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return NULL;
	if ( FClassnameIs( pEntity->pev, "func_tank" ) || FClassnameIs( pEntity->pev, "func_tanklaser" ) ||
		FClassnameIs( pEntity->pev, "func_tankmortar" ) || FClassnameIs( pEntity->pev, "func_tankrocket" ) )
		return (CFuncTank *)pEntity;
	return NULL;
}


void CFuncTank :: Spawn( void )
{
	Precache();

	pev->movetype	= MOVETYPE_PUSH;  // so it doesn't get pushed by anything
	pev->solid		= SOLID_BSP;
	SET_MODEL( ENT(pev), STRING(pev->model) );

	m_yawCenter = pev->angles.y;
	m_pitchCenter = pev->angles.x;

	if ( IsActive() )
		pev->nextthink = pev->ltime + 1.0;

	m_sightOrigin = BarrelPosition(); // Point at the end of the barrel

	if ( m_fireRate <= 0 )
		m_fireRate = 1;
	if ( m_spread > MAX_FIRING_SPREADS )
		m_spread = 0;

	pev->oldorigin = pev->origin;

	if ( !m_bulletCount )	// SBO 2.0 (Featureful): no limit
		m_bulletCount = -1;
}


void CFuncTank :: Precache( void )
{
	if ( m_iszSpriteSmoke )
		PRECACHE_MODEL( (char *)STRING(m_iszSpriteSmoke) );
	if ( m_iszSpriteFlash )
		PRECACHE_MODEL( (char *)STRING(m_iszSpriteFlash) );

	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING(pev->noise) );
	if ( m_bulletCount > 0 )	// SBO 2.0 (Featureful): the empty click (the weapons precache it too)
		PRECACHE_SOUND( "weapons/357_cock1.wav" );
}


void CFuncTank :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "yawrate"))
	{
		m_yawRate = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "yawrange"))
	{
		m_yawRange = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "yawtolerance"))
	{
		m_yawTolerance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitchrange"))
	{
		m_pitchRange = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitchrate"))
	{
		m_pitchRate = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitchtolerance"))
	{
		m_pitchTolerance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firerate"))
	{
		m_fireRate = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "barrel"))
	{
		m_barrelPos.x = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "barrely"))
	{
		m_barrelPos.y = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "barrelz"))
	{
		m_barrelPos.z = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spritescale"))
	{
		m_spriteScale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "player_weapon_volume"))	// SBO 2.0 (Featureful)
	{
		m_iPlayerWeaponVolume = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszLocusFire"))	// SBO 2.0 (Featureful)
	{
		m_iszLocusFire = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "smokerendermode"))	// SBO 2.0 (Featureful)
	{
		m_iSmokeRenderMode = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spritesmoke"))
	{
		m_iszSpriteSmoke = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spriteflash"))
	{
		m_iszSpriteFlash = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "rotatesound"))
	{
		pev->noise = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "persistence"))
	{
		m_persist = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "bullet"))
	{
		m_bulletType = (TANKBULLET)atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "bullet_damage" )) 
	{
		m_iBulletDamage = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firespread"))
	{
		m_spread = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "minRange"))
	{
		m_minRange = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "maxRange"))
	{
		m_maxRange = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
	{
		m_iszMaster = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "bulletCount") || FStrEq(pkvd->szKeyName, "bulletcount"))	// SBO 2.0 (Featureful)
	{
		m_bulletCount = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firemaster"))	// SBO 2.0 (Featureful)
	{
		m_iszFireMaster = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "extra_barrels"))	// SBO 2.0 (Featureful)
	{
		m_iszExtraBarrels = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iClass"))	// SBO 2.0 (Featureful)
	{
		m_iTankClass = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

////////////// START NEW STUFF //////////////

//==================================================================================
// TANK CONTROLLING
BOOL CFuncTank :: OnControls( entvars_t *pevTest )
{
	if ( !(pev->spawnflags & SF_TANK_CANCONTROL) )
		return FALSE;

	Vector offset = pevTest->origin - pev->origin;

	if ( (m_vecControllerUsePos - pevTest->origin).Length() < 30 )
		return TRUE;

	return FALSE;
}

BOOL CFuncTank :: StartControl( CBasePlayer *pController )
{
	if ( m_pController != NULL )
		return FALSE;
	if ( m_hSequence != NULL )
		return FALSE;	// SBO 2.0: a scripted_tanksequence has the tank

	// Team only or disabled?
	if ( m_iszMaster )
	{
		if ( !UTIL_IsMasterTriggered( m_iszMaster, pController ) )
			return FALSE;
	}

	ALERT( at_console, "using TANK!\n");

	// SBO 2.0 (Featureful 00405458, BugfixedHL): the player's tank only once he has the controls. Use set it before
	// this call, so a refused player (master not triggered) kept a tank without a controller, and his PostThink's
	// ControllerPostFrame dereferenced the NULL m_pController
	m_pController = pController;
	m_pController->m_pTank = this;
	if ( m_pController->m_pActiveItem )
	{
		m_pController->m_pActiveItem->Holster();
		m_pController->pev->weaponmodel = 0;
		m_pController->pev->viewmodel = 0; 

	}

	m_pController->m_iHideHUD |= HIDEHUD_WEAPONS;
	m_vecControllerUsePos = m_pController->pev->origin;
	
	pev->nextthink = pev->ltime + 0.1;
	
	return TRUE;
}

void CFuncTank :: StopControl()
{
	// SBO 2.0 (Featureful 0570769d / c8ebe744, BugfixedHL): a laser tank's beam goes off with its controller
	StopFire();

	// TODO: bring back the controllers current weapon
	if ( !m_pController )
		return;

	// halflife-updated (HLU #134): redeploy the weapon, or switch away if it can't be deployed (empty)
	m_pController->EquipWeapon();

	ALERT( at_console, "stopped using TANK\n");

	m_pController->m_iHideHUD &= ~HIDEHUD_WEAPONS;

	pev->nextthink = 0;
	// SBO 2.0 (Featureful 00405458): the player lets go of the tank here, whoever ended the control (another
	// player's +use on the tank stopped it and left the first player's m_pTank set)
	m_pController->m_pTank = NULL;
	m_pController = NULL;
	UpdateSpot();	// SBO 2.0: the laser spot goes out with the controls (unless the tank is active)

	if ( IsActive() )
		pev->nextthink = pev->ltime + 1.0;
}

// Called each frame by the player's ItemPostFrame
void CFuncTank :: ControllerPostFrame( void )
{
	ASSERT(m_pController != NULL);

	if ( gpGlobals->time < m_flNextAttack )
		return;

	if ( m_pController->pev->button & IN_ATTACK )
	{
		Vector vecForward;
		UTIL_MakeVectorsPrivate( pev->angles, vecForward, NULL, NULL );

		m_fireLast = gpGlobals->time - (1/m_fireRate) - 0.01;  // to make sure the gun doesn't fire too many bullets

		TryFire( BarrelPosition(), vecForward, m_pController->pev, m_pController );	// SBO 2.0 (Featureful)
		FireBarrels( TRUE, vecForward, m_pController->pev, m_pController );
		
		// HACKHACK -- make some noise (that the AI can hear)
		// SBO 2.0 (Featureful "player_weapon_volume"): its own loudness; -1 none
		if ( m_pController && m_pController->IsPlayer() && m_iPlayerWeaponVolume >= 0 )
			((CBasePlayer *)m_pController)->m_iWeaponVolume = m_iPlayerWeaponVolume > 0 ? m_iPlayerWeaponVolume : LOUD_GUN_VOLUME;

		m_flNextAttack = gpGlobals->time + (1/m_fireRate);
	}
}
////////////// END NEW STUFF //////////////


void CFuncTank :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_TANK_CANCONTROL )
	{  // player controlled turret

		// SBO 2.0 (Featureful 066169c5): fired with no activator (a delayed target, an env_global)
		if ( !pActivator || pActivator->Classify() != CLASS_PLAYER )
		{
			ALERT( at_warning, "Triggering a controllable %s with non-player activator. Probably a mapping error?\n", STRING(pev->classname) );
			return;
		}

		if ( value == 2 && useType == USE_SET )
		{
			ControllerPostFrame();
		}
		else if ( !m_pController && useType != USE_OFF )
		{
			StartControl( (CBasePlayer*)pActivator );
		}
		else
		{
			StopControl();
		}
	}
	else
	{
		if ( !ShouldToggle( useType, IsActive() ) )
			return;

		if ( IsActive() )
			TankDeactivate();
		else
			TankActivate();
		UpdateSpot();	// SBO 2.0 (Featureful flag 64): on / off with the tank
	}
}


// SBO 2.0: in range and nothing between the barrel and the target's eyes: the checks TrackTarget makes before it
// takes a new sight of its target
BOOL CFuncTank :: TargetVisible( edict_t *pTarget, const Vector &barrelEnd )
{
	Vector targetPosition = pTarget->v.origin + pTarget->v.view_ofs;

	if ( !InRange( ( targetPosition - barrelEnd ).Length() ) )
		return FALSE;

	TraceResult tr;
	UTIL_TraceLine( barrelEnd, targetPosition, dont_ignore_monsters, edict(), &tr );

	return tr.flFraction == 1.0 || tr.pHit == pTarget;
}

// SBO 2.0: can the tank turn far enough to aim at the target? (TrackTarget takes no sight of a target outside its
// yaw range. The barrel offset is left out here.)
BOOL CFuncTank :: TargetInYawRange( edict_t *pTarget )
{
	Vector angles = UTIL_VecToAngles( pTarget->v.origin - pev->origin );

	return fabs( UTIL_AngleDistance( angles.y, m_yawCenter ) ) <= m_yawRange;
}

// SBO 2.0: the tank picks its target among all players. The SDK asked the engine for "a client in my PVS"
// (FIND_CLIENT_IN_PVS), which is single-player code: the engine checks one client per 0.1 seconds in turn for all
// entities together, so with several players a tank was handed a different player every think, lost its target each
// time that player was somewhere else on the map, and slept 2 seconds whenever the player whose turn it was could not
// be seen from the tank. An automatic tank hardly tracked or fired with two or more players.
//
// Candidates are the players the engine's check accepts, for every slot: alive, without FL_NOTARGET, in the tank's
// PVS (or in plain sight of the barrel, for a tank whose origin is inside a wall); observers and spectators never.
// Among them, in this order:
//   1. the current target while it can be shot at (in range, line of sight, inside the yaw range), so the barrel
//      doesn't flip between two players
//   2. the nearest player who can be shot at
//   3. the current target, then the nearest player, in range and line of sight but outside the yaw range
//   4. the current target, then the nearest player, without line of sight: the tank behaves as it does towards a
//      single player it can't see (it stays on the last sight and fires for its "persistence" time)
// NULL when there is no candidate; only then the tank waits 2 seconds, as it did with nobody in its PVS.
edict_t *CFuncTank :: FindTarget( void )
{
	if ( m_iTankClass != 0 )
		return FindClassTarget();	// SBO 2.0 (Featureful "m_iClass")

	Vector vecView = pev->origin + pev->view_ofs;	// as the engine (PF_checkclient)
	unsigned char *pvs = ENGINE_SET_PVS( vecView );
	Vector barrelEnd = BarrelPosition();

	edict_t *pCurrent = NULL;
	if ( m_hTarget != NULL )
		pCurrent = m_hTarget->edict();

	edict_t *pBest = NULL;
	int iBestRank = 0;
	float flBestDist = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( !pPlayer || pPlayer->has_disconnected || !pPlayer->IsAlive() )
			continue;
		if ( pPlayer->pev->flags & FL_NOTARGET )
			continue;
		if ( pPlayer->IsObserver() || ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER ) )
			continue;

		edict_t *pEdict = pPlayer->edict();

		// 3: can be shot at, 2: visible but outside the yaw range, 1: in the PVS without line of sight (or out of range)
		int iRank;
		if ( TargetVisible( pEdict, barrelEnd ) )
			iRank = TargetInYawRange( pEdict ) ? 3 : 2;
		else if ( ENGINE_CHECK_VISIBILITY( pEdict, pvs ) )
			iRank = 1;
		else
			continue;


		float flDist = ( pPlayer->pev->origin + pPlayer->pev->view_ofs - barrelEnd ).Length();

		// the current target wins inside its rank
		if ( pEdict == pCurrent )
			flDist = -1;

		if ( !pBest || iRank > iBestRank || ( iRank == iBestRank && flDist < flBestDist ) )
		{
			pBest = pEdict;
			iBestRank = iRank;
			flBestDist = flDist;
		}
	}

	// SBO 2.0 (Featureful monster_target): active monster_targets that count as a player or a player ally are
	// candidates too, ranked the same way (none on a map without them: the list is empty)
	CBaseEntity *pTargets[32];
	int nTargets = GuideMonsterTargets_ForTank( pTargets, 32 );
	for ( int t = 0; t < nTargets; t++ )
	{
		edict_t *pEdict = pTargets[t]->edict();
		int iRank;
		if ( TargetVisible( pEdict, barrelEnd ) )
			iRank = TargetInYawRange( pEdict ) ? 3 : 2;
		else if ( ENGINE_CHECK_VISIBILITY( pEdict, pvs ) )
			iRank = 1;
		else
			continue;
		float flDist = ( pEdict->v.origin + pEdict->v.view_ofs - barrelEnd ).Length();
		if ( pEdict == pCurrent )
			flDist = -1;
		if ( !pBest || iRank > iBestRank || ( iRank == iBestRank && flDist < flBestDist ) )
		{
			pBest = pEdict;
			iBestRank = iRank;
			flBestDist = flDist;
		}
	}

	if ( pBest )
		m_hTarget = CBaseEntity::Instance( pBest );
	else
		m_hTarget = NULL;

	return pBest;
}



// SBO 2.0, from Featureful (CFuncTank::BestVisibleEnemy / IRelationship, "m_iClass" not 0): players and monsters
// in a box of the Maximum range (512 without one) around the tank that its class dislikes or worse, by
// CBaseMonster::ClassRelationship (the monsters' own table, with their "classify" / "is_player_ally"): the most
// hated first, then the nearest. Alive ones only, never a spectator or an entity with notarget
edict_t *CFuncTank :: FindClassTarget( void )
{
	float flLookDist = m_maxRange ? m_maxRange : 512;
	Vector delta( flLookDist, flLookDist, flLookDist );
	CBaseEntity *pList[100];
	int count = UTIL_EntitiesInBox( pList, 100, pev->origin - delta, pev->origin + delta, FL_CLIENT | FL_MONSTER );

	CBaseEntity *pBest = NULL;
	int iBestRelationship = R_DL;
	float flNearest = 8192;

	for ( int i = 0; i < count; i++ )
	{
		CBaseEntity *pEnt = pList[i];
		if ( pEnt == this || !pEnt->IsAlive() || ( pEnt->pev->flags & FL_NOTARGET ) )
			continue;
		if ( pEnt->IsPlayer() )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)pEnt;
			if ( pPlayer->has_disconnected || pPlayer->IsObserver() || ( pPlayer->m_afPhysicsFlags & PFLAG_OBSERVER ) )
				continue;
		}
		int iRelationship = CBaseMonster::ClassRelationship( m_iTankClass, pEnt );
		float flDist = ( pEnt->pev->origin - pev->origin ).Length();
		if ( iRelationship > iBestRelationship )
		{
			iBestRelationship = iRelationship;
			flNearest = flDist;
			pBest = pEnt;
		}
		else if ( iRelationship == iBestRelationship && flDist <= flNearest )
		{
			flNearest = flDist;
			pBest = pEnt;
		}
	}

	m_hTarget = pBest;
	return pBest ? pBest->edict() : NULL;
}


BOOL CFuncTank :: InRange( float range )
{
	if ( range < m_minRange )
		return FALSE;
	if ( m_maxRange > 0 && range > m_maxRange )
		return FALSE;

	return TRUE;
}


void CFuncTank :: Think( void )
{
	pev->avelocity = g_vecZero;
	TrackTarget();
	UpdateSpot();	// SBO 2.0 (Featureful flag 64)

	if ( fabs(pev->avelocity.x) > 1 || fabs(pev->avelocity.y) > 1 )
		StartRotSound();
	else
		StopRotSound();
}

void CFuncTank::TrackTarget( void )
{
	TraceResult tr;
	BOOL updateTime = FALSE, lineOfSight;
	Vector angles, direction, targetPosition, barrelEnd;
	edict_t *pTarget;

	// Get a position to aim for
	if ( m_hSequence != NULL )
	{
		// SBO 2.0 (Featureful): a scripted_tanksequence aims it
		pev->nextthink = pev->ltime + 0.05;
		if ( !TrackSequence( angles ) )
			return;
	}
	else if (m_pController)
	{
		pev->nextthink = pev->ltime + 0.05;
		if ( pev->spawnflags & SF_TANK_MATCHTARGET )
		{
			// SBO 2.0, from Featureful (Spirit's LRC "Match Target", 128): aim at the point the player looks at
			UTIL_MakeVectorsPrivate( m_pController->pev->v_angle, direction, NULL, NULL );
			Vector vecEye = m_pController->pev->origin + m_pController->pev->view_ofs;
			edict_t *pentOwner = pev->owner;
			pev->owner = m_pController->edict();	// the trace skips the player and the tank
			UTIL_TraceLine( vecEye, vecEye + direction * 1000, missile, ignore_glass, edict(), &tr );
			pev->owner = pentOwner;
			direction = tr.vecEndPos - pev->origin;
			angles = UTIL_VecToAngles( direction );
			AdjustAnglesForBarrel( angles, direction.Length() );
		}
		else
		{
			// Tanks attempt to mirror the player's angles
			angles = m_pController->pev->v_angle;
			angles[0] = 0 - angles[0];
		}
	}
	else
	{
		if ( IsActive() )
			pev->nextthink = pev->ltime + 0.1;
		else
			return;

		// SBO 2.0: was FIND_CLIENT_IN_PVS, one client per 0.1 seconds in turn (FindTarget)
		pTarget = FindTarget();
		if ( FNullEnt( pTarget ) )
		{
			if ( IsActive() )
				pev->nextthink = pev->ltime + 2;	// Wait 2 secs
			return;
		}


		// Calculate angle needed to aim at target
		barrelEnd = BarrelPosition();
		targetPosition = pTarget->v.origin + pTarget->v.view_ofs;
		float range = (targetPosition - barrelEnd).Length();
		
		if ( !InRange( range ) )
			return;

		UTIL_TraceLine( barrelEnd, targetPosition, dont_ignore_monsters, edict(), &tr );
		
		lineOfSight = FALSE;
		// No line of sight, don't track
		if ( tr.flFraction == 1.0 || tr.pHit == pTarget )
		{
			lineOfSight = TRUE;

			CBaseEntity *pInstance = CBaseEntity::Instance(pTarget);
			if ( InRange( range ) && pInstance && pInstance->IsAlive() )
			{
				updateTime = TRUE;
				m_sightOrigin = UpdateTargetPosition( pInstance );
			}
		}

		// Track sight origin

// !!! I'm not sure what i changed
		direction = m_sightOrigin - pev->origin;
//		direction = m_sightOrigin - barrelEnd;
		angles = UTIL_VecToAngles( direction );

		// Calculate the additional rotation to point the end of the barrel at the target (not the gun's center) 
		AdjustAnglesForBarrel( angles, direction.Length() );
	}

	angles.x = -angles.x;

	// Force the angles to be relative to the center position
	angles.y = m_yawCenter + UTIL_AngleDistance( angles.y, m_yawCenter );
	angles.x = m_pitchCenter + UTIL_AngleDistance( angles.x, m_pitchCenter );

	// Limit against range in y
	if ( angles.y > m_yawCenter + m_yawRange )
	{
		angles.y = m_yawCenter + m_yawRange;
		updateTime = FALSE;	// Don't update if you saw the player, but out of range
	}
	else if ( angles.y < (m_yawCenter - m_yawRange) )
	{
		angles.y = (m_yawCenter - m_yawRange);
		updateTime = FALSE; // Don't update if you saw the player, but out of range
	}

	if ( updateTime )
		m_lastSightTime = gpGlobals->time;

	// Move toward target at rate or less
	float distY = UTIL_AngleDistance( angles.y, pev->angles.y );
	pev->avelocity.y = distY * 10;
	if ( pev->avelocity.y > m_yawRate )
		pev->avelocity.y = m_yawRate;
	else if ( pev->avelocity.y < -m_yawRate )
		pev->avelocity.y = -m_yawRate;

	// Limit against range in x
	if ( angles.x > m_pitchCenter + m_pitchRange )
		angles.x = m_pitchCenter + m_pitchRange;
	else if ( angles.x < m_pitchCenter - m_pitchRange )
		angles.x = m_pitchCenter - m_pitchRange;

	// Move toward target at rate or less
	float distX = UTIL_AngleDistance( angles.x, pev->angles.x );
	pev->avelocity.x = distX  * 10;

	if ( pev->avelocity.x > m_pitchRate )
		pev->avelocity.x = m_pitchRate;
	else if ( pev->avelocity.x < -m_pitchRate )
		pev->avelocity.x = -m_pitchRate;

	// SBO 2.0 (Featureful "extra_barrels"): the extra barrels take its angles (they do not think themselves)
	if ( !FStringNull( m_iszExtraBarrels ) )
	{
		CBaseEntity *pTry = NULL;
		while ( ( pTry = UTIL_FindEntityByTargetname( pTry, STRING( m_iszExtraBarrels ) ) ) != NULL )
		{
			CFuncTank *pBarrel = GetFuncTankPointer( pTry );
			if ( pBarrel && pBarrel != this )
			{
				pBarrel->pev->angles = pev->angles;
				pBarrel->TankSpecialThink();
				pev->nextthink = pev->ltime + 0.05;	// update sooner with barrels
			}
		}
	}

	if ( m_hSequence != NULL )
	{
		// SBO 2.0 (Featureful): tell the sequence when the tank faces its target; fire when it says so
		if ( fabs( distY ) < 0.1 && fabs( distX ) < 0.1 )
			GuideTankSequence_Facing( m_hSequence );
		if ( m_hSequence != NULL && m_bSeqFire && gpGlobals->time >= m_flSeqNextAttack )
		{
			SequenceFire();
			m_flSeqNextAttack = gpGlobals->time + ( 1 / m_fireRate );
		}
		return;
	}

	if ( m_pController )
		return;

	if ( CanFire() && ( (fabs(distX) < m_pitchTolerance && fabs(distY) < m_yawTolerance) || (pev->spawnflags & SF_TANK_LINEOFSIGHT) ) )
	{
		BOOL fire = FALSE;
		Vector forward;
		UTIL_MakeVectorsPrivate( pev->angles, forward, NULL, NULL );

		if ( pev->spawnflags & SF_TANK_LINEOFSIGHT )
		{
			float length = direction.Length();
			UTIL_TraceLine( barrelEnd, barrelEnd + forward * length, dont_ignore_monsters, edict(), &tr );
			if ( tr.pHit == pTarget )
				fire = TRUE;
		}
		else
			fire = TRUE;

		if ( fire )
		{
			TryFire( BarrelPosition(), forward, pev, NULL );	// SBO 2.0 (Featureful)
			FireBarrels( FALSE, forward, pev, NULL );
		}
		else
		{
			m_fireLast = 0;
			ResetBarrelsTimers();
		}
	}
	else
	{
		m_fireLast = 0;
		ResetBarrelsTimers();
	}
}


// If barrel is offset, add in additional rotation
void CFuncTank::AdjustAnglesForBarrel( Vector &angles, float distance )
{
	float r2, d2;


	if ( m_barrelPos.y != 0 || m_barrelPos.z != 0 )
	{
		distance -= m_barrelPos.z;
		d2 = distance * distance;
		// halflife-updated (halflife #3303): avoid sqrt of a negative number (NaN angles)
		if ( m_barrelPos.y )
		{
			r2 = m_barrelPos.y * m_barrelPos.y;
			if ( d2 > r2 )
			{
				angles.y += (180.0 / M_PI) * atan2( m_barrelPos.y, sqrt( d2 - r2 ) );
			}
		}
		if ( m_barrelPos.z )
		{
			r2 = m_barrelPos.z * m_barrelPos.z;
			if ( d2 > r2 )
			{
				angles.x += (180.0 / M_PI) * atan2( -m_barrelPos.z, sqrt( d2 - r2 ) );
			}
		}
	}
}


// Fire targets and spawn sprites
void CFuncTank::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	if ( m_fireLast != 0 )
	{
		if ( m_iszSpriteSmoke )
		{
			CSprite *pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteSmoke), barrelEnd, TRUE );
			pSprite->AnimateAndDie( RANDOM_FLOAT( 15.0, 20.0 ) );
			pSprite->SetTransparency( m_iSmokeRenderMode ? m_iSmokeRenderMode : kRenderTransAlpha, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, 255, kRenderFxNone );
			pSprite->pev->velocity.z = RANDOM_FLOAT(40, 80);
			pSprite->SetScale( m_spriteScale );
		}
		if ( m_iszSpriteFlash )
		{
			CSprite *pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteFlash), barrelEnd, TRUE );
			pSprite->AnimateAndDie( 60 );
			pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
			pSprite->SetScale( m_spriteScale );

			// Hack Hack, make it stick around for at least 100 ms.
			pSprite->pev->nextthink += 0.1;
		}
		// SBO 2.0, from Featureful (LRC): "m_iszLocusFire" with a Locus point at the barrel's end
		if ( !FStringNull( m_iszLocusFire ) )
		{
			if ( m_hFireProxy == NULL )
				m_hFireProxy = CBaseEntity::Create( "info_target", barrelEnd, g_vecZero, NULL );
			CBaseEntity *pProxy = m_hFireProxy;
			if ( pProxy )
			{
				UTIL_SetOrigin( pProxy->pev, barrelEnd );
				pProxy->pev->velocity = forward;
				FireTargets( STRING( m_iszLocusFire ), pProxy, this, USE_TOGGLE, 0 );
			}
		}
		SUB_UseTargets( this, USE_TOGGLE, 0 );
	}
	m_fireLast = gpGlobals->time;
}


// SBO 2.0 (Featureful CFuncTank::UpdateSpot, flag 64 "Laser Spot"): the dot where the barrel points while a player
// has the controls or the tank is active; hidden otherwise. sprites/laserdot.spr is precached by every map
// (W_Precache), so the spot can be made at any time
void CFuncTank::UpdateSpot( void )
{
	if ( !( pev->spawnflags & SF_TANK_LASERSPOT ) )
		return;
	BOOL fInUse = m_pController != NULL || IsActive();
	CBaseEntity *pSpot = m_hSpot;
	if ( !pSpot )
	{
		if ( !fInUse || FBitSet( pev->flags, FL_KILLME ) )
			return;
		pSpot = CLaserSpot::CreateSpot();
		m_hSpot = pSpot;
		if ( !pSpot )
			return;
	}
	if ( !fInUse )
	{
		pSpot->pev->effects |= EF_NODRAW;
		return;
	}
	pSpot->pev->effects &= ~EF_NODRAW;
	Vector vecAiming;
	UTIL_MakeVectorsPrivate( pev->angles, vecAiming, NULL, NULL );
	Vector vecSrc = BarrelPosition();
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 8192, dont_ignore_monsters, ENT(pev), &tr );
	UTIL_SetOrigin( pSpot->pev, tr.vecEndPos );
}

// SBO 2.0: the Locus point of "m_iszLocusFire" goes with the tank
void CFuncTank::UpdateOnRemove( void )
{
	if ( m_hSpot != NULL )	// SBO 2.0 (Featureful): and the laser spot
		UTIL_Remove( m_hSpot );
	if ( m_hFireProxy != NULL )
		UTIL_Remove( m_hFireProxy );
	CBaseEntity::UpdateOnRemove();
}

// SBO 2.0 (Featureful CFuncTank::RemoveBullet): one bullet off ("bulletCount"); with "Smoke on empty" (256) the last
// one makes a smoke puff (TE_SMOKE, steam1.spr, scale 2, 5 fps) at the barrel end
void CFuncTank::RemoveBullet( void )
{
	if ( m_bulletCount > 0 )
	{
		m_bulletCount--;
		if ( !m_bulletCount && ( pev->spawnflags & SF_TANK_SMOKE_ON_EMPTY ) )
		{
			Vector pos = BarrelPosition();
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_SMOKE );
				WRITE_COORD( pos.x );
				WRITE_COORD( pos.y );
				WRITE_COORD( pos.z );
				WRITE_SHORT( g_sModelIndexSmoke );
				WRITE_BYTE( 20 ); // scale * 10
				WRITE_BYTE( 5 ); // framerate
			MESSAGE_END();
		}
	}
}

// SBO 2.0 (Featureful CFuncTank::TryFire): every shot of a tank goes through here. "firemaster" off: no shot (the
// tank still aims); out of bullets ("bulletCount"): the empty click, at most every 0.5 s
void CFuncTank::TryFire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker, CBaseEntity *pController )
{
	if ( !UTIL_IsMasterTriggered( m_iszFireMaster, pController ) )
		return;
	if ( !HaveBullets() )
	{
		if ( m_flEmptySoundTime < gpGlobals->time )
		{
			EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM );
			m_flEmptySoundTime = gpGlobals->time + 0.5;
		}
		return;
	}
	Fire( barrelEnd, forward, pevAttacker );
}

// SBO 2.0 (Featureful CFuncTank::FireBarrels): the extra barrels fire with the tank, each from its own barrel end in
// the tank's direction, through its own TryFire (its own bullets and fire master). Controlled or in a sequence each
// barrel keeps its own rate of fire
void CFuncTank::FireBarrels( BOOL controlledOrScripted, const Vector &forward, entvars_t *pevAttacker, CBaseEntity *pController )
{
	if ( FStringNull( m_iszExtraBarrels ) )
		return;
	CBaseEntity *pTry = NULL;
	while ( ( pTry = UTIL_FindEntityByTargetname( pTry, STRING( m_iszExtraBarrels ) ) ) != NULL )
	{
		CFuncTank *pBarrel = GetFuncTankPointer( pTry );
		if ( !pBarrel || pBarrel == this )
			continue;
		if ( controlledOrScripted && gpGlobals->time < pBarrel->m_flNextAttack )
			continue;
		if ( controlledOrScripted )
			pBarrel->UpdateFireLast();
		pBarrel->TryFire( pBarrel->BarrelPosition(), forward, pevAttacker == pev ? pBarrel->pev : pevAttacker, pController );
		if ( controlledOrScripted )
			pBarrel->m_flNextAttack = gpGlobals->time + ( 1 / pBarrel->m_fireRate );
	}
}

void CFuncTank::ResetBarrelsTimers( void )
{
	if ( FStringNull( m_iszExtraBarrels ) )
		return;
	CBaseEntity *pTry = NULL;
	while ( ( pTry = UTIL_FindEntityByTargetname( pTry, STRING( m_iszExtraBarrels ) ) ) != NULL )
	{
		CFuncTank *pBarrel = GetFuncTankPointer( pTry );
		if ( pBarrel && pBarrel != this )
			pBarrel->m_fireLast = 0;
	}
}

void CFuncTank::TankTrace( const Vector &vecStart, const Vector &vecForward, const Vector &vecSpread, TraceResult &tr )
{
	// get circular gaussian spread
	float x, y, z;
	do {
		x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
		z = x*x+y*y;
	} while (z > 1);
	Vector vecDir = vecForward +
		x * vecSpread.x * gpGlobals->v_right +
		y * vecSpread.y * gpGlobals->v_up;
	Vector vecEnd;
	
	vecEnd = vecStart + vecDir * 4096;
	UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, edict(), &tr );
}

	
void CFuncTank::StartRotSound( void )
{
	if ( !pev->noise || (pev->spawnflags & SF_TANK_SOUNDON) )
		return;
	pev->spawnflags |= SF_TANK_SOUNDON;
	EMIT_SOUND( edict(), CHAN_STATIC, (char*)STRING(pev->noise), 0.85, ATTN_NORM);
}


void CFuncTank::StopRotSound( void )
{
	if ( pev->spawnflags & SF_TANK_SOUNDON )
		STOP_SOUND( edict(), CHAN_STATIC, (char*)STRING(pev->noise) );
	pev->spawnflags &= ~SF_TANK_SOUNDON;
}

class CFuncTankGun : public CFuncTank
{
public:
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	void KeyValue( KeyValueData *pkvd );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	// SBO 2.0 (Featureful 69573239 "bullet_gib_policy"): 0 as SBO (FireBullets: with a Damage Per Bullet over 16
	// the kill gibs, at 16 or less it never does; without one the game decides by the damage), 1 the game decides,
	// 2 never gib, 3 always gib
	int m_iGibPolicy;
};
LINK_ENTITY_TO_CLASS( func_tank, CFuncTankGun );

TYPEDESCRIPTION	CFuncTankGun::m_SaveData[] =
{
	DEFINE_FIELD( CFuncTankGun, m_iGibPolicy, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CFuncTankGun, CFuncTank );

void CFuncTankGun::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "bullet_gib_policy"))
	{
		m_iGibPolicy = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CFuncTank::KeyValue( pkvd );
}

// SBO: the 2005 server numbers the monster bullets 17/18/19 (11 SBO player bullet types sit before them)
void CFuncTankGun::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	int i;

	if ( m_fireLast != 0 )
	{
		// FireBullets needs gpGlobals->v_up, etc.
		UTIL_MakeAimVectors(pev->angles);

		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;
		if ( bulletCount > 0 )
		{
			// SBO 2.0 (Featureful "bullet_gib_policy"): the gib bits of each hit; -1 FireBullets' own
			int bitsGib = -1;
			switch ( m_iGibPolicy )
			{
			case 1: bitsGib = 0; break;
			case 2: bitsGib = DMG_NEVERGIB; break;
			case 3: bitsGib = DMG_ALWAYSGIB; break;
			}

			// SBO 2.0 (Featureful "bulletCount"): one bullet off for each shot, none once it is empty
			for ( i = 0; i < bulletCount && HaveBullets(); i++ )
			{
				switch( m_bulletType )
				{
				case TANK_BULLET_9MM:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, 17, 1, m_iBulletDamage, pevAttacker, bitsGib );
					break;

				case TANK_BULLET_MP5:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, 18, 1, m_iBulletDamage, pevAttacker, bitsGib );
					break;

				case TANK_BULLET_12MM:
					FireBullets( 1, barrelEnd, forward, gTankSpread[m_spread], 4096, 19, 1, m_iBulletDamage, pevAttacker, bitsGib );
					break;

				default:
				case TANK_BULLET_NONE:
					break;
				}
				if ( m_bulletType < TANK_BULLET_9MM || m_bulletType > TANK_BULLET_12MM )
					break;	// Featureful: no bullet, none used
				RemoveBullet();
			}
			CFuncTank::Fire( barrelEnd, forward, pevAttacker );
		}
	}
	else
		CFuncTank::Fire( barrelEnd, forward, pevAttacker );
}



class CFuncTankLaser : public CFuncTank
{
public:
	void	Activate( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	void	Think( void );
	void	TankSpecialThink( void );
	CLaser *GetLaser( void );
	void	StopFire( void );
	void	UpdateOnRemove( void );
	void	Blocked( CBaseEntity *pOther );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
	CLaser	*m_pLaser;
	float	m_laserTime;
};
LINK_ENTITY_TO_CLASS( func_tanklaser, CFuncTankLaser );

TYPEDESCRIPTION	CFuncTankLaser::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTankLaser, m_pLaser, FIELD_CLASSPTR ),
	DEFINE_FIELD( CFuncTankLaser, m_laserTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CFuncTankLaser, CFuncTank );

void CFuncTankLaser::Activate( void )
{
	if ( !GetLaser() )
	{
		UTIL_Remove(this);
		ALERT( at_error, "Laser tank with no env_laser!\n" );
	}
	else
	{
		m_pLaser->TurnOff();
	}
}

// SBO 2.0 (Featureful c8ebe744, BugfixedHL): the beam goes off when the player leaves the controls
void CFuncTankLaser::StopFire( void )
{
	if ( m_pLaser )
		m_pLaser->TurnOff();
}

// SBO 2.0 (Featureful f6b7bde2 / CFuncTank::UpdateOnRemove's StopFire): a tank removed (killtarget) in the frame
// after a shot, before its think turned the beam off, left its env_laser drawn for good (co_corecity's laser tanks)
void CFuncTankLaser::UpdateOnRemove( void )
{
	StopFire();
	m_pLaser = NULL;
	CFuncTank::UpdateOnRemove();
}


void CFuncTankLaser::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "laserentity"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CFuncTank::KeyValue( pkvd );
}


CLaser *CFuncTankLaser::GetLaser( void )
{
	if ( m_pLaser )
		return m_pLaser;

	edict_t	*pentLaser;

	pentLaser = FIND_ENTITY_BY_TARGETNAME( NULL, STRING(pev->message) );
	while ( !FNullEnt( pentLaser ) )
	{
		// Found the landmark
		if ( FClassnameIs( pentLaser, "env_laser" ) )
		{
			m_pLaser = (CLaser *)CBaseEntity::Instance(pentLaser);
			break;
		}
		else
			pentLaser = FIND_ENTITY_BY_TARGETNAME( pentLaser, STRING(pev->message) );
	}

	return m_pLaser;
}


void CFuncTankLaser::Think( void )
{
	TankSpecialThink();

	CFuncTank::Think();
}

// SBO 2.0 (Featureful): the beam off after the shot; also from the tank this one is an "extra_barrels" of
void CFuncTankLaser::TankSpecialThink( void )
{
	if ( m_pLaser && (gpGlobals->time > m_laserTime) )
		m_pLaser->TurnOff();
}

// SBO 2.0 (Featureful 59228c10, same aim): a tank blocked by a player or monster stops its local time
// (MOVETYPE_PUSH), so its think never came and the beam stayed on. The engine calls Blocked every frame the tank is
// blocked: the beam goes off when the tank's think would have turned it off (0.05 s with a player at the controls,
// 0.1 s without, TrackTarget). Unblocked, the beam keeps 1.38's timing (Featureful's FL_ALWAYSTHINK cut it to one frame).
void CFuncTankLaser::Blocked( CBaseEntity *pOther )
{
	if ( m_pLaser && m_pLaser->IsOn() && gpGlobals->time > m_laserTime + ( m_pController ? 0.05 : 0.1 ) )
		m_pLaser->TurnOff();

	CFuncTank::Blocked( pOther );
}


void CFuncTankLaser::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	int i;
	TraceResult tr;

	if ( m_fireLast != 0 && GetLaser() )
	{
		// TankTrace needs gpGlobals->v_up, etc.
		UTIL_MakeAimVectors(pev->angles);

		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;
		if ( bulletCount )
		{
			for ( i = 0; i < bulletCount && HaveBullets(); i++ )	// SBO 2.0 (Featureful "bulletCount")
			{
				m_pLaser->pev->origin = barrelEnd;
				TankTrace( barrelEnd, forward, gTankSpread[m_spread], tr );
				
				m_laserTime = gpGlobals->time;
				m_pLaser->TurnOn();
				m_pLaser->pev->dmgtime = gpGlobals->time - 1.0;
				// SBO 2.0 (Valve halflife#561, Featureful's CLaser::FireAtPoint attacker): the player at the controls
				// is the attacker and this tank the inflictor (kill feed d_tanklaser); unmanned: the laser, as in HL
				if ( pevAttacker != pev )
					m_pLaser->FireAtPoint( tr, pevAttacker, pev );
				else
					m_pLaser->FireAtPoint( tr );
				m_pLaser->pev->nextthink = 0;
				RemoveBullet();
			}
			CFuncTank::Fire( barrelEnd, forward, pev );
		}
	}
	else
	{
		CFuncTank::Fire( barrelEnd, forward, pev );
	}
}

class CFuncTankRocket : public CFuncTank
{
public:
	void Precache( void );
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
};
LINK_ENTITY_TO_CLASS( func_tankrocket, CFuncTankRocket );

void CFuncTankRocket::Precache( void )
{
	UTIL_PrecacheOther( "rpg_rocket" );
	CFuncTank::Precache();
}



void CFuncTankRocket::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	int i;

	if ( m_fireLast != 0 )
	{
		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;
		if ( bulletCount > 0 )
		{
			for ( i = 0; i < bulletCount && HaveBullets(); i++ )	// SBO 2.0 (Featureful "bulletCount")
			{
				CBaseEntity *pRocket = CBaseEntity::Create( "rpg_rocket", barrelEnd, pev->angles, edict() );
				// SBO 2.0 (Valve halflife#561): the player at the controls gets the kill (CRpgRocket::SetTankAttacker)
				if ( pRocket && pevAttacker != pev )
					( (CRpgRocket *)pRocket )->SetTankAttacker( CBaseEntity::Instance( pevAttacker ) );
				RemoveBullet();
			}
			CFuncTank::Fire( barrelEnd, forward, pev );
		}
	}
	else
		CFuncTank::Fire( barrelEnd, forward, pev );
}


class CFuncTankMortar : public CFuncTank
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
};
LINK_ENTITY_TO_CLASS( func_tankmortar, CFuncTankMortar );


void CFuncTankMortar::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "iMagnitude"))
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CFuncTank::KeyValue( pkvd );
}


void CFuncTankMortar::Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker )
{
	if ( m_fireLast != 0 )
	{
		int bulletCount = (gpGlobals->time - m_fireLast) * m_fireRate;
		// Only create 1 explosion
		if ( bulletCount > 0 )
		{
			TraceResult tr;

			// TankTrace needs gpGlobals->v_up, etc.
			UTIL_MakeAimVectors(pev->angles);

			TankTrace( barrelEnd, forward, gTankSpread[m_spread], tr );

			// SBO 2.0 (Valve halflife#561, Featureful's ExplosionCreate attacker): the player at the controls is the
			// blast's attacker; unmanned: the explosion itself, as in HL
			ExplosionCreate( tr.vecEndPos, pev->angles, edict(), pev->impulse, TRUE, pevAttacker != pev ? pevAttacker : NULL );
			RemoveBullet();	// SBO 2.0 (Featureful "bulletCount")

			CFuncTank::Fire( barrelEnd, forward, pev );
		}
	}
	else
		CFuncTank::Fire( barrelEnd, forward, pev );
}



//============================================================================
// FUNC TANK CONTROLS
//============================================================================
class CFuncTankControls : public CBaseEntity
{
public:
	virtual int	ObjectCaps( void );
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think( void );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	CFuncTank *m_pTank;
};
LINK_ENTITY_TO_CLASS( func_tankcontrols, CFuncTankControls );

TYPEDESCRIPTION	CFuncTankControls::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTankControls, m_pTank, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CFuncTankControls, CBaseEntity );

int	CFuncTankControls :: ObjectCaps( void ) 
{ 
	return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE; 
}


void CFuncTankControls :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ // pass the Use command onto the controls
	if ( m_pTank )
		m_pTank->Use( pActivator, pCaller, useType, value );

	ASSERT( m_pTank != NULL );	// if this fails,  most likely means save/restore hasn't worked properly
}


void CFuncTankControls :: Think( void )
{
	edict_t *pTarget = NULL;

	do 
	{
		pTarget = FIND_ENTITY_BY_TARGETNAME( pTarget, STRING(pev->target) );
	} while ( !FNullEnt(pTarget) && strncmp( STRING(pTarget->v.classname), "func_tank", 9 ) );

	if ( FNullEnt( pTarget ) )
	{
		ALERT( at_console, "No tank %s\n", STRING(pev->target) );
		return;
	}

	m_pTank = (CFuncTank*)Instance(pTarget);
}

void CFuncTankControls::Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	SET_MODEL( ENT(pev), STRING(pev->model) );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );
	
	pev->nextthink = gpGlobals->time + 0.3;	// After all the func_tank's have spawned

	CBaseEntity::Spawn();
}


//============================================================================
// SBO 2.0, from Featureful (halflife-featureful dlls/func_tank.cpp CTankSequence, from Spirit of Half-Life; Half-Life 1
// SDK license; docs/FEATUREFUL_GUIDE_ENTITIES.md): scripted_tanksequence takes a func_tank ("m_iszEntity") and turns
// it (to its own angles, to face itself, or to face an enemy: "m_iszEnemy" a name, !activator or a classname
// within the tank's range; blank: the tank's own target), fires it (once at the end, constantly, or while facing),
// until it faces the target, the enemy dies, it is triggered again or "m_fDuration" runs out ("target" fires on the
// halt, "netname" on the timeout); then it can switch the tank on / off and its control. "Dump player" (1) takes
// the tank from a player at its controls; "Repeatable" (2) keeps it after one run. Featureful's laser spot is not
// ported (SBO's tanks have none).
//============================================================================
#define SF_TSEQ_DUMPPLAYER	1
#define SF_TSEQ_REPEATABLE	2

#define TSEQ_UNTIL_NONE		0
#define TSEQ_UNTIL_FACING	1
#define TSEQ_UNTIL_DEATH	2

#define TSEQ_TURN_NO		0
#define TSEQ_TURN_ANGLE		1
#define TSEQ_TURN_FACE		2
#define TSEQ_TURN_ENEMY		3

#define TSEQ_SHOOT_NO		0
#define TSEQ_SHOOT_ONCE		1
#define TSEQ_SHOOT_ALWAYS	2
#define TSEQ_SHOOT_FACING	3

#define TSEQ_FLAG_NOCHANGE	0
#define TSEQ_FLAG_ON		1
#define TSEQ_FLAG_OFF		2
#define TSEQ_FLAG_TOGGLE	3

class CTankSequence : public CBaseEntity
{
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT EndThink( void );
	void EXPORT TimeOutThink( void );
	void KeyValue( KeyValueData *pkvd );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	CFuncTank *GetTank( void ) { return (CFuncTank *)(CBaseEntity *)m_hTank; }

	void StopSequence( void );
	void FacingNotify( void );
	void DeadEnemyNotify( void );

	string_t m_iszEntity;
	string_t m_iszEnemy;
	int m_iUntil;
	float m_fDuration;
	int m_iTurn;
	int m_iShoot;
	int m_iActive;
	int m_iControllable;
	EHANDLE m_hTank;
};

LINK_ENTITY_TO_CLASS( scripted_tanksequence, CTankSequence );

static CFuncTank *Guide_FuncTank( CBaseEntity *pEnt )
{
	if ( !pEnt )
		return NULL;
	const char *cls = STRING( pEnt->pev->classname );
	if ( !strcmp( cls, "func_tank" ) || !strcmp( cls, "func_tanklaser" ) || !strcmp( cls, "func_tankrocket" ) ||
		 !strcmp( cls, "func_tankmortar" ) )
		return (CFuncTank *)pEnt;
	return NULL;
}

void CTankSequence::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iUntil" ) )
		m_iUntil = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iTurn" ) )
		m_iTurn = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iShoot" ) )
		m_iShoot = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iActive" ) )
		m_iActive = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iControllable" ) )
		m_iControllable = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iLaserSpot" ) )
		;	// Featureful's laser spot: SBO's tanks have none
	else if ( FStrEq( pkvd->szKeyName, "m_iszEntity" ) )
		m_iszEntity = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszEnemy" ) )
		m_iszEnemy = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_fDuration" ) )
		m_fDuration = atof( pkvd->szValue );
	else
	{
		CBaseEntity::KeyValue( pkvd );
		return;
	}
	pkvd->fHandled = TRUE;
}

// Featureful UTIL_FindEntityGeneric: a name (or !activator), else the nearest entity of that class within the radius
static CBaseEntity *Guide_FindEntityGeneric( const char *szName, const Vector &vecSrc, float flRadius, CBaseEntity *pActivator )
{
	CBaseEntity *pEntity = Locus_FindEntity( NULL, szName, pActivator );
	if ( pEntity )
		return pEntity;
	float flBest = flRadius > 0 ? flRadius : 8192;
	CBaseEntity *pSearch = NULL;
	CBaseEntity *pBest = NULL;
	while ( ( pSearch = UTIL_FindEntityByClassname( pSearch, szName ) ) != NULL )
	{
		if ( pSearch->IsPlayer() && !Logic_IsPlayerInPlay( pSearch ) )
			continue;	// never a spectator
		float flDist = ( pSearch->pev->origin - vecSrc ).Length();
		if ( flDist < flBest )
		{
			flBest = flDist;
			pBest = pSearch;
		}
	}
	return pBest;
}

void CTankSequence::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	BOOL hasTank = GetTank() != NULL;
	if ( !ShouldToggle( useType, hasTank ) )
		return;

	if ( !hasTank )
	{
		CFuncTank *pTank = Guide_FuncTank( UTIL_FindEntityByTargetname( NULL, STRING( m_iszEntity ) ) );
		if ( !pTank )
		{
			ALERT( at_console, "scripted_tanksequence \"%s\": no func_tank \"%s\"\n", STRING( pev->targetname ), STRING( m_iszEntity ) );
			return;
		}
		if ( pTank->m_hSequence != NULL )
			return;	// another sequence has it
		if ( pTank->Controller() )
		{
			if ( !FBitSet( pev->spawnflags, SF_TSEQ_DUMPPLAYER ) )
				return;	// a player has it
			pTank->StopControl();
		}

		if ( m_iTurn == TSEQ_TURN_ENEMY )
		{
			CBaseEntity *pEnemy = NULL;
			if ( !FStringNull( m_iszEnemy ) )
				pEnemy = Guide_FindEntityGeneric( STRING( m_iszEnemy ), pTank->pev->origin, pTank->MaxRange(), pActivator );
			else
			{
				edict_t *pTarget = pTank->FindTarget();
				pEnemy = FNullEnt( pTarget ) ? NULL : CBaseEntity::Instance( pTarget );
			}
			if ( !pEnemy )
				return;
			pTank->m_hSequenceEnemy = pEnemy;
		}
		pTank->StartSequence( this );
		pTank->m_bSeqFire = ( m_iShoot == TSEQ_SHOOT_ALWAYS );
		m_hTank = pTank;
		if ( m_fDuration > 0 )
		{
			SetThink( &CTankSequence::TimeOutThink );
			pev->nextthink = gpGlobals->time + m_fDuration;
		}
	}
	else
	{
		// any halt condition can be ended early by a trigger
		pev->nextthink = 0;
		StopSequence();
	}
}

void CTankSequence::FacingNotify( void )
{
	if ( m_iUntil == TSEQ_UNTIL_FACING )
	{
		SetThink( &CTankSequence::EndThink );
		pev->nextthink = gpGlobals->time;
	}
	else if ( m_iShoot == TSEQ_SHOOT_FACING && GetTank() )
		GetTank()->m_bSeqFire = TRUE;
}

void CTankSequence::DeadEnemyNotify( void )
{
	if ( m_iUntil == TSEQ_UNTIL_DEATH )
	{
		SetThink( &CTankSequence::EndThink );
		pev->nextthink = gpGlobals->time;
	}
}

void CTankSequence::EndThink( void )
{
	SetThink( NULL );
	StopSequence();
	if ( !FStringNull( pev->target ) )
		FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
}

void CTankSequence::TimeOutThink( void )
{
	SetThink( NULL );
	StopSequence();
	if ( !FStringNull( pev->netname ) )
		FireTargets( STRING( pev->netname ), this, this, USE_TOGGLE, 0 );
}

void CTankSequence::StopSequence( void )
{
	CFuncTank *pTank = GetTank();
	if ( !pTank )
		return;

	if ( m_iShoot == TSEQ_SHOOT_ONCE )
		pTank->SequenceFire();	// the shot at the end

	if ( m_iControllable )
	{
		BOOL on = FBitSet( pTank->pev->spawnflags, SF_TANK_CANCONTROL ) ? TRUE : FALSE;
		if ( on && m_iControllable != TSEQ_FLAG_ON )
			pTank->pev->spawnflags &= ~SF_TANK_CANCONTROL;
		else if ( !on && m_iControllable != TSEQ_FLAG_OFF )
			pTank->pev->spawnflags |= SF_TANK_CANCONTROL;
	}

	pTank->StopSequence();
	m_hTank = NULL;

	if ( pTank->IsActive() && ( m_iActive == TSEQ_FLAG_OFF || m_iActive == TSEQ_FLAG_TOGGLE ) )
		pTank->TankDeactivate();
	else if ( !pTank->IsActive() && ( m_iActive == TSEQ_FLAG_ON || m_iActive == TSEQ_FLAG_TOGGLE ) )
		pTank->TankActivate();

	if ( !FBitSet( pev->spawnflags, SF_TSEQ_REPEATABLE ) )
		UTIL_Remove( this );
}

void GuideTankSequence_Facing( CBaseEntity *pSequence )
{
	if ( pSequence && FClassnameIs( pSequence->pev, "scripted_tanksequence" ) )
		( (CTankSequence *)pSequence )->FacingNotify();
}

void CFuncTank::StartSequence( CBaseEntity *pSequence )
{
	m_hSequence = pSequence;
	m_flSeqNextAttack = 0;
	pev->nextthink = pev->ltime + 0.1;
}

void CFuncTank::StopSequence( void )
{
	StopRotSound();
	pev->nextthink = 0;
	pev->avelocity = g_vecZero;
	m_hSequence = NULL;
	m_hSequenceEnemy = NULL;
	m_bSeqFire = FALSE;
	if ( IsActive() )
		pev->nextthink = pev->ltime + 1.0;
}

BOOL CFuncTank::TrackSequence( Vector &angles )
{
	CTankSequence *pSeq = (CTankSequence *)(CBaseEntity *)m_hSequence;
	angles = pev->angles;	// no turn
	angles.x = -angles.x;	// TrackTarget flips the pitch back
	if ( pSeq->m_iTurn == TSEQ_TURN_ENEMY )
	{
		CBaseEntity *pEnemy = m_hSequenceEnemy;
		if ( !pEnemy || !pEnemy->IsAlive() )
		{
			pSeq->DeadEnemyNotify();
			if ( !pEnemy )
				return FALSE;
		}
		Vector direction = pEnemy->pev->origin + pEnemy->pev->view_ofs - pev->origin;
		angles = UTIL_VecToAngles( direction );
		AdjustAnglesForBarrel( angles, direction.Length() );
	}
	else if ( pSeq->m_iTurn == TSEQ_TURN_ANGLE )
	{
		angles = pSeq->pev->angles;	// as Featureful (TrackTarget then flips the pitch)
	}
	else if ( pSeq->m_iTurn == TSEQ_TURN_FACE )
	{
		Vector direction = pSeq->pev->origin - pev->origin;
		angles = UTIL_VecToAngles( direction );
		AdjustAnglesForBarrel( angles, direction.Length() );
	}
	return TRUE;
}

void CFuncTank::SequenceFire( void )
{
	Vector forward;
	UTIL_MakeVectorsPrivate( pev->angles, forward, NULL, NULL );
	m_fireLast = gpGlobals->time - ( 1 / m_fireRate ) - 0.01;	// exactly one shot
	TryFire( BarrelPosition(), forward, pev, NULL );	// SBO 2.0 (Featureful)
	FireBarrels( TRUE, forward, pev, NULL );
}

