//=========================================================
// Featureful's player and monster triggers (docs/LOGIC_ENTITIES.md):
//   trigger_playerfreeze  freezes / unfreezes the activator, or every player in play
//   trigger_killmonster   kills monsters by name, class name and squad name
//   trigger_hurt_remote   damages (or heals) named entities or a whole class, once or every few seconds
//   trigger_look          a brush that fires when a player in it looks at a target for some time
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) commit 30fc2f71
// (2026-10-30), dlls/triggers.cpp: CTriggerPlayerFreeze, CTriggerKillMonster, CTriggerHurtRemote, CTriggerLook.
// The same class names, key names, values and spawnflags, so Featureful entity setups carry over.
//
// SBO changes (marked "SBO" below):
//   * Players are affected only while in play (round_hooks.h Logic_IsPlayerInPlay): never spectators, Game Masters,
//     HLTV proxies, or players dead or waiting for a round.
//   * trigger_playerfreeze "All players" (spawnflag 1, SBO's addition): every player in play
//     instead of the activator. A frozen player stays frozen until unfrozen, until he dies or respawns (a new round),
//     as in the HL SDK where Spawn clears FL_FROZEN (CBasePlayer::m_bTriggerFrozen keeps SBO's per-frame unfreeze
//     in PreThink off him).
//   * trigger_hurt_remote and trigger_killmonster work on SBO's TakeDamage (HL SDK signature), which Featureful
//     replaced with a DamageInfo: "Ignore Armor", "No Camera Punch" and the minimum health are done around the call;
//     an instant kill is a kill whatever SBO's random damage scaling does (Kill_ByDamage below).
//   * trigger_hurt_remote: the activator gets the kill credit (Featureful), except for a player hurting another
//     player: then the entity itself is the attacker, so that co-op's friendly fire rule (no damage from a teammate
//     with mp_friendlyfire 0) does not stop a map hazard that a player switched on. Its Delay is at least 0.01 s; 0
//     or below (also set later by trigger_changevalue) is Featureful's 0.1 s default. With a minimum health a hit
//     never gets a time-based damage type (poison, nerve gas, radiation, ...): the lasting damage those start would
//     go below the minimum and on after the entity is switched off. The minimum holds whatever SBO's damage scaling
//     does (the health is held at it after the hit).
//     The entity is the inflictor (Featureful: the target), so the kill feed names it ("with trigger_hurt_remote");
//     the push the HL SDK's TakeDamage gives a player is taken back (Featureful's remote hurt does not push).
//   * trigger_look keeps the look time per player (Featureful kept one time for the whole brush, so two players in
//     it reset each other) and finds its "!activator" / "*player" look target through the Locus rules.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"player.h"
#include	"saverestore.h"
#include	"weapons.h"
#include	"locus.h"
#include	"round_hooks.h"

// SBO: a kill by damage that SBO's random damage scaling (CBaseMonster / CBasePlayer::TakeDamage scale a hit by 0.25
// to 2) cannot turn into a survival: four times the health plus one. Featureful's kill (damage = health, pure damage
// to health) never gibs by itself, so the overkill goes with DMG_NEVERGIB unless the entity should gib. A player's
// armor neither absorbs nor drains (pure damage to health); spawn protection still refuses it.
static int Kill_ByDamage( CBaseEntity *pTarget, entvars_t *pevInflictor, entvars_t *pevAttacker, int bitsDamage, BOOL fGib )
{
	const BOOL fPlayer = pTarget->IsPlayer();
	const float flArmor = pTarget->pev->armorvalue;
	if ( fPlayer )
		pTarget->pev->armorvalue = 0;

	bitsDamage &= ~( DMG_ALWAYSGIB | DMG_NEVERGIB );
	bitsDamage |= fGib ? DMG_ALWAYSGIB : DMG_NEVERGIB;
	// a corpse (trigger_hurt_remote "Do damage to dead"): Featureful's kill gibs it (DamageInfo gibCorpse); SBO's
	// DeadTakeDamage gibs a corpse only for a DMG_GIB_CORPSE type, so one of them goes with it
	if ( !pTarget->IsAlive() && fGib )
		bitsDamage |= DMG_CRUSH;
	const int iResult = pTarget->TakeDamage( pevInflictor, pevAttacker, pTarget->pev->health * 4 + 1, bitsDamage );

	if ( fPlayer )
		pTarget->pev->armorvalue = flArmor;
	return iResult;
}

