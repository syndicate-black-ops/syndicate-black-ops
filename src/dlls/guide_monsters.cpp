//=========================================================
// SBO 2.0: monster and script entities from the Featureful entity guide (see
// docs/FEATUREFUL_GUIDE_ENTITIES.md): monster_target, trigger_startpatrol,
// scripted_schedule, scripted_following.
// Ported from Featureful (https://github.com/FreeSlave/halflife-featureful, Half-Life 1 SDK license) dlls/monsters.cpp,
// dlls/triggers.cpp and dlls/scripted.cpp (monster_target and trigger_startpatrol from Spirit of Half-Life).
// Multiplayer: no save/restore; "the player" is the activator when he is a player in play.
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "animation.h"
#include "schedule.h"
#include "nodes.h"
#include "soundent.h"
#include "talkmonster.h"
#include "player.h"
#include "round_hooks.h"
#include "locus.h"
#include "guide_entities.h"

//=========================================================
// monster_target: an invisible fake monster other monsters see as an enemy of its class ("frags", When active,
// count as); "Start inactive" (1); On / Off / Toggle switch it. Not solid and takes no damage: shots go past it.
// Automatic func_tanks shoot at it when it counts as a player or a player ally (GuideMonsterTargets_NextForTank).
//=========================================================
#define SF_MONSTERTARGET_OFF	1

// every monster_target of the map, in spawn order, with no fixed limit: map-placed ones and ones made later (a
// monstermaker, ...). A removed one leaves a NULL handle, dropped when the list grows. Emptied at worldspawn
static EHANDLE *s_phMonsterTargets;
static int s_nMonsterTargets;
static int s_nMonsterTargetsAlloc;

void GuideMonsterTargets_Reset( void )
{
	for ( int i = 0; i < s_nMonsterTargets; i++ )
		s_phMonsterTargets[i] = NULL;
	s_nMonsterTargets = 0;
}

static void GuideMonsterTargets_Add( CBaseEntity *pTarget )
{
	if ( s_nMonsterTargets == s_nMonsterTargetsAlloc )
	{
		// drop the removed ones first; grow only when the live ones fill the list
		int n = 0;
		for ( int i = 0; i < s_nMonsterTargets; i++ )
		{
			if ( s_phMonsterTargets[i] != NULL )
				s_phMonsterTargets[n++] = s_phMonsterTargets[i];
		}
		for ( int i = n; i < s_nMonsterTargets; i++ )
			s_phMonsterTargets[i] = NULL;
		s_nMonsterTargets = n;

		if ( s_nMonsterTargets == s_nMonsterTargetsAlloc )
		{
			int nAlloc = s_nMonsterTargetsAlloc ? s_nMonsterTargetsAlloc * 2 : 32;
			EHANDLE *pNew = new EHANDLE[nAlloc];
			for ( int i = 0; i < s_nMonsterTargets; i++ )
				pNew[i] = s_phMonsterTargets[i];
			delete[] s_phMonsterTargets;
			s_phMonsterTargets = pNew;
			s_nMonsterTargetsAlloc = nAlloc;
		}
	}
	s_phMonsterTargets[s_nMonsterTargets++] = pTarget;
}

class CMonsterTarget : public CBaseEntity
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int Classify( void ) { return (int)pev->frags; }
	// SBO 2.1 (Spirit of Half-Life): On while active (monsters see it), Off while inactive
	STATE GetState( CBaseEntity *pActivator ) { return pev->health > 0 ? STATE_ON : STATE_OFF; }
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	BOOL m_bClassSet;	// the map gave "frags" (MonsterTarget_PreEntvarsKeyValue); read by Spawn only, not saved
};

LINK_ENTITY_TO_CLASS( monster_target, CMonsterTarget );

// SBO 2.0: "frags" is an entity field, so the class's KeyValue never sees it: note here that the map gave it, so a
// monster_target without it counts as the FGD default (Player ally) while an explicit 0 stays Ignored
BOOL MonsterTarget_PreEntvarsKeyValue( edict_t *pent, KeyValueData *pkvd )
{
	if ( !pkvd->szClassName || strcmp( pkvd->szClassName, "monster_target" ) || !FStrEq( pkvd->szKeyName, "frags" ) )
		return FALSE;
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( pent );
	if ( !pEntity || !FClassnameIs( pEntity->pev, "monster_target" ) )
		return FALSE;
	const char *pValue = pkvd->szValue ? pkvd->szValue : "";
	while ( *pValue == ' ' || *pValue == '\t' )
		pValue++;
	if ( !*pValue )
	{
		pkvd->fHandled = TRUE;	// blank: as left out
		return TRUE;
	}
	( (CMonsterTarget *)pEntity )->m_bClassSet = TRUE;
	return FALSE;
}

