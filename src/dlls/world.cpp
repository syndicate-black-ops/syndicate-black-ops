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

===== world.cpp ========================================================

  precaches and defs for entities and other data that must always be available.

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "nodes.h"
#include "soundent.h"
#include "inventory.h"	// SBO 2.0 inventory
#include "client.h"
#include "decals.h"
#include "skill.h"
#include "effects.h"
#include "player.h"
#include "triggers.h"	// SBO 2.0: g_fFogZones
#include "weapons.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "sequence.h"	// SBO 2.0: Sequence_LevelInit
#include "objecthint.h"	// SBO 2.0: ObjectHint_Reset
#include "light_origin.h"	// SBO 2.0: LightOrigin_Reset
#include "gmmode.h"	// SBO 2.0: GmMode_Precache
#include "alias.h"	// SBO 2.1: Alias_Reset

extern CGraph WorldGraph;
extern CSoundEnt *pSoundEnt;

extern CBaseEntity				*g_pLastSpawn;
extern void GlobalMp3_Reset( void );	// sound.cpp
extern void Limitator_ResetForMap( void );	// SBO 2.0: limitator.cpp
extern void GuideMonsterTargets_Reset( void );	// guide_monsters.cpp
DLL_GLOBAL edict_t				*g_pBodyQueueHead;
DLL_GLOBAL string_t				g_iszChapterTitle;	// SBO 2.0: worldspawn "chaptertitle" (CBasePlayer::UpdateClientData)
CGlobalState					gGlobalState;
extern DLL_GLOBAL	int			gDisplayTitle;

extern void W_Precache(void);

//
// This must match the list in util.h
//
DLL_DECALLIST gDecals[] = {
	{ "{shot1",	0 },		// DECAL_GUNSHOT1 
	{ "{shot2",	0 },		// DECAL_GUNSHOT2
	{ "{shot3",0 },			// DECAL_GUNSHOT3
	{ "{shot4",	0 },		// DECAL_GUNSHOT4
	{ "{shot5",	0 },		// DECAL_GUNSHOT5
	{ "{lambda01", 0 },		// DECAL_LAMBDA1
	{ "{lambda02", 0 },		// DECAL_LAMBDA2
	{ "{lambda03", 0 },		// DECAL_LAMBDA3
	{ "{lambda04", 0 },		// DECAL_LAMBDA4
	{ "{lambda05", 0 },		// DECAL_LAMBDA5
	{ "{lambda06", 0 },		// DECAL_LAMBDA6
	{ "{scorch1", 0 },		// DECAL_SCORCH1
	{ "{scorch2", 0 },		// DECAL_SCORCH2
	{ "{blood1", 0 },		// DECAL_BLOOD1
	{ "{blood2", 0 },		// DECAL_BLOOD2
	{ "{blood3", 0 },		// DECAL_BLOOD3
	{ "{blood4", 0 },		// DECAL_BLOOD4
	{ "{blood5", 0 },		// DECAL_BLOOD5
	{ "{blood6", 0 },		// DECAL_BLOOD6
	{ "{yblood1", 0 },		// DECAL_YBLOOD1
	{ "{yblood2", 0 },		// DECAL_YBLOOD2
	{ "{yblood3", 0 },		// DECAL_YBLOOD3
	{ "{yblood4", 0 },		// DECAL_YBLOOD4
	{ "{yblood5", 0 },		// DECAL_YBLOOD5
	{ "{yblood6", 0 },		// DECAL_YBLOOD6
	{ "{break1", 0 },		// DECAL_GLASSBREAK1
	{ "{break2", 0 },		// DECAL_GLASSBREAK2
	{ "{break3", 0 },		// DECAL_GLASSBREAK3
	{ "{bigshot1", 0 },		// DECAL_BIGSHOT1
	{ "{bigshot2", 0 },		// DECAL_BIGSHOT2
	{ "{bigshot3", 0 },		// DECAL_BIGSHOT3
	{ "{bigshot4", 0 },		// DECAL_BIGSHOT4
	{ "{bigshot5", 0 },		// DECAL_BIGSHOT5
	{ "{spit1", 0 },		// DECAL_SPIT1
	{ "{spit2", 0 },		// DECAL_SPIT2
	{ "{bproof1", 0 },		// DECAL_BPROOF1
	{ "{gargstomp", 0 },	// DECAL_GARGSTOMP1,	// Gargantua stomp crack
	{ "{smscorch1", 0 },	// DECAL_SMALLSCORCH1,	// Small scorch mark
	{ "{smscorch2", 0 },	// DECAL_SMALLSCORCH2,	// Small scorch mark
	{ "{smscorch3", 0 },	// DECAL_SMALLSCORCH3,	// Small scorch mark
	{ "{mommablob", 0 },	// DECAL_MOMMABIRTH		// BM Birth spray
	{ "{mommablob", 0 },	// DECAL_MOMMASPLAT		// BM Mortar spray?? need decal
};