//=========================================================
// trigger_playerfreeze
//=========================================================

enum
{
	USE_FREEZE_OFF = -1,
	USE_FREEZE_TOGGLE = 0,
	USE_FREEZE_ON = 1,
	USE_FREEZE_COPY_INPUT = 2,
};

#define SF_PLAYERFREEZE_ALL_PLAYERS		1	// SBO: every player in play, not the activator

class CTriggerPlayerFreeze : public CPointEntity
{
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );

private:
	void Freeze( CBasePlayer *pPlayer, int freezeType );
};

LINK_ENTITY_TO_CLASS( trigger_playerfreeze, CTriggerPlayerFreeze );

void CTriggerPlayerFreeze::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "use_type" ) )
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTriggerPlayerFreeze::Freeze( CBasePlayer *pPlayer, int freezeType )
{
	BOOL fFreeze;

	switch ( freezeType )
	{
	case USE_FREEZE_OFF:
		fFreeze = FALSE;
		break;
	case USE_FREEZE_ON:
		fFreeze = TRUE;
		break;
	case USE_FREEZE_TOGGLE:
		fFreeze = !( pPlayer->pev->flags & FL_FROZEN );
		break;
	default:
		ALERT( at_console, "%s: Unknown bad use type: %d\n", STRING( pev->classname ), freezeType );
		return;
	}

	// SBO: PreThink unfreezes a player every frame unless something holds him (player.cpp)
	pPlayer->m_bTriggerFrozen = fFreeze;
	if ( fFreeze || !pPlayer->IsFrozenByCamera() )
		pPlayer->EnableControl( !fFreeze );
}

void CTriggerPlayerFreeze::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int freezeType = pev->impulse;
	if ( pev->impulse == USE_FREEZE_COPY_INPUT )
	{
		if ( useType == USE_TOGGLE )
			freezeType = USE_FREEZE_TOGGLE;
		else if ( useType == USE_OFF )
			freezeType = USE_FREEZE_OFF;
		else if ( useType == USE_ON )
			freezeType = USE_FREEZE_ON;
		else
		{
			ALERT( at_console, "%s: Can't interpret input use type: %d\n", STRING( pev->classname ), (int)useType );
			return;
		}
	}

	if ( pev->spawnflags & SF_PLAYERFREEZE_ALL_PLAYERS )
	{
		// SBO: every player in play
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
			if ( Logic_IsPlayerInPlay( pPlayer ) )
				Freeze( (CBasePlayer *)pPlayer, freezeType );
		}
	}
	else if ( Logic_IsPlayerInPlay( pActivator ) )	// Featureful: EffectiveAlivePlayer( pActivator )
	{
		Freeze( (CBasePlayer *)pActivator, freezeType );
	}
}

//=========================================================
// trigger_killmonster
//=========================================================

#define SF_KILLMONSTER_FIREONCE		1
#define SF_KILLMONSTER_GIBALWAYS	16

class CTriggerKillMonster : public CPointEntity
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

protected:
	void KillMonster( CBaseEntity *pEntity );
	BOOL CheckTarget( CBaseEntity *pEntity );
};

LINK_ENTITY_TO_CLASS( trigger_killmonster, CTriggerKillMonster );

void CTriggerKillMonster::Spawn( void )
{
	CPointEntity::Spawn();
	pev->effects |= EF_NODRAW;
}

void CTriggerKillMonster::KillMonster( CBaseEntity *pEntity )
{
	CBaseMonster *pMonster = pEntity->MyMonsterPointer();
	// Featureful: IsFullyAlive (alive and not dying)
	if ( !pMonster || !pMonster->IsAlive() || pMonster->pev->deadflag != DEAD_NO )
		return;
	// SBO: a player only while in play
	if ( pMonster->IsPlayer() && !Logic_IsPlayerInPlay( pMonster ) )
		return;

	switch ( RANDOM_LONG( 0, 3 ) )
	{
	case 0:
		pMonster->m_LastHitGroup = HITGROUP_GENERIC;
		break;
	case 1:
		pMonster->m_LastHitGroup = HITGROUP_CHEST;
		break;
	case 2:
		pMonster->m_LastHitGroup = HITGROUP_STOMACH;
		break;
	default:
		break;
	}

	Kill_ByDamage( pMonster, pev, pev, DMG_GENERIC, ( pev->spawnflags & SF_KILLMONSTER_GIBALWAYS ) != 0 );
}

