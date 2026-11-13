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

===== explode.cpp ========================================================

  Explosion-related code

*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "decals.h"
#include "explode.h"
#include "locus.h"	// SBO 2.0
#include "weapons.h"	// SBO 2.0: ::RadiusDamage with a radius (env_explosion "iradius")

// Spark Shower
class CShower : public CBaseEntity
{
	void Spawn( void );
	void Think( void );
	void Touch( CBaseEntity *pOther );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
};

LINK_ENTITY_TO_CLASS( spark_shower, CShower );

void CShower::Spawn( void )
{
	pev->velocity = RANDOM_FLOAT( 200, 300 ) * pev->angles;
	pev->velocity.x += RANDOM_FLOAT(-100.f,100.f);
	pev->velocity.y += RANDOM_FLOAT(-100.f,100.f);
	if ( pev->velocity.z >= 0 )
		pev->velocity.z += 200;
	else
		pev->velocity.z -= 200;
	pev->movetype = MOVETYPE_BOUNCE;
	pev->gravity = 0.5;
	pev->nextthink = gpGlobals->time + 0.1;
	pev->solid = SOLID_NOT;
	SET_MODEL( edict(), "models/grenade.mdl");	// Need a model, just use the grenade, we don't draw it anyway
	UTIL_SetSize(pev, g_vecZero, g_vecZero );
	pev->effects |= EF_NODRAW;
	pev->speed = RANDOM_FLOAT( 0.5, 1.5 );

	pev->angles = g_vecZero;
}


void CShower::Think( void )
{
	UTIL_Sparks( pev->origin );

	pev->speed -= 0.1;
	if ( pev->speed > 0 )
		pev->nextthink = gpGlobals->time + 0.1;
	else
		UTIL_Remove( this );
	pev->flags &= ~FL_ONGROUND;
}

void CShower::Touch( CBaseEntity *pOther )
{
	if ( pev->flags & FL_ONGROUND )
		pev->velocity = pev->velocity * 0.1;
	else
		pev->velocity = pev->velocity * 0.6;

	if ( (pev->velocity.x*pev->velocity.x+pev->velocity.y*pev->velocity.y) < 10.0 )
		pev->speed = 0;
}