/*
==============================================================================

BODY QUE

==============================================================================
*/

extern BOOL g_fSaveRestored;	// SBO: cbase.cpp, DispatchRestore

#define SF_DECAL_NOTINDEATHMATCH		2048

class CDecal : public CBaseEntity
{
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	EXPORT StaticDecal( void );
	void	EXPORT TriggerDecal( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( infodecal, CDecal );

// UNDONE:  These won't get sent to joining players in multi-player
void CDecal :: Spawn( void )
{
	// SBO: the rules decide, not the engine's deathmatch flag (0 in a one-player session)
	if ( pev->skin < 0 || (g_pGameRules->IsDeathmatch() && FBitSet( pev->spawnflags, SF_DECAL_NOTINDEATHMATCH )) )
	{
		REMOVE_ENTITY(ENT(pev));
		return;
	}

	if ( FStringNull ( pev->targetname ) )
	{
		SetThink( &CDecal::StaticDecal );
		// if there's no targetname, the decal will spray itself on as soon as the world is done spawning.
		pev->nextthink = gpGlobals->time;
	}
	else
	{
		// if there IS a targetname, the decal sprays itself on when it is triggered.
		SetThink ( &CDecal::SUB_DoNothing );
		SetUse(&CDecal::TriggerDecal);
	}
}

void CDecal :: TriggerDecal ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// this is set up as a USE function for infodecals that have targetnames, so that the
	// decal doesn't get applied until it is fired. (usually by a scripted sequence)
	TraceResult trace;
	int			entityIndex;

	UTIL_TraceLine( pev->origin - Vector(5,5,5), pev->origin + Vector(5,5,5),  ignore_monsters, ENT(pev), &trace );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE( TE_BSPDECAL );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( (int)pev->skin );
		entityIndex = (short)ENTINDEX(trace.pHit);
		WRITE_SHORT( entityIndex );
		if ( entityIndex )
			WRITE_SHORT( (int)VARS(trace.pHit)->modelindex );
	MESSAGE_END();

	SetThink( &CDecal::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}


void CDecal :: StaticDecal( void )
{
	TraceResult trace;
	int			entityIndex, modelIndex;

	UTIL_TraceLine( pev->origin - Vector(5,5,5), pev->origin + Vector(5,5,5),  ignore_monsters, ENT(pev), &trace );

	entityIndex = (short)ENTINDEX(trace.pHit);
	if ( entityIndex )
		modelIndex = (int)VARS(trace.pHit)->modelindex;
	else
		modelIndex = 0;

	g_engfuncs.pfnStaticDecal( pev->origin, (int)pev->skin, entityIndex, modelIndex );

	SUB_Remove();
}


void CDecal :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->skin = DECAL_INDEX( pkvd->szValue );
		
		// halflife-updated #73: report the keyvalue as handled
		pkvd->fHandled = TRUE;

		// Found
		if ( pev->skin >= 0 )
			return;
		ALERT( at_console, "Can't find decal %s\n", pkvd->szValue );
	}
	else
		CBaseEntity::KeyValue( pkvd );
}


