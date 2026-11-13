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
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"client.h"
#include	"decals.h"
#include	"gamerules.h"
#include	"game.h"
#include	"semiclip.h"
#include	"movewith.h"	// SBO 2.0: entity parenting
#include	"map_compat.h"	// SBO 2.0
#include	"objecthint.h"	// SBO 2.0
#include	"light_origin.h"	// SBO 2.0: "lightorigin"
#include	"client_info.h"	// SBO 2.0: pfnCvarValue2

void EntvarsKeyvalue( entvars_t *pev, KeyValueData *pkvd );

extern "C" void PM_Move ( struct playermove_s *ppmove, int server );
extern "C" void PM_Init ( struct playermove_s *ppmove  );
extern "C" char PM_FindTextureType( char *name );

extern Vector VecBModelOrigin( entvars_t* pevBModel );
extern DLL_GLOBAL Vector		g_vecAttackDir;
extern DLL_GLOBAL int			g_iSkillLevel;
extern entvars_t *g_pevLastInflictor;	// chat-vgui2 (player.cpp)

static DLL_FUNCTIONS gFunctionTable = 
{
	GameDLLInit,				//pfnGameInit
	DispatchSpawn,				//pfnSpawn
	DispatchThink,				//pfnThink
	DispatchUse,				//pfnUse
	DispatchTouch,				//pfnTouch
	DispatchBlocked,			//pfnBlocked
	DispatchKeyValue,			//pfnKeyValue
	DispatchSave,				//pfnSave
	DispatchRestore,			//pfnRestore
	DispatchObjectCollsionBox,	//pfnAbsBox

	SaveWriteFields,			//pfnSaveWriteFields
	SaveReadFields,				//pfnSaveReadFields

	SaveGlobalState,			//pfnSaveGlobalState
	RestoreGlobalState,			//pfnRestoreGlobalState
	ResetGlobalState,			//pfnResetGlobalState

	ClientConnect,				//pfnClientConnect
	ClientDisconnect,			//pfnClientDisconnect
	ClientKill,					//pfnClientKill
	ClientPutInServer,			//pfnClientPutInServer
	ClientCommand,				//pfnClientCommand
	ClientUserInfoChanged,		//pfnClientUserInfoChanged
	ServerActivate,				//pfnServerActivate
	ServerDeactivate,			//pfnServerDeactivate

	PlayerPreThink,				//pfnPlayerPreThink
	PlayerPostThink,			//pfnPlayerPostThink

	StartFrame,					//pfnStartFrame
	ParmsNewLevel,				//pfnParmsNewLevel
	ParmsChangeLevel,			//pfnParmsChangeLevel

	GetGameDescription,         //pfnGetGameDescription    Returns string describing current .dll game.
	PlayerCustomization,        //pfnPlayerCustomization   Notifies .dll of new customization for player.

	SpectatorConnect,			//pfnSpectatorConnect      Called when spectator joins server
	SpectatorDisconnect,        //pfnSpectatorDisconnect   Called when spectator leaves the server
	SpectatorThink,				//pfnSpectatorThink        Called when spectator sends a command packet (usercmd_t)

	Sys_Error,					//pfnSys_Error				Called when engine has encountered an error

	Semiclip_PM_Move,			//pfnPM_Move				SBO 2.0: semiclip, then pm_shared's PM_Move (semiclip.cpp)
	PM_Init,					//pfnPM_Init				Server version of player movement initialization
	PM_FindTextureType,			//pfnPM_FindTextureType
	
	SetupVisibility,			//pfnSetupVisibility        Set up PVS and PAS for networking for this client
	UpdateClientData,			//pfnUpdateClientData       Set up data sent only to specific client
	AddToFullPack,				//pfnAddToFullPack
	CreateBaseline,				//pfnCreateBaseline			Tweak entity baseline for network encoding, allows setup of player baselines, too.
	RegisterEncoders,			//pfnRegisterEncoders		Callbacks for network encoding
	GetWeaponData,				//pfnGetWeaponData
	CmdStart,					//pfnCmdStart
	CmdEnd,						//pfnCmdEnd
	ConnectionlessPacket,		//pfnConnectionlessPacket
	GetHullBounds,				//pfnGetHullBounds
	CreateInstancedBaselines,   //pfnCreateInstancedBaselines
	InconsistentFile,			//pfnInconsistentFile
	AllowLagCompensation,		//pfnAllowLagCompensation
};

// ---- halflife-updated: NEW_DLL_FUNCTIONS (engine/eiface.h) ----
static void OnFreeEntPrivateData( edict_t *pEdict );

// SBO 2.0 (Featureful ShouldCollide, cbase.cpp): the entity being hit decides first (CBaseEntity::ShouldCollide: a
// door's "ignore_corpses" / "handle_tiny_creatures"), then the semiclip
static int DispatchShouldCollide( edict_t *pentTouched, edict_t *pentOther )
{
	if ( pentTouched && pentOther && pentTouched->pvPrivateData && pentOther->pvPrivateData )
	{
		CBaseEntity *pTouched = (CBaseEntity *)GET_PRIVATE( pentTouched );
		CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE( pentOther );
		if ( pTouched && pOther && !pTouched->ShouldCollide( pOther ) )
			return 0;
	}
	return Semiclip_ShouldCollide( pentTouched, pentOther );
}

