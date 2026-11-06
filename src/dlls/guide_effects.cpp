//=========================================================
// SBO 2.0: effect entities from the Featureful entity guide (docs/FEATUREFUL_GUIDE_ENTITIES.md):
// env_warpball, env_shockwave, env_beamtrail, env_quakefx, env_decal.
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) dlls/effects.cpp
// and dlls/warpball.cpp; env_shockwave, env_beamtrail, env_quakefx and env_decal come from Spirit of Half-Life
// (Laurie Cheers). Multiplayer: no save/restore; env_beamtrail sends its trails to players who join later.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "effects.h"
#include "decals.h"
#include "soundent.h"
#include "weapons.h"	// RadiusDamage
#include "locus.h"
#include "guide_entities.h"

//=========================================================
// env_warpball: the Xen teleport effect (sprites, chaotic beams, sound, shake, light) at a position, on its own or
// for a monstermaker's "warpball" key (GuideWarpball_Play).
//=========================================================
#define SF_WARPBALL_REMOVE_ON_FIRE	0x0001
#define SF_WARPBALL_KILL_CENTER		0x0002
#define SF_WARPBALL_NOSHAKE			0x0004
#define SF_WARPBALL_DYNLIGHT		0x0008
#define SF_WARPBALL_NOSOUND			0x0010

#define WARPBALL_RED_DEFAULT		77
#define WARPBALL_GREEN_DEFAULT		210
#define WARPBALL_BLUE_DEFAULT		130
#define WARPBALL_BEAM_RED_DEFAULT	20
#define WARPBALL_BEAM_GREEN_DEFAULT	243
#define WARPBALL_BEAM_BLUE_DEFAULT	20
#define WARPBALL_SPRITE				"sprites/fexplo1.spr"
#define WARPBALL_BEAM				"sprites/lgtning.spr"
#define WARPBALL_SOUND1				"debris/beamstart2.wav"
#define WARPBALL_SOUND2				"debris/beamstart7.wav"
#define WARPBALL_DAMAGE_RADIUS		48
#define WARPBALL_DAMAGE				300.0f

// Featureful DrawChaoticBeam / DrawChaoticBeams: beams from the center to the walls within the radius; if fewer
// than half hit something, the rest go out at half the radius, ignoring walls
static void Warpball_ChaoticBeam( const Vector &vecOrigin, const Vector &vecDest, int texture, int r, int g, int b )
{
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_COORD( vecDest.x );
		WRITE_COORD( vecDest.y );
		WRITE_COORD( vecDest.z );
		WRITE_SHORT( texture );
		WRITE_BYTE( 0 );	// frame start
		WRITE_BYTE( 10 );	// frame rate
		WRITE_BYTE( RANDOM_LONG( 5, 16 ) );	// life 0.5-1.6 s
		WRITE_BYTE( 30 );	// width
		WRITE_BYTE( 65 );	// noise
		WRITE_BYTE( r );
		WRITE_BYTE( g );
		WRITE_BYTE( b );
		WRITE_BYTE( 220 );	// brightness
		WRITE_BYTE( 35 );	// speed
	MESSAGE_END();
}

static void Warpball_ChaoticBeams( const Vector &vecOrigin, int radius, int iBeams, int texture, int r, int g, int b )
{
	int iTimes = 0;
	int iDrawn = 0;
	while ( iDrawn < iBeams && iTimes < iBeams * 2 )
	{
		TraceResult tr;
		Vector vecDest = vecOrigin + radius * Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ) ).Normalize();
		UTIL_TraceLine( vecOrigin, vecDest, ignore_monsters, NULL, &tr );
		if ( tr.flFraction != 1.0 )
		{
			iDrawn++;
			Warpball_ChaoticBeam( vecOrigin, tr.vecEndPos, texture, r, g, b );
		}
		iTimes++;
	}

	if ( iDrawn < iBeams / 2 )
	{
		int iRest = iBeams - iDrawn;
		if ( iRest > iBeams / 2 )
			iRest = iBeams / 2;
		for ( int i = 0; i < iRest; i++ )
		{
			Vector vecDest = vecOrigin + radius * 0.5f * Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ) );
			Warpball_ChaoticBeam( vecOrigin, vecDest, texture, r, g, b );
		}
	}
}

