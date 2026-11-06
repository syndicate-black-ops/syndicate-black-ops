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
//=========================================================
// Monster Maker - this is an entity that creates monsters
// in the game.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "saverestore.h"
#include "locus.h"		// SBO 2.0
#include "round_hooks.h"	// SBO 2.0: Logic_IsPlayerInPlay
#include "guide_entities.h"	// SBO 2.0: "warpball"

// Monstermaker spawnflags
#define	SF_MONSTERMAKER_START_ON	1 // start active ( if has targetname )
#define	SF_MONSTERMAKER_CYCLIC		4 // drop one monster every time fired.
#define SF_MONSTERMAKER_MONSTERCLIP	8 // Children are blocked by monsterclip
// SBO 2.0 (Featureful monstermaker.cpp): the flags given on to the children, the box test and the cyclic backlog
#define SF_MONSTERMAKER_PRISONER			16		// children: Prisoner
#define SF_MONSTERMAKER_AUTOSIZEBBOX		32		// the free-spot test uses the child's own size
#define SF_MONSTERMAKER_CYCLIC_BACKLOG		64		// cyclic: a trigger while blocked is kept and made later
#define SF_MONSTERMAKER_PREDISASTER			256		// children: Pre-Disaster
#define SF_MONSTERMAKER_WAIT_UNTIL_PROVOKED	8192	// children: monster flag 64 (wait until provoked)
#define SF_MONSTERMAKER_SPECIAL_FLAG		32768	// children: monster flag 32768
#define SF_MONSTERMAKER_NONSOLID_CORPSE		65536	// children: non-solid corpse
#define SF_MONSTERMAKER_IGNORE_PUSH			524288	// children: ignore move-away requests (player pushes)
#define SF_MONSTERMAKER_ACT_OUT_OF_PVS		1048576	// children: act out of the players' PVS

// SBO 2.0 (Featureful): the results of MakeMonster
enum
{
	MONSTERMAKER_LIMIT = 0,
	MONSTERMAKER_BLOCKED,
	MONSTERMAKER_NULLENTITY,
	MONSTERMAKER_SPAWNED,
	MONSTERMAKER_BADPLACE,
};

//=========================================================
// MonsterMaker - this ent creates monsters during the game.
//=========================================================
class CMonsterMaker : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData* pkvd);
	void EXPORT ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT CyclicUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT MakerThink ( void );
	void DeathNotice ( entvars_t *pevChild );// monster maker children use this to tell the monster maker that they have died.
	int MakeMonster( void );		// SBO 2.0: a MONSTERMAKER_* result (Featureful)
	CBaseEntity *SpawnChild( const Vector &placePosition, const Vector &placeAngles );
	void EXPORT CyclicBacklogThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];
	
	string_t m_iszMonsterClassname;// classname of the monster(s) that will be created.
	
	int	 m_cNumMonsters;// max number of monsters this ent can create

	
	int  m_cLiveChildren;// how many monsters made by this monster maker that are currently alive
	int	 m_iMaxLiveChildren;// max number of monsters that this maker may have out at one time.

	float m_flGround; // z coord of the ground under me, used to make sure no monsters are under the maker when it drops a new child

	BOOL m_fActive;
	BOOL m_fFadeChildren;// should we make the children fadeout?

	// SBO: keyvalues passed on to every child before it spawns (replaces the
	// monstermaker_weapons plugin). "weapons" needs no member: the engine
	// stores it in the maker's own pev->weapons. The maker's own netname,
	// target, TriggerTarget and TriggerCondition keep their stock meaning,
	// hence the child* names.
	string_t m_iszChildSquadName;	// "squadname": child netname (squad)
	string_t m_iszChildTarget;		// "childtarget": child target (path_corner to walk to)
	string_t m_iszChildTriggerTarget;	// "childtriggertarget": child TriggerTarget
	int		 m_iChildTriggerCondition;	// "childtriggercondition": child TriggerCondition
	string_t m_iszChildFf;			// "Ff": given to the child's own KeyValue (grunts, assassins, turrets)

	// SBO 2.0, from Featureful: "spawnorigin" [LP] where the child appears (empty: here), "face_position" [LP] the
	// point it faces ("*player": the nearest player in play); computed for the activator of the last trigger
	string_t m_iszPlacePosition;
	string_t m_positionToFaceTo;
	EHANDLE  m_hLocusActivator;
	int		 m_iChildSpawnflags;	// SBO 2.0 "childspawnflags": added to the child's spawnflags (e.g. the apache's "Fire Rockets")
	// SBO 2.0, from Featureful: "warpball" names an env_warpball that plays where each child appears; "impulse"
	// ("Warp at monster's...") -1 its origin, 0 / 1 its center
	string_t m_iszWarpball;

	int ChildSpawnflags( void );	// every spawnflag a child gets (MakeMonster)

	// SBO 2.0, from Featureful: "spawndelay" (the child appears this long after the spot is
	// taken: an invisible box holds it), "delay_after_blocked" (the next try after a blocked one), "yawdeviation"
	// (a random turn up to this many degrees), "target_activator" (-1 none, 0 / 1 the maker, 2 the child, 3 the
	// maker's activator), "trigger_on_death_notice" (fired when a child dies), "respawn_as_playerally" (children
	// with "is_player_ally"), "make_blocker_move_away" (1 walk, 2 run: a monster in the way is asked to move),
	// "gag" (children: Gag); the cyclic backlog
	float	 m_flSpawnDelay;
	float	 m_flDelayAfterBlocked;
	int		 m_iMaxYawDeviation;
	int		 m_iTargetActivator;
	string_t m_iszTriggerOnDeathNotice;
	BOOL	 m_fRespawnAsPlayerAlly;
	int		 m_iMakeBlockerMoveAway;
	BOOL	 m_fGag;
	BOOL	 m_fChangeRenderMode;	// SBO 2.0 (Featureful a8861730) "change_rendermode": children get the maker's render keys
	int		 m_iDelayedCount;
	int		 m_iCyclicBacklogSize;
	float	 m_flLastSpawnDelay;
	float	 NextMakeDelay( void );	// m_flDelay with the "speedmon" factor (1.38)
	static void ChildHull( const char *szClassname, Vector &vecMins, Vector &vecMaxs );
	static BOOL ChildTakesBody( const char *szClassname );
};