NEW_DLL_FUNCTIONS gNewDLLFunctions =
{
	OnFreeEntPrivateData,		//pfnOnFreeEntPrivateData	Called right before the engine frees an edict's private data
	NULL,						//pfnGameShutdown
	DispatchShouldCollide,		//pfnShouldCollide			SBO 2.0: entities' ShouldCollide, then semiclip on moving platforms (semiclip.cpp)
	NULL,						//pfnCvarValue
	ClientInfo_CvarValue2,		//pfnCvarValue2			SBO 2.0: answers to the client info queries (client_info.cpp)
};
// ---- end halflife-updated ----

static void SetObjectCollisionBox( entvars_t *pev );

#ifndef _WIN32
extern "C" {
#endif
int GetEntityAPI( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion )
{
	if ( !pFunctionTable || interfaceVersion != INTERFACE_VERSION )
	{
		return FALSE;
	}
	
	memcpy( pFunctionTable, &gFunctionTable, sizeof( DLL_FUNCTIONS ) );
	return TRUE;
}

int GetEntityAPI2( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion )
{
	if ( !pFunctionTable || *interfaceVersion != INTERFACE_VERSION )
	{
		// Tell engine what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}
	
	memcpy( pFunctionTable, &gFunctionTable, sizeof( DLL_FUNCTIONS ) );
	return TRUE;
}

// halflife-updated: the engine (and Metamod) look this export up by name after GetEntityAPI2
int GetNewDLLFunctions( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion )
{
	if ( !pFunctionTable || *interfaceVersion != NEW_DLL_FUNCTIONS_VERSION )
	{
		*interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		return FALSE;
	}

	memcpy( pFunctionTable, &gNewDLLFunctions, sizeof( gNewDLLFunctions ) );
	return TRUE;
}

#ifndef _WIN32
}
#endif

// halflife-updated: run the entity's destructor before the engine frees the edict's private data. SBO keeps the
// SDK allocation (operator new(size, pev) -> ALLOC_PRIVATE), so the memory stays engine-owned: destroy the object
// in place and let the engine free the block (halflife-updated allocates with ::operator new and deletes it here).
bool g_worldExists = false;	// SBO 2.0 (Featureful 7a20f521): a map is running (point_contents, game.cpp)

static void OnFreeEntPrivateData( edict_t *pEdict )
{
	// SBO 2.0 (Featureful 7a20f521): the world goes with the map
	if ( pEdict && pEdict == INDEXENT( 0 ) )
		g_worldExists = false;

	if ( pEdict && pEdict->pvPrivateData )
	{
		CBaseEntity *pEntity = reinterpret_cast<CBaseEntity *>( pEdict->pvPrivateData );

		pEntity->~CBaseEntity();
	}
}


void FgdDefaults_Spawned( edict_t *pent );	// below

int DispatchSpawn( edict_t *pent )
{
	FgdDefaults_Spawned( pent );	// SBO 2.0: later keys (plugins, test_kv) get no FGD defaults
	MapCompat_Spawn( pent );	// SBO 2.0: the entity's keys are all in (map_compat.cpp)

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);

	if (pEntity)
	{
		// Initialize these or entities who don't link to the world won't have anything in here
		pEntity->pev->absmin = pEntity->pev->origin - Vector(1,1,1);
		pEntity->pev->absmax = pEntity->pev->origin + Vector(1,1,1);

		// SBO 2.1: an entity without a model of its own precached what any entity of its class precaches (util.h)
		const BOOL fClassModel = FStringNull( pent->v.model ) || !STRING( pent->v.model )[0];
		const string_t iszClassname = pent->v.classname;

		MoveWith_PreSpawn( pent );	// SBO 2.0 movewith: the editor pose, for children placed relative to it

		pEntity->Spawn();

		// Try to get the pointer again, in case the spawn function deleted the entity.
		// UNDONE: Spawn() should really return a code to ask that the entity be deleted, but
		// that would touch too much code for me to do that right now.
		pEntity = (CBaseEntity *)GET_PRIVATE(pent);

		if ( pEntity )
		{
			MoveWith_PostSpawn( pent );

			if ( g_pGameRules && !g_pGameRules->IsAllowedToSpawn( pEntity ) )
				return -1;	// return that this entity should be deleted
			if ( pEntity->pev->flags & FL_KILLME )
				return -1;
			if ( fClassModel )
				UTIL_PrecachedClasses_Add( STRING( iszClassname ) );
		}


		// Handle global stuff here
		if ( pEntity && pEntity->pev->globalname ) 
		{
			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( pEntity->pev->globalname );
			if ( pGlobal )
			{
				// Already dead? delete
				if ( pGlobal->state == GLOBAL_DEAD )
					return -1;
				else if ( !FStrEq( STRING(gpGlobals->mapname), pGlobal->levelName ) )
					pEntity->MakeDormant();	// Hasn't been moved to this level yet, wait but stay alive
				// In this level & not dead, continue on as normal
			}
			else
			{
				// Spawned entities default to 'On'
				gGlobalState.EntityAdd( pEntity->pev->globalname, gpGlobals->mapname, GLOBAL_ON );
//				ALERT( at_console, "Added global entity %s (%s)\n", STRING(pEntity->pev->classname), STRING(pEntity->pev->globalname) );
			}
		}

	}

	return 0;
}

