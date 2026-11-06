//=========================================================
// locus_beam -- when triggered, makes a beam between two Locus points or entities (entity - entity, position -
// entity, position - position, position + direction), with its own name and a target fired with the beam as locus
// (docs/LOCUS.md).
//
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) commit 30fc2f71
// (2026-10-30), dlls/locus.cpp CLocusBeam (Laurie Cheers' entity from Spirit of Half-Life). The same class name,
// keys, values and spawnflags.
//
// SBO changes (marked "SBO" below):
//   * Damage amount, Damage type, StartSparks, EndSparks and Decal End work. In Featureful and in
//     Spirit of Half-Life 1.8 the beam is a plain beam entity without a think, so these keys did nothing. The beam
//     does what env_laser does (CLaser::FireAtPoint): every 0.1 s sparks at the chosen ends, damage per second along
//     the beam with the chosen damage type, a scorch decal where it hits a wall. The attacker is the activator (the
//     kill credit), except for a player hurting another player: then the beam is the attacker, as for env_laser,
//     so co-op's friendly fire rule does not stop a map hazard. Players are hit only while in play.
//   * A start or end entity that is a point entity (info_target, path_corner, an entity without a model) is taken
//     as its position, as env_beam does: the clients never get point entities, so Featureful's entity beam to one was
//     not drawn (only its sparks and damage, done on the server, showed).
//   * Ring is not offered: a beam entity cannot be drawn as a ring by the GoldSrc engine (only the TE_BEAMRING
//     temporary effect can), so it did nothing in Featureful either. The bit is ignored.
//   * No beam is made close to the edict limit (as locus_variable's markers); an empty sprite name takes the FGD's
//     default sprites/laserbeam.spr (Featureful precached the empty name); the beams always use the sprite precached
//     at the map load (a sprite name changed later by trigger_changevalue would be precached after the load, which
//     stops the server).
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"weapons.h"
#include	"effects.h"
#include	"decals.h"
#include	"saverestore.h"
#include	"locus.h"
#include	"round_hooks.h"

#define BEAM_FSINE		0x10
#define BEAM_FSOLID		0x20
#define BEAM_FSHADEIN	0x40
#define BEAM_FSHADEOUT	0x80

#define SF_LBEAM_SHADEIN	128
#define SF_LBEAM_SHADEOUT	256
#define SF_LBEAM_SOLID		512
#define SF_LBEAM_SINE		1024

int IsPointEntity( CBaseEntity *pEnt );	// effects.cpp (env_beam)

#define LBEAM_EDICT_MARGIN	64		// SBO: no beam within this many edicts of the limit
#define LBEAM_THINK			0.1		// SBO: sparks and damage, as env_laser

// SBO: the beam a locus_beam makes, with env_laser's sparks, damage and decals
class CLocusBeamBeam : public CBeam
{
public:
	void	EXPORT DamageThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	EHANDLE	m_hAttacker;
	int		m_iDamageType;
	float	m_flDieTime;	// 0: lives until killed
};