void CMonsterTarget::Spawn( void )
{
	if ( !m_bClassSet || pev->frags < CLASS_NONE || pev->frags > CLASS_ALIEN_BIOWEAPON )
		pev->frags = CLASS_PLAYER_ALLY;	// the FGD default (left out or out of range; 0 is Ignored)
	pev->health = ( pev->spawnflags & SF_MONSTERTARGET_OFF ) ? 0 : 1;	// alive while active: monsters see it
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	SetBits( pev->flags, FL_MONSTER );
	UTIL_SetOrigin( pev, pev->origin );

	GuideMonsterTargets_Add( this );	// the tanks' list
}

void CMonsterTarget::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ShouldToggle( useType, pev->health > 0 ) )
		pev->health = pev->health > 0 ? 0 : 1;
}

CBaseEntity *GuideMonsterTargets_NextForTank( int &iCursor )
{
	while ( iCursor >= 0 && iCursor < s_nMonsterTargets )
	{
		CBaseEntity *pEnt = s_phMonsterTargets[iCursor++];
		if ( !pEnt || pEnt->pev->health <= 0 )
			continue;
		int cls = pEnt->Classify();
		if ( cls == CLASS_PLAYER || cls == CLASS_PLAYER_ALLY )
			return pEnt;
	}
	return NULL;
}

// a monster free for an entity to steer: alive, not in a script, idle / alert / hunting (Featureful
// CBaseMonster::IsFreeToManipulate)
static BOOL Guide_MonsterIsFree( CBaseMonster *pMonster, BOOL allowInCombat )
{
	if ( !pMonster || !pMonster->IsAlive() || pMonster->m_pCine )
		return FALSE;
	if ( pMonster->m_IdealMonsterState == MONSTERSTATE_SCRIPT || pMonster->m_IdealMonsterState == MONSTERSTATE_PRONE )
		return FALSE;
	switch ( pMonster->m_MonsterState )
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_ALERT:
	case MONSTERSTATE_HUNT:
	case MONSTERSTATE_NONE:
		return TRUE;
	case MONSTERSTATE_COMBAT:
		return allowInCombat;
	default:
		return FALSE;
	}
}

// a talking monster (scientist, barney...) that follows players on +use
static CTalkMonster *Guide_TalkMonster( CBaseMonster *pMonster )
{
	if ( pMonster && pMonster->m_pfnUse == (USEPTR)&CTalkMonster::FollowerUse )
		return (CTalkMonster *)pMonster;
	return NULL;
}

// Featureful SCHED_IDLE_RUN: SCHED_IDLE_WALK running (a route whose path_corner speed is 200 or more)
static Task_t tlGuideIdleRun[] =
{
	{ TASK_RUN_PATH, (float)9999 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
};
static Schedule_t slGuideIdleRun[] =
{
	{ tlGuideIdleRun, ARRAYSIZE( tlGuideIdleRun ),
	  bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE | bits_COND_HEAR_SOUND |
	  bits_COND_SMELL_FOOD | bits_COND_SMELL | bits_COND_PROVOKED,
	  bits_SOUND_COMBAT | bits_SOUND_WORLD | bits_SOUND_PLAYER | bits_SOUND_DANGER | bits_SOUND_MEAT |
	  bits_SOUND_CARCASS | bits_SOUND_GARBAGE, "GuideIdleRun" },
};

// SBO 2.0, from Featureful (CBaseMonster::StartPatrol, Half-Life 1 SDK license): ReadyMonster's path_corner start as
// a function, for trigger_startpatrol and the patrol resume (schedule.cpp GetSchedule)
Schedule_t *CBaseMonster::StartPatrol( CBaseEntity *pPath )
{
	if ( !pPath )
		return NULL;
	m_pGoalEnt = pPath;
	MakeIdealYaw( m_pGoalEnt->pev->origin );	// start turning towards the destination
	m_movementGoal = MOVEGOAL_PATHCORNER;
	if ( pev->movetype == MOVETYPE_FLY )
		m_movementActivity = ACT_FLY;
	else if ( m_pGoalEnt->pev->speed < 200 )
		m_movementActivity = ACT_WALK;
	else
		m_movementActivity = ACT_RUN;
	if ( FRefreshRoute() )
		return m_movementActivity == ACT_RUN ? slGuideIdleRun : GetScheduleOfType( SCHED_IDLE_WALK );
	ALERT( at_aiconsole, "%s: couldn't create route. Can't patrol\n", STRING( pev->classname ) );
	return NULL;
}

//=========================================================
// trigger_startpatrol: sends the monster "target" (a name or !activator) along the path_corner route starting at
// "m_iszPath". The monster keeps fighting; when it is idle again it goes back to the route (Featureful's patrol
// resume, here only for monsters this entity started). A monster following a player stops following.
//=========================================================
class CTriggerSetPatrol : public CBaseDelay
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Spawn( void ) { pev->effects |= EF_NODRAW; }
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseDelay::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	string_t m_iszPath;
};

