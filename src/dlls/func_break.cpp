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

===== bmodels.cpp ========================================================

  spawn, think, and use functions for entities that use brush models

*/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "locus.h"		// SBO 2.0: func_breakable_effect "At position"
#include "func_break.h"
#include "decals.h"
#include "explode.h"

extern DLL_GLOBAL Vector		g_vecAttackDir;

// =================== FUNC_Breakable ==============================================

// Just add more items to the bottom of this array and they will automagically be supported
// This is done instead of just a classname in the FGD so we can control which entities can
// be spawned, and still remain fairly flexible
const char *CBreakable::pSpawnObjects[] =
{
	NULL,				// 0
	"item_battery",		// 1
	"item_healthkit",	// 2
	"weapon_9mmhandgun",// 3
	"ammo_9mmclip",		// 4
	"weapon_9mmAR",		// 5
	"ammo_9mmAR",		// 6
	"ammo_ARgrenades",	// 7
	"weapon_shotgun",	// 8
	"ammo_buckshot",	// 9
	"weapon_crossbow",	// 10
	"ammo_crossbow",	// 11
	"weapon_357",		// 12
	"ammo_357",			// 13
	"weapon_rpg",		// 14
	"ammo_rpgclip",		// 15
	"ammo_gaussclip",	// 16
	"weapon_handgrenade",// 17
	"weapon_tripmine",	// 18
	"weapon_satchel",	// 19
	"weapon_snark",		// 20
	"weapon_hornetgun",	// 21
};

void CBreakable::KeyValue( KeyValueData* pkvd )
{
	// UNDONE_WC: explicitly ignoring these fields, but they shouldn't be in the map file!
	if (FStrEq(pkvd->szKeyName, "explosion"))
	{
		if (!stricmp(pkvd->szValue, "directed"))
			m_Explosion = expDirected;
		else if (!stricmp(pkvd->szValue, "random"))
			m_Explosion = expRandom;
		else
			m_Explosion = expRandom;

		// halflife-updated #24: the FGD writes "1" (relative to attack) / "0" (random)
		if (!stricmp(pkvd->szValue, "1"))
			m_Explosion = expDirected;

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "material"))
	{
		int i = atoi( pkvd->szValue);

		// 0:glass, 1:metal, 2:flesh, 3:wood

		if ((i < 0) || (i >= matLastMaterial))
			m_Material = matWood;
		else
			m_Material = (Materials)i;

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "deadmodel"))
	{
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "shards"))
	{
//			m_iShards = atof(pkvd->szValue);
			pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "gibmodel") )
	{
		m_iszGibModel = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spawnobject") )
	{
		int object = atoi( pkvd->szValue );
		if ( object > 0 && object < ARRAYSIZE(pSpawnObjects) )
			m_iszSpawnObject = MAKE_STRING( pSpawnObjects[object] );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "explodemagnitude") )
	{
		ExplosionSetMagnitude( atoi( pkvd->szValue ) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "lip") )
		pkvd->fHandled = TRUE;
	// SBO 2.0 (Featureful)
	else if (FStrEq(pkvd->szKeyName, "m_iGibs"))
	{
		m_iGibs = atoi(pkvd->szValue);
		if ( m_iGibs > 255 )
			m_iGibs = 255;	// a byte on the wire
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "gibscale"))	// SBO 2.0 (Featureful 0a0160c4)
	{
		m_flGibScale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "whenhit"))					{ m_iszWhenHit = ALLOC_STRING(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "target_activator"))			{ m_iTargetActivator = atoi(pkvd->szValue); pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "spark_when_hit"))			{ m_fSparkWhenHit = atoi(pkvd->szValue) != 0; pkvd->fHandled = TRUE; }
	else if (FStrEq(pkvd->szKeyName, "switch_texture_when_damaged"))	{ m_fSwitchTextureWhenDamaged = atoi(pkvd->szValue) != 0; pkvd->fHandled = TRUE; }
	else
		CBaseDelay::KeyValue( pkvd );
}

// SBO 2.0, from Featureful: flags of func_breakable
#define SF_BREAK_INVERT_HIT		16		// "whenhit": the hit direction turned around
#define SF_BREAK_EXPLOSIVES_ONLY	512		// SBO 2.0 (Featureful): only blast damage hurts it
#define SF_PUSHABLE_DISABLED	(1<<24)	// SBO 2.0 (Featureful "toggleable_push"): pushing is off (set by the code only)
#define SF_BREAK_NOT_SOLID		4096
#define SF_BREAK_SMOKE_TRAILS	8192
#define SF_BREAK_TRANS_GIBS		16384

void CBreakable::UpdateOnRemove( void )
{
	if ( m_hHitProxy != NULL )
		UTIL_Remove( m_hHitProxy );
	CBaseDelay::UpdateOnRemove();
}


//
// func_breakable - bmodel that breaks into pieces after taking damage
//
LINK_ENTITY_TO_CLASS( func_breakable, CBreakable );
TYPEDESCRIPTION CBreakable::m_SaveData[] =
{
	DEFINE_FIELD( CBreakable, m_Material, FIELD_INTEGER ),
	DEFINE_FIELD( CBreakable, m_Explosion, FIELD_INTEGER ),

// Don't need to save/restore these because we precache after restore
//	DEFINE_FIELD( CBreakable, m_idShard, FIELD_INTEGER ),

	DEFINE_FIELD( CBreakable, m_angle, FIELD_FLOAT ),
	DEFINE_FIELD( CBreakable, m_iszGibModel, FIELD_STRING ),
	DEFINE_FIELD( CBreakable, m_iszSpawnObject, FIELD_STRING ),
	DEFINE_FIELD( CBreakable, m_flGibScale, FIELD_FLOAT ),	// SBO 2.0 (Featureful "gibscale")

	// Explosion magnitude is stored in pev->impulse
};

