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

===== util.cpp ========================================================

  Utility code.  Really not optional after all.

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include <time.h>
#include "shake.h"
#include "decals.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "render_fade.h"	// SBO 2.0: RenderFade_ScaleNow (UTIL_ModelScale)

float UTIL_WeaponTimeBase( void )
{
#if defined( CLIENT_WEAPONS )
	return 0.0;
#else
	return gpGlobals->time;
#endif
}

static unsigned int glSeed = 0; 

unsigned int seed_table[ 256 ] =
{
	28985, 27138, 26457, 9451, 17764, 10909, 28790, 8716, 6361, 4853, 17798, 21977, 19643, 20662, 10834, 20103,
	27067, 28634, 18623, 25849, 8576, 26234, 23887, 18228, 32587, 4836, 3306, 1811, 3035, 24559, 18399, 315,
	26766, 907, 24102, 12370, 9674, 2972, 10472, 16492, 22683, 11529, 27968, 30406, 13213, 2319, 23620, 16823,
	10013, 23772, 21567, 1251, 19579, 20313, 18241, 30130, 8402, 20807, 27354, 7169, 21211, 17293, 5410, 19223,
	10255, 22480, 27388, 9946, 15628, 24389, 17308, 2370, 9530, 31683, 25927, 23567, 11694, 26397, 32602, 15031,
	18255, 17582, 1422, 28835, 23607, 12597, 20602, 10138, 5212, 1252, 10074, 23166, 19823, 31667, 5902, 24630,
	18948, 14330, 14950, 8939, 23540, 21311, 22428, 22391, 3583, 29004, 30498, 18714, 4278, 2437, 22430, 3439,
	28313, 23161, 25396, 13471, 19324, 15287, 2563, 18901, 13103, 16867, 9714, 14322, 15197, 26889, 19372, 26241,
	31925, 14640, 11497, 8941, 10056, 6451, 28656, 10737, 13874, 17356, 8281, 25937, 1661, 4850, 7448, 12744,
	21826, 5477, 10167, 16705, 26897, 8839, 30947, 27978, 27283, 24685, 32298, 3525, 12398, 28726, 9475, 10208,
	617, 13467, 22287, 2376, 6097, 26312, 2974, 9114, 21787, 28010, 4725, 15387, 3274, 10762, 31695, 17320,
	18324, 12441, 16801, 27376, 22464, 7500, 5666, 18144, 15314, 31914, 31627, 6495, 5226, 31203, 2331, 4668,
	12650, 18275, 351, 7268, 31319, 30119, 7600, 2905, 13826, 11343, 13053, 15583, 30055, 31093, 5067, 761,
	9685, 11070, 21369, 27155, 3663, 26542, 20169, 12161, 15411, 30401, 7580, 31784, 8985, 29367, 20989, 14203,
	29694, 21167, 10337, 1706, 28578, 887, 3373, 19477, 14382, 675, 7033, 15111, 26138, 12252, 30996, 21409,
	25678, 18555, 13256, 23316, 22407, 16727, 991, 9236, 5373, 29402, 6117, 15241, 27715, 19291, 19888, 19847
};

unsigned int U_Random( void ) 
{ 
	glSeed *= 69069; 
	glSeed += seed_table[ glSeed & 0xff ];
 
	return ( ++glSeed & 0x0fffffff ); 
} 

void U_Srand( unsigned int seed )
{
	glSeed = seed_table[ seed & 0xff ];
}

/*
=====================
UTIL_SharedRandomLong
=====================
*/
int UTIL_SharedRandomLong( unsigned int seed, int low, int high )
{
	unsigned int range;

	U_Srand( (int)seed + low + high );

	range = high - low + 1;
	if ( !(range - 1) )
	{
		return low;
	}
	else
	{
		int offset;
		int rnum;

		rnum = U_Random();

		offset = rnum % range;

		return (low + offset);
	}
}

/*
=====================
UTIL_SharedRandomFloat
=====================
*/
float UTIL_SharedRandomFloat( unsigned int seed, float low, float high )
{
	//
	unsigned int range;

	U_Srand( (int)seed + *(int *)&low + *(int *)&high );

	U_Random();
	U_Random();

	range = high - low;
	if ( !range )
	{
		return low;
	}
	else
	{
		int tensixrand;
		float offset;

		tensixrand = U_Random() & 65535;

		offset = (float)tensixrand / 65536.0;

		return (low + offset * range );
	}
}

void UTIL_ParametricRocket( entvars_t *pev, Vector vecOrigin, Vector vecAngles, edict_t *owner )
{	
	pev->startpos = vecOrigin;
	// Trace out line to end pos
	TraceResult tr;
	UTIL_MakeVectors( vecAngles );
	UTIL_TraceLine( pev->startpos, pev->startpos + gpGlobals->v_forward * 8192, ignore_monsters, owner, &tr);
	pev->endpos = tr.vecEndPos;

	// Now compute how long it will take based on current velocity
	Vector vecTravel = pev->endpos - pev->startpos;
	float travelTime = 0.0;
	if ( pev->velocity.Length() > 0 )
	{
		travelTime = vecTravel.Length() / pev->velocity.Length();
	}
	pev->starttime = gpGlobals->time;
	pev->impacttime = gpGlobals->time + travelTime;
}

int g_groupmask = 0;
int g_groupop = 0;

