//=========================================================
// SBO 2.0: player and trigger entities from the Featureful entity guide (see
// docs/FEATUREFUL_GUIDE_ENTITIES.md): player_capabilities, player_speed, player_hasweapon, player_flicker,
// trigger_vehiclecontrols, env_soundmark, ambient_extraspeaker.
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) dlls/player.cpp,
// dlls/plats.cpp and dlls/sound.cpp. Multiplayer: no save/restore; Featureful's "the player" is the activator, a
// player in play (never a spectator, a dead or a waiting player).
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "trains.h"
#include "soundent.h"
#include "pm_shared.h"
#include "round_hooks.h"
#include "locus.h"

// the activator when it is a player in play, else NULL
static CBasePlayer *Guide_PlayerActivator( CBaseEntity *pActivator )
{
	return Logic_IsPlayerInPlay( pActivator ) ? (CBasePlayer *)pActivator : NULL;
}

//=========================================================
// player_capabilities: turns attack, jump, duck, use, step sounds or movement off and on for the activator, until
// another one changes them or he respawns. Each key: -1 Off, 0 No change, 1 On, 2 Toggle, 3 Copy use input (On:
// on, Off: off, else toggle). Featureful's "save game" capability is singleplayer only: left out.
//=========================================================
enum
{
	PLAYER_ABILITY_DISABLE = -1,
	PLAYER_ABILITY_NOCHANGE = 0,
	PLAYER_ABILITY_ENABLE = 1,
	PLAYER_ABILITY_TOGGLE = 2,
	PLAYER_ABILITY_COPYINPUT = 3,
};

class CPlayerCapabilities : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

private:
	static int Configure( int bits, int bit, int setting, USE_TYPE useType );
	int m_settings[6];	// attack, jump, duck, use, step sounds, movement
};

LINK_ENTITY_TO_CLASS( player_capabilities, CPlayerCapabilities );

static const int s_iCapabilityBits[6] =
{
	PLAYER_SUPPRESS_ATTACK, PLAYER_SUPPRESS_JUMP, PLAYER_SUPPRESS_DUCK, PLAYER_SUPPRESS_USE,
	PLAYER_SUPPRESS_STEP_SOUND, PLAYER_SUPPRESS_MOVEMENT
};

void CPlayerCapabilities::KeyValue( KeyValueData *pkvd )
{
	int i = -1;
	if ( FStrEq( pkvd->szKeyName, "attack_capability" ) )
		i = 0;
	else if ( FStrEq( pkvd->szKeyName, "jump_capability" ) )
		i = 1;
	else if ( FStrEq( pkvd->szKeyName, "duck_capability" ) )
		i = 2;
	else if ( FStrEq( pkvd->szKeyName, "use_capability" ) )
		i = 3;
	else if ( FStrEq( pkvd->szKeyName, "stepsound_capability" ) )
		i = 4;
	else if ( FStrEq( pkvd->szKeyName, "movement_capability" ) )
		i = 5;
	else if ( FStrEq( pkvd->szKeyName, "save_capability" ) )
	{
		pkvd->fHandled = TRUE;	// Featureful's save capability: singleplayer only, ignored
		return;
	}
	if ( i < 0 )
	{
		CPointEntity::KeyValue( pkvd );
		return;
	}
	m_settings[i] = atoi( pkvd->szValue );
	pkvd->fHandled = TRUE;
}

int CPlayerCapabilities::Configure( int bits, int bit, int setting, USE_TYPE useType )
{
	if ( setting == PLAYER_ABILITY_COPYINPUT )
	{
		if ( useType == USE_OFF )
			setting = PLAYER_ABILITY_DISABLE;
		else if ( useType == USE_ON )
			setting = PLAYER_ABILITY_ENABLE;
		else
			setting = PLAYER_ABILITY_TOGGLE;
	}
	if ( setting == PLAYER_ABILITY_DISABLE )
		bits |= bit;
	else if ( setting == PLAYER_ABILITY_ENABLE )
		bits &= ~bit;
	else if ( setting == PLAYER_ABILITY_TOGGLE )
		bits ^= bit;
	return bits;
}