IMPLEMENT_SAVERESTORE( CBreakable, CBaseEntity );

void CBreakable::Spawn( void )
{
    Precache( );    

	if ( FBitSet( pev->spawnflags, SF_BREAK_TRIGGER_ONLY ) )
		pev->takedamage	= DAMAGE_NO;
	else
		pev->takedamage	= DAMAGE_YES;
  
	pev->solid		= FBitSet( pev->spawnflags, SF_BREAK_NOT_SOLID ) ? SOLID_NOT : SOLID_BSP;	// SBO 2.0 (Featureful): 4096
	m_flInitialHealth = pev->health;
    pev->movetype	= MOVETYPE_PUSH;
    m_angle			= pev->angles.y;
	pev->angles.y	= 0;

	// HACK:  matGlass can receive decals, we need the client to know about this
	//  so use class to store the material flag
	if ( m_Material == matGlass )
	{
		pev->playerclass = 1;
	}

	SET_MODEL(ENT(pev), STRING(pev->model) );//set size and link into world.

	SetTouch( &CBreakable::BreakTouch );
	if ( FBitSet( pev->spawnflags, SF_BREAK_TRIGGER_ONLY ) )		// Only break on trigger
		SetTouch( NULL );

	// Flag unbreakable glass as "worldbrush" so it will block ALL tracelines
	if ( !IsBreakable() && pev->rendermode != kRenderNormal )
		pev->flags |= FL_WORLDBRUSH;
}


const char *CBreakable::pSoundsWood[] = 
{
	"debris/wood1.wav",
	"debris/wood2.wav",
	"debris/wood3.wav",
};

const char *CBreakable::pSoundsFlesh[] = 
{
	"debris/flesh1.wav",
	"debris/flesh2.wav",
	"debris/flesh3.wav",
	"debris/flesh5.wav",
	"debris/flesh6.wav",
	"debris/flesh7.wav",
};

const char *CBreakable::pSoundsMetal[] = 
{
	"debris/metal1.wav",
	"debris/metal2.wav",
	"debris/metal3.wav",
};

const char *CBreakable::pSoundsConcrete[] = 
{
	"debris/concrete1.wav",
	"debris/concrete2.wav",
	"debris/concrete3.wav",
};


const char *CBreakable::pSoundsGlass[] = 
{
	"debris/glass1.wav",
	"debris/glass2.wav",
	"debris/glass3.wav",
};

const char **CBreakable::MaterialSoundList( Materials precacheMaterial, int &soundCount )
{
	const char	**pSoundList = NULL;

    switch ( precacheMaterial ) 
	{
	case matWood:
		pSoundList = pSoundsWood;
		soundCount = ARRAYSIZE(pSoundsWood);
		break;
	case matFlesh:
		pSoundList = pSoundsFlesh;
		soundCount = ARRAYSIZE(pSoundsFlesh);
		break;
	case matComputer:
	case matUnbreakableGlass:
	case matGlass:
		pSoundList = pSoundsGlass;
		soundCount = ARRAYSIZE(pSoundsGlass);
		break;

	case matMetal:
		pSoundList = pSoundsMetal;
		soundCount = ARRAYSIZE(pSoundsMetal);
		break;

	case matCinderBlock:
	case matRocks:
		pSoundList = pSoundsConcrete;
		soundCount = ARRAYSIZE(pSoundsConcrete);
		break;
	
	
	case matCeilingTile:
	case matNone:
	default:
		soundCount = 0;
		break;
	}

	return pSoundList;
}

void CBreakable::MaterialSoundPrecache( Materials precacheMaterial )
{
	const char	**pSoundList;
	int			i, soundCount = 0;

	pSoundList = MaterialSoundList( precacheMaterial, soundCount );

	for ( i = 0; i < soundCount; i++ )
	{
		PRECACHE_SOUND( (char *)pSoundList[i] );
	}
}

void CBreakable::MaterialSoundRandom( edict_t *pEdict, Materials soundMaterial, float volume )
{
	const char	**pSoundList;
	int			soundCount = 0;

	pSoundList = MaterialSoundList( soundMaterial, soundCount );

	if ( soundCount )
		EMIT_SOUND( pEdict, CHAN_BODY, pSoundList[ RANDOM_LONG(0,soundCount-1) ], volume, 1.0 );
}