class CEnvExplosion : public CBaseMonster
{
public:
	void Spawn( );
	void Precache( void );
	void EXPORT Smoke ( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	// SBO 2.1: "iMagnitude" / "scale" changed at run time (trigger_changevalue / trigger_copyvalue)
	CKeyValue GetKeyValue( const char *keyName );
	void RuntimeKeyValueChanged( const char *keyName );
	void SetSpriteScale( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int m_iMagnitude;// how large is the fireball? how much damage?
	int m_spriteScale; // what's the exact fireball sprite scale? 
	EHANDLE m_hAttacker;	// SBO 2.0: ExplosionCreate's attacker (none: the explosion itself)
	int m_iRadius;			// SBO 2.0 (Featureful): "iradius", the damage radius; 0: 2.5 x magnitude (Half-Life)
	// SBO 2.0 (Featureful): "model" custom fireball sprite, "smokesprite" custom smoke sprite (0: the stock ones),
	// "scale" exact sprite scale (0: from the magnitude, Half-Life)
	string_t m_iszSmokeSprite;
	int m_iFireball;
	int m_iSmoke;
};

TYPEDESCRIPTION	CEnvExplosion::m_SaveData[] = 
{
	DEFINE_FIELD( CEnvExplosion, m_iMagnitude, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvExplosion, m_spriteScale, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvExplosion, m_hAttacker, FIELD_EHANDLE ),
	DEFINE_FIELD( CEnvExplosion, m_iRadius, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvExplosion, m_iszSmokeSprite, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CEnvExplosion, CBaseMonster );
LINK_ENTITY_TO_CLASS( env_explosion, CEnvExplosion );

void CEnvExplosion::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "iMagnitude"))
	{
		m_iMagnitude = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "iradius"))	// SBO 2.0 (Featureful)
	{
		m_iRadius = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "smokesprite"))	// SBO 2.0 (Featureful)
	{
		m_iszSmokeSprite = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

// SBO 2.0, from Featureful: the custom sprites a mapper named (at map load; an explosion made during the game by
// ExplosionCreate names none)
void CEnvExplosion::Precache( void )
{
	if ( !FStringNull( pev->model ) )
		m_iFireball = PRECACHE_MODEL( (char *)STRING( pev->model ) );
	if ( !FStringNull( m_iszSmokeSprite ) )
		m_iSmoke = PRECACHE_MODEL( (char *)STRING( m_iszSmokeSprite ) );
}

void CEnvExplosion::Spawn( void )
{ 
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
	/*
	if ( m_iMagnitude > 250 )
	{
		m_iMagnitude = 250;
	}
	*/

	SetSpriteScale();
}

// the fireball and smoke size from the magnitude, or "scale" (Spawn; SBO 2.1: again after a run-time change)
void CEnvExplosion::SetSpriteScale( void )
{
	float flSpriteScale;
	flSpriteScale = ( m_iMagnitude - 50) * 0.6;
	
	/*
	if ( flSpriteScale > 50 )
	{
		flSpriteScale = 50;
	}
	*/
	if ( flSpriteScale < 10 )
	{
		flSpriteScale = 10;
	}

	m_spriteScale = (int)flSpriteScale;

	// SBO 2.0 (Featureful): "Custom sprite scale"
	if ( pev->scale > 0 )
		m_spriteScale = min( (int)( pev->scale * 10.0f ), 255 );
}

CKeyValue CEnvExplosion::GetKeyValue( const char *keyName )
{
	CKeyValue keyvalue = g_emptyKeyValue;
	if ( FStrEq( keyName, "iMagnitude" ) || FStrEq( keyName, "iradius" ) )
	{
		keyvalue.keyType = KEY_TYPE_INT;
		keyvalue.keyName = FStrEq( keyName, "iMagnitude" ) ? "iMagnitude" : "iradius";
		keyvalue.iVal = FStrEq( keyName, "iMagnitude" ) ? m_iMagnitude : m_iRadius;
		return keyvalue;
	}
	return CBaseMonster::GetKeyValue( keyName );
}

// SBO 2.1: the damage reads the magnitude at each explosion; the fireball and smoke size were worked out once in
// Spawn, so they follow a new magnitude or scale here
void CEnvExplosion::RuntimeKeyValueChanged( const char *keyName )
{
	if ( FStrEq( keyName, "iMagnitude" ) || !stricmp( keyName, "scale" ) )
		SetSpriteScale();
}

void CEnvExplosion::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{ 
	TraceResult tr;

	pev->model = iStringNull;//invisible
	pev->solid = SOLID_NOT;// intangible

	Vector		vecSpot;// trace starts here!

	// SBO 2.0, from Featureful / Spirit of Half-Life: "target" is the position [LP] (an entity, !activator,
	// "x y z"); empty: here, as in Half-Life
	Vector		vecBase = pev->origin;
	if ( !FStringNull( pev->target ) )
	{
		if ( !TryCalcLocus_Position( this, pActivator, STRING( pev->target ), vecBase ) )
			return;
	}

	vecSpot = vecBase + Vector ( 0 , 0 , 8 );
	
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -40 ),  ignore_monsters, ENT(pev), & tr);
	
	// Pull out of the wall a bit
	if ( tr.flFraction != 1.0 )
	{
		pev->origin = tr.vecEndPos + (tr.vecPlaneNormal * (m_iMagnitude - 24) * 0.6);
	}
	else
	{
		pev->origin = vecBase;
	}

	// draw decal
	if (! ( pev->spawnflags & SF_ENVEXPLOSION_NODECAL))
	{
		if ( RANDOM_FLOAT( 0 , 1 ) < 0.5 )
		{
			UTIL_DecalTrace( &tr, DECAL_SCORCH1 );
		}
		else
		{
			UTIL_DecalTrace( &tr, DECAL_SCORCH2 );
		}
	}

	// SBO 2.0 (Featureful): "No sound"
	const int explosionFlags = ( pev->spawnflags & SF_ENVEXPLOSION_NOSOUND ) ? TE_EXPLFLAG_NOSOUND : TE_EXPLFLAG_NONE;

	// draw fireball
	if ( !( pev->spawnflags & SF_ENVEXPLOSION_NOFIREBALL ) )
	{
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_EXPLOSION);
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( m_iFireball ? m_iFireball : g_sModelIndexFireball );
			WRITE_BYTE( (BYTE)m_spriteScale ); // scale * 10
			WRITE_BYTE( 15  ); // framerate
			WRITE_BYTE( explosionFlags );
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_EXPLOSION);
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( g_sModelIndexFireball );
			WRITE_BYTE( 0 ); // no sprite
			WRITE_BYTE( 15  ); // framerate
			WRITE_BYTE( explosionFlags );
		MESSAGE_END();
	}

	// do damage
	if ( !( pev->spawnflags & SF_ENVEXPLOSION_NODAMAGE ) )
	{
		// SBO 2.0 (Featureful): "Activator is attacker" (a player who fired it gets the kills); "iradius"
		entvars_t *pevAttacker = m_hAttacker != NULL ? m_hAttacker->pev : pev;
		if ( ( pev->spawnflags & SF_ENVEXPLOSION_ACTIVATOR_IS_ATTACKER ) && pActivator )
			pevAttacker = pActivator->pev;
		const float flRadius = m_iRadius > 0 ? m_iRadius : m_iMagnitude * 2.5;
		::RadiusDamage( pev->origin, pev, pevAttacker, m_iMagnitude, flRadius, CLASS_NONE, DMG_BLAST );
	}

	SetThink( &CEnvExplosion::Smoke );
	pev->nextthink = gpGlobals->time + 0.3;

	// draw sparks
	if ( !( pev->spawnflags & SF_ENVEXPLOSION_NOSPARKS ) )
	{
		int sparkCount = RANDOM_LONG(0,3);

		for ( int i = 0; i < sparkCount; i++ )
		{
			Create( "spark_shower", pev->origin, tr.vecPlaneNormal, NULL );
		}
	}
}