static void FgdDefaults_Seed( edict_t *pent, KeyValueData *pkvd );	// below DispatchKeyValue

BOOL GamePlayerSettings_PreEntvarsKeyValue( edict_t *pent, KeyValueData *pkvd );	// maprules.cpp
BOOL DLight_PreEntvarsKeyValue( edict_t *pent, KeyValueData *pkvd );	// env_dlight.cpp
BOOL MonsterTarget_PreEntvarsKeyValue( edict_t *pent, KeyValueData *pkvd );	// guide_monsters.cpp

void DispatchKeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd )
{
	if ( !pkvd || !pentKeyvalue )
		return;

	FgdDefaults_Seed( pentKeyvalue, pkvd );	// SBO 2.0: FGD defaults of 2.x entities, before the map's first key

	// SBO 2.0: maps/map_compat.txt drops or overrides keys of custom maps while they load (map_compat.cpp)
	if ( MapCompat_KeyValue( pentKeyvalue, pkvd ) )
		return;

	// Half-Life Unified SDK: an entity's class can't be changed once it is set (a second "classname" key in the
	// map, trigger_changevalue / trigger_copyvalue on "classname"): the entity would keep its class's code but
	// carry another class's name
	if ( !FStringNull( pentKeyvalue->v.classname ) && pkvd->szKeyName && !stricmp( pkvd->szKeyName, "classname" ) )
	{
		ALERT( at_console, "%s: classname \"%s\" ignored (an entity's class can't be changed)\n",
			STRING( pentKeyvalue->v.classname ), pkvd->szValue ? pkvd->szValue : "" );
		return;
	}

	// SBO 2.0 (Featureful PreEntvarsKeyvalue): game_player_settings reads its "health", "max_health" and "armorvalue"
	// with their '+' / '-' / '=' / '<' / '>' prefix, which the entity fields would lose (maprules.cpp)
	if ( GamePlayerSettings_PreEntvarsKeyValue( pentKeyvalue, pkvd ) )
		return;

	// SBO 2.0: env_dlight / env_elight note whether the map set "renderamt" / "rendercolor" (left out: the FGD defaults)
	if ( DLight_PreEntvarsKeyValue( pentKeyvalue, pkvd ) )
		return;

	// SBO 2.0: monster_target notes whether the map set "frags" (left out: Player ally)
	if ( MonsterTarget_PreEntvarsKeyValue( pentKeyvalue, pkvd ) )
		return;

	EntvarsKeyvalue( VARS(pentKeyvalue), pkvd );

	// If the key was an entity variable, or there's no class set yet, don't look for the object, it may
	// not exist yet.
	if ( pkvd->fHandled || pkvd->szClassName == NULL )
		return;

	// SevenKewp (Sven Co-op): custom keyvalues "$i_x", "$f_x", "$v_x", "$s_x" on any entity (keyvalue.cpp)
	if ( KeyValue_SetCustom( pentKeyvalue, pkvd->szKeyName, pkvd->szValue ) )
	{
		pkvd->fHandled = TRUE;
		return;
	}

	// SBO 2.0: "movewith" / "parent" / "parentflags" on any entity (movewith.cpp)
	if ( MoveWith_KeyValue( pentKeyvalue, pkvd ) )
	{
		pkvd->fHandled = TRUE;
		return;
	}

	// Get the actualy entity object
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentKeyvalue);

	if ( !pEntity )
		return;

	// SBO 2.0 object hints (Featureful CBaseEntity::KeyValue): "objecthint" on any entity names its hint template,
	// "objecthint_distance" sets its scan distance
	if ( ObjectHint_KeyValue( pEntity, pkvd->szKeyName, pkvd->szValue ) )
	{
		pkvd->fHandled = TRUE;
		return;
	}

	// SBO 2.0 (Trinity renderer): "lightorigin" on any model entity names the info_light_origin it is lit from
	// (light_origin.cpp); env_static reads it in its own KeyValue
	if ( !FClassnameIs( pentKeyvalue, "env_static" ) && LightOrigin_KeyValue( pEntity, pkvd->szKeyName, pkvd->szValue ) )
	{
		pkvd->fHandled = TRUE;
		return;
	}

	pEntity->KeyValue( pkvd );
}