BOOL CTriggerKillMonster::CheckTarget( CBaseEntity *pEntity )
{
	if ( !FStringNull( pev->target ) )
	{
		if ( FStringNull( pEntity->pev->targetname ) )
			return FALSE;
		if ( !FStrEq( STRING( pev->target ), STRING( pEntity->pev->targetname ) ) )
			return FALSE;
	}

	if ( !FStringNull( pev->message ) )
	{
		if ( FStringNull( pEntity->pev->classname ) )
			return FALSE;
		if ( !FClassnameIs( pEntity->pev, STRING( pev->message ) ) )
			return FALSE;
	}

	if ( !FStringNull( pev->netname ) )
	{
		if ( FStringNull( pEntity->pev->netname ) )
			return FALSE;
		if ( !FStrEq( STRING( pev->netname ), STRING( pEntity->pev->netname ) ) )
			return FALSE;
	}

	return TRUE;
}

void CTriggerKillMonster::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pEntity = NULL;
	if ( !FStringNull( pev->target ) )
	{
		while ( ( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->target ) ) ) != NULL )
		{
			if ( CheckTarget( pEntity ) )
				KillMonster( pEntity );
		}
	}
	else if ( !FStringNull( pev->message ) )
	{
		while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, STRING( pev->message ) ) ) != NULL )
		{
			if ( CheckTarget( pEntity ) )
				KillMonster( pEntity );
		}
	}
	else if ( !FStringNull( pev->netname ) )
	{
		while ( ( pEntity = UTIL_FindEntityByString( pEntity, "netname", STRING( pev->netname ) ) ) != NULL )
		{
			if ( CheckTarget( pEntity ) )
				KillMonster( pEntity );
		}
	}

	if ( pev->spawnflags & SF_KILLMONSTER_FIREONCE )
		UTIL_Remove( this );
}

//=========================================================
// trigger_hurt_remote
//=========================================================

#define SF_TRIGGER_HURT_REMOTE_INSTANT_KILL		1
#define SF_TRIGGER_HURT_REMOTE_CONSTANT			2
#define SF_TRIGGER_HURT_REMOTE_STARTON			4
#define SF_TRIGGER_HURT_REMOTE_DO_ARMOR_DAMAGE	8
#define SF_TRIGGER_HURT_REMOTE_DO_CORPSES		16

#define SF_TRIGGER_HURT_REMOTE_IGNORE_ARMOR		256
#define SF_TRIGGER_HURT_REMOTE_NO_PUNCH			512

// SBO: the time-based damage types (CBasePlayer::CheckTimeBasedDamage), not used with a minimum health (DoDamage)
#define HURT_REMOTE_LASTING_DAMAGE	( DMG_PARALYZE | DMG_NERVEGAS | DMG_POISON | DMG_RADIATION | DMG_DROWNRECOVER | \
									  DMG_ACID | DMG_SLOWBURN | DMG_SLOWFREEZE )
#define HURT_REMOTE_HEALTH_BUFFER	100000.0f	// SBO: the minimum health (see DoDamage)

//=========================================================
// SBO 2.0: Featureful's DamageInfo options (SetIgnoreArmor, SetNoPunch, SetHealthFloor) around the HL SDK's
// TakeDamage, for trigger_hurt_remote and trigger_hurt. Players only for armor and punch (monsters have neither).
// flFloor > 0: "Minimum health threshold": damage never takes the target below it; a target at or below it takes
// none. The target is lifted far above the threshold for the call, so nothing in TakeDamage kills it, then put back
// and held at the threshold; lasting (time-based) damage types are dropped (they would go on hurting past it).
//=========================================================
void Featureful_TakeDamage( CBaseEntity *pTarget, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage,
	int bitsDamage, float flFloor, BOOL fIgnoreArmor, BOOL fNoPunch )
{
	const BOOL fPlayer = pTarget->IsPlayer();
	const float flArmor = pTarget->pev->armorvalue;
	const Vector vecPunch = pTarget->pev->punchangle;
	fIgnoreArmor = fIgnoreArmor && fPlayer;
	if ( fIgnoreArmor )
		pTarget->pev->armorvalue = 0;

	if ( flFloor > 0 )
	{
		bitsDamage &= ~HURT_REMOTE_LASTING_DAMAGE;
		if ( pTarget->pev->health <= flFloor )
		{
			if ( fIgnoreArmor )
				pTarget->pev->armorvalue = flArmor;
			return;
		}
		pTarget->pev->health += HURT_REMOTE_HEALTH_BUFFER;
	}

	pTarget->TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamage );

	if ( flFloor > 0 && pTarget->pev->health > 0 )
	{
		pTarget->pev->health -= HURT_REMOTE_HEALTH_BUFFER;
		if ( pTarget->pev->health < flFloor )
			pTarget->pev->health = flFloor;
	}
	if ( fIgnoreArmor )
		pTarget->pev->armorvalue = flArmor;
	if ( fPlayer && fNoPunch )
		pTarget->pev->punchangle = vecPunch;
}

