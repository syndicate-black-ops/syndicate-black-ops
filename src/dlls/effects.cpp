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
#include "monsters.h"
#include "customentity.h"
#include "effects.h"
#include "weapons.h"
#include "decals.h"
#include "func_break.h"
#include "shake.h"
#include "player.h"	// SoHL env_sky: CEnvSky::SendState
#include "animation.h"	// SBO 2.0: env_sprite with a studio model
#include "locus.h"	// SBO 2.0: Locus keys of gibshooter / env_shooter

#define	SF_GIBSHOOTER_REPEATABLE	1 // allows a gibshooter to be refired
#define	SF_GIBSHOOTER_INSTANT_START	8 // SBO 2.0 (Featureful): the first shot in the trigger's own frame
#define	SF_ENVSHOOTER_DONT_WAIT_TILL_LAND	4	// SBO 2.0 (Featureful): a gib fades at its life time even while it flies

#define SF_FUNNEL_REVERSE			1 // funnel effect repels particles instead of attracting them.


// Lightning target, just alias landmark
LINK_ENTITY_TO_CLASS( info_target, CPointEntity );


class CBubbling : public CBaseEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	
	void	EXPORT FizzThink( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	virtual int		ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	static	TYPEDESCRIPTION m_SaveData[];

	int		m_density;
	int		m_frequency;
	int		m_bubbleModel;
	int		m_state;
	string_t m_iszBubbleModel;	// SBO 2.0, from Featureful: "bubble_model" custom bubble sprite (empty: sprites/bubble.spr)
};

LINK_ENTITY_TO_CLASS( env_bubbles, CBubbling );

TYPEDESCRIPTION	CBubbling::m_SaveData[] = 
{
	DEFINE_FIELD( CBubbling, m_density, FIELD_INTEGER ),
	DEFINE_FIELD( CBubbling, m_frequency, FIELD_INTEGER ),
	DEFINE_FIELD( CBubbling, m_state, FIELD_INTEGER ),
	// Let spawn restore this!
	//	DEFINE_FIELD( CBubbling, m_bubbleModel, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBubbling, CBaseEntity );


#define SF_BUBBLES_STARTOFF		0x0001

void CBubbling::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT(pev), STRING(pev->model) );		// Set size

	pev->solid = SOLID_NOT;							// Remove model & collisions
	pev->renderamt = 0;								// The engine won't draw this model if this is set to 0 and blending is on
	pev->rendermode = kRenderTransTexture;
	int speed = pev->speed > 0 ? pev->speed : -pev->speed;

	// HACKHACK!!! - Speed in rendercolor
	pev->rendercolor.x = speed >> 8;
	pev->rendercolor.y = speed & 255;
	pev->rendercolor.z = (pev->speed < 0) ? 1 : 0;


	if ( !(pev->spawnflags & SF_BUBBLES_STARTOFF) )
	{
		SetThink( &CBubbling::FizzThink );
		pev->nextthink = gpGlobals->time + 2.0;
		m_state = 1;
	}
	else 
		m_state = 0;
}

void CBubbling::Precache( void )
{
	m_bubbleModel = PRECACHE_MODEL( FStringNull( m_iszBubbleModel ) ? (char *)"sprites/bubble.spr" : (char *)STRING( m_iszBubbleModel ) );	// Precache bubble sprite
}


void CBubbling::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ShouldToggle( useType, m_state ) )
		m_state = !m_state;

	if ( m_state )
	{
		SetThink( &CBubbling::FizzThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		SetThink( NULL );
		pev->nextthink = 0;
	}
}


void CBubbling::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "density"))
	{
		m_density = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "frequency"))
	{
		m_frequency = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "bubble_model"))	// SBO 2.0 (Featureful)
	{
		m_iszBubbleModel = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "current"))
	{
		pev->speed = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}


void CBubbling::FizzThink( void )
{
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, VecBModelOrigin(pev) );
		WRITE_BYTE( TE_FIZZ );
		WRITE_SHORT( (short)ENTINDEX( edict() ) );
		WRITE_SHORT( (short)m_bubbleModel );
		WRITE_BYTE( m_density );
	MESSAGE_END();

	if ( m_frequency > 19 )
		pev->nextthink = gpGlobals->time + 0.5;
	else
		pev->nextthink = gpGlobals->time + 2.5 - (0.1 * m_frequency);
}

// --------------------------------------------------
// 
// Beams
//
// --------------------------------------------------

LINK_ENTITY_TO_CLASS( beam, CBeam );

void CBeam::Spawn( void )
{
	pev->solid = SOLID_NOT;							// Remove model & collisions
	Precache( );
}

void CBeam::Precache( void )
{
	if ( pev->owner )
		SetStartEntity( ENTINDEX( pev->owner ) );
	if ( pev->aiment )
		SetEndEntity( ENTINDEX( pev->aiment ) );
}

void CBeam::SetStartEntity( int entityIndex ) 
{ 
	pev->sequence = (entityIndex & 0x0FFF) | (pev->sequence&0xF000);	// halflife-updated #74: keep the attachment bits
	pev->owner = g_engfuncs.pfnPEntityOfEntIndex( entityIndex );
}

void CBeam::SetEndEntity( int entityIndex ) 
{ 
	pev->skin = (entityIndex & 0x0FFF) | (pev->skin&0xF000);	// halflife-updated #74: keep the attachment bits
	pev->aiment = g_engfuncs.pfnPEntityOfEntIndex( entityIndex );
}


// These don't take attachments into account
const Vector &CBeam::GetStartPos( void )
{
	if ( GetType() == BEAM_ENTS )
	{
		edict_t *pent =  g_engfuncs.pfnPEntityOfEntIndex( GetStartEntity() );
		return pent->v.origin;
	}
	return pev->origin;
}


const Vector &CBeam::GetEndPos( void )
{
	int type = GetType();
	if ( type == BEAM_POINTS || type == BEAM_HOSE )
	{
		return pev->angles;
	}

	edict_t *pent =  g_engfuncs.pfnPEntityOfEntIndex( GetEndEntity() );
	if ( pent )
		return pent->v.origin;
	return pev->angles;
}


CBeam *CBeam::BeamCreate( const char *pSpriteName, int width )
{
	// Create a new entity with CBeam private data
	CBeam *pBeam = GetClassPtr( (CBeam *)NULL );
	pBeam->pev->classname = MAKE_STRING("beam");

	pBeam->BeamInit( pSpriteName, width );

	return pBeam;
}


void CBeam::BeamInit( const char *pSpriteName, int width )
{
	pev->flags |= FL_CUSTOMENTITY;
	SetColor( 255, 255, 255 );
	SetBrightness( 255 );
	SetNoise( 0 );
	SetFrame( 0 );
	SetScrollRate( 0 );
	pev->model = MAKE_STRING( pSpriteName );
	SetTexture( PRECACHE_MODEL( (char *)pSpriteName ) );
	SetWidth( width );
	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
}


void CBeam::PointsInit( const Vector &start, const Vector &end )
{
	SetType( BEAM_POINTS );
	SetStartPos( start );
	SetEndPos( end );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}


void CBeam::HoseInit( const Vector &start, const Vector &direction )
{
	SetType( BEAM_HOSE );
	SetStartPos( start );
	SetEndPos( direction );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}


void CBeam::PointEntInit( const Vector &start, int endIndex )
{
	SetType( BEAM_ENTPOINT );
	SetStartPos( start );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::EntsInit( int startIndex, int endIndex )
{
	SetType( BEAM_ENTS );
	SetStartEntity( startIndex );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}


void CBeam::RelinkBeam( void )
{
	const Vector &startPos = GetStartPos(), &endPos = GetEndPos();

	pev->mins.x = min( startPos.x, endPos.x );
	pev->mins.y = min( startPos.y, endPos.y );
	pev->mins.z = min( startPos.z, endPos.z );
	pev->maxs.x = max( startPos.x, endPos.x );
	pev->maxs.y = max( startPos.y, endPos.y );
	pev->maxs.z = max( startPos.z, endPos.z );
	pev->mins = pev->mins - pev->origin;
	pev->maxs = pev->maxs - pev->origin;

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );
}

#if 0
void CBeam::SetObjectCollisionBox( void )
{
	const Vector &startPos = GetStartPos(), &endPos = GetEndPos();

	pev->absmin.x = min( startPos.x, endPos.x );
	pev->absmin.y = min( startPos.y, endPos.y );
	pev->absmin.z = min( startPos.z, endPos.z );
	pev->absmax.x = max( startPos.x, endPos.x );
	pev->absmax.y = max( startPos.y, endPos.y );
	pev->absmax.z = max( startPos.z, endPos.z );
}
#endif


void CBeam::TriggerTouch( CBaseEntity *pOther )
{
	if ( pOther->pev->flags & (FL_CLIENT | FL_MONSTER) )
	{
		if ( pev->owner )
		{
			CBaseEntity *pOwner = CBaseEntity::Instance(pev->owner);
			pOwner->Use( pOther, this, USE_TOGGLE, 0 );
		}
		ALERT( at_console, "Firing targets!!!\n" );
	}
}


CBaseEntity *CBeam::RandomTargetname( const char *szName )
{
	int total = 0;

	CBaseEntity *pEntity = NULL;
	CBaseEntity *pNewEntity = NULL;
	while ((pNewEntity = UTIL_FindEntityByTargetname( pNewEntity, szName )) != NULL)
	{
		total++;
		if (RANDOM_LONG(0,total-1) < 1)
			pEntity = pNewEntity;
	}
	return pEntity;
}


void CBeam::DoSparks( const Vector &start, const Vector &end )
{
	if ( pev->spawnflags & (SF_BEAM_SPARKSTART|SF_BEAM_SPARKEND) )
	{
		if ( pev->spawnflags & SF_BEAM_SPARKSTART )
		{
			UTIL_Sparks( start );
		}
		if ( pev->spawnflags & SF_BEAM_SPARKEND )
		{
			UTIL_Sparks( end );
		}
	}
}