// SBO 2.0: the material's break sounds precached and its default gib model (NULL: none), shared by
// func_breakable and func_breakable_effect
const char *CBreakable::MaterialBustPrecache( Materials m_Material )
{
	const char *pGibName = NULL;	// matNone ("material" 9, not in the FGD) has no gibs

    switch (m_Material) 
	{
	case matWood:
		pGibName = "models/woodgibs.mdl";
		
		PRECACHE_SOUND("debris/bustcrate1.wav");
		PRECACHE_SOUND("debris/bustcrate2.wav");
		break;
	case matFlesh:
		pGibName = "models/fleshgibs.mdl";
		
		PRECACHE_SOUND("debris/bustflesh1.wav");
		PRECACHE_SOUND("debris/bustflesh2.wav");
		break;
	case matComputer:
		PRECACHE_SOUND("buttons/spark5.wav");
		PRECACHE_SOUND("buttons/spark6.wav");
		pGibName = "models/computergibs.mdl";
		
		PRECACHE_SOUND("debris/bustmetal1.wav");
		PRECACHE_SOUND("debris/bustmetal2.wav");
		break;

	case matUnbreakableGlass:
	case matGlass:
		pGibName = "models/glassgibs.mdl";
		
		PRECACHE_SOUND("debris/bustglass1.wav");
		PRECACHE_SOUND("debris/bustglass2.wav");
		break;
	case matMetal:
		pGibName = "models/metalplategibs.mdl";
		
		PRECACHE_SOUND("debris/bustmetal1.wav");
		PRECACHE_SOUND("debris/bustmetal2.wav");
		break;
	case matCinderBlock:
		pGibName = "models/cindergibs.mdl";
		
		PRECACHE_SOUND("debris/bustconcrete1.wav");
		PRECACHE_SOUND("debris/bustconcrete2.wav");
		break;
	case matRocks:
		pGibName = "models/rockgibs.mdl";
		
		PRECACHE_SOUND("debris/bustconcrete1.wav");
		PRECACHE_SOUND("debris/bustconcrete2.wav");
		break;
	case matCeilingTile:
		pGibName = "models/ceilinggibs.mdl";
		
		PRECACHE_SOUND ("debris/bustceiling.wav");  
		break;
	}
	MaterialSoundPrecache( m_Material );
	return pGibName;
}

void CBreakable::Precache( void )
{
	const char *pGibName = MaterialBustPrecache( m_Material );
	if ( m_iszGibModel )
		pGibName = STRING(m_iszGibModel);

	m_idShard = pGibName ? PRECACHE_MODEL( (char *)pGibName ) : 0;

	// Precache the spawn item's data
	if ( m_iszSpawnObject )
		UTIL_PrecacheOther( (char *)STRING( m_iszSpawnObject ) );
}

// play shard sound when func_breakable takes damage.
// the more damage, the louder the shard sound.


void CBreakable::DamageSound( void )
{
	int pitch;
	float fvol;
	char *rgpsz[6];
	int i;
	int material = m_Material;

//	if (RANDOM_LONG(0,1))
//		return;

	if (RANDOM_LONG(0,2))
		pitch = PITCH_NORM;
	else
		pitch = 95 + RANDOM_LONG(0,34);

	fvol = RANDOM_FLOAT(0.75, 1.0);

	if (material == matComputer && RANDOM_LONG(0,1))
		material = matMetal;

	switch (material)
	{
	case matComputer:
	case matGlass:
	case matUnbreakableGlass:
		rgpsz[0] = "debris/glass1.wav";
		rgpsz[1] = "debris/glass2.wav";
		rgpsz[2] = "debris/glass3.wav";
		i = 3;
		break;

	case matWood:
		rgpsz[0] = "debris/wood1.wav";
		rgpsz[1] = "debris/wood2.wav";
		rgpsz[2] = "debris/wood3.wav";
		i = 3;
		break;

	case matMetal:
		rgpsz[0] = "debris/metal1.wav";
		rgpsz[1] = "debris/metal3.wav";
		rgpsz[2] = "debris/metal2.wav";
		i = 2;
		break;

	case matFlesh:
		rgpsz[0] = "debris/flesh1.wav";
		rgpsz[1] = "debris/flesh2.wav";
		rgpsz[2] = "debris/flesh3.wav";
		rgpsz[3] = "debris/flesh5.wav";
		rgpsz[4] = "debris/flesh6.wav";
		rgpsz[5] = "debris/flesh7.wav";
		i = 6;
		break;

	case matRocks:
	case matCinderBlock:
		rgpsz[0] = "debris/concrete1.wav";
		rgpsz[1] = "debris/concrete2.wav";
		rgpsz[2] = "debris/concrete3.wav";
		i = 3;
		break;

	case matCeilingTile:
		// UNDONE: no ceiling tile shard sound yet
		i = 0;
		break;
	}

	if (i)
		EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, rgpsz[RANDOM_LONG(0,i-1)], fvol, ATTN_NORM, 0, pitch);
}

void CBreakable::BreakTouch( CBaseEntity *pOther )
{
	float flDamage;
	entvars_t*	pevToucher = pOther->pev;
	
	// only players can break these right now
	if ( !pOther->IsPlayer() || !IsBreakable() )
	{
        return;
	}

	if ( FBitSet ( pev->spawnflags, SF_BREAK_TOUCH ) )
	{// can be broken when run into 
		flDamage = pevToucher->velocity.Length() * 0.01;

		if (flDamage >= pev->health)
		{
			SetTouch( NULL );
			TakeDamage(pevToucher, pevToucher, flDamage, DMG_CRUSH);

			// do a little damage to player if we broke glass or computer
			pOther->TakeDamage( pev, pev, flDamage/4, DMG_SLASH );
		}
	}

	if ( FBitSet ( pev->spawnflags, SF_BREAK_PRESSURE ) && pevToucher->absmin.z >= pev->maxs.z - 2 )
	{// can be broken when stood upon
		
		// play creaking sound here.
		DamageSound();

		SetThink ( &CBreakable::Die );
		SetTouch( NULL );
		
		if ( m_flDelay == 0 )
		{// !!!BUGBUG - why doesn't zero delay work?
			m_flDelay = 0.1;
		}

		pev->nextthink = pev->ltime + m_flDelay;

	}

}