class CTriggerHurtRemote : public CPointEntity
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );
	void EXPORT PeriodicHurt( void );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	EHANDLE m_hActivator;

protected:
	void DoDamage( void );
	void DoDamage( CBaseEntity *pTarget );
	string_t TargetClass( void ) const { return pev->netname; }
	float Delay( void ) const { return pev->frags > 0 ? max( pev->frags, 0.01f ) : 0.1f; }	// SBO: never 0 or negative
	BOOL IsActive( void ) const { return FBitSet( pev->spawnflags, SF_TRIGGER_HURT_REMOTE_STARTON ); }
};

LINK_ENTITY_TO_CLASS( trigger_hurt_remote, CTriggerHurtRemote );

TYPEDESCRIPTION CTriggerHurtRemote::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerHurtRemote, m_hActivator, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CTriggerHurtRemote, CPointEntity );

void CTriggerHurtRemote::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "targetclass" ) )
	{
		pev->netname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "delay" ) )
	{
		pev->frags = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "damagetype" ) )
	{
		pev->weapons = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gib_policy" ) )
	{
		pev->impulse = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "min_health" ) )
	{
		pev->dmg_save = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "armordmg" ) )
	{
		pev->armorvalue = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CTriggerHurtRemote::Spawn( void )
{
	CPointEntity::Spawn();
	if ( FBitSet( pev->spawnflags, SF_TRIGGER_HURT_REMOTE_CONSTANT ) && IsActive() )
	{
		SetThink( &CTriggerHurtRemote::PeriodicHurt );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CTriggerHurtRemote::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FBitSet( pev->spawnflags, SF_TRIGGER_HURT_REMOTE_CONSTANT ) )
	{
		if ( ShouldToggle( useType, IsActive() ) )
		{
			if ( IsActive() )
			{
				ClearBits( pev->spawnflags, SF_TRIGGER_HURT_REMOTE_STARTON );
				SetThink( NULL );
				pev->nextthink = 0.0f;
				m_hActivator = NULL;
			}
			else
			{
				SetBits( pev->spawnflags, SF_TRIGGER_HURT_REMOTE_STARTON );
				SetThink( &CTriggerHurtRemote::PeriodicHurt );
				pev->nextthink = gpGlobals->time;
				m_hActivator = pActivator;
			}
		}
	}
	else
	{
		if ( useType != USE_OFF )
		{
			m_hActivator = pActivator;
			DoDamage();
		}
	}
}

void CTriggerHurtRemote::PeriodicHurt( void )
{
	DoDamage();
	pev->nextthink = gpGlobals->time + Delay();
}

void CTriggerHurtRemote::DoDamage( void )
{
	CBaseEntity *pTarget = NULL;
	CBaseEntity *pActivator = m_hActivator;
	if ( !FStringNull( pev->target ) )
	{
		// Featureful: UTIL_FindEntityByTargetname with the activator ("!activator", "*locus", "*player")
		while ( ( pTarget = Locus_FindEntity( pTarget, STRING( pev->target ), pActivator ) ) != NULL )
			DoDamage( pTarget );
	}

	string_t targetClass = TargetClass();
	if ( !FStringNull( targetClass ) )
	{
		pTarget = NULL;
		while ( ( pTarget = UTIL_FindEntityByClassname( pTarget, STRING( targetClass ) ) ) != NULL )
			DoDamage( pTarget );
	}
}

void CTriggerHurtRemote::DoDamage( CBaseEntity *pTarget )
{
	// SBO: a player only while in play (not dead, observing or a spectator: no "corpses" among players)
	if ( pTarget->IsPlayer() && !Logic_IsPlayerInPlay( pTarget ) )
		return;
	if ( !pTarget->IsAlive() && !FBitSet( pev->spawnflags, SF_TRIGGER_HURT_REMOTE_DO_CORPSES ) )
		return;
	// SBO: entities being removed, and entities that cannot take damage or health (Featureful's virtuals did nothing)
	if ( pTarget->pev->flags & FL_KILLME )
		return;

	CBaseEntity *pActivator = m_hActivator;

	if ( pev->armorvalue != 0 && FBitSet( pev->spawnflags, SF_TRIGGER_HURT_REMOTE_DO_ARMOR_DAMAGE ) && pTarget->IsPlayer() )
	{
		// Featureful CBasePlayer::TakeArmor( charger, -armordmg ): armor given up to the maximum, drained down to 0
		float flArmor = pTarget->pev->armorvalue;
		if ( pev->armorvalue < 0 )
		{
			if ( flArmor < MAX_NORMAL_BATTERY )
				flArmor = min( flArmor - pev->armorvalue, (float)MAX_NORMAL_BATTERY );
		}
		else
			flArmor = max( flArmor - pev->armorvalue, 0.0f );
		pTarget->pev->armorvalue = flArmor;
	}

	if ( pev->dmg >= 0 )
	{
		int bitsDamage = pev->weapons;
		switch ( pev->impulse )
		{
		case 1:
			bitsDamage |= DMG_ALWAYSGIB;
			break;
		case 2:
			bitsDamage |= DMG_NEVERGIB;
			break;
		default:
			break;
		}

		// SBO: the activator gets the kill, except a player hurting another player (friendly fire rules)
		entvars_t *pevAttacker = pActivator ? pActivator->pev : pev;
		if ( pActivator && pActivator->IsPlayer() && pTarget->IsPlayer() && pActivator != pTarget )
			pevAttacker = pev;

		// SBO: the entity is the inflictor, so the kill feed reads "with trigger_hurt_remote" (as a trigger_hurt's
		// "trigger_hurt"). Featureful's remote hurt does not push: the push CBaseMonster::TakeDamage gives a
		// walking target (a player) is taken back.
		const BOOL fWalking = pTarget->pev->movetype == MOVETYPE_WALK;
		const Vector vecVelocity = pTarget->pev->velocity;

		if ( FBitSet( pev->spawnflags, SF_TRIGGER_HURT_REMOTE_INSTANT_KILL ) )
		{
			// Featureful: damage = health, pure damage to health, the corpse gibs; non-players always gib
			const BOOL fGib = pev->impulse == 1 || ( pev->impulse != 2 && !pTarget->IsPlayer() );
			Kill_ByDamage( pTarget, pev, pevAttacker, bitsDamage, fGib );
		}
		else
		{
			// SBO: Featureful's DamageInfo options around the HL SDK call
			Featureful_TakeDamage( pTarget, pev, pevAttacker, pev->dmg, bitsDamage, pev->dmg_save,
				FBitSet( pev->spawnflags, SF_TRIGGER_HURT_REMOTE_IGNORE_ARMOR ), FBitSet( pev->spawnflags, SF_TRIGGER_HURT_REMOTE_NO_PUNCH ) );
		}

		if ( fWalking )
			pTarget->pev->velocity = vecVelocity;
	}
	else
	{
		// healing
		if ( FBitSet( pev->spawnflags, SF_TRIGGER_HURT_REMOTE_INSTANT_KILL ) )
			pTarget->TakeHealth( max( pTarget->pev->max_health - pTarget->pev->health, 0.0f ), DMG_GENERIC );
		else
			pTarget->TakeHealth( -pev->dmg, DMG_GENERIC );
	}
}

//=========================================================
// trigger_look
//=========================================================

#define SF_TRIGGERLOOK_FIREONCE		1
#define LOOK_MAX_CLIENTS			32	// the engine's limit (MAX_CLIENTS)

class CTriggerLook : public CBaseDelay
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void );
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Touch( CBaseEntity *pOther );

	EHANDLE m_hLookTarget;
	float m_flFieldOfView;
	float m_flLookTime;
	string_t m_sMaster;
	// SBO: per player (client index - 1): the time looked so far (-1: not looking), when it was last counted, when
	// the player last touched the brush
	float m_flLookTimeTotal[ LOOK_MAX_CLIENTS ];
	float m_flLookTimeLast[ LOOK_MAX_CLIENTS ];
	float m_flTouchTimeLast[ LOOK_MAX_CLIENTS ];

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