// SBO 2.0 (Featureful CMonsterMakerHull): holds the spot of a child that appears "spawndelay" seconds later (blocks
// players and monsters; invisible, no model), then makes it
class CMonsterMakerHull : public CBaseEntity
{
public:
	void Think( void );
};
LINK_ENTITY_TO_CLASS( monstermaker_hull, CMonsterMakerHull );

extern cvar_t makermon;
extern cvar_t speedmon;

LINK_ENTITY_TO_CLASS( monstermaker, CMonsterMaker );

TYPEDESCRIPTION	CMonsterMaker::m_SaveData[] = 
{
	DEFINE_FIELD( CMonsterMaker, m_iszMonsterClassname, FIELD_STRING ),
	DEFINE_FIELD( CMonsterMaker, m_cNumMonsters, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_cLiveChildren, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_flGround, FIELD_FLOAT ),
	DEFINE_FIELD( CMonsterMaker, m_iMaxLiveChildren, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_fActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMonsterMaker, m_fFadeChildren, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMonsterMaker, m_iszChildSquadName, FIELD_STRING ),
	DEFINE_FIELD( CMonsterMaker, m_iszChildTarget, FIELD_STRING ),
	DEFINE_FIELD( CMonsterMaker, m_iszChildTriggerTarget, FIELD_STRING ),
	DEFINE_FIELD( CMonsterMaker, m_iChildTriggerCondition, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_iszChildFf, FIELD_STRING ),
	DEFINE_FIELD( CMonsterMaker, m_iszPlacePosition, FIELD_STRING ),
	DEFINE_FIELD( CMonsterMaker, m_positionToFaceTo, FIELD_STRING ),
	DEFINE_FIELD( CMonsterMaker, m_hLocusActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( CMonsterMaker, m_iChildSpawnflags, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_iszWarpball, FIELD_STRING ),
	DEFINE_FIELD( CMonsterMaker, m_flSpawnDelay, FIELD_FLOAT ),
	DEFINE_FIELD( CMonsterMaker, m_flDelayAfterBlocked, FIELD_FLOAT ),
	DEFINE_FIELD( CMonsterMaker, m_iMaxYawDeviation, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_iTargetActivator, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_iszTriggerOnDeathNotice, FIELD_STRING ),
	DEFINE_FIELD( CMonsterMaker, m_fRespawnAsPlayerAlly, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMonsterMaker, m_iMakeBlockerMoveAway, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_fGag, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMonsterMaker, m_fChangeRenderMode, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMonsterMaker, m_iDelayedCount, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_iCyclicBacklogSize, FIELD_INTEGER ),
};


IMPLEMENT_SAVERESTORE( CMonsterMaker, CBaseMonster );

void CMonsterMaker :: KeyValue( KeyValueData *pkvd )
{
	
	if ( FStrEq(pkvd->szKeyName, "monstercount") )
	{
		m_cNumMonsters = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
		if ( m_cNumMonsters > 0 )
		{
			if ( makermon.value == 0 )
				m_cNumMonsters /= 2;
			else if ( makermon.value == 2 )
				m_cNumMonsters *= 2;
			else if ( makermon.value == 3 )
				m_cNumMonsters *= 3;

			// SBO: halving must not turn a finite count into 0, the counter
			// would then go negative in MakeMonster and never hit the "done"
			// check (endless spawns). Same floor as m_iMaxLiveChildren.
			if ( m_cNumMonsters < 1 )
				m_cNumMonsters = 1;
		}
	}
	else if ( FStrEq(pkvd->szKeyName, "m_imaxlivechildren") )
	{
		m_iMaxLiveChildren = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
		if ( m_iMaxLiveChildren > 0 )
		{
			if ( makermon.value == 0 )
				m_iMaxLiveChildren /= 2;
			else if ( makermon.value == 2 )
				m_iMaxLiveChildren *= 2;
			else if ( makermon.value == 3 )
				m_iMaxLiveChildren *= 3;
		}
		if ( m_iMaxLiveChildren < 1 )
			m_iMaxLiveChildren = 1;
	}
	else if ( FStrEq(pkvd->szKeyName, "monstertype") )
	{
		m_iszMonsterClassname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	// SBO: keys for the children, see MakeMonster
	else if ( FStrEq(pkvd->szKeyName, "squadname") )
	{
		m_iszChildSquadName = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "childtarget") )
	{
		m_iszChildTarget = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "childtriggertarget") )
	{
		m_iszChildTriggerTarget = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "childtriggercondition") )
	{
		m_iChildTriggerCondition = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "childspawnflags") )
	{
		m_iChildSpawnflags = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "Ff") )
	{
		m_iszChildFf = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "warpball") )			// SBO 2.0 (Featureful): an env_warpball's name
	{
		m_iszWarpball = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "spawnorigin") )		// SBO 2.0 (Featureful) [LP]
	{
		m_iszPlacePosition = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "face_position") )	// SBO 2.0 (Featureful) [LP]
	{
		m_positionToFaceTo = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	// SBO 2.0 (Featureful keys)
	else if ( FStrEq(pkvd->szKeyName, "spawndelay") )
	{
		m_flSpawnDelay = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "delay_after_blocked") )
	{
		m_flDelayAfterBlocked = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "yawdeviation") )
	{
		m_iMaxYawDeviation = abs( atoi( pkvd->szValue ) ) % 361;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "target_activator") )
	{
		m_iTargetActivator = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "trigger_on_death_notice") )
	{
		m_iszTriggerOnDeathNotice = pkvd->szValue[0] ? ALLOC_STRING( pkvd->szValue ) : iStringNull;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "respawn_as_playerally") )
	{
		m_fRespawnAsPlayerAlly = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "make_blocker_move_away") )
	{
		m_iMakeBlockerMoveAway = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "gag") )
	{
		m_fGag = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "change_rendermode") )	// SBO 2.0 (Featureful a8861730)
	{
		m_fChangeRenderMode = atoi( pkvd->szValue ) != 0;
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}