void CPlayerCapabilities::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = Guide_PlayerActivator( pActivator );
	if ( !pPlayer )
		return;
	int bits = pPlayer->m_iSuppressedCaps;
	for ( int i = 0; i < 6; i++ )
		bits = Configure( bits, s_iCapabilityBits[i], m_settings[i], useType );
	pPlayer->SetSuppressedCaps( bits );
}

//=========================================================
// player_speed: the activator's maximum speed, until another one changes it, "maxspeed" 0 resets it, or he
// respawns. "value_notion" 0: a fraction of his own speed (class, level, legs); 1: a speed in units per second.
// As in Featureful it is applied to his max speed (CBasePlayer::ApplySpeedOverride), which his client predicts with;
// sv_maxspeed still caps it.
//=========================================================
class CPlayerSpeed : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd )
	{
		if ( FStrEq( pkvd->szKeyName, "maxspeed" ) )
		{
			m_flSpeed = atof( pkvd->szValue );
			if ( !( m_flSpeed >= 0 && m_flSpeed < 100000 ) )
				m_flSpeed = 0;	// negative, NaN, absurd: reset
			pkvd->fHandled = TRUE;
		}
		else if ( FStrEq( pkvd->szKeyName, "value_notion" ) )
		{
			m_iAbsolute = atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else
			CPointEntity::KeyValue( pkvd );
	}
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBasePlayer *pPlayer = Guide_PlayerActivator( pActivator );
		if ( pPlayer )
			pPlayer->SetSpeedOverride( m_flSpeed, m_iAbsolute != 0 );
	}

	float m_flSpeed;
	int m_iAbsolute;
};

LINK_ENTITY_TO_CLASS( player_speed, CPlayerSpeed );