class CLightning : public CBeam
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Activate( void );

	void	EXPORT StrikeThink( void );
	void	EXPORT DamageThink( void );
	void	RandomArea( void );
	void	RandomPoint( Vector &vecSrc );
	void	Zap( const Vector &vecSrc, const Vector &vecDest );
	void	EXPORT StrikeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	EXPORT ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	inline BOOL ServerSide( void )
	{
		if ( m_life == 0 && !(pev->spawnflags & SF_BEAM_RING) )
			return TRUE;
		return FALSE;
	}

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void	BeamUpdateVars( void );

	int		m_active;
	int		m_iszStartEntity;
	int		m_iszEndEntity;
	float	m_life;
	int		m_boltWidth;
	int		m_noiseAmplitude;
	int		m_brightness;
	int		m_speed;
	float	m_restrike;
	int		m_spriteTexture;
	int		m_iszSpriteName;
	int		m_frameStart;

	float	m_radius;
};

LINK_ENTITY_TO_CLASS( env_lightning, CLightning );
LINK_ENTITY_TO_CLASS( env_beam, CLightning );

// UNDONE: Jay -- This is only a test
#if _DEBUG
class CTripBeam : public CLightning
{
	void Spawn( void );
};
LINK_ENTITY_TO_CLASS( trip_beam, CTripBeam );

void CTripBeam::Spawn( void )
{
	CLightning::Spawn();
	SetTouch( &CTripBeam::TriggerTouch );
	pev->solid = SOLID_TRIGGER;
	RelinkBeam();
}
#endif



TYPEDESCRIPTION	CLightning::m_SaveData[] = 
{
	DEFINE_FIELD( CLightning, m_active, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_iszStartEntity, FIELD_STRING ),
	DEFINE_FIELD( CLightning, m_iszEndEntity, FIELD_STRING ),
	DEFINE_FIELD( CLightning, m_life, FIELD_FLOAT ),
	DEFINE_FIELD( CLightning, m_boltWidth, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_noiseAmplitude, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_brightness, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_speed, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_restrike, FIELD_FLOAT ),
	DEFINE_FIELD( CLightning, m_spriteTexture, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_iszSpriteName, FIELD_STRING ),
	DEFINE_FIELD( CLightning, m_frameStart, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_radius, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CLightning, CBeam );


void CLightning::Spawn( void )
{
	if ( FStringNull( m_iszSpriteName ) )
	{
		SetThink( &CLightning::SUB_Remove );
		return;
	}
	pev->solid = SOLID_NOT;							// Remove model & collisions
	Precache( );

	pev->dmgtime = gpGlobals->time;

	if ( ServerSide() )
	{
		SetThink( NULL );
		if ( pev->dmg > 0 )
		{
			SetThink( &CLightning::DamageThink );
			pev->nextthink = gpGlobals->time + 0.1;
		}
		if ( pev->targetname )
		{
			if ( !(pev->spawnflags & SF_BEAM_STARTON) )
			{
				pev->effects = EF_NODRAW;
				m_active = 0;
				pev->nextthink = 0;
			}
			else
				m_active = 1;
		
			SetUse( &CLightning::ToggleUse );
		}
	}
	else
	{
		m_active = 0;
		if ( !FStringNull(pev->targetname) )
		{
			SetUse( &CLightning::StrikeUse );
		}
		if ( FStringNull(pev->targetname) || FBitSet(pev->spawnflags, SF_BEAM_STARTON) )
		{
			SetThink( &CLightning::StrikeThink );
			pev->nextthink = gpGlobals->time + 1.0;
		}
	}
}

void CLightning::Precache( void )
{
	m_spriteTexture = PRECACHE_MODEL( (char *)STRING(m_iszSpriteName) );
	CBeam::Precache();
}


void CLightning::Activate( void )
{
	if ( ServerSide() )
		BeamUpdateVars();
}


void CLightning::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "LightningStart"))
	{
		m_iszStartEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "LightningEnd"))
	{
		m_iszEndEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "life"))
	{
		m_life = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "BoltWidth"))
	{
		m_boltWidth = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		m_noiseAmplitude = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		m_speed = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "StrikeTime"))
	{
		m_restrike = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		m_frameStart = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "Radius"))
	{
		m_radius = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBeam::KeyValue( pkvd );
}


void CLightning::ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_active ) )
		return;
	if ( m_active )
	{
		m_active = 0;
		pev->effects |= EF_NODRAW;
		pev->nextthink = 0;
	}
	else
	{
		m_active = 1;
		pev->effects &= ~EF_NODRAW;
		DoSparks( GetStartPos(), GetEndPos() );
		if ( pev->dmg > 0 )
		{
			pev->nextthink = gpGlobals->time;
			pev->dmgtime = gpGlobals->time;
		}
	}
}


void CLightning::StrikeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_active ) )
		return;

	if ( m_active )
	{
		m_active = 0;
		SetThink( NULL );
	}
	else
	{
		SetThink( &CLightning::StrikeThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}

	if ( !FBitSet( pev->spawnflags, SF_BEAM_TOGGLE ) )
		SetUse( NULL );
}


int IsPointEntity( CBaseEntity *pEnt )
{
	if ( !pEnt->pev->modelindex )
		return 1;
	if ( FClassnameIs( pEnt->pev, "info_target" ) || FClassnameIs( pEnt->pev, "info_landmark" ) || FClassnameIs( pEnt->pev, "path_corner" ) )
		return 1;

	return 0;
}


void CLightning::StrikeThink( void )
{
	if ( m_life != 0 )
	{
		if ( pev->spawnflags & SF_BEAM_RANDOM )
			pev->nextthink = gpGlobals->time + m_life + RANDOM_FLOAT( 0, m_restrike );
		else
			pev->nextthink = gpGlobals->time + m_life + m_restrike;
	}
	m_active = 1;

	if (FStringNull(m_iszEndEntity))
	{
		if (FStringNull(m_iszStartEntity))
		{
			RandomArea( );
		}
		else
		{
			CBaseEntity *pStart = RandomTargetname( STRING(m_iszStartEntity) );
			if (pStart != NULL)
				RandomPoint( pStart->pev->origin );
			else
				ALERT( at_console, "env_beam: unknown entity \"%s\"\n", STRING(m_iszStartEntity) );
		}
		return;
	}

	CBaseEntity *pStart = RandomTargetname( STRING(m_iszStartEntity) );
	CBaseEntity *pEnd = RandomTargetname( STRING(m_iszEndEntity) );

	if ( pStart != NULL && pEnd != NULL )
	{
		if ( IsPointEntity( pStart ) || IsPointEntity( pEnd ) )
		{
			if ( pev->spawnflags & SF_BEAM_RING)
			{
				// don't work
				return;
			}
		}

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			if ( IsPointEntity( pStart ) || IsPointEntity( pEnd ) )
			{
				if ( !IsPointEntity( pEnd ) )	// One point entity must be in pEnd
				{
					CBaseEntity *pTemp;
					pTemp = pStart;
					pStart = pEnd;
					pEnd = pTemp;
				}
				if ( !IsPointEntity( pStart ) )	// One sided
				{
					WRITE_BYTE( TE_BEAMENTPOINT );
					WRITE_SHORT( pStart->entindex() );
					WRITE_COORD( pEnd->pev->origin.x);
					WRITE_COORD( pEnd->pev->origin.y);
					WRITE_COORD( pEnd->pev->origin.z);
				}
				else
				{
					WRITE_BYTE( TE_BEAMPOINTS);
					WRITE_COORD( pStart->pev->origin.x);
					WRITE_COORD( pStart->pev->origin.y);
					WRITE_COORD( pStart->pev->origin.z);
					WRITE_COORD( pEnd->pev->origin.x);
					WRITE_COORD( pEnd->pev->origin.y);
					WRITE_COORD( pEnd->pev->origin.z);
				}


			}
			else
			{
				if ( pev->spawnflags & SF_BEAM_RING)
					WRITE_BYTE( TE_BEAMRING );
				else
					WRITE_BYTE( TE_BEAMENTS );
				WRITE_SHORT( pStart->entindex() );
				WRITE_SHORT( pEnd->entindex() );
			}

			WRITE_SHORT( m_spriteTexture );
			WRITE_BYTE( m_frameStart ); // framestart
			WRITE_BYTE( (int)pev->framerate); // framerate
			WRITE_BYTE( (int)(m_life*10.0) ); // life
			WRITE_BYTE( m_boltWidth );  // width
			WRITE_BYTE( m_noiseAmplitude );   // noise
			WRITE_BYTE( (int)pev->rendercolor.x );   // r, g, b
			WRITE_BYTE( (int)pev->rendercolor.y );   // r, g, b
			WRITE_BYTE( (int)pev->rendercolor.z );   // r, g, b
			WRITE_BYTE( pev->renderamt );	// brightness
			WRITE_BYTE( m_speed );		// speed
		MESSAGE_END();
		DoSparks( pStart->pev->origin, pEnd->pev->origin );
		if ( pev->dmg > 0 )
		{
			TraceResult tr;
			UTIL_TraceLine( pStart->pev->origin, pEnd->pev->origin, dont_ignore_monsters, NULL, &tr );
			BeamDamageInstant( &tr, pev->dmg );
		}
	}
}


void CBeam::BeamDamage( TraceResult *ptr, entvars_t *pevAttacker, entvars_t *pevInflictor )
{
	if ( !pevAttacker )
		pevAttacker = pev;
	if ( !pevInflictor )
		pevInflictor = pev;

	RelinkBeam();
	if ( ptr->flFraction != 1.0 && ptr->pHit != NULL )
	{
		CBaseEntity *pHit = CBaseEntity::Instance(ptr->pHit);
		if ( pHit )
		{
			const Vector vecPunch = pHit->pev->punchangle;
			ClearMultiDamage();
			pHit->TraceAttack( pevAttacker, pev->dmg * (gpGlobals->time - pev->dmgtime), (ptr->vecEndPos - pev->origin).Normalize(), ptr, DMG_ENERGYBEAM );
			ApplyMultiDamage( pevInflictor, pevAttacker );
			// SBO 2.0, from Featureful: "No Camera Punch" (env_beam, env_laser): a hit player's view is not kicked
			if ( ( pev->spawnflags & SF_BEAM_NO_PUNCH ) && pHit->IsPlayer() )
				pHit->pev->punchangle = vecPunch;
			if ( pev->spawnflags & SF_BEAM_DECALS )
			{
				if ( pHit->IsBSPModel() )
					UTIL_DecalTrace( ptr, DECAL_BIGSHOT1 + RANDOM_LONG(0,4) );
			}
		}
	}
	pev->dmgtime = gpGlobals->time;
}