//
// Smash the our breakable object
//

// Break when triggered
void CBreakable::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( IsBreakable() )
	{
		pev->angles.y = m_angle;
		UTIL_MakeVectors(pev->angles);
		g_vecAttackDir = gpGlobals->v_forward;

		m_hBreakActivator = pActivator;	// SBO 2.0 (Featureful): target_activator
		Die();
	}
}


void CBreakable::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	// random spark if this is a 'computer' object (SBO 2.0, from Featureful: or "spark_when_hit")
	if (RANDOM_LONG(0,1) )
	{
		switch( m_fSparkWhenHit ? matComputer : m_Material )
		{
			case matComputer:
			{
				UTIL_Sparks( ptr->vecEndPos );

				float flVolume = RANDOM_FLOAT ( 0.7 , 1.0 );//random volume range
				switch ( RANDOM_LONG(0,1) )
				{
					case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark5.wav", flVolume, ATTN_NORM);	break;
					case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark6.wav", flVolume, ATTN_NORM);	break;
				}
			}
			break;
			
			case matUnbreakableGlass:
				UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT(0.5,1.5) );
			break;
		}
		if ( m_fSparkWhenHit && m_Material == matUnbreakableGlass )
			UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT(0.5,1.5) );
	}

	// SBO 2.0, from Featureful (Spirit's LRC "whenhit"): fire with a Locus point at the hit (its velocity and angles
	// the hit direction), an info_target made at the first hit and reused
	if ( !FStringNull( m_iszWhenHit ) )
	{
		if ( m_hHitProxy == NULL )
			m_hHitProxy = CBaseEntity::Create( "info_target", ptr->vecEndPos, g_vecZero, NULL );
		CBaseEntity *pProxy = m_hHitProxy;
		if ( pProxy )
		{
			Vector vecHitDir = vecDir;
			if ( FBitSet( pev->spawnflags, SF_BREAK_INVERT_HIT ) )
			{
				vecHitDir.x = -vecHitDir.x;
				vecHitDir.y = -vecHitDir.y;
			}
			UTIL_SetOrigin( pProxy->pev, ptr->vecEndPos );
			pProxy->pev->velocity = vecHitDir;
			pProxy->pev->angles = UTIL_VecToAngles( vecHitDir );
			FireTargets( STRING( m_iszWhenHit ), pProxy, this, USE_TOGGLE, 0 );
		}
	}

	CBaseDelay::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

//=========================================================
// Special takedamage for func_breakable. Allows us to make
// exceptions that are breakable-specific
// bitsDamageType indicates the type of damage sustained ie: DMG_CRUSH
//=========================================================
int CBreakable :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	Vector	vecTemp;

	// SBO 2.0 (Featureful flag 512 "Explosives only")
	if ( ( pev->spawnflags & SF_BREAK_EXPLOSIVES_ONLY ) && !( bitsDamageType & DMG_BLAST ) )
		return 0;

	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin). 
	if ( pevAttacker == pevInflictor )	
	{
		vecTemp = pevInflictor->origin - ( pev->absmin + ( pev->size * 0.5 ) );
		
		// if a client hit the breakable with a crowbar, and breakable is crowbar-sensitive, break it now.
		if ( FBitSet ( pevAttacker->flags, FL_CLIENT ) &&
				 FBitSet ( pev->spawnflags, SF_BREAK_CROWBAR ) && (bitsDamageType & DMG_CLUB))
			flDamage = pev->health;
	}
	else
	// an actual missile was involved.
	{
		vecTemp = pevInflictor->origin - ( pev->absmin + ( pev->size * 0.5 ) );
	}
	
	if (!IsBreakable())
		return 0;

	// Breakables take double damage from the crowbar
	if ( bitsDamageType & DMG_CLUB )
		flDamage *= 2;

	// Boxes / glass / etc. don't take much poison damage, just the impact of the dart - consider that 10%
	if ( bitsDamageType & DMG_POISON )
		flDamage *= 0.1;

// this global is still used for glass and other non-monster killables, along with decals.
	g_vecAttackDir = vecTemp.Normalize();
		
// do the damage
	// halflife-updated PR #239: Only Trigger (DAMAGE_NO) breakables must not lose health;
	// melee NPC attacks (CheckTraceHullAttack) call TakeDamage without checking takedamage.
	const float flHealthBefore = pev->health;
	if (pev->takedamage)
		pev->health -= flDamage;
	if (pev->health <= 0)
	{
		Killed( pevAttacker, GIB_NORMAL );
		m_hBreakActivator = CBaseEntity::Instance( pevAttacker );	// SBO 2.0 (Featureful): target_activator
		Die();
		return 0;
	}
	// SBO 2.0, from Featureful: "switch_texture_when_damaged": the alternate texture at half its health or less
	if ( m_fSwitchTextureWhenDamaged && m_flInitialHealth > 0 && flHealthBefore > m_flInitialHealth * 0.5 && pev->health <= m_flInitialHealth * 0.5 )
		pev->frame = 1;

	// Make a shard noise each time func breakable is hit.
	// Don't play shard noise if cbreakable actually died.

	DamageSound();

	return 1;
}