// SBO 2.0: the FGD defaults of SBO 2.x entities whose code reads a key left out as something else (mostly 0). A map
// entity of one of these classes gets them before its first key, as if the map gave them, so every key the map does
// give overrides them. Only keys before the entity spawns (a map's, ent_create's); an entity the game code makes
// with its own Spawn keeps its code's values, and keys after the spawn (plugins, test_kv) get no defaults. Only 2.x classes, and only keys no stock or custom map leaves out.
struct FgdDefault_t { const char *classname; const char *key; const char *value; };
static const FgdDefault_t s_FgdDefaults[] =
{
	{ "ambient_random", "health", "10" }, { "ambient_random", "pitch", "100" }, { "ambient_random", "pitchstart", "100" },
	{ "env_beamtrail", "rendercolor", "255 255 255" }, { "env_beamtrail", "armorvalue", "5" }, { "env_beamtrail", "health", "4.0" },
	{ "env_fog", "rendercolor", "128 128 128" },
	{ "env_fogzone", "rendercolor", "128 128 128" },
	{ "env_mirror", "rendermode", "2" }, { "env_mirror", "renderamt", "100" },
	{ "env_modeltrain", "speed", "64" },
	{ "env_overlay", "x", "-1" }, { "env_overlay", "y", "-1" }, { "env_overlay", "rendercolor", "255 255 255" },
	{ "env_overlay", "framerate", "10" }, { "env_overlay", "fadein", "0.5" }, { "env_overlay", "fadeout", "0.5" },
	{ "env_particle", "message", "aurora/steamvent.aur" },
	{ "env_quakefx", "impulse", "4" }, { "env_quakefx", "frags", "70" }, { "env_quakefx", "armortype", "250" },
	{ "env_quakefx", "health", "1.0" }, { "env_quakefx", "armorvalue", "10" },
	{ "env_shockwave", "rendercolor", "188 220 255" },
	{ "env_soundmark", "radius", "384" }, { "env_soundmark", "duration", "0.3" },
	{ "env_spritetrain", "model", "sprites/glow01.spr" }, { "env_spritetrain", "framerate", "10.0" }, { "env_spritetrain", "speed", "64" },
	{ "env_static", "framerate", "1.0" }, { "env_static", "scale", "1.0" },
	{ "env_warpball", "scale", "1" },
	{ "func_illusionary_toggle", "skin", "-1" },
	{ "func_rainvolume", "raindrop_width", "2" }, { "func_rainvolume", "raindrop_height", "80" },
	{ "func_rainvolume", "raindrop_min_speed", "500" }, { "func_rainvolume", "raindrop_max_speed", "1800" },
	{ "func_rainvolume", "raindrop_life", "1.0" },
	{ "func_snowvolume", "snowflake_min_size", "2.1" }, { "func_snowvolume", "snowflake_max_size", "2.6" },
	{ "func_snowvolume", "snowflake_min_speed", "131" }, { "func_snowvolume", "snowflake_max_speed", "210" },
	{ "func_snowvolume", "snowflake_life", "3.0" },
	{ "func_vehicle", "length", "256" }, { "func_vehicle", "width", "128" }, { "func_vehicle", "height", "4" },
	{ "scripted_schedule", "schedule", "1" },
	{ "trigger_compare", "initial_value", "0" }, { "trigger_compare", "compare_value", "0" },
	{ "trigger_hurt_remote", "dmg", "10" },
	{ "trigger_look", "look_time", "1" }, { "trigger_look", "field_of_view", "0.7" },
	{ "trigger_sensor", "radius", "192" }, { "trigger_sensor", "line_blocked", "2" }, { "trigger_sensor", "trace_to", "1" },
	{ "trigger_timer", "min_delay", "0.5" }, { "trigger_timer", "max_delay", "1.0" },
};

#define FGD_DEFAULTS_EDICTS	16384
static int s_iFgdDefaultsSerial[FGD_DEFAULTS_EDICTS];	// the edict's serial number + 1 once it has its defaults
static int s_iFgdSpawnedSerial[FGD_DEFAULTS_EDICTS];	// the edict's serial number + 1 once it spawned

void FgdDefaults_Spawned( edict_t *pent )
{
	int index = ENTINDEX( pent );
	if ( index >= 0 && index < FGD_DEFAULTS_EDICTS )
		s_iFgdSpawnedSerial[index] = pent->serialnumber + 1;
}

static void FgdDefaults_Seed( edict_t *pent, KeyValueData *pkvd )
{
	if ( !pkvd->szClassName )
		return;
	int index = ENTINDEX( pent );
	if ( index < 0 || index >= FGD_DEFAULTS_EDICTS || s_iFgdDefaultsSerial[index] == pent->serialnumber + 1
		|| s_iFgdSpawnedSerial[index] == pent->serialnumber + 1 )
		return;
	BOOL bListed = FALSE;
	for ( int i = 0; i < (int)ARRAYSIZE( s_FgdDefaults ) && !bListed; i++ )
		bListed = FStrEq( s_FgdDefaults[i].classname, pkvd->szClassName );
	if ( !bListed )
		return;
	s_iFgdDefaultsSerial[index] = pent->serialnumber + 1;
	for ( int i = 0; i < (int)ARRAYSIZE( s_FgdDefaults ); i++ )
	{
		if ( !FStrEq( s_FgdDefaults[i].classname, pkvd->szClassName ) )
			continue;
		KeyValueData kvd;
		kvd.szClassName = pkvd->szClassName;
		kvd.szKeyName = (char *)s_FgdDefaults[i].key;
		kvd.szValue = (char *)s_FgdDefaults[i].value;
		kvd.fHandled = FALSE;
		DispatchKeyValue( pent, &kvd );
	}
}