void CLightning::DamageThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
	TraceResult tr;
	UTIL_TraceLine( GetStartPos(), GetEndPos(), dont_ignore_monsters, NULL, &tr );
	BeamDamage( &tr );
}



void CLightning::Zap( const Vector &vecSrc, const Vector &vecDest )
{
#if 1
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_COORD(vecDest.x);
		WRITE_COORD(vecDest.y);
		WRITE_COORD(vecDest.z);
		WRITE_SHORT( m_spriteTexture );
		WRITE_BYTE( m_frameStart ); // framestart
		WRITE_BYTE( (int)pev->framerate); // framerate
		WRITE_BYTE( (int)(m_life*10.0) ); // life
		WRITE_BYTE( m_boltWidth );  // width
		WRITE_BYTE( m_noiseAmplitude );   // noise
		WRITE_BYTE( (int)pev->rendercolor.x );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.y );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.z );   // r, g, b
		WRITE_BYTE( pev->renderamt );	// brightness
		WRITE_BYTE( m_speed );		// speed
	MESSAGE_END();
#else
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE(TE_LIGHTNING);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_COORD(vecDest.x);
		WRITE_COORD(vecDest.y);
		WRITE_COORD(vecDest.z);
		WRITE_BYTE(10);
		WRITE_BYTE(50);
		WRITE_BYTE(40);
		WRITE_SHORT(m_spriteTexture);
	MESSAGE_END();
#endif
	DoSparks( vecSrc, vecDest );
}

void CLightning::RandomArea( void )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecSrc = pev->origin;

		Vector vecDir1 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir1 = vecDir1.Normalize();
		TraceResult		tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1 );

		if (tr1.flFraction == 1.0)
			continue;

		Vector vecDir2;
		do {
			vecDir2 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		} while (DotProduct(vecDir1, vecDir2 ) > 0);
		vecDir2 = vecDir2.Normalize();
		TraceResult		tr2;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir2 * m_radius, ignore_monsters, ENT(pev), &tr2 );

		if (tr2.flFraction == 1.0)
			continue;

		if ((tr1.vecEndPos - tr2.vecEndPos).Length() < m_radius * 0.1)
			continue;

		UTIL_TraceLine( tr1.vecEndPos, tr2.vecEndPos, ignore_monsters, ENT(pev), &tr2 );

		if (tr2.flFraction != 1.0)
			continue;

		Zap( tr1.vecEndPos, tr2.vecEndPos );

		break;
	}
}


void CLightning::RandomPoint( Vector &vecSrc )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecDir1 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir1 = vecDir1.Normalize();
		TraceResult		tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1 );

		if ((tr1.vecEndPos - vecSrc).Length() < m_radius * 0.1)
			continue;

		if (tr1.flFraction == 1.0)
			continue;

		Zap( vecSrc, tr1.vecEndPos );
		break;
	}
}



void CLightning::BeamUpdateVars( void )
{
	int beamType;
	int pointStart, pointEnd;

	edict_t *pStart = FIND_ENTITY_BY_TARGETNAME ( NULL, STRING(m_iszStartEntity) );
	edict_t *pEnd = FIND_ENTITY_BY_TARGETNAME ( NULL, STRING(m_iszEndEntity) );
	pointStart = IsPointEntity( CBaseEntity::Instance(pStart) );
	pointEnd = IsPointEntity( CBaseEntity::Instance(pEnd) );

	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
	pev->flags |= FL_CUSTOMENTITY;
	pev->model = m_iszSpriteName;
	SetTexture( m_spriteTexture );

	beamType = BEAM_ENTS;
	if ( pointStart || pointEnd )
	{
		if ( !pointStart )	// One point entity must be in pStart
		{
			edict_t *pTemp;
			// Swap start & end
			pTemp = pStart;
			pStart = pEnd;
			pEnd = pTemp;
			int swap = pointStart;
			pointStart = pointEnd;
			pointEnd = swap;
		}
		if ( !pointEnd )
			beamType = BEAM_ENTPOINT;
		else
			beamType = BEAM_POINTS;
	}

	SetType( beamType );
	if ( beamType == BEAM_POINTS || beamType == BEAM_ENTPOINT || beamType == BEAM_HOSE )
	{
		SetStartPos( pStart->v.origin );
		if ( beamType == BEAM_POINTS || beamType == BEAM_HOSE )
			SetEndPos( pEnd->v.origin );
		else
			SetEndEntity( ENTINDEX(pEnd) );
	}
	else
	{
		SetStartEntity( ENTINDEX(pStart) );
		SetEndEntity( ENTINDEX(pEnd) );
	}

	RelinkBeam();

	SetWidth( m_boltWidth );
	SetNoise( m_noiseAmplitude );
	SetFrame( m_frameStart );
	SetScrollRate( m_speed );
	int flags = 0;
	if ( pev->spawnflags & SF_BEAM_SHADEIN )
		flags = BEAM_FSHADEIN;
	else if ( pev->spawnflags & SF_BEAM_SHADEOUT )
		flags = BEAM_FSHADEOUT;
	// SBO 2.0, from Featureful (CLightning::BeamUpdateFlags, Half-Life 1 SDK license): 512 "Draw Solid", 1024 "Draw
	// Sine"; no map sets them
	if ( pev->spawnflags & SF_BEAM_SOLID )
		flags |= BEAM_FSOLID;
	if ( pev->spawnflags & SF_BEAM_SINE )
		flags |= BEAM_FSINE;
	SetFlags( flags );
}


LINK_ENTITY_TO_CLASS( env_laser, CLaser );

TYPEDESCRIPTION	CLaser::m_SaveData[] = 
{
	DEFINE_FIELD( CLaser, m_pSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( CLaser, m_iszSpriteName, FIELD_STRING ),
	DEFINE_FIELD( CLaser, m_firePosition, FIELD_POSITION_VECTOR ),
};

IMPLEMENT_SAVERESTORE( CLaser, CBeam );

void CLaser::Spawn( void )
{
	if ( FStringNull( pev->model ) )
	{
		SetThink( &CLaser::SUB_Remove );
		return;
	}
	pev->solid = SOLID_NOT;							// Remove model & collisions
	Precache( );

	SetThink( &CLaser::StrikeThink );
	pev->flags |= FL_CUSTOMENTITY;

	PointsInit( pev->origin, pev->origin );

	if ( !m_pSprite && m_iszSpriteName )
		m_pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteName), pev->origin, TRUE );
	else
		m_pSprite = NULL;

	if ( m_pSprite )
		m_pSprite->SetTransparency( kRenderGlow, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->renderfx );

	// SBO 2.0, from Featureful: the start sprite, made and shown like the end sprite
	if ( !m_pStartSprite && m_iszStartSpriteName )
	{
		m_pStartSprite = CSprite::SpriteCreate( STRING( m_iszStartSpriteName ), pev->origin, TRUE );
		if ( m_pStartSprite )
			m_pStartSprite->SetTransparency( kRenderGlow, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->renderfx );
	}

	if ( pev->targetname && !(pev->spawnflags & SF_BEAM_STARTON) )
		TurnOff();
	else
		TurnOn();
}

void CLaser::Precache( void )
{
	pev->modelindex = PRECACHE_MODEL( (char *)STRING(pev->model) );
	if ( m_iszSpriteName )
		PRECACHE_MODEL( (char *)STRING(m_iszSpriteName) );
	if ( m_iszStartSpriteName )
		PRECACHE_MODEL( (char *)STRING( m_iszStartSpriteName ) );
}