// Featureful CWarpballHurt: the "Kill center" damage after "Damage delay"
class CWarpballHurt : public CPointEntity
{
public:
	void Think( void )
	{
		::RadiusDamage( pev->origin, pev, pev, pev->dmg, pev->button, CLASS_NONE, DMG_SHOCK );
		UTIL_Remove( this );
	}
};

LINK_ENTITY_TO_CLASS( warpball_hurt, CWarpballHurt );

class CEnvWarpBall : public CBaseEntity
{
public:
	void Precache( void );
	void Spawn( void ) { Precache(); }
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Play( const Vector &vecOrigin, edict_t *pSoundEnt );

	float Radius( void ) { return m_baseRadius > 0 ? m_baseRadius : 192; }
	float Amplitude( void ) { return pev->friction ? pev->friction : 6; }
	float Frequency( void ) { return pev->dmg_save ? pev->dmg_save : 160; }
	float Duration( void ) { return pev->dmg_take ? pev->dmg_take : 1; }
	float Scale( void ) { return pev->scale > 0 ? pev->scale : 1; }
	int MaxBeamCount( void ) { return m_maxBeamCount > 0 ? m_maxBeamCount : 20; }
	float SoundVolume( void ) { return ( m_soundVolume > 0.0f && m_soundVolume <= 1.0f ) ? m_soundVolume : 1.0f; }
	float RenderAmount( void ) { return pev->renderamt ? pev->renderamt : 255; }
	int RenderFx( void ) { return pev->renderfx ? pev->renderfx : kRenderFxNoDissipation; }
	int RenderMode( void ) { return pev->rendermode ? pev->rendermode : kRenderGlow; }
	float SpriteFramerate( void ) { return pev->framerate ? pev->framerate : 12; }
	const char *Sound1( void ) { return !FStringNull( pev->noise1 ) ? STRING( pev->noise1 ) : WARPBALL_SOUND1; }
	// the second sound: noise2, or the stock one when noise1 is not set either
	const char *Sound2( void )
	{
		if ( !FStringNull( pev->noise2 ) )
			return STRING( pev->noise2 );
		return FStringNull( pev->noise1 ) ? WARPBALL_SOUND2 : NULL;
	}

	int m_maxBeamCount;
	int m_baseRadius;
	int m_aiSound;
	float m_aiSoundDuration;
	float m_soundVolume;
	string_t m_model2;
	Vector m_rendercolor2;
	int m_renderamt2;
	float m_scale2;
	float m_framerate2;
	Vector m_beamColor;
	int m_beamTexture;
};

LINK_ENTITY_TO_CLASS( env_warpball, CEnvWarpBall );