LINK_ENTITY_TO_CLASS( trigger_startpatrol, CTriggerSetPatrol );

void CTriggerSetPatrol::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "m_iszPath" ) )
	{
		m_iszPath = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDelay::KeyValue( pkvd );
}

void CTriggerSetPatrol::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FStringNull( m_iszPath ) || FStringNull( pev->target ) )
		return;
	CBaseEntity *pPath = UTIL_FindEntityByTargetname( NULL, STRING( m_iszPath ) );
	if ( !pPath )
		return;
	CBaseEntity *pTarget = Locus_FindEntity( NULL, STRING( pev->target ), pActivator );
	CBaseMonster *pMonster = pTarget ? pTarget->MyMonsterPointer() : NULL;
	if ( !pMonster || !FBitSet( pMonster->pev->flags, FL_MONSTER ) || !Guide_MonsterIsFree( pMonster, FALSE ) )
		return;

	Schedule_t *pPatrol = pMonster->StartPatrol( pPath );
	if ( !pPatrol )
		return;
	CTalkMonster *pTalk = Guide_TalkMonster( pMonster );
	if ( pTalk && pTalk->IsFollowing() )
		pTalk->StopFollowing( FALSE );
	pMonster->m_bPatrolResume = TRUE;
	pMonster->SetState( MONSTERSTATE_IDLE );
	pMonster->ChangeSchedule( pPatrol );
}

//=========================================================
// scripted_schedule: suggests an AI schedule to the monsters named "entity", about a spot ("spot_entity"; blank:
// the monster itself, or this entity for investigate / turn / move to): 1 move away from the spot, 2 take cover from
// it (or retreat), 3 investigate it, 4 turn to it, 5 move to it, 0 clear (a running suggested schedule stops).
// "mindist" / "maxdist" for 1 and 2, "move_preference" 0 auto / 1 walk / 2 run. The spot is taken where it is when
// fired ("spot_preference" is accepted; SBO builds the schedules from the stock AI tasks, Featureful has its own
// schedule system). A monster in a script or in combat is left alone; a following monster keeps following after.
//=========================================================
enum
{
	SCRIPTED_SCHEDULE_CLEAR = 0,
	SCRIPTED_SCHEDULE_MOVE_AWAY,
	SCRIPTED_SCHEDULE_MOVE_TO_COVER,
	SCRIPTED_SCHEDULE_INVESTIGATE_SPOT,
	SCRIPTED_SCHEDULE_TURN_TO_SPOT,
	SCRIPTED_SCHEDULE_MOVE_TO_SPOT,
};

// the route is built before the schedule starts (FindCover / Guide_FindSpotAway / MoveToLocation in Suggest). The
// spot to face is the monster's m_vecLastPosition (TASK_FACE_LASTPOSITION): m_hTargetEnt is a talking monster's
// follow target, and a player put there made the monster follow him.
static Task_t tlGuideRunRoute[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_IDLE_STAND },
	{ TASK_RUN_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FACE_LASTPOSITION, (float)0 },
};
static Task_t tlGuideWalkRoute[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_IDLE_STAND },
	{ TASK_WALK_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FACE_LASTPOSITION, (float)0 },
};
// moving away: no turn back to the spot at the end
static Task_t tlGuideRunAway[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_IDLE_STAND },
	{ TASK_RUN_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
	{ TASK_STOP_MOVING, (float)0 },
};
static Task_t tlGuideWalkAway[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_IDLE_STAND },
	{ TASK_WALK_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
	{ TASK_STOP_MOVING, (float)0 },
};
// investigate: stop and look, then the route to the spot again (TASK_STOP_MOVING cleared the one Suggest built), as
// Featureful's tlInvestigateSpot gets its path after stopping
static Task_t tlGuideInvestigate[] =
{
	{ TASK_SET_FAIL_SCHEDULE, (float)SCHED_IDLE_STAND },
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FACE_LASTPOSITION, (float)0 },
	{ TASK_WAIT, (float)1 },
	{ TASK_GET_PATH_TO_LASTPOSITION, (float)0 },
	{ TASK_WALK_PATH, (float)0 },
	{ TASK_WAIT_FOR_MOVEMENT, (float)0 },
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_WAIT, (float)3 },
};
static Task_t tlGuideTurn[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FACE_LASTPOSITION, (float)0 },
	{ TASK_SET_ACTIVITY, (float)ACT_IDLE },
	{ TASK_WAIT, (float)2 },
};