void CLaser::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "LaserTarget"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "width"))
	{
		SetWidth( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		SetNoise( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		SetScrollRate( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "EndSprite"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "StartSprite"))	// SBO 2.0 (Featureful)
	{
		m_iszStartSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		pev->frame = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBeam::KeyValue( pkvd );
}


// SBO: remove the end sprite with the laser (killtarget), or it stays behind
// (Valve issue #1747).
void CLaser::UpdateOnRemove( void )
{
	if ( m_pSprite )
	{
		UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}
	if ( m_pStartSprite )
	{
		UTIL_Remove( m_pStartSprite );
		m_pStartSprite = NULL;
	}
	CBeam::UpdateOnRemove();
}


int CLaser::IsOn( void )
{
	if (pev->effects & EF_NODRAW)
		return 0;
	return 1;
}


void CLaser::TurnOff( void )
{
	pev->effects |= EF_NODRAW;
	pev->nextthink = 0;
	if ( m_pSprite )
		m_pSprite->TurnOff();
	if ( m_pStartSprite )
		m_pStartSprite->TurnOff();
}


void CLaser::TurnOn( void )
{
	pev->effects &= ~EF_NODRAW;
	if ( m_pSprite )
		m_pSprite->TurnOn();
	if ( m_pStartSprite )
		m_pStartSprite->TurnOn();
	pev->dmgtime = gpGlobals->time;
	pev->nextthink = gpGlobals->time;
}


void CLaser::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int active = IsOn();

	if ( !ShouldToggle( useType, active ) )
		return;
	if ( active )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}


void CLaser::FireAtPoint( TraceResult &tr, entvars_t *pevAttacker, entvars_t *pevInflictor )
{
	SetEndPos( tr.vecEndPos );
	if ( m_pSprite )
		UTIL_SetOrigin( m_pSprite->pev, tr.vecEndPos );
	if ( m_pStartSprite )
		UTIL_SetOrigin( m_pStartSprite->pev, pev->origin );

	BeamDamage( &tr, pevAttacker, pevInflictor );
	DoSparks( GetStartPos(), tr.vecEndPos );
}

void CLaser::StrikeThink( void )
{
	CBaseEntity *pEnd = RandomTargetname( STRING(pev->message) );

	if ( pEnd )
		m_firePosition = pEnd->pev->origin;

	TraceResult tr;

	UTIL_TraceLine( pev->origin, m_firePosition, dont_ignore_monsters, NULL, &tr );
	FireAtPoint( tr );
	pev->nextthink = gpGlobals->time + 0.1;
}



class CGlow : public CPointEntity
{
public:
	void Spawn( void );
	void Think( void );
	void Animate( float frames );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float		m_lastTime;
	float		m_maxFrame;
};

LINK_ENTITY_TO_CLASS( env_glow, CGlow );

TYPEDESCRIPTION	CGlow::m_SaveData[] = 
{
	DEFINE_FIELD( CGlow, m_lastTime, FIELD_TIME ),
	DEFINE_FIELD( CGlow, m_maxFrame, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CGlow, CPointEntity );

void CGlow::Spawn( void )
{
	// SBO 2.0: no model, the editor placeholder or a file the server doesn't have ends the map at the precache below
	if ( UTIL_MapperModelUnusable( pev, STRING( pev->model ) ) )
	{
		REMOVE_ENTITY( ENT( pev ) );
		return;
	}

	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;

	PRECACHE_MODEL( (char *)STRING(pev->model) );
	SET_MODEL( ENT(pev), STRING(pev->model) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
	if ( m_maxFrame > 1.0 && pev->framerate != 0 )
		pev->nextthink	= gpGlobals->time + 0.1;

	m_lastTime = gpGlobals->time;
}


void CGlow::Think( void )
{
	Animate( pev->framerate * (gpGlobals->time - m_lastTime) );

	pev->nextthink		= gpGlobals->time + 0.1;
	m_lastTime			= gpGlobals->time;
}


void CGlow::Animate( float frames )
{ 
	if ( m_maxFrame > 0 )
		pev->frame = fmod( pev->frame + frames, m_maxFrame );
}


LINK_ENTITY_TO_CLASS( env_sprite, CSprite );

TYPEDESCRIPTION	CSprite::m_SaveData[] = 
{
	DEFINE_FIELD( CSprite, m_lastTime, FIELD_TIME ),
	DEFINE_FIELD( CSprite, m_maxFrame, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CSprite, CPointEntity );

void CSprite::Spawn( void )
{
	// SBO 2.0: a map's env_sprite only (the engine set its classname); the game's own sprites (SpriteCreate: no
	// classname yet) name files it ships
	if ( !FStringNull( pev->classname ) && UTIL_MapperModelUnusable( pev, STRING( pev->model ) ) )
	{
		REMOVE_ENTITY( ENT( pev ) );
		return;
	}

	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;

	Precache();
	SET_MODEL( ENT(pev), STRING(pev->model) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
	if ( pev->targetname && !(pev->spawnflags & SF_SPRITE_STARTON) )
		TurnOff();
	else
		TurnOn();
	
	// Worldcraft only sets y rotation, copy to Z
	// SBO 2.0, from Featureful: flag 8 "Don't mess Yaw" keeps the angles as the map gives them
	if ( !( pev->spawnflags & SF_SPRITE_DONT_MESS_YAW ) && pev->angles.y != 0 && pev->angles.z == 0 )
	{
		pev->angles.z = pev->angles.y;
		pev->angles.y = 0;
	}
}

// SBO 2.0, from Featureful (CSprite::AttachToEntity, Half-Life 1 SDK license): "message" names an entity the sprite
// rides on, at attachment "frags" (0: its origin); done when every entity exists and each time it is turned on
void CSprite::Activate( void )
{
	AttachToEntity();
}

void CSprite::AttachToEntity( void )
{
	if ( FStringNull( pev->message ) )
		return;
	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );
	if ( pTarget )
		SetAttachment( pTarget->edict(), (int)pev->frags );
}


void CSprite::Precache( void )
{
	PRECACHE_MODEL( (char *)STRING(pev->model) );

	// Reset attachment after save/restore
	if ( pev->aiment )
		SetAttachment( pev->aiment, pev->body );
	else
	{
		// Clear attachment
		pev->skin = 0;
		pev->body = 0;
	}
}


void CSprite::SpriteInit( const char *pSpriteName, const Vector &origin )
{
	pev->model = MAKE_STRING(pSpriteName);
	pev->origin = origin;
	Spawn();
}

CSprite *CSprite::SpriteCreate( const char *pSpriteName, const Vector &origin, BOOL animate )
{
	CSprite *pSprite = GetClassPtr( (CSprite *)NULL );
	pSprite->SpriteInit( pSpriteName, origin );
	pSprite->pev->classname = MAKE_STRING("env_sprite");
	pSprite->pev->solid = SOLID_NOT;
	pSprite->pev->movetype = MOVETYPE_NOCLIP;
	if ( animate )
		pSprite->TurnOn();

	return pSprite;
}


void CSprite::AnimateThink( void )
{
	Animate( pev->framerate * (gpGlobals->time - m_lastTime) );

	pev->nextthink		= gpGlobals->time + 0.1;
	m_lastTime			= gpGlobals->time;
}

void CSprite::AnimateUntilDead( void )
{
	if ( gpGlobals->time > pev->dmgtime )
		UTIL_Remove(this);
	else
	{
		AnimateThink();
		pev->nextthink = gpGlobals->time;
	}
}

void CSprite::Expand( float scaleSpeed, float fadeSpeed )
{
	pev->speed = scaleSpeed;
	pev->health = fadeSpeed;
	SetThink( &CSprite::ExpandThink );

	pev->nextthink	= gpGlobals->time;
	m_lastTime		= gpGlobals->time;
}


void CSprite::ExpandThink( void )
{
	float frametime = gpGlobals->time - m_lastTime;
	pev->scale += pev->speed * frametime;
	pev->renderamt -= pev->health * frametime;
	if ( pev->renderamt <= 0 )
	{
		pev->renderamt = 0;
		UTIL_Remove( this );
	}
	else
	{
		pev->nextthink		= gpGlobals->time + 0.1;
		m_lastTime			= gpGlobals->time;
	}
}


void CSprite::Animate( float frames )
{ 
	pev->frame += frames;
	if ( pev->frame > m_maxFrame )
	{
		if ( pev->spawnflags & SF_SPRITE_ONCE )
		{
			TurnOff();
		}
		else
		{
			if ( m_maxFrame > 0 )
				pev->frame = fmod( pev->frame, m_maxFrame );
		}
	}
}


void CSprite::TurnOff( void )
{
	pev->effects = EF_NODRAW;
	pev->nextthink = 0;
}


void CSprite::TurnOn( void )
{
	pev->effects = 0;

	// SBO 2.0: a studio model (docs/MODEL_SCALE.md). Not stepped like a sprite (that sent the model's frame and its
	// sprite frame rate to the client, which played the sequence "framerate" times too fast): the client animates it
	// from animtime at the playback rate "framerate" (1 = the sequence's own speed, 0 = holds its first frame), as
	// for any animated model; a looping sequence keeps looping without the server. "Play Once": hidden when the
	// sequence is done
	void *pmodel = GetStudioModelPtr( edict() );
	if ( pmodel )
	{
		pev->frame = 0;
		pev->animtime = gpGlobals->time;
		SetThink( NULL );
		pev->nextthink = 0;
		if ( ( pev->spawnflags & SF_SPRITE_ONCE ) && pev->framerate > 0 )
		{
			float flFrameRate = 0, flGroundSpeed = 0;
			GetSequenceInfo( pmodel, pev, &flFrameRate, &flGroundSpeed );
			if ( flFrameRate > 0 )
			{
				SetThink( &CSprite::StudioOnceThink );
				pev->nextthink = gpGlobals->time + 256.0 / ( flFrameRate * pev->framerate );
			}
		}
		return;
	}

	AttachToEntity();
	if ( (pev->framerate && m_maxFrame > 1.0) || (pev->spawnflags & SF_SPRITE_ONCE) )
	{
		SetThink( &CSprite::AnimateThink );
		pev->nextthink = gpGlobals->time;
		m_lastTime = gpGlobals->time;
	}
	// SBO 2.0, from Featureful: "impulse" start frame (a negative number: a random frame); 0: the first (Half-Life)
	pev->frame = 0;
	if ( pev->impulse > 0 )
		pev->frame = min( (int)m_maxFrame, pev->impulse );
	else if ( pev->impulse < 0 && m_maxFrame >= 1 )
		pev->frame = RANDOM_LONG( 0, (int)m_maxFrame );
}


void CSprite::StudioOnceThink( void )
{
	TurnOff();
}


void CSprite::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int on = pev->effects != EF_NODRAW;
	if ( ShouldToggle( useType, on ) )
	{
		if ( on )
		{
			TurnOff();
		}
		else
		{
			TurnOn();
		}
	}
}


class CGibShooter : public CBaseDelay
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void EXPORT ShootThink( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual CGib *CreateGib( void );
	// SBO 2.0 (Featureful env_shooter "m_iPhysics" 2-6): a shot instead of a gib (NULL: a gib); and what to do
	// with a new gib once it flies
	virtual CBaseEntity *CreateShot( const Vector &vecPos, const Vector &vecVel ) { return NULL; }
	virtual void GibLaunched( CGib *pGib ) {}
	string_t m_iszSpawnTarget;	// SBO 2.0 (Featureful "m_iszSpawnTarget"): fired for each gib / shot (activator: it)

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int	m_iGibs;
	int m_iGibCapacity;
	int m_iGibMaterial;
	int m_iGibModelIndex;
	float m_flGibVelocity;
	float m_flVariance;
	float m_flGibLife;

	// SBO 2.0, from Featureful / Spirit of Half-Life: position [LP], direction and speed [LV], speed factor [LR]
	// ("m_flVelocity" also takes a [LR] name or "a..b"); the activator they are computed for
	string_t m_iszPosition;
	string_t m_iszVelocity;
	string_t m_iszVelFactor;
	BOOL m_bVelocityKey;	// "m_flVelocity" was set (a number)
};

TYPEDESCRIPTION CGibShooter::m_SaveData[] =
{
	DEFINE_FIELD( CGibShooter, m_iGibs, FIELD_INTEGER ),
	DEFINE_FIELD( CGibShooter, m_iGibCapacity, FIELD_INTEGER ),
	DEFINE_FIELD( CGibShooter, m_iGibMaterial, FIELD_INTEGER ),
	DEFINE_FIELD( CGibShooter, m_iGibModelIndex, FIELD_INTEGER ),
	DEFINE_FIELD( CGibShooter, m_flGibVelocity, FIELD_FLOAT ),
	DEFINE_FIELD( CGibShooter, m_flVariance, FIELD_FLOAT ),
	DEFINE_FIELD( CGibShooter, m_flGibLife, FIELD_FLOAT ),
	DEFINE_FIELD( CGibShooter, m_iszPosition, FIELD_STRING ),
	DEFINE_FIELD( CGibShooter, m_iszVelocity, FIELD_STRING ),
	DEFINE_FIELD( CGibShooter, m_iszVelFactor, FIELD_STRING ),
	DEFINE_FIELD( CGibShooter, m_bVelocityKey, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGibShooter, m_iszSpawnTarget, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CGibShooter, CBaseDelay );
LINK_ENTITY_TO_CLASS( gibshooter, CGibShooter );


void CGibShooter :: Precache ( void )
{
	if ( g_Language == LANGUAGE_GERMAN )
	{
		m_iGibModelIndex = PRECACHE_MODEL ("models/germanygibs.mdl");
	}
	else
	{
		m_iGibModelIndex = PRECACHE_MODEL ("models/hgibs.mdl");
	}
}


void CGibShooter::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iGibs"))
	{
		m_iGibs = m_iGibCapacity = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flVelocity"))
	{
		// SBO 2.0 (Spirit / Featureful: a [LR] key): an entity name or "a..b" is computed when shooting; a number
		// is read as in Half-Life (".5" too, which Spirit's number test would take for a name)
		const char *v = pkvd->szValue;
		while ( *v == ' ' )
			v++;
		if ( strstr( v, ".." ) || ( *v && !( isdigit( (unsigned char)*v ) || *v == '-' || *v == '+' || *v == '.' ) ) )
			m_iszVelFactor = ALLOC_STRING( v );
		else
		{
			m_flGibVelocity = atof(pkvd->szValue);
			m_bVelocityKey = TRUE;
		}
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszVelFactor"))	// SBO 2.0 (Spirit of Half-Life)
	{
		m_iszVelFactor = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszPosition"))	// SBO 2.0 (Spirit / Featureful)
	{
		m_iszPosition = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszVelocity"))	// SBO 2.0 (Spirit / Featureful)
	{
		m_iszVelocity = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flVariance"))
	{
		m_flVariance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flGibLife"))
	{
		m_flGibLife = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSpawnTarget"))	// SBO 2.0 (Featureful)
	{
		m_iszSpawnTarget = pkvd->szValue[0] ? ALLOC_STRING(pkvd->szValue) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseDelay::KeyValue( pkvd );
	}
}

void CGibShooter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;	// SBO 2.0: the Locus keys are computed for it
	SetThink( &CGibShooter::ShootThink );
	pev->nextthink = gpGlobals->time;
	if ( pev->spawnflags & SF_GIBSHOOTER_INSTANT_START )	// SBO 2.0 (Featureful "Start instantly")
		ShootThink();
}

void CGibShooter::Spawn( void )
{
	Precache();

	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	if ( m_flDelay == 0 )
	{
		m_flDelay = 0.1;
	}

	if ( m_flGibLife == 0 )
	{
		m_flGibLife = 25;
	}

	SetMovedir ( pev );
	pev->body = MODEL_FRAMES( m_iGibModelIndex );
}


CGib *CGibShooter :: CreateGib ( void )
{
	if ( CVAR_GET_FLOAT("violence_hgibs") == 0 )
		return NULL;

	CGib *pGib = GetClassPtr( (CGib *)NULL );
	pGib->Spawn( "models/hgibs.mdl" );
	pGib->m_bloodColor = BLOOD_COLOR_RED;

	if ( pev->body <= 1 )
	{
		ALERT ( at_aiconsole, "GibShooter Body is <= 1!\n" );
	}

	pGib->pev->body = RANDOM_LONG ( 1, pev->body - 1 );// avoid throwing random amounts of the 0th gib. (skull).

	return pGib;
}


void CGibShooter :: ShootThink ( void )
{
	pev->nextthink = gpGlobals->time + m_flDelay;

	Vector vecShootDir;

	// SBO 2.0, from Featureful / Spirit of Half-Life: the Locus keys. Without them: as in Half-Life. A key that
	// gives no value skips this gib (SBO: still counted, so the shooter ends; Featureful shot nothing and tried again)
	float flGibVelocity = m_flGibVelocity;
	Vector vecBaseDir = pev->movedir;
	Vector vecPos = pev->origin;
	BOOL bShoot = TRUE;

	if ( !FStringNull( m_iszVelFactor ) )
		bShoot = TryCalcLocus_Ratio( m_hActivator, STRING( m_iszVelFactor ), flGibVelocity );
	else if ( !FStringNull( m_iszVelocity ) && !m_bVelocityKey )
		flGibVelocity = 1;	// Featureful: no factor, the vector's length is the speed

	if ( bShoot && !FStringNull( m_iszVelocity ) )
	{
		Vector vecDir;
		bShoot = TryCalcLocus_Velocity( this, m_hActivator, STRING( m_iszVelocity ), vecDir );
		if ( bShoot )
		{
			flGibVelocity *= vecDir.Length();
			vecBaseDir = vecDir.Normalize();
		}
	}

	if ( bShoot && !FStringNull( m_iszPosition ) )
		bShoot = TryCalcLocus_Position( this, m_hActivator, STRING( m_iszPosition ), vecPos );

	vecShootDir = vecBaseDir;

	vecShootDir = vecShootDir + gpGlobals->v_right * RANDOM_FLOAT( -1, 1) * m_flVariance;;
	vecShootDir = vecShootDir + gpGlobals->v_forward * RANDOM_FLOAT( -1, 1) * m_flVariance;;
	vecShootDir = vecShootDir + gpGlobals->v_up * RANDOM_FLOAT( -1, 1) * m_flVariance;;

	vecShootDir = vecShootDir.Normalize();
	// SBO 2.0 (Featureful env_shooter "m_iPhysics" 2-6): a shot instead of a gib
	CBaseEntity *pShot = bShoot ? CreateShot( vecPos, vecShootDir * flGibVelocity ) : NULL;
	CGib *pGib = ( bShoot && !pShot ) ? CreateGib() : NULL;
	if ( pShot && !FStringNull( m_iszSpawnTarget ) )
		FireTargets( STRING( m_iszSpawnTarget ), pShot, this, USE_TOGGLE, 0 );
	
	if ( pGib )
	{
		pGib->pev->origin = vecPos;
		pGib->pev->velocity = vecShootDir * flGibVelocity;
	
		pGib->pev->avelocity.x = RANDOM_FLOAT ( 100, 200 );
		pGib->pev->avelocity.y = RANDOM_FLOAT ( 100, 300 );

		float thinkTime = pGib->pev->nextthink - gpGlobals->time;

		pGib->m_lifeTime = (m_flGibLife * RANDOM_FLOAT( 0.95, 1.05 ));	// +/- 5%
		if ( pGib->m_lifeTime < thinkTime )
		{
			pGib->pev->nextthink = gpGlobals->time + pGib->m_lifeTime;
			pGib->m_lifeTime = 0;
		}
		GibLaunched( pGib );	// SBO 2.0 (Featureful env_shooter flags)
		if ( !FStringNull( m_iszSpawnTarget ) )	// SBO 2.0 (Featureful)
			FireTargets( STRING( m_iszSpawnTarget ), pGib, this, USE_TOGGLE, 0 );
		
	}

	if ( --m_iGibs <= 0 )
	{
		if ( pev->spawnflags & SF_GIBSHOOTER_REPEATABLE )
		{
			m_iGibs = m_iGibCapacity;
			SetThink ( NULL );
			pev->nextthink = gpGlobals->time;
		}
		else
		{
			SetThink ( &CGibShooter::SUB_Remove );
			pev->nextthink = gpGlobals->time;
		}
	}
}


// SBO 2.0 (Featureful CShot): an env_shooter's shot ("m_iPhysics" 2-6): a sprite or model that flies; it fires
// "m_iszTouch" (activator: the shot) and "m_iszTouchOther" (activator: what it hit) when it touches something
class CShot : public CSprite
{
public:
	void Touch( CBaseEntity *pOther );
	void Start( float flLife );
};
LINK_ENTITY_TO_CLASS( shot, CShot );

void CShot::Touch( CBaseEntity *pOther )
{
	if ( pev->teleport_time > gpGlobals->time )
		return;
	pev->teleport_time = gpGlobals->time + 0.1;	// not more than every 0.1 s
	if ( !FStringNull( pev->netname ) )
		FireTargets( STRING( pev->netname ), this, this, USE_TOGGLE, 0 );
	if ( !FStringNull( pev->message ) && pOther && pOther->entindex() != 0 )
		FireTargets( STRING( pev->message ), pOther, this, USE_TOGGLE, 0 );
}

void CShot::Start( float flLife )
{
	if ( pev->framerate )
	{
		m_maxFrame = (float)MODEL_FRAMES( pev->modelindex ) - 1;
		if ( m_maxFrame > 1.0 )
		{
			if ( flLife > 0 )
			{
				pev->dmgtime = gpGlobals->time + flLife;
				SetThink( &CSprite::AnimateUntilDead );
			}
			else
				SetThink( &CSprite::AnimateThink );
			pev->nextthink = gpGlobals->time;
			m_lastTime = gpGlobals->time;
			return;
		}
	}
	if ( flLife > 0 )
	{
		SetThink( &CShot::SUB_Remove );
		pev->nextthink = gpGlobals->time + flLife;
	}
}

class CEnvShooter : public CGibShooter
{
	void		Spawn( void );
	void		Precache( void );
	void		KeyValue( KeyValueData *pkvd );

	CGib		*CreateGib( void );
	CBaseEntity	*CreateShot( const Vector &vecPos, const Vector &vecVel );
	void		GibLaunched( CGib *pGib );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	// SBO 2.0 (Featureful keys): "m_iPhysics" 0 bouncing gib (HL), 1 sticky gib, 2 noclip shot, 3 flying shot, 4
	// flying and bouncing shot, 5 shot in an arc, 6 in an arc and bouncing; "m_fFriction", "m_vecSize" (a shot's),
	// "m_iszTouch" / "m_iszTouchOther" (a shot's touch targets), "bloodcolor" (gibs bleed; 0 they don't, HL),
	// "start_gib_body" (the first submodel a gib can have)
	int			m_iPhysics;
	float		m_fFriction;
	Vector		m_vecSize;
	string_t	m_iszTouch;
	string_t	m_iszTouchOther;
	int			m_iBloodColor;
	int			m_iStartGibBody;
};

LINK_ENTITY_TO_CLASS( env_shooter, CEnvShooter );

TYPEDESCRIPTION CEnvShooter::m_SaveData[] =
{
	DEFINE_FIELD( CEnvShooter, m_iPhysics, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvShooter, m_fFriction, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvShooter, m_vecSize, FIELD_VECTOR ),
	DEFINE_FIELD( CEnvShooter, m_iszTouch, FIELD_STRING ),
	DEFINE_FIELD( CEnvShooter, m_iszTouchOther, FIELD_STRING ),
	DEFINE_FIELD( CEnvShooter, m_iBloodColor, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvShooter, m_iStartGibBody, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CEnvShooter, CGibShooter );

void CEnvShooter :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "shootmodel"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "shootsounds"))
	{
		int iNoise = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
		switch( iNoise )
		{
		case 0:
			m_iGibMaterial = matGlass;
			break;
		case 1:
			m_iGibMaterial = matWood;
			break;
		case 2:
			m_iGibMaterial = matMetal;
			break;
		case 3:
			m_iGibMaterial = matFlesh;
			break;
		case 4:
			m_iGibMaterial = matRocks;
			break;
		
		default:
		case -1:
			m_iGibMaterial = matNone;
			break;
		}
	}
	// SBO 2.0 (Featureful keys)
	else if (FStrEq(pkvd->szKeyName, "m_iPhysics"))
	{
		m_iPhysics = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fFriction"))
	{
		m_fFriction = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_vecSize"))
	{
		UTIL_StringToVector( (float *)m_vecSize, pkvd->szValue );
		m_vecSize = m_vecSize / 2;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouch"))
	{
		m_iszTouch = pkvd->szValue[0] ? ALLOC_STRING(pkvd->szValue) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouchOther"))
	{
		m_iszTouchOther = pkvd->szValue[0] ? ALLOC_STRING(pkvd->szValue) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "bloodcolor"))
	{
		m_iBloodColor = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "start_gib_body"))
	{
		m_iStartGibBody = max( atoi(pkvd->szValue), 0 );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CGibShooter::KeyValue( pkvd );
	}
}


void CEnvShooter :: Spawn( void )
{
	// SBO 2.0: "shootmodel" missing, the editor placeholder or a file the server doesn't have ends the map at the
	// precache
	if ( UTIL_MapperModelUnusable( pev, STRING( pev->model ) ) )
	{
		REMOVE_ENTITY( ENT( pev ) );
		return;
	}

	CGibShooter::Spawn();
}

void CEnvShooter :: Precache ( void )
{
	m_iGibModelIndex = PRECACHE_MODEL( (char *)STRING(pev->model) );
	CBreakable::MaterialSoundPrecache( (Materials)m_iGibMaterial );
}


CGib *CEnvShooter :: CreateGib ( void )
{
	CGib *pGib = GetClassPtr( (CGib *)NULL );

	pGib->Spawn( STRING(pev->model) );
	
	int bodyPart = 0;

	// SBO 2.0 (Featureful "start_gib_body"): from that submodel on
	if ( pev->body > 1 )
		bodyPart = RANDOM_LONG( min( m_iStartGibBody, pev->body - 1 ), pev->body-1 );

	pGib->pev->body = bodyPart;
	pGib->m_bloodColor = m_iBloodColor > 0 ? m_iBloodColor : DONT_BLEED;	// SBO 2.0 (Featureful "bloodcolor")
	pGib->m_material = m_iGibMaterial;

	pGib->pev->rendermode = pev->rendermode;
	pGib->pev->renderamt = pev->renderamt;
	pGib->pev->rendercolor = pev->rendercolor;
	pGib->pev->renderfx = pev->renderfx;
	pGib->pev->scale = pev->scale;
	pGib->pev->skin = pev->skin;

	if ( m_iPhysics == 1 )	// SBO 2.0 (Featureful): a sticky gib
	{
		pGib->pev->movetype = MOVETYPE_TOSS;
		pGib->pev->solid = SOLID_BBOX;
		UTIL_SetSize( pGib->pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
		pGib->SetTouch( &CGib::StickyGibTouch );
	}

	return pGib;
}

// SBO 2.0 (Featureful "Don't wait till land"): the gib fades at its life time even while it still flies
void CEnvShooter :: GibLaunched( CGib *pGib )
{
	if ( pev->spawnflags & SF_ENVSHOOTER_DONT_WAIT_TILL_LAND )
	{
		float flLife = pGib->m_lifeTime > 0 ? pGib->m_lifeTime : ( pGib->pev->nextthink - gpGlobals->time );
		pGib->SetThink( &CBaseEntity::SUB_StartFadeOut );
		pGib->pev->nextthink = gpGlobals->time + max( flLife, 0.1f );
	}
}

// SBO 2.0 (Featureful CEnvShooter::CreateGib, "m_iPhysics" 2-6): a shot of the shooter's model
CBaseEntity *CEnvShooter :: CreateShot( const Vector &vecPos, const Vector &vecVel )
{
	if ( m_iPhysics < 2 || m_iPhysics > 6 )
		return NULL;
	CShot *pShot = GetClassPtr( (CShot *)NULL );
	pShot->pev->classname = MAKE_STRING( "shot" );
	pShot->pev->solid = SOLID_SLIDEBOX;
	pShot->pev->origin = vecPos;
	pShot->pev->velocity = vecVel;
	SET_MODEL( ENT( pShot->pev ), STRING( pev->model ) );
	UTIL_SetSize( pShot->pev, -m_vecSize, m_vecSize );
	pShot->pev->renderamt = pev->renderamt;
	pShot->pev->rendermode = pev->rendermode;
	pShot->pev->rendercolor = pev->rendercolor;
	pShot->pev->renderfx = pev->renderfx;
	pShot->pev->netname = m_iszTouch;
	pShot->pev->message = m_iszTouchOther;
	const char *pszModel = STRING( pev->model );
	const char *pszExt = strstr( pszModel, ".mdl" );
	if ( pszExt && strlen( pszExt ) == 4 )
	{
		pShot->pev->skin = pev->skin;
		pShot->pev->body = pev->body;
	}
	pShot->pev->scale = pev->scale;
	pShot->pev->frame = pev->frame;
	pShot->pev->framerate = pev->framerate;
	pShot->pev->friction = m_fFriction;
	switch ( m_iPhysics )
	{
	case 2:
		pShot->pev->movetype = MOVETYPE_NOCLIP;
		pShot->pev->solid = SOLID_NOT;
		break;
	case 3:
		pShot->pev->movetype = MOVETYPE_FLYMISSILE;
		break;
	case 4:
		pShot->pev->movetype = MOVETYPE_BOUNCEMISSILE;
		break;
	case 5:
		pShot->pev->movetype = MOVETYPE_TOSS;
		break;
	case 6:
		pShot->pev->movetype = MOVETYPE_BOUNCE;
		break;
	}
	pShot->Start( m_flGibLife );
	return pShot;
}




class CTestEffect : public CBaseDelay
{
public:
	void	Spawn( void );
	void	Precache( void );
	// void	KeyValue( KeyValueData *pkvd );
	void EXPORT TestThink( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int		m_iLoop;
	int		m_iBeam;
	CBeam	*m_pBeam[24];
	float	m_flBeamTime[24];
	float	m_flStartTime;
};


LINK_ENTITY_TO_CLASS( test_effect, CTestEffect );

void CTestEffect::Spawn( void )
{
	Precache( );
}

void CTestEffect::Precache( void )
{
	PRECACHE_MODEL( "sprites/lgtning.spr" );
}

void CTestEffect::TestThink( void )
{
	int i;
	float t = (gpGlobals->time - m_flStartTime);

	if (m_iBeam < 24)
	{
		CBeam *pbeam = CBeam::BeamCreate( "sprites/lgtning.spr", 100 );

		TraceResult		tr;

		Vector vecSrc = pev->origin;
		Vector vecDir = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir = vecDir.Normalize();
		UTIL_TraceLine( vecSrc, vecSrc + vecDir * 128, ignore_monsters, ENT(pev), &tr);

		pbeam->PointsInit( vecSrc, tr.vecEndPos );
		// pbeam->SetColor( 80, 100, 255 );
		pbeam->SetColor( 255, 180, 100 );
		pbeam->SetWidth( 100 );
		pbeam->SetScrollRate( 12 );
		
		m_flBeamTime[m_iBeam] = gpGlobals->time;
		m_pBeam[m_iBeam] = pbeam;
		m_iBeam++;

#if 0
		Vector vecMid = (vecSrc + tr.vecEndPos) * 0.5;
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE(TE_DLIGHT);
			WRITE_COORD(vecMid.x);	// X
			WRITE_COORD(vecMid.y);	// Y
			WRITE_COORD(vecMid.z);	// Z
			WRITE_BYTE( 20 );		// radius * 0.1
			WRITE_BYTE( 255 );		// r
			WRITE_BYTE( 180 );		// g
			WRITE_BYTE( 100 );		// b
			WRITE_BYTE( 20 );		// time * 10
			WRITE_BYTE( 0 );		// decay * 0.1
		MESSAGE_END( );
#endif
	}

	if (t < 3.0)
	{
		for (i = 0; i < m_iBeam; i++)
		{
			t = (gpGlobals->time - m_flBeamTime[i]) / ( 3 + m_flStartTime - m_flBeamTime[i]);
			m_pBeam[i]->SetBrightness( 255 * t );
			// m_pBeam[i]->SetScrollRate( 20 * t );
		}
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		for (i = 0; i < m_iBeam; i++)
		{
			UTIL_Remove( m_pBeam[i] );
		}
		m_flStartTime = gpGlobals->time;
		m_iBeam = 0;
		// pev->nextthink = gpGlobals->time;
		SetThink( NULL );
	}
}


void CTestEffect::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CTestEffect::TestThink );
	pev->nextthink = gpGlobals->time + 0.1;
	m_flStartTime = gpGlobals->time;
}



// Blood effects
class CBlood : public CPointEntity
{
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	inline	int		Color( void ) { return pev->impulse; }
	inline	float 	BloodAmount( void ) { return pev->dmg; }

	inline	void SetColor( int color ) { pev->impulse = color; }
	inline	void SetBloodAmount( float amount ) { pev->dmg = amount; }
	
	Vector	Direction( void );
	Vector	BloodPosition( CBaseEntity *pActivator );
	BOOL	LocusPlace( CBaseEntity *pActivator, Vector &vecPos, Vector &vecDir );

private:
};

LINK_ENTITY_TO_CLASS( env_blood, CBlood );



#define SF_BLOOD_RANDOM		0x0001
#define SF_BLOOD_STREAM		0x0002
#define SF_BLOOD_PLAYER		0x0004
#define SF_BLOOD_DECAL		0x0008

void CBlood::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;
	SetMovedir( pev );
}


void CBlood::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "color"))
	{
		int color = atoi(pkvd->szValue);
		switch( color )
		{
		case 0:
			SetColor( BLOOD_COLOR_RED );
			break;
		case 1:
			SetColor( BLOOD_COLOR_YELLOW );
			break;
		default:
			// SBO 2.0, from Featureful: any other number is a palette color (the FGD lists them); Half-Life made it red
			SetColor( ( color > 1 && color < 256 ) ? color : BLOOD_COLOR_RED );
			break;
		}

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "amount"))
	{
		SetBloodAmount( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}


Vector CBlood::Direction( void )
{
	if ( pev->spawnflags & SF_BLOOD_RANDOM )
		return UTIL_RandomBloodVector();
	
	return pev->movedir;
}

// SBO 2.0, from Featureful: "target" position [LP] and "netname" direction [LV] (empty: here / the angles); FALSE when a
// Locus key cannot be computed (no blood then)
BOOL CBlood::LocusPlace( CBaseEntity *pActivator, Vector &vecPos, Vector &vecDir )
{
	vecDir = Direction();
	if ( !( pev->spawnflags & SF_BLOOD_RANDOM ) && !FStringNull( pev->netname ) &&
		!TryCalcLocus_Velocity( this, pActivator, STRING( pev->netname ), vecDir ) )
		return FALSE;
	vecPos = BloodPosition( pActivator );
	if ( !( pev->spawnflags & SF_BLOOD_PLAYER ) && !FStringNull( pev->target ) &&
		!TryCalcLocus_Position( this, pActivator, STRING( pev->target ), vecPos ) )
		return FALSE;
	return TRUE;
}


Vector CBlood::BloodPosition( CBaseEntity *pActivator )
{
	if ( pev->spawnflags & SF_BLOOD_PLAYER )
	{
		edict_t *pPlayer;

		if ( pActivator && pActivator->IsPlayer() )
		{
			pPlayer = pActivator->edict();
		}
		else
			pPlayer = g_engfuncs.pfnPEntityOfEntIndex( 1 );
		if ( pPlayer )
			return (pPlayer->v.origin + pPlayer->v.view_ofs) + Vector( RANDOM_FLOAT(-10,10), RANDOM_FLOAT(-10,10), RANDOM_FLOAT(-10,10) );
	}

	return pev->origin;
}


void CBlood::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecPos, vecDir;
	if ( !LocusPlace( pActivator, vecPos, vecDir ) )
		return;

	if ( pev->spawnflags & SF_BLOOD_STREAM )
		UTIL_BloodStream( vecPos, vecDir, (Color() == BLOOD_COLOR_RED) ? 70 : Color(), BloodAmount() );
	else
		UTIL_BloodDrips( vecPos, vecDir, Color(), BloodAmount() );

	if ( pev->spawnflags & SF_BLOOD_DECAL )
	{
		Vector forward = vecDir;
		Vector start = vecPos;
		TraceResult tr;

		UTIL_TraceLine( start, start + forward * BloodAmount() * 2, ignore_monsters, NULL, &tr );
		if ( tr.flFraction != 1.0 )
			UTIL_BloodDecalTrace( &tr, Color() );
	}
}



// Screen shake
class CShake : public CPointEntity
{
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	inline	float	Amplitude( void ) { return pev->scale; }
	inline	float	Frequency( void ) { return pev->dmg_save; }
	inline	float	Duration( void ) { return pev->dmg_take; }
	inline	float	Radius( void ) { return pev->dmg; }

	inline	void	SetAmplitude( float amplitude ) { pev->scale = amplitude; }
	inline	void	SetFrequency( float frequency ) { pev->dmg_save = frequency; }
	inline	void	SetDuration( float duration ) { pev->dmg_take = duration; }
	inline	void	SetRadius( float radius ) { pev->dmg = radius; }
private:
};

LINK_ENTITY_TO_CLASS( env_shake, CShake );

// pev->scale is amplitude
// pev->dmg_save is frequency
// pev->dmg_take is duration
// pev->dmg is radius
// radius of 0 means all players
// NOTE: UTIL_ScreenShake() will only shake players who are on the ground

#define SF_SHAKE_EVERYONE	0x0001		// Don't check radius
// UNDONE: These don't work yet
#define SF_SHAKE_DISRUPT	0x0002		// Disrupt controls
#define SF_SHAKE_INAIR		0x0004		// Shake players in air

void CShake::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;
	
	if ( pev->spawnflags & SF_SHAKE_EVERYONE )
		pev->dmg = 0;
}


void CShake::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "amplitude"))
	{
		SetAmplitude( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "frequency"))
	{
		SetFrequency( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "radius"))
	{
		SetRadius( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}


void CShake::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SBO 2.0, from Featureful: "message" is the position [LP] (an entity, !activator, "x y z"); empty: here
	Vector vecPos = pev->origin;
	if ( !FStringNull( pev->message ) && !TryCalcLocus_Position( this, pActivator, STRING( pev->message ), vecPos ) )
		return;
	UTIL_ScreenShake( vecPos, Amplitude(), Frequency(), Duration(), Radius() );
}


class CFade : public CPointEntity
{
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	inline	float	Duration( void ) { return pev->dmg_take; }
	inline	float	HoldTime( void ) { return pev->dmg_save; }

	inline	void	SetDuration( float duration ) { pev->dmg_take = duration; }
	inline	void	SetHoldTime( float hold ) { pev->dmg_save = hold; }
private:
};

LINK_ENTITY_TO_CLASS( env_fade, CFade );

// pev->dmg_take is duration
// pev->dmg_save is hold duration
#define SF_FADE_IN				0x0001		// Fade in, not out
#define SF_FADE_MODULATE		0x0002		// Modulate, don't blend
#define SF_FADE_ONLYONE			0x0004
#define SF_FADE_BLINDDIRECT		0x0008		// SBO 2.0, from Featureful: "Directional blind"

// SBO 2.0, from Featureful (CalculateFadeAlpha, Half-Life 1 SDK license): with "Directional blind" the fade's alpha
// for a player depends on how much he looks at the env_fade: full when facing it (within about 30 degrees),
// fading to nothing when it is beside or behind him
static int Fade_DirectionalAlpha( const Vector &vecSource, CBaseEntity *pPlayer, int alpha )
{
	UTIL_MakeVectors( pPlayer->pev->v_angle );
	Vector vecTo = ( vecSource - ( pPlayer->pev->origin + pPlayer->pev->view_ofs ) ).Normalize();
	float flDot = DotProduct( gpGlobals->v_forward, vecTo );
	if ( flDot < 0 )
		return 0;
	return (int)( alpha * min( flDot + 0.134f, 1.0f ) );
}

void CFade::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;
}


void CFade::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		SetHoldTime( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}


void CFade::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int fadeFlags = 0;
	
	if ( !(pev->spawnflags & SF_FADE_IN) )
		fadeFlags |= FFADE_OUT;

	if ( pev->spawnflags & SF_FADE_MODULATE )
		fadeFlags |= FFADE_MODULATE;

	const BOOL fDirectional = FBitSet( pev->spawnflags, SF_FADE_BLINDDIRECT );
	if ( pev->spawnflags & SF_FADE_ONLYONE )
	{
		if ( pActivator && pActivator->IsNetClient() )
		{
			int alpha = fDirectional ? Fade_DirectionalAlpha( pev->origin, pActivator, (int)pev->renderamt ) : (int)pev->renderamt;
			UTIL_ScreenFade( pActivator, pev->rendercolor, Duration(), HoldTime(), alpha, fadeFlags );
		}
	}
	else if ( fDirectional )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer )
				UTIL_ScreenFade( pPlayer, pev->rendercolor, Duration(), HoldTime(),
					Fade_DirectionalAlpha( pev->origin, pPlayer, (int)pev->renderamt ), fadeFlags );
		}
	}
	else
	{
		UTIL_ScreenFadeAll( pev->rendercolor, Duration(), HoldTime(), pev->renderamt, fadeFlags );
	}
	SUB_UseTargets( this, USE_TOGGLE, 0 );
}