// Body queue class here.... It's really just CBaseEntity
class CCorpse : public CBaseEntity
{
	virtual int ObjectCaps( void ) { return FCAP_DONT_SAVE; }	
};

LINK_ENTITY_TO_CLASS( bodyque, CCorpse );

static void InitBodyQue(void)
{
	string_t	istrClassname = MAKE_STRING("bodyque");

	g_pBodyQueueHead = CREATE_NAMED_ENTITY( istrClassname );
	entvars_t *pev = VARS(g_pBodyQueueHead);
	
	// Reserve 3 more slots for dead bodies
	for ( int i = 0; i < 3; i++ )
	{
		pev->owner = CREATE_NAMED_ENTITY( istrClassname );
		pev = VARS(pev->owner);
	}
	
	pev->owner = g_pBodyQueueHead;
}


//
// make a body que entry for the given ent so the ent can be respawned elsewhere
//
// GLOBALS ASSUMED SET:  g_eoBodyQueueHead
//
void CopyToBodyQue(entvars_t *pev) 
{
	// HL25: nothing to copy if the body is already gone
	if (pev->effects & EF_NODRAW || pev->modelindex == 0 )
		return;

	entvars_t *pevHead	= VARS(g_pBodyQueueHead);

	pevHead->angles		= pev->angles;
	pevHead->model		= pev->model;
	pevHead->modelindex	= pev->modelindex;
	pevHead->frame		= pev->frame;
	pevHead->colormap	= pev->colormap;
	pevHead->movetype	= MOVETYPE_TOSS;
	pevHead->velocity	= pev->velocity;
	pevHead->flags		= 0;
	pevHead->deadflag	= pev->deadflag;
	pevHead->renderfx	= kRenderFxDeadPlayer;
	pevHead->renderamt	= ENTINDEX( ENT( pev ) );

	pevHead->effects    = pev->effects | EF_NOINTERP;
	//pevHead->goalstarttime = pev->goalstarttime;
	//pevHead->goalframe	= pev->goalframe;
	//pevHead->goalendtime = pev->goalendtime ;
	
	pevHead->sequence = pev->sequence;
	pevHead->animtime = pev->animtime;

	UTIL_SetOrigin(pevHead, pev->origin);
	UTIL_SetSize(pevHead, pev->mins, pev->maxs);
	g_pBodyQueueHead = pevHead->owner;
}


CGlobalState::CGlobalState( void )
{
	Reset();
}

void CGlobalState::Reset( void )
{
	m_pList = NULL; 
	m_listCount = 0;
}

globalentity_t *CGlobalState :: Find( string_t globalname )
{
	if ( !globalname )
		return NULL;

	globalentity_t *pTest;
	const char *pEntityName = STRING(globalname);

	
	pTest = m_pList;
	while ( pTest )
	{
		if ( FStrEq( pEntityName, pTest->name ) )
			break;
	
		pTest = pTest->pNext;
	}

	return pTest;
}


// This is available all the time now on impulse 104, remove later
//#ifdef _DEBUG
void CGlobalState :: DumpGlobals( void )
{
	static char *estates[] = { "Off", "On", "Dead" };
	globalentity_t *pTest;

	ALERT( at_console, "-- Globals --\n" );
	pTest = m_pList;
	while ( pTest )
	{
		ALERT( at_console, "%s: %s (%s)\n", pTest->name, pTest->levelName, estates[pTest->state] );
		pTest = pTest->pNext;
	}
}
//#endif