// Normal overrides
void UTIL_SetGroupTrace( int groupmask, int op )
{
	g_groupmask		= groupmask;
	g_groupop		= op;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

void UTIL_UnsetGroupTrace( void )
{
	g_groupmask		= 0;
	g_groupop		= 0;

	ENGINE_SETGROUPMASK( 0, 0 );
}

// Smart version, it'll clean itself up when it pops off stack
UTIL_GroupTrace::UTIL_GroupTrace( int groupmask, int op )
{
	m_oldgroupmask	= g_groupmask;
	m_oldgroupop	= g_groupop;

	g_groupmask		= groupmask;
	g_groupop		= op;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

UTIL_GroupTrace::~UTIL_GroupTrace( void )
{
	g_groupmask		=	m_oldgroupmask;
	g_groupop		=	m_oldgroupop;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

TYPEDESCRIPTION	gEntvarsDescription[] = 
{
	DEFINE_ENTITY_FIELD( classname, FIELD_STRING ),
	DEFINE_ENTITY_GLOBAL_FIELD( globalname, FIELD_STRING ),
	
	DEFINE_ENTITY_FIELD( origin, FIELD_POSITION_VECTOR ),
	DEFINE_ENTITY_FIELD( oldorigin, FIELD_POSITION_VECTOR ),
	DEFINE_ENTITY_FIELD( velocity, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( basevelocity, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( movedir, FIELD_VECTOR ),

	DEFINE_ENTITY_FIELD( angles, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( avelocity, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( punchangle, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( v_angle, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( fixangle, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( idealpitch, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( pitch_speed, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( ideal_yaw, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( yaw_speed, FIELD_FLOAT ),

	DEFINE_ENTITY_FIELD( modelindex, FIELD_INTEGER ),
	DEFINE_ENTITY_GLOBAL_FIELD( model, FIELD_MODELNAME ),

	DEFINE_ENTITY_FIELD( viewmodel, FIELD_MODELNAME ),
	DEFINE_ENTITY_FIELD( weaponmodel, FIELD_MODELNAME ),

	DEFINE_ENTITY_FIELD( absmin, FIELD_POSITION_VECTOR ),
	DEFINE_ENTITY_FIELD( absmax, FIELD_POSITION_VECTOR ),
	DEFINE_ENTITY_GLOBAL_FIELD( mins, FIELD_VECTOR ),
	DEFINE_ENTITY_GLOBAL_FIELD( maxs, FIELD_VECTOR ),
	DEFINE_ENTITY_GLOBAL_FIELD( size, FIELD_VECTOR ),

	DEFINE_ENTITY_FIELD( ltime, FIELD_TIME ),
	DEFINE_ENTITY_FIELD( nextthink, FIELD_TIME ),

	DEFINE_ENTITY_FIELD( solid, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( movetype, FIELD_INTEGER ),

	DEFINE_ENTITY_FIELD( skin, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( body, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( effects, FIELD_INTEGER ),

	DEFINE_ENTITY_FIELD( gravity, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( friction, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( light_level, FIELD_FLOAT ),

	DEFINE_ENTITY_FIELD( frame, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( scale, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( sequence, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( animtime, FIELD_TIME ),
	DEFINE_ENTITY_FIELD( framerate, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( controller, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( blending, FIELD_INTEGER ),

	DEFINE_ENTITY_FIELD( rendermode, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( renderamt, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( rendercolor, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( renderfx, FIELD_INTEGER ),

	DEFINE_ENTITY_FIELD( health, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( frags, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( weapons, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( takedamage, FIELD_FLOAT ),

	DEFINE_ENTITY_FIELD( deadflag, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( view_ofs, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( button, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( impulse, FIELD_INTEGER ),

	DEFINE_ENTITY_FIELD( chain, FIELD_EDICT ),
	DEFINE_ENTITY_FIELD( dmg_inflictor, FIELD_EDICT ),
	DEFINE_ENTITY_FIELD( enemy, FIELD_EDICT ),
	DEFINE_ENTITY_FIELD( aiment, FIELD_EDICT ),
	DEFINE_ENTITY_FIELD( owner, FIELD_EDICT ),
	DEFINE_ENTITY_FIELD( groundentity, FIELD_EDICT ),

	DEFINE_ENTITY_FIELD( spawnflags, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( flags, FIELD_FLOAT ),

	DEFINE_ENTITY_FIELD( colormap, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( team, FIELD_INTEGER ),

	DEFINE_ENTITY_FIELD( max_health, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( teleport_time, FIELD_TIME ),
	DEFINE_ENTITY_FIELD( armortype, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( armorvalue, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( waterlevel, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( watertype, FIELD_INTEGER ),

	// Having these fields be local to the individual levels makes it easier to test those levels individually.
	DEFINE_ENTITY_GLOBAL_FIELD( target, FIELD_STRING ),
	DEFINE_ENTITY_GLOBAL_FIELD( targetname, FIELD_STRING ),
	DEFINE_ENTITY_FIELD( netname, FIELD_STRING ),
	DEFINE_ENTITY_FIELD( message, FIELD_STRING ),

	DEFINE_ENTITY_FIELD( dmg_take, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( dmg_save, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( dmg, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( dmgtime, FIELD_TIME ),

	DEFINE_ENTITY_FIELD( noise, FIELD_SOUNDNAME ),
	DEFINE_ENTITY_FIELD( noise1, FIELD_SOUNDNAME ),
	DEFINE_ENTITY_FIELD( noise2, FIELD_SOUNDNAME ),
	DEFINE_ENTITY_FIELD( noise3, FIELD_SOUNDNAME ),
	DEFINE_ENTITY_FIELD( speed, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( air_finished, FIELD_TIME ),
	DEFINE_ENTITY_FIELD( pain_finished, FIELD_TIME ),
	DEFINE_ENTITY_FIELD( radsuit_finished, FIELD_TIME ),
};

#define ENTVARS_COUNT		(sizeof(gEntvarsDescription)/sizeof(gEntvarsDescription[0]))


#ifdef	DEBUG
edict_t *DBG_EntOfVars( const entvars_t *pev )
{
	if (pev->pContainingEntity != NULL)
		return pev->pContainingEntity;
	ALERT(at_console, "entvars_t pContainingEntity is NULL, calling into engine");
	edict_t* pent = (*g_engfuncs.pfnFindEntityByVars)((entvars_t*)pev);
	if (pent == NULL)
		ALERT(at_console, "DAMN!  Even the engine couldn't FindEntityByVars!");
	((entvars_t *)pev)->pContainingEntity = pent;
	return pent;
}
#endif //DEBUG


#ifdef	DEBUG
	void
DBG_AssertFunction(
	BOOL		fExpr,
	const char*	szExpr,
	const char*	szFile,
	int			szLine,
	const char*	szMessage)
	{
	if (fExpr)
		return;
	char szOut[512];
	if (szMessage != NULL)
		sprintf(szOut, "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage);
	else
		sprintf(szOut, "ASSERT FAILED:\n %s \n(%s@%d)", szExpr, szFile, szLine);
	ALERT(at_console, szOut);
	}
#endif	// DEBUG

BOOL UTIL_GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon )
{
	return g_pGameRules->GetNextBestWeapon( pPlayer, pCurrentWeapon );
}

// ripped this out of the engine
float	UTIL_AngleMod(float a)
{
	if (a < 0)
	{
		a = a + 360 * ((int)(a / 360) + 1);
	}
	else if (a >= 360)
	{
		a = a - 360 * ((int)(a / 360));
	}
	// a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}

float UTIL_AngleDiff( float destAngle, float srcAngle )
{
	float delta;

	delta = destAngle - srcAngle;
	if ( destAngle > srcAngle )
	{
		if ( delta >= 180 )
			delta -= 360;
	}
	else
	{
		if ( delta <= -180 )
			delta += 360;
	}
	return delta;
}

Vector UTIL_VecToAngles( const Vector &vec )
{
	float rgflVecOut[3];
	VEC_TO_ANGLES(vec, rgflVecOut);
	return Vector(rgflVecOut);
}
	
// UTIL_MoveToOrigin: with UTIL_SetSize (model scale)


int UTIL_EntitiesInBox( CBaseEntity **pList, int listMax, const Vector &mins, const Vector &maxs, int flagMask )
{
	// halflife-updated (halflife #3307): walk the edict list from the world edict. Index 1 is a client
	// slot and PEntityOfEntIndex(1) returns NULL when it is not in use (e.g. empty server with maxplayers 1).
	edict_t		*pEdict = g_engfuncs.pfnPEntityOfEntOffset( 0 );
	CBaseEntity *pEntity;
	int			count;

	count = 0;

	if ( !pEdict )
		return count;

	++pEdict;

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )	// Not in use
			continue;
		
		if ( flagMask && !(pEdict->v.flags & flagMask) )	// Does it meet the criteria?
			continue;

		if ( mins.x > pEdict->v.absmax.x ||
			 mins.y > pEdict->v.absmax.y ||
			 mins.z > pEdict->v.absmax.z ||
			 maxs.x < pEdict->v.absmin.x ||
			 maxs.y < pEdict->v.absmin.y ||
			 maxs.z < pEdict->v.absmin.z )
			 continue;

		pEntity = CBaseEntity::Instance(pEdict);
		if ( !pEntity )
			continue;

		pList[ count ] = pEntity;
		count++;

		if ( count >= listMax )
			return count;
	}

	return count;
}


int UTIL_MonstersInSphere( CBaseEntity **pList, int listMax, const Vector &center, float radius )
{
	// halflife-updated (halflife #3307): walk the edict list from the world edict. Index 1 is a client
	// slot and PEntityOfEntIndex(1) returns NULL when it is not in use (e.g. empty server with maxplayers 1).
	edict_t		*pEdict = g_engfuncs.pfnPEntityOfEntOffset( 0 );
	CBaseEntity *pEntity;
	int			count;
	float		distance, delta;

	count = 0;
	float radiusSquared = radius * radius;

	if ( !pEdict )
		return count;

	++pEdict;

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )	// Not in use
			continue;
		
		if ( !(pEdict->v.flags & (FL_CLIENT|FL_MONSTER)) )	// Not a client/monster ?
			continue;

		// Use origin for X & Y since they are centered for all monsters
		// Now X
		delta = center.x - pEdict->v.origin.x;//(pEdict->v.absmin.x + pEdict->v.absmax.x)*0.5;
		delta *= delta;

		if ( delta > radiusSquared )
			continue;
		distance = delta;
		
		// Now Y
		delta = center.y - pEdict->v.origin.y;//(pEdict->v.absmin.y + pEdict->v.absmax.y)*0.5;
		delta *= delta;

		distance += delta;
		if ( distance > radiusSquared )
			continue;

		// Now Z
		delta = center.z - (pEdict->v.absmin.z + pEdict->v.absmax.z)*0.5;
		delta *= delta;

		distance += delta;
		if ( distance > radiusSquared )
			continue;

		pEntity = CBaseEntity::Instance(pEdict);
		if ( !pEntity )
			continue;

		pList[ count ] = pEntity;
		count++;

		if ( count >= listMax )
			return count;
	}


	return count;
}


CBaseEntity *UTIL_FindEntityInSphere( CBaseEntity *pStartEntity, const Vector &vecCenter, float flRadius )
{
	edict_t	*pentEntity;

	if (pStartEntity)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = NULL;

	pentEntity = FIND_ENTITY_IN_SPHERE( pentEntity, vecCenter, flRadius);

	if (!FNullEnt(pentEntity))
		return CBaseEntity::Instance(pentEntity);
	return NULL;
}


CBaseEntity *UTIL_FindEntityByString( CBaseEntity *pStartEntity, const char *szKeyword, const char *szValue )
{
	edict_t	*pentEntity;

	if (pStartEntity)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = NULL;

	pentEntity = FIND_ENTITY_BY_STRING( pentEntity, szKeyword, szValue );

	if (!FNullEnt(pentEntity))
		return CBaseEntity::Instance(pentEntity);
	return NULL;
}

CBaseEntity *UTIL_FindEntityByClassname( CBaseEntity *pStartEntity, const char *szName )
{
	return UTIL_FindEntityByString( pStartEntity, "classname", szName );
}

CBaseEntity *UTIL_FindEntityByTargetname( CBaseEntity *pStartEntity, const char *szName )
{
	return UTIL_FindEntityByString( pStartEntity, "targetname", szName );
}


CBaseEntity *UTIL_FindEntityGeneric( const char *szWhatever, Vector &vecSrc, float flRadius )
{
	CBaseEntity *pEntity = NULL;

	pEntity = UTIL_FindEntityByTargetname( NULL, szWhatever );
	if (pEntity)
		return pEntity;

	CBaseEntity *pSearch = NULL;
	float flMaxDist2 = flRadius * flRadius;
	while ((pSearch = UTIL_FindEntityByClassname( pSearch, szWhatever )) != NULL)
	{
		float flDist2 = (pSearch->pev->origin - vecSrc).Length();
		flDist2 = flDist2 * flDist2;
		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}
	return pEntity;
}


// returns a CBaseEntity pointer to a player by index.  Only returns if the player is spawned and connected
// otherwise returns NULL
// Index is 1 based
CBaseEntity	*UTIL_PlayerByIndex( int playerIndex )
{
	CBaseEntity *pPlayer = NULL;

	if ( playerIndex > 0 && playerIndex <= gpGlobals->maxClients )
	{
		edict_t *pPlayerEdict = INDEXENT( playerIndex );
		if ( pPlayerEdict && !pPlayerEdict->free )
		{
			pPlayer = CBaseEntity::Instance( pPlayerEdict );
		}
	}
	
	return pPlayer;
}


void UTIL_MakeVectors( const Vector &vecAngles )
{
	MAKE_VECTORS( vecAngles );
}


void UTIL_MakeAimVectors( const Vector &vecAngles )
{
	float rgflVec[3];
	vecAngles.CopyToArray(rgflVec);
	rgflVec[0] = -rgflVec[0];
	MAKE_VECTORS(rgflVec);
}


#define SWAP(a,b,temp)	((temp)=(a),(a)=(b),(b)=(temp))

void UTIL_MakeInvVectors( const Vector &vec, globalvars_t *pgv )
{
	MAKE_VECTORS(vec);

	float tmp;
	pgv->v_right = pgv->v_right * -1;

	SWAP(pgv->v_forward.y, pgv->v_right.x, tmp);
	SWAP(pgv->v_forward.z, pgv->v_up.x, tmp);
	SWAP(pgv->v_right.z, pgv->v_up.y, tmp);
}


void UTIL_EmitAmbientSound( edict_t *entity, const Vector &vecOrigin, const char *samp, float vol, float attenuation, int fFlags, int pitch )
{
	float rgfl[3];
	vecOrigin.CopyToArray(rgfl);

	if (samp && *samp == '!')
	{
		char name[32];
		if (SENTENCEG_Lookup(samp, name) >= 0)
			EMIT_AMBIENT_SOUND(entity, rgfl, name, vol, attenuation, fFlags, pitch);
	}
	else
		EMIT_AMBIENT_SOUND(entity, rgfl, samp, vol, attenuation, fFlags, pitch);
}

static unsigned short FixedUnsigned16( float value, float scale )
{
	int output;

	output = value * scale;
	if ( output < 0 )
		output = 0;
	if ( output > 0xFFFF )
		output = 0xFFFF;

	return (unsigned short)output;
}

static short FixedSigned16( float value, float scale )
{
	int output;

	output = value * scale;

	if ( output > 32767 )
		output = 32767;

	if ( output < -32768 )
		output = -32768;

	return (short)output;
}

// Shake the screen of all clients within radius
// radius == 0, shake all clients
// UNDONE: Allow caller to shake clients not ONGROUND?
// UNDONE: Fix falloff model (disabled)?
// UNDONE: Affect user controls?
void UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius )
{
	int			i;
	float		localAmplitude;
	ScreenShake	shake;

	shake.duration = FixedUnsigned16( duration, 1<<12 );		// 4.12 fixed
	shake.frequency = FixedUnsigned16( frequency, 1<<8 );	// 8.8 fixed

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer || !(pPlayer->pev->flags & FL_ONGROUND) )	// Don't shake if not onground
			continue;

		localAmplitude = 0;

		if ( radius <= 0 )
			localAmplitude = amplitude;
		else
		{
			Vector delta = center - pPlayer->pev->origin;
			float distance = delta.Length();
	
			// Had to get rid of this falloff - it didn't work well
			if ( distance < radius )
				localAmplitude = amplitude;//radius - distance;
		}
		if ( localAmplitude )
		{
			shake.amplitude = FixedUnsigned16( localAmplitude, 1<<12 );		// 4.12 fixed
			
			MESSAGE_BEGIN( MSG_ONE, gmsgShake, NULL, pPlayer->edict() );		// use the magic #1 for "one client"
				
				WRITE_SHORT( shake.amplitude );				// shake amount
				WRITE_SHORT( shake.duration );				// shake lasts this long
				WRITE_SHORT( shake.frequency );				// shake noise frequency

			MESSAGE_END();
		}
	}
}



void UTIL_ScreenShakeAll( const Vector &center, float amplitude, float frequency, float duration )
{
	UTIL_ScreenShake( center, amplitude, frequency, duration, 0 );
}


void UTIL_ScreenFadeBuild( ScreenFade &fade, const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	fade.duration = FixedUnsigned16( fadeTime, 1<<12 );		// 4.12 fixed
	fade.holdTime = FixedUnsigned16( fadeHold, 1<<12 );		// 4.12 fixed
	fade.r = (int)color.x;
	fade.g = (int)color.y;
	fade.b = (int)color.z;
	fade.a = alpha;
	fade.fadeFlags = flags;
}


void UTIL_ScreenFadeWrite( const ScreenFade &fade, CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgFade, NULL, pEntity->edict() );		// use the magic #1 for "one client"
		
		WRITE_SHORT( fade.duration );		// fade lasts this long
		WRITE_SHORT( fade.holdTime );		// fade lasts this long
		WRITE_SHORT( fade.fadeFlags );		// fade type (in / out)
		WRITE_BYTE( fade.r );				// fade red
		WRITE_BYTE( fade.g );				// fade green
		WRITE_BYTE( fade.b );				// fade blue
		WRITE_BYTE( fade.a );				// fade blue

	MESSAGE_END();

	if ( pEntity->IsPlayer() )
		((CBasePlayer *)pEntity)->ScreenFadeSent( (fade.duration + fade.holdTime) / (float)(1<<12), fade.fadeFlags );
}


void UTIL_ScreenFadeAll( const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	int			i;
	ScreenFade	fade;


	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, alpha, flags );

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
	
		UTIL_ScreenFadeWrite( fade, pPlayer );
	}
}


void UTIL_ScreenFade( CBaseEntity *pEntity, const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	ScreenFade	fade;

	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, alpha, flags );
	UTIL_ScreenFadeWrite( fade, pEntity );
}


void UTIL_HudMessage( CBaseEntity *pEntity, const hudtextparms_t &textparms, const char *pMessage )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, NULL, pEntity->edict() );
		WRITE_BYTE( TE_TEXTMESSAGE );
		WRITE_BYTE( textparms.channel & 0xFF );

		WRITE_SHORT( FixedSigned16( textparms.x, 1<<13 ) );
		WRITE_SHORT( FixedSigned16( textparms.y, 1<<13 ) );
		WRITE_BYTE( textparms.effect );

		WRITE_BYTE( textparms.r1 );
		WRITE_BYTE( textparms.g1 );
		WRITE_BYTE( textparms.b1 );
		WRITE_BYTE( textparms.a1 );

		WRITE_BYTE( textparms.r2 );
		WRITE_BYTE( textparms.g2 );
		WRITE_BYTE( textparms.b2 );
		WRITE_BYTE( textparms.a2 );

		WRITE_SHORT( FixedUnsigned16( textparms.fadeinTime, 1<<8 ) );
		WRITE_SHORT( FixedUnsigned16( textparms.fadeoutTime, 1<<8 ) );
		WRITE_SHORT( FixedUnsigned16( textparms.holdTime, 1<<8 ) );

		if ( textparms.effect == 2 )
			WRITE_SHORT( FixedUnsigned16( textparms.fxTime, 1<<8 ) );
		
		if ( strlen( pMessage ) < 512 )
		{
			WRITE_STRING( pMessage );
		}
		else
		{
			char tmp[512];
			strncpy( tmp, pMessage, 511 );
			tmp[511] = 0;
			WRITE_STRING( tmp );
		}
	MESSAGE_END();
}

void UTIL_HudMessageAll( const hudtextparms_t &textparms, const char *pMessage )
{
	int			i;

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
			UTIL_HudMessage( pPlayer, textparms, pMessage );
	}
}

					 
extern int gmsgTextMsg, gmsgSayText;
void UTIL_ClientPrintAll( int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgTextMsg );
		WRITE_BYTE( msg_dest );
		WRITE_STRING( msg_name );

		if ( param1 )
			WRITE_STRING( param1 );
		if ( param2 )
			WRITE_STRING( param2 );
		if ( param3 )
			WRITE_STRING( param3 );
		if ( param4 )
			WRITE_STRING( param4 );

	MESSAGE_END();
}

void ClientPrint( entvars_t *client, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgTextMsg, NULL, client );
		WRITE_BYTE( msg_dest );
		WRITE_STRING( msg_name );

		if ( param1 )
			WRITE_STRING( param1 );
		if ( param2 )
			WRITE_STRING( param2 );
		if ( param3 )
			WRITE_STRING( param3 );
		if ( param4 )
			WRITE_STRING( param4 );

	MESSAGE_END();
}

void UTIL_SayText( const char *pText, CBaseEntity *pEntity )
{
	if ( !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, pEntity->edict() );
		WRITE_BYTE( pEntity->entindex() );
		WRITE_STRING( pText );
	MESSAGE_END();
}

void UTIL_SayTextAll( const char *pText, CBaseEntity *pEntity )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
		WRITE_BYTE( pEntity->entindex() );
		WRITE_STRING( pText );
	MESSAGE_END();
}


//=========================================================
// UTIL_ColorCodeLength - length of the chat colour code at
// psz: 2 for ^0..^9, 5 for ^xRGB (x or X and three hex
// digits, each a 4-bit channel like CSS #RGB), 8 for
// ^#RRGGBB (six hex digits), 0 if there is none (e.g.
// "^xZZ", "^#44AAF": literal text). Same rule as the
// client's GetColorCodeLength (cl_dll/vgui2/hud_chat.h).
//=========================================================
static BOOL IsColorCodeHexDigit( char c )
{
	return ( c >= '0' && c <= '9' ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' );
}

int UTIL_ColorCodeLength( const char *psz )
{
	if ( !psz || psz[0] != '^' )
		return 0;

	if ( psz[1] >= '0' && psz[1] <= '9' )
		return 2;

	if ( ( psz[1] == 'x' || psz[1] == 'X' )
		&& IsColorCodeHexDigit( psz[2] ) && IsColorCodeHexDigit( psz[3] ) && IsColorCodeHexDigit( psz[4] ) )
		return 5;

	if ( psz[1] == '#' )
	{
		for ( int i = 2; i < 8; i++ )
		{
			if ( !IsColorCodeHexDigit( psz[i] ) )
				return 0;
		}
		return 8;
	}

	return 0;
}


//=========================================================
// UTIL_NameColorCode - the chat colour code of a player's
// name colour: the "namecolor" key of his userinfo (client
// cvar namecolor, FCVAR_USERINFO), read on demand like
// cl_autowepswitch. Values as the client's ParseChatColor
// (cl_dll/vgui2/hud_chat.cpp): "1".."8" = ^1..^8, "xRGB"
// (three hex digits) = ^xRGB, "xRRGGBB" (six) = ^#RRGGBB,
// or ^xRGB when that is the same colour ('#' for 'x' and a
// leading '^' are read too), "0" and anything invalid =
// none. pszCode (10 chars) gets "^N", "^xRGB", "^#RRGGBB"
// or "". Returns its length.
//=========================================================
int UTIL_NameColorCode( edict_t *pPlayer, char *pszCode )
{
	pszCode[0] = 0;

	if ( !pPlayer )
		return 0;

	char *pszInfo = g_engfuncs.pfnGetInfoKeyBuffer( pPlayer );
	const char *pszValue = pszInfo ? g_engfuncs.pfnInfoKeyValue( pszInfo, "namecolor" ) : NULL;
	if ( !pszValue )
		return 0;

	while ( *pszValue == ' ' )
		pszValue++;

	const char *pszHex = pszValue;
	if ( pszHex[0] == '^' )
		pszHex++;
	if ( pszHex[0] == 'x' || pszHex[0] == 'X' || pszHex[0] == '#' )
	{
		pszHex++;

		int iDigits = 0;
		while ( iDigits <= 6 && IsColorCodeHexDigit( pszHex[iDigits] ) )
			iDigits++;

		if ( ( iDigits != 3 && iDigits != 6 ) || ( pszHex[iDigits] != 0 && pszHex[iDigits] != ' ' ) )
			return 0;

		// six digits with both digits of every channel the same: the compact code is the same colour
		if ( iDigits == 6 && toupper( pszHex[0] ) == toupper( pszHex[1] ) && toupper( pszHex[2] ) == toupper( pszHex[3] )
			&& toupper( pszHex[4] ) == toupper( pszHex[5] ) )
		{
			sprintf( pszCode, "^x%c%c%c", pszHex[0], pszHex[2], pszHex[4] );
			return 5;
		}

		sprintf( pszCode, iDigits == 3 ? "^x%.3s" : "^#%.6s", pszHex );
		return iDigits == 3 ? 5 : 8;
	}

	int iCode = (int)atof( pszValue );
	if ( iCode < 1 || iCode > 8 )
		return 0;

	sprintf( pszCode, "^%d", iCode );
	return 2;
}


//=========================================================
// UTIL_ChatPlayerName - a player name to embed in a chat
// line (SayText) the server builds around it, e.g.
// "(RADIO) <name> : Cover Me !". The chat's colour codes
// (hud_colortext; BHL's ^1..^8 and SBO's ^xRGB and
// ^#RRGGBB, see UTIL_ColorCodeLength) stay active until the next code,
// so a name like "^6Shades" or "^xF80Shades" would colour
// the rest of the line too. When the name has a code, it is
// ended with ^0, which restores the line's normal colour;
// names without codes are returned as they are.
// pPlayer (may be NULL): the name's player. His name colour
// (UTIL_NameColorCode) goes in front of the name as a code
// and replaces each ^0 / ^9 in the name: it is the colour of
// the name's uncoded text, codes typed in the name apply
// from where they are and a reset in the name goes back to
// the name colour -- the client's rule for the names it
// colours itself (ApplyNameColorCodes, hud_chat.h: say /
// say_team lines, scoreboard, kill feed, voice labels).
// Four rotating buffers, so a format can take two names.
//=========================================================
const char *UTIL_ChatPlayerName( const char *pszName, edict_t *pPlayer )
{
	static char s_szNames[4][64];
	static int s_iNext = 0;

	if ( !pszName )
		return "";

	char szNameColor[12];
	int iNameColorLen = UTIL_NameColorCode( pPlayer, szNameColor );

	BOOL bColorCode = iNameColorLen > 0;
	for ( const char *p = pszName; *p && !bColorCode; p++ )
	{
		if ( UTIL_ColorCodeLength( p ) > 0 )
			bColorCode = TRUE;
	}

	if ( !bColorCode )
		return pszName;

	char *pszBuf = s_szNames[s_iNext];
	s_iNext = ( s_iNext + 1 ) % 4;

	// room for the closing ^0 and the terminator (engine names are at most 31 bytes, 39 with the longest name
	// colour code, ^#RRGGBB: what doesn't fit is a name with several resets, each replaced by that code; it is cut
	// between characters / whole codes)
	const int iMax = sizeof( s_szNames[0] ) - 3;
	int iLen = 0;

	memcpy( pszBuf, szNameColor, iNameColorLen );
	iLen += iNameColorLen;

	for ( const char *p = pszName; *p; )
	{
		int iCodeLen = UTIL_ColorCodeLength( p );
		BOOL bReset = iCodeLen == 2 && ( p[1] == '0' || p[1] == '9' );
		if ( iCodeLen == 0 )
			iCodeLen = 1;

		if ( bReset && iNameColorLen > 0 && iLen + iNameColorLen <= iMax )
		{
			memcpy( pszBuf + iLen, szNameColor, iNameColorLen );
			iLen += iNameColorLen;
		}
		else
		{
			if ( iLen + iCodeLen > iMax )
				break;
			memcpy( pszBuf + iLen, p, iCodeLen );
			iLen += iCodeLen;
		}
		p += iCodeLen;
	}

	strcpy( pszBuf + iLen, "^0" );
	return pszBuf;
}

char *UTIL_dtos1( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos2( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos3( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos4( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

void UTIL_ShowMessage( const char *pString, CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgHudText, NULL, pEntity->edict() );
	WRITE_STRING( pString );
	MESSAGE_END();
}


void UTIL_ShowMessageAll( const char *pString )
{
	int		i;

	// loop through all players

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
			UTIL_ShowMessage( pString, pPlayer );
	}
}

// SBO 2.0 model scale: a trace that ends on a scaled entity's hitbox has a plane normal of length 1 / scale (the
// hitbox planes are made from the scaled bones, animation.cpp SV_StudioSetupBonesScaled): length 1 again
static void UTIL_UnscaleHitboxNormal( TraceResult *ptr )
{
	if ( ptr->flFraction >= 1.0f || !ptr->pHit || ptr->pHit->v.scale <= 0.0f || ptr->pHit->v.scale == 1.0f )
		return;

	float flLength = ptr->vecPlaneNormal.Length( );
	if ( flLength < 0.0001f || fabs( flLength - 1.0f ) < 0.0001f )
		return;

	ptr->vecPlaneNormal = ptr->vecPlaneNormal / flLength;
	ptr->flPlaneDist /= flLength;
}

// Overloaded to add IGNORE_GLASS
void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE) | (ignoreGlass?0x100:0), pentIgnore, ptr );
	UTIL_UnscaleHitboxNormal( ptr );
}


void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE), pentIgnore, ptr );
	UTIL_UnscaleHitboxNormal( ptr );
}


void UTIL_TraceHull( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_HULL( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE), hullNumber, pentIgnore, ptr );
	UTIL_UnscaleHitboxNormal( ptr );
}

void UTIL_TraceModel( const Vector &vecStart, const Vector &vecEnd, int hullNumber, edict_t *pentModel, TraceResult *ptr )
{
	g_engfuncs.pfnTraceModel( vecStart, vecEnd, hullNumber, pentModel, ptr );
}


TraceResult UTIL_GetGlobalTrace( )
{
	TraceResult tr;

	tr.fAllSolid		= gpGlobals->trace_allsolid;
	tr.fStartSolid		= gpGlobals->trace_startsolid;
	tr.fInOpen			= gpGlobals->trace_inopen;
	tr.fInWater			= gpGlobals->trace_inwater;
	tr.flFraction		= gpGlobals->trace_fraction;
	tr.flPlaneDist		= gpGlobals->trace_plane_dist;
	tr.pHit			= gpGlobals->trace_ent;
	tr.vecEndPos		= gpGlobals->trace_endpos;
	tr.vecPlaneNormal	= gpGlobals->trace_plane_normal;
	tr.iHitgroup		= gpGlobals->trace_hitgroup;
	return tr;
}

	
/*
SBO 2.0 model scale: the collision box (docs/MODEL_SCALE.md "Collision box and hitboxes")

The `scale` key draws a studio model scaled around its origin (client StudioSetUpTransform) and scales its hitboxes
(Server_GetBlendingInterface, animation.cpp). UTIL_SetSize scales the box the same way, so players stop at it and
bullets reach the scaled hitboxes (the engine tests a hitbox only when the trace touches the entity's absmin /
absmax). It keeps the box at scale 1 in the entity (m_vecSizeMins / Maxs) so later resizes (death, scripts) and a
scale changed while the map runs (trigger_changevalue) start from it.

Moving through the world stays on the engine's hulls (point, human, large, crouch; compiled into the map): the
engine picks one by the moving box's size. A scaled box would pick another hull (a 3 x grunt the large one, off its
centre), get stuck where the map was made for the monster's own size, and use other node graph links. So the
engine's monster movement (UTIL_WalkMove, UTIL_MoveToOrigin, UTIL_DropToFloor, UTIL_TraceMonsterHull) gets the box
at scale 1 for the call, as an unscaled monster has it; CGraph::HullIndex reads it too. A scaled-up monster is in
addition stopped by players and monsters at its scaled box (the engine's move only sees the box at scale 1), so it
does not walk into them.
*/

// the scaled box keeps at least this size on every axis it has a size on (a model scaled down to almost nothing
// can still be touched and shot)
#define MODEL_SCALE_MIN_BOX		1.0f

static BOOL UTIL_IsStudioModelName( const entvars_t *pev )
{
	if ( FStringNull( pev->model ) )
		return FALSE;

	const char *pszModel = STRING( pev->model );
	size_t len = strlen( pszModel );

	return len > 4 && !stricmp( pszModel + len - 4, ".mdl" );
}

// The classes whose collision box, hitboxes and server attachments follow the scale (docs/MODEL_SCALE.md):
// monsters (with the dead monsters, cycler, monster_furniture, monster_generic: all CBaseMonster), the pick-up items
// (item_*) and ammo_reserve. Everything else keeps its box and its hitboxes exactly as before, whatever its scale
// key means there (a sprite's size, a beam's width; brush entities and triggers carry it in some custom maps): the
// client only draws it scaled. Players never (the player movement's hulls are fixed)
static BOOL UTIL_ScalesWithModel( const entvars_t *pev )
{
	edict_t *pent = pev->pContainingEntity;
	if ( !pent || FBitSet( pev->flags, FL_CLIENT ) )
		return FALSE;

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( pent );
	if ( !pEntity )
		return FALSE;

	if ( pEntity->MyMonsterPointer( ) )
		return TRUE;

	const char *pszClass = STRING( pev->classname );
	return !strncmp( pszClass, "item_", 5 ) || !strcmp( pszClass, "ammo_reserve" );
}

// the scale the entity's collision box, hitboxes and server attachments follow: the one the clients draw its
// studio model at, for the classes above; 1 for everything else. During an env_render scale fade that is the scale
// of this moment (the clients draw the fade from the same start time and length; pev->scale holds its end)
float UTIL_ModelScale( const entvars_t *pev )
{
	if ( !pev )
		return 1.0f;

	float flScale = pev->scale;
	BOOL fFading = RenderFade_ScaleNow( pev, &flScale );

	if ( flScale <= 0.0f || flScale == 1.0f )
		return 1.0f;

	if ( !UTIL_IsStudioModelName( pev ) || !UTIL_ScalesWithModel( pev ) )
		return 1.0f;

	// a fade's scale is not sent (the clients compute it): not in the message's 1/256 steps
	return fFading ? flScale : UTIL_ModelScaleValue( flScale );
}

/*
SBO 2.0: the engine's hitbox cache (r_cachestudio, on by default) keeps the hitboxes of the last few studio models
a trace met, keyed by model, frame, sequence, angles, origin, bone controllers and blending: not by the entity or
its scale. A model that changes its hitbox size (model scale, an env_render scale fade, a player's head scale) while
it stands still on the same frame would be hit with the hitboxes cached before the change. Its frame therefore
carries a small offset that depends on that size (at most 1/16 of a frame of a 256-frame cycle, never seen): the key
differs from the cached one, and the same key again means the same size again. No offset at scale 1.
flKey: a number for the hitbox size (UTIL_ModelScale x head scale)
*/
void UTIL_StudioHitboxesChanged( entvars_t *pev, float flKey )
{
	if ( !pev || !pev->pContainingEntity )
		return;

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( ENT( pev ) );
	if ( !pEntity )
		return;

	float flBias = 0.0f;
	if ( flKey != 1.0f )
	{
		unsigned int q = (unsigned int)( flKey * 4096.0f + 0.5f ) * 2654435761u;
		flBias = (float)( ( q >> 24 ) | 1 ) / 4096.0f;	// 1 .. 255 / 4096
	}

	if ( flBias == pEntity->m_flHitboxFrameBias )
		return;

	// inside 0 .. 256: StudioFrameAdvance takes a frame outside it for a finished sequence
	float flFrame = pev->frame + flBias - pEntity->m_flHitboxFrameBias;
	if ( flFrame < 0.0f )
		flFrame = 0.0f;
	else if ( flFrame >= 256.0f )
		flFrame = 255.99f;
	pev->frame = flFrame;
	pEntity->m_flHitboxFrameBias = flBias;
}

void UTIL_ScaleBox( const Vector &vecMin, const Vector &vecMax, float flScale, Vector &vecOutMin, Vector &vecOutMax )
{
	vecOutMin = vecMin * flScale;
	vecOutMax = vecMax * flScale;

	for ( int i = 0; i < 3; i++ )
	{
		if ( vecMax[i] <= vecMin[i] || vecOutMax[i] - vecOutMin[i] >= MODEL_SCALE_MIN_BOX )
			continue;

		// grow away from the origin (a box standing on its origin stays on it)
		if ( vecOutMin[i] >= 0.0f )
			vecOutMax[i] = vecOutMin[i] + MODEL_SCALE_MIN_BOX;
		else if ( vecOutMax[i] <= 0.0f )
			vecOutMin[i] = vecOutMax[i] - MODEL_SCALE_MIN_BOX;
		else
		{
			float flMid = ( vecOutMin[i] + vecOutMax[i] ) * 0.5f;
			vecOutMin[i] = flMid - MODEL_SCALE_MIN_BOX * 0.5f;
			vecOutMax[i] = flMid + MODEL_SCALE_MIN_BOX * 0.5f;
		}
	}
}

// the entity whose pev->mins / maxs are the box UTIL_SetSize scaled (NULL: unscaled, or changed since by SET_MODEL
// or directly)
static CBaseEntity *UTIL_ScaledSizeEntity( entvars_t *pev )
{
	if ( !pev || !pev->pContainingEntity )
		return NULL;

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( ENT( pev ) );

	if ( !pEntity || pEntity->m_flSizeScale <= 0.0f || pEntity->m_flSizeScale == 1.0f || pEntity->m_fUnscaledHullMove )
		return NULL;

	Vector vecMin, vecMax;
	UTIL_ScaleBox( pEntity->m_vecSizeMins, pEntity->m_vecSizeMaxs, pEntity->m_flSizeScale, vecMin, vecMax );

	if ( pev->mins != vecMin || pev->maxs != vecMax )
		return NULL;

	return pEntity;
}

void UTIL_SetSize( entvars_t *pev, const Vector &vecMin, const Vector &vecMax )
{
	CBaseEntity *pEntity = pev->pContainingEntity ? (CBaseEntity *)GET_PRIVATE( ENT( pev ) ) : NULL;
	float flScale = UTIL_ModelScale( pev );

	if ( pEntity )
	{
		// copies first: the caller may pass this entity's own pev->mins / maxs
		Vector vecSizeMin = vecMin, vecSizeMax = vecMax;
		pEntity->m_vecSizeMins = vecSizeMin;
		pEntity->m_vecSizeMaxs = vecSizeMax;
		pEntity->m_flSizeScale = flScale;
	}

	if ( flScale == 1.0f || !pEntity )
	{
		SET_SIZE( ENT(pev), vecMin, vecMax );
		return;
	}

	Vector vecScaledMin, vecScaledMax;
	UTIL_ScaleBox( vecMin, vecMax, flScale, vecScaledMin, vecScaledMax );
	SET_SIZE( ENT(pev), vecScaledMin, vecScaledMax );
}

// SBO 2.0 model scale: attack reach. Monsters measure attack distances between origins (CheckEnemy) and reach
// with traces of a fixed length from their origin (CheckTraceHullAttack, kicks). How much farther half the box
// reaches out than at scale 1 (0 when unscaled; below 0 when scaled down): CheckEnemy subtracts it for the
// attacker and the enemy (box edge to box edge, as for unscaled boxes) and the reach traces add it
float UTIL_ScaledHalfWidthExtra( entvars_t *pev )
{
	CBaseEntity *pEntity = UTIL_ScaledSizeEntity( pev );
	if ( !pEntity )
		return 0.0f;

	return ( ( pev->maxs.x - pev->mins.x ) - ( pEntity->m_vecSizeMaxs.x - pEntity->m_vecSizeMins.x ) ) * 0.5f;
}

// the height of the entity's box at scale 1 (pev->size.z when unscaled): a scaled monster's melee traces start at
// the height an unscaled one's do, so they still meet targets of a normal size
float UTIL_UnscaledHeight( entvars_t *pev )
{
	CBaseEntity *pEntity = UTIL_ScaledSizeEntity( pev );
	if ( !pEntity )
		return pev->size.z;

	return pEntity->m_vecSizeMaxs.z - pEntity->m_vecSizeMins.z;
}

void UTIL_GetUnscaledSize( entvars_t *pev, Vector &vecMin, Vector &vecMax )
{
	CBaseEntity *pEntity = UTIL_ScaledSizeEntity( pev );

	if ( pEntity )
	{
		vecMin = pEntity->m_vecSizeMins;
		vecMax = pEntity->m_vecSizeMaxs;
	}
	else
	{
		vecMin = pev->mins;
		vecMax = pev->maxs;
	}
}

void UTIL_ModelScaleChanged( entvars_t *pev )
{
	if ( !pev || !pev->pContainingEntity )
		return;

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( ENT( pev ) );

	if ( !pEntity )
		return;

	// the hitboxes follow the scale at once (the bone setup reads it), the engine's cache too
	UTIL_StudioHitboxesChanged( pev, UTIL_ModelScale( pev ) );

	if ( pEntity->m_flSizeScale <= 0.0f || pEntity->m_fUnscaledHullMove )
		return;	// its box was never set by UTIL_SetSize

	// the box UTIL_SetSize set last, at the scale it had then: still the current one?
	Vector vecMin, vecMax;
	UTIL_ScaleBox( pEntity->m_vecSizeMins, pEntity->m_vecSizeMaxs, pEntity->m_flSizeScale, vecMin, vecMax );

	if ( pev->mins != vecMin || pev->maxs != vecMax || UTIL_ModelScale( pev ) == pEntity->m_flSizeScale )
		return;

	vecMin = pEntity->m_vecSizeMins;
	vecMax = pEntity->m_vecSizeMaxs;
	UTIL_SetSize( pev, vecMin, vecMax );
}

// The box at scale 1 in pev for one engine movement call. Not when that box is in solid where the entity stands
// while the scaled one is not (the engine's physics moved it with the scaled box: a fall, a leap, a push): then
// the call keeps the scaled box, so the entity can get out
class CUnscaledHullMove
{
public:
	CUnscaledHullMove( edict_t *pent ) : m_pEntity( NULL )
	{
		if ( FNullEnt( pent ) )
			return;

		CBaseEntity *pEntity = UTIL_ScaledSizeEntity( VARS( pent ) );
		if ( !pEntity )
			return;

		entvars_t *pev = pEntity->pev;
		m_vecMins = pev->mins;
		m_vecMaxs = pev->maxs;
		m_vecSize = pev->size;

		pev->mins = pEntity->m_vecSizeMins;
		pev->maxs = pEntity->m_vecSizeMaxs;
		pev->size = pev->maxs - pev->mins;
		pEntity->m_fUnscaledHullMove = TRUE;
		m_pEntity = pEntity;

		TraceResult tr;
		TRACE_MONSTER_HULL( pent, pev->origin, pev->origin, TRUE, pent, &tr );
		if ( tr.fStartSolid || tr.fAllSolid )
		{
			Restore( );
			TRACE_MONSTER_HULL( pent, pev->origin, pev->origin, TRUE, pent, &tr );
			if ( tr.fStartSolid || tr.fAllSolid )
				Swap( pEntity );	// stuck either way: the box at scale 1, as an unscaled entity
		}
	}

	~CUnscaledHullMove( ) { Restore( ); }

	// the scaled-up entity (its scaled box is bigger than the one the engine moved)
	CBaseEntity *ScaledUp( void ) { return ( m_pEntity && m_pEntity->m_flSizeScale > 1.0f ) ? m_pEntity : NULL; }

	void Restore( void )
	{
		if ( !m_pEntity )
			return;

		entvars_t *pev = m_pEntity->pev;
		pev->mins = m_vecMins;
		pev->maxs = m_vecMaxs;
		pev->size = m_vecSize;
		m_pEntity->m_fUnscaledHullMove = FALSE;
		m_pEntity = NULL;
	}

private:
	void Swap( CBaseEntity *pEntity )
	{
		entvars_t *pev = pEntity->pev;
		m_vecMins = pev->mins;
		m_vecMaxs = pev->maxs;
		m_vecSize = pev->size;
		pev->mins = pEntity->m_vecSizeMins;
		pev->maxs = pEntity->m_vecSizeMaxs;
		pev->size = pev->maxs - pev->mins;
		pEntity->m_fUnscaledHullMove = TRUE;
		m_pEntity = pEntity;
	}

	CBaseEntity *m_pEntity;
	Vector m_vecMins, m_vecMaxs, m_vecSize;
};

// A player or monster (solid, not the entity's owner or owned by it: as the engine's SV_Move skips them) whose box
// the entity's scaled box overlaps at its new origin but did not at vecOldOrigin. NULL when none
static edict_t *UTIL_ScaledBoxBlocker( CBaseEntity *pEntity, const Vector &vecOldOrigin )
{
	entvars_t *pev = pEntity->pev;
	edict_t *pentSelf = pEntity->edict( );
	Vector vecMin = pev->origin + pev->mins, vecMax = pev->origin + pev->maxs;
	Vector vecOldMin = vecOldOrigin + pev->mins, vecOldMax = vecOldOrigin + pev->maxs;

	edict_t *pEdict = g_engfuncs.pfnPEntityOfEntOffset( 0 );
	if ( !pEdict )
		return NULL;

	for ( int i = 1; i < gpGlobals->maxEntities; i++ )
	{
		edict_t *pOther = pEdict + i;

		if ( pOther->free || pOther == pentSelf || !( pOther->v.flags & ( FL_CLIENT | FL_MONSTER ) ) )
			continue;
		if ( pOther->v.solid != SOLID_BBOX && pOther->v.solid != SOLID_SLIDEBOX )
			continue;
		if ( pOther->v.owner == pentSelf || pev->owner == pOther )
			continue;

		Vector vecOtherMin = pOther->v.origin + pOther->v.mins, vecOtherMax = pOther->v.origin + pOther->v.maxs;

		if ( vecMin.x >= vecOtherMax.x || vecMin.y >= vecOtherMax.y || vecMin.z >= vecOtherMax.z ||
			 vecMax.x <= vecOtherMin.x || vecMax.y <= vecOtherMin.y || vecMax.z <= vecOtherMin.z )
			continue;	// not touching it at the new origin

		if ( !( vecOldMin.x >= vecOtherMax.x || vecOldMin.y >= vecOtherMax.y || vecOldMin.z >= vecOtherMax.z ||
			    vecOldMax.x <= vecOtherMin.x || vecOldMax.y <= vecOtherMin.y || vecOldMax.z <= vecOtherMin.z ) )
			continue;	// already in it before the move: let it move (out)

		return pOther;
	}

	return NULL;
}

// WALK_MOVE. A scaled-up entity blocked at its scaled box by a player or monster stays where it was and the call
// fails, with that one in gpGlobals->trace_ent (as CheckLocalMove reads it; not for WALKMOVE_WORLDONLY)
int UTIL_WalkMove( edict_t *pent, float yaw, float dist, int iMode )
{
	Vector vecOldOrigin = pent->v.origin;
	CUnscaledHullMove hull( pent );
	int iResult = WALK_MOVE( pent, yaw, dist, iMode );
	CBaseEntity *pScaledUp = hull.ScaledUp( );
	hull.Restore( );

	if ( iResult && pScaledUp && iMode != WALKMOVE_WORLDONLY && pent->v.origin != vecOldOrigin )
	{
		edict_t *pBlocker = UTIL_ScaledBoxBlocker( pScaledUp, vecOldOrigin );
		if ( pBlocker )
		{
			if ( iMode == WALKMOVE_CHECKONLY )
				pent->v.origin = vecOldOrigin;			// the check moves it without linking it
			else
				UTIL_SetOrigin( &pent->v, vecOldOrigin );
			gpGlobals->trace_ent = pBlocker;
			return 0;
		}
	}

	return iResult;
}

int UTIL_DropToFloor( edict_t *pent )
{
	CUnscaledHullMove hull( pent );
	return DROP_TO_FLOOR( pent );
}

void UTIL_TraceMonsterHull( edict_t *pent, const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr )
{
	CUnscaledHullMove hull( pent );
	TRACE_MONSTER_HULL( pent, vecStart, vecEnd, igmon == ignore_monsters ? TRUE : FALSE, pentIgnore, ptr );
}

// MOVE_TO_ORIGIN. A scaled-up entity blocked at its scaled box by a player or monster stays where it was
void UTIL_MoveToOrigin( edict_t *pent, const Vector &vecGoal, float flDist, int iMoveType )
{
	float rgfl[3];
	vecGoal.CopyToArray(rgfl);

	Vector vecOldOrigin = pent->v.origin;
	CUnscaledHullMove hull( pent );
	MOVE_TO_ORIGIN ( pent, rgfl, flDist, iMoveType ); 
	CBaseEntity *pScaledUp = hull.ScaledUp( );
	hull.Restore( );

	if ( pScaledUp && pent->v.origin != vecOldOrigin && UTIL_ScaledBoxBlocker( pScaledUp, vecOldOrigin ) )
		UTIL_SetOrigin( &pent->v, vecOldOrigin );
}
	
	
float UTIL_VecToYaw( const Vector &vec )
{
	return VEC_TO_YAW(vec);
}


void UTIL_SetOrigin( entvars_t *pev, const Vector &vecOrigin )
{
	SET_ORIGIN(ENT(pev), vecOrigin );
}

void UTIL_ParticleEffect( const Vector &vecOrigin, const Vector &vecDirection, ULONG ulColor, ULONG ulCount )
{
	PARTICLE_EFFECT( vecOrigin, vecDirection, (float)ulColor, (float)ulCount );
}


float UTIL_Approach( float target, float value, float speed )
{
	float delta = target - value;

	if ( delta > speed )
		value += speed;
	else if ( delta < -speed )
		value -= speed;
	else 
		value = target;

	return value;
}


float UTIL_ApproachAngle( float target, float value, float speed )
{
	target = UTIL_AngleMod( target );
	value = UTIL_AngleMod( target );
	
	float delta = target - value;

	// Speed is assumed to be positive
	if ( speed < 0 )
		speed = -speed;

	if ( delta < -180 )
		delta += 360;
	else if ( delta > 180 )
		delta -= 360;

	if ( delta > speed )
		value += speed;
	else if ( delta < -speed )
		value -= speed;
	else 
		value = target;

	return value;
}


float UTIL_AngleDistance( float next, float cur )
{
	float delta = next - cur;

	if ( delta < -180 )
		delta += 360;
	else if ( delta > 180 )
		delta -= 360;

	return delta;
}


float UTIL_SplineFraction( float value, float scale )
{
	value = scale * value;
	float valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}


char* UTIL_VarArgs( char *format, ... )
{
	va_list		argptr;
	static char		string[1024];
	
	va_start (argptr, format);
	vsprintf (string, format,argptr);
	va_end (argptr);

	return string;	
}
	
Vector UTIL_GetAimVector( edict_t *pent, float flSpeed )
{
	Vector tmp;
	GET_AIM_VECTOR(pent, flSpeed, tmp);
	return tmp;
}

int UTIL_IsMasterTriggered(string_t sMaster, CBaseEntity *pActivator)
{
	if (sMaster)
	{
		// SBO 2.0, from Featureful / Spirit of Half-Life: "~name" passes when the master named "name" doesn't
		// (no map had a master name starting with ~)
		const char *szMaster = STRING( sMaster );
		BOOL bReverse = FALSE;
		if ( szMaster[0] == '~' )
		{
			szMaster++;
			bReverse = TRUE;
		}

		edict_t *pentTarget = FIND_ENTITY_BY_TARGETNAME(NULL, szMaster);
	
		if ( !FNullEnt(pentTarget) )
		{
			CBaseEntity *pMaster = CBaseEntity::Instance(pentTarget);
			if ( pMaster && (pMaster->ObjectCaps() & FCAP_MASTER) )
				return bReverse ? !pMaster->IsTriggered( pActivator ) : pMaster->IsTriggered( pActivator );
		}

		ALERT(at_console, "Master was null or not a master!\n");
	}

	// if this isn't a master entity, just say yes.
	return 1;
}

BOOL UTIL_ShouldShowBlood( int color )
{
	if ( color != DONT_BLEED )
	{
		if ( color == BLOOD_COLOR_RED )
		{
			if ( CVAR_GET_FLOAT("violence_hblood") != 0 )
				return TRUE;
		}
		else
		{
			if ( CVAR_GET_FLOAT("violence_ablood") != 0 )
				return TRUE;
		}
	}
	return FALSE;
}

int UTIL_PointContents(	const Vector &vec )
{
	return POINT_CONTENTS(vec);
}

void UTIL_BloodStream( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( g_Language == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
		WRITE_BYTE( TE_BLOODSTREAM );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z );
		WRITE_COORD( direction.x );
		WRITE_COORD( direction.y );
		WRITE_COORD( direction.z );
		WRITE_BYTE( color );
		WRITE_BYTE( min( amount, 255 ) );
	MESSAGE_END();
}				

void UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( color == DONT_BLEED || amount == 0 )
		return;

	if ( g_Language == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// scale up blood effect in multiplayer for better visibility
		amount *= 2;
	}

	if ( amount > 255 )
		amount = 255;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
		WRITE_BYTE( TE_BLOODSPRITE );
		WRITE_COORD( origin.x);								// pos
		WRITE_COORD( origin.y);
		WRITE_COORD( origin.z);
		WRITE_SHORT( g_sModelIndexBloodSpray );				// initial sprite model
		WRITE_SHORT( g_sModelIndexBloodDrop );				// droplet sprite models
		WRITE_BYTE( color );								// color index into host_basepal
		WRITE_BYTE( min( max( 3, amount / 10 ), 16 ) );		// size
	MESSAGE_END();
}				

Vector UTIL_RandomBloodVector( void )
{
	Vector direction;

	direction.x = RANDOM_FLOAT ( -1, 1 );
	direction.y = RANDOM_FLOAT ( -1, 1 );
	direction.z = RANDOM_FLOAT ( 0, 1 );

	return direction;
}


void UTIL_BloodDecalTrace( TraceResult *pTrace, int bloodColor )
{
	if ( UTIL_ShouldShowBlood( bloodColor ) )
	{
		if ( bloodColor == BLOOD_COLOR_RED )
			UTIL_DecalTrace( pTrace, DECAL_BLOOD1 + RANDOM_LONG(0,5) );
		else
			UTIL_DecalTrace( pTrace, DECAL_YBLOOD1 + RANDOM_LONG(0,5) );
	}
}


void UTIL_DecalTrace( TraceResult *pTrace, int decalNumber )
{
	short entityIndex;
	int index;
	int message;

	if ( decalNumber < 0 )
		return;

	index = gDecals[ decalNumber ].index;

	if ( index < 0 )
		return;

	if (pTrace->flFraction == 1.0)
		return;

	// Only decal BSP models
	if ( pTrace->pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( pTrace->pHit );
		if ( pEntity && !pEntity->IsBSPModel() )
			return;
		entityIndex = ENTINDEX( pTrace->pHit );
	}
	else 
		entityIndex = 0;

	message = TE_DECAL;
	if ( entityIndex != 0 )
	{
		if ( index > 255 )
		{
			message = TE_DECALHIGH;
			index -= 256;
		}
	}
	else
	{
		message = TE_WORLDDECAL;
		if ( index > 255 )
		{
			message = TE_WORLDDECALHIGH;
			index -= 256;
		}
	}
	
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( message );
		WRITE_COORD( pTrace->vecEndPos.x );
		WRITE_COORD( pTrace->vecEndPos.y );
		WRITE_COORD( pTrace->vecEndPos.z );
		WRITE_BYTE( index );
		if ( entityIndex )
			WRITE_SHORT( entityIndex );
	MESSAGE_END();
}

/*
==============
UTIL_PlayerDecalTrace

A player is trying to apply his custom decal for the spray can.
Tell connected clients to display it, or use the default spray can decal
if the custom can't be loaded.
==============
*/
void UTIL_PlayerDecalTrace( TraceResult *pTrace, int playernum, int decalNumber, BOOL bIsCustom )
{
	int index;
	
	if (!bIsCustom)
	{
		if ( decalNumber < 0 )
			return;

		index = gDecals[ decalNumber ].index;
		if ( index < 0 )
			return;
	}
	else
		index = decalNumber;

	if (pTrace->flFraction == 1.0)
		return;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_PLAYERDECAL );
		WRITE_BYTE ( playernum );
		WRITE_COORD( pTrace->vecEndPos.x );
		WRITE_COORD( pTrace->vecEndPos.y );
		WRITE_COORD( pTrace->vecEndPos.z );
		WRITE_SHORT( (short)ENTINDEX(pTrace->pHit) );
		WRITE_BYTE( index );
	MESSAGE_END();
}

void UTIL_GunshotDecalTrace( TraceResult *pTrace, int decalNumber )
{
	if ( decalNumber < 0 )
		return;

	int index = gDecals[ decalNumber ].index;
	if ( index < 0 )
		return;

	if (pTrace->flFraction == 1.0)
		return;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pTrace->vecEndPos );
		WRITE_BYTE( TE_GUNSHOTDECAL );
		WRITE_COORD( pTrace->vecEndPos.x );
		WRITE_COORD( pTrace->vecEndPos.y );
		WRITE_COORD( pTrace->vecEndPos.z );
		WRITE_SHORT( (short)ENTINDEX(pTrace->pHit) );
		WRITE_BYTE( index );
	MESSAGE_END();
}


void UTIL_Sparks( const Vector &position )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, position );
		WRITE_BYTE( TE_SPARKS );
		WRITE_COORD( position.x );
		WRITE_COORD( position.y );
		WRITE_COORD( position.z );
	MESSAGE_END();
}


void UTIL_Ricochet( const Vector &position, float scale )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, position );
		WRITE_BYTE( TE_ARMOR_RICOCHET );
		WRITE_COORD( position.x );
		WRITE_COORD( position.y );
		WRITE_COORD( position.z );
		WRITE_BYTE( (int)(scale*10) );
	MESSAGE_END();
}


BOOL UTIL_TeamsMatch( const char *pTeamName1, const char *pTeamName2 )
{
	// Everyone matches unless it's teamplay
	if ( !g_pGameRules->IsTeamplay() )
		return TRUE;

	// Both on a team?
	if ( *pTeamName1 != 0 && *pTeamName2 != 0 )
	{
		if ( !stricmp( pTeamName1, pTeamName2 ) )	// Same Team?
			return TRUE;
	}

	return FALSE;
}


void UTIL_StringToVector( float *pVector, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	// HL25: bounded copy, map keyvalues can be longer than the buffer
	strncpy( tempString, pString, sizeof( tempString ) );
	tempString[ sizeof( tempString ) - 1 ] = '\0';
	pstr = pfront = tempString;

	for ( j = 0; j < 3; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atof( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if (!*pstr)
			break;
		pstr++;
		pfront = pstr;
	}
	if (j < 2)
	{
		/*
		ALERT( at_error, "Bad field in entity!! %s:%s == \"%s\"\n",
			pkvd->szClassName, pkvd->szKeyName, pkvd->szValue );
		*/
		for (j = j+1;j < 3; j++)
			pVector[j] = 0;
	}
}


void UTIL_StringToIntArray( int *pVector, int count, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	// HL25: bounded copy, map keyvalues can be longer than the buffer
	strncpy( tempString, pString, sizeof( tempString ) );
	tempString[ sizeof( tempString ) - 1 ] = '\0';
	pstr = pfront = tempString;

	for ( j = 0; j < count; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atoi( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if (!*pstr)
			break;
		pstr++;
		pfront = pstr;
	}

	for ( j++; j < count; j++ )
	{
		pVector[j] = 0;
	}
}

Vector UTIL_ClampVectorToBox( const Vector &input, const Vector &clampSize )
{
	Vector sourceVector = input;

	if ( sourceVector.x > clampSize.x )
		sourceVector.x -= clampSize.x;
	else if ( sourceVector.x < -clampSize.x )
		sourceVector.x += clampSize.x;
	else
		sourceVector.x = 0;

	if ( sourceVector.y > clampSize.y )
		sourceVector.y -= clampSize.y;
	else if ( sourceVector.y < -clampSize.y )
		sourceVector.y += clampSize.y;
	else
		sourceVector.y = 0;
	
	if ( sourceVector.z > clampSize.z )
		sourceVector.z -= clampSize.z;
	else if ( sourceVector.z < -clampSize.z )
		sourceVector.z += clampSize.z;
	else
		sourceVector.z = 0;

	return sourceVector.Normalize();
}


float UTIL_WaterLevel( const Vector &position, float minz, float maxz )
{
	Vector midUp = position;
	midUp.z = minz;

	if (UTIL_PointContents(midUp) != CONTENTS_WATER)
		return minz;

	midUp.z = maxz;
	if (UTIL_PointContents(midUp) == CONTENTS_WATER)
		return maxz;

	float diff = maxz - minz;
	while (diff > 1.0)
	{
		midUp.z = minz + diff/2.0;
		if (UTIL_PointContents(midUp) == CONTENTS_WATER)
		{
			minz = midUp.z;
		}
		else
		{
			maxz = midUp.z;
		}
		diff = maxz - minz;
	}

	return midUp.z;
}


extern DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model

void UTIL_Bubbles( Vector mins, Vector maxs, int count )
{
	Vector mid =  (mins + maxs) * 0.5;

	float flHeight = UTIL_WaterLevel( mid,  mid.z, mid.z + 1024 );
	flHeight = flHeight - mins.z;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, mid );
		WRITE_BYTE( TE_BUBBLES );
		WRITE_COORD( mins.x );	// mins
		WRITE_COORD( mins.y );
		WRITE_COORD( mins.z );
		WRITE_COORD( maxs.x );	// maxz
		WRITE_COORD( maxs.y );
		WRITE_COORD( maxs.z );
		WRITE_COORD( flHeight );			// height
		WRITE_SHORT( g_sModelIndexBubbles );
		WRITE_BYTE( count ); // count
		WRITE_COORD( 8 ); // speed
	MESSAGE_END();
}

void UTIL_BubbleTrail( Vector from, Vector to, int count )
{
	float flHeight = UTIL_WaterLevel( from,  from.z, from.z + 256 );
	flHeight = flHeight - from.z;

	if (flHeight < 8)
	{
		flHeight = UTIL_WaterLevel( to,  to.z, to.z + 256 );
		flHeight = flHeight - to.z;
		if (flHeight < 8)
			return;

		// UNDONE: do a ploink sound
		flHeight = flHeight + to.z - from.z;
	}

	if (count > 255) 
		count = 255;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BUBBLETRAIL );
		WRITE_COORD( from.x );	// mins
		WRITE_COORD( from.y );
		WRITE_COORD( from.z );
		WRITE_COORD( to.x );	// maxz
		WRITE_COORD( to.y );
		WRITE_COORD( to.z );
		WRITE_COORD( flHeight );			// height
		WRITE_SHORT( g_sModelIndexBubbles );
		WRITE_BYTE( count ); // count
		WRITE_COORD( 8 ); // speed
	MESSAGE_END();
}


void UTIL_Remove( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return;

	pEntity->UpdateOnRemove();
	pEntity->pev->flags |= FL_KILLME;
	pEntity->pev->targetname = 0;
}


BOOL UTIL_IsValidEntity( edict_t *pent )
{
	if ( !pent || pent->free || (pent->v.flags & FL_KILLME) )
		return FALSE;
	return TRUE;
}


void UTIL_PrecacheOther( const char *szClassname )
{
	edict_t	*pent;

	pent = CREATE_NAMED_ENTITY( MAKE_STRING( szClassname ) );
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in UTIL_PrecacheOther\n" );
		return;
	}
	

	CBaseEntity *pEntity = CBaseEntity::Instance (VARS( pent ));
	if (pEntity)
		pEntity->Precache( );
	REMOVE_ENTITY(pent);
}

BOOL UTIL_MapperModelUnusable( entvars_t *pev, const char *szModel )
{
	const char *pszWhy = NULL;

	if ( !szModel || !szModel[0] )
	{
		pszWhy = "has no model (key \"model\")";
	}
	else
	{
		// models/editor/... in any case, with \ or / (J.A.C.K. and Hammer write what the mapper typed)
		const char *p = szModel;
		while ( *p == '/' || *p == '\\' )
			p++;
		char szDir[15];
		int i;
		for ( i = 0; i < 14 && p[i]; i++ )
			szDir[i] = ( p[i] == '\\' ) ? '/' : p[i];
		szDir[i] = '\0';

		if ( i == 14 && !strnicmp( szDir, "models/editor/", 14 ) )
		{
			pszWhy = "uses the map editor's placeholder model, which the game does not ship";
		}
		else
		{
			// the same file system call the engine's model loader makes, so the answer is the one precaching gets
			int iLength = 0;
			byte *pFile = LOAD_FILE_FOR_ME( (char *)szModel, &iLength );
			if ( pFile )
				FREE_FILE( pFile );
			if ( !pFile || iLength <= 0 )
				pszWhy = "uses a model the server does not have";
		}
	}

	if ( !pszWhy )
		return FALSE;

	char szText[512];
	snprintf( szText, sizeof( szText ), "Map %s: %s \"%s\" at %.0f %.0f %.0f %s (\"%s\"): removed\n",
		STRING( gpGlobals->mapname ), STRING( pev->classname ), STRING( pev->targetname ),
		pev->origin.x, pev->origin.y, pev->origin.z, pszWhy, szModel ? szModel : "" );
	szText[ sizeof( szText ) - 1 ] = '\0';
	ALERT( at_console, "%s", szText );	// mapper messages only with developer 1 or 2
	return TRUE;
}

//=========================================================
// UTIL_LogPrintf - Prints a logged message to console.
// Preceded by LOG: ( timestamp ) < message >
//=========================================================
void UTIL_LogPrintf( char *fmt, ... )
{
	va_list			argptr;
	static char		string[1024];
	
	va_start ( argptr, fmt );
	vsprintf ( string, fmt, argptr );
	va_end   ( argptr );

	// Print to server console
	ALERT( at_logged, "%s", string );
}

//=========================================================
// UTIL_DotPoints - returns the dot product of a line from
// src to check and vecdir.
//=========================================================
float UTIL_DotPoints ( const Vector &vecSrc, const Vector &vecCheck, const Vector &vecDir )
{
	Vector2D	vec2LOS;

	vec2LOS = ( vecCheck - vecSrc ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	return DotProduct (vec2LOS , ( vecDir.Make2D() ) );
}


//=========================================================
// UTIL_StripToken - for redundant keynames
//=========================================================
void UTIL_StripToken( const char *pKey, char *pDest, int nLen )
{
	int i = 0;

	// HL25: bound the copy to the destination size
	while ( i < nLen - 1 && pKey[i] && pKey[i] != '#' )
	{
		pDest[i] = pKey[i];
		i++;
	}
	pDest[i] = 0;
}


// --------------------------------------------------------------
//
// CSave
//
// --------------------------------------------------------------
static int gSizes[FIELD_TYPECOUNT] = 
{
	sizeof(float),		// FIELD_FLOAT
	sizeof(int),		// FIELD_STRING
	sizeof(int),		// FIELD_ENTITY
	sizeof(int),		// FIELD_CLASSPTR
	sizeof(EHANDLE),	// FIELD_EHANDLE (halflife-updated #224: full EHANDLE size so arrays are saved/cleared completely)
	sizeof(int),		// FIELD_entvars_t
	sizeof(int),		// FIELD_EDICT
	sizeof(float)*3,	// FIELD_VECTOR
	sizeof(float)*3,	// FIELD_POSITION_VECTOR
	sizeof(int *),		// FIELD_POINTER
	sizeof(int),		// FIELD_INTEGER
	sizeof(int *),		// FIELD_FUNCTION
	sizeof(int),		// FIELD_BOOLEAN
	sizeof(short),		// FIELD_SHORT
	sizeof(char),		// FIELD_CHARACTER
	sizeof(float),		// FIELD_TIME
	sizeof(int),		// FIELD_MODELNAME
	sizeof(int),		// FIELD_SOUNDNAME
};


// Base class includes common SAVERESTOREDATA pointer, and manages the entity table
CSaveRestoreBuffer :: CSaveRestoreBuffer( void )
{
	m_pdata = NULL;
}


CSaveRestoreBuffer :: CSaveRestoreBuffer( SAVERESTOREDATA *pdata )
{
	m_pdata = pdata;
}


CSaveRestoreBuffer :: ~CSaveRestoreBuffer( void )
{
}

int	CSaveRestoreBuffer :: EntityIndex( CBaseEntity *pEntity )
{
	if ( pEntity == NULL )
		return -1;
	return EntityIndex( pEntity->pev );
}


int	CSaveRestoreBuffer :: EntityIndex( entvars_t *pevLookup )
{
	if ( pevLookup == NULL )
		return -1;
	return EntityIndex( ENT( pevLookup ) );
}

int	CSaveRestoreBuffer :: EntityIndex( EOFFSET eoLookup )
{
	return EntityIndex( ENT( eoLookup ) );
}


int	CSaveRestoreBuffer :: EntityIndex( edict_t *pentLookup )
{
	if ( !m_pdata || pentLookup == NULL )
		return -1;

	int i;
	ENTITYTABLE *pTable;

	for ( i = 0; i < m_pdata->tableCount; i++ )
	{
		pTable = m_pdata->pTable + i;
		if ( pTable->pent == pentLookup )
			return i;
	}
	return -1;
}


edict_t *CSaveRestoreBuffer :: EntityFromIndex( int entityIndex )
{
	if ( !m_pdata || entityIndex < 0 )
		return NULL;

	int i;
	ENTITYTABLE *pTable;

	for ( i = 0; i < m_pdata->tableCount; i++ )
	{
		pTable = m_pdata->pTable + i;
		if ( pTable->id == entityIndex )
			return pTable->pent;
	}
	return NULL;
}


int	CSaveRestoreBuffer :: EntityFlagsSet( int entityIndex, int flags )
{
	if ( !m_pdata || entityIndex < 0 )
		return 0;
	if ( entityIndex > m_pdata->tableCount )
		return 0;

	m_pdata->pTable[ entityIndex ].flags |= flags;

	return m_pdata->pTable[ entityIndex ].flags;
}


void CSaveRestoreBuffer :: BufferRewind( int size )
{
	if ( !m_pdata )
		return;

	if ( m_pdata->size < size )
		size = m_pdata->size;

	m_pdata->pCurrentData -= size;
	m_pdata->size -= size;
}

#ifndef _WIN32
extern "C" {
unsigned _rotr ( unsigned val, int shift)
{
        register unsigned lobit;        /* non-zero means lo bit set */
        register unsigned num = val;    /* number to rotate */

        shift &= 0x1f;                  /* modulo 32 -- this will also make
                                           negative shifts work */

        while (shift--) {
                lobit = num & 1;        /* get high bit */
                num >>= 1;              /* shift right one bit */
                if (lobit)
                        num |= 0x80000000;  /* set hi bit if lo bit was set */
        }

        return num;
}
}
#endif

unsigned int CSaveRestoreBuffer :: HashString( const char *pszToken )
{
	unsigned int	hash = 0;

	while ( *pszToken )
		hash = _rotr( hash, 4 ) ^ *pszToken++;

	return hash;
}

unsigned short CSaveRestoreBuffer :: TokenHash( const char *pszToken )
{
	unsigned short	hash = (unsigned short)(HashString( pszToken ) % (unsigned)m_pdata->tokenCount );
	
#if _DEBUG
	static int tokensparsed = 0;
	tokensparsed++;
	if ( !m_pdata->tokenCount || !m_pdata->pTokens )
		ALERT( at_error, "No token table array in TokenHash()!" );
#endif

	for ( int i=0; i<m_pdata->tokenCount; i++ )
	{
#if _DEBUG
		static qboolean beentheredonethat = FALSE;
		if ( i > 50 && !beentheredonethat )
		{
			beentheredonethat = TRUE;
			ALERT( at_error, "CSaveRestoreBuffer :: TokenHash() is getting too full!" );
		}
#endif

		int	index = hash + i;
		if ( index >= m_pdata->tokenCount )
			index -= m_pdata->tokenCount;

		if ( !m_pdata->pTokens[index] || strcmp( pszToken, m_pdata->pTokens[index] ) == 0 )
		{
			m_pdata->pTokens[index] = (char *)pszToken;
			return index;
		}
	}
		
	// Token hash table full!!! 
	// [Consider doing overflow table(s) after the main table & limiting linear hash table search]
	ALERT( at_error, "CSaveRestoreBuffer :: TokenHash() is COMPLETELY FULL!" );
	return 0;
}

void CSave :: WriteData( const char *pname, int size, const char *pdata )
{
	BufferField( pname, size, pdata );
}


void CSave :: WriteShort( const char *pname, const short *data, int count )
{
	BufferField( pname, sizeof(short) * count, (const char *)data );
}


void CSave :: WriteInt( const char *pname, const int *data, int count )
{
	BufferField( pname, sizeof(int) * count, (const char *)data );
}


void CSave :: WriteFloat( const char *pname, const float *data, int count )
{
	BufferField( pname, sizeof(float) * count, (const char *)data );
}


void CSave :: WriteTime( const char *pname, const float *data, int count )
{
	int i;
	Vector tmp, input;

	BufferHeader( pname, sizeof(float) * count );
	for ( i = 0; i < count; i++ )
	{
		float tmp = data[0];

		// Always encode time as a delta from the current time so it can be re-based if loaded in a new level
		// Times of 0 are never written to the file, so they will be restored as 0, not a relative time
		if ( m_pdata )
			tmp -= m_pdata->time;

		BufferData( (const char *)&tmp, sizeof(float) );
		data ++;
	}
}


void CSave :: WriteString( const char *pname, const char *pdata )
{
#ifdef TOKENIZE
	short	token = (short)TokenHash( pdata );
	WriteShort( pname, &token, 1 );
#else
	BufferField( pname, strlen(pdata) + 1, pdata );
#endif
}


void CSave :: WriteString( const char *pname, const int *stringId, int count )
{
	int i, size;

#ifdef TOKENIZE
	short	token = (short)TokenHash( STRING( *stringId ) );
	WriteShort( pname, &token, 1 );
#else
#if 0
	if ( count != 1 )
		ALERT( at_error, "No string arrays!\n" );
	WriteString( pname, (char *)STRING(*stringId) );
#endif

	size = 0;
	for ( i = 0; i < count; i++ )
		size += strlen( STRING( stringId[i] ) ) + 1;

	BufferHeader( pname, size );
	for ( i = 0; i < count; i++ )
	{
		const char *pString = STRING(stringId[i]);
		BufferData( pString, strlen(pString)+1 );
	}
#endif
}


void CSave :: WriteVector( const char *pname, const Vector &value )
{
	WriteVector( pname, &value.x, 1 );
}


void CSave :: WriteVector( const char *pname, const float *value, int count )
{
	BufferHeader( pname, sizeof(float) * 3 * count );
	BufferData( (const char *)value, sizeof(float) * 3 * count );
}



void CSave :: WritePositionVector( const char *pname, const Vector &value )
{

	if ( m_pdata && m_pdata->fUseLandmark )
	{
		Vector tmp = value - m_pdata->vecLandmarkOffset;
		WriteVector( pname, tmp );
	}

	WriteVector( pname, value );
}


void CSave :: WritePositionVector( const char *pname, const float *value, int count )
{
	int i;
	Vector tmp, input;

	BufferHeader( pname, sizeof(float) * 3 * count );
	for ( i = 0; i < count; i++ )
	{
		Vector tmp( value[0], value[1], value[2] );

		if ( m_pdata && m_pdata->fUseLandmark )
			tmp = tmp - m_pdata->vecLandmarkOffset;

		BufferData( (const char *)&tmp.x, sizeof(float) * 3 );
		value += 3;
	}
}


void CSave :: WriteFunction( const char *pname, const int *data, int count )
{
	const char *functionName;

	functionName = NAME_FOR_FUNCTION( *data );
	if ( functionName )
		BufferField( pname, strlen(functionName) + 1, functionName );
	else
		ALERT( at_error, "Invalid function pointer in entity!" );
}


// SBO 2.0: the type a field's value has in entvars_t. The save table above says FIELD_FLOAT for four fields that are
// ints in entvars_t (progdefs.h: flags, deadflag, fixangle, light_level), as Valve's does: save / restore copies the
// four bytes either way, so the table stays as it is. Reading or writing a value by name (the map loader, and
// trigger_changevalue / trigger_copyvalue through GetEntvarsKeyvalue) must treat them as ints: as floats, "flags"
// "512" stored the bits of 512.0f, and changevalue's OR on flags gave 1124073472 (no SBO map sets these keys).
template<typename T> struct EntvarsIsInt { enum { value = 0 }; };
template<> struct EntvarsIsInt<int> { enum { value = 1 }; };
#define ENTVARS_INT( field ) static_assert( EntvarsIsInt<decltype( ((entvars_t *)0)->field )>::value, #field " is an int in entvars_t" )
ENTVARS_INT( flags );
ENTVARS_INT( deadflag );
ENTVARS_INT( fixangle );
ENTVARS_INT( light_level );

static int EntvarsValueType( const TYPEDESCRIPTION *pField )
{
	if ( pField->fieldType == FIELD_FLOAT
		&& ( pField->fieldOffset == (int)offsetof( entvars_t, flags ) || pField->fieldOffset == (int)offsetof( entvars_t, deadflag )
			|| pField->fieldOffset == (int)offsetof( entvars_t, fixangle )
			|| pField->fieldOffset == (int)offsetof( entvars_t, light_level ) ) )
		return FIELD_INTEGER;
	return pField->fieldType;
}

void EntvarsKeyvalue( entvars_t *pev, KeyValueData *pkvd )
{
	int i;
	TYPEDESCRIPTION		*pField;

	for ( i = 0; i < ENTVARS_COUNT; i++ )
	{
		pField = &gEntvarsDescription[i];

		if ( !stricmp( pField->fieldName, pkvd->szKeyName ) )
		{
			switch( EntvarsValueType( pField ) )
			{
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
			case FIELD_STRING:
				(*(int *)((char *)pev + pField->fieldOffset)) = ALLOC_STRING( pkvd->szValue );
				break;

			case FIELD_TIME:
			case FIELD_FLOAT:
				(*(float *)((char *)pev + pField->fieldOffset)) = atof( pkvd->szValue );
				break;

			case FIELD_INTEGER:
				(*(int *)((char *)pev + pField->fieldOffset)) = atoi( pkvd->szValue );
				break;

			case FIELD_POSITION_VECTOR:
			case FIELD_VECTOR:
				UTIL_StringToVector( (float *)((char *)pev + pField->fieldOffset), pkvd->szValue );
				break;

			default:
			case FIELD_EVARS:
			case FIELD_CLASSPTR:
			case FIELD_EDICT:
			case FIELD_ENTITY:
			case FIELD_POINTER:
				ALERT( at_error, "Bad field in entity!!\n" );
				break;
			}
			pkvd->fHandled = TRUE;
			return;
		}
	}
}



// SevenKewp: GetEntvarsKeyvalue (dlls/util/util.cpp). The current value of an entvars field, by the name the
// map loader knows it by (EntvarsKeyvalue above).
CKeyValue GetEntvarsKeyvalue( entvars_t *pev, const char *keyName )
{
	CKeyValue keyvalue = g_emptyKeyValue;
	TYPEDESCRIPTION *pField;

	for ( int i = 0; i < (int)ENTVARS_COUNT; i++ )
	{
		pField = &gEntvarsDescription[i];

		if ( !stricmp( pField->fieldName, keyName ) )
		{
			keyvalue.keyName = pField->fieldName;
			keyvalue.keyOffset = pField->fieldOffset;

			switch ( EntvarsValueType( pField ) )
			{
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
			case FIELD_STRING:
				keyvalue.sVal = (*(int *)((char *)pev + pField->fieldOffset));
				keyvalue.keyType = KEY_TYPE_STRING;
				break;

			case FIELD_TIME:
			case FIELD_FLOAT:
				keyvalue.fVal = (*(float *)((char *)pev + pField->fieldOffset));
				keyvalue.keyType = KEY_TYPE_FLOAT;
				break;

			case FIELD_INTEGER:
				keyvalue.iVal = (*(int *)((char *)pev + pField->fieldOffset));
				keyvalue.keyType = KEY_TYPE_INT;
				break;

			case FIELD_POSITION_VECTOR:
			case FIELD_VECTOR:
				memcpy( keyvalue.vVal, (float *)((char *)pev + pField->fieldOffset), 3 * sizeof(float) );
				keyvalue.keyType = KEY_TYPE_VECTOR;
				break;

			// an entity: its name
			case FIELD_EVARS:
			{
				entvars_t *vars = (*(entvars_t **)((char *)pev + pField->fieldOffset));
				keyvalue.sVal = vars ? vars->targetname : 0;
				keyvalue.keyType = KEY_TYPE_STRING;
				break;
			}
			case FIELD_CLASSPTR:
			{
				CBaseEntity *ent = (*(CBaseEntity **)((char *)pev + pField->fieldOffset));
				keyvalue.sVal = ent ? ent->pev->targetname : 0;
				keyvalue.keyType = KEY_TYPE_STRING;
				break;
			}
			case FIELD_EDICT:
			{
				edict_t *ent = (*(edict_t **)((char *)pev + pField->fieldOffset));
				keyvalue.sVal = ent ? ent->v.targetname : 0;
				keyvalue.keyType = KEY_TYPE_STRING;
				break;
			}
			case FIELD_ENTITY:
			{
				edict_t *ent = ENT( (*(EOFFSET *)((char *)pev + pField->fieldOffset)) );
				keyvalue.sVal = ent ? ent->v.targetname : 0;
				keyvalue.keyType = KEY_TYPE_STRING;
				break;
			}
			default:
				break;
			}

			return keyvalue;
		}
	}

	return keyvalue;
}

// SBO: a key the engine needs a precache for (KeyValue_DispatchRuntime refuses it)
BOOL EntvarsKeyNeedsPrecache( const char *keyName )
{
	for ( int i = 0; i < (int)ENTVARS_COUNT; i++ )
	{
		if ( !stricmp( gEntvarsDescription[i].fieldName, keyName ) )
			return gEntvarsDescription[i].fieldType == FIELD_MODELNAME
				|| gEntvarsDescription[i].fieldType == FIELD_SOUNDNAME;
	}

	return FALSE;
}


int CSave :: WriteEntVars( const char *pname, entvars_t *pev )
{
	return WriteFields( pname, pev, gEntvarsDescription, ENTVARS_COUNT );
}



int CSave :: WriteFields( const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	int				i, j, actualCount, emptyCount;
	TYPEDESCRIPTION	*pTest;
	int				entityArray[MAX_ENTITYARRAY];

	// Precalculate the number of empty fields
	emptyCount = 0;
	for ( i = 0; i < fieldCount; i++ )
	{
		void *pOutputData;
		pOutputData = ((char *)pBaseData + pFields[i].fieldOffset );
		if ( DataEmpty( (const char *)pOutputData, pFields[i].fieldSize * gSizes[pFields[i].fieldType] ) )
			emptyCount++;
	}

	// Empty fields will not be written, write out the actual number of fields to be written
	actualCount = fieldCount - emptyCount;
	WriteInt( pname, &actualCount, 1 );

	for ( i = 0; i < fieldCount; i++ )
	{
		void *pOutputData;
		pTest = &pFields[ i ];
		pOutputData = ((char *)pBaseData + pTest->fieldOffset );

		// UNDONE: Must we do this twice?
		if ( DataEmpty( (const char *)pOutputData, pTest->fieldSize * gSizes[pTest->fieldType] ) )
			continue;

		switch( pTest->fieldType )
		{
		case FIELD_FLOAT:
			WriteFloat( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
		break;
		case FIELD_TIME:
			WriteTime( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
		break;
		case FIELD_MODELNAME:
		case FIELD_SOUNDNAME:
		case FIELD_STRING:
			WriteString( pTest->fieldName, (int *)pOutputData, pTest->fieldSize );
		break;
		case FIELD_CLASSPTR:
		case FIELD_EVARS:
		case FIELD_EDICT:
		case FIELD_ENTITY:
		case FIELD_EHANDLE:
			if ( pTest->fieldSize > MAX_ENTITYARRAY )
				ALERT( at_error, "Can't save more than %d entities in an array!!!\n", MAX_ENTITYARRAY );
			for ( j = 0; j < pTest->fieldSize; j++ )
			{
				switch( pTest->fieldType )
				{
					case FIELD_EVARS:
						entityArray[j] = EntityIndex( ((entvars_t **)pOutputData)[j] );
					break;
					case FIELD_CLASSPTR:
						entityArray[j] = EntityIndex( ((CBaseEntity **)pOutputData)[j] );
					break;
					case FIELD_EDICT:
						entityArray[j] = EntityIndex( ((edict_t **)pOutputData)[j] );
					break;
					case FIELD_ENTITY:
						entityArray[j] = EntityIndex( ((EOFFSET *)pOutputData)[j] );
					break;
					case FIELD_EHANDLE:
						entityArray[j] = EntityIndex( (CBaseEntity *)(((EHANDLE *)pOutputData)[j]) );
					break;
				}
			}
			WriteInt( pTest->fieldName, entityArray, pTest->fieldSize );
		break;
		case FIELD_POSITION_VECTOR:
			WritePositionVector( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
		break;
		case FIELD_VECTOR:
			WriteVector( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
		break;

		case FIELD_BOOLEAN:
		case FIELD_INTEGER:
			WriteInt( pTest->fieldName, (int *)pOutputData, pTest->fieldSize );
		break;

		case FIELD_SHORT:
			WriteData( pTest->fieldName, 2 * pTest->fieldSize, ((char *)pOutputData) );
		break;

		case FIELD_CHARACTER:
			WriteData( pTest->fieldName, pTest->fieldSize, ((char *)pOutputData) );
		break;

		// For now, just write the address out, we're not going to change memory while doing this yet!
		case FIELD_POINTER:
			WriteInt( pTest->fieldName, (int *)(char *)pOutputData, pTest->fieldSize );
		break;

		case FIELD_FUNCTION:
			WriteFunction( pTest->fieldName, (int *)(char *)pOutputData, pTest->fieldSize );
		break;
		default:
			ALERT( at_error, "Bad field type\n" );
		}
	}

	return 1;
}


void CSave :: BufferString( char *pdata, int len )
{
	char c = 0;

	BufferData( pdata, len );		// Write the string
	BufferData( &c, 1 );			// Write a null terminator
}


int CSave :: DataEmpty( const char *pdata, int size )
{
	for ( int i = 0; i < size; i++ )
	{
		if ( pdata[i] )
			return 0;
	}
	return 1;
}


void CSave :: BufferField( const char *pname, int size, const char *pdata )
{
	BufferHeader( pname, size );
	BufferData( pdata, size );
}


void CSave :: BufferHeader( const char *pname, int size )
{
	short	hashvalue = TokenHash( pname );
	if ( size > 1<<(sizeof(short)*8) )
		ALERT( at_error, "CSave :: BufferHeader() size parameter exceeds 'short'!" );
	BufferData( (const char *)&size, sizeof(short) );
	BufferData( (const char *)&hashvalue, sizeof(short) );
}


void CSave :: BufferData( const char *pdata, int size )
{
	if ( !m_pdata )
		return;

	if ( m_pdata->size + size > m_pdata->bufferSize )
	{
		ALERT( at_error, "Save/Restore overflow!" );
		m_pdata->size = m_pdata->bufferSize;
		return;
	}

	memcpy( m_pdata->pCurrentData, pdata, size );
	m_pdata->pCurrentData += size;
	m_pdata->size += size;
}



// --------------------------------------------------------------
//
// CRestore
//
// --------------------------------------------------------------

int CRestore::ReadField( void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount, int startField, int size, char *pName, void *pData )
{
	int i, j, stringCount, fieldNumber, entityIndex;
	TYPEDESCRIPTION *pTest;
	float	time, timeData;
	Vector	position;
	edict_t	*pent;
	char	*pString;

	time = 0;
	position = Vector(0,0,0);

	if ( m_pdata )
	{
		time = m_pdata->time;
		if ( m_pdata->fUseLandmark )
			position = m_pdata->vecLandmarkOffset;
	}

	for ( i = 0; i < fieldCount; i++ )
	{
		fieldNumber = (i+startField)%fieldCount;
		pTest = &pFields[ fieldNumber ];
		// HL25: fieldName can be NULL
		if ( pTest->fieldName && !stricmp( pTest->fieldName, pName ) )
		{
			if ( !m_global || !(pTest->flags & FTYPEDESC_GLOBAL) )
			{
				for ( j = 0; j < pTest->fieldSize; j++ )
				{
					void *pOutputData = ((char *)pBaseData + pTest->fieldOffset + (j*gSizes[pTest->fieldType]) );
					void *pInputData = (char *)pData + j * gSizes[pTest->fieldType];

					switch( pTest->fieldType )
					{
					case FIELD_TIME:
						timeData = *(float *)pInputData;
						// Re-base time variables
						timeData += time;
						*((float *)pOutputData) = timeData;
					break;
					case FIELD_FLOAT:
						*((float *)pOutputData) = *(float *)pInputData;
					break;
					case FIELD_MODELNAME:
					case FIELD_SOUNDNAME:
					case FIELD_STRING:
						// Skip over j strings
						pString = (char *)pData;
						for ( stringCount = 0; stringCount < j; stringCount++ )
						{
							while (*pString)
								pString++;
							pString++;
						}
						pInputData = pString;
						if ( strlen( (char *)pInputData ) == 0 )
							*((int *)pOutputData) = 0;
						else
						{
							int string;

							string = ALLOC_STRING( (char *)pInputData );
							
							*((int *)pOutputData) = string;

							if ( !FStringNull( string ) && m_precache )
							{
								if ( pTest->fieldType == FIELD_MODELNAME )
									PRECACHE_MODEL( (char *)STRING( string ) );
								else if ( pTest->fieldType == FIELD_SOUNDNAME )
									PRECACHE_SOUND( (char *)STRING( string ) );
							}
						}
					break;
					case FIELD_EVARS:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((entvars_t **)pOutputData) = VARS(pent);
						else
							*((entvars_t **)pOutputData) = NULL;
					break;
					case FIELD_CLASSPTR:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((CBaseEntity **)pOutputData) = CBaseEntity::Instance(pent);
						else
							*((CBaseEntity **)pOutputData) = NULL;
					break;
					case FIELD_EDICT:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						*((edict_t **)pOutputData) = pent;
					break;
					case FIELD_EHANDLE:
						// Input and Output sizes are different! (halflife-updated #224: saved as int indices)
						pInputData = (char *)pData + j * sizeof(int);
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((EHANDLE *)pOutputData) = CBaseEntity::Instance(pent);
						else
							*((EHANDLE *)pOutputData) = NULL;
					break;
					case FIELD_ENTITY:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((EOFFSET *)pOutputData) = OFFSET(pent);
						else
							*((EOFFSET *)pOutputData) = 0;
					break;
					case FIELD_VECTOR:
						((float *)pOutputData)[0] = ((float *)pInputData)[0];
						((float *)pOutputData)[1] = ((float *)pInputData)[1];
						((float *)pOutputData)[2] = ((float *)pInputData)[2];
					break;
					case FIELD_POSITION_VECTOR:
						((float *)pOutputData)[0] = ((float *)pInputData)[0] + position.x;
						((float *)pOutputData)[1] = ((float *)pInputData)[1] + position.y;
						((float *)pOutputData)[2] = ((float *)pInputData)[2] + position.z;
					break;

					case FIELD_BOOLEAN:
					case FIELD_INTEGER:
						*((int *)pOutputData) = *( int *)pInputData;
					break;

					case FIELD_SHORT:
						*((short *)pOutputData) = *( short *)pInputData;
					break;

					case FIELD_CHARACTER:
						*((char *)pOutputData) = *( char *)pInputData;
					break;

					case FIELD_POINTER:
						*((int *)pOutputData) = *( int *)pInputData;
					break;
					case FIELD_FUNCTION:
						if ( strlen( (char *)pInputData ) == 0 )
							*((int *)pOutputData) = 0;
						else
							*((int *)pOutputData) = FUNCTION_FROM_NAME( (char *)pInputData );
					break;

					default:
						ALERT( at_error, "Bad field type\n" );
					}
				}
			}
#if 0
			else
			{
				ALERT( at_console, "Skipping global field %s\n", pName );
			}
#endif
			return fieldNumber;
		}
	}

	return -1;
}


int CRestore::ReadEntVars( const char *pname, entvars_t *pev )
{
	return ReadFields( pname, pev, gEntvarsDescription, ENTVARS_COUNT );
}


int CRestore::ReadFields( const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	unsigned short	i, token;
	int		lastField, fileCount;
	HEADER	header;

	i = ReadShort();
	ASSERT( i == sizeof(int) );			// First entry should be an int

	token = ReadShort();

	// Check the struct name
	if ( token != TokenHash(pname) )			// Field Set marker
	{
//		ALERT( at_error, "Expected %s found %s!\n", pname, BufferPointer() );
		BufferRewind( 2*sizeof(short) );
		return 0;
	}

	// Skip over the struct name
	fileCount = ReadInt();						// Read field count

	lastField = 0;								// Make searches faster, most data is read/written in the same order

	// Clear out base data
	for ( i = 0; i < fieldCount; i++ )
	{
		// Don't clear global fields
		if ( !m_global || !(pFields[i].flags & FTYPEDESC_GLOBAL) )
			memset( ((char *)pBaseData + pFields[i].fieldOffset), 0, pFields[i].fieldSize * gSizes[pFields[i].fieldType] );
	}

	for ( i = 0; i < fileCount; i++ )
	{
		BufferReadHeader( &header );
		lastField = ReadField( pBaseData, pFields, fieldCount, lastField, header.size, m_pdata->pTokens[header.token], header.pData );
		lastField++;
	}
	
	return 1;
}


void CRestore::BufferReadHeader( HEADER *pheader )
{
	ASSERT( pheader!=NULL );
	pheader->size = ReadShort();				// Read field size
	pheader->token = ReadShort();				// Read field name token
	pheader->pData = BufferPointer();			// Field Data is next
	BufferSkipBytes( pheader->size );			// Advance to next field
}


short	CRestore::ReadShort( void )
{
	short tmp = 0;

	BufferReadBytes( (char *)&tmp, sizeof(short) );

	return tmp;
}

int	CRestore::ReadInt( void )
{
	int tmp = 0;

	BufferReadBytes( (char *)&tmp, sizeof(int) );

	return tmp;
}

int CRestore::ReadNamedInt( const char *pName )
{
	HEADER header;

	BufferReadHeader( &header );
	return ((int *)header.pData)[0];
}

char *CRestore::ReadNamedString( const char *pName )
{
	HEADER header;

	BufferReadHeader( &header );
#ifdef TOKENIZE
	return (char *)(m_pdata->pTokens[*(short *)header.pData]);
#else
	return (char *)header.pData;
#endif
}


char *CRestore::BufferPointer( void )
{
	if ( !m_pdata )
		return NULL;

	return m_pdata->pCurrentData;
}

void CRestore::BufferReadBytes( char *pOutput, int size )
{
	ASSERT( m_pdata !=NULL );

	if ( !m_pdata || Empty() )
		return;

	if ( (m_pdata->size + size) > m_pdata->bufferSize )
	{
		ALERT( at_error, "Restore overflow!" );
		m_pdata->size = m_pdata->bufferSize;
		return;
	}

	if ( pOutput )
		memcpy( pOutput, m_pdata->pCurrentData, size );
	m_pdata->pCurrentData += size;
	m_pdata->size += size;
}


void CRestore::BufferSkipBytes( int bytes )
{
	BufferReadBytes( NULL, bytes );
}

int CRestore::BufferSkipZString( void )
{
	char *pszSearch;
	int	 len;

	if ( !m_pdata )
		return 0;

	int maxLen = m_pdata->bufferSize - m_pdata->size;

	len = 0;
	pszSearch = m_pdata->pCurrentData;
	while ( *pszSearch++ && len < maxLen )
		len++;

	len++;

	BufferSkipBytes( len );

	return len;
}

int	CRestore::BufferCheckZString( const char *string )
{
	if ( !m_pdata )
		return 0;

	int maxLen = m_pdata->bufferSize - m_pdata->size;
	int len = strlen( string );
	if ( len <= maxLen )
	{
		if ( !strncmp( string, m_pdata->pCurrentData, len ) )
			return 1;
	}
	return 0;
}