class CMessage : public CPointEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );
private:
};

LINK_ENTITY_TO_CLASS( env_message, CMessage );


void CMessage::Spawn( void )
{
	Precache();

	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;

	switch( pev->impulse )
	{
	case 1: // Medium radius
		pev->speed = ATTN_STATIC;
		break;
	
	case 2:	// Large radius
		pev->speed = ATTN_NORM;
		break;

	case 3:	//EVERYWHERE
		pev->speed = ATTN_NONE;
		break;
	
	default:
	case 0: // Small radius
		pev->speed = ATTN_IDLE;
		break;
	}
	pev->impulse = 0;

	// No volume, use normal
	if ( pev->scale <= 0 )
		pev->scale = 1.0;
}


void CMessage::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING(pev->noise) );
}

void CMessage::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "messagesound"))
	{
		pev->noise = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messagevolume"))
	{
		pev->scale = atof(pkvd->szValue) * 0.1;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messageattenuation"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}


void CMessage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pPlayer = NULL;

	if ( pev->spawnflags & SF_MESSAGE_ALL )
		UTIL_ShowMessageAll( STRING(pev->message) );
	else
	{
		if ( pActivator && pActivator->IsPlayer() )
			pPlayer = pActivator;
		else
		{
			pPlayer = CBaseEntity::Instance( g_engfuncs.pfnPEntityOfEntIndex( 1 ) );
		}
		if ( pPlayer )
			UTIL_ShowMessage( STRING(pev->message), pPlayer );
	}
	if ( pev->noise )
	{
		EMIT_SOUND( edict(), CHAN_BODY, STRING(pev->noise), pev->scale, pev->speed );
	}
	if ( pev->spawnflags & SF_MESSAGE_ONCE )
		UTIL_Remove( this );

	SUB_UseTargets( this, USE_TOGGLE, 0 );
}



//=========================================================
// FunnelEffect
//=========================================================
class CEnvFunnel : public CBaseDelay
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int		m_iSprite;	// Don't save, precache
};