LINK_ENTITY_TO_CLASS( trigger_look, CTriggerLook );

TYPEDESCRIPTION CTriggerLook::m_SaveData[] =
{
	DEFINE_FIELD( CTriggerLook, m_hLookTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( CTriggerLook, m_flFieldOfView, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerLook, m_flLookTime, FIELD_FLOAT ),
	DEFINE_FIELD( CTriggerLook, m_sMaster, FIELD_STRING ),
	DEFINE_ARRAY( CTriggerLook, m_flLookTimeTotal, FIELD_FLOAT, LOOK_MAX_CLIENTS ),
	DEFINE_ARRAY( CTriggerLook, m_flLookTimeLast, FIELD_TIME, LOOK_MAX_CLIENTS ),
	DEFINE_ARRAY( CTriggerLook, m_flTouchTimeLast, FIELD_TIME, LOOK_MAX_CLIENTS ),
};

IMPLEMENT_SAVERESTORE( CTriggerLook, CBaseDelay );

void CTriggerLook::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "field_of_view" ) )
	{
		m_flFieldOfView = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "look_time" ) )
	{
		m_flLookTime = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "master" ) )
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerLook::Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	pev->effects |= EF_NODRAW;		// SBO: AAATRIGGER is not drawn (InitTrigger in the HL SDK)

	for ( int i = 0; i < LOOK_MAX_CLIENTS; i++ )
	{
		m_flLookTimeTotal[i] = -1.0f;
		m_flLookTimeLast[i] = 0;
		m_flTouchTimeLast[i] = 0;
	}
}