// SBO 2.0: the material's break sound on pEdict; returns its BREAK_* gib flag (func_breakable, func_breakable_effect)
char CBreakable::MaterialBustSound( edict_t *pEdict, Materials m_Material, float fvol, int pitch )
{
	char cFlag = 0;

	switch (m_Material)
	{
	case matGlass:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(pEdict, CHAN_VOICE, "debris/bustglass1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(pEdict, CHAN_VOICE, "debris/bustglass2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_GLASS;
		break;

	case matWood:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(pEdict, CHAN_VOICE, "debris/bustcrate1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(pEdict, CHAN_VOICE, "debris/bustcrate2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_WOOD;
		break;

	case matComputer:
	case matMetal:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(pEdict, CHAN_VOICE, "debris/bustmetal1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(pEdict, CHAN_VOICE, "debris/bustmetal2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_METAL;
		break;

	case matFlesh:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(pEdict, CHAN_VOICE, "debris/bustflesh1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(pEdict, CHAN_VOICE, "debris/bustflesh2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_FLESH;
		break;

	case matRocks:
	case matCinderBlock:
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND_DYN(pEdict, CHAN_VOICE, "debris/bustconcrete1.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		case 1:	EMIT_SOUND_DYN(pEdict, CHAN_VOICE, "debris/bustconcrete2.wav", fvol, ATTN_NORM, 0, pitch);	
			break;
		}
		cFlag = BREAK_CONCRETE;
		break;

	case matCeilingTile:
		EMIT_SOUND_DYN(pEdict, CHAN_VOICE, "debris/bustceiling.wav", fvol, ATTN_NORM, 0, pitch);
		break;
	}
	return cFlag;
}

extern int gmsgBreakModel;

// SBO 2.0: the TE_BREAKMODEL gibs (iGibs 0: the client decides how many). flGibScale above 0 (Featureful 0a0160c4
// "gibscale"): the same fields in SBO's "BreakModel" message with the scale after them (the client makes the gibs,
// break_gibs.cpp); 0 or less: TE_BREAKMODEL, the engine's gibs (scale 1)
void CBreakable::BreakModel( const Vector &vecSpot, const Vector &size, const Vector &vecVelocity, int shardModelIndex, int iGibs, char cFlag, float flGibScale )
{
	const BOOL fScaled = flGibScale > 0 && gmsgBreakModel;
	if ( fScaled )
		MESSAGE_BEGIN( MSG_PVS, gmsgBreakModel, vecSpot );
	else
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
	if ( !fScaled )
		WRITE_BYTE( TE_BREAKMODEL);

		// position
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );

		// size
		WRITE_COORD( size.x);
		WRITE_COORD( size.y);
		WRITE_COORD( size.z);

		// velocity
		WRITE_COORD( vecVelocity.x ); 
		WRITE_COORD( vecVelocity.y );
		WRITE_COORD( vecVelocity.z );

		// randomization
		WRITE_BYTE( 10 ); 

		// Model
		WRITE_SHORT( shardModelIndex );	//model id#

		// # of shards
		WRITE_BYTE( iGibs );	// 0: let the client decide

		// duration
		WRITE_BYTE( 25 );// 2.5 seconds

		// flags
		WRITE_BYTE( cFlag );

	if ( fScaled )
		WRITE_SHORT( flGibScale < 327 ? (int)( flGibScale * 100 ) : 32700 );	// a short on the wire
	MESSAGE_END();
}

void CBreakable::Die( void )
{
	Vector vecSpot;// shard origin
	Vector vecVelocity;// shard velocity
	CBaseEntity *pEntity = NULL;
	char cFlag = 0;
	int pitch;
	float fvol;
	
	// halflife-updated: Don't allow explosives to damage this again to prevent spawning multiple copies of items and gibs.
	// SBO 2.0: a "Not solid" (4096) breakable is SOLID_NOT from the start: it counts as broken once Die ran
	if (pev->solid == SOLID_NOT && ( !FBitSet( pev->spawnflags, SF_BREAK_NOT_SOLID ) || m_fBroken ))
	{
		return;
	}
	m_fBroken = TRUE;

	pitch = 95 + RANDOM_LONG(0,29);

	if (pitch > 97 && pitch < 103)
		pitch = 100;

	// The more negative pev->health, the louder
	// the sound should be.

	fvol = RANDOM_FLOAT(0.85, 1.0) + (fabs(pev->health) / 100.0);	// halflife-updated: fabs, was abs((int)x)

	if (fvol > 1.0)
		fvol = 1.0;


	cFlag = MaterialBustSound( ENT(pev), m_Material, fvol, pitch );
	// SBO 2.0 (Featureful): smoke trails, transparent gibs
	if ( FBitSet( pev->spawnflags, SF_BREAK_SMOKE_TRAILS ) && !FClassnameIs( pev, "func_pushable" ) )
		cFlag |= BREAK_SMOKE;
	if ( FBitSet( pev->spawnflags, SF_BREAK_TRANS_GIBS ) && !FClassnameIs( pev, "func_pushable" ) )
		cFlag |= BREAK_TRANS;

	if (m_Explosion == expDirected)
		vecVelocity = -g_vecAttackDir * 200;	// halflife-updated #24: g_vecAttackDir points at the attacker
	else
	{
		vecVelocity.x = 0;
		vecVelocity.y = 0;
		vecVelocity.z = 0;
	}

	vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
	if ( m_idShard && m_iGibs >= 0 )	// no gib model (matNone); SBO 2.0 (Featureful): "m_iGibs" (-1: none)
		BreakModel( vecSpot, pev->size, vecVelocity, m_idShard, m_iGibs, cFlag, m_flGibScale );

	float size = pev->size.x;
	if ( size < pev->size.y )
		size = pev->size.y;
	if ( size < pev->size.z )
		size = pev->size.z;

	// !!! HACK  This should work!
	// Build a box above the entity that looks like an 8 pixel high sheet
	Vector mins = pev->absmin;
	Vector maxs = pev->absmax;
	mins.z = pev->absmax.z;
	maxs.z += 8;

	// BUGBUG -- can only find 256 entities on a breakable -- should be enough
	CBaseEntity *pList[256];
	int count = UTIL_EntitiesInBox( pList, 256, mins, maxs, FL_ONGROUND );
	if ( count )
	{
		for ( int i = 0; i < count; i++ )
		{
			ClearBits( pList[i]->pev->flags, FL_ONGROUND );
			pList[i]->pev->groundentity = NULL;
		}
	}

	// Don't fire something that could fire myself
	pev->targetname = 0;

	pev->solid = SOLID_NOT;
	// Fire targets on break
	// SBO 2.0, from Featureful: "target_activator" 1 this breakable, 2 who broke it (none: Half-Life)
	CBaseEntity *pTargetActivator = NULL;
	if ( m_iTargetActivator == 1 )
		pTargetActivator = this;
	else if ( m_iTargetActivator == 2 )
		pTargetActivator = m_hBreakActivator;
	SUB_UseTargets( pTargetActivator, USE_TOGGLE, 0 );

	SetThink( &CBreakable::SUB_Remove );
	pev->nextthink = pev->ltime + 0.1;
	if ( m_iszSpawnObject )
		CBaseEntity::Create( (char *)STRING(m_iszSpawnObject), VecBModelOrigin(pev), pev->angles, edict() );


	if ( Explodable() )
	{
		ExplosionCreate( Center(), pev->angles, edict(), ExplosionMagnitude(), TRUE );
	}
}



BOOL CBreakable :: IsBreakable( void ) 
{ 
	return m_Material != matUnbreakableGlass;
}


int	CBreakable :: DamageDecal( int bitsDamageType )
{
	if ( m_Material == matGlass  )
		return DECAL_GLASSBREAK1 + RANDOM_LONG(0,2);

	if ( m_Material == matUnbreakableGlass )
		return DECAL_BPROOF1;

	return CBaseEntity::DamageDecal( bitsDamageType );
}


class CPushable : public CBreakable
{
public:
	void	Spawn ( void );
	void	Precache( void );
	void	Touch ( CBaseEntity *pOther );
	void	Move( CBaseEntity *pMover, int push );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	EXPORT StopSound( void );
//	virtual void	SetActivator( CBaseEntity *pActivator ) { m_pPusher = pActivator; }

	virtual int	ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_CONTINUOUS_USE; }
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	inline float MaxSpeed( void ) { return m_maxSpeed; }
	
	// breakables use an overridden takedamage
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );

	virtual int DamageDecal( int bitsDamageType );	// halflife-updated (484911d)
	
	static	TYPEDESCRIPTION m_SaveData[];

	static char *m_soundNames[3];
	int		m_lastSound;	// no need to save/restore, just keeps the same sound from playing twice in a row
	float	m_maxSpeed;
	float	m_soundTime;
	BOOL	m_fToggleable;	// SBO 2.0 (Featureful "toggleable_push"): a non-player Use turns pushing off / on
};

TYPEDESCRIPTION	CPushable::m_SaveData[] = 
{
	DEFINE_FIELD( CPushable, m_maxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CPushable, m_soundTime, FIELD_TIME ),
	DEFINE_FIELD( CPushable, m_fToggleable, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CPushable, CBreakable );

LINK_ENTITY_TO_CLASS( func_pushable, CPushable );

char *CPushable :: m_soundNames[3] = { "debris/pushbox1.wav", "debris/pushbox2.wav", "debris/pushbox3.wav" };


void CPushable :: Spawn( void )
{
	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
		CBreakable::Spawn();
	else
		Precache( );

	pev->movetype	= MOVETYPE_PUSHSTEP;
	pev->solid		= SOLID_BBOX;
	SET_MODEL( ENT(pev), STRING(pev->model) );

	if ( pev->friction > 399 )
		pev->friction = 399;

	m_maxSpeed = 400 - pev->friction;
	SetBits( pev->flags, FL_FLOAT );
	pev->friction = 0;
	
	pev->origin.z += 1;	// Pick up off of the floor
	UTIL_SetOrigin( pev, pev->origin );

	// Multiply by area of the box's cross-section (assume 1000 units^3 standard volume)
	pev->skin = ( pev->skin * (pev->maxs.x - pev->mins.x) * (pev->maxs.y - pev->mins.y) ) * 0.0005;
	m_soundTime = 0;
}


void CPushable :: Precache( void )
{
	for ( int i = 0; i < 3; i++ )
		PRECACHE_SOUND( m_soundNames[i] );

	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
		CBreakable::Precache( );
}


void CPushable :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq(pkvd->szKeyName, "size") )
	{
		int bbox = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;

		switch( bbox )
		{
		case 0:	// Point
			UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
			break;

		case 2: // Big Hull!?!?	!!!BUGBUG Figure out what this hull really is
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN*2, VEC_DUCK_HULL_MAX*2);
			break;

		case 3: // Player duck
			UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
			break;

		default:
		case 1: // Player
			UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
			break;
		}

	}
	else if ( FStrEq(pkvd->szKeyName, "buoyancy") )
	{
		pev->skin = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "toggleable_push") )	// SBO 2.0 (Featureful)
	{
		m_fToggleable = atoi(pkvd->szValue) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CBreakable::KeyValue( pkvd );
}