#define GUIDE_SCHED_INTERRUPT	( bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE )

static Schedule_t slGuideRunRoute[] = { { tlGuideRunRoute, ARRAYSIZE( tlGuideRunRoute ), bits_COND_HEAVY_DAMAGE, 0, "GuideRunRoute" } };
static Schedule_t slGuideWalkRoute[] = { { tlGuideWalkRoute, ARRAYSIZE( tlGuideWalkRoute ), GUIDE_SCHED_INTERRUPT, 0, "GuideWalkRoute" } };
static Schedule_t slGuideRunAway[] = { { tlGuideRunAway, ARRAYSIZE( tlGuideRunAway ), bits_COND_HEAVY_DAMAGE, 0, "GuideRunAway" } };
static Schedule_t slGuideWalkAway[] = { { tlGuideWalkAway, ARRAYSIZE( tlGuideWalkAway ), GUIDE_SCHED_INTERRUPT, 0, "GuideWalkAway" } };
static Schedule_t slGuideInvestigate[] = { { tlGuideInvestigate, ARRAYSIZE( tlGuideInvestigate ), GUIDE_SCHED_INTERRUPT, 0, "GuideInvestigate" } };
static Schedule_t slGuideTurn[] = { { tlGuideTurn, ARRAYSIZE( tlGuideTurn ), GUIDE_SCHED_INTERRUPT, 0, "GuideTurn" } };

static BOOL Guide_IsSuggestedSchedule( Schedule_t *p )
{
	return p == slGuideRunRoute || p == slGuideWalkRoute || p == slGuideRunAway || p == slGuideWalkAway ||
		p == slGuideInvestigate || p == slGuideTurn;
}

// The route's activity: a monster with no run animation (zombie, cyborg, cyberminotor) walks, as Half-Life's
// TASK_RUN_PATH does (schedule.cpp)
static Activity Guide_MoveActivity( CBaseMonster *pMonster, BOOL fRun )
{
	if ( fRun && pMonster->LookupActivity( ACT_RUN ) != ACTIVITY_NOT_AVAILABLE )
		return ACT_RUN;
	return ACT_WALK;
}

// MoveToLocation, kept only when the route's first leg is walkable now. A route that goes through the node graph is
// built without checking monsters (BuildRoute's FGetNodeRoute), so a node next to another monster gave a route the
// monster could not start ("Failed to move"), and every later try picked it again. Half-Life's Move tests the same leg
// (CheckLocalMove) before each step.
static BOOL Guide_MoveToLocationClear( CBaseMonster *pMonster, Activity act, const Vector &vecGoal )
{
	if ( !pMonster->MoveToLocation( act, 0, vecGoal ) )
		return FALSE;
	const Vector &vecFirst = pMonster->m_Route[ pMonster->m_iRouteIndex ].vecLocation;
	if ( pMonster->CheckLocalMove( pMonster->pev->origin, vecFirst, NULL, NULL ) == LOCALMOVE_VALID )
		return TRUE;
	pMonster->RouteClear();
	return FALSE;
}