// HACKHACK -- this is a hack to keep the node graph entity from "touching" things (like triggers)
// while it builds the graph
BOOL gTouchDisabled = FALSE;
void DispatchTouch( edict_t *pentTouched, edict_t *pentOther )
{
#ifdef SBO_TEST_COMMANDS
	MoveWith_TraceTouch( pentTouched, pentOther );	// test_mw_trace: what a traced player touches
#endif
	if ( gTouchDisabled )
		return;

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentTouched);
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE( pentOther );

	if ( pEntity && pOther && ! ((pEntity->pev->flags | pOther->pev->flags) & FL_KILLME) )
		pEntity->Touch( pOther );
}


void DispatchUse( edict_t *pentUsed, edict_t *pentOther )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentUsed);
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE(pentOther);

	if (pEntity && !(pEntity->pev->flags & FL_KILLME) )
		pEntity->Use( pOther, pOther, USE_TOGGLE, 0 );
}

void DispatchThink( edict_t *pent )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);
	if (pEntity)
	{
		if ( FBitSet( pEntity->pev->flags, FL_DORMANT ) )
			ALERT( at_error, "Dormant entity %s is thinking!!\n", STRING(pEntity->pev->classname) );
				
		pEntity->Think();
	}
}

void MoverPrediction_Blocked( edict_t *pent );	// client.cpp

void DispatchBlocked( edict_t *pentBlocked, edict_t *pentOther )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( pentBlocked );
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE( pentOther );

	// SBO 2.0 mover prediction: the engine leaves a blocked pusher's velocity as it was; the state sent to the
	// clients says it stands (client.cpp MoverPrediction_FillState). The engine calls only this when it blocks one.
	MoverPrediction_Blocked( pentBlocked );

	// SBO 2.0 movewith: a blocked child tells the top of its chain (a door reverses, a train hurts)
	if ( pEntity && !MoveWith_Blocked( pEntity, pOther ) )
		pEntity->Blocked( pOther );
}

void DispatchSave( edict_t *pent, SAVERESTOREDATA *pSaveData )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);
	
	if ( pEntity && pSaveData )
	{
		ENTITYTABLE *pTable = &pSaveData->pTable[ pSaveData->currentIndex ];

		if ( pTable->pent != pent )
			ALERT( at_error, "ENTITY TABLE OR INDEX IS WRONG!!!!\n" );

		if ( pEntity->ObjectCaps() & FCAP_DONT_SAVE )
			return;

		// These don't use ltime & nextthink as times really, but we'll fudge around it.
		if ( pEntity->pev->movetype == MOVETYPE_PUSH )
		{
			float delta = pEntity->pev->nextthink - pEntity->pev->ltime;
			pEntity->pev->ltime = gpGlobals->time;
			pEntity->pev->nextthink = pEntity->pev->ltime + delta;
		}

		pTable->location = pSaveData->size;		// Remember entity position for file I/O
		pTable->classname = pEntity->pev->classname;	// Remember entity class for respawn

		CSave saveHelper( pSaveData );
		pEntity->Save( saveHelper );

		pTable->size = pSaveData->size - pTable->location;	// Size of entity block is data size written to block
	}
}


// Find the matching global entity.  Spit out an error if the designer made entities of
// different classes with the same global name
CBaseEntity *FindGlobalEntity( string_t classname, string_t globalname )
{
	edict_t *pent = FIND_ENTITY_BY_STRING( NULL, "globalname", STRING(globalname) );
	CBaseEntity *pReturn = CBaseEntity::Instance( pent );
	if ( pReturn )
	{
		if ( !FClassnameIs( pReturn->pev, STRING(classname) ) )
		{
			ALERT( at_console, "Global entity found %s, wrong class %s\n", STRING(globalname), STRING(pReturn->pev->classname) );
			pReturn = NULL;
		}
	}

	return pReturn;
}


// SBO: set when the engine restores entities from a saved game. Only a one-player session ("maxplayers 1") can save
// and load (the engine's own commands; the game has no hook to refuse them), and a saved game holds none of SBO's
// state (rounds, loadouts, the modes' player fields): StartFrame starts the map again instead of playing it on.
BOOL g_fSaveRestored = FALSE;