// Pull the func_pushable
void CPushable :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// SBO 2.0 (Featureful "toggleable_push"): fired by anything but a player's +use it turns pushing off (solid
	// as a wall) or on again
	if ( m_fToggleable && ( !pCaller || !pCaller->IsPlayer() ) )
	{
		const BOOL fOn = !FBitSet( pev->spawnflags, SF_PUSHABLE_DISABLED );
		if ( ShouldToggle( useType, fOn ) )
		{
			if ( fOn )
			{
				SetBits( pev->spawnflags, SF_PUSHABLE_DISABLED );
				pev->solid = FBitSet( pev->spawnflags, SF_BREAK_NOT_SOLID ) ? SOLID_NOT : SOLID_BSP;
			}
			else
			{
				ClearBits( pev->spawnflags, SF_PUSHABLE_DISABLED );
				pev->solid = SOLID_BBOX;
			}
			UTIL_SetOrigin( pev, pev->origin );
		}
		return;
	}

	if ( !pActivator || !pActivator->IsPlayer() )
	{
		if ( pev->spawnflags & SF_PUSH_BREAKABLE )
			this->CBreakable::Use( pActivator, pCaller, useType, value );
		return;
	}

	if ( FBitSet( pev->spawnflags, SF_PUSHABLE_DISABLED ) )	// SBO 2.0
		return;

	if ( pActivator->pev->velocity != g_vecZero )
		Move( pActivator, 0 );
}