// SBO 2.0, from Featureful: "netname" particle sprite (empty: sprites/flare6.spr), "message" position [LP] (empty:
// here), flag 2 "Repeatable" (else removed after one use, as in Half-Life)
#define SF_FUNNEL_REPEATABLE	2

void CEnvFunnel :: Precache ( void )
{
	m_iSprite = PRECACHE_MODEL( FStringNull( pev->netname ) ? (char *)"sprites/flare6.spr" : (char *)STRING( pev->netname ) );
}

LINK_ENTITY_TO_CLASS( env_funnel, CEnvFunnel );

void CEnvFunnel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecPos = pev->origin;
	if ( !FStringNull( pev->message ) && !TryCalcLocus_Position( this, pActivator, STRING( pev->message ), vecPos ) )
		return;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_LARGEFUNNEL );
		WRITE_COORD( vecPos.x );
		WRITE_COORD( vecPos.y );
		WRITE_COORD( vecPos.z );
		WRITE_SHORT( m_iSprite );

		if ( pev->spawnflags & SF_FUNNEL_REVERSE )// funnel flows in reverse?
		{
			WRITE_SHORT( 1 );
		}
		else
		{
			WRITE_SHORT( 0 );
		}


	MESSAGE_END();

	if ( FBitSet( pev->spawnflags, SF_FUNNEL_REPEATABLE ) )
		return;
	SetThink( &CEnvFunnel::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

void CEnvFunnel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
}