void CEnvWarpBall::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "amplitude" ) )
	{
		pev->friction = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "frequency" ) )
	{
		pev->dmg_save = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "duration" ) )
	{
		pev->dmg_take = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "radius" ) )
	{
		m_baseRadius = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "warp_target" ) )
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "damage_delay" ) )
	{
		pev->frags = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "beamcolor" ) )
	{
		UTIL_StringToVector( (float *)m_beamColor, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "beamcount" ) )
	{
		m_maxBeamCount = atoi( pkvd->szValue );
		if ( m_maxBeamCount > 64 )
			m_maxBeamCount = 64;	// one message per beam
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "volume" ) )
	{
		m_soundVolume = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "aisound" ) )
	{
		m_aiSound = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "aisound_duration" ) )
	{
		m_aiSoundDuration = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "model2" ) )
	{
		m_model2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "rendercolor2" ) )
	{
		UTIL_StringToVector( (float *)m_rendercolor2, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "renderamt2" ) )
	{
		m_renderamt2 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "scale2" ) )
	{
		m_scale2 = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "framerate2" ) )
	{
		m_framerate2 = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CEnvWarpBall::Precache( void )
{
	m_beamTexture = PRECACHE_MODEL( WARPBALL_BEAM );
	if ( !FStringNull( pev->model ) )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
	else
		PRECACHE_MODEL( WARPBALL_SPRITE );
	if ( !FStringNull( m_model2 ) )
		PRECACHE_MODEL( (char *)STRING( m_model2 ) );

	PRECACHE_SOUND( (char *)Sound1() );
	if ( Sound2() )
		PRECACHE_SOUND( (char *)Sound2() );
}

static void Warpball_Sprite( const char *szSprite, const Vector &vecOrigin, float framerate, int rendermode, const Vector &color, float alpha, int renderfx, float scale )
{
	CSprite *pSpr = CSprite::SpriteCreate( szSprite, vecOrigin, TRUE );
	if ( !pSpr )
		return;
	pSpr->AnimateAndDie( framerate );
	pSpr->SetTransparency( rendermode, (int)color.x, (int)color.y, (int)color.z, (int)alpha, renderfx );
	pSpr->SetScale( scale > 0 ? scale : 1.0f );
}

// Featureful PlayWarpballEffect for an env_warpball: sounds (at pSoundEnt), shake, sprites, light, beams, AI sound
void CEnvWarpBall::Play( const Vector &vecOrigin, edict_t *pSoundEnt )
{
	if ( !FBitSet( pev->spawnflags, SF_WARPBALL_NOSOUND ) )
	{
		UTIL_EmitAmbientSound( pSoundEnt, vecOrigin, Sound1(), SoundVolume(), ATTN_NORM, 0, PITCH_NORM );
		if ( Sound2() )
			UTIL_EmitAmbientSound( pSoundEnt, vecOrigin, Sound2(), SoundVolume(), ATTN_NORM, 0, PITCH_NORM );
	}

	if ( !FBitSet( pev->spawnflags, SF_WARPBALL_NOSHAKE ) )
		UTIL_ScreenShake( vecOrigin, Amplitude(), Frequency(), Duration(), Radius() );

	Vector color = pev->rendercolor;
	if ( color == g_vecZero )
		color = Vector( WARPBALL_RED_DEFAULT, WARPBALL_GREEN_DEFAULT, WARPBALL_BLUE_DEFAULT );

	Warpball_Sprite( !FStringNull( pev->model ) ? STRING( pev->model ) : WARPBALL_SPRITE, vecOrigin, SpriteFramerate(),
		RenderMode(), color, RenderAmount(), RenderFx(), Scale() );
	if ( !FStringNull( m_model2 ) )
	{
		Vector color2 = ( m_rendercolor2 == g_vecZero ) ? color : m_rendercolor2;
		Warpball_Sprite( STRING( m_model2 ), vecOrigin, m_framerate2 > 0 ? m_framerate2 : SpriteFramerate(), RenderMode(),
			color2, m_renderamt2 > 0 ? m_renderamt2 : RenderAmount(), RenderFx(), m_scale2 > 0 ? m_scale2 : Scale() );
	}

	if ( FBitSet( pev->spawnflags, SF_WARPBALL_DYNLIGHT ) )
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
			WRITE_BYTE( TE_DLIGHT );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z );
			WRITE_BYTE( (int)( 200 * Scale() * 0.1f ) );	// radius * 0.1
			WRITE_BYTE( (int)color.x );
			WRITE_BYTE( (int)color.y );
			WRITE_BYTE( (int)color.z );
			WRITE_BYTE( 15 );	// life 1.5 s
			WRITE_BYTE( 7 );	// decay
		MESSAGE_END();
	}

	Vector beam = m_beamColor;
	if ( beam == g_vecZero )
		beam = Vector( WARPBALL_BEAM_RED_DEFAULT, WARPBALL_BEAM_GREEN_DEFAULT, WARPBALL_BEAM_BLUE_DEFAULT );
	int iBeams = RANDOM_LONG( MaxBeamCount() / 2, MaxBeamCount() );
	Warpball_ChaoticBeams( vecOrigin, (int)Radius(), iBeams, m_beamTexture, (int)beam.x, (int)beam.y, (int)beam.z );

	if ( m_aiSound )
		CSoundEnt::InsertSound( m_aiSound, vecOrigin, (int)Radius(), m_aiSoundDuration > 0.0f ? m_aiSoundDuration : 0.3f );
}

void CEnvWarpBall::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecOrigin = pev->origin;	// no "Warp position": here
	if ( !FStringNull( pev->message ) )
	{
		if ( !TryCalcLocus_Position( this, pActivator, STRING( pev->message ), vecOrigin ) )
			return;
	}

	Play( vecOrigin, edict() );
	SUB_UseTargets( this, USE_TOGGLE, 0 );

	if ( FBitSet( pev->spawnflags, SF_WARPBALL_KILL_CENTER ) )
	{
		if ( pev->frags == 0 )
			::RadiusDamage( vecOrigin, pev, pev, WARPBALL_DAMAGE, WARPBALL_DAMAGE_RADIUS, CLASS_NONE, DMG_SHOCK );
		else
		{
			CBaseEntity *pHurt = CBaseEntity::Create( "warpball_hurt", vecOrigin, g_vecZero, edict() );
			if ( pHurt )
			{
				pHurt->pev->dmg = WARPBALL_DAMAGE;
				pHurt->pev->button = WARPBALL_DAMAGE_RADIUS;
				pHurt->pev->nextthink = gpGlobals->time + pev->frags;
			}
		}
	}

	if ( FBitSet( pev->spawnflags, SF_WARPBALL_REMOVE_ON_FIRE ) )
		UTIL_Remove( this );
}