void CPushable :: Touch( CBaseEntity *pOther )
{
	if ( FBitSet( pev->spawnflags, SF_PUSHABLE_DISABLED ) )	// SBO 2.0 (Featureful "toggleable_push")
		return;

	if ( FClassnameIs( pOther->pev, "worldspawn" ) )
		return;

	Move( pOther, 1 );
}


void CPushable :: Move( CBaseEntity *pOther, int push )
{
	entvars_t*	pevToucher = pOther->pev;
	int playerTouch = 0;

	// Is entity standing on this pushable ?
	if ( FBitSet(pevToucher->flags,FL_ONGROUND) && pevToucher->groundentity && VARS(pevToucher->groundentity) == pev )
	{
		// Only push if floating
		if ( pev->waterlevel > 0 )
			pev->velocity.z += pevToucher->velocity.z * 0.1;

		return;
	}


	if ( pOther->IsPlayer() )
	{
		// HL25 (JoshA): Used to check for FORWARD too and logic was inverted
		// from comment which seems wrong.
		// Fixed to just check for USE being not set for PUSH.
		// Should have the right effect.
		if ( push && (pevToucher->button & IN_USE) )	// Don't push unless the player is not useing (pull)
			return;
		playerTouch = 1;
	}

	float factor;

	if ( playerTouch )
	{
		if ( !(pevToucher->flags & FL_ONGROUND) )	// Don't push away from jumping/falling players unless in water
		{
			if ( pev->waterlevel < 1 )
				return;
			else 
				factor = 0.1;
		}
		else
			factor = 1;
	}
	else 
		factor = 0.25;

	// HL25: This used to be added every 'frame', but to be consistent at high fps,
	// now act as if it's added at a constant rate with a fudge factor.
	extern cvar_t sv_pushable_fixed_tick_fudge;

	if ( !push && sv_pushable_fixed_tick_fudge.value >= 0.0f )
	{
		factor *= gpGlobals->frametime * sv_pushable_fixed_tick_fudge.value;
	}

	// HL25 (JoshA): Always apply this if pushing, or if under the player's velocity.
	// (fabs: HL25 calls abs on floats, which is std::abs(float) there.)
	if ( push || ( fabs(pev->velocity.x) < fabs(pevToucher->velocity.x - pevToucher->velocity.x * factor) ) )
		pev->velocity.x += pevToucher->velocity.x * factor;
	if ( push || ( fabs(pev->velocity.y) < fabs(pevToucher->velocity.y - pevToucher->velocity.y * factor) ) )
		pev->velocity.y += pevToucher->velocity.y * factor;

	float length = sqrt( pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y );
	if ( length > MaxSpeed() )	// HL25: clamp pulling too (was push only)
	{
		pev->velocity.x = (pev->velocity.x * MaxSpeed() / length );
		pev->velocity.y = (pev->velocity.y * MaxSpeed() / length );
	}
	if ( playerTouch )
	{
		// HL25 (JoshA): Match the player to our pushable's velocity.
		// Previously this always happened, but it should only
		// happen if the player is pushing (or rather, being pushed.)
		// This either stops the player in their tracks or nudges them along.
		if ( push )
		{
			pevToucher->velocity.x = pev->velocity.x;
			pevToucher->velocity.y = pev->velocity.y;
		}

		if ( (gpGlobals->time - m_soundTime) > 0.7 )
		{
			m_soundTime = gpGlobals->time;
			if ( length > 0 && FBitSet(pev->flags,FL_ONGROUND) )
			{
				m_lastSound = RANDOM_LONG(0,2);
				EMIT_SOUND(ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound], 0.5, ATTN_NORM);
	//			SetThink( &CPushable::StopSound );
	//			pev->nextthink = pev->ltime + 0.1;
			}
			else
				STOP_SOUND( ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound] );
		}
	}
}