//=========================================================
// Beverage Dispenser
// overloaded pev->frags, is now a flag for whether or not a can is stuck in the dispenser. 
// overloaded pev->health, is now how many cans remain in the machine.
//=========================================================
class CEnvBeverage : public CBaseDelay
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CEnvBeverage :: Precache ( void )
{
	PRECACHE_MODEL( "models/can.mdl" );
	PRECACHE_SOUND( "weapons/g_bounce3.wav" );
}

LINK_ENTITY_TO_CLASS( env_beverage, CEnvBeverage );

void CEnvBeverage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->frags != 0 || pev->health <= 0 )
	{
		// no more cans while one is waiting in the dispenser, or if I'm out of cans.
		return;
	}

	CBaseEntity *pCan = CBaseEntity::Create( "item_sodacan", pev->origin, pev->angles, edict() );	

	if ( pev->skin == 6 )
	{
		// random
		pCan->pev->skin = RANDOM_LONG( 0, 5 );
	}
	else
	{
		pCan->pev->skin = pev->skin;
	}

	pev->frags = 1;
	pev->health--;

	//SetThink (&CEnvBeverage::SUB_Remove);
	//pev->nextthink = gpGlobals->time;
}

void CEnvBeverage::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->frags = 0;

	if ( pev->health == 0 )
	{
		pev->health = 10;
	}
}