void CMonsterMaker :: Spawn( )
{
	pev->solid = SOLID_NOT;

	m_cLiveChildren = 0;
	Precache();
	if ( !FStringNull ( pev->targetname ) )
	{
		if ( pev->spawnflags & SF_MONSTERMAKER_CYCLIC )
		{
			SetUse ( &CMonsterMaker::CyclicUse );// drop one monster each time we fire
		}
		else
		{
			SetUse ( &CMonsterMaker::ToggleUse );// so can be turned on/off
		}

		if ( FBitSet ( pev->spawnflags, SF_MONSTERMAKER_START_ON ) )
		{// start making monsters as soon as monstermaker spawns
			m_fActive = TRUE;
			SetThink ( &CMonsterMaker::MakerThink );
		}
		else if ( FBitSet( pev->spawnflags, SF_MONSTERMAKER_CYCLIC ) && FBitSet( pev->spawnflags, SF_MONSTERMAKER_CYCLIC_BACKLOG ) )
		{
			// SBO 2.0 (Featureful): CyclicUse schedules this think for the triggers it could not make yet
			m_fActive = FALSE;
			SetThink( &CMonsterMaker::CyclicBacklogThink );
		}
		else
		{// wait to be activated.
			m_fActive = FALSE;
			SetThink ( &CMonsterMaker::SUB_DoNothing );
		}
	}
	else
	{// no targetname, just start.
			if ( speedmon.value == 0 )
				pev->nextthink = gpGlobals->time + m_flDelay * 2;
			else if ( speedmon.value == 1 )
				pev->nextthink = gpGlobals->time + m_flDelay;
			else if ( speedmon.value == 2 )
				pev->nextthink = gpGlobals->time + m_flDelay * 0.5f;
			else if ( speedmon.value == 3 )
				pev->nextthink = gpGlobals->time + m_flDelay / 3;
			m_fActive = TRUE;
			SetThink ( &CMonsterMaker::MakerThink );
	}

	if ( m_cNumMonsters == 1 )
	{
		m_fFadeChildren = FALSE;
	}
	else
	{
		m_fFadeChildren = TRUE;
	}

	m_flGround = 0;
}