int DispatchRestore( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);

	if ( pEntity && pSaveData )
	{
		g_fSaveRestored = TRUE;	// SBO: see above

		entvars_t tmpVars;
		Vector oldOffset;

		CRestore restoreHelper( pSaveData );
		if ( globalEntity )
		{
			CRestore tmpRestore( pSaveData );
			tmpRestore.PrecacheMode( 0 );
			tmpRestore.ReadEntVars( "ENTVARS", &tmpVars );

			// HACKHACK - reset the save pointers, we're going to restore for real this time
			pSaveData->size = pSaveData->pTable[pSaveData->currentIndex].location;
			pSaveData->pCurrentData = pSaveData->pBaseData + pSaveData->size;
			// -------------------


			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( tmpVars.globalname );
			
			// Don't overlay any instance of the global that isn't the latest
			// pSaveData->szCurrentMapName is the level this entity is coming from
			// pGlobla->levelName is the last level the global entity was active in.
			// If they aren't the same, then this global update is out of date.
			if ( !FStrEq( pSaveData->szCurrentMapName, pGlobal->levelName ) )
				return 0;

			// Compute the new global offset
			oldOffset = pSaveData->vecLandmarkOffset;
			CBaseEntity *pNewEntity = FindGlobalEntity( tmpVars.classname, tmpVars.globalname );
			if ( pNewEntity )
			{
//				ALERT( at_console, "Overlay %s with %s\n", STRING(pNewEntity->pev->classname), STRING(tmpVars.classname) );
				// Tell the restore code we're overlaying a global entity from another level
				restoreHelper.SetGlobalMode( 1 );	// Don't overwrite global fields
				pSaveData->vecLandmarkOffset = (pSaveData->vecLandmarkOffset - pNewEntity->pev->mins) + tmpVars.mins;
				pEntity = pNewEntity;// we're going to restore this data OVER the old entity
				pent = ENT( pEntity->pev );
				// Update the global table to say that the global definition of this entity should come from this level
				gGlobalState.EntityUpdate( pEntity->pev->globalname, gpGlobals->mapname );
			}
			else
			{
				// This entity will be freed automatically by the engine.  If we don't do a restore on a matching entity (below)
				// or call EntityUpdate() to move it to this level, we haven't changed global state at all.
				return 0;
			}

		}

		if ( pEntity->ObjectCaps() & FCAP_MUST_SPAWN )
		{
			pEntity->Restore( restoreHelper );
			pEntity->Spawn();
		}
		else
		{
			pEntity->Restore( restoreHelper );
			pEntity->Precache( );
		}

		// Again, could be deleted, get the pointer again.
		pEntity = (CBaseEntity *)GET_PRIVATE(pent);

#if 0
		if ( pEntity && pEntity->pev->globalname && globalEntity ) 
		{
			ALERT( at_console, "Global %s is %s\n", STRING(pEntity->pev->globalname), STRING(pEntity->pev->model) );
		}
#endif

		// Is this an overriding global entity (coming over the transition), or one restoring in a level
		if ( globalEntity )
		{
//			ALERT( at_console, "After: %f %f %f %s\n", pEntity->pev->origin.x, pEntity->pev->origin.y, pEntity->pev->origin.z, STRING(pEntity->pev->model) );
			pSaveData->vecLandmarkOffset = oldOffset;
			if ( pEntity )
			{
				UTIL_SetOrigin( pEntity->pev, pEntity->pev->origin );
				pEntity->OverrideReset();
			}
		}
		else if ( pEntity && pEntity->pev->globalname ) 
		{
			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( pEntity->pev->globalname );
			if ( pGlobal )
			{
				// Already dead? delete
				if ( pGlobal->state == GLOBAL_DEAD )
					return -1;
				else if ( !FStrEq( STRING(gpGlobals->mapname), pGlobal->levelName ) )
				{
					pEntity->MakeDormant();	// Hasn't been moved to this level yet, wait but stay alive
				}
				// In this level & not dead, continue on as normal
			}
			else
			{
				ALERT( at_error, "Global Entity %s (%s) not in table!!!\n", STRING(pEntity->pev->globalname), STRING(pEntity->pev->classname) );
				// Spawned entities default to 'On'
				gGlobalState.EntityAdd( pEntity->pev->globalname, gpGlobals->mapname, GLOBAL_ON );
			}
		}
	}
	return 0;
}


void DispatchObjectCollsionBox( edict_t *pent )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);
	if (pEntity)
	{
		pEntity->SetObjectCollisionBox();
	}
	else
		SetObjectCollisionBox( &pent->v );
}


void SaveWriteFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	CSave saveHelper( pSaveData );
	saveHelper.WriteFields( pname, pBaseData, pFields, fieldCount );
}


void SaveReadFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	CRestore restoreHelper( pSaveData );
	restoreHelper.ReadFields( pname, pBaseData, pFields, fieldCount );
}


edict_t * EHANDLE::Get( void ) 
{ 
	if (m_pent)
	{
		if (m_pent->serialnumber == m_serialnumber) 
			return m_pent; 
		else
			return NULL;
	}
	return NULL; 
};

edict_t * EHANDLE::Set( edict_t *pent ) 
{ 
	m_pent = pent;  
	if (pent) 
		m_serialnumber = m_pent->serialnumber; 
	return pent; 
};


EHANDLE :: operator CBaseEntity *() 
{ 
	return (CBaseEntity *)GET_PRIVATE( Get( ) ); 
};


CBaseEntity * EHANDLE :: operator = (CBaseEntity *pEntity)
{
	if (pEntity)
	{
		m_pent = ENT( pEntity->pev );
		if (m_pent)
			m_serialnumber = m_pent->serialnumber;
	}
	else
	{
		m_pent = NULL;
		m_serialnumber = 0;
	}
	return pEntity;
}

EHANDLE :: operator int ()
{
	return Get() != NULL;
}