//=========================================================
// player_hasweapon: fires "pass_target" if the activator has the weapon ("weapon_name", a classname; blank: any
// weapon), else "fail_target", with him as activator. As a master: on while the activator has it.
//=========================================================
class CPlayerHasWeapon : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd )
	{
		if ( FStrEq( pkvd->szKeyName, "weapon_name" ) )
		{
			m_iszWeapon = ALLOC_STRING( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else if ( FStrEq( pkvd->szKeyName, "pass_target" ) )
		{
			m_iszPass = ALLOC_STRING( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else if ( FStrEq( pkvd->szKeyName, "fail_target" ) )
		{
			m_iszFail = ALLOC_STRING( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else
			CPointEntity::KeyValue( pkvd );
	}
	virtual int ObjectCaps( void ) { return CPointEntity::ObjectCaps() | FCAP_MASTER; }
	BOOL HasThing( CBasePlayer *pPlayer )
	{
		if ( FStringNull( m_iszWeapon ) )
			return pPlayer->HasWeapons();
		return pPlayer->HasNamedPlayerItem( STRING( m_iszWeapon ) );
	}
	BOOL IsTriggered( CBaseEntity *pActivator )
	{
		CBasePlayer *pPlayer = Guide_PlayerActivator( pActivator );
		return pPlayer ? HasThing( pPlayer ) : FALSE;
	}
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBasePlayer *pPlayer = Guide_PlayerActivator( pActivator );
		if ( !pPlayer )
			return;
		BOOL success = HasThing( pPlayer );
		if ( success && !FStringNull( m_iszPass ) )
			FireTargets( STRING( m_iszPass ), pActivator, this, USE_TOGGLE, 0 );
		if ( !success && !FStringNull( m_iszFail ) )
			FireTargets( STRING( m_iszFail ), pActivator, this, USE_TOGGLE, 0 );
	}

	string_t m_iszWeapon;
	string_t m_iszPass;
	string_t m_iszFail;
};

LINK_ENTITY_TO_CLASS( player_hasweapon, CPlayerHasWeapon );

//=========================================================
// player_flicker: the activator's flashlight flickers (On), stops flickering (Off), Toggle by its state
//=========================================================
class CPlayerFlicker : public CPointEntity
{
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBasePlayer *pPlayer = Guide_PlayerActivator( pActivator );
		if ( pPlayer && ShouldToggle( useType, pPlayer->m_bFlashlightFlicker ) )
			pPlayer->SetFlashlightFlicker( !pPlayer->m_bFlashlightFlicker );
	}
};

LINK_ENTITY_TO_CLASS( player_flicker, CPlayerFlicker );

//=========================================================
// trigger_vehiclecontrols: locks or unlocks the controls of the func_tracktrain / func_vehicle named "target"
// (their "No User Control" flag): On unlocks, Off locks, Toggle switches
//=========================================================
class CTriggerVehicleControl : public CPointEntity
{
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if ( FStringNull( pev->target ) )
		{
			ALERT( at_console, "%s \"%s\" has no target\n", STRING( pev->classname ), STRING( pev->targetname ) );
			return;
		}
		CBaseEntity *pEntity = NULL;
		while ( ( pEntity = Locus_FindEntity( pEntity, STRING( pev->target ), pActivator ) ) != NULL )
		{
			if ( !FClassnameIs( pEntity->pev, "func_vehicle" ) && !FClassnameIs( pEntity->pev, "func_tracktrain" ) )
				continue;
			if ( ShouldToggle( useType, !FBitSet( pEntity->pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ) ) )
				pEntity->pev->spawnflags ^= SF_TRACKTRAIN_NOCONTROL;
		}
	}
};

LINK_ENTITY_TO_CLASS( trigger_vehiclecontrols, CTriggerVehicleControl );

//=========================================================
// env_soundmark: a sound or smell for the monsters' AI (not heard by players) at a position: Danger makes them
// run from it, Combat draws them in. "Remove On fire" (1).
//=========================================================
#define SF_SOUNDMARK_FIREONCE	1

class CEnvSoundMark : public CPointEntity
{
public:
	void Spawn( void ) { pev->effects |= EF_NODRAW; }
	void KeyValue( KeyValueData *pkvd )
	{
		if ( FStrEq( pkvd->szKeyName, "type" ) )
			pev->impulse = atoi( pkvd->szValue );
		else if ( FStrEq( pkvd->szKeyName, "radius" ) )
			pev->button = atoi( pkvd->szValue );
		else if ( FStrEq( pkvd->szKeyName, "duration" ) )
			pev->frags = atof( pkvd->szValue );
		else if ( FStrEq( pkvd->szKeyName, "position" ) )
			pev->message = ALLOC_STRING( pkvd->szValue );
		else
		{
			CPointEntity::KeyValue( pkvd );
			return;
		}
		pkvd->fHandled = TRUE;
	}
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		Vector vecPos = pev->origin;
		if ( !FStringNull( pev->message ) )
		{
			if ( !TryCalcLocus_Position( this, pActivator, STRING( pev->message ), vecPos ) )
				return;
		}
		int type = pev->impulse ? pev->impulse : bits_SOUND_DANGER;
		int radius = pev->button > 0 ? pev->button : 384;
		float duration = pev->frags > 0 ? pev->frags : 0.3;
		CSoundEnt::InsertSound( type, vecPos, radius, duration );

		// SBO: monsters in range that care about this sound think again now. Half-Life schedules filter sounds (a
		// scientist greeting a player hears no danger), so a mark placed while they idle near players changed
		// nothing (seen in a test); this only reaches the monsters of this one mark.
		CBaseEntity *pEnt = NULL;
		while ( ( pEnt = UTIL_FindEntityInSphere( pEnt, vecPos, radius ) ) != NULL )
		{
			CBaseMonster *pMonster = pEnt->MyMonsterPointer();
			if ( !pMonster || !FBitSet( pMonster->pev->flags, FL_MONSTER ) || !pMonster->IsAlive() || pMonster->m_pCine )
				continue;
			if ( pMonster->m_MonsterState != MONSTERSTATE_IDLE && pMonster->m_MonsterState != MONSTERSTATE_ALERT )
				continue;
			if ( !( pMonster->ISoundMask() & type ) )
				continue;
			pMonster->ClearSchedule();	// no old schedule's filter: it hears this sound now
			pMonster->Listen();
		}
		if ( FBitSet( pev->spawnflags, SF_SOUNDMARK_FIREONCE ) )
			UTIL_Remove( this );
	}
};

LINK_ENTITY_TO_CLASS( env_soundmark, CEnvSoundMark );

//=========================================================
// ambient_extraspeaker: plays a sound (a .wav or a "!sentence") on another entity, through an invisible speaker that
// follows it, so the sound moves with it and never cuts the entity's own sounds. "target": the entity (blank /
// *player / !activator: the activator); "channel" 0 = Static. Volume "health" (10 = loudest), pitch, the radius
// flags of ambient_generic; "Start Silent" (16) and "Not Toggled" (32: each trigger plays it once). Without "Not
// Toggled" a trigger starts it, the next stops it. Players who join later do not hear a sound already playing.
// Featureful's dynamic presets (spin, fade, LFO) are not ported.
//=========================================================
#define SF_EXTRASPEAKER_EVERYWHERE		1
#define SF_EXTRASPEAKER_SMALLRADIUS		2
#define SF_EXTRASPEAKER_MEDIUMRADIUS	4
#define SF_EXTRASPEAKER_LARGERADIUS		8
#define SF_EXTRASPEAKER_START_SILENT	16
#define SF_EXTRASPEAKER_NOT_TOGGLED		32

class CExtraSpeaker : public CPointEntity
{
public:
	// its own model too (ambient_extraspeaker precaches it): a map that places extra_speaker itself gets no
	// unprecached SET_MODEL (Host_Error on stock engines); at run time the model is already in the list
	void Precache( void ) { PRECACHE_MODEL( "sprites/iunknown.spr" ); }
	void Spawn( void )
	{
		Precache();
		pev->movetype = MOVETYPE_FOLLOW;
		pev->solid = SOLID_NOT;
		SET_MODEL( edict(), "sprites/iunknown.spr" );	// a model, so the clients know where the sound is
		pev->rendermode = kRenderTransAlpha;
		pev->renderamt = 0;
		pev->nextthink = gpGlobals->time + 1.0;
	}
	void Think( void )
	{
		// gone with its entity
		if ( FNullEnt( pev->aiment ) || !CBaseEntity::Instance( pev->aiment ) )
		{
			UTIL_Remove( this );
			return;
		}
		pev->nextthink = gpGlobals->time + 1.0;
	}
};

LINK_ENTITY_TO_CLASS( extra_speaker, CExtraSpeaker );

static CBaseEntity *ExtraSpeaker_For( CBaseEntity *pTarget )
{
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "extra_speaker" ) ) != NULL )
	{
		if ( pEntity->pev->aiment == pTarget->edict() )
			return pEntity;
	}
	pEntity = CBaseEntity::Create( "extra_speaker", pTarget->pev->origin, pTarget->pev->angles, NULL );
	if ( pEntity )
		pEntity->pev->aiment = pTarget->edict();
	return pEntity;
}