// Featureful CBaseMonster::FindSpotAway (Half-Life 1 SDK license), without its cover trace: a node between flMinDist
// and flMaxDist from the monster that is nearer to him than to the spot by the node graph's paths (with the spot at
// the monster himself, any node), and a route to it. The node search starts where the last one stopped, so monsters
// sent away together spread over different nodes.
static BOOL Guide_FindSpotAway( CBaseMonster *pMonster, const Vector &vecSpot, float flMinDist, float flMaxDist, Activity act )
{
	if ( flMaxDist <= 0 )
		flMaxDist = 784;
	if ( flMinDist > 0.5f * flMaxDist )
		flMinDist = 0.5f * flMaxDist;
	if ( !WorldGraph.m_fGraphPresent || !WorldGraph.m_fGraphPointersSet || WorldGraph.m_cNodes <= 0 )
		return FALSE;
	int iMyNode = WorldGraph.FindNearestNode( pMonster->pev->origin, pMonster );
	if ( iMyNode == NO_NODE )
		return FALSE;
	int iSpotNode = WorldGraph.FindNearestNode( vecSpot, pMonster );
	int iHull = WorldGraph.HullIndex( pMonster );
	BOOL spotIsMe = ( vecSpot - pMonster->pev->origin ).Length() < 1;
	if ( !spotIsMe && iSpotNode != NO_NODE && iSpotNode == iMyNode )
		iSpotNode = NO_NODE;	// on the spot: every node leads away from it
	for ( int i = 0; i < WorldGraph.m_cNodes; i++ )
	{
		int nodeNumber = ( i + WorldGraph.m_iLastCoverSearch ) % WorldGraph.m_cNodes;
		CNode &node = WorldGraph.Node( nodeNumber );
		float flDist = ( pMonster->pev->origin - node.m_vecOrigin ).Length();
		if ( flDist < flMinDist || flDist >= flMaxDist )
			continue;
		if ( !spotIsMe && iSpotNode != NO_NODE )
		{
			float flMine = WorldGraph.PathLength( iMyNode, nodeNumber, iHull, pMonster->m_afCapability );
			float flSpot = WorldGraph.PathLength( iSpotNode, nodeNumber, iHull, pMonster->m_afCapability );
			if ( flMine > flSpot && flSpot >= 0 )
				continue;
		}
		if ( Guide_MoveToLocationClear( pMonster, act, node.m_vecOrigin ) )
		{
			WorldGraph.m_iLastCoverSearch = nodeNumber + 1;
			return TRUE;
		}
	}
	return FALSE;
}

// the last resort (Featureful FindStraightSpotAway): a point straight away from the spot, as far as flMaxDist allows
static BOOL Guide_FindStraightSpotAway( CBaseMonster *pMonster, const Vector &vecSpot, float flMinDist, float flMaxDist, Activity act )
{
	Vector dir = pMonster->pev->origin - vecSpot;
	dir.z = 0;
	if ( dir.Length() < 1 )
	{
		UTIL_MakeVectors( pMonster->pev->angles );
		dir = -gpGlobals->v_forward;
		dir.z = 0;
	}
	dir = dir.Normalize();
	for ( float want = flMaxDist; want >= max( flMinDist, 64.0f ); want *= 0.5f )
	{
		if ( Guide_MoveToLocationClear( pMonster, act, pMonster->pev->origin + dir * want ) )
			return TRUE;
	}
	return FALSE;
}

class CScriptedSchedule : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	BOOL Suggest( CBaseMonster *pMonster, CBaseEntity *pSpot );
	void EXPORT PendingThink( void );
};

LINK_ENTITY_TO_CLASS( scripted_schedule, CScriptedSchedule );

// Suggestions to monsters that were not free when the entity fired (afraid, in combat, in a script): as in Featureful
// (CBaseMonster::SuggestSchedule keeps the suggestion until the monster picks its next schedule while free), the
// suggestion waits and plays once the monster is free again. One pending suggestion per monster: a newer one, or a
// Clear, replaces it.
#define MAX_PENDING_SCHEDULES	32
struct GuidePendingSchedule_t
{
	EHANDLE hMonster;
	EHANDLE hSchedule;
	EHANDLE hSpot;
	BOOL fSpot;
};
static GuidePendingSchedule_t s_PendingSchedules[MAX_PENDING_SCHEDULES];

static void Guide_DropPending( CBaseEntity *pMonster )
{
	for ( int i = 0; i < MAX_PENDING_SCHEDULES; i++ )
	{
		if ( (CBaseEntity *)s_PendingSchedules[i].hMonster == pMonster )
			s_PendingSchedules[i].hMonster = NULL;
	}
}

void CScriptedSchedule::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "entity" ) )
		pev->netname = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "schedule" ) )
		pev->weapons = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "mindist" ) )
		pev->health = atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "maxdist" ) )
		pev->max_health = atof( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "spot_entity" ) )
		pev->message = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "move_preference" ) )
		pev->impulse = atoi( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "spot_preference" ) )
		pev->button = atoi( pkvd->szValue );
	else
	{
		CPointEntity::KeyValue( pkvd );
		return;
	}
	pkvd->fHandled = TRUE;
}