void CMonsterMaker :: Precache( void )
{
	CBaseMonster::Precache();

	UTIL_PrecacheOther( STRING( m_iszMonsterClassname ) );
}

//=========================================================
// ChildSpawnflags - SBO 2.0: the spawnflags each child gets
//=========================================================
int CMonsterMaker :: ChildSpawnflags( void )
{
	int flags = SF_MONSTER_FALL_TO_GROUND | m_iChildSpawnflags;

	// Children hit monsterclip brushes
	if ( pev->spawnflags & SF_MONSTERMAKER_MONSTERCLIP )
		flags |= SF_MONSTER_HITMONSTERCLIP;

	// SBO 2.0 (Featureful SpawnMonster): the maker's flags for its children
	if ( pev->spawnflags & SF_MONSTERMAKER_PRISONER )
		flags |= SF_MONSTER_PRISONER;
	if ( pev->spawnflags & SF_MONSTERMAKER_PREDISASTER )
		flags |= SF_MONSTER_PREDISASTER;
	if ( pev->spawnflags & SF_MONSTERMAKER_WAIT_UNTIL_PROVOKED )
		flags |= SF_MONSTER_WAIT_UNTIL_PROVOKED;
	if ( pev->spawnflags & SF_MONSTERMAKER_SPECIAL_FLAG )
		flags |= SF_MONSTER_SPECIAL_FLAG;
	if ( pev->spawnflags & SF_MONSTERMAKER_NONSOLID_CORPSE )
		flags |= SF_MONSTER_NONSOLID_CORPSE;
	if ( pev->spawnflags & SF_MONSTERMAKER_IGNORE_PUSH )
		flags |= SF_MONSTER_IGNORE_PUSH;
	if ( pev->spawnflags & SF_MONSTERMAKER_ACT_OUT_OF_PVS )
		flags |= SF_MONSTER_ACT_OUT_OF_PVS;
	if ( m_fGag )
		flags |= SF_MONSTER_GAG;

	return flags;
}

// SBO 2.0 (Featureful "Auto size BBox"): a child's own hull (each class's Spawn); unknown classes: HL's maker box
// SBO 2.0: FALSE for the grunt family. Their body value is a head (x1) plus a gun (x4) and their Spawn only sets
// the gun for shotguns and the head for shotgun / M203 grunts, so a maker's body would draw the wrong gun, no gun
// or the commander head (e.g. body 4 on an MP5 grunt shows a shotgun). Scientists, monster_dark and zombies use it.
BOOL CMonsterMaker::ChildTakesBody( const char *szClassname )
{
	static const char *s_szGruntFamily[] =
	{
		"monster_human_grunt", "monster_grunt_repel", "monster_hgrunt_dead",
		"monster_grunt_hunter", "monster_grunt_hunter_repel", "monster_hgrunt_hunter_dead",
		"monster_zombie_grunt", "dark_grunt",
	};
	for ( int i = 0; i < (int)ARRAYSIZE( s_szGruntFamily ); i++ )
	{
		if ( FStrEq( szClassname, s_szGruntFamily[i] ) )
			return FALSE;
	}
	return TRUE;
}

void CMonsterMaker::ChildHull( const char *szClassname, Vector &vecMins, Vector &vecMaxs )
{
	static const struct { const char *cls; float mins[3], maxs[3]; } hulls[] =
	{
		{ "monster_apache", { -32, -32, -64 }, { 32, 32, 0 } },
		{ "monster_cyborg", { -30, -15, 0 }, { 30, 15, 135 } },
		{ "monster_gargantua", { -32, -32, 0 }, { 32, 32, 64 } },
		{ "monster_ichthyosaur", { -32, -32, -32 }, { 32, 32, 32 } },
		{ "monster_cyberminotor", { -32, -32, 0 }, { 32, 32, 230 } },
		{ "monster_osprey", { -400, -400, -100 }, { 400, 400, 32 } },
		{ "monster_sitting_scientist", { -14, -14, 0 }, { 14, 14, 36 } },
		{ "monster_turret", { -32, -32, -32 }, { 32, 32, 32 } },
		{ "monster_miniturret", { -16, -16, -16 }, { 16, 16, 16 } },
		{ "monster_sentry", { -16, -16, -16 }, { 16, 16, 16 } },
		{ "monster_snark", { -4, -4, 0 }, { 4, 4, 8 } },
	};
	for ( int i = 0; i < (int)ARRAYSIZE( hulls ); i++ )
	{
		if ( FStrEq( szClassname, hulls[i].cls ) )
		{
			vecMins = Vector( hulls[i].mins[0], hulls[i].mins[1], hulls[i].mins[2] );
			vecMaxs = Vector( hulls[i].maxs[0], hulls[i].maxs[1], hulls[i].maxs[2] );
			return;
		}
	}
	if ( !strncmp( szClassname, "monster_", 8 ) )
	{
		// barneys, grunts, scientists, zombies, aliens, imps, werewolves, hunters, ...: the human hull
		vecMins = VEC_HUMAN_HULL_MIN;
		vecMaxs = VEC_HUMAN_HULL_MAX;
		return;
	}
	vecMins = Vector( -34, -34, 0 );
	vecMaxs = Vector( 34, 34, 0 );
}