class CAmbientExtraSpeaker : public CPointEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT StartThink( void );
	void Play( CBaseEntity *pActivator );
	void Stop( void );

	int m_iChannel;
	int m_iPitch;
	float m_flAttenuation;
	BOOL m_fActive;
	EHANDLE m_hSpeaker;
};

LINK_ENTITY_TO_CLASS( ambient_extraspeaker, CAmbientExtraSpeaker );

void CAmbientExtraSpeaker::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "channel" ) )
	{
		m_iChannel = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "pitch" ) )
	{
		m_iPitch = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CAmbientExtraSpeaker::Precache( void )
{
	PRECACHE_MODEL( "sprites/iunknown.spr" );
	const char *szSound = STRING( pev->message );
	if ( !FStringNull( pev->message ) && szSound[0] && szSound[0] != '!' )
		PRECACHE_SOUND( (char *)szSound );
}

void CAmbientExtraSpeaker::Spawn( void )
{
	if ( FStringNull( pev->message ) || !( STRING( pev->message ) )[0] )
	{
		ALERT( at_console, "%s \"%s\" has no sound\n", STRING( pev->classname ), STRING( pev->targetname ) );
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}
	Precache();
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	if ( m_iChannel <= 0 || m_iChannel > CHAN_STATIC )
		m_iChannel = CHAN_STATIC;
	if ( m_iPitch <= 0 || m_iPitch > 255 )
		m_iPitch = PITCH_NORM;
	if ( pev->health <= 0 || pev->health > 10 )
		pev->health = 10;

	if ( FBitSet( pev->spawnflags, SF_EXTRASPEAKER_EVERYWHERE ) )
		m_flAttenuation = ATTN_NONE;
	else if ( FBitSet( pev->spawnflags, SF_EXTRASPEAKER_SMALLRADIUS ) )
		m_flAttenuation = ATTN_IDLE;
	else if ( FBitSet( pev->spawnflags, SF_EXTRASPEAKER_MEDIUMRADIUS ) )
		m_flAttenuation = ATTN_STATIC;
	else if ( FBitSet( pev->spawnflags, SF_EXTRASPEAKER_LARGERADIUS ) )
		m_flAttenuation = ATTN_NORM;
	else
		m_flAttenuation = ATTN_STATIC;

	// a looping speaker that does not start silent starts on its (named) entity once the map is up
	if ( !FBitSet( pev->spawnflags, SF_EXTRASPEAKER_START_SILENT | SF_EXTRASPEAKER_NOT_TOGGLED ) )
	{
		SetThink( &CAmbientExtraSpeaker::StartThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CAmbientExtraSpeaker::StartThink( void )
{
	Play( NULL );
	m_fActive = TRUE;
}

void CAmbientExtraSpeaker::Play( CBaseEntity *pActivator )
{
	CBaseEntity *pTarget = NULL;
	if ( FStringNull( pev->target ) || FStrEq( STRING( pev->target ), "*player" ) )
		pTarget = Guide_PlayerActivator( pActivator );
	else
		pTarget = Locus_FindEntity( NULL, STRING( pev->target ), pActivator );
	if ( !pTarget )
		return;

	Stop();
	CBaseEntity *pSpeaker = ExtraSpeaker_For( pTarget );
	if ( !pSpeaker )
		return;
	m_hSpeaker = pSpeaker;
	EMIT_SOUND_DYN( pSpeaker->edict(), m_iChannel, STRING( pev->message ), pev->health * 0.1, m_flAttenuation, 0, m_iPitch );
}

void CAmbientExtraSpeaker::Stop( void )
{
	if ( m_hSpeaker != NULL )
		STOP_SOUND( m_hSpeaker->edict(), m_iChannel, STRING( pev->message ) );
}

void CAmbientExtraSpeaker::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FBitSet( pev->spawnflags, SF_EXTRASPEAKER_NOT_TOGGLED ) )
	{
		Play( pActivator );	// once per trigger (a sound still playing starts over)
		return;
	}
	if ( !ShouldToggle( useType, m_fActive ) )
		return;
	if ( m_fActive )
	{
		Stop();
		m_fActive = FALSE;
	}
	else
	{
		Play( pActivator );
		m_fActive = TRUE;
	}
}