// TRUE when done (played, cleared, or impossible); FALSE when the monster is not free yet
BOOL CScriptedSchedule::Suggest( CBaseMonster *pMonster, CBaseEntity *pSpot )
{
	int type = pev->weapons;
	if ( type == SCRIPTED_SCHEDULE_CLEAR )
	{
		if ( Guide_IsSuggestedSchedule( pMonster->m_pSchedule ) )
		{
			// stop on the spot: the idle schedule of a talking monster that has not greeted a player yet
			// (slIdleHello) does not stop moving, so the route would be walked to its end
			pMonster->RouteClear();
			pMonster->Stop();
			pMonster->ClearSchedule();
			pMonster->ChangeSchedule( pMonster->GetScheduleOfType( SCHED_IDLE_STAND ) );
		}
		return TRUE;
	}
	if ( !pMonster->IsAlive() )
		return TRUE;
	if ( !Guide_MonsterIsFree( pMonster, FALSE ) )
		return FALSE;

	if ( !pSpot )
		pSpot = ( type == SCRIPTED_SCHEDULE_MOVE_AWAY || type == SCRIPTED_SCHEDULE_MOVE_TO_COVER ) ? (CBaseEntity *)pMonster : (CBaseEntity *)this;
	// the spot is where the spot entity is when fired (the monster turns to and walks to that point)
	Vector vecSpot = pSpot->IsBSPModel() ? VecBModelOrigin( pSpot->pev ) : pSpot->pev->origin;
	if ( pSpot == pMonster )
		vecSpot = pMonster->pev->origin;

	BOOL run = ( pev->impulse == 2 ) || ( pev->impulse == 0 && ( type == SCRIPTED_SCHEDULE_MOVE_AWAY || type == SCRIPTED_SCHEDULE_MOVE_TO_COVER ) );
	float minDist = pev->health > 0 ? pev->health : 0;
	float maxDist = pev->max_health > 0 ? pev->max_health : 784;
	Activity act = Guide_MoveActivity( pMonster, run );
	Schedule_t *pSched = NULL;

	switch ( type )
	{
	case SCRIPTED_SCHEDULE_MOVE_TO_COVER:
		// Featureful TASK_FIND_COVER_FROM_SPOT: cover, else a spot away, else straight away
		if ( pMonster->FindCover( vecSpot, g_vecZero, minDist, maxDist ) )
			pSched = run ? slGuideRunAway : slGuideWalkAway;
		else if ( Guide_FindSpotAway( pMonster, vecSpot, max( minDist, 64.0f ), maxDist, act ) ||
			Guide_FindStraightSpotAway( pMonster, vecSpot, max( minDist, 64.0f ), maxDist, act ) )
			pSched = run ? slGuideRunAway : slGuideWalkAway;
		break;
	case SCRIPTED_SCHEDULE_MOVE_AWAY:
		// Featureful TASK_FIND_SPOT_AWAY: a node away from the spot, else straight away
		if ( Guide_FindSpotAway( pMonster, vecSpot, max( minDist, 64.0f ), maxDist, act ) ||
			Guide_FindStraightSpotAway( pMonster, vecSpot, max( minDist, 64.0f ), maxDist, act ) )
			pSched = run ? slGuideRunAway : slGuideWalkAway;
		break;
	case SCRIPTED_SCHEDULE_INVESTIGATE_SPOT:
		if ( pMonster->MoveToLocation( act, 2, vecSpot ) )
			pSched = slGuideInvestigate;
		break;
	case SCRIPTED_SCHEDULE_TURN_TO_SPOT:
		pSched = slGuideTurn;
		break;
	case SCRIPTED_SCHEDULE_MOVE_TO_SPOT:
		if ( pMonster->MoveToLocation( act, 2, vecSpot ) )
			pSched = run ? slGuideRunRoute : slGuideWalkRoute;
		break;
	default:
		ALERT( at_console, "scripted_schedule \"%s\": unknown schedule %d\n", STRING( pev->targetname ), type );
		return TRUE;
	}
	if ( !pSched )
	{
		ALERT( at_aiconsole, "scripted_schedule \"%s\": no route for %s\n", STRING( pev->targetname ), STRING( pMonster->pev->classname ) );
		return TRUE;
	}
	pMonster->m_vecLastPosition = vecSpot;	// TASK_FACE_LASTPOSITION
	pMonster->SetState( MONSTERSTATE_IDLE );
	pMonster->m_IdealMonsterState = MONSTERSTATE_IDLE;
	pMonster->ChangeSchedule( pSched );
	return TRUE;
}