void CGlobalState :: EntityAdd( string_t globalname, string_t mapName, GLOBALESTATE state, int value )
{
	ASSERT( !Find(globalname) );

	globalentity_t *pNewEntity = (globalentity_t *)calloc( sizeof( globalentity_t ), 1 );
	ASSERT( pNewEntity != NULL );
	pNewEntity->pNext = m_pList;
	m_pList = pNewEntity;
	// bounded: a map's "globalname" (or a map name) longer than the fields overflowed the heap block
	strncpy( pNewEntity->name, STRING( globalname ), sizeof( pNewEntity->name ) - 1 );
	strncpy( pNewEntity->levelName, STRING(mapName), sizeof( pNewEntity->levelName ) - 1 );
	pNewEntity->state = state;
	pNewEntity->value = value;
	m_listCount++;
}

// SBO 2.0, from Featureful (CGlobalState::IncrementValue / DecrementValue / SetValue): env_global "Modify/Set value"
void CGlobalState :: EntityChangeValue( string_t globalname, int value, BOOL fAdd )
{
	globalentity_t *pEnt = Find( globalname );

	if ( pEnt )
		pEnt->value = fAdd ? pEnt->value + value : value;
}


void CGlobalState :: EntitySetState( string_t globalname, GLOBALESTATE state )
{
	globalentity_t *pEnt = Find( globalname );

	if ( pEnt )
		pEnt->state = state;
}


const globalentity_t *CGlobalState :: EntityFromTable( string_t globalname )
{
	globalentity_t *pEnt = Find( globalname );

	return pEnt;
}


GLOBALESTATE CGlobalState :: EntityGetState( string_t globalname )
{
	globalentity_t *pEnt = Find( globalname );
	if ( pEnt )
		return pEnt->state;

	return GLOBAL_OFF;
}


// Global Savedata for Delay
TYPEDESCRIPTION	CGlobalState::m_SaveData[] = 
{
	DEFINE_FIELD( CGlobalState, m_listCount, FIELD_INTEGER ),
};

// Global Savedata for Delay
TYPEDESCRIPTION	gGlobalEntitySaveData[] = 
{
	DEFINE_ARRAY( globalentity_t, name, FIELD_CHARACTER, 64 ),
	DEFINE_ARRAY( globalentity_t, levelName, FIELD_CHARACTER, 32 ),
	DEFINE_FIELD( globalentity_t, state, FIELD_INTEGER ),
};


int CGlobalState::Save( CSave &save )
{
	int i;
	globalentity_t *pEntity;

	if ( !save.WriteFields( "GLOBAL", this, m_SaveData, ARRAYSIZE(m_SaveData) ) )
		return 0;
	
	pEntity = m_pList;
	for ( i = 0; i < m_listCount && pEntity; i++ )
	{
		if ( !save.WriteFields( "GENT", pEntity, gGlobalEntitySaveData, ARRAYSIZE(gGlobalEntitySaveData) ) )
			return 0;

		pEntity = pEntity->pNext;
	}
	
	return 1;
}

int CGlobalState::Restore( CRestore &restore )
{
	int i, listCount;
	globalentity_t tmpEntity;


	ClearStates();
	if ( !restore.ReadFields( "GLOBAL", this, m_SaveData, ARRAYSIZE(m_SaveData) ) )
		return 0;
	
	listCount = m_listCount;	// Get new list count
	m_listCount = 0;				// Clear loaded data

	for ( i = 0; i < listCount; i++ )
	{
		if ( !restore.ReadFields( "GENT", &tmpEntity, gGlobalEntitySaveData, ARRAYSIZE(gGlobalEntitySaveData) ) )
			return 0;
		EntityAdd( MAKE_STRING(tmpEntity.name), MAKE_STRING(tmpEntity.levelName), tmpEntity.state );
	}
	return 1;
}

void CGlobalState::EntityUpdate( string_t globalname, string_t mapname )
{
	globalentity_t *pEnt = Find( globalname );

	if ( pEnt )
		strcpy( pEnt->levelName, STRING(mapname) );
}