// monstermaker "warpball": the named env_warpball plays at the new monster (its sound on the maker), without its
// own targets, damage or removal (Featureful StartWarpballEffect, USE_SET path)
BOOL GuideWarpball_Play( const char *szName, const Vector &vecOrigin, edict_t *pSoundEnt )
{
	CBaseEntity *pEnt = UTIL_FindEntityByTargetname( NULL, szName );
	while ( pEnt && !FClassnameIs( pEnt->pev, "env_warpball" ) )
		pEnt = UTIL_FindEntityByTargetname( pEnt, szName );
	if ( !pEnt )
		return FALSE;
	( (CEnvWarpBall *)pEnt )->Play( vecOrigin, pSoundEnt );
	return TRUE;
}

//=========================================================
// env_shockwave: a beam ring (houndeye wave), cylinder, torus or disk at a position
//=========================================================
#define SF_SHOCKWAVE_CENTERED	1
#define SF_SHOCKWAVE_REPEATABLE	2

class CEnvShockwave : public CPointEntity
{
public:
	void Precache( void );
	void Spawn( void ) { Precache(); }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );

	int m_iTime;
	int m_iRadius;
	int m_iHeight;
	int m_iScrollRate;
	int m_iNoise;
	int m_iFrameRate;
	int m_iStartFrame;
	int m_iSpriteTexture;
	int m_cType;
	string_t m_iszPosition;
};

LINK_ENTITY_TO_CLASS( env_shockwave, CEnvShockwave );

void CEnvShockwave::Precache( void )
{
	if ( FStringNull( pev->netname ) )
		pev->netname = MAKE_STRING( "sprites/shockwave.spr" );	// the FGD default, for a key left out
	m_iSpriteTexture = PRECACHE_MODEL( (char *)STRING( pev->netname ) );
}

void CEnvShockwave::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iTime" ) )
		m_iTime = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iRadius" ) )
		m_iRadius = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iHeight" ) )
		m_iHeight = atoi( pkvd->szValue ) / 2;	// Spirit: the drawn height is doubled
	else if ( FStrEq( pkvd->szKeyName, "m_iScrollRate" ) )
		m_iScrollRate = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iNoise" ) )
		m_iNoise = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iFrameRate" ) )
		m_iFrameRate = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iStartFrame" ) )
		m_iStartFrame = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_iszPosition" ) )
		m_iszPosition = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "m_cType" ) )
		m_cType = atoi( pkvd->szValue );
	else
	{
		CPointEntity::KeyValue( pkvd );
		return;
	}
	pkvd->fHandled = TRUE;
}

void CEnvShockwave::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecPos = pev->origin;
	if ( !FStringNull( m_iszPosition ) )
	{
		if ( !TryCalcLocus_Position( this, pActivator, STRING( m_iszPosition ), vecPos ) )
			return;
	}

	if ( !( pev->spawnflags & SF_SHOCKWAVE_CENTERED ) )
		vecPos.z += m_iHeight;

	int type = m_cType;
	if ( type != TE_BEAMTORUS && type != TE_BEAMDISK && type != TE_BEAMCYLINDER )
		type = TE_BEAMCYLINDER;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecPos );
		WRITE_BYTE( type );
		WRITE_COORD( vecPos.x );
		WRITE_COORD( vecPos.y );
		WRITE_COORD( vecPos.z );
		WRITE_COORD( vecPos.x );
		WRITE_COORD( vecPos.y );
		WRITE_COORD( vecPos.z + m_iRadius );	// the final radius
		WRITE_SHORT( m_iSpriteTexture );
		WRITE_BYTE( m_iStartFrame );
		WRITE_BYTE( m_iFrameRate );	// 0.1's
		WRITE_BYTE( m_iTime );		// life in 0.1's
		WRITE_BYTE( m_iHeight );	// line width in 0.1's
		WRITE_BYTE( m_iNoise );		// noise amplitude in 0.01's
		WRITE_BYTE( (int)pev->rendercolor.x );
		WRITE_BYTE( (int)pev->rendercolor.y );
		WRITE_BYTE( (int)pev->rendercolor.z );
		WRITE_BYTE( (int)pev->renderamt );
		WRITE_BYTE( m_iScrollRate );	// 0.1's
	MESSAGE_END();

	if ( !( pev->spawnflags & SF_SHOCKWAVE_REPEATABLE ) )
	{
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time;
	}
}