float CMonsterMaker::NextMakeDelay( void )
{
	if ( speedmon.value == 0 )
		return m_flDelay * 2;
	if ( speedmon.value == 2 )
		return m_flDelay * 0.5f;
	if ( speedmon.value == 3 )
		return m_flDelay / 3;
	return m_flDelay;
}

//=========================================================
// MakeMonster-  this is the code that drops the monster
//=========================================================
int CMonsterMaker::MakeMonster( void )
{
	m_flLastSpawnDelay = 0;

	// SBO 2.0 (Featureful): children still on their way ("spawndelay") count too
	if ( m_iMaxLiveChildren > 0 && m_cLiveChildren + m_iDelayedCount >= m_iMaxLiveChildren )
	{// not allowed to make a new one yet. Too many live ones out right now.
		return MONSTERMAKER_LIMIT;
	}

	// SBO 2.0 (Featureful): where the child appears and where it faces
	Vector placePosition = pev->origin;
	Vector placeAngles = pev->angles;
	float flGround = m_flGround;

	if ( !FStringNull( m_iszPlacePosition ) )
	{
		if ( !TryCalcLocus_Position( this, m_hLocusActivator, STRING( m_iszPlacePosition ), placePosition ) )
			return MONSTERMAKER_BADPLACE;

		// the position can change: its own ground each time
		TraceResult tr;
		UTIL_TraceLine( placePosition, placePosition - Vector( 0, 0, 2048 ), ignore_monsters, ENT(pev), &tr );
		flGround = tr.vecEndPos.z;
	}
	else if ( !m_flGround )
	{
		// set altitude. Now that I'm activated, any breakables, etc should be out from under me. 
		TraceResult tr;

		UTIL_TraceLine ( pev->origin, pev->origin - Vector ( 0, 0, 2048 ), ignore_monsters, ENT(pev), &tr );
		m_flGround = tr.vecEndPos.z;
		flGround = m_flGround;
	}

	if ( !FStringNull( m_positionToFaceTo ) )
	{
		if ( FStrEq( STRING( m_positionToFaceTo ), "*player" ) )
		{
			// Featureful: the nearest living player; SBO: the nearest player in play (no spectators)
			CBaseEntity *pNearest = NULL;
			float flBest = 0;
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				if ( !Logic_IsPlayerInPlay( pPlayer ) )
					continue;
				float flDist = ( pPlayer->pev->origin - placePosition ).Length();
				if ( !pNearest || flDist < flBest )
				{
					pNearest = pPlayer;
					flBest = flDist;
				}
			}
			if ( pNearest )
				placeAngles = Vector( 0, UTIL_VecToYaw( pNearest->pev->origin - placePosition ), 0 );
		}
		else
		{
			Vector positionToAngleTo;
			if ( TryCalcLocus_Position( this, m_hLocusActivator, STRING( m_positionToFaceTo ), positionToAngleTo ) )
				placeAngles = Vector( 0, UTIL_VecToYaw( positionToAngleTo - placePosition ), 0 );
		}
	}

	// SBO 2.0 (Featureful "yawdeviation"): a random turn left or right
	if ( m_iMaxYawDeviation )
	{
		const int deviation = RANDOM_LONG( 0, m_iMaxYawDeviation );
		placeAngles.y = UTIL_AngleMod( placeAngles.y + ( RANDOM_LONG( 0, 1 ) ? deviation : -deviation ) );
	}

	// the free-spot box: HL's 68 x 68 from the ground up to the maker; with "Auto size BBox" (SBO 2.0, Featureful)
	// the child's own width, up to its own height
	Vector vecHullMins( -34, -34, 0 ), vecHullMaxs( 34, 34, 0 );
	if ( FBitSet( pev->spawnflags, SF_MONSTERMAKER_AUTOSIZEBBOX ) )
	{
		ChildHull( STRING( m_iszMonsterClassname ), vecHullMins, vecHullMaxs );
		vecHullMins.z = 0;
	}
	Vector mins = placePosition + vecHullMins;
	Vector maxs = placePosition + vecHullMaxs;
	if ( !FBitSet( pev->spawnflags, SF_MONSTERMAKER_AUTOSIZEBBOX ) )
		maxs.z = placePosition.z;
	mins.z = flGround;

	CBaseEntity *pList[2];
	int count = UTIL_EntitiesInBox( pList, 2, mins, maxs, FL_CLIENT|FL_MONSTER );
	if ( !count && ( m_iDelayedCount || m_flSpawnDelay > 0 ) )
	{
		// SBO 2.0: a spot held for a delayed child ("spawndelay"; the box is not a monster for the other code)
		CBaseEntity *pAll[32];
		int n = UTIL_EntitiesInBox( pAll, 32, mins, maxs, 0 );
		for ( int i = 0; i < n; i++ )
			if ( FClassnameIs( pAll[i]->pev, "monstermaker_hull" ) && pAll[i]->pev->solid != SOLID_NOT )
				return MONSTERMAKER_BLOCKED;
	}
	if ( count )
	{
		// don't build a stack of monsters!
		// SBO 2.0 (Featureful "make_blocker_move_away"): a monster in the way is asked to move (1 walk, 2 run)
		if ( m_iMakeBlockerMoveAway )
		{
			for ( int i = 0; i < count; i++ )
			{
				CBaseMonster *pBlocker = pList[i]->MyMonsterPointer();
				if ( pBlocker && !pBlocker->IsPlayer() && pBlocker->IsAlive() )
					GuideMonster_MoveAwayFromSpot( pBlocker, placePosition, vecHullMaxs.x * 1.5f, m_iMakeBlockerMoveAway == 2 );
			}
		}
		return MONSTERMAKER_BLOCKED;
	}

	// SBO 2.0 (Featureful "spawndelay"): an invisible box holds the spot, the child appears when it is due
	if ( m_flSpawnDelay > 0 )
	{
		CMonsterMakerHull *pHull = GetClassPtr( (CMonsterMakerHull *)NULL );
		pHull->pev->classname = MAKE_STRING( "monstermaker_hull" );
		UTIL_SetOrigin( pHull->pev, placePosition );
		pHull->pev->angles = placeAngles;
		pHull->pev->solid = SOLID_BBOX;
		pHull->pev->movetype = MOVETYPE_NONE;
		pHull->pev->effects = EF_NODRAW;
		pHull->pev->owner = edict();
		UTIL_SetSize( pHull->pev, Vector( vecHullMins.x, vecHullMins.y, flGround - placePosition.z ),
			Vector( vecHullMaxs.x, vecHullMaxs.y, max( vecHullMaxs.z, 1.0f ) ) );
		pHull->pev->nextthink = gpGlobals->time + m_flSpawnDelay;
		m_iDelayedCount++;
		m_flLastSpawnDelay = m_flSpawnDelay;

		if ( !FStringNull( m_iszWarpball ) )	// the warpball now, at the spot (Featureful)
		{
			Vector vecChildMins, vecChildMaxs;
			ChildHull( STRING( m_iszMonsterClassname ), vecChildMins, vecChildMaxs );
			Vector vecWarp = pev->impulse < 0 ? placePosition : placePosition + ( vecChildMins + vecChildMaxs ) * 0.5f;
			if ( !GuideWarpball_Play( STRING( m_iszWarpball ), vecWarp, edict() ) )
				ALERT( at_console, "monstermaker \"%s\": warpball \"%s\" is not an env_warpball\n", STRING( pev->targetname ), STRING( m_iszWarpball ) );
		}
		return MONSTERMAKER_SPAWNED;
	}

	if ( !SpawnChild( placePosition, placeAngles ) )
		return MONSTERMAKER_NULLENTITY;
	return MONSTERMAKER_SPAWNED;
}