void CTriggerLook::Touch( CBaseEntity *pOther )
{
	// SBO: players in play only (Featureful: any player); nothing after "Remove On Fire" fired
	if ( pev->solid == SOLID_NOT || !Logic_IsPlayerInPlay( pOther ) || FStringNull( pev->netname ) )
		return;
	if ( !FStringNull( m_sMaster ) && !UTIL_IsMasterTriggered( m_sMaster, pOther ) )
		return;

	const int i = pOther->entindex() - 1;
	if ( i < 0 || i >= LOOK_MAX_CLIENTS )
		return;

	if ( m_hLookTarget == NULL )
	{
		m_hLookTarget = Locus_FindEntity( NULL, STRING( pev->netname ), pOther );
		if ( m_hLookTarget == NULL )
			return;
	}

	if ( gpGlobals->time - m_flTouchTimeLast[i] >= 0.2f )
	{
		// haven't been touched for a while
		m_flLookTimeTotal[i] = -1.0f;
	}

	m_flTouchTimeLast[i] = gpGlobals->time;

	UTIL_MakeVectors( pOther->pev->v_angle );
	const Vector vLookDir = gpGlobals->v_forward;
	const Vector vTargetDir = ( m_hLookTarget->pev->origin - pOther->EyePosition() ).Normalize();

	const float fDotPr = DotProduct( vLookDir, vTargetDir );
	if ( fDotPr > m_flFieldOfView )
	{
		if ( m_flLookTimeTotal[i] == -1.0f )
		{
			m_flLookTimeLast[i] = gpGlobals->time;
			m_flLookTimeTotal[i] = 0;
		}
		else
		{
			m_flLookTimeTotal[i] += gpGlobals->time - m_flLookTimeLast[i];
			m_flLookTimeLast[i] = gpGlobals->time;
		}

		if ( m_flLookTimeTotal[i] >= m_flLookTime )
		{
			m_flLookTimeTotal[i] = -1.0f;
			SUB_UseTargets( pOther, USE_TOGGLE, 0 );
			if ( FBitSet( pev->spawnflags, SF_TRIGGERLOOK_FIREONCE ) )
			{
				// SBO: no more touches once it is done (another player in the same frame would fire it again)
				pev->solid = SOLID_NOT;
				UTIL_SetOrigin( pev, pev->origin );
				SetThink( &CBaseEntity::SUB_Remove );
				pev->nextthink = gpGlobals->time;
			}
		}
	}
	else
	{
		m_flLookTimeTotal[i] = -1.0f;
	}
}