//=========================================================
// Soda can
//=========================================================
class CItemSoda : public CBaseEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	EXPORT CanThink ( void );
	void	EXPORT CanTouch ( CBaseEntity *pOther );
};

void CItemSoda :: Precache ( void )
{
	// a can placed in the map (the FGD offers item_sodacan) set a model nobody had precached: the engine stops the
	// server ("no precache"); env_beverage's cans find it precached already
	PRECACHE_MODEL( "models/can.mdl" );
}

LINK_ENTITY_TO_CLASS( item_sodacan, CItemSoda );

void CItemSoda::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;

	SET_MODEL ( ENT(pev), "models/can.mdl" );
	UTIL_SetSize ( pev, Vector ( 0, 0, 0 ), Vector ( 0, 0, 0 ) );
	
	SetThink (&CItemSoda::CanThink);
	pev->nextthink = gpGlobals->time + 0.5;
}

void CItemSoda::CanThink ( void )
{
	EMIT_SOUND (ENT(pev), CHAN_WEAPON, "weapons/g_bounce3.wav", 1, ATTN_NORM );

	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize ( pev, Vector ( -8, -8, 0 ), Vector ( 8, 8, 8 ) );
	SetThink ( NULL );
	SetTouch ( &CItemSoda::CanTouch );
}

void CItemSoda::CanTouch ( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	// spoit sound here

	pOther->TakeHealth( 1, DMG_GENERIC );// a bit of health.

	if ( !FNullEnt( pev->owner ) )
	{
		// tell the machine the can was taken
		pev->owner->v.frags = 0;
	}

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = EF_NODRAW;
	SetTouch ( NULL );
	SetThink ( &CItemSoda::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}


//=========================================================
// SoHL (Spirit of Half-Life 1.2 by Laurie Cheers "LRC", dlls/effects.cpp; the scale is AJH's, SoHL 1.8): env_sky,
// an Unreal Tournament style 3D sky. The clients draw the map's sky room from this point before each view
// (cl_dll/view.cpp V_SkyViewStart), and it shows where the view has no face: the env_sky tool texture. See
// docs/ENV_SKY.md.
// SBO 2.0: SoHL sent the point once to everyone a second after the map started (MSG_BROADCAST), and the client's
// InitHUD cleared it, so a player who joined later had no sky. Each player now gets the state with every HUD
// initialisation (EnvSky_SendState from CBasePlayer::UpdateClientData: connect, map change, fullupdate, demo
// start), and a change goes to all. One env_sky is on at a time: at the start the first one in the map without
// "Start off", then the last one triggered on; triggering off the one that is on leaves the map without 3D sky.
//=========================================================
#define SF_ENVSKY_START_OFF		0x0001

extern int gmsgSetSky;

class CEnvSky : public CPointEntity
{
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	UpdateOnRemove( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void	SendState( CBasePlayer *pPlayer );	// NULL: to all

	BOOL	m_fActive;
};

LINK_ENTITY_TO_CLASS( env_sky, CEnvSky );

TYPEDESCRIPTION	CEnvSky::m_SaveData[] =
{
	DEFINE_FIELD( CEnvSky, m_fActive, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CEnvSky, CPointEntity );

// the env_sky that is on, if any
static CEnvSky *EnvSky_Active( void )
{
	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "env_sky" ) ) != NULL )
	{
		if ( static_cast<CEnvSky *>( pEntity )->m_fActive )
			return static_cast<CEnvSky *>( pEntity );
	}

	return NULL;
}

void CEnvSky :: Spawn( void )
{
	CPointEntity::Spawn();
	pev->effects |= EF_NODRAW;

	// the env_sky entities spawn in the order of the map
	m_fActive = !FBitSet( pev->spawnflags, SF_ENVSKY_START_OFF ) && !EnvSky_Active();
}

void CEnvSky :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SoHL: triggering sends the sky again (it was lost after loading a saved game), whatever the use type.
	// SBO 2.0: off switches it off; anything else makes this the sky, and the one that was on goes off.
	if ( useType == USE_OFF )
	{
		if ( m_fActive )
		{
			m_fActive = FALSE;
			SendState( NULL );
		}
		return;
	}

	if ( m_fActive )
		return;

	CEnvSky *pOther = EnvSky_Active();

	if ( pOther )
		pOther->m_fActive = FALSE;

	m_fActive = TRUE;
	SendState( NULL );
}

void CEnvSky :: UpdateOnRemove( void )
{
	// killtarget: no sky (it was this one)
	if ( m_fActive )
	{
		m_fActive = FALSE;
		SendState( NULL );
	}

	CPointEntity::UpdateOnRemove();
}

// SoHL "SetSky": byte mode (1 on, 0 off), coord x y z (the sky camera), byte scale (SoHL 1.8 AJH: 0 = no parallax,
// else the camera moves 1/scale of the view's offset from the map origin; the "frags" key)
void CEnvSky :: SendState( CBasePlayer *pPlayer )
{
	if ( pPlayer )
		MESSAGE_BEGIN( MSG_ONE, gmsgSetSky, NULL, pPlayer->pev );
	else
		MESSAGE_BEGIN( MSG_ALL, gmsgSetSky );

		WRITE_BYTE( m_fActive ? 1 : 0 );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_BYTE( pev->frags < 0 ? 0 : ( pev->frags > 255 ? 255 : (int)pev->frags ) );
	MESSAGE_END();
}

// SBO 2.0: the state of the map's 3D sky for one player whose HUD is initialised (CBasePlayer::UpdateClientData). The
// client's InitHUD, sent just before, clears it; nothing is sent on a map without env_sky.
void EnvSky_SendState( CBasePlayer *pPlayer )
{
	CEnvSky *pSky = EnvSky_Active();

	if ( !pSky )
		pSky = static_cast<CEnvSky *>( UTIL_FindEntityByClassname( NULL, "env_sky" ) );

	if ( pSky )
		pSky->SendState( pPlayer );
}