#if 0
void CPushable::StopSound( void )
{
	Vector dist = pev->oldorigin - pev->origin;
	if ( dist.Length() <= 0 )
		STOP_SOUND( ENT(pev), CHAN_WEAPON, m_soundNames[m_lastSound] );
}
#endif

int CPushable::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if ( pev->spawnflags & SF_PUSH_BREAKABLE )
		return CBreakable::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

	return 1;
}

// halflife-updated (484911d): only a breakable pushable gets its material's decals (glass cracks); an unbreakable one
// (material 0 = glass by default) got glass-break decals from melee hits
int CPushable::DamageDecal( int bitsDamageType )
{
	if ( FBitSet( pev->spawnflags, SF_PUSH_BREAKABLE ) )
		return CBreakable::DamageDecal( bitsDamageType );

	return CBaseEntity::DamageDecal( bitsDamageType );
}


// SBO 2.0, from Featureful (halflife-featureful dlls/func_break.cpp CFuncBreakableEffect, Half-Life 1 SDK license):
// func_breakable_effect, an invisible, non-solid brush that plays the break of a func_breakable (its material's
// sound and gibs) when triggered, using func_breakable's own material code. "Repeatable" (1) keeps it for the next
// trigger (else it is removed); "At position" [LP] moves the gibs there (the sound stays at the brush);
// "Custom number of gibs" (0: the client decides, below 0: no gibs); "Smoke trails on gibs" (8192), "Transparent
// gibs" (16384).
#define SF_BREAKEFFECT_REPEATABLE		1
#define SF_BREAKEFFECT_SMOKE_TRAILS		8192
#define SF_BREAKEFFECT_TRANSPARENT_GIBS	16384

class CFuncBreakableEffect : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	Materials m_Material;
	string_t m_iszGibModel;
	int m_iGibs;
	string_t m_position;
	int m_idShard;
};

LINK_ENTITY_TO_CLASS( func_breakable_effect, CFuncBreakableEffect );

void CFuncBreakableEffect::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "material" ) )
	{
		int i = atoi( pkvd->szValue );
		if ( i < 0 || i >= matLastMaterial )
			m_Material = matWood;
		else
			m_Material = (Materials)i;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gibmodel" ) )
	{
		m_iszGibModel = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "m_iGibs" ) )
	{
		m_iGibs = atoi( pkvd->szValue );
		if ( m_iGibs > 255 )
			m_iGibs = 255;	// a byte on the wire
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "position" ) )
	{
		m_position = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CFuncBreakableEffect::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SET_MODEL( ENT( pev ), STRING( pev->model ) );	// size, and link into the world
	pev->effects |= EF_NODRAW;
}

void CFuncBreakableEffect::Precache( void )
{
	const char *pGibName = CBreakable::MaterialBustPrecache( m_Material );
	if ( m_iszGibModel )
		pGibName = STRING( m_iszGibModel );
	m_idShard = pGibName ? PRECACHE_MODEL( (char *)pGibName ) : 0;
}

void CFuncBreakableEffect::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecOrigin = pev->origin;
	if ( !FStringNull( m_position ) )
	{
		if ( !TryCalcLocus_Position( this, pActivator, STRING( m_position ), vecOrigin ) )
			return;
	}

	int pitch = 95 + RANDOM_LONG( 0, 29 );
	if ( pitch > 97 && pitch < 103 )
		pitch = 100;
	float fvol = RANDOM_FLOAT( 0.85, 1.0 );

	// the sound plays at the brush (an entity sound); "At position" moves only the gibs, as in Featureful
	char cFlag = CBreakable::MaterialBustSound( ENT( pev ), m_Material, fvol, pitch );
	if ( FBitSet( pev->spawnflags, SF_BREAKEFFECT_SMOKE_TRAILS ) )
		cFlag |= BREAK_SMOKE;
	if ( FBitSet( pev->spawnflags, SF_BREAKEFFECT_TRANSPARENT_GIBS ) )
		cFlag |= BREAK_TRANS;

	// SBO: "At position" is where the gibs come out. Featureful added the brush's center to it, which for a brush
	// without an ORIGIN brush is its world position: the gibs landed far from the position.
	Vector vecSpot = FStringNull( m_position ) ? vecOrigin + ( pev->mins + pev->maxs ) * 0.5 : vecOrigin;
	if ( m_iGibs >= 0 && m_idShard )
		CBreakable::BreakModel( vecSpot, pev->size, g_vecZero, m_idShard, m_iGibs, cFlag );

	if ( !FBitSet( pev->spawnflags, SF_BREAKEFFECT_REPEATABLE ) )
		UTIL_Remove( this );
}