CBaseEntity * EHANDLE :: operator -> ()
{
	return (CBaseEntity *)GET_PRIVATE( Get( ) ); 
}


// give health
int CBaseEntity :: TakeHealth( float flHealth, int bitsDamageType )
{
	if (!pev->takedamage)
		return 0;

// heal
	if ( pev->health >= pev->max_health )
		return 0;

	pev->health += flHealth;

	if (pev->health > pev->max_health)
		pev->health = pev->max_health;

	return 1;
}

// inflict damage on this entity.  bitsDamageType indicates type of damage inflicted, ie: DMG_CRUSH

int CBaseEntity :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	Vector			vecTemp;

	if (!pev->takedamage)
		return 0;

	// UNDONE: some entity types may be immune or resistant to some bitsDamageType
	
	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin). 
	if ( pevAttacker == pevInflictor )	
	{
		vecTemp = pevInflictor->origin - ( VecBModelOrigin(pev) );
	}
	else
	// an actual missile was involved.
	{
		vecTemp = pevInflictor->origin - ( VecBModelOrigin(pev) );
	}

// this global is still used for glass and other non-monster killables, along with decals.
	g_vecAttackDir = vecTemp.Normalize();
		
// save damage based on the target's armor level

// figure momentum add (don't let hurt brushes or other triggers move player)
	if ((!FNullEnt(pevInflictor)) && (pev->movetype == MOVETYPE_WALK || pev->movetype == MOVETYPE_STEP) && (pevAttacker->solid != SOLID_TRIGGER) )
	{
		Vector vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();

		float flForce = flDamage * ((32 * 32 * 72.0) / (pev->size.x * pev->size.y * pev->size.z)) * 5;
		
		if (flForce > 1000.0) 
			flForce = 1000.0;
		pev->velocity = pev->velocity + vecDir * flForce;
	}

// do the damage
	pev->health -= flDamage;
	if (pev->health <= 0)
	{
		entvars_t *pevOldInflictor = g_pevLastInflictor;	// chat-vgui2: kill feed weapon (SboKillfeed_NpcKilled)
		g_pevLastInflictor = pevInflictor;
		Killed( pevAttacker, GIB_NORMAL );
		g_pevLastInflictor = pevOldInflictor;
		return 0;
	}

	return 1;
}


void CBaseEntity :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	UTIL_Remove( this );
}


CBaseEntity *CBaseEntity::GetNextTarget( void )
{
	if ( FStringNull( pev->target ) )
		return NULL;
	edict_t *pTarget = FIND_ENTITY_BY_TARGETNAME ( NULL, STRING(pev->target) );
	if ( FNullEnt(pTarget) )
		return NULL;

	return Instance( pTarget );
}

// Global Savedata for Delay
TYPEDESCRIPTION	CBaseEntity::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseEntity, m_pGoalEnt, FIELD_CLASSPTR ),

	DEFINE_FIELD( CBaseEntity, m_pfnThink, FIELD_FUNCTION ),		// UNDONE: Build table of these!!!
	DEFINE_FIELD( CBaseEntity, m_pfnTouch, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseEntity, m_pfnUse, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseEntity, m_pfnBlocked, FIELD_FUNCTION ),

	// SBO 2.0 model scale (UTIL_SetSize)
	DEFINE_FIELD( CBaseEntity, m_vecSizeMins, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseEntity, m_vecSizeMaxs, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseEntity, m_flSizeScale, FIELD_FLOAT ),

	DEFINE_FIELD( CBaseEntity, m_objectHint, FIELD_STRING ),	// SBO 2.0 object hints
	DEFINE_FIELD( CBaseEntity, m_flObjectHintDistance, FIELD_FLOAT ),	// SBO 2.0 object hints
};


int CBaseEntity::Save( CSave &save )
{
	if ( save.WriteEntVars( "ENTVARS", pev ) )
		return save.WriteFields( "BASE", this, m_SaveData, ARRAYSIZE(m_SaveData) );

	return 0;
}

int CBaseEntity::Restore( CRestore &restore )
{
	int status;

	status = restore.ReadEntVars( "ENTVARS", pev );
	if ( status )
		status = restore.ReadFields( "BASE", this, m_SaveData, ARRAYSIZE(m_SaveData) );

    if ( pev->modelindex != 0 && !FStringNull(pev->model) )
	{
		Vector mins, maxs;
		UTIL_GetUnscaledSize( pev, mins, maxs );	// Set model is about to destroy these (SBO 2.0: at scale 1, UTIL_SetSize scales them)


		PRECACHE_MODEL( (char *)STRING(pev->model) );
		SET_MODEL(ENT(pev), STRING(pev->model));
		UTIL_SetSize(pev, mins, maxs);	// Reset them
	}

	return status;
}