// the child at placePosition: NULL when the class does not exist
CBaseEntity *CMonsterMaker::SpawnChild( const Vector &placePosition, const Vector &placeAngles )
{
	edict_t	*pent;
	entvars_t		*pevCreate;

	pent = CREATE_NAMED_ENTITY( m_iszMonsterClassname );

	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in MonsterMaker!\n" );
		// SBO 2.0 (Featureful 6c2da6ef): no such class, so no try will make one; stop trying (the line came every
		// think, for good). Triggering the maker again tries once more
		SetThink( NULL );
		return NULL;
	}
	
	// If I have a target, fire!
	if ( !FStringNull ( pev->target ) && m_iTargetActivator == 0 )
	{
		// delay already overloaded for this entity, so can't call SUB_UseTargets()
		FireTargets( STRING(pev->target), this, this, USE_TOGGLE, 0 );
	}

	pevCreate = VARS( pent );
	pevCreate->origin = placePosition;	// SBO 2.0: the maker's origin and angles unless spawnorigin / face_position
	pevCreate->angles = placeAngles;
	SetBits( pevCreate->spawnflags, ChildSpawnflags() );	// SBO 2.0: fall to ground, monsterclip, + childspawnflags

	// SBO: give the child the maker's child keyvalues before it spawns, the same
	// order as a map-placed monster (keyvalues, then Spawn). Monsters only fill in
	// their defaults (weapons, ...) when the key was not set.
	if ( pev->weapons )
		pevCreate->weapons = pev->weapons;
	if ( !FStringNull( m_iszChildSquadName ) )
		pevCreate->netname = m_iszChildSquadName;
	if ( !FStringNull( m_iszChildTarget ) )
		pevCreate->target = m_iszChildTarget;
	// SBO 2.0 (Featureful): the maker's body and scale (0 = the child's own). The grunt family gets no body: its
	// body value is head + gun, and the grunt's weapons pick those (ChildTakesBody)
	if ( pev->body && ChildTakesBody( STRING( m_iszMonsterClassname ) ) )
		pevCreate->body = pev->body;
	if ( pev->scale )
		pevCreate->scale = pev->scale;

	CBaseEntity *pChild = CBaseEntity::Instance( pent );
	CBaseMonster *pChildMonster = pChild ? pChild->MyMonsterPointer() : NULL;
	if ( pChildMonster )
	{
		// Set directly rather than through KeyValue, which would allocate the
		// string again for every child.
		if ( !FStringNull( m_iszChildTriggerTarget ) )
			pChildMonster->m_iszTriggerTarget = m_iszChildTriggerTarget;
		if ( m_iChildTriggerCondition )
			pChildMonster->m_iTriggerCondition = m_iChildTriggerCondition;
		// SBO 2.0 (Featureful SpawnMonster): the maker's "classify" and "respawn_as_playerally"
		if ( m_iClassKey )
			pChildMonster->m_iClassKey = m_iClassKey;
		if ( m_fRespawnAsPlayerAlly )
			pChildMonster->m_fReverseRelationship = TRUE;
	}

	if ( !FStringNull( m_iszChildFf ) )
	{
		// Each class stores "Ff" itself, so pass it to the child's KeyValue the
		// way the engine does for map entities.
		KeyValueData kvd;
		kvd.szClassName = (char *)STRING( pevCreate->classname );
		kvd.szKeyName = (char *)"Ff";
		kvd.szValue = (char *)STRING( m_iszChildFf );
		kvd.fHandled = FALSE;
		DispatchKeyValue( pent, &kvd );
	}

	DispatchSpawn( ENT( pevCreate ) );
	pevCreate->owner = edict();

	// SBO 2.0 (Featureful StartWarpballEffect): the warpball at the new monster, its sound on the maker (with
	// "spawndelay" it played when the spot was taken)
	if ( !FStringNull( m_iszWarpball ) && m_flSpawnDelay <= 0 )
	{
		CBaseEntity *pCreated = CBaseEntity::Instance( pent );
		Vector vecWarp = ( pev->impulse < 0 || !pCreated ) ? pevCreate->origin : pCreated->Center();
		if ( !GuideWarpball_Play( STRING( m_iszWarpball ), vecWarp, edict() ) )
			ALERT( at_console, "monstermaker \"%s\": warpball \"%s\" is not an env_warpball\n", STRING( pev->targetname ), STRING( m_iszWarpball ) );
	}

	if ( !FStringNull( pev->netname ) )
	{
		// if I have a netname (overloaded), give the child monster that name as a targetname
		pevCreate->targetname = pev->netname;
	}

	// SBO 2.0 (Featureful a8861730 "change_rendermode"): the maker's render keys, set after the child spawned, so
	// they win over what its Spawn set
	if ( m_fChangeRenderMode )
	{
		pevCreate->rendermode = pev->rendermode;
		pevCreate->renderamt = pev->renderamt;
		pevCreate->rendercolor = pev->rendercolor;
		pevCreate->renderfx = pev->renderfx;
	}

	m_cLiveChildren++;// count this monster
	m_cNumMonsters--;

	if ( m_cNumMonsters == 0 )
	{
		// Disable this forever.  Don't kill it because it still gets death notices
		SetThink( NULL );
		SetUse( NULL );
	}

	// SBO 2.0 (Featureful "target_activator"): -1 no activator, 1 the maker, 2 the child, 3 the maker's activator; 0
	// fired above, before the child spawned (HL)
	if ( !FStringNull( pev->target ) && m_iTargetActivator != 0 )
	{
		CBaseEntity *pActivator = this;
		if ( m_iTargetActivator < 0 )
			pActivator = NULL;
		else if ( m_iTargetActivator == 2 )
			pActivator = CBaseEntity::Instance( pent );
		else if ( m_iTargetActivator == 3 )
			pActivator = m_hLocusActivator;
		FireTargets( STRING( pev->target ), pActivator, this, USE_TOGGLE, 0 );
	}
	return CBaseEntity::Instance( pent );
}