//=========================================================
// env_beamtrail: a TE_BEAMFOLLOW trail behind its target (or itself) while it moves. On: a trail; Off: the
// target's beams killed; Toggle: by state. SBO: while on, every player gets the trail when the target is in his PVS
// (players who join later, and players who come back into view: the client drops a follow beam whose entity leaves
// its PVS; in Featureful a trail sent at map start reached nobody, so "Start on" showed nothing in multiplayer).
//=========================================================
#define SF_BEAMTRAIL_OFF	1

class CEnvBeamTrail : public CPointEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT StartTrailThink( void );
	void EXPORT CatchUpThink( void );
	void Affect( CBaseEntity *pTarget, USE_TYPE useType, CBasePlayer *pOnly );
	void ForTargets( CBaseEntity *pActivator, USE_TYPE useType, CBasePlayer *pOnly );
	BOOL ClientReady( CBasePlayer *pPlayer );
	BOOL TargetVisibleTo( CBasePlayer *pPlayer );

	int m_iSprite;
	int m_iSentUserId[33];	// per client slot: the user id the current trail was sent to (0: not sent)
	EHANDLE m_hActivator;	// for "!activator" targets of a catch-up send
};

LINK_ENTITY_TO_CLASS( env_beamtrail, CEnvBeamTrail );

void CEnvBeamTrail::Precache( void )
{
	if ( FStringNull( pev->netname ) )
		pev->netname = MAKE_STRING( "sprites/smoke.spr" );	// the FGD default, for a key left out
	m_iSprite = PRECACHE_MODEL( (char *)STRING( pev->netname ) );
}

void CEnvBeamTrail::Spawn( void )
{
	Precache();
	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	memset( m_iSentUserId, 0, sizeof( m_iSentUserId ) );

	if ( !( pev->spawnflags & SF_BEAMTRAIL_OFF ) )
	{
		SetThink( &CEnvBeamTrail::StartTrailThink );
		pev->nextthink = gpGlobals->time + 0.1f;
	}
}

void CEnvBeamTrail::StartTrailThink( void )
{
	pev->spawnflags |= SF_BEAMTRAIL_OFF;	// fake turning off, so the Use turns it on properly
	Use( this, this, USE_ON, 0 );
}

BOOL CEnvBeamTrail::ClientReady( CBasePlayer *pPlayer )
{
	return pPlayer && ( pPlayer->pev->flags & FL_CLIENT ) && pPlayer->m_fGameHUDInitialized && !pPlayer->m_fInitHUD;
}

// a follow trail lives on a client only while its entity is in that client's PVS (the client drops the beam when the
// entity stops being updated): a trail is sent to a player when the target is in his PVS, and again each time the
// target comes back into it
BOOL CEnvBeamTrail::TargetVisibleTo( CBasePlayer *pPlayer )
{
	CBaseEntity *pTarget = FStringNull( pev->target ) ? (CBaseEntity *)this : Locus_FindEntity( NULL, STRING( pev->target ), m_hActivator );
	if ( !pTarget )
		return FALSE;
	Vector vecView = pPlayer->pev->origin + pPlayer->pev->view_ofs;
	unsigned char *pvs = ENGINE_SET_PVS( (float *)&vecView );
	return ENGINE_CHECK_VISIBILITY( pTarget->edict(), pvs ) ? TRUE : FALSE;
}

void CEnvBeamTrail::Affect( CBaseEntity *pTarget, USE_TYPE useType, CBasePlayer *pOnly )
{
	if ( pOnly )
		MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, NULL, pOnly->edict() );
	else
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
	if ( useType == USE_ON || pev->spawnflags & SF_BEAMTRAIL_OFF )
	{
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( pTarget->entindex() );
		WRITE_SHORT( m_iSprite );
		WRITE_BYTE( (int)( pev->health * 10 ) );	// life
		WRITE_BYTE( (int)pev->armorvalue );		// width
		WRITE_BYTE( (int)pev->rendercolor.x );
		WRITE_BYTE( (int)pev->rendercolor.y );
		WRITE_BYTE( (int)pev->rendercolor.z );
		WRITE_BYTE( (int)pev->renderamt );
	}
	else
	{
		WRITE_BYTE( TE_KILLBEAM );
		WRITE_SHORT( pTarget->entindex() );
	}
	MESSAGE_END();
}