BOOL GuideMonster_MoveAwayFromSpot( CBaseMonster *pMonster, const Vector &vecSpot, float flMinDist, BOOL fRun )
{
	if ( !Guide_MonsterIsFree( pMonster, FALSE ) )
		return FALSE;
	Activity act = Guide_MoveActivity( pMonster, fRun );
	// Featureful SCHED_RETREAT_FROM_SPOT: a node away from the spot (up to 256 units), else straight away
	if ( !Guide_FindSpotAway( pMonster, vecSpot, flMinDist, 256, act ) &&
		 !Guide_FindStraightSpotAway( pMonster, vecSpot, flMinDist, 256, act ) )
		return FALSE;
	pMonster->m_vecLastPosition = vecSpot;
	pMonster->ChangeSchedule( fRun ? slGuideRunAway : slGuideWalkAway );
	return TRUE;
}

void CScriptedSchedule::PendingThink( void )
{
	BOOL any = FALSE;
	for ( int i = 0; i < MAX_PENDING_SCHEDULES; i++ )
	{
		GuidePendingSchedule_t &p = s_PendingSchedules[i];
		if ( (CBaseEntity *)p.hSchedule != this || p.hMonster == NULL )
			continue;
		CBaseMonster *pMonster = p.hMonster->MyMonsterPointer();
		CBaseEntity *pSpot = p.hSpot;
		if ( !pMonster || ( p.fSpot && !pSpot ) )
		{
			p.hMonster = NULL;	// the monster or the spot entity is gone
			continue;
		}
		if ( Suggest( pMonster, pSpot ) )
			p.hMonster = NULL;
		else
			any = TRUE;
	}
	if ( any )
		pev->nextthink = gpGlobals->time + 0.1;
	else
		SetThink( NULL );
}

void CScriptedSchedule::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FStringNull( pev->netname ) )
	{
		ALERT( at_console, "scripted_schedule \"%s\": no affected monster\n", STRING( pev->targetname ) );
		return;
	}
	CBaseEntity *pSpot = NULL;
	if ( !FStringNull( pev->message ) )
	{
		pSpot = Locus_FindEntity( NULL, STRING( pev->message ), pActivator );
		if ( !pSpot )
		{
			ALERT( at_console, "scripted_schedule \"%s\": no spot entity \"%s\"\n", STRING( pev->targetname ), STRING( pev->message ) );
			return;
		}
	}
	CBaseEntity *pEntity = NULL;
	int iGuard = 0;
	while ( ( pEntity = Locus_FindEntity( pEntity, STRING( pev->netname ), pActivator ) ) != NULL && iGuard++ < 64 )
	{
		CBaseMonster *pMonster = pEntity->MyMonsterPointer();
		if ( !pMonster || !FBitSet( pMonster->pev->flags, FL_MONSTER ) )
			continue;
		Guide_DropPending( pMonster );	// a newer suggestion or a Clear replaces a waiting one
		if ( Suggest( pMonster, pSpot ) )
			continue;
		ALERT( at_aiconsole, "scripted_schedule \"%s\": %s is not free (state %d, ideal %d, enemy %s): waiting\n",
			STRING( pev->targetname ), STRING( pMonster->pev->classname ), pMonster->m_MonsterState, pMonster->m_IdealMonsterState,
			pMonster->m_hEnemy != NULL ? STRING( pMonster->m_hEnemy->pev->classname ) : "none" );
		for ( int i = 0; i < MAX_PENDING_SCHEDULES; i++ )
		{
			GuidePendingSchedule_t &p = s_PendingSchedules[i];
			if ( p.hMonster == NULL )
			{
				p.hMonster = pMonster;
				p.hSchedule = this;
				p.hSpot = pSpot;
				p.fSpot = pSpot != NULL;
				SetThink( &CScriptedSchedule::PendingThink );
				pev->nextthink = gpGlobals->time + 0.1;
				break;
			}
		}
	}
}

//=========================================================
// scripted_following: makes the talking monster "entity" start (1) or stop (-1) following the player who fired it
// (a player in play), or "Forward input" (0): On start, Off stop, Toggle switch. Fires on_start_following /
// on_stop_following (activator: the player) only when the state changed. A monster in a script is retried until it
// is free. "Remove on fire" (1).
//=========================================================
#define SF_SCRIPTED_FOLLOWING_ONCE	1

enum
{
	FOLLOWING_INVALID = 0,
	FOLLOWING_STARTED,
	FOLLOWING_STOPPED,
	FOLLOWING_NOCHANGE,
	FOLLOWING_BUSYINSCRIPT,
};