// Initialize absmin & absmax to the appropriate box
void SetObjectCollisionBox( entvars_t *pev )
{
	// SBO 2.0 movewith: a turning brush child gets the box of its turned bounds, not the cube below around its
	// origin, which for a child without an origin brush is far from its brushes (movewith.cpp)
	if ( MoveWith_CollisionBox( pev ) )
		return;

	if ( (pev->solid == SOLID_BSP) && 
		 (pev->angles.x || pev->angles.y|| pev->angles.z) )
	{	// expand for rotation
		float		max, v;
		int			i;

		max = 0;
		for (i=0 ; i<3 ; i++)
		{
			v = fabs( ((float *)pev->mins)[i]);
			if (v > max)
				max = v;
			v = fabs( ((float *)pev->maxs)[i]);
			if (v > max)
				max = v;
		}
		for (i=0 ; i<3 ; i++)
		{
			((float *)pev->absmin)[i] = ((float *)pev->origin)[i] - max;
			((float *)pev->absmax)[i] = ((float *)pev->origin)[i] + max;
		}
	}
	else
	{
		pev->absmin = pev->origin + pev->mins;
		pev->absmax = pev->origin + pev->maxs;
	}

	pev->absmin.x -= 1;
	pev->absmin.y -= 1;
	pev->absmin.z -= 1;
	pev->absmax.x += 1;
	pev->absmax.y += 1;
	pev->absmax.z += 1;
}


void CBaseEntity::SetObjectCollisionBox( void )
{
	// SBO 2.0 model scale: while a movement wrapper (UTIL_WalkMove, ...) has the box at scale 1 in pev for the
	// engine's move, the engine links the entity at the end of the move: absmin / absmax stay the scaled box's
	if ( m_fUnscaledHullMove )
	{
		Vector vecMins = pev->mins, vecMaxs = pev->maxs;
		UTIL_ScaleBox( m_vecSizeMins, m_vecSizeMaxs, m_flSizeScale, pev->mins, pev->maxs );
		::SetObjectCollisionBox( pev );
		pev->mins = vecMins;
		pev->maxs = vecMaxs;
		return;
	}

	::SetObjectCollisionBox( pev );
}


int	CBaseEntity :: Intersects( CBaseEntity *pOther )
{
	if ( pOther->pev->absmin.x > pev->absmax.x ||
		 pOther->pev->absmin.y > pev->absmax.y ||
		 pOther->pev->absmin.z > pev->absmax.z ||
		 pOther->pev->absmax.x < pev->absmin.x ||
		 pOther->pev->absmax.y < pev->absmin.y ||
		 pOther->pev->absmax.z < pev->absmin.z )
		 return 0;
	return 1;
}

void CBaseEntity :: MakeDormant( void )
{
	SetBits( pev->flags, FL_DORMANT );
	
	// Don't touch
	pev->solid = SOLID_NOT;
	// Don't move
	pev->movetype = MOVETYPE_NONE;
	// Don't draw
	SetBits( pev->effects, EF_NODRAW );
	// Don't think
	pev->nextthink = 0;
	// Relink
	UTIL_SetOrigin( pev, pev->origin );
}

int CBaseEntity :: IsDormant( void )
{
	return FBitSet( pev->flags, FL_DORMANT );
}

BOOL CBaseEntity :: IsInWorld( void )
{
	// position 
	if (pev->origin.x >= 4096) return FALSE;
	if (pev->origin.y >= 4096) return FALSE;
	if (pev->origin.z >= 4096) return FALSE;
	if (pev->origin.x <= -4096) return FALSE;
	if (pev->origin.y <= -4096) return FALSE;
	if (pev->origin.z <= -4096) return FALSE;
	// speed
	if (pev->velocity.x >= 2000) return FALSE;
	if (pev->velocity.y >= 2000) return FALSE;
	if (pev->velocity.z >= 2000) return FALSE;
	if (pev->velocity.x <= -2000) return FALSE;
	if (pev->velocity.y <= -2000) return FALSE;
	if (pev->velocity.z <= -2000) return FALSE;

	return TRUE;
}

int CBaseEntity::ShouldToggle( USE_TYPE useType, BOOL currentState )
{
	if ( useType != USE_TOGGLE && useType != USE_SET )
	{
		if ( (currentState && useType == USE_ON) || (!currentState && useType == USE_OFF) )
			return 0;
	}
	return 1;
}


int	CBaseEntity :: DamageDecal( int bitsDamageType )
{
	if ( pev->rendermode == kRenderTransAlpha )
		return -1;

	if ( pev->rendermode != kRenderNormal )
		return DECAL_BPROOF1;

	return DECAL_GUNSHOT1 + RANDOM_LONG(0,4);
}



// NOTE: szName must be a pointer to constant memory, e.g. "monster_class" because the entity
// will keep a pointer to it after this call.
CBaseEntity * CBaseEntity::Create( char *szName, const Vector &vecOrigin, const Vector &vecAngles, edict_t *pentOwner )
{
	edict_t	*pent;
	CBaseEntity *pEntity;

	pent = CREATE_NAMED_ENTITY( MAKE_STRING( szName ));
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in Create!\n" );
		return NULL;
	}
	pEntity = Instance( pent );
	pEntity->pev->owner = pentOwner;
	pEntity->pev->origin = vecOrigin;
	pEntity->pev->angles = vecAngles;
	DispatchSpawn( pEntity->edict() );
	return pEntity;
}