void CGlobalState::ClearStates( void )
{
	globalentity_t *pFree = m_pList;
	while ( pFree )
	{
		globalentity_t *pNext = pFree->pNext;
		free( pFree );
		pFree = pNext;
	}
	Reset();
}


void SaveGlobalState( SAVERESTOREDATA *pSaveData )
{
	CSave saveHelper( pSaveData );
	gGlobalState.Save( saveHelper );
}


void RestoreGlobalState( SAVERESTOREDATA *pSaveData )
{
	CRestore restoreHelper( pSaveData );
	gGlobalState.Restore( restoreHelper );
}


void ResetGlobalState( void )
{
	gGlobalState.ClearStates();
	gInitHUD = TRUE;	// Init the HUD on a new game / load game
}

// moved CWorld class definition to cbase.h
//=======================
// CWorld
//
// This spawns first when each level begins.
//=======================

LINK_ENTITY_TO_CLASS( worldspawn, CWorld );

#define SF_WORLD_DARK		0x0001		// Fade from black at startup
#define SF_WORLD_TITLE		0x0002		// Display game title at startup
#define SF_WORLD_FORCETEAM	0x0004		// Force teams

extern DLL_GLOBAL BOOL		g_fGameOver;
extern int gUniqueId;
extern int gFUniqueId;

static BOOL s_fMapSkyName = FALSE;	// SBO 2.1: the map being loaded set "skyname" (CWorld::KeyValue, CWorld::Precache)

void CWorld :: Spawn( void )
{
	// halflife-updated: ignore extra "worldspawn" entities (see Precache)
	if ( edict() != INDEXENT( 0 ) )
	{
		Precache( );
		return;
	}

	g_fSaveRestored = FALSE;	// SBO: a map started new, not from a saved game (cbase.cpp, DispatchRestore)
	g_fGameOver = FALSE;
	Precache( );
	gUniqueId = 0;
	gFUniqueId = 0;
}