void CEnvExplosion::Smoke( void )
{
	if ( !( pev->spawnflags & SF_ENVEXPLOSION_NOSMOKE ) )
	{
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( m_iSmoke ? m_iSmoke : g_sModelIndexSmoke );
			WRITE_BYTE( (BYTE)m_spriteScale ); // scale * 10
			WRITE_BYTE( 12  ); // framerate
		MESSAGE_END();
	}
	
	if ( !(pev->spawnflags & SF_ENVEXPLOSION_REPEATABLE) )
	{
		UTIL_Remove( this );
	}
}


// HACKHACK -- create one of these and fake a keyvalue to get the right explosion setup
void ExplosionCreate( const Vector &center, const Vector &angles, edict_t *pOwner, int magnitude, BOOL doDamage,
	entvars_t *pevAttacker )
{
	KeyValueData	kvd;
	char			buf[128];

	CBaseEntity *pExplosion = CBaseEntity::Create( "env_explosion", center, angles, pOwner );
	sprintf( buf, "%3d", magnitude );
	kvd.szKeyName = "iMagnitude";
	kvd.szValue = buf;
	pExplosion->KeyValue( &kvd );
	if ( !doDamage )
		pExplosion->pev->spawnflags |= SF_ENVEXPLOSION_NODAMAGE;
	if ( pevAttacker )
		( (CEnvExplosion *)pExplosion )->m_hAttacker = CBaseEntity::Instance( pevAttacker );

	pExplosion->Spawn();
	pExplosion->Use( NULL, NULL, USE_TOGGLE, 0 );
}