// SBO 2.0 (Featureful CMonsterMakerHull::Think): the delayed child appears where the box held the spot
void CMonsterMakerHull::Think( void )
{
	pev->solid = SOLID_NOT;
	CBaseEntity *pOwner = FNullEnt( pev->owner ) ? NULL : CBaseEntity::Instance( pev->owner );
	if ( pOwner && FClassnameIs( pOwner->pev, "monstermaker" ) )
	{
		CMonsterMaker *pMaker = (CMonsterMaker *)pOwner;
		pMaker->m_iDelayedCount--;
		UTIL_SetOrigin( pev, pev->origin );	// unlinked from the box test before the child takes the spot
		pMaker->SpawnChild( pev->origin, pev->angles );
	}
	UTIL_Remove( this );
}

//=========================================================
// CyclicUse - drops one monster from the monstermaker
// each time we call this.
//=========================================================
void CMonsterMaker::CyclicUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hLocusActivator = pActivator;	// SBO 2.0: spawnorigin / face_position are computed for it
	// SBO 2.0 (Featureful "Cyclic backlog"): a trigger that finds the spot taken is kept and made when it is free
	if ( MakeMonster() == MONSTERMAKER_BLOCKED && FBitSet( pev->spawnflags, SF_MONSTERMAKER_CYCLIC_BACKLOG ) )
	{
		m_iCyclicBacklogSize++;
		pev->nextthink = gpGlobals->time + ( m_flDelayAfterBlocked > 0 ? m_flDelayAfterBlocked : m_flDelay );
	}
}