class CScriptedFollowing : public CPointEntity
{
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT FindThink( void );
	int MakeFollowing( CBasePlayer *pPlayer, USE_TYPE useType );

	EHANDLE m_hActivator;
	string_t m_onStartFollowing;
	string_t m_onStopFollowing;
};

LINK_ENTITY_TO_CLASS( scripted_following, CScriptedFollowing );

void CScriptedFollowing::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "entity" ) )
		pev->netname = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "on_start_following" ) )
		m_onStartFollowing = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "on_stop_following" ) )
		m_onStopFollowing = ALLOC_STRING( pkvd->szValue );
	else if ( FStrEq( pkvd->szKeyName, "use_type" ) )
		pev->impulse = atoi( pkvd->szValue );
	else
	{
		CPointEntity::KeyValue( pkvd );
		return;
	}
	pkvd->fHandled = TRUE;
}

int CScriptedFollowing::MakeFollowing( CBasePlayer *pPlayer, USE_TYPE useType )
{
	int result = FOLLOWING_INVALID;
	CBaseEntity *pEntity = NULL;
	while ( ( pEntity = UTIL_FindEntityByTargetname( pEntity, STRING( pev->netname ) ) ) != NULL )
	{
		CBaseMonster *pMonster = pEntity->MyMonsterPointer();
		CTalkMonster *pTalk = Guide_TalkMonster( pMonster );
		if ( !pTalk || !pTalk->IsAlive() )
			continue;
		if ( pTalk->m_pCine || pTalk->m_MonsterState == MONSTERSTATE_SCRIPT )
		{
			result = FOLLOWING_BUSYINSCRIPT;
			break;
		}
		BOOL following = pTalk->IsFollowing() && pTalk->m_hTargetEnt == (CBaseEntity *)pPlayer;
		BOOL start = useType == USE_ON || ( useType != USE_OFF && !following );
		if ( start )
		{
			if ( following )
				result = FOLLOWING_NOCHANGE;
			else if ( !pTalk->ScriptedFollowAllowed() )
				result = FOLLOWING_NOCHANGE;	// SBO's follow rule, as scripted_sentence's Follow action: a barney only as a player ally with a Followage policy set
			else
			{
				pTalk->LimitFollowers( pPlayer, 1 );
				pTalk->StartFollowing( pPlayer );
				result = FOLLOWING_STARTED;
			}
		}
		else
		{
			if ( pTalk->IsFollowing() )
			{
				pTalk->StopFollowing( TRUE );
				result = FOLLOWING_STOPPED;
			}
			else
				result = FOLLOWING_NOCHANGE;
		}
		break;
	}
	if ( result == FOLLOWING_STARTED && !FStringNull( m_onStartFollowing ) )
		FireTargets( STRING( m_onStartFollowing ), pPlayer, this, USE_TOGGLE, 0 );
	if ( result == FOLLOWING_STOPPED && !FStringNull( m_onStopFollowing ) )
		FireTargets( STRING( m_onStopFollowing ), pPlayer, this, USE_TOGGLE, 0 );
	return result;
}

void CScriptedFollowing::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( FStringNull( pev->netname ) )
	{
		ALERT( at_console, "scripted_following \"%s\": no affected monster\n", STRING( pev->targetname ) );
		return;
	}
	if ( !Logic_IsPlayerInPlay( pActivator ) )
		return;
	if ( pev->impulse < 0 )
		useType = USE_OFF;
	else if ( pev->impulse > 0 )
		useType = USE_ON;

	SetThink( NULL );
	m_hActivator = pActivator;
	int result = MakeFollowing( (CBasePlayer *)pActivator, useType );
	if ( result == FOLLOWING_BUSYINSCRIPT && useType == USE_ON )
	{
		SetThink( &CScriptedFollowing::FindThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else if ( FBitSet( pev->spawnflags, SF_SCRIPTED_FOLLOWING_ONCE ) )
		UTIL_Remove( this );
}

void CScriptedFollowing::FindThink( void )
{
	if ( !Logic_IsPlayerInPlay( m_hActivator ) )
	{
		SetThink( NULL );
		return;
	}
	int result = MakeFollowing( (CBasePlayer *)(CBaseEntity *)m_hActivator, USE_ON );
	if ( result == FOLLOWING_BUSYINSCRIPT )
		pev->nextthink = gpGlobals->time + 0.1;
	else
	{
		SetThink( NULL );
		if ( FBitSet( pev->spawnflags, SF_SCRIPTED_FOLLOWING_ONCE ) )
			UTIL_Remove( this );
	}
}