void CEnvBeamTrail::ForTargets( CBaseEntity *pActivator, USE_TYPE useType, CBasePlayer *pOnly )
{
	if ( !FStringNull( pev->target ) )
	{
		int iGuard = 0;
		CBaseEntity *pTarget = Locus_FindEntity( NULL, STRING( pev->target ), pActivator );
		while ( pTarget && iGuard++ < 256 )
		{
			Affect( pTarget, useType, pOnly );
			pTarget = Locus_FindEntity( pTarget, STRING( pev->target ), pActivator );
		}
	}
	else
		Affect( this, useType, pOnly );
}

void CEnvBeamTrail::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FStringNull( pev->target ) && !ShouldToggle( useType, !( pev->spawnflags & SF_BEAMTRAIL_OFF ) ) )
		return;
	ForTargets( pActivator, useType, NULL );

	if ( useType == USE_ON )
		pev->spawnflags &= ~SF_BEAMTRAIL_OFF;
	else if ( useType == USE_OFF )
		pev->spawnflags |= SF_BEAMTRAIL_OFF;
	else
		pev->spawnflags ^= SF_BEAMTRAIL_OFF;

	// the players who got this send; the rest get it when they are in the game (CatchUpThink)
	m_hActivator = pActivator;
	for ( int i = 1; i <= gpGlobals->maxClients && i < 33; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		m_iSentUserId[i] = ( ClientReady( pPlayer ) && TargetVisibleTo( pPlayer ) ) ? GETPLAYERUSERID( pPlayer->edict() ) : 0;
	}
	if ( !( pev->spawnflags & SF_BEAMTRAIL_OFF ) )
	{
		SetThink( &CEnvBeamTrail::CatchUpThink );
		pev->nextthink = gpGlobals->time + 0.5f;
	}
	else
		SetThink( NULL );
}

void CEnvBeamTrail::CatchUpThink( void )
{
	if ( pev->spawnflags & SF_BEAMTRAIL_OFF )
		return;
	for ( int i = 1; i <= gpGlobals->maxClients && i < 33; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !ClientReady( pPlayer ) )
			continue;
		if ( !TargetVisibleTo( pPlayer ) )
		{
			m_iSentUserId[i] = 0;	// his client dropped the trail (or never had it): send it when he sees the target
			continue;
		}
		int userid = GETPLAYERUSERID( pPlayer->edict() );
		if ( m_iSentUserId[i] == userid )
			continue;
		m_iSentUserId[i] = userid;
		ForTargets( m_hActivator, USE_ON, pPlayer );
	}
	pev->nextthink = gpGlobals->time + 0.5f;
}

//=========================================================
// env_quakefx: Quake 1 particle effects at a position
//=========================================================
#define SF_QUAKEFX_REPEATABLE	1

class CEnvQuakeFx : public CPointEntity
{
public:
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( env_quakefx, CEnvQuakeFx );

void CEnvQuakeFx::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecPos = pev->origin;
	if ( !FStringNull( pev->message ) )
	{
		if ( !TryCalcLocus_Position( this, pActivator, STRING( pev->message ), vecPos ) )
			return;
	}

	int type = pev->impulse;
	if ( type != TE_TAREXPLOSION && type != TE_LAVASPLASH && type != TE_TELEPORT && type != TE_EXPLOSION2
		&& type != TE_IMPLOSION && type != TE_PARTICLEBURST )
		type = TE_TAREXPLOSION;	// SBO: only the six FGD choices reach the engine (the FGD default)

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( type );
		WRITE_COORD( vecPos.x );
		WRITE_COORD( vecPos.y );
		WRITE_COORD( vecPos.z );
		if ( type == TE_PARTICLEBURST )
		{
			WRITE_SHORT( (int)pev->armortype );	// radius
			WRITE_BYTE( (int)pev->frags );		// particle color
			WRITE_BYTE( (int)( pev->health * 10 ) );	// duration
		}
		else if ( type == TE_EXPLOSION2 )
		{
			WRITE_BYTE( 0 );	// color (Spirit: no visible effect; 0 colors crashes)
			WRITE_BYTE( 1 );	// number of colors
		}
		else if ( type == TE_IMPLOSION )
		{
			WRITE_BYTE( (int)pev->armortype );	// radius
			WRITE_BYTE( (int)pev->armorvalue );	// count
			WRITE_BYTE( (int)( pev->health * 10 ) );	// duration
		}
	MESSAGE_END();