// SBO 2.0 (Featureful): the kept triggers of a cyclic backlog maker, one per try
void CMonsterMaker::CyclicBacklogThink( void )
{
	int result = MakeMonster();
	if ( result == MONSTERMAKER_SPAWNED )
		m_iCyclicBacklogSize--;
	if ( m_iCyclicBacklogSize > 0 )
		pev->nextthink = gpGlobals->time + ( result == MONSTERMAKER_BLOCKED && m_flDelayAfterBlocked > 0 ? m_flDelayAfterBlocked : m_flDelay );
}

//=========================================================
// ToggleUse - activates/deactivates the monster maker
//=========================================================
void CMonsterMaker :: ToggleUse ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_fActive ) )
		return;

	m_hLocusActivator = pActivator;	// SBO 2.0: spawnorigin / face_position are computed for it

	if ( m_fActive )
	{
		m_fActive = FALSE;
		SetThink ( NULL );
	}
	else
	{
		m_fActive = TRUE;
		SetThink ( &CMonsterMaker::MakerThink );
	}

	pev->nextthink = gpGlobals->time;
}

//=========================================================
// MakerThink - creates a new monster every so often
//=========================================================
void CMonsterMaker :: MakerThink ( void )
{
	pev->nextthink = gpGlobals->time + NextMakeDelay();

	int result = MakeMonster();
	// SBO 2.0 (Featureful): "delay_after_blocked" after a blocked try; a delayed child: not before it appeared
	if ( result == MONSTERMAKER_BLOCKED && m_flDelayAfterBlocked > 0 )
		pev->nextthink = gpGlobals->time + m_flDelayAfterBlocked;
	else if ( result == MONSTERMAKER_SPAWNED && m_flLastSpawnDelay > 0 && FStringNull( m_iszPlacePosition ) )
		pev->nextthink = gpGlobals->time + max( m_flLastSpawnDelay, NextMakeDelay() );
}


//=========================================================
//=========================================================
void CMonsterMaker :: DeathNotice ( entvars_t *pevChild )
{
	// ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
	m_cLiveChildren--;

	if ( !m_fFadeChildren )
	{
		pevChild->owner = NULL;
	}

	// SBO 2.0 (Featureful "trigger_on_death_notice")
	if ( !FStringNull( m_iszTriggerOnDeathNotice ) )
		FireTargets( STRING( m_iszTriggerOnDeathNotice ), this, this, USE_TOGGLE, 0 );
}