TYPEDESCRIPTION CLocusBeamBeam::m_SaveData[] =
{
	DEFINE_FIELD( CLocusBeamBeam, m_hAttacker, FIELD_EHANDLE ),
	DEFINE_FIELD( CLocusBeamBeam, m_iDamageType, FIELD_INTEGER ),
	DEFINE_FIELD( CLocusBeamBeam, m_flDieTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CLocusBeamBeam, CBeam );

void CLocusBeamBeam::DamageThink( void )
{
	if ( m_flDieTime && gpGlobals->time >= m_flDieTime )
	{
		UTIL_Remove( this );
		return;
	}
	pev->nextthink = gpGlobals->time + LBEAM_THINK;

	const Vector vecStart = GetStartPos();
	const Vector vecEnd = GetEndPos();
	RelinkBeam();

	// env_laser (CLaser::FireAtPoint): sparks at the ends
	DoSparks( vecStart, vecEnd );

	if ( pev->dmg <= 0 && !( pev->spawnflags & SF_BEAM_DECALS ) )
		return;

	TraceResult tr;
	UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	// CBeam::BeamDamage with the chosen damage type and the activator as attacker
	const float flDamage = pev->dmg * ( gpGlobals->time - pev->dmgtime );
	pev->dmgtime = gpGlobals->time;
	if ( tr.flFraction == 1.0 || tr.pHit == NULL )
		return;
	CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
	if ( !pHit )
		return;

	if ( pev->dmg > 0 && !( pHit->IsPlayer() && !Logic_IsPlayerInPlay( pHit ) ) )
	{
		CBaseEntity *pAttacker = m_hAttacker;
		entvars_t *pevAttacker = pAttacker ? pAttacker->pev : pev;
		if ( pAttacker && pAttacker->IsPlayer() && pHit->IsPlayer() && pAttacker != pHit )
			pevAttacker = pev;
		ClearMultiDamage();
		pHit->TraceAttack( pevAttacker, flDamage, ( tr.vecEndPos - vecStart ).Normalize(), &tr, m_iDamageType );
		ApplyMultiDamage( pev, pevAttacker );
	}
	if ( ( pev->spawnflags & SF_BEAM_DECALS ) && pHit->IsBSPModel() )
		UTIL_DecalTrace( &tr, DECAL_BIGSHOT1 + RANDOM_LONG( 0, 4 ) );
}

//=========================================================
// locus_beam
//=========================================================

class CLocusBeam : public CPointEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	string_t m_iszSprite;
	string_t m_iszSpritePrecached;	// SBO: the one precached in Spawn, which the beams use
	string_t m_iszTargetName;
	string_t m_iszStart;
	string_t m_iszEnd;
	int		m_iWidth;
	int		m_iDistortion;
	float	m_fFrame;
	int		m_iScrollRate;
	float	m_fDuration;
	float	m_fDamage;
	int		m_iDamageType;
	int		m_iFlags;
	int		m_iStartAttachment;
	int		m_iEndAttachment;
};

TYPEDESCRIPTION CLocusBeam::m_SaveData[] =
{
	DEFINE_FIELD( CLocusBeam, m_iszSprite, FIELD_STRING ),
	DEFINE_FIELD( CLocusBeam, m_iszSpritePrecached, FIELD_STRING ),
	DEFINE_FIELD( CLocusBeam, m_iszTargetName, FIELD_STRING ),
	DEFINE_FIELD( CLocusBeam, m_iszStart, FIELD_STRING ),
	DEFINE_FIELD( CLocusBeam, m_iszEnd, FIELD_STRING ),
	DEFINE_FIELD( CLocusBeam, m_iWidth, FIELD_INTEGER ),
	DEFINE_FIELD( CLocusBeam, m_iDistortion, FIELD_INTEGER ),
	DEFINE_FIELD( CLocusBeam, m_fFrame, FIELD_FLOAT ),
	DEFINE_FIELD( CLocusBeam, m_iScrollRate, FIELD_INTEGER ),
	DEFINE_FIELD( CLocusBeam, m_fDuration, FIELD_FLOAT ),
	DEFINE_FIELD( CLocusBeam, m_fDamage, FIELD_FLOAT ),
	DEFINE_FIELD( CLocusBeam, m_iDamageType, FIELD_INTEGER ),
	DEFINE_FIELD( CLocusBeam, m_iFlags, FIELD_INTEGER ),
	DEFINE_FIELD( CLocusBeam, m_iStartAttachment, FIELD_INTEGER ),
	DEFINE_FIELD( CLocusBeam, m_iEndAttachment, FIELD_INTEGER ),
};

LINK_ENTITY_TO_CLASS( locus_beam, CLocusBeam );
IMPLEMENT_SAVERESTORE( CLocusBeam, CPointEntity );