void CWorld :: Precache( void )
{
	// halflife-updated: only the real world entity (edict 0) may do this; a second "worldspawn"
	// in the map data would otherwise delete and recreate the game rules mid-load
	if ( edict() != INDEXENT( 0 ) )
	{
		ALERT( at_error, "Do not create multiple instances of worldspawn\n" );
		UTIL_Remove( this );
		return;
	}

	extern bool g_worldExists;
	g_worldExists = true;	// SBO 2.0 (Featureful 7a20f521): point_contents (game.cpp)

	g_pLastSpawn = NULL;
	g_fFogZones = FALSE;	// SBO 2.0: set again by this map's env_fogzone entities (triggers.cpp)
	ObjectHint_Reset();		// SBO 2.0: set again by this map's "objecthint" keys (objecthint.cpp)
	LightOrigin_Reset();	// SBO 2.0: set again by this map's "lightorigin" keys (light_origin.cpp)
	GlobalMp3_Reset();		// SevenKewp: no music yet; set again by this map's ambient_music entities (sound.cpp)
	Limitator_ResetForMap();	// SBO 2.0: no Limitator, default reserves; set again by this map's Limitator
	GuideMonsterTargets_Reset();	// SBO 2.0: filled again by this map's monster_targets (guide_monsters.cpp)
	Alias_Reset();			// SBO 2.1: no alias change waits from the last map (alias.cpp)
	Inventory_MapStart();	// SBO 2.0 inventory: a new map starts with no items held anywhere
	UTIL_PrecachedClasses_Reset();	// SBO 2.1: filled again by this map's precaches and entities (util.h)

	// SBO 2.1 (Half-Life Unified SDK, game_end): each map starts with an empty global state table. The engine clears it
	// only on "map", not on "changelevel", so an entity with a "globalname" that was removed (a broken func_breakable)
	// stayed away every time the map came back, and one whose "globalname" another map had used first was made
	// dormant. The world spawns before every other entity, so the map's own entities fill the table again.
	// Saved games are not supported (client.cpp restarts the map), so no transition state is lost.
	gGlobalState.ClearStates();

	
#if 1
	CVAR_SET_STRING("sv_gravity", "800"); // 67ft/sec
	CVAR_SET_STRING("sv_stepsize", "18");
#else
	CVAR_SET_STRING("sv_gravity", "384"); // 32ft/sec
	CVAR_SET_STRING("sv_stepsize", "24");
#endif

	CVAR_SET_STRING("room_type", "0");// clear DSP

	// SBO 2.0: the sky light back to the engine's defaults. Only a light_environment sets these cvars (lights.cpp),
	// so a map without one kept the sky color and direction of the map before it (models lit by the last map's sun).
	// The world spawns before any other entity's keyvalues are read, so the map's own light_environment still wins.
	CVAR_SET_STRING( "sv_skycolor_r", "0" );
	CVAR_SET_STRING( "sv_skycolor_g", "0" );
	CVAR_SET_STRING( "sv_skycolor_b", "0" );
	CVAR_SET_STRING( "sv_skyvec_x", "0" );
	CVAR_SET_STRING( "sv_skyvec_y", "0" );
	CVAR_SET_STRING( "sv_skyvec_z", "0" );

	// SBO 2.1 (Half-Life Unified SDK): a map without a "skyname" gets Half-Life's default sky (desert, the default of
	// the engine's sv_skyname) instead of keeping the sky of the map before it. The worldspawn's keys are read before
	// it spawns, so a map's own "skyname" (CWorld::KeyValue) has already been seen here.
	if ( !s_fMapSkyName )
		CVAR_SET_STRING( "sv_skyname", "desert" );
	s_fMapSkyName = FALSE;	// for the next map


	// Set up game rules
	if (g_pGameRules)
	{
		delete g_pGameRules;
	}

	g_pGameRules = InstallGameRules( );

	//!!!UNDONE why is there so much Spawn code in the Precache function? I'll just keep it here 

	///!!!LATER - do we want a sound ent in deathmatch? (sjb)
	//pSoundEnt = CBaseEntity::Create( "soundent", g_vecZero, g_vecZero, edict() );
	pSoundEnt = GetClassPtr( ( CSoundEnt *)NULL );
	pSoundEnt->Spawn();

	if ( !pSoundEnt )
	{
		ALERT ( at_console, "**COULD NOT CREATE SOUNDENT**\n" );
	}

	InitBodyQue();
	
// init sentence group playback stuff from sentences.txt.
// ok to call this multiple times, calls after first are ignored.

	SENTENCEG_Init();

// init texture type array from materials.txt
// SBO 2.0: the material registry, loaded again at every map start (pm_materials.h, docs/MATERIALS.md)

	TEXTURETYPE_Init();


// the area based ambient sounds MUST be the first precache_sounds

// player precaches     
	W_Precache ();									// get weapon precaches

	ClientPrecache();

	GmMode_Precache();	// SBO 2.0 Game Master mode (every co-op map, whatever mp_gmmode is): the spawn catalog's monsters and the display assets

// sounds used from C physics code
	PRECACHE_SOUND("common/null.wav");				// clears sound channels

	PRECACHE_SOUND( "items/suitchargeok1.wav" );//!!! temporary sound for respawning weapons.
	PRECACHE_SOUND( "items/gunpickup2.wav" );// player picks up a gun.

	PRECACHE_SOUND( "common/bodydrop3.wav" );// dead bodies hitting the ground (animation events)
	PRECACHE_SOUND( "common/bodydrop4.wav" );
	
	g_Language = (int)CVAR_GET_FLOAT( "sv_language" );
	if ( g_Language == LANGUAGE_GERMAN )
	{
		PRECACHE_MODEL( "models/germangibs.mdl" );
	}
	else
	{
		PRECACHE_MODEL( "models/hgibs.mdl" );
		PRECACHE_MODEL( "models/agibs.mdl" );
	}

	PRECACHE_SOUND ("weapons/ric1.wav");
	PRECACHE_SOUND ("weapons/ric2.wav");
	PRECACHE_SOUND ("weapons/ric3.wav");
	PRECACHE_SOUND ("weapons/ric4.wav");
	PRECACHE_SOUND ("weapons/ric5.wav");

	// SBO 2.0: step and hit sounds of the materials this map's textures use (was every HL material in
	// ClientPrecache)
	TEXTURETYPE_PrecacheLevel();

	// SBO 2.0: the map's sequence file (trigger_sequence, docs/SEQUENCES.md): loaded, put in the download table,
	// its sounds precached. After SENTENCEG_Init (it looks sentence groups up) and before any entity spawns.
	Sequence_LevelInit();

//
// Setup light animation tables. 'a' is total darkness, 'z' is maxbright.
//

	// 0 normal
	LIGHT_STYLE(0, "m");
	
	// 1 FLICKER (first variety)
	LIGHT_STYLE(1, "mmnmmommommnonmmonqnmmo");
	
	// 2 SLOW STRONG PULSE
	LIGHT_STYLE(2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");
	
	// 3 CANDLE (first variety)
	LIGHT_STYLE(3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");
	
	// 4 FAST STROBE
	LIGHT_STYLE(4, "mamamamamama");
	
	// 5 GENTLE PULSE 1
	LIGHT_STYLE(5,"jklmnopqrstuvwxyzyxwvutsrqponmlkj");
	
	// 6 FLICKER (second variety)
	LIGHT_STYLE(6, "nmonqnmomnmomomno");
	
	// 7 CANDLE (second variety)
	LIGHT_STYLE(7, "mmmaaaabcdefgmmmmaaaammmaamm");
	
	// 8 CANDLE (third variety)
	LIGHT_STYLE(8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");
	
	// 9 SLOW STROBE (fourth variety)
	LIGHT_STYLE(9, "aaaaaaaazzzzzzzz");
	
	// 10 FLUORESCENT FLICKER
	LIGHT_STYLE(10, "mmamammmmammamamaaamammma");

	// 11 SLOW PULSE NOT FADE TO BLACK
	LIGHT_STYLE(11, "abcdefghijklmnopqrrqponmlkjihgfedcba");
	
	// 12 UNDERWATER LIGHT MUTATION
	// this light only distorts the lightmap - no contribution
	// is made to the brightness of affected surfaces
	LIGHT_STYLE(12, "mmnnmmnnnmmnn");
	
	// styles 32-62 are assigned by the light program for switchable lights

	// 63 testing
	LIGHT_STYLE(63, "a");

	for ( int i = 0; i < ARRAYSIZE(gDecals); i++ )
		gDecals[i].index = DECAL_INDEX( gDecals[i].name );

// init the WorldGraph.
	WorldGraph.InitGraph();

// make sure the .NOD file is newer than the .BSP file.
	if ( !WorldGraph.CheckNODFile ( ( char * )STRING( gpGlobals->mapname ) ) )
	{// NOD file is not present, or is older than the BSP file.
		WorldGraph.AllocNodes ();
	}
	else
	{// Load the node graph for this level
		if ( !WorldGraph.FLoadGraph ( (char *)STRING( gpGlobals->mapname ) ) )
		{// couldn't load, so alloc and prepare to build a graph.
			ALERT ( at_console, "*Error opening .NOD file\n" );
			WorldGraph.AllocNodes ();
		}
		else
		{
			ALERT ( at_console, "\n*Graph Loaded!\n" );
		}
	}

	if ( pev->speed > 0 )
		CVAR_SET_FLOAT( "sv_zmax", pev->speed );
	else
		CVAR_SET_FLOAT( "sv_zmax", 4096 );

	// SBO 2.0: the chapter title goes to every player once, when they first enter the game on this map (players
	// who join later too; CBasePlayer::UpdateClientData). Half-Life (and 1.38) made an env_message 0.3 s into the
	// map with no activator, which showed it to player 1 only.
	g_iszChapterTitle = pev->netname;
	if ( pev->netname )
	{
		ALERT( at_aiconsole, "Chapter title: %s\n", STRING(pev->netname) );
		pev->netname = 0;
	}

	if ( pev->spawnflags & SF_WORLD_DARK )
		CVAR_SET_FLOAT( "v_dark", 1.0 );
	else
		CVAR_SET_FLOAT( "v_dark", 0.0 );

	if ( pev->spawnflags & SF_WORLD_TITLE )
		gDisplayTitle = TRUE;		// display the game title if this key is set
	else
		gDisplayTitle = FALSE;

	if ( pev->spawnflags & SF_WORLD_FORCETEAM )
	{
		CVAR_SET_FLOAT( "mp_defaultteam", 1 );
	}
	else
	{
		CVAR_SET_FLOAT( "mp_defaultteam", 0 );
	}
}


//
// Just to ignore the "wad" field.
//
void CWorld :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq(pkvd->szKeyName, "skyname") )
	{
		// Sent over net now.
		CVAR_SET_STRING( "sv_skyname", pkvd->szValue );
		// SBO 2.1: seen while the map loads (the world has not spawned yet), CWorld::Precache keeps it; else it sets
		// the default sky. A change at run time (trigger_changevalue) does not count for the next map.
		extern bool g_worldExists;
		if ( !g_worldExists )
			s_fMapSkyName = TRUE;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "sounds") )
	{
		gpGlobals->cdAudioTrack = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "WaveHeight") )
	{
		// Sent over net now.
		pev->scale = atof(pkvd->szValue) * (1.0/8.0);
		pkvd->fHandled = TRUE;
		CVAR_SET_FLOAT( "sv_wateramp", pev->scale );
	}
	else if ( FStrEq(pkvd->szKeyName, "MaxRange") )
	{
		pev->speed = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "chaptertitle") )
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "startdark") )
	{
		// UNDONE: This is a gross hack!!! The CVAR is NOT sent over the client/sever link
		// but it will work for single player
		int flag = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
		if ( flag )
			pev->spawnflags |= SF_WORLD_DARK;
	}
	else if ( FStrEq(pkvd->szKeyName, "newunit") )
	{
		// Single player only.  Clear save directory if set
		if ( atoi(pkvd->szValue) )
			CVAR_SET_FLOAT( "sv_newunit", 1 );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "gametitle") )
	{
		if ( atoi(pkvd->szValue) )
			pev->spawnflags |= SF_WORLD_TITLE;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "mapteams") )
	{
		pev->team = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "grassSprite") || FStrEq(pkvd->szKeyName, "grassFadeDist") ||
		FStrEq(pkvd->szKeyName, "grassMinHeight") || FStrEq(pkvd->szKeyName, "grassMaxHeight") ||
		FStrEq(pkvd->szKeyName, "grassDensity") )
	{
		// SBO 2.0: CS:CZ Deleted Scenes' grass on the map's grass textures (sound.cpp, docs/MATERIALS.md)
		TEXTURETYPE_SetGrassKey( pkvd->szKeyName, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "materials_file") )
	{
		// SBO 2.0 (SevenKewp / Sven Co-op): the map's materials file, docs/MATERIALS.md
		TEXTURETYPE_SetMapFile( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "freeroam") )
	{
		// SBO 2.0 (Featureful): monsters with "freeroam" 0 walk around while idle
		if ( atoi(pkvd->szValue) )
			pev->spawnflags |= SF_WORLD_FREEROAM;
		else
			pev->spawnflags &= ~SF_WORLD_FREEROAM;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "defaultteam") )
	{
		if ( atoi(pkvd->szValue) )
		{
			pev->spawnflags |= SF_WORLD_FORCETEAM;
		}
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}