	if ( !( pev->spawnflags & SF_QUAKEFX_REPEATABLE ) )
	{
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time;
	}
}

//=========================================================
// env_decal: sprays a decal where a trace from the position hits a surface
//=========================================================
class CEnvDecal : public CPointEntity
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( env_decal, CEnvDecal );

void CEnvDecal::Spawn( void )
{
	if ( pev->impulse == 0 )
	{
		pev->skin = FStringNull( pev->noise ) ? -1 : DECAL_INDEX( STRING( pev->noise ) );
		if ( pev->skin < 0 )
			ALERT( at_console, "%s \"%s\" can't find decal \"%s\"\n", STRING( pev->classname ), STRING( pev->targetname ), STRING( pev->noise ) );
	}
}

void CEnvDecal::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int iTexture = 0;

	// SBO has no Opposing Force decals: 9 to 11 take Featureful's fallbacks
	switch ( pev->impulse )
	{
	case 1:  iTexture = DECAL_GUNSHOT1 + RANDOM_LONG( 0, 4 ); break;
	case 2:  iTexture = DECAL_BLOOD1 + RANDOM_LONG( 0, 5 ); break;
	case 3:  iTexture = DECAL_YBLOOD1 + RANDOM_LONG( 0, 5 ); break;
	case 4:  iTexture = DECAL_GLASSBREAK1 + RANDOM_LONG( 0, 2 ); break;
	case 5:  iTexture = DECAL_BIGSHOT1 + RANDOM_LONG( 0, 4 ); break;
	case 6:  iTexture = DECAL_SCORCH1 + RANDOM_LONG( 0, 1 ); break;
	case 7:  iTexture = DECAL_SPIT1 + RANDOM_LONG( 0, 1 ); break;
	case 8:  iTexture = DECAL_SMALLSCORCH1 + RANDOM_LONG( 0, 2 ); break;
	case 9:  iTexture = DECAL_YBLOOD5 + RANDOM_LONG( 0, 1 ); break;
	case 10: iTexture = DECAL_SCORCH1 + RANDOM_LONG( 0, 1 ); break;
	case 11: iTexture = DECAL_SMALLSCORCH1 + RANDOM_LONG( 0, 2 ); break;
	default: break;
	}

	if ( pev->impulse >= 1 && pev->impulse <= 11 )
		iTexture = gDecals[iTexture].index;
	else
		iTexture = (int)pev->skin;	// the custom texture
	if ( iTexture < 0 )
		return;

	Vector vecPos = pev->origin;
	if ( !FStringNull( pev->target ) )
	{
		if ( !TryCalcLocus_Position( this, pActivator, STRING( pev->target ), vecPos ) )
			return;
	}

	Vector vecOffs;
	if ( !FStringNull( pev->netname ) )
	{
		if ( !TryCalcLocus_Velocity( this, pActivator, STRING( pev->netname ), vecOffs ) )
			return;
	}
	else
	{
		UTIL_MakeVectors( pev->angles );
		vecOffs = gpGlobals->v_forward;
	}
	if ( vecOffs == g_vecZero )
		return;
	vecOffs = vecOffs.Normalize() * 4000;

	TraceResult trace;
	UTIL_TraceLine( vecPos, vecPos + vecOffs, ignore_monsters, NULL, &trace );
	if ( trace.flFraction == 1.0 )
		return;	// hit nothing

	int entityIndex = (short)ENTINDEX( trace.pHit );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BSPDECAL );
		WRITE_COORD( trace.vecEndPos.x );
		WRITE_COORD( trace.vecEndPos.y );
		WRITE_COORD( trace.vecEndPos.z );
		WRITE_SHORT( iTexture );
		WRITE_SHORT( entityIndex );
		if ( entityIndex )
			WRITE_SHORT( (int)VARS( trace.pHit )->modelindex );
	MESSAGE_END();
}