void CLocusBeam::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszSprite" ) )
	{
		m_iszSprite = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszTargetName" ) )
	{
		m_iszTargetName = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszStart" ) )
	{
		m_iszStart = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iszEnd" ) )
	{
		m_iszEnd = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iWidth" ) )
	{
		m_iWidth = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iDistortion" ) )
	{
		m_iDistortion = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_fFrame" ) )
	{
		m_fFrame = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iScrollRate" ) )
	{
		m_iScrollRate = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_fDuration" ) )
	{
		m_fDuration = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_fDamage" ) )
	{
		m_fDamage = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iDamageType" ) )
	{
		m_iDamageType = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iStartAttachment" ) )
	{
		m_iStartAttachment = atoi( pkvd->szValue );
		if ( m_iStartAttachment > 4 || m_iStartAttachment < 0 )
			m_iStartAttachment = 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iEndAttachment" ) )
	{
		m_iEndAttachment = atoi( pkvd->szValue );
		if ( m_iEndAttachment > 4 || m_iEndAttachment < 0 )
			m_iEndAttachment = 0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CLocusBeam::Precache( void )
{
	// SBO: an empty name takes the FGD's default (precaching "" stops the map)
	if ( FStringNull( m_iszSprite ) || !STRING( m_iszSprite )[0] )
		m_iszSprite = MAKE_STRING( "sprites/laserbeam.spr" );
	PRECACHE_MODEL( (char *)STRING( m_iszSprite ) );
	m_iszSpritePrecached = m_iszSprite;
}

void CLocusBeam::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pStartEnt;
	CBaseEntity *pEndEnt;
	Vector vecStartPos;
	Vector vecEndPos;

	// SBO: a beam is an edict; none close to the limit (the engine stops the server when it runs out)
	if ( NUMBER_OF_ENTITIES() >= gpGlobals->maxEntities - LBEAM_EDICT_MARGIN )
	{
		ALERT( at_console, "locus_beam \"%s\": too many entities, no beam made\n", STRING( pev->targetname ) );
		return;
	}

	CLocusBeamBeam *pBeam = NULL;
	BOOL fReversed = FALSE;		// SBO: the beam runs from the end to the start (its spark ends swap)
	switch ( pev->impulse )
	{
	case 0: // ents
		pStartEnt = Locus_FindEntity( NULL, STRING( m_iszStart ), pActivator );
		pEndEnt = Locus_FindEntity( NULL, STRING( m_iszEnd ), pActivator );

		if ( pStartEnt == NULL || pEndEnt == NULL )
			return;
		pBeam = GetClassPtr( (CLocusBeamBeam *)NULL );
		pBeam->BeamInit( STRING( m_iszSpritePrecached ), m_iWidth );
		// SBO: a point entity (info_target, path_corner, ...) is not sent to the clients, so a beam that ends on one
		// is not drawn: its origin is used as a position instead, as env_beam does (IsPointEntity, effects.cpp)
		if ( IsPointEntity( pStartEnt ) && IsPointEntity( pEndEnt ) )
			pBeam->PointsInit( pStartEnt->pev->origin, pEndEnt->pev->origin );
		else if ( IsPointEntity( pStartEnt ) )
		{
			pBeam->PointEntInit( pStartEnt->pev->origin, pEndEnt->entindex() );
			pBeam->SetEndAttachment( m_iEndAttachment );
		}
		else if ( IsPointEntity( pEndEnt ) )
		{
			pBeam->PointEntInit( pEndEnt->pev->origin, pStartEnt->entindex() );
			pBeam->SetEndAttachment( m_iStartAttachment );
			fReversed = TRUE;
		}
		else
		{
			pBeam->EntsInit( pStartEnt->entindex(), pEndEnt->entindex() );
			pBeam->SetStartAttachment( m_iStartAttachment );
			pBeam->SetEndAttachment( m_iEndAttachment );
		}
		break;

	case 1: // pointent
		if ( !TryCalcLocus_Position( this, pActivator, STRING( m_iszStart ), vecStartPos ) )
			return;

		pEndEnt = Locus_FindEntity( NULL, STRING( m_iszEnd ), pActivator );

		if ( pEndEnt == NULL )
			return;
		pBeam = GetClassPtr( (CLocusBeamBeam *)NULL );
		pBeam->BeamInit( STRING( m_iszSpritePrecached ), m_iWidth );
		if ( IsPointEntity( pEndEnt ) )	// SBO: as above
			pBeam->PointsInit( vecStartPos, pEndEnt->pev->origin );
		else
		{
			pBeam->PointEntInit( vecStartPos, pEndEnt->entindex() );
			pBeam->SetEndAttachment( m_iEndAttachment );
		}
		break;

	case 2: // points
		if ( !TryCalcLocus_Position( this, pActivator, STRING( m_iszStart ), vecStartPos ) ||
			 !TryCalcLocus_Position( this, pActivator, STRING( m_iszEnd ), vecEndPos ) )
			return;

		pBeam = GetClassPtr( (CLocusBeamBeam *)NULL );
		pBeam->BeamInit( STRING( m_iszSpritePrecached ), m_iWidth );
		pBeam->PointsInit( vecStartPos, vecEndPos );
		break;

	case 3: // point & offset
		if ( !TryCalcLocus_Position( this, pActivator, STRING( m_iszStart ), vecStartPos ) ||
			 !TryCalcLocus_Velocity( this, pActivator, STRING( m_iszEnd ), vecEndPos ) )
			return;

		pBeam = GetClassPtr( (CLocusBeamBeam *)NULL );
		pBeam->BeamInit( STRING( m_iszSpritePrecached ), m_iWidth );
		pBeam->PointsInit( vecStartPos, vecStartPos + vecEndPos );
		break;

	default:
		ALERT( at_error, "%s: unknown 'Start and End' type. Refusing to spawn a beam\n", STRING( pev->classname ) );
		return;
	}

	pBeam->pev->classname = MAKE_STRING( "beam" );	// as CBeam::BeamCreate
	pBeam->SetColor( pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z );
	pBeam->SetBrightness( pev->renderamt );
	pBeam->SetNoise( m_iDistortion );
	pBeam->SetFrame( m_fFrame );
	pBeam->SetScrollRate( m_iScrollRate );
	pBeam->SetFlags( m_iFlags );
	pBeam->pev->dmg = m_fDamage;
	pBeam->pev->frags = m_iDamageType;
	pBeam->pev->spawnflags |= pev->spawnflags & SF_BEAM_DECALS;
	if ( pev->spawnflags & SF_BEAM_SPARKSTART )
		pBeam->pev->spawnflags |= fReversed ? SF_BEAM_SPARKEND : SF_BEAM_SPARKSTART;
	if ( pev->spawnflags & SF_BEAM_SPARKEND )
		pBeam->pev->spawnflags |= fReversed ? SF_BEAM_SPARKSTART : SF_BEAM_SPARKEND;
	pBeam->m_iDamageType = m_iDamageType;
	pBeam->m_hAttacker = pActivator;
	pBeam->pev->targetname = m_iszTargetName;

	// SBO: env_laser's sparks, damage and decals (see the top); otherwise Featureful's plain beam
	const BOOL fThink = m_fDamage > 0 || ( pev->spawnflags & ( SF_BEAM_SPARKSTART | SF_BEAM_SPARKEND | SF_BEAM_DECALS ) );
	if ( fThink )
	{
		pBeam->m_flDieTime = m_fDuration ? gpGlobals->time + m_fDuration : 0;
		pBeam->pev->dmgtime = gpGlobals->time;
		pBeam->SetThink( &CLocusBeamBeam::DamageThink );
		pBeam->pev->nextthink = gpGlobals->time + LBEAM_THINK;
	}
	else if ( m_fDuration )
	{
		pBeam->SetThink( &CBeam::SUB_Remove );
		pBeam->pev->nextthink = gpGlobals->time + m_fDuration;
	}

	if ( pev->target )
		FireTargets( STRING( pev->target ), pBeam, this, USE_TOGGLE, 0 );
}

void CLocusBeam::Spawn( void )
{
	Precache();
	m_iFlags = 0;
	if ( pev->spawnflags & SF_LBEAM_SHADEIN )
		m_iFlags |= BEAM_FSHADEIN;
	if ( pev->spawnflags & SF_LBEAM_SHADEOUT )
		m_iFlags |= BEAM_FSHADEOUT;
	if ( pev->spawnflags & SF_LBEAM_SINE )
		m_iFlags |= BEAM_FSINE;
	if ( pev->spawnflags & SF_LBEAM_SOLID )
		m_iFlags |= BEAM_FSOLID;
}
